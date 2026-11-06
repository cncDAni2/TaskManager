#include "SyncPoll.h"
#include "AppState.h"
#include <algorithm>
#include <atomic>
#include <memory>

namespace {
struct Job {
    HWND owner = nullptr;
    std::vector<SyncFiles::PollFile> files;
};

std::atomic<bool> g_running{ false };

void CALLBACK Run(PTP_CALLBACK_INSTANCE, void* context) {
    std::unique_ptr<Job> job(static_cast<Job*>(context));
    auto& files = job->files;
    files.erase(std::remove_if(files.begin(), files.end(),
        [](SyncFiles::PollFile& file) { return !SyncFiles::Poll(file); }), files.end());
    if (!files.empty() &&
        PostMessageW(job->owner, WM_APP_SYNC_POLL_RESULT, 0, reinterpret_cast<LPARAM>(job.get()))) {
        job.release();
        return;
    }
    g_running = false;
}
}

void SyncPoll::Request(HWND owner) {
    if (g_store.syncSources.empty() || g_running) return;
    std::vector<SyncFiles::PollFile> files = g_store.PollSnapshot();
    files.erase(std::remove_if(files.begin(), files.end(),
        [](const SyncFiles::PollFile& file) { return !SyncFiles::NeedsRead(file); }), files.end());
    if (files.empty()) return;

    g_running = true;
    auto job = std::make_unique<Job>();
    job->owner = owner;
    job->files = std::move(files);
    if (TrySubmitThreadpoolCallback(Run, job.get(), nullptr)) job.release();
    else g_running = false;
}

std::vector<SyncFiles::PollFile> SyncPoll::TakeResult(LPARAM lParam) {
    std::unique_ptr<Job> job(reinterpret_cast<Job*>(lParam));
    g_running = false;
    return std::move(job->files);
}
