// What the panel is doing: manual remote, Autopilot (our own edging generator),
// or a local script. Runs on the LVGL thread from a 100 ms timer and only ever
// sets autoblow::Desired.
#pragma once

namespace control {

enum class Active { None, Remote, Autopilot, Script };

void begin();
Active active();

// Remote control
void remoteStart();
void remotePause();
void remoteSetSpeed(int speed);  // also starts, like moving a speed slider would
int remoteSpeed();
void remoteSetVacuum(int level);  // 0 off, 1 low, 2 medium, 3 high
int remoteVacuum();

// Hold-to-open valves (both pages)
void strokePlus(bool held);
void strokeMinus(bool held);

// Autopilot
void autopilotStart();
void autopilotPause();
void autopilotSetIntensity(int level);  // 0 warmup, 1 low, 2 medium, 3 high
void autopilotSetEdge(int level);       // 0 gentle, 1 moderate, 2 intense
void autopilotSetVacuum(int level);     // 0 off, 1 low, 2 high
int autopilotIntensity();
int autopilotEdge();
int autopilotVacuum();
int autopilotSpeed();  // last speed sent, 0 when not running
int autopilotProgress();  // 0..100 through the generated script
void finishMe();

// Local scripts
void scriptPlay(int slot);  // 0..29
void scriptToggle();        // pause / resume
int scriptSlot();           // -1 if none
bool scriptPlaying();

// Everything off, valves closed, and resend it all.
void stopAll();
// stopAll(), then restart the panel once the device has stopped (5 s at most).
void stopAndRestart();

}  // namespace control
