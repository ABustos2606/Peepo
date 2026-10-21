# Pedal-inspired LV2 plugins

**Assessment, 14 October 2026.** Effects in the style of the Chase Bliss MOOD MKII and Habit, the Red Panda Tensor and the Hologram Microcosm can all be built as LV2 plugins for the Pi 5 running Ingen. An assessment from Gemini gets the big picture right but oversells the details.

Processing power is not the limit. The real limits are keeping Ingen free of audio dropouts (xruns) and the months of sound tuning that give these pedals their character.

The current catalogue already covers much of the Microcosm and part of the Tensor through Clouds and related modules. It has nothing like Habit's long history buffer or MOOD's shared Clock. Those two are the plugins worth writing.

## Where Gemini is right

- **Feasibility.** These effects mostly come down to a buffer, a few read positions moving through it, crossfades and filters. A Pi 5 (four Cortex-A76 cores at 2.4 GHz) has plenty of headroom for that.
- **Memory.** A 3-minute stereo buffer at 48 kHz in 32-bit float is about 69 MB, which is trivial on 2 GB. Allocate it when the plugin is created, never in the audio callback.
- **Methods.** The right building blocks are:
    - a ring buffer that is always recording;
    - read positions that move at fractional speeds, with interpolation;
    - Hann-windowed grains and crossfaded jumps;
    - a simulated slower clock (hold each sample, then low-pass filter).
- **Frameworks.** DPF and Faust suit plugins without a UI, which is how Ingen uses them. Faust ships a Dattorro reverb and fractional delay lines. JUCE works but is heavier than needed.
- **Modularity.** In Ingen these plugins can be patched together in ways the hardware can't, such as a pitch shifter inside a delay's feedback loop.

## Where to push back

1. **The CPU numbers are invented.** "3–5%" and "all four at once with 70% free" are guesses. What the Pi struggles with is missed audio deadlines, not average load. The bring-up log already shows `JackEngine::XRun: client = ingen was not finished`. Ingen runs the whole patch as one JACK client, so a dense patch is limited by whether the audio keeps running without dropouts.
2. **"Easy" is misleading.** The code for Habit is simple. The feel of these pedals comes from months of tuning crossfade shapes, randomness, control response and filtering. Gemini admits this only for the Microcosm, but it applies to all four.
3. **Signalsmith Stretch "in 25 lines".** The library is real and MIT-licensed. With its default settings it adds tens of milliseconds of delay or more, which feels sluggish under a guitar. It can be tuned for less delay at the cost of more artifacts.
4. **Doubtful claims:**
    - Luppp and SooperLooper are standalone JACK apps, not LV2 plugins.
    - Beebo's looper modules (`loop_common_in/out`) have no public source.
    - "ChowMatrix compiles cleanly to ARM64 LV2" is optimistic.
    - The hardware details (Spin FV-1 chips, the 2–64 kHz clock range, which collaborator designed which side of the MOOD) look muddled. Verify them before relying on them.
5. **Missed: buffers are not saved with patches.** The Clouds tests ([20-clouds.md](20-clouds.md)) note that Clouds has no LV2 state, so a frozen buffer is lost on reload. A frozen MOOD loop or Habit's 3-minute history would be lost the same way unless the plugin saves LV2 state. Saving 69 MB per patch would be a bad idea anyway.
6. **Missed: control smoothing.** Ingen delivers control values once per audio block. Scan and speed controls must glide between values, or they will produce zipper noise.

## What the catalogue already covers

The Microcosm is mostly covered already, and Habit not at all. This table was checked against `config/catalogue/overlay.json`, `config/catalogue/inventory.txt` and `docs/evidence/`.

| Pedal | Already covered by existing modules | Still missing |
| --- | --- | --- |
| Microcosm | Most of it: Clouds (granular), Clouds Looper, Clouds Stretch, Oliverb, Beat Repeat (few controls work), avw Granulator, MVerb, convolution reverbs, the filters. Envelope follower and clock divider modules provide motion and triggering. | Rhythmic slicing modes (Mosaic, Seq) and the polish of its presets. Least worth rebuilding. |
| Tensor | Part of it: Clouds Stretch for pitch and time, TAP Pitch Shifter, TAP Reflector for reverse, toggle and sample-and-hold modules for random changes. | Instant hold, stutter and reverse in one plugin with low delay. Separate modules patched together won't feel the same. |
| MOOD | The pieces. Clouds Looper's freeze loops whatever is already in its buffer, which is close to MOOD's always-listening looper. Also the envelope follower for Env mode, Oliverb/MVerb, and the avw Downsampler and mda Degrade for grit. | The Clock. One knob slows the delay, reverb and loop together. All Ingen plugins share one sample rate, so the Clock must live inside one plugin. |
| Habit | Nothing. mda Delay and DubDelay have short buffers. | The 3-minute memory you can scan through, the second (Spread) playback position, Collect (the memory overdubs itself) and FEED (the output is recorded back into the memory). |

