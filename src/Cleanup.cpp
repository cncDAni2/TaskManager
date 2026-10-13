#include "Cleanup.h"
#include "AppState.h"
#include "Layout.h"
#include "UIEventHandler.h"
#include <commctrl.h>
#include <algorithm>
#include <string>

namespace {
HWND g_hCleanupEdit = nullptr;
HWND g_hCleanupBtn = nullptr;
HWND g_hCleanupTooltip = nullptr;
constexpr int DAY_SECONDS = 24 * 60 * 60;

bool IsHistoryView() {
    return g_viewMode == ViewMode::WorkHistory;
}

int ReadValue() {
    wchar_t text[8]{};
    GetWindowTextW(g_hCleanupEdit, text, 8);
    return _wtoi(text);
}

void SetTooltip(const wchar_t* text) {
    if (!g_hCleanupTooltip) return;
    TOOLINFOW tool{};
    tool.cbSize = sizeof(tool);
    tool.hwnd = GetParent(g_hCleanupBtn);
    tool.uId = reinterpret_cast<UINT_PTR>(g_hCleanupBtn);
    tool.lpszText = const_cast<LPWSTR>(text);
    SendMessageW(g_hCleanupTooltip, TTM_UPDATETIPTEXTW, 0, (LPARAM)&tool);
}

void RefreshView(HWND hWnd) {
    g_scrollY = 0;
    g_selectedIndex = -1;
    RecalculateLayout();
    InvalidateRect(hWnd, nullptr, TRUE);
}

void CleanCompletedTasks(HWND hWnd, int days) {
    const time_t cutoff = time(nullptr) - static_cast<time_t>(days) * DAY_SECONDS;
    std::vector<int> ids;
    int syncCount = 0;
    for (const Task& task : g_store.tasks) {
        if (!task.completed || task.completed_at <= 0 || task.completed_at >= cutoff) continue;
        ids.push_back(task.id);
        if (task.is_sync) ++syncCount;
    }
    if (ids.empty()) {
        ShowCenteredMessageBox(hWnd, L"Nincs " + std::to_wstring(days) + L" napnál régebben elkészült feladat.",
            L"Takarítás", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring text = std::to_wstring(ids.size()) + L" elkészült feladat törlődik, amely " +
        std::to_wstring(days) + L" napnál régebben készült el.";
    if (syncCount > 0) {
        text += L"\nEbből " + std::to_wstring(syncCount) +
            L" szinkronizált: azok a többi felhasználónál is törlődnek.";
    }
    text += L"\n\nFolytatod?";
    if (ShowCenteredMessageBox(hWnd, text, L"Takarítás", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    g_store.DeleteMany(ids);
    RefreshView(hWnd);
}

// Keeps the current week and the `weeks` weeks before it.
std::string WeekCutoffKey(int weeks) {
    const time_t now = time(nullptr);
    tm local{};
    localtime_s(&local, &now);
    const int daysSinceMonday = (local.tm_wday + 6) % 7;
    return WorkHistory::DateKey(now - static_cast<time_t>(daysSinceMonday + 7 * weeks) * DAY_SECONDS);
}

void CleanWorkHistory(HWND hWnd, int weeks) {
    const std::string cutoff = WeekCutoffKey(weeks);
    const int count = g_store.CountWorkHistoryBefore(cutoff);
    if (count == 0) {
        ShowCenteredMessageBox(hWnd, L"Nincs " + std::to_wstring(weeks) + L" hétnél régebbi mért vagy kézi munkaidő.",
            L"Takarítás", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring text = std::to_wstring(count) + L" nap mért és kézi munkaideje törlődik (" +
        TaskUtils::Utf8ToWide(cutoff) + L" előtt).\n\nFolytatod?";
    if (ShowCenteredMessageBox(hWnd, text, L"Takarítás", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    g_store.PurgeWorkHistoryBefore(cutoff);
    RefreshView(hWnd);
}
}

void Cleanup::CreateControls(HWND parent, HINSTANCE instance) {
    g_hCleanupEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_TABSTOP | ES_NUMBER | ES_CENTER, 0, 0, 30, 22, parent,
        (HMENU)IDC_CLEANUP_EDIT, instance, nullptr);
    SendMessageW(g_hCleanupEdit, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    SendMessageW(g_hCleanupEdit, EM_LIMITTEXT, 2, 0);
    g_hCleanupBtn = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 250, 26, parent, (HMENU)IDC_CLEANUP_BTN, instance, nullptr);
    SendMessageW(g_hCleanupBtn, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

    g_hCleanupTooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        parent, nullptr, instance, nullptr);
    if (g_hCleanupTooltip) {
        SendMessageW(g_hCleanupTooltip, TTM_SETMAXTIPWIDTH, 0, 320);
        TOOLINFOW tool{};
        tool.cbSize = sizeof(tool);
        tool.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
        tool.hwnd = parent;
        tool.uId = reinterpret_cast<UINT_PTR>(g_hCleanupBtn);
        tool.lpszText = const_cast<LPWSTR>(L"");
        SendMessageW(g_hCleanupTooltip, TTM_ADDTOOLW, 0, (LPARAM)&tool);
    }
}

static bool HasControls() {
    return !g_isMiniMode && (g_viewMode == ViewMode::CompletedTasks || IsHistoryView());
}

// Just wide enough for the label.
static int ButtonWidth(const wchar_t* label) {
    HDC dc = GetDC(g_hCleanupBtn);
    HGDIOBJ oldFont = SelectObject(dc, g_hFontSmall);
    SIZE size{};
    GetTextExtentPoint32W(dc, label, static_cast<int>(wcslen(label)), &size);
    SelectObject(dc, oldFont);
    ReleaseDC(g_hCleanupBtn, dc);
    return size.cx + 18;
}

void Cleanup::UpdateVisibility(int clientWidth) {
    if (!g_hCleanupEdit) return;
    const bool show = HasControls();
    ShowWindow(g_hCleanupEdit, show ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hCleanupBtn, show ? SW_SHOW : SW_HIDE);
    if (!show) return;

    const bool history = IsHistoryView();
    const int value = history ? g_store.cleanup_weeks : g_store.cleanup_days;
    SetWindowTextW(g_hCleanupEdit, std::to_wstring(value).c_str());
    const wchar_t* label = history ? L"hétnél régebbi idők törlése" : L"napnál régebbi feladatok törlése";
    SetWindowTextW(g_hCleanupBtn, label);
    SetTooltip(history
        ? L"Törli a mért és kézi munkaidőket, a mostani hetet és az előtte lévő megadott számú hetet megtartva. Előtte megerősítést kér."
        : L"Törli azokat az elkészült feladatokat, amelyek a megadott számú napnál régebben készültek el. Előtte megerősítést kér.");

    // Floats over the list's top-right corner, which the date/column header row leaves free.
    constexpr int height = 22;
    const int top = GetListTop() + 4;
    const int buttonWidth = ButtonWidth(label);
    const int buttonX = clientWidth - 14 - buttonWidth;
    SetWindowPos(g_hCleanupBtn, HWND_TOP, buttonX, top, buttonWidth, height, SWP_NOACTIVATE);
    SetWindowPos(g_hCleanupEdit, HWND_TOP, buttonX - 4 - 30, top, 30, height, SWP_NOACTIVATE);
    InvalidateRect(g_hCleanupBtn, nullptr, TRUE);
}

COLORREF Cleanup::SurfaceColor() {
    if (IsHistoryView()) return GetHistoryColors(g_themeMode).background;
    return GetThemeColors(g_themeMode).headerSectionBg; // The first row is a date header.
}

bool Cleanup::HandleCommand(HWND hWnd, WPARAM wParam) {
    const int id = LOWORD(wParam);
    if (id == IDC_CLEANUP_EDIT && HIWORD(wParam) == EN_CHANGE) {
        const int value = ReadValue();
        if (value > 0) {
            if (IsHistoryView()) g_store.cleanup_weeks = value;
            else g_store.cleanup_days = value;
        }
        return true;
    }
    if (id != IDC_CLEANUP_BTN) return false;
    const int value = ReadValue();
    if (value <= 0) {
        SetFocus(g_hCleanupEdit);
        return true;
    }
    if (IsHistoryView()) CleanWorkHistory(hWnd, value);
    else CleanCompletedTasks(hWnd, value);
    g_store.SaveLocal(); // Also remembers the chosen numbers.
    return true;
}
