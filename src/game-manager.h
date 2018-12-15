/*
 * game-manager.h - Gameplay related functions
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

#ifndef SRC_GAME_MANAGER_H_
#define SRC_GAME_MANAGER_H_

#include <memory>
#include <list>
#include <string>

#include "src/mission.h"
#include "src/game.h"

class GameManager {
 public:
  class Handler {
   public:
    virtual void on_new_game(PGame game) = 0;
    virtual void on_end_game(PGame game) = 0;
  };

 protected:
  PGame current_game;
  typedef std::list<Handler*> Handlers;
  Handlers handlers;

  GameManager();

 public:
  virtual ~GameManager();

  static GameManager &get_instance();

  void add_handler(Handler *handler);
  void del_handler(Handler *handler);

  PGame get_current_game() { return current_game; }

  bool start_random_game();
  bool start_game(PGameInfo game_info);
  bool load_game(const std::string &path);

 protected:
  void set_current_game(PGame new_game);
};

#endif  // SRC_GAME_MANAGER_H_
