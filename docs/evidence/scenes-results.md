# Scenes: bypass tails and atomic changes, measured (P13.1, P13.2), and the spill bypass (P13.3)

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
- **Defaults without a tail** (Dragonfly Plate and Room, Gx Echo, MDA DubDelay, Gx Multiband reverb): their replay shows the buffer is kept, so their tail is cut as well. Under the spill bypass none of them replays (below); their tails at settings that make one are for P13.4's tail check, before each is marked.

## Atomic bundles and the existing checks (P13.2)

With the engine started with `-a` (now the default in `env.sh`), the existing checks were rerun unchanged: `07-adapter.sh` (all adapter checks passed, Unix socket and engine restart included; edit round trip median 5.9 ms, p95 11.1 ms), `23-midi.sh` (MIDI cable checks passed), `24-clock.sh` (MIDI clock checks passed), `25-midi-settings.sh` (MIDI settings checks passed). Patch saving and loading, which go through the engine's own bundle format and not through message bundles, behaved as before. No client of Peepo's sends message bundles yet, so nothing else is affected until scenes do.

## Spill bypass, engine patch 0013 (P13.3)

24 October 2026, same WSL setup, engine rebuilt with patches 0001-0013 and started with `-a`. Script: `27-scenes.sh spill`, with `SCENES_LV2` naming the Euclidelay and Eureka build directories. Raw output: [scenes-spill-run.txt](scenes-spill-run.txt); data: [scenes-spill.json](scenes-spill.json).

