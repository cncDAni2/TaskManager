#include "JsonLite.h"
#include "TaskUtils.h"
#include <cstdlib>

namespace {
size_t SkipSpace(const std::string& text, size_t pos) {
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' ||
        text[pos] == '\r' || text[pos] == '\n')) {
        ++pos;
    }
    return pos;
}

// Returns the position of the value that follows `"key":`, or npos.
size_t FindValue(const std::string& json, const char* key) {
    const std::string pattern = std::string("\"") + key + "\"";
    size_t pos = 0;
    while ((pos = json.find(pattern, pos)) != std::string::npos) {
        size_t colon = SkipSpace(json, pos + pattern.size());
        if (colon < json.size() && json[colon] == ':') return SkipSpace(json, colon + 1);
        pos += pattern.size();
    }
    return std::string::npos;
}

// Returns the index just past the closing quote of the string starting at `start`, or npos.
size_t SkipString(const std::string& text, size_t start) {
    size_t pos = start + 1;
    while (pos < text.size()) {
        if (text[pos] == '\\') {
            pos += 2;
        } else if (text[pos] == '"') {
            return pos + 1;
        } else {
            ++pos;
        }
    }
    return std::string::npos;
}

size_t FindArrayStart(const std::string& content, const char* key) {
    size_t pos = FindValue(content, key);
    if (pos == std::string::npos || pos >= content.size() || content[pos] != '[') return std::string::npos;
    return pos;
}
}

bool JsonLite::ReadObjectArray(const std::string& content, const char* key, std::vector<std::string>& objects) {
    objects.clear();
    size_t pos = FindArrayStart(content, key);
    if (pos == std::string::npos) return false;
    ++pos;
    while (true) {
        pos = SkipSpace(content, pos);
        if (pos >= content.size()) return false;
        if (content[pos] == ']') return true;
        if (content[pos] == ',') {
            ++pos;
            continue;
        }
        if (content[pos] != '{') return false;

        size_t start = pos;
        int depth = 0;
        while (pos < content.size()) {
            char c = content[pos];
            if (c == '"') {
                pos = SkipString(content, pos);
                if (pos == std::string::npos) return false;
                continue;
            }
            if (c == '{') ++depth;
            if (c == '}' && --depth == 0) break;
            ++pos;
        }
        if (pos >= content.size()) return false;
        objects.push_back(content.substr(start, pos - start + 1));
        ++pos;
    }
}

bool JsonLite::ReadScalarArray(const std::string& content, const char* key, std::vector<std::string>& values) {
    values.clear();
    size_t pos = FindArrayStart(content, key);
    if (pos == std::string::npos) return false;
    ++pos;
    while (true) {
        pos = SkipSpace(content, pos);
        if (pos >= content.size()) return false;
        if (content[pos] == ']') return true;
        if (content[pos] == ',') {
            ++pos;
            continue;
        }
        if (content[pos] == '"') {
            size_t end = SkipString(content, pos);
            if (end == std::string::npos) return false;
            values.push_back(TaskUtils::UnescapeJsonString(content.substr(pos + 1, end - pos - 2)));
            pos = end;
        } else {
            size_t end = content.find_first_of(",] \t\r\n", pos);
            if (end == std::string::npos) return false;
            values.push_back(content.substr(pos, end - pos));
            pos = end;
        }
    }
}

bool JsonLite::GetString(const std::string& json, const char* key, std::string& value) {
    size_t pos = FindValue(json, key);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '"') return false;
    size_t end = SkipString(json, pos);
    if (end == std::string::npos) return false;
    value = TaskUtils::UnescapeJsonString(json.substr(pos + 1, end - pos - 2));
    return true;
}

bool JsonLite::GetWideString(const std::string& json, const char* key, std::wstring& value) {
    std::string utf8;
    if (!GetString(json, key, utf8)) return false;
    value = TaskUtils::Utf8ToWide(utf8);
    return true;
}

bool JsonLite::GetInt64(const std::string& json, const char* key, long long& value) {
    size_t pos = FindValue(json, key);
    if (pos == std::string::npos || pos >= json.size()) return false;
    const char* begin = json.c_str() + pos;
    char* end = nullptr;
    long long parsed = std::strtoll(begin, &end, 10);
    if (end == begin) return false;
    value = parsed;
    return true;
}

bool JsonLite::GetBool(const std::string& json, const char* key, bool& value) {
    size_t pos = FindValue(json, key);
    if (pos == std::string::npos) return false;
    if (json.compare(pos, 4, "true") == 0) {
        value = true;
        return true;
    }
    if (json.compare(pos, 5, "false") == 0) {
        value = false;
        return true;
    }
    return false;
}
