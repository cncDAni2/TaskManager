#pragma once

#include <windows.h>

// Modal "Gyorsbillentyűk" window opened from the Settings dialog. Global hotkeys are
// suspended while it is open so the combos can be pressed into the fields.
void ShowHotkeyDialog(HWND owner);
