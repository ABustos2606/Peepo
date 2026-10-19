# Patchstorage LV2 plugins compared with Peepo

**Patchstorage checked 19 October 2026. Peepo installation evidence: 19 October 2026.**

Every current Patchstorage LV2 entry is listed below. Peepo status uses the recorded Pi export plus its current presentation overlay. This is a comparison with the saved instrument evidence, not a fresh query of the running Pi.

Blokas documents Patchstorage as MODEP’s plugin cloud since MODEP 1.12. These are plugins available to install through MODEP, not a claim that every plugin is preinstalled or tested in Peepo. [Blokas MODEP](https://blokas.io/modep/); [Patchstorage platform](https://patchstorage.com/platform/lv2-plugins/).

## Summary

| Patchstorage entries | Count | Meaning |
| --- | ---: | --- |
| All ready | 88 | All inventoried identities are installed and offered in Peepo Add. |
| Partial bundle | 2 | Some identities are installed; others are missing. |
| Installed, restricted | 3 | All identities are installed, but at least one is disabled or unavailable. |
| No exact plugin installed | 293 | No inventoried identity appears in the recorded Peepo export. |
| Total | 386 | One row per Patchstorage publication, including bundle publications. |

The publications advertise **451 distinct LV2 identities**. Including additional identities found in the earlier archive inspection gives **553 inventoried identities**: **94 ready**, **2 needing adaptation**, **29 unavailable in the host**, and **428 not installed**. Identities are deduplicated for these totals; a bundle can contain many identities.

**Why are Gx effects missing when Peepo has all 43 GxPlugins?** The installed collection is [GxPlugins.lv2](https://github.com/brummer10/GxPlugins.lv2), upstream’s extra plugin set. The [main Guitarix project](https://github.com/brummer10/guitarix) also ships LV2 effects. Patchstorage includes both sets, so installing the whole extra collection does not install the whole Guitarix library. [Installed collection and native results](32-gxplugins.md).

## How to read the list

- **Ready** means selectable in Peepo, not a listening test or identical version of the Patchstorage build.
- **Pi64** means the publication advertises an `rpi-aarch64` download. It does not establish Ingen compatibility. Other advertised builds are shown too.
- **Ready/WIP** in the publication column is Patchstorage’s state, independent of Peepo’s status.
- An exact match uses the LV2 URI, not a similar name. Different builds with the same URI can differ in version or features.
- Related AMS/CV implementations are explicitly noted, with the earlier source review retained below. Other missing plugins have not received a functional-equivalence review.
- “Not installed” means absent from Peepo’s exported catalogue. It does not audit unselected bundles elsewhere on the Pi filesystem.

Build key: **Pi64** = `rpi-aarch64`; **PB32** = `patchbox-os-arm32`; **x64** = `linux-amd64`.

## All ready (88)

| Patchstorage plugin / bundle | Type | Advertised builds | Publication | Peepo module / detail |
| --- | --- | --- | --- | --- |
| [Dragonfly Early Reflections](https://patchstorage.com/dragonfly-early-reflections/) | Effect | x64, PB32, Pi64 | Ready | Dragonfly Early Reflections — Ready |
| [Dragonfly Hall Reverb](https://patchstorage.com/dragonfly-hall-reverb/) | Effect | x64, PB32, Pi64 | Ready | Dragonfly Hall — Ready |
| [Dragonfly Plate Reverb](https://patchstorage.com/dragonfly-plate-reverb/) | Effect | x64, PB32, Pi64 | Ready | Dragonfly Plate — Ready |
| [Dragonfly Room Reverb](https://patchstorage.com/dragonfly-room-reverb/) | Effect | x64, PB32, Pi64 | Ready | Dragonfly Room — Ready |
| [fil4.lv2 Bundle](https://patchstorage.com/fil4-lv2-bundle/) | Effect, Utility | x64, PB32, Pi64 | WIP | Parametric EQ stereo — Ready; Parametric EQ — Ready |
| [Gx Studio Preamp Stereo](https://patchstorage.com/gx-studio-preamp-stereo/) | Effect | x64, PB32, Pi64 | WIP | Gx Studio preamp stereo — Ready |
| [gx_redeye.lv2 Bundle](https://patchstorage.com/gx_redeye-lv2-bundle/) | Utility | x64, PB32, Pi64 | WIP | Gx Redeye Big Chump — Ready; Gx Redeye Chump — Ready; Gx Redeye Vibro Chump — Ready |
| [GxAlembic](https://patchstorage.com/gxalembic/) | Effect | x64, PB32, Pi64 | WIP | Gx Alembic preamp — Ready |
| [GxAmplifier-Stereo-X](https://patchstorage.com/gxamplifier-stereo-x/) | Effect | x64, PB32, Pi64 | WIP | Gx Amplifier stereo — Ready |
| [GxAmplifier-X](https://patchstorage.com/gxamplifier-x/) | Effect | x64, PB32, Pi64 | WIP | Gx Amplifier — Ready |
| [GxBaJaTubeDriver](https://patchstorage.com/gxbajatubedriver/) | Effect | x64, PB32, Pi64 | Ready | Baja Tube Driver — Ready |
| [GxBarkGraphicEQ](https://patchstorage.com/gxbarkgraphiceq/) | Effect, Utility | Pi64 | WIP | Gx Bark graphic EQ — Ready |
| [GxBooster](https://patchstorage.com/gxbooster/) | Utility | x64, PB32, Pi64 | WIP | Gx Booster — Ready |
| [GxBottleRocket](https://patchstorage.com/gxbottlerocket/) | Effect | x64, PB32, Pi64 | Ready | Bottle Rocket — Ready |
| [GxCabinet](https://patchstorage.com/gxcabinet/) | Effect | x64, PB32, Pi64 | WIP | Gx Cabinet — Ready |
| [GxChorus-Stereo](https://patchstorage.com/gxchorus-stereo/) | Effect, Utility | Pi64 | WIP | Gx Chorus stereo — Ready |
| [GxColorSound Tonebender](https://patchstorage.com/gxcolorsound-tonebender/) | Effect | x64, PB32, Pi64 | WIP | Gx ColorSound Tonebender — Ready |
| [GxCompressor](https://patchstorage.com/gxcompressor/) | Effect, Utility | x64, PB32, Pi64 | WIP | Gx Compressor — Ready |
| [GxCrybabyGCB95](https://patchstorage.com/gxcrybabygcb95/) | Effect | x64, PB32, Pi64 | Ready | Gx Crybaby GCB95 — Ready |
| [GxDelay-Stereo](https://patchstorage.com/gxdelay-stereo/) | Effect | x64, PB32, Pi64 | WIP | Gx Delay stereo — Ready |
| [Gxdetune](https://patchstorage.com/gxdetune/) | Effect | x64, PB32, Pi64 | WIP | Gx Detune — Ready |
| [Gxdigital_delay](https://patchstorage.com/gxdigital_delay/) | Effect | x64, PB32, Pi64 | WIP | Gx Digital delay — Ready |
| [Gxdigital_delay_st](https://patchstorage.com/gxdigital_delay_st/) | Effect | x64, PB32, Pi64 | WIP | Gx Digital delay stereo — Ready |
| [Gxduck_delay](https://patchstorage.com/gxduck_delay/) | Effect | x64, PB32, Pi64 | WIP | Gx Duck delay — Ready |
| [Gxduck_delay_st](https://patchstorage.com/gxduck_delay_st/) | Effect | x64, PB32, Pi64 | WIP | Gx Duck delay stereo — Ready |
| [GxEcho-Stereo](https://patchstorage.com/gxecho-stereo/) | Effect | x64, PB32, Pi64 | WIP | Gx Echo stereo — Ready |
| [GxEchoCat](https://patchstorage.com/gxechocat/) | Effect | Pi64 | WIP | Gx Echo Cat — Ready |
| [GxEpic](https://patchstorage.com/gxepic/) | Effect | x64, PB32, Pi64 | Ready | Epic — Ready |
| [GxExpander](https://patchstorage.com/gxexpander/) | Utility | x64, PB32, Pi64 | WIP | Gx Expander — Ready |
| [GxFlanger](https://patchstorage.com/gxflanger/) | Effect, Utility | x64, PB32, Pi64 | WIP | Gx Flanger — Ready |
| [GxFuzz](https://patchstorage.com/gxfuzz/) | Effect | x64, PB32, Pi64 | WIP | Gx Fuzz — Ready |
| [GxFuzzFaceFuller](https://patchstorage.com/gxfuzzfacefuller/) | Effect | x64, PB32, Pi64 | WIP | Gx Fuzz Face Fuller — Ready |
| [GxFuzzFaceJH2](https://patchstorage.com/gxfuzzfacejh2/) | Effect | x64, PB32, Pi64 | WIP | Gx Fuzz Face JH-2 — Ready |
| [GxFuzzMaster](https://patchstorage.com/gxfuzzmaster/) | Effect | x64, PB32, Pi64 | WIP | Gx Fuzz Master — Ready |
| [GxGraphicEQ](https://patchstorage.com/gxgraphiceq/) | Effect, Utility | Pi64 | WIP | Gx Graphic EQ — Ready |
| [GxGuvnor](https://patchstorage.com/gxguvnor/) | Effect | x64, PB32, Pi64 | Ready | Guvnor — Ready |
| [GxHF_Brightener](https://patchstorage.com/gxhf_brightener/) | Effect, Utility | x64, PB32, Pi64 | WIP | Gx HF Brightener — Ready |
| [GxHogsFoot](https://patchstorage.com/gxhogsfoot/) | Effect | x64, PB32, Pi64 | WIP | Gx Hogs Foot — Ready |
| [GxHornet](https://patchstorage.com/gxhornet/) | Effect | x64, PB32, Pi64 | WIP | Gx Hornet — Ready |
| [GxHotBox](https://patchstorage.com/gxhotbox/) | Effect | x64, PB32, Pi64 | Ready | Hot Box — Ready |
| [GxHyperion](https://patchstorage.com/gxhyperion/) | Effect | x64, PB32, Pi64 | Ready | Hyperion — Ready |
| [Gxjcm800pre](https://patchstorage.com/gxjcm800pre/) | Utility | x64, PB32, Pi64 | WIP | Gx JCM800 preamp — Ready |
| [Gxjcm800preST](https://patchstorage.com/gxjcm800prest/) | Utility | x64, PB32, Pi64 | WIP | Gx JCM800 preamp stereo — Ready |
| [GxKnightFuzz](https://patchstorage.com/gxknightfuzz/) | Effect | x64, PB32, Pi64 | Ready | Knight Fuzz — Ready |
| [GxLiquidDrive](https://patchstorage.com/gxliquiddrive/) | Effect | x64, PB32, Pi64 | Ready | Liquid Drive — Ready |
| [Gxlivelooper](https://patchstorage.com/gxlivelooper/) | Utility | Pi64 | WIP | Gx Live looper — Ready |
| [GxMole](https://patchstorage.com/gxmole/) | Effect | x64, PB32, Pi64 | Ready | Gx Mole — Ready |
| [GxMuff](https://patchstorage.com/gxmuff/) | Effect | x64, PB32, Pi64 | Ready | Gx Muff — Ready |
| [GxMultiBandCompressor](https://patchstorage.com/gxmultibandcompressor/) | Effect, Utility | Pi64 | WIP | Gx Multiband compressor — Ready |
| [GxMultiBandDelay](https://patchstorage.com/gxmultibanddelay/) | Effect | x64, PB32, Pi64 | WIP | Gx Multiband delay — Ready |
| [GxMultiBandDistortion](https://patchstorage.com/gxmultibanddistortion/) | Effect | Pi64 | WIP | Gx Multiband distortion — Ready |
| [GxMultiBandEcho](https://patchstorage.com/gxmultibandecho/) | Effect | Pi64 | WIP | Gx Multiband echo — Ready |
| [GxMultiBandReverb](https://patchstorage.com/gxmultibandreverb/) | Effect | Pi64 | WIP | Gx Multiband reverb — Ready |
| [Gxoc_2](https://patchstorage.com/gxoc_2/) | Effect | x64, PB32, Pi64 | WIP | Gx OC-2 — Ready |
| [GxPhaser](https://patchstorage.com/gxphaser/) | Effect, Utility | x64, PB32, Pi64 | WIP | Gx Phaser — Ready |
| [GxQuack](https://patchstorage.com/gxquack/) | Effect | x64, PB32, Pi64 | Ready | Quack — Ready |
| [GxRangeMaster](https://patchstorage.com/gxrangemaster/) | Effect | x64, PB32, Pi64 | WIP | Gx Rangemaster — Ready |
| [GxReverb-Stereo](https://patchstorage.com/gxreverb-stereo/) | Effect | x64, PB32, Pi64 | WIP | Gx Reverb stereo — Ready |
| [Gxroom_simulator](https://patchstorage.com/gxroom_simulator/) | Effect | x64, PB32, Pi64 | WIP | Gx Room simulator — Ready |
| [GxSaturator](https://patchstorage.com/gxsaturator/) | Effect | x64, PB32, Pi64 | Ready | Saturator — Ready |
| [GxScreamingBird](https://patchstorage.com/gxscreamingbird/) | Effect | x64, PB32, Pi64 | WIP | Gx Screaming Bird — Ready |
| [GxSD1](https://patchstorage.com/gxsd1/) | Effect | x64, PB32, Pi64 | Ready | SD-1 — Ready |
| [Gxshimmizita](https://patchstorage.com/gxshimmizita/) | Effect | x64, PB32, Pi64 | WIP | Gx Shimmizita — Ready |
| [GxSlowGear](https://patchstorage.com/gxslowgear/) | Utility | x64, PB32, Pi64 | WIP | Slow gear — Ready |
| [GxSustainer](https://patchstorage.com/gxsustainer/) | Effect | x64, PB32, Pi64 | WIP | Gx Sustainer — Ready |
| [GxSVT](https://patchstorage.com/gxsvt/) | Effect | x64, PB32, Pi64 | Ready | Bass amp (SVT) — Ready |
| [Gxswitched_tremolo](https://patchstorage.com/gxswitched_tremolo/) | Utility | x64, PB32, Pi64 | WIP | Gx Switched tremolo — Ready |
| [GxSwitchlessWah](https://patchstorage.com/gxswitchlesswah/) | Effect | x64, PB32, Pi64 | WIP | Gx Switchless wah — Ready |
| [GxTiltTone](https://patchstorage.com/gxtilttone/) | Effect | x64, PB32, Pi64 | WIP | Gx Tilt tone — Ready |
| [GxTremolo](https://patchstorage.com/gxtremolo/) | Utility | Pi64 | WIP | Gx Tremolo — Ready |
| [GxTubeDelay](https://patchstorage.com/gxtubedelay/) | Effect | Pi64 | WIP | Gx Tube delay — Ready |
| [GxTubeScreamer](https://patchstorage.com/gxtubescreamer/) | Effect | x64, PB32, Pi64 | Ready | Gx Tube Screamer — Ready |
| [GxTubeTremelo](https://patchstorage.com/gxtubetremelo/) | Utility | Pi64 | WIP | Gx Tube tremolo — Ready |
| [GxTubeVibrato](https://patchstorage.com/gxtubevibrato/) | Utility | Pi64 | WIP | Gx Tube vibrato — Ready |
| [GxUltraCab](https://patchstorage.com/gxultracab/) | Effect | x64, PB32, Pi64 | Ready | Ultra Cab — Ready |
| [GxVBassPreAmp](https://patchstorage.com/gxvbasspreamp/) | Effect, Utility | x64, PB32, Pi64 | Ready | VBass Pre Amp — Ready |
| [gxwah.lv2 Bundle](https://patchstorage.com/gxautowah/) | Effect | x64, PB32, Pi64 | WIP | Gx Auto wah — Ready; Gx Wah — Ready |
| [GxWahwah](https://patchstorage.com/gxwahwah/) | Utility | x64, PB32, Pi64 | Ready | Gx Wahwah — Ready |
| [GxZita_rev1-Stereo](https://patchstorage.com/gxzita_rev1-stereo/) | Effect | Pi64 | WIP | Gx Zita reverb stereo — Ready |
| [Instrument Tuner](https://patchstorage.com/instrument-tuner/) | Utility | x64, PB32, Pi64 | WIP | Tuner — Ready |
| [MVerb](https://patchstorage.com/mverb/) | Effect | x64, PB32, Pi64 | WIP | MVerb — Ready |
| [Neural Amp Modeler](https://patchstorage.com/neural-amp-modeler/) | Effect | PB32, Pi64 | Ready | NAM — Ready |
| [setBfree Whirl Speaker (Old version)](https://patchstorage.com/setbfree-whirl-speaker-old-version/) | Effect | PB32 | Ready | Rotary speaker — Ready |
| [TAP Pitch Shifter](https://patchstorage.com/tap-pitch-shifter/) | Effect | x64, PB32, Pi64 | WIP | Pitch Shifter — Ready |
| [TAP Reflector](https://patchstorage.com/tap-reflector/) | Effect | x64, PB32, Pi64 | WIP | Reverse (Reflector) — Ready |
| [TAP Sigmoid Booster](https://patchstorage.com/tap-sigmoid-booster/) | Effect | x64, PB32, Pi64 | WIP | Sigmoid Booster — Ready |
| [TAP Tubewarmth](https://patchstorage.com/tap-tubewarmth/) | Effect | x64, PB32, Pi64 | WIP | Tubewarmth — Ready |
| [the infamous power cut](https://patchstorage.com/the-infamous-power-cut/) | Utility | x64, PB32, Pi64 | WIP | Power cut — Ready |

## Partial bundle (2)

| Patchstorage plugin / bundle | Type | Advertised builds | Publication | Peepo module / detail |
| --- | --- | --- | --- | --- |
| [fomp.lv2 Bundle](https://patchstorage.com/fomp-lv2-bundle/) | Effect, Utility | x64, PB32, Pi64 | WIP | Moog Low-Pass Filter 1 — Ready; 16 identities not installed (see bundle detail below) |
| [stuck.lv2 Bundle](https://patchstorage.com/stuck-lv2-bundle/) | Effect | x64, PB32, Pi64 | WIP | Stuck (freeze) — Ready; 1 identity not installed (see bundle detail below) |

## Installed, restricted (3)

| Patchstorage plugin / bundle | Type | Advertised builds | Publication | Peepo module / detail |
| --- | --- | --- | --- | --- |
| [CV meter](https://patchstorage.com/cv-meter/) | Utility | x64, PB32, Pi64 | WIP | CV meter — Unavailable |
| [x42 MIDI Filter Collection](https://patchstorage.com/midifilter-lv2-bundle/) | Utility | Pi64 | Ready | 28 unavailable (see identity details below) |
| [zeroconvo.lv2 Bundle](https://patchstorage.com/zeroconvo-lv2-bundle/) | Effect | x64, PB32, Pi64 | WIP | IR Convolver Mono — Needs adaptation; IR Convolver Stereo — Needs adaptation |

## No exact plugin installed (293)

| Patchstorage plugin / bundle | Type | Advertised builds | Publication | Peepo module / detail |
| --- | --- | --- | --- | --- |
| [16 Pads](https://patchstorage.com/16-pads/) | Synthesizer | Pi64 | Ready | Not installed |
| [2Voices](https://patchstorage.com/2voices/) | Effect | x64, Pi64 | WIP | Not installed |
| [3 Band EQ](https://patchstorage.com/3-band-eq/) | Effect, Utility | x64, PB32, Pi64 | Ready | Not installed |
| [3 Band Splitter](https://patchstorage.com/3-band-splitter/) | Effect, Utility | x64, PB32, Pi64 | Ready | Not installed |
| [abGate](https://patchstorage.com/abgate/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Acceleration2](https://patchstorage.com/acceleration2/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [ADClip7](https://patchstorage.com/adclip7/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Aether](https://patchstorage.com/aether/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [AIDA-X](https://patchstorage.com/aida-x/) | Effect | PB32, Pi64 | Ready | Not installed |
| [ALO Plugin](https://patchstorage.com/alo-plugin/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [Amp Profiler](https://patchstorage.com/amp-profiler/) | Effect | PB32, Pi64 | Ready | Not installed |
| [Amplitude Imposer](https://patchstorage.com/amplitude-imposer/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [amsynth](https://patchstorage.com/amsynth/) | Synthesizer | x64, PB32, Pi64 | WIP | Not installed |
| [Arpeggiator](https://patchstorage.com/arpeggiator/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [artyfx.lv2 Bundle](https://patchstorage.com/artyfx-lv2-bundle/) | Effect, Utility | x64, PB32, Pi64 | WIP | Not installed |
| [Attenuverter Booster](https://patchstorage.com/attenuverter-booster/) | Utility | x64, PB32, Pi64 | WIP | Not installed; related Peepo modules: Amp (partial functional overlap; Ready); Boost (partial functional overlap; Hidden); Product (partial functional overlap; Ready); Product (partial functional overlap; Unavailable) |
| [Audio to CV](https://patchstorage.com/audio-to-cv/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [AudioToCV Pitch](https://patchstorage.com/audiotocv-pitch/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Autotune](https://patchstorage.com/autotune-2/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [avocado](https://patchstorage.com/avocado/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [BandPassFilter](https://patchstorage.com/bandpassfilter/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [Baxandall](https://patchstorage.com/baxandall/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Bitta](https://patchstorage.com/bitta/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [Black Pearl 4A](https://patchstorage.com/black-pearl-4a/) | Synthesizer | Pi64 | Ready | Not installed |
| [Black Pearl 4B](https://patchstorage.com/black-pearl-4b/) | Synthesizer | Pi64 | Ready | Not installed |
| [Black Pearl 5](https://patchstorage.com/black-pearl-5/) | Synthesizer | Pi64 | Ready | Not installed |
| [bluesbreaker](https://patchstorage.com/bluesbreaker/) | Effect | PB32, Pi64 | Ready | Not installed |
| [Bollie Delay](https://patchstorage.com/bollie-delay/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [Bollie Delay XT](https://patchstorage.com/bollie-delay-xt/) | Effect | PB32, Pi64 | WIP | Not installed |
| [Boreas](https://patchstorage.com/boreas/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [C* AmpVTS - Tube amp + Tone stack](https://patchstorage.com/c-ampvts-tube-amp-tone-stack/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [C* AutoFilter](https://patchstorage.com/c-autofilter/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [C* CabinetIII - Idealised loudspeaker cabinet emulation](https://patchstorage.com/c-cabinetiii-idealised-loudspeaker-cabinet-emulation/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [C* CabinetIV - Idealised loudspeaker cabinet emulation](https://patchstorage.com/c-cabinetiv-idealised-loudspeaker-cabinet-emulation/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [C* CEO - Chief Executive Oscillator](https://patchstorage.com/c-ceo-chief-executive-oscillator/) | Synthesizer | x64, PB32, Pi64 | WIP | Not installed |
| [C* ChorusI - Mono chorus/flanger](https://patchstorage.com/c-chorusi-mono-chorus-flanger/) | Effect, Utility | x64, PB32, Pi64 | WIP | Not installed |
| [C* Click - Metronome](https://patchstorage.com/c-click-metronome/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [C* Compress - Mono compressor](https://patchstorage.com/c-compress-mono-compressor/) | Effect, Utility | x64, PB32, Pi64 | WIP | Not installed |
| [C* CompressX2 - Stereo compressor](https://patchstorage.com/c-compressx2-stereo-compressor/) | Effect, Utility | x64, PB32, Pi64 | WIP | Not installed |
| [C* Eq10 - 10-band equalizer](https://patchstorage.com/c-eq10-10-band-equalizer/) | Effect, Utility | x64, PB32, Pi64 | WIP | Not installed |
| [C* Eq10X2 - 10-band equalizer](https://patchstorage.com/c-eq10x2-10-band-equalizer/) | Effect, Utility | x64, PB32, Pi64 | WIP | Not installed |
| [C* Fractal - Audio stream from deterministic chaos](https://patchstorage.com/c-fractal-audio-stream-from-deterministic-chaos/) | Synthesizer | x64, PB32, Pi64 | WIP | Not installed |
| [C* Narrower - Stereo image width reduction](https://patchstorage.com/c-narrower-stereo-image-width-reduction/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [C* Noisegate - Attenuate noise resident in silence](https://patchstorage.com/c-noisegate-attenuate-noise-resident-in-silence/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [C* PhaserII - Mono phaser modulated by a Lorenz fractal](https://patchstorage.com/c-phaserii-mono-phaser-modulated-by-a-lorenz-fractal/) | Effect, Utility | x64, PB32, Pi64 | WIP | Not installed |
| [C* Plate - Versatile plate reverb](https://patchstorage.com/c-plate-versatile-plate-reverb/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [C* PlateX2 - Stereo in/out Versatile plate reverb](https://patchstorage.com/c-platex2-stereo-in-out-versatile-plate-reverb/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [C* Saturate](https://patchstorage.com/c-saturate/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [C* Scape - Stereo delay + Filters](https://patchstorage.com/c-scape-stereo-delay-filters/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [C* Sin - Sine wave generator](https://patchstorage.com/c-sin-sine-wave-generator/) | Synthesizer | x64, PB32, Pi64 | WIP | Not installed |
| [C* Spice](https://patchstorage.com/c-spice/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [C* SpiceX2](https://patchstorage.com/c-spicex2/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [C* ToneStack - Tone stack emulation](https://patchstorage.com/c-tonestack-tone-stack-emulation/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [C* White - White noise generator](https://patchstorage.com/c-white-white-noise-generator/) | Synthesizer | x64, PB32, Pi64 | WIP | Not installed |
| [C* Wider - Stereo image Synthesis](https://patchstorage.com/c-wider-stereo-image-synthesis/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [Cabinet Loader](https://patchstorage.com/cabinet-loader/) | Effect | PB32, Pi64 | Ready | Not installed |
| [calf.lv2 Bundle](https://patchstorage.com/calf-lv2-bundle/) | Effect, Utility | x64, PB32 | WIP | Not installed |
| [Capacitor](https://patchstorage.com/capacitor/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Capacitor2](https://patchstorage.com/capacitor2/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Capo Plugin](https://patchstorage.com/capo-plugin/) | Effect | x64, Pi64 | WIP | Not installed |
| [carla-audiogain.lv2 Bundle](https://patchstorage.com/carla-audiogain-lv2-bundle/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [carla-files.lv2 Bundle](https://patchstorage.com/carla-files-lv2-bundle/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Channel8](https://patchstorage.com/channel8/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [ChowCentaur](https://patchstorage.com/chowcentaur/) | Effect | PB32, Pi64 | Ready | Not installed |
| [ClipOnly](https://patchstorage.com/cliponly/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [ClipOnly2](https://patchstorage.com/cliponly2/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [CollisionDrive](https://patchstorage.com/collisiondrive/) | Effect | PB32, Pi64 | Ready | Not installed |
| [Compressor](https://patchstorage.com/compressor-2/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Compressor Advanced](https://patchstorage.com/compressor-advanced/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Console7Buss](https://patchstorage.com/console7buss/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Console7Cascade](https://patchstorage.com/console7cascade/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Console7Channel](https://patchstorage.com/console7channel/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Console7Crunch](https://patchstorage.com/console7crunch/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Control to CV](https://patchstorage.com/control-to-cv/) | Utility | x64, PB32, Pi64 | WIP | Not installed; related Peepo modules: CV Source (partial functional overlap; Ready); Control to CV (functional counterpart; Ready) |
| [Convolution Loader](https://patchstorage.com/convolution-loader/) | Effect | PB32, Pi64 | Ready | Not installed |
| [CrossOver 2](https://patchstorage.com/crossover-2/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [CrossOver 3](https://patchstorage.com/crossover-3/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [CV ABS](https://patchstorage.com/cv-abs/) | Utility | x64, PB32, Pi64 | WIP | Not installed; related Peepo modules: Absolute (functional counterpart; Ready); Signal Absolute Value (functional counterpart; Ready) |
| [CV Gate](https://patchstorage.com/cv-gate/) | Utility | x64, PB32, Pi64 | WIP | Not installed; related Peepo modules: CV to trigger (partial functional overlap; Ready) |
| [CV Parameter Modulation](https://patchstorage.com/cv-parameter-modulation/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [CV Range Divider](https://patchstorage.com/cv-range-divider/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [CV Round](https://patchstorage.com/cv-round/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [DeArmondo610](https://patchstorage.com/dearmondo610/) | Effect | x64, Pi64 | Ready | Not installed |
| [DeBess](https://patchstorage.com/debess/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Dexed](https://patchstorage.com/dexed/) | Synthesizer | x64, PB32, Pi64 | WIP | Not installed |
| [DIE Fluid Synth](https://patchstorage.com/die-fluid-synth/) | Synthesizer | x64, PB32, Pi64 | Ready | Not installed |
| [Dimension IV](https://patchstorage.com/dimension-iv/) | Effect | x64, Pi64 | Ready | Not installed |
| [Dirty Talk](https://patchstorage.com/dirty-talk/) | Effect | x64, Pi64 | Ready | Not installed |
| [Drop Plugin](https://patchstorage.com/drop-plugin/) | Effect | x64, Pi64 | WIP | Not installed |
| [DS1 Plugin](https://patchstorage.com/ds1-plugin/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [Dyno Plugin](https://patchstorage.com/dyno-plugin/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Echomatica](https://patchstorage.com/echomatica/) | Effect | x64, Pi64 | Ready | Not installed |
| [EdIsDim](https://patchstorage.com/edisdim/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [EveryTrim](https://patchstorage.com/everytrim/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [FatFrog](https://patchstorage.com/fatfrog/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [floaty](https://patchstorage.com/floaty/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [Fluid Bass](https://patchstorage.com/fluid-bass/) | Synthesizer | Pi64 | Ready | Not installed |
| [Fluid Brass](https://patchstorage.com/fluid-brass/) | Synthesizer | Pi64 | Ready | Not installed |
| [Fluid Chromatic Percussion](https://patchstorage.com/fluid-chromatic-percussion/) | Synthesizer | Pi64 | Ready | Not installed |
| [Fluid Drums](https://patchstorage.com/fluid-drums/) | Synthesizer | Pi64 | Ready | Not installed |
| [Fluid Ensemble](https://patchstorage.com/fluid-ensemble/) | Synthesizer | Pi64 | Ready | Not installed |
| [Fluid Ethnic](https://patchstorage.com/fluid-ethnic/) | Synthesizer | Pi64 | Ready | Not installed |
| [Fluid Guitars](https://patchstorage.com/fluid-guitars/) | Synthesizer | Pi64 | Ready | Not installed |
| [Fluid Organs](https://patchstorage.com/fluid-organs/) | Synthesizer | Pi64 | Ready | Not installed |
| [Fluid Percussion](https://patchstorage.com/fluid-percussion/) | Synthesizer | Pi64 | Ready | Not installed |
| [Fluid Pianos](https://patchstorage.com/fluid-pianos/) | Synthesizer | Pi64 | Ready | Not installed |
| [Fluid Pipes](https://patchstorage.com/fluid-pipes/) | Synthesizer | Pi64 | Ready | Not installed |
| [Fluid Reeds](https://patchstorage.com/fluid-reeds/) | Synthesizer | Pi64 | Ready | Not installed |
| [Fluid SoundFX](https://patchstorage.com/fluid-soundfx/) | Synthesizer | Pi64 | Ready | Not installed |
| [Fluid Strings](https://patchstorage.com/fluid-strings/) | Synthesizer | Pi64 | Ready | Not installed |
| [Fluid SynthFX](https://patchstorage.com/fluid-synthfx/) | Synthesizer | Pi64 | Ready | Not installed |
| [Fluid SynthLeads](https://patchstorage.com/fluid-synthleads/) | Synthesizer | Pi64 | Ready | Not installed |
| [Fluid SynthPads](https://patchstorage.com/fluid-synthpads/) | Synthesizer | Pi64 | Ready | Not installed |
| [Freakclip](https://patchstorage.com/freakclip/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [Freaktail](https://patchstorage.com/freaktail/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [Gain 2x2](https://patchstorage.com/gain-2x2/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [Gain Plugin](https://patchstorage.com/gain-plugin/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [Galactic](https://patchstorage.com/galactic/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Granulator](https://patchstorage.com/granulator-2/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [Hardware Bypass](https://patchstorage.com/hardware-bypass/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Harmless](https://patchstorage.com/harmless/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [Harmonic Exciter](https://patchstorage.com/harmonic-exciter/) | Utility | PB32, Pi64 | Ready | Not installed |
| [Harmonizer](https://patchstorage.com/harmonizer/) | Effect | x64, Pi64 | WIP | Not installed |
| [Harmonizer2](https://patchstorage.com/harmonizer2/) | Effect | x64, Pi64 | WIP | Not installed |
| [HarmonizerCS](https://patchstorage.com/harmonizercs/) | Effect | x64, Pi64 | WIP | Not installed |
| [Hex Chain Amp](https://patchstorage.com/hex-chain/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [Hex Chain Cabinet](https://patchstorage.com/hex-chain-cabinet/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [Hex Chain Compressor](https://patchstorage.com/hex-chain-compressor/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Hex Chain Delay](https://patchstorage.com/hex-chain-delay/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [Hex Chain Drive](https://patchstorage.com/hex-chain-drive/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [Hex Chain Fuzz](https://patchstorage.com/hex-chain-fuzz/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [Hex Chain Input Trim](https://patchstorage.com/hex-chain-input-trim/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Hex Chain Modulation](https://patchstorage.com/hex-chain-modulation/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Hex Chain Nail](https://patchstorage.com/hex-chain-nail/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [Hex Chain Noise Gate](https://patchstorage.com/hex-chain-noise-gate/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Hex Chain Octave](https://patchstorage.com/hex-chain-octave/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Hex Chain Reverb](https://patchstorage.com/hex-chain-reverb/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [Hex Chain Scratch Pad](https://patchstorage.com/hex-chain-scratch-pad/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Hex Chain Wah](https://patchstorage.com/hex-chain-wah/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [Hex Forge](https://patchstorage.com/hex-forge/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [HighPassFilter](https://patchstorage.com/highpassfilter/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [IntamixX LR4](https://patchstorage.com/intamixx-lr4-2/) | Effect, Utility | x64 | Ready | Not installed |
| [invada.lv2 Bundle](https://patchstorage.com/invada-lv2-bundle/) | Effect, Utility | x64, PB32, Pi64 | WIP | Not installed |
| [IR loader cabsim](https://patchstorage.com/ir-loader-cabsim/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [JTBassMIDI](https://patchstorage.com/jtbassmidi/) | Utility | Pi64 | Ready | Not installed |
| [JTBassSynth](https://patchstorage.com/jtbasssynth/) | Utility | Pi64 | Ready | Not installed |
| [Kars Plugin](https://patchstorage.com/kars-plugin/) | Synthesizer | x64, PB32, Pi64 | WIP | Not installed |
| [Larynx](https://patchstorage.com/larynx/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [Level Meter](https://patchstorage.com/level-meter/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [LowPassFilter](https://patchstorage.com/lowpassfilter/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [MaBitcrush](https://patchstorage.com/mabitcrush/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [Mackity](https://patchstorage.com/mackity/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [MaFreeverb](https://patchstorage.com/mafreeverb/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [MaGigaverb](https://patchstorage.com/magigaverb/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [MaPitchshift](https://patchstorage.com/mapitchshift/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [MDA Ambience](https://patchstorage.com/mda-ambience/) | Effect | PB32 | WIP | Not installed |
| [MDA Bandisto](https://patchstorage.com/mda-bandisto/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [MDA BeatBox](https://patchstorage.com/mda-beatbox/) | Synthesizer | PB32 | WIP | Not installed |
| [MDA Combo](https://patchstorage.com/mda-combo/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [MDA De-ess](https://patchstorage.com/mda-de-ess/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [MDA Degrade](https://patchstorage.com/mda-degrade/) | Effect | PB32 | WIP | Not installed |
| [MDA Delay](https://patchstorage.com/mda-delay/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [MDA Detune](https://patchstorage.com/mda-detune/) | Effect | PB32 | WIP | Not installed |
| [MDA Dither](https://patchstorage.com/mda-dither/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [MDA DubDelay](https://patchstorage.com/mda-dubdelay/) | Effect | PB32 | WIP | Not installed |
| [MDA DX10](https://patchstorage.com/mda-dx10/) | Synthesizer | PB32 | WIP | Not installed |
| [MDA Dynamics](https://patchstorage.com/mda-dynamics/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [MDA ePiano](https://patchstorage.com/mda-epiano/) | Synthesizer | PB32 | WIP | Not installed |
| [MDA Image](https://patchstorage.com/mda-image/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [MDA JX10](https://patchstorage.com/mda-jx10/) | Synthesizer | PB32 | WIP | Not installed |
| [MDA Leslie](https://patchstorage.com/mda-leslie/) | Effect | PB32 | WIP | Not installed |
| [MDA Limiter](https://patchstorage.com/mda-limiter/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [MDA Loudness](https://patchstorage.com/mda-loudness/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [MDA MultiBand](https://patchstorage.com/mda-multiband/) | Effect, Utility | x64, PB32, Pi64 | WIP | Not installed |
| [MDA Overdrive](https://patchstorage.com/mda-overdrive/) | Effect | PB32 | WIP | Not installed |
| [MDA Piano](https://patchstorage.com/mda-piano/) | Synthesizer | PB32 | WIP | Not installed |
| [MDA RePsycho!](https://patchstorage.com/mda-repsycho/) | Effect | PB32 | WIP | Not installed |
| [MDA RezFilter](https://patchstorage.com/mda-rezfilter/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [MDA RingMod](https://patchstorage.com/mda-ringmod/) | Utility | PB32 | WIP | Not installed |
| [MDA RoundPan](https://patchstorage.com/mda-roundpan/) | Effect | PB32 | WIP | Not installed |
| [MDA Shepard](https://patchstorage.com/mda-shepard/) | Synthesizer | PB32 | WIP | Not installed |
| [MDA Splitter](https://patchstorage.com/mda-splitter/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [MDA Stereo](https://patchstorage.com/mda-stereo/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [MDA SubSynth](https://patchstorage.com/mda-subsynth/) | Effect | PB32 | WIP | Not installed |
| [MDA TalkBox](https://patchstorage.com/mda-talkbox/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [MDA TestTone](https://patchstorage.com/mda-testtone/) | Synthesizer | x64, PB32, Pi64 | WIP | Not installed |
| [MDA ThruZero](https://patchstorage.com/mda-thruzero/) | Effect, Utility | PB32 | WIP | Not installed |
| [MDA Tracker](https://patchstorage.com/mda-tracker/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [MDA Transient](https://patchstorage.com/mda-transient/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [MDA VocInput](https://patchstorage.com/mda-vocinput/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [MDA Vocoder](https://patchstorage.com/mda-vocoder/) | Effect | PB32 | WIP | Not installed |
| [MetalTone](https://patchstorage.com/metaltone/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [MIDI Clock Generator](https://patchstorage.com/midi-clock-generator/) | Utility | x64, PB32, Pi64 | WIP | Not installed; includes selected reference identities without an installed implementation |
| [MIDI display](https://patchstorage.com/midi-display/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [MIDI Generator](https://patchstorage.com/midi-generator/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [MIDI Step Sequencer16x8](https://patchstorage.com/midi-step-sequencer16x8/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [MIDI Step Sequencer32x8](https://patchstorage.com/midi-step-sequencer32x8/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [MIDI Step Sequencer8x16](https://patchstorage.com/midi-step-sequencer8x16/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [MIDI Step Sequencer8x4](https://patchstorage.com/midi-step-sequencer8x4/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [MIDI Step Sequencer8x8](https://patchstorage.com/midi-step-sequencer8x8/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [MIDI Timecode (MTC) Generator](https://patchstorage.com/midi-timecode-mtc-generator/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [MidSide](https://patchstorage.com/midside/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Mixer](https://patchstorage.com/mixer/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Mixer Stereo](https://patchstorage.com/mixer-stereo/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [mod-ams.lv2 Bundle](https://patchstorage.com/mod-ams-lv2-bundle/) | Utility | x64, PB32, Pi64 | Ready | Not installed; related Peepo modules: Envelope (same lineage variant; Ready); LFO - Freq (same lineage variant; Ready); LFO - Tempo (related variant; Ready); VCA Exp - Audio (same lineage variant; Ready); VCA Exp - CV (same lineage variant; Ready); VCA Lin - Audio (same lineage variant; Ready); VCA Lin - CV (same lineage variant; Ready); VCF (same lineage variant; Ready); vco2_audio (same lineage variant; Unavailable); vco2_cv (same lineage variant; Unavailable) |
| [Modulay](https://patchstorage.com/modulay/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [Mojo Plugin](https://patchstorage.com/mojo-plugin/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Molot Lite Mono](https://patchstorage.com/molot-lite-mono/) | Effect, Utility | x64, PB32, Pi64 | WIP | Not installed |
| [mud Plugin](https://patchstorage.com/mud-plugin/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [Multi Button to CV](https://patchstorage.com/multi-button-to-cv/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [Nekobi](https://patchstorage.com/nekobi/) | Synthesizer | x64, PB32, Pi64 | WIP | Not installed |
| [Neural Record](https://patchstorage.com/neural-record/) | Utility | PB32, Pi64 | Ready | Not installed |
| [Nikola](https://patchstorage.com/nikola/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [No Delay Line](https://patchstorage.com/no-delay-line/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [Noise Gate](https://patchstorage.com/noise-gate/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Noise Gate Advanced](https://patchstorage.com/noise-gate-advanced/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Open Big Muff](https://patchstorage.com/open-big-muff/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [paranoia](https://patchstorage.com/paranoia/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [Parkinsound Step Gate](https://patchstorage.com/parkinsound-step-gate/) | Effect | x64, Pi64 | Ready | Not installed |
| [Parkinsound Step Gate 4](https://patchstorage.com/parkinsound-step-gate-4/) | Effect | x64, Pi64 | Ready | Not installed |
| [Ping Pong Pan](https://patchstorage.com/ping-pong-pan/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [Pitchotto](https://patchstorage.com/pitchotto/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [PocketVerbs](https://patchstorage.com/pocketverbs/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [portal.lv2 Bundle](https://patchstorage.com/portal-lv2-bundle/) | Utility | PB32, Pi64 | WIP | Not installed |
| [PowerAmpImpulses](https://patchstorage.com/powerampimpulses/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [PowerAmpTubes](https://patchstorage.com/poweramptubes/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [PreAmpImpulses](https://patchstorage.com/preampimpulses/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [PreAmpTubes](https://patchstorage.com/preamptubes/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [Prefreak](https://patchstorage.com/prefreak/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [Pressure5](https://patchstorage.com/pressure5/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [Random Generator](https://patchstorage.com/random-generator/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [Red Zeppelin 4](https://patchstorage.com/red-zeppelin-4/) | Synthesizer | Pi64 | Ready | Not installed |
| [Red Zeppelin 5](https://patchstorage.com/red-zeppelin-5/) | Synthesizer | Pi64 | Ready | Not installed |
| [rkr.lv2 Bundle](https://patchstorage.com/rkr-lv2-bundle/) | Effect, Utility | x64, PB32, Pi64 | WIP | Not installed |
| [rubberband.lv2 Bundle](https://patchstorage.com/rubberband-lv2-bundle/) | Effect | Pi64 | Ready | Not installed |
| [Rumor](https://patchstorage.com/rumor/) | Effect | PB32, Pi64 | Ready | Not installed |
| [Rézonateur](https://patchstorage.com/rezonateur/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [samplv1](https://patchstorage.com/samplv1/) | Synthesizer | x64, PB32, Pi64 | WIP | Not installed |
| [sc_record.lv2 Bundle](https://patchstorage.com/sc_record-lv2-bundle/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [Schrammel OJD](https://patchstorage.com/schrammel-ojd/) | Effect | PB32, Pi64 | Ready | Not installed |
| [setBfree DSP Tonewheel Organ](https://patchstorage.com/setbfree-dsp-tonewheel-organ/) | Synthesizer | PB32 | Ready | Not installed |
| [setBfree MIDI controller](https://patchstorage.com/setbfree-midi-controller/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [setBfree Organ Overdrive](https://patchstorage.com/setbfree-organ-overdrive/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [setBfree Organ Reverb](https://patchstorage.com/setbfree-organ-reverb/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [Shiroverb](https://patchstorage.com/shiroverb/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [Sidepass](https://patchstorage.com/sidepass/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [SimSam](https://patchstorage.com/simsam/) | Synthesizer | x64, PB32, Pi64 | WIP | Not installed |
| [SooperLooper](https://patchstorage.com/sooperlooper/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [Soul Force](https://patchstorage.com/soul-force/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [Spectrum Analyzer](https://patchstorage.com/spectrum-analyzer/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [Spiral](https://patchstorage.com/spiral-2/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [StarChild](https://patchstorage.com/starchild/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [Stereo Balance Control](https://patchstorage.com/stereo-balance-control/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [Stereo X-Fade](https://patchstorage.com/stereo-x-fade/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [Super Capo](https://patchstorage.com/super-capo/) | Effect | x64, Pi64 | WIP | Not installed |
| [Super Whammy](https://patchstorage.com/super-whammy/) | Effect | x64, Pi64 | WIP | Not installed |
| [Switchbox 1-2](https://patchstorage.com/switchbox-1-2/) | Utility | PB32 | Ready | Not installed |
| [SwitchTrigger4](https://patchstorage.com/switchtrigger4/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [TAP AutoPanner](https://patchstorage.com/tap-autopanner/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [TAP Chorus/Flanger](https://patchstorage.com/tap-chorus-flanger/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [TAP DeEsser](https://patchstorage.com/tap-deesser/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [TAP Equalizer](https://patchstorage.com/tap-equalizer/) | Effect, Utility | x64, PB32, Pi64 | WIP | Not installed |
| [TAP Equalizer/BW](https://patchstorage.com/tap-equalizer-bw/) | Effect, Utility | x64, PB32, Pi64 | WIP | Not installed |
| [TAP Fractal Doubler](https://patchstorage.com/tap-fractal-doubler/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [TAP Mono Dynamics](https://patchstorage.com/tap-mono-dynamics/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [TAP Pink/Fractal Noise](https://patchstorage.com/tap-pink-fractal-noise/) | Synthesizer | x64, PB32, Pi64 | WIP | Not installed |
| [TAP Reverberator](https://patchstorage.com/tap-reverberator/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [TAP Rotary Speaker](https://patchstorage.com/tap-rotary-speaker/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [TAP Scaling Limiter](https://patchstorage.com/tap-scaling-limiter/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [TAP Stereo Dynamics](https://patchstorage.com/tap-stereo-dynamics/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [TAP Stereo Echo](https://patchstorage.com/tap-stereo-echo/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [TAP Tremolo](https://patchstorage.com/tap-tremolo/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [TAP Vibrato](https://patchstorage.com/tap-vibrato/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [the infamous ewham](https://patchstorage.com/the-infamous-ewham/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [the infamous Hip2B](https://patchstorage.com/the-infamous-hip2b/) | Effect | x64, PB32, Pi64 | WIP | Not installed |
| [the infamous mindi](https://patchstorage.com/the-infamous-mindi/) | Synthesizer | x64, PB32, Pi64 | WIP | Not installed |
| [tinygain.lv2 Bundle](https://patchstorage.com/tinygain-lv2-bundle/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [ToggleSwitch](https://patchstorage.com/toggleswitch/) | Utility | x64, PB32, Pi64 | WIP | Not installed |
| [Triceratops](https://patchstorage.com/triceratops/) | Synthesizer | Pi64 | WIP | Not installed |
| [Vibrato](https://patchstorage.com/vibrato-2/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [ZamAutoSat](https://patchstorage.com/zamautosat/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [ZaMaximX2](https://patchstorage.com/zamaximx2/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [ZamComp](https://patchstorage.com/zamcomp/) | Effect, Utility | x64, PB32, Pi64 | Ready | Not installed |
| [ZamCompX2](https://patchstorage.com/zamcompx2/) | Effect, Utility | x64, PB32, Pi64 | Ready | Not installed |
| [ZamDelay](https://patchstorage.com/zamdelay/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [ZamEQ2](https://patchstorage.com/zameq2/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [ZamGate](https://patchstorage.com/zamgate/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [ZamGateX2](https://patchstorage.com/zamgatex2/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [ZamGEQ31](https://patchstorage.com/zamgeq31/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [ZamHeadX2](https://patchstorage.com/zamheadx2/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [ZamTube](https://patchstorage.com/zamtube/) | Utility | x64, PB32, Pi64 | Ready | Not installed |
| [ZaMultiComp](https://patchstorage.com/zamulticomp/) | Effect | x64, PB32, Pi64 | Ready | Not installed |
| [ZaMultiCompX2](https://patchstorage.com/zamulticompx2/) | Effect | x64, PB32, Pi64 | Ready | Not installed |

## Bundle and restriction details

Every identity in a multi-plugin entry is listed here, as well as single-plugin entries installed with restrictions. **Extra** marks an identity found in the inspected archive but absent from the publication’s advertised URI list. Extra identities are confirmed only for the inspected target and date; see the evidence section.

### artyfx.lv2 Bundle

[Publication](https://patchstorage.com/artyfx-lv2-bundle/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| Driva | Not installed | Extra | `http://www.openavproductions.com/artyfx#driva` |
| Filta | Not installed |  | `http://www.openavproductions.com/artyfx#filta` |
| Kuiza | Not installed |  | `http://www.openavproductions.com/artyfx#kuiza` |
| Roomy | Not installed |  | `http://www.openavproductions.com/artyfx#roomy` |

### calf.lv2 Bundle

[Publication](https://patchstorage.com/calf-lv2-bundle/); archive inventory: patchbox-os-arm32, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| Calf Analyzer | Not installed | Extra | `http://calf.sourceforge.net/plugins/Analyzer` |
| Calf Bass Enhancer | Not installed | Extra | `http://calf.sourceforge.net/plugins/BassEnhancer` |
| Calf Compensation Delay Line | Not installed | Extra | `http://calf.sourceforge.net/plugins/CompensationDelay` |
| Calf Compressor | Not installed | Extra | `http://calf.sourceforge.net/plugins/Compressor` |
| Calf Crusher | Not installed | Extra | `http://calf.sourceforge.net/plugins/Crusher` |
| Calf Deesser | Not installed | Extra | `http://calf.sourceforge.net/plugins/Deesser` |
| Calf Emphasis | Not installed | Extra | `http://calf.sourceforge.net/plugins/Emphasis` |
| Calf Envelope Filter | Not installed |  | `http://calf.sourceforge.net/plugins/EnvelopeFilter` |
| Calf Equalizer 12 Band | Not installed | Extra | `http://calf.sourceforge.net/plugins/Equalizer12Band` |
| Calf Equalizer 30 Band | Not installed | Extra | `http://calf.sourceforge.net/plugins/Equalizer30Band` |
| Calf Equalizer 5 Band | Not installed | Extra | `http://calf.sourceforge.net/plugins/Equalizer5Band` |
| Calf Equalizer 8 Band | Not installed | Extra | `http://calf.sourceforge.net/plugins/Equalizer8Band` |
| Calf Exciter | Not installed | Extra | `http://calf.sourceforge.net/plugins/Exciter` |
| Calf Filter | Not installed | Extra | `http://calf.sourceforge.net/plugins/Filter` |
| Calf Filterclavier | Not installed | Extra | `http://calf.sourceforge.net/plugins/Filterclavier` |
| Calf Flanger | Not installed |  | `http://calf.sourceforge.net/plugins/Flanger` |
| Calf Gate | Not installed | Extra | `http://calf.sourceforge.net/plugins/Gate` |
| Calf Haas Stereo Enhancer | Not installed | Extra | `http://calf.sourceforge.net/plugins/HaasEnhancer` |
| Calf Limiter | Not installed | Extra | `http://calf.sourceforge.net/plugins/Limiter` |
| Calf Mono Compressor | Not installed | Extra | `http://calf.sourceforge.net/plugins/MonoCompressor` |
| Calf Mono Input | Not installed | Extra | `http://calf.sourceforge.net/plugins/MonoInput` |
| Calf Monosynth | Not installed | Extra | `http://calf.sourceforge.net/plugins/Monosynth` |
| Calf Multi Chorus | Not installed | Extra | `http://calf.sourceforge.net/plugins/MultiChorus` |
| Calf Multiband Compressor | Not installed | Extra | `http://calf.sourceforge.net/plugins/MultibandCompressor` |
| Calf Multiband Gate | Not installed | Extra | `http://calf.sourceforge.net/plugins/MultibandGate` |
| Calf Multiband Limiter | Not installed | Extra | `http://calf.sourceforge.net/plugins/MultibandLimiter` |
| Calf Organ | Not installed | Extra | `http://calf.sourceforge.net/plugins/Organ` |
| Calf Phaser | Not installed |  | `http://calf.sourceforge.net/plugins/Phaser` |
| Calf Pulsator | Not installed | Extra | `http://calf.sourceforge.net/plugins/Pulsator` |
| Calf Reverb | Not installed | Extra | `http://calf.sourceforge.net/plugins/Reverb` |
| Calf Reverse Delay | Not installed | Extra | `http://calf.sourceforge.net/plugins/ReverseDelay` |
| Calf Ring Modulator | Not installed | Extra | `http://calf.sourceforge.net/plugins/RingModulator` |
| Calf Rotary Speaker | Not installed | Extra | `http://calf.sourceforge.net/plugins/RotarySpeaker` |
| Calf Saturator | Not installed |  | `http://calf.sourceforge.net/plugins/Saturator` |
| Calf Sidechain Compressor | Not installed | Extra | `http://calf.sourceforge.net/plugins/SidechainCompressor` |
| Calf Sidechain Gate | Not installed | Extra | `http://calf.sourceforge.net/plugins/SidechainGate` |
| Calf Sidechain Limiter | Not installed | Extra | `http://calf.sourceforge.net/plugins/SidechainLimiter` |
| Calf Stereo Tools | Not installed | Extra | `http://calf.sourceforge.net/plugins/StereoTools` |
| Calf Tape Simulator | Not installed | Extra | `http://calf.sourceforge.net/plugins/TapeSimulator` |
| Calf Transient Designer | Not installed | Extra | `http://calf.sourceforge.net/plugins/TransientDesigner` |
| Calf Vintage Delay | Not installed | Extra | `http://calf.sourceforge.net/plugins/VintageDelay` |
| Calf Vocoder | Not installed | Extra | `http://calf.sourceforge.net/plugins/Vocoder` |
| Calf X-Over 2 Band | Not installed | Extra | `http://calf.sourceforge.net/plugins/XOver2Band` |
| Calf X-Over 3 Band | Not installed | Extra | `http://calf.sourceforge.net/plugins/XOver3Band` |
| Calf X-Over 4 Band | Not installed | Extra | `http://calf.sourceforge.net/plugins/XOver4Band` |

### carla-audiogain.lv2 Bundle

[Publication](https://patchstorage.com/carla-audiogain-lv2-bundle/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| Audio Gain (Mono) | Not installed |  | `http://kxstudio.sf.net/carla/plugins/audiogain` |
| Audio Gain (Stereo) | Not installed |  | `http://kxstudio.sf.net/carla/plugins/audiogain_s` |

### carla-files.lv2 Bundle

[Publication](https://patchstorage.com/carla-files-lv2-bundle/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| Audio File | Not installed |  | `http://kxstudio.sf.net/carla/plugins/audiofile` |
| MIDI File | Not installed |  | `http://kxstudio.sf.net/carla/plugins/midifile` |

### Convolution Loader

[Publication](https://patchstorage.com/convolution-loader/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| MOD Cabinet Loader | Not installed | Extra | `https://mod.audio/plugins/CabinetLoader` |
| MOD Convolution Loader | Not installed |  | `https://mod.audio/plugins/ConvolutionLoader` |

### CV meter

[Publication](https://patchstorage.com/cv-meter/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| CV meter | Unavailable | This module has no audio or control connectors to patch. | `http://moddevices.com/plugins/mod-devel/mod-cv-meter` |

### fil4.lv2 Bundle

[Publication](https://patchstorage.com/fil4-lv2-bundle/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| Parametric EQ | Ready |  | `http://gareus.org/oss/lv2/fil4#mono` |
| Parametric EQ stereo | Ready |  | `http://gareus.org/oss/lv2/fil4#stereo` |

### fomp.lv2 Bundle

[Publication](https://patchstorage.com/fomp-lv2-bundle/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| 4-Band Parametric Filter | Not installed | Extra | `http://drobilla.net/plugins/fomp/parametric1` |
| Auto-wah | Not installed |  | `http://drobilla.net/plugins/fomp/autowah` |
| CS Chorus 1 | Not installed |  | `http://drobilla.net/plugins/fomp/cs_chorus1` |
| CS Chorus 2 | Not installed |  | `http://drobilla.net/plugins/fomp/cs_chorus2` |
| CS Phaser 1 | Not installed | Extra | `http://drobilla.net/plugins/fomp/cs_phaser1` |
| CS Phaser 1 with LFO | Not installed |  | `http://drobilla.net/plugins/fomp/cs_phaser1_lfo` |
| Moog High-Pass Filter 1 | Not installed | Extra | `http://drobilla.net/plugins/fomp/mvchpf1` |
| Moog Low-Pass Filter 1 | Ready | Extra | `http://drobilla.net/plugins/fomp/mvclpf1` |
| Moog Low-Pass Filter 2 | Not installed | Extra | `http://drobilla.net/plugins/fomp/mvclpf2` |
| Moog Low-Pass Filter 3 | Not installed | Extra | `http://drobilla.net/plugins/fomp/mvclpf3` |
| Moog Low-Pass Filter 4 | Not installed | Extra | `http://drobilla.net/plugins/fomp/mvclpf4` |
| Pulse VCO | Not installed | Extra | `http://drobilla.net/plugins/fomp/pulse_vco` |
| Rec VCO | Not installed | Extra | `http://drobilla.net/plugins/fomp/rec_vco` |
| reverb | Not installed |  | `http://drobilla.net/plugins/fomp/reverb` |
| reverb-amb | Not installed | Extra | `http://drobilla.net/plugins/fomp/reverb_amb` |
| Saw VCO | Not installed | Extra | `http://drobilla.net/plugins/fomp/saw_vco` |
| Triple chorus | Not installed |  | `http://drobilla.net/plugins/fomp/triple_chorus` |

### gx_redeye.lv2 Bundle

[Publication](https://patchstorage.com/gx_redeye-lv2-bundle/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| Gx Redeye Big Chump | Ready |  | `http://guitarix.sourceforge.net/plugins/gx_redeye#bigchump` |
| Gx Redeye Chump | Ready |  | `http://guitarix.sourceforge.net/plugins/gx_redeye#chump` |
| Gx Redeye Vibro Chump | Ready |  | `http://guitarix.sourceforge.net/plugins/gx_redeye#vibrochump` |

### gxwah.lv2 Bundle

[Publication](https://patchstorage.com/gxautowah/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| Gx Auto wah | Ready |  | `http://guitarix.sourceforge.net/plugins/gxautowah#autowah` |
| Gx Wah | Ready |  | `http://guitarix.sourceforge.net/plugins/gxautowah#wah` |

### invada.lv2 Bundle

[Publication](https://patchstorage.com/invada-lv2-bundle/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| Invada Compressor (mono) | Not installed |  | `http://invadarecords.com/plugins/lv2/compressor/mono` |
| Invada Compressor (stereo) | Not installed |  | `http://invadarecords.com/plugins/lv2/compressor/stereo` |
| Invada Delay Munge (mono in) | Not installed |  | `http://invadarecords.com/plugins/lv2/delay/mono` |
| Invada Delay Munge (sum L+R in) | Not installed |  | `http://invadarecords.com/plugins/lv2/delay/sum` |
| Invada Early Reflection Reverb (mono in) | Not installed | Extra | `http://invadarecords.com/plugins/lv2/erreverb/mono` |
| Invada Early Reflection Reverb (sum L+R in) | Not installed | Extra | `http://invadarecords.com/plugins/lv2/erreverb/sum` |
| Invada High Pass Filter (mono) | Not installed | Extra | `http://invadarecords.com/plugins/lv2/filter/hpf/mono` |
| Invada High Pass Filter (stereo) | Not installed | Extra | `http://invadarecords.com/plugins/lv2/filter/hpf/stereo` |
| Invada Input Module | Not installed | Extra | `http://invadarecords.com/plugins/lv2/input` |
| Invada Low Pass Filter (mono) | Not installed | Extra | `http://invadarecords.com/plugins/lv2/filter/lpf/mono` |
| Invada Low Pass Filter (stereo) | Not installed | Extra | `http://invadarecords.com/plugins/lv2/filter/lpf/stereo` |
| Invada Meter | Not installed | Extra | `http://invadarecords.com/plugins/lv2/meter` |
| Invada Stereo Phaser (mono in) | Not installed |  | `http://invadarecords.com/plugins/lv2/phaser/mono` |
| Invada Stereo Phaser (stereo in) | Not installed |  | `http://invadarecords.com/plugins/lv2/phaser/stereo` |
| Invada Stereo Phaser (sum L+R in) | Not installed |  | `http://invadarecords.com/plugins/lv2/phaser/sum` |
| Invada Test Tones | Not installed | Extra | `http://invadarecords.com/plugins/lv2/testtone` |
| Invada Tube Distortion (mono) | Not installed | Extra | `http://invadarecords.com/plugins/lv2/tube/mono` |
| Invada Tube Distortion (stereo) | Not installed | Extra | `http://invadarecords.com/plugins/lv2/tube/stereo` |

### mod-ams.lv2 Bundle

[Publication](https://patchstorage.com/mod-ams-lv2-bundle/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| AMS Envelope | Not installed |  | `http://github.com/blablack/ams-lv2/env` |
| AMS LFO2 - Freq | Not installed |  | `http://github.com/blablack/ams-lv2/lfo2_freq` |
| AMS VCA Exp | Not installed |  | `http://github.com/blablack/ams-lv2/vcaexp` |
| AMS VCA Lin | Not installed |  | `http://github.com/blablack/ams-lv2/vcalin` |
| AMS VCF | Not installed |  | `http://github.com/blablack/ams-lv2/vcf` |
| AMS VCO3 | Not installed |  | `http://github.com/blablack/ams-lv2/vco3` |

### portal.lv2 Bundle

[Publication](https://patchstorage.com/portal-lv2-bundle/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| Portal Sink | Not installed |  | `https://falktx.com/plugins/portal#sink` |
| Portal Source | Not installed |  | `https://falktx.com/plugins/portal#source` |

### rkr.lv2 Bundle

[Publication](https://patchstorage.com/rkr-lv2-bundle/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| Fuzz | Not installed |  | `http://rakarrack.sourceforge.net/effects.html#StompBox_fuzz` |
| rkr AlienWah | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#awha` |
| rkr Analog Phaser | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#aphas` |
| rkr Arpie | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Arpie` |
| rkr Cabinet | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#cabe` |
| rkr Coil Crafter | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#CoilCrafter` |
| rkr CompBand | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#CompBand` |
| rkr Compressor | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#comp` |
| rkr Derelict | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#dere` |
| rkr DistBand | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#DistBand` |
| rkr Distortion | Not installed |  | `http://rakarrack.sourceforge.net/effects.html#dist` |
| rkr Dual Flange | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Dual_Flange` |
| rkr Echo | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#eco` |
| rkr Echotron | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Echotron` |
| rkr Echoverse | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Echoverse` |
| rkr EQ | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#eql` |
| rkr Exciter | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Exciter` |
| rkr Expander | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Expander` |
| rkr Flanger/Chorus | Not installed |  | `http://rakarrack.sourceforge.net/effects.html#chor` |
| rkr Harmonizer | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#har` |
| rkr Harmonizer (no midi) | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#har_no_mid` |
| rkr Infinity | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Infinity` |
| rkr Musical Delay | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#delm` |
| rkr MuTroMojo | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#MuTroMojo` |
| rkr OpticalTrem | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Otrem` |
| rkr Pan | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#pan` |
| rkr Parametric EQ | Not installed |  | `http://rakarrack.sourceforge.net/effects.html#eqp` |
| rkr Reverb | Not installed |  | `http://rakarrack.sourceforge.net/effects.html#reve` |
| rkr Reverbtron | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Reverbtron` |
| rkr Ring | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Ring` |
| rkr Sequence | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Sequence` |
| rkr Shelf Boost | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#ShelfBoost` |
| rkr Shifter | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Shifter` |
| rkr Shuffle | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Shuffle` |
| rkr StereoHarmonizer (no midi) | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#StereoHarm_no_mid` |
| rkr StompBox | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#StompBox` |
| rkr Sustainer | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Sustainer` |
| rkr Synthfilter | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Synthfilter` |
| rkr Valve | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Valve` |
| rkr VaryBand | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#VaryBand` |
| rkr Vibe | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Vibe` |
| rkr Vocoder | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#Vocoder` |
| rkr WahWah | Not installed | Extra | `http://rakarrack.sourceforge.net/effects.html#wha` |

### rubberband.lv2 Bundle

[Publication](https://patchstorage.com/rubberband-lv2-bundle/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| Rubberband (Mono) | Not installed |  | `http://www.breakfastquay.com/rubberband#Mono` |
| Rubberband (Stereo) | Not installed |  | `http://www.breakfastquay.com/rubberband#Stereo` |

### sc_record.lv2 Bundle

[Publication](https://patchstorage.com/sc_record-lv2-bundle/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| Record-Mono | Not installed |  | `https://github.com/brummer10/screcord#mono_record` |
| Record-Quad | Not installed |  | `https://github.com/brummer10/screcord#quad_record` |
| Record-Stereo | Not installed |  | `https://github.com/brummer10/screcord#stereo_record` |

### stuck.lv2 Bundle

[Publication](https://patchstorage.com/stuck-lv2-bundle/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| Stuck (freeze) | Ready |  | `http://ssj71.github.io/infamousPlugins/plugs.html#stuck` |
| the infamous stuck stacker | Not installed |  | `http://ssj71.github.io/infamousPlugins/plugs.html#stuckstacker` |

### tinygain.lv2 Bundle

[Publication](https://patchstorage.com/tinygain-lv2-bundle/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| TinyGain Mono | Not installed |  | `http://gareus.org/oss/lv2/tinygain#mono` |
| TinyGain Stereo | Not installed |  | `http://gareus.org/oss/lv2/tinygain#stereo` |

### x42 MIDI Filter Collection

[Publication](https://patchstorage.com/midifilter-lv2-bundle/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| MIDI CC Map | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#mapcc` |
| MIDI CC to Note | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#cctonote` |
| MIDI Channel Filter | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#channelfilter` |
| MIDI Channel Map | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#channelmap` |
| MIDI Channel Unisono | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#mididup` |
| MIDI Chord | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#midichord` |
| MIDI Chromatic Transpose | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#miditranspose` |
| MIDI Delayline | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#mididelay` |
| MIDI Duplicate Blocker | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#nodup` |
| MIDI Enforce Scale | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#enforcescale` |
| MIDI Event Filter | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#eventblocker` |
| MIDI Key-Range Filter | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#keyrange` |
| MIDI Keysplit | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#keysplit` |
| MIDI Monophonic Legato | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#monolegato` |
| MIDI N-Tap Delay | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#ntapdelay` |
| MIDI Note to CC | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#notetocc` |
| MIDI Note Toggle | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#notetoggle` |
| MIDI Note Transpose | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#mapkeyscale` |
| MIDI Note/Channel Map | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#mapkeychannel` |
| MIDI Quantization | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#quantize` |
| MIDI Remove Active Sensing | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#noactivesensing` |
| MIDI Scale CC Value | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#scalecc` |
| MIDI Simple Channel Filter | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#onechannelfilter` |
| MIDI Sostenuto | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#sostenuto` |
| MIDI Strum | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#midistrum` |
| MIDI Velocity Adjust | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#velocityscale` |
| MIDI Velocity Randomization | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#randvelocity` |
| MIDI Velocity-Range Filter | Unavailable | This module has no audio or control connectors to patch. | `http://gareus.org/oss/lv2/midifilter#velocityrange` |

### zeroconvo.lv2 Bundle

[Publication](https://patchstorage.com/zeroconvo-lv2-bundle/); archive inventory: rpi-aarch64, 13 October 2026.

| Plugin / Peepo name | Peepo status | Note | LV2 identity |
| --- | --- | --- | --- |
| IR Convolver Mono | Needs adaptation | Does not load an impulse response in this engine: its worker reports an error (docs/29). | `http://gareus.org/oss/lv2/zeroconvolv#CfgMono` |
| IR Convolver Stereo | Needs adaptation | Does not load an impulse response in this engine: its worker reports an error (docs/29). | `http://gareus.org/oss/lv2/zeroconvolv#CfgStereo` |

## Related implementations already in Peepo

These relationships come from the source review of 13 October. They do not make the Patchstorage identities installed, and they are not guaranteed drop-in replacements. [Full source comparison](../LV2%20Plugin%20Comparison.md#corrected-comparison-ams-and-cv-counterparts).

| Patchstorage identity | Peepo module | Relationship | Difference / evidence |
| --- | --- | --- | --- |
| `http://github.com/blablack/ams-lv2/env` | Envelope — Ready | same lineage variant | Same AMS envelope algorithm and port layout; MOD source changes peak from 1 to 10 and sustain scaling. IDs and CV levels differ. [Poly source](https://github.com/polyeffects/avwlv2/blob/bfb2e1f1851bdc6192e0401789a5d62045156d20/src/env.cpp); [MOD source](https://github.com/mod-audio/mod-ams-lv2/blob/81c4aea173e6a9507312f1a8e173341b485d2792/src/env.cpp). |
| `http://github.com/blablack/ams-lv2/vcf` | VCF — Ready | same lineage variant | AMS multi-mode VCF lineage and same port symbols; MOD changes CV modulation scaling and filter-state handling. [Poly source](https://github.com/polyeffects/avwlv2/blob/bfb2e1f1851bdc6192e0401789a5d62045156d20/src/vcf.cpp); [MOD source](https://github.com/mod-audio/mod-ams-lv2/blob/81c4aea173e6a9507312f1a8e173341b485d2792/src/vcf.cpp). |
| `http://github.com/blablack/ams-lv2/vcaexp` | VCA Exp - Audio — Ready | same lineage variant | Same AMS exponential gain/mixing algorithm; MOD scales modulation inputs by 0.1. Audio ports correspond. [Poly source](https://github.com/polyeffects/avwlv2/blob/bfb2e1f1851bdc6192e0401789a5d62045156d20/src/vcaexp_audio.cpp); [MOD source](https://github.com/mod-audio/mod-ams-lv2/blob/81c4aea173e6a9507312f1a8e173341b485d2792/src/vcaexp.cpp). |
| `http://github.com/blablack/ams-lv2/vcaexp` | VCA Exp - CV — Ready | same lineage variant | Same AMS exponential gain/mixing algorithm; MOD scales modulation inputs by 0.1. Poly variant processes CV signals; the MOD bundle variant uses audio signal ports, so it is a related port variant. [Poly source](https://github.com/polyeffects/avwlv2/blob/bfb2e1f1851bdc6192e0401789a5d62045156d20/src/vcaexp_cv.cpp); [MOD source](https://github.com/mod-audio/mod-ams-lv2/blob/81c4aea173e6a9507312f1a8e173341b485d2792/src/vcaexp.cpp). |
| `http://github.com/blablack/ams-lv2/vcalin` | VCA Lin - Audio — Ready | same lineage variant | Same AMS linear gain/mixing algorithm; MOD scales modulation inputs by 0.2. Audio ports correspond. [Poly source](https://github.com/polyeffects/avwlv2/blob/bfb2e1f1851bdc6192e0401789a5d62045156d20/src/vcalin_audio.cpp); [MOD source](https://github.com/mod-audio/mod-ams-lv2/blob/81c4aea173e6a9507312f1a8e173341b485d2792/src/vcalin.cpp). |
| `http://github.com/blablack/ams-lv2/vcalin` | VCA Lin - CV — Ready | same lineage variant | Same AMS linear gain/mixing algorithm; MOD scales modulation inputs by 0.2. Poly variant processes CV signals; the MOD bundle variant uses audio signal ports, so it is a related port variant. [Poly source](https://github.com/polyeffects/avwlv2/blob/bfb2e1f1851bdc6192e0401789a5d62045156d20/src/vcalin_cv.cpp); [MOD source](https://github.com/mod-audio/mod-ams-lv2/blob/81c4aea173e6a9507312f1a8e173341b485d2792/src/vcalin.cpp). |
| `http://github.com/blablack/ams-lv2/lfo2_freq` | LFO - Freq — Ready | same lineage variant | Shared AMS LFO code structure and waveform set; MOD LFO2 multiplies output by 5 and changes reset threshold and state updates. [Poly source](https://github.com/polyeffects/avwlv2/blob/bfb2e1f1851bdc6192e0401789a5d62045156d20/src/lfo_freq.cpp); [MOD source](https://github.com/mod-audio/mod-ams-lv2/blob/81c4aea173e6a9507312f1a8e173341b485d2792/src/lfo2_freq.cpp). |
| `http://github.com/blablack/ams-lv2/vco3` | vco2_audio — Unavailable | same lineage variant | AMS oscillator lineage; MOD VCO3 adds a sync input, changes FM/PWM scaling and frequency limits. Both output audio. [Poly source](https://github.com/polyeffects/avwlv2/blob/bfb2e1f1851bdc6192e0401789a5d62045156d20/src/vco2_audio.cpp); [MOD source](https://github.com/mod-audio/mod-ams-lv2/blob/81c4aea173e6a9507312f1a8e173341b485d2792/src/vco3.cpp). |
| `http://github.com/blablack/ams-lv2/vco3` | vco2_cv — Unavailable | same lineage variant | AMS oscillator lineage; MOD VCO3 adds a sync input, changes FM/PWM scaling and frequency limits. Poly variant outputs CV; MOD VCO3 outputs audio. [Poly source](https://github.com/polyeffects/avwlv2/blob/bfb2e1f1851bdc6192e0401789a5d62045156d20/src/vco2_cv.cpp); [MOD source](https://github.com/mod-audio/mod-ams-lv2/blob/81c4aea173e6a9507312f1a8e173341b485d2792/src/vco3.cpp). |
| `http://github.com/blablack/ams-lv2/lfo2_freq` | LFO - Tempo — Ready | related variant | Same AMS waveform family, but Beebo/Poly LFO is controlled by tempo and multiplier; MOD LFO2 is frequency-controlled. This is not counted as a verified equivalent. [Poly source](https://github.com/polyeffects/avwlv2/blob/bfb2e1f1851bdc6192e0401789a5d62045156d20/src/lfo_tempo.cpp); [MOD source](https://github.com/mod-audio/mod-ams-lv2/blob/81c4aea173e6a9507312f1a8e173341b485d2792/src/lfo2_freq.cpp). |
| `http://moddevices.com/plugins/mod-devel/mod-cv-abs` | Signal Absolute Value — Ready | functional counterpart | Both rectify a sample to its absolute value. MOD adds enabled/bypass handling; different implementation and URI. [Poly source](https://github.com/polyeffects/avwlv2/blob/bfb2e1f1851bdc6192e0401789a5d62045156d20/src/absolute.cpp); [MOD source](https://github.com/mod-audio/mod-cv-plugins/blob/a2feb53157ca5605ae8fea8149abef9cff948cb4/source/mod-cv-abs/mod-cv-abs.c). |
| `http://moddevices.com/plugins/mod-devel/mod-cv-abs` | Absolute — Ready | functional counterpart | Same absolute-value operation when Poly input offset is zero. Poly permits an input offset; MOD adds enabled/bypass handling. Beebo calls the Poly plugin rectify_value. [Poly source](https://github.com/polyeffects/PolyLV2/blob/6f3ae80f6b4eccb8daebfc2b641fde1d48224250/basic_modular.lv2/difference.c); [MOD source](https://github.com/mod-audio/mod-cv-plugins/blob/a2feb53157ca5605ae8fea8149abef9cff948cb4/source/mod-cv-abs/mod-cv-abs.c). |
| `http://moddevices.com/plugins/mod-devel/mod-cv-control` | Control to CV — Ready | functional counterpart | Both broadcast a control value as a constant CV buffer when MOD smoothing is disabled. MOD adds optional smoothing and has different control ranges. [Poly source](https://github.com/polyeffects/avwlv2/blob/bfb2e1f1851bdc6192e0401789a5d62045156d20/src/controltocv.cpp); [MOD source](https://github.com/mod-audio/mod-cv-plugins/blob/a2feb53157ca5605ae8fea8149abef9cff948cb4/source/mod-cv-control/mod-cv-control.c). |
| `http://moddevices.com/plugins/mod-devel/mod-cv-control` | CV Source — Ready | partial functional overlap | Both generate constant CV values, but Poly AMS CV Source provides four coarse/fine outputs; MOD Control to CV provides one output and optional smoothing. [Poly source](https://github.com/polyeffects/avwlv2/blob/bfb2e1f1851bdc6192e0401789a5d62045156d20/src/cvs.cpp); [MOD source](https://github.com/mod-audio/mod-cv-plugins/blob/a2feb53157ca5605ae8fea8149abef9cff948cb4/source/mod-cv-control/mod-cv-control.c). |
| `http://moddevices.com/plugins/mod-devel/mod-cv-attenuverter` | Amp — Ready | partial functional overlap | Both can multiply a signal by gain. MOD adds offset, smoothing and bypass; audio/CV port typing and gain ranges require adaptation. [Poly source](https://github.com/polyeffects/avwlv2/blob/bfb2e1f1851bdc6192e0401789a5d62045156d20/src/amp.cpp); [MOD source](https://github.com/mod-audio/mod-cv-plugins/blob/a2feb53157ca5605ae8fea8149abef9cff948cb4/source/mod-cv-attenuverter/mod-cv-attenuverter.c). |
| `http://moddevices.com/plugins/mod-devel/mod-cv-attenuverter` | Boost — Hidden | partial functional overlap | Both have a linear gain operation, but the Poly boost is an audio effect with an on control, while MOD is a bipolar CV gain/offset processor. [Poly source](https://github.com/polyeffects/PolyLV2/blob/6f3ae80f6b4eccb8daebfc2b641fde1d48224250/basic_modular.lv2/amp.c); [MOD source](https://github.com/mod-audio/mod-cv-plugins/blob/a2feb53157ca5605ae8fea8149abef9cff948cb4/source/mod-cv-attenuverter/mod-cv-attenuverter.c). |
| `http://moddevices.com/plugins/mod-devel/mod-cv-attenuverter` | Product — Ready | partial functional overlap | Poly product can perform CV multiplication; MOD attenuation plus output offset/smoothing is not the same complete interface or algorithm. [Poly source](https://github.com/polyeffects/PolyLV2/blob/6f3ae80f6b4eccb8daebfc2b641fde1d48224250/basic_modular.lv2/difference.c); [MOD source](https://github.com/mod-audio/mod-cv-plugins/blob/a2feb53157ca5605ae8fea8149abef9cff948cb4/source/mod-cv-attenuverter/mod-cv-attenuverter.c). |
| `http://moddevices.com/plugins/mod-devel/mod-cv-attenuverter` | Product — Unavailable | partial functional overlap | Beebo maps attenuverter to BLOP product. Same gain/multiplication task; MOD also has an output offset and smoothing. No identical ID or whole-plugin equivalence is established. [Poly source](https://github.com/polyeffects/digit_ui/blob/feb4df451067850770ddd13af9f4e2828234dabc/module_info.py); [MOD source](https://github.com/mod-audio/mod-cv-plugins/blob/a2feb53157ca5605ae8fea8149abef9cff948cb4/source/mod-cv-attenuverter/mod-cv-attenuverter.c). |
| `http://moddevices.com/plugins/mod-devel/mod-cv-gate` | CV to trigger — Ready | partial functional overlap | Both use threshold logic, but Poly schmitt generates a trigger while MOD gates a separate input signal using open/close thresholds. [Poly source](https://github.com/polyeffects/PolyLV2/blob/6f3ae80f6b4eccb8daebfc2b641fde1d48224250/basic_modular.lv2/difference.c); [MOD source](https://github.com/mod-audio/mod-cv-plugins/blob/a2feb53157ca5605ae8fea8149abef9cff948cb4/source/mod-cv-gate/mod-cv-gate.c). |

## Evidence and limitations

- Current publication metadata: [Patchstorage snapshot](evidence/patchstorage-lv2-20261019.json), fetched through the public API in four pages of up to 100 entries. [API page 1](https://patchstorage.com/api/beta/patches/?platforms=8046&per_page=100&page=1).
- Installation evidence: [Pi catalogue, 19 October 2026](evidence/catalogue-pi-20261019.json), [overlay](../config/catalogue/overlay.json), [selected inventory](../config/catalogue/inventory.txt). [Complete Peepo module list](33-module-list.md) supplies descriptions and connections.
- Bundle expansion: [13 October download inspection](lv2-plugin-comparison-data.json). One archive per entry was inspected then; this report reuses it only for entries whose revision, update timestamp, advertised identities and targets remain unchanged. It does not claim an inspection of every architecture or a new download inspection today.
- The new **16 Pads** entry and updated **JTBassMIDI** entry use current advertised identities only. They have no fresh archive inspection in this comparison.
- None of the missing entries has been built or tested in Peepo for this comparison. An advertised Pi64 binary can still need host features, file handling, MIDI routing or other adaptation.
- Generated by `python tools/compare-patchstorage.py`. The historical Poly/Beebo comparison and its input data remain unchanged.
