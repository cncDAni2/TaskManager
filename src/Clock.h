#pragma once

// Time and input sources of the timed logic (IntervalTimer, WorkTick). The test build
// (TASKMANAGER_TEST) replaces them with a hand-driven clock, so tests can run whole work days
// through the timers without waiting.

#include <windows.h>

namespace Clock {
#ifdef TASKMANAGER_TEST
extern unsigned long long testMs;   // Every clock below reads this.
extern DWORD testLastInputTick;     // The last user input, in InputTick() units.

inline unsigned long long Ms() { return testMs; }
inline unsigned long long UnbiasedMs() { return testMs; }
inline DWORD InputTick() { return static_cast<DWORD>(testMs); }
inline bool LastInputTick(DWORD& tick) { tick = testLastInputTick; return true; }
#else
// Milliseconds since boot.
inline unsigned long long Ms() { return GetTickCount64(); }
// Milliseconds that stop during sleep and hibernation.
inline unsigned long long UnbiasedMs() {
    ULONGLONG interruptTime = 0; // 100 ns units.
    QueryUnbiasedInterruptTime(&interruptTime);
    return interruptTime / 10000;
}
// The 32-bit tick that LastInputTick() is measured in.
inline DWORD InputTick() { return GetTickCount(); }
inline bool LastInputTick(DWORD& tick) {
    LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) };
    if (!GetLastInputInfo(&lii)) return false;
    tick = lii.dwTime;
    return true;
}
#endif
}
