#pragma once

// The Settings icon row for PinButtons (click toggles, drag or Ctrl+Left/Right reorders, applies
// instantly).

#include <windows.h>

// Settings row of the PIN window's quick buttons: click toggles one, dragging reorders them.
// Keyboard: Left/Right selects, Space toggles, Ctrl+Left/Right moves. Changes apply at once.
namespace PinButtonsEditor {
HWND Create(HWND parent, int x, int y, int id, COLORREF background);
}
