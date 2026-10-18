// rexlauncher - menu input: SDL3 gamepads (the runtime's SDL instance) and
// the keyboard, turned into menu actions.

#pragma once

#include <optional>
#include <vector>

struct SDL_Gamepad;

namespace rexlauncher {

enum class Action { kUp, kDown, kLeft, kRight, kAccept, kBack, kStart };

struct PadState {
  bool up = false, down = false, left = false, right = false;
  bool accept = false, back = false, start = false;
};

// The menu's own gamepad handles, merged into one state.
class Gamepads {
 public:
  Gamepads();
  ~Gamepads();
  Gamepads(const Gamepads&) = delete;
  Gamepads& operator=(const Gamepads&) = delete;

  PadState Read();

 private:
  void Refresh();

  bool initialized_ = false;
  int refresh_countdown_ = 0;
  std::vector<SDL_Gamepad*> pads_;
};

// Gamepad, keyboard and auto-repeat state for one open menu.
class MenuInput {
 public:
  // The actions since the last call; call once per frame.
  std::vector<Action> Poll();

 private:
  Gamepads gamepads_;
  PadState previous_{};
  bool first_read_ = true;
  std::optional<Action> held_;
  double next_repeat_ = 0.0;
};

// Whether any button or trigger is held on the pad.
bool PadHasInput(SDL_Gamepad* pad);

// Reads the pads the SDK's input driver has open (without opening any):
// whether anything is pressed, and whether Back + Start are held together.
bool ReadPadsForMenu(bool& chord);

}  // namespace rexlauncher
