#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
#
# Install a Peepo package on the Pi. Run as root from the unpacked package:
#   sudo ./install.sh            install, keeping the previous one for rollback
#   sudo ./install.sh --enable   also start the instrument at every boot
#   sudo ./install.sh --instrument  direct boot, without desktop or MODEP
#   sudo ./install.sh --start    also start it now
#   sudo ./install.sh --rollback put the previous installation back
#   sudo ./install.sh --remove   stop and remove the services (keeps patches and settings)
#   ./install.sh --dry-run ...   say what would be done
# It never edits /boot/firmware/config.txt. Only --instrument stops and masks
# the desktop/MODEP; ordinary installation reports competing audio services.
set -euo pipefail
HERE="$(dirname "$(readlink -f "$0")")"
PREFIX=/opt/peepo
UNITS="peepo.target peepo-jack.service peepo-engine.service peepo-route.service peepo-a2j.service peepo-midi.service peepo-ui.service"
dry=0; enable=0; start=0; instrument=0; action=install
for arg in "$@"; do
    case $arg in
        --dry-run) dry=1 ;; --enable) enable=1 ;; --start) start=1 ;;
        --instrument) instrument=1 ;;
        --rollback) action=rollback ;; --remove) action=remove ;;
        *) echo "unknown option $arg"; exit 2 ;;
    esac
done
[ $instrument = 0 ] || [ "$action" = install ] || { echo '--instrument is an installation option.'; exit 2; }
run() { echo "+ $*"; [ $dry = 1 ] || "$@"; }
[ $dry = 1 ] || [ "$(id -u)" = 0 ] || { echo "run as root (sudo)"; exit 1; }

if [ $action = remove ]; then
    if [ -f /etc/peepo/instrument-boot-state/units ] && [ -x "$PREFIX/bin/peepo-instrument-boot" ] && [ "$(systemctl show -p LoadState --value peepo.target)" = loaded ]; then
        run "$PREFIX/bin/peepo-instrument-boot" --restore
    fi
    if [ -f /etc/peepo/splash-state/cmdline.txt ] && [ -x "$PREFIX/bin/peepo-splash" ]; then run "$PREFIX/bin/peepo-splash" --restore; fi
    if [ -f /etc/peepo/slim-boot-state/units ] && [ -x "$PREFIX/bin/peepo-slim-boot" ]; then run "$PREFIX/bin/peepo-slim-boot" --restore; fi
    for unit in $UNITS; do run systemctl disable --now "$unit" 2>/dev/null || true; run rm -f "/etc/systemd/system/$unit"; done
    run rm -f /etc/polkit-1/rules.d/50-peepo-power.rules
    run systemctl daemon-reload
    echo "Removed the services. $PREFIX, /etc/peepo and /var/lib/peepo are kept."
    exit 0
fi
if [ $action = rollback ]; then
    [ -d "$PREFIX.previous" ] || { echo "no previous installation at $PREFIX.previous"; exit 1; }
    run systemctl stop peepo.target 2>/dev/null || true
    run rm -rf "$PREFIX.failed"
    run mv "$PREFIX" "$PREFIX.failed"
    run mv "$PREFIX.previous" "$PREFIX"
    echo "Restored the previous installation; the replaced one is in $PREFIX.failed."
    exit 0
fi

# The package must match this machine.
package_arch=$(awk '{print $2}' "$HERE/VERSION")
machine_arch=$(dpkg --print-architecture)
[ "$package_arch" = "$machine_arch" ] || { echo "this package is for $package_arch, this machine is $machine_arch"; exit 1; }
echo "Installing Peepo $(cat "$HERE/VERSION")"

# What the programs need from the distribution (they were built against these).
DEPENDS="jackd2 a2jmidid liblilv-0-0 libserd-0-0 libsord-0-0 libsratom-0-0 libsuil-0-0 libsigc++-2.0-0v5 libjack-jackd2-0 libdrm2 mda-lv2 lv2-examples x42-plugins setbfree blop-lv2 fomp calf-plugins libzita-convolver4 libzita-resampler1 libsndfile1 libsamplerate0 libfftw3-single3 libcairo2 libx11-6 libxext6 libxrandr2 libxcursor1"
if apt-cache show jack-example-tools >/dev/null 2>&1; then DEPENDS="$DEPENDS jack-example-tools"; fi
run env DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends $DEPENDS

