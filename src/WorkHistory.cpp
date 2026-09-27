#include "WorkHistory.h"
#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <map>
#include <random>
#include <sstream>

namespace WorkHistory {
    namespace {
        struct DayEntry {
            std::string date;
            std::wstring label;
            int seconds;
            bool isSunday;
        };

        struct WeekEntries {
            int weekNumber = 0;
            long long totalSeconds = 0;
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

    bool SeedIfEmpty(Records& records) {
        if (!records.empty()) return false;

        std::random_device randomDevice;
        std::mt19937 generator(randomDevice());
        std::uniform_int_distribution<int> duration(45 * 60, 8 * 60 * 60);
        time_t now = time(nullptr);
        for (int daysAgo = 14; daysAgo >= 1; --daysAgo) {
            time_t sampleDay = now;
            tm localTime{};
            localtime_s(&localTime, &sampleDay);
            localTime.tm_hour = 12;
            localTime.tm_min = 0;
            localTime.tm_sec = 0;
            localTime.tm_mday -= daysAgo;
            sampleDay = mktime(&localTime);
            records[DateKey(sampleDay)] = duration(generator);
        }
        return true;
    }

    void LoadFromJson(const std::string& content, Records& records) {
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
            if (datePos != std::string::npos && secondsPos != std::string::npos) {
                size_t dateStart = object.find('"', object.find(':', datePos) + 1);
                size_t dateEnd = dateStart == std::string::npos ? std::string::npos : object.find('"', dateStart + 1);
                size_t valueStart = object.find_first_of("0123456789", object.find(':', secondsPos) + 1);
                if (dateEnd != std::string::npos && valueStart != std::string::npos) {
                    std::string date = object.substr(dateStart + 1, dateEnd - dateStart - 1);
                    size_t valueEnd = object.find_first_not_of("0123456789", valueStart);
                    int seconds = std::stoi(object.substr(valueStart, valueEnd - valueStart));
                    if (date.size() == 10 && seconds >= 0) records[date] = seconds;
                }
            }
            position = objectEnd + 1;
        }
    }

    void WriteJson(std::ostream& out, const Records& records) {
        out << "  \"work_history\": [";
        bool first = true;
        for (const auto& entry : records) {
            if (!first) out << ", ";
            first = false;
            out << "{\"date\": \"" << entry.first << "\", \"seconds\": " << entry.second << "}";
        }
        out << "],\n";
    }

    std::vector<DisplayEntry> GetDisplayEntries(
        const Records& records, time_t currentDay, int currentSeconds) {
        Records displayRecords = records;
        displayRecords[DateKey(currentDay)] = currentSeconds;

        std::map<std::string, WeekEntries> weeks;
        for (auto it = displayRecords.rbegin(); it != displayRecords.rend(); ++it) {
            std::string weekStart;
            int weekNumber = 0;
            bool isSunday = false;
            if (!GetWeekInfo(it->first, weekStart, weekNumber, isSunday)) continue;

            WeekEntries& week = weeks[weekStart];
            week.weekNumber = weekNumber;
            week.totalSeconds += std::max(0, it->second);

            std::wstring label = std::to_wstring(it->first[5] - '0') + std::to_wstring(it->first[6] - '0') + L"." +
                std::to_wstring(it->first[8] - '0') + std::to_wstring(it->first[9] - '0');
            week.days.push_back({ it->first, label, it->second, isSunday });
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
                    entries.push_back({ std::to_wstring(week.weekNumber) + L". hét", totalSeconds, true });
                }
                entries.push_back({ week.days[i].label, week.days[i].seconds, false });
            }
        }
        return entries;
    }
}