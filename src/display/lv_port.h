#pragma once

// LVGL port for this board: the RGB panel as an LVGL display and the GT911 as a pointer
// indev.
//
// Display: LVGL renders into a 480x40 strip buffer in PSRAM; the flush callback blits
// each finished area with Arduino_GFX's draw16bitRGBBitmap(), which does the PSRAM cache
// write-back the RGB DMA needs and keeps the DRAM bounce buffer in display_panel.cpp in
// play. The strip lives in PSRAM because internal RAM belongs to Wi-Fi/TLS and the decoder.
//
// Input: LVGL polls the GT911 every LV_INDEV_DEF_READ_PERIOD (25 ms) and applies the
// 3x3 TouchCalibration mapping. Disable the indev while the app reads raw coordinates
// for the calibration grid - both cannot own the controller (reading clears its ready
// flag).

#include <Arduino.h>

class DisplayPanel;
class TouchGt911;
class TouchCalibration;

namespace lvport {

bool begin(DisplayPanel &panel, TouchGt911 &touch, TouchCalibration &cal);

// Runs one lv_timer_handler() pass; returns the microseconds it took.
uint32_t loop();

// The calibration screen samples the raw controller itself, so the indev must let go
// of it for the duration.
void touchEnabled(bool enabled);
bool touchEnabled();

// Last mapped touch point; false while the finger is up. Used for the on-screen touch
// marker, without going through LVGL's indev state machine.
bool lastPoint(uint16_t &x, uint16_t &y);

}  // namespace lvport
