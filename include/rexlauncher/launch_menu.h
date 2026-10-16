// rexlauncher - in-window launch menu for rexglue recompilation projects.
//
// A PPSSPP-style menu (big rows, focus highlight, controller navigation) drawn
// in the game's own window before the game starts, like picking a game in an
// emulator. "Play" hands the window over to the game.
//
// Hook it into a ReXApp subclass:
//
//   #include <rexlauncher/launch_menu.h>
//
//   void OnConfigureFonts(ImFontAtlas* atlas) override {
//     rexlauncher::ConfigureFonts(atlas);
//   }
//   std::optional<rex::PathConfig> OnFinalizePaths(
//       const rex::PathConfig& defaults,
//       std::function<void(rex::PathConfig)> resume) override {
//     return rexlauncher::Show(imgui_drawer(), app_context(), defaults, std::move(resume),
//                              {.title = "My Game"});
//   }
//
// Options are SDK cvars. Choices are applied as they are made and saved to the
// app's config file (PathConfig::config_path, e.g. <exe dir>/<name>.toml),
// which the SDK loads at the next start. Cvars given on the command line are
// shown but locked. Options read before the menu appears (presenter settings)
// are marked requires_restart: if one changed, Play restarts the process
// (Linux) so it takes effect, skipping the menu the second time.
//
// The menu is skipped with --launcher_menu=false or REXLAUNCHER_SKIP=1 (for
// unattended runs such as `rex triage`).
//
// The same menu can also be opened while the game runs (F1, or Back + Start
// on a controller) after enabling it once the runtime exists:
//
//   void OnPostSetup() override {
//     rexlauncher::EnableInGameMenu(imgui_drawer(), app_context(), runtime());
//   }
//
// It reuses the Config given to Show(). While it is open the game gets no
// controller input and is sent the "system UI showing" notification (the one
// the Xbox guide sends), which most games answer by pausing. Settings that
// cannot change under a running game are marked for the next start, and a
// Restart game row applies them.

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <rex/rex_app.h>

struct ImFontAtlas;

namespace rex {
class Runtime;
}  // namespace rex

namespace rex::ui {
class ImGuiDrawer;
class WindowedAppContext;
}  // namespace rex::ui

namespace rexlauncher {

struct Choice {
  std::string value;  // cvar value, as the cvar parser takes it ("true", "2", "fsr")
  std::string label;  // shown in the menu
};

struct Option {
  std::string label;
  std::string cvar;
  std::vector<Choice> choices;  // cycled with left/right
  // The cvar is read before the menu appears (window/presenter creation);
  // changing it restarts the process on Play.
  bool requires_restart = false;
  // The game reads the cvar only when it starts: from the in-game menu, a
  // change applies at the next start. (Cvars the SDK marks kRequiresRestart
  // are treated this way automatically.)
  bool restart_in_game = false;
};

struct Config {
  std::string title;
  std::string subtitle;
  // Game-specific rows, shown after the standard ones.
  std::vector<Option> extra;
  // Rendering resolution, display mode, window size, upscaling filter, VSync.
  bool standard_options = true;
};

// Registers the menu fonts (call from ReXApp::OnConfigureFonts). Without it
// the menu falls back to the SDK's small default font.
void ConfigureFonts(ImFontAtlas* atlas);

// Shows the menu and returns std::nullopt: the game starts when the player
// picks Play (resume is called with `defaults`). Returns `defaults` right away
// when the menu is disabled, so the game starts as if there were no menu.
std::optional<rex::PathConfig> Show(rex::ui::ImGuiDrawer* imgui_drawer,
                                    rex::ui::WindowedAppContext& app_context,
                                    const rex::PathConfig& defaults,
                                    std::function<void(rex::PathConfig)> resume, Config config);

// Lets the player open the menu during the game (call from
// ReXApp::OnPostSetup, after Show). Replaces the input system's active
// callback; the SDK's own overlays still block game input while they hold the
// mouse.
void EnableInGameMenu(rex::ui::ImGuiDrawer* imgui_drawer,
                      rex::ui::WindowedAppContext& app_context, rex::Runtime* runtime);

}  // namespace rexlauncher
