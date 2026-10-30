#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
#
# USB sticks (P14.1), without one plugged in: disk images on loop devices
# stand in for sticks, and peepo-usb is run on them as its unit would run it.
# Checks FAT32, exFAT and NTFS mounted read-only, nosuid, nodev and noexec
# with their files owned by peepo, the info file the interface reads, an
# unreadable filesystem (ext4) and an empty disk reported, a disk mounted
# elsewhere refused, bad device names refused, and unmounting.
# Run as root, on the Pi or in WSL:  bash check-usb.sh [path/to/peepo-usb]
# NTFS is skipped where the kernel has no ntfs3 (WSL). The udev rule and the
# unit are checked on the Pi by plugging a real stick (P14.4).
set -uo pipefail
HERE="$(dirname "$(readlink -f "$0")")"
USB=${1:-}
[ -n "$USB" ] || for try in /opt/peepo/bin/peepo-usb "$HERE/config/bin/peepo-usb" "$HERE/../../config/pi/bin/peepo-usb"; do
    [ -x "$try" ] && { USB=$try; break; }
done
[ -n "$USB" ] && [ -f "$USB" ] || { echo "peepo-usb not found; give its path"; exit 2; }
[ "$(id -u)" = 0 ] || { echo "Run as root (sudo)."; exit 1; }
OWNER=peepo; id "$OWNER" >/dev/null 2>&1 || OWNER=nobody
export PEEPO_USB_RUN=/run/peepo-usb-check PEEPO_USB_OWNER=$OWNER
WORK=$(mktemp -d)
failures=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; failures=$((failures + 1)); }
skip() { echo "SKIP  $*"; }
usb() { sh "$USB" "$@"; }
info() { sed -n "s/^$2=//p" "$PEEPO_USB_RUN/$1.info" 2>/dev/null; }
cleanup() {
    umount "$WORK/fill" "$WORK/elsewhere" 2>/dev/null
    for img in "$WORK"/*.img; do
        for loop in $(losetup -nO NAME -j "$img" 2>/dev/null); do
            usb unmount "${loop#/dev/}" >/dev/null 2>&1
            losetup -d "$loop" 2>/dev/null
        done
    done
    rm -rf "$WORK" "$PEEPO_USB_RUN"
}
trap cleanup EXIT

# A 64 MB stick of the given type, with a model, an IR in a folder and a
# script that must not run. Prints its loop device.
make_stick() {  # type label
    local img=$WORK/$1.img loop
    truncate -s 64M "$img"
    case $1 in
        vfat) mkfs.vfat -F 32 -n "$2" "$img" ;;
        exfat) mkfs.exfat -L "$2" "$img" ;;
        ntfs) mkfs.ntfs -F -Q -L "$2" "$img" ;;
        ext4) mkfs.ext4 -q -L "$2" "$img" ;;
        none) ;;
    esac >/dev/null 2>&1 || return 1
    loop=$(losetup -f --show "$img") || return 1
    if [ "$1" != none ]; then
        mkdir -p "$WORK/fill"
        local fs=$1; [ "$fs" = ntfs ] && fs=ntfs-3g
        if mount -t "$fs" "$loop" "$WORK/fill" 2>/dev/null; then
            mkdir -p "$WORK/fill/Cab IRs"
            echo '{"version":"0.5.4","architecture":"WaveNet","config":{},"weights":[]}' >"$WORK/fill/Clean amp.nam"
            head -c 4096 /dev/zero >"$WORK/fill/Cab IRs/4x12.wav"
            printf '#!/bin/sh\necho ran\n' >"$WORK/fill/run.sh"; chmod 0755 "$WORK/fill/run.sh"
            umount "$WORK/fill"
        fi
    fi
    echo "$loop"
}

check_readable() {  # type label
    local type=$1 label=$2 loop dev dir options
    loop=$(make_stick "$type" "$label") || { skip "$type: cannot make the image here"; return; }
    dev=${loop#/dev/}; dir=$PEEPO_USB_RUN/$dev
    usb mount "$dev" >/dev/null
    if [ "$type" = ntfs ] && [[ $(info "$dev" message) == *"cannot read"* ]]; then
        skip "ntfs: no ntfs3 in this kernel ($(info "$dev" message))"
        usb unmount "$dev"; return
    fi
    [ "$(info "$dev" state)" = mounted ] && pass "$type: mounted" || { fail "$type: state '$(info "$dev" state)': $(info "$dev" message)"; return; }
    [ "$(info "$dev" label)" = "$label" ] && pass "$type: label '$label'" || fail "$type: label '$(info "$dev" label)'"
    [ "$(info "$dev" type)" = "$type" ] && pass "$type: type" || fail "$type: type '$(info "$dev" type)'"
    options=",$(findmnt -no OPTIONS "$dir"),"
    local missing=''
    for o in ro nosuid nodev noexec; do [[ $options == *",$o,"* ]] || missing="$missing $o"; done
    [ -z "$missing" ] && pass "$type: ro, nosuid, nodev, noexec" || fail "$type: options lack$missing ($options)"
    [ -f "$dir/Clean amp.nam" ] && [ -f "$dir/Cab IRs/4x12.wav" ] && pass "$type: files and folders with spaces readable" || fail "$type: files missing: $(ls -R "$dir" 2>&1 | tr '\n' ' ')"
    [ "$(stat -c %U "$dir/Clean amp.nam" 2>/dev/null)" = "$OWNER" ] && pass "$type: files owned by $OWNER" || fail "$type: owner $(stat -c %U "$dir/Clean amp.nam" 2>&1)"
    [ "$(stat -c %a "$dir/Clean amp.nam" 2>/dev/null)" = 644 ] && pass "$type: files 644" || fail "$type: mode $(stat -c %a "$dir/Clean amp.nam" 2>&1)"
    touch "$dir/written" 2>/dev/null && fail "$type: writable" || pass "$type: cannot be written"
    [ "$("$dir/run.sh" 2>/dev/null)" = ran ] && fail "$type: a script on it ran" || pass "$type: a script on it cannot run"
    [ "$(stat -c %a "$PEEPO_USB_RUN/$dev.info")" = 644 ] && pass "$type: info readable by all" || fail "$type: info mode"
    usb mount "$dev" >/dev/null
    [ "$(findmnt -n "$dir" | wc -l)" = 1 ] && pass "$type: mounting twice leaves one mount" || fail "$type: $(findmnt -n "$dir" | wc -l) mounts"
    usb unmount "$dev"
    ! mountpoint -q "$dir" 2>/dev/null && [ ! -e "$dir" ] && [ ! -e "$PEEPO_USB_RUN/$dev.info" ] && pass "$type: unmounted and forgotten" || fail "$type: left behind after unmount"
}

check_readable vfat "PEEPO 1"
check_readable exfat "Peepo stick"
check_readable ntfs "Peepo NTFS"

loop=$(make_stick ext4 "Mac stick") && {
    dev=${loop#/dev/}; usb mount "$dev" >/dev/null
    [ "$(info "$dev" state)" = unreadable ] && [[ $(info "$dev" message) == *ext4* ]] && ! mountpoint -q "$PEEPO_USB_RUN/$dev" &&
        pass "ext4: unreadable, says so: $(info "$dev" message)" || fail "ext4: state '$(info "$dev" state)': $(info "$dev" message)"
    usb unmount "$dev"
}
loop=$(make_stick none "") && {
    dev=${loop#/dev/}; usb mount "$dev" >/dev/null
    [ "$(info "$dev" state)" = unreadable ] && pass "empty disk: unreadable, says so: $(info "$dev" message)" || fail "empty disk: state '$(info "$dev" state)'"
    usb unmount "$dev"
}
loop=$(make_stick vfat "IN USE") && {
    dev=${loop#/dev/}; mkdir -p "$WORK/elsewhere"; mount -o ro "$loop" "$WORK/elsewhere"
    usb mount "$dev" >/dev/null
    [ "$(info "$dev" state)" = refused ] && ! mountpoint -q "$PEEPO_USB_RUN/$dev" &&
        pass "disk mounted elsewhere: refused: $(info "$dev" message)" || fail "disk mounted elsewhere: state '$(info "$dev" state)'"
    usb unmount "$dev"; umount "$WORK/elsewhere"
}
root=$(findmnt -no SOURCE /)
if [ -b "$root" ]; then
    dev=$(basename "$root"); usb mount "$dev" >/dev/null
    [ "$(info "$dev" state)" = refused ] && pass "the disk Linux runs from ($dev): refused" || fail "root disk $dev: state '$(info "$dev" state)'"
    usb unmount "$dev"
fi
for bad in ../sda1 'sda1;reboot' '' .hidden 'sd a1'; do
    usb mount "$bad" 2>/dev/null; [ $? = 2 ] && pass "bad device name '$bad' refused" || fail "bad device name '$bad' accepted"
done

echo
[ $failures = 0 ] && echo "All USB stick checks passed." || echo "$failures check(s) failed."
exit $((failures > 0))
