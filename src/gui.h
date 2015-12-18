/*
 * gui.h - Base functions for the GUI hierarchy
 *
 * Copyright (C) 2012  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef SRC_GUI_H_
#define SRC_GUI_H_

#include <list>

#include "src/misc.h"
BEGIN_EXT_C
  #include "gfx.h"
END_EXT_C
#include "src/event_loop.h"

class gui_object_t : public event_handler_t {
 private:
  typedef std::list<gui_object_t*> float_list_t;
  float_list_t floats;

 protected:
  int x, y;
  int width, height;
  bool displayed;
  bool enabled;
  bool redraw;
  gui_object_t *parent;
  frame_t *frame;
  static gui_object_t *focused_object;
  bool focused;

  virtual void internal_draw() = 0;
  virtual void layout();

  virtual bool handle_click_left(int x, int y) { return 0; }
  virtual bool handle_dbl_click(int x, int y, event_button_t button) {
    return 0; }
  virtual bool handle_drag(int dx, int dy) { return 0; }
  virtual bool handle_key_pressed(char key, int modifier) { return 0; }
  virtual bool handle_focus_loose() { return 0; }

  void delete_frame();

 public:
  gui_object_t();
  virtual ~gui_object_t();

  void draw(frame_t *frame);
  void move_to(int x, int y);
  void get_position(int *x, int *y);
  void set_size(int width, int height);
  void get_size(int *width, int *height);
  void set_displayed(bool displayed);
  void set_enabled(bool enabled);
  void set_redraw();
  bool is_displayed() { return displayed; }
  gui_object_t *get_parent() { return parent; }
  void set_parent(gui_object_t *parent) { this->parent = parent; }
  bool point_inside(int point_x, int point_y);

  void add_float(gui_object_t *obj, int x, int y);
  void del_float(gui_object_t *obj);

  virtual bool handle_event(const event_t *event);
};

int gui_get_slider_click_value(int x);

#endif  // SRC_GUI_H_
