#pragma once

// The timed sync check: every ten ticks of IDT_WORK_TIMER timestamps are compared on the UI thread,
// and only a changed file is read on a thread-pool thread (SyncFiles::Poll on a copy from
// PollSnapshot), so a slow download cannot freeze the window and the idle working set stays small
// (a thread at every check costs about 750 KB).
// WM_APP_SYNC_POLL_RESULT is posted only when a file changed and ApplyPolled drops files written
// here in the meantime. Opening a window reads no file; own changes and "ÉN" read and write in
// place on the UI thread.

#include "SyncFiles.h"
#include <vector>

// The timed sync check: a changed file is read on a thread-pool thread, so a slow download cannot
// freeze the window. Own changes and "ÉN" are not affected: they read and write in place.
namespace SyncPoll {
    // Compares the timestamps here and starts the thread only for a change: running it at every
    // check would keep about 750 KB more in the working set.
    void Request(HWND owner);
    // WM_APP_SYNC_POLL_RESULT (sent only when a file changed): the files to apply.
    std::vector<SyncFiles::PollFile> TakeResult(LPARAM lParam);
}
