# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.0.0-rc1_PSP] - 2026-10-20

### Added

- **PSP-1000 (Fat) 24 MB RAM support**:
  - Native partition detection using `sceKernelMaxFreeMemSize()` instead of exploratory arena allocations.
  - Dynamic release of title screen and frontend textures upon entering gameplay, freeing ~2 MB of RAM.
  - Peak gameplay RAM constrained to 15.05–16.61 MB, well within the 24 MB user partition budget.
  - Updated title screen badge to **PSP EDITION RC 1**.

### Fixed

- **Audio pause / unpause integrity**:
  - Implemented `PauseMusic()` and `ResumeMusic()` across native PSP and PC backends to freeze and resume music streaming without drift or stream death.
  - Preserved multi-song track indexing (`currentMusicSong`) for flawless pause/resume behavior in the Hub.
  - Eliminated audio worker redundant loop and semaphore contention, resolving menu music interruption and CPU starvation.
- **GE GPU load measurement**:
  - Corrected GPU rasterization time tracking on 2D backdrop draws and idle frames, removing false 100% readings in the pause menu (actual menu load: ~2.7%, average gameplay load: ~19.7%).

## [1.1.0] - 2026-09-17

### Added

- **macOS (arm64) support** with a native Metal rendering backend under
  `vendor/libp3d/pddi/metal/`, selected via `RC_PLATFORM_MACOS`.
- Metal implementation of the full `pddi` interface: vertex/index buffers,
  RGBA8 textures and R16UI PSX VRAM textures, a reversed-Z depth buffer,
  MSL shaders compiled at runtime, off-screen render targets
  (RGBA8/RGBA16F/Depth32F/R32Uint), cascaded shadow maps, an ImGui/Metal
  overlay and GLFW gamepad support.
- Runtime MSL ports of the 3D PSX texture shader (CLUT/palette decoding,
  magenta/zero-texel keying) and the title/movie effect shaders (`tilt`,
  `glow`, `godrays`, `dot`, `moviedenoise`, `movieupscale`, `moviesharp`).
- GLFW Cocoa backend sources wired into the macOS build and
  `scripts/build_macos.sh` to generate makefiles and build on macOS.
- **Controller button-prompt style for PlayStation/DualSense** (`ps`): a
  PlayStation glyph sheet and controller overlay (`controller_sheet_ps.png`,
  `controller_overlay_ps.png`), selectable in Controller Settings alongside the
  existing Xbox (default) and Nintendo Switch styles. Additive: the default
  style and existing behavior are unchanged.
- **DualSense/DualShock 4 support** through a new optional
  `pddiGamepad::SetLight(r,g,b)` (a no-op on backends without support):
  - **Light bar** driven by Jackie's health (green → yellow → red, pulsing when
    critical), a red flash when landing a hit and a red blink when taking
    damage.
  - **Rumble** on macOS via raw IOHID, and the shared light bar/rumble effects
    report on Windows/Linux (`pddi/gl/glsonyhid.{h,cpp}`: SetupAPI + hid.dll on
    Windows, `/sys/class/hidraw` on Linux). USB only; Bluetooth and DualShock 4
    LED are not implemented yet.
- Controller settings: a **Vibration** strength slider (0–100), a **Lightbar**
  on/off toggle and the existing **Shock** toggle. They are disabled unless a
  rumble-capable DualSense/DualShock is connected.
- **Brazilian Portuguese** localization (`pt` language,
  `res/pc/text/portuguese.txt`).
- **Movie subtitle support** (`.srt`) for cutscenes, including mod-provided
  subtitle files (ModLoader).
- **Save/Load entries** in the main and pause menus.

### Fixed

- Enemy attack animations: a non-forced stance-loop update (`SetAnim` with
  `force=0`) no longer interrupts a one-shot animation that is still playing.
  The fighting AI re-applies its stance loop every frame, so attack animations
  were visible for a single frame and enemies appeared to stay stiff in their
  fighting pose while attacking.
- Level 1 soft-lock: the `death_fall_goo` script was reconstructed larger than
  the original, overlapping the `NISdoor1`/`NISdoor1WithDialog` regions and
  breaking the door cutscene (player input stayed disabled forever).
- Door collision/cutscene handling: the door NIS now works without an explicit
  `SetDoor` command, death volumes no longer interrupt a door cutscene, and the
  active code snip is cleared when a script ends.

### Changed

- `premake5.lua` and `vendor/libp3d/premake5.lua`: added a `system:macosx` build
  configuration (arm64, required frameworks, Cocoa GLFW sources). The OpenGL
  backend is excluded on macOS and the Metal backend is excluded on every other
  platform. `multiprocessorcompile` is now guarded for generators that do not
  support it.
- `src/pc/crashreporter.cpp`: the POSIX crash handler reports the correct
  platform label on macOS, and `/proc/self/maps` and `/etc/os-release` are only
  read on Linux.
- Vibration strength is scaled by the new vibration setting.

### Notes

- Windows (x86_64) and Linux (x86_64) builds keep using the OpenGL backend; the
  macOS port is additive.
