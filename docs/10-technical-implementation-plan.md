# Technical implementation plan

**Project:** Peepo
**Prepared:** 13 October 2026 (America/Buenos_Aires)  
**Reconciled with implementation evidence:** 14 October 2026

**Current state:** the instrument runs on the Pi with its native touchscreen interface and Tascam US-100. The user has heard guitar through Clouds, saved and reloaded that patch, and confirmed no interruption during an interface-only restart. It now boots directly into Peepo and the saved Clouds patch, with desktop/MODEP disabled; a software reboot and physical display/touch confirmation passed. These are completed implementation-plan results. The full controlled milestone still needs its conventional-effect acceptance and physical knob/stomp assignments; offline/repeated cold-boot, wider recovery and release validation remain unfinished. [Direct boot evidence](24-instrument-boot.md).

The deployed arm64 package includes Clouds, the selected NAM implementation and the buildable selected plugin inventory. The catalogue was generated on the Pi itself: 226 entries. All eight arm64 build tests passed. Six offered Clouds modes passed native load/control checks; NAM passed a native check without a model. During guitar playing, 8,251 monitored blocks over 22 seconds had no gaps, xruns or silent outputs while only the UI restarted. The saved graph and controls survived unchanged, and the user reported "No interruption". [Deployment evidence](evidence/pi-update-results.txt), [persistence/restart evidence](evidence/pi-persistence-results.txt), [hardware record](23-pi-bring-up.md).

**Task-status rules:** a checked task has evidence for its stated implementation scope, with the platform identified in its phase record. PC implementation checks do not certify all plugins or workflows on the Pi. Partially completed tasks remain unchecked with completed and remaining subchecks below. The card-backup requirement has an explicit user waiver under P0.6. Historical execution records preserve what was known at their date; the reconciled status and current task notes supersede their old "not done" statements.

**Display target:** 800 × 480.  
**Hardware baseline:** Raspberry Pi 5, 2 GB RAM; Windows PC for everyday interface development.

This plan turns the [requirements](01-requirements.md), [architecture](02-architecture.md), [interaction design](03-interface-and-controls.md), and [validation rules](06-validation.md) into executable phases. It preserves the new LVGL interface, Poly Ingen audio engine, Mutable Instruments-derived modules, and audio plus control (CV) routing (D16, revised 13 October 2026). Layout and control assignments are stored as engine properties inside the patch bundle; the earlier companion JSON was dropped on 13 October 2026 (D13). Casette is an unchanged technical reference. Independent Windows UI implementation was authorized on 13 October 2026; see [its development record](11-windows-ui-foundation.md). The current package is installed in the isolated Pi prefix; system settings and the patch store are preserved.

Every phase has numbered tasks, a deliverable, and an exit check. A phase is complete only when its evidence exists; a desktop prototype does not establish Pi audio performance or finger usability. Technical choices marked proposed are defaults to evaluate, not new agreed product requirements.

## Development order and checkpoints

| Phase | Main result | Work location | Depends on |
| --- | --- | --- | --- |
| 0 | Hardware/OS profile, backup and audio baseline | PC and Pi | Access to the Pi and selected audio hardware for audio measurements |
| 1 | Shared application skeleton and LVGL simulator | Windows PC | Can start immediately; no Pi dependency |
| 2 | Interactive patch editor using a simulated engine | Windows PC | Phase 1 |
| 3 | Minimal working JACK/Ingen audio path and protocol evidence | Linux build environment and Pi | Phase 0; can overlap Phases 1–2 |
| 4 | Real engine integration and native Pi touch UI | PC and Pi | Phases 2 and 3 |
| 5 | Reliable patch/state/asset persistence | PC and Pi | Phase 4 |
| 6 | Clouds plus conventional effect, edited on the instrument | Pi, with PC UI development | Phases 4 and 5 |
| 7 | Assignable physical controls and first full milestone | PC and Pi/controller hardware | Phases 4–6 and identified controls |
| 8 | NAM/cabinet evaluation and supported resource budgets | PC and Pi | Phases 4 and 5; may start earlier than Phase 7 completion |
| 9 | Tested module catalogue and remaining interface workflows | PC and Pi | Phases 5–7; NAM results from Phase 8 for NAM entries |
| 10 | Automatic startup, readiness and recovery | Pi | Working representative patch from Phases 5–7 |
| 11 | Complete validation and reproducible release package | PC and Pi | Phases 0–10 for the selected release scope |
| 12 | MIDI: devices, MIDI cables and modules, clock, program change and learn | PC and Pi, USB-MIDI devices, later the enclosure's jacks | Phases 4 and 5; the set list for program change; Phase 11 again for a release that includes it |

Phases 1–2 do not wait for the Pi. Phase 3 must pass before building the full interface around assumed engine behaviour. NAM and catalogue expansion do not block the first Clouds instrument milestone. Work can proceed on independent tasks without completing every phase strictly in numeric order.

- **Checkpoint A — desktop editor:** passed on the PC.
- **First audible musical milestone:** achieved on the Pi: guitar through Clouds, saved/reloaded, with uninterrupted audio during UI restart.
- **Checkpoint B — real audio controlled through our UI:** its audio/native-screen/reconnect core is verified; the complete Phase 4 touch-workflow checklist remains P4.7.
- **Checkpoint C — first complete controlled musical milestone:** still open: add one conventional effect and validate one physical knob and one stomp, alongside the saved Clouds patch.
- **Checkpoint D — candidate dedicated instrument:** basic direct boot and JACK/engine recovery verified; full offline/cold-boot/storage/recovery acceptance remains open.
- **Checkpoint E — validated release:** open; release acceptance is not complete.

## Current phase status and next work

| Phase | Current result | Remaining before phase completion |
| --- | --- | --- |
| 0 | System/services/display profiled; controls identified; backup waived explicitly | Complete audio/gain/buffer profile and baseline measurements (P0.3, P0.7) |
| 1 | Shared Windows/Linux application, pinned LVGL and arm64 builds complete | Recorded debugger breakpoint session (P1.1) |
| 2 | Desktop editor acceptance complete | Pi-specific acceptance belongs to Phases 4 and 9 |
| 3 | Compatible engine/dependency build, isolated Pi run and reconnect while audio continues; protocol, host features and fan-out/fan-in repeated on the Pi (16 October) | Overdrive listening and converter latency (P3.4); mono-to-stereo through a real plugin and the merge policy (P3.7) |
| 4 | Native 800 x 480 display/touch, Pi catalogue, real adapter, UI-only restart and deployment loop working | Complete physical-screen add/edit/remove/invalid-cable/merge workflow (P4.7) |
| 5 | Storage implemented; actual Clouds patch and two revisions saved/reloaded on Pi | Native file-based state/assets, physical assignments and full patch-manager acceptance; power loss stays in P10.5 |
| 6 | First audible Clouds milestone achieved; patch saved/reloaded; UI restart preserves guitar audio | Full mode/control listening sweep, Clouds plus conventional effect, combined-load measurements (P6.4–P6.6) |
| 7 | Controls identified; simulation, mappings and assignment UI implemented | Select/verify wiring, connect hardware and validate actions/resilience (P7.1, P7.3, P7.6, P7.7) |
| 8 | NAM candidates pinned and built for arm64; model/worker/state/error/block-size checks pass on the Pi (16 October) | Selectors, IRs and performance budgets (P8.3–P8.7) |
| 9 | All buildable selected families packaged; catalogue exported on Pi; PC compatibility matrix exists | Native per-entry compatibility, remaining presentation/screens and physical usability/load (P9.3–P9.6) |
| 10 | Direct boot into saved Clouds verified by reboot and user touch confirmation; independent services, automatic JACK/engine recovery and native UI-crash recovery verified | Full readiness/error cases, offline/five cold boots, missing assets/plugins, shutdown/storage and wider recovery (P10.2–P10.7) |
| 11 | Development package installed; current evidence linked to requirements below | Complete acceptance matrix, endurance/lifecycle, release provenance/source package and operation guide |
| 12 | Planned 20 October 2026 ([MIDI](39-midi.md)); one merged MIDI IN decided; MIDI cables built and checked on the PC and in WSL (P12.4); clock out built and measured in WSL (P12.3) | P12.1, P12.2, P12.3 on the Pi, P12.5–P12.11; P12.4 on the Pi with the next package (its install exports the version 2 catalogue) |

**User-directed execution, 14 October 2026:** direct instrument boot was prioritized and completed: enable Peepo at boot, mask the desktop/MODEP/automatic-console competitors, select saved Clouds, preserve the newer unsaved wendy patch, reboot, verify restoration/routing and confirm display/touch. Automatic JACK/engine recovery also passed. [Record](24-instrument-boot.md). Phase 10's full acceptance remains open. The remaining conventional-effect and touch workflow checks (P3.4, P4.7), Clouds-plus-conventional-effect persistence (P6.5), P6.4 listening and physical controls remain the subsequent work. The observed power/throttling issue must be resolved and rechecked before latency or sustained load budgets are accepted (P0.3, P0.7, P6.6). NAM and catalogue expansion remain separate work.

## Technical structure

Selected language/build baseline: C++20, CMake and pinned LVGL v9.5.0, built for Windows and Debian bookworm arm64 and running on the Pi. Use one pinned LVGL revision for Windows and Pi builds. Do not choose moving upstream defaults as a reproducible dependency specification.

| Component | Responsibility | Key boundary |
| --- | --- | --- |
| Application/domain core | Module identities, port descriptions, connections, typed values, pending edits, assignments | No LVGL, JACK or GPIO dependency |
| Engine adapter | Graph commands, snapshots, updates, errors and connection state | One interface implemented by simulated and Ingen adapters |
| LVGL views | Canvas, parameters, browser, patches, assignments and settings | Consumes application state; LVGL operations stay on the UI thread |
| Platform layer | Display, input, clock, paths and process integration | Windows simulator versus Linux DRM/framebuffer and touch |
| Catalogue | Versioned LV2 metadata export plus project-owned presentation overlay | Linux discovery is independent of the Windows simulator |
| Persistence | Ingen graph/state bundle carrying layout and assignments as properties, asset references and committed revisions | Uses verified host serialization; never reconstructs plugin state from sliders alone |
| Controls | Device events, debounce, mapping, ranges and action dispatch | No blocking reads or persistence on an audio callback |
| Diagnostics | Engine readiness, DSP/xruns, memory and thermal observations | Labels real measurements separately from simulated values |

