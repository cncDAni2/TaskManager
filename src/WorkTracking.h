#pragma once

// Work-time measurement on/off (work_tracking_enabled; Settings checkbox and the history-view
// footer button). Off skips the foreground-title check and accrual, hides the work bar in full and
// mini views (BottomBarHeight() / MiniBarHeight() return 0) and hides/ignores STRICT MODE; history
// is kept.
// The accrual itself, the idle/excluded-app checks and the interval-timer coupling of the
// one-second IDT_WORK_TIMER are in WorkTick.

#include <windows.h>

// Switches work-time measurement on or off (Settings and the work-history view). When off, no
// idle/excluded-window checks run for it, the work bar is hidden in both the full and the mini
// view, and STRICT MODE is ignored; recorded history stays untouched.
namespace WorkTracking {
bool IsEnabled();
void SetEnabled(bool enabled);

// Height of the work bar at the bottom of the full window / the mini view; 0 when off.
int BottomBarHeight();
int MiniBarHeight();

void CreateButton(HWND parent, HINSTANCE instance);
// Places the footer buttons of the work-history view ("Mérés leállítása" and the on/off toggle).
void LayoutFooter(bool historyVisible);
bool HandleCommand(WORD id);
}
