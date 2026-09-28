#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace BarTooltips {
    struct Region {
        RECT rect;
        std::wstring text;
    };

    void Update(HWND owner, const std::vector<Region>& regions);
    bool HandleNotify(LPARAM lParam);
}
