#pragma once

namespace AudioCue {
void PlayTimerWorkMelody();
void PlayTimerRestMelody();
void PlayVisionBreakStart();
void PlayVisionBreakEnd();
// Mutes / unmutes every sound (the Settings "Hangok" option); hotkey and tray item.
void ToggleSounds();
}