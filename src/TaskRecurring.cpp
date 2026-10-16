#include "Task.h"
#include <algorithm>

std::string TaskStore::CurrentWorkDay() const {
    return WorkHistory::DateKey(TaskUtils::GetLastResetCutoff(time(nullptr), work_reset_hour, work_reset_minute));
}

bool TaskStore::IsOpenTask(const std::string& uid) const {
    if (uid.empty()) return false;
    for (const Task& task : tasks) {
        if (task.uid == uid) return !task.completed;
    }
    return false; // Deleted (or a sync file no longer configured).
}

// Removed target files fall back to a local task, so a due task is never lost.
int TaskStore::CreateRecurringInstance(RecurringRule& rule) {
    std::vector<int> sources;
    for (int sourceId : rule.targets) {
        if (FindSource(sourceId)) sources.push_back(sourceId);
    }
    const int id = Add(rule.text, sources, rule.repeat_count > 0 ? rule.uid : "");
    if (const Task* task = FindTask(id)) rule.last_task_uid = task->uid;
    return id;
}

std::vector<int> TaskStore::SaveRecurringRule(RecurringRule rule) {
    std::vector<int> created;
    auto existing = std::find_if(recurring_rules.begin(), recurring_rules.end(),
        [&rule](const RecurringRule& r) { return r.uid == rule.uid; });
    if (existing == recurring_rules.end()) {
        rule.uid = SyncFiles::NewUid();
        rule.created_at = time(nullptr);
        rule.last_task_uid.clear();
    } else {
        rule.created_at = existing->created_at;
        rule.last_task_uid = existing->last_task_uid;
    }

    // "Today" is the calendar date here: a rule saved before the reset time still starts at once.
    const std::string today = Recurring::Today();
    rule.next_due = rule.start_date;
    bool keep = true;
    if (rule.next_due <= today) {
        if (!IsOpenTask(rule.last_task_uid)) {
            const int id = CreateRecurringInstance(rule);
            if (id != 0) created.push_back(id);
        }
        keep = rule.repeat_count > 0;
        if (keep) rule.next_due = Recurring::NextAfter(rule, today);
    }

    // Add() may rebuild the task list but never touches the rules, yet re-find to be safe.
    existing = std::find_if(recurring_rules.begin(), recurring_rules.end(),
        [&rule](const RecurringRule& r) { return r.uid == rule.uid; });
    if (!keep) {
        if (existing != recurring_rules.end()) recurring_rules.erase(existing);
    } else if (existing != recurring_rules.end()) {
        *existing = rule;
    } else {
        recurring_rules.push_back(rule);
    }
    SaveLocal();
    return created;
}

void TaskStore::DeleteRecurringRule(const std::string& uid) {
    const size_t before = recurring_rules.size();
    recurring_rules.erase(std::remove_if(recurring_rules.begin(), recurring_rules.end(),
        [&uid](const RecurringRule& rule) { return rule.uid == uid; }), recurring_rules.end());
    if (recurring_rules.size() != before) SaveLocal();
}

std::vector<int> TaskStore::RunDueRecurring() {
    std::vector<int> created;
    const std::string today = CurrentWorkDay();
    bool changed = false;
    for (size_t i = 0; i < recurring_rules.size();) {
        RecurringRule& rule = recurring_rules[i];
        if (rule.next_due.empty() || rule.next_due > today) {
            ++i;
            continue;
        }
        changed = true;
        // Missed occurrences (e.g. a holiday) give a single instance, and none while the last one is open.
        if (!IsOpenTask(rule.last_task_uid)) {
            const int id = CreateRecurringInstance(rule);
            if (id != 0) created.push_back(id);
        }
        if (rule.repeat_count <= 0) {
            recurring_rules.erase(recurring_rules.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        rule.next_due = Recurring::NextAfter(rule, today);
        ++i;
    }
    if (changed) SaveLocal();
    return created;
}
