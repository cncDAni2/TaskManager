#include "LauraLink.h"
#include "AppState.h"
#include "IntervalTimer.h"
#include "Layout.h"
#include "LauraProtocol.h"
#include "MainWindow.h"
#include "SettingsDialog.h"
#include "StateToast.h"
#include "TaskUtils.h"
#include "TimerView.h"
#include "UIEventHandler.h"
#include "WorkTracking.h"
#include <cwctype>
#include <deque>

namespace LauraLink {
namespace {
UINT g_message = 0;
HWND g_self = nullptr;
LauraProtocol::Session g_session;
HANDLE g_process = nullptr;
HANDLE g_wait = nullptr;
UINT_PTR g_watchNumber = 0;
std::deque<std::string> g_pending; // Commands copied out of WM_COPYDATA, run from the queue.
bool g_spokeDuringCommand = false;
bool g_runningCommand = false;

void CALLBACK OnProcessExit(void* context, BOOLEAN) {
    PostMessageW(g_self, WM_APP_LAURA_EXITED, reinterpret_cast<WPARAM>(context), 0);
}

void Unwatch() {
    if (g_wait) UnregisterWaitEx(g_wait, INVALID_HANDLE_VALUE); // Waits for a running callback.
    if (g_process) CloseHandle(g_process);
    g_wait = nullptr;
    g_process = nullptr;
    ++g_watchNumber; // A posted exit of the old process is ignored.
}

void Watch(HWND peer) {
    DWORD pid = 0;
    GetWindowThreadProcessId(peer, &pid);
    g_process = pid ? OpenProcess(SYNCHRONIZE, FALSE, pid) : nullptr;
    if (!g_process) return;
    if (!RegisterWaitForSingleObject(&g_wait, g_process, OnProcessExit,
            reinterpret_cast<void*>(++g_watchNumber), INFINITE, WT_EXECUTEONLYONCE)) {
        g_wait = nullptr;
    }
}

void Signal(HWND to, LauraProtocol::Signal signal) {
    if (to) PostMessageW(to, g_message, reinterpret_cast<WPARAM>(g_self), signal);
}

void Apply(const LauraProtocol::Actions& actions);

bool Send(const std::string& json) {
    const HWND peer = g_session.Peer();
    if (!peer) return false;
    COPYDATASTRUCT data{ LauraProtocol::COPYDATA_TAG, static_cast<DWORD>(json.size()),
        const_cast<char*>(json.data()) };
    DWORD_PTR answer = 0;
    const bool sent = SendMessageTimeoutW(peer, WM_COPYDATA, reinterpret_cast<WPARAM>(g_self),
        reinterpret_cast<LPARAM>(&data), SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 1000, &answer) != 0;
    if (!sent && !IsWindow(peer)) Apply(g_session.OnPeerExited());
    return sent && answer != 0;
}

void Apply(const LauraProtocol::Actions& actions) {
    if (actions.unwatch) Unwatch();
    if (actions.watch && g_session.Peer()) Watch(g_session.Peer());
    if (actions.sendAck) Signal(g_session.Peer(), LauraProtocol::HelloAck);
    if (actions.sendHello) Signal(g_session.Peer(), LauraProtocol::Hello);
    if (actions.sendBye) Signal(g_session.Peer(), LauraProtocol::Bye);
    if (actions.sendCatalog) Send(LauraProtocol::Catalog());
    if (actions.lost) {
        StateToast::ShowLaura(false);
        if (g_store.sounds_enabled) MessageBeep(MB_ICONWARNING);
    }
    if (actions.found) StateToast::ShowLaura(true);
    if (actions.changed) RefreshSettingsLaura();
}

void Repaint() {
    RecalculateLayout();
    if (g_hWnd) InvalidateRect(g_hWnd, nullptr, FALSE);
}

void PressTimer(int buttonId) {
    TimerView::HandleCommand(g_hWnd, MAKEWPARAM(buttonId, BN_CLICKED));
}

std::wstring Trim(std::wstring text) {
    while (!text.empty() && (std::iswspace(text.front()) || text.front() == L':')) text.erase(text.begin());
    while (!text.empty() && (std::iswspace(text.back()) || text.back() == L'.')) text.pop_back();
    return text;
}

// Runs one command; the reply is the sentence Laura reads aloud (empty: a cue already spoke).
std::wstring Run(const LauraProtocol::Request& request, bool& ok) {
    using LauraProtocol::Command;
    ok = true;
    switch (request.command) {
    case Command::MeasureOn: {
        const bool wasOn = WorkTracking::IsEnabled() && !g_workMeasurementStopped;
        if (!WorkTracking::IsEnabled()) WorkTracking::SetEnabled(true);
        SetWorkMeasurementStopped(false);
        Repaint();
        return wasOn ? L"Az időmérés már fut." : L"Elindítottam az időmérést.";
    }
    case Command::MeasureOff:
        if (!WorkTracking::IsEnabled() || g_workMeasurementStopped) return L"Az időmérés nem fut.";
        SetWorkMeasurementStopped(true);
        Repaint();
        return L"Leállítottam az időmérést.";
    case Command::TimerStart:
        if (!g_intervalTimer.IsActive()) {
            TimerView::StartConfiguredTimer();
            Repaint();
            return L"Elindítottam az időzítőt.";
        }
        if (!g_intervalTimer.IsPaused()) return L"Az időzítő már fut.";
        PressTimer(IDC_TIMER_PAUSE_BTN);
        Repaint();
        return L"Folytatom az időzítőt.";
    case Command::TimerPause:
        if (!g_intervalTimer.IsActive()) return L"Az időzítő nem fut.";
        if (g_intervalTimer.IsPaused()) return L"Az időzítő már szünetel.";
        PressTimer(IDC_TIMER_PAUSE_BTN);
        Repaint();
        return L"Megállítottam az időzítőt.";
    case Command::TimerNext:
        if (!g_intervalTimer.IsActive()) return L"Az időzítő nem fut.";
        PressTimer(IDC_TIMER_NEXT_BTN);
        Repaint();
        if (!g_intervalTimer.IsActive()) return L"Az időzítő végzett.";
        return g_intervalTimer.IsWorkPhase() ? L"Jöhet a munka." : L"Jöhet a pihenő.";
    case Command::TaskAdd: {
        const std::wstring text = Trim(request.text);
        if (text.empty()) {
            ok = false;
            return L"Mit vegyek fel feladatnak?";
        }
        const int id = g_store.Add(text, g_store.DefaultSourceIds());
        if (id != 0 && g_sessionActiveIdsInitialized) {
            g_sessionActiveTaskIds.insert(g_sessionActiveTaskIds.begin(), id);
        }
        Repaint();
        return L"Felvettem: " + text + L".";
    }
    case Command::TaskDone: {
        const int id = LauraProtocol::FindOpenTask(g_store.tasks, request.text);
        if (id == LauraProtocol::AMBIGUOUS_TASK) {
            ok = false;
            return L"Több feladat is illik rá, mondd pontosabban.";
        }
        const Task* task = nullptr;
        for (const Task& candidate : g_store.tasks) {
            if (candidate.id == id) task = &candidate;
        }
        if (!task) {
            ok = false;
            return L"Nem találtam ilyen nyitott feladatot.";
        }
        const std::wstring text = task->text;
        g_store.ToggleCompleted(id);
        UpdateControlsVisibility();
        Repaint();
        return L"Késznek jelöltem: " + text + L".";
    }
    case Command::OpenWindow:
        ShowAppWindow();
        return L"";
    case Command::OpenPin:
        ShowPinWindow();
        return L"";
    case Command::Unknown:
        break;
    }
    ok = false;
    return L"Ezt a parancsot nem ismerem.";
}

void RunPending() {
    if (g_pending.empty()) return;
    const std::string json = std::move(g_pending.front());
    g_pending.pop_front();
    LauraProtocol::Request request;
    if (!LauraProtocol::ParseRequest(json, request)) return;
    bool ok = true;
    g_spokeDuringCommand = false;
    g_runningCommand = true;
    std::wstring reply = Run(request, ok);
    g_runningCommand = false;
    if (g_spokeDuringCommand) reply.clear(); // The timer cue already told what happened.
    Send(LauraProtocol::BuildReply(request.req, ok, reply));
}
}

void Start(HWND hWnd) {
    g_self = hWnd;
    g_message = RegisterWindowMessageW(LauraProtocol::MESSAGE_NAME);
    g_session = LauraProtocol::Session(g_store.laura_enabled);
    if (g_message && g_store.laura_enabled) {
        PostMessageW(HWND_BROADCAST, g_message, reinterpret_cast<WPARAM>(hWnd), LauraProtocol::Hello);
    }
}

void Stop() {
    if (g_session.Connected()) Signal(g_session.Peer(), LauraProtocol::Bye);
    Unwatch();
}

bool HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT& result) {
    if (msg == g_message && g_message != 0) {
        const HWND from = reinterpret_cast<HWND>(wParam);
        result = 0;
        if (from == g_self || !IsWindow(from)) return true; // Our own broadcast comes back too.
        switch (lParam) {
        case LauraProtocol::Hello: Apply(g_session.OnHello(from)); break;
        case LauraProtocol::HelloAck: Apply(g_session.OnHelloAck(from)); break;
        case LauraProtocol::Bye: Apply(g_session.OnBye(from)); break;
        }
        return true;
    }
    switch (msg) {
    case WM_COPYDATA: {
        const auto* data = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
        if (!data || data->dwData != LauraProtocol::COPYDATA_TAG) return false;
        result = FALSE;
        if (!g_session.Connected() || reinterpret_cast<HWND>(wParam) != g_session.Peer()) return true;
        if (data->cbData == 0 || data->cbData > 64 * 1024 || g_pending.size() >= 16) return true;
        g_pending.emplace_back(static_cast<const char*>(data->lpData), data->cbData);
        PostMessageW(g_self, WM_APP_LAURA_COMMAND, 0, 0);
        result = TRUE;
        return true;
    }
    case WM_APP_LAURA_COMMAND:
        RunPending();
        result = 0;
        return true;
    case WM_APP_LAURA_EXITED:
        if (wParam == g_watchNumber) Apply(g_session.OnPeerExited());
        result = 0;
        return true;
    }
    return false;
}

bool IsDetected() { return g_session.Detected(); }
bool IsConnected() { return g_session.Connected(); }
bool IsEnabled() { return g_session.Enabled(); }

void ToggleEnabled() {
    const bool on = !g_session.Enabled();
    g_store.laura_enabled = on;
    g_store.SaveLocal();
    Apply(g_session.SetEnabled(on));
}

bool Say(const char* event, const std::wstring& text) {
    if (!g_session.Connected()) return false;
    const bool taken = Send(LauraProtocol::BuildSay(event, text));
    if (taken && g_runningCommand) g_spokeDuringCommand = true;
    return taken;
}
}
