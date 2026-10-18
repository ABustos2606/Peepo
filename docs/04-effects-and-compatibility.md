# Effects and plugin compatibility

## Desired collection — agreed

Include Poly's published Mutable Instruments-derived modules and extend the new interface's module catalogue with compatible effects already available through MODEP.

LV2 provides a shared plugin format. It does not guarantee that every MODEP bundle runs in Ingen or appears correctly in the new interface. Plugins have to pass three separate checks: binary/dependency compatibility, audio-host compatibility, and catalogue/editor compatibility in the new interface.

## Mutable Instruments-derived families — observed

**Update, 13 October 2026:** all seven libraries below were built for x86-64 at the pinned commit and every one of their 21 plugins loads in Ingen with ports and controls matching its metadata; Clouds and Warps passed a measured signal. Clouds, Warps and Rings are silent until their own `enabled` control is set. See [catalogue export](16-catalogue-export.md). Nothing has been built for the Pi or judged by ear.

**Update, 14 October 2026:** Clouds' seven modes were checked with test signals and given a per-control overlay. Beat repeat ignores most of its controls in this port, and In gain does nothing in any mode. See [Clouds](20-clouds.md).

Poly's `lenticular_lv2` Makefile defines these processing libraries:

| Original family | Library | Examples of Beebo module mappings in the inspected source |
| --- | --- | --- |
| Clouds / Parasites variants | `polyclouds` | `granular`, `granular_looping`, `time_stretch`, `pitch_verb`, `resonestor` |
| Warps / Parasites variants | `polywarps` | `meta_modulation`, `wavefolder`, `bitmangle`, `twist_delay`, `doppler_panner` |
| Plaits | `polyplaits` | `macro_osc` |
| Rings | `polyrings` | `multi_resonator` |
| Tides | `polytides` | `looping_envelope` |
| Marbles | `polymarbles` | `chaos_controller` |
| Grids | `polygrids` | `drum_patterns` |

These are existing source/build targets and module definitions, **not verified Pi binaries**. Availability of every firmware mode and equivalence to original hardware require individual checks. Clouds is a granular processor; this collection also contains oscillators, resonators, modulation sources, and rhythm generators, rather than only filters.

**Observed in the published `.ttl` metadata (13 October 2026):** the Clouds-derived plugins have stereo audio in/out as four mono ports plus 12 CV inputs; Warps-derived plugins have two mono audio inputs and two outputs; Rings has one input and two outputs; Plaits has two audio outputs and no audio input. Grids, Marbles and Tides have **no audio ports at all**: they are pure control sources. Since control (CV) cables were added on 13 October 2026 (D16 revised) all seven families can be patched, and Plaits can be driven instead of droning at its knob settings. The simulator catalogue is generated from this metadata; see [UI development](11-windows-ui-foundation.md).

The source references Eurorack and Parasites dependencies, including Git submodules. Required submodule revisions must be captured in the eventual build manifest. Embedded AVR/STM bootloader sources are not automatically needed for Linux DSP builds; determine dependencies from the selected targets.

## Other Beebo sources

`PolyLV2` contains modular utilities and effect directories with individual Makefiles. It does not have a top-level Makefile in the inspected checkout. `poly_filters` has a separate build for Poly filter algorithms. Beebo also references plugins from other developers; even the broader public Poly reference collection does not necessarily contain every third-party effect listed in the UI. See [Reference sources](08-reference-sources.md) for the downloaded inventory and coverage limits.

