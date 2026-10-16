// Guition JC8048W550 hardware: RGB panel, GT911 touch, backlight, hooked into LVGL.
#pragma once
#include <stdint.h>

void boardBegin();
// Backlight. While it is off, the first touch only wakes the screen.
void boardScreenOff();
bool boardScreenIsOn();
// The 800x480 RGB565 frame buffer as shown on screen (for screenshots).
const uint16_t *boardFrameBuffer();
