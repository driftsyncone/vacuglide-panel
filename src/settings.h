// Panel settings in NVS. Wi-Fi and token default to secrets.h until the
// Settings tab saves its own.
#pragma once
#include <Arduino.h>

static const int SCRIPT_SLOTS = 30;

struct Settings {
  String wifiSsid;
  String wifiPass;
  String token;
  int sleepMin = 5;  // screen off after this many idle minutes, 0 = never
  bool flip = false;  // show the picture upside down (panel mounted the other way round)
  String scriptNames[SCRIPT_SLOTS];  // "" = unnamed
};

extern Settings settings;

void settingsLoad();
// Wi-Fi + token. They are only read at boot, so the caller restarts after.
void settingsSave();
void settingsSaveSleep();
void settingsSaveFlip();
void settingsSaveScriptName(int slot);
