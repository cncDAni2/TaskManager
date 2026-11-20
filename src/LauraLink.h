#pragma once

// Link to Laura, the user's voice assistant (protocol and connection states in LauraProtocol.h).
// Costs nothing until Laura announces herself: then it watches her process (one thread-pool
// wait, reported back via PostMessage) so a crash is noticed without polling, runs her commands
// one by one from WM_APP_LAURA_COMMAND (never inside her SendMessage), and replies with a short
// text she reads aloud. Say() lets the timer cues speak through Laura instead of beeping.
// laura_enabled (local setting) is the Settings button's on/off; off still detects Laura.
// While connected, a colleague's new shared task and reaching 90% of the daily work time are spoken.

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
// A random wording of `kind`.
std::wstring Announcement(int kind, double restMinutes = 0);
// A colleague's new shared task: Laura reads it out instead of the pop-up, once the user is active
// (not locked, input within 2 minutes). False when she cannot (not connected, sounds off): show the pop-up.
bool AnnounceNewTask(const std::wstring& author, const std::wstring& text);
// Part of the one-second IDT_WORK_TIMER: waiting announcements, and the 90% of the daily work time.
void Tick();
}
