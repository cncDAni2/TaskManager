#include "Marker.h"
#include "AppState.h"
#include <algorithm>
#include <cmath>

static POINT ScaleMarkerPoint(const RECT& rect, int x, int y) {
    return {
        rect.left + (rect.right - rect.left - 1) * x / 100,
        rect.top + (rect.bottom - rect.top - 1) * y / 100
    };
}

static void DrawFilledMarkerShape(HDC hdc, const RECT& rect, const POINT* normalizedPoints, int pointCount) {
    POINT points[20];
    for (int i = 0; i < pointCount; ++i) {
        points[i] = ScaleMarkerPoint(rect, normalizedPoints[i].x, normalizedPoints[i].y);
    }
    Polygon(hdc, points, pointCount);
}

void DrawTaskMarker(HDC hdc, const RECT& rect, TaskMarker marker, COLORREF emptyBackground, COLORREF emptyBorder) {
    const ThemeMode selectedTheme = g_themeMode;
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int inset = (std::max)(1, (std::min)(width, height) / 8);
    RECT shapeRect = { rect.left + inset, rect.top + inset, rect.right - inset, rect.bottom - inset };

    COLORREF fillColor = emptyBackground;
    COLORREF borderColor = emptyBorder;
    if (marker == TaskMarker::Heart) {
        if (selectedTheme == ThemeMode::Pink) {
            fillColor = RGB(79, 0, 72);
        } else {
            fillColor = RGB(225, 29, 72);
        }
        borderColor = fillColor;
    } else if (marker == TaskMarker::Crescent) {
        fillColor = RGB(37, 99, 235);
        borderColor = fillColor;
    } else if (marker == TaskMarker::Lightning) {
        if (selectedTheme == ThemeMode::Light) {
            fillColor = RGB(126, 126, 0);
        } else {
            fillColor = RGB(255, 255, 0);
        }
        borderColor = fillColor;
    }

    HBRUSH brush = CreateSolidBrush(fillColor);
    HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    if (marker == TaskMarker::Heart) {
        const POINT points[] = {
            { 50, 94 }, { 14, 61 }, { 7, 47 }, { 8, 34 }, { 16, 23 },
            { 28, 18 }, { 40, 22 }, { 50, 34 }, { 60, 22 }, { 72, 18 },
            { 84, 23 }, { 92, 34 }, { 93, 47 }, { 86, 61 }
        };
        DrawFilledMarkerShape(hdc, shapeRect, points, static_cast<int>(std::size(points)));
    } else if (marker == TaskMarker::Crescent) {
        const POINT points[] = {
            { 66, 6 }, { 49, 7 }, { 33, 14 }, { 20, 26 }, { 11, 41 }, { 8, 56 },
            { 12, 70 }, { 22, 82 }, { 36, 91 }, { 52, 95 }, { 68, 92 }, { 57, 82 },
            { 49, 69 }, { 45, 55 }, { 46, 41 }, { 51, 27 }, { 59, 15 }
        };
        DrawFilledMarkerShape(hdc, shapeRect, points, static_cast<int>(std::size(points)));
    } else if (marker == TaskMarker::Lightning) {
        const POINT points[] = {
            { 59, 4 }, { 27, 53 }, { 46, 53 }, { 38, 96 },
            { 79, 40 }, { 57, 40 }, { 70, 4 }
        };
        DrawFilledMarkerShape(hdc, shapeRect, points, static_cast<int>(std::size(points)));
    } else {
        Ellipse(hdc, shapeRect.left, shapeRect.top, shapeRect.right, shapeRect.bottom);
    }

    if (marker != TaskMarker::None) {
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
        return;
    }

    COLORREF starColor = emptyBorder;
    HPEN starPen = CreatePen(PS_SOLID, 1, starColor);
    HBRUSH starBrush = CreateSolidBrush(starColor);
    SelectObject(hdc, starPen);
    SelectObject(hdc, starBrush);

    int centerX = (shapeRect.left + shapeRect.right) / 2;
    int centerY = (shapeRect.top + shapeRect.bottom) / 2;
    double radius = (std::min)(shapeRect.right - shapeRect.left, shapeRect.bottom - shapeRect.top) * 0.36;
    POINT starPoints[10];
    const double pi = 3.14159265358979323846;
    for (int i = 0; i < 10; ++i) {
        double angle = -pi / 2.0 + i * pi / 5.0;
        double pointRadius = (i % 2 == 0) ? radius : radius * 0.45;
        starPoints[i] = {
            centerX + static_cast<int>(std::round(std::cos(angle) * pointRadius)),
            centerY + static_cast<int>(std::round(std::sin(angle) * pointRadius))
        };
    }
    Polygon(hdc, starPoints, 10);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(starBrush);
    DeleteObject(starPen);
    DeleteObject(brush);
    DeleteObject(pen);
}