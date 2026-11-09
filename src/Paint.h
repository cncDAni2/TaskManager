#pragma once

// Double-buffered GDI rendering for full/mini views and measured/manual work bars. Only ps.rcPaint
// is rendered: the back buffer (UiScale::Buffer) covers just that, and the list is skipped when
// outside it (its tooltip regions are reused).
// - Suppress WM_ERASEBKGND (return 1) and avoid unnecessary invalidation; use dirty rectangles for
//   small indicator updates (two rectangles far apart merge into one large repaint unless
//   UpdateWindow separates them).
// - The PIN window's work bar repaints only when MiniWorkBarChanged says so (once a minute or on a
//   state change).

#include "AppTypes.h"

// Only `dirty` (in units, see UiScale) is rendered: the back buffer covers just that rectangle.
void PaintMainWindow(HWND hWnd, HDC hdc, const RECT& dirty);
void PaintMiniWindow(HWND hWnd, HDC hdc, const RECT& dirty);
// True when the PIN window's work bar (or its tooltip) has something new to show: once a minute.
bool MiniWorkBarChanged();
