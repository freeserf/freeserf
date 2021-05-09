/*
 * list.h - String list GUI component
 *
 * Copyright (C) 2017  Wicked_Digger <wicked_digger@mail.ru>
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

#ifndef SRC_LIST_H_
#define SRC_LIST_H_

#include <string>
#include <vector>
#include <functional>

#include "src/gui.h"
#include "src/game-manager.h"

class ListGames : public GuiObject {
 protected:
  Color color_focus;
  Color color_text;
  Color color_background;

  PGameSource game_source;
  unsigned int first_visible_item;
  int selected_item;
  std::function<void(PGameInfo)> selection_handler;

 public:
  explicit ListGames(PGameSource _game_source);
  virtual ~ListGames();

  void set_selection_handler(std::function<void(PGameInfo)> handler) {
    selection_handler = handler;
  }
  PGameInfo get_selected() const;

 protected:
  virtual void internal_draw();

  virtual bool handle_click_left(int x, int y);
  virtual bool handle_drag(int dx, int dy);
  virtual bool handle_key_pressed(char key, int modifier);
  virtual bool handle_focus_loose();
};

#endif  // SRC_LIST_H_
