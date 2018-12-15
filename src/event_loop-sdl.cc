/*
 * event_loop-sdl.cc - User and system events handling
 *
 * Copyright (C) 2012-2018  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/event_loop-sdl.h"

#include <SDL.h>

#include "src/log.h"
#include "src/gfx.h"
#include "src/freeserf.h"
#include "src/video-sdl.h"

EventLoop &
EventLoop::get_instance() {
  static EventLoopSDL event_loop;
  return event_loop;
}

/* How fast consequtive mouse events need to be generated
 in order to be interpreted as click and double click. */
#define MOUSE_TIME_SENSITIVITY  600
/* How much the mouse can move between events to be still
 considered as a double click. */
#define MOUSE_MOVE_SENSITIVITY  8

EventLoopSDL::EventLoopSDL()
  : zoom_factor(1.f)
  , screen_factor_x(1.f)
  , screen_factor_y(1.f) {
}

static Uint32
timer_callback(Uint32 interval, void *param) {
  SDL_Event event;
  event.type = SDL_USEREVENT;
  event.user.type = SDL_USEREVENT;
  event.user.code = EventLoopSDL::EventUserTypeStep;
  event.user.data1 = 0;
  event.user.data2 = 0;
  SDL_PushEvent(&event);

  return interval;
}

void
EventLoopSDL::quit() {
  SDL_Event event;
  event.type = SDL_USEREVENT;
  event.user.type = SDL_USEREVENT;
  event.user.code = EventUserTypeQuit;
  event.user.data1 = 0;
  event.user.data2 = 0;
  SDL_PushEvent(&event);
}

void
EventLoopSDL::deferred_call(DeferredCall call, void *data) {
  deferred_calls.push_back(call);

  SDL_Event event;
  event.type = SDL_USEREVENT;
  event.user.type = SDL_USEREVENT;
  event.user.code = EventUserTypeCall;
  SDL_PushEvent(&event);
}

/* event_loop() has been turned into a SDL based loop.
 The code for one iteration of the original game_loop is
 in game_loop_iter. */
