#include "WorkTick.h"
#include "Clock.h"
#include "ExcludedApps.h"
#include "IntervalTimer.h"
#include "Task.h"
#include <algorithm>

namespace {
constexpr DWORD IDLE_MS = 120000;
constexpr unsigned long long MAX_TICK_MS = 5000;

bool StrictModeActive(const TaskStore& store) {
    return store.timer_strict_mode && store.work_tracking_enabled;
}
}

bool WorkTick::AdvanceTimer(const TaskStore& store, IntervalTimer& timer) {
    timer.SetVisionBreakEnabled(store.timer_2020_enabled);
    return timer.Tick();
}

bool WorkTick::StrictStopAfterPhaseChange(const TaskStore& store, const IntervalTimer& timer, bool& stop) {
    if (!StrictModeActive(store)) return false;
    if (!timer.IsWorkPhase()) {
        stop = true;
        return true;
    }
    if (!timer.IsRunning() && !timer.IsAwaitingActivity()) return false;
    stop = false;
    return true;
}

int WorkTick::TakeCountedSeconds(bool counting) {
    const unsigned long long nowMs = Clock::UnbiasedMs();
    const unsigned long long elapsedMs = lastMs != 0 ? nowMs - lastMs : 1000;
    lastMs = nowMs;
    if (!counting) {
        pendingMs = 0;
        return 0;
    }
    pendingMs += (std::min)(elapsedMs, MAX_TICK_MS);
    const int seconds = static_cast<int>(pendingMs / 1000);
    pendingMs %= 1000;
    return seconds;
}

WorkTick::Result WorkTick::Run(TaskStore& store, IntervalTimer& timer, const Status& status,
    TitleReader readTitle, std::wstring& excludedName) {
    Result result;
    DWORD lastInput = 0;
    Clock::LastInputTick(lastInput);
    const DWORD nowTick = Clock::InputTick();
    const DWORD idleMs = nowTick >= lastInput ? nowTick - lastInput : 0;
    result.idlePaused = idleMs >= IDLE_MS;

    excludedName.clear();
    const bool tracking = store.work_tracking_enabled;
    // The title only matters while it could pause counting.
    const bool titleMatters = tracking && !status.measurementStopped && !status.sessionLocked &&
        !status.focusMode && !result.idlePaused;
    if (titleMatters && readTitle) {
        std::wstring title;
        readTitle(title);
        excludedName = ExcludedApps::FindMatch(store.excluded_window_keywords, title);
    }

    result.countWork = tracking && !status.measurementStopped && !status.sessionLocked &&
        (status.focusMode || (!result.idlePaused && excludedName.empty()));

    const int countedSeconds = TakeCountedSeconds(result.countWork);
    if (countedSeconds > 0) {
        const int workBefore = store.work_seconds_today;
        store.work_seconds_today += countedSeconds;
        const int workNow = store.work_seconds_today;
        if (workBefore / 600 != workNow / 600) store.Save();
        result.historyChanged = workBefore / 30 != workNow / 30;
    }

    // Besides every 10 counted minutes, work time is saved when counting stops.
    const bool idleStopped = result.idlePaused && !result.countWork;
    if (idleStopped && !wasIdleStopped) store.Save();
    wasIdleStopped = idleStopped;

    const bool pauseStrictWorkTimer = tracking && store.timer_strict_mode &&
        timer.IsActive() && timer.IsWorkPhase() &&
        (status.measurementStopped || status.sessionLocked || (result.idlePaused && !status.focusMode));
    if (pauseStrictWorkTimer) timer.PauseForMeasurement();
    else timer.ResumeFromMeasurement();
    const bool userActive = !status.sessionLocked && (status.focusMode || !result.idlePaused);
    timer.SetVisionBreakScreenActive(userActive);
    const bool timerWasAwaiting = timer.IsAwaitingActivity();
    timer.SetUserActive(userActive);
    result.timerResumed = timerWasAwaiting && !timer.IsAwaitingActivity();
    return result;
}
