#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
# Our own plugins' tails (P13.5): builds Aura, Eureka, Euclidelay, Drizzle,
# Loo-pi and Discipline at the commits 14-more-plugins.sh pins, runs
# 27-scenes.sh tails on them (the click switching off, then off and on again),
# and checks that Trails in a patch saved by Discipline 0.4.0 stays 0 when
# loaded under the pinned Discipline while a new block gets the default.
# Run as root inside Debian:
#   bash 28-own-tails.sh
# OWN_PLUGINS (default /mnt/c/LV2-own-plugins) holds the repositories.
# DISCIPLINE_TREE=1 builds Discipline from its working tree instead of its pin.
# Results: $RUN/own-tails/scenes-tails-off.json, scenes-tails-on-again.json.
D="$(dirname "$(readlink -f "$0")")"
. "$D/env.sh"
OWN="${OWN_PLUGINS:-/mnt/c/LV2-own-plugins}"
W="$RUN/own-tails"
# The last Discipline with Trails defaulting to 0.
OLD_DISCIPLINE=4f68f3b967f8fb92c4968554b44c131adfda3717
CATALOGUE="${PEEPO_CATALOGUE:-$D/../../docs/evidence/catalogue-pi-20261023-euclidelay.json}"
rm -rf "$W"; mkdir -p "$W/src" "$W/lv2" "$W/old-lv2"
BASE="$LV2_PATH"

pinned() { awk -v n="$1" '$1 == "third" && $2 == n { print $4 }' "$D/14-more-plugins.sh"; }
build() { # name commit-or-"tree" destination
    local src="$W/src/$1-$2"
    if [ "$2" = tree ]; then src="$OWN/$1"; else
        mkdir -p "$src"
        git -c safe.directory='*' -C "$OWN/$1" archive "$2" | tar -x -C "$src" || return 1
    fi
    make -C "$src" -j"$(nproc)" OPT=-O2 LV2DIR="$3" install > "$W/$1-$2.log" 2>&1 || { tail "$W/$1-$2.log"; return 1; }
    echo "built $1 at $2"
}
for name in aura eureka euclidelay drizzle loo-pi discipline; do
    commit="$(pinned $name)"
    [ "$name" = discipline ] && [ "${DISCIPLINE_TREE:-0}" = 1 ] && commit=tree
    [ -n "$commit" ] || { echo "no pin for $name in 14-more-plugins.sh"; exit 1; }
    build $name "$commit" "$W/lv2" || exit 1
done
build discipline $OLD_DISCIPLINE "$W/old-lv2" || exit 1

result=0
for again in 0 1; do
    echo "== tails, click switching off$([ $again = 1 ] && echo " and on again")"
    SCENES_ON_AGAIN=$again SCENES_LV2="$W/lv2" PEEPO_CATALOGUE="$CATALOGUE" bash "$D/27-scenes.sh" tails ABustos2606 || result=1
    cp "$RUN/scenes-tails.json" "$W/scenes-tails-$([ $again = 1 ] && echo on-again || echo off).json"
    sleep 3
done

echo "== Trails in a patch saved by Discipline 0.4.0"
B="$W/old-patch.ingen"
LV2_PATH="$W/old-lv2:$BASE" start_engine || exit 1
python3 "$D/28-own-tails.py" save "$B" || result=1
stop_engine; sleep 3
LV2_PATH="$W/lv2:$BASE" start_engine || exit 1
python3 "$D/28-own-tails.py" load "$B" || result=1
stop_engine
exit $result
