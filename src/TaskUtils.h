#pragma once

#include "AppTypes.h"
#include <string>
#include <ctime>

namespace TaskUtils {
    // Builds the app's JSON text. iostreams would add about 150 KB to the EXE for this.
    class TextBuffer {
    public:
        TextBuffer& operator<<(const char* text) { data += text; return *this; }
        TextBuffer& operator<<(const std::string& text) { data += text; return *this; }
        TextBuffer& operator<<(int value) { data += std::to_string(value); return *this; }
        TextBuffer& operator<<(unsigned value) { data += std::to_string(value); return *this; }
        TextBuffer& operator<<(long long value) { data += std::to_string(value); return *this; }
        TextBuffer& operator<<(unsigned long long value) { data += std::to_string(value); return *this; }
        TextBuffer& operator<<(double value);
        const std::string& str() const { return data; }

    private:
        std::string data;
    };

    std::string WideToUtf8(const std::wstring& wstr);
    std::wstring Utf8ToWide(const std::string& str);
    std::wstring FormatDate(time_t t);
    std::wstring FormatTime(time_t t);
    std::wstring FormatDateTime(time_t t);
    time_t GetYesterday930Cutoff(time_t now);
    time_t GetLastResetCutoff(time_t now, int hour, int minute);
    std::wstring FormatWorkDuration(int totalSeconds);
    std::string EscapeJsonString(const std::string& s);
    std::string UnescapeJsonString(const std::string& s);
    std::wstring GetCleanUserName();
    void EnsureParentDirectoryExists(const std::wstring& path);
    // Reads a whole file; false if it cannot be opened or read.
    bool ReadFileBytes(const std::wstring& path, std::string& content);
    // Writes a temporary file beside `path`, then swaps it in, so a crash or power cut leaves
    // either the old or the new content, never a truncated file. False leaves `path` untouched.
    bool WriteFileAtomic(const std::wstring& path, const std::string& content);
}
