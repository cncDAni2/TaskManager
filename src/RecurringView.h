#pragma once

#include "AppTypes.h"

// "Ütemezett feladatok" page: form for recurring / later tasks (opened by the repeat button next
// to "+ Hozzáad") and the list of saved rules.
namespace RecurringView {
    void CreateControls(HWND owner, HINSTANCE instance);
    // Opens the page; text typed into the new-task field is taken over.
    void Open(HWND owner);
    // Shows the form while the page is open, and the repeat button in the active view.
    void UpdateVisibility(bool pageVisible);
    int ListTop();
    void RecalculateLayout(int clientWidth);
    void PaintForm(HDC hdc, int clientWidth);
    void PaintList(HDC hdc, int clientWidth, int listTop, int listBottom);
    bool HandleCommand(HWND owner, WPARAM wParam);
    bool HandleNotify(LPARAM lParam);
    void HandleClick(HWND owner, int x, int rawY);
    void HandleMouseMove(HWND owner, int x, int rawY);
    bool HandleDrawItem(const DRAWITEMSTRUCT* dis);
    void FocusForm();
    // A date or unit drop-down is open: Escape must close it instead of hiding the window.
    bool IsDropDownOpen();
    // Puts tasks created by a work-day rollover on top of the active list.
    void TakeNewTasks(HWND owner);
}
