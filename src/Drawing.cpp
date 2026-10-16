#include "Drawing.h"
#include <dwmapi.h>
#include <algorithm>
#include <cmath>
#include <utility>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

void ApplyDarkModeTitleBar(HWND hWnd, bool dark) {
    BOOL useDark = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
}

namespace {
constexpr int kSize = 20;
constexpr int kScale = 4;
constexpr int kBig = kSize * kScale;

// GDI has no anti-aliasing: `drawShape` paints a white shape on an 80x80 black canvas, which is
// shrunk with HALFTONE averaging and used as coverage to blend `color` over the 20x20 target.
void DrawSmoothShape(HDC hdc, const RECT& rc, COLORREF color, void (*drawShape)(HDC)) {
    const int left = rc.left + (rc.right - rc.left - kSize) / 2;
    const int top = rc.top + (rc.bottom - rc.top - kSize) / 2;

    auto createDib = [hdc](int size, HDC& dc, DWORD*& bits) {
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = size;
        bi.bmiHeader.biHeight = -size;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* raw = nullptr;
        HBITMAP bmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &raw, nullptr, 0);
        dc = CreateCompatibleDC(hdc);
        bits = static_cast<DWORD*>(raw);
        return std::make_pair(bmp, SelectObject(dc, bmp));
    };

    HDC bigDc = nullptr, maskDc = nullptr, destDc = nullptr;
    DWORD* bigBits = nullptr;
    DWORD* maskBits = nullptr;
    DWORD* destBits = nullptr;
    auto big = createDib(kBig, bigDc, bigBits);
    auto mask = createDib(kSize, maskDc, maskBits);
    auto dest = createDib(kSize, destDc, destBits);

    if (bigBits && maskBits && destBits) {
        drawShape(bigDc);

        SetStretchBltMode(maskDc, HALFTONE);
        SetBrushOrgEx(maskDc, 0, 0, nullptr);
        StretchBlt(maskDc, 0, 0, kSize, kSize, bigDc, 0, 0, kBig, kBig, SRCCOPY);
        GdiFlush();
        BitBlt(destDc, 0, 0, kSize, kSize, hdc, left, top, SRCCOPY);
        GdiFlush();

        const int r = GetRValue(color), g = GetGValue(color), b = GetBValue(color);
        for (int i = 0; i < kSize * kSize; ++i) {
            const int a = maskBits[i] & 0xFF;
            if (a == 0) continue;
            const DWORD px = destBits[i];
            const int db = px & 0xFF, dg = (px >> 8) & 0xFF, dr = (px >> 16) & 0xFF;
            destBits[i] = static_cast<DWORD>(((dr * (255 - a) + r * a) / 255) << 16 |
                ((dg * (255 - a) + g * a) / 255) << 8 | ((db * (255 - a) + b * a) / 255));
        }
        BitBlt(hdc, left, top, kSize, kSize, destDc, 0, 0, SRCCOPY);
    }

    auto release = [](HDC dc, const std::pair<HBITMAP, HGDIOBJ>& dib) {
        if (dc) {
            SelectObject(dc, dib.second);
            DeleteDC(dc);
        }
        if (dib.first) DeleteObject(dib.first);
    };
    release(bigDc, big);
    release(maskDc, mask);
    release(destDc, dest);
}
}

