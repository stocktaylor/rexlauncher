// rexlauncher - the Keyboard page, opened from the Controls page (Map
// keyboard, or the keyboard's row once mapped): a list of the controller's
// inputs, each with two key slots. Choosing a slot waits for a key; Escape
// cancels, Delete or Backspace clears the focused slot. Leaving the page turns
// the keyboard on as a virtual controller (mnk_mode), so it shows on the
// Controls page; Remove turns it off.

#include "menu.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>  // ImRect

#include "draw.h"
#include "fonts.h"
#include "keyboard.h"
#include "settings.h"
#include "theme.h"

namespace rexlauncher {

// --- Rows -----------------------------------------------------------------------

size_t LaunchMenu::KeyRowCount() const {
  return 1 + KeyActions().size() + 1 + (KeyboardMapped() ? 1 : 0);
}

LaunchMenu::KeyRow LaunchMenu::KeyRowKind(size_t index) const {
  size_t actions = KeyActions().size();
  if (index == 0) {
    return KeyRow::kDone;
  }
  if (index <= actions) {
    return KeyRow::kAction;
  }
  return index == actions + 1 ? KeyRow::kReset : KeyRow::kRemove;
}

bool LaunchMenu::KeyRowEnabled(size_t index) const {
  switch (KeyRowKind(index)) {
    case KeyRow::kAction:
      return !IsLockedByCommandLine(KeyActions()[index - 1].cvar);
    case KeyRow::kReset:
      return std::none_of(KeyActions().begin(), KeyActions().end(), [](const KeyAction& action) {
        return IsLockedByCommandLine(action.cvar);
      });
    case KeyRow::kRemove:
      return !KeyboardLocked();
    default:
      return true;
  }
}

// --- Actions --------------------------------------------------------------------

void LaunchMenu::OpenKeyboard() {
  confirm_.reset();
  message_.clear();
  page_ = Page::kKeyboard;
  key_focus_ = 1;  // the first input
  key_slot_ = 0;
  key_scroll_ = 0.0f;
  capturing_ = false;
}

void LaunchMenu::CloseKeyboard(bool keep_mapped) {
  capturing_ = false;
  message_.clear();
  if (keep_mapped && !KeyboardLocked() && AnyKeyBound()) {
    SetKeyboardMapped(true);
  }
  SaveToConfig(paths_.config_path, ManagedCvars());
  page_ = Page::kControls;
  BuildPadRows();
  focus_ = rows_.size() - 1;  // the keyboard's row
}

void LaunchMenu::HandleKeyboard(Action action) {
  size_t n = KeyRowCount();
  switch (action) {
    case Action::kUp:
    case Action::kDown: {
      int dir = action == Action::kUp ? -1 : 1;
      size_t i = key_focus_;
      for (size_t step = 0; step < n; ++step) {
        i = (i + n + dir) % n;
        if (KeyRowEnabled(i)) {
          key_focus_ = i;
          break;
        }
      }
      break;
    }
    case Action::kLeft:
      key_slot_ = 0;
      break;
    case Action::kRight:
      key_slot_ = 1;
      break;
    case Action::kAccept:
      ActivateKeyRow(key_focus_);
      break;
    case Action::kBack:
      CloseKeyboard(true);
      break;
    case Action::kStart:
      break;
  }
}

void LaunchMenu::ActivateKeyRow(size_t index) {
  if (!KeyRowEnabled(index)) {
    return;
  }
  switch (KeyRowKind(index)) {
    case KeyRow::kDone:
      CloseKeyboard(true);
      break;
    case KeyRow::kAction:
      StartCapture(index, key_slot_);
      break;
    case KeyRow::kReset:
      ResetKeyBinds();
      message_ = "Keys reset to the defaults.";
      break;
    case KeyRow::kRemove:
      SetKeyboardMapped(false);
      CloseKeyboard(false);
      break;
  }
}

void LaunchMenu::StartCapture(size_t row, size_t slot) {
  if (KeyRowKind(row) != KeyRow::kAction || !KeyRowEnabled(row)) {
    return;
  }
  key_focus_ = row;
  key_slot_ = slot;
  capturing_ = true;
  message_.clear();
}

bool LaunchMenu::UpdateKeyCapture() {
  capture_ended_ = false;
  if (capturing_) {
    const KeyAction& action = KeyActions()[key_focus_ - 1];
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
      release_key_ = ImGuiKey_Escape;
      capturing_ = false;
    } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
               ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      capturing_ = false;
    } else if (auto pressed = PressedKey()) {
      auto moved_from = BindKey(action.cvar, key_slot_, pressed->first);
      if (moved_from) {
        message_ = pressed->first + " moved here from " + *moved_from + ".";
      }
      release_key_ = pressed->second;
      capturing_ = false;
    }
    capture_ended_ = !capturing_;
    return true;
  }
  if (release_key_ != ImGuiKey_None) {
    if (ImGui::IsKeyDown(release_key_)) {
      return true;
    }
    release_key_ = ImGuiKey_None;
  }
  if (KeyRowKind(key_focus_) == KeyRow::kAction && KeyRowEnabled(key_focus_) &&
      (ImGui::IsKeyPressed(ImGuiKey_Delete, false) ||
       ImGui::IsKeyPressed(ImGuiKey_Backspace, false))) {
    BindKey(KeyActions()[key_focus_ - 1].cvar, key_slot_, "");
    message_.clear();
  }
  return false;
}

