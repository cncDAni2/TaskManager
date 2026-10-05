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
    void SetWorkMeasurementActive(bool active);
    void SetVisionBreakEnabled(bool enabled);
    bool Tick();
    bool AdvancePhase();

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
    bool measurementPaused = false;
    bool workPhase = true;
    int workSeconds = 50 * 60;
    int restSeconds = 10 * 60;
    int repetitions = 8;
    int completedRepetitions = 0;
    int pausedSecondsRemaining = 0;
    unsigned long long phaseEndTick = 0;
    bool visionBreakEnabled = false;
    bool visionBreakActive = false;
    bool workMeasurementActive = true;
    bool visionBreakClockPaused = false;
    unsigned long long visionBreakPauseTick = 0;
    unsigned long long nextVisionBreakTick = 0;
    unsigned long long visionBreakEndTick = 0;

    void UpdateVisionBreakClockPause();
    bool AdvancePhaseAt(unsigned long long now);
};

extern IntervalTimer g_intervalTimer;
std::wstring FormatTimerTime(int seconds);