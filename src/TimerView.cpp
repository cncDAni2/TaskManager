#include "TimerView.h"
#include "AppState.h"
#include "IntervalTimer.h"
#include "Layout.h"
#include <commctrl.h>
#include <string>

namespace {
HWND g_workEdit = nullptr;
HWND g_restEdit = nullptr;
HWND g_repetitionsEdit = nullptr;
HWND g_startButton = nullptr;
HWND g_pauseButton = nullptr;
bool g_viewVisible = false;
bool g_initializing = false;

bool ReadValue(HWND edit, int minimum, int maximum, int& value) {
    const int length = GetWindowTextLengthW(edit);
    if (length <= 0) return false;
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(edit, &text[0], length + 1);
    const int parsed = _wtoi(text.c_str());
    if (parsed < minimum || parsed > maximum) return false;
    value = parsed;
    return true;
}

bool ReadMinutesValue(HWND edit, int& seconds) {
    const int length = GetWindowTextLengthW(edit);
    if (length <= 0) return false;
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(edit, &text[0], length + 1);

    int wholeMinutes = 0;
    int tenth = 0;
    bool hasWholeDigit = false;
    bool hasDecimalSeparator = false;
    bool hasFractionalDigit = false;
    for (wchar_t ch : text) {
        if (ch >= L'0' && ch <= L'9') {
            if (hasDecimalSeparator) {
                if (hasFractionalDigit) return false;
                tenth = ch - L'0';
                hasFractionalDigit = true;
            } else {
                hasWholeDigit = true;
                wholeMinutes = wholeMinutes * 10 + ch - L'0';
            }
        } else if ((ch == L'.' || ch == L',') && !hasDecimalSeparator) {
            hasDecimalSeparator = true;
        } else if (ch != L'\0') {
            return false;
        }
    }

    if (!hasWholeDigit || (hasDecimalSeparator && !hasFractionalDigit)) return false;
    const int tenthsOfMinute = wholeMinutes * 10 + tenth;
    if (tenthsOfMinute < 1 || tenthsOfMinute > 9999) return false;
    seconds = tenthsOfMinute * 6;
    return true;
}

std::wstring FormatMinutes(double minutes) {
    const int tenthsOfMinute = static_cast<int>(minutes * 10.0 + 0.5);
    const int wholeMinutes = tenthsOfMinute / 10;
    const int tenth = tenthsOfMinute % 10;
    if (tenth == 0) return std::to_wstring(wholeMinutes);
    return std::to_wstring(wholeMinutes) + L"," + std::to_wstring(tenth);
}

bool IsTimerEdit(HWND control) {
    return control == g_workEdit || control == g_restEdit || control == g_repetitionsEdit;
}

HWND GetAdjacentTimerControl(HWND currentControl, bool reverse) {
    HWND focusStops[5]{};
    size_t focusStopCount = 0;
    const HWND timerEdits[] = { g_workEdit, g_restEdit, g_repetitionsEdit };
    for (HWND edit : timerEdits) {
        if (IsWindowVisible(edit) && IsWindowEnabled(edit)) focusStops[focusStopCount++] = edit;
    }
    if (IsWindowVisible(g_startButton) && IsWindowEnabled(g_startButton)) {
        focusStops[focusStopCount++] = g_startButton;
    }
    if (IsWindowVisible(g_pauseButton) && IsWindowEnabled(g_pauseButton)) {
        focusStops[focusStopCount++] = g_pauseButton;
    }
    if (focusStopCount == 0) return nullptr;

    for (size_t index = 0; index < focusStopCount; ++index) {
        if (focusStops[index] == currentControl) {
            const size_t nextIndex = reverse
                ? (index + focusStopCount - 1) % focusStopCount
                : (index + 1) % focusStopCount;
            return focusStops[nextIndex];
        }
    }
    return nullptr;
}

LRESULT CALLBACK TimerControlSubclassProc(HWND control, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR /*refData*/) {
    if (message == WM_KEYDOWN && wParam == VK_TAB) {
        const bool reverse = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        HWND nextControl = GetAdjacentTimerControl(control, reverse);
        if (nextControl) {
            SetFocus(nextControl);
            if (IsTimerEdit(nextControl)) SendMessageW(nextControl, EM_SETSEL, 0, -1);
        }
        return 0;
    }
    if (message == WM_KEYDOWN && wParam == VK_RETURN) {
        HWND button = control == g_pauseButton ? g_pauseButton : g_startButton;
        if (IsWindowVisible(button) && IsWindowEnabled(button)) SendMessageW(button, BM_CLICK, 0, 0);
        return 0;
    }
    if (message == WM_KEYDOWN && wParam == VK_SPACE &&
        (control == g_startButton || control == g_pauseButton)) {
        SendMessageW(control, BM_CLICK, 0, 0);
        return 0;
    }
    if (message == WM_CHAR && (wParam == L'\t' || wParam == L'\r' ||
        (wParam == L' ' && (control == g_startButton || control == g_pauseButton)))) return 0;
    if (message == WM_NCDESTROY) RemoveWindowSubclass(control, TimerControlSubclassProc, subclassId);
    return DefSubclassProc(control, message, wParam, lParam);
}

void SaveChangedValue(int id) {
    int value = 0;
    if (id == IDC_TIMER_WORK_EDIT && ReadMinutesValue(g_workEdit, value)) {
        g_store.timer_work_minutes = static_cast<double>(value) / 60.0;
    } else if (id == IDC_TIMER_REST_EDIT && ReadMinutesValue(g_restEdit, value)) {
        g_store.timer_rest_minutes = static_cast<double>(value) / 60.0;
    } else if (id == IDC_TIMER_REPEATS_EDIT && ReadValue(g_repetitionsEdit, 1, 99, value)) {
        g_store.timer_repetitions = value;
    } else {
        return;
    }
    g_store.SaveLocal();
}

}

