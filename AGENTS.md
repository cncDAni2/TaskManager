# TaskManager Agent Guidelines

Ultra-lightweight native Windows C++ task and work time tracking application designed for near-zero resource consumption.

For feature overview and usage, see [README.md](README.md).

## Build & Test

- **Build**: Run [build.bat](build.bat) via terminal (`cmd.exe /c "cd /d C:\work\TaskManager && build.bat"`).
  - Uses MSVC x64 (`vcvars64.bat`), compiles resources via `rc.exe`, and builds `src\*.cpp` with `/utf-8 /std:c++17 /O2 /MT /W4`.
  - Intermediate object and resource files: `dist\obj\` and `dist\TaskManager.res` (ignored by Git).
  - Output binary: `TaskManager.exe` (kept beside `tasks-local.json`, which the app locates relative to its executable).
- **Versioning**:
  - The authoritative major/minor/patch values are in [src/Version.h](src/Version.h). The Settings dialog and [TaskManager.rc](TaskManager.rc) derive displayed and embedded versions from these macros; change the shared values there rather than duplicating version literals.
  - Preserve the final newline in `src/Version.h`; `rc.exe` can report `RC1004` at EOF if the closing preprocessor directive is not terminated.
  - Update the version automatically for requests that change application code; do not bump it for questions or documentation-only changes.
  - The AI must never increment the major version.
  - Increment the patch by 1 for bug fixes, small changes, refactoring, and any change that does not clearly meet the Minor criteria. When uncertain, use Patch.
  - Increment the minor by 1 and reset the patch to 0 only for a genuinely new feature or an exceptionally large bug fix. Do not use Minor merely because a change spans many files or has a large diff.
  - A release happens when changes are committed. Check the current version against `HEAD` before bumping; do not bump again if a version increase is already uncommitted.
- **GitHub releases** (`gh` CLI is installed and logged in as the repo owner): after a version commit is pushed, publish it as a release when the user asks (the in-app update check reads `releases/latest`, so an unreleased version is invisible to users).
  - Command: `gh release create v<X.Y.Z> TaskManager.exe --repo cncDAni2/TaskManager --target <FULL commit SHA> --title "TaskManager v<X.Y.Z>" --notes-file <notes.md>`. `--target` needs the full 40-character SHA (`git rev-parse <commit>`); a short hash fails with `HTTP 422 ... target_commitish is invalid`.
  - Always attach the `TaskManager.exe` built from that commit (check `git diff --quiet HEAD -- TaskManager.exe` first). Users download it via `https://github.com/cncDAni2/TaskManager/releases/latest/download/TaskManager.exe`.
  - Release notes are in Hungarian, short and user-facing (a few bullet points of what changed); keep the notes file in the scratchpad, not in the repo. Show the draft to the user before publishing.
- **Process lifecycle**: Before building or testing, stop any running instance to release `TaskManager.exe` (`Stop-Process -Name TaskManager -Force -ErrorAction SilentlyContinue`). After a successful build, reopen it with `Start-Process .\TaskManager.exe` from the repository root; `build.bat` does not launch it.
- **Testing**:
  - There are currently no checked-in automated test sources. Use the full build for compile/resource validation and manually exercise affected UI behavior; a successful build alone does not verify visual appearance.
  - For standalone logic tests, compile a focused `test_*.cpp` against [src/Task.h](src/Task.h) with MSVC (`cl.exe /nologo /EHsc /std:c++17 test_*.cpp user32.lib secur32.lib`) and clean up test binaries.
  - UI / Process checks: inspect process status and working set using PowerShell `Get-Process -Name TaskManager`.

## Architecture & Code Boundaries

The application is modularized under `src/` into focused, single-responsibility files. Keep new modules under 500 lines where practical; [src/MainWindow.cpp](src/MainWindow.cpp) is an existing oversized dispatcher, so put new behavior in its owning module instead of expanding it further.

