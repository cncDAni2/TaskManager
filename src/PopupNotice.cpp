#include "PopupNotice.h"
#include "AppState.h"

namespace {
constexpr wchar_t kNoticeClassName[] = L"TaskManager_NewTask_Notice_Class";
constexpr int kNoticeWidth = 340;
constexpr int kNoticeHeight = 132;

LRESULT CALLBACK NoticeWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_NCCREATE: {
            auto createInfo = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(createInfo->lpCreateParams));
            return TRUE;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hWnd, &ps);
            ThemeColors theme = GetThemeColors(g_themeMode);
            RECT client{};
            GetClientRect(hWnd, &client);
            HBRUSH background = CreateSolidBrush(theme.bgWindow);
            FillRect(hdc, &client, background);
            DeleteObject(background);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, theme.borderCardHover);
            HFONT headingFont = CreateFontW(17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HFONT titleFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            RECT headingRect{ 16, 14, client.right - 16, 40 };
            HFONT previousFont = (HFONT)SelectObject(hdc, headingFont);
            DrawTextW(hdc, L"ÚJ FELADAT", -1, &headingRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            SetTextColor(hdc, theme.textPrimary);
            RECT titleRect{ 16, 46, client.right - 16, client.bottom - 12 };
            SelectObject(hdc, titleFont);
            auto taskTitle = reinterpret_cast<const std::wstring*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
            if (taskTitle) {
                DrawTextW(hdc, taskTitle->c_str(), -1, &titleRect,
                    DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
            }

            SelectObject(hdc, previousFont);
            DeleteObject(headingFont);
            DeleteObject(titleFont);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_NCDESTROY: {
            auto taskTitle = reinterpret_cast<std::wstring*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
            delete taskTitle;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
            break;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

BOOL CALLBACK CountNoticeWindows(HWND hWnd, LPARAM countPtr) {
    wchar_t className[64]{};
    if (GetClassNameW(hWnd, className, static_cast<int>(std::size(className))) &&
        wcscmp(className, kNoticeClassName) == 0) {
        ++*reinterpret_cast<int*>(countPtr);
    }
    return TRUE;
}
}

void ShowNewTaskNotice(const std::wstring& taskTitle) {
    static bool classRegistered = false;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!classRegistered) {
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = NoticeWndProc;
        windowClass.hInstance = instance;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
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
    auto title = new std::wstring(taskTitle);
    HWND hNotice = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kNoticeClassName,
        L"Új feladat",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, kNoticeWidth, kNoticeHeight,
        nullptr, nullptr, instance, title);
    if (hNotice) {
        ShowWindow(hNotice, SW_SHOWNOACTIVATE);
        UpdateWindow(hNotice);
    } else {
        delete title;
    }
}