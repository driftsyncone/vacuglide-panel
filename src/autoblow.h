// Autoblow cloud API (developers.autoblow.com/reference/http-api-v1-vacuglide/).
//
// The UI sets the *desired* device state; a network task on core 0 sends
// whatever differs from what it last sent, one request at a time, and retries
// until it lands. The UI never waits on the network.
#pragma once
#include <stdint.h>

namespace autoblow {

enum class Want : uint8_t { Idle, Speed, Script };

struct Desired {
  Want want = Want::Idle;
  int speed = 0;        // 1..100, used when want == Speed
  int script = 0;       // 0..29, used when want == Script
  bool scriptPlaying = true;
  bool valvePlus = false;
  bool valveMinus = false;
  // One-shot vacuum pulse: every new pulseSeq opens the Stroke- valve for
  // pulseMs, then closes it. Timed by the network task, right where the
  // requests go out, so the valve really is open for pulseMs. Only while
  // want == Speed and the valve isn't held; a pulse that fails is skipped.
  uint32_t pulseSeq = 0;
  uint16_t pulseMs = 0;
};

struct Status {
  bool wifi = false;
  bool online = false;  // device reachable through the cloud
  char mode[24] = "";   // operationalMode from the device
  int targetSpeed = 0;
  bool valvePlus = false, valveMinus = false;
  int localScript = 0;
  char error[64] = "";
  uint32_t lost = 0;    // how often the device dropped off (Wi-Fi down or 502)
};

void begin();

Desired desired();
void setDesired(const Desired &d);
// Forget what was sent, so the next pass resends everything (used by STOP).
void resync();
// After a loss the task only sends STOP until the UI has seen Status::lost
// change and acknowledged it (so a stale speed can't be resent first).
void ackLost();

Status status();

}  // namespace autoblow
