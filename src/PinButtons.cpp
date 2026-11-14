#include "PinButtons.h"
#include "AppState.h"
#include "AudioCue.h"
#include "Drawing.h"
#include "FocusMode.h"
#include "Hotkeys.h"
#include "IntervalTimer.h"
#include "Layout.h"
#include "MainWindow.h"
#include "PinList.h"
#include "TimerHold.h"

namespace PinButtons {
namespace {
constexpr const char* KEYS[COUNT] = { "timer", "focus", "mute", "hold", "pin", "drag", "add" };
constexpr COLORREF ACTIVE_BG = RGB(30, 58, 138); // Same blue as the main window's toggles.
constexpr int GAP = 3;

struct Placed {
    Id id;
    RECT rect;
};

bool IsVisible(Id id) {
    return id != Timer || g_intervalTimer.IsActive() || g_intervalTimer.IsVisionBreakActive();
}

bool IsOn(Id id) {
    switch (id) {
    case Focus: return g_focusMode;
    case Mute: return !g_store.sounds_enabled;
    case Hold: return TimerHold::IsActive();
    case Pin: return g_pinMode;
    default: return false;
    }
}

// Visible buttons in configured order (left to right), placed from the right edge leftwards.
std::vector<Placed> Place(int clientWidth) {
    const Config config = Current();
    std::vector<Placed> placed;
    int right = clientWidth - 4;
    for (auto it = config.rbegin(); it != config.rend(); ++it) {
        if (!it->enabled || !IsVisible(it->id)) continue;
        const int width = it->id == Timer ? 38 : (it->id == Drag ? 28 : 30);
        const RECT rect = it->id == Timer ? RECT{ right - width, STRIP_BOTTOM - 14, right, STRIP_BOTTOM }
            : RECT{ right - width, 0, right, STRIP_BOTTOM };
        placed.insert(placed.begin(), { it->id, rect });
        right -= width + GAP;
    }
    // Without the task list an empty strip would leave nothing to show that PIN mode is on.
    if (placed.empty() && PinList::CurrentMode() != PinList::Always) {
        placed.push_back({ Status, RECT{ right - 30, 0, right, STRIP_BOTTOM } });
    }
    return placed;
}

COLORREF IdleIconColor() {
    return g_themeMode == ThemeMode::Pink ? RGB(0, 0, 0)
        : (g_darkMode ? RGB(212, 212, 216) : RGB(71, 85, 105));
}

void PaintTimer(HDC hdc, const RECT& rc) {
    const bool workPhase = g_intervalTimer.IsWorkPhase();
    const bool held = TimerHold::IsActive();
    const COLORREF fill = held ? RGB(107, 114, 128) : (workPhase ? RGB(220, 38, 38) : RGB(37, 99, 235));
    const COLORREF border = held ? RGB(75, 85, 99) : (workPhase ? RGB(185, 28, 28) : RGB(29, 78, 216));
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 5, 5);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
    HGDIOBJ oldFont = SelectObject(hdc, g_hFontMiniTimer);
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    const std::wstring remaining = held ? L"ÁLL"
        : (g_intervalTimer.IsVisionBreakActive() ? L"👁️" : FormatTimerTime(g_intervalTimer.SecondsRemaining()));
    RECT textRect = rc;
    DrawTextW(hdc, remaining.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
}

void DrawPlusIcon(HDC hdc, const RECT& rc, COLORREF color) {
    const int cx = (rc.left + rc.right) / 2;
    const int cy = (rc.top + rc.bottom) / 2;
    // Two filled bars of the same size around the same center: equal arms in every direction.
    HBRUSH brush = CreateSolidBrush(color);
    const RECT across = { cx - 6, cy - 1, cx + 6, cy + 1 };
    const RECT down = { cx - 1, cy - 6, cx + 1, cy + 6 };
    FillRect(hdc, &across, brush);
    FillRect(hdc, &down, brush);
    DeleteObject(brush);
}

// A ring with a dot in its middle.
void DrawStatusIcon(HDC hdc, const RECT& rc, COLORREF color) {
    const int cx = (rc.left + rc.right) / 2;
    const int cy = (rc.top + rc.bottom) / 2 + 1;
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, cx - 6, cy - 6, cx + 7, cy + 7);
    SelectObject(hdc, brush);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, cx - 2, cy - 2, cx + 4, cy + 4);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawIcon(HDC hdc, const RECT& rc, Id id, COLORREF color, bool muted) {
    switch (id) {
    case Timer: DrawTimerIcon(hdc, rc, color); break;
    case Focus: DrawFocusIcon(hdc, rc, color); break;
    case Mute: DrawSpeakerIcon(hdc, rc, color, muted); break;
    case Hold: DrawHandIcon(hdc, rc, color); break;
    case Pin: DrawPinIcon(hdc, rc, color, true); break;
    case Drag: DrawGripIcon(hdc, rc, color); break;
    case Add: DrawPlusIcon(hdc, rc, color); break;
    case Status: DrawStatusIcon(hdc, rc, color); break;
    default: break;
    }
}

std::wstring TooltipText(Id id) {
    switch (id) {
    case Timer: return g_intervalTimer.IsActive() ? L"Időzítő megnyitása" : L"20-20-20 szünet";
    case Focus: return g_focusMode ? L"Fókusz mód kikapcsolása"
        : L"Fókusz mód bekapcsolása\nTétlenség és kizárt ablakok alatt is méri a munkaidőt.";
    case Mute: return (g_store.sounds_enabled ? L"Hangok némítása" : L"Hangok visszakapcsolása") +
        Hotkeys::Suffix(Hotkeys::ActionMute);
    case Hold: return TimerHold::MenuLabel();
    case Pin: return L"PIN mód kikapcsolása" + Hotkeys::Suffix(Hotkeys::ActionPin) +
        L"\nA kitűzött ablak bezárul, a program a tálcán fut tovább.";
    case Drag: return L"Húzással az ablak áthelyezhető";
    case Add: return L"Új feladat\nMegnyílik a főablak, és rögtön írhatod a feladatot.";
    case Status: {
        const std::wstring combo = Hotkeys::Format(g_store.hotkeys[Hotkeys::ActionPin]);
        return L"PIN mód aktív, kikapcsolása: " + (combo.empty() ? L"főablakból vagy a tálca menüjéből" : combo) +
            L"\nKattintásra megnyílik a főablak.";
    }
    default: return L"";
    }
}
}

