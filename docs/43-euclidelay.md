# Euclidelay integration

**23 October 2026. In the inventory, overlay, WSL fixture and module list; patch tempo and the module page checked; engine patch 0011 written. Installed in the Pi's catalogue on 24 October 2026 (UTC).**

Euclidelay is a delay whose echoes fall on the hits of four Euclidean rhythms, the project's own: https://github.com/ABustos2606/euclidelay, GPL-2.0-or-later, Copyright (c) 2026 Alejandro Bustos. Its spec is the repository's `docs/design.md`; its patterns match the Euclidean module's for the same numbers, tested against `src/core/euclidean.hpp`. This is its milestone 5.

## Build and catalogue

- **Pin.** `14-more-plugins.sh` builds commit `4ee5c87c266691ba415049885734b297ef327e8f` (its milestone 4, ready to tag `v0.1.0`) with plain `make`, the same flags as Discipline (its Makefile adds `-fno-fast-math` after `OPT`). The pin is also in `reference/manifest.json`, and `tools/pi/check-prefix.py` now requires `euclidelay.lv2`. The arm64 build root and the Pi already have the bundle from milestone 4; the Pi's `euclidelay.ttl` and `manifest.ttl` are byte-identical to the pin's.
- **Inventory.** `tools/catalogue-inventory.py` lists `https://github.com/ABustos2606/euclidelay#euclidelay`; the inventory has 371 identities.
- **Overlay.** Name Euclidelay, category Delay, a description of what it does, `"bypass": "port"` (the engine's bypass would cut the echoes and leave the right side silent) and `"tempo": "bpm"`. Groups: Main (Step, Echoes, Dry, Tone), Voice 1 to Voice 4 (the voice's switch, Steps, Hits, Shift, Feedback, Level, Pan) and Tempo and output (BPM and the general Output). Groups are headings on one page with no size limit, so each voice stays one group.
- **Exports.** Exported alone with `peepo_catalogue_export --only`, in WSL and on the Pi: available, `enable` = `enabled`, 33 controls (Enabled is left to the plugin). The two records are identical apart from the bundle path. `tools/merge-catalogue.py` merged them into `config/catalogue/wsl-debian.json` (339 modules) and into [the Pi evidence export](evidence/catalogue-pi-20261023-euclidelay.json) (351), keeping every other record byte for byte; it refuses an export with other identities or an unavailable one. The [module list](33-module-list.md) was regenerated from that export: Euclidelay is Ready under Delay.
- **On the Pi.** The installed catalogue, overlay and inventory were backed up to `/opt/peepo/share/peepo/backups/euclidelay-20261024T024355Z/`; Euclidelay was exported again there and merged into `catalogue.json` (351 modules) with `tools/merge-catalogue.py`, and the overlay and inventory replaced by this repository's, which differed from the installed ones only by Euclidelay. The running engine predated the bundle, so `peepo-engine` was restarted (the playing patch was a test), then `peepo-ui`, `peepo-route` and `peepo-midi`; all came back active and the UI logged no catalogue error. The engine created a Euclidelay block (status 0) and deleted it again. Still to run: `python3 tools/source-cache.py save` for the new pin.

## Module page

The generic page: 34 bars (33 controls and the general Output) under six headings, the voice's own switch first under each voice heading. `peepo_ui_check` loads Euclidelay from the WSL fixture and the overlay, not from a hand-written entry, and checks every heading and bar, the group order, Step showing 1/16, no Enabled bar, and that the rail reaches the last controls (`86-euclidelay.bmp`, `87-euclidelay-end.bmp`). Passed on Windows, with the full `ctest`. Not yet seen on the 5-inch touchscreen. A page with four rings, like the Euclidean module's, is later work and needs no new port.

## Patch tempo

`tools/ingen-spike/26-euclidelay.sh` (root, in WSL; `EUCLIDELAY_LV2` adds another bundle directory) sets the tempo through Peepo's adapter, as the Tempo page does, with voice 1 alone at 4 steps, 4 hits, no feedback, 1/16. `peepo_ingen_live` reads `PEEPO_OVERLAY` for this, since the overlay names the tempo control. Results on the dummy JACK server, 48 kHz, 128 frames:

| Patch tempo | `bpm` read back | First three echoes (samples) |
| --- | --- | --- |
| 120 | 120 | 6000, 12000, 18000 |
| 97 | 97.0 | 7423, 14845, 22268 |
| 143 | 143.0 | 5035, 10070, 15105 |
| saved at 87, then 100 played, then loaded | 100.0, then 87.0 | 8276, 16552, 24828 |

Every echo is on its step to the sample. The probe connects to the engine only between the adapter's runs: a client that does not read its socket fills it with the engine's broadcasts, and the engine then stalls (the adapter's requests time out).

## Engine patch 0011: describing a graph whose ports are being created

Euclidelay's milestone 4 found that loading a saved patch whose block fails to `instantiate` (for example when memory is short) crashes Ingen when the graph is next described, which Peepo does after every load. The cause is not the failed block. `ClientUpdate::put_graph` counted the graph's ports from the pre-processor's lists but read each one from the process thread's ports array, which grows only when the audio thread executes a port's creation. A `Get` pre-processed straight after a load's port creations, before the next cycle, read past the array. A block that instantiates takes long enough for the cycle to come first; a failed one does not, so the load raced the `Get` and lost. Any load could in principle do the same.

`patches/0011-describe-graph-ports-being-created.patch` lists the ports from the pre-processor's own lists, in index order as before. Euclidelay's `host_smoke.py` against a refusing build: Peepo's installed engine crashed in 4 runs of 4 (SIGSEGV), the patched one loaded the patch without the block and described the graph in 4 of 4, with every other host check passing; the tempo check above also passes against it. Built in a scratch tree at first; WSL's `/opt/peepo` was rebuilt with it (and 0012) on 24 October 2026 for [scenes](44-scenes.md). The Pi runs the unpatched engine until the package is rebuilt.

## Still to do

- Rebuild the engine with patch 0011 in WSL and for the Pi.
- On the instrument: the page on the touchscreen, a guitar through the default patch, the tempo from the Tempo page and a tap, and bypass leaving trails.
- Euclidelay's own open items (listening, tuning, the 30-minute Pi run on the new power supply) are in its implementation plan.
