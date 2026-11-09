#pragma once

// GitHub releases/latest lookup via delay-loaded WinHTTP on a thread-pool thread. No timer: it runs
// on startup / showing the window / entering PIN at most every 12 hours, retries a failure on a
// later invocation after 5 minutes (max 3 retries), and posts WM_APP_UPDATE_RESULT. Request(true)
// (the Settings version link) skips the interval and the retry delay.

#include <windows.h>

// Looks up the latest GitHub release (releases/latest -> tag_name) on a thread-pool thread.
// No timer: it runs when the app is brought up (startup, opening the window, entering PIN
// mode) and at most every 12 hours. A failed check is retried on a later invocation after
// 5 minutes, at most 3 times; then it counts as checked. The result is posted back as
// WM_APP_UPDATE_RESULT and stored in tasks-local.json.
namespace UpdateCheck {
constexpr wchar_t DOWNLOAD_URL[] = L"https://github.com/cncDAni2/TaskManager/releases/latest/download/TaskManager.exe";

// force: check now regardless of the interval and retry delay (still not while one is running).
void Request(bool force = false);
void HandleResult(WPARAM wParam, LPARAM lParam);
}
