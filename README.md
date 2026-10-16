# vacuglide-panel

A standalone touchscreen remote for the **Autoblow VacuGlide**, running on a
**Guition JC8048W550** (ESP32-S3, 5" 800x480 touch display). It talks straight
to Autoblow's public device API with your device token. No phone, PC or
home-automation hub is needed once it's set up.

> Unofficial community project. Not made, endorsed or supported by Autoblow.
> "Autoblow" and "VacuGlide" are trademarks of their owners.

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

`ota.sh` sends to `OTA_HOST` from `secrets.h` (default `vgremote.local`, the
panel's mDNS name). Override with `PANEL_HOST=192.168.x.y` or a second argument.
If the panel sits on an isolated IoT network, allow your computer to reach it
on TCP port 80.

## How it's built

| File | What |
|---|---|
| `src/board.cpp` | RGB panel (esp_lcd, bounce buffer), GT911 touch, LVGL glue, backlight |
| `src/autoblow.cpp` | Network task: desired-state sync to the API, keep-alive HTTPS, state polling |
| `src/control.cpp` | Modes: remote, Autopilot generator, vacuum pulses, scripts, STOP |
| `src/ui.cpp` | LVGL screens |
| `src/wlan.cpp` | Wi-Fi connect, retry watchdog, disconnect reasons, signal, mDNS |
| `src/settings.cpp` | NVS: Wi-Fi + token (override secrets.h), screen timeout, script names |
| `src/ota.cpp` | Firmware upload endpoint (`POST /update`) |
| `src/lv_mem_psram.c` | LVGL heap in PSRAM, so widgets don't take internal RAM from TLS |

The UI only sets a *desired* state; a network task on the other core sends
whatever differs, one request at a time, and retries until it lands. STOP
forgets what was sent, so everything is resent.

## License

[CC BY-NC-SA 4.0](LICENSE): you may use, change and share this project for
**non-commercial** purposes, with credit, and anything you build from it must be
shared under the same license. Third-party components keep their own licenses;
see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
