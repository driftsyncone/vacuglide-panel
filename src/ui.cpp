#include "ui.h"

#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>

#include "autoblow.h"
#include "battery.h"
#include "board.h"
#include "control.h"
#include "settings.h"
#include "wlan.h"

using control::Active;

static const lv_color_t RED = lv_color_hex(0xD32F2F);
static const lv_color_t ORANGE = lv_color_hex(0xEF6C00);
static const lv_color_t GREEN = lv_color_hex(0x2E7D32);
static const lv_color_t CYAN = lv_color_hex(0x00838F);
static const lv_color_t GREY = lv_color_hex(0x9E9E9E);

static lv_obj_t *statusDot, *statusLabel, *speedLabel, *wifiLabel, *lostBanner;
static lv_obj_t *battLabel, *battBanner;
static lv_obj_t *tabview;
static lv_obj_t *rSlider, *rSliderLabel, *rStartBtn;
static lv_obj_t *apStartBtn, *apStateLabel, *apBar;
static lv_obj_t *scButtons[SCRIPT_SLOTS], *scToggleBtn;
static lv_obj_t *stSsid, *stPass, *stToken, *stScanBtn, *stSaveBtn, *stInfo;

// ---------- helpers ----------

static bool deviceMoving(const autoblow::Status &st) {
  return st.online && (!strcmp(st.mode, "TARGET_SPEED_PLAYING") ||
                       !strcmp(st.mode, "LOCAL_SCRIPT_PLAYING"));
}

static lv_obj_t *button(lv_obj_t *parent, const char *text, int w, int h, lv_color_t color,
                        lv_event_cb_t cb, void *data = nullptr) {
  lv_obj_t *b = lv_button_create(parent);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_bg_color(b, color, 0);
  lv_obj_t *l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_28, 0);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, data);
  return b;
}

static void setButtonText(lv_obj_t *b, const char *text, lv_color_t color) {
  lv_label_set_text(lv_obj_get_child(b, 0), text);
  lv_obj_set_style_bg_color(b, color, 0);
}

// A button that is "on" only while a finger is on it.
static void holdEvent(lv_event_t *e) {
  auto fn = (void (*)(bool))lv_event_get_user_data(e);
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_PRESSED) fn(true);
  else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) fn(false);
}

static lv_obj_t *holdButton(lv_obj_t *parent, const char *text, int w, int h, void (*fn)(bool)) {
  lv_obj_t *b = button(parent, text, w, h, CYAN, nullptr);
  lv_obj_set_style_bg_color(b, RED, LV_STATE_PRESSED);
  lv_obj_add_event_cb(b, holdEvent, LV_EVENT_ALL, (void *)fn);
  return b;
}

// One-of-N selector. `fn` gets the chosen index.
static void segEvent(lv_event_t *e) {
  auto fn = (void (*)(int))lv_event_get_user_data(e);
  fn(lv_buttonmatrix_get_selected_button(lv_event_get_target_obj(e)));
}

static lv_obj_t *segmented(lv_obj_t *parent, const char **map, int selected, void (*fn)(int)) {
  lv_obj_t *m = lv_buttonmatrix_create(parent);
  lv_buttonmatrix_set_map(m, map);
  lv_buttonmatrix_set_button_ctrl_all(m, LV_BUTTONMATRIX_CTRL_CHECKABLE);
  lv_buttonmatrix_set_one_checked(m, true);
  lv_buttonmatrix_set_button_ctrl(m, selected, LV_BUTTONMATRIX_CTRL_CHECKED);
  lv_obj_add_event_cb(m, segEvent, LV_EVENT_VALUE_CHANGED, (void *)fn);
  return m;
}

static lv_obj_t *row(lv_obj_t *parent, int h) {
  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_set_size(r, lv_pct(100), h);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, 10, 0);
  return r;
}

