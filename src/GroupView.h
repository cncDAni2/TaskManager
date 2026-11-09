#pragma once

// Group frames: Layout opens and closes them around the rows (the header is an isHeader row with
// groupUid; the PIN window gets a frame only), and they are painted under the cards.
// Renaming is StartGroupRename in InlineEdit (g_editingTaskId is -2 meanwhile).

#include "AppTypes.h"
#include <string>
#include <vector>

// The frames of task groups: Layout builds them while it lays out the rows, Paint draws them.
// Rects are in list coordinates, like DisplayItem::rect.
namespace GroupView {
struct Frame {
    RECT rect{};
    std::string uid;
};

constexpr int MAIN_INSET = 7;  // Rows in a frame are this much narrower on both sides.
constexpr int MAIN_GAP = 3;    // Between rows in a frame.
constexpr int PIN_INSET = 3;
constexpr int PIN_GAP = 2;

void BeginLayout();
const std::vector<Frame>& Frames();

// Open*: starts a frame at `y` and returns the y of its first row. Close*: `y` is where the next
// row would come; returns where the row after the frame goes.
int OpenMain(const std::string& uid, int y, int clientW);
int CloseMain(int y);
int OpenPin(const std::string& uid, int y, int miniW);
int ClosePin(int y);

// Under the rows, so a frame never covers a card. `highlightUid`: the group being dragged.
void PaintMain(HDC hdc, int scrollY, const ThemeColors& th, const std::string& highlightUid);
void PaintPin(HDC hdc, const ThemeColors& th);
}
