#include "SelfUpdate.h"
#include "AppState.h"
#include "InlineEdit.h"
#include "MemoryTrim.h"
#include "TaskUtils.h"
#include "UpdateOverlay.h"
#include "Version.h"
#include "VersionHint.h"
#include <shellapi.h>
#include <winhttp.h>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// The startup half (waiting for the replaced process) must run before g_store's constructor
// loads tasks-local.json, so this file's statics are initialized in the library segment.
#pragma warning(disable : 4073)
#pragma init_seg(lib)

namespace {
constexpr DWORD CONNECT_TIMEOUT_MS = 10000;
constexpr DWORD TRANSFER_TIMEOUT_MS = 30000;
constexpr size_t MIN_EXE_BYTES = 64 * 1024;
constexpr size_t MAX_EXE_BYTES = 32 * 1024 * 1024;
constexpr DWORD OLD_EXIT_WAIT_MS = 30000;
constexpr wchar_t UPDATED_ARG[] = L"--updated";

#define TASK_MANAGER_WIDEN_INNER(value) L##value
#define TASK_MANAGER_WIDEN(value) TASK_MANAGER_WIDEN_INNER(value)
constexpr wchar_t USER_AGENT[] = L"TaskManager/" TASK_MANAGER_WIDEN(TASK_MANAGER_VERSION_STRING);

bool g_busy = false;

struct Job {
    HWND owner = nullptr;
    std::string version;
    std::wstring newPath;
};

std::wstring ExePath() {
    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    return length > 0 && length < MAX_PATH ? std::wstring(path, length) : std::wstring();
}

bool Download(const std::string& version, std::string& data) {
    HINTERNET session = WinHttpOpen(USER_AGENT, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        session = WinHttpOpen(USER_AGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (!session) return false;
    WinHttpSetTimeouts(session, CONNECT_TIMEOUT_MS, CONNECT_TIMEOUT_MS, TRANSFER_TIMEOUT_MS, TRANSFER_TIMEOUT_MS);

    // The exact release the check reported; GitHub redirects it (https only) to its file host.
    const std::wstring path = L"/cncDAni2/TaskManager/releases/download/v" +
        TaskUtils::Utf8ToWide(version) + L"/TaskManager.exe";
    bool ok = false;
    HINTERNET connection = WinHttpConnect(session, L"github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (request &&
        WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr)) {
        DWORD status = 0;
        DWORD size = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        DWORD expected = 0;
        size = sizeof(expected);
        const bool hasLength = WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &expected, &size, WINHTTP_NO_HEADER_INDEX) != FALSE;
        if (status == 200 && (!hasLength || expected <= MAX_EXE_BYTES)) {
            std::vector<char> buffer(64 * 1024);
            DWORD read = 0;
            bool readOk = true;
            while ((readOk = WinHttpReadData(request, buffer.data(), (DWORD)buffer.size(), &read) != FALSE) &&
                read > 0 && data.size() + read <= MAX_EXE_BYTES) {
                data.append(buffer.data(), read);
            }
            // A dropped connection or an oversized file leaves read > 0 or readOk false.
            ok = readOk && read == 0 && (!hasLength || data.size() == expected);
        }
    }
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return ok;
}

bool IsX64Exe(const std::string& data) {
    if (data.size() < MIN_EXE_BYTES || data[0] != 'M' || data[1] != 'Z') return false;
    DWORD peOffset = 0;
    std::memcpy(&peOffset, data.data() + 0x3C, sizeof(peOffset));
    if (peOffset > data.size() - 6) return false;
    WORD machine = 0;
    std::memcpy(&machine, data.data() + peOffset + 4, sizeof(machine));
    return std::memcmp(data.data() + peOffset, "PE\0\0", 4) == 0 && machine == IMAGE_FILE_MACHINE_AMD64;
}

bool WriteWhole(const std::wstring& path, const std::string& data) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = WriteFile(file, data.data(), (DWORD)data.size(), &written, nullptr) &&
        written == data.size() && FlushFileBuffers(file);
    CloseHandle(file);
    return ok;
}

