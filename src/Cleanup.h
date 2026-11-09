#pragma once

// Cleanup bar below the header in the completed (old tasks, days) and work-history (old work time,
// weeks) views, with confirmation; remembered values cleanup_days / cleanup_weeks.

#include <windows.h>

// "Takarítás": deletes old completed tasks (Elkészült view) and old work history (Idők view)
// via a number field and a button floating in the top-right corner of the list.
namespace Cleanup {
    void CreateControls(HWND parent, HINSTANCE instance);
    // Shows the controls (top-right corner of the list, floating above it) in the views that
    // have them and fills in the view's label and value.
    void UpdateVisibility(int clientWidth);
    // Surface behind the button, for its rounded corners.
    COLORREF SurfaceColor();
    bool HandleCommand(HWND hWnd, WPARAM wParam);
}
