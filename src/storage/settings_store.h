#pragma once

// Persistent settings in NVS - the only module that talks to Preferences.
//
// Layout (namespace "radio"): ssid, pass, n, n<i>, u<i>, active.
// Layout (namespace "touch3"): a..f affine + x0..x8 / y0..y8 raw calibration grid.
// RadioConfig used to own this; the data is unchanged, so an existing board boots with
// the settings it already has.
//
// NVS strings are limited (~4000 bytes per namespace), which is why the station count and
// field sizes are capped.

#include <Arduino.h>
#include <Preferences.h>

#define RADIO_MAX_STATIONS 12
#define RADIO_NAME_LEN 40
#define RADIO_URL_LEN 160

struct StoredStation {
  char name[RADIO_NAME_LEN];
  char url[RADIO_URL_LEN];
};

// Raw touch samples for the 3x3 calibration grid plus the affine fallback.
struct TouchCalData {
  bool valid;
  uint16_t rawX[9];
  uint16_t rawY[9];
  float aff[6];  // screen = (aff[0..2]) * raw + aff[2]; (aff[3..5]) for y
};

class SettingsStore {
 public:
  bool begin();

  // ---- Wi-Fi -------------------------------------------------------------------------
  bool hasWifi() const { return ssid_[0] != 0; }
  const char *ssid() const { return ssid_; }
  const char *pass() const { return pass_; }
  void setWifi(const char *ssid, const char *pass);

  // ---- Stations ----------------------------------------------------------------------
  uint8_t loadStations(StoredStation *out, uint8_t max);
  void saveStations(const StoredStation *in, uint8_t count);
  uint8_t loadActive();
  void saveActive(uint8_t idx);

  // ---- Touch calibration --------------------------------------------------------------
  bool loadTouchCal(TouchCalData &out);
  void saveTouchCal(const TouchCalData &in);

 private:
  Preferences prefs_;
  bool opened_ = false;
  char ssid_[40] = {0};
  char pass_[64] = {0};
};
