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

ListGames::ListGames(PGameSource _game_source)
  : GuiObject()
  , game_source(_game_source) {
  color_focus = Color(0x00, 0x8b, 0x47);
  color_text = Color::green;
  color_background = Color::black;

  first_visible_item = 0;
  selected_item = -1;
}

ListGames::~ListGames() {
}

PGameInfo
ListGames::get_selected() const {
  if (!game_source || (game_source->count() == 0)) {
    return nullptr;
  }

  PGameInfo game_info = game_source->game_info_at(selected_item);
  return game_info;
}

void
ListGames::internal_draw() {
  frame->fill_rect(0, 0, width, height, color_background);

  if (focused) {
    frame->draw_rect(0, 0, width, height, color_focus);
  }

  if (!game_source) {
    return;
  }

  unsigned int item = first_visible_item;
  for (int ly = 0; (ly < (height - 6)) && (item < game_source->count());
       ly += 9) {
    Color tc = color_text;
    if (static_cast<int>(item) == selected_item) {
      frame->fill_rect(2, ly + 2, width - 4, 9, Color::green);
      tc = Color::black;
    }
    std::string name;
    PGameInfo game_info = game_source->game_info_at(item);
    if (game_info) {
      name = game_info->get_name();
    }
    frame->draw_string(3, 3 + ly, name, tc);
    item++;
  }
}

bool
ListGames::handle_click_left(int /*cx*/, int cy) {
  set_focused();
  cy -= 3;
  if (cy >= 0) {
    cy = first_visible_item + (cy / 9);
    if ((selected_item != cy) && (cy >= 0) &&
        (cy < static_cast<int>(game_source->count()))) {
      selected_item = cy;
      set_redraw();
      if (selection_handler != nullptr) {
        selection_handler(game_source->game_info_at(selected_item));
      }
    }
  }
  return true;
}

bool
ListGames::handle_drag(int dx, int dy) {
  if (!focused) {
    return false;
  }

  int nfvi = static_cast<int>(first_visible_item) + (dy / 32);
  if (nfvi < 0) {
    nfvi = 0;
  }
  if (((static_cast<int>(game_source->count()) - nfvi) * 9) <= height-8) {
    return true;
  }
  if (static_cast<int>(first_visible_item) != nfvi) {
    first_visible_item = nfvi;
    set_redraw();
  }
  return true;
}

bool
ListGames::handle_key_pressed(char key, int modifier) {
  return true;
}

bool
ListGames::handle_focus_loose() {
  focused = false;
  set_redraw();
  return true;
}
