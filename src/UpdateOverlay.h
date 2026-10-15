#pragma once

#include <windows.h>

// Full-window "Új verzió érhető el" cover for the main window and the PIN view, shown while
// a newer GitHub release is known. It swallows input; the X (or the download) hides it for
// the rest of the day.
namespace UpdateOverlay {
bool IsVisible();
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
