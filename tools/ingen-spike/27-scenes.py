# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
"""Scenes, measured (P13.1) and the spill bypass checked (P13.3). Run by
27-scenes.sh.

    python3 27-scenes.py tails  CATALOGUE OVERLAY [URI-SUBSTRING]
    python3 27-scenes.py spill  CATALOGUE OVERLAY [URI-SUBSTRING]
    python3 27-scenes.py atomic

tails: every Delay, Reverb and Texture module the catalogue offers, under the
bypass it uses today (the engine's, or the plugin's own switch for
"bypass": "port"). Each check uses a fresh block, wired to the graph's stereo
ports, at its default values:

- tail:   a 30 ms noise burst, bypass 100 ms later; the level from 0.4 to
          1.4 s after the burst, against the same with the module left on.
- click:  a steady 220 Hz tone, bypass in the middle; the largest jump in the
          second difference of the output near the switch, in dB above the
          recording's usual level.
- replay: a burst, bypass 100 ms later, 2.5 s of silence, switched on again;
          the peak level in the 0.8 s after switching on, against the same
          moment in a run where the module was never bypassed.
- side:   for a module with one audio input and two outputs, the right output
          against the left while bypassed with the tone playing. Euclidelay,
          the only such module in WSL, is measured once more under the
          engine's bypass for it (add its build directory to SCENES_LV2).

spill: the same checks with every module that uses the engine's bypass marked
urn:peepo:ns#spillBypass (engine patch 0013). The click is measured switching
off and on again, and one more check:

- sleep:  the engine's CPU use (3 s windows) while the module is on, just
          after bypassing it after a burst, and once its tail has stayed below
          -90 dBFS for 2.5 s (waiting at most 20 s), against the engine with
          no module.

atomic: two eg-amp blocks, A from in_l to out_l and B from in_l to out_r, with
a steady 0.5 on in_l. A scene-like bundle switches them opposite ways (their
bypass, or their gain between 0 and -90 dB) and the frames where out_l and
out_r change are compared. Run once with the engine started plainly and once
with --atomic-bundles.

The probe talks to the engine through short connections, as 26-euclidelay.py
does: a client that is not reading fills its socket with the engine's
broadcasts, and the engine then stalls.
"""
import json
import os
import re
import subprocess
import sys
import threading
import time

import jack
import numpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ingenprobe import Client

MAIN = "ingen:/main/"
SILENT = 1e-9


def db(x):
    return 20 * numpy.log10(max(float(x), SILENT))


def rms(x):
    return float(numpy.sqrt(numpy.mean(numpy.square(x)))) if len(x) else 0.0


class Probe:
    """Sends a signal into the graph and records both outputs, with frame numbers."""

    def __init__(self):
        self.client = jack.Client("scenes-probe")
        self.send_l = self.client.outports.register("send_l")
        self.send_r = self.client.outports.register("send_r")
        self.cap_l = self.client.inports.register("left")
        self.cap_r = self.client.inports.register("right")
        self.lock = threading.Lock()
        self.frame = 0
        self.pending = numpy.zeros(0, dtype=numpy.float32)
        self.tone = 0.0
        self.dc = 0.0
        self.phase = 0.0
        self.chunks = []
        self.recording = False
        self.xruns = 0
        self.client.set_process_callback(self.process)
        self.client.set_xrun_callback(self.xrun)
        self.client.activate()
        self.rate = self.client.samplerate

    def process(self, frames):
        out = numpy.zeros(frames, dtype=numpy.float32)
        with self.lock:
            if self.tone:
                n = numpy.arange(frames)
                out += self.tone * numpy.sin(self.phase + 2 * numpy.pi * 220.0 * n / self.rate)
                self.phase = (self.phase + 2 * numpy.pi * 220.0 * frames / self.rate) % (2 * numpy.pi)
            out += self.dc
            if len(self.pending):
                take = self.pending[:frames]
                out[:len(take)] += take
                self.pending = self.pending[frames:]
            if self.recording:
                self.chunks.append((self.frame, self.cap_l.get_array().copy(), self.cap_r.get_array().copy()))
            self.frame += frames
        self.send_l.get_array()[:] = out
        self.send_r.get_array()[:] = out

    def xrun(self, delay):
        self.xruns += 1

    def now(self):
        with self.lock:
            return self.frame

    def start(self):
        with self.lock:
            self.chunks = []
            self.recording = True

    def stop(self):
        with self.lock:
            self.recording = False
            chunks, self.chunks = self.chunks, []
        if not chunks:
            return 0, numpy.zeros(0), numpy.zeros(0)
        return chunks[0][0], numpy.concatenate([c[1] for c in chunks]), numpy.concatenate([c[2] for c in chunks])

    def burst(self):
        """30 ms of Hann-windowed noise; returns the frame it starts at."""
        n = int(0.03 * self.rate)
        noise = numpy.random.default_rng(7).uniform(-0.5, 0.5, n) * numpy.hanning(n)
        with self.lock:
            self.pending = noise.astype(numpy.float32)
            return self.frame

    def cable(self):
        for _ in range(50):
            names = [p.name for p in self.client.get_ports("ingen:")]
            if all("ingen:" + p in names for p in ("in_l", "in_r", "out_l", "out_r")):
                break
            time.sleep(0.1)
        for a, b in (("scenes-probe:send_l", "ingen:in_l"), ("scenes-probe:send_r", "ingen:in_r"),
                     ("ingen:out_l", "scenes-probe:left"), ("ingen:out_r", "scenes-probe:right")):
            if b not in [p.name for p in self.client.get_all_connections(a)]:
                self.client.connect(a, b)

    def close(self):
        self.client.deactivate()
        self.client.close()


