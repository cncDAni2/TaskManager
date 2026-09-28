#pragma once

#include <windows.h>
#include <string>
#include "WorkHistory.h"

bool ShowManualWorkDialog(HWND owner, const WorkHistory::Records& manualRecords,
	std::string& date, int& seconds);
