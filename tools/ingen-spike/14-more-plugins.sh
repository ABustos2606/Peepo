#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peepo contributors
#
# Build the rest of the catalogue inventory (P9.2) for the development engine:
# Poly's remaining plugin repositories at the commits pinned in
# reference/manifest.json, the third-party sources Beebo's module map refers
# to at recorded commits, and the Debian packages that carry the rest. Run as
# root inside Debian (or the arm64 root), after 02-plugins.sh:
#   bash 14-more-plugins.sh
# Every component is built on its own and logged in $SRC/more-plugins/; one
# that fails is reported and the rest carry on. Nothing under reference/ is
# touched: sources are cloned fresh, and any build fix is a make or cmake
# variable given here, never an edit to upstream files.
set -uo pipefail
HERE="$(dirname "$(readlink -f "$0")")"

PREFIX=/opt/peepo
SRC=$PREFIX/src/more-plugins
LV2DIR=$PREFIX/lib/lv2
JOBS=$(nproc)
mkdir -p "$SRC" "$LV2DIR"
# Some builds run in a child shell (bash -c), which needs these.
export PREFIX SRC LV2DIR JOBS

export DEBIAN_FRONTEND=noninteractive
# Stone-phaser always builds its interface. Ykchorus also compiles DGL's
# OpenGL library even when only its DSP bundle is produced.
BUILD_DEPS="build-essential pkg-config git cmake lv2-dev libzita-convolver-dev libzita-resampler-dev libsndfile1-dev libsamplerate0-dev libfftw3-dev gettext-base libcairo2-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libgl-dev libglu1-mesa-dev"
# Debian packages for third-party plugins Beebo maps: x42 (EQ, compressor,
# MIDI filters, convolver, tuner), setBfree's Whirl speaker, blop envelopes,
# fomp filters and calf's Vinyl. Not Debian's guitarix or omins: Beebo's
# guitarix amps are built below, and Debian's omins is LADSPA only.
PACKAGES="x42-plugins setbfree blop-lv2 fomp calf-plugins"
# shellcheck disable=SC2086
apt-get install -y -q --no-install-recommends $BUILD_DEPS $PACKAGES >"$SRC/apt.log" 2>&1 || { echo "apt-get failed:"; tail -5 "$SRC/apt.log"; exit 1; }

# The repositories' own flags target SSE; on the Pi 5 the same optimizations
# are kept and the SSE ones replaced by tuning for its Cortex-A76 cores.
if [ "$(uname -m)" = aarch64 ]; then
    OPT="-mcpu=cortex-a76 -ffast-math -fomit-frame-pointer -O3 -fno-finite-math-only -DNDEBUG"
else
    OPT="-msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer -O3 -fno-finite-math-only -DNDEBUG"
fi

RESULTS=()
record() { RESULTS+=("$(printf '%-30s %-10s %s' "$1" "$2" "$3")"); }

fetch() { # url dir commit [submodules]
    [ -d "$SRC/$2/.git" ] || git clone -q "$1" "$SRC/$2" || return 1
    git -C "$SRC/$2" fetch -q origin "$3" 2>/dev/null
    git -C "$SRC/$2" checkout -q -f "$3" || return 1
    git -C "$SRC/$2" clean -qfdx
    [ -z "${4:-}" ] || git -C "$SRC/$2" submodule update -q --init --recursive
}

# Runs a build step in a component's directory, logging it; returns its status.
step() { # name dir command...
    local name=$1 dir=$2; shift 2
    (cd "$dir" && "$@") >>"$SRC/$name.log" 2>&1
}

poly() { # repository commit
    fetch "https://github.com/polyeffects/$1.git" "$1" "$2" || { record "$1" FAILED "clone"; return; }
    rm -f "$SRC/$1.log"
}

