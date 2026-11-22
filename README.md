# Peepo

Peepo is an open-source modular effects instrument for the Raspberry Pi 5. It boots straight into a touchscreen patch editor: effects appear as blocks you connect with virtual cables, in series or in parallel, and you can play them with rotary encoders and footswitches.

The concept is inspired by Poly Effects' Beebo. The audio side uses Poly's Ingen-based engine and LV2 plugins, together with a large library of compatible open-source LV2 effects (Guitarix, Dragonfly, NAM, IR convolution, ports of Mutable Instruments modules, and more). The touchscreen interface is a new LVGL application written for Peepo. It contains no Beebo interface code or assets.

## Features

- Drag-and-drop patching on an 800 × 480 touchscreen, with no desktop environment
- Series and parallel routing, mono and stereo
- Hundreds of LV2 effects, browsable by category ([module list](docs/33-module-list.md))
- Neural Amp Modeler and impulse-response loading
- A tuner, global tempo, scenes and MIDI (USB and TRS)
- Patch storage with a configurable startup patch
- Assignable encoders and footswitches
- Direct boot into the instrument. Audio keeps playing if the interface restarts.
- A Windows simulator for working on the interface without a Pi

## What you need

| Part | Notes |
| --- | --- |
| Raspberry Pi 5 | 2 GB RAM or more, with a good 5 V / 5 A supply |
| microSD card | Running a 64-bit Debian-based OS (Raspberry Pi OS or similar) |
| Audio interface | Any USB class-compliant interface that ALSA supports |
| Touchscreen | 800 × 480. Touch over USB keeps the GPIO header free. |
| Controls (optional) | Push-button rotary encoders and momentary SPST footswitches wired to GPIO |
| Enclosure, jacks, wiring | As needed for your build |

## Get the source

```bash
git clone https://github.com/ABustos2606/Peepo.git
cd Peepo
```

The submodules under `reference/` hold upstream reference material only. You don't need them to build. See [reference/README.md](reference/README.md) if you want them.

## Build and install on the Pi

You can build the package either on the Pi itself or on a PC.

**On the Pi** (Debian / Raspberry Pi OS arm64, as root):

```bash
sudo bash tools/pi/build.sh
```

**On a PC** (inside Debian under WSL2 or Linux, as root). This builds for arm64 under emulation, so the first build is slow:

```bash
sudo bash tools/pi/build-arm64.sh
```

Either way, the build leaves a package in `build/pi/`. Copy it to the Pi, unpack it, and install:

```bash
sudo ./install.sh               # install the services and keep the previous version for rollback
sudo ./install.sh --enable --start  # also start now and at every boot
sudo ./install.sh --instrument  # also boot directly into Peepo (disables the desktop and competing audio services)
sudo ./install.sh --rollback    # restore the previous installation
sudo ./install.sh --remove      # uninstall
```

Full details: [Linux build, Pi packaging and services](docs/19-linux-and-pi.md) and [instrument boot](docs/24-instrument-boot.md).

## Configure your hardware

### Audio interface and screen

Edit `/etc/peepo/peepo.conf` (template: [config/pi/peepo.conf](config/pi/peepo.conf)). Later installs never overwrite it.

```ini
JACK_DRIVER=alsa
JACK_DRIVER_OPTIONS=-d hw:USB -r 48000 -p 128 -n 3   # use your card's name from `aplay -l`
AUDIO_IN_LEFT=system:capture_1
AUDIO_OUT_LEFT=system:playback_1
UI_OPTIONS=--display drm                             # add --rotate 180 or --touch /dev/input/by-id/... if needed
```

Set `-d hw:<name>` to match your interface. Adjust the sample rate (`-r`), period size (`-p`) and number of periods (`-n`) to trade latency against stability. Then run `sudo systemctl restart peepo.target`.

### Encoders and footswitches

Peepo reads physical controls as Linux input events, so you don't need an extra board:

1. Copy the overlay lines from [config/pi/config.txt](config/pi/config.txt) into `/boot/firmware/config.txt`. Change the GPIO pins to match your wiring, then reboot.
2. Map the controls in `/etc/peepo/controls.conf` (template: [config/pi/controls.conf](config/pi/controls.conf)). Use `evtest` to check the codes. Add `reverse` if a knob turns the wrong way.

Controls are optional, because everything also works from the touchscreen. See [physical controls](docs/18-physical-controls.md) for the suggested wiring and how the controls behave.

## Run the simulator on Windows

