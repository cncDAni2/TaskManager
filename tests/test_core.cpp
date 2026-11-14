// Logic tests: sync merge, sync and local files, JSON, recurring dates, cutoffs. Run tests\run_tests.bat.
// Everything happens in the test EXE's own folder (dist\test), never on real data.
#include "Task.h"
#include "Version.h"
#include <cstdio>
#include <functional>

// Hotkeys.cpp needs the whole app; the store only asks this one thing from it.
namespace Hotkeys { bool IsValidCombo(long long) { return true; } }

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

void Run(const char* name, const std::function<void()>& test) {
    const int failuresBefore = g_failures;
    test();
    std::printf("%s %s\n", g_failures == failuresBefore ? "ok    " : "FAILED", name);
}
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
    CleanFiles();
    std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
