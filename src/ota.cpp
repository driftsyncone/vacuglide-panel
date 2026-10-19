#include "ota.h"

#include <Arduino.h>
#include <Update.h>
#include <WebServer.h>
#include <esp_app_desc.h>
#include <Wire.h>
#include <esp_system.h>

#include "battery.h"
#include "board.h"
#include "control.h"
#include "rollback.h"
#include "secrets.h"
#include "ui.h"
#include "version.h"

static WebServer server(80);
static bool authorized;  // for the upload in progress; cleared when it ends

static bool login() {
  if (server.authenticate("ota", OTA_PASSWORD)) return true;
  server.requestAuthentication();
  return false;
}

static void onUpload() {
  HTTPUpload &u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    authorized = server.authenticate("ota", OTA_PASSWORD);
    if (!authorized) return;
    Serial.printf("OTA: receiving %s\n", u.filename.c_str());
    {
      UiLock l;
      control::stopAll();
      uiShowUpdating(-1);
    }
    // Flash writes stall the display's PSRAM reads and scramble any picture,
    // so show the message, then keep the backlight off while writing.
    delay(1500);
    {
      UiLock l;
      boardBacklight(false);
    }
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Serial.printf("OTA: %s\n", Update.errorString());
  } else if (!authorized) {
    return;
  } else if (u.status == UPLOAD_FILE_WRITE) {
    Update.write(u.buf, u.currentSize);
  } else if (u.status == UPLOAD_FILE_END) {
    Update.end(true);
    Serial.printf("OTA: %u bytes, %s\n", u.totalSize, Update.hasError() ? Update.errorString() : "ok");
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    UiLock l;
    uiHideUpdating();
    boardBacklight(true);
  }
}

static void onDone() {
  bool ok = authorized;
  authorized = false;
  if (!ok || !login()) return;
  if (Update.hasError()) {
    {
      UiLock l;
      uiHideUpdating();
      boardBacklight(true);
    }
    server.send(500, "text/plain", String(Update.errorString()) + "\n");
    return;
  }
  {
    UiLock l;
    uiShowUpdating(100);  // "Updating firmware 100%": done, restarting
    boardBacklight(true);
  }
  server.send(200, "text/plain", "OK, rebooting\n");
  delay(300);
  ESP.restart();
}

// GET /screenshot[?tab=0..3]: the screen as a 16-bit BMP (login: it can switch
// tabs, and Settings shows Wi-Fi and the token's tail). The frame buffer is
// read without the lock: the UI keeps running, at the cost of a torn frame if
// something redraws meanwhile.
static void onScreenshot() {
  if (!login()) return;
  if (server.hasArg("tab")) {
    UiLock l;
    uiShowTab(server.arg("tab").toInt());
  }
  const uint16_t *fb = boardFrameBuffer();
  if (!fb) return server.send(500, "text/plain", "no frame buffer\n");

  const int W = 800, H = 480, ROW = W * 2;  // 1600 bytes, already 4-byte aligned
  const uint32_t HDR = 14 + 40 + 12, size = HDR + ROW * H;
  uint8_t h[HDR] = {'B', 'M'};
  auto put32 = [&](int at, uint32_t v) { memcpy(h + at, &v, 4); };
  put32(2, size);
  put32(10, HDR);
  put32(14, 40);
  put32(18, W);
  put32(22, H);  // positive height: rows bottom-up
  h[26] = 1;     // planes
  h[28] = 16;    // bits per pixel
  put32(30, 3);  // BI_BITFIELDS: RGB565 masks follow
  put32(34, ROW * H);
  put32(54, 0xF800);
  put32(58, 0x07E0);
  put32(62, 0x001F);

  server.setContentLength(size);
  server.send(200, "image/bmp", "");
  server.sendContent((const char *)h, HDR);
  if (!boardIsFlipped()) {
    for (int y = H - 1; y >= 0; y--) server.sendContent((const char *)(fb + y * W), ROW);
    return;
  }
  // Flipped: the frame buffer is upside down, so send it turned back.
  static uint16_t line[W];
  for (int y = 0; y < H; y++) {
    const uint16_t *src = fb + y * W;
    for (int x = 0; x < W; x++) line[x] = src[W - 1 - x];
    server.sendContent((const char *)line, ROW);
  }
}

// GET /i2c: which chips answer on the touch bus and on the P4 connector, plus the
// IP5306's status registers if it is the I2C variant (address 0x75). Diagnostic only.
static String scanBus(TwoWire &w) {
  String s;
  for (uint8_t a = 1; a < 127; a++) {
    w.beginTransmission(a);
    if (w.endTransmission() == 0) s += " 0x" + String(a, HEX);
  }
  return s.length() ? s : String(" (none)");
}

