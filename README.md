<p align="right">
  <b>English</b> |
  <a href="README.pt-BR.md">Português (Brasil)</a> |
  <a href="README.es.md">Español</a>
</p>

<table align="center">
<tr>
<td valign="middle">

# ReChan — Enhanced & Optimized Edition
**By Luckvice**

**ReChan** is an open reverse-engineering reimplementation of the classic PlayStation game **Jackie Chan Stuntmaster** (SLUS-00684).

This fork by **Luckvice** aims to bring an optimized, enhanced, and definitive gameplay experience to modern and retro hardware, featuring major performance overhauls, dedicated native console ports, visual refinements, and extensive quality-of-life additions.

> [!NOTE]
> **Special Thanks & Recognition**: Massive thanks to the original creator ([SilverwireGames](https://github.com/SilverwireGames/ReChan)) and all contributors to the original reverse-engineering codebase. The purpose of this fork is to elevate the *Jackie Chan Stuntmaster* experience to its highest level of fidelity and performance.

> [!IMPORTANT]
> **Official Project Status**: We are officially in **Beta 4.1**! Current efforts are focused on consolidating stability, performance, and overall polish across all targets and the core engine, preparing the ground for the upcoming **RC1 (Release Candidate 1)** release.

### To use this project:
* You **must own a legal copy** of *Jackie Chan Stuntmaster* (SLUS-00684).
* You must provide your own game disc image.
* All rights to the original game, assets and intellectual property belong to their respective owners.

<br>
<img src="https://img.shields.io/badge/milestone-Beta%204.1-orange?style=flat-square" />
<img src="https://img.shields.io/badge/next-RC1%20Soon-yellow?style=flat-square" />
<img src="https://img.shields.io/badge/optimized%20by-Luckvice-blue?style=flat-square" />
<img src="https://img.shields.io/badge/license-MIT-green?style=flat-square" />
</td>

<td align="center" valign="middle" width="256">
<img src="res/rechanlogo512.png" width="256">
</td>
</tr>
</table>

## About
* Target version: **SLUS-00684** (NTSC-U)
* Development started: **October 2025**
* Current status: **Beta 4.1** (RC1 coming soon)

The project focuses on:
* Reconstructing game logic and original systems
* Rebuilding data structures and matching authentic runtime behavior
* Providing native, ultra-optimized target ports for iconic gaming platforms (PSP, macOS Metal, PS2 WIP)
* Elevating performance, visual fidelity, and audio quality beyond the original hardware limits

## Enhancements
Beyond reconstruction, the project also introduces improvements over the original PlayStation version:
- keyboard and mouse support  
- gamepad support with vibration, including **DualSense/DualShock light bar** and configurable vibration strength
- controller button-prompt styles (Xbox, Nintendo Switch and **PlayStation/DualSense**)
- HOR+ widescreen support  
- higher resolution rendering  
- dynamic shadows and anti-aliasing options  
- PC-specific settings and enhancements  
- **Brazilian Portuguese** localization (plus English, German, French, Italian and Spanish)
- movie subtitle support (`.srt`) and in-game save/load shortcuts  
- general quality-of-life improvements  

## Screenshots

### Desktop Edition
<p align="center">
  <img src="res/screenshots/title.png" width="30%">
  <img src="res/screenshots/hub.png" width="30%">
  <img src="res/screenshots/level.png" width="30%">
</p>

### PSP — RE:Chan HD Edition (Native 480×272)
<p align="center">
  <img src="res/screenshots_psp/xmb.png" width="24%" alt="PSP XMB Interface & Artwork">
  <img src="res/screenshots_psp/title.png" width="24%" alt="PSP Native Title Screen">
  <img src="res/screenshots_psp/hub.png" width="24%" alt="PSP City Hub with HD HUD">
  <img src="res/screenshots_psp/gameplay.png" width="24%" alt="PSP 60 FPS Chinatown Gameplay">
</p>

## Getting Started

Game data is **not included**. Place a legally obtained NTSC-U game image from
your own copy (`.bin` or `.iso`) in a `discimage` folder beside the executable:

```text
rechan/
|-- rechan.exe       # Windows (use "rechan" on Linux and macOS)
`-- discimage/
    `-- game.bin     # or game.iso, file name doesn't matter
```

Keep only one disc image in `discimage`. On first launch, ReChan verifies the
disc version and uses its built-in extractor to prepare the required assets.
No separate extraction utility is needed.
Once assets are extracted the disc image folder can be deleted.

ReChan currently supports 64-bit x86 (`x86_64`) Windows and Linux, and Apple
Silicon (`arm64`) macOS. On macOS the renderer uses a native Metal backend. To
compile it yourself, see the [build guide](docs/visionmaster/business/sources/BUILDING.md). A native PSP
target is also available through PSPSDK. See the platform sections below for
the full feature lists of each edition.

## Platform editions

### PSP — *RE:Chan HD Edition* (Beta 4.1 — RC1 coming soon)

<p align="center">
  <img src="res/screenshots_psp/xmb.png" width="48%" alt="PSP XMB Interface & Artwork">
  <img src="res/screenshots_psp/title.png" width="48%" alt="PSP Native Title Screen">
</p>
<p align="center">
  <img src="res/screenshots_psp/hub.png" width="48%" alt="PSP City Hub with HD HUD">
  <img src="res/screenshots_psp/gameplay.png" width="48%" alt="PSP 60 FPS Chinatown Gameplay">
</p>

A fully native PSP build (no emulation) compiled with PSPSDK, targeting the
fixed 480×272 display. It ships as its own edition with platform features on
top of the base game:

- **60 FPS on PSP-2000/3000** (measured 58.5–60 FPS across all levels), with
  an optional **Cinema mode** locked at a steady 30 FPS
- Triple-buffered rendering with native indexed draws (GE list memory and
  persistent indices)
- **Modern HUD** (the PC HD HUD adapted to 480×272) or **Classic** style,
  switchable on first boot
- Native XMB-friendly menus with **PlayStation button prompts**
- **Three closer camera options** besides the original camera
- **FMV movies with subtitles in six languages** (Brazilian Portuguese,
  English, Spanish, French, German, Italian)
- Boot logos, title-screen **attract demo** and a **contributors' credits roll**
- **On-device disc image extraction**: the PSP prepares the assets from the
  user's own disc image, with language and HUD choice on first boot — no PC
  step required
- PSP-specific settings menu, plus debug/QA, telemetry, soak and PSPLink PRX
  build modes for development
- Uses the extended user memory partition (MEMSIZE) on PSP-2000/3000/Go, with specialized low-memory heap optimizations (<24 MB user partition) for full **PSP-1000 (Fat)** compatibility

Requirements: Any PSP model (PSP-1000 Fat, PSP-2000 Slim, PSP-3000 Brite, or PSP Go) with custom firmware or HEN, an
NTSC-U (SLUS-00684) disc image, and ~1.3 GB free during install (~600 MB
after). Install package and full notes: `dist/psp-release/` (Beta 4.1).

### macOS — Metal Edition

A native Apple Silicon (`arm64`) build rendering through a dedicated **Metal
backend** (no OpenGL translation layer), with the full desktop feature set:

- Native **Metal** renderer written against `pddi` (shaders compiled at
  runtime, MSAA, dynamic textures, ImGui overlays)
- Everything from [Enhancements](#enhancements): HOR+ widescreen, high
  resolution rendering, dynamic shadows, DualSense/DualShock support with
  light bar, keyboard and mouse, all localizations, `.srt` movie subtitles
- Same disc-image extraction flow as Windows/Linux (`discimage/` folder)
- Deterministic `--autotest` harness with screenshot capture through the
  Metal readback path, used for renderer regression testing

To build, see the [build guide](docs/visionmaster/business/sources/BUILDING.md) (`premake5 gmake2` +
`make config=release`).

### PlayStation 2 — Native GS Edition (Work In Progress 🚧)

A fully native PlayStation 2 port running directly on the console's **Emotion Engine (EE)** and rendering onto the **Graphics Synthesizer (GS)** hardware through a specialized `pddi` backend and `gsKit`:

- **Native 480p Progressive Scan**: Presents at native $640 \times 480$ progressive output (Mode 80 @ 59.94 Hz) with double-buffering.
- **Dedicated 16-bit VRAM Architecture**: Implements 16-bit framebuffers and texture storage mode (`GS_PSM_CT16` / RGBA5551, 2 bytes/texel), freeing ~2.12 MiB of GS memory. Fullscreen $640 \times 360$ frontend layers (`background`, `jackie`, `jcslogo`), font atlas, and particle systems reside simultaneously in VRAM with **zero thrashing / zero per-frame evictions**.
- **Hardware-Tailored Rendering**: Fixed-function GS rasterization replacing desktop programmable shaders, featuring textured and Gouraud primitive queues, hardware alpha-blending, and rich color reproduction without blowout.
- **Native Subsystems**: Integrated `libpad` controller polling, IOP driver initialization, and DMA GIF data transfers.
- **Verification**: Tested on both PCSX2 and real PlayStation 2 hardware via `host:` filesystem mapping.

For build and development details, see [ps2/README.md](ps2/README.md).

## Credits & Acknowledgments

- **Original Project Creator**: Sincere thanks and deep recognition to [SilverwireGames](https://github.com/SilverwireGames/ReChan) for the monumental original reverse engineering and code reconstruction of *Jackie Chan Stuntmaster*.
- **Luckvice**: Creator and maintainer of this fork, responsible for the optimized editions, conception of the *RE:Chan HD Edition* for PSP, native Metal backend for macOS, native PlayStation 2 (GS) port, deep CPU/memory optimizations, and overall enhancements for the definitive experience.
- **Community & Contributors**: To all fans, testers, and researchers collaborating with testing, bug reporting, documentation, and support to keep the legacy of this classic alive.

## Legal and Asset Notice

ReChan is an unofficial, free, non-commercial reimplementation project for
*Jackie Chan Stuntmaster*. It is not affiliated with, endorsed by, sponsored
by, or approved by any original developer, publisher, license holder, or
rights holder.

The release package may include support files created specifically for ReChan
by the project author, including new text files, fonts, and textures. These are
newly authored ReChan files, not files from the PlayStation game. They do not
contain or redistribute content extracted or copied from the original game.

For the avoidance of doubt, **no original game assets, disc images, ROMs,
original game executables, videos, music, voice clips, textures, models,
levels, or other copyrighted PlayStation game data are included or distributed
with ReChan.**
The disc image placed in `discimage` is supplied by the user from their own
legally obtained copy. Its required assets are extracted locally on that
user's computer and are not part of the ReChan distribution.

*Jackie Chan Stuntmaster* and all related trademarks, copyrights, characters,
names, logos, assets, and game data remain the property of their respective
rights holders. Third-party libraries or assets included with ReChan remain
subject to their respective licenses.

## FAQ
**Is this a PS1 emulator?**  
No. ReChan is a reimplementation, the game logic is rewritten in C++ for PC and does not emulate PS1 hardware.

**Will this work with PAL versions?**
Currently only the NTSC-U version (**SLUS-00684**) is targeted. The PAL version is not supported and may differ in data layouts.

**Will this ever support original PS1 hardware?**  
No. That is not a goal of this project.

**Can I contribute?**  
Yes. Documentation, data formats, and behavior analysis are all welcome. See
the [build guide](docs/visionmaster/business/sources/BUILDING.md) to get started.

## Crash reports

Fatal crashes create `minidumps/rechan-crash-<process-id>.txt` and ask the user
to open a GitHub issue. Windows also creates a matching `.dmp` minidump in that
folder. Debug reports append `rechan.log`; Release builds compile out logging
entirely. Reports are stored locally and are never uploaded automatically.

Maintainers can verify the packaged crash handler with
`--test-crash-reporter`. Set `RECHAN_CRASH_REPORTER_NO_DIALOG=1` when running
that check in automation.

## Changelog

Notable changes are listed in [CHANGELOG.md](CHANGELOG.md).

## License
MIT License.

Applies only to this codebase.
Does not apply to the original game or its assets.
