# Plugin catalogue: inventory, builds and compatibility records

**Looking for the module names? See the [complete module list](33-module-list.md):
all 357 selected identities, their touchscreen categories, descriptions,
connections and current availability.** This page records the build and test history.

**19 October 2026:** [66 retained Patchstorage identities](35-patchstorage-expansion.md)
are installed and tested on PC and Pi. Bollie Delay XT and Granulator were removed at the user’s request. The selected inventory contains 357
identities, 335 installed and 282 ready to add. The historical snapshots below
retain their original dates and counts.

**18 October 2026:** all 43 GxPlugins are now built, tested on PC and Pi, and
installed. The selected inventory has 291 identities. The original counts and
matrix below remain the 14 October snapshot; see [GxPlugins](32-gxplugins.md)
for the new source pins, results and activation record.

**Recorded:** 14 October 2026, on the Windows PC with the engine under WSL2 (Debian 13, x86-64). **Scope:** P9.1 (inventory), P9.2 (reproducible builds), P9.3 (compatibility records) and the presentation part of P9.4. **Verified against:** the patched Ingen of [the engine evidence](14-ingen-engine-evidence.md), JACK's dummy backend at 48 kHz and 128 frames. **Results:** [the compatibility matrix](evidence/compatibility-matrix.md), one row per entry, and [compatibility.json](evidence/compatibility.json), the full record. **Not covered:** the Pi, listening, processor load, MIDI routing and file selection.

**Every entry in the selected inventory has a result.** "Supported on the PC" means every host check passed on the PC engine. It becomes "supported" only when the same checks pass on the Pi.

| Result | Entries |
| --- | --- |
| Supported on the PC | 125 |
| Needs adaptation | 62 |
| Not tested (loads and matches, but the generic signal check could not judge it) | 39 |
| Unavailable | 23 |

## The inventory (P9.1)

The agreed collection is "Poly's Mutable-derived modules and compatible MODEP effects" (D04). Which MODEP effects are essential is still open (question 6 of the [decisions](07-source-audit-and-decisions.md)). Until it is answered, the selected inventory is:

- every plugin identity in **Beebo's module map** and **Poly's public plugin sources**: 207 entries from [the comparison data](lv2-plugin-comparison-data.json), the modular-pedal reference this project follows;
- the **MDA effects** and **LV2 examples** the development engine uses as conventional effects and test plugins: 42 entries.

`tools/catalogue-inventory.py` writes them to `config/catalogue/inventory.txt`. The exporter's new `--only` option offers just those plugins. Installing a package therefore no longer puts its other plugins in front of the player: Debian's x42 package alone holds dozens more. Patchstorage's 552 MODEP identities stay out until a choice is made. Any of them can be added to the inventory and checked with the same tools.

## Builds (P9.2)

`tools/ingen-spike/14-more-plugins.sh` builds everything below into `/opt/peepo`, beside the Mutable-derived modules of `02-plugins.sh` and the NAM candidates of `12-nam.sh` ([NAM in Ingen](21-nam.md)).

- Sources are cloned fresh at the commits shown; nothing under `reference/` changes.
- Each build fix is a variable or a build-tree copy given in the script, never an edit to upstream files.
- On the Pi each build swaps the SSE flags for `-mcpu=cortex-a76`. The arm64 status is at the end.

