#pragma once

#include "SyncFiles.h"
#include <windows.h>
#include <string>
#include <vector>

// The editable sync-file table of the Settings dialog.
namespace SettingsSyncRows {
    struct Row {
        int sourceId = 0; // 0 for rows added in this dialog session.
        std::wstring name;
        std::wstring path;
        bool isDefault = false;
    };

    struct Controls {
        std::vector<HWND> defaults;
        std::vector<HWND> names;
    };

    constexpr int MAX_ROWS = 8;

    // True for the add button and every per-row control.
    bool OwnsControl(int controlId);

    std::vector<Row> FromSources(const std::vector<SyncSource>& sources);
    std::vector<SyncSource> ToSources(const std::vector<Row>& rows);

    // Creates the group box and its rows between `left` and `right`; returns the group's bottom edge.
    int Create(HWND dialog, HWND tooltip, const std::vector<Row>& rows, Controls& controls,
        int top, int left, int right);

    // Copies the edited names and default checkboxes back into `rows`.
    void Snapshot(const Controls& controls, std::vector<Row>& rows);

    // Handles add/browse/remove buttons. Returns true when the rows changed and the table must be rebuilt.
    bool HandleCommand(HWND dialog, int controlId, std::vector<Row>& rows);
}
