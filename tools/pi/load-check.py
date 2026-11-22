#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
# Pi load check for Drizzle, Chamber and Mist Beat Repeat, on the running Ingen
# graph (built from Slope's tools/pi_check.py). Base patch: IN -> Overdrive ->
# Tempo delay -> Mist -> OUT. Watches JACK xruns, DSP load and the engine's RSS.
# Run as the peepo user with the saved patch loaded; the caller reloads it.
#   load-check.py drizzle|chamber|beat|baseline SECONDS
# Results of 22 November 2026: docs/evidence/eurorack-pi-20261122.txt
import json, re, socket, subprocess, sys, time

SOCK = '/run/peepo/ingen.sock'
M = 'ingen:/main'
OD = 'http://drobilla.net/plugins/mda/Overdrive'
DLY = 'http://polyeffects.com/lv2/digit_delay'
DRIZZLE = 'https://github.com/ABustos2606/drizzle#drizzle'
DSYNTH = 'https://github.com/ABustos2606/drizzle#synth'
CHAMBER = 'https://github.com/ABustos2606/chamber#chamber'
BEAT = 'http://polyeffects.com/lv2/polyclouds#beat_repeat'
PREFIXES = ''.join('@prefix %s: <%s> .\n' % p for p in [
    ('ingen', 'http://drobilla.net/ns/ingen#'), ('lv2', 'http://lv2plug.in/ns/lv2core#'),
    ('patch', 'http://lv2plug.in/ns/ext/patch#'), ('xsd', 'http://www.w3.org/2001/XMLSchema#')])


class Ingen:
    def __init__(self):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.connect(SOCK)
        self.s.settimeout(0.2)
        self.seq = 1000
        self.buf = ''
        self.s.sendall(PREFIXES.encode())

    def _send(self, body):
        self.seq += 1
        text = '[]\n\ta patch:%s ;\n\tpatch:sequenceNumber "%d"^^xsd:int ;\n%s' % (body[0], self.seq, body[1])
        self.s.sendall(text.encode())
        return self._wait(self.seq)

    def _wait(self, seq):
        pat = re.compile(r'patch:Response\s*;\s*patch:sequenceNumber\s*"%d"\^\^xsd:int\s*;(?:\s*patch:subject\s*<[^>]*>\s*;)?\s*patch:body\s*"?(-?\d+)' % seq)
        until = time.time() + 30
        while time.time() < until:
            m = pat.search(self.buf)
            if m:
                self.buf = self.buf[m.end():]
                return int(m.group(1))
            try:
                chunk = self.s.recv(65536)
                if chunk: self.buf += chunk.decode(errors='replace')
            except socket.timeout:
                pass
            if len(self.buf) > 4_000_000: self.buf = self.buf[-100000:]
        raise RuntimeError('no response to %d' % seq)

    def check(self, what, status):
        print(('PASS ' if status == 0 else 'FAIL ') + what + ('' if status == 0 else ' (status %d)' % status), flush=True)
        if status != 0: raise SystemExit(1)

    def block(self, sym, uri, x, y):
        self.check('add %s' % sym, self._send(('Put', '\tpatch:subject <%s/%s> ;\n\tpatch:body [\n\t\ta ingen:Block ;\n\t\tlv2:prototype <%s> ;\n\t\tingen:canvasX %.1f ;\n\t\tingen:canvasY %.1f\n\t] .\n' % (M, sym, uri, x, y))))

    def arc(self, tail, head):
        self.check('cable %s -> %s' % (tail, head), self._send(('Put', '\tpatch:subject <%s> ;\n\tpatch:body [\n\t\ta ingen:Arc ;\n\t\tingen:tail <%s/%s> ;\n\t\tingen:head <%s/%s>\n\t] .\n' % (M, M, tail, M, head))))

    def unarc(self, tail, head):
        self.check('uncable %s -> %s' % (tail, head), self._send(('Delete', '\tpatch:body [\n\t\ta ingen:Arc ;\n\t\tingen:tail <%s/%s> ;\n\t\tingen:head <%s/%s>\n\t] .\n' % (M, tail, M, head))))

    def set(self, port, value, quiet=False):
        st = self._send(('Set', '\tpatch:subject <%s/%s> ;\n\tpatch:property ingen:value ;\n\tpatch:value "%s"^^xsd:float .\n' % (M, port, repr(float(value)))))
        if not quiet or st: self.check('set %s = %s' % (port, value), st)

    def sets(self, block, values, label):
        for k, v in values.items(): self.set('%s/%s' % (block, k), v, quiet=True)
        print('PASS %s (%d controls)' % (label, len(values)), flush=True)


