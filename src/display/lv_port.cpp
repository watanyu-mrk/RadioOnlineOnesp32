#include "display/lv_port.h"

#include <lvgl.h>

#include "display/display_panel.h"
#include "display/touch_calibration.h"
#include "display/touch_gt911.h"

namespace {

Arduino_GFX *gfx = nullptr;
TouchGt911 *touchDev = nullptr;
TouchCalibration *calDev = nullptr;

lv_disp_draw_buf_t drawBuf;
lv_disp_drv_t dispDrv;
lv_indev_drv_t indevDrv;
lv_indev_t *indev = nullptr;
lv_color_t *drawPixels = nullptr;

// LVGL renders into a strip buffer and we push it through Arduino_GFX's own bitmap blit.
// That path does the PSRAM cache write-back the RGB DMA needs (direct writes into the
// framebuffer would stay in cache and tear), and it keeps the DRAM bounce buffer in play.
// The strip lives in PSRAM: internal RAM is what Wi-Fi/TLS and the audio decoder need.
constexpr uint16_t STRIP_LINES = 40;

bool touchOn = true;
bool lastPressed = false;
uint16_t lastX = 0, lastY = 0;

void flushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
  const uint16_t w = (uint16_t)(area->x2 - area->x1 + 1);
  const uint16_t h = (uint16_t)(area->y2 - area->y1 + 1);
  gfx->draw16bitRGBBitmap((int16_t)area->x1, (int16_t)area->y1, (uint16_t *)&px->full, w, h);
  lv_disp_flush_ready(drv);
}

// Same diagnostic log the sketch had when it polled the panel directly: one line per
// press and one per release, with the raw sample for touch debugging.
void indevRead(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  (void)drv;
  static bool pressed = false;
  static bool reported = false;
  static uint16_t sx = 0, sy = 0;

  if (!touchDev || !calDev || !touchOn) {
    data->state = LV_INDEV_STATE_RELEASED;
    data->point.x = sx;
    data->point.y = sy;
    return;
  }

  bool hasSample = false;
  TouchGt911::Point raw = touchDev->read(&hasSample);
  if (hasSample) {
    if (raw.pressed) {
      pressed = true;
      calDev->map(raw.x, raw.y, sx, sy);
      lastPressed = true;
      lastX = sx;
      lastY = sy;
    } else {
      pressed = false;
      lastPressed = false;
    }
    if (pressed != reported) {
      reported = pressed;
      const uint8_t *b = touchDev->lastBytes();
      Serial.printf("touch     : %s x=%u y=%u (raw %u,%u) bytes %02X %02X %02X %02X %02X "
                    "%02X %02X\n",
                    pressed ? "down" : "up", sx, sy, raw.x, raw.y, b[0], b[1], b[2], b[3],
                    b[4], b[5], b[6]);
    }
  }

  data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  data->point.x = sx;
  data->point.y = sy;
}

}  // namespace

namespace lvport {

bool begin(DisplayPanel &panel, TouchGt911 &touch, TouchCalibration &cal) {
  gfx = panel.gfx();
  touchDev = &touch;
  calDev = &cal;
  if (!gfx) return false;

  lv_init();

  drawPixels = (lv_color_t *)heap_caps_malloc(480 * STRIP_LINES * sizeof(lv_color_t),
                                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!drawPixels) {
    Serial.println("lvgl      : draw buffer allocation failed");
    return false;
  }
  lv_disp_draw_buf_init(&drawBuf, drawPixels, nullptr, 480 * STRIP_LINES);
  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = 480;
  dispDrv.ver_res = 480;
  dispDrv.flush_cb = flushCb;
  dispDrv.draw_buf = &drawBuf;
  if (!lv_disp_drv_register(&dispDrv)) return false;

  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = indevRead;
  // Volume rocker repeats while the finger stays down (used to be a 300 ms timer).
  indevDrv.long_press_time = 300;
  indevDrv.long_press_repeat_time = 300;
  indev = lv_indev_drv_register(&indevDrv);
  if (!indev) return false;

  Serial.printf("lvgl      : v8.3 display 480x480, %u-line strip in PSRAM, indev GT911\n",
                (unsigned)STRIP_LINES);
  return true;
}

uint32_t loop() {
  uint32_t t0 = micros();
  lv_timer_handler();
  return micros() - t0;
}

void touchEnabled(bool enabled) {
  touchOn = enabled;
  if (!enabled) lastPressed = false;
}

bool touchEnabled() { return touchOn; }

bool lastPoint(uint16_t &x, uint16_t &y) {
  x = lastX;
  y = lastY;
  return lastPressed;
}

}  // namespace lvport
