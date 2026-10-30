#include "UpdateOverlay.h"
#include "AppState.h"
#include "Drawing.h"
#include "Layout.h"
#include "SelfUpdate.h"
#include "UpdateCheck.h"
#include "Version.h"
#include "VersionHint.h"
#include "WorkHistory.h"
#include <shellapi.h>
#include <windowsx.h>
#include <string>

namespace {
constexpr COLORREF ACCENT = RGB(37, 99, 235);
constexpr int FULL_LAYOUT_MIN_HEIGHT = 150; // Below this (a short PIN view) everything fits on two lines.

enum class Hover { None, Close, Download };
Hover g_hover = Hover::None;

struct Geometry {
    bool compact = false;
    RECT close{};
    RECT title{};
    RECT version{};
    RECT download{};
    RECT icon{};
    RECT label{};
};

Geometry Measure(const RECT& client) {
    Geometry g;
    const int w = client.right - client.left;
    const int h = client.bottom - client.top;
    const int cx = w / 2;
    const int cy = h / 2;
    g.compact = h < FULL_LAYOUT_MIN_HEIGHT;
    if (g.compact) {
        g.close = { w - 24, 4, w - 6, 22 };
        g.title = { 10, cy - 21, w - 28, cy - 2 };
        g.download = { cx - 62, cy + 1, cx + 62, cy + 23 };
        g.icon = { g.download.left + 10, g.download.top + 3, g.download.left + 26, g.download.top + 19 };
        g.label = { g.icon.right + 6, g.download.top, g.download.right - 8, g.download.bottom };
    } else {
        g.close = { w - 40, 10, w - 10, 40 };
        g.title = { 16, cy - 104, w - 16, cy - 72 };
        g.version = { 16, cy - 70, w - 16, cy - 48 };
        g.download = { cx - 72, cy - 34, cx + 72, cy + 74 };
        g.icon = { cx - 28, cy - 24, cx + 28, cy + 32 };
        g.label = { g.download.left, cy + 38, g.download.right, cy + 64 };
    }
    return g;
}

void Dismiss() {
    g_store.update_dismissed_date = WorkHistory::DateKey(time(nullptr));
    g_store.SaveLocal();
    g_hover = Hover::None;
    UpdateOverlay::Refresh();
}

Hover HitTest(HWND hWnd, POINT point) {
    RECT client{};
    GetClientRect(hWnd, &client);
    const Geometry g = Measure(client);
    if (PtInRect(&g.close, point)) return Hover::Close;
    if (PtInRect(&g.download, point)) return Hover::Download;
    return Hover::None;
}

BOOL CALLBACK HideChild(HWND child, LPARAM parent) {
    if (GetParent(child) == reinterpret_cast<HWND>(parent)) ShowWindow(child, SW_HIDE);
    return TRUE;
}

void DrawClose(HDC hdc, const RECT& rc, COLORREF color, COLORREF hoverBg, bool hovered) {
    if (hovered) {
        HBRUSH brush = CreateSolidBrush(hoverBg);
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);
    }
    const int inset = (rc.right - rc.left) / 3;
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    MoveToEx(hdc, rc.left + inset, rc.top + inset, nullptr);
    LineTo(hdc, rc.right - inset, rc.bottom - inset);
    MoveToEx(hdc, rc.right - inset, rc.top + inset, nullptr);
    LineTo(hdc, rc.left + inset, rc.bottom - inset);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}
}

void UpdateOverlay::OpenDownloadPage() {
    ShellExecuteW(nullptr, L"open", UpdateCheck::DOWNLOAD_URL, nullptr, nullptr, SW_SHOWNORMAL);
    Dismiss();
}

bool UpdateOverlay::IsVisible() {
    const TaskStore& store = g_store;
    if (!store.update_check_enabled || store.update_latest_version.empty()) return false;
    if (VersionHint::Compare(store.update_latest_version, TASK_MANAGER_VERSION_STRING) <= 0) return false;
    return store.update_dismissed_date != WorkHistory::DateKey(time(nullptr));
}

