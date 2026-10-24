# Scenes: bypass tails and atomic changes, measured (P13.1, P13.2)

24 October 2026, WSL2 Debian, JACK dummy backend, 48 kHz, 128 frames. Engine: upstream Ingen 13a045d0 + Poly's MIDI-learn fix + project patches 0001-0012, rebuilt in `/opt/peepo` for this record (it is the first WSL build with 0011 and 0012). Script: `tools/ingen-spike/27-scenes.sh` / `27-scenes.py`. Raw output: [scenes-run.txt](scenes-run.txt); data: [third-party tails](scenes-tails-thirdparty.json), [own plugins' tails](scenes-tails-own.json), [bundles, plain engine](scenes-atomic-plain.json), [bundles, `-a`](scenes-atomic-a.json).

## Method

Each check uses a fresh block wired to the graph's stereo ports, bypassed the way the adapter does it: `ingen:enabled false` on the block (the engine's bypass), or the plugin's own switch at 0 for an overlay `"bypass": "port"`.

- **Tail:** a 30 ms noise burst, bypass 100 ms later; RMS from 0.4 to 1.4 s after the burst, against the same run with the module left on. Rings on: within 3 dB. Cut: below -80 dBFS or 40 dB down.
- **Click:** a steady 220 Hz tone at 0.25, bypass mid-recording; the largest second difference of the output in 2.5 ms windows near the switch, in dB above the median window. Flagged above 20 dB.
- **Replay:** a burst, bypass at 0.1 s, switched on again at 2.6 s; the peak in the next 0.8 s, against the same moment in a run never bypassed. Flagged when more than 10 dB louder and above -70 dBFS.
- **Side:** for one audio input and two outputs, the right output against the left while bypassed with the tone.
- **Bundles:** two eg-amp blocks, A from `in_l` to `out_l`, B from `in_l` to `out_r`, 0.5 DC on `in_l`. A bundle (`ingen:BundleStart` ... `ingen:BundleEnd`, sent in one write) switches them opposite ways: both bypasses, both gains (0 / -90 dB), or A's bypass with B's gain. The frames where `out_l` and `out_r` change are compared, 40 bundles each.

Default values, except modules silent at their defaults: Aura `wet_on` 1; Drizzle `feedback` 0.6, `reverb` 0.5. Discipline is measured twice, the second time with `trails` 1.

## Bundles: one frame only with `-a` and patch 0012

| Engine | Bypass pair | Parameter pair | Bypass + parameter |
| --- | --- | --- | --- |
| Plain, before 0012 | 36 of 40 on one frame; the rest a whole cycle (128 frames) apart | 0 of 40; up to 17 frames apart | not measured |
| `-a`, before 0012 | 40 of 40 | 38 of 40; 13 frames apart twice | not measured |
| Plain, with 0012 | 35 of 40; up to 128 frames | 0 of 40; up to 21 frames | 0 of 40; up to 138 frames |
| **`-a`, with 0012** | **40 of 40** | **40 of 40** | **40 of 40** |

Without `-a` each message of a bundle is executed when it is ready, so a bundle can straddle two cycles. With `-a` the audio thread waits for the whole bundle and runs it in one cycle (`Mark::get_execution`, `PreProcessor::process`), but each event still kept the time it arrived, and a control change is applied at that frame within the cycle. Patch 0012 gives every event of an atomic bundle the cycle's first frame.

## Tails under today's bypass

**The engine's bypass (third-party modules), 24 installed in WSL:**

