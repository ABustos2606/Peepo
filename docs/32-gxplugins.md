# Full GxPlugins collection

**Installed and verified, 18 October 2026.** Peepo includes all 43 effects in
[brummer10/GxPlugins.lv2](https://github.com/brummer10/GxPlugins.lv2) at collection
commit `19df03e5e1be1a97bf3c60c66a0d3f830c0f35b9`. This adds 39 plugins to the
four already installed. The existing SVT, SlowGear, CreamMachine and Supersonic
revisions, URIs and display names are retained.

**19 October update:** the [Patchstorage expansion](35-patchstorage-expansion.md)
adds 62 more Guitarix identities from its main LV2 collection. Peepo now has
105 Guitarix identities in total; the 43 recorded on this page are retained.

**Scope:** this is the complete **GxPlugins.lv2 extra collection**, not every
LV2 effect in the wider Guitarix project. The [main Guitarix repository](https://github.com/brummer10/guitarix)
ships another set of LV2 effects, including delays, reverbs and modulation.
Patchstorage publishes effects from both sets. Its additional Gx entries do
not indicate a failed installation of this 43-plugin collection.

[gxplugins.json](../config/catalogue/gxplugins.json) records each source URL,
full commit, URI and bundle name. The other 39 revisions follow the collection's
gitlinks. The manifest feeds the inventory generator and build script; the
overlay provides names, descriptions and Drive/Amp/Filter/Dynamics categories.
Where the plugin has an output level, it is presented as Output; otherwise the
engine's general Output is used. Desktop fixtures include the 39 new identities.

<!-- gxplugins-list:start -->
## All 43 GxPlugins

These are the names shown in Peepo. All 43 are installed and ready to add; see the [complete module list](33-module-list.md) for the rest of the instrument.

| Module in Peepo | Category | What it does |
| --- | --- | --- |
| Bass amp (SVT) | Amp | Classic tube bass amp simulation. |
| Blue Amp | Amp | Single-ended amp inspired by late 1950s Fender Princeton and Champ designs. |
| Epic | Amp | Based on Valve Junior. |
| Plexi | Amp | Plexi power amp simulation. |
| Power amp (Cream) | Amp | Cream-coloured power amp emulation with bass and treble. |
| Power amp (Supersonic) | Amp | Vintage American power amp emulation. |
| Ultra Cab | Amp | Cabinet simulator pedal. |
| UVox 720k | Amp | Based on Univox 720k keyboard amp from 1978. |
| VBass Pre Amp | Amp | Simulation of the 1984 Vox Venue Bass 100 Pre Amp section. |
| Vox MKII | Amp | Based on Vox MKII solid state preamp of the late 60s. |
| Axis Face | Drive | Based on the Axis Face Silicon fuzz. |
| Baja Tube Driver | Drive | Based on the BaJaTubeDriver. |
| Boob Tube | Drive | Based on the Boob Tube. |
| Bottle Rocket | Drive | Based on the Mesa V1 Bottle Rocket tube overdrive. |
| Club Drive | Drive | Overdrive Pedal Simulation based on a EF86 Pentode Valve Simulation. |
| DOP 250 | Drive | Based on the Analog Man DOD OD-250 Yellow Overdrive. |
| Eternity | Drive | Low compression overdrive pedal. |
| Guvnor | Drive | Based on the Marshall "The Guv'nor" Overdrive. |
| Heathkit | Drive | Based on the Heathkit TA-28 distortion/booster/fuzz. |
| Hot Box | Drive | Based on the Matchless Hot Box tube overdrive. |
| Hyperion | Drive | Based on the Devi Ever FX Hyperion fuzz. |
| Knight Fuzz | Drive | Fuzz based on the Basic Audio Knight Fuzz pedal. |
| Liquid Drive | Drive | Based on the Liquid Drive fuzz, a modified Ross Distortion. |
| Luna | Drive | Gnarly overdrive pedal simulation. |
| Maestro FZ-1B | Drive | Based on the Robert Moog-designed Maestro FZ-1B fuzz. |
| Maestro FZ-1S | Drive | Based on the Maestro FZ-1S Super-Fuzz. |
| Micro Amp | Drive | A simple booster. |
| Saturator | Drive | Based on the Joe Satriani-specified Vox Satchurator distortion. |
| SD-1 | Drive | Based on the Boss SD-1 Super Overdrive. |
| SD-2 Lead | Drive | Based on the Boss SD-2 Dual Overdrive. |
| Shaka Tube | Drive | Based on the ShakaTube. |
| Sloopy Blue | Drive | Overdrive pedal simulation. |
| Sun Face | Drive | Based on the Analog Man Sun Face fuzz. |
| Super Fuzz | Drive | Based on the Univox Super-Fuzz. |
| Suppa Tone Bender | Drive | Based on the Vox Supa Tonebender fuzz. |
| Tim Ray | Drive | Based on the Vemuram Jan Ray. |
| Tone Machine | Drive | Based on the Foxx Tone Machine fuzz. |
| Tube Distortion | Drive | Generic tube distortion based on http://www.montagar.com/~patj/tubedist.gif. |
| Valve Caster | Drive | Based on the Valve Caster. |
| Vintage Fuzz Master | Drive | Based on the Devi Ever Vintage Fuzz Master. |
| Voodoo Fuzz | Drive | Based on the Voodoo Lab SuperFuzz. |
| Slow gear | Dynamics | Automatically swells volume to remove note attack, for violin-like playing. |
| Quack | Filter | Autowah. |

<!-- gxplugins-list:end -->

## Build and verification

`tools/ingen-spike/21-gxplugins.sh` builds all 43 with `make mod` (no desktop UI).
It explicitly overrides `SSE_CFLAGS`, using Cortex-A76 flags on arm64, including
under qemu where `/proc/cpuinfo` describes the x86 host. The full catalogue build
calls this script, so future Pi packages include the collection. A failed build
prevents packaging a partial collection.

All 43 compiled on Debian x86-64 and natively on the Pi's Debian 12 arm64.
The same isolated checks passed on both at 48 kHz, 128 frames:

- Loading through Peepo's Ingen adapter, matching ports and controls.
- Setting every shown control to its minimum, maximum and default; bypass
  toggled and read back through the adapter.
- Passing a 220 Hz sine through every effect, with finite, non-silent output.
- Saving a changed continuous control and restoring it from an Ingen bundle.
  Tube Distortion has no continuous plugin control to restore.

The checks use a separate named JACK dummy server and engine. They do not edit
the instrument's playing patch. These are functional checks, not guitar
listening or simultaneous-effect CPU budgets.

Results: [PC](evidence/gxplugins-amd64.json), [Pi](evidence/gxplugins-arm64.json),
[installation](evidence/gxplugins-install.json). The older full compatibility
matrix remains the 14 October snapshot; these records supplement it.

To repeat (with `python3-jack-client`, `python3-numpy` and Lilv Python bindings):

```bash
bash tools/ingen-spike/21-gxplugins.sh
bash tools/ingen-spike/21-gxplugins-check.sh /tmp/gx-results
```

`GX_EXPORTER` and `GX_LIVE` select development tool binaries; `GX_PLUGIN_PATH`
selects staged bundles. On the Pi, the distribution's package index did not
provide `python3-lilv`; its pure Python binding from the development machine was
used temporarily against the Pi's installed native Lilv library.

## Pi activation

The incremental installer added only missing bundles and kept the original
four binaries. Ingen's `ingen:loadedBundle` API registered the new effects
without restarting the engine or editing its graph. The touchscreen interface
alone restarted to reload the catalogue. JACK, engine and UI remained active;
the engine kept PID 764. Before and after graph snapshots are equivalent
(225 RDF triples each), including Clouds, its controls and cables.

The refreshed Pi catalogue has 269 entries before the overlay: 225 available
and 44 withheld; all 43 GxPlugins are available. The broader selected inventory
has 291 identities, including entries without installed implementations.

Backup metadata: `/opt/peepo/gxplugins-backup.rtARAa`.
Playing-graph snapshots: `/var/lib/peepo/gxplugins-update.b3YiSZ`.
The reusable incremental package is retained locally at
`build/pi/peepo-gxplugins-arm64-20261018.tar.gz`. After unpacking its `stage/` and
`repo/` directories, repeat installation with:

```bash
sudo bash repo/tools/pi/install-gxplugins.sh stage results/results.json
```

## Corresponding source and notices

Each exact plugin source archive and its file-specific notices are installed
under `/opt/peepo/share/peepo/sources/gxplugins` and
`/opt/peepo/share/peepo/licenses/gxplugins`. `SHA256SUMS`, the manifest and the
build script accompany the source archives. For an offline rebuild, run the
supplied `tools/ingen-spike/21-gxplugins.sh` from that source set with a fresh
`PREFIX` and `GX_SOURCE_ROOT`; it verifies and extracts the archives itself.
All 43 also passed this offline source rebuild on the PC.

Source files and root license texts retain their own grants. Several MOD RDF
files advertise ISC while their C++ source contains GPL notices; the RDF field
alone must not be used to label the entire plugin. No upstream source was
modified; compiler flags are supplied as make variables.
