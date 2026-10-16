#include "autoblow.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>

#include <string>

#include "settings.h"

namespace autoblow {

static SemaphoreHandle_t mtx;
static TaskHandle_t task;
static Desired want;
static Status stat;
static bool resyncReq;

// What the device was last told. -1 = unknown.
struct Sent {
  int want = -1;  // Want, or -1
  int speed = -1;
  int script = -1;
  int scriptPlaying = -1;
  int valvePlus = -1, valveMinus = -1;
};

static esp_http_client_handle_t http;
static std::string body;
static std::string host;  // cluster base URL, empty until discovered

struct Lock {
  Lock() { xSemaphoreTake(mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(mtx); }
};

Desired desired() {
  Lock l;
  return want;
}

void setDesired(const Desired &d) {
  {
    Lock l;
    want = d;
  }
  if (task) xTaskNotifyGive(task);
}

void resync() {
  {
    Lock l;
    resyncReq = true;
  }
  if (task) xTaskNotifyGive(task);
}

Status status() {
  Lock l;
  return stat;
}

static void setError(const char *fmt, ...) {
  Lock l;
  char msg[sizeof stat.error];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  if (strcmp(msg, stat.error)) Serial.printf("autoblow: %s\n", msg);
  strcpy(stat.error, msg);
}

// ---------- HTTP ----------

static esp_err_t onHttpEvent(esp_http_client_event_t *e) {
  if (e->event_id == HTTP_EVENT_ON_DATA && body.size() < 4096)
    body.append((const char *)e->data, e->data_len);
  return ESP_OK;
}

static const char *netErrorText(esp_err_t err) {
  switch (err) {
    case ESP_ERR_HTTP_CONNECT: return "can't reach Autoblow (internet down?)";
    case ESP_ERR_HTTP_EAGAIN:
    case ESP_ERR_TIMEOUT: return "Autoblow not answering (timeout)";
    case ESP_ERR_HTTP_FETCH_HEADER:
    case ESP_ERR_HTTP_CONNECTION_CLOSED:
    case ESP_ERR_HTTP_WRITE_DATA: return "connection to Autoblow dropped";
    default: return esp_err_to_name(err);
  }
}

// Returns the HTTP status, or -1 on a network error. Response body in `body`.
static int request(esp_http_client_method_t method, const std::string &url,
                   const char *json = nullptr) {
  body.clear();
  esp_http_client_set_url(http, url.c_str());
  esp_http_client_set_method(http, method);
  esp_http_client_set_header(http, "x-device-token", settings.token.c_str());
  // The API wants no Content-Type on requests without a body.
  if (json) {
    esp_http_client_set_header(http, "Content-Type", "application/json");
    esp_http_client_set_post_field(http, json, strlen(json));
  } else {
    esp_http_client_delete_header(http, "Content-Type");
    esp_http_client_set_post_field(http, nullptr, 0);
  }
  esp_err_t err = esp_http_client_perform(http);
  if (err != ESP_OK) {
    setError("%s", netErrorText(err));
    esp_http_client_close(http);
    return -1;
  }
  return esp_http_client_get_status_code(http);
}

static void parseState() {
  JsonDocument doc;
  if (deserializeJson(doc, body)) return;
  if (!doc["operationalMode"].is<const char *>()) return;
  Lock l;
  strlcpy(stat.mode, doc["operationalMode"] | "", sizeof stat.mode);
  stat.targetSpeed = doc["targetSpeed"] | 0;
  stat.valvePlus = doc["strokePlusValve"] | false;
  stat.valveMinus = doc["strokeMinusValve"] | false;
  stat.localScript = doc["localScript"] | 0;
}

static void setOnline(bool on) {
  Lock l;
  stat.online = on;
  if (on) stat.error[0] = 0;
}

static bool discover() {
  int code = request(HTTP_METHOD_GET, "https://latency.autoblowapi.com/vacuglide/connected");
  if (code != 200) {
    if (code > 0) setError("discover: HTTP %d", code);
    return false;
  }
  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  if (!(doc["connected"] | false)) {
    setError("VacuGlide offline: hold its mode button 2.5 s");
    return false;
  }
  std::string c = doc["cluster"] | "";
  for (const char *p : {"https://", "http://"})
    if (c.rfind(p, 0) == 0) c = c.substr(strlen(p));
  if (c.find('.') == std::string::npos) c += ".autoblowapi.com";
  host = "https://" + c;
  Serial.printf("VacuGlide on %s\n", host.c_str());
  return true;
}

// ---------- sync loop ----------

// One step toward the desired state. Returns false if nothing to do.
static bool step(const Desired &d, Sent &s) {
  char json[48];
  int code = 0;
  auto put = [&](const char *path, const char *j = nullptr) {
    code = request(HTTP_METHOD_PUT, host + path, j);
    return code;
  };

  // 1. Stop first, so STOP is never queued behind anything else.
  if (d.want == Want::Idle && s.want != (int)Want::Idle) {
    if (s.want != (int)Want::Script) put("/vacuglide/target-speed/stop");
    if (code < 0) return true;
    if (s.want != (int)Want::Speed) put("/vacuglide/local-script/stop");
    if (code < 0) return true;
    s.want = (int)Want::Idle;
    s.scriptPlaying = -1;
  }
  // 2. Valves.
  else if (s.valvePlus != d.valvePlus) {
    snprintf(json, sizeof json, "{\"valveState\":%s}", d.valvePlus ? "true" : "false");
    if (put("/vacuglide/valve/stroke-plus", json) >= 0) s.valvePlus = d.valvePlus;
  } else if (s.valveMinus != d.valveMinus) {
    snprintf(json, sizeof json, "{\"valveState\":%s}", d.valveMinus ? "true" : "false");
    if (put("/vacuglide/valve/stroke-minus", json) >= 0) s.valveMinus = d.valveMinus;
  }
  // 3. Speed.
  else if (d.want == Want::Speed && (s.want != (int)Want::Speed || s.speed != d.speed)) {
    snprintf(json, sizeof json, "{\"targetSpeed\":%d}", d.speed);
    if (put("/vacuglide/target-speed", json) >= 0) {
      s.want = (int)Want::Speed;
      s.speed = d.speed;
    }
  }
  // 4. Local scripts. Selecting one starts it and closes both valves.
  else if (d.want == Want::Script && (s.want != (int)Want::Script || s.script != d.script)) {
    snprintf(json, sizeof json, "{\"localScriptIndex\":%d}", d.script);
    if (put("/vacuglide/local-script", json) >= 0) {
      s.want = (int)Want::Script;
      s.script = d.script;
      s.scriptPlaying = 1;
      s.valvePlus = s.valveMinus = 0;
    }
  } else if (d.want == Want::Script && s.scriptPlaying != d.scriptPlaying) {
    if (put(d.scriptPlaying ? "/vacuglide/local-script/start" : "/vacuglide/local-script/stop") >= 0)
      s.scriptPlaying = d.scriptPlaying;
  } else {
    return false;
  }

  if (code >= 200 && code < 300) {
    parseState();
    setOnline(true);
  } else if (code == 502) {
    // Device dropped off the cloud: rediscover and resend everything.
    setError("VacuGlide went offline");
    setOnline(false);
    host.clear();
    s = Sent();
  } else if (code == 429) {
    setError("rate limited");
    vTaskDelay(pdMS_TO_TICKS(1000));
  } else if (code > 0) {
    // Other 4xx (e.g. script start in the wrong mode): recorded as sent, not retried.
    setError("HTTP %d", code);
    Serial.printf("HTTP %d: %s\n", code, body.c_str());
  } else {
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  return true;
}

static void netTask(void *) {
  esp_http_client_config_t cfg = {};
  cfg.url = "https://latency.autoblowapi.com/";
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.keep_alive_enable = true;
  cfg.timeout_ms = 8000;
  cfg.event_handler = onHttpEvent;
  http = esp_http_client_init(&cfg);

  Sent sent;
  uint32_t lastPoll = 0;

  for (;;) {
    bool wifi = WiFi.status() == WL_CONNECTED;
    {
      Lock l;
      if (wifi != stat.wifi)
        Serial.printf("Wi-Fi %s %s\n", wifi ? "up" : "down", wifi ? WiFi.localIP().toString().c_str() : "");
      stat.wifi = wifi;
      if (resyncReq) {
        resyncReq = false;
        sent = Sent();
      }
    }
    if (!wifi) {
      setOnline(false);
      host.clear();
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    if (host.empty()) {
      if (!discover()) {
        setOnline(false);
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(3000));
        continue;
      }
      setOnline(true);
      sent = Sent();
    }
    if (step(desired(), sent)) continue;

    // Nothing to send: poll state now and then, otherwise wait for a change.
    if (millis() - lastPoll > 3000) {
      lastPoll = millis();
      int code = request(HTTP_METHOD_GET, host + "/vacuglide/state");
      if (code == 200) {
        parseState();
        setOnline(true);
      } else if (code == 502) {
        setOnline(false);
        host.clear();
      }
      continue;
    }
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250));
  }
}

void begin() {
  mtx = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(netTask, "autoblow", 8192, nullptr, 5, &task, 0);
}

}  // namespace autoblow
