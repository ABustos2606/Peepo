#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
# P13.10 on the Pi: the scenes checks of docs/44-scenes.md with the Pi's own
# engine build and audio interface. Run as root, with 27-scenes.py and
# ingenprobe.py (tools/ingen-spike) beside this script:
#   sudo check-scenes.sh tails [URI-SUBSTRING]  tail, click (off and on), replay,
#                                               side and sleep of every Delay,
#                                               Reverb and Texture module
#   sudo check-scenes.sh spill [URI-SUBSTRING]  the same with every module that
#                                               uses the engine's bypass spilling,
#                                               before it is marked "spill"
#   sudo check-scenes.sh playing                a scene every 500 ms for a minute
#                                               while a tone plays
#   sudo check-scenes.sh cpu [PATCH_ID...]      each stored patch as saved and
#                                               with every module switched on
# The IR reverbs get a made-up impulse response (the Pi has none for reverbs).
# tails, spill and playing stop the instrument and run an engine of their own on the
# instrument's JACK, wired only to the probe, so nothing reaches the speakers;
# cpu loads each patch into the instrument's engine with the guitar and
# speakers unwired. The instrument is started again, or the running patch
# loaded again, at the end, whatever happens. Results: /tmp/peepo-scenes/.
# SCENES_GDB=1 runs their engine under gdb, for a backtrace; SCENES_TAG is
# added to the results' name.
set -euo pipefail
HERE="$(dirname "$(readlink -f "$0")")"
OUT=/tmp/peepo-scenes
CATALOGUE=/opt/peepo/share/peepo/catalogue.json
OVERLAY=/opt/peepo/share/peepo/catalogue-overlay.json
TOOL=/opt/peepo/libexec/peepo/peepo_effect_live
[ "$(id -u)" = 0 ] || { echo 'Run as root'; exit 1; }
for unit in peepo-jack peepo-engine peepo-route; do systemctl is-active --quiet "$unit"; done
python3 -c 'import jack, numpy' 2>/dev/null || env DEBIAN_FRONTEND=noninteractive apt-get install -y -qq python3-jack-client python3-numpy
# The probe runs as the instrument's user, the owner of the JACK server.
mkdir -p "$OUT"
cp "$HERE/27-scenes.py" "$HERE/ingenprobe.py" "$OUT/"
chown -R peepo: "$OUT"
as_peepo() { runuser -u peepo -- env "$@"; }
# A probe that hangs must not keep the instrument stopped: an hour at most.
probe() { timeout -k 10 3600 runuser -u peepo -- env "$@"; }

shown=$(runuser -u peepo -- "$TOOL" show)
patch=$(sed -n 's/^GRAPH .* id=\([^ ]*\) dirty=.*/\1/p' <<<"$shown")
dirty=$(sed -n 's/^GRAPH .* dirty=\(.*\)/\1/p' <<<"$shown")
[ "$dirty" = 0 ] || { echo "The running patch must be saved first (id=$patch dirty=$dirty)"; exit 1; }
what=${1:-}; shift || true
echo "VERSION $(cat /opt/peepo/VERSION 2>/dev/null || ls -l --time-style=+%F /opt/peepo/bin/peepo | awk '{print $6}')"
echo "throttled: $(vcgencmd get_throttled 2>/dev/null || echo unknown)"
grep -E '^JACK_DRIVER_OPTIONS' /etc/peepo/peepo.conf || true

