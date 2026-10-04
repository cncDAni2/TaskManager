#pragma once

#include <string>

class IntervalTimer {
public:
    void Start(int workSeconds, int restSeconds, int repetitions, bool visionBreakEnabled);
    void Stop();
    void Pause();
    void Resume();
    bool Tick();

    bool IsRunning() const;
    bool IsActive() const;
    bool IsPaused() const;
    bool IsWorkPhase() const;
    int SecondsRemaining() const;
    bool IsVisionBreakActive() const;
    int CurrentRepetition() const;
    int RepetitionCount() const;

private:
    bool running = false;
    bool paused = false;
    bool workPhase = true;
    int workSeconds = 50 * 60;
    int restSeconds = 10 * 60;
    int repetitions = 8;
    int completedRepetitions = 0;
    int pausedSecondsRemaining = 0;
    unsigned long long phaseEndTick = 0;
    bool visionBreakEnabled = false;
    bool visionBreakActive = false;
    unsigned long long phaseStartedTick = 0;
    unsigned long long nextVisionBreakTick = 0;
    unsigned long long visionBreakEndTick = 0;
    unsigned long long pausedPhaseElapsedMs = 0;
    unsigned long long pausedVisionBreakRemainingMs = 0;
};

extern IntervalTimer g_intervalTimer;
std::wstring FormatTimerTime(int seconds);