#include "UIEventHandler.h"
#include "AppState.h"
#include "Drawing.h"
#include "Layout.h"
#include "InlineEdit.h"
#include "MainWindow.h"
#include "WorkTimeDialog.h"
#include <windowsx.h>
#include <commdlg.h>
#include <algorithm>

namespace {
HWND g_messageBoxOwner = nullptr;

LRESULT CALLBACK CenterMessageBoxHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HCBT_ACTIVATE) {
        HWND hDialog = (HWND)wParam;
        wchar_t className[16]{};
        if (GetClassNameW(hDialog, className, _countof(className)) &&
            wcscmp(className, L"#32770") == 0 && g_messageBoxOwner) {
            RECT ownerRect{};
            RECT dialogRect{};
            if (GetWindowRect(g_messageBoxOwner, &ownerRect) && GetWindowRect(hDialog, &dialogRect)) {
                int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - (dialogRect.right - dialogRect.left)) / 2;
                int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - (dialogRect.bottom - dialogRect.top)) / 2;
                SetWindowPos(hDialog, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}
}

bool HandleKeyDown(HWND hWnd, WPARAM wParam) {
    if (wParam == VK_ESCAPE) {
        if (g_pinMode && !g_isMiniMode) {
            EnterMiniMode();
        } else if (!g_pinMode) {
            HideAppWindow();
        }
        return true;
    } else if (wParam == VK_TAB) {
        bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (isShift) {
            int prevIdx = g_selectedIndex - 1;
            while (prevIdx >= 0 && g_displayItems[prevIdx].isHeader) prevIdx--;
            if (prevIdx < 0) {
                g_selectedIndex = -1;
                if (g_viewMode == ViewMode::ActiveTasks && g_hEdit) {
                    SetFocus(g_hEdit);
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
            } else {
                g_selectedIndex = prevIdx;
                EnsureVisible(g_selectedIndex);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
        } else {
            int nextIdx = g_selectedIndex + 1;
            while (nextIdx < (int)g_displayItems.size() && g_displayItems[nextIdx].isHeader) nextIdx++;
            if (nextIdx >= (int)g_displayItems.size()) {
                g_selectedIndex = -1;
                if (g_viewMode == ViewMode::ActiveTasks && g_hEdit) {
                    SetFocus(g_hEdit);
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
            } else {
                g_selectedIndex = nextIdx;
                EnsureVisible(g_selectedIndex);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
        }
        return true;
    } else if (wParam == VK_UP) {
        int prevIdx = g_selectedIndex - 1;
        while (prevIdx >= 0 && g_displayItems[prevIdx].isHeader) prevIdx--;
        if (prevIdx >= 0) {
            g_selectedIndex = prevIdx;
            EnsureVisible(g_selectedIndex);
            InvalidateRect(hWnd, nullptr, FALSE);
        } else if (g_viewMode == ViewMode::ActiveTasks && g_hEdit) {
            g_selectedIndex = -1;
            SetFocus(g_hEdit);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return true;
    } else if (wParam == VK_DOWN) {
        int nextIdx = g_selectedIndex + 1;
        while (nextIdx < (int)g_displayItems.size() && g_displayItems[nextIdx].isHeader) nextIdx++;
        if (nextIdx < (int)g_displayItems.size()) {
            g_selectedIndex = nextIdx;
            EnsureVisible(g_selectedIndex);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return true;
    } else if (wParam == VK_SPACE) {
        if (g_selectedIndex >= 0 && g_selectedIndex < (int)g_displayItems.size()) {
            const auto& item = g_displayItems[g_selectedIndex];
            if (!item.isHeader) {
                g_store.ToggleCompleted(item.task.id);
                UpdateControlsVisibility();
                RecalculateLayout();
                InvalidateRect(hWnd, nullptr, TRUE);
            }
        }
        return true;
    } else if (wParam == VK_RETURN) {
        if (g_viewMode == ViewMode::ActiveTasks && g_selectedIndex >= 0 && g_selectedIndex < (int)g_displayItems.size()) {
            StartInlineEdit(g_selectedIndex);
        }
        return true;
    } else if (wParam == VK_DELETE) {
        if (g_selectedIndex >= 0 && g_selectedIndex < (int)g_displayItems.size()) {
            const auto& item = g_displayItems[g_selectedIndex];
            if (!item.isHeader) {
                int delId = item.task.id;
                g_store.Delete(delId);

                auto it = std::find(g_sessionActiveTaskIds.begin(), g_sessionActiveTaskIds.end(), delId);
                if (it != g_sessionActiveTaskIds.end()) {
                    g_sessionActiveTaskIds.erase(it);
                }

                UpdateControlsVisibility();
                RecalculateLayout();
                if (g_selectedIndex >= (int)g_displayItems.size()) {
                    g_selectedIndex = (int)g_displayItems.size() - 1;
                }
                InvalidateRect(hWnd, nullptr, TRUE);
            }
        }
        return true;
    }
    return false;
}

bool HandleLButtonDown(HWND hWnd, LPARAM lParam) {
    if (g_isMiniMode) {
        int mx = GET_X_LPARAM(lParam);
        int my = GET_Y_LPARAM(lParam);
        RECT rcClient;
        GetClientRect(hWnd, &rcClient);
        RECT rcDragHandle = GetMiniDragHandleRect(rcClient.right);
        if (g_pinMode && PtInRect(&rcDragHandle, { mx, my })) {
            ReleaseCapture();
            SendMessageW(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return true;
        }

        for (size_t i = 0; i < g_displayItems.size(); ++i) {
            const auto& item = g_displayItems[i];
            if (item.assignRect.right > 0 && PtInRect(&item.assignRect, { mx, my })) {
                g_store.ToggleAssignment(item.task.id, TaskUtils::GetCleanUserName());
                RecalculateMiniLayout();
                InvalidateRect(hWnd, nullptr, FALSE);
                return true;
            } else if (PtInRect(&item.checkRect, { mx, my })) {
                g_store.ToggleCompleted(item.task.id);
                g_store.Save();
                RecalculateMiniLayout();
                return true;
            } else if (PtInRect(&item.rect, { mx, my })) {
                g_dragPotentialSourceIndex = (int)i;
                g_dragStartMouseY = my;
                SetCapture(hWnd);
                return true;
            }
        }

        ReleaseCapture();
        SendMessageW(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return true;
    }

    int clickRawY = GET_Y_LPARAM(lParam);
    if (clickRawY < 48) {
        CommitInlineEdit();
        ReleaseCapture();
        SendMessageW(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return true;
    }

    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    if (clickRawY >= rcClient.bottom - BOTTOM_BAR_HEIGHT) {
        CommitInlineEdit();
        return true;
    }

    int mx = GET_X_LPARAM(lParam);
    auto sm = GetScrollbarMetrics();
    if (sm.visible && mx >= sm.rcTrack.left - 3 && mx <= rcClient.right && clickRawY >= sm.rcTrack.top && clickRawY <= sm.rcTrack.bottom) {
        CommitInlineEdit();
        if (clickRawY >= sm.rcThumb.top && clickRawY <= sm.rcThumb.bottom) {
            g_scrollbarDragging = true;
            g_scrollDragStartY = clickRawY;
            g_scrollDragStartScrollY = g_scrollY;
            SetCapture(hWnd);
            InvalidateRect(hWnd, nullptr, FALSE);
            return true;
        } else if (clickRawY < sm.rcThumb.top) {
            int topOffset = g_viewMode == ViewMode::ActiveTasks ? 94 : (g_viewMode == ViewMode::WorkHistory ? 48 : 88);
            int viewableHeight = rcClient.bottom - topOffset - BOTTOM_BAR_HEIGHT;
            g_scrollY = std::max(0, g_scrollY - viewableHeight);
            UpdateInlineEditPos();
            InvalidateRect(hWnd, nullptr, TRUE);
            return true;
        } else {
            int topOffset = g_viewMode == ViewMode::ActiveTasks ? 94 : (g_viewMode == ViewMode::WorkHistory ? 48 : 88);
            int viewableHeight = rcClient.bottom - topOffset - BOTTOM_BAR_HEIGHT;
            g_scrollY = std::min(sm.maxScroll, g_scrollY + viewableHeight);
            UpdateInlineEditPos();
            InvalidateRect(hWnd, nullptr, TRUE);
            return true;
        }
    }

    int my = clickRawY + g_scrollY;

    for (size_t i = 0; i < g_displayItems.size(); ++i) {
        const auto& item = g_displayItems[i];
        if (item.isHeader) continue;

        if (item.assignRect.right > 0 && PtInRect(&item.assignRect, { mx, my })) {
            int taskId = item.task.id;
            CommitInlineEdit();
            g_store.ToggleAssignment(taskId, TaskUtils::GetCleanUserName());
            RecalculateLayout();
            SetFocus(hWnd);
            InvalidateRect(hWnd, nullptr, FALSE);
            return true;
        } else if (PtInRect(&item.checkRect, { mx, my })) {
            CommitInlineEdit();
            g_selectedIndex = (int)i;
            g_store.ToggleCompleted(item.task.id);
            UpdateControlsVisibility();
            RecalculateLayout();
            SetFocus(hWnd);
            InvalidateRect(hWnd, nullptr, TRUE);
            return true;
        } else if (item.editRect.right > 0 && PtInRect(&item.editRect, { mx, my })) {
            if (g_editingTaskId == item.task.id ||
                (g_lastCommittedTaskId == item.task.id && (GetTickCount64() - g_lastCommitTick < 250))) {
                CommitInlineEdit();
                g_lastCommittedTaskId = -1;
                return true;
            }
            StartInlineEdit((int)i);
            return true;
        } else if (PtInRect(&item.deleteRect, { mx, my })) {
            int delId = item.task.id;
            std::wstring taskName = item.task.text;
            if (g_editingTaskId == delId && g_hInlineEdit) {
                int textLength = GetWindowTextLengthW(g_hInlineEdit);
                std::vector<wchar_t> textBuffer(textLength + 1);
                GetWindowTextW(g_hInlineEdit, textBuffer.data(), textLength + 1);
                taskName = textBuffer.data();
            }

            std::wstring confirmText = L"Biztos törli ezt a feladatot?\n" + taskName;
            bool wasInContextMenu = g_inContextMenu;
            g_inContextMenu = true;
            g_messageBoxOwner = hWnd;
            HHOOK messageBoxHook = SetWindowsHookExW(WH_CBT, CenterMessageBoxHook, nullptr, GetCurrentThreadId());
            int answer = MessageBoxW(hWnd, confirmText.c_str(), L"Feladat törlése",
                MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
            if (messageBoxHook) UnhookWindowsHookEx(messageBoxHook);
            g_messageBoxOwner = nullptr;
            g_inContextMenu = wasInContextMenu;
            if (answer != IDYES) return true;

            CommitInlineEdit();
            g_store.Delete(delId);

            auto it = std::find(g_sessionActiveTaskIds.begin(), g_sessionActiveTaskIds.end(), delId);
            if (it != g_sessionActiveTaskIds.end()) {
                g_sessionActiveTaskIds.erase(it);
            }

            UpdateControlsVisibility();
            RecalculateLayout();
            SetFocus(hWnd);
            InvalidateRect(hWnd, nullptr, TRUE);
            return true;
        } else if (PtInRect(&item.rect, { mx, my })) {
            CommitInlineEdit();
            g_selectedIndex = (int)i;
            if (g_viewMode == ViewMode::ActiveTasks) {
                g_dragPotentialSourceIndex = (int)i;
                g_dragStartMouseY = clickRawY;
                SetCapture(hWnd);
            }
            SetFocus(hWnd);
            InvalidateRect(hWnd, nullptr, FALSE);
            return true;
        }
    }
    CommitInlineEdit();
    return true;
}

bool HandleMouseMove(HWND hWnd, LPARAM lParam) {
    int mx = GET_X_LPARAM(lParam);
    int rawY = GET_Y_LPARAM(lParam);

    if (g_dragPotentialSourceIndex != -1 && !g_taskDragging) {
        if (abs(rawY - g_dragStartMouseY) >= 5) {
            g_taskDragging = true;
            g_dragSourceIndex = g_dragPotentialSourceIndex;
            g_dragCurrentY = rawY;
            g_dragInsertIndex = g_dragSourceIndex;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
    }

    if (g_taskDragging) {
        g_dragCurrentY = rawY;
        int my = rawY + (g_isMiniMode ? 0 : g_scrollY);

        int bestInsertIdx = -1;
        for (size_t i = 0; i < g_displayItems.size(); ++i) {
            const auto& itm = g_displayItems[i];
            if (itm.isHeader) continue;
            int itemCenterY = (itm.rect.top + itm.rect.bottom) / 2;
            if (my < itemCenterY) {
                bestInsertIdx = (int)i;
                break;
            }
        }
        if (bestInsertIdx == -1) {
            bestInsertIdx = (int)g_displayItems.size();
        }
        if (bestInsertIdx != g_dragInsertIndex) {
            g_dragInsertIndex = bestInsertIdx;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return true;
    }

    if (g_scrollbarDragging) {
        auto sm = GetScrollbarMetrics();
        if (sm.visible) {
            int trackH = sm.rcTrack.bottom - sm.rcTrack.top;
            int thumbH = sm.rcThumb.bottom - sm.rcThumb.top;
            int travel = trackH - thumbH;
            if (travel > 0) {
                int dy = rawY - g_scrollDragStartY;
                int newScroll = g_scrollDragStartScrollY + (int)((double)dy / travel * sm.maxScroll);
                if (newScroll < 0) newScroll = 0;
                if (newScroll > sm.maxScroll) newScroll = sm.maxScroll;
                if (newScroll != g_scrollY) {
                    g_scrollY = newScroll;
                    UpdateInlineEditPos();
                    InvalidateRect(hWnd, nullptr, TRUE);
                }
            }
        }
        return true;
    }

    auto sm = GetScrollbarMetrics();
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int clientW = rcClient.right - rcClient.left;
    bool isSbHover = (sm.visible && mx >= sm.rcTrack.left - 3 && mx <= clientW && rawY >= sm.rcTrack.top && rawY <= sm.rcTrack.bottom);
    if (isSbHover != g_scrollbarHovered) {
        g_scrollbarHovered = isSbHover;
        InvalidateRect(hWnd, nullptr, FALSE);
    }

    int my = rawY + (g_isMiniMode ? 0 : g_scrollY);

    int newHoverIdx = -1;
    int newHoverBtn = 0;

    if (!isSbHover) {
        for (size_t i = 0; i < g_displayItems.size(); ++i) {
            const auto& item = g_displayItems[i];
            if (item.isHeader) continue;
            if (PtInRect(&item.rect, { mx, my })) {
                newHoverIdx = (int)i;
                if (item.assignRect.right > 0 && PtInRect(&item.assignRect, { mx, my })) newHoverBtn = 4;
                else if (PtInRect(&item.checkRect, { mx, my })) newHoverBtn = 1;
                else if (item.editRect.right > 0 && PtInRect(&item.editRect, { mx, my })) newHoverBtn = 2;
                else if (item.deleteRect.right > 0 && PtInRect(&item.deleteRect, { mx, my })) newHoverBtn = 3;
                break;
            }
        }
    }

    if (newHoverIdx != g_hoverItemIndex || newHoverBtn != g_hoverButtonType) {
        g_hoverItemIndex = newHoverIdx;
        g_hoverButtonType = newHoverBtn;
        InvalidateRect(hWnd, nullptr, FALSE);
    }
    return true;
}

bool HandleLButtonUp(HWND hWnd, LPARAM) {
    if (g_scrollbarDragging) {
        g_scrollbarDragging = false;
        ReleaseCapture();
        InvalidateRect(hWnd, nullptr, FALSE);
        return true;
    }

    if (g_taskDragging) {
        int srcIdx = g_dragSourceIndex;
        int insIdx = g_dragInsertIndex;

        g_taskDragging = false;
        g_dragSourceIndex = -1;
        g_dragInsertIndex = -1;
        g_dragPotentialSourceIndex = -1;
        ReleaseCapture();

        if (srcIdx >= 0 && srcIdx < (int)g_displayItems.size()) {
            int draggedTaskId = g_displayItems[srcIdx].task.id;

            int targetOrderIndex = 0;
            if (insIdx >= (int)g_displayItems.size()) {
                targetOrderIndex = (int)g_sessionActiveTaskIds.size();
            } else if (insIdx > 0) {
                int refTaskId = g_displayItems[insIdx].task.id;
                auto it = std::find(g_sessionActiveTaskIds.begin(), g_sessionActiveTaskIds.end(), refTaskId);
                if (it != g_sessionActiveTaskIds.end()) {
                    targetOrderIndex = (int)std::distance(g_sessionActiveTaskIds.begin(), it);
                }
            }

            auto itSrc = std::find(g_sessionActiveTaskIds.begin(), g_sessionActiveTaskIds.end(), draggedTaskId);
            if (itSrc != g_sessionActiveTaskIds.end()) {
                int oldOrderIndex = (int)std::distance(g_sessionActiveTaskIds.begin(), itSrc);
                g_sessionActiveTaskIds.erase(itSrc);
                if (targetOrderIndex > oldOrderIndex) {
                    targetOrderIndex--;
                }
                if (targetOrderIndex < 0) {
                    targetOrderIndex = 0;
                }
                if (targetOrderIndex > (int)g_sessionActiveTaskIds.size()) {
                    targetOrderIndex = (int)g_sessionActiveTaskIds.size();
                }
                g_sessionActiveTaskIds.insert(g_sessionActiveTaskIds.begin() + targetOrderIndex, draggedTaskId);

                g_store.active_order = g_sessionActiveTaskIds;
                g_store.SaveLocal();
            }
        }

        if (g_isMiniMode) {
            RecalculateMiniLayout();
        } else {
            RecalculateLayout();
        }
        InvalidateRect(hWnd, nullptr, FALSE);
        return true;
    }

    if (g_dragPotentialSourceIndex != -1) {
        int clickedIdx = g_dragPotentialSourceIndex;
        g_dragPotentialSourceIndex = -1;
        ReleaseCapture();

        if (g_isMiniMode && clickedIdx >= 0 && clickedIdx < (int)g_displayItems.size()) {
            int targetTaskId = g_displayItems[clickedIdx].task.id;
            g_viewMode = ViewMode::ActiveTasks;
            ExitMiniMode(false);
            SetForegroundWindow(g_hWnd);
            for (size_t j = 0; j < g_displayItems.size(); ++j) {
                if (!g_displayItems[j].isHeader && g_displayItems[j].task.id == targetTaskId) {
                    EnsureVisible((int)j);
                    StartInlineEdit((int)j);
                    break;
                }
            }
            return true;
        }
    }

    return false;
}

bool HandleCommand(HWND hWnd, int id) {
    if (id == IDC_CLOSE_BTN) {
        g_pinMode = false;
        g_miniPositionValid = false;
        if (g_hPinBtn) InvalidateRect(g_hPinBtn, nullptr, TRUE);
        HideAppWindow();
        return true;
    } else if (id == IDC_PIN_BTN) {
        g_pinMode = !g_pinMode;
        g_miniPositionValid = false;
        if (g_hPinBtn) InvalidateRect(g_hPinBtn, nullptr, TRUE);
        return true;
    } else if (id == IDC_SYNC_TOGGLE_BTN) {
        g_syncToggle = !g_syncToggle;
        if (g_hSyncToggleBtn) InvalidateRect(g_hSyncToggleBtn, nullptr, TRUE);
        SetFocus(g_hEdit);
        return true;
    } else if (id == IDC_SYNC_FOLDER_BTN) {
        wchar_t szFile[MAX_PATH] = { 0 };
        if (!g_store.syncFilePath.empty()) {
            wcsncpy_s(szFile, g_store.syncFilePath.c_str(), MAX_PATH - 1);
        } else {
            std::wstring defPath = TaskUtils::GetDefaultSyncFilePath();
            wcsncpy_s(szFile, defPath.c_str(), MAX_PATH - 1);
        }

        OPENFILENAMEW ofn = { 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hWnd;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
        ofn.lpstrFilter = L"JSON fájlok (*.json)\0*.json\0Minden fájl (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrTitle = L"Szinkronizált feladatok fájljának kiválasztása";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_ENABLESIZING | OFN_NOCHANGEDIR;

        std::wstring initDir;
        size_t lastSlash = g_store.syncFilePath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            initDir = g_store.syncFilePath.substr(0, lastSlash);
            ofn.lpstrInitialDir = initDir.c_str();
        }

        g_inContextMenu = true;
        BOOL ok = GetOpenFileNameW(&ofn);
        g_inContextMenu = false;

        if (ok) {
            g_store.SetSyncFilePath(szFile);
            UpdateControlsVisibility();
            RecalculateLayout();
            InvalidateRect(hWnd, nullptr, TRUE);
        }
        return true;
    } else if (id == IDC_TIME_HISTORY_BTN) {
        CommitInlineEdit();
        g_viewMode = ViewMode::WorkHistory;
        g_scrollY = 0;
        g_selectedIndex = -1;
        UpdateControlsVisibility();
        RecalculateLayout();
        InvalidateRect(hWnd, nullptr, TRUE);
        return true;
    } else if (id == IDC_MANUAL_WORK_BTN) {
        std::string date;
        int seconds = 0;
        if (ShowManualWorkDialog(hWnd, g_store.manual_work_history, date, seconds)) {
            g_store.SetManualWork(date, seconds);
            RecalculateLayout();
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return true;
    } else if (id == IDC_TOGGLE_VIEW_BTN) {
        CommitInlineEdit();
        if (g_viewMode == ViewMode::ActiveTasks) {
            g_viewMode = ViewMode::CompletedTasks;
            g_completedFilter = CompletedFilter::SinceYesterday930;
        } else {
            g_viewMode = ViewMode::ActiveTasks;
        }
        g_scrollY = 0;
        g_selectedIndex = -1;
        UpdateControlsVisibility();
        RecalculateLayout();
        InvalidateRect(hWnd, nullptr, TRUE);
        return true;
    } else if (id == IDC_FILTER_RECENT) {
        g_completedFilter = CompletedFilter::SinceYesterday930;
        g_scrollY = 0;
        g_selectedIndex = -1;
        UpdateControlsVisibility();
        RecalculateLayout();
        InvalidateRect(hWnd, nullptr, TRUE);
        return true;
    } else if (id == IDC_FILTER_ALL) {
        g_completedFilter = CompletedFilter::AllByDay;
        g_scrollY = 0;
        g_selectedIndex = -1;
        UpdateControlsVisibility();
        RecalculateLayout();
        InvalidateRect(hWnd, nullptr, TRUE);
        return true;
    } else if (id == IDC_ADD_TASK_BTN) {
        int len = GetWindowTextLengthW(g_hEdit);
        if (len > 0) {
            std::vector<wchar_t> buf(len + 1);
            GetWindowTextW(g_hEdit, buf.data(), len + 1);
            std::wstring text = buf.data();
            while (!text.empty() && iswspace(text.front())) text.erase(text.begin());
            while (!text.empty() && iswspace(text.back())) text.pop_back();

            if (!text.empty()) {
                g_store.Add(text, g_syncToggle);
                if (!g_store.tasks.empty()) {
                    int newTaskId = g_store.tasks.back().id;
                    g_sessionActiveTaskIds.insert(g_sessionActiveTaskIds.begin(), newTaskId);
                }
                if (g_syncToggle) {
                    g_syncToggle = false;
                    if (g_hSyncToggleBtn) InvalidateRect(g_hSyncToggleBtn, nullptr, TRUE);
                }
                SetWindowTextW(g_hEdit, L"");
                UpdateControlsVisibility();
                RecalculateLayout();
                InvalidateRect(hWnd, nullptr, TRUE);
            }
        }
        SetFocus(g_hEdit);
        return true;
    } else if (id == IDM_TRAY_OPEN) {
        if (g_isMiniMode) {
            ExitMiniMode(false);
        }
        g_viewMode = ViewMode::ActiveTasks;
        ShowAppWindow();
        return true;
    } else if (id == IDM_TRAY_COMPLETED) {
        if (g_isMiniMode) {
            ExitMiniMode(false);
        }
        g_viewMode = ViewMode::CompletedTasks;
        g_completedFilter = CompletedFilter::SinceYesterday930;
        ShowAppWindow();
        return true;
    } else if (id == IDM_TRAY_PIN) {
        g_pinMode = !g_pinMode;
        g_miniPositionValid = false;
        if (!g_pinMode && g_isMiniMode) {
            ExitMiniMode(false);
        }
        if (g_hPinBtn) InvalidateRect(g_hPinBtn, nullptr, TRUE);
        return true;
    } else if (id == IDM_TRAY_AUTORUN) {
        bool current = IsAutoRunEnabled();
        SetAutoRun(!current);
        return true;
    } else if (id == IDM_TRAY_THEME) {
        g_darkMode = !g_darkMode;
        ThemeColors th = g_darkMode ? GetDarkTheme() : GetLightTheme();
        if (g_hEditBrush) DeleteObject(g_hEditBrush);
        g_hEditBrush = CreateSolidBrush(th.bgEdit);
        ApplyDarkModeTitleBar(hWnd, g_darkMode);
        if (g_hPinBtn) InvalidateRect(g_hPinBtn, nullptr, TRUE);
        InvalidateRect(hWnd, nullptr, TRUE);
        return true;
    } else if (id == IDM_TRAY_EXIT) {
        DestroyWindow(hWnd);
        return true;
    }
    return false;
}

bool HandleDrawItem(HWND /*hWnd*/, DRAWITEMSTRUCT* pDIS) {
    if (!pDIS) return false;
    ThemeColors th = g_darkMode ? GetDarkTheme() : GetLightTheme();
    bool isSelected = (pDIS->itemState & ODS_SELECTED) != 0;

    if (pDIS->CtlID == IDC_PIN_BTN) {
        bool isPinned = g_pinMode;
        COLORREF bgBtn;
        COLORREF borderBtn;
        COLORREF pinColor;

        if (isPinned) {
            bgBtn = g_darkMode ? RGB(37, 99, 235) : RGB(59, 130, 246);
            borderBtn = g_darkMode ? RGB(96, 165, 250) : RGB(37, 99, 235);
            pinColor = RGB(255, 255, 255);
        } else if (isSelected) {
            bgBtn = g_darkMode ? RGB(55, 65, 81) : RGB(226, 232, 240);
            borderBtn = th.borderCardHover;
            pinColor = th.textPrimary;
        } else {
            bgBtn = th.bgHeader;
            borderBtn = th.borderSep;
            pinColor = th.textSecondary;
        }

        HBRUSH hBr = CreateSolidBrush(bgBtn);
        HPEN hPen = CreatePen(PS_SOLID, 1, borderBtn);
        HGDIOBJ hOldBr = SelectObject(pDIS->hDC, hBr);
        HGDIOBJ hOldPen = SelectObject(pDIS->hDC, hPen);
        RoundRect(pDIS->hDC, pDIS->rcItem.left, pDIS->rcItem.top, pDIS->rcItem.right, pDIS->rcItem.bottom, 6, 6);
        SelectObject(pDIS->hDC, hOldBr);
        SelectObject(pDIS->hDC, hOldPen);
        DeleteObject(hBr);
        DeleteObject(hPen);

        RECT rcPin = pDIS->rcItem;
        if (isSelected) OffsetRect(&rcPin, 1, 1);
        DrawPinIcon(pDIS->hDC, rcPin, pinColor, isPinned);
        return true;
    } else if (pDIS->CtlID == IDC_SYNC_FOLDER_BTN) {
        COLORREF bgBtn = isSelected ? (g_darkMode ? RGB(55, 65, 81) : RGB(226, 232, 240)) : th.bgHeader;
        COLORREF borderBtn = isSelected ? th.borderCardHover : th.borderSep;
        COLORREF iconColor = isSelected ? th.textPrimary : th.textSecondary;

        HBRUSH hBr = CreateSolidBrush(bgBtn);
        HPEN hPen = CreatePen(PS_SOLID, 1, borderBtn);
        HGDIOBJ hOldBr = SelectObject(pDIS->hDC, hBr);
        HGDIOBJ hOldPen = SelectObject(pDIS->hDC, hPen);
        RoundRect(pDIS->hDC, pDIS->rcItem.left, pDIS->rcItem.top, pDIS->rcItem.right, pDIS->rcItem.bottom, 6, 6);
        SelectObject(pDIS->hDC, hOldBr);
        SelectObject(pDIS->hDC, hOldPen);
        DeleteObject(hBr);
        DeleteObject(hPen);

        RECT rcIcon = pDIS->rcItem;
        if (isSelected) OffsetRect(&rcIcon, 1, 1);
        DrawFolderCloudIcon(pDIS->hDC, rcIcon, iconColor);
        return true;
    } else if (pDIS->CtlID == IDC_SYNC_TOGGLE_BTN) {
        COLORREF bgBtn;
        COLORREF borderBtn;
        COLORREF cloudColor;

        if (g_syncToggle) {
            bgBtn = g_darkMode ? RGB(37, 99, 235) : RGB(59, 130, 246);
            borderBtn = g_darkMode ? RGB(96, 165, 250) : RGB(37, 99, 235);
            cloudColor = RGB(255, 255, 255);
        } else if (isSelected) {
            bgBtn = g_darkMode ? RGB(55, 65, 81) : RGB(226, 232, 240);
            borderBtn = th.borderCardHover;
            cloudColor = th.textPrimary;
        } else {
            bgBtn = th.bgCard;
            borderBtn = th.borderSep;
            cloudColor = th.textSecondary;
        }

        HBRUSH hBr = CreateSolidBrush(bgBtn);
        HPEN hPen = CreatePen(PS_SOLID, 1, borderBtn);
        HGDIOBJ hOldBr = SelectObject(pDIS->hDC, hBr);
        HGDIOBJ hOldPen = SelectObject(pDIS->hDC, hPen);
        RoundRect(pDIS->hDC, pDIS->rcItem.left, pDIS->rcItem.top, pDIS->rcItem.right, pDIS->rcItem.bottom, 6, 6);
        SelectObject(pDIS->hDC, hOldBr);
        SelectObject(pDIS->hDC, hOldPen);
        DeleteObject(hBr);
        DeleteObject(hPen);

        RECT rcCloud = pDIS->rcItem;
        if (isSelected) OffsetRect(&rcCloud, 1, 1);
        DrawCloudIcon(pDIS->hDC, rcCloud, cloudColor, true);
        return true;
    }
    return false;
}

bool HandleAppTray(HWND hWnd, LPARAM lParam) {
    if (lParam == WM_LBUTTONUP) {
        ToggleWindow();
        return true;
    } else if (lParam == WM_RBUTTONUP) {
        POINT pt;
        GetCursorPos(&pt);
        g_inContextMenu = true;
        HMENU hMenu = CreatePopupMenu();
        InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, IDM_TRAY_OPEN, L"Megnyitás (Ctrl+F1)");
        InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_STRING, IDM_TRAY_COMPLETED, L"Elkészült feladatok...");
        InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);

        UINT uPinCheck = g_pinMode ? MF_CHECKED : MF_UNCHECKED;
        InsertMenuW(hMenu, 3, MF_BYPOSITION | MF_STRING | uPinCheck, IDM_TRAY_PIN, L"PIN mód (Ctrl+F2)");

        UINT uCheck = IsAutoRunEnabled() ? MF_CHECKED : MF_UNCHECKED;
        InsertMenuW(hMenu, 4, MF_BYPOSITION | MF_STRING | uCheck, IDM_TRAY_AUTORUN, L"Indítás a Windows-zal");

        std::wstring themeStr = g_darkMode ? L"Világos téma" : L"Sötét téma";
        InsertMenuW(hMenu, 5, MF_BYPOSITION | MF_STRING, IDM_TRAY_THEME, themeStr.c_str());

        InsertMenuW(hMenu, 6, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
        InsertMenuW(hMenu, 7, MF_BYPOSITION | MF_STRING, IDM_TRAY_EXIT, L"Kilépés");

        SetForegroundWindow(hWnd);
        TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, nullptr);
        DestroyMenu(hMenu);
        g_inContextMenu = false;
        return true;
    }
    return false;
}
