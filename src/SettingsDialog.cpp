#include "SettingsDialog.h"
#include "AppState.h"
#include "Drawing.h"
#include "Layout.h"
#include "Version.h"
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <vector>

#define TASK_MANAGER_WIDEN_INNER(value) L##value
#define TASK_MANAGER_WIDEN(value) TASK_MANAGER_WIDEN_INNER(value)

namespace {
    constexpr int IDC_SETTINGS_PATH = 5101;
    constexpr int IDC_SETTINGS_BROWSE = 5102;
    constexpr int IDC_SETTINGS_THEME = 5103;
    constexpr int IDC_SETTINGS_RESET_TIME = 5104;
    constexpr int IDC_SETTINGS_OK = 5105;
    constexpr int IDC_SETTINGS_SOUNDS = 5106;
    constexpr int IDC_SETTINGS_TIMER_AUTO_START = 5107;
    constexpr int IDC_SETTINGS_SYNC_NAME = 5108;
    constexpr wchar_t APP_VERSION[] = L"v"
        TASK_MANAGER_WIDEN(TASK_MANAGER_STRINGIFY(TASK_MANAGER_VERSION_MAJOR)) L"."
        TASK_MANAGER_WIDEN(TASK_MANAGER_STRINGIFY(TASK_MANAGER_VERSION_MINOR)) L"."
        TASK_MANAGER_WIDEN(TASK_MANAGER_STRINGIFY(TASK_MANAGER_VERSION_PATCH));
    constexpr COLORREF SETTINGS_DIALOG_BG = RGB(245, 245, 245);
    constexpr COLORREF SETTINGS_DIALOG_TEXT = RGB(0, 0, 0);
    constexpr wchar_t DIALOG_CLASS[] = L"TaskManager_Settings_Dialog_Class";

    struct DialogState {
        HWND owner = nullptr;
        HWND pathText = nullptr;
        HWND syncName = nullptr;
        HWND syncTooltip = nullptr;
        HWND themeCombo = nullptr;
        HWND resetTime = nullptr;
        HWND soundsCheckbox = nullptr;
        HWND timerAutoStartCheckbox = nullptr;
        HBRUSH backgroundBrush = nullptr;
        std::wstring syncFilePath;
        int resetHour = 9;
        int resetMinute = 15;
    };

    void SetControlFont(HWND control, HFONT font) {
        if (control && font) SendMessageW(control, WM_SETFONT, (WPARAM)font, TRUE);
    }

    void AddSyncTooltip(HWND tooltip, HWND dialog, HWND control, const wchar_t* text) {
        TOOLINFOW tool{};
        tool.cbSize = sizeof(tool);
        tool.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
        tool.hwnd = dialog;
        tool.uId = reinterpret_cast<UINT_PTR>(control);
        tool.lpszText = const_cast<LPWSTR>(text);
        SendMessageW(tooltip, TTM_ADDTOOLW, 0, (LPARAM)&tool);
    }

