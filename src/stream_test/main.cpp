// Phase 4 - internet radio firmware, modular build.
//
// This file only wires the modules together and owns the app-level policy:
// boot order, the serial console keys, the panel refresh cadence and the stats log.
//
//   storage/   NVS persistence (Wi-Fi, stations, touch calibration)
//   radio/     station list, Wi-Fi link + clock, play/stop/next/prev + restart watchdog
//   audio/     ESP32-audioI2S wrapper (setPinout once, volume/mute, ICY metadata)
//   display/   ST7701 panel, GT911 touch, raw->screen calibration, LVGL port
//   ui/        retro hi-fi faceplate in LVGL (pure view + action queue)
//   web/       AP setup portal
//
// Everything shown on the panel comes from the real module state - nothing is faked.

#include <Arduino.h>

#include "app_config.h"
#include "board_pins.h"
#include "secrets.h"

#include "audio/audio_engine.h"
#include "display/display_panel.h"
#include "display/lv_port.h"
#include "display/touch_calibration.h"
#include "display/touch_gt911.h"
#include "radio/network_link.h"
#include "radio/radio_player.h"
#include "radio/station_manager.h"
#include "storage/settings_store.h"
#include "ui/lv_ui.h"
#include "web/web_setup.h"

static SettingsStore gStore;
static StationManager gStations;
static NetworkLink gNet;
static AudioEngine gAudio;
static RadioPlayer gPlayer;
static DisplayPanel gPanel;
static TouchGt911 gTouch;
static TouchCalibration gCal;
static LvUi gUi;
static WebSetup gPortal;

static bool uiReady = false;
static bool touchReady = false;
static bool setupMode = false;
static uint32_t lastWifiRetry = 0;
static uint32_t lastStats = 0;
static uint32_t lastPos = 0;
static uint32_t gUiMaxUs = 0;     // slowest lv_timer_handler() pass this second
static uint32_t gUiReportUs = 0;  // snapshot for the stats line
// Repaint switch, normally UI_DYNAMIC. 'u' turns the periodic redraw off so a sound test
// can tell whether the display work is what the audio problem follows.
static bool gUiDynamic = UI_DYNAMIC;

// ---- setup / station helpers ------------------------------------------------------------

static void enterSetup() {
  Serial.println("setup     : entering AP setup mode");
  gPlayer.stop();
  setupMode = true;
  gPortal.begin(&gStations);
  if (uiReady) {
    gUi.setupScreen(SETUP_AP_SSID, SETUP_AP_PASS, gPortal.ip().c_str());
  }
}

// Volume rocker (on-screen buttons) and the serial keys both land here.
static void applyVolume(int delta) {
  int v = (int)gAudio.volume() + delta;
  if (v < 0) v = 0;
  if (v > AUDIO_VOLUME_MAX) v = AUDIO_VOLUME_MAX;
  gAudio.setVolume((uint8_t)v);
  Serial.printf("volume    : %u/%u%s\n", (unsigned)gAudio.volume(), AUDIO_VOLUME_MAX,
                gAudio.muted() ? " (muted)" : "");
}

static void showStationList() {
  static const char *names[RADIO_MAX_STATIONS];
  uint8_t n = gStations.count();
  for (uint8_t i = 0; i < n; i++) names[i] = gStations.station(i).name;
  gUi.openList(names, n);
}

static void pickStation(uint8_t idx) {
  if (idx >= gStations.count()) return;
  gStations.setActive(idx);
  Serial.printf("station   : picked %u \"%s\"\n", (unsigned)idx + 1,
                gStations.station(idx).name);
  gPlayer.playActive();
}

static void startCalibration() {
  gCal.start();
  lvport::touchEnabled(false);  // the calibration grid samples the raw controller
  if (uiReady) {
    gUi.calibrationTarget(0, TouchCalibration::POINTS, TouchCalibration::targetX(0),
                          TouchCalibration::targetY(0));
  }
}

// ---- serial console ---------------------------------------------------------------------

// Returns true when the sketch has to leave the normal loop (setup mode / reboot).
static bool handleSerial() {
  while (Serial.available()) {
    int c = Serial.read();
    if (c == 'm' || c == 'M') {
      gAudio.setMuted(!gAudio.muted());
      Serial.printf("test      : %s (volume=%u) - watch the panel now\n",
                    gAudio.muted() ? "MUTED" : "unmuted", (unsigned)gAudio.volume());
    } else if (c == 'v' || c == '+' || c == 'V' || c == '-') {
      applyVolume((c == 'v' || c == '+') ? 1 : -1);
    } else if (c == 'n' || c == 'N') {
      gPlayer.changeStation(1);  // same path as the NEXT button
    } else if (c == 'b' || c == 'B') {
      gPlayer.changeStation(-1);
    } else if (c == 's' || c == 'S') {
      gPlayer.toggle();
    } else if (c == 'u' || c == 'U') {
      gUiDynamic = !gUiDynamic;
      Serial.printf("test      : panel repaint %s (sound test)\n", gUiDynamic ? "ON" : "OFF");
    } else if (c == 't' || c == 'T') {
      gTouch.dump();
      gTouch.dumpConfig();
    } else if (c == 'c' || c == 'C') {
      gTouch.dumpConfig();
    } else if (c == 'k' || c == 'K') {
      startCalibration();
    } else if (c == 'w' || c == 'W') {
      enterSetup();
      return true;
    } else if (c == 'r' || c == 'R') {
      Serial.println("test      : reboot requested");
      delay(120);
      ESP.restart();
    }
  }
  return false;
}