class Engine:
    """A connection for a few requests, closed again straight after."""

    def __enter__(self):
        self.c = Client()
        return self.c

    def __exit__(self, *_):
        self.c.close()


def boolean(on):
    return "true" if on else "false"


def number(x):
    return "\"%s\"^^xsd:float" % x


def graph_ports(probe):
    with Engine() as c:
        for port, kind in (("in_l", "lv2:InputPort , lv2:AudioPort"), ("in_r", "lv2:InputPort , lv2:AudioPort"),
                           ("out_l", "lv2:OutputPort , lv2:AudioPort"), ("out_r", "lv2:OutputPort , lv2:AudioPort")):
            c.put(MAIN + port, "a " + kind)
    time.sleep(0.3)
    probe.cable()


# ---------------------------------------------------------------- tails ----

def present(module, rules):
    module = dict(module)
    for rule in rules:
        if not (module["uri"] == rule["uri"] if "uri" in rule else rule["match"] in module["uri"]):
            continue
        for key in ("name", "category", "bypass"):
            if rule.get(key):
                module[key] = rule[key]
        if rule.get("hidden"):
            module["hidden"] = True
    return module


# Modules silent at their default values, set so that they make a tail.
SETTINGS = {
    "https://github.com/ABustos2606/aura#aura": {"wet_on": 1},
    "https://github.com/ABustos2606/drizzle#drizzle": {"feedback": 0.6, "reverb": 0.5},
    # Third-party modules with no tail at their defaults, for the tail check
    # before each is marked "bypass": "spill" (P13.4).
    "http://guitarix.sourceforge.net/plugins/gxechocat#echocat": {"head1": 1, "head2": 1, "swell": 0.8, "sustain": 0.3},
    "http://guitarix.sourceforge.net/plugins/gx_echo_stereo#_echo_stereo": {"level_l": 60, "level_r": 60, "timt_l": 400, "timt_r": 400},
    "http://drobilla.net/plugins/mda/DubDelay": {"delay": 0.3, "fx_mix": 0.6},
    "http://polyeffects.com/lv2/polyclouds#beat_repeat": {"reverb_param": 0.9},
    "urn:dragonfly:plate": {"decay": 3, "early_level": 50},
    "urn:dragonfly:room": {"decay": 3, "late_level": 50},
    "http://guitarix.sourceforge.net/plugins/gx_mbreverb_#_mbreverb_": dict(
        [("ROOMSIZE%d" % b, 0.95) for b in range(1, 6)] + [("WET_DRY%d" % b, 100) for b in range(1, 6)]),
}
# Measured a second time with these values, under a name of their own.
# Discipline's Trails default to 1 from 0.4.1 (P13.5); before, they were 0.
VARIANTS = {
    "https://github.com/ABustos2606/discipline#discipline": ("Discipline, Trails off", {"trails": 0}),
}
# Not a tail: bypassing a looper mutes its loops on purpose.
LOOPERS = ("https://github.com/ABustos2606/loo-pi#loopi",)
SPILL = os.environ.get("SCENES_SPILL") == "1"
# The click switching off and on again, as under the spill bypass (P13.5).
ON_AGAIN = SPILL or os.environ.get("SCENES_ON_AGAIN") == "1"
# A directory to keep each recording whose click is over 20 dB in.
DUMP = os.environ.get("SCENES_DUMP")
# One input and two outputs, measured again under the engine's bypass for the
# side check.
MONO_TO_STEREO = {
    "https://github.com/ABustos2606/euclidelay#euclidelay": "Euclidelay, engine bypass",
}


