# TaskManager Agent Guidelines

Ultra-lightweight native Windows C++ task and work time tracking application designed for near-zero resource consumption.

For feature overview and usage, see [README.md](README.md).

## Build & Test

- **Build**: Run [build.bat](build.bat) via terminal (`cmd.exe /c "cd /d C:\work\TaskManager && build.bat"`).
  - Uses MSVC x64 (`vcvars64.bat`), compiles resources via `rc.exe`, and builds `src\*.cpp` with `/utf-8 /std:c++17 /O2 /MT /W4`.
  - Intermediate object and resource files: `dist\obj\` and `dist\TaskManager.res` (ignored by Git).
  - Output binary: `TaskManager.exe` (kept beside `tasks-local.json`, which the app locates relative to its executable).
- **Versioning**:
  - The Settings dialog version is `APP_VERSION` in [src/SettingsDialog.cpp](src/SettingsDialog.cpp). Update it automatically for requests that change application code; do not bump it for questions or documentation-only changes.
  - Keep the major version unchanged. For a bug fix or small change, increment the patch by 1 (for example, `v1.6.4` to `v1.6.5`). For a new feature or larger change, increment the minor by 1 and reset the patch to 0 (for example, `v1.6.4` to `v1.7.0`).
  - A release happens when the changes are committed. Check the current version against `HEAD` before bumping: if a version increase is already uncommitted, do not bump again for later requests before that commit. If that pending patch bump should instead represent a feature or larger change, promote it to the next minor version with patch 0 (for example, `v1.6.5` to `v1.7.0`), not an additional increment.
- **Process lifecycle**: Before building or testing, stop any running instance to release `TaskManager.exe` (`Stop-Process -Name TaskManager -Force -ErrorAction SilentlyContinue`). After a successful build, reopen the app with `Start-Process .\TaskManager.exe` from the repository root; `build.bat` does not launch it automatically.
- **Testing**:
  - Logic tests: compile against [Task.h](Task.h) or `src/Task.h` with MSVC (`cl.exe /nologo /EHsc /std:c++17 test_*.cpp user32.lib secur32.lib`) and clean up test binaries.
  - UI / Process tests: inspect process status and working set using PowerShell `Get-Process -Name TaskManager`.

## Architecture & Code Boundaries

The application is modularized under `src/` into focused, single-responsibility files (100–500 lines each):
- [src/AppTypes.h](src/AppTypes.h): UI IDs, task/view/filter types, theme colors, display items, and scrollbar metrics.
- [src/AppState.h](src/AppState.h) / [src/AppState.cpp](src/AppState.cpp): Centralized globals for the store, views, window modes, focus/session state, and rendering/input state.
- [src/Task.h](src/Task.h) / [src/Task.cpp](src/Task.cpp): `TaskStore` for local persistence, OneDrive task sync, active-task ordering, work tracking, and settings.
- [src/TaskUtils.h](src/TaskUtils.h) / [src/TaskUtils.cpp](src/TaskUtils.cpp): UTF-8/wide conversion, JSON escaping, date/time formatting, configurable work-reset cutoffs, and user/path helpers.
- [src/Drawing.h](src/Drawing.h) / [src/Drawing.cpp](src/Drawing.cpp): GDI icon rendering for task, settings, pin, and focus controls, plus DWM titlebar styling.
- [src/BarTooltips.h](src/BarTooltips.h) / [src/BarTooltips.cpp](src/BarTooltips.cpp): Dynamic tooltip regions for work-history bars.
- [src/FocusMode.h](src/FocusMode.h) / [src/FocusMode.cpp](src/FocusMode.cpp): Focus-mode toggling and Windows session lock/unlock notifications.
- [src/InlineEdit.h](src/InlineEdit.h) / [src/InlineEdit.cpp](src/InlineEdit.cpp): Task inline editing logic, input subclassing, commit/cancel lifecycle.
- [src/Layout.h](src/Layout.h) / [src/Layout.cpp](src/Layout.cpp): Window geometry, mini-mode transitions, control visibility, and custom scrollbar metrics.
- [src/Paint.h](src/Paint.h) / [src/Paint.cpp](src/Paint.cpp): Double-buffered GDI rendering for full/mini views and measured/manual work bars.
- [src/PopupNotice.h](src/PopupNotice.h) / [src/PopupNotice.cpp](src/PopupNotice.cpp): Non-activating notices for newly added tasks.
- [src/SettingsDialog.h](src/SettingsDialog.h) / [src/SettingsDialog.cpp](src/SettingsDialog.cpp): Settings UI for sync-file path, dark mode, and daily work-reset time.
- [src/UIEventHandler.h](src/UIEventHandler.h) / [src/UIEventHandler.cpp](src/UIEventHandler.cpp): Keyboard/mouse handling, task reordering, commands, and tray interactions.
- [src/WorkHistory.h](src/WorkHistory.h) / [src/WorkHistory.cpp](src/WorkHistory.cpp): Measured/manual work-history persistence and daily/weekly display aggregation.
- [src/WorkTimeDialog.h](src/WorkTimeDialog.h) / [src/WorkTimeDialog.cpp](src/WorkTimeDialog.cpp): Date and duration input for manual work-history entries.
- [src/MainWindow.h](src/MainWindow.h) / [src/MainWindow.cpp](src/MainWindow.cpp): `WndProc` dispatcher, window lifecycle (`ShowAppWindow`, `HideAppWindow`, `ToggleWindow`).
- [src/Main.cpp](src/Main.cpp): `wWinMain` entry point, single-instance mutex check, and message loop.
- [resource.h](resource.h), [TaskManager.rc](TaskManager.rc), [app.manifest](app.manifest):
  - DPI awareness (PerMonitorV2) and Common Controls 6.0 manifest.

## Conventions & Critical Gotchas

- **Modular Structure & Small Files (Strict 100–500 lines limit)**:
  - Keep all source and header files compact, readable, and focused (strictly under 500 lines).
  - When implementing new features or components, **always create dedicated new `.h` / `.cpp` files** (or logical subfolders under `src/`) rather than inflating existing modules.
  - Maintain clean logical separation of concerns (e.g. types, state, event handling, rendering, business logic, storage).
- **Maximum Performance & Near-Zero Footprint**:
  - Target: zero idle CPU usage, minimum memory working set (~15-18 MB active, trimmed to ~2-3 MB on hide via `SetProcessWorkingSetSize`).
  - Render path: 100% flicker-free double-buffered GDI. Suppress `WM_ERASEBKGND` (`return 1`). Avoid unnecessary `InvalidateRect` calls; use dirty rect invalidation when updating indicators.
  - Event loop: use `GetMessage`; avoid busy polling and worker threads. The existing one-second `IDT_WORK_TIMER` is for work-time accrual; do not add periodic work unrelated to tracking.
  - Background sync checking: only reload and recalculate layout if the sync file's `ftLastWriteTime` actually changed (`CheckSyncFileChanged`), preventing needless CPU wakeups and UI redraws.
- **Character Encoding**: Always maintain UTF-8 encoding across files and compilation flags (`/utf-8`, `UNICODE`, `_UNICODE`). Use `std::wstring` and wide-character Win32 API functions (`W` suffixes) throughout the UI. Font creation must use `DEFAULT_CHARSET` to properly render Hungarian accents (`á, é, í, ó, ö, ő, ú, ü, ű`).
- **Single Instance**: Controlled via named mutex `TaskManager_SingleInstance_Mutex_98741`. Second launch signals existing window via `WM_HOTKEY` and exits immediately.
- **Two-Tier Data Persistence**:
  - Local `tasks-local.json` stores local tasks, `next_id`, active ordering, current and historical work time (including manual entries), reset timestamp, theme, work-reset time, and chosen `sync_file_path`.
  - OneDrive shared `tasks.json` stores only synchronized tasks (IDs in the 1,000,000+ range), creator (`author`), and assignee; keep local settings and work history out of the shared file.
  - Write operations must use binary/UTF-8 mode and properly escape strings using `TaskUtils::EscapeJsonString`.
- **Work Timer & Focus Mode**: Focus mode overrides idle/excluded-app pauses, but session lock always stops counting and disables focus mode; unlocking does not re-enable focus mode. Keep `FocusMode` session-notification registration and unregistration paired.
- **Work Reset vs Completed Filter**: The daily work reset time is configurable in Settings (default 09:15); the completed-task recent filter has its separate yesterday-09:30 cutoff. Do not conflate these date boundaries.
- **Context Menu & Focus Loss (`WM_ACTIVATE`)**: `TrackPopupMenu` causes the owner window to receive `WM_ACTIVATE` with `WA_INACTIVE`. Always guard focus-loss handling with `g_inContextMenu` to avoid inadvertently hiding the window or collapsing into mini mode while a user browses the context menu.
- **Custom Task List Scrollbar & Mini Mode**: The task list uses an integrated, sleek 5px custom GDI scrollbar rendered strictly within the list bounds rather than standard Win32 non-client scrollbars (`WS_VSCROLL`), ensuring clean visuals and no interference with mini mode. Always remove the `WS_EX_LAYERED` style upon returning to full mode to avoid rendering artifacts with child controls and standard GDI double buffering.
- **Escape Key Dual-Path**: In Win32 popup windows with dialog-like message loops, `VK_ESCAPE` can be intercepted both in the `wWinMain` message loop and in `WndProc (WM_KEYDOWN)`. Keep Escape handling logic synchronized in both places (e.g. transitioning to mini mode when pinned vs hiding to tray when unpinned).
- **Window Positioning & DPI**: Mini mode dynamically anchors to the bottom-right of `g_fullWinX`/`g_fullWinY`/`g_fullWinW`/`g_fullWinH`, constrained to `SPI_GETWORKAREA` to keep the window docked above the taskbar on multi-monitor or varied DPI displays.
