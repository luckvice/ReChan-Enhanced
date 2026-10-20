#pragma once
#include "core.h"
#include "pc/apppaths.h"

struct GameSettings {
    bool Load(const char* path);
    bool Save(const char* path);
};

extern GameSettings g_settings;

#if defined(RC_PLATFORM_PSP)
// Cinema mode: a steady 30 fps (one 30 Hz logic tick per frame, no
// interpolation) instead of up to 60. Saved with the PSP settings.
bool PspIsCinemaMode();
void PspSetCinemaMode(bool enabled);

// Follow-camera distance: 0 = the level's original framing, 1-3 = closer.
static constexpr s32 kPspCameraDistanceCount = 4;
s32 PspGetCameraDistance();
void PspSetCameraDistance(s32 level);

// First boot: no language and HUD style chosen yet. The intro shows the
// choice once; finishing it saves the settings with the setup mark.
bool PspIsFirstBootSetupPending();
void PspFinishFirstBootSetup();
#endif

// Performance OSD display modes
enum PspPerfDisplayMode : s32 {
    PspPerfDisplay_Off = 0,
    PspPerfDisplay_Fps,       // "60 FPS  16.6 ms"
    PspPerfDisplay_Full,      // "60 FPS  16.6 ms | CPU 24% | VRAM 1.5/2.0 MB | RAM 18.2 MB"
    PspPerfDisplay_Count
};

s32 PspGetPerfDisplayMode();
void PspSetPerfDisplayMode(s32 mode);
u32 PspGetGpuDurationUs();

static constexpr const char* SETTINGS_PATH = apppaths::kSettingsPath;