- [src/AppTypes.h](src/AppTypes.h): UI IDs, task/view/filter types, theme colors (plus `GetHistoryColors` for the work-history view in every theme), display items, and scrollbar metrics.
- [src/AppState.h](src/AppState.h) / [src/AppState.cpp](src/AppState.cpp): Centralized globals for the store, views, window modes, focus/session state, and rendering/input state.
- [src/Task.h](src/Task.h) / [src/Task.cpp](src/Task.cpp): `TaskStore` core: task operations, runtime-id mapping, active-task ordering, work tracking, and settings. Split across [src/TaskLocalFile.cpp](src/TaskLocalFile.cpp) (local JSON load/save and legacy migration) and [src/TaskSync.cpp](src/TaskSync.cpp) (multi-file sync: read, merge, relay, notices).
- [src/SyncFiles.h](src/SyncFiles.h) / [src/SyncFiles.cpp](src/SyncFiles.cpp): Shared sync-file records, safe read/write (truncated files are never overwritten), newest-wins merge, tombstone purge, and UID generation.
- [src/JsonLite.h](src/JsonLite.h) / [src/JsonLite.cpp](src/JsonLite.cpp): String-aware readers for the app's hand-written JSON (object/scalar arrays, top-level object members with raw values, and typed fields).
- [src/VersionHint.h](src/VersionHint.h) / [src/VersionHint.cpp](src/VersionHint.cpp): Offline guess from a sync file's `app_version` stamp and unknown members that a colleague runs a newer/older version; shown once per run via `PopupNotice`.
- [src/UpdateCheck.h](src/UpdateCheck.h) / [src/UpdateCheck.cpp](src/UpdateCheck.cpp): GitHub `releases/latest` lookup via delay-loaded WinHTTP on a thread-pool thread; no timer, it runs on startup / showing the window / entering PIN at most every 12 hours, retries a failure on a later invocation after 5 minutes (max 3 retries), and posts `WM_APP_UPDATE_RESULT`.
- [src/UpdateOverlay.h](src/UpdateOverlay.h) / [src/UpdateOverlay.cpp](src/UpdateOverlay.cpp): Full-window "Új verzió érhető el" cover (full and compact PIN layout) while a newer release is known; swallows input and hides child controls; X or Letöltés hides it for the day.
- [src/SyncPicker.h](src/SyncPicker.h) / [src/SyncPicker.cpp](src/SyncPicker.cpp): New-task target selection (cloud button, checklist menu, count badges, dynamic tooltip).
- [src/TaskUtils.h](src/TaskUtils.h) / [src/TaskUtils.cpp](src/TaskUtils.cpp): UTF-8/wide conversion, JSON escaping, date/time formatting, configurable work-reset cutoffs, and user/path helpers.
- [src/AudioCue.h](src/AudioCue.h) / [src/AudioCue.cpp](src/AudioCue.cpp): Generated WAV cues and asynchronous playback.
- [src/IntervalTimer.h](src/IntervalTimer.h) / [src/IntervalTimer.cpp](src/IntervalTimer.cpp): Work/rest cycles and the optional 20-20-20 reminder.
- [src/TimerHold.h](src/TimerHold.h) / [src/TimerHold.cpp](src/TimerHold.cpp): "Időstop" toggle (Ctrl+F3, tray item, hand button) that freezes work-time measurement, the interval timer and the 20-20-20 reminder; runtime-only, the 20-20-20 cycle restarts on release.
- [src/TimerView.h](src/TimerView.h) / [src/TimerView.cpp](src/TimerView.cpp): Interval timer controls and saved timer configuration.
- [src/WorkTracking.h](src/WorkTracking.h) / [src/WorkTracking.cpp](src/WorkTracking.cpp): Work-time measurement on/off (`work_tracking_enabled`; Settings checkbox and the history-view footer button). Off skips the foreground-title check and accrual, hides the work bar in full and mini views (`BottomBarHeight()` / `MiniBarHeight()` return 0) and hides/ignores STRICT MODE; history is kept.
- [src/ExcludedApps.h](src/ExcludedApps.h) / [src/ExcludedApps.cpp](src/ExcludedApps.cpp): Configurable comma-separated window-title keywords that pause work-time counting; cached case-insensitive matching.
- [src/Cleanup.h](src/Cleanup.h) / [src/Cleanup.cpp](src/Cleanup.cpp): Cleanup bar below the header in the completed (old tasks, days) and work-history (old work time, weeks) views, with confirmation; remembered values `cleanup_days` / `cleanup_weeks`.
- [src/MemoryTrim.h](src/MemoryTrim.h) / [src/MemoryTrim.cpp](src/MemoryTrim.cpp): Debounced one-shot working-set trim after startup, entering mini mode, and hiding to tray.
- [src/Marker.h](src/Marker.h) / [src/Marker.cpp](src/Marker.cpp): Task marker behavior and presentation.
- [src/Drawing.h](src/Drawing.h) / [src/Drawing.cpp](src/Drawing.cpp): GDI icon rendering for task, settings, pin, and focus controls, plus DWM titlebar styling.
- [src/BarTooltips.h](src/BarTooltips.h) / [src/BarTooltips.cpp](src/BarTooltips.cpp): Dynamic tooltip regions for work-history bars.
- [src/FocusMode.h](src/FocusMode.h) / [src/FocusMode.cpp](src/FocusMode.cpp): Focus-mode toggling and Windows session lock/unlock notifications.
- [src/InlineEdit.h](src/InlineEdit.h) / [src/InlineEdit.cpp](src/InlineEdit.cpp): Task inline editing logic, input subclassing, commit/cancel lifecycle.
- [src/Layout.h](src/Layout.h) / [src/Layout.cpp](src/Layout.cpp): Window geometry, mini-mode transitions, control visibility, and custom scrollbar metrics. `GetListTop()` / `GetListBottom()` are the single source of the list area for every view.
- [src/Paint.h](src/Paint.h) / [src/Paint.cpp](src/Paint.cpp): Double-buffered GDI rendering for full/mini views and measured/manual work bars.
- [src/PopupNotice.h](src/PopupNotice.h) / [src/PopupNotice.cpp](src/PopupNotice.cpp): Non-activating notices: new tasks (stay until clicked) and changed tasks (auto-close countdown, paused on hover).
- [src/SettingsDialog.h](src/SettingsDialog.h) / [src/SettingsDialog.cpp](src/SettingsDialog.cpp): Settings UI for themes, sound, daily work-reset time, timer auto-start, and excluded window-title keywords; rebuilt in place when sync rows change. Every setting applies and saves instantly except the work-reset time, which waits for the highlighted "Mentés" button (the OK button changes to it).
- [src/SettingsSyncRows.h](src/SettingsSyncRows.h) / [src/SettingsSyncRows.cpp](src/SettingsSyncRows.cpp): Settings table of sync files (default flag, name, path, browse, remove, add).
- [src/UIEventHandler.h](src/UIEventHandler.h) / [src/UIEventHandler.cpp](src/UIEventHandler.cpp): Keyboard/mouse handling, task reordering, commands, and tray interactions.
- [src/WorkHistory.h](src/WorkHistory.h) / [src/WorkHistory.cpp](src/WorkHistory.cpp): Measured/manual work-history persistence and daily/weekly display aggregation.
- [src/WorkTimeDialog.h](src/WorkTimeDialog.h) / [src/WorkTimeDialog.cpp](src/WorkTimeDialog.cpp): Date and duration input for manual work-history entries.
- [src/MainWindow.h](src/MainWindow.h) / [src/MainWindow.cpp](src/MainWindow.cpp): `WndProc` dispatcher and window lifecycle (`ShowAppWindow`, `HideAppWindow`, `ToggleWindow`).
- [src/Main.cpp](src/Main.cpp): `wWinMain` entry point, single-instance mutex check, and message loop.
- [src/Version.h](src/Version.h): Shared version values used by the Settings dialog and EXE version resource.
- [resource.h](resource.h), [TaskManager.rc](TaskManager.rc), [app.manifest](app.manifest): Resource definitions, PerMonitorV2 DPI awareness, and Common Controls 6.0 manifest.

