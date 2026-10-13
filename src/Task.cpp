#include "Task.h"
#include <algorithm>
#include <set>

TaskStore::TaskStore() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0';
        filePath = std::wstring(exePath) + L"tasks-local.json";
    } else {
        filePath = L"tasks-local.json";
    }
    Load();
    CheckWorkReset();
}

void TaskStore::CheckWorkReset() {
    time_t now = time(nullptr);
    time_t cutoff = TaskUtils::GetLastResetCutoff(now, work_reset_hour, work_reset_minute);
    if (last_reset_time == 0) {
        last_reset_time = cutoff;
        SaveLocal();
    } else if (last_reset_time < cutoff) {
        work_history[WorkHistory::DateKey(last_reset_time)] = work_seconds_today;
        work_seconds_today = 0;
        last_reset_time = cutoff;
        SaveLocal();
    }
}

int TaskStore::RuntimeIdFor(const std::string& uid) {
    auto it = runtimeIds.find(uid);
    if (it != runtimeIds.end()) return it->second;
    int id = nextRuntimeId++;
    runtimeIds.emplace(uid, id);
    return id;
}

Task* TaskStore::FindTask(int id) {
    auto it = std::find_if(tasks.begin(), tasks.end(), [id](const Task& t) { return t.id == id; });
    return it == tasks.end() ? nullptr : &*it;
}

int TaskStore::Add(const std::wstring& text, const std::vector<int>& sourceIds) {
    if (text.empty()) return 0;
    Task t;
    t.uid = SyncFiles::NewUid();
    t.id = RuntimeIdFor(t.uid);
    t.text = text;
    t.created_at = time(nullptr);

    for (int sourceId : sourceIds) {
        if (FindSource(sourceId)) t.sources.push_back(sourceId);
    }
    t.is_sync = !t.sources.empty();
    if (t.is_sync) {
        t.author = TaskUtils::GetCleanUserName();
        t.updated_by = t.author;
    }
    tasks.push_back(t);
    active_order.insert(active_order.begin(), t.id);
    const int id = t.id;
    if (t.is_sync) PublishSyncTask(tasks.back(), false);
    SaveLocal();
    return id;
}

void TaskStore::UpdateText(int id, const std::wstring& newText) {
    Task* t = FindTask(id);
    if (!t || t->text == newText) return;
    t->text = newText;
    if (t->is_sync) {
        // Only creating or rewriting the text changes updated_by; ticking or "ÉN" keeps it.
        t->updated_by = TaskUtils::GetCleanUserName();
        PublishSyncTask(*t, false);
    }
    else SaveLocal();
}

void TaskStore::CycleMarker(int id) {
    Task* t = FindTask(id);
    if (!t || t->is_sync) return;
    int nextMarker = (static_cast<int>(t->marker) + 1) % 4;
    t->marker = static_cast<TaskMarker>(nextMarker);
    SaveLocal();
}

void TaskStore::ToggleCompleted(int id) {
    Task* t = FindTask(id);
    if (!t) return;
    t->completed = !t->completed;
    if (t->completed) {
        t->completed_at = time(nullptr);
        auto it = std::find(active_order.begin(), active_order.end(), id);
        if (it != active_order.end()) active_order.erase(it);
    } else {
        t->completed_at = 0;
        if (std::find(active_order.begin(), active_order.end(), id) == active_order.end()) {
            active_order.push_back(id);
        }
    }
    if (t->is_sync) PublishSyncTask(*t, false);
    SaveLocal();
}

std::wstring TaskStore::ToggleAssignment(int id, const std::wstring& userName) {
    // Re-read first so a colleague's fresh assignment is not overwritten.
    ReloadSync(false, false);

    Task* t = FindTask(id);
    if (!t || !t->is_sync) return L"";
    if (!t->assignee.empty() && t->assignee != userName) return t->assignee;
    t->assignee = (t->assignee == userName) ? L"" : userName;
    PublishSyncTask(*t, false);
    return L"";
}

void TaskStore::Delete(int id) {
    Task* t = FindTask(id);
    if (!t) return;
    if (t->is_sync) PublishSyncTask(*t, true);
    tasks.erase(std::remove_if(tasks.begin(), tasks.end(),
        [id](const Task& task) { return task.id == id; }), tasks.end());
    auto itOrder = std::find(active_order.begin(), active_order.end(), id);
    if (itOrder != active_order.end()) active_order.erase(itOrder);
    SaveLocal();
}

