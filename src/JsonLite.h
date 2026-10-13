#pragma once

#include <string>
#include <vector>

// Minimal, string-aware readers for the app's own hand-written JSON files.
namespace JsonLite {
    // Collects the raw `{...}` objects of the array stored under `key`.
    // Returns false when the key is missing or the array is truncated/malformed.
    bool ReadObjectArray(const std::string& content, const char* key, std::vector<std::string>& objects);

    // Collects scalar items (strings unescaped, numbers as text) of the array stored under `key`.
    bool ReadScalarArray(const std::string& content, const char* key, std::vector<std::string>& values);

    // Field readers for a single object (or the whole document for top-level keys).
    bool GetString(const std::string& json, const char* key, std::string& value);
    bool GetWideString(const std::string& json, const char* key, std::wstring& value);
    bool GetInt64(const std::string& json, const char* key, long long& value);
    bool GetBool(const std::string& json, const char* key, bool& value);
}