## Conventions & Critical Gotchas

- **User-facing documentation and controls**: [README.md](README.md) is a short user guide, not full program documentation: when adding a user-facing feature, add only what a user would miss (usually one or two sentences); leave out edge cases, internal mechanics and small visual details. Minor/technical features get a brief note or nothing. Give buttons clear, self-explanatory labels; add concise tooltips for icon-only, abbreviated, state-dependent, or otherwise ambiguous actions. A tooltip should clarify the action or effect rather than merely repeat the label, and should not be redundant when the label and context are already clear.
- **Modular structure**: Keep source and header files compact and focused. Prefer a dedicated `.h` / `.cpp` pair or a logical subfolder for new features rather than inflating an unrelated module.
- **Performance**:
  - Preserve the event-driven, non-polling design. The README contains approximate resource-use estimates; do not present them as measured values without checking the running process.
  - The render path uses double-buffered GDI. Suppress `WM_ERASEBKGND` (`return 1`) and avoid unnecessary invalidation; use dirty rectangles for small indicator updates.
  - The existing one-second `IDT_WORK_TIMER` drives work-time accrual, idle/excluded-app checks, and interval-timer ticks. Sync is checked every ten ticks and only changed sync files are reloaded (by timestamp). Do not add unrelated periodic work.
- **Character encoding**: Maintain UTF-8 encoding and compile flags (`/utf-8`, `UNICODE`, `_UNICODE`). Use `std::wstring` and wide-character Win32 API functions (`W` suffixes) throughout the UI. Font creation must use `DEFAULT_CHARSET` to properly render Hungarian accents.
- **Single instance**: Controlled via named mutex `TaskManager_SingleInstance_Mutex_98741`. A second launch signals the existing window via `WM_HOTKEY` and exits immediately.
- **Two-tier data persistence**:
  - Every task has a persistent string `uid` (new tasks: GUID; legacy numeric ids are kept as their decimal string, sync ids normalized to 1,000,000+). `Task::id` is a runtime-only handle from `TaskStore::RuntimeIdFor`; never persist it.
  - Local `tasks-local.json` stores local tasks, active ordering (as uids), current and historical work time (including manual entries), reset timestamp, theme, work-reset time, the `sync_files` list (local `source_id`, name, path, default flag), timer durations/repetitions, the 20-20-20 toggle, timer auto-start preference, sound preference, work-tracking on/off, update-check state (`update_*`: enabled, last check, failures, latest release, dismissed date), and excluded window-title keywords. A legacy `sync_file_path` is migrated to the first `sync_files` entry.
  - Shared sync files store only synchronized tasks with `uid`, `updated_at`, `updated_by`, creator (`author`), assignee, and a `deleted` tombstone flag; keep local settings and work history out of them. The file carries the last writer's `app_version`. Members this version does not know (per task and top-level) are kept as raw JSON (`extras`) and written back unchanged; own edits inherit the task's extras, new tasks have none. Every change bumps `updated_at`, but `updated_by` names only who created or last rewrote the text (ticking, "ÉN" and deleting keep it); the merge relies on `updated_at` alone.
  - Sync writes are always read → merge → write. The newest `updated_at` wins; relaying keeps the winner's original `updated_at`/`updated_by`. Changes are written only to files that already contain the uid. Tombstones are purged after 14 days. Files that fail to parse are never overwritten; own changes wait in `pendingWrites`.
  - JSON parsing is hand-written. Write operations must use binary/UTF-8 mode and escape strings with `TaskUtils::EscapeJsonString`.
