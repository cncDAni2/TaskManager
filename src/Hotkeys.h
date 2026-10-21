#pragma once

#include "AppTypes.h"
#include <array>
#include <string>

// Configurable global hotkeys. A combo is (MOD_* flags << 16) | virtual key; 0 means not set.
namespace Hotkeys {
enum Action { ActionWindow, ActionPin, ActionHold, ActionMute, ACTION_COUNT };

constexpr std::array<unsigned, ACTION_COUNT> DEFAULT_COMBOS = {
    (MOD_CONTROL << 16) | VK_F1, (MOD_CONTROL << 16) | VK_F2, (MOD_CONTROL << 16) | VK_F3, 0u };
constexpr const char* JSON_KEYS[ACTION_COUNT] = { "hotkey_window", "hotkey_pin", "hotkey_hold", "hotkey_mute" };

constexpr unsigned Make(UINT modifiers, UINT vk) { return (modifiers << 16) | vk; }
constexpr UINT Modifiers(unsigned combo) { return (combo >> 16) & (MOD_ALT | MOD_CONTROL | MOD_SHIFT); }
constexpr UINT Key(unsigned combo) { return combo & 0xFF; }
bool IsValidCombo(long long combo);

const wchar_t* ActionName(int action);
std::wstring FormatModifiers(UINT modifiers); // "Ctrl+Alt+"
std::wstring Format(unsigned combo);          // "Ctrl+F1"; empty when not set
std::wstring Suffix(int action);              // " (Ctrl+F1)"; empty when not set
// True when a Shift-only or Ctrl+Alt (AltGr) combo types a character in the current layout.
bool TypesCharacter(UINT modifiers, UINT vk);

void RegisterAll(HWND hWnd);
void UnregisterAll(HWND hWnd);
// Set, but another program already owns the combo.
bool IsBlocked(int action);

std::wstring TrayTip();
void UpdateTrayTip();
}
