#pragma once

// The "Gyorsbillentyűk" window opened from Settings; suspends the hotkeys while open, requires a
// modifier, rejects combos that type a character (Shift / AltGr), duplicates and combos owned by
// other programs.

#include <windows.h>

// Modal "Gyorsbillentyűk" window opened from the Settings dialog. Global hotkeys are
// suspended while it is open so the combos can be pressed into the fields.
void ShowHotkeyDialog(HWND owner);