| Source | Commit | Plugins | Built with | Fix needed here |
| --- | --- | --- | --- | --- |
| Poly `PolyLV2` | `6f3ae80f` | 33 | `make` per directory | basic_modular's makefile strips one shared file from several rules, so it races under `-j`; built with one job |
| Poly `poly_filters` | `ebf8fd1c` | 6 | `make` | the makefile names `poly_filter.cpp`, the source is `poly_filters.cpp`; a copy is made in the build tree |
| Poly `polyconvo.lv2` | `fe8edd2d` | 6 | `make` | — |
| Poly `convoLV2` | `8ddddc69` | 3 | `make` | — |
| Poly `tuna.lv2` | `cfdfb6ae` | 3 | `make MOD=1`, no OpenGL | — |
| Poly `cv_to_cc.lv2` | `23152a8b` | 1 | `make` | — |
| Poly `digit_carla` (16 October 2026) | `75609d7d` | 1: Beebo's Delay | `make` in `LV2/digit_delay` | — (its Faust output is committed) |
| Peepo `plugins/` (16 October 2026) | this repository | 1: Foot switch | `gcc` | — |
| Poly `PolyMclkIn.lv2` | `64acb88d` | 1 | `gcc`, as its wscript | its waf needs a `waflib` the repository points at without mapping, and that autowaf revision imports Python's removed `imp`; one C file, compiled directly |
| Poly `avwlv2` | `bfb2e1f1` | 46 | `g++` per plugin, with its wscript's flags and lists | its bundled waf 1.7.11 cannot run on Python 3.13, and it needs LVTK 1 (not packaged); built against LVTK 1.2.0 (`9cc19eba`), without its GTK 2 interfaces |
| `moddevices/tap-lv2` | `cab6e0df` | 4 in the inventory | `make` | `NOOPT=true` on the Pi, which drops its SSE flags |
| `jpcima/stone-phaser` | `f057ea5c` | 2 | DPF | its Cairo interface is always built, and GCC 14 needs `-include cstdint` for it |
| `SpotlightKid/ykchorus` | `ee0e362f` | 1 | DPF, no interface | — |
| `DISTRHO/MVerb` | `5ae9f571` | 1 | DPF, no interface | — |
| `ssj71/infamousPlugins` | `8c0e11ec` | 3 in the inventory | CMake, no interfaces (NTK absent) | needs `libzita-resampler-dev` |
| `moddevices/mod-cv-plugins` | `f0a6b39a` | 1 in the inventory | `make` | installs to `/usr/lib/lv2` unless `LV2_DESTDIR` is given |
| `brummer10/GxPlugins.lv2` collection (expanded 18 October) | [43 per-plugin pins](../config/catalogue/gxplugins.json), preserving the four original revisions | 43 | `21-gxplugins.sh`, `make mod`, no interface | Explicit ARM flags; source archives and notices packaged. [Native results](32-gxplugins.md). |
| Debian 13 `x42-plugins` | 20240611+ds-1 | 39 in the inventory | package | — |
| Debian 13 `setbfree`, `blop-lv2`, `fomp`, `calf-plugins`, `mda-lv2`, `lv2-examples` | as installed | 2, 3, 1, 1, 36, 7 | packages | — |

Every component builds for x86-64. All binaries need only libraries the system provides. No required LV2 feature is outside what Ingen offers.

## Compatibility records (P9.3)

`tools/ingen-spike/15-compat.sh` does the following:

1. Exports the catalogue restricted to the inventory.
2. Loads every offered entry in the engine and compares it with the export (`peepo_catalogue_live`).
3. For every inventory entry, records:
   - its source and bundle;
   - its binary's machine and libraries;
   - its required features and extension data;
   - its port counts, MIDI ports and file parameters;
   - the export's verdict;
   - the engine check.
4. For each offered entry, it also:
   - feeds a 220 Hz sine into the first audio input (or the first CV input, for control modules);
   - measures the first output;
   - changes a control, saves the graph to a bundle, loads it back as a subgraph, and reads the control.

**What needs adaptation, and why.**

| Reason | Entries | What would change it |
| --- | --- | --- |
| MIDI only | 33 x42 MIDI filters | MIDI routing, deferred by D16 |
| Played by MIDI notes | 4 MDA instruments | MIDI routing |
| Takes a file | 12 convolvers (Poly, convoLV2, x42) and NAM | a file selector (P8.3); the host side is verified for NAM |
| Morph ports (control or CV, chosen by the host) | blop's DAHDSR and Product | the export reads them as controls while Ingen makes them CV. The exporter should follow the host's choice. |
| Upstream metadata does not parse | avwlv2 `vco2_audio`, `vco2_cv`, `scope` | their `.ttl` uses an undefined `param:` prefix; a metadata fix |
| Non-finite output | Poly's Moog ladder; avwlv2 Hz-to-VC | the Moog ladder produced NaN or infinity from a plain sine, which must not enter a graph. Hz-to-VC was fed a negative "frequency", outside its domain (inference). |
| Most controls inert | Clouds beat repeat | see [Clouds](20-clouds.md) |
| Nothing to patch | Poly tempo ratio, MOD CV meter, LV2 example params | their only connections are controls, MIDI or meters |

