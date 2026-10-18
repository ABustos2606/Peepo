# Effects and the instrument tuner

**Full reference:** [complete module list](33-module-list.md), grouped by category
with every module's description, connections and availability. The [GxPlugins
table](32-gxplugins.md#all-43-gxplugins) lists all 43 installed Gx effects.

The Add list contains upstream LV2 modules. Addable means the installed
engine can instantiate a module and the interface can control and connect
it. It does not mean every effect has passed guitar listening tests on this
Pi. Clouds remains the guitar-listening milestone verified by the user.
Windows without `--engine` is a simulation and makes no sound.

## What the groups mean

| Group | Purpose | Installed examples |
| --- | --- | --- |
| Drive | Saturation, distortion and lo-fi processing | Gain, Overdrive, Bandisto, Degrade, Tubewarmth, Sigmoid Booster, SD-1, Guvnor, Bottle Rocket, the Gx fuzzes, downsamplers |
| Amp | Conventional amplifier simulations | Combo, Bass amp (SVT), Power amp (Cream), Power amp (Supersonic), Blue Amp, Plexi, Epic, Vox MKII, Ultra Cab |
| Cabinet | Impulse response cabinet simulation | Cabinet mono, mono-to-stereo and stereo; requires an IR file |
| Dynamics | Controls level, attack and sustain | Compressor mono/stereo, Dynamics, Limiter, MultiBand, Transient, Slow gear |
| Filter | Shapes frequency content | Parametric EQ mono/stereo, RezFilter, diode ladder, Korg 35 high/low-pass, Oberheim; vocoders need two input signals |
| Modulation | Motion and signal combination | Chorus, YK Chorus, flanger, vibrato, harmonic tremolo, Stone phaser, rotary speakers, ring modulation, Warps/Twist modes |
| Pitch | Changes pitch | Clouds Stretch, Detune, RePsycho, TAP Pitch Shifter, SubSynth, Shepard |
| Delay | Echo and looping delay | Tempo delay, Stereo delay, DubDelay, CV delay, Clouds Looper, Discipline |
| Reverb | Acoustic space and sustained tails | Ambience, MVerb, Oliverb |
| Texture | Granulation, freezing and reversal | Clouds, Clouds Spectral, granulators, beat slicers/repeaters, Vinyl, Reverse, Stuck, Power cut |
| Resonator | Resonating instruments played by input | Rings/Resonator, Resonestor |
| Sound source | Generates sound | Plaits, noise, TestTone, BeatBox, Tracker |
| Control | Modulation, gates, triggers and sequences | Grids, Marbles, Tides, LFOs, envelopes, sequencers, clocks, quantizer, envelope follower |
| Utility | Routing, level, arithmetic and analysis | Mixers, VCAs, converters, Tuner |

`(CV)` variants expose modulation connections; mono/stereo variants have
different channel layouts. These are functional differences.

NAM and cabinet/IR reverbs have model and impulse response selectors; see
[file handling](29-parameters-files-tempo.md#model-and-ir-files-p83). The x42 IR
Convolver remains disabled for its worker error. MIDI-played instruments need MIDI routing. Clouds Beat Repeat,
the Poly Moog ladder and Bitcrushed delay are withheld for documented defects.
Clean Boost (`basic_modular#boost`) is hidden from Add as it is a rigid 2×–4×
multiplier completely redundant with Gain (`eg-amp`, -90 dB to +24 dB); its
identity remains recognized for saved patches. For coloured boost, use
Sigmoid Booster or Tubewarmth.
Disabled entries explain their reason. Category counts now read `ready /
total`: ready means addable, not completed listening tests.

The retained Pi export from **18 October 2026**, with the current overlay, has
**216 addable entries**, 10 needing adaptation, 40 unavailable in the current
host and 3 hidden identities (two duplicate tuners and redundant clean Boost):
269 installed identities in total. Another 22 selected reference identities are
not installed. All **43 GxPlugins** are included. See the [complete module
list](33-module-list.md) for every entry and the [technical catalogue
record](22-plugin-catalogue.md) for compatibility evidence and its limits.

## One tuner, with a tuning display

The three x42 identities are `tuna#mod`, `tuna#one` and `tuna#two`: the MOD
build, standard build, and a variant with a spectrum view in its original
plugin interface. Peepo supplies its own LVGL interface, so the spectrum
variant adds no spectrum view here. Only `#mod` appears in Add. The others
remain recognized and usable in saved patches, with the dedicated tuner screen.

Connect **IN → Tuner** and tap the tuner. It shows a large note and octave,
frequency, signed cents, and a moving flat/sharp indicator. The green target
spans ±5 cents. Tighten a flat string; loosen a sharp string. **A reference**
changes the default 440 Hz reference. (17 October 2026: the A reference is now a bar under the meter, and **All** is gone);
**Actions** provides the normal module actions.

Silence, bypass, a disconnected/stalled engine and the audio-free simulator
show no tuned note. Frequency is telemetry, not a saved or editable parameter,
and pitch updates do not rebuild the touchscreen page.

![Tuner showing a flat A4](images/ui-tuner.png)

The host subscribes only to the shown tuner's `freq_out` output. Ingen Get
replies describe initialization values for output controls, so they must not
be used as live pitch. Engine patch 0006 sends explicitly monitored control
readings at 25 Hz, including unchanged values. Patch 0007 terminates standalone
socket notifications as well as outer bundles. Both fixes are applied by the
normal engine build.

## Verification

LVGL workflows cover in-tune, flat, sharp, low E, alternate A reference,
silence, bypass and disconnect, with 800×480 captures. Adapter tests cover
subscriptions, stale readings, initialization values and leaving the tuner.
Catalogue tests check exactly one offered tuner and three retained identities.

`tools/ingen-spike/17-tuner.sh` runs an isolated WSL JACK/Ingen session with a
generated harmonic tone. It refuses to replace an existing audio session.
`peepo_tuner_live` checks A4, −23 cents, +19 cents, E2, A=442 Hz and silence
through the real plugin and adapter. All six cases passed on x86-64. Guitar
listening on the Pi remains a separate check.
