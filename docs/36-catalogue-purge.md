# Catalogue curation

**Started 19 October 2026; updated 20 November 2026.**

The expansion phase installed whole plugin collections, and Add grew from a
curated, Beebo-like selection to about 280 ready entries: 55 drives and 20 amps
alone. This file records how the catalogue is brought back to a curated
instrument: what was retired, what was merged, and how the rest is chosen.

## Direction

Peepo's strongest side is the experimental modular part (Discipline, Clouds,
the resonators, the Twist processors, patchable control) beside a dependable
conventional pedalboard. The conventional categories are where the excess is;
the modular part is kept.

- **Retire, do not just hide.** A module rejected in a session is retired like
  the first purge: out of the inventory, out of builds, its bundle moved to
  recovery storage. A saved patch that uses it needs the recovery bundle.
- **Merge before judging by ear.** Mono/stereo, CV and clock variants become one
  card ([families](#families-in-add)); no listening needed.
- **Choose conventional effects by ear, family by family**, level-matched, one
  or two kept per family ([drive audition](#drive-audition)). The shortlists in
  an outside review were guessed from names and are not used.
- **One rule for new modules:** a new module does something nothing in the
  catalogue does, or replaces one that is there.
- **The target is how fast a module is found, not a count.** Roughly 10 drives
  and 15 modulation effects; the rest can stay mostly as it is.

Order of work: purge of obvious duplicates (done), families (done), MIDI modules
offered (done), drive sessions (done), amp, modulation and delay sessions (done), then filter, dynamics, pitch, reverb and cabinet sessions, and utility by function; reverb is nearly done.

## First purge

**19 October 2026. Applied to the Pi and Windows editor selection.**

The user requested retirement of Gx Live looper, Ambience, MVerb, Gx Reverb
stereo, Gx Room simulator, Gx Zita reverb stereo and Dragonfly Early Reflections.
Gx Live looper goes because Peepo gets its own looper; Clouds Looper stays, as
a texture effect rather than a performance looper. The selected inventory falls
from 357 to **350 identities**, and the Pi export from 335 to **328**. Add offers
**272 ready identities**, down from 282: seven retirements and three previously
offered duplicate convolvers.

### IR processing

New patches use the existing Poly convolver implementations. Their identities,
file properties, IR folders and DSP are unchanged; only presentation changes.

| Family | Configuration | IR mapping |
| --- | --- | --- |
| Cabinet | Mono | One input/output; uses the first IR channel |
| Cabinet | Mono to Stereo | One input, two outputs; stereo IR provides two responses |
| Cabinet | Stereo | Two inputs/outputs; mono/stereo IRs process L/R separately; four-channel IR adds cross paths |
| IR Reverb | Mono | One input/output; uses the first IR channel |
| IR Reverb | Mono to Stereo | One input, two outputs; stereo IR provides two responses |
| IR Reverb | Stereo | Two inputs/outputs; mono/stereo IRs process L/R separately; four-channel IR adds cross paths |

With a mono IR, Mono to Stereo uses the same response for both outputs. Four-channel
responses are ordered LL, LR, RL, RR in this implementation. Cabinet retains
Poly's short-IR configuration; IR Reverb retains its longer-IR configuration.
In Add, each is one card with three pills ([families](#families-in-add)).

The three LV2 Convolution and three x42 IR Convolver identities are hidden in
Add. They remain installed with their file metadata for older patches; the
existing x42 worker limitation remains. Gx Cabinet and Ultra Cab remain as
separate, explicitly labelled **built-in** cabinet simulators.

### Selection and builds

`config/catalogue/retired.json` is the retirement policy. The inventory generator
filters it after collecting all sources, so Beebo reference maps cannot
reintroduce retired effects. The expansion manifest excludes five publications,
and MVerb is no longer built. Reused build prefixes move six retired standalone
bundles into recovery storage before exporting and packaging, including builds
using `--skip-engine`. Ambience shares Debian's MDA bundle with retained effects:
that bundle stays intact, while inventory selection excludes Ambience.

The Windows fixture excludes the seven identities. Hidden overlay rules also
keep them out of Add if an older export is used. Historical exports and test
records retain their original contents and counts.

### Deployment and validation

The Pi update moved six standalone bundles out of the active LV2 path and
retained metadata and a full snapshot of the current playing graph. It restarted
only the interface. The graph snapshots compare equal as RDF, including the
current controls and connections. JACK PID 672 and Ingen PID 41642 were unchanged.

- [Initial purge report](evidence/catalogue-purge-20261019.json)
- [Final presentation verification](evidence/catalogue-purge-verification-20261019.json)
- [Current Pi export](evidence/catalogue-pi-20261019-purged.json)
- [Complete module list](33-module-list.md)

Recovery copies of the six bundles and prior metadata are at
`/var/lib/peepo/catalogue-purge-hjvvby_3/` on the Pi. The final presentation refresh
has another metadata/graph backup at `/var/lib/peepo/catalogue-purge-4slrowc5/`.
Do not discard those copies while patches may still refer to retired effects.
Older patches using a physically retired standalone plugin require its recovery
bundle; no automatic substitution is performed.

Validation: the normal Windows build and all seven CTest suites passed;
inventory regeneration is deterministic; the fixture and Pi exports exclude
all seven retired identities; only the six Poly IR configurations remain
visible; legacy convolution definitions remain hidden; build pruning was
checked with a disposable prefix, repeated execution and an invalid path.
The convolver channel mapping was checked against its pinned source. No DSP
changed and no new guitar listening or CPU benchmark was performed.

## Audition retirements deployed

**20 November 2026. Applied to the Pi.**

`tools/pi/purge-catalogue.py` was written for the first purge and refused a
second one (it required all retired modules present or all absent). It now
removes whatever in `retired.json` is still installed, does nothing when that
is empty, and refuses before changing anything if the playing patch or a saved
patch under `/var/lib/peepo/patches` uses a module being removed.
`tools/prune-retired-bundles.py` moves a bundle named by several retired
plugins once.

Before deploying, all 42 saved patch files were searched for the 104 retired
identities, in full and prefixed form: none used one (the only drive in them is
Tubewarmth, kept).

The run removed **97 identities** (the first purge's seven were already gone):
68 standalone bundles moved to recovery storage, and 26 identities in shared
bundles (MDA, x42 MIDI filters, LV2 examples, Poly filters, AMS) were excluded
while their bundles stay. The Pi catalogue went from 351 to **254 modules**;
`/opt/peepo/lib/lv2` from 198 to 130 bundles; 237 entries ready in Add, nine
hidden. The six IR families stay. Only the interface restarted: the playing
patch compared equal as RDF before and after, and the JACK and engine PIDs were
unchanged. The installed catalogue and overlay load
(`peepo_effect_live show`).

Recovery: `/var/lib/peepo/catalogue-purge-sd9yxyow/` holds the 68 bundles, the
previous metadata and catalogue, and the graph snapshots. Keep it while older
patches might use a retired module.

The build root needs no direct change: `build.sh` prunes retired bundles from
the prefix and installs `retired.json` and the inventory from the repo, so the
next package matches the Pi.

The new boot logo (commit bbd8eae, `config/pi/splash/peepo/logo.png`) was
installed at the same time: copied over the installed theme, `peepo-splash` run
again (it rebuilt both initramfs images), and the logo inside
`initramfs_2712` checked by hash. The Pi was then rebooted to show it. The
next package carries the logo from the repo.

## Families in Add

**22 October 2026. Windows editor and tests; not yet deployed to the Pi.**

Plugins that do one job in several configurations are now one card in Add,
with a pill for each variant. Tapping the card picks the variant its pills
show, a pill picks another, and a second tap or Add adds it. The card
remembers the last variant picked; encoder 1 steps through the variants as
through cards. Nothing is removed: each variant keeps its identity, its name
on the canvas and its place in saved patches.

The `families` list in `config/catalogue/overlay.json` defines them: a name, a
card description and the variants in pill order, all of one category. A
variant that is not installed is left out; a plugin in two families, a repeated
label or mixed categories refuse the overlay.

| Family | Pills |
| --- | --- |
| Cabinet, IR Reverb | Mono, Mono > stereo, Stereo |
| Gx Amplifier, Gx JCM800 preamp, Gx Digital delay, Gx Duck delay, Downsampler, Compressor, Parametric EQ, Stone phaser, Beat Repeater, Beat Slicer, Granulator, Scope | Mono, Stereo |
| Chorus, Flanger, Harmonic tremolo, Vibrato | LFO, CV |
| Rotary speaker | Simple, Full |
| 16 Step Sequencer, 16 Step Note Sequencer | Trigger, BPM |
| LFO | Rate, Tempo |

Ring Modulator stays two cards: the CV one multiplies control signals, a
different job from the audio effect. With the Pi export of 22 October, the
281 ready entries become 257 cards.

Validation: the Windows build and all eight CTest suites passed, with overlay
tests for families and a UI check of a three-pill card (`85-browser-family`);
the overlay loads against the Pi export of 22 October.

## MIDI modules offered

**22 October 2026. Takes effect on the Pi with the next catalogue export.**

MIDI cables, MIDI IN and OUT, MIDI learn, clock and program change are built
([MIDI](39-midi.md)). The exporter still withheld every instrument and MIDI-only
module "until checked with a MIDI controller", and four overlay rules still said
MIDI routing was not implemented. Both holds are lifted
(`tools/catalogue-export/main.cpp`, `config/catalogue/overlay.json`): 32 x42
MIDI filters, Fifths, MIDI clock and the MDA instruments DX10, ePiano, JX10 and
Piano are offered. None has been played from a real controller on the Pi yet;
one that misbehaves is restricted on its own. The Windows fixture is an older
export and still withholds them.

## Drive audition

**Plan 22 October 2026; sessions held, results recorded 20 November 2026.**
**Retirement applied to the Windows editor and builds; not yet deployed to the Pi.**

The 55 Drive entries, grouped by the circuit each is based on, one listening
session per group. Same riff every session, through the same amp or NAM model;
level-match each module with its Output against a bypassed reference before
judging. Keep at most two per session; the rest are retired as above.

A **?** marks a module whose circuit its plugin does not state: a best guess
that moves if it sounds out of place. **Beebo** marks a plugin in Beebo's module
map (the `# Beebo:` entries of [the inventory](../config/catalogue/inventory.txt)),
with Beebo's name for it. Only two of the 55 are. Beebo's own clean Boost is
also a Drive entry but already hidden as a duplicate of Gain; Beebo's other
drive-like modules are outside Drive (Twist Bitmangler under Texture, the amps
under Amp).

### Gain staging

Not really drives: a quick pass, keep one or two, then consider moving them to Utility.

**Session 1: Boosts**

| Module | Based on | Beebo |
| --- | --- | --- |
| Gain | clean level, -90 to +24 dB | |
| Gx Booster | clean boost | |
| Micro Amp | MXR Micro Amp | |
| Sigmoid Booster | boost with soft saturation | Yes (saturator) |
| Gx Hogs Foot | bass booster | |
| Gx Mole | bass booster | |
| Gx HF Brightener | treble lift | |

**Session 2: Treble boosters**

| Module | Based on | Beebo |
| --- | --- | --- |
| Gx Rangemaster | Dallas Rangemaster | |
| Gx Screaming Bird | EHX Screaming Bird | |

### Overdrive

**Session 3: Tube Screamer types (mid hump)**

| Module | Based on | Beebo |
| --- | --- | --- |
| Gx Tube Screamer | Ibanez Tube Screamer | |
| SD-1 | Boss SD-1, the asymmetric cousin | |
| Eternity ? | Lovepedal Eternity; low compression | |

**Session 4: Low-gain and op-amp overdrives**

| Module | Based on | Beebo |
| --- | --- | --- |
| Tim Ray | Vemuram Jan Ray; transparent | |
| DOP 250 | DOD OD-250 (Distortion+ style) | |
| Overdrive | generic soft clipping | |
| Luna ? | not stated; "gnarly overdrive" | |
| Sloopy Blue ? | not stated | |

**Session 5: Tube overdrives** (the biggest; split it in two if needed)

| Module | Based on | Beebo |
| --- | --- | --- |
| Bottle Rocket | Mesa V1 Bottle Rocket | |
| Hot Box | Matchless Hot Box | |
| Club Drive | EF86 pentode | |
| Valve Caster | Valve Caster (12AU7, starved plate) | |
| Baja Tube Driver | Tube Driver | |
| Shaka Tube ? | ShakaTube | |
| Boob Tube ? | Boob Tube | |
| Tube Distortion | generic tube distortion | |
| Tubewarmth | triode warmth, gentle drive | Yes (warmth) |

### Distortion

**Session 6: Distortion**

| Module | Based on | Beebo |
| --- | --- | --- |
| Guvnor | Marshall The Guv'nor | |
| SD-2 Lead | Boss SD-2, Lead channel | |
| Saturator | Vox Satchurator | |
| Liquid Drive | modified Ross Distortion | |
| Gx Hornet ? | not stated | |

### Fuzz

**Session 7: Fuzz Face (two transistors)**

| Module | Based on | Beebo |
| --- | --- | --- |
| Gx Fuzz Face Fuller | Fuzz Face, Fuller modification | |
| Gx Fuzz Face JH-2 | Fuzz Face, JH-2 | |
| Sun Face | Analog Man Sun Face | |
| Axis Face | Axis Face, silicon | |

**Session 8: Tone Bender (three transistors) and Muff.** Gx Muff is the only Muff: it stays unless it sounds bad.

| Module | Based on | Beebo |
| --- | --- | --- |
| Gx ColorSound Tonebender | ColorSound Tonebender | |
| Suppa Tone Bender | Vox Supa Tonebender | |
| Knight Fuzz ? | Basic Audio Knight Fuzz | |
| Gx Muff | Big Muff | |

**Session 9: Octave fuzz.** Super Fuzz and Voodoo Fuzz are the same circuit family: keep one.

| Module | Based on | Beebo |
| --- | --- | --- |
| Super Fuzz | Univox Super-Fuzz | |
| Voodoo Fuzz | Voodoo Lab SuperFuzz | |
| Tone Machine | Foxx Tone Machine | |

**Session 10: Vintage and odd fuzz**

| Module | Based on | Beebo |
| --- | --- | --- |
| Maestro FZ-1B | Maestro FZ-1B | |
| Maestro FZ-1S | Maestro FZ-1S Super-Fuzz | |
| Heathkit | Heathkit TA-28 | |
| Hyperion | Devi Ever Hyperion | |
| Vintage Fuzz Master | Devi Ever Vintage Fuzz Master | |
| Gx Fuzz Master | fuzz with tone and output | |
| Gx Fuzz | generic fuzz | |

### Not guitar drives

**Session 11: Lo-fi and multiband.** Decide these by function rather than by ear. Bandisto and Gx Multiband distortion do the same job: keep one. Downsampler Mono and Stereo are already one card.

| Module | Based on | Beebo |
| --- | --- | --- |
| Degrade | reduced sample rate and resolution | |
| Dither | bit reduction with dither | |
| Downsampler - Mono | sample-rate reduction | |
| Downsampler - Stereo | sample-rate reduction | |
| Bandisto | three-band distortion | |
| Gx Multiband distortion | multiband distortion | |

### Results

| Session | Kept | Retired |
| --- | --- | --- |
| 1 Boosts | Gain, Micro Amp | Gx Booster, Sigmoid Booster, Gx Hogs Foot, Gx Mole, Gx HF Brightener |
| 2 Treble boosters | Gx Rangemaster, Gx Screaming Bird | |
| 3 Tube Screamer types | Gx Tube Screamer, SD-1 | Eternity |
| 4 Low-gain and op-amp | DOP 250 | Tim Ray, Overdrive, Luna, Sloopy Blue |
| 5 Tube overdrives | Tubewarmth, Bottle Rocket | Hot Box, Club Drive, Valve Caster, Baja Tube Driver, Shaka Tube, Boob Tube, Tube Distortion |
| 6 Distortion | Guvnor, SD-2 Lead | Saturator, Liquid Drive, Gx Hornet |
| 7 Fuzz Face | Gx Fuzz Face Fuller, Sun Face | Gx Fuzz Face JH-2, Axis Face |
| 8 Tone Bender and Muff | Gx ColorSound Tonebender, Gx Muff | Suppa Tone Bender, Knight Fuzz |
| 9 Octave fuzz | Super Fuzz | Voodoo Fuzz, Tone Machine |
| 10 Vintage and odd fuzz | | Maestro FZ-1B, Maestro FZ-1S, Heathkit, Hyperion, Vintage Fuzz Master, Gx Fuzz Master, Gx Fuzz |
| 11 Lo-fi and multiband | Degrade, Dither, Downsampler, Bandisto | Gx Multiband distortion |

The 34 are retired as in the [first purge](#first-purge):

- `config/catalogue/retired.json` lists them (41 entries with the first purge),
  so the inventory drops from 371 to **337 identities** and builds move 33
  standalone bundles to recovery storage. MDA Overdrive shares Debian's MDA
  bundle with kept effects: that bundle stays, and only its identity is excluded.
- The Patchstorage manifest drops nine publications that held only a retired
  drive (58 to 49), so they are no longer downloaded or built.
- The Windows fixture drops from 339 to 305 modules.
- Overlay rules keep them hidden if an older export still includes them.

Drive in Add goes from 55 entries to 20 cards (Downsampler is one card):
16 guitar drives and four lo-fi or multiband modules. The GxPlugins source
pins (`gxplugins.json`) are unchanged; the collection is built whole and
pruned.

Validation: inventory regeneration removed exactly the 34; the fixture is the
previous one minus the 34; pruning a disposable prefix moved all 39 retired
bundles and kept Gx Muff. Seven of the eight CTest suites pass, including
`catalogue`, which loads the overlay against the fixture. `ui_workflows` times
out after 120 s with and without this change, so the timeout was already there.

Still to do:

- **Pi:** `tools/pi/purge-catalogue.py` asserts a purge is all-or-nothing and
  was written for the first one, so it needs adjusting before it can deploy
  this second set. Any saved patch that uses a retired drive needs checking first.
- **Module list:** `tools/document-catalogue.py` asserts all 43 GxPlugins are
  installed and Ready, so [the module list](33-module-list.md) is not
  regenerated until that assertion and a new Pi export are updated.

## Amp audition

**Plan and sessions 20 November 2026. Retirement applied to the Windows editor
and builds; not yet deployed to the Pi.**

The 20 Amp entries (18 cards), grouped by what each one is, one listening
session per group. Same riff every session; level-match each module with its
Output against a bypassed reference before judging. Keep at most two per
session; the rest are retired like the drives.

Amps differ from drives in one way: most have no speaker. Every amp without
its own cabinet plays through the same Cabinet IR. Gx Amplifier, MDA Combo,
Bass amp (SVT) and VBass Pre Amp have their own cabinet or speaker switch:
judge them with it on and the IR off. Power amps (session 4) play after the
same preamp and the same IR.

A **?** marks a module whose original its plugin does not state. **Beebo**
marks a plugin in Beebo's module map, with Beebo's name for it.

**NAM is not auditioned.** It plays model files rather than one amp, so it
stays; which models to ship is a separate choice.

### Before the sessions: one merge

Gx Alembic preamp and Gx Studio preamp stereo are the same preamp with the
same five controls, the stereo one doubled per side. The plan was to merge them
into one family card first; both were retired in session 3, so the merge was
not needed.

### Sessions

**Session 1: Small single-ended combos.** The three Redeye modules are one
plugin with three voicings and the same eight controls: keep at most one.

| Module | Based on | Beebo |
| --- | --- | --- |
| Blue Amp | late 1950s Fender Princeton and Champ | |
| Epic | Epiphone Valve Junior | |
| Gx Redeye Chump ? | Fender Champ style | |
| Gx Redeye Vibro Chump ? | Fender Vibro Champ style, with vibrato | |
| Gx Redeye Big Chump ? | larger Champ style | |

**Session 2: Complete amps.** Each has its own tone stack and speaker or
amp models.

| Module | Based on | Beebo |
| --- | --- | --- |
| Gx Amplifier (Mono, Stereo) | Guitarix tube and tone stack models, own cabinet | |
| MDA Combo | generic amp and speaker models | |
| UVox 720k | Univox 720k keyboard amp, 1978, with reverb | |

**Session 3: Preamps.** Through the same Cabinet IR, no power amp.

| Module | Based on | Beebo |
| --- | --- | --- |
| Gx JCM800 preamp (Mono, Stereo) | Marshall JCM800 preamp | |
| Gx Alembic preamp (Mono, Stereo) | Alembic F-2B studio preamp | |
| Vox MKII | late 1960s Vox solid-state preamp, with vibe and reverb | |

**Session 4: Power amps.** After the same preamp, judged on feel and
compression as much as tone.

| Module | Based on | Beebo |
| --- | --- | --- |
| Plexi | Marshall Plexi power amp | |
| Power amp (Cream) ? | not stated | Yes (power_amp_cream) |
| Power amp (Supersonic) ? | not stated; vintage American | Yes (power_amp_super) |

**Session 5: Bass amps.** Decide by function as much as by ear: keep one
unless Peepo is played with a bass.

| Module | Based on | Beebo |
| --- | --- | --- |
| Bass amp (SVT) | Ampeg SVT | Yes (amp_bass) |
| VBass Pre Amp | Vox Venue Bass 100 preamp, 1984 | |

### Results

| Session | Kept | Retired |
| --- | --- | --- |
| 1 Small single-ended combos | Blue Amp | Epic, Gx Redeye Chump, Gx Redeye Vibro Chump, Gx Redeye Big Chump |
| 2 Complete amps | MDA Combo, UVox 720k | Gx Amplifier (Mono and Stereo) |
| 3 Preamps | Gx JCM800 preamp (Mono, Stereo) | Gx Alembic preamp, Gx Studio preamp stereo, Vox MKII |
| 4 Power amps | Plexi | Power amp (Cream), Power amp (Supersonic) |
| 5 Bass amps | Bass amp (SVT) | VBass Pre Amp |

12 identities are retired the same way as the drives:

- `retired.json` now has 53 entries, and the inventory drops from 337 to
  **325 identities**. The three Redeye amps share one bundle, so builds move
  10 bundles to recovery storage. All are standalone; none is shared with a
  kept module.
- The Patchstorage manifest drops five more publications (49 to 44): Gx
  Redeye, Gx Amplifier mono and stereo, and both studio preamps.
- The Gx Amplifier family is removed from the overlay; overlay rules keep the
  12 hidden if an older export still includes them.
- The Windows fixture drops from 305 to 293 modules.

Amp in Add goes from 18 cards to **7**: NAM, Blue Amp, MDA Combo, UVox 720k,
Gx JCM800 preamp, Plexi and Bass amp (SVT). Power amp (Cream) and Power amp
(Supersonic) were in Beebo's module map; the Beebo reference can no longer
reintroduce them, because the inventory generator filters retired identities.

`tools/prune-retired-bundles.py` failed when one bundle held several retired
plugins (Redeye): it tried to move the bundle a second time and rolled back.
It now moves each bundle once.

Validation: inventory regeneration removed exactly the 12; the fixture is the
previous one minus all retired modules; pruning a disposable prefix moved all
49 retired bundles and kept Blue Amp, JCM800, Plexi, SVT and Gx Muff. Seven of
the eight CTest suites pass; `ui_workflows` still times out as before.
The [Pi and module list work](#results) listed for the drives applies here too.

## Modulation audition

**Plan and sessions 20 November 2026. Retirement applied to the Windows editor
and builds; not yet deployed to the Pi.**

Modulation has 31 cards; the target is about 15. Same method as the drives:
same riff, level-matched against bypass, keep at most two per session, the
rest retired. Set every module in a session to a similar rate and depth before
judging; play stereo modules on a stereo output.

### Not auditioned

- **Our own modules** stay: Eureka Flock (chorus) and Eureka Ripple (phaser).
  Play them in sessions 1 and 3 as a reference.
- **The Twist processors** are the modular part and stay: Bitmangler,
  Chebyshev, Comparator, Frequency Shifter, Meta, Vocoder, Wavefolder, Twist
  Delay and Binaural Doppler Panner. They are not modulation effects, and nine
  of the 31 cards. Decide first: move them to Texture (Twist Delay to Delay),
  which leaves Modulation with the guitar effects.
- **AMS Ring Modulator - CV** multiplies control signals: move it to Control.

### Sessions

**Session 1: Chorus**

| Module | Based on | Beebo |
| --- | --- | --- |
| Chorus (LFO, CV) | 8-voice multi-dimensional chorus | Yes (chorus_d) |
| Gx Chorus stereo | stereo chorus | |
| YK Chorus | two-chorus ensemble, stereo | Yes (chorus_j) |

**Session 2: Flanger**

| Module | Based on | Beebo |
| --- | --- | --- |
| Flanger (LFO, CV) | flanger with feedback and wave shape | Yes (flanger) |
| Gx Flanger | flanger | |
| MDA ThruZero | through-zero tape flanging | Yes (thruzero_flange) |

**Session 3: Phaser**

| Module | Based on | Beebo |
| --- | --- | --- |
| Stone phaser (Mono, Stereo) | vintage-style phaser ? | Yes (phaser) |
| Gx Phaser | phaser | |

**Session 4: Tremolo**

| Module | Based on | Beebo |
| --- | --- | --- |
| Gx Tremolo | amp-style tremolo | |
| Gx Tube tremolo | tube amp tremolo | |
| Gx Switched tremolo | stepped tremolo with switched rates | |
| Harmonic tremolo (LFO, CV) | Fender brownface harmonic tremolo ? | Yes (harmonic_tremolo) |

**Session 5: Vibrato**

| Module | Based on | Beebo |
| --- | --- | --- |
| Vibrato (LFO, CV) | phaser-based vibrato, Uni-Vibe style ? | Yes (vibrato) |
| Gx Tube vibrato | tube amp vibrato | |

**Session 6: Rotary speaker**

| Module | Based on | Beebo |
| --- | --- | --- |
| Rotary speaker (Simple, Full) | Leslie, physically modelled | Yes (rotary) |
| MDA Leslie | Leslie | |

**Session 7: Ring modulator and panner.** Decide by function: keep one ring
modulator; keep RoundPan only if auto-pan is wanted beside the Doppler Panner.

| Module | Based on | Beebo |
| --- | --- | --- |
| AMS Ring Modulator - Audio | multiplies two audio inputs | |
| MDA RingMod | ring modulator with its own oscillator | |
| MDA RoundPan | auto-panner | |

### Results

| Session | Kept | Retired |
| --- | --- | --- |
| 1 Chorus | Chorus (LFO, CV), YK Chorus | Gx Chorus stereo |
| 2 Flanger | Flanger (LFO, CV), MDA ThruZero | Gx Flanger |
| 3 Phaser | Stone phaser (Mono, Stereo) | Gx Phaser |
| 4 Tremolo | Gx Tremolo, Harmonic tremolo (LFO, CV) | Gx Tube tremolo, Gx Switched tremolo |
| 5 Vibrato | Vibrato (LFO, CV) | Gx Tube vibrato |
| 6 Rotary speaker | Rotary speaker (Simple, Full) | MDA Leslie |
| 7 Ring modulator and panner | AMS Ring Modulator - Audio, MDA RingMod, MDA RoundPan | |

Seven identities retired; Modulation in Add goes from 31 cards to 24. MDA
Leslie shares Debian's MDA bundle with kept effects: only its identity is
excluded. The other six bundles move to recovery storage.

The category moves were then applied in the overlay: the `polywarps` match rule
puts the Twist processors in Texture, Twist Delay goes to Delay, and Ring
Modulator - CV and CV delay go to Control. Modulation now holds **14 cards**,
Delay stays at 15, Texture has 22 and Control 32. Saved patches are unaffected;
only Add's grouping changes.

## Delay audition

**Plan and sessions 20 November 2026. Retirement applied to the Windows editor
and builds; not yet deployed to the Pi.**

Delay has 21 cards. Set every module in a session to the same time (a dotted
eighth at the same tempo) and similar feedback, level-match, and listen to the
repeats as well as the attack. Keep at most two per session.

### Not auditioned

- **Our own modules** stay: Aura, Discipline, Euclidelay, Eureka Skip, Eureka
  Spool and Loo-pi. Spool is the reference in session 2.
- **Mist Looper and Mist Beat Repeat** (Clouds) are the modular part and stay.
- **CV delay** delays control signals, not audio: move it to Control.

### Sessions

**Session 1: Clean digital delays.** Tempo delay outputs only the echoes:
cable the dry signal to OUT as well when comparing.

| Module | Based on | Beebo |
| --- | --- | --- |
| Tempo delay | delay timed in beats, follows patch tempo | Yes (delay) |
| Gx Digital delay (Mono, Stereo) | digital delay with tempo and filters | |
| Stereo delay | MDA stereo delay, separate left and right times | |
| Gx Delay stereo | stereo delay with LFO | |
| Gx Echo stereo | stereo echo with LFO | |

**Session 2: Tape and character delays**

| Module | Based on | Beebo |
| --- | --- | --- |
| Gx Echo Cat | three-head tape echo, Watkins Copicat style ? | |
| Gx Tube delay | tube delay | |
| MDA DubDelay | modulated dub echo | |
| Bitcrushed delay | delay with bitcrushed repeats | Yes (bitcrushed_delay) |

**Session 3: Ducking and multiband.** Decide Multiband delay and Multiband
echo by function: they do the same job, keep one.

| Module | Based on | Beebo |
| --- | --- | --- |
| Gx Duck delay (Mono, Stereo) | repeats duck while playing | |
| Gx Multiband delay | five bands, own time and feedback each | |
| Gx Multiband echo | five bands, own time and level each | |

### Results

| Session | Kept | Retired |
| --- | --- | --- |
| 1 Clean digital delays | Tempo delay, Stereo delay | Gx Digital delay (Mono, Stereo), Gx Delay stereo, Gx Echo stereo |
| 2 Tape and character delays | Gx Echo Cat, MDA DubDelay, Bitcrushed delay | Gx Tube delay |
| 3 Ducking and multiband | Gx Duck delay (Mono, Stereo) | Gx Multiband delay, Gx Multiband echo |

Session 2 kept three: the user retired only Gx Tube delay.

Seven identities retired, all standalone bundles; Delay in Add goes from 21
cards to 15. The Gx Digital delay family is removed from the overlay.

Retirement of the modulation and delay sessions together (14 identities):
`retired.json` has 67 entries; the inventory drops from 325 to **311
identities**; the Patchstorage manifest drops 13 publications (44 to 31); the
Windows fixture drops from 293 to 279 modules. Seven retired delays were
marked to ring on when bypassed, so the `catalogue_files` test now expects 18
spilling modules instead of 25.

Validation: inventory regeneration removed exactly the 14; the fixture is the
previous one minus all retired modules; pruning a disposable prefix moved all
62 retired bundles and kept the Gx Duck delays, Gx Tremolo, Plexi and Gx Muff.
Seven of the eight CTest suites pass; `ui_workflows` still times out as before.

## Filter audition

**Plan and sessions 20 November 2026. Retirement applied to the Windows editor
and builds; not yet deployed to the Pi.**

Filter has 23 cards. Same method: same riff, level-matched, keep at most two
per session, the rest retired.

**Session 1: Pedal wahs. Decided without listening, 20 November 2026.**
Peepo has no expression pedal input (not yet scoped,
[interface](03-interface-and-controls.md)). A pedal wah can only be moved by a
MIDI pedal or controller through MIDI learn, or parked as a fixed "cocked wah"
tone, and one wah covers both. Gx Crybaby GCB95 is kept; the other three are
retired. Switchless wah in particular is useless without a pedal: its feature is
switching on when the pedal moves. Look again if an expression input is added.

| Module | Based on | Beebo |
| --- | --- | --- |
| Gx Crybaby GCB95 | Dunlop Cry Baby GCB95 | |
| Gx Wah | wah with manual sweep | |
| Gx Wahwah | Colorsound wah ? | |
| Gx Switchless wah | wah that switches on when the pedal moves | |

**Session 2: Auto wahs and envelope filters.** Play the same dynamics into
each: soft, then hard.

| Module | Based on | Beebo |
| --- | --- | --- |
| Gx Auto wah | envelope-driven wah | |
| Quack | envelope filter, Mu-Tron style ? | |
| MDA RezFilter | resonant filter with envelope and LFO | |

**Session 3: EQ.** Decide mostly by function: one parametric and at most one
graphic.

| Module | Based on | Beebo |
| --- | --- | --- |
| Parametric EQ (Mono, Stereo) | x42 parametric EQ | Yes (mono_EQ) |
| Gx Graphic EQ | graphic EQ | |
| Gx Bark graphic EQ | graphic EQ on the Bark scale | |
| Gx Tilt tone | one-knob tilt EQ | |

**Session 4: Synth filters.** These are the modular part and have CV inputs;
the session only removes duplicates. Four are Moog-style ladders: keep at most
two of them. Play each with the same CV sweep (an LFO on cutoff).

| Module | Based on | Beebo |
| --- | --- | --- |
| Moog ladder | Moog ladder, 4-pole | |
| Moog half ladder | Moog ladder, 2-pole | Yes (oog_half_lpf) |
| Moog Low-Pass Filter 1 | Moog ladder (FOMP) | Yes (filter) |
| Moog LPF | Moog ladder (AMS) | |
| Diode ladder | Roland TB-303 style diode ladder ? | Yes (diode_ladder_lpf) |
| Korg 35 low-pass | Korg MS-20 filter | Yes (k_org_lpf) |
| Korg 35 high-pass | Korg MS-20 filter | Yes (k_org_hpf) |
| Oberheim filter | Oberheim SEM, multi-output | Yes (filter_uberheim) |
| AMS VCF | multi-mode filter | |

**Session 5: Vocoder and talk box.** Decide by function. VocInput only
prepares a signal for a vocoder: it goes if MDA Vocoder goes. Twist Vocoder
(Texture) stays and is the reference.

| Module | Based on | Beebo |
| --- | --- | --- |
| MDA Vocoder | channel vocoder, two inputs | |
| MDA TalkBox | talk box, two inputs | |
| MDA VocInput | vocoder input preparation | |

### Results

| Session | Kept | Retired |
| --- | --- | --- |
| 1 Pedal wahs | Gx Crybaby GCB95 | Gx Wah, Gx Wahwah, Gx Switchless wah |
| 2 Auto wahs and envelope filters | Quack, MDA RezFilter | Gx Auto wah |
| 3 EQ | Parametric EQ (Mono, Stereo), Gx Graphic EQ | Gx Bark graphic EQ, Gx Tilt tone |
| 4 Synth filters | Moog half ladder, Moog Low-Pass Filter 1, Diode ladder, Korg 35 low-pass, Korg 35 high-pass, Oberheim filter, AMS VCF | Moog ladder, Moog LPF |
| 5 Vocoder and talk box | MDA TalkBox | MDA Vocoder, MDA VocInput |

Session 1 retirement: `retired.json` has 70 entries; the inventory drops from
311 to **308 identities**. Gx Wah shares the `gxwah.lv2` bundle with the kept
Gx Auto wah: the bundle and its Patchstorage publication stay, and only the
identity is excluded. Gx Wahwah and Gx Switchless wah are standalone: their
two publications leave the manifest (31 to 29) and their bundles move to
recovery storage. The Windows fixture drops from 279 to 276. Filter in Add goes
from 23 cards to 20. All seven runnable CTest suites pass; pruning a disposable
prefix kept `gxwah.lv2` and the Crybaby.

## Dynamics audition

**Plan and sessions 20 November 2026. Retirement applied to the Windows editor
and builds; not yet deployed to the Pi.**

Dynamics has 13 cards. Compressors are judged by ear: same riff, same gain
reduction (about 6 dB), level-matched. The rest are decided by function.

**Session 1: Compressors and sustain**

| Module | Based on | Beebo |
| --- | --- | --- |
| Compressor (Mono, Stereo) | x42 dynamic compressor | Yes (mono_compressor) |
| Gx Compressor | compressor | |
| MDA Dynamics | compressor, limiter and gate in one | |
| Gx Sustainer | sustainer | |

**Session 2: Multiband.** They do the same job: keep one.

| Module | Based on | Beebo |
| --- | --- | --- |
| Gx Multiband compressor | multiband compressor | |
| MDA MultiBand | three-band compressor | |

**Session 3: Attack and level shapers**

| Module | Based on | Beebo |
| --- | --- | --- |
| Slow gear | Boss SG-1 Slow Gear | Yes (auto_swell) |
| MDA Transient | transient shaper | |
| Gx Expander | expander, reduces noise between notes | |
| MDA Limiter | limiter | |

**Session 4: Studio tools, by function.** De-ess is for voices, Loudness is
a mastering filter, Splitter divides a signal by frequency or level.

| Module | Based on | Beebo |
| --- | --- | --- |
| MDA De-ess | de-esser | |
| MDA Loudness | loudness contour | |
| MDA Splitter | frequency or level splitter | |

### Results

| Session | Kept | Retired |
| --- | --- | --- |
| 1 Compressors and sustain | Compressor (Mono, Stereo), MDA Dynamics | Gx Compressor, Gx Sustainer |
| 2 Multiband | MDA MultiBand | Gx Multiband compressor |
| 3 Attack and level shapers | Slow gear, MDA Transient | Gx Expander, MDA Limiter |
| 4 Studio tools | MDA De-ess, MDA Loudness, MDA Splitter | |

## Pitch, reverb and cabinet audition

**Plan and sessions 20 November 2026. Retirement applied to the Windows editor
and builds; not yet deployed to the Pi.**

One short round for three small categories. Mist Stretch, Mist Oliverb, the
Eureka reverbs, Dragonfly and IR Reverb stay and are the references.

**Session 1: Detune.** Same job: keep one.

| Module | Based on | Beebo |
| --- | --- | --- |
| Gx Detune | detune with octave, wet and dry levels | |
| MDA Detune | small pitch offsets for doubling | |

**Session 2: Octave and pitch shift**

| Module | Based on | Beebo |
| --- | --- | --- |
| Gx OC-2 | Boss OC-2 octave down | |
| MDA SubSynth | synthesized sub-octave | |
| TAP Pitch Shifter | pitch shifter | Yes (pitch_shift) |

**Session 3: Odd pitch, by function**

| Module | Based on | Beebo |
| --- | --- | --- |
| MDA RePsycho! | audio-triggered pitch shift and playback | |
| MDA Shepard | endlessly rising or falling Shepard tone | |

**Session 4: Reverb.** Shimmizita is compared with Mist Oliverb (also a
pitched reverb); Multiband reverb with Dragonfly Hall.

| Module | Based on | Beebo |
| --- | --- | --- |
| Gx Shimmizita | shimmer reverb on Zita | |
| Gx Multiband reverb | reverb per frequency band | |

**Session 5: Built-in cabinets.** The IR Cabinet stays. Compare each with it,
using the same amp; keep one built-in cabinet, or none.

| Module | Based on | Beebo |
| --- | --- | --- |
| Gx Cabinet (built-in) | Guitarix speaker models | |
| Ultra Cab (built-in) | cabinet simulator pedal | |

### Results

| Session | Kept | Retired |
| --- | --- | --- |
| 1 Detune | Gx Detune, MDA Detune | |
| 2 Octave and pitch shift | Gx OC-2, TAP Pitch Shifter | MDA SubSynth |
| 3 Odd pitch | MDA RePsycho!, MDA Shepard | |
| 4 Reverb | | Gx Shimmizita, Gx Multiband reverb |
| 5 Built-in cabinets | Gx Cabinet (built-in) | Ultra Cab (built-in) |

Session 1 kept both detunes although the plan proposed one.

### Retirement of the filter (sessions 2 to 5), dynamics and pitch, reverb and cabinet sessions

16 identities, retired as before:

- `retired.json` has 86 entries; the inventory drops from 308 to **292
  identities**. Ten standalone bundles move to recovery storage. With Gx
  Auto wah gone, `gxwah.lv2` holds no kept plugin, so the earlier Gx Wah entry
  now names it as well. Moog ladder (Poly filters), Moog LPF (AMS) and the four
  MDA modules share bundles with kept modules: only their identities are
  excluded.
- The Patchstorage manifest drops nine publications (29 to 20); the Windows
  fixture drops from 276 to 260.
- Gx Shimmizita and Gx Multiband reverb were marked to ring on when bypassed,
  so `catalogue_files` now expects 16 spilling modules.

Cards in Add: Filter 13, Dynamics 8, Pitch 7, Reverb 7, Cabinet 2; 234 in all.

Validation: the fixture is the previous one minus all retired modules; pruning
a disposable prefix moved all 74 retired bundles and kept the shared Poly
filter and AMS bundles, Crybaby, Quack, Graphic EQ, Slow gear, Gx Detune and
Gx Cabinet. Seven of the eight CTest suites pass; `ui_workflows` still times out
as before.

## Utility and Sound source: by function

**20 November 2026. Decided and applied to the Windows editor; not yet
deployed to the Pi.**

No listening. The modular helpers (AMS mixers, VCAs, switches, slew, pitch
calibration, math, CV and MIDI converters, Discipline Return, Tuner) and our
own modules stay and are not listed.

### MIDI filters (32 cards)

**Where they come from.** All 32 are one collection, x42 MIDI filters by Robin
Gareus, and **every one is in Beebo's module map** (Beebo offers the whole
collection; the inventory notes each as `# Beebo: midi_...`). They were offered
in Add on 22 October and none has been played from a controller yet.

**What a MIDI filter is.** It makes no sound. It sits on a MIDI cable, between
a MIDI device (or MIDI IN) and something that plays notes (a synth module,
MIDI to CV, MIDI OUT to another device), and changes the messages passing
through: which channel they are on, which notes, how hard, or which knob.

**Cost of keeping them.** All 32 live in one shared Debian bundle, so
retiring some removes them from Add only; nothing is uninstalled. The only
gain is a shorter Utility list.

**How the suggestion was made.** Keep what a player routing a controller,
Peepo and other MIDI gear would reach for. Retire exact duplicates,
piano-keyboard tools (sustain-pedal behaviour) and housekeeping that fixes
problems rarely met.

| Module | What it does, in plain words | Beebo | Suggestion | Why |
| --- | --- | --- | --- | --- |
| MIDI Channel Map | moves messages from one channel to another | Yes | keep | send a controller to a device listening on another channel |
| MIDI Channel Filter | lets through only the channels you switch on | Yes | keep | the general channel filter |
| MIDI Simple Channel Filter | lets through one channel | Yes | retire | Channel Filter does the same |
| MIDI Channel Unisono | copies every message to a second channel | Yes | keep | play two synths or devices at once |
| MIDI Note/Channel Map | sends each of the 12 notes to its own channel | Yes | retire | very specialised |
| MIDI Chromatic Transpose | shifts every note up or down by semitones | Yes | keep | plain transpose |
| MIDI Note Transpose | remaps each of the 12 notes to any other note | Yes | retire | despite its name it is a note map; Enforce Scale covers the musical use |
| MIDI Enforce Scale | moves wrong notes onto a chosen scale | Yes | keep | |
| MIDI Chord | turns one note into a chord in a scale | Yes | keep | |
| MIDI Strum | plays a chord's notes one after another, like a strum | Yes | keep | |
| MIDI Keysplit | low notes to one channel, high notes to another | Yes | keep | two sounds from one keyboard |
| MIDI Key-Range Filter | drops notes outside a range | Yes | retire | keyboard tool; Keysplit covers the common use |
| MIDI Monophonic Legato | holds a note until the next one, for mono synths | Yes | keep | |
| MIDI Note Toggle | first press starts a note, second press stops it | Yes | keep | drones held without a foot on a key |
| MIDI Sostenuto | piano sostenuto pedal | Yes | retire | piano-keyboard tool |
| MIDI Tonal Pedal | holds notes while the sustain pedal is down | Yes | retire | piano-keyboard tool |
| MIDI Choke | a new note cuts the previous one in its group | Yes | retire; **kept by the user** | drum-pad tool |
| MIDI Duplicate Blocker | drops a note already playing | Yes | retire | housekeeping |
| MIDI Quantization | moves notes onto a time grid | Yes | keep | tightens played notes to the tempo |
| MIDI N-Tap Delay | repeats notes several times, like an echo | Yes | keep | MIDI echoes |
| MIDI Delayline | delays notes once, optionally at random | Yes | retire | N-Tap Delay covers echoes |
| MIDI Velocity Adjust | makes notes softer or harder overall | Yes | keep | |
| MIDI Velocity Gamma | reshapes the soft-to-hard curve | Yes | retire | Velocity Adjust is enough for most |
| MIDI Velocity Randomization | varies how hard notes are, at random | Yes | retire | specialised |
| MIDI Velocity-Range Filter | drops notes softer or harder than a range | Yes | retire | specialised |
| MIDI CC Map | turns one knob number (CC) into another | Yes | keep | make a controller's knob reach the CC a device expects |
| MIDI Scale CC Value | narrows or flips a knob's range | Yes | keep | a knob that only covers part of a control |
| MIDI CC to Note | turns a knob or switch into notes | Yes | retire; **kept by the user** | specialised |
| MIDI Note to CC | turns notes into knob messages | Yes | keep | pads or keys as switches for a device |
| MIDI Note to PC | turns notes into program changes | Yes | keep | change another device's preset from pads or keys |
| MIDI Event Filter | blocks one kind of message | Yes | retire | housekeeping |
| MIDI Remove Active Sensing | strips a keep-alive message some keyboards send | Yes | retire | housekeeping |

Suggested: **17 kept, 15 retired.** This corrects the first proposal, which
called Chromatic Transpose a duplicate of Note Transpose (they do different
jobs: the plain transpose is the one to keep) and proposed retiring Channel
Unisono and Scale CC Value, which are useful for layering and for knobs.

### Examples and stereo tools

| Module | Decision |
| --- | --- |
| Example Fifths | retired |
| Example MIDI Gate | retired |
| Example Metronome | retired |
| Example Scope (Mono, Stereo) | kept |
| MDA Image | kept |
| MDA Stereo | retired |

### Sound source

| Module | Decision |
| --- | --- |
| MDA TestTone | retired |
| MDA BeatBox, MDA Tracker | kept |
| MDA DX10, JX10, ePiano, Piano | kept |
| AMS Noise2, Weave Oscillator, Drizzle Synth, Eureka Spark | kept |

### Results

Five identities retired, none of them in Beebo's module map: Example Fifths,
Example MIDI Gate, Example Metronome, MDA Stereo and MDA TestTone. All are in
Debian system bundles shared with kept plugins or outside the Peepo prefix, so
only their identities are excluded; nothing moves to recovery storage.
`retired.json` has 91 entries; the inventory drops from 292 to **287
identities**; the Windows fixture drops from 260 to 255. All seven runnable
CTest suites pass.

MIDI filters: the user kept the 17 suggested plus MIDI CC to Note and MIDI
Choke, **19 in all**, and retired the other 13: Simple Channel Filter,
Note/Channel Map, Note Transpose, Key-Range Filter, Sostenuto, Tonal Pedal,
Duplicate Blocker, Event Filter, Remove Active Sensing, Delayline, Velocity
Gamma, Velocity Randomization and Velocity-Range Filter. All 13 were in Beebo's
module map. They share the x42 MIDI filter bundle with the kept 19, so only
their identities are excluded. `retired.json` has 104 entries; the inventory
drops from 287 to **274 identities**; the Windows fixture from 255 to 242. All
seven runnable CTest suites pass; the live engine test's MIDI Chromatic
Transpose is kept.
