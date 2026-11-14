#pragma once

// TaskStore core: task operations, runtime-id mapping, active-task ordering, work tracking and
// settings. Split across TaskLocalFile.cpp, TaskSync.cpp, TaskUndo.cpp, TaskGroups.cpp and
// TaskRecurring.cpp.
// - Every task has a persistent string uid (new tasks: GUID; legacy numeric ids are kept as their
//   decimal string, sync ids normalized to 1,000,000+). Task::id is a runtime-only handle from
//   TaskStore::RuntimeIdFor; never persist it.
// - Local tasks-local.json stores local tasks, active ordering (as uids), pin_hidden_tasks (uids
//   left out of the PIN window; local even for synced tasks, mirrored in Task::pin_hidden),
//   task_groups (uid, name, members: never call that key "tasks", the task list is looked up by
//   name in the whole file), the recurring_rules list, current and historical work time (including
//   manual entries), reset timestamp, theme, work-reset time, the sync_files list (local source_id,
//   name, path, default flag), timer durations/repetitions, the 20-20-20 toggle, timer auto-start
//   preference, sound preference, hotkeys, PIN quick buttons (pin_buttons), PIN task list display
//   (pin_list_mode, pin_list_count), work-tracking on/off,
//   update-check state (update_*: enabled, last check, failures, latest release, dismissed date),
//   and excluded window-title keywords. A legacy sync_file_path is migrated to the first sync_files
//   entry.
// - Only a missing file is a first run: if it exists but cannot be read, or its task list is
//   damaged, localLoadFailed blocks every save and wWinMain exits with a message, so it is never
//   overwritten.
// - Call Save() after a change (identical content is not written). Work seconds are saved every 10
//   counted minutes and when counting stops (idle, lock, Időstop, hiding, suspend, Windows
//   shutdown, exit), so a power cut loses at most the last 10 minutes.

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

