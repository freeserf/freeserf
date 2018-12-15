/*
 * game-manager.cc - Gameplay related functions
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

#include "src/game-manager.h"

#include <string>
#include <utility>

#include "src/savegame.h"

GameManager &
GameManager::get_instance() {
  static GameManager game_manager;
  return game_manager;
}

GameManager::GameManager() {
}

GameManager::~GameManager() {
  set_current_game(nullptr);
}

void
GameManager::add_handler(Handler *handler) {
  handlers.push_back(handler);
}

void
GameManager::del_handler(Handler *handler) {
  handlers.remove(handler);
}

void
GameManager::set_current_game(PGame new_game) {
  if (current_game) {
    for (Handler *handler : handlers) {
      handler->on_end_game(current_game);
    }
  }

  current_game = std::move(new_game);
  if (!current_game) {
    return;
  }

  for (Handler *handler : handlers) {
    handler->on_new_game(current_game);
  }
}

bool
GameManager::start_random_game() {
  PGameInfo game_info(new GameInfo(Random()));
  return start_game(game_info);
}

bool
GameManager::start_game(PGameInfo game_info) {
  PGame new_game = game_info->instantiate();
  if (!new_game) {
    return false;
  }

  set_current_game(new_game);

  return true;
}

bool
GameManager::load_game(const std::string &path) {
  PGame new_game = std::make_shared<Game>();
  if (!GameStore::get_instance().load(path, new_game.get())) {
    return false;
  }

  set_current_game(new_game);
  new_game->pause();

  return true;
}
