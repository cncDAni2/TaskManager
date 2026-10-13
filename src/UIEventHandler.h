#pragma once

#include "AppTypes.h"
#include <string>

bool HandleKeyDown(HWND hWnd, WPARAM wParam);
bool HandleLButtonDown(HWND hWnd, LPARAM lParam);
bool HandleLButtonUp(HWND hWnd, LPARAM lParam);
bool HandleMouseMove(HWND hWnd, LPARAM lParam);
bool HandleCommand(HWND hWnd, int id);
bool HandleDrawItem(HWND hWnd, DRAWITEMSTRUCT* pDIS);
bool HandleAppTray(HWND hWnd, LPARAM lParam);
// Modal message box centered on `owner`; keeps focus-loss handling from hiding the window.
int ShowCenteredMessageBox(HWND owner, const std::wstring& text, const wchar_t* caption, UINT flags);
void SetWorkMeasurementStopped(bool stopped);
