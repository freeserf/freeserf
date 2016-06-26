/*
 * game-init.h - Game initialization GUI component header
 *
 * Copyright (C) 2013-2015  Jon Lund Steffensen <jonlst@gmail.com>
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
#include "src/game.h"
#include "src/mission.h"

class interface_t;
class random_input_t;
class minimap_t;

class game_init_box_t : public gui_object_t {

public:
  enum game_types {
    GAMETYPE_FREE = 0,
    GAMETYPE_MISSION = 1,
    GAMETYPE_TUTORIAL = 2
  };

protected:
  interface_t *interface;

  int map_size;
  int game_mission = 0;
  game_types game_type = GAMETYPE_FREE;

  mission_t custom_mission;
  mission_t *mission;

  random_input_t *field;
  map_t *map;
  minimap_t *minimap;

 public:
  explicit game_init_box_t(interface_t *interface);
  virtual ~game_init_box_t();

 protected:
  void draw_box_icon(int x, int y, int sprite);
  void draw_box_string(int x, int y, const std::string &str);
  void draw_player_box(int player, int x, int y);
  void draw_backgraund();
  unsigned int get_player_face_sprite(size_t face);
  void handle_action(int action);
  bool handle_player_click(int player, int x, int y);
  void apply_random(random_state_t rnd);
  void generate_map_priview();

  virtual void internal_draw();
  virtual bool handle_click_left(int x, int y);
};

#endif  // SRC_GAME_INIT_H_