**Not tested** covers modules that load and match but cannot be judged by one sine. That includes:

- triggers, gates and clock dividers that wait for an edge;
- sequencers that wait for a clock;
- ring modulators and VCAs that multiply by a second input left at zero;
- envelopes that wait for a gate;
- MIDI-clock and CV-to-MIDI modules;
- the MDA vocoders and talk box, which need a modulator.

Each needs its own patch to judge. That is listening or control work, not a host problem.

**Plaits and Marbles, 16 October 2026.** Both were recorded as needs-adaptation ("engine check failed", with no reason given). Measured again, they work, on the PC and on the Pi ([results](evidence/plaits-marbles-results.txt)). Plaits sounds in all 17 models without a trigger. Marbles keeps its own clock (8 ticks in 4 s at 120), gives random gates and changing random voltages. Its T range defaults to the slowest (x0.25), as on Beebo, so it ticks at a quarter of its rate until that is raised. The compatibility files change at the next `15-compat.sh` run.

**Unavailable.**

- 16 entries: Beebo maps a Poly plugin with no public source (foot switches, loopers, MIDI-to-CV, detectors, a 3-band EQ, a bitcrusher, two phaser variants).
- 2: archived Digit plugins, not built. Since 16 October 2026 one of them, `digit_delay`, is built: it is Beebo's Delay (its module map names it). The other, `polylfo`, is a MIDI curve generator that Beebo does not offer; Beebo's LFO is AMS's tempo LFO, already in the catalogue. The Foot switch module stands in for Beebo's foot switches ([physical controls](18-physical-controls.md)). This record and its evidence files are from 14 October; they change at the next `15-compat.sh` run.
- 2: omins envelopes, LADSPA only in Debian.
- 3: x42's MIDI clock, MOD's 4×3 matrix mixer and a MIDI controller plugin, none of them packaged and two of them MIDI.

**Every saved control came back.** No entry failed the save and reload, and none needs a feature Ingen lacks.

**Engine and tool notes.**

- The live check runs without the overlay, so it tests every entry the export offers, including those the overlay withholds.
- Loading a whole-graph bundle back over a root graph that already has ports of the same names doubles those ports' properties. The interface clears the graph first, and the check loads into a subgraph instead.

## Presentation (P9.4)

The overlay (`config/catalogue/overlay.json`) now presents the whole offered catalogue:

