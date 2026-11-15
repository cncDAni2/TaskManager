#pragma once

// Link to Laura, the user's voice assistant (protocol and connection states in LauraProtocol.h).
// Costs nothing until Laura announces herself: then it watches her process (one thread-pool
// wait, reported back via PostMessage) so a crash is noticed without polling, runs her commands
// one by one from WM_APP_LAURA_COMMAND (never inside her SendMessage), and replies with a short
// text she reads aloud. Say() lets the timer cues speak through Laura instead of beeping.
// laura_enabled (local setting) is the Settings button's on/off; off still detects Laura.

#include <windows.h>
#include <string>

namespace LauraLink {
// End of WM_CREATE: says Hello to a Laura that is already running.
void Start(HWND hWnd);
// WM_DESTROY: Bye, so Laura does not report a lost link.
void Stop();
// Handles the handshake message, WM_COPYDATA and the WM_APP_LAURA_* messages.
bool HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT& result);

bool IsDetected();
bool IsConnected();
bool IsEnabled();
// The Settings button: switches the link off (Bye) or on again (Hello).
void ToggleEnabled();

// Asks Laura to read `text` aloud; false when not connected or she did not take it (then beep).
bool Say(const char* event, const std::wstring& text);
}