bool HasVersion(const std::wstring& path, const std::string& version) {
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
    if (size == 0) return false;
    std::vector<BYTE> info(size);
    VS_FIXEDFILEINFO* fixed = nullptr;
    UINT length = 0;
    if (!GetFileVersionInfoW(path.c_str(), 0, size, info.data()) ||
        !VerQueryValueW(info.data(), L"\\", reinterpret_cast<void**>(&fixed), &length) || !fixed) {
        return false;
    }
    const std::string actual = std::to_string(HIWORD(fixed->dwFileVersionMS)) + "." +
        std::to_string(LOWORD(fixed->dwFileVersionMS)) + "." + std::to_string(HIWORD(fixed->dwFileVersionLS));
    return VersionHint::Compare(actual, version) == 0;
}

void CALLBACK DownloadCallback(PTP_CALLBACK_INSTANCE, void* context) {
    std::unique_ptr<Job> job(static_cast<Job*>(context));
    std::string data;
    bool ok = Download(job->version, data) && IsX64Exe(data) && WriteWhole(job->newPath, data);
    std::string().swap(data);
    if (ok) ok = HasVersion(job->newPath, job->version);
    if (!ok) DeleteFileW(job->newPath.c_str());
    PostMessageW(job->owner, WM_APP_SELF_UPDATE_RESULT, ok ? 1 : 0, 0);
}

// Puts ".new" in place of the running exe and starts it; on failure the files are as before.
bool SwapAndStart() {
    const std::wstring exe = ExePath();
    if (exe.empty()) return false;
    const std::wstring oldPath = exe + L".old";
    const std::wstring newPath = exe + L".new";
    DeleteFileW(oldPath.c_str());
    if (!MoveFileExW(exe.c_str(), oldPath.c_str(), MOVEFILE_REPLACE_EXISTING)) return false;
    if (!MoveFileExW(newPath.c_str(), exe.c_str(), 0)) {
        MoveFileExW(oldPath.c_str(), exe.c_str(), 0);
        return false;
    }
    std::wstring commandLine = L"\"" + exe + L"\" " + UPDATED_ARG + L" " + std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(exe.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
            &startup, &process)) {
        MoveFileExW(exe.c_str(), newPath.c_str(), MOVEFILE_REPLACE_EXISTING);
        MoveFileExW(oldPath.c_str(), exe.c_str(), 0);
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

// Runs at startup: after an update, wait until the replaced process has exited (it still holds
// the single-instance mutex and may be writing tasks-local.json), then remove its file.
void FinishPreviousUpdate() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i + 1 < argc; ++i) {
            if (wcscmp(argv[i], UPDATED_ARG) != 0) continue;
            const DWORD pid = wcstoul(argv[i + 1], nullptr, 10);
            HANDLE oldProcess = pid ? OpenProcess(SYNCHRONIZE, FALSE, pid) : nullptr;
            if (oldProcess) {
                WaitForSingleObject(oldProcess, OLD_EXIT_WAIT_MS);
                CloseHandle(oldProcess);
            }
        }
        LocalFree(argv);
    }
    const std::wstring exe = ExePath();
    if (!exe.empty()) DeleteFileW((exe + L".old").c_str()); // Fails harmlessly while it still runs.
}

struct StartupHook {
    StartupHook() { FinishPreviousUpdate(); }
} g_startupHook;
}

bool SelfUpdate::IsBusy() {
    return g_busy;
}

bool SelfUpdate::Start(HWND owner) {
    if (g_busy) return true;
    const std::wstring exe = ExePath();
    if (exe.empty() || g_store.update_latest_version.empty()) return false;
    auto* job = new Job{ owner, g_store.update_latest_version, exe + L".new" };
    if (!TrySubmitThreadpoolCallback(DownloadCallback, job, nullptr)) {
        delete job;
        return false;
    }
    g_busy = true;
    UpdateOverlay::Refresh();
    return true;
}

void SelfUpdate::HandleResult(HWND hWnd, WPARAM wParam) {
    g_busy = false;
    if (wParam) {
        CommitInlineEdit();
        g_store.SaveLocal();
        if (SwapAndStart()) {
            DestroyWindow(hWnd);
            return;
        }
    }
    UpdateOverlay::OpenDownloadPage();
    if (g_isMiniMode || !IsWindowVisible(hWnd)) MemoryTrim::Request(hWnd);
}
