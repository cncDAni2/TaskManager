#include "ThemedDatePicker.h"
#include "AppState.h"
#include "Recurring.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <algorithm>

namespace {
SYSTEMTIME Today() {
    SYSTEMTIME today{};
    GetLocalTime(&today);
    today.wHour = today.wMinute = today.wSecond = today.wMilliseconds = 0;
    today.wDayOfWeek = 0;
    return today;
}

void Notify(HWND picker) {
    NMDATETIMECHANGE change{};
    change.nmhdr.hwndFrom = picker;
    change.nmhdr.idFrom = static_cast<UINT_PTR>(GetDlgCtrlID(picker));
    change.nmhdr.code = DTN_DATETIMECHANGE;
    change.dwFlags = GDT_VALID;
    DateTime_GetSystemtime(picker, &change.st);
    SendMessageW(GetParent(picker), WM_NOTIFY, change.nmhdr.idFrom, reinterpret_cast<LPARAM>(&change));
}

void Step(HWND picker, RepeatUnit unit, int delta) {
    const std::string date = ThemedDatePicker::GetDate(picker);
    ThemedDatePicker::SetDate(picker, delta == 0 ? Recurring::Today() : Recurring::Occurrence(date, 1, unit, delta));
    InvalidateRect(picker, nullptr, FALSE);
    Notify(picker);
}

void Paint(HWND picker) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(picker, &ps);
    RECT client{};
    GetClientRect(picker, &client);
    const ThemeColors th = GetThemeColors(g_themeMode);
    HBRUSH background = CreateSolidBrush(th.bgEdit);
    FillRect(dc, &client, background);
    DeleteObject(background);

    SYSTEMTIME value{};
    wchar_t text[64] = L"";
    if (DateTime_GetSystemtime(picker, &value) == GDT_VALID) {
        GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &value, nullptr, text, 64, nullptr);
    }
    HGDIOBJ oldFont = SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(picker, WM_GETFONT, 0, 0)));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, th.textEdit);
    RECT textRect = { client.left + 4, client.top, client.right - 22, client.bottom };
    DrawTextW(dc, text, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SetTextColor(dc, th.textSecondary);
    RECT arrowRect = { client.right - 22, client.top, client.right, client.bottom };
    DrawTextW(dc, L"▾", -1, &arrowRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, oldFont);

    HBRUSH frameBrush = CreateSolidBrush(GetFocus() == picker ? th.borderCardSelected : th.textSecondary);
    FrameRect(dc, &client, frameBrush);
    DeleteObject(frameBrush);
    EndPaint(picker, &ps);
}

LRESULT CALLBACK PickerProc(HWND picker, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR /*refData*/) {
    switch (message) {
    case WM_NCDESTROY:
        RemoveWindowSubclass(picker, PickerProc, subclassId);
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        Paint(picker);
        return 0;
    case WM_SETFOCUS:
    case WM_KILLFOCUS: {
        const LRESULT result = DefSubclassProc(picker, message, wParam, lParam);
        InvalidateRect(picker, nullptr, FALSE);
        return result;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        SetFocus(picker);
        PostMessageW(picker, WM_KEYDOWN, VK_F4, 0);
        return 0;
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_UP: Step(picker, RepeatUnit::Day, 1); return 0;
        case VK_DOWN: Step(picker, RepeatUnit::Day, -1); return 0;
        case VK_PRIOR: Step(picker, RepeatUnit::Month, 1); return 0;
        case VK_NEXT: Step(picker, RepeatUnit::Month, -1); return 0;
        case VK_HOME: Step(picker, RepeatUnit::Day, 0); return 0;
        case VK_SPACE: return DefSubclassProc(picker, WM_KEYDOWN, VK_F4, 0);
        case VK_F4: case VK_TAB: case VK_RETURN: break; // Calendar; the form handles Tab and Enter.
        default: return 0; // Field editing would change parts of the date the user cannot see.
        }
        break;
    case WM_CHAR:
        return 0;
    }
    return DefSubclassProc(picker, message, wParam, lParam);
}
}

void ThemedDatePicker::Attach(HWND picker) {
    SetWindowSubclass(picker, PickerProc, 0, 0);
    const SYSTEMTIME today = Today();
    SYSTEMTIME range[2] = { today, {} };
    DateTime_SetRange(picker, GDTR_MIN, range);
}

std::string ThemedDatePicker::GetDate(HWND picker) {
    SYSTEMTIME selected{};
    if (DateTime_GetSystemtime(picker, &selected) != GDT_VALID) return Recurring::Today();
    const std::string date = Recurring::DateKey(selected.wYear, selected.wMonth, selected.wDay);
    return (std::max)(date, Recurring::Today());
}

void ThemedDatePicker::SetDate(HWND picker, const std::string& date) {
    // The range is refreshed too, since "today" moves while the app keeps running.
    SYSTEMTIME value = Today();
    SYSTEMTIME range[2] = { value, {} };
    DateTime_SetRange(picker, GDTR_MIN, range);
    int year = 0, month = 0, day = 0;
    if (date > Recurring::Today() && Recurring::IsValidDate(date) &&
        sscanf_s(date.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
        value.wYear = static_cast<WORD>(year);
        value.wMonth = static_cast<WORD>(month);
        value.wDay = static_cast<WORD>(day);
    }
    DateTime_SetSystemtime(picker, GDT_VALID, &value);
    InvalidateRect(picker, nullptr, FALSE);
}

void ThemedDatePicker::StyleDropDown(HWND picker) {
    HWND calendar = DateTime_GetMonthCal(picker);
    if (!calendar) return;
    // Visual styles ignore the color settings, so the pop-up is drawn classic, in theme colors.
    SetWindowTheme(calendar, L"", L"");
    const ThemeColors th = GetThemeColors(g_themeMode);
    MonthCal_SetColor(calendar, MCSC_BACKGROUND, th.bgEdit);
    MonthCal_SetColor(calendar, MCSC_MONTHBK, th.bgEdit);
    MonthCal_SetColor(calendar, MCSC_TEXT, th.textEdit);
    MonthCal_SetColor(calendar, MCSC_TITLEBK, th.borderCardSelected);
    MonthCal_SetColor(calendar, MCSC_TITLETEXT, RGB(255, 255, 255));
    MonthCal_SetColor(calendar, MCSC_TRAILINGTEXT, th.textSecondary);
    // The classic calendar needs a different size than the themed one it replaced.
    RECT needed{};
    if (MonthCal_GetMinReqRect(calendar, &needed)) {
        HWND popup = GetParent(calendar);
        RECT popupRect{};
        GetWindowRect(popup, &popupRect);
        RECT calendarRect{};
        GetWindowRect(calendar, &calendarRect);
        const int extraW = (popupRect.right - popupRect.left) - (calendarRect.right - calendarRect.left);
        const int extraH = (popupRect.bottom - popupRect.top) - (calendarRect.bottom - calendarRect.top);
        SetWindowPos(calendar, nullptr, 0, 0, needed.right - needed.left, needed.bottom - needed.top,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        if (popup && popup != picker) {
            SetWindowPos(popup, nullptr, 0, 0, needed.right - needed.left + extraW, needed.bottom - needed.top + extraH,
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
}
