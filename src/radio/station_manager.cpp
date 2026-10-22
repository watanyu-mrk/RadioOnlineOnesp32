#include "radio/station_manager.h"

void StationManager::begin(SettingsStore *store) {
  store_ = store;
  count_ = store_ ? store_->loadStations(stations_, RADIO_MAX_STATIONS) : 0;
  active_ = store_ ? store_->loadActive() : 0;
  if (active_ >= count_) active_ = 0;
}

void StationManager::setWifi(const char *ssid, const char *pass) {
  if (store_) store_->setWifi(ssid, pass);
}

const StoredStation &StationManager::station(uint8_t i) const {
  static const StoredStation kEmpty = {"", ""};
  return count_ ? stations_[i < count_ ? i : 0] : kEmpty;
}

void StationManager::setActive(uint8_t idx) {
  active_ = (count_ && idx < count_) ? idx : 0;
  if (store_) store_->saveActive(active_);
}

int StationManager::add(const char *name, const char *url) {
  if (count_ >= RADIO_MAX_STATIONS) return -1;
  if (!url || !url[0]) return -1;
  StoredStation &s = stations_[count_];
  snprintf(s.name, sizeof(s.name), "%s", (name && name[0]) ? name : url);
  snprintf(s.url, sizeof(s.url), "%s", url);
  count_++;
  persist();
  return count_ - 1;
}

bool StationManager::remove(uint8_t idx) {
  if (idx >= count_) return false;
  for (uint8_t i = idx; i + 1 < count_; i++) stations_[i] = stations_[i + 1];
  count_--;
  if (active_ >= count_) active_ = count_ ? count_ - 1 : 0;
  persist();
  if (store_) store_->saveActive(active_);
  return true;
}

void StationManager::ensureDefault(const char *url) {
  if (count_ > 0 || !url || !url[0]) return;
  add("Default station", url);
}

int16_t StationManager::step(int8_t delta) {
  if (count_ == 0) return -1;
  int idx = (int)active() + delta;
  if (idx < 0) idx = (int)count_ - 1;
  if (idx >= (int)count_) idx = 0;
  setActive((uint8_t)idx);
  return (int16_t)idx;
}

void StationManager::persist() {
  if (store_) store_->saveStations(stations_, count_);
}