static lv_obj_t *labeledSeg(lv_obj_t *parent, const char *title, const char **map, int sel,
                            void (*fn)(int)) {
  lv_obj_t *r = row(parent, 56);
  lv_obj_t *l = lv_label_create(r);
  lv_label_set_text(l, title);
  lv_obj_set_width(l, 130);
  lv_obj_t *m = segmented(r, map, sel, fn);
  lv_obj_set_height(m, 56);
  lv_obj_set_flex_grow(m, 1);
  lv_obj_set_style_pad_all(m, 4, 0);
  return m;
}

// Full-screen popup with a one-line text field and keyboard. `done` gets the
// text on OK; Cancel just closes.
using TextDone = void (*)(const char *text, intptr_t data);

struct EditCtx {
  TextDone done;
  intptr_t data;
  lv_obj_t *bg;
};

static void editEvent(lv_event_t *e) {
  auto *ctx = (EditCtx *)lv_event_get_user_data(e);
  lv_event_code_t c = lv_event_get_code(e);
  if ((c != LV_EVENT_READY && c != LV_EVENT_CANCEL) || !ctx->bg) return;
  // The keyboard's OK reaches the field twice; ctx->bg guards against that.
  if (c == LV_EVENT_READY) ctx->done(lv_textarea_get_text(lv_event_get_target_obj(e)), ctx->data);
  lv_obj_delete_async(ctx->bg);
  ctx->bg = nullptr;
}

static void editText(const char *title, const char *initial, bool password, TextDone done,
                     intptr_t data = 0) {
  lv_obj_t *bg = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(bg);
  lv_obj_set_size(bg, 800, 480);
  lv_obj_set_style_bg_color(bg, lv_color_hex(0x121212), 0);
  lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
  lv_obj_add_flag(bg, LV_OBJ_FLAG_CLICKABLE);  // nothing underneath gets touches

  lv_obj_t *t = lv_label_create(bg);
  lv_label_set_text(t, title);
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 20, 20);

  lv_obj_t *ta = lv_textarea_create(bg);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_password_mode(ta, password);
  lv_textarea_set_text(ta, initial);
  lv_obj_set_width(ta, password ? 620 : 760);
  lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 20, 60);
  lv_obj_add_state(ta, LV_STATE_FOCUSED);

  if (password) {
    lv_obj_t *show = button(bg, "Show", 130, 50, CYAN, [](lv_event_t *e) {
      auto *f = (lv_obj_t *)lv_event_get_user_data(e);
      bool hidden = lv_textarea_get_password_mode(f);
      lv_textarea_set_password_mode(f, !hidden);
      lv_label_set_text(lv_obj_get_child(lv_event_get_target_obj(e), 0), hidden ? "Hide" : "Show");
    }, ta);
    lv_obj_set_style_text_font(lv_obj_get_child(show, 0), &lv_font_montserrat_20, 0);
    lv_obj_align(show, LV_ALIGN_TOP_RIGHT, -20, 60);
  }

  lv_obj_t *kb = lv_keyboard_create(bg);
  lv_obj_set_size(kb, 800, 320);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(kb, ta);

  auto *ctx = new EditCtx{done, data, bg};
  lv_obj_add_event_cb(ta, editEvent, LV_EVENT_ALL, ctx);
  lv_obj_add_event_cb(bg, [](lv_event_t *e) { delete (EditCtx *)lv_event_get_user_data(e); },
                      LV_EVENT_DELETE, ctx);
}

// ---------- top bar ----------

