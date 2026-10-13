#include "Task.h"
#include "JsonLite.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

namespace {
std::string Quote(const std::wstring& text) {
    return "\"" + TaskUtils::EscapeJsonString(TaskUtils::WideToUtf8(text)) + "\"";
}

// The company OneDrive folder, or empty when this user does not have one.
std::wstring CompanyOneDriveFolder() {
    wchar_t userProfile[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) == 0) return L"";
    std::wstring folder = std::wstring(userProfile) + L"\\OneDrive - Siemens AG";
    const DWORD attributes = GetFileAttributesW(folder.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) return L"";
    return folder;
}
}

// First start (no local file yet): preconfigure the shared company file only if OneDrive exists.
void TaskStore::AddFirstRunSyncSource() {
    const std::wstring oneDrive = CompanyOneDriveFolder();
    if (oneDrive.empty()) return;
    SyncSource source;
    source.id = next_source_id++;
    source.path = oneDrive + L"\\TaskManager\\tasks.json";
    source.name = SyncFiles::DefaultName(source.path);
    syncSources.push_back(source);
}

void TaskStore::SaveLocal() {
    std::ofstream out(filePath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out.is_open()) return;

    out << "{\n  \"work_seconds\": " << work_seconds_today << ",\n"
        << "  \"last_reset\": " << (unsigned long long)last_reset_time << ",\n";
    WorkHistory::WriteJson(out, work_history, manual_work_history);
    const char* themeName = theme_mode == ThemeMode::Pink ? "pink"
        : (theme_mode == ThemeMode::Light ? "light" : "dark");
    out << "  \"dark_mode\": " << (theme_mode == ThemeMode::Dark ? "true" : "false") << ",\n"
        << "  \"theme_mode\": \"" << themeName << "\",\n"
        << "  \"work_reset_hour\": " << work_reset_hour << ",\n"
        << "  \"work_reset_minute\": " << work_reset_minute << ",\n"
        << "  \"timer_work_minutes\": " << timer_work_minutes << ",\n"
        << "  \"timer_rest_minutes\": " << timer_rest_minutes << ",\n"
        << "  \"timer_repetitions\": " << timer_repetitions << ",\n"
        << "  \"timer_2020_enabled\": " << (timer_2020_enabled ? "true" : "false") << ",\n"
        << "  \"timer_strict_mode\": " << (timer_strict_mode ? "true" : "false") << ",\n"
        << "  \"timer_start_with_app\": " << (timer_start_with_app ? "true" : "false") << ",\n"
        << "  \"sounds_enabled\": " << (sounds_enabled ? "true" : "false") << ",\n"
        << "  \"excluded_window_keywords\": " << Quote(excluded_window_keywords) << ",\n"
        << "  \"next_source_id\": " << next_source_id << ",\n"
        << "  \"sync_files\": [";
    for (size_t i = 0; i < syncSources.size(); ++i) {
        const SyncSource& source = syncSources[i];
        out << (i > 0 ? "," : "") << "\n    { \"source_id\": " << source.id
            << ", \"name\": " << Quote(source.name)
            << ", \"path\": " << Quote(source.path)
            << ", \"default\": " << (source.isDefault ? "true" : "false") << " }";
    }
    out << (syncSources.empty() ? "],\n" : "\n  ],\n") << "  \"active_order\": [";
    bool firstOrder = true;
    for (int id : active_order) {
        auto it = std::find_if(tasks.begin(), tasks.end(), [id](const Task& t) { return t.id == id; });
        if (it == tasks.end()) continue;
        out << (firstOrder ? "" : ", ") << "\"" << TaskUtils::EscapeJsonString(it->uid) << "\"";
        firstOrder = false;
    }
    out << "],\n"
        << "  \"tasks\": [\n";
    bool first = true;
    for (const auto& t : tasks) {
        if (t.is_sync) continue;
        if (!first) out << ",\n";
        first = false;
        out << "    {\n";
        out << "      \"uid\": \"" << TaskUtils::EscapeJsonString(t.uid) << "\",\n";
        out << "      \"text\": " << Quote(t.text) << ",\n";
        out << "      \"completed\": " << (t.completed ? "true" : "false") << ",\n";
        out << "      \"created_at\": " << t.created_at << ",\n";
        out << "      \"completed_at\": " << t.completed_at << ",\n";
        out << "      \"marker\": " << static_cast<int>(t.marker) << "\n";
        out << "    }";
    }
    out << "\n  ]\n}\n";
}

