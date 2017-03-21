/*
 * mission.h - Predefined game mission maps
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

#ifndef SRC_MISSION_H_
#define SRC_MISSION_H_

#include <vector>
#include <string>
#include <memory>

#include "src/random.h"

typedef struct {
  unsigned char red;
  unsigned char green;
  unsigned char blue;
} PlayerColor;

typedef struct {
  unsigned int face;
  const char *name;
  const char *characterization;
} CharacterPreset;

typedef struct {
  int col;
  int row;
} PosPreset;

typedef struct {
  CharacterPreset *character;
  unsigned int intelligence;
  unsigned int supplies;
  unsigned int reproduction;
  PosPreset castle;
} PlayerPreset;

typedef struct {
  const char *name;
  Random rnd;
  PlayerPreset player[4];
} MissionPreset;

class PlayerInfo {
 protected:
  unsigned int intelligence;
  unsigned int supplies;
  unsigned int reproduction;
  unsigned int face;
  PlayerColor color;
  std::string name;
  std::string characterization;
  PosPreset castle_pos;

 public:
  explicit PlayerInfo(Random *random_base);
  PlayerInfo(size_t character, const PlayerColor &_color,
             unsigned int _intelligence, unsigned int _supplies,
             unsigned int _reproduction);

  void set_intelligence(unsigned int _intelligence) {
    intelligence = _intelligence; }
  void set_supplies(unsigned int _supplies) { supplies = _supplies; }
  void set_reproduction(unsigned int _reproduction) {
    reproduction = _reproduction; }
  void set_castle_pos(PosPreset _castle_pos);
  void set_character(size_t character);
  void set_color(const PlayerColor &_color) { color = _color; }

  unsigned int get_intelligence() const { return intelligence; }
  unsigned int get_supplies() const { return supplies; }
  unsigned int get_reproduction() const { return reproduction; }
  unsigned int get_face() const { return face; }
  PlayerColor get_color() const { return color; }
  PosPreset get_castle_pos() const { return castle_pos; }

  bool has_castle() const;
};

typedef std::shared_ptr<PlayerInfo> PPlayerInfo;
typedef std::vector<PPlayerInfo> PlayerInfos;

class GameInfo {
 protected:
  unsigned int map_size;
  Random random_base;
  PlayerInfos players;
  std::string name;

  static MissionPreset mission[];

  explicit GameInfo(const MissionPreset *mission_preset);

 public:
  explicit GameInfo(const Random &random_base);

  unsigned int get_map_size() const { return map_size; }
  void set_map_size(unsigned int size) { map_size = size; }
  Random get_random_base() const { return random_base; }
  void set_random_base(const Random &base);
  size_t get_player_count() const { return players.size(); }
  PPlayerInfo get_player(size_t player) const { return players[player]; }

  void add_player(const PPlayerInfo &player);
  void add_player(size_t character, const PlayerColor &_color,
                  unsigned int _intelligence, unsigned int _supplies,
                  unsigned int _reproduction);
  void remove_all_players();

  static std::shared_ptr<GameInfo> get_mission(size_t mission);
  static size_t get_mission_count();

  static CharacterPreset *get_character(size_t character);
  static size_t get_character_count();
};

typedef std::shared_ptr<GameInfo> PGameInfo;

#endif  // SRC_MISSION_H_
