/*
 * game-source-local.h - Saved games source
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

#ifndef SRC_GAME_SOURCE_LOCAL_H_
#define SRC_GAME_SOURCE_LOCAL_H_

#include <vector>
#include <string>
#include <memory>

#include "src/game-manager.h"
#include "src/random.h"

class PlayerInfoLocal : public PlayerInfoBase {
 public:
  PlayerInfoLocal();

  virtual bool has_castle() const;
};

typedef std::shared_ptr<PlayerInfoLocal> PPlayerInfoLocal;

class GameInfoLocal : public GameInfo {
 public:
  typedef enum Type {
    Legacy,
    Regular
  } Type;

 protected:
  size_t map_size;
  Random random_base;
  std::vector<PPlayerInfoLocal> players;
  std::string name;
  std::string path;

 public:
  explicit GameInfoLocal(const std::string &path);
  GameInfoLocal(const std::string &path, const std::string &name, Type type);

  virtual std::string get_name() const { return name; }
  virtual std::string get_path() const { return path; }
  virtual size_t get_map_size() const { return map_size; }
  virtual Random get_random_base() const { return random_base; }
  virtual size_t get_player_count() const { return players.size(); }
  virtual PPlayerInfo get_player(size_t player) const {
    return players[player];
  }
};

typedef std::shared_ptr<GameInfoLocal> PGameInfoLocal;

class GameSourceLocal : public GameSource {
 protected:
  std::vector<PGameInfoLocal> games;
  std::string folder_path;

 public:
  GameSourceLocal();
  virtual ~GameSourceLocal() {}

  virtual std::string get_name() const { return "local"; }
  virtual size_t count() const { return games.size(); }
  virtual PGameInfo game_info_at(const size_t &pos) const;
  virtual PGameInfo create_game(const std::string &path) const;
  virtual bool publicate(PGame game);

 protected:
  bool update();
  void find_legacy();
  void find_files(GameInfoLocal::Type type);
  bool create_folder(const std::string &path) const;
  bool is_folder_exists(const std::string &path) const;
  std::string name_from_file(const std::string &file_name) const;
};

#endif  // SRC_GAME_SOURCE_LOCAL_H_
