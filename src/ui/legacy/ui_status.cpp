#include "ui/ui_status.h"

#include <math.h>
#include <stdarg.h>
#include <string.h>

#include "ui_icons.h"

namespace {

// Faceplate palette.
//
// Warm "black wood + amber phosphor" rather than the blue-violet these RGB565 values tend
// to turn into on this glossy panel (a cold background reads as royal blue under the
// backlight, which fights the retro look). Keep every dim colour at or below ~20% of its
// lit counterpart: at 35% the unlit seven-segment and meter segments were as bright as the
// lit ones and the clock became unreadable.
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
constexpr uint16_t BG = rgb565(12, 9, 6);
constexpr uint16_t FRAME = rgb565(214, 120, 50);
constexpr uint16_t FRAME_DIM = rgb565(104, 56, 24);
constexpr uint16_t PANEL = rgb565(30, 20, 12);
constexpr uint16_t PANEL_HI = rgb565(44, 30, 16);
constexpr uint16_t CREAM = rgb565(255, 236, 196);
constexpr uint16_t AMBER = rgb565(255, 186, 74);
constexpr uint16_t AMBER_DIM = rgb565(46, 24, 8);
constexpr uint16_t DIM = rgb565(158, 132, 106);
constexpr uint16_t GREEN = rgb565(96, 226, 122);
constexpr uint16_t RED = rgb565(255, 90, 66);

// Seven-segment geometry: HH:MM:SS = three pairs of digits with centred colons.
constexpr int16_t DIG_W = 38;
constexpr int16_t DIG_H = 66;
constexpr int16_t DIG_T = 9;
constexpr int16_t DIG_Y = 192;
constexpr int16_t DIG_X = 22;       // first digit
constexpr int16_t DIG_PITCH = 42;   // digit -> digit inside a pair
constexpr int16_t PAIR_PITCH = 106; // HH -> MM -> SS
constexpr uint8_t METER_SEGS = 27;

}  // namespace

bool UiStatus::begin(Arduino_GFX *gfx) {
  gfx_ = gfx;
  return gfx_ != nullptr;
}

void UiStatus::drawStatic(const char *fwName) {
  if (!gfx_) return;
  fwName_ = fwName;
  gfx_->fillScreen(BG);

  // A full repaint wipes every value, but update() only redraws the fields whose cached
  // "last" value changed. Drop those caches here: without this, tapping the panel (which
  // repaints to erase the touch marker) left the clock ghosted, the station/title blank
  // and the volume number gone until each digit happened to tick - i.e. HH:MM reappeared
  // one digit at a time over the following minute.
  memset(lastDigits_, 0xFF, sizeof(lastDigits_));
  lastSegs_ = 0xFF;
  lastVol_ = 0xFF;
  lastStation_[0] = 0;
  lastTitle_[0] = 0;

  // Faceplate.
  gfx_->drawRect(6, 6, 468, 468, FRAME);
  gfx_->drawRect(9, 9, 462, 462, FRAME_DIM);
  for (uint8_t i = 0; i < 4; i++) {
    int16_t sx = (i & 1) ? 452 : 14;
    int16_t sy = (i & 2) ? 452 : 14;
    gfx_->fillRect(sx, sy, 5, 5, FRAME_DIM);
  }

  // Header strip: brand + tuner icon + link bars.
  face(20, 20, 440, 62);
  drawIcon(30, 28, ICON_RADIO, 2, AMBER);
  text(86, 28, 3, CREAM, "%s", "INTERNET RADIO");
  text(88, 58, 2, DIM, "%s", "RETRO STREAM DECK");
  drawIcon(408, 26, ICON_SIGNAL, 2, DIM);

  // Station plate.
  face(20, 90, 440, 74, true);
  drawIcon(30, 104, ICON_MUSIC, 2, AMBER);
  text(86, 98, 3, CREAM, "%s", "-");
  text(88, 132, 2, AMBER, "%s", "-");
  drawDisc(430, 127, 15, 0);

  // Wall clock (Bangkok) + format block.
  text(24, 172, 2, DIM, "%s", "BANGKOK   UTC+7");
  clockGhost();
  face(320, 192, 140, 66, true);
  text(330, 198, 3, CREAM, "%s", "---");
  text(330, 226, 2, AMBER, "%s", "- kbps");
  text(330, 246, 1, DIM, "%s", "- kHz - ch");

  // Buffer meter.
  text(24, 282, 2, DIM, "%s", "BUFFER");
  meter(24, 304, 432, 18, 0);

  // Small status chiplets: link / memory / volume rocker.
  chip(24, 332, 136, ICON_SIGNAL, "LINK", "-");
  chip(172, 332, 136, ICON_BATTERY, "HEAP", "-");
  face(320, 332, 136, 44, true);
  text(328, 339, 1, DIM, "%s", "VOLUME");
  volButtons(0);

  // Control strip: prev / play-stop / next / list / setup.
  controlRow();

  // Firmware name in the bottom strip; update() adds uptime + IP to its right.
  gfx_->fillRect(20, 456, 200, 10, BG);
  text(24, 458, 1, DIM, "%.30s", fwName ? fwName : "");
}

