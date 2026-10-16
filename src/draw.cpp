// rexlauncher - drawing helpers.

#include "draw.h"

#include <cfloat>
#include <cmath>

#include "fonts.h"
#include "theme.h"

namespace rexlauncher {

ImRect PaintText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color,
                 std::string_view text, float align_x, float align_y) {
  const char* begin = text.data();
  const char* end = begin + text.size();
  ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, begin, end);
  ImVec2 top_left(pos.x - extent.x * align_x, pos.y - extent.y * align_y);
  dl->AddText(font, size, top_left, color, begin, end);
  return ImRect(top_left, ImVec2(top_left.x + extent.x, top_left.y + extent.y));
}

void Triangle(ImDrawList* dl, ImVec2 c, float size, bool right, ImU32 color) {
  float d = right ? 1.0f : -1.0f;
  dl->AddTriangleFilled(ImVec2(c.x - d * size * 0.5f, c.y - size * 0.6f),
                        ImVec2(c.x + d * size * 0.6f, c.y),
                        ImVec2(c.x - d * size * 0.5f, c.y + size * 0.6f), color);
}

void DrawBackground(ImDrawList* dl, ImVec2 size, double t, float s) {
  dl->AddRectFilled(ImVec2(0, 0), size, kBg);
  dl->AddRectFilledMultiColor(ImVec2(0, 0), size, kBgTop, kBgTop, kBgBottom, kBgBottom);
  // Slowly drifting face-button symbols, after PPSSPP's floating PSP symbols.
  static constexpr struct {
    const char* letter;
    ImU32 color;
  } kSymbols[] = {{"A", kButtonA}, {"B", kButtonB}, {"X", kButtonX}, {"Y", kButtonY}};
  for (int i = 0; i < 24; ++i) {
    float seed = float(i) * 12.9898f;
    float fx = std::fabs(std::fmod(std::sin(seed) * 43758.547f, 1.0f));
    float fy = std::fabs(std::fmod(std::sin(seed * 1.7f) * 24634.635f, 1.0f));
    float speed = 6.0f + 10.0f * std::fabs(std::cos(seed * 2.3f));
    float span = size.x + 80.0f * s;
    float x = std::fmod(fx * size.x + float(t) * speed * s, span);
    if (x < 0) x += span;
    x -= 40.0f * s;
    float y = fy * size.y + std::sin(float(t) * 0.4f + seed) * 12.0f * s;
    float r = (14.0f + 10.0f * fx) * s;
    ImU32 faint = WithAlpha(kSymbols[i % 4].color, 26);
    dl->AddCircle(ImVec2(x, y), r, faint, 0, 2.0f * s);
    PaintText(dl, BoldFont(), r * 1.1f, ImVec2(x, y), faint, kSymbols[i % 4].letter, 0.5f, 0.5f);
  }
}

float DrawHint(ImDrawList* dl, ImVec2 left, Glyph glyph, std::string_view caption, float s) {
  float r = 13.0f * s;
  ImVec2 c(left.x + r, left.y);
  switch (glyph) {
    case Glyph::kA:
    case Glyph::kB:
      dl->AddCircleFilled(c, r, glyph == Glyph::kA ? kButtonA : kButtonB);
      PaintText(dl, BoldFont(), 16.0f * s, c, kText, glyph == Glyph::kA ? "A" : "B", 0.5f, 0.5f);
      break;
    case Glyph::kArrows:
      dl->AddCircleFilled(c, r, kButtonGrey);
      Triangle(dl, ImVec2(c.x - 5.0f * s, c.y), 8.0f * s, false, kText);
      Triangle(dl, ImVec2(c.x + 5.0f * s, c.y), 8.0f * s, true, kText);
      break;
    case Glyph::kMenu:
      dl->AddCircleFilled(c, r, kButtonGrey);
      for (float dy : {-4.0f, 0.0f, 4.0f}) {
        dl->AddLine(ImVec2(c.x - 6.0f * s, c.y + dy * s), ImVec2(c.x + 6.0f * s, c.y + dy * s),
                    kText, 2.0f * s);
      }
      break;
  }
  ImRect text = PaintText(dl, RegularFont(), 18.0f * s, ImVec2(c.x + r + 8.0f * s, left.y), kText,
                          caption);
  return text.Max.x - left.x + 28.0f * s;
}

}  // namespace rexlauncher
