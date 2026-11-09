#pragma once

// Keyboard/mouse handling, task reordering, commands and tray interactions.
// Main-window WM_DRAWITEM handling is centralized in HandleDrawItem. Match child control
// backgrounds to the parent ThemeColors surface and use the actual item rectangle; compile success
// does not verify alignment or appearance. Stateful owner-drawn toggles must update model state on
// BN_CLICKED and invalidate the control.

#include "AppTypes.h"
#include <string>

bool HandleKeyDown(HWND hWnd, WPARAM wParam);
bool HandleLButtonDown(HWND hWnd, LPARAM lParam);
bool HandleLButtonUp(HWND hWnd, LPARAM lParam);
bool HandleMouseMove(HWND hWnd, LPARAM lParam);
bool HandleCommand(HWND hWnd, int id);
// Paints what shows around a button's rounded corners; `pDIS->rcItem` still in real pixels.
void FillButtonSurface(const DRAWITEMSTRUCT* pDIS);
bool HandleDrawItem(HWND hWnd, DRAWITEMSTRUCT* pDIS);
bool HandleAppTray(HWND hWnd, LPARAM lParam);
// Modal message box centered on `owner`; keeps focus-loss handling from hiding the window.
int ShowCenteredMessageBox(HWND owner, const std::wstring& text, const wchar_t* caption, UINT flags);
void SetWorkMeasurementStopped(bool stopped);
