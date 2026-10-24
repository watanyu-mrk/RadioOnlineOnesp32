#pragma once

// Retro "hi-fi receiver" faceplate, rebuilt on LVGL 8.3.
//
// Same design as before: burnt-orange bezel, cream silk-screen lettering, amber
// seven-segment Bangkok wall clock (with dim ghost segments), segmented buffer meter,
// spinning station disc, volume rocker and the five-button control strip. The port keeps
// every coordinate, colour and glyph from the Arduino_GFX version: text is drawn with the
// very same 5x7 bitmap font Arduino_GFX uses (6x8 cell), and the seven-segment clock,
// pixel icons and meter are custom draw callbacks. LVGL owns the object tree, invalidation,
// screens and the input device; the UI still never touches audio or network.
//
// Screens: main faceplate (with live L/R VU meters), station list, equalizer, AP setup,
// touch calibration. Actions are queued and collected by the sketch with takeAction().

#include <Arduino.h>
#include <lvgl.h>

#include "storage/settings_store.h"

struct UiInfo {
  bool wifiConnected;
  const char *ip;
  int rssi;
  bool streamRunning;
  const char *station;
  const char *title;
  uint32_t bitrate;
  const char *codec;
  uint32_t sampleRate;
  uint8_t channels;
  uint32_t playedSec;
  uint32_t bufferBytes;
  uint32_t heapFree;
  uint32_t uptimeSec;
  // Wall clock for the big display; false until NTP has set the system time.
  bool timeValid;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  // Amplifier level 0..21 and mute flag, so the rocker renders real state.
  uint8_t volume;
  bool muted;
};

class LvUi {
 public:
  enum Action : int8_t {
    ACT_NONE = 0,
    ACT_VOL_DOWN = -1,
    ACT_VOL_UP = 1,
    ACT_STOP = -2,
    ACT_PLAY = 2,
    ACT_PREV = 3,        // previous station
    ACT_NEXT = 4,        // next station
    ACT_LIST = 5,        // open the station list
    ACT_LIST_ITEM = 6,   // a row was tapped, see listIndex()
    ACT_LIST_CLOSE = 7,  // list dismissed
    ACT_SETUP = 8,       // enter the Wi-Fi / station setup portal
    ACT_SETUP_DONE = 9,  // leave setup (reboot)
    ACT_EQ = 10,         // open the equalizer screen
    ACT_EQ_CLOSE = 11,   // back to the faceplate
    ACT_EQ_PRESET = 12,  // a preset was tapped, see eqPresetIndex()
    ACT_EQ_BAND_DOWN = 13,  // a band -/+ was tapped, see eqBand()
    ACT_EQ_BAND_UP = 14,
  };

  bool begin();
  void update(const UiInfo &i);

  // One queued action per call; ACT_NONE when nothing is pending.
  int8_t takeAction();

  uint8_t listIndex() const { return listSel_; }
  uint8_t eqPresetIndex() const { return eqPresetSel_; }
  uint8_t eqBand() const { return eqBandSel_; }

  // Live VU levels (0..255 per channel). Invalidates only the small meter object, so it
  // is cheap enough to call at 20 Hz from the main loop.
  void setVu(uint8_t left, uint8_t right);

  // Renders the tone state the sketch got from the Equalizer module.
  void setEq(uint8_t preset, int8_t low, int8_t mid, int8_t high);

  // ---- screens ------------------------------------------------------------------------
  void showMain();
  void openList(const char *const *names, uint8_t count);
  void closeList();
  void openEq(const char *const *names, uint8_t count);
  void setupScreen(const char *ssid, const char *pass, const char *ip);
  void calibrationTarget(uint8_t step, uint8_t total, int16_t tx, int16_t ty);

  // How many LVGL objects the UI needed (boot diagnostics).
  uint16_t objectCount() const { return itemCount_; }

 private:
  enum Kind : uint8_t {
    K_TEXT = 0,
    K_ICON,
    K_METER,
    K_CLOCK,
    K_DISC,
    K_PLAYICON,
    K_TARGET,
    K_ARROW,
    K_MARK,
    K_VU,
    K_EQBAR,
  };

  struct Item {
    lv_obj_t *obj = nullptr;
    uint8_t kind = K_TEXT;
    uint8_t scale = 1;
    lv_color_t color = {};
    const uint32_t *icon = nullptr;
    char text[32] = {0};
    int16_t anchorX = 0;  // right-aligned items keep this as their right edge
    bool rightAlign = false;
    int16_t p1 = 0, p2 = 0;  // pixel coords for the touch marker
    uint8_t a = 0, b = 0, c = 0;
    bool flag = false;
  };

  struct BtnData {
    LvUi *ui = nullptr;
    lv_obj_t *obj = nullptr;
    Item *icon = nullptr;
    Item *label = nullptr;
    int8_t action = 0;
    int8_t page = 0;   // -1 page up, +1 page down (station list)
    int8_t row = -1;   // station-list row index
    uint8_t sign = 0;  // 0 none, 1 minus, 2 plus (volume rocker)
    bool highlight = true;
    bool pressed = false;
  };

  static constexpr uint8_t MAX_ITEMS = 160;  // five screens + the equalizer's 8 presets
  static constexpr uint8_t MAX_BTNS = 40;
  static constexpr uint8_t LIST_ROWS = 7;
  static constexpr uint8_t LIST_MAX = RADIO_MAX_STATIONS;
  static constexpr uint8_t EQ_PRESETS = 8;

