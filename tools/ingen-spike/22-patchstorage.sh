#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Install the explicitly requested expansion at locked artifact/source versions.
set -euo pipefail
HERE="$(dirname "$(readlink -f "$0")")"
REPO="$(readlink -f "$HERE/../..")"
python3 "$REPO/tools/install-patchstorage.py" --prefix "${PREFIX:-/opt/peepo}" \
    --cache "${PLUGIN_ARTIFACT_CACHE:-$REPO/build/scratch/expanded-lv2-cache}"
