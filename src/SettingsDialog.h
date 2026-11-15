#pragma once

// Settings UI for themes, sound, daily work-reset time, timer auto-start and excluded window-title
// keywords, and the Laura button (bottom left, only while Laura is detected); rebuilt in place when sync rows change. Every setting applies and saves instantly
// except the work-reset time, which waits for the highlighted "Mentés" button (the OK button
// changes to it).

#include <windows.h>

void ShowSettingsDialog(HWND owner);
// Updates the "Hangok" checkbox of an open Settings dialog after muting from elsewhere.
void RefreshSettingsSounds();
// Shows, hides or repaints the Laura button of an open Settings dialog (LauraLink state change).
void RefreshSettingsLaura();