#pragma once

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <ostream>

namespace WorkHistory {
    using Records = std::map<std::string, int>;

    struct DisplayEntry {
        std::wstring label;
        int seconds;
        int manualSeconds;
        bool isWeeklySummary;
    };

    std::string DateKey(time_t timestamp);
    void LoadFromJson(const std::string& content, Records& records, Records& manualRecords);
    void WriteJson(std::ostream& out, const Records& records, const Records& manualRecords);
    std::vector<DisplayEntry> GetDisplayEntries(
        const Records& records, const Records& manualRecords, time_t currentDay, int currentSeconds);
}