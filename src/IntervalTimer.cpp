#include "IntervalTimer.h"
#include "AudioCue.h"
#include <windows.h>
#include <algorithm>

IntervalTimer g_intervalTimer;

namespace {
constexpr unsigned long long kVisionBreakIntervalMs = 20ULL * 60 * 1000;
constexpr unsigned long long kVisionBreakDurationMs = 25ULL * 1000;
}

// While held, every countdown reads the frozen hold moment instead of the clock.
unsigned long long IntervalTimer::Now() const {
    return held ? holdTick : GetTickCount64();
}

void IntervalTimer::Hold() {
    if (held) return;
    holdTick = GetTickCount64();
    held = true;
    if (visionBreakActive || visionBreakAwaitingInput) {
        visionBreakActive = false;
        visionBreakAwaitingInput = false;
        nextVisionBreakTick = holdTick + kVisionBreakIntervalMs;
    }
}

void IntervalTimer::Release() {
    if (!held) return;
    const unsigned long long now = GetTickCount64();
    phaseEndTick += now - holdTick;
    held = false;
    // The 20-20-20 cycle restarts with a full 20 minutes of screen time.
    visionBreakActive = false;
    visionBreakAwaitingInput = false;
    nextVisionBreakTick = now + kVisionBreakIntervalMs;
    if (visionBreakClockPaused) visionBreakPauseTick = now;
}

void IntervalTimer::Start(int newWorkSeconds, int newRestSeconds, int newRepetitions,
    bool newVisionBreakEnabled) {
    workSeconds = newWorkSeconds;
    restSeconds = newRestSeconds;
    repetitions = newRepetitions;
    completedRepetitions = 0;
    workPhase = true;
    paused = false;
    measurementPaused = false;
    awaitingActivity = false;
    pausedSecondsRemaining = 0;
    running = true;
    const unsigned long long now = Now();
    phaseEndTick = now + static_cast<unsigned long long>(workSeconds) * 1000;
    SetVisionBreakEnabled(newVisionBreakEnabled);
    UpdateVisionBreakClockPause();
}

void IntervalTimer::Stop() {
    running = false;
    paused = false;
    measurementPaused = false;
    awaitingActivity = false;
    pausedSecondsRemaining = 0;
    UpdateVisionBreakClockPause();
}

void IntervalTimer::Pause() {
    if (awaitingActivity) {
        awaitingActivity = false;
        paused = true;
        return;
    }
    if (!running) return;
    pausedSecondsRemaining = SecondsRemaining();
    running = false;
    paused = true;
    UpdateVisionBreakClockPause();
}

void IntervalTimer::Resume() {
    if (!paused) return;
    const unsigned long long now = Now();
    phaseEndTick = now + static_cast<unsigned long long>(pausedSecondsRemaining) * 1000;
    pausedSecondsRemaining = 0;
    paused = false;
    running = true;
    UpdateVisionBreakClockPause();
}

void IntervalTimer::PauseForMeasurement() {
    if (!running || !workPhase) return;
    pausedSecondsRemaining = SecondsRemaining();
    running = false;
    measurementPaused = true;
    UpdateVisionBreakClockPause();
}

void IntervalTimer::ResumeFromMeasurement() {
    if (!measurementPaused) return;
    const unsigned long long now = Now();
    phaseEndTick = now + static_cast<unsigned long long>(pausedSecondsRemaining) * 1000;
    pausedSecondsRemaining = 0;
    measurementPaused = false;
    running = true;
    UpdateVisionBreakClockPause();
}

void IntervalTimer::SetUserActive(bool active) {
    userActive = active;
    if (!active || !awaitingActivity || held) return;
    // The work countdown starts only once the user is back after a rest phase.
    phaseEndTick = Now() + static_cast<unsigned long long>(pausedSecondsRemaining) * 1000;
    pausedSecondsRemaining = 0;
    awaitingActivity = false;
    running = true;
}

void IntervalTimer::SetVisionBreakScreenActive(bool active) {
    if (visionBreakScreenActive == active) return;
    visionBreakScreenActive = active;
    UpdateVisionBreakClockPause();
}

void IntervalTimer::UpdateVisionBreakClockPause() {
    const bool shouldPause = visionBreakEnabled && !visionBreakScreenActive;
    if (shouldPause == visionBreakClockPaused) return;

    const unsigned long long now = Now();
    if (shouldPause) {
        visionBreakClockPaused = true;
        visionBreakPauseTick = now;
        return;
    }

    const unsigned long long pausedDuration = now - visionBreakPauseTick;
    if (visionBreakActive) visionBreakEndTick += pausedDuration;
    else nextVisionBreakTick += pausedDuration;
    visionBreakClockPaused = false;
    visionBreakPauseTick = 0;
}

