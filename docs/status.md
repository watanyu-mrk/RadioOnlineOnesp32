# สถานะโปรเจกต์ — จุดกลับมาทำต่อ

อัปเดตล่าสุด: **2026-10-22 11:00** · บอร์ดต่ออยู่ที่ **COM5** (USB-SERIAL CH340)

> บอร์ดเปลี่ยนเลขพอร์ตจาก COM29 มาเป็น **COM5** แล้ว (แก้ใน `platformio.ini` ทั้ง 3 env เรียบร้อย)
> ถ้าเลขพอร์ตเปลี่ยนอีก ให้เช็กด้วย `python -m platformio device list`
> (บนเครื่องนี้ใช้ `C:/Users/Mrk/.platformio/penv/Scripts/python.exe -m platformio` — `python`
> ตัวหลักเป็น 3.14 ที่ยังไม่ได้ลง PlatformIO)

## 1. เฟิร์มแวร์ที่อยู่บนบอร์ดตอนนี้

`stream_test` (จอ retro **บน LVGL 8.3.11** + RGB bounce buffer + touch ทุกปุ่ม + IP ในแถบล่าง) —
แฟลชล่าสุด SUCCESS (2026-10-22 10:55) · โค้ดแยกเป็นโมดูลแล้ว (ดู §1.1)

```
python -m platformio run -e stream_test -t upload
```

บอร์ดบูตเอง → ต่อ Wi-Fi → เล่น `STREAM_URL` → วาดสถานะบนจอทุก 1 วินาที
(ถอด/เสียบไฟใหม่ก็กลับมาเล่นเอง ไม่ต้องแฟลชซ้ำ)

### 1.1 โครงสร้างโค้ด — แยกโมดูลตาม `promp.txt` §8 แล้ว (2026-10-22)

`src/stream_test/main.cpp` เหลือ ~370 บรรทัด เป็นตัวต่อสาย/ลูปหลักเท่านั้น:

```
include/  app_config.h (ค่า tune: bounce buffer, pclk, volume, TZ, retry)
          lv_conf.h (LVGL 8.3.11: heap/draw buffer ใน PSRAM, tick=millis)
          board_pins.h  secrets.h  ui_icons.h (generated)
src/stream_test/main.cpp   boot order · serial keys · panel refresh · stats log · action pump
src/storage/     settings_store.{h,cpp}    NVS: wifi, stations, touch cal (ที่เดียวที่แตะ Preferences)
src/radio/       station_manager.{h,cpp}   รายการสถานี + active + next/prev
                 network_link.{h,cpp}      Wi-Fi STA + IP + NTP/เวลา Bangkok
                 radio_player.{h,cpp}      play/stop/next/prev + watchdog 15 s
src/audio/       audio_engine.{h,cpp}      ห่อ ESP32-audioI2S (setPinout ครั้งเดียว, volume/mute, ICY)
src/display/     display_panel.{h,cpp}     ST7701 + backlight + bounce buffer
                 touch_gt911.{h,cpp}       ไดรเวอร์ polled I²C (คืนค่าดิบ)
                 touch_calibration.{h,cpp} grid 3×3 + affine → พิกัดจอ
                 lv_port.{h,cpp}           LVGL: display (480×40 strip) + indev GT911
src/ui/          lv_ui.{h,cpp}             หน้าจอ retro 4 หน้าใน LVGL (view + action queue)
                 legacy/ui_status.{h,cpp}  เวอร์ชัน Arduino_GFX เดิม (ไม่ถูกคอมไพล์ — เก็บไว้ rollback)
src/web/         web_setup.{h,cpp}          พอร์ทัล AP (+ HTML ใน PROGMEM)
```

- `include/ui_status.h`, `include/web_setup.h`, `include/radio_config.h`,
  `include/touch_gt911.h` **ถูกลบแล้ว** — ของเดิมอยู่ใน `include/` ทั้งหมด
- `build_src_filter` ใน `platformio.ini`: `stream_test` = `+<stream_test/> +<audio/> +<display/>
  +<radio/> +<storage/> +<ui/> -<ui/legacy/> +<web/>`; env หลักและ `audio_test` ตัดโมดูลเหล่านี้ออก
  (env หลักยังเป็น bring-up test `src/main.cpp` ตามเดิม)
