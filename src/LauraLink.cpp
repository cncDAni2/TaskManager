#include "LauraLink.h"
#include "AppState.h"
#include "AudioCue.h"
#include "IntervalTimer.h"
#include "Layout.h"
#include "LauraProtocol.h"
#include "Clock.h"
#include "MainWindow.h"
#include "PopupNotice.h"
#include "SettingsDialog.h"
#include "StateToast.h"
#include "TaskUtils.h"
#include "TimerView.h"
#include "UndoToast.h"
#include "UIEventHandler.h"
#include "WorkTracking.h"
#include <algorithm>
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
struct NewTask { std::wstring author, text; };
std::deque<NewTask> g_newTasks;        // Waiting for the user to be active.
int g_lastWorkTotal = -1;              // Today's work seconds at the previous tick (-1: not known).
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

// Announcements Laura can no longer make become the usual pop-ups.
void FlushToPopups() {
    for (const NewTask& task : g_newTasks) ShowNewTaskNotice(task.text);
    g_newTasks.clear();
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
        AudioCue::PlayLinkLost();
    }
    if (actions.found) StateToast::ShowLaura(true);
    if (actions.changed) RefreshSettingsLaura();
    if (!g_session.Connected()) FlushToPopups();
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

std::string CurrentWorkDay() {
    return WorkHistory::DateKey(g_store.last_reset_time != 0 ? g_store.last_reset_time : time(nullptr));
}

int ManualSeconds(const std::string& day) {
    const auto it = g_store.manual_work_history.find(day);
    return it == g_store.manual_work_history.end() ? 0 : (std::max)(0, it->second);
}

bool UserAway() {
    DWORD lastInput = 0;
    if (g_isSessionLocked) return true;
    return Clock::LastInputTick(lastInput) && Clock::InputTick() - lastInput >= 120000;
}

std::vector<LauraProtocol::ScheduledTask> ScheduledInOrder() {
    std::vector<LauraProtocol::ScheduledTask> list;
    for (const RecurringRule& rule : g_store.recurring_rules) {
        list.push_back({ rule.text, rule.next_due, rule.repeat_count, rule.repeat_unit });
    }
    std::stable_sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.nextDue < b.nextDue; });
    return list;
}

// Open tasks in the order of the list (most important first), with their group names.
std::vector<LauraProtocol::ListedTask> OpenTasksInOrder() {
    std::vector<int> order = g_sessionActiveIdsInitialized ? g_sessionActiveTaskIds : g_store.active_order;
    g_store.PrependUnorderedActive(order);
    std::vector<LauraProtocol::ListedTask> list;
    for (int id : order) {
        for (const Task& task : g_store.tasks) {
            if (task.id != id || task.completed) continue;
            const TaskGroup* group = task.group_uid.empty() ? nullptr : g_store.FindGroup(task.group_uid);
            list.push_back({ task.text, group ? group->name : std::wstring(), task.assignee });
            break;
        }
    }
    return list;
}

