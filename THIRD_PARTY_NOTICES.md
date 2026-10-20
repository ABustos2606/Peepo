# Licensing and third-party notices

## Original Peepo files

Original code and documentation authored for Peepo are licensed under the GNU General Public License, version 3 or (at your option) any later version: **SPDX-License-Identifier: GPL-3.0-or-later**. This selection was requested by the project owner on 13 October 2026. The complete license text is in [LICENSE](LICENSE).

Peepo is distributed in the hope that it will be useful, but without any warranty, including the implied warranties of merchantability or fitness for a particular purpose. See the license for its full terms.

Add `SPDX-License-Identifier: GPL-3.0-or-later` and an accurate author/copyright notice to new project source files. Preserve existing notices in adapted third-party files. Authors retain their copyrights; the project license does not transfer ownership.

## Upstream references and dependencies

The project license does not replace the licenses of repositories, nested dependencies, archives or assets under `reference/`. These sources are retained at the revisions recorded in [the manifest](reference/manifest.json), with their existing notices. The following are inspected examples, not a complete license inventory of every file or a conclusion about every possible build combination.

| Component | Inspected evidence | Treatment |
| --- | --- | --- |
| Poly Ingen | [AGPLv3 text](reference/upstream/ingen/COPYING); [public header](reference/upstream/ingen/include/ingen/ingen.h) explicitly permits version 3 or later | Retain AGPL-3.0-or-later for Ingen and modifications to it. Its notices identify David Robillard and other contributors. |
| Poly `digit_ui` | [README](reference/upstream/digit_ui/README.md) says code without a notice is GPL, without a version; [ingen.py](reference/upstream/digit_ui/ingen.py) has David Robillard's ISC-style notice | Beebo UI remains conceptual reference material. No code, QML or assets are copied into our new interface. File-specific notices take precedence. |
| Poly `PolyLV2` | [COPYING](reference/upstream/PolyLV2/COPYING) is GPLv2; [generated vibrato source](reference/upstream/PolyLV2/poly_vibrato_ext/vibrato_ext.cpp) also contains GPLv3-or-later, LGPLv3-or-later and Faust exceptions | Keep all file and section notices. Determine the actual license and exceptions of each selected module and dependency when integrating it. |
| Poly `lenticular_lv2` | [LICENSE.txt](reference/upstream/lenticular_lv2/LICENSE.txt) is GPLv2; [LICENSE-dist.txt](reference/upstream/lenticular_lv2/LICENSE-dist.txt) includes the MIT notice for Emilie Gillet's Mutable Instruments STM32F modules (the notice predates her current name) and further component notices | Preserve the wrapper, Mutable Instruments, Parasites and dependency notices separately; assess the selected module's complete source set. |
| Poly `digit_carla` (Delay) | [LICENSE](reference/upstream/digit_carla/LICENSE) is GPLv3; the generated [digit_delay.cpp](reference/upstream/digit_carla/LV2/digit_delay/digit_delay.cpp) also carries Albert Graef's LGPLv3-or-later Faust architecture notice with the Faust exception, and Faust library declarations | Built unchanged as Beebo's Delay since 16 October 2026 (`14-more-plugins.sh`). Keep the repository license and every file and section notice with the packaged plugin. |
| Poly NAM LV2 fork | [LICENCE.md](reference/upstream/neural-amp-modeler-lv2/LICENCE.md) contains GPLv3 text | Keep plugin and nested dependency notices; record the exact version grant from file notices for the selected implementation. Models have separate provenance. |
| Poly Serd | [COPYING](reference/upstream/serd/COPYING) contains David Robillard's ISC-style permission and disclaimer | Include this notice with copied or packaged Serd code. |
| Casette | No root `LICENSE`, `LICENCE` or `COPYING` file, or license notice in the README and C++ files inspected at the pinned revision | Technical reference only. Establish permission before copying implementation or distributing its bundled models/IRs; the Peepo license does not grant rights to these files. |

