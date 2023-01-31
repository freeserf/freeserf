/*
 * gui.cc - Base functions for the GUI hierarchy
 *
 * Copyright (C) 2013-2018  Jon Lund Steffensen <jonlst@gmail.com>
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
#include "src/lookup.h"  // for DataSourceType enum

#include <SDL.h>  // for turning of mouse pointer during dragging because of its weird behavior I don't understand yet

/* Get the resulting value from a click on a slider bar. */
int
gui_get_slider_click_value(int x) {
  return 1310 * clamp(0, x - 7, 50);
}

void
GuiObject::layout() {
  // this function only exists to allow overloading by Viewport and Interface, I think, but it is important for those
  //  this function cannot be removed, and the upstream call to it cannot be removed either!
  Log::Debug["gui.cc"] << "the useless function GuiObject::layout() has been called";
}

//void
//GuiObject::store_prev_res() {
//  Log::Debug["gui.cc"] << "inside GuiObject::store_prev_res, doing nothing";
//  //store_prev_res();
//}

void
GuiObject::store_prev_window_size() {
  Log::Debug["gui.cc"] << "inside GuiObject::store_prev_window_size, doing nothing";
  //store_prev_res();
}

GuiObject *GuiObject::focused_object = nullptr;

GuiObject::GuiObject() {
  x = 0;
  y = 0;
  width = 0;
  height = 0;
  displayed = false;
  enabled = true;
  redraw = true;
  parent = nullptr;
  frame = nullptr;
  focused = false;
  objclass = GuiObjClass::ClassNone;
  objtype = 0;
  being_dragged = false;
}

GuiObject::~GuiObject() {
  if (focused_object == this) {
    focused_object = nullptr;
  }
  delete_frame();
}

void
GuiObject::delete_frame() {
  if (frame != nullptr) {
    delete frame;
    frame = nullptr;
  }
}

void
GuiObject::draw(Frame *_frame) {
  if (!displayed) {
    return;
  }

  //Log::Debug["event_loop.cc"] << "inside GuiObject::draw, this->objclass " << this->get_objclass();

  if (frame == nullptr) {
    //Log::Debug["event_loop.cc"] << "inside GuiObject::draw, this->objclass " << this->get_objclass() << " frame is nullptr, creating new frame";
    frame = Graphics::get_instance().create_frame(width, height);
  }

  if (redraw) {
    //Log::Debug["event_loop.cc"] << "inside GuiObject::draw, this->objclass " << this->get_objclass() << " redraw is true, calling internal_draw";
    internal_draw();

    for (GuiObject *float_window : floats) {
      //Log::Debug["event_loop.cc"] << "inside GuiObject::draw, about to call float->draw for float with class " << float_window->get_objclass() << " and type " << float_window->get_objtype();
      float_window->draw(frame);
    }

    redraw = false;
  }
  //Log::Debug["event_loop.cc"] << "inside GuiObject::draw, this->objclass " << this->get_objclass() << " calling draw_frame";
  _frame->draw_frame(x, y, 0, 0, frame, width, height);
}