**What the patch does** (`tools/ingen-spike/patches/0013-spill-bypass.patch`, in `BlockImpl` and `LV2Block`): a block with the property `peepo:spillBypass true` (Bool or Int; `urn:peepo:ns#spillBypass`) keeps running when bypassed. Its audio inputs are replaced by copies faded to silence over 5 ms, and the dry signal is crossfaded in over the same time and added to its outputs; switching on reverses both. The dry signal for an output with no matching input is the last audio input, in the spill bypass and in the dumb bypass. Once its outputs stay below -90 dBFS for one second (before the block's output level) it falls back to the dumb bypass until switched on again. The faded copies are made in `LV2Block::instantiate`, never in the audio thread. A block already in the dumb bypass when it is marked stays there until switched on, so a frozen buffer never spills out. A duplicated block keeps the mark. The block's output level applies to its tail, not to the dry signal. Unmarked blocks bypass as before.

**Method:** the checks of P13.1, with every module that uses the engine's bypass marked right after it is created. The click window now holds both switches: off, then on again 0.2 s later. One more check, **sleep**: the engine's CPU use over 3 s windows with the module on, right after it is bypassed following a burst, and after its tail has stayed below -90 dBFS for 2.5 s (at most 20 s), against the engine with no module (2.0 %).

| Module | Tail | Click, off and on (dB) | Replay: after / never bypassed (dBFS) | CPU %: on / spilling / after the tail |
| --- | --- | --- | --- | --- |
| Euclidelay, engine bypass (1 in, 2 out) | rings on | 14.4 | -24.2 / -24.2 | 2.7 / 2.3 / 2.3 |
| Gx Delay stereo | rings on | 0.0 | silent / silent | 2.0 / 2.0 / 1.7 |
| Gx Digital delay | rings on | 4.3 | -48.6 / -48.6 | 2.3 / 2.0 / 2.0 |
| Gx Digital delay stereo | rings on | 4.3 | -48.6 / -48.6 | 2.3 / 2.3 / 1.7 |
| Gx Duck delay | rings on | 0.0 | silent / silent | 1.7 / 2.0 / 1.7 |
| Gx Duck delay stereo | rings on | 0.0 | silent / silent | 2.0 / 2.0 / 1.3 |
| Gx Echo Cat | none at defaults | 19.2 | silent / silent | 4.7 / 2.7 / 1.3 |
| Gx Echo stereo | none at defaults (-71.7) | 6.0 | silent / silent | 1.7 / 2.0 / 2.0 |
| Gx Multiband delay | rings on | 1.4 | -35.2 / -35.2 | 2.3 / 3.0 / 2.7 |
| Gx Multiband echo | rings on | 9.4 | -54.7 / -54.7 | 2.7 / 2.7 / 3.3 |
| Gx Tube delay | rings on | 6.9 | silent / silent | 2.7 / 2.7 / 1.3 |
| MDA DubDelay | none at defaults | 0.1 | -18.6 / -26.7 | 1.7 / 2.0 / 2.0 |
| Mist Beat Repeat | none at defaults | 3.7 | silent / silent | 2.7 / 2.7 / 2.0 |
| Mist Looper | rings on | 2.6 | -54.0 / -54.0 | 2.7 / 3.0 / 1.7 |
| Stereo delay | rings on | 12.5 | -69.1 / -69.1 | 1.7 / 1.7 / 1.7 |
| Tempo delay | rings on | 16.6 | -120.6 / -120.6 | 2.0 / 1.7 / 1.7 |
| Dragonfly Hall | rings on | 5.2 | -122.8 / -128.1 | 4.3 / 3.7 / 2.0 |
| Dragonfly Plate | none at defaults (-98.5) | 6.1 | silent / silent | 2.0 / 2.3 / 1.7 |
| Dragonfly Room | none at defaults (-80.5) | 3.8 | -159.3 / silent | 4.3 / 3.3 / 1.7 |
| Gx Multiband reverb | none at defaults (-72.7) | 16.9 | -139.2 / -159.7 | 4.0 / 3.7 / 3.3 |
| Gx Shimmizita | rings on | 15.5 | -80.3 / -80.3 | 3.7 / 3.7 / 1.7 |
| Mist Oliverb | rings on | 8.9 | -38.8 / -39.5 | 4.0 / 4.0 / 2.0 |
| Mist | rings on | 4.6 | silent / silent | 2.3 / 2.3 / 2.0 |
| Mist Spectral | rings on (sustains itself) | 12.2 | 3.0 / 3.0 | 4.3 / 4.3 / 4.3 |
| Vinyl | none at defaults | 0.0 | silent / silent | 2.7 / 5.0 / 6.0 |

Against the engine's bypass (P13.1): every module with a tail at its defaults (14, and Euclidelay) **rings on**, within 0.4 dB of the module left on (Mist 3.9 dB louder: its tail with a silent input is longer than with the module on). **No click** above 20 dB, off and on (largest 19.2, Gx Echo Cat, silent at its defaults; was 42 to 74). **No replay**: every module switched on again plays what the module never bypassed plays (MDA DubDelay is 8 dB louder at that moment than the module never bypassed, under the 10 dB mark; its echoes sustain either way). **No lost side:** Euclidelay, the only module with one input and two outputs in WSL, measured under the engine's bypass, plays its right side at -1 dB of its left with spill, and at +0 dB under the dumb bypass, which also has the fix (`27-scenes.sh tails euclidelay`, same day).

**Sleep:** the heavier modules fall back to the empty engine's CPU after their tail (Dragonfly Hall 4.3 → 2.0 %, Room 4.3 → 1.7, Shimmizita 3.7 → 1.7, Oliverb 4.0 → 2.0, Echo Cat 4.7 → 1.3). The lighter ones are within the 0.3 % step of these windows. Two do not sleep, as expected from what they are: **Mist Spectral** sustains its own sound with no input (it stays at -8 dBFS, and the same is true of the module left on), so it would keep sounding when bypassed; **Vinyl** stayed at 5 to 6 %, unexplained (its output was silent). Neither should be marked `"bypass": "spill"` (P13.4). The Pi's CPU check (P13.10) measures this properly.

**Unmarked blocks:** `27-scenes.sh tails` on Stereo delay with patch 0013 gives the P13.1 result (cut, 54.3 dB click, replay at -11.4 dBFS).

**Our plugins' own switch, off and on:** measured in the same run (their bypass is unchanged by 0013). Tails ring on and nothing replays, but switching back on adds to the click: Eureka Hush 20.0 dB, Granny 22.8 dB, Prism 27.9 dB (was 10.7, 12.7 and 23.3 switching off only); the others stay at 13 to 15 dB. Euclidelay measured 56.5 dB once and 14.4 dB in the full run. For P13.5, by ear on the Pi.

**Not covered:** the modules not installed in WSL (listed above) and the arm64 build's behaviour; both in P13.10.
