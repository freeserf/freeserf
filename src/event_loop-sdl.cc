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

#include "src/version.h"  // for tick_length, I couldn't figure out how else to get it without redefinitions because of the rat's nest of header includes

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
  SDL_InitSubSystem(SDL_INIT_EVENTS | SDL_INIT_TIMER);

  eventUserTypeStep = SDL_RegisterEvents(2);
  if (eventUserTypeStep == (Uint32)-1) {
    throw ExceptionFreeserf("Failed to register SDL event");
  }
  eventUserTypeStep++;
}

// note SDL_Timer does not appear to create just a single timer run,
//  but it initiates a repeating, never-ending Timer!
Uint32
EventLoopSDL::timer_callback(Uint32 interval, void *param) {
  EventLoopSDL *eventLoop = static_cast<EventLoopSDL*>(param);
  SDL_Event event;
  event.type = eventLoop->eventUserTypeStep;
  event.user.type = eventLoop->eventUserTypeStep;
  event.user.code = 0;
  event.user.data1 = 0;
  event.user.data2 = 0;
  SDL_PushEvent(&event);
  //Log::Error["event_loop-sdl.cc"] << "inside EventLoopSDL::timer_callback, SDL_TimerID returning interval " << interval;
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

// event_loop() has been turned into a SDL based loop.
// The code for one iteration of the original game_loop is in game_loop_iter.
void
EventLoopSDL::run() {
  // note SDL_Timer does not appear to create just a single timer run,
  //  but it initiates a repeating, never-ending Timer!  each Timer expiration
  // triggers an SDL Event that causes the loop to run 
  //Log::Debug["event_loop-sdl.cc"] << "tick_length is " << tick_length;
  //SDL_TimerID timer_id = SDL_AddTimer(TICK_LENGTH, timer_callback, this);
  // need to detect when tick_interval is changed, and when detected
  //  stop the SDL Timer and replace it with one using the new tick_interval!
  SDL_TimerID timer_id = SDL_AddTimer(tick_length, timer_callback, this);
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
  
  // for FPS counter
  //int frames = 0;
  //int timenow = time(0);
  //int last_time = timenow;
  //int target_fps = 49;  // default game speed 2 is 49-50fps.  Adjusted later for game_speed

  // trying to debug zoom issues with quickly changing zoom
  bool zoom_changed = false;

  /* target fps for various speeds (best case scenario, tiny tiny screen, no players active)
  speed fps
  3   53/52
  4   56/55
  5   59/58
  6   63/62/61
  7   67/66
  8   72/71/70
  9   77/76
  10  83/82
  11  91/90
  12  97/96 << math says target should be 100fps, 1000/10, this is actually lagging!
  */

  // it seems freeserf caps FPS at 50 by default, it seems to be controlled by the TICK_LENGTH define# 
  // and SDL_ADDTimer controls it by creating a timer event every TICK_LENGTH settings intead of using an open SDL_PollEvent

  int last_tick_length = tick_length;

  while (SDL_WaitEvent(&event)) {

  /*
        if (button_left_down) {
          Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), DEBUG button_left_down";
        }else{
          Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), DEBUG button_left_up";
        }
        // NOTE - SDL shows middle/wheel mouse button as button2, not button3 as you might expect
        if (button_middle_down) {
          Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), DEBUG button_middle_down";
        }else{
          Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), DEBUG button_middle_up";
        }
        // NOTE - SDL shows right mouse button as button3, not button2 as you might expect
        if (button_right_down) {
          Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), DEBUG button_right_down";
        }else{
          Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), DEBUG button_right_up";
        }
  */

    //Log::Debug["event_loop-sdl.cc"] << "tick_length is " << tick_length;
    // TRIGGER RESET OF Timer here!
    if (tick_length != last_tick_length){
      Log::Debug["event_loop-sdl.cc"] << "tick_length changed from " << last_tick_length << " to " << tick_length << ", resetting SDL Timer";
      last_tick_length = tick_length;
      if (SDL_RemoveTimer(timer_id)){
        Log::Debug["event_loop-sdl.cc"] << "SDL_RemoveTimer successfully removed timer_id " << timer_id;
      }else{
        Log::Error["event_loop-sdl.cc"] << "SDL_RemoveTimer failed to remove timer_id " << timer_id << "!";
        throw ExceptionFreeserf("failed to remove SDL timer when changing game speed!  this would result in multiple timers or other strange behavior");
        return;
      }
      timer_id = SDL_AddTimer(tick_length, timer_callback, this);
      if (timer_id == 0) {
        Log::Error["event_loop-sdl.cc"] << "SDL_TimerID returned 0, indicating error setting timer.  If this becomes a problem, add code to capture the error";
        throw ExceptionFreeserf("failed to re-create SDL timer when changing game speed!  this would result in game stopping or other strange behavior");
        return;
      }
    }
    unsigned int current_ticks = SDL_GetTicks();
    //SDL_Keymod mod = SDL_GetModState();

    //timenow = time(0); // for FPS counter
    //zoom_changed = false; // trying to solve erratic zoom issue
    //MouseState * m = (MouseState*) userdata;  // trying to solve issue with mouse pointer not being reported exactly correctly


    switch (event.type) {
      case SDL_MOUSEBUTTONUP:
        //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEBUTTONUP";
        if (drag_button == event.button.button) {
          drag_button = 0;
          //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEBUTTONUP, drag_button = 0";
        }

        if (event.button.button <= 3) {
          //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEBUTTONUP, event.button.button " << event.button.button << " is <= 3";
          int x = static_cast<int>(static_cast<float>(event.button.x) *
                                   zoom_factor * screen_factor_x);
          int y = static_cast<int>(static_cast<float>(event.button.y) *
                                   zoom_factor * screen_factor_y);
          if (button_middle_down || event.button.button == 2){
            if (option_SpecialClickMiddle){
              notify_middle_click(x, y);  // this simply executes special click function
            }
          }else if (button_left_down && button_right_down){
            if (option_SpecialClickBoth){
              notify_special_click(x, y);
            }
          }else if (event.button.button == 1){
            SDL_Keymod mod = SDL_GetModState();
            notify_left_click(x, y, mod, (Event::Button)event.button.button);
          }else if (event.button.button == 3){
            notify_right_click(x, y);
          }
          //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEBUTTONUP, foo";

          if (option_SpecialClickDouble){
            if (current_ticks - last_click[event.button.button] <
                  MOUSE_TIME_SENSITIVITY &&
                event.button.x >= (last_click_x - MOUSE_MOVE_SENSITIVITY) &&
                event.button.x <= (last_click_x + MOUSE_MOVE_SENSITIVITY) &&
                event.button.y >= (last_click_y - MOUSE_MOVE_SENSITIVITY) &&
                event.button.y <= (last_click_y + MOUSE_MOVE_SENSITIVITY)) {
              notify_dbl_click(x, y, (Event::Button)event.button.button);
            }
            //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEBUTTONUP, foo2";
          }
          //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEBUTTONUP, foo3";
          last_click[event.button.button] = current_ticks;
          last_click_x = event.button.x;
          last_click_y = event.button.y;
        }

        //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEBUTTONUP, foo4";
        button_left_down = false;
        button_middle_down = false;
        button_right_down = false;
        break;
      case SDL_MOUSEBUTTONDOWN:
        //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEBUTTONDOWN";
        if (event.button.button <= 3) {
          // track which buttons down for detect special-click (both left and right buttons)
          if (event.button.button == 1) {
            //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEBUTTONDOWN left";
            button_left_down = true;
          }
          // NOTE - SDL shows middle/wheel mouse button as button2, not button3 as you might expect
          if (event.button.button == 2) {
            //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEBUTTONDOWN middle";
            button_middle_down = true;
          }
          // NOTE - SDL shows right mouse button as button3, not button2 as you might expect
          if (event.button.button == 3) {
            //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEBUTTONDOWN right";
            button_right_down = true;
          }
        }
        break;
      case SDL_MOUSEMOTION:
        //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEMOTION";
        for (int button = 1; button <= 3; button++) {
          if (event.motion.state & SDL_BUTTON(button)) {
            if (drag_button == 0) {
              //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEMOTION, drag_button was 0, seting drag_button and drag_x/y";
              drag_button = button;
              drag_x = event.motion.x;
              drag_y = event.motion.y;
              /* this doesn't work
              int wtf_x = 0;
              int wtf_y = 0;
              SDL_GetRelativeMouseState(&wtf_x, &wtf_y);
              drag_x = wtf_x;
              drag_y = wtf_y;
              */
            }

            //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEMOTION, drag_button " << drag_button << ", drag_x " << drag_x << ", drag_y " << drag_y;
            //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEMOTION, zoom_factor " << zoom_factor << ", screen_factor_x " << screen_factor_x << ", screen_factor_y " << screen_factor_y;
            //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEMOTION, event.motion.x " << event.motion.x << ", event.motion.y " << event.motion.y;

            int x = static_cast<int>(static_cast<float>(drag_x) *
                                     zoom_factor * screen_factor_x);
            int y = static_cast<int>(static_cast<float>(drag_y) *
                                     zoom_factor * screen_factor_y);

            if (option_InvertMouse){
              //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEMOTION, calling notify_drag inverted with x,y values " << x << ", " << y << ", xmove " << event.motion.x - drag_x << ", ymove " << (event.motion.y - drag_y) * -1;
              notify_drag(x, y, event.motion.x - drag_x, (event.motion.y - drag_y) * -1, (Event::Button)drag_button);
            }else{
              //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEMOTION, calling notify_drag normal with x,y values " << x << ", " << y << ", xmove " << event.motion.x - drag_x << ", ymove " << event.motion.y - drag_y;
              notify_drag(x, y, event.motion.x - drag_x, event.motion.y - drag_y, (Event::Button)drag_button);
            }

            //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), type SDL_MOUSEMOTION, calling SDL_WarpMouseInWindow with drag_x/y values " << drag_x << ", " << drag_y;
            SDL_WarpMouseInWindow(nullptr, drag_x, drag_y);

            break;
          }
        }
        break;
      case SDL_MOUSEWHEEL: {
          // the mousewheel is used to scroll the load game box, when in focus, otherwise it does game zoom
          //  this is a global extern bool in game-options.h, set in list.cc, the load game box functions
          if (is_list_in_focus){
            //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), is_list_in_focus is true, using SCROLL";
            notify_list_scroll(event.wheel.y);
          }else{
            //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), is_list_in_focus is false, using ZOOM";

            // I am seeing an issue where if zooming quickly the zoom is messed up
            //  trying to limit it to a single increment per game update/tick
            // NOPE THIS ISN'T THE CAUSE
            /*
            if (event.wheel.y > 1){
              Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), event.wheel.y is " << event.wheel.y << ", limit it to 1";
              event.wheel.y = 1;
            }else if (event.wheel.y < -1){
              event.wheel.y = -1;
              Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), event.wheel.y is " << event.wheel.y << ", limit it to -1";
            }
            */

            if (zoom_changed){
              Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::run(), zoom was already changed this SDL loop, not changing again";
              break;
            }

            if (option_InvertWheelZoom){
              // the wheel zoom is backwards unless reversed
              //  it seems Mac users may expect reversed zoom?
              //zoom(0.2f * static_cast<float>(event.wheel.y));
              //zoom(0.05f * static_cast<float>(event.wheel.y));  // adding zoom levels
              zoom(0.05f * static_cast<float>(event.wheel.y), 1);  // adding zoom type for centering, 1 means mousewheel zoom, centered on current mouse pointer position
              zoom_changed = true;
            }else{
              //zoom(0.2f * static_cast<float>((event.wheel.y * -1)));
              //zoom(0.05f * static_cast<float>((event.wheel.y * -1)));
              zoom(0.05f * static_cast<float>((event.wheel.y * -1)), 1); // adding zoom type for centering, 1 means mousewheel zoom, centered on current mouse pointer position
              zoom_changed = true;
            }
          }
        //}
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
          // Map scroll AND Load-game box scroll
          //  I think it may be okay to submit the event to both? assuming map won't scroll while game-init box open
          //  NO! it almost works, but notify_drag causes a focus_loose event which messes up the load game scrolling focus
          //  must exclude it when not in list scrolling mode
          case SDLK_UP: {
            if (!is_list_in_focus){
              notify_drag(0, 0, 0, -32, Event::ButtonLeft);  // hack for map scrolling
            }
            notify_arrow_key_pressed(0);  // for load game scrolling  //0=up,1=down,2=left,3=right
            break;
          }
          case SDLK_DOWN: {
            if (!is_list_in_focus){
              notify_drag(0, 0, 0, 32, Event::ButtonLeft); // hack for map scrolling
            }
            notify_arrow_key_pressed(1);  // for load game scrolling //0=up,1=down,2=left,3=right
            break;
          }
          case SDLK_LEFT: {
            if (!is_list_in_focus){
              notify_drag(0, 0, -32, 0, Event::ButtonLeft); // hack for map scrolling
            }
            break;
          }
          case SDLK_RIGHT: {
            if (!is_list_in_focus){
              notify_drag(0, 0, 32, 0, Event::ButtonLeft); // hack for map scrolling
            }
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

          // Video
          case SDLK_f:
            if (event.key.keysym.mod & KMOD_CTRL) {
              gfx.set_fullscreen(!gfx.is_fullscreen());
            }else{
              // if this isn't handled the 'f' key doesn't work for savegame names
              notify_key_pressed(event.key.keysym.sym, modifier);
            }
            break;
          case SDLK_RIGHTBRACKET:
            //zoom(-0.2f);
            //zoom(-0.05f);
            zoom(-0.05f, 0);  // adding zoom type for centering, 0 means keyboard zoom, centered on center of viewport
            break;
          case SDLK_LEFTBRACKET:
            //zoom(0.2f);
            //zoom(0.05f);
            zoom(-0.05f, 0);  // adding zoom type for centering, 0 means keyboard zoom, centered on center of viewport
            break;

          // Misc
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
          zoom(0.0, 0); // this re-applies any existing zoom but does not adjust
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
      default:
        if (event.type == eventUserTypeStep) {

          // show frames/updates per second
          //frames++;
          //int time_delta = timenow - last_time;
          //if (time_delta >= 1){
          //  target_fps = 1000 / tick_length; // same as TICKS_PER_SEC
          //  int fps_lag = target_fps - (frames / time_delta);
          //  if (fps_lag < 0){fps_lag = 0;}
          //  Log::Info["event_loop-sdl.cc"] << "in past " << time_delta << "sec, processed " << frames << " frames, target_fps " << target_fps << ", FPS loss:" << fps_lag;
          //  frames = 0;
          //  last_time = timenow;
          //}

          // Update and draw interface
          notify_update();

          if (screen == nullptr) {
            screen = gfx.get_screen_frame();
          }
          notify_draw(screen);

          // Swap video buffers
          gfx.swap_buffers();

          SDL_FlushEvent(eventUserTypeStep);

          zoom_changed = false; // trying to solve erratic zoom issue
        }
    }
  }

  SDL_RemoveTimer(timer_id);
  if (screen != nullptr) {
    delete screen;
    screen = nullptr;
  }
}

void
//EventLoopSDL::zoom(float delta) {
EventLoopSDL::zoom(float delta, int type) {
  Graphics &gfx = Graphics::get_instance();
  //unsigned int oldwidth = 0;
  //unsigned int oldheight = 0;
  //gfx.get_resolution(&oldwidth, &oldheight);
  //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::zoom, zoom_factor was " << gfx.get_zoom_factor() << ", resolution was " << oldwidth << " w / " << oldheight << " h ";
  float factor = gfx.get_zoom_factor();
  if (gfx.set_zoom_factor(factor + delta)) {
    zoom_factor = gfx.get_zoom_factor();
    gfx.set_zoom_type(type);
    //zoom_type = type;
    unsigned int width = 0;
    unsigned int height = 0;
    gfx.get_resolution(&width, &height);
    //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::zoom, zoom_factor is now " << gfx.get_zoom_factor() << ", resolution is now " << width << " w / " << height << " h ";
    //notify_resize(width, height);
    notify_zoom_resize(width, height);

  }

  //debug - this works now
  //int x = 0;
  //int y = 0;
  //gfx.get_mouse_cursor_coord(&x,&y);
  //Log::Debug["event_loop-sdl.cc"] << "inside EventLoopSDL::zoom, the mouse cursor pointer is at coord " << x << "," << y;

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

