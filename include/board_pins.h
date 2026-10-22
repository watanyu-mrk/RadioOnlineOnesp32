#pragma once

// Board pin map for Guition ESP32-S3-4848S040.
//
// Sources (cross-checked, see docs/phase1_hardware_audit.md):
//  - Arduino_GFX PDQgraphicstest board definition ESP32_4848S040_86BOX_GUITION
//  - HomeDing DisplayST7701Adapter (st7701_4848S040_init)
//  - ESPHome devices config for guition-esp32-s3-4848s040
//
// These are the pins used by every public project for this exact model.
// They are NOT taken from a vendor schematic (the datasheet pinout page is an image).

// ---------- Display: ST7701, 480x480, RGB565 + 3-wire 9-bit SPI ----------
#define LCD_CS 39
#define LCD_SCK 48
#define LCD_MOSI 47
#define LCD_DE 18
#define LCD_VSYNC 17
#define LCD_HSYNC 16
#define LCD_PCLK 21

#define LCD_R0 11
#define LCD_R1 12
#define LCD_R2 13
#define LCD_R3 14
#define LCD_R4 0

#define LCD_G0 8
#define LCD_G1 20
#define LCD_G2 3
#define LCD_G3 46
#define LCD_G4 9
#define LCD_G5 10

#define LCD_B0 4
#define LCD_B1 5
#define LCD_B2 6
#define LCD_B3 7
#define LCD_B4 15

#define LCD_BL 38  // backlight, LEDC PWM

// ---------- Touch: GT911 on I2C ----------
#define TOUCH_SDA 19
#define TOUCH_SCL 45
#define TOUCH_INT -1  // not wired to a GPIO
#define TOUCH_RST -1  // not wired to a GPIO
#define GT911_ADDR_PRIMARY 0x5D
#define GT911_ADDR_ALT 0x14

// ---------- microSD (SPI shared with the display) ----------
#define SD_CS 42
#define SD_MISO 41

// ---------- Relays (shared with the NS4168 I2S lines) ----------
#define RELAY_L1 40
#define RELAY_L2 2
#define RELAY_L3 1

// ---------- Audio: I2S to the onboard NS4168 mono class-D amp ----------
// Same three pins as the relays. With relays unused, they can carry I2S.
// The vendor manual says audio needs 0R R25/R26/R27 moved to R21/R22/R23;
// `src/audio_test` exists to prove whether that is true on this board.
// A stereo I2S DAC (PCM5102A etc.) can tap these same lines later for line-out.
#define I2S_BCLK 1
#define I2S_LRCLK 2
#define I2S_DOUT 40
