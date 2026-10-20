#!/usr/bin/env python3
"""
generate_psp_eboot_artwork.py — Automated Artwork and Media Pipeline for ReChan PSP.

Renders HTML artwork templates to exact PSP XMB formats:
  - ICON0.PNG (144x80)
  - PIC1.PNG (480x272)
  - PIC0.PNG (310x180)
Extracts 15s of bin/fe/movies/demo.str into:
  - ICON1.PMF (144x80 PSMF animated icon video)
  - demo_15s_480x272.mp4 (480x272 preview video)
Injects artwork directly into psp/EBOOT.PBP.
"""

import os
import sys
import subprocess
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

CHROME_BIN = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

def run_cmd(cmd):
    print(">>", " ".join(cmd))
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print("ERROR:", res.stderr)
        sys.exit(res.returncode)
    return res

def render_artwork():
    print("--- Rendering HTML Artwork via Headless Chrome ---")
    psp_dir = ROOT / "psp"
    art_dir = psp_dir / "artwork"

    tasks = [
        ("pic1_background.html", psp_dir / "PIC1.PNG", 480, 272),
        ("icon0_cover.html", psp_dir / "ICON0.PNG", 144, 80),
        ("pic0_overlay.html", psp_dir / "PIC0.PNG", 310, 180),
        ("xmb_render.html", ROOT / "res" / "screenshots_psp" / "xmb.png", 480, 272),
    ]

    for html_name, out_png, w, h in tasks:
        html_file = art_dir / html_name
        cmd = [
            CHROME_BIN,
            "--headless=new",
            f"--screenshot={out_png}",
            f"--window-size={w},{h}",
            "--hide-scrollbars",
            "--default-background-color=00000000",
            f"file://{html_file}"
        ]
        run_cmd(cmd)
        print(f"Rendered {out_png.name} ({w}x{h})")

def update_eboot(eboot_path):
    if not os.path.exists(eboot_path):
        return
    print(f"--- Injecting Artwork into {eboot_path} ---")
    d = open(eboot_path, 'rb').read()
    if d[:4] != b'\x00PBP':
        print("Not a PBP:", eboot_path)
        return
    offs = list(struct.unpack_from('<8I', d, 8)) + [len(d)]
    entries = [d[offs[i]:offs[i+1]] for i in range(8)]
    ver = d[4:8]

    psp_dir = ROOT / "psp"
    icon0 = (psp_dir / "ICON0.PNG").read_bytes()
    pic1 = (psp_dir / "PIC1.PNG").read_bytes()
    pic0 = b''  # Omit PIC0 so PIC1 background is fully unobstructed
    icon1 = (psp_dir / "ICON1.PMF").read_bytes() if (psp_dir / "ICON1.PMF").exists() else b''
    snd0 = (psp_dir / "SND0.AT3").read_bytes() if (psp_dir / "SND0.AT3").exists() else b''

    entries[1] = icon0
    entries[2] = icon1
    entries[3] = pic0
    entries[4] = pic1
    entries[5] = snd0

    off, new_offs = 40, []
    for e in entries:
        new_offs.append(off)
        off += len(e)

    with open(eboot_path, 'wb') as f:
        f.write(b'\x00PBP' + ver + struct.pack('<8I', *new_offs))
        for e in entries:
            f.write(e)
    print(f"Updated {eboot_path}: {len(d)} -> {off} bytes")

def main():
    render_artwork()
    update_eboot(ROOT / "psp" / "EBOOT.PBP")
    if (ROOT / "psp" / "EBOOT-native.PBP").exists():
        update_eboot(ROOT / "psp" / "EBOOT-native.PBP")
    print("Artwork generation complete!")

if __name__ == "__main__":
    main()