| Module | Tail | Click (dB) | Replay: after / never bypassed (dBFS) |
| --- | --- | --- | --- |
| Gx Delay stereo | cut | 51.3 | silent / silent |
| Gx Digital delay | cut | 55.3 | **-12.2** / -48.6 |
| Gx Digital delay stereo | cut | 55.3 | **-12.2** / -48.6 |
| Gx Duck delay | cut | 61.4 | **-7.1** / silent |
| Gx Duck delay stereo | cut | 50.5 | **-14.0** / silent |
| Gx Echo Cat | none at defaults | 55.2 | -82.4 / silent |
| Gx Echo stereo | none at defaults (-71.7) | 54.0 | **-16.6** / silent |
| Gx Multiband delay | cut | 57.8 | **-10.4** / -35.2 |
| Gx Multiband echo | cut | 69.2 | **-22.2** / -54.7 |
| Gx Tube delay | cut | 73.0 | **-2.2** / silent |
| MDA DubDelay | none at defaults | 42.2 | silent / -26.7 |
| Mist Beat Repeat | none at defaults | 56.9 | silent / silent |
| Mist Looper | cut | 53.1 | **-15.5** / -54.0 |
| Stereo delay | cut | 54.3 | **-11.4** / -69.1 |
| Tempo delay | cut | 50.7 | **-13.2** / -120.6 |
| Dragonfly Hall | cut | 42.3 | **-26.0** / -127.9 |
| Dragonfly Plate | none at defaults (-98.5) | 51.0 | **-27.9** / silent |
| Dragonfly Room | none at defaults (-80.5) | 47.9 | **-37.6** / silent |
| Gx Multiband reverb | none at defaults (-72.7) | 64.8 | **-28.0** / -159.7 |
| Gx Shimmizita | cut | 73.6 | **-11.9** / -80.3 |
| Mist Oliverb | cut | 68.4 | **-7.9** / -38.2 |
| Mist | cut | 60.4 | **-25.4** / silent |
| Mist Spectral | cut | 72.0 | -2.4 / 3.3 (sustains itself) |
| Vinyl | none at defaults | 0.0 | silent / silent |

Every module with a tail at its defaults (14) lost it at once; 23 of 24 clicked (42 to 74 dB); 19 played their old buffer when switched on again, up to -2 dBFS, where the module never bypassed had decayed to silence. The Gx modules here are Debian's guitarix in `/usr/lib/lv2`, which shadows the project's build in this WSL root.

**The plugin's own switch (our plugins):**

| Module | Tail | Click (dB) | Replay: after / never bypassed (dBFS) |
| --- | --- | --- | --- |
| Aura (Wet on) | rings on | 9.5 | -75.4 / -75.2 |
| Discipline | **cut** | 8.9 | silent / -50.7 |
| Discipline, Trails on | rings on | 4.8 | silent / -50.7 |
| Euclidelay | rings on | 15.4 | -24.2 / -24.2 |
| Eureka Skip | rings on | 9.8 | silent / silent |
| Eureka Spool | rings on | 5.4 | -37.6 / -37.8 |
| Eureka Hush | rings on | 10.7 | -47.2 / -47.2 |
| Eureka Slowmo | rings on | 14.2 | -43.5 / -43.5 |
| Drizzle (Feedback, Reverb) | rings on | 6.0 | silent / silent |
| Eureka Granny | rings on | 12.7 | -29.9 / -29.9 |
| Eureka Bloom | none at defaults (-76.8) | 9.9 | -83.4 / -83.4 |
| Eureka Crinkle | none at defaults | 9.6 | silent / silent |
| Eureka Prism | none at defaults (-125.8) | 23.3 | silent / silent |
| Loo-pi | a looper: bypass mutes its loops on purpose | - | - |

None replays; clicks stay at 5 to 15 dB, the tone's own variation, except Prism (23.3 dB while its output is almost silent at its defaults; to be heard on the Pi). Discipline is the only one cutting its tail, and Trails on fixes it. Euclidelay, one input and two outputs, keeps the dry signal on the right while bypassed (-2 dB against the left, its pan).

Bundles of our own plugins came from their repositories' `build/` directories (x86-64), with the Pi's catalogue export for their ports, since the WSL fixture does not list Aura, Discipline, Drizzle or Loo-pi.

## Not covered here

- Not installed in this WSL root: Bitcrushed delay, the three IR reverbs, Beat Repeater, Beat Slicer and Granulator (mono and stereo), Power cut, Reverse (Reflector), Stuck. CV delay has no audio connectors and is skipped.
- **Lost side** needs a module with one input and two outputs under the engine's bypass; the only one, IR Reverb - Mono to Stereo, is not in WSL. To run on the Pi with the rest (P13.10).
- **Defaults without a tail** (Dragonfly Plate and Room, Gx Echo, MDA DubDelay, Gx Multiband reverb): their replay shows the buffer is kept, so their tail is cut as well; the spill bypass check (P13.3) will set them to make a tail.

## Atomic bundles and the existing checks (P13.2)

With the engine started with `-a` (now the default in `env.sh`), the existing checks were rerun unchanged: `07-adapter.sh` (all adapter checks passed, Unix socket and engine restart included; edit round trip median 5.9 ms, p95 11.1 ms), `23-midi.sh` (MIDI cable checks passed), `24-clock.sh` (MIDI clock checks passed), `25-midi-settings.sh` (MIDI settings checks passed). Patch saving and loading, which go through the engine's own bundle format and not through message bundles, behaved as before. No client of Peepo's sends message bundles yet, so nothing else is affected until scenes do.
