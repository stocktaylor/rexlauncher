// rexlauncher - standard options.

#include "options.h"

#include <algorithm>
#include <cstdint>
#include <iterator>

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>

#include <rex/cvar.h>

namespace rexlauncher {

namespace {

struct WindowSize {
  int width, height;
};
constexpr WindowSize kWindowPresets[] = {
    {1280, 720}, {1280, 800}, {1600, 900}, {1920, 1080}, {2560, 1440}};

}  // namespace

Option ResolutionScaleOption() {
  int base_w = std::max(1, rex::cvar::Query<int32_t>("video_mode_width"));
  int base_h = std::max(1, rex::cvar::Query<int32_t>("video_mode_height"));
  Option scale{"Rendering resolution", "resolution_scale", {}};
  for (int k = 1; k <= 4; ++k) {
    std::string size = std::to_string(base_w * k) + "×" + std::to_string(base_h * k);
    scale.choices.push_back(
        {std::to_string(k), k == 1 ? "Native (" + size + ")" : std::to_string(k) + "× (" + size + ")"});
  }
  return scale;
}

Option DisplayModeOption() {
  return {"Display mode", "fullscreen", {{"true", "Fullscreen"}, {"false", "Windowed"}}};
}

Option UpscalingFilterOption() {
  return {"Upscaling filter",
          "present_effect",
          {{"bilinear", "Bilinear"}, {"cas", "AMD CAS (sharpen)"}, {"fsr", "AMD FSR"}},
          true};
}

Option VSyncOption() {
  return {"VSync", "vsync", {{"true", "On"}, {"false", "Off"}}, true};
}

std::string CurrentChoiceLabel(const Option& option) {
  std::string value = rex::cvar::GetFlagByName(option.cvar);
  for (const auto& choice : option.choices) {
    if (choice.value == value) {
      return choice.label;
    }
  }
  return value;
}

void CycleChoice(const Option& option, int dir) {
  if (option.choices.empty()) {
    return;
  }
  const auto& choices = option.choices;
  std::string value = rex::cvar::GetFlagByName(option.cvar);
  int n = int(choices.size());
  int index = 0;
  for (int i = 0; i < n; ++i) {
    if (choices[i].value == value) {
      index = (i + dir + n) % n;
      break;
    }
  }
  rex::cvar::SetFlagByName(option.cvar, choices[index].value);
  if (option.cvar == "fullscreen") {
    ApplyWindowSize();  // the SDK switches fullscreen itself (cvar callback)
  }
}

std::string WindowSizeLabel() {
  int w = rex::cvar::Query<int32_t>("window_width");
  int h = rex::cvar::Query<int32_t>("window_height");
  if (w <= 0 || h <= 0) {
    return "Default";
  }
  return std::to_string(w) + "×" + std::to_string(h);
}

void CycleWindowSize(int dir) {
  int w = rex::cvar::Query<int32_t>("window_width");
  int h = rex::cvar::Query<int32_t>("window_height");
  int n = int(std::size(kWindowPresets));
  int index = 0;
  for (int i = 0; i < n; ++i) {
    if (kWindowPresets[i].width == w && kWindowPresets[i].height == h) {
      index = (i + dir + n) % n;
      break;
    }
    index = dir > 0 ? 0 : n - 1;  // unknown size: start from either end
  }
  rex::cvar::SetFlagByName("window_width", std::to_string(kWindowPresets[index].width));
  rex::cvar::SetFlagByName("window_height", std::to_string(kWindowPresets[index].height));
  ApplyWindowSize();
}

void ApplyWindowSize() {
  if (rex::cvar::GetFlagByName("fullscreen") == "true") {
    return;
  }
  int w = rex::cvar::Query<int32_t>("window_width");
  int h = rex::cvar::Query<int32_t>("window_height");
  if (w <= 0 || h <= 0) {
    return;
  }
  int count = 0;
  SDL_Window** windows = SDL_GetWindows(&count);
  if (windows && count > 0) {
    SDL_SetWindowSize(windows[0], w, h);
  }
  SDL_free(windows);
}

}  // namespace rexlauncher
