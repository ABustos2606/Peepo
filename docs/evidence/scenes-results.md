# Scenes: bypass tails and atomic changes, measured (P13.1, P13.2), the spill bypass (P13.3), its marks in the catalogue (P13.4), our own plugins (P13.5) and scenes sent and stored (P13.7)

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

## Spill bypass in the catalogue (P13.4)

25 October 2026, same WSL setup (engine patches 0001-0013, `-a`). Raw output: [scenes-p134-run.txt](scenes-p134-run.txt); data: [scenes-p134.json](scenes-p134.json).

**What changed:** the overlay accepts `"bypass": "spill"` (`ModuleDefinition::spill_bypass`). It is refused for a module with no audio output, and beside `"bypass": "port"`, whether the two come from one rule list or from two rules; `"bypass": "engine"` clears either. The adapter puts `peepo:spillBypass true` on the block when it adds the module and when it rebuilds a session snapshot, and sets it on every marked block after a stored patch loads, before reading the graph back, so a patch saved before its module was marked carries the mark from its next save. The simulator's bundle writes it too. The bypass itself is unchanged: `ingen:enabled false`, which the engine now spills.

**Settings for the modules silent at their defaults** (now in `27-scenes.py`'s `SETTINGS`): Gx Echo Cat Head 1 and 2 on, Swell 0.8, Sustain 0.3; Gx Echo stereo both levels 60 and times 400 ms; MDA DubDelay Delay 0.3 and FX Mix 0.6 (at 0.1 it measured nothing in the window: the delay glides from its default); Dragonfly Plate Decay 3 s, Wet 50; Dragonfly Room Decay 3 s, Late 50; Gx Multiband reverb every room size 0.95 and wet 100. With Sustain 0.6 Echo Cat's echoes outlasted the 20 s wait, so it never slept within it; at 0.3 they end after 3 s and it sleeps. **Mist Beat Repeat** repeats only on a trigger at its CV input, so a burst gives it no tail at any setting tried; it is not marked.

**The tail check, with the overlay's marks:** `27-scenes.sh tails`, which now marks a block exactly when the overlay says `"spill"`, as the adapter does. Every one of the 21 marked modules rings on (within 0.7 dB of the module left on), clicks at most 18.4 dB switching off and on (Gx Tube delay; Gx Multiband echo, Multiband reverb and Shimmizita 16 dB), never replays, and falls back to about the empty engine's CPU after its tail. Mist Spectral (unmarked) still cuts and clicks under the dumb bypass, as before; Vinyl makes nothing at its defaults.

| Marked `"bypass": "spill"` | Modules |
| --- | --- |
| Delay (14) | Stereo delay, MDA DubDelay, Tempo delay, Mist Looper, Gx Delay stereo, Gx Digital delay (mono and stereo), Gx Duck delay (mono and stereo), Gx Echo stereo, Gx Echo Cat, Gx Multiband delay, Gx Multiband echo, Gx Tube delay |
| Reverb (6) | Dragonfly Hall, Plate and Room, Gx Multiband reverb, Gx Shimmizita, Mist Oliverb |
| Texture (1) | Mist: a long memory and its own reverb; rings on like the reverbs |

**Not marked:** our own plugins (`"bypass": "port"`, P13.5); Mist Spectral and Vinyl (they sound, or cost CPU, by themselves); Mist Beat Repeat (no tail without a trigger); CV delay (no audio); and the modules not installed in WSL: Bitcrushed delay, the three IR reverbs, and the AVW, TAP and infamous Texture modules. Those wait for the tail check on the Pi (P13.10).

**Through the engine's own save and load** (`peepo_ingen_live --spill http://drobilla.net/plugins/mda/Delay`, with `PEEPO_OVERLAY`): Stereo delay and an unmarked amplifier added and the delay bypassed; Ingen's `main.ttl` has `peepo:spillBypass true` on the delay's block alone; a copy with the mark removed loads bypassed and unmodified, and saved again has the mark. `07-adapter.sh` passes unchanged (Unix socket, TCP, the audio check and the engine restart). Unit tests: `peepo_catalogue_tests` (the rule, its refusals, and the 21 marks in the fixture, none of them Mist Spectral or Vinyl), `peepo_ingen_tests` (the mark on add, on bypass, after a snapshot restore and after loading an unmarked patch, which stays unmodified), `peepo_storage_tests` (the simulator's bundle). Stopping the engine after the live check ended in a segmentation fault in its exit handler, as already recorded before patches 0012 and 0013 ([NAM](nam-results.txt), [Pi engine check](pi-engine-check-results.txt)).

## Our own plugins (P13.5)

