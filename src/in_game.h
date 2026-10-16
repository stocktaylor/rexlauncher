// rexlauncher - in-game menu state (EnableInGameMenu, see
// include/rexlauncher/launch_menu.h): opened with F1 or Back + Start, it
// blocks game input and tells the game a system UI is showing.

#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>

#include <rexlauncher/launch_menu.h>

namespace rexlauncher {

class LaunchMenu;

struct InGame {
  // From Show() / EnableInGameMenu().
  Config config;
  std::filesystem::path config_path;
  rex::ui::ImGuiDrawer* drawer = nullptr;
  rex::ui::WindowedAppContext* app_context = nullptr;
  rex::Runtime* runtime = nullptr;
  bool enabled = false;
  // Values of the menu's cvars as the game started, to tell which changes
  // wait for a restart.
  std::map<std::string, std::string> running_values;

  // UI thread only.
  LaunchMenu* menu = nullptr;

  // Read by the guest threads polling input.
  std::atomic<bool> open{false};
  std::atomic<bool> open_queued{false};
  // After the menu closes, game input stays off until every button is let go,
  // so the press that closed the menu does not reach the game.
  std::atomic<bool> releasing{false};

  std::mutex poll_mutex;
  std::chrono::steady_clock::time_point next_poll{};
  bool chord_held = false;
};

// Never destroyed: guest threads may poll input during shutdown.
InGame& State();

// XN_SYS_UI to the game: what XAM sends while the guide is up.
void SendSystemUINotification(bool showing);

}  // namespace rexlauncher
