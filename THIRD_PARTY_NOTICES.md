# Third-party notices

This project's own code is licensed under CC BY-NC-SA 4.0 (see `LICENSE`).
It builds on the components below, which PlatformIO downloads at build time.
They are not part of this repository and keep their own licenses.

## LVGL 9.5.0 — MIT

Copyright (c) 2025 LVGL Kft
https://github.com/lvgl/lvgl

## ArduinoJson 7 — MIT

Copyright © 2014-2026, Benoit BLANCHON
https://github.com/bblanchon/ArduinoJson

MIT License text (applies to both of the above):

> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

## Arduino core for ESP32 and ESP-IDF (via pioarduino)

- Arduino-ESP32: LGPL-2.1, https://github.com/espressif/arduino-esp32
- ESP-IDF: Apache-2.0, https://github.com/espressif/esp-idf

These are linked into the firmware you build. This repository ships source
only; if you distribute built firmware images, follow the LGPL-2.1 terms for
the Arduino core.

## Autoblow device API

The panel uses Autoblow's public device API
(https://developers.autoblow.com/). No Autoblow code or Autopilot patterns are
included; the Autopilot generator in `src/control.cpp` is this project's own.
