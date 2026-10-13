#include "ExcludedApps.h"
#include <windows.h>
#include <vector>

const wchar_t ExcludedApps::DEFAULT_KEYWORDS[] = L"YouTube, Discord, Facebook";

namespace {
struct Keyword {
    std::wstring display;
    std::wstring lower;
};

std::wstring ToLower(std::wstring text) {
    if (!text.empty()) CharLowerBuffW(text.data(), static_cast<DWORD>(text.size()));
    return text;
}

std::vector<std::wstring> Split(const std::wstring& keywords) {
    std::vector<std::wstring> items;
    size_t start = 0;
    while (start <= keywords.size()) {
        size_t end = keywords.find(L',', start);
        if (end == std::wstring::npos) end = keywords.size();
        size_t first = keywords.find_first_not_of(L" \t", start);
        if (first != std::wstring::npos && first < end) {
            size_t last = keywords.find_last_not_of(L" \t", end - 1);
            items.push_back(keywords.substr(first, last - first + 1));
        }
        start = end + 1;
    }
    return items;
}

// The work timer queries this every second, so the parsed list is cached per settings string.
const std::vector<Keyword>& ParsedKeywords(const std::wstring& keywords) {
    static std::wstring s_source;
    static std::vector<Keyword> s_parsed;
    static bool s_initialized = false;
    if (!s_initialized || s_source != keywords) {
        s_source = keywords;
        s_parsed.clear();
        for (const std::wstring& item : Split(keywords)) {
            s_parsed.push_back({ item, ToLower(item) });
        }
        s_initialized = true;
    }
    return s_parsed;
}
}

std::wstring ExcludedApps::Normalize(const std::wstring& keywords) {
    std::wstring result;
    for (const std::wstring& item : Split(keywords)) {
        if (!result.empty()) result += L", ";
        result += item;
    }
    return result;
}

std::wstring ExcludedApps::FindMatch(const std::wstring& keywords, const std::wstring& windowTitle) {
    const std::vector<Keyword>& parsed = ParsedKeywords(keywords);
    if (parsed.empty() || windowTitle.empty()) return L"";
    const std::wstring title = ToLower(windowTitle);
    for (const Keyword& keyword : parsed) {
        if (title.find(keyword.lower) != std::wstring::npos) return keyword.display;
    }
    return L"";
}
