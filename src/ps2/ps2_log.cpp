#include "pc/log.h"
#include <cstdarg>
#include <cstdio>

namespace {
std::FILE* g_ps2LogFile = nullptr;
}

Log& Log::Get() {
    static Log instance;
    return instance;
}

Log::Log() = default;
Log::~Log() {
    Shutdown();
}

void Log::Init(const char* filename) {
    if (g_ps2LogFile != nullptr) {
        return;
    }
    // In PCSX2, host: writes directly to the emulator directory on PC.
    const char* logPath = (filename != nullptr) ? filename : "host:/rechan-ps2.log";
    g_ps2LogFile = std::fopen(logPath, "w");
    if (g_ps2LogFile == nullptr && filename != nullptr) {
        g_ps2LogFile = std::fopen(filename, "w");
    }
}

void Log::Shutdown() {
    if (g_ps2LogFile != nullptr) {
        std::fflush(g_ps2LogFile);
        std::fclose(g_ps2LogFile);
        g_ps2LogFile = nullptr;
    }
}

void Log::WriteToConsole(const char* text) {
    if (text != nullptr) {
        std::printf("%s", text);
    }
}

void Log::LogMessage(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogMessageV(fmt, args);
    va_end(args);
}

void Log::LogMessageV(const char* fmt, va_list args) {
    char buffer[1024];
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    WriteToConsole(buffer);
    if (g_ps2LogFile != nullptr) {
        std::fputs(buffer, g_ps2LogFile);
        std::fflush(g_ps2LogFile);
    }
}
