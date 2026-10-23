# Eureka integration

**23 October 2026. Installed and host-checked; instrument qualification open.**

Eureka adds twelve independent stereo effects: Slowmo, Hush, Bloom, Skip,
Ripple, Prism, Spool, Granny, Spark, Chime, Flock and Crinkle. Their source is
pinned in `14-more-plugins.sh` and `reference/manifest.json` to published commit
`4b52532c3f0f426a38005c01cee5532e8a15b542` (firmware Size/Time/Rate laws and pedal Mix;
M6/M7 used `7e7eed3`). No commit, push or publication was
made during integration; pre-existing Aura pin changes were preserved.

The overlay follows Eureka's indexed contracts and categories. Each has Main,
Sound and hold, and Output groups; Skip/Spool also have Tempo. General Output
is used, and all twelve are bypassed through their actual enabled ports so
tails can run. Only Skip and Spool follow patch BPM. Chime has Hybrid/Clean/
Clipped Voicing. All main controls and Mix have patchable CVs, and each effect
has its own hold gate. Cable a mono guitar explicitly to both audio inputs.

The source inventory has 370 identities. The WSL fixture retains its previous
326 records and adds twelve; the installed Pi catalogue retains its previous
338 and adds twelve (350 total). Export only Eureka when integrating this
family, then merge those records; the WSL and Pi have different installed
families. Catalogue Ready means host-compatible, not guitar-qualified.

Both WSL and native make/test pass. All twelve have actual adapter create,
control, hold-assignment, enabled-port bypass, duplicate, disk save/reload and
remove checks. Skip/Spool followed 87/143 BPM changes. Separate native and WSL
Ingen/dummy-JACK audio checks cover wet sound, Sway CV, Footswitch gates and
bypass drainage. The first native Chime reload timed out; an unchanged repeat
passed all twelve. Settings persist, audio history deliberately does not.

At 48 kHz/128 frames, native callbacks for all twelve and four three-effect
chains fit 2,666.67 us. Existing-server short JACK runs had three xruns for
Slowmo; the other effects and four short chains had none. Two-minute chain
tests recorded 0/9/10/6 xruns (Flock-Spool-Slowmo / Skip-Granny-Crinkle /
Spark-Chime-Prism / Slowmo-Hush-Prism), despite no measured callback overruns.
Before/after power snapshots were `0x50005`, including active undervoltage and
throttling; later probes returned `0x50000`. Stable-power endurance is not passed.

Parallel dry Flock/Prism branches report 89/482 frames and arrive 393 frames
(8.1875 ms) apart in both engines. Ingen does not compensate this graph.
Serial dry/wet alignment passes; do not claim arbitrary parallel alignment.
Hardware pitch calibration, physical stomp use and remaining guitar/screen
checks remain pending. No default changed and no module is instrument-ready.

The bundle is `/opt/peepo/lib/lv2/eureka.lv2`; exact source, archive SHA-256,
reference and notices are in `/opt/peepo/share/peepo/sources/eureka/`.
Catalogue/inventory/overlay backups are in
`/opt/peepo/share/peepo/backups/eureka-m7-20261023T142247Z/`.
Ingen discovered the bundle through its engine loadedBundle property; only
the UI was restarted. JACK PID 684 and Ingen PID 758 were retained, and the
playing graph including unsaved state matched before/after.

Machine-readable measurements and remaining qualification are in
[the M7 evidence](evidence/eureka-m7-20261023.json). The actual installed
catalogue is [the dated export](evidence/catalogue-pi-20261023-eureka.json).
Reproduction and full failure records are in Eureka's
`docs/peepo-integration.md` and `docs/validation.md`.

## First guitar report, 23 October 2026

The user reports Spool and Hush as good. Flock was heard as chorus and Slowmo
as reverb; descriptions now use those audible effect names. Skip Time became
audible after the user switched Sync off: Sync deliberately substitutes BPM and
division. The control named Repeat heads selects 2..16 taps, not feedback.
Ripple's Pause sweep holds filter position while continuing audio processing.
Hush Spread changes right-side delay lengths and alternating stereo taps.

New installed-library audio regressions confirm free Skip Time endpoints with
first taps near 32 ms and 2.10 s, 2 versus 16 taps, synced Time override, active
filtering during Ripple Pause, and Hush stereo Spread. No DSP/default/port
changed. The installed overlay was backed up under
`eureka-presentation-20261023T152718Z`; the UI was reloaded for the wording and
Repeat heads/Pause sweep labels. See the machine-readable evidence for exact
measurements and which controls the user actually reported testing.
