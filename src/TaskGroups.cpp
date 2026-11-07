// Task groups: this user's own view, like the order of the list. They live in tasks-local.json and
// never reach a sync file. A group's tasks stand next to each other in the flat order lists.
#include "Task.h"
#include <algorithm>
#include <iterator>

namespace {
constexpr wchar_t kDefaultGroupName[] = L"Csoport";
}

const Task* TaskStore::GroupedTask(int id) const {
    auto it = std::find_if(tasks.begin(), tasks.end(), [id](const Task& task) { return task.id == id; });
    return it == tasks.end() ? nullptr : &*it;
}

const TaskGroup* TaskStore::FindGroup(const std::string& uid) const {
    auto it = std::find_if(task_groups.begin(), task_groups.end(),
        [&uid](const TaskGroup& group) { return group.uid == uid; });
    return it == task_groups.end() ? nullptr : &*it;
}

std::vector<TaskGroup> TaskStore::GroupsOf(const std::vector<Task>& members) const {
    std::vector<TaskGroup> groups;
    for (const TaskGroup& group : task_groups) {
        if (std::any_of(members.begin(), members.end(),
            [&group](const Task& task) { return task.group_uid == group.uid; })) {
            groups.push_back(group);
        }
    }
    return groups;
}

void TaskStore::ForgetGroupMember(const std::string& taskUid) {
    auto member = group_members.find(taskUid);
    if (member == group_members.end()) return;
    const std::string groupUid = member->second;
    group_members.erase(member);
    const bool stillUsed = std::any_of(group_members.begin(), group_members.end(),
        [&groupUid](const auto& entry) { return entry.second == groupUid; });
    if (stillUsed) return;
    task_groups.erase(std::remove_if(task_groups.begin(), task_groups.end(),
        [&groupUid](const TaskGroup& group) { return group.uid == groupUid; }), task_groups.end());
}

void TaskStore::SetTaskGroup(Task& task, const std::string& groupUid) {
    if (task.group_uid == groupUid) return;
    ForgetGroupMember(task.uid);
    task.group_uid = groupUid;
    if (!groupUid.empty()) group_members[task.uid] = groupUid;
}

void TaskStore::NormalizeGroups(std::vector<int>& order) const {
    if (task_groups.empty()) return;
    std::vector<const std::string*> groups(order.size(), nullptr);
    for (size_t i = 0; i < order.size(); ++i) {
        const Task* task = GroupedTask(order[i]);
        if (task && !task->group_uid.empty()) groups[i] = &task->group_uid;
    }

    std::vector<int> result;
    result.reserve(order.size());
    std::vector<bool> placed(order.size(), false);
    for (size_t i = 0; i < order.size(); ++i) {
        if (placed[i]) continue;
        result.push_back(order[i]);
        if (!groups[i]) continue;
        for (size_t j = i + 1; j < order.size(); ++j) {
            if (!placed[j] && groups[j] && *groups[j] == *groups[i]) {
                result.push_back(order[j]);
                placed[j] = true;
            }
        }
    }
    order.swap(result);
}

void TaskStore::PlaceTask(std::vector<int>& order, int id, int beforeId, const std::string& groupUid) {
    Task* task = FindTask(id);
    auto it = std::find(order.begin(), order.end(), id);
    if (!task || it == order.end()) return;
    if (beforeId == id) beforeId = (it + 1 == order.end()) ? 0 : *(it + 1);
    order.erase(it);
    order.insert(beforeId ? std::find(order.begin(), order.end(), beforeId) : order.end(), id);
    SetTaskGroup(*task, groupUid);
    NormalizeGroups(order);
}

void TaskStore::MoveGroup(std::vector<int>& order, const std::string& groupUid, int beforeId) {
    const auto isMember = [this, &groupUid](int id) {
        const Task* task = GroupedTask(id);
        return task && task->group_uid == groupUid;
    };
    if (beforeId && isMember(beforeId)) return; // Its own place.
    std::vector<int> members;
    std::copy_if(order.begin(), order.end(), std::back_inserter(members), isMember);
    order.erase(std::remove_if(order.begin(), order.end(), isMember), order.end());
    order.insert(beforeId ? std::find(order.begin(), order.end(), beforeId) : order.end(),
        members.begin(), members.end());
}

