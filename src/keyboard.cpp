// rexlauncher - the keyboard as a virtual controller (see keyboard.h).

#include "keyboard.h"

#include <algorithm>
#include <string_view>

#include <rex/cvar.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/virtual_key.h>

#include "settings.h"
#include "theme.h"

namespace rexlauncher {

namespace {

using rex::ui::VirtualKey;

// ImGui's key to the SDK's, whose names the bind cvars use. Modifiers are
// left out: they only prefix another key.
VirtualKey ToVirtualKey(ImGuiKey key) {
  if (key >= ImGuiKey_A && key <= ImGuiKey_Z) {
    return VirtualKey(uint16_t(VirtualKey::kA) + (key - ImGuiKey_A));
  }
  if (key >= ImGuiKey_0 && key <= ImGuiKey_9) {
    return VirtualKey(uint16_t(VirtualKey::k0) + (key - ImGuiKey_0));
  }
  if (key >= ImGuiKey_F1 && key <= ImGuiKey_F24) {
    return VirtualKey(uint16_t(VirtualKey::kF1) + (key - ImGuiKey_F1));
  }
  if (key >= ImGuiKey_Keypad0 && key <= ImGuiKey_Keypad9) {
    return VirtualKey(uint16_t(VirtualKey::kNumpad0) + (key - ImGuiKey_Keypad0));
  }
  switch (key) {
    case ImGuiKey_Space: return VirtualKey::kSpace;
    case ImGuiKey_Enter: return VirtualKey::kReturn;
    case ImGuiKey_KeypadEnter: return VirtualKey::kReturn;
    case ImGuiKey_Tab: return VirtualKey::kTab;
    case ImGuiKey_Backspace: return VirtualKey::kBack;
    case ImGuiKey_Delete: return VirtualKey::kDelete;
    case ImGuiKey_Insert: return VirtualKey::kInsert;
    case ImGuiKey_Home: return VirtualKey::kHome;
    case ImGuiKey_End: return VirtualKey::kEnd;
    case ImGuiKey_PageUp: return VirtualKey::kPrior;
    case ImGuiKey_PageDown: return VirtualKey::kNext;
    case ImGuiKey_LeftArrow: return VirtualKey::kLeft;
    case ImGuiKey_RightArrow: return VirtualKey::kRight;
    case ImGuiKey_UpArrow: return VirtualKey::kUp;
    case ImGuiKey_DownArrow: return VirtualKey::kDown;
    case ImGuiKey_GraveAccent: return VirtualKey::kOem3;
    case ImGuiKey_Minus: return VirtualKey::kOemMinus;
    case ImGuiKey_Equal: return VirtualKey::kOemPlus;
    case ImGuiKey_LeftBracket: return VirtualKey::kOem4;
    case ImGuiKey_RightBracket: return VirtualKey::kOem6;
    case ImGuiKey_Backslash: return VirtualKey::kOem5;
    case ImGuiKey_Semicolon: return VirtualKey::kOem1;
    case ImGuiKey_Apostrophe: return VirtualKey::kOem7;
    case ImGuiKey_Comma: return VirtualKey::kOemComma;
    case ImGuiKey_Period: return VirtualKey::kOemPeriod;
    case ImGuiKey_Slash: return VirtualKey::kOem2;
    case ImGuiKey_KeypadDecimal: return VirtualKey::kDecimal;
    case ImGuiKey_KeypadDivide: return VirtualKey::kDivide;
    case ImGuiKey_KeypadMultiply: return VirtualKey::kMultiply;
    case ImGuiKey_KeypadSubtract: return VirtualKey::kSubtract;
    case ImGuiKey_KeypadAdd: return VirtualKey::kAdd;
    default: return VirtualKey::kNone;
  }
}

std::string_view Trim(std::string_view s) {
  while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
  while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
  return s;
}

void SetKeys(const char* cvar, const std::vector<std::string>& keys) {
  std::string value;
  for (const auto& key : keys) {
    if (key.empty()) {
      continue;
    }
    if (!value.empty()) {
      value += ',';
    }
    value += key;
  }
  rex::cvar::SetFlagByName(cvar, value);
}

}  // namespace

const std::vector<KeyAction>& KeyActions() {
  static const std::vector<KeyAction> actions = {
      {"A", "keybind_a", kButtonA},
      {"B", "keybind_b", kButtonB},
      {"X", "keybind_x", kButtonX},
      {"Y", "keybind_y", kButtonY},
      {"Left bumper", "keybind_left_shoulder", 0},
      {"Right bumper", "keybind_right_shoulder", 0},
      {"Left trigger", "keybind_left_trigger", 0},
      {"Right trigger", "keybind_right_trigger", 0},
      {"Back", "keybind_back", 0},
      {"Start", "keybind_start", 0},
      {"Guide", "keybind_guide", 0},
      {"D-pad up", "keybind_dpad_up", 0},
      {"D-pad down", "keybind_dpad_down", 0},
      {"D-pad left", "keybind_dpad_left", 0},
      {"D-pad right", "keybind_dpad_right", 0},
      {"Left stick up", "keybind_lstick_up", 0},
      {"Left stick down", "keybind_lstick_down", 0},
      {"Left stick left", "keybind_lstick_left", 0},
      {"Left stick right", "keybind_lstick_right", 0},
      {"Left stick click", "keybind_lstick_press", 0},
      {"Right stick up", "keybind_rstick_up", 0},
      {"Right stick down", "keybind_rstick_down", 0},
      {"Right stick left", "keybind_rstick_left", 0},
      {"Right stick right", "keybind_rstick_right", 0},
      {"Right stick click", "keybind_rstick_press", 0},
  };
  return actions;
}

bool KeyboardAvailable() { return rex::cvar::GetFlagInfo(kKeyboardCvar) != nullptr; }

bool KeyboardMapped() {
  return KeyboardAvailable() && rex::cvar::GetFlagByName(kKeyboardCvar) == "true";
}

void SetKeyboardMapped(bool mapped) {
  rex::cvar::SetFlagByName(kKeyboardCvar, mapped ? "true" : "false");
}

bool KeyboardLocked() { return IsLockedByCommandLine(kKeyboardCvar); }

std::vector<std::string> KeyboardCvars() {
  std::vector<std::string> cvars{kKeyboardCvar};
  for (const auto& action : KeyActions()) {
    cvars.push_back(action.cvar);
  }
  return cvars;
}

std::vector<std::string> BoundKeys(const char* cvar) {
  std::vector<std::string> keys;
  std::string value = rex::cvar::GetFlagByName(cvar);
  std::string_view rest = value;
  while (!rest.empty()) {
    size_t comma = rest.find(',');
    std::string_view key = Trim(rest.substr(0, comma));
    rest = comma == std::string_view::npos ? std::string_view() : rest.substr(comma + 1);
    if (!key.empty()) {
      keys.emplace_back(key);
    }
  }
  return keys;
}

bool AnyKeyBound() {
  return std::any_of(KeyActions().begin(), KeyActions().end(),
                     [](const KeyAction& action) { return !BoundKeys(action.cvar).empty(); });
}

std::optional<std::string> BindKey(const char* cvar, size_t slot, const std::string& key) {
  std::optional<std::string> moved_from;
  if (!key.empty()) {
    for (const auto& action : KeyActions()) {
      if (std::string_view(action.cvar) == cvar) {
        continue;
      }
      auto keys = BoundKeys(action.cvar);
      if (std::erase(keys, key) > 0) {
        SetKeys(action.cvar, keys);
        moved_from = action.label;
      }
    }
  }
  auto keys = BoundKeys(cvar);
  if (keys.size() < 2) {
    keys.resize(2);
  }
  if (!key.empty()) {
    // Already in the other slot: it moves.
    std::replace(keys.begin(), keys.end(), key, std::string());
  }
  keys[slot] = key;
  SetKeys(cvar, keys);
  return moved_from;
}

void ResetKeyBinds() {
  for (const auto& action : KeyActions()) {
    if (const auto* info = rex::cvar::GetFlagInfo(action.cvar)) {
      rex::cvar::SetFlagByName(action.cvar, info->default_value);
    }
  }
}

std::optional<std::pair<std::string, ImGuiKey>> PressedKey() {
  for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
    auto key = ImGuiKey(k);
    if (key == ImGuiKey_Escape || !ImGui::IsKeyPressed(key, false)) {
      continue;
    }
    std::string name = rex::ui::VirtualKeyToString(ToVirtualKey(key));
    if (name.empty()) {
      continue;  // a modifier, mouse button or key the binds can't name
    }
    const ImGuiIO& io = ImGui::GetIO();
    std::string mods;
    if (io.KeyCtrl) mods += "Ctrl+";
    if (io.KeyShift) mods += "Shift+";
    if (io.KeyAlt) mods += "Alt+";
    return std::make_pair(mods + name, key);
  }
  return std::nullopt;
}

}  // namespace rexlauncher
