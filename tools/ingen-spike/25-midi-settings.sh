#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
# Settings > MIDI against a live Ingen (P12.6-P12.8): program change, clock
# restart, Start and Stop, clock in and MIDI learn, with a JACK probe as the device.
# Run as root inside Debian:  bash 25-midi-settings.sh
D="$(dirname "$(readlink -f "$0")")"
. "$D/env.sh"
BUILD="${PEEPO_BUILD:-/tmp/peepo-adapter-build}"
CATALOGUE="${PEEPO_CATALOGUE:-$D/../../config/catalogue/wsl-debian.json}"
if [ -z "${PEEPO_PREBUILT:-}" ]; then
    cmake -S "$D/../.." -B "$BUILD" -G Ninja -DPEEPO_BUILD_UI=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null || exit 1
    cmake --build "$BUILD" 2>&1 | grep -E "error|warning|FAILED"
fi
[ -x "$BUILD/peepo-midi" ] && [ -x "$BUILD/peepo_midi_tests" ] || { echo "peepo-midi was not built (is libjack-jackd2-dev installed?)"; exit 1; }
"$BUILD/peepo_midi_tests" || exit 1
python3 -c "import jack" 2>/dev/null || apt-get install -y -qq python3-jack-client >/dev/null
start_engine || exit 1
python3 "$D/25-midi-settings.py" "$BUILD" "$CATALOGUE"; result=$?
stop_engine
exit $result
