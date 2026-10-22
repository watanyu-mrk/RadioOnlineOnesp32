#!/usr/bin/env python3
"""Convert PNG icons into RGB565 C headers for the 480x480 ST7701 panel.

No third-party modules: PNG chunks are decoded with zlib + struct only, so this runs on
the same python that PlatformIO ships (C:/Users/Mrk/.platformio/penv/Scripts/python.exe).

Each source icon is
  * decoded (palette / truecolour / truecolour+alpha, bit depth 1..8),
  * box-filtered down to the requested display size (alpha-weighted),
  * posterised onto a small retro palette (dark -> mid -> amber -> cream),
  * composited over the UI background colour so the panel can blit it opaque,
  * written out as `static const uint16_t` arrays in big-endian RGB565.

Usage:
  python tools/png2header.py --out include/ui_icons.h \
      --size 44 --bg 0x1082 \
      radio=assets/icons/1f4fb.png note=assets/icons/1f3b5.png
"""

import argparse
import struct
import zlib

# Retro phosphor ramp, dark to light: used for every icon so the screen shares one palette.
DEFAULT_RAMP = ["#2a1038", "#7a2d18", "#e0862c", "#ffd9a0"]


def read_png(path):
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("%s: not a PNG" % path)

    pos = 8
    width = height = depth = ctype = None
    palette = []
    trns = b""
    idat = bytearray()

    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        ctag = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length

        if ctag == b"IHDR":
            width, height, depth, ctype, _comp, _filt, interlace = struct.unpack(
                ">IIBBBBB", body)
            if interlace:
                raise SystemExit("%s: interlaced PNG is not supported" % path)
        elif ctag == b"PLTE":
            palette = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif ctag == b"tRNS":
            trns = body
        elif ctag == b"IDAT":
            idat += body
        elif ctag == b"IEND":
            break

    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    bpp = max(1, (channels * depth + 7) // 8)          # bytes per pixel for filtering
    stride = (width * channels * depth + 7) // 8
    raw = zlib.decompress(bytes(idat))

    rows = []
    prev = bytearray(stride)
    off = 0
    for _ in range(height):
        ftype = raw[off]
        line = bytearray(raw[off + 1:off + 1 + stride])
        off += 1 + stride
        if ftype == 1:      # Sub
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif ftype == 2:    # Up
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ftype == 3:    # Average
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif ftype == 4:    # Paeth
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        elif ftype != 0:
            raise SystemExit("%s: bad filter %d" % (path, ftype))
        prev = line
        rows.append(pixels_of(line, width, depth, ctype, channels, palette, trns, path))

    return width, height, rows


def pixels_of(line, width, depth, ctype, channels, palette, trns, path):
    """Return [(r, g, b, a), ...] for one scanline."""
    out = []
    if depth < 8:
        per_byte = 8 // depth
        mask = (1 << depth) - 1
        samples = []
        for byte in line:
            for k in range(per_byte):
                samples.append((byte >> (8 - depth * (k + 1))) & mask)
        samples = samples[:width * channels]
    elif depth == 8:
        samples = list(line)
    else:
        samples = []
        for i in range(0, len(line), 2):
            samples.append((line[i] << 8) | line[i + 1])

    if ctype == 3:
        for idx in samples:
            r, g, b = palette[idx]
            a = trns[idx] if idx < len(trns) else 255
            out.append((r, g, b, a))
    elif ctype == 6:
        for i in range(0, len(samples), 4):
            out.append(tuple(samples[i:i + 4]))
    elif ctype == 2:
        for i in range(0, len(samples), 3):
            out.append(tuple(samples[i:i + 3]) + (255,))
    elif ctype == 0:
        for v in samples:
            out.append((v, v, v, 255))
    else:  # 4 = grey + alpha
        for i in range(0, len(samples), 2):
            v, a = samples[i], samples[i + 1]
            out.append((v, v, v, a))
    return out


def resample(rows, sw, sh, size):
    """Alpha-weighted box filter down to size x size."""
    out = []
    for dy in range(size):
        y0, y1 = dy * sh / size, (dy + 1) * sh / size
        line = []
        for dx in range(size):
            x0, x1 = dx * sw / size, (dx + 1) * sw / size
            rsum = gsum = bsum = asum = wsum = 0.0
            for sy in range(int(y0), min(sh, int(y1) + 1)):
                wy = min(y1, sy + 1) - max(y0, sy)
                if wy <= 0:
                    continue
                row = rows[sy]
                for sx in range(int(x0), min(sw, int(x1) + 1)):
                    wx = min(x1, sx + 1) - max(x0, sx)
                    if wx <= 0:
                        continue
                    w = wx * wy
                    r, g, b, a = row[sx]
                    rsum += r * w * a
                    gsum += g * w * a
                    bsum += b * w * a
                    asum += a * w
                    wsum += w
            if asum > 0:
                line.append((rsum / asum, gsum / asum, bsum / asum, asum / wsum))
            else:
                line.append((0.0, 0.0, 0.0, 0.0))
        out.append(line)
    return out


def hex_rgb(s):
    s = s.lstrip("#")
    return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4))


