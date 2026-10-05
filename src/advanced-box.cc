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
static const int kRowHeight = 16;
static const int kFirstRowY = 19;

typedef enum Option {
  OptionInvertScrolling = 0,
} Option;

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
  int y = kFirstRowY + OptionInvertScrolling * kRowHeight;
  frame->draw_string(kLabelX, y + 4, "Invert scrolling", Color::green,
                     Color::black);
  frame->draw_sprite(check_x, y, Data::AssetIcon,
                     interface->get_invert_scrolling() ? 288 : 220);

  frame->draw_sprite(width - 8 - kExitWidth, height - 7 - kExitHeight,
                     Data::AssetIcon, 60);  // Exit
}

bool
AdvancedBox::handle_click_left(int x, int y) {
  int check_x = width - 24 - kCheckSize;
  int row_y = kFirstRowY + OptionInvertScrolling * kRowHeight;
  if (x >= check_x && x < check_x + kCheckSize &&
      y >= row_y && y < row_y + kCheckSize) {
    play_sound(Audio::TypeSfxClick);
    interface->switch_invert_scrolling();
    set_redraw();
    return true;
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
