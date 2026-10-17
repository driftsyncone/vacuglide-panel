# vacuglide-panel

A standalone touchscreen remote for the **Autoblow VacuGlide**, running on a
**Guition JC8048W550** (ESP32-S3, 5" 800x480 touch display). It talks straight
to Autoblow's public device API with your device token. No phone, PC or
home-automation hub is needed once it's set up.

> Unofficial community project. Not made, endorsed or supported by Autoblow.
> "Autoblow" and "VacuGlide" are trademarks of their owners.

| Remote | Autopilot | Scripts |
|---|---|---|
| ![Remote tab](docs/remote.png) | ![Autopilot tab](docs/autopilot.png) | ![Scripts tab](docs/scripts.png) |

<sub>Screenshots taken from the panel itself (`GET /screenshot`), with the VacuGlide switched off.</sub>

## What it does

- **Remote:** speed slider (5-100 %), Start/Pause, hold-to-open Stroke - /
  Stroke +, vacuum maintenance Off/Low/Medium/High.
- **Autopilot (edging):** an on-device generator, with Intensity, Edge,
  Vacuum and Finish Me. A session is 8 waves: each climbs to a peak, holds it,
  then drops to a rest. Peaks get higher as the session goes on; Edge sets how
  long the peaks and rests last. When a session ends, a new random one starts.
- **Scripts:** play any of the device's 30 local-script slots, pause/resume.
  Long-press a slot to give it a name (stored on the panel, since the API only
  knows slot numbers).
- **Settings:** Wi-Fi (with a network scan), password, device token, and
  screen timeout. Wi-Fi and token apply after "Save & restart".
- **Screen sleep:** backlight off after 1/5/15 idle minutes (default 5), only
  while nothing is running. The touch that wakes it does nothing else.
- **STOP** on every screen.
- **Connection problems, explained:** the top bar shows Wi-Fi signal strength
  and, when Wi-Fi is down, why (wrong password, network not found, signal lost).
  The panel retries from scratch every 20 s.

### Safety notes

- The panel controls the device through Autoblow's cloud. **If the panel loses
  its connection, it cannot stop the device.** It shows a red banner saying the
  VacuGlide may still be running, and sends STOP first as soon as it reconnects.
  Keep the device's own controls within reach.
- If the device drops off the cloud while running, the panel stops everything
  instead of resuming later.
- Saving settings or installing an update stops the device first.

## You need

- A Guition **JC8048W550** (ESP32-S3-N16R8 with 8 MB PSRAM, GT911 touch).
- An Autoblow **VacuGlide** connected to Wi-Fi, and its **device token**
  (see [Autoblow's developer docs](https://developers.autoblow.com/)).
- [PlatformIO](https://platformio.org/) (CLI or the VS Code extension).

## Build and flash

1. `cp include/secrets.example.h include/secrets.h` and fill it in. Wi-Fi and
   token are only defaults; you can also leave them and set them on the
   panel's Settings tab later.
2. Plug the panel in by USB (shows up as `/dev/ttyUSB0` on Linux).
3. `pio run -t upload && pio device monitor`

Back up the stock firmware first if you might want it back:
`esptool --port /dev/ttyUSB0 read-flash 0 0x1000000 stock.bin`

## Updates over Wi-Fi

`pio run -e ota -t upload` builds the firmware and sends it to the panel with
`./ota.sh` (HTTP basic auth: user `ota`, password `OTA_PASSWORD` from
`secrets.h`). The panel stops the VacuGlide, installs, and reboots.

**Safety net:** an update is written to the other firmware slot and only
switched to once it's complete and its checksum verifies, so a cut-off upload
leaves the old firmware running. The new firmware then runs on probation: it
must get onto Wi-Fi and run for 30 s. If it crashes before that, or isn't
confirmed within 3 minutes (frozen, no Wi-Fi), the panel goes back to the
previous firmware by itself. `ota.sh` waits and reports `OK: new firmware
confirmed` or `ROLLED BACK`. `GET /` shows the build time and `firmware:
confirmed/pending`. USB flashing always works as the last resort.

`ota.sh` sends to `OTA_HOST` from `secrets.h` (default `vgremote.local`, the
panel's mDNS name). Override with `PANEL_HOST=192.168.x.y` or a second argument.
If the panel sits on an isolated IoT network, allow your computer to reach it
on TCP port 80.

`GET /screenshot?tab=0..3` (same login) returns the screen as a BMP and can
switch tabs first: 0 Remote, 1 Autopilot, 2 Scripts, 3 Settings. Settings shows
your token, so don't share that one. `GET /i2c` lists the chips answering on the
touch bus and the P4 connector.

## Running on battery

The JC8048W550 has a battery input built in: an **IP5306** charger/boost chip,
the 2-pin **BAT** connector (P7, **BAT− left, BAT+ right**, check your cell's
polarity) and a power button. Use a single-cell (1S) protected LiPo. The IP5306
charges at about 2 A, so pick a cell rated for that (around 4000 mAh, or one
whose datasheet allows 1C charging). Press the button to start on battery;
USB-C charges it while the panel runs.

The IP5306 on this board has no data connection, so the panel can't read the
battery level by itself. Add a **MAX17048** (or MAX17043) fuel gauge, e.g.
Adafruit #5580, between the cell and BAT, and connect its I2C to either:

- the **P4** connector (GND, 3.3 V, SDA = GPIO17, SCL = GPIO18), or
- the touch bus (SDA = GPIO19, SCL = GPIO20).

(Gauge support is new and not yet tried with a real MAX17048; the display was
tested with simulated readings. Reports welcome.)

The panel finds it automatically (also when plugged in later) and then shows
the level under the Wi-Fi signal, a charging mark (MAX17048), and voltage on the
Settings tab. At **5 % or less on battery** it stops the VacuGlide and won't run
it until USB is plugged in, so the panel can't die and leave the device running.

## How it's built

| File | What |
|---|---|
| `src/board.cpp` | RGB panel (esp_lcd, bounce buffer), GT911 touch, LVGL glue, backlight |
| `src/autoblow.cpp` | Network task: desired-state sync to the API, keep-alive HTTPS, state polling |
| `src/control.cpp` | Modes: remote, Autopilot generator, vacuum pulses, scripts, STOP |
| `src/ui.cpp` | LVGL screens |
| `src/wlan.cpp` | Wi-Fi connect, retry watchdog, disconnect reasons, signal, mDNS |
| `src/battery.cpp` | MAX17048/MAX17043 fuel gauge: auto-detect on either I2C bus, level, voltage |
| `src/settings.cpp` | NVS: Wi-Fi + token (override secrets.h), screen timeout, script names |
| `src/ota.cpp` | Firmware upload (`POST /update`), `/screenshot`, `/i2c`, status (`/`) |
| `src/rollback.cpp` | Update probation: confirm after 30 s healthy on Wi-Fi, else roll back |
| `src/lv_mem_psram.c` | LVGL heap in PSRAM, so widgets don't take internal RAM from TLS |

The UI only sets a *desired* state; a network task on the other core sends
whatever differs, one request at a time, and retries until it lands. STOP
forgets what was sent, so everything is resent.

## License

[CC BY-NC-SA 4.0](LICENSE): you may use, change and share this project for
**non-commercial** purposes, with credit, and anything you build from it must be
shared under the same license. Third-party components keep their own licenses;
see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
