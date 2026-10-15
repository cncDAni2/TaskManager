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

// Returns the index just past the value starting at `pos` (string, object, array or scalar), or npos.
size_t SkipValue(const std::string& text, size_t pos) {
    if (pos >= text.size()) return std::string::npos;
    const char first = text[pos];
    if (first == '"') return SkipString(text, pos);
    if (first == '{' || first == '[') {
        int depth = 0;
        while (pos < text.size()) {
            const char c = text[pos];
            if (c == '"') {
                pos = SkipString(text, pos);
                if (pos == std::string::npos) return std::string::npos;
                continue;
            }
            if (c == '{' || c == '[') ++depth;
            if ((c == '}' || c == ']') && --depth == 0) return pos + 1;
            ++pos;
        }
        return std::string::npos;
    }
    // A scalar is always followed by a separator or the closing bracket in a complete file.
    size_t end = text.find_first_of(",}] \t\r\n", pos);
    return end == pos ? std::string::npos : end;
}

size_t FindArrayStart(const std::string& content, const char* key) {
    size_t pos = FindValue(content, key);
    if (pos == std::string::npos || pos >= content.size() || content[pos] != '[') return std::string::npos;
    return pos;
}

bool ReadObjectArrayAt(const std::string& content, size_t pos, std::vector<std::string>& objects) {
    objects.clear();
    if (pos >= content.size() || content[pos] != '[') return false;
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

        size_t end = SkipValue(content, pos);
        if (end == std::string::npos) return false;
        objects.push_back(content.substr(pos, end - pos));
        pos = end;
    }
}

bool StringAt(const std::string& json, size_t pos, std::string& value) {
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '"') return false;
    size_t end = SkipString(json, pos);
    if (end == std::string::npos) return false;
    value = TaskUtils::UnescapeJsonString(json.substr(pos + 1, end - pos - 2));
    return true;
}

bool Int64At(const std::string& json, size_t pos, long long& value) {
    if (pos == std::string::npos || pos >= json.size()) return false;
    const char* begin = json.c_str() + pos;
    char* end = nullptr;
    long long parsed = std::strtoll(begin, &end, 10);
    if (end == begin) return false;
    value = parsed;
    return true;
}

bool BoolAt(const std::string& json, size_t pos, bool& value) {
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
}

bool JsonLite::ReadObjectArray(const std::string& content, const char* key, std::vector<std::string>& objects) {
    size_t pos = FindArrayStart(content, key);
    if (pos == std::string::npos) {
        objects.clear();
        return false;
    }
    return ReadObjectArrayAt(content, pos, objects);
}

bool JsonLite::ReadObjectArrayValue(const std::string& array, std::vector<std::string>& objects) {
    return ReadObjectArrayAt(array, SkipSpace(array, 0), objects);
}

bool JsonLite::ReadMembers(const std::string& object, std::vector<Member>& members) {
    members.clear();
    size_t pos = SkipSpace(object, 0);
    if (pos >= object.size() || object[pos] != '{') return false;
    ++pos;
    while (true) {
        pos = SkipSpace(object, pos);
        if (pos >= object.size()) return false;
        if (object[pos] == '}') return true;
        if (object[pos] == ',') {
            ++pos;
            continue;
        }
        if (object[pos] != '"') return false;

        size_t keyEnd = SkipString(object, pos);
        if (keyEnd == std::string::npos) return false;
        std::string key = object.substr(pos + 1, keyEnd - pos - 2);
        pos = SkipSpace(object, keyEnd);
        if (pos >= object.size() || object[pos] != ':') return false;
        pos = SkipSpace(object, pos + 1);
        size_t valueEnd = SkipValue(object, pos);
        if (valueEnd == std::string::npos) return false;
        members.emplace_back(std::move(key), object.substr(pos, valueEnd - pos));
        pos = valueEnd;
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
    return StringAt(json, FindValue(json, key), value);
}

bool JsonLite::GetWideString(const std::string& json, const char* key, std::wstring& value) {
    std::string utf8;
    if (!GetString(json, key, utf8)) return false;
    value = TaskUtils::Utf8ToWide(utf8);
    return true;
}

bool JsonLite::GetInt64(const std::string& json, const char* key, long long& value) {
    return Int64At(json, FindValue(json, key), value);
}

bool JsonLite::GetBool(const std::string& json, const char* key, bool& value) {
    return BoolAt(json, FindValue(json, key), value);
}

bool JsonLite::StringValue(const std::string& raw, std::string& value) {
    return StringAt(raw, 0, value);
}

bool JsonLite::WideStringValue(const std::string& raw, std::wstring& value) {
    std::string utf8;
    if (!StringValue(raw, utf8)) return false;
    value = TaskUtils::Utf8ToWide(utf8);
    return true;
}

bool JsonLite::Int64Value(const std::string& raw, long long& value) {
    return Int64At(raw, 0, value);
}

bool JsonLite::BoolValue(const std::string& raw, bool& value) {
    return BoolAt(raw, 0, value);
}
