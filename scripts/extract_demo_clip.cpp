#include <cstdio>
#include <cstdint>
#include <vector>
#include <iostream>
#include "psp/psp_strdecode.h"

static uint32_t ReadU32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
static uint16_t ReadU16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "bin/fe/movies/demo.str";
    const char* out_video = argc > 2 ? argv[2] : "/tmp/demo_15s_video.raw";
    const char* out_audio = argc > 3 ? argv[3] : "/tmp/demo_15s_audio.raw";
    const int max_frames = 225; // 15 seconds @ 15 fps
    const uint32_t target_audio_samples = 37800 * 15; // 15s @ 37.8kHz

    FILE* f = fopen(path, "rb");
    if (!f) { std::cerr << "Cannot open " << path << "\n"; return 1; }
    FILE* fv = fopen(out_video, "wb");
    FILE* fa = fopen(out_audio, "wb");

    const uint32_t sectorSize = 2336;
    std::vector<uint8_t> sector(sectorSize);
    std::vector<uint8_t> frame;
    uint32_t frameNum = 0xFFFFFFFFu, chunks = 0, received = 0, width = 0, height = 0, demux = 0;
    int frames_decoded = 0;
    uint32_t audio_samples_decoded = 0;

    StrDecode::XaState xa;
    std::vector<int16_t> pcm(StrDecode::kXaSamplesPerSector);

    while ((frames_decoded < max_frames || audio_samples_decoded < target_audio_samples) &&
           fread(sector.data(), 1, sectorSize, f) == sectorSize) {
        uint8_t submode = sector[2];
        uint8_t coding = sector[3];
        const uint8_t* data = sector.data() + 8;

        if (submode & 0x04) {
            // Audio sector
            if (audio_samples_decoded < target_audio_samples) {
                uint32_t samples = StrDecode::DecodeXaSector(data, coding, xa, pcm.data());
                uint32_t channels = StrDecode::XaChannels(coding);
                if (samples > 0) {
                    uint32_t to_write = samples;
                    if (audio_samples_decoded + to_write > target_audio_samples) {
                        to_write = target_audio_samples - audio_samples_decoded;
                    }
                    fwrite(pcm.data(), sizeof(int16_t), to_write * channels, fa);
                    audio_samples_decoded += to_write;
                }
            }
            continue;
        }

        if (frames_decoded >= max_frames) continue;

        if ((ReadU32(data) & 0xFFFFFF) != 0x010160) continue;
        uint16_t chunk = ReadU16(data + 4);
        uint16_t count = ReadU16(data + 6);
        uint32_t num = ReadU32(data + 8);

        if (num != frameNum) {
            if (received && received >= chunks && demux > 0) {
                frame.resize(demux);
                std::vector<uint16_t> rgb565(width * height);
                if (StrDecode::DecodeFrame565(frame.data(), demux, width, height, rgb565.data(), width)) {
                    std::vector<uint8_t> rgb24(width * height * 3);
                    for (size_t i = 0; i < rgb565.size(); ++i) {
                        uint16_t p = rgb565[i];
                        uint8_t r = ((p & 0x001F) * 527 + 23) >> 6;
                        uint8_t g = (((p >> 5) & 0x003F) * 259 + 33) >> 6;
                        uint8_t b = (((p >> 11) & 0x001F) * 527 + 23) >> 6;
                        rgb24[i * 3 + 0] = r;
                        rgb24[i * 3 + 1] = g;
                        rgb24[i * 3 + 2] = b;
                    }
                    fwrite(rgb24.data(), 1, rgb24.size(), fv);
                    frames_decoded++;
                }
            }
            frameNum = num;
            chunks = count;
            received = 0;
            demux = ReadU32(data + 12);
            width = ReadU16(data + 16);
            height = ReadU16(data + 18);
            frame.clear();
        }

        if (chunk < count) {
            uint8_t form2 = submode & 0x20;
            uint32_t payload = (form2 ? 2328u : 2048u) - 32u;
            frame.insert(frame.end(), data + 32, data + 32 + payload);
            received++;
        }
    }

    fclose(f);
    fclose(fv);
    fclose(fa);

    std::cout << "Successfully extracted " << frames_decoded << " video frames and "
              << audio_samples_decoded << " audio samples (15.00s).\n";
    return 0;
}
