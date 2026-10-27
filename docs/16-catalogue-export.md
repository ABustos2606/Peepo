# Catalogue export

**Implemented:** 13 October 2026, on the Windows PC with the engine under WSL2. **Scope:** P4.4 (metadata discovery), with first host evidence for P6.2 and P9.3. **Verified against:** the patched Ingen of [the engine evidence](14-ingen-engine-evidence.md), x86-64, JACK's dummy backend. **Not covered:** the Pi, an ARM build of any plugin, how any module sounds, processor load, plugin state, and MIDI.

The interface no longer needs a hand-written entry to use a real plugin. A Linux tool reads the installed LV2 plugins and writes a catalogue file; the interface loads that file when it drives a real engine. Windows uses an exported file as a fixture, so it needs no LV2 binaries.

![The Windows interface showing Clouds loaded in the live engine](images/ui-ingen-catalogue.png)

## What was verified

`bash tools/ingen-spike/08-catalogue.sh`, as root inside the Debian of the engine evidence, exports the catalogue, loads every offered entry in a live engine and measures sound. The recorded output is in [evidence/catalogue-export-results.txt](evidence/catalogue-export-results.txt): 67 checks passed, none failed.

| Check | Result |
| --- | --- |
| Export | 64 plugins exported: 58 offered, 6 withheld with a reason. 4 engine-internal templates skipped. |
| Load and match | All 58 offered modules load in Ingen through the adapter. For each, the engine's connectors equal the catalogue's, every control accepts its minimum, maximum and default, bypass reads back both ways, and the module is removed again. The engine was still running afterwards. |
| Mutable-derived modules | All 21 of Poly's modules (Clouds in its seven modes, Warps in nine, Plaits, Rings, Marbles, Tides, Grids) built for x86-64 at the pinned commit and are among the 58. |
| Sound | A 440 Hz sine fed through Clouds (granular), added and cabled by the adapter, came back at 0.035 RMS for 0.177 in. Asked of the engine directly: Clouds granular 0.031, Clouds Oliverb 0.063, Warps wavefolder 0.516, MDA Overdrive 0.177, MDA Delay 0.216. These show the DSP runs in the host; they say nothing about how it sounds. |
| Two readings agree | The Mutable-derived entries exported through Lilv from the installed bundles equal, connector for connector and control for control, the earlier reading of the upstream Turtle files through rdflib. |
| Windows | The Windows build loaded the same 58 modules in the WSL engine over TCP from the exported file. All five Windows CTest suites pass. |

## A finding that changed the adapter

**Clouds, Warps and Rings are silent until switched on.** Each has a control named `enabled` whose default is 0, and with it at 0 the plugin writes silence. Poly's interface sets that control instead of using the engine's bypass. Loaded with default values, all three measured 0.0000 out.

The catalogue now records such a control as the module's on/off switch. Adding the module sets it to 1, and it stays at 1. **Software bypass** uses the engine's own bypass, as for every other module, which passes each input to its output. (Clearing the switch instead only muted the module, which made bypass act as a kill switch.) A module whose switch is 0, from a patch saved before this change, reads as bypassed, and enabling it sets the switch back to 1. The switch is not shown as a parameter.

**A plugin that bypasses itself.** The engine's bypass copies input to output and stops calling the plugin. A plugin that keeps working while bypassed (a delay whose echoes ring on, or whose memory keeps recording, such as Discipline) needs the opposite. An overlay rule with `"bypass": "port"` marks it: software bypass, stomp bypass and saved patches then set the plugin's own switch to 0, and the block stays enabled. The rule is refused for a plugin with no such switch. `"bypass": "engine"` restores the usual behaviour.

**A delay or reverb whose tail rings on.** `"bypass": "spill"` keeps the engine's bypass but makes it spill (engine patch 0013, [Scenes](44-scenes.md#spill-bypass-in-the-engine)): the plugin keeps running on silence and its tail is added to the dry signal until it has died away. The adapter puts `peepo:spillBypass true` on the block when it adds the module, rebuilds a snapshot or loads a stored patch, so the engine saves it with the patch. The rule is refused for a module with no audio output, and beside `"bypass": "port"`, from the same rule list or another rule; `"bypass": "engine"` clears either. Marked on the 21 delays and reverbs that passed the tail check in WSL (25 October 2026, [record](evidence/scenes-results.md#spill-bypass-in-the-catalogue-p134)); the [module list](33-module-list.md) says *tail rings on when bypassed* for each.

## How it works

