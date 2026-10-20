# MIDI: devices, cables between modules, clock and learn

**Planned 20 October 2026.** Nothing is built yet. This plan covers all of Beebo's MIDI ([Beebo manual gaps](30-beebo-manual-gaps.md#7-midi), item 7), not only tempo out:

1. **MIDI in and out** on the Pi: USB devices first, then 3.5 mm TRS jacks.
2. **MIDI inside the patch:** MIDI cables between modules on the canvas, MIDI IN and MIDI OUT connectors, and the MIDI modules the catalogue already lists. This revises D16, which deferred MIDI cables ([decisions](07-source-audit-and-decisions.md)).
3. **Instrument-wide MIDI:** clock out (the first target is a Boss SL-2), clock in, program change selecting the set list, and MIDI learn on any control.

## 1. MIDI in and out on the Pi

### USB: the first route, no soldering

The Pi 5 has four USB-A ports (two USB 3, two USB 2). The Tascam US-100 takes one and the touch screen another; two are free, and a powered hub adds more. The Pi is the USB **host**, so any class-compliant USB-MIDI **device** works with Linux's own driver:

| Plugged in | What it gives |
| --- | --- |
| A MIDI foot controller with USB (e.g. a Morningstar) | program changes, CCs, tap tempo into Peepo |
| A USB keyboard or pad controller | notes for the instruments and MIDI to CV; CCs for MIDI learn |
| A pedal with a USB-MIDI port | MIDI both ways over one cable, no adapter |
| A USB-MIDI interface (e.g. Roland UM-ONE: one DIN in, one DIN out) | any DIN or TRS device: a drum machine's clock in, Peepo's clock out to the SL-2 |

Limits:

