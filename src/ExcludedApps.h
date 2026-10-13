#pragma once
#include <string>

namespace ExcludedApps {
    // Default comma-separated keywords used when no setting has been saved yet.
    extern const wchar_t DEFAULT_KEYWORDS[];

    // Normalizes user input: trims items, drops empty ones, and joins them with ", ".
    std::wstring Normalize(const std::wstring& keywords);

    // Returns the first keyword (as the user typed it) contained in the window title,
    // compared case-insensitively; empty when nothing matches.
    std::wstring FindMatch(const std::wstring& keywords, const std::wstring& windowTitle);
}
