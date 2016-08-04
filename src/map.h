/*
 * map.h - Map data and map update functions
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

#ifndef SRC_MAP_H_
#define SRC_MAP_H_

#include <list>
#include <limits>
#include <utility>

#include "src/misc.h"
#include "src/random.h"

/* Map directions

      A ______ B
       /\    /
      /  \  /
   C /____\/ D

   Six standard directions:
   RIGHT: A to B
   DOWN_RIGHT: A to D
   DOWN: A to C
   LEFT: D to C
   UP_LEFT: D to A
   UP: D to B

   Non-standard directions:
   UP_RIGHT: C to B
   DOWN_LEFT: B to C
*/
typedef enum Direction {
  DirectionNone = -1,

  DirectionRight = 0,
  DirectionDownRight,
  DirectionDown,
  DirectionLeft,
  DirectionUpLeft,
  DirectionUp,

  DirectionUpRight,
  DirectionDownLeft
} Direction;

/* Return the given direction turned clockwise a number of times.

   Return the resulting direction from turning the given direction
   clockwise in 60 degree increment the specified number of times.
   If times is a negative number the direction will be turned counter
   clockwise. NOTE: Only valid for the six standard directions. */
inline Direction turn_direction(Direction d, int times) {
  int td = (static_cast<int>(d) + times) % 6;
  if (td < 0) td += 6;
  return static_cast<Direction>(td);
}

/* Return the given direction reversed.

   NOTE: Only valid for the six standard directions.
*/
inline Direction reverse_direction(Direction d) {
  return turn_direction(d, 3);
}

/* MapPos is a compact composition of col and row values that
   uniquely identifies a vertex in the map space. It is also used
   directly as index to map data arrays. */
typedef unsigned int MapPos;
const MapPos bad_map_pos = std::numeric_limits<unsigned int>::max();
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
  Direction get_last() const { return dirs.back(); }
  bool is_extendable() const { return (dirs.size() < max_length); }
  bool is_valid_extension(Map *map, Direction dir) const;
  bool is_undo(Direction dir) const;
  bool extend(Direction dir);
  bool undo();
  MapPos get_end(Map *map) const;
};

class SaveReaderBinary;
class SaveReaderText;
class SaveWriterText;
class MapGenerator;