# Anything else that may own the audio interface is reported, never stopped.
for other in modep-mod-host modep-mod-ui jack jackd pipewire pulseaudio; do
    if systemctl is-active --quiet "$other" 2>/dev/null; then
        echo "NOTE: $other is running and may hold the audio interface. Stop it yourself before starting Peepo."
    fi
done

# The service account, with access to audio, the screen and the controls.
if ! id peepo >/dev/null 2>&1; then
    groups=""
    for group in audio video render input gpio; do getent group "$group" >/dev/null && groups="$groups,$group"; done
    run useradd --system --home-dir /var/lib/peepo --shell /usr/sbin/nologin --groups "${groups#,}" peepo
fi
# Real-time scheduling for JACK and the engine is granted by the units (LimitRTPRIO).

# The programs: the previous installation stays as $PREFIX.previous.
if [ -d "$PREFIX" ]; then run rm -rf "$PREFIX.previous"; run mv "$PREFIX" "$PREFIX.previous"; fi
run cp -a "$HERE/root/opt/peepo" "$PREFIX"
run install -m 0755 "$HERE/config/bin/peepo-engine" "$HERE/config/bin/peepo-route" "$PREFIX/bin/"
run install -m 0755 "$HERE/export-catalogue.sh" "$PREFIX/bin/peepo-export-catalogue"
run install -m 0755 "$HERE/instrument-boot.sh" "$PREFIX/bin/peepo-instrument-boot"
run install -m 0755 "$HERE/splash.sh" "$PREFIX/bin/peepo-splash"
run install -m 0755 "$HERE/slim-boot.sh" "$PREFIX/bin/peepo-slim-boot"
run install -d -m 0755 "$PREFIX/share/peepo/splash"
run cp -r "$HERE/config/splash/peepo" "$PREFIX/share/peepo/splash/"
run install -m 0755 "$HERE/check-ui-restart.sh" "$HERE/check-ui-crash.sh" "$HERE/monitor-ui-restart.py" "$HERE/check-effect.sh" "$HERE/measure-effect.py" "$HERE/check-output-level.sh" "$PREFIX/libexec/peepo/"
run "$PREFIX/bin/peepo-export-catalogue"

# Settings are the user's: installed once, never overwritten.
run install -d -m 0755 /etc/peepo
for file in peepo.conf controls.conf; do
    if [ -e "/etc/peepo/$file" ]; then echo "keeping /etc/peepo/$file (new default: $PREFIX/share/peepo/$file)"; fi
    run install -m 0644 "$HERE/config/$file" "$PREFIX/share/peepo/$file"
    [ -e "/etc/peepo/$file" ] || run install -m 0644 "$HERE/config/$file" "/etc/peepo/$file"
done
run install -m 0644 "$HERE/config/config.txt" "$PREFIX/share/peepo/config.txt"
run install -d -o peepo -g peepo -m 0755 /var/lib/peepo /var/lib/peepo/patches

for unit in $UNITS; do run install -m 0644 "$HERE/config/systemd/$unit" "/etc/systemd/system/$unit"; done
# The interface's Shut down button.
run install -d -m 0755 /etc/polkit-1/rules.d
run install -m 0644 "$HERE/config/polkit/50-peepo-power.rules" /etc/polkit-1/rules.d/
run systemctl daemon-reload
if [ $instrument = 1 ]; then
    if [ $dry = 1 ]; then run "$PREFIX/bin/peepo-instrument-boot" --dry-run
    else run "$PREFIX/bin/peepo-instrument-boot"; fi
fi
if [ $enable = 1 ]; then run systemctl enable peepo.target; fi
if [ $start = 1 ]; then run systemctl restart peepo.target; fi

cat <<EOF
Installed in $PREFIX; settings in /etc/peepo; patches in /var/lib/peepo/patches.
Wiring of the encoders and footswitches: add the lines of $PREFIX/share/peepo/config.txt
to /boot/firmware/config.txt yourself, then reboot.
Start now:          sudo systemctl start peepo.target
Start at every boot: sudo systemctl enable peepo.target
Direct instrument boot (saves previous desktop/MODEP policy): sudo $PREFIX/bin/peepo-instrument-boot
Peepo boot screen instead of boot messages: sudo $PREFIX/bin/peepo-splash
Faster boot without the desktop's services: sudo $PREFIX/bin/peepo-slim-boot
Restart only the interface (audio keeps playing): sudo systemctl restart peepo-ui
EOF
