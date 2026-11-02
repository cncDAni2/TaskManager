#pragma once

#include "AppTypes.h"

void ToggleWindow();
void ShowAppWindow();
void HideAppWindow();
// Turns PIN mode on and shows the PIN window (from the tray, the hotkey or the full window).
void ShowPinWindow();
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