// ---- LVGL + actions ---------------------------------------------------------------------

static void lvTick() {
  uint32_t us = lvport::loop();
  if (us > gUiMaxUs) gUiMaxUs = us;
}

// LVGL turned a touch into an action; apply it. Everything below talks to modules, never
// to the UI, so the faceplate stays a view.
static void pumpActions() {
  int8_t a;
  while ((a = gUi.takeAction()) != LvUi::ACT_NONE) {
    switch (a) {
      case LvUi::ACT_VOL_UP: applyVolume(1); break;
      case LvUi::ACT_VOL_DOWN: applyVolume(-1); break;
      case LvUi::ACT_PLAY:  // one button, real state decides play or stop
        gPlayer.toggle();
        break;
      case LvUi::ACT_STOP: gPlayer.stop(); break;
      case LvUi::ACT_PREV: gPlayer.changeStation(-1); break;
      case LvUi::ACT_NEXT: gPlayer.changeStation(1); break;
      case LvUi::ACT_LIST: showStationList(); break;
      case LvUi::ACT_LIST_ITEM: {
        uint8_t idx = gUi.listIndex();
        gUi.closeList();
        pickStation(idx);
        break;
      }
      case LvUi::ACT_LIST_CLOSE: gUi.closeList(); break;
      case LvUi::ACT_SETUP: enterSetup(); return;
      case LvUi::ACT_SETUP_DONE:
        Serial.println("setup     : DONE tapped, rebooting");
        delay(200);
        ESP.restart();
        break;
      default: break;
    }
  }
}

// Calibration keeps the raw controller to itself (the LVGL indev is disabled), so the
// grid is sampled here.
static void pollCalibration() {
  static uint32_t lastPoll = 0;
  static bool calPressed = false;
  static uint32_t lastSample = 0;
  if (!gCal.active() || !touchReady) return;
  if (millis() - lastPoll < 25) return;
  lastPoll = millis();

  TouchGt911::Point raw = gTouch.read();
  // Ignore repeats of the same press: a flaky sample can look like down/up/down and would
  // record the same touch as two calibration points.
  if (raw.pressed && !calPressed && millis() - lastSample > 400) {
    calPressed = true;
    lastSample = millis();
    uint8_t step = gCal.step();
    Serial.printf("cal       : target %u at (%d,%d) -> raw=(%u,%u)\n", (unsigned)step + 1,
                  TouchCalibration::targetX(step), TouchCalibration::targetY(step), raw.x,
                  raw.y);
    if (gCal.sample(raw.x, raw.y)) {
      Serial.println("cal       : done, redrawing the faceplate");
      lvport::touchEnabled(true);
      gUi.showMain();
    } else {
      uint8_t next = gCal.step();
      gUi.calibrationTarget(next, TouchCalibration::POINTS, TouchCalibration::targetX(next),
                            TouchCalibration::targetY(next));
    }
  } else if (!raw.pressed) {
    calPressed = false;
  }
}

// ---- panel refresh + stats --------------------------------------------------------------

static void refreshPanel() {
  static uint32_t lastUi = 0;
  if (!(uiReady && gUiDynamic) || gCal.active()) return;
  if (millis() - lastUi < 1000) return;
  lastUi = millis();
  gUiReportUs = gUiMaxUs;
  gUiMaxUs = 0;

  UiInfo info = {};
  info.wifiConnected = gNet.connected();
  info.ip = gNet.ipString();
  info.rssi = gNet.rssi();
  info.streamRunning = gAudio.running();
  info.station = gAudio.stationName();
  info.title = gAudio.title();
  info.bitrate = gAudio.bitrate();
  info.codec = gAudio.codec();
  info.sampleRate = gAudio.sampleRate();
  info.channels = gAudio.channels();
  info.playedSec = gAudio.playedSeconds();
  info.bufferBytes = gAudio.bufferBytes();
  info.heapFree = ESP.getFreeHeap();
  info.uptimeSec = millis() / 1000;
  info.timeValid = gNet.clock(info.hour, info.minute, info.second);
  info.volume = gAudio.volume();
  info.muted = gAudio.muted();

  gUi.update(info);
}

