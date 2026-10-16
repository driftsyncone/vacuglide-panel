// Pins and timings from the community ESPHome config for the JC8048W550.
#include "board.h"

#include <Arduino.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <lvgl.h>

static const int W = 800, H = 480;
static const int PIN_BACKLIGHT = 2;
static const int PIN_SDA = 19, PIN_SCL = 20;

static esp_lcd_panel_handle_t panel;
static uint8_t gtAddr = 0x5D;
static bool screenOn = true;

void boardScreenOff() {
  screenOn = false;
  digitalWrite(PIN_BACKLIGHT, LOW);
}

bool boardScreenIsOn() { return screenOn; }

// ---------- display ----------

static void flushCb(lv_display_t *disp, const lv_area_t *a, uint8_t *px) {
  esp_lcd_panel_draw_bitmap(panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, px);
  lv_display_flush_ready(disp);
}

static void panelBegin() {
  esp_lcd_rgb_panel_config_t cfg = {};
  cfg.clk_src = LCD_CLK_SRC_DEFAULT;
  cfg.timings.pclk_hz = 14 * 1000 * 1000;  // 16 MHz garbles under PSRAM load
  cfg.timings.h_res = W;
  cfg.timings.v_res = H;
  cfg.timings.hsync_pulse_width = 4;
  cfg.timings.hsync_back_porch = 8;
  cfg.timings.hsync_front_porch = 8;
  cfg.timings.vsync_pulse_width = 4;
  cfg.timings.vsync_back_porch = 8;
  cfg.timings.vsync_front_porch = 8;
  cfg.timings.flags.pclk_active_neg = 1;
  cfg.data_width = 16;
  cfg.bits_per_pixel = 16;
  cfg.num_fbs = 1;
  // Bounce buffer keeps the picture steady while Wi-Fi competes for PSRAM bandwidth.
  cfg.bounce_buffer_size_px = W * 16;
  cfg.hsync_gpio_num = 39;
  cfg.vsync_gpio_num = 41;
  cfg.de_gpio_num = 40;
  cfg.pclk_gpio_num = 42;
  cfg.disp_gpio_num = -1;
  // RGB565 bus order: B0..B4, G0..G5, R0..R4.
  const int pins[16] = {8, 3, 46, 9, 1, 5, 6, 7, 15, 16, 4, 45, 48, 47, 21, 14};
  for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = pins[i];
  cfg.flags.fb_in_psram = 1;

  ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&cfg, &panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
}

// ---------- touch (GT911) ----------

static bool gtRead(uint16_t reg, uint8_t *buf, size_t len) {
  Wire.beginTransmission(gtAddr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(gtAddr, (uint8_t)len) != len) return false;
  for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

static void gtWrite(uint16_t reg, uint8_t val) {
  Wire.beginTransmission(gtAddr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  Wire.write(val);
  Wire.endTransmission();
}

static void touchBegin() {
  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  for (uint8_t addr : {0x5D, 0x14}) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      gtAddr = addr;
      Serial.printf("GT911 at 0x%02X\n", addr);
      return;
    }
  }
  Serial.println("GT911 not found");
}

static void touchCb(lv_indev_t *, lv_indev_data_t *data) {
  // GT911 sets bit 7 of the status register when a new sample is ready.
  // Between samples, keep reporting the last state.
  static int16_t lastX, lastY;
  static bool pressed;
  static bool swallow;  // the touch that woke the screen, until the finger lifts
  uint8_t st;
  if (gtRead(0x814E, &st, 1) && (st & 0x80)) {
    pressed = false;
    uint8_t p[8];
    if ((st & 0x0F) > 0 && gtRead(0x814F, p, 8)) {
      lastX = p[1] | (p[2] << 8);
      lastY = p[3] | (p[4] << 8);
      pressed = true;
    }
    gtWrite(0x814E, 0);
  }
  if (pressed && !screenOn) {
    screenOn = swallow = true;
    digitalWrite(PIN_BACKLIGHT, HIGH);
    lv_display_trigger_activity(nullptr);
  }
  if (!pressed) swallow = false;
  data->state = pressed && !swallow ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  data->point.x = lastX;
  data->point.y = lastY;
}

// ---------- LVGL ----------

void boardBegin() {
  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, LOW);

  panelBegin();
  touchBegin();

  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  lv_display_t *disp = lv_display_create(W, H);
  // Draw buffers in internal RAM are fast, but TLS needs ~40 KB of it too.
  const size_t bufBytes = W * 20 * 2;
  void *b1 = heap_caps_malloc(bufBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  void *b2 = heap_caps_malloc(bufBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  lv_display_set_buffers(disp, b1, b2, bufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flushCb);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchCb);

  digitalWrite(PIN_BACKLIGHT, HIGH);
}
