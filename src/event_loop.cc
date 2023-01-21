/*
 * event_loop.cc - User and system events handling
 *
 * Copyright (C) 2012-2017  Jon Lund Steffensen <jonlst@gmail.com>
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

#include <algorithm>

EventLoop *
EventLoop::instance = nullptr;

EventLoop::EventLoop() {
}

void
EventLoop::add_handler(Handler *handler) {
  event_handlers.push_back(handler);
}

void
EventLoop::del_handler(Handler *handler) {
  event_handlers.remove(handler);
  removed.push_back(handler);
}

bool
EventLoop::notify_handlers(Event *event) {
  if (event_handlers.empty()) {
    return false;
  }

  bool result = false;

  Handlers handlers = event_handlers;
  for (Handler *handler : handlers) {
    if (std::find(removed.begin(), removed.end(), handler) == removed.end()) {
      result |= handler->handle_event(event);
    }
  }

  removed.clear();

  return result;
}

bool
EventLoop::notify_left_click(int x, int y, unsigned char modifier, Event::Button button) {
  Event event;
  event.type = Event::TypeLeftClick;
  event.x = x;
  event.y = y;
  event.dy = modifier;
  event.button = button;
  return notify_handlers(&event);
}

bool
EventLoop::notify_right_click(int x, int y) {
  Event event;
  event.type = Event::TypeRightClick;
  event.x = x;
  event.y = y;
  //event.button = button;
  return notify_handlers(&event);
}

bool
EventLoop::notify_dbl_click(int x, int y, Event::Button button) {
  Event event;
  event.type = Event::TypeDoubleClick;
  event.x = x;
  event.y = y;
  event.button = button;
  return notify_handlers(&event);
}

bool
EventLoop::notify_middle_click(int x, int y){ //,Event::Button button) {
  Event event;
  event.type = Event::TypeMiddleClick;
  event.x = x;
  event.y = y;
  //event.button = button;
  return notify_handlers(&event);
}

bool
EventLoop::notify_special_click(int x, int y){ //,Event::Button button) {
  Event event;
  event.type = Event::TypeSpecialClick;
  event.x = x;
  event.y = y;
  //event.button = button;
  return notify_handlers(&event);
}

bool
EventLoop::notify_drag(int x, int y, int dx, int dy, Event::Button button) {
  Event event;
  event.type = Event::TypeDrag;
  event.x = x;
  event.y = y;
  event.dx = dx;
  event.dy = dy;
  event.button = button;
  return notify_handlers(&event);
}

bool
EventLoop::notify_key_pressed(unsigned char key, unsigned char modifier) {
  Event event;
  event.type = Event::TypeKeyPressed;
  event.x = 0;
  event.y = 0;
  event.dx = key;
  event.dy = modifier;
  return notify_handlers(&event);
}

// 0=up, 1=down, 3=left, 4=right
bool
EventLoop::notify_arrow_key_pressed(unsigned char key) {
  Event event;
  event.type = Event::TypeArrowKeyPressed;
  event.x = 0;
  event.y = 0;
  event.dx = key; // this is where the key value is passed
  return notify_handlers(&event);
}

bool
EventLoop::notify_list_scroll(int y){
  Event event;
  event.type = Event::TypeListScroll;
  event.x = 0;
  event.y = 0;
  event.dx = y; // this is where the y value is passed
  return notify_handlers(&event);
}

bool
EventLoop::notify_resize(unsigned int width, unsigned int height) {
  Event event;
  event.type = Event::TypeResize;
  event.x = 0;
  event.y = 0;
  event.dx = width;
  event.dy = height;
  return notify_handlers(&event);
}

bool
EventLoop::notify_zoom_resize(unsigned int width, unsigned int height) {
  Event event;
  event.type = Event::TypeZoom;
  event.x = 0;
  event.y = 0;
  event.dx = width;
  event.dy = height;
  return notify_handlers(&event);
}

bool
EventLoop::notify_update() {
  Event event;
  event.type = Event::TypeUpdate;
  event.x = 0;
  event.y = 0;
  return notify_handlers(&event);
}

bool
EventLoop::notify_draw(Frame *frame) {
  Event event;
  event.type = Event::TypeDraw;
  event.x = 0;
  event.y = 0;
  event.object = frame;
  return notify_handlers(&event);
}
