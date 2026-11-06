#include "TaskSearch.h"
#include "AppState.h"
#include "Layout.h"
#include <commctrl.h>

namespace {
constexpr int BUTTON_SIZE = 24;
constexpr int FIELD_WIDTH = 230;
constexpr int HEIGHT = 22;

HWND g_button = nullptr;
HWND g_field = nullptr;
HWND g_close = nullptr;
bool g_active = false;
std::wstring g_query;

void AddTooltip(HWND tooltip, HWND parent, HWND control, const wchar_t* text) {
    TOOLINFOW tool{};
    tool.cbSize = sizeof(tool);
    tool.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    tool.hwnd = parent;
    tool.uId = reinterpret_cast<UINT_PTR>(control);
    tool.lpszText = const_cast<LPWSTR>(text);
    SendMessageW(tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
}

void RefreshList() {
    g_scrollY = 0;
    g_selectedIndex = -1;
    RecalculateLayout();
    InvalidateRect(g_hWnd, nullptr, FALSE);
}

void Open() {
    g_active = true;
    UpdateControlsVisibility();
    RefreshList();
    SetFocus(g_field);
}

void FocusFirstResult() {
    for (size_t i = 0; i < g_displayItems.size(); ++i) {
        if (g_displayItems[i].isHeader) continue;
        g_selectedIndex = static_cast<int>(i);
        EnsureVisible(g_selectedIndex);
        SetFocus(g_hWnd);
        InvalidateRect(g_hWnd, nullptr, FALSE);
        return;
    }
}

LRESULT CALLBACK FieldProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) {
    if (message == WM_KEYDOWN && (wParam == VK_DOWN || wParam == VK_RETURN || wParam == VK_TAB)) {
        FocusFirstResult();
        return 0;
    }
    // Otherwise the edit control beeps at them.
    if (message == WM_CHAR && (wParam == VK_RETURN || wParam == VK_TAB || wParam == VK_ESCAPE)) return 0;
    return DefSubclassProc(hWnd, message, wParam, lParam);
}
}

void TaskSearch::CreateControls(HWND parent, HINSTANCE instance) {
    g_button = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, BUTTON_SIZE, HEIGHT, parent, (HMENU)IDC_SEARCH_BTN, instance, nullptr);
    g_close = CreateWindowW(L"BUTTON", L"✕", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, BUTTON_SIZE, HEIGHT, parent, (HMENU)IDC_SEARCH_CLOSE_BTN, instance, nullptr);
    SendMessageW(g_close, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    g_field = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, FIELD_WIDTH, HEIGHT, parent, (HMENU)IDC_SEARCH_EDIT, instance, nullptr);
    UiScale::SetControlFont(g_field, 14);
    SendMessageW(g_field, EM_SETCUEBANNER, TRUE, (LPARAM)L"Keresés...");
    SetWindowSubclass(g_field, FieldProc, 0, 0);

    HWND tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        parent, nullptr, instance, nullptr);
    if (tooltip) {
        AddTooltip(tooltip, parent, g_button, L"Keresés");
        AddTooltip(tooltip, parent, g_close, L"Keresés bezárása (Esc)");
    }
}

bool TaskSearch::IsActive() {
    return g_active;
}

bool TaskSearch::Matches(const std::wstring& text) {
    return !g_active || TaskUtils::ContainsText(text, g_query);
}

void TaskSearch::UpdateVisibility(int clientWidth, int cleanupLeft) {
    if (!g_button) return;
    const bool view = !g_isMiniMode && g_viewMode == ViewMode::CompletedTasks;
    if (!view && g_active) Reset();
    ShowWindow(g_button, view && !g_active ? SW_SHOW : SW_HIDE);
    ShowWindow(g_field, g_active ? SW_SHOW : SW_HIDE);
    ShowWindow(g_close, g_active ? SW_SHOW : SW_HIDE);
    if (!view) return;

    const int top = GetListTop() + 4;
    if (g_active) {
        const int closeX = clientWidth - 14 - BUTTON_SIZE;
        UiScale::Place(g_close, HWND_TOP, closeX, top, BUTTON_SIZE, HEIGHT, SWP_NOACTIVATE);
        UiScale::Place(g_field, HWND_TOP, closeX - 4 - FIELD_WIDTH, top, FIELD_WIDTH, HEIGHT, SWP_NOACTIVATE);
    } else {
        UiScale::Place(g_button, HWND_TOP, cleanupLeft - 4 - BUTTON_SIZE, top, BUTTON_SIZE, HEIGHT, SWP_NOACTIVATE);
    }
}

bool TaskSearch::HandleCommand(WPARAM wParam) {
    const int id = LOWORD(wParam);
    if (id == IDC_SEARCH_BTN) {
        Open();
        return true;
    }
    if (id == IDC_SEARCH_CLOSE_BTN) {
        Close();
        return true;
    }
    if (id != IDC_SEARCH_EDIT) return false;
    if (HIWORD(wParam) == EN_CHANGE && g_active) {
        const int length = GetWindowTextLengthW(g_field);
        std::wstring text(static_cast<size_t>(length) + 1, L'\0');
        GetWindowTextW(g_field, text.data(), length + 1);
        text.resize(static_cast<size_t>(length));
        g_query = std::move(text);
        RefreshList();
    }
    return true;
}

void TaskSearch::Close() {
    Reset();
    UpdateControlsVisibility();
    RefreshList();
    SetFocus(g_hWnd);
}

void TaskSearch::Reset() {
    g_active = false;
    g_query.clear();
    if (g_field) SetWindowTextW(g_field, L"");
}

void TaskSearch::FocusField() {
    g_selectedIndex = -1;
    SetFocus(g_field);
    InvalidateRect(g_hWnd, nullptr, FALSE);
}
