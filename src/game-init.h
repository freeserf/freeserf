/*
 * game-init.h - Game initialization GUI component header
 *
 * Copyright (C) 2013  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef SRC_GAME_INIT_H_
#define SRC_GAME_INIT_H_

#include <string>

#include "src/gui.h"
#include "src/misc.h"
BEGIN_EXT_C
  #include "src/game.h"
END_EXT_C

class interface_t;

class game_init_box_t : public gui_object_t {
 protected:
  interface_t *interface;

  int map_size;
  int game_mission;

  uint face[GAME_MAX_PLAYER_COUNT];
  uint intelligence[GAME_MAX_PLAYER_COUNT];
  uint supplies[GAME_MAX_PLAYER_COUNT];
  uint reproduction[GAME_MAX_PLAYER_COUNT];

 public:
  explicit game_init_box_t(interface_t *interface);

 protected:
  void draw_box_icon(int x, int y, int sprite);
  void draw_box_string(int x, int y, const std::string &str);
  int get_player_face_sprite(int face);
  void handle_action(int action);

  virtual void internal_draw();
  virtual bool handle_click_left(int x, int y);
};

#endif  // SRC_GAME_INIT_H_
