#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <string>
#include <vector>
#include <ctime>

// UI Control and Command IDs
#define IDC_NEW_TASK_EDIT   2001
#define IDC_ADD_TASK_BTN    2002
#define IDC_TOGGLE_VIEW_BTN 2003
#define IDC_CLOSE_BTN       2004
#define IDC_INLINE_EDIT     2007
#define IDC_PIN_BTN         2008
#define IDC_SYNC_TOGGLE_BTN 2009
#define IDC_SETTINGS_BTN    2010
#define IDC_TIME_HISTORY_BTN 2011
#define IDC_MANUAL_WORK_BTN  2012
#define IDC_FOCUS_MODE_BTN  2013
#define IDC_STOP_WORK_BTN   2014
#define IDC_TIMER_BTN       2015
#define IDC_TIMER_WORK_EDIT 2016
#define IDC_TIMER_REST_EDIT 2017
#define IDC_TIMER_REPEATS_EDIT 2018
#define IDC_TIMER_START_BTN 2019
#define IDC_TIMER_PAUSE_BTN 2020
#define IDC_TIMER_2020_CHECK 2021

// System Tray and Menu IDs
#define IDM_TRAY_OPEN       3001
#define IDM_TRAY_COMPLETED  3002
#define IDM_TRAY_PIN        3003
#define IDM_TRAY_AUTORUN    3004
#define IDM_TRAY_TIMER      3005
#define IDM_TRAY_EXIT       3006

// Hotkey IDs
#define ID_HOTKEY_TOGGLE    1001
#define ID_HOTKEY_UNPIN     1002

// Custom Messages and Timers
#define WM_APP_TRAY         (WM_APP + 100)
#define IDT_WORK_TIMER      4001
#define IDT_MINI_TIMER_FLASH 4002
#define BOTTOM_BAR_HEIGHT   28
#define WORK_MEASURE_FOOTER_HEIGHT 40
#define TIMER_STATUS_HEIGHT 22

enum class ViewMode {
    ActiveTasks,
    CompletedTasks,
    WorkHistory,
    IntervalTimer
};

enum class ThemeMode {
    Dark,
    Light,
    Pink
};

enum class TaskMarker {
    None = 0,
    Heart = 1,
    Crescent = 2,
    Lightning = 3
};

struct Task {
    int id = 0;
    std::wstring text;
    bool completed = false;
    time_t created_at = 0;
    time_t completed_at = 0;
    bool is_sync = false;
    std::wstring author;
    std::wstring assignee;
    TaskMarker marker = TaskMarker::None;
};

struct DisplayItem {
    bool isHeader = false;
    std::wstring headerText;
    Task task;
    RECT rect{};
    RECT checkRect{};
    RECT editRect{};
    RECT deleteRect{};
    RECT assignRect{};
    RECT textRect{};
    RECT markerRect{};
    RECT optionsRect{};
};

struct ScrollbarMetrics {
    bool visible = false;
    RECT rcTrack{};
    RECT rcThumb{};
    int maxScroll = 0;
};

struct ThemeColors {
    COLORREF bgWindow;
    COLORREF bgHeader;
    COLORREF bgSubBar;
    COLORREF bgCard;
    COLORREF bgCardHover;
    COLORREF bgCardSelected;
    COLORREF borderCard;
    COLORREF borderCardHover;
    COLORREF borderCardSelected;
    COLORREF borderSep;
    COLORREF textTitle;
    COLORREF textPrimary;
    COLORREF textCompleted;
    COLORREF textSecondary;
    COLORREF textEmpty;
    COLORREF bgEdit;
    COLORREF textEdit;
    COLORREF checkActiveBorder;
    COLORREF checkActiveHover;
    COLORREF checkDoneBg;
    COLORREF checkDoneHover;
    COLORREF headerSectionBg;
    COLORREF headerSectionText;
};

