# Clouds

**Recorded:** 14 October 2026, on the Windows PC with the engine under WSL2. **Scope:** P6.1 (build record), P6.2 (binary and host check), P6.3 (parameter overlay), and the part of P6.4 that test signals can do. **Verified with:** the patched Ingen of [the engine evidence](14-ingen-engine-evidence.md), JACK's dummy backend at 48 kHz and 128 frames, and generated signals. Recorded output: [evidence/clouds-results.txt](evidence/clouds-results.txt). **Not covered:** the Pi, a guitar or any listening, processor load, the milestone patch (P6.5) and combined load (P6.6).

Poly's `polyclouds` library is Mutable Instruments' Clouds with the Parasites firmware, built as seven LV2 plugins, one per mode. All seven load, process sound and restore their controls in Ingen. The overlay now presents each mode with its own control names, switches for freeze and reverse, whole-semitone pitch, and the controls that matter on the first page.

## Build record (P6.1)

| Part | Revision |
| --- | --- |
| `lenticular_lv2` (Poly) | `141fdc89e250987bae421664132ddaedae735107`, as pinned in [the manifest](../reference/manifest.json) |
| `parasites/stmlib` (submodule; Clouds' DSP is in-tree under `parasites/clouds`) | `8ab2aaee77cbacb47b646d46d22ee5d358effe2d` |
| `eurorack` and `eurorack/stmlib` (not used by Clouds) | `99432f2b…`, `d9b18967…` |

`tools/ingen-spike/02-plugins.sh` clones these fresh, so nothing under `reference/` changes. The build differs from upstream in two ways, both given on the command line:

- **The SSE flags.** The makefile's flags target x86 SSE. For the Pi 5 they are replaced by `-mcpu=cortex-a76` with the same optimizations, and `tools/pi/build-arm64.sh` builds it that way.
- **A stray command in the polyrings.ttl rule.** It is bypassed by copying that one file in first.

| Binary | Machine | Needs |
| --- | --- | --- |
| x86-64 (development engine) | X86-64 | `libstdc++`, `libm`, `libc` |
| arm64 (Debian 12 root, emulated) | AArch64 | the same three, all resolved in that root |

Neither binary has any other undefined symbol. The arm64 binary has not run on a Pi.

## Host check (P6.2)

All seven modes declare 4 audio ports, 12 CV inputs and 13 controls. Each requires `urid:map`, which Ingen provides, and offers `hardRTCapable`. None has LV2 state, a worker or a file parameter.

| Finding | Evidence | Consequence |
| --- | --- | --- |
| Silent until switched on | Its `enabled` control at 0 writes silence; 0.000000 RMS measured | The adapter sets it when adding the module and keeps it set ([catalogue export](16-catalogue-export.md)). Bypass is the engine's own block bypass, which passes the input through unchanged (measured equal to a direct cable). A patch saved when bypass cleared the control still loads as bypassed, and enabling it turns the control back on. |
| **In gain does nothing** | `in_gain_param` is read, but its use is commented out upstream; 0.0624 RMS out at 0 and at 1 | Hidden by the overlay |
| Freeze and reverse are switches | The plugin acts only at 1.0 (or a CV above 0.4); 0.6 did not hold | Shown as switches |
| Blend 0 was dry, but 9 dB down, and not dry with reverb | 100% of the output was the input tone, at 0.0624 RMS for 0.1768 in (−9.1 dB): Clouds' crossfade gives the dry path 0.707 and its output conversion halves everything (the module's analogue stage makes that up). Its reverb comes after the blend, so at reverb 1 the dry path was pulled down further (−11/−17 dB on the Pi) and reverb added. | **Fixed by `tools/plugin-patches/clouds-blend-and-output.patch` (16 October 2026).** The wrapper runs Clouds fully wet and mixes the untouched input itself, like a pedal's mix knob: dry at full level up to blend 0.5, falling to nothing at 1; Clouds' sound rising to full by 0.5, doubled back to the input's level. Resonestor keeps the original behaviour (its blend is distortion). The same patch adds an **Output** control to every mode, -24 to +12 dB, for the level of the whole mix: dry plus Clouds at blend 0.5 is above unity ([Output results](evidence/pi-effect-results.md#clouds-output)). Measured on the Pi: blend 0 equals the input (within 7e-13 at reverb 0; 1e-4 at reverb 1, the end of the control's smoothing from the previous setting); 0.5 is full dry plus Clouds at −1 to −3 dB; 1 leaves Clouds alone, about 0 dB. [Results](evidence/pi-effect-results.md#clouds-blend). |
| Input clamps at full scale | The plugin converts to 16 bits; a 1.5 input came out no louder than 0.99 did | None needed |
| Pitch is exact | Wet only, a 440 Hz input came out at 439.7, 884.1, 216.5 and 662.1 Hz for 0, +12, −12 and +7 semitones | Whole semitones in the overlay, as on Beebo |
| Freeze holds the buffer | Input stopped: 0.0669 RMS a second later with freeze on, silence with it off | — |
| Values survive a bundle | 11 changed controls and the on switch saved and loaded back exactly, and the reloaded patch is heard | — |
| The buffer does not survive | Clouds has no LV2 state, so a frozen sound is not saved with the patch | Saved patches restore settings, not captured audio |
| Runs at the host rate | The DSP is the firmware's, which ran at 32 kHz with a fixed buffer | Grain and buffer times are shorter than on the module (inference from the source, not measured) |

**Which controls change the sound in each mode.** Each control was moved from one end to the other, with noise and with gated tones as input. It counts when the change exceeds twice the spread between three takes at the same settings. This is a coarse screen, not a listening test. Granular output is random, so its threshold is high.

| Mode | Clearly changes the sound | No clear change in this screen |
| --- | --- | --- |
| granular | position, pitch, density, texture | size, blend, spread, feedback, reverb |
| stretch | pitch, density, blend, feedback, reverb | position, size, texture, spread |
| looping_delay | position, size, pitch, blend, feedback, reverb | density, texture, spread |
| spectral | pitch, density, texture, blend, feedback, reverb | position, size, spread |
| oliverb | pitch, density, texture, blend | position, size, spread, feedback, reverb |
| resonestor | everything except blend | blend |
| beat_repeat | reverb only | everything else |

**Beat repeat needed adaptation.** The firmware derives its beat-repeat settings from the knobs: probability, clock divider, pitch mode, slice selection. The LV2 port never set them, so they stayed at zero and the mode responded only to reverb; nor did anything clock it unless a trigger was cabled. Beebo does not offer it either. Fixed on 21 November 2026, see [Beat repeat](#beat-repeat). Spectral is not offered by Beebo either; it works, and its names follow the firmware source.

Spread acts between the two channels, which the screen measures only weakly.

## Parameter overlay (P6.3)

The overlay (`config/catalogue/overlay.json`) can now present individual controls. The rules are in [catalogue.hpp](../src/catalogue/catalogue.hpp) and tested in `peepo_catalogue_tests`. A per-module rule may list controls, each by symbol, and give it:

- a name;
- a unit;
- a kind: continuous, whole numbers, or an on/off switch;
- a default inside the plugin's range;
- or hide it.

Listed controls come first, in the order listed, and the rest keep theirs. The overlay cannot change a range: the engine neither reports nor checks ranges, so it stays the plugin's.

A symbol the module lacks, a switch over a range other than 0–1, a default outside the range, or an attempt to invent choices refuses the whole overlay. A chosen default is set by the adapter when the module is added, because the plugin starts at its own.

For Clouds the overlay:

- hides In gain;
- makes freeze and reverse switches that start off;
- makes pitch whole semitones from 0 (upstream's default is 0.1, a slight detune);
- names each mode's controls as the Parasites firmware does, following Beebo's labels where Beebo offers the mode;
- orders the controls so the first page of four holds the ones that shape the sound and the second holds pitch, blend and freeze;
- puts Peepo's Output (added by the plugin patch, -24 to +12 dB) right after Blend, so the mix and its level sit together.

| Mode | Shown as | Page 1 | Page 2 |
| --- | --- | --- | --- |
| granular | Clouds | Position, Size, Density, Texture | Pitch, Blend, Output, Freeze |
| stretch | Clouds Stretch | Position, Size, Pitch, Blend | Output, Diffusion, Filter, Freeze |
| looping_delay | Clouds Looper | Tape length, Pitch window, Pitch, Blend | Output, Diffusion, Filter, Loop |
| spectral | Clouds Spectral | Position, Warp, Refresh, Quantize | Pitch, Blend, Output, Freeze |
| oliverb | Oliverb | Pre-delay, Size, Decay, Damp | Pitch, Blend, Output, Hold |
| resonestor | Resonestor | Timbre, Chord, Decay, Filter | Pitch, Harmonics, Scatter, Switch voice |
| beat_repeat | Mist Beat Repeat | Chance, Slice length, Loop size, Loop start | Pitch, Pitch mode, Repeat, Output |

The remaining controls, Reverse and Output among them where not listed above, follow on page 3. Resonestor's Output comes after Random mod there.

The remaining controls follow on a third page. The generic page is the Clouds editor for now. The interface's custom Clouds page still serves only the simulator's demo module; the plan makes a custom editor optional.

## Beat repeat

**21 November 2026.** Peepo's Clouds patch (`tools/plugin-patches/clouds-blend-and-output.patch`) now makes beat repeat work. Poly's Turtle file already named its controls after the firmware's knob assignments in this mode (`cv_scaler.cc`); the patch sets the firmware's settings from them, as the firmware does:

| Control | Firmware setting | Values |
| --- | --- | --- |
| Chance (`blend_param`) | probability that a clock starts a repeat | 0 to 1 |
| Slice length (`spread_param`) | clock divider: a slice lasts 1, 2, 4 or 8 clocks | choice |
| Pitch mode (`feedback_param`) | Fixed, Reverse, Slows down, Speeds up, Scratch | choice |
| Slice step (`texture_param`) | which earlier slice the next repeat takes: Off, 1, 2, 3, 5, 6, 7, Random | choice |
| Loop shrink (`density_param`) | the loop shortens as the slice plays | 0 to 1 |
| Loop start, Loop size (`position_param`, `size_param`) | the loop inside a slice, quantized; Loop size above the middle plays it back and forth | 0 to 1 |
| Pitch (`pitch_param`) | the repeats' playback speed, 2^(semitones/12); the firmware only slows them, so above 0 changes nothing | semitones |
| Repeat (`freeze_param`) | every clock repeats | switch |
| Reverb (`reverb_param`) | the output reverb. Poly called it Feedback, but the firmware has no feedback path in this mode | 0 to 1 |

The CV inputs add to these as before; Texture's CV input is the firmware's slice selection.

**A clock.** The firmware measures the slice from the interval between triggers, so without a cable it never repeated. Three controls are added after Output, on beat repeat only: **BPM**, which follows the patch tempo (`"tempo": "bpm"`), **Clock note** (1/16 to 1 bar, 1/8 by default), and **Clock from Trigger in**, off by default, which uses the Trigger CV input instead. On the tempo clock a processing block ends where a tick falls, so the firmware measures the clock to the sample at any host period.

**Level.** Between repeats the firmware plays the live input from its buffer, fully wet. Its post gain, crossfade and conversion (1.2 x 0.7071 x 0.5) leave 0.424 of the level, which the patch makes up; Blend's dry path outside Clouds is not used here, as it would double the input. Output sets the level as in the other modes. Reverse is hidden: the mode ignores it.

Checked without an engine by `tools/ingen-spike/30-beat-repeat.sh` ([results](evidence/beat-repeat-results.txt)): unity pass-through; with Repeat on, each 1/8 at 120 BPM plays its first half live and then repeats it, at host periods of 128 and 100; Trigger in clocks it and nothing repeats without a trigger; Chance 0.5 repeated 38 of 76 clocks; the other six modes still run. `peepo_catalogue_tests` passes with the overlay and the updated WSL fixture.

**22 November 2026, on the Pi:** installed with package `peepo-arm64-20261122-bac75f6-dirty`; the installed `polyclouds.ttl` has the clock controls, and the Pi's catalogue offers Mist Beat Repeat as Ready ([export](evidence/catalogue-pi-20261122.json), [module list](33-module-list.md)). CPU and xruns measured the same day ([evidence](evidence/eurorack-pi-20261122.txt)): in IN -> Overdrive -> delay -> Beat Repeat -> Mist -> OUT, clocked at 1/16 and 120 BPM, Chance 0.5, every pitch mode and Repeat on and off, `tools/pi/load-check.py` for 10 minutes gave 0 xruns, DSP load 2.7% mean and 3.3% max (2.3% without it), power flags 0x0. Not yet done: listening with guitar.

## Run it

Inside Debian (WSL), as root, after `02-plugins.sh`:

```bash
bash tools/ingen-spike/11-clouds.sh
```

It prints the build record and both binaries (with `PEEPO_ARM64_ROOT` naming the arm64 root, default `/opt/peepo-arm64`), then runs `11-clouds.py` against a fresh engine. That takes about 15 minutes, most of it the per-mode screen.

## Still open

**First musical milestone, 14 October 2026:** the updated package runs on the Pi and the user confirmed guitar listening through Clouds ("it's working!"). The user's patch has also been saved and reloaded on the Pi, and the user heard no interruption while only the interface restarted during playing; the monitored audio had no gaps or xruns. See [Pi bring-up](23-pi-bring-up.md). The original Freeze-on settings remain as revision 1; revision 2 has Freeze off for live input. The captured buffer is not saved. A complete listening sweep of the modes and controls, and the formal Clouds-plus-conventional-effect milestone patch, are still separate work.

- **P6.1, P6.2 on the Pi:** now checked in the [Pi update](23-pi-bring-up.md#todays-package-brought-to-the-pi). The six offered modes load and their controls and bypass pass native checks; granular Clouds also processes stereo test audio. The guitar listening milestone remains pending.
- **P6.4.** Needs live guitar input, listening, and the Pi's levels. The checks here establish only that the DSP responds as the source says.
- **P6.5 and P6.6.**
