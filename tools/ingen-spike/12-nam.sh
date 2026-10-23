#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
#
# Build the two NAM candidates of P8.1 for the development engine:
#   current  mikeoliphant/neural-amp-modeler-lv2 on NeuralAudio
#   poly     the Poly fork pinned in reference/manifest.json, on NAM Core
# Both declare the same plugin URI, so each goes into its own directory and
# the engine is started with one of them on LV2_PATH at a time:
#   LV2_PATH=$LV2_PATH:/opt/peepo/nam/current
# Run as root inside Debian (or the arm64 root), after 01-build.sh:
#   bash 12-nam.sh
# Nothing under reference/ is touched; sources are cloned fresh.
set -euo pipefail

PREFIX=/opt/peepo
SRC=$PREFIX/src
CURRENT_COMMIT=96924205f9cbfbbf0e63f7fe7f4b6d2f1b60c402
POLY_COMMIT=a3a6520ea519513a6ca1e82d350d47e3c7891732
JOBS=$(nproc)

export DEBIAN_FRONTEND=noninteractive
# gettext-base only quietens git's submodule script.
dpkg -s cmake g++ git gettext-base >/dev/null 2>&1 || apt-get install -y -q cmake g++ git gettext-base >/dev/null

# The Pi 5's cores; on x86-64 the projects' own defaults are kept.
ARCH_FLAGS=()
if [ "$(uname -m)" = aarch64 ]; then ARCH_FLAGS=(-DCMAKE_CXX_FLAGS="-mcpu=cortex-a76"); fi

fetch() { # url dir commit
    [ -d "$SRC/$2" ] || git clone -q "$1" "$SRC/$2"
    git -C "$SRC/$2" fetch -q origin "$3" 2>/dev/null || true
    git -C "$SRC/$2" checkout -q "$3"
    git -C "$SRC/$2" submodule update -q --init --recursive
}

build() { # dir install-dir
    cmake -S "$SRC/$1" -B "$SRC/$1/build-peepo" -DCMAKE_BUILD_TYPE=Release "${ARCH_FLAGS[@]}" >"$SRC/$1-cmake.log" 2>&1 \
        || { tail -30 "$SRC/$1-cmake.log"; exit 1; }
    cmake --build "$SRC/$1/build-peepo" -j"$JOBS" >"$SRC/$1-build.log" 2>&1 \
        || { grep -E "error|Error" "$SRC/$1-build.log" | head -30; exit 1; }
    rm -rf "$2"; mkdir -p "$2"
    # Both projects leave a finished bundle in their build tree.
    bundle=$(find "$SRC/$1/build-peepo" -name manifest.ttl -path '*neural_amp_modeler.lv2*' -printf '%h\n' | head -1)
    [ -n "$bundle" ] || { echo "no bundle produced for $1"; exit 1; }
    cp -r "$bundle" "$2/"
}

fetch https://github.com/peepo-sources/neural-amp-modeler-lv2.git nam-current "$CURRENT_COMMIT"
build nam-current "$PREFIX/nam/current"
fetch https://github.com/peepo-sources/neural-amp-modeler-lv2-poly.git nam-poly "$POLY_COMMIT"
build nam-poly "$PREFIX/nam/poly"

echo "--- built"
for c in current poly; do
    echo "$c: $(ls "$PREFIX/nam/$c"/*/)"
    git -C "$SRC/nam-$c" log --oneline -1
    git -C "$SRC/nam-$c" submodule status --recursive | sed 's/^/    /'
done
grep -m1 "CXX compiler identification" "$SRC/nam-current-cmake.log" || true
