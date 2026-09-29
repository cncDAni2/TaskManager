#include "SettingsDialog.h"
#include "AppState.h"
#include "Drawing.h"
#include "Layout.h"
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <vector>

namespace {
    constexpr int IDC_SETTINGS_PATH = 5101;
    constexpr int IDC_SETTINGS_BROWSE = 5102;
    constexpr int IDC_SETTINGS_DARK_MODE = 5103;
    constexpr int IDC_SETTINGS_RESET_TIME = 5104;
    constexpr int IDC_SETTINGS_OK = 5105;
    constexpr wchar_t DIALOG_CLASS[] = L"TaskManager_Settings_Dialog_Class";

    struct DialogState {
        HWND owner = nullptr;
        HWND pathText = nullptr;
        HWND darkModeCheck = nullptr;
        HWND resetTime = nullptr;
        HBRUSH backgroundBrush = nullptr;
        std::wstring syncFilePath;
        bool darkMode = true;
        int resetHour = 9;
        int resetMinute = 15;
    };

    void SetControlFont(HWND control, HFONT font) {
        if (control && font) SendMessageW(control, WM_SETFONT, (WPARAM)font, TRUE);
    }

    void BrowseSyncFile(HWND owner, DialogState* state) {
        std::vector<wchar_t> filePath(32768, L'\0');
        if (!state->syncFilePath.empty()) {
            wcscpy_s(filePath.data(), filePath.size(), state->syncFilePath.c_str());
        }

        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = owner;
        ofn.lpstrFile = filePath.data();
        ofn.nMaxFile = static_cast<DWORD>(filePath.size());
        ofn.lpstrFilter = L"JSON fájlok (*.json)\0*.json\0Minden fájl (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrTitle = L"Szinkronizált feladatok fájljának kiválasztása";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_ENABLESIZING | OFN_NOCHANGEDIR;

        std::wstring initialDirectory;
        size_t lastSlash = state->syncFilePath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            initialDirectory = state->syncFilePath.substr(0, lastSlash);
            ofn.lpstrInitialDir = initialDirectory.c_str();
        }

