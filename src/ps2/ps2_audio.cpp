#include "pc/audio.h"

bool AudioEngine::Init() {
    return true;
}

void AudioEngine::Shutdown() {}

bool AudioEngine::IsInitialized() {
    return true;
}

AudioSample AudioEngine::LoadSample(const s16*, u32, u32, u32) {
    static AudioSample next = 1;
    return next++;
}

void AudioEngine::UnloadSample(AudioSample) {}

void AudioEngine::UnloadAllSamples() {}

AudioVoice AudioEngine::PlaySample(AudioSample, f32, f32, bool, u8) {
    return AUDIO_VOICE_INVALID;
}

AudioVoice AudioEngine::PlaySample3D(AudioSample, const LVector&, f32, bool, bool, f32, f32, u8) {
    return AUDIO_VOICE_INVALID;
}

AudioVoice AudioEngine::ReserveVoice(u8) {
    return AUDIO_VOICE_INVALID;
}

void AudioEngine::ReleaseVoice(AudioVoice) {}

AudioVoice AudioEngine::PlayOnReservedVoice(AudioVoice, AudioSample, f32, f32) {
    return AUDIO_VOICE_INVALID;
}

void AudioEngine::StopVoice(AudioVoice) {}

void AudioEngine::StopAllVoices() {}

bool AudioEngine::IsVoicePlaying(AudioVoice) {
    return false;
}

void AudioEngine::SetVoiceVolume(AudioVoice, f32) {}

void AudioEngine::FadeVoiceVolume(AudioVoice, f32, u32) {}

void AudioEngine::SetVoicePan(AudioVoice, f32) {}

void AudioEngine::SetVoicePitch(AudioVoice, f32) {}

void AudioEngine::SetVoicePosition(AudioVoice, const LVector&) {}

void AudioEngine::SetVoiceDistanceRange(AudioVoice, f32, f32) {}

void AudioEngine::SetListener(const LVector&, s32) {}

bool AudioEngine::PlayMusic(const char*, f32, bool) {
    return true;
}

bool AudioEngine::PlayMusicSample(AudioSample, f32, bool) {
    return true;
}

void AudioEngine::StopMusic() {}

void AudioEngine::PauseMusic() {}
void AudioEngine::ResumeMusic() {}

void AudioEngine::SetMusicVolume(f32) {}

f32 AudioEngine::GetMusicVolume() { return 0.0f; }

void AudioEngine::FadeMusicVolume(f32, u32) {}

bool AudioEngine::IsMusicPlaying() {
    return false;
}

void AudioEngine::SetMasterVolume(f32) {}

f32 AudioEngine::GetMasterVolume() {
    return 1.0f;
}

void AudioEngine::SetOutputMono(bool) {}

bool AudioEngine::GetOutputMono() {
    return false;
}

u32 AudioEngine::GetOutputChannels() {
    return 2;
}

void AudioEngine::SetSurroundEnabled(bool) {}

bool AudioEngine::GetSurroundEnabled() {
    return false;
}
