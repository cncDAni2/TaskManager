#pragma once

#include <windows.h>
#include <string>

// "Időstop": a special state that freezes work-time measurement, the interval
// timer and the 20-20-20 reminder without changing their own settings.
namespace TimerHold {
void CreateButton(HWND parent, HINSTANCE instance);
void UpdateVisibility(bool visible);
bool IsActive();
void Toggle();
bool HandleCommand(WORD id);
std::wstring MenuLabel();
// Re-reads the hotkey shown in the button tooltip.
void RefreshTooltip();
void DrawButton(const DRAWITEMSTRUCT* dis, COLORREF idleBg, COLORREF idleBorder, COLORREF idleIcon);
}