The simulator runs the real interface at 800 × 480 against a simulated engine. It makes no sound. It needs Visual Studio C++ Build Tools and CMake:

```powershell
cmake --preset windows
cmake --build --preset windows --parallel
ctest --preset windows
.\build\Debug\peepo.exe
```

The keyboard stands in for the hardware: **Q/A** and **W/S** turn the encoders, **1** and **2** press them, and **Z**, **X** and **C** are the footswitches. See [Windows UI development](docs/11-windows-ui-foundation.md).

<!-- module-list:start -->
## Modules

Everything you can add to a patch, as listed in Add on the instrument (21 November 2026): **219 modules** in 14 categories. A name with options in brackets is one card with a pill for each variant (mono or stereo, internal LFO or CV). Connections, status and plugin identities are in the [complete module list](docs/33-module-list.md).

### Drive

| Module | What it does |
| --- | --- |
| Bandisto | Distortion with separate low, middle and high bands. |
| Bottle Rocket | Based on the Mesa V1 Bottle Rocket tube overdrive. |
| Degrade | Lo-fi distortion through reduced sample rate and resolution. |
| Dither | Reduces bit depth with dither, for lo-fi grit or clean level reduction. |
| DOP 250 | Based on the DOD OD-250 Yellow Overdrive. |
| Downsampler (Mono, Stereo) | Lo-fi crusher that lowers the sample rate for gritty, aliased tones. |
| Gain | Changes signal level. Clean gain or attenuation from -90 dB to +24 dB. |
| Guvnor | Based on the Marshall "The Guv'nor" Overdrive. |
| Gx ColorSound Tonebender | Tonebender-style fuzz distortion. |
| Gx Fuzz Face Fuller | Fuzz Face-style distortion with the Fuller modification. |
| Gx Muff | Muff-style fuzz distortion. |
| Gx Rangemaster | Rangemaster-style treble booster. |
| Gx Screaming Bird | Treble booster for bright, cutting tones. |
| Gx Tube Screamer | Tube Screamer-style overdrive. |
| Micro Amp | A simple booster. |
| SD-1 | Based on the Boss SD-1 Super Overdrive. |
| SD-2 Lead | Based on the Boss SD-2 Dual Overdrive. |
| Sun Face | Based on the Analog Man Sun Face fuzz. |
| Super Fuzz | Based on the Univox Super-Fuzz. |
| Tubewarmth | Tube triode emulation for warmth and gentle drive. |

### Amp

| Module | What it does |
| --- | --- |
| Bass amp (SVT) | Classic tube bass amp simulation. |
| Blue Amp | Single-ended amp inspired by late 1950s Fender Princeton and Champ designs. |
| Combo | A conventional guitar amp and speaker simulation; no model file needed. |
| Gx JCM800 preamp (Mono, Stereo) | JCM800-style guitar preamp. |
| NAM | Amp model player. Loads models trained on real amps and pedals. |
| Plexi | Plexi power amp simulation. |
| UVox 720k | Based on Univox 720k keyboard amp from 1978. |

### Cabinet

| Module | What it does |
| --- | --- |
| Cabinet (Mono, Mono > stereo, Stereo) | Cabinet from an impulse response file. Stereo processes left and right separately; a four-channel IR adds cross-channel responses. |
| Gx Cabinet (built-in) | Cabinet simulator with built-in speaker models. No impulse response file needed. |

### Dynamics

| Module | What it does |
| --- | --- |
| Compressor (Mono, Stereo) | Evens out the level of a signal. |
| De-ess | Reduces harsh high-frequency peaks. |
| Dynamics | Compressor, limiter and gate for controlling signal level. |
| Loudness | Changes perceived loudness with level-dependent filtering. |
| MultiBand | Compresses low, middle and high frequency bands separately. |
| Slow gear | Automatically swells volume to remove note attack, for violin-like playing. |
| Splitter | Splits the signal by frequency or level, to treat lows and highs or loud and quiet parts apart. |
| Transient | Reshapes the attack and sustain of the input. |

### Filter

