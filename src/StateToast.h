#pragma once

// One-second click-through, non-activating confirmation after a hotkey or tray toggle of mute /
// focus mode (icon box blue when on, theme surface when off, plus text), or the Laura link found / lost; fade via layered
// alpha on a 16 ms timer that exists only while it is shown (0.25 s in, 0.5 s, 0.25 s out). Skipped
// when PinButtons::IsOnScreen (the PIN window shows that button); placed above the PIN window or
// the taskbar.

#include "PinButtons.h"

// One-second, click-through confirmation ("Hangok kikapcsolva", "Fókusz mód aktiválva") after a hotkey
// or tray toggle (also "Fókusz mód"): 0.25 s fade in, 0.5 s shown, 0.25 s fade out. Skipped while the PIN window
// shows that button, since its blue state already tells. Accepts PinButtons::Mute and ::Focus.
namespace StateToast {
void ShowToggled(PinButtons::Id id);
// "Laura kapcsolódott" with her sign, or "Elveszett a kapcsolat Laurával" with it crossed; held 3 s
// (LauraLink).
void ShowLaura(bool connected);
}
