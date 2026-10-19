# Re:Chan HD Edition (PlayStation 2)

This is the native PlayStation 2 target: Emotion Engine + Graphics Synthesizer,
with no emulation of the PS1 original. The game outputs 640x480 progressive
(480p) at 60 Hz — the NTSC-U disc (SLUS-00684) ran 640x448 interlaced — and is
exercised in PCSX2 during development.

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
  they have to be reachable from the ELF's directory. `ps2/tools/prepare-host-data.sh`
  (called by `build-remote.sh`) copies `res/pc` there as `dist/ps2/pc` and
  downscales the 1920x1080 desktop art once (fullscreen 16:9 to 640x360, the rest
  to fit 512x512): the EE cannot hold an 8 MiB `stbi_load` buffer per sheet and
  the GS cannot address a 1920x1080 texture. The same script links every entry of
  the repository's `bin/` tree (`xc/`, `SOUND/`, `RCHARS/`, `lev*.lcf`, `*.rr`,
  `discimage/`) into `dist/ps2` as symlinks: the extracted disc is ~1.3 GB, it is
  the same data the desktop build reads, and nothing is copied per build. Without
  `bin/` the ELF still boots and shows the frontend, just without the world. The
  original PSX data is not distributed with the engine.
* **Screenshots** — `ps2/tools/pcsx2-shot.sh` boots an ELF, waits for the
  emulator log to report that the ELF (not the BIOS) is executing, settles a few
  seconds and captures the window; `ps2/tools/verify-render.py` prints the colour
  histogram, checks the expected pattern colours (`--preset pddi`), measures how
  much of the game area is not black (`--content`) and can crop the game area to
  a 640x480 PNG.

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
`host:` — with no EE exceptions.

Step 4, the frontend, now draws: the splash/title art loads (all 13 sheets of
`res/pc/textures`, including the 1920x1080 ones, reduced to 640x360) and the
GS presents the logo, the loading bar and the menu ornaments, with the emulator
at 100% speed. The engine still runs with **no** disc-derived assets, so the
world geometry, the PSX VRAM textures, the XC fonts and the audio are absent
from this build. The 3D scene stays empty until the user's own disc image is
extracted into the host directory.

Step 5, 480p, is done: `ps2gs_init` brings the GS up in gsKit's DTV 480p mode
(640x480, non-interlaced, 60 Hz) instead of the 480i mode of the disc. A 32-bit
framebuffer at this size would take 3.7 MiB of the 4 MiB of VRAM and leave
nothing for textures, so the colour framebuffers and the z-buffer run in 16 bits,
as most 480i/480p titles did; that drops their footprint to 1.88 MiB and leaves
~2.2 MiB for textures (the GS trace prints the first texture address). With that
budget the 16:9 art is back at the display width (640x360, 900 KiB) and the
sprite path keeps a 512x512 ceiling for everything else. A fullscreen sheet now
fits on its own, but the splash shows three of them per frame (background,
silhouette, logo), which still exceeds the pool: the texture manager re-uploads
about one sheet per frame (the GS trace shows ~2.9k binds over 1k frames). The
GS absorbs that at 60 fps in the emulator; pooling the splash sheets or batching
the frontend into one atlas is the follow-up.

Step 6, the text, is done: `StbTextBackend` now uses the PSP's atlas recipe on
the PS2 — 512x512, no oversampling, Latin Extended-A/B dropped — because a
2048x2048 atlas is above the 1024x1024 the GS can address and was rejected
outright (`tex: rejected` in the GS trace), leaving every glyph untextured. Each
atlas is 1 MiB of RGBA, which fits the 2.2 MiB left after the framebuffers; the
frontend loads two of them (Menu and Legal), so they still take turns with the
640x360 sheets in the texture pool. Without oversampling the glyphs are minified
from the 48 px strike to the UI size, so the atlas texture asks for bilinear
filtering on the PS2 instead of the GS default of nearest — that is what turns
the blocky edges into readable text. "PRESS ANY BUTTON TO START" now renders in
the title screen. The white squares that remain over the splash are the PSX VRAM
particles, which need textures from the disc.

Step 7, the disc data, is done: with `bin/` reachable the engine loads the real
assets — `RCHARS/JACKIE.RR` (768 resources, 26 joints, 410 polygons, 123
animations), the XC fonts and the PSX VRAM palette uploads — and the state
machine walks `Init -> Title` into `TitleLoop`, holding 640x480p at 60 Hz for as
long as the emulator runs. Two allocations had to be turned off to get there,
both of which only appear once the disc is present: `RSDIALOG.DLG` is a 33 MB
container that the desktop path loads whole into the 32 MiB of the EE, and the
FAG music preload decodes a full track to PCM in the same frame the title opens.
The PS2 now skips both (one log line each) — the PSP solved them with streaming
readers that have no console counterpart yet, so dialogue and music stay silent.
The white squares over the splash are unchanged: they are PSX VRAM particles and
their texture lookup is still missing.

Next pieces of the port: the PSX VRAM texture lookups, the retained-mode mesh
path over the world geometry, and the streaming audio (music, ambience, dialogue).
