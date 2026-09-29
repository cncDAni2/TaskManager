#pragma once

#include "AppTypes.h"
#include <string>
#include <ctime>

namespace TaskUtils {
    std::string WideToUtf8(const std::wstring& wstr);
    std::wstring Utf8ToWide(const std::string& str);
    std::wstring FormatDate(time_t t);
    std::wstring FormatTime(time_t t);
    std::wstring FormatDateTime(time_t t);
    time_t GetYesterday930Cutoff(time_t now);
    time_t GetLastResetCutoff(time_t now, int hour, int minute);
    std::wstring FormatWorkDuration(int totalSeconds);
    std::string EscapeJsonString(const std::string& s);
    std::string UnescapeJsonString(const std::string& s);
    std::wstring GetCleanUserName();
    std::wstring GetDefaultSyncFilePath();
    void EnsureParentDirectoryExists(const std::wstring& path);
}