void TimerView::CreateControls(HWND owner, HINSTANCE instance) {
    g_initializing = true;
    const DWORD editStyle = WS_CHILD | WS_TABSTOP | ES_CENTER | ES_AUTOHSCROLL;
    g_workEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", editStyle,
        0, 0, 136, 32, owner, (HMENU)IDC_TIMER_WORK_EDIT, instance, nullptr);
    g_restEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", editStyle,
        0, 0, 136, 32, owner, (HMENU)IDC_TIMER_REST_EDIT, instance, nullptr);
    g_repetitionsEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", editStyle | ES_NUMBER,
        0, 0, 100, 32, owner, (HMENU)IDC_TIMER_REPEATS_EDIT, instance, nullptr);
    SetWindowSubclass(g_workEdit, TimerControlSubclassProc, IDC_TIMER_WORK_EDIT, 0);
    SetWindowSubclass(g_restEdit, TimerControlSubclassProc, IDC_TIMER_REST_EDIT, 0);
    SetWindowSubclass(g_repetitionsEdit, TimerControlSubclassProc, IDC_TIMER_REPEATS_EDIT, 0);
    g_startButton = CreateWindowW(L"BUTTON", L"Start",
        WS_CHILD | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 104, 32, owner,
        (HMENU)IDC_TIMER_START_BTN, instance, nullptr);
    g_pauseButton = CreateWindowW(L"BUTTON", L"Pause",
        WS_CHILD | BS_OWNERDRAW | WS_TABSTOP, 0, 0, 104, 32, owner,
        (HMENU)IDC_TIMER_PAUSE_BTN, instance, nullptr);
    SetWindowSubclass(g_startButton, TimerControlSubclassProc, IDC_TIMER_START_BTN, 0);
    SetWindowSubclass(g_pauseButton, TimerControlSubclassProc, IDC_TIMER_PAUSE_BTN, 0);

    for (HWND edit : { g_workEdit, g_restEdit, g_repetitionsEdit }) {
        SendMessageW(edit, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    }
    SendMessageW(g_workEdit, EM_SETLIMITTEXT, 5, 0);
    SendMessageW(g_restEdit, EM_SETLIMITTEXT, 5, 0);
    SendMessageW(g_repetitionsEdit, EM_SETLIMITTEXT, 2, 0);
    SendMessageW(g_startButton, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    SendMessageW(g_pauseButton, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    SetWindowTextW(g_workEdit, FormatMinutes(g_store.timer_work_minutes).c_str());
    SetWindowTextW(g_restEdit, FormatMinutes(g_store.timer_rest_minutes).c_str());
    SetWindowTextW(g_repetitionsEdit, std::to_wstring(g_store.timer_repetitions).c_str());
    g_initializing = false;
    RefreshRunState();
    Layout(owner);
}

void TimerView::UpdateVisibility(bool visible) {
    g_viewVisible = visible && !g_isMiniMode;
    const int show = g_viewVisible ? SW_SHOW : SW_HIDE;
    ShowWindow(g_workEdit, show);
    ShowWindow(g_restEdit, show);
    ShowWindow(g_repetitionsEdit, show);
    ShowWindow(g_startButton, show);
    ShowWindow(g_pauseButton, g_viewVisible && g_intervalTimer.IsActive() ? SW_SHOW : SW_HIDE);
    if (g_hWnd) Layout(g_hWnd);
}

void TimerView::RefreshRunState() {
    const bool active = g_intervalTimer.IsActive();
    EnableWindow(g_workEdit, !active);
    EnableWindow(g_restEdit, !active);
    EnableWindow(g_repetitionsEdit, !active);
    SetWindowTextW(g_startButton, active ? L"Stop" : L"Start");
    SetWindowTextW(g_pauseButton, g_intervalTimer.IsPaused() ? L"Folytatás" : L"Pause");
    ShowWindow(g_pauseButton, g_viewVisible && active ? SW_SHOW : SW_HIDE);
    if (g_hWnd) Layout(g_hWnd);
}

void TimerView::Layout(HWND owner) {
    if (!owner) return;
    RECT client{};
    GetClientRect(owner, &client);
    const int width = client.right - client.left;
    const int rowHeight = 32;
    const int rowGap = 46;
    const RECT panel = GetTimerPanelRect(width);
    const int firstRowY = panel.top + 18;
    const int inputX = panel.left + 162;
    const int inputWidth = 82;
    SetWindowPos(g_workEdit, nullptr, inputX, firstRowY, inputWidth, rowHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g_restEdit, nullptr, inputX, firstRowY + rowGap, inputWidth, rowHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g_repetitionsEdit, nullptr, inputX, firstRowY + rowGap * 2,
        inputWidth, rowHeight, SWP_NOZORDER | SWP_NOACTIVATE);

    const int buttonY = panel.bottom - 52;
    const bool active = g_intervalTimer.IsActive();
    const int groupWidth = active ? 216 : 104;
    const int buttonLeft = (width - groupWidth) / 2;
    SetWindowPos(g_startButton, nullptr, buttonLeft, buttonY, 104, 32,
        SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g_pauseButton, nullptr, buttonLeft + 112, buttonY, 104, 32,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

bool TimerView::HandleCommand(HWND owner, WPARAM wParam) {
    const int id = LOWORD(wParam);
    const int notification = HIWORD(wParam);
    if (notification == EN_CHANGE && !g_initializing && !g_intervalTimer.IsActive()) {
        if (id == IDC_TIMER_WORK_EDIT || id == IDC_TIMER_REST_EDIT || id == IDC_TIMER_REPEATS_EDIT) {
            SaveChangedValue(id);
            return true;
        }
    }

    if (notification != BN_CLICKED) return false;
    if (id == IDC_TIMER_START_BTN) {
        if (g_intervalTimer.IsActive()) {
            g_intervalTimer.Stop();
        } else {
            int workSeconds = 0;
            int restSeconds = 0;
            int repetitions = 0;
            if (!ReadMinutesValue(g_workEdit, workSeconds)) {
                if (g_store.sounds_enabled) MessageBeep(MB_ICONWARNING);
                SetFocus(g_workEdit);
                return true;
            }
            if (!ReadMinutesValue(g_restEdit, restSeconds)) {
                if (g_store.sounds_enabled) MessageBeep(MB_ICONWARNING);
                SetFocus(g_restEdit);
                return true;
            }
            if (!ReadValue(g_repetitionsEdit, 1, 99, repetitions)) {
                if (g_store.sounds_enabled) MessageBeep(MB_ICONWARNING);
                SetFocus(g_repetitionsEdit);
                return true;
            }
            g_store.timer_work_minutes = static_cast<double>(workSeconds) / 60.0;
            g_store.timer_rest_minutes = static_cast<double>(restSeconds) / 60.0;
            g_store.timer_repetitions = repetitions;
            g_store.SaveLocal();
            g_intervalTimer.Start(workSeconds, restSeconds, repetitions);
        }
    } else if (id == IDC_TIMER_PAUSE_BTN) {
        if (g_intervalTimer.IsPaused()) {
            g_intervalTimer.Resume();
        } else {
            g_intervalTimer.Pause();
        }
    } else {
        return false;
    }

    RefreshRunState();
    UpdateControlsVisibility();
    RecalculateLayout();
    InvalidateRect(owner, nullptr, FALSE);
    return true;
}