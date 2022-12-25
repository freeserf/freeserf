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
#include "src/savegame.h"

class ListSavedFiles : public GuiObject {
 protected:
  Color color_focus;
  Color color_text;
  Color color_background;

  GameStore *save_game;
  std::vector<GameStore::SaveInfo> items;
  unsigned int first_visible_item;
  int selected_item;
  std::function<void(const std::string&)> selection_handler;

 public:
  ListSavedFiles();

  void set_selection_handler(std::function<void(const std::string&)> handler) {
    selection_handler = handler;
  }
  std::string get_selected() const;
  std::string get_folder_path() const { return save_game->get_folder_path(); }

 protected:
  virtual void internal_draw();

  virtual bool handle_left_click(int x, int y, int modifier);
  virtual bool handle_drag(int dx, int dy);
  virtual bool handle_key_pressed(char key, int modifier);
  virtual bool handle_arrow_key_pressed(uint8_t key);
  virtual bool handle_list_scroll(int y);
  virtual bool handle_focus_loose();
};

#endif  // SRC_LIST_H_