def base(e, into_mist=True):
    # IN -> Overdrive -> delay; the delay feeds Mist unless a module goes between.
    e.block('od', OD, 312, 40)
    e.block('dly', DLY, 312, 220)
    for t, h in [('in_l', 'clouds/l_in'), ('in_r', 'clouds/r_in')]: e.unarc(t, h)
    for t, h in [('in_l', 'od/left_in'), ('in_r', 'od/right_in'), ('od/left_out', 'dly/in0')]: e.arc(t, h)
    if into_mist:
        for t, h in [('dly/out0', 'clouds/l_in'), ('dly/out0', 'clouds/r_in')]: e.arc(t, h)
    for p, v in [('od/drive', 0.3), ('od/muffle', 0.2), ('od/output', 0.5), ('dly/BPM_0', 120), ('dly/Delay_1', 0.375),
                 ('dly/Feedback_4', 0.4), ('dly/Amp_5', 0.5), ('dly/EnableEcho_7', 1)]: e.set(p, v)


def build_drizzle(e):
    base(e, into_mist=False)
    e.block('drz', DRIZZLE, 520, 220)
    e.block('drzs', DSYNTH, 520, 400)
    for t, h in [('dly/out0', 'drz/in_l'), ('dly/out0', 'drz/in_r'), ('drz/out_l', 'clouds/l_in'), ('drz/out_r', 'clouds/r_in'),
                 ('drzs/out_l', 'clouds/l_in'), ('drzs/out_r', 'clouds/r_in')]: e.arc(t, h)
    e.sets('drz', {'density': 1, 'size': 0.6, 'time': 0.5, 'shape': 0.5, 'pitch': 7, 'feedback': 0.5, 'dry_wet': 0.6,
                   'reverb': 0.5, 'seed_lock': 1, 'quality': 0, 'time_mod': 0.3, 'size_mod': 0.3, 'pitch_mod': 0.2}, 'Drizzle at Density 1')
    e.sets('drzs', {'density': 1, 'wavetable': 0.4, 'osc_grains': 0.7, 'reverb': 0.5, 'quality': 0, 'output': -12}, 'Drizzle Synth at Density 1')

    def phase(i):
        q = i % 4
        e.set('drz/quality', q, quiet=True); e.set('drzs/quality', q, quiet=True)
        e.set('drz/freeze', 1 if i % 8 >= 4 else 0, quiet=True)
        print('PHASE quality %d%s' % (q, ', freeze on' if i % 8 >= 4 else ''), flush=True)
        return 'quality %d' % q
    return phase, None


def build_chamber(e):
    base(e)
    e.block('chm', CHAMBER, 520, 400)
    # The guitar strikes it and the delay blows it; its sound joins the delay into Mist.
    for t, h in [('in_l', 'chm/strike_in'), ('dly/out0', 'chm/blow_in'), ('chm/out_l', 'clouds/l_in'), ('chm/out_r', 'clouds/r_in')]: e.arc(t, h)
    # Heaviest modal case: all three exciters at once, strings model, played every second.
    e.sets('chm', {'model': 2, 'bow_level': 0.8, 'blow_level': 0.8, 'strike_level': 0.8, 'strength': 0.8, 'space': 0.88,
                   'strike_in_level': 0, 'blow_in_level': 0, 'output': -6}, 'Chamber, every exciter, strings model')
    gate = {'on': False}

    def tick():
        gate['on'] = not gate['on']; e.set('chm/play', 1 if gate['on'] else 0, quiet=True)

    def phase(i):
        om = i % 2
        e.set('chm/ominous', om, quiet=True)
        print('PHASE %s' % ('Ominous voice' if om else 'modal voice'), flush=True)
        return 'ominous' if om else 'modal'
    return phase, tick


