# Complete module list

**Updated 22 October 2026.** This is the full selected Peepo module inventory, using the names and categories shown on the touchscreen.

**358 plugin identities:** 338 installed in the Pi export and 20 not installed. All **101 selected Guitarix identities**, including the original **43 GxPlugins**, and the **three retained Dragonfly reverbs** are included.

**Removed effects:** Bollie Delay XT and Faust Granulator have been removed from Peepo and this inventory. The Granulator - Mono and Granulator - Stereo entries below are separate AVW plugins. [Removal details](35-patchstorage-expansion.md).

**First catalogue purge:** Gx Live looper, Ambience, MVerb, Gx Reverb stereo, Gx Room simulator, Gx Zita reverb stereo and Dragonfly Early Reflections are retired. IR effects now use the Cabinet and IR Reverb families; alternate convolution identities are hidden for older patches. [Purge details](36-catalogue-purge.md).

The tables below list every identity once. IN and OUT are fixed patch endpoints, described separately.

## Availability

| Status | Count | Meaning |
| --- | ---: | --- |
| Ready | 281 | Can be added to a patch. This does not imply a guitar listening test of every module. |
| Needs adaptation | 41 | Installed, but disabled in Add for the stated limitation. |
| Unavailable | 7 | Present in the Pi export but unusable by the current host; see each reason. |
| Hidden | 9 | Omitted from Add as a duplicate or redundant module; retained for saved patches. |
| Not installed | 20 | Selected reference identity without an installed implementation. |

