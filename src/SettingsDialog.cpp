#include "SettingsDialog.h"
#include "AppState.h"
#include "Drawing.h"
#include "ExcludedApps.h"
#include "Layout.h"
#include "SettingsSyncRows.h"
#include "SyncPicker.h"
#include "Version.h"
#include <commctrl.h>
#include <string>
#include <vector>

#define TASK_MANAGER_WIDEN_INNER(value) L##value
#define TASK_MANAGER_WIDEN(value) TASK_MANAGER_WIDEN_INNER(value)

namespace {
    constexpr int IDC_SETTINGS_THEME = 5103;
    constexpr int IDC_SETTINGS_RESET_TIME = 5104;
    constexpr int IDC_SETTINGS_OK = 5105;
    constexpr int IDC_SETTINGS_SOUNDS = 5106;
    constexpr int IDC_SETTINGS_TIMER_AUTO_START = 5107;
    constexpr int IDC_SETTINGS_EXCLUDED_KEYWORDS = 5109;
    constexpr int SETTINGS_DIALOG_WIDTH = 560;
    constexpr UINT WM_APP_REBUILD = WM_APP + 1;
    constexpr DWORD DIALOG_STYLE = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    constexpr DWORD DIALOG_EX_STYLE = WS_EX_DLGMODALFRAME | WS_EX_TOPMOST | WS_EX_CONTROLPARENT;
    constexpr wchar_t APP_VERSION[] = L"v"
        TASK_MANAGER_WIDEN(TASK_MANAGER_STRINGIFY(TASK_MANAGER_VERSION_MAJOR)) L"."
        TASK_MANAGER_WIDEN(TASK_MANAGER_STRINGIFY(TASK_MANAGER_VERSION_MINOR)) L"."
        TASK_MANAGER_WIDEN(TASK_MANAGER_STRINGIFY(TASK_MANAGER_VERSION_PATCH));
    constexpr COLORREF SETTINGS_DIALOG_BG = RGB(245, 245, 245);
    constexpr COLORREF SETTINGS_DIALOG_TEXT = RGB(0, 0, 0);
    constexpr wchar_t DIALOG_CLASS[] = L"TaskManager_Settings_Dialog_Class";

    // Working copy of every value, so the dialog content can be rebuilt without losing edits.
    struct DialogState {
        HWND owner = nullptr;
        HWND tooltip = nullptr;
        HWND themeCombo = nullptr;
        HWND resetTime = nullptr;
        HWND soundsCheckbox = nullptr;
        HWND timerAutoStartCheckbox = nullptr;
        HWND excludedKeywordsEdit = nullptr;
        HWND okButton = nullptr;
        HBRUSH backgroundBrush = nullptr;
        bool building = false;
        bool resetDirty = false; // Only the work-reset time waits for "Mentés"; everything else applies instantly.
        std::vector<SettingsSyncRows::Row> syncRows;
        SettingsSyncRows::Controls syncControls;
        int resetHour = 9;
        int resetMinute = 15;
        bool soundsEnabled = true;
        bool timerAutoStart = false;
        std::wstring excludedKeywords;
    };