static void buildTopBar(lv_obj_t *scr) {
  lv_obj_t *bar = lv_obj_create(scr);
  lv_obj_set_size(bar, 800, 64);
  lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_pad_hor(bar, 12, 0);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  statusDot = lv_obj_create(bar);
  lv_obj_set_size(statusDot, 16, 16);
  lv_obj_set_style_radius(statusDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(statusDot, 0, 0);
  lv_obj_align(statusDot, LV_ALIGN_LEFT_MID, 0, 0);

  statusLabel = lv_label_create(bar);
  lv_obj_set_width(statusLabel, 380);
  lv_label_set_long_mode(statusLabel, LV_LABEL_LONG_DOT);
  lv_obj_align(statusLabel, LV_ALIGN_LEFT_MID, 26, 0);

  wifiLabel = lv_label_create(bar);
  lv_obj_set_style_text_font(wifiLabel, &lv_font_montserrat_14, 0);
  lv_obj_align(wifiLabel, LV_ALIGN_LEFT_MID, 418, 0);

  // Under the Wi-Fi signal once a fuel gauge answers (see refresh()).
  battLabel = lv_label_create(bar);
  lv_obj_set_style_text_font(battLabel, &lv_font_montserrat_14, 0);
  lv_obj_align(battLabel, LV_ALIGN_LEFT_MID, 418, 11);
  lv_obj_add_flag(battLabel, LV_OBJ_FLAG_HIDDEN);

  battBanner = lv_label_create(lv_layer_top());
  lv_obj_set_size(battBanner, 800, LV_SIZE_CONTENT);
  lv_obj_align(battBanner, LV_ALIGN_BOTTOM_MID, 0, 0);  // bottom: keeps the tabs usable
  lv_obj_set_style_bg_color(battBanner, RED, 0);
  lv_obj_set_style_bg_opa(battBanner, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(battBanner, 10, 0);
  lv_label_set_long_mode(battBanner, LV_LABEL_LONG_WRAP);
  lv_obj_add_flag(battBanner, LV_OBJ_FLAG_HIDDEN);

  // Shown when the link drops while the device was moving: STOP can't reach it.
  lostBanner = lv_label_create(lv_layer_top());
  lv_obj_set_size(lostBanner, 800, LV_SIZE_CONTENT);
  lv_obj_align(lostBanner, LV_ALIGN_TOP_MID, 0, 64);
  lv_obj_set_style_bg_color(lostBanner, RED, 0);
  lv_obj_set_style_bg_opa(lostBanner, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(lostBanner, 10, 0);
  lv_label_set_long_mode(lostBanner, LV_LABEL_LONG_WRAP);
  lv_label_set_text(lostBanner,
                    LV_SYMBOL_WARNING " Connection lost while the VacuGlide was running. It may "
                    "still be moving: switch it off on the device. The panel sends STOP as soon "
                    "as it reconnects.");
  lv_obj_add_flag(lostBanner, LV_OBJ_FLAG_HIDDEN);

  speedLabel = lv_label_create(bar);
  lv_obj_set_style_text_font(speedLabel, &lv_font_montserrat_28, 0);
  lv_obj_align(speedLabel, LV_ALIGN_RIGHT_MID, -200, 0);

  lv_obj_t *stop = button(bar, LV_SYMBOL_STOP " STOP", 180, 52, RED,
                          [](lv_event_t *) { control::stopAll(); });
  lv_obj_align(stop, LV_ALIGN_RIGHT_MID, 0, 0);
}

// ---------- remote tab ----------

static void buildRemote(lv_obj_t *tab) {
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(tab, 20, 0);

  lv_obj_t *left = lv_obj_create(tab);
  lv_obj_remove_style_all(left);
  lv_obj_set_size(left, 150, lv_pct(100));
  lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(left, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(left, 14, 0);

  rSlider = lv_slider_create(left);
  lv_obj_set_size(rSlider, 60, 260);
  lv_slider_set_range(rSlider, 5, 100);
  lv_slider_set_value(rSlider, control::remoteSpeed(), LV_ANIM_OFF);
  lv_obj_add_event_cb(rSlider, [](lv_event_t *e) {
    int v = lv_slider_get_value(rSlider);
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED)
      lv_label_set_text_fmt(rSliderLabel, "%d%%", v);
    else if (lv_event_get_code(e) == LV_EVENT_RELEASED)
      control::remoteSetSpeed(v);
  }, LV_EVENT_ALL, nullptr);

  rSliderLabel = lv_label_create(left);
  lv_obj_set_style_text_font(rSliderLabel, &lv_font_montserrat_28, 0);
  lv_label_set_text_fmt(rSliderLabel, "%d%%", control::remoteSpeed());

  lv_obj_t *right = lv_obj_create(tab);
  lv_obj_remove_style_all(right);
  lv_obj_set_height(right, lv_pct(100));
  lv_obj_set_flex_grow(right, 1);
  lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(right, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(right, 16, 0);

  rStartBtn = button(right, "", lv_pct(100), 80, GREEN, [](lv_event_t *) {
    if (control::active() == Active::Remote) control::remotePause();
    else control::remoteStart();
  });

  lv_obj_t *r = row(right, 80);
  lv_obj_set_style_pad_column(r, 16, 0);
  lv_obj_set_flex_grow(holdButton(r, "Stroke -", 0, 80, control::strokeMinus), 1);
  lv_obj_set_flex_grow(holdButton(r, "Stroke +", 0, 80, control::strokePlus), 1);

  static const char *vac[] = {"Off", "Low", "Medium", "High", ""};
  labeledSeg(right, "Vacuum", vac, control::remoteVacuum(), control::remoteSetVacuum);
}

// ---------- autopilot tab ----------

static void buildAutopilot(lv_obj_t *tab) {
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(tab, 10, 0);

  lv_obj_t *top = row(tab, 64);
  apStartBtn = button(top, "", 260, 64, GREEN, [](lv_event_t *) {
    if (control::active() == Active::Autopilot) control::autopilotPause();
    else control::autopilotStart();
  });
  lv_obj_t *info = lv_obj_create(top);
  lv_obj_remove_style_all(info);
  lv_obj_set_height(info, 64);
  lv_obj_set_flex_grow(info, 1);
  lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(info, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(info, 8, 0);
  apStateLabel = lv_label_create(info);
  apBar = lv_bar_create(info);
  lv_obj_set_size(apBar, lv_pct(100), 12);
  lv_bar_set_range(apBar, 0, 100);

  static const char *intensity[] = {"Warmup", "Low", "Medium", "High", ""};
  static const char *edge[] = {"Gentle", "Moderate", "Intense", ""};
  static const char *vac[] = {"Off", "Low", "High", ""};
  labeledSeg(tab, "Intensity", intensity, control::autopilotIntensity(),
             control::autopilotSetIntensity);
  labeledSeg(tab, "Edge", edge, control::autopilotEdge(), control::autopilotSetEdge);
  labeledSeg(tab, "Vacuum", vac, control::autopilotVacuum(), control::autopilotSetVacuum);

  lv_obj_t *r = row(tab, 64);
  lv_obj_set_style_pad_column(r, 12, 0);
  lv_obj_set_flex_grow(holdButton(r, "Stroke -", 0, 64, control::strokeMinus), 1);
  lv_obj_set_flex_grow(holdButton(r, "Stroke +", 0, 64, control::strokePlus), 1);
  lv_obj_t *fin = button(r, "Finish Me", 0, 64, lv_color_hex(0xC2185B),
                         [](lv_event_t *) { control::finishMe(); });
  lv_obj_set_flex_grow(fin, 2);
}

// ---------- scripts tab ----------

static void scriptLabel(int slot) {
  lv_obj_t *l = lv_obj_get_child(scButtons[slot], 0);
  const String &n = settings.scriptNames[slot];
  if (n.length()) lv_label_set_text_fmt(l, "%d  %s", slot + 1, n.c_str());
  else lv_label_set_text_fmt(l, "%d", slot + 1);
}

static void scriptRenamed(const char *text, intptr_t slot) {
  String n = text;
  n.trim();
  settings.scriptNames[slot] = n;
  settingsSaveScriptName(slot);
  scriptLabel(slot);
}

// Tap plays the slot, long press names it.
static void scriptEvent(lv_event_t *e) {
  int slot = (intptr_t)lv_event_get_user_data(e);
  if (lv_event_get_code(e) == LV_EVENT_SHORT_CLICKED) {
    control::scriptPlay(slot);
  } else if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) {
    char title[40];
    snprintf(title, sizeof title, "Name for script %d (empty = none)", slot + 1);
    editText(title, settings.scriptNames[slot].c_str(), false, scriptRenamed, slot);
  }
}

static void buildScripts(lv_obj_t *tab) {
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(tab, 12, 0);
  lv_obj_remove_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *grid = lv_obj_create(tab);
  lv_obj_remove_style_all(grid);
  lv_obj_set_width(grid, lv_pct(100));
  lv_obj_set_flex_grow(grid, 1);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_gap(grid, 8, 0);
  lv_obj_set_scroll_dir(grid, LV_DIR_VER);

  for (int i = 0; i < SCRIPT_SLOTS; i++) {
    lv_obj_t *b = button(grid, "", lv_pct(19), 64, lv_color_hex(0x37474F), nullptr);
    lv_obj_set_style_bg_color(b, CYAN, LV_STATE_CHECKED);
    lv_obj_t *l = lv_obj_get_child(b, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
    lv_obj_set_width(l, lv_pct(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_event_cb(b, scriptEvent, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)i);
    lv_obj_add_event_cb(b, scriptEvent, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
    scButtons[i] = b;
    scriptLabel(i);
  }

  scToggleBtn = button(tab, "", lv_pct(100), 72, ORANGE,
                       [](lv_event_t *) { control::scriptToggle(); });
}

// ---------- settings tab ----------
// Wi-Fi and token are edited here and only take effect after Save & restart.
// The screen timeout applies at once.

static String pSsid, pPass, pToken;  // pending, not saved yet

static void settingsShow() {
  lv_label_set_text(lv_obj_get_child(stSsid, 0), pSsid.length() ? pSsid.c_str() : "(none)");
  lv_label_set_text(lv_obj_get_child(stPass, 0),
                    pPass.length() ? LV_SYMBOL_BULLET LV_SYMBOL_BULLET LV_SYMBOL_BULLET
                                     LV_SYMBOL_BULLET LV_SYMBOL_BULLET LV_SYMBOL_BULLET
                                   : "(none)");
  lv_label_set_text(lv_obj_get_child(stToken, 0), pToken.length() ? pToken.c_str() : "(none)");
  bool dirty = pSsid != settings.wifiSsid || pPass != settings.wifiPass || pToken != settings.token;
  lv_obj_set_state(stSaveBtn, LV_STATE_DISABLED, !dirty);
}

static lv_obj_t *valueField(lv_obj_t *parent, const char *title, lv_event_cb_t cb) {
  lv_obj_t *r = row(parent, 56);
  lv_obj_t *l = lv_label_create(r);
  lv_label_set_text(l, title);
  lv_obj_set_width(l, 150);
  lv_obj_t *b = button(r, "", 0, 56, lv_color_hex(0x37474F), cb);
  lv_obj_set_flex_grow(b, 1);
  lv_obj_t *v = lv_obj_get_child(b, 0);
  lv_obj_set_style_text_font(v, &lv_font_montserrat_20, 0);
  lv_obj_set_width(v, lv_pct(100));
  lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
  lv_obj_align(v, LV_ALIGN_LEFT_MID, 0, 0);
  return b;
}

static void editPass() {
  editText("Wi-Fi password", pPass.c_str(), true, [](const char *t, intptr_t) {
    pPass = t;
    settingsShow();
  });
}

static void networkChosen(lv_event_t *e) {
  pSsid = lv_list_get_button_text(lv_obj_get_parent(lv_event_get_target_obj(e)),
                                  lv_event_get_target_obj(e));
  lv_obj_delete_async((lv_obj_t *)lv_event_get_user_data(e));
  settingsShow();
  editPass();
}

static void showNetworks(int n) {
  lv_obj_t *bg = lv_obj_create(lv_layer_top());
  lv_obj_set_size(bg, 800, 480);
  lv_obj_set_style_radius(bg, 0, 0);
  lv_obj_set_style_border_width(bg, 0, 0);
  lv_obj_set_flex_flow(bg, LV_FLEX_FLOW_COLUMN);
  lv_obj_remove_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *top = row(bg, 56);
  lv_obj_t *t = lv_label_create(top);
  lv_label_set_text(t, n > 0 ? "Pick a network" : "No networks found");
  lv_obj_set_flex_grow(t, 1);
  button(top, "Cancel", 160, 52, GREY, [](lv_event_t *e) {
    lv_obj_delete_async((lv_obj_t *)lv_event_get_user_data(e));
  }, bg);

  lv_obj_t *list = lv_list_create(bg);
  lv_obj_set_width(list, lv_pct(100));
  lv_obj_set_flex_grow(list, 1);
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;  // hidden network
    bool dup = false;
    for (int j = 0; j < i; j++) dup |= WiFi.SSID(j) == ssid;
    if (dup) continue;  // several APs, same network
    lv_obj_t *b = lv_list_add_button(list, LV_SYMBOL_WIFI, ssid.c_str());
    lv_obj_set_height(b, 56);
    lv_obj_add_event_cb(b, networkChosen, LV_EVENT_CLICKED, bg);
  }
  WiFi.scanDelete();
}

static void scanPoll(lv_timer_t *t) {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  lv_timer_delete(t);
  setButtonText(stScanBtn, "Scan", CYAN);
  showNetworks(n);  // n < 0: scan failed, shows "No networks found"
}

static void restartPoll(lv_timer_t *t) {
  // Give the STOP a moment to reach the device, but don't hang on a dead network.
  static uint32_t start = millis();
  uint32_t ms = millis() - start;
  if ((ms > 800 && !deviceMoving(autoblow::status())) || ms > 5000) ESP.restart();
}

static void buildSettings(lv_obj_t *tab) {
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(tab, 10, 0);

  pSsid = settings.wifiSsid;
  pPass = settings.wifiPass;
  pToken = settings.token;

  stSsid = valueField(tab, "Wi-Fi", [](lv_event_t *) {
    editText("Wi-Fi network name", pSsid.c_str(), false, [](const char *t, intptr_t) {
      pSsid = t;
      settingsShow();
    });
  });
  stScanBtn = button(lv_obj_get_parent(stSsid), "Scan", 140, 56, CYAN, [](lv_event_t *) {
    if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) return;
    WiFi.scanNetworks(true);
    setButtonText(stScanBtn, "...", GREY);
    lv_timer_create(scanPoll, 300, nullptr);
  });
  lv_obj_set_style_text_font(lv_obj_get_child(stScanBtn, 0), &lv_font_montserrat_20, 0);
  stPass = valueField(tab, "Password", [](lv_event_t *) { editPass(); });
  stToken = valueField(tab, "Token", [](lv_event_t *) {
    editText("Autoblow device token", pToken.c_str(), false, [](const char *t, intptr_t) {
      pToken = t;
      pToken.trim();
      settingsShow();
    });
  });

  static const char *sleep[] = {"Never", "1 min", "5 min", "15 min", ""};
  static const int SLEEP_MIN[] = {0, 1, 5, 15};
  int sel = 2;
  for (int i = 0; i < 4; i++)
    if (SLEEP_MIN[i] == settings.sleepMin) sel = i;
  labeledSeg(tab, "Screen off", sleep, sel, [](int i) {
    settings.sleepMin = SLEEP_MIN[i];
    settingsSaveSleep();
  });

  lv_obj_t *r = row(tab, 64);
  stInfo = lv_label_create(r);
  lv_obj_set_flex_grow(stInfo, 1);
  lv_obj_set_style_text_font(stInfo, &lv_font_montserrat_14, 0);
  stSaveBtn = button(r, "Save & restart", 300, 64, GREEN, [](lv_event_t *) {
    settings.wifiSsid = pSsid;
    settings.wifiPass = pPass;
    settings.token = pToken;
    settingsSave();
    control::stopAll();
    setButtonText(stSaveBtn, "Restarting...", GREY);
    lv_timer_create(restartPoll, 100, nullptr);
  });
  lv_obj_set_style_bg_color(stSaveBtn, lv_color_hex(0x424242), LV_STATE_DISABLED);

  settingsShow();
}

// ---------- refresh ----------

static const char *modeText(const char *m) {
  if (!strcmp(m, "TARGET_SPEED_PLAYING")) return "Running";
  if (!strcmp(m, "TARGET_SPEED_PAUSED")) return "Paused";
  if (!strcmp(m, "LOCAL_SCRIPT_PLAYING")) return "Script playing";
  if (!strcmp(m, "LOCAL_SCRIPT_PAUSED")) return "Script paused";
  if (!strcmp(m, "ONLINE_CONNECTED")) return "Ready";
  if (!strcmp(m, "FIRMWARE_UPDATING")) return "Updating firmware";
  return m;
}

static void refresh(lv_timer_t *) {
  autoblow::Status st = autoblow::status();
  Active a = control::active();

  wlan::Info w = wlan::info();
  int bars = wlan::bars(w.rssi);

  lv_color_t dot = !st.wifi ? RED : st.online ? GREEN : ORANGE;
  lv_obj_set_style_bg_color(statusDot, dot, 0);
  if (!st.wifi) {
    // Problems only the Settings tab can fix get a pointer there.
    bool fixable = !strcmp(w.problem, "wrong password?") ||
                   !strcmp(w.problem, "network not found") || !strcmp(w.problem, "not set up");
    lv_label_set_text_fmt(statusLabel, "No Wi-Fi: %s%s", w.problem[0] ? w.problem : "connecting...",
                          fixable ? " Check Settings" : "");
  } else if (!st.online) {
    lv_label_set_text(statusLabel, st.error[0] ? st.error : "Connecting...");
  } else if (st.error[0]) {
    lv_label_set_text_fmt(statusLabel, "%s - %s", modeText(st.mode), st.error);
  } else if (bars == 1) {
    lv_label_set_text_fmt(statusLabel, "%s - weak Wi-Fi", modeText(st.mode));
  } else {
    lv_label_set_text(statusLabel, modeText(st.mode));
  }

  static const lv_color_t BAR_COLOR[] = {RED, RED, ORANGE, GREEN, GREEN};
  lv_obj_set_style_text_color(wifiLabel, BAR_COLOR[bars], 0);
  if (w.up) lv_label_set_text_fmt(wifiLabel, LV_SYMBOL_WIFI " %d", w.rssi);
  else lv_label_set_text(wifiLabel, LV_SYMBOL_WIFI " --");

  // Battery (only with a fuel gauge): stack it under the Wi-Fi signal.
  battery::Info b = battery::info();
  static bool hadGauge;
  if (b.present != hadGauge) {
    hadGauge = b.present;
    lv_obj_align(wifiLabel, LV_ALIGN_LEFT_MID, 418, b.present ? -11 : 0);
    lv_obj_set_flag(battLabel, LV_OBJ_FLAG_HIDDEN, !b.present);
  }
  if (b.present) {
    int pct = lroundf(b.percent);
    const char *icon = pct > 80 ? LV_SYMBOL_BATTERY_FULL : pct > 55 ? LV_SYMBOL_BATTERY_3
                     : pct > 30 ? LV_SYMBOL_BATTERY_2 : pct > 10 ? LV_SYMBOL_BATTERY_1
                     : LV_SYMBOL_BATTERY_EMPTY;
    lv_label_set_text_fmt(battLabel, "%s%s %d%%", b.charging ? LV_SYMBOL_CHARGE " " : "", icon, pct);
    lv_obj_set_style_text_color(battLabel, b.charging || pct > 20 ? GREEN : pct > 10 ? ORANGE : RED, 0);
  }

  bool moving = deviceMoving(st);

  // Nearly empty on battery: keep the device stopped, so the panel can't die mid-run
  // and leave it running with nothing to stop it. Plugging in USB lifts this
  // (MAX17048 sees the charge; with a MAX17043 it lifts once the level passes 5 %).
  bool empty = b.present && !b.charging && b.percent <= 5;
  if (empty && a != Active::None) control::stopAll();
  if (empty)
    lv_label_set_text_fmt(battBanner, LV_SYMBOL_BATTERY_EMPTY " Battery at %d%%: VacuGlide control is "
                          "paused until you plug in USB.", (int)lroundf(b.percent));
  lv_obj_set_flag(battBanner, LV_OBJ_FLAG_HIDDEN, !empty);
  // Remember the last thing the device told us while it was reachable.
  static bool lastMoving;
  if (st.online) lastMoving = moving;
  bool lost = !st.online && lastMoving;
  lv_obj_set_flag(lostBanner, LV_OBJ_FLAG_HIDDEN, !lost);

  if (moving) lv_label_set_text_fmt(speedLabel, "%d%%", st.targetSpeed);
  else lv_label_set_text(speedLabel, "");

  // Remote
  if (a == Active::Remote) setButtonText(rStartBtn, LV_SYMBOL_PAUSE " Pause", ORANGE);
  else setButtonText(rStartBtn, LV_SYMBOL_PLAY " Start", GREEN);
  if (!lv_obj_has_state(rSlider, LV_STATE_PRESSED) &&
      lv_slider_get_value(rSlider) != control::remoteSpeed()) {
    lv_slider_set_value(rSlider, control::remoteSpeed(), LV_ANIM_OFF);
    lv_label_set_text_fmt(rSliderLabel, "%d%%", control::remoteSpeed());
  }

  // Autopilot
  if (a == Active::Autopilot) {
    setButtonText(apStartBtn, LV_SYMBOL_PAUSE " Pause", ORANGE);
    lv_label_set_text_fmt(apStateLabel, "Autopilot running at %d%%", control::autopilotSpeed());
  } else {
    setButtonText(apStartBtn, LV_SYMBOL_PLAY " Start", GREEN);
    lv_label_set_text(apStateLabel, "Autopilot standby");
  }
  lv_bar_set_value(apBar, control::autopilotProgress(), LV_ANIM_OFF);

  // Scripts
  static int lastSlot = -2;
  int slot = control::scriptSlot();
  if (slot != lastSlot) {
    for (int i = 0; i < SCRIPT_SLOTS; i++) lv_obj_set_state(scButtons[i], LV_STATE_CHECKED, i == slot);
    lastSlot = slot;
  }
  if (slot < 0) setButtonText(scToggleBtn, "Tap a slot to play", GREY);
  else if (control::scriptPlaying()) setButtonText(scToggleBtn, LV_SYMBOL_PAUSE " Pause", ORANGE);
  else setButtonText(scToggleBtn, LV_SYMBOL_PLAY " Resume", GREEN);

  // Settings
  char battLine[64];
  if (b.present)
    snprintf(battLine, sizeof battLine, "Battery %d%%, %.2f V%s (%s)", (int)lroundf(b.percent),
             b.volts, b.charging ? ", charging" : "", b.chip);
  else
    snprintf(battLine, sizeof battLine, "Battery level: no fuel gauge connected");
  if (w.up)
    lv_label_set_text_fmt(stInfo, "Panel %s, signal %d dBm\n%s\nBuilt " __DATE__,
                          WiFi.localIP().toString().c_str(), w.rssi, battLine);
  else lv_label_set_text_fmt(stInfo, "Wi-Fi: %s\n%s\nBuilt " __DATE__, w.problem, battLine);

  // Screen sleep, only while nothing runs, so STOP is always one tap away.
  if (boardScreenIsOn() && settings.sleepMin > 0 && a == Active::None && !lost &&
      lv_display_get_inactive_time(nullptr) > settings.sleepMin * 60000u)
    boardScreenOff();
}

void uiBegin() {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x121212), 0);

  buildTopBar(scr);

  lv_obj_t *tv = tabview = lv_tabview_create(scr);
  lv_obj_set_size(tv, 800, 480 - 64);
  lv_obj_align(tv, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_tabview_set_tab_bar_size(tv, 52);

  buildRemote(lv_tabview_add_tab(tv, "Remote"));
  buildAutopilot(lv_tabview_add_tab(tv, "Autopilot"));
  buildScripts(lv_tabview_add_tab(tv, "Scripts"));
  buildSettings(lv_tabview_add_tab(tv, LV_SYMBOL_SETTINGS " Settings"));

  lv_timer_create(refresh, 200, nullptr);
  refresh(nullptr);
}

void uiShowTab(int tab) {
  lv_tabview_set_active(tabview, constrain(tab, 0, 3), LV_ANIM_OFF);
  refresh(nullptr);
  lv_refr_now(nullptr);
}
