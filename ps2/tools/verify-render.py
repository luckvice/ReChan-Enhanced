#!/usr/bin/env python3
"""Analysis of a BMP capture of the PCSX2 window: colour histogram, check for
the expected test-pattern colours, and an optional crop of the game area to a
640x448 PNG (the PS2's native resolution).

Usage:
    python3 ps2/tools/verify-render.py <capture.bmp> [--preset pddi|basic]
                                       [--png <out.png>]

macOS screenshots are colour-managed, so raw channel values may differ from the
colours set on the GS; pixels are matched to the nearest preset colour and the
areas are compared against the geometry the ELF draws, not against bytes.
"""
import struct
import sys
import zlib

# The scenes the boot ELF can draw. `colours` are the values handed to the GS,
# `expected` the share of the 640x448 frame each one covers plus the absolute
# tolerance allowed (the screenshot is scaled by the window and shifted by
# colour management, so areas drift a little).
PRESETS = {
    # ps2/main.cpp: RGBA32 texture, CLUT texture, gouraud quad, triangle batch.
    "pddi": {
        "colours": {
            "background": (0x10, 0x20, 0x60),
            "tex A yellow": (0xf0, 0xc0, 0x20),
            "tex A red": (0xd0, 0x30, 0x20),
            "tex A green": (0x20, 0xd0, 0x50),
            "tex A magenta": (0xd0, 0x20, 0xc0),
            "tex B cyan": (0x20, 0xd0, 0xe0),
            "tex B orange": (0xf0, 0x80, 0x20),
            "tex B purple": (0x70, 0x20, 0xd0),
            "tex B white": (0xff, 0xff, 0xff),
            "gouraud teal": (0x20, 0xa0, 0xa0),
            "batch lime": (0x38, 0xf0, 0x00),
        },
        "expected": {
            "background": (0.5446, 0.030),
            "tex A yellow": (0.0357, 0.008),
            "tex A red": (0.0357, 0.008),
            "tex A green": (0.0357, 0.008),
            "tex A magenta": (0.0357, 0.008),
            "tex B cyan": (0.0335, 0.008),
            "tex B orange": (0.0335, 0.008),
            "tex B purple": (0.0335, 0.008),
            "tex B white": (0.0335, 0.008),
            "gouraud teal": (0.0714, 0.012),
            "batch lime": (0.1071, 0.015),
        },
    },
    # The gsKit-only pattern step 1 drew, kept for the record.
    "basic": {
        "colours": {
            "background blue": (0x10, 0x20, 0x60),
            "yellow": (0xf0, 0xc0, 0x20),
            "red": (0xd0, 0x30, 0x20),
            "green": (0x20, 0xd0, 0x50),
            "white": (0xff, 0xff, 0xff),
        },
        "expected": {
            "background blue": (0.6610, 0.030),
            "yellow": (0.1451, 0.030),
            "red": (0.1451, 0.030),
            "green": (0.0363, 0.030),
            "white": (0.0126, 0.030),
        },
    },
}
DEFAULT_PRESET = "pddi"

# A pixel further than this (per channel) from every preset colour is window
# chrome, not part of the picture.
CHANNEL_TOLERANCE = 40


def load_bmp(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:2] == b"BM", "not a BMP"
    off = struct.unpack_from("<I", data, 10)[0]
    w = struct.unpack_from("<i", data, 18)[0]
    h = struct.unpack_from("<i", data, 22)[0]
    bpp = struct.unpack_from("<H", data, 28)[0]
    return data, off, w, abs(h), bpp


