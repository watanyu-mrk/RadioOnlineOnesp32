// Phase 2 hardware test for Guition ESP32-S3-4848S040
// - ST7701 480x480 RGB panel via Arduino_GFX
// - GT911 touch over I2C (minimal polled driver, no external library)
// - Prints flash / PSRAM / heap / I2C diagnostics on UART0 (115200)
//
// This is a hardware bring-up sketch. It does NOT implement radio audio.

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include "board_pins.h"

#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif
#include <esp_system.h>

// ---------------------------------------------------------------- display ---

Arduino_DataBus *bus = new Arduino_SWSPI(
    GFX_NOT_DEFINED /* DC */, LCD_CS,
    LCD_SCK, LCD_MOSI, GFX_NOT_DEFINED /* MISO */);

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    LCD_DE, LCD_VSYNC, LCD_HSYNC, LCD_PCLK,
    LCD_R0, LCD_R1, LCD_R2, LCD_R3, LCD_R4,
    LCD_G0, LCD_G1, LCD_G2, LCD_G3, LCD_G4, LCD_G5,
    LCD_B0, LCD_B1, LCD_B2, LCD_B3, LCD_B4,
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */,
    0 /* pclk_active_neg */, 12000000 /* prefer_speed */, false /* useBigEndian */,
    0 /* de_idle_high */, 0 /* pclk_idle_high */, 0 /* bounce_buffer_size_px */);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480, 480, rgbpanel, 0 /* rotation */, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */,
    st7701_type9_init_operations, sizeof(st7701_type9_init_operations));

// ------------------------------------------------------------------ touch ---

#define GT911_REG_PRODUCT_ID 0x8140
#define GT911_REG_STATUS 0x814E
#define GT911_REG_POINT1 0x8150

static uint8_t gTouchAddr = 0;
static bool gTouchReady = false;

struct TouchState {
  bool pressed;
  uint16_t x;
  uint16_t y;
  uint16_t rawX;
  uint16_t rawY;
  uint8_t points;
};

// ------------------------------------------------------------------ colors --

static const uint16_t COL_BG = 0x0861;      // dark navy
static const uint16_t COL_PANEL = 0x10C2;   // slightly lighter
static const uint16_t COL_TEXT = 0xFFFF;
static const uint16_t COL_DIM = 0x8C71;
static const uint16_t COL_ACCENT = 0x07FF;
static const uint16_t COL_OK = 0x07E0;
static const uint16_t COL_WARN = 0xFD20;

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// ------------------------------------------------------------------- i2c ----

static bool i2cRead(uint8_t addr, uint16_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(addr, len) != len) {
    return false;
  }
  for (uint8_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
  return true;
}

static bool i2cWrite8(uint8_t addr, uint16_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static void scanI2c() {
  Serial.print("I2C scan:");
  uint8_t found = 0;
  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    if (i2cPresent(addr)) {
      Serial.printf(" 0x%02X", addr);
      found++;
    }
  }
  if (found == 0) {
    Serial.print(" (nothing found)");
  }
  Serial.println();
}

static bool gt911Init() {
  const uint8_t candidates[] = {GT911_ADDR_PRIMARY, GT911_ADDR_ALT};
  for (uint8_t addr : candidates) {
    if (!i2cPresent(addr)) {
      continue;
    }
    uint8_t id[4] = {0};
    if (!i2cRead(addr, GT911_REG_PRODUCT_ID, id, sizeof(id))) {
      continue;
    }
    Serial.printf("GT911 candidate 0x%02X product-id: 0x%02X 0x%02X 0x%02X 0x%02X (%c%c%c%c)\n",
                  addr, id[0], id[1], id[2], id[3],
                  isprint(id[0]) ? id[0] : '.', isprint(id[1]) ? id[1] : '.',
                  isprint(id[2]) ? id[2] : '.', isprint(id[3]) ? id[3] : '.');
    if (id[0] == '9' && id[1] == '1' && id[2] == '1') {
      gTouchAddr = addr;
      return true;
    }
  }
  return false;
}

