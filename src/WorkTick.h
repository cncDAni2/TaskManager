#pragma once

// The work-time part of the one-second IDT_WORK_TIMER, without UI: idle (2 minutes without
// input) and excluded-window checks, whether the second counts as work, accrual into
// TaskStore::work_seconds_today by real elapsed time (the timer runs late; sleep is not counted
// and a stall counts as at most 5 s), saving every 10 counted minutes and when idle stops
// counting, and the interval timer's STRICT MODE pause, 20-20-20 screen clock and wait for the
// user after a rest. MainWindow applies the result to the globals and repaints.
// Time and input come from Clock.h, so tests can run whole work days through it.

#include <string>

class TaskStore;
class IntervalTimer;

class WorkTick {
public:
    struct Status {
        bool measurementStopped = false; // "Mérés leállítása" or STRICT MODE rest.
        bool sessionLocked = false;
        bool focusMode = false;
    };
    struct Result {
        bool idlePaused = false;
        bool countWork = false;
        bool historyChanged = false; // The work time crossed a 30-second step.
        bool timerResumed = false;   // The interval timer stopped waiting for the user.
    };
    // Reads the foreground window's title; only called while the title could pause counting.
    using TitleReader = void (*)(std::wstring& title);

    // Start of the tick: applies the 20-20-20 setting and steps the interval timer.
    // True on a phase change.
    static bool AdvanceTimer(const TaskStore& store, IntervalTimer& timer);
    // After a phase change in STRICT MODE: the rest phase stops work measurement, the next work
    // phase (running or waiting for the user) restarts it. False when it stays as it is.
    static bool StrictStopAfterPhaseChange(const TaskStore& store, const IntervalTimer& timer, bool& stop);

    // excludedName receives the matching keyword, empty when none.
    Result Run(TaskStore& store, IntervalTimer& timer, const Status& status, TitleReader readTitle,
        std::wstring& excludedName);

private:
    unsigned long long lastMs = 0;
    unsigned long long pendingMs = 0;
    bool wasIdleStopped = false;

    int TakeCountedSeconds(bool counting);
};
