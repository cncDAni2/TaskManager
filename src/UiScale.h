#pragma once

#include <windows.h>

// The main window (with the PIN view) and the toasts scale natively, so they stay sharp at any
// display scale; other windows are scaled by Windows (GDI scaling), which is slightly soft.
// The app's code keeps working in fixed 96-DPI units: only these helpers meet real pixels, at the
// borders of painting, mouse input, child window placement and control fonts. At 100% they change
// nothing.
namespace UiScale {
// Call first thing at startup, before any window exists.
void InitProcess();

// Windows created while this is alive use native (true) or GDI (false) scaling, and coordinates
// mean real pixels or the scaled ones respectively.
class ContextScope {
public:
    explicit ContextScope(bool native);
    ~ContextScope();
    ContextScope(const ContextScope&) = delete;
    ContextScope& operator=(const ContextScope&) = delete;

private:
    DPI_AWARENESS_CONTEXT previous;
};

// Takes over the DPI of the main window; false if it did not change.
bool UpdateFromWindow(HWND hWnd);
void SetDpi(UINT dpi);
double Factor();

int Px(int units);
int Units(int pixels);
RECT Px(const RECT& units);
RECT Units(const RECT& pixels); // Rounded outwards.
POINT Units(POINT pixels);

// Counterparts of the Win32 calls, in units.
void ClientRect(HWND hWnd, RECT* rect);
BOOL Place(HWND hWnd, HWND insertAfter, int x, int y, int width, int height, UINT flags);
void Invalidate(HWND hWnd, const RECT* rect, BOOL erase);
void CursorPos(HWND hWnd, POINT* point);

// Lets a DC be drawn on in units. Text is rendered at the real size, so it stays sharp.
class DrawScope {
public:
    explicit DrawScope(HDC hdc);
    // For a control: `units` fills `pixels` so that shapes keep their right and bottom edges, which
    // scaled drawing would otherwise put one pixel outside the control.
    DrawScope(HDC hdc, const RECT& pixels, const RECT& units);
    ~DrawScope();
    DrawScope(const DrawScope&) = delete;
    DrawScope& operator=(const DrawScope&) = delete;

private:
    HDC dc;
};

// True for the DC of a DrawScope or Buffer; other windows' DCs simply take their own coordinates.
bool IsScaled(HDC hdc);

// While alive, the DC of a DrawScope or Buffer takes real pixels (see Px).
class PixelScope {
public:
    explicit PixelScope(HDC hdc);
    ~PixelScope();
    PixelScope(const PixelScope&) = delete;
    PixelScope& operator=(const PixelScope&) = delete;

private:
    HDC dc;
    XFORM saved;
};

// Back buffer of a WM_PAINT: covers just `dirty` and is drawn on in units.
class Buffer {
public:
    Buffer(HDC target, const RECT& dirty);
    ~Buffer();
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    HDC Dc() const { return mem; }
    void ClipTo(const RECT& rect);
    void ClearClip();
    void Present();

private:
    HDC target;
    HDC mem;
    HBITMAP bitmap;
    HGDIOBJ oldBitmap;
    RECT area; // Real pixels.
};

// For controls that draw their own text (EDIT and the like): a font of `height` units, renewed on
// a DPI change. Owner-drawn controls use the plain fonts under a DrawScope.
void SetControlFont(HWND control, int height);
}