| Module | What it does |
| --- | --- |
| Diode ladder | A diode ladder low pass filter similar to the vintage Japanese designs. |
| Gx Crybaby GCB95 | Crybaby-style wah filter with a sweep control. |
| Gx Graphic EQ | Graphic equalizer with separate frequency-band gains. |
| Korg 35 high-pass | A high pass filter similar to the vintage Japanese designs. |
| Korg 35 low-pass | A low pass filter similar to the vintage Japanese designs. |
| Moog half ladder | A low pass filter inspired by vintage American designs. |
| Moog Low-Pass Filter 1 | Resonant ladder low pass filter with modulation inputs for cutoff and resonance. |
| Oberheim filter | A multi out filter inspired by vintage American designs. |
| Parametric EQ (Mono, Stereo) | Adjusts selected frequency bands of a signal. |
| Quack | Autowah. |
| RezFilter | Resonant filter with envelope and LFO modulation. |
| TalkBox | Shapes a carrier with another signal; needs both audio inputs connected. |
| VCF | Resonant multi-mode filter whose cutoff follows a control signal. |

### Modulation

| Module | What it does |
| --- | --- |
| Chorus (LFO, CV) | 8 voice multi-dimensional chorus. CV drives its LFO from another module. |
| Eureka Flock | Stereo chorus and ensemble with up to ten voices. Rate and Depth set the movement; Random motion varies it. |
| Eureka Ripple | Resonant stereo phaser. Pause sweep holds the filter position while audio keeps passing through it. |
| Flanger (LFO, CV) | Flanger. CV drives its LFO from another module. |
| Gx Tremolo | Rhythmic volume modulation. |
| Harmonic tremolo (LFO, CV) | Harmonic tremolo. CV drives its LFO from another module. |
| Ring Modulator - Audio | Ring modulator: multiplies two audio signals for metallic, bell-like tones. |
| RingMod | Ring modulation for metallic, bell-like tones. |
| Rotary speaker (Simple, Full) | A rotating loudspeaker using physical modelling. Full has the same sound with more controls. |
| RoundPan | Moves the sound between left and right. |
| Stone phaser (Mono, Stereo) | Warm vintage-style phaser with an internal LFO. |
| ThruZero | Through-zero flanging. |
| Vibrato (LFO, CV) | Vibrato. CV drives its LFO from another module. |
| YK Chorus | Chorus for widening and thickening the input. |

### Pitch

| Module | What it does |
| --- | --- |
| Detune | Small pitch offsets for doubling and thickening. |
| Gx Detune | Pitch detuning and octave effects with wet and dry levels. |
| Gx OC-2 | Octave-down effect with separate octave and dry levels. |
| Mist Stretch | A granular time stretching and pitch shifting module. |
| Octant | Adds octaves to what you play, two down to two up, each with its own level. Follows chords as well as single notes. |
| Pitch Shifter | Changes the pitch of the incoming audio. |
| RePsycho! | Audio-triggered pitch shifting and sample playback. |
| Shepard | Shepard-tone processing for an endlessly rising or falling illusion. |

### Delay

| Module | What it does |
| --- | --- |
| Aura | An always-listening micro-looper and reverb, delay or sampler sharing one clock. Clock slows both together. Stereo Spread and six CV inputs shape the sound. |
| Bitcrushed delay | Flexible delay module with bitcrushed repeats. |
| Discipline | Delay with a 3-minute memory you can scan back into. Works alone. Modifier Outside sends the repeats through your modules to a Discipline Return. |
| DubDelay | Modulated echo with feedback for dub-style repeats. |
| Euclidelay | Echoes on four Euclidean rhythms. Steps, Hits and Shift set each voice's pattern; each voice loops it with feedback, with its own level and pan. |
| Eureka Skip | Pitched multi-tap delay. Repeat heads sets tap count. Sync uses BPM and Beat division instead of Delay time. |
| Eureka Spool | Variable-speed stereo tape echo with drift. Sync follows patch tempo; Infinite holds echoes. |
| Gx Duck delay (Mono, Stereo) | Delay that reduces the repeats while the input is playing. |
| Gx Echo Cat | Echo effect with controls for its repeat character. |
| Loo-pi | Eight stereo loops: record, overdub, multiply, insert, reverse, half and double speed, sync to the tempo or a loop. Open its page for loop cards and commands. |
| Mist Looper | Granular pitch shifting, micro looping delay. Freeze loops what is in the buffer. |
| Stereo delay | Stereo echo with independent left/right delay times, feedback and filtering. |
| Tempo delay | Flexible delay timed in beats, so it follows the patch tempo. Only the echoes come out: cable the dry signal to OUT as well. |
| Twist Delay | A delay where speed and length interact with quality. |

### Reverb