Config Parse(const std::string& text) {
    Config config{};
    bool used[COUNT] = {};
    size_t count = 0;
    size_t start = 0;
    while (start <= text.size() && count < COUNT) {
        size_t end = text.find(',', start);
        if (end == std::string::npos) end = text.size();
        std::string key = text.substr(start, end - start);
        const bool enabled = key.empty() || key[0] != '-';
        if (!enabled) key.erase(0, 1);
        for (int i = 0; i < COUNT; ++i) {
            if (!used[i] && key == KEYS[i]) {
                used[i] = true;
                config[count++] = { static_cast<Id>(i), enabled };
                break;
            }
        }
        start = end + 1;
    }
    // Buttons missing from the text (older or hand-edited files) come hidden: the new-task button
    // right after the timer, like for new users, the others at the end.
    for (int i = 0; i < COUNT; ++i) {
        if (used[i]) continue;
        size_t at = count;
        if (i == Add) {
            for (size_t j = 0; j < count; ++j) {
                if (config[j].id == Timer) at = j + 1;
            }
        }
        for (size_t j = count; j > at; --j) config[j] = config[j - 1];
        config[at] = { static_cast<Id>(i), false };
        ++count;
    }
    return config;
}

std::string Serialize(const Config& config) {
    std::string text;
    for (const Slot& slot : config) {
        if (!text.empty()) text += ',';
        if (!slot.enabled) text += '-';
        text += KEYS[slot.id];
    }
    return text;
}

Config Current() {
    return Parse(g_store.pin_buttons);
}

void Apply(const Config& config) {
    g_store.pin_buttons = Serialize(config);
    g_store.SaveLocal();
    if (g_hWnd && g_isMiniMode && IsWindowVisible(g_hWnd)) {
        RecalculateMiniLayout();
        InvalidateRect(g_hWnd, nullptr, FALSE);
    }
}

