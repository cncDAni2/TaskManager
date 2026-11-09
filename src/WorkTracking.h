#pragma once

// Work-time measurement on/off (work_tracking_enabled; Settings checkbox and the history-view
// footer button). Off skips the foreground-title check and accrual, hides the work bar in full and
// mini views (BottomBarHeight() / MiniBarHeight() return 0) and hides/ignores STRICT MODE; history
// is kept.
// The one-second IDT_WORK_TIMER drives work-time accrual (by real elapsed time via
// TakeCountedSeconds, since the timer itself runs late), idle/excluded-app checks and
// interval-timer ticks.

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

// Call on every tick: the whole seconds really elapsed since the last counted tick, so slow or
// late timer messages lose no work time. Sleep is not included and a long stall counts as 5 s.
int TakeCountedSeconds(bool counting);

void CreateButton(HWND parent, HINSTANCE instance);
// Places the footer buttons of the work-history view ("Mérés leállítása" and the on/off toggle).
void LayoutFooter(bool historyVisible);
bool HandleCommand(WORD id);
}
