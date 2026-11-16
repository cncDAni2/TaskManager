#pragma once

// Generated WAV cues and asynchronous playback on the thread pool. A shared atomic flag prevents
// overlapping cues, and cues requested while another is playing are dropped rather than queued.
// While Laura is connected (LauraLink), the timer and 20-20-20 cues are spoken sentences through her
// instead, and nothing beeps: no melody fallback, no system beeps or message box sounds (BeepsAllowed).

namespace AudioCue {
void PlayTimerWorkMelody();
void PlayTimerRestMelody();
void PlayVisionBreakStart();
void PlayVisionBreakEnd();
// "Ti-tü" (high, then low): the link to Laura was lost.
void PlayLinkLost();
// System beeps and message box icon sounds may play: sounds on and Laura not connected.
bool BeepsAllowed();
// Mutes / unmutes every sound (the Settings "Hangok" option); hotkey and tray item.
void ToggleSounds();
}