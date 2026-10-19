#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Rebuild only Detune's DSP with bounded FFT planning, from pinned source.
set -euo pipefail
REPO="$(readlink -f "$(dirname "$0")/..")"
CACHE=${1:?Give the source archive cache}
OUT=${2:?Give an output directory}
ARCHIVE="$CACHE/guitarix-detune-b337e6c3b7d54a605922af201804fb71a4b83f1d.tar.gz"
mkdir -p "$OUT/source"
tar -xzf "$ARCHIVE" -C "$OUT/source"
TREE="$OUT/source/trunk/src/LV2"
python3 - "$TREE/gx_detune.lv2/detune.cc" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]);text=p.read_text()
assert text.count('FFTW_MEASURE') == 2
p.write_text(text.replace('FFTW_MEASURE','FFTW_ESTIMATE'))
PY
flags=
[ "$(uname -m)" != aarch64 ] || flags=-mcpu=cortex-a76
g++ -shared -fPIC -O2 -std=c++11 -DNDEBUG -DLV2_SO $flags \
    -I"$TREE/faust" -I"$TREE/DSP" -I"$TREE/gx_detune.lv2" \
    "$TREE/gx_detune.lv2/gx_detune.cpp" "$TREE/DSP/gx_resampler.cc" \
    -o "$OUT/gx_detune.so" -lfftw3f -lzita-resampler -ldl
echo 'BUILT Detune with FFTW_ESTIMATE'
