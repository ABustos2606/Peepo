# Scenes

**Planned 24 October 2026. P13.1 (measurements) and P13.2 (atomic bundles, engine patch 0012) done in WSL the same day ([record](evidence/scenes-results.md)); nothing of the scenes themselves is built yet.** Decided with the user: a scene stores every module's bypass plus only the parameters marked "per scene"; up to **8 scenes** per patch; delays and reverbs ring on when bypassed, including Discipline's and Aura's by default. The steps are Phase 13 of the [implementation plan](10-technical-implementation-plan.md#phase-13--scenes).

A scene is a saved state inside one patch. "Verse" can have the chorus on and the drive off, and "Solo" the reverse, so one song is played on one patch by changing scenes. Changing scene must not break the sound: no gap, no click, and the echoes and reverb of what was just switched off ring on.

## Why scenes, not patch changes

Loading a patch replaces the engine's blocks with new ones ([Ingen adapter](15-ingen-adapter.md)). Every sound stops, tails included, and there is a gap that has never been measured; [validation](06-validation.md) says not to promise seamless patch changes. Making patch changes seamless would need two graphs running at once and a crossfade between them, which is not planned.

A scene never adds or removes a module. It only sets values and bypass switches on modules that are already running, exactly like a stomp press, which already plays without a gap. So a scene is "many stomp presses at once, saved with the patch". The price: every module used in any scene is in the graph all the time, so **a patch must fit the Pi's CPU with all its modules on**.

## Tails today (measured 24 October 2026)

There are two kinds of bypass ([catalogue export](16-catalogue-export.md)):

**The engine's bypass**, used by every module without `"bypass": "port"` in the overlay: Stereo delay, Dub delay, Tempo delay, Bitcrushed delay, the IR reverbs, Mist, Gx, Dragonfly and the rest. Ingen's `BlockImpl::bypass` is a "dumb bypass": it stops calling the plugin and copies each input to the matching output, and clears an output with no matching input. Three faults for a delay or reverb:

1. **The tail stops dead.** Measured in WSL on all 24 such Delay, Reverb and Texture modules installed there: every one with a tail at its defaults (14) lost it at once.
2. **It clicks.** There is no ramp: 23 of 24 clicked when bypassed with a tone playing, 42 to 74 dB above the tone's own variation.
3. **The plugin's memory freezes.** Its buffer is not cleared, so when it is switched back on the echoes left from before play again: 19 of 24, up to -2 dBFS, where the same module never bypassed had decayed to silence.
4. **Mono in, stereo out loses a side.** A mono-to-stereo module bypassed plays the dry signal on the left only (from the code; the only such module, IR Reverb - Mono to Stereo, is not in WSL and is measured on the Pi).

**The plugin's own switch** (`"bypass": "port"`), used by our own plugins. What each does when switched off, read from their source and measured: none replays, and clicks stay at 5 to 15 dB, the tone's own variation (Eureka Prism measured 23 dB while almost silent at its defaults; to be heard on the Pi).

| Module | When switched off | Tails |
| --- | --- | --- |
| Eureka (all 12) | Input fades out; every effect passes `spill=true`, so the wet sound stays at full level (`eureka/src/dsp/wrapper.hpp`) | Ring on (measured: Skip, Spool, Hush, Slowmo, Granny) |
| Euclidelay | Input fades out of the loops; the voices keep repeating (`src/engine.cpp`) | Ring on (measured) |
| Drizzle | Input to the grains fades out; the wet sound is not faded (`src/drizzle.cpp`) | Ring on (measured) |
| Aura | Wet sound kept while **Trails** is on, default 1 (`aura.ttl`) | Ring on by default (measured) |
| Discipline | Echoes dropped unless **Trails** is on, default 0 (`discipline.ttl`) | **Cut by default** (measured); ring on with Trails on |
| Loo-pi | Each loop mutes with a short ramp (`lv2/stereo_loop.cpp`) | A looper, not a tail: kept as it is |

## Making every delay and reverb ring on

### Spill bypass in the engine

For the third-party modules, a new bypass in Ingen: the block keeps running, its inputs are replaced by silence, and its outputs are added to the dry signal:

```
bypassed output = input + plugin(silence)
```

