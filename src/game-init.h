/*
 * game-init.h - Game initialization GUI component header
 *
 * Copyright (C) 2013-2016  Jon Lund Steffensen <jonlst@gmail.com>
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
#include <memory>

#include "src/gui.h"
#include "src/game.h"
#include "src/game-manager.h"
#include "src/game-source-custom.h"

class Interface;
class RandomInput;
class Minimap;
class ListGames;

class GameInitBox : public GuiObject {
 protected:
  typedef enum Action {
    ActionStartGame,
    ActionToggleGameType,
    ActionShowOptions,
    ActionIncrement,
    ActionDecrement,
    ActionClose,
    ActionGenRandom,
    ActionApplyRandom
  } Action;

  typedef enum GameType {
    GameCustom = 0,
    GameMission = 1,
    GameLoad = 2
  } GameType;

 protected:
  Interface *interface;

  unsigned int game_type;
  int game_mission;

  PGameInfoCustom custom_mission;
  PGameInfo mission;

  std::unique_ptr<RandomInput> random_input;
  PMap map;
  std::unique_ptr<Minimap> minimap;
  std::unique_ptr<ListGames> file_list;

 public:
  explicit GameInitBox(Interface *interface);
  virtual ~GameInitBox();

 protected:
  void draw_box_icon(int x, int y, int sprite);
  void draw_box_string(int x, int y, const std::string &str);
  void draw_player_box(unsigned int player, int x, int y);
  void draw_background();
  unsigned int get_player_face_sprite(size_t face);
  void handle_action(int action);
  bool handle_player_click(unsigned int player, int x, int y);
  unsigned int get_next_character(unsigned int player);
  void apply_random(Random rnd);
  void generate_map_preview();

  virtual void internal_draw();
  virtual bool handle_click_left(int x, int y);
};

#endif  // SRC_GAME_INIT_H_
