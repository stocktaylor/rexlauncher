// rexlauncher - menu fonts. The font data is compiled in by CMakeLists.txt
// (rexlauncher_fonts.cpp, from assets/).

#include "fonts.h"

#include <cstddef>
#include <cstdio>

#include <imgui.h>

#include <rexlauncher/launch_menu.h>

namespace rexlauncher {

namespace assets {
extern const unsigned char kRobotoCondensedRegular[];
extern const std::size_t kRobotoCondensedRegularSize;
extern const unsigned char kRobotoCondensedBold[];
extern const std::size_t kRobotoCondensedBoldSize;
}  // namespace assets

namespace {

// Fonts are baked once (the SDK's ImGui renderer has no dynamic atlas), large,
// and drawn scaled down.
constexpr float kRegularBakeSize = 44.0f;
constexpr float kBoldBakeSize = 88.0f;
ImFont* g_regular = nullptr;
ImFont* g_bold = nullptr;

}  // namespace

ImFont* RegularFont() {
  return g_regular ? g_regular : ImGui::GetFont();
}

ImFont* BoldFont() {
  return g_bold ? g_bold : RegularFont();
}

void ConfigureFonts(ImFontAtlas* atlas) {
  ImFontConfig config;
  config.FontDataOwnedByAtlas = false;  // compiled-in data
  config.OversampleH = 2;
  std::snprintf(config.Name, sizeof(config.Name), "Roboto Condensed (rexlauncher)");
  g_regular = atlas->AddFontFromMemoryTTF(
      const_cast<unsigned char*>(assets::kRobotoCondensedRegular),
      int(assets::kRobotoCondensedRegularSize), kRegularBakeSize, &config);
  std::snprintf(config.Name, sizeof(config.Name), "Roboto Condensed Bold (rexlauncher)");
  g_bold = atlas->AddFontFromMemoryTTF(const_cast<unsigned char*>(assets::kRobotoCondensedBold),
                                       int(assets::kRobotoCondensedBoldSize), kBoldBakeSize,
                                       &config);
}

}  // namespace rexlauncher