class Block:
    count = 0

    def __init__(self, module):
        Block.count += 1
        self.module = module
        self.path = MAIN + "scene%d" % Block.count
        self.by_port = module.get("bypass") == "port"
        self.enable = module.get("enable") or ""
        audio = [p for p in module["ports"] if p["type"] == "audio"]
        self.ins = [p["symbol"] for p in audio if p["direction"] == "input"][:2]
        self.outs = [p["symbol"] for p in audio if p["direction"] == "output"][:2]

    def create(self):
        with Engine() as c:
            status = c.put(self.path, "a ingen:Block ;\n\t\tlv2:prototype <%s>" % self.module["uri"], timeout=20)[0]
            if status != 0:
                return status
            if self.enable:
                c.set(self.path + "/" + self.enable, "ingen:value", number(1))
            # As the adapter does for a module the overlay marks "spill".
            if (SPILL and not self.by_port) or self.module.get("bypass") == "spill":
                c.set(self.path, "peepo:spillBypass", "true")
            for symbol, value in self.module.get("settings", {}).items():
                c.set(self.path + "/" + symbol, "ingen:value", number(value))
            for port, symbol in zip(("in_l", "in_r"), self.ins):
                c.connect(MAIN + port, self.path + "/" + symbol)
            for port, symbol in zip(("out_l", "out_r"), self.outs):
                c.connect(self.path + "/" + symbol, MAIN + port)
        return 0

    def bypass(self, on):
        """As the adapter does it: the plugin's own switch, or the engine's."""
        with Engine() as c:
            if self.by_port:
                c.set(self.path + "/" + self.enable, "ingen:value", number(0 if on else 1))
            else:
                c.set(self.path, "ingen:enabled", boolean(not on))

    def delete(self):
        with Engine() as c:
            c.delete(self.path, timeout=10)
        time.sleep(0.2)


def window(start_frame, signal, first, at, seconds, rate):
    """`seconds` of `signal` starting at frame `at` (absolute)."""
    a = max(0, int(at - first))
    return signal[a:a + int(seconds * rate)]


def run_tail(probe, module, latency, bypass):
    block = Block(module)
    status = block.create()
    if status != 0:
        return None, status
    time.sleep(0.6)
    probe.start()
    start = probe.burst()
    time.sleep(0.1)
    if bypass:
        block.bypass(True)
    time.sleep(1.6)
    first, left, right = probe.stop()
    block.delete()
    rate = probe.rate
    tail = window(first, left, first, start + latency + 0.4 * rate, 1.0, rate)
    tail_r = window(first, right, first, start + latency + 0.4 * rate, 1.0, rate)
    return max(rms(tail), rms(tail_r)), 0


def run_click(probe, module):
    """The click, measured again (at most three times in all) when JACK
    reported an xrun during the recording: a lost cycle is a jump in the tone
    on a cycle boundary, not the module's click (P13.5)."""
    for attempt in range(3):
        worst, side, xruns = click_once(probe, module)
        if not xruns:
            break
        print("%-28s xrun during the click recording (%.1f dB): measured again" % (module["name"], worst or 0), flush=True)
    return worst, side


