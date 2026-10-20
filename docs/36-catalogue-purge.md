# First catalogue purge

**19 October 2026. Applied to the Pi and Windows editor selection.**

The user requested retirement of Gx Live looper, Ambience, MVerb, Gx Reverb
stereo, Gx Room simulator, Gx Zita reverb stereo and Dragonfly Early Reflections.
The selected inventory falls from 357 to **350 identities**, and the Pi export
from 335 to **328**. Add offers **272 ready identities**, down from 282:
seven retirements and three previously offered duplicate convolvers.

## IR processing

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
These are six explicit routing entries under two families, not a new mode
selector or automatic channel switching on an existing block.

The three LV2 Convolution and three x42 IR Convolver identities are hidden in
Add. They remain installed with their file metadata for older patches; the
existing x42 worker limitation remains. Gx Cabinet and Ultra Cab remain as
separate, explicitly labelled **built-in** cabinet simulators.

## Selection and builds

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

## Deployment and validation

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