| Part | Role |
| --- | --- |
| `tools/catalogue-export/` (`peepo_catalogue_export`) | Linux tool. Reads plugins through Lilv, as the engine does, following the same `LV2_PATH`. Writes the catalogue file. Loads no plugin code. |
| `src/catalogue/` | Reads and writes the file on every platform, applies the overlay, and holds a small strict JSON reader. |
| `config/catalogue/wsl-debian.json` | The exported file for the WSL engine: the fixture Windows uses. |
| `config/catalogue/overlay.json` | Project-owned presentation rules, kept apart so a new export does not undo them. |
| `tests/catalogue_live.cpp` (`peepo_catalogue_live`) | Loads every offered entry in a running engine and compares it with the file. |
| `tools/ingen-spike/02-plugins.sh` | Builds Poly's Mutable-derived plugins and installs Debian's MDA effects for the development engine. |
| `tools/ingen-spike/09-sound.py` | Measures a sine through chosen modules. |

**What the exporter decides.** A module is offered when the engine could load and patch it. It is withheld, with the reason shown in the browser, when it requires an LV2 feature Ingen does not provide (the list is taken from Ingen's source), has a port type Ingen does not handle, or has no audio, control or MIDI connector. An instrument played by MIDI notes, or a module that works on MIDI alone (the x42 MIDI filters), is marked untested until checked with a MIDI controller (P12.5). An atom port is written `"midi"` only when it declares `atom:supports midi:MidiEvent`, and `"atom"` otherwise (a file or parameter input, never a connector). This is format version 2 (20 October 2026); a version 1 file called every atom port `"midi"`, so the reader takes them all as `"atom"`, and MIDI connectors appear once the catalogue is exported again. Each control becomes continuous, whole-number, on/off or a choice from its LV2 properties; ranges and defaults are written as the plugin's metadata states them; units are resolved to symbols. Controls marked as not for interfaces are left to the plugin. Categories come from the LV2 plugin class.

**What the overlay may do.** Rename a module, strip a maker's prefix, set its category, colour or description, and restrict its availability. It cannot offer a module the export withheld: only the installed metadata can say a plugin is loadable.

**What the reader refuses.** The file is generated, so one invalid entry refuses the whole file and names the entry: a default outside its range, a repeated address or symbol, an unknown kind, a newer format version. A refused overlay leaves the catalogue as it was.

When the interface drives a real engine it offers exactly the exported modules, and the browser says "Exported from the engine's installed plugins". The simulation offers its seven demo modules followed by the exported ones, which replace the built-in reference entries for the same plugins; their controls and connectors work, but nothing is heard. Without a `catalogue.json` the simulation keeps its built-in list. A control the catalogue leaves out is not shown, even though the engine has it.

## Run it

Inside Debian (WSL), once:

```bash
bash tools/ingen-spike/02-plugins.sh     # Mutable-derived plugins and MDA effects
bash tools/ingen-spike/08-catalogue.sh   # export, load everything, measure
```

The second command rewrites `config/catalogue/wsl-debian.json`. The Windows build copies it beside the program as `catalogue.json`, with the overlay as `catalogue-overlay.json`, and `--engine` loads both. `--catalogue file.json` names another file.

```powershell
& '.\build\Debug\peepo.exe' --engine tcp://127.0.0.1:16180
```

The exporter alone: `LV2_PATH=/usr/lib/lv2 peepo_catalogue_export -o catalogue.json`, with `--exclude` to leave plugins out by part of their address and `--sample-rate` for the few controls whose range depends on it.

## Limits and open points

- **x86-64 only.** The Mutable-derived plugins were built with the upstream makefile's own flags for this PC. The Pi build (P6.1) still has to be done and its revisions recorded there; the catalogue then has to be exported on the Pi.
- **Offered is not tested.** An offered module loads, matches its metadata and, for the five measured, passes sound. None has been judged by ear, measured for load, or checked for saved state.
- **Upstream defects met.** The makefile's rule for `polyrings.ttl` ends in a stray command and fails; the build script works around it without changing the source. Upstream names the Rings module "Random CV Sampler", the same as Marbles; the overlay calls it "Resonator".
- **The browser is long.** Since 14 October 2026 the export offers only the selected inventory (`--only config/catalogue/inventory.txt`), but that is still about 180 modules. Categories keep it navigable; filtering belongs to P9.5.
- **Per-control presentation.** Since 14 October 2026 the overlay can rename, reorder, hide and retype individual controls and choose their defaults; Clouds uses it ([Clouds](20-clouds.md)). Other modules still show their controls as exported.
- **A narrower range.** Since 27 October 2026 a control's rule may give a `"maximum"` inside the plugin's own range, never outside it. It is for a plugin that wraps its values round: Flow Envelope (Tides) declares Range and Ramp as 0 to 3 and Outputs as 0 to 4, but plays three, three and four values, so the top one repeats the first. Cut to what it plays, each gets `"choices"`. A saved value above the new maximum is clamped when the patch loads.
- **File parameters.** Plugins that take a file through an LV2 patch property, such as a NAM model, are exported with their ordinary controls only.
- **Added to the WSL Debian:** `mda-lv2`, `gettext-base`, and the plugin build under `/opt/peepo`.
