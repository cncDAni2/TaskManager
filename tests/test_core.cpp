// Logic tests: sync merge, sync and local files, JSON, recurring dates and tasks, cutoffs, work
// history, settings round trip, assignment, and the timed flows (work-time counting, interval
// timer, STRICT MODE, 20-20-20, PIN timer buttons) on a hand-driven clock, and the Laura link
// protocol. Run tests\run_tests.bat.
// Everything happens in the test EXE's own folder (dist\test), never on real data.
#include "Clock.h"
#include "IntervalTimer.h"
#include "JsonLite.h"
#include "LauraProtocol.h"
#include "PinTimerButtons.h"
#include "Task.h"
#include "Version.h"
#include "WorkTick.h"
#include <cstdio>
#include <functional>
#include <set>

// Hotkeys.cpp needs the whole app; the store only asks this one thing from it.
namespace Hotkeys { bool IsValidCombo(long long) { return true; } }

// The hand-driven clock of the test build (Clock.h).
unsigned long long Clock::testMs = 0;
DWORD Clock::testLastInputTick = 0;

// The cues the timers asked for, in order, instead of sounds.
std::vector<std::string> g_cues;
namespace AudioCue {
void PlayTimerWorkMelody() { g_cues.push_back("work"); }
void PlayTimerRestMelody() { g_cues.push_back("rest"); }
void PlayVisionBreakStart() { g_cues.push_back("vision start"); }
void PlayVisionBreakEnd() { g_cues.push_back("vision end"); }
}

namespace {
int g_checks = 0;
int g_failures = 0;

#define CHECK(condition) Check((condition), #condition, __LINE__)

void Check(bool ok, const char* text, int line) {
    ++g_checks;
    if (ok) return;
    ++g_failures;
    std::printf("  FAILED line %d: %s\n", line, text);
}

std::wstring ExeFolder() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    *(wcsrchr(path, L'\\') + 1) = L'\0';
    return path;
}

std::wstring LocalPath() { return ExeFolder() + L"tasks-local.json"; }
std::wstring SyncPath(const wchar_t* name) { return ExeFolder() + name; }

void WriteText(const std::wstring& path, const std::string& text) {
    CHECK(TaskUtils::WriteFileAtomic(path, text));
}

std::string ReadText(const std::wstring& path) {
    std::string text;
    TaskUtils::ReadFileBytes(path, text);
    return text;
}

void CleanFiles() {
    DeleteFileW(LocalPath().c_str());
    DeleteFileW(SyncPath(L"a.json").c_str());
    DeleteFileW(SyncPath(L"b.json").c_str());
}

SyncRecord Record(const char* uid, const wchar_t* text, time_t updatedAt, bool deleted = false) {
    SyncRecord record;
    record.uid = uid;
    record.text = text;
    record.created_at = 1000;
    record.updated_at = updatedAt;
    record.updated_by = L"kolléga";
    record.deleted = deleted;
    return record;
}

void WriteSync(const std::wstring& path, const std::vector<SyncRecord>& records) {
    Sleep(20); // Changes are noticed by the file time: two writes in one clock tick would look like one.
    FILETIME written{};
    CHECK(SyncFiles::Write(path, records, SyncFileInfo(), written));
}

std::vector<SyncRecord> ReadSync(const std::wstring& path) {
    std::vector<SyncRecord> records;
    SyncFileInfo info;
    FILETIME written{};
    CHECK(SyncFiles::Read(path, records, info, written) == SyncFiles::ReadResult::Ok);
    return records;
}

SyncSource Source(int id, const std::wstring& path) {
    SyncSource source;
    source.id = id;
    source.name = L"teszt";
    source.path = path;
    return source;
}

const Task* FindByUid(const TaskStore& store, const char* uid) {
    for (const Task& task : store.tasks) {
        if (task.uid == uid) return &task;
    }
    return nullptr;
}

void TestMergeRules() {
    SyncFiles::RecordMap winners;
    SyncFiles::MergeNewest(winners, { Record("x", L"régi", 10) });
    SyncFiles::MergeNewest(winners, { Record("x", L"új", 20) });
    SyncFiles::MergeNewest(winners, { Record("x", L"megint régi", 15) });
    CHECK(winners["x"].text == L"új");
    SyncFiles::MergeNewest(winners, { Record("x", L"azonos idő", 20) });
    CHECK(winners["x"].text == L"új"); // A tie keeps what is there, so the merge cannot flip-flop.

    std::vector<SyncRecord> file = { Record("x", L"régi", 10) };
    SyncFiles::RecordMap other;
    SyncFiles::MergeNewest(other, { Record("x", L"új", 20), Record("y", L"más fájlé", 20) });
    CHECK(SyncFiles::ApplyNewer(file, other));
    CHECK(file.size() == 1 && file[0].text == L"új"); // Never adds a uid the file does not hold.
    CHECK(!SyncFiles::ApplyNewer(file, other));

    std::vector<SyncRecord> records = { Record("x", L"eredeti", 10) };
    CHECK(!SyncFiles::Upsert(records, Record("x", L"régebbi", 9)));
    CHECK(!SyncFiles::Upsert(records, Record("x", L"azonos", 10)));
    CHECK(records[0].text == L"eredeti");
    CHECK(SyncFiles::Upsert(records, Record("x", L"újabb", 11)));
    CHECK(SyncFiles::Upsert(records, Record("y", L"új uid", 1)));
    CHECK(records.size() == 2 && records[0].text == L"újabb");

    // A delete is a newer change like any other, and an older copy cannot bring the task back.
    std::vector<SyncRecord> deleted = { Record("x", L"", 30, true) };
    CHECK(!SyncFiles::Upsert(deleted, Record("x", L"feltámadna", 29)));
    CHECK(deleted[0].deleted);
    CHECK(SyncFiles::Upsert(deleted, Record("x", L"visszavonás", 31)));
    CHECK(!deleted[0].deleted);
}

void TestTombstones() {
    const time_t now = 100 * 86400;
    const time_t retention = 14 * 86400;
    std::vector<SyncRecord> records = {
        Record("kept", L"", now - retention, true),
        Record("gone", L"", now - retention - 1, true),
        Record("alive", L"régi, de él", 1),
    };
    CHECK(SyncFiles::PurgeTombstones(records, now));
    CHECK(records.size() == 2 && SyncFiles::Contains(records, "kept") && SyncFiles::Contains(records, "alive"));
    CHECK(!SyncFiles::PurgeTombstones(records, now));
}

void TestSyncFileRoundTrip() {
    CleanFiles();
    const std::wstring path = SyncPath(L"a.json");
    SyncRecord record = Record("x", L"Árvíztűrő \"tükör\" \\ fúrógép\nmásodik {sor} [1]", 1234);
    record.author = L"Kovács Éva";
    record.assignee = L"Nagy Ödön";
    record.completed = true;
    record.completed_at = 999;
    record.recurring_uid = "szabály";
    record.extras.push_back({ "jovobeli", "{\"a\": [1, 2]}" });
    SyncFileInfo info;
    info.extras.push_back({ "felso", "[\"x\"]" });
    FILETIME written{};
    CHECK(SyncFiles::Write(path, { record }, info, written));

    std::vector<SyncRecord> read;
    SyncFileInfo readInfo;
    FILETIME readTime{};
    CHECK(SyncFiles::Read(path, read, readInfo, readTime) == SyncFiles::ReadResult::Ok);
    CHECK(read.size() == 1);
    CHECK(read[0].text == record.text && read[0].author == record.author && read[0].assignee == record.assignee);
    CHECK(read[0].completed && read[0].completed_at == 999 && read[0].updated_at == 1234);
    CHECK(read[0].recurring_uid == "szabály");
    CHECK(read[0].extras.size() == 1 && read[0].extras[0].second == "{\"a\": [1, 2]}");
    CHECK(readInfo.extras.size() == 1 && readInfo.extras[0].first == "felso");
    CHECK(readInfo.appVersion == TASK_MANAGER_VERSION_STRING);
    CHECK(CompareFileTime(&written, &readTime) == 0);
}

void TestDamagedSyncFiles() {
    CleanFiles();
    const std::wstring path = SyncPath(L"a.json");
    std::vector<SyncRecord> records;
    SyncFileInfo info;
    FILETIME written{};
    CHECK(SyncFiles::Read(path, records, info, written) == SyncFiles::ReadResult::Missing);
    WriteText(path, "");
    CHECK(SyncFiles::Read(path, records, info, written) == SyncFiles::ReadResult::Ok && records.empty());
    WriteText(path, "{\n  \"tasks\": [\n    { \"uid\": \"x\", \"text\": \"csonka");
    CHECK(SyncFiles::Read(path, records, info, written) == SyncFiles::ReadResult::Error);
    WriteText(path, "{ \"valami\": 1 }");
    CHECK(SyncFiles::Read(path, records, info, written) == SyncFiles::ReadResult::Error);
}

void TestPoll() {
    CleanFiles();
    const std::wstring path = SyncPath(L"a.json");
    SyncFiles::PollFile missing;
    missing.path = path;
    CHECK(!SyncFiles::Poll(missing)); // Was missing, still missing.

    WriteSync(path, { Record("x", L"egy", 10) });
    FILETIME current{};
    CHECK(SyncFiles::GetWriteTime(path, current));
    SyncFiles::PollFile same;
    same.path = path;
    same.seenWrite = current;
    CHECK(!SyncFiles::Poll(same));
    same.force = true;
    CHECK(SyncFiles::Poll(same) && same.records.size() == 1);

    SyncFiles::PollFile changed;
    changed.path = path;
    CHECK(SyncFiles::Poll(changed));
    CHECK(changed.result == SyncFiles::ReadResult::Ok && changed.records[0].text == L"egy");

    WriteText(path, "{ \"tasks\": [ {");
    SyncFiles::PollFile damaged;
    damaged.path = path;
    CHECK(!SyncFiles::Poll(damaged)); // Nothing to apply: the cached contents stay.

    DeleteFileW(path.c_str());
    SyncFiles::PollFile deleted;
    deleted.path = path;
    deleted.seenWrite = current;
    CHECK(SyncFiles::Poll(deleted) && deleted.result == SyncFiles::ReadResult::Missing);
}

