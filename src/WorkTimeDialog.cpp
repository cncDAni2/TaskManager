#include "WorkTimeDialog.h"
#include "AppState.h"
#include "WorkHistory.h"
#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cwchar>

namespace {
    constexpr int IDC_WORK_CALENDAR = 5001;
    constexpr int IDC_WORK_HOURS = 5002;
    constexpr int IDC_WORK_OK = 5003;
    constexpr int IDC_WORK_CANCEL = 5004;
    constexpr wchar_t DIALOG_CLASS[] = L"TaskManager_ManualWork_Dialog_Class";

    struct DialogState {
        HWND calendar = nullptr;
        HWND hoursEdit = nullptr;
        const WorkHistory::Records* manualRecords = nullptr;
        std::string date;
        int seconds = 0;
        bool accepted = false;
    };

    bool GetSelectedDate(HWND calendar, SYSTEMTIME& selected, std::string& date) {
        if (!SendMessageW(calendar, MCM_GETCURSEL, 0, (LPARAM)&selected)) return false;
        tm selectedDate{};
        selectedDate.tm_year = selected.wYear - 1900;
        selectedDate.tm_mon = selected.wMonth - 1;
        selectedDate.tm_mday = selected.wDay;
        selectedDate.tm_hour = 12;
        time_t timestamp = mktime(&selectedDate);
        if (timestamp == static_cast<time_t>(-1)) return false;
        date = WorkHistory::DateKey(timestamp);
        return true;
    }

    std::wstring FormatHours(int seconds) {
        wchar_t buffer[32]{};
        swprintf_s(buffer, L"%.4f", (std::max)(0, seconds) / 3600.0);
        std::wstring value = buffer;
        while (!value.empty() && value.back() == L'0') value.pop_back();
        if (!value.empty() && value.back() == L'.') value.pop_back();
        std::replace(value.begin(), value.end(), L'.', L',');
        return value;
    }

    void UpdateHoursForSelectedDate(DialogState* state) {
        if (!state->hoursEdit) return;
        SYSTEMTIME selected{};
        std::string date;
        if (!GetSelectedDate(state->calendar, selected, date)) return;
        auto it = state->manualRecords->find(date);
        int seconds = it == state->manualRecords->end() ? 0 : it->second;
        std::wstring value = FormatHours(seconds);
        SetWindowTextW(state->hoursEdit, value.c_str());
    }

    bool ParseHours(const wchar_t* input, int& seconds) {
        std::wstring value = input;
        if (value.empty()) return false;
        size_t decimal = value.find_first_of(L",.");
        if (decimal == 0 || decimal == value.size() - 1) return false;
        if (decimal != std::wstring::npos && value.find_first_of(L",.", decimal + 1) != std::wstring::npos) return false;
        if (decimal != std::wstring::npos && value.size() - decimal - 1 > 4) return false;
        for (wchar_t character : value) {
            if ((character < L'0' || character > L'9') && character != L',' && character != L'.') return false;
        }
        std::replace(value.begin(), value.end(), L',', L'.');
        wchar_t* end = nullptr;
        double hours = std::wcstod(value.c_str(), &end);
        if (end == value.c_str() || *end != L'\0' || !std::isfinite(hours) || hours < 0 || hours >= 1000) return false;
        seconds = static_cast<int>(std::llround(hours * 3600.0));
        return true;
    }

