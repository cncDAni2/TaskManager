#pragma once

#include "AppTypes.h"

void RecalculateLayout();
void RecalculateMiniLayout();
void EnterMiniMode();
void ExitMiniMode(bool toHidden = false);
void UpdateControlsVisibility();
void EnsureVisible(int itemIndex);
int GetListBottom(int clientHeight);
ScrollbarMetrics GetScrollbarMetrics();
RECT GetMiniDragHandleRect(int clientWidth);
RECT GetMiniFocusButtonRect(int clientWidth);
RECT GetMiniTimerTextRect(int clientWidth);
RECT GetTimerPanelRect(int clientWidth);
