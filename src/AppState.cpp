#include "AppState.h"

// Shared Window Handles
HWND g_hWnd = nullptr;
HWND g_hEdit = nullptr;
HWND g_hAddBtn = nullptr;
HWND g_hToggleViewBtn = nullptr;
HWND g_hTimeHistoryBtn = nullptr;
HWND g_hManualWorkBtn = nullptr;
HWND g_hCloseBtn = nullptr;
HWND g_hPinBtn = nullptr;
HWND g_hSyncFolderBtn = nullptr;
HWND g_hSyncToggleBtn = nullptr;
HWND g_hFilterRecentBtn = nullptr;
HWND g_hFilterAllBtn = nullptr;
HWND g_hInlineEdit = nullptr;
HBRUSH g_hEditBrush = nullptr;
NOTIFYICONDATAW g_nid{};
HICON g_hIcon = nullptr;

// Fonts
HFONT g_hFontTitle = nullptr;
HFONT g_hFontNormal = nullptr;
HFONT g_hFontNormalStrike = nullptr;
HFONT g_hFontSmall = nullptr;
HFONT g_hFontHeader = nullptr;

// Data Store and View Configuration
TaskStore g_store;
ViewMode g_viewMode = ViewMode::ActiveTasks;
CompletedFilter g_completedFilter = CompletedFilter::SinceYesterday930;
bool g_darkMode = true;
bool g_syncToggle = false;

// Window Mode and Geometry
bool g_pinMode = true;
bool g_isMiniMode = false;
bool g_inContextMenu = false;
int g_fullWinX = 0;
int g_fullWinY = 0;
int g_fullWinW = 585;
int g_fullWinH = 550;
int g_miniWinX = 0;
int g_miniWinY = 0;
bool g_miniPositionValid = false;

// Session Tasks Cache
std::vector<int> g_sessionActiveTaskIds;
bool g_sessionActiveIdsInitialized = false;

// Render Items & Scroll State
std::vector<DisplayItem> g_displayItems;
int g_scrollY = 0;
int g_totalContentHeight = 0;
bool g_scrollbarHovered = false;
bool g_scrollbarDragging = false;
int g_scrollDragStartY = 0;
int g_scrollDragStartScrollY = 0;

bool g_taskDragging = false;
int g_dragSourceIndex = -1;
int g_dragCurrentY = 0;
int g_dragInsertIndex = -1;
int g_dragStartMouseY = 0;
int g_dragPotentialSourceIndex = -1;
int g_hoverItemIndex = -1;
int g_hoverButtonType = 0;

// Navigation & Editing State
int g_selectedIndex = -1;
int g_editingTaskId = -1;
int g_lastCommittedTaskId = -1;
ULONGLONG g_lastCommitTick = 0;

// Work Tracking State
bool g_isWorkActive = false;
bool g_isIdlePaused = false;
bool g_isExcludedApp = false;
std::wstring g_excludedAppName = L"";
int g_secondsSinceLastSave = 0;

bool IsAutoRunEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t path[MAX_PATH];
        DWORD size = sizeof(path);
        DWORD type = 0;
        LSTATUS st = RegQueryValueExW(hKey, L"UltraTaskManager", nullptr, &type, (LPBYTE)path, &size);
        RegCloseKey(hKey);
        return (st == ERROR_SUCCESS);
    }
    return false;
}

void SetAutoRun(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            std::wstring cmd = L"\"" + std::wstring(exePath) + L"\"";
            RegSetValueExW(hKey, L"UltraTaskManager", 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.size() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, L"UltraTaskManager");
        }
        RegCloseKey(hKey);
    }
}
