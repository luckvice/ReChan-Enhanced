#include "snd/rsdformat.h"
#include <fstream>
#include <iostream>
#include <vector>

static void writeWav(const std::string& path, const std::vector<s16>& pcm, u32 sampleRate, u32 channels) {
    std::ofstream out(path, std::ios::binary);
    u32 dataSize = pcm.size() * sizeof(s16);
    u32 riffSize = 36 + dataSize;
    u16 numChannels = channels;
    u16 bitsPerSample = 16;
    u32 byteRate = sampleRate * numChannels * (bitsPerSample / 8);
    u16 blockAlign = numChannels * (bitsPerSample / 8);

    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&riffSize), 4);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    u32 subchunk1Size = 16;
    u16 audioFormat = 1; // PCM
    out.write(reinterpret_cast<const char*>(&subchunk1Size), 4);
    out.write(reinterpret_cast<const char*>(&audioFormat), 2);
    out.write(reinterpret_cast<const char*>(&numChannels), 2);
    out.write(reinterpret_cast<const char*>(&sampleRate), 4);
    out.write(reinterpret_cast<const char*>(&byteRate), 4);
    out.write(reinterpret_cast<const char*>(&blockAlign), 2);
    out.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&dataSize), 4);
    out.write(reinterpret_cast<const char*>(pcm.data()), dataSize);
}

int main(int argc, char** argv) {
    std::string fagPath = "bin/music/title.fag";
    if (argc > 1) fagPath = argv[1];

    std::ifstream in(fagPath, std::ios::binary);
    if (!in) {
        std::cerr << "Cannot open " << fagPath << std::endl;
        return 1;
    }
    std::vector<u8> buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::cout << "Read " << buffer.size() << " bytes from " << fagPath << std::endl;

    RsdFormat::FagTrack track = RsdFormat::LoadFag(buffer.data(), buffer.size(), 0);
    std::cout << "Decoded: " << track.numFrames << " frames, " << track.channels 
              << " channels, " << track.sampleRate << " Hz" << std::endl;

    double duration = (double)track.numFrames / track.sampleRate;
    std::cout << "Duration: " << duration << " seconds" << std::endl;

    writeWav("/tmp/title_theme.wav", track.pcmData, track.sampleRate, track.channels);
    std::cout << "Wrote /tmp/title_theme.wav" << std::endl;
    return 0;
}