- พฤติกรรม/NVS ไม่เปลี่ยน: namespace `radio` + `touch3` เดิม บอร์ดบูตขึ้นมาใช้ค่าที่มีอยู่แล้ว
- ขนาด: Flash **32.9%** (2,155,878 B — LVGL กินเพิ่ม ~3.4%), DRAM **21.7%** (71,116 B),
  heap ตอนสตรีม ~**82 KB** (เดิม 91.8 KB: LVGL object + heap ใน PSRAM แต่ style/property
  บางส่วนยังอยู่ DRAM) — ยังเหลือเฟือสำหรับ TLS

### 1.2 UI = LVGL (ทำเสร็จ 2026-10-22 ตาม `promp.txt` §4)

- **display**: `lv_disp_drv_t` 480×480 + draw buffer เป็น **strip 480×40 ใน PSRAM** (38 KB)
  — ไม่ใช้ framebuffer เต็มก้อนของ Arduino_GFX เป็น draw buffer เพราะ LVGL ต้องมี
  invalidation ของตัวเอง; `flush_cb` ส่งแต่ละ area เข้า `draw16bitRGBBitmap()`
  ซึ่งทำให้ PSRAM cache write-back ที่ RGB DMA ต้องการ + ใช้ bounce buffer เดิม
- **indev**: `lv_indev_drv_t` pointer อ่าน GT911 ทุก 25 ms ผ่าน `TouchCalibration::map()`;
  ตอนคาลิเบรตต้อง `lvport::touchEnabled(false)` เพราะทั้งสองฝั่งแย่งคอนโทรลเลอร์กัน
- **หน้าจอ 4 หน้า** (parity กับเวอร์ชัน GFX): faceplate หลัก, LIST (7 แถว + เลื่อนหน้า),
  SETUP (SSID/รหัส/IP + DONE), TOUCH CAL (จุดเป้ากากบาท 9 จุด)
- **หน้าตาเดิมเป๊ะ**: palette/พิกัด/ฟอนต์ 5×7 ตัวเดียวกับ Arduino_GFX (`font/glcdfont.h`),
  นาฬิกา 7-segment/มิตเตอร์/ไอคอน/แผ่นเสียงวาดด้วย custom draw callback (`LV_EVENT_DRAW_MAIN`)
- **UI ไม่แตะ audio/network**: ปุ่มทุกปุ่ม push `LvUi::Action` ลง queue เดียว
  `main.cpp` ดึงด้วย `takeAction()` แล้วเรียกโมดูลเอง (แบบเดียวกับ `handleTouch()` เดิม)
- ทุก action ใน enum ต้องมี `case` ใน `pumpActions()` — ลืม `ACT_LIST_CLOSE` ทำให้ปุ่ม CLOSE
  เงียบและจอค้างอยู่หน้า LIST (เจอตอนทดสอบ แล้วแก้แล้ว)

## 2. หน้าจอ (จาก `src/ui/lv_ui.cpp`) — สไตล์ retro hi-fi

ออกแบบใหม่เป็นหน้าเครื่องรับวิทยุยุค 70s/80s: กรอบสีส้มไหม้ ตัวหนังสือสีครีม
ตัวเลข 7-segment สีเหลืองอำพัน (มี segment จางแบบ VFD จริง), มิเตอร์ buffer แบบแบ่งช่อง,
ตลับเทปที่มี reel หมุนตามเวลาเล่นจริง และ LED สถานะ (เขียว/ส้ม/แดง ตามคุณภาพริง)

```
┌ กรอบส้ม + สกรู 4 มุม ────────────────────────────────┐
│ [วิทยุ] INTERNET RADIO            [เสาสัญญาณ เปลี่ยนสีตาม RSSI] │
│         RETRO STREAM DECK                            │
│ [โน้ต]  COOLfahrenheit | Easy List..                 │
│         Music Freeze Your Mind                       │
│ BANGKOK   UTC+7      ┌ MP3 / 128 kbps / 44.1 kHz ┐   │
│  [2][0]:[4][1]:[0][7]└ 2ch                        ┘   │
│ BUFFER                       258 KB  50%             │
│ ▮▮▮▮▮▮▮▮▮▮▮▮▮▮▮▯▯▯▯▯▯▯▯▯▯▯▯                          │
│ [LINK -81 dBm] [HEAP 94 K] [ VOLUME  − 12 + ]        │
│ [◀PREV][■PLAY][NEXT▶][LIST][SETUP]                   │
│ stream test ...           UP 12:34   ip 192.168.1.147 │
└──────────────────────────────────────────────────────┘
```

