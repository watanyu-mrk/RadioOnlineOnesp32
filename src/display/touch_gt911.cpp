#include "display/touch_gt911.h"

bool TouchGt911::begin(uint8_t sda, uint8_t scl, uint32_t hz) {
  Wire.begin(sda, scl, hz);
  delay(50);
  const uint8_t candidates[] = {GT911_ADDR_PRIMARY, GT911_ADDR_ALT};
  for (uint8_t addr : candidates) {
    uint8_t id[4] = {0};
    if (!read(addr, REG_PRODUCT_ID, id, sizeof(id))) continue;
    if (id[0] == '9' && id[1] == '1' && id[2] == '1') {
      addr_ = addr;
      ready_ = true;
      return true;
    }
  }
  return false;
}

void TouchGt911::dump() {
  if (!ready_) {
    Serial.println("gt911     : driver not ready (no device answered)");
    return;
  }
  uint8_t id[4] = {0}, cfg = 0, st = 0;
  bool a = read(addr_, REG_PRODUCT_ID, id, 4);
  bool b = read(addr_, REG_CONFIG_VERSION, &cfg, 1);
  bool c = read(addr_, REG_STATUS, &st, 1);
  Serial.printf("gt911     : addr=0x%02X id=%s cfg=0x%02X status=0x%02X "
                "(id:%d cfg:%d st:%d) ok=%lu fail=%lu\n",
                addr_, a ? (const char *)id : "????", b ? cfg : 0, c ? st : 0, a, b, c,
                (unsigned long)okReads_, (unsigned long)failReads_);
}

void TouchGt911::dumpConfig() {
  if (!ready_) {
    Serial.println("gt911 cfg : driver not ready");
    return;
  }
  uint8_t b[8] = {0};
  if (!read(addr_, REG_CONFIG_VERSION, b, sizeof(b))) {
    Serial.println("gt911 cfg : read failed");
    return;
  }
  uint16_t xmax = (uint16_t)b[1] | ((uint16_t)b[2] << 8);
  uint16_t ymax = (uint16_t)b[3] | ((uint16_t)b[4] << 8);
  Serial.printf("gt911 cfg : ver=0x%02X xmax=%u ymax=%u touchNum=%u sw1=0x%02X sw2=0x%02X\n",
                b[0], xmax, ymax, b[5], b[6], b[7]);
}

TouchGt911::Point TouchGt911::read(bool *hasSample) {
  Point p = {false, 0, 0, 0};
  if (hasSample) *hasSample = false;
  if (!ready_) return p;

  uint8_t status = 0;
  if (!read(addr_, REG_STATUS, &status, 1)) {
    failReads_++;
    return p;
  }
  okReads_++;
  lastStatus_ = status;
  if ((status & 0x80) == 0) return p;  // no new sample since the last clear

  if (hasSample) *hasSample = true;
  uint8_t n = status & 0x0F;
  if (n > 0) {
    uint8_t raw[7] = {0};
    if (read(addr_, REG_POINT1, raw, sizeof(raw))) {
      for (uint8_t i = 0; i < sizeof(raw); i++) lastBytes_[i] = raw[i];
      haveBytes_ = true;
      // Little-endian from the first byte: there is no track-id byte in this record.
      p.x = (uint16_t)raw[0] | ((uint16_t)raw[1] << 8);
      p.y = (uint16_t)raw[2] | ((uint16_t)raw[3] << 8);
      p.points = n;
      p.pressed = true;
    }
  }
  write8(addr_, REG_STATUS, 0);
  return p;
}

bool TouchGt911::read(uint8_t addr, uint16_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

bool TouchGt911::write8(uint8_t addr, uint16_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  Wire.write(value);
  return Wire.endTransmission() == 0;
}
