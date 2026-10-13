#pragma once

#include "AppTypes.h"
#include <windows.h>
#include <string>

// The cloud button next to the new-task field: which sync files the next task goes to.
namespace SyncPicker {
    // Restores the selection to the sync files marked as default in Settings.
    void ResetSelection();
    bool HasSources();
    bool IsSelected(int sourceId);
    int SelectedCount();

    // One sync file: toggles it. Several: opens a checklist menu below `button`.
    void HandleClick(HWND owner, HWND button);

    // Supplies the dynamic tooltip of the cloud button; returns true when handled.
    bool HandleTooltipNotify(LPARAM lParam, HWND button);

    // Writes `count` into the cloud drawn by DrawCloudIcon with the same rect.
    void DrawCloudCount(HDC hdc, const RECT& cloudRect, int count, COLORREF text);

    // Names of the sync files that contain `task`, one per line.
    std::wstring SourceNames(const Task& task);

    // True when more than one sync file is configured, so counts are worth showing.
    bool ShowCounts();
}
