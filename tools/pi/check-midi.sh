#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
#
# MIDI devices on the installed instrument (P12.2), without one plugged in:
# the kernel's virtual MIDI card (snd-virmidi) stands in for a USB device.
# Loading it is plugging it in, unloading it is unplugging it. Checks that
# a2jmidid makes it JACK MIDI ports, peepo-route wires it both ways, the clock
# reaches it, and it comes back after a replug. The patch is not touched.
# Run as root on the Pi, with peepo.target running:  bash check-midi.sh
# With --device NAME it instead watches a real device whose JACK port names
# contain NAME: unplug and plug it when asked.
set -uo pipefail
failures=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; failures=$((failures + 1)); }
as_peepo() { runuser -u peepo -- "$@"; }
wait_for() { for _ in $(seq "$1"); do eval "$2" && return 0; sleep 0.5; done; return 1; }
xruns() { journalctl -u peepo-jack -o cat --since "$START" 2>/dev/null | grep -ci xrun; }
ports() { as_peepo jack_lsp 2>/dev/null | grep -F "$NAME"; }
capture() { as_peepo jack_lsp -p 2>/dev/null | awk -v n="$NAME" '/^[^\t]/ {p=$0} /properties: output/ && index(p, n) {print p}' | head -1; }
playback() { as_peepo jack_lsp -p 2>/dev/null | awk -v n="$NAME" '/^[^\t]/ {p=$0} /properties: input/ && index(p, n) {print p}' | head -1; }
wired() { as_peepo jack_lsp -c "$1" 2>/dev/null | grep -qxF "   $2"; }
START=$(date '+%Y-%m-%d %H:%M:%S')

for unit in peepo-jack peepo-engine peepo-route peepo-a2j peepo-midi; do
    systemctl is-active --quiet "$unit" && pass "$unit is running" || fail "$unit is not running"
done
wait_for 20 'as_peepo jack_lsp | grep -qxF ingen:midi_in' && pass "the engine has its MIDI endpoints" || fail "ingen:midi_in is missing"

REAL=0; NAME="VirMIDI"
if [ "${1:-}" = "--device" ] && [ -n "${2:-}" ]; then REAL=1; NAME=$2
else
    # The card cannot be unloaded while a2jmidid has it open: start from none.
    systemctl stop peepo-a2j; modprobe -r snd-virmidi 2>/dev/null; systemctl start peepo-a2j
    lsmod | grep -q snd_virmidi && { fail "snd-virmidi is still loaded"; exit 1; }
fi
plug_in() {
    local began=$(date +%s.%N)
    if [ $REAL = 1 ]; then echo "      plug $NAME in now"; else modprobe snd-virmidi midi_devs=1; fi
    wait_for 40 '[ -n "$(capture)" ] && [ -n "$(playback)" ]' || { fail "$NAME did not appear as JACK MIDI ports"; return 1; }
    local in out; in=$(capture); out=$(playback)
    if wait_for 20 'wired "$in" ingen:midi_in && wired ingen:midi_out "$out" && wired peepo-midi:clock_out "$out"'; then
        pass "$1: wired both ways in $(awk -v a="$began" -v b="$(date +%s.%N)" 'BEGIN { printf "%.1f", b - a }') s -- $in -> ingen:midi_in; ingen:midi_out, peepo-midi:clock_out -> $out"
    else fail "$1: not wired -- $(as_peepo jack_lsp -c "$in" | tr '\n' ' ')"; fi
}
plug_in "first plugged in"
if [ $REAL = 0 ]; then
    card=$(awk '/VirMIDI/ {print $1; exit}' /proc/asound/cards)
    # What the device plays reaches JACK, on the port wired to the engine.
    in=$(capture)
    as_peepo sh -c "jack_midi_dump checkprobe > /tmp/check-midi.dump 2>&1 & echo \$! > /tmp/check-midi.pid; sleep 1; jack_connect '$in' checkprobe:input"
    amidi -p "hw:$card,0" -S "90 3C 64 80 3C 00"; sleep 0.5
    as_peepo sh -c 'kill -INT $(cat /tmp/check-midi.pid)'; sleep 0.3
    grep -q "90 3c 64" /tmp/check-midi.dump && grep -q "80 3c 00" /tmp/check-midi.dump && pass "a note played on the device arrives in JACK on its port to ingen:midi_in" ||
        fail "the device's note did not arrive -- $(tr '\n' ' ' < /tmp/check-midi.dump)"
    # The clock through a2jmidid out to ALSA and back. The virtual card does
    # not hand sequencer events to a raw reader, so ALSA's Midi Through, a
    # kernel port like a device's, loops it back instead.
    thru_in=$(as_peepo jack_lsp | grep -F "Midi Through" | grep -F playback | head -1)
    thru_out=$(as_peepo jack_lsp | grep -F "Midi Through" | grep -F capture | head -1)
    as_peepo sh -c "jack_midi_dump checkprobe > /tmp/check-midi.dump 2>&1 & echo \$! > /tmp/check-midi.pid; sleep 1; jack_connect '$thru_out' checkprobe:input; jack_connect peepo-midi:clock_out '$thru_in'; sleep 1; jack_disconnect peepo-midi:clock_out '$thru_in'; kill -INT \$(cat /tmp/check-midi.pid)"
    sleep 0.3
    clocks=$(grep -c " f8" /tmp/check-midi.dump)
    [ "$clocks" -ge 10 ] && pass "the clock goes out through ALSA, as to a device -- $clocks clock messages in 1 s" || fail "no clock through ALSA ($clocks messages)"
    rm -f /tmp/check-midi.dump /tmp/check-midi.pid
fi
# Unplug, then plug again: the ports go, come back and are wired again. The
# virtual card can only go while a2jmidid is stopped, so it stands in for a
# device unplugged while a2jmidid restarts; a real device is pulled live.
if [ $REAL = 1 ]; then echo "      unplug $NAME now"; else systemctl stop peepo-a2j; modprobe -r snd-virmidi; systemctl start peepo-a2j; fi
wait_for 60 '[ -z "$(ports)" ]' && pass "unplugged: its ports are gone" || fail "its ports are still there"
plug_in "plugged in again"
if [ $REAL = 0 ]; then systemctl stop peepo-a2j; modprobe -r snd-virmidi; systemctl start peepo-a2j; fi
x=$(xruns)
late=$(journalctl -u peepo-jack -o cat --since "$START" 2>/dev/null | grep -o 'client = [^ ]* was not finished' | sort | uniq -c | tr -s ' ' | paste -sd, -)
[ "$x" = 0 ] && pass "no xruns on the audio interface during the check" || fail "$x xruns during the check -- late: ${late:-?}; $(vcgencmd get_throttled 2>/dev/null) (0x50005: undervoltage, throttled)"
journalctl -u peepo-route -o cat --since "$START" | grep -F "$NAME" | sed 's/^/      | /' | tail -6
[ $failures = 0 ] && echo "SUMMARY MIDI device checks passed" || echo "SUMMARY $failures failed"
exit $((failures > 0))