void
EventLoopSDL::run() {
  SDL_InitSubSystem(SDL_INIT_EVENTS | SDL_INIT_TIMER);

  SDL_TimerID timer_id = SDL_AddTimer(TICK_LENGTH, timer_callback, 0);
  if (timer_id == 0) {
    return;
  }

  int drag_button = 0;
  int drag_x = 0;
  int drag_y = 0;

  unsigned int last_click[6] = {0};
  int last_click_x = 0;
  int last_click_y = 0;

  SDL_Event event;

  Graphics &gfx = Graphics::get_instance();
  Frame *screen = nullptr;
  gfx.get_screen_factor(&screen_factor_x, &screen_factor_y);

  while (SDL_WaitEvent(&event)) {
    unsigned int current_ticks = SDL_GetTicks();

    switch (event.type) {
      case SDL_MOUSEBUTTONUP:
        if (drag_button == event.button.button) {
          drag_button = 0;
        }

        if (event.button.button <= 3) {
          int x = static_cast<int>(static_cast<float>(event.button.x) *
                                   zoom_factor * screen_factor_x);
          int y = static_cast<int>(static_cast<float>(event.button.y) *
                                   zoom_factor * screen_factor_y);
          notify_click(x, y, (Event::Button)event.button.button);

          if (current_ticks - last_click[event.button.button] <
                MOUSE_TIME_SENSITIVITY &&
              event.button.x >= (last_click_x - MOUSE_MOVE_SENSITIVITY) &&
              event.button.x <= (last_click_x + MOUSE_MOVE_SENSITIVITY) &&
              event.button.y >= (last_click_y - MOUSE_MOVE_SENSITIVITY) &&
              event.button.y <= (last_click_y + MOUSE_MOVE_SENSITIVITY)) {
            notify_dbl_click(x, y, (Event::Button)event.button.button);
          }

          last_click[event.button.button] = current_ticks;
          last_click_x = event.button.x;
          last_click_y = event.button.y;
        }
        break;
      case SDL_MOUSEBUTTONDOWN:
        break;
      case SDL_MOUSEMOTION:
        for (int button = 1; button <= 3; button++) {
          if (event.motion.state & SDL_BUTTON(button)) {
            if (drag_button == 0) {
              drag_button = button;
              drag_x = event.motion.x;
              drag_y = event.motion.y;
            }

            int x = static_cast<int>(static_cast<float>(drag_x) *
                                     zoom_factor * screen_factor_x);
            int y = static_cast<int>(static_cast<float>(drag_y) *
                                     zoom_factor * screen_factor_y);

            notify_drag(x, y,
                        event.motion.x - drag_x, event.motion.y - drag_y,
                        (Event::Button)drag_button);

            SDL_WarpMouseInWindow(nullptr, drag_x, drag_y);

            break;
          }
        }
        break;
      case SDL_MOUSEWHEEL: {
        SDL_Keymod mod = SDL_GetModState();
        if ((mod & KMOD_CTRL) != 0) {
          zoom(0.2f * static_cast<float>(event.wheel.y));
        }
        break;
      }
      case SDL_KEYDOWN: {
        if (event.key.keysym.sym == SDLK_q &&
            (event.key.keysym.mod & KMOD_CTRL)) {
          quit();
          break;
        }

        unsigned char modifier = 0;
        if (event.key.keysym.mod & KMOD_CTRL) {
          modifier |= 1;
        }
        if (event.key.keysym.mod & KMOD_SHIFT) {
          modifier |= 2;
        }
        if (event.key.keysym.mod & KMOD_ALT) {
          modifier |= 4;
        }

        switch (event.key.keysym.sym) {
          /* Map scroll */
          case SDLK_UP: {
            notify_drag(0, 0, 0, -32, Event::ButtonLeft);
            break;
          }
          case SDLK_DOWN: {
            notify_drag(0, 0, 0, 32, Event::ButtonLeft);
            break;
          }
          case SDLK_LEFT: {
            notify_drag(0, 0, -32, 0, Event::ButtonLeft);
            break;
          }
          case SDLK_RIGHT: {
            notify_drag(0, 0, 32, 0, Event::ButtonLeft);
            break;
          }

          case SDLK_PLUS:
          case SDLK_KP_PLUS:
          case SDLK_EQUALS:
            notify_key_pressed('+', 0);
            break;
          case SDLK_MINUS:
          case SDLK_KP_MINUS:
            notify_key_pressed('-', 0);
            break;

            /* Video */
          case SDLK_f:
            if (event.key.keysym.mod & KMOD_CTRL) {
              gfx.set_fullscreen(!gfx.is_fullscreen());
            }
            break;
          case SDLK_RIGHTBRACKET:
            zoom(-0.2f);
            break;
          case SDLK_LEFTBRACKET:
            zoom(0.2f);
            break;

            /* Misc */
          case SDLK_F10:
            notify_key_pressed('n', 1);
            break;

          default:
            notify_key_pressed(event.key.keysym.sym, modifier);
            break;
        }

        break;
      }
      case SDL_QUIT:
        notify_key_pressed('c', 1);
        break;
      case SDL_WINDOWEVENT:
        if (SDL_WINDOWEVENT_SIZE_CHANGED == event.window.event) {
          unsigned int width = event.window.data1;
          unsigned int height = event.window.data2;
          gfx.set_resolution(width, height, gfx.is_fullscreen());
          gfx.get_screen_factor(&screen_factor_x, &screen_factor_y);
          notify_resize(width, height);
        }
        break;
      case SDL_USEREVENT:
        switch (event.user.code) {
          case EventUserTypeQuit:
            SDL_RemoveTimer(timer_id);
            if (screen != nullptr) {
              delete screen;
              screen = nullptr;
            }
            return;
          case EventUserTypeStep:
            /* Update and draw interface */
            notify_update();

            if (screen == nullptr) {
              screen = gfx.get_screen_frame();
            }
            notify_draw(screen);

            /* Swap video buffers */
            gfx.swap_buffers();
            break;
          case EventUserTypeCall: {
            while (!deferred_calls.empty()) {
              deferred_calls.front()(nullptr);
              deferred_calls.pop_front();
            }
            break;
          }
          default:
            break;
        }
        break;
    }
  }

  SDL_RemoveTimer(timer_id);
  if (screen != nullptr) {
    delete screen;
    screen = nullptr;
  }
}

void
EventLoopSDL::zoom(float delta) {
  Graphics &gfx = Graphics::get_instance();
  float factor = gfx.get_zoom_factor();
  if (gfx.set_zoom_factor(factor + delta)) {
    zoom_factor = gfx.get_zoom_factor();
    unsigned int width = 0;
    unsigned int height = 0;
    gfx.get_resolution(&width, &height);
    notify_resize(width, height);
  }
}

class TimerSDL : public Timer {
 protected:
  SDL_TimerID timer_id;

 public:
  TimerSDL(unsigned int _id, unsigned int _interval, Timer::Handler *_handler)
    : Timer(_id, _interval, _handler), timer_id(0) {}

  virtual ~TimerSDL() {
    stop();
  }

  virtual void run() {
    if (timer_id == 0) {
      timer_id = SDL_AddTimer(interval, TimerSDL::callback, this);
    }
  }

  virtual void stop() {
    if (timer_id != 0) {
      SDL_RemoveTimer(timer_id);
      timer_id = 0;
    }
  }

  static Uint32 callback(Uint32 interval, void *param) {
    TimerSDL *timer = reinterpret_cast<TimerSDL*>(param);
    if (timer->handler != nullptr) {
      timer->handler->on_timer_fired(timer->id);
    }
    return interval;
  }
};

Timer *
Timer::create(unsigned int _id, unsigned int _interval,
              Timer::Handler *_handler) {
  return new TimerSDL(_id, _interval, _handler);
}
