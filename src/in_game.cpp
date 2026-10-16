// rexlauncher - the in-game menu: trigger (F1, Back + Start) and game input
// gating.

#include "in_game.h"

#include <cstdint>
#include <vector>

#include <rex/cvar.h>
#include <rex/input/input_system.h>
#include <rex/runtime.h>
#include <rex/system/kernel_state.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/windowed_app_context.h>

#include "input.h"
#include "menu.h"

namespace rexlauncher {

namespace {

// XN_SYS_UI: what XAM sends while the guide or another system screen is up.
constexpr uint32_t kNotificationSystemUI = 0x00000009;

void OpenInGameMenu() {
  auto& g = State();
  if (g.open || !g.drawer || !g.app_context) {
    g.open_queued = false;
    return;
  }
  g.open = true;
  g.open_queued = false;
  rex::PathConfig paths;
  paths.config_path = g.config_path;
  g.menu = new LaunchMenu(g.drawer, *g.app_context, std::move(paths), nullptr, g.config, true);
  SendSystemUINotification(true);
}

void ToggleInGameMenu() {
  auto& g = State();
  if (g.menu) {
    g.menu->Resume();
  } else {
    OpenInGameMenu();
  }
}

// The input system's active callback. Called from the guest threads each time
// the game polls a controller, which also makes it the place to watch for the
// Back + Start chord (no ImGui dialog, so no UI frame, runs during play).
bool GameInputActive() {
  auto& g = State();
  if (g.open) {
    return false;
  }
  if (std::unique_lock lock(g.poll_mutex, std::try_to_lock); lock) {
    auto now = std::chrono::steady_clock::now();
    if (now >= g.next_poll) {
      g.next_poll = now + std::chrono::milliseconds(15);
      bool chord = false;
      bool any = ReadPadsForMenu(chord);
      if (g.releasing && !any) {
        g.releasing = false;
      }
      if (chord && !g.chord_held && !g.releasing && !g.open_queued.exchange(true)) {
        g.app_context->CallInUIThread([] { OpenInGameMenu(); });
      }
      g.chord_held = chord;
    }
  }
  if (g.releasing || g.open_queued) {
    return false;
  }
  // As the SDK's own callback: an overlay holding the mouse keeps input.
  return !g.drawer->GetIO().WantCaptureMouse;
}

}  // namespace

InGame& State() {
  static InGame* state = new InGame;
  return *state;
}

void SendSystemUINotification(bool showing) {
  auto& g = State();
  if (auto* kernel = g.runtime ? g.runtime->kernel_state() : nullptr) {
    kernel->BroadcastNotification(kNotificationSystemUI, showing ? 1 : 0);
  }
}

void EnableInGameMenu(rex::ui::ImGuiDrawer* imgui_drawer,
                      rex::ui::WindowedAppContext& app_context, rex::Runtime* runtime) {
  auto& g = State();
  if (g.enabled || !imgui_drawer || !runtime) {
    return;
  }
  auto* input = static_cast<rex::input::InputSystem*>(runtime->input_system());
  if (!input) {
    return;
  }
  g.drawer = imgui_drawer;
  g.app_context = &app_context;
  g.runtime = runtime;
  g.enabled = true;

  // What the running game was started with.
  std::vector<std::string> cvars;
  for (const auto& option : g.config.extra) {
    cvars.push_back(option.cvar);
  }
  if (g.config.standard_options) {
    for (const char* cvar : {"resolution_scale", "present_effect", "vsync", "fullscreen"}) {
      cvars.push_back(cvar);
    }
  }
  for (const auto& cvar : cvars) {
    if (rex::cvar::GetFlagInfo(cvar)) {
      g.running_values[cvar] = rex::cvar::GetFlagByName(cvar);
    }
  }

  input->SetActiveCallback(GameInputActive);
  rex::ui::RegisterBind("bind_launcher_menu", "F1", "Open the game menu (rexlauncher)",
                        [] { ToggleInGameMenu(); });
}

}  // namespace rexlauncher
