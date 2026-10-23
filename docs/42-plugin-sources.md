# Plugin sources: forks and source cache

**23 October 2026. Forks and the Poly NAM mirror are in `peepo-sources`, the build scripts point at them, and every pinned source is cached in `C:\peepo-sources`.**

Peepo builds every plugin from someone's Git repository at a pinned commit, so a deleted or private repository would break the next fresh build or package. The Pi keeps working, because it only runs compiled bundles. The plan has two parts:

1. **Fork** the 22 third-party GitHub repositories Peepo builds into the `peepo-sources` organisation, as Poly Effects does for Beebo.
2. **Cache** every repository and submodule on this PC as Git bundles, plus a tarball of every pinned commit, including the GitLab repositories, the 43 GxPlugins and the Patchstorage downloads, which forks can't cover. A build can be switched to read only the cache.

## Status, 23 October 2026

The GitHub organisation [peepo-sources](https://github.com/peepo-sources) holds forks of 21 repositories, created with every branch. Every commit Peepo pins was confirmed present in its fork by a partial clone, including Ingen's base `13a045d0` and its cherry-picked upstream commit `8dae7404`. The three `moddevices` repositories were forked from their current `mod-audio` addresses.

`01-build.sh` (Ingen), `02-plugins.sh` (Lenticular), `12-nam.sh` (current NAM) and `14-more-plugins.sh` (Poly repositories, lvtk and seven third-party plugins) now clone from `peepo-sources`. Existing local checkouts keep their old `origin`; their pinned commits are already local.

GitHub allows one fork per repository family in an organisation, and both NAM versions are one family. The fork went to `mikeoliphant/neural-amp-modeler-lv2`, which is maintained. Poly's version is the plain repository `peepo-sources/neural-amp-modeler-lv2-poly`, a push of all its branches and tags; its pinned commit `a3a6520e` is present. Unlike a fork, it has no "Sync fork" button: update it by pushing from `polyeffects` again.

## Source cache

`tools/source-cache.py` (run as root in WSL) keeps everything Peepo builds in `C:\peepo-sources` (`/mnt/c/peepo-sources`), outside WSL. It reads the pins from `01-build.sh`, `02-plugins.sh`, `12-nam.sh`, `14-more-plugins.sh` and `config/catalogue/gxplugins.json`, so it follows every pin change; it finds each pinned commit's submodules and caches those too.

| Command | What it does |
| --- | --- |
| `python3 tools/source-cache.py save` | Updates the working mirrors in `/var/cache/peepo-git`, then writes the cache. Run after moving any pin. |
| `python3 tools/source-cache.py verify` | Checks every file against `SHA256SUMS` and every bundle with `git bundle verify`. |
| `python3 tools/source-cache.py offline ROOT` | Restores the bundles into `ROOT/var/cache/peepo-git` and adds a marked block to `ROOT/etc/gitconfig` that sends every `https://` (and `git@github.com:`) clone there. The build scripts run unchanged. `ROOT` is `/opt/peepo-arm64` for the Pi build, `/` for the WSL build. |
| `python3 tools/source-cache.py online ROOT` | Removes that block. |

The cache holds:

- `git/<host>/<path>.bundle`: each whole repository, every branch and tag (94 on 23 October 2026);
- `tarballs/<name>-<commit>.tar.gz`: each pinned tree with its submodules, the source to hand over with a package;
- `patchstorage/`: a copy of `build/scratch/expanded-lv2-cache`, the Patchstorage downloads;
- `manifest.json` (every pin, its submodules and files) and `SHA256SUMS`.

First save, 23 October 2026: 76 pins, 94 repositories, 407 files, 0.72 GiB. The mirror update never prunes, so history deleted upstream stays in the cache.

**Two commits had already vanished upstream.** Stone Phaser's DPF pins `DISTRHO/pugl` at `0fdc1905`, and Lenticular's `eurorack` submodule pins `AndrewBelt/eurorack` at `99432f2b`; neither repository has those commits any more, so a fresh `submodule update` would fail today. Both were still in this PC's build checkouts and are kept in the cache as `refs/peepo/rescued/<commit>`.

Eureka's repository is private, so its mirror was seeded from a local bundle of `4b52532`; the save warns that it cannot update it.

**Offline test, 23 October 2026.** With the arm64 root switched offline, fresh clones in it succeeded from the cache alone: Lenticular with `eurorack` and its nested `stmlib`, Ingen's base plus the cherry-pick, lilv's GitLab tag at depth 1, NAM with recursive submodules, `GxSD1.lv2` by its `brummer10` URL, and Stone Phaser including the rescued `pugl`. `online` then left `/opt/peepo-arm64/etc/gitconfig` clean.

## How Peepo got its plugins before the forks

Every plugin is cloned from its repository and checked out at an exact commit. Nothing updates on its own.

- **Own plugins** (Aura, Chamber, Discipline, Drizzle, Eureka, Loo-pi, MIDI to CV, MIDI PC) are pinned in `tools/ingen-spike/14-more-plugins.sh` to commits in `github.com/ABustos2606`.
- **Third-party plugins** are pinned to their authors' repositories in the same script, in `21-gxplugins.sh` (43 GxPlugins), `12-nam.sh` (NAM) and `01-build.sh` (the Ingen engine and its libraries).
- **Patchstorage plugins** (Dragonfly and extra Gx) are prebuilt downloads from patchstorage.com, kept in `build/scratch/expanded-lv2-cache`.
- `fetch()` in `14-more-plugins.sh` reuses an existing local checkout and only clones when there is none. A failed `git fetch` is ignored if the pinned commit is already local.
- `tools/pi/check-prefix.py` refuses a package when any own plugin's bundle is missing.

Eureka showed the risk: it was installed straight onto the Pi in its Milestone 7 and was never in the arm64 build root. It is being added there at commit `4b52532` ([Eureka integration](41-eureka-integration.md)).

## If a repository disappears or goes private

Nothing breaks on the Pi. The damage shows at the next build that needs a fresh clone.

| Situation | Result |
| --- | --- |
| Peepo on the Pi | Keeps working. It runs bundles in `/opt/peepo/lib/lv2` and never contacts GitHub. |
| Rebuild on this PC with an existing checkout | Usually works: the pinned commit is already in the local clone. |
| New PC, rebuilt WSL, deleted source folder, newly added plugin | The clone fails; the build records the plugin as `FAILED clone`. |
| Packaging | Stops: `check-prefix.py` requires every own plugin and every selected Gx/Patchstorage bundle. |
| Handing someone a package | Most plugins are GPL, so the recipient is owed the source. Without upstream, the local copy is the only source available. |

This already happens: the three `moddevices` repositories have moved to the `mod-audio` organisation, and two repositories Peepo builds are archived.

## Why this route, and how Beebo does it

The own-repository rule was set on 16 October 2026 with Discipline ([pedal-inspired plugins](26-pedal-inspired-plugins.md)). Each plugin lives in its own repository and Peepo builds it at a pinned commit:

- **Standalone testing.** A plugin can be tested in any LV2 host, without Ingen or the Pi.
- **Separate licences.** Peepo is GPL-3.0 and the own plugins are GPL-2.0-or-later. Third-party code adds MIT, AGPL (Ingen), BSL, ISC and some with no licence stated.
- **A small Peepo.** Peepo holds only the pin, the catalogue entry and the patches.

One repository with everything vendored would remove the GitHub dependency, but it would be large, mix many licences in one tree and turn updates into copying code by hand.

**Poly Effects (Beebo)** forks the upstream projects they ship into their own GitHub organisation and builds from the forks. `polyeffects` holds forks of Ingen (David Robillard), convoLV2 and tuna.lv2 (x42), avwlv2 (AMS), ArableInstruments (as `lenticular_lv2`) and the NAM LV2 plugin, next to their own PolyLV2, `poly_filters` and `digit_ui`. How they assemble the device image is not public.

Peepo's `reference/` submodules don't protect builds: they point at upstream URLs and only keep a copy on this PC ([reference sources](08-reference-sources.md)).

## Forks and upstream updates

A fork doesn't update itself, but Peepo doesn't follow upstream today either: every pin is one exact commit, moved by hand. With forks, an update takes one extra step:

1. Sync the fork: "Sync fork" on GitHub, or `gh repo sync ABustos2606/<name>`.
2. Move the pin to the new commit, as now.

Syncing is a natural point to review what changed. If upstream pushes something broken or rewrites history, the fork still holds the version that worked.

**Alternative:** keep the upstream URLs as primary and use the forks or the cache only as a fallback when the original can't be reached. Updates then work exactly as today.

## Repositories to fork

The 22 third-party GitHub repositories the build scripts clone. Licences are as GitHub reported them on 23 October 2026. The Poly repositories and Ingen come first: Peepo can't run without them.

| Repository | Licence | Used for | Notes |
| --- | --- | --- | --- |
| [polyeffects/ingen](https://github.com/polyeffects/ingen) | AGPL-3.0 | The audio engine | Fork of drobilla/ingen |
| [polyeffects/lenticular_lv2](https://github.com/polyeffects/lenticular_lv2) | GPL-2.0 | Clouds, Warps, Rings, Plaits and other Mutable modules | Has submodules |
| [polyeffects/PolyLV2](https://github.com/polyeffects/PolyLV2) | GPL-2.0 | Poly utilities and effects | |
| [polyeffects/poly_filters](https://github.com/polyeffects/poly_filters) | none stated | Poly filters | |
| [polyeffects/polyconvo.lv2](https://github.com/polyeffects/polyconvo.lv2) | GPL-2.0 | Convolution | |
| [polyeffects/convoLV2](https://github.com/polyeffects/convoLV2) | GPL-2.0 | Convolution | Fork of x42/convoLV2 |
| [polyeffects/tuna.lv2](https://github.com/polyeffects/tuna.lv2) | GPL-2.0 | Tuner | Fork of x42/tuna.lv2 |
| [polyeffects/cv_to_cc.lv2](https://github.com/polyeffects/cv_to_cc.lv2) | none stated | CV to MIDI CC | |
| [polyeffects/digit_carla](https://github.com/polyeffects/digit_carla) | GPL-3.0 | Poly plugins from Digit | Archived |
| [polyeffects/PolyMclkIn.lv2](https://github.com/polyeffects/PolyMclkIn.lv2) | none stated | MIDI clock in | |
| [polyeffects/avwlv2](https://github.com/polyeffects/avwlv2) | GPL-2.0 | AMS modules, including the LFO | Fork of mfisher31/avwlv2 |
| [polyeffects/neural-amp-modeler-lv2](https://github.com/polyeffects/neural-amp-modeler-lv2) | GPL-3.0 | NAM, Poly's version | Has submodules |
| [mikeoliphant/neural-amp-modeler-lv2](https://github.com/mikeoliphant/neural-amp-modeler-lv2) | GPL-3.0 | NAM, current version | Has submodules |
| [moddevices/tap-lv2](https://github.com/moddevices/tap-lv2) | none stated | TAP effects | Moved to `mod-audio`; fork from there |
| [moddevices/mod-cv-plugins](https://github.com/moddevices/mod-cv-plugins) | none stated | MOD CV utilities | Moved to `mod-audio` |
| [moddevices/guitarix](https://github.com/moddevices/guitarix) | none stated | Source for one Patchstorage plugin | Moved to `mod-audio` |
| [jpcima/stone-phaser](https://github.com/jpcima/stone-phaser) | BSL-1.0 | Stone Phaser | DPF submodule |
| [SpotlightKid/ykchorus](https://github.com/SpotlightKid/ykchorus) | unclear | YK Chorus | DPF submodule |
| [ssj71/infamousPlugins](https://github.com/ssj71/infamousPlugins) | GPL-2.0 | Infamous plugins | |
| [blablack/midimsg-lv2](https://github.com/blablack/midimsg-lv2) | GPL-3.0 | MIDI message utilities | Archived |
| [x42/mclk.lv2](https://github.com/x42/mclk.lv2) | GPL-2.0 | MIDI clock generator | |
| [lvtk/lvtk](https://github.com/lvtk/lvtk) | ISC | Build library for other plugins | |

Repositories with no licence stated can still be forked. Shipping their binaries is a separate open question that predates this plan.

## What forking can't cover

| Source | Why a fork doesn't help | Cover with |
| --- | --- | --- |
| Submodules of `lenticular_lv2`, NAM, stone-phaser, ykchorus (Mutable eurorack code, NAM DSP libraries, DPF) | A fork still points its submodules at the original authors | Tarballs that include submodules |
| GitLab: `drobilla/raul`, `drobilla/zix`, `lv2/lilv` (engine build) | GitHub can't fork from GitLab | Tarballs, or import as new GitHub repositories |
| 43 GxPlugins (`brummer10/Gx*.lv2`) | 43 forks is tedious | `21-gxplugins.sh` already rebuilds from a saved source-archive set |
| Patchstorage plugins (Dragonfly, extra Gx) | Prebuilt downloads, not Git | Back up `build/scratch/expanded-lv2-cache` |

## Plan

Forking publishes under the user's account, so it is the user's step.

- [x] Decide: forks as primary pins (23 October 2026)
- [x] Fork 21 repositories into `peepo-sources` (the three `moddevices` ones from `mod-audio`)
- [x] Push Poly's NAM into `peepo-sources/neural-amp-modeler-lv2-poly` and point `12-nam.sh` at it
- [x] Point `01-build.sh`, `02-plugins.sh`, `12-nam.sh` and `14-more-plugins.sh` at the forks
- [ ] Rebuild one Poly plugin from a fresh clone of its fork to prove the new URLs
- [x] Write the cache script, `tools/source-cache.py`, into `C:\peepo-sources` (bundles plus a tarball per pinned commit)
- [x] Let builds read the cache: `source-cache.py offline ROOT` redirects every clone in that root, instead of a per-script fallback
- [x] Include the GitLab repositories, all submodules, the GxPlugins and the Patchstorage downloads in the cache
- [ ] Back up the cache folder off this PC
- [x] Test: fresh clones of the hardest cases in the arm64 root from the cache alone (23 October 2026)
- [ ] Run `save` again whenever a pin moves
