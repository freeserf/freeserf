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
  // allow junk?  need to create defaults
  // just set the defaults here for now, need to make these initialized and passed or otherwise work without specifying
  CustomMapGeneratorOptions custom_map_generator_options;
  for (int x = 0; x < 23; x++){
    custom_map_generator_options.opt[x] = 1.00;
  }
  custom_map_generator_options.opt[CustomMapGeneratorOption::MountainGold] = 2.00;
  custom_map_generator_options.opt[CustomMapGeneratorOption::MountainIron] = 4.00;
  custom_map_generator_options.opt[CustomMapGeneratorOption::MountainCoal] = 9.00;
  custom_map_generator_options.opt[CustomMapGeneratorOption::MountainStone] = 2.00;

  PGameInfo game_info(new GameInfo(Random()));
  //return start_game(game_info);
  return start_game(game_info, custom_map_generator_options);
}

bool
GameManager::start_game(PGameInfo game_info, CustomMapGeneratorOptions custom_map_generator_options) {
  //PGame new_game = game_info->instantiate();
  PGame new_game = game_info->instantiate(custom_map_generator_options);
  if (!new_game) {
    return false;
  }

  set_current_game(new_game);
  //Log::Debug["game-manager.cc"] << "inside GameManager::start_game, this game map is " << new_game->get_map()->get_cols() << " cols wide x " << new_game->get_map()->get_rows() << " rows high";

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
