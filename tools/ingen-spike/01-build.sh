#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
#
# Build Poly's Ingen fork at the pinned reference commit inside a Debian
# environment (WSL2 or a Pi), in an isolated prefix. Run as root:
#   bash 01-build.sh
# Nothing under reference/ is modified; sources are cloned fresh.
set -euo pipefail

# Resolved before any cd, so the script also works when called by a relative path.
HERE="$(dirname "$(readlink -f "$0")")"
PREFIX=/opt/peepo
SRC=$PREFIX/src
# Poly's main (8dae7404) is upstream Ingen 13a045d0 plus three commits. Two of
# them switch the client socket to JSON through SERD_JSONTURTLE, which exists in
# no published serd (Poly's public serd branches stop at SERD_HEXTUPLES), so
# main does not compile. Our client is new, so build the last upstream commit,
# which speaks Turtle with stock serd, plus Poly's one-line MIDI-learn fix.
INGEN_BASE=13a045d0
INGEN_PICK=8dae740425f0f1468198e0cdf1e1c61b3fc6ec5e
RAUL_TAG=v2.0.0

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq --no-install-recommends \
    build-essential meson ninja-build pkg-config git ca-certificates python3 python3-rdflib \
    libboost-dev libsigc++-2.0-dev libjack-jackd2-dev jackd2 \
    lv2-dev libserd-dev libsord-dev libsratom-dev liblilv-dev libsuil-dev \
    lilv-utils lv2-examples socat procps
# jack_lsp and jack_connect: their own package since Debian 13, part of jackd2 before.
if apt-cache show jack-example-tools >/dev/null 2>&1; then apt-get install -y -qq --no-install-recommends jack-example-tools; fi

mkdir -p "$SRC"
cd "$SRC"

# Raul is header-only and not packaged in current Debian.
if [ ! -d raul ]; then git clone -q --depth 1 --branch "$RAUL_TAG" https://gitlab.com/drobilla/raul.git; fi
if [ ! -f "$PREFIX/lib/pkgconfig/raul-2.pc" ] && [ ! -f "$PREFIX/lib/x86_64-linux-gnu/pkgconfig/raul-2.pc" ] && [ ! -f "$PREFIX/lib/aarch64-linux-gnu/pkgconfig/raul-2.pc" ]; then
    meson setup raul/build raul --prefix "$PREFIX" -Dtests=disabled >/dev/null
    meson install -C raul/build >/dev/null
fi

# Configure a meson project once; a configure that failed part-way is redone.
configure() {
    local project=$1; shift
    [ -f "$project/build/build.ninja" ] && return 0
    rm -rf "$project/build"
    meson setup "$project/build" "$project" --prefix "$PREFIX" --buildtype release "$@" >/dev/null
}

export PKG_CONFIG_PATH="$(find "$PREFIX" -name pkgconfig -type d | paste -sd:)"
# This Ingen needs lilv 0.24.21 or later. Debian 12 (and so Raspberry Pi OS
# bookworm) has 0.24.14: there, build lilv and the zix it needs into the prefix.
LILV_TAG=v0.24.24
ZIX_TAG=v0.4.2
if ! pkg-config --atleast-version=0.24.21 lilv-0; then
    [ -d zix ] || git clone -q --depth 1 --branch "$ZIX_TAG" https://gitlab.com/drobilla/zix.git
    configure zix -Dtests=disabled -Dbenchmarks=disabled -Ddocs=disabled
    meson install -C zix/build >/dev/null
    export PKG_CONFIG_PATH="$(find "$PREFIX" -name pkgconfig -type d | paste -sd:)"
    [ -d lilv ] || git clone -q --depth 1 --branch "$LILV_TAG" https://gitlab.com/lv2/lilv.git
    configure lilv -Dtests=disabled -Ddocs=disabled -Dbindings_py=disabled -Ddefault_lv2_path="$PREFIX/lib/lv2:/usr/lib/lv2:/usr/local/lib/lv2"
    meson install -C lilv/build >/dev/null
    export PKG_CONFIG_PATH="$(find "$PREFIX" -name pkgconfig -type d | paste -sd:)"
    echo "built lilv $(pkg-config --modversion lilv-0) into $PREFIX"
fi

if [ ! -d ingen ]; then
    git clone -q https://github.com/peepo-sources/ingen.git
fi
if ! git -C ingen rev-parse -q --verify peepo-spike >/dev/null; then
    git -C ingen checkout -q -b peepo-spike "$INGEN_BASE"
    git -C ingen -c user.name=peepo -c user.email=peepo@localhost cherry-pick "$INGEN_PICK"
fi

# Our small changes to the engine (AGPL-3.0-or-later, like the files they touch).
for patch in "$HERE"/patches/*.patch; do
    git -C ingen apply --check "$patch" 2>/dev/null && git -C ingen apply "$patch" && echo "applied $(basename "$patch")"
done

export PKG_CONFIG_PATH="$(find "$PREFIX" -name pkgconfig -type d | paste -sd:)"
configure ingen -Dgui=disabled -Ddocs=disabled -Dbindings_py=disabled -Dportaudio=disabled -Djack=enabled -Dlint=false
meson compile -C ingen/build
meson install -C ingen/build >/dev/null

echo "--- versions"
for p in serd-0 sord-0 sratom-0 suil-0 lv2 lilv-0 raul-2 jack; do printf '%s %s\n' "$p" "$(pkg-config --modversion "$p")"; done
git -C ingen log --oneline -2
ls "$PREFIX/bin"