void UiStatus::update(const UiInfo &i) {
  if (!gfx_) return;
  if (listOpen_ || screen_ == SCREEN_SETUP) return;  // overlays own the screen
  if (repaint_) {  // wipe the touch marker left over from the previous press
    repaint_ = false;
    drawStatic(fwName_);
  }
  char buf[64];

  // Station / title only when the ICY metadata actually changed.
  if (strncmp(i.station ? i.station : "", lastStation_, sizeof(lastStation_)) != 0) {
    fit(lastStation_, sizeof(lastStation_), i.station, 20);
    clearText(86, 98, 3, 20);
    text(86, 98, 3, CREAM, "%s", lastStation_[0] ? lastStation_ : "-");
    lastTitle_[0] = 0;  // force the title line to redraw too
  }
  if (strncmp(i.title ? i.title : "", lastTitle_, sizeof(lastTitle_)) != 0) {
    fit(lastTitle_, sizeof(lastTitle_), i.title, 30);
    clearText(88, 132, 2, 31);
    text(88, 132, 2, AMBER, "%s", lastTitle_[0] ? lastTitle_ : "-");
  }

  // Link state: the radio icon goes green/amber/red with the real RSSI.
  uint16_t linkColor =
      !i.wifiConnected ? RED : (i.rssi > -67 ? GREEN : (i.rssi > -78 ? AMBER : RED));
  drawIcon(408, 26, ICON_SIGNAL, 2, linkColor);

  // Format block.
  clearText(330, 198, 3, 5);
  snprintf(buf, sizeof(buf), "%.5s", i.codec ? i.codec : "?");
  text(330, 198, 3, CREAM, "%s", buf);
  clearText(330, 226, 2, 8);
  snprintf(buf, sizeof(buf), "%lu kbps", (unsigned long)(i.bitrate / 1000));
  text(330, 226, 2, AMBER, "%s", buf);
  clearText(330, 246, 1, 13);
  snprintf(buf, sizeof(buf), "%lu.%01lu kHz %uch", (unsigned long)(i.sampleRate / 1000),
           (unsigned long)((i.sampleRate % 1000) / 100), i.channels);
  text(330, 246, 1, DIM, "%s", buf);

  // Wall clock. Before the first NTP sync only the ghost segments are visible.
  if (i.timeValid) {
    showClock(i.hour, i.minute, i.second);
  } else {
    clockGhost();
  }

  // Buffer meter + label value.
  uint8_t segs = meter(24, 304, 432, 18, i.bufferBytes);
  snprintf(buf, sizeof(buf), "%lu KB  %u%%", (unsigned long)(i.bufferBytes / 1024),
           (unsigned)(segs * 100 / METER_SEGS));
  gfx_->fillRect(320, 280, 140, 18, BG);
  text(460 - (int16_t)strlen(buf) * 12, 282, 2, AMBER, "%s", buf);

  // Chiplets: link / heap / volume.
  if (!i.wifiConnected) {
    chipValue(24, 332, "no link", RED);
  } else {
    snprintf(buf, sizeof(buf), "%d dBm", i.rssi);
    chipValue(24, 332, buf, linkColor);
  }
  snprintf(buf, sizeof(buf), "%lu K", (unsigned long)(i.heapFree / 1024));
  chipValue(172, 332, buf, i.heapFree < 40000 ? RED : CREAM);
  if (i.volume != lastVol_ || i.muted != lastMuted_) {
    lastVol_ = i.volume;
    lastMuted_ = i.muted;
    volValue(i.volume, i.muted);
  }

  // Play/stop button reflects the real decoder state; the station disc spins with it.
  spin_ = (uint8_t)((i.playedSec * 24) & 0xFF);
  if (i.streamRunning != running_) {
    running_ = i.streamRunning;
    playButton(110, zone_ == ACT_PLAY || zone_ == ACT_STOP);
  }
  drawDisc(430, 127, 15, i.streamRunning ? spin_ : 0);

  // Bottom caption strip (it sits on BG, not on a panel): uptime, then the IP right
  // aligned so a phone on the same network can be pointed at the setup portal.
  gfx_->fillRect(210, 456, 258, 10, BG);
  text(220, 458, 1, DIM, "UP %lu:%02lu", (unsigned long)(i.uptimeSec / 60),
       (unsigned long)(i.uptimeSec % 60));
  const char *ip = (i.ip && i.ip[0]) ? i.ip : "-";
  int16_t ipX = (int16_t)(464 - (int16_t)strlen(ip) * 6);
  text(ipX - 18, 458, 1, DIM, "%s", "ip");
  text(ipX, 458, 1, AMBER, "%s", ip);
}

