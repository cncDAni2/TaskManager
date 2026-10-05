#pragma once

#include <windows.h>

namespace TimerView {
void CreateControls(HWND owner, HINSTANCE instance);
void UpdateVisibility(bool visible);
void RefreshRunState();
void HandlePhaseChange();
void StartConfiguredTimer();
void Layout(HWND owner);
bool HandleCommand(HWND owner, WPARAM wParam);
}