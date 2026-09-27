#include "InlineEdit.h"
#include "AppState.h"
#include "Layout.h"
#include "MainWindow.h"
#include <commctrl.h>
#include <cctype>

LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR /*dwRefData*/) {
    if (uMsg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            SendMessageW(g_hWnd, WM_COMMAND, MAKEWPARAM(IDC_ADD_TASK_BTN, BN_CLICKED), 0);
            return 0;
        } else if (wParam == VK_ESCAPE) {
            if (g_pinMode && !g_isMiniMode) {
                EnterMiniMode();
            } else if (!g_pinMode) {
                HideAppWindow();
            }
            return 0;
        } else if (wParam == VK_TAB) {
            if (!g_displayItems.empty()) {
                g_selectedIndex = -1;
                for (size_t i = 0; i < g_displayItems.size(); ++i) {
                    if (!g_displayItems[i].isHeader) {
                        g_selectedIndex = (int)i;
                        break;
                    }
                }
                if (g_selectedIndex != -1) {
                    EnsureVisible(g_selectedIndex);
                    SetFocus(g_hWnd);
                    InvalidateRect(g_hWnd, nullptr, FALSE);
                    return 0;
                }
            }
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK InlineEditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR /*dwRefData*/) {
    if (uMsg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            CommitInlineEdit();
            return 0;
        } else if (wParam == VK_ESCAPE) {
            CancelInlineEdit();
            return 0;
        }
    } else if (uMsg == WM_KILLFOCUS) {
        CommitInlineEdit();
        return 0;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void StartInlineEdit(int itemIndex) {
    if (itemIndex < 0 || itemIndex >= (int)g_displayItems.size()) return;
    const auto& targetItem = g_displayItems[itemIndex];
    if (targetItem.isHeader || targetItem.task.completed) return;

    if (g_editingTaskId == targetItem.task.id) {
        return;
    }

    int targetTaskId = targetItem.task.id;
    CommitInlineEdit();

    int resolvedIndex = -1;
    for (size_t i = 0; i < g_displayItems.size(); ++i) {
        if (!g_displayItems[i].isHeader && g_displayItems[i].task.id == targetTaskId) {
            resolvedIndex = (int)i;
            break;
        }
    }
    if (resolvedIndex == -1) return;

    const auto& item = g_displayItems[resolvedIndex];
    g_editingTaskId = item.task.id;
    g_selectedIndex = resolvedIndex;

    RECT r = item.textRect;
    r.top -= g_scrollY;
    r.bottom -= g_scrollY;
    int editH = 24;
    int editY = r.top + (r.bottom - r.top - editH) / 2;

    HINSTANCE hInst = GetModuleHandle(nullptr);
    g_hInlineEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", item.task.text.c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        r.left - 2, editY, (r.right - r.left) + 4, editH,
        g_hWnd, (HMENU)IDC_INLINE_EDIT, hInst, nullptr);

    SendMessageW(g_hInlineEdit, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    SendMessageW(g_hInlineEdit, EM_SETSEL, 0, -1);
    SetWindowSubclass(g_hInlineEdit, InlineEditSubclassProc, 0, 0);
    SetFocus(g_hInlineEdit);
}

void CommitInlineEdit() {
    if (!g_hInlineEdit || g_editingTaskId == -1) return;

    HWND hEdit = g_hInlineEdit;
    g_hInlineEdit = nullptr;
    int taskId = g_editingTaskId;
    g_editingTaskId = -1;
    g_lastCommittedTaskId = taskId;
    g_lastCommitTick = GetTickCount64();

    int len = GetWindowTextLengthW(hEdit);
    std::vector<wchar_t> buf(len + 1);
    GetWindowTextW(hEdit, buf.data(), len + 1);
    DestroyWindow(hEdit);

    std::wstring text = buf.data();
    while (!text.empty() && iswspace(text.front())) text.erase(text.begin());
    while (!text.empty() && iswspace(text.back())) text.pop_back();

    if (!text.empty()) {
        g_store.UpdateText(taskId, text);
    }
    RecalculateLayout();
    SetFocus(g_hWnd);
    InvalidateRect(g_hWnd, nullptr, TRUE);
}

void CancelInlineEdit() {
    if (!g_hInlineEdit) return;
    HWND hEdit = g_hInlineEdit;
    g_hInlineEdit = nullptr;
    g_editingTaskId = -1;
    g_lastCommittedTaskId = -1;
    DestroyWindow(hEdit);
    SetFocus(g_hWnd);
    InvalidateRect(g_hWnd, nullptr, TRUE);
}

void UpdateInlineEditPos() {
    if (g_hInlineEdit && g_selectedIndex >= 0 && g_selectedIndex < (int)g_displayItems.size()) {
        RECT r = g_displayItems[g_selectedIndex].textRect;
        r.top -= g_scrollY;
        r.bottom -= g_scrollY;
        int editH = 24;
        int editY = r.top + (r.bottom - r.top - editH) / 2;
        SetWindowPos(g_hInlineEdit, nullptr, r.left - 2, editY, (r.right - r.left) + 4, editH, SWP_NOZORDER);
    }
}
