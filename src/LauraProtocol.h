#pragma once

// Protocol with Laura (the user's voice assistant, C:\Work\Laura), without UI (tested).
// Handshake: both sides register LauraTaskmanager_v1 and send it with wParam = own HWND and
// lParam = Hello / HelloAck / Bye. Each broadcasts Hello at startup; whoever gets a Hello answers
// HelloAck straight to the sender, so the link forms whichever starts first. The TaskManager then
// sends its command catalog. Content goes as WM_COPYDATA (dwData = COPYDATA_TAG, UTF-8 JSON
// object with "t"): TaskManager -> Laura "katalogus", "valasz" (reply to "req"), "mond" (text to
// read aloud instead of a beep; a reply may carry "adat", e.g. the task list for Claude); Laura -> TaskManager "parancs" ("id", "req", optional "szoveg").
// Laura matches speech to the catalog herself, so no LLM is needed for these commands.
// Session is the connection state: the user can switch the link off (the Settings button); then
// Laura is still detected but gets Bye and no answers.

#include "AppTypes.h"
#include "Recurring.h"
#include "WorkHistory.h"
#include <string>
#include <vector>

namespace LauraProtocol {
constexpr wchar_t MESSAGE_NAME[] = L"LauraTaskmanager_v1";
constexpr ULONG_PTR COPYDATA_TAG = 0x4C544D31; // "LTM1"
enum Signal : LPARAM { Hello = 1, HelloAck = 2, Bye = 3 };

enum class Command { Unknown, MeasureOn, MeasureOff, TimerStart, TimerPause, TimerNext, TaskAdd, TaskDone, TaskList, TaskFirst, Status, TaskSchedule, DoneList, TaskEdit, WorkDays, ManualWork, TaskDelete, OpenWindow, OpenPin };

struct Request {
    long long req = 0;
    Command command = Command::Unknown;
    std::string id;
    std::wstring text;
    // feladat.idozit: first day, and the repeat (0: once).
    std::string date;
    int repeatCount = 0;
    RepeatUnit repeatUnit = RepeatUnit::Day;
    std::wstring newText;           // feladat.modosit
    std::string from, to;           // munkaido.napok
    int minutes = 0;                // munkaido.kezi (negative: take away)
};

// The commands Laura may send, with the word stems she recognizes them by (JSON, UTF-8).
const std::string& Catalog();
Command Lookup(const std::string& id);
// False when it is not a "parancs" message.
bool ParseRequest(const std::string& json, Request& request);
// `data`: raw JSON value sent as "adat" (empty: none).
std::string BuildReply(long long req, bool ok, const std::wstring& text, const std::string& data = "");
std::string BuildSay(const char* event, const std::wstring& text);

// The open task the spoken name refers to (accents, case and word endings do not matter): its id,
// 0 when none fits, -1 when several fit equally well.
constexpr int AMBIGUOUS_TASK = -1;
int FindOpenTask(const std::vector<Task>& tasks, const std::wstring& spoken);

// An open task in list order (most important first) with its group's name (empty: none).
struct ListedTask {
    std::wstring text;
    std::wstring group;
    std::wstring assignee;
};
// The first `limit` tasks as one spoken sentence; a group's tasks are named together.
std::wstring DescribeTasks(const std::vector<ListedTask>& tasks, size_t limit);
// The most important task as a sentence.
std::wstring DescribeFirstTask(const std::vector<ListedTask>& tasks);
// A scheduled task that is not in the list yet (a recurring rule).
struct ScheduledTask {
    std::wstring text;
    std::string nextDue;            // YYYY-MM-DD
    int repeatCount = 0;
    RepeatUnit repeatUnit = RepeatUnit::Day;
};
// The whole list for Claude, in list order: {"osszes": N, "elemek": [task | {"csoport", "feladatok": [task]}],
// "utemezett": [{"szoveg", "megjelenik", "ismetles"}]}, a task being {"szoveg", "felelos" (only when assigned)}.
// The scheduled ones are not active yet; they come in the order they appear.
std::string TasksJson(const std::vector<ListedTask>& tasks, const std::vector<ScheduledTask>& scheduled = {});

// Work time and interval timer as they are now (idozito.allapot, for Claude only).
struct Status {
    bool measuring = false;         // Work-time measurement switched on and not stopped.
    bool counting = false;          // Counting this second (not idle, no excluded window).
    std::wstring pausedFor;         // Why it is not counting: "inaktivitás", "kizárt ablak: X", "zárolt gép".
    bool focusMode = false;
    int workedSeconds = 0;          // Measured today.
    int manualSeconds = 0;          // Entered by hand for today.
    int dailyTargetSeconds = 0;
    bool timerActive = false;
    bool timerPaused = false;
    bool workPhase = true;
    int secondsLeft = 0;
    int round = 0;
    int rounds = 0;
    double workMinutes = 0;
    double restMinutes = 0;
    bool visionRule = false;        // 20-20-20 on.
    bool visionBreak = false;       // In a 20-second eye break now.
};
std::string StatusJson(const Status& status);

// A finished task for Claude (feladat.keszek).
struct DoneTask {
    std::wstring text;
    time_t completedAt = 0;
    bool shared = false;            // In a sync file, seen by others too.
    std::wstring files;             // Names of those sync files, ", " between them.
    std::wstring assignee;
    std::wstring changedBy;         // Last to change it; for a shared task usually who finished it.
};
// {"osszes": N, "elemek": [{"szoveg", "kesz": "YYYY-MM-DD HH:MM", "kozos", "fajl"?, "felelos"?, "modositotta"?}]}:
// the newest `limit`, newest first (`tasks` already sorted so).
std::string DoneTasksJson(const std::vector<DoneTask>& tasks, size_t limit);

// Work time of the days from `from` to `to` (YYYY-MM-DD, inclusive) for Claude:
// {"tol", "ig", "napok": [{"datum", "nap", "mert_perc", "kezi_perc", "osszes_perc"}], "teljes_perc", "napi_cel_perc"},
// only days with work. `currentDay` counts `currentSeconds` measured (it is not in `measured` yet).
std::string WorkDaysJson(const WorkHistory::Records& measured, const WorkHistory::Records& manual,
    const std::string& currentDay, int currentSeconds, const std::string& from, const std::string& to, int dailyTarget);
// "45 perc", "2 óra", "1 óra 20 perc".
std::wstring FormatDuration(int minutes);
// True when the day's work time reaches `percent` of the target with this second.
bool CrossesShare(int beforeSeconds, int nowSeconds, int targetSeconds, int percent);
// What the TaskManager says through Laura instead of a sound, in a few wordings each.
enum class Announce { Rest, Work, TimerEnd, VisionStart, VisionEnd, WorkNinety };
// A wording of `kind` chosen by `random`; Rest names the break's minutes.
std::wstring AnnouncementText(Announce kind, unsigned random, double restMinutes = 0);

// "Anna új feladatot írt ki: Számla." (or "Új közös feladat: …" without a name).
std::wstring NewTaskSentence(const std::wstring& author, const std::wstring& text);

// feladat.torol: Laura may delete only tasks written in the last 24 hours. An error to read aloud,
// or empty when it may go.
constexpr time_t DELETE_WINDOW_SECONDS = 24 * 60 * 60;
std::wstring CheckDelete(time_t createdAt, time_t now);

// feladat.idozit: an error to read aloud when the request cannot be scheduled, else empty.
std::wstring CheckSchedule(const Request& request, const std::string& today);
// "Ütemeztem: Számla, 2026.12.03., hetente."
std::wstring DescribeSchedule(const std::wstring& text, const std::string& date, int count, RepeatUnit unit);

// What the caller has to do after a state change.
struct Actions {
    bool unwatch = false;     // Stop watching the previous Laura process (before watch).
    bool watch = false;       // Watch the process of Peer() for an exit without Bye.
    bool sendAck = false;     // HelloAck to Peer().
    bool sendHello = false;   // Hello to Peer().
    bool sendBye = false;     // Bye to Peer().
    bool sendCatalog = false;
    bool lost = false;        // A live link ended without Bye: tell the user.
    bool found = false;       // Laura appeared while this was running and the link formed: tell the user.
    bool changed = false;     // Detected / connected changed: refresh the Settings button.
};

class Session {
public:
    explicit Session(bool enabled = true) : enabled(enabled) {}
    Actions OnHello(HWND from);
    Actions OnHelloAck(HWND from);
    Actions OnBye(HWND from);
    Actions OnPeerExited();
    Actions SetEnabled(bool on);

    bool Detected() const { return peer != nullptr; }
    bool Connected() const { return connected; }
    bool Enabled() const { return enabled; }
    HWND Peer() const { return peer; }

private:
    HWND peer = nullptr;
    bool connected = false;
    bool enabled = true;
    Actions Adopt(HWND from);
    Actions Connect(Actions actions);
};
}
