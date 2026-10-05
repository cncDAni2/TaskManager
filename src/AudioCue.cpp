#include "AudioCue.h"
#include "AppState.h"
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

namespace {
std::atomic_flag g_isPlaying = ATOMIC_FLAG_INIT;
constexpr std::uint32_t kVisionBeepFrequency = 880;
constexpr std::uint32_t kVisionBeepDurationMs = 330;
constexpr double kVisionBeepAmplitude = 5200.0;

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

struct MelodyNote {
    std::uint32_t frequency;
    std::uint32_t durationMs;
    std::uint32_t silenceAfterMs;
    double amplitude;
};

void AppendTone(std::vector<std::int16_t>& samples, std::uint32_t frequency,
    std::size_t count, double amplitude) {
    constexpr double pi = 3.14159265358979323846;
    constexpr std::size_t fadeSamples = 110;

    for (std::size_t i = 0; i < count; ++i) {
        const double fadeIn = static_cast<double>(i) / fadeSamples;
        const double fadeOut = static_cast<double>(count - i) / fadeSamples;
        const double envelope = (std::min)(1.0, (std::min)(fadeIn, fadeOut));
        const double phase = 2.0 * pi * frequency * i / 22050.0;
        samples.push_back(static_cast<std::int16_t>(std::sin(phase) * amplitude * envelope));
    }
}

void CALLBACK PlayMelody(PTP_CALLBACK_INSTANCE, void* context) {
    try {
        constexpr std::uint32_t sampleRate = 22050;
        static constexpr MelodyNote newTaskNotes[] = {
            { 784, 115, 30, 6000.0 }, { 1047, 115, 30, 6000.0 }
        };
        const std::uintptr_t melody = reinterpret_cast<std::uintptr_t>(context);
        std::vector<MelodyNote> notes;
        if (melody == 1) {
            notes = {
                { 392, 150, 30, 6000.0 },
                { 523, 150, 30, 6000.0 },
                { 659, 66, 30, 6000.0 },
                { 523, 66, 30, 6000.0 },
                { 392, 150, 30, 6000.0 },
                { 659, 150, 30, 6000.0 },
                { 523, 150, 30, 6000.0 },
                { 659, 66, 30, 6000.0 },
                { 523, 66, 30, 6000.0 },
                { 392, 150, 30, 6000.0 },
                { 659, 150, 30, 6000.0 },
                { 523, 150, 30, 6000.0 },
                { 659, 66, 30, 6000.0 },
                { 523, 66, 30, 6000.0 },
                { 392, 150, 30, 6000.0 },
                { 523, 150, 30, 6000.0 },
                { 659, 400, 0, 6000.0 }
            };
        } else if (melody == 2) {
            notes = {
                { 247, 650, 170, 3600.0 },
                { 220, 650, 170, 3600.0 },
                { 196, 650, 0, 3600.0 }
            };
        } else if (melody == 3) {
            notes = {
                { kVisionBeepFrequency, kVisionBeepDurationMs, 100, kVisionBeepAmplitude },
                { kVisionBeepFrequency, kVisionBeepDurationMs, 0, kVisionBeepAmplitude }
            };
        } else if (melody == 4) {
            notes = { { kVisionBeepFrequency, kVisionBeepDurationMs, 0, kVisionBeepAmplitude } };
        } else {
            notes.assign(std::begin(newTaskNotes), std::end(newTaskNotes));
        }

        std::vector<std::int16_t> samples;
        for (std::size_t noteIndex = 0; noteIndex < notes.size(); ++noteIndex) {
            const MelodyNote& note = notes[noteIndex];
            const std::size_t toneSamples = static_cast<std::size_t>(sampleRate) * note.durationMs / 1000;
            AppendTone(samples, note.frequency, toneSamples, note.amplitude);
            if (noteIndex + 1 < notes.size() && note.silenceAfterMs > 0) {
                const std::size_t silenceSamples = static_cast<std::size_t>(sampleRate) * note.silenceAfterMs / 1000;
                samples.insert(samples.end(), silenceSamples, 0);
            }
        }
        const std::uint32_t dataSize = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));

        const WaveHeader header{
            { 'R', 'I', 'F', 'F' }, sizeof(WaveHeader) - 8 + dataSize,
            { 'W', 'A', 'V', 'E' }, { 'f', 'm', 't', ' ' }, 16,
            1, 1, sampleRate, sampleRate * sizeof(std::int16_t),
            sizeof(std::int16_t), 16, { 'd', 'a', 't', 'a' }, dataSize
        };
        std::vector<std::uint8_t> wave(sizeof(header) + dataSize);
        std::memcpy(wave.data(), &header, sizeof(header));
        std::memcpy(wave.data() + sizeof(header), samples.data(), dataSize);
        if (g_store.sounds_enabled) {
            PlaySoundW(reinterpret_cast<LPCWSTR>(wave.data()), nullptr, SND_MEMORY | SND_NODEFAULT);
        }
    } catch (...) {
    }
    g_isPlaying.clear(std::memory_order_release);
}

void SubmitMelody(std::uintptr_t melody) {
    if (!g_store.sounds_enabled) return;
    if (g_isPlaying.test_and_set(std::memory_order_acquire)) return;
    if (!TrySubmitThreadpoolCallback(PlayMelody, reinterpret_cast<void*>(melody), nullptr)) {
        g_isPlaying.clear(std::memory_order_release);
    }
}
}

void AudioCue::PlayNewTaskMelody() {
    SubmitMelody(0);
}

void AudioCue::PlayTimerWorkMelody() {
    SubmitMelody(1);
}

void AudioCue::PlayTimerRestMelody() {
    SubmitMelody(2);
}

void AudioCue::PlayVisionBreakStart() {
    SubmitMelody(3);
}

void AudioCue::PlayVisionBreakEnd() {
    SubmitMelody(4);
}