- ทุกค่ามาจากสถานะจริงของ `Audio` (`isRunning()`, bitrate, sample rate, ICY metadata)
- สีของไอคอนเสา = คุณภาพสัญญาณจริง (เขียว > -67 dBm, ส้ม > -78, แดงต่ำกว่านั้น/หลุด)
- แถบล่างสุด (บรรทัดเล็กๆ ใต้ปุ่ม) แสดงชื่อเฟิร์มแวร์ · `UP mm:ss` (เวลาทำงาน) ·
  `ip 192.168.1.147` ชิดขวา (สีส้ม) — เอาไว้เปิดพอร์ทัลตั้งค่าจากมือถือได้โดยไม่ต้องดู serial
  ค่า IP มาจาก `WiFi.localIP()` ผ่าน `UiInfo::ip` แล้ววาดใหม่ทุกวินาที
- reel ตลับเทปหยุดหมุนเมื่อ `isRunning()` เป็น false — ไม่มีอนิเมชันหลอก
- นาฬิกา = เวลาจริงเขตกรุงเทพ (UTC+7) จาก NTP (`ICT-7`), ก่อนซิงก์เห็นแค่ segment จาง
- ปุ่มทั้งแถวเป็น**ทัชจริง** (GT911 → indev ของ LVGL, `src/display/touch_gt911.cpp`):
  `PREV` / `NEXT` เปลี่ยนสถานีทันทีไม่ต้องรีบูต, `LIST` เปิดรายการสถานีเต็มจอ, `SETUP` เปิดพอร์ทัล
  (hit-test เป็นของ LVGL เอง ไม่มีพิกัดถูกล็อกในโค้ด UI อีกแล้ว)
- ปุ่มกลางสลับ `PLAY` / `STOP` ตาม `Audio::isRunning()` จริง; reel/แผ่นเสียงหมุนเมื่อเล่นจริงเท่านั้น
- `−` / `+` ยังเป็นทัชจริง แตะค้างเพื่อเร่งทุก 300 ms
- คีย์ลัดทาง serial: `n`/`b` สถานีถัดไป/ก่อนหน้า, `s` play-stop, `m` mute, `v`/`V` volume,
  `k` คาลิเบรตทัช, `w` เปิดพอร์ทัล, `r` รีบูต, `t` dump ค่า GT911

### 2.1 ไอคอน (ดาวน์โหลดจากอินเทอร์เน็ต)

ใช้ชุด **Pixelarticons** (MIT) — pixel art 24×24 — แปลงเป็น bitmap 1-bit ฝังในแฟลช
960 B ทั้งชุด ไม่ใช้ RAM และไม่ต้องมีตัวถอดรูปบนบอร์ด

```bash
# ดึง/อัปเดตไอคอน (assets/icons/svg/*.svg)
curl -sO https://raw.githubusercontent.com/halfmage/pixelarticons/master/svg/radio.svg

# สร้าง include/ui_icons.h ใหม่
python tools/svg2header.py --out include/ui_icons.h \
  radio=assets/icons/svg/radio.svg music=assets/icons/svg/music.svg \
  note=assets/icons/svg/note.svg signal=assets/icons/svg/signal.svg \
  headphone=assets/icons/svg/headphone.svg clock=assets/icons/svg/clock.svg \
  battery=assets/icons/svg/battery-full.svg album=assets/icons/svg/album.svg \
  keyboard=assets/icons/svg/keyboard-music.svg bars=assets/icons/svg/cellular-signal-3.svg
```

> `tools/svg2header.py` เป็น pure python (ไม่ต้องลง Pillow) รองรับ path แบบ
> M/m/L/l/H/h/V/v/Z + scanline fill พอสำหรับ pixel art ชุดนี้
> `tools/png2header.py` เก็บไว้อีกทางเลือกเผื่ออยากใช้ PNG/RGB565 (Twemoji CC-BY 4.0)
> — โฟลเดอร์ `assets/icons/*.png` ยังอยู่แต่ยังไม่ได้ใช้ในเฟิร์มแวร์

### 2.2 พอร์ทัลตั้งค่า (`src/web/web_setup.cpp` + `src/storage` / `src/radio`)

ปุ่ม `SETUP` บนหน้าจอ (หรือคีย์ `w` ทาง serial, หรือบู้ตครั้งแรกที่ยังไม่มี Wi-Fi)
-> หยุดสตรีม แล้วเปิด AP `RetroRadio-Setup` / รหัส `radio1234` -> เปิด `http://192.168.4.1`
จากมือถือ (หน้าจอโชว์ SSID/รหัส/IP ให้อ่าน)

