#pragma once

#include <string>
#include <vector>

// Floating "N feladat törölve. Visszavonás" bar shown for 5 seconds after a delete. These calls do
// the deleting, so that everything needed to undo it is saved first. Deletes following each other
// while the bar is up add up and are undone together.
namespace UndoToast {
void DeleteTasks(const std::vector<int>& ids);
void PurgeWorkHistory(const std::string& dateKey);
}
