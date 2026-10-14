#pragma once

#include <windows.h>

// "Időstop": a special state that freezes work-time measurement, the interval
// timer and the 20-20-20 reminder without changing their own settings.
namespace TimerHold {
void CreateButton(HWND parent, HINSTANCE instance);
void UpdateVisibility(bool visible);
bool IsActive();
void Toggle();
bool HandleCommand(WORD id);
const wchar_t* MenuLabel();
void DrawButton(const DRAWITEMSTRUCT* dis, COLORREF idleBg, COLORREF idleBorder, COLORREF idleIcon);
}
