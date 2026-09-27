#pragma once

#include "AppTypes.h"

bool HandleKeyDown(HWND hWnd, WPARAM wParam);
bool HandleLButtonDown(HWND hWnd, LPARAM lParam);
bool HandleLButtonUp(HWND hWnd, LPARAM lParam);
bool HandleMouseMove(HWND hWnd, LPARAM lParam);
bool HandleCommand(HWND hWnd, int id);
bool HandleDrawItem(HWND hWnd, DRAWITEMSTRUCT* pDIS);
bool HandleAppTray(HWND hWnd, LPARAM lParam);
