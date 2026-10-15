#include "WorkTracking.h"
#include "AppState.h"
#include "IntervalTimer.h"
#include "Layout.h"
#include "UIEventHandler.h"
#include <commctrl.h>

namespace {
HWND g_hTrackingBtn = nullptr;
HWND g_hTrackingTooltip = nullptr;

const wchar_t* ButtonLabel() {
    return g_store.work_tracking_enabled ? L"Időmérés kikapcsolása" : L"Időmérés bekapcsolása";
}

const wchar_t* ButtonTooltip() {
    return g_store.work_tracking_enabled
        ? L"Teljesen kikapcsolja a munkaidő mérését, és elrejti a munkaidő-sávot. A korábbi idők megmaradnak. A Beállításokban is átállítható."
        : L"Újra méri a képernyő előtt töltött aktív időt, és visszahozza a munkaidő-sávot.";
}

void RefreshButton() {
    if (!g_hTrackingBtn) return;
    SetWindowTextW(g_hTrackingBtn, ButtonLabel());
    if (g_hTrackingTooltip) {
        TOOLINFOW tool{};
        tool.cbSize = sizeof(tool);
        tool.hwnd = GetParent(g_hTrackingBtn);
        tool.uId = reinterpret_cast<UINT_PTR>(g_hTrackingBtn);
        tool.lpszText = const_cast<LPWSTR>(ButtonTooltip());
        SendMessageW(g_hTrackingTooltip, TTM_UPDATETIPTEXTW, 0, (LPARAM)&tool);
    }
    InvalidateRect(g_hTrackingBtn, nullptr, TRUE);
}
}

bool WorkTracking::IsEnabled() {
    return g_store.work_tracking_enabled;
}

void WorkTracking::SetEnabled(bool enabled) {
    if (g_store.work_tracking_enabled == enabled) return;
    // A manual or STRICT MODE pause belongs to the measurement being switched; start clean.
    SetWorkMeasurementStopped(false);
    g_store.work_tracking_enabled = enabled;
    g_isWorkActive = false;
    g_isExcludedApp = false;
    g_excludedAppName.clear();
    g_secondsSinceLastSave = 0;
    if (enabled && g_store.timer_strict_mode && g_intervalTimer.IsActive() && !g_intervalTimer.IsWorkPhase()) {
        SetWorkMeasurementStopped(true);
    }
    g_store.SaveLocal();

    RefreshButton();
    if (!g_hWnd) return;
    UpdateControlsVisibility();
    RecalculateLayout();
    InvalidateRect(g_hWnd, nullptr, TRUE);
}

int WorkTracking::BottomBarHeight() {
    return g_store.work_tracking_enabled ? BOTTOM_BAR_HEIGHT : 0;
}

int WorkTracking::MiniBarHeight() {
    return g_store.work_tracking_enabled ? 3 : 0;
}

void WorkTracking::CreateButton(HWND parent, HINSTANCE instance) {
    g_hTrackingBtn = CreateWindowW(L"BUTTON", ButtonLabel(), WS_CHILD | BS_OWNERDRAW | WS_TABSTOP,
        0, 0, 160, 26, parent, (HMENU)IDC_WORK_TRACKING_BTN, instance, nullptr);
    SendMessageW(g_hTrackingBtn, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

    g_hTrackingTooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        parent, nullptr, instance, nullptr);
    if (g_hTrackingTooltip) {
        SendMessageW(g_hTrackingTooltip, TTM_SETMAXTIPWIDTH, 0, 320);
        TOOLINFOW tool{};
        tool.cbSize = sizeof(tool);
        tool.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
        tool.hwnd = parent;
        tool.uId = reinterpret_cast<UINT_PTR>(g_hTrackingBtn);
        tool.lpszText = const_cast<LPWSTR>(ButtonTooltip());
        SendMessageW(g_hTrackingTooltip, TTM_ADDTOOLW, 0, (LPARAM)&tool);
    }
}

void WorkTracking::LayoutFooter(bool historyVisible) {
    const bool enabled = g_store.work_tracking_enabled;
    ShowWindow(g_hWorkMeasureBtn, historyVisible && enabled ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hTrackingBtn, historyVisible ? SW_SHOW : SW_HIDE);
    if (!historyVisible) return;

    RECT client{};
    GetClientRect(g_hWnd, &client);
    constexpr int measureWidth = 132;
    constexpr int trackingWidth = 160;
    constexpr int gap = 8;
    constexpr int height = 26;
    const int groupWidth = enabled ? measureWidth + gap + trackingWidth : trackingWidth;
    const int left = (client.right - groupWidth) / 2;
    const int top = GetListBottom(client.bottom) + (WORK_MEASURE_FOOTER_HEIGHT - height) / 2;
    if (enabled) {
        SetWindowPos(g_hWorkMeasureBtn, nullptr, left, top, measureWidth, height, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    SetWindowPos(g_hTrackingBtn, nullptr, enabled ? left + measureWidth + gap : left, top,
        trackingWidth, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

bool WorkTracking::HandleCommand(WORD id) {
    if (id != IDC_WORK_TRACKING_BTN) return false;
    SetEnabled(!g_store.work_tracking_enabled);
    return true;
}
