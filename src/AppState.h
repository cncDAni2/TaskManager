#pragma once

#include "AppTypes.h"
#include "Task.h"
#include <shellapi.h>

// Shared Window Handles
extern HWND g_hWnd;
extern HWND g_hEdit;
extern HWND g_hAddBtn;
extern HWND g_hToggleViewBtn;
extern HWND g_hTimeHistoryBtn;
extern HWND g_hFocusModeBtn;
extern HWND g_hManualWorkBtn;
extern HWND g_hWorkMeasureBtn;
extern HWND g_hCloseBtn;
extern HWND g_hPinBtn;
extern HWND g_hSettingsBtn;
extern HWND g_hTimerBtn;
extern HWND g_hSyncToggleBtn;
extern HWND g_hInlineEdit;
extern HBRUSH g_hEditBrush;
extern NOTIFYICONDATAW g_nid;
extern HICON g_hIcon;

// Fonts
extern HFONT g_hFontTitle;
extern HFONT g_hFontNormal;
extern HFONT g_hFontNormalStrike;
extern HFONT g_hFontSmall;
extern HFONT g_hFontHeader;
extern HFONT g_hFontMiniTimer;
extern HFONT g_hFontTimerLabel;

// Data Store and View Configuration
extern TaskStore g_store;
extern ViewMode g_viewMode;
extern ThemeMode g_themeMode;
extern bool g_darkMode;
extern bool g_syncToggle;

// Window Mode and Geometry
extern bool g_pinMode;
extern bool g_isMiniMode;
extern bool g_inContextMenu;
extern int g_fullWinX;
extern int g_fullWinY;
extern int g_fullWinW;
extern int g_fullWinH;
extern int g_miniWinX;
extern int g_miniWinY;
extern bool g_miniPositionValid;
extern int g_miniTimerFlashStep;
extern bool g_miniTimerFlashWorkPhase;

// Session Tasks Cache
extern std::vector<int> g_sessionActiveTaskIds;
extern bool g_sessionActiveIdsInitialized;

// Render Items & Scroll State
extern std::vector<DisplayItem> g_displayItems;
extern int g_scrollY;
extern int g_totalContentHeight;
extern bool g_scrollbarHovered;
extern bool g_scrollbarDragging;
extern int g_scrollDragStartY;
extern int g_scrollDragStartScrollY;

extern bool g_taskDragging;
extern int g_dragSourceIndex;
extern int g_dragCurrentY;
extern int g_dragInsertIndex;
extern int g_dragStartMouseY;
extern int g_dragPotentialSourceIndex;
extern int g_hoverItemIndex;
extern int g_hoverButtonType;

// Navigation & Editing State
extern int g_selectedIndex;
extern int g_editingTaskId;
extern int g_lastCommittedTaskId;
extern ULONGLONG g_lastCommitTick;

// Work Tracking State
extern bool g_isWorkActive;
extern bool g_workMeasurementStopped;
extern bool g_focusMode;
extern bool g_isSessionLocked;
extern bool g_isIdlePaused;
extern bool g_isExcludedApp;
extern std::wstring g_excludedAppName;
extern int g_secondsSinceLastSave;

// System Settings Helpers
bool IsAutoRunEnabled();
void SetAutoRun(bool enable);