## Recommendation

Build what doesn't exist rather than redoing what Clouds already does well.

1. **Habit-style plugin first.** It needs a long memory with a Scan control, a Spread playback position, a separate echo after it, and the IN / OUT / FEED routings. It is simple to code, and nothing in the catalogue resembles it. Give it a send and a return so existing modules (TAP Pitch, Degrade, filters) can be patched into its feedback loops in Ingen. That is the modular version of Habit's modifiers. Ingen does not allow a loop back into the same plugin, so the send and return must be two separate plugins (see [Discipline](#discipline-the-habit-style-plugin)).
2. **MOOD-style plugin second.** An always-listening micro-looper plus a small reverb/delay, sharing one internal Clock. The shared Clock is the point of the pedal.
3. **Skip Tensor and Microcosm clones for now.** Make Clouds-based presets instead.

Describe the plugins as "inspired by" these pedals, and don't copy the real pedals' names, presets or look.

## Discipline: the Habit-style plugin

**Update, 15 October 2026.** The Habit-style plugin is called Discipline. It is the first LV2 plugin written for Peepo.

**Update, 16 October 2026.** Discipline has its own repository (`C:\LV2-own-plugins\discipline`, to be published as `https://github.com/ABustos2606/discipline`). Its spec, including the summary of the Habit manual, moved to that repository's `docs/design.md`. The main decisions:

- **Its own repository.** Peepo builds it at a pinned commit, like every other plugin, and updates on tagged releases. It can be tested in any LV2 host without Ingen or the Pi, and its license is chosen separately from Peepo's GPLv3.
- **URI** `https://github.com/ABustos2606/discipline#`, not Peepo's namespace. No "Habit" or "Chase Bliss" in names or descriptions.
- **Two plugins, Discipline and Discipline Return,** sharing one buffer. Ingen rejects connections that close a loop (`COMPILATION_FAILED`, `reference/upstream/ingen/src/server/events/Connect.cpp:137`), so the send and return of one plugin can't be patched through other modules. The return writes into the echo's feedback loop (IN), into the memory (FEED) or into the output (OUT). **Update, 17 October 2026, after the first Pi test:** the echo now runs inside Discipline and works alone; the Return is used only when Discipline's Modifier is Outside, and the Pair control is gone (one pair per patch). See Discipline's `docs/design.md`.
- **CV inputs instead of Habit's ramp engine.** Auto-scan is still built in.
- **Patch tempo** reaches it through a BPM control, as for Beebo's Delay ([29-parameters-files-tempo.md](29-parameters-files-tempo.md)).

What stays in Peepo:

- its entry in `config/catalogue/overlay.json`;
- the build step that fetches and builds it at a pinned commit;
- patches that use it, and xrun measurements;
- this document and any test notes.

## Next steps

- [x] Get the Habit manual and confirm the controls and behaviours (summarised in Discipline's `docs/design.md`).
- [x] Create the Discipline repository folder.
- [x] Publish the repository and choose its license.
- [x] Discipline milestones 1 to 3 (in Discipline's `docs/design.md`).
- [x] Add it to `config/catalogue/overlay.json`, with its ports laid out in the overlay format.
- [x] Build it in the WSL environment, then on the Pi, through a pinned build step in Peepo.
- [x] Measure xruns on the Pi with it in a typical patch.

## Aura: micro-looper and shared-clock effects

Aura has its own private repository at https://github.com/ABustos2606/aura. It combines an always-listening Env/Tape/Stretch micro-looper with Reverb, Delay and Slip, at one internal Clock. Its source pin is in the build script and reference manifest. Copyright (c) 2026 Alejandro Bustos. All rights reserved; no license is granted. Access to the private repository is required.

The overlay uses port bypass and patch BPM. Aura has a page of its own on two tabs ([Module rail, Aura's and Discipline's pages](40-module-rail-and-aura.md)): the six main controls, three modes and four stomps on the first, every other option on the second. Stomps exposes Wet on, Freeze, Loop on and Overdub for patch-specific footswitch assignments. With Latch off, Freeze and Overdub expect held gates; with Latch on, rising presses toggle. Six additive CV connectors replace hardware ramping.

Pi listening, native arm64 tests and a 30-minute typical-patch xrun/CPU/RSS record are still required in docs/evidence/ before release acceptance.
