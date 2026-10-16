// rexlauncher - in-window launch menu. See include/rexlauncher/launch_menu.h.
//
// Source layout:
//   launch_menu.cpp  Show(): the menu before the game
//   in_game.*        EnableInGameMenu(): the menu over the running game
//   menu.*           the menu dialog: rows and actions
//   menu_draw.cpp    the menu dialog: drawing and mouse
//   input.*          gamepad and keyboard input
//   options.*        standard option rows and the cvar changes behind them
//   settings.*       saving to the config file, settings restart
//   draw.*, theme.h, fonts.*  drawing helpers, colors, fonts (ConfigureFonts)

#include <rexlauncher/launch_menu.h>

#include <cstdlib>

#include <rex/cvar.h>

#include "in_game.h"
#include "menu.h"
#include "settings.h"

REXCVAR_DEFINE_BOOL(launcher_menu, true, "Launcher",
                    "Show the launch menu before the game starts (rexlauncher)");

namespace rexlauncher {

namespace {

constexpr const char* kSkipEnv = "REXLAUNCHER_SKIP";

}  // namespace

std::optional<rex::PathConfig> Show(rex::ui::ImGuiDrawer* imgui_drawer,
                                    rex::ui::WindowedAppContext& app_context,
                                    const rex::PathConfig& defaults,
                                    std::function<void(rex::PathConfig)> resume, Config config) {
  // Kept for the in-game menu.
  State().config = config;
  State().config_path = defaults.config_path;
  // Second start after a settings restart: go straight to the game.
  if (std::getenv(kAutostartEnv)) {
#if defined(__linux__)
    unsetenv(kAutostartEnv);
#endif
    return defaults;
  }
  if (!imgui_drawer || !REXCVAR_GET(launcher_menu) || std::getenv(kSkipEnv)) {
    return defaults;
  }
  // The dialog registers with the drawer and deletes itself when closed.
  new LaunchMenu(imgui_drawer, app_context, defaults, std::move(resume), std::move(config));
  return std::nullopt;
}

}  // namespace rexlauncher
