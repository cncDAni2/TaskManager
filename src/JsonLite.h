#pragma once

#include <string>
#include <utility>
#include <vector>

// Minimal, string-aware readers for the app's own hand-written JSON files.
namespace JsonLite {
    // One member of an object: the key as written (still escaped) and its raw value text,
    // so members the app does not understand can be written back unchanged.
    using Member = std::pair<std::string, std::string>;

    // Collects the raw `{...}` objects of the array stored under `key`.
    // Returns false when the key is missing or the array is truncated/malformed.
    bool ReadObjectArray(const std::string& content, const char* key, std::vector<std::string>& objects);

    // Same, for a raw array value (`[...]`).
    bool ReadObjectArrayValue(const std::string& array, std::vector<std::string>& objects);

    // Splits a raw object into its top-level members; nested keys are not matched.
    // Returns false when the object is truncated/malformed.
    bool ReadMembers(const std::string& object, std::vector<Member>& members);

    // Collects scalar items (strings unescaped, numbers as text) of the array stored under `key`.
    bool ReadScalarArray(const std::string& content, const char* key, std::vector<std::string>& values);

    // Field readers for a single object (or the whole document for top-level keys).
    bool GetString(const std::string& json, const char* key, std::string& value);
    bool GetWideString(const std::string& json, const char* key, std::wstring& value);
    bool GetInt64(const std::string& json, const char* key, long long& value);
    bool GetBool(const std::string& json, const char* key, bool& value);

    // Readers for a raw member value as returned by ReadMembers.
    bool StringValue(const std::string& raw, std::string& value);
    bool WideStringValue(const std::string& raw, std::wstring& value);
    bool Int64Value(const std::string& raw, long long& value);
    bool BoolValue(const std::string& raw, bool& value);
}
