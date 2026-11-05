#include "UiScale.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace UiScale {
namespace {
using SetProcessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
using SetThreadContextFn = DPI_AWARENESS_CONTEXT(WINAPI*)(DPI_AWARENESS_CONTEXT);
using GetDpiFn = UINT(WINAPI*)(HWND);

struct ScaledControl {
    HWND control;
    int height;
};

double g_factor = 1.0;
SetThreadContextFn g_setThreadContext = nullptr;
GetDpiFn g_getDpiForWindow = nullptr;
std::map<int, HFONT> g_fonts; // By height in units, for the current DPI.
std::vector<ScaledControl> g_controls;

FARPROC User32Function(const char* name) {
    return GetProcAddress(GetModuleHandleW(L"user32.dll"), name);
}

HFONT FontFor(int height) {
    auto it = g_fonts.find(height);
    if (it != g_fonts.end()) return it->second;
    HFONT font = CreateFontW(Px(height), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_fonts[height] = font;
    return font;
}

void ApplyTransform(HDC hdc) {
    SetGraphicsMode(hdc, GM_ADVANCED);
    const XFORM transform = { static_cast<FLOAT>(g_factor), 0, 0, static_cast<FLOAT>(g_factor), 0, 0 };
    SetWorldTransform(hdc, &transform);
}
}

void InitProcess() {
    const auto setProcessContext = reinterpret_cast<SetProcessContextFn>(User32Function("SetProcessDpiAwarenessContext"));
    g_setThreadContext = reinterpret_cast<SetThreadContextFn>(User32Function("SetThreadDpiAwarenessContext"));
    g_getDpiForWindow = reinterpret_cast<GetDpiFn>(User32Function("GetDpiForWindow"));
    // The manifest's gdiScaling setting does not turn this on, hence the call. Older Windows
    // versions lack these functions: there everything simply gets stretched.
    if (setProcessContext) setProcessContext(DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED);
}

ContextScope::ContextScope(bool native) : previous(nullptr) {
    if (g_setThreadContext) {
        previous = g_setThreadContext(native ? DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
            : DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED);
    }
}

ContextScope::~ContextScope() {
    if (previous) g_setThreadContext(previous);
}

bool UpdateFromWindow(HWND hWnd) {
    if (!g_getDpiForWindow) return false;
    const UINT dpi = g_getDpiForWindow(hWnd);
    if (dpi == 0 || dpi / 96.0 == g_factor) return false;
    SetDpi(dpi);
    return true;
}

void SetDpi(UINT dpi) {
    g_factor = dpi / 96.0;
    g_controls.erase(std::remove_if(g_controls.begin(), g_controls.end(),
        [](const ScaledControl& entry) { return !IsWindow(entry.control); }), g_controls.end());
    std::map<int, HFONT> oldFonts;
    oldFonts.swap(g_fonts);
    for (const ScaledControl& entry : g_controls) {
        SendMessageW(entry.control, WM_SETFONT, reinterpret_cast<WPARAM>(FontFor(entry.height)), TRUE);
    }
    for (const auto& font : oldFonts) DeleteObject(font.second);
}

double Factor() {
    return g_factor;
}

int Px(int units) {
    return static_cast<int>(std::lround(units * g_factor));
}

int Units(int pixels) {
    return static_cast<int>(std::floor(pixels / g_factor));
}

RECT Px(const RECT& units) {
    return { Px(units.left), Px(units.top), Px(units.right), Px(units.bottom) };
}

RECT Units(const RECT& pixels) {
    return { Units(pixels.left), Units(pixels.top),
        static_cast<int>(std::ceil(pixels.right / g_factor)), static_cast<int>(std::ceil(pixels.bottom / g_factor)) };
}

POINT Units(POINT pixels) {
    return { Units(pixels.x), Units(pixels.y) };
}

void ClientRect(HWND hWnd, RECT* rect) {
    ContextScope native(true);
    GetClientRect(hWnd, rect);
    rect->right = static_cast<int>(std::lround(rect->right / g_factor));
    rect->bottom = static_cast<int>(std::lround(rect->bottom / g_factor));
}

BOOL Place(HWND hWnd, HWND insertAfter, int x, int y, int width, int height, UINT flags) {
    ContextScope native(true);
    return SetWindowPos(hWnd, insertAfter, Px(x), Px(y), Px(x + width) - Px(x), Px(y + height) - Px(y), flags);
}

void Invalidate(HWND hWnd, const RECT* rect, BOOL erase) {
    ContextScope native(true);
    if (!rect) {
        InvalidateRect(hWnd, nullptr, erase);
        return;
    }
    RECT pixels = Px(*rect);
    InflateRect(&pixels, g_factor > 1.0 ? 1 : 0, g_factor > 1.0 ? 1 : 0); // Rounded edges.
    InvalidateRect(hWnd, &pixels, erase);
}

void CursorPos(HWND hWnd, POINT* point) {
    ContextScope native(true);
    GetCursorPos(point);
    ScreenToClient(hWnd, point);
    *point = Units(*point);
}

DrawScope::DrawScope(HDC hdc) : dc(hdc) {
    if (g_factor != 1.0) ApplyTransform(dc);
}

DrawScope::DrawScope(HDC hdc, const RECT& pixels, const RECT& units) : dc(hdc) {
    if (g_factor == 1.0) return;
    SetGraphicsMode(dc, GM_ADVANCED);
    XFORM transform{};
    transform.eM11 = static_cast<FLOAT>(pixels.right - pixels.left - 1) / (units.right - units.left);
    transform.eM22 = static_cast<FLOAT>(pixels.bottom - pixels.top - 1) / (units.bottom - units.top);
    SetWorldTransform(dc, &transform);
}

DrawScope::~DrawScope() {
    if (g_factor == 1.0) return;
    ModifyWorldTransform(dc, nullptr, MWT_IDENTITY);
    SetGraphicsMode(dc, GM_COMPATIBLE);
}

bool IsScaled(HDC hdc) {
    return GetGraphicsMode(hdc) == GM_ADVANCED;
}

PixelScope::PixelScope(HDC hdc) : dc(hdc), saved{} {
    if (!IsScaled(dc)) return;
    GetWorldTransform(dc, &saved);
    ModifyWorldTransform(dc, nullptr, MWT_IDENTITY);
}

PixelScope::~PixelScope() {
    if (IsScaled(dc)) SetWorldTransform(dc, &saved);
}

Buffer::Buffer(HDC targetDc, const RECT& dirty) : target(targetDc), area(Px(dirty)) {
    mem = CreateCompatibleDC(target);
    bitmap = CreateCompatibleBitmap(target, (std::max)(1, static_cast<int>(area.right - area.left)),
        (std::max)(1, static_cast<int>(area.bottom - area.top)));
    oldBitmap = SelectObject(mem, bitmap);
    SetViewportOrgEx(mem, -area.left, -area.top, nullptr);
    if (g_factor != 1.0) ApplyTransform(mem);
}

Buffer::~Buffer() {
    SelectObject(mem, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(mem);
}

void Buffer::ClipTo(const RECT& rect) {
    // Clip regions are in buffer pixels.
    const RECT pixels = Px(rect);
    HRGN region = CreateRectRgn(pixels.left - area.left, pixels.top - area.top,
        pixels.right - area.left, pixels.bottom - area.top);
    SelectClipRgn(mem, region);
    DeleteObject(region);
}

void Buffer::ClearClip() {
    SelectClipRgn(mem, nullptr);
}

void Buffer::Present() {
    ModifyWorldTransform(mem, nullptr, MWT_IDENTITY);
    SetViewportOrgEx(mem, 0, 0, nullptr);
    BitBlt(target, area.left, area.top, area.right - area.left, area.bottom - area.top, mem, 0, 0, SRCCOPY);
}

void SetControlFont(HWND control, int height) {
    g_controls.erase(std::remove_if(g_controls.begin(), g_controls.end(),
        [](const ScaledControl& entry) { return !IsWindow(entry.control); }), g_controls.end());
    g_controls.push_back({ control, height });
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(FontFor(height)), TRUE);
}
}
