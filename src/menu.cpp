// rexlauncher - the menu's rows and what choosing them does.

#include "menu.h"

#include <algorithm>

#include <imgui.h>

#include <rex/cvar.h>
#include <rex/ui/windowed_app_context.h>

#include "in_game.h"
#include "keyboard.h"
#include "options.h"
#include "settings.h"

namespace rexlauncher {

LaunchMenu::LaunchMenu(rex::ui::ImGuiDrawer* drawer, rex::ui::WindowedAppContext& app_context,
                       rex::PathConfig paths, std::function<void(rex::PathConfig)> resume,
                       Config config, bool in_game)
    : ImGuiDialog(drawer),
      app_context_(app_context),
      paths_(std::move(paths)),
      resume_(std::move(resume)),
      config_(std::move(config)),
      in_game_(in_game) {
  BuildRows();
  for (const auto& row : rows_) {
    if (row.kind == RowKind::kOption && AppliesAtRestart(row.option)) {
      const auto& running = State().running_values;
      auto it = running.find(row.option.cvar);
      initial_values_[row.option.cvar] = in_game_ && it != running.end()
                                             ? it->second
                                             : rex::cvar::GetFlagByName(row.option.cvar);
    }
  }
}

// --- Rows -----------------------------------------------------------------------

void LaunchMenu::BuildRows() {
  auto& rows = main_rows_;
  rows.push_back({RowKind::kPlay, {}});
  if (config_.standard_options) {
    rows.push_back({RowKind::kOption, ResolutionScaleOption()});
    if (config_.display_options) {
      rows.push_back({RowKind::kOption, DisplayModeOption()});
      rows.push_back({RowKind::kWindowSize, {}});
    }
    rows.push_back({RowKind::kOption, UpscalingFilterOption()});
    rows.push_back({RowKind::kOption, VSyncOption()});
  }
  if (config_.controls) {
    rows.push_back({RowKind::kControls, {}});
  }
  for (const auto& option : config_.extra) {
    rows.push_back({RowKind::kOption, option});
  }
  if (in_game_) {
    rows.push_back({RowKind::kRestart, {}});
  }
  rows.push_back({RowKind::kQuit, {}});
  rows_ = main_rows_;
}

void LaunchMenu::BuildPadRows() {
  rows_.clear();
  rows_.push_back({RowKind::kBack, {}});
  for (size_t i = 0; i < pads_.size(); ++i) {
    Row row{RowKind::kPad, {}, pads_[i]};
    // Number pads with the same name: "Xbox Controller 2".
    auto same_name = [&](const Pad& pad) { return pad.name == pads_[i].name; };
    if (std::count_if(pads_.begin(), pads_.end(), same_name) > 1) {
      auto before = std::count_if(pads_.begin(), pads_.begin() + i, same_name);
      row.pad.name += " " + std::to_string(before + 1);
    }
    rows_.push_back(std::move(row));
  }
  if (KeyboardAvailable()) {
    rows_.push_back({KeyboardMapped() ? RowKind::kKeyboard : RowKind::kMapKeyboard, {}});
  }
}

// Called every frame: notes which pads are pressed and, on the Controls page,
// follows pads being connected and disconnected.
void LaunchMenu::RefreshPads() {
  std::vector<Pad> pads = ConnectedPads();
  double now = ImGui::GetTime();
  for (const auto& pad : pads) {
    if (pad.active) {
      pad_pressed_[pad.key] = now;
    }
  }
  bool changed = !std::equal(pads.begin(), pads.end(), pads_.begin(), pads_.end(),
                             [](const Pad& a, const Pad& b) { return a.key == b.key; });
  pads_ = std::move(pads);
  if (!changed || page_ != Page::kControls) {
    return;
  }
  RowKind focused_kind = rows_[focus_].kind;
  std::string focused = focused_kind == RowKind::kPad ? rows_[focus_].pad.key : "";
  BuildPadRows();
  focus_ = 0;
  for (size_t i = 0; i < rows_.size(); ++i) {
    if (rows_[i].kind == focused_kind && rows_[i].pad.key == focused) {
      focus_ = i;
    }
  }
}

// Whether a change shows only after the process restarts: before the game,
// only for cvars read before the menu appears; in-game, also for those the
// game or the SDK reads once at start.
bool LaunchMenu::AppliesAtRestart(const Option& option) const {
  if (option.requires_restart) {
    return true;
  }
  if (!in_game_) {
    return false;
  }
  if (option.restart_in_game) {
    return true;
  }
  const auto* info = rex::cvar::GetFlagInfo(option.cvar);
  return info && info->lifecycle != rex::cvar::Lifecycle::kHotReload;
}

bool LaunchMenu::RestartPending() const {
  return std::any_of(initial_values_.begin(), initial_values_.end(), [](const auto& entry) {
    return rex::cvar::GetFlagByName(entry.first) != entry.second;
  });
}

bool LaunchMenu::Enabled(const Row& row) const {
  switch (row.kind) {
    case RowKind::kWindowSize:
      return rex::cvar::GetFlagByName("fullscreen") != "true" &&
             !IsLockedByCommandLine("window_width");
    case RowKind::kOption: {
      const auto* info = rex::cvar::GetFlagInfo(row.option.cvar);
      if (!info || IsLockedByCommandLine(row.option.cvar)) {
        return false;
      }
      // Init-only cvars are refused once the runtime is up.
      return !in_game_ || info->lifecycle != rex::cvar::Lifecycle::kInitOnly;
    }
    case RowKind::kPad:
      return !IsLockedByCommandLine(kPortsCvar);
    default:
      return true;
  }
}

bool LaunchMenu::Cycles(const Row& row) const {
  return row.kind == RowKind::kOption || row.kind == RowKind::kWindowSize ||
         row.kind == RowKind::kPad;
}

std::string LaunchMenu::Label(const Row& row) const {
  switch (row.kind) {
    case RowKind::kPlay:
      return in_game_ ? "Resume" : "Play";
    case RowKind::kRestart:
      return confirm_ == std::optional<size_t>(&row - rows_.data())
                 ? "Restart game: press again (unsaved progress is lost)"
                 : "Restart game";
    case RowKind::kQuit:
      if (!in_game_) {
        return "Quit";
      }
      return confirm_ == std::optional<size_t>(&row - rows_.data())
                 ? "Quit game: press again (unsaved progress is lost)"
                 : "Quit game";
    case RowKind::kWindowSize:
      return "Window size";
    case RowKind::kOption:
      return row.option.label;
    case RowKind::kControls:
      return "Controls";
    case RowKind::kBack:
      return "Back";
    case RowKind::kPad:
      return row.pad.name;
    case RowKind::kKeyboard:
      return "Keyboard (virtual controller)";
    case RowKind::kMapKeyboard:
      return "Map keyboard";
  }
  return {};
}

std::optional<std::string> LaunchMenu::Value(const Row& row) const {
  switch (row.kind) {
    case RowKind::kWindowSize:
      return WindowSizeLabel();
    case RowKind::kOption: {
      if (!rex::cvar::GetFlagInfo(row.option.cvar)) {
        return std::string("Unavailable");
      }
      std::string shown = CurrentChoiceLabel(row.option);
      if (IsLockedByCommandLine(row.option.cvar)) {
        shown += "  (command line)";
      } else if (auto it = initial_values_.find(row.option.cvar);
                 it != initial_values_.end() &&
                 it->second != rex::cvar::GetFlagByName(row.option.cvar)) {
        shown += in_game_ ? "  (next start)" : "  (restarts)";
      }
      return shown;
    }
    case RowKind::kControls: {
      size_t count = pads_.size() + (KeyboardMapped() ? 1 : 0);
      if (count == 0) {
        return std::string("No controllers");
      }
      return std::to_string(count) + (count == 1 ? " controller" : " controllers");
    }
    case RowKind::kKeyboard:
      // The keyboard/mouse device always feeds player 1 (ports.h).
      return std::string(KeyboardLocked() ? "Player 1  (command line)" : "Player 1");
    case RowKind::kMapKeyboard:
      return std::string("Play with the keyboard");
    case RowKind::kPad: {
      int setting = PortSetting(row.pad.key);
      std::optional<int> port = CurrentPort(pads_, row.pad.key);
      std::string shown = setting == kOff ? "Off"
                          : port         ? "Player " + std::to_string(*port + 1)
                                         : "Not used";
      if (setting == kAuto) {
        shown += "  (auto)";
      }
      if (IsLockedByCommandLine(kPortsCvar)) {
        shown += "  (command line)";
      }
      return shown;
    }
    default:
      return std::nullopt;
  }
}

std::vector<std::string> LaunchMenu::ManagedCvars() const {
  std::vector<std::string> cvars;
  for (const auto& row : main_rows_) {
    if (row.kind == RowKind::kOption) {
      cvars.push_back(row.option.cvar);
    } else if (row.kind == RowKind::kWindowSize) {
      cvars.push_back("window_width");
      cvars.push_back("window_height");
    } else if (row.kind == RowKind::kControls) {
      cvars.push_back(kPortsCvar);
      if (KeyboardAvailable()) {
        auto keyboard = KeyboardCvars();
        cvars.insert(cvars.end(), keyboard.begin(), keyboard.end());
      }
    }
  }
  return cvars;
}

// --- Actions --------------------------------------------------------------------

void LaunchMenu::Change(Row& row, int dir) {
  if (!Enabled(row)) {
    return;
  }
  if (row.kind == RowKind::kWindowSize) {
    CycleWindowSize(dir);
  } else if (row.kind == RowKind::kOption) {
    CycleChoice(row.option, dir);
  } else if (row.kind == RowKind::kPad) {
    CyclePortSetting(row.pad.key, dir);
  }
}

void LaunchMenu::MoveFocus(int dir) {
  confirm_.reset();
  int n = int(rows_.size());
  int i = int(focus_);
  for (int step = 0; step < n; ++step) {
    i = (i + dir + n) % n;
    if (Enabled(rows_[i])) {
      focus_ = size_t(i);
      return;
    }
  }
}

void LaunchMenu::Activate(size_t index) {
  pressed_flash_ = std::make_pair(index, ImGui::GetTime());
  switch (rows_[index].kind) {
    case RowKind::kPlay:
      Play();
      break;
    case RowKind::kControls:
      OpenControls();
      break;
    case RowKind::kBack:
      CloseControls();
      break;
    case RowKind::kKeyboard:
    case RowKind::kMapKeyboard:
      OpenKeyboard();
      break;
    case RowKind::kRestart:
    case RowKind::kQuit:
      if (in_game_ && confirm_ != index) {
        confirm_ = index;
        break;
      }
      if (rows_[index].kind == RowKind::kRestart) {
        Restart();
      } else {
        Quit();
      }
      break;
    default:
      Change(rows_[index], 1);
      break;
  }
}

void LaunchMenu::Handle(Action action) {
  if (page_ == Page::kKeyboard) {
    HandleKeyboard(action);
    return;
  }
  switch (action) {
    case Action::kUp:
      MoveFocus(-1);
      break;
    case Action::kDown:
      MoveFocus(1);
      break;
    case Action::kLeft:
      Change(rows_[focus_], -1);
      break;
    case Action::kRight:
      Change(rows_[focus_], 1);
      break;
    case Action::kAccept:
      Activate(focus_);
      break;
    case Action::kBack:
      if (page_ == Page::kControls) {
        CloseControls();
      } else if (in_game_) {
        Play();  // B resumes
      } else {
        Quit();
      }
      break;
    case Action::kStart:
      Play();
      break;
  }
}

void LaunchMenu::Play() {
  if (finished_) {
    return;
  }
  if (page_ == Page::kKeyboard) {
    CloseKeyboard(true);  // keys mapped, then F1/Start: keep the keyboard on
  }
  SaveToConfig(paths_.config_path, ManagedCvars());
  if (in_game_) {
    // Changes waiting for a restart stay saved for the next start.
    LeaveGameMenu();
    return;
  }
  if (RestartPending()) {
    RestartProcess();  // returns only if it failed; carry on without it
  }
  finished_ = true;
  Close();
  // Start the game after this frame, outside the ImGui draw.
  app_context_.CallInUIThreadDeferred(
      [resume = std::move(resume_), paths = paths_]() { resume(paths); });
}

void LaunchMenu::Quit() {
  if (finished_) {
    return;
  }
  SaveToConfig(paths_.config_path, ManagedCvars());
  if (in_game_) {
    LeaveGameMenu();
  } else {
    finished_ = true;
    Close();
  }
  auto* context = &app_context_;
  context->CallInUIThreadDeferred([context]() { context->QuitFromUIThread(); });
}

void LaunchMenu::Restart() {
  SaveToConfig(paths_.config_path, ManagedCvars());
  RestartProcess();  // returns only if it failed
  confirm_.reset();
  message_ = "Could not restart; changes apply the next time the game starts.";
}

// In-game: closes the menu and hands input back to the game.
void LaunchMenu::LeaveGameMenu() {
  finished_ = true;
  Close();
  auto& g = State();
  if (g.menu == this) {
    g.menu = nullptr;
  }
  g.releasing = true;
  g.open = false;
  SendSystemUINotification(false);
}

void LaunchMenu::OpenControls() {
  confirm_.reset();
  main_focus_ = focus_;
  page_ = Page::kControls;
  BuildPadRows();
  focus_ = rows_.size() > 1 ? 1 : 0;  // the first controller
}

void LaunchMenu::CloseControls() {
  page_ = Page::kMain;
  rows_ = main_rows_;
  focus_ = main_focus_;
}

}  // namespace rexlauncher
