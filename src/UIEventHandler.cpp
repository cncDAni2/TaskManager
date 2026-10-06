#include "UIEventHandler.h"
#include "AppState.h"
#include "Drawing.h"
#include "Layout.h"
#include "InlineEdit.h"
#include "MainWindow.h"
#include "WorkTimeDialog.h"
#include "SettingsDialog.h"
#include "FocusMode.h"
#include "IntervalTimer.h"
#include <windowsx.h>
#include <algorithm>
#include <cwctype>

void SetWorkMeasurementStopped(bool stopped) {
    if (g_workMeasurementStopped == stopped) return;
    g_workMeasurementStopped = stopped;
    g_isWorkActive = false;
    SetWindowTextW(g_hWorkMeasureBtn,
        stopped ? L"Mérés indítása" : L"Mérés leállítása");
    g_store.Save();
    g_secondsSinceLastSave = 0;
    if (g_hWorkMeasureBtn) InvalidateRect(g_hWorkMeasureBtn, nullptr, TRUE);
    if (g_hWnd) InvalidateRect(g_hWnd, nullptr, FALSE);
}

namespace {
HWND g_messageBoxOwner = nullptr;

void ToggleTaskAssignment(HWND hWnd, int taskId) {
    std::wstring conflictingAssignee = g_store.ToggleAssignment(taskId, TaskUtils::GetCleanUserName());
    if (conflictingAssignee.empty()) return;

    std::wstring message = conflictingAssignee + L" már levette ezt a feladatot.";
    bool wasInContextMenu = g_inContextMenu;
    g_inContextMenu = true;
    MessageBoxW(hWnd, message.c_str(), L"Feladat már foglalt",
        MB_OK | (g_store.sounds_enabled ? MB_ICONINFORMATION : 0));
    g_inContextMenu = wasInContextMenu;
}

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

void DeleteTaskWithConfirmation(HWND hWnd, int taskId, const std::wstring& initialTaskName) {
    std::wstring taskName = initialTaskName;
    if (g_editingTaskId == taskId && g_hInlineEdit) {
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
        MB_YESNO | (g_store.sounds_enabled ? MB_ICONQUESTION : 0) | MB_DEFBUTTON2);
    if (messageBoxHook) UnhookWindowsHookEx(messageBoxHook);
    g_messageBoxOwner = nullptr;
    g_inContextMenu = wasInContextMenu;
    if (answer != IDYES) return;

    CommitInlineEdit();
    g_store.Delete(taskId);
    auto it = std::find(g_sessionActiveTaskIds.begin(), g_sessionActiveTaskIds.end(), taskId);
    if (it != g_sessionActiveTaskIds.end()) g_sessionActiveTaskIds.erase(it);
    UpdateControlsVisibility();
    RecalculateLayout();
    if (g_selectedIndex >= (int)g_displayItems.size()) g_selectedIndex = (int)g_displayItems.size() - 1;
    SetFocus(hWnd);
    InvalidateRect(hWnd, nullptr, TRUE);
}

void ShowTaskOptionsMenu(HWND hWnd, int itemIndex) {
    if (itemIndex < 0 || itemIndex >= (int)g_displayItems.size()) return;
    int taskId = g_displayItems[itemIndex].task.id;
    CommitInlineEdit();

    int resolvedIndex = -1;
    for (size_t i = 0; i < g_displayItems.size(); ++i) {
        if (!g_displayItems[i].isHeader && g_displayItems[i].task.id == taskId) {
            resolvedIndex = (int)i;
            break;
        }
    }
    if (resolvedIndex < 0) return;

    const Task task = g_displayItems[resolvedIndex].task;
    const RECT optionsRect = g_displayItems[resolvedIndex].optionsRect;
    g_selectedIndex = resolvedIndex;

    enum : UINT { MenuDelete = 1, MenuEdit, MenuToggleCompleted };
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, MenuDelete, L"Törlés");
    AppendMenuW(menu, MF_STRING, MenuEdit, L"Szerkesztés (Enter)");
    AppendMenuW(menu, MF_STRING, MenuToggleCompleted,
        task.completed ? L"Újra megnyitás (Space)" : L"Késznek jelölés (Space)");
    if (task.completed) EnableMenuItem(menu, MenuEdit, MF_BYCOMMAND | MF_GRAYED);

