#pragma once

#include <windows.h>
#include <string>

// Search in the completed view: the magnifier beside the cleanup controls swaps them for a search
// field. While it is open the list is filtered as you type and nothing is struck through.
namespace TaskSearch {
    void CreateControls(HWND parent, HINSTANCE instance);
    bool IsActive();
    bool Matches(const std::wstring& text);
    // `cleanupLeft`: left edge of the cleanup controls, the magnifier goes beside them.
    void UpdateVisibility(int clientWidth, int cleanupLeft);
    bool HandleCommand(WPARAM wParam);
    void Close();
    void Reset(); // Like Close, without touching the layout (the window is about to be rebuilt).
    void FocusField();
}
