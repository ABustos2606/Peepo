# Roadmap: Drizzle, Chamber, Slope and Reel

**Planned 19 October 2026.** Peepo will have four new plugins of its own, ported from Emilie Gillet's eurorack code. Poly's `lenticular_lv2` has none of them.

| Peepo plugin | Ported from | What it is |
| --- | --- | --- |
| **Drizzle** | Beads | Granular processor and delay with built-in reverb; also a wavetable synth |
| **Chamber** | Elements | Modal synthesis voice: bow, blow and strike exciters into a resonator, with reverb; external audio can excite it |
| **Slope** | Stages | Six-segment generator: envelopes, LFOs, step sequences, chained |
| **Reel** | Frames | Keyframe mixer: four channels whose levels morph between stored keyframes; four-output LFO mode |

## Where they live

Like [Discipline](26-pedal-inspired-plugins.md#discipline-the-habit-style-plugin) and Loo-pi, **each plugin is its own repository**, not part of Peepo:

| Plugin | Folder | Repository | URI prefix |
| --- | --- | --- | --- |
| Drizzle | `C:\LV2-own-plugins\drizzle` | `https://github.com/ABustos2606/drizzle` | `https://github.com/ABustos2606/drizzle#` |
| Chamber | `C:\LV2-own-plugins\chamber` | `https://github.com/ABustos2606/chamber` | `https://github.com/ABustos2606/chamber#` |
| Slope | `C:\LV2-own-plugins\slope` | `https://github.com/ABustos2606/slope` | `https://github.com/ABustos2606/slope#` |
| Reel | `C:\LV2-own-plugins\reel` | `https://github.com/ABustos2606/reel` | `https://github.com/ABustos2606/reel#` |

**20 October 2026:** the four folders exist, each a local git repository (not yet committed or published) with its README, `CLAUDE.md`/`AGENTS.md`, MIT `LICENSE`, `docs/design.md` (the spec) and `docs/implementation-plan.md`; Reel also has `docs/peepo-page.md`. No code yet.

Each repository follows Discipline's layout: `Makefile` (with `OPT`, `LV2DIR` and `install`), `src/`, the `.lv2` bundle's Turtle files, `tests/` with a standalone host so it runs without Ingen or the Pi, `docs/design.md` as the spec, `CLAUDE.md`/`AGENTS.md`, and an MIT `LICENSE`, the same license as the upstream code. Each vendors the upstream files it uses, with their notices, so it builds on its own.

What stays in Peepo, as for Discipline:

- the build step in `tools/ingen-spike/14-more-plugins.sh` that fetches and builds each repository at a pinned commit (`third <name> <url> <commit> ...`, `-mcpu=cortex-a76` on the Pi);
- the URIs in `config/catalogue/inventory.txt` and `tools/catalogue-inventory.py`;
- the entries in `config/catalogue/overlay.json`;
- patches that use them, measurements on the Pi, and this roadmap.

## What the code says

Read from the upstream repository `pichenettes/eurorack` at `f4cfa7697095a28d292bff758beb51deb49b5a86` (12 October 2026, the first commit with Beads), and from the older copies under `reference/upstream/lenticular_lv2/eurorack/`.

| | Drizzle (Beads) | Chamber (Elements) | Slope (Stages) | Reel (Frames) |
| --- | --- | --- | --- | --- |
| Internal rate | 48 kHz, the same as Peepo | **32 kHz, a compile-time constant** (`dsp/dsp.h`) | **31.25 kHz, a compile-time constant** (`segment_generator.h`) | Control rate, no audio DSP |
| Memory | 768 KB delay buffers + 64 KB reverb | 64 KB reverb | Small | Small |
| Hardware-specific code in the DSP | None found in `beads/dsp/` | No | No | No |
| State to keep in a patch | Settings only (the buffer is lost, like Mist) | Settings only | Each segment's type and loop flag | **The keyframes**: LV2 state |
| Size of the DSP | ~5,800 lines (`dsp/`) | ~3,700 lines (`dsp/`) | ~1,500 lines (segment generator, ramp extractor, oscillator) | ~650 lines (keyframer, LFO) |
| Main difficulty | Mapping the hardware's button gestures (quality, mono, seed, synth mode) to controls | Resampling 48 <-> 32 kHz, and CPU | Grouping segments without "is this gate patched" | Editing keyframes from the touchscreen |

Proven in Peepo already: Ingen saves and restores LV2 state on the Pi ([implementation plan](10-technical-implementation-plan.md), 16 October 2026), modules can expose CV inputs and outputs, and overlay groups split many controls into pages.

## Order

1. **Drizzle**: the most wanted, and the cleanest port: native 48 kHz, no hardware code, a single `Processor` call.
2. **Chamber**: the most valuable as a guitar effect (guitar into the resonator), but needs a resampler and a CPU budget.
3. **Slope**: no audio, no state beyond its controls; mainly control design.
4. **Reel**: the smallest DSP but needs LV2 state and the most interface work.

Each plugin is finished, in its repository and in Peepo on the Pi, before the next one starts.

## Every plugin, the same steps

In its repository:

- [ ] Create the repository from Discipline's layout; vendor the upstream DSP and the current `stmlib` (it differs from Lenticular's copies in 18 files), with the MIT notice.
- [ ] `docs/design.md`: controls, ports, CV ranges, block size, what is and is not ported.
- [ ] Wrapper: split the host block into the firmware's block size; CV and gate handling consistent with Peepo's other Gillet-derived modules (read Lenticular's ports); no x86-only flags.
- [ ] Tests in its own host: renders, no NaN or denormals, controls and state restore, real-time safety (no allocation in `run`).
- [ ] Tagged commit to pin.

In Peepo:

- [ ] `third` build step in `14-more-plugins.sh` at the pinned commit, built on x86-64 (WSL) and arm64 (Pi); `ldd` clean.
- [ ] URIs in `inventory.txt` and `catalogue-inventory.py`.
- [ ] Engine check on the PC and on the Pi (load, sound or CV out, save and reload, no xruns).
- [ ] CPU measured on the Pi with `tools/pi/measure-effect.py`, at 48 kHz and 128 frames, alone and in a typical patch.
- [ ] Overlay entry: name, category, description (what it does, ASCII, no maker), parameter names, kinds, units, groups and pages, `output` level control.
- [ ] Pi catalogue exported; `docs/33-module-list.md` regenerated.
- [ ] Simulator check in `tests/ui_check.cpp` where the plugin has something special (custom page, state).
- [ ] Listened to with guitar on the Pi; result recorded under `docs/evidence/`.
- [ ] Row in [third-party notices](../THIRD_PARTY_NOTICES.md).

## 1. Drizzle (Beads)

**21 October 2026:** offline work done in Drizzle's repository (milestones 0 to 2: `docs/milestone-*-validation.md` there); Peepo's build step (pinned at `v0.2.0`), inventory, overlay entries and notice added. **22 October:** built, tested and installed on the Pi, offered in its catalogue, and the user's guitar test passed. Not yet done: the measurements below.

- [x] Wrap `beads::Processor`: delay and reverb buffers allocated in `instantiate`, `Process` in whole 1/4000 s blocks whatever Ingen's slices, stereo in and out. Dirty tape runs at 24 kHz and vintage digital at 32 kHz, as on the hardware, resampled around the processor; latency 95 frames (2 ms) at 48 kHz in every quality, reported to the host.
- [x] Controls: Time, Size, Shape, Pitch (semitones), Density, Feedback, Dry/wet, Reverb; Freeze switch; Seed as a trigger; random amount per parameter (Beads' attenurandomizers); Quality as an enumeration of its four media (switching fades out, clears the buffer over 16 ms and fades in); Mono switch; Auto gain and Input gain; On (`lv2:enabled`, `"bypass": "port"`: tails ring out).
- [x] CV inputs for the main parameters, the Assign CV, seed and freeze gates, pitch in V/oct; a Grain out CV output (trigger or gate).
- [x] The button gestures and "is a cable in" cases are explicit controls: Seed lock, the four "uses CV" switches, Seed uses gate, CV assign to, Gate out, Mono.
- [x] **Drizzle Synth** (decided 20 October 2026): Beads becomes a synth when its inputs are unplugged, which an LV2 plugin cannot see, so the synth is a second plugin in the same repository, a wavetable sound source.
- [x] Overlay: Drizzle under Texture (27 controls in seven pages: Grain, Density and mix, Play, Modulation, CV use, Input and output, CV assign and out), Drizzle Synth under Sound source (24 controls in six pages). Checked with the exporter and `peepo_catalogue_tests` under WSL.
- [ ] CPU and xruns on the Pi (`make test-timing` there, then `tools/pi/measure-effect.py` alone and in a typical patch, Density at maximum, every quality, with a Drizzle Synth).
- [x] Listened to with guitar on the Pi (22 October 2026): passed, no default changed. Record in Drizzle's `docs/milestone-3-validation.md`.
- [x] Pi catalogue exported (22 October 2026: 336 -> 338 modules, only Drizzle's two added).
- [x] `docs/33-module-list.md` regenerated (22 October 2026, from that export: 358 identities, 338 installed).
- [ ] Not in scope: saving the captured buffer in the patch (possible later with LV2 state, about 1 MB per instance).

## 2. Chamber (Elements)

**21 October 2026:** offline work done in Chamber's repository (milestones 0 to 2: `docs/milestone-*-validation.md` there); Peepo's build step, inventory and overlay entry added. Not yet done: the Pi checks below.

- [x] **Resampling** (decided 20 October 2026). Elements' tables, filters and exciter samples assume 32 kHz, so Chamber resamples 48 -> 32 -> 48 kHz around the unchanged voice. Latency, reported to the host: 88 frames (1.83 ms) at 48 kHz.
- [x] Wrap `elements::Part` with its reverb buffer; inputs: gate, V/oct, strength, external blow and strike audio; outputs: main and aux.
- [x] 33 controls and 14 CV inputs, in eight overlay pages: Resonator, Input and output, Play, Strike, Bow and blow, Pitch, and the CV amounts for exciter and resonator.
- [x] Guitar use: external audio into the strike input, with a level control, tested offline as an effect (it rings at the resonator's pitch with no gate). Blow in sounds only while Play or a gate is held, as on the module.
- [x] **Decision:** the Ominous mode is a switch in Chamber, not a second plugin, offered only if CPU allows. The overlay hides it until the Pi measurement says it fits; on a desktop it costs 1.8 times the worst modal case.
- [ ] CPU and xruns on the Pi (`make test-timing` there, then `tools/pi/measure-effect.py` alone and in a typical patch), with and without the Ominous voice.
- [ ] Listened to with guitar on the Pi; defaults tuned for guitar use (Damping, input levels); result in `docs/evidence/`.
- [x] Pi catalogue exported; `docs/33-module-list.md` regenerated (22 October 2026, with Drizzle's export: Chamber listed under Resonator, Ready).

## 3. Slope (Stages)

**20 November 2026:** offline work done in Slope's repository (milestones 0 to 2: `docs/milestone-*-validation.md` there). **21 November 2026:** Peepo's build step (pinned at `13b8d63`, milestone 2, until `v0.2.0` is tagged), inventory, overlay entry and notice added, checked under WSL with the exporter and `peepo_catalogue_tests`; built, tested and installed on the Pi and offered in its catalogue; two Slopes in a typical patch for 30 minutes with no xruns ([evidence](evidence/slope-pi-20261121.txt), Slope's `docs/milestone-3-validation.md`). Not yet done: the user's playing test.

- [x] **Rate** (decided 20 October 2026). Its timing and two lookup tables assume 31.25 kHz. Slope regenerates the tables for 44.1, 48 and 96 kHz with a Python 3 port of upstream's generator and sets `kSampleRate` at instantiate; outputs are CV, so no resampling.
- [x] Wrap `stages::SegmentGenerator` for six segments: per segment a level/time slider, a shape control, a type (ramp, step, hold, alt) and a loop switch, as ordinary controls. Patches then keep the configuration without LV2 state.
- [x] Grouping: on hardware, a patched gate input starts a new group of segments. Slope adds a "Starts group" switch per segment, and a Trigger per segment so a stomp can play it.
- [x] Six gate inputs, six CV inputs, six CV outputs.
- [x] The harmonic oscillator (`ProcessOuroboros`) is included (decided 20 October 2026), behind a switch. Not in scope: chaining several instances (`chain_state.cc`).
- [x] A clock (BPM, Clock note, Clock 1 to 6) follows the patch tempo (`"tempo": "bpm"`).
- [x] Overlay: Slope under Control, `"bypass": "port"`; the page settled by mockup (21 November 2026): a "Tempo and mode" heading (Harmonic, BPM, Clock note), then one heading per segment (Type, Loop, Level/time, Shape/glide, Starts group, Trigger, Clock), 45 controls. Not a page per pair of segments, as first planned.
- [x] On the Pi (21 November 2026): built natively, `make test` 31 PASS, `make test-timing` under 1% of a core per instance; catalogue exported (255 -> 256, only Slope added).
- [x] CPU and xruns with two Slopes in a typical patch (IN -> Overdrive -> delay -> Mist -> OUT) for 30 minutes: 0 xruns, DSP load 7.3% mean against 2.3% without them, power flags 0x0.
- [x] `docs/33-module-list.md` regenerated from the Pi's export (21 November 2026).
- [ ] Played with Peepo's CV modules and stomps (the user).

## 4. Reel (Frames)

**21 November 2026:** milestones 0 to 2 done offline in Reel's repository (`docs/milestone-*-validation.md` there): keyframes, LV2 state, the sequencer, Reel LFO (with a BPM control that follows the patch tempo), and the keyframe list for the page on an atom output, checked through Ingen. Peepo side: overlay entries (Reel in Utility, Reel LFO in Control with `"tempo": "bpm"`), the inventory, `watch_reel` in the Ingen adapter, and the Reel page built from the mockup approved that day (`images/ui-reel.png`; `ui_check` captures `99-reel-*`, `ingen_tests` `reel_monitor`). **21-22 November 2026, on the Pi:** installed, `make test` 28 PASS natively, catalogue exported (256 -> 258, only Reel and Reel LFO added; `evidence/catalogue-pi-20261121-reel.json`). 30 minutes with Reel and Reel LFO in an Overdrive / delay / Mist patch: 0 xruns, DSP load 4.4% mean, 9.2% max (2.5% without them); the keyframes came back after a reboot ([evidence](evidence/reel-pi-20261121.txt)). Build step pinned at `v0.2.0` (`143488d`). Still to do: see the page on the Pi with the next interface install.

**22 November 2026, Reel milestone 3** (its `docs/milestone-3-validation.md`): Reel 0.3 adds a `commands` atom input (port 41) to move a keyframe. The page drags a keyframe by its numbered tag (a move per step, then 0, since Ingen saves the last value and sets it again on load). Dragging a lane up or down on the editing target's mark sets that level. Two stomp actions, **Next keyframe** and **Previous keyframe** (Settings > Controls, under the Reel), set Frame onto the keyframe after or before it, wrapping; the adapter keeps those Reels' lists watched with the page closed (`watch_reel_lists`, `reel_keyframes`, `reel_command`). `ingen_adapter`, `physical_controls` and `ui_workflows` pass on Windows. Still to do: pin Reel at `v0.3.0` once tagged, and try the page and stomps on the Pi by touch and by ear.

- [x] Wrap `frames::Keyframer` and `frames::PolyLfo`: frame position as a control and CV input, four channel levels as CV outputs, and four audio VCAs plus a mix output.
- [x] **LV2 state** for the keyframes (up to 64, each with four values and an easing curve), saved and restored by Ingen.
- [x] Editing: Add and Delete as momentary controls; the four channel controls edit the keyframe the position is on, as on the hardware.
- [x] A custom module page in Peepo, like the Euclidean one, showing keyframes along the position axis; layout accepted on 20 October 2026 (Reel's `docs/peepo-page.md`). This is the largest piece of interface work in the roadmap.
- [x] Two plugins in the repository: Reel (keyframe mixer) and Reel LFO.
- [x] Installed on the Pi, catalogue exported; xruns measured and keyframes checked after a reboot.
- [ ] Milestone 3: keyframes moved and levels set by touch, Next and Previous keyframe on stomps (built and tested on Windows, 22 November 2026); pinned at `v0.3.0` and tried on the Pi.

## Decisions

Settled with the user on 20 October 2026:

| Plugin | Decision |
| --- | --- |
| Drizzle | Synth mode is a separate plugin, Drizzle Synth |
| Chamber | Resample 48 -> 32 -> 48 kHz around the unchanged voice |
| Slope | Regenerate the lookup tables for the host rate; include the harmonic oscillator |
| Reel | The page layout in Reel's `docs/peepo-page.md` |
| Drizzle | Vintage digital runs at 32 kHz with resampling, as on the hardware |
| Chamber | The Ominous voice is offered only if CPU allows, measured on the Pi in milestone 2 |
| Reel | Reel LFO is a separate plugin; easing and response are plain controls |

Settled for Chamber on 21 October 2026: no Dry/wet control (Peepo's mixer modules blend the dry guitar); the Ominous voice is a switch in Chamber, hidden by the overlay until it is measured on the Pi.

Settled for Drizzle on 21 October 2026 (Drizzle's `docs/design.md`, Decisions): dirty tape also runs at its hardware rate (24 kHz); Drizzle has an On port and is bypassed through it; Seed lock starts on and Density at 0.7 so a new Drizzle plays grains.

No decisions are open.
