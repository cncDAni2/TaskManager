#include "PinButtonsEditor.h"
#include "PinButtons.h"
#include <commctrl.h>
#include <windowsx.h>
#include <algorithm>
#include <cstdlib>
#include <string>

namespace PinButtonsEditor {
namespace {
constexpr wchar_t CLASS_NAME[] = L"TaskManager_PinButtons_Editor";
constexpr int SLOT_W = 34;
constexpr int SLOT_H = 28;
constexpr int GAP = 6;
constexpr int COUNT = PinButtons::COUNT;
constexpr COLORREF ON_BG = RGB(30, 58, 138);
constexpr COLORREF OFF_BG = RGB(255, 255, 255);
constexpr COLORREF OFF_BORDER = RGB(190, 190, 190);
constexpr COLORREF OFF_ICON = RGB(150, 150, 150);

struct State {
    COLORREF background = RGB(245, 245, 245);
    PinButtons::Config config = PinButtons::Current();
    HWND tooltip = nullptr;
    std::wstring tooltipText;
    int focusIndex = 0;
    int pressIndex = -1; // Slot under the pressed mouse button; follows it while dragging.
    int pressX = 0;
    bool dragging = false;
    bool moved = false;
};

RECT SlotRect(int index) {
    const int left = index * (SLOT_W + GAP);
    return { left, 0, left + SLOT_W, SLOT_H };
}

int HitTest(int x, int y) {
    for (int i = 0; i < COUNT; ++i) {
        RECT rect = SlotRect(i);
        if (PtInRect(&rect, { x, y })) return i;
    }
    return -1;
}

void MoveSlot(State* state, int from, int to) {
    auto& slots = state->config;
    if (from < to) std::rotate(slots.begin() + from, slots.begin() + from + 1, slots.begin() + to + 1);
    else std::rotate(slots.begin() + to, slots.begin() + from, slots.begin() + from + 1);
}

void Paint(HWND hWnd, State* state) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT client{};
    GetClientRect(hWnd, &client);
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bitmap = CreateCompatibleBitmap(hdc, client.right, client.bottom);
    HGDIOBJ oldBitmap = SelectObject(mem, bitmap);
    HBRUSH background = CreateSolidBrush(state->background);
    FillRect(mem, &client, background);
    DeleteObject(background);

    for (int i = 0; i < COUNT; ++i) {
        const PinButtons::Slot& slot = state->config[i];
        const RECT rect = SlotRect(i);
        const bool lifted = state->dragging && i == state->pressIndex;
        HBRUSH brush = CreateSolidBrush(slot.enabled ? ON_BG : OFF_BG);
        HPEN pen = CreatePen(PS_SOLID, lifted ? 2 : 1,
            lifted ? RGB(59, 130, 246) : (slot.enabled ? ON_BG : OFF_BORDER));
        HGDIOBJ oldBrush = SelectObject(mem, brush);
        HGDIOBJ oldPen = SelectObject(mem, pen);
        RoundRect(mem, rect.left, rect.top, rect.right, rect.bottom, 6, 6);
        SelectObject(mem, oldPen);
        SelectObject(mem, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);
        PinButtons::DrawPreviewIcon(mem, rect, slot.id, slot.enabled ? RGB(255, 255, 255) : OFF_ICON);
        if (GetFocus() == hWnd && i == state->focusIndex) {
            RECT focusRect = rect;
            InflateRect(&focusRect, -3, -3);
            DrawFocusRect(mem, &focusRect);
        }
    }

    BitBlt(hdc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(mem);
    EndPaint(hWnd, &ps);
}

void CreateTooltip(HWND hWnd, State* state) {
    state->tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hWnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!state->tooltip) return;
    SendMessageW(state->tooltip, TTM_SETMAXTIPWIDTH, 0, 320);
    for (int i = 0; i < COUNT; ++i) {
        TOOLINFOW tool{};
        tool.cbSize = sizeof(tool);
        tool.uFlags = TTF_SUBCLASS;
        tool.hwnd = hWnd;
        tool.uId = static_cast<UINT_PTR>(i + 1);
        tool.rect = SlotRect(i);
        tool.lpszText = LPSTR_TEXTCALLBACKW;
        SendMessageW(state->tooltip, TTM_ADDTOOLW, 0, (LPARAM)&tool);
    }
}

void Toggle(HWND hWnd, State* state, int index) {
    state->config[index].enabled = !state->config[index].enabled;
    PinButtons::Apply(state->config);
    InvalidateRect(hWnd, nullptr, FALSE);
}

void EndPress(HWND hWnd, State* state) {
    if (state->moved) PinButtons::Apply(state->config);
    state->pressIndex = -1;
    state->dragging = false;
    state->moved = false;
    InvalidateRect(hWnd, nullptr, FALSE);
}

LRESULT CALLBACK EditorProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<State*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        state = static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (!state) return DefWindowProcW(hWnd, message, wParam, lParam);

