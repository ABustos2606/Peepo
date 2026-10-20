# Peepo

Modular effects instrument for Raspberry Pi 5, with a new touchscreen interface inspired by Poly Effects' Beebo.

**Status, 19 October 2026:** The engine, touchscreen interface and Tascam audio path run on the Pi. After the [first catalogue purge](docs/36-catalogue-purge.md), the Pi export contains **328 identities**, with **272 ready to add**, and the selected inventory contains **350 identities**. Seven effects were retired; IR processing now uses the Cabinet and IR Reverb families with explicit channel configurations, while alternate convolvers remain hidden for older patches. The retained expansion has three Dragonfly reverbs and 58 additional Guitarix identities, bringing the selected Guitarix total to 101. The current patch was preserved and JACK and the engine kept running. NAM and IR effects have file selectors. The Windows editor, patch storage and simulated physical controls remain available; GPIO controls are not wired yet.

**[Complete module list: every module, category, description, connections and availability](docs/33-module-list.md).** Includes all 101 selected Guitarix identities, the three Dragonfly reverbs, disabled and hidden identities, and the selected modules not installed.

**[Patchstorage vs Peepo: all 386 LV2 entries, installed and missing](docs/34-patchstorage-peepo-comparison.md).** Checked 19 October 2026, with Pi64 download availability and bundle details.

**Project folder:** `C:\Pedal PI`

## Purpose

Create a standalone guitar effects instrument. The device should start directly into a usable patch, show readable modules connected by virtual cables on a 5-inch touchscreen, and support assignable knobs and stomp switches.

**The touchscreen interface is a new LVGL application, designed and written for this instrument — agreed.** It provides a simple drag-and-drop layout: each module is a square showing its name, tapping a module opens its internal parameters (drive, gain, volume, time, etc.), and modules are patched with virtual cables in series or parallel. Beebo's published interface is a reference for concepts only; no Beebo code, QML, or assets are used. The interface runs natively at 800 × 480 without a desktop environment for fast cold boot and low overhead.

The audio side reuses Poly's published engine (its Ingen fork) and LV2 modules, including its ports of Emilie Gillet's open-source eurorack modules (Peepo names them Mist, Twist, Weave, Bell, Dice, Lattice and Flow; see [module names](docs/37-module-names.md)), extended with compatible LV2 effects used by MODEP.

The existing MODEP interface is unsuitable for the user's 800 × 480 display. Opening MODEP automatically or hiding its desktop does not meet the interface requirement.

## Licensing

Original Peepo code and documentation are licensed under **GNU GPL version 3 or later** (`GPL-3.0-or-later`); see [LICENSE](LICENSE). Peepo is free software: you may redistribute and modify these files under those terms. It is provided without warranty, including any implied warranty of merchantability or fitness for a particular purpose.

Upstream reference sources and third-party assets retain their own licenses and copyright notices. In particular, Poly's Ingen engine is **AGPL-3.0-or-later**, and other Poly repositories include GPL, LGPL and permissive components. See [third-party notices](THIRD_PARTY_NOTICES.md) for the inspected evidence and implementation/release requirements.

## Run the Windows UI

The simulator uses actual LVGL at 800 × 480 and a simulated engine. It needs no Raspberry Pi, audio interface, or reference submodule initialization. Install Visual Studio C++ Build Tools and CMake; the supplied preset selects Visual Studio 2026 x64 (use `-G "Visual Studio 17 2022" -A x64` in a separate build directory for VS 2022).

```powershell
cmake --preset windows
cmake --build --preset windows --parallel 6
ctest --preset windows
& '.\build\Debug\peepo.exe'
```

The first configure fetches LVGL at a pinned commit. After that the cached build and application work offline. See [Windows UI development](docs/11-windows-ui-foundation.md) for interactions, architecture, checks and current limits.

