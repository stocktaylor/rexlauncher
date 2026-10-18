// rexlauncher - controller ports (see ports.h).

#include "ports.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string_view>

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_guid.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_stdinc.h>

#include <rex/cvar.h>
#include <rex/input/device_assignment.h>
#include <rex/input/input_system.h>

#include "input.h"

REXCVAR_DEFINE_STRING(controller_ports, "", "Launcher",
                      "Controller to player assignments, set on the launch menu's Controls page "
                      "(rexlauncher)");

namespace rexlauncher {

namespace {

using rex::input::DeviceId;
using rex::input::DeviceInfo;
using rex::input::kMaxGuestUsers;

constexpr int kPlayers = int(kMaxGuestUsers);

// One physical controller and the ordinal the SDK numbered it with.
struct Slot {
  std::string key;
  uint32_t ordinal;
};

std::string MakeKey(const std::string& guid, int earlier) {
  return guid + "#" + std::to_string(earlier);
}

// "<key>=<1-4|off>,...": players are 1-based in the file. Auto is the default
// and is not written.
std::map<std::string, int> ParseSettings(std::string_view text) {
  std::map<std::string, int> settings;
  while (!text.empty()) {
    size_t comma = text.find(',');
    std::string_view entry = text.substr(0, comma);
    text = comma == std::string_view::npos ? std::string_view() : text.substr(comma + 1);
    size_t eq = entry.find('=');
    if (eq == std::string_view::npos) {
      continue;
    }
    std::string key(entry.substr(0, eq));
    std::string_view value = entry.substr(eq + 1);
    if (value == "off") {
      settings[key] = kOff;
    } else if (value.size() == 1 && value[0] >= '1' && value[0] < '1' + kPlayers) {
      settings[key] = value[0] - '1';
    }
  }
  return settings;
}

std::string FormatSettings(const std::map<std::string, int>& settings) {
  std::string text;
  for (const auto& [key, setting] : settings) {
    if (setting == kAuto) {
      continue;
    }
    if (!text.empty()) {
      text += ',';
    }
    text += key + "=" + (setting == kOff ? std::string("off") : std::to_string(setting + 1));
  }
  return text;
}

int SettingFor(const std::map<std::string, int>& settings, const std::string& key) {
  auto it = settings.find(key);
  return it == settings.end() ? kAuto : it->second;
}

// Pads set to a player get it (several may share one, their input merges).
// Auto pads, in ordinal order, take the player matching their ordinal when no
// pad holds it, else the lowest free one, so unplugging a pad does not move
// the others.
std::vector<std::optional<int>> AssignPorts(const std::vector<Slot>& slots,
                                            const std::map<std::string, int>& settings) {
  std::vector<std::optional<int>> ports(slots.size());
  std::array<bool, kPlayers> taken{};
  for (size_t i = 0; i < slots.size(); ++i) {
    int setting = SettingFor(settings, slots[i].key);
    if (setting >= 0) {
      ports[i] = setting;
      taken[setting] = true;
    }
  }
  std::vector<size_t> order(slots.size());
  for (size_t i = 0; i < order.size(); ++i) {
    order[i] = i;
  }
  std::stable_sort(order.begin(), order.end(),
                   [&](size_t a, size_t b) { return slots[a].ordinal < slots[b].ordinal; });
  for (size_t i : order) {
    if (SettingFor(settings, slots[i].key) != kAuto) {
      continue;
    }
    int port = -1;
    if (slots[i].ordinal < kMaxGuestUsers && !taken[slots[i].ordinal]) {
      port = int(slots[i].ordinal);
    } else {
      auto free = std::find(taken.begin(), taken.end(), false);
      if (free != taken.end()) {
        port = int(free - taken.begin());
      }
    }
    if (port >= 0) {
      ports[i] = port;
      taken[port] = true;
    }
  }
  return ports;
}

// Shared by the input system (guest threads) and the menu (UI thread).
struct PortState {
  std::mutex mutex;
  bool installed = false;
  std::map<std::string, int> settings;
  std::vector<DeviceInfo> devices;  // from the SDK, sorted by ordinal
  std::array<std::vector<DeviceId>, kPlayers> users;
};

// Never destroyed: guest threads may poll input during shutdown.
PortState& Ports() {
  static PortState* state = new PortState;
  return *state;
}

// The SDK's physical devices as slots. Their keys count earlier same-GUID
// devices by device id, which the SDL driver hands out in connection order.
std::vector<Slot> DeviceSlots(const std::vector<DeviceInfo>& devices,
                              std::vector<DeviceId>* ids = nullptr) {
  std::vector<Slot> slots;
  for (const auto& device : devices) {
    if (device.synthetic) {
      continue;
    }
    int earlier = int(std::count_if(devices.begin(), devices.end(), [&](const DeviceInfo& other) {
      return !other.synthetic && other.guid == device.guid && other.id < device.id;
    }));
    slots.push_back({MakeKey(device.guid, earlier), device.ordinal});
    if (ids) {
      ids->push_back(device.id);
    }
  }
  return slots;
}

void RecomputeLocked(PortState& state) {
  for (auto& user : state.users) {
    user.clear();
  }
  std::vector<DeviceId> ids;
  std::vector<Slot> slots = DeviceSlots(state.devices, &ids);
  std::vector<std::optional<int>> ports = AssignPorts(slots, state.settings);
  for (size_t i = 0; i < slots.size(); ++i) {
    if (ports[i]) {
      state.users[*ports[i]].push_back(ids[i]);
    }
  }
  // After the pads, so player 1's capabilities come from a pad when one is
  // there (the SDK answers with the first device).
  for (const auto& device : state.devices) {
    if (device.synthetic) {
      state.users[0].push_back(device.id);
    }
  }
}

class PortAssignment final : public rex::input::DeviceAssignment {
 public:
  void OnDevicesChanged(const std::vector<DeviceInfo>& devices) override {
    auto& state = Ports();
    std::lock_guard lock(state.mutex);
    state.devices = devices;
    RecomputeLocked(state);
  }