static TouchState readTouch() {
  TouchState t = {false, 0, 0, 0, 0, 0};
  if (!gTouchReady) {
    return t;
  }

  uint8_t status = 0;
  if (!i2cRead(gTouchAddr, GT911_REG_STATUS, &status, 1)) {
    return t;
  }

  uint8_t points = status & 0x0F;
  if ((status & 0x80) == 0) {
    return t;
  }

  t.points = points;
  if (points > 0) {
    uint8_t p[7] = {0};
    if (i2cRead(gTouchAddr, GT911_REG_POINT1, p, sizeof(p))) {
      t.pressed = true;
      // NOTE: this panel reports each axis high-byte-first. Reading it as
      // little-endian (the usual GT911 layout) produces out-of-range values
      // like x=42496; swapping the pair yields valid 0..479 coordinates.
      t.rawX = ((uint16_t)p[1] << 8) | (uint16_t)p[2];
      t.rawY = ((uint16_t)p[3] << 8) | (uint16_t)p[4];
      t.x = t.rawX;
      t.y = t.rawY;

      static uint8_t dumps = 0;
      if (dumps < 24) {
        dumps++;
        Serial.printf("point raw: %02X %02X %02X %02X %02X %02X %02X  -> x=%u y=%u (LE would be %u,%u)\n",
                      p[0], p[1], p[2], p[3], p[4], p[5], p[6],
                      t.rawX, t.rawY,
                      (unsigned)((uint16_t)p[1] | ((uint16_t)p[2] << 8)),
                      (unsigned)((uint16_t)p[3] | ((uint16_t)p[4] << 8)));
      }
    }
  }

  // Clear the buffer-ready flag so the controller can report the next sample.
  i2cWrite8(gTouchAddr, GT911_REG_STATUS, 0);
  return t;
}

// ------------------------------------------------------------------- ui ------

static void setBacklight(uint8_t level) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(LCD_BL, level);
#else
  ledcWrite(0, level);
#endif
}

static void backlightInit() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(LCD_BL, 5000, 8);
#else
  ledcSetup(0, 5000, 8);
  ledcAttachPin(LCD_BL, 0);
#endif
  setBacklight(0);
}

static void drawText(int16_t x, int16_t y, uint8_t size, uint16_t color, const char *text) {
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(x, y);
  gfx->print(text);
}

static void drawCornerMarks() {
  const int16_t L = 26;
  const uint16_t c = COL_ACCENT;
  gfx->fillRect(0, 0, L, 3, c);
  gfx->fillRect(0, 0, 3, L, c);
  gfx->fillRect(480 - L, 0, L, 3, c);
  gfx->fillRect(477, 0, 3, L, c);
  gfx->fillRect(0, 477, L, 3, c);
  gfx->fillRect(0, 480 - L, 3, L, c);
  gfx->fillRect(480 - L, 477, L, 3, c);
  gfx->fillRect(477, 480 - L, 3, L, c);
}

static void drawStaticUi() {
  gfx->fillScreen(COL_BG);

  gfx->fillRect(0, 0, 480, 78, COL_PANEL);
  drawText(20, 14, 3, COL_TEXT, "ESP32-S3 4848S040");
  drawText(20, 48, 2, COL_DIM, "Phase 2 - display / touch / memory");

  drawText(20, 94, 2, COL_ACCENT, "DISPLAY");
  drawText(150, 94, 2, COL_TEXT, "ST7701 480x480 RGB565");

  drawText(20, 120, 2, COL_ACCENT, "TOUCH");
  if (gTouchReady) {
    char buf[48];
    snprintf(buf, sizeof(buf), "GT911 @ 0x%02X (I2C poll)", gTouchAddr);
    drawText(150, 120, 2, COL_OK, buf);
  } else {
    drawText(150, 120, 2, COL_WARN, "NOT FOUND");
  }

  drawText(20, 146, 2, COL_ACCENT, "PSRAM");
  drawText(150, 146, 2, COL_TEXT, psramFound() ? "OPI PSRAM detected" : "NOT detected");

  drawText(20, 172, 2, COL_ACCENT, "HEAP");
  char heap[64];
  snprintf(heap, sizeof(heap), "free %u / %u bytes",
           (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getHeapSize());
  drawText(150, 172, 2, COL_TEXT, heap);

  drawText(20, 198, 2, COL_ACCENT, "FLASH");
  char flash[48];
  snprintf(flash, sizeof(flash), "%u MB", (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)));
  drawText(150, 198, 2, COL_TEXT, flash);

  drawText(20, 224, 2, COL_ACCENT, "I2C");
  char i2cbuf[64] = "bus";
  drawText(150, 224, 2, COL_TEXT, i2cbuf);

  // color swatches
  static const uint16_t swatches[] = {rgb565(255, 0, 0), rgb565(0, 255, 0), rgb565(0, 0, 255),
                                      rgb565(255, 255, 0), rgb565(0, 255, 255), rgb565(255, 0, 255),
                                      rgb565(255, 255, 255)};
  for (int i = 0; i < 7; i++) {
    gfx->fillRect(20 + i * 64, 252, 56, 34, swatches[i]);
  }

  // touch panel
  gfx->fillRect(18, 300, 444, 164, COL_PANEL);
  gfx->drawRect(18, 300, 444, 164, COL_ACCENT);
  drawText(34, 310, 2, COL_DIM, "TOUCH TEST - tap inside this box");
  drawText(34, 370, 2, COL_DIM, "raw");
  drawText(34, 398, 2, COL_DIM, "points");
  drawText(34, 426, 2, COL_DIM, "last event");

  drawCornerMarks();
}

