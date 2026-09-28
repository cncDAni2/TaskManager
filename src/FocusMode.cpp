#include "FocusMode.h"
#include "AppState.h"

namespace FocusMode {
void RegisterSessionNotifications(HWND hWnd) {
    WTSRegisterSessionNotification(hWnd, NOTIFY_FOR_THIS_SESSION);
}

void UnregisterSessionNotifications(HWND hWnd) {
    WTSUnRegisterSessionNotification(hWnd);
}

bool HandleSessionChange(WPARAM event) {
    if (event == WTS_SESSION_LOCK) {
        g_isSessionLocked = true;
        g_focusMode = false;
        g_isWorkActive = false;
        if (g_hFocusModeBtn) InvalidateRect(g_hFocusModeBtn, nullptr, TRUE);
        if (g_hWnd) InvalidateRect(g_hWnd, nullptr, FALSE);
        return true;
    }
    if (event == WTS_SESSION_UNLOCK) {
        g_isSessionLocked = false;
        return true;
    }
    return false;
}

void Toggle() {
    g_focusMode = !g_focusMode;
    if (g_hFocusModeBtn) InvalidateRect(g_hFocusModeBtn, nullptr, TRUE);
    if (g_hWnd) InvalidateRect(g_hWnd, nullptr, FALSE);
}
}