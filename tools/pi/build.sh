#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
#
# Build everything the instrument runs into /opt/peepo, test it, and pack it
# for installation: the engine (Poly's Ingen fork with our patches), the
# Mutable-derived plugins, and the interface for the Pi's own screen. Runs as
# root on Debian or Raspberry Pi OS, natively on the Pi or in the arm64 root of
# build-arm64.sh:
#   bash build.sh [--skip-engine] [--skip-tests]
# The package lands in build/pi/ of the repository.
set -euo pipefail
REPO="$(readlink -f "$(dirname "$(readlink -f "$0")")/../..")"
PREFIX=/opt/peepo
BUILD=$PREFIX/build/interface
ARCH=$(dpkg --print-architecture)
skip_engine=0; skip_tests=0
for arg in "$@"; do
    case $arg in --skip-engine) skip_engine=1 ;; --skip-tests) skip_tests=1 ;; *) echo "unknown option $arg"; exit 2 ;; esac
done
export DEBIAN_FRONTEND=noninteractive

step() { echo "==== $*"; }
if [ $skip_engine = 0 ]; then
    step "engine"
    bash "$REPO/tools/ingen-spike/01-build.sh"
    step "plugins"
    bash "$REPO/tools/ingen-spike/02-plugins.sh"
    step "NAM"
    bash "$REPO/tools/ingen-spike/12-nam.sh"
    step "full catalogue plugins"
    bash "$REPO/tools/ingen-spike/14-more-plugins.sh"
fi

step "interface"
# Libraries built into the prefix (lilv on Debian 12) come before the system's.
export PKG_CONFIG_PATH="$(find "$PREFIX" -name pkgconfig -type d 2>/dev/null | paste -sd:)"
# DRM/KMS and the framebuffer for the Pi's screen; no desktop window.
apt-get install -y -qq --no-install-recommends cmake ninja-build g++ git pkg-config libdrm-dev liblilv-dev ca-certificates >/dev/null
cmake -S "$REPO" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX="$PREFIX" -DPEEPO_WITH_SDL=OFF -DBUILD_TESTING=ON >/dev/null
cmake --build "$BUILD"
[ -x "$BUILD/peepo" ] || { echo "the interface did not build"; exit 1; }
cmake --install "$BUILD" >/dev/null

if [ $skip_tests = 0 ]; then
    step "tests"
    (cd "$BUILD" && ctest --output-on-failure 2>&1 | tail -15)
    "$BUILD/peepo" --display offscreen --smoke --screenshot "$PREFIX/build/screen-$ARCH.bmp"
fi

step "catalogue"
# Reused prefixes can still hold retired plugins, even with --skip-engine.
# Move only dedicated bundles; shared Debian bundles remain intact.
python3 "$REPO/tools/prune-retired-bundles.py" --prefix "$PREFIX" --backup "$PREFIX/build/retired-bundles"
# The modules this build of the engine can load, exported here so the Pi
# offers exactly these; the project's overlay goes beside it.
mkdir -p "$PREFIX/share/peepo"
cp "$REPO/config/catalogue/inventory.txt" "$PREFIX/share/peepo/catalogue-inventory.txt"
bash "$REPO/tools/pi/export-catalogue.sh"
cp "$REPO/config/catalogue/overlay.json" "$PREFIX/share/peepo/catalogue-overlay.json"
cp "$REPO/config/catalogue/retired.json" "$PREFIX/share/peepo/catalogue-retired.json"
echo "$(grep -c '"uri"' "$PREFIX/share/peepo/catalogue.json") plugins exported"

step "package"
# The repository may belong to another user, and Git LFS need not be installed
# here: neither matters for naming the commit.
VERSION=$(git -c safe.directory="*" -c filter.lfs.process= -c filter.lfs.clean=cat -c filter.lfs.smudge=cat -c filter.lfs.required=false \
    -C "$REPO" describe --always --dirty 2>/dev/null || echo unknown)
STAGE=$PREFIX/build/stage
rm -rf "$STAGE"; mkdir -p "$STAGE/peepo/root/opt"
# The prefix without its sources and build trees, and what installs it.
tar -C /opt --exclude=peepo/src --exclude=peepo/build -cf - peepo | tar -C "$STAGE/peepo/root/opt" -xf -
cp -r "$REPO/config/pi" "$STAGE/peepo/config"
cp "$REPO/tools/pi/install.sh" "$REPO/tools/pi/instrument-boot.sh" "$REPO/tools/pi/splash.sh" "$REPO/tools/pi/slim-boot.sh" "$REPO/tools/pi/check-services.sh" "$REPO/tools/pi/export-catalogue.sh" \
    "$REPO/tools/pi/check-ui-restart.sh" "$REPO/tools/pi/check-ui-crash.sh" "$REPO/tools/pi/monitor-ui-restart.py" "$REPO/tools/pi/check-effect.sh" "$REPO/tools/pi/measure-effect.py" "$REPO/tools/pi/check-output-level.sh" "$STAGE/peepo/"
cp -r "$REPO/licenses" "$REPO/LICENSE" "$REPO/THIRD_PARTY_NOTICES.md" "$STAGE/peepo/"
echo "$VERSION $ARCH $(date -u +%Y-%m-%dT%H:%MZ)" > "$STAGE/peepo/VERSION"
OUT="$REPO/build/pi"
mkdir -p "$OUT"
PACKAGE="$OUT/peepo-$ARCH-$(date -u +%Y%m%d)-$VERSION.tar.gz"
tar -C "$STAGE" -czf "$PACKAGE" peepo
ls -l "$PACKAGE"