def build_beat(e):
    base(e, into_mist=False)
    e.block('br', BEAT, 520, 220)
    for t, h in [('dly/out0', 'br/l_in'), ('dly/out0', 'br/r_in'), ('br/l_out', 'clouds/l_in'), ('br/r_out', 'clouds/r_in')]: e.arc(t, h)
    e.sets('br', {'bpm': 120, 'clock_note': 0, 'blend_param': 0.5, 'spread_param': 0, 'texture_param': 7, 'density_param': 0.5,
                  'size_param': 0.7, 'pitch_param': -12, 'reverb_param': 0.5}, 'Beat Repeat on 1/16 at 120 BPM')

    def phase(i):
        mode = i % 5
        e.set('br/feedback_param', mode, quiet=True); e.set('br/freeze_param', i % 2, quiet=True)
        print('PHASE pitch mode %d, Repeat %s' % (mode, 'on' if i % 2 else 'off'), flush=True)
        return 'beat repeat'
    return phase, None


def engine_pid():
    return subprocess.run(['pgrep', '-f', '-o', '^/opt/peepo/bin/ingen '], capture_output=True, text=True).stdout.split()[0]


def watch(seconds, phase=None, tick=None, phase_len=300):
    import jack
    client = jack.Client('peepo_load_check', no_start_server=True)
    xruns = []
    client.set_xrun_callback(lambda delay: xruns.append(delay))
    client.activate()
    pid = engine_pid()
    loads, rss, per = [], [], {}
    label = phase(0) if phase else 'all'
    start = time.time(); n = 0; i = 0; seen = 0
    print('WATCH %d s, %d Hz, %d frames, engine pid %s' % (seconds, client.samplerate, client.blocksize, pid), flush=True)
    while time.time() - start < seconds:
        time.sleep(1); n += 1
        if tick: tick()
        if n % 5: continue
        load = client.cpu_load()
        loads.append(load)
        s = per.setdefault(label, {'loads': [], 'xruns': 0})
        s['loads'].append(load)
        now = len(xruns); s['xruns'] += now - seen; seen = now
        with open('/proc/%s/status' % pid) as f:
            rss.append(int(next(l for l in f if l.startswith('VmRSS')).split()[1]))
        if len(loads) % 60 == 0:
            print('  %4d s: load mean %.1f%% max %.1f%%, xruns %d, RSS %d KiB' % (time.time() - start, sum(loads) / len(loads), max(loads), len(xruns), rss[-1]), flush=True)
        if phase and time.time() - start >= (i + 1) * phase_len and time.time() - start < seconds:
            i += 1
            label = phase(i)
    client.deactivate(); client.close()
    out = {'seconds': seconds, 'xruns': len(xruns), 'load_mean': sum(loads) / len(loads), 'load_max': max(loads),
           'rss_kib_first': rss[0], 'rss_kib_last': rss[-1], 'rss_kib_max': max(rss),
           'phases': {k: {'load_mean': sum(v['loads']) / len(v['loads']), 'load_max': max(v['loads']), 'xruns': v['xruns']} for k, v in per.items()}}
    print('RESULT ' + json.dumps(out), flush=True)


if __name__ == '__main__':
    mode, seconds = sys.argv[1], int(sys.argv[2])
    e = Ingen()
    if mode == 'baseline':
        base(e); watch(seconds)
    else:
        builders = {'drizzle': (build_drizzle, 150), 'chamber': (build_chamber, 300), 'beat': (build_beat, 120)}
        build, plen = builders[mode]
        phase, tick = build(e)
        watch(seconds, phase, tick, plen)
