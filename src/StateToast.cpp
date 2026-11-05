#include "StateToast.h"
#include "AppState.h"
#include "Drawing.h"
#include "TimerHold.h"
#include <string>

namespace StateToast {
namespace {
constexpr wchar_t kClassName[] = L"TaskManager_State_Toast_Class";
constexpr UINT_PTR kFadeTimer = 1;
constexpr DWORD kFadeInMs = 250;
constexpr DWORD kHoldMs = 500;
constexpr DWORD kFadeOutMs = 250;
constexpr int kPadding = 8;
constexpr int kIconSize = 30;
constexpr int kGap = 10;
constexpr COLORREF kActiveBg = RGB(30, 58, 138); // Same blue as the PIN window's toggles.

HWND g_toast = nullptr;
DWORD g_shownAt = 0;
bool g_active = false; // The toggle is on: blue icon box.
bool g_muted = false;
PinButtons::Id g_id = PinButtons::Mute;
std::wstring g_text;

HFONT ToastFont() {
    static HFONT font = CreateFontW(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return font;
}

COLORREF Background(const ThemeColors& theme) {
    return g_themeMode == ThemeMode::Pink ? RGB(255, 20, 147) : theme.bgCard;
}

COLORREF TextColor(const ThemeColors& theme) {
    return g_themeMode == ThemeMode::Pink ? RGB(0, 0, 0) : theme.textPrimary;
}

void Paint(HWND hWnd, HDC hdc) {
    const ThemeColors theme = GetThemeColors(g_themeMode);
    RECT client{};
    UiScale::ClientRect(hWnd, &client);
    UiScale::Buffer buffer(hdc, client);
    HDC mem = buffer.Dc();

    HBRUSH background = CreateSolidBrush(Background(theme));
    HPEN border = CreatePen(PS_SOLID, 1, theme.borderCard);
    HGDIOBJ oldBrush = SelectObject(mem, background);
    HGDIOBJ oldPen = SelectObject(mem, border);
    RoundRect(mem, 0, 0, client.right, client.bottom, 12, 12);

    const RECT icon = { kPadding, kPadding, kPadding + kIconSize, kPadding + kIconSize };
    HBRUSH iconBrush = nullptr;
    HPEN iconPen = nullptr;
    if (g_active) {
        iconBrush = CreateSolidBrush(kActiveBg);
        iconPen = CreatePen(PS_SOLID, 1, kActiveBg);
        SelectObject(mem, iconBrush);
        SelectObject(mem, iconPen);
        RoundRect(mem, icon.left, icon.top, icon.right, icon.bottom, 6, 6);
    }
    SelectObject(mem, oldPen);
    SelectObject(mem, oldBrush);
    DeleteObject(border);
    DeleteObject(background);
    if (iconPen) DeleteObject(iconPen);
    if (iconBrush) DeleteObject(iconBrush);

    const COLORREF iconColor = g_active ? RGB(255, 255, 255) : TextColor(theme);
    if (g_id == PinButtons::Hold) DrawHandIcon(mem, icon, iconColor);
    else if (g_id == PinButtons::Focus) DrawFocusIcon(mem, icon, iconColor);
    else DrawSpeakerIcon(mem, icon, iconColor, g_muted);

    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, TextColor(theme));
    HGDIOBJ oldFont = SelectObject(mem, ToastFont());
    RECT textRect = { icon.right + kGap, 0, client.right - kPadding, client.bottom };
    DrawTextW(mem, g_text.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(mem, oldFont);

    buffer.Present();
}

// Alpha for the time since the toast appeared; -1 once it is over.
int AlphaAt(DWORD elapsed) {
    if (elapsed < kFadeInMs) return static_cast<int>(255 * elapsed / kFadeInMs);
    elapsed -= kFadeInMs;
    if (elapsed < kHoldMs) return 255;
    elapsed -= kHoldMs;
    if (elapsed < kFadeOutMs) return static_cast<int>(255 * (kFadeOutMs - elapsed) / kFadeOutMs);
    return -1;
}

LRESULT CALLBACK ToastProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        Paint(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_TIMER: {
        if (wParam != kFadeTimer) break;
        const int alpha = AlphaAt(GetTickCount() - g_shownAt);
        if (alpha < 0) {
            DestroyWindow(hWnd);
            return 0;
        }
        SetLayeredWindowAttributes(hWnd, 0, static_cast<BYTE>(alpha), LWA_ALPHA);
        return 0;
    }
    case WM_NCDESTROY:
        KillTimer(hWnd, kFadeTimer);
        if (g_toast == hWnd) g_toast = nullptr;
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool EnsureWindow() {
    if (g_toast) return true;
    static bool registered = false;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!registered) {
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = ToastProc;
        windowClass.hInstance = instance;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.lpszClassName = kClassName;
        registered = RegisterClassW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        if (!registered) return false;
    }
    // Click-through and never activated, so it cannot steal focus from what the user is doing.
    g_toast = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        kClassName, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    return g_toast != nullptr;
}
}

void ShowToggled(PinButtons::Id id) {
    if (id != PinButtons::Mute && id != PinButtons::Hold && id != PinButtons::Focus) return;
    if (PinButtons::IsOnScreen(id)) return;
    g_id = id;
    if (id == PinButtons::Hold) {
        g_active = TimerHold::IsActive();
        g_text = g_active ? L"Időstop aktiválva" : L"Időstop kikapcsolva";
    } else if (id == PinButtons::Focus) {
        g_active = g_focusMode;
        g_text = g_active ? L"Fókusz mód aktiválva" : L"Fókusz mód kikapcsolva";
    } else {
        g_muted = !g_store.sounds_enabled;
        g_active = g_muted;
        g_text = g_muted ? L"Hangok kikapcsolva" : L"Hangok aktiválva";
    }
    const bool restarting = g_toast != nullptr;
    if (!EnsureWindow()) return;

    HDC screen = GetDC(nullptr);
    HGDIOBJ oldFont = SelectObject(screen, ToastFont());
    SIZE textSize{};
    GetTextExtentPoint32W(screen, g_text.c_str(), static_cast<int>(g_text.size()), &textSize);
    SelectObject(screen, oldFont);
    ReleaseDC(nullptr, screen);
    const int width = UiScale::Px(kPadding + kIconSize + kGap + textSize.cx + kPadding + 4);
    const int height = UiScale::Px(kPadding + kIconSize + kPadding);
    const int corner = UiScale::Px(12);

    // Bottom right above the taskbar, or above the PIN window when it is there.
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int x = workArea.right - width - UiScale::Px(14);
    int y = workArea.bottom - height - UiScale::Px(14);
    RECT pin{};
    if (g_isMiniMode && IsWindowVisible(g_hWnd) && GetWindowRect(g_hWnd, &pin)) {
        x = pin.right - width;
        y = pin.top - height - UiScale::Px(8);
        if (y < workArea.top) y = pin.bottom + UiScale::Px(8);
    }
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, corner, corner);
    if (!SetWindowRgn(g_toast, region, FALSE)) DeleteObject(region);
    SetWindowPos(g_toast, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(g_toast, nullptr, FALSE);

    // A repeated toggle keeps the toast fully visible instead of fading in again.
    g_shownAt = GetTickCount() - (restarting ? kFadeInMs : 0);
    SetLayeredWindowAttributes(g_toast, 0, static_cast<BYTE>(restarting ? 255 : 0), LWA_ALPHA);
    SetTimer(g_toast, kFadeTimer, 16, nullptr);
}
}