| Module | What it does |
| --- | --- |
| Dragonfly Hall | Hall reverb with early and late reflections, modulation, filters and decay controls. |
| Dragonfly Plate | Plate reverb with selectable algorithms, damping and decay controls. |
| Dragonfly Room | Room reverb with separate early and late levels, size, filters and decay controls. |
| Eureka Hush | Diffuse stereo reverb. Spread changes the stereo pattern; Freeze holds the tail. |
| Eureka Slowmo | Long stereo reverb with size and diffusion controls. Freeze holds the tail. |
| IR Reverb (Mono, Mono > stereo, Stereo) | Reverb from an impulse response file. Stereo processes left and right separately; a four-channel IR adds cross-channel responses. |
| Mist Oliverb | A special effects reverb with pitch-shifted, modulated tails. Hold sustains the tail. |

### Texture

| Module | What it does |
| --- | --- |
| Beat Repeater (Mono, Stereo) | Repeats a slice of the last beat while the gate is high, like a stutter edit. |
| Beat Slicer (Mono, Stereo) | Records a few beats and plays back slices of them while the gate is high, optionally reversed. |
| Drizzle | Grains of the live input, with reverb; Size at the top makes it a delay. Freeze holds the recording. With Seed lock on grains run at the Density rate; off, they play while Seed is held. |
| Eureka Bloom | Swell the guitar into a pitched, diffuse texture. Freeze holds the sound. |
| Eureka Crinkle | Unstable stereo delay with random snags and drift. Tape stop slows the wet sound to silence. |
| Eureka Granny | Two shaped grains tumble through the live input. Infinite holds the recording. |
| Eureka Prism | Spectral gates sweep across the sound with resonance. Pause holds their phase. |
| Granulator (Mono, Stereo) | Chops the input into short overlapping grains for smeared, cloud-like textures. |
| Mist | Granular texture from the live input. Freeze holds the buffer. Blend mixes in the clean input: all of it up to the middle, none at the top. |
| Mist Spectral | Spectral processor that blurs, freezes and reshapes the input in the frequency domain. |
| Power cut | Simulates turning off a turntable: the sound slows down and stops. |
| Reverse (Reflector) | Reverse effect. Try a short fragment length for a weird tremolo. |
| Stuck (freeze) | Holds what you are playing when the control level is active, creating a drone. |
| Twist Binaural Doppler Panner | Binaural panner, allows positioning in 3D. |
| Twist Bitmangler | Brutal bitmangler. Warning, can get very loud. |
| Twist Chebyshev | Chebyshev wave folder for bright, harmonically rich distortion. |
| Twist Comparator | Combines two signals by comparing them, for harsh digital tones. |
| Twist Frequency Shifter | Shifts every frequency by the same amount, for clangorous and barber-pole effects. |
| Twist Meta | A powerful cross modulation module which applies an algorithm to the two inputs. |
| Twist Vocoder | Vocoder: shapes the carrier with the spectrum of the modulator. |
| Twist Wavefolder | Wave folder for complex, buzzy overtones. |
| Vinyl | Emulates dirty old lo-fi vinyl records: crackle, hum, wow and flutter. |

### Resonator

| Module | What it does |
| --- | --- |
| Bell Resonator | Resonator simulating multiple vibrating structures: strings, modes and more. |
| Chamber | A bell, plate or strings that your guitar rings at its own pitch: cable it to Strike in, no gate needed. It starts as a long bell with reverb. Raise Strike, Bow or Blow level to play it with Play or a gate; Blow in sounds only while one is held. |
| Eureka Chime | Twelve resonators follow the left input. Pitch freeze holds tuning while new audio still excites them. |
| Mist Resonestor | Dual voice, four part resonator played by the input. |

### Sound source

| Module | What it does |
| --- | --- |
| BeatBox | Triggers synthesized drum sounds from incoming audio. |
| Drizzle Synth | A wavetable voice played as grains, with reverb. Needs no input. Wavetable picks the sound, Osc/grains mixes the plain voice with its grains, and Pitch CV takes volt per octave. |
| DX10 | FM synthesizer voice for bells, keys and basses. |
| ePiano | Electric piano instrument. |
| Eureka Spark | Four voices track pitches in the left input. Portamento glides; Infinite holds the sound. |
| JX10 | Polyphonic analog-style synthesizer. |
| Noise2 - Audio | Noise source: white, random or pink noise as audio. |
| Piano | Acoustic piano instrument. |
| Tracker | Pitch-tracking oscillator played by incoming audio. |
| Weave Oscillator | A powerful multi-model oscillator voice. |

