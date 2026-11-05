#include "Task.h"
#include <algorithm>

void TaskStore::RestoreTasks(std::vector<Task> saved, const OrderSlots& orderSlots) {
    std::vector<SyncSource*> touched;
    for (Task& task : saved) {
        // A sync file removed in the meantime cannot take the task back: it stays here as a local one.
        task.sources.erase(std::remove_if(task.sources.begin(), task.sources.end(),
            [this](int sourceId) { return FindSource(sourceId) == nullptr; }), task.sources.end());
        task.is_sync = !task.sources.empty();
        if (task.is_sync) QueueSyncRecord(task, false, touched);
        else tasks.push_back(task);
    }
    for (SyncSource* source : touched) RewriteSource(*source, nullptr, nullptr);

    for (const auto& slot : orderSlots) {
        const auto position = active_order.begin() + (std::min)(slot.first, static_cast<int>(active_order.size()));
        active_order.insert(position, slot.second);
    }
    if (!touched.empty()) ReloadSync(false, false);
    SaveLocal();
}

void TaskStore::RestoreWorkHistory(const WorkHistory::Records& measured, const WorkHistory::Records& manual) {
    work_history.insert(measured.begin(), measured.end());
    manual_work_history.insert(manual.begin(), manual.end());
    SaveLocal();
}
