#include "Paint.h"
#include "AppState.h"
#include "Drawing.h"
#include "Layout.h"
#include "TaskUtils.h"
#include "BarTooltips.h"
#include "Marker.h"

namespace {
    constexpr int DAILY_WORK_TARGET_SECONDS = 6 * 3600 + 50 * 60;
    constexpr int WEEKLY_WORK_TARGET_SECONDS = 34 * 3600 + 10 * 60;

    int CurrentManualWorkSeconds() {
        time_t currentDay = g_store.last_reset_time != 0 ? g_store.last_reset_time : time(nullptr);
        auto it = g_store.manual_work_history.find(WorkHistory::DateKey(currentDay));
        return it == g_store.manual_work_history.end() ? 0 : std::max(0, it->second);
    }

    std::wstring FormatTooltipDuration(int seconds) {
        int minutes = std::max(0, seconds) / 60;
        return std::to_wstring(minutes / 60) + L" óra " + std::to_wstring(minutes % 60) + L" perc";
    }

    std::wstring BuildWorkTooltip(int measuredSeconds, int manualSeconds) {
        return L"Mért: " + FormatTooltipDuration(measuredSeconds) +
            L"\nKézi: " + FormatTooltipDuration(manualSeconds) +
            L"\nÖsszesen: " + FormatTooltipDuration(measuredSeconds + manualSeconds);
    }

