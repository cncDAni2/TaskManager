#pragma once

#include "AppTypes.h"
#include "ExcludedApps.h"
#include "Hotkeys.h"
#include "Recurring.h"
#include "SyncFiles.h"
#include "TaskUtils.h"
#include "VersionHint.h"
#include "WorkHistory.h"
#include <map>
#include <set>
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
    std::array<unsigned, Hotkeys::ACTION_COUNT> hotkeys = Hotkeys::DEFAULT_COMBOS;
    bool work_tracking_enabled = true; // Off: no work-time measurement and no work bar; history is kept.
    // GitHub release check (UpdateCheck / UpdateOverlay).
    bool update_check_enabled = true;
    time_t update_last_check = 0;     // Last completed check (also set after giving up retries).
    int update_failures = 0;          // Consecutive failed attempts since then.
    time_t update_last_failure = 0;
    std::string update_latest_version; // Latest release seen, e.g. "1.18.0".
    std::string update_dismissed_date; // YYYY-MM-DD when the overlay was closed.
    std::wstring excluded_window_keywords = ExcludedApps::DEFAULT_KEYWORDS;
    int cleanup_days = 14;
    int cleanup_weeks = 8;
    ThemeMode theme_mode = ThemeMode::Dark;
    WorkHistory::Records work_history;
    WorkHistory::Records manual_work_history;
    std::wstring filePath;
    std::vector<SyncSource> syncSources;
    int next_source_id = 1;
    std::vector<int> active_order;
    std::vector<RecurringRule> recurring_rules;
    std::vector<int> newRecurringTaskIds; // Created by a work-day rollover; the UI puts them on top.
    std::vector<SyncNotice> pendingNotices;
    std::vector<VersionNotice> pendingVersionNotices; // Queued even when reloading silently.

    TaskStore();

    // On a new work day also creates the due recurring tasks (see newRecurringTaskIds).
    void CheckWorkReset();
    // Returns the runtime id of the new task, or 0 when nothing was added.
    int Add(const std::wstring& text, const std::vector<int>& sourceIds, const std::string& recurringUid = "");
    void UpdateText(int id, const std::wstring& newText);
    void CycleMarker(int id);
    void ToggleCompleted(int id);
    std::wstring ToggleAssignment(int id, const std::wstring& userName);
    void Delete(int id);
    // Deletes several tasks with one write per sync file.
    void DeleteMany(const std::vector<int>& ids);
    int CountWorkHistoryBefore(const std::string& dateKey) const;
    void PurgeWorkHistoryBefore(const std::string& dateKey);

    // Puts open tasks missing from `order` (e.g. new ones from a sync file) on top, newest first.
    void PrependUnorderedActive(std::vector<int>& order) const;
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

    // Recurring and later tasks (TaskRecurring.cpp)
    // The work day the reset time belongs to (before the reset time: the previous day).
    std::string CurrentWorkDay() const;
    // Adds or replaces (same uid) a rule. An instance starting today is created at once, and a
    // one-off rule that ran is not kept. Returns the ids of the created tasks.
    std::vector<int> SaveRecurringRule(RecurringRule rule);
    void DeleteRecurringRule(const std::string& uid);
    // Creates the instances due on the current work day; returns their ids.
    std::vector<int> RunDueRecurring();

    void SaveLocal();
    void Save();
    void LoadLocal();
    void Load();

private:
    std::map<std::string, int> runtimeIds;
    int nextRuntimeId = 1;
    std::map<std::string, std::wstring> knownSyncTexts;
    bool syncKnownInitialized = false;
    std::set<std::wstring> shownVersionHints; // Each hint is shown once per run.

    int RuntimeIdFor(const std::string& uid);
    void AddFirstRunSyncSource();
    Task* FindTask(int id);
    bool IsOpenTask(const std::string& uid) const;
    int CreateRecurringInstance(RecurringRule& rule);
    void PublishSyncTask(Task& task, bool deleted);
    SyncRecord MakeSyncRecord(Task& task, bool deleted);
    void RewriteSource(SyncSource& source, const SyncFiles::RecordMap* winners, const SyncRecord* upsert);
    void NoteVersionHint(const SyncSource& source, const SyncFileInfo& info);
};
