#pragma once

// Focus-mode toggling and Windows session lock/unlock notifications. Focus mode overrides
// idle/excluded-app pauses, but session lock always stops counting and disables focus mode;
// unlocking does not re-enable it. Keep the session-notification registration and unregistration
// paired.

#include <windows.h>
#include <wtsapi32.h>

namespace FocusMode {
void RegisterSessionNotifications(HWND hWnd);
void UnregisterSessionNotifications(HWND hWnd);
bool HandleSessionChange(WPARAM event);
void Toggle();
}