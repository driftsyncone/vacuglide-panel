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
  long the peaks and rests last. Changing a setting mid-session blends in
  (the waves are re-planned, the place in the session is kept). When a session
  ends, a new random one starts.
- **Scripts:** play any of the device's 30 local-script slots, pause/resume.
  Long-press a slot to give it a name (stored on the panel, since the API only
  knows slot numbers).
- **Settings:** Wi-Fi (with a network scan), password, device token (only its
  last 3 characters show), screen timeout, and **Flip screen** for mounting the
  panel the other way round (touch follows). Wi-Fi and token apply after
  "Save & restart" (the same button reads "Restart" when nothing changed).
  **Power off** explains the battery button (hold it 2 s; software can't cut
  the power on this board) and offers Screen off. The firmware version is
  shown at the bottom.
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

## Build your own

Everything below is the exact hardware this panel runs on. Prices are what we
paid or saw in 2026 and change often.

### Parts

| # | Part | Where | ~Price | Notes |
|---|---|---|---|---|
| 1 | **Guition JC8048W550C** — ESP32-S3-N16R8, 5" 800×480 IPS, **capacitive** touch | [AliExpress](https://www.aliexpress.us/item/3256807009269027.html), [Surenoo](https://www.surenoo.com/products/23291185) (Guition reseller) | $17–30 | Pick the **capacitive** option (see below) |
| 2 | **1S LiPo, 3.7 V 3000 mAh, size 103665** (≈10 × 36 × 65 mm) with a **JST 1.25 mm** 2-pin plug | [Amazon B0CG1TCXVZ](https://www.amazon.com/dp/B0CG1TCXVZ) (sold as a Miyoo Mini battery) | ~$10–15 | Plugs straight into the board's BAT socket |
| 3 | **Case**: xTouch Pro enclosure for the Guition 5" | [USB-C variant](https://makerworld.com/en/models/1127192-xtouch-pro-usb-hole-variant) (recommended) or [official base with pogo-pin charging](https://makerworld.com/en/models/1016156-xtouch-pro-official-base-for-guition-5inch-lcd) | print it | See "Case" below |
| 4 | USB-C cable + a **2 A or better** USB charger | anywhere | — | The board charges and runs at the same time |

> **Battery not available?** Look for a **Miyoo Mini compatible** replacement
> battery: they're the same size and plug. Still check that the red wire lands on
> **BAT+** before plugging in, since sellers don't always wire them the same way.

Optional, for a battery % on screen (see [Battery level](#battery-level-optional)):

| # | Part | Where | ~Price |
|---|---|---|---|
| 5 | Adafruit **MAX17048** fuel gauge, STEMMA QT | [Adafruit #5580](https://www.adafruit.com/product/5580) | $5.95 |
| 6 | STEMMA QT → male header cable (red 3.3 V, black GND, blue SDA, yellow SCL) | [Adafruit #4209](https://www.adafruit.com/product/4209) | $0.95 |
| 7 | 2 × **JST 1.25 mm ↔ JST-PH 2.0 mm** 2-pin adapters (the gauge has PH 2.0 ports; battery and board are 1.25 mm), or splice the wires | electronics/hobby shops | ~$5 |
| — | The **JST 1.25 mm 4-pin to female jumper ("Dupont") cable** that comes in the box with the board | included | — |

### Getting the right board

Listings often sell all three versions on one page and default to the wrong one.
It's the right board when:

1. It's **Guition JC8048W550** (printed on the back of the board).
2. You picked **capacitive touch** → **JC8048W550C**. Not N (no touch) or R (resistive).
3. **ESP32-S3-N16R8** ("8M PSRAM 16M FLASH"), **800×480**, **ST7262** display driver.
4. The back has a **BAT** socket at the top right, next to the speaker socket.
   Look-alikes such as the Sunton ESP32-8048S050 may not have the battery circuit.

Guition's spec sheet: [JC8048W550 specifications (PDF)](https://www.guition.com/-download).

### What's on the board

- **Power:** an **IP5306** chip charges the battery from USB-C (~2 A), boosts it to
  5 V, runs the panel while charging, and cuts off when the cell is low.
  About 320 mA at 5 V while running.
- **BAT socket (P7):** JST 1.25 mm, **BAT− left, BAT+ right** (looking at the back,
  socket at the top). Check your battery's red wire goes to BAT+ before plugging in;
  a reversed cell can destroy the IP5306.
- **Battery button:** next to the P5 connector. A short press switches the panel on
  when it runs from the battery alone.
- **P4 connector** (from the community pinout; check the silkscreen): GND, 3.3 V,
  GPIO17, GPIO18. The panel uses it for the optional fuel gauge.
- Touch (GT911) is on I2C GPIO19/20; backlight on GPIO2.

### Case

The [xTouch Pro](https://makerworld.com/en/models/1127192-xtouch-pro-usb-hole-variant)
cases were made for this exact board (by the xTouch printer-screen project) and
print on any FDM printer.

- **USB-C variant (recommended):** the USB-C port stays reachable, so you can
  charge and, if ever needed, flash firmware over USB without opening the case.
- **Official base with pogo pins:** charges through magnetic pogo pins on a stand;
  needs the pogo-pin parts listed on its page, and the case has to come apart to
  reach USB.

Check the case's battery space fits your cell (the one above is ≈10 × 36 × 65 mm),
and keep the cell away from the IP5306 and its inductor (top right, near BAT): they
get warm while charging.

> **Our own cases are in the works.** Neither community case has room for the
> battery *and* the fuel gauge, so this repo now carries a parametric design for
> both boards in [`case/`](case/README.md): flat back, battery cradle, gauge
> bracket, USB-C and the power button reachable. Fit tests are being printed;
> until they pass, the cases above are the safe choice.

### Assembly

1. **Flash first** ([Flash the firmware](#flash-the-firmware) below) while the board is still bare on your desk.
2. Check battery polarity (red → BAT+), then plug the battery into **BAT**.
3. With USB unplugged, **press the battery button** briefly: the panel should start
   on battery. Plug USB-C in: it keeps running and charges. **Hold** the button
   for more than 2 s to cut the power (it stops the panel, not the VacuGlide).
4. Leave it charging for ~10 minutes and feel the battery: warm is fine, **hot means
   unplug** (the charger runs at ~2 A, about 0.7C for a 3000 mAh cell).
5. Fit the board and battery in the case.

### Battery level (optional)

Two ways, the gauge being the better one:

- **Fuel gauge** (below): exact, tracks charging, no soldering.
- **Voltage divider on GPIO17:** two resistors, BAT+ -33 kΩ- IO17 -100 kΩ- GND,
  with IO17 and GND on the P4 header and BAT+ tapped at the battery connector.
  The firmware turns the voltage into a percentage with a LiPo curve: a few
  percent low under load, and it sits at 100 % while charging. `GET /adc` shows
  the raw readings. (The Sunton ESP32-8048S050 has this divider on the board;
  the Guition JC8048W550C doesn't.)

The IP5306 has no data connection, so without extra hardware the panel can't
show the battery level (it runs fine without it). A MAX17048 (or MAX17043)
fuel gauge adds a battery % under the Wi-Fi signal, voltage on the Settings tab,
and a safety stop: **at 5 % or less on battery** the panel stops the VacuGlide
and won't run it until USB is plugged in, so it can't die and leave the device
running.

Wiring with the parts above:

1. Battery → adapter → the gauge's JST-PH port; the gauge's other JST-PH port →
   adapter → board **BAT**. The gauge passes power straight through.
2. Plug the board's included 4-pin cable into **P4**, and push the male pins of
   cable #6 (from the gauge's STEMMA QT port) into its female ends:
   red → 3.3 V, black → GND, blue (SDA) → GPIO17, yellow (SCL) → GPIO18.
   Go by the pin labels printed next to P4 on the board, not by the included
   cable's wire colours, which don't follow a standard.
   The touch bus (SDA GPIO19, SCL GPIO20) works too.

The panel finds the gauge on either bus by itself. On the touch bus it keeps
looking (plug it in whenever); on P4 it only looks at boot, because GPIO17 is
also the battery ADC pin. `GET /i2c` shows what answers (the gauge is at
address 0x36).

> Gauge support is new and not yet tried with a real MAX17048; the display was
> tested with simulated readings. Reports welcome.

### Alternative board: Waveshare ESP32-S3-Touch-LCD-5

If the Guition board isn't available to you, the firmware also builds for the
[Waveshare ESP32-S3-Touch-LCD-5](https://www.waveshare.com/esp32-s3-touch-lcd-5.htm)
([on Amazon](https://www.amazon.com/waveshare-Capacitive-Development-Dual-Core-Resolution/dp/B0DD7N19FT)).
Get the **800×480** touch version, **not** the 1024×600 "5B".

Tested on a real board: picture, touch, Wi-Fi and Wi-Fi updates work. Its
battery side hasn't been tried yet.

How it differs from the Guition build:

- **Build target:** `pio run -e waveshare5 -t upload` (USB) and
  `pio run -e waveshare5-ota -t upload` (Wi-Fi; address from `OTA_HOST_WS` in
  `secrets.h`, default `vgremote-ws.local`). Its network name is
  **`vgremote-ws`**, so it doesn't clash with a Guition panel.
- **Battery:** its own charger (CS8501, about 560 mA, so slower and gentler than
  the Guition's ~2 A) and a battery **ON/OFF switch**. Waveshare: the screen draws
  about 550 mA, so **switch the battery OFF when it's plugged into a computer**
  whose USB port gives less than 500 mA. Waveshare recommends cells up to 2000 mAh
  (at 560 mA a 3000 mAh cell takes about 6 hours to charge). Its docs call the
  socket MX1.25; check your battery's plug and polarity before connecting.
- **No battery %:** there's no battery measurement on the board, and a fuel gauge
  can't be added (its IO-expander chip answers on the gauge's I2C address).
- **Backlight:** on/off only (through the board's CH422G IO expander), so screen
  sleep works but there's no dimming.
- **Case:** a community case, [HexCase](https://makerworld.com/models/3010625), fits
  the touch versions (check it has room for a battery). Our own flat-backed case
  for this board is in [`case/`](case/README.md), fit test pending.

## Flash the firmware

You need [PlatformIO](https://platformio.org/) (CLI or the VS Code extension).

1. `cp include/secrets.example.h include/secrets.h` and fill it in. Wi-Fi and
   token are only defaults; you can also leave them and set them on the
   panel's Settings tab later.
2. Plug the panel in by USB (shows up as `/dev/ttyUSB0` on Linux).
3. `pio run -t upload && pio device monitor` (Guition). For the Waveshare board:
   `pio run -e waveshare5 -t upload`.

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
switch tabs first: 0 Remote, 1 Autopilot, 2 Scripts, 3 Settings (which shows
your Wi-Fi name and the token's last 3 characters). `GET /i2c` lists the chips
answering on the touch bus and the P4 connector, `GET /adc` the raw battery ADC
readings. `POST /restart` (same login) restarts the panel when the screen is
unusable, e.g. `curl -u ota:PASSWORD -X POST http://vgremote.local/restart`,
and `POST /resync` restarts only the display output. `GET /` shows the version,
build id, board, firmware state and the reason for the last reset.

**Security, plainly:** the Wi-Fi password, device token and OTA password from
`secrets.h` sit unencrypted in the firmware image and in the panel's flash, and
updates travel over plain HTTP on your LAN with Basic auth. Fine for a device on
your own network; don't publish a built `firmware.bin`, and treat the panel's
USB port like the token itself.

## How it's built

| File | What |
|---|---|
| `src/board.cpp` | Per-board pins and timings (Guition / Waveshare), RGB panel, GT911 touch, LVGL glue, backlight |
| `src/autoblow.cpp` | Network task: desired-state sync to the API, keep-alive HTTPS, state polling |
| `src/control.cpp` | Modes: remote, Autopilot generator, vacuum pulses, scripts, STOP |
| `src/ui.cpp` | LVGL screens |
| `src/wlan.cpp` | Wi-Fi connect, retry watchdog, disconnect reasons, signal, mDNS |
| `src/battery.cpp` | Battery level: MAX17048/MAX17043 gauge on either I2C bus, or a voltage divider on the ADC pin |
| `src/settings.cpp` | NVS: Wi-Fi + token (override secrets.h), screen timeout, script names |
| `src/ota.cpp` | Firmware upload (`POST /update`), `/restart`, `/resync`, `/screenshot`, `/i2c`, `/adc`, status (`/`) |
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
