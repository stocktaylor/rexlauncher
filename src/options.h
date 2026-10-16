// rexlauncher - the standard option rows and the cvar changes behind them.

#pragma once

#include <string>

#include <rexlauncher/launch_menu.h>

namespace rexlauncher {

// Standard rows (Config::standard_options).
Option ResolutionScaleOption();
Option DisplayModeOption();
Option UpscalingFilterOption();
Option VSyncOption();

// The label of the cvar's current choice, or its raw value if no choice has it.
std::string CurrentChoiceLabel(const Option& option);

// Sets the cvar to the next (dir > 0) or previous choice, wrapping around.
void CycleChoice(const Option& option, int dir);

// Window size row: window_width/window_height, stepped through presets.
std::string WindowSizeLabel();
void CycleWindowSize(int dir);

// Window size is otherwise read only when the window is created; resize the
// (single) SDL window now so the choice is visible.
void ApplyWindowSize();

}  // namespace rexlauncher
