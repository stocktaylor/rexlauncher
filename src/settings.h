// rexlauncher - saving the menu's cvars to the SDK config file, and restarting
// the process so settings read before the menu appeared take effect.

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace rexlauncher {

// Set on the restarted process so its menu is skipped.
inline constexpr const char* kAutostartEnv = "REXLAUNCHER_AUTOSTART";

bool IsLockedByCommandLine(const std::string& cvar);

// Writes `cvars` as top-level keys of the SDK config file (before any [table],
// where top-level keys must go), replacing earlier values of the same keys and
// keeping everything else.
void SaveToConfig(const std::filesystem::path& path, const std::vector<std::string>& cvars);

// Re-executes this process with the same arguments, skipping the menu, so
// settings read before the menu appeared take effect. Returns only on failure.
void RestartProcess();

}  // namespace rexlauncher