  void DevicesForUser(uint32_t user_index, std::vector<DeviceId>& out) const override {
    out.clear();
    if (user_index >= kMaxGuestUsers) {
      return;
    }
    auto& state = Ports();
    std::lock_guard lock(state.mutex);
    out = state.users[user_index];
  }
};

}  // namespace

std::vector<Pad> ConnectedPads() {
  std::vector<Pad> pads;
  std::vector<std::string> guids;
  int count = 0;
  SDL_JoystickID* ids = SDL_GetGamepads(&count);
  // Instance ids grow with each connection.
  std::vector<SDL_JoystickID> sorted(ids, ids + (ids ? count : 0));
  SDL_free(ids);
  std::sort(sorted.begin(), sorted.end());
  for (SDL_JoystickID id : sorted) {
    char guid[33] = {};
    SDL_GUIDToString(SDL_GetJoystickGUIDForID(id), guid, int(sizeof(guid)));
    int earlier = int(std::count(guids.begin(), guids.end(), guid));
    guids.push_back(guid);
    const char* name = SDL_GetGamepadNameForID(id);
    SDL_Gamepad* open = SDL_GetGamepadFromID(id);
    pads.push_back({MakeKey(guid, earlier), name && *name ? name : "Controller",
                    open && PadHasInput(open)});
  }
  return pads;
}

int PortSetting(const std::string& key) {
  auto& state = Ports();
  std::lock_guard lock(state.mutex);
  if (!state.installed) {
    // Before the game: the cvar is the only copy.
    return SettingFor(ParseSettings(rex::cvar::GetFlagByName(kPortsCvar)), key);
  }
  return SettingFor(state.settings, key);
}

void CyclePortSetting(const std::string& key, int dir) {
  // Auto, Player 1-4, Off.
  static constexpr int kCycle[] = {kAuto, 0, 1, 2, 3, kOff};
  constexpr int n = int(std::size(kCycle));
  auto settings = ParseSettings(rex::cvar::GetFlagByName(kPortsCvar));
  int current = SettingFor(settings, key);
  int index = 0;
  for (int i = 0; i < n; ++i) {
    if (kCycle[i] == current) {
      index = (i + dir + n) % n;
      break;
    }
  }
  if (kCycle[index] == kAuto) {
    settings.erase(key);
  } else {
    settings[key] = kCycle[index];
  }
  rex::cvar::SetFlagByName(kPortsCvar, FormatSettings(settings));

  auto& state = Ports();
  std::lock_guard lock(state.mutex);
  state.settings = std::move(settings);
  if (state.installed) {
    RecomputeLocked(state);
  }
}

std::optional<int> CurrentPort(const std::vector<Pad>& pads, const std::string& key) {
  auto& state = Ports();
  std::lock_guard lock(state.mutex);
  std::vector<Slot> slots;
  std::map<std::string, int> settings;
  if (state.installed) {
    slots = DeviceSlots(state.devices);
    settings = state.settings;
  } else {
    // The SDK numbers the pads present at start in connection order.
    for (size_t i = 0; i < pads.size(); ++i) {
      slots.push_back({pads[i].key, uint32_t(i)});
    }
    settings = ParseSettings(rex::cvar::GetFlagByName(kPortsCvar));
  }
  std::vector<std::optional<int>> ports = AssignPorts(slots, settings);
  for (size_t i = 0; i < slots.size(); ++i) {
    if (slots[i].key == key) {
      return ports[i];
    }
  }
  return std::nullopt;
}

void InstallPortAssignment(rex::input::InputSystem* input) {
  auto& state = Ports();
  {
    std::lock_guard lock(state.mutex);
    state.settings = ParseSettings(rex::cvar::GetFlagByName(kPortsCvar));
    state.installed = true;
  }
  // Calls OnDevicesChanged with the devices seen so far.
  input->SetDeviceAssignment(std::make_unique<PortAssignment>());
}

}  // namespace rexlauncher