### Control

| Module | What it does |
| --- | --- |
| 16 Step Note Sequencer (Trigger, BPM) | 16 step note sequencer. Trigger steps it from another module; BPM runs on its own clock. |
| 16 Step Sequencer (Trigger, BPM) | 16 step sequencer. Trigger steps it from another module; BPM runs on its own clock. |
| ADSR Envelope | Basic ADSR envelope generator. Takes in a gate. |
| Advanced Envelope | Multi-stage envelope generator with four attack and three release stages. Takes in a gate. |
| Analog Driver | Adds slow drift and detune to a pitch CV, for the slightly unstable feel of analog oscillators. |
| Clock divider | Outputs a trigger every n triggers. |
| CV delay | Delays a control signal by up to 10 seconds. Use with LFOs, envelopes or sequencers. |
| CV Source | Four fixed control voltages, each with coarse and fine knobs. |
| CV to trigger | A Schmitt trigger that converts a control signal to on and off triggers. |
| Dice Random Sampler | Powerful repeatable randomness source: random gates and control signals. |
| Envelope | ADSR envelope generator with delay and hold stages. Takes in a gate. |
| Envelope follower | Tracks an input signal and converts it into a control signal. |
| Euclidean | A euclidean sequencer with 4 tracks. Connect a trigger to go to the next step. |
| Flow Envelope | A powerful envelope and LFO generator with four related outputs. |
| Foot switch | A stomp as a control signal: On value while it is down, Off value otherwise. Latching flips it on each press instead. |
| Lattice Drum Sequencer | Drum trigger explorer for kick, snare and hi-hat. Expects a 4 ppq clock if using external clock. |
| LFO (Rate, Tempo) | Low frequency oscillator with several wave shapes, set by rate or synced to a tempo. Sends a control signal. |
| MIDI clock | MIDI clock at the patch tempo on a MIDI cable, with start and stop. |
| MIDI clock in | Follows incoming MIDI clock: tempo, start and pulse outputs. |
| MIDI controller | One MIDI control change as a control value, to cable to any control. |
| MIDI program change | Sends MIDI program changes to switch presets: a chosen program, or the next or previous one on a trigger. |
| MIDI to CV | Plays CV modules from MIDI notes: pitch, gate, velocity, a trigger on each new note, mod wheel and pressure. |
| MIDI to CV poly | Plays chords on up to four CV voices from MIDI notes: pitch, gate and velocity for each voice, mod wheel and pressure. |
| Noise2 - CV | Random control signal: noise or stepped random values at a set rate. |
| Percussive Envelope | Short attack decay envelope for plucks and hits. Takes in a gate. |
| Quantizer | Quantizes a volt per octave signal to a musical scale. |
| Reel LFO | Four related LFOs from one rate: shapes, phases or rates spread across the outputs, and each can bend its neighbour. With Tempo sync on, a cycle lasts a note at the patch tempo. |
| Retriggerable DAHDSR Envelope | A delay attack hold decay sustain release envelope generator. Can be retriggered. |
| Ring Modulator - CV | Multiplies two control signals. |
| Sample&Hold | Samples a control value when a trigger arrives and holds it until the next one. |
| Slope | Six segments that chain into envelopes, LFOs and step sequences. Starts group on makes a segment begin a group played by its gate, whose shape comes out of that segment's Out. Alone, a segment is an LFO, envelope, S&H, delay or oscillator. |
| Strum | MIDI and volt per octave controller based on strumming. |
| Toggle | Toggles the output value on every trigger. |
| Trigger to gate | Takes in a trigger and outputs a gate with a length related to the tempo. |

### Utility

