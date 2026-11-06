#pragma once

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