| ส่วนของหน้า | ทำอะไรได้ |
|-------------|----------|
| 1 · Wi-Fi | ดูเครือข่ายที่บันทึกไว้, `Scan networks` แล้วแตะชื่อเพื่อเติมช่อง, บันทึก -> รีบูต |
| 2 · Stations | ตาราง `ชื่อ + ลิงก์` + ปุ่ม `x` ลบทีละรายการ, ฟอร์มเพิ่มสถานีใหม่ (สูงสุด 12) |
| ล่างสุด | `Reboot to player` |

- ระหว่างพอร์ทัลทำงาน สตรีมถูกหยุด (`audio.stopSong()`); AP เปิดแบบ `WIFI_AP_STA`
  เพื่อให้สแกนเน็ตได้โดยไม่หลุดจากมือถือ และถ้าเน็ตที่บันทึกไว้ยังถึง จะเข้าได้ทั้ง
  `192.168.4.1` และ IP ใน LAN (log: `setup : also on LAN http://...`)
- ทุกอย่างเก็บใน NVS namespace `radio` (`ssid`, `pass`, `n`, `n<i>`, `u<i>`, `active`)
  -> รีบูตกลับมาเล่นสถานีที่ `active` ชี้ ไม่ใช่ลิสต์ฝังในโค้ด
- บู้ตครั้งแรกที่ยังไม่มีสถานี ระบบ seed ให้ 1 รายการจาก `STREAM_URL` ชื่อ `Default station`
- หน้าเว็บ 4,458 B อยู่ใน PROGMEM (ไม่กิน RAM) — ทั้ง Flash 31.1%, DRAM 19.8%
- รายการสถานีบนจอแสดงได้ 7 แถวต่อหน้า; ถ้ามีเกินจะมีลูกศรซ้าย/ขวาที่แถบล่างเพื่อเลื่อนหน้า
- ทดสอบซ้ำได้ด้วย `tools/portal_check.py` (ไม่ต้องใช้มือถือ): เข้าโหมดตั้งค่าทาง serial
  แล้วยิง HTTP ทั้งหน้าเว็บและ API จากเครื่องคอม ->
  `C:/Users/Mrk/.platformio/penv/Scripts/python.exe tools/portal_check.py 192.168.1.147 COM5`

## 3. ผลทดสอบที่ผ่านจริง (มี log)

| หัวข้อ | ผล | หลักฐาน |
|--------|-----|---------|
| คอมไพล์ 3 env | ผ่าน | `esp32-s3-4848s040`, `audio_test`, `stream_test` |
| จอ ST7701 + สี/ทิศทาง | ผ่าน (ผู้ใช้ยืนยันด้วยตา) | `docs/phase2_bringup.md` |
| ทัช GT911 ตอบสนอง | ผ่าน (ผู้ใช้ยืนยัน) | coordinate mapping ยังไม่ยืนยัน |
| Wi-Fi + HTTPS | ผ่าน | ip 192.168.8.59, TLS 710–8951 ms |
| ถอดรหัส MP3 | ผ่าน | 128 kbps, 44.1 kHz, 2ch, ICY metadata |
| I²S ส่งข้อมูลออก | ไม่มี error, DMA ทำงาน | `i2s=ready`, mean level ไม่ได้วัด |
| **เสียงออกลำโพง** | **ผ่าน** (ผู้ใช้ยืนยัน 2026-10-21) | บัดกรี R21–R23 แล้วสตรีมมีเสียงออกลำโพง |
| ทัชแม่นยำหลังแก้ parser | **ผ่าน** (ผู้ใช้ยืนยัน “ตรงแล้วดีมาก”) | `src/display/touch_gt911.cpp` อ่าน x/y แบบ little-endian |
| ปุ่ม `PREV`/`NEXT`/`LIST`/`SETUP` + พอร์ทัล | ผ่านการทดสอบจาก PC (2026-10-21 21:4x) | `GET /` 200 (4,458 B), `/api/status`, `/api/scan` (4 เครือข่าย, 2.9 s), add/del สถานี, รีบูตกลับมาเล่นต่อ |
| **แยกโมดูล (2026-10-22 09:1x–09:26)** | ผ่าน | 3 env คอมไพล์ผ่าน (`stream_test`, `esp32-s3-4848s040`, `audio_test`); บนบอร์ด: สตรีม MP3 128 kbps `played=+5s`, buffer ~258 KB, heap 91,824 B, ui 8.8–19.5 ms; stop→play `played=+2..3s` ไม่มี i2s error; สลับสถานี `b` กลับมา MP3 ได้; `tools/portal_check.py` → `GET /` 200 (4,458 B), scan 6 เครือข่าย, add/del, รีบูตกลับมาเล่น |
| **ย้าย UI ไป LVGL (2026-10-22 10:2x–11:00)** | ผ่าน | 3 env คอมไพล์ผ่าน; บูตขึ้น `lvgl : v8.3.11`, `display : ready (ST7701 480x480, LVGL) (74 LVGL objects)`, heap ~82 KB, สตรีม Talay AAC 96 kbps + COOLfahrenheit MP3 128 kbps เล่นได้ต่อเนื่อง; ทดสอบปุ่มด้วยการฉีดพิกัดเข้า indev ชั่วคราว (โค้ดทดสอบถอดออกแล้ว): LIST→เลือกแถว = `picked 1`, เลื่อนหน้า→`picked 8 "Talay"`, CLOSE กลับหน้าแรกแล้ว NEXT/PREV/PLAY-STOP/VOLUME ทำงาน, SETUP เปิดพอร์ทัล + DONE รีบูตเอง; `GET /` 200 (4,458 B) + `/api/status` 200 จาก LAN ระหว่างหน้า SETUP แสดงอยู่ |
| ทัชจริงบนปุ่ม LVGL | **ผ่าน** (ผู้ใช้ยืนยัน “ทัชตรงแล้ว” 2026-10-22 14:3x) | `touch : down x=0 y=0 (raw 156,410)` = ค่าดิบถูกต้อง แต่ `TouchCalibration::map()` คืน (0,0) เพราะ branch identity หาย; แก้แล้วใน `src/display/touch_calibration.cpp` |
| นาฬิกา 7-seg บน LVGL | **ผ่าน** (ยืนยันจาก framebuffer map) | `clockFace()` วาดที่พิกัด relative → ถูก clip; แก้ให้บวก `ox,oy` แล้วเห็น `15:02:39` ในแผนที่ 8 px |
| ตัวควบคุม GT911 ยังสแกนและตอบ I²C | ผ่าน | `ok/fail = 99400 / 0`, `status=0x80`, product id `911`, config `ver 0x43 xmax/ymax 480/480`, config block 0x8047–0x80FE อ่านครบ |