// --- Drawing --------------------------------------------------------------------

void LaunchMenu::DrawKeyboardPage(ImDrawList* dl, ImGuiIO& io, float s, float margin,
                                  double now) {
  const auto& actions = KeyActions();
  size_t n = KeyRowCount();
  if (key_focus_ >= n) {
    key_focus_ = 0;
  }
  ImVec2 screen = io.DisplaySize;
  float list_w = std::min(screen.x - 2.0f * margin, 820.0f * s);
  float slot_w = std::min(190.0f * s, list_w * 0.25f);
  float slot_gap = 10.0f * s;
  float pad_x = 14.0f * s;
  // Key slot `k`'s left edge, from the row's right edge.
  auto slot_x = [&](float right, size_t k) {
    return right - pad_x - (2 - k) * slot_w - (1 - k) * slot_gap;
  };

  // Column headings
  float head_y = 146.0f * s;
  float list_right = margin + list_w;
  PaintText(dl, RegularFont(), 18.0f * s, ImVec2(margin + pad_x, head_y + 10.0f * s), kTextDim,
            "Controller");
  for (size_t k = 0; k < 2; ++k) {
    float x = slot_x(list_right, k);
    PaintText(dl, RegularFont(), 18.0f * s, ImVec2(x + slot_w * 0.5f, head_y + 10.0f * s),
              kTextDim, k == 0 ? "Key 1" : "Key 2", 0.5f);
  }

  // The list scrolls; the focused row stays in view.
  float top = head_y + 28.0f * s;
  float bottom = screen.y - 52.0f * s - 40.0f * s;
  float h = 42.0f * s;
  float pitch = h + 4.0f * s;
  int visible = std::max(1, int((bottom - top) / pitch));
  int max_scroll = std::max(0, int(n) - visible);
  bool mouse_in_list = io.MousePos.x >= margin && io.MousePos.x <= list_right &&
                       io.MousePos.y >= top && io.MousePos.y <= bottom;
  if (mouse_in_list && io.MouseWheel != 0.0f && !capturing_) {
    key_scroll_ = std::clamp(key_scroll_ - io.MouseWheel * 3.0f, 0.0f, float(max_scroll));
    // Keep the focus on screen, so the next frame doesn't scroll back to it.
    int first = int(key_scroll_);
    key_focus_ = size_t(std::clamp(int(key_focus_), first, first + visible - 1));
  }
  if (float(key_focus_) < key_scroll_) {
    key_scroll_ = float(key_focus_);
  } else if (float(key_focus_) >= key_scroll_ + visible) {
    key_scroll_ = float(key_focus_ - visible + 1);
  }
  key_scroll_ = std::clamp(key_scroll_, 0.0f, float(max_scroll));
  int first = int(key_scroll_);

  bool mouse_moved = io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f;
  bool clicks = !capturing_ && !capture_ended_;
  bool locked_shown = false;
  float y = top;
  for (int i = first; i < std::min(int(n), first + visible); ++i) {
    size_t row = size_t(i);
    KeyRow kind = KeyRowKind(row);
    bool enabled = KeyRowEnabled(row);
    locked_shown |= kind == KeyRow::kAction && !enabled;
    ImVec2 min(margin, y), max(list_right, y + h);
    y += pitch;
    float cy = (min.y + max.y) * 0.5f;

    bool hovered = ImGui::IsMouseHoveringRect(min, max, false);
    if (enabled && hovered && mouse_moved && !capturing_) {
      key_focus_ = row;
    }
    bool focused = key_focus_ == row;
    ImU32 fg = enabled ? kText : kTextDisabled;

    if (kind != KeyRow::kAction) {
      bool down = enabled && hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
      dl->AddRectFilled(min, max, down ? kDownBg : focused ? kFocusBg : kItemBg, 3.0f * s);
      const char* label = kind == KeyRow::kDone    ? "Done"
                          : kind == KeyRow::kReset ? "Reset to defaults"
                                                   : "Remove keyboard controller";
      PaintText(dl, RegularFont(), 22.0f * s, ImVec2(min.x + 22.0f * s, cy), fg, label);
      if (clicks && enabled && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        key_focus_ = row;
        ActivateKeyRow(row);
        if (page_ != Page::kKeyboard) {
          return;  // left the page; the Controls page draws next frame
        }
      }
      continue;
    }

    const KeyAction& action = actions[row - 1];
    dl->AddRectFilled(min, max, focused ? WithAlpha(kFocusBg, 0x48) : kItemBg, 3.0f * s);
    float label_x = min.x + 22.0f * s;
    if (action.color) {
      // Face buttons: the colored button with its letter.
      ImVec2 c(label_x + 13.0f * s, cy);
      dl->AddCircleFilled(c, 13.0f * s, enabled ? action.color : WithAlpha(action.color, 0x80));
      PaintText(dl, BoldFont(), 18.0f * s, c, kText, action.label, 0.5f);
    } else {
      PaintText(dl, RegularFont(), 22.0f * s, ImVec2(label_x, cy), fg, action.label);
    }

    std::vector<std::string> keys = BoundKeys(action.cvar);
    for (size_t k = 0; k < 2; ++k) {
      ImVec2 smin(slot_x(max.x, k), min.y + 5.0f * s);
      ImVec2 smax(smin.x + slot_w, max.y - 5.0f * s);
      bool slot_hovered = ImGui::IsMouseHoveringRect(smin, smax, false);
      if (enabled && slot_hovered && mouse_moved && !capturing_) {
        key_slot_ = k;
      }
      bool slot_focused = focused && key_slot_ == k;
      bool waiting = capturing_ && slot_focused;
      ImU32 bg = waiting ? (std::fmod(now, 0.8) < 0.4 ? kDownBg : kFocusBg)
                 : slot_focused ? kFocusBg
                                : IM_COL32(0x00, 0x00, 0x00, 0x60);
      dl->AddRectFilled(smin, smax, bg, 3.0f * s);
      ImVec2 c((smin.x + smax.x) * 0.5f, cy);
      if (waiting) {
        PaintText(dl, RegularFont(), 19.0f * s, c, kText, "Press a key", 0.5f);
      } else if (k < keys.size()) {
        PaintText(dl, RegularFont(), 20.0f * s, c, fg, keys[k], 0.5f);
      } else {
        PaintText(dl, RegularFont(), 20.0f * s, c, kTextDisabled, "-", 0.5f);
      }
      if (clicks && enabled && slot_hovered) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
          StartCapture(row, k);
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          key_focus_ = row;
          key_slot_ = k;
          BindKey(action.cvar, k, "");
        }
      }
    }
  }

  // Scroll position
  if (max_scroll > 0) {
    float track_h = bottom - top;
    float thumb_h = std::max(track_h * float(visible) / float(n), 20.0f * s);
    float thumb_y = top + (track_h - thumb_h) * key_scroll_ / float(max_scroll);
    float x = list_right + 8.0f * s;
    dl->AddRectFilled(ImVec2(x, top), ImVec2(x + 4.0f * s, bottom), kItemBg, 2.0f * s);
    dl->AddRectFilled(ImVec2(x, thumb_y), ImVec2(x + 4.0f * s, thumb_y + thumb_h), kTextDim,
                      2.0f * s);
  }

  std::string note = !message_.empty() ? message_
                     : locked_shown    ? "Keys set on the command line (by the launcher) can't be "
                                         "changed here."
                                       : "";
  if (!note.empty()) {
    PaintText(dl, RegularFont(), 18.0f * s, ImVec2(margin, bottom + 12.0f * s), kTextDim, note,
              0.0f, 0.0f);
  }

  // Button hints
  ImVec2 bar_min(0, screen.y - 52.0f * s);
  dl->AddRectFilled(bar_min, screen, kItemBg);
  float x = margin;
  float bar_cy = bar_min.y + 26.0f * s;
  if (capturing_) {
    std::string prompt = std::string("Press a key for ") + actions[key_focus_ - 1].label +
                         "   (Esc cancels)";
    PaintText(dl, RegularFont(), 20.0f * s, ImVec2(x, bar_cy), kText, prompt);
    return;
  }
  x += DrawHint(dl, ImVec2(x, bar_cy), Glyph::kA, "Bind key", s);
  x += DrawHint(dl, ImVec2(x, bar_cy), Glyph::kArrows, "Move", s);
  x += DrawHint(dl, ImVec2(x, bar_cy), Glyph::kB, "Done", s);
  PaintText(dl, RegularFont(), 20.0f * s, ImVec2(x, bar_cy), kTextDim,
            "Delete or right-click clears a key");
}

}  // namespace rexlauncher
