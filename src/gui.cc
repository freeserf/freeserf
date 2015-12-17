/*
 * gui.cc - Base functions for the GUI hierarchy
 *
 * Copyright (C) 2013  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/gui.h"

#include <algorithm>

#include "src/misc.h"

#ifdef min
# undef min
#endif

#ifdef max
# undef max
#endif

/* Get the resulting value from a click on a slider bar. */
int
gui_get_slider_click_value(int x) {
  return 1310 * clamp(0, x - 7, 50);
}

void
gui_object_t::set_size(int width, int height) {
  this->width = width;
  this->height = height;
}

gui_object_t::gui_object_t() {
  width = 0;
  height = 0;
  displayed = 0;
  enabled = 1;
  need_redraw = 0;
  parent = NULL;
}

void
gui_object_t::redraw(frame_t *frame) {
  draw(frame);
  need_redraw = 0;
}

void
gui_object_t::set_displayed(int displayed) {
  this->displayed = displayed;
  if (displayed) {
    set_redraw();
  } else if (parent != NULL) {
    parent->set_redraw();
  }
}

void
gui_object_t::set_enabled(int enabled) {
  this->enabled = enabled;
}

void
gui_object_t::set_redraw() {
  need_redraw = 1;
  if (parent != NULL) {
    parent->set_redraw_child(this);
  }
}

gui_container_t::gui_container_t() {
}

void
gui_container_t::set_redraw_child(gui_object_t *child) {
  set_redraw();
}
