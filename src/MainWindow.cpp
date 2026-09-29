#include "MainWindow.h"
#include "AppState.h"
#include "Drawing.h"
#include "Layout.h"
#include "InlineEdit.h"
#include "Paint.h"
#include "UIEventHandler.h"
#include "PopupNotice.h"
#include "BarTooltips.h"
#include "FocusMode.h"
#include "SettingsDialog.h"
#include <windowsx.h>
#include <commctrl.h>
#include <algorithm>
#include <set>

namespace {
void ReloadSyncAndNotify() {
    std::set<int> knownSyncIds;
    for (const auto& task : g_store.tasks) {
        if (task.is_sync) knownSyncIds.insert(task.id);
    }

    g_store.LoadSync();
    for (const auto& task : g_store.tasks) {
        if (task.is_sync && knownSyncIds.find(task.id) == knownSyncIds.end()) {
            ShowNewTaskNotice(task.text);
        }
    }
}
}

void ToggleWindow() {
    if (g_isMiniMode) {
        g_viewMode = ViewMode::ActiveTasks;
        ExitMiniMode(false);
        SetForegroundWindow(g_hWnd);
        if (g_viewMode == ViewMode::ActiveTasks && g_hEdit) SetFocus(g_hEdit);
    } else if (IsWindowVisible(g_hWnd)) {
        if (g_pinMode) {
            EnterMiniMode();
        } else {
            HideAppWindow();
        }
    } else {
        g_viewMode = ViewMode::ActiveTasks;
        ShowAppWindow();
    }
}

void ShowAppWindow() {
    CancelInlineEdit();

    if (g_isMiniMode) {
        ExitMiniMode(false);
    }

    RECT rcWork;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);
    int winW = 585;
    int winH = 550;
    int x = rcWork.right - winW - 12;
    int y = rcWork.bottom - winH - 12;

    g_fullWinX = x;
    g_fullWinY = y;
    g_fullWinW = winW;
    g_fullWinH = winH;

    ReloadSyncAndNotify();
    for (const auto& t : g_store.tasks) {
        if (!t.completed && std::find(g_store.active_order.begin(), g_store.active_order.end(), t.id) == g_store.active_order.end()) {
            g_store.active_order.push_back(t.id);
        }
    }
    g_sessionActiveTaskIds = g_store.active_order;
    g_sessionActiveIdsInitialized = true;
    g_selectedIndex = -1;

    SetWindowPos(g_hWnd, HWND_TOPMOST, x, y, winW, winH, SWP_SHOWWINDOW);
    SetForegroundWindow(g_hWnd);
    UpdateControlsVisibility();
    RecalculateLayout();
    if (g_hPinBtn) InvalidateRect(g_hPinBtn, nullptr, TRUE);
    if (g_hSettingsBtn) InvalidateRect(g_hSettingsBtn, nullptr, TRUE);
    if (g_hSyncToggleBtn) InvalidateRect(g_hSyncToggleBtn, nullptr, TRUE);
    InvalidateRect(g_hWnd, nullptr, TRUE);

    if (g_viewMode == ViewMode::ActiveTasks && g_hEdit) {
        SetFocus(g_hEdit);
    }
}

