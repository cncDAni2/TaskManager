#pragma once

#include <string>

// Stays until clicked or closed.
void ShowNewTaskNotice(const std::wstring& taskTitle);

// Closes itself after a countdown (paused while hovered); any click closes it.
void ShowTaskChangedNotice(const std::wstring& newText, const std::wstring& previousText,
    const std::wstring& changedBy);