void UiStatus::calibrationTarget(uint8_t step, uint8_t total, int16_t tx, int16_t ty) {
  if (!gfx_) return;
  gfx_->fillScreen(BG);
  gfx_->drawRect(6, 6, 468, 468, FRAME);
  gfx_->drawRect(9, 9, 462, 462, FRAME_DIM);
  text(24, 24, 2, AMBER, "TOUCH CALIBRATION %u/%u", (unsigned)step + 1, (unsigned)total);
  text(24, 46, 2, DIM, "%s", "tap the centre of the cross");
  gfx_->drawCircle(tx, ty, 28, AMBER);
  gfx_->drawCircle(tx, ty, 27, AMBER);
  gfx_->drawFastHLine(tx - 40, ty, 80, AMBER);
  gfx_->drawFastVLine(tx, ty - 40, 80, AMBER);
  gfx_->fillRect(tx - 3, ty - 3, 6, 6, CREAM);
}

void UiStatus::openList(const char *const *names, uint8_t count) {
  listCount_ = count > MAX_LIST_ROWS ? MAX_LIST_ROWS : count;
  for (uint8_t i = 0; i < listCount_; i++) {
    snprintf(listNames_[i], sizeof(listNames_[i]), "%s", names[i] ? names[i] : "-");
  }
  listSel_ = 0xFF;
  listTop_ = 0;
  listOpen_ = true;
  drawList();
}

void UiStatus::closeList() {
  listOpen_ = false;
  repaint_ = true;
}

void UiStatus::setupScreen(const char *ssid, const char *pass, const char *ip) {
  screen_ = SCREEN_SETUP;
  if (!gfx_) return;
  gfx_->fillScreen(BG);
  gfx_->drawRect(6, 6, 468, 468, FRAME);
  gfx_->drawRect(9, 9, 462, 462, FRAME_DIM);
  drawIcon(24, 24, ICON_WIFI, 2, AMBER);
  text(84, 26, 3, CREAM, "%s", "SETUP MODE");
  text(24, 96, 2, DIM, "%s", "1. join this Wi-Fi");
  face(24, 116, 432, 46, true);
  text(36, 130, 3, AMBER, "%s", ssid ? ssid : "");
  text(24, 176, 2, DIM, "%s", "password");
  face(24, 196, 432, 46, true);
  text(36, 210, 3, CREAM, "%s", pass ? pass : "");
  text(24, 256, 2, DIM, "%s", "2. open in browser");
  face(24, 276, 432, 46, true);
  text(36, 290, 3, GREEN, "http://%s", ip ? ip : "192.168.4.1");
  text(24, 336, 1, DIM, "%s", "add stations / set your Wi-Fi, then tap DONE");
  face(24, 384, 432, 60, true);
  drawIcon(40, 402, ICON_CLOSE, 1, AMBER);
  text(76, 406, 3, CREAM, "%s", "DONE (reboot)");
}