void HideAppWindow() {
    CommitInlineEdit();
    if (g_isMiniMode) {
        ExitMiniMode(true);
    } else {
        ShowWindow(g_hWnd, SW_HIDE);
    }
    g_sessionActiveIdsInitialized = false;
    g_store.Save();
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hWnd = hWnd;
            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

            ApplyDarkModeTitleBar(hWnd, g_darkMode);

            g_hFontTitle = CreateFontW(19, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontNormal = CreateFontW(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontNormalStrike = CreateFontW(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, TRUE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontSmall = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontHeader = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            ThemeColors th = g_darkMode ? GetDarkTheme() : GetLightTheme();
            g_hEditBrush = CreateSolidBrush(th.bgEdit);

            g_hTimeHistoryBtn = CreateWindowW(L"BUTTON", L"Idők",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                286, 10, 54, 28, hWnd, (HMENU)IDC_TIME_HISTORY_BTN, hInst, nullptr);
            SendMessageW(g_hTimeHistoryBtn, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

            g_hFocusModeBtn = CreateWindowW(L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                252, 10, 28, 28, hWnd, (HMENU)IDC_FOCUS_MODE_BTN, hInst, nullptr);

            g_hManualWorkBtn = CreateWindowW(L"BUTTON", L"Manuális",
                WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
                394, 10, 80, 28, hWnd, (HMENU)IDC_MANUAL_WORK_BTN, hInst, nullptr);
            SendMessageW(g_hManualWorkBtn, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

            g_hToggleViewBtn = CreateWindowW(L"BUTTON", L"Elkészült (0)",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                345, 10, 125, 28, hWnd, (HMENU)IDC_TOGGLE_VIEW_BTN, hInst, nullptr);
            SendMessageW(g_hToggleViewBtn, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

            g_hSettingsBtn = CreateWindowW(L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                478, 10, 28, 28, hWnd, (HMENU)IDC_SETTINGS_BTN, hInst, nullptr);

            g_hPinBtn = CreateWindowW(L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                512, 10, 28, 28, hWnd, (HMENU)IDC_PIN_BTN, hInst, nullptr);

            g_hCloseBtn = CreateWindowW(L"BUTTON", L"✕",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                545, 10, 28, 28, hWnd, (HMENU)IDC_CLOSE_BTN, hInst, nullptr);
            SendMessageW(g_hCloseBtn, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            HWND hTooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr,
                WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                hWnd, nullptr, hInst, nullptr);
            if (hTooltip) {
                TOOLINFOW tiSettings{};
                tiSettings.cbSize = sizeof(TOOLINFOW);
                tiSettings.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
                tiSettings.hwnd = hWnd;
                tiSettings.uId = (UINT_PTR)g_hSettingsBtn;
                tiSettings.lpszText = (LPWSTR)L"Beállítások";
                SendMessageW(hTooltip, TTM_ADDTOOL, 0, (LPARAM)&tiSettings);

                TOOLINFOW tiPin{};
                tiPin.cbSize = sizeof(TOOLINFOW);
                tiPin.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
                tiPin.hwnd = hWnd;
                tiPin.uId = (UINT_PTR)g_hPinBtn;
                tiPin.lpszText = (LPWSTR)L"PIN mód (fókuszvesztéskor kisméretű lista)";
                SendMessageW(hTooltip, TTM_ADDTOOL, 0, (LPARAM)&tiPin);

                TOOLINFOW tiFocus{};
                tiFocus.cbSize = sizeof(TOOLINFOW);
                tiFocus.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
                tiFocus.hwnd = hWnd;
                tiFocus.uId = (UINT_PTR)g_hFocusModeBtn;
                tiFocus.lpszText = (LPWSTR)L"Fókusz mód (zároláskor automatikusan kikapcsol)";
                SendMessageW(hTooltip, TTM_ADDTOOL, 0, (LPARAM)&tiFocus);

                TOOLINFOW tiClose{};
                tiClose.cbSize = sizeof(TOOLINFOW);
                tiClose.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
                tiClose.hwnd = hWnd;
                tiClose.uId = (UINT_PTR)g_hCloseBtn;
                tiClose.lpszText = (LPWSTR)L"Bezárás (PIN módból kilépés)";
                SendMessageW(hTooltip, TTM_ADDTOOL, 0, (LPARAM)&tiClose);

                TOOLINFOW tiManualWork{};
                tiManualWork.cbSize = sizeof(TOOLINFOW);
                tiManualWork.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
                tiManualWork.hwnd = hWnd;
                tiManualWork.uId = (UINT_PTR)g_hManualWorkBtn;
                tiManualWork.lpszText = (LPWSTR)L"Manuális munkaidő szerkesztése";
                SendMessageW(hTooltip, TTM_ADDTOOL, 0, (LPARAM)&tiManualWork);
            }

            g_hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                12, 56, 425, 28, hWnd, (HMENU)IDC_NEW_TASK_EDIT, hInst, nullptr);
            SendMessageW(g_hEdit, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            SendMessageW(g_hEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Új feladat hozzáadása... (Enter / Tab navigációhoz)");
            SetWindowSubclass(g_hEdit, EditSubclassProc, 0, 0);

            g_hAddBtn = CreateWindowW(L"BUTTON", L"+ Hozzáad",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                444, 56, 92, 28, hWnd, (HMENU)IDC_ADD_TASK_BTN, hInst, nullptr);
            SendMessageW(g_hAddBtn, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            g_hSyncToggleBtn = CreateWindowW(L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                542, 56, 31, 28, hWnd, (HMENU)IDC_SYNC_TOGGLE_BTN, hInst, nullptr);

            if (hTooltip) {
                TOOLINFOW tiSyncToggle{};
                tiSyncToggle.cbSize = sizeof(TOOLINFOW);
                tiSyncToggle.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
                tiSyncToggle.hwnd = hWnd;
                tiSyncToggle.uId = (UINT_PTR)g_hSyncToggleBtn;
                tiSyncToggle.lpszText = (LPWSTR)L"Globális szinkronizált feladat (OneDrive)";
                SendMessageW(hTooltip, TTM_ADDTOOL, 0, (LPARAM)&tiSyncToggle);
            }

            g_hFilterRecentBtn = CreateWindowW(L"BUTTON", L"Előző nap 9:30 óta",
                WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
                12, 54, 270, 26, hWnd, (HMENU)IDC_FILTER_RECENT, hInst, nullptr);
            SendMessageW(g_hFilterRecentBtn, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

            g_hFilterAllBtn = CreateWindowW(L"BUTTON", L"MIND (napi bontás)",
                WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
                290, 54, 280, 26, hWnd, (HMENU)IDC_FILTER_ALL, hInst, nullptr);
            SendMessageW(g_hFilterAllBtn, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

            RegisterHotKey(hWnd, ID_HOTKEY_TOGGLE, MOD_CONTROL, VK_F1);
            RegisterHotKey(hWnd, ID_HOTKEY_UNPIN, MOD_CONTROL, VK_F2);
            FocusMode::RegisterSessionNotifications(hWnd);

            SetTimer(hWnd, IDT_WORK_TIMER, 1000, nullptr);

            g_hIcon = GenerateAppIcon();
            g_nid.cbSize = sizeof(NOTIFYICONDATAW);
            g_nid.hWnd = hWnd;
            g_nid.uID = 1;
            g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            g_nid.uCallbackMessage = WM_APP_TRAY;
            g_nid.hIcon = g_hIcon;
            wcscpy_s(g_nid.szTip, L"Feladatkezelő (Ctrl+F1)");
            Shell_NotifyIconW(NIM_ADD, &g_nid);

            UpdateControlsVisibility();
            RecalculateLayout();
            return 0;
        }

        case WM_ACTIVATE: {
            if (LOWORD(wParam) == WA_INACTIVE) {
                if (g_inContextMenu) return 0;
                HWND hActivating = (HWND)lParam;
                if (hActivating && (hActivating == hWnd || IsChild(hWnd, hActivating))) {
                    return 0;
                }
                if (IsWindowVisible(hWnd)) {
                    if (g_pinMode) {
                        EnterMiniMode();
                    } else {
                        HideAppWindow();
                    }
                }
            }
            return 0;
        }

        case WM_ACTIVATEAPP: {
            if (!wParam) {
                if (g_inContextMenu) return 0;
                if (IsWindowVisible(hWnd)) {
                    if (g_pinMode) {
                        EnterMiniMode();
                    } else {
                        HideAppWindow();
                    }
                }
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == IDT_WORK_TIMER) {
                if (g_isMiniMode && IsWindowVisible(hWnd)) {
                    const time_t now = time(nullptr);
                    const bool hasExpiredCompletedTask = std::any_of(
                        g_displayItems.begin(), g_displayItems.end(), [now](const DisplayItem& item) {
                            return !item.isHeader && item.task.completed && item.task.completed_at > 0 &&
                                now - item.task.completed_at >= 20;
                        });
                    if (hasExpiredCompletedTask) {
                        RecalculateMiniLayout();
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                }

                g_store.CheckWorkReset();

                LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) };
                GetLastInputInfo(&lii);
                DWORD nowTick = GetTickCount();
                DWORD idleMs = (nowTick >= lii.dwTime) ? (nowTick - lii.dwTime) : 0;
                g_isIdlePaused = (idleMs >= 120000);

                g_isExcludedApp = false;
                g_excludedAppName = L"";
                HWND hFore = GetForegroundWindow();
                if (hFore) {
                    wchar_t titleBuf[512] = { 0 };
                    GetWindowTextW(hFore, titleBuf, 512);
                    std::wstring lower = titleBuf;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
                    if (lower.find(L"youtube") != std::wstring::npos) {
                        g_isExcludedApp = true;
                        g_excludedAppName = L"Youtube";
                    } else if (lower.find(L"discord") != std::wstring::npos) {
                        g_isExcludedApp = true;
                        g_excludedAppName = L"Discord";
                    } else if (lower.find(L"facebook") != std::wstring::npos) {
                        g_isExcludedApp = true;
                        g_excludedAppName = L"Facebook";
                    }
                }

                bool countWork = !g_isSessionLocked &&
                    (g_focusMode || (!g_isIdlePaused && !g_isExcludedApp));
                g_isWorkActive = countWork;

                if (countWork) {
                    g_store.work_seconds_today++;
                    g_secondsSinceLastSave++;
                    if (g_secondsSinceLastSave >= 30) {
                        g_store.Save();
                        g_secondsSinceLastSave = 0;
                        if (g_viewMode == ViewMode::WorkHistory && IsWindowVisible(hWnd)) {
                            InvalidateRect(hWnd, nullptr, FALSE);
                        }
                    }
                }

                if (IsWindowVisible(hWnd)) {
                    RECT rcClient;
                    GetClientRect(hWnd, &rcClient);
                    if (g_isMiniMode) {
                        RECT rcMiniBottom = { 0, rcClient.bottom - 3, rcClient.right, rcClient.bottom };
                        InvalidateRect(hWnd, &rcMiniBottom, FALSE);
                    } else {
                        RECT rcBottom = { 0, rcClient.bottom - BOTTOM_BAR_HEIGHT, rcClient.right, rcClient.bottom };
                        InvalidateRect(hWnd, &rcBottom, FALSE);
                    }

                    static int s_syncTimer = 0;
                    s_syncTimer++;
                    if (s_syncTimer >= 10) {
                        s_syncTimer = 0;
                        if (g_editingTaskId == -1 && g_store.CheckSyncFileChanged()) {
                            ReloadSyncAndNotify();
                            RecalculateLayout();
                            InvalidateRect(hWnd, nullptr, FALSE);
                        }
                    }
                }
            }
            return 0;
        }

        case WM_HOTKEY: {
            if (wParam == ID_HOTKEY_TOGGLE) {
                ToggleWindow();
            } else if (wParam == ID_HOTKEY_UNPIN) {
                if (IsWindowVisible(hWnd)) {
                    g_pinMode = false;
                    g_miniPositionValid = false;
                    if (g_hPinBtn) InvalidateRect(g_hPinBtn, nullptr, TRUE);
                    HideAppWindow();
                } else {
                    g_pinMode = true;
                    g_miniPositionValid = false;
                    if (g_hPinBtn) InvalidateRect(g_hPinBtn, nullptr, TRUE);

                    g_sessionActiveTaskIds = g_store.active_order;
                    g_sessionActiveIdsInitialized = true;
                    g_selectedIndex = -1;

                    EnterMiniMode();
                }
            }
            return 0;
        }

        case WM_EXITSIZEMOVE: {
            if (g_isMiniMode && g_pinMode) {
                RECT rc;
                if (GetWindowRect(hWnd, &rc)) {
                    g_miniWinX = rc.left;
                    g_miniWinY = rc.top;
                    g_miniPositionValid = true;
                }
            }
            return 0;
        }

        case WM_WTSSESSION_CHANGE: {
            if (FocusMode::HandleSessionChange(wParam)) return 0;
            break;
        }

        case WM_APP_TRAY: {
            if (HandleAppTray(hWnd, lParam)) return 0;
            break;
        }

        case WM_KEYDOWN: {
            if (HandleKeyDown(hWnd, wParam)) return 0;
            break;
        }

        case WM_DRAWITEM: {
            if (HandleDrawItem(hWnd, (DRAWITEMSTRUCT*)lParam)) return TRUE;
            break;
        }

        case WM_NOTIFY: {
            if (BarTooltips::HandleNotify(lParam)) return 0;
            break;
        }

        case WM_COMMAND: {
            if (HandleCommand(hWnd, LOWORD(wParam))) return 0;
            break;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            g_scrollY -= (delta / WHEEL_DELTA) * 36;
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            int topOffset = (g_viewMode == ViewMode::ActiveTasks) ? 94 : 88;
            int viewableHeight = rcClient.bottom - topOffset - BOTTOM_BAR_HEIGHT;
            int maxScroll = std::max(0, g_totalContentHeight - viewableHeight);

            if (g_scrollY < 0) g_scrollY = 0;
            if (g_scrollY > maxScroll) g_scrollY = maxScroll;

            UpdateInlineEditPos();
            InvalidateRect(hWnd, nullptr, TRUE);
            return 0;
        }

        case WM_SETCURSOR: {
            if (g_isMiniMode && g_pinMode && LOWORD(lParam) == HTCLIENT) {
                POINT cursorPoint;
                GetCursorPos(&cursorPoint);
                ScreenToClient(hWnd, &cursorPoint);
                RECT rcClient;
                GetClientRect(hWnd, &rcClient);
                RECT rcDragHandle = GetMiniDragHandleRect(rcClient.right);
                if (PtInRect(&rcDragHandle, cursorPoint)) {
                    SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
                    return TRUE;
                }
            }
            break;
        }

        case WM_MOUSEMOVE: {
            if (HandleMouseMove(hWnd, lParam)) return 0;
            break;
        }

        case WM_LBUTTONDOWN: {
            if (HandleLButtonDown(hWnd, lParam)) return 0;
            break;
        }

        case WM_LBUTTONDBLCLK: {
            if (g_isMiniMode) {
                return SendMessageW(hWnd, WM_LBUTTONDOWN, wParam, lParam);
            }
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam) + g_scrollY;
            if (g_viewMode == ViewMode::ActiveTasks) {
                for (size_t i = 0; i < g_displayItems.size(); ++i) {
                    const auto& item = g_displayItems[i];
                    if (item.isHeader) continue;
                    if (PtInRect(&item.checkRect, { mx, my }) ||
                        (item.optionsRect.right > 0 && PtInRect(&item.optionsRect, { mx, my })) ||
                        PtInRect(&item.deleteRect, { mx, my }) ||
                        (item.markerRect.right > 0 && PtInRect(&item.markerRect, { mx, my })) ||
                        (item.assignRect.right > 0 && PtInRect(&item.assignRect, { mx, my })) ||
                        (item.editRect.right > 0 && PtInRect(&item.editRect, { mx, my }))) {
                        return SendMessageW(hWnd, WM_LBUTTONDOWN, wParam, lParam);
                    }
                    if (PtInRect(&item.rect, { mx, my })) {
                        StartInlineEdit((int)i);
                        return 0;
                    }
                }
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (HandleLButtonUp(hWnd, lParam)) return 0;
            break;
        }

        case WM_CAPTURECHANGED: {
            if (g_scrollbarDragging) {
                g_scrollbarDragging = false;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            if (g_taskDragging || g_dragPotentialSourceIndex != -1) {
                g_taskDragging = false;
                g_dragSourceIndex = -1;
                g_dragInsertIndex = -1;
                g_dragPotentialSourceIndex = -1;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            break;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            if (g_isMiniMode) {
                PaintMiniWindow(hWnd, hdc);
            } else {
                PaintMainWindow(hWnd, hdc);
            }
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            ThemeColors th = g_darkMode ? GetDarkTheme() : GetLightTheme();
            SetTextColor(hdcEdit, th.textEdit);
            SetBkColor(hdcEdit, th.bgEdit);
            return (LRESULT)g_hEditBrush;
        }

        case WM_CTLCOLORBTN:
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }

        case WM_DESTROY: {
            FocusMode::UnregisterSessionNotifications(hWnd);
            UnregisterHotKey(hWnd, ID_HOTKEY_TOGGLE);
            UnregisterHotKey(hWnd, ID_HOTKEY_UNPIN);
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            if (g_hIcon) DestroyIcon(g_hIcon);
            if (g_hEditBrush) DeleteObject(g_hEditBrush);
            if (g_hFontTitle) DeleteObject(g_hFontTitle);
            if (g_hFontNormal) DeleteObject(g_hFontNormal);
            if (g_hFontNormalStrike) DeleteObject(g_hFontNormalStrike);
            if (g_hFontSmall) DeleteObject(g_hFontSmall);
            if (g_hFontHeader) DeleteObject(g_hFontHeader);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
