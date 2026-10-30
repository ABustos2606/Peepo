# Files and network: getting models and IRs onto Peepo

**Planned 30 October 2026; order and open questions decided with the user the same day ([decisions](#decided-with-the-user-30-october-2026)). P14.1 (the stick mount) built and checked in WSL the same day ([record](evidence/usb-results.txt)); P14.2 (file checks and copy, `src/storage/file_import.hpp`) done the same day; P14.3 (Settings → Files) the same day, from an approved mockup.** Today a NAM model or an IR reaches Peepo only by SSH from a computer on the same Wi-Fi, and the Pi knows one Wi-Fi network, joined by hand with `nmcli` ([instrument boot](24-instrument-boot.md)). Away from home the instrument cannot get online and cannot take a new file. Phase 14 of the [implementation plan](10-technical-implementation-plan.md#phase-14--files-and-network), where its steps are kept.

## What exists

- **The folders.** Choose lists the files in `/var/lib/peepo/files` (`nam/`, `ir/cab/`, `ir/reverb/`), made at start ([files](29-parameters-files-tempo.md#model-and-ir-files-p83)). With none it says which folder to copy into.
- **Patches carry their files.** Saving copies the model or IR into the patch bundle ([NAM](21-nam.md)), so a saved patch keeps playing if the file is later deleted from the folder.
- **Settings shows the network** (name, address, signal, or "Not connected") but cannot change it (`src/platform/linux/network_status.hpp`).
- **An on-screen keyboard** already names patches and scenes (`src/ui/application.cpp`).
- **`udisks2` is switched off** by `peepo-slim-boot`, so nothing mounts a USB stick today.

So every way in below only has to put a file into those folders, and nothing in the module pages or the picker changes.

## Order

1. **USB stick import** (P14.1-P14.4). Works with no network at all, which is the case in most rehearsal rooms and on stage.
2. **Wi-Fi on the pedal** (P14.5-P14.6). Settings → Network: pick a network, type the password on the touchscreen. No phone or computer needed.
3. **Upload page** (P14.7-P14.8). A small page served by Peepo for a phone or laptop on the same network: drop files in, without a stick.
4. **Tone3000** (P14.9-P14.10). Research first; built only if their terms allow it, and then on the upload page, not the pedal screen.

None of it may delay boot, the startup patch or the audio ([P10.3](10-technical-implementation-plan.md)).

## USB stick

**What the player does.** Plug in a stick. Settings gets a **Files** pill (beside Controls, MIDI, Diagnostics, Power); its page shows the stick and what Peepo found on it, by kind: models, cabinet IRs, reverb IRs. **Import** copies them; then the page says the stick can be pulled out. The same page lists what is already on Peepo, by folder, with **Delete** (a saved patch keeps its own copy, so deleting cannot break it). Nothing appears on the patch page.

**Where files go.**
- `.nam` → `nam/`.
- `.wav`, `.flac` in a folder whose name contains `cab` → `ir/cab/`; containing `reverb` or `room` → `ir/reverb/`.
- Any other `.wav` or `.flac`: the page asks, once for the whole lot, Cabinet or Reverb. Cabinet IRs are short; Peepo can suggest by length (under about 0.5 s cabinet) but the player decides.
- Subfolders are flattened; a name already on Peepo with the same size is skipped, a different file with the same name gets ` (2)`.

**How.**
- A udev rule starts `peepo-usb@<device>.service` (root, small) when a USB disk partition appears. It mounts it **read-only**, `nosuid,nodev,noexec`, at `/run/peepo-usb/<device>` (its own directory: `/run/peepo` is the engine's and goes when the engine stops), and writes `/run/peepo-usb/<device>.info` for the interface: mounted, unreadable or refused, the stick's name and why. FAT32, exFAT and NTFS (kernel `ntfs3`); anything else is reported as unreadable with its format. A disk with anything mounted elsewhere, such as the one Linux runs from, is refused. Removal unmounts.
- The interface reads the mount point, the service does nothing else. Copying runs in the interface's own worker thread at idle I/O priority (`ioprio` idle) into a temporary name, `fsync`, then rename, so a pulled stick or a power cut never leaves half a model in the folder.
- Each file is checked before it is offered: `.nam` is JSON with `architecture` and `weights`, under 10 MB; `.wav` / `.flac` have a valid header, 1-4 channels, under 30 s. Rejected files are listed with the reason. The copy is checked again before it is renamed, since the stick may have changed after it was looked at, and an import keeps 64 MB free on the card. Hidden files (macOS's `._` files), Windows' system folders and anything deeper than 8 folders are passed over.

**Power.** The Pi already reports undervoltage (`0x50005`, [instrument boot](24-instrument-boot.md)) with the Tascam and the touchscreen on USB, and in P13.10 plugging a USB device made the supply sag and the audio interface drop out until a restart. A stick draws more. Measure with the stick plugged and copying, and say so on the page if the supply cannot carry it.

### On the screen: Settings → Files

Built 30 October 2026 from the approved mockups (`images/mockup-files-1-stick.png`, `-2-cabinet-or-reverb.png`, `-3-delete.png`), like every other page: header pills, one list of bars two to a row, nothing on the patch page.

- **Header:** **Import** (white; grey until there is something new and the question is answered) and **Delete**. While copying only **Stop**; while Delete waits for a tap only **Cancel**.
- **USB stick:** its name and format (FAT32, exFAT, NTFS) and how many files are new; *LOOKING* while it is looked through, *COPYING n OF m* while importing. A stick Peepo cannot read, or refuses, shows peepo-usb's reason.
- **Its files** by kind, each with size (models) or length (IRs) and its folder; *On Peepo already* for the same name and size. **Cabinet or reverb?** asks once for the unsorted IRs, saying how many are under half a second. **Not taken** lists refused files with the reason. At most 40 per group are shown; Import takes them all.
- **On Peepo:** models, cabinet IRs and reverb IRs by folder, with sizes. With Delete lit only these show; a tap asks first.
- Plugging a stick in says so in the footer on any page; the end of an import sums it up there and says the stick can be pulled out.

**Later, not this phase:** export patches to the stick as a backup (the only time the stick would be mounted for writing).

## Wi-Fi on the pedal

Settings → **Network** (the network rows move from Settings to this page; Settings keeps one line):
- The current network, address and signal, and **Disconnect**.
- **Known networks**, each with **Forget**.
- **Nearby networks** from a scan, strongest first, lock shown. Tap one: the on-screen keyboard asks for the password, with a show/hide toggle. Join, then the result ("Connected", "Wrong password", "No answer").
- New connections get `autoconnect-retries 0`, as the home network has now, so a weak signal never gives up.

**How.** NetworkManager over D-Bus (`nmcli` is enough for a first version). A polkit rule, like `50-peepo-power.rules`, grants the `peepo` user the NetworkManager actions for Wi-Fi only (`wifi.share`, `network-control`, `settings.modify.system`) and nothing else. Scans run only while the Network page is open; NetworkManager scans in the background anyway, and a scan while playing is checked for xruns.

**Fallback without a keyboard or screen**: not needed while the instrument has its touchscreen. Your phone's hotspot can be added as a known network today, so it is online anywhere.

## Upload page

A small HTTP server in its own service (`peepo-files`, not the interface or engine), on the local network only:
- **Off by default.** Turned on from Settings → Files; the page shows the address (`http://peepo.local` and the IP) and a **4-digit code**, new each time it is turned on and shown only on the pedal. A browser that entered the code stays allowed until the page turns off: after 15 minutes idle, from Settings, or at reboot. Wrong codes are slowed down after three tries.
- One page: what is on Peepo by folder, delete, and drop files in. Uploads go through the same checks and the same temporary-then-rename copy as the stick.
- No patches, no settings, no shell: only the three folders.
- Runs at low priority; checked for xruns while a file uploads during playing.

**No hotspot of its own** (decided below): with no Wi-Fi around, the phone's hotspot is the network.

## Tone3000

Before building anything:
- Does Tone3000 offer a public API for search and download, and on what terms? Whether a device may download on a user's behalf, whether it needs the user's own account, and what attribution it asks for.
- How others do it (the Reddit projects): official API, or scraping. Peepo does not scrape.

If it is allowed: search and download on the **upload page** (a phone is good at search and typing; the pedal screen is not), saved through the same checks into `nam/` or `ir/`. The pedal screen shows only the result: the new files in the picker.

## Decided with the user (30 October 2026)

1. **Order:** USB first, then Wi-Fi on the pedal, then the upload page, then Tone3000.
2. **Stick folders:** "cab" or "reverb"/"room" in the folder name decides; anything else is asked once for the lot.
3. **Upload page:** a new 4-digit code each time it is turned on, shown only on the pedal's screen, so being at the pedal is the permission. A browser that entered it stays allowed until the page turns off. No fixed password to remember or reset.
4. **Hotspot fallback:** not built. The stick covers having no Wi-Fi; the phone's hotspot, added as a known network, gives the upload page a network anywhere. Revisit only if that proves awkward.
5. **Patch export to the stick:** later, not in this phase.

## Steps

P14.1-P14.10 and their records are in [Phase 14](10-technical-implementation-plan.md#phase-14--files-and-network).

**Exit check:** away from home, with no computer, a player joins a new Wi-Fi from the pedal, imports a model from a stick and an IR from a phone, plays them in a patch and saves it, and none of it interrupts the sound.