def click_once(probe, module):
    block = Block(module)
    if block.create() != 0:
        return None, None, 0
    with probe.lock:
        probe.tone, probe.phase = 0.25, 0.0
    time.sleep(0.8)
    xruns = probe.xruns
    probe.start()
    time.sleep(0.5)
    sent = probe.now()
    block.bypass(True)
    if ON_AGAIN:
        # Off and on again: both ramps are inside the window measured below.
        time.sleep(0.2)
        block.bypass(False)
    time.sleep(0.7)
    first, left, right = probe.stop()
    with probe.lock:
        probe.tone = 0.0
    block.delete()
    rate = probe.rate
    worst = 0.0
    for channel in (left, right) if len(block.outs) > 1 else (left,):
        d2 = numpy.abs(numpy.diff(channel, n=2))
        size = int(0.0025 * rate)
        peaks = numpy.array([d2[i:i + size].max() for i in range(0, len(d2) - size, size)])
        if not len(peaks):
            continue
        usual = numpy.median(peaks)
        a = max(0, int((sent - first) / size) - 20)
        near = peaks[a:a + int(0.4 * rate / size)]
        if len(near):
            worst = max(worst, db(near.max()) - db(usual))
    if DUMP and worst > 20:
        # The recording kept, with the xruns JACK reported during it.
        os.makedirs(DUMP, exist_ok=True)
        name = re.sub(r"[^A-Za-z0-9]+", "-", module["name"]) + "-%d" % int(time.time() * 1000)
        numpy.savez(os.path.join(DUMP, name), left=left, right=right, first=first, sent=sent,
                    xruns=probe.xruns - xruns, click=worst)
        print("%-28s click %.1f dB kept as %s.npz, xruns during it: %d" % (
            module["name"], worst, name, probe.xruns - xruns), flush=True)
    side = None
    if len(block.ins) == 1 and len(block.outs) == 2:
        end = int(0.3 * rate)
        side = db(rms(right[-end:])) - db(rms(left[-end:]))
    return worst, side, probe.xruns - xruns


def run_replay(probe, module, bypass):
    """Peak level in the 0.8 s from 2.6 s after a burst; with `bypass`, the
    module is bypassed from 0.1 s and switched on again at 2.6 s."""
    block = Block(module)
    if block.create() != 0:
        return None
    time.sleep(0.6)
    probe.start()
    start = probe.burst()
    time.sleep(0.1)
    if bypass:
        block.bypass(True)
    time.sleep(2.5)
    if bypass:
        block.bypass(False)
    time.sleep(1.0)
    first, left, right = probe.stop()
    block.delete()
    rate = probe.rate
    return db(max(numpy.abs(window(first, s, first, start + 2.6 * rate, 0.8, rate)).max(initial=0) for s in (left, right)))