25 October 2026, same WSL setup (engine patches 0001-0013, `-a`). Script: `tools/ingen-spike/28-own-tails.sh`, which builds each of our plugins at the commit `14-more-plugins.sh` pins (Discipline at 12625b6, the rest unchanged), runs `27-scenes.sh tails` on them twice (the click switching off, then off and on again, `SCENES_ON_AGAIN=1`), and checks Trails in saved patches. Raw output: [scenes-p135-run.txt](scenes-p135-run.txt); data: [scenes-p135.json](scenes-p135.json).

**What changed:** Discipline's Trails default to 1 (`discipline.ttl`, and the plugin's own fallback when the port is not connected), LV2 version 0.4.1, its tests passing; no port, symbol or URI changed. Pinned at 12625b6 in `14-more-plugins.sh` and listed in `reference/manifest.json`. `27-scenes.py` measures Discipline at its default and a second time with Trails off. The module list marks our own modules whose tail rings on (`OWN_TAILS` in `tools/document-catalogue.py`), as it marks the spill bypass.

| Module (own switch) | Tail | Click off (dB) | Click off and on (dB) | Replay |
| --- | --- | --- | --- | --- |
| Aura (Wet on) | rings on | 8.9 | 19.5 | none |
| **Discipline** (Trails on, the new default) | **rings on** | 4.8 | 8.0 | none |
| Discipline, Trails off | cut | 6.1 | 4.3 | none |
| Euclidelay | rings on | 15.4 | 14.5 | none |
| Eureka Skip | rings on | 3.9 | 3.9 | none |
| Eureka Spool | rings on | 1.1 | 1.2 | none |
| Eureka Hush | rings on | 7.5 | 5.5 | none |
| Eureka Slowmo | rings on | 5.6 | 4.6 | none |
| Eureka Granny | rings on | 1.7 | 2.6 | none |
| Drizzle (Feedback, Reverb) | rings on | 7.0 | 5.8 | none |
| Eureka Bloom, Crinkle | no tail at their defaults | 6.4, 10.8 | 6.9, 6.4 | none |
| Eureka Prism | no tail at its defaults (-110.6) | 28.6 | 29.1 | none |
| Loo-pi | a looper: bypass mutes its loops on purpose, unchanged | - | - | - |

Every module with a tail rings on, within 0.3 dB of the module left on, and none replays. Euclidelay keeps its right side (-1 to -2 dB, its pan); under the engine's bypass it is cut, clicks and replays, as before, so its overlay stays `"bypass": "port"`.

**Xruns, not clicks.** Before the retry below, Discipline switching off and on measured 66 to 68 dB in 4 of 17 runs, Euclidelay 36 and 68 dB in 2 of 9, and Discipline with Trails off 56 dB once in 8. With `SCENES_DUMP` keeping each recording over 20 dB (Discipline's eight runs, three kept), every kept recording had 1 to 3 xruns reported by JACK during it (the WSL dummy backend), and its jump was a step in the 220 Hz tone on the last frame of a 128-frame cycle, where whole cycles were lost from the recording, with no silence around it. Euclidelay's were not kept; with the retry it measured 14.4 to 14.6 dB in all five runs, so they were most likely the same, as was P13.3's 56.5 dB. `27-scenes.py` now counts JACK's xruns and measures the click again, up to three times, when one happened during its recording.

**Clicks over 20 dB, four runs more** (same builds, xruns measured again): switching off and on, **Aura** 21.0, 20.5, 17.2 and 14.2 dB, around the 20 dB mark; **Euclidelay** 14.4 to 14.6 every time; **Eureka Prism** 26.4 to 29.4 dB, also switching off only, while its output is about -110 dBFS, so measured against almost nothing. Eureka Hush and Granny, 20.0 and 22.8 dB switching off and on in P13.3's run (from the repositories' `build/` directories), measured 5.5 and 2.6 at their pins here. Aura and Prism are for the ear on the Pi (P13.10).

**Trails in saved patches** (`28-own-tails.py`): a Discipline 0.4.0 block (4f68f3b, Trails untouched) saved by Ingen holds `ingen:value 0.0` for Trails, since Ingen saves every control's value. Loaded into an engine with 0.4.1 (12625b6), its Trails stay 0; a new block gets 1; saved again, both keep their values. So a patch saved before plays as it did, and only modules added from now on ring on.

**Not covered:** the arm64 builds and the Pi (P13.10).

## Scenes sent and stored (P13.7)

25 October 2026, same WSL setup, engine with patches 0001-0013, started with `-a`. Raw output: [scenes-p137-run.txt](scenes-p137-run.txt).

