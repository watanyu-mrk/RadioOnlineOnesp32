# วิทยุออนไลน์ ESP32-S3 + จอทัช 4.0" (Guition ESP32-S3-4848S040)

เฟิร์มแวร์วิทยุอินเทอร์เน็ตบนบอร์ด **Guition ESP32-S3-4848S040** — จอ RGB 480×480
พร้อมทัช GT911, หน้าจอสไตล์เครื่องรับวิทยุ retro (วาดด้วย **LVGL 8.3.11**) และลำโพง
ในตัว สตรีม MP3/AAC จาก URL ผ่าน Wi-Fi, เก็บรายการสถานีใน NVS, ตั้งค่า Wi-Fi ผ่าน
พอร์ทัลบนมือถือได้โดยไม่ต้องต่อคอม

ทุกค่าที่ขึ้นบนจอมาจากสถานะจริงของฮาร์ดแวร์ (`Audio` ของ ESP32-audioI2S, `WiFi`,
NTP) — ไม่มีตัวเลขหลอก

---

## 1. ภาพรวม — ทำอะไรได้

| ความสามารถ | รายละเอียด |
|---|---|
| เล่นวิทยุออนไลน์ | MP3 / AAC จาก URL (HTTP/HTTPS), ตรวจแล้ว 96 kbps AAC 32 kHz และ 128 kbps MP3 44.1 kHz |
| รายการสถานี | สูงสุด 12 สถานี เก็บใน NVS, เลือก/สลับ/เล่น/หยุดได้จากจอ |
| หน้าจอ retro | กรอบส้ม ตัวอักษรครีม, นาฬิกา 7-segment เวลากรุงเทพ (NTP), มิเตอร์ buffer, reel ตลับเทปหมุนตามเวลาเล่นจริง |
| สถานะระบบ | codec/bitrate/sample rate, ระดับ buffer, RSSI (สีไอคอนเสาเปลี่ยนตามคุณภาพ), heap, IP, uptime |
| ทัชทั้งจอ | GT911 → LVGL pointer indev, ปุ่มทุกปุ่มเป็นทัชจริง (ไม่มีพิกัด hardcode) |
| พอร์ทัลตั้งค่า | AP `RetroRadio-Setup` → `http://192.168.4.1` ตั้ง Wi-Fi, เพิ่ม/ลบสถานี, รีบูต |
| คีย์ลัดทาง serial | เปลี่ยนสถานี, mute, volume, คาลิเบรตทัช, dump ค่า GT911 |

หน้าจอมี 4 หน้า: **faceplate หลัก**, **LIST** (7 แถว/หน้า + เลื่อนหน้า),
**SETUP** (SSID/รหัส/IP + DONE) และ **TOUCH CAL** (กริด 3×3, ใช้เมื่อจำเป็นเท่านั้น)

---

## 2. ฮาร์ดแวร์

### 2.1 ที่ต้องมี

- บอร์ด **Guition ESP32-S3-4848S040** — ESP32-S3-WROOM-1 **N16R8** (แฟลช 16 MB QIO + PSRAM 8 MB OPI)
- จอ **ST7701 480×480 RGB** พร้อมทัช **GT911** (มาพร้อมบอร์ด)
- ลำโพงต่อกับแอมป์ **NS4168** บนบอร์ด
- สาย USB-C (อัปโหลดผ่านชิป **CH340 → UART0**; บอร์ดนี้**ไม่มี native USB**)
- แหล่งจ่าย 5 V ที่จ่ายได้พอ (สตรีม + Wi-Fi + จอกินกระแสต่อเนื่อง)

> **ดัดแปลงฮาร์ดแวร์ที่จำเป็น:** เสียงจะออกลำโพงก็ต่อเมื่อย้ายตัวต้านทาน 0Ω
> (**R25/R26/R27 → R21/R22/R23**) ตามคู่มือผู้ขาย หลังบัดกรีแล้วสตรีมมีเสียงออกลำโพงจริง
> (ยืนยันโดยผู้ใช้) ถ้ายังไม่ได้ย้าย ให้ใช้ `src/audio_test` ตรวจเส้นทางเสียงก่อน

