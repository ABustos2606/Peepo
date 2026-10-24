# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
"""Euclidelay and the patch tempo, measured. Run by 26-euclidelay.sh.

    python3 26-euclidelay.py BUILD CATALOGUE

Voice 1 alone at Steps 4, Hits 4, Feedback 0, Step 1/16, Dry 0: an impulse
comes back as three echoes one, two and three sixteenths later. The tempo is
set through Peepo's adapter, as the Tempo page or a tap sets it, and the
echoes must move with it; a patch saved at 87 BPM and loaded while 100 BPM
plays must come back at 87. Read through the graph's audio ports, which the
engine gives JACK. The probe talks to the engine only between the adapter's
runs: a client that is not reading fills its socket with the engine's
broadcasts, and the engine then stalls writing to it.
"""
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

URI = "https://github.com/ABustos2606/euclidelay#euclidelay"
BLOCK = "ingen:/main/euclidelay"
ENGINE = "unix:///tmp/peepo-ingen.sock"
build, catalogue = sys.argv[1], sys.argv[2]
failed = 0


def report(ok, text):
    global failed
    failed += 0 if ok else 1
    print("%s  %s" % ("PASS" if ok else "FAIL", text), flush=True)


class Probe:
    def __init__(self):
        self.client = jack.Client("euclidelay-probe")
        self.out = self.client.outports.register("send")
        self.cap = self.client.inports.register("audio")
        self.lock = threading.Lock()
        self.impulse = False
        self.audio = []
        self.client.set_process_callback(self.process)
        self.client.activate()
        self.rate = self.client.samplerate

    def process(self, frames):
        send = self.out.get_array()
        send[:] = 0
        with self.lock:
            if self.impulse:
                send[0] = 0.5
                self.impulse = False
            self.audio.append(self.cap.get_array().copy())

    def cable(self):
        # The engine's ports are made again when a patch is loaded.
        for _ in range(50):
            names = [p.name for p in self.client.get_ports("ingen:")]
            if "ingen:in_l" in names and "ingen:out_l" in names:
                break
            time.sleep(0.1)
        for a, b in (("euclidelay-probe:send", "ingen:in_l"), ("ingen:out_l", "euclidelay-probe:audio")):
            if b not in [p.name for p in self.client.get_all_connections(a)]:
                self.client.connect(a, b)

    def record(self, seconds, impulse=False):
        with self.lock:
            self.audio = []
            self.impulse = impulse
        time.sleep(seconds)
        with self.lock:
            return numpy.concatenate(self.audio)


def onsets(signal, count):
    """The first `count` places where the signal rises out of silence."""
    found, at = [], 0
    while len(found) < count:
        loud = numpy.flatnonzero(numpy.abs(signal[at:]) > 0.01)
        if not len(loud):
            break
        found.append(at + int(loud[0]))
        at = found[-1] + 2000
    return found


class Engine:
    """A connection for a few requests, closed again before the adapter runs."""
    def __enter__(self):
        self.c = Client()
        return self.c

    def __exit__(self, *_):
        self.c.close()


def value(c, symbol, number):
    c.set(BLOCK + "/" + symbol, "ingen:value", "\"%s\"^^xsd:float" % number)


def read(symbol):
    with Engine() as c:
        status, reply, others = c.get(BLOCK + "/" + symbol)
    found = re.findall(r'ingen:value "?([0-9.eE+-]+)', "".join(others) + reply)
    return float(found[0]) if status == 0 and found else None


def adapter(*args):
    run = subprocess.run([build + "/peepo_ingen_live", ENGINE, catalogue, *args], capture_output=True, text=True, timeout=60)
    if run.returncode != 0:
        print(run.stdout + run.stderr, flush=True)
    return run.returncode == 0


def echoes(probe, base, bpm, label):
    time.sleep(1.0)  # the last echoes die away
    audio = probe.record(1.4, impulse=True)
    step = 60.0 / bpm / 4 * probe.rate
    heard = [n - base for n in onsets(audio, 4)]
    expected = [round(step * k) for k in (1, 2, 3)]
    ok = len(heard) == 3 and heard[0] > 100 and all(abs(h - e) <= 2 for h, e in zip(heard, expected))
    report(ok, "%s: echoes after %s samples (expected %s, no dry signal, nothing after)" % (label, heard, expected))


def main():
    probe = Probe()
    with Engine() as c:
        for port, kind in (("in_l", "lv2:InputPort , lv2:AudioPort"), ("out_l", "lv2:OutputPort , lv2:AudioPort")):
            c.put("ingen:/main/" + port, "a " + kind)
        time.sleep(0.3)
        probe.cable()
        # The round trip through JACK and the engine, without a module.
        c.connect("ingen:/main/in_l", "ingen:/main/out_l")
    base = onsets(probe.record(0.5, impulse=True), 1)
    with Engine() as c:
        c.disconnect("ingen:/main/in_l", "ingen:/main/out_l")
    report(len(base) == 1, "the measurement itself -- a direct cable returns the impulse after %s samples" % (base[0] if base else "no"))
    base = base[0] if base else 0

    report(adapter("--set-tempo", "120"), "the patch plays at 120 BPM")
    with Engine() as c:
        status = c.put(BLOCK, "a ingen:Block ;\n\t\tlv2:prototype <%s>" % URI)[0]
        report(status == 0, "the engine loads Euclidelay (status %s)" % status)
        for symbol, number in (("voice_2", 0), ("dry", 0), ("steps_1", 4), ("hits_1", 4), ("shift_1", 0), ("feedback_1", 0), ("pan_1", -1)):
            value(c, symbol, number)
        c.connect("ingen:/main/in_l", BLOCK + "/in")
        c.connect(BLOCK + "/out_l", "ingen:/main/out_l")
    echoes(probe, base, 120, "120 BPM, 1/16")

    for bpm in (97, 143):
        report(adapter("--set-tempo", str(bpm)), "set the patch tempo to %d BPM, as the Tempo page does" % bpm)
        got = read("bpm")
        report(got is not None and abs(got - bpm) < 1e-3, "Euclidelay's BPM follows: %s" % got)
        echoes(probe, base, bpm, "%d BPM, 1/16" % bpm)

    report(adapter("--save-tempo-patch", "87", "100"), "save the patch at 87 BPM, then play 100 BPM")
    got = read("bpm")
    report(got is not None and abs(got - 100) < 1e-3, "Euclidelay's BPM follows to 100: %s" % got)
    report(adapter("--load-tempo-patch"), "load the patch saved at 87 BPM")
    time.sleep(0.5)
    got = read("bpm")
    report(got is not None and abs(got - 87) < 1e-3, "after loading, Euclidelay's BPM is the patch's: %s" % got)
    probe.cable()
    echoes(probe, base, 87, "the loaded patch, 87 BPM")

    with Engine() as c:
        c.delete(BLOCK)
        time.sleep(0.2)
    probe.client.deactivate()
    probe.client.close()
    sys.exit(1 if failed else 0)


main()