void DrawHandIcon(HDC hdc, const RECT& rc, COLORREF color) {
    DrawSmoothShape(hdc, rc, color, [](HDC bigDc) {
        LOGBRUSH lb{ BS_SOLID, RGB(255, 255, 255), 0 };
        HPEN fingerPen = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND, 11, &lb, 0, nullptr);
        HGDIOBJ oldPen = SelectObject(bigDc, fingerPen);
        const POINT fingers[4][2] = {
            { { 12, 30 }, { 12, 52 } }, { { 28, 14 }, { 28, 52 } },
            { { 44, 11 }, { 44, 52 } }, { { 60, 18 }, { 60, 52 } } };
        for (const auto& finger : fingers) {
            MoveToEx(bigDc, finger[0].x, finger[0].y, nullptr);
            LineTo(bigDc, finger[1].x, finger[1].y);
        }
        MoveToEx(bigDc, 56, 64, nullptr);
        LineTo(bigDc, 73, 43);
        SelectObject(bigDc, GetStockObject(NULL_PEN));
        HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
        HGDIOBJ oldBrush = SelectObject(bigDc, white);
        RoundRect(bigDc, 6, 40, 67, 79, 30, 30);
        Rectangle(bigDc, 6, 40, 67, 60);
        // Straight edge from the thumb's underside, tangent to the palm's rounded corner.
        const POINT thumbBase[] = { { 73, 43 }, { 77, 47 }, { 66, 71 }, { 52, 64 } };
        Polygon(bigDc, thumbBase, 4);
        SelectObject(bigDc, oldBrush);
        SelectObject(bigDc, oldPen);
        DeleteObject(white);
        DeleteObject(fingerPen);
    });
}

// Two arcs chasing each other clockwise, each ending in an arrowhead.
void DrawRepeatIcon(HDC hdc, const RECT& rc, COLORREF color) {
    DrawSmoothShape(hdc, rc, color, [](HDC bigDc) {
        constexpr double pi = 3.14159265358979323846;
        constexpr double cx = 40.0, cy = 40.0, radius = 26.0;
        LOGBRUSH lb{ BS_SOLID, RGB(255, 255, 255), 0 };
        HPEN pen = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT, 9, &lb, 0, nullptr);
        HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
        HGDIOBJ oldPen = SelectObject(bigDc, pen);
        HGDIOBJ oldBrush = SelectObject(bigDc, white);
        // Angles grow clockwise on screen (y points down).
        for (double startDeg : { 195.0, 15.0 }) {
            const double endDeg = startDeg + 125.0;
            POINT arc[24];
            for (int i = 0; i < 24; ++i) {
                const double a = (startDeg + (endDeg - startDeg) * i / 23.0) * pi / 180.0;
                arc[i] = { static_cast<LONG>(std::lround(cx + radius * std::cos(a))),
                    static_cast<LONG>(std::lround(cy + radius * std::sin(a))) };
            }
            Polyline(bigDc, arc, 24);
            const double e = endDeg * pi / 180.0;
            const double ex = cx + radius * std::cos(e), ey = cy + radius * std::sin(e);
            const double tx = -std::sin(e), ty = std::cos(e); // Clockwise tangent.
            const double nx = std::cos(e), ny = std::sin(e);  // Outward normal.
            SelectObject(bigDc, GetStockObject(NULL_PEN));
            const POINT head[3] = {
                { static_cast<LONG>(std::lround(ex + tx * 16)), static_cast<LONG>(std::lround(ey + ty * 16)) },
                { static_cast<LONG>(std::lround(ex + nx * 14 - tx * 2)), static_cast<LONG>(std::lround(ey + ny * 14 - ty * 2)) },
                { static_cast<LONG>(std::lround(ex - nx * 14 - tx * 2)), static_cast<LONG>(std::lround(ey - ny * 14 - ty * 2)) } };
            Polygon(bigDc, head, 3);
            SelectObject(bigDc, pen);
        }
        SelectObject(bigDc, oldBrush);
        SelectObject(bigDc, oldPen);
        DeleteObject(white);
        DeleteObject(pen);
    });
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
    constexpr int toothCount = 8;
    constexpr int verticesPerTooth = 5;
    constexpr double pi = 3.14159265358979323846;
    // Angles are relative to each tooth center: root-start, tip-start, tip-end, root-end, gap midpoint.
    constexpr double angleOffsets[verticesPerTooth] = { -19.0, -10.0, 10.0, 19.0, 22.5 };
    constexpr double radiusFactors[verticesPerTooth] = { 0.7, 1.0, 1.0, 0.7, 0.7 };

    const int cx = rc.left + (rc.right - rc.left) / 2;
    const int cy = rc.top + (rc.bottom - rc.top) / 2;
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    const int diameter = width < height ? width : height;
    const double outerRadius = diameter / 2.0 - 4.0;

    POINT points[toothCount * verticesPerTooth]{};
    for (int tooth = 0; tooth < toothCount; ++tooth) {
        const double centerAngle = -90.0 + tooth * 360.0 / toothCount;
        for (int vertex = 0; vertex < verticesPerTooth; ++vertex) {
            const double angle = (centerAngle + angleOffsets[vertex]) * pi / 180.0;
            const double radius = outerRadius * radiusFactors[vertex];
            points[tooth * verticesPerTooth + vertex] = {
                static_cast<LONG>(cx + std::lround(std::cos(angle) * radius)),
                static_cast<LONG>(cy + std::lround(std::sin(angle) * radius))
            };
        }
    }

    HBRUSH hBrush = CreateSolidBrush(color);
    HGDIOBJ hOldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
    HGDIOBJ hOldBrush = SelectObject(hdc, hBrush);
    Polygon(hdc, points, _countof(points));

    const int holeRadius = static_cast<int>(std::lround(outerRadius * 0.3));
    HBRUSH hHoleBrush = CreateSolidBrush(backgroundColor);
    SelectObject(hdc, hHoleBrush);
    Ellipse(hdc, cx - holeRadius, cy - holeRadius, cx + holeRadius + 1, cy + holeRadius + 1);

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBrush);
    DeleteObject(hHoleBrush);
}