void TestStoreSync() {
    CleanFiles();
    const std::wstring path = SyncPath(L"a.json");
    WriteSync(path, { Record("x", L"első", 100) });
    TaskStore store;
    store.SetSyncSources({ Source(1, path) });
    CHECK(FindByUid(store, "x") && FindByUid(store, "x")->text == L"első");

    // A colleague's newer change arrives; an older one does not win.
    WriteSync(path, { Record("x", L"kolléga módosította", 200) });
    store.ReloadSync(false, true);
    CHECK(FindByUid(store, "x")->text == L"kolléga módosította");

    // Read -> merge -> write: an own change keeps what a colleague added in the meantime.
    std::vector<SyncRecord> current = ReadSync(path);
    current.push_back(Record("y", L"kolléga új feladata", 300));
    WriteSync(path, current);
    const int id = FindByUid(store, "x")->id;
    store.UpdateText(id, L"saját módosítás");
    std::vector<SyncRecord> merged = ReadSync(path);
    CHECK(merged.size() == 2 && SyncFiles::Contains(merged, "y"));
    CHECK(FindByUid(store, "x")->text == L"saját módosítás" && FindByUid(store, "y"));

    // A tombstone removes the task, and the delete stays in the file.
    merged = ReadSync(path);
    for (SyncRecord& record : merged) {
        if (record.uid == "y") {
            record.deleted = true;
            record.updated_at += 100;
        }
    }
    WriteSync(path, merged);
    store.ReloadSync(false, true);
    CHECK(!FindByUid(store, "y"));
}

void TestDamagedFileIsNotOverwritten() {
    CleanFiles();
    const std::wstring path = SyncPath(L"a.json");
    WriteSync(path, { Record("x", L"első", 100) });
    TaskStore store;
    store.SetSyncSources({ Source(1, path) });

    const std::string damaged = "{ \"tasks\": [ { \"uid\": \"x\", ";
    WriteText(path, damaged);
    store.UpdateText(FindByUid(store, "x")->id, L"vár a kiírásra");
    CHECK(ReadText(path) == damaged);
    CHECK(FindByUid(store, "x") && FindByUid(store, "x")->text == L"vár a kiírásra");

    // Once the file is readable again the waiting change goes out.
    WriteSync(path, { Record("x", L"első", 100) });
    store.ReloadSync(false, true);
    CHECK(ReadSync(path)[0].text == L"vár a kiírásra");
}

void TestRelay() {
    CleanFiles();
    const std::wstring a = SyncPath(L"a.json");
    const std::wstring b = SyncPath(L"b.json");
    WriteSync(a, { Record("x", L"újabb", 200), Record("csak-a", L"csak itt", 150) });
    WriteSync(b, { Record("x", L"régebbi", 100) });
    TaskStore store;
    store.SetSyncSources({ Source(1, a), Source(2, b) });

    const std::vector<SyncRecord> relayed = ReadSync(b);
    CHECK(relayed.size() == 1); // Files get changes only for uids they already hold.
    CHECK(relayed[0].text == L"újabb");
    CHECK(relayed[0].updated_at == 200 && relayed[0].updated_by == L"kolléga"); // Not a new edit.
    CHECK(FindByUid(store, "x")->sources.size() == 2);
}

void TestApplyPolled() {
    CleanFiles();
    const std::wstring path = SyncPath(L"a.json");
    WriteSync(path, { Record("x", L"első", 100) });
    TaskStore store;
    store.SetSyncSources({ Source(1, path) });

    // The background read finds a colleague's change and it gets applied.
    WriteSync(path, { Record("x", L"háttérből", 200) });
    std::vector<SyncFiles::PollFile> files = store.PollSnapshot();
    CHECK(files.size() == 1 && SyncFiles::Poll(files[0]));
    store.ApplyPolled(files);
    CHECK(FindByUid(store, "x")->text == L"háttérből");

    // An own write during the background read wins: the older result is dropped.
    WriteSync(path, { Record("x", L"a szál ezt látta", 300) });
    files = store.PollSnapshot();
    CHECK(SyncFiles::Poll(files[0]));
    store.UpdateText(FindByUid(store, "x")->id, L"közben saját írás");
    store.ApplyPolled(files);
    CHECK(FindByUid(store, "x")->text == L"közben saját írás");
    CHECK(ReadSync(path)[0].text == L"közben saját írás");

    // A file that left the settings meanwhile is ignored.
    files = store.PollSnapshot();
    files[0].result = SyncFiles::ReadResult::Ok;
    files[0].records.clear();
    files[0].sourceId = 99;
    store.ApplyPolled(files);
    CHECK(FindByUid(store, "x") != nullptr);
}

