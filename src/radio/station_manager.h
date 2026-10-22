#pragma once

// Station list + active index, kept in RAM and persisted through SettingsStore.
//
// Same policy as the old RadioConfig (max 12 stations, one active index, first boot seeds
// one station from STREAM_URL) plus next/previous navigation, so the player and the web
// portal share a single source of truth.

#include <Arduino.h>

#include "storage/settings_store.h"

class StationManager {
 public:
  void begin(SettingsStore *store);

  // ---- Wi-Fi -------------------------------------------------------------------------
  bool hasWifi() const { return store_ && store_->hasWifi(); }
  const char *ssid() const { return store_ ? store_->ssid() : ""; }
  const char *pass() const { return store_ ? store_->pass() : ""; }
  void setWifi(const char *ssid, const char *pass);

  // ---- Stations ----------------------------------------------------------------------
  uint8_t count() const { return count_; }
  const StoredStation &station(uint8_t i) const;
  uint8_t active() const { return active_ < count_ ? active_ : 0; }
  const StoredStation &current() const { return station(active()); }

  void setActive(uint8_t idx);

  // Adds a station and returns its index, or -1 when the list is full / input is invalid.
  int add(const char *name, const char *url);
  bool remove(uint8_t idx);
  void ensureDefault(const char *url);

  // Wraps around the list. Returns the new index, or -1 when there is nothing to move to.
  int16_t step(int8_t delta);

 private:
  void persist();

  SettingsStore *store_ = nullptr;
  uint8_t count_ = 0;
  uint8_t active_ = 0;
  StoredStation stations_[RADIO_MAX_STATIONS];
};
