#include "PopupNotice.h"
#include "AppState.h"
#include <iterator>

namespace {
constexpr wchar_t kNoticeClassName[] = L"TaskManager_NewTask_Notice_Class";
constexpr int kNoticeWidth = 340;
constexpr int kNoticeHeight = 132;
constexpr UINT_PTR kCountdownTimer = 1;
constexpr UINT kCountdownTickMs = 100;
constexpr int kAutoCloseMs = 10000;
constexpr int kProgressHeight = 4;

struct Notice {
    std::wstring heading;
    std::wstring body;
    std::wstring footer;
    bool autoClose = false;
    int remainingMs = kAutoCloseMs;
    bool hovered = false;
    DWORD lastTick = 0;
};

HFONT NoticeFont(int height, int weight) {
    return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

RECT ProgressRect(HWND hWnd) {
    RECT client{};
    GetClientRect(hWnd, &client);
    return { 0, client.bottom - kProgressHeight, client.right, client.bottom };
}

void Paint(HWND hWnd, HDC hdc, const Notice& notice) {
    static HFONT headingFont = NoticeFont(17, FW_BOLD);
    static HFONT bodyFont = NoticeFont(16, FW_NORMAL);
    static HFONT footerFont = NoticeFont(14, FW_NORMAL);

    ThemeColors theme = GetThemeColors(g_themeMode);
    RECT client{};
    GetClientRect(hWnd, &client);
    HBRUSH background = CreateSolidBrush(theme.bgWindow);
    FillRect(hdc, &client, background);
    DeleteObject(background);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, theme.borderCardHover);
    HGDIOBJ previousFont = SelectObject(hdc, headingFont);
    RECT headingRect{ 16, 12, client.right - 16, 36 };
    DrawTextW(hdc, notice.heading.c_str(), -1, &headingRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    const int footerHeight = notice.footer.empty() ? 0 : 20;
    SetTextColor(hdc, theme.textPrimary);
    SelectObject(hdc, bodyFont);
    RECT bodyRect{ 16, 40, client.right - 16, client.bottom - 10 - footerHeight - kProgressHeight };
    DrawTextW(hdc, notice.body.c_str(), -1, &bodyRect,
        DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX | DT_EDITCONTROL);

    if (footerHeight > 0) {
        SetTextColor(hdc, theme.textSecondary);
        SelectObject(hdc, footerFont);
        RECT footerRect{ 16, bodyRect.bottom + 2, client.right - 16, bodyRect.bottom + 2 + footerHeight };
        DrawTextW(hdc, notice.footer.c_str(), -1, &footerRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    if (notice.autoClose) {
        // The bar shrinks towards the left as the notice is about to close.
        RECT progress = ProgressRect(hWnd);
        progress.right = progress.left + MulDiv(progress.right - progress.left, notice.remainingMs, kAutoCloseMs);
        HBRUSH bar = CreateSolidBrush(theme.borderCardHover);
        FillRect(hdc, &progress, bar);
        DeleteObject(bar);
    }
    SelectObject(hdc, previousFont);
}

void TrackHover(HWND hWnd, Notice* notice, bool nonClient) {
    if (notice->hovered) return;
    notice->hovered = true;
    TRACKMOUSEEVENT track{ sizeof(track) };
    track.dwFlags = TME_LEAVE | (nonClient ? TME_NONCLIENT : 0);
    track.hwndTrack = hWnd;
    TrackMouseEvent(&track);
}

LRESULT CALLBACK NoticeWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* notice = reinterpret_cast<Notice*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    switch (msg) {
        case WM_NCCREATE: {
            auto createInfo = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(createInfo->lpCreateParams));
            return TRUE;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hWnd, &ps);
            if (notice) Paint(hWnd, hdc, *notice);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_TIMER: {
            if (wParam != kCountdownTimer || !notice) break;
            const DWORD now = GetTickCount();
            if (!notice->hovered) {
                notice->remainingMs -= static_cast<int>(now - notice->lastTick);
                if (notice->remainingMs <= 0) {
                    DestroyWindow(hWnd);
                    return 0;
                }
                RECT progress = ProgressRect(hWnd);
                InvalidateRect(hWnd, &progress, FALSE);
            }
            notice->lastTick = now;
            return 0;
        }

        case WM_MOUSEMOVE:
        case WM_NCMOUSEMOVE:
            if (notice) TrackHover(hWnd, notice, msg == WM_NCMOUSEMOVE);
            break;

        case WM_MOUSELEAVE:
        case WM_NCMOUSELEAVE: {
            // Moving between the caption and the client area leaves one and enters the other.
            POINT cursor{};
            GetCursorPos(&cursor);
            RECT window{};
            GetWindowRect(hWnd, &window);
            if (notice && !PtInRect(&window, cursor)) {
                notice->hovered = false;
                notice->lastTick = GetTickCount();
            } else if (notice) {
                notice->hovered = false;
                TrackHover(hWnd, notice, msg == WM_MOUSELEAVE);
            }
            break;
        }

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_NCLBUTTONDOWN:
        case WM_NCRBUTTONDOWN:
            DestroyWindow(hWnd);
            return 0;

        case WM_NCDESTROY: {
            KillTimer(hWnd, kCountdownTimer);
            delete notice;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
            break;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

BOOL CALLBACK CountNoticeWindows(HWND hWnd, LPARAM countPtr) {
    wchar_t className[64]{};
    if (GetClassNameW(hWnd, className, static_cast<int>(std::size(className))) &&
        wcscmp(className, kNoticeClassName) == 0 && IsWindowVisible(hWnd)) {
        ++*reinterpret_cast<int*>(countPtr);
    }
    return TRUE;
}

void ShowNotice(Notice* notice, const wchar_t* caption) {
    static bool classRegistered = false;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!classRegistered) {
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = NoticeWndProc;
        windowClass.hInstance = instance;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_HAND);
        windowClass.lpszClassName = kNoticeClassName;
        RegisterClassW(&windowClass);
        classRegistered = true;
    }

    int existingNotices = 0;
    EnumWindows(CountNoticeWindows, reinterpret_cast<LPARAM>(&existingNotices));

    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int x = workArea.right - kNoticeWidth - 14;
    int y = workArea.top + 14 + existingNotices * (kNoticeHeight + 10);
    HWND hNotice = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kNoticeClassName,
        caption,
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, kNoticeWidth, kNoticeHeight,
        nullptr, nullptr, instance, notice);
    if (!hNotice) {
        delete notice;
        return;
    }
    if (notice->autoClose) {
        notice->lastTick = GetTickCount();
        SetTimer(hNotice, kCountdownTimer, kCountdownTickMs, nullptr);
    }
    ShowWindow(hNotice, SW_SHOWNOACTIVATE);
    UpdateWindow(hNotice);
}
}

void ShowNewTaskNotice(const std::wstring& taskTitle) {
    auto* notice = new Notice();
    notice->heading = L"ÚJ FELADAT";
    notice->body = taskTitle;
    ShowNotice(notice, L"Új feladat");
}

void ShowTaskChangedNotice(const std::wstring& newText, const std::wstring& previousText,
    const std::wstring& changedBy) {
    auto* notice = new Notice();
    notice->heading = changedBy.empty() ? L"MÓDOSÍTOTT FELADAT" : L"MÓDOSÍTOTTA: " + changedBy;
    notice->body = newText;
    notice->footer = L"Korábban: " + previousText;
    notice->autoClose = true;
    ShowNotice(notice, L"Módosított feladat");
}

void ShowVersionNotice(const std::wstring& heading, const std::wstring& body, const std::wstring& footer) {
    auto* notice = new Notice();
    notice->heading = heading;
    notice->body = body;
    notice->footer = footer;
    ShowNotice(notice, L"Eltérő verzió");
}
