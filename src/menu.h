// rexlauncher - the menu dialog, shown before the game (Show) or over it
// (in-game mode, see in_game.h).

#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <rex/rex_app.h>
#include <rex/ui/imgui_dialog.h>

#include <rexlauncher/launch_menu.h>

#include "input.h"

namespace rexlauncher {

class LaunchMenu : public rex::ui::ImGuiDialog {
 public:
  LaunchMenu(rex::ui::ImGuiDrawer* drawer, rex::ui::WindowedAppContext& app_context,
             rex::PathConfig paths, std::function<void(rex::PathConfig)> resume, Config config,
             bool in_game = false);

  // In-game: back to the game (also F1 while open).
  void Resume() { Play(); }

 protected:
  // Drawing and mouse handling (menu_draw.cpp).
  void OnDraw(ImGuiIO& io) override;

 private:
  enum class RowKind { kPlay, kOption, kWindowSize, kRestart, kQuit };
  struct Row {
    RowKind kind;
    Option option;  // kOption
  };

  // Rows
  void BuildRows();
  bool AppliesAtRestart(const Option& option) const;
  bool RestartPending() const;
  bool Enabled(const Row& row) const;
  std::string Label(const Row& row) const;
  std::optional<std::string> Value(const Row& row) const;
  std::vector<std::string> ManagedCvars() const;

  // Actions
  void Change(Row& row, int dir);
  void MoveFocus(int dir);
  void Activate(size_t index);
  void Handle(Action action);
  void Play();
  void Quit();
  void Restart();
  void LeaveGameMenu();

  rex::ui::WindowedAppContext& app_context_;
  rex::PathConfig paths_;
  std::function<void(rex::PathConfig)> resume_;
  Config config_;
  bool in_game_ = false;
  std::vector<Row> rows_;
  std::map<std::string, std::string> initial_values_;
  size_t focus_ = 0;
  // In-game Restart/Quit ask for a second press.
  std::optional<size_t> confirm_;
  bool finished_ = false;
  std::string message_;

  MenuInput input_;
  std::optional<std::pair<size_t, double>> pressed_flash_;
};

}  // namespace rexlauncher