### 2.2 ตาราง GPIO

ทั้งหมดอยู่ใน `include/board_pins.h` — **ห้ามเดาเลข GPIO ใหม่** ตัวเลขชุดนี้มาจาก
Arduino_GFX board definition `ESP32_4848S040_86BOX_GUITION`, HomeDing
`DisplayST7701Adapter` และ ESPHome config ของ `guition-esp32-s3-4848s040`
(ไม่ได้มาจาก schematic ของผู้ผลิตโดยตรง — ดู `docs/phase1_hardware_audit.md`)

| หมวด | สัญญาณ | GPIO |
|---|---|---|
| จอ (3-wire SPI init) | CS / SCK / MOSI | 39 / 48 / 47 |
| จอ (RGB sync) | DE / VSYNC / HSYNC / PCLK | 18 / 17 / 16 / 21 |
| จอ (R0–R4) | แดง | 11, 12, 13, 14, 0 |
| จอ (G0–G5) | เขียว | 8, 20, 3, 46, 9, 10 |
| จอ (B0–B4) | น้ำเงิน | 4, 5, 6, 7, 15 |
| แบ็คไลท์ | LEDC PWM | 38 |
| ทัช GT911 | SDA / SCL | 19 / 45 |
| ทัช GT911 | INT / RST | **ไม่ต่อ GPIO** (INT ลง GND, RST ไม่ได้ต่อ) → ต้อง poll I²C เท่านั้น |
| I²S → NS4168 | BCLK / LRCLK / DOUT | 1 / 2 / 40 |
| รีเลย์ (ไม่ใช้) | L1 / L2 / L3 | 40 / 2 / 1 (ใช้ร่วมกับ I²S) |
| microSD (ไม่ใช้) | CS / MISO | 42 / 41 (MOSI/CLK ใช้ร่วมกับจอ 47/48) |

> GPIO 33–37 ถูก PSRAM แบบ OPI ใช้งาน จึงห้ามจัดสรรให้อุปกรณ์อื่น

### 2.3 ไดอะแกรมการต่อ (netlist)

> ทุกเส้นทางด้านล่าง **เดินอยู่ใน PCB ของบอร์ดแล้ว** ผ่านขั้ว FPC ของจอ/ทัช —
> ไดอะแกรมนี้คือ netlist สำหรับตรวจสอบ/อ้างอิงเวลาไล่ปัญหา **ไม่ต้องต่อสายเพิ่ม**
> ยกเว้น 2 อย่าง: สายลำโพง 2 เส้น และการย้ายตัวต้านทาน 0Ω (R25–R27 → R21–R23) ใน §2.1

```
┌────────────────────────────────────────────────────────────────┐
│                    ESP32-S3-WROOM-1  N16R8                     │
│                16 MB QIO flash | 8 MB OPI PSRAM                │
│                 GPIO 33-37 = PSRAM (reserved)                  │
└───────────────┬──────────────┬──────────────┬──────────────┬───┘
                │ RGB565+sync  │ I2C          │ I2S          │ UART0
                │ (20 signals) │ (2)          │ (3)          │ (CH340)
                ▼              ▼              ▼              ▼
          ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐
          │ ST7701    │  │ GT911     │  │ NS4168    │  │ USB-C     │
          │ 480x480   │  │ touch     │  │ class-D   │  │ CH340     │
          │ +backlight│  │           │  │           │  │           │
          └───────────┘  └───────────┘  └─────┬─────┘  └───────────┘
                                              │
                                              ▼
                                        ┌───────────┐
                                        │  speaker  │
                                        └───────────┘
```

อ่านไดอะแกรม:

- ลูกศร `▼` / `▶` = ทิศทางข้อมูล, ตัวเลขในวงเล็บ = จำนวนเส้น
- กล่อง `+backlight` คือไฟหลังจอ **GPIO38 (LEDC PWM)** — ไม่แยกเส้นเพื่อให้อ่านง่าย
- `USB-C` ต่อผ่าน **CH340 → UART0** เท่านั้น (บอร์ดนี้ไม่มี native USB) ใช้ทั้งแฟลชและ log
- สีจอ 16 บิต = **R 5 + G 6 + B 5**; 3-wire SPI (`CS`/`SCK`/`MOSI`) ใช้เฉพาะตอน init

