// rexlauncher - the menu's rows and what choosing them does.

#include "menu.h"

#include <algorithm>

#include <imgui.h>

#include <rex/cvar.h>
#include <rex/ui/windowed_app_context.h>

#include "in_game.h"
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
  rows_.push_back({RowKind::kPlay, {}});
  if (config_.standard_options) {
    rows_.push_back({RowKind::kOption, ResolutionScaleOption()});
    rows_.push_back({RowKind::kOption, DisplayModeOption()});
    rows_.push_back({RowKind::kWindowSize, {}});
    rows_.push_back({RowKind::kOption, UpscalingFilterOption()});
    rows_.push_back({RowKind::kOption, VSyncOption()});
  }
  for (const auto& option : config_.extra) {
    rows_.push_back({RowKind::kOption, option});
  }
  if (in_game_) {
    rows_.push_back({RowKind::kRestart, {}});
  }
  rows_.push_back({RowKind::kQuit, {}});
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
    default:
      return true;
  }
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
    default:
      return std::nullopt;
  }
}

std::vector<std::string> LaunchMenu::ManagedCvars() const {
  std::vector<std::string> cvars;
  for (const auto& row : rows_) {
    if (row.kind == RowKind::kOption) {
      cvars.push_back(row.option.cvar);
    } else if (row.kind == RowKind::kWindowSize) {
      cvars.push_back("window_width");
      cvars.push_back("window_height");
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
      if (in_game_) {
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

}  // namespace rexlauncher
