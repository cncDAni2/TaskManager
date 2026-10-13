#include "MemoryTrim.h"
#include "AppTypes.h"

namespace {
constexpr UINT TRIM_DELAY_MS = 2000;
}

void MemoryTrim::Request(HWND hWnd) {
    // Re-arming the timer debounces bursts of transitions into a single trim.
    SetTimer(hWnd, IDT_MEMORY_TRIM, TRIM_DELAY_MS, nullptr);
}

bool MemoryTrim::HandleTimer(HWND hWnd, WPARAM timerId) {
    if (timerId != IDT_MEMORY_TRIM) return false;
    KillTimer(hWnd, IDT_MEMORY_TRIM);
    HeapCompact(GetProcessHeap(), 0);
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    return true;
}
