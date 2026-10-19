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
  lives next to it as `PCSX2.ini.bak-rechan`). The GS backend writes its own
  trace to `host:/rechan-ps2-gs.log`.
* **EE TTY** — `printf` from the EE does not reach `-logfile` with this setup,
  which is why the file channel above exists.
* **Game data** — the engine opens its assets as `pc/...` through `host:`, so
  they have to be reachable from the ELF's directory: `build-remote.sh` links
  `res/pc` there as `dist/ps2/pc`. The original PSX data (`SOUND/`, `XC/`, ...)
  is not distributed with the engine; place it in the same directory when you
  have extracted it locally.
* **Screenshots** — `ps2/tools/pcsx2-shot.sh` boots an ELF, waits for the
  emulator log to report that the ELF (not the BIOS) is executing, settles a few
  seconds and captures the window; `ps2/tools/verify-render.py` prints the colour
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

Step 1 is done: the ELF boots in PCSX2, brings up the GS at 640x448 NTSC and
renders — the clear plus queued sprites from both the persistent and the oneshot
queues, over a 600-frame loop at ~60 Hz. That synthetic harness now lives in
`ps2/test_pddi_main.cpp` and is not part of the game build any more.

Step 2 is done: the PDDI GS backend sits in `vendor/libp3d/pddi/ps2` and the
platform services in `src/ps2` (file I/O over `host:`, EE clock, audio stubs,
desktop-only stubs).

Step 3, the engine itself, is integrated: `rechan.elf` is built from the real
sources (`src/gen`, `src/ai`, `src/fe`, `src/radlib`, `src/snd`, `src/xclib`,
`vendor/libp3d/p3d`) with `-DRC_PLATFORM_PS2`, boots through `Game::Open()` and
runs the `Game::Step()` loop at a self-reported 60 Hz, reading its data through
`host:` — with no EE exceptions and only the assets the user has to provide
missing from the log. What is still blank is the picture: the game reaches its
state machine but draws nothing visible yet, so the retained-mode mesh path
(`pddiPrimBuffer` over the world geometry), the PSX VRAM/texture lookups and the
render targets are the next pieces of the port.
