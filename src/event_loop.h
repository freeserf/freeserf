/*
 * event_loop.h - User and system events handling
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

#ifndef SRC_EVENT_LOOP_H_
#define SRC_EVENT_LOOP_H_

#include <list>
#include <functional>

class Event {
 public:
  typedef enum Type {
    TypeLeftClick,
    TypeRightClick,
    TypeDoubleClick,
    TypeMiddleClick,
    TypeSpecialClick,
    TypeDrag,
    TypeKeyPressed,
    TypeArrowKeyPressed,
    TypeScroll,
    TypeResize,
    TypeUpdate,
    TypeDraw,
  } Type;

  typedef enum Button {
    ButtonLeft = 1,
    ButtonMiddle,
    ButtonRight
  } Button;

 public:
  void *object;
  int x, y;
  int dx, dy;
  Type type;
  Button button;
};

class Frame;

class Timer {
 public:
  class Handler {
   public:
    virtual ~Handler() {}
    virtual void on_timer_fired(unsigned int id) = 0;
  };

 protected:
  unsigned int id;
  unsigned int interval;
  Handler *handler;

  Timer(unsigned int _id, unsigned int _interval, Handler *_handler)
    : id(_id), interval(_interval), handler(_handler) {}

 public:
  virtual ~Timer() {}

  virtual void run() = 0;
  virtual void stop() = 0;

  static Timer *create(unsigned int _id, unsigned int _interval,
                       Handler *_handler);
};

class EventLoop {
 public:
  class Handler {
   public:
    virtual ~Handler() {}
    virtual bool handle_event(const Event *event) = 0;
  };

 protected:
  typedef std::list<Handler*> Handlers;
  typedef std::function<void(void*)> DeferredCall;

 protected:
  Handlers event_handlers;
  Handlers removed;
  static EventLoop *instance;

 public:
  static EventLoop &get_instance();
  virtual ~EventLoop() {}

  virtual void run() = 0;
  virtual void quit() = 0;
  virtual void deferred_call(DeferredCall call, void *data) = 0;

  void add_handler(Handler *handler);
  void del_handler(Handler *handler);

 protected:
  EventLoop();

  bool notify_handlers(Event *event);

  bool notify_left_click(int x, int y, unsigned char modifier, Event::Button button);
  bool notify_right_click(int x, int y);
  bool notify_dbl_click(int x, int y, Event::Button button);
  bool notify_middle_click(int x, int y); //, Event::Button button);
  bool notify_special_click(int x, int y); //, Event::Button button);
  bool notify_drag(int x, int y, int dx, int dy, Event::Button button);
  bool notify_key_pressed(unsigned char key, unsigned char modifier);
  bool notify_arrow_key_pressed(uint8_t key);  //0=up,1=down,2=left,3=right
  bool notify_scroll(int y);
  bool notify_resize(unsigned int width, unsigned int height);
  bool notify_update();
  bool notify_draw(Frame *frame);
};

#endif  // SRC_EVENT_LOOP_H_