void UiStatus::mainScreen() {
  screen_ = SCREEN_MAIN;
  listOpen_ = false;
  drawStatic(fwName_);
}

int8_t UiStatus::handleTouch(bool pressed, uint16_t x, uint16_t y) {
  if (!gfx_) return ACT_NONE;

  if (screen_ == SCREEN_SETUP) {
    return (pressed && y >= 380) ? ACT_SETUP_DONE : ACT_NONE;
  }

  if (listOpen_) {
    if (!pressed) return ACT_NONE;
    if (y >= 396) {
      if (x < 120) {  // page up
        if (listTop_ >= MAX_LIST_ROWS) {
          listTop_ = (uint8_t)(listTop_ - MAX_LIST_ROWS);
          drawList();
        }
        return ACT_NONE;
      }
      if (x > 360) {  // page down
        if (listTop_ + MAX_LIST_ROWS < listCount_) {
          listTop_ = (uint8_t)(listTop_ + MAX_LIST_ROWS);
          drawList();
        }
        return ACT_NONE;
      }
      closeList();
      return ACT_LIST_CLOSE;
    }
    int16_t row = (int16_t)((y - 72) / 44);
    int16_t idx = (int16_t)(listTop_ + row);
    if (y >= 72 && y < 72 + MAX_LIST_ROWS * 44 && row >= 0 && idx < (int16_t)listCount_) {
      listSel_ = (uint8_t)idx;
      return ACT_LIST_ITEM;
    }
    return ACT_NONE;
  }

  int8_t hit = ACT_NONE;
  if (pressed) {
    drawTouchMark(x, y);  // shows where the panel thinks the finger is
    markDrawn_ = true;
    if (y >= 330 && y <= 382 && x >= 300) {
      hit = (x <= 392) ? ACT_VOL_DOWN : ACT_VOL_UP;
    } else if (y >= 386 && y <= 450) {
      if (x >= 20 && x <= 102) {
        hit = ACT_PREV;
      } else if (x >= 106 && x <= 192) {
        hit = running_ ? ACT_STOP : ACT_PLAY;
      } else if (x >= 196 && x <= 278) {
        hit = ACT_NEXT;
      } else if (x >= 282 && x <= 364) {
        hit = ACT_LIST;
      } else if (x >= 368 && x <= 458) {
        hit = ACT_SETUP;
      }
    }
  }

  if (!pressed) {
    if (zone_ != ACT_NONE) {
      zone_ = ACT_NONE;
      paintPressed(ACT_NONE);
    }
    if (markDrawn_) {  // clears the touch marker on the next update, once per press
      markDrawn_ = false;
      repaint_ = true;
    }
    return ACT_NONE;
  }
  if (hit != zone_) {
    zone_ = hit;
    paintPressed(hit);
    lastFire_ = millis();
    return hit;
  }
  if ((zone_ == ACT_VOL_DOWN || zone_ == ACT_VOL_UP) && millis() - lastFire_ >= 300) {
    lastFire_ = millis();
    return zone_;
  }
  return ACT_NONE;
}

// ---- primitives ------------------------------------------------------------------------

void UiStatus::text(int16_t x, int16_t y, uint8_t size, uint16_t color, const char *fmt, ...) {
  char buf[80];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  gfx_->setTextSize(size);
  gfx_->setTextColor(color);
  gfx_->setCursor(x, y);
  gfx_->print(buf);
}

// Clears a text cell before writing, so shorter values do not leave leftovers.
void UiStatus::clearText(int16_t x, int16_t y, uint8_t size, int chars) {
  gfx_->fillRect(x, y, chars * 6 * size, 8 * size, PANEL);
}