const wchar_t* Name(Id id) {
    switch (id) {
    case Timer: return L"Időinformációk (időzítő)";
    case Focus: return L"Fókusz mód";
    case Mute: return L"Némítás";
    case Hold: return L"Időstop";
    case Pin: return L"PIN mód ki/be";
    case Drag: return L"Ablak mozgatása (fogantyú)";
    case Add: return L"Új feladat";
    default: return L"";
    }
}

bool IsOnScreen(Id id) {
    if (!g_hWnd || !g_isMiniMode || !IsWindowVisible(g_hWnd)) return false;
    for (const Slot& slot : Current()) {
        if (slot.id == id) return slot.enabled;
    }
    return false;
}

void DrawPreviewIcon(HDC hdc, const RECT& rc, Id id, COLORREF color) {
    DrawIcon(hdc, rc, id, color, true);
}

bool GetRect(int clientWidth, Id id, RECT& rect) {
    for (const Placed& placed : Place(clientWidth)) {
        if (placed.id != id) continue;
        rect = placed.rect;
        return true;
    }
    return false;
}

void AddToRegion(HRGN region, int clientWidth) {
    for (const Placed& placed : Place(clientWidth)) {
        const RECT r = UiScale::Px(placed.rect);
        // Rounded like the painted shapes, so no square corners stick out above the window. Tabs
        // reach into the body so only their top corners are rounded.
        HRGN part = placed.id == Timer
            ? CreateRoundRectRgn(r.left, r.top, r.right + 1, r.bottom + 1, UiScale::Px(5), UiScale::Px(5))
            : CreateRoundRectRgn(r.left, r.top, r.right + 1, r.bottom + UiScale::Px(8), UiScale::Px(6), UiScale::Px(6));
        CombineRgn(region, region, part, RGN_OR);
        DeleteObject(part);
    }
}

// Idle buttons have no fill or border of their own: they blend into the strip.
void Paint(HDC hdc, int clientWidth) {
    for (const Placed& placed : Place(clientWidth)) {
        if (placed.id == Timer) {
            PaintTimer(hdc, placed.rect);
            continue;
        }
        const bool on = IsOn(placed.id);
        if (on) {
            // Rounded top, flat bottom on the window's edge: drawn taller and clipped at the edge.
            const RECT& r = placed.rect;
            const int saved = SaveDC(hdc);
            IntersectClipRect(hdc, r.left, r.top, r.right, r.bottom);
            HBRUSH brush = CreateSolidBrush(ACTIVE_BG);
            HPEN pen = CreatePen(PS_SOLID, 1, ACTIVE_BG);
            SelectObject(hdc, brush);
            SelectObject(hdc, pen);
            RoundRect(hdc, r.left, r.top, r.right, r.bottom + 8, 6, 6);
            RestoreDC(hdc, saved);
            DeleteObject(pen);
            DeleteObject(brush);
        }
        DrawIcon(hdc, placed.rect, placed.id, on ? RGB(255, 255, 255) : IdleIconColor(), on);
    }
}

void AddTooltips(std::vector<BarTooltips::Region>& regions, int clientWidth) {
    for (const Placed& placed : Place(clientWidth)) {
        regions.push_back({ placed.rect, TooltipText(placed.id) });
    }
}

bool HandleClick(HWND hWnd, POINT point, int clientWidth) {
    for (const Placed& placed : Place(clientWidth)) {
        if (!PtInRect(&placed.rect, point)) continue;
        switch (placed.id) {
        case Timer:
            if (!g_intervalTimer.IsActive()) return false; // The 20-20-20 badge just drags the window.
            g_viewMode = ViewMode::IntervalTimer;
            ShowAppWindow();
            return true;
        case Focus:
            FocusMode::Toggle();
            return true;
        case Mute:
            AudioCue::ToggleSounds();
            return true;
        case Hold:
            TimerHold::Toggle();
            return true;
        case Pin:
            g_pinMode = false;
            g_miniPositionValid = false;
            if (g_hPinBtn) InvalidateRect(g_hPinBtn, nullptr, TRUE);
            HideAppWindow();
            return true;
        case Drag:
            ReleaseCapture();
            SendMessageW(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return true;
        case Add:
            ToggleWindow(); // The main window opens with the new-task box focused.
            return true;
        case Status:
            ToggleWindow(); // Like a click on the empty area below the strip.
            return true;
        default:
            return false;
        }
    }
    return false;
}
}