void TaskStore::DeleteMany(const std::vector<int>& ids) {
    std::vector<SyncSource*> touched;
    for (int id : ids) {
        Task* t = FindTask(id);
        if (!t || !t->is_sync) continue;
        const SyncRecord record = MakeSyncRecord(*t, true);
        for (int sourceId : t->sources) {
            for (SyncSource& source : syncSources) {
                if (source.id != sourceId) continue;
                SyncFiles::Upsert(source.pendingWrites, record);
                SyncFiles::Upsert(source.records, record);
                if (std::find(touched.begin(), touched.end(), &source) == touched.end()) touched.push_back(&source);
            }
        }
    }
    for (SyncSource* source : touched) RewriteSource(*source, nullptr, nullptr);

    tasks.erase(std::remove_if(tasks.begin(), tasks.end(), [&ids](const Task& task) {
        return std::find(ids.begin(), ids.end(), task.id) != ids.end();
    }), tasks.end());
    active_order.erase(std::remove_if(active_order.begin(), active_order.end(), [&ids](int id) {
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    }), active_order.end());
    if (!touched.empty()) ReloadSync(false, false);
    SaveLocal();
}

int TaskStore::CountWorkHistoryBefore(const std::string& dateKey) const {
    std::set<std::string> dates;
    for (const auto& entry : work_history) if (entry.first < dateKey) dates.insert(entry.first);
    for (const auto& entry : manual_work_history) if (entry.first < dateKey) dates.insert(entry.first);
    return static_cast<int>(dates.size());
}

void TaskStore::PurgeWorkHistoryBefore(const std::string& dateKey) {
    // Keys are "YYYY-MM-DD", so string order is date order.
    work_history.erase(work_history.begin(), work_history.lower_bound(dateKey));
    manual_work_history.erase(manual_work_history.begin(), manual_work_history.lower_bound(dateKey));
    SaveLocal();
}

std::vector<Task> TaskStore::GetActiveTasks() const {
    std::vector<Task> res;
    for (const auto& t : tasks) {
        if (!t.completed) res.push_back(t);
    }
    return res;
}

std::vector<Task> TaskStore::GetRecentCompletedTasks() const {
    time_t cutoff = TaskUtils::GetYesterday930Cutoff(time(nullptr));
    std::vector<Task> res;
    for (const auto& t : tasks) {
        if (t.completed && t.completed_at >= cutoff) {
            res.push_back(t);
        }
    }
    std::sort(res.begin(), res.end(), [](const Task& a, const Task& b) {
        return a.completed_at > b.completed_at;
    });
    return res;
}

std::vector<Task> TaskStore::GetAllCompletedTasks() const {
    std::vector<Task> res;
    for (const auto& t : tasks) {
        if (t.completed) {
            res.push_back(t);
        }
    }
    std::sort(res.begin(), res.end(), [](const Task& a, const Task& b) {
        return a.completed_at > b.completed_at;
    });
    return res;
}

std::vector<WorkHistory::DisplayEntry> TaskStore::GetWorkHistory() const {
    time_t currentDay = last_reset_time != 0 ? last_reset_time : time(nullptr);
    return WorkHistory::GetDisplayEntries(work_history, manual_work_history, currentDay, work_seconds_today);
}

void TaskStore::SetManualWork(const std::string& date, int seconds) {
    if (date.size() != 10) return;
    if (seconds <= 0) {
        manual_work_history.erase(date);
        auto measuredIt = work_history.find(date);
        if (measuredIt != work_history.end() && measuredIt->second <= 0) work_history.erase(measuredIt);
    } else {
        manual_work_history[date] = seconds;
    }
    SaveLocal();
}

void TaskStore::Save() {
    SaveLocal();
}

void TaskStore::Load() {
    tasks.clear();
    LoadLocal();
    ReloadSync(true, true);

    std::vector<int> reconciled;
    std::set<int> seen;
    for (int id : active_order) {
        const Task* t = FindTask(id);
        if (t && !t->completed && seen.insert(id).second) reconciled.push_back(id);
    }
    for (const auto& t : tasks) {
        if (!t.completed && seen.insert(t.id).second) reconciled.push_back(t.id);
    }
    active_order = reconciled;
    SaveLocal();
}
