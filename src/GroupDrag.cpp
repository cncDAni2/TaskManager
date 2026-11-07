#include "GroupDrag.h"
#include "AppState.h"
#include "GroupView.h"
#include "Task.h"
#include <algorithm>

namespace GroupDrag {
namespace {
constexpr UINT kHoldMs = 500;

struct Target {
    int beforeId = 0;     // 0 = the end of the list.
    std::string groupUid; // The group the task belongs to after the drop.
};

Target g_target;
Line g_line;
int g_holdRow = -1;  // Row under the cursor's middle zone, while the hold runs or has passed.
bool g_holdDone = false;
std::string g_draggedGroup;

int ListRight() {
    RECT client{};
    UiScale::ClientRect(g_hWnd, &client);
    return client.right - 14;
}

Line TopLevelLine(int y) {
    return { true, y, g_isMiniMode ? 8 : 10, g_isMiniMode ? ListRight() + 6 : ListRight() };
}

const GroupView::Frame* FrameAt(int y) {
    for (const GroupView::Frame& frame : GroupView::Frames()) {
        if (y >= frame.rect.top && y < frame.rect.bottom) return &frame;
    }
    return nullptr;
}

const GroupView::Frame* FrameOf(const std::string& uid) {
    for (const GroupView::Frame& frame : GroupView::Frames()) {
        if (frame.uid == uid) return &frame;
    }
    return nullptr;
}

// The row after the last member of the frame's group; the size of the list when there is none.
size_t RowAfter(const std::string& uid) {
    size_t after = g_displayItems.size();
    for (size_t i = g_displayItems.size(); i-- > 0;) {
        const DisplayItem& item = g_displayItems[i];
        if (item.task.group_uid == uid || item.groupUid == uid) break;
        after = i;
    }
    return after;
}

int FirstTaskIdAt(size_t row) {
    if (row >= g_displayItems.size()) return 0;
    if (g_displayItems[row].isHeader) ++row; // A frame starts with its header.
    return row < g_displayItems.size() ? g_displayItems[row].task.id : 0;
}

int ListEndLine() {
    if (g_displayItems.empty()) return 0;
    int bottom = g_displayItems.back().rect.bottom;
    const auto& frames = GroupView::Frames();
    if (!frames.empty()) bottom = (std::max)(bottom, static_cast<int>(frames.back().rect.bottom));
    return bottom + 3;
}

// A slot among the rows of a frame.
void TargetInFrame(const GroupView::Frame& frame, int y) {
    g_target.groupUid = frame.uid;
    const DisplayItem* last = nullptr;
    for (const DisplayItem& item : g_displayItems) {
        if (item.isHeader || item.task.group_uid != frame.uid) continue;
        if (y < (item.rect.top + item.rect.bottom) / 2) {
            g_target.beforeId = item.task.id;
            g_line = { true, item.rect.top - 2, item.rect.left, item.rect.right };
            return;
        }
        last = &item;
    }
    g_target.beforeId = FirstTaskIdAt(RowAfter(frame.uid));
    if (last) g_line = { true, last->rect.bottom + 1, last->rect.left, last->rect.right };
}

// A slot between the rows and frames of the list. Frames count as one block.
void TargetTopLevel(int y) {
    g_target.groupUid.clear();
    for (size_t i = 0; i < g_displayItems.size();) {
        const DisplayItem& item = g_displayItems[i];
        const GroupView::Frame* frame = item.isHeader ? FrameOf(item.groupUid) : nullptr;
        const RECT block = frame ? frame->rect : item.rect;
        if (y < (block.top + block.bottom) / 2) {
            g_target.beforeId = FirstTaskIdAt(i);
            g_line = TopLevelLine(block.top - 3);
            return;
        }
        i = frame ? RowAfter(frame->uid) : i + 1;
    }
    g_target.beforeId = 0;
    g_line = TopLevelLine(ListEndLine());
}

void UpdatePinTarget(const DisplayItem& source, int y) {
    int beforeId = 0;
    for (const DisplayItem& item : g_displayItems) {
        if (y < (item.rect.top + item.rect.bottom) / 2) {
            beforeId = item.task.id;
            break;
        }
    }
    bool wholeGroup = false;
    g_target.beforeId = g_store.ResolvePinDrop(g_sessionActiveTaskIds, source.task.id, beforeId, wholeGroup);
    g_target.groupUid = wholeGroup ? "" : source.task.group_uid;

    // The target may be hidden here: the line goes above the next task that is shown.
    auto position = g_target.beforeId
        ? std::find(g_sessionActiveTaskIds.begin(), g_sessionActiveTaskIds.end(), g_target.beforeId)
        : g_sessionActiveTaskIds.end();
    for (; position != g_sessionActiveTaskIds.end(); ++position) {
        for (const DisplayItem& item : g_displayItems) {
            if (item.task.id != *position) continue;
            const GroupView::Frame* frame = FrameOf(item.task.group_uid);
            const bool inOwnGroup = !source.task.group_uid.empty() && !wholeGroup &&
                item.task.group_uid == source.task.group_uid;
            g_line = TopLevelLine((frame && !inOwnGroup ? frame->rect.top : item.rect.top) - 2);
            return;
        }
    }
    g_line = TopLevelLine(ListEndLine() - 1);
}

void StopHold(HWND hWnd) {
    if (g_holdRow != -1 && !g_holdDone) KillTimer(hWnd, IDT_GROUP_HOLD);
    g_holdRow = -1;
    g_holdDone = false;
}

// The row whose middle the cursor is on, when dropping there could group the two tasks.
int HoldRowAt(const DisplayItem& source, int x, int y) {
    if (source.task.completed) return -1;
    for (size_t i = 0; i < g_displayItems.size(); ++i) {
        const DisplayItem& item = g_displayItems[i];
        if (item.isHeader || item.task.id == source.task.id || item.task.completed) continue;
        const int quarter = (item.rect.bottom - item.rect.top) / 4;
        if (x < item.rect.left || x >= item.rect.right ||
            y < item.rect.top + quarter || y >= item.rect.bottom - quarter) continue;
        const bool sameGroup = !source.task.group_uid.empty() && source.task.group_uid == item.task.group_uid;
        return sameGroup ? -1 : static_cast<int>(i);
    }
    return -1;
}
}

void Update(HWND hWnd, int x, int y) {
    if (g_dragSourceIndex < 0 || g_dragSourceIndex >= static_cast<int>(g_displayItems.size())) return;
    const DisplayItem& source = g_displayItems[g_dragSourceIndex];
    const Line oldLine = g_line;
    const int oldMerge = MergeTarget();

    if (g_isMiniMode) {
        UpdatePinTarget(source, y);
    } else if (source.isHeader) {
        g_draggedGroup = source.groupUid;
        TargetTopLevel(y);
    } else {
        const GroupView::Frame* frame = FrameAt(y);
        if (frame) TargetInFrame(*frame, y);
        else TargetTopLevel(y);

        const int holdRow = HoldRowAt(source, x, y);
        if (holdRow != g_holdRow) {
            StopHold(hWnd);
            g_holdRow = holdRow;
            if (holdRow != -1) SetTimer(hWnd, IDT_GROUP_HOLD, kHoldMs, nullptr);
        }
    }

    if (oldLine.y != g_line.y || oldLine.left != g_line.left || oldLine.right != g_line.right ||
        oldLine.visible != g_line.visible || oldMerge != MergeTarget()) {
        InvalidateRect(hWnd, nullptr, FALSE);
    }
}

const Line& InsertLine() {
    static const Line hidden;
    return MergeTarget() != -1 ? hidden : g_line;
}

int MergeTarget() {
    return g_holdDone ? g_holdRow : -1;
}

const std::string& DraggedGroup() {
    return g_draggedGroup;
}

bool HandleTimer(HWND hWnd, WPARAM timerId) {
    if (timerId != IDT_GROUP_HOLD) return false;
    KillTimer(hWnd, IDT_GROUP_HOLD);
    if (g_taskDragging && g_holdRow != -1) {
        g_holdDone = true;
        InvalidateRect(hWnd, nullptr, FALSE);
    }
    return true;
}

std::string Apply(int sourceIndex) {
    if (sourceIndex < 0 || sourceIndex >= static_cast<int>(g_displayItems.size())) return "";
    const DisplayItem& source = g_displayItems[sourceIndex];
    if (!g_line.visible) return ""; // The mouse never reached a slot.

    std::string created;
    std::vector<int>& order = g_sessionActiveTaskIds;
    const int mergeRow = MergeTarget();
    if (source.isHeader) {
        g_store.MoveGroup(order, source.groupUid, g_target.beforeId);
    } else if (mergeRow != -1) {
        created = g_store.GroupWith(order, source.task.id, g_displayItems[mergeRow].task.id);
    } else if (g_isMiniMode && g_target.groupUid != source.task.group_uid) {
        g_store.MoveGroup(order, source.task.group_uid, g_target.beforeId); // Leaving: all of it moves.
    } else {
        g_store.PlaceTask(order, source.task.id, g_target.beforeId, g_target.groupUid);
    }
    g_store.active_order = order;
    g_store.SaveLocal();
    return created;
}

void Reset(HWND hWnd) {
    StopHold(hWnd);
    g_line = {};
    g_target = {};
    g_draggedGroup.clear();
}
}
