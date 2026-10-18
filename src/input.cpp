// rexlauncher - menu input. Gamepads: D-pad/left stick move and change, A
// select, B back, Start play. Keyboard: arrows, Enter/Space, Esc.

#include "input.h"

#include <algorithm>
#include <utility>

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_stdinc.h>

#include <imgui.h>

namespace rexlauncher {

// --- Gamepads -------------------------------------------------------------------

Gamepads::Gamepads() {
  initialized_ = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
}

Gamepads::~Gamepads() {
  for (auto* pad : pads_) {
    SDL_CloseGamepad(pad);
  }
  if (initialized_) {
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
  }
}

PadState Gamepads::Read() {
  PadState state;
  if (!initialized_) {
    return state;
  }
  if (refresh_countdown_-- <= 0) {
    Refresh();
    refresh_countdown_ = 30;
  }
  SDL_UpdateGamepads();
  for (auto* pad : pads_) {
    auto button = [pad](SDL_GamepadButton b) { return SDL_GetGamepadButton(pad, b); };
    float x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
    float y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
    state.up |= button(SDL_GAMEPAD_BUTTON_DPAD_UP) || y < -0.6f;
    state.down |= button(SDL_GAMEPAD_BUTTON_DPAD_DOWN) || y > 0.6f;
    state.left |= button(SDL_GAMEPAD_BUTTON_DPAD_LEFT) || x < -0.6f;
    state.right |= button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || x > 0.6f;
    state.accept |= button(SDL_GAMEPAD_BUTTON_SOUTH);
    state.back |= button(SDL_GAMEPAD_BUTTON_EAST);
    state.start |= button(SDL_GAMEPAD_BUTTON_START);
  }
  return state;
}

void Gamepads::Refresh() {
  int count = 0;
  SDL_JoystickID* ids = SDL_GetGamepads(&count);
  for (int i = 0; ids && i < count; ++i) {
    bool open = std::any_of(pads_.begin(), pads_.end(),
                            [&](SDL_Gamepad* pad) { return SDL_GetGamepadID(pad) == ids[i]; });
    if (!open) {
      if (SDL_Gamepad* pad = SDL_OpenGamepad(ids[i])) {
        pads_.push_back(pad);
      }
    }
  }
  SDL_free(ids);
  // Drop disconnected pads.
  std::erase_if(pads_, [](SDL_Gamepad* pad) {
    if (SDL_GamepadConnected(pad)) {
      return false;
    }
    SDL_CloseGamepad(pad);
    return true;
  });
}

// --- Menu actions ---------------------------------------------------------------

std::vector<Action> MenuInput::Poll() {
  std::vector<Action> actions;
  double now = ImGui::GetTime();

  PadState pad = gamepads_.Read();
  if (first_read_) {
    // Ignore buttons already held when the menu opened (e.g. the A press that
    // launched the game from Steam).
    previous_ = pad;
    first_read_ = false;
  }
  if (pad.accept && !previous_.accept) actions.push_back(Action::kAccept);
  if (pad.back && !previous_.back) actions.push_back(Action::kBack);
  if (pad.start && !previous_.start) actions.push_back(Action::kStart);

  // Held direction with console-style auto-repeat.
  std::optional<Action> dir;
  if (pad.up) dir = Action::kUp;
  else if (pad.down) dir = Action::kDown;
  else if (pad.left) dir = Action::kLeft;
  else if (pad.right) dir = Action::kRight;
  if (dir != held_) {
    held_ = dir;
    next_repeat_ = now + 0.38;
    if (dir) actions.push_back(*dir);
  } else if (dir && now >= next_repeat_) {
    next_repeat_ = now + 0.11;
    actions.push_back(*dir);
  }
  previous_ = pad;

  static constexpr std::pair<ImGuiKey, Action> kKeys[] = {
      {ImGuiKey_UpArrow, Action::kUp},       {ImGuiKey_DownArrow, Action::kDown},
      {ImGuiKey_LeftArrow, Action::kLeft},   {ImGuiKey_RightArrow, Action::kRight},
      {ImGuiKey_Enter, Action::kAccept},     {ImGuiKey_KeypadEnter, Action::kAccept},
      {ImGuiKey_Space, Action::kAccept},     {ImGuiKey_Escape, Action::kBack},
  };
  for (const auto& [key, action] : kKeys) {
    if (ImGui::IsKeyPressed(key, true)) {
      actions.push_back(action);
    }
  }
  return actions;
}

// --- The SDK's pads (in-game chord) ---------------------------------------------

bool PadHasInput(SDL_Gamepad* pad) {
  for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b) {
    if (SDL_GetGamepadButton(pad, SDL_GamepadButton(b))) {
      return true;
    }
  }
  return SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 8000 ||
         SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 8000;
}

bool ReadPadsForMenu(bool& chord) {
  chord = false;
  bool any = false;
  int count = 0;
  SDL_JoystickID* ids = SDL_GetGamepads(&count);
  for (int i = 0; ids && i < count; ++i) {
    SDL_Gamepad* pad = SDL_GetGamepadFromID(ids[i]);
    if (!pad) {
      continue;
    }
    chord |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_BACK) &&
             SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_START);
    any |= PadHasInput(pad);
  }
  SDL_free(ids);
  return any;
}

}  // namespace rexlauncher