To drive a real engine instead of the simulation, start Ingen under WSL and pass its address: `& '.\build\Debug\peepo.exe' --engine tcp://127.0.0.1:16180`. See [the adapter record](docs/15-ingen-adapter.md) for setup and limits. Without `--engine`, **+ Add** lists the demo modules followed by the exported plugins; in the simulation they can be patched and edited, but nothing is heard.

Drag empty space to pan; **Home** restores the original view. Hold a module for parameters, software bypass, duplication or deletion. Tap a cable to select it, then use **Delete cable** in the footer. These editor features work entirely with the simulated engine.

A module's page shows all its controls as bars, a dozen on one screen: drag one sideways to set it, tap a switch or a mode. The Tuner adds a live note, cents and flat/sharp meter above its A reference. The Add list offers one tuner and explains effect categories and setup requirements; see [effects and tuner](docs/25-effects-and-tuner.md). **Patches → Parallel demo** loads an editable split/merge example: Drive feeds Clouds and Delay, then both connect to OUT. Loading it guards unsaved edits. This demonstrates routing and UI behavior; audio summing still requires Pi verification.

Global BPM and CPU readouts stay in the bottom-right footer on every page. The simulator displays 120 BPM / 12% CPU SIM as fixed display fixtures; real tempo synchronization and processor measurements await engine integration. CPU is shown as `--` when the simulator is unavailable.

**Patches** saves patches to disk (`%LOCALAPPDATA%/Peepo/patches`, or `--patches DIR`) and provides **Save**, **Save as**, **Load**, **Startup**, **Delete**, **New patch** and **Rename**; see [patch storage](docs/17-patch-storage.md). Names can be entered with the on-screen keyboard; replacing unsaved edits has a cancel option. The catalogue distinguishes available, untested and unavailable demo entries; **Settings → Missing module** exercises placeholder recovery.

Keys stand in for the physical controls: **Q/A, W/S** turn encoders 1 and 2, **1, 2** push them, **Z, X, C** are the stomps. Encoder 1 moves through the page (hold it for Back), encoder 2 turns the highlighted value; **Settings → Controls** can pin them and assigns the stomps. See [physical controls](docs/18-physical-controls.md).

## Build for the Pi

Inside Debian under WSL2, as root: `bash tools/pi/build-arm64.sh` builds the engine, plugins and interface for arm64 under emulation, tests them and leaves a package in `build/pi/`. On the Pi, unpack it and run `sudo ./install.sh`. See [Linux build, Pi packaging and services](docs/19-linux-and-pi.md).

**Direct instrument boot is now verified on the Pi.** It powers up into Peepo and the saved Mist (Clouds) guitar patch; LightDM, MODEP and automatic console login are disabled/masked. The user confirmed the screen and touch after reboot. For a new package use `sudo ./install.sh --instrument`; for an installed Pi use `sudo /opt/peepo/bin/peepo-instrument-boot`. [Setup, results and restoration](docs/24-instrument-boot.md).

## Documentation