Proposed future workspace layout: `src/core`, `src/ui`, `src/engine`, `src/platform/windows`, `src/platform/linux`, `src/catalogue`, `src/storage`, `src/controls`, `tools`, `tests`, `assets`, and `config`. Build outputs go in a separate ignored build directory. Upstream snapshots remain unchanged under `reference/`; implementation dependencies and patches are recorded separately.

The engine is the authority for live DSP state. The UI model is its synchronized representation plus editor-only metadata. Pending commands must not appear as accepted engine changes until acknowledged or observed in authoritative updates. This clarifies the architecture's single-source-of-truth rule for views without making the interface responsible for keeping audio alive.

## Phase 0 — identify and preserve the Pi

**Purpose:** establish the actual system before choosing build dependencies or touching the existing installation.

**Reconciled status — 14 October 2026:** P0.1, P0.2 and P0.4 are complete on the Pi. P0.5 has the owned control types/counts and the unresolved connection recorded; wiring and bench validation belong to Phase 7. P0.6 is complete with the user-approved card-backup waiver, isolated prefix and documented recovery. P0.3 and P0.7 remain partial/open for the measurements below. [Evidence](23-pi-bring-up.md).

**Historical execution record — 14 October 2026:** P0.1, P0.2 and P0.4 are recorded, and P0.3 has identified the interface (Tascam US-100, ALSA `CODEC`) but not yet measured it. P0.6: the user waived a backup because the card holds only earlier experiments; the isolated prefix is `/opt/peepo`, and MODEP is left installed. The control inventory is now complete under P0.5; wiring remains Phase 7 and the unmeasured baseline remains P0.7. The Pi runs Debian 12 bookworm arm64, matching the build root, so D06's working baseline stands. See [Pi bring-up](23-pi-bring-up.md).

- [x] **P0.1 — System profile:** record board/RAM, OS release, userland architecture, kernel, storage/free space, power supply, cooling, CPU governor, network and available remote access.
- [x] **P0.2 — Service inventory:** record active JACK/MODEP/PipeWire/PulseAudio services where present, current audio ownership, startup configuration and installed LV2 paths. Identify potential conflicts without changing them during inventory.
- [ ] **P0.3 — Audio profile:** identify the selected interface, channels, guitar input suitability, device identity, gain controls, sample rates and usable period/buffer configurations. If the interface is not selected, record selection criteria and leave audio tests blocked by that specific dependency.
  - [x] Tascam US-100 / CODEC, stereo channels and working 48 kHz / 128 x 3 guitar path identified.
  - [ ] Complete gain/headroom/noise and alternative usable buffer/round-trip latency checks.
- [x] **P0.4 — Display/input profile:** confirm actual 800 × 480 output, DRM/framebuffer availability, touchscreen USB/input identity, calibration, orientation and single-touch operation.
- [x] **P0.5 — Controls profile:** identify the owned knobs as encoders or potentiometers, their count and connection; identify stomp-switch hardware and transport options. Record unresolved wiring rather than assigning arbitrary GPIO pins.
- [x] **P0.6 — Preservation:** make a recoverable backup or development SD card, record restoration steps, and choose an isolated development prefix/plugin path. Preserve the working MODEP installation until replacement passes validation.
  - [x] User explicitly waived a full-card backup because the card held earlier experiments.
  - [x] Isolated prefix, previous-program rollback and the route back to MODEP are recorded.
- [ ] **P0.7 — Baseline:** measure current power-on-to-audio and power-on-to-screen separately; record latency, xruns, memory and temperature for an available representative patch.
  - [x] Initial idle/service observations and the short guitar/UI-restart continuity run are recorded.
  - [ ] Measure power-on-to-audio/screen and representative latency/load/thermal baseline with adequate power.

**Deliverable:** hardware/OS profile, existing service/plugin inventory, baseline measurement record and recovery procedure.

**Exit check:** Linux architecture and device identities are known; development changes have a recovery path. Missing audio/control hardware is explicitly recorded. Phase 1 can proceed even if those items remain unresolved.

## Phase 1 — shared code and Windows simulator

**Purpose:** make everyday UI work possible on the PC without the Pi.

**Reconciled status — 14 October 2026:** P1.3 includes the working native Pi DRM/touch application and the Linux/arm64 test results. The Linux build route is established. Only the debugger-session evidence remains under P1.1; this phase is not the project starting point any longer.

**Historical execution record — 14 October 2026:** P1.3 is complete. The same shared code now builds for Linux, with DRM/KMS, framebuffer, SDL and offscreen displays. All its tests pass natively under WSL2 and on arm64 under emulation. See [Linux and Pi](19-linux-and-pi.md). P1.1's debugger validation remains.

**Historical execution record — 13 October 2026:** P1.2 and P1.4–P1.8 are complete. The Windows Debug build and all three CTest checks passed. Domain and engine contracts, missing-module scenarios, and build/run/debug instructions are documented. VS Code launch/task configuration is prepared; its C/C++ debugger extension is not installed on this PC, so an actual breakpoint session remains part of P1.1. Linux UI platform work was subsequently completed under P1.3. See [UI development](11-windows-ui-foundation.md) and [domain/engine contracts](13-domain-and-engine-contracts.md).

