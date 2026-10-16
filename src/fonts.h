// rexlauncher - menu fonts (registered by ConfigureFonts, see
// include/rexlauncher/launch_menu.h).

#pragma once

struct ImFont;

namespace rexlauncher {

// Roboto Condensed, or ImGui's current font if ConfigureFonts was not called.
ImFont* RegularFont();
ImFont* BoldFont();

}  // namespace rexlauncher
