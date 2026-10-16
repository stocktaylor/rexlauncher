// rexlauncher - drawing the menu, in the style of PPSSPP's default theme,
// scaled from a 1280x800 (Steam Deck) layout. Also handles the mouse (hover to
// focus, click to select or change).

#include "menu.h"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>  // ImRect

#include "draw.h"
#include "fonts.h"
#include "theme.h"

namespace rexlauncher {

void LaunchMenu::OnDraw(ImGuiIO& io) {
  if (finished_) {
    return;
  }
  for (Action action : input_.Poll()) {
    Handle(action);
    if (finished_) {
      return;
    }
  }

  ImVec2 screen = io.DisplaySize;
  double now = ImGui::GetTime();
  // Laid out for 1280x800 (Steam Deck), scaled to the window.
  float s = std::clamp(std::min(screen.x / 1280.0f, screen.y / 800.0f), 0.5f, 3.0f);

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(screen);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::Begin("##rexlauncher", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav |
                   ImGuiWindowFlags_NoScrollWithMouse);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  if (in_game_) {
    // Dim the game behind the menu.
    dl->AddRectFilledMultiColor(ImVec2(0, 0), screen, WithAlpha(kBgTop, 0xE0),
                                WithAlpha(kBgTop, 0xE0), WithAlpha(kBgBottom, 0xF0),
                                WithAlpha(kBgBottom, 0xF0));
  } else {
    DrawBackground(dl, screen, now, s);
  }

  // Title
  float margin = 56.0f * s;
  PaintText(dl, BoldFont(), 46.0f * s, ImVec2(margin, 48.0f * s), kText, config_.title, 0.0f, 0.0f);
  if (!config_.subtitle.empty()) {
    PaintText(dl, RegularFont(), 18.0f * s, ImVec2(margin + 2.0f * s, 104.0f * s), kTextDim,
              config_.subtitle, 0.0f, 0.0f);
  }

  // Rows
  float list_w = std::min(screen.x - 2.0f * margin, 820.0f * s);
  float y = 146.0f * s;
  // Rows shrink to fit when the in-game list (two extra rows) runs long.
  float bottom = screen.y - 52.0f * s - 40.0f * s;
  float natural = 74.0f * s + 10.0f * s;
  for (size_t i = 1; i < rows_.size(); ++i) {
    natural += 58.0f * s;
  }
  float row_scale = std::clamp((bottom - y) / natural, 0.7f, 1.0f);
  bool mouse_moved = io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f;
  for (size_t i = 0; i < rows_.size(); ++i) {
    Row& row = rows_[i];
    bool tall = row.kind == RowKind::kPlay;
    float h = (tall ? 74.0f : 52.0f) * s * row_scale;
    // A gap before the last group (Quit; in-game Restart + Quit).
    if (row.kind == (in_game_ ? RowKind::kRestart : RowKind::kQuit)) {
      y += 10.0f * s * row_scale;
    }
    ImVec2 min(margin, y), max(margin + list_w, y + h);
    y += h + 6.0f * s * row_scale;

    bool enabled = Enabled(row);
    bool hovered = ImGui::IsMouseHoveringRect(min, max, false);
    if (enabled && hovered && mouse_moved) {
      focus_ = i;
    }
    bool focused = focus_ == i;
    bool flashing = pressed_flash_ && pressed_flash_->first == i && now - pressed_flash_->second < 0.12;
    bool mouse_down = enabled && hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    ImU32 bg = (flashing || mouse_down) ? kDownBg : focused ? kFocusBg : kItemBg;
    dl->AddRectFilled(min, max, bg, 3.0f * s);
    ImU32 fg = enabled ? kText : kTextDisabled;
    float cy = (min.y + max.y) * 0.5f;
    float label_x = min.x + 22.0f * s;

    if (tall) {
      Triangle(dl, ImVec2(label_x + 14.0f * s, cy), 22.0f * s, true, fg);
      PaintText(dl, BoldFont(), 32.0f * s * row_scale, ImVec2(label_x + 40.0f * s, cy), fg, Label(row));
    } else {
      bool confirming = confirm_ == i;
      PaintText(dl, RegularFont(), 23.0f * s * row_scale, ImVec2(label_x, cy),
                confirming ? kError : fg, Label(row));
    }

    bool clicked = enabled && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    if (auto value = Value(row)) {
      float right = max.x - 22.0f * s;
      bool arrows = focused && enabled;
      float text_right = arrows ? right - 24.0f * s : right;
      ImRect text = PaintText(dl, RegularFont(), 23.0f * s * row_scale, ImVec2(text_right, cy), fg,
                              *value, 1.0f);
      if (arrows) {
        Triangle(dl, ImVec2(right - 6.0f * s, cy), 13.0f * s, true, fg);
        Triangle(dl, ImVec2(text.Min.x - 18.0f * s, cy), 13.0f * s, false, fg);
      }
      if (clicked) {
        // Left of the value steps back, anywhere else forward.
        focus_ = i;
        Change(row, io.MousePos.x < text.Min.x + 8.0f * s ? -1 : 1);
      }
    } else if (clicked) {
      focus_ = i;
      Activate(i);
      if (finished_) {
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
      }
    }
  }

  if (!message_.empty()) {
    PaintText(dl, RegularFont(), 18.0f * s, ImVec2(margin, y + 8.0f * s), kError, message_, 0.0f, 0.0f);
  } else if (in_game_ && RestartPending()) {
    PaintText(dl, RegularFont(), 18.0f * s, ImVec2(margin, y + 8.0f * s), kTextDim,
              "Changes marked (next start) are saved; Restart game applies them now.", 0.0f, 0.0f);
  }

  // Button hints
  ImVec2 bar_min(0, screen.y - 52.0f * s);
  dl->AddRectFilled(bar_min, screen, kItemBg);
  float x = margin;
  float cy = bar_min.y + 26.0f * s;
  x += DrawHint(dl, ImVec2(x, cy), Glyph::kA, "Select", s);
  x += DrawHint(dl, ImVec2(x, cy), Glyph::kArrows, "Change", s);
  if (in_game_) {
    DrawHint(dl, ImVec2(x, cy), Glyph::kB, "Resume", s);
  } else {
    x += DrawHint(dl, ImVec2(x, cy), Glyph::kB, "Quit", s);
    DrawHint(dl, ImVec2(x, cy), Glyph::kMenu, "Play", s);
  }

  ImGui::End();
  ImGui::PopStyleVar(2);
}

}  // namespace rexlauncher
