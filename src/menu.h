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

#include <imgui.h>

#include <rex/rex_app.h>
#include <rex/ui/imgui_dialog.h>

#include <rexlauncher/launch_menu.h>

#include "input.h"
#include "ports.h"

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
  // kKeyboard: the keyboard listed as a virtual controller once mapped;
  // kMapKeyboard: the button that maps it.
  enum class RowKind {
    kPlay, kOption, kWindowSize, kControls, kRestart, kQuit, kBack, kPad, kKeyboard, kMapKeyboard
  };
  struct Row {
    RowKind kind;
    Option option;  // kOption
    Pad pad;        // kPad
  };
  // The Controls page lists the connected controllers; the Keyboard page maps
  // keys to controller inputs.
  enum class Page { kMain, kControls, kKeyboard };
  // The Keyboard page's rows: Done, one per KeyActions(), Reset, Remove.
  enum class KeyRow { kDone, kAction, kReset, kRemove };

  // Rows
  void BuildRows();
  void BuildPadRows();
  void RefreshPads();
  bool AppliesAtRestart(const Option& option) const;
  bool RestartPending() const;
  bool Enabled(const Row& row) const;
  // Left/right (and clicking the value) step the row's value.
  bool Cycles(const Row& row) const;
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
  void OpenControls();
  void CloseControls();

  // Keyboard page (keyboard_page.cpp).
  void OpenKeyboard();
  void CloseKeyboard(bool keep_mapped);
  size_t KeyRowCount() const;
  KeyRow KeyRowKind(size_t index) const;
  bool KeyRowEnabled(size_t index) const;
  void HandleKeyboard(Action action);
  void ActivateKeyRow(size_t index);
  void StartCapture(size_t row, size_t slot);
  // Key capture and Delete/Backspace; true while keys must not reach Handle.
  bool UpdateKeyCapture();
  void DrawKeyboardPage(ImDrawList* dl, ImGuiIO& io, float s, float margin, double now);

  rex::ui::WindowedAppContext& app_context_;
  rex::PathConfig paths_;
  std::function<void(rex::PathConfig)> resume_;
  Config config_;
  bool in_game_ = false;
  // The rows shown: main_rows_, or the Controls page's.
  std::vector<Row> rows_;
  std::vector<Row> main_rows_;
  Page page_ = Page::kMain;
  size_t main_focus_ = 0;
  std::vector<Pad> pads_;
  // When each pad (by key) last had a button down, to light its row.
  std::map<std::string, double> pad_pressed_;
  std::map<std::string, std::string> initial_values_;
  size_t focus_ = 0;
  // In-game Restart/Quit ask for a second press.
  std::optional<size_t> confirm_;
  bool finished_ = false;
  std::string message_;

  // Keyboard page.
  size_t key_focus_ = 0;
  size_t key_slot_ = 0;
  float key_scroll_ = 0.0f;  // rows scrolled off the top
  bool capturing_ = false;
  // Capture ended this frame: its click must not start another.
  bool capture_ended_ = false;
  // The key that bound or cancelled: menu keys stay off until it is let go.
  ImGuiKey release_key_ = ImGuiKey_None;

  MenuInput input_;
  std::optional<std::pair<size_t, double>> pressed_flash_;
};

}  // namespace rexlauncher
