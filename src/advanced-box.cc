/*
 * advanced-box.cc - Box of the advanced options
 *
 * Copyright (C) 2026  Wicked_Digger <wicked_digger@mail.ru>
 *
 * This file is part of freeserf.
 *
 * freeserf is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * freeserf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with freeserf.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "src/advanced-box.h"

#include <string>

#include "src/interface.h"
#include "src/popup.h"
#include "src/box-frame.h"
#include "src/audio.h"
#include "src/data.h"

/* Exit button in the bottom right corner, where the options box has it. */
static const int kExitWidth = 16;
static const int kExitHeight = 16;

/* Options as in the options box: a label on the left, a check box on the
   right. */
static const int kLabelX = 16;
static const int kCheckSize = 16;
static const int kRowHeight = 20;
static const int kFirstRowY = 19;

/* An option is a check box (get) or a value shown as text (text); a click
   on either calls change. */
typedef struct Option {
  const char *label;
  bool (Interface::*get)() const;
  std::string (*text)(const Interface *interface);
  void (Interface::*change)();
} Option;

static std::string
autosave_text(const Interface *interface) {
  unsigned int minutes = interface->get_autosave_minutes();
  return (minutes == 0) ? "Off" : std::to_string(minutes) + " min";
}

static const Option options[] = {
  { "Invert scrolling", &Interface::get_invert_scrolling, nullptr,
    &Interface::switch_invert_scrolling },
  { "Large numbers", &Interface::get_large_numbers, nullptr,
    &Interface::switch_large_numbers },
  { "Autosave", nullptr, autosave_text,
    &Interface::next_autosave_interval },
  { "Stock box until occupied", &Interface::get_stock_box_occupied, nullptr,
    &Interface::switch_stock_box_occupied },
};

/* Width of the value texts, right aligned to the check boxes. */
static const int kValueWidth = 48;
static const int kOptionCount = sizeof(options) / sizeof(options[0]);

AdvancedBox::AdvancedBox(Interface *_interface)
  : interface(_interface) {
  set_size(kWidth, kHeight);
}

void
AdvancedBox::internal_draw() {
  /* Background of the options box. */
  for (int y = 0; y < height; y += 16) {
    for (int x = 0; x < width; x += 16) {
      frame->draw_sprite(x, y, Data::AssetIcon,
                         PopupBox::PatternDiagonalGreen);
    }
  }
  draw_box_frame(frame, width, height);

  int check_x = width - 24 - kCheckSize;
  for (int i = 0; i < kOptionCount; i++) {
    int y = kFirstRowY + i * kRowHeight;
    frame->draw_string(kLabelX, y + 4, options[i].label, Color::green,
                       Color::black);
    if (options[i].get != nullptr) {
      bool on = (interface->*options[i].get)();
      frame->draw_sprite(check_x, y, Data::AssetIcon, on ? 288 : 220);
    } else {
      std::string value = options[i].text(interface);
      int value_x = check_x + kCheckSize - 8 * static_cast<int>(value.size());
      frame->draw_string(value_x, y + 4, value, Color::green, Color::black);
    }
  }

  frame->draw_sprite(width - 8 - kExitWidth, height - 7 - kExitHeight,
                     Data::AssetIcon, 60);  // Exit
}

bool
AdvancedBox::handle_click_left(int x, int y) {
  int check_x = width - 24 - kCheckSize;
  for (int i = 0; i < kOptionCount; i++) {
    int row_y = kFirstRowY + i * kRowHeight;
    int left = (options[i].get != nullptr) ? check_x :
                                             check_x + kCheckSize - kValueWidth;
    if (x >= left && x < check_x + kCheckSize &&
        y >= row_y && y < row_y + kCheckSize) {
      play_sound(Audio::TypeSfxClick);
      (interface->*options[i].change)();
      set_redraw();
      return true;
    }
  }

  int exit_x = width - 8 - kExitWidth;
  int exit_y = height - 7 - kExitHeight;
  if (x >= exit_x && x < exit_x + kExitWidth &&
      y >= exit_y && y < exit_y + kExitHeight) {
    play_sound(Audio::TypeSfxClick);
    /* Leave the options as well. */
    interface->close_advanced();
  }
  return true;
}
