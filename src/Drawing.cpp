#include "Drawing.h"
#include <dwmapi.h>
#include <cmath>

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

void DrawMoreIcon(HDC hdc, const RECT& rc, COLORREF color) {
    int cx = rc.left + (rc.right - rc.left) / 2;
    int cy = rc.top + (rc.bottom - rc.top) / 2;
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    for (int offset : { -5, 0, 5 }) {
        Ellipse(hdc, cx - 2, cy + offset - 2, cx + 2, cy + offset + 2);
    }
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawFocusIcon(HDC hdc, const RECT& rc, COLORREF color) {
    int cx = rc.left + (rc.right - rc.left) / 2;
    int cy = rc.top + (rc.bottom - rc.top) / 2;
    HPEN hPen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ hOldPen = SelectObject(hdc, hPen);
    HGDIOBJ hOldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, cx - 6, cy - 6, cx + 7, cy + 7);
    Ellipse(hdc, cx - 2, cy - 2, cx + 3, cy + 3);
    MoveToEx(hdc, cx, cy - 9, nullptr);
    LineTo(hdc, cx, cy - 6);
    MoveToEx(hdc, cx, cy + 6, nullptr);
    LineTo(hdc, cx, cy + 9);
    MoveToEx(hdc, cx - 9, cy, nullptr);
    LineTo(hdc, cx - 6, cy);
    MoveToEx(hdc, cx + 6, cy, nullptr);
    LineTo(hdc, cx + 9, cy);
    SelectObject(hdc, hOldBrush);
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

void DrawSettingsIcon(HDC hdc, const RECT& rc, COLORREF color, COLORREF backgroundColor) {
    int cx = rc.left + (rc.right - rc.left) / 2;
    int cy = rc.top + (rc.bottom - rc.top) / 2;
    constexpr int toothCount = 8;
    constexpr double pi = 3.14159;
    constexpr double angleOffsets[] = { -22.5, -14.0, -14.0, 14.0, 14.0, 22.5 };
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    const int diameter = width < height ? width : height;
    const double outerRadius = diameter / 2.0 - 3.0;
    const double rootRadius = outerRadius * 0.72;
    const double radii[] = { rootRadius, rootRadius, outerRadius, outerRadius, rootRadius, rootRadius };
    POINT points[toothCount * 6]{};

    for (int tooth = 0; tooth < toothCount; ++tooth) {
        const double centerAngle = (-90.0 + tooth * 360.0 / toothCount) * pi / 180.0;
        for (int vertex = 0; vertex < 6; ++vertex) {
            const double angle = centerAngle + angleOffsets[vertex] * pi / 180.0;
            points[tooth * 6 + vertex] = {
                static_cast<LONG>(cx + std::lround(std::cos(angle) * radii[vertex])),
                static_cast<LONG>(cy + std::lround(std::sin(angle) * radii[vertex]))
            };
        }
    }

    HPEN hPen = CreatePen(PS_SOLID, 1, color);
    HBRUSH hBrush = CreateSolidBrush(color);
    HGDIOBJ hOldPen = SelectObject(hdc, hPen);
    HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);
    Polygon(hdc, points, _countof(points));

    HBRUSH hHoleBrush = CreateSolidBrush(backgroundColor);
    HGDIOBJ hGearBrush = SelectObject(hdc, hHoleBrush);
    const int holeRadius = static_cast<int>(std::lround(rootRadius * 0.45));
    Ellipse(hdc, cx - holeRadius, cy - holeRadius, cx + holeRadius + 1, cy + holeRadius + 1);

    SelectObject(hdc, hGearBrush);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}
