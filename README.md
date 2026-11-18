# Peepo

Peepo is an open-source modular effects instrument for the Raspberry Pi 5. It boots straight into a touchscreen patch editor: effects appear as blocks you connect with virtual cables, in series or in parallel, and you can play them with rotary encoders and footswitches.

The concept is inspired by Poly Effects' Beebo. The audio side uses Poly's Ingen-based engine and LV2 plugins, together with a large library of compatible open-source LV2 effects (Guitarix, Dragonfly, NAM, IR convolution, ports of Mutable Instruments modules, and more). The touchscreen interface is a new LVGL application written for Peepo. It contains no Beebo interface code or assets.

## Features

- Drag-and-drop patching on an 800 × 480 touchscreen, with no desktop environment
- Series and parallel routing, mono and stereo
- Hundreds of LV2 effects, browsable by category ([module list](docs/33-module-list.md))
- Neural Amp Modeler and impulse-response loading
- A tuner, global tempo, scenes and MIDI (USB and TRS)
- Patch storage with a configurable startup patch
- Assignable encoders and footswitches
- Direct boot into the instrument. Audio keeps playing if the interface restarts.
- A Windows simulator for working on the interface without a Pi

## What you need

| Part | Notes |
| --- | --- |
| Raspberry Pi 5 | 2 GB RAM or more, with a good 5 V / 5 A supply |
| microSD card | Running a 64-bit Debian-based OS (Raspberry Pi OS or similar) |
| Audio interface | Any USB class-compliant interface that ALSA supports |
| Touchscreen | 800 × 480. Touch over USB keeps the GPIO header free. |
| Controls (optional) | Push-button rotary encoders and momentary SPST footswitches wired to GPIO |
| Enclosure, jacks, wiring | As needed for your build |

## Get the source

```bash
git clone https://github.com/ABustos2606/Peepo.git
cd Peepo
```

The submodules under `reference/` hold upstream reference material only. You don't need them to build. See [reference/README.md](reference/README.md) if you want them.

## Build and install on the Pi

You can build the package either on the Pi itself or on a PC.

**On the Pi** (Debian / Raspberry Pi OS arm64, as root):

```bash
sudo bash tools/pi/build.sh
```

**On a PC** (inside Debian under WSL2 or Linux, as root). This builds for arm64 under emulation, so the first build is slow:

```bash
sudo bash tools/pi/build-arm64.sh
```

Either way, the build leaves a package in `build/pi/`. Copy it to the Pi, unpack it, and install:

```bash
sudo ./install.sh               # install the services and keep the previous version for rollback
sudo ./install.sh --enable --start  # also start now and at every boot
sudo ./install.sh --instrument  # also boot directly into Peepo (disables the desktop and competing audio services)
sudo ./install.sh --rollback    # restore the previous installation
sudo ./install.sh --remove      # uninstall
```

Full details: [Linux build, Pi packaging and services](docs/19-linux-and-pi.md) and [instrument boot](docs/24-instrument-boot.md).

## Configure your hardware

### Audio interface and screen

Edit `/etc/peepo/peepo.conf` (template: [config/pi/peepo.conf](config/pi/peepo.conf)). Later installs never overwrite it.

```ini
JACK_DRIVER=alsa
JACK_DRIVER_OPTIONS=-d hw:USB -r 48000 -p 128 -n 3   # use your card's name from `aplay -l`
AUDIO_IN_LEFT=system:capture_1
AUDIO_OUT_LEFT=system:playback_1
UI_OPTIONS=--display drm                             # add --rotate 180 or --touch /dev/input/by-id/... if needed
```

Set `-d hw:<name>` to match your interface. Adjust the sample rate (`-r`), period size (`-p`) and number of periods (`-n`) to trade latency against stability. Then run `sudo systemctl restart peepo.target`.

### Encoders and footswitches

Peepo reads physical controls as Linux input events, so you don't need an extra board:

1. Copy the overlay lines from [config/pi/config.txt](config/pi/config.txt) into `/boot/firmware/config.txt`. Change the GPIO pins to match your wiring, then reboot.
2. Map the controls in `/etc/peepo/controls.conf` (template: [config/pi/controls.conf](config/pi/controls.conf)). Use `evtest` to check the codes. Add `reverse` if a knob turns the wrong way.

Controls are optional, because everything also works from the touchscreen. See [physical controls](docs/18-physical-controls.md) for the suggested wiring and how the controls behave.

## Run the simulator on Windows

The simulator runs the real interface at 800 × 480 against a simulated engine. It makes no sound. It needs Visual Studio C++ Build Tools and CMake:

```powershell
cmake --preset windows
cmake --build --preset windows --parallel
ctest --preset windows
.\build\Debug\peepo.exe
```

The keyboard stands in for the hardware: **Q/A** and **W/S** turn the encoders, **1** and **2** press them, and **Z**, **X** and **C** are the footswitches. See [Windows UI development](docs/11-windows-ui-foundation.md).

## Documentation

- [Architecture](docs/02-architecture.md)
- [Touchscreen and controls](docs/03-interface-and-controls.md)
- [Module list](docs/33-module-list.md) and [module names](docs/37-module-names.md)
- [Patch storage](docs/17-patch-storage.md)
- [MIDI](docs/39-midi.md) and [scenes](docs/44-scenes.md)
- [Linux build, Pi packaging and services](docs/19-linux-and-pi.md)

The `docs/` folder holds the full design notes and development records.

## License

Original Peepo code and documentation are licensed under the **GNU GPL v3 or later**. See [LICENSE](LICENSE).

Third-party components keep their own licenses. For example, Poly's Ingen engine is AGPL-3.0-or-later. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