void DrawTimerIcon(HDC hdc, const RECT& rc, COLORREF color) {
    const int cx = rc.left + (rc.right - rc.left) / 2;
    const int cy = rc.top + (rc.bottom - rc.top) / 2 + 1;
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, cx - 7, cy - 7, cx + 8, cy + 8);
    MoveToEx(hdc, cx, cy, nullptr);
    LineTo(hdc, cx, cy - 4);
    MoveToEx(hdc, cx, cy, nullptr);
    LineTo(hdc, cx + 4, cy + 2);
    MoveToEx(hdc, cx - 3, cy - 10, nullptr);
    LineTo(hdc, cx + 3, cy - 10);
    MoveToEx(hdc, cx, cy - 10, nullptr);
    LineTo(hdc, cx, cy - 7);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void DrawDownloadIcon(HDC hdc, const RECT& rc, COLORREF color) {
    const int size = (std::min)(rc.right - rc.left, rc.bottom - rc.top);
    const int cx = rc.left + (rc.right - rc.left) / 2;
    const int top = rc.top + (rc.bottom - rc.top - size) / 2;
    const int stroke = (std::max)(2, size / 12);

    HPEN pen = CreatePen(PS_SOLID, stroke, color);
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);

    // Arrow: shaft and a filled head pointing down.
    const int headTop = top + size * 9 / 20;
    const int headBottom = top + size * 7 / 10;
    MoveToEx(hdc, cx, top + size / 10, nullptr);
    LineTo(hdc, cx, headTop);
    POINT head[3] = { { cx - size / 4, headTop }, { cx + size / 4, headTop }, { cx, headBottom } };
    Polygon(hdc, head, 3);

    // Tray below the arrow.
    const int trayTop = top + size * 7 / 10;
    const int trayBottom = top + size * 9 / 10;
    const int trayLeft = rc.left + (rc.right - rc.left - size) / 2 + size / 8;
    const int trayRight = trayLeft + size * 3 / 4;
    MoveToEx(hdc, trayLeft, trayTop, nullptr);
    LineTo(hdc, trayLeft, trayBottom);
    LineTo(hdc, trayRight, trayBottom);
    LineTo(hdc, trayRight, trayTop - 1);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}
