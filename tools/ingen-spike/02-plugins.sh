#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
#
# Give the development engine real plugins to load: Poly's Mutable
# Instruments-derived modules, built at the commit pinned in
# reference/manifest.json, and Debian's packaged MDA effects as conventional
# ones. Run as root inside Debian, after 01-build.sh:
#   bash 02-plugins.sh
# Nothing under reference/ is touched; sources are cloned fresh. This is an
# x86-64 build for exercising the host and the catalogue; tools/pi/build-arm64.sh
# runs the same script in an arm64 root for the Pi.
set -euo pipefail

PREFIX=/opt/peepo
SRC=$PREFIX/src
LENTICULAR_COMMIT=141fdc89e250987bae421664132ddaedae735107

export DEBIAN_FRONTEND=noninteractive
# gettext-base only quietens git's submodule script.
dpkg -s mda-lv2 gettext-base >/dev/null 2>&1 || apt-get install -y -q mda-lv2 gettext-base >/dev/null

mkdir -p "$SRC"
cd "$SRC"
if [ ! -d lenticular_lv2 ]; then
    git clone -q https://github.com/peepo-sources/lenticular_lv2.git
fi
git -C lenticular_lv2 checkout -q "$LENTICULAR_COMMIT"
# Only the DSP libraries the build reads; the firmware bootloaders are not needed.
git -C lenticular_lv2 submodule update -q --init eurorack parasites/stmlib
git -C lenticular_lv2/eurorack submodule update -q --init stmlib
# Clouds' blend mixes the untouched input with Clouds' own sound, and an
# Output control sets the level of the mix (the patch explains both). The two
# files are reset first, so an earlier version of the patch never gets in the way.
CLOUDS_PATCH="$(dirname "$(readlink -f "$0")")/../plugin-patches/clouds-blend-and-output.patch"
if ! git -C lenticular_lv2 apply --reverse --check "$CLOUDS_PATCH" 2>/dev/null; then
    git -C lenticular_lv2 checkout -q -- src/polyclouds.cpp polyclouds.ttl
    git -C lenticular_lv2 apply "$CLOUDS_PATCH"
fi

# The makefile's rule for polyrings.ttl ends in a stray `""` command, which
# fails. Copying that one file first means make never runs the rule.
mkdir -p lenticular_lv2/build
cp lenticular_lv2/polyrings.ttl lenticular_lv2/build/polyrings.ttl
# The makefile's own flags target SSE, which is right on x86-64 only. On the
# Pi 5 the same optimizations are kept and the SSE ones replaced by tuning for
# its Cortex-A76 cores.
FLAGS=()
if [ "$(uname -m)" = aarch64 ]; then
    FLAGS=(OPTIMIZATIONS="-mcpu=cortex-a76 -ffast-math -fomit-frame-pointer -O3 -fno-finite-math-only -DNDEBUG -ftree-vectorize -funroll-loops -fdata-sections -g")
fi
make -C lenticular_lv2 -j"$(nproc)" PREFIX="$PREFIX" "${FLAGS[@]}" install > "$SRC/lenticular-build.log" 2>&1 || { grep -E "error|Error" "$SRC/lenticular-build.log" | head -20; exit 1; }

echo "--- installed"
ls "$PREFIX/lib/lv2/polylenticular.lv2"
git -C lenticular_lv2 log --oneline -1
for part in eurorack eurorack/stmlib parasites/stmlib; do echo "$part $(git -C "lenticular_lv2/$part" rev-parse HEAD)"; done
dpkg -s mda-lv2 | grep -E "^(Package|Version)"