    switch (message) {
    case WM_CREATE:
        CreateTooltip(hWnd, state);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        Paint(hWnd, state);
        return 0;
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(LoadCursorW(nullptr, state->dragging ? IDC_SIZEWE : IDC_HAND));
            return TRUE;
        }
        break;
    case WM_LBUTTONDOWN: {
        SetFocus(hWnd);
        const int index = HitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (index < 0) return 0;
        state->focusIndex = index;
        state->pressIndex = index;
        state->pressX = GET_X_LPARAM(lParam);
        state->dragging = false;
        state->moved = false;
        SetCapture(hWnd);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (state->pressIndex < 0 || GetCapture() != hWnd) return 0;
        const int x = GET_X_LPARAM(lParam);
        if (!state->dragging && std::abs(x - state->pressX) > 4) {
            state->dragging = true;
            if (state->tooltip) SendMessageW(state->tooltip, TTM_POP, 0, 0);
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        if (!state->dragging) return 0;
        const int target = std::clamp((x + GAP / 2) / (SLOT_W + GAP), 0, COUNT - 1);
        if (target != state->pressIndex) {
            MoveSlot(state, state->pressIndex, target);
            state->pressIndex = target;
            state->focusIndex = target;
            state->moved = true;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (state->pressIndex < 0) return 0;
        const bool click = !state->dragging &&
            HitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)) == state->pressIndex;
        const int index = state->pressIndex;
        EndPress(hWnd, state);
        if (click) Toggle(hWnd, state, index);
        if (GetCapture() == hWnd) ReleaseCapture();
        return 0;
    }
    case WM_CAPTURECHANGED:
        if (state->pressIndex >= 0) EndPress(hWnd, state); // Capture lost mid-drag.
        return 0;
    case WM_KEYDOWN: {
        const bool ctrl = GetKeyState(VK_CONTROL) < 0;
        if (wParam == VK_LEFT || wParam == VK_RIGHT) {
            const int target = std::clamp(state->focusIndex + (wParam == VK_LEFT ? -1 : 1), 0, COUNT - 1);
            if (target == state->focusIndex) return 0;
            if (ctrl) {
                MoveSlot(state, state->focusIndex, target);
                PinButtons::Apply(state->config);
            }
            state->focusIndex = target;
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (wParam == VK_SPACE) {
            Toggle(hWnd, state, state->focusIndex);
            return 0;
        }
        break;
    }
    case WM_NOTIFY: {
        auto* header = reinterpret_cast<NMHDR*>(lParam);
        if (header && header->hwndFrom == state->tooltip && header->code == TTN_GETDISPINFOW &&
            header->idFrom >= 1 && header->idFrom <= COUNT) {
            const PinButtons::Slot& slot = state->config[header->idFrom - 1];
            state->tooltipText = std::wstring(PinButtons::Name(slot.id)) +
                (slot.enabled ? L": látszik" : L": rejtve") +
                L"\nKattintás: megjelenítés / elrejtés. Húzással a sorrend módosítható.";
            reinterpret_cast<NMTTDISPINFOW*>(lParam)->lpszText = state->tooltipText.data();
            return 0;
        }
        break;
    }
    case WM_NCDESTROY:
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        delete state;
        return 0;
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}
}

HWND Create(HWND parent, int x, int y, int id, COLORREF background) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = EditorProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = CLASS_NAME;
        registered = RegisterClassW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        if (!registered) return nullptr;
    }
    auto* state = new State();
    state->background = background;
    HWND editor = CreateWindowExW(0, CLASS_NAME, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        x, y, COUNT * SLOT_W + (COUNT - 1) * GAP, SLOT_H, parent, (HMENU)(INT_PTR)id,
        GetModuleHandleW(nullptr), state);
    return editor;
}
}
