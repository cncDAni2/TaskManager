#pragma once

#include "PinButtons.h"

// One-second, click-through confirmation ("Időstop aktiválva", "Hangok kikapcsolva") after a hotkey
// or tray toggle (also "Fókusz mód"): 0.25 s fade in, 0.5 s shown, 0.25 s fade out. Skipped while the PIN window
// shows that button, since its blue state already tells. Accepts PinButtons::Mute, ::Hold and ::Focus.
namespace StateToast {
void ShowToggled(PinButtons::Id id);
}