void UpdateOverlay::Refresh() {
    if (!g_hWnd) return;
    UpdateControlsVisibility(); // Shows the right controls again, or hides them all via HideChildren.
    InvalidateRect(g_hWnd, nullptr, TRUE);
}

void UpdateOverlay::HideChildren(HWND parent) {
    HWND focus = GetFocus();
    if (focus && IsChild(parent, focus)) SetFocus(parent);
    EnumChildWindows(parent, HideChild, reinterpret_cast<LPARAM>(parent));
}

void UpdateOverlay::Paint(HDC hdc, const RECT& client) {
    if (!IsVisible()) return;
    const ThemeColors th = GetThemeColors(g_themeMode);
    const Geometry g = Measure(client);

    HBRUSH background = CreateSolidBrush(th.bgWindow);
    FillRect(hdc, &client, background);
    DeleteObject(background);
    SetBkMode(hdc, TRANSPARENT);

    const std::wstring latest = TaskUtils::Utf8ToWide(g_store.update_latest_version);
    HGDIOBJ oldFont = SelectObject(hdc, g.compact ? g_hFontHeader : g_hFontTitle);
    SetTextColor(hdc, th.textTitle);
    const std::wstring title = g.compact ? L"Új verzió érhető el: " + latest : L"Új verzió érhető el";
    RECT titleRect = g.title;
    DrawTextW(hdc, title.c_str(), -1, &titleRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    if (!g.compact) {
        SelectObject(hdc, g_hFontSmall);
        SetTextColor(hdc, th.textSecondary);
        const std::wstring version = latest + L"  (nálad " +
            TaskUtils::Utf8ToWide(TASK_MANAGER_VERSION_STRING) + L")";
        RECT versionRect = g.version;
        DrawTextW(hdc, version.c_str(), -1, &versionRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    // Download button: icon with the label below (compact: beside it).
    const bool downloadHover = g_hover == Hover::Download;
    HBRUSH buttonBrush = CreateSolidBrush(downloadHover ? th.bgCardHover : th.bgCard);
    HPEN buttonPen = CreatePen(PS_SOLID, 1, downloadHover ? ACCENT : th.borderCard);
    HGDIOBJ oldBrush = SelectObject(hdc, buttonBrush);
    HGDIOBJ oldPen = SelectObject(hdc, buttonPen);
    RoundRect(hdc, g.download.left, g.download.top, g.download.right, g.download.bottom, 10, 10);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(buttonPen);
    DeleteObject(buttonBrush);
    DrawDownloadIcon(hdc, g.icon, ACCENT);

    SelectObject(hdc, g.compact ? g_hFontNormal : g_hFontHeader);
    SetTextColor(hdc, ACCENT);
    RECT labelRect = g.label;
    DrawTextW(hdc, SelfUpdate::IsBusy() ? L"Letöltés…" : L"Frissítés", -1, &labelRect,
        (g.compact ? DT_LEFT : DT_CENTER) | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    DrawClose(hdc, g.close, th.textSecondary, th.bgCardHover, g_hover == Hover::Close);
    SelectObject(hdc, oldFont);
}

bool UpdateOverlay::HandleLButtonDown(HWND hWnd, LPARAM lParam) {
    if (!IsVisible()) return false;
    const Hover hit = HitTest(hWnd, { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
    if (hit == Hover::Download) {
        if (!SelfUpdate::Start(hWnd)) OpenDownloadPage();
    } else if (hit == Hover::Close) {
        Dismiss();
    }
    return true;
}

bool UpdateOverlay::HandleMouseMove(HWND hWnd, LPARAM lParam) {
    if (!IsVisible()) return false;
    const Hover hit = HitTest(hWnd, { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
    if (hit != g_hover) {
        g_hover = hit;
        InvalidateRect(hWnd, nullptr, FALSE);
    }
    return true;
}

bool UpdateOverlay::HandleSetCursor(HWND hWnd) {
    if (!IsVisible()) return false;
    POINT cursor{};
    GetCursorPos(&cursor);
    ScreenToClient(hWnd, &cursor);
    SetCursor(LoadCursorW(nullptr, HitTest(hWnd, cursor) == Hover::None ? IDC_ARROW : IDC_HAND));
    return true;
}
