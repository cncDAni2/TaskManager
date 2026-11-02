#pragma once

#include "BarTooltips.h"
#include <windows.h>
#include <array>
#include <string>
#include <vector>

// Quick buttons on the top strip of the PIN (mini) window: which ones show and in what order
// (`pin_buttons` in tasks-local.json, e.g. "timer,focus,-mute,-hold,-pin,drag"; "-" = hidden).
namespace PinButtons {
enum Id { Timer, Focus, Mute, Hold, Pin, Drag, COUNT };

struct Slot {
    Id id;
    bool enabled;
};
using Config = std::array<Slot, COUNT>;

// New users; a file from before this setting existed keeps the old strip (LEGACY_CONFIG).
constexpr char DEFAULT_CONFIG[] = "timer,-focus,-mute,-hold,pin,drag";
constexpr char LEGACY_CONFIG[] = "timer,focus,-mute,-hold,-pin,drag";
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
// Handles a click on a strip button; true when consumed.
bool HandleClick(HWND hWnd, POINT point, int clientWidth);
}