inline ThemeColors GetDarkTheme() {
    ThemeColors c;
    c.bgWindow = RGB(24, 24, 27);          // Zinc 900
    c.bgHeader = RGB(15, 15, 18);          // Darker Zinc
    c.bgSubBar = RGB(32, 32, 36);          // Zinc 850
    c.bgCard = RGB(39, 39, 42);            // Zinc 800
    c.bgCardHover = RGB(48, 48, 52);       // Zinc 750
    c.bgCardSelected = RGB(45, 55, 72);    // Selected blueish-zinc
    c.borderCard = RGB(63, 63, 70);        // Zinc 700
    c.borderCardHover = RGB(96, 165, 250); // Sky blue
    c.borderCardSelected = RGB(59, 130, 246); // Bright blue focus
    c.borderSep = RGB(45, 45, 50);
    c.textTitle = RGB(244, 244, 245);
    c.textPrimary = RGB(244, 244, 245);
    c.textCompleted = RGB(113, 113, 122);  // Zinc 500
    c.textSecondary = RGB(161, 161, 170);  // Zinc 400
    c.textEmpty = RGB(113, 113, 122);
    c.bgEdit = RGB(39, 39, 42);
    c.textEdit = RGB(244, 244, 245);
    c.checkActiveBorder = RGB(113, 113, 122);
    c.checkActiveHover = RGB(96, 165, 250);
    c.checkDoneBg = RGB(34, 197, 94);      // Green 500
    c.checkDoneHover = RGB(22, 163, 74);   // Green 600
    c.headerSectionBg = RGB(30, 30, 35);
    c.headerSectionText = RGB(148, 163, 184); // Slate 400
    return c;
}

inline ThemeColors GetLightTheme() {
    ThemeColors c;
    c.bgWindow = RGB(248, 250, 252);       // Slate 50
    c.bgHeader = RGB(255, 255, 255);
    c.bgSubBar = RGB(241, 245, 249);       // Slate 100
    c.bgCard = RGB(255, 255, 255);
    c.bgCardHover = RGB(241, 245, 249);
    c.bgCardSelected = RGB(239, 246, 255); // Blue 50
    c.borderCard = RGB(226, 232, 240);     // Slate 200
    c.borderCardHover = RGB(59, 130, 246);
    c.borderCardSelected = RGB(37, 99, 235);
    c.borderSep = RGB(226, 232, 240);
    c.textTitle = RGB(15, 23, 42);         // Slate 900
    c.textPrimary = RGB(15, 23, 42);
    c.textCompleted = RGB(148, 163, 184);  // Slate 400
    c.textSecondary = RGB(100, 116, 139);  // Slate 500
    c.textEmpty = RGB(148, 163, 184);
    c.bgEdit = RGB(255, 255, 255);
    c.textEdit = RGB(15, 23, 42);
    c.checkActiveBorder = RGB(203, 213, 225);
    c.checkActiveHover = RGB(59, 130, 246);
    c.checkDoneBg = RGB(34, 197, 94);
    c.checkDoneHover = RGB(22, 163, 74);
    c.headerSectionBg = RGB(241, 245, 249);
    c.headerSectionText = RGB(71, 85, 105);
    return c;
}

inline ThemeColors GetPinkTheme() {
    ThemeColors c;
    c.bgWindow = RGB(255, 123, 189);
    c.bgHeader = RGB(255, 70, 162);
    c.bgSubBar = RGB(255, 105, 180);
    c.bgCard = RGB(255, 247, 251);
    c.bgCardHover = RGB(255, 176, 215);
    c.bgCardSelected = RGB(255, 159, 207);
    c.borderCard = RGB(217, 0, 108);
    c.borderCardHover = RGB(181, 0, 90);
    c.borderCardSelected = RGB(144, 0, 72);
    c.borderSep = RGB(255, 141, 198);
    c.textTitle = RGB(255, 255, 255);
    c.textPrimary = RGB(79, 0, 72);
    c.textCompleted = RGB(159, 96, 134);
    c.textSecondary = RGB(108, 0, 54);
    c.textEmpty = RGB(108, 0, 54);
    c.bgEdit = RGB(255, 248, 252);
    c.textEdit = RGB(79, 0, 72);
    c.checkActiveBorder = RGB(217, 0, 108);
    c.checkActiveHover = RGB(181, 0, 90);
    c.checkDoneBg = RGB(255, 70, 162);
    c.checkDoneHover = RGB(217, 0, 108);
    c.headerSectionBg = RGB(255, 141, 198);
    c.headerSectionText = RGB(79, 0, 72);
    return c;
}

inline ThemeColors GetThemeColors(ThemeMode mode) {
    if (mode == ThemeMode::Dark) return GetDarkTheme();
    if (mode == ThemeMode::Pink) return GetPinkTheme();
    return GetLightTheme();
}