/* Map data.
 Initialization of a new map_t takes three steps:

 1. Construct map_t and call init() to have basic size fields initialized.
 2. Constuct a MapGenerator subclass based on the map_t object and use it
 to generate the map data.
 3. Call init_tiles() on the map_t supplying the MapGenerator object to
 copy the new tile data into map_t.
 */

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
  } LandscapeTile;

  struct UpdateState {
    int remove_signs_counter;
    uint16_t last_tick;
    int counter;
    MapPos initial_pos;
  };

 protected:
  typedef struct GameTile {
    uint8_t paths;
    unsigned int serf;
    unsigned int owner;
    bool idle_serf;
    unsigned int obj_index;
  } GameTile;

  /* Fundamentals */
  unsigned int size;
  LandscapeTile *landscape_tiles;
  GameTile *game_tiles;
  unsigned int col_size, row_size;

  /* Derived */
  MapPos dirs[8];
  unsigned int tile_count;
  unsigned int cols, rows;
  unsigned int col_mask, row_mask;
  unsigned int row_shift;

  uint16_t regions;

  uint32_t gold_deposit;

  UpdateState update_state;

  /* Callback for map height changes */
  typedef std::list<Handler*> change_handlers_t;
  change_handlers_t change_handlers;

  MapPos *spiral_pos_pattern;

 public:
  Map();
  virtual ~Map();

  unsigned int get_size() const { return size; }
  unsigned int get_cols() const { return cols; }
  unsigned int get_rows() const { return rows; }
  unsigned int get_col_mask() const { return col_mask; }
  unsigned int get_row_mask() const { return row_mask; }
  unsigned int get_row_shift() const { return row_shift; }
  unsigned int get_region_count() const { return regions; }

  /* Extract col and row from MapPos */
  int pos_col(int pos) const { return (pos & col_mask); }
  int pos_row(int pos) const { return ((pos >> row_shift) & row_mask); }

  /* Translate col, row coordinate to MapPos value. */
  MapPos pos(int x, int y) const { return ((y << row_shift) | x); }

  /* Addition of two map positions. */
  MapPos pos_add(MapPos pos_, MapPos off) const {
    return pos((pos_col(pos_) + pos_col(off)) & col_mask,
               (pos_row(pos_) + pos_row(off)) & row_mask); }
  MapPos pos_add_spirally(MapPos pos_, unsigned int off) const {
    return pos_add(pos_, spiral_pos_pattern[off]); }

  /* Get random position */
  MapPos get_rnd_coord(int *col, int *row, Random *rnd) const;

  /* Movement of map position according to directions. */
  MapPos move(MapPos pos, Direction dir) const {
    return pos_add(pos, dirs[dir]); }

  MapPos move_right(MapPos pos) const { return move(pos, DirectionRight); }
  MapPos move_down_right(MapPos pos) const {
    return move(pos, DirectionDownRight); }
  MapPos move_down(MapPos pos) const { return move(pos, DirectionDown); }
  MapPos move_left(MapPos pos) const { return move(pos, DirectionLeft); }
  MapPos move_up_left(MapPos pos) const { return move(pos, DirectionUpLeft); }
  MapPos move_up(MapPos pos) const { return move(pos, DirectionUp); }

  MapPos move_up_right(MapPos pos) const {
    return move(pos, DirectionUpRight); }
  MapPos move_down_left(MapPos pos) const {
    return move(pos, DirectionDownLeft); }

  MapPos move_right_n(MapPos pos, int n) const {
    return pos_add(pos, dirs[DirectionRight]*n); }
  MapPos move_down_n(MapPos pos, int n) const {
    return pos_add(pos, dirs[DirectionDown]*n); }

  /* Extractors for map data. */
  unsigned int paths(MapPos pos) const {
    return (game_tiles[pos].paths & 0x3f); }
  bool has_path(MapPos pos, Direction dir) const {
    return (BIT_TEST(game_tiles[pos].paths, dir) != 0); }
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
  unsigned int get_res_fish(MapPos pos) const {
    return landscape_tiles[pos].resource_amount; }
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
  void set_object(MapPos pos, Object obj, int index);
  void remove_ground_deposit(MapPos pos, int amount);
  void remove_fish(MapPos pos, int amount);
  void set_serf_index(MapPos pos, int index);

  unsigned int get_gold_deposit() const { return gold_deposit; }
  void add_gold_deposit(int delta) { gold_deposit += delta; }

  void init(unsigned int size);
  void init_dimensions();

  void init_tiles(const MapGenerator &generator);

  void update(unsigned int tick, Random *rnd);
  const UpdateState& get_update_state() const { return update_state; }
  void set_update_state(const UpdateState& update_state_) {
    update_state = update_state_;
  }

  void add_change_handler(Handler *handler);
  void del_change_handler(Handler *handler);

  static int *get_spiral_pattern();

  /* Actually place road segments */
  bool place_road_segments(const Road &road);
  bool remove_road_backref_until_flag(MapPos pos, Direction dir);
  bool remove_road_backrefs(MapPos pos);
  Direction remove_road_segment(MapPos *pos, Direction dir);
  bool road_segment_in_water(MapPos pos, Direction dir);
  bool is_road_segment_valid(MapPos pos, Direction dir);

  friend SaveReaderBinary&
    operator >> (SaveReaderBinary &reader, Map &map);
  friend SaveReaderText&
    operator >> (SaveReaderText &reader, Map &map);
  friend SaveWriterText&
    operator << (SaveWriterText &writer, Map &map);

 protected:
  void deinit();

  void init_ground_gold_deposit();
  void init_spiral_pos_pattern();

  void update_public(MapPos pos, Random *rnd);
  void update_hidden(MapPos pos, Random *rnd);
};

#endif  // SRC_MAP_H_
