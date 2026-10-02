#include "AudioCue.h"
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {
std::atomic_flag g_isPlaying = ATOMIC_FLAG_INIT;

#pragma pack(push, 1)
struct WaveHeader {
    char riff[4];
    std::uint32_t fileSize;
    char wave[4];
    char format[4];
    std::uint32_t formatSize;
    std::uint16_t audioFormat;
    std::uint16_t channels;
    std::uint32_t sampleRate;
    std::uint32_t byteRate;
    std::uint16_t blockAlign;
    std::uint16_t bitsPerSample;
    char data[4];
    std::uint32_t dataSize;
};
#pragma pack(pop)

void AppendTone(std::vector<std::int16_t>& samples, std::uint32_t frequency, std::size_t count) {
    constexpr double pi = 3.14159265358979323846;
    constexpr std::size_t fadeSamples = 110;
    constexpr double amplitude = 6000.0;

    for (std::size_t i = 0; i < count; ++i) {
        const double fadeIn = static_cast<double>(i) / fadeSamples;
        const double fadeOut = static_cast<double>(count - i) / fadeSamples;
        const double envelope = (std::min)(1.0, (std::min)(fadeIn, fadeOut));
        const double phase = 2.0 * pi * frequency * i / 22050.0;
        samples.push_back(static_cast<std::int16_t>(std::sin(phase) * amplitude * envelope));
    }
}

void CALLBACK PlayMelody(PTP_CALLBACK_INSTANCE, void*) {
    try {
        constexpr std::uint32_t sampleRate = 22050;
        constexpr std::uint32_t firstToneSamples = sampleRate * 75 / 1000;
        constexpr std::uint32_t silenceSamples = sampleRate * 30 / 1000;
        constexpr std::uint32_t secondToneSamples = sampleRate * 125 / 1000;
        constexpr std::uint32_t dataSize =
            (firstToneSamples + silenceSamples + secondToneSamples) * sizeof(std::int16_t);

        std::vector<std::int16_t> samples;
        samples.reserve(dataSize / sizeof(std::int16_t));
        AppendTone(samples, 784, firstToneSamples);
        samples.insert(samples.end(), silenceSamples, 0);
        AppendTone(samples, 1047, secondToneSamples);

        const WaveHeader header{
            { 'R', 'I', 'F', 'F' }, sizeof(WaveHeader) - 8 + dataSize,
            { 'W', 'A', 'V', 'E' }, { 'f', 'm', 't', ' ' }, 16,
            1, 1, sampleRate, sampleRate * sizeof(std::int16_t),
            sizeof(std::int16_t), 16, { 'd', 'a', 't', 'a' }, dataSize
        };
        std::vector<std::uint8_t> wave(sizeof(header) + dataSize);
        std::memcpy(wave.data(), &header, sizeof(header));
        std::memcpy(wave.data() + sizeof(header), samples.data(), dataSize);
        PlaySoundW(reinterpret_cast<LPCWSTR>(wave.data()), nullptr, SND_MEMORY | SND_NODEFAULT);
    } catch (...) {
    }
    g_isPlaying.clear(std::memory_order_release);
}
}

void AudioCue::PlayNewTaskMelody() {
    if (g_isPlaying.test_and_set(std::memory_order_acquire)) return;
    if (!TrySubmitThreadpoolCallback(PlayMelody, nullptr, nullptr)) {
        g_isPlaying.clear(std::memory_order_release);
    }
}