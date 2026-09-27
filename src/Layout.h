#pragma once

#include "AppTypes.h"

void RecalculateLayout();
void RecalculateMiniLayout();
void EnterMiniMode();
void ExitMiniMode(bool toHidden = false);
void UpdateControlsVisibility();
void EnsureVisible(int itemIndex);
ScrollbarMetrics GetScrollbarMetrics();