def write_png(path, width, height, rgb_rows):
    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload
                + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    raw = b"".join(b"\x00" + bytes(row) for row in rgb_rows)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        print(__doc__)
        return
    src = args[0]
    png_out = None
    if "--png" in sys.argv:
        png_out = sys.argv[sys.argv.index("--png") + 1]
    preset_name = DEFAULT_PRESET
    if "--preset" in sys.argv:
        preset_name = sys.argv[sys.argv.index("--preset") + 1]
    if preset_name not in PRESETS:
        print(f"unknown preset: {preset_name} (have {', '.join(PRESETS)})")
        return
    pattern = PRESETS[preset_name]["colours"]
    expected = PRESETS[preset_name]["expected"]
    print(f"preset: {preset_name}")

    data, off, w, h, bpp = load_bmp(src)
    step = bpp // 8
    row_bytes = ((bpp * w + 31) // 32) * 4
    if bpp < 24:
        print(f"unsupported bpp: {bpp}")
        return
    print(f"image: {w}x{h} bpp={bpp}")

    def px(x, y):
        p = off + y * row_bytes + x * step
        return data[p + 2], data[p + 1], data[p]

    hist = {}
    ys = range(0, h, max(1, h // 300))
    xs = range(0, w, max(1, w // 400))
    for y in ys:
        for x in xs:
            r, g, b = px(x, y)
            q = (r // 16 * 16, g // 16 * 16, b // 16 * 16)
            hist[q] = hist.get(q, 0) + 1

    print("top 10 quantised colours (RGB):")
    for (r, g, b), n in sorted(hist.items(), key=lambda kv: -kv[1])[:10]:
        print(f"  #{r:02x}{g:02x}{b:02x}  {n}")

    # Assign every sampled pixel to the nearest pattern colour and compare the
    # proportions with the area each one covers in the test pattern. Robust
    # against the channel shifts introduced by macOS colour management.
    counts = {name: 0 for name in pattern}
    total = 0
    for (r, g, b), n in hist.items():
        best, best_d = None, 1 << 30
        for name, (wr, wg, wb) in pattern.items():
            d = (r - wr) ** 2 + (g - wg) ** 2 + (b - wb) ** 2
            if d < best_d:
                best, best_d = name, d
        if best_d > CHANNEL_TOLERANCE ** 2 * 3:
            continue  # window chrome / black borders, not part of the pattern
        counts[best] += n
        total += n
    print(f"pattern proportions over {total} sampled pixels:")
    hits = 0
    for name, (share, tol) in expected.items():
        got = counts[name] / total if total else 0.0
        ok = abs(got - share) <= tol
        hits += 1 if ok else 0
        print(f"  [{'OK' if ok else '--'}] {name}: {got*100:5.1f}% "
              f"(expected {share*100:.1f}% +/-{tol*100:.1f})")
    print(f"pattern colours within tolerance: {hits}/{len(expected)}")

    if png_out is None:
        return

    # The game area is the span of saturated pixels: rows/columns with a low
    # saturated count (window title bar, shadow, black bars) are ignored.
    row_count = [0] * h
    col_count = [0] * w
    for y in range(h):
        for x in range(w):
            r, g, b = px(x, y)
            if max(r, g, b) - min(r, g, b) > 40:
                row_count[y] += 1
                col_count[x] += 1
    row_min = max(1, int(0.5 * max(row_count)))
    col_min = max(1, int(0.5 * max(col_count)))

    sat = [[0] * w for _ in range(h)]
    minx, miny, maxx, maxy = w, h, -1, -1
    for y in range(h):
        if row_count[y] < row_min:
            continue
        for x in range(w):
            if col_count[x] < col_min:
                continue
            r, g, b = px(x, y)
            if max(r, g, b) - min(r, g, b) > 40:
                sat[y][x] = 1
                if x < minx:
                    minx = x
                if x > maxx:
                    maxx = x
                if y < miny:
                    miny = y
                if y > maxy:
                    maxy = y
    if maxx < 0:
        print("no saturated pixels found (nothing was drawn?)")
        return
    bw, bh = maxx - minx + 1, maxy - miny + 1
    print(f"game area: {bw}x{bh} at +{minx}+{miny}")

    out_w, out_h = 640, 448
    rows = []
    for oy in range(out_h):
        sy = miny + (oy * bh) // out_h
        row = bytearray()
        for ox in range(out_w):
            sx = minx + (ox * bw) // out_w
            r, g, b = px(sx, sy)
            row += bytes((r, g, b))
        rows.append(row)
    write_png(png_out, out_w, out_h, rows)
    print(f"wrote {out_w}x{out_h} PNG to {png_out}")


main()
