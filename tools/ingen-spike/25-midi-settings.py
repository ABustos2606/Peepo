# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
# Settings > MIDI against a live engine (P12.6-P12.8): peepo-midi reads the
# settings file the interface writes, and a JACK MIDI probe stands in for a
# device. Program change loads stored patches by number on the chosen
# channel; the clock out restarts, goes without Start and Stop, and stops;
# with the tempo taken from incoming clock the patch follows it without being
# marked as edited; and MIDI learn binds a controller through the engine's
# control port, saved with the patch. Run by 25-midi-settings.sh.
#   python3 25-midi-settings.py BUILD CATALOGUE
import os
import shutil
import subprocess
import sys
import time
import jack

build, catalogue = sys.argv[1], sys.argv[2]
engine = "unix:///tmp/peepo-ingen.sock"
work = "/tmp/peepo-midi-settings"
settings, run, patches = work + "/midi.conf", work + "/run", work + "/patches"
failures = 0
def check(ok, name, detail=""):
    global failures
    print("%s  %s%s" % ("PASS" if ok else "FAIL", name, " -- " + detail if detail else ""), flush=True)
    failures += 0 if ok else 1
def adapter(*args):
    done = subprocess.run([build + "/peepo_ingen_live", engine, catalogue, *args], capture_output=True, text=True, timeout=60)
    if done.returncode != 0: print(done.stdout + done.stderr, flush=True)
    return done
def state():
    for line in adapter("--state").stdout.splitlines():
        if line.startswith("STATE "):
            return dict(part.split("=", 1) for part in line[6:].split(" ") if "=" in part)
    return {}
def configure(**values):
    lines = ["%s=%s" % (k, v) for k, v in values.items()]
    with open(settings + ".new", "w") as f: f.write("\n".join(lines) + "\n")
    os.replace(settings + ".new", settings)
    time.sleep(0.6)  # read again within 250 ms

shutil.rmtree(work, ignore_errors=True)
os.makedirs(run)
check(adapter("--midi-patches", patches).returncode == 0, "store Alpha (90 BPM), Bravo (100) and Charlie (110), then play an unsaved patch")

client = jack.Client("settingsprobe")
send = client.midi_outports.register("send")
hear = client.midi_inports.register("hear")
rate = client.samplerate
queue, seen, controls = [], [], []
clock = {"interval": 0, "next": 0}
frame = [0]
@client.set_process_callback
def process(frames):
    # JACK's own frame time: a period Python misses in WSL still counts.
    frame[0] = client.last_frame_time
    send.clear_buffer()
    while queue: send.write_midi_event(0, queue.pop(0))
    # Incoming clock at an exact interval, when asked for.
    if clock["interval"]:
        while clock["next"] < frame[0] + frames:
            send.write_midi_event(max(0, int(clock["next"] - frame[0])), b"\xf8")
            clock["next"] += clock["interval"]
    for offset, data in hear.incoming_midi_events():
        seen.append((frame[0] + offset, bytes(data)[0]))
        if bytes(data)[0] & 0xF0 == 0xB0: controls.append(bytes(data))
client.activate()