| Module | What it does |
| --- | --- |
| Absolute | Rectify or get the absolute value of the input. |
| Amp | Scales a control signal by a fixed gain; negative gain inverts it. |
| Amplitude Modifier | Rescales a control signal from one range to another. |
| Control to CV | Turns a knob into a control signal you can patch to other modules. |
| CV to Control | Turns a control signal back into a knob value. |
| CV to MIDI CC | Converts a control signal to MIDI CC. |
| CV to MIDI Note | Converts volt per octave pitch to MIDI notes. Velocity is optional. |
| Difference | a - b for control signals. |
| Discipline Return | Brings Discipline's repeats back in when its Modifier is Outside. One per patch. |
| Hz to VC | Converts a frequency in Hz into a volt per octave pitch signal. |
| Image | Adjusts stereo width and the balance of mid and side. |
| Inv | Inverts a control signal: up becomes down. |
| Maximum | Max of a, b; also logical or. |
| MIDI CC Map | Changes one MIDI control message into another. |
| MIDI CC to Note | Translates MIDI control changes to note on and off messages. |
| MIDI Channel Filter | Discards MIDI messages per channel. |
| MIDI Channel Map | Maps any MIDI channel to another MIDI channel. |
| MIDI Channel Unisono | Duplicates MIDI events from one channel to another. |
| MIDI Choke | Triggers note-off events to create exclusive note groups. |
| MIDI Chord | Creates MIDI chords from a single note in a given musical scale. |
| MIDI Chromatic Transpose | Chromatically transposes MIDI notes. |
| MIDI Enforce Scale | Forces MIDI notes onto a given musical scale. |
| MIDI Keysplit | Changes MIDI channel depending on the note, with optional transpose. |
| MIDI Monophonic Legato | Holds a MIDI note until the next note arrives. |
| MIDI N-Tap Delay | Repeats MIDI notes a number of times, with tempo ramps. |
| MIDI Note to CC | Converts MIDI note-on messages to control changes. |
| MIDI Note to PC | Converts MIDI notes to program change messages. |
| MIDI Note Toggle | Play a MIDI note to turn it on, play it again to turn it off. |
| MIDI Quantization | Quantizes live MIDI events to a time grid. |
| MIDI Scale CC Value | Scales the value of a MIDI control change. |
| MIDI Strum | MIDI arpeggio that simulates strumming a guitar. |
| MIDI Velocity Adjust | Scales and offsets MIDI note velocity. |
| Minimum | Min of a, b; also logical and. |
| Mixer 2 channels - Audio | Mixes two audio signals with a level for each. |
| Mixer 2 channels - CV | Mixes two control signals with a level for each. |
| Mixer 4 channels - Audio | Mixes four audio signals with a level for each. |
| Mixer 4 channels - CV | Mixes four control signals with a level for each. |
| Pitch calibration in | Pitch calibration for a volt per octave input. |
| Pitch calibration out | Pitch calibration for a volt per octave output. |
| Product | a times b for control signals. |
| Ratio | a divided by b for control signals. |
| Reel | Keyframe mixer: stores up to 64 snapshots of four levels along Frame and morphs between them as Frame moves, by hand or by CV. Four VCAs, a mix and four level CVs. Open its page to see and edit the keyframes. |
| Scope (Mono, Stereo) | Oscilloscope view of a signal. Audio passes through unchanged. |
| Signal Absolute Value | Rectifies a control signal: negative values become positive. |
| Slew Limiter | Slows how fast a control signal changes, with separate rise and fall times. Useful with foot switches. |
| Sum | a + b for control signals. |
| Tuner | Chromatic instrument tuner. Shows the note and how flat or sharp it is; connect your input and play one string. |
| VC Panning | Pans audio between two outputs from a control signal. |
| VC Switch - Audio | Switches or crossfades between two audio signals from a control signal. |
| VC Switch - CV | Switches or crossfades between two control signals from a control signal. |
| VC to Hz | Converts a volt per octave pitch signal into a frequency in Hz. |
| VCA | Simple voltage controlled amplifier. Sets audio level from a control signal. |
| VCA Exp - Audio | Voltage controlled amplifier with an exponential response. Sets audio level from a control signal. |
| VCA Exp - CV | Voltage controlled amplifier with an exponential response, for control signals. |
| VCA Lin - Audio | Voltage controlled amplifier with a linear response. Sets audio level from a control signal. |
| VCA Lin - CV | Voltage controlled amplifier with a linear response, for control signals. |

<!-- module-list:end -->
## Documentation

- [Architecture](docs/02-architecture.md)
- [Touchscreen and controls](docs/03-interface-and-controls.md)
- [Module list](docs/33-module-list.md) and [module names](docs/37-module-names.md)
- [Patch storage](docs/17-patch-storage.md)
- [MIDI](docs/39-midi.md) and [scenes](docs/44-scenes.md)
- [Linux build, Pi packaging and services](docs/19-linux-and-pi.md)

The `docs/` folder holds the full design notes and development records.

## License

Original Peepo code and documentation are licensed under the **GNU GPL v3 or later**. See [LICENSE](LICENSE).

Third-party components keep their own licenses. For example, Poly's Ingen engine is AGPL-3.0-or-later. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