def lum(c):
    return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]


def posterise(px, ramp):
    a = px[3]
    if a < 0.35:
        return None
    l = lum(px)
    # Emoji are drawn with bright fills, so map their real luminance range (not 0..255)
    # onto the ramp: otherwise almost everything lands on the lightest step.
    t = max(0.0, min(1.0, (l - 40.0) / 150.0))
    idx = t * (len(ramp) - 1)
    lo = int(idx)
    hi = min(lo + 1, len(ramp) - 1)
    f = idx - lo
    return tuple(ramp[lo][k] * (1 - f) + ramp[hi][k] * f for k in range(3))


def to565(rgb, bg565):
    r, g, b = (int(round(v)) for v in rgb)
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--size", type=int, default=44)
    ap.add_argument("--bg", default="0x1082", help="background RGB565 behind the icon")
    ap.add_argument("--ramp", default=",".join(DEFAULT_RAMP))
    ap.add_argument("--preview", action="store_true")
    ap.add_argument("icons", nargs="+", help="name=path.png")
    args = ap.parse_args()

    ramp = [hex_rgb(c) for c in args.ramp.split(",")]
    bg565 = int(args.bg, 16)
    bg_rgb = (((bg565 >> 11) & 0x1F) * 255 // 31,
              ((bg565 >> 5) & 0x3F) * 255 // 63,
              (bg565 & 0x1F) * 255 // 31)

    blocks = []
    for spec in args.icons:
        name, path = spec.split("=", 1)
        sw, sh, rows = read_png(path)
        small = resample(rows, sw, sh, args.size)
        vals = []
        for line in small:
            for px in line:
                p = posterise(px, ramp)
                a = px[3]
                if p is None:
                    vals.append(to565(bg_rgb, bg565))
                else:
                    mix = tuple(p[k] * a + bg_rgb[k] * (1 - a) for k in range(3))
                    vals.append(to565(mix, bg565))
        if args.preview:
            print("%s (%dx%d from %s)" % (name, args.size, args.size, path))
            for y in range(args.size):
                row = ""
                for x in range(args.size):
                    v = vals[y * args.size + x]
                    l = lum((((v >> 11) & 0x1F) << 3, ((v >> 5) & 0x3F) << 2,
                             (v & 0x1F) << 3))
                    row += " .:-=+*#%@"[min(9, int(l / 26))]
                print(row)
        ident = "ICON_" + name.upper().replace("-", "_")
        body = ",\n".join(
            "  " + ", ".join("0x%04X" % v for v in vals[i:i + 12])
            for i in range(0, len(vals), 12))
        blocks.append(
            "// %s - %s (%d x %d, RGB565, composited over 0x%04X)\n"
            "static const uint16_t %s[%d] = {\n%s\n};\n"
            % (ident, path.replace("\\", "/"), args.size, args.size, bg565,
               ident, len(vals), body))

    header = (
        "// Generated by tools/png2header.py - do not edit by hand.\n"
        "//\n"
        "// Icons from Twemoji 15.0.3 (jdecked/twemoji), CC-BY 4.0.\n"
        "// https://github.com/jdecked/twemoji - recoloured onto this screen's palette.\n"
        "#pragma once\n\n#include <Arduino.h>\n\n"
        "#define UI_ICON_SIZE %d\n\n" % args.size) + "\n".join(blocks)
    open(args.out, "w", encoding="utf-8", newline="\n").write(header)
    print("wrote %s (%d icons, %d B flash)" %
          (args.out, len(blocks), args.size * args.size * 2 * len(blocks)))


if __name__ == "__main__":
    main()