    void ApplySelectedTheme(HWND dialog, DialogState* state, ThemeMode mode) {
        g_themeMode = mode;
        g_darkMode = mode == ThemeMode::Dark;
        g_store.theme_mode = mode;
        g_store.SaveLocal();

        ThemeColors theme = GetThemeColors(mode);
        if (g_hEditBrush) DeleteObject(g_hEditBrush);
        g_hEditBrush = CreateSolidBrush(theme.bgEdit);
        ApplyDarkModeTitleBar(state->owner, g_darkMode);
        ApplyDarkModeTitleBar(dialog, false);
        UINT redrawFlags = RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_FRAME;
        RedrawWindow(state->owner, nullptr, nullptr, redrawFlags);
        RedrawWindow(dialog, nullptr, nullptr, redrawFlags);
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
            RECT client{};
            GetClientRect(hWnd, &client);
            constexpr int groupLeft = 12;
            constexpr int groupRightMargin = 12;
            const int groupRight = client.right - groupRightMargin;
            const int groupWidth = groupRight - groupLeft;
            const int numberX = groupLeft + 14;
            const int nameX = groupLeft + 48;
            const int pathX = groupLeft + 140;
            constexpr int browseWidth = 76;
            const int browseX = groupRight - 10 - browseWidth;
            const int nameWidth = pathX - nameX - 8;
            const int pathWidth = browseX - pathX - 8;

            HWND syncGroup = CreateWindowW(L"BUTTON", L"Szinkronizációs file-ok ℹ️",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX, groupLeft, 12, groupWidth, 110,
                hWnd, nullptr, instance, nullptr);
            SetControlFont(syncGroup, g_hFontNormal);

            HWND numberHeader = CreateWindowW(L"STATIC", L"No",
                WS_CHILD | WS_VISIBLE | SS_CENTER, numberX, 45, 30, 18, hWnd, nullptr, instance, nullptr);
            SetControlFont(numberHeader, g_hFontSmall);
            HWND nameHeader = CreateWindowW(L"STATIC", L"Név",
                WS_CHILD | WS_VISIBLE | SS_LEFT, nameX, 45, nameWidth, 18, hWnd, nullptr, instance, nullptr);
            SetControlFont(nameHeader, g_hFontSmall);
            HWND pathHeader = CreateWindowW(L"STATIC", L"Útvonal",
                WS_CHILD | WS_VISIBLE | SS_LEFT, pathX, 45, pathWidth, 18, hWnd, nullptr, instance, nullptr);
            SetControlFont(pathHeader, g_hFontSmall);

            HWND syncNumber = CreateWindowW(L"STATIC", L"1",
                WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
                numberX, 67, 30, 28, hWnd, nullptr, instance, nullptr);
            SetControlFont(syncNumber, g_hFontNormal);
            state->syncName = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"1",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                nameX, 69, nameWidth, 28, hWnd, (HMENU)(INT_PTR)IDC_SETTINGS_SYNC_NAME, instance, nullptr);
            SetControlFont(state->syncName, g_hFontNormal);
            state->pathText = CreateWindowW(L"STATIC", state->syncFilePath.c_str(),
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                pathX, 66, pathWidth, 42, hWnd, (HMENU)(INT_PTR)IDC_SETTINGS_PATH, instance, nullptr);
            SetControlFont(state->pathText, g_hFontNormal);

            CreateWindowW(L"BUTTON", L"Tallózás", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                browseX, 69, browseWidth, 30, hWnd, (HMENU)(INT_PTR)IDC_SETTINGS_BROWSE, instance, nullptr);
            SetControlFont(GetDlgItem(hWnd, IDC_SETTINGS_BROWSE), g_hFontSmall);

