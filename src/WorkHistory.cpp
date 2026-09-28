#include "WorkHistory.h"
#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>

namespace WorkHistory {
    namespace {
        struct DayEntry {
            std::string date;
            std::wstring label;
            int seconds;
            int manualSeconds;
            bool isSunday;
        };

        struct WeekEntries {
            int weekNumber = 0;
            long long totalSeconds = 0;
            long long manualSeconds = 0;
            std::vector<DayEntry> days;
        };

        bool GetWeekInfo(const std::string& date, std::string& weekStart, int& weekNumber, bool& isSunday) {
            int year = 0;
            int month = 0;
            int day = 0;
            if (date.size() != 10 || sscanf_s(date.c_str(), "%4d-%2d-%2d", &year, &month, &day) != 3) {
                return false;
            }

            tm localDate{};
            localDate.tm_year = year - 1900;
            localDate.tm_mon = month - 1;
            localDate.tm_mday = day;
            localDate.tm_hour = 12;
            if (mktime(&localDate) == static_cast<time_t>(-1)) return false;
            isSunday = localDate.tm_wday == 0;

            tm monday = localDate;
            monday.tm_mday -= (localDate.tm_wday + 6) % 7;
            time_t mondayTime = mktime(&monday);
            if (mondayTime == static_cast<time_t>(-1)) return false;
            weekStart = DateKey(mondayTime);

            tm thursday = localDate;
            int isoWeekday = (localDate.tm_wday + 6) % 7 + 1;
            thursday.tm_mday += 4 - isoWeekday;
            if (mktime(&thursday) == static_cast<time_t>(-1)) return false;
            weekNumber = thursday.tm_yday / 7 + 1;
            return true;
        }
    }

    std::string DateKey(time_t timestamp) {
        tm localTime{};
        localtime_s(&localTime, &timestamp);
        char buffer[11]{};
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &localTime);
        return buffer;
    }

    void LoadFromJson(const std::string& content, Records& records, Records& manualRecords) {
        size_t historyPos = content.find("\"work_history\":");
        if (historyPos == std::string::npos) return;
        size_t arrayStart = content.find('[', historyPos);
        size_t arrayEnd = (arrayStart == std::string::npos) ? std::string::npos : content.find(']', arrayStart);
        if (arrayEnd == std::string::npos) return;

        size_t position = arrayStart + 1;
        while ((position = content.find('{', position)) != std::string::npos && position < arrayEnd) {
            size_t objectEnd = content.find('}', position);
            if (objectEnd == std::string::npos || objectEnd > arrayEnd) break;
            std::string object = content.substr(position, objectEnd - position + 1);
            size_t datePos = object.find("\"date\"");
            size_t secondsPos = object.find("\"seconds\"");
            size_t manualSecondsPos = object.find("\"manual_seconds\"");
            if (datePos != std::string::npos && secondsPos != std::string::npos) {
                size_t dateStart = object.find('"', object.find(':', datePos) + 1);
                size_t dateEnd = dateStart == std::string::npos ? std::string::npos : object.find('"', dateStart + 1);
                size_t valueStart = object.find_first_of("0123456789", object.find(':', secondsPos) + 1);
                if (dateEnd != std::string::npos && valueStart != std::string::npos) {
                    std::string date = object.substr(dateStart + 1, dateEnd - dateStart - 1);
                    size_t valueEnd = object.find_first_not_of("0123456789", valueStart);
                    int seconds = std::stoi(object.substr(valueStart, valueEnd - valueStart));
                    if (date.size() == 10 && seconds >= 0) {
                        records[date] = seconds;
                        if (manualSecondsPos != std::string::npos) {
                            size_t manualStart = object.find_first_of("0123456789", object.find(':', manualSecondsPos) + 1);
                            if (manualStart != std::string::npos) {
                                size_t manualEnd = object.find_first_not_of("0123456789", manualStart);
                                int manualSeconds = std::stoi(object.substr(manualStart, manualEnd - manualStart));
                                if (manualSeconds > 0) manualRecords[date] = manualSeconds;
                                else manualRecords.erase(date);
                            }
                        }
                    }
                }
            }
            position = objectEnd + 1;
        }
    }

    void WriteJson(std::ostream& out, const Records& records, const Records& manualRecords) {
        out << "  \"work_history\": [";
        bool first = true;
        std::map<std::string, std::pair<int, int>> combined;
        for (const auto& entry : records) combined[entry.first].first = entry.second;
        for (const auto& entry : manualRecords) combined[entry.first].second = entry.second;
        for (const auto& entry : combined) {
            if (entry.second.first <= 0 && entry.second.second <= 0) continue;
            if (!first) out << ", ";
            first = false;
            out << "{\"date\": \"" << entry.first << "\", \"seconds\": " << entry.second.first;
            if (entry.second.second > 0) out << ", \"manual_seconds\": " << entry.second.second;
            out << "}";
        }
        out << "],\n";
    }

    std::vector<DisplayEntry> GetDisplayEntries(
        const Records& records, const Records& manualRecords, time_t currentDay, int currentSeconds) {
        Records displayRecords = records;
        displayRecords[DateKey(currentDay)] = currentSeconds;
        for (const auto& entry : manualRecords) displayRecords.emplace(entry.first, 0);

        std::map<std::string, WeekEntries> weeks;
        for (auto it = displayRecords.rbegin(); it != displayRecords.rend(); ++it) {
            std::string weekStart;
            int weekNumber = 0;
            bool isSunday = false;
            if (!GetWeekInfo(it->first, weekStart, weekNumber, isSunday)) continue;

            WeekEntries& week = weeks[weekStart];
            week.weekNumber = weekNumber;
            int manualSeconds = 0;
            auto manualIt = manualRecords.find(it->first);
            if (manualIt != manualRecords.end()) manualSeconds = std::max(0, manualIt->second);
            int measuredSeconds = std::max(0, it->second);
            if (measuredSeconds == 0 && manualSeconds == 0 && it->first != DateKey(currentDay)) continue;
            week.totalSeconds += measuredSeconds + manualSeconds;
            week.manualSeconds += manualSeconds;

            std::wstring label = std::to_wstring(it->first[5] - '0') + std::to_wstring(it->first[6] - '0') + L"." +
                std::to_wstring(it->first[8] - '0') + std::to_wstring(it->first[9] - '0');
            week.days.push_back({ it->first, label, measuredSeconds + manualSeconds, manualSeconds, isSunday });
        }

        std::vector<DisplayEntry> entries;
        entries.reserve(displayRecords.size() + weeks.size());
        for (auto weekIt = weeks.rbegin(); weekIt != weeks.rend(); ++weekIt) {
            const WeekEntries& week = weekIt->second;
            size_t summaryIndex = 0;
            for (size_t i = 0; i < week.days.size(); ++i) {
                if (week.days[i].isSunday) {
                    summaryIndex = i;
                    break;
                }
            }

            for (size_t i = 0; i < week.days.size(); ++i) {
                if (i == summaryIndex) {
                    int totalSeconds = static_cast<int>(std::min<long long>(
                        week.totalSeconds, std::numeric_limits<int>::max()));
                    int manualSeconds = static_cast<int>(std::min<long long>(
                        week.manualSeconds, std::numeric_limits<int>::max()));
                    entries.push_back({ std::to_wstring(week.weekNumber) + L". hét", totalSeconds, manualSeconds, true });
                }
                entries.push_back({ week.days[i].label, week.days[i].seconds, week.days[i].manualSeconds, false });
            }
        }
        return entries;
    }
}