void UiStatus::fit(char *dst, size_t n, const char *src, int maxChars) {
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

void UiStatus::face(int16_t x, int16_t y, int16_t w, int16_t h, bool inset) {
  gfx_->fillRect(x, y, w, h, inset ? PANEL : PANEL_HI);
  gfx_->drawRect(x, y, w, h, FRAME_DIM);
}

// 1-bit pixel icon at an integer scale, drawn as horizontal runs (few fillRect calls).
void UiStatus::drawIcon(int16_t x, int16_t y, const uint32_t *rows, uint8_t scale,
                        uint16_t color) {
  for (uint8_t r = 0; r < UI_ICON_SIZE; r++) {
    uint32_t m = rows[r];
    if (!m) continue;
    uint8_t c = 0;
    while (c < UI_ICON_SIZE) {
      if (m & (1UL << c)) {
        uint8_t s = c;
        while (c < UI_ICON_SIZE && (m & (1UL << c))) c++;
        gfx_->fillRect(x + s * scale, y + r * scale, (c - s) * scale, scale, color);
      } else {
        c++;
      }
    }
  }
}

// ---- wall clock ------------------------------------------------------------------------

int16_t UiStatus::digitX(uint8_t idx) {  // idx 0..5 = H H : M M : S S
  return DIG_X + (idx / 2) * PAIR_PITCH + (idx % 2) * DIG_PITCH;
}

int16_t UiStatus::colonX(uint8_t pair) {  // colon between pair 0-1 and 1-2
  return DIG_X + pair * PAIR_PITCH + DIG_PITCH + DIG_W + 8;
}

// One seven-segment digit; dim segments stay visible like a real VFD/LED display.
void UiStatus::digit(int16_t x, int16_t y, uint8_t value) {
  static const uint8_t MAP[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                                  0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  uint8_t lit = MAP[value % 10];
  for (uint8_t s = 0; s < 7; s++) {
    segment(x, y, s, (lit >> s) & 1 ? AMBER : AMBER_DIM);
  }
}

void UiStatus::segment(int16_t x, int16_t y, uint8_t idx, uint16_t color) {
  const int16_t w = DIG_W, h = DIG_H, t = DIG_T;
  const int16_t half = h / 2;
  const int16_t upStart = t + 1;
  const int16_t upLen = half - t / 2 - 1 - upStart;
  const int16_t loStart = half + t / 2 + 1;
  const int16_t loLen = h - t - 1 - loStart;
  switch (idx) {
    case 0: gfx_->fillRect(x + t, y, w - 2 * t, t, color); break;                  // A
    case 1: gfx_->fillRect(x + w - t, y + upStart, t, upLen, color); break;        // B
    case 2: gfx_->fillRect(x + w - t, y + loStart, t, loLen, color); break;        // C
    case 3: gfx_->fillRect(x + t, y + h - t, w - 2 * t, t, color); break;          // D
    case 4: gfx_->fillRect(x, y + loStart, t, loLen, color); break;                // E
    case 5: gfx_->fillRect(x, y + upStart, t, upLen, color); break;                // F
    default: gfx_->fillRect(x + t, y + half - t / 2, w - 2 * t, t, color); break;  // G
  }
}

void UiStatus::colon(uint8_t pair, uint16_t color) {
  int16_t x = colonX(pair);
  gfx_->fillRect(x, DIG_Y + DIG_H / 3, DIG_T, DIG_T, color);
  gfx_->fillRect(x, DIG_Y + 2 * DIG_H / 3, DIG_T, DIG_T, color);
}

// Every segment unlit, like a display that is on but shows nothing (also the pre-NTP
// and pre-sync state).
void UiStatus::clockGhost() {
  for (uint8_t d = 0; d < 6; d++) {
    for (uint8_t s = 0; s < 7; s++) segment(digitX(d), DIG_Y, s, AMBER_DIM);
  }
  colon(0, AMBER_DIM);
  colon(1, AMBER_DIM);
}

void UiStatus::showClock(uint8_t hour, uint8_t minute, uint8_t second) {
  const uint8_t d[6] = {(uint8_t)((hour / 10) % 10), (uint8_t)(hour % 10),
                        (uint8_t)((minute / 10) % 10), (uint8_t)(minute % 10),
                        (uint8_t)((second / 10) % 10), (uint8_t)(second % 10)};
  for (uint8_t i = 0; i < 6; i++) {
    if (d[i] != lastDigits_[i]) {
      lastDigits_[i] = d[i];
      digit(digitX(i), DIG_Y, d[i]);
    }
  }
  colon(0, (second & 1) ? AMBER : AMBER_DIM);  // one-second blink, like a deck counter
  colon(1, (second & 1) ? AMBER : AMBER_DIM);
}

// ---- meters, chips, deck, volume -------------------------------------------------------

// Segmented bar. Returns how many segments are lit (buffer full at 512 KB).
uint8_t UiStatus::meter(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t bytes) {
  const int16_t segW = (w - (METER_SEGS - 1) * 2) / METER_SEGS;
  uint8_t lit = (uint8_t)((uint64_t)bytes * METER_SEGS / 512000UL);
  if (lit > METER_SEGS) lit = METER_SEGS;
  if (bytes == 0) lit = 0;
  if (lit == lastSegs_ && bytes != 0) return lit;
  for (uint8_t s = 0; s < METER_SEGS; s++) {
    uint16_t c = s < lit ? (s < METER_SEGS - 5 ? AMBER : GREEN) : AMBER_DIM;
    gfx_->fillRect(x + s * (segW + 2), y, segW, h, c);
  }
  lastSegs_ = lit;
  return lit;
}

void UiStatus::chip(int16_t x, int16_t y, int16_t w, const uint32_t *icon, const char *label,
                    const char *value) {
  face(x, y, w, 44, true);
  drawIcon(x + 8, y + 10, icon, 1, AMBER);
  text(x + 40, y + 7, 1, DIM, "%s", label);
  text(x + 40, y + 20, 2, CREAM, "%s", value);
}

void UiStatus::chipValue(int16_t x, int16_t y, const char *value, uint16_t color) {
  clearText(x + 40, y + 20, 2, 8);
  text(x + 40, y + 20, 2, color, "%s", value);
}

void UiStatus::volButton(int16_t x, int16_t y, bool plus, bool pressed) {
  gfx_->fillRect(x, y, 44, 22, pressed ? AMBER : PANEL_HI);
  gfx_->drawRect(x, y, 44, 22, pressed ? AMBER : FRAME_DIM);
  uint16_t c = pressed ? BG : AMBER;
  gfx_->fillRect(x + 14, y + 10, 16, 3, c);
  if (plus) gfx_->fillRect(x + 20, y + 4, 3, 15, c);
}

void UiStatus::volButtons(int8_t zone) {
  volButton(324, 350, false, zone < 0);
  volButton(412, 350, true, zone > 0);
}

void UiStatus::volValue(uint8_t volume, bool muted) {
  clearText(372, 350, 3, 2);
  text(373, 350, 3, muted ? RED : CREAM, "%u", volume > 99 ? 99 : volume);
}

// Five-button control strip: prev / play-stop / next / list / setup.
void UiStatus::controlRow() {
  actionButton(24, ICON_LEFT, "PREV", zone_ == ACT_PREV);
  playButton(110, zone_ == ACT_PLAY || zone_ == ACT_STOP);
  actionButton(196, ICON_RIGHT, "NEXT", zone_ == ACT_NEXT);
  actionButton(282, ICON_LIST, "LIST", zone_ == ACT_LIST);
  actionButton(368, ICON_GEAR, "SETUP", zone_ == ACT_SETUP);
}

void UiStatus::actionButton(int16_t x, const uint32_t *icon, const char *label, bool pressed) {
  const int16_t y = 390, w = 80, h = 52;
  gfx_->fillRect(x, y, w, h, pressed ? AMBER : PANEL_HI);
  gfx_->drawRect(x, y, w, h, pressed ? AMBER : FRAME_DIM);
  drawIcon(x + 28, y + 6, icon, 1, pressed ? BG : AMBER);
  text(x + (w - (int16_t)strlen(label) * 6) / 2, y + 36, 1, pressed ? BG : DIM, "%s", label);
}

void UiStatus::playButton(int16_t x, bool pressed) {
  const int16_t y = 390, w = 80, h = 52;
  gfx_->fillRect(x, y, w, h, pressed ? AMBER : PANEL_HI);
  gfx_->drawRect(x, y, w, h, pressed ? AMBER : FRAME_DIM);
  uint16_t c = pressed ? BG : (running_ ? GREEN : AMBER);
  int16_t cx = x + 40, cy = y + 20;
  if (running_) {
    gfx_->fillTriangle(cx - 6, cy - 9, cx - 6, cy + 9, cx + 8, cy, c);
  } else {
    gfx_->fillRect(cx - 7, cy - 8, 15, 16, c);
  }
  const char *label = running_ ? "STOP" : "PLAY";
  text(x + (w - (int16_t)strlen(label) * 6) / 2, y + 36, 1, pressed ? BG : DIM, "%s", label);
}

// Small spinning disc in the station plate: visual "is it playing" cue.
void UiStatus::drawDisc(int16_t cx, int16_t cy, int16_t r, uint8_t phase) {
  gfx_->fillCircle(cx, cy, r, PANEL_HI);
  gfx_->drawCircle(cx, cy, r, FRAME_DIM);
  gfx_->drawCircle(cx, cy, r / 3, FRAME_DIM);
  float a = (float)(phase % 96) * 6.2831853f / 96.0f;
  gfx_->drawLine(cx, cy, cx + (int16_t)(cosf(a) * (r - 2)),
                 cy + (int16_t)(sinf(a) * (r - 2)), AMBER);
  gfx_->fillCircle(cx, cy, 2, FRAME);
}

// Seven rows fit under the title, so a full list of RADIO_MAX_STATIONS pages with the
// arrows either side of CLOSE.
void UiStatus::drawList() {
  gfx_->fillScreen(BG);
  gfx_->drawRect(6, 6, 468, 468, FRAME);
  gfx_->drawRect(9, 9, 462, 462, FRAME_DIM);
  text(24, 24, 3, CREAM, "%s", "STATIONS");
  text(280, 34, 1, DIM, "%u-%u / %u", (unsigned)listTop_ + 1,
       (unsigned)((listTop_ + MAX_LIST_ROWS < listCount_) ? listTop_ + MAX_LIST_ROWS
                                                          : listCount_),
       (unsigned)listCount_);
  for (uint8_t r = 0; r < MAX_LIST_ROWS; r++) {
    uint8_t i = (uint8_t)(listTop_ + r);
    if (i >= listCount_) break;
    int16_t y = 72 + r * 44;
    face(24, y, 432, 40, true);
    char num[4];
    snprintf(num, sizeof(num), "%u", (unsigned)i + 1);
    text(36, y + 12, 2, AMBER, "%s", num);
    text(76, y + 12, 2, CREAM, "%.54s", listNames_[i]);
  }
  if (listCount_ == 0) {
    text(24, 72, 2, DIM, "%s", "no stations saved");
  }
  face(24, 396, 432, 48, true);
  // page up / down arrows, only drawn when that direction exists
  if (listTop_ > 0) {
    gfx_->fillTriangle(38, 428, 58, 428, 48, 410, AMBER);
  }
  if (listTop_ + MAX_LIST_ROWS < listCount_) {
    gfx_->fillTriangle(422, 410, 442, 410, 432, 428, AMBER);
  }
  drawIcon(96, 408, ICON_CLOSE, 1, AMBER);
  text(132, 414, 2, CREAM, "%s", "CLOSE");
}

// Debug aid: a red cross at the mapped touch position, so a finger and the reported
// point can be compared directly on the panel.
void UiStatus::drawTouchMark(uint16_t x, uint16_t y) {
  gfx_->drawCircle(x, y, 13, RED);
  gfx_->drawCircle(x, y, 12, RED);
  gfx_->fillRect(x - 18, y - 2, 8, 4, RED);
  gfx_->fillRect(x + 11, y - 2, 8, 4, RED);
  gfx_->fillRect(x - 2, y - 18, 4, 8, RED);
  gfx_->fillRect(x - 2, y + 11, 4, 8, RED);
}

// Visual feedback for whichever button is currently pressed.
void UiStatus::paintPressed(int8_t action) {
  volButtons(action == ACT_VOL_DOWN ? -1 : (action == ACT_VOL_UP ? 1 : 0));
  controlRow();
}
