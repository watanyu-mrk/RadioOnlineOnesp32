#pragma once

// Retro "hi-fi receiver" status screen for the ST7701 480x480 panel.
//
// Design: late-70s/80s tuner faceplate - amber bezel, cream silk-screen lettering, amber
// seven-segment wall clock (Bangkok, UTC+7) with dim "ghost" segments, segmented buffer
// meter, spinning station disc and a volume rocker. Everything is drawn with Arduino_GFX
// primitives plus the 1-bit pixel icons in ui_icons.h (Pixelarticons, MIT), so there is no
// image decoder, no extra library and no PSRAM frame buffering for assets.
//
// The UI never touches the audio library or the network: the sketch fills UiInfo from the
// real module state, and handleTouch() returns an Action (volume step, station change, ...)
// for the caller to apply - this class stays a pure view.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

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

class UiStatus {
 public:
  // Draws on an already initialised panel (see DisplayPanel).
  bool begin(Arduino_GFX *gfx);

  void drawStatic(const char *fwName);
  void update(const UiInfo &i);

  // What a touch on the faceplate asks the caller to do.
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
  };

  uint8_t listIndex() const { return listSel_; }

  // Full-screen calibration target. Raw GT911 coordinates do not match screen pixels on
  // this panel (taps landed tens of pixels away from the drawn buttons), so
  // TouchCalibration walks the user through nine of these.
  void calibrationTarget(uint8_t step, uint8_t total, int16_t tx, int16_t ty);

  // ---- station list and setup overlays ------------------------------------------------

  void openList(const char *const *names, uint8_t count);
  void closeList();

  // AP portal screen: the phone connects to these credentials.
  void setupScreen(const char *ssid, const char *pass, const char *ip);
  void mainScreen();

  // Touch from the GT911 (already mapped to screen pixels). Returns the action to apply,
  // ACT_NONE otherwise. Volume repeats every 300 ms while held; buttons fire once per press.
  int8_t handleTouch(bool pressed, uint16_t x, uint16_t y);

 private:
  static constexpr uint8_t MAX_LIST_ROWS = 7;

  // ---- primitives ----------------------------------------------------------------------
  void text(int16_t x, int16_t y, uint8_t size, uint16_t color, const char *fmt, ...);
  void clearText(int16_t x, int16_t y, uint8_t size, int chars);
  static void fit(char *dst, size_t n, const char *src, int maxChars);
  void face(int16_t x, int16_t y, int16_t w, int16_t h, bool inset = false);
  void drawIcon(int16_t x, int16_t y, const uint32_t *rows, uint8_t scale, uint16_t color);

  // ---- wall clock ----------------------------------------------------------------------
  static int16_t digitX(uint8_t idx);
  static int16_t colonX(uint8_t pair);
  void digit(int16_t x, int16_t y, uint8_t value);
  void segment(int16_t x, int16_t y, uint8_t idx, uint16_t color);
  void colon(uint8_t pair, uint16_t color);
  void clockGhost();
  void showClock(uint8_t hour, uint8_t minute, uint8_t second);

  // ---- meters, chips, deck, volume -----------------------------------------------------
  uint8_t meter(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t bytes);
  void chip(int16_t x, int16_t y, int16_t w, const uint32_t *icon, const char *label,
            const char *value);
  void chipValue(int16_t x, int16_t y, const char *value, uint16_t color);
  void volButton(int16_t x, int16_t y, bool plus, bool pressed);
  void volButtons(int8_t zone);
  void volValue(uint8_t volume, bool muted);
  void controlRow();
  void actionButton(int16_t x, const uint32_t *icon, const char *label, bool pressed);
  void playButton(int16_t x, bool pressed);
  void drawDisc(int16_t cx, int16_t cy, int16_t r, uint8_t phase);
  void drawList();
  void drawTouchMark(uint16_t x, uint16_t y);
  void paintPressed(int8_t action);

  Arduino_GFX *gfx_ = nullptr;

  char lastStation_[24] = {0};
  char lastTitle_[36] = {0};
  uint8_t spin_ = 0;
  uint8_t lastDigits_[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t lastSegs_ = 0xFF;
  uint8_t lastVol_ = 0xFF;
  bool lastMuted_ = false;
  enum Screen : uint8_t { SCREEN_MAIN, SCREEN_SETUP };

  bool running_ = false;
  bool repaint_ = false;
  bool markDrawn_ = false;
  Screen screen_ = SCREEN_MAIN;
  bool listOpen_ = false;
  uint8_t listCount_ = 0;
  uint8_t listSel_ = 0xFF;
  uint8_t listTop_ = 0;  // first visible row when the list is paged
  char listNames_[MAX_LIST_ROWS][40] = {};
  const char *fwName_ = "";
  int8_t zone_ = ACT_NONE;
  uint32_t lastFire_ = 0;
};