bool
GuiObject::handle_event(const Event *event) {
  Log::Debug["gui.cc"] << "start of GuiObject::handle_event with event type #" << event->type;
  Log::Debug["gui.cc"] << "start of GuiObject::handle_event with event type " << event->type << " / " << NameGuiObjEvent[event->type] << " and objclass " << objclass << " / " << NameGuiObjClass[objclass];
  if (!enabled || !displayed) {
    Log::Debug["gui.cc"] << "inside GuiObject::handle_event with event type " << event->type << " / " << NameGuiObjEvent[event->type] << " and objclass " << objclass << " / " << NameGuiObjClass[objclass] << ", rejected because !enabled or !displayed";
    return false;
  }

  Log::Debug["event_loop.cc"] << "inside GuiObject::handle_event with event type " << event->type << " / " << NameGuiObjEvent[event->type] << " and objclass " << get_objclass() << " / " << NameGuiObjClass[get_objclass()] << " this object is enabled and displayed";

  //
  // check to see if the mouse is within a Popup window so we can determine
  //  which object should handle/ignore it.  
  // If the position is outside the popup we return false (except with certain Drag rules)
  //  otherwise we continue which lets the click be handled by popup
  //
  int event_x = event->x;
  int event_y = event->y;
  bool in_scope = true;  // set to true if the mouse pointer is not within the GuiObject's area (i.e. if within the popup window, or panelbar)
  if (event->type == Event::TypeLeftClick ||
      event->type == Event::TypeRightClick ||
      event->type == Event::TypeDoubleClick ||
      event->type == Event::TypeMiddleClick ||
      event->type == Event::TypeSpecialClick ||
      event->type == Event::TypeMouseButtonDown ||
      event->type == Event::TypeDrag) {
    // I think this is adjusting by the offset of the GUI object's starting pos 0,0 (top-left) for popup, panelbar, etc.
    event_x = event->x - x;  
    event_y = event->y - y;
    if (event_x < 0 || event_y < 0 || event_x > width || event_y > height) {
      // mouse pos is outside this gui object area
      in_scope = false;
      Log::Debug["gui.cc"] << "inside GuiObject::handle_event with event type " << event->type << " / " << NameGuiObjEvent[event->type] << " and objclass " << get_objclass() << " / " << NameGuiObjClass[get_objclass()] << " and objtype " << get_objtype() << ", click/drag event is NOT in_scope";
    }else{
      Log::Debug["gui.cc"] << "inside GuiObject::handle_event with event type " << event->type << " / " << NameGuiObjEvent[event->type] << " and objclass " << get_objclass() << " / " << NameGuiObjClass[get_objclass()] << " and objtype " << get_objtype() << ", click/drag event is in_scope";
    }
  }

  // special handling for dragging
  //  when any mouse button is pressed DOWN, the current position of the mouse pointer on button-down
  //  sets the 'being_dragged' bool to true for whichever GuiObject/popup the pointer is within
  // The if a Drag event is found, the original mouse pos determines scope and NOT the current mouse
  //  pointer pos because as the GuiObject is dragged the mouse pointer moves and would otherwise
  //  start dragging the object now under the pointer when the mouse is no longer over the area of the
  //  popup.  NOTE that it should be easier to simply move the moues pointer ALONG WITH the popup
  //  but I couldn't figure out how
  bool skip_event = !in_scope;
  //if (objclass != GuiObjectClass::ClassInterface && event->type == Event::TypeDrag){
  if (event->type == Event::TypeDrag){
    //if (!in_scope && being_dragged){
    if (!in_scope && focused){
      //Log::Debug["event_loop.cc"] << "inside GuiObject::handle_event, GuiObjectClass " << NameGuiObjClass[objclass] << ", event is out of bounds BUT being_dragged bool is true so continuing as if it were in focus";
      Log::Debug["gui.cc"] << "inside GuiObject::handle_event, GuiObjectClass " << NameGuiObjClass[objclass] << " and objtype " << get_objtype() << ", event is out of bounds BUT focused bool is true so continuing as if it were in focus";
      skip_event = false;
    //}else if (!in_scope && !being_dragged){
    }else if (!in_scope && !focused){
      //Log::Debug["event_loop.cc"] << "inside GuiObject::handle_event, GuiObjectClass " << NameGuiObjClass[objclass] << ", event is out of bounds and not being_dragged, ignoring this event for this game object";
      Log::Debug["gui.cc"] << "inside GuiObject::handle_event, GuiObjectClass " << NameGuiObjClass[objclass] << " and objtype " << get_objtype() << ", event is out of bounds and not focused, ignoring this event for this game object";
      skip_event = true;
    //}else if (in_scope && being_dragged){
    }else if (in_scope && focused){
      //Log::Debug["event_loop.cc"] << "inside GuiObject::handle_event, GuiObjectClass " << NameGuiObjClass[objclass] << ", event is in bounds and being_dragged, handling normally";
      Log::Debug["gui.cc"] << "inside GuiObject::handle_event, GuiObjectClass " << NameGuiObjClass[objclass] << " and objtype " << get_objtype() << ", event is in bounds and focused, handling normally";
      // to avoid issue where slight movement while clicking a popup button is intepreted as a drag,
      //  force the initial drag to be a strong motion to "release" the popup into drag mode
      if (being_dragged == false){
        if (objclass == GuiObjectClass::ClassPopupBox) {
          if (event->dx < -4 || event->dx > 4 || event->dy < -4 || event->dy > 4){
            Log::Debug["gui.cc"] << "inside GuiObject::handle_event, GuiObjectClass " << NameGuiObjClass[objclass] << " and objtype " << get_objtype() << ", event is in bounds and focused, and drag pressure is enough to release the popup to drag mode, setting being_dragged bool true";
            being_dragged = true;
            Log::Debug["gui.cc"] << "inside GuiObject::handle_event, GuiObjectClass " << NameGuiObjClass[objclass] << " and objtype " << get_objtype() << ", event is in bounds and focused, and drag pressure is enough to release the popup to drag mode, setting global is_dragging_popup bool true";
            is_dragging_popup = true; // global bool, for telling SDL event loop if the mouse pointer should try to stay put (for viewport/minimap) or move with drag motion (for popups)
            //Graphics &gfx = Graphics::get_instance();
            //gfx.get_mouse_cursor_coord(&mouse_x_after_drag, &mouse_y_after_drag);  // store the CURRENT mouse x/y BEFORE dragging so its adjusted position can track the drag
            Log::Debug["gui.cc"] << "inside GuiObject::handle_event, GuiObjectClass " << NameGuiObjClass[objclass] << " and objtype " << get_objtype() << ", event is in bounds and focused, and drag pressure is enough to release the popup to drag mode, hiding the mouse pointer until dragging complete";
            SDL_ShowCursor(SDL_DISABLE);
            skip_event = false;
          }else{
            Log::Debug["gui.cc"] << "inside GuiObject::handle_event, GuiObjectClass " << NameGuiObjClass[objclass] << " and objtype " << get_objtype() << ", event is in bounds and focused, but drag pressure is not enough, ignoring drag and allowing event to be handled as click";
            skip_event = true;
          }
        }else{
          // this is not a popup, must be the Viewport (MiniMap is inside of a Popup)
          being_dragged = true;
          skip_event = false;
        }
      }else{
        Log::Debug["gui.cc"] << "inside GuiObject::handle_event, GuiObjectClass " << NameGuiObjClass[objclass] << " and objtype " << get_objtype() << ", event is in bounds and focused, and being_dragged bool is already true!";
      }
    //}else if (in_scope && !being_dragged){
    }else if (in_scope && !focused){
      //Log::Debug["event_loop.cc"] << "inside GuiObject::handle_event, GuiObjectClass " << NameGuiObjClass[objclass] << ", event is in bounds BUT not being_dragged, ignoring this event for this object becaus some other object is being dragged";
      Log::Debug["gui.cc"] << "inside GuiObject::handle_event, GuiObjectClass " << NameGuiObjClass[objclass] << " and objtype " << get_objtype() << ", event is in bounds BUT not focused, ignoring this event for this object becaus some other object is being dragged";
      skip_event = true;
    }
  }

  //if (skip_event){
  //  Log::Debug["event_loop.cc"] << "inside GuiObject::handle_event, GuiObjectClass " << NameGuiObjClass[objclass] << ", skip_event is true, returning false";
  //  return false;
  //}

  //
  // if this point reached, the event is in scope for this game object
  //

  Event internal_event;
  internal_event.type = event->type;
  internal_event.x = event_x;
  internal_event.y = event_y;
  internal_event.dx = event->dx;
  internal_event.dy = event->dy;
  internal_event.button = event->button;

  // Find the corresponding float element if any //
  // this triggers the event for ALL FLOATS one by one
  //  until one returns true.  Floats that ignore the action
  //  will return false to indicate such, and floats that do not even have a
  //  ::handle_<event_type> action will default to the virtual GuiObject function
  //  which simply returns false
  //     Once any float returns true, the no other floats are checked!
  //      if we want to change this to allow multiple floats to process same
  //      event, simply don't "return" early
  //      NO THIS DOESN'T WORK
  //  ADDITIONAL NOTE - there is not a single 'floats' list, but one for EACH GUI OBJECT
  //   this is so the add_float function create additional sub-floats for popups the have multiple
  //   float components such as minimap, save/load game, anything with a text input
  //  
  //int floats_size = floats.size();
  //int floats_i = 0;
  FloatList::reverse_iterator fl = floats.rbegin();
  for ( ; fl != floats.rend() ; ++fl) {
    //floats_i++;
    //Log::Debug["event_loop.cc"] << "inside GuiObject::handle_event for fl with objclass " << int((*fl)->get_objclass()) << " and objtype " << int((*fl)->get_objtype());
    // failed attempt to only zoom the viewport, not the UI elements
    //if (internal_event.type == Event::TypeZoom
    // && (*fl)->get_objclass() != GuiObjClass::ClassViewport){
    //  Log::Debug["event_loop.cc"] << "inside GuiObject::handle_event, not zooming non-Viewport float";
    //  continue;
    //}
    //if (skip_event && (*fl) == this){
    //  Log::Debug["event_loop.cc"] << "inside GuiObject::handle_event for fl with objclass " << int((*fl)->get_objclass()) << " and objtype " << int((*fl)->get_objtype()) << " this fl is the current object and is marked skip_event";
    //  continue;
    //}

    //Log::Debug["gui.cc"] << "inside GuiObject::handle_event for fl,  floats_size " << floats_size << ", floats_i " << floats_i;
    //if (floats_i > floats_size){
    //  // don't handle the last one
    //  break;
    //}

    Log::Debug["gui.cc"] << "inside GuiObject::handle_event for fl with objclass " << int((*fl)->get_objclass()) << " and objtype " << int((*fl)->get_objtype()) << ", calling handle_event for this fl";
    bool result = (*fl)->handle_event(&internal_event);
    if (result != 0) {
      // stop checking other floats as one seems to have handled this
      Log::Debug["gui.cc"] << "inside GuiObject::handle_event for fl with objclass " << int((*fl)->get_objclass()) << " and objtype " << int((*fl)->get_objtype()) << " returning float element result";
      return result; 
      ////trying NOT returning early
      // THIS DOES NOT WORK... not exactly sure why
      //Log::Debug["gui.cc"] << "inside GuiObject::handle_event for fl with objclass " << int((*fl)->get_objclass()) << " and objtype " << int((*fl)->get_objtype()) << " NOT RETURNING EARLY";
    }else{
      // check next float
      Log::Debug["gui.cc"] << "inside GuiObject::handle_event for fl with objclass " << int((*fl)->get_objclass()) << " and objtype " << int((*fl)->get_objtype()) << " returned false, continuing";
    }
  }

  if (skip_event){
    Log::Debug["gui.cc"] << "inside GuiObject::handle_event, GuiObjectClass " << NameGuiObjClass[objclass] << ", skip_event is true, returning false";
    return false;
  }

  Log::Debug["event_loop.cc"] << "inside GuiObject::handle_event with event type " << NameGuiObjEvent[event->type] << " and objclass " << NameGuiObjClass[objclass] << " and objtype " << get_objtype() << ", checking event type w switch/case";
  bool result = false;
  switch (event->type) {
    case Event::TypeLeftClick:
      result = handle_left_click(event_x, event_y, event->dy);
      break;
    case Event::TypeMouseButtonDown:
      Log::Debug["gui.cc"] << "inside GuiObject::handle_event with event type " << NameGuiObjEvent[event->type] << " and objclass " << NameGuiObjClass[objclass] << " and objtype " << get_objtype() << " calling 'result = handle_mouse_button_down' with event->dx " << event->dx << " and event->dy " << event->dy;
      result = handle_mouse_button_down(event->dx, event->dy, event->button);
      Log::Debug["gui.cc"] << "inside GuiObject::handle_event with event type " << NameGuiObjEvent[event->type] << " and objclass " << NameGuiObjClass[objclass] << " and objtype " << get_objtype() << " called 'result = handle_mouse_button_down', result was " << result;
      break;
    case Event::TypeDrag:
      Log::Debug["gui.cc"] << "inside GuiObject::handle_event with event type " << NameGuiObjEvent[event->type] << " and objclass " << NameGuiObjClass[objclass] << " and objtype " << get_objtype() << " calling 'result = handle_drag' with event->dx " << event->dx << " and event->dy " << event->dy;
      result = handle_drag(event->dx, event->dy);
      Log::Debug["gui.cc"] << "inside GuiObject::handle_event with event type " << NameGuiObjEvent[event->type] << " and objclass " << NameGuiObjClass[objclass] << " and objtype " << get_objtype() << " called 'result = handle_drag', result was " << result;
      break;
    case Event::TypeRightClick:
      result = handle_click_right(event_x, event_y);
      break;
    case Event::TypeDoubleClick:
      result = handle_dbl_click(event_x, event_y, event->button);
      break;
    case Event::TypeMiddleClick:
      result = handle_special_click(event_x, event_y);
      break;
    case Event::TypeSpecialClick:
      result = handle_special_click(event_x, event_y);
      break;
    case Event::TypeKeyPressed:
      result = handle_key_pressed(event->dx, event->dy);
      break;
    case Event::TypeNumPadKeyPressed:
      Log::Debug["gui.cc"] << "inside GuiObject::handle_event type TypeNumPadKeyPressed, event->dx is " << event->dx; 
      result = handle_numpad_key_pressed(event->dx);
      break;
    case Event::TypeArrowKeyPressed:
      result = handle_arrow_key_pressed(event->dx);
      break;
    case Event::TypeListScroll:
      result = handle_list_scroll(event->dx);
      break;
    default:
      Log::Debug["gui.cc"] << "inside GuiObject::handle_event with event type #" << event->type << " and objclass " << NameGuiObjClass[objclass] << ", case default, no matching event type";
      break;
  }

  Log::Debug["gui.cc"] << "inside GuiObject::handle_event with event type " << NameGuiObjEvent[event->type] << " and objclass " << NameGuiObjClass[objclass] << ", result was " << result << ", running refocus logic";
  if (result && (focused_object != this)) {
    Log::Debug["gui.cc"] << "inside GuiObject::handle_event(), focus, triggering focus_loose, result was " << result;
    if (focused_object != nullptr) {
      focused_object->focused = false;
      focused_object->handle_focus_loose();
      focused_object->set_redraw();
      focused_object = nullptr;
    }
  }

  Log::Debug["gui.cc"] << "inside GuiObject::handle_event with event type " << NameGuiObjEvent[event->type] << " and objclass " << NameGuiObjClass[objclass] << ", returning result " << result;
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
GuiObject::move_to(int px, int py) {
  Log::Debug["gui.cc"] << "inside GuiObject::move_to()";
  x = px;
  y = py;
  set_redraw();
}

void
GuiObject::get_position(int *px, int *py) {
  if (px != nullptr) {
    *px = x;
  }
  if (py != nullptr) {
    *py = y;
  }
}

void
GuiObject::set_size(int new_width, int new_height) {
  Log::Debug["gui.cc"] << "start of GuiObject::set_size";
  delete_frame();
  width = new_width;
  height = new_height;
  layout();  // this appears to do nothing for generic GuiObject, but I think it exists because it is overridden by some GuiObject superclasses such as Viewport, Interface, and their layout() is important
  //store_prev_res();
  set_redraw();
}

void
GuiObject::get_size(int *pwidth, int *pheight) {
  if (pwidth != nullptr) {
    *pwidth = width;
  }
  if (pheight != nullptr) {
    *pheight = height;
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
  if (parent != nullptr) {
    parent->set_redraw();
  }
}

bool
GuiObject::point_inside(int point_x, int point_y) {
  return (point_x >= x && point_y >= y &&
          point_x < x + width && point_y < y + height);
}

void
GuiObject::add_float(GuiObject *obj, int fx, int fy) {
  Log::Debug["gui.cc"] << "inside GuiObject::add_float() with type " << NameGuiObjClass[obj->get_objclass()] << " and type " << obj->get_objtype();
  obj->set_parent(this);
  // it seems this parent concept is used only multi-part single popup windows with text files/file lists to allow both to be refreshed
  //  such as save/load game boxes and random seed number box
  // it has nothing to do with one popup opening another popup!  such as opening Options or Save window from the "computer" panel icon popup
  floats.push_back(obj);
  obj->move_to(fx, fy);
  set_redraw();
}

void
GuiObject::del_float(GuiObject *obj) {
  obj->set_parent(nullptr);
  floats.remove(obj);
  set_redraw();
}

void
GuiObject::play_sound(int sound) {
  Audio &audio = Audio::get_instance();
  Audio::PPlayer player = audio.get_sound_player();
  if (player) {
    Data &data = Data::get_instance();
    if (data.get_data_source_DOS() != nullptr){
      Audio::PTrack t = player->play_track(sound, DataSourceType::DOS);  // default to DOS
    }else{
      // if no DOS resources available, try Amiga sound
      Audio::PTrack t = player->play_track(sound, DataSourceType::Amiga);  // fall back to Amiga
    }
  }
}

void
//GuiObject::play_sound(int sound) {
GuiObject::play_sound(int sound, int source_type) {
  Audio &audio = Audio::get_instance();
  Audio::PPlayer player = audio.get_sound_player();

  if (player) {
    Data &data = Data::get_instance();
    if (data.get_data_source_DOS() == nullptr){
      // if no DOS resources available, force Amiga sounds only (which hopefully exist, or game likely won't start... unless using Custom stuff which isn't supported yet
      source_type = DataSourceType::Amiga;
    }
    if (data.get_data_source_Amiga() == nullptr){
      // if no Amiga resources available, force DOS sounds only (which hopefully exist, or game likely won't start... unless using Custom stuff which isn't supported yet
      source_type = DataSourceType::DOS;
    }
    Audio::PTrack t = player->play_track(sound, source_type);
  }

}
