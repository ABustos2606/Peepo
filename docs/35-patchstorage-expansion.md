# Guitarix and Dragonfly expansion

**19 October 2026, after the [first catalogue purge](36-catalogue-purge.md).**
The retained expansion contains three Dragonfly reverbs and 58 additional Guitarix
identities: **61 plugin identities from 58 publications**. With the existing 43
GxPlugins, Peepo selects **101 Guitarix identities**.

Bollie Delay XT and Faust Granulator were removed earlier after listening revealed
problems ([earlier removal](evidence/patchstorage-removal.json)). The first purge also
retires Gx Live looper, Ambience, MVerb, Gx Reverb stereo, Gx Room simulator,
Gx Zita reverb stereo and Dragonfly Early Reflections. The current selection has
**350 identities, 328 in the Pi export and 272 ready to add**. The current patch
was unchanged and the engine and JACK kept running; only the UI restarted.
See the [purge record](evidence/catalogue-purge-20261019.json).

## Sources and repeatable installation

[patchstorage.json](../config/catalogue/patchstorage.json) records the selected
publications, download URLs, SHA-256 checksums, source links, native bundle
names and Peepo presentation. ARM64 uses the published Patchstorage builds.
The PC uses their x64 builds where available, a pinned Debian Guitarix LV2
package for the 14 remaining Gx publications. Detune is rebuilt on both architectures as described below.

`tools/install-patchstorage.py --prefix STAGE` verifies downloads and stages
the bundles without changing services or the current patch. It refuses to
overwrite a differing existing bundle. The full catalogue build calls
`tools/ingen-spike/22-patchstorage.sh`, so future packages include this set.
`tools/catalogue-inventory.py` selects every identity, and
`tools/catalogue-patchstorage.py` refreshes their names, descriptions and
categories without replacing other presentation rules.

The Windows editor keeps its previous definitions and adds the Pi definitions
for these effects, so its new patches target the instrument's controls. PC
audio builds are tested independently; some build versions expose different
controls despite sharing an LV2 identity.

The three retained Dragonfly modules are under Reverb, and the new Guitarix names start
with **Gx** to distinguish them from existing effects.

## Integration fixes

- **Wah discovery.** Both published binaries export Wah and AutoWah, and their
  plugin metadata describes both, but the manifest indexes only Wah. The
  installer adds the missing AutoWah manifest entry.
- **Redeye discovery.** The ARM binary exports Chump, Big Chump and Vibro Chump,
  but its metadata describes only Vibro Chump. The matching metadata from the
  same publication's x64 archive restores the other two variants. Their
  connectors and controls are checked against the native binary.
- **Detune startup.** Its FFTW_MEASURE planning took about 16 seconds on the
  Pi, exceeding the adapter's command timeout. A source rebuild replaces the
  two planning calls with FFTW_ESTIMATE. The exact nine source files, their
  hashes and the pinned source revision are recorded in the manifest; the
  source archive and build recipe are included with installation.
- **Worker lifetime, engine patch 0009.** The debugger caught FFT planning in
  a worker after its plugin had been removed. The engine now counts pending
  asynchronous work and drains it before deactivation/cleanup outside the
  audio thread. Creation, activation, worker calls and cleanup share the
  existing instance lock to protect planner/library operations.
- **Guitarix processing, engine patch 0010.** Sample-offset control changes
  caused runs of arbitrary small sizes and repeated FFT/convolution worker
  reconfiguration. Guitarix blocks now process complete engine periods and
  take the latest control value for the period. Other plugin families retain
  their existing processing behavior.

## Original installation verification

These records describe the original 68-plugin installation, including the two
subsequently removed effects. Automated checks did not establish satisfactory
listening behavior for those effects. Current selection and installer checks
cover the currently selected identities.

The isolated check runs a separately named JACK dummy server and Ingen at
48 kHz and 128 frames. The original run checked all 68 identities through Peepo's adapter:

- Discovery, connectors, parameter defaults, minimum/maximum/default control
  changes, bypass readback and removal.
- Finite, non-silent signal output and a changed control saved and restored
  through an Ingen bundle.
- Both output channels with the dry path removed for Bollie XT, Granulator and
  all four Dragonfly effects. Granulator uses a short buffer and dense grains
  so the measurement captures its processed signal.
- A second control/load pass after the signal checks, exercising repeated
  creation and deletion in the same engine.
- On the Pi, adapter/control regression checks for all 43 existing GxPlugins,
  Clouds, Beebo's Delay and NAM.

The test records include hashes of the manifest, plugin tree and engine
server library. Activation requires a fully passing native record matching
the actual staged files. These are functional checks, not a guitar listening
test of every effect, a combined CPU budget, or proof that captured looper
audio survives patch save/reload.

## Activation and recovery

`tools/pi/install-patchstorage.py` captures the current playing graph, backs up
the engine and catalogue, installs the new bundles, restarts the engine and
UI once, and restores the graph. JACK stays running. It compares the restored
graph with the original as RDF, including another comparison after the UI
reconnects. Failure restores the previous engine/catalogue and the captured
graph. The snapshot and rollback files are retained on the Pi.

Results: [PC, 68/68](evidence/patchstorage-amd64.json),
[Pi, 68/68 plus repeated controls](evidence/patchstorage-arm64.json),
[46 legacy controls/adapter checks](evidence/patchstorage-legacy-controls.txt),
[installation and graph preservation](evidence/patchstorage-install.json),
and [current Pi catalogue after purge](evidence/catalogue-pi-20261019-purged.json).
All seven Windows CTest suites passed.

Recovery files are retained at
`/var/lib/peepo/patchstorage-update-mintm8tr/` on the Pi. The catalogue and
engine backup are under `backup/`, and `before.ingen` is the captured graph.

Removal recovery copies are retained outside the active plugin paths at
`/var/lib/peepo/plugin-removal-04xa4zn9/` on the Pi. They are not offered in Add
and are not included in the current installer manifest.