case $what in
tails|spill|playing)
    SOCKET=$OUT/ingen.sock
    restart() {
        # Only this check's engine runs now. It can hang in its exit handler:
        # ask, then insist.
        pkill -u peepo -x ingen 2>/dev/null || true
        for _ in $(seq 30); do pgrep -u peepo -x ingen >/dev/null || break; sleep 0.1; done
        pkill -9 -u peepo -x ingen 2>/dev/null || true
        sleep 1
        systemctl start peepo-engine peepo-route peepo-midi peepo-ui
        echo "instrument started again: $(systemctl is-active peepo-engine peepo-route peepo-ui | paste -sd' ')"
    }
    trap restart EXIT
    systemctl stop peepo-ui peepo-midi peepo-route peepo-engine
    rm -f "$SOCKET"
    if [ "${SCENES_GDB:-0}" = 1 ]; then
        # The engine under gdb: a crash leaves every thread's backtrace in the log.
        PREFIX=/opt/peepo
        as_peepo LD_LIBRARY_PATH="$(find $PREFIX/lib -maxdepth 2 -name 'libingen*' -printf '%h\n' | sort -u | paste -sd:)" \
            LV2_PATH="$(find $PREFIX/lib -maxdepth 2 -name lv2 -type d | paste -sd:):$PREFIX/nam/current:/usr/lib/lv2" \
            gdb -q -batch -ex 'handle SIGPIPE nostop noprint pass' -ex run -ex 'x/6i $pc-12' -ex 'info registers' -ex 'info sharedlibrary' -ex 'thread apply all bt' \
            --args $PREFIX/bin/ingen -e -f -a -E 0 -S "$SOCKET" >"$OUT/ingen-$what.log" 2>&1 &
    else
        as_peepo PEEPO_SOCKET="$SOCKET" /opt/peepo/bin/peepo-engine >"$OUT/ingen-$what.log" 2>&1 &
    fi
    for _ in $(seq 100); do [ -S "$SOCKET" ] && break; sleep 0.1; done
    [ -S "$SOCKET" ] || { echo "engine did not open its socket"; tail -20 "$OUT/ingen-$what.log"; exit 1; }
    pid=$(pgrep -n -u peepo -x ingen)
    if [ "$what" != playing ]; then
        # The Pi has no reverb impulse responses: a made-up one, 2 s of stereo
        # noise decaying by 60 dB, so the IR reverbs have a tail to measure.
        as_peepo python3 -c 'import numpy, wave, sys
r = 48000; t = numpy.arange(2 * r) / r
x = numpy.random.default_rng(3).uniform(-1, 1, (2 * r, 2)) * (10 ** (-3 * t))[:, None] * 0.5
w = wave.open(sys.argv[1], "wb"); w.setnchannels(2); w.setsampwidth(2); w.setframerate(r)
w.writeframes((x * 32767).astype("<i2").tobytes()); w.close()' "$OUT/reverb-ir.wav"
        probe PEEPO_INGEN_SOCKET="$SOCKET" PEEPO_ENGINE_PID="$pid" SCENES_ON_AGAIN=1 SCENES_IR="$OUT/reverb-ir.wav" \
            SCENES_SPILL=$([ "$what" = spill ] && echo 1 || echo 0) SCENES_RESULTS="$OUT/scenes-$what-pi${SCENES_TAG:-}.json" \
            python3 "$OUT/27-scenes.py" "$what" "$CATALOGUE" "$OVERLAY" "${1:-}"
    else
        # SCENES_EVERY=0 plays the same minute without scene changes, for a baseline.
        probe PEEPO_INGEN_SOCKET="$SOCKET" PEEPO_ENGINE_PID="$pid" SCENES_SECONDS="${SCENES_SECONDS:-60}" \
            SCENES_EVERY="${SCENES_EVERY:-0.5}" SCENES_READ_BACK="${SCENES_READ_BACK:-1}" SCENES_DUMP="${SCENES_DUMP:-}" \
            SCENES_RESULTS="$OUT/scenes-playing-pi${SCENES_TAG:-}.json" \
            python3 "$OUT/27-scenes.py" playing "$CATALOGUE" "$OVERLAY"
    fi
    grep -E "rror" "$OUT/ingen-$what.log" | sort | uniq -c | head -10 || true
    ;;
cpu)
    restore() {
        runuser -u peepo -- "$TOOL" restore "$patch" | grep -E '^(GRAPH)' || true
        systemctl start peepo-route
        sleep 3
        echo "running patch $patch loaded again; guitar and speakers wired again"
    }
    trap restore EXIT
    # The router would wire the guitar and speakers back in mid-measurement.
    systemctl stop peepo-route
    ids=("$@")
    [ ${#ids[@]} -gt 0 ] || mapfile -t ids < <(ls /var/lib/peepo/patches | grep -v '^startup$')
    pid=$(pgrep -n -x -u peepo ingen)
    for id in "${ids[@]}"; do
        echo "== $id: $(runuser -u peepo -- "$TOOL" restore "$id" | sed -n 's/^GRAPH \(.*\) id=.*/\1/p')"
        sleep 3
        probe PEEPO_INGEN_SOCKET=/run/peepo/ingen.sock PEEPO_ENGINE_PID="$pid" SCENES_RESULTS="$OUT/scenes-cpu-$id.json" \
            python3 "$OUT/27-scenes.py" cpu "$CATALOGUE" "$OVERLAY" || echo "FAIL  measuring $id"
    done
    ;;
*)
    echo "usage: check-scenes.sh tails|spill [URI-SUBSTRING] | playing | cpu [PATCH_ID...]"; exit 2 ;;
esac
