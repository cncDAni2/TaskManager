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
    constexpr int IDC_SETTINGS_THEME = 5103;
    constexpr int IDC_SETTINGS_RESET_TIME = 5104;
    constexpr int IDC_SETTINGS_OK = 5105;
    constexpr wchar_t APP_VERSION[] = L"v1.8.0";
    constexpr COLORREF SETTINGS_DIALOG_BG = RGB(245, 245, 245);
    constexpr COLORREF SETTINGS_DIALOG_TEXT = RGB(0, 0, 0);
    constexpr wchar_t DIALOG_CLASS[] = L"TaskManager_Settings_Dialog_Class";

    struct DialogState {
        HWND owner = nullptr;
        HWND pathText = nullptr;
        HWND syncTooltip = nullptr;
        HWND themeCombo = nullptr;
        HWND resetTime = nullptr;
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
            HWND syncLabel = CreateWindowW(L"STATIC", L"Szinkronizációs file ℹ️",
                WS_CHILD | WS_VISIBLE | SS_LEFT, 18, 40, 152, 42, hWnd, nullptr, instance, nullptr);
            SetControlFont(syncLabel, g_hFontNormal);
            HWND syncDivider = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDVERT,
                170, 30, 2, 60, hWnd, nullptr, instance, nullptr);
            state->pathText = CreateWindowW(L"STATIC", state->syncFilePath.c_str(),
                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                184, 34, 206, 54, hWnd, (HMENU)(INT_PTR)IDC_SETTINGS_PATH, instance, nullptr);
            SetControlFont(state->pathText, g_hFontNormal);

            CreateWindowW(L"BUTTON", L"Tallózás", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                402, 43, 80, 30, hWnd, (HMENU)(INT_PTR)IDC_SETTINGS_BROWSE, instance, nullptr);
            SetControlFont(GetDlgItem(hWnd, IDC_SETTINGS_BROWSE), g_hFontSmall);

            static constexpr wchar_t syncHelp[] =
                L"OneDrive-megosztáshoz először ossz meg egy mappát a másik személlyel.\n"
                L"A másik fél a \u201eParancsikon hozzáadása Saját fájlokhoz\u201d gombbal szinkronizálja a mappát a gépére.\n"
                L"Ezután a C:\\Users\\<felhasználónév>\\OneDrive - Siemens AG\\ mappában megjelenik a megosztó személyének nevét viselő mappa. "
                L"A megosztott fájl vagy mappa azon belül található; ezt tallózd be itt.";
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
                rowTool.uId = IDC_SETTINGS_PATH;
                rowTool.rect = RECT{ 10, 30, 490, 100 };
                rowTool.lpszText = const_cast<LPWSTR>(syncHelp);
                SendMessageW(state->syncTooltip, TTM_ADDTOOLW, 0, (LPARAM)&rowTool);
                AddSyncTooltip(state->syncTooltip, hWnd, syncLabel, syncHelp);
                AddSyncTooltip(state->syncTooltip, hWnd, syncDivider, syncHelp);
                AddSyncTooltip(state->syncTooltip, hWnd, state->pathText, syncHelp);
                AddSyncTooltip(state->syncTooltip, hWnd, GetDlgItem(hWnd, IDC_SETTINGS_BROWSE), syncHelp);
            }

            HWND themeLabel = CreateWindowW(L"STATIC", L"Téma",
                WS_CHILD | WS_VISIBLE | SS_LEFT, 18, 119, 144, 28, hWnd, nullptr, instance, nullptr);
            SetControlFont(themeLabel, g_hFontNormal);
            CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDVERT,
                170, 105, 2, 54, hWnd, nullptr, instance, nullptr);
            state->themeCombo = CreateWindowW(L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                184, 114, 206, 150, hWnd, (HMENU)(INT_PTR)IDC_SETTINGS_THEME, instance, nullptr);
            SendMessageW(state->themeCombo, CB_ADDSTRING, 0, (LPARAM)L"Sötét");
            SendMessageW(state->themeCombo, CB_ADDSTRING, 0, (LPARAM)L"Világos");
            SendMessageW(state->themeCombo, CB_ADDSTRING, 0, (LPARAM)L"Pink");
            SendMessageW(state->themeCombo, CB_SETCURSEL, static_cast<WPARAM>(g_store.theme_mode), 0);
            SetControlFont(state->themeCombo, g_hFontNormal);

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

            HWND versionLabel = CreateWindowW(L"STATIC", APP_VERSION,
                WS_CHILD | WS_VISIBLE | SS_LEFT, 18, 242, 120, 24, hWnd, nullptr, instance, nullptr);
            SetControlFont(versionLabel, g_hFontSmall);

            HWND okButton = CreateWindowW(L"BUTTON", L"OK",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                392, 238, 90, 30, hWnd, (HMENU)(INT_PTR)IDC_SETTINGS_OK, instance, nullptr);
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
        case WM_CTLCOLORBTN:
        case WM_CTLCOLORLISTBOX:
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