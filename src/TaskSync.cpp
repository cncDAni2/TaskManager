#include "Task.h"
#include <algorithm>

namespace {
const std::wstring& CurrentUser() {
    static const std::wstring user = TaskUtils::GetCleanUserName();
    return user;
}

bool SameFileTime(const FILETIME& left, const FILETIME& right) {
    return left.dwLowDateTime == right.dwLowDateTime && left.dwHighDateTime == right.dwHighDateTime;
}

bool IsZero(const FILETIME& time) {
    return time.dwLowDateTime == 0 && time.dwHighDateTime == 0;
}
}

void TaskStore::SetSyncSources(const std::vector<SyncSource>& sources) {
    syncSources.clear();
    for (const SyncSource& source : sources) {
        if (source.path.empty()) continue;
        SyncSource copy;
        copy.id = source.id > 0 ? source.id : next_source_id++;
        copy.name = source.name;
        copy.path = source.path;
        copy.isDefault = source.isDefault;
        next_source_id = (std::max)(next_source_id, copy.id + 1);
        // A newly named file is created right away, so it can be shared before the first task.
        if (GetFileAttributesW(copy.path.c_str()) == INVALID_FILE_ATTRIBUTES &&
            GetLastError() == ERROR_FILE_NOT_FOUND) {
            SyncFiles::Write(copy.path, {}, SyncFileInfo(), copy.lastWrite);
            copy.lastWrite = { 0, 0 }; // Still read it on the reload below.
        }
        syncSources.push_back(copy);
    }
    ReloadSync(true, true);
    SaveLocal();
}

const SyncSource* TaskStore::FindSource(int sourceId) const {
    for (const SyncSource& source : syncSources) {
        if (source.id == sourceId) return &source;
    }
    return nullptr;
}

std::vector<int> TaskStore::DefaultSourceIds() const {
    std::vector<int> ids;
    for (const SyncSource& source : syncSources) {
        if (source.isDefault) ids.push_back(source.id);
    }
    return ids;
}

bool TaskStore::CheckSyncFilesChanged() {
    for (const SyncSource& source : syncSources) {
        if (!source.pendingWrites.empty()) return true;
        FILETIME writeTime{};
        if (SyncFiles::GetWriteTime(source.path, writeTime)) {
            if (!SameFileTime(writeTime, source.lastWrite)) return true;
        } else if (!IsZero(source.lastWrite)) {
            return true;
        }
    }
    return false;
}

void TaskStore::RewriteSource(SyncSource& source, const SyncFiles::RecordMap* winners, const SyncRecord* upsert) {
    if (upsert) {
        SyncFiles::Upsert(source.pendingWrites, *upsert);
        SyncFiles::Upsert(source.records, *upsert);
    }

    // Read -> merge -> write, so changes made by others since our last read are kept.
    std::vector<SyncRecord> fresh;
    SyncFileInfo info;
    FILETIME writeTime{};
    if (SyncFiles::Read(source.path, fresh, info, writeTime) == SyncFiles::ReadResult::Error) return;
    NoteVersionHint(source, info);

    bool changed = false;
    if (winners) changed |= SyncFiles::ApplyNewer(fresh, *winners);
    for (const SyncRecord& pending : source.pendingWrites) {
        changed |= SyncFiles::Upsert(fresh, pending);
    }
    changed |= SyncFiles::PurgeTombstones(fresh, time(nullptr));

    if (changed) {
        if (!SyncFiles::Write(source.path, fresh, info, writeTime)) return;
        source.lastWrite = writeTime;
    }
    source.pendingWrites.clear();
    source.records = std::move(fresh);
}

void TaskStore::NoteVersionHint(const SyncSource& source, const SyncFileInfo& info) {
    VersionNotice notice;
    if (!VersionHint::Describe(source.name, info, notice)) return;
    if (!shownVersionHints.insert(source.path + L"|" + notice.heading + L"|" + notice.body).second) return;
    pendingVersionNotices.push_back(notice);
}

