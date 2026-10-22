#pragma once

// Firmware-wide tunables. Wiring stays in board_pins.h; this file holds the values that
// were previously buried in the sketch or in ui_status.h.

// Shown in the bottom caption strip of the panel.
#define APP_FW_NAME "stream test - WiFi + MP3 decode"

// ---- RGB panel ------------------------------------------------------------------------
// The 480x480x2 framebuffer lives in PSRAM. Without a DRAM bounce buffer the LCD DMA
// reads PSRAM directly, and Wi-Fi/TLS + audio decoding starve it: the picture jitters
// left/right. 16 lines = 2 x 15,360 B of internal RAM keeps it steady.
// Do not shrink this or raise LCD_PCLK_HZ without re-testing on the panel.
#define LCD_BOUNCE_LINES 16
#define LCD_BOUNCE_PX (480 * LCD_BOUNCE_LINES)
#define LCD_PCLK_HZ 12000000

// UI_DYNAMIC=0 keeps the panel initialised but stops the periodic repaint, so a stream
// throughput change can be attributed to panel scan-out vs. our pixel writes.
#ifndef UI_DYNAMIC
#define UI_DYNAMIC 1
#endif

// ---- Audio ----------------------------------------------------------------------------
#define AUDIO_VOLUME_MAX 21
#define AUDIO_VOLUME_DEFAULT 12

// Only restart a stream the library reports stopped after this long: a live stream stays
// "running" while it logs "slow stream", and restarting too early throws away a buffer
// that is still filling.
#define STREAM_RETRY_MS 15000

// ---- Clock ----------------------------------------------------------------------------
// POSIX TZ for Asia/Bangkok (UTC+7, no DST), so localtime_r() reports Bangkok time.
#define APP_TZ "ICT-7"
#define APP_NTP_1 "pool.ntp.org"
#define APP_NTP_2 "time.google.com"
// Anything before 2020 means NTP has not answered yet.
#define APP_TIME_VALID_MIN 1600000000UL
