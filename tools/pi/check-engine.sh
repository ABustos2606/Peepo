#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
#
# The engine's behaviour on the Pi, beside the running instrument: a second
# Ingen from /opt/peepo on a JACK server of its own (dummy driver, not real
# time), so the instrument, its audio interface and its patch are never
# touched. Runs the PC's engine probes against it:
#   04-verify.py        protocol, add/remove/connect/set, fan-out, fan-in
#                       summing, feedback, reconnects, second client (P3.5, P3.7)
#   10-bundle-probe.py  save/load while running, plugin state, paths with
#                       spaces, a missing plugin (P3.6, P5.2)
#   peepo_ingen_live    the interface's own adapter and patch store (P3.5)
#   13-nam.py           NAM models: message forms, worker, missing and corrupt
#                       files, switching, quality, state through a bundle,
#                       at 128, 64 and 1024 frames (P3.6, P8.2)
#   peepo_file_live     a model and impulse responses given through the
#                       interface's adapter: NAM and the convolvers offered,
#                       measured before and after, after a save and reload,
#                       and as a duplicate (P8.3)
# Run as root from a directory holding ingen-spike/ (the probes), models/
# (two .nam captures, A1 and A2) and irs/ (one cabinet .wav), for every
# section or only those named:
#   sudo bash check-engine.sh [protocol] [bundles] [adapter] [nam] [files]
set -uo pipefail
HERE="$(dirname "$(readlink -f "$0")")"
PREFIX=/opt/peepo
RUN=/tmp/peepo-check
SERVER=peepo-check
[ "$(id -u)" = 0 ] || { echo 'Run as root'; exit 1; }
sections=${*:-protocol bundles adapter nam files}
wanted() { [[ " $sections " == *" $1 "* ]]; }
python3 -c 'import jack, numpy' 2>/dev/null || env DEBIAN_FRONTEND=noninteractive apt-get install -y -qq python3-jack-client python3-numpy >/dev/null

export JACK_DEFAULT_SERVER=$SERVER JACK_NO_AUDIO_RESERVATION=1 PEEPO_INGEN_SOCKET=$RUN/ingen.sock
export LD_LIBRARY_PATH="$(find "$PREFIX/lib" -maxdepth 2 -name 'libingen*' -printf '%h\n' | sort -u | paste -sd:)"
# The instrument's plugins, with the selected NAM candidate (see peepo-engine).
export LV2_PATH="$(find "$PREFIX/lib" -maxdepth 2 -name lv2 -type d | paste -sd:):$PREFIX/nam/current:/usr/lib/lv2"
export PATH="$PREFIX/bin:$PATH"

jack_pid= engine_pid=
stop_check() {
    # Only the processes started here, never the instrument's.
    for pid in $engine_pid $jack_pid; do kill "$pid" 2>/dev/null; done
    for _ in $(seq 30); do { [ -n "$engine_pid" ] && kill -0 "$engine_pid" 2>/dev/null; } || { [ -n "$jack_pid" ] && kill -0 "$jack_pid" 2>/dev/null; } || break; sleep 0.1; done
    for pid in $engine_pid $jack_pid; do kill -9 "$pid" 2>/dev/null; done
    jack_pid= engine_pid=
    rm -f "$RUN/ingen.sock"
    sleep 0.5
}
start_check() {
    stop_check
    mkdir -p "$RUN"
    jackd -n $SERVER --no-realtime -d dummy -r 48000 -p "$1" >"$RUN/jackd.log" 2>&1 &
    jack_pid=$!
    for _ in $(seq 50); do jack_lsp >/dev/null 2>&1 && break; sleep 0.1; done
    ingen -e -f -a -E 0 -s $SERVER -S "$RUN/ingen.sock" >"$RUN/ingen.log" 2>&1 &
    engine_pid=$!
    export PEEPO_ENGINE_PID=$engine_pid
    for _ in $(seq 100); do [ -S "$RUN/ingen.sock" ] && break; sleep 0.1; done
    [ -S "$RUN/ingen.sock" ] || { echo "FAIL  the check engine did not open its socket"; tail -20 "$RUN/ingen.log"; return 1; }
}
trap stop_check EXIT

