#include "storage/settings_store.h"

bool SettingsStore::begin() {
  opened_ = prefs_.begin("radio", false);
  if (!opened_) return false;
  snprintf(ssid_, sizeof(ssid_), "%s", prefs_.getString("ssid", "").c_str());
  snprintf(pass_, sizeof(pass_), "%s", prefs_.getString("pass", "").c_str());
  return true;
}

void SettingsStore::setWifi(const char *ssid, const char *pass) {
  snprintf(ssid_, sizeof(ssid_), "%s", ssid ? ssid : "");
  snprintf(pass_, sizeof(pass_), "%s", pass ? pass : "");
  if (!opened_) return;
  prefs_.putString("ssid", ssid_);
  prefs_.putString("pass", pass_);
}

uint8_t SettingsStore::loadStations(StoredStation *out, uint8_t max) {
  if (!opened_ || !out || max == 0) return 0;
  uint8_t n = prefs_.getUChar("n", 0);
  if (n > max) n = 0;  // corrupt / foreign namespace: start over rather than overrun
  for (uint8_t i = 0; i < n; i++) {
    char key[8];
    snprintf(key, sizeof(key), "n%u", (unsigned)i);
    String name = prefs_.getString(key, "");
    snprintf(key, sizeof(key), "u%u", (unsigned)i);
    String url = prefs_.getString(key, "");
    snprintf(out[i].name, sizeof(out[i].name), "%s", name.c_str());
    snprintf(out[i].url, sizeof(out[i].url), "%s", url.c_str());
  }
  return n;
}

void SettingsStore::saveStations(const StoredStation *in, uint8_t count) {
  if (!opened_ || !in) return;
  if (count > RADIO_MAX_STATIONS) count = RADIO_MAX_STATIONS;
  prefs_.putUChar("n", count);
  for (uint8_t i = 0; i < count; i++) {
    char key[8];
    snprintf(key, sizeof(key), "n%u", (unsigned)i);
    prefs_.putString(key, in[i].name);
    snprintf(key, sizeof(key), "u%u", (unsigned)i);
    prefs_.putString(key, in[i].url);
  }
}

uint8_t SettingsStore::loadActive() {
  return opened_ ? prefs_.getUChar("active", 0) : 0;
}

void SettingsStore::saveActive(uint8_t idx) {
  if (opened_) prefs_.putUChar("active", idx);
}

// ---- equalizer ------------------------------------------------------------------------

bool SettingsStore::loadEq(uint8_t &preset, int8_t &low, int8_t &mid, int8_t &high) {
  preset = 0;
  low = mid = high = 0;
  if (!opened_ || !prefs_.isKey("eqp")) return false;
  preset = prefs_.getUChar("eqp", 0);
  low = prefs_.getChar("eql", 0);
  mid = prefs_.getChar("eqm", 0);
  high = prefs_.getChar("eqh", 0);
  return true;
}

void SettingsStore::saveEq(uint8_t preset, int8_t low, int8_t mid, int8_t high) {
  if (!opened_) return;
  prefs_.putUChar("eqp", preset);
  prefs_.putChar("eql", low);
  prefs_.putChar("eqm", mid);
  prefs_.putChar("eqh", high);
}

// ---- touch calibration ----------------------------------------------------------------

bool SettingsStore::loadTouchCal(TouchCalData &out) {
  memset(&out, 0, sizeof(out));
  Preferences p;
  if (!p.begin("touch3", true)) return false;
  bool ok = p.getBool("grid", false) && p.isKey("x0");
  if (ok) {
    for (uint8_t i = 0; i < 9; i++) {
      char key[8];
      snprintf(key, sizeof(key), "x%u", (unsigned)i);
      out.rawX[i] = p.getUShort(key, 0);
      snprintf(key, sizeof(key), "y%u", (unsigned)i);
      out.rawY[i] = p.getUShort(key, 0);
    }
    out.aff[0] = p.getFloat("a", 1.0f);
    out.aff[1] = p.getFloat("b", 0.0f);
    out.aff[2] = p.getFloat("c", 0.0f);
    out.aff[3] = p.getFloat("d", 0.0f);
    out.aff[4] = p.getFloat("e", 1.0f);
    out.aff[5] = p.getFloat("f", 0.0f);
    out.valid = true;
  }
  p.end();
  return ok;
}

void SettingsStore::saveTouchCal(const TouchCalData &in) {
  Preferences p;
  if (!p.begin("touch3", false)) return;
  for (uint8_t i = 0; i < 9; i++) {
    char key[8];
    snprintf(key, sizeof(key), "x%u", (unsigned)i);
    p.putUShort(key, in.rawX[i]);
    snprintf(key, sizeof(key), "y%u", (unsigned)i);
    p.putUShort(key, in.rawY[i]);
  }
  p.putFloat("a", in.aff[0]);
  p.putFloat("b", in.aff[1]);
  p.putFloat("c", in.aff[2]);
  p.putFloat("d", in.aff[3]);
  p.putFloat("e", in.aff[4]);
  p.putFloat("f", in.aff[5]);
  p.putBool("grid", true);
  p.end();
}
