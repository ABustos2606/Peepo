# Catalogue curation

**Started 19 October 2026; updated 22 October 2026.**

The expansion phase installed whole plugin collections, and Add grew from a
curated, Beebo-like selection to about 280 ready entries: 55 drives and 20 amps
alone. This file records how the catalogue is brought back to a curated
instrument: what was retired, what was merged, and how the rest is chosen.

## Direction

Peepo's strongest side is the experimental modular part (Discipline, Clouds,
the resonators, the Twist processors, patchable control) beside a dependable
conventional pedalboard. The conventional categories are where the excess is;
the modular part is kept.

- **Hide, do not delete.** A rejected module leaves Add; saved patches still
  load it. Plugins are removed from builds only after that.
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
offered (done), drive and amp sessions, then modulation; reverb is nearly done.

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

**22 October 2026. Plan; no session held yet.**

The 55 Drive entries, grouped by the circuit each is based on, one listening
session per group. Same riff every session, through the same amp or NAM model;
level-match each module with its Output against a bypassed reference before
judging. Keep at most two per session; the rest are hidden as above.

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

| Session | Kept | Hidden |
| --- | --- | --- |
| 1 Boosts | | |
| 2 Treble boosters | | |
| 3 Tube Screamer types | | |
| 4 Low-gain and op-amp | | |
| 5 Tube overdrives | | |
| 6 Distortion | | |
| 7 Fuzz Face | | |
| 8 Tone Bender and Muff | | |
| 9 Octave fuzz | | |
| 10 Vintage and odd fuzz | | |
| 11 Lo-fi and multiband | | |
