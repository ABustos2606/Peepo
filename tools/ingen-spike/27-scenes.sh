#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
# Scenes, measured (P13.1): what each Delay, Reverb and Texture module does to
# its tail, a tone and its memory when bypassed today, then whether a bundle of
# changes lands in one frame, with the engine started plainly and with
# --atomic-bundles. "spill" repeats the tail checks with the engine's bypass
# spilling (engine patch 0013, P13.3); it is not part of "all". See
# 27-scenes.py. Run as root inside Debian:
#   bash 27-scenes.sh [tails|spill|atomic] [URI-SUBSTRING]
# Results: $RUN/scenes-tails.json, scenes-spill.json, scenes-atomic-plain.json,
# scenes-atomic-a.json.
# SCENES_LV2 names more bundle directories, searched first (such as our own
# plugins' build directories, with PEEPO_CATALOGUE a catalogue that lists them).
D="$(dirname "$(readlink -f "$0")")"
. "$D/env.sh"
[ -n "${SCENES_LV2:-}" ] && export LV2_PATH="$SCENES_LV2:$LV2_PATH"
CATALOGUE="${PEEPO_CATALOGUE:-$D/../../config/catalogue/wsl-debian.json}"
OVERLAY="${PEEPO_OVERLAY:-$D/../../config/catalogue/overlay.json}"
what="${1:-all}"
result=0
if [ "$what" = all ] || [ "$what" = tails ]; then
    echo "== tails, click, replay and side"
    start_engine || exit 1
    SCENES_RESULTS="$RUN/scenes-tails.json" python3 "$D/27-scenes.py" tails "$CATALOGUE" "$OVERLAY" "${2:-}" || result=1
    grep -E "rror" "$RUN/ingen.log" | sort | uniq -c | head -10
    stop_engine
fi
if [ "$what" = spill ]; then
    # Engine patch 0013: the same checks with the engine's bypass spilling.
    echo "== spill bypass: tails, click, replay, side and sleep"
    start_engine || exit 1
    SCENES_SPILL=1 SCENES_RESULTS="$RUN/scenes-spill.json" python3 "$D/27-scenes.py" spill "$CATALOGUE" "$OVERLAY" "${2:-}" || result=1
    grep -E "rror" "$RUN/ingen.log" | sort | uniq -c | head -10
    stop_engine
fi
if [ "$what" = all ] || [ "$what" = atomic ]; then
    for mode in plain a; do
        echo "== bundles, engine started $([ $mode = a ] && echo with --atomic-bundles || echo plainly)"
        if [ $mode = a ]; then PEEPO_ATOMIC=1 start_engine || exit 1; else PEEPO_ATOMIC=0 start_engine || exit 1; fi
        SCENES_RESULTS="$RUN/scenes-atomic-$mode.json" python3 "$D/27-scenes.py" atomic || result=1
        grep -E "rror" "$RUN/ingen.log" | sort | uniq -c | head -10
        stop_engine
    done
fi
exit $result
