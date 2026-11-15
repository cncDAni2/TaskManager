#pragma once

// PIN-window top strip (timer badge, new task, focus, mute, unpin, move handle, timer pause/next): configured
// order and visibility (pin_buttons, e.g. "timer,-add,focus,-mute,-pin,drag", - = hidden; a
// file without "add" gets it hidden right after the timer), right-aligned
// layout, rounded window-region tabs, painting (idle = no fill or border, on = blue), tooltips via
// BarTooltips, clicks (a double click on the move handle, in MainWindow, opens the main window).
// While the task list can be hidden (PinList mode other than always) and no button is on the strip, a status ring stands there instead: its tooltip tells how to leave PIN
// mode, a click opens the main window.
// The timer pause and next buttons press the timer view's Szünet/Folytatás and Következő buttons, or
// Start with the saved settings while the timer is not running; icons and presses: PinTimerButtons.h.

#include "BarTooltips.h"
#include <windows.h>
#include <array>
#include <string>
#include <vector>

// Quick buttons on the top strip of the PIN (mini) window: which ones show and in what order
// (`pin_buttons` in tasks-local.json, e.g. "timer,focus,-mute,-pin,drag"; "-" = hidden).
namespace PinButtons {
enum Id { Timer, Focus, Mute, Pin, Drag, Add, PlayPause, Next, COUNT,
    Status = COUNT }; // The status ring: never in the configuration.

struct Slot {
    Id id;
    bool enabled;
};
using Config = std::array<Slot, COUNT>;

// A file from before this setting existed keeps the old strip; new users get TaskStore::pin_buttons.
constexpr char LEGACY_CONFIG[] = "timer,-add,focus,-mute,-pin,drag";
// Top edge of the window body: the strip's tabs sit on it, the task list starts below.
constexpr int STRIP_BOTTOM = 20;

Config Parse(const std::string& text);
std::string Serialize(const Config& config);
Config Current();
// Saves the configuration and refreshes the PIN window if it is open.
void Apply(const Config& config);

const wchar_t* Name(Id id);
// True while the PIN window is on screen with this button switched on in the configuration.
bool IsOnScreen(Id id);
// Settings preview icon (the mute slot shows the muted speaker).
void DrawPreviewIcon(HDC hdc, const RECT& rc, Id id, COLORREF color);

// PIN window strip: visible buttons laid out right-to-left from the window's right edge.
bool GetRect(int clientWidth, Id id, RECT& rect);
void AddToRegion(HRGN region, int clientWidth);
void Paint(HDC hdc, int clientWidth);
void AddTooltips(std::vector<BarTooltips::Region>& regions, int clientWidth);
// True once each time the timer's run state (stopped / running / paused) differs from the last call:
// the pause and next icons then need a repaint.
bool TimerStateChanged();
// Handles a click on a strip button; true when consumed.
bool HandleClick(HWND hWnd, POINT point, int clientWidth);
}