#### ก. จอ ST7701 480×480 (RGB565 + 3-wire SPI สำหรับ init)

```
 ESP32-S3        สัญญาณ              ST7701 / แผงจอ
 ────────        ───────              ──────────────
 GPIO18  ───────▶ DE      (data enable)
 GPIO17  ───────▶ VSYNC
 GPIO16  ───────▶ HSYNC
 GPIO21  ───────▶ PCLK    (12 MHz → LCD_PCLK_HZ)

 GPIO11  ───────▶ R0      GPIO8   ───────▶ G0      GPIO4  ───────▶ B0
 GPIO12  ───────▶ R1      GPIO20  ───────▶ G1      GPIO5  ───────▶ B1
 GPIO13  ───────▶ R2      GPIO3   ───────▶ G2      GPIO6  ───────▶ B2
 GPIO14  ───────▶ R3      GPIO46  ───────▶ G3      GPIO7  ───────▶ B3
 GPIO0   ───────▶ R4      GPIO9   ───────▶ G4      GPIO15 ───────▶ B4
                          GPIO10  ───────▶ G5

 GPIO39  ───────▶ CS      (3-wire 9-bit SPI: ใช้ตอน init เท่านั้น)
 GPIO48  ───────▶ SCK
 GPIO47  ───────▶ MOSI    (ใช้ร่วมกับ microSD)

 GPIO38  ───────▶ BL      (LEDC PWM → วงจร backlight)
```

- 16 บิตสีต่อพิกเซล: R 5 บิต, G 6 บิต, B 5 บิต (RGB565) — framebuffer 480×480×2 = 460 KB ใน PSRAM
- LCD DMA ดึงภาพผ่าน **DRAM bounce buffer 16 บรรทัด** (`LCD_BOUNCE_LINES`) ไม่ได้อ่าน PSRAM ตรงๆ

#### ข. ทัช GT911 (I²C — poll เท่านั้น)

```
 ESP32-S3                       GT911
 ────────                       ─────
 GPIO19  ──────── SDA ─────────▶ SDA      (addr 0x5D, 100 kHz)
 GPIO45  ──────── SCL ─────────▶ SCL      (strapping pin — ห้ามขับตอนบูต)

           (INT) ─── ต่อลง GND  ✗ ไม่มี GPIO
           (RST) ─── ไม่ได้ต่อ   ✗ ไม่มี GPIO
```

- เพราะ INT ไม่ได้ต่อ GPIO จึงต้อง **poll ทะเบียน `0x814E` ทุก 25 ms** (LVGL indev)
- ไม่มี RST จาก MCU → รีเซ็ตทั้งบอร์ดต้อง **ถอดปลั๊ก** (กด EN ของ ESP32 ไม่รีเซ็ต GT911)
- ค่าดิบจากชิป = พิกัดจออยู่แล้ว (identity) คาลิเบรต 3×3 (`k`) เป็นทางเลือก

#### ค. เสียง I²S → NS4168 → ลำโพง

```
 ESP32-S3                       NS4168 (class-D)              ลำโพง
 ────────                       ───────────────              ─────
 GPIO1   ──────── BCLK ────────▶ BCLK
 GPIO2   ──────── LRCLK ───────▶ LRCK / WCLK
 GPIO40  ──────── SDATA ───────▶ DIN
                                 OUT+ ───────────────────────▶ + (สาย 1)
                                 OUT- ───────────────────────▶ − (สาย 2)
```

- GPIO1 / GPIO2 / GPIO40 เป็นพินเดียวกับรีเลย์ L3 / L2 / L1 ที่ไม่ได้ใช้ในโปรเจกต์นี้
- **เงื่อนไขให้มีเสียง:** ย้าย 0Ω จาก **R25/R26/R27 → R21/R22/R23** (ตามคู่มือผู้ขาย)
- `setPinout()` ถูกเรียก **ครั้งเดียวต่อบูต** — เรียกซ้ำจะสร้าง I²S channel ไม่ได้และเสียงเงียบ

