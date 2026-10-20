# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
# MIDI clock out (P12.3) against a live engine: starts peepo-midi, records
# every message on peepo-midi:clock_out with its frame, and changes the tempo
# through the engine as the interface would. Run by 24-clock.sh.
#   python3 24-clock.py BUILD CATALOGUE
import signal
import subprocess
import sys
import time
import jack

build, catalogue = sys.argv[1], sys.argv[2]
engine = "unix:///tmp/peepo-ingen.sock"
failures = 0
def check(ok, name, detail=""):
    global failures
    print("%s  %s%s" % ("PASS" if ok else "FAIL", name, " -- " + detail if detail else ""), flush=True)
    failures += 0 if ok else 1

client = jack.Client("clockprobe")
port = client.midi_inports.register("clock")
rate = client.samplerate
seen = []  # (absolute frame, status byte)
# Python in WSL sometimes misses a period itself (JACK logs "clockprobe was not
# finished"). Its own xruns are recorded, and an interval across one is not
# held against the clock.
xruns = []
@client.set_xrun_callback
def xrun(delay):
    xruns.append(client.frame_time)
@client.set_process_callback
def process(frames):
    start = client.last_frame_time
    for offset, data in port.incoming_midi_events():
        seen.append((start + offset, bytes(data)[0]))
client.activate()
def now():
    return client.frame_time
def interval(bpm):
    return rate * 60.0 / (bpm * 24)
def ticks_between(a, b):
    return [f for f, s in seen if s == 0xF8 and a <= f < b]
excluded = 0
def spacing(frames):
    global excluded
    margin = rate // 10  # when JACK reports it, not exactly when it happened
    gaps = []
    for a, b in zip(frames, frames[1:]):
        if any(a - margin <= x <= b + margin for x in xruns): excluded += 1
        else: gaps.append(b - a)
    return gaps
def steady(name, bpm, since, seconds=2.0):
    time.sleep(seconds)
    gaps = spacing(ticks_between(since, now()))
    want = interval(bpm)
    ok = len(gaps) >= 8 and all(abs(g - want) <= 1 for g in gaps)
    check(ok, "%s: clock every %.2f frames (%g BPM)" % (name, want, bpm),
          "%d intervals, %d to %d frames" % (len(gaps), min(gaps or [0]), max(gaps or [0])))
def adapter(*args):
    run = subprocess.run([build + "/peepo_ingen_live", engine, catalogue, *args], capture_output=True, text=True, timeout=60)
    if run.returncode != 0: print(run.stdout + run.stderr, flush=True)
    return run.returncode == 0
def changes(name, old, new, since):
    # From the next clock message on: every interval is the old tempo's or the
    # new one's, and once the new one starts the old one never returns.
    gaps = spacing(ticks_between(since, now()))
    a, b = interval(old), interval(new)
    kinds = ["old" if abs(g - a) <= 1 else "new" if abs(g - b) <= 1 else "other" for g in gaps]
    first_new = kinds.index("new") if "new" in kinds else len(kinds)
    ok = "other" not in kinds and "new" in kinds and all(k == "new" for k in kinds[first_new:])
    odd = [g for g, k in zip(gaps, kinds) if k == "other"]
    check(ok, "%s: %g to %g BPM from one clock message to the next" % (name, old, new),
          "%d old intervals, then %d new%s" % (first_new, len(kinds) - first_new, ", other intervals %s" % odd[:6] if odd else ""))

midi = subprocess.Popen([build + "/peepo-midi", "--engine", engine], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
for _ in range(100):
    if "peepo-midi:clock_out" in [p.name for p in client.get_ports("peepo-midi:")]: break
    time.sleep(0.05)
client.connect("peepo-midi:clock_out", "clockprobe:clock")
mark = now()
time.sleep(1)
check(seen and seen[0][1] == 0xFA and len(seen) > 1 and seen[1][1] == 0xF8 and seen[1][0] == seen[0][0],
      "Start once, with the first clock message", " ".join("%02x" % s for _, s in seen[:4]))
check(sum(1 for _, s in seen if s == 0xFA) == 1, "Start is sent once")

check(adapter("--set-tempo", "120"), "the engine plays at 120 BPM")
steady("120 BPM", 120, now())
for old, new in ((120, 30), (30, 300), (300, 120)):
    since = now()
    check(adapter("--set-tempo", str(new)), "set the tempo to %g BPM, as the Tempo page or a tap does" % new)
    time.sleep(2)
    changes("tempo change", old, new, since)
    steady("%g BPM" % new, new, now())
# A patch saved at 87 BPM is loaded while 100 plays. Loading passes through
# no other tempo: no interval of the default 120 BPM, or any other, between.
check(adapter("--save-tempo-patch", "87", "100"), "save a patch at 87 BPM, then play 100 BPM")
steady("100 BPM", 100, now())
since = now()
check(adapter("--load-tempo-patch"), "load the patch saved at 87 BPM")
time.sleep(2)
changes("patch change", 100, 87, since)
steady("87 BPM", 87, now())
# An interface that dies without a word, and a new one: the clock never notices.
since = now()
for _ in range(3):
    gone = subprocess.Popen([build + "/peepo_ingen_live", engine, catalogue, "--wait"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(0.7)
    gone.kill(); gone.wait()
steady("while three interfaces connect and are killed", 87, since, 0.5)
# The whole run: never a gap, never two clocks closer than the fastest tempo.
gaps = spacing(ticks_between(mark, now()))
check(gaps and max(gaps) <= interval(30) + 1 and min(gaps) >= interval(300) - 1, "no gap or burst in the whole run",
      "%d intervals, %d to %d frames" % (len(gaps), min(gaps), max(gaps)))
# Stopping the service sends Stop last.
midi.send_signal(signal.SIGTERM)
log, _ = midi.communicate(timeout=10)
time.sleep(0.3)
check(midi.returncode == 0 and seen[-1][1] == 0xFC, "stopping peepo-midi sends Stop last", "exit %s, last %02x" % (midi.returncode, seen[-1][1]))
print("".join("       | " + line + "\n" for line in log.splitlines()[-12:]), end="")
client.deactivate(); client.close()
print("INFO  the probe's own xruns: %d; intervals left out around them: %d (counted once per check)" % (len(xruns), excluded))
print("SUMMARY %s" % ("%d failed" % failures if failures else "MIDI clock checks passed"))
sys.exit(1 if failures else 0)
