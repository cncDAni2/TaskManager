#pragma once

// WndProc dispatcher and window lifecycle (ShowAppWindow, HideAppWindow, ToggleWindow).
// MainWindow.cpp is oversized: put new behavior in its owning module.
// TrackPopupMenu causes the owner window to receive WM_ACTIVATE with WA_INACTIVE. Guard focus-loss
// handling with g_inContextMenu to avoid hiding the window or collapsing into mini mode while
// browsing the context menu.

#include "AppTypes.h"

void ToggleWindow();
void ShowAppWindow();
void HideAppWindow();
// Turns PIN mode on and shows the PIN window (from the tray, the hotkey or the full window).
void ShowPinWindow();
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
