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

#include "src/gfx.h"
#include "src/event_loop.h"

class GuiObject : public EventLoop::Handler {
 public:
  typedef enum GuiObjectClass {
    ClassNone = 0,
    ClassGameInitBox,
    ClassViewport,
    ClassPopupBox,
    ClassPanelBar,
    ClassMinimap,
    ClassNotificationBox,
    ClassSaveGameFileList,
    ClassLoadGameFileList,
    ClassSaveGameNameInput,
    ClassRandomSeedInput
  } GuiObjClass;

  const std::string NameGuiObjClass[11]{
    "GuiObjClass::ClassNone",
    "GuiObjClass::ClassGameInitBox",
    "GuiObjClass::ClassViewport",
    "GuiObjClass::ClassPopupBox",
    "GuiObjClass::ClassPanelBar",
    "GuiObjClass::ClassMinimap",
    "GuiObjClass::ClassNotificationBox",
    "GuiObjClass::ClassSaveGameFileList",
    "GuiObjClass::ClassLoadGameFileList",
    "GuiObjClass::ClassSaveGameNameInput",
    "GuiObjClass::ClassRandomSeedInput",
  };

 private:
  typedef std::list<GuiObject*> FloatList;
  FloatList floats;

 protected:
  int x, y;
  int width, height;
  bool displayed;
  bool enabled;
  bool redraw;
  GuiObject *parent;
  Frame *frame;
  static GuiObject *focused_object;
  bool focused;
  GuiObjClass objclass;  // PanelBar, PopupBox, GameInit, etc.
  int objtype;   // type within above class

  virtual void internal_draw() = 0;
  virtual void layout();
  virtual void store_prev_window_size();  // does nothing
  //virtual void store_prev_res() = 0;    // noop, overloaded
  //virtual void store_prev_res();

  virtual bool handle_left_click(int x, int y, int modifier) { return false; }
  virtual bool handle_click_right(int x, int y) { return false; }
  virtual bool handle_dbl_click(int x, int y, Event::Button button) { return false; }
  virtual bool handle_special_click(int x, int y) { return false; }  // noop, overloaded
  virtual bool handle_drag(int dx, int dy) { return true; }
  virtual bool handle_key_pressed(char key, int modifier) { return false; }
  virtual bool handle_arrow_key_pressed(uint8_t key) { return false; } // noop, overloaded
  virtual bool handle_list_scroll(int y) { return false; } // noop, overloaded
  virtual bool handle_focus_loose() { return false; }

  void delete_frame();

 public:
  GuiObject();
  virtual ~GuiObject();

  void draw(Frame *frame);
  void move_to(int x, int y);
  void get_position(int *x, int *y);
  void set_size(int width, int height);
  void get_size(int *width, int *height);
  void set_displayed(bool displayed);
  void set_enabled(bool enabled);
  void set_redraw();
  bool is_displayed() { return displayed; }
  GuiObject *get_parent() { return parent; }
  void set_parent(GuiObject *parent) { this->parent = parent; }
  bool point_inside(int point_x, int point_y);

  void add_float(GuiObject *obj, int x, int y);
  void del_float(GuiObject *obj);
  GuiObjClass get_objclass() { return objclass; }
  void set_objclass(GuiObjClass objclass_) { objclass = objclass_; }
  int get_objtype() { return objtype; }
  void set_objtype(int objtype_) { objtype = objtype_; }

  void set_focused();

  virtual bool handle_event(const Event *event);

  void play_sound(int sound);
  void play_sound(int sound, int source_type);
};

int gui_get_slider_click_value(int x);

#endif  // SRC_GUI_H_
