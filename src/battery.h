// Battery level from a MAX17048 / MAX17043 fuel gauge (address 0x36). The board's
// IP5306 charger has no data bus, so without a gauge the level is unknown.
// The gauge may sit on the touch bus (GPIO19/20) or on the P4 connector (GPIO17/18);
// both are probed, and again every 30 s until one answers (so it can be plugged in later).
#pragma once

namespace battery {

struct Info {
  bool present = false;  // a gauge answered
  float percent = 0;     // state of charge, 0..100
  float volts = 0;       // cell voltage
  bool charging = false; // MAX17048 only (from its charge-rate register)
  const char *chip = ""; // "MAX17048" / "MAX17043"
};

void begin();
void loop();  // call from loop(); reads every few seconds
Info info();

}  // namespace battery
