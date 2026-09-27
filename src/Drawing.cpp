#include "Drawing.h"
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

void ApplyDarkModeTitleBar(HWND hWnd, bool dark) {
    BOOL useDark = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
}

HICON GenerateAppIcon() {
    int cx = 32;
    int cy = 32;
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = cx;
    bi.bmiHeader.biHeight = -cy;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBmpColor = CreateDIBSection(hdcMem, &bi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    HBITMAP hBmpMask = CreateBitmap(cx, cy, 1, 1, nullptr);

    HGDIOBJ hOld = SelectObject(hdcMem, hBmpColor);

    HBRUSH hBrushBg = CreateSolidBrush(RGB(79, 70, 229));
    HPEN hPenBg = CreatePen(PS_SOLID, 1, RGB(67, 56, 202));
    SelectObject(hdcMem, hBrushBg);
    SelectObject(hdcMem, hPenBg);
    RoundRect(hdcMem, 2, 2, cx - 2, cy - 2, 10, 10);
    DeleteObject(hBrushBg);
    DeleteObject(hPenBg);

    HPEN hPenCheck = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
    SelectObject(hdcMem, hPenCheck);
    POINT pts[3] = { {8, 17}, {13, 23}, {24, 10} };
    Polyline(hdcMem, pts, 3);
    DeleteObject(hPenCheck);

    SelectObject(hdcMem, hOld);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmMask = hBmpMask;
    ii.hbmColor = hBmpColor;
    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hBmpColor);
    DeleteObject(hBmpMask);
    return hIcon;
}

void DrawPencilIcon(HDC hdc, const RECT& rc, COLORREF color) {
    int cx = rc.left + (rc.right - rc.left) / 2;
    int cy = rc.top + (rc.bottom - rc.top) / 2;

    HPEN hPen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ hOldPen = SelectObject(hdc, hPen);

    // Pencil barrel slanted from bottom-left to top-right
    MoveToEx(hdc, cx - 5, cy + 4, nullptr);
    LineTo(hdc, cx + 3, cy - 4);

    MoveToEx(hdc, cx - 3, cy + 6, nullptr);
    LineTo(hdc, cx + 5, cy - 2);

    // Tip
    MoveToEx(hdc, cx - 6, cy + 6, nullptr);
    LineTo(hdc, cx - 5, cy + 4);

    // Eraser cap top
    MoveToEx(hdc, cx + 3, cy - 4, nullptr);
    LineTo(hdc, cx + 5, cy - 2);

    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

void DrawPinIcon(HDC hdc, const RECT& rc, COLORREF color, bool /*isPinned*/) {
    int cx = rc.left + (rc.right - rc.left) / 2;
    int cy = rc.top + (rc.bottom - rc.top) / 2;

    HPEN hPen = CreatePen(PS_SOLID, 2, color);
    HBRUSH hBrush = CreateSolidBrush(color);
    HGDIOBJ hOldPen = SelectObject(hdc, hPen);
    HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);

    // Needle pointing downwards
    HPEN hNeedlePen = CreatePen(PS_SOLID, 1, color);
    SelectObject(hdc, hNeedlePen);
    MoveToEx(hdc, cx, cy + 2, nullptr);
    LineTo(hdc, cx, cy + 8);
    SelectObject(hdc, hPen);
    DeleteObject(hNeedlePen);

    // Base collar
    MoveToEx(hdc, cx - 5, cy + 2, nullptr);
    LineTo(hdc, cx + 6, cy + 2);

    // Body (tapered)
    POINT bodyPts[4] = {
        { cx - 4, cy + 2 },
        { cx - 2, cy - 3 },
        { cx + 3, cy - 3 },
        { cx + 5, cy + 2 }
    };
    Polygon(hdc, bodyPts, 4);

    // Head cap
    RoundRect(hdc, cx - 5, cy - 6, cx + 6, cy - 2, 2, 2);

    // Top knob
    RoundRect(hdc, cx - 2, cy - 8, cx + 3, cy - 5, 2, 2);

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBrush);
    DeleteObject(hPen);
}

void DrawCloudIcon(HDC hdc, const RECT& rc, COLORREF color, bool filled) {
    int cx = rc.left + (rc.right - rc.left) / 2;
    int cy = rc.top + (rc.bottom - rc.top) / 2;

    HPEN hPen = CreatePen(PS_SOLID, 1, color);
    HBRUSH hBrush = filled ? CreateSolidBrush(color) : (HBRUSH)GetStockObject(NULL_BRUSH);
    HGDIOBJ hOldPen = SelectObject(hdc, hPen);
    HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);

    // Pill base
    RoundRect(hdc, cx - 7, cy, cx + 8, cy + 6, 4, 4);
    // Left puff
    Ellipse(hdc, cx - 6, cy - 3, cx + 1, cy + 4);
    // Center puff (taller)
    Ellipse(hdc, cx - 3, cy - 6, cx + 4, cy + 3);
    // Right puff
    Ellipse(hdc, cx + 1, cy - 2, cx + 8, cy + 4);

    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
    if (filled) DeleteObject(hBrush);
}

void DrawFolderCloudIcon(HDC hdc, const RECT& rc, COLORREF color) {
    int cx = rc.left + (rc.right - rc.left) / 2;
    int cy = rc.top + (rc.bottom - rc.top) / 2;

    HPEN hPen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ hOldPen = SelectObject(hdc, hPen);
    HGDIOBJ hOldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

    // Folder tab
    POINT tabPts[4] = {
        { cx - 8, cy - 2 },
        { cx - 8, cy - 6 },
        { cx - 3, cy - 6 },
        { cx - 1, cy - 2 }
    };
    Polyline(hdc, tabPts, 4);

    // Folder body
    RoundRect(hdc, cx - 8, cy - 2, cx + 7, cy + 8, 2, 2);

    // Small cloud on folder
    RECT rcCloud = { cx - 3, cy, cx + 7, cy + 7 };
    DrawCloudIcon(hdc, rcCloud, color, true);

    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
}
