// Wi-Fi station: connect, keep retrying, and say in plain words why it is down.
#pragma once

namespace wlan {

struct Info {
  bool up = false;
  int rssi = 0;           // dBm, 0 while down
  const char *problem;    // why it is down, "" while up
};

void begin();  // connects with settings.wifiSsid / wifiPass
void loop();   // reconnect watchdog and RSSI sampling; call from loop()
Info info();

// Signal quality from RSSI: 0 (none) .. 4 (excellent).
int bars(int rssi);

}  // namespace wlan
