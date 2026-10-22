# Project Knowledge — Guition ESP32-S3-4848S040 Internet Radio

## Resume here

`docs/status.md` is the single source of truth for "what is flashed on the board right now,
what is verified, and what to do next". Keep it updated at the end of every working session.

Current firmware on COM5: `stream_test` with the retro hi-fi status screen (Pixelarticons
icons, Bangkok wall clock on a 7-segment display, control strip PREV/PLAY/NEXT/LIST/SETUP,
touch volume rocker), the RGB DRAM bounce buffer, and the AP setup portal for Wi-Fi +
stations. Audio works after the R21–R23 solder link and the display
jitter is fixed — both confirmed by the user on 2026-10-21, so the bounce buffer and pclk
table are now "do not regress" settings rather than open questions.

The firmware is split into modules (2026-10-22): `src/stream_test/main.cpp` only wires
`src/storage` + `src/radio` + `src/audio` + `src/display` + `src/ui` + `src/web` together.
The UI runs on **LVGL 8.3.11** (`src/ui/lv_ui.*` + `src/display/lv_port.*`, `include/lv_conf.h`)
since 2026-10-22; the old Arduino_GFX faceplate is kept, uncompiled, in `src/ui/legacy/`.

## What this repo is

Firmware project for an **Internet Radio Player** on the Guition **ESP32-S3-4848S040**
(4.0" 480×480 touch display). Audio is streamed from radio stream URLs and sent to an
**external** audio system. Spotify / Spotify Connect is explicitly out of scope.

**Current state: Phase 4 firmware split into modules, UI on LVGL** — `stream_test`
streams real stations on the board; the UI, audio, radio, storage, web and display code
each live in their own `src/` folder. Remaining work is split-out `test/` envs, the extra
web-page features, and the stability run (see `docs/status.md`).

Still authoritative/reference material:

- `promp.txt` — the authoritative project spec (Thai). Read it before any work.
- `docs/phase1_hardware_audit.md` — sourced GPIO table, audio findings, open questions.
- `ESP32-4848S040 Specifications-EN.pdf` — vendor datasheet (JCZN / Guition).
- `Getting started 4.0 Inch.pdf` — vendor Arduino IDE setup guide.

Per the spec (§9), work starts at **Phase 1: Hardware Audit**, then Minimal Compile,
Audio Test, Full Firmware, Stability Test. Do not skip ahead to full firmware.

## Hardware (datasheet + cross-checked community docs)

| Item | Value |
| --- | --- |
| MCU | ESP32-S3-WROOM-1, dual-core LX7 @240 MHz, 512 KB SRAM, 384 KB ROM |
| Flash | 16 MB |
| PSRAM | 8 MB **OPI** (octal) PSRAM |
| Display | 4.0" TFT, 480×480, RGB565 (65K), driver **ST7701** |
| Touch | Capacitive, reported as **GT911** — verify against the actual PCB revision |
| Power | 5 V, ~260 mA |
| Extras | TF card slot, IO header, optional 1-way (`_Y_1`) or 3-way (`_Y_3`) relay |
| Audio | NS4168 mono class-D amp + 1.25 mm speaker connector, per community reports; **not in vendor datasheet**, not confirmed on this unit |

## Confirmed pinout (community sources, agree across 3+ projects)

Full sourced table: `docs/phase1_hardware_audit.md`.

- **LCD ST7701**: CS 39, SCK 48, MOSI 47, DE 18, VSYNC 17, HSYNC 16, PCLK 21, BL 38.
  RGB: R `11,12,13,14,0`; G `8,20,3,46,9,10`; B `4,5,6,7,15`.
- **Touch GT911**: I²C SDA 19, SCL 45, addr 0x5D. No GPIO for INT/RST.
- **microSD** (shared SPI): CS 42, MISO 41, MOSI 47, CLK 48.
- **Relays** (active low): L1 40, L2 2, L3 1.
- **I²S to NS4168**: BCLK 1, LRCLK 2, SDATA 40 — the *same* pins as the relays.
- **Unusable**: 33–37 OPI PSRAM, 26–32 flash. Nearly no free GPIO; 43/44 are UART0/CH340.

## Audio constraints (important)

- **User's board is the no-relay variant**, has NS4168 + speaker connected, and is still silent
  after a verified I²S tone test (see `docs/phase3_audio_status.md`).
- Pins are right: vendor pin table says IO1 = I2S-BCLK, IO2 = I2S-LRCLK, IO40 = I2S-DIN.
- Confirmed on the actual PCB: the 0 Ω parts sit at **R25/R26/R27** (ULN2003 relay-driver
  side) and **R21/R22/R23 are empty**, so GPIO1/2/40 never reach the NS4168. Audio needs
  0 Ω added at R21/R22/R23 (R25–R27 can stay; there are no relays to load the driver).
  Until that link exists, any "no sound" result is a hardware gap, not a software bug.
- NS4168 is a **mono BTL speaker amp** (CTRL pin selects L or R channel), not a line output.
  Never feed its outputs to an AUX/RCA input.
- For stereo line-out, use an external I²S DAC (PCM5102A) on the same BCLK/LRCLK/DIN — the
  same three pins, tapped at the L1/L2/L3 terminal, no soldering on the board.

## Commands

PlatformIO Core is installed for the user via pip, so use `python -m platformio` (there is no
`pio` executable on PATH). Verified working on 2026-10-21:

```bash
python -m platformio run              # build  -> SUCCESS
python -m platformio run -t upload    # flash to COM5 -> SUCCESS
```

```bash
# Phase 3 audio path test (separate firmware, no Wi-Fi)
python -m platformio run -e audio_test
python -m platformio run -e audio_test -t upload

python -m platformio device monitor   # serial log (115200) — not used; a small pyserial
                                       # snippet was used instead to capture logs to a file
```

Three envs live in `platformio.ini` (`esp32-s3-4848s040`, `audio_test`, `stream_test`) and each
sketch dir defines its own `setup()/loop()`. Every env therefore filters sources, and the
modules only belong to `stream_test`:

```
esp32-s3-4848s040 : +<*> -<audio_test/> -<stream_test/> -<audio/> -<display/> -<radio/> -<storage/> -<ui/> -<web/>
audio_test        : +<audio_test/>
stream_test       : +<stream_test/> +<audio/> +<display/> +<radio/> +<storage/> +<ui/> +<web/>
```

Adding another sketch dir or module dir without updating the main env's filter breaks the
build (`multiple definition of setup()` once the env also sees the file, or
`Audio.h: No such file` when it does not have that library). Includes are written as
`"radio/station_manager.h"` — PlatformIO puts `src/` on the include path for these builds.

Board is on **COM5**, UART0 via CH340 at 115200. `test/` and `pio check` do not exist yet.
`python` on this machine is 3.14 without PlatformIO; run `C:/Users/Mrk/.platformio/penv/Scripts/python.exe -m platformio`.
Do not record a command as "working" until it has actually been run in this repo.

## Current layout (spec §8, adapted)

```
platformio.ini
include/{app_config.h,board_pins.h,secrets.h,ui_icons.h,lv_conf.h}
src/main.cpp                      Phase 2 display/touch bring-up (its own env)
src/stream_test/main.cpp          wiring only: boot order, serial keys, panel 1 Hz, stats
src/storage/settings_store        NVS (radio: wifi/stations; touch3: calibration)
src/radio/station_manager         station list + active + next/prev
src/radio/network_link            Wi-Fi STA, IP string, NTP/Bangkok clock
src/radio/radio_player            play/stop/next/prev + 15 s restart watchdog
src/audio/audio_engine            ESP32-audioI2S wrapper (setPinout once, volume, ICY)
src/display/display_panel         ST7701 + backlight + DRAM bounce buffer
src/display/touch_gt911           polled GT911 driver (raw coordinates)
src/display/touch_calibration     raw -> screen (3x3 grid + affine), NVS backed
src/display/lv_port               LVGL display (480x40 PSRAM strip) + GT911 pointer indev
src/ui/lv_ui                      retro faceplate in LVGL (4 screens + action queue)
src/ui/legacy/ui_status           previous Arduino_GFX faceplate (kept for rollback)
src/web/web_setup                 AP portal (HTML page in PROGMEM)
src/audio_test/main.cpp           Phase 3 I2S tone test (its own env)
```

No `data/`-served assets yet: the setup page is still embedded in `web_setup.cpp`. Keep
new code in the module it belongs to — a single-file sketch is not acceptable.

`build_src_filter` for `stream_test` also excludes `-<ui/legacy/>`, so the old faceplate
stays in the tree without being compiled. To roll back, drop that exclusion, restore the
`UiStatus` calls in `main.cpp` and remove `+<display/lv_port>` usage — the LVGL library
dependency in `platformio.ini` is the only other thing to take out.

## Stack decisions

- PlatformIO + Arduino framework (ESP32-S3).
- Arduino_GFX 1.6.9 for the ST7701 panel + LVGL 8.3.11 for UI (both pinned in `lib_deps`;
  LVGL 8.3 is the last line that still has the `lv_disp_draw_buf` / draw-event API this port uses).
- ESP32-audioI2S for radio (verify release supports ESP32-S3 + chosen Arduino-ESP32).
- WiFi + WebServer (Arduino-ESP32), Preferences for persistence, ArduinoJson if needed.
- Web UI is hand-written HTML/CSS/JS served from the device — **no CDN frameworks**.

## Hard rules (from spec §3, §10)

1. **Never invent GPIO numbers.** Confirm LCD, touch, backlight, I²S pins from vendor
   docs/schematic or from the user (photo of the board / schematic).
2. **Never invent station URLs.** Unverified streams are stored as `UNVERIFIED` and
   must not be presented as playable.
3. **Never claim a test passed unless it was actually run.** Separate "verified" from
   "not yet tested" in all reports.
4. Do not build fake "Playing" states — UI status must reflect real audio state.
5. No hardware rewiring/soldering instructions until the audio circuit is confirmed.
6. If hardware info is missing, stop only the affected step, list what is missing, and
   ask the user. Do not fill gaps with guesses.

## Gotchas

- **Pinout is image-only in the PDF.** `pdftotext` on the "Interface Description" page
  returns no table — the pin/GPIO map is a picture. Pin values above come from community
  projects for this exact model; never infer from a different 4848S040 variant.
- **Audio path is conditional.** An onboard NS4168 is reported, but it is wired to the
  relay pins unless three 0 Ω resistors are moved. Treat audio as blocked on user
  confirmation; do not assume it works out of the box.
- **LCD init quirks:** use the ESPHome/community init sequence (`0xFF 77 01 00 00 10`,
  `0xCD 0x00`) and the red/blue pin order above; other orders produce wrong colors or
  4-bit red/blue.
- **RGB framebuffer lives in PSRAM (460 KB).** Without a bounce buffer the LCD DMA reads
  PSRAM directly, and Wi-Fi/TLS + audio decoding + UI writes starve it, which shows up as
  text jittering left/right while a stream plays. `src/display/display_panel.cpp` now passes
  `LCD_BOUNCE_PX` (16 lines = 2 × 15,360 B DRAM) as `bounce_buffer_size_px`. Cost: ~31 KB of
  internal RAM (free heap dropped 126 KB → 96,772 B). If jitter returns, raise the line
  count, or lower pclk from 12 MHz to 10/9 MHz. `m` on the serial console mutes the amp
  while the stream keeps decoding — use it to tell a power/amp problem from DMA starvation.
  **Confirmed fixed** by the user after the 16-line bounce buffer: the picture is steady
  while streaming, so this is the working configuration — do not shrink the bounce buffer
  or raise pclk without re-testing on the panel.
- **Wall clock needs the POSIX TZ, not an offset.** `configTzTime("ICT-7", ...)` in
  `src/stream_test/main.cpp` sets Asia/Bangkok (UTC+7, no DST); `localtime_r()` then returns
  Bangkok time directly. Until NTP answers (`time() < 1.6e9`), the UI draws only the ghost
  segments, never a fake time.
- **Touch is polled in `stream_test`** through `src/display/touch_gt911.*` (same registers and
  little-endian record described below). 25 ms poll interval, no INT pin, fed to LVGL's
  pointer indev by `src/display/lv_port.cpp`. LVGL does the hit-testing itself now; the UI
  only queues an action (`LvUi::takeAction()`) and `main.cpp` calls the modules, so only
  `main.cpp` touches `Audio`. Volume rocker repeats via `LV_EVENT_LONG_PRESSED_REPEAT`
  (300 ms). Hit areas follow the drawing, so a bad GT911 axis mapping now shows up as taps
  landing on the neighbouring button — check the axis byte order first.
- **"Touch dead" can be the coordinate mapper, not the controller.** 2026-10-22: every tap
  put the red marker at the top-left corner and no button fired, yet the log showed correct
  raw values (`touch : down x=0 y=0 (raw 156,410) bytes 9C 00 9A 01 1F 00 00`).
  `TouchCalibration::map()` fell through to `data_.aff[]` when no grid was stored
  (`have_ == false`), and that struct is zero-initialised — so mapped = (0,0) always. The
  identity default was lost in the modular split. Fix: return the raw pixel unchanged
  (clamped 0..479) when no calibration is loaded — confirmed by the user on the panel
  (taps land where the finger is, no 3×3 run needed). Before blaming I²C/hardware, read the
  `raw=` part of that log line: raw correct + mapped 0 means the mapper, and the GT911 was
  proven healthy the whole time (status `0x80`, config block 0x8047–0x80FE readable,
  `ok/fail = 99400/0`).
- **LVGL draw callbacks must use absolute screen coordinates.** 2026-10-22: the 7-segment
  clock vanished after the port because `clockFace()` drew at `(digitX(i), 0)` — relative to
  the object — while `fillRect()` hands `lv_draw_rect()` a screen-space area. Everything
  landed outside the object box and LVGL clipped it away silently: no error, the callback
  still ran every second (`ui` pass cost was normal). Any `LV_EVENT_DRAW_MAIN` callback here
  gets `co.x1/co.y1` from `lv_obj_get_coords()` and must add them to every draw primitive.
- **Verify the panel from the framebuffer, not by eye.** `Arduino_RGB_Display::getFramebuffer()`
  returns the PSRAM frame; dumping it as an 8-pixel ASCII map over serial shows whether a
  region is background or drawn, and it caught this bug (clock box was `.` everywhere).
  Remember to force `lv_obj_invalidate(lv_scr_act())` + `lv_refr_now(NULL)` first, otherwise
  the dump shows stale pixels from the last partial refresh.
- **Every `LvUi::Action` needs a `case` in `pumpActions()`.** `ACT_LIST_CLOSE` was missing
  during the port and the CLOSE button silently did nothing, so the panel stayed on the list
  and the main-screen buttons looked dead — the enum compiles fine without it. When adding an
  action, add the branch in the same change.
- **Status UI style: retro hi-fi faceplate** (`src/ui/lv_ui.*`, LVGL since 2026-10-22).
  Burnt-orange bezel, cream silk-screen text, amber seven-segment wall clock (HH:MM:SS,
  colons centred in the pair gap) with dim "ghost" segments, segmented buffer meter,
  spinning station disc, volume rocker and a five-button control strip. Same palette,
  coordinates and glyphs as the Arduino_GFX version: text uses GFX's own 5×7 table
  (`font/glcdfont.h`, 6×8 cell), and the clock/meter/disc/icons are `LV_EVENT_DRAW_MAIN`
  callbacks that draw with `lv_draw_rect` / scanline fills. (A CRT scanline overlay was
  tried and dropped: the once-per-second redraws erase it piecemeal.)
  Steady-state 1 Hz update costs 2–36 ms per pass; a full screen change (LIST, SETUP) costs
  110–145 ms because LVGL software-renders 230k px into the PSRAM strip. Audio keeps
  playing through that spike (input buffer stays full), but do not add more full-screen
  redraws per second.
- **LVGL port shape: 480×40 strip in PSRAM, no full-screen draw buffer.** `flush_cb` hands
  each area to `draw16bitRGBBitmap()`, which keeps the PSRAM cache write-back the RGB DMA
  needs and reuses the DRAM bounce buffer; writing into the framebuffer directly leaves
  lines in cache and tears. LVGL's own heap is PSRAM (`LV_MEM_CUSTOM` → `heap_caps_malloc`),
  which is why internal RAM stays at 21.7 % with the UI running.
  While the touch calibration grid is active the sketch reads the GT911 raw itself, so the
  indev must be switched off (`lvport::touchEnabled(false)`) — reading clears the
  controller's ready flag and both readers would starve each other.
- **Icon pipeline (no Pillow on this machine).** Icons come from Pixelarticons (MIT,
  pixel art 24×24 SVG). `tools/svg2header.py` is a pure-python M/L/H/V path parser +
  scanline rasteriser that emits `include/ui_icons.h` (`static const uint32_t ICON_X[24]`,
  bit set = pixel on, 96 B/icon). `drawIcon()` blits them as horizontal runs at any integer
  scale, so one icon recolours per state (e.g. the signal icon turns amber/red with RSSI).
  `tools/png2header.py` is the alternative PNG→RGB565 path (ex-`currentColor`, supports
  palette/low-bit-depth PNG) if a raster set is ever needed; `assets/icons/*.png` are
  Twemoji (CC-BY 4.0) and are currently unused.
  Regenerate after editing assets, never hand-edit `ui_icons.h`.
- **Text overflow is prevented by `fit()`** (truncates with `..` per field width) and
  `clearText()` before rewriting a cell — without the clear, shorter values leave
  fragments of the previous text on screen.
- **Touch controller varies by batch** (GT911 vs FT6336U); verify with an I²C scan.
  On this board only 0x5D answered and product-id reads `911`.
- **`audio.setPinout()` is a once-per-boot call.** It *creates* the I2S channel
  (`i2s_new_channel`), so a second call fails with `no available channel found`, the library
  stores `m_f_I2S_init = false`, and its audio task then skips `performAudioTask()` entirely:
  the stream keeps filling the input buffer (`played` frozen at 0s, buffer growing to ~400 KB,
  silence) until a reboot. This is what made "stop then play" and "change station" lose the
  sound. `startStream()` guards it with `audioPinsReady`; never move it back into the
  per-stream path. Recheck with `tools/audio_cycle_check.py COM5 stop-play|station`
  (expect `played=+5s/5s` and a steady buffer after the cycle).
- **Legacy GFX UI (only if you roll back to `src/ui/legacy/`): `drawStatic()` wipes the
  screen, so it must also wipe the UI's "last drawn" caches.**
  `UiStatus::update()` only redraws a field whose cached value changed (`lastDigits_`,
  `lastSegs_`, `lastVol_`, `lastStation_`, `lastTitle_`) - a saving that made the 1 Hz update
  cost ~7-14 ms instead of a full frame. A `drawStatic()` repaint (boot, list close, and every
  touch release, which repaints to erase the red touch marker) draws placeholders and ghost
  segments, so those caches must be invalidated at the top of `drawStatic()` or the values
  stay missing until they happen to change - that is what made HH:MM vanish on tap and come
  back one digit per minute. Any new cached field needs the same treatment.
- **The panel can be inspected without eyes.** `Arduino_RGB_Display::getFramebuffer()` returns
  the 480x480x2 PSRAM framebuffer, readable from the sketch: compare a pixel with the palette
  constants (`AMBER` lit vs `AMBER_DIM` ghost segment) or print a text row as ASCII art. That
  is how the repaint bug above was proven (A/B: ghosts vs the exact segment pattern of
  `07:04:02`). Use it for temporary instrumentation, then remove the code before flashing the
  final build.
- **VU level is only meaningful for MP3 here.** The AAC station reports a constant
  `vu=0x0101` even while it decodes and plays; use the play clock and input-buffer drain as
  the "audio is really running" indicators for AAC.
- **Stations and Wi-Fi live in NVS** (`src/storage/settings_store.*`, namespace `radio`):
  SSID, password, up to 12 stations (name 40 B / URL 160 B) and the active index;
  `StationManager` holds the RAM copy and persists through the store. `secrets.h` is
  only a first-boot migration source. The AP portal (`src/web/web_setup.*`) serves the edit
  page at 192.168.4.1 while the player is stopped; serial `w` enters it and `r` reboots out
  of it (the touch DONE button does the same).
- **Serial keys** (`src/stream_test/main.cpp`): `m` mute, `v`/`+` up, `V`/`-` down,
  `n`/`b` next/previous station, `s` play-stop, `t` dump GT911, `k` touch calibration,
  `w` setup portal, `r` reboot. The `n`/`b`/`s` keys call the same functions as the touch
  buttons, which is how the audio restart bug above could be reproduced from a terminal.
- **Portal runs `WIFI_AP_STA`, not `AP`.** Scanning needs the STA interface, and in AP-only
  mode a scan can drop the phone that is editing the page. `begin()` also `WiFi.begin()`s
the saved network (non-blocking) so the page answers on the LAN IP too - that is what made
  end-to-end testing possible from a PC without leaving the home network.
- **Station list on the panel pages by 7 rows.** `MAX_LIST_ROWS` rows fit under the title;
  with more stations the left/right triangles on the bottom strip page, CLOSE sits between
  them. `listSel_` is the absolute index (`listTop_ + row`), not the row on screen.
- **Closing the serial port can power-reset the board.** The CH340 auto-reset circuit fires
  when pyserial drops DTR/RTS, so a script that enters setup mode and then closes COM5 makes
  the player reboot a second later - it looks like a firmware crash (`rst:0x10
  RTCWDT_RTC_RST`) but the firmware never faulted. Keep the port open for the whole test and
  reboot explicitly with `r`; check the reset reason in the boot banner before hunting a bug.
- **Portal HTTP can be tested without a phone**: `tools/portal_check.py` (pyserial + urllib)
  enters setup over serial, then hits `/`, `/api/status`, `/api/stations`, `/api/scan`,
  add/delete, and reboots. Pass the board's LAN IP if DHCP moved it.
- **GT911 point record: little-endian, no track-id byte** (verified 2026-10-21 by dumping
  the raw bytes of five known taps). The record at `0x8150` is
  `[xL xH yL yH sizeL sizeH flags]` and the values are already screen pixels (tap at 460,20
  → `C4 01 2A 00` = 452,42). The older "high-byte-first" parse in
  `docs/phase2_bringup.md` §4 starts one byte late, so x and y are mixed: taps come out
  scattered and non-monotonic and every mapping looks distorted. With the correct parse the
  mapping is identity — no calibration matrix needed (`k` still runs a 3×3 grid fit).
- **`st7701_type9_init_operations` from Arduino_GFX 1.6.9 works** with the RGB pin order in
  `board_pins.h` — panel image was confirmed correct by the user (no invert/rotation needed).
- ST7701 RGB panel with `pclk=12 MHz` and the porch values in `src/main.cpp` produced a
  stable, correctly coloured image; do not change them without re-verifying on hardware.
- **Native USB is unavailable** — the USB-C port goes through a CH340 to UART0 (GPIO43/44).
- **OPI PSRAM** consumes GPIO 33–37 on ESP32-S3 (board-independent); do not assign
  peripheral pins there.
- PSRAM is required for LVGL draw buffers/framebuffers — size buffers from real color
  format × buffer count, don't allocate full-screen by default; watch internal heap too.
- LVGL is not thread-safe: only one task may touch LVGL objects; keep audio decoding off
  the LVGL event loop and avoid blocking `delay()` in UI/network paths.
- Thai language support: UI must render Thai script with an appropriate font; deliverable
  README is Thai.
- Web config: escape/validate all user input, cap request and metadata sizes, never echo
  Wi-Fi passwords in the page or logs.
- `promp.txt` has CRLF line endings — diff/edits may show the whole file changed.
- Deliverables require a GPIO table with cited sources, a wiring diagram (only once
  verified), and a Thai README. The audit supplies the GPIO table; the wiring diagram
  and Thai README do not exist yet.