With a silent input the plugin's own dry path gives nothing, so what comes out is only the tail, whatever the module's mix is set to. The buffer then drains naturally, which also removes fault 3; the ramps below remove fault 2. Details:

- **Ramps.** Switching off fades the plugin's input to silence over about 5 ms; switching on fades it back, and crossfades the output from `input + plugin` to `plugin` over the same time. Without the input fade, a hard cut is itself a click that the delay repeats.
- **Mono to stereo.** An output with no matching input takes the dry signal from the last input, so a mono-to-stereo module bypassed plays the dry signal on both sides (fault 4). The same fix applies to the dumb bypass.
- **Stopping when silent.** Once every output has stayed below -90 dBFS for one second, the block falls back to the dumb bypass and stops using CPU. Its buffer is silent by then, so switching it on again brings nothing back.
- **Allocation.** The silent input buffers and the ramp state are made when the block is created, never in the audio thread.
- **Marking.** A new overlay value, `"bypass": "spill"`, for Delay and Reverb modules (and any Texture module with a long memory) after each passes the tail check. The adapter sets it on the block as a property when the module is added, as it does for the block's output level (engine patch 0008), so it is saved with the patch. The rule is refused for a module that already uses `"bypass": "port"`.
- **Written as** engine patch 0013 in `tools/ingen-spike/patches/`, applied to the build root like the others ([Pi changes go in the build root](19-linux-and-pi.md)).

### Our own plugins

- **Discipline:** Trails defaults to 1. Patches saved with Trails at 0 keep it, so their sound does not change.
- **Aura, Eureka, Euclidelay, Drizzle:** already ring on; each gets the tail check below so it stays that way.
- **Loo-pi:** unchanged. Bypassing a looper mutes its loops, and a scene that does not change Loo-pi's bypass leaves its loops playing.

## What a scene stores

- **Every module's bypass, always.** A module added to a patch that has scenes is written into every scene with its bypass at the time; one removed leaves every scene.
- **Only the parameters marked "per scene".** The patch holds one list of marked parameters, the same for every scene, at most 16. Marking a parameter copies its current value into every scene; unmarking keeps the current value for all of them. Every other parameter is shared: adjusting the amp's tone in Verse changes it in every scene, which is almost always what the player means.
- **A name**, up to 12 characters, default "Scene 1" to "Scene 8".
- **Not stored per scene:** tempo, routing, cables, modules, assignments, the CC list sent on load, file choices (models, IRs, samples). These stay patch-wide.

A patch with no scenes behaves as today. Adding the first scene copies the current state into it; scenes are added up to 8 and can be renamed, copied over another, reordered and deleted (the last one cannot be deleted while others exist; deleting all of them returns the patch to no scenes).

### Storage

Graph properties, like the stomp assignments ([physical controls](18-physical-controls.md)): they live in the engine, are saved in the bundle, and are found again by an interface that restarts. Entries are separated by spaces, fields by `|`; plugin URIs hold neither.

```
peepo:scene-count   3
peepo:scene-current 2
peepo:scene-params  "m4|<plugin>|gain m7|<plugin>|mix"
peepo:scene-1-name  "Verse"
peepo:scene-1       "b|m3|<plugin>|0 b|m4|<plugin>|1 p|m4|<plugin>|gain|0.42 p|m7|<plugin>|mix|0.3"
peepo:scene-2-name  "Solo"
peepo:scene-2       "b|m3|<plugin>|1 b|m4|<plugin>|0 p|m4|<plugin>|gain|0.8 p|m7|<plugin>|mix|0.5"
```

`b` entries hold 1 when bypassed. As with assignments, each entry keeps its module's plugin, so a different module that later holds the same identity is never changed by mistake; an entry whose module is gone or holds another plugin is kept as an orphan, which validation accepts and nothing applies. Duplicating a module copies its scene entries to the copy. The interface's session snapshot (Save snapshot / Load) includes the scene properties.

Loading a patch restores the scene that was current when it was saved: the saved block values already are that scene, so nothing extra is sent.

## Changing scene

