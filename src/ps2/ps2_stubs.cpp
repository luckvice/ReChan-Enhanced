#include "pc/debugui.h"
#include "pc/crashreporter.h"
#include "pc/settings.h"
#include "radmovie/movieplayer.h"

#include <cstdio>
#include <cstdlib>

namespace DebugUI {
    void Init() {}
    void Draw() {}
    bool IsEnabled() { return false; }
    bool IsHumanoidDamageDisabled() { return false; }
    bool ShouldBlockGameInput() { return false; }
    bool IsPlayerInputAllowed() { return true; }
    bool IsDebugCameraInputAllowed() { return true; }
}

namespace CrashReporter {
    void Install() {}
    void ShowFatalMessage(const char* title, const char* message) {
        std::printf("[FATAL] %s: %s\n", title ? title : "", message ? message : "");
    }
    void TriggerTestCrash() {
        std::abort();
    }
}

GameSettings g_settings;

bool GameSettings::Load(const char*) {
    return true;
}

bool GameSettings::Save(const char*) {
    return true;
}

MoviePlayer::MoviePlayer() = default;
MoviePlayer::~MoviePlayer() = default;

bool MoviePlayer::Open(const char*) {
    return false;
}

void MoviePlayer::Close() {}

bool MoviePlayer::AdvanceFrame() {
    return false;
}

void MoviePlayer::Render() {}

bool MoviePlayer::IsFinished() const {
    return true;
}

bool MoviePlayer::IsOpen() const {
    return false;
}

f32 MoviePlayer::GetFrameRate() const {
    return 30.0f;
}

#include "extra/psxdiscextractor.h"

PsxDiscExtractor* g_psxDiscExtractor = nullptr;

void PsxDiscExtractor::Init() {}
void PsxDiscExtractor::Shutdown() {}
void PsxDiscExtractor::ScanAsync() { m_state = State::Idle; }
void PsxDiscExtractor::StartExtractAsync() { m_state = State::Done; }
float PsxDiscExtractor::GetProgress() const { return 1.0f; }
bool PsxDiscExtractor::AreAssetsPresent() { return true; }
