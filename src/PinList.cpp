#include "PinList.h"
#include "AppState.h"
#include "Layout.h"

namespace PinList {
namespace {
constexpr UINT HIDE_DELAY_MS = 2000;

bool g_open = false;     // Mouse mode: the list is out.
bool g_tracking = false; // A WM_MOUSELEAVE is requested.

bool PinWindowOnScreen() {
    return g_hWnd && g_isMiniMode && IsWindowVisible(g_hWnd);
}

void Refresh() {
    if (!PinWindowOnScreen()) return;
    RecalculateMiniLayout();
    InvalidateRect(g_hWnd, nullptr, FALSE);
}

void TrackLeave(HWND hWnd) {
    if (g_tracking) return;
    TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, hWnd, 0 };
    g_tracking = TrackMouseEvent(&track) != FALSE;
}

// Screen pixels: WindowFromPoint respects the window region, so the cut-out corners do not count.
bool CursorOverWindow(HWND hWnd) {
    POINT cursor{};
    if (!GetCursorPos(&cursor)) return false;
    const HWND under = WindowFromPoint(cursor);
    return under && GetAncestor(under, GA_ROOT) == hWnd;
}
}

Mode CurrentMode() {
    const int mode = g_store.pin_list_mode;
    return mode == Never || mode == OnHover ? static_cast<Mode>(mode) : Always;
}

int Limit() {
    return g_store.pin_list_count > 0 ? g_store.pin_list_count : 0;
}

bool IsShown() {
    const Mode mode = CurrentMode();
    return mode == Always || (mode == OnHover && g_open);
}

void Apply(Mode mode, int limit) {
    g_store.pin_list_mode = mode;
    g_store.pin_list_count = limit < 0 ? 0 : limit;
    g_store.SaveLocal();
    if (g_hWnd) Reset(g_hWnd);
    Refresh();
}

void Reset(HWND hWnd) {
    KillTimer(hWnd, IDT_PIN_LIST_HIDE);
    g_open = false;
    g_tracking = false;
}

void HandleMouseMove(HWND hWnd) {
    if (!g_isMiniMode || CurrentMode() != OnHover) return;
    TrackLeave(hWnd);
    KillTimer(hWnd, IDT_PIN_LIST_HIDE);
    if (g_open) return;
    g_open = true;
    Refresh();
}

void HandleMouseLeave(HWND hWnd) {
    g_tracking = false;
    if (g_isMiniMode && g_open) SetTimer(hWnd, IDT_PIN_LIST_HIDE, HIDE_DELAY_MS, nullptr);
}

bool HandleTimer(HWND hWnd, WPARAM timerId) {
    if (timerId != IDT_PIN_LIST_HIDE) return false;
    // A drag keeps the rows it points into; the timer fires again after it.
    if (g_taskDragging || g_dragPotentialSourceIndex != -1) return true;
    KillTimer(hWnd, IDT_PIN_LIST_HIDE);
    if (CursorOverWindow(hWnd)) {
        TrackLeave(hWnd); // Back over it without a move message yet.
        return true;
    }
    g_open = false;
    Refresh();
    return true;
}
}
