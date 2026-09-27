#pragma once

#include "AppTypes.h"

void ToggleWindow();
void ShowAppWindow();
void HideAppWindow();
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
