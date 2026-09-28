#pragma once

#include <windows.h>
#include <wtsapi32.h>

namespace FocusMode {
void RegisterSessionNotifications(HWND hWnd);
void UnregisterSessionNotifications(HWND hWnd);
bool HandleSessionChange(WPARAM event);
void Toggle();
}