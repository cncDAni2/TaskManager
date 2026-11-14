#pragma once

// PIN window task list display (pin_list_mode, pin_list_count in tasks-local.json; Settings row
// "PIN feladatlista"): never, while the mouse is over the window, or always; and how many tasks of
// the PIN order show (0 = all). In the mouse mode the list opens on the first mouse move over the
// window (the window grows upwards, it is anchored at the bottom) and closes 2 s after the mouse
// left it (one-shot IDT_PIN_LIST_HIDE, re-armed while a drag still runs). Without the list only the
// top strip and the work bar show; PinButtons draws a status ring when the strip would be empty.

#include <windows.h>

namespace PinList {
enum Mode { Never = 0, OnHover = 1, Always = 2 };

Mode CurrentMode();
// The first this many tasks of the PIN order show; 0 = all.
int Limit();
// True while the PIN window shows its task list.
bool IsShown();

// Saves the settings and refreshes the PIN window if it is open.
void Apply(Mode mode, int limit);
// The PIN window opens or closes: the mouse mode starts with the list closed.
void Reset(HWND hWnd);

// PIN window messages.
void HandleMouseMove(HWND hWnd);
void HandleMouseLeave(HWND hWnd);
bool HandleTimer(HWND hWnd, WPARAM timerId);
}
