#include "GroupView.h"
#include "AppState.h"
#include "Task.h"

namespace GroupView {
namespace {
constexpr int kHeaderHeight = 20;
constexpr int kMainBorder = 2;
constexpr int kMainPadding = 6;   // Below the header and the last row.
constexpr int kMainSpacing = 6;   // Below the frame, as below a row.
constexpr int kPinPadding = 3;
constexpr int kPinSpacing = 4;
constexpr int kNameMargin = 8;

std::vector<Frame> g_frames;

void DrawBorder(HDC hdc, RECT rect, int width, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    const RECT sides[] = {
        { rect.left, rect.top, rect.right, rect.top + width },
        { rect.left, rect.bottom - width, rect.right, rect.bottom },
        { rect.left, rect.top, rect.left + width, rect.bottom },
        { rect.right - width, rect.top, rect.right, rect.bottom },
    };
    for (const RECT& side : sides) FillRect(hdc, &side, brush);
    DeleteObject(brush);
}
}

void BeginLayout() {
    g_frames.clear();
}

const std::vector<Frame>& Frames() {
    return g_frames;
}

int OpenMain(const std::string& uid, int y, int clientW) {
    const TaskGroup* group = g_store.FindGroup(uid);
    DisplayItem header;
    header.isHeader = true;
    header.groupUid = uid;
    if (group) header.headerText = group->name;
    header.rect = { 10, y, clientW - 10, y + kHeaderHeight };

    Frame frame;
    frame.rect = { header.rect.left, y, header.rect.right, y };
    frame.uid = uid;
    g_frames.push_back(std::move(frame));
    g_displayItems.push_back(std::move(header));
    return y + kHeaderHeight + kMainPadding - 2;
}

int CloseMain(int y) {
    RECT& rect = g_frames.back().rect;
    rect.bottom = y - MAIN_GAP + kMainPadding;
    return rect.bottom + kMainSpacing;
}

int OpenPin(const std::string& uid, int y, int miniW) {
    Frame frame;
    frame.rect = { 6, y, miniW - 6, y };
    frame.uid = uid;
    g_frames.push_back(std::move(frame));
    return y + kPinPadding;
}

int ClosePin(int y) {
    RECT& rect = g_frames.back().rect;
    rect.bottom = y - PIN_GAP + kPinPadding;
    return rect.bottom + kPinSpacing;
}

void PaintMain(HDC hdc, int scrollY, const ThemeColors& th, const std::string& highlightUid) {
    SetBkMode(hdc, TRANSPARENT);
    for (const Frame& frame : g_frames) {
        RECT rect = frame.rect;
        OffsetRect(&rect, 0, -scrollY);
        const COLORREF color = frame.uid == highlightUid ? th.borderCardSelected : th.groupFrame;
        DrawBorder(hdc, rect, kMainBorder, color);

        RECT header = { rect.left, rect.top, rect.right, rect.top + kHeaderHeight };
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(hdc, &header, brush);
        DeleteObject(brush);

        const TaskGroup* group = g_store.FindGroup(frame.uid);
        if (!group) continue;
        RECT name = { header.left + kNameMargin, header.top, header.right - kNameMargin, header.bottom };
        SelectObject(hdc, g_hFontHeader);
        SetTextColor(hdc, th.groupHeaderText);
        DrawTextW(hdc, group->name.c_str(), -1, &name,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
}

void PaintPin(HDC hdc, const ThemeColors& th) {
    const COLORREF color = g_themeMode == ThemeMode::Pink ? RGB(0, 0, 0)
        : (g_themeMode == ThemeMode::Dark ? RGB(0xAA, 0xAA, 0xAA) : th.groupFrame);
    for (const Frame& frame : g_frames) DrawBorder(hdc, frame.rect, 1, color);
}
}
