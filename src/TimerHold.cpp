#include "TimerHold.h"
#include "AppState.h"
#include "Drawing.h"
#include "IntervalTimer.h"
#include "Layout.h"
#include <commctrl.h>

namespace {
bool g_active = false;
HWND g_hButton = nullptr;
HWND g_hTooltip = nullptr;

const wchar_t* TooltipText() {
    return g_active
        ? L"Időzítők folytatása (Ctrl+F3)\nA munkaidő-mérés és az időzítő folytatódik, a 20-20-20 újraindul."
        : L"Időstop - időzítők megállítása (Ctrl+F3)\nMegállítja a munkaidő-mérést, az időzítőt és a 20-20-20 szabályt.";
}

void UpdateTooltip() {
    if (!g_hTooltip) return;
    TOOLINFOW tool{};
    tool.cbSize = sizeof(tool);
    tool.hwnd = GetParent(g_hButton);
    tool.uId = reinterpret_cast<UINT_PTR>(g_hButton);
    tool.lpszText = const_cast<LPWSTR>(TooltipText());
    SendMessageW(g_hTooltip, TTM_UPDATETIPTEXTW, 0, (LPARAM)&tool);
}
}

namespace TimerHold {
void CreateButton(HWND parent, HINSTANCE instance) {
    g_hButton = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        218, 10, 28, 28, parent, (HMENU)IDC_TIMER_HOLD_BTN, instance, nullptr);
    g_hTooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        parent, nullptr, instance, nullptr);
    if (!g_hTooltip) return;
    SendMessageW(g_hTooltip, TTM_SETMAXTIPWIDTH, 0, 320);
    TOOLINFOW tool{};
    tool.cbSize = sizeof(tool);
    tool.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    tool.hwnd = parent;
    tool.uId = reinterpret_cast<UINT_PTR>(g_hButton);
    tool.lpszText = const_cast<LPWSTR>(TooltipText());
    SendMessageW(g_hTooltip, TTM_ADDTOOLW, 0, (LPARAM)&tool);
}

void UpdateVisibility(bool visible) {
    if (g_hButton) ShowWindow(g_hButton, visible ? SW_SHOW : SW_HIDE);
}

bool IsActive() {
    return g_active;
}

void Toggle() {
    g_active = !g_active;
    if (g_active) {
        g_intervalTimer.Hold();
        g_isWorkActive = false;
        g_store.Save();
        g_secondsSinceLastSave = 0;
    } else {
        g_intervalTimer.Release();
    }
    UpdateTooltip();
    if (g_hButton) InvalidateRect(g_hButton, nullptr, TRUE);
    if (g_hWnd) {
        if (g_isMiniMode && IsWindowVisible(g_hWnd)) RecalculateMiniLayout();
        InvalidateRect(g_hWnd, nullptr, FALSE);
    }
}

bool HandleCommand(WORD id) {
    if (id != IDC_TIMER_HOLD_BTN && id != IDM_TRAY_HOLD) return false;
    Toggle();
    return true;
}

const wchar_t* MenuLabel() {
    return g_active ? L"Időzítők folytatása (Ctrl+F3)" : L"Időzítők megállítása (Ctrl+F3)";
}

void DrawButton(const DRAWITEMSTRUCT* dis, COLORREF idleBg, COLORREF idleBorder, COLORREF idleIcon) {
    // Active state uses the same blue as the PIN toggle.
    const COLORREF bg = g_active ? RGB(30, 58, 138) : idleBg;
    const COLORREF border = g_active ? RGB(30, 64, 175) : idleBorder;
    const COLORREF icon = g_active ? RGB(255, 255, 255) : idleIcon;
    HBRUSH brush = CreateSolidBrush(bg);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(dis->hDC, brush);
    HGDIOBJ oldPen = SelectObject(dis->hDC, pen);
    RoundRect(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom, 6, 6);
    SelectObject(dis->hDC, oldBrush);
    SelectObject(dis->hDC, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
    RECT rcIcon = dis->rcItem;
    if (dis->itemState & ODS_SELECTED) OffsetRect(&rcIcon, 1, 1);
    DrawHandIcon(dis->hDC, rcIcon, icon);
}
}
