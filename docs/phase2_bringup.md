# Phase 2 — Minimal Compile / Bring-up (ผลทดสอบจริง)

บอร์ดทดสอบ: Guition ESP32-S3-4848S040 บนพอร์ต `COM29`
วันที่ทดสอบ: 2026-10-21

ไฟล์ที่ใช้: `platformio.ini`, `include/board_pins.h`, `src/main.cpp`

> ทุกตัวเลขในเอกสารนี้คัดลอกจาก log ที่อ่านจาก COM29 จริง ไม่ใช่ค่าคาดเดา

## 1. Dependency ที่คอมไพล์และแฟลชผ่านจริง

| รายการ | เวอร์ชัน | หมายเหตุ |
|--------|---------|----------|
| PlatformIO Core | 6.1.x (ติดตั้งผ่าน `pip install --user platformio`) | ใช้ `python -m platformio` |
| platform | `espressif32@55.3.312` | ให้ Arduino-ESP32 core 3.3.12 |
| board | `esp32-s3-devkitc-1` (override memory/flash) | PlatformIO ไม่มี board สำหรับ 4848S040 |
| framework | Arduino | |
| library | `moononournation/GFX Library for Arduino@1.6.9` | ใช้ `st7701_type9_init_operations` |
| flash / PSRAM | `qio` 16 MB / `qio_opi` 8 MB | `default_16MB.csv` |

คำสั่งที่ใช้: `python -m platformio run` และ `python -m platformio run -t upload`

## 2. ผลจาก Serial (COM29, 115200)

```
chip      : ESP32-S3 rev 2, 2 core(s) @ 240 MHz
flash     : 16 MB
psram     : found=1 size=8388608 bytes
I2C scan  : 0x5D
GT911 candidate 0x5D product-id: 0x39 0x31 0x31 0x00 (911.)
GT911     : ready
display   : init... OK
heap: free=337672 total=381792 | psram: free=7922312 total=8388608
```

- **I²C bus มีอุปกรณ์เดียวคือ 0x5D** → บอร์ดนี้เป็น GT911 (ไม่ใช่ FT6336U)
- จอ ST7701 init ผ่านด้วย `st7701_type9_init_operations` + RGB pin ตาม `board_pins.h`
- ผ่านไป 5,256 วินาที (87 นาที) ค่า heap/PSRAM คงที่ทุกค่าวินาทีที่บันทึก → ไม่มี memory leak ในเส้นทาง display/touch

## 3. สิ่งที่ผู้ใช้ยืนยันด้วยตา

- ภาพบนจอแสดงถูกต้อง (สี, ทิศทาง, ไม่กลับด้าน, ไม่หมุน)
- ทัชตอบสนองเมื่อแตะ

## 4. บั๊กที่พบและแก้แล้ว: ลำดับไบต์ของพิกัด GT911

อ่านจุดสัมผัสจาก `0x8150` แบบ little-endian (ตาม datasheet GT911) ได้ค่าที่เป็นไปไม่ได้:

```
touch: down x=42496 y=5633 raw=(42496,5633) points=1
```

ค่าเดียวกันเมื่อสลับไบต์เป็น high-byte-first คือ `(166, 278)` ซึ่งอยู่ในช่วง 0–479
จึงแก้ `readTouch()` ใน `src/main.cpp` เป็น

```cpp
t.rawX = ((uint16_t)p[1] << 8) | (uint16_t)p[2];
t.rawY = ((uint16_t)p[3] << 8) | (uint16_t)p[4];
```

พร้อมพิมพ์ hex ของ 7 ไบต์ (สูงสุด 24 ครั้ง) เพื่อยืนยันซ้ำ:

```
point raw: 00 00 A6 01 16 .. ..  -> x=166 y=278 (LE would be 42496,5633)
```

**สถานะ:** แก้โค้ดและแฟลชแล้ว — **ยังไม่ได้ยืนยันด้วยการแตะ 4 มุมจอ**
จึงยังไม่ถือว่าพิกัด/การหมุนผ่าน (ต้องรอผลแตะมุมจริง)

## 5. ยังไม่ได้ทดสอบ (ห้ามถือว่าผ่าน)

- เสียง / I²S / NS4168 — ติดเงื่อนไขฮาร์ดแวร์ (ดู `docs/phase1_hardware_audit.md` §4)
- LVGL — ยังไม่ติดตั้งหรือคอมไพล์
- Wi-Fi, HTTP stream, Web Configuration
- microSD, รีเลย์
- Stability test 60 นาที (ทดสอบได้เฉพาะ display/touch เท่านั้นถ้าต้องการ)

## 6. ข้อจำกัดที่ทราบ

- `esp32-s3-devkitc-1` เป็น board definition ชั่วคราว; ค่า flash/PSRAM ถูก override
  ให้ตรง N16R8 แล้ว แต่ถ้าเปลี่ยนบอร์ดต้องแก้ `platformio.ini`
- PSRAM 8 MB ยังไม่ได้ใช้เลยใน Phase 2 (Arduino_GFX โหมด RGB ใช้ internal heap เป็นหลัก)
  — ยังไม่มีตัวเลข framebuffer/PSRAM ของ LVGL
