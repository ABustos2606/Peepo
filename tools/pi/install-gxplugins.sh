#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
# Incremental GxPlugins install after native verification, without replacing
# the engine, UI executable, four existing bundles or playing graph.
# Usage: sudo bash tools/pi/install-gxplugins.sh STAGE NATIVE-RESULTS.json
# STAGE is the PREFIX used by 21-gxplugins.sh for the native staging build.
set -euo pipefail
REPO="$(readlink -f "$(dirname "$(readlink -f "$0")")/../..")"
STAGE="$(readlink -f "${1:?Give the native staging prefix}")"
RESULTS="$(readlink -f "${2:?Give the native test result JSON}")"
[ "$(id -u)" = 0 ] || { echo 'Run as root'; exit 1; }
export REPO STAGE RESULTS
python3 - <<'PY'
import json, os, platform
from pathlib import Path
manifest = json.load(open(Path(os.environ['REPO']) / 'config/catalogue/gxplugins.json'))
result = json.load(open(os.environ['RESULTS']))
assert result['machine'] == platform.machine() == 'aarch64'
assert result['passed'] == result['total'] == 43
assert result['collection_commit'] == manifest['collection_commit']
assert {e['uri'] for e in result['entries']} == {p['uri'] for p in manifest['plugins']}
assert all((Path(os.environ['STAGE']) / 'lib/lv2' / p['bundle'] / 'manifest.ttl').is_file() for p in manifest['plugins'])
PY
BACKUP=$(mktemp -d /opt/peepo/gxplugins-backup.XXXXXX)
cp /opt/peepo/share/peepo/catalogue*.json /opt/peepo/share/peepo/catalogue-inventory.txt "$BACKUP/"
RECORD=$(mktemp -d /var/lib/peepo/gxplugins-update.XXXXXX)
chown peepo:peepo "$RECORD"
chmod 0755 "$RECORD"
# Never overwrite a bundle that the running engine may have mapped.
for bundle in "$STAGE"/lib/lv2/*.lv2; do
    name=$(basename "$bundle")
    [ -e "/opt/peepo/lib/lv2/$name" ] || cp -a "$bundle" /opt/peepo/lib/lv2/
done
mkdir -p /opt/peepo/share/peepo/sources /opt/peepo/share/peepo/licenses
cp -a "$STAGE/share/peepo/sources/gxplugins" /opt/peepo/share/peepo/sources/
cp -a "$STAGE/share/peepo/licenses/gxplugins" /opt/peepo/share/peepo/licenses/
cp "$REPO/config/catalogue/gxplugins.json" /opt/peepo/share/peepo/sources/gxplugins/
cp "$REPO/config/catalogue/gxplugins.json" /opt/peepo/share/peepo/sources/gxplugins/config/catalogue/
for mapping in 'inventory.txt catalogue-inventory.txt' 'overlay.json catalogue-overlay.json'; do
    read -r source dest <<< "$mapping"
    tmp=$(mktemp "/opt/peepo/share/peepo/$dest.XXXXXX")
    install -m 0644 "$REPO/config/catalogue/$source" "$tmp"
    mv "$tmp" "/opt/peepo/share/peepo/$dest"
done
/opt/peepo/bin/peepo-export-catalogue
engine_before=$(systemctl show -p MainPID --value peepo-engine)
python3 "$REPO/tools/ingen-spike/21-gxplugins-register.py" "$REPO/config/catalogue/gxplugins.json" /run/peepo/ingen.sock "$RECORD"
# Reload only presentation metadata. The playing engine and JACK stay up.
systemctl restart peepo-ui
[ "$(systemctl show -p MainPID --value peepo-engine)" = "$engine_before" ]
systemctl is-active --quiet peepo-engine peepo-ui peepo-jack
echo "PASS installed and registered; engine PID $engine_before unchanged"
echo "BACKUP $BACKUP"
echo "RECORD $RECORD"
