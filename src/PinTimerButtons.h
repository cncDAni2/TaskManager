#pragma once

// Logic of the PIN strip's timer pause and next buttons, without UI (header only, tested): which
// icon each shows and which timer-view button a click presses. While the timer is not running
// (never started, stopped or finished) both show the restart icon and press Start; otherwise the
// pause button shows pause while paused (also when STRICT MODE paused it) and play while running.

#include "IntervalTimer.h"

namespace PinTimerButtons {
enum class Icon { Restart, Play, Pause, Next };
enum class Press { Start, PauseResume, Next };

inline Icon IconFor(bool nextButton, const IntervalTimer& timer) {
    if (!timer.IsActive()) return Icon::Restart;
    if (nextButton) return Icon::Next;
    return timer.IsPaused() ? Icon::Pause : Icon::Play;
}

inline Press PressFor(bool nextButton, const IntervalTimer& timer) {
    if (!timer.IsActive()) return Press::Start;
    return nextButton ? Press::Next : Press::PauseResume;
}
}