## 4. ปัญหาที่ค้างอยู่

**นาฬิกา 7-segment หายไปจากหน้า faceplate — แก้แล้ว (2026-10-22 15:0x):**
อาการคือกรอบนาฬิกา (x 22–313, y 192–257) ว่างเป็นพื้นหลังสนิท ทั้งที่ข้อมูลเวลามาจริง
(`state : time=15:02:39`) และ LVGL ก็เรนเดอร์ทุกวินาที (`ui`~36 ms/รอบ)
ต้นเหตุ: `clockFace()` ใน `src/ui/lv_ui.cpp` วาดด้วยพิกัด **relative** (`digitX(i)`,
`y = 0`) แต่ `fillRect()` ของพอร์ตนี้เรียก `lv_draw_rect()` ซึ่งต้องการพิกัด **absolute**
บนจอ → ทุก segment ถูก clip ทิ้งเพราะอยู่นอกกล่องของ object
แก้โดยรับ `ox, oy` (มุมบนซ้ายของ object) เข้า `clockFace()`/`colon()` แล้วบวกก่อนวาด
วิธีพิสูจน์โดยไม่ต้องใช้ตา: ดัมป์ framebuffer (`Arduino_RGB_Display::getFramebuffer()`)
เป็นแผนที่ ASCII ทุก 8 px — ก่อนแก้พบว่าพื้นที่นาฬิกาเป็น `.` ทั้งบล็อก หลังแก้เห็นลาย
segment ของ `15:02:39` ครบ (โค้ดดัมป์เป็น TEMP — ถอดออกแล้ว)


