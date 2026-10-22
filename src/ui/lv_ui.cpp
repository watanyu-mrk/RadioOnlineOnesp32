#include "ui/lv_ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "display/lv_port.h"
#include "ui_icons.h"

// Faceplate font: this is the very table Arduino_GFX's setTextSize()/print() uses (5x7
// glyphs in a 6x8 cell, column-major bits), so the LVGL port keeps the exact glyphs,
// character advance and line height of the previous drawing code.
#include "font/glcdfont.h"

namespace {

// ---- palette (0xRRGGBB, same values as the Arduino_GFX faceplate) ----------------------
constexpr uint32_t P_BG = 0x0C0906;
constexpr uint32_t P_FRAME = 0xD67832;
constexpr uint32_t P_FRAME_DIM = 0x683818;
constexpr uint32_t P_PANEL = 0x1E140C;
constexpr uint32_t P_PANEL_HI = 0x2C1E10;
constexpr uint32_t P_CREAM = 0xFFECC4;
constexpr uint32_t P_AMBER = 0xFFBA4A;
constexpr uint32_t P_AMBER_DIM = 0x2E1808;
constexpr uint32_t P_DIM = 0x9E846A;
constexpr uint32_t P_GREEN = 0x60E27A;
constexpr uint32_t P_RED = 0xFF5A42;

inline lv_color_t rgb(uint32_t hex) {
  return lv_color_make((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

// ---- seven-segment geometry (identical to the Arduino_GFX version) --------------------
constexpr int16_t DIG_W = 38;
constexpr int16_t DIG_H = 66;
constexpr int16_t DIG_T = 9;
constexpr int16_t DIG_X = 22;
constexpr int16_t DIG_PITCH = 42;
constexpr int16_t PAIR_PITCH = 106;
constexpr uint8_t METER_SEGS = 27;

// ---- primitives on the LVGL draw context ----------------------------------------------

void fillRect(lv_draw_ctx_t *ctx, int16_t x, int16_t y, int16_t w, int16_t h,
              lv_color_t c) {
  if (w <= 0 || h <= 0) return;
  lv_area_t a = {x, y, (int16_t)(x + w - 1), (int16_t)(y + h - 1)};
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = c;
  d.bg_opa = LV_OPA_COVER;
  d.radius = 0;
  lv_draw_rect(ctx, &d, &a);
}

void fillCircle(lv_draw_ctx_t *ctx, int16_t cx, int16_t cy, int16_t r, lv_color_t c) {
  lv_area_t a = {(int16_t)(cx - r), (int16_t)(cy - r), (int16_t)(cx + r),
                 (int16_t)(cy + r)};
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = c;
  d.bg_opa = LV_OPA_COVER;
  d.radius = LV_RADIUS_CIRCLE;
  lv_draw_rect(ctx, &d, &a);
}

void strokeCircle(lv_draw_ctx_t *ctx, int16_t cx, int16_t cy, int16_t r, lv_color_t c) {
  lv_area_t a = {(int16_t)(cx - r), (int16_t)(cy - r), (int16_t)(cx + r),
                 (int16_t)(cy + r)};
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_opa = LV_OPA_TRANSP;
  d.border_width = 1;
  d.border_opa = LV_OPA_COVER;
  d.border_color = c;
  d.radius = LV_RADIUS_CIRCLE;
  lv_draw_rect(ctx, &d, &a);
}

void line(lv_draw_ctx_t *ctx, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
          lv_color_t c) {
  lv_point_t p1 = {x1, y1};
  lv_point_t p2 = {x2, y2};
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = c;
  d.width = 1;
  d.opa = LV_OPA_COVER;
  lv_draw_line(ctx, &d, &p1, &p2);
}

// Scanline fill; enough for the play triangle and the list paging arrows.
void fillTriangle(lv_draw_ctx_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  int16_t x2, int16_t y2, lv_color_t c) {
  const int16_t px[3] = {x0, x1, x2};
  const int16_t py[3] = {y0, y1, y2};
  int16_t ya = LV_MIN(y0, LV_MIN(y1, y2));
  int16_t yb = LV_MAX(y0, LV_MAX(y1, y2));
  for (int16_t y = ya; y <= yb; y++) {
    float lo = 1e9f, hi = -1e9f;
    for (uint8_t i = 0; i < 3; i++) {
      uint8_t j = (uint8_t)((i + 1) % 3);
      int16_t ay = py[i], by = py[j];
      if (ay == by) {
        if (y == ay) {
          lo = LV_MIN(lo, (float)LV_MIN(px[i], px[j]));
          hi = LV_MAX(hi, (float)LV_MAX(px[i], px[j]));
        }
        continue;
      }
      if (y < LV_MIN(ay, by) || y > LV_MAX(ay, by)) continue;
      float t = (float)(y - ay) / (float)(by - ay);
      float x = (float)px[i] + t * (float)(px[j] - px[i]);
      lo = LV_MIN(lo, x);
      hi = LV_MAX(hi, x);
    }
    if (hi < lo) continue;
    int16_t a = (int16_t)lroundf(lo);
    int16_t b = (int16_t)lroundf(hi);
    fillRect(ctx, a, y, (int16_t)(b - a + 1), 1, c);
  }
}

// 5x7 text, 6x8 cell, drawn as horizontal runs per glyph row.
void drawText(lv_draw_ctx_t *ctx, int16_t x, int16_t y, const char *s, uint8_t scale,
              lv_color_t c) {
  if (!s) return;
  int16_t cx = x;
  for (const char *p = s; *p; p++, cx = (int16_t)(cx + 6 * scale)) {
    uint8_t ch = (uint8_t)*p;
    if (ch < 32) continue;
    const unsigned char *g = &font[ch * 5];
    for (uint8_t row = 0; row < 8; row++) {
      uint8_t bits = 0;
      for (uint8_t col = 0; col < 5; col++) {
        bits = (uint8_t)(bits | (((pgm_read_byte(&g[col]) >> row) & 1) << col));
      }
      uint8_t i = 0;
      while (i < 5) {
        if (bits & (1 << i)) {
          uint8_t s0 = i;
          while (i < 5 && (bits & (1 << i))) i++;
          fillRect(ctx, (int16_t)(cx + s0 * scale), (int16_t)(y + row * scale),
                   (int16_t)((i - s0) * scale), scale, c);
        } else {
          i++;
        }
      }
    }
  }
}

// 1-bit pixel icon at an integer scale, drawn as horizontal runs.
void drawIcon(lv_draw_ctx_t *ctx, int16_t x, int16_t y, const uint32_t *rows,
              uint8_t scale, lv_color_t c) {
  if (!rows) return;
  for (uint8_t r = 0; r < UI_ICON_SIZE; r++) {
    uint32_t m = rows[r];
    if (!m) continue;
    uint8_t i = 0;
    while (i < UI_ICON_SIZE) {
      if (m & (1UL << i)) {
        uint8_t s = i;
        while (i < UI_ICON_SIZE && (m & (1UL << i))) i++;
        fillRect(ctx, (int16_t)(x + s * scale), (int16_t)(y + r * scale),
                 (int16_t)((i - s) * scale), scale, c);
      } else {
        i++;
      }
    }
  }
}

void segment(lv_draw_ctx_t *ctx, int16_t x, int16_t y, uint8_t idx, lv_color_t c) {
  const int16_t w = DIG_W, h = DIG_H, t = DIG_T;
  const int16_t half = h / 2;
  const int16_t upStart = t + 1;
  const int16_t upLen = half - t / 2 - 1 - upStart;
  const int16_t loStart = half + t / 2 + 1;
  const int16_t loLen = h - t - 1 - loStart;
  switch (idx) {
    case 0: fillRect(ctx, x + t, y, w - 2 * t, t, c); break;                  // A
    case 1: fillRect(ctx, x + w - t, y + upStart, t, upLen, c); break;        // B
    case 2: fillRect(ctx, x + w - t, y + loStart, t, loLen, c); break;        // C
    case 3: fillRect(ctx, x + t, y + h - t, w - 2 * t, t, c); break;          // D
    case 4: fillRect(ctx, x, y + loStart, t, loLen, c); break;                // E
    case 5: fillRect(ctx, x, y + upStart, t, upLen, c); break;                // F
    default: fillRect(ctx, x + t, y + half - t / 2, w - 2 * t, t, c); break;  // G
  }
}

void digit(lv_draw_ctx_t *ctx, int16_t x, int16_t y, uint8_t value, lv_color_t lit,
           lv_color_t dim) {
  static const uint8_t MAP[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                                  0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  uint8_t segs = MAP[value % 10];
  for (uint8_t s = 0; s < 7; s++) {
    segment(ctx, x, y, s, (segs >> s) & 1 ? lit : dim);
  }
}

int16_t digitX(uint8_t idx) {  // idx 0..5 = H H : M M : S S, relative to the clock box
  return (int16_t)((idx / 2) * PAIR_PITCH + (idx % 2) * DIG_PITCH);
}

void colon(lv_draw_ctx_t *ctx, uint8_t pair, lv_color_t c) {
  int16_t x = (int16_t)(pair * PAIR_PITCH + DIG_PITCH + DIG_W + 8);
  fillRect(ctx, x, DIG_H / 3, DIG_T, DIG_T, c);
  fillRect(ctx, x, 2 * DIG_H / 3, DIG_T, DIG_T, c);
}

void clockFace(lv_draw_ctx_t *ctx, uint8_t hour, uint8_t minute, uint8_t second,
               bool valid) {
  const uint8_t d[6] = {(uint8_t)((hour / 10) % 10), (uint8_t)(hour % 10),
                        (uint8_t)((minute / 10) % 10), (uint8_t)(minute % 10),
                        (uint8_t)((second / 10) % 10), (uint8_t)(second % 10)};
  lv_color_t amber = rgb(P_AMBER);
  lv_color_t ghost = rgb(P_AMBER_DIM);
  for (uint8_t i = 0; i < 6; i++) {
    digit(ctx, digitX(i), 0, d[i], valid ? amber : ghost, ghost);
  }
  // One-second blink on the colons, like a deck counter.
  lv_color_t c = (valid && (second & 1)) ? amber : ghost;
  colon(ctx, 0, c);
  colon(ctx, 1, c);
}

// Truncates with ".." so a long value never overruns its cell.
void fit(char *dst, size_t n, const char *src, int maxChars) {
  if (!src) {
    dst[0] = 0;
    return;
  }
  size_t len = strlen(src);
  if ((int)len <= maxChars) {
    snprintf(dst, n, "%s", src);
    return;
  }
  size_t keep = (size_t)maxChars > 2 ? (size_t)maxChars - 2 : 1;
  if (keep > n - 3) keep = n - 3;
  memcpy(dst, src, keep);
  dst[keep] = '.';
  dst[keep + 1] = '.';
  dst[keep + 2] = 0;
}

// Outline-only, transparent container (bezel rings).
lv_obj_t *strokeObj(lv_obj_t *parent, int16_t x, int16_t y, int16_t w, int16_t h,
                    lv_color_t c) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(o, c, 0);
  lv_obj_set_style_border_width(o, 1, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

}  // namespace

// -----------------------------------------------------------------------------------------
// Item drawing
// -----------------------------------------------------------------------------------------

void LvUi::drawItem(lv_event_t *e) {
  Item *it = (Item *)lv_event_get_user_data(e);
  lv_obj_t *obj = lv_event_get_target(e);
  lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
  lv_area_t co;
  lv_obj_get_coords(obj, &co);
  const int16_t x = co.x1, y = co.y1;

  switch (it->kind) {
    case K_TEXT:
      drawText(ctx, x, y, it->text, it->scale, it->color);
      break;
    case K_ICON:
      drawIcon(ctx, x, y, it->icon, it->scale, it->color);
      break;
    case K_METER: {
      const int16_t segW = (432 - (METER_SEGS - 1) * 2) / METER_SEGS;
      uint8_t lit = it->a;
      if (lit > METER_SEGS) lit = METER_SEGS;
      for (uint8_t s = 0; s < METER_SEGS; s++) {
        lv_color_t c = s < lit ? rgb(s < METER_SEGS - 5 ? P_AMBER : P_GREEN)
                               : rgb(P_AMBER_DIM);
        fillRect(ctx, (int16_t)(x + s * (segW + 2)), y, segW, 18, c);
      }
      break;
    }
    case K_CLOCK:
      clockFace(ctx, it->a, it->b, it->c, it->flag);
      break;
    case K_DISC: {
      const int16_t cx = (int16_t)(x + 15), cy = (int16_t)(y + 15);
      fillCircle(ctx, cx, cy, 15, rgb(P_PANEL_HI));
      strokeCircle(ctx, cx, cy, 15, rgb(P_FRAME_DIM));
      strokeCircle(ctx, cx, cy, 5, rgb(P_FRAME_DIM));
      float a = (float)(it->a % 96) * 6.2831853f / 96.0f;
      line(ctx, cx, cy, (int16_t)(cx + cosf(a) * 13), (int16_t)(cy + sinf(a) * 13),
           rgb(P_AMBER));
      fillCircle(ctx, cx, cy, 2, rgb(P_FRAME));
      break;
    }
    case K_PLAYICON:
      if (it->flag) {
        fillTriangle(ctx, (int16_t)(x + 1), y, (int16_t)(x + 1), (int16_t)(y + 18),
                     (int16_t)(x + 15), (int16_t)(y + 9), it->color);
      } else {
        fillRect(ctx, x, (int16_t)(y + 1), 15, 16, it->color);
      }
      break;
    case K_TARGET: {
      const int16_t cx = (int16_t)(x + 40), cy = (int16_t)(y + 40);
      strokeCircle(ctx, cx, cy, 28, rgb(P_AMBER));
      strokeCircle(ctx, cx, cy, 27, rgb(P_AMBER));
      line(ctx, (int16_t)(cx - 40), cy, (int16_t)(cx + 40), cy, rgb(P_AMBER));
      line(ctx, cx, (int16_t)(cy - 40), cx, (int16_t)(cy + 40), rgb(P_AMBER));
      fillRect(ctx, (int16_t)(cx - 3), (int16_t)(cy - 3), 6, 6, rgb(P_CREAM));
      break;
    }
    case K_ARROW:
      if (it->a == 0) {  // page up
        fillTriangle(ctx, x, (int16_t)(y + 18), (int16_t)(x + 20), (int16_t)(y + 18),
                     (int16_t)(x + 10), y, rgb(P_AMBER));
      } else {  // page down
        fillTriangle(ctx, x, y, (int16_t)(x + 20), y, (int16_t)(x + 10),
                     (int16_t)(y + 18), rgb(P_AMBER));
      }
      break;
    case K_MARK: {
      if (!it->flag) break;
      const int16_t cx = it->p1, cy = it->p2;
      lv_color_t c = it->color;
      strokeCircle(ctx, cx, cy, 13, c);
      strokeCircle(ctx, cx, cy, 12, c);
      fillRect(ctx, (int16_t)(cx - 18), (int16_t)(cy - 2), 8, 4, c);
      fillRect(ctx, (int16_t)(cx + 11), (int16_t)(cy - 2), 8, 4, c);
      fillRect(ctx, (int16_t)(cx - 2), (int16_t)(cy - 18), 4, 8, c);
      fillRect(ctx, (int16_t)(cx - 2), (int16_t)(cy + 11), 4, 8, c);
      break;
    }
    default:
      break;
  }
}

// The volume rocker draws its own + / - mark, coloured like the old volButton().
void LvUi::drawButton(lv_event_t *e) {
  BtnData *b = (BtnData *)lv_event_get_user_data(e);
  if (!b || !b->sign) return;
  lv_obj_t *obj = lv_event_get_target(e);
  lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
  lv_area_t co;
  lv_obj_get_coords(obj, &co);
  lv_color_t c = b->pressed ? rgb(P_BG) : rgb(P_AMBER);
  fillRect(ctx, (int16_t)(co.x1 + 14), (int16_t)(co.y1 + 10), 16, 3, c);
  if (b->sign == 2) fillRect(ctx, (int16_t)(co.x1 + 20), (int16_t)(co.y1 + 4), 3, 15, c);
}

// -----------------------------------------------------------------------------------------
// Object construction
// -----------------------------------------------------------------------------------------

lv_obj_t *LvUi::mkFill(lv_obj_t *parent, int16_t x, int16_t y, int16_t w, int16_t h,
                       lv_color_t color) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, color, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

lv_obj_t *LvUi::panel(lv_obj_t *parent, int16_t x, int16_t y, int16_t w, int16_t h,
                      bool inset) {
  lv_obj_t *o = mkFill(parent, x, y, w, h, rgb(inset ? P_PANEL : P_PANEL_HI));
  lv_obj_set_style_border_color(o, rgb(P_FRAME_DIM), 0);
  lv_obj_set_style_border_width(o, 1, 0);
  return o;
}

LvUi::Item *LvUi::newItem(Kind kind, lv_obj_t *parent, int16_t x, int16_t y, int16_t w,
                          int16_t h) {
  if (itemCount_ >= MAX_ITEMS) return &items_[MAX_ITEMS - 1];
  Item *it = &items_[itemCount_++];
  it->kind = (uint8_t)kind;
  it->obj = lv_obj_create(parent);
  lv_obj_remove_style_all(it->obj);
  lv_obj_set_pos(it->obj, x, y);
  lv_obj_set_size(it->obj, w, h);
  lv_obj_clear_flag(it->obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(it->obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(it->obj, drawItem, LV_EVENT_DRAW_MAIN, it);
  return it;
}

LvUi::Item *LvUi::mkText(lv_obj_t *parent, const char *initial, int16_t x, int16_t y,
                         uint8_t scale, lv_color_t color, bool rightAlign,
                         int16_t anchorX) {
  size_t len = strlen(initial ? initial : "");
  int16_t w = (int16_t)(len * 6 * scale);
  if (w <= 0) w = (int16_t)(6 * scale);
  Item *it = newItem(K_TEXT, parent, rightAlign ? (int16_t)(anchorX - w) : x, y, w,
                     (int16_t)(8 * scale));
  it->scale = scale;
  it->color = color;
  it->rightAlign = rightAlign;
  it->anchorX = anchorX;
  snprintf(it->text, sizeof(it->text), "%s", initial ? initial : "");
  return it;
}

LvUi::Item *LvUi::mkIcon(lv_obj_t *parent, const uint32_t *rows, int16_t x, int16_t y,
                         uint8_t scale, lv_color_t color) {
  Item *it = newItem(K_ICON, parent, x, y, (int16_t)(UI_ICON_SIZE * scale),
                     (int16_t)(UI_ICON_SIZE * scale));
  it->icon = rows;
  it->scale = scale;
  it->color = color;
  return it;
}

LvUi::BtnData &LvUi::mkButton(lv_obj_t *parent, int16_t x, int16_t y, int16_t w,
                              int16_t h, const uint32_t *icon, const char *label,
                              int8_t action, bool highlight, uint8_t sign, int8_t row,
                              int8_t page) {
  lv_obj_t *o = panel(parent, x, y, w, h, false);
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);

  if (btnCount_ >= MAX_BTNS) btnCount_ = 0;  // cannot happen with the fixed layout
  BtnData &b = btns_[btnCount_++];
  b.ui = this;
  b.obj = o;
  b.action = action;
  b.highlight = highlight;
  b.sign = sign;
  b.row = row;
  b.page = page;
  b.pressed = false;
  b.icon = nullptr;
  b.label = nullptr;

  if (icon) b.icon = mkIcon(o, icon, 28, 6, 1, rgb(P_AMBER));
  if (label) {
    int16_t tw = (int16_t)(strlen(label) * 6);
    b.label = mkText(o, label, (int16_t)((w - tw) / 2), 36, 1, rgb(P_DIM));
  }
  if (sign) lv_obj_add_event_cb(o, drawButton, LV_EVENT_DRAW_MAIN, &b);
  lv_obj_add_event_cb(o, buttonEvent, LV_EVENT_ALL, &b);
  return b;
}

void LvUi::mkRowButton(lv_obj_t *parent, uint8_t row, int16_t y) {
  BtnData &b = mkButton(parent, 24, y, 432, 40, nullptr, nullptr, ACT_LIST_ITEM, false,
                        0, (int8_t)row);
  listRows_[row] = b.obj;
  listNums_[row] = mkText(b.obj, "1", 12, 12, 2, rgb(P_AMBER));
  listNames_[row] = mkText(b.obj, "-", 52, 12, 2, rgb(P_CREAM));
}

void LvUi::paintButton(BtnData &b, bool pressed) {
  if (!b.highlight) return;
  b.pressed = pressed;
  lv_obj_set_style_bg_color(b.obj, rgb(pressed ? P_AMBER : P_PANEL_HI), 0);
  if (b.icon) {
    lv_color_t c =
        pressed ? rgb(P_BG) : rgb(b.icon == playIcon_ && running_ ? P_GREEN : P_AMBER);
    setColor(*b.icon, c);
  }
  if (b.label) setColor(*b.label, pressed ? rgb(P_BG) : rgb(P_DIM));
  lv_obj_invalidate(b.obj);
}

// -----------------------------------------------------------------------------------------
// Text / colour helpers
// -----------------------------------------------------------------------------------------

void LvUi::setText(Item &it, const char *value, int maxChars) {
  char buf[sizeof(it.text)];
  fit(buf, sizeof(buf), value, maxChars);
  if (strcmp(buf, it.text) == 0) return;
  lv_obj_invalidate(it.obj);
  snprintf(it.text, sizeof(it.text), "%s", buf);
  int16_t w = (int16_t)(strlen(it.text) * 6 * it.scale);
  lv_obj_set_size(it.obj, w, (int16_t)(8 * it.scale));
  if (it.rightAlign) lv_obj_set_x(it.obj, (int16_t)(it.anchorX - w));
  lv_obj_invalidate(it.obj);
}

void LvUi::setColor(Item &it, lv_color_t color) {
  if (it.color.full == color.full) return;
  it.color = color;
  lv_obj_invalidate(it.obj);
}

void LvUi::setRightAnchor(Item &it, int16_t anchor) {
  if (it.anchorX == anchor) return;
  it.anchorX = anchor;
  lv_obj_set_x(it.obj,
               (int16_t)(anchor - (int16_t)(strlen(it.text) * 6 * it.scale)));
  lv_obj_invalidate(it.obj);
}

void LvUi::touchMark(int16_t x, int16_t y, bool visible) {
  if (!markItem_ || !markObj_) return;
  markItem_->p1 = x;
  markItem_->p2 = y;
  markItem_->flag = visible;
  if (visible) {
    lv_obj_clear_flag(markObj_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(markObj_, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_invalidate(markObj_);
}

// -----------------------------------------------------------------------------------------
// Screens
// -----------------------------------------------------------------------------------------

lv_obj_t *LvUi::screen() {
  lv_obj_t *s = lv_obj_create(NULL);
  lv_obj_remove_style_all(s);
  lv_obj_set_style_bg_color(s, rgb(P_BG), 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  attachTouchMarker(s);
  return s;
}

void LvUi::attachTouchMarker(lv_obj_t *scr) {
  lv_obj_add_event_cb(scr, screenPressed, LV_EVENT_PRESSED, this);
  lv_obj_add_event_cb(scr, screenReleased, LV_EVENT_RELEASED, this);
  lv_obj_add_event_cb(scr, screenReleased, LV_EVENT_PRESS_LOST, this);
}

void LvUi::loadScreen(lv_obj_t *scr) {
  cur_ = scr;
  lv_scr_load(scr);
}

bool LvUi::begin() {
  fwName_ = APP_FW_NAME;

  scrMain_ = screen();
  buildMain(scrMain_);
  scrList_ = screen();
  buildList(scrList_);
  scrSetup_ = screen();
  buildSetup(scrSetup_);
  scrCal_ = screen();
  buildCal(scrCal_);

  // Touch marker on the top layer: above every screen, so no screen has to repaint
  // itself to erase it (that repaint is what used to blank the clock).
  markObj_ = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(markObj_);
  lv_obj_set_pos(markObj_, 0, 0);
  lv_obj_set_size(markObj_, 480, 480);
  lv_obj_clear_flag(markObj_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  markItem_ = &items_[itemCount_++];
  markItem_->kind = K_MARK;
  markItem_->obj = markObj_;
  markItem_->color = rgb(P_RED);
  lv_obj_add_event_cb(markObj_, drawItem, LV_EVENT_DRAW_MAIN, markItem_);
  lv_obj_add_flag(markObj_, LV_OBJ_FLAG_HIDDEN);

  loadScreen(scrMain_);
  return true;
}

void LvUi::buildMain(lv_obj_t *scr) {
  // Faceplate frame, inner ring and the four screws.
  strokeObj(scr, 6, 6, 468, 468, rgb(P_FRAME));
  strokeObj(scr, 9, 9, 462, 462, rgb(P_FRAME_DIM));
  mkFill(scr, 14, 14, 5, 5, rgb(P_FRAME_DIM));
  mkFill(scr, 452, 14, 5, 5, rgb(P_FRAME_DIM));
  mkFill(scr, 14, 452, 5, 5, rgb(P_FRAME_DIM));
  mkFill(scr, 452, 452, 5, 5, rgb(P_FRAME_DIM));

  // Header strip.
  panel(scr, 20, 20, 440, 62, false);
  mkIcon(scr, ICON_RADIO, 30, 28, 2, rgb(P_AMBER));
  mkText(scr, "INTERNET RADIO", 86, 28, 3, rgb(P_CREAM));
  mkText(scr, "RETRO STREAM DECK", 88, 58, 2, rgb(P_DIM));
  signalIcon_ = mkIcon(scr, ICON_SIGNAL, 408, 26, 2, rgb(P_DIM));

  // Station plate.
  panel(scr, 20, 90, 440, 74, true);
  mkIcon(scr, ICON_MUSIC, 30, 104, 2, rgb(P_AMBER));
  station_ = mkText(scr, "-", 86, 98, 3, rgb(P_CREAM));
  title_ = mkText(scr, "-", 88, 132, 2, rgb(P_AMBER));
  disc_ = newItem(K_DISC, scr, 415, 112, 30, 30);

  // Wall clock + format block.
  mkText(scr, "BANGKOK   UTC+7", 24, 172, 2, rgb(P_DIM));
  clock_ = newItem(K_CLOCK, scr, DIG_X, 192, 292, DIG_H);
  panel(scr, 320, 192, 140, 66, true);
  codec_ = mkText(scr, "---", 330, 198, 3, rgb(P_CREAM));
  kbps_ = mkText(scr, "- kbps", 330, 226, 2, rgb(P_AMBER));
  fmt_ = mkText(scr, "- kHz - ch", 330, 246, 1, rgb(P_DIM));

  // Buffer meter.
  mkText(scr, "BUFFER", 24, 282, 2, rgb(P_DIM));
  meter_ = newItem(K_METER, scr, 24, 304, 432, 18);
  meterValue_ = mkText(scr, "-", 0, 282, 2, rgb(P_AMBER), true, 460);

  // Status chiplets: link / heap / volume rocker.
  panel(scr, 24, 332, 136, 44, true);
  mkIcon(scr, ICON_SIGNAL, 32, 342, 1, rgb(P_AMBER));
  mkText(scr, "LINK", 64, 339, 1, rgb(P_DIM));
  linkValue_ = mkText(scr, "-", 64, 352, 2, rgb(P_CREAM));

  panel(scr, 172, 332, 136, 44, true);
  mkIcon(scr, ICON_BATTERY, 180, 342, 1, rgb(P_AMBER));
  mkText(scr, "HEAP", 212, 339, 1, rgb(P_DIM));
  heapValue_ = mkText(scr, "-", 212, 352, 2, rgb(P_CREAM));

  panel(scr, 320, 332, 136, 44, true);
  mkText(scr, "VOLUME", 328, 339, 1, rgb(P_DIM));
  volValue_ = mkText(scr, "12", 373, 350, 3, rgb(P_CREAM));
  mkButton(scr, 324, 350, 44, 22, nullptr, nullptr, ACT_VOL_DOWN, true, 1);
  mkButton(scr, 412, 350, 44, 22, nullptr, nullptr, ACT_VOL_UP, true, 2);

  // Control strip: prev / play-stop / next / list / setup.
  mkButton(scr, 24, 390, 80, 52, ICON_LEFT, "PREV", ACT_PREV);
  BtnData &bPlay = mkButton(scr, 110, 390, 80, 52, nullptr, nullptr, ACT_PLAY);
  playIcon_ = newItem(K_PLAYICON, scr, 143, 401, 16, 19);
  playIcon_->color = rgb(P_AMBER);
  bPlay.icon = playIcon_;
  playLabel_ = mkText(scr, "PLAY", 138, 426, 1, rgb(P_DIM));
  bPlay.label = playLabel_;
  mkButton(scr, 196, 390, 80, 52, ICON_RIGHT, "NEXT", ACT_NEXT);
  mkButton(scr, 282, 390, 80, 52, ICON_LIST, "LIST", ACT_LIST);
  mkButton(scr, 368, 390, 80, 52, ICON_GEAR, "SETUP", ACT_SETUP);

  // Bottom caption strip: firmware, uptime, and the device IP (for the portal).
  fwLabel_ = mkText(scr, fwName_, 24, 458, 1, rgb(P_DIM));
  uptime_ = mkText(scr, "UP 0:00", 220, 458, 1, rgb(P_DIM));
  ipLabel_ = mkText(scr, "ip", 400, 458, 1, rgb(P_DIM));
  ip_ = mkText(scr, "-", 0, 458, 1, rgb(P_AMBER), true, 464);
}

void LvUi::buildList(lv_obj_t *scr) {
  strokeObj(scr, 6, 6, 468, 468, rgb(P_FRAME));
  strokeObj(scr, 9, 9, 462, 462, rgb(P_FRAME_DIM));

  mkText(scr, "STATIONS", 24, 24, 3, rgb(P_CREAM));
  listRange_ = mkText(scr, "0-0 / 0", 280, 34, 1, rgb(P_DIM));

  for (uint8_t r = 0; r < LIST_ROWS; r++) {
    mkRowButton(scr, r, (int16_t)(72 + r * 44));
  }
  listEmpty_ = mkText(scr, "no stations saved", 24, 72, 2, rgb(P_DIM));

  panel(scr, 24, 396, 432, 48, true);

  // Paging arrows: the item draws the triangle, the same object is the hit area.
  listUp_ = newItem(K_ARROW, scr, 36, 408, 24, 22);
  listUp_->a = 0;
  lv_obj_add_flag(listUp_->obj, LV_OBJ_FLAG_CLICKABLE);
  BtnData &up = btns_[btnCount_++];
  up.ui = this;
  up.obj = listUp_->obj;
  up.page = -1;
  up.highlight = false;
  lv_obj_add_event_cb(listUp_->obj, buttonEvent, LV_EVENT_ALL, &up);

  listDown_ = newItem(K_ARROW, scr, 420, 408, 24, 22);
  listDown_->a = 1;
  lv_obj_add_flag(listDown_->obj, LV_OBJ_FLAG_CLICKABLE);
  BtnData &down = btns_[btnCount_++];
  down.ui = this;
  down.obj = listDown_->obj;
  down.page = 1;
  down.highlight = false;
  lv_obj_add_event_cb(listDown_->obj, buttonEvent, LV_EVENT_ALL, &down);

  // CLOSE: an invisible hit area over the middle of the bottom bar, with the label
  // pair drawn as children exactly where they were before.
  BtnData &close = mkButton(scr, 96, 396, 200, 48, nullptr, nullptr, ACT_LIST_CLOSE,
                            false);
  lv_obj_set_style_bg_opa(close.obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(close.obj, 0, 0);
  mkIcon(close.obj, ICON_CLOSE, 0, 12, 1, rgb(P_AMBER));
  mkText(close.obj, "CLOSE", 36, 18, 2, rgb(P_CREAM));

  refreshList();
}

void LvUi::buildSetup(lv_obj_t *scr) {
  strokeObj(scr, 6, 6, 468, 468, rgb(P_FRAME));
  strokeObj(scr, 9, 9, 462, 462, rgb(P_FRAME_DIM));

  mkIcon(scr, ICON_WIFI, 24, 24, 2, rgb(P_AMBER));
  mkText(scr, "SETUP MODE", 84, 26, 3, rgb(P_CREAM));

  mkText(scr, "1. join this Wi-Fi", 24, 96, 2, rgb(P_DIM));
  panel(scr, 24, 116, 432, 46, true);
  setupSsid_ = mkText(scr, "", 36, 130, 3, rgb(P_AMBER));

  mkText(scr, "password", 24, 176, 2, rgb(P_DIM));
  panel(scr, 24, 196, 432, 46, true);
  setupPass_ = mkText(scr, "", 36, 210, 3, rgb(P_CREAM));

  mkText(scr, "2. open in browser", 24, 256, 2, rgb(P_DIM));
  panel(scr, 24, 276, 432, 46, true);
  setupIp_ = mkText(scr, "http://192.168.4.1", 36, 290, 3, rgb(P_GREEN));

  mkText(scr, "add stations / set your Wi-Fi, then tap DONE", 24, 336, 1, rgb(P_DIM));

  // DONE: the whole bar is the button (inset panel, no press highlight).
  BtnData &done = mkButton(scr, 24, 384, 432, 60, nullptr, nullptr, ACT_SETUP_DONE,
                           false);
  lv_obj_set_style_bg_color(done.obj, rgb(P_PANEL), 0);
  mkIcon(done.obj, ICON_CLOSE, 16, 18, 1, rgb(P_AMBER));
  mkText(done.obj, "DONE (reboot)", 52, 22, 3, rgb(P_CREAM));
}

void LvUi::buildCal(lv_obj_t *scr) {
  strokeObj(scr, 6, 6, 468, 468, rgb(P_FRAME));
  strokeObj(scr, 9, 9, 462, 462, rgb(P_FRAME_DIM));

  calTitle_ = mkText(scr, "TOUCH CALIBRATION 1/9", 24, 24, 2, rgb(P_AMBER));
  mkText(scr, "tap the centre of the cross", 24, 46, 2, rgb(P_DIM));
  calTarget_ = newItem(K_TARGET, scr, 200, 200, 80, 80);
}

// -----------------------------------------------------------------------------------------
// Values -> screen
// -----------------------------------------------------------------------------------------

void LvUi::update(const UiInfo &i) {
  if (!station_) return;
  char buf[64];

  setText(*station_, (i.station && i.station[0]) ? i.station : "-", 20);
  setText(*title_, (i.title && i.title[0]) ? i.title : "-", 30);

  lv_color_t link = !i.wifiConnected
                        ? rgb(P_RED)
                        : (i.rssi > -67 ? rgb(P_GREEN)
                                        : (i.rssi > -78 ? rgb(P_AMBER) : rgb(P_RED)));
  setColor(*signalIcon_, link);

  snprintf(buf, sizeof(buf), "%.5s", i.codec ? i.codec : "?");
  setText(*codec_, buf, 5);
  snprintf(buf, sizeof(buf), "%lu kbps", (unsigned long)(i.bitrate / 1000));
  setText(*kbps_, buf, 8);
  snprintf(buf, sizeof(buf), "%lu.%01lu kHz %uch", (unsigned long)(i.sampleRate / 1000),
           (unsigned long)((i.sampleRate % 1000) / 100), i.channels);
  setText(*fmt_, buf, 13);

  if (clock_->flag != i.timeValid || clock_->a != i.hour || clock_->b != i.minute ||
      clock_->c != i.second) {
    clock_->flag = i.timeValid;
    clock_->a = i.hour;
    clock_->b = i.minute;
    clock_->c = i.second;
    lv_obj_invalidate(clock_->obj);
  }

  uint8_t lit = (uint8_t)((uint64_t)i.bufferBytes * METER_SEGS / 512000UL);
  if (lit > METER_SEGS) lit = METER_SEGS;
  if (lit != meter_->a) {
    meter_->a = lit;
    lv_obj_invalidate(meter_->obj);
  }
  snprintf(buf, sizeof(buf), "%lu KB  %u%%", (unsigned long)(i.bufferBytes / 1024),
           (unsigned)(lit * 100 / METER_SEGS));
  setText(*meterValue_, buf, 20);

  if (!i.wifiConnected) {
    setText(*linkValue_, "no link", 8);
    setColor(*linkValue_, rgb(P_RED));
  } else {
    snprintf(buf, sizeof(buf), "%d dBm", i.rssi);
    setText(*linkValue_, buf, 8);
    setColor(*linkValue_, link);
  }

  snprintf(buf, sizeof(buf), "%lu K", (unsigned long)(i.heapFree / 1024));
  setText(*heapValue_, buf, 8);
  setColor(*heapValue_, i.heapFree < 40000 ? rgb(P_RED) : rgb(P_CREAM));

  snprintf(buf, sizeof(buf), "%u", i.volume > 99 ? 99 : i.volume);
  setText(*volValue_, buf, 2);
  setColor(*volValue_, i.muted ? rgb(P_RED) : rgb(P_CREAM));

  if (i.streamRunning != running_) {
    running_ = i.streamRunning;
    if (playIcon_) {
      playIcon_->flag = running_;
      playIcon_->color = rgb(running_ ? P_GREEN : P_AMBER);
      lv_obj_invalidate(playIcon_->obj);
    }
    if (playLabel_) setText(*playLabel_, running_ ? "STOP" : "PLAY", 4);
  }

  uint8_t phase = i.streamRunning ? (uint8_t)((i.playedSec * 24) & 0xFF) : 0;
  if (disc_->a != phase || disc_->flag != i.streamRunning) {
    disc_->a = phase;
    disc_->flag = i.streamRunning;
    lv_obj_invalidate(disc_->obj);
  }

  snprintf(buf, sizeof(buf), "UP %lu:%02lu", (unsigned long)(i.uptimeSec / 60),
           (unsigned long)(i.uptimeSec % 60));
  setText(*uptime_, buf, 12);

  const char *ip = (i.ip && i.ip[0]) ? i.ip : "-";
  setText(*ip_, ip, 15);
  setRightAnchor(*ipLabel_, (int16_t)(464 - (int16_t)(strlen(ip_->text) * 6) - 18));
}

// -----------------------------------------------------------------------------------------
// Public screen API
// -----------------------------------------------------------------------------------------

int8_t LvUi::takeAction() {
  int8_t a = action_;
  action_ = ACT_NONE;
  return a;
}

void LvUi::showMain() { loadScreen(scrMain_); }

void LvUi::closeList() { loadScreen(scrMain_); }

void LvUi::openList(const char *const *names, uint8_t count) {
  listTotal_ = count > LIST_MAX ? LIST_MAX : count;
  for (uint8_t i = 0; i < listTotal_; i++) listSrc_[i] = names[i];
  listTop_ = 0;
  listSel_ = 0xFF;
  refreshList();
  loadScreen(scrList_);
}

void LvUi::refreshList() {
  for (uint8_t r = 0; r < LIST_ROWS; r++) {
    uint8_t idx = (uint8_t)(listTop_ + r);
    bool have = idx < listTotal_;
    if (have) {
      lv_obj_clear_flag(listRows_[r], LV_OBJ_FLAG_HIDDEN);
      char num[4];
      snprintf(num, sizeof(num), "%u", (unsigned)idx + 1);
      setText(*listNums_[r], num, 2);
      setText(*listNames_[r], listSrc_[idx], 26);
    } else {
      lv_obj_add_flag(listRows_[r], LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (listEmpty_) {
    if (listTotal_ == 0) {
      lv_obj_clear_flag(listEmpty_->obj, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(listEmpty_->obj, LV_OBJ_FLAG_HIDDEN);
    }
  }

  uint16_t last = (uint16_t)(listTop_ + LIST_ROWS);
  if (last > listTotal_) last = listTotal_;
  char range[16];
  snprintf(range, sizeof(range), "%u-%u / %u",
           (unsigned)(listTotal_ ? listTop_ + 1 : 0), (unsigned)last,
           (unsigned)listTotal_);
  setText(*listRange_, range, 12);

  if (listUp_) {
    if (listTop_ > 0) {
      lv_obj_clear_flag(listUp_->obj, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(listUp_->obj, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (listDown_) {
    if (listTop_ + LIST_ROWS < listTotal_) {
      lv_obj_clear_flag(listDown_->obj, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(listDown_->obj, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void LvUi::setupScreen(const char *ssid, const char *pass, const char *ip) {
  setText(*setupSsid_, ssid ? ssid : "", 24);
  setText(*setupPass_, pass ? pass : "", 24);
  char buf[32];
  snprintf(buf, sizeof(buf), "http://%s", (ip && ip[0]) ? ip : "192.168.4.1");
  setText(*setupIp_, buf, 24);
  loadScreen(scrSetup_);
}

void LvUi::calibrationTarget(uint8_t step, uint8_t total, int16_t tx, int16_t ty) {
  char buf[32];
  snprintf(buf, sizeof(buf), "TOUCH CALIBRATION %u/%u", (unsigned)step + 1,
           (unsigned)total);
  setText(*calTitle_, buf, 24);
  lv_obj_set_pos(calTarget_->obj, (int16_t)(tx - 40), (int16_t)(ty - 40));
  lv_obj_invalidate(calTarget_->obj);
  loadScreen(scrCal_);
}

// -----------------------------------------------------------------------------------------
// Events
// -----------------------------------------------------------------------------------------

void LvUi::buttonEvent(lv_event_t *e) {
  BtnData *b = (BtnData *)lv_event_get_user_data(e);
  if (!b || !b->ui) return;
  switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
      // The volume rocker acts on the press, then repeats while held.
      if (b->action == ACT_VOL_UP || b->action == ACT_VOL_DOWN) b->ui->setAction(b->action);
      b->ui->paintButton(*b, true);
      break;
    case LV_EVENT_LONG_PRESSED_REPEAT:
      if (b->action == ACT_VOL_UP || b->action == ACT_VOL_DOWN) b->ui->setAction(b->action);
      break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
      b->ui->paintButton(*b, false);
      break;
    case LV_EVENT_CLICKED:
      if (b->action == ACT_VOL_UP || b->action == ACT_VOL_DOWN) break;  // already fired
      if (b->row >= 0) {
        b->ui->listSel_ = (uint8_t)(b->ui->listTop_ + b->row);
        b->ui->setAction(ACT_LIST_ITEM);
      } else if (b->page) {
        if (b->page < 0 && b->ui->listTop_ >= LIST_ROWS) {
          b->ui->listTop_ = (uint8_t)(b->ui->listTop_ - LIST_ROWS);
        } else if (b->page > 0 &&
                   b->ui->listTop_ + LIST_ROWS < b->ui->listTotal_) {
          b->ui->listTop_ = (uint8_t)(b->ui->listTop_ + LIST_ROWS);
        }
        b->ui->refreshList();
      } else {
        b->ui->setAction(b->action);
      }
      break;
    default:
      break;
  }
}

void LvUi::screenPressed(lv_event_t *e) {
  LvUi *ui = (LvUi *)lv_event_get_user_data(e);
  uint16_t x = 0, y = 0;
  if (ui && lvport::lastPoint(x, y)) ui->touchMark((int16_t)x, (int16_t)y, true);
}

void LvUi::screenReleased(lv_event_t *e) {
  LvUi *ui = (LvUi *)lv_event_get_user_data(e);
  if (ui) ui->touchMark(0, 0, false);
}
