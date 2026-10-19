#include "control.h"

#include <Arduino.h>
#include <lvgl.h>

#include "autoblow.h"

namespace control {

using autoblow::Want;

static Active act = Active::None;
static bool plusHeld, minusHeld;

// Vacuum maintenance pulse: briefly opens the Stroke- valve. The network task
// times it (see autoblow::Desired); here it is just a counted request.
static uint32_t pulseSeq;
static uint16_t pulseMs;

// ---------- remote ----------
// Vacuum maintenance: open the Stroke- valve for pulseMs every intervalMs.
struct Vac { uint16_t pulseMs; uint16_t intervalMs; };  // also used by Autopilot below
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
static const Vac AP_VAC[] = {{0, 0}, {250, 4000}, {400, 2500}};

// A session is planned as WAVES waves up front (so progress is known), then
// played as a state machine driven by the clock: climb in steps to the peak,
// hold it (with a little wobble), drop to the floor and rest. Changing a
// setting re-plans the waves but keeps the position, so it blends in.
enum class Phase : uint8_t { Climb, Hold, Rest };
struct Wave { int peak; uint8_t steps; uint32_t climbMs, holdMs, restMs; };
static Wave waves[WAVES];
static uint32_t sessionMs;  // planned length of the session
static bool apPlaying, apPlanned;
static int apIntensity = 2, apEdge = 1, apVac = 0;
static int apWave;
static Phase apPhase;
static uint32_t apPhaseMs, apElapsed, apLastTick, apLastSuction;
static int apWobble, apWobbleSlot = -1;
static int apSpeed;

// ---------- local scripts ----------
static int sSlot = -1;
static bool sPlaying;

static uint32_t lostSeen;

// ---------- desired state ----------

static void push() {
  autoblow::Desired d;
  switch (act) {
    case Active::Remote:
      d.want = Want::Speed;
      d.speed = finishing ? 100 : rSpeed;
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
  d.valveMinus = minusHeld;
  d.pulseSeq = pulseSeq;
  d.pulseMs = pulseMs;

  autoblow::Desired cur = autoblow::desired();
  if (cur.want != d.want || cur.speed != d.speed || cur.script != d.script ||
      cur.scriptPlaying != d.scriptPlaying || cur.valvePlus != d.valvePlus ||
      cur.valveMinus != d.valveMinus || cur.pulseSeq != d.pulseSeq)
    autoblow::setDesired(d);
}

static void startPulse(uint32_t ms) {
  pulseSeq++;
  pulseMs = constrain(ms, 50, 2000);
}

// Leave whatever mode is running.
static void leave() {
  apPlaying = false;
  finishing = false;
  act = Active::None;
}

// ---------- autopilot engine ----------

static int rnd(int lo, int hi) { return lo + esp_random() % (hi - lo + 1); }

static void plan() {
  const Level &lv = LEVELS[apIntensity];
  const EdgeStyle &es = EDGE_STYLES[apEdge];
  sessionMs = 0;
  for (int w = 0; w < WAVES; w++) {
    Wave &wv = waves[w];
    float build = 0.6f + 0.4f * w / (WAVES - 1);  // first peak at 60 % of the range, last at 100 %
    wv.peak = lv.floor + lroundf((lv.ceiling - lv.floor) * build);
    wv.steps = rnd(4, 7);
    wv.climbMs = wv.steps * rnd(2000, 3500);
    wv.holdMs = rnd(es.peakMinMs, es.peakMaxMs);
    wv.restMs = rnd(es.restMinMs, es.restMaxMs);  // the edge: drop and rest
    sessionMs += wv.climbMs + wv.holdMs + wv.restMs;
  }
  apPlanned = true;
}

static void restartSession() {
  plan();
  apWave = 0;
  apPhase = Phase::Climb;
  apPhaseMs = apElapsed = 0;
  apWobbleSlot = -1;
}

static uint32_t phaseLength() {
  const Wave &w = waves[apWave];
  return apPhase == Phase::Climb ? w.climbMs : apPhase == Phase::Hold ? w.holdMs : w.restMs;
}

// Speed for the current position in the session.
static int apCurrentSpeed() {
  const Level &lv = LEVELS[apIntensity];
  const Wave &w = waves[apWave];
  switch (apPhase) {
    case Phase::Climb: {
      int step = min<int>(w.steps, 1 + apPhaseMs * w.steps / max<uint32_t>(w.climbMs, 1));
      return constrain(lv.floor + (w.peak - lv.floor) * step / w.steps, 5, 100);
    }
    case Phase::Hold: {
      int slot = apPhaseMs / 2500;  // a fresh random dip every 2.5 s
      if (slot != apWobbleSlot) {
        apWobbleSlot = slot;
        apWobble = rnd(0, EDGE_STYLES[apEdge].wobble);
      }
      return constrain(w.peak - apWobble, 5, 100);
    }
    default: return constrain(lv.floor, 5, 100);
  }
}

static void autopilotTick() {
  uint32_t now = millis();
  uint32_t dt = now - apLastTick;  // real time, so a stalled loop doesn't stretch the session
  apLastTick = now;
  apPhaseMs += dt;
  apElapsed += dt;
  while (apPhaseMs >= phaseLength()) {
    apPhaseMs -= phaseLength();
    apWobbleSlot = -1;
    if (apPhase == Phase::Climb) {
      apPhase = Phase::Hold;
    } else if (apPhase == Phase::Hold) {
      apPhase = Phase::Rest;
    } else if (++apWave >= WAVES) {
      restartSession();  // session over: start a fresh one
      break;
    } else {
      apPhase = Phase::Climb;
    }
  }
  apSpeed = apCurrentSpeed();

  const Vac &v = AP_VAC[apVac];
  if (v.pulseMs && apElapsed - apLastSuction >= v.intervalMs) {
    // A little shorter at high speed, where the stroke itself pulls more air.
    startPulse(lroundf(v.pulseMs * (1.2f - apSpeed / 250.0f)));
    apLastSuction = apElapsed;
  }
}

// ---------- 100 ms timer ----------

static void tick(lv_timer_t *) {
  uint32_t now = millis();

  if (act == Active::Autopilot && apPlaying) autopilotTick();

  if (act == Active::Remote && !finishing) {
    const Vac &v = REMOTE_VAC[rVac];
    if (v.pulseMs && !minusHeld && now - rLastPulse >= v.intervalMs) {
      startPulse(v.pulseMs);
      rLastPulse = now;
    }
  }

  // Lost the device (Wi-Fi or cloud, even briefly): stop everything rather than
  // resume later. The network task sends nothing but STOP until this ack.
  autoblow::Status st = autoblow::status();
  if (st.lost != lostSeen) {
    lostSeen = st.lost;
    if (act != Active::None) stopAll();
    autoblow::ackLost();
  }

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
  if (!held) rLastPulse = millis();
  push();
}

// ---------- autopilot ----------

void autopilotStart() {
  leave();
  if (!apPlanned) restartSession();
  act = Active::Autopilot;
  apPlaying = true;
  apLastTick = millis();  // resume where it paused, without the pause counting
  apSpeed = apCurrentSpeed();
  push();
}

void autopilotPause() {
  if (act == Active::Autopilot) leave();
  push();
}

// Changing a setting re-plans the waves but keeps the place in the session.
static void replan() {
  uint32_t done = apElapsed;
  plan();
  apElapsed = min(done, sessionMs);
  if (act == Active::Autopilot) {
    apSpeed = apCurrentSpeed();
    push();
  }
}
void autopilotSetIntensity(int l) { apIntensity = constrain(l, 0, 3); replan(); }
void autopilotSetEdge(int l) { apEdge = constrain(l, 0, 2); replan(); }
void autopilotSetVacuum(int l) {
  apVac = constrain(l, 0, 2);
  apLastSuction = 0;
  push();
}
int autopilotIntensity() { return apIntensity; }
int autopilotEdge() { return apEdge; }
int autopilotVacuum() { return apVac; }
int autopilotSpeed() { return act == Active::Autopilot ? apSpeed : 0; }
int autopilotProgress() {
  if (!apPlanned || !sessionMs) return 0;
  return min<uint32_t>(100, (uint64_t)apElapsed * 100 / sessionMs);
}

void finishMe() {
  leave();
  plusHeld = minusHeld = false;
  act = Active::Remote;
  finishing = true;
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

// ---------- restart ----------

static uint32_t restartStart;

static void restartPoll(lv_timer_t *) {
  autoblow::Status st = autoblow::status();
  bool moving = st.online && (!strcmp(st.mode, "TARGET_SPEED_PLAYING") ||
                              !strcmp(st.mode, "LOCAL_SCRIPT_PLAYING"));
  uint32_t ms = millis() - restartStart;
  // Give the STOP a moment to reach the device, but don't hang on a dead network.
  if ((ms > 800 && !moving) || ms > 5000) ESP.restart();
}

void stopAndRestart() {
  stopAll();
  restartStart = millis();
  lv_timer_create(restartPoll, 100, nullptr);
}

}  // namespace control
