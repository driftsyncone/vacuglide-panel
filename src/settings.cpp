#include "settings.h"

#include <Preferences.h>

#include "secrets.h"

Settings settings;

static const char *NS = "vgpanel";

static String nameKey(int slot) { return "sn" + String(slot); }

void settingsLoad() {
  Preferences p;
  p.begin(NS, true);
  settings.wifiSsid = p.getString("ssid", WIFI_SSID);
  settings.wifiPass = p.getString("pass", WIFI_PASS);
  settings.token = p.getString("token", DEVICE_TOKEN);
  settings.sleepMin = p.getInt("sleep", settings.sleepMin);
  for (int i = 0; i < SCRIPT_SLOTS; i++)
    settings.scriptNames[i] = p.getString(nameKey(i).c_str(), "");
  p.end();
}

void settingsSave() {
  Preferences p;
  p.begin(NS, false);
  p.putString("ssid", settings.wifiSsid);
  p.putString("pass", settings.wifiPass);
  p.putString("token", settings.token);
  p.end();
}

void settingsSaveSleep() {
  Preferences p;
  p.begin(NS, false);
  p.putInt("sleep", settings.sleepMin);
  p.end();
}

void settingsSaveScriptName(int slot) {
  Preferences p;
  p.begin(NS, false);
  String key = nameKey(slot);
  if (settings.scriptNames[slot].length()) p.putString(key.c_str(), settings.scriptNames[slot]);
  else p.remove(key.c_str());
  p.end();
}
