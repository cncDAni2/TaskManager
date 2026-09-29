#include "TaskUtils.h"

#define SECURITY_WIN32
#include <security.h>
#include <sstream>
#include <iomanip>
#include <cctype>

namespace TaskUtils {

std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &result[0], size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &result[0], size);
    return result;
}

std::wstring FormatDate(time_t t) {
    if (t == 0) return L"-";
    struct tm lt;
    localtime_s(&lt, &t);
    wchar_t buf[64];
    swprintf_s(buf, L"%04d.%02d.%02d", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
    return buf;
}

std::wstring FormatTime(time_t t) {
    if (t == 0) return L"-";
    struct tm lt;
    localtime_s(&lt, &t);
    wchar_t buf[64];
    swprintf_s(buf, L"%02d:%02d", lt.tm_hour, lt.tm_min);
    return buf;
}

std::wstring FormatDateTime(time_t t) {
    if (t == 0) return L"-";
    struct tm lt;
    localtime_s(&lt, &t);
    wchar_t buf[64];
    swprintf_s(buf, L"%04d.%02d.%02d %02d:%02d", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min);
    return buf;
}

time_t GetYesterday930Cutoff(time_t now) {
    struct tm lt;
    localtime_s(&lt, &now);
    lt.tm_hour = 9;
    lt.tm_min = 30;
    lt.tm_sec = 0;
    lt.tm_isdst = -1;
    lt.tm_mday -= 1;
    return mktime(&lt);
}

time_t GetLastResetCutoff(time_t now, int hour, int minute) {
    struct tm lt;
    localtime_s(&lt, &now);
    struct tm cut = lt;
    cut.tm_hour = hour < 0 ? 0 : (hour > 23 ? 23 : hour);
    cut.tm_min = minute < 0 ? 0 : (minute > 59 ? 59 : minute);
    cut.tm_sec = 0;
    cut.tm_isdst = -1;
    time_t cutTime = mktime(&cut);
    if (now < cutTime) {
        cut.tm_mday -= 1;
        cutTime = mktime(&cut);
    }
    return cutTime;
}

std::wstring FormatWorkDuration(int totalSeconds) {
    if (totalSeconds < 0) totalSeconds = 0;
    int h = totalSeconds / 3600;
    int m = (totalSeconds % 3600) / 60;
    int s = totalSeconds % 60;
    wchar_t buf[64];
    if (h > 0) {
        swprintf_s(buf, L"%dó %02dp", h, m);
    } else if (m > 0) {
        swprintf_s(buf, L"%dp %02dmp", m, s);
    } else {
        swprintf_s(buf, L"%dmp", s);
    }
    return buf;
}

std::string EscapeJsonString(const std::string& s) {
    std::ostringstream o;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (c < 0x20) {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                } else {
                    o << (char)c;
                }
                break;
        }
    }
    return o.str();
}

std::string UnescapeJsonString(const std::string& s) {
    std::string res;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char next = s[++i];
            switch (next) {
                case '"': res += '"'; break;
                case '\\': res += '\\'; break;
                case '/': res += '/'; break;
                case 'b': res += '\b'; break;
                case 'f': res += '\f'; break;
                case 'n': res += '\n'; break;
                case 'r': res += '\r'; break;
                case 't': res += '\t'; break;
                case 'u': {
                    if (i + 4 < s.size()) {
                        std::string hex = s.substr(i + 1, 4);
                        i += 4;
                        wchar_t code = (wchar_t)std::stoul(hex, nullptr, 16);
                        std::wstring w(1, code);
                        res += WideToUtf8(w);
                    }
                    break;
                }
                default: res += next; break;
            }
        } else {
            res += s[i];
        }
    }
    return res;
}

std::wstring GetCleanUserName() {
    wchar_t nameBuf[256] = { 0 };
    ULONG nameSize = 256;
    if (GetUserNameExW(NameDisplay, nameBuf, &nameSize) && nameSize > 0) {
        std::wstring name = nameBuf;
        size_t parenPos = name.find(L'(');
        if (parenPos != std::wstring::npos) {
            name = name.substr(0, parenPos);
        }
        while (!name.empty() && (iswspace(name.back()) || name.back() == L',')) {
            name.pop_back();
        }
        while (!name.empty() && iswspace(name.front())) {
            name.erase(name.begin());
        }
        if (!name.empty()) {
            return name;
        }
    }
    DWORD len = 256;
    if (GetUserNameW(nameBuf, &len) && len > 0) {
        return nameBuf;
    }
    wchar_t envBuf[256] = { 0 };
    if (GetEnvironmentVariableW(L"USERNAME", envBuf, 256) > 0) {
        return envBuf;
    }
    return L"Felhasználó";
}

std::wstring GetDefaultSyncFilePath() {
    wchar_t userProfile[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
        std::wstring path = userProfile;
        path += L"\\OneDrive - Siemens AG\\TaskManager\\tasks.json";
        return path;
    }
    return L"tasks_sync.json";
}

void EnsureParentDirectoryExists(const std::wstring& path) {
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        std::wstring dir = path.substr(0, lastSlash);
        for (size_t i = 0; i < dir.size(); ++i) {
            if (dir[i] == L'\\' || dir[i] == L'/') {
                std::wstring sub = dir.substr(0, i);
                if (!sub.empty() && sub.back() != L':') {
                    CreateDirectoryW(sub.c_str(), nullptr);
                }
            }
        }
        CreateDirectoryW(dir.c_str(), nullptr);
    }
}

} // namespace TaskUtils
