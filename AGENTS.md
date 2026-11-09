# TaskManager Agent Guidelines

Ultra-lightweight native Windows C++ task and work time tracking application designed for near-zero resource consumption. Features and usage: [README.md](README.md).

**Priorities**: 1. the least allocated memory, 2. the least computation/CPU time. EXE size and build time are not important factors. Decide trade-offs (compiler flags, caches, threads) by measuring these two.

**Module details live in the code**: before editing a module, read the comment at the top of its header (modules without a header: the top of the `.cpp`). If a change alters what that comment says, update it in the same change.

## Build & Test

- **Build**: `cmd.exe /c "cd /d C:\work\TaskManager && .\build.bat"` (MSVC x64; flags are in [build.bat](build.bat)). Measured: `/O1` uses no less memory than the current `/O2`, it only makes the EXE smaller. Intermediates go to `dist\` (ignored by Git). Output: `TaskManager.exe`, kept beside `tasks-local.json`, which the app locates relative to its executable.
- **Process lifecycle**: Before building or testing, stop any running instance (`Stop-Process -Name TaskManager -Force -ErrorAction SilentlyContinue`). After a successful build, reopen it with `Start-Process .\TaskManager.exe` from the repository root; `build.bat` does not launch it.
- **Logic tests**: `cmd.exe /c ".\tests\run_tests.bat"` builds and runs [tests/test_core.cpp](tests/test_core.cpp) in `dist\test` with its own files, never real data. Run it after changing `TaskStore`, `SyncFiles`, `JsonLite` or date logic, and add a case for what you changed. In this environment bat files need the `.\` prefix.
- **UI code has no automated tests**: use the full build for compile/resource validation and manually exercise affected behavior; a successful build does not verify appearance. Process status and working set: `Get-Process -Name TaskManager`.
- **Versioning**: The values are in [src/Version.h](src/Version.h); the Settings dialog and [TaskManager.rc](TaskManager.rc) derive from them, so never duplicate version literals. Preserve that file's final newline (`rc.exe` reports `RC1004` otherwise).
  - Bump automatically for requests that change application code; not for questions or documentation-only changes. Check the version against `HEAD` first: do not bump again if an increase is already uncommitted. A release happens when changes are committed.
  - Never increment the major version.
  - Patch +1 for bug fixes, small changes, refactoring, and anything that does not clearly meet Minor. When uncertain, use Patch.
  - Minor +1 (patch reset to 0) only for a genuinely new feature or an exceptionally large bug fix, not merely because a change spans many files.
- **GitHub releases**: only when the user asks. Follow [.claude/skills/release/SKILL.md](.claude/skills/release/SKILL.md).

## Modules

Everything is in `src/`, as `.h` / `.cpp` pairs unless noted. Keep new modules under 500 lines where practical; prefer a new focused pair over inflating an unrelated module. `MainWindow.cpp` is an oversized dispatcher: put new behavior in its owning module.

**Core and data (no UI)**
- `Task`: `TaskStore` core and file contents. Same class, `.cpp` only: `TaskLocalFile` (local load/save), `TaskSync` (multi-file sync), `TaskUndo` (restore), `TaskGroups` (groups), `TaskRecurring` (recurring run).
- `SyncFiles`: sync records, safe read/write, merge, tombstones, UIDs. `SyncPoll`: timed background sync check. `VersionHint`: guess a colleague's app version.
- `Recurring`: rules and date math. `WorkHistory`: work-history persistence and aggregation.
- `JsonLite`: string-aware JSON readers. `TaskUtils`: text, JSON escaping, `TextBuffer`, file and date helpers.

**App state and shell**
- `AppTypes.h`: UI IDs, task/view/filter types, theme colors (`GetHistoryColors` for the work-history view), display items, scrollbar metrics. `AppState`: central globals.
- `Main.cpp`: `wWinMain`, single-instance check, message loop. `MainWindow`: `WndProc`, window lifecycle. `UIEventHandler`: input, reordering, commands, tray, `WM_DRAWITEM`.
- `Layout`: geometry, mini mode, control visibility, list area. `UiScale`: display scaling. `Paint`: rendering. `Drawing`: GDI icons, DWM titlebar. `MemoryTrim`: one-shot working-set trim.
- `resource.h`, `TaskManager.rc`, `app.manifest` (repository root): resources, Common Controls 6.0 manifest.

**Task list UI**
- `InlineEdit`: inline editing and group rename. `Marker`: task marker. `TaskSearch`: search in the completed view. `Cleanup`: cleanup bar.
- `GroupView`: group frames. `GroupDrag`: drop target of every list drag.
- `RecurringView`: "Ütemezett feladatok" page. `ThemedDatePicker`: themed date control. `SyncPicker`: new-task target selection (cloud button, checklist menu, badges, tooltip).

**Time tracking**
- `WorkTracking`: measurement on/off and the one-second tick. `ExcludedApps`: title keywords that pause counting. `IntervalTimer`: work/rest cycles, 20-20-20 reminder. `TimerView`: timer controls. `TimerHold`: "Időstop". `FocusMode`: focus mode, session lock. `WorkTimeDialog`: manual work entries.

**Other windows and feedback**
- `SettingsDialog`, `SettingsSyncRows` (sync file table), `HotkeyDialog`, `Hotkeys` (global hotkeys), `PinButtons` (PIN top strip), `PinButtonsEditor`.
- `PopupNotice`, `StateToast`, `UndoToast`, `BarTooltips` (dynamic tooltip regions), `AudioCue`.
- `UpdateCheck`, `UpdateOverlay`, `SelfUpdate`, `Version.h`.

## Data Safety

These protect user data, wherever you are working.

- Never overwrite a file that could not be read: a damaged `tasks-local.json` blocks every save (`localLoadFailed`), sync files that fail to parse wait with `pendingWrites`.
- Sync writes are always read, merge, write. Keep local settings and work history out of sync files; keep unknown members (`extras`) unchanged.
- JSON is hand-written. Write only through `TaskUtils::WriteFileAtomic` and escape strings with `TaskUtils::EscapeJsonString`.
- Tasks are identified by the persistent `uid`. `Task::id` is runtime-only: never persist it.
- Never name a key `tasks` inside `tasks-local.json` other than the task list: that list is looked up by name in the whole file.
- Delete tasks and work history only through `UndoToast::DeleteTasks` / `PurgeWorkHistory`.
- Call `Save()` after a change.

## Conventions

- **User-facing documentation and controls**: [README.md](README.md) is a short user guide: for a new feature add only what a user would miss (usually one or two sentences), no edge cases or internals; minor or technical features get a brief note or nothing. Give buttons clear labels. Add concise tooltips for icon-only, abbreviated, state-dependent or ambiguous actions; a tooltip clarifies the effect rather than repeating the label.
- **Performance**: Preserve the event-driven, non-polling design. Do not add unrelated periodic work: reuse the one-second `IDT_WORK_TIMER`. Rendering is double-buffered GDI with dirty rectangles (see `Paint.h`). The README's resource figures are estimates; do not present them as measured without checking the running process.
- **Threads**: Single-threaded design. The only exceptions are thread-pool callbacks in `AudioCue`, `UpdateCheck`, `SelfUpdate` and `SyncPoll`; the last three touch no shared state and report back only via `PostMessage`.
- **Display scaling**: Main-window code works in 96-DPI units: use `UiScale` helpers instead of `GetClientRect`, child `SetWindowPos`, `InvalidateRect` with a rect, or `GetCursorPos` (see `UiScale.h`).
- **Character encoding**: Keep UTF-8 and the compile flags (`/utf-8`, `UNICODE`, `_UNICODE`). Use `std::wstring` and `W` Win32 functions in the UI. Fonts must use `DEFAULT_CHARSET` for Hungarian accents.
- **Single instance**: Named mutex `TaskManager_SingleInstance_Mutex_98741`. A second launch signals the existing window via `WM_HOTKEY` and exits.
- **Work reset vs completed filter**: The daily work reset is configurable in Settings (default 09:15); the completed-task recent filter has a separate yesterday-09:30 cutoff. Do not conflate them.
- **Escape key dual path**: `VK_ESCAPE` can be intercepted both in the `wWinMain` message loop and in `WndProc (WM_KEYDOWN)`. Keep both in sync (for example pinned-to-mini versus unpinned-to-tray).
- **Edit Enter handling**: If an edit control consumes `VK_RETURN` in `WM_KEYDOWN`, suppress the translated `WM_CHAR` carriage return when needed, or the default system sound can still play.
