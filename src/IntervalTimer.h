#pragma once

#include <string>

class IntervalTimer {
public:
    void Start(int workSeconds, int restSeconds, int repetitions, bool visionBreakEnabled);
    void Stop();
    void Pause();
    void Resume();
    void PauseForMeasurement();
    void ResumeFromMeasurement();
    void SetVisionBreakScreenActive(bool active);
    void SetVisionBreakEnabled(bool enabled);
    void SetUserActive(bool active);
    void Hold();
    void Release();
    bool Tick();
    bool AdvancePhase();

    bool IsRunning() const;
    bool IsActive() const;
    bool IsPaused() const;
    bool IsAwaitingActivity() const;
    bool IsHeld() const;
    bool IsWorkPhase() const;
    int SecondsRemaining() const;
    bool IsVisionBreakActive() const;
    bool IsVisionBreakEnabled() const;
    int VisionBreakSecondsRemaining() const;
    int CurrentRepetition() const;
    int RepetitionCount() const;

private:
    bool running = false;
    bool paused = false;
    bool measurementPaused = false;
    bool awaitingActivity = false;
    bool userActive = true;
    bool held = false;
    unsigned long long holdTick = 0;
    bool workPhase = true;
    int workSeconds = 50 * 60;
    int restSeconds = 10 * 60;
    int repetitions = 8;
    int completedRepetitions = 0;
    int pausedSecondsRemaining = 0;
    unsigned long long phaseEndTick = 0;
    bool visionBreakEnabled = false;
    bool visionBreakActive = false;
    bool visionBreakScreenActive = true;
    bool visionBreakClockPaused = false;
    bool visionBreakAwaitingInput = false;
    unsigned long visionBreakEndInputTick = 0;
    unsigned long long visionBreakPauseTick = 0;
    unsigned long long nextVisionBreakTick = 0;
    unsigned long long visionBreakEndTick = 0;

    unsigned long long Now() const;
    void UpdateVisionBreakClockPause();
    bool AdvancePhaseAt(unsigned long long now, bool holdWorkUntilActive);
};

extern IntervalTimer g_intervalTimer;
std::wstring FormatTimerTime(int seconds);