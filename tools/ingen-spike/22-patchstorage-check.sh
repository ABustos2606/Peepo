#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Isolated checks for the explicitly selected Patchstorage expansion.
set -euo pipefail
HERE="$(dirname "$(readlink -f "$0")")"
REPO="$(readlink -f "$HERE/../..")"
PREFIX=${PREFIX:-/opt/peepo}
OUT="$(readlink -m "${1:?Give a result directory}")"
mkdir -p "$OUT"
RUN=$(mktemp -d /tmp/peepo-expansion-check.XXXXXX)
export JACK_DEFAULT_SERVER="peepo-expansion-$$" JACK_NO_AUDIO_RESERVATION=1
export PEEPO_INGEN_SOCKET="$RUN/ingen.sock"
export PATH="$PREFIX/bin:$PATH"
export LD_LIBRARY_PATH="$(find "$PREFIX/lib" -maxdepth 2 -name 'libingen*' -printf '%h\n' | sort -u | paste -sd:)${PLUGIN_TEST_LIBRARY_PATH:+:$PLUGIN_TEST_LIBRARY_PATH}"
export LV2_PATH="${PLUGIN_TEST_PATH:?Give the staged LV2 directory}"
export PLUGIN_TEST_ENGINE_PREFIX="$PREFIX"
export INGEN_MODULE_PATH="$(find "$PREFIX/lib" -maxdepth 2 -name 'libingen_server.so' -printf '%h\n')"
exporter=${PLUGIN_TEST_EXPORTER:-$PREFIX/bin/peepo_catalogue_export}
live=${PLUGIN_TEST_LIVE:-$PREFIX/libexec/peepo/peepo_catalogue_live}
python3 - "$REPO/config/catalogue/patchstorage.json" "$OUT/inventory.txt" <<'PY'
import json,os,pathlib,platform,sys
target={'aarch64':'rpi-aarch64','x86_64':'linux-amd64'}[platform.machine()]
entries=json.load(open(sys.argv[1]))['entries']
uris={uri for e in entries if target in e['archives'] or e.get('source_build') or target in e.get('package_targets',{}) for uri in e['advertised_uris']}
if os.environ.get('PLUGIN_TEST_ONLY'):
    chosen=set(os.environ['PLUGIN_TEST_ONLY'].split(','))
    assert chosen <= uris
    uris=chosen
pathlib.Path(sys.argv[2]).write_text('\n'.join(sorted(uris))+'\n')
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
jackd -n "$JACK_DEFAULT_SERVER" --no-realtime -d dummy -r 48000 -p 128 >"$OUT/jack.log" 2>&1 & jack_pid=$!
for _ in $(seq 100); do jack_lsp >/dev/null 2>&1 && break; sleep 0.1; done
if [ "${PLUGIN_TEST_GDB:-0}" = 1 ]; then
    gdb -q -batch -ex run -ex 'thread apply all bt' --args ingen -e -f -E 0 -s "$JACK_DEFAULT_SERVER" -S "$PEEPO_INGEN_SOCKET" >"$OUT/ingen.log" 2>&1 & engine_pid=$!
else
    ingen -e -f -E 0 -s "$JACK_DEFAULT_SERVER" -S "$PEEPO_INGEN_SOCKET" >"$OUT/ingen.log" 2>&1 & engine_pid=$!
fi
for _ in $(seq 100); do [ -S "$PEEPO_INGEN_SOCKET" ] && break; sleep 0.1; done
[ -S "$PEEPO_INGEN_SOCKET" ] || { tail -20 "$OUT/ingen.log"; exit 1; }
timeout 300 "$live" "unix://$PEEPO_INGEN_SOCKET" "$OUT/catalogue.json" "$REPO/config/catalogue/overlay.json" >"$OUT/live.txt"
tail -1 "$OUT/live.txt"
if [ "${PLUGIN_TEST_CONTROL_ONLY:-0}" != 1 ]; then
    python3 "$HERE/22-patchstorage-check.py" "$OUT/catalogue.json" "$OUT/live.txt" "$OUT/results.json" "$RUN/bundles" "$OUT/inventory.txt" | tee "$OUT/signal.txt"
fi
timeout 300 "$live" "unix://$PEEPO_INGEN_SOCKET" "$OUT/catalogue.json" "$REPO/config/catalogue/overlay.json" >"$OUT/control-recheck.txt"
tail -1 "$OUT/control-recheck.txt"
if [ -n "${PLUGIN_TEST_LEGACY:-}" ]; then
    timeout 300 "$live" "unix://$PEEPO_INGEN_SOCKET" "$PLUGIN_TEST_LEGACY" "$REPO/config/catalogue/overlay.json" >"$OUT/legacy-controls.txt"
    tail -1 "$OUT/legacy-controls.txt"
fi
python3 - "$OUT/results.json" "$REPO/config/catalogue/patchstorage.json" "$OUT/ingen.log" <<'PY'
import hashlib,json,os,pathlib,sys
p=pathlib.Path(sys.argv[1]);r=json.loads(p.read_text())
r['control_recheck']=True
r['manifest_sha256']=hashlib.sha256(pathlib.Path(sys.argv[2]).read_bytes()).hexdigest()
prefix=pathlib.Path(os.environ['PLUGIN_TEST_ENGINE_PREFIX'])
server=next(prefix.glob('lib/*/libingen_server.so'))
assert 'Loading module "'+str(server)+'"' in pathlib.Path(sys.argv[3]).read_text(), 'The staged server module was not loaded'
r['server_module_path']=str(server)
r['engine_server_sha256']=hashlib.sha256(server.read_bytes()).hexdigest()
root=pathlib.Path(os.environ['LV2_PATH'].split(':')[0]);tree=hashlib.sha256()
for f in sorted(root.rglob('*')):
 if f.is_file():
  tree.update(f.relative_to(root).as_posix().encode());tree.update(hashlib.sha256(f.read_bytes()).digest())
r['plugin_tree_sha256']=tree.hexdigest()
if os.environ.get('PLUGIN_TEST_LEGACY'):r['legacy_controls_passed']=True
p.write_text(json.dumps(r,indent=2)+'\n')
PY
