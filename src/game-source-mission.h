/*
 * game-source-mission.h - Predefined game mission maps
 *
 * Copyright (C) 2013-2019  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef SRC_GAME_SOURCE_MISSION_H_
#define SRC_GAME_SOURCE_MISSION_H_

#include <vector>
#include <string>
#include <memory>

#include "src/game-manager.h"
#include "src/random.h"

class Character;

typedef struct PlayerPreset {
  size_t character_index;
  unsigned int intelligence;
  unsigned int supplies;
  unsigned int reproduction;
  PlayerInfo::BuildingPos castle;
} PlayerPreset;

typedef struct MissionPreset {
  const std::string name;
  Random rnd;
  std::vector<PlayerPreset> player;
} MissionPreset;

class PlayerInfoMission : public PlayerInfo {
 protected:
  const PlayerPreset *player_preset;
  size_t id;

 public:
  explicit PlayerInfoMission(const PlayerPreset *player_preset, size_t id);

  virtual std::string get_name() const;
  virtual std::string get_characterization() const;
  virtual unsigned int get_face() const;
  virtual PlayerInfo::Color get_color() const;

  virtual unsigned int get_intelligence() const;
  virtual unsigned int get_supplies() const;
  virtual unsigned int get_reproduction() const;
  virtual PlayerInfo::BuildingPos get_castle_pos() const;
  virtual bool has_castle() const;
};

class GameInfoMission : public GameInfo {
 protected:
  const MissionPreset *mission_preset;

 public:
  explicit GameInfoMission(const MissionPreset *mission_preset);

  virtual std::string get_name() const;
  virtual std::string get_path() const { return std::string(); }
  virtual size_t get_map_size() const { return 3; }
  virtual Random get_random_base() const;
  virtual size_t get_player_count() const;
  virtual PPlayerInfo get_player(size_t player) const;
};

class GameSourceMission : public GameSource {
 public:
  GameSourceMission();
  virtual ~GameSourceMission() {}

  virtual std::string get_name() const { return "mission"; }
  virtual size_t count() const;
  virtual PGameInfo game_info_at(const size_t &pos) const;
  virtual PGameInfo create_game(const std::string &path) const;
  virtual bool publicate(PGame game) { return false; }
};

#endif  // SRC_GAME_SOURCE_MISSION_H_
