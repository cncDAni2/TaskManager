#pragma once

#include <windows.h>
#include "JsonLite.h"
#include <ctime>
#include <map>
#include <string>
#include <vector>

// One task entry as stored in a shared sync file. Deleted entries are kept as tombstones
// so that relaying peers cannot resurrect them.
struct SyncRecord {
    std::string uid;
    std::wstring text;
    bool completed = false;
    time_t created_at = 0;
    time_t completed_at = 0;
    time_t updated_at = 0;
    std::wstring updated_by; // Who created or last rewrote the text; the merge uses only updated_at.
    std::wstring author;
    std::wstring assignee;
    bool deleted = false;
    std::string recurring_uid; // Written only when set.
    // Members this version does not know (written by a newer one); kept and written back unchanged.
    std::vector<JsonLite::Member> extras;
};

// File-level data besides the tasks.
struct SyncFileInfo {
    std::string appVersion; // Version of the app that last wrote the file; empty for older writers.
    std::vector<JsonLite::Member> extras; // Unknown top-level members, written back unchanged.
    std::vector<std::string> unknownKeys; // Unknown member names anywhere in the file (for the version hint).
};

// A configured shared file. `id` is a local, never-reused identifier.
struct SyncSource {
    int id = 0;
    std::wstring name;
    std::wstring path;
    bool isDefault = false;
    FILETIME lastWrite = { 0, 0 };
    std::vector<SyncRecord> records;
    std::vector<SyncRecord> pendingWrites; // Own changes not yet written (file was locked/unreadable).
};

namespace SyncFiles {
    using RecordMap = std::map<std::string, SyncRecord>;

    enum class ReadResult { Ok, Missing, Error };

    // Error means the file exists but is unreadable or truncated; callers must not overwrite it.
    ReadResult Read(const std::wstring& path, std::vector<SyncRecord>& records, SyncFileInfo& info,
        FILETIME& writeTime);
    // Stamps the file with this app's version; unknown members in `info` and the records are kept.
    bool Write(const std::wstring& path, const std::vector<SyncRecord>& records, const SyncFileInfo& info,
        FILETIME& writeTime);
    bool GetWriteTime(const std::wstring& path, FILETIME& writeTime);

    // Keeps the most recently updated version of every uid.
    void MergeNewest(RecordMap& winners, const std::vector<SyncRecord>& records);

    // Replaces entries older than their winner; never adds uids the file does not contain.
    bool ApplyNewer(std::vector<SyncRecord>& records, const RecordMap& winners);

    // Inserts or replaces the given record when it is newer than the stored one.
    bool Upsert(std::vector<SyncRecord>& records, const SyncRecord& record);

    // Drops tombstones older than the retention period.
    bool PurgeTombstones(std::vector<SyncRecord>& records, time_t now);

    bool Contains(const std::vector<SyncRecord>& records, const std::string& uid);

    std::string NewUid();

    // Display name for a file, taken from its folder: "...\Csapat\tasks.json" -> "Csapat".
    std::wstring DefaultName(const std::wstring& path);
}
