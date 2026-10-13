#pragma once
#include <windows.h>

namespace MemoryTrim {
    // Schedules a one-shot working-set trim once the UI has settled.
    void Request(HWND hWnd);
    // Handles IDT_MEMORY_TRIM; returns true when the timer was consumed.
    bool HandleTimer(HWND hWnd, WPARAM timerId);
}
