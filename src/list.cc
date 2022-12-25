/*
 * list.cc - String list GUI component
 *
 * Copyright (C) 2017-2018  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/list.h"

ListSavedFiles::ListSavedFiles()
  : GuiObject()
  , save_game(&GameStore::get_instance()) {
  color_focus = Color(0x00, 0x8b, 0x47);
  color_text = Color::green;
  color_background = Color::black;

  items = save_game->get_saved_games();
  first_visible_item = 0;
  selected_item = -1;
}

std::string
ListSavedFiles::get_selected() const {
  if (items.size() == 0) {
    return std::string();
  }
  std::string file_path = items[selected_item].path;
  return file_path;
}

void
ListSavedFiles::internal_draw() {
  frame->fill_rect(0, 0, width, height, color_background);

  if (selected_item != -1){
    Log::Debug["list.cc"] << "inside ListSavedFiles::internal_draw(), foo0";
    set_focused();
  }

  if (focused) {
    frame->draw_rect(0, 0, width, height, color_focus);
  }

  unsigned int last_visible_item = first_visible_item + 16;
  if (selected_item > -1){
    if (selected_item > last_visible_item){
      Log::Debug["list.cc"] << "inside ListSavedFiles::internal_draw(), fooA";
      first_visible_item = selected_item - 15;
      last_visible_item = selected_item;
    }else if (selected_item < first_visible_item){
      Log::Debug["list.cc"] << "inside ListSavedFiles::internal_draw(), fooB";
      first_visible_item = selected_item;
      last_visible_item = selected_item + 16;
    }
  }
  unsigned int item = first_visible_item;
  for (int ly = 0; (ly < (height - 6)) && (item < items.size()); ly += 9) {
    Color tc = color_text;
    if (static_cast<int>(item) == selected_item) {
      frame->fill_rect(2, ly + 2, width - 4, 9, Color::green);
      tc = Color::black;
    }
    frame->draw_string(3, 3 + ly, items[item].name, tc);
    item++;
  }
}

bool
ListSavedFiles::handle_left_click(int /*cx*/, int cy, int modifier) {
  set_focused();
  cy -= 3;
  if (cy >= 0) {
    cy = first_visible_item + (cy / 9);
    if ((selected_item != cy) && (cy >= 0) &&
        (cy < static_cast<int>(items.size()))) {
      selected_item = cy;
      set_redraw();
      if (selection_handler != nullptr) {
        std::string file_path = items[selected_item].path;
        selection_handler(file_path);
      }
    }
  }
  return true;
}

bool
ListSavedFiles::handle_drag(int dx, int dy) {
  if (!focused) {
    return false;
  }

  int nfvi = static_cast<int>(first_visible_item) + (dy / 32);
  if (nfvi < 0) {
    nfvi = 0;
  }
  if (((static_cast<int>(items.size()) - nfvi) * 9) <= height-8) {
    return true;
  }
  if (static_cast<int>(first_visible_item) != nfvi) {
    first_visible_item = nfvi;
    set_redraw();
  }
  return true;
}

bool
ListSavedFiles::handle_key_pressed(char key, int modifier) {
  return true;
}

//0=up,1=down,2=left,3=right
bool
ListSavedFiles::handle_arrow_key_pressed(uint8_t key) {
  //Log::Debug["list.cc"] << "inside ListSavedFiles::handle_arrow_key_pressed, key=" << key;
  set_focused();
  if (key == 0){
    if (selected_item < 1){
      selected_item = 0;
    }else{
      selected_item--;
    }
  }else if (key == 1 && selected_item < items.size()){
    selected_item++;
  }
  set_redraw();
  return true;
}

// this is only int return type to avoid conflict with Minimap::handle_scroll
int
ListSavedFiles::handle_scroll(int y) {
  //Log::Debug["list.cc"] << "inside ListSavedFiles::handle_scroll, y=" << y << " selected item was " << selected_item;
  // scroll 4 lines at time
  selected_item = selected_item + y*4 * -1; // the wheel y values seem inversed
  if (selected_item < 1){
    selected_item = 0;
  } else if (selected_item > items.size()){
    selected_item = items.size();
  }
  //Log::Debug["list.cc"] << "inside ListSavedFiles::handle_scroll, y=" << y << " selected item is now " << selected_item;
  set_redraw();
  return true;
}


bool
ListSavedFiles::handle_focus_loose() {
  //Log::Debug["list.cc"] << "inside ListSavedFiles::handle_focus_loose";
  focused = false;
  is_list_in_focus = false;
  set_redraw();
  return true;
}
