#include "control.h"

#include <Arduino.h>
#include <lvgl.h>

#include <vector>

#include "autoblow.h"

namespace control {

using autoblow::Want;

static Active act = Active::None;
static bool plusHeld, minusHeld;

// Vacuum maintenance pulse: briefly opens the Stroke- valve.
static bool pulseOpen;
static uint32_t pulseUntil;

// ---------- remote ----------
// Vacuum maintenance: open the Stroke- valve for pulseMs every intervalMs.
struct Vac { uint16_t pulseMs; uint16_t intervalMs; };
static const Vac REMOTE_VAC[] = {{0, 0}, {300, 12000}, {400, 9000}, {500, 6000}};  // off low medium high
static int rSpeed = 20, rVac = 0;
static bool finishing;  // "Finish Me" runs on the remote path without vacuum pulses
static uint32_t rLastPulse;

// ---------- autopilot ----------
// Our own edging generator (not the Autoblow web app's patterns): a session is
// WAVES waves. Each climbs from the level's floor to a peak, rides the peak,
// then drops back to the floor and rests. Peaks rise as the session goes on.
static const int WAVES = 8;
struct Level { int floor, ceiling; };
static const Level LEVELS[] = {{8, 25}, {10, 40}, {20, 70}, {35, 100}};  // warmup low medium high
struct EdgeStyle {
  uint16_t peakMinMs, peakMaxMs;  // time spent at the peak
  uint16_t restMinMs, restMaxMs;  // time at the floor after the drop
  uint8_t wobble;                 // random dip below the peak while holding it
};
static const EdgeStyle EDGE_STYLES[] = {
    {5000, 9000, 15000, 25000, 12},  // gentle: short peaks, long rests
    {10000, 16000, 10000, 15000, 8},  // moderate
    {15000, 25000, 5000, 9000, 4},  // intense: long peaks, short rests
};
// Vacuum maintenance while Autopilot runs: pulse length and interval (off low high).
struct ApVac { uint16_t pulseMs; uint16_t intervalMs; };
static const ApVac AP_VAC[] = {{0, 0}, {250, 4000}, {400, 2500}};

struct Point { int speed; uint32_t at; };
static std::vector<Point> script;
static bool apPlaying;
static int apIntensity = 2, apEdge = 1, apVac = 0;
static uint32_t apTime, apLastSuction;
static size_t apIndex, apLastSent;
static int apSpeed;

// ---------- local scripts ----------
static int sSlot = -1;
static bool sPlaying;

static bool wasOnline;

// ---------- desired state ----------

static void push() {
  autoblow::Desired d;
  switch (act) {
    case Active::Remote:
      d.want = Want::Speed;
      d.speed = rSpeed;
      break;
    case Active::Autopilot:
      d.want = Want::Speed;
      d.speed = apSpeed;
      break;
    case Active::Script:
      d.want = Want::Script;
      d.script = sSlot;
      d.scriptPlaying = sPlaying;
      break;
    case Active::None:
      break;
  }
  d.valvePlus = plusHeld;
  d.valveMinus = minusHeld || pulseOpen;

  autoblow::Desired cur = autoblow::desired();
  if (cur.want != d.want || cur.speed != d.speed || cur.script != d.script ||
      cur.scriptPlaying != d.scriptPlaying || cur.valvePlus != d.valvePlus ||
      cur.valveMinus != d.valveMinus)
    autoblow::setDesired(d);
}

static void startPulse(uint32_t ms) {
  pulseOpen = true;
  pulseUntil = millis() + ms;
}

// Leave whatever mode is running.
static void leave() {
  apPlaying = false;
  finishing = false;
  pulseOpen = false;
  act = Active::None;
}

// ---------- autopilot engine ----------

static int rnd(int lo, int hi) { return lo + esp_random() % (hi - lo + 1); }

static void generate() {
  const Level &lv = LEVELS[apIntensity];
  const EdgeStyle &es = EDGE_STYLES[apEdge];
  script.clear();
  uint32_t t = 0;
  auto add = [&](int speed, uint32_t ms) {
    script.push_back({constrain(speed, 5, 100), t});
    t += ms;
  };
  for (int w = 0; w < WAVES; w++) {
    float build = 0.6f + 0.4f * w / (WAVES - 1);  // first peak at 60 % of the range, last at 100 %
    int peak = lv.floor + lroundf((lv.ceiling - lv.floor) * build);
    int steps = rnd(4, 7);
    for (int i = 1; i <= steps; i++) add(lv.floor + (peak - lv.floor) * i / steps, rnd(2000, 3500));
    for (int held = rnd(es.peakMinMs, es.peakMaxMs); held > 0; held -= 2500)
      add(peak - rnd(0, es.wobble), 2500);
    add(lv.floor, rnd(es.restMinMs, es.restMaxMs));  // the edge: drop and rest
  }
  script.push_back({lv.floor, t});  // end of session
  apIndex = apLastSent = 0;
  apTime = 0;
}

static void autopilotTick() {
  if (apIndex >= script.size()) generate();  // session over: start a fresh one
  if (apTime >= script[apIndex].at) {
    apSpeed = script[apIndex].speed;
    apLastSent = apIndex++;
  }
  const ApVac &v = AP_VAC[apVac];
  if (v.pulseMs && apTime - apLastSuction >= v.intervalMs) {
    // A little shorter at high speed, where the stroke itself pulls more air.
    startPulse(lroundf(v.pulseMs * (1.2f - apSpeed / 250.0f)));
    apLastSuction = apTime;
  }
  apTime += 100;
}

// ---------- 100 ms timer ----------

static void tick(lv_timer_t *) {
  uint32_t now = millis();

  if (pulseOpen && (int32_t)(now - pulseUntil) >= 0) pulseOpen = false;

  if (act == Active::Autopilot && apPlaying) autopilotTick();

  if (act == Active::Remote && !finishing) {
    const Vac &v = REMOTE_VAC[rVac];
    if (v.pulseMs && !minusHeld && !pulseOpen && now - rLastPulse >= v.intervalMs) {
      startPulse(v.pulseMs);
      rLastPulse = now;
    }
  }

  // Lost the device while running: stop everything rather than resume later.
  bool online = autoblow::status().online;
  if (wasOnline && !online && act != Active::None) stopAll();
  wasOnline = online;

  push();
}

void begin() { lv_timer_create(tick, 100, nullptr); }

Active active() { return act; }

// ---------- remote ----------

void remoteStart() {
  leave();
  act = Active::Remote;
  rLastPulse = millis();
  push();
}

void remotePause() {
  if (act == Active::Remote) leave();
  push();
}

void remoteSetSpeed(int s) {
  rSpeed = constrain(s, 5, 100);
  if (act != Active::Remote || finishing) remoteStart();
  push();
}

int remoteSpeed() { return rSpeed; }

void remoteSetVacuum(int level) {
  rVac = constrain(level, 0, 3);
  if (!REMOTE_VAC[rVac].pulseMs) pulseOpen = false;
  rLastPulse = millis();
  push();
}

int remoteVacuum() { return rVac; }

// ---------- valves ----------

void strokePlus(bool held) {
  plusHeld = held;
  push();
}

void strokeMinus(bool held) {
  minusHeld = held;
  if (held) pulseOpen = false;
  else rLastPulse = millis();
  push();
}

// ---------- autopilot ----------

void autopilotStart() {
  leave();
  if (script.empty()) generate();
  act = Active::Autopilot;
  apPlaying = true;
  apSpeed = script[apLastSent].speed;
  push();
}

void autopilotPause() {
  if (act == Active::Autopilot) leave();
  push();
}

// Changing a setting starts a fresh session with it.
void autopilotSetIntensity(int l) { apIntensity = constrain(l, 0, 3); generate(); }
void autopilotSetEdge(int l) { apEdge = constrain(l, 0, 2); generate(); }
void autopilotSetVacuum(int l) {
  apVac = constrain(l, 0, 2);
  apLastSuction = 0;
  pulseOpen = false;
  push();
}
int autopilotIntensity() { return apIntensity; }
int autopilotEdge() { return apEdge; }
int autopilotVacuum() { return apVac; }
int autopilotSpeed() { return act == Active::Autopilot ? apSpeed : 0; }
int autopilotProgress() {
  if (script.empty() || !script.back().at) return 0;
  return min<uint32_t>(100, apTime * 100 / script.back().at);
}

void finishMe() {
  leave();
  plusHeld = minusHeld = false;
  act = Active::Remote;
  finishing = true;
  rSpeed = 100;
  push();
}

// ---------- local scripts ----------

void scriptPlay(int slot) {
  leave();
  act = Active::Script;
  sSlot = constrain(slot, 0, 29);
  sPlaying = true;
  push();
}

void scriptToggle() {
  if (act != Active::Script) return;
  sPlaying = !sPlaying;
  push();
}

int scriptSlot() { return act == Active::Script ? sSlot : -1; }
bool scriptPlaying() { return act == Active::Script && sPlaying; }

// ---------- stop ----------

void stopAll() {
  leave();
  plusHeld = minusHeld = false;
  push();
  autoblow::resync();
}

}  // namespace control
