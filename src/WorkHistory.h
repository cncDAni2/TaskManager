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
        bool isWeeklySummary;
    };

    std::string DateKey(time_t timestamp);
    bool SeedIfEmpty(Records& records);
    void LoadFromJson(const std::string& content, Records& records);
    void WriteJson(std::ostream& out, const Records& records);
    std::vector<DisplayEntry> GetDisplayEntries(
        const Records& records, time_t currentDay, int currentSeconds);
}