// Runs one command; the reply is the sentence Laura reads aloud (empty: a cue already spoke).
// `data` (JSON) goes along for Claude, who may need more than the spoken sentence.
std::wstring Run(const LauraProtocol::Request& request, bool& ok, std::string& data) {
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
    case Command::TaskList: {
        const auto tasks = OpenTasksInOrder();
        data = LauraProtocol::TasksJson(tasks, ScheduledInOrder());
        return LauraProtocol::DescribeTasks(tasks, 5);
    }
    case Command::TaskFirst: {
        const auto tasks = OpenTasksInOrder();
        data = LauraProtocol::TasksJson(tasks, ScheduledInOrder());
        return LauraProtocol::DescribeFirstTask(tasks);
    }
    case Command::Status: {
        LauraProtocol::Status s;
        s.measuring = WorkTracking::IsEnabled() && !g_workMeasurementStopped;
        s.counting = s.measuring && g_isWorkActive;
        if (s.measuring && !s.counting) {
            s.pausedFor = g_isSessionLocked ? L"zárolt gép"
                : g_isExcludedApp ? L"kizárt ablak: " + g_excludedAppName
                : g_isIdlePaused ? L"inaktivitás" : L"";
        }
        s.focusMode = g_focusMode;
        s.workedSeconds = g_store.work_seconds_today;
        s.manualSeconds = ManualSeconds(CurrentWorkDay());
        s.dailyTargetSeconds = DAILY_WORK_TARGET_SECONDS;
        s.timerActive = g_intervalTimer.IsActive();
        s.timerPaused = g_intervalTimer.IsPaused();
        s.workPhase = g_intervalTimer.IsWorkPhase();
        s.secondsLeft = g_intervalTimer.SecondsRemaining();
        s.round = g_intervalTimer.CurrentRepetition();
        s.rounds = g_intervalTimer.RepetitionCount();
        s.workMinutes = g_store.timer_work_minutes;
        s.restMinutes = g_store.timer_rest_minutes;
        s.visionRule = g_store.timer_2020_enabled;
        s.visionBreak = g_intervalTimer.IsVisionBreakActive();
        data = LauraProtocol::StatusJson(s);
        return L"";
    }
    case Command::TaskSchedule: {
        const std::wstring problem = LauraProtocol::CheckSchedule(request, Recurring::Today());
        if (!problem.empty()) {
            ok = false;
            return problem;
        }
        RecurringRule rule;
        rule.text = Trim(request.text);
        rule.start_date = request.date;
        rule.repeat_count = request.repeatCount;
        rule.repeat_unit = request.repeatUnit;
        rule.targets = g_store.DefaultSourceIds();
        const std::vector<int> created = g_store.SaveRecurringRule(rule); // Today: the task appears at once.
        if (g_sessionActiveIdsInitialized) {
            for (auto it = created.rbegin(); it != created.rend(); ++it) {
                g_sessionActiveTaskIds.insert(g_sessionActiveTaskIds.begin(), *it);
            }
        }
        Repaint();
        return LauraProtocol::DescribeSchedule(rule.text, rule.start_date, rule.repeat_count, rule.repeat_unit);
    }
    case Command::DoneList: {
        std::vector<LauraProtocol::DoneTask> done;
        for (const Task& task : g_store.GetAllCompletedTasks()) { // Newest first.
            LauraProtocol::DoneTask item{ task.text, task.completed_at, task.is_sync, L"", task.assignee,
                task.is_sync ? task.updated_by : L"" };
            for (int sourceId : task.sources) {
                const SyncSource* source = g_store.FindSource(sourceId);
                if (!source) continue;
                if (!item.files.empty()) item.files += L", ";
                item.files += source->name;
            }
            done.push_back(std::move(item));
        }
        data = LauraProtocol::DoneTasksJson(done, 50);
        return L"";
    }
    case Command::TaskEdit: {
        const std::wstring newText = Trim(request.newText);
        if (newText.empty()) {
            ok = false;
            return L"Mire írjam át?";
        }
        const int id = LauraProtocol::FindOpenTask(g_store.tasks, request.text);
        if (id == LauraProtocol::AMBIGUOUS_TASK) {
            ok = false;
            return L"Több feladat is illik rá, mondd pontosabban.";
        }
        if (id == 0 || std::none_of(g_store.tasks.begin(), g_store.tasks.end(),
                [id](const Task& task) { return task.id == id; })) {
            ok = false;
            return L"Nem találtam ilyen nyitott feladatot.";
        }
        g_store.UpdateText(id, newText);
        Repaint();
        return L"Átírtam: " + newText + L".";
    }
    case Command::WorkDays: {
        const std::string today = CurrentWorkDay();
        std::string from = request.from.empty() ? request.to : request.from;
        std::string to = request.to.empty() ? from : request.to;
        if (from.empty()) from = to = today;
        if (!Recurring::IsValidDate(from) || !Recurring::IsValidDate(to) || from > to) {
            ok = false;
            return L"Nem értem, melyik napokra gondolsz.";
        }
        data = LauraProtocol::WorkDaysJson(g_store.work_history, g_store.manual_work_history, today,
            g_store.work_seconds_today, from, to, DAILY_WORK_TARGET_SECONDS);
        return L"";
    }
    case Command::ManualWork: {
        const std::string today = CurrentWorkDay();
        const std::string day = request.date.empty() ? today : request.date;
        if (!Recurring::IsValidDate(day)) {
            ok = false;
            return L"Nem értem a dátumot.";
        }
        if (day > today) {
            ok = false;
            return L"Jövőbeli napra nem írhatok munkaidőt.";
        }
        if (request.minutes == 0) {
            ok = false;
            return L"Mennyi időt írjak be?";
        }
        const int seconds = (std::max)(0, ManualSeconds(day) + request.minutes * 60);
        g_store.SetManualWork(day, seconds);
        Repaint();
        return std::wstring(request.minutes > 0 ? L"Beírtam" : L"Levontam") + L", a kézi munkaidő " +
            (day == today ? L"ma" : Recurring::FormatDate(day)) + L" összesen " +
            LauraProtocol::FormatDuration(seconds / 60) + L".";
    }
    case Command::TaskDelete: {
        const int id = LauraProtocol::FindOpenTask(g_store.tasks, request.text);
        if (id == LauraProtocol::AMBIGUOUS_TASK) {
            ok = false;
            return L"Több feladat is illik rá, mondd pontosabban.";
        }
        const auto task = std::find_if(g_store.tasks.begin(), g_store.tasks.end(),
            [id](const Task& candidate) { return candidate.id == id; });
        if (id == 0 || task == g_store.tasks.end()) {
            ok = false;
            return L"Nem találtam ilyen nyitott feladatot.";
        }
        const std::wstring refused = LauraProtocol::CheckDelete(task->created_at, time(nullptr));
        if (!refused.empty()) {
            ok = false;
            return refused;
        }
        const std::wstring text = task->text;
        UndoToast::DeleteTasks({ id }); // 5 seconds to undo it.
        UpdateControlsVisibility();
        if (g_selectedIndex >= static_cast<int>(g_displayItems.size())) {
            g_selectedIndex = static_cast<int>(g_displayItems.size()) - 1;
        }
        Repaint();
        return L"Töröltem: " + text + L".";
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
    std::string data;
    std::wstring reply = Run(request, ok, data);
    g_runningCommand = false;
    if (g_spokeDuringCommand) reply.clear(); // The timer cue already told what happened.
    Send(LauraProtocol::BuildReply(request.req, ok, reply, data));
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

std::wstring Announcement(int kind, double restMinutes) {
    static unsigned random = GetTickCount();
    random = random * 1103515245u + 12345u;
    return LauraProtocol::AnnouncementText(static_cast<LauraProtocol::Announce>(kind),
        random >> 16, restMinutes);
}

bool AnnounceNewTask(const std::wstring& author, const std::wstring& text) {
    if (!g_session.Connected() || !g_store.sounds_enabled) return false;
    g_newTasks.push_back({ author, text });
    Tick(); // At once when the user is here.
    return true;
}

void Tick() {
    if (!g_session.Connected()) {
        g_lastWorkTotal = -1;
        return;
    }
    const int total = g_store.work_seconds_today + ManualSeconds(CurrentWorkDay());
    if (g_lastWorkTotal >= 0 && g_store.sounds_enabled &&
        LauraProtocol::CrossesShare(g_lastWorkTotal, total, DAILY_WORK_TARGET_SECONDS, 90)) {
        Say("munkaido_90", Announcement(static_cast<int>(LauraProtocol::Announce::WorkNinety)));
    }
    g_lastWorkTotal = total;
    if (g_newTasks.empty() || UserAway()) return;
    while (!g_newTasks.empty()) {
        const NewTask task = g_newTasks.front();
        g_newTasks.pop_front();
        if (!Say("uj_kozos_feladat", LauraProtocol::NewTaskSentence(task.author, task.text))) {
            ShowNewTaskNotice(task.text);
        }
    }
}

bool Say(const char* event, const std::wstring& text) {
    if (!g_session.Connected()) return false;
    const bool taken = Send(LauraProtocol::BuildSay(event, text));
    if (taken && g_runningCommand) g_spokeDuringCommand = true;
    return taken;
}
}