static void logStats() {
  if (millis() - lastStats < 5000) return;
  lastStats = millis();

  // getAudioFilePosition() stays 0 on live streams; the play clock and the input buffer
  // fill level are the real progress indicators here.
  uint32_t played = gAudio.playedSeconds();
  // A restart rewinds the play clock, so only report a delta when it actually advanced.
  uint32_t delta = played >= lastPos ? played - lastPos : 0;
  uint8_t hh = 0, mm = 0, ss = 0;
  char clk[12] = "--:--:--";
  if (gNet.clock(hh, mm, ss)) snprintf(clk, sizeof(clk), "%02u:%02u:%02u", hh, mm, ss);
  Serial.printf("state     : running=%d played=%lus (+%lus) bitrate=%lu codec=%s %luHz/%uch "
                "buf=%lu rssi=%d heap=%u time=%s vol=%u ui=%luus touch=%lu/%lu st=0x%02X "
                "vu=0x%04X\n",
                gAudio.running() ? 1 : 0, (unsigned long)played, (unsigned long)delta,
                (unsigned long)gAudio.bitrate(), gAudio.codec(),
                (unsigned long)gAudio.sampleRate(), gAudio.channels(),
                (unsigned long)gAudio.bufferBytes(), gNet.rssi(), (unsigned)ESP.getFreeHeap(),
                clk, (unsigned)gAudio.volume(), (unsigned long)gUiReportUs,
                (unsigned long)gTouch.okReads(), (unsigned long)gTouch.failReads(),
                gTouch.lastStatus(), (unsigned)gAudio.vu());
  lastPos = played;
}

// -----------------------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("==================================================");
  Serial.println(" Internet radio - modular firmware (LVGL UI)");
  Serial.println("==================================================");
  Serial.printf("chip      : %s, psram found=%d free=%u\n", ESP.getChipModel(), psramFound(),
                (unsigned)ESP.getFreePsram());
  Serial.printf("i2s       : BCLK=%d LRCLK=%d DOUT=%d\n", I2S_BCLK, I2S_LRCLK, I2S_DOUT);
  Serial.printf("display   : RGB bounce buffer %d lines (%u px, 2 x %u bytes DRAM)\n",
                LCD_BOUNCE_LINES, (unsigned)LCD_BOUNCE_PX, (unsigned)(LCD_BOUNCE_PX * 2));
  Serial.printf("lvgl      : %d.%d.%d\n", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
                LVGL_VERSION_PATCH);
  Serial.println("test      : 'm' mute, 'v'/'V' volume, 'n'/'b' station, 's' play-stop");
  Serial.println("test      : 'k' touch calibration, 'w' setup portal, 'r' reboot");

  gStore.begin();
  gStations.begin(&gStore);
  gStations.ensureDefault(STREAM_URL);
  if (!gStations.hasWifi() && strlen(WIFI_SSID) > 0) {
    gStations.setWifi(WIFI_SSID, WIFI_PASS);  // one-time migration from secrets.h
    Serial.println("wifi      : migrated credentials from secrets.h");
  }
  Serial.printf("config    : %u station(s), wifi %s\n", (unsigned)gStations.count(),
                gStations.hasWifi() ? "stored" : "MISSING");

  gAudio.begin();
  gPlayer.begin(&gStations, &gAudio);

  touchReady = gTouch.begin();
  Serial.printf("touch     : %s\n",
                touchReady ? "GT911 ready (polled I2C)" : "GT911 NOT FOUND");
  if (touchReady) {
    gTouch.dump();        // product id / config version / status at boot, for comparison
    gTouch.dumpConfig();  // resolution the controller is configured for (should be 480x480)
  }

  // Identity is the correct default now that the point bytes are parsed properly: raw
  // values are already screen pixels. Calibration stays available ('k') for fine tuning.
  gCal.begin(&gStore);
  Serial.printf("touch     : mapping %s\n", gCal.loaded() ? "calibrated" : "identity");

  uiReady = gPanel.begin() && lvport::begin(gPanel, gTouch, gCal) && gUi.begin();
  Serial.printf("display   : %s (%u LVGL objects)\n",
                uiReady ? "ready (ST7701 480x480, LVGL)" : "INIT FAILED",
                (unsigned)gUi.objectCount());
  if (uiReady) gUi.showMain();

  if (!gStations.hasWifi()) {
    enterSetup();  // nothing stored yet: bring up the phone portal instead of failing
    return;
  }

  if (gNet.connect(gStations.ssid(), gStations.pass())) {
    gPlayer.playActive();
  } else {
    lastWifiRetry = millis();
  }
  lastStats = millis();
}

void loop() {
  if (setupMode) {
    while (Serial.available()) {
      int c = Serial.read();
      if (c == 'r' || c == 'R') {  // escape hatch: the board is otherwise stuck here
        Serial.println("setup     : reboot requested over serial");
        delay(120);
        ESP.restart();
      }
    }
    gPortal.loop();
    if (uiReady) {
      lvTick();
      pumpActions();
    }
    delay(2);
    return;
  }

  if (!gNet.connected()) {
    if (millis() - lastWifiRetry > 15000) {
      lastWifiRetry = millis();
      Serial.println("wifi      : lost, reconnecting");
      if (gNet.connect(gStations.ssid(), gStations.pass(), 10000)) gPlayer.playActive();
    }
    if (uiReady) lvTick();
    delay(100);
    return;
  }

  if (handleSerial()) return;  // entered setup mode

  if (uiReady) {
    lvTick();
    pumpActions();
  }
  pollCalibration();
  gPlayer.tick();
  refreshPanel();
  logStats();

  delay(1);
}