instrument() { for unit in peepo-jack peepo-engine peepo-ui; do printf '%s=%s ' "$unit" "$(systemctl show -p MainPID --value "$unit")"; done; }
before=$(instrument)
xruns_before=$(journalctl -u peepo-jack -b --no-pager 2>/dev/null | grep -c 'XRun')
echo "INSTRUMENT $before"
echo "SYSTEM $(uname -m) $(. /etc/os-release; echo "$PRETTY_NAME") jack $(jackd --version 2>&1 | head -1 | awk '{print $3}') throttled=$(vcgencmd get_throttled 2>/dev/null | cut -d= -f2)"
status=0

if wanted protocol; then
    echo "==== protocol, routing and reconnects (04-verify.py)"
    start_check 128 || exit 1
    mkdir -p /tmp/peepo-spike
    timeout 180 python3 "$HERE/ingen-spike/04-verify.py" || status=1
fi
if wanted bundles; then
    echo "==== bundles, plugin state and paths (10-bundle-probe.py)"
    start_check 128 || exit 1
    # Its last step leaves a client that never reads, which stalls the engine; it is stopped afterwards.
    timeout 90 python3 "$HERE/ingen-spike/10-bundle-probe.py" "$RUN/bundles" || status=1
fi
if wanted adapter; then
    echo "==== the interface's adapter and patch store (peepo_ingen_live)"
    start_check 128 || exit 1
    mkdir -p "$RUN/patches"
    PEEPO_PATCHES="$RUN/patches" timeout 300 "$PREFIX/libexec/peepo/peepo_ingen_live" "unix://$RUN/ingen.sock" "$PREFIX/share/peepo/catalogue.json" > "$RUN/adapter.log" 2>&1
    grep -E "^(FAIL|SUMMARY)" "$RUN/adapter.log"
    echo "$(grep -c '^PASS' "$RUN/adapter.log") adapter checks passed"
    grep -q "SUMMARY all adapter checks passed" "$RUN/adapter.log" || status=1
fi

A1="$HERE/models/a1.nam" A2="$HERE/models/a2.nam"
for model in "$A1" "$A2"; do
    python3 -c 'import json,sys,hashlib; p=sys.argv[1]; d=json.load(open(p)); print("MODEL %s: %s %s, %s Hz, sha256 %s" % (p.rsplit("/",1)[-1], d.get("architecture"), d.get("version"), d.get("sample_rate"), hashlib.sha256(open(p,"rb").read()).hexdigest()[:16]))' "$model"
done
wanted nam && for period in 128 64 1024; do
    echo "==== NAM at $period frames (13-nam.py)"
    start_check "$period" || exit 1
    if [ "$period" = 128 ]; then
        timeout 600 python3 "$HERE/ingen-spike/13-nam.py" current "$A1" "$A2" "$RUN/nam" || status=1
        grep -E "rror|nable|arning" "$RUN/ingen.log" | grep -v "Duplicate plugin\|found in file\|and      file" | sort | uniq -c | head -12
        # The saved patch, into an engine that has never seen the model.
        start_check "$period" || exit 1
        timeout 120 python3 "$HERE/ingen-spike/13-nam.py" --restore current "$RUN/nam/nam.ingen" || status=1
    else
        timeout 300 python3 "$HERE/ingen-spike/13-nam.py" current "$A1" "$A2" "$RUN/nam-$period" --quick || status=1
    fi
    kill -0 "$engine_pid" 2>/dev/null && echo "PASS  the check engine is still running" || { echo "FAIL  the check engine did not survive"; status=1; }