  // ---- construction -------------------------------------------------------------------
  void buildMain(lv_obj_t *scr);
  void buildList(lv_obj_t *scr);
  void buildEq(lv_obj_t *scr);
  void buildSetup(lv_obj_t *scr);
  void buildCal(lv_obj_t *scr);
  void refreshList();
  void refreshEq();

  lv_obj_t *panel(lv_obj_t *parent, int16_t x, int16_t y, int16_t w, int16_t h,
                  bool inset);
  lv_obj_t *screen();
  Item *newItem(Kind kind, lv_obj_t *parent, int16_t x, int16_t y, int16_t w, int16_t h);
  Item *mkText(lv_obj_t *parent, const char *initial, int16_t x, int16_t y, uint8_t scale,
               lv_color_t color, bool rightAlign = false, int16_t anchorX = 0);
  Item *mkIcon(lv_obj_t *parent, const uint32_t *rows, int16_t x, int16_t y,
               uint8_t scale, lv_color_t color);
  BtnData &mkButton(lv_obj_t *parent, int16_t x, int16_t y, int16_t w, int16_t h,
                    const uint32_t *icon, const char *label, int8_t action,
                    bool highlight = true, uint8_t sign = 0,
                    int8_t row = -1, int8_t page = 0);
  lv_obj_t *mkFill(lv_obj_t *parent, int16_t x, int16_t y, int16_t w, int16_t h,
                   lv_color_t color);
  void mkRowButton(lv_obj_t *parent, uint8_t row, int16_t y);

  static void setText(Item &it, const char *value, int maxChars);
  static void setColor(Item &it, lv_color_t color);
  static void setRightAnchor(Item &it, int16_t anchor);
  void paintButton(BtnData &b, bool pressed);
  void touchMark(int16_t x, int16_t y, bool visible);
  void setAction(int8_t action) { action_ = action; }
  void loadScreen(lv_obj_t *scr);
  void attachTouchMarker(lv_obj_t *scr);

  // ---- draw callbacks -----------------------------------------------------------------
  static void drawItem(lv_event_t *e);
  static void drawButton(lv_event_t *e);
  static void buttonEvent(lv_event_t *e);
  static void screenPressed(lv_event_t *e);
  static void screenReleased(lv_event_t *e);

  // ---- state --------------------------------------------------------------------------
  lv_obj_t *scrMain_ = nullptr;
  lv_obj_t *scrList_ = nullptr;
  lv_obj_t *scrEq_ = nullptr;
  lv_obj_t *scrSetup_ = nullptr;
  lv_obj_t *scrCal_ = nullptr;
  lv_obj_t *cur_ = nullptr;

  Item items_[MAX_ITEMS];
  uint16_t itemCount_ = 0;
  BtnData btns_[MAX_BTNS];
  uint8_t btnCount_ = 0;

  lv_obj_t *markObj_ = nullptr;
  Item *markItem_ = nullptr;

  Item *station_ = nullptr;
  Item *title_ = nullptr;
  Item *signalIcon_ = nullptr;
  Item *linkValue_ = nullptr;
  Item *heapValue_ = nullptr;
  Item *volValue_ = nullptr;
  Item *codec_ = nullptr;
  Item *kbps_ = nullptr;
  Item *fmt_ = nullptr;
  Item *clock_ = nullptr;
  Item *vu_ = nullptr;
  Item *bufferValue_ = nullptr;
  Item *disc_ = nullptr;
  Item *playIcon_ = nullptr;
  Item *playLabel_ = nullptr;
  Item *uptime_ = nullptr;
  Item *ipLabel_ = nullptr;
  Item *ip_ = nullptr;
  Item *fwLabel_ = nullptr;

  Item *listRange_ = nullptr;
  Item *listEmpty_ = nullptr;
  lv_obj_t *listRows_[LIST_ROWS] = {nullptr};
  Item *listNames_[LIST_ROWS] = {nullptr};
  Item *listNums_[LIST_ROWS] = {nullptr};
  Item *listUp_ = nullptr;
  Item *listDown_ = nullptr;

  Item *eqCustom_ = nullptr;
  BtnData *eqPresetBtns_[EQ_PRESETS] = {nullptr};
  Item *eqPresetLabels_[EQ_PRESETS] = {nullptr};
  Item *eqBandValues_[3] = {nullptr};
  Item *eqBars_[3] = {nullptr};
  uint8_t eqPresetCount_ = EQ_PRESETS;
  uint8_t eqPresetSel_ = 0;  // last tapped preset (for the action, not the highlight)
  uint8_t eqBandSel_ = 0;    // last tapped band (for the action)
  uint8_t eqPreset_ = 0;
  int8_t eqGain_[3] = {0, 0, 0};
  uint8_t vuFrame_ = 0;

  Item *calTitle_ = nullptr;
  Item *calTarget_ = nullptr;
  Item *setupSsid_ = nullptr;
  Item *setupPass_ = nullptr;
  Item *setupIp_ = nullptr;

  const char *listSrc_[LIST_MAX] = {nullptr};
  uint8_t listTotal_ = 0;
  uint8_t listTop_ = 0;
  uint8_t listSel_ = 0xFF;

  bool running_ = false;
  int8_t action_ = 0;
  const char *fwName_ = "";
};
