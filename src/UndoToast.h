#pragma once

// Floating "N feladat törölve. Visszavonás" bar for 5 seconds after deleting tasks or work history;
// its DeleteTasks / PurgeWorkHistory do the deleting, so always delete through them.
// Non-activating, so clicking it does not count as focus loss. Consecutive deletes add up. A synced
// task comes back as a newer change over its tombstone. A single task has no confirmation dialog;
// the cleanup bar keeps its own.

#include <string>
#include <vector>

// Floating "N feladat törölve. Visszavonás" bar shown for 5 seconds after a delete. These calls do
// the deleting, so that everything needed to undo it is saved first. Deletes following each other
// while the bar is up add up and are undone together.
namespace UndoToast {
void DeleteTasks(const std::vector<int>& ids);
void PurgeWorkHistory(const std::string& dateKey);
}
