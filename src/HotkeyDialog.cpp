#include "HotkeyDialog.h"
#include "AppState.h"
#include "Drawing.h"
#include "Hotkeys.h"
#include "TimerHold.h"
#include <commctrl.h>
#include <string>

namespace {
constexpr int IDC_HOTKEY_FIRST_FIELD = 5201;
constexpr int IDC_HOTKEY_FIRST_CLEAR = 5211;
constexpr int TEST_HOTKEY_ID = 0xBFFF;
constexpr int DIALOG_WIDTH = 510;
constexpr DWORD DIALOG_STYLE = WS_POPUP | WS_CAPTION | WS_SYSMENU;
constexpr DWORD DIALOG_EX_STYLE = WS_EX_DLGMODALFRAME | WS_EX_TOPMOST | WS_EX_CONTROLPARENT;
constexpr COLORREF DIALOG_BG = RGB(245, 245, 245);
constexpr COLORREF TEXT_COLOR = RGB(0, 0, 0);
constexpr COLORREF INFO_COLOR = RGB(75, 85, 99);
constexpr COLORREF ERROR_COLOR = RGB(185, 28, 28);
constexpr COLORREF LISTENING_BG = RGB(219, 234, 254);
constexpr wchar_t DIALOG_CLASS[] = L"TaskManager_Hotkey_Dialog_Class";
constexpr wchar_t NOT_SET[] = L"Nincs beállítva";
constexpr wchar_t CAPTURE_HINT[] =
    L"Nyomd le az új kombinációt: Ctrl, Alt, Shift vagy AltGr + billentyű (pl. Ctrl+Shift+F5).";

struct DialogState {
    HWND fields[Hotkeys::ACTION_COUNT] = {};
    HWND status = nullptr;
    HBRUSH backgroundBrush = nullptr;
    HBRUSH listeningBrush = nullptr;
    bool statusIsError = false;
    bool blockedAtOpen[Hotkeys::ACTION_COUNT] = {};
};

DialogState* StateOf(HWND dialog) {
    return reinterpret_cast<DialogState*>(GetWindowLongPtrW(dialog, GWLP_USERDATA));
}

HWND CreateChild(HWND dialog, const wchar_t* className, const wchar_t* text, DWORD style,
    int x, int y, int width, int height, int id, HFONT font, DWORD exStyle = 0) {
    HWND control = CreateWindowExW(exStyle, className, text, WS_CHILD | WS_VISIBLE | style,
        x, y, width, height, dialog, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
    if (control && font) SendMessageW(control, WM_SETFONT, (WPARAM)font, TRUE);
    return control;
}

void SetStatus(DialogState* state, const std::wstring& text, bool error) {
    state->statusIsError = error;
    SetWindowTextW(state->status, text.c_str());
    InvalidateRect(state->status, nullptr, TRUE);
}

void ShowCombo(DialogState* state, int action) {
    const std::wstring text = Hotkeys::Format(g_store.hotkeys[action]);
    SetWindowTextW(state->fields[action], text.empty() ? NOT_SET : text.c_str());
}

UINT CurrentModifiers() {
    UINT modifiers = 0;
    if (GetKeyState(VK_CONTROL) < 0) modifiers |= MOD_CONTROL;
    if (GetKeyState(VK_MENU) < 0) modifiers |= MOD_ALT;
    if (GetKeyState(VK_SHIFT) < 0) modifiers |= MOD_SHIFT;
    return modifiers;
}

bool IsModifierKey(UINT vk) {
    switch (vk) {
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
    case VK_MENU: case VK_LMENU: case VK_RMENU:
    case VK_LWIN: case VK_RWIN: case VK_CAPITAL: case VK_NUMLOCK: case VK_SCROLL:
        return true;
    }
    return false;
}

void Capture(HWND field, int action, UINT vk) {
    HWND dialog = GetParent(field);
    DialogState* state = StateOf(dialog);
    const UINT modifiers = CurrentModifiers();
    if (IsModifierKey(vk)) {
        SetWindowTextW(field, (Hotkeys::FormatModifiers(modifiers) + L"…").c_str());
        return;
    }
    ShowCombo(state, action);
    if (modifiers == 0) {
        SetStatus(state, L"Csak Ctrl, Alt, Shift vagy AltGr billentyűvel együtt adható meg (pl. Ctrl+F5).", true);
        return;
    }
    const unsigned combo = Hotkeys::Make(modifiers, vk);
    const std::wstring name = Hotkeys::Format(combo);
    if (Hotkeys::TypesCharacter(modifiers, vk)) {
        SetStatus(state, L"Ez a kombináció (" + name + L") karaktert ír be, így gépelés közben zavarna. "
            L"Válassz mást, például funkcióbillentyűt.", true);
        return;
    }
    for (int i = 0; i < Hotkeys::ACTION_COUNT; ++i) {
        if (i == action || g_store.hotkeys[i] != combo) continue;
        SetStatus(state, L"Ezt a kombinációt (" + name + L") már a „" + Hotkeys::ActionName(i) + L"” használja.", true);
        return;
    }
    if (g_store.hotkeys[action] != combo) {
        // Own hotkeys are suspended while the dialog is open, so a failure means another program owns it.
        if (!RegisterHotKey(dialog, TEST_HOTKEY_ID, modifiers | MOD_NOREPEAT, vk)) {
            SetStatus(state, L"Ezt a kombinációt (" + name + L") már egy másik program használja.", true);
            return;
        }
        UnregisterHotKey(dialog, TEST_HOTKEY_ID);
        g_store.hotkeys[action] = combo;
        g_store.SaveLocal();
        ShowCombo(state, action);
    }
    state->blockedAtOpen[action] = false;
    SetStatus(state, L"Mentve: " + std::wstring(Hotkeys::ActionName(action)) + L" – " + name, false);
}

LRESULT CALLBACK FieldProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData) {
    const int action = static_cast<int>(refData);
    switch (message) {
    case WM_GETDLGCODE: {
        // Tab, and plain Enter / Esc, stay with the dialog for navigation, OK and closing.
        const MSG* msg = reinterpret_cast<const MSG*>(lParam);
        if (msg && (msg->wParam == VK_TAB ||
            ((msg->wParam == VK_RETURN || msg->wParam == VK_ESCAPE) && CurrentModifiers() == 0))) {
            return 0;
        }
        return DLGC_WANTALLKEYS;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        Capture(hWnd, action, static_cast<UINT>(wParam));
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (IsModifierKey(static_cast<UINT>(wParam))) {
            const UINT modifiers = CurrentModifiers();
            if (modifiers) SetWindowTextW(hWnd, (Hotkeys::FormatModifiers(modifiers) + L"…").c_str());
            else ShowCombo(StateOf(GetParent(hWnd)), action);
        }
        return 0;
    case WM_CHAR:
    case WM_SYSCHAR:
    case WM_DEADCHAR:
    case WM_SYSDEADCHAR:
    case WM_CONTEXTMENU:
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        SetFocus(hWnd);
        return 0;
    case WM_SETFOCUS:
    case WM_KILLFOCUS: {
        const LRESULT result = DefSubclassProc(hWnd, message, wParam, lParam);
        DialogState* state = StateOf(GetParent(hWnd));
        if (message == WM_SETFOCUS) {
            HideCaret(hWnd);
            SetStatus(state, CAPTURE_HINT, false);
        } else {
            ShowCombo(state, action);
        }
        InvalidateRect(hWnd, nullptr, TRUE);
        return result;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, FieldProc, 0);
        break;
    }
    return DefSubclassProc(hWnd, message, wParam, lParam);
}

void BuildControls(HWND hWnd, DialogState* state) {
    CreateChild(hWnd, L"STATIC",
        L"Kattints egy mezőbe, és nyomd le a kívánt kombinációt. A gyorsbillentyűk bárhonnan működnek, "
        L"akkor is, ha a program a háttérben fut.",
        SS_LEFT, 18, 14, DIALOG_WIDTH - 36, 40, 0, g_hFontSmall);
    int top = 62;
    for (int i = 0; i < Hotkeys::ACTION_COUNT; ++i) {
        CreateChild(hWnd, L"STATIC", Hotkeys::ActionName(i), SS_LEFT, 18, top + 5, 240, 24, 0, g_hFontNormal);
        state->fields[i] = CreateChild(hWnd, L"EDIT", L"", WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL,
            266, top, 150, 28, IDC_HOTKEY_FIRST_FIELD + i, g_hFontNormal, WS_EX_CLIENTEDGE);
        SetWindowSubclass(state->fields[i], FieldProc, 0, static_cast<DWORD_PTR>(i));
        ShowCombo(state, i);
        CreateChild(hWnd, L"BUTTON", L"Törlés", WS_TABSTOP | BS_PUSHBUTTON,
            424, top, 68, 28, IDC_HOTKEY_FIRST_CLEAR + i, g_hFontNormal);
        top += 38;
    }
    state->status = CreateChild(hWnd, L"STATIC", L"", SS_LEFT, 18, top + 6, DIALOG_WIDTH - 36, 40, 0, g_hFontSmall);
    top += 52;
    CreateChild(hWnd, L"BUTTON", L"OK", WS_TABSTOP | BS_DEFPUSHBUTTON,
        DIALOG_WIDTH - 18 - 90, top, 90, 30, IDOK, g_hFontNormal);

    std::wstring blocked;
    for (int i = 0; i < Hotkeys::ACTION_COUNT; ++i) {
        if (!state->blockedAtOpen[i]) continue;
        blocked += (blocked.empty() ? L"" : L", ") + Hotkeys::Format(g_store.hotkeys[i]);
    }
    if (!blocked.empty()) {
        SetStatus(state, L"Egy másik program már használja, ezért nem működik: " + blocked +
            L". Válassz másik kombinációt.", true);
    }

    RECT desired = { 0, 0, DIALOG_WIDTH, top + 30 + 14 };
    AdjustWindowRectEx(&desired, DIALOG_STYLE, FALSE, DIALOG_EX_STYLE);
    SetWindowPos(hWnd, nullptr, 0, 0, desired.right - desired.left, desired.bottom - desired.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK HotkeyDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    DialogState* state = StateOf(hWnd);
    if (message == WM_NCCREATE) {
        state = static_cast<DialogState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }

    switch (message) {
    case WM_CREATE:
        BuildControls(hWnd, state);
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
        HWND control = (HWND)lParam;
        for (HWND field : state->fields) {
            if (control != field) continue;
            // Read-only edits ask for static colors; keep them white, light blue while listening.
            const bool listening = GetFocus() == field;
            SetTextColor(dc, TEXT_COLOR);
            SetBkColor(dc, listening ? LISTENING_BG : RGB(255, 255, 255));
            return (LRESULT)(listening ? state->listeningBrush : GetStockObject(WHITE_BRUSH));
        }
        SetTextColor(dc, control == state->status ? (state->statusIsError ? ERROR_COLOR : INFO_COLOR) : TEXT_COLOR);
        SetBkColor(dc, DIALOG_BG);
        SetBkMode(dc, TRANSPARENT);
        return (LRESULT)state->backgroundBrush;
    }
    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        if (id == IDOK || id == IDCANCEL) {
            DestroyWindow(hWnd);
            return 0;
        }
        if (id >= IDC_HOTKEY_FIRST_CLEAR && id < IDC_HOTKEY_FIRST_CLEAR + Hotkeys::ACTION_COUNT &&
            HIWORD(wParam) == BN_CLICKED) {
            const int action = id - IDC_HOTKEY_FIRST_CLEAR;
            g_store.hotkeys[action] = 0;
            g_store.SaveLocal();
            state->blockedAtOpen[action] = false;
            ShowCombo(state, action);
            SetStatus(state, L"Törölve: " + std::wstring(Hotkeys::ActionName(action)), false);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}
}

void ShowHotkeyDialog(HWND owner) {
    WNDCLASSW dialogClass{};
    dialogClass.lpfnWndProc = HotkeyDialogProc;
    dialogClass.hInstance = GetModuleHandleW(nullptr);
    dialogClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    dialogClass.lpszClassName = DIALOG_CLASS;
    if (!RegisterClassW(&dialogClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;

    DialogState state;
    for (int i = 0; i < Hotkeys::ACTION_COUNT; ++i) state.blockedAtOpen[i] = Hotkeys::IsBlocked(i);
    state.backgroundBrush = CreateSolidBrush(DIALOG_BG);
    state.listeningBrush = CreateSolidBrush(LISTENING_BG);

    // Suspended so pressing an existing combo reaches the field instead of firing.
    Hotkeys::UnregisterAll(g_hWnd);
    EnableWindow(owner, FALSE);
    HWND dialog = CreateWindowExW(DIALOG_EX_STYLE, DIALOG_CLASS, L"Gyorsbillentyűk", DIALOG_STYLE,
        CW_USEDEFAULT, CW_USEDEFAULT, DIALOG_WIDTH, 300, owner, nullptr, GetModuleHandleW(nullptr), &state);
    if (dialog) {
        ApplyDarkModeTitleBar(dialog, false);
        RECT ownerRect{};
        RECT dialogRect{};
        GetWindowRect(owner, &ownerRect);
        GetWindowRect(dialog, &dialogRect);
        const int width = dialogRect.right - dialogRect.left;
        const int height = dialogRect.bottom - dialogRect.top;
        SetWindowPos(dialog, HWND_TOPMOST,
            ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2,
            ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2,
            0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
        SetForegroundWindow(dialog);
        SetFocus(GetDlgItem(dialog, IDOK));

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
    }

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    Hotkeys::RegisterAll(g_hWnd);
    Hotkeys::UpdateTrayTip();
    TimerHold::RefreshTooltip();
    if (g_hWnd) InvalidateRect(g_hWnd, nullptr, FALSE);
    DeleteObject(state.backgroundBrush);
    DeleteObject(state.listeningBrush);
}
