/*
 * game-manager.cc - Gameplay related functions
 *
 * Copyright (C) 2017-2019  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/game.h"
#include "src/savegame.h"
#include "src/game-source-local.h"
#include "src/game-source-mission.h"
#include "src/game-source-custom.h"

GameManager &
GameManager::get_instance() {
  static GameManager game_manager;
  return game_manager;
}

GameManager::GameManager() {
  game_sources.push_back(std::make_shared<GameSourceLocal>());
  game_sources.push_back(std::make_shared<GameSourceMission>());
  game_sources.push_back(std::make_shared<GameSourceCustom>());
}

GameManager::~GameManager() {
  set_current_game(nullptr, nullptr);
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
GameManager::set_current_game(PGame new_game, PGameInfo new_game_info) {
  if (current_game) {
    for (Handler *handler : handlers) {
      handler->on_end_game(current_game);
    }
  }

  current_game = std::move(new_game);
  current_game_info = std::move(new_game_info);
  if (!current_game) {
    return;
  }

  for (Handler *handler : handlers) {
    handler->on_new_game(current_game, current_game_info);
  }
}

bool
GameManager::start_game(PGameInfo game_info) {
  if (!game_info) {
    return false;
  }

  PGame new_game = std::make_shared<Game>(game_info->get_map_size(),
                                          game_info->get_random_base());
  if (!new_game) {
    return false;
  }

  size_t players = game_info->get_player_count();
  for (size_t i = 0; i < players; ++i) {
    PPlayerInfo player = game_info->get_player(i);
    unsigned int index = new_game->add_player(player->get_intelligence(),
                                              player->get_supplies(),
                                              player->get_reproduction());
    if (player->has_castle()) {
      Player *p = new_game->get_player(index);
      PlayerInfo::BuildingPos pos = player->get_castle_pos();
      PMap map = new_game->get_map();
      new_game->build_castle(map->pos(pos.col, pos.row), p);
    }
  }

  set_current_game(new_game, game_info);

  return true;
}

PGameSource
GameManager::get_game_source(const std::string &name) {
  for (PGameSource source : game_sources) {
    if (source->get_name() == name) {
      return source;
    }
  }

  return nullptr;
}

bool
GameManager::publicate_current_game(const std::string &source_name,
                                    const std::string &name) {
  PGameSource source = get_game_source(source_name);
  if (source) {
    return source->publicate(get_current_game());
  }
  return false;
}