        if (GetOpenFileNameW(&ofn)) {
            state->syncFilePath = filePath.data();
            SetWindowTextW(state->pathText, state->syncFilePath.c_str());
            g_store.SetSyncFilePath(state->syncFilePath);
            RecalculateLayout();
            InvalidateRect(state->owner, nullptr, TRUE);
        }
    }

    LRESULT CALLBACK SettingsDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            auto* createInfo = reinterpret_cast<CREATESTRUCTW*>(lParam);
            state = static_cast<DialogState*>(createInfo->lpCreateParams);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        }

        switch (message) {
        case WM_CREATE: {
            HINSTANCE instance = GetModuleHandleW(nullptr);
            HWND syncLabel = CreateWindowW(L"STATIC", L"Szinkronizációs file",
                WS_CHILD | WS_VISIBLE | SS_LEFT, 18, 40, 144, 42, hWnd, nullptr, instance, nullptr);
            SetControlFont(syncLabel, g_hFontNormal);
            CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDVERT,
                170, 30, 2, 60, hWnd, nullptr, instance, nullptr);
            state->pathText = CreateWindowW(L"STATIC", state->syncFilePath.c_str(),
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                184, 34, 206, 54, hWnd, (HMENU)(INT_PTR)IDC_SETTINGS_PATH, instance, nullptr);
            SetControlFont(state->pathText, g_hFontNormal);

            CreateWindowW(L"BUTTON", L"Tallózás", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                402, 43, 80, 30, hWnd, (HMENU)(INT_PTR)IDC_SETTINGS_BROWSE, instance, nullptr);
            SetControlFont(GetDlgItem(hWnd, IDC_SETTINGS_BROWSE), g_hFontSmall);

            HWND darkModeLabel = CreateWindowW(L"STATIC", L"Sötét mód",
                WS_CHILD | WS_VISIBLE | SS_LEFT, 18, 119, 144, 28, hWnd, nullptr, instance, nullptr);
            SetControlFont(darkModeLabel, g_hFontNormal);
            CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDVERT,
                170, 105, 2, 54, hWnd, nullptr, instance, nullptr);
            state->darkModeCheck = CreateWindowW(L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                184, 118, 24, 26, hWnd, (HMENU)(INT_PTR)IDC_SETTINGS_DARK_MODE, instance, nullptr);
            SendMessageW(state->darkModeCheck, BM_SETCHECK,
                state->darkMode ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND resetLabel = CreateWindowW(L"STATIC", L"Munkaidő kezdete",
                WS_CHILD | WS_VISIBLE | SS_LEFT, 18, 178, 144, 28, hWnd, nullptr, instance, nullptr);
            SetControlFont(resetLabel, g_hFontNormal);
            CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDVERT,
                170, 165, 2, 54, hWnd, nullptr, instance, nullptr);
            state->resetTime = CreateWindowExW(0, DATETIMEPICK_CLASSW, L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | DTS_TIMEFORMAT | DTS_UPDOWN,
                184, 173, 112, 28, hWnd, (HMENU)(INT_PTR)IDC_SETTINGS_RESET_TIME, instance, nullptr);
            SendMessageW(state->resetTime, DTM_SETFORMATW, 0, (LPARAM)L"HH':'mm");
            SYSTEMTIME selectedTime{};
            GetLocalTime(&selectedTime);
            selectedTime.wHour = static_cast<WORD>(state->resetHour);
            selectedTime.wMinute = static_cast<WORD>(state->resetMinute);
            selectedTime.wSecond = 0;
            DateTime_SetSystemtime(state->resetTime, GDT_VALID, &selectedTime);

            HWND okButton = CreateWindowW(L"BUTTON", L"OK",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                392, 238, 90, 30, hWnd, (HMENU)(INT_PTR)IDC_SETTINGS_OK, instance, nullptr);
            SetControlFont(okButton, g_hFontNormal);
            SetFocus(state->darkModeCheck);
            return 0;
        }
        case WM_ERASEBKGND: {
            RECT client{};
            GetClientRect(hWnd, &client);
            FillRect((HDC)wParam, &client, state->backgroundBrush);
            return 1;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC dc = (HDC)wParam;
            SetTextColor(dc, RGB(0, 0, 0));
            SetBkColor(dc, RGB(245, 245, 245));
            SetBkMode(dc, OPAQUE);
            return (LRESULT)state->backgroundBrush;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_SETTINGS_BROWSE) {
                BrowseSyncFile(hWnd, state);
                return 0;
            }
            if (LOWORD(wParam) == IDC_SETTINGS_DARK_MODE && HIWORD(wParam) == BN_CLICKED) {
                state->darkMode = SendMessageW(state->darkModeCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
                g_store.dark_mode = state->darkMode;
                g_darkMode = state->darkMode;
                g_store.SaveLocal();
                ThemeColors theme = g_darkMode ? GetDarkTheme() : GetLightTheme();
                if (g_hEditBrush) DeleteObject(g_hEditBrush);
                g_hEditBrush = CreateSolidBrush(theme.bgEdit);
                ApplyDarkModeTitleBar(state->owner, g_darkMode);
                if (g_hPinBtn) InvalidateRect(g_hPinBtn, nullptr, TRUE);
                if (g_hFocusModeBtn) InvalidateRect(g_hFocusModeBtn, nullptr, TRUE);
                if (g_hSettingsBtn) InvalidateRect(g_hSettingsBtn, nullptr, TRUE);
                InvalidateRect(state->owner, nullptr, TRUE);
                return 0;
            }
            if (LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hWnd);
                return 0;
            }
            if (LOWORD(wParam) == IDC_SETTINGS_OK || LOWORD(wParam) == IDOK) {
                SYSTEMTIME selectedTime{};
                if (DateTime_GetSystemtime(state->resetTime, &selectedTime) != GDT_VALID) return 0;
                g_store.work_reset_hour = selectedTime.wHour;
                g_store.work_reset_minute = selectedTime.wMinute;
                g_store.SaveLocal();
                g_store.CheckWorkReset();
                DestroyWindow(hWnd);
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hWnd);
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

void ShowSettingsDialog(HWND owner) {
    WNDCLASSW dialogClass{};
    dialogClass.lpfnWndProc = SettingsDialogProc;
    dialogClass.hInstance = GetModuleHandleW(nullptr);
    dialogClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    dialogClass.lpszClassName = DIALOG_CLASS;
    if (!RegisterClassW(&dialogClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;

    DialogState state;
    state.syncFilePath = g_store.syncFilePath;
    state.darkMode = g_store.dark_mode;
    state.resetHour = g_store.work_reset_hour;
    state.resetMinute = g_store.work_reset_minute;
    state.owner = owner;
    state.backgroundBrush = CreateSolidBrush(RGB(245, 245, 245));

    g_inContextMenu = true;
    EnableWindow(owner, FALSE);
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST | WS_EX_CONTROLPARENT,
        DIALOG_CLASS, L"Beállítások", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 310, owner, nullptr,
        GetModuleHandleW(nullptr), &state);
    if (!dialog) {
        EnableWindow(owner, TRUE);
        g_inContextMenu = false;
        DeleteObject(state.backgroundBrush);
        return;
    }

    ApplyDarkModeTitleBar(dialog, false);
    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);
    SetWindowPos(dialog, HWND_TOPMOST,
        ownerRect.left + ((ownerRect.right - ownerRect.left) - 500) / 2,
        ownerRect.top + ((ownerRect.bottom - ownerRect.top) - 310) / 2,
        500, 310, SWP_SHOWWINDOW);
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
    DeleteObject(state.backgroundBrush);
}