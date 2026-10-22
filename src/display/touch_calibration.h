#pragma once

// Raw GT911 coordinates -> screen pixels, with the 3x3 grid this panel needs.
//
// Corners alone could not describe the touch layer (taps in the lower middle came out
// ~70 px away), so the screen is measured at nine points and each pair of grid cells is
// mapped per triangle; outside the grid a least-squares affine fit is used.
//
// The mapping is stored in NVS ("touch3"), and identity is the default: the point bytes
// are parsed correctly now, so raw values are already screen pixels until someone runs
// the calibration ('k' on the serial console).

#include <Arduino.h>

#include "storage/settings_store.h"

class TouchCalibration {
 public:
  static const uint8_t POINTS = 9;

  void begin(SettingsStore *store);  // loads a saved grid if one exists

  bool loaded() const { return have_ && !active_; }
  bool active() const { return active_; }
  uint8_t step() const { return step_; }

  static int16_t targetX(uint8_t i);
  static int16_t targetY(uint8_t i);

  void start();

  // Records one sample for the current target. Returns true when the grid is complete and
  // the mapping has been fitted + saved.
  bool sample(uint16_t rawX, uint16_t rawY);

  void map(uint16_t rawX, uint16_t rawY, uint16_t &sx, uint16_t &sy) const;

 private:
  void finish();

  SettingsStore *store_ = nullptr;
  TouchCalData data_ = {};
  bool have_ = false;
  bool active_ = false;
  uint8_t step_ = 0;
};