static void drawTouchState(const TouchState &t, const TouchState &prev, bool force) {
  if (!force && t.pressed == prev.pressed && t.x == prev.x && t.y == prev.y) {
    return;
  }

  // Clear only the dynamic part of the touch box.
  gfx->fillRect(28, 340, 424, 24, COL_PANEL);
  gfx->fillRect(100, 364, 352, 88, COL_PANEL);

  char line[64];
  if (t.pressed) {
    snprintf(line, sizeof(line), "X %4u   Y %4u", t.x, t.y);
    drawText(34, 340, 3, COL_OK, line);

    snprintf(line, sizeof(line), "%u, %u", t.rawX, t.rawY);
    drawText(100, 368, 2, COL_TEXT, line);

    snprintf(line, sizeof(line), "%u", t.points);
    drawText(100, 396, 2, COL_TEXT, line);

    drawText(100, 424, 2, COL_OK, "pressed");
  } else {
    snprintf(line, sizeof(line), "X ----   Y ----");
    drawText(34, 340, 3, COL_DIM, line);
    drawText(100, 424, 2, COL_DIM, "released");
  }
}

static void printMemory() {
  Serial.printf("heap: free=%u total=%u | psram: free=%u total=%u | uptime=%lus\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getHeapSize(),
                (unsigned)ESP.getFreePsram(), (unsigned)ESP.getPsramSize(),
                (unsigned long)(millis() / 1000));
}

// ------------------------------------------------------------------ setup ---

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("==================================================");
  Serial.println(" Guition ESP32-S3-4848S040 - Phase 2 hardware test");
  Serial.println("==================================================");
  Serial.printf("chip      : %s rev %d, %d core(s) @ %u MHz\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(),
                (unsigned)getCpuFrequencyMhz());
  Serial.printf("flash     : %u MB\n", (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)));
  Serial.printf("psram     : found=%d size=%u bytes\n",
                psramFound(), (unsigned)ESP.getPsramSize());
  Serial.printf("reset     : %d\n", (int)esp_reset_reason());

  backlightInit();

  Wire.begin(TOUCH_SDA, TOUCH_SCL, 100000);
  delay(50);
  scanI2c();
  gTouchReady = gt911Init();
  Serial.printf("GT911     : %s\n", gTouchReady ? "ready" : "NOT FOUND");

  Serial.print("display   : init...");
  if (!gfx->begin()) {
    Serial.println(" FAILED");
    drawText(20, 20, 3, COL_WARN, "DISPLAY INIT FAILED");
    setBacklight(255);
    while (true) {
      delay(1000);
    }
  }
  Serial.println(" OK");

  gfx->fillScreen(COL_BG);
  drawStaticUi();

  Serial.printf("framebuffer: %ux%u, free heap=%u, free psram=%u\n",
                480, 480, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());

  setBacklight(255);
  Serial.println("backlight : on (GPIO38)");
  printMemory();

  // Initial touch state
  TouchState t = readTouch();
  TouchState none = {false, 0, 0, 0, 0, 0};
  drawTouchState(t, none, true);
  Serial.println("ready: display + touch test running");
}

// ------------------------------------------------------------------- loop ---

void loop() {
  static uint32_t lastPoll = 0;
  static uint32_t lastStats = 0;
  static TouchState prev = {false, 0, 0, 0, 0, 0};

  uint32_t now = millis();
  if (now - lastPoll >= 15) {
    lastPoll = now;
    TouchState t = readTouch();
    drawTouchState(t, prev, false);
    if (t.pressed != prev.pressed || t.x != prev.x || t.y != prev.y) {
      Serial.printf("touch: %s x=%u y=%u raw=(%u,%u) points=%u\n",
                    t.pressed ? "down" : "up", t.x, t.y, t.rawX, t.rawY, t.points);
    }
    prev = t;
  }

  if (now - lastStats >= 5000) {
    lastStats = now;
    printMemory();
  }

  delay(2);
}
