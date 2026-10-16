// rexlauncher - drawing helpers on ImGui draw lists. Sizes are given for the
// 1280x800 layout and multiplied by the scale `s`.

#pragma once

#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>  // ImRect

namespace rexlauncher {

// Draws text with its anchor at `pos` (align 0 = left/top, 0.5 = center,
// 1 = right/bottom); returns the text's rectangle.
ImRect PaintText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color,
                 std::string_view text, float align_x = 0.0f, float align_y = 0.5f);

// A filled arrowhead centered on `c`, pointing right or left.
void Triangle(ImDrawList* dl, ImVec2 c, float size, bool right, ImU32 color);

// The launch menu's backdrop: gradient and drifting face-button symbols.
void DrawBackground(ImDrawList* dl, ImVec2 size, double t, float s);

enum class Glyph { kA, kB, kArrows, kMenu };

// A controller button glyph and its caption; returns the width used.
float DrawHint(ImDrawList* dl, ImVec2 left, Glyph glyph, std::string_view caption, float s);

}  // namespace rexlauncher
