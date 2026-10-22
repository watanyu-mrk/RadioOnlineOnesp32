# Phase 3 — Audio status (ผลทดสอบจริง)

อัปเดต: 2026-10-21 · บอร์ดบน `COM29`

## 1. ข้อมูลบอร์ดที่ผู้ใช้ยืนยัน (ต่างจากสมมติฐานใน Phase 1)

- มีชิป **NS4168** และ **เสียบลำโพง** อยู่จริง
- **บอร์ดรุ่นไม่มีรีเลย์** → GPIO1/2/40 ไม่ได้ขับรีเลย์
- ทดสอบโทน 440 Hz / 1 kHz / sweep แล้ว **ไม่มีเสียงออกลำโพง**

## 2. Pin I²S ยืนยันจากแหล่งอิสระ

ตาราง pin ของบอร์ดซีรีส์นี้ (Tasmota discussion #20527, คัดจากคู่มือผู้ผลิต):

```
IO1  I2S-BCLK / L3
IO2  I2S-LRCLK / L2
IO40 I2S-DIN / L1
```

คู่มือผู้ผลิต (SpotPear wiki + คู่มือจีน) ระบุตรงกันว่า:

> "The 4-inch screen relay and I2S audio share IO ports, namely IO1, IO2, IO40.
> If audio is required, the 0 ohm resistors R25, R26, R27 should be moved to R21, R22, R23."

ดังนั้น **หมายเลข GPIO ที่ใช้ในโค้ดถูกต้องแล้ว** — เส้นทางที่ขาดคือฝั่งฮาร์ดแวร์ (แผ่น resistor
ระหว่าง GPIO กับขา DIN ของ NS4168)

## 3. ซอฟต์แวร์เสียง — ผ่านจริง

### 3.1 I²S tone (`src/audio_test`, env `audio_test`)

```
I2S       : BCLK=GPIO1  LRCLK=GPIO2  DOUT=GPIO40  (no MCLK)
format    : 44100 Hz, 16-bit, stereo (identical L/R)
I2S       : ready
PHASE tone 440 Hz (A4) ...
```

วน 4 รอบใน ~60 วินาที, heap 346,308 คงที่, ไม่มี panic

### 3.2 สตรีมจริง (`src/stream_test`, env `stream_test`)

Wi-Fi `spd_wifi` + URL ที่ผู้ใช้ให้มา:

```
wifi      : connected ip=192.168.8.59 rssi=-63 dBm
event     : info: SSL has been established in 710 ms
event     : chunked data transfer
event     : station_name: COOLfahrenheit | Easy Listening
event     : streamtitle: Music Freeze Your Mind
event     : info: MP3Decoder has been initialized
event     : info: MPEG-1 Layer III | 44100 Hz | 2 ch | 16 bit
event     : bitrate (b/s): 128000
state     : running=1 played=865s (+5s/5s) bitrate=128000 codec=MP3 44100Hz/2ch buf≈256–580KB
```

- เล่นต่อเนื่อง **865 วินาที (14.4 นาที)** โดยไม่หยุด, heap 132,780 คงที่ทั้งช่วง,
  ไม่มี `Guru`/`panic`/`assert` ใน log (buffer ขยับ 250–580 KB ตาม network jitter ปกติ)
- ยืนยันว่า **URL นี้เป็นสตรีมเสียงจริง** (ICY metadata + MP3 syncword) ไม่ใช่หน้าเว็บ

## 4. สรุป: ชิ้นส่วนที่ยังขาด

| ชั้น | สถานะ |
|------|--------|
| Wi-Fi / HTTPS | ✅ ทดสอบแล้ว |
| ถอดรหัส MP3 | ✅ ทดสอบแล้ว (128 kbps, 44.1 kHz, 2ch) |
| ส่ง PCM เข้า I²S (GPIO1/2/40) | ✅ ไม่มี error, DMA ทำงาน |
| NS4168 รับสัญญาณ | ❌ ไม่มีเสียง — สงสัยเส้นทาง resistor R21–R23 ยังไม่ต่อ |
| ลำโพง | ❌ ยังไม่เคยมีเสียงออก |

## 4.1 สาเหตุที่ยืนยันแล้ว (ตรวจบอร์ดจริง 2026-10-21)

- ตัวต้านทาน 0 Ω **บัดกรีอยู่แถว R25, R26, R27** (ฝั่ง ULN2003 / ไดรเวอร์รีเลย์)
- **R21, R22, R23 ว่าง** (ฝั่งที่ต่อไปยังขา 2/3/4 ของ NS4168)
- บอร์ดมี U2 (SOIC-16, ไดรเวอร์รีเลย์) และ U4 (SOP-8, NS4168) อยู่จริง แม้ไม่มีรีเลย์บัดกรี

⇒ เส้น I²S จาก GPIO1/2/40 ยังไม่ถึงแอมป์ จึงไม่มีเสียงทั้งโทนและสตรีม

**วิธีแก้ที่เสนอ:** เชื่อม 0 Ω (หรือ solder bridge) ที่ **R21, R22, R23**
— ไม่จำเป็นต้องถอด R25–R27 เพราะรีเลย์ไม่มีอยู่จริง (ULN2003 แค่รับสัญญาณโดยไม่มีโหลด)

## 5. ทางเลือกที่เหลือ (ยังไม่ดำเนินการ)

1. **บัดกรี 0 Ω / solder bridge ที่ R21–R23** — ตรวจแล้วว่า R25–R27 มีอยู่และ R21–R23 ว่าง
   (ผู้ใช้ต้องอนุมัติและลงมือ เพราะเป็นการแก้ฮาร์ดแวร์) จากนั้นแฟลช `audio_test` เพื่อฟังโทน
2. **ใช้ I²S DAC ภายนอก (PCM5102A)** ต่อกับ GPIO1/2/40 ที่ขั้วต่อ L1/L2/L3 + 3V3/GND
   → ได้ line-out สเตอริโอจริง ไม่ต้องบัดกรีบนบอร์ด (ต้องยืนยันว่ามีขั้วต่อนี้)
3. ตรวจสอบด้วยมัลติมิเตอร์: continuity จากขา DIN ของ NS4168 ไปยัง GPIO40

## 5.1 สถานะเฟิร์มแวร์บนบอร์ด (2026-10-21)

แฟลช `stream_test` ค้างไว้บน COM29 แล้ว (`Hash of data verified`, SUCCESS 51 s)
เพื่อให้หลังบัดกรี R21–R23 เสร็จเพียง **ถอด/เสียบไฟใหม่** ก็มีเสียงทันที ไม่ต้องแฟลชซ้ำ

> ก่อนบัดกรี: ถอด USB/ไฟเลี้ยงออกทุกครั้ง

## 6. สิ่งที่ห้ามสรุป

- ห้ามรายงานว่า "เสียงผ่าน" จนกว่าจะได้ยินจริง
- ห้ามแนะนำให้บัดกรีจนกว่าจะเห็นสถานะ R21–R23 บนบอร์ดจริง
