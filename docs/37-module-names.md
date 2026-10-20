# Module names

**Decided 19 October 2026.** Peepo's ports of Emilie Gillet's eurorack modules (built by Poly's `lenticular_lv2`) carry Peepo's own names. The original module names appear only in credits, in source paths and in dated engineering records.

## Why

The upstream repository's guidelines for derivative works (`reference/upstream/lenticular_lv2/eurorack/README.md`):

- "Mutable Instruments is a registered trademark" and the name "should not be used on any of the derivative works". This is a requirement.
- They "do not recommend" keeping the original module name, for example a Clouds adaptation called "Foobar Modular - Particle Generator". This is a recommendation, and Peepo follows it.

## Names

Each family has one invented word, followed by the mode where a family has several modules.

| Upstream module | Peepo family | Modules shown on the touchscreen |
| --- | --- | --- |
| Clouds (Parasites firmware) | **Mist** | Mist, Mist Stretch, Mist Looper, Mist Spectral, Mist Oliverb, Mist Resonestor, Mist Beat Repeat |
| Warps (Parasites firmware) | **Twist** | Twist Meta, Twist Wavefolder, Twist Chebyshev, Twist Frequency Shifter, Twist Bitmangler, Twist Comparator, Twist Vocoder, Twist Delay, Twist Binaural Doppler Panner |
| Plaits | **Weave** | Weave Oscillator |
| Rings | **Bell** | Bell Resonator |
| Marbles | **Dice** | Dice Random Sampler |
| Grids | **Lattice** | Lattice Drum Sequencer |
| Tides | **Flow** | Flow Envelope |

Peepo's own ports, each in its own repository like Discipline ([roadmap](38-eurorack-ports-roadmap.md)):

| Upstream module | Peepo plugin | Repository |
| --- | --- | --- |
| Beads | **Drizzle** | `https://github.com/ABustos2606/drizzle` |
| Elements | **Chamber** | `https://github.com/ABustos2606/chamber` |
| Stages | **Slope** | `https://github.com/ABustos2606/slope` |
| Frames | **Reel** | `https://github.com/ABustos2606/reel` |

Oliverb and Resonestor are the Parasites firmware's own mode names, not Mutable Instruments module names. They keep them inside the Mist family. Braids is itself a Mutable Instruments module, so it is not used for Plaits.

The names live in `config/catalogue/overlay.json`, which both the Pi and the Windows simulator apply. `docs/33-module-list.md` is regenerated from it with `tools/document-catalogue.py`.

## What keeps the old names

- **LV2 URIs** such as `http://polyeffects.com/lv2/polyclouds#granular`. Saved patches refer to modules by URI, so changing them would break every patch. They are identifiers, never shown on the touchscreen.
- **`src/core/reference_catalogue.cpp`**, a generated reading of Poly's unchanged Turtle files, with Poly's names. It is only shown in the simulator when no catalogue file is present, and `catalogue_files` checks it against the live export.
- **Source paths, build scripts and dated records** (`docs/20-clouds.md`, `tools/ingen-spike/11-clouds.*`, `docs/evidence/`). They describe the upstream code as it was measured.
- **Saved patch names** such as the Pi's Clouds guitar patch, which are user data.

## Credits

The DSP in these modules is Emilie Gillet's, released under the MIT license (STM32 modules), and the Clouds and Warps modes come from Matthias Puech's Parasites firmware. The LV2 wrapper is Poly Effects' `lenticular_lv2`. The MIT license requires keeping the copyright notice with the code. The license entries are in [third-party notices](../THIRD_PARTY_NOTICES.md). Module descriptions on the touchscreen say what a module does, not who made it. A credits page on the device is still to be added.
