// Copy to secrets.h and fill in. Baked in at build time as defaults; the
// panel's Settings tab can change Wi-Fi and the token later (stored in NVS).
#pragma once
#define WIFI_SSID    "your-ssid"
#define WIFI_PASS    "your-password"
#define DEVICE_TOKEN "your-autoblow-device-token"
#define OTA_PASSWORD "pick-a-password"  // for ./ota.sh uploads over Wi-Fi
#define OTA_HOST     "vgremote.local"   // where ./ota.sh sends updates (hostname or IP)
