#pragma once

#include "AppTypes.h"

LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK InlineEditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

void StartInlineEdit(int itemIndex);
// The same edit box over the header of a task group: Enter or clicking away saves, Esc cancels.
void StartGroupRename(const std::string& groupUid);
void CommitInlineEdit();
void CancelInlineEdit();
void UpdateInlineEditPos();
