# Re:Chan HD Edition (PlayStation 2)

This is the native PlayStation 2 target: Emotion Engine + Graphics Synthesizer,
with no emulation of the PS1 original. The game runs on the PS2's native video
mode — 640x448 NTSC interlaced (480i), matching the NTSC-U disc (SLUS-00684) —
and is exercised in PCSX2 during development.

The renderer for this target goes through gsKit (the GS abstraction shipped
with the ps2dev toolchain), which suits a palette-heavy game: GS CLUT textures,
fixed-function blending and per-primitive Z, with no shaders and no VFPU.

## Toolchain

There is no EE toolchain on the macOS host (and the local disk is tight), so the
build runs in the official ps2dev container on a remote Docker host:

| Item | Value |
| --- | --- |
| Image | `ghcr.io/ps2dev/ps2dev:latest` |
| Compiler | `mips64r5900el-ps2-elf-gcc` (EE, GCC 15.2.0) |
| Render | gsKit at `$PS2DEV/gsKit` (outside ps2sdk) |
| Remote host | `dev@192.168.18.24` (override with `RECHAN_PS2_HOST`) |

The image is Alpine and does **not** ship `make`; the build script installs it
inside the container before compiling.

## Build

```bash
bash ps2/build-remote.sh
# -> dist/ps2/rechan.elf   (dist/ is git-ignored)
```

The script tars `ps2/`, copies it to `/tmp/rechan-ps2` on the remote host,
builds inside the container and brings the ELF back. Overridable variables:
`RECHAN_PS2_HOST`, `RECHAN_PS2_IMAGE`, `RECHAN_PS2_DIR`, `RECHAN_PS2_TARGET`.

## Run in PCSX2

PCSX2 2.8.2 with a PS2 BIOS dump:

```bash
# windowed (what the QA screenshot loop uses)
/Applications/PCSX2.app/Contents/MacOS/PCSX2 -elf dist/ps2/rechan.elf -logfile /tmp/pcsx2.log

# no main window, exits when the ELF returns
/Applications/PCSX2.app/Contents/MacOS/PCSX2 -nogui -batch -elf dist/ps2/rechan.elf -logfile /tmp/pcsx2.log
```

## QA channels

* **Program log** — the program writes `host:/rechan-ps2.log`, which PCSX2 maps
  to the ELF's own directory, so it lands on `dist/ps2/rechan-ps2.log`. This
  needs `HostFs = true` under `[EmuCore]` in
  `~/Library/Application Support/PCSX2/inis/PCSX2.ini` (a backup of the original
  lives next to it as `PCSX2.ini.bak-rechan`).
* **EE TTY** — `printf` from the EE does not reach `-logfile` with this setup,
  which is why the file channel above exists.
* **Screenshots** — `ps2/tools/pcsx2-shot.sh` boots an ELF, waits for the game
  window and captures it; `ps2/tools/verify-render.py` prints the colour
  histogram, checks the expected pattern colours and can crop the game area to a
  640x448 PNG.

```bash
bash ps2/tools/pcsx2-shot.sh "$PWD/dist/ps2/rechan.elf" /tmp/shot
python3 ps2/tools/verify-render.py /tmp/shot2.bmp --png /tmp/frame.png
```

Note that macOS screenshots are colour-managed, so raw channel values in a
capture can differ from the colours set on the GS; compare hues, not exact
bytes.

## Status

Step 1 (this directory's `main.cpp`) is done: the ELF boots in PCSX2, brings up
the GS at 640x448 NTSC and renders — the clear plus queued sprites from both the
persistent and the oneshot queues, over a 600-frame loop at ~60 Hz.

Next steps for the port: the PDDI GS backend (`vendor/libp3d/pddi/ps2`), the
platform services (`src/ps2`: file I/O and ISO, audio via audsrv, timer/vsync,
controller) and then the game itself.
