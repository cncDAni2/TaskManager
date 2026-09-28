#include "BarTooltips.h"
#include <commctrl.h>

namespace BarTooltips {
    namespace {
        HWND tooltip = nullptr;
        HWND tooltipOwner = nullptr;
        std::vector<Region> activeRegions;

        bool SameRect(const RECT& left, const RECT& right) {
            return left.left == right.left && left.top == right.top &&
                left.right == right.right && left.bottom == right.bottom;
        }
    }

    void Update(HWND owner, const std::vector<Region>& regions) {
        bool unchanged = owner == tooltipOwner && regions.size() == activeRegions.size();
        if (unchanged) {
            for (size_t i = 0; i < regions.size(); ++i) {
                if (!SameRect(regions[i].rect, activeRegions[i].rect) ||
                    regions[i].text != activeRegions[i].text) {
                    unchanged = false;
                    break;
                }
            }
        }
        if (unchanged) return;

        if (tooltip && tooltipOwner) {
            for (size_t i = 0; i < activeRegions.size(); ++i) {
                TOOLINFOW tool{};
                tool.cbSize = sizeof(tool);
                tool.hwnd = tooltipOwner;
                tool.uId = i + 1;
                SendMessageW(tooltip, TTM_DELTOOLW, 0, (LPARAM)&tool);
            }
        }
        activeRegions = regions;
        tooltipOwner = owner;
        if (!tooltip || !IsWindow(tooltip)) {
            tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                owner, nullptr, GetModuleHandleW(nullptr), nullptr);
            if (tooltip) SendMessageW(tooltip, TTM_SETMAXTIPWIDTH, 0, 300);
        }
        if (!tooltip) return;

        for (size_t i = 0; i < activeRegions.size(); ++i) {
            TOOLINFOW tool{};
            tool.cbSize = sizeof(tool);
            tool.uFlags = TTF_SUBCLASS;
            tool.hwnd = owner;
            tool.uId = i + 1;
            tool.rect = activeRegions[i].rect;
            tool.lpszText = LPSTR_TEXTCALLBACKW;
            SendMessageW(tooltip, TTM_ADDTOOLW, 0, (LPARAM)&tool);
        }
    }

    bool HandleNotify(LPARAM lParam) {
        auto* header = reinterpret_cast<NMHDR*>(lParam);
        if (!header || header->hwndFrom != tooltip || header->code != TTN_GETDISPINFOW) return false;
        auto* info = reinterpret_cast<NMTTDISPINFOW*>(lParam);
        if (header->idFrom == 0 || header->idFrom > activeRegions.size()) return false;
        info->lpszText = activeRegions[header->idFrom - 1].text.data();
        return true;
    }
}
