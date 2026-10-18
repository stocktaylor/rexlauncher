// rexlauncher - controller ports: which player each connected controller
// plays as. Chosen on the Controls page, saved in the controller_ports cvar
// and applied by a device assignment installed in the SDK's input system.
//
// A controller is known by its key, "<SDL GUID>#<n>", where n counts the
// connected controllers with the same GUID that were connected before it, so
// two identical pads keep apart. Each key is set to Auto, a player, or Off.
// Auto keeps the SDK's behaviour (the Nth pad connected plays as player N)
// but skips players another pad was set to.

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace rex::input {
class InputSystem;
}  // namespace rex::input

namespace rexlauncher {

inline constexpr const char* kPortsCvar = "controller_ports";

// A port setting: kAuto, a player (0-3), or kOff.
inline constexpr int kAuto = -1;
inline constexpr int kOff = -2;

struct Pad {
  std::string key;
  std::string name;
  bool active = false;  // a button is held right now
};

// The connected controllers, in connection order. Activity is only seen on
// pads somebody has open (the menu's Gamepads keeps every pad open).
std::vector<Pad> ConnectedPads();

int PortSetting(const std::string& key);

// Steps the pad's setting through Auto, Player 1-4, Off and applies it.
void CyclePortSetting(const std::string& key, int dir);

// The player (0-3) the pad's input goes to, or nullopt when it is off or no
// player is left for it. `pads` is ConnectedPads(); before the game starts
// they are the order the SDK will number them in.
std::optional<int> CurrentPort(const std::vector<Pad>& pads, const std::string& key);

// Routes the SDK's devices by the settings (call before the guest polls
// input). The keyboard/mouse and the NOP stand-in stay on player 1.
void InstallPortAssignment(rex::input::InputSystem* input);

}  // namespace rexlauncher
