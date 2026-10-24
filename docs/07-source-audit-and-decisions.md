# Source audit and open decisions

## Audit baseline

The following public repositories were first downloaded into the earlier chat workspace for source inspection. Their working trees were clean when checked. The project now also contains a larger unchanged reference collection under `reference/`, including these repositories. No build or deployment has been performed. See [Reference sources](08-reference-sources.md) for the current download inventory and manifest.

| Repository | Inspected commit |
| --- | --- |
| [digit_ui](https://github.com/polyeffects/digit_ui/tree/feb4df451067850770ddd13af9f4e2828234dabc) | `feb4df451067850770ddd13af9f4e2828234dabc` |
| [ingen](https://github.com/polyeffects/ingen/tree/8dae740425f0f1468198e0cdf1e1c61b3fc6ec5e) | `8dae740425f0f1468198e0cdf1e1c61b3fc6ec5e` |
| [lenticular_lv2](https://github.com/polyeffects/lenticular_lv2/tree/141fdc89e250987bae421664132ddaedae735107) | `141fdc89e250987bae421664132ddaedae735107` |
| [PolyLV2](https://github.com/polyeffects/PolyLV2/tree/6f3ae80f6b4eccb8daebfc2b641fde1d48224250) | `6f3ae80f6b4eccb8daebfc2b641fde1d48224250` |
| [poly_filters](https://github.com/polyeffects/poly_filters/tree/ebf8fd1cea20e5d167f0c04ded12da13bd70829f) | `ebf8fd1cea20e5d167f0c04ded12da13bd70829f` |
| [Casette](https://github.com/pawelKapl/Casette/tree/75ded6663e2d758bfe8a290952c672b2e8c01697) — additional reference, downloaded 13 October 2026 | `75ded6663e2d758bfe8a290952c672b2e8c01697` |

These revisions establish what was originally inspected, not the final compatible build set. Submodules were not initialized during that initial inspection; the project reference collection now includes the declared recursive submodule revisions, recorded in its manifest.

## Beebo interface findings — reference only

The new interface is written from scratch (decision D01). These observations from `digit_ui` explain what Beebo assumed and what the new interface must handle in its own way. They are no longer adaptation tasks.

| Area | Source evidence | Lesson for the new interface |
| --- | --- | --- |
| UI technology | `digit_ui/README.md`, PySide2 imports in `show_widget.py` | Beebo uses Qt 5/PySide2 on X11; our toolkit is chosen independently (D12) and should not need a desktop session |
| Screen geometry | 27 QML files reference `1280`, including `TopLevelWindow.qml`, `TitleFooter.qml`, `PatchBay.qml` | Beebo is designed for 1280 × 720; design natively for 800 × 480 |
| Custom controls | About 60 QML files import `"controls"`; that directory is absent in the inspected checkout | Beebo's UI would not launch unchanged; not relevant to a new interface |
| Hardware controls | `peepo_hardware.py` | Fixed device paths and two-encoder model; design our own hardware adapter |
| Hardware bypass | `peepo_hardware.py` | Original GPIO numbers do not describe Raspberry Pi wiring; bypass is out of scope without suitable hardware |
| Input gain | `show_widget.py:set_input_level` | Beebo-specific codec commands; use the chosen audio interface's own controls |
| Persistent paths | `show_widget.py`, `ingen_wrapper.py`, asset-browser modules | Define our own storage layout |
| Maintenance actions | Firmware/reset/import routines in `show_widget.py` | Not carried over; design Pi equivalents only if needed |
| MIDI preset handling | `show_widget.py:midi_pc_thread` | Original waits for `ttymidi`; choose our own controller transport |
| Engine integration | `ingen.py`, `ingen_wrapper.py` | Shows which Ingen messages Beebo uses; a useful guide for our engine client |
| Editor restart | `digit_ui/README.md` | Ingen does not cope with Beebo's frontend restarting; our interface must reconnect and resynchronize (R13) |
| Mutable module build | `lenticular_lv2/Makefile` and `.gitmodules` | Still relevant: ARM compiler options, submodules, and build recipes need checking |
| Module metadata | `module_info.py`, `effect_proto_to_js.py` | Hand-maintained definitions; our catalogue is generated from LV2 metadata plus our own overlay |

## Casette and NAM findings — additional reference

The unchanged Casette checkout contains all 93 tracked files, including bundled `.nam` models and `.wav` IRs, and is clean at the recorded commit. It has no tracked Git submodule entries or `.gitmodules`; its ignored `deps/` folder is absent. The published project therefore does not pin or supply a complete dependency set. No build, deployment script, service, or bundled asset has been executed or installed.

Its source uses LVGL framebuffer/evdev in one executable with a custom JACK callback and fixed effects chain. Both NAM instances execute in that callback, even in the parallel blend mode. Its optional second-model quality reduction and approximate process CPU meter are not evidence of guaranteed spare processing capacity. The model-loading functions replace raw pointers without deleting previous objects; repeated switching warrants memory-ownership tests in our selected implementation. The build targets Cortex-A72 with hardcoded Windows sysroot paths; Pi 5 Cortex-A76 settings need separate validation.

The local Poly NAM fork is an older NAM Core integration. Current upstream plugin/NeuralAudio documentation was inspected online on 13 October 2026 and describes an optimized A1/A2-capable runtime. These online sources are not local dependency snapshots or a selected build. See [the detailed Casette/NAM audit](09-casette-and-nam.md) for source links, limitations, and proposed evaluation.

## Licensing observations

- `digit_ui` has no licence file at the inspected revision; its README states that code without a notice is GPL. The new interface does not copy its code or assets.
- `ingen.py` in `digit_ui` carries David Robillard's ISC-style permission notice.
- Poly's Ingen fork is AGPL-3.0-or-later, as stated in its source headers; `COPYING` contains the AGPLv3 text. The new interface communicates with it as a separate program over its socket protocol. Changes to Ingen retain that license.
- `PolyLV2/COPYING` contains GPLv2, while generated source sections also carry GPLv3-or-later, LGPLv3-or-later and exceptions. Do not assign one license to every file based only on the root license text.
- `lenticular_lv2/LICENSE.txt` contains GPLv2; `LICENSE-dist.txt` includes separate upstream notices, including the MIT notice for Mutable Instruments STM32F modules.
- The Poly NAM LV2 fork includes GPLv3 text in `LICENCE.md`; dependency and file notices remain authoritative for their respective parts.
- The pinned Casette snapshot has no root license file or license notice in its inspected README and C++ files. It remains a technical reference; copying its code or bundled model/IR assets requires establishing the applicable permissions.

**Project license — agreed 13 October 2026:** original Peepo code and documentation use GPL-3.0-or-later. The root [LICENSE](../LICENSE) contains the complete GPLv3 text. [Third-party notices](../THIRD_PARTY_NOTICES.md) define its scope and record the inspected upstream evidence. The repository license does not relicense reference sources, dependencies, models, IRs, fonts or other third-party assets. Phase 11 includes a license/source inventory for the actual release components.

## Engine dependency observations

The inspected Ingen `meson.build` requests the following minimum versions, in addition to other dependencies and build tools:

| Library | Minimum in inspected source |
| --- | --- |
| Serd | 0.30.4 |
| Sord | 0.16.15 |
| Sratom | 0.6.0 |
| Suil | 0.10.0 |
| LV2 | 1.18.0 |
| Lilv | 0.24.21 |
| Raul | 2.0.0, using the `raul-2` package identity |

This is a source-level dependency observation. The Pi has since been identified as Debian 12 bookworm arm64. The compatible prefix build and actual Pi installation are verified; see D06 and the implementation plan. The minimum-version table remains source-audit evidence.

## Decisions

The [technical implementation plan](10-technical-implementation-plan.md), prepared 13 October 2026, provides the authoritative proposed phase/task sequence. Its Windows simulator workflow uses shared UI code and a simulated engine, with early Pi audio validation. Source inspection of Ingen's `src/Serialiser.cpp` confirms bundle serialization using `main.ttl` and `manifest.ttl`; bundle save and reload were then verified at runtime ([evidence](14-ingen-engine-evidence.md)); plugin state was subsequently checked on PC, and the Clouds graph/control bundle now saves and reloads on Pi; native file-based assets remain open. The companion JSON design was dropped on 13 October 2026 (D13).

| ID | Topic | State |
| --- | --- | --- |
| D01 | Build a new touchscreen interface; Beebo is reference/inspiration only; MODEP is not the main interface | Agreed (revised 13 October 2026) |
| D02 | Use the existing Raspberry Pi 5, 2 GB as starting hardware | Agreed |
| D03 | Design for the user's 5-inch 800 × 480 screen | Agreed and verified on Pi, 14 October 2026: HDMI-A-1 at 800 × 480, QDtech MPI5001 USB touch, native DRM interface with correct touch positioning ([record](23-pi-bring-up.md)) |
| D04 | Include Poly's Mutable Instruments-derived modules and compatible MODEP effects | Agreed ambition; individual compatibility unverified |
| D05 | Documentation before implementation | Agreed |
| D06 | OS/dependency baseline | Current baseline verified on Pi: Patchbox OS / Debian 12 bookworm arm64; matching bookworm build root with lilv 0.24.24 and zix 0.4.2 in /opt/peepo. The rebuilt package runs there; a later final OS decision remains separate ([record](23-pi-bring-up.md)) |
| D07 | Audio interface, channels, sample rate and buffers | Current development path verified: Tascam US-100 / CODEC, stereo at 48 kHz / 128 frames × 3 periods. Guitar and short UI-restart continuity are confirmed; final interface choice, gain/latency and buffer budgets remain open. A built-in I²S alternative (PCM1808 as clock master + PCM5102A + guitar buffer) is assessed but not yet built ([record](27-i2s-audio-interface.md)) |
| D08 | Knob/switch counts and transport | Counts agreed 14 October 2026: four push encoders the user owns, three momentary SPST footswitches. Transport proposed: direct GPIO through the kernel's rotary-encoder and gpio-key drivers, to verify on the Pi ([physical controls](18-physical-controls.md)) |
| D09 | Numeric cold-boot and latency targets | Open; measure first |
| D10 | Storage layout, recovery, and shutdown design | Open |
| D11 | Four encoders and three stomp switches | Agreed 14 October 2026, matching the user's hardware |
| D12 | UI toolkit and programming language | Agreed: LVGL (C/C++ on Linux DRM/KMS or framebuffer with evdev touch) |
| D13 | Storage of UI data (positions, assignments) | Agreed, revised 13 October 2026: stored as properties on blocks, ports and the graph inside the engine and its saved bundle. The earlier companion JSON file is dropped |
| D14 | Visual interaction and identity | Agreed interaction: drag-and-drop square modules, tap to enter parameter page, series/parallel audio routing; name agreed as **Peepo** on 14 October 2026; visual styling open |
| D15 | Keep Poly's Ingen fork as the engine | Current direction; the engine client layer keeps substitution possible |
| D16 | Routing cable types | Agreed, revised 13 October 2026: audio cables and control (CV) cables, each patched connector by connector. Audio and control are not joined to each other in the interface. Revised 20 October 2026: MIDI cables too, MIDI to MIDI only, with IN's MIDI output and OUT's MIDI input as the engine's `midi_in` and `midi_out` ([MIDI](39-midi.md), P12.4) |
| D17 | Retain Casette as another unchanged technical reference and update project documentation | Agreed 13 October 2026; downloaded and recorded in the reference manifest |
| D18 | Evaluate current NeuralAudio-based NAM against the Poly fork | Comparison complete on PC; selected current implementation pinned, built for arm64 and loaded on Pi without a model. Pi model/worker/state acceptance, selectors and budgets remain P8.2–P8.7 ([record](21-nam.md)) |
| D19 | Automatic NAM quality reduction under load | Open; no automatic reduction policy agreed |
| D20 | Shared Windows 800 × 480 LVGL simulator and Pi UI workflow | Implemented: pinned LVGL v9.5.0, C++20/CMake, Windows and Linux DRM/evdev backends; native Pi screen/touch and UI-only restart verified. Debugger session remains P1.1 |
| D21 | Detailed phase/task implementation plan | Implementation under way since 13 October 2026. Reconciled 14 October with actual task checkboxes, native evidence, partial checks and next actions; the plan is the authoritative progress record ([plan](10-technical-implementation-plan.md)) |
| D23 | Engine build and protocol | Verified working baseline: upstream Ingen 13a045d0 + Poly MIDI-learn fix + project patches 0001–0005, Turtle protocol. ARM build and Pi guitar/Clouds/save/load/UI-reconnect checks pass; remaining native host/routing cases stay P3.4–P3.7 ([record](23-pi-bring-up.md)) |
| D22 | Project license | Agreed 13 October 2026: GPL-3.0-or-later for original Peepo files; preserve upstream licenses, including Ingen's AGPL-3.0-or-later |
| D24 | Scenes | Agreed 24 October 2026: up to 8 scenes per patch; a scene stores every module's bypass plus only the parameters marked per scene (Helix-style), the rest shared; changes sent as one atomic bundle; delays and reverbs ring on when bypassed (engine spill bypass for third-party modules, Trails on by default in our own). Planned as Phase 13 ([scenes](44-scenes.md)) |

## Remaining information and resolved implementation inputs

1. Intended guitar/output connections and requirements for selecting a suitable audio interface: USB, or the built-in I²S converters assessed in [I²S audio interface](27-i2s-audio-interface.md).
2. Resolved for the current baseline: Debian 12 bookworm arm64, kernel and service inventory recorded in [Pi bring-up](23-pi-bring-up.md).
3. Screen controller and actual pixel mode. The user confirms a capacitive panel with five touch points, touch and power over USB and video over HDMI, working under Zynthian and Patchbox/MODEP.
4. Answered 14 October 2026: four 20-pulse push encoders and three momentary SPST footswitches. Still open: the enclosure arrangement (its size depends mainly on the audio interface and power design; see [I²S audio interface](27-i2s-audio-interface.md)), and the encoders' detent count on the bench.
5. Cooling, storage, and whether a spare card is available for development. The power supply is the official 27 W unit.
6. Which conventional effects are essential in the first usable patch, alongside Clouds.
7. Resolved: SSH access to the Pi is configured and has been used for deployment and native tests.
8. Look-and-feel preferences, colour theme, or branding references for the instrument's visual styling (when ready).
9. Which NAM captures and representative patches to benchmark; desired quality and simultaneous model count remain open rather than guaranteed features.

## Supporting references

- [Beebo module integration guide at inspected revision](https://github.com/polyeffects/digit_ui/blob/feb4df451067850770ddd13af9f4e2828234dabc/add-module.md)
- [Beebo module definitions at inspected revision](https://github.com/polyeffects/digit_ui/blob/feb4df451067850770ddd13af9f4e2828234dabc/module_info.py) — reference only
- [Mutable Instruments-derived build at inspected revision](https://github.com/polyeffects/lenticular_lv2/blob/141fdc89e250987bae421664132ddaedae735107/Makefile)
- [Ingen build dependencies at inspected revision](https://github.com/polyeffects/ingen/blob/8dae740425f0f1468198e0cdf1e1c61b3fc6ec5e/meson.build)
- [Blokas MODEP setup](https://blokas.io/modep/docs/setup/)
- [Blokas MODEP interface and plugin location](https://blokas.io/modep/docs/running-modep/)
- [User's display purchase link](https://www.mercadolibre.com.ar/pantalla-tactil-raspberry-pi-lcd-touch-5-800-x-480-hdmi/p/MLA2071447608?pdp_filters=item_id:MLA864457610&sid=purchases)
- [Casette at downloaded revision](https://github.com/pawelKapl/Casette/tree/75ded6663e2d758bfe8a290952c672b2e8c01697)
- [Current upstream NAM LV2 documentation](https://github.com/mikeoliphant/neural-amp-modeler-lv2/blob/main/README.md) — online reference; pin before building
- [NeuralAudio documentation](https://github.com/mikeoliphant/NeuralAudio/blob/main/README.md) — online reference; pin before building

Source links establish provenance. They do not replace target-hardware testing or resolve the open decisions above.