void TestLocalLoad() {
    CleanFiles();
    {
        TaskStore store; // First run.
        CHECK(!store.localLoadFailed);
        CHECK(store.pin_list_mode == 2 && store.pin_list_count == 0); // Defaults keep the old PIN list.
        store.Add(L"Első feladat ő", {});
        store.work_seconds_today = 1234;
        store.pin_list_mode = 1;
        store.pin_list_count = 5;
        store.Save();
    }
    {
        TaskStore store;
        CHECK(!store.localLoadFailed);
        CHECK(store.pin_list_mode == 1 && store.pin_list_count == 5);
        CHECK(store.tasks.size() == 1 && store.tasks[0].text == L"Első feladat ő");
        CHECK(store.active_order.size() == 1 && store.active_order[0] == store.tasks[0].id);
    }

    // Unreadable (held by another program): must not count as a first run.
    const std::string before = ReadText(LocalPath());
    HANDLE lock = CreateFileW(LocalPath().c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    CHECK(lock != INVALID_HANDLE_VALUE);
    {
        TaskStore store;
        CHECK(store.localLoadFailed);
        CloseHandle(lock);
        store.Add(L"nem írhatja felül", {});
        store.Save();
    }
    CHECK(ReadText(LocalPath()) == before);

    // A damaged task list would look like an empty one.
    const std::string damaged = "{\n  \"work_seconds\": 5,\n  \"tasks\": [\n    {\n      \"uid\": \"x\",\n      \"text\": \"cso";
    WriteText(LocalPath(), damaged);
    {
        TaskStore store;
        CHECK(store.localLoadFailed);
        store.Save();
    }
    CHECK(ReadText(LocalPath()) == damaged);

    // A file without a task list is not damaged: early versions and hand-made files can be like that.
    WriteText(LocalPath(), "{\n  \"work_seconds\": 5\n}\n");
    {
        TaskStore store;
        CHECK(!store.localLoadFailed);
    }
}

void TestPinHidden() {
    CleanFiles();
    const std::wstring path = SyncPath(L"a.json");
    WriteSync(path, { Record("x", L"szinkron", 100) });
    int localId = 0;
    {
        TaskStore store;
        store.SetSyncSources({ Source(1, path) });
        localId = store.Add(L"helyi", {});
        store.TogglePinHidden(localId);
        store.TogglePinHidden(FindByUid(store, "x")->id);
        store.Save();
    }
    CHECK(ReadText(path).find("pin_hidden") == std::string::npos); // It is this user's own view.
    {
        TaskStore store;
        CHECK(FindByUid(store, "x") && FindByUid(store, "x")->pin_hidden);
        for (const Task& task : store.tasks) CHECK(task.pin_hidden);

        // It survives a change arriving from the sync file, and can be switched back.
        WriteSync(path, { Record("x", L"módosult", 200) });
        store.ReloadSync(false, true);
        CHECK(FindByUid(store, "x")->pin_hidden);
        store.TogglePinHidden(FindByUid(store, "x")->id);
        CHECK(!FindByUid(store, "x")->pin_hidden);

        // Deleting forgets the task; undoing the delete brings the mark back with it.
        Task local;
        for (const Task& task : store.tasks) {
            if (!task.is_sync) local = task;
        }
        store.DeleteMany({ local.id });
        CHECK(store.pin_hidden_uids.empty());
        store.RestoreTasks({ local }, {});
        CHECK(store.pin_hidden_uids.count(local.uid) == 1);
    }
}

// Five local tasks; returns their ids in list order, like the 1-5 of the PIN example.
std::vector<int> FiveTasks(TaskStore& store) {
    std::vector<int> ids;
    for (const wchar_t* text : { L"5", L"4", L"3", L"2", L"1" }) ids.insert(ids.begin(), store.Add(text, {}));
    return ids;
}

const Task* TaskWithId(const TaskStore& store, int id) {
    for (const Task& task : store.tasks) {
        if (task.id == id) return &task;
    }
    return nullptr;
}

void TestGroupOrder() {
    CleanFiles();
    TaskStore store;
    const std::vector<int> t = FiveTasks(store);
    std::vector<int> order = t;

    // Dropping 5 on 2: a new group at the place of 2, the dragged task after it.
    const std::string group = store.GroupWith(order, t[4], t[1]);
    CHECK(!group.empty());
    CHECK(store.FindGroup(group) && store.FindGroup(group)->name == L"Csoport");
    CHECK((order == std::vector<int>{ t[0], t[1], t[4], t[2], t[3] }));

    // Dropping on a member joins that group and creates nothing.
    CHECK(store.GroupWith(order, t[3], t[1]).empty());
    CHECK((order == std::vector<int>{ t[0], t[1], t[3], t[4], t[2] }));
    CHECK(TaskWithId(store, t[3])->group_uid == group);

    // Taking a task out puts it right above the group, also when it was the first member.
    store.RemoveFromGroup(order, t[4]);
    CHECK((order == std::vector<int>{ t[0], t[4], t[1], t[3], t[2] }));
    CHECK(TaskWithId(store, t[4])->group_uid.empty());
    store.RemoveFromGroup(order, t[1]);
    CHECK((order == std::vector<int>{ t[0], t[4], t[1], t[3], t[2] }));

    // Placing inside and outside by the group given; the last member ends the group.
    store.PlaceTask(order, t[0], t[3], group);
    CHECK((order == std::vector<int>{ t[4], t[1], t[0], t[3], t[2] }));
    store.PlaceTask(order, t[0], t[2], "");
    store.PlaceTask(order, t[3], 0, "");
    CHECK(store.task_groups.empty());
    CHECK(store.group_members.empty());
}

void TestGroupMoveAndNormalize() {
    CleanFiles();
    TaskStore store;
    const std::vector<int> t = FiveTasks(store);
    std::vector<int> order = t;
    const std::string group = store.GroupWith(order, t[3], t[2]);
    store.GroupWith(order, t[4], t[3]);

    store.MoveGroup(order, group, t[0]);
    CHECK((order == std::vector<int>{ t[2], t[3], t[4], t[0], t[1] }));
    store.MoveGroup(order, group, t[3]); // On itself: nothing happens.
    CHECK((order == std::vector<int>{ t[2], t[3], t[4], t[0], t[1] }));
    store.MoveGroup(order, group, 0);
    CHECK((order == std::vector<int>{ t[0], t[1], t[2], t[3], t[4] }));

    // A member that got away, like a reopened task at the end, is pulled back.
    order = { t[2], t[0], t[3], t[1], t[4] };
    store.NormalizeGroups(order);
    CHECK((order == std::vector<int>{ t[2], t[3], t[4], t[0], t[1] }));
}

void TestPinDrop() {
    CleanFiles();
    TaskStore store;
    const std::vector<int> t = FiveTasks(store);
    std::vector<int> order = t;
    store.GroupWith(order, t[3], t[2]);
    store.GroupWith(order, t[4], t[3]); // 1 2 [3 4 5]
    bool whole = false;

    // 5 between 2 and 3: stays in the group, as its first task.
    int before = store.ResolvePinDrop(order, t[4], t[2], whole);
    CHECK(!whole && before == t[2]);
    // 5 above 1: the whole group goes there.
    before = store.ResolvePinDrop(order, t[4], t[0], whole);
    CHECK(whole && before == t[0]);
    // 3 below the group: its last task.
    before = store.ResolvePinDrop(order, t[2], 0, whole);
    CHECK(!whole && before == 0);
    // An outside task never lands inside: it goes to the nearer end of the group.
    before = store.ResolvePinDrop(order, t[0], t[3], whole);
    CHECK(!whole && before == t[2]);
    before = store.ResolvePinDrop(order, t[0], t[4], whole);
    CHECK(!whole && before == 0);

    // A task hidden from the PIN window does not count as the slot below the group.
    order = { t[2], t[3], t[0], t[1], t[4] };
    store.NormalizeGroups(order); // 3 4 5 1 2
    store.TogglePinHidden(t[0]);
    before = store.ResolvePinDrop(order, t[2], t[1], whole);
    CHECK(!whole && before == t[0]);
}

void TestGroupPersistence() {
    CleanFiles();
    const std::wstring path = SyncPath(L"a.json");
    WriteSync(path, { Record("x", L"szinkron", 100), Record("y", L"másik", 100) });
    std::string group;
    {
        TaskStore store;
        store.SetSyncSources({ Source(1, path) });
        const int local = store.Add(L"helyi \"idézett\"", {});
        std::vector<int> order = store.active_order;
        group = store.GroupWith(order, local, FindByUid(store, "x")->id);
        store.RenameGroup(group, L"Költözés \"A\"");
        store.active_order = order;
        store.Save();
    }
    CHECK(ReadText(path).find("group") == std::string::npos); // It is this user's own view.
    {
        TaskStore store; // The nested "members" must not disturb the task list.
        CHECK(store.tasks.size() == 3);
        CHECK(store.FindGroup(group) && store.FindGroup(group)->name == L"Költözés \"A\"");
        CHECK(FindByUid(store, "x")->group_uid == group);

        // A change from the sync file keeps the task in the group, a delete takes it out.
        WriteSync(path, { Record("x", L"módosult", 200), Record("y", L"másik", 100) });
        store.ReloadSync(false, true);
        CHECK(FindByUid(store, "x")->group_uid == group);
        WriteSync(path, { Record("x", L"módosult", 300, true), Record("y", L"másik", 100) });
        store.ReloadSync(false, true);
        CHECK(store.group_members.count("x") == 0);
        CHECK(store.FindGroup(group)); // The local task is still in it.
    }
}

void TestGroupUndoAndFinish() {
    CleanFiles();
    TaskStore store;
    const std::vector<int> t = FiveTasks(store);
    std::vector<int> order = t;
    const std::string group = store.GroupWith(order, t[1], t[0]);
    store.RenameGroup(group, L"Név");

    // Deleting every member ends the group; undoing it brings back the group and its name.
    const std::vector<Task> saved = { *TaskWithId(store, t[0]), *TaskWithId(store, t[1]) };
    const std::vector<TaskGroup> groups = store.GroupsOf(saved);
    store.DeleteMany({ t[0], t[1] });
    CHECK(store.task_groups.empty());
    store.RestoreTasks(saved, {}, groups);
    CHECK(store.FindGroup(group) && store.FindGroup(group)->name == L"Név");
    CHECK(TaskWithId(store, t[0])->group_uid == group);

    // A tick can be taken back while the list session lasts; after that the task has left.
    store.ToggleCompleted(t[0]);
    CHECK(TaskWithId(store, t[0])->group_uid == group);
    store.ToggleCompleted(t[0]);
    CHECK(!store.DropCompletedFromGroups());
    store.ToggleCompleted(t[0]);
    CHECK(store.DropCompletedFromGroups());
    CHECK(TaskWithId(store, t[0])->group_uid.empty());
    CHECK(store.FindGroup(group));
    store.ToggleCompleted(t[1]);
    store.DropCompletedFromGroups();
    CHECK(!store.FindGroup(group));
}

void TestJson() {
    std::vector<std::string> objects;
    const std::string text = "{ \"tasks\": [ { \"text\": \"a } ] \\\" b\" }, { \"n\": { \"m\": [1] } } ], \"z\": 1 }";
    CHECK(JsonLite::ReadObjectArray(text, "tasks", objects) && objects.size() == 2);
    std::string value;
    CHECK(JsonLite::GetString(objects[0], "text", value) && value == "a } ] \" b");
    CHECK(!JsonLite::ReadObjectArray("{ \"tasks\": [ { \"a\": 1 }", "tasks", objects));
    CHECK(!JsonLite::ReadObjectArray("{ \"other\": [] }", "tasks", objects));

    std::vector<JsonLite::Member> members;
    CHECK(JsonLite::ReadMembers("{ \"a\": { \"b\": 1 }, \"c\": \"}\" }", members));
    CHECK(members.size() == 2 && members[0].first == "a" && members[1].first == "c");
    CHECK(!JsonLite::ReadMembers("{ \"a\": \"csonka", members));

    long long number = 0;
    CHECK(JsonLite::GetInt64("{ \"n\": -42 }", "n", number) && number == -42);
    bool flag = false;
    CHECK(JsonLite::GetBool("{ \"f\": true }", "f", flag) && flag);
    std::vector<std::string> scalars;
    CHECK(JsonLite::ReadScalarArray("{ \"s\": [\"a\\\"b\", 12] }", "s", scalars));
    CHECK(scalars.size() == 2 && scalars[0] == "a\"b" && scalars[1] == "12");

    const std::string nasty = "idéző \" per \\ sor\n tab\t";
    CHECK(TaskUtils::UnescapeJsonString(TaskUtils::EscapeJsonString(nasty)) == nasty);
}

void TestRecurringDates() {
    using Recurring::Occurrence;
    CHECK(Occurrence("2026-01-31", 1, RepeatUnit::Month, 1) == "2026-02-28");
    CHECK(Occurrence("2026-01-31", 1, RepeatUnit::Month, 2) == "2026-03-31"); // Keeps the start's day.
    CHECK(Occurrence("2024-01-31", 1, RepeatUnit::Month, 1) == "2024-02-29");
    CHECK(Occurrence("2026-11-30", 1, RepeatUnit::Month, 3) == "2027-02-28");
    CHECK(Occurrence("2026-12-30", 3, RepeatUnit::Day, 1) == "2027-01-02");
    CHECK(Occurrence("2026-02-26", 1, RepeatUnit::Week, 1) == "2026-03-05");

    RecurringRule rule;
    rule.start_date = "2026-01-31";
    rule.repeat_count = 1;
    rule.repeat_unit = RepeatUnit::Month;
    CHECK(Recurring::NextAfter(rule, "2026-01-31") == "2026-02-28");
    CHECK(Recurring::NextAfter(rule, "2026-02-28") == "2026-03-31");
    CHECK(Recurring::NextAfter(rule, "2026-06-15") == "2026-06-30"); // Missed periods give one instance.
    rule.repeat_count = 0;
    CHECK(Recurring::NextAfter(rule, "2026-01-31").empty());

    CHECK(Recurring::IsValidDate("2024-02-29"));
    CHECK(!Recurring::IsValidDate("2026-02-29"));
    CHECK(!Recurring::IsValidDate("2026-13-01"));
    CHECK(!Recurring::IsValidDate("2026-1-1"));
}

time_t LocalTime(int year, int month, int day, int hour, int minute) {
    tm local{};
    local.tm_year = year - 1900;
    local.tm_mon = month - 1;
    local.tm_mday = day;
    local.tm_hour = hour;
    local.tm_min = minute;
    local.tm_isdst = -1;
    return mktime(&local);
}

void TestWorkDayCutoffs() {
    using TaskUtils::GetLastResetCutoff;
    const time_t today = LocalTime(2026, 11, 6, 9, 15);
    const time_t yesterday = LocalTime(2026, 11, 5, 9, 15);
    CHECK(GetLastResetCutoff(LocalTime(2026, 11, 6, 9, 14), 9, 15) == yesterday);
    CHECK(GetLastResetCutoff(today, 9, 15) == today);
    CHECK(GetLastResetCutoff(LocalTime(2026, 11, 6, 23, 0), 9, 15) == today);
    // Over the daylight-saving changes (Oct 25 and Mar 29 in Hungary).
    CHECK(GetLastResetCutoff(LocalTime(2026, 10, 25, 8, 0), 9, 15) == LocalTime(2026, 10, 24, 9, 15));
    CHECK(GetLastResetCutoff(LocalTime(2026, 3, 29, 8, 0), 9, 15) == LocalTime(2026, 3, 28, 9, 15));
    CHECK(WorkHistory::DateKey(today) == "2026-11-06");
}

void TestVersionCompare() {
    using VersionHint::Compare;
    CHECK(Compare("1.16.2", "1.16.2") == 0);
    CHECK(Compare("1.9.0", "1.16.0") < 0); // Numbers, not text.
    CHECK(Compare("2.0", "1.99.99") > 0);
    CHECK(Compare("2.1", "2.1.0") == 0);
}

void TestSearchText() {
    using TaskUtils::ContainsText;
    CHECK(ContainsText(L"Bármi", L""));
    CHECK(ContainsText(L"Árvíztűrő TÜKÖRFÚRÓGÉP", L"tukorfurogep"));
    CHECK(ContainsText(L"feladat", L"FELADÁT"));
    CHECK(ContainsText(L"Csütörtök", L"c")); // Hungarian rules would hide the "c" of "cs".
    CHECK(ContainsText(L"Szerda", L"z"));
    CHECK(!ContainsText(L"alma", L"körte"));
    CHECK(!ContainsText(L"", L"a"));
}

const Task* FindById(const TaskStore& store, int id) {
    for (const Task& task : store.tasks) {
        if (task.id == id) return &task;
    }
    return nullptr;
}

void TestWorkHistoryFile() {
    // Days with nothing are left out; a manual-only day keeps a zero measured value.
    const WorkHistory::Records measured{ { "2026-11-02", 3600 }, { "2026-11-03", 0 }, { "2026-11-04", 120 } };
    const WorkHistory::Records manual{ { "2026-11-04", 600 }, { "2026-11-05", 900 } };
    TaskUtils::TextBuffer out;
    WorkHistory::WriteJson(out, measured, manual);
    WorkHistory::Records readMeasured;
    WorkHistory::Records readManual;
    WorkHistory::LoadFromJson("{\n" + out.str() + "}\n", readMeasured, readManual);
    const WorkHistory::Records expectedMeasured{ { "2026-11-02", 3600 }, { "2026-11-04", 120 }, { "2026-11-05", 0 } };
    CHECK(readMeasured == expectedMeasured);
    CHECK(readManual == manual);

    // Weeks newest first, the summary on the Sunday (or the newest day), manual time counted in.
    const WorkHistory::Records days{ { "2026-11-02", 3600 }, { "2026-11-08", 60 }, { "2026-11-09", 100 } };
    const WorkHistory::Records manualDays{ { "2026-11-04", 600 } };
    const auto entries = WorkHistory::GetDisplayEntries(days, manualDays, LocalTime(2026, 11, 10, 10, 0), 30);
    CHECK(entries.size() == 7);
    if (entries.size() != 7) return;
    CHECK(entries[0].isWeeklySummary && entries[0].label == L"46. hét" && entries[0].seconds == 130);
    CHECK(entries[1].label == L"11.10" && entries[1].isCurrentDay && entries[1].seconds == 30);
    CHECK(entries[2].label == L"11.09" && !entries[2].isCurrentDay);
    CHECK(entries[3].isWeeklySummary && entries[3].label == L"45. hét" && entries[3].seconds == 4260 &&
        entries[3].manualSeconds == 600);
    CHECK(entries[4].label == L"11.08");
    CHECK(entries[5].label == L"11.04" && entries[5].seconds == 600 && entries[5].manualSeconds == 600);
    CHECK(entries[6].label == L"11.02" && entries[6].seconds == 3600);
}

void TestWorkDayRollover() {
    CleanFiles();
    std::string previousDay;
    {
        TaskStore store; // First run: the work day starts now.
        const time_t cutoff = store.last_reset_time;
        CHECK(cutoff == TaskUtils::GetLastResetCutoff(time(nullptr), store.work_reset_hour, store.work_reset_minute));
        const time_t previous = TaskUtils::GetLastResetCutoff(cutoff - 1, store.work_reset_hour, store.work_reset_minute);
        previousDay = WorkHistory::DateKey(previous);
        store.last_reset_time = previous; // As if the app had been running since yesterday.
        store.work_seconds_today = 4321;
        store.CheckWorkReset();
        CHECK(store.work_history.size() == 1 && store.work_history[previousDay] == 4321);
        CHECK(store.work_seconds_today == 0 && store.last_reset_time == cutoff);
        store.work_seconds_today = 50;
        store.CheckWorkReset(); // Same work day: nothing moves.
        CHECK(store.work_history.size() == 1 && store.work_seconds_today == 50);
        store.Save();
    }
    {
        TaskStore store;
        CHECK(store.work_history.size() == 1 && store.work_history[previousDay] == 4321);
        CHECK(store.work_seconds_today == 50);
    }
}

void TestWorkHistoryCleanup() {
    CleanFiles();
    TaskStore store;
    store.work_history = { { "2026-01-05", 100 }, { "2026-02-01", 200 }, { "2026-03-01", 300 } };
    store.manual_work_history = { { "2026-01-10", 50 }, { "2026-03-02", 60 } };
    CHECK(store.CountWorkHistoryBefore("2026-02-01") == 2);

    // What UndoToast::PurgeWorkHistory keeps for the undo.
    const WorkHistory::Records keptMeasured(store.work_history.begin(), store.work_history.lower_bound("2026-02-01"));
    const WorkHistory::Records keptManual(store.manual_work_history.begin(),
        store.manual_work_history.lower_bound("2026-02-01"));
    store.PurgeWorkHistoryBefore("2026-02-01");
    CHECK(store.work_history.size() == 2 && store.work_history.count("2026-02-01") == 1);
    CHECK(store.manual_work_history.size() == 1 && store.manual_work_history.count("2026-03-02") == 1);
    CHECK(store.CountWorkHistoryBefore("2026-02-01") == 0);

    store.manual_work_history["2026-03-01"] = 999; // Changed after the delete: the undo must not overwrite it.
    store.RestoreWorkHistory(keptMeasured, keptManual);
    CHECK(store.work_history.size() == 3 && store.work_history["2026-01-05"] == 100);
    CHECK(store.manual_work_history.size() == 3 && store.manual_work_history["2026-01-10"] == 50);
    CHECK(store.manual_work_history["2026-03-01"] == 999);

    // Clearing the manual time of a day with no measured time removes the day.
    store.work_history["2026-04-01"] = 0;
    store.SetManualWork("2026-04-01", 120);
    CHECK(store.manual_work_history["2026-04-01"] == 120);
    store.SetManualWork("2026-04-01", 0);
    CHECK(store.manual_work_history.count("2026-04-01") == 0 && store.work_history.count("2026-04-01") == 0);
    store.SetManualWork("2026-03-01", 0); // Measured time stays.
    CHECK(store.manual_work_history.count("2026-03-01") == 0 && store.work_history["2026-03-01"] == 300);
}

int CountRecurringTasks(const TaskStore& store, const std::string& ruleUid) {
    int count = 0;
    for (const Task& task : store.tasks) {
        if (task.recurring_uid == ruleUid) ++count;
    }
    return count;
}

void TestRecurringTasks() {
    CleanFiles();
    std::string ruleUid;
    std::string nextDue;
    {
        TaskStore store;
        store.work_reset_hour = 0; // The work day is the calendar day, like SaveRecurringRule's "today".
        store.work_reset_minute = 0;
        const std::string today = Recurring::Today();

        RecurringRule rule;
        rule.text = L"Heti jelentés ő";
        rule.start_date = today;
        rule.repeat_count = 1;
        rule.repeat_unit = RepeatUnit::Week;
        std::vector<int> created = store.SaveRecurringRule(rule);
        CHECK(created.size() == 1 && store.recurring_rules.size() == 1);
        if (created.size() != 1 || store.recurring_rules.size() != 1) return;
        ruleUid = store.recurring_rules[0].uid;
        CHECK(!ruleUid.empty() && store.recurring_rules[0].next_due > today);
        const Task* task = FindById(store, created[0]);
        CHECK(task && task->text == rule.text && task->recurring_uid == ruleUid && !task->is_sync);
        CHECK(task && store.recurring_rules[0].last_task_uid == task->uid);

        CHECK(store.RunDueRecurring().empty()); // A restart on the same day creates nothing twice.

        // Due again while the last one is still open: no second copy, but the rule moves on.
        store.recurring_rules[0].next_due = "2020-01-01";
        CHECK(store.RunDueRecurring().empty());
        CHECK(store.recurring_rules[0].next_due > today);
        CHECK(CountRecurringTasks(store, ruleUid) == 1);

        // Finished: the next due day gives a new one, missed periods only one.
        store.ToggleCompleted(created[0]);
        store.recurring_rules[0].next_due = "2020-01-01";
        created = store.RunDueRecurring();
        CHECK(created.size() == 1 && CountRecurringTasks(store, ruleUid) == 2);
        nextDue = store.recurring_rules[0].next_due;

        // A one-off rule whose target file was removed: a local task, and the rule is not kept.
        RecurringRule once;
        once.text = L"Egyszeri";
        once.start_date = today;
        once.targets = { 42 };
        created = store.SaveRecurringRule(once);
        CHECK(created.size() == 1 && store.recurring_rules.size() == 1);
        task = created.empty() ? nullptr : FindById(store, created[0]);
        CHECK(task && task->text == L"Egyszeri" && !task->is_sync && task->recurring_uid.empty());

        // A later one-off waits.
        once.start_date = "2999-01-01";
        CHECK(store.SaveRecurringRule(once).empty());
        CHECK(store.recurring_rules.size() == 2 && store.recurring_rules[1].next_due == "2999-01-01");
        store.DeleteRecurringRule(store.recurring_rules[1].uid);
        CHECK(store.recurring_rules.size() == 1);
        store.Save();
    }
    {
        TaskStore store; // Startup runs the due rules: nothing is due.
        CHECK(store.recurring_rules.size() == 1);
        if (store.recurring_rules.empty()) return;
        const RecurringRule& rule = store.recurring_rules[0];
        CHECK(rule.uid == ruleUid && rule.text == L"Heti jelentés ő" && rule.next_due == nextDue);
        CHECK(rule.repeat_count == 1 && rule.repeat_unit == RepeatUnit::Week);
        CHECK(CountRecurringTasks(store, ruleUid) == 2);
    }
}

void TestSettingsRoundTrip() {
    CleanFiles();
    std::string saved;
    {
        TaskStore store;
        // Every saved value differs from its default, so a value that is not saved or not loaded shows.
        store.work_seconds_today = 777;
        store.work_reset_hour = 7;
        store.work_reset_minute = 45;
        store.last_reset_time = TaskUtils::GetLastResetCutoff(time(nullptr), 7, 45); // No rollover on load.
        store.timer_work_minutes = 25.5;
        store.timer_rest_minutes = 4.5;
        store.timer_repetitions = 3;
        store.timer_2020_enabled = true;
        store.timer_strict_mode = true;
        store.timer_start_with_app = true;
        store.sounds_enabled = false;
        for (int i = 0; i < Hotkeys::ACTION_COUNT; ++i) store.hotkeys[i] = Hotkeys::Make(MOD_ALT, 'A' + i);
        store.work_tracking_enabled = false;
        store.update_check_enabled = false;
        store.laura_enabled = false;
        store.update_last_check = 1700000000;
        store.update_failures = 2;
        store.update_last_failure = 1700000500;
        store.update_latest_version = "9.8.7";
        store.update_dismissed_date = "2026-11-01";
        store.excluded_window_keywords = L"Zoom, \"Teams\" ő";
        store.pin_buttons = "drag,pin,-timer,add,focus,-mute";
        store.pin_list_mode = 0;
        store.pin_list_count = 7;
        store.cleanup_days = 30;
        store.cleanup_weeks = 12;
        store.theme_mode = ThemeMode::Pink;
        store.work_history = { { "2026-11-02", 3600 } };
        store.manual_work_history = { { "2026-11-03", 600 } };
        SyncSource source = Source(store.next_source_id++, SyncPath(L"a.json"));
        source.name = L"Közös \"lista\"";
        source.isDefault = true;
        store.syncSources = { source };
        RecurringRule rule;
        rule.uid = "szabály-1";
        rule.text = L"Havi zárás";
        rule.start_date = "2999-01-31";
        rule.next_due = "2999-01-31";
        rule.repeat_count = 2;
        rule.repeat_unit = RepeatUnit::Month;
        rule.targets = { source.id };
        rule.last_task_uid = "előző";
        rule.created_at = 1600000000;
        store.recurring_rules = { rule };
        const int id = store.Add(L"Feladat \"idézőjellel\" és \\ jellel", {});
        store.CycleMarker(id);
        store.Save();
        saved = ReadText(LocalPath());
    }
    {
        TaskStore store;
        CHECK(!store.localLoadFailed);
        CHECK(store.work_seconds_today == 777);
        CHECK(store.work_reset_hour == 7 && store.work_reset_minute == 45);
        CHECK(store.timer_work_minutes == 25.5 && store.timer_rest_minutes == 4.5 && store.timer_repetitions == 3);
        CHECK(store.timer_2020_enabled && store.timer_strict_mode && store.timer_start_with_app);
        CHECK(!store.sounds_enabled && !store.work_tracking_enabled);
        for (int i = 0; i < Hotkeys::ACTION_COUNT; ++i) CHECK(store.hotkeys[i] == Hotkeys::Make(MOD_ALT, 'A' + i));
        CHECK(!store.update_check_enabled && store.update_last_check == 1700000000 && store.update_failures == 2);
        CHECK(!store.laura_enabled);
        CHECK(store.update_last_failure == 1700000500 && store.update_latest_version == "9.8.7");
        CHECK(store.update_dismissed_date == "2026-11-01");
        CHECK(store.excluded_window_keywords == L"Zoom, \"Teams\" ő");
        CHECK(store.pin_buttons == "drag,pin,-timer,add,focus,-mute");
        CHECK(store.pin_list_mode == 0 && store.pin_list_count == 7);
        CHECK(store.cleanup_days == 30 && store.cleanup_weeks == 12);
        CHECK(store.theme_mode == ThemeMode::Pink);
        // A manual-only day comes back with a zero measured value.
        const WorkHistory::Records expectedMeasured{ { "2026-11-02", 3600 }, { "2026-11-03", 0 } };
        CHECK(store.work_history == expectedMeasured);
        CHECK(store.manual_work_history.size() == 1 && store.manual_work_history["2026-11-03"] == 600);
        CHECK(store.syncSources.size() == 1);
        if (!store.syncSources.empty()) {
            const SyncSource& source = store.syncSources[0];
            CHECK(source.name == L"Közös \"lista\"" && source.path == SyncPath(L"a.json") && source.isDefault);
            CHECK(store.next_source_id == source.id + 1);
        }
        CHECK(store.recurring_rules.size() == 1);
        if (!store.recurring_rules.empty()) {
            const RecurringRule& rule = store.recurring_rules[0];
            CHECK(rule.uid == "szabály-1" && rule.text == L"Havi zárás" && rule.start_date == "2999-01-31");
            CHECK(rule.next_due == "2999-01-31" && rule.repeat_count == 2 && rule.repeat_unit == RepeatUnit::Month);
            CHECK(rule.targets.size() == 1 && rule.last_task_uid == "előző" && rule.created_at == 1600000000);
        }
        CHECK(store.tasks.size() == 1);
        if (!store.tasks.empty()) {
            CHECK(store.tasks[0].text == L"Feladat \"idézőjellel\" és \\ jellel");
            CHECK(store.tasks[0].marker != TaskMarker::None);
        }
        // Loading then saving changes nothing: every saved value is also read back.
        store.SaveLocal();
        CHECK(ReadText(LocalPath()) == saved);
    }
}

void TestAssignment() {
    CleanFiles();
    const std::wstring path = SyncPath(L"a.json");
    WriteSync(path, { Record("x", L"közös feladat", 100) });
    TaskStore store;
    store.SetSyncSources({ Source(1, path) });
    const int id = FindByUid(store, "x")->id;

    // Taking it and giving it back both reach the shared file.
    CHECK(store.ToggleAssignment(id, L"én").empty());
    CHECK(FindByUid(store, "x")->assignee == L"én" && ReadSync(path)[0].assignee == L"én");
    CHECK(store.ToggleAssignment(id, L"én").empty());
    CHECK(FindByUid(store, "x")->assignee.empty() && ReadSync(path)[0].assignee.empty());

    // A colleague took it in the meantime: the toggle names them and overwrites nothing.
    std::vector<SyncRecord> records = ReadSync(path);
    records[0].assignee = L"kolléga";
    records[0].updated_at += 100;
    WriteSync(path, records);
    CHECK(store.ToggleAssignment(id, L"én") == L"kolléga");
    CHECK(FindByUid(store, "x")->assignee == L"kolléga" && ReadSync(path)[0].assignee == L"kolléga");

    // A local task has no one to share it with.
    const int localId = store.Add(L"saját", {});
    CHECK(store.ToggleAssignment(localId, L"én").empty());
    for (const Task& task : store.tasks) {
        if (task.id == localId) CHECK(task.assignee.empty());
    }
}

// The one-second IDT_WORK_TIMER of MainWindow on the hand-driven clock, without the window.
std::wstring g_foregroundTitle;
int g_titleReads = 0;

void ReadForegroundTitle(std::wstring& title) {
    ++g_titleReads;
    title = g_foregroundTitle;
}

struct SimulatedApp {
    TaskStore& store;
    IntervalTimer timer;
    WorkTick work;
    WorkTick::Status status;
    WorkTick::Result last;
    std::wstring excludedName;
    bool userPresent = true; // Gives input every second.
    int phaseChanges = 0;

    explicit SimulatedApp(TaskStore& s) : store(s) {
        Clock::testMs = 100000000;
        Clock::testLastInputTick = Clock::InputTick();
        g_cues.clear();
        g_foregroundTitle = L"Visual Studio";
    }

    void Second() {
        Clock::testMs += 1000;
        if (userPresent) Clock::testLastInputTick = Clock::InputTick();
        if (WorkTick::AdvanceTimer(store, timer)) {
            ++phaseChanges;
            bool stop = false;
            if (WorkTick::StrictStopAfterPhaseChange(store, timer, stop)) status.measurementStopped = stop;
        }
        last = work.Run(store, timer, status, ReadForegroundTitle, excludedName);
    }

    void Seconds(int count) {
        for (int i = 0; i < count; ++i) Second();
    }
};

int SavedWorkSeconds() {
    TaskStore saved;
    return saved.work_seconds_today;
}

void TestWorkTimeCounting() {
    CleanFiles();
    TaskStore store;
    store.excluded_window_keywords = ExcludedApps::Normalize(L" YouTube ,, discord,  ");
    CHECK(store.excluded_window_keywords == L"YouTube, discord");
    SimulatedApp app(store);
    const int& work = store.work_seconds_today;

    app.Seconds(60);
    CHECK(work == 60 && app.last.countWork && app.excludedName.empty());

    // An excluded window pauses counting and shows the keyword as typed.
    g_foregroundTitle = L"Zene - youtube - Chrome";
    app.Seconds(30);
    CHECK(work == 60 && !app.last.countWork && app.excludedName == L"YouTube");
    // Focus mode counts anyway and does not even read the title.
    app.status.focusMode = true;
    int reads = g_titleReads;
    app.Seconds(10);
    CHECK(work == 70 && g_titleReads == reads && app.excludedName.empty());
    app.status.focusMode = false;
    // A changed keyword list applies from the next second.
    g_foregroundTitle = L"Visual Studio";
    store.excluded_window_keywords = L"studio";
    app.Second();
    CHECK(work == 70 && app.excludedName == L"studio");
    store.excluded_window_keywords = L"YouTube";
    app.Second();
    CHECK(work == 71);

    // Away: counting goes on until 2 minutes without input, then stops and saves once.
    app.userPresent = false;
    app.Seconds(200);
    CHECK(work == 71 + 119 && app.last.idlePaused && !app.last.countWork);
    CHECK(SavedWorkSeconds() == 190);
    // Focus mode counts even without input (reading, a call).
    app.status.focusMode = true;
    app.Seconds(5);
    CHECK(work == 195);
    app.status.focusMode = false;
    // Back: the first second counts again.
    app.userPresent = true;
    app.Second();
    CHECK(work == 196 && app.last.countWork && !app.last.idlePaused);

    // A locked session and "Mérés leállítása" stop it.
    app.status.sessionLocked = true;
    app.Seconds(10);
    app.status.sessionLocked = false;
    app.status.measurementStopped = true;
    app.Seconds(10);
    app.status.measurementStopped = false;
    CHECK(work == 196);

    // A late timer counts the real elapsed time, a stall at most 5 seconds.
    Clock::testMs += 2500;
    app.Second();
    CHECK(work == 199);
    Clock::testMs += 60000;
    app.Second();
    CHECK(work == 204);

    // Every 10 counted minutes are saved.
    app.Seconds(395);
    CHECK(work == 599 && SavedWorkSeconds() == 190);
    app.Second();
    CHECK(work == 600 && SavedWorkSeconds() == 600 && app.last.historyChanged);

    // Measurement switched off: nothing counts and no title is read.
    store.work_tracking_enabled = false;
    reads = g_titleReads;
    app.Seconds(10);
    CHECK(work == 600 && !app.last.countWork && g_titleReads == reads);
}

void TestIntervalTimerCycle() {
    CleanFiles();
    TaskStore store;
    SimulatedApp app(store);
    IntervalTimer& timer = app.timer;
    timer.Start(60, 30, 2, false);
    CHECK(timer.IsRunning() && timer.IsWorkPhase() && timer.SecondsRemaining() == 60);
    CHECK(timer.CurrentRepetition() == 1 && timer.RepetitionCount() == 2);

    app.Seconds(59);
    CHECK(timer.SecondsRemaining() == 1 && app.phaseChanges == 0);
    app.Second();
    CHECK(app.phaseChanges == 1 && !timer.IsWorkPhase() && timer.SecondsRemaining() == 30);
    CHECK(g_cues == std::vector<std::string>({ "rest" }));
    app.Seconds(30);
    CHECK(timer.IsWorkPhase() && timer.CurrentRepetition() == 2 && g_cues.back() == "work");

    // A pause keeps the remaining time, however long it lasts.
    app.Seconds(20);
    CHECK(timer.SecondsRemaining() == 40);
    timer.Pause();
    app.Seconds(300);
    CHECK(timer.IsPaused() && timer.IsActive() && timer.SecondsRemaining() == 40 && app.phaseChanges == 2);
    timer.Resume();
    app.Seconds(39);
    CHECK(timer.IsWorkPhase() && timer.SecondsRemaining() == 1);
    app.Second();
    CHECK(!timer.IsWorkPhase() && app.phaseChanges == 3);

    // The last rest ends the run.
    app.Seconds(30);
    CHECK(!timer.IsActive() && app.phaseChanges == 4 && g_cues.size() == 4 && g_cues.back() == "work");
    app.Seconds(100);
    CHECK(app.phaseChanges == 4 && timer.SecondsRemaining() == 0);

    // A manual step skips to the next phase.
    timer.Start(600, 60, 2, false);
    CHECK(timer.AdvancePhase() && !timer.IsWorkPhase() && timer.SecondsRemaining() == 60);
}

void TestIntervalTimerWaitsForUser() {
    CleanFiles();
    TaskStore store;
    SimulatedApp app(store);
    IntervalTimer& timer = app.timer;
    timer.Start(600, 300, 3, false);

    // Without STRICT MODE an idle user does not stop the work countdown.
    app.userPresent = false;
    app.Seconds(300);
    CHECK(timer.IsRunning() && timer.SecondsRemaining() == 300);
    app.Seconds(300);
    CHECK(!timer.IsWorkPhase() && timer.IsRunning());

    // The rest ends while the user is away: the next work phase waits for them.
    app.Seconds(300);
    CHECK(timer.IsWorkPhase() && timer.IsAwaitingActivity() && !timer.IsRunning() && timer.IsActive());
    CHECK(timer.SecondsRemaining() == 600);
    app.Seconds(100);
    CHECK(timer.IsAwaitingActivity() && timer.SecondsRemaining() == 600);

    // Back: the full work phase starts now.
    app.userPresent = true;
    app.Second();
    CHECK(app.last.timerResumed && timer.IsRunning() && timer.SecondsRemaining() == 600);
    CHECK(timer.CurrentRepetition() == 2);
    app.Seconds(600);
    CHECK(!timer.IsWorkPhase());

    // A present user gets the next work phase straight away.
    app.Seconds(300);
    CHECK(timer.IsWorkPhase() && timer.IsRunning() && !timer.IsAwaitingActivity());
}

void TestStrictMode() {
    CleanFiles();
    TaskStore store;
    store.timer_strict_mode = true;
    SimulatedApp app(store);
    IntervalTimer& timer = app.timer;
    const int& work = store.work_seconds_today;
    timer.Start(600, 60, 2, false);

    // The rest phase stops work measurement, the next work phase restarts it.
    app.Seconds(600);
    CHECK(!timer.IsWorkPhase() && app.status.measurementStopped);
    const int workBeforeRest = work;
    app.Seconds(59);
    CHECK(work == workBeforeRest && !app.last.countWork);
    app.Second();
    CHECK(timer.IsWorkPhase() && !app.status.measurementStopped && work == workBeforeRest + 1);

    // Idle during work pauses the countdown too, and input continues it.
    app.userPresent = false;
    app.Seconds(119);
    CHECK(timer.IsRunning() && timer.SecondsRemaining() == 481);
    app.Second();
    CHECK(timer.IsPaused() && timer.SecondsRemaining() == 480);
    app.Seconds(100);
    CHECK(timer.IsPaused() && timer.SecondsRemaining() == 480);
    app.userPresent = true;
    app.Second();
    CHECK(timer.IsRunning() && timer.SecondsRemaining() == 480);

    // So do "Mérés leállítása" and a locked session.
    app.status.measurementStopped = true;
    app.Seconds(30);
    CHECK(timer.IsPaused() && timer.SecondsRemaining() == 479);
    app.status.measurementStopped = false;
    app.status.sessionLocked = true;
    app.Seconds(30);
    CHECK(timer.IsPaused() && timer.SecondsRemaining() == 479);
    app.status.sessionLocked = false;
    app.Second();
    CHECK(timer.IsRunning() && timer.SecondsRemaining() == 479);

    // With work measurement off, STRICT MODE does nothing.
    store.work_tracking_enabled = false;
    app.status.measurementStopped = true;
    app.Seconds(10);
    CHECK(timer.IsRunning() && timer.SecondsRemaining() == 469);
}

void TestVisionBreak() {
    CleanFiles();
    TaskStore store;
    store.timer_2020_enabled = true;
    SimulatedApp app(store);
    IntervalTimer& timer = app.timer;

    // 20 minutes of screen time, then a 25-second break.
    app.Seconds(1200);
    CHECK(!timer.IsVisionBreakActive() && g_cues.empty());
    app.Second();
    CHECK(timer.IsVisionBreakActive() && g_cues == std::vector<std::string>({ "vision start" }));
    app.userPresent = false; // Looks away.
    app.Seconds(24);
    CHECK(timer.IsVisionBreakActive());
    app.Second();
    CHECK(!timer.IsVisionBreakActive() && g_cues.back() == "vision end");

    // The next 20 minutes start only with the first input after the break.
    app.Seconds(60);
    app.userPresent = true;
    app.Second();
    app.Seconds(1199);
    CHECK(!timer.IsVisionBreakActive());
    app.Second();
    CHECK(timer.IsVisionBreakActive());
    app.Seconds(25);
    CHECK(!timer.IsVisionBreakActive());

    // Time away from the screen (locked) does not count toward the 20 minutes.
    app.Second(); // The input that restarts the 20 minutes.
    app.Seconds(600);
    app.status.sessionLocked = true;
    app.Seconds(900);
    app.status.sessionLocked = false;
    app.Seconds(599);
    CHECK(!timer.IsVisionBreakActive());
    app.Second();
    CHECK(timer.IsVisionBreakActive());

    // A manual interval-timer step restarts the full 20 minutes.
    timer.Start(3600, 60, 1, true);
    timer.AdvancePhase();
    CHECK(!timer.IsVisionBreakActive());
    app.Seconds(1199);
    CHECK(!timer.IsVisionBreakActive());
    app.Second();
    CHECK(timer.IsVisionBreakActive());

    // Switching it off ends a running break.
    store.timer_2020_enabled = false;
    app.Second();
    CHECK(!timer.IsVisionBreakActive() && g_cues.back() == "vision end");
    app.Seconds(1500);
    CHECK(!timer.IsVisionBreakActive());
}

void Run(const char* name, const std::function<void()>& test) {
    const int failuresBefore = g_failures;
    test();
    std::printf("%s %s\n", g_failures == failuresBefore ? "ok    " : "FAILED", name);
}
}

