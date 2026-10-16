// rexlauncher - colors, after PPSSPP's default theme (UI/Theme.cpp; its colors
// are ABGR).

#pragma once

#include <imgui.h>

namespace rexlauncher {

inline constexpr ImU32 kBg = IM_COL32(0x24, 0x4D, 0x75, 0xFF);        // uBackgroundColor
inline constexpr ImU32 kBgTop = IM_COL32(0x2C, 0x5C, 0x8A, 0xFF);
inline constexpr ImU32 kBgBottom = IM_COL32(0x17, 0x34, 0x52, 0xFF);
inline constexpr ImU32 kItemBg = IM_COL32(0x00, 0x00, 0x00, 0x55);    // uItemStyleBg
inline constexpr ImU32 kFocusBg = IM_COL32(0x4C, 0xC2, 0xED, 0xFF);   // uItemFocusedStyleBg
inline constexpr ImU32 kDownBg = IM_COL32(0x39, 0x99, 0xBD, 0xFF);    // uItemDownStyleBg
inline constexpr ImU32 kText = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
inline constexpr ImU32 kTextDisabled = IM_COL32(0xEE, 0xEE, 0xEE, 0x80);  // uItemDisabledStyleFg
inline constexpr ImU32 kTextDim = IM_COL32(0xFF, 0xFF, 0xFF, 0xB4);
inline constexpr ImU32 kError = IM_COL32(0xFF, 0x8A, 0x7A, 0xFF);
inline constexpr ImU32 kButtonA = IM_COL32(0x60, 0xB0, 0x45, 0xFF);
inline constexpr ImU32 kButtonB = IM_COL32(0xD0, 0x45, 0x3A, 0xFF);
inline constexpr ImU32 kButtonX = IM_COL32(0x3A, 0x7C, 0xD0, 0xFF);
inline constexpr ImU32 kButtonY = IM_COL32(0xE0, 0xB0, 0x30, 0xFF);
inline constexpr ImU32 kButtonGrey = IM_COL32(0x50, 0x50, 0x50, 0xFF);

inline ImU32 WithAlpha(ImU32 color, int alpha) {
  return (color & ~IM_COL32_A_MASK) | (ImU32(alpha) << IM_COL32_A_SHIFT);
}

}  // namespace rexlauncher
