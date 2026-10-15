#pragma once

#include "SyncFiles.h"
#include <string>

// Guesses from a shared sync file whether colleagues run a different app version. There is no
// network check: the hint comes from the writer's version stamp and from members this version
// does not know.
struct VersionNotice {
    std::wstring heading;
    std::wstring body;
    std::wstring footer;
};

namespace VersionHint {
    // Returns false when the file gives no sign of another version.
    bool Describe(const std::wstring& sourceName, const SyncFileInfo& info, VersionNotice& notice);
}