The inspected Beebo UI catalogue contains **157 browser entries referring to 153 distinct plugin URIs**. It is a useful reference inventory of what a modular pedal can offer, not proof that those plugins are present, compatible, or available for simultaneous use. The new interface builds its own catalogue from LV2 metadata (see [Architecture](02-architecture.md#module-catalogue)).

## NAM and cabinet IR candidates — proposed

Casette demonstrates a source integration of NeuralAudio with a custom JACK effects chain. It is not an LV2 host and does not establish compatibility with Poly's Ingen. The reference is downloaded unchanged; see [Casette and NAM evaluation](09-casette-and-nam.md) for code findings and limitations.

The downloaded Poly `neural-amp-modeler-lv2` fork at `a3a6520ea519513a6ca1e82d350d47e3c7891732` uses NAM Core. Current upstream [neural-amp-modeler-lv2](https://github.com/mikeoliphant/neural-amp-modeler-lv2) uses [NeuralAudio](https://github.com/mikeoliphant/NeuralAudio), supports A1/A2 models, and exposes model-path and quality controls. Those current upstream projects were inspected online; they have not been downloaded as additional repositories or selected as our final build set.

Compare the older fork with a compatible current implementation using recorded plugin/runtime commits, compiler versions/options, model files, quality settings, and identical audio conditions. Optimized standard architectures and fallback implementations can have different costs. A1 Standard/Lite/Feather/Nano and A2 Lite/Full are comparison categories, not promises of equal compatibility or fixed resource use.

For each candidate, verify model-path (`atom:Path`/patch) handling, LV2 worker/state support, save/restore, missing-model recovery, calibration, quality control, and block-size changes. Match the model's sample rate for the baseline; generally use 48 kHz for a 48 kHz capture. Oversampling has additional CPU cost and is outside the initial comparison. Use a cabinet IR after amp-only models and avoid automatically adding a second cabinet to a full-rig capture. [Upstream plugin documentation](https://github.com/mikeoliphant/neural-amp-modeler-lv2/blob/main/README.md)

**Update, 14 October 2026:** both candidates were built and checked in Ingen on the PC. The current NeuralAudio-based plugin is selected: it loads A1 and A2 models, switches quality safely, and saves its model inside the patch bundle. The Poly fork cannot load A2 models. See [NAM in Ingen](21-nam.md). No NAM candidate is supported yet. One mono NAM plus a cabinet IR is the first test; NAM plus Clouds, delay/reverb, and multiple models enter the catalogue only with measured patch budgets and stability results.

## Inventory and records — 14 October 2026

**Current inventory, 18 October 2026:** the [complete module list](33-module-list.md)
contains all 291 selected identities, including all 43 GxPlugins. It shows the
current Pi availability and touchscreen names. The counts below describe the
original 14 October selection.

The selected inventory is every plugin identity in Beebo's module map and Poly's public plugin sources, plus the MDA and LV2 example plugins used for development: 249 entries, listed in `config/catalogue/inventory.txt`. The exporter offers only those, so installing a package no longer puts its other plugins in front of the player.

Every entry has a compatibility record following the workflow below, on the PC. The results are in [the compatibility matrix](evidence/compatibility-matrix.md) and explained in [the plugin catalogue](22-plugin-catalogue.md). "Essential MODEP effects" is still an open product choice (question 6 of the [decisions](07-source-audit-and-decisions.md)); the Beebo map stands in for it until one is made.

## Compatibility workflow

For each plugin:

1. Record its URI, bundle location, source/version, and licence notices.
2. Check the binary architecture and dynamic library dependencies against the actual Pi OS.
3. Inspect required LV2 features, extensions, and audio/control/CV/event port types.
4. Load it in the selected Ingen build and verify representative audio or control behaviour.
5. Confirm the generic editor works, then add the catalogue overlay entry: display name, group, parameter pages, ranges, units, discrete values, connection types, and any custom editor.
6. Verify saving/restoring its state and mappings, including any external files.
7. Test it on the target display and record resource use at the chosen audio settings.

Classify results as **supported**, **needs adaptation**, **unavailable**, or **not tested**. Do not advertise untested plugins as supported.

## Why an LV2 effect may need extra work

- A 32-bit ARM bundle cannot be assumed to load into a 64-bit host. Matching architecture still does not establish matching library versions.
- Host support for optional/required features can differ from MODEP's MOD host.
- LV2 control parameters and CV ports are distinct. A parameter does not automatically accept an arbitrary audio-rate modulation cable.
- Plugins using patch messages, external sample/IR/model files, or custom state need appropriate host and interface handling.
- Plugin-specific graphical interfaces may not be useful on the small screen. A generic module editor should use plugin metadata and provide appropriate controls.
- Plugin state can require more than restoring numeric control values.

## ARM build observations

The inspected `lenticular_lv2` and `poly_filters` Makefiles default to x86 SSE compiler options. Those defaults must be overridden or adapted for the Pi. Do not simply run their default build and expect ARM binaries.

The `lenticular_lv2` Makefile also contains a suspicious empty quoted command in the Rings metadata target. Confirm and address this in the implementation phase. Both are now built: the SSE flags are replaced by `-mcpu=cortex-a76` for the Pi, and the Rings defect is bypassed without changing the source. Other repositories' defects are listed in [the plugin catalogue](22-plugin-catalogue.md).

## Catalogue record — proposed

Each tested entry should record its URI, source revision, installed binary, required features, supported ports, module editor, control mappings, save/restore result, audio settings, resource measurements, and known limitations.

Preserve source and attribution information for upstream components. A distribution or licence assessment would be a separate task; this documentation does not claim permission for every possible distribution arrangement.

## Acceptance of “all effects”

The product ambition is the full compatible collection. Completion should mean that the agreed inventory has a recorded result for every entry and that supported entries pass validation. It must not mean that every installed LV2 bundle is automatically marked usable or that all effects can run in one patch on 2 GB RAM.
