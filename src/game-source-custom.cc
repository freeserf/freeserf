/*
 * game-source-custom.cc - Predefined game mission maps
 *
 * Copyright (C) 2019  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/game-source-custom.h"
#include "src/player.h"
#include "src/characters.h"

static PlayerInfo::Color def_color[4] = {
  {0x00, 0xe3, 0xe3},
  {0xcf, 0x63, 0x63},
  {0xdf, 0x7f, 0xef},
  {0xef, 0xef, 0x8f}
};

GameInfoCustom::GameInfoCustom(const Random &_random_base) {
  map_size = 3;
  name = _random_base;
  set_random_base(_random_base);
}

void
GameInfoCustom::set_random_base(const Random &base) {
  Random random = base;
  random_base = base;

  players.clear();

  // Player 0
  players.push_back(std::make_shared<PlayerInfoCustom>(&random));
  players[0]->set_character(12);
  players[0]->set_intelligence(40);

  // Player 1
  players.push_back(std::make_shared<PlayerInfoCustom>(&random));

  uint32_t val = random.random();
  if ((val & 7) != 0) {
    // Player 2
    players.push_back(std::make_shared<PlayerInfoCustom>(&random));
    uint32_t val = random.random();
    if ((val & 3) == 0) {
      // Player 3
      players.push_back(std::make_shared<PlayerInfoCustom>(&random));
    }
  }

  int i = 0;
  for (PPlayerInfoCustom info : players) {
    info->set_color(def_color[i++]);
  }
}

void
GameInfoCustom::add_player(const PPlayerInfoCustom &player) {
  players.push_back(player);
}

void
GameInfoCustom::remove_player(size_t player) {
  if (player >= players.size()) {
    return;
  }

  players.erase(players.begin() + 1);
}

void
GameInfoCustom::remove_all_players() {
  players.clear();
}

PlayerInfoCustom::PlayerInfoCustom() {
  Random random_base;
  size_t character = (((random_base.random() * 10) >> 16) + 1) & 0xFF;
  set_character(character);
  set_intelligence(((random_base.random() * 41) >> 16) & 0xFF);
  set_supplies(((random_base.random() * 41) >> 16) & 0xFF);
  set_reproduction(((random_base.random() * 41) >> 16) & 0xFF);
  set_castle_pos({-1, -1});
}

PlayerInfoCustom::PlayerInfoCustom(const PlayerInfo &other) {
  intelligence = other.get_intelligence();
  supplies = other.get_supplies();
  reproduction = other.get_reproduction();
  face = other.get_face();
  color = other.get_color();
  name = other.get_name();
  characterization = other.get_characterization();
  castle_pos = other.get_castle_pos();
}

PlayerInfoCustom::PlayerInfoCustom(Random *random_base) {
  size_t character = (((random_base->random() * 10) >> 16) + 1) & 0xFF;
  set_character(character);
  set_intelligence(((random_base->random() * 41) >> 16) & 0xFF);
  set_supplies(((random_base->random() * 41) >> 16) & 0xFF);
  set_reproduction(((random_base->random() * 41) >> 16) & 0xFF);
  set_castle_pos({-1, -1});
}

PlayerInfoCustom::PlayerInfoCustom(size_t character,
                                   const PlayerInfo::Color &_color,
                                   unsigned int _intelligence,
                                   unsigned int _supplies,
                                   unsigned int _reproduction) {
  set_character(character);
  set_intelligence(_intelligence);
  set_supplies(_supplies);
  set_reproduction(_reproduction);
  set_color(_color);
  set_castle_pos({-1, -1});
}

void
PlayerInfoCustom::set_castle_pos(PlayerInfo::BuildingPos _castle_pos) {
  castle_pos = _castle_pos;
}

void
PlayerInfoCustom::set_character(size_t index) {
  Character &character = Characters::characters.get_character(index);
  face = character.get_face();
  name = character.get_name();
  characterization = character.get_characterization();
}

bool
PlayerInfoCustom::has_castle() const {
  return (castle_pos.col >= 0);
}

// GameSourceCustom

GameSourceCustom::GameSourceCustom() {
}

PGameInfo
GameSourceCustom::game_info_at(const size_t &pos) const {
  if (pos >= count()) {
    return nullptr;
  }

  return game_info;
}

PGameInfo
GameSourceCustom::create_game(const std::string &path) const {
  return nullptr;
}
