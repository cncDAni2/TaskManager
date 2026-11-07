#pragma once

#include "AppTypes.h"
#include <string>

// Where a dragged task or group header would land, for the main window and the PIN window alike.
// In the main window holding a task on the middle of another one for half a second groups them;
// the PIN window only reorders (TaskStore::ResolvePinDrop).
namespace GroupDrag {
struct Line {
    bool visible = false;
    int y = 0; // List coordinates.
    int left = 0;
    int right = 0;
};

// Follows the cursor (list coordinates) during the drag of the row at g_dragSourceIndex.
void Update(HWND hWnd, int x, int y);
const Line& InsertLine();
// The row that a drop would group with, once the hold has passed; -1 otherwise.
int MergeTarget();
// The group whose header is being dragged, or an empty string.
const std::string& DraggedGroup();

// Handles IDT_GROUP_HOLD; returns true when the timer was consumed.
bool HandleTimer(HWND hWnd, WPARAM timerId);
// Applies the drop of the row at `sourceIndex`. Returns the uid of a group this created.
std::string Apply(int sourceIndex);
void Reset(HWND hWnd);
}