| File | Contents |
| --- | --- |
| [Complete module list](docs/33-module-list.md) | All 350 selected identities, grouped by touchscreen category, with descriptions, connections and availability; 272 ready to add |
| [Drizzle, Chamber, Slope and Reel](docs/38-eurorack-ports-roadmap.md) | Roadmap for four new plugins of our own, each in its own repository, ported from Beads, Elements, Stages and Frames |
| [Module names](docs/37-module-names.md) | Peepo's own names for the modules ported from Emilie Gillet's eurorack code, why they differ, and the credits |
| [All 43 GxPlugins](docs/32-gxplugins.md#all-43-gxplugins) | Every Gx effect by its Peepo name, category and description, plus build and native test results |
| [Requirements](docs/01-requirements.md) | Agreed behaviour, hardware baseline, scope, and success criteria |
| [Architecture](docs/02-architecture.md) | Audio engine, LVGL touchscreen application, controls, storage, and startup |
| [Touchscreen and physical controls](docs/03-interface-and-controls.md) | Drag-and-drop 800 × 480 interaction design, series/parallel routing, and control assignments |
| [Effects and plugin compatibility](docs/04-effects-and-compatibility.md) | Modules ported from Emilie Gillet's eurorack code, MODEP effects, and compatibility checks |
| [Implementation roadmap](docs/05-implementation-stages.md) | Phase overview, development order, dependencies, and evidence required to advance |
| [Validation](docs/06-validation.md) | Audio, interface, hardware, startup, and persistence checks |
| [Source audit and open decisions](docs/07-source-audit-and-decisions.md) | Inspected source versions, Beebo reference findings, decisions log, and unanswered questions |
| [Reference source collection](docs/08-reference-sources.md) | Download scope, repository versions, declared submodules, and Windows storage details |
| [Casette and NAM evaluation](docs/09-casette-and-nam.md) | Pinned Casette findings, lessons for this instrument, NAM feasibility, and proposed benchmark plan |
| [Detailed technical implementation plan](docs/10-technical-implementation-plan.md) | Phases 0–11 with numbered tasks, PC/Pi work split, deliverables, exit checks, and requirements coverage |
| [Windows UI development](docs/11-windows-ui-foundation.md) | Build/run instructions, shared UI and simulated engine, verification and remaining Phase 1–2 work |
| [Desktop editor review](docs/12-desktop-editor-review.md) | Recorded Phase 2 desktop acceptance, screenshots and interaction/domain checks |
| [Ingen engine evidence](docs/14-ingen-engine-evidence.md) | Engine built and measured on the PC under WSL2: protocol, routing, summing, reconnect, save/load, six defects and four patches |
| [Catalogue export](docs/16-catalogue-export.md) | Installed LV2 plugins exported to a catalogue file, every entry loaded in the live engine, sound measured through Mist (Clouds), and the overlay the project owns |
| [Ingen engine adapter](docs/15-ingen-adapter.md) | The interface's real engine backend: mapping, threading, synchronization, reconnection, live results and limits |
| [Domain and engine contracts](docs/13-domain-and-engine-contracts.md) | Identity, typed metadata, missing modules, mono/stereo routing, lifecycle, save and capability semantics |
| [Patch storage and connection states](docs/17-patch-storage.md) | Patches on disk with plugin state: what the engine does on save and load, revisions and recovery, property schema, assets, the patch manager and the connection banner |
| [Physical controls](docs/18-physical-controls.md) | The two encoders and three momentary footswitches: behaviour, mapping rules, assignments, simulator keys and a proposed GPIO wiring |
| [Linux build, Pi packaging and services](docs/19-linux-and-pi.md) | The interface on Linux (DRM/KMS, framebuffer, SDL), the arm64 build route under emulation, the package, installer and rollback, and the services that keep audio playing while the interface restarts |
| [Mist (Clouds)](docs/20-clouds.md) | The granular module's build record for x86-64 and arm64, its seven modes checked in Ingen with test signals, and the per-control overlay |
| [NAM in Ingen](docs/21-nam.md) | The two NAM plugins compared in the engine: model loading, worker and state, errors and the selected candidate |
| [Plugin catalogue](docs/22-plugin-catalogue.md) | The selected inventory, how the rest of Poly's and Beebo's plugins are built, and a compatibility record for every entry |
| [Pi bring-up](docs/23-pi-bring-up.md) | The Pi as found, and the first sound through the Peepo engine and interface on it |
| [Module page and two encoders](docs/31-module-page-and-two-encoders.md) | Every control as a bar, a dozen on one screen, set by dragging; two encoders (move, turn) instead of four |
| [Beebo manual gaps](docs/30-beebo-manual-gaps.md) | Features in the Beebo manual that Peepo lacked: looper, set list, MIDI, USB import/export, effects loops; Beebo's Delay, the Foot switch module and assigning stomps from the patch are done |
| [MIDI](docs/39-midi.md) | Plan: MIDI in and out over USB and 3.5 mm TRS jacks, MIDI cables and the MIDI modules on the canvas, clock in and out, program change and MIDI learn |

## Retrieve the project and reference sources

Project documentation is tracked in the main repository. The ordinary upstream references and recovered Autowaf reference are pinned Git submodules. The complete kernel source archive is tracked with Git LFS; its local bare Git object cache and incomplete `.part` download are not published.

With Git and Git LFS installed:

```powershell
git clone https://github.com/ABustos2606/Peepo.git
cd Peepo
git lfs install --local
git lfs pull
git submodule update --init
git -C reference/upstream/lenticular_lv2 submodule update --init --recursive
git -C reference/upstream/neural-amp-modeler-lv2 submodule update --init --recursive
git -C reference/upstream/tuna.lv2 submodule update --init --recursive
```

Initialize the top-level references first, then the three repositories with declared dependencies as shown. PolyMclkIn's unmapped `waflib` pointer is supplied separately at `reference/dependencies/autowaf`; its upstream repository is unchanged. See [the reference index](reference/README.md) and [manifest](reference/manifest.json) for exact revisions and Windows checkout limitations. This retrieves reference material, not a working instrument or a build environment.

## Reading the status labels

- **Agreed:** directly supported by the user's instructions.
- **Observed:** found in inspected upstream source; not evidence that it runs on this Pi.
- **Proposed:** a recommended design or test target awaiting review.
- **Unverified:** requires hardware inspection, a build, or a runtime test.

## Current facts

- Raspberry Pi 5 with 2 GB RAM.
- Patchbox OS on Debian 12 arm64, profiled on the Pi on 14 October 2026. Peepo starts directly into its touchscreen interface; MODEP and the desktop are masked ([bring-up](docs/23-pi-bring-up.md), [boot](docs/24-instrument-boot.md)).
- 5-inch HDMI touchscreen advertised as 800 × 480. The user confirms (13 October 2026) that it is capacitive with five touch points, touch and power over USB, video over HDMI, so it leaves the GPIO header free. The Pi and screen already run Zynthian and Patchbox OS with MODEP, on the official 27 W supply. The controller identity and any calibration still need checking on the Pi.
- The user has four 20-pulse rotary encoders with push switches and will use three momentary SPST footswitches (14 October 2026); since 17 October 2026 Peepo uses two of the encoders. Their wiring to the Pi is proposed in [physical controls](docs/18-physical-controls.md), not yet built.
- The audio interface is a Tascam US-100, running at 48 kHz, 128 frames and three periods. Guitar through Mist (Clouds) has been heard and confirmed on this path.
- Public Beebo-related source repositories are stored under `reference/`. Runtime builds fetch pinned sources separately; the complete installed collection and remaining gaps are in the [module list](docs/33-module-list.md).
- Casette is also downloaded under `reference/upstream/Casette` at commit `75ded6663e2d758bfe8a290952c672b2e8c01697`. It is a reference for LVGL, JACK, NAM, and preset handling; its fixed effects chain does not replace our modular engine.
- The current NeuralAudio-based NAM LV2 implementation is selected and has a model selector. Model loading and host/state behavior have been checked; musical listening and dense-patch performance budgets remain separate work ([NAM](docs/21-nam.md), [file handling](docs/29-parameters-files-tempo.md)).
- Beebo's interface repository (`digit_ui`) is **reference material only**. No code, QML, icons, fonts, or other assets from it will be copied into the new interface.

Original application code and CMake build files now exist under `src/` and `config/`. The simulator is a UI development foundation; it does not process audio. On 14 October 2026 the Pi was profiled and first played guitar through the Peepo engine and interface ([Pi bring-up](docs/23-pi-bring-up.md)); hardware measurements wait for an adequate power supply. Upstream reference repositories remain unchanged.

## Remaining validation

The Pi bring-up, native touchscreen, direct boot and first Mist (Clouds) guitar milestone are complete. Remaining work includes physical GPIO controls, broader guitar listening, model/IR listening and measured performance budgets for combined patches. Follow the [detailed phase plan](docs/10-technical-implementation-plan.md) and the per-module results linked from the [complete module list](docs/33-module-list.md).
