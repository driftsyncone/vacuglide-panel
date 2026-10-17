// VacuGlide remote panel: standalone, talks straight to the Autoblow cloud.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "autoblow.h"
#include "battery.h"
#include "board.h"
#include "control.h"
#include "ota.h"
#include "rollback.h"
#include "settings.h"
#include "ui.h"
#include "wlan.h"

void setup() {
  Serial.begin(115200);
  rollback::begin();  // first: arms the deadline for a fresh update
  settingsLoad();
  autoblow::begin();  // before the UI, which reads its status

  boardBegin();
  battery::begin();  // after boardBegin(): shares the touch I2C bus
  uiBegin();
  control::begin();

  wlan::begin();
  otaBegin();
  Serial.printf("free internal %u, largest block %u\n",
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
}

void loop() {
  lv_timer_handler();
  wlan::loop();
  battery::loop();
  rollback::loop();
#ifdef TEST_ROLLBACK_CRASH  // test builds only: crash 15 s after boot
  if (millis() > 15000) abort();
#endif
  otaLoop();
  delay(5);
}
