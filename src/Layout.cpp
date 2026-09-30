#include "Layout.h"
#include "AppState.h"
#include "InlineEdit.h"
#include <algorithm>
#include <set>

static int MeasureTaskTextHeight(HDC hdc, const std::wstring& text, int width) {
    RECT rc = { 0, 0, width, 0 };
    DrawTextW(hdc, text.c_str(), -1, &rc, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
    return std::max(20, (int)(rc.bottom - rc.top));
}

void EnsureVisible(int itemIndex) {
    if (itemIndex < 0 || itemIndex >= (int)g_displayItems.size()) return;
    RECT rcClient;
    GetClientRect(g_hWnd, &rcClient);
    int topOffset = g_viewMode == ViewMode::ActiveTasks ? 94 : (g_viewMode == ViewMode::WorkHistory ? 48 : 54);
    int listBottom = rcClient.bottom - BOTTOM_BAR_HEIGHT;
    int viewableHeight = listBottom - topOffset;

    const auto& item = g_displayItems[itemIndex];
    if (item.rect.top - g_scrollY < topOffset) {
        g_scrollY = item.rect.top - topOffset;
    } else if (item.rect.bottom - g_scrollY > listBottom) {
        g_scrollY = item.rect.bottom - listBottom + 4;
    }

    int maxScroll = std::max(0, g_totalContentHeight - viewableHeight);
    if (g_scrollY < 0) g_scrollY = 0;
    if (g_scrollY > maxScroll) g_scrollY = maxScroll;
}

ScrollbarMetrics GetScrollbarMetrics() {
    ScrollbarMetrics m{};
    m.visible = false;
    if (g_isMiniMode) return m;

    RECT rcClient;
    GetClientRect(g_hWnd, &rcClient);
    int clientW = rcClient.right - rcClient.left;
    int clientH = rcClient.bottom - rcClient.top;

    int topOffset = g_viewMode == ViewMode::ActiveTasks ? 94 : (g_viewMode == ViewMode::WorkHistory ? 48 : 54);
    int listBottom = clientH - BOTTOM_BAR_HEIGHT;
    int viewableHeight = listBottom - topOffset;

    m.maxScroll = std::max(0, g_totalContentHeight - viewableHeight);
    if (m.maxScroll <= 0 || viewableHeight <= 0) {
        return m;
    }

    m.visible = true;

    int scrollbarWidth = 5;
    int scrollbarRight = clientW - 3;
    int scrollbarLeft = scrollbarRight - scrollbarWidth;

    int trackTop = topOffset + 2;
    int trackBottom = listBottom - 2;
    int trackH = trackBottom - trackTop;

    m.rcTrack = { scrollbarLeft, trackTop, scrollbarRight, trackBottom };

    if (trackH <= 10) {
        m.rcThumb = m.rcTrack;
        return m;
    }

    double ratio = (double)viewableHeight / (double)(g_totalContentHeight > 0 ? g_totalContentHeight : viewableHeight);
    int thumbH = (int)(trackH * ratio);
    if (thumbH < 24) thumbH = 24;
    if (thumbH > trackH) thumbH = trackH;

    int travel = trackH - thumbH;
    int thumbY = trackTop;
    if (m.maxScroll > 0 && travel > 0) {
        thumbY = trackTop + (int)((double)g_scrollY / (double)m.maxScroll * travel);
        if (thumbY + thumbH > trackBottom) {
            thumbY = trackBottom - thumbH;
        }
        if (thumbY < trackTop) {
            thumbY = trackTop;
        }
    }

    m.rcThumb = { scrollbarLeft, thumbY, scrollbarRight, thumbY + thumbH };
    return m;
}

RECT GetMiniDragHandleRect(int clientWidth) {
    return { clientWidth - 32, 2, clientWidth - 4, 24 };
}

RECT GetMiniFocusButtonRect(int clientWidth) {
    return { clientWidth - 66, 2, clientWidth - 34, 24 };
}

void UpdateControlsVisibility() {
    if (g_isMiniMode) {
        ShowWindow(g_hEdit, SW_HIDE);
        ShowWindow(g_hAddBtn, SW_HIDE);
        ShowWindow(g_hSyncToggleBtn, SW_HIDE);
        ShowWindow(g_hSettingsBtn, SW_HIDE);
        ShowWindow(g_hToggleViewBtn, SW_HIDE);
        ShowWindow(g_hTimeHistoryBtn, SW_HIDE);
        ShowWindow(g_hFocusModeBtn, SW_HIDE);
        ShowWindow(g_hManualWorkBtn, SW_HIDE);
        ShowWindow(g_hCloseBtn, SW_HIDE);
        ShowWindow(g_hPinBtn, SW_HIDE);
        return;
    }

    ShowWindow(g_hCloseBtn, SW_SHOW);
    ShowWindow(g_hPinBtn, SW_SHOW);
    ShowWindow(g_hFocusModeBtn, SW_SHOW);
    ShowWindow(g_hSettingsBtn, SW_SHOW);
    ShowWindow(g_hToggleViewBtn, SW_SHOW);

    bool isActive = (g_viewMode == ViewMode::ActiveTasks);
    bool isHistory = (g_viewMode == ViewMode::WorkHistory);
    ShowWindow(g_hTimeHistoryBtn, isHistory ? SW_HIDE : SW_SHOW);
    SetWindowPos(g_hToggleViewBtn, nullptr, isHistory ? 286 : 345, 10,
        isHistory ? 104 : 125, 28, SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(g_hManualWorkBtn, isHistory ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEdit, isActive ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hAddBtn, isActive ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hSyncToggleBtn, isActive ? SW_SHOW : SW_HIDE);
    if (isHistory) {
        SetWindowTextW(g_hToggleViewBtn, L"← Feladatok");
        SetWindowTextW(g_hTimeHistoryBtn, L"● Idők");
    } else if (isActive) {
        SetWindowTextW(g_hTimeHistoryBtn, L"Idők");
        size_t count = g_store.GetRecentCompletedTasks().size();
        std::wstring label = L"Elkészült (" + std::to_wstring(count) + L")";
        SetWindowTextW(g_hToggleViewBtn, label.c_str());
    } else {
        SetWindowTextW(g_hTimeHistoryBtn, L"Idők");
        SetWindowTextW(g_hToggleViewBtn, L"← Aktívak");
    }
}

void RecalculateMiniLayout() {
    g_displayItems.clear();
    int miniW = g_fullWinW / 2;
    if (miniW < 260) miniW = 292;

    int curY = 26;
    const int itemH = 22;
    const int spacing = 4;
    const time_t now = time(nullptr);
    const auto shouldShowTask = [now](const Task& task) {
        return !task.completed || (task.completed_at > 0 && now - task.completed_at < 20);
    };

    std::vector<Task> orderedActiveTasks;
    std::set<int> seenIds;
    for (int id : g_sessionActiveTaskIds) {
        for (const auto& t : g_store.tasks) {
            if (t.id == id && shouldShowTask(t)) {
                seenIds.insert(id);
                orderedActiveTasks.push_back(t);
                break;
            }
        }
    }
    for (const auto& t : g_store.tasks) {
        if (shouldShowTask(t) && seenIds.find(t.id) == seenIds.end()) {
            if (!t.completed) g_sessionActiveTaskIds.push_back(t.id);
            orderedActiveTasks.push_back(t);
        }
    }

    int activeCount = 0;
    for (const auto& t : orderedActiveTasks) {
        activeCount++;
        DisplayItem item;
        item.isHeader = false;
        item.task = t;
        item.rect = { 6, curY, miniW - 6, curY + itemH };
        item.checkRect = { 14, curY + (itemH - 20) / 2, 14 + 20, curY + (itemH + 20) / 2 };
        item.editRect = { 0, 0, 0, 0 };
        item.deleteRect = { 0, 0, 0, 0 };
        if (t.is_sync) {
            item.assignRect = { miniW - 46, curY + (itemH - 20) / 2, miniW - 10, curY + (itemH + 20) / 2 };
        }
        int textLeft = item.checkRect.right + 8 + (t.is_sync ? 18 : 0);
        int textRight = t.is_sync ? item.assignRect.left - 6 : miniW - 12;
        item.textRect = { textLeft, curY + 1, textRight, curY + itemH - 1 };
        g_displayItems.push_back(item);
        curY += itemH + spacing;
    }

    int totalH = 0;
    if (activeCount == 0) {
        totalH = 70 + 3;
    } else {
        totalH = curY - spacing + 6 + 3;
    }

    RECT rcWork;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);
    int maxH = (rcWork.bottom - rcWork.top) - 30;
    if (totalH > maxH) totalH = maxH;

    int x = g_miniPositionValid ? g_miniWinX : rcWork.right - miniW - 12;
    int y = g_miniPositionValid ? g_miniWinY : rcWork.bottom - totalH - 12;
    if (y < rcWork.top) y = rcWork.top;
    if (x < rcWork.left) x = rcWork.left;
    if (x + miniW > rcWork.right) x = rcWork.right - miniW;
    if (y + totalH > rcWork.bottom) y = rcWork.bottom - totalH;

    g_miniWinX = x;
    g_miniWinY = y;
    g_miniPositionValid = true;

    RECT rcDragHandle = GetMiniDragHandleRect(miniW);
    HRGN hMiniRegion = CreateRectRgn(0, rcDragHandle.bottom - 4, miniW, totalH);
    HRGN hHandleRegion = CreateRectRgn(rcDragHandle.left, rcDragHandle.top,
        rcDragHandle.right, rcDragHandle.bottom);
    CombineRgn(hMiniRegion, hMiniRegion, hHandleRegion, RGN_OR);
    DeleteObject(hHandleRegion);
    RECT rcFocusButton = GetMiniFocusButtonRect(miniW);
    HRGN hFocusButtonRegion = CreateRectRgn(rcFocusButton.left, rcFocusButton.top,
        rcFocusButton.right, rcFocusButton.bottom);
    CombineRgn(hMiniRegion, hMiniRegion, hFocusButtonRegion, RGN_OR);
    DeleteObject(hFocusButtonRegion);
    if (!SetWindowRgn(g_hWnd, hMiniRegion, TRUE)) {
        DeleteObject(hMiniRegion);
    }

    SetWindowPos(g_hWnd, HWND_TOPMOST, x, y, miniW, totalH, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    InvalidateRect(g_hWnd, nullptr, TRUE);
}

void EnterMiniMode() {
    if (g_isMiniMode) return;

    CommitInlineEdit();

    RECT rcWork;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);
    int defaultW = 585;
    int defaultH = 550;
    int defaultX = rcWork.right - defaultW - 12;
    int defaultY = rcWork.bottom - defaultH - 12;

    g_fullWinX = defaultX;
    g_fullWinY = defaultY;
    g_fullWinW = defaultW;
    g_fullWinH = defaultH;

    g_isMiniMode = true;
    g_scrollbarHovered = false;
    g_scrollbarDragging = false;

    UpdateControlsVisibility();

    LONG_PTR exStyle = GetWindowLongPtrW(g_hWnd, GWL_EXSTYLE);
    if (!(exStyle & WS_EX_LAYERED)) {
        SetWindowLongPtrW(g_hWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
    }
    BYTE miniOpacity = (BYTE)(255 * 0.80);
    SetLayeredWindowAttributes(g_hWnd, 0, miniOpacity, LWA_ALPHA);

    RecalculateMiniLayout();
}

void ExitMiniMode(bool toHidden) {
    if (!g_isMiniMode) return;
    if (!toHidden && g_pinMode) {
        RECT rc;
        if (GetWindowRect(g_hWnd, &rc)) {
            g_miniWinX = rc.left;
            g_miniWinY = rc.top;
            g_miniPositionValid = true;
        }
    }
    g_isMiniMode = false;
    SetWindowRgn(g_hWnd, nullptr, TRUE);

    SetLayeredWindowAttributes(g_hWnd, 0, 255, LWA_ALPHA);
    LONG_PTR exStyle = GetWindowLongPtrW(g_hWnd, GWL_EXSTYLE);
    SetWindowLongPtrW(g_hWnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);

    if (toHidden) {
        ShowWindow(g_hWnd, SW_HIDE);
        return;
    }

    SetWindowPos(g_hWnd, HWND_TOPMOST, g_fullWinX, g_fullWinY, g_fullWinW, g_fullWinH, SWP_SHOWWINDOW);
    UpdateControlsVisibility();
    RecalculateLayout();
    if (g_hPinBtn) InvalidateRect(g_hPinBtn, nullptr, TRUE);
    if (g_hSettingsBtn) InvalidateRect(g_hSettingsBtn, nullptr, TRUE);
    if (g_hSyncToggleBtn) InvalidateRect(g_hSyncToggleBtn, nullptr, TRUE);
    InvalidateRect(g_hWnd, nullptr, TRUE);
}

void RecalculateLayout() {
    if (g_isMiniMode) {
        RecalculateMiniLayout();
        return;
    }
    g_displayItems.clear();
    RECT rcClient;
    GetClientRect(g_hWnd, &rcClient);
    int clientWidth = rcClient.right - rcClient.left;

    int topOffset = g_viewMode == ViewMode::ActiveTasks ? 94 : (g_viewMode == ViewMode::WorkHistory ? 48 : 54);

    if (g_viewMode == ViewMode::WorkHistory) {
        auto entries = g_store.GetWorkHistory();
        int weeklySpacing = 0;
        for (const auto& entry : entries) {
            if (entry.isWeeklySummary) weeklySpacing += 12;
        }
        g_totalContentHeight = 24 + (int)entries.size() * 18 + weeklySpacing;
        return;
    }

    int currentY = topOffset;
    const int itemHeight = 44;
    const int headerHeight = 28;
    HDC hdcMeasure = GetDC(g_hWnd);
    HFONT hOldFont = (HFONT)SelectObject(hdcMeasure, g_hFontNormal);

    if (g_viewMode == ViewMode::ActiveTasks) {
        std::set<int> seenIds;
        for (int id : g_sessionActiveTaskIds) {
            for (const auto& t : g_store.tasks) {
                if (t.id == id) {
                    seenIds.insert(id);
                    DisplayItem item;
                    item.isHeader = false;
                    item.task = t;
                    item.optionsRect = { clientWidth - 36, currentY + 10, clientWidth - 12, currentY + 34 };
                    if (!t.is_sync) {
                        item.markerRect = { clientWidth - 68, currentY + 8, clientWidth - 40, currentY + 36 };
                    }
                    if (t.is_sync) {
                        item.assignRect = { clientWidth - 82, currentY + 10, clientWidth - 40, currentY + 34 };
                    }
                    int textLeft = 18 + 20 + 10 + (t.is_sync ? 19 : 0);
                    int textRight = t.is_sync ? item.assignRect.left - 6
                        : item.markerRect.left - 6;
                    int textHeight = MeasureTaskTextHeight(hdcMeasure, t.text, textRight - textLeft);
                    int rowHeight = std::max(itemHeight, textHeight + 28);
                    item.rect = { 10, currentY, clientWidth - 10, currentY + rowHeight };
                    item.checkRect = { 18, currentY + (rowHeight - 20) / 2, 38, currentY + (rowHeight + 20) / 2 };
                    item.optionsRect.top = currentY + (rowHeight - 24) / 2;
                    item.optionsRect.bottom = item.optionsRect.top + 24;
                    if (item.markerRect.right > 0) {
                        item.markerRect.top = currentY + (rowHeight - 28) / 2;
                        item.markerRect.bottom = item.markerRect.top + 28;
                    }
                    item.deleteRect.top = currentY + (rowHeight - 24) / 2;
                    item.deleteRect.bottom = item.deleteRect.top + 24;
                    item.textRect = { textLeft, currentY + 5, textRight, currentY + 5 + textHeight };

                    g_displayItems.push_back(item);
                    currentY += rowHeight + 6;
                    break;
                }
            }
        }
        for (const auto& t : g_store.tasks) {
            if (!t.completed && seenIds.find(t.id) == seenIds.end()) {
                g_sessionActiveTaskIds.push_back(t.id);
                DisplayItem item;
                item.isHeader = false;
                item.task = t;
                item.optionsRect = { clientWidth - 36, currentY + 10, clientWidth - 12, currentY + 34 };
                if (!t.is_sync) {
                    item.markerRect = { clientWidth - 68, currentY + 8, clientWidth - 40, currentY + 36 };
                }
                if (t.is_sync) {
                    item.assignRect = { clientWidth - 82, currentY + 10, clientWidth - 40, currentY + 34 };
                }
                int textLeft = 18 + 20 + 10 + (t.is_sync ? 19 : 0);
                int textRight = t.is_sync ? item.assignRect.left - 6
                    : item.markerRect.left - 6;
                int textHeight = MeasureTaskTextHeight(hdcMeasure, t.text, textRight - textLeft);
                int rowHeight = std::max(itemHeight, textHeight + 28);
                item.rect = { 10, currentY, clientWidth - 10, currentY + rowHeight };
                item.checkRect = { 18, currentY + (rowHeight - 20) / 2, 38, currentY + (rowHeight + 20) / 2 };
                item.optionsRect.top = currentY + (rowHeight - 24) / 2;
                item.optionsRect.bottom = item.optionsRect.top + 24;
                if (item.markerRect.right > 0) {
                    item.markerRect.top = currentY + (rowHeight - 28) / 2;
                    item.markerRect.bottom = item.markerRect.top + 28;
                }
                item.deleteRect.top = currentY + (rowHeight - 24) / 2;
                item.deleteRect.bottom = item.deleteRect.top + 24;
                item.textRect = { textLeft, currentY + 5, textRight, currentY + 5 + textHeight };

                g_displayItems.push_back(item);
                currentY += rowHeight + 6;
            }
        }
    } else {
        auto compTasks = g_store.GetAllCompletedTasks();
        std::wstring lastDateStr = L"";
        time_t now = time(nullptr);
        std::wstring todayStr = TaskUtils::FormatDate(now);
        std::wstring yestStr = TaskUtils::FormatDate(now - 86400);

        for (const auto& t : compTasks) {
            std::wstring dateStr = TaskUtils::FormatDate(t.completed_at);
            if (dateStr != lastDateStr) {
                lastDateStr = dateStr;
                DisplayItem headerItem;
                headerItem.isHeader = true;
                std::wstring label = dateStr;
                if (dateStr == todayStr) label += L" (Ma)";
                else if (dateStr == yestStr) label += L" (Tegnap)";
                headerItem.headerText = label;
                headerItem.rect = { 10, currentY, clientWidth - 10, currentY + headerHeight };
                g_displayItems.push_back(headerItem);
                currentY += headerHeight + 4;
            }

            DisplayItem item;
            item.isHeader = false;
            item.task = t;
            item.optionsRect = { clientWidth - 36, currentY + 10, clientWidth - 12, currentY + 34 };
            if (t.is_sync) {
                item.assignRect = { clientWidth - 82, currentY + 10, clientWidth - 40, currentY + 34 };
            }
            int textLeft = 18 + 20 + 10 + (t.is_sync ? 19 : 0);
            int textRight = t.is_sync ? item.assignRect.left - 6 : item.optionsRect.left - 6;
            int textHeight = MeasureTaskTextHeight(hdcMeasure, t.text, textRight - textLeft);
            int rowHeight = std::max(itemHeight, textHeight + 28);
            item.rect = { 10, currentY, clientWidth - 10, currentY + rowHeight };
            item.checkRect = { 18, currentY + (rowHeight - 20) / 2, 38, currentY + (rowHeight + 20) / 2 };
            item.optionsRect.top = currentY + (rowHeight - 24) / 2;
            item.optionsRect.bottom = item.optionsRect.top + 24;
            item.textRect = { textLeft, currentY + 5, textRight, currentY + 5 + textHeight };

            g_displayItems.push_back(item);
            currentY += rowHeight + 6;
        }
    }

    SelectObject(hdcMeasure, hOldFont);
    ReleaseDC(g_hWnd, hdcMeasure);
    int listContentHeight = currentY - topOffset;
    g_totalContentHeight = g_displayItems.empty() ? 0 : (listContentHeight + 10);
}