// The PIN strip's pause and next buttons through a whole run: what they show and which timer-view
// button they press (carried out here with the same timer calls as TimerView::HandleCommand).
void TestPinTimerButtons() {
    CleanFiles();
    TaskStore store;
    SimulatedApp app(store);
    IntervalTimer& timer = app.timer;
    using PinTimerButtons::Icon;
    using PinTimerButtons::Press;
    auto icons = [&timer](Icon pause, Icon next) {
        return PinTimerButtons::IconFor(false, timer) == pause && PinTimerButtons::IconFor(true, timer) == next;
    };
    auto click = [&timer](bool nextButton) {
        const Press press = PinTimerButtons::PressFor(nextButton, timer);
        if (press == Press::Start) timer.Start(60, 30, 1, false);
        else if (press == Press::PauseResume) timer.IsPaused() ? timer.Resume() : timer.Pause();
        else timer.AdvancePhase();
        return press;
    };

    // Not started: both buttons show restart and press Start.
    CHECK(icons(Icon::Restart, Icon::Restart));
    CHECK(PinTimerButtons::PressFor(true, timer) == Press::Start);
    CHECK(click(false) == Press::Start && timer.IsRunning() && timer.IsWorkPhase());
    CHECK(icons(Icon::Play, Icon::Next));

    // Pause / continue.
    CHECK(click(false) == Press::PauseResume && timer.IsPaused() && icons(Icon::Pause, Icon::Next));
    app.Seconds(10);
    CHECK(timer.SecondsRemaining() == 60);
    CHECK(click(false) == Press::PauseResume && !timer.IsPaused() && icons(Icon::Play, Icon::Next));
    // A STRICT MODE pause shows as paused too.
    timer.PauseForMeasurement();
    CHECK(icons(Icon::Pause, Icon::Next));
    timer.ResumeFromMeasurement();

    // Next steps to the rest phase; at the end of the run both switch back to restart.
    CHECK(click(true) == Press::Next && !timer.IsWorkPhase() && timer.SecondsRemaining() == 30);
    CHECK(icons(Icon::Play, Icon::Next));
    app.Seconds(30);
    CHECK(!timer.IsActive() && icons(Icon::Restart, Icon::Restart));
    CHECK(click(true) == Press::Start && timer.IsRunning() && timer.IsWorkPhase());

    // A stopped timer shows restart as well.
    timer.Stop();
    CHECK(icons(Icon::Restart, Icon::Restart) && PinTimerButtons::PressFor(false, timer) == Press::Start);
}

