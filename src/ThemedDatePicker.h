#pragma once

#include "AppTypes.h"
#include <string>

// A system date picker drawn in the app's theme colors (the control itself always paints white).
// Its own fields are not shown, so the keys work on the whole date: Up/Down = ±1 day,
// PgUp/PgDn = ±1 month, Home = today; a click, Space or F4 opens the calendar. Earliest date: today.
namespace ThemedDatePicker {
    void Attach(HWND picker);
    std::string GetDate(HWND picker);                 // YYYY-MM-DD, never before today.
    void SetDate(HWND picker, const std::string& date); // Past or invalid dates become today.
    // Colors the calendar pop-up; call on DTN_DROPDOWN.
    void StyleDropDown(HWND picker);
}