void IntervalTimer::SetVisionBreakEnabled(bool enabled) {
    if (visionBreakEnabled == enabled) return;
    visionBreakEnabled = enabled;
    if (!enabled) {
        if (visionBreakActive) AudioCue::PlayVisionBreakEnd();
        visionBreakActive = false;
        visionBreakAwaitingInput = false;
        visionBreakClockPaused = false;
        visionBreakPauseTick = 0;
        return;
    }

    visionBreakAwaitingInput = false;
    nextVisionBreakTick = Now() + kVisionBreakIntervalMs;
    UpdateVisionBreakClockPause();
}

bool IntervalTimer::Tick() {
    if (held) return false;
    const unsigned long long now = Now();
    if (visionBreakEnabled && !visionBreakClockPaused) {
        if (visionBreakActive && now >= visionBreakEndTick) {
            // The next 20 minutes start only with the first input after the break.
            visionBreakActive = false;
            visionBreakAwaitingInput = true;
            visionBreakEndInputTick = GetTickCount();
            AudioCue::PlayVisionBreakEnd();
        } else if (visionBreakAwaitingInput) {
            LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) };
            if (GetLastInputInfo(&lii) &&
                static_cast<long>(lii.dwTime - visionBreakEndInputTick) > 0) {
                visionBreakAwaitingInput = false;
                nextVisionBreakTick = now + kVisionBreakIntervalMs;
            }
        } else if (!visionBreakActive && now >= nextVisionBreakTick) {
            visionBreakActive = true;
            visionBreakEndTick = now + kVisionBreakDurationMs;
            AudioCue::PlayVisionBreakStart();
        }
    }
    if (!running) return false;
    if (now < phaseEndTick) return false;

    return AdvancePhaseAt(now, true);
}

bool IntervalTimer::AdvancePhase() {
    if (!IsActive()) return false;
    const unsigned long long now = Now();
    // A manual step also restarts the 20-20-20 cycle with a full 20 minutes of screen time.
    visionBreakActive = false;
    visionBreakAwaitingInput = false;
    nextVisionBreakTick = now + kVisionBreakIntervalMs;
    if (visionBreakClockPaused) visionBreakPauseTick = now;
    return AdvancePhaseAt(now, false);
}

bool IntervalTimer::AdvancePhaseAt(unsigned long long now, bool holdWorkUntilActive) {
    paused = false;
    measurementPaused = false;
    awaitingActivity = false;
    running = true;
    pausedSecondsRemaining = 0;

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
            if (holdWorkUntilActive && !userActive) {
                running = false;
                awaitingActivity = true;
                pausedSecondsRemaining = workSeconds;
            } else {
                phaseEndTick = now + static_cast<unsigned long long>(workSeconds) * 1000;
            }
            UpdateVisionBreakClockPause();
        }
    }
    return true;
}

bool IntervalTimer::IsRunning() const {
    return running;
}

bool IntervalTimer::IsActive() const {
    return running || paused || measurementPaused || awaitingActivity;
}

bool IntervalTimer::IsPaused() const {
    return paused || measurementPaused;
}

bool IntervalTimer::IsHeld() const {
    return held;
}

bool IntervalTimer::IsAwaitingActivity() const {
    return awaitingActivity;
}

bool IntervalTimer::IsWorkPhase() const {
    return workPhase;
}

int IntervalTimer::SecondsRemaining() const {
    if (paused || measurementPaused || awaitingActivity) return pausedSecondsRemaining;
    if (!running) return 0;
    const unsigned long long now = Now();
    if (now >= phaseEndTick) return 0;
    return static_cast<int>((phaseEndTick - now + 999) / 1000);
}

bool IntervalTimer::IsVisionBreakActive() const {
    return visionBreakActive;
}

bool IntervalTimer::IsVisionBreakEnabled() const {
    return visionBreakEnabled;
}

int IntervalTimer::VisionBreakSecondsRemaining() const {
    if (!visionBreakEnabled) return 0;
    if (visionBreakAwaitingInput) return static_cast<int>(kVisionBreakIntervalMs / 1000);
    const unsigned long long now = visionBreakClockPaused ? visionBreakPauseTick : Now();
    const unsigned long long target = visionBreakActive ? visionBreakEndTick : nextVisionBreakTick;
    if (now >= target) return 0;
    return static_cast<int>((target - now + 999) / 1000);
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