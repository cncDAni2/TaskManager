#pragma once

#include "AppTypes.h"
#include "ExcludedApps.h"
#include "SyncFiles.h"
#include "TaskUtils.h"
#include "WorkHistory.h"
#include <map>
#include <string>
#include <vector>

// A synchronized change worth telling the user about.
struct SyncNotice {
    bool isNew = false;
    std::wstring text;
    std::wstring previousText;
    std::wstring changedBy;
};

class TaskStore {
public:
    std::vector<Task> tasks;
    int work_seconds_today = 0;
    time_t last_reset_time = 0;
    int work_reset_hour = 9;
    int work_reset_minute = 15;
    double timer_work_minutes = 50.0;
    double timer_rest_minutes = 10.0;
    int timer_repetitions = 8;
    bool timer_2020_enabled = false;
    bool timer_strict_mode = false;
    bool timer_start_with_app = false;
    bool sounds_enabled = true;
    std::wstring excluded_window_keywords = ExcludedApps::DEFAULT_KEYWORDS;
    ThemeMode theme_mode = ThemeMode::Dark;
    WorkHistory::Records work_history;
    WorkHistory::Records manual_work_history;
    std::wstring filePath;
    std::vector<SyncSource> syncSources;
    int next_source_id = 1;
    std::vector<int> active_order;
    std::vector<SyncNotice> pendingNotices;

    TaskStore();

    void CheckWorkReset();
    // Returns the runtime id of the new task, or 0 when nothing was added.
    int Add(const std::wstring& text, const std::vector<int>& sourceIds);
    void UpdateText(int id, const std::wstring& newText);
    void CycleMarker(int id);
    void ToggleCompleted(int id);
    std::wstring ToggleAssignment(int id, const std::wstring& userName);
    void Delete(int id);

    std::vector<Task> GetActiveTasks() const;
    std::vector<Task> GetRecentCompletedTasks() const;
    std::vector<Task> GetAllCompletedTasks() const;
    std::vector<WorkHistory::DisplayEntry> GetWorkHistory() const;
    void SetManualWork(const std::string& date, int seconds);

    // Sync (TaskSync.cpp)
    void SetSyncSources(const std::vector<SyncSource>& sources);
    const SyncSource* FindSource(int sourceId) const;
    std::vector<int> DefaultSourceIds() const;
    bool CheckSyncFilesChanged();
    // Reads changed (or all) sync files, merges them, relays newer versions, and
    // queues notices into `pendingNotices` unless `silent`.
    void ReloadSync(bool forceAll, bool silent);

    void SaveLocal();
    void Save();
    void LoadLocal();
    void Load();

private:
    std::map<std::string, int> runtimeIds;
    int nextRuntimeId = 1;
    std::map<std::string, std::wstring> knownSyncTexts;
    bool syncKnownInitialized = false;

    int RuntimeIdFor(const std::string& uid);
    void AddFirstRunSyncSource();
    Task* FindTask(int id);
    void PublishSyncTask(Task& task, bool deleted);
    void RewriteSource(SyncSource& source, const SyncFiles::RecordMap* winners, const SyncRecord* upsert);
};
