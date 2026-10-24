#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
# Euclidelay follows the patch tempo, measured in a fresh engine: the tempo set
# through Peepo's adapter reaches its bpm control (the overlay's "tempo") and
# its echoes move with it, also in a patch saved at another tempo and loaded.
# Run as root inside Debian, after 14-more-plugins.sh (which builds it):
#   bash 26-euclidelay.sh
# EUCLIDELAY_LV2 names another directory holding euclidelay.lv2, searched first.
D="$(dirname "$(readlink -f "$0")")"
. "$D/env.sh"
[ -n "${EUCLIDELAY_LV2:-}" ] && export LV2_PATH="$EUCLIDELAY_LV2:$LV2_PATH"
BUILD="${PEEPO_BUILD:-/tmp/peepo-adapter-build}"
CATALOGUE="${PEEPO_CATALOGUE:-$D/../../config/catalogue/wsl-debian.json}"
export PEEPO_OVERLAY="${PEEPO_OVERLAY:-$D/../../config/catalogue/overlay.json}"
if [ -z "${PEEPO_PREBUILT:-}" ]; then
    cmake -S "$D/../.." -B "$BUILD" -G Ninja -DPEEPO_BUILD_UI=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null || exit 1
    cmake --build "$BUILD" 2>&1 | grep -E "error|warning|FAILED"
fi
[ -x "$BUILD/peepo_ingen_live" ] || { echo "peepo_ingen_live was not built"; exit 1; }
start_engine || exit 1
python3 "$D/26-euclidelay.py" "$BUILD" "$CATALOGUE"; result=$?
grep -E "rror" "$RUN/ingen.log" | sort | uniq -c | head -10
stop_engine
exit $result
