/*
 * box-frame.cc - Frame of the boxes in the style of the popups
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

#include "src/box-frame.h"

#include <algorithm>

#include "src/gfx.h"
#include "src/data.h"

/* Frame of the small popup boxes (144x160) stretched to the size of the
   box: the corners and the ornament of the top edge are kept, the plain
   wood between them is repeated. */
void
draw_box_frame(Frame *frame, int width, int height) {
  Graphics &gfx = Graphics::get_instance();

  /* One edge sprite in its own frame, to copy parts of it. */
  auto edge = [&gfx](unsigned int index, int w, int h) {
    Frame *part = gfx.create_frame(w, h);
    part->draw_sprite(0, 0, Data::AssetFramePopup, index);
    return part;
  };
  /* Fill dx..dx+dw of a horizontal edge with sx..sx+sw of the sprite. */
  auto fill_x = [frame](Frame *src, int dx, int dw, int dy, int sx, int sw,
                       int h) {
    for (int x = dx; x < dx + dw; x += sw) {
      frame->draw_frame(x, dy, sx, 0, src, std::min(sw, dx + dw - x), h);
    }
  };
  /* The same vertically for a side edge. */
  auto fill_y = [frame](Frame *src, int dx, int dy, int dh, int sy, int sh,
                       int w) {
    for (int y = dy; y < dy + dh; y += sh) {
      frame->draw_frame(dx, y, 0, sy, src, w, std::min(sh, dy + dh - y));
    }
  };

  const int top_h = 9;
  const int bottom_h = 7;
  const int side_w = 8;

  /* Top: left end, ornament in the middle, right end. */
  Frame *top = edge(0, 144, top_h);
  int ornament_x = (width - 56) / 2;
  frame->draw_frame(0, 0, 0, 0, top, 44, top_h);
  fill_x(top, 44, ornament_x - 44, 0, 12, 32, top_h);
  frame->draw_frame(ornament_x, 0, 44, 0, top, 56, top_h);
  fill_x(top, ornament_x + 56, width - 44 - ornament_x - 56, 0, 12, 32,
         top_h);
  frame->draw_frame(width - 44, 0, 100, 0, top, 44, top_h);
  delete top;

  /* Bottom: both ends with plain wood between. */
  Frame *bottom = edge(1, 144, bottom_h);
  int bottom_y = height - bottom_h;
  frame->draw_frame(0, bottom_y, 0, 0, bottom, 40, bottom_h);
  fill_x(bottom, 40, width - 80, bottom_y, 40, 64, bottom_h);
  frame->draw_frame(width - 40, bottom_y, 104, 0, bottom, 40, bottom_h);
  delete bottom;

  /* Sides: the whole sprite, then its middle repeated down. */
  int side_h = height - top_h - bottom_h;
  for (int i = 0; i < 2; i++) {
    Frame *side = edge(2 + i, side_w, 144);
    int x = (i == 0) ? 0 : width - side_w;
    frame->draw_frame(x, top_h, 0, 0, side, side_w, 144);
    fill_y(side, x, top_h + 144, side_h - 144, 16, 96, side_w);
    delete side;
  }
}