#### ง. ไฟเลี้ยง / debug

```
 USB-C ──▶ CH340 (USB-SERIAL) ──▶ UART0 GPIO43/RX · GPIO44/TX   (115200)
   │
   └────▶ 5 V ──▶ LDO 3.3 V ──▶ ESP32-S3 / จอ / GT911 / NS4168

 ปุ่ม EN / BOOT บนบอร์ด = รีเซ็ต/แฟลช (ไม่รีเซ็ต GT911 — ดูข้อ ข)
```

---

## 3. สถาปัตยกรรมซอฟต์แวร์

```
src/stream_test/main.cpp   ตัวต่อสาย: boot order, serial keys, panel refresh, action pump
src/storage/   settings_store   NVS: wifi, stations, touch cal (ที่เดียวที่แตะ Preferences)
src/radio/     station_manager  รายการสถานี + active + next/prev
               network_link     Wi-Fi STA + IP + NTP (Asia/Bangkok)
               radio_player     play/stop/next/prev + watchdog รีสตาร์ท 15 s
src/audio/     audio_engine     ห่อ ESP32-audioI2S (setPinout ครั้งเดียว, volume/mute, ICY)
src/display/   display_panel    ST7701 + backlight + bounce buffer
               touch_gt911      ไดรเวอร์ polled I²C (คืนค่าดิบ ไม่แปลงพิกัด)
               touch_calibration grid 3×3 + affine → พิกัดจอ, NVS namespace "touch3"
               lv_port          LVGL: display (strip 480×40) + pointer indev
src/ui/        lv_ui            หน้าจอ retro 4 หน้า (view + action queue)
               legacy/          UI เวอร์ชัน Arduino_GFX เดิม (ไม่ถูกคอมไพล์ — เก็บไว้ rollback)
src/web/       web_setup        พอร์ทัล AP (HTML ใน PROGMEM)
```

กติกาสำคัญที่ยึดตลอดโปรเจกต์:

- **UI เป็น view ล้วน** — ปุ่มทุกปุ่ม push `LvUi::Action` ลงคิว `main.cpp` ดึงด้วย
  `takeAction()` แล้วเรียกโมดูลเอง มีที่เดียวที่แตะ `Audio` คือ `main.cpp`
- **แยก buffer ให้เหมาะกับ bus**: framebuffer 480×480×2 อยู่ใน PSRAM แต่ LCD DMA
  ต้องมี **DRAM bounce buffer 16 บรรทัด** (2 × 15,360 B) ไม่งั้น Wi-Fi/TLS + ถอดรหัสเสียง
  แย่ง bandwidth แล้วภาพสั่นซ้าย-ขวา
- **LVGL ไม่ใช้ framebuffer เป็น draw buffer**: วาดลง strip 480×40 ใน PSRAM แล้ว
  `flush_cb` ส่งเข้า `draw16bitRGBBitmap()` เพื่อให้ได้ PSRAM cache write-back ที่ RGB DMA
  ต้องการ; heap ของ LVGL อยู่ใน PSRAM (`LV_MEM_CUSTOM`)
- **ทัช: poll เท่านั้น** (INT ไม่ได้ต่อ GPIO) ทุก 25 ms ผ่าน `LV_INDEV_DEF_READ_PERIOD`
  ค่าดิบจาก GT911 เป็นพิกัดจออยู่แล้ว → default เป็น **identity** ไม่ต้องคาลิเบรต
  (`k` ทาง serial ยังรันกริด 3×3 ได้ถ้าอนาคตใส่ฟิล์มหนา)

---

## 4. วิธีแฟลช

### 4.1 เตรียมเครื่อง

```bash
pip install platformio        # หรือใช้ PlatformIO IDE ใน VS Code
```

