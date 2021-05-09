/*
 * game-manager.h - Gameplay related functions
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

#ifndef SRC_GAME_MANAGER_H_
#define SRC_GAME_MANAGER_H_

#include <memory>
#include <list>
#include <string>

class Random;

class PlayerInfo {
 public:
  typedef struct Color {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
  } Color;

  typedef struct BuildingPos {
    int col;
    int row;
  } BuildingPos;

 public:
  virtual ~PlayerInfo() {}

  virtual std::string get_name() const = 0;
  virtual std::string get_characterization() const = 0;
  virtual unsigned int get_face() const = 0;
  virtual Color get_color() const = 0;

  virtual unsigned int get_intelligence() const = 0;
  virtual unsigned int get_supplies() const = 0;
  virtual unsigned int get_reproduction() const = 0;
  virtual BuildingPos get_castle_pos() const = 0;
  virtual bool has_castle() const = 0;
};

typedef std::shared_ptr<PlayerInfo> PPlayerInfo;

class PlayerInfoBase : public PlayerInfo {
 protected:
  unsigned int intelligence;
  unsigned int supplies;
  unsigned int reproduction;
  unsigned int face;
  PlayerInfo::Color color;
  std::string name;
  std::string characterization;
  PlayerInfo::BuildingPos castle_pos;

 public:
  PlayerInfoBase()
    : intelligence(0)
    , supplies(0)
    , reproduction(0)
    , face(0)
    , color({0, 0, 0}) {}
  virtual ~PlayerInfoBase() {}

  virtual std::string get_name() const { return name; }
  virtual std::string get_characterization() const { return characterization; }
  virtual unsigned int get_face() const { return face; }
  virtual PlayerInfo::Color get_color() const { return color; }

  virtual unsigned int get_intelligence() const  { return intelligence; }
  virtual unsigned int get_supplies() const  { return supplies; }
  virtual unsigned int get_reproduction() const  { return reproduction; }
  virtual PlayerInfo::BuildingPos get_castle_pos() const  { return {0, 0}; }
  virtual bool has_castle() const  { return false; }
};

class GameInfo {
 public:
  virtual ~GameInfo() {}

  virtual std::string get_name() const = 0;
  virtual std::string get_path() const = 0;
  virtual size_t get_map_size() const = 0;
  virtual Random get_random_base() const = 0;
  virtual size_t get_player_count() const = 0;
  virtual PPlayerInfo get_player(size_t player) const = 0;
};

typedef std::shared_ptr<GameInfo> PGameInfo;

class Game;
typedef std::shared_ptr<Game> PGame;

class GameSource {
 public:
  virtual ~GameSource() {}

  virtual std::string get_name() const = 0;
  virtual size_t count() const = 0;
  virtual PGameInfo game_info_at(const size_t &pos) const = 0;
  virtual PGameInfo create_game(const std::string &path) const = 0;
  virtual bool publicate(PGame game) = 0;
};

typedef std::shared_ptr<GameSource> PGameSource;

class GameManager {
 public:
  class Handler {
   public:
    virtual void on_new_game(PGame game, PGameInfo game_info) = 0;
    virtual void on_end_game(PGame game) = 0;
  };

 protected:
  PGame current_game;
  PGameInfo current_game_info;
  typedef std::list<Handler*> Handlers;
  Handlers handlers;

  std::list<PGameSource> game_sources;

  GameManager();

 public:
  virtual ~GameManager();

  static GameManager &get_instance();

  void add_handler(Handler *handler);
  void del_handler(Handler *handler);

  bool start_game(PGameInfo game_info);
  PGame get_current_game() { return current_game; }
  PGameInfo get_current_game_info() { return current_game_info; }

  std::list<PGameSource> get_game_sources() const { return game_sources; }
  PGameSource get_game_source(const std::string &name);
  bool publicate_current_game(const std::string &source_name,
                              const std::string &name = std::string());

 protected:
  void set_current_game(PGame new_game, PGameInfo new_game_info);
};

#endif  // SRC_GAME_MANAGER_H_