The Windows UI uses LVGL v9.5.0 at commit `85aa60d18b3d5e5588d7b247abf90198f07c8a63`, fetched from its official repository. Its Win32 display/input driver is part of the same source revision. [cmake/lvgl_windows_fix.cmake](cmake/lvgl_windows_fix.cmake) builds a corrected copy of `src/osal/lv_windows.c`, changing two cleanup calls to dereference the Windows thread handle pointer. The pinned checkout is unchanged and the generated copy remains MIT licensed. The supplied notice is preserved in [licenses/LVGL-MIT.txt](licenses/LVGL-MIT.txt) and copied beside the executable.

Built-in Montserrat font tables at sizes 14, 18, 20, 24 and 28 are supplied by this LVGL revision; their generated headers also identify Font Awesome 5 glyphs. The upstream [Montserrat OFL notice](licenses/Montserrat-OFL.txt) and [Font Awesome Free notice](licenses/Font-Awesome-Free.txt) are retained. Their notice sources are [Montserrat](https://github.com/JulietaUla/Montserrat/blob/master/OFL.txt) and [Font Awesome 5.15.4](https://github.com/FortAwesome/Font-Awesome/blob/5.15.4/LICENSE.txt); LVGL does not record the precise original font revision in those generated table headers. No original Beebo assets are used.

The interface also compiles its own font tables, generated with [lv_font_conv](https://github.com/lvgl/lv_font_conv) 1.5.3 into `src/ui/fonts/` (printable ASCII, 4 bits per pixel):

- **Montserrat Bold**, sizes 14, 18, 20, 24 and 48, from [JulietaUla/Montserrat](https://github.com/JulietaUla/Montserrat) `fonts/ttf/Montserrat-Bold.ttf`. Covered by the same [Montserrat OFL notice](licenses/Montserrat-OFL.txt).
- **JetBrains Mono Regular**, size 14, from [JetBrains/JetBrainsMono](https://github.com/JetBrains/JetBrainsMono) `fonts/ttf/JetBrainsMono-Regular.ttf`. Its [OFL notice](licenses/JetBrains-Mono-OFL.txt) is retained.

Both fonts were downloaded on 15 October 2026. The OFL allows bundling the fonts with software. The generated tables keep the original font names, and the fonts are not sold on their own.

Apply the same provenance/notice process to each selected LV2 plugin. Images, presets, NAM models and cabinet impulse responses need their own provenance and applicable terms.

## GxPlugins collection

Since 18 October 2026 Peepo builds all 43 plugins in brummer10's
[GxPlugins.lv2](https://github.com/brummer10/GxPlugins.lv2). Exact repository
URLs and commits are recorded in [the manifest](config/catalogue/gxplugins.json).
The four previously installed revisions are retained. No upstream source is
modified; the DSP-only `mod` target receives architecture flags through make.

The build packages the exact Git source archives (including all file-specific
notices), archive checksums, manifest and rebuild script in
`share/peepo/sources/gxplugins`, plus root notices in
`share/peepo/licenses/gxplugins`. Those directories travel with the Pi package.
Keep the grants in the actual sources and license files: some MOD metadata
declares ISC while the plugin implementation contains GPL notices. See
[the integration record](docs/32-gxplugins.md).

## Implementation and release requirements

- Keep original copyright, license and exception notices with reused source, including modifications. Identify changed files and retain their applicable licenses.
- Maintain a component inventory with source URL, revision, license/version grant, modifications and required notices for the actual build. A license text alone does not establish whether an upstream grant is version-only or permits later versions.
- Package the required license/notices and the corresponding source and build/install material required by each distributed component's terms. Record how recipients obtain that source for the exact shipped version.
- Preserve Ingen's AGPL terms. If GPLv3 and AGPLv3 code are combined into one work, section 13 addresses that combination and its network-interaction requirements; selecting GPL for our files does not remove the AGPL requirements.
- Review the actual dependencies and assets selected for a release during Phase 11. The current repository contains a Windows UI simulator and unchanged references; the complete instrument has not been implemented or validated.

Authoritative texts: [GNU GPLv3](https://www.gnu.org/licenses/gpl-3.0.html), [GNU AGPLv3](https://www.gnu.org/licenses/agpl-3.0.html), and [GNU guidance on applying licenses](https://www.gnu.org/licenses/gpl-howto.html).
