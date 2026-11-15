#pragma once

#include "AppTypes.h"

void DrawMoreIcon(HDC hdc, const RECT& rc, COLORREF color);
void DrawSearchIcon(HDC hdc, const RECT& rc, COLORREF color);
void DrawHiddenEyeIcon(HDC hdc, const RECT& rc, COLORREF color);
void DrawFocusIcon(HDC hdc, const RECT& rc, COLORREF color);
void DrawPinIcon(HDC hdc, const RECT& rc, COLORREF color, bool isPinned);
void DrawCloudIcon(HDC hdc, const RECT& rc, COLORREF color, bool filled = false);
void DrawSettingsIcon(HDC hdc, const RECT& rc, COLORREF color, COLORREF backgroundColor);
void DrawTimerIcon(HDC hdc, const RECT& rc, COLORREF color);
void DrawRepeatIcon(HDC hdc, const RECT& rc, COLORREF color);
void DrawDownloadIcon(HDC hdc, const RECT& rc, COLORREF color);
void DrawSpeakerIcon(HDC hdc, const RECT& rc, COLORREF color, bool muted);
void DrawGripIcon(HDC hdc, const RECT& rc, COLORREF color);
// Laura's sign (green L with a small bow), `size` units square; `crossed`: red X over it.
void DrawLauraIcon(HDC hdc, const RECT& rc, int size, bool crossed);
// Anti-aliased 20x20 icon: `drawShape` paints white on an 80x80 black canvas.
void DrawSmoothIcon(HDC hdc, const RECT& rc, COLORREF color, void (*drawShape)(HDC));
HICON GenerateAppIcon();
void ApplyDarkModeTitleBar(HWND hWnd, bool dark);