def cpu_seconds():
    """The engine's CPU time so far, in seconds."""
    pid = subprocess.run(["pgrep", "-n", "-x", "ingen"], capture_output=True, text=True).stdout.strip()
    fields = open("/proc/%s/stat" % pid).read().rsplit(")", 1)[1].split()
    return (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")


def cpu_over(seconds):
    """The engine's CPU use over the next `seconds`, in percent of one core."""
    a = cpu_seconds()
    time.sleep(seconds)
    return 100 * (cpu_seconds() - a) / seconds


def run_sleep(probe, module):
    """CPU on, just after a spill bypass, and once the tail has ended."""
    block = Block(module)
    if block.create() != 0:
        return None
    time.sleep(0.6)
    on = cpu_over(3.0)
    probe.start()
    start = probe.burst()
    time.sleep(0.1)
    block.bypass(True)
    spilling = cpu_over(3.0)
    rate = probe.rate
    # Wait for the tail (the dry signal is silent) to fall below -90 dBFS.
    waited = 3.1
    quiet_from = None
    while waited < 20:
        time.sleep(0.5)
        waited += 0.5
        with probe.lock:
            chunks = list(probe.chunks)
        loud = [f for f, l, r in chunks if max(numpy.abs(l).max(), numpy.abs(r).max()) >= 3.1623e-5]
        last = (loud[-1] - start) / rate if loud else 0.0
        if waited - last > 2.5:
            quiet_from = last
            break
    late = cpu_over(3.0)
    probe.stop()
    block.delete()
    return {"cpu_on": round(on, 1), "cpu_spilling": round(spilling, 1), "cpu_after_tail": round(late, 1),
            "tail_ends_s": None if quiet_from is None else round(quiet_from, 1)}


def tails(catalogue, overlay, only):
    rules = json.load(open(overlay, encoding="utf-8"))["rules"]
    modules = [present(m, rules) for m in json.load(open(catalogue, encoding="utf-8"))["modules"]]
    modules = [m for m in modules if m["availability"] == "available" and not m.get("hidden")
               and m["category"] in ("Delay", "Reverb", "Texture") and (not only or only in m["uri"])]
    modules = [m for m in modules if any(p["type"] == "audio" and p["direction"] == "input" for p in m["ports"])]
    for m in modules:
        m["settings"] = SETTINGS.get(m["uri"], {})
    for m in list(modules):
        if m["uri"] in VARIANTS:
            name, settings = VARIANTS[m["uri"]]
            modules.append(dict(m, name=name, settings=dict(m["settings"], **settings)))
        if m["uri"] in MONO_TO_STEREO:
            modules.append(dict(m, name=MONO_TO_STEREO[m["uri"]], bypass=None))
    modules.sort(key=lambda m: (m["category"], m["name"]))
    probe = Probe()
    graph_ports(probe)
    with Engine() as c:
        c.connect(MAIN + "in_l", MAIN + "out_l")
    probe.start()
    start = probe.burst()
    time.sleep(0.4)
    first, left, _ = probe.stop()
    loud = numpy.flatnonzero(numpy.abs(left) > 0.01)
    latency = int(first + loud[0] - start) if len(loud) else 0
    with Engine() as c:
        c.disconnect(MAIN + "in_l", MAIN + "out_l")
    print("round trip without a module: %d frames" % latency, flush=True)
    spilling = SPILL or any(m.get("bypass") == "spill" for m in modules)
    idle = cpu_over(3.0) if spilling else None
    if spilling:
        print("engine CPU with no module: %.1f%%" % idle, flush=True)

    results = []
    for m in modules:
        r = {"uri": m["uri"], "name": m["name"], "category": m["category"],
             "bypass": m.get("bypass") if m.get("bypass") in ("port", "spill") else "spill" if SPILL else "engine",
             "audio": "%d in / %d out" % (len(Block(m).ins), len(Block(m).outs))}
        on, status = run_tail(probe, m, latency, False)
        if on is None:
            r["result"] = "not installed in this engine (status %s)" % status
            print("%-28s %-8s %s" % (m["name"], m["category"], r["result"]), flush=True)
            results.append(r)
            continue
        off, _ = run_tail(probe, m, latency, True)
        click, side = run_click(probe, m)
        before = run_replay(probe, m, False)
        after = run_replay(probe, m, True)
        if r["bypass"] == "spill":
            r["sleep"] = run_sleep(probe, m)
            r["sleep"]["cpu_no_module"] = round(idle, 1)
        r.update({"settings": m["settings"], "tail_on_dbfs": round(db(on), 1), "tail_bypassed_dbfs": round(db(off), 1),
                  "click_db": round(click, 1) if click is not None else None,
                  "replay_never_bypassed_dbfs": round(before, 1), "replay_after_dbfs": round(after, 1),
                  "right_vs_left_db": None if side is None else round(side, 1)})
        if m["uri"] in LOOPERS:
            r["tail"] = "looper: mutes on purpose"
        elif db(on) < -70:
            r["tail"] = "no tail at these values"
        elif db(off) - db(on) >= -3:
            r["tail"] = "rings on"
        elif db(off) < -80 or db(off) - db(on) <= -40:
            r["tail"] = "cut"
        else:
            r["tail"] = "partly cut (%.0f dB)" % (db(off) - db(on))
        r["click"] = bool(click is not None and click > 20)
        r["replay"] = bool(after > -70 and after - max(before, -90) > 10)
        r["side_lost"] = bool(side is not None and side < -40)
        print("%-28s %-8s %-6s tail %-22s on %6.1f off %6.1f | click %5.1f dB%s | replay %6.1f, never bypassed %6.1f%s%s" % (
            m["name"], m["category"], r["bypass"], r["tail"], db(on), db(off), click or 0,
            " CLICK" if r["click"] else "", after, before, " REPLAY" if r["replay"] else "",
            "" if side is None else " | right %+.0f dB%s" % (side, " LOST" if r["side_lost"] else "")), flush=True)
        if r.get("sleep"):
            z = r["sleep"]
            print("%-28s CPU %%: on %.1f, spilling %.1f, after the tail %.1f (no module %.1f); tail ends %s s" % (
                "", z["cpu_on"], z["cpu_spilling"], z["cpu_after_tail"], idle, z["tail_ends_s"]), flush=True)
        results.append(r)
    probe.close()
    return results


# --------------------------------------------------------------- atomic ----

AMP = "http://lv2plug.in/plugins/eg-amp"


def send_bundle(c, messages):
    """Every message at once, wrapped in BundleStart/BundleEnd; then the replies."""
    first = c.seq + 1
    text = "[] a ingen:BundleStart ;\n\tpatch:sequenceNumber \"%d\"^^xsd:int .\n" % first
    c.seq = first
    for body in messages:
        c.seq += 1
        text += "[]\n\t%s ;\n\tpatch:sequenceNumber \"%d\"^^xsd:int .\n" % (body, c.seq)
    c.seq += 1
    text += "[] a ingen:BundleEnd ;\n\tpatch:sequenceNumber \"%d\"^^xsd:int .\n" % c.seq
    c.send_raw(text)
    deadline = time.monotonic() + 3
    seen = set()
    while time.monotonic() < deadline and len(seen) < c.seq - first + 1:
        m = c.read_message(deadline - time.monotonic())
        if m is None:
            break
        seen.update(int(n) for n in re.findall(r'patch:sequenceNumber "(\d+)"', m) if first <= int(n) <= c.seq)


def set_message(subject, key, value):
    return "a patch:Set ;\n\tpatch:subject <%s> ;\n\tpatch:property %s ;\n\tpatch:value %s" % (subject, key, value)


def change_frame(signal, before, after):
    """First frame where the signal is nearer `after` than `before`."""
    near = numpy.flatnonzero(numpy.abs(signal - after) < numpy.abs(signal - before))
    return int(near[0]) if len(near) else None


def atomic():
    probe = Probe()
    graph_ports(probe)
    a, b = MAIN + "amp_a", MAIN + "amp_b"
    with Engine() as c:
        for block, out in ((a, "out_l"), (b, "out_r")):
            status = c.put(block, "a ingen:Block ;\n\t\tlv2:prototype <%s>" % AMP)[0]
            if status != 0:
                print("FAIL  eg-amp is not installed (status %s)" % status)
                return None
            c.connect(MAIN + "in_l", block + "/in")
            c.connect(block + "/out", MAIN + out)
    with probe.lock:
        probe.dc = 0.5
    results = {}
    trials = 40
    for kind in ("bypass", "parameter", "mixed"):
        # Start: left silent, right playing.
        with Engine() as c:
            if kind == "bypass":
                for block in (a, b):
                    c.set(block + "/gain", "ingen:value", number(-90))
                c.set(a, "ingen:enabled", "true")
                c.set(b, "ingen:enabled", "false")
            elif kind == "parameter":
                for block in (a, b):
                    c.set(block, "ingen:enabled", "true")
                c.set(a + "/gain", "ingen:value", number(-90))
                c.set(b + "/gain", "ingen:value", number(0))
            else:
                for block in (a, b):
                    c.set(block, "ingen:enabled", "true")
                c.set(a + "/gain", "ingen:value", number(-90))
                c.set(b + "/gain", "ingen:value", number(0))
        time.sleep(0.3)
        gaps = []
        left_on = False
        for _ in range(trials):
            left_on = not left_on
            if kind == "bypass":
                # Bypassed passes 0.5; on at -90 dB gives 0.
                messages = [set_message(a, "ingen:enabled", boolean(not left_on)),
                            set_message(b, "ingen:enabled", boolean(left_on))]
            elif kind == "parameter":
                messages = [set_message(a + "/gain", "ingen:value", number(0 if left_on else -90)),
                            set_message(b + "/gain", "ingen:value", number(-90 if left_on else 0))]
            else:
                messages = [set_message(a, "ingen:enabled", boolean(not left_on)),
                            set_message(b + "/gain", "ingen:value", number(-90 if left_on else 0))]
            probe.start()
            time.sleep(0.03)
            with Engine() as c:
                send_bundle(c, messages)
            time.sleep(0.08)
            _, left, right = probe.stop()
            fl = change_frame(left, 0.0 if left_on else 0.5, 0.5 if left_on else 0.0)
            fr = change_frame(right, 0.5 if left_on else 0.0, 0.0 if left_on else 0.5)
            gaps.append(None if fl is None or fr is None else fr - fl)
        heard = [g for g in gaps if g is not None]
        together = sum(1 for g in heard if g == 0)
        results[kind] = {"trials": trials, "both_changed": len(heard), "same_frame": together,
                         "largest_gap_frames": max((abs(g) for g in heard), default=None)}
        print("%-9s: %d of %d bundles changed both outputs; %d in the same frame; largest gap %s frames" % (
            kind, len(heard), trials, together, results[kind]["largest_gap_frames"]), flush=True)
    with probe.lock:
        probe.dc = 0.0
    with Engine() as c:
        c.delete(a)
        c.delete(b)
    probe.close()
    return results


def main():
    mode = sys.argv[1]
    out = os.environ.get("SCENES_RESULTS")
    if mode in ("tails", "spill"):
        results = tails(sys.argv[2], sys.argv[3], sys.argv[4] if len(sys.argv) > 4 else "")
    else:
        results = atomic()
    if out and results is not None:
        with open(out, "w", encoding="utf-8", newline="\n") as f:
            json.dump(results, f, indent=1)
    sys.exit(0 if results is not None else 1)


main()
