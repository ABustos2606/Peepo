# Beebo manual: features Peepo is missing

**Assessment, 16 October 2026.** The Beebo manual (`reference/Beebo manual.pdf`, 97 pages) was read end to end and compared with these documents, the catalogue overlay, the compatibility matrix and the source. This records what Beebo offers that Peepo does not yet do. It is a gap list, not a decision: nothing here is agreed until the user chooses it. Items marked done were chosen by the user and implemented.

## Already covered

Adding and dragging modules; series and parallel routing; control (CV) cables; a cable per connector, so modules with several inputs or outputs need no port-choice screen; parameter pages; encoders with fine adjustment; patches with a startup patch; stomps as bypass, toggle and momentary; tap tempo and the patch tempo; next and previous patch; the tuner; Clouds' granular modes; NAM and IR file selection; the CPU reading.

## Gaps, most important first

### 1. Beebo's Delay and LFO are not built: done

**Done, 16 October 2026.** Installed on the Pi the same day (package `peepo-arm64-20261016-cdc4ebe-dirty`, all 10 arm64 suites passing); the Pi's catalogue offers both new modules, and the measurements below pass there too.

The manual's tutorial is built on two modules, the Delay and the LFO. The Delay (`http://polyeffects.com/lv2/digit_delay`) was **unavailable**: its source is the archived Digit tree (`digit_carla`), which was not built ([compatibility matrix](evidence/compatibility-matrix.md), row 98). With it went the **rhythmic delay** (manual 1.14): a beats mode with a BPM and a subdivision picker, beside a milliseconds mode.

Correction: the LFO was never missing. Beebo's module map gives its LFO as AMS's tempo LFO (`http://avwlv2.sourceforge.net/plugins/avw/lfo_tempo`), which Peepo already offered and which already followed the patch tempo. `polylfo`, the archived plugin listed here before, is a MIDI curve generator Beebo does not offer.

What was done:

- **Built.** `14-more-plugins.sh` builds the Delay unchanged from `digit_carla` at `75609d7d`, for the engine on the PC and in the Pi's package. The overlay offers it as **Delay** (TIME group), with pages **Echo** (Time, Feedback, Output, Input) and **Character** (Warp, Glide, Tone, BPM). Like Beebo's, it gives only the echoes: the dry signal is cabled to OUT beside it. **Input** off stops feeding it, so the repeats ring out.
- **Rhythmic.** Its time is in beats at its own BPM, and the BPM follows the patch tempo. The Time tile names the note (1/32 to 4 bars, dotted and triplet) and shows the milliseconds at that tempo. **-**, **+** and the encoders step from note to note ([times in beats](29-parameters-files-tempo.md#patch-tempo)).
- **Measured in the engine** (`tools/ingen-spike/19-delay-footswitch.sh`, under WSL): an impulse comes back after exactly 12,000 and 24,000 samples at 120 BPM (1/8 is 250 ms at 48 kHz), then 24,000 and 48,000 once the BPM is 60, with no dry signal ([results](evidence/delay-footswitch-results.txt)). The live catalogue check loads it and moves every control.
- **Not done:** Beebo's milliseconds mode, which leaves the tempo alone. Typing a time in milliseconds is under "Typing a value" below.

### 2. Looper

Part II of the manual (four pages) covers the Loopler, based on SooperLooper and the EDP:

- commands: record (with an input threshold), overdub (with feedback), multiply (with MultiIncrease), insert, replace, substitute, reverse, undo, redo, mute, trigger, one-shot, pause, solo, delay mode;
- several loops; a command acts on the selected loop, all loops or a given loop; previous and next loop;
- quantise and sync to MIDI clock;
- each command bound to a footswitch (hold the switch, press the command) or to a MIDI CC or note;
- `loop common in` and `loop common out` modules, so effects can sit on the dry path, the loop send or the loop return; two of each for stereo;
- `loop extra midi` (internal MIDI generators drive the looper) and `loop midi out` (MIDI clock from the loop).

Peepo has no looper; Clouds Looper is a micro-looper. Beebo's loop modules have no public source ([pedal-inspired plugins](26-pedal-inspired-plugins.md)). The MOD SooperLooper LV2 (`http://moddevices.com/plugins/sooperlooper`) appears in the [plugin comparison data](lv2-plugin-comparison-data.json) and is a possible route; it is not evaluated.

### 3. Footswitches as patchable sources: done

**Done, 16 October 2026.**

Beebo offers Foot Switch A-E as modules (manual 1.9, 6.32-6.36). Each has a **value** output (pressed or not, with latching, on value and off value) and a **tap BPM** output, cabled like any other source. For example, stomp to slew limiter to delay feedback, or a stomp's taps to one delay only. Peepo's stomps acted only through assignments; they could not be cabled.

What was done ([physical controls](18-physical-controls.md)):

- **A Foot switch module** (CV + UTIL). Beebo's foot switch plugin has no public source, so this is Peepo's own small LV2 plugin (`plugins/footswitch.lv2`, `urn:peepo:lv2:footswitch`, GPL-3.0-or-later). Its controls: **Stomp** (1, 2 or 3), **Latching**, **On value**, **Off value** and **Pressed**. Its control output can be cabled to any control input.
- **The stomp drives it.** The interface sets Pressed when the stomp goes down and up, or flips it on each press when Latching is on. This is beside whatever the stomp is assigned to. A stomp a Foot switch module listens to does not also take its every-patch action (next patch, tap tempo). The module shows its stomp's number on the patch.
- **Measured in the engine:** the output gives Off value (0.2), then On value (0.8) once Pressed is set ([results](evidence/delay-footswitch-results.txt)). The live catalogue check loads it and moves every control.
- **Not done: the tap BPM output.** Control cables join control (CV) ports, and a module's BPM is a plain control. A stomp set to **Tap tempo** sets the patch tempo instead, which every module with a BPM follows. A tap tempo for one module only is not offered.

### 4. Footswitch assignment workflow: done

**Done, 16 October 2026.**

Beebo (manual 1.8):

- **Quick assign:** hold a module, press a stomp. A letter in a circle on the module shows the stomp.
- **One stomp, several modules.**
- **A/B swap:** set one of two delays to disabled, assign both to the same stomp; each press switches which one plays.

Peepo stored one assignment per stomp and assigned only from Settings > Controls. Now ([physical controls](18-physical-controls.md)):

- **Quick assign.** Holding a module opens its actions, as before. A stomp pressed there switches that module's bypass from then on, beside any others; pressing it again there takes the module off. The page says which stomps switch the module.
- **Numbered dots.** On the patch, a module a stomp acts on shows the stomp's number in a white dot on its top right corner. That is through its bypass or an assigned parameter, or, for a Foot switch module, the stomp it listens to.
- **One stomp, several modules.** A stomp's property holds one assignment per module. **Assign > Bypass** in Settings > Controls adds a module too, and Controls lists them ("Drive, Clouds / bypass"). Removing a module leaves the others on the stomp.
- **A/B swap.** Each module flips on each press, so two set opposite ways swap places.
- **Checked** on Windows (LVGL workflows), in the simulator and the Ingen adapter tests, and in a bundle round trip. Not yet tried with the physical stomps, which are not wired.

### 5. Stomp combinations

Left and centre pressed together act as switch D, right and centre as switch E; by default they step backwards and forwards through the set list (manual 1.7, 4.1, 6.35-6.36). Three stomps give five switches. Peepo recognizes single presses only.

### 6. Set list

An ordered performance list, separate from the patch library. Its order is what the D/E footswitch pairs step through, and each entry's number is the MIDI program change that selects it (manual 1.7, 4.1). Peepo's next and previous patch step through every stored patch.

### 7. MIDI

All deferred under D16 ([decisions](07-source-audit-and-decisions.md)):

- program change selects a set-list entry;
- **MIDI learn** (manual 4.2): hold the more button, touch a slider, press the MIDI icon, send a CC. The engine already carries Poly's MIDI-learn fix (D23);
- MIDI clock in and out, and MIDI cables between modules;
- about 35 MIDI modules: filters, chord, strum, arpeggio-like delays, CC and note conversion, note to CV.

The hardware also needs MIDI jacks. Planned since 20 October 2026, with MIDI cables and modules: [MIDI](39-midi.md).

### 8. USB import and export

- **Patches:** export the current patch or all patches, and import them (manual 1.16).
- **Impulse responses:** folders `reverbs` and `cabs` on a stick, then Settings > Copy IRs; 48 kHz WAV only (manual 1.15).
- **Updates:** packages copied from a FAT32 stick, then Upgrade (manual 1.11).

Peepo keeps models and IRs in its asset folder ([patch storage](17-patch-storage.md)) but has no way to bring files in or out without ssh.

### 9. Modules with no public source

Sixteen Beebo modules map to Poly plugins whose source is not published: the foot switches, the loopers, MIDI to CV, the onset and pitch detectors, a 3-band EQ, a bitcrusher and two phaser variants ([plugin catalogue](22-plugin-catalogue.md)). Each needs a substitute or a plugin of our own.

### 10. Inputs and outputs as effects loops

On Beebo a spare output and input pair becomes an effects loop (manual 1.13). Cable output 3 to an external phaser and the phaser back into input 3. Then route a delay to output 3 and input 3 to output 1: the repeats go through the phaser, the dry signal does not. The manual warns against feedback paths and against mixing dry signal with a looped copy, since the loop's extra latency causes phase effects. It recommends looping only tails such as delays and reverbs.

Peepo cannot do this properly:

- **One input pair, one output pair.** The canvas has one IN and one OUT, each left and right. The engine's ports are fixed at `in_l`, `in_r`, `out_l` and `out_r` (`src/engine/ingen/session.cpp:16`, `src/storage/bundle.cpp:20`), and `peepo-route` wires them to `system:capture_1/2` and `system:playback_1/2` ([Pi bring-up](23-pi-bring-up.md)).
- **The hardware has no spare jacks.** The Tascam US-100 is 2 in / 2 out, and the proposed I2S interface is one stereo ADC and one stereo DAC ([I2S audio interface](27-i2s-audio-interface.md)).
- **A one-loop workaround, untested.** With a mono guitar on the left input, cabling a delay to OUT right, the external pedal from output right back to input right, and IN right to OUT left gives one mono loop. It costs the stereo output, and which US-100 jack feeds capture 2 is not checked.

Proper support needs:

- an interface with at least two pairs, for example a second stereo ADC/DAC on I2S;
- more endpoint connectors on the canvas, or numbered send and return modules;
- matching engine ports and routing in `peepo-route`;
- saving those connections in patches.

## Smaller differences, probably fine as they are

- **Knob mapping.** On Beebo the last slider touched is mapped to the knobs, the right one coarse and the left one fine. Since 17 October 2026 Peepo also has two encoders: encoder 1 moves a highlight between the page's bars and encoder 2 turns the highlighted one, with push and turn for fine steps; either can be pinned.
- **Typing a value.** Beebo lets the delay time be typed in milliseconds. Peepo has no numeric entry for values.
- **CPU meter.** Beebo shows it only above 60 %; Peepo always shows it.

## Suggested order

1. ~~Items 1, 3 and 4: small, the most effect when playing with three stomps, no MIDI hardware needed.~~ Done 16 October 2026.
2. Items 5 and 6, together: the set list gives the stomp pairs something to step through.
3. Item 2, once a looper plugin is chosen.
4. Items 7 and 8 when MIDI hardware and a USB workflow are wanted.
5. Item 10 together with the enclosure's audio interface: the number of jacks is a hardware decision first.
