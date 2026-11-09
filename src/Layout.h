#pragma once

// Window geometry, mini-mode transitions, control visibility and custom scrollbar metrics.
// GetListTop() / GetListBottom() are the single source of the list area for every view.
// - The task list uses an integrated 5px custom GDI scrollbar inside list bounds rather than
//   standard non-client scrollbars (WS_VSCROLL).
// - Remove WS_EX_LAYERED when returning to full mode to avoid rendering artifacts with child
//   controls and double buffering.
// - g_fullWinW / g_fullWinH are units, g_fullWinX / g_fullWinY and g_miniWinX / g_miniWinY real
//   pixels (see UiScale.h).
// - Mini mode anchors to the bottom-right of g_fullWinX / g_fullWinY / g_fullWinW / g_fullWinH,
//   constrained to SPI_GETWORKAREA to stay above the taskbar across monitors.

#include "AppTypes.h"

void RecalculateLayout();
void RecalculateMiniLayout();
void EnterMiniMode();
void ExitMiniMode(bool toHidden = false);
void FinalizeCompletedTaskSession();
void UpdateControlsVisibility();
void EnsureVisible(int itemIndex);
int GetListTop();
int GetListBottom(int clientHeight);
ScrollbarMetrics GetScrollbarMetrics();
RECT GetTimerPanelRect(int clientWidth);