- **Host to host does not work.** A computer is also a host, so a computer cannot be plugged into the Pi's USB-A ports. Between them, use a USB-MIDI interface on each side. The Pi 5's USB-C port can act as a USB-MIDI device (Linux's MIDI gadget), but that port is also the power input, and a computer cannot supply the Pi 5's power. Not planned.
- **The SL-2's own USB port** is for BOSS Tone Studio. Whether it also shows up as a USB-MIDI device is to be tried: plug it in and run `aconnect -l`. If it does, the SL-2 needs no MIDI cable at all.
- **The Tascam shares the bus.** MIDI uses very little bandwidth, but the audio interface must never be disturbed. Check for xruns with a MIDI device plugged in, and if needed put it on the other USB controller.

### 3.5 mm TRS jacks on the Pi's UART: the enclosure's own MIDI

GPIO 14 (TX) and 15 (RX) are already kept free for this ([wiring proposal](18-physical-controls.md#wiring-proposal-p71--proposed-to-verify-on-the-pi)). Both jacks are 3.5 mm TRS, **Type A** (tip = MIDI pin 5, ring = pin 4, sleeve = pin 2). Adapter cables reach DIN devices; Boss's BCC-1-3535 goes straight to the SL-2.

**MIDI OUT (GPIO 14):**

| Part | Purpose |
| --- | --- |
| 3.5 mm TRS jack, panel mount | MIDI OUT |
| Non-inverting buffer, e.g. 74LVC1G17 (or two gates of a 74HC14 in series) | between GPIO 14 and the jack, so a short on the cable cannot reach the Pi |
| 10 Ω resistor | buffer output to the **tip** |
| 33 Ω resistor | 3.3 V to the **ring** |
| 100 nF capacitor | decoupling |

The sleeve goes to ground at the sender only. The values are the MIDI Association's 3.3 V figures; check them against the current specification when ordering.

**MIDI IN (GPIO 15):**

| Part | Purpose |
| --- | --- |
| 3.5 mm TRS jack, panel mount | MIDI IN |
| H11L1 optocoupler (works at 3.3 V) | keeps the sending device electrically separate from the Pi, as MIDI requires |
| 220 Ω resistor | in series with the optocoupler's LED |
| 1N4148 diode | across the LED, reversed, against wrong polarity |
| Pull-up to 3.3 V on the output | value from the optocoupler's datasheet |
| 100 nF capacitor | decoupling |

The MIDI IN sleeve is **not** grounded; that is what prevents ground loops. A 6N137 needs 5 V and is not used.

**On the Pi:**

- [ ] free the UART from the serial console and enable it (`config.txt`); `/dev/ttyAMA0` expected on the Pi 5;
- [ ] check the UART runs at MIDI's **31250 baud**, which is not a standard rate. With a 48 MHz reference clock the divider is exact (48,000,000 / 16 / 31250 = 96); measure it with a logic analyser or by a device locking to it;
- [ ] make the UART an ALSA MIDI device. Linux has a driver for exactly this, `snd-serial-generic`, bound to the UART by a device-tree overlay; check that Raspberry Pi's kernel builds it. If not, a small bridge in `peepo-midi` reads and writes the serial port instead.

Then the jacks look exactly like a USB-MIDI device to everything below.

## 2. How MIDI reaches the engine

**All MIDI devices go through JACK**, like the audio. `a2jmidid` (a service of its own) turns every ALSA MIDI device, USB or UART, into JACK MIDI ports and follows hot-plugging. `peepo-route`, which already wires the audio, wires the MIDI too:

```
USB / UART devices --a2jmidid--> JACK MIDI --peepo-route--> engine's midi_in   --> MIDI IN connector on the canvas
                                                       \--> engine's control   --> MIDI learn (Ingen's bindings)
                                                       \--> peepo-midi         --> clock in, program change
engine's midi_out --+
peepo-midi (clock) -+--> JACK MIDI --> devices
```

- **The engine's ports.** Today the root graph has `in_l`, `in_r`, `out_l`, `out_r` (`src/engine/ingen/session.cpp:16`). It gains `midi_in` and `midi_out`, both atom ports of MIDI events.
- **MIDI learn already exists in Ingen.** Its control bindings read MIDI from the root graph's first port (`control`, `Engine.cpp` runs `control_bindings()->pre_process` on port 0 every cycle) and set any port with a `midi:binding` property. Peepo only has to wire that port, start learning and show the binding.
- **Merging.** Several cables into one MIDI input are merged in time order by Ingen (`mix.cpp`, sequences), so all devices can feed one MIDI IN.
- **Which devices.** By default every input device feeds MIDI IN and MIDI OUT goes to every output device. Settings → MIDI can turn a device off in either direction; that is stored instrument-wide, not in patches, since devices come and go.

## 3. MIDI inside the patch

### Ports and cables

- **The port type needs splitting first.** `PortType::Midi` (`src/core/model.hpp:28`) is given to every LV2 atom port. Many of those carry only patch messages, such as the Cabinet's and NAM's file selection (`src/engine/ingen/session.cpp:566`); they must not become MIDI connectors. A port is a MIDI connector only when it declares `atom:supports midi:MidiEvent`. The others stay hidden, as now.
- **A third cable type: MIDI**, with its own colour. It joins a MIDI output to a MIDI input only; audio-to-audio and control-to-control stay as they are. The same rules as the others: several cables may leave an output and enter an input, no duplicates, no feedback cycles ([domain and engine contracts](13-domain-and-engine-contracts.md#cables)).
- **MIDI IN and MIDI OUT connectors** on the canvas beside IN and OUT, connected to the engine's `midi_in` and `midi_out`.
- **One merged MIDI IN** (decided 20 October 2026). Every input device feeds the same connector, so a patch does not depend on what is plugged in. To tell a keyboard from a foot controller, set them to different channels and split with x42's channel filter.
- **Saved in patches** like the other cables. A patch with MIDI cables loaded where no MIDI device is plugged in still loads; the cables are there and simply carry nothing.

### The modules

The inventory already lists 42 MIDI entries mapped from Beebo (`config/catalogue/inventory.txt`). Their state today ([module list](33-module-list.md)):

| Modules | Today | With MIDI cables |
| --- | --- | --- |
| x42 MIDI filters: chord, strum, transpose, enforce scale, delays, note/CC conversions, channel and velocity tools (about 30) | Unavailable: nothing to patch | usable between MIDI IN, each other and MIDI OUT |
| MDA DX10, ePiano, JX10, Piano | Needs adaptation | played from a keyboard or a MIDI module |
| MIDI clock in (Poly), CV to MIDI CC, CV to MIDI Note, Strum, MIDI Gate, Metronome | Ready, but only their CV and audio sides can be cabled | fully usable |
| x42 MIDI clock, midimsg MIDI controller (CC to control value) | Not packaged; not built because MIDI was deferred | build them (`tools/ingen-spike/14-more-plugins.sh`) |
| Poly MIDI to CV mono and poly, MIDI program change, `loop_extra_midi`, `loop_midi_out` | No public source | our own plugins (below) |

**Our own plugins.** MIDI to CV is the important one: it turns a keyboard's or a MIDI module's notes into volt per octave and gate for Plaits, the filters and the envelopes. Each is its own repository under `C:\LV2-own-plugins\<name>`, GPL-2.0-or-later, like Discipline and Loo-pi. The two looper ones belong with Loo-pi's MIDI sync.

## 4. Instrument-wide MIDI: `peepo-midi`

A service of its own, separate from the interface, which can restart while audio plays ([physical controls](18-physical-controls.md), last section); clock and program changes must keep working through that. It is a JACK MIDI client and a second engine client, which the engine already supports.

- **Clock out.** 24 clock messages (`F8`) per beat at the patch tempo (`peepo:tempo`, 30 to 300 BPM, [patch tempo](29-parameters-files-tempo.md#patch-tempo)), placed at the exact frame inside each JACK period. It follows a tap, the Tempo page and patch changes. Peepo has no transport, so it sends **Start** once when the clock begins, and the Tempo page gets a **Restart** button (Stop, then Start) to line up another pedal's pattern by hand.
- **Clock in.** Settings → MIDI chooses the tempo source: the patch, or incoming clock. With incoming clock, the patch tempo follows it, so every module with a BPM control follows too. Peepo then stops sending its own clock, to avoid a loop.
- **Program change** selects a set-list entry: entry number = program number ([set list](30-beebo-manual-gaps.md#6-set-list), item 6). Until the set list exists, it selects a stored patch in order. The channel is set in Settings → MIDI.
- **CC per patch.** A patch can carry a short list of CC messages (channel, number, value) sent when it loads, for example an SL-2 pattern per patch. Saved as graph properties, like the tempo. Inside a patch, the same can be done live with CV to MIDI CC cabled to MIDI OUT.

**MIDI learn** is the engine's, not `peepo-midi`'s (section 2). On a module page: hold a bar, choose **MIDI learn**, move a knob on the controller. The bar then shows its CC, and **Forget** removes it. Bindings are port properties, so they are saved with the patch.

## The first target: Boss SL-2

What the shop listings give (Boss's own manual still to read): a **MIDI IN** on a 3.5 mm TRS mini jack, no MIDI out or thru; it follows **MIDI clock** and takes **CC** messages. Boss's cables: BMIDI-5-35 (5-pin DIN to 3.5 mm), BCC-1-3535 (3.5 mm to 3.5 mm). It also has a 1/4" CTL/EXP jack and a USB port for BOSS Tone Studio.

Its first route is a UM-ONE on a USB port and a BMIDI-5-35; later Peepo's own TRS jack and a BCC-1-3535.

To check in the SL-2 manual (step P12.1):

- [ ] the TRS pinout: expected **Type A**;
- [ ] the MIDI channel and how to set it; whether clock sync must be enabled on the pedal;
- [ ] what it does with Start, Stop and Continue, and when the clock stops;
- [ ] its CC map (pattern, effect level and so on);
- [ ] whether its USB port is also USB-MIDI.

## Steps

These are Phase 12 of the [implementation plan](10-technical-implementation-plan.md#phase-12--midi), which keeps their status.

USB devices first, since they need no soldering; the canvas and modules next, since that is most of Beebo's MIDI; the jacks when the enclosure is built.

- [ ] **P12.1 — SL-2:** the checks above.
- [ ] **P12.2 — MIDI on the Pi through USB:** `a2jmidid` as a service; a UM-ONE and a USB controller appear as JACK MIDI ports and come back after unplugging; `peepo-route` wires them. Check for xruns with a device plugged in.
- [ ] **P12.3 — Clock out (`peepo-midi`):** on the PC under WSL2 first (JACK's dummy driver, a virtual MIDI port), then on the Pi to the SL-2. Tests: interval at 30, 120 and 300 BPM; a tempo change from the next clock message; a patch change carrying its tempo; the clock continuing while the interface restarts. Measure the intervals with a second interface or a logic analyser.
- [ ] **P12.4 — MIDI cables (PC):** split the port type; the MIDI cable and its rules; MIDI IN and MIDI OUT connectors; `midi_in` / `midi_out` on the engine; saving and loading. Tests as for the audio and control cables, plus file-message ports staying hidden. Record the revision of D16.
- [ ] **P12.5 — The MIDI modules:** rerun the compatibility checks with MIDI cables (`15-compat.sh`); build x42 MIDI clock and midimsg; mark the MIDI filters and the MDA instruments ready once they play from a controller on the Pi.
- [ ] **P12.6 — Settings → MIDI:** devices in and out, clock out on or off, Start on begin, tempo source, program-change channel; the Tempo page's Restart.
- [ ] **P12.7 — MIDI learn:** wire the engine's `control` port; learn and forget on the module page; the binding shown on the bar; saved and reloaded.
- [ ] **P12.8 — Program change and clock in** in `peepo-midi`.
- [ ] **P12.9 — Our own MIDI to CV (mono, then poly)**, then MIDI program change, in their own repositories.
- [ ] **P12.10 — The TRS jacks:** wire them, configure the UART and `snd-serial-generic`, measure 31250 baud, repeat P12.2 and P12.3 on them.
- [ ] **P12.11 — CC per patch:** the list on the patch, sent on load, and a page to edit it.

## Open questions

- Whether the enclosure needs 5-pin DIN as well, or only the 3.5 mm jacks with adapter cables.
- Whether incoming notes should also reach the canvas's MIDI IN when a program change or learned CC is used for the instrument, or be filtered out.

## Sources

- [Boss SL-2 at Full Compass](https://www.fullcompass.com/prod/615699-boss-sl-2-bss-compact-slicer-effect-pedal) (specifications: MIDI IN jack, CTL/EXP jack, cables)
- [Boss SL-2 at Promenade Music](https://www.promenademusic.co.uk/boss-sl2-slicer-pedal)
- [Boss SL-2 at Flipside Music](https://flipside-music.com/products/boss-sl-2-slicer)
- Ingen: `reference/upstream/ingen/src/server/Engine.cpp` (control bindings on the root graph's first port), `ControlBindings.cpp`, `mix.cpp` (merging event sequences)
