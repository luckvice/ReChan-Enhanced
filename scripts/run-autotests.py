#!/usr/bin/env python3
"""macOS/Linux port of scripts/run-autotests.ps1.

Runs rechan's scripted --autotest mode over a set of levels and frame rates,
then compares the screenshots with a baseline.

    python3 scripts/run-autotests.py                    compare against scripts/autotest-baseline
    python3 scripts/run-autotests.py --update-baseline  record the current output as the baseline
    python3 scripts/run-autotests.py --exe bin/rechan-asan --no-compare   crash/ASan sweep only

The baseline is local (it is rendered from the user's own game data), so it is
not committed. Record it on a known-good build, then re-run after renderer
changes.
"""

import argparse
import os
import struct
import subprocess
import sys
import time
import zlib
from pathlib import Path

# name, level index, petal, frames, held buttons, fps
CASES = [
    ("l0_start_30", 0, 0, 240, "", 30),
    ("l0_start_60", 0, 0, 480, "", 60),
    ("l0_start_144", 0, 0, 1152, "", 144),
    ("l0_enemy_30", 0, 0, 750, "right", 30),
    ("l0_enemy_60", 0, 0, 1500, "right", 60),
    ("l1_start_60", 1, 0, 480, "", 60),
    ("l2_start_60", 2, 0, 480, "", 60),
    ("l3_start_60", 3, 0, 480, "", 60),
    ("l4_start_60", 4, 0, 480, "", 60),
    ("l5_start_60", 5, 0, 480, "", 60),
    ("l6_hub_60", 6, 0, 480, "", 60),
]


def read_png(path):
    """Minimal PNG reader for 8-bit RGB/RGBA non-interlaced images."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    pos = 8
    width = height = color_type = interlace = None
    idat = []
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos : pos + 4])
        ctype = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + length]
        if ctype == b"IHDR":
            width, height, bit_depth, color_type, _, _, interlace = struct.unpack(
                ">IIBBBBB", chunk
            )
            if bit_depth != 8 or interlace != 0 or color_type not in (2, 6):
                raise ValueError(
                    f"{path}: unsupported PNG (depth={bit_depth} "
                    f"color={color_type} interlace={interlace})"
                )
        elif ctype == b"IDAT":
            idat.append(chunk)
        elif ctype == b"IEND":
            break
        pos += 12 + length
    raw = zlib.decompress(b"".join(idat))
    channels = 4 if color_type == 6 else 3
    stride = width * channels
    out = bytearray(height * stride)
    prev = bytearray(stride)
    pos = 0
    for y in range(height):
        filter_type = raw[pos]
        pos += 1
        line = bytearray(raw[pos : pos + stride])
        pos += stride
        if filter_type == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 0xFF
        elif filter_type == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif filter_type == 3:
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif filter_type == 4:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                b = prev[i]
                c = prev[i - channels] if i >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                if pa <= pb and pa <= pc:
                    pred = a
                elif pb <= pc:
                    pred = b
                else:
                    pred = c
                line[i] = (line[i] + pred) & 0xFF
        elif filter_type != 0:
            raise ValueError(f"{path}: unknown PNG filter {filter_type} on row {y}")
        out[y * stride : (y + 1) * stride] = line
        prev = line
    return width, height, channels, bytes(out)


def mean_diff(path_a, path_b):
    width_a, height_a, channels_a, pixels_a = read_png(path_a)
    width_b, height_b, channels_b, pixels_b = read_png(path_b)
    if width_a != width_b or height_a != height_b or channels_a != channels_b:
        return 255.0
    # Sample every 4th byte: plenty for a mean and keeps this fast.
    total = 0
    count = 0
    for i in range(0, len(pixels_a), 4):
        total += abs(pixels_a[i] - pixels_b[i])
        count += 1
    return total / count if count else 255.0


def main():
    repo = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", default=str(Path("bin") / "rechan"))
    parser.add_argument("--out-dir", default=str(Path("build") / "autotest"))
    parser.add_argument("--baseline-dir", default=str(Path("scripts") / "autotest-baseline"))
    parser.add_argument("--threshold", type=float, default=2.0)
    parser.add_argument("--update-baseline", action="store_true")
    parser.add_argument("--no-compare", action="store_true")
    args = parser.parse_args()

    exe_path = (repo / args.exe).resolve()
    if not exe_path.exists():
        print(f"exe not found: {exe_path}", file=sys.stderr)
        return 2
    work_dir = exe_path.parent
    out_path = repo / args.out_dir
    base_path = repo / args.baseline_dir
    out_path.mkdir(parents=True, exist_ok=True)
    if args.update_baseline:
        base_path.mkdir(parents=True, exist_ok=True)

    env = dict(os.environ, RECHAN_CRASH_REPORTER_NO_DIALOG="1")
    failed = 0
    started = time.time()
    for name, level, petal, frames, hold, fps in CASES:
        shot = out_path / f"{name}.png"
        shot.unlink(missing_ok=True)

        arg_line = [
            str(exe_path),
            "--autotest", str(level),
            "--petal", str(petal),
            "--frames", str(frames),
            "--fps", str(fps),
            "--screenshot", str(shot),
        ]
        if hold:
            arg_line += ["--hold", hold]

        stderr_path = out_path / f"{name}.stderr.txt"
        stdout_path = out_path / "stdout.txt"
        case_started = time.time()
        with open(stderr_path, "wb") as err_f, open(stdout_path, "wb") as out_f:
            proc = subprocess.Popen(
                arg_line,
                cwd=work_dir,
                env=env,
                stdout=out_f,
                stderr=err_f,
            )
            try:
                proc.wait(timeout=600)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
                print(f"FAIL  {name}  timed out")
                failed += 1
                continue
        elapsed = time.time() - case_started

        status = "ok"
        if proc.returncode != 0:
            status = f"exit code {proc.returncode}"
        elif (
            stderr_path.exists()
            and b"AddressSanitizer" in stderr_path.read_bytes()
        ):
            status = f"ASan report in {stderr_path}"
        elif not shot.exists():
            status = "no screenshot"

        if status != "ok":
            print(f"FAIL  {name}  {status}")
            failed += 1
            continue

        base_shot = base_path / f"{name}.png"
        if args.update_baseline:
            base_shot.write_bytes(shot.read_bytes())
            print(f"BASE  {name}  ({elapsed:.1f}s)")
        elif args.no_compare:
            print(f"RAN   {name}  ({elapsed:.1f}s)")
        elif not base_shot.exists():
            print(f"NEW   {name}  (no baseline)")
        else:
            diff = mean_diff(shot, base_shot)
            if diff > args.threshold:
                print(f"DIFF  {name}  mean abs diff {diff:.2f} > {args.threshold}")
                failed += 1
            else:
                print(f"PASS  {name}  mean abs diff {diff:.2f}  ({elapsed:.1f}s)")

    print(f"{failed} failed of {len(CASES)}  ({time.time() - started:.0f}s total)")
    return failed


if __name__ == "__main__":
    sys.exit(main())
