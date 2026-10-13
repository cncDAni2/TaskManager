#include "SyncFiles.h"
#include "JsonLite.h"
#include "TaskUtils.h"
#include <rpc.h>
#include <algorithm>
#include <fstream>
#include <iterator>

namespace {
constexpr time_t TOMBSTONE_RETENTION_SECONDS = 14 * 24 * 60 * 60;
constexpr long long LEGACY_SYNC_ID_BASE = 1000000;

bool ParseRecord(const std::string& object, SyncRecord& record) {
    if (!JsonLite::GetString(object, "uid", record.uid) || record.uid.empty()) {
        long long legacyId = 0;
        if (!JsonLite::GetInt64(object, "id", legacyId)) return false;
        // Older files used small ids that were shifted into the sync range on load.
        if (legacyId < LEGACY_SYNC_ID_BASE) legacyId += LEGACY_SYNC_ID_BASE;
        record.uid = std::to_string(legacyId);
    }
    if (!JsonLite::GetWideString(object, "text", record.text)) return false;

    long long number = 0;
    JsonLite::GetBool(object, "completed", record.completed);
    if (JsonLite::GetInt64(object, "created_at", number)) record.created_at = static_cast<time_t>(number);
    if (JsonLite::GetInt64(object, "completed_at", number)) record.completed_at = static_cast<time_t>(number);
    JsonLite::GetWideString(object, "author", record.author);
    JsonLite::GetWideString(object, "assignee", record.assignee);
    JsonLite::GetBool(object, "deleted", record.deleted);
    if (JsonLite::GetInt64(object, "updated_at", number)) {
        record.updated_at = static_cast<time_t>(number);
    } else {
        record.updated_at = (std::max)(record.created_at, record.completed_at);
    }
    if (!JsonLite::GetWideString(object, "updated_by", record.updated_by)) {
        record.updated_by = record.author;
    }
    return true;
}

std::string Quote(const std::wstring& text) {
    return "\"" + TaskUtils::EscapeJsonString(TaskUtils::WideToUtf8(text)) + "\"";
}
}

bool SyncFiles::GetWriteTime(const std::wstring& path, FILETIME& writeTime) {
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes)) return false;
    writeTime = attributes.ftLastWriteTime;
    return true;
}

SyncFiles::ReadResult SyncFiles::Read(const std::wstring& path, std::vector<SyncRecord>& records,
    FILETIME& writeTime) {
    records.clear();
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        return (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            ? ReadResult::Missing : ReadResult::Error;
    }

    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in.is_open()) return ReadResult::Error;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    GetWriteTime(path, writeTime);

    // An empty file is a freshly created share; anything else must parse completely.
    if (content.find_first_not_of(" \t\r\n") == std::string::npos) return ReadResult::Ok;
    std::vector<std::string> objects;
    if (!JsonLite::ReadObjectArray(content, "tasks", objects)) return ReadResult::Error;

    for (const std::string& object : objects) {
        SyncRecord record;
        if (ParseRecord(object, record)) Upsert(records, record);
    }
    return ReadResult::Ok;
}

bool SyncFiles::Write(const std::wstring& path, const std::vector<SyncRecord>& records, FILETIME& writeTime) {
    TaskUtils::EnsureParentDirectoryExists(path);
    std::ofstream out(path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out.is_open()) return false;

    out << "{\n  \"tasks\": [\n";
    for (size_t i = 0; i < records.size(); ++i) {
        const SyncRecord& r = records[i];
        out << "    {\n"
            << "      \"uid\": \"" << TaskUtils::EscapeJsonString(r.uid) << "\",\n"
            << "      \"text\": " << Quote(r.text) << ",\n"
            << "      \"completed\": " << (r.completed ? "true" : "false") << ",\n"
            << "      \"created_at\": " << static_cast<long long>(r.created_at) << ",\n"
            << "      \"completed_at\": " << static_cast<long long>(r.completed_at) << ",\n"
            << "      \"updated_at\": " << static_cast<long long>(r.updated_at) << ",\n"
            << "      \"updated_by\": " << Quote(r.updated_by) << ",\n"
            << "      \"author\": " << Quote(r.author) << ",\n"
            << "      \"assignee\": " << Quote(r.assignee) << ",\n"
            << "      \"deleted\": " << (r.deleted ? "true" : "false") << "\n"
            << "    }" << (i + 1 < records.size() ? ",\n" : "\n");
    }
    out << "  ]\n}\n";
    out.close();
    if (out.fail()) return false;
    GetWriteTime(path, writeTime);
    return true;
}

void SyncFiles::MergeNewest(RecordMap& winners, const std::vector<SyncRecord>& records) {
    for (const SyncRecord& record : records) {
        auto it = winners.find(record.uid);
        if (it == winners.end()) {
            winners.emplace(record.uid, record);
        } else if (record.updated_at > it->second.updated_at) {
            it->second = record;
        }
    }
}

bool SyncFiles::ApplyNewer(std::vector<SyncRecord>& records, const RecordMap& winners) {
    bool changed = false;
    for (SyncRecord& record : records) {
        auto it = winners.find(record.uid);
        if (it != winners.end() && it->second.updated_at > record.updated_at) {
            record = it->second;
            changed = true;
        }
    }
    return changed;
}

bool SyncFiles::Upsert(std::vector<SyncRecord>& records, const SyncRecord& record) {
    for (SyncRecord& existing : records) {
        if (existing.uid != record.uid) continue;
        if (record.updated_at <= existing.updated_at) return false;
        existing = record;
        return true;
    }
    records.push_back(record);
    return true;
}

bool SyncFiles::PurgeTombstones(std::vector<SyncRecord>& records, time_t now) {
    const size_t before = records.size();
    records.erase(std::remove_if(records.begin(), records.end(), [now](const SyncRecord& record) {
        return record.deleted && now - record.updated_at > TOMBSTONE_RETENTION_SECONDS;
    }), records.end());
    return records.size() != before;
}

bool SyncFiles::Contains(const std::vector<SyncRecord>& records, const std::string& uid) {
    return std::any_of(records.begin(), records.end(),
        [&uid](const SyncRecord& record) { return record.uid == uid; });
}

std::string SyncFiles::NewUid() {
    UUID uuid{};
    UuidCreate(&uuid);
    char buffer[40];
    sprintf_s(buffer, "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid.Data1, uuid.Data2, uuid.Data3, uuid.Data4[0], uuid.Data4[1], uuid.Data4[2],
        uuid.Data4[3], uuid.Data4[4], uuid.Data4[5], uuid.Data4[6], uuid.Data4[7]);
    return buffer;
}

std::wstring SyncFiles::DefaultName(const std::wstring& path) {
    size_t fileSlash = path.find_last_of(L"\\/");
    if (fileSlash == std::wstring::npos || fileSlash == 0) return L"Szinkron";
    size_t folderSlash = path.find_last_of(L"\\/", fileSlash - 1);
    size_t start = folderSlash == std::wstring::npos ? 0 : folderSlash + 1;
    std::wstring name = path.substr(start, fileSlash - start);
    return name.empty() ? L"Szinkron" : name;
}