- **One message group.** Peepo works out what differs between the live state and the target scene (bypasses and marked parameters only) and sends it between BundleStart and BundleEnd. Ingen executes a bundle in one audio cycle only when started with `--atomic-bundles` (`-a`, default off; `Mark::get_execution`), and even then each change kept the frame it arrived at, so parameter changes in one bundle landed up to 17 frames apart. **Engine patch 0012** (`0012-atomic-bundle-one-frame.patch`) puts every event of an atomic bundle on the cycle's first frame. The `peepo-engine` launcher, `tools/ingen-spike/env.sh` and `tools/pi/check-engine.sh` start the engine with `-a`. Measured in WSL: with both, 120 of 120 bundles (bypasses, parameters, and the two mixed) changed on one frame; without, mixed bundles were up to 138 frames apart. The adapter, patch storage and MIDI checks pass unchanged with `-a` ([record](evidence/scenes-results.md)).
- **One command for every client.** A new engine command `SelectScene{n}`: `IngenEngine` sends the group and sets `peepo:scene-current` in it; the simulator applies the same changes at once. The difference is worked out by a pure function in `src/core`, unit tested, so the interface and `peepo-midi` (already an engine client, independent of the interface) behave the same.
- **Parameter jumps** rely on each plugin smoothing its own controls, as for a knob turned fast. Most do; the scene check below finds any that click, and their overlay gets a note, or Peepo ramps those few itself.
- **Selecting the current scene again** restores its saved state (see "Edits" below).

## Edits

- **On the touch screen, the current scene is edited.** A bypass changed, or a marked parameter set, is written into the current scene in the same message group, and the patch is dirty as for any edit. An unmarked parameter is a patch-wide edit, as today.
- **Stomps, knobs, MIDI CC and learned controls are performance changes:** they change the sound but not the stored scene. Selecting the scene again, or another and back, brings back its saved state. (Helix calls this "discard"; it lets a player kick a boost on for one bar without rewriting the scene.)
- On the module page, a marked parameter's bar shows a small scene marker; holding a bar offers **Per scene** / **Same in every scene**.

## On the screen, stomps and MIDI

Built to the [design language](03-interface-and-controls.md): header pills and one list of bars, no cards. A mockup comes first and is followed as drawn.

- **Patch page:** the scenes as pills along the top, the current one white; touching one selects it. A **Scenes** page (header pills: Add, Rename, Copy to, Delete) lists the eight as bars.
- **Stomps** ([physical controls](18-physical-controls.md)): new actions **Scene 1** to **Scene 8**, **Next scene** and **Previous scene**, per patch or for every patch beside Next patch and Tap tempo. Next scene wraps from the last to the first.
- **MIDI:** a CC selects a scene (value 0 to 7 is scene 1 to 8), on the program-change channel set in Settings → MIDI. The CC number is a setting, default 69 (Line 6's choice, so foot controllers already set up for it work). Program change still selects a patch.
- **Later:** a scene may also send a few CC messages to the player's other gear, like the patch's CC list on load, so an external pedal changes with the scene.

## Checks

- **Tail check** (`tools/ingen-spike/27-scenes.sh tails`, root, in WSL; `SCENES_LV2` adds bundle directories): for each Delay, Reverb and Texture module, the tail, click, replay and side checks described in the [record](evidence/scenes-results.md). Pass: the tail within 3 dB of the module left on, a click no more than 20 dB above the tone's own variation, no replay, no lost side. The engine's bypass fails all of them, which shows the check works.
- **Atomic change** (`27-scenes.sh atomic`): two modules switched opposite ways in one bundle, by bypass, by parameter and mixed; both outputs must change on the same frame. Run with and without `-a`.
- **Scene changes while playing:** 8 scenes, 4 modules each changing, switched every 500 ms for a minute: no xrun, no gap, every change read back.
- **CPU:** the heaviest catalogue patch with all modules on; spill-bypassed delays stop costing CPU after their tail.
- **Storage:** `peepo_core_tests` (differences, edits, orphans, duplicate, remove), `peepo_storage_tests` (bundle round trip), `peepo_ingen_tests` (a second client sees a scene change); `peepo_ui_check` screens for the pills, the Scenes page and the marker.
- **On the Pi**, by ear, with guitar: a verse/solo patch with a delay and a reverb switched by stomps and by a MIDI foot controller.
