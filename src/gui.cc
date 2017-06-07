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
#include "src/audio.h"

/* Get the resulting value from a click on a slider bar. */
int
gui_get_slider_click_value(int x) {
  return 1310 * clamp(0, x - 7, 50);
}

void
GuiObject::layout() {
}

GuiObject *GuiObject::focused_object = NULL;

GuiObject::GuiObject() {
  x = 0;
  y = 0;
  width = 0;
  height = 0;
  displayed = false;
  enabled = true;
  redraw = true;
  parent = NULL;
  frame = NULL;
  focused = false;
}

GuiObject::~GuiObject() {
  if (focused_object == this) {
    focused_object = nullptr;
  }
  delete_frame();
}

void
GuiObject::delete_frame() {
  if (frame != NULL) {
    delete frame;
    frame = NULL;
  }
}

void
GuiObject::draw(Frame *_frame) {
  if (!displayed) {
    return;
  }

  if (frame == NULL) {
    frame = Graphics::get_instance()->create_frame(width, height);
  }

  if (redraw) {
    internal_draw();

    for (GuiObject *float_window : floats) {
      float_window->draw(frame);
    }

    redraw = false;
  }
  _frame->draw_frame(x, y, 0, 0, frame, width, height);
}

bool
GuiObject::handle_event(const Event *event) {
  if (!enabled || !displayed) {
    return false;
  }

  int event_x = event->x;
  int event_y = event->y;
  if (event->type == Event::TypeClick ||
      event->type == Event::TypeDoubleClick ||
      event->type == Event::TypeDrag) {
    event_x = event->x - x;
    event_y = event->y - y;
    if (event_x < 0 || event_y < 0 || event_x > width || event_y > height) {
      return false;
    }
  }

  Event internal_event;
  internal_event.type = event->type;
  internal_event.x = event_x;
  internal_event.y = event_y;
  internal_event.dx = event->dx;
  internal_event.dy = event->dy;
  internal_event.button = event->button;

  /* Find the corresponding float element if any */
  FloatList::reverse_iterator fl = floats.rbegin();
  for ( ; fl != floats.rend() ; ++fl) {
    bool result = (*fl)->handle_event(&internal_event);
    if (result != 0) {
      return result;
    }
  }

  bool result = false;
  switch (event->type) {
    case Event::TypeClick:
      if (event->button == Event::ButtonLeft) {
        result = handle_click_left(event_x, event_y);
      }
      break;
    case Event::TypeDrag:
      result = handle_drag(event->dx, event->dy);
      break;
    case Event::TypeDoubleClick:
      result = handle_dbl_click(event->x, event->y, event->button);
      break;
    case Event::TypeKeyPressed:
      result = handle_key_pressed(event->dx, event->dy);
      break;
    default:
      break;
  }

  if (result && (focused_object != this)) {
    if (focused_object != nullptr) {
      focused_object->focused = false;
      focused_object->handle_focus_loose();
      focused_object->set_redraw();
      focused_object = nullptr;
    }
  }

  return result;
}

void
GuiObject::set_focused() {
  if (focused_object != this) {
    if (focused_object != nullptr) {
      focused_object->focused = false;
      focused_object->handle_focus_loose();
      focused_object->set_redraw();
    }
    focused = true;
    focused_object = this;
    set_redraw();
  }
}

void
GuiObject::move_to(int x, int y) {
  this->x = x;
  this->y = y;
  set_redraw();
}

void
GuiObject::get_position(int *x, int *y) {
  if (x != NULL) {
    *x = this->x;
  }
  if (y != NULL) {
    *y = this->y;
  }
}

void
GuiObject::set_size(int width, int height) {
  delete_frame();
  this->width = width;
  this->height = height;
  layout();
  set_redraw();
}

void
GuiObject::get_size(int *width, int *height) {
  if (width != NULL) {
    *width = this->width;
  }
  if (height != NULL) {
    *height = this->height;
  }
}

void
GuiObject::set_displayed(bool displayed) {
  this->displayed = displayed;
  set_redraw();
}

void
GuiObject::set_enabled(bool enabled) {
  this->enabled = enabled;
}

void
GuiObject::set_redraw() {
  redraw = true;
  if (parent != NULL) {
    parent->set_redraw();
  }
}

bool
GuiObject::point_inside(int point_x, int point_y) {
  return (point_x >= x && point_y >= y &&
          point_x < x + width && point_y < y + height);
}

void
GuiObject::add_float(GuiObject *obj, int x, int y) {
  obj->set_parent(this);
  floats.push_back(obj);
  obj->move_to(x, y);
  set_redraw();
}

void
GuiObject::del_float(GuiObject *obj) {
  obj->set_parent(NULL);
  floats.remove(obj);
  set_redraw();
}

void
GuiObject::play_sound(int sound) {
  Audio *audio = Audio::get_instance();
  Audio::Player *player = audio->get_sound_player();
  if (player != NULL) {
    player->play_track(sound);
  }
}