**What was built.** Engine commands in `src/engine/engine.hpp`: `SelectScene`, `AddScene`, `RenameScene`, `CopyScene`, `MoveScene`, `DeleteScene` and `MarkSceneParameter` (`marked` false unmarks). `SetBypass` and `SetParameter` take `scene` for an edit on the touch screen, written into the current scene in the same request group; stomps, knobs and MIDI leave it false. `IngenEngine` sends a selection as `ingen:BundleStart`, the bypasses and values that differ (`scene_changes`), `peepo:scene-current`, then `ingen:BundleEnd`; the engine answers the two marks like any request. Selecting a scene leaves the unsaved marker alone: it is playing, not editing. Selecting the current scene with nothing to change sends nothing. A scene edit, and adding, duplicating or removing a module in a patch with scenes, sends the scene properties that changed (`peepo:scene-count`, `-current`, `-params`, `-N-name`, `-N`), and empties the slots no longer used. A new patch and a load empty them first, so a bundle without scenes leaves none. The simulator applies the same commands to its graph. `fit_scenes` brings scene values read from the engine or a bundle inside their parameter's range, as the live values are. Patch bundles (`write_main_ttl`, `read_bundle`, `bundle_graph`) carry the properties, and the session snapshot carries them through `LoadGraph`.

**Against the real engine** (`peepo_ingen_live --scenes`): two eg-amp blocks and two scenes, the second block's gain marked per scene; in scene 2 the first block bypassed and the second turned down from the screen. A second client reads the scenes; selecting scene 1 restores both blocks, the second client sees it, and the patch stays unmodified. Ingen's `main.ttl` holds the seven scene properties; a new patch has none; loading the patch restores both scenes with the second current and its values the saved ones; a restarted interface finds them; the session snapshot keeps them. 27 of 27 pass. `07-adapter.sh` passes unchanged (Unix socket, TCP, the audio check and the engine restart).

**Unit tests:** `peepo_ingen_tests` (the stand-in engine records each bundle: a selection is one bundle holding the bypass, the value and `scene-current`; a second client and a restarted one see the change; rename, move, copy, delete, mark and unmark; a module added, duplicated and removed; snapshot, save and load) and `peepo_storage_tests` (bundle round trip with an orphan and a quoted name; the simulator saves with the second scene current and loads it so). All 8 CTest suites pass on Windows; the core, Ingen and storage tests pass in WSL.

**Not measured here:** that the adapter's own bundle lands on one frame (`27-scenes.sh atomic` showed it for bundles of the same shape, P13.2); scene changes while playing, by ear; the Pi (P13.10).

## Stomps and MIDI (P13.9)

25 October 2026. Windows build (all 8 CTest suites), and WSL Debian with the engine started with `-a` (patches 0001-0013). Raw output: [scenes-p139-run.txt](scenes-p139-run.txt).

**What was built.** `ControlRecognizer` holds a stomp's press for `chord_ms`, 40 ms. A neighbour pressed within that time makes the two one switch: `stomp4` (D, A+B) or `stomp5` (E, B+C), released with the first of the two. `ControlLayout` lists A to E, and `control_label` and `stomp_letter` name them by letter. In scene mode `Application::scene_stomp` plays scene A-C of the bank shown and steps the bank with D and E. `scene_change` reads the scene controller in `peepo-midi`. `scene_cc` in the MIDI settings file holds the controller, default 69, set on Settings → MIDI. The Foot switch module's Stomp choices read A, B and C (overlay).

**Unit and screen checks (Windows).** `peepo_controls_tests`: A then B within 40 ms is D, C then B is E; a single stomp waits 40 ms; the first released ends D and the other says nothing; a neighbour after 40 ms is a stomp of its own; A and C together are two stomps; a quick tap inside the wait is played at its release; a stomp sent as D directly acts at once. `peepo_midi_tests`: the scene controller on its channel and on any; refused from another channel or controller, with program change off, cut short, past scene 8, or from a program change; `scene_cc` read and written, 128 and -1 refused. `peepo_ui_check` (capture 95): in scene mode C plays Solo; E steps to bank 2 and names it; B there has no scene and says so; A plays Bridge; D steps back; back in stomp mode, A+B pressed on Drive's actions assigns D, which then switches Drive and shows a D on it. The earlier captures now show letters.

**Against the real engine** (`25-midi-settings.sh`, extended): Charlie is stored with three scenes, the third playing. It loads with the third. CC 69 value 0 plays scene 1, and the patch stays unedited. CC 70 and a value past the patch's scenes change nothing. With `scene_cc=11`, CC 11 value 1 plays scene 2. A program change to Charlie and CC 11 value 0 in one period load Charlie, then play scene 1, not the saved third. A scene on another channel than program change is ignored. The earlier program-change, clock and MIDI learn checks pass unchanged.

**Not covered:** the 40 ms wait with real footswitches and one foot pressing two of them; scene changes by stomp and by a foot controller while playing, by ear (P13.10). The Linux interface was not built here (WSL builds without it), so the simulator keys V and B were only built for Windows.
