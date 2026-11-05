#include "UndoToast.h"
#include "AppState.h"
#include "InlineEdit.h"
#include "Layout.h"
#include "WorkTracking.h"
#include <algorithm>
#include <windowsx.h>

namespace UndoToast {
namespace {
constexpr wchar_t kClassName[] = L"TaskManager_Undo_Toast_Class";
constexpr wchar_t kButtonText[] = L"Visszavonás";
constexpr UINT_PTR kCloseTimer = 1;
constexpr UINT kVisibleMs = 5000;
constexpr int kHeight = 40;
constexpr int kPadding = 14;
constexpr int kGap = 16;
constexpr int kButtonPadding = 12;
constexpr int kButtonMargin = 6;
constexpr int kCorner = 12;

struct DeletedTasks {
    std::vector<Task> tasks;
    OrderSlots activeOrder;
    OrderSlots sessionOrder;
};

HWND g_toast = nullptr;
RECT g_button{};
bool g_buttonHot = false;
std::wstring g_text;
std::vector<DeletedTasks> g_deletedTasks; // Oldest first; undone newest first.
WorkHistory::Records g_deletedWork;
WorkHistory::Records g_deletedManualWork;
int g_deletedDays = 0;

OrderSlots FindSlots(const std::vector<int>& order, const std::vector<int>& ids) {
    OrderSlots slots;
    for (size_t i = 0; i < order.size(); ++i) {
        if (std::find(ids.begin(), ids.end(), order[i]) != ids.end()) {
            slots.emplace_back(static_cast<int>(i), order[i]);
        }
    }
    return slots;
}

void ClearDeleted() {
    g_deletedTasks.clear();
    g_deletedWork.clear();
    g_deletedManualWork.clear();
    g_deletedDays = 0;
}

void Undo() {
    CommitInlineEdit(); // The list is rebuilt below.
    for (auto it = g_deletedTasks.rbegin(); it != g_deletedTasks.rend(); ++it) {
        g_store.RestoreTasks(std::move(it->tasks), it->activeOrder);
        for (const auto& slot : it->sessionOrder) {
            const int index = (std::min)(slot.first, static_cast<int>(g_sessionActiveTaskIds.size()));
            g_sessionActiveTaskIds.insert(g_sessionActiveTaskIds.begin() + index, slot.second);
        }
    }
    if (g_deletedDays > 0) g_store.RestoreWorkHistory(g_deletedWork, g_deletedManualWork);
    ClearDeleted();
    if (IsWindowVisible(g_hWnd)) {
        RecalculateLayout();
        InvalidateRect(g_hWnd, nullptr, FALSE);
    }
}

void Paint(HWND hWnd, HDC hdc) {
    const ThemeColors theme = GetThemeColors(g_themeMode);
    const bool pink = g_themeMode == ThemeMode::Pink;
    RECT client{};
    UiScale::ClientRect(hWnd, &client);
    UiScale::Buffer buffer(hdc, client);
    HDC mem = buffer.Dc();

    HBRUSH background = CreateSolidBrush(pink ? RGB(255, 20, 147) : theme.bgCard);
    HPEN border = CreatePen(PS_SOLID, 1, theme.borderCard);
    HGDIOBJ oldBrush = SelectObject(mem, background);
    HGDIOBJ oldPen = SelectObject(mem, border);
    RoundRect(mem, 0, 0, client.right, client.bottom, kCorner, kCorner);

    const COLORREF buttonColor = g_buttonHot ? RGB(29, 78, 216) : RGB(37, 99, 235);
    HBRUSH buttonBrush = CreateSolidBrush(buttonColor);
    HPEN buttonPen = CreatePen(PS_SOLID, 1, buttonColor);
    SelectObject(mem, buttonBrush);
    SelectObject(mem, buttonPen);
    RoundRect(mem, g_button.left, g_button.top, g_button.right, g_button.bottom, 6, 6);
    SelectObject(mem, oldPen);
    SelectObject(mem, oldBrush);
    DeleteObject(border);
    DeleteObject(background);
    DeleteObject(buttonPen);
    DeleteObject(buttonBrush);

    SetBkMode(mem, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(mem, g_hFontNormal);
    SetTextColor(mem, pink ? RGB(0, 0, 0) : theme.textPrimary);
    RECT textRect = { kPadding, 0, g_button.left - kGap, client.bottom };
    DrawTextW(mem, g_text.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SetTextColor(mem, RGB(255, 255, 255));
    DrawTextW(mem, kButtonText, -1, &g_button, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(mem, oldFont);

    buffer.Present();
}

void SetButtonHot(HWND hWnd, bool hot) {
    if (g_buttonHot == hot) return;
    g_buttonHot = hot;
    InvalidateRect(hWnd, nullptr, FALSE);
}

LRESULT CALLBACK ToastProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE; // Clicking must not count as the main window losing focus.
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        Paint(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, g_buttonHot ? IDC_HAND : IDC_ARROW));
        return TRUE;
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, hWnd, 0 };
        TrackMouseEvent(&track);
        const POINT point = UiScale::Units(POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
        SetButtonHot(hWnd, PtInRect(&g_button, point) != FALSE);
        return 0;
    }
    case WM_MOUSELEAVE:
        SetButtonHot(hWnd, false);
        return 0;
    case WM_LBUTTONUP: {
        const POINT point = UiScale::Units(POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
        if (PtInRect(&g_button, point)) {
            Undo();
            DestroyWindow(hWnd);
        }
        return 0;
    }
    case WM_TIMER:
        if (wParam != kCloseTimer) break;
        DestroyWindow(hWnd);
        return 0;
    case WM_NCDESTROY:
        KillTimer(hWnd, kCloseTimer);
        if (g_toast == hWnd) {
            g_toast = nullptr;
            ClearDeleted(); // The delete is final now.
        }
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
        windowClass.lpszClassName = kClassName;
        registered = RegisterClassW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        if (!registered) return false;
    }
    // Owned, so it stays above the (also topmost) main window; never activated, so focus stays there.
    g_toast = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kClassName, L"", WS_POPUP,
        0, 0, 0, 0, g_hWnd, nullptr, instance, nullptr);
    return g_toast != nullptr;
}

void Show() {
    if (!EnsureWindow()) {
        ClearDeleted();
        return;
    }
    size_t taskCount = 0;
    for (const DeletedTasks& deleted : g_deletedTasks) taskCount += deleted.tasks.size();
    g_text = g_deletedDays > 0 ? std::to_wstring(g_deletedDays) + L" nap munkaideje törölve."
        : std::to_wstring(taskCount) + L" feladat törölve.";

    HDC screen = GetDC(nullptr);
    HGDIOBJ oldFont = SelectObject(screen, g_hFontNormal);
    SIZE textSize{};
    SIZE buttonSize{};
    GetTextExtentPoint32W(screen, g_text.c_str(), static_cast<int>(g_text.size()), &textSize);
    GetTextExtentPoint32W(screen, kButtonText, static_cast<int>(wcslen(kButtonText)), &buttonSize);
    SelectObject(screen, oldFont);
    ReleaseDC(nullptr, screen);

    const int buttonWidth = buttonSize.cx + 2 * kButtonPadding;
    const int width = kPadding + textSize.cx + kGap + buttonWidth + kButtonMargin;
    g_button = { width - kButtonMargin - buttonWidth, kButtonMargin, width - kButtonMargin, kHeight - kButtonMargin };

    // Bottom centre of the main window, just above the work bar.
    RECT owner{};
    GetWindowRect(g_hWnd, &owner);
    const int pixelW = UiScale::Px(width);
    const int pixelH = UiScale::Px(kHeight);
    const int x = owner.left + (owner.right - owner.left - pixelW) / 2;
    const int y = owner.bottom - UiScale::Px(WorkTracking::BottomBarHeight() + 12) - pixelH;
    HRGN region = CreateRoundRectRgn(0, 0, pixelW + 1, pixelH + 1, UiScale::Px(kCorner), UiScale::Px(kCorner));
    if (!SetWindowRgn(g_toast, region, FALSE)) DeleteObject(region);
    SetWindowPos(g_toast, HWND_TOPMOST, x, y, pixelW, pixelH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(g_toast, nullptr, FALSE);
    SetTimer(g_toast, kCloseTimer, kVisibleMs, nullptr);
}
}

void DeleteTasks(const std::vector<int>& ids) {
    if (g_deletedDays > 0) ClearDeleted(); // Only one kind of delete can be undone at a time.
    DeletedTasks deleted;
    for (const Task& task : g_store.tasks) {
        if (std::find(ids.begin(), ids.end(), task.id) != ids.end()) deleted.tasks.push_back(task);
    }
    deleted.activeOrder = FindSlots(g_store.active_order, ids);
    deleted.sessionOrder = FindSlots(g_sessionActiveTaskIds, ids);

    g_store.DeleteMany(ids);
    for (const auto& slot : deleted.sessionOrder) {
        g_sessionActiveTaskIds.erase(std::find(g_sessionActiveTaskIds.begin(), g_sessionActiveTaskIds.end(), slot.second));
    }
    g_deletedTasks.push_back(std::move(deleted));
    Show();
}

void PurgeWorkHistory(const std::string& dateKey) {
    g_deletedTasks.clear(); // Only one kind of delete can be undone at a time.
    g_deletedDays += g_store.CountWorkHistoryBefore(dateKey);
    const auto& measured = g_store.work_history;
    const auto& manual = g_store.manual_work_history;
    g_deletedWork.insert(measured.begin(), measured.lower_bound(dateKey));
    g_deletedManualWork.insert(manual.begin(), manual.lower_bound(dateKey));
    g_store.PurgeWorkHistoryBefore(dateKey);
    Show();
}
}