- **Work timer & focus mode**: Focus mode overrides idle/excluded-app pauses, but session lock always stops counting and disables focus mode; unlocking does not re-enable it. Keep `FocusMode` session-notification registration and unregistration paired.
- **Work reset vs completed filter**: The daily work reset time is configurable in Settings (default 09:15); the completed-task recent filter has a separate yesterday-09:30 cutoff. Do not conflate these date boundaries.
- **Context menu & focus loss (`WM_ACTIVATE`)**: `TrackPopupMenu` causes the owner window to receive `WM_ACTIVATE` with `WA_INACTIVE`. Guard focus-loss handling with `g_inContextMenu` to avoid hiding the window or collapsing into mini mode while browsing the context menu.
- **Custom task-list scrollbar & mini mode**: The task list uses an integrated 5px custom GDI scrollbar inside list bounds rather than standard non-client scrollbars (`WS_VSCROLL`). Remove `WS_EX_LAYERED` when returning to full mode to avoid rendering artifacts with child controls and double buffering.
- **Escape key dual path**: In popup windows with dialog-like message loops, `VK_ESCAPE` can be intercepted both in the `wWinMain` message loop and in `WndProc (WM_KEYDOWN)`. Keep handling synchronized in both places (for example, pinned-to-mini versus unpinned-to-tray).
- **Window positioning & DPI**: Mini mode anchors to the bottom-right of `g_fullWinX` / `g_fullWinY` / `g_fullWinW` / `g_fullWinH`, constrained to `SPI_GETWORKAREA` to stay above the taskbar across monitors and DPI settings.
- **Owner-drawn controls**: Main-window `WM_DRAWITEM` handling is centralized in `UIEventHandler::HandleDrawItem`. Match child control backgrounds to the parent `ThemeColors` surface and use the actual item rectangle; compile success does not verify alignment or appearance. Stateful owner-drawn toggles must update model state on `BN_CLICKED` and invalidate the control.
- **Edit Enter handling**: If an edit control consumes `VK_RETURN` in `WM_KEYDOWN`, suppress the translated `WM_CHAR` carriage return when needed; otherwise the edit control can still trigger its default system sound.
- **Audio cues**: `AudioCue` (sound playback) and `UpdateCheck` (one HTTPS request) use thread-pool callbacks as the only exceptions to the single-threaded design; `UpdateCheck` touches no shared state and reports back only via `PostMessage`. A shared atomic flag prevents overlapping cues, and cues requested while another is playing are dropped rather than queued.