**จอไม่ตอบสนองต่อทัช / จุดแดงขึ้นมุมซ้ายบนตลอด — แก้แล้วและยืนยันบนจอจริง (2026-10-22 14:1x–14:3x):**
อาการคือแตะตรงไหนก็ได้ marker สีแดงไปโผล่ที่ (0,0) และไม่มีปุ่มไหนทำงาน ทั้งที่ GT911
อ่านค่าดิบได้ถูกต้อง (`touch : down x=0 y=0 (raw 156,410) bytes 9C 00 9A 01 1F 00 00`).
ต้นเหตุอยู่ใน `TouchCalibration::map()`: ตอนที่ NVS ยังไม่มีตารางคาลิเบรต (`have_ == false`)
mันยังใช้ `data_.aff[]` ซึ่งเป็นศูนย์ทั้งหมด (struct ถูก zero-init) → ทุกพิกัดกลายเป็น 0,0
ทั้งที่คอมเมนต์ใน `main.cpp` ระบุว่า identity คือค่าเริ่มต้นที่ถูกต้อง branch identity
หายไปตอนแยกโมดูล แก้โดยคืนค่าดิบตรงๆ (clamp 0..479) เมื่อ `!have_`
บทเรียน: อาการ "ทัชไม่ติด" ที่ระดับ LVGL ให้ดู log `raw=` ก่อน — ถ้า raw ถูกแต่ mapped เป็น 0
ต้นเหตุอยู่ที่ layer แปลงพิกัด ไม่ใช่ไดรเวอร์ I²C (การวินิจฉัยที่ผ่านมาทั้งหมด: GT911 สแกนอยู่,
config block 0x8047–0x80FE สมบูรณ์, `ok/fail = 99400/0`, ไม่มี i2c error)

**แตะจอทีไร เวลาชั่วโมง:นาทีหาย แล้วค่อยกลับมาทีละหลัก — แก้แล้ว (2026-10-22 07:0x):**
การแตะจอวาดเครื่องหมายกากบาท แล้วตอนปล่อยนิ้วตั้ง `repaint_ = true` ให้ `update()` วาด
`drawStatic()` ใหม่ทั้งหน้า ซึ่งเรียก `clockGhost()` = segment จางหมด แต่แคช `lastDigits_`
ยังเก็บเลขเดิมไว้ -> `showClock()` เห็นว่า "ไม่เปลี่ยน" จึงไม่วาดหลักนั้น -> HH:MM ค้างเป็น
segment จางจนกว่าหลักนั้นจะเปลี่ยนจริง (นาทีละครั้ง) อาการเดียวกันนี้ทำให้ชื่อสถานี/ชื่อเพลง/
เลข volume หายไปด้วยหลังแตะ
แก้โดย **ล้างแคชทั้งหมดตอนต้น `drawStatic()`** (`lastDigits_`, `lastSegs_`, `lastVol_`,
`lastStation_`, `lastTitle_`) แล้ว `update()` จะวาดค่าจริงทับในรอบเดียวกัน — ไม่มีช่วงที่จอว่าง
> หมายเหตุ: บั๊กนี้อยู่ใน UI เวอร์ชัน Arduino_GFX (`src/ui/legacy/`) — เวอร์ชัน LVGL ไม่มีแคช
> แบบนั้นแล้ว (object ของ LVGL ถือค่าล่าสุดเอง) แต่เก็บ legacy ไว้เป็นทาง rollback

หลักฐาน (อ่าน framebuffer กลับมาทีละ pixel ผ่าน `getFramebuffer()`):

| สถานะ | segment ที่สว่างจริงตอน 07:04:02 | ผล |
|-------|-----------------------------------|-----|
| ปิดการล้างแคช (จำลองบั๊ก) | `....... ....... ....... ....... ....... .......` | จางหมด = บั๊กเดิม |
| เปิดการล้างแคช (โค้ดใหม่) | `######. ###.... ######. .##..## ######. ##.##.#` | = 0 7 0 4 0 2 ตรงเวลาจริง |

**เสียงเงียบ — แก้แล้ว:** เชื่อม R21–R23 แล้วเสียงออกปกติ (ยืนยันโดยผู้ใช้)
รายละเอียดเดิม: `docs/phase3_audio_status.md`, `docs/phase3_measurement_guide.md`

**กดหยุด/เปลี่ยนสถานีแล้วกดเล่นใหม่ ไม่มีเสียง — แก้แล้ว (2026-10-21 22:1x):**
`startStream()` เรียก `audio.setPinout()` ทุกครั้งที่เริ่มสตรีม แต่ฟังก์ชันนี้ *สร้าง* I2S channel
จึงเรียกซ้ำไม่ได้: ครั้งที่สอง `i2s_new_channel()` ตอบ
`no available channel found` -> ไลบรารีเซ็ต `m_f_I2S_init = false` -> audio task ข้าม
`performAudioTask()` ทั้งหมด สตรีมไหลเข้า buffer อย่างเดียว (log: `played=0s (+0s)`,
`buf` ไต่ 0 -> 408 KB ไม่หยุด) จนกว่าจะรีบูต
แก้โดยเรียก `setPinout()` **ครั้งเดียวต่อบูต** (guard `audioPinsReady`) แล้วตรวจซ้ำ:
stop->play ได้ `played=+5s/5s`, buffer นิ่ง ~50-67 KB, heap 93,004 เท่าเดิม;
สลับสถานี (Talay AAC 32 kHz -> COOLfahrenheit MP3 128 kbps) ได้ `played=+5s/5s`,
`vu=0x6356 -> 0xCDCD -> 0xF3E1` (PCM ออกจริง) ไม่มี error i2s อีก
สคริปต์ตรวจซ้ำ: `tools/audio_cycle_check.py COM5 stop-play|station`

