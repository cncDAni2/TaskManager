#pragma once

// Non-activating notices: new tasks (stay until clicked) and changed tasks (auto-close countdown,
// paused on hover).

#include <string>

// Stays until clicked or closed.
void ShowNewTaskNotice(const std::wstring& taskTitle);

// Closes itself after a countdown (paused while hovered); any click closes it.
void ShowTaskChangedNotice(const std::wstring& newText, const std::wstring& previousText,
    const std::wstring& changedBy);

// Stays until clicked or closed; tells that a colleague probably runs another app version.
void ShowVersionNotice(const std::wstring& heading, const std::wstring& body, const std::wstring& footer);
