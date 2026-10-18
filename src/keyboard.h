// rexlauncher - the keyboard as a virtual controller: the SDK's keyboard/mouse
// driver (the mnk_mode cvar) and its keybind_* cvars, set on the Controls
// page's keyboard mapping page (keyboard_page.cpp).
//
// A bind cvar holds a comma-separated list of keys, each optionally with
// modifiers ("Space,Semicolon", "Shift+Up"); the page shows the first two as
// Key 1 and Key 2.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

namespace rexlauncher {

inline constexpr const char* kKeyboardCvar = "mnk_mode";

struct KeyAction {
  const char* label;  // the controller input, as listed on the page
  const char* cvar;
  ImU32 color;  // face button color, 0 = none
};

// Every controller input, in the page's order.
const std::vector<KeyAction>& KeyActions();

// Whether the SDK has the keyboard driver (mnk_mode) at all.
bool KeyboardAvailable();
// Whether the keyboard is on as a virtual controller.
bool KeyboardMapped();
void SetKeyboardMapped(bool mapped);
// mnk_mode or a bind given on the command line (e.g. by Blade for a game it
// launched): shown, not changed.
bool KeyboardLocked();

// mnk_mode and the bind cvars, to save to the config file.
std::vector<std::string> KeyboardCvars();

// The keys bound to an action, in order.
std::vector<std::string> BoundKeys(const char* cvar);
bool AnyKeyBound();

// Sets key `slot` (0 or 1) of the action; an empty key clears it. A key bound
// to another action moves here: returns that action's label.
std::optional<std::string> BindKey(const char* cvar, size_t slot, const std::string& key);

void ResetKeyBinds();

// The key first pressed this frame, as a bind name with the modifiers held
// ("Shift+Up"), and its ImGui key. Bare modifiers and Escape don't count.
std::optional<std::pair<std::string, ImGuiKey>> PressedKey();

}  // namespace rexlauncher