void TaskStore::LoadLocal() {
    tasks.erase(std::remove_if(tasks.begin(), tasks.end(), [](const Task& t) {
        return !t.is_sync;
    }), tasks.end());

    std::ifstream in(filePath, std::ios::in | std::ios::binary);
    if (!in.is_open()) {
        AddFirstRunSyncSource();
        return;
    }

    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    WorkHistory::LoadFromJson(content, work_history, manual_work_history);

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

    bool themeLoaded = false;
    size_t themeModePos = content.find("\"theme_mode\":");
    if (themeModePos != std::string::npos) {
        size_t valueStart = content.find_first_not_of(" \t\r\n", themeModePos + 13);
        if (valueStart != std::string::npos && content[valueStart] == '"') {
            size_t valueEnd = content.find('"', valueStart + 1);
            if (valueEnd != std::string::npos) {
                std::string themeName = content.substr(valueStart + 1, valueEnd - valueStart - 1);
                if (themeName == "pink") {
                    theme_mode = ThemeMode::Pink;
                    themeLoaded = true;
                } else if (themeName == "light") {
                    theme_mode = ThemeMode::Light;
                    themeLoaded = true;
                } else if (themeName == "dark") {
                    theme_mode = ThemeMode::Dark;
                    themeLoaded = true;
                }
            }
        }
    }
    if (!themeLoaded) {
        size_t darkModePos = content.find("\"dark_mode\":");
        if (darkModePos != std::string::npos) {
            size_t valueStart = content.find_first_not_of(" \t\r\n", darkModePos + 12);
            if (valueStart != std::string::npos) {
                theme_mode = content.compare(valueStart, 4, "true") == 0
                    ? ThemeMode::Dark : ThemeMode::Light;
            }
        }
    }

    size_t resetHourPos = content.find("\"work_reset_hour\":");
    if (resetHourPos != std::string::npos) {
        size_t valueStart = content.find_first_of("0123456789", resetHourPos + 18);
        if (valueStart != std::string::npos) {
            work_reset_hour = (std::min)(23, std::stoi(content.substr(valueStart)));
        }
    }

    size_t resetMinutePos = content.find("\"work_reset_minute\":");
    if (resetMinutePos != std::string::npos) {
        size_t valueStart = content.find_first_of("0123456789", resetMinutePos + 20);
        if (valueStart != std::string::npos) {
            work_reset_minute = (std::min)(59, std::stoi(content.substr(valueStart)));
        }
    }

    const auto loadTimerMinutes = [&content](const char* key, double& value) {
        size_t keyPos = content.find(key);
        if (keyPos == std::string::npos) return;
        size_t valueStart = content.find_first_of("0123456789", keyPos + strlen(key));
        if (valueStart == std::string::npos) return;
        try {
            const double loaded = std::stod(content.substr(valueStart));
            const double clamped = (std::max)(0.1, (std::min)(999.9, loaded));
            value = static_cast<double>(static_cast<int>(clamped * 10.0 + 0.5)) / 10.0;
        } catch (...) {
        }
    };
    loadTimerMinutes("\"timer_work_minutes\":", timer_work_minutes);
    loadTimerMinutes("\"timer_rest_minutes\":", timer_rest_minutes);

    const auto loadTimerRepetitions = [&content](int& value) {
        const char* key = "\"timer_repetitions\":";
        size_t keyPos = content.find(key);
        if (keyPos == std::string::npos) return;
        size_t valueStart = content.find_first_of("0123456789", keyPos + strlen(key));
        if (valueStart == std::string::npos) return;
        value = (std::max)(1, (std::min)(99, std::stoi(content.substr(valueStart))));
    };
    loadTimerRepetitions(timer_repetitions);

    size_t timer2020Pos = content.find("\"timer_2020_enabled\":");
    if (timer2020Pos != std::string::npos) {
        size_t valueStart = content.find_first_not_of(" \t\r\n", timer2020Pos + 21);
        if (valueStart != std::string::npos) {
            timer_2020_enabled = content.compare(valueStart, 4, "true") == 0;
        }
    }

    size_t timerStrictPos = content.find("\"timer_strict_mode\":");
    if (timerStrictPos != std::string::npos) {
        const char* timerStrictKey = "\"timer_strict_mode\":";
        size_t valueStart = content.find_first_not_of(" \t\r\n", timerStrictPos + strlen(timerStrictKey));
        if (valueStart != std::string::npos) {
            timer_strict_mode = content.compare(valueStart, 4, "true") == 0;
        }
    }

    const char* timerStartKey = "\"timer_start_with_app\":";
    size_t timerStartPos = content.find(timerStartKey);
    if (timerStartPos != std::string::npos) {
        size_t valueStart = content.find_first_not_of(" \t\r\n", timerStartPos + strlen(timerStartKey));
        if (valueStart != std::string::npos) {
            timer_start_with_app = content.compare(valueStart, 4, "true") == 0;
        }
    }

    size_t soundsEnabledPos = content.find("\"sounds_enabled\":");
    if (soundsEnabledPos != std::string::npos) {
        size_t valueStart = content.find_first_not_of(" \t\r\n", soundsEnabledPos + 17);
        if (valueStart != std::string::npos) {
            sounds_enabled = content.compare(valueStart, 4, "true") == 0;
        }
    }

    std::wstring parsedKeywords;
    if (JsonLite::GetWideString(content, "excluded_window_keywords", parsedKeywords)) {
        excluded_window_keywords = ExcludedApps::Normalize(parsedKeywords);
    }

    syncSources.clear();
    long long sourceCounter = 0;
    if (JsonLite::GetInt64(content, "next_source_id", sourceCounter) && sourceCounter > 0) {
        next_source_id = static_cast<int>(sourceCounter);
    }
    std::vector<std::string> sourceObjects;
    if (JsonLite::ReadObjectArray(content, "sync_files", sourceObjects)) {
        for (const std::string& object : sourceObjects) {
            SyncSource source;
            long long sourceId = 0;
            if (!JsonLite::GetInt64(object, "source_id", sourceId) || sourceId <= 0) continue;
            if (!JsonLite::GetWideString(object, "path", source.path) || source.path.empty()) continue;
            source.id = static_cast<int>(sourceId);
            JsonLite::GetWideString(object, "name", source.name);
            JsonLite::GetBool(object, "default", source.isDefault);
            if (source.name.empty()) source.name = SyncFiles::DefaultName(source.path);
            next_source_id = (std::max)(next_source_id, source.id + 1);
            syncSources.push_back(source);
        }
    } else {
        // Migrate the single-file setting of earlier versions.
        std::wstring legacyPath;
        if (JsonLite::GetWideString(content, "sync_file_path", legacyPath) && !legacyPath.empty()) {
            SyncSource source;
            source.id = next_source_id++;
            source.path = legacyPath;
            source.name = SyncFiles::DefaultName(legacyPath);
            syncSources.push_back(source);
        }
    }

    std::vector<std::string> taskObjects;
    JsonLite::ReadObjectArray(content, "tasks", taskObjects);
    for (const std::string& object : taskObjects) {
        Task t;
        t.is_sync = false;
        if (!JsonLite::GetString(object, "uid", t.uid) || t.uid.empty()) {
            long long legacyId = 0;
            if (!JsonLite::GetInt64(object, "id", legacyId)) continue;
            t.uid = std::to_string(legacyId);
        }
        if (!JsonLite::GetWideString(object, "text", t.text)) continue;
        long long number = 0;
        JsonLite::GetBool(object, "completed", t.completed);
        if (JsonLite::GetInt64(object, "created_at", number)) t.created_at = static_cast<time_t>(number);
        if (JsonLite::GetInt64(object, "completed_at", number)) t.completed_at = static_cast<time_t>(number);
        if (JsonLite::GetInt64(object, "marker", number)) {
            t.marker = static_cast<TaskMarker>((std::min)(3LL, (std::max)(0LL, number)));
        }
        t.id = RuntimeIdFor(t.uid);
        tasks.push_back(t);
    }

    // Entries may be uids (strings) or the numeric ids of earlier versions.
    active_order.clear();
    std::vector<std::string> orderUids;
    JsonLite::ReadScalarArray(content, "active_order", orderUids);
    for (const std::string& uid : orderUids) {
        if (!uid.empty()) active_order.push_back(RuntimeIdFor(uid));
    }
}
