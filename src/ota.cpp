#include "ota.h"

#include <Arduino.h>
#include <Update.h>
#include <WebServer.h>
#include <Wire.h>

#include "board.h"
#include "control.h"
#include "rollback.h"
#include "secrets.h"
#include "ui.h"

static WebServer server(80);
static bool authorized;

static void onUpload() {
  HTTPUpload &u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    authorized = server.authenticate("ota", OTA_PASSWORD);
    if (!authorized) return;
    Serial.printf("OTA: receiving %s\n", u.filename.c_str());
    control::stopAll();
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (!authorized) {
    return;
  } else if (u.status == UPLOAD_FILE_WRITE) {
    Update.write(u.buf, u.currentSize);
  } else if (u.status == UPLOAD_FILE_END) {
    Update.end(true);
    Serial.printf("OTA: %u bytes, %s\n", u.totalSize, Update.hasError() ? Update.errorString() : "ok");
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
  }
}

static void onDone() {
  if (!authorized) return server.requestAuthentication();
  if (Update.hasError()) {
    server.send(500, "text/plain", String(Update.errorString()) + "\n");
    return;
  }
  server.send(200, "text/plain", "OK, rebooting\n");
  delay(300);
  ESP.restart();
}

// GET /screenshot[?tab=0..3]: the screen as a 16-bit BMP. Same login as /update,
// since the Settings tab shows the token. Runs on the loop task, like LVGL.
static void onScreenshot() {
  if (!server.authenticate("ota", OTA_PASSWORD)) return server.requestAuthentication();
  if (server.hasArg("tab")) uiShowTab(server.arg("tab").toInt());
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
  for (int y = H - 1; y >= 0; y--) server.sendContent((const char *)(fb + y * W), ROW);
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

static int readReg(TwoWire &w, uint8_t addr, uint8_t reg) {
  w.beginTransmission(addr);
  w.write(reg);
  if (w.endTransmission(false) != 0 || w.requestFrom(addr, (uint8_t)1) != 1) return -1;
  return w.read();
}

static void onI2c() {
  if (!server.authenticate("ota", OTA_PASSWORD)) return server.requestAuthentication();
  String out = "touch bus (SDA 19, SCL 20):" + scanBus(Wire) + "\n";
  for (uint8_t reg : {0x00, 0x01, 0x70, 0x71, 0x78}) {
    int v = readReg(Wire, 0x75, reg);
    out += "IP5306 reg 0x" + String(reg, HEX) + " = " + (v < 0 ? String("no answer") : "0x" + String(v, HEX)) + "\n";
  }
  Wire1.begin(17, 18, 100000);
  out += "P4 pins (SDA 17, SCL 18):" + scanBus(Wire1) + "\n";
  Wire1.end();
  server.send(200, "text/plain", out);
}

void otaBegin() {
  server.on("/i2c", HTTP_GET, onI2c);
  server.on("/update", HTTP_POST, onDone, onUpload);
  server.on("/screenshot", HTTP_GET, onScreenshot);
  // Build time + probation state, so ota.sh can tell whether an update stuck.
  server.on("/", HTTP_GET, [] {
    server.send(200, "text/plain", String("vacuglide-panel\nbuilt " __DATE__ " " __TIME__ "\nfirmware: ") +
                                       rollback::state() + "\n");
  });
  server.begin();
}

void otaLoop() { server.handleClient(); }
