#pragma once

#include <windows.h>

// Looks up the latest GitHub release (releases/latest -> tag_name) on a thread-pool thread.
// No timer: it runs when the app is brought up (startup, opening the window, entering PIN
// mode) and at most every 12 hours. A failed check is retried on a later invocation after
// 5 minutes, at most 3 times; then it counts as checked. The result is posted back as
// WM_APP_UPDATE_RESULT and stored in tasks-local.json.
namespace UpdateCheck {
constexpr wchar_t DOWNLOAD_URL[] = L"https://github.com/cncDAni2/TaskManager/releases/latest/download/TaskManager.exe";

void Request();
void HandleResult(WPARAM wParam, LPARAM lParam);
}