# --- Poly: PolyLV2, one make project per directory
poly PolyLV2 6f3ae80f6b4eccb8daebfc2b641fde1d48224250
for dir in "$SRC"/PolyLV2/*/; do
    name=PolyLV2/$(basename "$dir")
    [ -f "$dir/Makefile" ] || continue
    # One job: basic_modular's makefile strips a shared file from several rules.
    if step "PolyLV2" "$dir" make OPTIMIZATIONS="$OPT" PREFIX="$PREFIX" LV2DIR="$LV2DIR" install; then
        record "$name" built "make"
    else
        record "$name" FAILED "$(grep -m1 -E 'error|Error' "$SRC/PolyLV2.log" | cut -c1-120)"
    fi
done

# --- Poly: filters
poly poly_filters ebf8fd1cea20e5d167f0c04ded12da13bd70829f
# The makefile builds poly_filter.cpp; the source is poly_filters.cpp. A copy
# in the build tree, as for lenticular's polyrings.ttl, leaves upstream as is.
cp "$SRC/poly_filters/poly_filters.cpp" "$SRC/poly_filters/poly_filter.cpp"
step poly_filters "$SRC/poly_filters" make -j"$JOBS" OPTIMIZATIONS="$OPT" PREFIX="$PREFIX" LV2DIR="$LV2DIR" install \
    && record poly_filters built make || record poly_filters FAILED "$(grep -m1 -E 'error' "$SRC/poly_filters.log" | cut -c1-120)"

# --- Poly: convolution (IR loaders)
poly polyconvo.lv2 fe8edd2da2e348ecc383c93c5d4e7961e7d8dc5b
step polyconvo.lv2 "$SRC/polyconvo.lv2" make -j"$JOBS" OPTIMIZATIONS="$OPT" PREFIX="$PREFIX" LV2DIR="$LV2DIR" install \
    && record polyconvo.lv2 built make || record polyconvo.lv2 FAILED "$(grep -m1 -E 'error' "$SRC/polyconvo.lv2.log" | cut -c1-120)"
poly convoLV2 8ddddc69e3a0a4dd1e0fa220144b8c343252409d
step convoLV2 "$SRC/convoLV2" make -j"$JOBS" OPTIMIZATIONS="$OPT" PREFIX="$PREFIX" LV2DIR="$LV2DIR" install \
    && record convoLV2 built make || record convoLV2 FAILED "$(grep -m1 -E 'error' "$SRC/convoLV2.log" | cut -c1-120)"

# --- Poly: tuner, the MOD variant Beebo maps (no OpenGL interface)
poly tuna.lv2 cfdfb6aeb065b278f58b6c6d4c23b5d82ae34279
TUNA_PATCH="$HERE/../plugin-patches/tuna-fast-fft-plan.patch"
if git -C "$SRC/tuna.lv2" apply --check "$TUNA_PATCH" 2>/dev/null; then
    git -C "$SRC/tuna.lv2" apply "$TUNA_PATCH"
else
    git -C "$SRC/tuna.lv2" apply --reverse --check "$TUNA_PATCH"
fi
step tuna.lv2 "$SRC/tuna.lv2" make -j"$JOBS" MOD=1 BUILDOPENGL=no BUILDJACKAPP=no OPTIMIZATIONS="$OPT" PREFIX="$PREFIX" LV2DIR="$LV2DIR" install \
    && record tuna.lv2 built "make MOD=1" || record tuna.lv2 FAILED "$(grep -m1 -E 'error' "$SRC/tuna.lv2.log" | cut -c1-120)"

# --- Poly: CV to MIDI, MIDI clock in
poly cv_to_cc.lv2 23152a8bf6dcdf2496df0d696dfd4c1a61f67e54
step cv_to_cc.lv2 "$SRC/cv_to_cc.lv2" make -j"$JOBS" OPTIMIZATIONS="$OPT" PREFIX="$PREFIX" LV2DIR="$LV2DIR" install \
    && record cv_to_cc.lv2 built make || record cv_to_cc.lv2 FAILED "$(grep -m1 -E 'error' "$SRC/cv_to_cc.lv2.log" | cut -c1-120)"

# --- Poly: Beebo's Delay, from the archived Digit application. Its Faust
# output is committed, so only the plugin directory is built.
poly digit_carla 75609d7dd365628fca044f64a7141686853e7945
step digit_carla "$SRC/digit_carla/LV2/digit_delay" make OPTIMIZATIONS="$OPT" PREFIX="$PREFIX" LV2DIR="$LV2DIR" install \
    && record digit_carla/digit_delay built make || record digit_carla/digit_delay FAILED "$(grep -m1 -E 'error' "$SRC/digit_carla.log" | cut -c1-120)"

# --- Peepo's own plugins, from plugins/ of this repository
peepo_plugins() {
    for bundle in "$HERE"/../../plugins/*.lv2; do
        local name; name=$(basename "$bundle" .lv2)
        mkdir -p "$LV2DIR/peepo-$name.lv2" &&
        gcc -shared -fPIC $OPT -std=c99 -Wall "$bundle/$name.c" -o "$LV2DIR/peepo-$name.lv2/$name.so" -lm &&
        cp "$bundle"/*.ttl "$LV2DIR/peepo-$name.lv2/" || return 1
    done
}
export -f peepo_plugins
export HERE OPT
rm -f "$SRC/peepo.log"
step peepo "$SRC" bash -c peepo_plugins && record peepo built "gcc" || record peepo FAILED "$(grep -m1 -E 'error' "$SRC/peepo.log" | cut -c1-120)"

# --- Poly: MIDI clock in. Its waf wants a waflib the repository points at
# without mapping (reference/README.md records it as autowaf c87cc53c), and
# that waflib imports Python's removed `imp`. The plugin is one C file, so it
# is compiled directly as its wscript describes.
poly PolyMclkIn.lv2 64acb88d6c574b4efc743b770d1d22444391ba02
mclk() {
    mkdir -p "$LV2DIR/poly_mclk_in.lv2" &&
    gcc -shared -fPIC $OPT mclk_in.c -o "$LV2DIR/poly_mclk_in.lv2/mclk_in.so" &&
    sed 's/@LIB_EXT@/.so/' manifest.ttl.in > "$LV2DIR/poly_mclk_in.lv2/manifest.ttl" &&
    cp mclk_in.ttl "$LV2DIR/poly_mclk_in.lv2/"
}
export -f mclk
export OPT
step PolyMclkIn.lv2 "$SRC/PolyMclkIn.lv2" bash -c mclk && record PolyMclkIn.lv2 built "gcc, as its wscript" \
    || record PolyMclkIn.lv2 FAILED "$(grep -m1 -E 'error' "$SRC/PolyMclkIn.lv2.log" | cut -c1-120)"

# --- Poly: avwlv2, the AMS modules. Its bundled waf 1.7.11 cannot run on this
# Python and its interfaces need GTK 2, so the plugins are compiled directly
# with the flags and source lists of its wscript, against LVTK 1.2.0 (not
# packaged). The GTK interfaces are not built.
poly avwlv2 bfb2e1f1851bdc6192e0401789a5d62045156d20
fetch https://github.com/lvtk/lvtk.git lvtk 9cc19eba92dd4e4cfd0e4cb78762fb202e11bbc2
# The sources include <lvtk-1/lvtk/...>, the installed layout.
mkdir -p "$SRC/lvtk-include" && ln -sfn "$SRC/lvtk" "$SRC/lvtk-include/lvtk-1"
avw() { # extra-source plugins...
    local extra=$1; shift
    for p in "$@"; do
        g++ -shared -fPIC $OPT -std=c++11 -I. -Isrc -I"$SRC/lvtk" -I"$SRC/lvtk-include" -DPLUGIN_CLASS="$p" \
            -DURI_PREFIX='"http://avwlv2.sourceforge.net/plugins/avw/"' -DPLUGIN_URI_SUFFIX="\"$p\"" -DPLUGIN_HEADER="\"src/$p.hpp\"" \
            "src/$p.cpp" $extra "$SRC/lvtk/src/plugin.cpp" -o "$LV2DIR/avw.lv2/$p.so" || return 1
    done
}
avw_all() {
    mkdir -p "$LV2DIR/avw.lv2" && cp avw.lv2/*.ttl "$LV2DIR/avw.lv2/" &&
    avw "" advenv amp env hztovc lfo_tempo lfo_freq mixer_4ch_audio mixer_4ch_cv mixer_2ch_audio mixer_2ch_cv noise2_audio noise2_cv \
        slew vcalin_audio vcalin_cv vctohz cvs delay absolute inv ringmodulator_audio ringmodulator_cv percussiveenv samplehold \
        vcpanning vcswitch_audio vcswitch_cv mooglpf downsampler_mono downsampler_stereo controltocv cvtocontrol amplitude scope &&
    avw src/envgen.cpp granulator_mono granulator_stereo beatrepeater_mono beatrepeater_stereo beatslicer_mono beatslicer_stereo &&
    avw src/synthdata.cpp vco2_audio vco2_cv ad vcf vcaexp_audio vcaexp_cv
}
export -f avw avw_all
export OPT SRC
step avwlv2 "$SRC/avwlv2" bash -c avw_all && record avwlv2 built "wscript's lists, LVTK 1.2.0, no GTK UIs" \
    || record avwlv2 FAILED "$(grep -m1 -E 'error' "$SRC/avwlv2.log" | cut -c1-120)"

# --- Third-party sources Beebo maps, at recorded commits
third() { # name url commit build-description command...
    local name=$1 url=$2 commit=$3 how=$4; shift 4
    rm -f "$SRC/$name.log"
    fetch "$url" "$name" "$commit" submodules || { record "$name" FAILED clone; return; }
    if step "$name" "$SRC/$name" "$@"; then record "$name" built "$how"; else record "$name" FAILED "$(grep -m1 -E 'error' "$SRC/$name.log" | cut -c1-120)"; fi
}
copy_bundles() { # DPF projects leave finished bundles in bin/
    cp -r bin/*.lv2 "$LV2DIR/"
}
# GCC 14 no longer brings <cstdint> in through other headers; stone-phaser's UI relied on it.
dpf_ui() { CXXFLAGS="-include cstdint" make -j"$JOBS" BUILD_DSSI=false BUILD_VST2=false BUILD_VST3=false BUILD_JACK=false BUILD_CLAP=false && copy_bundles; }
dpf() { make -j"$JOBS" HAVE_OPENGL=false HAVE_CAIRO=false HAVE_DGL=false BUILD_DSSI=false BUILD_VST2=false BUILD_VST3=false BUILD_JACK=false BUILD_CLAP=false NOOPT=false && copy_bundles; }
export -f dpf dpf_ui copy_bundles

# TAP adds SSE flags to its own unless NOOPT is set; on the Pi it is.
TAP_NOOPT=false; [ "$(uname -m)" = aarch64 ] && TAP_NOOPT=true
third tap-lv2 https://github.com/moddevices/tap-lv2.git cab6e0dfb2ce20e4ad34b067d1281ec0b193598a "make" \
    make -j"$JOBS" NOOPT="$TAP_NOOPT" INSTALL_PATH="$LV2DIR" install
third stone-phaser https://github.com/jpcima/stone-phaser.git f057ea5c1e368d6cd937f4e359a9514ac72fe902 "DPF, with its Cairo UI" bash -c dpf_ui
third ykchorus https://github.com/SpotlightKid/ykchorus.git ee0e362f8ef30b55a440715e4da3bfb3dad6a30f "DPF, no UI" bash -c dpf
# MVerb was retired from the curated instrument; do not build it again.
third infamousPlugins https://github.com/ssj71/infamousPlugins.git 8c0e11ecac624e014caba00e5cf4fb0ca2faa806 "cmake (no NTK, so no UIs)" \
    bash -c 'cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX='"$PREFIX"' -DLIBDIR=lib && cmake --build build -j'"$JOBS"' && cmake --install build'
# Full GxPlugins collection, including the four Beebo modules at their
# existing revisions. All identities and source pins share one manifest.
if bash "$HERE/21-gxplugins.sh" >"$SRC/GxPlugins.log" 2>&1; then
    record GxPlugins.lv2 built "43 plugins, make mod; source and notices included"
else
    record GxPlugins.lv2 FAILED "see $SRC/GxPlugins.log"
fi
third mod-cv-plugins https://github.com/moddevices/mod-cv-plugins.git f0a6b39a5ebb8cf2ab428d76aa8c8262aceb6362 "make" \
    make -j"$JOBS" LV2_DESTDIR="$LV2DIR" install

# --- Discipline (delay with a scannable memory, and its Return), the
# project's own. Its Makefile adds -fno-fast-math after OPT: its NaN guards
# need IEEE arithmetic, so it gets plain -O2 rather than the shared flags.
DISCIPLINE_OPT="-O2"; [ "$(uname -m)" = aarch64 ] && DISCIPLINE_OPT="-O2 -mcpu=cortex-a76"
third discipline https://github.com/ABustos2606/discipline.git 4f68f3b967f8fb92c4968554b44c131adfda3717 "make" \
    make -j"$JOBS" OPT="$DISCIPLINE_OPT" LV2DIR="$LV2DIR" install
# --- Loo-pi (multi-loop looper on SooperLooper's core), the project's own.
# Like Discipline it adds -fno-fast-math after OPT, so it gets the same flags.
third loo-pi https://github.com/ABustos2606/loo-pi.git 45c301f80c1ec63edeb99a45ae0998f01ac424fd "make" \
    make -j"$JOBS" OPT="$DISCIPLINE_OPT" LV2DIR="$LV2DIR" install

# Requested MODEP/Patchstorage effects, including the remaining main Guitarix
# LV2 collection. Artifacts, source rebuilds and metadata repairs are locked.
if bash "$HERE/22-patchstorage.sh" >"$SRC/Patchstorage.log" 2>&1; then
    record Patchstorage built "selected identities; pinned builds and Detune source rebuild"
else
    record Patchstorage FAILED "see $SRC/Patchstorage.log"
fi

echo "==== components"
printf '%s\n' "${RESULTS[@]}"
echo "==== packages"
for p in $PACKAGES; do dpkg-query -W -f='${Package} ${Version}\n' "$p" 2>/dev/null || echo "$p not installed"; done
echo "==== bundles now in $LV2DIR"
ls "$LV2DIR" | tr '\n' ' '; echo
# A partial catalogue must not silently become a release package.
for result in "${RESULTS[@]}"; do
    if [[ "$result" == *"FAILED"* ]]; then exit 1; fi
done
