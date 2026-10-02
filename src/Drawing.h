#pragma once

#include "AppTypes.h"

void DrawPencilIcon(HDC hdc, const RECT& rc, COLORREF color);
void DrawMoreIcon(HDC hdc, const RECT& rc, COLORREF color);
void DrawFocusIcon(HDC hdc, const RECT& rc, COLORREF color);
void DrawPinIcon(HDC hdc, const RECT& rc, COLORREF color, bool isPinned);
void DrawCloudIcon(HDC hdc, const RECT& rc, COLORREF color, bool filled = false);
void DrawFolderCloudIcon(HDC hdc, const RECT& rc, COLORREF color);
void DrawSettingsIcon(HDC hdc, const RECT& rc, COLORREF color, COLORREF backgroundColor);
HICON GenerateAppIcon();
void ApplyDarkModeTitleBar(HWND hWnd, bool dark);