> บนเครื่องที่พัฒนาโปรเจกต์นี้ `python` หลักเป็น 3.14 ที่ยังไม่ได้ลง PlatformIO
> จึงเรียกผ่าน virtualenv ของ PlatformIO เอง:
> `C:/Users/Mrk/.platformio/penv/Scripts/python.exe -m platformio run ...`

### 4.2 สร้างไฟล์ความลับ

`include/secrets.h` ถูก gitignore ไว้ (**ห้าม commit**) สร้างเองแล้วใส่ค่าของคุณ:

```cpp
#pragma once

#define WIFI_SSID   "ชื่อไวไฟ"
#define WIFI_PASS   "รหัสผ่าน"
#define STREAM_URL  "https://example.com/stream"
```

ค่าเหล่านี้ใช้แค่ครั้งแรก: ถ้า NVS ยังไม่มี Wi-Fi ระบบจะ migrate ให้แล้วเก็บลง NVS
หลังจากนั้นแก้ผ่านพอร์ทัลได้เลย (ไม่ต้องแฟลชใหม่)
(`STREAM_URL` ถูก seed เป็นสถานีแรกชื่อ `Default station`)

### 4.3 เลือก environment

| env | ใช้ทำอะไร | คำสั่ง |
|---|---|---|
| `stream_test` | **เฟิร์มแวร์หลัก** (วิทยุ + LVGL UI + พอร์ทัล) | `pio run -e stream_test -t upload` |
| `audio_test` | ทดสอบเส้นทางเสียงอย่างเดียว (ไม่มี Wi-Fi/จอ) | `pio run -e audio_test -t upload` |
| `esp32-s3-4848s040` | bring-up test จอ + ทัช + หน่วยความจำ (`src/main.cpp`) | `pio run -e esp32-s3-4848s040 -t upload` |

### 4.4 แฟลช + ดู log

```bash
pio device list                          # ดูว่าได้ COM อะไร (ตัวอย่างนี้ COM5)
pio run -e stream_test -t upload         # คอมไพล์ + แฟลช (921600 baud)
pio device monitor -b 115200             # ดู log (115200)
```

`upload_port` / `monitor_port` ถูกกำหนดเป็น `COM5` ไว้ใน `platformio.ini` ทั้ง 3 env —
**ถ้าเลขพอร์ตเปลี่ยนให้แก้ทั้งไฟล์** (หรือลบออกแล้วให้ PlatformIO ตรวจจับเอง)

บอร์ดที่แฟลชแล้วจะบูตเอง → ต่อ Wi-Fi → เล่นสถานีที่ `active` → วาดสถานะทุก 1 วินาที
ถอด/เสียบไฟใหม่ก็กลับมาเล่นเอง ไม่ต้องแฟลชซ้ำ

### 4.5 ปัญหาที่เจอบ่อยตอนแฟลช

| อาการ | สาเหตุ / วิธีแก้ |
|---|---|
| `Failed to connect ... wrong port` หรือพอร์ตถูกจอง | ปิด serial monitor / หน้าต่างที่เปิด COM5 อยู่ (เปิดได้ทีละโปรแกรม) |
| ปิด monitor แล้วบอร์ดรีบูต ขึ้น `rst:0x10 RTCWDT_RTC_RST` | เป็นเรื่องปกติของ CH340 เมื่อปิดพอร์ต **ไม่ใช่เฟิร์มแวร์ crash** |
| แฟลชแล้วจอยังไม่ขึ้นภาพ | ตรวจว่าได้ env `stream_test` (env `audio_test` ไม่มีโค้ดจอ) |
| ไม่มีเสียงออกลำโพง | ยังไม่ได้ย้าย R25–R27 → R21–R23 (ดู §2.1) |
| ภาพสั่นซ้าย-ขวาระหว่างสตรีม | อย่าลด `LCD_BOUNCE_LINES` ต่ำกว่า 16 และอย่าเพิ่ม `LCD_PCLK_HZ` จาก 12 MHz โดยไม่ทดสอบซ้ำ |

---

## 5. การใช้งาน

### 5.1 ปุ่มบนหน้าจอ (ทัชจริง)

