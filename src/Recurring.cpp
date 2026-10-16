#include "Recurring.h"
#include "JsonLite.h"
#include "TaskUtils.h"
#include <algorithm>
#include <cstdio>
#include <cwctype>

namespace {
// Days since 1970-01-01 for a proleptic Gregorian date (H. Hinnant's days_from_civil).
long long DaysFromCivil(int year, int month, int day) {
    year -= month <= 2;
    const long long era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

void CivilFromDays(long long days, int& year, int& month, int& day) {
    days += 719468;
    const long long era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(days - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    year = static_cast<int>(yoe + era * 400 + (month <= 2));
}

int DaysInMonth(int year, int month) {
    static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return month == 2 && leap ? 29 : days[month - 1];
}

bool Parse(const std::string& date, int& year, int& month, int& day) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') return false;
    if (sscanf_s(date.c_str(), "%4d-%2d-%2d", &year, &month, &day) != 3) return false;
    return month >= 1 && month <= 12 && day >= 1 && day <= DaysInMonth(year, month);
}

const char* UnitKey(RepeatUnit unit) {
    return unit == RepeatUnit::Month ? "month" : (unit == RepeatUnit::Week ? "week" : "day");
}

std::string Quote(const std::wstring& text) {
    return "\"" + TaskUtils::EscapeJsonString(TaskUtils::WideToUtf8(text)) + "\"";
}

std::string QuoteUtf8(const std::string& text) {
    return "\"" + TaskUtils::EscapeJsonString(text) + "\"";
}
}

bool Recurring::IsValidDate(const std::string& date) {
    int year = 0, month = 0, day = 0;
    return Parse(date, year, month, day);
}

std::string Recurring::DateKey(int year, int month, int day) {
    char buffer[16]{};
    sprintf_s(buffer, "%04d-%02d-%02d", year, month, day);
    return buffer;
}

std::string Recurring::Today() {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    return DateKey(now.wYear, now.wMonth, now.wDay);
}

std::string Recurring::Occurrence(const std::string& start, int count, RepeatUnit unit, long long index) {
    int year = 0, month = 0, day = 0;
    if (!Parse(start, year, month, day)) return "";
    const long long steps = index * (std::max)(count, 1);
    if (unit == RepeatUnit::Month) {
        const long long months = static_cast<long long>(year) * 12 + (month - 1) + steps;
        const int targetYear = static_cast<int>(months / 12);
        const int targetMonth = static_cast<int>(months % 12) + 1;
        return DateKey(targetYear, targetMonth, (std::min)(day, DaysInMonth(targetYear, targetMonth)));
    }
    const long long days = DaysFromCivil(year, month, day) + steps * (unit == RepeatUnit::Week ? 7 : 1);
    CivilFromDays(days, year, month, day);
    return DateKey(year, month, day);
}

std::string Recurring::NextAfter(const RecurringRule& rule, const std::string& date) {
    if (rule.repeat_count <= 0) return "";
    int startYear = 0, startMonth = 0, startDay = 0;
    int year = 0, month = 0, day = 0;
    if (!Parse(rule.start_date, startYear, startMonth, startDay) || !Parse(date, year, month, day)) return "";
    if (rule.start_date > date) return rule.start_date;

    // Estimate the index from the elapsed time, then step past `date`.
    long long index = 0;
    if (rule.repeat_unit == RepeatUnit::Month) {
        const long long months = (static_cast<long long>(year) - startYear) * 12 + (month - startMonth);
        index = (std::max)(0LL, months / rule.repeat_count);
    } else {
        const long long step = static_cast<long long>(rule.repeat_count) * (rule.repeat_unit == RepeatUnit::Week ? 7 : 1);
        index = (DaysFromCivil(year, month, day) - DaysFromCivil(startYear, startMonth, startDay)) / step;
    }
    std::string next = Occurrence(rule.start_date, rule.repeat_count, rule.repeat_unit, index);
    while (!next.empty() && next <= date) {
        next = Occurrence(rule.start_date, rule.repeat_count, rule.repeat_unit, ++index);
    }
    return next;
}

std::wstring Recurring::DescribeRepeat(int count, RepeatUnit unit) {
    if (count <= 0) return L"Egyszeri";
    const wchar_t* suffix = unit == RepeatUnit::Month ? L"havonta" : (unit == RepeatUnit::Week ? L"hetente" : L"naponta");
    if (count == 1) {
        std::wstring text = suffix;
        text[0] = static_cast<wchar_t>(towupper(text[0]));
        return text;
    }
    return std::to_wstring(count) + L" " + suffix;
}

std::wstring Recurring::UnitLabel(RepeatUnit unit) {
    return unit == RepeatUnit::Month ? L"hónap" : (unit == RepeatUnit::Week ? L"hét" : L"nap");
}

std::wstring Recurring::FormatDate(const std::string& date) {
    if (!IsValidDate(date)) return L"-";
    std::wstring text = TaskUtils::Utf8ToWide(date) + L".";
    std::replace(text.begin(), text.end(), L'-', L'.');
    return text;
}

void Recurring::WriteJson(std::ostream& out, const std::vector<RecurringRule>& rules) {
    out << "  \"recurring_rules\": [";
    for (size_t i = 0; i < rules.size(); ++i) {
        const RecurringRule& rule = rules[i];
        out << (i > 0 ? "," : "") << "\n    { \"uid\": " << QuoteUtf8(rule.uid)
            << ", \"text\": " << Quote(rule.text)
            << ", \"start_date\": " << QuoteUtf8(rule.start_date)
            << ", \"repeat_count\": " << rule.repeat_count
            << ", \"repeat_unit\": \"" << UnitKey(rule.repeat_unit) << "\""
            << ", \"targets\": [";
        for (size_t t = 0; t < rule.targets.size(); ++t) out << (t > 0 ? ", " : "") << rule.targets[t];
        out << "], \"next_due\": " << QuoteUtf8(rule.next_due)
            << ", \"last_task_uid\": " << QuoteUtf8(rule.last_task_uid)
            << ", \"created_at\": " << static_cast<long long>(rule.created_at) << " }";
    }
    out << (rules.empty() ? "],\n" : "\n  ],\n");
}

void Recurring::LoadFromJson(const std::string& content, std::vector<RecurringRule>& rules) {
    rules.clear();
    std::vector<std::string> objects;
    if (!JsonLite::ReadObjectArray(content, "recurring_rules", objects)) return;
    for (const std::string& object : objects) {
        RecurringRule rule;
        if (!JsonLite::GetString(object, "uid", rule.uid) || rule.uid.empty()) continue;
        if (!JsonLite::GetWideString(object, "text", rule.text) || rule.text.empty()) continue;
        JsonLite::GetString(object, "start_date", rule.start_date);
        JsonLite::GetString(object, "next_due", rule.next_due);
        if (!IsValidDate(rule.start_date) || !IsValidDate(rule.next_due)) continue;
        long long number = 0;
        if (JsonLite::GetInt64(object, "repeat_count", number)) {
            rule.repeat_count = static_cast<int>((std::max)(0LL, (std::min)(static_cast<long long>(MAX_REPEAT_COUNT), number)));
        }
        std::string unit;
        JsonLite::GetString(object, "repeat_unit", unit);
        rule.repeat_unit = unit == "month" ? RepeatUnit::Month : (unit == "week" ? RepeatUnit::Week : RepeatUnit::Day);
        std::vector<std::string> targets;
        JsonLite::ReadScalarArray(object, "targets", targets);
        for (const std::string& target : targets) {
            const int id = atoi(target.c_str());
            if (id > 0) rule.targets.push_back(id);
        }
        JsonLite::GetString(object, "last_task_uid", rule.last_task_uid);
        if (JsonLite::GetInt64(object, "created_at", number)) rule.created_at = static_cast<time_t>(number);
        rules.push_back(rule);
    }
}
