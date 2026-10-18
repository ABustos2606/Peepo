#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
# Build the full, pinned GxPlugins collection without desktop interfaces.
# Run on Debian x86-64 or the Pi (build-essential, git, pkg-config, lv2-dev,
# python3 required). Can also run alone instead of rebuilding the catalogue.
set -euo pipefail
REPO="$(readlink -f "$(dirname "$(readlink -f "$0")")/../..")"
PREFIX=${PREFIX:-/opt/peepo}
SRC=${GX_SOURCE_ROOT:-$PREFIX/src/GxPlugins.lv2}
LV2DIR=${GX_INSTALL_DIR:-$PREFIX/lib/lv2}
MANIFEST="$REPO/config/catalogue/gxplugins.json"
SOURCES="$PREFIX/share/peepo/sources/gxplugins"
NOTICES="$PREFIX/share/peepo/licenses/gxplugins"
mkdir -p "$SRC" "$LV2DIR" "$SOURCES" "$NOTICES"
case $(uname -m) in
    aarch64) cpu_flags=-mcpu=cortex-a76 ;;
    x86_64) cpu_flags='-msse2 -mfpmath=sse' ;;
    *) cpu_flags= ;;
esac
specs=$(python3 - "$MANIFEST" <<'PY'
import json, re, sys
entries = json.load(open(sys.argv[1]))['plugins']
assert len(entries) == 43 and len({p['uri'] for p in entries}) == 43
for p in entries:
    assert re.fullmatch(r'Gx[A-Za-z0-9]+\.lv2', p['repo'])
    assert re.fullmatch(r'[0-9a-f]{40}', p['commit'])
    assert p['url'].startswith('https://github.com/brummer10/')
    print(p['repo'], p['url'], p['commit'], sep='\t')
PY
)
# A shipped source set can be rebuilt offline; its archives were created
# directly from the pinned commits and include every source file/notice.
archives=0
if [ -f "$REPO/SHA256SUMS" ]; then
    (cd "$REPO" && sha256sum --check --status SHA256SUMS)
    archives=1
fi
while IFS=$'\t' read -r name url revision; do
    dir="$SRC/$name"
    echo "Building $name at $revision"
    if [ "$archives" = 1 ]; then
        tar -xzf "$REPO/$name.tar.gz" -C "$SRC"
    else
        if [ ! -e "$dir/.git" ]; then git clone -q "$url" "$dir"; fi
        git -C "$dir" cat-file -e "$revision^{commit}" 2>/dev/null || git -C "$dir" fetch -q origin "$revision"
        git -C "$dir" checkout -q --detach "$revision"
    fi
    # Explicit flags: under qemu /proc/cpuinfo describes the x86 host.
    # One make job: several upstream targets clean shared output directories.
    (cd "$dir" && make -j1 mod SSE_CFLAGS="$cpu_flags" && make -j1 install INSTALL_DIR="$LV2DIR") >"$SRC/$name.log" 2>&1 || {
        tail -25 "$SRC/$name.log" >&2; exit 1;
    }
    # Exact corresponding source, including all file-specific notices. There
    # are no source edits; only the make variable above changes the flags.
    if [ "$archives" = 1 ]; then
        [ "$REPO" = "$SOURCES" ] || cp "$REPO/$name.tar.gz" "$SOURCES/"
    else
        git -C "$dir" archive --format=tar --prefix="$name/" "$revision" | gzip -n >"$SOURCES/$name.tar.gz"
    fi
    mkdir -p "$NOTICES/$name"
    find "$dir" -maxdepth 1 -type f \( -iname 'license*' -o -iname 'copying*' -o -iname 'readme*' \) -exec cp -t "$NOTICES/$name" {} +
done <<< "$specs"
cp "$MANIFEST" "$SOURCES/gxplugins.json"
# Keep the original relative paths so the supplied build script can be run
# after extracting the source archives into GX_SOURCE_ROOT.
mkdir -p "$SOURCES/config/catalogue" "$SOURCES/tools/ingen-spike"
cp "$MANIFEST" "$SOURCES/config/catalogue/"
cp "$0" "$SOURCES/tools/ingen-spike/21-gxplugins.sh"
(cd "$SOURCES" && sha256sum ./*.tar.gz > SHA256SUMS)
echo 'PASS all 43 GxPlugins built; sources and notices packaged'
