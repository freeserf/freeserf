/*
 * event_loop-sdl.h - User and system events handling
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

#ifndef SRC_EVENT_LOOP_SDL_H_
#define SRC_EVENT_LOOP_SDL_H_

#include <list>
#include <SDL.h>

#include "src/event_loop.h"

class EventLoopSDL : public EventLoop {
 public:
  typedef enum EventUserType {
    EventUserTypeStep,
    EventUserTypeQuit,
    EventUserTypeCall,
  } EventUserType;

 protected:
  std::list<DeferredCall> deferred_calls;
  float zoom_factor;
  float screen_factor_x;
  float screen_factor_y;
  Uint32 eventUserTypeStep;

 public:
  EventLoopSDL();

  virtual void quit();
  virtual void run();
  virtual void deferred_call(DeferredCall call, void *data);

 protected:
  void zoom(float delta);
  static Uint32 timer_callback(Uint32 interval, void *param);
};

#endif  // SRC_EVENT_LOOP_SDL_H_
