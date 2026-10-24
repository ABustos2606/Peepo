# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
# Shared environment for the Ingen spike scripts. Source it.
export PREFIX=/opt/peepo
export PATH="$PREFIX/bin:$PATH"
export LD_LIBRARY_PATH="$(find "$PREFIX/lib" -maxdepth 2 -name 'libingen*' -printf '%h\n' | sort -u | paste -sd:)"
export LV2_PATH="/usr/lib/lv2:$(find "$PREFIX/lib" -maxdepth 2 -name lv2 -type d | paste -sd:)"
export SOCK=unix:///tmp/peepo-ingen.sock
export RUN=/tmp/peepo-spike

start_engine() {
    # JACK's dummy backend stands in for an audio interface: 48 kHz, 128 frames
    # unless PEEPO_PERIOD says otherwise.
    mkdir -p "$RUN"
    stop_engine
    rm -f /tmp/peepo-ingen.sock
    JACK_NO_AUDIO_RESERVATION=1 nohup jackd -d dummy -r 48000 -p "${PEEPO_PERIOD:-128}" >"$RUN/jackd.log" 2>&1 &
    for _ in $(seq 50); do jack_lsp >/dev/null 2>&1 && break; sleep 0.1; done
    # -a as the instrument runs it (config/pi/bin/peepo-engine); PEEPO_ATOMIC=0
    # starts the engine without it, for comparison.
    local atomic=-a
    [ "${PEEPO_ATOMIC:-1}" = 0 ] && atomic=
    nohup ingen -e -f $atomic -S /tmp/peepo-ingen.sock "$@" >"$RUN/ingen.log" 2>&1 &
    for _ in $(seq 50); do [ -S /tmp/peepo-ingen.sock ] && break; sleep 0.1; done
    [ -S /tmp/peepo-ingen.sock ] || { echo "engine did not open its socket"; tail -20 "$RUN/ingen.log"; return 1; }
}
stop_engine() {
    # An engine can hang in its exit handler, and a new JACK server cannot
    # start while the old one lingers: ask, wait, then insist.
    pkill -x ingen 2>/dev/null; pkill -x jackd 2>/dev/null
    for _ in $(seq 30); do pgrep -x ingen >/dev/null || pgrep -x jackd >/dev/null || break; sleep 0.1; done
    pkill -9 -x ingen 2>/dev/null; pkill -9 -x jackd 2>/dev/null
    for _ in $(seq 30); do pgrep -x ingen >/dev/null || pgrep -x jackd >/dev/null || break; sleep 0.1; done
    # A killed server leaves its semaphores behind, and the next one trips on them.
    pgrep -x jackd >/dev/null || rm -f /dev/shm/jack_sem.* /dev/shm/jack-*
    sleep 0.5
    return 0
}
