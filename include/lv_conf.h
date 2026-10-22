// LVGL 8.3 configuration for the Guition ESP32-S3-4848S040 radio.
//
// Only the options that differ from the LVGL defaults are listed; lv_conf_internal.h
// fills in the rest. Build flag `-DLV_CONF_INCLUDE_SIMPLE` makes LVGL find this file
// through the project include path.
//
// Memory: the object tree and LVGL's own heap live in PSRAM. Internal RAM is the scarce
// resource on this board (RGB bounce buffer 31 KB + Wi-Fi/TLS + audio decode), and the UI
// must not take a 48 KB bite out of it. The draw buffer (480x40 strip) is PSRAM too.

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ---- colour ---- */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0  // RGB panel byte order (Arduino_GFX useBigEndian = false)

/* ---- memory ---- */
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <esp_heap_caps.h>
#define LV_MEM_CUSTOM_ALLOC(sz) heap_caps_malloc((sz), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define LV_MEM_CUSTOM_FREE(p) heap_caps_free(p)
#define LV_MEM_CUSTOM_REALLOC(p, sz) \
  heap_caps_realloc((p), (sz), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

/* ---- tick ---- */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/* ---- refresh / input periods ---- */
#define LV_DISP_DEF_REFR_PERIOD 30
#define LV_INDEV_DEF_READ_PERIOD 25  // same poll interval the sketch used before

/* ---- fonts: the faceplate draws its own 5x7 bitmap font, but the default
        theme still references Montserrat 14 ---- */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* ---- trim what this UI does not use ---- */
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0
#define LV_USE_FLEX 0
#define LV_USE_GRID 0
#define LV_USE_ANIMIMG 0
#define LV_USE_CALENDAR 0
#define LV_USE_CHART 0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LED 0
#define LV_USE_LIST 0
#define LV_USE_MENU 0
#define LV_USE_METER 0
#define LV_USE_MSGBOX 0
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0
#define LV_USE_THEME_DEFAULT 1
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0

/* ---- image decoders: the retrofit icons are 1-bit runs drawn by code ---- */
#define LV_USE_PNG 0
#define LV_USE_BMP 0
#define LV_USE_SJPG 0
#define LV_USE_GIF 0
#define LV_USE_QRCODE 0
#define LV_USE_FREETYPE 0
#define LV_USE_TINY_TTF 0
#define LV_USE_RLOTTIE 0
#define LV_USE_FFMPEG 0

#endif  // LV_CONF_H