// Laura link: handshake in either start order, crash versus Bye, switching off and on, and the
// messages both ways.
void TestLauraLink() {
    using namespace LauraProtocol;
    const HWND laura = reinterpret_cast<HWND>(static_cast<INT_PTR>(0x100));
    const HWND restarted = reinterpret_cast<HWND>(static_cast<INT_PTR>(0x200));
    Session session(true);
    CHECK(!session.Detected() && !session.Connected());

    // Laura starts after the TaskManager: her Hello gets an answer and the catalog.
    Actions a = session.OnHello(laura);
    CHECK(a.watch && !a.unwatch && a.sendAck && a.sendCatalog && a.changed && a.found);
    CHECK(session.Connected() && session.Peer() == laura);
    a = session.OnHello(laura); // She says Hello again (her matcher restarted): no new watch.
    CHECK(!a.watch && a.sendAck && a.sendCatalog && !a.changed && !a.found);

    // She crashes: the live link is reported lost.
    a = session.OnPeerExited();
    CHECK(a.unwatch && a.lost && a.changed && !session.Detected());
    CHECK(!session.OnPeerExited().changed);

    // The TaskManager starts after Laura: its Hello is answered with HelloAck.
    a = session.OnHelloAck(restarted);
    CHECK(a.watch && !a.sendAck && a.sendCatalog && session.Connected() && !a.found); // Laura tells this one.

    // Switched off in Settings: Bye, still detected, her Hello is not answered.
    a = session.SetEnabled(false);
    CHECK(a.sendBye && a.changed && session.Detected() && !session.Connected());
    a = session.OnHello(restarted);
    CHECK(!a.sendAck && !a.sendCatalog && !a.found && !session.Connected());
    CHECK(session.OnHelloAck(restarted).sendBye); // A late answer to an earlier Hello.
    // Closing Laura now is no lost link.
    a = session.OnPeerExited();
    CHECK(!a.lost && a.changed && !session.Detected());

    // Switched on again while she runs: Hello, connected once she answers.
    session.OnHello(laura);
    a = session.SetEnabled(true);
    CHECK(a.sendHello && !session.Connected());
    a = session.OnHelloAck(laura);
    CHECK(a.sendCatalog && a.changed && session.Connected());
    // Bye from someone else is ignored; hers closes quietly.
    CHECK(!session.OnBye(restarted).changed && session.Connected());
    a = session.OnBye(laura);
    CHECK(a.unwatch && !a.lost && !session.Detected());
    CHECK(Session(false).SetEnabled(false).changed == false);

    // Every catalog command is known and described.
    std::vector<std::string> commands;
    CHECK(JsonLite::ReadObjectArray(Catalog(), "parancsok", commands));
    CHECK(commands.size() == 18);
    for (const std::string& command : commands) {
        std::string id, description;
        CHECK(JsonLite::GetString(command, "id", id) && Lookup(id) != Command::Unknown);
        CHECK(JsonLite::GetString(command, "leiras", description) && !description.empty());
    }
    std::string type;
    CHECK(JsonLite::GetString(Catalog(), "t", type) && type == "katalogus");

    // Laura's command, with an escaped accented text.
    Request request;
    CHECK(ParseRequest(u8R"({"t": "parancs", "req": 7, "id": "feladat.uj", "szoveg": "Számla \"befizetés\""})", request));
    CHECK(request.req == 7 && request.command == Command::TaskAdd && request.text == L"Számla \"befizetés\"");
    CHECK(ParseRequest(R"({"t": "parancs", "req": 8, "id": "ismeretlen"})", request));
    CHECK(request.command == Command::Unknown && request.text.empty());
    CHECK(!ParseRequest(R"({"t": "valasz", "req": 1})", request));

    // "Pipáld ki a …": the open task the spoken name means.
    auto task = [](int id, const wchar_t* text, bool completed = false) {
        Task t;
        t.id = id;
        t.text = text;
        t.completed = completed;
        return t;
    };
    const std::vector<Task> tasks = { task(1, L"Számla befizetés"), task(2, L"Tej vásárlás"),
        task(3, L"Számla kiküldése Péternek"), task(4, L"Régi jelentés", true), task(5, L"Jelentés megírása") };
    CHECK(FindOpenTask(tasks, L"a számla befizetést") == 1);   // Ending and accents do not matter.
    CHECK(FindOpenTask(tasks, L"SZAMLA BEFIZETES feladatot") == 1);
    CHECK(FindOpenTask(tasks, L"tejet") == 0);                 // Shorter than 4 letters: whole word only.
    CHECK(FindOpenTask(tasks, L"tej vásárlást") == 2);
    CHECK(FindOpenTask(tasks, L"kiküldés Péternek") == 3);
    CHECK(FindOpenTask(tasks, L"számla") == AMBIGUOUS_TASK);   // Two tasks, neither named in full.
    CHECK(FindOpenTask(tasks, L"jelentést") == 5);              // The finished one does not count.
    CHECK(FindOpenTask(tasks, L"porszívózás") == 0);
    CHECK(FindOpenTask(tasks, L"") == 0);

    // Reading the list out: most important first, a group's neighbours said together.
    const std::vector<ListedTask> listed = { { L"Számla", L"", L"" }, { L"Terv", L"Munka", L"Anna" },
        { L"Teszt", L"Munka", L"" }, { L"Tej", L"", L"" }, { L"Hívás", L"Otthon", L"" }, { L"Posta", L"", L"" } };
    CHECK(DescribeTasks({}, 5) == L"Nincs nyitott feladatod.");
    CHECK(DescribeTasks({ { L"Tej", L"", L"" } }, 5) == L"Egy feladatod van: Tej.");
    CHECK(DescribeTasks(listed, 4) ==
        L"6 feladatod van, az első 4 a legfontosabbal kezdve: Számla, a Munka csoportban Terv és Teszt, majd Tej.");
    CHECK(DescribeTasks(listed, 9) == L"6 feladatod van, a legfontosabbal kezdve: Számla, a Munka csoportban "
        L"Terv és Teszt, Tej, az Otthon csoportban Hívás, majd Posta.");
    CHECK(DescribeFirstTask(listed) == L"A legfontosabb feladatod: Számla.");
    CHECK(DescribeFirstTask({ listed[4] }) == L"A legfontosabb feladatod az Otthon csoportban: Hívás.");
    // For Claude: the whole list in order, groups as blocks, the assignee where there is one.
    const std::string listJson = TasksJson(listed);
    long long total = 0;
    std::vector<std::string> items;
    CHECK(JsonLite::GetInt64(listJson, "osszes", total) && total == 6);
    CHECK(JsonLite::ReadObjectArray(listJson, "elemek", items) && items.size() == 5);
    std::wstring value;
    CHECK(items.size() == 5 && JsonLite::GetWideString(items[0], "szoveg", value) && value == L"Számla");
    CHECK(items.size() == 5 && JsonLite::GetWideString(items[1], "csoport", value) && value == L"Munka");
    std::vector<std::string> groupTasks;
    CHECK(items.size() == 5 && JsonLite::ReadObjectArray(items[1], "feladatok", groupTasks) && groupTasks.size() == 2);
    CHECK(groupTasks.size() == 2 && JsonLite::GetWideString(groupTasks[0], "felelos", value) && value == L"Anna");
    CHECK(groupTasks.size() == 2 && !JsonLite::GetWideString(groupTasks[1], "felelos", value));
    const std::string withData = BuildReply(3, true, L"Kész.", listJson);
    std::vector<std::string> sent;
    CHECK(JsonLite::ReadObjectArray(withData, "elemek", sent) && sent.size() == 5);

    // The list also brings the scheduled tasks, which are not active yet.
    const std::string withScheduled = TasksJson({ listed[0] },
        { { L"Fogorvos", "2026-11-24", 0, RepeatUnit::Day }, { L"Jelentés", "2026-11-23", 1, RepeatUnit::Week } });
    std::vector<std::string> scheduledItems;
    CHECK(JsonLite::ReadObjectArray(withScheduled, "utemezett", scheduledItems) && scheduledItems.size() == 2);
    std::string due;
    CHECK(scheduledItems.size() == 2 && JsonLite::GetString(scheduledItems[0], "megjelenik", due) && due == "2026-11-24");
    CHECK(scheduledItems.size() == 2 && JsonLite::GetWideString(scheduledItems[0], "ismetles", value) && value == L"egyszeri");
    CHECK(scheduledItems.size() == 2 && JsonLite::GetWideString(scheduledItems[1], "ismetles", value) && value == L"hetente");
    CHECK(TasksJson({ listed[0] }).find("utemezett") == std::string::npos);

    // Editing, work time by days and the manual entry arrive with their own fields.
    Request more;
    CHECK(ParseRequest(u8R"({"t": "parancs", "id": "feladat.modosit", "szoveg": "tej", "uj_szoveg": "Kakaó"})", more));
    CHECK(more.command == Command::TaskEdit && more.text == L"tej" && more.newText == L"Kakaó");
    CHECK(ParseRequest(R"({"t": "parancs", "id": "munkaido.napok", "tol": "2026-11-16", "ig": "2026-11-20"})", more));
    CHECK(more.command == Command::WorkDays && more.from == "2026-11-16" && more.to == "2026-11-20");
    CHECK(ParseRequest(R"({"t": "parancs", "id": "munkaido.kezi", "perc": -30})", more));
    CHECK(more.command == Command::ManualWork && more.minutes == -30 && more.date.empty());

    // Work time of a range: only days with work, today from the live counter, with the weekday.
    const WorkHistory::Records measured = { { "2026-11-13", 7200 }, { "2026-11-16", 3600 }, { "2026-11-18", 0 },
        { "2026-11-20", 60 } };
    const WorkHistory::Records manual = { { "2026-11-17", 1800 }, { "2026-11-21", 600 } };
    const std::string daysJson = WorkDaysJson(measured, manual, "2026-11-20", 5400, "2026-11-16", "2026-11-20", 24600);
    std::vector<std::string> days;
    long long dayMinutes = 0;
    CHECK(JsonLite::ReadObjectArray(daysJson, "napok", days) && days.size() == 3);
    std::string dayName;
    CHECK(days.size() == 3 && JsonLite::GetString(days[0], "nap", dayName) && dayName == u8"hétfő");
    CHECK(days.size() == 3 && JsonLite::GetInt64(days[1], "kezi_perc", dayMinutes) && dayMinutes == 30);
    CHECK(days.size() == 3 && JsonLite::GetInt64(days[2], "mert_perc", dayMinutes) && dayMinutes == 90); // Live, not 1.
    CHECK(JsonLite::GetInt64(daysJson, "teljes_perc", dayMinutes) && dayMinutes == 60 + 30 + 90);
    CHECK(JsonLite::GetInt64(daysJson, "napi_cel_perc", dayMinutes) && dayMinutes == 410);

    // Deleting: only a task written in the last 24 hours.
    const time_t now = 1800000000;
    CHECK(ParseRequest(R"({"t": "parancs", "id": "feladat.torol", "szoveg": "tej"})", more));
    CHECK(more.command == Command::TaskDelete && more.text == L"tej");
    CHECK(CheckDelete(now - 60, now).empty());
    CHECK(CheckDelete(now - DELETE_WINDOW_SECONDS, now).empty());
    CHECK(!CheckDelete(now - DELETE_WINDOW_SECONDS - 1, now).empty());
    CHECK(!CheckDelete(0, now).empty()); // Unknown age: kept.

    // Announcements: always something to say, and the wording changes with the random number.
    for (int kind = 0; kind <= static_cast<int>(Announce::WorkNinety); ++kind) {
        std::set<std::wstring> wordings;
        for (unsigned random = 0; random < 50; ++random) {
            const std::wstring text = AnnouncementText(static_cast<Announce>(kind), random, 10);
            CHECK(!text.empty() && text.find(L'{') == std::wstring::npos);
            wordings.insert(text);
        }
        CHECK(wordings.size() > 1);
    }

    // Spoken bits.
    CHECK(FormatDuration(45) == L"45 perc" && FormatDuration(120) == L"2 óra" && FormatDuration(80) == L"1 óra 20 perc");
    CHECK(CrossesShare(22139, 22140, 24600, 90) && !CrossesShare(22140, 22141, 24600, 90));
    CHECK(!CrossesShare(20000, 21000, 24600, 90) && !CrossesShare(0, 0, 0, 90));
    CHECK(NewTaskSentence(L"Anna", L"Számla") == L"Anna új feladatot írt ki: Számla.");
    CHECK(NewTaskSentence(L"", L"Számla") == L"Új közös feladat: Számla.");

    // Finished tasks for Claude: when, shared or not and where, newest first, at most `limit`.
    tm finishedAt{};
    finishedAt.tm_year = 2026 - 1900;
    finishedAt.tm_mon = 10;
    finishedAt.tm_mday = 16;
    finishedAt.tm_hour = 14;
    finishedAt.tm_min = 32;
    finishedAt.tm_isdst = -1;
    const std::vector<DoneTask> done = {
        { L"Riport", mktime(&finishedAt), true, L"Közös \"lista\"", L"Anna", L"Béla" },
        { L"Tej", mktime(&finishedAt) - 86400, false, L"", L"", L"" },
        { L"Régi", 1, false, L"", L"", L"" } };
    const std::string doneJson = DoneTasksJson(done, 2);
    std::vector<std::string> doneItems;
    CHECK(JsonLite::GetInt64(doneJson, "osszes", total) && total == 3);
    CHECK(JsonLite::ReadObjectArray(doneJson, "elemek", doneItems) && doneItems.size() == 2);
    std::string when;
    bool shared = false;
    CHECK(doneItems.size() == 2 && JsonLite::GetString(doneItems[0], "kesz", when) && when == "2026-11-16 14:32");
    CHECK(doneItems.size() == 2 && JsonLite::GetBool(doneItems[0], "kozos", shared) && shared);
    CHECK(doneItems.size() == 2 && JsonLite::GetWideString(doneItems[0], "fajl", value) && value == L"Közös \"lista\"");
    CHECK(doneItems.size() == 2 && JsonLite::GetWideString(doneItems[0], "modositotta", value) && value == L"Béla");
    CHECK(doneItems.size() == 2 && JsonLite::GetBool(doneItems[1], "kozos", shared) && !shared);
    CHECK(doneItems.size() == 2 && !JsonLite::GetWideString(doneItems[1], "fajl", value));
    CHECK(doneItems.size() == 2 && JsonLite::GetString(doneItems[1], "kesz", when) && when == "2026-11-15 14:32");

    // A task for a later day, repeating only when asked.
    Request scheduled;
    CHECK(ParseRequest(u8R"({"t": "parancs", "req": 9, "id": "feladat.idozit", "szoveg": "Számla", "datum": "2026-12-03", "ismetles": 2, "egyseg": "het"})", scheduled));
    CHECK(scheduled.command == Command::TaskSchedule && scheduled.date == "2026-12-03");
    CHECK(scheduled.repeatCount == 2 && scheduled.repeatUnit == RepeatUnit::Week);
    CHECK(CheckSchedule(scheduled, "2026-11-17").empty());
    CHECK(CheckSchedule(scheduled, "2026-12-03").empty());               // Today is fine: it appears at once.
    CHECK(CheckSchedule(scheduled, "2026-12-04") == L"Ez a nap már elmúlt.");
    CHECK(DescribeSchedule(scheduled.text, scheduled.date, 2, RepeatUnit::Week) == L"Ütemeztem: Számla, 2026.12.03., 2 hetente.");
    CHECK(DescribeSchedule(L"Zárás", "2027-01-31", 1, RepeatUnit::Month) == L"Ütemeztem: Zárás, 2027.01.31., havonta.");
    CHECK(DescribeSchedule(L"Fogorvos", "2026-11-20", 0, RepeatUnit::Day) == L"Ütemeztem: Fogorvos, 2026.11.20.");
    CHECK(ParseRequest(R"({"t": "parancs", "id": "feladat.idozit", "szoveg": "x", "datum": "2026-02-30"})", scheduled));
    CHECK(scheduled.repeatCount == 0 && scheduled.repeatUnit == RepeatUnit::Day);
    CHECK(CheckSchedule(scheduled, "2026-01-01") == L"Nem értem a dátumot.");
    CHECK(ParseRequest(R"({"t": "parancs", "id": "feladat.idozit", "szoveg": " ", "datum": "2026-12-01"})", scheduled));
    CHECK(CheckSchedule(scheduled, "2026-01-01") == L"Mi legyen a feladat szövege?");
    CHECK(ParseRequest(R"({"t": "parancs", "id": "feladat.idozit", "szoveg": "x", "datum": "2026-12-01", "ismetles": 5000})", scheduled));
    CHECK(CheckSchedule(scheduled, "2026-01-01") == L"Ilyen ismétlést nem tudok beállítani.");

    // Timer and work-time status for Claude: the timer details only while it runs.
    Status status;
    status.measuring = true;
    status.pausedFor = L"kizárt ablak: \"YouTube\"";
    status.workedSeconds = 3 * 3600 + 125;
    status.manualSeconds = 1800;
    status.dailyTargetSeconds = 6 * 3600 + 50 * 60;
    status.workMinutes = 50;
    status.restMinutes = 7.5;
    std::string statusJson = StatusJson(status);
    std::string paused;
    bool flag = true;
    long long minutes = 0;
    CHECK(JsonLite::GetString(statusJson, "szunetel_mert", paused) && paused == u8"kizárt ablak: \"YouTube\"");
    CHECK(JsonLite::GetBool(statusJson, "most_szamol", flag) && !flag);
    CHECK(JsonLite::GetInt64(statusJson, "mai_mert_perc", minutes) && minutes == 182);
    CHECK(JsonLite::GetInt64(statusJson, "mai_osszes_perc", minutes) && minutes == 212);
    CHECK(JsonLite::GetInt64(statusJson, "napi_cel_perc", minutes) && minutes == 410);
    CHECK(JsonLite::GetBool(statusJson, "fut", flag) && !flag && statusJson.find("hatra_mp") == std::string::npos);
    CHECK(statusJson.find("\"piheno_perc\": 7.5") != std::string::npos);
    status.timerActive = true;
    status.workPhase = false;
    status.secondsLeft = 95;
    status.round = 2;
    status.rounds = 8;
    statusJson = StatusJson(status);
    std::string phase;
    CHECK(JsonLite::GetString(statusJson, "szakasz", phase) && phase == u8"pihenő");
    CHECK(JsonLite::GetInt64(statusJson, "hatra_mp", minutes) && minutes == 95);
    CHECK(JsonLite::GetInt64(statusJson, "korok", minutes) && minutes == 8);

    // Reply and spoken text.
    const std::string reply = BuildReply(7, true, L"Felvettem: \"ő\".");
    long long req = 0;
    bool ok = false;
    std::wstring text;
    CHECK(JsonLite::GetString(reply, "t", type) && type == "valasz");
    CHECK(JsonLite::GetInt64(reply, "req", req) && req == 7 && JsonLite::GetBool(reply, "ok", ok) && ok);
    CHECK(JsonLite::GetWideString(reply, "szoveg", text) && text == L"Felvettem: \"ő\".");
    const std::string say = BuildSay("pihenes", L"Pihenj 5 percet.");
    std::string event;
    CHECK(JsonLite::GetString(say, "t", type) && type == "mond");
    CHECK(JsonLite::GetString(say, "esemeny", event) && event == "pihenes");
    CHECK(JsonLite::GetWideString(say, "szoveg", text) && text == L"Pihenj 5 percet.");
}

