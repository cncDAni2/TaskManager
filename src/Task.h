#pragma once

#include "AppTypes.h"
#include "TaskUtils.h"
#include "WorkHistory.h"
#include <string>
#include <vector>

class TaskStore {
public:
    std::vector<Task> tasks;
    int next_id = 1;
    int next_sync_id = 1000001;
    int work_seconds_today = 0;
    time_t last_reset_time = 0;
    WorkHistory::Records work_history;
    std::wstring filePath;
    std::wstring syncFilePath;
    FILETIME lastSyncFileTime = { 0, 0 };
    std::vector<int> active_order;

    TaskStore();

    void CheckWorkReset();
    void Add(const std::wstring& text, bool isSync = false);
    void UpdateText(int id, const std::wstring& newText);
    void ToggleCompleted(int id);
    void ToggleAssignment(int id, const std::wstring& userName);
    void Delete(int id);

    std::vector<Task> GetActiveTasks() const;
    std::vector<Task> GetRecentCompletedTasks() const;
    std::vector<Task> GetAllCompletedTasks() const;
    std::vector<WorkHistory::DisplayEntry> GetWorkHistory() const;

    void SetSyncFilePath(const std::wstring& newPath);
    void SaveLocal();
    void SaveSync();
    void Save();
    bool CheckSyncFileChanged();
    bool LoadSync();
    void LoadLocal();
    void Load();
};
