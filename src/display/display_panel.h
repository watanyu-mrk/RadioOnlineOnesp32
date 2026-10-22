#pragma once

// ST7701 480x480 RGB panel + backlight. Everything hardware-specific about the display
// lives here; the UI only receives the Arduino_GFX pointer.
//
// The framebuffer is 480x480x2 = 460 KB in PSRAM and the LCD DMA streams through two DRAM
// bounce buffers of LCD_BOUNCE_LINES lines (see app_config.h) - that is the setting that
// stopped the left/right jitter while streaming.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

class DisplayPanel {
 public:
  bool begin();
  Arduino_GFX *gfx() const { return gfx_; }
  void setBacklight(uint8_t level);

 private:
  void backlightBegin();

  Arduino_DataBus *bus_ = nullptr;
  Arduino_ESP32RGBPanel *panel_ = nullptr;
  Arduino_RGB_Display *gfx_ = nullptr;
};
