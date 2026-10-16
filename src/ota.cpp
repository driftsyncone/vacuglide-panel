#include "ota.h"

#include <Arduino.h>
#include <Update.h>
#include <WebServer.h>

#include "control.h"
#include "secrets.h"

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

void otaBegin() {
  server.on("/update", HTTP_POST, onDone, onUpload);
  server.on("/", HTTP_GET, [] { server.send(200, "text/plain", "vacuglide-panel\n"); });
  server.begin();
}

void otaLoop() { server.handleClient(); }
