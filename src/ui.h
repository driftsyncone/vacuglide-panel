// 800x480 LVGL UI: status bar with STOP, and Remote / Autopilot / Scripts tabs.
#pragma once

void uiBegin();
// Switch to tab 0..3 (Remote, Autopilot, Scripts, Settings) and redraw now.
void uiShowTab(int tab);
