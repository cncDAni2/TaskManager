#pragma once

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