// Where a deleted task stood in an id list: (index, id), ascending by index.
using OrderSlots = std::vector<std::pair<int, int>>;

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
    std::string pin_buttons = "timer,-add,-focus,-mute,-hold,pin,drag"; // PinButtons::DEFAULT_CONFIG; "-" = hidden.
    int pin_list_mode = 2;  // PIN task list: 0 = never, 1 = while the mouse is over it, 2 = always (PinList::Mode).
    int pin_list_count = 0; // The first this many tasks of the PIN list; 0 = all.
    int cleanup_days = 14;
    int cleanup_weeks = 8;
    ThemeMode theme_mode = ThemeMode::Dark;
    WorkHistory::Records work_history;
    WorkHistory::Records manual_work_history;
    std::wstring filePath;
    std::vector<SyncSource> syncSources;
    int next_source_id = 1;
    std::vector<int> active_order;
    std::set<std::string> pin_hidden_uids; // Local even for synced tasks: it is this user's own view.
    std::vector<TaskGroup> task_groups;
    std::map<std::string, std::string> group_members; // Task uid -> group uid; mirrored in Task::group_uid.
    std::vector<RecurringRule> recurring_rules;
    std::vector<int> newRecurringTaskIds; // Created by a work-day rollover; the UI puts them on top.
    std::vector<SyncNotice> pendingNotices;
    std::vector<VersionNotice> pendingVersionNotices; // Queued even when reloading silently.
    // tasks-local.json exists but could not be read: nothing is saved, so it cannot be overwritten.
    bool localLoadFailed = false;

    TaskStore();

    // On a new work day also creates the due recurring tasks (see newRecurringTaskIds).
    void CheckWorkReset();
    // Returns the runtime id of the new task, or 0 when nothing was added.
    int Add(const std::wstring& text, const std::vector<int>& sourceIds, const std::string& recurringUid = "");
    void UpdateText(int id, const std::wstring& newText);
    void CycleMarker(int id);
    void TogglePinHidden(int id);
    void ToggleCompleted(int id);
    std::wstring ToggleAssignment(int id, const std::wstring& userName);
    // Deletes tasks with one write per sync file.
    void DeleteMany(const std::vector<int>& ids);
    // Undo of DeleteMany (TaskUndo.cpp): `saved` are the tasks as they were before it. Synced tasks
    // come back in their files as a newer change, so the delete is undone for the others too.
    // `groups` brings back the ones that ended with the delete.
    void RestoreTasks(std::vector<Task> saved, const OrderSlots& orderSlots,
        const std::vector<TaskGroup>& groups = {});
    int CountWorkHistoryBefore(const std::string& dateKey) const;
    void PurgeWorkHistoryBefore(const std::string& dateKey);
    void RestoreWorkHistory(const WorkHistory::Records& measured, const WorkHistory::Records& manual);

    // Puts open tasks missing from `order` (e.g. new ones from a sync file) on top, newest first.
    void PrependUnorderedActive(std::vector<int>& order) const;
    std::vector<Task> GetActiveTasks() const;
    std::vector<Task> GetRecentCompletedTasks() const;
    std::vector<Task> GetAllCompletedTasks() const;
    std::vector<WorkHistory::DisplayEntry> GetWorkHistory() const;
    void SetManualWork(const std::string& date, int seconds);

    // Task groups (TaskGroups.cpp). `order` is the list being shown; these change it in place.
    const TaskGroup* FindGroup(const std::string& uid) const;
    std::vector<TaskGroup> GroupsOf(const std::vector<Task>& members) const;
    // Pulls the members of every group next to the first one.
    void NormalizeGroups(std::vector<int>& order) const;
    // Puts the task in front of `beforeId` (0 = the end) as a member of `groupUid` (empty = none).
    void PlaceTask(std::vector<int>& order, int id, int beforeId, const std::string& groupUid);
    void MoveGroup(std::vector<int>& order, const std::string& groupUid, int beforeId);
    // Joins the group of the target, right after it; returns the uid when this created the group.
    std::string GroupWith(std::vector<int>& order, int id, int targetId);
    // Takes the task out, to right above its group.
    void RemoveFromGroup(std::vector<int>& order, int id);
    void RenameGroup(const std::string& uid, const std::wstring& name);
    // Finished tasks leave their groups when the list session ends. True when anything changed.
    bool DropCompletedFromGroups();
    // A drop in the PIN window never changes groups. Returns the task to go in front of (0 = the
    // end); `wholeGroup`: the task is leaving its group, so all of the group moves there.
    int ResolvePinDrop(const std::vector<int>& order, int id, int beforeId, bool& wholeGroup) const;

    // Sync (TaskSync.cpp)
    void SetSyncSources(const std::vector<SyncSource>& sources);
    const SyncSource* FindSource(int sourceId) const;
    std::vector<int> DefaultSourceIds() const;
    // Reads changed (or all) sync files, merges them, relays newer versions, and
    // queues notices into `pendingNotices` unless `silent`.
    void ReloadSync(bool forceAll, bool silent);
    // The timed check reads the files on another thread (SyncPoll): this is what it starts from,
    // and ApplyPolled takes its result. A file written here in the meantime is left for the next check.
    std::vector<SyncFiles::PollFile> PollSnapshot() const;
    void ApplyPolled(std::vector<SyncFiles::PollFile>& files);

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
    std::string savedLocalContent; // What tasks-local.json holds now, so unchanged saves are skipped.
    std::map<std::string, int> runtimeIds;
    int nextRuntimeId = 1;
    std::map<std::string, std::wstring> knownSyncTexts;
    bool syncKnownInitialized = false;
    std::set<std::wstring> shownVersionHints; // Each hint is shown once per run.

    int RuntimeIdFor(const std::string& uid);
    void AddFirstRunSyncSource();
    Task* FindTask(int id);
    bool IsOpenTask(const std::string& uid) const;
    const Task* GroupedTask(int id) const;
    void ForgetGroupMember(const std::string& taskUid); // A group ends with its last member.
    void SetTaskGroup(Task& task, const std::string& groupUid);
    int SnapOutOfGroup(const std::vector<int>& order, int beforeId) const;
    int CreateRecurringInstance(RecurringRule& rule);
    void PublishSyncTask(Task& task, bool deleted);
    // Records a change in the task's files in memory; RewriteSource on `touched` writes them.
    void QueueSyncRecord(Task& task, bool deleted, std::vector<SyncSource*>& touched);
    SyncRecord MakeSyncRecord(Task& task, bool deleted);
    void RewriteSource(SyncSource& source, const SyncFiles::RecordMap* winners, const SyncRecord* upsert);
    void NoteVersionHint(const SyncSource& source, const SyncFileInfo& info);
    void StoreRead(SyncSource& source, SyncFiles::ReadResult result, std::vector<SyncRecord>& records,
        const SyncFileInfo& info, const FILETIME& writeTime);
    void MergeSources(bool silent);
};
