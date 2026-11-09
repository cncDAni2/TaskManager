#pragma once

// In-place update. A thread-pool thread downloads releases/download/v<latest>/TaskManager.exe as
// TaskManager.exe.new and checks it (x64 PE, Content-Length, version resource = expected release);
// the UI thread renames the running exe to .old, .new to TaskManager.exe, starts it with --updated
// <pid> and closes (rolls back on any failure).
// Startup: a #pragma init_seg(lib) hook waits for that pid to exit before g_store loads (and before
// the mutex check), then deletes .old.
// A failure shows its reason on the overlay; after BROWSER_AFTER failures in a row the overlay
// offers the browser download instead. Releases must keep the asset name TaskManager.exe and a
// correct version resource, or old clients fail the update.

#include <windows.h>

// In-place update from the overlay's "Frissítés" button. A thread-pool thread downloads the
// release's TaskManager.exe beside the running one as "TaskManager.exe.new" and checks it (PE
// x64 image, full length, version resource = the expected release). Back on the UI thread the
// running exe is renamed to ".old" (Windows allows renaming a running image), ".new" takes its
// name, the new exe is started with "--updated <pid>" and this instance closes. The new one
// waits for the old process to exit before anything else (it loads tasks-local.json in a
// global constructor) and deletes ".old". A failure leaves the files as before and keeps the
// overlay up with its reason, for a retry.
namespace SelfUpdate {
constexpr int BROWSER_AFTER = 2;
bool IsBusy();
// Reason of the last failed attempt; nullptr when none (or after ResetFailure).
const wchar_t* FailureText();
// True after BROWSER_AFTER failures in a row: the overlay button opens the browser download.
bool OfferBrowser();
// Forgets failures: on dismiss, browser fallback, or when a different release is reported.
void ResetFailure();
// Starts the download; false when it could not be started.
bool Start(HWND owner);
// WM_APP_SELF_UPDATE_RESULT carries the download/validation result.
void HandleResult(HWND hWnd, WPARAM wParam);
}
