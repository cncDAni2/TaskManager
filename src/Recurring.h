#pragma once

#include <ctime>
#include <ostream>
#include <string>
#include <vector>

enum class RepeatUnit { Day = 0, Week = 1, Month = 2 };

// A local task writer: creates a task on its due day, and again every `repeat_count` units when set.
// The created tasks are ordinary (local or synchronized) tasks; only this rule stays local.
struct RecurringRule {
    std::string uid;
    std::wstring text;
    std::string start_date;    // YYYY-MM-DD; occurrences are counted from here.
    int repeat_count = 0;      // 0: a one-off later task, removed after it ran.
    RepeatUnit repeat_unit = RepeatUnit::Day;
    std::vector<int> targets;  // Sync source ids; empty (or all removed since): a local task.
    std::string next_due;      // YYYY-MM-DD of the next instance.
    std::string last_task_uid; // No new instance while this task is still open.
    time_t created_at = 0;
};

namespace Recurring {
    constexpr int MAX_REPEAT_COUNT = 999;

    bool IsValidDate(const std::string& date);
    std::string DateKey(int year, int month, int day);
    std::string Today(); // Local calendar date.
    // Occurrence `index` (0 = start). Months keep the start's day, clamped to the month's length.
    std::string Occurrence(const std::string& start, int count, RepeatUnit unit, long long index);
    // First occurrence strictly after `date`; empty for a one-off rule.
    std::string NextAfter(const RecurringRule& rule, const std::string& date);

    std::wstring DescribeRepeat(int count, RepeatUnit unit); // "Naponta", "3 hetente", "Egyszeri"
    std::wstring UnitLabel(RepeatUnit unit);                 // "nap", "hét", "hónap"
    std::wstring FormatDate(const std::string& date);        // "2026.10.20."

    void WriteJson(std::ostream& out, const std::vector<RecurringRule>& rules);
    void LoadFromJson(const std::string& content, std::vector<RecurringRule>& rules);
}
