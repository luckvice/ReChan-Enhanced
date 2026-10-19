#include "p3d/fileio.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

static std::string ToPs2Path(const std::string& input) {
    std::string path = input;
    std::replace(path.begin(), path.end(), '\\', '/');
    if (path.empty()) return path;
    // If it already has a PS2 device prefix (host:, cdrom0:, mc0:, etc.), keep it.
    if (path.find(':') != std::string::npos) {
        return path;
    }
    // Relative paths default to host: for development in PCSX2.
    // Try host: directly, or fallback to host:data/ if not found.
    std::string hostPath = "host:" + path;
    std::FILE* test = std::fopen(hostPath.c_str(), "rb");
    if (test != nullptr) {
        std::fclose(test);
        return hostPath;
    }
    std::string dataPath = "host:data/" + path;
    test = std::fopen(dataPath.c_str(), "rb");
    if (test != nullptr) {
        std::fclose(test);
        return dataPath;
    }
    return hostPath;
}

} // namespace

namespace p3d::io {

std::string NormalizeSeparators(const std::string& path) {
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

std::string JoinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return NormalizeSeparators(b);
    if (b.empty()) return NormalizeSeparators(a);
    return NormalizeSeparators(a + (a.back() == '/' ? "" : "/") + b);
}

std::string ResolvePath(const char* path) {
    return path != nullptr ? ResolvePath(std::string(path)) : std::string();
}

std::string ResolvePath(const std::string& path) {
    return ToPs2Path(path);
}

void InvalidateResolveCache(const std::string&) {}

bool FileExists(const std::string& path) {
    const std::string resolved = ToPs2Path(path);
    std::FILE* fp = std::fopen(resolved.c_str(), "rb");
    if (fp != nullptr) {
        std::fclose(fp);
        return true;
    }
    return false;
}

bool DirExists(const std::string& path) {
    // Basic existence check on PS2 host:
    return FileExists(path);
}

bool CreateDirectories(const std::string&) {
    return true;
}

std::vector<DirEntryInfo> ListDirectory(const std::string&, bool) {
    return {};
}

std::optional<std::vector<uint8_t>> ReadFile(const std::string& path) {
    FileHandle file;
    if (!file.Open(path, FileHandle::Mode::Read)) {
        return std::nullopt;
    }
    const size_t sz = static_cast<size_t>(std::max<int64_t>(0, file.Size()));
    std::vector<uint8_t> data(sz);
    if (!data.empty() && file.Read(data.data(), data.size()) != data.size()) {
        return std::nullopt;
    }
    return data;
}

bool WriteFile(const std::string& path, const void* data, size_t size) {
    FileHandle file;
    if (!file.Open(path, FileHandle::Mode::Write, false)) {
        return false;
    }
    return file.Write(data, size) == size;
}

bool WriteFile(const std::string& path, const std::vector<uint8_t>& data) {
    return WriteFile(path, data.data(), data.size());
}

std::optional<std::string> ReadTextFile(const std::string& path) {
    const auto bytes = ReadFile(path);
    if (!bytes) {
        return std::nullopt;
    }
    return std::string(bytes->begin(), bytes->end());
}

bool WriteTextFile(const std::string& path, const std::string& text) {
    return WriteFile(path, text.data(), text.size());
}

FileHandle::~FileHandle() {
    Close();
}

FileHandle::FileHandle(FileHandle&& other) noexcept
    : m_file(other.m_file), m_size(other.m_size) {
    other.m_file = nullptr;
    other.m_size = -1;
}

FileHandle& FileHandle::operator=(FileHandle&& other) noexcept {
    if (this != &other) {
        Close();
        m_file = other.m_file;
        m_size = other.m_size;
        other.m_file = nullptr;
        other.m_size = -1;
    }
    return *this;
}

bool FileHandle::Open(const std::string& path, Mode mode, bool) {
    Close();
    const char* modeStr = "rb";
    if (mode == Mode::Write) modeStr = "wb";
    else if (mode == Mode::Append) modeStr = "ab";

    const std::string resolved = ToPs2Path(path);
    std::FILE* fp = std::fopen(resolved.c_str(), modeStr);
    if (fp == nullptr) {
        return false;
    }

    m_file = fp;
    if (mode == Mode::Read) {
        std::fseek(fp, 0, SEEK_END);
        m_size = std::ftell(fp);
        std::fseek(fp, 0, SEEK_SET);
    }
    else {
        m_size = 0;
    }
    return true;
}

void FileHandle::Close() {
    if (m_file != nullptr) {
        std::fclose(static_cast<std::FILE*>(m_file));
        m_file = nullptr;
    }
    m_size = -1;
}

int64_t FileHandle::Tell() const {
    if (m_file == nullptr) return -1;
    return std::ftell(static_cast<std::FILE*>(m_file));
}

bool FileHandle::Seek(int64_t offset, int whence) {
    if (m_file == nullptr) return false;
    return std::fseek(static_cast<std::FILE*>(m_file), (long)offset, whence) == 0;
}

size_t FileHandle::Read(void* dst, size_t bytes) {
    if (m_file == nullptr || bytes == 0) return 0;
    return std::fread(dst, 1, bytes, static_cast<std::FILE*>(m_file));
}

size_t FileHandle::Write(const void* src, size_t bytes) {
    if (m_file == nullptr || bytes == 0) return 0;
    const size_t written = std::fwrite(src, 1, bytes, static_cast<std::FILE*>(m_file));
    if (written > 0) {
        m_size = std::max<int64_t>(m_size, Tell());
    }
    return written;
}

void FileHandle::Flush() {
    if (m_file != nullptr) {
        std::fflush(static_cast<std::FILE*>(m_file));
    }
}

std::string GetExecutablePath() {
    return "host:rechan.elf";
}

std::string GetExecutableDir() {
    return "host:";
}

std::string GetTempDir() {
    return "host:/tmp";
}

} // namespace p3d::io
