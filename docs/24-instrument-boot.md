# Direct instrument boot

**Verified on the Raspberry Pi, 14 October 2026.** The Pi now boots directly into Peepo on its 800 x 480 touchscreen, restoring the saved **Clouds guitar** patch through the Tascam US-100. The user confirmed after reboot: "Yes, Peepo appears and touch works". No desktop login or MODEP launch is required.

## Installed boot policy

- Default target: `multi-user.target`; `peepo.target` enabled.
- LightDM, MODEP's JACK, every installed `modep-*` service and its update path are disabled and masked. Masking prevents dependency activation as well as ordinary boot activation.
- The automatic login on `tty1` is disabled and masked, so it cannot write a console over the instrument. Serial console and SSH remain available.
- UI uses `--display drm`; no desktop or browser is involved. No firmware configuration or kernel command line was changed.
- Programs, catalogue, plugins and patches remain on the card. No network-ready target is required by the instrument services. A boot with networking physically unavailable remains untested.
- JACK, Ingen, routing and UI remain separate services. Start limits are disabled for their retry policies, so late devices do not exhaust the retry allowance. JACK wants the engine so its automatic restart also brings back the engine stopped by `BindsTo`.

The selected startup identity is `p20261014-222121-f9e0`, currently revision `r000002.ingen`, with Freeze off. The UI restores the latest saved revision of this identity when a fresh engine is ready. Restarting only the UI retains a live patch and its unsaved edits. Use **Patches > Startup** to change the startup choice. Save changes that should survive an engine restart or power cycle.

Before reboot, the newer unsaved **wendy** patch was saved as `p20261014-224640-727c/r000001.ingen`. Its Tubewarmth, Chorus and Harmonic tremolo modules, five cables, controls and layout were verified against the saved bundle; saving did not rebuild its audio graph. It remains available in the patch manager alongside Clouds.

## Repeatable setup and recovery

Packages include `instrument-boot.sh`, installed as `/opt/peepo/bin/peepo-instrument-boot`. An ordinary installation preserves existing boot policy; `sudo ./install.sh --instrument` explicitly selects instrument mode. For an existing installation:

```bash
sudo /opt/peepo/bin/peepo-instrument-boot --dry-run
sudo /opt/peepo/bin/peepo-instrument-boot --startup p20261014-222121-f9e0
sudo reboot
```

The script saves the previous default target and each affected unit's enable/mask policy in `/etc/peepo/instrument-boot-state` before changing them. Repeating the command preserves that original snapshot. It never deletes MODEP, a saved patch, settings or plugins. A startup identity is checked before any boot-policy mutation; the UI performs its existing bundle/plugin/asset validation at restoration.

To return to the previous desktop/MODEP boot policy:

```bash
sudo /opt/peepo/bin/peepo-instrument-boot --restore --dry-run
sudo /opt/peepo/bin/peepo-instrument-boot --restore
sudo reboot
```

Restoring stops Peepo and restores the original unit policy and default target. Patches and the startup choice remain. Package removal also restores the saved boot policy before deleting Peepo's service units. The original Peepo unit files from this deployment are retained separately in `/etc/peepo/instrument-unit-backup`.

## Verification and limits

One software reboot was verified with a changed Linux boot identity. All five instrument units were active, the saved Clouds bundle matched the engine's identity, modules, four cables, controls and layout, and all four capture/playback connections were present. LightDM, MODEP and the `tty1` login were masked; no services were failed. The user independently checked the physical display and touch.

Recorded boot events, in the journal's monotonic seconds since kernel startup:

| Event | Seconds |
| --- | ---: |
| JACK started / responsive | 7.09 / 8.03 |
| Ingen service ready | 9.37 |
| UI process started | 9.40 |
| Saved Clouds bundle loading began | 10.33 |
| All four external audio connections established | 11.92 |

These are service and routing events, not measured power-on-to-audible-sound or power-on-to-responsive-touch times. Firmware time is excluded. `systemd-analyze` reported 16.00 seconds for kernel plus userspace overall; it includes other system services.

A separate deliberate JACK `SIGKILL` recovered automatically: JACK PID 736 became 2296, Ingen 894 became 2311, UI stayed PID 978. Within the eight-second observation window the saved Clouds patch matched the bundle again and all four Tascam connections were restored. This is audio-service recovery with an interruption, distinct from the earlier uninterrupted UI-only restart test.

A passive 22-second post-boot monitor observed 8,250 blocks with no frame gaps or xruns and finite samples throughout. It also recorded 96 left and 86 right exactly silent blocks, so this run is **not** a sustained-playing/no-silence acceptance test. A subsequent audit saw a dirty graph during the touch session; the later recovery audit verified the saved revision. No audio was injected or recorded by the monitor.

