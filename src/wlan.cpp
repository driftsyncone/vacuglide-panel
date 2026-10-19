#include "wlan.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "secrets.h"
#include "settings.h"

namespace wlan {

// DHCP hostname and mDNS name (vgremote.local).
#ifndef PANEL_HOSTNAME  // set in secrets.h to tell several panels apart
#define PANEL_HOSTNAME "vgremote"
#endif
static const char *HOSTNAME = PANEL_HOSTNAME;

// Written by the Wi-Fi event task, read on the loop task.
static volatile int lastReason;  // 0 = none yet
static volatile bool gotIp;

static int rssi;
static uint32_t downSince, lastAttempt, lastSample;

static const uint32_t RETRY_MS = 20000;

static void onEvent(arduino_event_id_t id, arduino_event_info_t info) {
  if (id == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    gotIp = true;
    lastReason = 0;
  } else if (id == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    gotIp = false;
    int r = info.wifi_sta_disconnected.reason;
    Serial.printf("Wi-Fi disconnected, reason %d\n", r);
    // Our own disconnect() before a retry; keep the reason that got us here.
    if (r != WIFI_REASON_ASSOC_LEAVE) lastReason = r;
  }
}

static void connect() {
  lastAttempt = millis();
  WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPass.c_str());
}

void begin() {
  WiFi.setHostname(HOSTNAME);  // must come before mode()/begin()
  WiFi.persistent(false);  // no flash writes on connect (they glitch the display)
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // lower latency for valve pulses
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(onEvent);
  downSince = millis();
  if (settings.wifiSsid.length()) connect();
}

void loop() {
  uint32_t now = millis();
  bool up = WiFi.status() == WL_CONNECTED;
  if (up) {
    static bool mdns;
    if (!mdns && MDNS.begin(HOSTNAME)) {
      MDNS.addService("http", "tcp", 80);
      mdns = true;
    }
    downSince = now;
    if (now - lastSample > 2000) {
      lastSample = now;
      rssi = WiFi.RSSI();
    }
    return;
  }
  rssi = 0;
  // Auto-reconnect gives up on some reasons (e.g. a failed handshake), so
  // start over from scratch every so often while down.
  if (settings.wifiSsid.length() && now - downSince > RETRY_MS && now - lastAttempt > RETRY_MS &&
      WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
    Serial.println("Wi-Fi: retrying");
    WiFi.disconnect();
    connect();
  }
}

static const char *reasonText(int r) {
  switch (r) {
    case 0: return "connecting...";
    case WIFI_REASON_NO_AP_FOUND:
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
      return "network not found";
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE:
      return "wrong password?";
    case WIFI_REASON_BEACON_TIMEOUT:
    case WIFI_REASON_MISSING_ACKS:
    case WIFI_REASON_DISASSOC_DUE_TO_INACTIVITY:
      return "signal lost";
    case WIFI_REASON_ASSOC_FAIL:
    case WIFI_REASON_ASSOC_TOOMANY:
    case WIFI_REASON_AP_INITIATED:
      return "router dropped the panel";
    default: {
      static char buf[32];
      snprintf(buf, sizeof buf, "disconnected (code %d)", r);
      return buf;
    }
  }
}

static bool fixable(int r) {
  switch (r) {
    case WIFI_REASON_NO_AP_FOUND:
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE:
      return true;
    default: return false;
  }
}

Info info() {
  Info i;
  i.up = WiFi.status() == WL_CONNECTED && gotIp;
  i.rssi = i.up ? rssi : 0;
  if (i.up) {
    i.problem = "";
  } else if (!settings.wifiSsid.length()) {
    i.problem = "not set up";
    i.fixable = true;
  } else {
    i.problem = reasonText(lastReason);
    i.fixable = fixable(lastReason);
  }
  return i;
}

int bars(int r) {
  if (r == 0) return 0;
  if (r >= -55) return 4;
  if (r >= -65) return 3;
  if (r >= -75) return 2;
  return 1;
}

}  // namespace wlan