done
# Noise through IN (left), and what comes out of OUT (left): its level and
# its spectrum in eight bands, to compare one setting with another.
measure() {
    python3 - <<'PY'
import time, jack, numpy
rng = numpy.random.default_rng(7)
c = jack.Client("file-probe"); out = c.outports.register("noise"); cap = c.inports.register("cap"); got = []
def process(frames):
    out.get_array()[:] = rng.uniform(-0.1, 0.1, frames).astype(numpy.float32)
    got.append(cap.get_array().copy())
c.set_process_callback(process); c.activate()
c.connect("file-probe:noise", "ingen:in_l"); c.connect("ingen:out_l", "file-probe:cap")
time.sleep(1.0); start = len(got); time.sleep(1.5)
x = numpy.concatenate(got[start:]); c.deactivate(); c.close()
rms = float(numpy.sqrt(numpy.mean(x * x)))
spectrum = numpy.abs(numpy.fft.rfft(x[:65536] if len(x) >= 65536 else x)) ** 2
bands = [float(b.sum()) for b in numpy.array_split(spectrum[1:], 8)]
total = sum(bands) or 1.0
print("%.6f %s %d" % (rms, ",".join("%.4f" % (b / total) for b in bands), int(numpy.isfinite(x).all())))
PY
}
# Whether two measurements differ: over 1 dB in level, or a reshaped spectrum.
differ() {
    python3 - "$1" "$2" <<'PY'
import math, sys
a, b = sys.argv[1].split(), sys.argv[2].split()
la, lb = float(a[0]), float(b[0])
level = abs(20 * math.log10((la + 1e-9) / (lb + 1e-9)))
shape = sum(abs(float(x) - float(y)) for x, y in zip(a[1].split(","), b[1].split(",")))
print("%.1f dB apart, spectrum %.2f apart" % (level, shape))
sys.exit(0 if level > 1 or shape > 0.1 else 1)
PY
}
if wanted files; then
    LIVE="$PREFIX/libexec/peepo/peepo_file_live"
    CATALOGUE="$PREFIX/share/peepo/catalogue.json" OVERLAY="$PREFIX/share/peepo/catalogue-overlay.json"
    FILES=$RUN/files
    rm -rf "$FILES" "$RUN/file-patches"; mkdir -p "$FILES/nam" "$FILES/ir" "$RUN/file-patches"
    cp "$HERE/models/a1.nam" "$FILES/nam/A1 standard.nam"
    cp "$HERE/irs/cab.wav" "$FILES/ir/Cab 4x12.wav"
    for entry in "NAM|http://github.com/mikeoliphant/neural-amp-modeler-lv2|$FILES/nam/A1 standard.nam" \
                 "convoLV2|http://gareus.org/oss/lv2/convoLV2#Mono|$FILES/ir/Cab 4x12.wav" \
\
                 "polyconvo|http://polyeffects.com/lv2/polyconvo#CabMono|$FILES/ir/Cab 4x12.wav"; do
        IFS='|' read -r name uri file <<<"$entry"
        result=
        echo "==== $name: $uri"
        start_check 128 || exit 1
        live() { timeout 120 "$LIVE" "unix://$RUN/ingen.sock" "$CATALOGUE" "$OVERLAY" "$@" | grep -E "^(PASS|FAIL)"; [ "${PIPESTATUS[0]}" = 0 ] || status=1; }
        live setup "$uri"
        without=$(measure)
        live file "$file"
        sleep 2
        with=$(measure)
        if [ "${with##* }" = 1 ] && result=$(differ "$without" "$with"); then echo "PASS  $name: the file changes the sound -- $result"
        else echo "FAIL  $name: the file did not change the sound -- ${result:-not finite}"; status=1; fi
        live reload "$RUN/file-patches"
        sleep 2
        reloaded=$(measure)
        if ! result=$(differ "$with" "$reloaded"); then echo "PASS  $name: the reloaded patch sounds the same -- $result"
        else echo "FAIL  $name: the reloaded patch sounds different -- $result"; status=1; fi
        live duplicate
        sleep 2
        copied=$(measure)
        if ! result=$(differ "$with" "$copied"); then echo "PASS  $name: the duplicate sounds the same -- $result"
        else echo "FAIL  $name: the duplicate sounds different -- $result"; status=1; fi
        kill -0 "$engine_pid" 2>/dev/null && echo "PASS  $name: the check engine is still running" || { echo "FAIL  $name: the check engine did not survive"; status=1; }
    done
fi
stop_check

after=$(instrument)
xruns_after=$(journalctl -u peepo-jack -b --no-pager 2>/dev/null | grep -c 'XRun')
if [ "$before" = "$after" ]; then echo "PASS  the instrument's JACK, engine and interface kept their processes ($after)"
else echo "FAIL  an instrument process changed: before $before, after $after"; status=1; fi
echo "NOTE  the instrument's JACK logged $((xruns_after - xruns_before)) xrun lines while this ran"
exit $status