Phase 10 remains partial: physical offline boot, five cold power cycles, late physical USB availability, controller recovery, missing-plugin/asset startup cases and shutdown/power-loss storage acceptance remain open. The undervoltage (`throttled=0x50000`) was the old supply; since 18 November 2026 a 5 A supply keeps the rail at 5.1 V with no throttling ([power supply](23-pi-bring-up.md#new-power-supply-18-november-2026)).

Evidence: [instrument boot results](evidence/pi-instrument-boot-results.txt). The boot utility and service changes were deployed without rebuilding or replacing the engine, plugins or touchscreen application; the patch-preservation CLI was compiled natively against the already deployed core library. Bash syntax and systemd unit verification passed on the Pi.

## Boot screen, slimmer boot and shutdown (15 October 2026)

**Boot screen.** `peepo-splash` (`tools/pi/splash.sh`) shows the Peepo logo from the initramfs until the interface draws:
- It installs the Plymouth theme in `config/pi/splash/peepo` (placeholder logo from `tools/pi/make-splash-logo.py`).
- It makes the kernel command line quiet and moves the console to tty3.
- It adds a marked `disable_splash=1` block to `config.txt`.
- Plymouth quits with `--retain-splash` only when the interface starts.

The first try left the screen black. The quit ran as the service user, and Plymouth ignored it, so Plymouth still owned the DRM card when the interface opened it. LVGL then waited forever for a page flip and ignored `SIGTERM`. Two fixes followed:
- The quit now runs privileged (`ExecStartPre=-+`).
- The interface waits until the card is free, and says so on stderr.

The reboot after the fix showed the interface as DRM master, with no restarts. The originals are kept in `/etc/peepo/splash-state`; `--restore` puts them back.

**Slimmer boot.** `peepo-slim-boot` (`tools/pi/slim-boot.sh`) disables and masks 29 desktop and Patchbox units the instrument does not use. Among them:
- the wait for the network (7 to 11 s per boot);
- printing, Bluetooth and the modem manager;
- Patchbox's module starter and telemetry;
- the Pisound services;
- swap on the SD card, file-system scrubbing, and background apt/man-db timers.

SSH, Wi-Fi, avahi and time sync stay on. Each unit's previous policy is saved in `/etc/peepo/slim-boot-state`; `--restore` re-enables them.

| Measured on the Pi | Before | After |
| --- | ---: | ---: |
| `systemd-analyze` (kernel + userspace) | 16.0 s | 6.1 s |
| All four audio connections made (since kernel start) | 11.9 s | 8.5 s |

Firmware time is still excluded. This was one warm reboot, not five cold power cycles.

**Shutdown.** Settings has a **Shut down** button on the instrument (DRM or framebuffer display only, never on a desktop). It asks first, warns about unsaved edits, then runs `systemctl poweroff`. The polkit rule `config/pi/polkit/50-peepo-power.rules` allows the `peepo` user that one action. Saves now `fsync` the revision's files and directories, and the startup and control files, before and after each rename, so a power cut right after a save keeps it. These changes are built and tested on Windows and Linux (WSL), and on arm64 (8/8 suites).

**On the Pi.** `peepo-arm64-20261015-98fe06c-dirty.tar.gz` replaced the pre-rename `pedalpi` install. The old files are kept in `/opt/pedalpi`, `/etc/pedalpi` (unit files in `units-backup`) and `/var/lib/pedalpi`. The migration copied:
- the audio interface and display settings;
- the boot policy saved before the first instrument boot;
- both saved patches, with their project namespace rewritten from `urn:pedalpi:ns#` to `urn:peepo:ns#`.

After a reboot:
- Linux took 5.9 s.
- The startup patch began loading at 7.3 s, and the four audio connections were made by 8.3 s.
- The interface held the DRM card on its first start.
- `pkcheck` granted power-off to `peepo` and refused it to other users.

The Shut down button was first used on 16 October 2026: the user shut down from Settings, power-cycled, and the instrument came back with its patch and services.

**Wi-Fi, 16 October 2026.** The Pi sits upstairs on the guest network `Camarones-invitades-2.4`, where its signal is moderate (about -55 dBm). On one boot it failed to associate 26 times; NetworkManager then stopped retrying (its default allows 4 rounds), so the Pi stayed unreachable until a restart. With the instrument on the screen there is no visible console (Ctrl+Alt+F2 shows nothing while the interface holds the display), so:
- the connection now retries without limit: `nmcli con mod Camarones-invitades-2.4 connection.autoconnect-retries 0` (undo with `-1`);
- Settings shows the Wi-Fi name, address and signal, or "Not connected", so the instrument can be found without a keyboard.

The new interface binary was installed by itself (`/opt/peepo/bin/peepo`, previous one kept as `peepo.prev`) and only `peepo-ui` was restarted; JACK and the engine kept their PIDs. The Pi still reported undervoltage and throttling (`vcgencmd get_throttled` = `0x50005`).

**Native interface crash, 16 October 2026 (P10.7).** `tools/pi/check-ui-crash.sh` sent SIGKILL to the interface while the instrument played its saved patch. systemd started a new one at once; it reconnected to the engine 1.7 s after the crash. JACK and the engine kept their PIDs, the engine's graph was unchanged, and a passive monitor counted 8,251 audio blocks over 22 s with no gap and no xrun. [Results](evidence/pi-ui-crash-results.txt).
