#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
# What CV range Poly's CV modules respond to, and Sway driving them, measured
# in a fresh engine. Run as root inside Debian, with PolyLV2's *_ext modules
# and Sway installed in the engine's LV2 path:
#   bash 29-sway-cv.sh [flanger chorus vibrato tremolo]
D="$(dirname "$(readlink -f "$0")")"
. "$D/env.sh"
start_engine || exit 1
python3 "$D/29-sway-cv.py" "$@"; result=$?
grep -E "rror" "$RUN/ingen.log" | sort | uniq -c | head -10
stop_engine
exit $result
