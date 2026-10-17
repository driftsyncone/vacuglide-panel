#include "battery.h"

#include <Arduino.h>
#include <Wire.h>

namespace battery {

static const uint8_t ADDR = 0x36;
static const uint8_t REG_VCELL = 0x02, REG_SOC = 0x04, REG_VERSION = 0x08, REG_CRATE = 0x16;

static TwoWire *bus = nullptr;  // where the gauge answered, or null
static bool p4Started;
static Info cur;
static uint32_t lastRead, lastProbe;

static bool present(TwoWire &w) {
  w.beginTransmission(ADDR);
  return w.endTransmission() == 0;
}

static bool read16(uint16_t &out, uint8_t reg) {
  bus->beginTransmission(ADDR);
  bus->write(reg);
  if (bus->endTransmission(false) != 0 || bus->requestFrom(ADDR, (uint8_t)2) != 2) return false;
  out = (bus->read() << 8) | bus->read();
  return true;
}

static void probe() {
  lastProbe = millis();
  // Touch bus first: already running (board.cpp), shared with the GT911 at 0x5D.
  if (present(Wire)) {
    bus = &Wire;
  } else {
    if (!p4Started) p4Started = Wire1.begin(17, 18, 100000);
    if (p4Started && present(Wire1)) bus = &Wire1;
  }
  if (!bus) return;
  uint16_t ver = 0;
  read16(ver, REG_VERSION);
  // MAX17048/49 report 0x001x; the older MAX17043/44 report 0x000x.
  cur.chip = (ver & 0xFFF0) == 0x0010 ? "MAX17048" : "MAX17043";
  Serial.printf("battery: %s (version 0x%04x) on %s\n", cur.chip, ver, bus == &Wire ? "touch bus" : "P4");
}

static void read() {
  uint16_t v, soc;
  if (!read16(v, REG_VCELL) || !read16(soc, REG_SOC)) {
    // Unplugged or bus error: forget it and probe again later.
    bus = nullptr;
    cur = Info();
    return;
  }
  cur.present = true;
  cur.volts = v * 78.125e-6f;  // both chips: 78.125 uV per LSB
  cur.percent = constrain(soc / 256.0f, 0.0f, 100.0f);
  cur.charging = false;
  uint16_t crate;
  if (strcmp(cur.chip, "MAX17048") == 0 && read16(crate, REG_CRATE))
    cur.charging = (int16_t)crate * 0.208f > 0.5f;  // %/hour; above +0.5 %/h = charging
}

void begin() { probe(); }

void loop() {
#ifdef BATTERY_FAKE_PERCENT  // UI test builds only: -DBATTERY_FAKE_PERCENT=57
  cur.present = true;
  cur.percent = BATTERY_FAKE_PERCENT;
  cur.volts = 3.5f + BATTERY_FAKE_PERCENT * 0.007f;
  cur.chip = "fake";
  return;
#endif
  uint32_t now = millis();
  if (!bus) {
    if (now - lastProbe > 30000) probe();
    return;
  }
  if (now - lastRead > 5000 || !cur.present) {
    lastRead = now;
    read();
  }
}

Info info() { return cur; }

}  // namespace battery
