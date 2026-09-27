#include "Paint.h"
#include "AppState.h"
#include "Drawing.h"
#include "Layout.h"
#include "TaskUtils.h"

static const std::wstring& CurrentUserName() {
    static const std::wstring userName = TaskUtils::GetCleanUserName();
    return userName;
}

static bool IsAssignedToOtherUser(const Task& task) {
    return task.is_sync && !task.assignee.empty() && task.assignee != CurrentUserName();
}

void PaintMiniWindow(HWND hWnd, HDC hdc) {
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int clientW = rcClient.right - rcClient.left;
    int clientH = rcClient.bottom - rcClient.top;

    ThemeColors th = g_darkMode ? GetDarkTheme() : GetLightTheme();

    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hBmp = CreateCompatibleBitmap(hdc, clientW, clientH);
    HGDIOBJ hOldBmp = SelectObject(hdcMem, hBmp);

    HBRUSH hBrushBg = CreateSolidBrush(th.bgWindow);
    FillRect(hdcMem, &rcClient, hBrushBg);
    DeleteObject(hBrushBg);

    HPEN hPenBorder = CreatePen(PS_SOLID, 1, g_darkMode ? RGB(63, 63, 70) : RGB(203, 213, 225));
    HGDIOBJ hOldPen = SelectObject(hdcMem, hPenBorder);
    HGDIOBJ hOldBrush = SelectObject(hdcMem, GetStockObject(NULL_BRUSH));
    Rectangle(hdcMem, 0, 0, clientW, clientH);
    SelectObject(hdcMem, hOldPen);
    SelectObject(hdcMem, hOldBrush);
    DeleteObject(hPenBorder);

    if (g_displayItems.empty()) {
        SelectObject(hdcMem, g_hFontNormal);
        SetTextColor(hdcMem, th.textEmpty);
        SetBkMode(hdcMem, TRANSPARENT);
        RECT rcEmpty = { 10, 0, clientW - 10, clientH - 3 };
        DrawTextW(hdcMem, L"Nincs aktív feladat! 🎉", -1, &rcEmpty, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        for (size_t i = 0; i < g_displayItems.size(); ++i) {
            const auto& item = g_displayItems[i];
            RECT r = item.rect;
            bool isHovered = ((int)i == g_hoverItemIndex);
            bool isBeingDragged = (g_taskDragging && (int)i == g_dragSourceIndex);
            bool isAssignedToOther = IsAssignedToOtherUser(item.task);

            COLORREF cardBg = isBeingDragged ? (g_darkMode ? RGB(30, 41, 59) : RGB(224, 231, 255))
                            : (isAssignedToOther ? (g_darkMode ? RGB(30, 58, 90) : RGB(219, 234, 254))
                            : (isHovered ? th.bgCardHover : th.bgCard));
            COLORREF cardBorder = isBeingDragged ? RGB(59, 130, 246)
                                : (isAssignedToOther ? (g_darkMode ? RGB(59, 130, 246) : RGB(147, 197, 253))
                                : (isHovered ? th.borderCardHover : th.borderCard));

            HBRUSH hCardBrush = CreateSolidBrush(cardBg);
            HPEN hCardPen = CreatePen(PS_SOLID, isBeingDragged ? 2 : 1, cardBorder);
            SelectObject(hdcMem, hCardBrush);
            SelectObject(hdcMem, hCardPen);
            RoundRect(hdcMem, r.left, r.top, r.right, r.bottom, 6, 6);
            DeleteObject(hCardBrush);
            DeleteObject(hCardPen);

            RECT rcChk = item.checkRect;
            bool chkHovered = (isHovered && g_hoverButtonType == 1);
            HBRUSH hChkBrush = CreateSolidBrush(chkHovered ? (g_darkMode ? RGB(55, 65, 81) : RGB(241, 245, 249)) : th.bgCard);
            HPEN hChkPen = CreatePen(PS_SOLID, chkHovered ? 2 : 1, chkHovered ? th.checkActiveHover : th.checkActiveBorder);
            SelectObject(hdcMem, hChkBrush);
            SelectObject(hdcMem, hChkPen);
            RoundRect(hdcMem, rcChk.left, rcChk.top, rcChk.right, rcChk.bottom, 5, 5);
            DeleteObject(hChkBrush);
            DeleteObject(hChkPen);

            if (item.task.is_sync) {
                int cy = (r.top + r.bottom) / 2;
                RECT rcCloud = { item.checkRect.right + 6, cy - 6, item.checkRect.right + 6 + 14, cy + 6 };
                COLORREF cloudCol = g_darkMode ? RGB(96, 165, 250) : RGB(37, 99, 235);
                DrawCloudIcon(hdcMem, rcCloud, cloudCol, true);
            }

            SelectObject(hdcMem, g_hFontNormal);
            SetTextColor(hdcMem, isHovered ? (g_darkMode ? RGB(255, 255, 255) : RGB(0, 0, 0)) : th.textPrimary);
            SetBkMode(hdcMem, TRANSPARENT);
            DrawTextW(hdcMem, item.task.text.c_str(), -1, (LPRECT)&item.textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            if (item.assignRect.right > 0) {
                bool isAssignedToMe = item.task.assignee == CurrentUserName();
                bool isAssignHovered = isHovered && g_hoverButtonType == 4;
                HBRUSH hAssignBrush = CreateSolidBrush(isAssignedToMe ? RGB(220, 38, 38) : th.bgCard);
                HPEN hAssignPen = CreatePen(PS_SOLID, 1, isAssignedToMe ? RGB(220, 38, 38)
                    : (isAssignHovered ? th.borderCardHover : th.borderCard));
                SelectObject(hdcMem, hAssignBrush);
                SelectObject(hdcMem, hAssignPen);
                RoundRect(hdcMem, item.assignRect.left, item.assignRect.top, item.assignRect.right, item.assignRect.bottom, 5, 5);
                DeleteObject(hAssignBrush);
                DeleteObject(hAssignPen);
                SelectObject(hdcMem, g_hFontSmall);
                SetTextColor(hdcMem, isAssignedToMe ? RGB(255, 255, 255) : th.textSecondary);
                DrawTextW(hdcMem, L"ÉN", -1, (LPRECT)&item.assignRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }

        if (g_taskDragging && g_dragInsertIndex >= 0) {
            int lineY = 0;
            if (g_dragInsertIndex < (int)g_displayItems.size()) {
                lineY = g_displayItems[g_dragInsertIndex].rect.top - 3;
            } else if (!g_displayItems.empty()) {
                lineY = g_displayItems.back().rect.bottom + 2;
            }
            HPEN hPenLine = CreatePen(PS_SOLID, 2, RGB(59, 130, 246));
            HGDIOBJ hPrevPen = SelectObject(hdcMem, hPenLine);
            MoveToEx(hdcMem, 8, lineY, nullptr);
            LineTo(hdcMem, clientW - 8, lineY);
            SelectObject(hdcMem, hPrevPen);
            DeleteObject(hPenLine);
        }
    }

    RECT rcTrackBg = { 0, clientH - 3, clientW, clientH };
    COLORREF bgTrack = g_darkMode ? RGB(39, 39, 42) : RGB(226, 232, 240);
    HBRUSH hBrTrackBg = CreateSolidBrush(bgTrack);
    FillRect(hdcMem, &rcTrackBg, hBrTrackBg);
    DeleteObject(hBrTrackBg);

    int workSec = g_store.work_seconds_today;
    const int targetWorkSec = 8 * 3600;
    double ratio = (double)workSec / (double)targetWorkSec;
    if (ratio > 1.0) ratio = 1.0;
    if (ratio < 0.0) ratio = 0.0;
    int barWidth = (int)(clientW * ratio);

    if (barWidth > 0) {
        COLORREF fillCol = g_isWorkActive ? RGB(34, 197, 94) : RGB(239, 68, 68);
        HBRUSH hBrFill = CreateSolidBrush(fillCol);
        RECT rcFill = { 0, clientH - 3, barWidth, clientH };
        FillRect(hdcMem, &rcFill, hBrFill);
        DeleteObject(hBrFill);
    }

    BitBlt(hdc, 0, 0, clientW, clientH, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
}

void PaintMainWindow(HWND hWnd, HDC hdc) {
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int clientW = rcClient.right - rcClient.left;
    int clientH = rcClient.bottom - rcClient.top;

    ThemeColors th = g_darkMode ? GetDarkTheme() : GetLightTheme();

    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hBmp = CreateCompatibleBitmap(hdc, clientW, clientH);
    HGDIOBJ hOldBmp = SelectObject(hdcMem, hBmp);

    const bool isHistory = g_viewMode == ViewMode::WorkHistory;
    const COLORREF historyBg = RGB(8, 25, 54);
    HBRUSH hBrushBg = CreateSolidBrush(isHistory ? historyBg : th.bgWindow);
    FillRect(hdcMem, &rcClient, hBrushBg);
    DeleteObject(hBrushBg);

    // 1. Draw Header
    RECT rcHeader = { 0, 0, clientW, 48 };
    HBRUSH hBrushHeader = CreateSolidBrush(isHistory ? historyBg : th.bgHeader);
    FillRect(hdcMem, &rcHeader, hBrushHeader);
    DeleteObject(hBrushHeader);

    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, isHistory ? RGB(235, 242, 252) : th.textTitle);
    SelectObject(hdcMem, g_hFontTitle);
    std::wstring title = g_viewMode == ViewMode::ActiveTasks ? L"Feladatok"
        : (isHistory ? L"Munkaidő" : L"Elkészült feladatok");
    RECT rcTitle = { 14, 12, 280, 40 };
    DrawTextW(hdcMem, title.c_str(), -1, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // 2. Draw sub-bar background
    if (g_viewMode == ViewMode::ActiveTasks) {
        RECT rcInputBg = { 0, 48, clientW, 94 };
        HBRUSH hBrushInput = CreateSolidBrush(th.bgSubBar);
        FillRect(hdcMem, &rcInputBg, hBrushInput);
        DeleteObject(hBrushInput);

        HPEN hPenSep = CreatePen(PS_SOLID, 1, th.borderSep);
        SelectObject(hdcMem, hPenSep);
        MoveToEx(hdcMem, 0, 93, nullptr);
        LineTo(hdcMem, clientW, 93);
        DeleteObject(hPenSep);
    } else if (g_viewMode == ViewMode::CompletedTasks) {
        RECT rcFilterBg = { 0, 48, clientW, 88 };
        HBRUSH hBrushFilter = CreateSolidBrush(th.bgSubBar);
        FillRect(hdcMem, &rcFilterBg, hBrushFilter);
        DeleteObject(hBrushFilter);

        HPEN hPenSep = CreatePen(PS_SOLID, 1, th.borderSep);
        SelectObject(hdcMem, hPenSep);
        MoveToEx(hdcMem, 0, 87, nullptr);
        LineTo(hdcMem, clientW, 87);
        DeleteObject(hPenSep);
    }

    // 3. Draw items with clipping to list area
    int topOffset = g_viewMode == ViewMode::ActiveTasks ? 94 : (isHistory ? 48 : 88);
    int listBottom = clientH - BOTTOM_BAR_HEIGHT;
    HRGN hRgnClip = CreateRectRgn(0, topOffset, clientW, listBottom);
    SelectClipRgn(hdcMem, hRgnClip);

    if (isHistory) {
        auto entries = g_store.GetWorkHistory();
        int maxSeconds = 1;
        for (const auto& entry : entries) {
            if (!entry.isWeeklySummary) maxSeconds = std::max(maxSeconds, entry.seconds);
        }

        SelectObject(hdcMem, g_hFontSmall);
        SetTextColor(hdcMem, RGB(153, 183, 219));
        RECT rcDayHeader = { 16, topOffset + 2 - g_scrollY, 64, topOffset + 22 - g_scrollY };
        RECT rcTimeHeader = { 68, topOffset + 2 - g_scrollY, 140, topOffset + 22 - g_scrollY };
        DrawTextW(hdcMem, L"Nap", -1, &rcDayHeader, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextW(hdcMem, L"Idő", -1, &rcTimeHeader, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        int rowY = topOffset + 24 - g_scrollY;
        for (const auto& entry : entries) {
            if (entry.isWeeklySummary) rowY += 12;
            int rowTop = rowY;
            rowY += 18;
            if (rowTop + 18 < topOffset || rowTop > listBottom) continue;

            SelectObject(hdcMem, g_hFontNormal);
            SetTextColor(hdcMem, RGB(235, 242, 252));
            RECT rcDate = { 16, rowTop, entry.isWeeklySummary ? 82 : 64, rowTop + 18 };
            DrawTextW(hdcMem, entry.label.c_str(), -1, &rcDate, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            int tenthsOfHour = std::max(0, entry.seconds) / 360;
            std::wstring duration = std::to_wstring(tenthsOfHour / 10) + L"," +
                std::to_wstring(tenthsOfHour % 10) + L" óra";
            RECT rcDuration = { entry.isWeeklySummary ? 84 : 68, rowTop, 140, rowTop + 18 };
            DrawTextW(hdcMem, duration.c_str(), -1, &rcDuration, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            RECT rcBar = entry.isWeeklySummary
                ? RECT{ 145, rowTop + 5, clientW - 20, rowTop + 12 }
                : RECT{ 145, rowTop + 7, clientW - 20, rowTop + 10 };
            HBRUSH hTrack = CreateSolidBrush(RGB(16, 43, 78));
            FillRect(hdcMem, &rcBar, hTrack);
            DeleteObject(hTrack);
            int barWidth = entry.isWeeklySummary
                ? (int)((long long)(rcBar.right - rcBar.left) * std::min(entry.seconds, 40 * 60 * 60) / (40 * 60 * 60))
                : (int)((long long)(rcBar.right - rcBar.left) * std::max(0, entry.seconds) / maxSeconds);
            if (barWidth > 0) {
                RECT rcFill = rcBar;
                rcFill.right = rcFill.left + barWidth;
                HBRUSH hFill = CreateSolidBrush(entry.isWeeklySummary ? RGB(255, 205, 64) : RGB(55, 145, 232));
                FillRect(hdcMem, &rcFill, hFill);
                DeleteObject(hFill);
            }
        }
    } else if (g_displayItems.empty()) {
        SelectObject(hdcMem, g_hFontNormal);
        SetTextColor(hdcMem, th.textEmpty);
        RECT rcEmpty = { 20, topOffset + 60, clientW - 20, topOffset + 140 };
        std::wstring emptyMsg = (g_viewMode == ViewMode::ActiveTasks)
            ? L"Nincs aktív feladat! Írj be egy újat a fenti mezőbe."
            : L"Nincs megjeleníthető elkészült feladat.";
        DrawTextW(hdcMem, emptyMsg.c_str(), -1, &rcEmpty, DT_CENTER | DT_WORDBREAK);
    } else {
        for (size_t i = 0; i < g_displayItems.size(); ++i) {
            const auto& item = g_displayItems[i];
            RECT r = item.rect;
            r.top -= g_scrollY;
            r.bottom -= g_scrollY;

            if (r.bottom < topOffset || r.top > listBottom) continue;

            if (item.isHeader) {
                RECT rcHdr = r;
                HBRUSH hHdrBrush = CreateSolidBrush(th.headerSectionBg);
                FillRect(hdcMem, &rcHdr, hHdrBrush);
                DeleteObject(hHdrBrush);

                SetTextColor(hdcMem, th.headerSectionText);
                SelectObject(hdcMem, g_hFontHeader);
                RECT rcHdrText = { rcHdr.left + 8, rcHdr.top, rcHdr.right, rcHdr.bottom };
                DrawTextW(hdcMem, item.headerText.c_str(), -1, &rcHdrText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                continue;
            }

            bool isHovered = ((int)i == g_hoverItemIndex);
            bool isSelected = ((int)i == g_selectedIndex);
            bool isBeingDragged = (g_taskDragging && (int)i == g_dragSourceIndex);
            bool isAssignedToOther = IsAssignedToOtherUser(item.task);

            COLORREF cardBg = isBeingDragged ? (g_darkMode ? RGB(30, 41, 59) : RGB(224, 231, 255))
                            : (isAssignedToOther ? (g_darkMode ? RGB(30, 58, 90) : RGB(219, 234, 254))
                            : (isSelected ? th.bgCardSelected : (isHovered ? th.bgCardHover : th.bgCard)));
            COLORREF cardBorder = isBeingDragged ? RGB(59, 130, 246)
                                : (isAssignedToOther ? (g_darkMode ? RGB(59, 130, 246) : RGB(147, 197, 253))
                                : (isSelected ? th.borderCardSelected : (isHovered ? th.borderCardHover : th.borderCard)));

            HBRUSH hCardBrush = CreateSolidBrush(cardBg);
            HPEN hCardPen = CreatePen(PS_SOLID, (isSelected || isBeingDragged) ? 2 : 1, cardBorder);
            SelectObject(hdcMem, hCardBrush);
            SelectObject(hdcMem, hCardPen);
            RoundRect(hdcMem, r.left, r.top, r.right, r.bottom, 8, 8);
            DeleteObject(hCardBrush);
            DeleteObject(hCardPen);

            RECT rcChk = item.checkRect;
            rcChk.top -= g_scrollY;
            rcChk.bottom -= g_scrollY;

            bool chkHovered = (isHovered && g_hoverButtonType == 1);

            if (item.task.completed) {
                HBRUSH hChkBrush = CreateSolidBrush(chkHovered ? th.checkDoneHover : th.checkDoneBg);
                HPEN hChkPen = CreatePen(PS_SOLID, 1, th.checkDoneHover);
                SelectObject(hdcMem, hChkBrush);
                SelectObject(hdcMem, hChkPen);
                RoundRect(hdcMem, rcChk.left, rcChk.top, rcChk.right, rcChk.bottom, 6, 6);
                DeleteObject(hChkBrush);
                DeleteObject(hChkPen);

                HPEN hTickPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
                SelectObject(hdcMem, hTickPen);
                POINT tickPts[3] = {
                    { rcChk.left + 4, rcChk.top + 10 },
                    { rcChk.left + 8, rcChk.top + 14 },
                    { rcChk.left + 15, rcChk.top + 6 }
                };
                Polyline(hdcMem, tickPts, 3);
                DeleteObject(hTickPen);
            } else {
                HBRUSH hChkBrush = CreateSolidBrush(chkHovered ? (g_darkMode ? RGB(55, 65, 81) : RGB(241, 245, 249)) : th.bgCard);
                HPEN hChkPen = CreatePen(PS_SOLID, chkHovered ? 2 : 1, chkHovered ? th.checkActiveHover : th.checkActiveBorder);
                SelectObject(hdcMem, hChkBrush);
                SelectObject(hdcMem, hChkPen);
                RoundRect(hdcMem, rcChk.left, rcChk.top, rcChk.right, rcChk.bottom, 6, 6);
                DeleteObject(hChkBrush);
                DeleteObject(hChkPen);
            }

            bool isInlineEditingThis = (g_editingTaskId == item.task.id);
            int textRight = item.editRect.right > 0 ? (item.editRect.left - 6) : (item.deleteRect.left - 6);
            RECT rcText = item.textRect;
            rcText.top -= g_scrollY;
            rcText.bottom -= g_scrollY;
            rcText.right = textRight;

            int metaTop = r.top + 24;
            if (metaTop < rcText.bottom + 2) metaTop = rcText.bottom + 2;
            RECT rcMeta = { rcChk.right + 10, metaTop, textRight, r.bottom - 3 };

            if (item.task.is_sync) {
                int cloudLeft = rcChk.right + 10;
                int cy = rcText.top + (rcText.bottom - rcText.top) / 2;
                RECT rcCloud = { cloudLeft, cy - 6, cloudLeft + 14, cy + 6 };
                COLORREF cloudCol = item.task.completed ? th.textCompleted : (g_darkMode ? RGB(96, 165, 250) : RGB(37, 99, 235));
                DrawCloudIcon(hdcMem, rcCloud, cloudCol, true);
            }

            if (!isInlineEditingThis) {
                if (item.task.completed) {
                    SelectObject(hdcMem, g_hFontNormalStrike);
                    SetTextColor(hdcMem, th.textCompleted);
                } else {
                    SelectObject(hdcMem, g_hFontNormal);
                    SetTextColor(hdcMem, th.textPrimary);
                }
                DrawTextW(hdcMem, item.task.text.c_str(), -1, &rcText, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

                SelectObject(hdcMem, g_hFontSmall);
                SetTextColor(hdcMem, th.textSecondary);
                std::wstring meta;
                if (item.task.completed) {
                    meta = L"Kész: " + TaskUtils::FormatDateTime(item.task.completed_at);
                } else {
                    meta = L"Létrehozva: " + TaskUtils::FormatDateTime(item.task.created_at);
                }
                if (item.task.is_sync && !item.task.author.empty()) {
                    meta += L" • " + item.task.author;
                }
                DrawTextW(hdcMem, meta.c_str(), -1, &rcMeta, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }

            // Edit button (active view only)
            if (item.editRect.right > 0) {
                RECT rcEdt = item.editRect;
                rcEdt.top -= g_scrollY;
                rcEdt.bottom -= g_scrollY;
                bool btnHover = (isHovered && g_hoverButtonType == 2);

                if (btnHover) {
                    HBRUSH hBtnBr = CreateSolidBrush(g_darkMode ? RGB(55, 65, 81) : RGB(238, 242, 255));
                    HPEN hBtnPen = CreatePen(PS_SOLID, 1, g_darkMode ? RGB(96, 165, 250) : RGB(199, 210, 254));
                    SelectObject(hdcMem, hBtnBr);
                    SelectObject(hdcMem, hBtnPen);
                    RoundRect(hdcMem, rcEdt.left, rcEdt.top, rcEdt.right, rcEdt.bottom, 4, 4);
                    DeleteObject(hBtnBr);
                    DeleteObject(hBtnPen);
                }
                COLORREF pencilColor = btnHover ? (g_darkMode ? RGB(147, 197, 253) : RGB(37, 99, 235)) : th.textSecondary;
                DrawPencilIcon(hdcMem, rcEdt, pencilColor);
            }

            // Delete button (✕)
            RECT rcDel = item.deleteRect;
            rcDel.top -= g_scrollY;
            rcDel.bottom -= g_scrollY;
            bool delHover = (isHovered && g_hoverButtonType == 3);

            if (delHover) {
                HBRUSH hDelBr = CreateSolidBrush(g_darkMode ? RGB(127, 29, 29) : RGB(254, 242, 242));
                HPEN hDelPen = CreatePen(PS_SOLID, 1, g_darkMode ? RGB(239, 68, 68) : RGB(254, 202, 202));
                SelectObject(hdcMem, hDelBr);
                SelectObject(hdcMem, hDelPen);
                RoundRect(hdcMem, rcDel.left, rcDel.top, rcDel.right, rcDel.bottom, 4, 4);
                DeleteObject(hDelBr);
                DeleteObject(hDelPen);
            }
            SetTextColor(hdcMem, delHover ? RGB(239, 68, 68) : th.textSecondary);
            SelectObject(hdcMem, g_hFontHeader);
            DrawTextW(hdcMem, L"✕", -1, &rcDel, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            if (item.assignRect.right > 0) {
                RECT rcAssign = item.assignRect;
                rcAssign.top -= g_scrollY;
                rcAssign.bottom -= g_scrollY;
                bool isAssignedToMe = item.task.assignee == CurrentUserName();
                bool isAssignHovered = (isHovered && g_hoverButtonType == 4);
                HBRUSH hAssignBrush = CreateSolidBrush(isAssignedToMe ? RGB(220, 38, 38) : th.bgCard);
                HPEN hAssignPen = CreatePen(PS_SOLID, 1, isAssignedToMe ? RGB(220, 38, 38)
                    : (isAssignHovered ? th.borderCardHover : th.borderCard));
                SelectObject(hdcMem, hAssignBrush);
                SelectObject(hdcMem, hAssignPen);
                RoundRect(hdcMem, rcAssign.left, rcAssign.top, rcAssign.right, rcAssign.bottom, 5, 5);
                DeleteObject(hAssignBrush);
                DeleteObject(hAssignPen);
                SelectObject(hdcMem, g_hFontSmall);
                SetTextColor(hdcMem, isAssignedToMe ? RGB(255, 255, 255) : th.textSecondary);
                DrawTextW(hdcMem, L"ÉN", -1, &rcAssign, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }

        if (g_viewMode == ViewMode::ActiveTasks && g_taskDragging && g_dragInsertIndex >= 0) {
            int lineY = 0;
            if (g_dragInsertIndex < (int)g_displayItems.size()) {
                lineY = g_displayItems[g_dragInsertIndex].rect.top - g_scrollY - 3;
            } else if (!g_displayItems.empty()) {
                lineY = g_displayItems.back().rect.bottom - g_scrollY + 3;
            }
            if (lineY >= topOffset && lineY <= listBottom) {
                HPEN hPenLine = CreatePen(PS_SOLID, 2, RGB(59, 130, 246));
                HGDIOBJ hOldPen = SelectObject(hdcMem, hPenLine);
                MoveToEx(hdcMem, 10, lineY, nullptr);
                LineTo(hdcMem, clientW - 14, lineY);
                SelectObject(hdcMem, hOldPen);
                DeleteObject(hPenLine);
            }
        }
    }

    SelectClipRgn(hdcMem, nullptr);
    DeleteObject(hRgnClip);

    // 3b. Draw Custom 5px Scrollbar
    if (!g_isMiniMode) {
        auto sm = GetScrollbarMetrics();
        if (sm.visible) {
            if (g_scrollbarHovered || g_scrollbarDragging) {
                COLORREF trackColor = g_darkMode ? RGB(35, 35, 42) : RGB(232, 235, 240);
                HBRUSH hBrTrack = CreateSolidBrush(trackColor);
                HPEN hNullPen = (HPEN)GetStockObject(NULL_PEN);
                HGDIOBJ hOldPen = SelectObject(hdcMem, hNullPen);
                HGDIOBJ hOldBr = SelectObject(hdcMem, hBrTrack);
                RoundRect(hdcMem, sm.rcTrack.left, sm.rcTrack.top, sm.rcTrack.right, sm.rcTrack.bottom, 5, 5);
                SelectObject(hdcMem, hOldBr);
                SelectObject(hdcMem, hOldPen);
                DeleteObject(hBrTrack);
            }

            COLORREF thumbColor;
            if (g_scrollbarDragging) {
                thumbColor = g_darkMode ? RGB(180, 180, 205) : RGB(100, 105, 120);
            } else if (g_scrollbarHovered) {
                thumbColor = g_darkMode ? RGB(140, 140, 160) : RGB(140, 145, 160);
            } else {
                thumbColor = g_darkMode ? RGB(85, 85, 100) : RGB(190, 195, 205);
            }

            HBRUSH hBrThumb = CreateSolidBrush(thumbColor);
            HPEN hNullPen = (HPEN)GetStockObject(NULL_PEN);
            HGDIOBJ hOldPen = SelectObject(hdcMem, hNullPen);
            HGDIOBJ hOldBr = SelectObject(hdcMem, hBrThumb);
            RoundRect(hdcMem, sm.rcThumb.left, sm.rcThumb.top, sm.rcThumb.right, sm.rcThumb.bottom, 5, 5);
            SelectObject(hdcMem, hOldBr);
            SelectObject(hdcMem, hOldPen);
            DeleteObject(hBrThumb);
        }
    }

    // 4. Draw Bottom Work Time Progress Bar
    RECT rcBottom = { 0, clientH - BOTTOM_BAR_HEIGHT, clientW, clientH };
    COLORREF bgBottom = isHistory ? RGB(8, 25, 54) : (g_darkMode ? RGB(18, 18, 22) : RGB(241, 245, 249));
    HBRUSH hBrBottom = CreateSolidBrush(bgBottom);
    FillRect(hdcMem, &rcBottom, hBrBottom);
    DeleteObject(hBrBottom);

    HPEN hPenBotBorder = CreatePen(PS_SOLID, 1, th.borderSep);
    SelectObject(hdcMem, hPenBotBorder);
    MoveToEx(hdcMem, 0, rcBottom.top, nullptr);
    LineTo(hdcMem, clientW, rcBottom.top);
    DeleteObject(hPenBotBorder);

    RECT rcTrack = { 10, rcBottom.top + 3, clientW - 10, rcBottom.bottom - 3 };
    COLORREF borderTrack = isHistory ? RGB(104, 139, 181) : (g_darkMode ? RGB(255, 255, 255) : RGB(100, 116, 139));
    COLORREF bgTrack = isHistory ? RGB(16, 43, 78) : (g_darkMode ? RGB(28, 28, 34) : RGB(226, 232, 240));

    HBRUSH hBrTrack = CreateSolidBrush(bgTrack);
    HPEN hPenTrack = CreatePen(PS_SOLID, 1, borderTrack);
    SelectObject(hdcMem, hBrTrack);
    SelectObject(hdcMem, hPenTrack);
    RoundRect(hdcMem, rcTrack.left, rcTrack.top, rcTrack.right, rcTrack.bottom, 4, 4);
    DeleteObject(hBrTrack);
    DeleteObject(hPenTrack);

    int workSec = g_store.work_seconds_today;
    const int targetWorkSec = 8 * 3600;
    double ratio = (double)workSec / (double)targetWorkSec;
    if (ratio > 1.0) ratio = 1.0;
    if (ratio < 0.0) ratio = 0.0;
    int trackInnerW = (rcTrack.right - 1) - (rcTrack.left + 1);
    int barWidth = (int)(trackInnerW * ratio);

    if (barWidth > 0) {
        RECT rcFill = { rcTrack.left + 1, rcTrack.top + 1, rcTrack.left + 1 + barWidth, rcTrack.bottom - 1 };
        COLORREF fillCol = g_isWorkActive 
            ? (g_darkMode ? RGB(37, 99, 235) : RGB(147, 197, 253)) 
            : (g_darkMode ? RGB(71, 85, 105) : RGB(203, 213, 225));
        HBRUSH hBrFill = CreateSolidBrush(fillCol);
        FillRect(hdcMem, &rcFill, hBrFill);
        DeleteObject(hBrFill);
    }

    int dotY = rcTrack.top + (rcTrack.bottom - rcTrack.top) / 2;
    int dotX = rcTrack.left + 10;
    COLORREF dotCol = RGB(156, 163, 175);
    if (g_isWorkActive) {
        dotCol = RGB(34, 197, 94);
    } else if (g_isExcludedApp) {
        dotCol = RGB(249, 115, 22);
    }

    HBRUSH hBrDot = CreateSolidBrush(dotCol);
    HPEN hPenDot = CreatePen(PS_SOLID, 1, dotCol);
    SelectObject(hdcMem, hBrDot);
    SelectObject(hdcMem, hPenDot);
    Ellipse(hdcMem, dotX - 4, dotY - 4, dotX + 4, dotY + 4);
    DeleteObject(hBrDot);
    DeleteObject(hPenDot);

    SelectObject(hdcMem, g_hFontSmall);
    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, isHistory ? RGB(235, 242, 252) : (g_darkMode ? RGB(255, 255, 255) : RGB(15, 23, 42)));

    std::wstring workStr = TaskUtils::FormatWorkDuration(workSec);
    double pct = ((double)workSec / (double)targetWorkSec) * 100.0;
    wchar_t labelBuf[128];
    if (g_isWorkActive) {
        swprintf_s(labelBuf, L"Munkaidő: %s (%.0f%%)", workStr.c_str(), pct);
    } else if (g_isExcludedApp) {
        swprintf_s(labelBuf, L"Munkaidő: %s (Szüneteltetve: %s megnyitva)", workStr.c_str(), g_excludedAppName.c_str());
    } else {
        swprintf_s(labelBuf, L"Munkaidő: %s (Szüneteltetve: inaktív)", workStr.c_str());
    }
    RECT rcLabel = { dotX + 10, rcTrack.top, rcTrack.right - 10, rcTrack.bottom };
    DrawTextW(hdcMem, labelBuf, -1, &rcLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    BitBlt(hdc, 0, 0, clientW, clientH, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
}
