// Guition JC8048W550 hardware: RGB panel, GT911 touch, backlight, hooked into LVGL.
#pragma once

void boardBegin();
// Backlight. While it is off, the first touch only wakes the screen.
void boardScreenOff();
bool boardScreenIsOn();
