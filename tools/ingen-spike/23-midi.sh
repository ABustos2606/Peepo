#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
# MIDI cables against a live Ingen (P12.4): the adapter creates the engine's
# MIDI endpoints, cables MIDI IN -> x42 MIDI Transpose (+12) -> MIDI OUT, saves
# and loads the patch, and a JACK MIDI client then plays a note through it.
# Needs x42-midifilter. Run as root inside Debian:  bash 23-midi.sh
D="$(dirname "$(readlink -f "$0")")"
. "$D/env.sh"
BUILD="${PEEPO_BUILD:-/tmp/peepo-adapter-build}"
CATALOGUE="${PEEPO_CATALOGUE:-$D/../../config/catalogue/wsl-debian.json}"
if [ -z "${PEEPO_PREBUILT:-}" ]; then
    cmake -S "$D/../.." -B "$BUILD" -G Ninja -DPEEPO_BUILD_UI=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null || exit 1
    cmake --build "$BUILD" 2>&1 | grep -E "error|warning|FAILED"
fi
[ -x "$BUILD/peepo_ingen_live" ] || exit 1
python3 -c "import jack" 2>/dev/null || apt-get install -y -qq python3-jack-client >/dev/null
start_engine || exit 1
"$BUILD/peepo_ingen_live" unix:///tmp/peepo-ingen.sock "$CATALOGUE" --midi; live=$?
python3 - <<'PY'
import time
import jack
client = jack.Client("midiprobe")
notes = client.midi_outports.register("notes")
seen_port = client.midi_inports.register("seen")
sent, seen = [], []
def process(frames):
    notes.clear_buffer()
    if sent and not sent[0][1]:
        notes.write_midi_event(0, sent[0][0]); sent[0][1] = True
    for offset, data in seen_port.incoming_midi_events():
        seen.append(bytes(data))
client.set_process_callback(process)
client.activate()
kinds = {p.name: ("midi" if p.is_midi else "audio") for p in client.get_ports("ingen:")}
types_ok = kinds.get("ingen:midi_in") == "midi" and kinds.get("ingen:midi_out") == "midi"
print("%s  the engine's MIDI endpoints are JACK MIDI ports -- %s" % ("PASS" if types_ok else "FAIL", ", ".join("%s (%s)" % kv for kv in sorted(kinds.items()))))
client.connect("midiprobe:notes", "ingen:midi_in")
client.connect("ingen:midi_out", "midiprobe:seen")
time.sleep(0.5)
ok = types_ok
for message, expected in ((b"\x90\x3c\x64", b"\x90\x48\x64"), (b"\x80\x3c\x00", b"\x80\x48\x00")):
    seen.clear(); sent[:] = [[message, False]]
    deadline = time.time() + 3
    while expected not in seen and time.time() < deadline: time.sleep(0.01)
    got = expected in seen
    ok = ok and got
    print("%s  %s into MIDI IN comes out of MIDI OUT as %s -- saw %s" % ("PASS" if got else "FAIL", message.hex(" "), expected.hex(" "), [s.hex(" ") for s in seen] or "nothing"))
client.deactivate(); client.close()
raise SystemExit(0 if ok else 1)
PY
heard=$?
grep -E "rror|MIDI" "$RUN/ingen.log" | sort | uniq -c | head -10
stop_engine
[ "$live" = 0 ] && [ "$heard" = 0 ]
