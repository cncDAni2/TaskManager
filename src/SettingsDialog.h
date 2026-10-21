#pragma once

#include <windows.h>

void ShowSettingsDialog(HWND owner);
// Updates the "Hangok" checkbox of an open Settings dialog after muting from elsewhere.
void RefreshSettingsSounds();