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

- [ ] Wrap `beads::Processor`: allocate its delay and reverb buffers in `instantiate`, call `Process` in Beads' block size (12 frames in its own test), stereo in and out.
- [ ] Controls: Time, Size, Shape, Pitch (semitones), Density, Feedback, Dry/wet, Reverb; Freeze switch; Seed as a trigger; random amount per parameter (Beads' attenurandomizers); Quality as an enumeration of its four media (`Configure` switches them while running); Mono switch.
- [ ] CV inputs for the main parameters, seed and freeze gates, pitch in V/oct.
- [ ] Read `beads.cc` and `ui.cc` for the button gestures (seed held = latched, clocked or gated grain triggers; media + seed = mono) and make each one an explicit control.
- [ ] **Drizzle Synth** (decided 20 October 2026): Beads becomes a synth when its inputs are unplugged, which an LV2 plugin cannot see, so the synth is a second plugin in the same repository, a wavetable sound source.
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
- [ ] Pi catalogue exported; `docs/33-module-list.md` regenerated.

## 3. Slope (Stages)

- [ ] **Rate** (decided 20 October 2026). Its timing and two lookup tables assume 31.25 kHz. Slope regenerates the tables for 44.1, 48 and 96 kHz with a Python 3 port of upstream's generator and sets `kSampleRate` at instantiate; outputs are CV, so no resampling.
- [ ] Wrap `stages::SegmentGenerator` for six segments: per segment a level/time slider, a shape control, a type (ramp, hold, step) and a loop switch, as ordinary controls. Patches then keep the configuration without LV2 state.
- [ ] Grouping: on hardware, a patched gate input starts a new group of segments. Slope adds a "starts group" switch per segment.
- [ ] Six gate inputs, six CV inputs, six CV outputs; one overlay page per pair of segments.
- [ ] The harmonic oscillator (`ProcessOuroboros`) is included (decided 20 October 2026), behind a switch. Not in scope at first: chaining several instances (`chain_state.cc`).

## 4. Reel (Frames)

- [ ] Wrap `frames::Keyframer` and `frames::PolyLfo`: frame position as a control and CV input, four channel levels as CV outputs, and four audio VCAs plus a mix output.
- [ ] **LV2 state** for the keyframes (up to 64, each with four values and an easing curve), saved and restored by Ingen.
- [ ] Editing: Add and Delete as momentary controls; the four channel controls edit the keyframe the position is on, as on the hardware.
- [ ] A custom module page in Peepo, like the Euclidean one, showing keyframes along the position axis; layout accepted on 20 October 2026 (Reel's `docs/peepo-page.md`). This is the largest piece of interface work in the roadmap.
- [ ] Two plugins in the repository: Reel (keyframe mixer) and Reel LFO.

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

No decisions are open.