#if BOARD_HAS_P4_I2C
static int readReg(TwoWire &w, uint8_t addr, uint8_t reg) {
  w.beginTransmission(addr);
  w.write(reg);
  if (w.endTransmission(false) != 0 || w.requestFrom(addr, (uint8_t)1) != 1) return -1;
  return w.read();
}
#endif

static void i2cReport(void *arg) {
  String &out = *(String *)arg;
  out = String("board: " BOARD_NAME "\nmain I2C bus:") + scanBus(Wire) + "\n";
#if BOARD_HAS_P4_I2C  // Guition only: on the Waveshare GPIO17/18 drive the display
  for (uint8_t reg : {0x00, 0x01, 0x70, 0x71, 0x78}) {
    int v = readReg(Wire, 0x75, reg);
    out += "IP5306 reg 0x" + String(reg, HEX) + " = " + (v < 0 ? String("no answer") : "0x" + String(v, HEX)) + "\n";
  }
  if (battery::usesP4()) {
    out += "P4 pins (SDA 17, SCL 18): in use by the fuel gauge\n";
  } else {
    Wire1.begin(17, 18, 100000);
    out += "P4 pins (SDA 17, SCL 18):" + scanBus(Wire1) + "\n";
    Wire1.end();
  }
#endif
}

static void onI2c() {
  if (!login()) return;
  String out;
  uiCall(i2cReport, &out);  // on the loop task: the I2C driver lives there
  server.send(200, "text/plain", out);
}

// POST /restart: reboot from the network, e.g. when the screen is unusable.
// (No remote power off: cutting power from software can lock the IP5306 out
// until the battery is reconnected.)
static void onRestart() {
  if (!login()) return;
  server.send(200, "text/plain", "OK, restarting\n");
  UiLock l;
  control::stopAndRestart();  // after the reply has gone out
}

// GET /adc: raw readings of the battery ADC pin (for checking a divider / calibration).
static void adcReport(void *arg) {
  String &out = *(String *)arg;
#if BOARD_BATTERY_ADC_PIN >= 0
  out = "pin " + String(BOARD_BATTERY_ADC_PIN) + " mV:";
  for (int i = 0; i < 9; i++) {
    out += " " + String(analogReadMilliVolts(BOARD_BATTERY_ADC_PIN));
    delay(2);
  }
  out += "\nraw:";
  for (int i = 0; i < 5; i++) out += " " + String(analogRead(BOARD_BATTERY_ADC_PIN));
  out += "\n";
#else
  out = "no battery ADC on this board\n";
#endif
}

static void onAdc() {
  if (!login()) return;
  if (battery::usesP4()) return server.send(409, "text/plain", "GPIO17 is in use by the fuel gauge on P4\n");
  String out;
  uiCall(adcReport, &out);  // on the loop task, like battery::loop()
  server.send(200, "text/plain", out);
}

static const char *resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_DEEPSLEEP: return "deep-sleep wake";
    default: return "other";
  }
}

void otaBegin() {
  server.on("/adc", HTTP_GET, onAdc);
  server.on("/restart", HTTP_POST, onRestart);
  // POST /resync: restart the display output without rebooting.
  server.on("/resync", HTTP_POST, [] {
    if (!login()) return;
    {
      UiLock l;
      boardDisplayResync();
    }
    server.send(200, "text/plain", "OK, display resynced\n");
  });
  server.on("/i2c", HTTP_GET, onI2c);
  server.on("/update", HTTP_POST, onDone, onUpload);
  server.on("/screenshot", HTTP_GET, onScreenshot);
  // Build time + probation state, so ota.sh can tell whether an update stuck.
  server.on("/", HTTP_GET, [] {
    // __DATE__/__TIME__ only change when this file recompiles; the ELF hash
    // changes with every build, so ota.sh compares that.
    char sha[17];
    esp_app_get_elf_sha256(sha, sizeof sha);
    server.send(200, "text/plain",
                String("vacuglide-panel " FW_VERSION "\nbuilt " __DATE__ " " __TIME__ "\nid: ") + sha +
                    "\nboard: " BOARD_NAME "\nfirmware: " + rollback::state() + "\ndisplay fixes: " +
                    boardDisplayGuard() + "\nreset: " + resetReason() + "\n");
  });
  server.begin();
  // Core 0, next to Wi-Fi and the Autoblow task; the UI has core 1 to itself.
  xTaskCreatePinnedToCore([](void *) {
    for (;;) {
      server.handleClient();
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }, "web", 8192, nullptr, 1, nullptr, 0);
}