            static constexpr wchar_t syncHelp[] =
                L"Olyan fájlt válassz, amelyet valamelyik felhőszolgáltatás automatikusan szinkronizál.\n\n"
                L"OneDrive esetén először ossz meg egy mappát a másik személlyel. A másik fél a "
                L"\u201eParancsikon hozzáadása Saját fájlokhoz\u201d lehetőséggel szinkronizálja a mappát a gépére. "
                L"Ezután a C:\\Users\\<felhasználónév>\\OneDrive - Siemens AG\\ mappában keresd meg a megosztó "
                L"nevét viselő mappát, és tallózd be a megosztott fájlt.";
            state->syncTooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                hWnd, nullptr, instance, nullptr);
            if (state->syncTooltip) {
                SendMessageW(state->syncTooltip, TTM_SETMAXTIPWIDTH, 0, 360);
                TOOLINFOW rowTool{};
                rowTool.cbSize = sizeof(rowTool);
                rowTool.uFlags = TTF_SUBCLASS;
                rowTool.hwnd = hWnd;
                rowTool.uId = 1;
                rowTool.rect = RECT{ groupLeft, 12, groupRight, 122 };
                rowTool.lpszText = const_cast<LPWSTR>(syncHelp);
                SendMessageW(state->syncTooltip, TTM_ADDTOOLW, 0, (LPARAM)&rowTool);
                AddSyncTooltip(state->syncTooltip, hWnd, syncGroup, syncHelp);
                AddSyncTooltip(state->syncTooltip, hWnd, syncNumber, syncHelp);
                AddSyncTooltip(state->syncTooltip, hWnd, state->syncName, syncHelp);
                AddSyncTooltip(state->syncTooltip, hWnd, state->pathText, syncHelp);
                AddSyncTooltip(state->syncTooltip, hWnd, GetDlgItem(hWnd, IDC_SETTINGS_BROWSE), syncHelp);
            }

            HWND themeLabel = CreateWindowW(L"STATIC", L"Téma",
                WS_CHILD | WS_VISIBLE | SS_LEFT, 18, 133, 144, 28, hWnd, nullptr, instance, nullptr);
            SetControlFont(themeLabel, g_hFontNormal);
            CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDVERT,
                170, 119, 2, 54, hWnd, nullptr, instance, nullptr);
            state->themeCombo = CreateWindowW(L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                184, 128, 206, 150, hWnd, (HMENU)(INT_PTR)IDC_SETTINGS_THEME, instance, nullptr);
            SendMessageW(state->themeCombo, CB_ADDSTRING, 0, (LPARAM)L"Sötét");
            SendMessageW(state->themeCombo, CB_ADDSTRING, 0, (LPARAM)L"Világos");
            SendMessageW(state->themeCombo, CB_ADDSTRING, 0, (LPARAM)L"Pink");
            SendMessageW(state->themeCombo, CB_SETCURSEL, static_cast<WPARAM>(g_store.theme_mode), 0);
            SetControlFont(state->themeCombo, g_hFontNormal);

            HWND resetLabel = CreateWindowW(L"STATIC", L"Munkaidő kezdete",
                WS_CHILD | WS_VISIBLE | SS_LEFT, 18, 192, 144, 28, hWnd, nullptr, instance, nullptr);
            SetControlFont(resetLabel, g_hFontNormal);
            CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDVERT,
                170, 179, 2, 54, hWnd, nullptr, instance, nullptr);
            state->resetTime = CreateWindowExW(0, DATETIMEPICK_CLASSW, L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | DTS_TIMEFORMAT | DTS_UPDOWN,
                184, 187, 112, 28, hWnd, (HMENU)(INT_PTR)IDC_SETTINGS_RESET_TIME, instance, nullptr);
            SendMessageW(state->resetTime, DTM_SETFORMATW, 0, (LPARAM)L"HH':'mm");
            SYSTEMTIME selectedTime{};
            GetLocalTime(&selectedTime);
            selectedTime.wHour = static_cast<WORD>(state->resetHour);
            selectedTime.wMinute = static_cast<WORD>(state->resetMinute);
            selectedTime.wSecond = 0;
            DateTime_SetSystemtime(state->resetTime, GDT_VALID, &selectedTime);

            HWND soundsLabel = CreateWindowW(L"STATIC", L"Hangok",
                WS_CHILD | WS_VISIBLE | SS_LEFT, 18, 227, 144, 28, hWnd, nullptr, instance, nullptr);
            SetControlFont(soundsLabel, g_hFontNormal);
            CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDVERT,
                170, 217, 2, 38, hWnd, nullptr, instance, nullptr);
            state->soundsCheckbox = CreateWindowW(L"BUTTON", L"Engedélyezve",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                184, 222, 190, 28, hWnd, (HMENU)(INT_PTR)IDC_SETTINGS_SOUNDS, instance, nullptr);
            SendMessageW(state->soundsCheckbox, BM_SETCHECK,
                g_store.sounds_enabled ? BST_CHECKED : BST_UNCHECKED, 0);
            SetControlFont(state->soundsCheckbox, g_hFontNormal);

            HWND timerAutoStartLabel = CreateWindowW(L"STATIC", L"Munkaidőzítő indítása a programmal",
                WS_CHILD | WS_VISIBLE | SS_LEFT, 18, 261, 144, 40, hWnd, nullptr, instance, nullptr);
            SetControlFont(timerAutoStartLabel, g_hFontNormal);
            CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDVERT,
                170, 258, 2, 40, hWnd, nullptr, instance, nullptr);
            state->timerAutoStartCheckbox = CreateWindowW(L"BUTTON", L"Engedélyezve",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                184, 264, 190, 28, hWnd,
                (HMENU)(INT_PTR)IDC_SETTINGS_TIMER_AUTO_START, instance, nullptr);
            SendMessageW(state->timerAutoStartCheckbox, BM_SETCHECK,
                g_store.timer_start_with_app ? BST_CHECKED : BST_UNCHECKED, 0);
            SetControlFont(state->timerAutoStartCheckbox, g_hFontNormal);

            HWND versionLabel = CreateWindowW(L"STATIC", APP_VERSION,
                WS_CHILD | WS_VISIBLE | SS_LEFT, 18, 318, 120, 24, hWnd, nullptr, instance, nullptr);
            SetControlFont(versionLabel, g_hFontSmall);

            HWND okButton = CreateWindowW(L"BUTTON", L"OK",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                392, 312, 90, 30, hWnd, (HMENU)(INT_PTR)IDC_SETTINGS_OK, instance, nullptr);
            SetControlFont(okButton, g_hFontNormal);
            SetFocus(state->themeCombo);
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
            SetTextColor(dc, SETTINGS_DIALOG_TEXT);
            SetBkMode(dc, TRANSPARENT);
            return (LRESULT)GetStockObject(HOLLOW_BRUSH);
        }
        case WM_CTLCOLORLISTBOX: {
            HDC dc = (HDC)wParam;
            SetTextColor(dc, SETTINGS_DIALOG_TEXT);
            SetBkColor(dc, SETTINGS_DIALOG_BG);
            SetBkMode(dc, OPAQUE);
            return (LRESULT)state->backgroundBrush;
        }
        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wParam;
            SetTextColor(dc, SETTINGS_DIALOG_TEXT);
            SetBkColor(dc, SETTINGS_DIALOG_BG);
            SetBkMode(dc, OPAQUE);
            return (LRESULT)state->backgroundBrush;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_SETTINGS_BROWSE) {
                BrowseSyncFile(hWnd, state);
                return 0;
            }
            if (LOWORD(wParam) == IDC_SETTINGS_THEME && HIWORD(wParam) == CBN_SELCHANGE) {
                int selection = static_cast<int>(SendMessageW(state->themeCombo, CB_GETCURSEL, 0, 0));
                if (selection >= 0 && selection <= static_cast<int>(ThemeMode::Pink)) {
                    ApplySelectedTheme(hWnd, state, static_cast<ThemeMode>(selection));
                }
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
                g_store.sounds_enabled = SendMessageW(state->soundsCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
                g_store.timer_start_with_app = SendMessageW(state->timerAutoStartCheckbox,
                    BM_GETCHECK, 0, 0) == BST_CHECKED;
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
        case WM_DESTROY:
            if (state->syncTooltip && IsWindow(state->syncTooltip)) {
                DestroyWindow(state->syncTooltip);
                state->syncTooltip = nullptr;
            }
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
    state.resetHour = g_store.work_reset_hour;
    state.resetMinute = g_store.work_reset_minute;
    state.owner = owner;
    state.backgroundBrush = CreateSolidBrush(SETTINGS_DIALOG_BG);

    g_inContextMenu = true;
    EnableWindow(owner, FALSE);
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST | WS_EX_CONTROLPARENT,
        DIALOG_CLASS, L"Beállítások", WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 383, owner, nullptr,
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
        ownerRect.top + ((ownerRect.bottom - ownerRect.top) - 383) / 2,
        500, 383, SWP_SHOWWINDOW);
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