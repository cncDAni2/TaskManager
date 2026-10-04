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
    int work_reset_hour = 9;
    int work_reset_minute = 15;
    double timer_work_minutes = 50.0;
    double timer_rest_minutes = 10.0;
    int timer_repetitions = 8;
    bool timer_2020_enabled = false;
    bool timer_start_with_app = false;
    bool sounds_enabled = true;
    ThemeMode theme_mode = ThemeMode::Dark;
    WorkHistory::Records work_history;
    WorkHistory::Records manual_work_history;
    std::wstring filePath;
    std::wstring syncFilePath;
    FILETIME lastSyncFileTime = { 0, 0 };
    std::vector<int> active_order;

    TaskStore();

    void CheckWorkReset();
    void Add(const std::wstring& text, bool isSync = false);
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

    void SetSyncFilePath(const std::wstring& newPath);
    void SaveLocal();
    void SaveSync();
    void Save();
    bool CheckSyncFileChanged();
    bool LoadSync();
    void LoadLocal();
    void Load();
};
