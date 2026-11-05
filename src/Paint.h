#pragma once

#include "AppTypes.h"

// Only `dirty` (in units, see UiScale) is rendered: the back buffer covers just that rectangle.
void PaintMainWindow(HWND hWnd, HDC hdc, const RECT& dirty);
void PaintMiniWindow(HWND hWnd, HDC hdc, const RECT& dirty);