int main() {
    // No first-run OneDrive folder: the tests must not reach any real file.
    SetEnvironmentVariableW(L"USERPROFILE", ExeFolder().c_str());
    Run("merge rules", TestMergeRules);
    Run("tombstones", TestTombstones);
    Run("sync file round trip", TestSyncFileRoundTrip);
    Run("damaged sync files", TestDamagedSyncFiles);
    Run("poll", TestPoll);
    Run("store sync", TestStoreSync);
    Run("damaged file is not overwritten", TestDamagedFileIsNotOverwritten);
    Run("relay", TestRelay);
    Run("apply polled", TestApplyPolled);
    Run("local load", TestLocalLoad);
    Run("pin hidden", TestPinHidden);
    Run("group order", TestGroupOrder);
    Run("group move and normalize", TestGroupMoveAndNormalize);
    Run("pin drop", TestPinDrop);
    Run("group persistence", TestGroupPersistence);
    Run("group undo and finish", TestGroupUndoAndFinish);
    Run("json", TestJson);
    Run("recurring dates", TestRecurringDates);
    Run("work day cutoffs", TestWorkDayCutoffs);
    Run("version compare", TestVersionCompare);
    Run("search text", TestSearchText);
    Run("work history file", TestWorkHistoryFile);
    Run("work day rollover", TestWorkDayRollover);
    Run("work history cleanup", TestWorkHistoryCleanup);
    Run("recurring tasks", TestRecurringTasks);
    Run("settings round trip", TestSettingsRoundTrip);
    Run("assignment", TestAssignment);
    Run("work time counting", TestWorkTimeCounting);
    Run("interval timer cycle", TestIntervalTimerCycle);
    Run("interval timer waits for user", TestIntervalTimerWaitsForUser);
    Run("strict mode", TestStrictMode);
    Run("vision break", TestVisionBreak);
    Run("pin timer buttons", TestPinTimerButtons);
    Run("laura link", TestLauraLink);
    CleanFiles();
    std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
