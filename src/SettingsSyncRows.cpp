#include "SettingsSyncRows.h"
#include "AppState.h"
#include <commctrl.h>
#include <commdlg.h>

namespace {
constexpr int IDC_ADD_ROW = 5190;
constexpr int IDC_ROW_BASE = 5200;
constexpr int ROW_STRIDE = 10;
constexpr int COL_DEFAULT = 0;
constexpr int COL_NAME = 1;
constexpr int COL_BROWSE = 3;
constexpr int COL_REMOVE = 4;
constexpr int ROW_HEIGHT = 36;

constexpr wchar_t SYNC_HELP[] =
    L"Olyan fájlt válassz, amelyet valamelyik felhőszolgáltatás automatikusan szinkronizál.\n\n"
    L"OneDrive esetén először ossz meg egy mappát a másik személlyel. A másik fél a "
    L"\u201eParancsikon hozzáadása Saját fájlokhoz\u201d lehetőséggel szinkronizálja a mappát a gépére. "
    L"Ezután a C:\\Users\\<felhasználónév>\\OneDrive - Siemens AG\\ mappában keresd meg a megosztó "
    L"nevét viselő mappát, és tallózd be a megosztott fájlt.";

int RowControlId(size_t row, int column) {
    return IDC_ROW_BASE + static_cast<int>(row) * ROW_STRIDE + column;
}

HWND CreateChild(HWND dialog, const wchar_t* className, const wchar_t* text, DWORD style,
    int x, int y, int width, int height, int id, HFONT font, DWORD exStyle = 0) {
    HWND control = CreateWindowExW(exStyle, className, text, WS_CHILD | WS_VISIBLE | style,
        x, y, width, height, dialog, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
    if (control && font) SendMessageW(control, WM_SETFONT, (WPARAM)font, TRUE);
    return control;
}

void AddTip(HWND tooltip, HWND dialog, HWND control, const std::wstring& text) {
    if (!tooltip || !control) return;
    TOOLINFOW tool{};
    tool.cbSize = sizeof(tool);
    tool.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    tool.hwnd = dialog;
    tool.uId = reinterpret_cast<UINT_PTR>(control);
    tool.lpszText = const_cast<LPWSTR>(text.c_str()); // The tooltip keeps its own copy.
    SendMessageW(tooltip, TTM_ADDTOOLW, 0, (LPARAM)&tool);
}

bool BrowseForFile(HWND owner, std::wstring& path) {
    std::vector<wchar_t> buffer(32768, L'\0');
    if (!path.empty()) wcscpy_s(buffer.data(), buffer.size(), path.c_str());

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = static_cast<DWORD>(buffer.size());
    ofn.lpstrFilter = L"JSON fájlok (*.json)\0*.json\0Minden fájl (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"json";
    ofn.lpstrTitle = L"Szinkronizációs fájl kiválasztása vagy új fájl nevének megadása";
    // A save dialog lets the user pick an existing file or type a new name; nothing is overwritten.
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_ENABLESIZING | OFN_NOCHANGEDIR | OFN_NOREADONLYRETURN;

    std::wstring initialDirectory;
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        initialDirectory = path.substr(0, lastSlash);
        ofn.lpstrInitialDir = initialDirectory.c_str();
    }
    if (!GetSaveFileNameW(&ofn)) return false;
    path = buffer.data();
    return true;
}
}

std::vector<SettingsSyncRows::Row> SettingsSyncRows::FromSources(const std::vector<SyncSource>& sources) {
    std::vector<Row> rows;
    for (const SyncSource& source : sources) {
        rows.push_back({ source.id, source.name, source.path, source.isDefault });
    }
    return rows;
}

std::vector<SyncSource> SettingsSyncRows::ToSources(const std::vector<Row>& rows) {
    std::vector<SyncSource> sources;
    for (const Row& row : rows) {
        if (row.path.empty()) continue;
        bool duplicate = false;
        for (const SyncSource& existing : sources) {
            if (_wcsicmp(existing.path.c_str(), row.path.c_str()) == 0) duplicate = true;
        }
        if (duplicate) continue;
        SyncSource source;
        source.id = row.sourceId;
        source.path = row.path;
        source.name = row.name.empty() ? SyncFiles::DefaultName(row.path) : row.name;
        source.isDefault = row.isDefault;
        sources.push_back(source);
    }
    return sources;
}

