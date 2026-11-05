#pragma once

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace TaskUtils { class TextBuffer; }

namespace WorkHistory {
    using Records = std::map<std::string, int>;

    struct DisplayEntry {
        std::wstring label;
        int seconds;
        int manualSeconds;
        bool isWeeklySummary;
        bool isCurrentDay = false;
    };

    std::string DateKey(time_t timestamp);
    void LoadFromJson(const std::string& content, Records& records, Records& manualRecords);
    void WriteJson(TaskUtils::TextBuffer& out, const Records& records, const Records& manualRecords);
    std::vector<DisplayEntry> GetDisplayEntries(
        const Records& records, const Records& manualRecords, time_t currentDay, int currentSeconds);
}