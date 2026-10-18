#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
# Check just GxPlugins on a separate JACK dummy server and Ingen, including
# every control's range, bypass readback, audio and a saved/restored control.
# Usage: bash 21-gxplugins-check.sh RESULT-DIRECTORY
# GX_PLUGIN_PATH can point at staged bundles before installation.
# GX_EXPORTER and GX_LIVE can point at development builds of the test tools.
set -euo pipefail
HERE="$(dirname "$(readlink -f "$0")")"
REPO="$(readlink -f "$HERE/../..")"
PREFIX=${PREFIX:-/opt/peepo}
OUT="$(readlink -m "${1:?Give a result directory}")"
mkdir -p "$OUT"
RUN=$(mktemp -d /tmp/peepo-gx-check.XXXXXX)
export JACK_DEFAULT_SERVER="peepo-gx-$$" JACK_NO_AUDIO_RESERVATION=1
export PEEPO_INGEN_SOCKET="$RUN/ingen.sock"
export PATH="$PREFIX/bin:$PATH"
export LD_LIBRARY_PATH="$(find "$PREFIX/lib" -maxdepth 2 -name 'libingen*' -printf '%h\n' | sort -u | paste -sd:)"
export LV2_PATH="${GX_PLUGIN_PATH:-$PREFIX/lib/lv2}"
exporter=${GX_EXPORTER:-$PREFIX/bin/peepo_catalogue_export}
live=${GX_LIVE:-$PREFIX/libexec/peepo/peepo_catalogue_live}
python3 -c 'import jack, lilv, numpy'
python3 - "$REPO/config/catalogue/gxplugins.json" "$OUT/inventory.txt" <<'PY'
import json, pathlib, sys
pathlib.Path(sys.argv[2]).write_text(''.join(p['uri']+'\n' for p in json.load(open(sys.argv[1]))['plugins']))
PY
"$exporter" --only "$OUT/inventory.txt" -o "$OUT/catalogue.json"
jack_pid= engine_pid=
cleanup() {
    for pid in $engine_pid $jack_pid; do kill "$pid" 2>/dev/null || true; done
    for _ in $(seq 30); do
        alive=0
        for pid in $engine_pid $jack_pid; do kill -0 "$pid" 2>/dev/null && alive=1; done
        [ "$alive" = 1 ] || break
        sleep 0.1
    done
    for pid in $engine_pid $jack_pid; do kill -9 "$pid" 2>/dev/null || true; done
}
trap cleanup EXIT
jackd -n "$JACK_DEFAULT_SERVER" --no-realtime -d dummy -r 48000 -p 128 >"$OUT/jack.log" 2>&1 &
jack_pid=$!
for _ in $(seq 100); do jack_lsp >/dev/null 2>&1 && break; sleep 0.1; done
ingen -e -f -E 0 -s "$JACK_DEFAULT_SERVER" -S "$PEEPO_INGEN_SOCKET" >"$OUT/ingen.log" 2>&1 &
engine_pid=$!
export PEEPO_ENGINE_PID=$engine_pid
for _ in $(seq 100); do [ -S "$PEEPO_INGEN_SOCKET" ] && break; sleep 0.1; done
[ -S "$PEEPO_INGEN_SOCKET" ] || { tail -20 "$OUT/ingen.log"; exit 1; }
"$live" "unix://$PEEPO_INGEN_SOCKET" "$OUT/catalogue.json" "$REPO/config/catalogue/overlay.json" >"$OUT/live.txt"
tail -1 "$OUT/live.txt"
python3 "$HERE/21-gxplugins-check.py" "$OUT/catalogue.json" "$OUT/live.txt" "$OUT/results.json" "$RUN/bundles" | tee "$OUT/signal.txt"