    LRESULT CALLBACK ManualWorkDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            auto* createInfo = reinterpret_cast<CREATESTRUCTW*>(lParam);
            state = static_cast<DialogState*>(createInfo->lpCreateParams);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        }

        switch (message) {
        case WM_CREATE: {
            state->calendar = CreateWindowExW(0, MONTHCAL_CLASSW, L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | MCS_NOTODAYCIRCLE,
                12, 34, 286, 205, hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_WORK_CALENDAR)),
                GetModuleHandleW(nullptr), nullptr);
            SYSTEMTIME today{};
            GetLocalTime(&today);
            SendMessageW(state->calendar, MCM_SETCURSEL, 0, (LPARAM)&today);

            CreateWindowW(L"STATIC", L"Idő (óra):", WS_CHILD | WS_VISIBLE,
                14, 249, 92, 24, hWnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            state->hoursEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                112, 246, 96, 26, hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_WORK_HOURS)),
                GetModuleHandleW(nullptr), nullptr);
            SendMessageW(state->hoursEdit, EM_SETLIMITTEXT, 16, 0);
            UpdateHoursForSelectedDate(state);
            CreateWindowW(L"STATIC", L"óra", WS_CHILD | WS_VISIBLE,
                216, 249, 50, 24, hWnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Mentés", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                112, 292, 88, 28, hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_WORK_OK)),
                GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"BUTTON", L"Mégse", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                208, 292, 88, 28, hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_WORK_CANCEL)),
                GetModuleHandleW(nullptr), nullptr);
            SetFocus(state->hoursEdit);
            return 0;
        }
        case WM_NOTIFY: {
            auto* header = reinterpret_cast<NMHDR*>(lParam);
            if (header && header->idFrom == IDC_WORK_CALENDAR && header->code == MCN_SELCHANGE) {
                UpdateHoursForSelectedDate(state);
                return 0;
            }
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_WORK_CANCEL || LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hWnd);
                return 0;
            }
            if (LOWORD(wParam) == IDC_WORK_OK || LOWORD(wParam) == IDOK) {
                wchar_t input[17]{};
                GetWindowTextW(state->hoursEdit, input, _countof(input));
                int seconds = 0;
                if (!ParseHours(input, seconds)) {
                    MessageBoxW(hWnd, L"Adj meg 0 és 1000 közötti óraszámot, legfeljebb 4 tizedessel.", L"Manuális munkaidő", MB_OK | MB_ICONWARNING);
                    SetFocus(state->hoursEdit);
                    return 0;
                }

                SYSTEMTIME selected{};
                if (!GetSelectedDate(state->calendar, selected, state->date)) return 0;
                state->seconds = seconds;
                state->accepted = true;
                DestroyWindow(hWnd);
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hWnd);
                return 0;
            }
            if (wParam == VK_RETURN) {
                SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDC_WORK_OK, BN_CLICKED), 0);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;
        }
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
}

bool ShowManualWorkDialog(HWND owner, const WorkHistory::Records& manualRecords,
    std::string& date, int& seconds) {
    WNDCLASSW dialogClass{};
    dialogClass.lpfnWndProc = ManualWorkDialogProc;
    dialogClass.hInstance = GetModuleHandleW(nullptr);
    dialogClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    dialogClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    dialogClass.lpszClassName = DIALOG_CLASS;
    if (!RegisterClassW(&dialogClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    DialogState state;
    state.manualRecords = &manualRecords;
    g_inContextMenu = true;
    EnableWindow(owner, FALSE);
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST | WS_EX_CONTROLPARENT,
        DIALOG_CLASS, L"Manuális munkaidő", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 322, 365, owner, nullptr,
        GetModuleHandleW(nullptr), &state);
    if (!dialog) {
        EnableWindow(owner, TRUE);
        g_inContextMenu = false;
        return false;
    }

    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);
    SetWindowPos(dialog, HWND_TOPMOST,
        ownerRect.left + ((ownerRect.right - ownerRect.left) - 322) / 2,
        ownerRect.top + ((ownerRect.bottom - ownerRect.top) - 365) / 2,
        322, 365, SWP_SHOWWINDOW);
    SetFocus(state.hoursEdit);
    SetForegroundWindow(dialog);

    MSG message{};
    while (IsWindow(dialog)) {
        BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result <= 0) {
            if (result == 0) PostQuitMessage(static_cast<int>(message.wParam));
            break;
        }
        if (!IsDialogMessageW(dialog, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    g_inContextMenu = false;
    if (!state.accepted) return false;
    date = state.date;
    seconds = state.seconds;
    return true;
}
