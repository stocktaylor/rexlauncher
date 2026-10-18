# rexlauncher

An in-window launch menu for rexglue recompilation projects, in the style of
PPSSPP: the game's own window opens on a menu (Play, rendering resolution,
display mode, window size, upscaling filter, VSync, controls, game-specific
options, Quit), and Play hands the window over to the game. Built for a TV or a Steam
Deck: big rows scaled to the screen, full controller navigation.

| Input | Action |
|---|---|
| D-pad / left stick | Move and change values (held: auto-repeat) |
| A | Select |
| B | Quit |
| Start | Play |
| Keyboard | Arrows, Enter/Space, Esc |
| Mouse | Hover to focus, click to select or change |

## Adding it to a project

`CMakeLists.txt`, after `rexglue_setup_target(...)`:

```cmake
add_subdirectory(../rexlauncher rexlauncher)   # path from the project to this folder
target_link_libraries(<game> PRIVATE rexlauncher)
```

The app class (`src/<name>_app.h`):

```cpp
#include <rexlauncher/launch_menu.h>

void OnConfigureFonts(ImFontAtlas* atlas) override {
  rexlauncher::ConfigureFonts(atlas);
}

std::optional<rex::PathConfig> OnFinalizePaths(
    const rex::PathConfig& defaults,
    std::function<void(rex::PathConfig)> resume) override {
  rexlauncher::Config menu;
  menu.title = "Burnout CRASH!";
  menu.subtitle = "...";
  // Optional game-specific rows: any cvar, cycled through its choices.
  // menu.extra.push_back({"Frame rate", "london_60fps",
  //                       {{"true", "60 fps"}, {"false", "30 fps (original)"}}});
  return rexlauncher::Show(imgui_drawer(), app_context(), defaults,
                           std::move(resume), std::move(menu));
}
```

London (`../London/src/london_app.h`) is the reference.

To open the same menu during the game (F1, or Back + Start on a controller):

```cpp
void OnPostSetup() override {
  rexlauncher::EnableInGameMenu(imgui_drawer(), app_context(), runtime());
}
```

## Controls

The Controls row opens a page listing the connected controllers. Each one is
set to Auto, Player 1-4 or Off (left/right). Pressing X or Y on a controller
lights up its row, to tell identical pads apart.

- Auto is the SDK's default: the Nth controller connected plays as player N,
  skipping players another controller was set to. Several controllers set to
  the same player merge, as the SDK already does for one player's devices.
- The keyboard/mouse device stays on player 1.
- Map keyboard (at the bottom of the list) opens a page listing every
  controller input with two key slots. Choose a slot and press a key (with
  Shift/Ctrl/Alt held for a combination); Esc cancels, Delete or right-click
  clears. A key already used elsewhere moves to the new input. Leaving the
  page turns the keyboard on as a virtual controller (the SDK's `mnk_mode`),
  listed as "Keyboard (virtual controller)"; choosing that row edits the keys
  again or removes it. The keys are the SDK's `keybind_*` cvars, saved with
  the other settings. `Config::controls = false` hides the page.
- Choices are saved in the `controller_ports` cvar, keyed by the controller's
  SDL GUID and, for identical controllers, their connection order
  (`<guid>#0=2,<guid>#1=off`).
- They apply through a device assignment (`rex::input::DeviceAssignment`)
  that `EnableInGameMenu` installs in the runtime's input system, so a game
  needs that call for the page to have any effect; changes made in the
  in-game menu apply at once. A game that installs its own assignment
  afterwards replaces it.

## In-game menu

- Rows: Resume, the same settings, Restart game, Quit game. B (or Esc, F1,
  Start) resumes. Restart and Quit ask for a second press, since unsaved
  progress is lost.
- While it is open the game's controller input reads as untouched (the
  input system's active callback), and the game is sent XN_SYS_UI, the
  notification the Xbox guide sends; most games pause on it. After closing,
  input stays off until every button is released, so the button that closed
  the menu doesn't reach the game.
- Display mode, window size and hot-reload cvars (e.g. London's frame rate)
  apply at once. Cvars the SDK marks `kRequiresRestart` (rendering resolution,
  upscaling filter), plus options with `restart_in_game = true`, show
  "(next start)": they are saved, and Restart game applies them now.
  Init-only cvars are greyed out.
- Back + Start is watched from the active callback, which the game calls
  whenever it polls a controller, so nothing runs per frame while the menu is
  closed. F1 is a normal SDK key bind (`bind_launcher_menu`, rebindable in
  the config).

## How it works

- `ReXApp::OnInitialize` creates the window, the presenter and ImGui, then
  calls `OnFinalizePaths`. `Show` opens the menu (an `ImGuiDialog`) and returns
  `std::nullopt`, so the SDK keeps drawing it; Play calls `resume`, which builds
  the runtime and starts the game in the same window.
- Options are SDK cvars, applied as they are chosen (display mode and window
  size change the window immediately) and saved as top-level keys of the app's
  config file (`<exe dir>/<name>.toml`, `PathConfig::config_path`), which the
  SDK loads at the next start. Cvars given on the command line are shown but
  locked.
- The upscaling filter and VSync are read when the presenter is created,
  before the menu. If one of them changed, Play restarts the process (Linux,
  `execv` of `/proc/self/exe` with the same arguments) and the second start
  skips the menu, so the change takes effect immediately.
- ImGui and SDL3 come from the runtime library (`rex::runtime`), so the menu
  uses the SDK's ImGui context and SDL event loop. Gamepads are read through
  SDL3's gamepad API.
- Fonts: PPSSPP's Roboto Condensed (SIL OFL 1.1, `assets/`), compiled in at
  configure time and baked into the SDK's font atlas by `ConfigureFonts`
  (the SDK's ImGui renderer has a fixed atlas, so they are baked large and
  drawn scaled down).

## Skipping the menu

`--launcher_menu=false`, or `REXLAUNCHER_SKIP=1` in the environment
(`rex triage` sets it, since unattended runs can't press Play).
