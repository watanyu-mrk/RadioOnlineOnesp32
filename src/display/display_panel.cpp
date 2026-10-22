#include "display/display_panel.h"

#include "app_config.h"
#include "board_pins.h"

bool DisplayPanel::begin() {
  bus_ = new Arduino_SWSPI(GFX_NOT_DEFINED, LCD_CS, LCD_SCK, LCD_MOSI, GFX_NOT_DEFINED);
  panel_ = new Arduino_ESP32RGBPanel(
      LCD_DE, LCD_VSYNC, LCD_HSYNC, LCD_PCLK,
      LCD_R0, LCD_R1, LCD_R2, LCD_R3, LCD_R4,
      LCD_G0, LCD_G1, LCD_G2, LCD_G3, LCD_G4, LCD_G5,
      LCD_B0, LCD_B1, LCD_B2, LCD_B3, LCD_B4,
      1, 10, 8, 50,    // hsync: polarity, front, pulse, back
      1, 10, 8, 20,    // vsync: polarity, front, pulse, back
      0, LCD_PCLK_HZ, false, 0, 0, (size_t)LCD_BOUNCE_PX);
  gfx_ = new Arduino_RGB_Display(480, 480, panel_, 0, true, bus_, GFX_NOT_DEFINED,
                                 st7701_type9_init_operations,
                                 sizeof(st7701_type9_init_operations));

  backlightBegin();
  if (!gfx_->begin()) return false;
  gfx_->fillScreen(0x0000);
  setBacklight(255);
  return true;
}

void DisplayPanel::setBacklight(uint8_t level) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(LCD_BL, level);
#else
  ledcWrite(0, level);
#endif
}

void DisplayPanel::backlightBegin() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(LCD_BL, 5000, 8);
#else
  ledcSetup(0, 5000, 8);
  ledcAttachPin(LCD_BL, 0);
#endif
  setBacklight(0);
}
