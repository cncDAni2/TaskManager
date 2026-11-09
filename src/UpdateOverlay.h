#pragma once

// Full-window "Új verzió érhető el" cover (the PIN window grows to fit it) while a newer release is
// known; swallows input and hides child controls; X hides it for the day; "Frissítés" starts
// SelfUpdate; a failure shows its reason and "Újrapróba"; after repeated failures the button
// ("Böngészőben") opens the download in the browser.

#include <windows.h>

// Full-window "Új verzió érhető el" cover for the main window and the PIN view, shown while
// a newer GitHub release is known. It swallows input; the X hides it for the rest of the day.
// "Frissítés" updates in place (SelfUpdate); a failure shows its reason with "Újrapróba", and
// after SelfUpdate::BROWSER_AFTER failures the button opens the download page instead.
namespace UpdateOverlay {
// Height (units, below the PIN button strip) of the full layout; the PIN window is made at least
// this tall while the overlay shows, so only a tiny work area falls back to the compact layout.
constexpr int FULL_LAYOUT_MIN_HEIGHT = 240;
bool IsVisible();
// Opens the release download in the browser and hides the overlay for the day.
void OpenDownloadPage();
// Re-applies child-control visibility and repaints after the state may have changed.
void Refresh();
// Hides the main window's child controls while the overlay covers it.
void HideChildren(HWND parent);

void Paint(HDC hdc, const RECT& client);
// Mouse handling while visible; every message is consumed.
bool HandleLButtonDown(HWND hWnd, LPARAM lParam);
bool HandleMouseMove(HWND hWnd, LPARAM lParam);
bool HandleSetCursor(HWND hWnd);
}
