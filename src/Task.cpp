#include "Task.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <set>

TaskStore::TaskStore() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0';
        filePath = std::wstring(exePath) + L"tasks.json";
    } else {
        filePath = L"tasks.json";
    }
    syncFilePath = TaskUtils::GetDefaultSyncFilePath();
    Load();
    CheckWorkReset();
}

void TaskStore::CheckWorkReset() {
    time_t now = time(nullptr);
    time_t cutoff = TaskUtils::GetLast915Cutoff(now);
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

void TaskStore::Add(const std::wstring& text, bool isSync) {
    if (text.empty()) return;
    Task t;
    t.text = text;
    t.completed = false;
    t.created_at = time(nullptr);
    t.completed_at = 0;
    t.is_sync = isSync;

    if (isSync) {
        int maxSyncId = 1000000;
        for (const auto& existing : tasks) {
            if (existing.is_sync && existing.id > maxSyncId) {
                maxSyncId = existing.id;
            }
        }
        if (next_sync_id <= maxSyncId) {
            next_sync_id = maxSyncId + 1;
        }
        t.id = next_sync_id++;
        t.author = TaskUtils::GetCleanUserName();
        tasks.push_back(t);
        active_order.insert(active_order.begin(), t.id);
        SaveSync();
        SaveLocal();
    } else {
        t.id = next_id++;
        t.author = L"";
        tasks.push_back(t);
        active_order.insert(active_order.begin(), t.id);
        SaveLocal();
    }
}

void TaskStore::UpdateText(int id, const std::wstring& newText) {
    for (auto& t : tasks) {
        if (t.id == id) {
            t.text = newText;
            if (t.is_sync) SaveSync();
            else SaveLocal();
            return;
        }
    }
}

void TaskStore::ToggleCompleted(int id) {
    for (auto& t : tasks) {
        if (t.id == id) {
            t.completed = !t.completed;
            if (t.completed) {
                t.completed_at = time(nullptr);
                auto it = std::find(active_order.begin(), active_order.end(), id);
                if (it != active_order.end()) active_order.erase(it);
            } else {
                t.completed_at = 0;
                if (std::find(active_order.begin(), active_order.end(), id) == active_order.end()) {
                    active_order.push_back(id);
                }
            }
            if (t.is_sync) SaveSync();
            SaveLocal();
            return;
        }
    }
}

void TaskStore::ToggleAssignment(int id, const std::wstring& userName) {
    for (auto& t : tasks) {
        if (t.id == id && t.is_sync) {
            t.assignee = (t.assignee == userName) ? L"" : userName;
            SaveSync();
            return;
        }
    }
}

void TaskStore::Delete(int id) {
    bool isSync = false;
    auto it = std::remove_if(tasks.begin(), tasks.end(), [id, &isSync](const Task& t) {
        if (t.id == id) {
            isSync = t.is_sync;
            return true;
        }
        return false;
    });
    if (it != tasks.end()) {
        tasks.erase(it, tasks.end());
        auto itOrder = std::find(active_order.begin(), active_order.end(), id);
        if (itOrder != active_order.end()) active_order.erase(itOrder);
        if (isSync) SaveSync();
        SaveLocal();
    }
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

void TaskStore::SetSyncFilePath(const std::wstring& newPath) {
    if (newPath.empty() || newPath == syncFilePath) return;
    syncFilePath = newPath;
    SaveLocal();
    LoadSync();
}

void TaskStore::SaveLocal() {
    std::ofstream out(filePath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out.is_open()) return;

    out << "{\n  \"next_id\": " << next_id << ",\n"
        << "  \"work_seconds\": " << work_seconds_today << ",\n"
        << "  \"last_reset\": " << (unsigned long long)last_reset_time << ",\n";
    WorkHistory::WriteJson(out, work_history, manual_work_history);
    out << "  \"sync_file_path\": \"" << TaskUtils::EscapeJsonString(TaskUtils::WideToUtf8(syncFilePath)) << "\",\n"
        << "  \"active_order\": [";
    for (size_t i = 0; i < active_order.size(); ++i) {
        if (i > 0) out << ", ";
        out << active_order[i];
    }
    out << "],\n"
        << "  \"tasks\": [\n";
    bool first = true;
    for (const auto& t : tasks) {
        if (t.is_sync) continue;
        if (!first) out << ",\n";
        first = false;
        out << "    {\n";
        out << "      \"id\": " << t.id << ",\n";
        out << "      \"text\": \"" << TaskUtils::EscapeJsonString(TaskUtils::WideToUtf8(t.text)) << "\",\n";
        out << "      \"completed\": " << (t.completed ? "true" : "false") << ",\n";
        out << "      \"created_at\": " << t.created_at << ",\n";
        out << "      \"completed_at\": " << t.completed_at << "\n";
        out << "    }";
    }
    out << "\n  ]\n}\n";
}

void TaskStore::SaveSync() {
    if (syncFilePath.empty()) return;
    TaskUtils::EnsureParentDirectoryExists(syncFilePath);
    std::ofstream out(syncFilePath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out.is_open()) return;

    out << "{\n  \"tasks\": [\n";
    bool first = true;
    for (const auto& t : tasks) {
        if (!t.is_sync) continue;
        if (!first) out << ",\n";
        first = false;
        out << "    {\n";
        out << "      \"id\": " << t.id << ",\n";
        out << "      \"text\": \"" << TaskUtils::EscapeJsonString(TaskUtils::WideToUtf8(t.text)) << "\",\n";
        out << "      \"completed\": " << (t.completed ? "true" : "false") << ",\n";
        out << "      \"created_at\": " << t.created_at << ",\n";
        out << "      \"completed_at\": " << t.completed_at << ",\n";
        out << "      \"author\": \"" << TaskUtils::EscapeJsonString(TaskUtils::WideToUtf8(t.author)) << "\",\n";
        out << "      \"assignee\": \"" << TaskUtils::EscapeJsonString(TaskUtils::WideToUtf8(t.assignee)) << "\"\n";
        out << "    }";
    }
    out << "\n  ]\n}\n";
    out.close();

    HANDLE hFile = CreateFileW(syncFilePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        GetFileTime(hFile, nullptr, nullptr, &lastSyncFileTime);
        CloseHandle(hFile);
    }
}

void TaskStore::Save() {
    SaveLocal();
}

bool TaskStore::CheckSyncFileChanged() {
    if (syncFilePath.empty()) return false;
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExW(syncFilePath.c_str(), GetFileExInfoStandard, &fad)) {
        if (fad.ftLastWriteTime.dwLowDateTime != lastSyncFileTime.dwLowDateTime ||
            fad.ftLastWriteTime.dwHighDateTime != lastSyncFileTime.dwHighDateTime) {
            return true;
        }
    }
    return false;
}

bool TaskStore::LoadSync() {
    if (syncFilePath.empty()) return false;

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExW(syncFilePath.c_str(), GetFileExInfoStandard, &fad)) {
        lastSyncFileTime = fad.ftLastWriteTime;
    }

    tasks.erase(std::remove_if(tasks.begin(), tasks.end(), [](const Task& t) {
        return t.is_sync;
    }), tasks.end());

    std::ifstream in(syncFilePath, std::ios::in | std::ios::binary);
    if (!in.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    WorkHistory::LoadFromJson(content, work_history, manual_work_history);

    size_t pos = 0;
    int maxSyncId = 1000000;
    while ((pos = content.find('{', pos)) != std::string::npos) {
        size_t objEnd = content.find('}', pos);
        if (objEnd == std::string::npos) break;

        std::string objStr = content.substr(pos, objEnd - pos + 1);
        pos = objEnd + 1;

        if (objStr.find("\"id\":") != std::string::npos && objStr.find("\"text\":") != std::string::npos) {
            Task t;
            t.is_sync = true;
            size_t p = objStr.find("\"id\":");
            if (p != std::string::npos) {
                size_t numStart = objStr.find_first_of("-0123456789", p + 5);
                if (numStart != std::string::npos) {
                    t.id = std::stoi(objStr.substr(numStart));
                }
            }
            if (t.id < 1000000) {
                t.id += 1000000;
            }
            p = objStr.find("\"text\":");
            if (p != std::string::npos) {
                size_t strStart = objStr.find('"', p + 7);
                if (strStart != std::string::npos) {
                    size_t strEnd = strStart + 1;
                    while (strEnd < objStr.size()) {
                        if (objStr[strEnd] == '"' && objStr[strEnd - 1] != '\\') break;
                        strEnd++;
                    }
                    if (strEnd < objStr.size()) {
                        std::string raw = objStr.substr(strStart + 1, strEnd - strStart - 1);
                        t.text = TaskUtils::Utf8ToWide(TaskUtils::UnescapeJsonString(raw));
                    }
                }
            }
            p = objStr.find("\"completed\":");
            if (p != std::string::npos) {
                size_t bStart = objStr.find_first_not_of(" \t\r\n", p + 12);
                if (bStart != std::string::npos) {
                    t.completed = (objStr.substr(bStart, 4) == "true");
                }
            }
            p = objStr.find("\"created_at\":");
            if (p != std::string::npos) {
                size_t numStart = objStr.find_first_of("0123456789", p + 13);
                if (numStart != std::string::npos) {
                    t.created_at = (time_t)std::stoull(objStr.substr(numStart));
                }
            }
            p = objStr.find("\"completed_at\":");
            if (p != std::string::npos) {
                size_t numStart = objStr.find_first_of("0123456789", p + 15);
                if (numStart != std::string::npos) {
                    t.completed_at = (time_t)std::stoull(objStr.substr(numStart));
                }
            }
            p = objStr.find("\"author\":");
            if (p != std::string::npos) {
                size_t strStart = objStr.find('"', p + 9);
                if (strStart != std::string::npos) {
                    size_t strEnd = strStart + 1;
                    while (strEnd < objStr.size()) {
                        if (objStr[strEnd] == '"' && objStr[strEnd - 1] != '\\') break;
                        strEnd++;
                    }
                    if (strEnd < objStr.size()) {
                        std::string raw = objStr.substr(strStart + 1, strEnd - strStart - 1);
                        t.author = TaskUtils::Utf8ToWide(TaskUtils::UnescapeJsonString(raw));
                    }
                }
            }
            p = objStr.find("\"assignee\":");
            if (p != std::string::npos) {
                size_t strStart = objStr.find('"', p + 11);
                if (strStart != std::string::npos) {
                    size_t strEnd = strStart + 1;
                    while (strEnd < objStr.size()) {
                        if (objStr[strEnd] == '"' && objStr[strEnd - 1] != '\\') break;
                        strEnd++;
                    }
                    if (strEnd < objStr.size()) {
                        std::string raw = objStr.substr(strStart + 1, strEnd - strStart - 1);
                        t.assignee = TaskUtils::Utf8ToWide(TaskUtils::UnescapeJsonString(raw));
                    }
                }
            }

            if (t.id > maxSyncId) maxSyncId = t.id;
            tasks.push_back(t);
        }
    }
    next_sync_id = maxSyncId + 1;
    return true;
}

void TaskStore::LoadLocal() {
    tasks.erase(std::remove_if(tasks.begin(), tasks.end(), [](const Task& t) {
        return !t.is_sync;
    }), tasks.end());

    std::ifstream in(filePath, std::ios::in | std::ios::binary);
    if (!in.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    WorkHistory::LoadFromJson(content, work_history, manual_work_history);

    size_t nextIdPos = content.find("\"next_id\":");
    if (nextIdPos != std::string::npos) {
        size_t valStart = content.find_first_of("0123456789", nextIdPos);
        if (valStart != std::string::npos) {
            next_id = std::stoi(content.substr(valStart));
        }
    }

    size_t wsPos = content.find("\"work_seconds\":");
    if (wsPos != std::string::npos) {
        size_t valStart = content.find_first_of("-0123456789", wsPos + 14);
        if (valStart != std::string::npos) {
            int sec = std::stoi(content.substr(valStart));
            work_seconds_today = (sec > 0) ? sec : 0;
        }
    }

    size_t lrPos = content.find("\"last_reset\":");
    if (lrPos != std::string::npos) {
        size_t valStart = content.find_first_of("0123456789", lrPos + 12);
        if (valStart != std::string::npos) {
            last_reset_time = (time_t)std::stoull(content.substr(valStart));
        }
    }

    size_t sfpPos = content.find("\"sync_file_path\":");
    if (sfpPos != std::string::npos) {
        size_t strStart = content.find('"', sfpPos + 17);
        if (strStart != std::string::npos) {
            size_t strEnd = strStart + 1;
            while (strEnd < content.size()) {
                if (content[strEnd] == '"' && content[strEnd - 1] != '\\') break;
                strEnd++;
            }
            if (strEnd < content.size()) {
                std::string raw = content.substr(strStart + 1, strEnd - strStart - 1);
                std::wstring parsed = TaskUtils::Utf8ToWide(TaskUtils::UnescapeJsonString(raw));
                if (!parsed.empty()) {
                    syncFilePath = parsed;
                }
            }
        }
    }

    active_order.clear();
    size_t orderPos = content.find("\"active_order\":");
    if (orderPos != std::string::npos) {
        size_t arrStart = content.find('[', orderPos);
        size_t arrEnd = (arrStart != std::string::npos) ? content.find(']', arrStart) : std::string::npos;
        if (arrStart != std::string::npos && arrEnd != std::string::npos) {
            std::string arrStr = content.substr(arrStart + 1, arrEnd - arrStart - 1);
            size_t p = 0;
            while ((p = arrStr.find_first_of("0123456789", p)) != std::string::npos) {
                size_t numEnd = arrStr.find_first_not_of("0123456789", p);
                if (numEnd == std::string::npos) numEnd = arrStr.size();
                active_order.push_back(std::stoi(arrStr.substr(p, numEnd - p)));
                p = numEnd;
            }
        }
    }

    size_t pos = 0;
    int maxId = 0;
    while ((pos = content.find('{', pos)) != std::string::npos) {
        size_t objEnd = content.find('}', pos);
        if (objEnd == std::string::npos) break;

        std::string objStr = content.substr(pos, objEnd - pos + 1);
        pos = objEnd + 1;

        if (objStr.find("\"id\":") != std::string::npos && objStr.find("\"text\":") != std::string::npos) {
            Task t;
            t.is_sync = false;
            size_t p = objStr.find("\"id\":");
            if (p != std::string::npos) {
                size_t numStart = objStr.find_first_of("-0123456789", p + 5);
                if (numStart != std::string::npos) {
                    t.id = std::stoi(objStr.substr(numStart));
                }
            }
            p = objStr.find("\"text\":");
            if (p != std::string::npos) {
                size_t strStart = objStr.find('"', p + 7);
                if (strStart != std::string::npos) {
                    size_t strEnd = strStart + 1;
                    while (strEnd < objStr.size()) {
                        if (objStr[strEnd] == '"' && objStr[strEnd - 1] != '\\') break;
                        strEnd++;
                    }
                    if (strEnd < objStr.size()) {
                        std::string raw = objStr.substr(strStart + 1, strEnd - strStart - 1);
                        t.text = TaskUtils::Utf8ToWide(TaskUtils::UnescapeJsonString(raw));
                    }
                }
            }
            p = objStr.find("\"completed\":");
            if (p != std::string::npos) {
                size_t bStart = objStr.find_first_not_of(" \t\r\n", p + 12);
                if (bStart != std::string::npos) {
                    t.completed = (objStr.substr(bStart, 4) == "true");
                }
            }
            p = objStr.find("\"created_at\":");
            if (p != std::string::npos) {
                size_t numStart = objStr.find_first_of("0123456789", p + 13);
                if (numStart != std::string::npos) {
                    t.created_at = (time_t)std::stoull(objStr.substr(numStart));
                }
            }
            p = objStr.find("\"completed_at\":");
            if (p != std::string::npos) {
                size_t numStart = objStr.find_first_of("0123456789", p + 15);
                if (numStart != std::string::npos) {
                    t.completed_at = (time_t)std::stoull(objStr.substr(numStart));
                }
            }

            if (t.id > maxId) maxId = t.id;
            tasks.push_back(t);
        }
    }

    if (next_id <= maxId) {
        next_id = maxId + 1;
    }
}

void TaskStore::Load() {
    tasks.clear();
    LoadLocal();
    LoadSync();

    std::vector<int> reconciled;
    std::set<int> seen;
    for (int id : active_order) {
        for (const auto& t : tasks) {
            if (t.id == id && !t.completed) {
                reconciled.push_back(id);
                seen.insert(id);
                break;
            }
        }
    }
    for (const auto& t : tasks) {
        if (!t.completed && seen.find(t.id) == seen.end()) {
            reconciled.push_back(t.id);
            seen.insert(t.id);
        }
    }
    active_order = reconciled;
    SaveLocal();
}
