# TaskManager Agent Guidelines

Ultra-lightweight native Windows C++ task and work time tracking application designed for near-zero resource consumption.

For feature overview and usage, see [README.md](README.md).

## Build & Test

- **Build**: Run [build.bat](build.bat) via terminal (`cmd.exe /c "cd /d C:\work\TaskManager && build.bat"`).
  - Uses MSVC x64 (`vcvars64.bat`), compiles resources via `rc.exe`, and builds `src\*.cpp` with `/utf-8 /std:c++17 /O2 /MT /W4`.
  - Output binary: `TaskManager.exe`.
- **Process locking**: Before rebuilding or testing, ensure any running instance is stopped (`Stop-Process -Name TaskManager -Force -ErrorAction SilentlyContinue`).
- **Testing**:
  - Logic tests: compile against [Task.h](Task.h) or `src/Task.h` with MSVC (`cl.exe /nologo /EHsc /std:c++17 test_*.cpp user32.lib secur32.lib`) and clean up test binaries.
  - UI / Process tests: inspect process status and working set using PowerShell `Get-Process -Name TaskManager`.

## Architecture & Code Boundaries

The application is modularized under `src/` into focused, single-responsibility files (100–500 lines each):
- [src/AppTypes.h](src/AppTypes.h): Constants (`IDC_*`, `IDM_*`), enums (`ViewMode`, `CompletedFilter`), `ThemeColors`, `DisplayItem`, `ScrollbarMetrics`.
- [src/AppState.h](src/AppState.h) / [src/AppState.cpp](src/AppState.cpp): Centralized global state declarations & definitions (`g_store`, `g_viewMode`, `g_pinMode`, etc.).
- [src/Task.h](src/Task.h) / [src/Task.cpp](src/Task.cpp): `TaskStore` class for local persistence, OneDrive sync loading/saving, and state resets.
- [src/TaskUtils.h](src/TaskUtils.h) / [src/TaskUtils.cpp](src/TaskUtils.cpp): String encoding helpers (UTF-8/Wide), JSON escape/unescape, date formatting, and user name detection.
- [src/Drawing.h](src/Drawing.h) / [src/Drawing.cpp](src/Drawing.cpp): Crisp vector icon rendering (pencil, pin, cloud, folder-cloud) and DWM dark mode titlebar styling.
- [src/InlineEdit.h](src/InlineEdit.h) / [src/InlineEdit.cpp](src/InlineEdit.cpp): Task inline editing logic, input subclassing, commit/cancel lifecycle.
- [src/Layout.h](src/Layout.h) / [src/Layout.cpp](src/Layout.cpp): Geometry, mini mode transitions, dynamic height calculation, and custom scrollbar metrics.
- [src/Paint.h](src/Paint.h) / [src/Paint.cpp](src/Paint.cpp): Double-buffered GDI rendering for main window and compact mini mode.
- [src/UIEventHandler.h](src/UIEventHandler.h) / [src/UIEventHandler.cpp](src/UIEventHandler.cpp): Event handlers for keyboard navigation, mouse click/move, commands, and tray interactions.
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
  - Event loop: strictly event-driven with `GetMessage`. Never introduce active polling loops or worker threads for idle states.
  - Background sync checking: only reload and recalculate layout if the sync file's `ftLastWriteTime` actually changed (`CheckSyncFileChanged`), preventing needless CPU wakeups and UI redraws.
- **Character Encoding**: Always maintain UTF-8 encoding across files and compilation flags (`/utf-8`, `UNICODE`, `_UNICODE`). Use `std::wstring` and wide-character Win32 API functions (`W` suffixes) throughout the UI. Font creation must use `DEFAULT_CHARSET` to properly render Hungarian accents (`á, é, í, ó, ö, ő, ú, ü, ű`).
- **Single Instance**: Controlled via named mutex `TaskManager_SingleInstance_Mutex_98741`. Second launch signals existing window via `WM_HOTKEY` and exits immediately.
- **Two-Tier Data Persistence**:
  - Local [tasks.json](tasks.json) stores local tasks, `next_id`, `work_seconds`, `last_reset`, and user's chosen `sync_file_path`.
  - OneDrive shared `tasks.json` stores ONLY synchronized tasks (IDs in the 1,000,000+ range) and creator's display name (`author`).
  - Write operations must use binary/UTF-8 mode and properly escape strings using `TaskUtils::EscapeJsonString`.
- **Context Menu & Focus Loss (`WM_ACTIVATE`)**: `TrackPopupMenu` causes the owner window to receive `WM_ACTIVATE` with `WA_INACTIVE`. Always guard focus-loss handling with `g_inContextMenu` to avoid inadvertently hiding the window or collapsing into mini mode while a user browses the context menu.
- **Custom Task List Scrollbar & Mini Mode**: The task list uses an integrated, sleek 5px custom GDI scrollbar rendered strictly within the list bounds rather than standard Win32 non-client scrollbars (`WS_VSCROLL`), ensuring clean visuals and no interference with mini mode. Always remove the `WS_EX_LAYERED` style upon returning to full mode to avoid rendering artifacts with child controls and standard GDI double buffering.
- **Escape Key Dual-Path**: In Win32 popup windows with dialog-like message loops, `VK_ESCAPE` can be intercepted both in the `wWinMain` message loop and in `WndProc (WM_KEYDOWN)`. Keep Escape handling logic synchronized in both places (e.g. transitioning to mini mode when pinned vs hiding to tray when unpinned).
- **Window Positioning & DPI**: Mini mode dynamically anchors to the bottom-right of `g_fullWinX`/`g_fullWinY`/`g_fullWinW`/`g_fullWinH`, constrained to `SPI_GETWORKAREA` to keep the window docked above the taskbar on multi-monitor or varied DPI displays.
