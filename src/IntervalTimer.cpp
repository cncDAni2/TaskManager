#include "IntervalTimer.h"
#include "AudioCue.h"
#include <windows.h>
#include <algorithm>

IntervalTimer g_intervalTimer;

void IntervalTimer::Start(int newWorkSeconds, int newRestSeconds, int newRepetitions) {
    workSeconds = newWorkSeconds;
    restSeconds = newRestSeconds;
    repetitions = newRepetitions;
    completedRepetitions = 0;
    workPhase = true;
    paused = false;
    pausedSecondsRemaining = 0;
    running = true;
    phaseEndTick = GetTickCount64() + static_cast<unsigned long long>(workSeconds) * 1000;
}

void IntervalTimer::Stop() {
    running = false;
    paused = false;
    pausedSecondsRemaining = 0;
}

void IntervalTimer::Pause() {
    if (!running) return;
    pausedSecondsRemaining = SecondsRemaining();
    running = false;
    paused = true;
}

void IntervalTimer::Resume() {
    if (!paused) return;
    phaseEndTick = GetTickCount64() + static_cast<unsigned long long>(pausedSecondsRemaining) * 1000;
    pausedSecondsRemaining = 0;
    paused = false;
    running = true;
}

bool IntervalTimer::Tick() {
    if (!running || GetTickCount64() < phaseEndTick) return false;

    const unsigned long long now = GetTickCount64();
    if (workPhase) {
        AudioCue::PlayTimerRestMelody();
        workPhase = false;
        phaseEndTick = now + static_cast<unsigned long long>(restSeconds) * 1000;
    } else {
        AudioCue::PlayTimerWorkMelody();
        ++completedRepetitions;
        if (completedRepetitions >= repetitions) {
            running = false;
        } else {
            workPhase = true;
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