    void DrawBarSegments(HDC hdc, const RECT& track, int measuredSeconds, int manualSeconds,
        int denominator, COLORREF measuredColor, COLORREF manualColor) {
        int trackWidth = std::max(0, static_cast<int>(track.right - track.left));
        if (trackWidth == 0 || denominator <= 0) return;
        long long totalSeconds = (long long)std::max(0, measuredSeconds) + std::max(0, manualSeconds);
        int totalWidth = static_cast<int>((long long)trackWidth * std::min<long long>(totalSeconds, denominator) / denominator);
        int measuredWidth = static_cast<int>((long long)trackWidth * std::min(std::max(0, measuredSeconds), denominator) / denominator);
        measuredWidth = std::min(measuredWidth, totalWidth);
        if (measuredWidth > 0) {
            RECT fill = track;
            fill.right = fill.left + measuredWidth;
            HBRUSH brush = CreateSolidBrush(measuredColor);
            FillRect(hdc, &fill, brush);
            DeleteObject(brush);
        }
        if (totalWidth > measuredWidth) {
            RECT fill = track;
            fill.left += measuredWidth;
            fill.right = track.left + totalWidth;
            HBRUSH brush = CreateSolidBrush(manualColor);
            FillRect(hdc, &fill, brush);
            DeleteObject(brush);
        }
    }
}

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

    ThemeColors th = GetThemeColors(g_themeMode);
    const COLORREF miniBg = g_themeMode == ThemeMode::Pink ? RGB(255, 20, 147) : th.bgWindow;

    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hBmp = CreateCompatibleBitmap(hdc, clientW, clientH);
    HGDIOBJ hOldBmp = SelectObject(hdcMem, hBmp);

    HBRUSH hBrushBg = CreateSolidBrush(miniBg);
    FillRect(hdcMem, &rcClient, hBrushBg);
    DeleteObject(hBrushBg);

    HPEN hPenBorder = CreatePen(PS_SOLID, 1, th.borderCard);
    HGDIOBJ hOldPen = SelectObject(hdcMem, hPenBorder);
    HGDIOBJ hOldBrush = SelectObject(hdcMem, GetStockObject(NULL_BRUSH));
    Rectangle(hdcMem, 0, 0, clientW, clientH);
    SelectObject(hdcMem, hOldPen);
    SelectObject(hdcMem, hOldBrush);
    DeleteObject(hPenBorder);

    RECT rcDragHandle = GetMiniDragHandleRect(clientW);
    HBRUSH hBrushGripDots = CreateSolidBrush(g_themeMode == ThemeMode::Pink
        ? RGB(0, 0, 0) : (g_darkMode ? RGB(212, 212, 216) : RGB(71, 85, 105)));
    HGDIOBJ hOldDotsBrush = SelectObject(hdcMem, hBrushGripDots);
    HGDIOBJ hOldDotsPen = SelectObject(hdcMem, GetStockObject(NULL_PEN));
    int gripCenterX = rcDragHandle.left + (rcDragHandle.right - rcDragHandle.left) / 2;
    int gripCenterY = rcDragHandle.top + (rcDragHandle.bottom - rcDragHandle.top) / 2;
    for (int row = -1; row <= 1; ++row) {
        for (int column = 0; column < 2; ++column) {
            int dotX = gripCenterX + (column == 0 ? -4 : 4);
            int dotY = gripCenterY + row * 5;
            Ellipse(hdcMem, dotX - 2, dotY - 2, dotX + 2, dotY + 2);
        }
    }
    SelectObject(hdcMem, hOldDotsPen);
    SelectObject(hdcMem, hOldDotsBrush);
    DeleteObject(hBrushGripDots);

    RECT rcFocusButton = GetMiniFocusButtonRect(clientW);
    COLORREF focusBg = g_focusMode ? RGB(30, 58, 138)
        : (g_themeMode == ThemeMode::Pink ? miniBg : th.bgCard);
    COLORREF focusBorder = g_focusMode ? RGB(30, 64, 175) : th.borderSep;
    COLORREF focusIcon = g_focusMode ? RGB(255, 255, 255)
        : (g_themeMode == ThemeMode::Pink ? RGB(0, 0, 0) : th.textSecondary);
    HBRUSH hFocusBrush = CreateSolidBrush(focusBg);
    HPEN hFocusPen = CreatePen(PS_SOLID, 1, focusBorder);
    HGDIOBJ hOldFocusBrush = SelectObject(hdcMem, hFocusBrush);
    HGDIOBJ hOldFocusPen = SelectObject(hdcMem, hFocusPen);
    RoundRect(hdcMem, rcFocusButton.left, rcFocusButton.top, rcFocusButton.right,
        rcFocusButton.bottom, 6, 6);
    SelectObject(hdcMem, hOldFocusPen);
    SelectObject(hdcMem, hOldFocusBrush);
    DeleteObject(hFocusPen);
    DeleteObject(hFocusBrush);
    DrawFocusIcon(hdcMem, rcFocusButton, focusIcon);

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

            COLORREF cardBg = isBeingDragged ? th.bgCardSelected
                : (isAssignedToOther ? (g_themeMode == ThemeMode::Pink ? RGB(108, 0, 54)
                    : (g_darkMode ? RGB(30, 58, 90) : RGB(219, 234, 254)))
                    : (g_themeMode == ThemeMode::Pink ? miniBg
                        : (isHovered ? th.bgCardHover : th.bgCard)));
            COLORREF cardBorder = isBeingDragged ? th.borderCardSelected
                : (isAssignedToOther ? (g_themeMode == ThemeMode::Pink ? RGB(244, 114, 182)
                    : (g_darkMode ? RGB(59, 130, 246) : RGB(147, 197, 253)))
                    : (g_themeMode == ThemeMode::Pink
                        ? (isHovered ? RGB(0x48, 0, 0x24) : th.borderCard)
                        : (isHovered ? th.borderCardHover : th.borderCard)));

            HBRUSH hCardBrush = CreateSolidBrush(cardBg);
            HPEN hCardPen = CreatePen(PS_SOLID, isBeingDragged ? 2 : 1, cardBorder);
            SelectObject(hdcMem, hCardBrush);
            SelectObject(hdcMem, hCardPen);
            RoundRect(hdcMem, r.left, r.top, r.right, r.bottom, 6, 6);
            DeleteObject(hCardBrush);
            DeleteObject(hCardPen);

            RECT rcChk = item.checkRect;
            bool chkHovered = (isHovered && g_hoverButtonType == 1);
            if (item.task.completed) {
                HBRUSH hChkBrush = CreateSolidBrush(chkHovered ? th.checkDoneHover : th.checkDoneBg);
                HPEN hChkPen = CreatePen(PS_SOLID, 1, th.checkDoneHover);
                SelectObject(hdcMem, hChkBrush);
                SelectObject(hdcMem, hChkPen);
                RoundRect(hdcMem, rcChk.left, rcChk.top, rcChk.right, rcChk.bottom, 5, 5);
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
                HBRUSH hChkBrush = CreateSolidBrush(chkHovered ? (g_darkMode ? RGB(55, 65, 81) : th.bgCardHover) : th.bgCard);
                HPEN hChkPen = CreatePen(PS_SOLID, chkHovered ? 2 : 1, chkHovered ? th.checkActiveHover : th.checkActiveBorder);
                SelectObject(hdcMem, hChkBrush);
                SelectObject(hdcMem, hChkPen);
                RoundRect(hdcMem, rcChk.left, rcChk.top, rcChk.right, rcChk.bottom, 5, 5);
                DeleteObject(hChkBrush);
                DeleteObject(hChkPen);
            }

            if (item.task.is_sync) {
                int cy = (r.top + r.bottom) / 2;
                RECT rcCloud = { item.checkRect.right + 6, cy - 6, item.checkRect.right + 6 + 14, cy + 6 };
                COLORREF cloudCol = th.borderCardSelected;
                DrawCloudIcon(hdcMem, rcCloud, cloudCol, true);
            }

            SelectObject(hdcMem, g_hFontNormal);
            SetTextColor(hdcMem, g_themeMode == ThemeMode::Pink
                ? (isAssignedToOther ? RGB(255, 255, 255) : RGB(0, 0, 0))
                : th.textPrimary);
            SetBkMode(hdcMem, TRANSPARENT);
            DrawTextW(hdcMem, item.task.text.c_str(), -1, (LPRECT)&item.textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            if (!item.task.is_sync && item.task.marker != TaskMarker::None) {
                RECT rcMarker = { item.rect.right - 21, item.rect.top + 2, item.rect.right - 3, item.rect.bottom - 2 };
                DrawTaskMarker(hdcMem, rcMarker, item.task.marker,
                    g_themeMode == ThemeMode::Pink ? miniBg : th.bgCard, th.borderCard);
            }

            if (item.assignRect.right > 0) {
                bool isAssignedToMe = item.task.assignee == CurrentUserName();
                bool isUnavailable = IsAssignedToOtherUser(item.task);
                bool isAssignHovered = !isUnavailable && isHovered && g_hoverButtonType == 4;
                COLORREF assignBg = isUnavailable ? (g_darkMode ? RGB(55, 65, 81) : RGB(226, 232, 240))
                    : (isAssignedToMe ? RGB(220, 38, 38) : th.bgCard);
                COLORREF assignBorder = isUnavailable ? (g_darkMode ? RGB(75, 85, 99) : RGB(203, 213, 225))
                    : (isAssignedToMe ? RGB(220, 38, 38)
                        : (isAssignHovered ? th.borderCardHover : th.borderCard));
                HBRUSH hAssignBrush = CreateSolidBrush(assignBg);
                HPEN hAssignPen = CreatePen(PS_SOLID, 1, assignBorder);
                SelectObject(hdcMem, hAssignBrush);
                SelectObject(hdcMem, hAssignPen);
                RoundRect(hdcMem, item.assignRect.left, item.assignRect.top, item.assignRect.right, item.assignRect.bottom, 5, 5);
                DeleteObject(hAssignBrush);
                DeleteObject(hAssignPen);
                SelectObject(hdcMem, g_hFontSmall);
                SetTextColor(hdcMem, isUnavailable ? (g_darkMode ? RGB(156, 163, 175) : RGB(100, 116, 139))
                    : (isAssignedToMe ? RGB(255, 255, 255) : th.textSecondary));
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
    COLORREF bgTrack = g_themeMode == ThemeMode::Pink ? RGB(217, 0, 108) : th.borderSep;
    HBRUSH hBrTrackBg = CreateSolidBrush(bgTrack);
    FillRect(hdcMem, &rcTrackBg, hBrTrackBg);
    DeleteObject(hBrTrackBg);

    int workSec = g_store.work_seconds_today;
    const int targetWorkSec = DAILY_WORK_TARGET_SECONDS;
    int manualSec = CurrentManualWorkSeconds();
    RECT rcMiniBar = { 0, clientH - 3, clientW, clientH };
    DrawBarSegments(hdcMem, rcMiniBar, workSec, manualSec, targetWorkSec,
        g_isWorkActive ? RGB(34, 197, 94) : RGB(239, 68, 68), RGB(245, 158, 11));
    std::vector<BarTooltips::Region> barToolRegions = {
        { rcMiniBar, BuildWorkTooltip(workSec, manualSec) }
    };
    BarTooltips::Update(hWnd, barToolRegions);

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

    ThemeColors th = GetThemeColors(g_themeMode);

    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hBmp = CreateCompatibleBitmap(hdc, clientW, clientH);
    HGDIOBJ hOldBmp = SelectObject(hdcMem, hBmp);

    const bool isHistory = g_viewMode == ViewMode::WorkHistory;
    std::vector<BarTooltips::Region> barToolRegions;
    const COLORREF historyBg = g_themeMode == ThemeMode::Pink ? th.bgHeader : RGB(8, 25, 54);
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
    }

    // 3. Draw items with clipping to list area
    int topOffset = g_viewMode == ViewMode::ActiveTasks ? 94 : (isHistory ? 48 : 54);
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
        SetTextColor(hdcMem, g_themeMode == ThemeMode::Pink ? th.bgCardHover : RGB(153, 183, 219));
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
            HBRUSH hTrack = CreateSolidBrush(g_themeMode == ThemeMode::Pink ? th.bgCardSelected : RGB(16, 43, 78));
            FillRect(hdcMem, &rcBar, hTrack);
            DeleteObject(hTrack);
            int barWidth = entry.isWeeklySummary
                ? (int)((long long)(rcBar.right - rcBar.left) * std::min(entry.seconds, WEEKLY_WORK_TARGET_SECONDS) / WEEKLY_WORK_TARGET_SECONDS)
                : (int)((long long)(rcBar.right - rcBar.left) * std::max(0, entry.seconds) / maxSeconds);
            if (barWidth > 0) {
                RECT rcFill = rcBar;
                int manualWidth = entry.isWeeklySummary
                    ? (int)((long long)(rcBar.right - rcBar.left) * std::min(entry.manualSeconds, WEEKLY_WORK_TARGET_SECONDS) / WEEKLY_WORK_TARGET_SECONDS)
                    : (int)((long long)(rcBar.right - rcBar.left) * std::max(0, entry.manualSeconds) / maxSeconds);
                manualWidth = std::min(manualWidth, barWidth);
                int measuredWidth = barWidth - manualWidth;
                rcFill.right = rcFill.left + measuredWidth;
                HBRUSH hFill = CreateSolidBrush(entry.isWeeklySummary ? RGB(255, 205, 64) : RGB(55, 145, 232));
                if (measuredWidth > 0) FillRect(hdcMem, &rcFill, hFill);
                DeleteObject(hFill);
                if (manualWidth > 0) {
                    rcFill.left += measuredWidth;
                    rcFill.right = rcFill.left + manualWidth;
                    HBRUSH hManual = CreateSolidBrush(entry.isWeeklySummary ? RGB(244, 122, 52) : RGB(245, 158, 11));
                    FillRect(hdcMem, &rcFill, hManual);
                    DeleteObject(hManual);
                }
            }
            if (entry.isWeeklySummary) {
                int percentage = static_cast<int>(
                    (static_cast<long long>(std::max(0, entry.seconds)) * 100 + WEEKLY_WORK_TARGET_SECONDS / 2) /
                    WEEKLY_WORK_TARGET_SECONDS);
                std::wstring percentageText = std::to_wstring(percentage) + L"%";
                HGDIOBJ oldFont = SelectObject(hdcMem, g_hFontSmall);
                COLORREF oldTextColor = SetTextColor(hdcMem, RGB(0, 0, 0));
                int oldBackgroundMode = SetBkMode(hdcMem, TRANSPARENT);
                SIZE textSize{};
                GetTextExtentPoint32W(hdcMem, percentageText.c_str(), static_cast<int>(percentageText.size()), &textSize);

                int textX = rcBar.left + ((rcBar.right - rcBar.left) - textSize.cx) / 2;
                int textY = rcBar.top + ((rcBar.bottom - rcBar.top) - textSize.cy) / 2;
                RECT labelBackground = { textX - 2, textY - 2, textX + textSize.cx + 2, textY + textSize.cy + 2 };
                HBRUSH labelBrush = CreateSolidBrush(RGB(255, 255, 255));
                FillRect(hdcMem, &labelBackground, labelBrush);
                DeleteObject(labelBrush);
                TextOutW(hdcMem, textX, textY, percentageText.c_str(), static_cast<int>(percentageText.size()));

                SetBkMode(hdcMem, oldBackgroundMode);
                SetTextColor(hdcMem, oldTextColor);
                SelectObject(hdcMem, oldFont);
            }
            RECT rcRowTooltip = { 0, std::max(rowTop, topOffset), clientW, std::min(rowTop + 18, listBottom) };
            if (rcRowTooltip.top < rcRowTooltip.bottom) {
                barToolRegions.push_back({ rcRowTooltip,
                    BuildWorkTooltip(entry.seconds - entry.manualSeconds, entry.manualSeconds) });
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

            COLORREF cardBg = isBeingDragged ? th.bgCardSelected
                : (isAssignedToOther ? (g_themeMode == ThemeMode::Pink ? RGB(108, 0, 54)
                    : (g_darkMode ? RGB(30, 58, 90) : RGB(219, 234, 254)))
                    : (g_themeMode == ThemeMode::Pink ? RGB(183, 0, 99)
                        : (isSelected ? th.bgCardSelected : (isHovered ? th.bgCardHover : th.bgCard))));
            COLORREF cardBorder = isBeingDragged ? th.borderCardSelected
                : (isAssignedToOther
                    ? (g_themeMode == ThemeMode::Pink ? RGB(244, 114, 182)
                        : (g_darkMode ? RGB(59, 130, 246) : RGB(147, 197, 253)))
                    : (isSelected && g_themeMode == ThemeMode::Pink ? RGB(108, 0, 54)
                        : (isSelected ? th.borderCardSelected : (isHovered ? th.borderCardHover : th.borderCard))));

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
                HBRUSH hChkBrush = CreateSolidBrush(chkHovered ? (g_darkMode ? RGB(55, 65, 81) : th.bgCardHover) : th.bgCard);
                HPEN hChkPen = CreatePen(PS_SOLID, chkHovered ? 2 : 1, chkHovered ? th.checkActiveHover : th.checkActiveBorder);
                SelectObject(hdcMem, hChkBrush);
                SelectObject(hdcMem, hChkPen);
                RoundRect(hdcMem, rcChk.left, rcChk.top, rcChk.right, rcChk.bottom, 6, 6);
                DeleteObject(hChkBrush);
                DeleteObject(hChkPen);
            }

            bool isInlineEditingThis = (g_editingTaskId == item.task.id);
            int textRight = item.markerRect.right > 0 ? (item.markerRect.left - 6)
                : (item.assignRect.right > 0 ? (item.assignRect.left - 6)
                    : (item.optionsRect.right > 0 ? (item.optionsRect.left - 6) : (item.deleteRect.left - 6)));
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
                COLORREF cloudCol = item.task.completed ? th.textCompleted : th.borderCardSelected;
                DrawCloudIcon(hdcMem, rcCloud, cloudCol, true);
            }

            if (!isInlineEditingThis) {
                if (item.task.completed) {
                    SelectObject(hdcMem, g_hFontNormalStrike);
                    SetTextColor(hdcMem, g_themeMode == ThemeMode::Pink
                        ? RGB(255, 211, 235) : th.textCompleted);
                } else {
                    SelectObject(hdcMem, g_hFontNormal);
                    SetTextColor(hdcMem, g_themeMode == ThemeMode::Pink
                        ? RGB(255, 255, 255) : th.textPrimary);
                }
                DrawTextW(hdcMem, item.task.text.c_str(), -1, &rcText, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

                SelectObject(hdcMem, g_hFontSmall);
                SetTextColor(hdcMem, g_themeMode == ThemeMode::Pink
                    ? RGB(255, 230, 242) : th.textSecondary);
                std::wstring meta;
                if (item.task.completed) {
                    meta = L"Kész: " + TaskUtils::FormatDateTime(item.task.completed_at);
                } else {
                    meta = TaskUtils::FormatDateTime(item.task.created_at);
                }
                if (item.task.is_sync && !item.task.author.empty()) {
                    meta += L" • " + item.task.author;
                }
                if (item.task.is_sync && !item.task.assignee.empty()) {
                    meta += L" (" + item.task.assignee + L")";
                }
                DrawTextW(hdcMem, meta.c_str(), -1, &rcMeta, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }

            if (!item.task.is_sync && item.markerRect.right > 0) {
                RECT rcMarker = item.markerRect;
                rcMarker.top -= g_scrollY;
                rcMarker.bottom -= g_scrollY;
                bool markerHovered = isHovered && g_hoverButtonType == 5;
                COLORREF markerBg = g_themeMode == ThemeMode::Pink ? RGB(183, 0, 99)
                    : (markerHovered ? th.bgCardSelected : th.bgCard);
                COLORREF markerBorder = g_themeMode == ThemeMode::Pink ? RGB(153, 0, 82)
                    : (markerHovered ? th.borderCardHover : th.borderCard);
                DrawTaskMarker(hdcMem, rcMarker, item.task.marker, markerBg, markerBorder);
            }

            if (item.optionsRect.right > 0) {
                RECT rcOptions = item.optionsRect;
                rcOptions.top -= g_scrollY;
                rcOptions.bottom -= g_scrollY;
                bool optionsHovered = isHovered && g_hoverButtonType == 6;
                if (optionsHovered) {
                    HBRUSH buttonBrush = CreateSolidBrush(th.bgCardSelected);
                    HPEN buttonPen = CreatePen(PS_SOLID, 1, th.borderCardHover);
                    SelectObject(hdcMem, buttonBrush);
                    SelectObject(hdcMem, buttonPen);
                    RoundRect(hdcMem, rcOptions.left, rcOptions.top, rcOptions.right, rcOptions.bottom, 4, 4);
                    DeleteObject(buttonBrush);
                    DeleteObject(buttonPen);
                }
                COLORREF iconColor = optionsHovered ? th.borderCardSelected : th.textSecondary;
                DrawMoreIcon(hdcMem, rcOptions, iconColor);
            }

            if (item.assignRect.right > 0) {
                RECT rcAssign = item.assignRect;
                rcAssign.top -= g_scrollY;
                rcAssign.bottom -= g_scrollY;
                bool isAssignedToMe = item.task.assignee == CurrentUserName();
                bool isUnavailable = IsAssignedToOtherUser(item.task);
                bool isAssignHovered = !isUnavailable && (isHovered && g_hoverButtonType == 4);
                COLORREF assignBg = isUnavailable ? (g_darkMode ? RGB(55, 65, 81) : RGB(226, 232, 240))
                    : (isAssignedToMe ? RGB(220, 38, 38) : th.bgCard);
                COLORREF assignBorder = isUnavailable ? (g_darkMode ? RGB(75, 85, 99) : RGB(203, 213, 225))
                    : (isAssignedToMe ? RGB(220, 38, 38)
                        : (isAssignHovered ? th.borderCardHover : th.borderCard));
                HBRUSH hAssignBrush = CreateSolidBrush(assignBg);
                HPEN hAssignPen = CreatePen(PS_SOLID, 1, assignBorder);
                SelectObject(hdcMem, hAssignBrush);
                SelectObject(hdcMem, hAssignPen);
                RoundRect(hdcMem, rcAssign.left, rcAssign.top, rcAssign.right, rcAssign.bottom, 5, 5);
                DeleteObject(hAssignBrush);
                DeleteObject(hAssignPen);
                SelectObject(hdcMem, g_hFontSmall);
                SetTextColor(hdcMem, isUnavailable ? (g_darkMode ? RGB(156, 163, 175) : RGB(100, 116, 139))
                    : (isAssignedToMe ? RGB(255, 255, 255) : th.textSecondary));
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
                HPEN hPenLine = CreatePen(PS_SOLID, 2, th.borderCardSelected);
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
    COLORREF bgBottom = isHistory ? (g_themeMode == ThemeMode::Pink ? th.bgHeader : RGB(8, 25, 54))
        : (g_darkMode ? RGB(18, 18, 22) : th.bgSubBar);
    HBRUSH hBrBottom = CreateSolidBrush(bgBottom);
    FillRect(hdcMem, &rcBottom, hBrBottom);
    DeleteObject(hBrBottom);

    HPEN hPenBotBorder = CreatePen(PS_SOLID, 1, th.borderSep);
    SelectObject(hdcMem, hPenBotBorder);
    MoveToEx(hdcMem, 0, rcBottom.top, nullptr);
    LineTo(hdcMem, clientW, rcBottom.top);
    DeleteObject(hPenBotBorder);

    RECT rcTrack = { 10, rcBottom.top + 3, clientW - 10, rcBottom.bottom - 3 };
    COLORREF borderTrack = isHistory ? (g_themeMode == ThemeMode::Pink ? th.bgCardHover : RGB(104, 139, 181))
        : (g_darkMode ? RGB(255, 255, 255) : th.textSecondary);
    COLORREF bgTrack = isHistory ? (g_themeMode == ThemeMode::Pink ? th.bgCardSelected : RGB(16, 43, 78))
        : (g_darkMode ? RGB(28, 28, 34) : th.borderSep);

    HBRUSH hBrTrack = CreateSolidBrush(bgTrack);
    HPEN hPenTrack = CreatePen(PS_SOLID, 1, borderTrack);
    SelectObject(hdcMem, hBrTrack);
    SelectObject(hdcMem, hPenTrack);
    RoundRect(hdcMem, rcTrack.left, rcTrack.top, rcTrack.right, rcTrack.bottom, 4, 4);
    DeleteObject(hBrTrack);
    DeleteObject(hPenTrack);

    int workSec = g_store.work_seconds_today;
    int manualSec = CurrentManualWorkSeconds();
    int totalWorkSec = workSec + manualSec;
    const int targetWorkSec = DAILY_WORK_TARGET_SECONDS;
    RECT rcFill = { rcTrack.left + 1, rcTrack.top + 1, rcTrack.right - 1, rcTrack.bottom - 1 };
    COLORREF measuredColor = g_isWorkActive ? RGB(59, 130, 246) : RGB(71, 85, 105);
    DrawBarSegments(hdcMem, rcFill, workSec, manualSec, targetWorkSec, measuredColor, RGB(245, 158, 11));
    barToolRegions.push_back({ rcTrack, BuildWorkTooltip(workSec, manualSec) });

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
    SetTextColor(hdcMem, isHistory ? RGB(235, 242, 252) : th.textPrimary);

    std::wstring workStr = TaskUtils::FormatWorkDuration(totalWorkSec);
    double pct = ((double)totalWorkSec / (double)targetWorkSec) * 100.0;
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

    BarTooltips::Update(hWnd, barToolRegions);
    BitBlt(hdc, 0, 0, clientW, clientH, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
}