**จอสั่นซ้าย-ขวา — แก้แล้ว** (ปิดเคส 2026-10-21 19:00): ผู้ใช้ยืนยันว่า
“ภาพหน้าจอไม่มีปัญหาการสั่น” หลังแฟลชบิลด์ที่มี **DRAM bounce buffer 16 บรรทัด**
(`LCD_BOUNCE_LINES` ใน `include/app_config.h` = 15,360 B × 2) → สาเหตุคือ LCD DMA
อ่าน framebuffer 460 KB จาก PSRAM แล้วโดน Wi-Fi/TLS + audio task แย่ง bandwidth จริง

- อย่าลด `LCD_BOUNCE_LINES` ต่ำกว่า 16 หรือเพิ่ม pclk จาก 12 MHz โดยไม่ทดสอบซ้ำ
- ถ้าอาการกลับมา: ตรวจว่า heap ภายในยังเหลือ ~95 KB, แล้วลองเพิ่มเป็น 24–30 บรรทัด
- คำสั่ง `m` ทาง serial (mute แอมป์โดยสตรีมยังเล่น) ยังอยู่ ใช้แยก power/amp ออกจาก DMA ได้

## 5. หมายเหตุเชิงเทคนิคที่เพิ่งค้นพบ (2026-10-21)

- เพิ่มจอแล้วสตรีมช่วงแรกขึ้น `slow stream` เพราะ buffer ยังสะสมไม่ทัน **ไม่ใช่บั๊ก**
  หลัง ~40 วินาทีจะกลับมา `+5s/5s` และ buffer ไต่ถึง 100–480 KB
- การวาดจอใช้เวลา **11–19 ms ต่อการอัปเดต 1 วินาที** → ไม่กระทบเสียง
- logic restart สตรีมแก้แล้ว: เดิมรีสตาร์ททันทีเมื่อ `!isRunning()` ทำให้ buffer ที่กำลังไต่ถูกทิ้ง
  ตอนนี้รอ **15 วินาที** ก่อนลองใหม่ (ตัวแปร `deadSince`)
- `UI_DYNAMIC` (ใน `src/stream_test/main.cpp`) ตั้ง 0 ได้เพื่อทดสอบแบบไม่วาดจอซ้ำ
- heap หลังเปิดจอ+สตรีม = **~126 KB** ก่อนเพิ่ม bounce buffer, **96,772 B** หลังเพิ่ม
  (PSRAM 8 MB แทบไม่ถูกใช้)
- การวาดจอหลังเปิด bounce buffer วัดได้ **13–23 ms/ครั้ง** (ก่อนหน้า 11–19 ms) → ยังไม่กระทบเสียง
- หน้าจอ retro ใหม่วัดได้ **18–33 ms/ครั้ง** (ตลับเทป + ตัวเลข 7-segment + polling ทัช)
  heap **95,740 B** หลังเพิ่ม GT911 polling + volume rocker → ยังไม่กระทบเสียง
- **LVGL (2026-10-22)**: pass ที่ต้องวาดใหม่ทั้งหน้า (เปลี่ยนจอ/เปิด LIST) = **110–145 ms**
  (LVGL software render 230k px ลง draw buffer ใน PSRAM ~0.5 µs/px) แต่ pass ปกติ
  (อัปเดตนาฬิกา/มิตเตอร์ 1 Hz) = **2–36 ms**; การ flush ไม่เคยเกิน 3 ms — ระหว่างสไปก์
  เสียงไม่สะดุด (buffer ไต่ถึง 100+ KB) ส่วนที่กินเวลาคือการ render ไม่ใช่การส่งภาพออกจอ
- วัดปริมาณการวาดต่อวินาทีได้ด้วยการ log ใน `flushCb` (ปกติ ~43k px/s = นาฬิกา 302×76,
  มิตเตอร์ 442×28, แผ่นเสียง 40×40, ข้อความบรรทัดล่าง) — ใช้โค้ดชั่วคราวแล้วถอดออก