| ปุ่ม | การทำงาน |
|---|---|
| `◀ PREV` / `NEXT ▶` | เปลี่ยนสถานีทันที (ไม่ต้องรีบูต) |
| `■ PLAY` / `▶ STOP` | สลับตามสถานะจริงของสตรีม (`isRunning()`) |
| `LIST` | เปิดรายการสถานีเต็มจอ 7 แถว/หน้า, ลูกศรซ้าย/ขวาเลื่อนหน้า, `CLOSE` กลับ |
| `SETUP` | หยุดสตรีมแล้วเปิดพอร์ทัล AP |
| `−` / `+` | ลด/เพิ่มเสียง แตะค้างเพื่อเร่งทุก 300 ms |

### 5.2 คีย์ลัดทาง serial (115200)

| คีย์ | การทำงาน |
|---|---|
| `n` / `b` | สถานีถัดไป / ก่อนหน้า |
| `s` | play ↔ stop |
| `m` | mute (สตรีมยังเล่น) |
| `v` / `V` / `+` / `-` | volume ขึ้น / ลง |
| `k` | เข้าโหมดคาลิเบรตทัช (กริด 3×3) |
| `w` | เปิดพอร์ทัลตั้งค่า (เหมือนกด `SETUP`) |
| `t` | dump ค่า GT911 (product id, config, สถานะ, จำนวน read ที่สำเร็จ/ล้มเหลว) |
| `r` | รีบูต |

### 5.3 พอร์ทัลตั้งค่า (จากมือถือ)

1. กด `SETUP` บนจอ (หรือคีย์ `w`) — สตรีมหยุดชั่วคราว
2. มือถือต่อ AP **`RetroRadio-Setup`** รหัส **`radio1234`** (ดู SSID/รหัส/IP บนจอได้)
3. เปิด `http://192.168.4.1`
   - **Wi-Fi**: ดูเครือข่ายที่บันทึกไว้, `Scan networks`, แตะชื่อเพื่อเติมช่อง, บันทึก → รีบูต
   - **Stations**: ตาราง `ชื่อ + ลิงก์` + ปุ่มลบ, ฟอร์มเพิ่มสถานี (สูงสุด 12)
   - **Reboot to player** กลับไปเล่น
4. กด `DONE` บนจอเพื่อรีบูตเช่นกัน

AP เปิดแบบ `WIFI_AP_STA` เพื่อให้สแกนเน็ตได้โดยไม่หลุดจากมือถือ และถ้าเน็ตที่บันทึกไว้
ยังเข้าถึงได้ จะเปิดพอร์ทัลบน IP ของ LAN ด้วย

---

## 6. ค่าที่ปรับได้ (`include/app_config.h`)

| ค่า | ค่าเริ่มต้น | หมายเหตุ |
|---|---|---|
| `LCD_BOUNCE_LINES` | `16` | DRAM bounce buffer ของ LCD DMA — **อย่าลด** ไม่งั้นภาพสั่นตอนสตรีม |
| `LCD_PCLK_HZ` | `12000000` | pclk ของจอ; เปลี่ยนแล้วต้องทดสอบภาพใหม่ |
| `UI_DYNAMIC` | `1` | ตั้ง `0` เพื่อหยุดวาดซ้ำ (ใช้แยกปัญหาภาพออกจากการวาด) |
| `AUDIO_VOLUME_MAX` / `_DEFAULT` | `21` / `12` | ช่วงเสียงของ NS4168 |
| `STREAM_RETRY_MS` | `15000` | รอ 15 s ก่อนรีสตาร์ทสตรีม (สตรีมจริงขึ้น `slow stream` ได้โดยไม่ต้องรีสตาร์ท) |
| `APP_TZ` | `"ICT-7"` | POSIX TZ ของ Asia/Bangkok (UTC+7 ไม่มี DST) |

สี/พิกัด/ฟอนต์ของหน้าจออยู่ใน `src/ui/lv_ui.cpp` (พาเลตต์เดียวกับเวอร์ชัน Arduino_GFX เดิม,
ฟอนต์ 5×7 ตัวเดียวกับ `font/glcdfont.h` ของ Arduino_GFX)

