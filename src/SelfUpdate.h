#pragma once

#include <windows.h>

// In-place update from the overlay's "Frissítés" button. A thread-pool thread downloads the
// release's TaskManager.exe beside the running one as "TaskManager.exe.new" and checks it (PE
// x64 image, full length, version resource = the expected release). Back on the UI thread the
// running exe is renamed to ".old" (Windows allows renaming a running image), ".new" takes its
// name, the new exe is started with "--updated <pid>" and this instance closes. The new one
// waits for the old process to exit before anything else (it loads tasks-local.json in a
// global constructor) and deletes ".old". Any failure rolls the files back and opens the
// download page in the browser instead.
namespace SelfUpdate {
bool IsBusy();
// Starts the download; false when it could not be started.
bool Start(HWND owner);
// WM_APP_SELF_UPDATE_RESULT: wParam != 0 means the downloaded file passed the checks.
void HandleResult(HWND hWnd, WPARAM wParam);
}