midi = subprocess.Popen([build + "/peepo-midi", "--engine", engine, "--settings", settings, "--status", run, "--patches", patches, "--catalogue", catalogue],
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
for _ in range(100):
    if "peepo-midi:midi_in" in [p.name for p in client.get_ports("peepo-midi:")]: break
    time.sleep(0.05)
client.connect("settingsprobe:send", "peepo-midi:midi_in")
client.connect("peepo-midi:clock_out", "settingsprobe:hear")
time.sleep(1.5)
check(os.path.exists(run + "/status"), "peepo-midi tells the interface what it hears", open(run + "/status").read().strip() or "(no devices, no clock)" if os.path.exists(run + "/status") else "")
check(any(s == 0xF8 for _, s in seen), "with no settings file, the clock goes out")

# Program change.
def program(message):
    queue.append(message); time.sleep(1.5)
    return state()
s = program(b"\xc0\x01")
check(s.get("name") == "Scratch", "program change while it is off changes nothing", str(s))
configure(program_change=1)
del controls[:]
s = program(b"\xc0\x01")
check(s.get("name") == "Bravo" and float(s.get("tempo", 0)) == 100, "program 2 on channel 1 loads the second stored patch, with its tempo", str(s))
check(controls == [b"\xb0\x07\x64", b"\xb1\x0a\x40"], "Bravo's two control changes go out as it loads", " ".join(c.hex(" ") for c in controls))
s = program(b"\xc1\x02")
check(s.get("name") == "Bravo", "a program change on another channel is ignored", str(s))
s = program(b"\xc0\x09")
check(s.get("name") == "Bravo", "a program with no stored patch changes nothing", str(s))
configure(program_change="any")
s = program(b"\xc5\x02")
check(s.get("name") == "Charlie" and s.get("dirty") == "0", "with any channel, program 3 on channel 6 loads Charlie", str(s))

# Scene change (P13.9): CC 69 unless set otherwise, on the program-change
# channel; values 0 to 7 play scenes 1 to 8.
check(s.get("scene") == "3/3", "Charlie loads with the third of its scenes playing, as saved", str(s))
s = program(b"\xb4\x45\x00")
check(s.get("scene") == "1/3" and s.get("dirty") == "0", "CC 69 value 0 plays scene 1, the patch left unedited", str(s))
s = program(b"\xb4\x46\x01")
check(s.get("scene") == "1/3", "another controller plays no scene", str(s))
s = program(b"\xb4\x45\x05")
check(s.get("scene") == "1/3", "a scene the patch does not have changes nothing", str(s))
configure(program_change="any", scene_cc=11)
s = program(b"\xb4\x0b\x01")
check(s.get("scene") == "2/3", "the scene controller follows the setting: CC 11 value 1 plays scene 2", str(s))
s = program(b"\xc0\x00")
check(s.get("name") == "Alpha" and s.get("scene") == "0/0", "program 1 loads Alpha, which has no scenes", str(s))
queue.extend([b"\xc0\x02", b"\xb0\x0b\x00"])
time.sleep(1.5)
s = state()
check(s.get("name") == "Charlie" and s.get("scene") == "1/3", "a program change and a scene together: the scene plays in the patch loaded", str(s))
configure(program_change=1)
s = program(b"\xb2\x45\x01")
check(s.get("scene") == "1/3", "a scene on another channel than program change is ignored", str(s))
configure(program_change="any")

# Clock out: restart, then no Start or Stop, then off.
def restart():
    count = int(open(run + "/restart").read()) if os.path.exists(run + "/restart") else 0
    with open(run + "/restart", "w") as f: f.write("%d\n" % (count + 1))
    since = frame[0]; time.sleep(0.8)
    return [s for f, s in seen if f >= since]
got = restart()
check(0xFC in got and 0xFA in got and got.index(0xFC) < got.index(0xFA) and got.count(0xF8) > 10, "Restart sends Stop, then Start, and the clock goes on",
      " ".join("%02x" % s for s in got[:6]))
configure(program_change="any", start_stop="off")
got = restart()
check(0xFC not in got and 0xFA not in got and got.count(0xF8) > 10, "with Start and Stop off, a restart sends neither")
configure(program_change="any", clock_out="off")
since = frame[0]; time.sleep(0.5)
check(not [s for f, s in seen if f >= since], "clock out off: nothing goes out")
since = frame[0]
configure(program_change="any")
check([s for f, s in seen if f >= since][:1] == [0xFA], "clock out on again: it begins with Start")

# Tempo from incoming clock: 125 BPM is a clock every 960 frames at 48 kHz.
configure(tempo_source="clock")
before = state()
clock["next"] = frame[0] + rate // 10; clock["interval"] = rate * 60 / (125 * 24)
time.sleep(2)
since = frame[0]; time.sleep(0.5)
s = state()
check(abs(float(s.get("tempo", 0)) - 125) < 0.15, "the patch tempo follows incoming clock at 125 BPM", "%s (was %s)" % (s.get("tempo"), before.get("tempo")))
check(s.get("dirty") == before.get("dirty") == "0", "following the clock does not mark the patch as edited", str(s))
check(not [x for f, x in seen if f >= since], "while following incoming clock, Peepo sends none of its own")
status = open(run + "/status").read()
check("clock_in=125.0" in status, "the interface is told the clock it hears", status.strip())
clock["interval"] = rate * 60 / (93.5 * 24)
time.sleep(2)
check(abs(float(state().get("tempo", 0)) - 93.5) < 0.15, "and follows a change to 93.5 BPM")
clock["interval"] = 0
time.sleep(1)
check("clock_in" not in open(run + "/status").read(), "a clock that stops is no longer reported")
configure(tempo_source="patch")

# MIDI learn, through the engine's control port.
learn = subprocess.Popen([build + "/peepo_ingen_live", engine, catalogue, "--midi-learn"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
client.connect("settingsprobe:send", "ingen:control")
lines = []
for line in learn.stdout:
    lines.append(line.rstrip())
    if line.startswith("LEARNING"): time.sleep(0.3); queue.append(b"\xb2\x07\x7f")
    if line.startswith("LEARNED"): time.sleep(0.3); queue.append(b"\xb2\x07\x00")
learn.wait()
print("".join("       | " + l + "\n" for l in lines if l.startswith(("PASS", "FAIL", "LEARN"))), end="")
check(learn.returncode == 0 and any("LEARNED CC 7 / 3" in l for l in lines), "controller 7 on channel 3 learned, sets the control, is saved, loaded and forgotten")

midi.terminate()
log, _ = midi.communicate(timeout=10)
print("".join("       | " + line + "\n" for line in log.splitlines()[-14:]), end="")
client.deactivate(); client.close()
print("SUMMARY %s" % ("%d failed" % failures if failures else "MIDI settings checks passed"))
sys.exit(1 if failures else 0)
