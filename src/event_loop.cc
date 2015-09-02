/*
 * event_loop.cc - User and system events handling
 *
 * Copyright (C) 2012-2014  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/event_loop.h"

#include <cstdlib>

#include "src/event_loop-sdl.h"

event_loop_t::event_loop_t() {
}

void event_loop_t::add_handler(event_handler_t *handler) {
  event_handlers.push_back(handler);
}

void event_loop_t::del_handler(event_handler_t *handler) {
  event_handlers.remove(handler);
}

bool
event_loop_t::notify_handlers(event_t *event) {
  if (event_handlers.empty()) {
    return false;
  }

  bool result = false;

  event_handlers_t handlers = event_handlers;
  for (event_handlers_t::iterator it = handlers.begin();
       it != handlers.end(); ++it) {
    result |= (*it)->handle_event(event);
  }

  return result;
}

bool
event_loop_t::notify_click(int x, int y, event_button_t button) {
  event_t event;
  event.type = EVENT_TYPE_CLICK;
  event.x = x;
  event.y = y;
  event.button = button;
  return notify_handlers(&event);
}

bool
event_loop_t::notify_dbl_click(int x, int y, event_button_t button) {
  event_t event;
  event.type = EVENT_TYPE_DBL_CLICK;
  event.x = x;
  event.y = y;
  event.button = button;
  return notify_handlers(&event);
}

bool
event_loop_t::notify_drag(int x, int y, int dx, int dy, event_button_t button) {
  event_t event;
  event.type = EVENT_TYPE_DRAG;
  event.x = x;
  event.y = y;
  event.dx = dx;
  event.dy = dy;
  event.button = button;
  return notify_handlers(&event);
}

bool
event_loop_t::notify_key_pressed(unsigned char key, unsigned char morifier) {
  event_t event;
  event.type = EVENT_KEY_PRESSED;
  event.x = 0;
  event.y = 0;
  event.dx = key;
  event.dy = morifier;
  return notify_handlers(&event);
}

bool
event_loop_t::notify_resize(unsigned int width, unsigned int height) {
  event_t event;
  event.type = EVENT_RESIZE;
  event.x = 0;
  event.y = 0;
  event.dx = width;
  event.dy = height;
  return notify_handlers(&event);
}

bool
event_loop_t::notify_update() {
  event_t event;
  event.type = EVENT_UPDATE;
  event.x = 0;
  event.y = 0;
  return notify_handlers(&event);
}

bool
event_loop_t::notify_draw(frame_t *frame) {
  event_t event;
  event.type = EVENT_DRAW;
  event.x = 0;
  event.y = 0;
  event.object = frame;
  return notify_handlers(&event);
}
