#include "AppTypes.h"
#include "AppState.h"
#include "MainWindow.h"
#include "Layout.h"
#include <commctrl.h>
#include <set>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "wtsapi32.lib")

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"TaskManager_SingleInstance_Mutex_98741");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(L"TaskManager_Popup_Window_Class", nullptr);
        if (hExisting) {
            SendMessageW(hExisting, WM_HOTKEY, ID_HOTKEY_TOGGLE, 0);
        }
        return 0;
    }

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES | ICC_DATE_CLASSES;
    InitCommonControlsEx(&icc);

    g_themeMode = g_store.theme_mode;
    g_darkMode = g_themeMode == ThemeMode::Dark;

    WNDCLASSW wc{};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"TaskManager_Popup_Window_Class";
    RegisterClassW(&wc);

    RECT rcWork;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);
    int defaultW = 620;
    int defaultH = 550;
    int defaultX = rcWork.right - defaultW - 12;
    int defaultY = rcWork.bottom - defaultH - 12;

    HWND hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"Feladatkezelő",
        WS_POPUP | WS_BORDER | WS_CLIPCHILDREN,
        defaultX, defaultY, defaultW, defaultH,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd) return 0;

    g_fullWinX = defaultX;
    g_fullWinY = defaultY;
    g_fullWinW = defaultW;
    g_fullWinH = defaultH;

    g_sessionActiveTaskIds = g_store.active_order;
    g_sessionActiveIdsInitialized = true;
    g_selectedIndex = -1;

    EnterMiniMode();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE &&
            (!g_hInlineEdit || msg.hwnd != g_hInlineEdit)) {
            if (g_pinMode && !g_isMiniMode) {
                EnterMiniMode();
            } else if (!g_pinMode) {
                HideAppWindow();
            }
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
    return (int)msg.wParam;
}