int SettingsSyncRows::Create(HWND dialog, HWND tooltip, const std::vector<Row>& rows, Controls& controls,
    int top, int left, int right) {
    controls = Controls{};
    const int defaultX = left + 12;
    const int nameX = left + 36;
    constexpr int nameWidth = 130;
    const int pathX = nameX + nameWidth + 8;
    constexpr int removeWidth = 28;
    const int removeX = right - 10 - removeWidth;
    constexpr int browseWidth = 76;
    const int browseX = removeX - 6 - browseWidth;
    const int pathWidth = browseX - pathX - 8;
    const int headerY = top + 33;
    const int firstRowY = top + 55;

    HWND group = CreateChild(dialog, L"BUTTON", L"Szinkronizációs file-ok ℹ️", BS_GROUPBOX,
        left, top, right - left, 10, 0, g_hFontNormal);
    AddTip(tooltip, dialog, group, SYNC_HELP);

    HWND defaultHeader = CreateChild(dialog, L"STATIC", L"A.", SS_CENTER | SS_NOTIFY,
        defaultX - 4, headerY, 26, 18, 0, g_hFontSmall);
    AddTip(tooltip, dialog, defaultHeader,
        L"Alapértelmezett: új feladat felvételekor ezek a fájlok vannak előre kijelölve.");
    CreateChild(dialog, L"STATIC", L"Név", SS_LEFT, nameX, headerY, nameWidth, 18, 0, g_hFontSmall);
    CreateChild(dialog, L"STATIC", L"Útvonal", SS_LEFT, pathX, headerY, pathWidth, 18, 0, g_hFontSmall);

    int y = firstRowY;
    for (size_t i = 0; i < rows.size(); ++i) {
        const Row& row = rows[i];
        HWND checkbox = CreateChild(dialog, L"BUTTON", L"", BS_AUTOCHECKBOX | WS_TABSTOP,
            defaultX, y + 5, 18, 18, RowControlId(i, COL_DEFAULT), nullptr);
        SendMessageW(checkbox, BM_SETCHECK, row.isDefault ? BST_CHECKED : BST_UNCHECKED, 0);
        AddTip(tooltip, dialog, checkbox, L"Új feladat alapértelmezetten ebbe a fájlba is bekerül.");
        controls.defaults.push_back(checkbox);

        HWND nameEdit = CreateChild(dialog, L"EDIT", row.name.c_str(), ES_AUTOHSCROLL | WS_TABSTOP,
            nameX, y, nameWidth, 28, RowControlId(i, COL_NAME), g_hFontNormal, WS_EX_CLIENTEDGE);
        AddTip(tooltip, dialog, nameEdit, L"Rövid név: ez látszik a feladatok mellett és a fájlválasztóban.");
        controls.names.push_back(nameEdit);

        HWND pathText = CreateChild(dialog, L"STATIC", row.path.c_str(),
            SS_LEFT | SS_NOPREFIX | SS_PATHELLIPSIS | SS_CENTERIMAGE | SS_NOTIFY,
            pathX, y, pathWidth, 28, 0, g_hFontNormal);
        AddTip(tooltip, dialog, pathText, row.path);

        HWND browse = CreateChild(dialog, L"BUTTON", L"Tallózás", BS_PUSHBUTTON | WS_TABSTOP,
            browseX, y, browseWidth, 28, RowControlId(i, COL_BROWSE), g_hFontSmall);
        AddTip(tooltip, dialog, browse, L"Másik meglévő fájl választása, vagy új fájlnév megadása (azonnal létrejön).");
        HWND remove = CreateChild(dialog, L"BUTTON", L"✕", BS_PUSHBUTTON | WS_TABSTOP,
            removeX, y, removeWidth, 28, RowControlId(i, COL_REMOVE), g_hFontSmall);
        AddTip(tooltip, dialog, remove, L"Eltávolítás a listából. A fájl nem törlődik, csak nem szinkronizálunk vele.");
        y += ROW_HEIGHT;
    }
    if (rows.empty()) {
        CreateChild(dialog, L"STATIC", L"Nincs szinkronizációs fájl: minden feladat csak helyben tárolódik.",
            SS_LEFT | SS_CENTERIMAGE, nameX, y, right - nameX - 12, 28, 0, g_hFontSmall);
        y += ROW_HEIGHT;
    }

    HWND addButton = CreateChild(dialog, L"BUTTON", L"+ Új fájl hozzáadása vagy létrehozása",
        BS_PUSHBUTTON | WS_TABSTOP, nameX, y + 2, 250, 28, IDC_ADD_ROW, g_hFontSmall);
    AddTip(tooltip, dialog, addButton,
        L"Válassz egy meglévő fájlt, vagy írj be egy még nem létező fájlnevet: "
        L"az új fájlt a program azonnal létrehozza.");
    EnableWindow(addButton, rows.size() < MAX_ROWS);
    const int bottom = y + 2 + 28 + 10;
    SetWindowPos(group, nullptr, 0, 0, right - left, bottom - top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    return bottom;
}

void SettingsSyncRows::Snapshot(const Controls& controls, std::vector<Row>& rows) {
    for (size_t i = 0; i < rows.size(); ++i) {
        if (i < controls.defaults.size()) {
            rows[i].isDefault = SendMessageW(controls.defaults[i], BM_GETCHECK, 0, 0) == BST_CHECKED;
        }
        if (i < controls.names.size()) {
            const int length = GetWindowTextLengthW(controls.names[i]);
            std::wstring name(static_cast<size_t>(length) + 1, wchar_t());
            GetWindowTextW(controls.names[i], name.data(), length + 1);
            name.resize(static_cast<size_t>(length));
            rows[i].name = name;
        }
    }
}

bool SettingsSyncRows::OwnsControl(int controlId) {
    return controlId == IDC_ADD_ROW ||
        (controlId >= IDC_ROW_BASE && controlId < IDC_ROW_BASE + MAX_ROWS * ROW_STRIDE);
}

bool SettingsSyncRows::HandleCommand(HWND dialog, int controlId, std::vector<Row>& rows) {
    if (controlId == IDC_ADD_ROW) {
        if (rows.size() >= MAX_ROWS) return false;
        Row row;
        if (!rows.empty()) row.path = rows.back().path; // Start browsing next to the last file.
        if (!BrowseForFile(dialog, row.path)) return false;
        row.name = std::to_wstring(rows.size() + 1); // Numbered by default; rename freely.
        rows.push_back(row);
        return true;
    }
    if (controlId < IDC_ROW_BASE) return false;
    const size_t row = static_cast<size_t>((controlId - IDC_ROW_BASE) / ROW_STRIDE);
    const int column = (controlId - IDC_ROW_BASE) % ROW_STRIDE;
    if (row >= rows.size()) return false;

    if (column == COL_BROWSE) {
        std::wstring path = rows[row].path;
        if (!BrowseForFile(dialog, path)) return false;
        rows[row].path = path;
        if (rows[row].name.empty()) rows[row].name = SyncFiles::DefaultName(path);
        return true;
    }
    if (column == COL_REMOVE) {
        rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(row));
        return true;
    }
    return false;
}