**P1.4 correction:** a user-reported blank native window exposed a startup refresh race missed by the original launch-only check. The initial redraw now waits for the Windows buffer, and the native test verifies rendered pixels and a completed flush without requiring a click. See the [startup fix and regression evidence](11-windows-ui-foundation.md#verification).

| Task | Implemented | Remaining before completion |
| --- | --- | --- |
| P1.1 | Windows toolchain/preset, VS Code setup and compatible Linux/arm64 route | Actual debugger breakpoint validation |
| P1.3 | Complete: shared C++20/CMake core/views, Windows and working native Pi/Linux platforms; notices retained | None for the application skeleton; later acceptance stays in its owning phase |
| P1.5 | Complete: stable identity/display-name separation, explicit port types/channel symbols, tagged numeric values, validated ranges/graphs and retained missing-module metadata | Real plugin discovery/adaptation belongs to Phases 3–4 |
| P1.6 | Complete: abstract Engine, connection lifecycle, correlated results/snapshots, create/rename/load/session-save and capability failures | Actual protocol and persistent plugin state belong to Phases 4–5 |
| P1.7 | Complete: deterministic catalogue, delayed/rejected edits/save, missing-module graph fixture and disconnect/reconnect | No audio processing is claimed |
| P1.8 | Complete: build/run/test/debug instructions, VS Code configurations, pinned dependency and reviewed captures | Debugger installation/runtime validation remains P1.1 |

- [ ] **P1.1 — Toolchain inventory:** identify available Windows C++ compiler, CMake, debugger and dependency tooling. Select a supported combination and document setup; choose a Linux build route for later Pi deployment separately.
  - [x] Windows toolchain and reproducible Debian bookworm arm64 Linux route selected.
  - [ ] Record an actual debugger breakpoint session.
- [x] **P1.2 — Pin graphics dependencies:** select a stable LVGL revision and compatible Windows display/input backend. Prefer the official Win32 simulator when it fits the available compiler; SDL is a documented alternative. Record revisions, licenses and required fonts/assets; preserve supplied notices.
- [x] **P1.3 — Application skeleton:** create the proposed shared/platform separation and build targets for a Windows simulator and later Linux UI. Keep simulator drivers out of the shared application logic. Add GPL-3.0-or-later SPDX and accurate copyright notices to original project source files; keep adapted upstream files under their applicable terms.
- [x] **P1.4 — Display baseline:** run a real LVGL application with an 800 × 480 logical drawing area, mouse pointer input and keyboard input. Account for Windows DPI scaling; optional zoom changes presentation, not logical coordinates.
- [x] **P1.5 — Domain contracts:** define stable instance IDs, plugin URIs, port symbols/types/directions, parameter value types/ranges/units, connection records, canvas positions and assignment records. Separate engine identity from user display names.
- [x] **P1.6 — Engine contract:** define connect/disconnect, snapshot, add/remove module, connect/disconnect ports, set parameter, load/save and state/error events. Define pending/success/failure semantics and unsupported-capability reporting before coding protocol details.
- [x] **P1.7 — Simulated engine:** provide a small deterministic catalogue, graph state, delayed responses, rejected commands, missing modules and disconnect/reconnect scenarios. Simulated graphs do not process audio, and simulated resource values are labelled as such.
- [x] **P1.8 — Developer loop:** document reproducible build/run/debug steps and capture the initial screenshot. Pin dependencies so an ordinary rebuild does not silently upgrade LVGL.

**Deliverable:** runnable Windows LVGL simulator, shared domain model, engine interface, simulated backend and setup instructions.

**Exit check:** launch repeatedly on the PC at 800 × 480 without the Pi or internet, exercise a command/update exchange, and switch between ready/disconnected/error states.

**Evidence sources:** [official Windows simulator](https://github.com/lvgl/lv_port_pc_visual_studio), [official SDL driver documentation](https://docs.lvgl.io/master/integration/pc/sdl.html). Exact selected versions are recorded during execution rather than inferred from these moving documentation links.

## Phase 2 — build the patch editor on the PC

**Purpose:** validate the core interaction before audio integration expands its scope.

**Historical execution record — 13 October 2026:** P2.1–P2.8 and the desktop exit check are complete. Actual LVGL pointer/keypad checks cover creation/naming/rename, cancellation and unsaved-replacement guards, pagination, catalogue availability, missing-module repair, all core editor workflows and rejected/pending operations. Sparse, panned, dense, long-name, keyboard and error views are reviewed at 800 × 480; domain tests cover all mono/stereo expansion rules. Logical cables retain physical routing plans using demo or supplied channel symbols; actual Ingen ports and merge levels remain unverified. See the [desktop review checklist](12-desktop-editor-review.md). Physical switch assignment remains Phase 7; Pi touch acceptance remains Phase 4.

| Task | Implemented | Remaining before completion |
| --- | --- | --- |
| P2.2 | Complete: named squares, movement threshold, snapping, 44-pixel jack hit areas, empty-space panning and long-press actions (parameters, software bypass, duplicate, delete) | Physical control assignment is tracked in Phase 7 |
| P2.3 | Complete: preview, valid targets, selection/delete, split/merge drawing, errors and physical connection plans | Verify actual Ingen port identities in Phases 3–4 |
| P2.4 | Complete: direction/type/channel, fixed endpoint, duplicate and cycle validation; four mono/stereo rules | Actual merge levels/compensation remain Phase 3 measurements |
| P2.5 | Complete: paginated categorized catalogue with available/untested/unavailable status and reasons | Actual installed/tested plugins remain later integration |
| P2.6 | Complete: tagged controls, logarithmic/integer ranges, units, software bypass and parameter pagination | Physical touch review remains Phase 4 |
| P2.7 | Complete: new/rename/edit/duplicate/delete, cleanup, pending/rejected commands, session save/restore and backend updates | Disk/plugin-state persistence remains Phase 5 |
| P2.8 | Complete: recorded sparse/dense/long-name/edge/keyboard/error review and meaningful channel/range checks | Pi usability and audio acceptance remain unverified |

- [x] **P2.1 — Canvas and header:** fixed IN/OUT modules, patch name, dirty marker, readiness state, add-module, patches and settings navigation. Begin with the sizing targets in the interaction document and use a shared theme.
- [x] **P2.2 — Module interaction:** named square modules, grid snapping, tap-to-open parameters, movement threshold, drag positioning, empty-space panning and context actions. Keep port hit areas large enough for the intended screen.
- [x] **P2.3 — Cable interaction:** output-to-input drag preview, valid-target indication, cable selection/removal, series split/merge display and clear rejected-connection messages. Represent each logical cable's eventual physical engine connections.
- [x] **P2.4 — Graph validation:** enforce port direction/type compatibility, fixed endpoint rules and rejection of feedback cycles. Implement mono/stereo expansion as explicit rules; leave merge compensation provisional until Phase 3 measurements.
- [x] **P2.5 — Module browser:** categories, names and availability status from the simulated catalogue. Keep unavailable or untested modules distinguishable from supported modules.
- [x] **P2.6 — Generic parameters:** continuous/logarithmic values, integers, toggles, enumerations and units; paginate crowded pages. Prevent out-of-range values and distinguish plugin bypass from a hardware bypass claim.
- [x] **P2.7 — Simulated workflows:** create a patch, edit it, duplicate/delete a module, remove related cables, and exercise pending/rejected operations. Include graph changes arriving from the backend rather than only from local gestures.
- [x] **P2.8 — Desktop review:** inspect sparse and dense patches, long names, edge gestures, keyboard overlays and error messages at 800 × 480. Exercise meaningful domain tests for graph validation, channel expansion and range conversion.
- [x] **P2.9 — Module-specific UI and parallel starter (user-requested extension, 13 October 2026):** Clouds Texture / Mix-hold pages and Delay Character controls, shared generic fallback for changed metadata, and an editable parallel demo with guarded replacement. Pointer checks cover custom edits, mode rejection, fallback value continuity, branch delete/reconnect, and cancelled/rejected demo loading; all three desktop tests pass. Review captures are recorded in the [desktop checklist](12-desktop-editor-review.md). Real audio summing remains Phase 3.
- [x] **P2.10 — Global tempo and CPU readouts (user-requested extension, 13 October 2026):** bottom-right footer on every page, with space retained for cable actions. Shared engine telemetry provides optional BPM and CPU percent; the simulator supplies explicit display fixtures (120 BPM / 12% CPU SIM) and clears CPU when disconnected/connecting/in error. Unknown or invalid readings show `--`. Desktop rendering and existing interaction checks pass; real tempo synchronization and CPU measurement remain engine/Pi integration work. **16 October 2026:** the BPM is now the patch tempo, stored in the engine and saved with the patch, and opens a Tempo page; modules with a BPM control follow it ([record](29-parameters-files-tempo.md#patch-tempo)).

- [x] **P2.11 - Reference metadata, multi-port jacks and live controls (13 October 2026):** catalogue fixture generated from the unchanged Mutable-derived `.ttl` files (the Windows part of P4.4), several audio jacks per side with port-aware cables, and continuous parameters streamed during the gesture with newest-value coalescing (the UI half of P4.2). Domain and pointer checks cover real stereo symbols, a two-input module, CV-only refusal and mid-gesture engine updates. Host behaviour is still unverified.

- [x] **P2.12 - Free patching and control cables (user-requested, 13 October 2026):** every audio port is its own connector with one cable per channel; control (CV) outputs and inputs are patched from a square connector above and below each module, with the ports chosen from a list. Domain and pointer checks cover left and right through different effects, control to control, and refusal of audio joined to control. Engine behaviour for control cables is read from source only, not yet run.

**Deliverable:** interactive desktop editor using actual shared LVGL code and a recorded review checklist.

**Exit check:** complete add → drag → connect → edit → disconnect → delete without invalid graph state or clipped essential controls. Backend rejection must revert/resolve pending UI state. Finger usability remains unverified until Phase 4.

## Phase 3 — prove JACK, Ingen and the protocol on the Pi

**Purpose:** verify the engine assumptions while the desktop editor is still small.

**Reconciled status — 14 October 2026:** P3.1–P3.3 are complete: the Pi is Debian 12 bookworm arm64, matching the emulated build root; the rebuilt binaries and libraries run in the isolated prefix with the real Tascam at 48 kHz, 128 frames x 3 periods. P3.8 is verified by client/UI reconnection during guitar playing without an audio gap. Ingen remains the selected host (P3.9), with known module limitations recorded. P3.4–P3.7 retain the specific native checks below. The current package includes project patches 0001–0005. [Build/deployment](evidence/pi-update-results.txt), [reconnection/audio evidence](evidence/pi-persistence-results.txt).

**Historical execution record - 14 October 2026 (PC only):** the build route of P3.2 is established and the dependency baseline of P3.1 is chosen for Debian 12 (bookworm), the base of Raspberry Pi OS bookworm.

- **Build route.** One script builds the engine, plugins and interface, runs the tests and packs them. It runs natively on the Pi, or on the PC in a Debian arm64 root under qemu, where the engine, plugins and interface were built and tested.
- **Baseline.** Debian 12's lilv is too old for this Ingen, so lilv 0.24.24 and zix 0.4.2 are built into the prefix.
- **Plugins.** The Mutable-derived plugins are built with Cortex-A76 flags instead of SSE.

Both items are now complete: the Pi is confirmed as Debian 12 arm64, and the matching bookworm package runs there. See the reconciled record above and [Linux and Pi](19-linux-and-pi.md).

**Historical execution record - 13 October 2026 (PC only):** the hardware-independent parts of P3.1 and P3.5-P3.8, and the bundle format of P5.2, were verified on x86-64 Debian under WSL2 with JACK's dummy backend; see [Ingen engine evidence](14-ingen-engine-evidence.md). Poly's published `main` does not compile (it needs an unpublished serd), so the engine is upstream Ingen plus Poly's MIDI-learn fix and three project patches. The reconciled checklist below identifies which tasks now have native Pi evidence and which checks still need repeating there.

- [x] **P3.1 — Build baseline:** select Poly Ingen and dependency revisions compatible with the Phase 0 OS. Check the inspected Serd/Sord/Sratom/Suil/LV2/Lilv/Raul requirements; choose build dependencies or an alternative supported development image from evidence.
- [x] **P3.2 — Linux builds:** establish a native Pi or reproducible Linux cross-build route with correct architecture and sysroot. Do not transplant Casette's Windows paths, Cortex-A72 target or unpinned dependency folder.
- [x] **P3.3 — Isolated engine run:** launch JACK and Ingen with explicit device/plugin paths in the development setup. Avoid competing MODEP graph ownership and record every configuration change and its rollback.
- [ ] **P3.4 — Simple effect:** build/load one compatible low-cost LV2 effect and confirm guitar input → effect → output, gain behaviour and explicit JACK port routing. Record audio settings and observed xruns/latency.
  - [x] Real guitar input/output and stereo Clouds processing work with explicit JACK routes.
  - [x] MDA Overdrive's gain measured on the Pi with a test tone kept off the speakers: unity at neutral, output -20/+20 dB, muffle -14.4 dB at 1 kHz, level-dependent drive gain (light +8.1 to +1.2 dB). JACK's nominal round trip is 10.67 ms (128 + 384 frames); 2 xruns while the graph was rebuilt and controls changed, none in 22 s of steady playback. [Results](evidence/pi-effect-results.md).
  - [ ] Listen to the guitar through Overdrive; measure converter round-trip latency with a loopback cable and try other buffer sizes once the power supply is fixed.
- [x] **P3.5 — Protocol proof:** verify socket address, connection lifecycle, Turtle/Atom message structure, framing, request/status correlation, subscriptions and full graph retrieval. Capture sanitized examples for add/remove/connect/disconnect/set/read operations.
  - [x] Protocol and sanitized operation traces verified on PC; native discovery/control/save/load and UI reconnect work.
  - [x] 16 October 2026, on the Pi beside the running instrument (a second engine from `/opt/peepo` on its own dummy JACK server): 34 protocol checks (add/remove/connect/disconnect/set, refused URI, second-client notification, reconnects, a half-sent message and a killed client, full graph read-back, save) and the interface adapter's 56 live checks all pass; control round trip median 5.17 ms. The instrument kept its processes, 0 xruns. [Results](evidence/pi-engine-check-results.txt).
- [x] **P3.6 — Host features:** confirm selected LV2 feature support, worker scheduling, state save/restore, path messages and optional bypass semantics. Inspect source as a guide, then verify runtime behaviour; source presence alone is not a pass.
  - [x] Worker/state/path/model cases tested on PC; native Clouds bypass and NAM load/control checks pass.
  - [x] 16 October 2026, on the Pi: LV2 state saved into the bundle and reloaded, also after moving the bundle; paths with spaces and percent-encoding; a missing plugin; NAM model paths as file URI and `atom:Path`, worker loading, missing/corrupt files and state carried in the bundle into a fresh engine. Same results as on the PC. [Results](evidence/pi-engine-check-results.txt).
- [ ] **P3.7 — Routing semantics:** test mono/stereo connections, fan-out, multiple connections into one input, summed levels and needed compensation/mixer behaviour. Record a valid first-version policy before enabling merges in the real UI.
  - [x] Stereo IN -> Clouds -> OUT is read back and heard on Pi; routing policy and summing have PC evidence.
  - [x] 16 October 2026, on the Pi: fan-out leaves the first branch unchanged; two cables into one input sum at unity with no compensation (0.25 + 0.25 = 0.5000, +6 dB), as on the PC. [Results](evidence/pi-engine-check-results.txt).
  - [ ] Mono-to-stereo expansion through a real mono plugin on the Pi, and the merge policy (attenuate or mixer) before merges are offered.
- [x] **P3.8 — Reconnection spike:** connect a minimal client, disconnect it, reconnect and retrieve the active graph without disturbing audio. Investigate any failure in Poly's fork before implementing a full UI around it.
- [x] **P3.9 — Host suitability decision:** document passing features and any adaptations. Keep Ingen as the direction; propose a host substitution only if a reproducible blocker prevents core requirements, with the concrete impact recorded.

**Deliverable:** reproducible engine/plugin build record, working saved example graph, protocol examples and host-capability/routing results.

**Exit check:** audible processing is stable, graph commands are verified and the client can reconnect while audio continues. If a required host feature fails, resolve that specific issue before expanding dependent work.

## Phase 4 — real engine integration and native Pi graphics

**Purpose:** turn the desktop editor into an instrument controller without coupling audio to the UI process.

**Reconciled status — 14 October 2026:** P4.4 now includes catalogue generation on the Pi: 226 inventory entries. P4.5 is complete on the physical 800 x 480 HDMI screen with QDtech USB touch and correct orientation/positioning. P4.6/P4.8 include the native interface-only restart: JACK and Ingen kept their PIDs, the new interface recovered the same saved graph, and the user heard no interruption. P4.7 remains partial for the full physical touch-workflow checklist. [Native evidence](23-pi-bring-up.md#patch-saveload-and-interface-restart-with-guitar).

**Historical execution record - 14 October 2026 (PC only):** the code side of P4.5 is in place: DRM/KMS (preferring an 800 × 480 mode) or framebuffer, evdev touch found by capability, and rotation. The build was initially tested under emulation; native screen/touch acceptance has since passed, as recorded above.

P4.8 is complete and verified under WSL2:

- a package and installer with rollback, which never touches config.txt or MODEP;
- `systemctl restart peepo-ui` restarts only the interface;
- engine patch 0004 lets the installed engine listen on its UNIX socket only.

See [Linux and Pi](19-linux-and-pi.md).

**Historical execution record - 13 October 2026 (PC only):** P4.1–P4.3 are implemented in `src/engine/ingen/` and pass 39 live checks against the patched Ingen under WSL2, including a measured audio level through a patch the adapter built, a second client, an interface restart and an engine restart; the command checks were repeated over TCP. The Windows interface runs against that engine with `--engine`. The reconnecting half of P4.6 is included. See [the adapter record](15-ingen-adapter.md). The ARM build, Pi deployment, catalogue export and native backend have since passed; the remaining physical workflow checks are tracked under P4.7.

- [x] **P4.1 — Ingen adapter:** implement the verified protocol using suitable RDF/Atom serialization/parsing, complete-message handling and bounded input. Do not parse Turtle by splitting arbitrary periods or assuming each socket read is a full message.
- [x] **P4.2 — Communication threading:** isolate socket I/O from LVGL drawing, queue normalized events to the UI thread, bound queues and coalesce redundant continuous-parameter updates. Preserve discrete switch actions and graph operations.
- [x] **P4.3 — State synchronization:** load a complete engine snapshot, apply subsequent updates and correlate pending edits with results. Define snapshot completion and update ordering so reconnect cannot lose edits or duplicate modules.
**Historical execution record - 13 October 2026 (PC only):** P4.4 is implemented: `peepo_catalogue_export` reads installed plugins through Lilv and writes a versioned catalogue file, which the interface loads with a project-owned overlay. On the WSL engine it exported 64 plugins and offers 58; all 58 load in Ingen and match their exported metadata, including Poly's 21 Mutable-derived modules built for x86-64. This is also first evidence for P6.2 and P9.3: Clouds loads, is switched on through its own `enabled` control, and passes a measured signal. See [the catalogue record](16-catalogue-export.md). The ARM builds and export on the Pi have since completed; see the reconciled record above.

- [x] **P4.4 — Metadata discovery:** export installed LV2 metadata on Linux using the selected host/Lilv facilities. Generate a versioned catalogue consumed by the UI; use an exported fixture on Windows so PC work does not need native LV2 binaries.
- [x] **P4.5 — Pi platform backend:** build the same views/domain code for Linux with a validated DRM/KMS or framebuffer driver and evdev/libinput touch. Configure stable device selection, 800 × 480 output and rotation/calibration where needed.
**Historical execution record - 14 October 2026 (PC only):** P4.6 is complete. The interface shows starting, ready, reconnecting, offline and engine-error states with the reason, keeps the last graph while edits are paused, disables adding, and names the module when a plugin fails to load. A restarted interface recovers the graph, the stored identity and the unsaved-changes marker, which now live in the engine. Nothing in the interface starts or stops an engine. Verified with LVGL pointer checks and against the WSL engine stopped and restarted under a running interface; see [patch storage](17-patch-storage.md).

- [x] **P4.6 — Error and reconnect states:** show starting/disconnected/reconnecting/plugin-failed status, reject unavailable operations, and recover the current graph after UI-only restart. A transient communication error must not launch a competing audio engine.
- [ ] **P4.7 — Initial live workflows:** add/connect/edit/remove the simple effect on the physical screen; verify actual audio against the displayed graph. Test source/output endpoints, invalid cables and merge policy from Phase 3.
  - [x] Physical touch positioning, IN/OUT patching, Clouds control use and actual guitar sound confirmed.
  - [ ] Complete physical-screen add/remove, conventional-effect editing, invalid-cable and merge-policy checks.
- [x] **P4.8 — Deployment loop:** document copying/installing the UI to an isolated Pi prefix and starting/restarting only the UI over configured remote access. Keep Windows simulation independent; direct Windows-to-Pi Ingen control is optional and requires an explicitly verified transport.

**Deliverable:** real Ingen adapter, exported catalogue, native Pi LVGL build and working touch-controlled simple patch.

**Exit check:** graph and parameters shown match the engine; killing/restarting the UI preserves audio and recovers current unsaved engine state. Complete finger tasks at 800 × 480 and measure UI resources without assuming a RAM ceiling.

## Phase 5 — patches, plugin state and assets

**Purpose:** preserve the entire musical patch, not merely its screen positions and numeric values.

**Reconciled status — 14 October 2026:** The implemented storage contract is now exercised on the Pi with the actual Clouds guitar patch. Revision 1 preserves the user's Freeze-on controls; revision 2 is the current saved/loaded live-input version with Freeze off. Name, stored identity, module, four cables, layout and eleven controls restore (numeric tolerance 1e-6); both revisions remain. These checks used the application's adapter and PatchStore. Clouds has no persistent captured-buffer state. File-based assets/state, physical assignments, the full touchscreen patch-manager interaction sweep and abrupt power loss were not established by this run. [Native evidence](evidence/pi-persistence-results.txt).

**Historical execution record - 14 October 2026 (PC only):** P5.1–P5.8 are implemented in `src/storage/`, the adapter and the interface, and verified against the patched Ingen under WSL2 and from the Windows build through `/mnt/c`. The engine writes each revision with plugin state into staging; the interface publishes it with an atomic rename and keeps two revisions. The live checks reload a patch with a Mutable-derived module and measure the reloaded patch's level; the probe saves and reloads a stateful plugin's state, also after moving the bundle. They also load a bundle stripped of every project property, and refuse missing plugins and unreadable bundles. See [patch storage](17-patch-storage.md) and [the results](evidence/patch-storage-results.txt). Not done: power-loss tests and `fsync` (deferred to P10.5, as P5.8 allows), real model, impulse-response and sample assets (Phase 8), and control-assignment mappings (Phase 7).

- [x] **P5.1 — Storage contract:** assign stable patch identities, schema versions and display names. Use one Ingen graph/state bundle per patch, with positions, view state and control assignments stored as `urn:peepo:ns#` properties on blocks, ports and the graph.
- [x] **P5.2 — Confirm bundle format:** verify actual host output and load behaviour. The inspected Ingen serializer supports `.ingen` directory bundles containing `main.ttl`, `manifest.ttl` and related state; do not assume the `.ingen` path always names one flat file.
- [x] **P5.3 — Property schema:** define the project properties for positions, colours/names, assignments and view settings, with a schema version on the graph. Missing or unreadable properties should permit usable audio graph loading and default layout recovery.
- [x] **P5.4 — Asset policy:** define model/IR/sample locations, referenced versus imported assets, portable state/path mapping and missing-file detection. Store plugin assets/state through host-supported mechanisms and verify relocation rather than rewriting paths blindly.
- [x] **P5.5 — Complete saves:** save the bundle into staging, wait for the engine's reply, then publish it with an atomic rename where the platform supports it. Preserve the previous complete revision until the new one is in place.
- [x] **P5.6 — Patch manager:** load, save, save as, rename, delete and select startup patch, with touch keyboard and explicit handling of unsaved changes. Renaming a display label must not invalidate instance/control identities.
- [x] **P5.7 — Restore/error handling:** recover routing, values, plugin state/assets and UI metadata; report absent plugins, wrong versions and invalid mappings without silent substitution. Distinguish the selected startup patch from the currently edited patch.
- [x] **P5.8 — Persistence evidence:** test stateful/file-based plugins, save/reload, a bundle with its project properties removed, missing assets and interrupted staging. Validate schema migration and revision selection with meaningful tests; defer abrupt power-loss tests until recovery storage is ready.

**Deliverable:** patch manager, documented/versioned storage format, saved examples and recovery results.

**Exit check:** reload reproduces the intended graph, sound, metadata and asset references; an incomplete save does not overwrite the last complete revision. The UI can rebuild a layout when the layout properties are missing.

**Source evidence:** [Ingen serializer at inspected revision](https://github.com/polyeffects/ingen/blob/8dae740425f0f1468198e0cdf1e1c61b3fc6ec5e/src/Serialiser.cpp).

## Phase 6 — Clouds and the first musical patch

**Purpose:** validate the distinctive granular module before broad catalogue expansion.

**Reconciled status — 14 October 2026:** P6.1–P6.3 have native build/library/load/control evidence for the six offered modes, plus guitar listening and a saved/reloaded granular patch. The user confirmed the first audible musical milestone. P6.4 is partial: the complete per-mode musical sweep remains. P6.5 is partial: saving/reloading Clouds is done, but no conventional effect is in this patch yet. P6.6 retains the combined-load/latency/thermal budget work; the 22-second restart test is functional continuity evidence. [Clouds](20-clouds.md), [native results](23-pi-bring-up.md).

**Historical execution record - 14 October 2026 (PC only):** P6.1–P6.3 are done on the PC.

- The build is recorded for x86-64 and arm64; the arm64 binary needs only the C and C++ runtimes.
- All seven modes load in Ingen and process sound.
- Pitch tracks whole semitones exactly, and freeze holds the buffer after the input stops.
- Blend 0 is the dry signal, 9 dB down.
- Values come back from a bundle; the captured audio does not (Clouds has no LV2 state).
- In gain does nothing, and beat repeat ignores most of its controls, because the port never sets the firmware's beat-repeat parameters.
- The overlay can now present individual controls. Clouds shows per-mode names, switches for freeze and reverse, and whole-semitone pitch.

See [Clouds](20-clouds.md). Pi guitar listening, native mode/control checks, Clouds patch save/reload and interface-only restart are now recorded above. A full listening sweep (the rest of P6.4), the Clouds-plus-conventional-effect patch (P6.5), and combined-load budgets (P6.6) remain open.

- [x] **P6.1 — Build dependency record:** select the Clouds-derived target, capture its Eurorack/Parasites revisions and required ARM build changes, and keep changes outside the unchanged reference snapshot.
- [x] **P6.2 — Binary/host check:** verify architecture, dynamic dependencies, URI, required features, ports and intended operating mode through Ingen.
- [x] **P6.3 — Editor integration:** generate the generic parameter page and project-owned overlay for key granular controls, ranges, units and freeze. A custom Clouds editor is optional after the generic page is usable.
- [ ] **P6.4 — Musical validation:** exercise live input, wet/dry, pitch/grain controls, freeze and supported modes; check levels, unintended silence and state restoration.
  - [x] Live guitar listening, granular control changes, saved-control restore and UI-restart continuity confirmed.
  - [x] Wet/dry fixed after the user heard the dry signal drop when Clouds was switched on: blend now mixes the untouched input like a pedal's mix knob (full dry to 0.5, Clouds full from 0.5), with Clouds' reverb kept off the dry path. Measured on the Pi: blend 0 equals the input, 0.5 is full dry plus Clouds, 1 is Clouds alone. Since that sum is above unity, every Clouds mode now has an Output control (-24 to +12 dB, default 0 dB; the dry path follows it to within 0.03 dB). [Results](evidence/pi-effect-results.md#clouds-blend), [Clouds](20-clouds.md).
  - [ ] Finish the per-mode pitch/grain/freeze, level and unintended-silence listening checklist, including blend by ear in each mode.
- [ ] **P6.5 — Milestone patch:** build input → Clouds plus one conventional effect → output, using series or validated parallel routing as musically appropriate. Save it through Phase 5 workflows and reload it on the instrument.
  - [x] Clouds guitar patch is saved and reloaded on the instrument with two retained revisions.
  - [x] `Overdrive into Clouds` (IN -> MDA Overdrive -> Clouds -> OUT) built through the adapter, saved as a new patch and reloaded from the card with every module, cable, control and layout restored; 22 s of steady playback without gaps or xruns. [Results](evidence/pi-effect-results.md).
  - [ ] User listening with the guitar, then edit/save/reload it on the touchscreen.
- [ ] **P6.6 — Combined load:** record DSP, memory, xruns, latency and thermal behaviour while changing parameters and navigating. Document limitations before adding more expensive modules.
  - [x] Short restart continuity run: 22 seconds, 8,251 blocks, no observed gaps/xruns/silent outputs.
  - [ ] Measure sustained DSP/memory/latency/thermal behaviour while editing representative combined patches.

**Deliverable:** supported Clouds catalogue entry, saved representative patch and resource/behaviour record.

**Exit check:** the patch can be built and edited with fingers, heard, saved and restored; UI restart leaves audio running. Physical assignments needed for the full first milestone are completed in Phase 7.

## Phase 7 — physical controls and performance assignments

**Purpose:** make the saved patch usable with owned knobs and selected switches.

**Reconciled status — 14 October 2026:** The control inventory is known (four 20-pulse push encoders, three momentary SPST footswitches). No controls are connected. GPIO/kernel-input transport and wiring are proposed; the working audio/UI restart test does not close physical-event, assignment or controller-recovery acceptance.

**Historical execution record - 14 October 2026 (PC only), P7.3:**

- The adapter reads the kernel's input devices without blocking: each encoder on its own relative axis, each push or footswitch on its own key.
- It finds devices again after they go, and ignores the kernel's autorepeat.
- Debounce and press/release/hold come from the shared recognizer.
- It was tested with virtual devices made through uinput.
- Bindings and the proposed boot lines are in `config/pi/`.

It stays open until it runs on the real encoders and footswitches. See [Linux and Pi](19-linux-and-pi.md).

**Historical execution record - 14 October 2026 (PC only):** the hardware is known: four 20-pulse push encoders and three momentary SPST footswitches. P7.2, P7.4 and P7.5 are complete. Keys stand in for the controls and produce the same events the GPIO adapter will. Encoders follow the parameter page in view or are pinned to a parameter. Stomps toggle, hold or bypass, or change patch for every patch. Assignments are saved with the patch as engine graph properties and orphans are never applied. A direct-GPIO wiring and the kernel drivers are proposed for P7.1. P7.6 is done apart from tap tempo, which needs a tempo the engine does not yet have. See [physical controls](18-physical-controls.md). Still open: real GPIO/device bench checks (P7.3), tap tempo, and physical-controller resilience on the Pi (P7.7).

- [ ] **P7.1 — Select transport:** choose USB MIDI, Linux input or an appropriate GPIO/ADC adapter from the hardware profile. Document wiring and device identification only after the actual components are known.
- [x] **P7.2 — Simulated controls:** map desktop keys/wheel or explicit simulator controls to the same normalized events as the hardware adapter. Use this to develop assignment workflows without the Pi.
- [ ] **P7.3 — Hardware events:** implement nonblocking reads, reconnect detection, debounce, press/release/hold and encoder direction/detent handling. Queue events independently of drawing work.
- [x] **P7.4 — Mapping rules:** support relative encoder increments, bounded acceleration/fine adjustment, potentiometer pickup where applicable, inversion, numeric/logarithmic ranges and discrete values. Avoid jumps when changing patches.
- [x] **P7.5 — Assignment UI:** select a control and target parameter/action, preview its effect, pin assignments and persist them as properties on the target port or graph. Show and disable orphaned mappings rather than targeting a different module accidentally.
- [ ] **P7.6 — Musical actions:** validate one knob parameter and one stomp action first; include freeze/toggle, then tap tempo and patch changes where supported. Preserve urgent discrete actions when coalescing continuous controls.
  - [x] Toggle, momentary, bypass and patch-change actions on simulated controls (14 October).
  - [x] Tap tempo, 16 October 2026: a footswitch set to Tap tempo (every patch) or the Tempo page's Tap pad sets the patch tempo, which every module with a BPM control follows; encoder 1 turns it on that page. Simulated controls and LVGL checks ([record](29-parameters-files-tempo.md#patch-tempo)). Plugins that read JACK transport do not follow it yet.
  - [ ] Validate with the real knob and stomps once wired (P7.1, P7.3).
- [ ] **P7.7 — Resilience:** verify controller unplug/reconnect, UI restart, patch reload, bounce, rapid gestures and removal of an assigned module. Record any control interruption during UI failure; uninterrupted physical control during UI downtime is a separate architecture decision if required.

**Deliverable:** hardware adapter, assignment screen, saved mappings and first fully controlled Clouds patch.

**Exit check:** the instrument satisfies the proposed first milestone: saved patch, Clouds, conventional effect, one knob, one stomp switch, touch editing and a normal restart cycle.

## Phase 8 — NAM and cabinet IR evaluation

**Purpose:** establish what NAM can run without assuming unlimited capacity from Casette's feature list.

**Reconciled status — 14 October 2026:** Both candidate plugins now build for arm64; only the selected NeuralAudio-based plugin is on the engine's path. Its native load, ports, three controls and bypass check passed without a model. P8.1 is complete; P8.2 is partial on Pi because the model/worker/state/error tests are still PC evidence. Model selection and the remaining evaluation tasks stay open. [NAM record](21-nam.md), [native load check](evidence/pi-update-results.txt).

**Historical execution record - 14 October 2026 (PC only):** P8.1 and P8.2 are done on the PC. The current NeuralAudio-based plugin is selected over the Poly fork.

| | Current | Poly fork |
| --- | --- | --- |
| A1 models | load and run | load and run |
| A2 models | load and run | do not load |
| Quality control | yes, and switching is safe | none |
| Maximum block size | takes the host's | does not; allocates in `run()` when the block size changes |
| Model saved with the patch | yes: Ingen copies the file into the bundle, and a fresh engine restores it | the same |

Both take the model as a `patch:Set` through Ingen, either as Beebo's file URI or as an `atom:Path`. A failed load leaves the engine running, but makes the current plugin drop its model. The plugin reports its model only to a client that subscribes to broadcasts.

See [NAM in Ingen](21-nam.md). The arm64 build and native plugin load are complete. Native model/worker/state acceptance (P8.2), selectors (P8.3) and cost evaluation (P8.4 onwards) remain.

- [x] **P8.1 — Candidate versions:** pin a compatible current NeuralAudio-based LV2 plugin and dependencies for comparison with the downloaded Poly NAM fork. Record compiler/target optimizations and actual model implementation selected.
- [x] **P8.2 — Host integration:** verify model-path messages, worker/state handling, maximum block size, error recovery and model lifetime in Ingen. No loading, allocation or unsafe first-use quality initialization belongs on the real-time callback.
  - [x] Full model/worker/state integration checks on PC; native plugin load/ports/controls without a model.
  - [x] 16 October 2026, on the Pi (A1 WaveNet and A2 slimmable captures, 48 kHz): both message forms load a model; both models run; missing and corrupt files leave the engine running and the plugin dry; 60 switches (engine 114 to 118 MiB); A2 quality switching; the model saved inside the bundle runs after reload, after the original file is renamed, and in a fresh engine; models load at 64, 128 and 1024 frames. Dummy JACK, not real time: no load figures. [Results](evidence/pi-engine-check-results.txt).
- [ ] **P8.3 — NAM/cabinet pages:** add local selectors, active file names, calibrated input/output controls and supported quality settings. Confirm whether each capture is amp-only or already includes a cabinet.
  - [x] 16 October 2026: a Model or Impulse response tile on the module's page names the chosen file and opens a picker of the files in `/var/lib/peepo/files/nam` or `ir`; the adapter gives the plugin the path and names it on the block, so it is saved, reloaded and duplicated with the patch. NAM shows Input, Output and Quality. NAM, LV2 Convolution and Poly's convolvers are offered again; the x42 IR Convolver stays withheld (its worker fails to load an IR in Ingen). [Record](29-parameters-files-tempo.md#model-and-ir-files-p83).
  - [ ] Note per capture whether it includes a cabinet (the user's models), review on the physical screen, and listen.
- [ ] **P8.4 — Single-model baseline:** benchmark one mono NAM, then NAM plus IR, at the capture's sample rate. Compare representative A1 categories and supported A2 quality levels at 64/128 frames where supported; record exact model/IR hashes and quality.
- [ ] **P8.5 — Combined patches:** add conventional effects and Clouds incrementally, then evaluate additional models only if the complete chain retains headroom. Parallel cables alone do not imply multicore processing.
- [ ] **P8.6 — Endurance and switching:** run a representative 30-minute session, repeat model/IR/preset changes and inspect memory trends, load spikes, xruns, temperatures and throttling. Include corrupt/missing assets and quality changes.
- [ ] **P8.7 — Catalogue decision:** publish supported model/runtime conditions and tested patch budgets, mark failures/untested combinations explicitly, and retain user-selected quality. Automatic quality reduction remains an open product decision.

**Deliverable:** NAM/IR compatibility results, measured comparison, working saved NAM patch and documented capacity limits for tested configurations.

**Exit check:** a selected NAM patch restores its assets/quality and remains stable under realistic use. Failed or incomplete NAM evaluation does not invalidate the already working Clouds milestone; it remains an explicit outstanding candidate feature.

## Phase 9 — complete the catalogue and interface workflows

**Purpose:** expand coverage without advertising installed plugins as automatically supported.

**Reconciled status — 14 October 2026:** P9.2 now includes the arm64 build and actual Pi installation of all buildable selected components, including the repaired chorus build. The Pi exporter found 226 entries from the 249-entry inventory; 186 were available and 40 withheld before presentation overlays. The latest PC matrix records 91 supported-pc, 110 needs-adaptation, 25 not-tested and 23 unavailable. P9.3 remains partial for native per-entry acceptance: the six offered Clouds modes and NAM received the focused native checks, not the full inventory. [Latest matrix](evidence/compatibility-matrix.md), [deployment](evidence/pi-update-results.txt).

**Historical execution record - 14 October 2026 (PC only):** P9.1–P9.3 are done on the PC, and P9.4 is under way.

- **Inventory.** The selected inventory is every identity in Beebo's module map and Poly's plugin sources (207), plus the MDA and LV2 example plugins used for development (42). The catalogue now offers only what that list names.
- **Builds.** Poly's remaining repositories, Beebo's third-party sources and Debian packages are built for x86-64, at recorded commits. Upstream defects are worked around on the command line.
- **Records.** Every selected entry has a PC compatibility record. The current machine-readable record and matrix contain 91 supported-pc, 110 needs-adaptation, 25 not-tested and 23 unavailable. These remain PC classifications; installation and a catalogue entry do not establish native musical support for every plugin.
- **Presentation.** The overlay gives every offered module a category and a readable name, adds a Control category, and withholds the file-based and faulty entries with a reason.

See [the plugin catalogue](22-plugin-catalogue.md) and [the compatibility matrix](evidence/compatibility-matrix.md). Arm64 builds and Pi catalogue generation are complete. Native per-entry acceptance (P9.3), remaining presentation work (P9.4), P9.5 and P9.6 remain.

- [x] **P9.1 — Inventory:** reconcile the agreed Mutable families and essential MODEP-compatible effects with the actual installed/source inventory. Classify every entry as supported, needs adaptation, unavailable or not tested.
- [x] **P9.2 — Reproducible builds:** build candidate families incrementally with captured revisions, architecture flags and dependency fixes. Separate platform changes from unrelated upstream modifications.
- [ ] **P9.3 — Compatibility records:** verify each URI's required features, audio/control/CV/event ports, save/restore, external assets and representative behaviour through Ingen. The canvas routes audio and control (CV); mark modules that need deferred MIDI routing clearly.
  - [x] Every selected identity has a PC compatibility result; focused native checks cover six Clouds modes and NAM.
  - [ ] Run and record required native behaviour/state/assets checks for each entry advertised as supported.
- [ ] **P9.4 — Presentation overlays:** add project-owned categories, names, parameter grouping and suitable value editors. Use generic pages first; add custom editors only when they materially improve musical operation.
  - [x] Categories/readable names for exported modules, generic editors and Clouds per-mode overlays are implemented.
  - [x] One Output on every module that sends audio: the plugin's own level control where it has one (hand-checked list in the overlay), otherwise a general -24 to +12 dB Output applied by the engine (patch 0008), saved with the patch and not applied when bypassed. Verified against the patched engine under WSL2 (level, save/reload, duplicate, bypass). [Catalogue](22-plugin-catalogue.md#output-on-every-module-16-october-2026).
  - [x] 16 October 2026: named parameter groups from the overlay (Clouds modes, Rotary speaker, Parametric EQ, Vinyl, MultiBand, Marbles), logarithmic scales and named values for controls, mode buttons for short lists and -/+ for whole numbers. [Record](29-parameters-files-tempo.md#parameter-pages-p94).
  - [ ] Physical-screen review, and further modules grouped as they are used.
- [ ] **P9.5 — Complete screens:** finish browser filtering, patch management, assignment management, diagnostics/settings, shutdown navigation and repair states for missing plugins/assets.
  - [x] Settings shows how the instrument is reached: Wi-Fi name, IPv4 address and signal (or "Not connected"), read from the kernel and refreshed every 2 s while open. Linux parsing test and LVGL check pass (arm64 too); installed on the Pi 16 October 2026.
  - [x] 16 October 2026, PC and arm64 tests (not yet installed on the Pi): category chips inside a browser group; Revert to the last saved revision; Settings > Diagnostics (engine, under-voltage alarm with a once-only footer warning, temperature, memory, storage, uptime, read from the kernel; xruns not counted); Settings > Power with Shut down and Restart and a final page; a canvas banner and an explained actions page for missing modules. [Record](28-screens-and-diagnostics.md).
  - [ ] Install on the Pi and check Diagnostics, Restart (polkit) and the banner on the physical screen; xrun counting; a per-module replacement for a missing module, if wanted.
- [ ] **P9.6 — Usability and load:** exercise sparse/dense graphs, long names, touch keyboard and navigation on the physical screen. Record module costs and tested combination budgets rather than one universal module count.

**Deliverable:** versioned supported catalogue, completed interface workflows and per-entry compatibility matrix.

**Exit check:** all entries in the selected release inventory have a result; every entry advertised as supported passes its required evidence. Full collection completion still requires resolving remaining agreed entries or explicit acceptance of limitations.

## Phase 10 — dedicated startup and recovery

**Purpose:** turn the tested application into an unattended instrument.

**Reconciled status — 14 October 2026:** P10.1 is complete for the configured service boundaries and real device permissions. The target is now enabled, with multi-user boot and desktop/MODEP/automatic-console competitors masked. One software reboot restored the selected Clouds guitar revision and all four Tascam connections; the user confirmed direct display and working touch. A deliberate JACK crash recovered JACK and Ingen and restored Clouds while the UI kept its PID. The earlier UI-only restart during guitar playing preserved audio over 8,251 blocks. Automatic UI-crash recovery was checked under emulation; native UI crash, physical late hardware, offline/five cold boots and the other cases below remain open. [Direct boot](24-instrument-boot.md), [UI continuity evidence](evidence/pi-persistence-results.txt).

**Historical execution record - 14 October 2026 (PC only), P10.1:** JACK, the engine, the audio routing and the interface are separate systemd services under a `peepo` account, grouped by `peepo.target`. With JACK's dummy driver under WSL2, killing the interface during playback left all 750 measured periods at the expected level and restarted only the interface. An engine restart left the interface running, reconnected and rewired. It is now closed by the native device/real-time service run and UI-only restart recorded above; broader native crash recovery remains P10.7. See [Linux and Pi](19-linux-and-pi.md).

- [x] **P10.1 — Service boundaries:** configure JACK, Ingen and UI startup separately with correct user/device permissions and a deliberate restart policy. Normal UI failure must not restart Ingen or JACK.
- [ ] **P10.2 — Readiness:** gate startup on usable devices, responsive engine, successful patch restoration and explicit routing. Provide waiting/error states rather than relying on fixed sleeps or a splash screen.
  - [x] Native reboot: JACK readiness probe, socket availability, UI connection, saved patch restoration and four external routes verified. No desktop/login required.
  - [ ] Complete device/patch readiness and native error-state acceptance, including missing/late physical hardware.
- [ ] **P10.3 — Offline operation:** ensure startup and everyday playing/editing do not wait for network or model downloads. Cache the tested catalogue/assets locally.
  - [x] Plugins, catalogue and the saved Clouds patch are local and used during the playing session.
  - [ ] Verify unattended startup and everyday operations with network unavailable.
- [ ] **P10.4 — Startup patch:** select and restore the intended saved revision, handle missing plugins/assets and show a repairable result without silently substituting a different musical patch.
  - [x] Startup selection/restore code has PC tests; a real saved Clouds revision exists on Pi.
  - [x] Select Clouds guitar and verify native boot restoration of its saved revision, identity, controls, layout and cables.
  - [ ] Verify native missing-asset/plugin startup handling; boot restoration currently depends on the UI.
- [ ] **P10.5 — Shutdown/storage:** finish saves before graceful shutdown, document safe recovery and decide whether an overlay/read-only root is needed after measuring its implications for persistent user storage.
  - [x] Saved revisions, the startup choice and control settings are flushed (`fsync` of each file and its directory) around their atomic renames. Shut down refuses while an edit, patch change or save is pending, checked again at confirmation.
  - [ ] Pull-the-power test after a save on the Pi, documented recovery, and the overlay/read-only root decision.
- [ ] **P10.6 — Cold-boot measurements:** perform the proposed five cold boots and measure power-on-to-audio and power-on-to-responsive-editor separately. Optimize observed delays; set numeric targets from the measured baseline.
  - [x] Record one software reboot's service timings: UI starts at 9.40 s, Clouds load begins at 10.33 s, all external routes at 11.92 s since kernel startup. Physical display/touch confirmed.
  - [ ] Five physical cold boots and separate power-on/audio/editor measurements, including firmware time.
- [ ] **P10.7 — Failure recovery:** exercise UI crash, late audio-device availability, engine startup failure and controller reconnect. Distinguish UI recovery with continuous audio from engine recovery where an audio interruption is expected and must be reported.
  - [x] Native ordinary UI restart preserves guitar audio and graph; automatic UI crash recovery has emulated evidence.
  - [x] Native deliberate JACK crash: JACK/Ingen restart automatically, UI stays running, saved Clouds and four audio routes return.
  - [x] 16 October 2026, native UI crash: SIGKILL of the interface while the instrument played; systemd restarted it, it reconnected 1.7 s later, JACK and the engine kept their PIDs, the graph was unchanged, 8,251 audio blocks over 22 s with no gap or xrun. [Results](evidence/pi-ui-crash-results.txt).
  - [ ] Verify late audio hardware, engine-start failure and controller-reconnect cases.

**Deliverable:** verified service configuration, startup patch behaviour, shutdown/recovery instructions and measured boot results.

**Exit check:** repeated unattended boots reach the intended usable patch and controls without login or desktop interaction, including offline operation. UI-only failure preserves audio.

## Phase 11 — validation, packaging and handoff

**Purpose:** make the result reproducible and state precisely what is supported.

**Reconciled status — 14 October 2026:** The development package has been built/tested for arm64, installed on the actual Pi and retained with rollback instructions. The requirements table below now links the current native evidence. This is partial P11.1/P11.3/P11.5/P11.7 evidence; release provenance/source completeness, full lifecycle/endurance, remaining requirements and the complete user guide are still open.

- [ ] **P11.1 — Requirements matrix:** map R01–R13 to named test results on the actual hardware. Treat NAM and extra model counts as separately recorded candidate capabilities, not implied requirements.
  - [x] Current native evidence and gaps are mapped to R01–R13 in this plan.
  - [ ] Complete the release acceptance matrix with the remaining named hardware test results.
- [ ] **P11.2 — End-to-end sessions:** test representative Clouds, conventional and supported NAM patches under realistic screen/control activity; record DSP/per-thread CPU, xruns, memory, latency and thermal stability.
- [ ] **P11.3 — Lifecycle regression:** verify normal boot/shutdown, UI restart, engine failure recovery, saved revisions, interrupted writes, asset relocation where supported, missing plugins and controller reconnect.
  - [x] Native saved revisions, reload and UI-only restart have recorded results.
  - [ ] Complete boot/shutdown, native engine failure, interrupted-write, relocation and controller cases.
- [ ] **P11.4 — Provenance and licenses:** record all upstream and project commits, dependency pins, compiler options, plugin URIs, fonts/assets, license/version grants, exceptions and applicable notices. Inventory the selected modules and their complete dependencies; preserve Ingen's AGPL-3.0-or-later terms. Establish permissions for every distributed model/IR/asset. Ensure the new interface contains no copied Beebo code or assets. Follow [third-party notices](../THIRD_PARTY_NOTICES.md).
- [ ] **P11.5 — Installation package:** prepare repeatable build/install steps, configuration templates, supported plugin/asset inventory and rollback instructions. Include required license/copyright notices and the corresponding source and build/install material required by the shipped components' terms, with an exact-version source retrieval record. Validate them on spare storage or a recoverable installation, not only the development tree.
  - [x] Reproducible arm64 development package, actual Pi install and prefix rollback route recorded.
  - [ ] Finish the release component/source/license inventory and full recovery-installation acceptance.
- [ ] **P11.6 — User operation guide:** document patching, parameters, assignments, models/IRs, save/startup patch selection, shutdown, diagnostics and known limitations with actual screenshots.
- [ ] **P11.7 — Release record:** report passed, failed, not-tested and explicitly accepted limitations. Do not mark the full instrument complete while agreed requirements remain unresolved.

**Deliverable:** versioned release candidate, reproducible manifest, validation matrix, operating guide and recovery package.

**Exit check:** agreed release requirements have evidence, installation can be reproduced, and all remaining limitations are visible. A compiler success or a screenshot is not release acceptance.

## Phase 12 — MIDI

**Purpose:** MIDI in and out on the instrument, MIDI cables between modules, and instrument-wide clock, program change and MIDI learn, as Beebo has them ([Beebo manual gaps](30-beebo-manual-gaps.md#7-midi), item 7). The design, hardware and wiring are in [MIDI](39-midi.md).

**Status — 20 October 2026:** P12.4 done on the PC and in WSL. Built and checked on the PC and in WSL, waiting for the Pi and real devices: P12.3's PC half (clock out), P12.6 (Settings → MIDI), P12.7 (MIDI learn), P12.8 (program change, clock in), P12.11 (control changes on load), and P12.5's two plugin builds ([record](evidence/midi-settings-results.txt), details in [MIDI](39-midi.md#steps)). P12.9, 21 October 2026: MIDI to CV mono and poly on the Pi (Peepo sets the poly's Voices from its cables); MIDI program change written, checked in WSL and installed on the Pi. Not built: P12.10 (the jacks). Decided: **one merged MIDI IN** on the canvas; every input device feeds it, and a patch tells devices apart by channel. MIDI cables revise D16, which deferred them; the decision log is updated. MIDI jacks are teal diamonds on the side edges, chosen from three sketches. Order: USB devices first (no soldering), then MIDI cables and modules, then learn, our own plugins and the enclosure's jacks.

- [ ] **P12.1 — MIDI devices:** for each device tried (the user has an OBNE Dark Star, a TE KO-2, Roland Aira units, Korg Volcas, a Boss SL-2): its MIDI connector (DIN, TRS Type A or B, sync jack, USB-MIDI), its channel, whether clock sync must be enabled, its response to Start/Stop/Continue and a stopped clock, and its CC map. Any USB port is checked for USB-MIDI (`aconnect -l`).
- [ ] **P12.2 — MIDI devices through USB on the Pi:** `a2jmidid` as its own service, turning ALSA MIDI devices into JACK MIDI ports; `peepo-route` wires them. USB MIDI devices and USB-MIDI interfaces appear, and come back after unplugging. No xruns on the Tascam with a MIDI device on the bus. *Built and checked on the Pi with a virtual device, 20-21 October 2026* ([record](evidence/midi-devices-results.txt), `tools/pi/check-midi.sh`): `peepo-a2j.service` runs `a2jmidid -e`; `peepo-route` wires every device's MIDI out to `ingen:midi_in` and `ingen:midi_out` and `peepo-midi:clock_out` to every device's MIDI in, by port direction, not name (ALSA's Midi Through excluded). A virtual card was wired both ways within 2 s, a note from it reached JACK, the clock went out through ALSA, and it was wired again after a replug; a JACK restart brought every MIDI service and connection back. Still to do: a real device (`check-midi.sh --device NAME`), and the xrun check, which waits for the power supply: the engine's xruns under undervoltage were there before MIDI.
- [ ] **P12.3 — Clock out (`peepo-midi`):** a separate service, a JACK MIDI client and second engine client. 24 clock messages per beat at the patch tempo, placed at the exact frame; Start when the clock begins. On the PC under WSL2 first (dummy driver, virtual MIDI port), then on the Pi to devices that follow MIDI clock. Checks: intervals at 30, 120 and 300 BPM, measured with a second interface or a logic analyser; a tempo change from the next clock message; a patch change carrying its tempo; the clock continuing while the interface restarts. *PC half done 20 October 2026 ([record](evidence/midi-clock-results.txt), `tools/ingen-spike/24-clock.sh`):* `src/midi/clock.hpp` (unit-tested in CTest `midi`, also on Windows) and the Linux service `peepo-midi` (JACK port `peepo-midi:clock_out`, unit `peepo-midi.service` in `peepo.target`). The clock begins once the engine has said the tempo, with Start; Stop when the service stops. In WSL: exact intervals at 30, 120, 300, 100 and 87 BPM; every change from the next clock message; a patch load goes straight from the old tempo to the new (a load used to pass through 120 BPM; fixed); no gap while interfaces connect and are killed. Still to do: on the Pi, wired to devices once P12.2 routes MIDI, measured there and checked by ear on the user's clock followers.
- [x] **P12.4 — MIDI cables (PC):** split `PortType::Midi`, so only atom ports that support `midi:MidiEvent` become connectors and file-message ports (Cabinet, NAM) stay hidden; a MIDI cable type with its own colour, MIDI output to MIDI input only, the same duplicate and cycle rules; MIDI IN and MIDI OUT connectors; `midi_in` and `midi_out` on the engine's root graph; saved and loaded with the patch, and loaded without a device plugged in. Record the revision of D16. *Done 20 October 2026* ([record](evidence/midi-cables-results.txt), `tools/ingen-spike/23-midi.sh`): `PortType::Atom` for message ports, catalogue format version 2, IN's MIDI output and OUT's MIDI input as `midi_in` / `midi_out`, a note through x42 MIDI Transpose from `ingen:midi_in` to `ingen:midi_out` after a save and load; D16 revised.
- [ ] **P12.5 — MIDI modules:** rerun the compatibility checks with MIDI cables; build x42 MIDI clock and midimsg's MIDI controller; the x42 MIDI filters and the MDA instruments offered once they play from a controller on the Pi; the Ready modules with MIDI ports (MIDI clock in, CV to MIDI CC and Note, Strum, MIDI Gate, Metronome) cabled on their MIDI side.
- [ ] **P12.6 — Settings → MIDI:** devices on or off in each direction, clock out on or off, Start on begin, tempo source, program-change channel; stored instrument-wide beside the patches. The Tempo page gets **Restart** (Stop, then Start).
- [ ] **P12.7 — MIDI learn:** wire Ingen's root `control` port, which its control bindings read; learn and forget on the module page; the CC shown on the bar; bindings saved with the patch as `midi:binding` port properties and restored.
- [ ] **P12.8 — Program change and clock in (`peepo-midi`):** a program change selects a set-list entry (a stored patch in order until the set list exists); incoming clock as the tempo source, with Peepo's own clock out stopped meanwhile.
- [ ] **P12.9 — Our own MIDI plugins:** MIDI to CV mono, then poly, then MIDI program change, each in its own repository under `C:\LV2-own-plugins`, GPL-2.0-or-later; Beebo's two looper MIDI modules with Loo-pi.
- [ ] **P12.10 — The enclosure's TRS jacks:** MIDI OUT on GPIO 14 and MIDI IN on GPIO 15 as wired in [MIDI](39-midi.md); UART freed from the console; 31250 baud measured; `snd-serial-generic` (or a bridge in `peepo-midi` if Raspberry Pi's kernel lacks it); P12.2 and P12.3 repeated on the jacks.
- [ ] **P12.11 — CC per patch:** a short list of CC messages on the patch, sent when it loads, and a page to edit it.

**Deliverable:** MIDI devices usable over USB and the enclosure's jacks, MIDI cables and modules on the canvas, clock in and out, program change and MIDI learn, each with its record.

**Exit check:** on the Pi, a USB controller plays an MDA instrument through a MIDI filter cabled on the canvas; a learned CC moves a control and survives a reload; a program change selects a patch; devices that follow MIDI clock follow Peepo's tempo, a tap and a patch change, also while the interface restarts; and none of it causes xruns.

## Phase 13 — Scenes

**Purpose:** up to eight saved states inside one patch, changed while playing with no gap or click, with the tails of delays and reverbs ringing on. The design, storage format and checks are in [Scenes](44-scenes.md).

**Status — 24 October 2026:** P13.1 and P13.2 done in WSL ([record](evidence/scenes-results.md)): the engine's bypass cut every tail, clicked on 23 of 24 modules and replayed the old buffer on 19; our own plugins ring on except Discipline with Trails off; with `-a` and engine patch 0012 every bundle changes on one frame, and the existing checks pass with it. P13.3 done the same day: with engine patch 0013 every third-party delay and reverb measured rings on when bypassed, without a click or a replayed buffer. Next: P13.4. Decided with the user: a scene stores every module's bypass plus the parameters marked per scene; 8 scenes per patch; delays and reverbs ring on when bypassed, Discipline's and Aura's by default. Order: measure first, then tails, then the scenes themselves, then the screen, stomps and MIDI.

- [x] **P13.1 — Measure (WSL):** `tools/ingen-spike/27-scenes.sh` with the tail, click, replay and side checks for every Delay, Reverb and Texture module under its current bypass, and the atomic-change check (two modules switched in one bundle, by bypass, parameter and mixed) with and without Ingen's `--atomic-bundles`. *Done 24 October 2026 ([record](evidence/scenes-results.md)):* 24 third-party modules and 13 of ours measured; 13 modules not installed in WSL, and the lost-side case (only IR Reverb - Mono to Stereo has it), wait for the Pi in P13.10.
- [x] **P13.2 — Atomic bundles:** `-a` added to the `peepo-engine` launcher, `tools/ingen-spike/env.sh` (`PEEPO_ATOMIC=0` leaves it out) and `tools/pi/check-engine.sh`; patch loading, the adapter checks (`07-adapter.sh`) and the MIDI checks (`23`–`25`) rerun with it on, with no change in results. *Done 24 October 2026:* `-a` alone still let parameter changes in one bundle land up to 17 frames apart, so **engine patch 0012** puts an atomic bundle's events on the cycle's first frame; with both, 120 of 120 bundles changed on one frame. WSL's engine rebuilt with 0011 and 0012; the arm64 build root and the Pi in P13.10.
- [x] **P13.3 — Spill bypass (engine patch 0013):** in `BlockImpl`, a bypassed block marked spill keeps running on silent inputs, adds its outputs to the dry signal, ramps 5 ms each way, takes the dry signal for an unmatched output from the last input (also in the dumb bypass), and falls back to the dumb bypass after one second below -90 dBFS. Buffers made at instantiation. Built in WSL and the arm64 build root. *Done 24 October 2026 ([record](evidence/scenes-results.md#spill-bypass-engine-patch-0013-p133)):* block property `peepo:spillBypass`; `27-scenes.sh spill` on the 22 third-party modules in WSL and Euclidelay under the engine's bypass: every tail rings on, clicks at most 19.2 dB switching off and on, nothing replays, both sides kept; the heavier reverbs back to the empty engine's CPU after their tail. Not to be marked: Mist Spectral (sustains itself) and Vinyl (never slept). Our own switches add up to 28 dB of click switching back on (Eureka Hush, Granny, Prism), for P13.5. The arm64 build root's engine rebuilt with 0011-0013 (its `libingen_server.so` carries the new property); the Pi in P13.10.
- [ ] **P13.4 — `"bypass": "spill"` in the catalogue:** overlay value, refused beside `"port"`; the adapter sets the block property when adding the module and keeps it in saved patches; `peepo_catalogue_export` and `catalogue_tests` know it. Marked on every Delay and Reverb module that passes the tail check with it; the module list regenerated.
- [ ] **P13.5 — Our own plugins:** Discipline's Trails default to 1 in its own repository (saved patches keep their value), a new pin in `14-more-plugins.sh` and `reference/manifest.json`; the tail check run on Aura, Eureka, Euclidelay and Drizzle; Loo-pi unchanged.
- [ ] **P13.6 — Scene model (`src/core`):** scenes, marked parameters (at most 16), names; encode and decode the graph properties; the pure function giving the changes between the live state and a scene; edits on the screen written into the current scene, performance changes not; modules added, removed and duplicated; orphans kept. `peepo_core_tests`.
- [ ] **P13.7 — Engine command `SelectScene`:** `IngenEngine` sends the changes and `peepo:scene-current` as one bundle; the simulator applies them at once; scene add, rename, copy, reorder, delete and mark/unmark as commands. Session snapshot includes scenes. `peepo_ingen_tests` (a second client sees the change), `peepo_storage_tests` (bundle round trip, loading restores the current scene).
- [ ] **P13.8 — Screen:** a mockup approved first; then scene pills on the patch page, the Scenes page, the per-scene marker and hold menu on the module page. `peepo_ui_check` screens.
- [ ] **P13.9 — Stomps and MIDI:** Scene 1–8, Next scene and Previous scene as stomp actions, per patch and for every patch; scene select by CC in `peepo-midi` (setting in Settings → MIDI, default CC 69, on the program-change channel). Core and MIDI tests.
- [ ] **P13.10 — On the Pi:** the package with engine patches 0011–0013 and `-a`; the tail check and the scene-changes-while-playing check run there; a verse/solo patch played with guitar, changed by stomps and by a MIDI foot controller; the heaviest patch with all modules on measured for CPU.
- [ ] **P13.11 — Later:** CC messages sent to other gear when a scene is selected.

**Deliverable:** scenes stored in the patch, changed from the screen, stomps and MIDI; delays and reverbs that ring on when bypassed, for third-party modules and our own; each with its record.

**Exit check:** on the Pi, with guitar, a patch with at least three scenes is played through scene changes from a stomp and from a MIDI controller: the changes are heard at once and together, delay and reverb tails ring on across them, nothing clicks or replays an old buffer, no xruns; the scenes survive a save, a reload and an interface restart.

## Verification strategy and phase records

Use focused automated tests for failure-prone shared logic: graph cycle/channel rules, parameter conversions, protocol parsing, event ordering/reconnect, metadata schemas, assignment pickup and coherent persistence. Use Windows interaction checks for actual view workflows. Use live Pi tests for audio/plugin behaviour, finger operation, GPIO/MIDI, device permissions and boot. Avoid tests that simply mirror widget construction or repeat already passed checks without a new change or unresolved issue.

Each phase record includes task IDs, date, source/build versions, hardware/audio settings where relevant, expected/observed behaviour, result, evidence files and blockers. Screenshots show UI progress; saved graphs and measurement logs show audio/state progress. Keep simulated results labelled separately from target-hardware results.

## Requirements coverage

| Requirement | Primary phases | Current native evidence and remaining acceptance |
| --- | --- | --- |
| R01 automatic start | 10–11 | Basic direct boot passed: target enabled, desktop/MODEP masked, saved Clouds/routing restored, physical display/touch confirmed ([record](24-instrument-boot.md)); full P10.2/P10.4/P10.6 acceptance remains |
| R02 restore saved patch | 5, 7, 10–11 | Clouds graph/controls and two revisions restore on Pi ([persistence results](evidence/pi-persistence-results.txt)); file assets, physical assignments and boot restore remain |
| R03 modules/cables | 2, 4, 9 | Touch patching and four stereo Clouds cables confirmed ([Pi record](23-pi-bring-up.md)); full add/remove/invalid/merge physical workflow remains P4.7 |
| R04 usable parameters | 2, 4, 6, 9 | User changed Clouds controls and eleven values restored ([persistence](evidence/pi-persistence-results.txt)); full per-mode/editor finger sweep remains P6.4/P9.6 |
| R05 guitar processing | 0, 3–4, 11 | Real guitar/Tascam/Clouds path heard; 22-second continuity run passed ([results](evidence/pi-persistence-results.txt)); gain/latency/endurance budgets remain |
| R06 Mutable processing | 6, 9, 11 | Six offered Clouds modes pass native load/control checks; granular is heard ([deployment](evidence/pi-update-results.txt)); wider family/mode behaviour remains |
| R07 MODEP-compatible LV2 | 3, 9, 11 | 226 inventory entries exported on Pi ([deployment](evidence/pi-update-results.txt)); native per-entry functional acceptance remains P9.3 |
| R08 knob assignments | 7, 11 | PC simulation implemented; no physical encoders connected or accepted |
| R09 switch assignments | 7, 11 | PC simulation implemented; no physical stomps connected or accepted |
| R10 offline operation | 1–2, 5, 10–11 | Local binaries/catalogue/patch used; disconnected-network boot and operation not yet tested (P10.3) |
| R11 patch management | 5, 9, 11 | Actual Pi save/reload/revisions pass ([results](evidence/pi-persistence-results.txt)); complete physical Save/Save-as/rename/delete/startup interaction sweep remains |
| R12 new native LVGL UI | 1–2, 4, 9, 11 | Project-owned UI runs on actual 800 x 480 DRM/touch display ([bring-up](23-pi-bring-up.md)); dense/long-name/full navigation touch acceptance remains P9.6 |
| R13 independent audio | 3–4, 10–11 | Passed for native UI-only restart: same JACK/Ingen PIDs, unchanged saved graph, no measured or heard interruption ([results](evidence/pi-persistence-results.txt)); wider failures remain P10.7 |

## Established baseline and remaining decisions

**Established for current development:** Debian 12 bookworm arm64 on the existing Pi 5; the compatible isolated dependency build; LVGL/C++20 and the DRM/evdev backend; the Tascam US-100 at 48 kHz / 128 x 3; four owned push encoders and three momentary SPST stomps; and the selected NeuralAudio-based NAM candidate. The desktop editor is accepted and the first audible Clouds milestone is achieved. These are no longer implementation starting questions. See [the decision log](07-source-audit-and-decisions.md).

**Still to decide or verify:** final audio/power hardware and numeric latency/boot/resource targets; actual encoder/stomp transport and wiring; the conventional effect in the combined milestone patch; release plugin scope and adaptation work; NAM models, IRs, quality policy and measured budgets; dedicated boot/readiness/shutdown and storage durability; final operation/release acceptance; MIDI's points are settled: only 3.5 mm jacks, and instrument messages also reach the canvas's MIDI IN ([MIDI](39-midi.md#decisions-and-open-questions)).

The active sequence is the **remaining Pi integration checks and saved combined Clouds patch**, followed by physical-control acceptance. Phase 1 debugger evidence is an independent outstanding item. Do not restart Phase 1 or treat completed Pi work as an unplanned side task. Future implementation work must update the owning numbered task, platform/evidence scope and remaining checks in this plan as well as its detailed record.
