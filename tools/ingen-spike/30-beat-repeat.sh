#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
# Mist Beat Repeat after Peepo's Clouds patch: pass-through level, repeats on
# the patch-tempo clock and on Trigger in, Chance, Output, and the other Mist
# modes still running. No engine; the plugin binary is loaded directly. Run as
# root inside Debian after 02-plugins.sh, or on the Pi:
#   bash 30-beat-repeat.sh [path to polyclouds.so]
set -euo pipefail
D="$(dirname "$(readlink -f "$0")")"
SO="${1:-/opt/peepo/lib/lv2/polylenticular.lv2/polyclouds.so}"
BIN="$(mktemp -d)/30-beat-repeat"
INC=()
[ -d /opt/peepo/include ] && INC=(-I/opt/peepo/include)
g++ -O2 -std=c++17 "${INC[@]}" "$D/30-beat-repeat.cpp" -o "$BIN" -ldl
echo "polyclouds.so: $SO"
"$BIN" "$SO"
