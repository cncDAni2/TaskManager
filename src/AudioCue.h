#pragma once

// Generated WAV cues and asynchronous playback on the thread pool. A shared atomic flag prevents
// overlapping cues, and cues requested while another is playing are dropped rather than queued.

namespace AudioCue {
void PlayTimerWorkMelody();
void PlayTimerRestMelody();
void PlayVisionBreakStart();
void PlayVisionBreakEnd();
// Mutes / unmutes every sound (the Settings "Hangok" option); hotkey and tray item.
void ToggleSounds();
}