    HWND CreateChild(HWND dialog, const wchar_t* className, const wchar_t* text, DWORD style,
        int x, int y, int width, int height, int id, HFONT font, DWORD exStyle = 0) {
        HWND control = CreateWindowExW(exStyle, className, text, WS_CHILD | WS_VISIBLE | style,
            x, y, width, height, dialog, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
        if (control && font) SendMessageW(control, WM_SETFONT, (WPARAM)font, TRUE);
        return control;
    }

    void AddTooltip(HWND tooltip, HWND dialog, HWND control, const wchar_t* text) {
        if (!tooltip || !control) return;
        TOOLINFOW tool{};
        tool.cbSize = sizeof(tool);
        tool.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
        tool.hwnd = dialog;
        tool.uId = reinterpret_cast<UINT_PTR>(control);
        tool.lpszText = const_cast<LPWSTR>(text);
        SendMessageW(tooltip, TTM_ADDTOOLW, 0, (LPARAM)&tool);
    }

    std::wstring ReadText(HWND control) {
        const int length = GetWindowTextLengthW(control);
        std::wstring text(static_cast<size_t>(length) + 1, wchar_t());
        GetWindowTextW(control, text.data(), length + 1);
        text.resize(static_cast<size_t>(length));
        return text;
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

    void SnapshotControls(DialogState* state) {
        SettingsSyncRows::Snapshot(state->syncControls, state->syncRows);
        SYSTEMTIME selectedTime{};
        if (DateTime_GetSystemtime(state->resetTime, &selectedTime) == GDT_VALID) {
            state->resetHour = selectedTime.wHour;
            state->resetMinute = selectedTime.wMinute;
        }
        state->soundsEnabled = SendMessageW(state->soundsCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
        state->timerAutoStart = SendMessageW(state->timerAutoStartCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
        state->excludedKeywords = ReadText(state->excludedKeywordsEdit);
    }

    // Label | separator for one settings row of `rowHeight` starting at `top`. Returns the label.
    HWND CreateRow(HWND dialog, const wchar_t* text, int top, int rowHeight, bool notify) {
        const bool twoLines = rowHeight > 40;
        const int labelHeight = twoLines ? 40 : 28;
        // Single-line labels align with the control's text baseline (control top + 5).
        const int labelY = twoLines ? top + (rowHeight - labelHeight) / 2 : top + (rowHeight - 28) / 2 + 5;
        HWND label = CreateChild(dialog, L"STATIC", text, SS_LEFT | (notify ? SS_NOTIFY : 0),
            18, labelY, 144, labelHeight, 0, g_hFontNormal);
        CreateChild(dialog, L"STATIC", L"", SS_ETCHEDVERT, 170, top + 3, 2, rowHeight - 6, 0, nullptr);
        return label;
    }

    // Creates every control from `state` and sizes the dialog to fit; safe to call again after
    // destroying the children (used when sync rows are added or removed).
    void BuildControls(HWND hWnd, DialogState* state) {
        state->building = true;
        RECT client{};
        GetClientRect(hWnd, &client);
        constexpr int left = 12;
        const int right = client.right - 12;

        state->tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
            WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            hWnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (state->tooltip) SendMessageW(state->tooltip, TTM_SETMAXTIPWIDTH, 0, 380);

        const int groupBottom = SettingsSyncRows::Create(hWnd, state->tooltip, state->syncRows,
            state->syncControls, 12, left, right);

        // Uniform rows below the sync table: label | separator | 28px control, vertically centered.
        constexpr int ROW = 38;
        constexpr int TALL_ROW = 46; // Two-line label.
        int top = groupBottom + 6;
        auto controlY = [&top](int rowHeight) { return top + (rowHeight - 28) / 2; };

        CreateRow(hWnd, L"Téma", top, ROW, false);
        state->themeCombo = CreateChild(hWnd, L"COMBOBOX", L"",
            WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 184, controlY(ROW), 206, 150,
            IDC_SETTINGS_THEME, g_hFontNormal);
        SendMessageW(state->themeCombo, CB_ADDSTRING, 0, (LPARAM)L"Sötét");
        SendMessageW(state->themeCombo, CB_ADDSTRING, 0, (LPARAM)L"Világos");
        SendMessageW(state->themeCombo, CB_ADDSTRING, 0, (LPARAM)L"Pink");
        SendMessageW(state->themeCombo, CB_SETCURSEL, static_cast<WPARAM>(g_store.theme_mode), 0);
        top += ROW;

        CreateRow(hWnd, L"Munkaidő kezdete", top, ROW, false);
        state->resetTime = CreateChild(hWnd, DATETIMEPICK_CLASSW, L"",
            WS_TABSTOP | DTS_TIMEFORMAT | DTS_UPDOWN, 184, controlY(ROW), 112, 28, IDC_SETTINGS_RESET_TIME, nullptr);
        SendMessageW(state->resetTime, DTM_SETFORMATW, 0, (LPARAM)L"HH':'mm");
        SYSTEMTIME selectedTime{};
        GetLocalTime(&selectedTime);
        selectedTime.wHour = static_cast<WORD>(state->resetHour);
        selectedTime.wMinute = static_cast<WORD>(state->resetMinute);
        selectedTime.wSecond = 0;
        DateTime_SetSystemtime(state->resetTime, GDT_VALID, &selectedTime);
        top += ROW;

        CreateRow(hWnd, L"Hangok", top, ROW, false);
        state->soundsCheckbox = CreateChild(hWnd, L"BUTTON", L"Engedélyezve",
            WS_TABSTOP | BS_AUTOCHECKBOX, 184, controlY(ROW), 190, 28, IDC_SETTINGS_SOUNDS, g_hFontNormal);
        SendMessageW(state->soundsCheckbox, BM_SETCHECK, state->soundsEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
        top += ROW;

        CreateRow(hWnd, L"Munkaidőzítő indítása a programmal", top, TALL_ROW, false);
        state->timerAutoStartCheckbox = CreateChild(hWnd, L"BUTTON", L"Engedélyezve",
            WS_TABSTOP | BS_AUTOCHECKBOX, 184, controlY(TALL_ROW), 190, 28,
            IDC_SETTINGS_TIMER_AUTO_START, g_hFontNormal);
        SendMessageW(state->timerAutoStartCheckbox, BM_SETCHECK,
            state->timerAutoStart ? BST_CHECKED : BST_UNCHECKED, 0);
        top += TALL_ROW;

        static constexpr wchar_t excludedHelp[] =
            L"Vesszővel elválasztott kulcsszavak. Ha az előtérben lévő ablak címe tartalmazza "
            L"valamelyiket, a munkaidő mérése szünetel. A kis- és nagybetű nem számít. "
            L"Fókusz módban a mérés ilyenkor is folytatódik.";
        HWND excludedLabel = CreateRow(hWnd, L"Kizárt ablakcímek", top, ROW, true);
        state->excludedKeywordsEdit = CreateChild(hWnd, L"EDIT", state->excludedKeywords.c_str(),
            WS_TABSTOP | ES_AUTOHSCROLL, 184, controlY(ROW), right - 184, 28,
            IDC_SETTINGS_EXCLUDED_KEYWORDS, g_hFontNormal, WS_EX_CLIENTEDGE);
        SendMessageW(state->excludedKeywordsEdit, EM_SETCUEBANNER, TRUE,
            (LPARAM)L"pl. YouTube, Discord, Facebook");
        AddTooltip(state->tooltip, hWnd, excludedLabel, excludedHelp);
        AddTooltip(state->tooltip, hWnd, state->excludedKeywordsEdit, excludedHelp);
        top += ROW;

        const int y = top + 16;
        CreateChild(hWnd, L"STATIC", APP_VERSION, SS_LEFT, 18, y + 6, 120, 24, 0, g_hFontSmall);
        state->okButton = CreateChild(hWnd, L"BUTTON", state->resetDirty ? L"Mentés" : L"OK",
            WS_TABSTOP | BS_OWNERDRAW, right - 90, y, 90, 30, IDC_SETTINGS_OK, g_hFontNormal);
        AddTooltip(state->tooltip, hWnd, state->okButton,
            L"A beállítások azonnal mentődnek. Csak a munkaidő kezdetének módosítását kell a Mentés gombbal jóváhagyni.");

        RECT desired = { 0, 0, SETTINGS_DIALOG_WIDTH, y + 30 + 14 };
        AdjustWindowRectEx(&desired, DIALOG_STYLE, FALSE, DIALOG_EX_STYLE);
        RECT current{};
        GetWindowRect(hWnd, &current);
        SetWindowPos(hWnd, nullptr, 0, 0, current.right - current.left, desired.bottom - desired.top,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        state->building = false;
    }

    BOOL CALLBACK DestroyChild(HWND child, LPARAM) {
        DestroyWindow(child);
        return TRUE;
    }

    void RebuildControls(HWND hWnd, DialogState* state) {
        SendMessageW(hWnd, WM_SETREDRAW, FALSE, 0);
        if (state->tooltip && IsWindow(state->tooltip)) DestroyWindow(state->tooltip);
        state->tooltip = nullptr;
        EnumChildWindows(hWnd, DestroyChild, 0);
        BuildControls(hWnd, state);
        SendMessageW(hWnd, WM_SETREDRAW, TRUE, 0);
        RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
    }

    void RefreshMainWindow(DialogState* state) {
        SyncPicker::ResetSelection();
        UpdateControlsVisibility();
        RecalculateLayout();
        InvalidateRect(state->owner, nullptr, TRUE);
    }

    // Applies the sync table immediately. A structural change (file added, replaced or removed)
    // reloads the files and creates new ones; names and default flags are just saved.
    void ApplySyncRows(DialogState* state, bool structural) {
        std::vector<SyncSource> sources = SettingsSyncRows::ToSources(state->syncRows);
        if (structural) {
            g_store.SetSyncSources(sources);
            state->syncRows = SettingsSyncRows::FromSources(g_store.syncSources); // Picks up new ids.
        } else {
            for (const SyncSource& source : sources) {
                for (SyncSource& existing : g_store.syncSources) {
                    if (existing.id != source.id) continue;
                    existing.name = source.name;
                    existing.isDefault = source.isDefault;
                }
            }
            g_store.SaveLocal();
        }
        RefreshMainWindow(state);
    }

    void UpdateResetDirty(DialogState* state) {
        SYSTEMTIME selectedTime{};
        if (DateTime_GetSystemtime(state->resetTime, &selectedTime) != GDT_VALID) return;
        state->resetHour = selectedTime.wHour;
        state->resetMinute = selectedTime.wMinute;
        const bool dirty = state->resetHour != g_store.work_reset_hour ||
            state->resetMinute != g_store.work_reset_minute;
        if (dirty == state->resetDirty) return;
        state->resetDirty = dirty;
        SetWindowTextW(state->okButton, dirty ? L"Mentés" : L"OK");
        InvalidateRect(state->okButton, nullptr, TRUE);
    }

    void SaveAndClose(HWND hWnd, DialogState* state) {
        if (state->resetDirty) {
            g_store.work_reset_hour = state->resetHour;
            g_store.work_reset_minute = state->resetMinute;
            g_store.SaveLocal();
            g_store.CheckWorkReset();
            InvalidateRect(state->owner, nullptr, TRUE);
        }
        DestroyWindow(hWnd);
    }

    // Plain button normally; filled accent while there is an unsaved work-reset time.
    void DrawOkButton(const DRAWITEMSTRUCT* item, const DialogState* state) {
        const bool pressed = (item->itemState & ODS_SELECTED) != 0;
        COLORREF fill = state->resetDirty ? (pressed ? RGB(29, 78, 216) : RGB(37, 99, 235))
            : (pressed ? RGB(229, 231, 235) : RGB(255, 255, 255));
        COLORREF border = state->resetDirty ? RGB(29, 78, 216) : RGB(160, 160, 160);
        HDC dc = item->hDC;
        FillRect(dc, &item->rcItem, state->backgroundBrush);
        HBRUSH brush = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, border);
        HGDIOBJ oldBrush = SelectObject(dc, brush);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        RoundRect(dc, item->rcItem.left, item->rcItem.top, item->rcItem.right, item->rcItem.bottom, 6, 6);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);

        wchar_t text[32]{};
        GetWindowTextW(item->hwndItem, text, 32);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, state->resetDirty ? RGB(255, 255, 255) : SETTINGS_DIALOG_TEXT);
        HGDIOBJ oldFont = SelectObject(dc, state->resetDirty ? g_hFontTitle : g_hFontNormal);
        RECT textRect = item->rcItem;
        DrawTextW(dc, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, oldFont);
        if (item->itemState & ODS_FOCUS) {
            RECT focusRect = item->rcItem;
            InflateRect(&focusRect, -4, -4);
            DrawFocusRect(dc, &focusRect);
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
        case WM_CREATE:
            BuildControls(hWnd, state);
            SetFocus(state->themeCombo);
            return 0;
        case WM_ERASEBKGND: {
            RECT client{};
            GetClientRect(hWnd, &client);
            FillRect((HDC)wParam, &client, state->backgroundBrush);
            return 1;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC dc = (HDC)wParam;
            // Labels and checkboxes sit directly on the dialog surface, so they get its solid color.
            SetTextColor(dc, SETTINGS_DIALOG_TEXT);
            SetBkColor(dc, SETTINGS_DIALOG_BG);
            SetBkMode(dc, TRANSPARENT);
            return (LRESULT)state->backgroundBrush;
        }
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLOREDIT: {
            // Editable fields stay white so they do not look read-only.
            HDC dc = (HDC)wParam;
            SetTextColor(dc, SETTINGS_DIALOG_TEXT);
            SetBkColor(dc, RGB(255, 255, 255));
            SetBkMode(dc, OPAQUE);
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        }
        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            if (id == IDC_SETTINGS_THEME && HIWORD(wParam) == CBN_SELCHANGE) {
                int selection = static_cast<int>(SendMessageW(state->themeCombo, CB_GETCURSEL, 0, 0));
                if (selection >= 0 && selection <= static_cast<int>(ThemeMode::Pink)) {
                    ApplySelectedTheme(hWnd, state, static_cast<ThemeMode>(selection));
                }
                return 0;
            }
            if (id == IDCANCEL) {
                DestroyWindow(hWnd);
                return 0;
            }
            if (id == IDC_SETTINGS_OK || id == IDOK) {
                SaveAndClose(hWnd, state);
                return 0;
            }
            if (state->building) return 0;
            const int code = HIWORD(wParam);
            if (id == IDC_SETTINGS_EXCLUDED_KEYWORDS && code == EN_CHANGE) {
                state->excludedKeywords = ReadText(state->excludedKeywordsEdit);
                g_store.excluded_window_keywords = ExcludedApps::Normalize(state->excludedKeywords);
                g_store.SaveLocal();
                return 0;
            }
            if ((id == IDC_SETTINGS_SOUNDS || id == IDC_SETTINGS_TIMER_AUTO_START) && code == BN_CLICKED) {
                SnapshotControls(state);
                g_store.sounds_enabled = state->soundsEnabled;
                g_store.timer_start_with_app = state->timerAutoStart;
                g_store.SaveLocal();
                return 0;
            }
            if (SettingsSyncRows::OwnsControl(id) && (code == BN_CLICKED || code == EN_CHANGE)) {
                SnapshotControls(state);
                if (code == BN_CLICKED && SettingsSyncRows::HandleCommand(hWnd, id, state->syncRows)) {
                    ApplySyncRows(state, true);
                    // Rebuild after this click finishes; the clicked button is among the destroyed controls.
                    PostMessageW(hWnd, WM_APP_REBUILD, 0, 0);
                } else {
                    ApplySyncRows(state, false); // Name typed or default flag toggled.
                }
                return 0;
            }
            break;
        }
        case WM_NOTIFY: {
            auto* header = reinterpret_cast<NMHDR*>(lParam);
            if (header && header->hwndFrom == state->resetTime && header->code == DTN_DATETIMECHANGE) {
                UpdateResetDirty(state);
                return 0;
            }
            break;
        }
        case WM_DRAWITEM: {
            auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (item && item->CtlID == IDC_SETTINGS_OK) {
                DrawOkButton(item, state);
                return TRUE;
            }
            break;
        }
        case WM_APP_REBUILD:
            RebuildControls(hWnd, state);
            return 0;
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
            if (state->tooltip && IsWindow(state->tooltip)) {
                DestroyWindow(state->tooltip);
                state->tooltip = nullptr;
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
    state.syncRows = SettingsSyncRows::FromSources(g_store.syncSources);
    state.resetHour = g_store.work_reset_hour;
    state.resetMinute = g_store.work_reset_minute;
    state.soundsEnabled = g_store.sounds_enabled;
    state.timerAutoStart = g_store.timer_start_with_app;
    state.excludedKeywords = g_store.excluded_window_keywords;
    state.owner = owner;
    state.backgroundBrush = CreateSolidBrush(SETTINGS_DIALOG_BG);

    g_inContextMenu = true;
    EnableWindow(owner, FALSE);
    HWND dialog = CreateWindowExW(DIALOG_EX_STYLE, DIALOG_CLASS, L"Beállítások", DIALOG_STYLE,
        CW_USEDEFAULT, CW_USEDEFAULT, SETTINGS_DIALOG_WIDTH, 400, owner, nullptr,
        GetModuleHandleW(nullptr), &state);
    if (!dialog) {
        EnableWindow(owner, TRUE);
        g_inContextMenu = false;
        DeleteObject(state.backgroundBrush);
        return;
    }

    ApplyDarkModeTitleBar(dialog, false);
    g_settingsOpen = true;
    if (g_hSettingsBtn) InvalidateRect(g_hSettingsBtn, nullptr, TRUE);
    RECT ownerRect{};
    RECT dialogRect{};
    GetWindowRect(owner, &ownerRect);
    GetWindowRect(dialog, &dialogRect);
    const int dialogWidth = dialogRect.right - dialogRect.left;
    const int dialogHeight = dialogRect.bottom - dialogRect.top;
    SetWindowPos(dialog, HWND_TOPMOST,
        ownerRect.left + ((ownerRect.right - ownerRect.left) - dialogWidth) / 2,
        ownerRect.top + ((ownerRect.bottom - ownerRect.top) - dialogHeight) / 2,
        0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
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

    g_settingsOpen = false;
    if (g_hSettingsBtn) InvalidateRect(g_hSettingsBtn, nullptr, TRUE);
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    g_inContextMenu = false;
    DeleteObject(state.backgroundBrush);
}