    POINT menuPoint = { optionsRect.right, optionsRect.top - g_scrollY };
    ClientToScreen(hWnd, &menuPoint);
    bool wasInContextMenu = g_inContextMenu;
    g_inContextMenu = true;
    SetForegroundWindow(hWnd);
    UINT command = TrackPopupMenu(menu,
        TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
        menuPoint.x, menuPoint.y, 0, hWnd, nullptr);
    DestroyMenu(menu);
    g_inContextMenu = wasInContextMenu;
    SetFocus(hWnd);

    if (command == MenuDelete) {
        DeleteTaskWithConfirmation(hWnd, task.id, task.text);
    } else if (command == MenuEdit) {
        StartInlineEdit(resolvedIndex);
    } else if (command == MenuToggleCompleted) {
        CommitInlineEdit();
        g_store.ToggleCompleted(task.id);
        UpdateControlsVisibility();
        RecalculateLayout();
        if (g_selectedIndex >= (int)g_displayItems.size()) g_selectedIndex = (int)g_displayItems.size() - 1;
        InvalidateRect(hWnd, nullptr, TRUE);
    }
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
                DeleteTaskWithConfirmation(hWnd, item.task.id, item.task.text);
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
        RECT rcTimerText = GetMiniTimerTextRect(rcClient.right);
        RECT rcDragHandle = GetMiniDragHandleRect(rcClient.right);
        RECT rcFocusButton = GetMiniFocusButtonRect(rcClient.right);
        if (g_intervalTimer.IsActive() && PtInRect(&rcTimerText, { mx, my })) {
            g_viewMode = ViewMode::IntervalTimer;
            ShowAppWindow();
            return true;
        }
        if (g_pinMode && PtInRect(&rcFocusButton, { mx, my })) {
            FocusMode::Toggle();
            return true;
        }
        if (g_pinMode && PtInRect(&rcDragHandle, { mx, my })) {
            ReleaseCapture();
            SendMessageW(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return true;
        }

        for (size_t i = 0; i < g_displayItems.size(); ++i) {
            const auto& item = g_displayItems[i];
            if (item.assignRect.right > 0 && PtInRect(&item.assignRect, { mx, my })) {
                ToggleTaskAssignment(hWnd, item.task.id);
                RecalculateMiniLayout();
                InvalidateRect(hWnd, nullptr, FALSE);
                return true;
            } else if (PtInRect(&item.checkRect, { mx, my })) {
                const bool wasCompleted = item.task.completed;
                g_store.ToggleCompleted(item.task.id);
                auto graceIt = std::find(g_miniCompletionGraceTaskIds.begin(),
                    g_miniCompletionGraceTaskIds.end(), item.task.id);
                if (wasCompleted) {
                    if (graceIt != g_miniCompletionGraceTaskIds.end()) {
                        g_miniCompletionGraceTaskIds.erase(graceIt);
                    }
                } else if (graceIt == g_miniCompletionGraceTaskIds.end()) {
                    g_miniCompletionGraceTaskIds.push_back(item.task.id);
                }
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
    const int timerStatusTop = rcClient.bottom - BOTTOM_BAR_HEIGHT - TIMER_STATUS_HEIGHT;
    const int workStatusTop = rcClient.bottom - BOTTOM_BAR_HEIGHT;
    if (g_intervalTimer.IsActive() && clickRawY >= timerStatusTop && clickRawY < workStatusTop) {
        CommitInlineEdit();
        g_viewMode = ViewMode::IntervalTimer;
        g_scrollY = 0;
        g_selectedIndex = -1;
        UpdateControlsVisibility();
        RecalculateLayout();
        InvalidateRect(hWnd, nullptr, TRUE);
        return true;
    }
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
            int topOffset = g_viewMode == ViewMode::ActiveTasks ? 94 : (g_viewMode == ViewMode::WorkHistory ? 48 : 54);
            int viewableHeight = GetListBottom(rcClient.bottom) - topOffset;
            g_scrollY = std::max(0, g_scrollY - viewableHeight);
            UpdateInlineEditPos();
            InvalidateRect(hWnd, nullptr, TRUE);
            return true;
        } else {
            int topOffset = g_viewMode == ViewMode::ActiveTasks ? 94 : (g_viewMode == ViewMode::WorkHistory ? 48 : 54);
            int viewableHeight = GetListBottom(rcClient.bottom) - topOffset;
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
            ToggleTaskAssignment(hWnd, taskId);
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
        } else if (!item.task.is_sync && item.markerRect.right > 0 && PtInRect(&item.markerRect, { mx, my })) {
            CommitInlineEdit();
            g_store.CycleMarker(item.task.id);
            RecalculateLayout();
            SetFocus(hWnd);
            InvalidateRect(hWnd, nullptr, FALSE);
            return true;
        } else if (item.optionsRect.right > 0 && PtInRect(&item.optionsRect, { mx, my })) {
            ShowTaskOptionsMenu(hWnd, (int)i);
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
            DeleteTaskWithConfirmation(hWnd, item.task.id, item.task.text);
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
                else if (item.markerRect.right > 0 && PtInRect(&item.markerRect, { mx, my })) newHoverBtn = 5;
                else if (item.optionsRect.right > 0 && PtInRect(&item.optionsRect, { mx, my })) newHoverBtn = 6;
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
    if (id == IDC_ADD_TASK_BTN) {
        if (!g_hEdit || g_viewMode != ViewMode::ActiveTasks) return true;
        int textLength = GetWindowTextLengthW(g_hEdit);
        std::vector<wchar_t> textBuffer(textLength + 1);
        GetWindowTextW(g_hEdit, textBuffer.data(), textLength + 1);
        std::wstring taskText = textBuffer.data();
        while (!taskText.empty() && std::iswspace(taskText.front())) taskText.erase(taskText.begin());
        while (!taskText.empty() && std::iswspace(taskText.back())) taskText.pop_back();
        if (taskText.empty()) {
            SetFocus(g_hEdit);
            return true;
        }

        g_store.Add(taskText, g_syncToggle);
        g_syncToggle = false;
        if (g_hSyncToggleBtn) InvalidateRect(g_hSyncToggleBtn, nullptr, TRUE);
        g_sessionActiveTaskIds.insert(g_sessionActiveTaskIds.begin(), g_store.tasks.back().id);
        SetWindowTextW(g_hEdit, L"");
        g_scrollY = 0;
        g_selectedIndex = -1;
        UpdateControlsVisibility();
        RecalculateLayout();
        SetFocus(g_hEdit);
        InvalidateRect(hWnd, nullptr, TRUE);
        return true;
    } else if (id == IDC_CLOSE_BTN) {
        if (g_pinMode) {
            EnterMiniMode();
        } else {
            HideAppWindow();
        }
        return true;
    } else if (id == IDC_PIN_BTN) {
        g_pinMode = !g_pinMode;
        g_miniPositionValid = false;
        if (g_hPinBtn) InvalidateRect(g_hPinBtn, nullptr, TRUE);
        return true;
    } else if (id == IDC_FOCUS_MODE_BTN) {
        FocusMode::Toggle();
        return true;
    } else if (id == IDC_SYNC_TOGGLE_BTN) {
        g_syncToggle = !g_syncToggle;
        if (g_hSyncToggleBtn) InvalidateRect(g_hSyncToggleBtn, nullptr, TRUE);
        SetFocus(g_hEdit);
        return true;
    } else if (id == IDC_SETTINGS_BTN) {
        ShowSettingsDialog(hWnd);
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
    } else if (id == IDC_TIMER_BTN) {
        CommitInlineEdit();
        g_viewMode = ViewMode::IntervalTimer;
        g_scrollY = 0;
        g_selectedIndex = -1;
        UpdateControlsVisibility();
        RecalculateLayout();
        if (g_hTimerBtn) InvalidateRect(g_hTimerBtn, nullptr, TRUE);
        InvalidateRect(hWnd, nullptr, TRUE);
        return true;
    } else if (id == IDC_STOP_WORK_BTN) {
        SetWorkMeasurementStopped(!g_workMeasurementStopped);
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
        if (g_viewMode == ViewMode::IntervalTimer) {
            g_viewMode = ViewMode::ActiveTasks;
        } else if (g_viewMode == ViewMode::ActiveTasks) {
            g_viewMode = ViewMode::CompletedTasks;
        } else {
            g_viewMode = ViewMode::ActiveTasks;
        }
        g_scrollY = 0;
        g_selectedIndex = -1;
        UpdateControlsVisibility();
        RecalculateLayout();
        if (g_hTimerBtn) InvalidateRect(g_hTimerBtn, nullptr, TRUE);
        InvalidateRect(hWnd, nullptr, TRUE);
        return true;
    } else if (id == IDC_SETTINGS_BTN) {
        ShowSettingsDialog(hWnd);
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
        ShowAppWindow();
        return true;
    } else if (id == IDM_TRAY_TIMER) {
        if (g_isMiniMode) {
            ExitMiniMode(false);
        }
        g_viewMode = ViewMode::IntervalTimer;
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
    } else if (id == IDM_TRAY_EXIT) {
        DestroyWindow(hWnd);
        return true;
    }
    return false;
}

bool HandleDrawItem(HWND /*hWnd*/, DRAWITEMSTRUCT* pDIS) {
    if (!pDIS) return false;
    ThemeColors th = GetThemeColors(g_themeMode);
    bool isSelected = (pDIS->itemState & ODS_SELECTED) != 0;
    bool isPink = g_themeMode == ThemeMode::Pink;
    COLORREF pinkButtonBg = RGB(183, 0, 99);

    if (pDIS->CtlID == IDC_TIMER_2020_CHECK || pDIS->CtlID == IDC_TIMER_STRICT_CHECK) {
        const bool isDisabled = (pDIS->itemState & ODS_DISABLED) != 0;
        const bool isChecked = pDIS->CtlID == IDC_TIMER_2020_CHECK
            ? g_store.timer_2020_enabled : g_store.timer_strict_mode;
        const COLORREF textColor = isDisabled ? th.textSecondary : th.textPrimary;
        HBRUSH backgroundBrush = CreateSolidBrush(th.bgWindow);
        FillRect(pDIS->hDC, &pDIS->rcItem, backgroundBrush);
        DeleteObject(backgroundBrush);

        RECT checkRect = pDIS->rcItem;
        checkRect.left += 1;
        checkRect.top += ((checkRect.bottom - checkRect.top) - 18) / 2;
        checkRect.right = checkRect.left + 18;
        checkRect.bottom = checkRect.top + 18;

        SetBkMode(pDIS->hDC, TRANSPARENT);
        HGDIOBJ oldBrush = SelectObject(pDIS->hDC, GetStockObject(NULL_BRUSH));
        HPEN borderPen = CreatePen(PS_SOLID, 1, isDisabled ? th.textSecondary : th.checkActiveBorder);
        HGDIOBJ oldPen = SelectObject(pDIS->hDC, borderPen);
        Rectangle(pDIS->hDC, checkRect.left, checkRect.top, checkRect.right, checkRect.bottom);
        SelectObject(pDIS->hDC, oldPen);
        DeleteObject(borderPen);

        if (isChecked) {
            HPEN checkPen = CreatePen(PS_SOLID, 2, textColor);
            oldPen = SelectObject(pDIS->hDC, checkPen);
            MoveToEx(pDIS->hDC, checkRect.left + 4, checkRect.top + 9, nullptr);
            LineTo(pDIS->hDC, checkRect.left + 7, checkRect.top + 13);
            LineTo(pDIS->hDC, checkRect.left + 14, checkRect.top + 4);
            SelectObject(pDIS->hDC, oldPen);
            DeleteObject(checkPen);
        }

        SetTextColor(pDIS->hDC, textColor);
        HFONT font = (HFONT)SendMessageW(pDIS->hwndItem, WM_GETFONT, 0, 0);
        HGDIOBJ oldFont = font ? SelectObject(pDIS->hDC, font) : nullptr;
        wchar_t label[128]{};
        GetWindowTextW(pDIS->hwndItem, label, _countof(label));
        RECT textRect = pDIS->rcItem;
        textRect.left = checkRect.right + 8;
        DrawTextW(pDIS->hDC, label, -1, &textRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (pDIS->itemState & ODS_FOCUS) {
            RECT focusRect = pDIS->rcItem;
            InflateRect(&focusRect, -1, -1);
            DrawFocusRect(pDIS->hDC, &focusRect);
        }
        if (oldFont) SelectObject(pDIS->hDC, oldFont);
        SelectObject(pDIS->hDC, oldBrush);
        return true;
    }

    bool isTextButton = pDIS->CtlID == IDC_TIME_HISTORY_BTN || pDIS->CtlID == IDC_MANUAL_WORK_BTN ||
        pDIS->CtlID == IDC_STOP_WORK_BTN ||
        pDIS->CtlID == IDC_TOGGLE_VIEW_BTN || pDIS->CtlID == IDC_TIMER_START_BTN ||
        pDIS->CtlID == IDC_TIMER_PAUSE_BTN || pDIS->CtlID == IDC_TIMER_NEXT_BTN ||
        pDIS->CtlID == IDC_CLOSE_BTN ||
        pDIS->CtlID == IDC_ADD_TASK_BTN;
    if (isTextButton) {
        bool isDark = g_themeMode == ThemeMode::Dark;
        bool isMeasurementStopped = pDIS->CtlID == IDC_STOP_WORK_BTN && g_workMeasurementStopped;
        COLORREF buttonBg = isMeasurementStopped ? RGB(220, 38, 38)
            : (isPink ? pinkButtonBg
                : (isDark ? RGB(60, 60, 60) : (isSelected ? th.bgCardHover : th.bgHeader)));
        COLORREF buttonText = isMeasurementStopped ? RGB(0, 0, 0)
            : (isPink || isDark ? RGB(255, 255, 255) : th.textPrimary);
        COLORREF buttonBorder = isMeasurementStopped ? RGB(255, 255, 255)
            : (isPink ? (isSelected ? th.bgHeader : th.borderCard)
                : (isDark ? RGB(82, 82, 82) : (isSelected ? th.borderCardHover : th.borderSep)));
        HBRUSH brush = CreateSolidBrush(buttonBg);
        HPEN pen = CreatePen(PS_SOLID, 1, buttonBorder);
        HGDIOBJ oldBrush = SelectObject(pDIS->hDC, brush);
        HGDIOBJ oldPen = SelectObject(pDIS->hDC, pen);
        RoundRect(pDIS->hDC, pDIS->rcItem.left, pDIS->rcItem.top,
            pDIS->rcItem.right, pDIS->rcItem.bottom, 6, 6);
        SelectObject(pDIS->hDC, oldBrush);
        SelectObject(pDIS->hDC, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);

        wchar_t label[128]{};
        GetWindowTextW(pDIS->hwndItem, label, _countof(label));
        SetBkMode(pDIS->hDC, TRANSPARENT);
        SetTextColor(pDIS->hDC, buttonText);
        HFONT font = (HFONT)SendMessageW(pDIS->hwndItem, WM_GETFONT, 0, 0);
        HGDIOBJ oldFont = font ? SelectObject(pDIS->hDC, font) : nullptr;
        RECT textRect = pDIS->rcItem;
        InflateRect(&textRect, -4, -2);
        DrawTextW(pDIS->hDC, label, -1, &textRect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if ((pDIS->CtlID == IDC_TIMER_START_BTN || pDIS->CtlID == IDC_TIMER_PAUSE_BTN ||
            pDIS->CtlID == IDC_TIMER_NEXT_BTN) &&
            (pDIS->itemState & ODS_FOCUS)) {
            RECT focusRect = pDIS->rcItem;
            InflateRect(&focusRect, -4, -4);
            DrawFocusRect(pDIS->hDC, &focusRect);
        }
        if (oldFont) SelectObject(pDIS->hDC, oldFont);
        return true;
    }

    if (pDIS->CtlID == IDC_FOCUS_MODE_BTN) {
        COLORREF bgBtn = g_focusMode ? RGB(30, 58, 138)
            : (isPink ? pinkButtonBg : (isSelected ? th.bgCardHover : th.bgHeader));
        COLORREF borderBtn = g_focusMode ? RGB(30, 64, 175)
            : (isSelected ? th.borderCardHover : th.borderSep);
        COLORREF iconColor = isPink || g_focusMode ? RGB(255, 255, 255)
            : (isSelected ? th.textPrimary : th.textSecondary);
        HBRUSH hBr = CreateSolidBrush(bgBtn);
        HPEN hPen = CreatePen(PS_SOLID, 1, borderBtn);
        HGDIOBJ hOldBr = SelectObject(pDIS->hDC, hBr);
        HGDIOBJ hOldPen = SelectObject(pDIS->hDC, hPen);
        RoundRect(pDIS->hDC, pDIS->rcItem.left, pDIS->rcItem.top,
            pDIS->rcItem.right, pDIS->rcItem.bottom, 6, 6);
        SelectObject(pDIS->hDC, hOldBr);
        SelectObject(pDIS->hDC, hOldPen);
        DeleteObject(hBr);
        DeleteObject(hPen);
        DrawFocusIcon(pDIS->hDC, pDIS->rcItem, iconColor);
        return true;
    } else if (pDIS->CtlID == IDC_PIN_BTN) {
        bool isPinned = g_pinMode;
        COLORREF bgBtn;
        COLORREF borderBtn;
        COLORREF pinColor;

        if (isPinned) {
            bgBtn = RGB(30, 58, 138);
            borderBtn = RGB(30, 64, 175);
            pinColor = RGB(255, 255, 255);
        } else if (isSelected) {
            bgBtn = isPink ? pinkButtonBg : th.bgCardHover;
            borderBtn = th.borderCardHover;
            pinColor = isPink ? RGB(255, 255, 255) : th.textPrimary;
        } else {
            bgBtn = isPink ? pinkButtonBg : th.bgHeader;
            borderBtn = th.borderSep;
            pinColor = isPink ? RGB(255, 255, 255) : th.textSecondary;
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
    } else if (pDIS->CtlID == IDC_TIMER_BTN) {
        const bool selected = g_viewMode == ViewMode::IntervalTimer;
        COLORREF bgBtn = isPink ? pinkButtonBg
            : (selected ? th.bgCardSelected : th.bgHeader);
        COLORREF borderBtn = selected ? th.borderCardSelected : th.borderSep;
        COLORREF iconColor = (isPink || selected) ? RGB(255, 255, 255) : th.textSecondary;
        HBRUSH brush = CreateSolidBrush(bgBtn);
        HPEN pen = CreatePen(PS_SOLID, 1, borderBtn);
        HGDIOBJ oldBrush = SelectObject(pDIS->hDC, brush);
        HGDIOBJ oldPen = SelectObject(pDIS->hDC, pen);
        RoundRect(pDIS->hDC, pDIS->rcItem.left, pDIS->rcItem.top,
            pDIS->rcItem.right, pDIS->rcItem.bottom, 6, 6);
        SelectObject(pDIS->hDC, oldBrush);
        SelectObject(pDIS->hDC, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
        RECT iconRect = pDIS->rcItem;
        if (isSelected) OffsetRect(&iconRect, 1, 1);
        DrawTimerIcon(pDIS->hDC, iconRect, iconColor);
        return true;
    } else if (pDIS->CtlID == IDC_SETTINGS_BTN) {
        COLORREF bgBtn = isSelected ? RGB(226, 232, 240) : RGB(248, 250, 252);
        COLORREF borderBtn = RGB(203, 213, 225);
        COLORREF iconColor = RGB(71, 85, 105);

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
        DrawSettingsIcon(pDIS->hDC, rcIcon, iconColor, bgBtn);
        return true;
    } else if (pDIS->CtlID == IDC_SYNC_TOGGLE_BTN) {
        COLORREF bgBtn;
        COLORREF borderBtn;
        COLORREF cloudColor;

        if (g_syncToggle) {
            bgBtn = RGB(30, 58, 138);
            borderBtn = RGB(30, 64, 175);
            cloudColor = RGB(255, 255, 255);
        } else if (isPink) {
            bgBtn = pinkButtonBg;
            borderBtn = th.borderCard;
            cloudColor = RGB(255, 255, 255);
        } else if (isSelected) {
            bgBtn = th.bgCardHover;
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
        InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING, IDM_TRAY_TIMER, L"Időzítő");
        InsertMenuW(hMenu, 3, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);

        UINT uPinCheck = g_pinMode ? MF_CHECKED : MF_UNCHECKED;
        InsertMenuW(hMenu, 4, MF_BYPOSITION | MF_STRING | uPinCheck, IDM_TRAY_PIN, L"PIN mód (Ctrl+F2)");

        UINT uCheck = IsAutoRunEnabled() ? MF_CHECKED : MF_UNCHECKED;
        InsertMenuW(hMenu, 5, MF_BYPOSITION | MF_STRING | uCheck, IDM_TRAY_AUTORUN, L"Indítás a Windows-zal");

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
