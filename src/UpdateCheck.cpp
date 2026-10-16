#include "UpdateCheck.h"
#include "AppState.h"
#include "JsonLite.h"
#include "MemoryTrim.h"
#include "UpdateOverlay.h"
#include "Version.h"
#include <winhttp.h>
#include <string>

namespace {
constexpr time_t CHECK_INTERVAL_SECONDS = 12 * 60 * 60;
constexpr time_t RETRY_DELAY_SECONDS = 5 * 60;
constexpr int MAX_RETRIES = 3;
constexpr DWORD TIMEOUT_MS = 5000;
constexpr size_t MAX_RESPONSE_BYTES = 256 * 1024;

#define TASK_MANAGER_WIDEN_INNER(value) L##value
#define TASK_MANAGER_WIDEN(value) TASK_MANAGER_WIDEN_INNER(value)
constexpr wchar_t USER_AGENT[] = L"TaskManager/" TASK_MANAGER_WIDEN(TASK_MANAGER_VERSION_STRING);

bool g_inFlight = false;

// "v1.18.0" -> "1.18.0"; empty when the tag is not a dotted version.
std::string NormalizeTag(std::string tag) {
    if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) tag.erase(0, 1);
    if (tag.empty() || tag.find_first_not_of("0123456789.") != std::string::npos) return "";
    return tag;
}

bool FetchLatestTag(std::string& version) {
    HINTERNET session = WinHttpOpen(USER_AGENT, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        // Automatic proxy needs Windows 8.1; fall back to the system default.
        session = WinHttpOpen(USER_AGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (!session) return false;
    WinHttpSetTimeouts(session, TIMEOUT_MS, TIMEOUT_MS, TIMEOUT_MS, TIMEOUT_MS);

    bool ok = false;
    std::string body;
    HINTERNET connection = WinHttpConnect(session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET",
        L"/repos/cncDAni2/TaskManager/releases/latest", nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (request &&
        WinHttpSendRequest(request, L"Accept: application/vnd.github+json\r\n", (DWORD)-1L,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr)) {
        DWORD status = 0;
        DWORD size = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        if (status == 200) {
            char buffer[4096];
            DWORD read = 0;
            while (WinHttpReadData(request, buffer, sizeof(buffer), &read) && read > 0 &&
                body.size() < MAX_RESPONSE_BYTES) {
                body.append(buffer, read);
            }
            std::string tag;
            if (JsonLite::GetString(body, "tag_name", tag)) {
                version = NormalizeTag(tag);
                ok = !version.empty();
            }
        }
    }
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return ok;
}

void CALLBACK CheckCallback(PTP_CALLBACK_INSTANCE, void* context) {
    HWND owner = static_cast<HWND>(context);
    std::string version;
    const bool ok = FetchLatestTag(version);
    auto* result = ok ? new std::string(version) : nullptr;
    if (!PostMessageW(owner, WM_APP_UPDATE_RESULT, ok ? 1 : 0, reinterpret_cast<LPARAM>(result))) {
        delete result;
    }
}
}

void UpdateCheck::Request() {
    if (!g_store.update_check_enabled || g_inFlight || !g_hWnd) return;
    const time_t now = time(nullptr);
    const TaskStore& store = g_store;
    if (store.update_last_check > 0 && store.update_last_check <= now &&
        now - store.update_last_check < CHECK_INTERVAL_SECONDS) return;
    if (store.update_failures > 0 && store.update_last_failure <= now &&
        now - store.update_last_failure < RETRY_DELAY_SECONDS) return;

    g_inFlight = TrySubmitThreadpoolCallback(CheckCallback, g_hWnd, nullptr) != FALSE;
}

void UpdateCheck::HandleResult(WPARAM wParam, LPARAM lParam) {
    g_inFlight = false;
    std::string* version = reinterpret_cast<std::string*>(lParam);
    const time_t now = time(nullptr);
    if (wParam && version) {
        g_store.update_last_check = now;
        g_store.update_failures = 0;
        g_store.update_latest_version = *version;
    } else if (++g_store.update_failures > MAX_RETRIES) {
        // Out of retries: wait for the next regular check instead of nagging the network.
        g_store.update_last_check = now;
        g_store.update_failures = 0;
    } else {
        g_store.update_last_failure = now;
    }
    delete version;
    g_store.SaveLocal();
    UpdateOverlay::Refresh();
    // WinHTTP and TLS leave ~2 MB of touched pages behind; give them back unless the full window
    // is open (hiding it trims anyway).
    if (g_hWnd && (g_isMiniMode || !IsWindowVisible(g_hWnd))) MemoryTrim::Request(g_hWnd);
}
