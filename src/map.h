/*
 * map.h - Map data and map update functions
 *
 * Copyright (C) 2013-2018  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef SRC_MAP_H_
#define SRC_MAP_H_

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "src/map-geometry.h"
#include "src/misc.h"
#include "src/random.h"

class Map;

class Road {
 public:
  typedef std::list<Direction> Dirs;

 protected:
  MapPos begin;
  Dirs dirs;

  static const size_t max_length = 256;

 public:
  Road() { begin = bad_map_pos; }

  bool is_valid() const { return (begin != bad_map_pos); }
  void invalidate() { begin = bad_map_pos; dirs.clear(); }
  void start(MapPos start) { begin = start; }
  MapPos get_source() const { return begin; }
  Dirs get_dirs() const { return dirs; }
  size_t get_length() const { return dirs.size(); }
  Direction get_first() const { return dirs.front(); }
  Direction get_last() const { return dirs.back(); }
  bool is_extendable() const { return (dirs.size() < max_length); }
  bool is_valid_extension(Map *map, Direction dir) const;
  bool is_undo(Direction dir) const;
  bool extend(Direction dir);
  bool undo();
  MapPos get_end(Map *map) const;
  bool has_pos(Map *map, MapPos pos) const;
};

class SaveReaderBinary;
class SaveReaderText;
class SaveWriterText;
class MapGenerator;

// Map data.
//
// Initialization of a new Map takes a few steps:
//
// 1. Construct a MapGeometry specifying the size of the map.
// 2. Construct a Map from the MapGeometry.
// 3. Construct a MapGenerator subclass based on the Map object and use
//    it to generate the initial map data.
// 4. Call init_tiles() on the Map supplying the MapGenerator object to copy
//    the new tile data into the Map.
class Map {
 public:
  typedef enum Object {
    ObjectNone = 0,
    ObjectFlag,
    ObjectSmallBuilding,
    ObjectLargeBuilding,
    ObjectCastle,

    ObjectTree0 = 8,
    ObjectTree1,
    ObjectTree2, /* 10 */
    ObjectTree3,
    ObjectTree4,
    ObjectTree5,
    ObjectTree6,
    ObjectTree7, /* 15 */

    ObjectPine0,
    ObjectPine1,
    ObjectPine2,
    ObjectPine3,
    ObjectPine4, /* 20 */
    ObjectPine5,
    ObjectPine6,
    ObjectPine7,

    ObjectPalm0,
    ObjectPalm1, /* 25 */
    ObjectPalm2,
    ObjectPalm3,

    ObjectWaterTree0,
    ObjectWaterTree1,
    ObjectWaterTree2, /* 30 */
    ObjectWaterTree3,

    ObjectStone0 = 72,
    ObjectStone1,
    ObjectStone2,
    ObjectStone3, /* 75 */
    ObjectStone4,
    ObjectStone5,
    ObjectStone6,
    ObjectStone7,

    ObjectSandstone0, /* 80 */
    ObjectSandstone1,

    ObjectCross,
    ObjectStub,

    ObjectStone,
    ObjectSandstone3, /* 85 */

    ObjectCadaver0,
    ObjectCadaver1,

    ObjectWaterStone0,
    ObjectWaterStone1,

    ObjectCactus0, /* 90 */
    ObjectCactus1,

    ObjectDeadTree,

    ObjectFelledPine0,
    ObjectFelledPine1,
    ObjectFelledPine2, /* 95 */
    ObjectFelledPine3,
    ObjectFelledPine4,

    ObjectFelledTree0,
    ObjectFelledTree1,
    ObjectFelledTree2, /* 100 */
    ObjectFelledTree3,
    ObjectFelledTree4,

    ObjectNewPine,
    ObjectNewTree,

    ObjectSeeds0, /* 105 */
    ObjectSeeds1,
    ObjectSeeds2,
    ObjectSeeds3,
    ObjectSeeds4,
    ObjectSeeds5, /* 110 */
    ObjectFieldExpired,

    ObjectSignLargeGold,
    ObjectSignSmallGold,
    ObjectSignLargeIron,
    ObjectSignSmallIron, /* 115 */
    ObjectSignLargeCoal,
    ObjectSignSmallCoal,
    ObjectSignLargeStone,
    ObjectSignSmallStone,

    ObjectSignEmpty, /* 120 */

    ObjectField0,
    ObjectField1,
    ObjectField2,
    ObjectField3,
    ObjectField4, /* 125 */
    ObjectField5,
    Object127
  } Object;

  typedef enum Minerals {
    MineralsNone = 0,
    MineralsGold,
    MineralsIron,
    MineralsCoal,
    MineralsStone,
  } Minerals;

  /* A map space can be OPEN which means that
     a building can be constructed in the space.
     A FILLED space can be passed by a serf, but
     nothing can be built in this space except roads.
     A SEMIPASSABLE space is like FILLED but no roads
     can be built. A IMPASSABLE space can neither be
     used for contructions nor passed by serfs. */
  typedef enum Space {
    SpaceOpen = 0,
    SpaceFilled,
    SpaceSemipassable,
    SpaceImpassable,
  } Space;

  typedef enum {
    TerrainWater0 = 0,
    TerrainWater1,
    TerrainWater2,
    TerrainWater3,
    TerrainGrass0,  // 4
    TerrainGrass1,
    TerrainGrass2,
    TerrainGrass3,
    TerrainDesert0,  // 8
    TerrainDesert1,
    TerrainDesert2,
    TerrainTundra0,  // 11
    TerrainTundra1,
    TerrainTundra2,
    TerrainSnow0,  // 14
    TerrainSnow1
  } Terrain;

  class Handler {
   public:
    virtual ~Handler() {}
    virtual void on_height_changed(MapPos pos) = 0;
    virtual void on_object_changed(MapPos pos) = 0;
  };

  typedef struct LandscapeTile {
    // Landscape filds
    unsigned int height;
    Terrain type_up;
    Terrain type_down;
    Minerals mineral;
    int resource_amount;
    // Mingled fields
    Object obj;

    bool operator == (const LandscapeTile& rhs) const {
      return this->height == rhs.height &&
        this->type_up == rhs.type_up &&
        this->type_down == rhs.type_down &&
        this->mineral == rhs.mineral &&
        this->resource_amount == rhs.resource_amount &&
        this->obj == rhs.obj;
    }
    bool operator != (const LandscapeTile& rhs) const {
      return !(*this == rhs); }
  } LandscapeTile;

  struct UpdateState {
    int remove_signs_counter;
    uint16_t last_tick;
    int counter;
    MapPos initial_pos;

    bool operator == (const UpdateState& rhs) const {
      return this->remove_signs_counter == rhs.remove_signs_counter &&
        this->last_tick == rhs.last_tick &&
        this->counter == rhs.counter &&
        this->initial_pos == rhs.initial_pos;
    }
    bool operator != (const UpdateState& rhs) const {
      return !(*this == rhs);
    }
  };

 protected:
  typedef struct GameTile {
    unsigned int serf;
    unsigned int owner;
    unsigned int obj_index;
    uint8_t paths;
    bool idle_serf;

    bool operator == (const GameTile& rhs) const {
      return this->paths == rhs.paths &&
        this->serf == rhs.serf &&
        this->owner == rhs.owner &&
        this->idle_serf == rhs.idle_serf &&
        this->obj_index == rhs.obj_index;
    }
    bool operator != (const GameTile& rhs) const {
      return !(*this == rhs); }
  } GameTile;

  MapGeometry geom_;
  std::vector<LandscapeTile> landscape_tiles;
  std::vector<GameTile> game_tiles;

  uint16_t regions;

  UpdateState update_state;

  /* Callback for map height changes */
  typedef std::list<Handler*> ChangeHandlers;
  ChangeHandlers change_handlers;

  std::unique_ptr<MapPos[]> spiral_pos_pattern;
  std::unique_ptr<MapPos[]> extended_spiral_pos_pattern;


 public:

  explicit Map(const MapGeometry& geom);

  const MapGeometry& geom() const { return geom_; }

  unsigned int get_size() const { return geom_.size(); }
  unsigned int get_cols() const { return geom_.cols(); }
  unsigned int get_rows() const { return geom_.rows(); }
  unsigned int get_col_mask() const { return geom_.col_mask(); }
  unsigned int get_row_mask() const { return geom_.row_mask(); }
  unsigned int get_row_shift() const { return geom_.row_shift(); }
  unsigned int get_region_count() const { return regions; }

  // Extract col and row from MapPos
  int pos_col(MapPos pos) const { return geom_.pos_col(pos); }
  int pos_row(MapPos pos) const { return geom_.pos_row(pos); }

  // Translate col, row coordinate to MapPos value. */
  MapPos pos(int x, int y) const { return geom_.pos(x, y); }

  // Addition of two map positions.
  MapPos pos_add(MapPos pos, int x, int y) const {
    return geom_.pos_add(pos, x, y); }
  MapPos pos_add(MapPos pos, MapPos off) const {
    return geom_.pos_add(pos, off); }
  MapPos pos_add_spirally(MapPos pos_, unsigned int off) const {
  if (off > 295) { Log::Error["map"] << "cannot use pos_add_spirally() beyond 295 positions (~10 shells)"; }
    return pos_add(pos_, spiral_pos_pattern[off]); }
  MapPos pos_add_extended_spirally(MapPos pos_, unsigned int off) const {
  if (off > 3268) { Log::Error["map"] << "cannot use pos_add_extended_spirally() beyond 3268 positions (~24 shells)"; }
    return pos_add(pos_, extended_spiral_pos_pattern[off]);
  }

  // Shortest distance between map positions.
  int dist_x(MapPos pos1, MapPos pos2) const {
    return -geom_.dist_x(pos1, pos2); }
  int dist_y(MapPos pos1, MapPos pos2) const {
    return -geom_.dist_y(pos1, pos2); }

  // Get random position
  MapPos get_rnd_coord(int *col, int *row, Random *rnd) const;

  // Movement of map position according to directions.
  MapPos move(MapPos pos, Direction dir) const {
    return geom_.move(pos, dir); }

  static void next_extended_spiral_coord(int x, int y, std::vector<int> *vector) {
    vector->push_back(x);
    vector->push_back(y);
  }

  MapPos move_right(MapPos pos) const { return geom_.move_right(pos); }
  MapPos move_down_right(MapPos pos) const {
    return geom_.move_down_right(pos); }
  MapPos move_down(MapPos pos) const { return geom_.move_down(pos); }
  MapPos move_left(MapPos pos) const { return geom_.move_left(pos); }
  MapPos move_up_left(MapPos pos) const { return geom_.move_up_left(pos); }
  MapPos move_up(MapPos pos) const { return geom_.move_up(pos); }

  MapPos move_right_n(MapPos pos, int n) const {
    return geom_.move_right_n(pos, n); }
  MapPos move_down_n(MapPos pos, int n) const {
    return geom_.move_down_n(pos, n); }

  /* Extractors for map data. */
  unsigned int paths(MapPos pos) const {
    return (game_tiles[pos].paths & 0x3f); }
  bool has_path(MapPos pos, Direction dir) const {
    return (BIT_TEST(game_tiles[pos].paths, dir) != 0); }
  bool has_any_path(MapPos pos) {
    for (Direction d : cycle_directions_cw()) {
      if (has_path(pos, d))
        return true;
    }
    return false;
  }
  void add_path(MapPos pos, Direction dir) {
    game_tiles[pos].paths |= BIT(dir); }
  void del_path(MapPos pos, Direction dir) {
    game_tiles[pos].paths &= ~BIT(dir); }

  bool has_owner(MapPos pos) const { return (game_tiles[pos].owner != 0); }
  unsigned int get_owner(MapPos pos) const {
    return game_tiles[pos].owner - 1; }
  void set_owner(MapPos pos, unsigned int _owner) {
    game_tiles[pos].owner = _owner + 1; }
  void del_owner(MapPos pos) { game_tiles[pos].owner = 0; }
  unsigned int get_height(MapPos pos) const {
    return landscape_tiles[pos].height; }

  Terrain type_up(MapPos pos) const { return landscape_tiles[pos].type_up; }
  Terrain type_down(MapPos pos) const { return landscape_tiles[pos].type_down; }
  bool types_within(MapPos pos, Terrain low, Terrain high);

  Object get_obj(MapPos pos) const { return landscape_tiles[pos].obj; }
  bool get_idle_serf(MapPos pos) const { return game_tiles[pos].idle_serf; }
  void set_idle_serf(MapPos pos) { game_tiles[pos].idle_serf = true; }
  void clear_idle_serf(MapPos pos) { game_tiles[pos].idle_serf = false; }

  unsigned int get_obj_index(MapPos pos) const {
    return game_tiles[pos].obj_index; }
  void set_obj_index(MapPos pos, unsigned int index) {
    game_tiles[pos].obj_index = index; }
  Minerals get_res_type(MapPos pos) const {
    return landscape_tiles[pos].mineral; }
  unsigned int get_res_amount(MapPos pos) const {
    return landscape_tiles[pos].resource_amount; }
  unsigned int get_res_fish(MapPos pos) const { return get_res_amount(pos); }
  unsigned int get_serf_index(MapPos pos) const { return game_tiles[pos].serf; }
  unsigned int has_serf(MapPos pos) const {
    return (game_tiles[pos].serf != 0); }

  bool has_flag(MapPos pos) const { return (get_obj(pos) == ObjectFlag); }
  bool has_building(MapPos pos) const { return (get_obj(pos) >=
                                                   ObjectSmallBuilding &&
                                                   get_obj(pos) <=
                                                   ObjectCastle); }

  /* Whether any of the two up/down tiles at this pos are water. */
  bool is_water_tile(MapPos pos) const {
    return (type_down(pos) <= TerrainWater3 &&
            type_up(pos) <= TerrainWater3); }

  /* Whether the position is completely surrounded by water. */
  bool is_in_water(MapPos pos) const {
    return (is_water_tile(pos) &&
            is_water_tile(move_up_left(pos)) &&
            type_down(move_left(pos)) <= TerrainWater3 &&
            type_up(move_up(pos)) <= TerrainWater3); }

  /* Mapping from Object to Space. */
  static const Space map_space_from_obj[128];

  void set_height(MapPos pos, int height);
  void set_height_no_refresh(MapPos pos, int height);
  void set_object(MapPos pos, Object obj, int index);
  void remove_ground_deposit(MapPos pos, int amount);
  void remove_fish(MapPos pos, int amount);
  void set_serf_index(MapPos pos, int index);

  unsigned int get_gold_deposit() const;

  void init_tiles(const MapGenerator &generator);

  void update(unsigned int tick, Random *rnd);
  const UpdateState& get_update_state() const { return update_state; }
  void set_update_state(const UpdateState& update_state_) {
    update_state = update_state_;
  }

  void add_change_handler(Handler *handler);
  void del_change_handler(Handler *handler);

  static int *get_spiral_pattern();
  static int *get_extended_spiral_pattern();

  /* Actually place road segments */
  bool place_road_segments(const Road &road);
  bool remove_road_backref_until_flag(MapPos pos, Direction dir);
  bool remove_road_backrefs(MapPos pos);
  Direction remove_road_segment(MapPos *pos, Direction dir);
  bool road_segment_in_water(MapPos pos, Direction dir);
  bool is_road_segment_valid(MapPos pos, Direction dir) const;

  bool operator == (const Map& rhs) const;
  bool operator != (const Map& rhs) const;

  friend SaveReaderBinary&
    operator >> (SaveReaderBinary &reader, Map &map);
  friend SaveReaderText&
    operator >> (SaveReaderText &reader, Map &map);
  friend SaveWriterText&
    operator << (SaveWriterText &writer, Map &map);

  MapPos pos_from_saved_value(uint32_t val);

 protected:
  void init_spiral_pos_pattern();
  void init_extended_spiral_pos_pattern();

  void update_public(MapPos pos, Random *rnd);
  void update_hidden(MapPos pos, Random *rnd);
};

typedef std::shared_ptr<Map> PMap;

#endif  // SRC_MAP_H_