std::string TaskStore::GroupWith(std::vector<int>& order, int id, int targetId) {
    Task* task = FindTask(id);
    Task* target = FindTask(targetId);
    if (!task || !target || id == targetId) return "";

    std::string created;
    if (target->group_uid.empty()) {
        created = SyncFiles::NewUid();
        task_groups.push_back({ created, kDefaultGroupName });
        SetTaskGroup(*target, created);
    }
    const std::string groupUid = target->group_uid; // A copy: SetTaskGroup may erase the old group.
    auto it = std::find(order.begin(), order.end(), id);
    if (it != order.end()) order.erase(it);
    auto targetPosition = std::find(order.begin(), order.end(), targetId);
    order.insert(targetPosition == order.end() ? order.end() : targetPosition + 1, id);
    SetTaskGroup(*task, groupUid);
    NormalizeGroups(order);
    return created;
}

void TaskStore::RemoveFromGroup(std::vector<int>& order, int id) {
    const Task* task = GroupedTask(id);
    if (!task || task->group_uid.empty()) return;
    const std::string groupUid = task->group_uid;
    auto first = std::find_if(order.begin(), order.end(), [this, &groupUid](int otherId) {
        const Task* other = GroupedTask(otherId);
        return other && other->group_uid == groupUid;
    });
    PlaceTask(order, id, first == order.end() ? id : *first, "");
}

void TaskStore::RenameGroup(const std::string& uid, const std::wstring& name) {
    auto it = std::find_if(task_groups.begin(), task_groups.end(),
        [&uid](const TaskGroup& group) { return group.uid == uid; });
    if (it == task_groups.end() || it->name == name) return;
    it->name = name;
    SaveLocal();
}

bool TaskStore::DropCompletedFromGroups() {
    bool changed = false;
    for (Task& task : tasks) {
        if (!task.completed || task.group_uid.empty()) continue;
        SetTaskGroup(task, "");
        changed = true;
    }
    return changed;
}

int TaskStore::SnapOutOfGroup(const std::vector<int>& order, int beforeId) const {
    const Task* target = beforeId ? GroupedTask(beforeId) : nullptr;
    if (!target || target->group_uid.empty()) return beforeId;

    size_t first = order.size();
    size_t last = 0;
    for (size_t i = 0; i < order.size(); ++i) {
        const Task* task = GroupedTask(order[i]);
        if (!task || task->group_uid != target->group_uid) continue;
        first = (std::min)(first, i);
        last = i;
    }
    if (first == order.size()) return beforeId;
    const size_t slot = std::find(order.begin(), order.end(), beforeId) - order.begin();
    if (slot - first <= last + 1 - slot) return order[first]; // The nearer end of the group.
    return last + 1 < order.size() ? order[last + 1] : 0;
}

int TaskStore::ResolvePinDrop(const std::vector<int>& order, int id, int beforeId, bool& wholeGroup) const {
    wholeGroup = false;
    const Task* task = GroupedTask(id);
    if (!task || task->group_uid.empty()) return SnapOutOfGroup(order, beforeId);

    const Task* target = beforeId ? GroupedTask(beforeId) : nullptr;
    if (target && target->group_uid == task->group_uid) return beforeId;

    // The slot right below the group, as the PIN window shows it, is still the group's own end.
    size_t after = 0;
    for (size_t i = 0; i < order.size(); ++i) {
        const Task* other = GroupedTask(order[i]);
        if (other && other->group_uid == task->group_uid) after = i + 1;
    }
    const int directlyAfter = after < order.size() ? order[after] : 0;
    int shownAfter = 0;
    for (size_t i = after; i < order.size(); ++i) {
        const Task* other = GroupedTask(order[i]);
        if (other && !other->pin_hidden) {
            shownAfter = order[i];
            break;
        }
    }
    if (beforeId == shownAfter) return directlyAfter;

    wholeGroup = true;
    return SnapOutOfGroup(order, beforeId);
}
