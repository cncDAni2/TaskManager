#include "SyncPicker.h"
#include "AppState.h"
#include <commctrl.h>
#include <algorithm>

namespace {
std::wstring MenuLabel(const std::wstring& name) {
    std::wstring label;
    for (wchar_t c : name) {
        label += c;
        if (c == L'&') label += L'&'; // Show '&' literally instead of as a mnemonic.
    }
    return label;
}
}

void SyncPicker::ResetSelection() {
    g_syncSelection = g_store.DefaultSourceIds();
    if (g_hSyncToggleBtn) InvalidateRect(g_hSyncToggleBtn, nullptr, TRUE);
}

bool SyncPicker::HasSources() {
    return !g_store.syncSources.empty();
}

bool SyncPicker::ShowCounts() {
    return g_store.syncSources.size() > 1;
}

bool SyncPicker::IsSelected(int sourceId) {
    return std::find(g_syncSelection.begin(), g_syncSelection.end(), sourceId) != g_syncSelection.end();
}

int SyncPicker::SelectedCount() {
    int count = 0;
    for (const SyncSource& source : g_store.syncSources) {
        if (IsSelected(source.id)) ++count;
    }
    return count;
}

static void Toggle(int sourceId) {
    auto it = std::find(g_syncSelection.begin(), g_syncSelection.end(), sourceId);
    if (it == g_syncSelection.end()) g_syncSelection.push_back(sourceId);
    else g_syncSelection.erase(it);
}

void SyncPicker::HandleClick(HWND owner, HWND button) {
    const auto& sources = g_store.syncSources;
    if (sources.empty()) return;
    if (sources.size() == 1) {
        Toggle(sources.front().id);
        InvalidateRect(button, nullptr, TRUE);
        return;
    }

    // A menu closes after every pick, so reopen it until the user clicks elsewhere.
    // TPM_NOANIMATION: the system fade-in would make every reopen flicker.
    RECT buttonRect{};
    GetWindowRect(button, &buttonRect);
    g_inContextMenu = true;
    while (true) {
        HMENU menu = CreatePopupMenu();
        for (size_t i = 0; i < sources.size(); ++i) {
            AppendMenuW(menu, MF_STRING | (IsSelected(sources[i].id) ? MF_CHECKED : MF_UNCHECKED),
                i + 1, MenuLabel(sources[i].name).c_str());
        }
        SetForegroundWindow(owner);
        const UINT picked = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_NOANIMATION,
            buttonRect.right, buttonRect.bottom + 2, 0, owner, nullptr);
        DestroyMenu(menu);
        if (picked == 0 || picked > sources.size()) break;
        Toggle(sources[picked - 1].id);
        InvalidateRect(button, nullptr, TRUE);
        UpdateWindow(button);
    }
    g_inContextMenu = false;
}

bool SyncPicker::HandleTooltipNotify(LPARAM lParam, HWND button) {
    auto* header = reinterpret_cast<NMHDR*>(lParam);
    if (!header || header->code != TTN_GETDISPINFOW || header->idFrom != reinterpret_cast<UINT_PTR>(button)) {
        return false;
    }
    static std::wstring text;
    const auto& sources = g_store.syncSources;
    if (sources.size() == 1) {
        text = IsSelected(sources.front().id)
            ? L"Az új feladat szinkronizálva lesz ide: " + sources.front().name + L"\nKattints a kikapcsoláshoz."
            : L"Kattints, hogy az új feladat szinkronizálva legyen ide: " + sources.front().name;
    } else {
        std::wstring selected;
        for (const SyncSource& source : sources) {
            if (IsSelected(source.id)) selected += L"\n• " + source.name;
        }
        text = selected.empty()
            ? L"Az új feladat csak helyben marad.\nKattints a szinkronizációs fájlok kiválasztásához."
            : L"Az új feladat ide kerül:" + selected + L"\nKattints a módosításhoz.";
    }
    reinterpret_cast<NMTTDISPINFOW*>(lParam)->lpszText = text.data();
    return true;
}

void SyncPicker::DrawCloudCount(HDC hdc, const RECT& cloudRect, int count, COLORREF text) {
    if (count <= 0) return;
    static HFONT font = CreateFontW(-10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    // Same center as DrawCloudIcon; the cloud body spans roughly cx-7..cx+8 and cy-6..cy+6.
    const int cx = cloudRect.left + (cloudRect.right - cloudRect.left) / 2;
    const int cy = cloudRect.top + (cloudRect.bottom - cloudRect.top) / 2;
    RECT textRect = { cx - 6, cy - 5, cx + 7, cy + 7 };

    wchar_t digits[4];
    swprintf_s(digits, count > 9 ? L"+" : L"%d", count);
    HGDIOBJ oldFont = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, text);
    DrawTextW(hdc, digits, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
}

std::wstring SyncPicker::SourceNames(const Task& task) {
    std::wstring names;
    for (int sourceId : task.sources) {
        const SyncSource* source = g_store.FindSource(sourceId);
        if (!source) continue;
        if (!names.empty()) names += L"\n";
        names += source->name;
    }
    return names;
}
