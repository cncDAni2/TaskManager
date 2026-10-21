#include "Hotkeys.h"
#include "AppState.h"

namespace {
bool g_registered[Hotkeys::ACTION_COUNT] = {};

std::wstring KeyName(UINT vk) {
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) return std::wstring(1, static_cast<wchar_t>(vk));
    const UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC_EX);
    LONG keyParam = static_cast<LONG>((scanCode & 0xFF) << 16);
    if (scanCode & 0xFF00) keyParam |= 1 << 24; // Extended key (arrows, Insert, Page Up...).
    wchar_t name[64]{};
    if (scanCode && GetKeyNameTextW(keyParam, name, 64) > 0) return name;
    return L"#" + std::to_wstring(vk);
}
}

namespace Hotkeys {
bool IsValidCombo(long long combo) {
    if (combo == 0) return true;
    if (combo < 0 || combo > 0xFFFFF) return false;
    const unsigned value = static_cast<unsigned>(combo);
    return Modifiers(value) != 0 && (value & 0xFF00) == 0 && Key(value) > 0 && Key(value) < 0xFF;
}

const wchar_t* ActionName(int action) {
    switch (action) {
    case ActionWindow: return L"Főablak mutatása / elrejtése";
    case ActionPin: return L"PIN mód be- / kikapcsolása";
    case ActionHold: return L"Időzítők megállítása / folytatása";
    case ActionMute: return L"Hangok némítása / engedélyezése";
    }
    return L"";
}

std::wstring FormatModifiers(UINT modifiers) {
    std::wstring text;
    if (modifiers & MOD_CONTROL) text += L"Ctrl+";
    if (modifiers & MOD_ALT) text += L"Alt+";
    if (modifiers & MOD_SHIFT) text += L"Shift+";
    return text;
}

std::wstring Format(unsigned combo) {
    if (combo == 0) return L"";
    return FormatModifiers(Modifiers(combo)) + KeyName(Key(combo));
}

std::wstring Suffix(int action) {
    const std::wstring text = Format(g_store.hotkeys[action]);
    return text.empty() ? text : L" (" + text + L")";
}

bool TypesCharacter(UINT modifiers, UINT vk) {
    const bool shiftOnly = modifiers == MOD_SHIFT;
    const bool altGr = (modifiers & (MOD_CONTROL | MOD_ALT)) == (MOD_CONTROL | MOD_ALT);
    if (!shiftOnly && !altGr) return false;
    BYTE keys[256]{};
    if (modifiers & MOD_SHIFT) keys[VK_SHIFT] = keys[VK_LSHIFT] = 0x80;
    if (modifiers & MOD_CONTROL) keys[VK_CONTROL] = keys[VK_LCONTROL] = 0x80;
    if (modifiers & MOD_ALT) keys[VK_MENU] = keys[VK_RMENU] = 0x80;
    HKL layout = GetKeyboardLayout(0);
    wchar_t buffer[8]{};
    // Flag 4: do not change the keyboard (dead-key) state.
    const int count = ToUnicodeEx(vk, MapVirtualKeyExW(vk, MAPVK_VK_TO_VSC, layout), keys, buffer, 8, 4, layout);
    return count < 0 || (count > 0 && buffer[0] >= 0x20);
}

void RegisterAll(HWND hWnd) {
    UnregisterAll(hWnd);
    for (int i = 0; i < ACTION_COUNT; ++i) {
        const unsigned combo = g_store.hotkeys[i];
        if (combo == 0) continue;
        g_registered[i] = RegisterHotKey(hWnd, ID_HOTKEY_TOGGLE + i, Modifiers(combo) | MOD_NOREPEAT, Key(combo)) != FALSE;
    }
}

void UnregisterAll(HWND hWnd) {
    for (int i = 0; i < ACTION_COUNT; ++i) {
        if (g_registered[i]) UnregisterHotKey(hWnd, ID_HOTKEY_TOGGLE + i);
        g_registered[i] = false;
    }
}

bool IsBlocked(int action) {
    return g_store.hotkeys[action] != 0 && !g_registered[action];
}

std::wstring TrayTip() {
    std::wstring tip = L"Feladatkezelő" + Suffix(ActionWindow);
    if (!g_store.sounds_enabled) tip += L"\nHangok némítva";
    return tip;
}

void UpdateTrayTip() {
    if (!g_nid.hWnd) return;
    wcsncpy_s(g_nid.szTip, TrayTip().c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}
}