- **Categories.** Every module has one, so the browser's groups mean something. Before, 147 exports fell under "Utility".
  - A new **Control** category, in yellow, collects envelopes, LFOs, sequencers, clocks, logic and random sources.
  - **Cabinet** (already in the browser's order) collects the cabinet convolvers.
- **Names.** Makers' prefixes are stripped (AMS, TAP, Gx, "the infamous"). Raw symbols such as `harm_trem_ext` become "Harmonic tremolo (CV)". Mutable modules use their module names (Plaits, Grids, Marbles, Tides).
- **Withheld with a reason:**
  - the convolvers and NAM, until a file can be chosen;
  - Clouds' beat repeat;
  - the Moog ladder;
  - duplicate tuner identities are hidden from Add while retained for saved patches; see [effects and tuner](25-effects-and-tuner.md);
  - redundant clean Boost (`basic_modular#boost`) is hidden from Add in favour of Gain (`eg-amp`), which covers -90 dB to +24 dB cleanly; its identity is retained for saved patches.

  The export may already offer them; withholding them is presentation, so the records above still show the host results.

### Output on every module (16 October 2026)

Every module that sends audio has exactly one **Output** control, because an effect that changes the level needs a way to set it back. It is chosen in one of two ways:

- **The plugin's own level control,** where the overlay names it with `"output": "<symbol>"`. It is shown with the name Output and keeps its own range and units. Hand-checked on 16 October 2026: MDA Ambience, Combo, Degrade, Delay, Detune, DubDelay, Dynamics, Image, Leslie, Limiter, Loudness, Overdrive, RezFilter, Shepard, Splitter, Tracker, Transient and Vocoder (`output`) and TestTone (`level`); the Guitarix Cream, SVT and Supersonic amps (`VOLUME`); NAM (`output_level`); TAP Sigmoid (`Postgain`); fil4 (`gain`); fomp ladder (`out_gain`); AMS Moog LPF and granulators (`outputGain`), mixers (`gain`) and VCAs (`outputLevel`); MVerb (`gain`); Calf Vinyl (`level_out`); polyconvo and convoLV2 (`gain`); the LV2 example amp (`gain`); and every Clouds mode (`output_param`, added by the project's plugin patch).
- **The general Output** for every other module with audio outputs: -24 to +12 dB, default 0 dB, shown last. It is not a plugin port: it is the block's `peepo:outputLevel` property, which engine patch 0008 applies to the block's audio outputs after the plugin runs, ramped across one period so changes do not click. It is saved with the patch, survives reload and duplication, and is not applied while the module is bypassed, so bypass stays exactly unity.

Controls that set a dry or wet level, a single band, or only the input are not outputs, so those modules get the general Output (TAP Pitch and Reflector, zeroconvolv, Bandisto, MultiBand, b_whirl, the Twist modes, the darc compressor, Boost). `"output": "none"` leaves a pass-through module without either; the tuners use it.

The general Output scales the sound after the plugin, so it cannot prevent clipping inside it; where a plugin has its own output control, that one acts inside the plugin. Checked end to end with `tools/ingen-spike/18-output-level.sh` against the patched engine: -6.02 dB halves the measured output (0.1250 for 0.2500 in), the value and the gain return after saving and loading the patch back from disk, a duplicate keeps it, and bypass passes 0.2500 unchanged. On the Pi, after installing the package with patch 0008, `tools/pi/check-output-level.sh` repeated the level, duplicate and bypass steps against the instrument's engine with the audio interface detached, with the same results; the save-and-reload step was checked on the PC only.

Clouds has per-control presentation ([Clouds](20-clouds.md)). The tuner has a dedicated note/cents display. Conventional effects now have explicit categories and descriptions even when Pi metadata classifies them as Utility. See [effects and tuner](25-effects-and-tuner.md).

## arm64

The arm64 builds are now complete and installed on the Pi ([Pi update](23-pi-bring-up.md#todays-package-brought-to-the-pi)). The Pi-generated catalogue contains 226 inventory entries. Only the six offered Clouds modes and the selected NAM plugin received native load/control checks in this update; the per-entry compatibility records above remain PC results.

The first arm64 ykchorus build lacked `GL/gl.h`: its top-level makefile compiles DGL's OpenGL library even though the finished LV2 bundle contains only DSP. Installing `libgl-dev` and `libglu1-mesa-dev` fixed the build; those headers are now explicit build dependencies. A failed component now makes the catalogue build fail instead of silently packaging a partial result.

## Run it

Inside Debian (WSL), as root, after `02-plugins.sh` and `12-nam.sh`:

```bash
bash tools/ingen-spike/14-more-plugins.sh   # build; about 10 minutes
bash tools/ingen-spike/15-compat.sh         # export, check and record; about 15 minutes
```

`15-compat.sh` replaces `config/catalogue/wsl-debian.json`, the catalogue the Windows build uses, and rewrites the matrix and the JSON record. `python3 tools/catalogue-inventory.py` regenerates the inventory from the comparison data.