void TaskStore::ReloadSync(bool forceAll, bool silent) {
    for (SyncSource& source : syncSources) {
        FILETIME current{};
        const bool exists = SyncFiles::GetWriteTime(source.path, current);
        if (!forceAll && source.pendingWrites.empty()) {
            if (exists && SameFileTime(current, source.lastWrite)) continue;
            if (!exists && IsZero(source.lastWrite)) continue;
        }

        std::vector<SyncRecord> records;
        SyncFileInfo info;
        FILETIME writeTime{};
        switch (SyncFiles::Read(source.path, records, info, writeTime)) {
        case SyncFiles::ReadResult::Ok:
            NoteVersionHint(source, info);
            for (const SyncRecord& pending : source.pendingWrites) SyncFiles::Upsert(records, pending);
            source.records = std::move(records);
            source.lastWrite = writeTime;
            break;
        case SyncFiles::ReadResult::Missing:
            source.records = source.pendingWrites;
            source.lastWrite = { 0, 0 };
            break;
        case SyncFiles::ReadResult::Error:
            break; // Keep the cached contents and retry on the next check.
        }
    }

    SyncFiles::RecordMap winners;
    for (const SyncSource& source : syncSources) {
        SyncFiles::MergeNewest(winners, source.records);
    }

    // Relay: bring every file that holds an older copy (or has unsaved own changes) up to date.
    // The winner keeps its original updated_at/updated_by, so relaying never looks like a new edit.
    for (SyncSource& source : syncSources) {
        std::vector<SyncRecord> probe = source.records;
        if (!source.pendingWrites.empty() || SyncFiles::ApplyNewer(probe, winners)) {
            RewriteSource(source, &winners, nullptr);
        }
    }

    std::vector<const SyncRecord*> visible;
    for (const auto& entry : winners) {
        if (!entry.second.deleted) visible.push_back(&entry.second);
    }
    std::sort(visible.begin(), visible.end(), [](const SyncRecord* a, const SyncRecord* b) {
        return a->created_at < b->created_at;
    });

    tasks.erase(std::remove_if(tasks.begin(), tasks.end(),
        [](const Task& t) { return t.is_sync; }), tasks.end());
    std::map<std::string, std::wstring> currentTexts;
    for (const SyncRecord* record : visible) {
        Task t;
        t.uid = record->uid;
        t.id = RuntimeIdFor(record->uid);
        t.is_sync = true;
        t.text = record->text;
        t.completed = record->completed;
        t.created_at = record->created_at;
        t.completed_at = record->completed_at;
        t.author = record->author;
        t.assignee = record->assignee;
        t.updated_at = record->updated_at;
        t.updated_by = record->updated_by;
        for (const SyncSource& source : syncSources) {
            if (SyncFiles::Contains(source.records, record->uid)) t.sources.push_back(source.id);
        }
        tasks.push_back(t);
        currentTexts[record->uid] = record->text;

        if (silent || !syncKnownInitialized || record->updated_by == CurrentUser()) continue;
        auto known = knownSyncTexts.find(record->uid);
        if (known == knownSyncTexts.end()) {
            if (!record->completed) {
                pendingNotices.push_back({ true, record->text, L"",
                    record->author.empty() ? record->updated_by : record->author });
            }
        } else if (known->second != record->text) {
            pendingNotices.push_back({ false, record->text, known->second, record->updated_by });
        }
    }
    knownSyncTexts = std::move(currentTexts);
    syncKnownInitialized = true;
}

SyncRecord TaskStore::MakeSyncRecord(Task& task, bool deleted) {
    SyncRecord record;
    record.uid = task.uid;
    record.text = task.text;
    record.completed = task.completed;
    record.created_at = task.created_at;
    record.completed_at = task.completed_at;
    record.author = task.author;
    record.assignee = task.assignee;
    record.deleted = deleted;
    // Keep what a newer version stored with this task, taken from the newest copy we know.
    const SyncRecord* known = nullptr;
    for (const SyncSource& source : syncSources) {
        for (const SyncRecord& candidate : source.records) {
            if (candidate.uid == task.uid && (!known || candidate.updated_at > known->updated_at)) known = &candidate;
        }
    }
    if (known) record.extras = known->extras;
    // Always strictly newer than the version we started from, even with a lagging clock.
    record.updated_at = (std::max)(time(nullptr), task.updated_at + 1);
    // updated_by names who created or last rewrote the text (set by Add/UpdateText); ticking,
    // "ÉN" and deleting keep it. The merge relies on updated_at alone.
    record.updated_by = task.updated_by.empty() ? CurrentUser() : task.updated_by;

    task.updated_at = record.updated_at;
    task.updated_by = record.updated_by;
    if (!deleted) knownSyncTexts[task.uid] = task.text;
    return record;
}

void TaskStore::PublishSyncTask(Task& task, bool deleted) {
    const SyncRecord record = MakeSyncRecord(task, deleted);
    const std::vector<int> targets = task.sources;
    for (int sourceId : targets) {
        for (SyncSource& source : syncSources) {
            if (source.id == sourceId) RewriteSource(source, nullptr, &record);
        }
    }
    // `task` may be rebuilt below; it must not be used after this point.
    ReloadSync(false, false);
}