**File-based effects:** NAM, Cabinet and IR Reverb have file selectors. Each IR family offers Mono, Mono to Stereo and Stereo configurations. Stereo accepts mono/stereo IRs for separate left/right processing; a four-channel IR adds cross-channel responses. Alternate convolvers remain hidden for older patches. [File handling](29-parameters-files-tempo.md#model-and-ir-files-p83).

**Reading connections:** counts are plugin connectors before Peepo's mono/stereo routing. CV connectors carry modulation. MIDI/events includes LV2 event/message ports, including file-selection messages; it does not imply implemented musical MIDI routing.

## Browse by category

| Category | Ready / listed |
| --- | ---: |
| [Drive](#drive) | 55 / 55 |
| [Amp](#amp) | 20 / 20 |
| [Cabinet](#cabinet) | 5 / 5 |
| [Dynamics](#dynamics) | 14 / 14 |
| [Filter](#filter) | 23 / 24 |
| [Modulation](#modulation) | 35 / 35 |
| [Pitch](#pitch) | 8 / 8 |
| [Delay](#delay) | 18 / 20 |
| [Reverb](#reverb) | 9 / 9 |
| [Texture](#texture) | 13 / 13 |
| [Resonator](#resonator) | 3 / 3 |
| [Sound source](#sound-source) | 6 / 12 |
| [Control](#control) | 32 / 34 |
| [Utility](#utility) | 40 / 77 |

Also: [hidden identities](#hidden-identities), [not installed](#not-installed), [fixed endpoints](#fixed-endpoints), [all 43 GxPlugins](32-gxplugins.md#all-43-gxplugins).

## Drive

| Module | What it does | Connections | Status |
| --- | --- | --- | --- |
| Axis Face (GxPlugins) | Based on the Axis Face Silicon fuzz. | audio 1 in / 1 out | Ready |
| Baja Tube Driver (GxPlugins) | Based on the BaJaTubeDriver. | audio 1 in / 1 out | Ready |
| Bandisto | Distortion with separate low, middle and high bands. | audio 2 in / 2 out | Ready |
| Boob Tube (GxPlugins) | Based on the Boob Tube. | audio 1 in / 1 out | Ready |
| Bottle Rocket (GxPlugins) | Based on the Mesa V1 Bottle Rocket tube overdrive. | audio 1 in / 1 out | Ready |
| Club Drive (GxPlugins) | Overdrive Pedal Simulation based on a EF86 Pentode Valve Simulation. | audio 1 in / 1 out | Ready |
| Degrade | Lo-fi distortion through reduced sample rate and resolution. | audio 2 in / 2 out | Ready |
| Dither | Reduces bit depth with dither, for lo-fi grit or clean level reduction. | audio 2 in / 2 out | Ready |
| DOP 250 (GxPlugins) | Based on the Analog Man DOD OD-250 Yellow Overdrive. | audio 1 in / 1 out | Ready |
| Downsampler - Mono | Lo-fi crusher that lowers the sample rate for gritty, aliased tones. | audio 1 in / 1 out | Ready |
| Downsampler - Stereo | Stereo lo-fi crusher that lowers the sample rate for gritty, aliased tones. | audio 2 in / 2 out | Ready |
| Eternity (GxPlugins) | Low compression overdrive pedal. | audio 1 in / 1 out | Ready |
| Gain | Changes signal level. Clean gain or attenuation from -90 dB to +24 dB. | audio 1 in / 1 out | Ready |
| Guvnor (GxPlugins) | Based on the Marshall "The Guv'nor" Overdrive. | audio 1 in / 1 out | Ready |
| Gx Booster | Raises the signal level. | audio 1 in / 1 out | Ready |
| Gx ColorSound Tonebender | Tonebender-style fuzz distortion. | audio 1 in / 1 out | Ready |
| Gx Fuzz | Guitarix fuzz distortion. | audio 1 in / 1 out | Ready |
| Gx Fuzz Face Fuller | Fuzz Face-style distortion with the Fuller modification. | audio 1 in / 1 out | Ready |
| Gx Fuzz Face JH-2 | Fuzz Face-style distortion, JH-2 variant. | audio 1 in / 1 out | Ready |
| Gx Fuzz Master | Fuzz distortion with tone and output controls. | audio 1 in / 1 out | Ready |
| Gx HF Brightener | Brightens the signal's upper frequencies. | audio 1 in / 1 out | Ready |
| Gx Hogs Foot | Bass booster emphasizing low frequencies. | audio 1 in / 1 out | Ready |
| Gx Hornet | Guitarix Hornet distortion effect. | audio 1 in / 1 out | Ready |
| Gx Mole | Low-frequency booster for adding weight to the signal. | audio 1 in / 1 out | Ready |
| Gx Muff | Muff-style fuzz distortion. | audio 1 in / 1 out | Ready |
| Gx Multiband distortion | Applies distortion to separate frequency bands. | audio 1 in / 1 out | Ready |
| Gx Rangemaster | Rangemaster-style treble booster. | audio 1 in / 1 out | Ready |
| Gx Screaming Bird | Treble booster for bright, cutting tones. | audio 1 in / 1 out | Ready |
| Gx Tube Screamer | Tube Screamer-style overdrive. | audio 1 in / 1 out | Ready |
| Heathkit (GxPlugins) | Based on the Heathkit TA-28 distortion/booster/fuzz. | audio 1 in / 1 out | Ready |
| Hot Box (GxPlugins) | Based on the Matchless Hot Box tube overdrive. | audio 1 in / 1 out | Ready |
| Hyperion (GxPlugins) | Based on the Devi Ever FX Hyperion fuzz. | audio 1 in / 1 out | Ready |
| Knight Fuzz (GxPlugins) | Fuzz based on the Basic Audio Knight Fuzz pedal. | audio 1 in / 1 out | Ready |
| Liquid Drive (GxPlugins) | Based on the Liquid Drive fuzz, a modified Ross Distortion. | audio 1 in / 1 out | Ready |
| Luna (GxPlugins) | Gnarly overdrive pedal simulation. | audio 1 in / 1 out | Ready |
| Maestro FZ-1B (GxPlugins) | Based on the Robert Moog-designed Maestro FZ-1B fuzz. | audio 1 in / 1 out | Ready |
| Maestro FZ-1S (GxPlugins) | Based on the Maestro FZ-1S Super-Fuzz. | audio 1 in / 1 out | Ready |
| Micro Amp (GxPlugins) | A simple booster. | audio 1 in / 1 out | Ready |
| Overdrive | Soft-clipping distortion with tone and level controls. | audio 2 in / 2 out | Ready |
| Saturator (GxPlugins) | Based on the Joe Satriani-specified Vox Satchurator distortion. | audio 1 in / 1 out | Ready |
| SD-1 (GxPlugins) | Based on the Boss SD-1 Super Overdrive. | audio 1 in / 1 out | Ready |
| SD-2 Lead (GxPlugins) | Based on the Boss SD-2 Dual Overdrive. | audio 1 in / 1 out | Ready |
| Shaka Tube (GxPlugins) | Based on the ShakaTube. | audio 1 in / 1 out | Ready |
| Sigmoid Booster | Soft saturation and boost. | audio 1 in / 1 out | Ready |
| Sloopy Blue (GxPlugins) | Overdrive pedal simulation. | audio 1 in / 1 out | Ready |
| Sun Face (GxPlugins) | Based on the Analog Man Sun Face fuzz. | audio 1 in / 1 out | Ready |
| Super Fuzz (GxPlugins) | Based on the Univox Super-Fuzz. | audio 1 in / 1 out | Ready |
| Suppa Tone Bender (GxPlugins) | Based on the Vox Supa Tonebender fuzz. | audio 1 in / 1 out | Ready |
| Tim Ray (GxPlugins) | Based on the Vemuram Jan Ray. | audio 1 in / 1 out | Ready |
| Tone Machine (GxPlugins) | Based on the Foxx Tone Machine fuzz. | audio 1 in / 1 out | Ready |
| Tube Distortion (GxPlugins) | Generic tube distortion based on http://www.montagar.com/~patj/tubedist.gif. | audio 1 in / 1 out | Ready |
| Tubewarmth | Tube triode emulation for warmth and gentle drive. | audio 1 in / 1 out | Ready |
| Valve Caster (GxPlugins) | Based on the Valve Caster. | audio 1 in / 1 out | Ready |
| Vintage Fuzz Master (GxPlugins) | Based on the Devi Ever Vintage Fuzz Master. | audio 1 in / 1 out | Ready |
| Voodoo Fuzz (GxPlugins) | Based on the Voodoo Lab SuperFuzz. | audio 1 in / 1 out | Ready |

## Amp

| Module | What it does | Connections | Status |
| --- | --- | --- | --- |
| Bass amp (SVT) (GxPlugins) | Classic tube bass amp simulation. | audio 1 in / 1 out | Ready |
| Blue Amp (GxPlugins) | Single-ended amp inspired by late 1950s Fender Princeton and Champ designs. | audio 1 in / 1 out | Ready |
| Combo | A conventional guitar amp and speaker simulation; no model file needed. | audio 2 in / 2 out | Ready |
| Epic (GxPlugins) | Based on Valve Junior. | audio 1 in / 1 out | Ready |
| Gx Alembic preamp | Studio preamp with tone controls. | audio 1 in / 1 out | Ready |
| Gx Amplifier | Guitarix guitar amp with tone controls and cabinet processing. | audio 1 in / 1 out | Ready |
| Gx Amplifier stereo | Stereo Guitarix amp with tone controls and cabinet processing. | audio 2 in / 2 out | Ready |
| Gx JCM800 preamp | JCM800-style guitar preamp. | audio 1 in / 1 out | Ready |
| Gx JCM800 preamp stereo | Stereo JCM800-style guitar preamp. | audio 2 in / 2 out | Ready |
| Gx Redeye Big Chump | Big Chump variant of the Redeye guitar amp models. | audio 1 in / 1 out | Ready |
| Gx Redeye Chump | Chump variant of the Redeye guitar amp models. | audio 1 in / 1 out | Ready |
| Gx Redeye Vibro Chump | Vibro Chump variant of the Redeye guitar amp models. | audio 1 in / 1 out | Ready |
| Gx Studio preamp stereo | Stereo studio preamp with tone controls. | audio 2 in / 2 out | Ready |
| NAM | Amp model player. Loads models trained on real amps and pedals. | audio 1 in / 1 out | Ready |
| Plexi (GxPlugins) | Plexi power amp simulation. | audio 1 in / 1 out | Ready |
| Power amp (Cream) (GxPlugins) | Cream-coloured power amp emulation with bass and treble. | audio 1 in / 1 out | Ready |
| Power amp (Supersonic) (GxPlugins) | Vintage American power amp emulation. | audio 1 in / 1 out | Ready |
| UVox 720k (GxPlugins) | Based on Univox 720k keyboard amp from 1978. | audio 1 in / 1 out | Ready |
| VBass Pre Amp (GxPlugins) | Simulation of the 1984 Vox Venue Bass 100 Pre Amp section. | audio 1 in / 1 out | Ready |
| Vox MKII (GxPlugins) | Based on Vox MKII solid state preamp of the late 60s. | audio 1 in / 1 out | Ready |

## Cabinet

| Module | What it does | Connections | Status |
| --- | --- | --- | --- |
| Cabinet - Mono | Cabinet from an impulse response file. Mono input and output; use a mono IR. | audio 1 in / 1 out | Ready |
| Cabinet - Mono to Stereo | Cabinet from an impulse response file. Mono input and stereo output; use a stereo IR. | audio 1 in / 2 out | Ready |
| Cabinet - Stereo | Cabinet from an impulse response file. Stereo input and output. Mono or stereo IRs process left and right separately; four-channel IRs add cross-channel responses. | audio 2 in / 2 out | Ready |
| Gx Cabinet (built-in) | Cabinet simulator with built-in speaker models. No impulse response file needed. | audio 1 in / 1 out | Ready |
| Ultra Cab (built-in) (GxPlugins) | Cabinet simulator pedal with built-in processing. No impulse response file needed. | audio 2 in / 2 out | Ready |

## Dynamics

| Module | What it does | Connections | Status |
| --- | --- | --- | --- |
| Compressor | Controls dynamics of a mono signal. | audio 1 in / 1 out | Ready |
| Compressor stereo | Controls dynamics of a stereo signal. | audio 2 in / 2 out | Ready |
| De-ess | Reduces harsh high-frequency peaks. | audio 2 in / 2 out | Ready |
| Dynamics | Compressor, limiter and gate for controlling signal level. | audio 2 in / 2 out | Ready |
| Gx Compressor | Controls signal dynamics with compression and output gain. | audio 1 in / 1 out | Ready |
| Gx Expander | Expands dynamics to reduce quiet background sound. | audio 1 in / 1 out | Ready |
| Gx Multiband compressor | Compresses separate frequency bands. | audio 1 in / 1 out | Ready |
| Gx Sustainer | Controls level to extend sustain. | audio 1 in / 1 out | Ready |
| Limiter | Limits loud peaks. | audio 2 in / 2 out | Ready |
| Loudness | Changes perceived loudness with level-dependent filtering. | audio 2 in / 2 out | Ready |
| MultiBand | Compresses low, middle and high frequency bands separately. | audio 2 in / 2 out | Ready |
| Slow gear (GxPlugins) | Automatically swells volume to remove note attack, for violin-like playing. | audio 1 in / 1 out | Ready |
| Splitter | Splits the signal by frequency or level, to treat lows and highs or loud and quiet parts apart. | audio 2 in / 2 out | Ready |
| Transient | Reshapes the attack and sustain of the input. | audio 2 in / 2 out | Ready |

## Filter

| Module | What it does | Connections | Status |
| --- | --- | --- | --- |
| Diode ladder | A diode ladder low pass filter similar to the vintage Japanese designs. | audio 1 in / 1 out; CV 2 in / 0 out | Ready |
| Gx Auto wah | Automatic wah filtering driven by the input signal. | audio 1 in / 1 out | Ready |
| Gx Bark graphic EQ | Graphic equalizer with frequency bands spaced on the Bark scale. | audio 1 in / 1 out | Ready |
| Gx Crybaby GCB95 | Crybaby-style wah filter with a sweep control. | audio 1 in / 1 out | Ready |
| Gx Graphic EQ | Graphic equalizer with separate frequency-band gains. | audio 1 in / 1 out | Ready |
| Gx Switchless wah | Wah filter with automatic switching. | audio 1 in / 1 out | Ready |
| Gx Tilt tone | Tilts the balance between low and high frequencies. | audio 1 in / 1 out | Ready |
| Gx Wah | Wah filter with a manual sweep control. | audio 1 in / 1 out | Ready |
| Gx Wahwah | Wah filter with a sweep control. | audio 1 in / 1 out | Ready |
| Korg 35 high-pass | A high pass filter similar to the vintage Japanese designs. | audio 1 in / 1 out; CV 2 in / 0 out | Ready |
| Korg 35 low-pass | A low pass filter similar to the vintage Japanese designs. | audio 1 in / 1 out; CV 2 in / 0 out | Ready |
| Moog half ladder | A low pass filter inspired by vintage American designs. | audio 1 in / 1 out; CV 2 in / 0 out | Ready |
| Moog ladder | Classic resonant ladder low pass filter inspired by vintage American designs. | audio 1 in / 1 out; CV 2 in / 0 out | Needs adaptation: Produced non-finite samples from a plain sine in the compatibility check. |
| Moog Low-Pass Filter 1 | Resonant ladder low pass filter with modulation inputs for cutoff and resonance. | audio 1 in / 1 out; CV 3 in / 0 out | Ready |
| Moog LPF | Resonant ladder low pass filter in the style of vintage American synths. | audio 1 in / 1 out; CV 3 in / 0 out | Ready |
| Oberheim filter | A multi out filter inspired by vintage American designs. | audio 1 in / 4 out; CV 2 in / 0 out | Ready |
| Parametric EQ | Adjusts selected frequency bands of a mono signal. | audio 1 in / 1 out | Ready |
| Parametric EQ stereo | Adjusts selected frequency bands of a stereo signal. | audio 2 in / 2 out | Ready |
| Quack (GxPlugins) | Autowah. | audio 1 in / 1 out | Ready |
| RezFilter | Resonant filter with envelope and LFO modulation. | audio 2 in / 2 out | Ready |
| TalkBox | Shapes a carrier with another signal; needs both audio inputs connected. | audio 2 in / 2 out | Ready |
| VCF | Resonant multi-mode filter whose cutoff follows a control signal. | audio 1 in / 1 out; CV 4 in / 0 out | Ready |
| VocInput | Prepares a voice or other input for a vocoder. | audio 2 in / 2 out | Ready |
| Vocoder | Shapes a carrier with another signal; needs both audio inputs connected. | audio 2 in / 2 out | Ready |

## Modulation

| Module | What it does | Connections | Status |
| --- | --- | --- | --- |
| Chorus | 8 voice multi-dimensional chorus with internal LFO. | audio 1 in / 2 out | Ready |
| Chorus (CV) | 8 voice multi-dimensional chorus with CV for the LFO. | audio 1 in / 2 out; CV 1 in / 0 out | Ready |
| Flanger | Flanger with internal LFO. | audio 1 in / 1 out | Ready |
| Flanger (CV) | Flanger with CV for the LFO. | audio 1 in / 1 out; CV 1 in / 0 out | Ready |
| Gx Chorus stereo | Stereo chorus with rate and depth controls. | audio 2 in / 2 out | Ready |
| Gx Flanger | Sweeping comb-filter modulation with rate and depth controls. | audio 1 in / 1 out | Ready |
| Gx Phaser | Sweeping phase-shift effect. | audio 1 in / 1 out | Ready |
| Gx Switched tremolo | Switched tremolo for pronounced rhythmic level changes. | audio 1 in / 1 out | Ready |
| Gx Tremolo | Rhythmic volume modulation. | audio 1 in / 1 out | Ready |
| Gx Tube tremolo | Tube-style rhythmic volume modulation. | audio 1 in / 1 out | Ready |
| Gx Tube vibrato | Tube-style pitch modulation. | audio 1 in / 1 out | Ready |
| Harmonic tremolo | Harmonic tremolo with internal LFO. | audio 1 in / 1 out | Ready |
| Harmonic tremolo (CV) | Harmonic tremolo with CV for the LFO. | audio 1 in / 1 out; CV 1 in / 0 out | Ready |
| Leslie | Rotating speaker simulation. | audio 2 in / 2 out | Ready |
| Ring Modulator - Audio | Ring modulator: multiplies two audio signals for metallic, bell-like tones. | audio 1 in / 1 out; CV 1 in / 0 out | Ready |
| Ring Modulator - CV | Multiplies two control signals. | CV 2 in / 1 out | Ready |
| RingMod | Ring modulation for metallic, bell-like tones. | audio 2 in / 2 out | Ready |
| Rotary speaker | A rotating loudspeaker using physical modelling. | audio 1 in / 2 out | Ready |
| Rotary speaker (full) | A rotating loudspeaker using physical modelling. Same sound, more controls. | audio 1 in / 2 out | Ready |
| RoundPan | Moves the sound between left and right. | audio 2 in / 2 out | Ready |
| Stone phaser | Warm vintage-style phaser with an internal LFO. | audio 1 in / 1 out | Ready |
| Stone phaser stereo | Warm vintage-style stereo phaser with an internal LFO. | audio 2 in / 2 out | Ready |
| ThruZero | Through-zero flanging. | audio 2 in / 2 out | Ready |
| Twist Binaural Doppler Panner | Binaural panner, allows positioning in 3D. | audio 2 in / 2 out; CV 4 in / 0 out | Ready |
| Twist Bitmangler | Brutal bitmangler. Warning, can get very loud. | audio 2 in / 2 out; CV 4 in / 0 out | Ready |
| Twist Chebyshev | Chebyshev wave folder for bright, harmonically rich distortion. | audio 2 in / 2 out; CV 4 in / 0 out | Ready |
| Twist Comparator | Combines two signals by comparing them, for harsh digital tones. | audio 2 in / 2 out; CV 4 in / 0 out | Ready |
| Twist Delay | A delay where speed and length interact with quality. | audio 2 in / 2 out; CV 4 in / 0 out | Ready |
| Twist Frequency Shifter | Shifts every frequency by the same amount, for clangorous and barber-pole effects. | audio 2 in / 2 out; CV 4 in / 0 out | Ready |
| Twist Meta | A powerful cross modulation module which applies an algorithm to the two inputs. | audio 2 in / 2 out; CV 4 in / 0 out | Ready |
| Twist Vocoder | Vocoder: shapes the carrier with the spectrum of the modulator. | audio 2 in / 2 out; CV 4 in / 0 out | Ready |
| Twist Wavefolder | Wave folder for complex, buzzy overtones. | audio 2 in / 2 out; CV 4 in / 0 out | Ready |
| Vibrato | Vibrato with internal LFO. | audio 1 in / 1 out | Ready |
| Vibrato (CV) | Vibrato with CV for the LFO. | audio 1 in / 1 out; CV 1 in / 0 out | Ready |
| YK Chorus | Chorus for widening and thickening the input. | audio 2 in / 2 out | Ready |

## Pitch

| Module | What it does | Connections | Status |
| --- | --- | --- | --- |
| Detune | Small pitch offsets for doubling and thickening. | audio 2 in / 2 out | Ready |
| Gx Detune | Pitch detuning and octave effects with wet and dry levels. | audio 1 in / 1 out | Ready |
| Gx OC-2 | Octave-down effect with separate octave and dry levels. | audio 1 in / 1 out | Ready |
| Mist Stretch | A granular time stretching and pitch shifting module. | audio 2 in / 2 out; CV 12 in / 0 out | Ready |
| Pitch Shifter | Changes the pitch of the incoming audio. | audio 1 in / 1 out | Ready |
| RePsycho! | Audio-triggered pitch shifting and sample playback. | audio 2 in / 2 out | Ready |
| Shepard | Shepard-tone processing for an endlessly rising or falling illusion. | audio 2 in / 2 out | Ready |
| SubSynth | Adds synthesized low-frequency content below the input. | audio 2 in / 2 out | Ready |

## Delay

| Module | What it does | Connections | Status |
| --- | --- | --- | --- |
| Aura | An always-listening micro-looper and reverb, delay or sampler sharing one clock. Clock slows both together. Stereo Spread and six CV inputs shape the sound. | audio 2 in / 2 out; CV 6 in / 0 out | Ready |
| Bitcrushed delay | Flexible delay module with bitcrushed repeats. | audio 1 in / 1 out; CV 6 in / 0 out | Needs adaptation: Has crashed the engine; see docs/22-plugin-catalogue.md. |
| CV delay | Delays a control signal by up to 10 seconds. Use with LFOs, envelopes or sequencers. | CV 1 in / 1 out | Ready |
| Discipline | Delay with a 3-minute memory you can scan back into. Works alone. Modifier Outside sends the repeats through your modules to a Discipline Return. | audio 2 in / 4 out; CV 6 in / 1 out | Ready |
| DubDelay | Modulated echo with feedback for dub-style repeats. | audio 2 in / 2 out | Ready |
| Gx Delay stereo | Stereo delay with time, feedback and mix controls. | audio 2 in / 2 out | Ready |
| Gx Digital delay | Mono digital delay with feedback and mix controls. | audio 1 in / 1 out | Ready |
| Gx Digital delay stereo | Stereo digital delay with feedback and mix controls. | audio 2 in / 2 out | Ready |
| Gx Duck delay | Mono delay that reduces the repeats while the input is playing. | audio 1 in / 1 out | Ready |
| Gx Duck delay stereo | Stereo delay that reduces the repeats while the input is playing. | audio 2 in / 2 out | Ready |
| Gx Echo Cat | Echo effect with controls for its repeat character. | audio 1 in / 1 out | Ready |
| Gx Echo stereo | Stereo echo effect with feedback and level controls. | audio 2 in / 2 out | Ready |
| Gx Multiband delay | Applies delay to separate frequency bands. | audio 1 in / 1 out | Ready |
| Gx Multiband echo | Echo processing with separate frequency bands. | audio 1 in / 1 out | Ready |
| Gx Tube delay | Tube-style delay effect. | audio 1 in / 1 out | Ready |
| Loo-pi | Eight stereo loops: record, overdub, multiply, insert, reverse, half and double speed, sync to the tempo or a loop. Open its page for loop cards and commands. | audio 2 in / 2 out; CV 0 in / 2 out | Ready |
| Mist Beat Repeat | Clocked beat repeat: loops and slices recent audio, with pitch and feedback. | audio 2 in / 2 out; CV 12 in / 0 out | Needs adaptation: Most controls do nothing in this port: its beat-repeat settings are never set (see docs/20-clouds.md, the Mist modules). |
| Mist Looper | Granular pitch shifting, micro looping delay. Freeze loops what is in the buffer. | audio 2 in / 2 out; CV 12 in / 0 out | Ready |
| Stereo delay | Stereo echo with independent left/right delay times, feedback and filtering. | audio 2 in / 2 out | Ready |
| Tempo delay | Flexible delay timed in beats, so it follows the patch tempo. Only the echoes come out: cable the dry signal to OUT as well. | audio 1 in / 1 out; MIDI/events 1 in / 0 out | Ready |

## Reverb

| Module | What it does | Connections | Status |
| --- | --- | --- | --- |
| Dragonfly Hall | Hall reverb with early and late reflections, modulation, filters and decay controls. | audio 2 in / 2 out | Ready |
| Dragonfly Plate | Plate reverb with selectable algorithms, damping and decay controls. | audio 2 in / 2 out | Ready |
| Dragonfly Room | Room reverb with separate early and late levels, size, filters and decay controls. | audio 2 in / 2 out | Ready |
| Gx Multiband reverb | Reverb processing with separate frequency bands. | audio 1 in / 1 out | Ready |
| Gx Shimmizita | Shimmer reverb combining pitch shifting and a reverb tail. | audio 2 in / 2 out | Ready |
| IR Reverb - Mono | IR Reverb from an impulse response file. Mono input and output; use a mono IR. | audio 1 in / 1 out | Ready |
| IR Reverb - Mono to Stereo | IR Reverb from an impulse response file. Mono input and stereo output; use a stereo IR. | audio 1 in / 2 out | Ready |
| IR Reverb - Stereo | IR Reverb from an impulse response file. Stereo input and output. Mono or stereo IRs process left and right separately; four-channel IRs add cross-channel responses. | audio 2 in / 2 out | Ready |
| Mist Oliverb | A special effects reverb with pitch-shifted, modulated tails. Hold sustains the tail. | audio 2 in / 2 out; CV 12 in / 0 out | Ready |

## Texture

| Module | What it does | Connections | Status |
| --- | --- | --- | --- |
| Beat Repeater - Mono | Repeats a slice of the last beat while the gate is high, like a stutter edit. | audio 1 in / 1 out; CV 1 in / 0 out | Ready |
| Beat Repeater - Stereo | Stereo beat repeater: repeats a slice of the last beat while the gate is high. | audio 2 in / 2 out; CV 1 in / 0 out | Ready |
| Beat Slicer - Mono | Records a few beats and plays back slices of them while the gate is high, optionally reversed. | audio 1 in / 1 out; CV 1 in / 0 out | Ready |
| Beat Slicer - Stereo | Stereo beat slicer: plays back slices of the last few beats while the gate is high. | audio 2 in / 2 out; CV 1 in / 0 out | Ready |
| Drizzle | Grains of the live input, with reverb; Size at the top makes it a delay. Freeze holds the recording. With Seed lock on grains run at the Density rate; off, they play while Seed is held. | audio 2 in / 2 out; CV 8 in / 1 out | Ready |
| Granulator - Mono | Chops the input into short overlapping grains for smeared, cloud-like textures. | audio 1 in / 1 out | Ready |
| Granulator - Stereo | Stereo granulator: chops the input into overlapping grains for cloud-like textures. | audio 2 in / 2 out | Ready |
| Mist | Granular texture from the live input. Freeze holds the buffer. Blend mixes in the clean input: all of it up to the middle, none at the top. | audio 2 in / 2 out; CV 12 in / 0 out | Ready |
| Mist Spectral | Spectral processor that blurs, freezes and reshapes the input in the frequency domain. | audio 2 in / 2 out; CV 12 in / 0 out | Ready |
| Power cut | Simulates turning off a turntable: the sound slows down and stops. | audio 1 in / 1 out | Ready |
| Reverse (Reflector) | Reverse effect. Try a short fragment length for a weird tremolo. | audio 1 in / 1 out | Ready |
| Stuck (freeze) | Holds what you are playing when the control level is active, creating a drone. | audio 1 in / 1 out | Ready |
| Vinyl | Emulates dirty old lo-fi vinyl records: crackle, hum, wow and flutter. | audio 2 in / 2 out; MIDI/events 1 in / 1 out | Ready |

## Resonator

| Module | What it does | Connections | Status |
| --- | --- | --- | --- |
| Bell Resonator | Resonator simulating multiple vibrating structures: strings, modes and more. | audio 1 in / 2 out; CV 7 in / 0 out | Ready |
| Chamber | A bell, plate or strings that your guitar rings at its own pitch: cable it to Strike in, no gate needed. It starts as a long bell with reverb. Raise Strike, Bow or Blow level to play it with Play or a gate; Blow in sounds only while one is held. | audio 2 in / 2 out; CV 14 in / 0 out | Ready |
| Mist Resonestor | Dual voice, four part resonator played by the input. | audio 2 in / 2 out; CV 12 in / 0 out | Ready |

## Sound source

| Module | What it does | Connections | Status |
| --- | --- | --- | --- |
| BeatBox | Triggers synthesized drum sounds from incoming audio. | audio 2 in / 2 out | Ready |
| Drizzle Synth | A wavetable voice played as grains, with reverb. Needs no input. Wavetable picks the sound, Osc/grains mixes the plain voice with its grains, and Pitch CV takes volt per octave. | audio 0 in / 2 out; CV 8 in / 1 out | Ready |
| DX10 | FM synthesizer voice for bells, keys and basses. | audio 0 in / 2 out; MIDI/events 1 in / 0 out | Needs adaptation: Played by MIDI notes; not yet checked with a MIDI controller. |
| ePiano | Electric piano instrument. | audio 0 in / 2 out; MIDI/events 1 in / 0 out | Needs adaptation: Played by MIDI notes; not yet checked with a MIDI controller. |
| JX10 | Polyphonic analog-style synthesizer. | audio 0 in / 2 out; MIDI/events 1 in / 0 out | Needs adaptation: Played by MIDI notes; not yet checked with a MIDI controller. |
| Noise2 - Audio | Noise source: white, random or pink noise as audio. | audio 0 in / 1 out | Ready |
| Piano | Acoustic piano instrument. | audio 0 in / 2 out; MIDI/events 1 in / 0 out | Needs adaptation: Played by MIDI notes; not yet checked with a MIDI controller. |
| TestTone | Generates test tones for checking a patch. | audio 2 in / 2 out | Ready |
| Tracker | Pitch-tracking oscillator played by incoming audio. | audio 2 in / 2 out | Ready |
| vco2_audio | Oscillator with several wave shapes, tuned by a pitch control signal. | No patchable connectors | Unavailable: This module has no audio, control or MIDI connectors to patch. |
| vco2_cv | Oscillator with several wave shapes that sends a control signal. | No patchable connectors | Unavailable: This module has no audio, control or MIDI connectors to patch. |
| Weave Oscillator | A powerful multi-model oscillator voice. | audio 0 in / 2 out; CV 8 in / 0 out | Ready |

## Control

| Module | What it does | Connections | Status |
| --- | --- | --- | --- |
| 16 Step Note Sequencer | 16 step note sequencer that takes in a trigger for next or previous. | CV 3 in / 1 out | Ready |
| 16 Step Note Sequencer BPM | 16 step note sequencer with internal clock. | CV 3 in / 1 out | Ready |
| 16 Step Sequencer | 16 step sequencer that takes in a trigger for next or previous. | CV 3 in / 1 out | Ready |
| 16 Step Sequencer BPM | 16 step sequencer with internal clock. | CV 3 in / 1 out | Ready |
| ADSR Envelope | Basic ADSR envelope generator. Takes in a gate. | CV 1 in / 1 out | Ready |
| Advanced Envelope | Multi-stage envelope generator with four attack and three release stages. Takes in a gate. | CV 2 in / 2 out | Ready |
| Analog Driver | Adds slow drift and detune to a pitch CV, for the slightly unstable feel of analog oscillators. | CV 1 in / 2 out | Ready |
| Clock divider | Outputs a trigger every n triggers. | CV 1 in / 1 out | Ready |
| CV Source | Four fixed control voltages, each with coarse and fine knobs. | CV 0 in / 4 out | Ready |
| CV to trigger | A Schmitt trigger that converts a control signal to on and off triggers. | CV 1 in / 1 out | Ready |
| Dice Random Sampler | Powerful repeatable randomness source: random gates and control signals. | CV 9 in / 7 out | Ready |
| Envelope | ADSR envelope generator with delay and hold stages. Takes in a gate. | CV 2 in / 2 out | Ready |
| Envelope follower | Tracks an input signal and converts it into a control signal. | audio 1 in / 1 out; CV 0 in / 1 out | Ready |
| Euclidean | A euclidean sequencer with 4 tracks. Connect a trigger to go to the next step. | CV 3 in / 8 out | Ready |
| Flow Envelope | A powerful envelope and LFO generator with four related outputs. | CV 8 in / 4 out | Ready |
| Foot switch | A stomp as a control signal: On value while it is down, Off value otherwise. Latching flips it on each press instead. | CV 0 in / 1 out | Ready |
| Lattice Drum Sequencer | Drum trigger explorer for kick, snare and hi-hat. Expects a 4 ppq clock if using external clock. | CV 10 in / 6 out | Ready |
| LFO - Freq | Low frequency oscillator with several wave shapes, set by rate. Sends a control signal. | CV 1 in / 1 out | Ready |
| LFO - Tempo | Low frequency oscillator synced to a tempo, with several wave shapes. Sends a control signal. | CV 1 in / 1 out | Ready |
| MIDI clock | MIDI clock at the patch tempo on a MIDI cable, with start and stop. | MIDI/events 0 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI clock in | Follows incoming MIDI clock: tempo, start and pulse outputs. | CV 0 in / 3 out; MIDI/events 1 in / 0 out | Ready |
| MIDI controller | One MIDI control change as a control value, to cable to any control. | CV 0 in / 1 out; MIDI/events 1 in / 0 out | Ready |
| MIDI program change | Sends MIDI program changes to switch presets: a chosen program, or the next or previous one on a trigger. | CV 3 in / 0 out; MIDI/events 0 in / 1 out | Ready |
| MIDI to CV | Plays CV modules from MIDI notes: pitch, gate, velocity, a trigger on each new note, mod wheel and pressure. | CV 0 in / 6 out; MIDI/events 1 in / 0 out | Ready |
| MIDI to CV poly | Plays chords on up to four CV voices from MIDI notes: pitch, gate and velocity for each voice, mod wheel and pressure. | CV 0 in / 14 out; MIDI/events 1 in / 0 out | Ready |
| Noise2 - CV | Random control signal: noise or stepped random values at a set rate. | CV 0 in / 1 out | Ready |
| Percussive Envelope | Short attack decay envelope for plucks and hits. Takes in a gate. | CV 1 in / 2 out | Ready |
| Product | a times b for control signals. | No patchable connectors | Unavailable: This module has no audio, control or MIDI connectors to patch. |
| Quantizer | Quantizes a volt per octave signal to a musical scale. | CV 1 in / 2 out | Ready |
| Retriggerable DAHDSR Envelope | A delay attack hold decay sustain release envelope generator. Can be retriggered. | CV 2 in / 1 out | Ready |
| Sample&Hold | Samples a control value when a trigger arrives and holds it until the next one. | CV 2 in / 2 out | Ready |
| Strum | MIDI and volt per octave controller based on strumming. | CV 0 in / 2 out; MIDI/events 0 in / 1 out | Ready |
| Toggle | Toggles the output value on every trigger. | CV 1 in / 1 out | Ready |
| Trigger to gate | Takes in a trigger and outputs a gate with a length related to the tempo. | CV 2 in / 1 out | Ready |

## Utility

| Module | What it does | Connections | Status |
| --- | --- | --- | --- |
| Absolute | Rectify or get the absolute value of the input. | CV 1 in / 1 out | Ready |
| Amp | Scales a control signal by a fixed gain; negative gain inverts it. | CV 1 in / 1 out | Ready |
| Amplitude Modifier | Rescales a control signal from one range to another. | CV 1 in / 1 out | Ready |
| Control to CV | Turns a knob into a control signal you can patch to other modules. | CV 0 in / 1 out | Ready |
| CV meter | Simple CV meter that shows instant values with no smoothing. | No patchable connectors | Unavailable: This module has no audio, control or MIDI connectors to patch. |
| CV to Control | Turns a control signal back into a knob value. | CV 1 in / 0 out | Ready |
| CV to MIDI CC | Converts a control signal to MIDI CC. | CV 1 in / 0 out; MIDI/events 0 in / 1 out | Ready |
| CV to MIDI Note | Converts volt per octave pitch to MIDI notes. Velocity is optional. | CV 3 in / 0 out; MIDI/events 0 in / 1 out | Ready |
| Difference | a - b for control signals. | CV 2 in / 1 out | Ready |
| Discipline Return | Brings Discipline's repeats back in when its Modifier is Outside. One per patch. | audio 2 in / 0 out | Ready |
| Fifths | Adds a fifth above every MIDI note. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| Hz to VC | Converts a frequency in Hz into a volt per octave pitch signal. | CV 1 in / 1 out | Ready |
| Image | Adjusts stereo width and the balance of mid and side. | audio 2 in / 2 out | Ready |
| Inv | Inverts a control signal: up becomes down. | CV 1 in / 1 out | Ready |
| Maximum | Max of a, b; also logical or. | CV 2 in / 1 out | Ready |
| Metronome | Metronome click that follows the host tempo. | audio 0 in / 1 out | Ready |
| MIDI CC Map | Changes one MIDI control message into another. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI CC to Note | Translates MIDI control changes to note on and off messages. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Channel Filter | Discards MIDI messages per channel. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Channel Map | Maps any MIDI channel to another MIDI channel. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Channel Unisono | Duplicates MIDI events from one channel to another. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Choke | Triggers note-off events to create exclusive note groups. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Chord | Creates MIDI chords from a single note in a given musical scale. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Chromatic Transpose | Chromatically transposes MIDI notes. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Delayline | Delays MIDI events, with optional randomization. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Duplicate Blocker | Blocks duplicate MIDI notes. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Enforce Scale | Forces MIDI notes onto a given musical scale. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Event Filter | Notch-style MIDI filter that suppresses specific messages. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Gate | Lets audio through only while a MIDI note is held. | audio 1 in / 1 out; MIDI/events 1 in / 0 out | Ready |
| MIDI Key-Range Filter | Discards MIDI notes outside a given range. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Keysplit | Changes MIDI channel depending on the note, with optional transpose. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Monophonic Legato | Holds a MIDI note until the next note arrives. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI N-Tap Delay | Repeats MIDI notes a number of times, with tempo ramps. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Note to CC | Converts MIDI note-on messages to control changes. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Note to PC | Converts MIDI notes to program change messages. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Note Toggle | Play a MIDI note to turn it on, play it again to turn it off. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Note Transpose | Flexible 12-tone MIDI note map. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Note/Channel Map | Sends each of the 12 notes to its own MIDI channel. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Quantization | Quantizes live MIDI events to a time grid. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Remove Active Sensing | Strips MIDI active-sensing messages. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Scale CC Value | Scales the value of a MIDI control change. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Simple Channel Filter | Basic MIDI channel filter. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Sostenuto | Delays MIDI note-offs, like a piano sostenuto pedal. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Strum | MIDI arpeggio that simulates strumming a guitar. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Tonal Pedal | Holds the notes being played while the sustain pedal is down. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Velocity Adjust | Scales and offsets MIDI note velocity. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Velocity Gamma | Reshapes the MIDI velocity curve. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Velocity Randomization | Randomly changes the velocity of MIDI notes. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| MIDI Velocity-Range Filter | Filters MIDI notes by velocity. | MIDI/events 1 in / 1 out | Needs adaptation: Works on MIDI only; not yet checked with a MIDI controller. |
| Minimum | Min of a, b; also logical and. | CV 2 in / 1 out | Ready |
| Mixer 2 channels - Audio | Mixes two audio signals with a level for each. | audio 2 in / 1 out | Ready |
| Mixer 2 channels - CV | Mixes two control signals with a level for each. | CV 2 in / 1 out | Ready |
| Mixer 4 channels - Audio | Mixes four audio signals with a level for each. | audio 4 in / 1 out | Ready |
| Mixer 4 channels - CV | Mixes four control signals with a level for each. | CV 4 in / 1 out | Ready |
| Parameters | Test module for plugin parameters. Does nothing to the sound. | No patchable connectors | Unavailable: This module has no audio, control or MIDI connectors to patch. |
| Pitch calibration in | Pitch calibration for a volt per octave input. | CV 1 in / 1 out | Ready |
| Pitch calibration out | Pitch calibration for a volt per octave output. | CV 1 in / 1 out | Ready |
| Product | a times b for control signals. | CV 2 in / 1 out | Ready |
| Ratio | a divided by b for control signals. | CV 2 in / 1 out | Ready |
| Scope | Oscilloscope view of a signal. | No patchable connectors | Unavailable: This module has no audio, control or MIDI connectors to patch. |
| Scope (Mono) | Oscilloscope view of a mono signal. Audio passes through unchanged. | audio 1 in / 1 out | Ready |
| Scope (Stereo) | Oscilloscope view of a stereo signal. Audio passes through unchanged. | audio 2 in / 2 out | Ready |
| Signal Absolute Value | Rectifies a control signal: negative values become positive. | CV 1 in / 1 out | Ready |
| Slew Limiter | Slows how fast a control signal changes, with separate rise and fall times. Useful with foot switches. | CV 1 in / 1 out | Ready |
| Stereo | Turns a mono signal into a wide stereo image with a short delay. | audio 2 in / 2 out | Ready |
| Sum | a + b for control signals. | CV 2 in / 1 out | Ready |
| tempo_ratio | Ratio between tempos, like 3/4 of the input tempo. | No patchable connectors | Unavailable: This module has no audio, control or MIDI connectors to patch. |
| Tuner | Chromatic instrument tuner. Shows the note and how flat or sharp it is; connect your input and play one string. | audio 1 in / 1 out; MIDI/events 0 in / 1 out | Ready |
| VC Panning | Pans audio between two outputs from a control signal. | audio 1 in / 2 out; CV 1 in / 0 out | Ready |
| VC Switch - Audio | Switches or crossfades between two audio signals from a control signal. | audio 2 in / 3 out; CV 1 in / 0 out | Ready |
| VC Switch - CV | Switches or crossfades between two control signals from a control signal. | CV 3 in / 3 out | Ready |
| VC to Hz | Converts a volt per octave pitch signal into a frequency in Hz. | CV 1 in / 1 out | Ready |
| VCA | Simple voltage controlled amplifier. Sets audio level from a control signal. | audio 1 in / 1 out; CV 1 in / 0 out | Ready |
| VCA Exp - Audio | Voltage controlled amplifier with an exponential response. Sets audio level from a control signal. | audio 2 in / 1 out; CV 2 in / 0 out | Ready |
| VCA Exp - CV | Voltage controlled amplifier with an exponential response, for control signals. | CV 4 in / 1 out | Ready |
| VCA Lin - Audio | Voltage controlled amplifier with a linear response. Sets audio level from a control signal. | audio 2 in / 1 out; CV 2 in / 0 out | Ready |
| VCA Lin - CV | Voltage controlled amplifier with a linear response, for control signals. | CV 4 in / 1 out | Ready |

## Hidden identities

| Module | Category | Why it is hidden | Plugin identity |
| --- | --- | --- | --- |
| Boost | Drive | Redundant clean boost; use Gain. | `http://polyeffects.com/lv2/basic_modular#boost` |
| IR Convolver Mono | Reverb | Legacy convolution identity; use Cabinet or IR Reverb for new patches. | `http://gareus.org/oss/lv2/zeroconvolv#CfgMono` |
| IR Convolver Mono => Stereo | Reverb | Legacy convolution identity; use Cabinet or IR Reverb for new patches. | `http://gareus.org/oss/lv2/zeroconvolv#CfgMonoToStereo` |
| IR Convolver Stereo | Reverb | Legacy convolution identity; use Cabinet or IR Reverb for new patches. | `http://gareus.org/oss/lv2/zeroconvolv#CfgStereo` |
| LV2 Convolution Mono | Reverb | Legacy convolution identity; use Cabinet or IR Reverb for new patches. | `http://gareus.org/oss/lv2/convoLV2#Mono` |
| LV2 Convolution Mono=>Stereo | Reverb | Legacy convolution identity; use Cabinet or IR Reverb for new patches. | `http://gareus.org/oss/lv2/convoLV2#MonoToStereo` |
| LV2 Convolution Stereo | Reverb | Legacy convolution identity; use Cabinet or IR Reverb for new patches. | `http://gareus.org/oss/lv2/convoLV2#Stereo` |
| Tuner | Utility | Duplicate tuner; use Tuner. | `http://gareus.org/oss/lv2/tuna#one` |
| Tuner | Utility | Duplicate tuner; use Tuner. | `http://gareus.org/oss/lv2/tuna#two` |

## Not installed

These are reference identities, not available effects. The installed Foot switch, mixers and other modules can cover some use cases without implementing the original identity.

| Reference module | Reason / reference description | Plugin identity |
| --- | --- | --- |
| Attack/decay envelope | Debian's omins package is LADSPA only; its LV2 port is not packaged. Reference: A attack decay envelope generator. Works with a trigger. | `http://drobilla.net/plugins/omins/adenv` |
| Attack/decay envelope with levels | Debian's omins package is LADSPA only; its LV2 port is not packaged. Reference: An attack decay envelope generator with start and end levels. Works with a trigger. | `http://drobilla.net/plugins/omins/adenv_lvl` |
| Bitcrusher | no public implementation known; Beebo maps it without a source. Reference: basic bit reduction bit crusher, CV control of bits. | `http://polyeffects.com/lv2/bitcrusher` |
| Looper extra MIDI | no public implementation known; Beebo maps it without a source. Reference: Connect internal MIDI generators to the Looper, do not connect external MIDI here, will duplicate. | `http://polyeffects.com/lv2/loop_extra_midi` |
| Looper input | no public implementation known; Beebo maps it without a source. Reference: Common input to the loopler | `http://polyeffects.com/lv2/loop_common_in` |
| Looper MIDI output | no public implementation known; Beebo maps it without a source. Reference: Ouput MIDI Clock from looper. | `http://polyeffects.com/lv2/loop_midi_out` |
| Looper output | no public implementation known; Beebo maps it without a source. Reference: Common output from the loopler | `http://polyeffects.com/lv2/loop_common_out` |
| Matrix mixer (4 in, 3 out) | MOD's 4-in 3-out build of x42's matrix mixer; Debian ships only the 8 x 8 one. Reference: 4 in 3 out matrix mixer | `http://gareus.org/oss/lv2/matrixmixer#i4o3` |
| MIDI program change | no public implementation known; Beebo maps it without a source. Reference: Send a MIDI program change message when triggered | `http://polyeffects.com/lv2/midi_pc` |
| MIDI to CV (mono) | no public implementation known; Beebo maps it without a source. Reference: convert MIDI notes to v per octave pitch CVs | `http://polyeffects.com/lv2/midi_to_cv_mono` |
| MIDI to CV (poly) | no public implementation known; Beebo maps it without a source. Reference: convert poly MIDI notes to v per octave pitch CVs | `http://polyeffects.com/lv2/midi_to_cv_poly` |
| Onset detector | no public implementation known; Beebo maps it without a source. Reference: detects when a note starts and sends a trigger | `http://polyeffects.com/lv2/detect#onset` |
| Original Poly foot switches | no public implementation known; Beebo maps it without a source. Reference: The right and centre footswitch. Also available in Hector to allow patch compatability. | `http://polyeffects.com/lv2/polyfoot` |
| Phaser (CV) | no public implementation known; Beebo maps it without a source. Reference: Mono phaser controlled by CV input | `http://polyeffects.com/lv2/stone-phaser-ext` |
| Phaser stereo (CV) | no public implementation known; Beebo maps it without a source. Reference: Stereo phaser controlled by CV input | `http://polyeffects.com/lv2/stone-phaser-stereo-ext` |
| Pitch detector | no public implementation known; Beebo maps it without a source. Reference: BETA detects played notes and converts it volt per octave and MIDI | `http://polyeffects.com/lv2/detect#pitch` |
| Poly LFO | archived Digit source (digit_carla), not built | `http://polyeffects.com/lv2/polylfo` |
| Three-band EQ | no public implementation known; Beebo maps it without a source. Reference: Amps style 3 knob tone stack | `http://polyeffects.com/lv2/poly_3_band_eq` |
| Wet/dry (mono) | no public implementation known; Beebo maps it without a source. Reference: blend between two inputs | `http://polyeffects.com/lv2/wet_dry` |
| Wet/dry (stereo) | no public implementation known; Beebo maps it without a source. Reference: blend stereo inputs to stereo out | `http://polyeffects.com/lv2/wet_dry_stereo` |

## Fixed endpoints

| Endpoint | Purpose |
| --- | --- |
| IN | Stereo input from the audio interface. |
| OUT | Stereo output to the audio interface. |

## Source and updates

Generated by `tools/document-catalogue.py` from the [Pi export](evidence/catalogue-pi-20261022.json), [presentation overlay](../config/catalogue/overlay.json) and [selected inventory](../config/catalogue/inventory.txt). The export contains installed identities; the overlay supplies Peepo's names, categories, hidden flags and restrictions. Exact URIs and control metadata for installed modules are in the export.

The older [compatibility matrix](evidence/compatibility-matrix.md) is a historical test record. [GxPlugins results](32-gxplugins.md) supplement it. Availability in this list follows the recorded Pi export and current overlay, not the historical PC matrix.

Regenerate after saving a new Pi export and updating the overlay:

```bash
python tools/document-catalogue.py
# For another recorded export:
python tools/document-catalogue.py --catalogue docs/evidence/catalogue-pi-YYYYMMDD.json --date YYYY-MM-DD
```
