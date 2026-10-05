#include "IntervalTimer.h"
#include "AudioCue.h"
#include <windows.h>
#include <algorithm>

IntervalTimer g_intervalTimer;

namespace {
constexpr unsigned long long kVisionBreakIntervalMs = 20ULL * 60 * 1000;
constexpr unsigned long long kVisionBreakDurationMs = 25ULL * 1000;
}

void IntervalTimer::Start(int newWorkSeconds, int newRestSeconds, int newRepetitions,
    bool newVisionBreakEnabled) {
    workSeconds = newWorkSeconds;
    restSeconds = newRestSeconds;
    repetitions = newRepetitions;
    completedRepetitions = 0;
    workPhase = true;
    paused = false;
    pausedSecondsRemaining = 0;
    running = true;
    const unsigned long long now = GetTickCount64();
    phaseEndTick = now + static_cast<unsigned long long>(workSeconds) * 1000;
    visionBreakEnabled = newVisionBreakEnabled;
    visionBreakActive = false;
    nextVisionBreakTick = now + kVisionBreakIntervalMs;
    visionBreakEndTick = 0;
    pausedNextVisionBreakRemainingMs = 0;
    pausedVisionBreakRemainingMs = 0;
}

void IntervalTimer::Stop() {
    running = false;
    paused = false;
    pausedSecondsRemaining = 0;
    visionBreakActive = false;
    pausedNextVisionBreakRemainingMs = 0;
    pausedVisionBreakRemainingMs = 0;
}

void IntervalTimer::Pause() {
    if (!running) return;
    pausedSecondsRemaining = SecondsRemaining();
    const unsigned long long now = GetTickCount64();
    pausedVisionBreakRemainingMs = visionBreakActive && visionBreakEndTick > now
        ? visionBreakEndTick - now : 0;
    pausedNextVisionBreakRemainingMs = !visionBreakActive && workPhase && visionBreakEnabled &&
        static_cast<unsigned long long>(workSeconds) * 1000 > kVisionBreakIntervalMs &&
        nextVisionBreakTick > now ? nextVisionBreakTick - now : 0;
    running = false;
    paused = true;
}

void IntervalTimer::Resume() {
    if (!paused) return;
    const unsigned long long now = GetTickCount64();
    phaseEndTick = now + static_cast<unsigned long long>(pausedSecondsRemaining) * 1000;
    if (visionBreakActive) visionBreakEndTick = now + pausedVisionBreakRemainingMs;
    else nextVisionBreakTick = now + pausedNextVisionBreakRemainingMs;
    pausedSecondsRemaining = 0;
    pausedNextVisionBreakRemainingMs = 0;
    pausedVisionBreakRemainingMs = 0;
    paused = false;
    running = true;
}

void IntervalTimer::SetVisionBreakEnabled(bool enabled) {
    if (visionBreakEnabled == enabled) return;
    visionBreakEnabled = enabled;
    if (!enabled) {
        if (visionBreakActive) AudioCue::PlayVisionBreakEnd();
        visionBreakActive = false;
        pausedNextVisionBreakRemainingMs = 0;
        pausedVisionBreakRemainingMs = 0;
        return;
    }

    if (!workPhase) return;
    if (paused) {
        pausedNextVisionBreakRemainingMs = kVisionBreakIntervalMs;
    } else if (running) {
        nextVisionBreakTick = GetTickCount64() + kVisionBreakIntervalMs;
    }
}

bool IntervalTimer::Tick() {
    if (!running) return false;

    const unsigned long long now = GetTickCount64();
    if (now < phaseEndTick && workPhase && visionBreakEnabled &&
        static_cast<unsigned long long>(workSeconds) * 1000 > kVisionBreakIntervalMs) {
        if (visionBreakActive && now >= visionBreakEndTick) {
            visionBreakActive = false;
            nextVisionBreakTick = now + kVisionBreakIntervalMs;
            AudioCue::PlayVisionBreakEnd();
        } else if (!visionBreakActive && now >= nextVisionBreakTick) {
            visionBreakActive = true;
            visionBreakEndTick = now + kVisionBreakDurationMs;
            AudioCue::PlayVisionBreakStart();
        }
    }
    if (now < phaseEndTick) return false;

    if (workPhase) {
        AudioCue::PlayTimerRestMelody();
        visionBreakActive = false;
        workPhase = false;
        phaseEndTick = now + static_cast<unsigned long long>(restSeconds) * 1000;
    } else {
        AudioCue::PlayTimerWorkMelody();
        ++completedRepetitions;
        if (completedRepetitions >= repetitions) {
            running = false;
        } else {
            workPhase = true;
            visionBreakActive = false;
            nextVisionBreakTick = now + kVisionBreakIntervalMs;
            phaseEndTick = now + static_cast<unsigned long long>(workSeconds) * 1000;
        }
    }
    return true;
}

bool IntervalTimer::IsRunning() const {
    return running;
}

bool IntervalTimer::IsActive() const {
    return running || paused;
}

bool IntervalTimer::IsPaused() const {
    return paused;
}

bool IntervalTimer::IsWorkPhase() const {
    return workPhase;
}

int IntervalTimer::SecondsRemaining() const {
    if (paused) return pausedSecondsRemaining;
    if (!running) return 0;
    const unsigned long long now = GetTickCount64();
    if (now >= phaseEndTick) return 0;
    return static_cast<int>((phaseEndTick - now + 999) / 1000);
}

bool IntervalTimer::IsVisionBreakActive() const {
    return visionBreakActive;
}

int IntervalTimer::CurrentRepetition() const {
    return (std::min)(completedRepetitions + 1, repetitions);
}

int IntervalTimer::RepetitionCount() const {
    return repetitions;
}

std::wstring FormatTimerTime(int seconds) {
    seconds = (std::max)(0, seconds);
    const int minutes = seconds / 60;
    const int remainingSeconds = seconds % 60;
    const std::wstring minuteText = minutes < 10 ? L"0" + std::to_wstring(minutes) : std::to_wstring(minutes);
    const std::wstring secondText = remainingSeconds < 10
        ? L"0" + std::to_wstring(remainingSeconds) : std::to_wstring(remainingSeconds);
    return minuteText + L":" + secondText;
}