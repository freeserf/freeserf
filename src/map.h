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
#include <bitset>   // TEMP DEBUG, only for printing binary representation of integers

#include "src/map-geometry.h"
#include "src/misc.h"
#include "src/random.h"
#include "src/game-options.h"  // for seasons

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
  typedef enum Object {     // NOTE, the map_objects sprite index is always minus 8 from the object number
    ObjectNone = 0,         //     shown in code as Object# - Map::ObjectTree0
    ObjectFlag,
    ObjectSmallBuilding,    // NOTE, even though this enum ends around 126 (sprite #118) the last farmer Field, 
    ObjectLargeBuilding,    //     starting at 136 (sprite #128) begins Flag sprites, followed by Building sprites
    ObjectCastle,           //     these do not exist in the enum Map::Object because the Map does not store the
                            //     actual building type only indicates Flag/SmallBuilding/LargeBuilding/Castle, the
                            //     game has the actual building type.  When drawing game objects, if the map->get_obj
                            //     call returns 'XXXBuilding', game->get_building_at_pos is called to get the actual
                            //     building type so the correct sprite can be drawn
                            //     SO, what this means is that arbitrary new Map::Object types can be added here
                            //     to support new graphics & map features, BUT they cannot be allowed to map directly
                            //     to sprite indexes using the normal sprite# = obj# + 8 because there are only 7 free
                            //     slots before hitting the start of the Flag sprites.  To add more than 7 new objects
                            //     with their own graphics, the map object index must be caught by special logic and
                            //     resolve to a higher sprite index beyond the original range (max 193)

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

    ObjectField0, // 121, sprite 113   // NOTE there is no visual difference between Field0-5, they all contain a copy of the same sprite
    ObjectField1,                      //   the reason there are six types is to easily track the progression from Seeds to Field to FieldExpired
    ObjectField2,                      //   fully inside the Map object (and outside the Game object) 
    ObjectField3,
    ObjectField4, 
    ObjectField5, // 126, sprite 118
    Object127,    // what is this?  I am thinking it is a spillover value so that a Field5 can still be advanced and will be set to FieldExpired if detected?
    
    ObjectFlowerGroupA0,  // 128, sprite 120
    ObjectFlowerGroupA1,  //   IMPORTANT - because sprite 128 (corresponding to undefined MapObject 136) is the start of Flag sprites
    ObjectFlowerGroupA2,  //     there are only 8 empty sprite indexes that could be used for new objects!  Unless, this enum could be extended all the way out
    ObjectFlowerGroupA3,  //     to the full 193 sprites (object 185) which would allow adding new objects 186+ without running into issues with objects that 
    ObjectFlowerGroupA4,  //     cannot be "resolved" in enum (which might not even matter at all?)
    ObjectFlowerGroupA5,  // 133, sprite 125
    ObjectFlowerGroupA6,  // 134, sprite 126
    ObjectFlowerGroupB0,  // 135, sprite 127
    ObjectFlowerGroupB1,  // 136, sprite 128 <-- start of Flag sprites!
    ObjectFlowerGroupB2,  // 137, sprite 129
    ObjectFlowerGroupB3,  // 138, sprite 130
    ObjectFlowerGroupB4,  // 139, sprite 131
    ObjectFlowerGroupB5,  // 140, sprite 132
    ObjectFlowerGroupB6,  // 141, sprite 133
    ObjectFlowerGroupC0,  // 142, sprite 135
    ObjectFlowerGroupC1,  // 143, sprite 136
    ObjectFlowerGroupC2,  // 144, sprite 137
    ObjectFlowerGroupC3,  // 145, sprite 138
    ObjectFlowerGroupC4,  // 146, sprite 139
    ObjectFlowerGroupC5,  // 147, sprite 140
    ObjectFlowerGroupC6,  // 148, sprite 141
  } Object;  // NOTE, the map_objects sprite index is always minus 8 from the object number

  typedef enum Minerals {
    MineralsNone = 0,   // fish???
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
    //TerrainShroud,  // 16  // for option_FogOfWar
    //TerrainWaterFog,       // for option_FogOfWar   // THERE IS NO DARK WATER TILE, WHAT TO DO?
    //TerrainGrassFog,       // for option_FogOfWar
    //TerrainDesertFog,      // for option_FogOfWar
    //TerrainTundraFog, //20 // for option_FogOfWar
    //TerrainSnowFog         // for option_FogOfWar
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
    unsigned int owner;  // I believe this is only used to store values -1 (i.e. INTEGER_MAX) for unowned, or 0-3 for Player0 through Player3.
                         //   Hijacking higher values for option_FogOfWar for various FoW reveal states
                         // NOTE the *actual* in-memory values of game_tile[#].owner are 0-4 (unowned through Player3) but they
                         //  are "minus one" when the getter and setter are used, so they fake being values -1 through 3
                         //  so to avoid stepping on this, skip the 3rd-least-significant bit when it comes to bit-testing for FogOfWar "owner"
                         //
                         //    FAKE VALUES as seen by get/set_owner functions
                         // 11111111 11111111 11111111 11111111 unowned, -1
                         // 00000000 00000000 00000000 00000000 Player0
                         // 00000000 00000000 00000000 00000001 Player1
                         // 00000000 00000000 00000000 00000010 Player2
                         // 00000000 00000000 00000000 00000011 Player3
                         //
                         //    REAL VALUES stored in game_tiles[#].owner variable 
                         // 00000000 00000000 00000000 00000000 unowned
                         // 00000000 00000000 00000000 00000001 Player0
                         // 00000000 00000000 00000000 00000010 Player1
                         // 00000000 00000000 00000000 00000011 Player2
                         // 00000000 00000000 00000000 00000100 Player3
                         // 00000000 00000000 00000xxx xxxx1xxx revealed by Player0
                         // 00000000 00000000 00000xxx xxx1xxxx  visible by Player0
                         // 00000000 00000000 00000xxx xx1xxxxx revealed by Player1
                         //                   ... and so on...
                         // 00000000 00000000 000001xx xxxxxxxx  visible by Player3
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
  std::unique_ptr<MapPos[]> directional_fill_pos_pattern;


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
    if (off > 295) {
      Log::Error["map"] << "cannot use pos_add_spirally() beyond 295 positions (~10 shells), instead try pos_add_extended_spirally";
    }
    return pos_add(pos_, spiral_pos_pattern[off]); }

  MapPos pos_add_extended_spirally(MapPos pos_, unsigned int off) const {
    // NOTE that if the spiral_dist radius/shells is increased further, you must figure out how many
    //   mappos are found by checking the extended_spiral_coord_vector.size() and setting the
    //   extended_spiral_pattern[] int array to that size (maybe plus 1?)
    // you can NOT simply double/times-X increase it, it increases exponentially because the shells 
    //  larger the further out you go  24 shells was 3268 pos, 48 shells is 13445 pos 
    if (off > 13445) {
      Log::Error["map"] << "cannot use pos_add_extended_spirally() beyond 13445 positions (~48 shells)";
      throw ExceptionFreeserf("cannot use pos_add_extended_spirally() beyond 13445 positions (~48 shells)");
    }
    return pos_add(pos_, extended_spiral_pos_pattern[off]);
  }

  MapPos pos_add_directional_fill(MapPos pos_, unsigned int off, int dir) const {
    int dir_offset = 52 * dir;
    return pos_add(pos_, directional_fill_pos_pattern[off+dir_offset]);
  }

  // Shortest distance between map positions.
  int dist_x(MapPos pos1, MapPos pos2) const {
    return -geom_.dist_x(pos1, pos2); }
  int dist_y(MapPos pos1, MapPos pos2) const {
    return -geom_.dist_y(pos1, pos2); }

  // Get random position
  MapPos get_rnd_coord(int *col, int *row, Random *rnd) const;
  MapPos get_better_rnd_coord() const; // truly random function, doesn't require args, returns only MapPos - tlongstretch

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
  // this function is very misleading!  it appears to return true for Direction UpLeft (4)
  //  if there is a building there that accepts_resources.  To check for only real paths
  //   you must also check for building in dir UpLeft and if so skip that Dir when pathfinding
  bool has_path(MapPos pos, Direction dir) const {
    return (BIT_TEST(game_tiles[pos].paths, dir) != 0); }
  // improved function used only for new/AI functions that includes the UpLeft building check
  bool has_path_IMPROVED(MapPos pos, Direction dir) const {
    if (dir == DirectionUpLeft && has_building(move_up_left(pos))){
      return false;
    }else if(has_path(pos, dir)){
      return true;
    }
    return false;
  }
  bool has_any_path(MapPos pos) {
    for (Direction d : cycle_directions_cw()) {
      // must check for building because has_path returns true
      //  for UpLeft if there is a building there that accepts resources!
      if (has_building(move_up_left(pos)) && d == DirectionUpLeft)
        continue;
      if (has_path(pos, d))
        return true;
    }
    return false;
  }
  void add_path(MapPos pos, Direction dir) { game_tiles[pos].paths |= BIT(dir); }
  void del_path(MapPos pos, Direction dir) { game_tiles[pos].paths &= ~BIT(dir); }

  //bool has_owner(MapPos pos) const { return (game_tiles[pos].owner != 0); }  // original
  //bool has_owner(MapPos pos) const { return (game_tiles[pos].owner & 7 != 0); }  // added support for option_FogOfWar
  bool has_owner(MapPos pos) const {
    unsigned int tmp = game_tiles[pos].owner;
    tmp &= 7;
    bool foo = false;
    if (tmp != 0){
      foo = true;
    }
    tmp--;
    //Log::Debug["map.h"] << "inside Map::has_owner, pos " << pos << " has fake owner Player" << tmp << ", real owner " << std::bitset<11>(game_tiles[pos].owner) << ", returning bool " << foo;  // added support for option_FogOfWar
    //return (game_tiles[pos].owner & 7 != 0);
    return foo;
  } 
  //unsigned int get_owner(MapPos pos) const { return game_tiles[pos].owner - 1; }  // original
  //unsigned int get_owner(MapPos pos) const { return (game_tiles[pos].owner & 7) - 1;  // added support for option_FogOfWar
  unsigned int get_owner(MapPos pos) const {
    // seems this is sometimes returning invalid values, saw player #6 (fake player)
    unsigned int tmp = game_tiles[pos].owner;
    tmp &= 7;
    tmp--;
    //Log::Debug["map.h"] << "inside Map::get_owner, pos " << pos << " has fake owner Player" << tmp  << ", real owner " << std::bitset<11>(game_tiles[pos].owner);  // added support for option_FogOfWar
    return tmp;
  }
  //void set_owner(MapPos pos, unsigned int _owner) { game_tiles[pos].owner = _owner + 1; }  // original
  void set_owner(MapPos pos, unsigned int _owner) { // added support for option_FogOfWar
    unsigned int tmp = game_tiles[pos].owner;
    tmp &= 7;
    //Log::Debug["map.h"] << "inside Map::set_owner, pos " << pos << " had fake owner Player" << tmp  << ", real owner " << std::bitset<11>(game_tiles[pos].owner);  // added support for option_FogOfWar
    game_tiles[pos].owner &= -8;  // clear the least-3-bits which hold the Player owner
    //Log::Debug["map.h"] << "inside Map::set_owner, pos " << pos << " applying AND " << std::bitset<11>(-8) << ", interim real owner " << std::bitset<11>(game_tiles[pos].owner);  // added support for option_FogOfWar
    //if (_owner > 3 || _owner == -1){
    //  throw ExceptionFreeserf("_owner Player# >3 or -1 specified to Game::set_owner, invalid");
    //}
    game_tiles[pos].owner |= _owner + 1;  // set the least-3-bits to Player owner
    //Log::Debug["map.h"] << "inside Map::set_owner, pos " << pos << " applying AND " << std::bitset<11>(-8) << ", interim real owner " << std::bitset<11>(game_tiles[pos].owner);  // added support for option_FogOfWar
    tmp = game_tiles[pos].owner;
    tmp &= 7;
    tmp--;
    //Log::Debug["map.h"] << "inside Map::set_owner, pos " << pos << " now has fake owner Player" << tmp  << ", real owner " << std::bitset<11>(game_tiles[pos].owner);  // added support for option_FogOfWar
    //Log::Debug["map.h"] << "inside Map::set_owner calling set_visible for pos " << pos << " with fake owner Player" << tmp;
    set_visible(pos, _owner);  // is this actually needed?
  } 
  //void set_revealed(MapPos pos, unsigned int player) { game_tiles[pos].owner |= BIT(3 + player*2); }   // THIS MIGHT BE BROKEN/WRONG
  void set_revealed(MapPos pos, unsigned int player) {
    unsigned int tmp = game_tiles[pos].owner;
    tmp &= 7;
    tmp--;
    //Log::Debug["map.h"] << "inside Map::set_revealed, pos " << pos << " had fake owner Player" << tmp << ", real owner " << std::bitset<11>(game_tiles[pos].owner);  // added support for option_FogOfWar
    game_tiles[pos].owner |= BIT(3 + player*2);
    tmp = game_tiles[pos].owner;
    tmp &= 7;
    tmp--;
    //Log::Debug["map.h"] << "inside Map::set_revealed, pos " << pos << " now has fake owner Player" << tmp << ", real owner " << std::bitset<11>(game_tiles[pos].owner);  // added support for option_FogOfWar
  }
  // void is_revealead
  void set_visible(MapPos pos, unsigned int player) {
    unsigned int tmp = game_tiles[pos].owner;
    tmp &= 7;
    tmp--;
    //Log::Debug["map.h"] << "inside Map::set_visible, pos " << pos << " had fake owner Player" << tmp << ", real owner " << std::bitset<11>(game_tiles[pos].owner);  // added support for option_FogOfWar
    game_tiles[pos].owner |= BIT(3 + player*2 + 1);
    tmp = game_tiles[pos].owner;
    tmp &= 7;
    tmp--;
    //Log::Debug["map.h"] << "inside Map::set_visible, pos " << pos << " now has fake owner Player" << tmp << ", real owner " << std::bitset<11>(game_tiles[pos].owner);  // added support for option_FogOfWar
    //set_revealed(pos, player);  // this should be independently set when setting visible now that revealed area is a bit larger than visible area
  }
  bool is_revealed(MapPos pos, unsigned int player) {
    //Log::Debug["map.h"] << "inside Map::is_visible, pos " << pos << " has owner " << std::bitset<11>(game_tiles[pos].owner) << ", returning " << BIT_TEST(game_tiles[pos].owner, 3 + player*2);
    return BIT_TEST(game_tiles[pos].owner, 3 + player*2);
  } 
  bool is_visible(MapPos pos, unsigned int player) {
    //Log::Debug["map.h"] << "inside Map::is_visible, pos " << pos << " has owner " << std::bitset<11>(game_tiles[pos].owner) << ", returning " << BIT_TEST(game_tiles[pos].owner, 3 + player*2 + 1);
    return BIT_TEST(game_tiles[pos].owner, 3 + player*2 + 1);
  } 
  //bool is_visible(MapPos pos, unsigned int player) {
  //  Log::Debug["map.h"] << "inside Map::is_visible, pos " << pos << " has owner " << std::bitset<11>(game_tiles[pos].owner);  // added support for option_FogOfWar
  //  unsigned int tmp = 3 + player + 1;
  //  return BIT_TEST(game_tiles[pos].owner, tmp);
  //}
  /* this might not actually be needed, can always use unset_all_visible?
  void unset_visible(MapPos pos, unsigned int player) {
    unsigned int tmp = game_tiles[pos].owner;
    tmp &= 7;
    tmp--;
    Log::Debug["map.h"] << "inside Map::unset_visible, pos " << pos << " had fake owner Player" << tmp << ", real owner " << std::bitset<11>(game_tiles[pos].owner);  // added support for option_FogOfWar
    Log::Debug["map.h"] << "inside Map::unset_visible, pos " << pos << " had fake owner Player" << tmp << ", BIT is " << std::bitset<11>(~BIT(3 + player*2 + 1));  // added support for option_FogOfWar
    game_tiles[pos].owner &= ~BIT(3 + player*2 + 1);
    tmp = game_tiles[pos].owner;
    tmp &= 7;
    tmp--;
    Log::Debug["map.h"] << "inside Map::unset_visible, pos " << pos << " now has fake owner Player" << tmp << ", real owner " << std::bitset<11>(game_tiles[pos].owner);  // added support for option_FogOfWar
  }
  */
  void unset_all_visible(MapPos pos) {
    // 10101010000 is 1360
    // inverse is 01010101111 or 687
    unsigned int tmp = game_tiles[pos].owner;
    tmp &= 7;
    tmp--;
    //Log::Debug["map.h"] << "inside Map::unset_all_visible, pos " << pos << " had fake owner Player" << tmp << ", real owner " << std::bitset<11>(game_tiles[pos].owner);  // added support for option_FogOfWar
    //Log::Debug["map.h"] << "inside Map::unset_all_visible, pos " << pos << " had fake owner Player" << tmp << ", BIT is " << std::bitset<11>(687);  // added support for option_FogOfWar
    game_tiles[pos].owner &= 687;
    tmp = game_tiles[pos].owner;
    tmp &= 7;
    tmp--;
    //Log::Debug["map.h"] << "inside Map::unset_all_visible, pos " << pos << " now has fake owner Player" << tmp << ", real owner " << std::bitset<11>(game_tiles[pos].owner);  // added support for option_FogOfWar
  }
  //void del_owner(MapPos pos) { game_tiles[pos].owner = 0; } // original
  void del_owner(MapPos pos) {
    int tmp = game_tiles[pos].owner;
    //Log::Debug["map.h"] << "inside Map::del_owner, pos " << pos << " had fake owner Player" << tmp << ", real owner " << std::bitset<11>(game_tiles[pos].owner);  // added support for option_FogOfWar
    tmp &= -8;
    game_tiles[pos].owner = tmp;
    tmp &= 7;
    tmp--;
    //Log::Debug["map.h"] << "inside Map::del_owner, pos " << pos << " now has fake owner Player" << tmp << ", real owner " << std::bitset<11>(game_tiles[pos].owner);  // added support for option_FogOfWar
  }   // added support for option_FogOfWar

  unsigned int get_height(MapPos pos) const { return landscape_tiles[pos].height; }

  Terrain type_up(MapPos pos) const { return landscape_tiles[pos].type_up; }
  Terrain type_down(MapPos pos) const { return landscape_tiles[pos].type_down; }
  
  //
  //  DO NOT MESS WITH THIS HERE, INSTEAD CHANGE THE TILES IN Viewport::draw_triangle_up/down
  //   so that it does not affect the gameplay, only the graphics drawn
  //
  /*
  // messing with weather/seasons/palette
  // WINTER
  // make snow cover a bit more of mountains by changing the *appearance*
  // of Tundra0/1/2 to Snow, but it still functions as normal Tundra in game
  // WAIT!!!!! this likely affects the game, I see a lot of functions that call
  //  map->type_xx.  INSTEAD, create a separate function and change viewport to 
  //  use it when drawing terrain, or find the function that does the drawing
  //  and change it only there instead of here
  Terrain type_up(MapPos pos) const {
    Terrain type = landscape_tiles[pos].type_up;
    if (season == 3){
      if (type >= Terrain::TerrainTundra2){  // a bit more snow on mountains
      //if (type >= Terrain::TerrainGrass0){  // all non water-tiles become snow
        return Terrain::TerrainSnow0;
      }else{
        return type;
      }
    }else{
      return type;
    }
  }
  Terrain type_down(MapPos pos) const {
    Terrain type = landscape_tiles[pos].type_down;
    if (season == 3){
      if (type >= Terrain::TerrainTundra2){  // a bit more snow on mountains
      //if (type >= Terrain::TerrainGrass0){  // all non water-tiles become snow
        return Terrain::TerrainSnow0;
      }else{
        return type;
      }
    }else{
      return type;
    }
  }
  */
  
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


  // NOTE - because terrain is only drawn once, this isn't helpful for ambient 
  //  sound triggering.  Disabling for now, might use it later for something else
  /*
  // Whether any of the two up/down tiles at this pos are desert.
  bool is_desert_tile(MapPos pos) const {
    Log::Info["map.h"] << "inside is_desert_tile, type_down is " << type_down(pos) << ", type_up is " << type_up(pos);
    if ( (type_down(pos) >= TerrainDesert0 && type_down(pos) <= TerrainDesert2)
      || (type_up(pos)   >= TerrainDesert0 && type_up(pos)   <= TerrainDesert2)){
        Log::Info["map.h"] << "inside is_desert_tile, IS desert tile";
        return true;
    }
    return false;
  }
  */
          


  /* Mapping from Object to Space. */
  //static const Space map_space_from_obj[128];
  static const Space map_space_from_obj[131];  // added Flowers   
  static const uint8_t obj_height_for_slope_darken[148];  // added Flowers0

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
  static int *get_directional_fill_pattern();

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
  void init_directional_fill_pos_pattern();

  void update_public(MapPos pos, Random *rnd);
  void update_hidden(MapPos pos, Random *rnd);
  void update_environment(MapPos pos, Random *rnd); // tlongstretch new features
};

typedef std::shared_ptr<Map> PMap;

#endif  // SRC_MAP_H_