---

## 7. ประสิทธิภาพที่วัดได้จริง (บนบอร์ดนี้)

| ตัวชี้วัด | ค่า |
|---|---|
| แฟลชที่ใช้ | **32.9 %** (2,155,878 B) |
| DRAM | **21.7 %** (71,116 B) |
| heap ตอนสตรีม + UI | ~**82 KB** (PSRAM ยังว่างมาก) |
| LVGL pass ปกติ (นาฬิกา/มิเตอร์ 1 Hz) | **2–36 ms** |
| LVGL pass ตอนเปลี่ยนจอทั้งหน้า (LIST/SETUP) | **110–145 ms** (render 230k px ลง PSRAM) |
| เวลา flush ต่อ area | **< 3 ms** (ไม่เคยเกิน) |
| อัตราการ poll ทัช | 40 ครั้ง/วินาที, `ok/fail = 99,400 / 0` |
| เสียงระหว่างวาดจอ | ไม่สะดุด (input buffer ไต่ถึง 100+ KB) |

> ระหว่างสไปก์ตอนเปลี่ยนจอ 110–145 ms เสียงยังเล่นปกติ เพราะเวลาที่กินคือการ render
> ไม่ใช่การส่งภาพออกจอ — แต่ไม่ควรเพิ่มจำนวนการวาดเต็มจอต่อวินาที

---

## 8. ข้อจำกัด / ยังไม่ได้ทำ

- **ไม่มี native USB** — debug ผ่าน UART0 (CH340) เท่านั้น
- **ไม่ใช้ microSD และรีเลย์** — พินถูกใช้ร่วมกับจอ/I²S ตามตาราง §2.2
- **ไม่มี unit test / host build** — ทุกอย่างตรวจบนบอร์ดจริงผ่าน serial log
  (สคริปต์ช่วยทดสอบอยู่ใน `tools/`: `portal_check.py`, `audio_cycle_check.py`)
- **คาลิเบรตทัชยังไม่เคยรันจริง** — ค่าดิบเป็นพิกัดจออยู่แล้ว (identity) จึงไม่จำเป็น
  จนกว่าจะมีฟิล์มหนา/เปลี่ยนแผง
- **wiring diagram ยังไม่มี** — มีแค่ตาราง GPIO ที่อ้างอิงแหล่งข้างต้น
- LVGL ตรึงที่ **8.3.11** (ไม่ใช่ v9) เพราะ API `lv_disp_draw_buf` / draw event ที่พอร์ตนี้ใช้
- `src/ui/legacy/` เป็นโค้ดเก่าไว้ rollback ไม่ได้ถูกคอมไพล์

---

## 9. เอกสารในโปรเจกต์

| ไฟล์ | เนื้อหา |
|---|---|
| `docs/status.md` | สถานะล่าสุด, ผลทดสอบที่มี log, ปัญหาที่แก้แล้ว/ค้างอยู่ |
| `docs/phase1_hardware_audit.md` | ตาราง GPIO + แหล่งอ้างอิง, ข้อควรระวังฮาร์ดแวร์ |
| `docs/phase2_bringup.md` | bring-up จอ/ทัช/หน่วยความจำ |
| `docs/phase3_audio_status.md`, `docs/phase3_measurement_guide.md` | เส้นทางเสียง I²S และวิธีวัด |
| `knowledge.md` | บทเรียนเชิงเทคนิคสะสม (อ่านก่อนแก้โค้ด) |
| `promp.txt` | โจทย์/ข้อกำหนดของโปรเจกต์ |

---

## 10. เครดิต / สิทธิ์การใช้งาน

- ไอคอน pixel art: **Pixelarticons** (MIT) — แปลงเป็น bitmap 1-bit ด้วย `tools/svg2header.py`
- ไลบรารี: ESP32-audioI2S, GFX Library for Arduino, LVGL — ดู LICENSE ของแต่ละโปรเจกต์ต้นทาง
- โค้ดในโปรเจกต์นี้เขียนขึ้นสำหรับบอร์ด Guition ESP32-S3-4848S040 โดยเฉพาะ