- ทดสอบปุ่มโดยไม่มีนิ้ว: ใส่ `lvport::debugTap(x,y)` ชั่วคราวให้ `indevRead()` คืน
  pressed 90 ms ที่พิกัดที่สั่งผ่าน serial (`d x y`) → ยืนยัน hit area/การเปลี่ยนหน้าจอได้
  ครบทุกปุ่ม **แต่ไม่ได้ยืนยันตัว GT911 จริง** (ต้องรอผู้ใช้แตะ) — โค้ดนี้ถอดออกจาก
  เฟิร์มแวร์ที่แฟลชแล้ว
- **เวลาบนจอ = เวลาจริงเขตกรุงเทพ (UTC+7)**: ตั้งด้วย `configTzTime("ICT-7", ...)`
  ใน `src/stream_test/main.cpp` แล้วดึงด้วย `localtime_r()` ทุกวินาที (log: `time=19:00:38`)
  ก่อน NTP ซิงก์ จะเห็นเฉพาะ segment จาง ไม่มีเลขมั่ว
- ไอคอน 10 ตัวในแฟลชใช้แค่ **960 B** (bitmap 1-bit 24×24) ไม่กิน RAM
- log ล่าสุด: `jitter_serial.log` (บูต + สตรีม + heap + ui draw time)
- ตรวจหน้าจอโดยไม่ต้องใช้ตา: `Arduino_RGB_Display::getFramebuffer()` คืน pointer ของ
  framebuffer 480×480×2 ใน PSRAM อ่านค่าจาก PSRAM ได้เลย วิธีนี้ใช้ยืนยันบั๊กการวาดจอ
  (เทียบสี segment ที่ควรเป็น `AMBER` กับ `AMBER_DIM`, หรือพิมพ์แถบข้อความเป็น ASCII art)
  — ใช้เฉพาะตอนทดสอบแล้วลบโค้ดออก (ไม่เหลือในเฟิร์มแวร์ที่แฟลช)

## 6. งานถัดไปตามลำดับ

1. ยืนยันด้วยนิ้วจริงบนปุ่มแถวล่าง (PREV / PLAY / NEXT / LIST / SETUP) + หน้า LIST + DONE
   ว่า hit-test ตรงกับปุ่มที่วาด **และดูหน้าตาจอ LVGL ด้วยตา** (สี/ฟอนต์/นาฬิกา ควรเหมือนเดิมเป๊ะ)
   — hit area ทดสอบแล้วด้วยพิกัดฉีด แต่เส้นทาง GT911 → indev ยังไม่ได้แตะจริง
2. ถ้าต้องลด pclk: แก้ `LCD_PCLK_HZ` ใน `include/app_config.h` → 10000000/9000000 แล้วทดสอบซ้ำ
3. หน้าเว็บยังไม่มี: แก้ไข/สลับลำดับสถานี, เลือกสถานีที่เล่นอยู่ (ตอนนี้เลือกได้ที่จอเท่านั้น)
4. ถ้าอยากให้มือถือเด้งหน้าตั้งค่าอัตโนมัติ (captive portal) ต้องเพิ่ม `DNSServer`
   ชี้ทุกโดเมนมาที่ `192.168.4.1`
5. ~~ย้าย UI ไป LVGL~~ (เสร็จ 2026-10-22 ดู §1.2) — เหลือแตก `test/` เป็น `display_test` /
   `touch_test` / `audio_test` ถ้าต้องการตาม `promp.txt` §8
6. Phase 5: stability 60 นาที, ทดสอบ Wi-Fi หลุด, URL ผิด

## 7. ไฟล์ log ที่เก็บไว้

| ไฟล์ | เนื้อหา |
|------|---------|
| `jitter_serial.log` | บูต + สตรีมหลังเปิด bounce buffer (heap 96,772 B, ui 13–23 ms) |
| `stream_serial.log` | สตรีม + UI ก่อนแก้ jitter (played/buffer/ui draw) |
| `ui_static.log` | ทดสอบตอน buffer ไต่ถึง 480 KB |
| `audio_serial.log` | โทนทดสอบ I²S (440 Hz / 1 kHz / sweep) |
| `serial2.log`, `serial.log` | Phase 2 display/touch + touch coordinate |

## 8. คำสั่งที่ใช้บ่อย

```bash
python -m platformio run -e stream_test -t upload   # เฟิร์มแวร์สตรีม + จอ
python -m platformio run -e audio_test -t upload    # โทนทดสอบ I²S
python -m platformio run -t upload                  # display/touch bring-up
```

อ่าน log: เปิด COM5 115200 (ตัวบันทึกที่รันในเซสชันนี้ถูกปิดแล้วเพื่อคืนพอร์ต)
