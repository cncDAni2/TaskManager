#include "VersionHint.h"
#include "TaskUtils.h"
#include "Version.h"
#include <cstdlib>

int VersionHint::Compare(const std::string& left, const std::string& right) {
    const char* a = left.c_str();
    const char* b = right.c_str();
    while (*a || *b) {
        char* nextA = nullptr;
        char* nextB = nullptr;
        const long partA = std::strtol(a, &nextA, 10);
        const long partB = std::strtol(b, &nextB, 10);
        if (partA != partB) return partA < partB ? -1 : 1;
        a = *nextA == '.' ? nextA + 1 : nextA + std::char_traits<char>::length(nextA);
        b = *nextB == '.' ? nextB + 1 : nextB + std::char_traits<char>::length(nextB);
    }
    return 0;
}

namespace {
std::wstring JoinKeys(const std::vector<std::string>& keys) {
    std::wstring joined;
    for (const std::string& key : keys) {
        if (!joined.empty()) joined += L", ";
        joined += TaskUtils::Utf8ToWide(key);
    }
    return joined;
}
}

bool VersionHint::Describe(const std::wstring& sourceName, const SyncFileInfo& info, VersionNotice& notice) {
    const std::string own = TASK_MANAGER_VERSION_STRING;
    const int comparison = info.appVersion.empty() ? 0 : Compare(info.appVersion, own);
    const std::wstring theirs = TaskUtils::Utf8ToWide(info.appVersion);
    const std::wstring mine = TaskUtils::Utf8ToWide(own);

    notice.newer = comparison > 0;
    if (comparison > 0) {
        notice.heading = L"ÚJABB VERZIÓ VAN HASZNÁLATBAN";
        notice.body = L"Valakinél " + theirs + L" verzió fut, nálad " + mine + L".";
    } else if (!info.unknownKeys.empty()) {
        notice.heading = L"VÉLHETŐLEG ÚJ VERZIÓ JELENT MEG";
        notice.body = L"Ismeretlen adatok: " + JoinKeys(info.unknownKeys);
    } else if (comparison < 0) {
        notice.heading = L"RÉGEBBI VERZIÓ IS HASZNÁLATBAN";
        notice.body = L"Valakinél " + theirs + L" verzió fut, nálad " + mine + L".";
    } else {
        return false;
    }
    notice.footer = L"Szinkronfájl: " + sourceName;
    return true;
}
