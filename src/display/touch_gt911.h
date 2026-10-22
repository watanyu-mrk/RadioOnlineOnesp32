#pragma once

// Minimal polled GT911 driver (I2C, no external library).
//
// Same proven register sequence as the Phase 2 bring-up sketch: product-id probe on
// 0x5D/0x14, status flag at 0x814E cleared after every sample.
//
// Point record layout on this panel (verified from raw dumps of five known taps):
//   [xL xH yL yH sizeL sizeH flags]  - little-endian, NO leading track-id byte, and the
// values are already raw screen pixels (tap at 460,20 -> 0x01C4,0x002A = 452,42).
// Reading it as "track id + high-byte-first axes" scrambled x and y, which is why every
// mapping looked distorted.
// The touch INT line is not wired on this board (TOUCH_INT == -1), so polling is the only
// option; the caller decides how often.
//
// Raw-to-screen mapping is not done here: TouchCalibration owns it.

#include <Arduino.h>
#include <Wire.h>

#include "board_pins.h"

class TouchGt911 {
 public:
  struct Point {
    bool pressed;
    uint16_t x;
    uint16_t y;
    uint8_t points;
  };

  bool begin(uint8_t sda = TOUCH_SDA, uint8_t scl = TOUCH_SCL, uint32_t hz = 100000);

  bool ready() const { return ready_; }
  uint8_t address() const { return addr_; }

  // Diagnostics: how often the status register could be read at all, and what it said.
  const uint8_t *lastBytes() const { return lastBytes_; }
  uint32_t okReads() const { return okReads_; }
  uint32_t failReads() const { return failReads_; }
  uint8_t lastStatus() const { return lastStatus_; }

  // Dumps the registers that decide whether the controller is scanning.
  void dump();
  // Reports the resolution the controller itself is configured for; if xmax/ymax are not
  // 480 the chip clamps coordinates, which explains taps that all land on the same value.
  void dumpConfig();

  // Poll one sample. Clears the buffer-ready flag so the controller reports the next one.
  // `hasSample` (optional) tells the caller whether the controller had new data at all:
  // between two scans there is none, and the pressed state must then be held rather than
  // treated as a release (LVGL would see a click bouncing every poll).
  Point read(bool *hasSample);
  Point read() { return read(nullptr); }

 private:
  static const uint16_t REG_PRODUCT_ID = 0x8140;
  static const uint16_t REG_CONFIG_VERSION = 0x8047;
  static const uint16_t REG_STATUS = 0x814E;
  static const uint16_t REG_POINT1 = 0x8150;

  uint8_t addr_ = 0;
  bool ready_ = false;
  uint32_t okReads_ = 0;
  uint32_t failReads_ = 0;
  uint8_t lastStatus_ = 0;
  uint8_t lastBytes_[7] = {0};
  bool haveBytes_ = false;

  static bool read(uint8_t addr, uint16_t reg, uint8_t *buf, uint8_t len);
  static bool write8(uint8_t addr, uint16_t reg, uint8_t value);
};
