/*
 * box-frame.h - Frame of the boxes in the style of the popups
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

#ifndef SRC_BOX_FRAME_H_
#define SRC_BOX_FRAME_H_

class Frame;

/* Draw the frame of the popup boxes along the edges of a box of any size
   (at least the 144x160 of a popup). */
void draw_box_frame(Frame *frame, int width, int height);

#endif  // SRC_BOX_FRAME_H_
