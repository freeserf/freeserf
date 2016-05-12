/*
 * map.h - Map generators and map update functions
 *
 * Copyright (C) 2013  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/misc.h"
#include "src/random.h"

typedef enum {
  DIR_NONE = -1,

  DIR_RIGHT = 0,
  DIR_DOWN_RIGHT,
  DIR_DOWN,
  DIR_LEFT,
  DIR_UP_LEFT,
  DIR_UP,

  DIR_UP_RIGHT,
  DIR_DOWN_LEFT
} dir_t;

#define DIR_REVERSE(dir)  ((dir_t)(((dir) + 3) % 6))

typedef enum {
  MAP_OBJ_NONE = 0,
  MAP_OBJ_FLAG,
  MAP_OBJ_SMALL_BUILDING,
  MAP_OBJ_LARGE_BUILDING,
  MAP_OBJ_CASTLE,

  MAP_OBJ_TREE_0 = 8,
  MAP_OBJ_TREE_1,
  MAP_OBJ_TREE_2, /* 10 */
  MAP_OBJ_TREE_3,
  MAP_OBJ_TREE_4,
  MAP_OBJ_TREE_5,
  MAP_OBJ_TREE_6,
  MAP_OBJ_TREE_7, /* 15 */

  MAP_OBJ_PINE_0,
  MAP_OBJ_PINE_1,
  MAP_OBJ_PINE_2,
  MAP_OBJ_PINE_3,
  MAP_OBJ_PINE_4, /* 20 */
  MAP_OBJ_PINE_5,
  MAP_OBJ_PINE_6,
  MAP_OBJ_PINE_7,

  MAP_OBJ_PALM_0,
  MAP_OBJ_PALM_1, /* 25 */
  MAP_OBJ_PALM_2,
  MAP_OBJ_PALM_3,

  MAP_OBJ_WATER_TREE_0,
  MAP_OBJ_WATER_TREE_1,
  MAP_OBJ_WATER_TREE_2, /* 30 */
  MAP_OBJ_WATER_TREE_3,

  MAP_OBJ_STONE_0 = 72,
  MAP_OBJ_STONE_1,
  MAP_OBJ_STONE_2,
  MAP_OBJ_STONE_3, /* 75 */
  MAP_OBJ_STONE_4,
  MAP_OBJ_STONE_5,
  MAP_OBJ_STONE_6,
  MAP_OBJ_STONE_7,

  MAP_OBJ_SANDSTONE_0, /* 80 */
  MAP_OBJ_SANDSTONE_1,

  MAP_OBJ_CROSS,
  MAP_OBJ_STUB,

  MAP_OBJ_STONE,
  MAP_OBJ_SANDSTONE_3, /* 85 */

  MAP_OBJ_CADAVER_0,
  MAP_OBJ_CADAVER_1,

  MAP_OBJ_WATER_STONE_0,
  MAP_OBJ_WATER_STONE_1,

  MAP_OBJ_CACTUS_0, /* 90 */
  MAP_OBJ_CACTUS_1,

  MAP_OBJ_DEAD_TREE,

  MAP_OBJ_FELLED_PINE_0,
  MAP_OBJ_FELLED_PINE_1,
  MAP_OBJ_FELLED_PINE_2, /* 95 */
  MAP_OBJ_FELLED_PINE_3,
  MAP_OBJ_FELLED_PINE_4,

  MAP_OBJ_FELLED_TREE_0,
  MAP_OBJ_FELLED_TREE_1,
  MAP_OBJ_FELLED_TREE_2, /* 100 */
  MAP_OBJ_FELLED_TREE_3,
  MAP_OBJ_FELLED_TREE_4,

  MAP_OBJ_NEW_PINE,
  MAP_OBJ_NEW_TREE,

  MAP_OBJ_SEEDS_0, /* 105 */
  MAP_OBJ_SEEDS_1,
  MAP_OBJ_SEEDS_2,
  MAP_OBJ_SEEDS_3,
  MAP_OBJ_SEEDS_4,
  MAP_OBJ_SEEDS_5, /* 110 */
  MAP_OBJ_FIELD_EXPIRED,

  MAP_OBJ_SIGN_LARGE_GOLD,
  MAP_OBJ_SIGN_SMALL_GOLD,
  MAP_OBJ_SIGN_LARGE_IRON,
  MAP_OBJ_SIGN_SMALL_IRON, /* 115 */
  MAP_OBJ_SIGN_LARGE_COAL,
  MAP_OBJ_SIGN_SMALL_COAL,
  MAP_OBJ_SIGN_LARGE_STONE,
  MAP_OBJ_SIGN_SMALL_STONE,

  MAP_OBJ_SIGN_EMPTY, /* 120 */

  MAP_OBJ_FIELD_0,
  MAP_OBJ_FIELD_1,
  MAP_OBJ_FIELD_2,
  MAP_OBJ_FIELD_3,
  MAP_OBJ_FIELD_4, /* 125 */
  MAP_OBJ_FIELD_5,
  MAP_OBJ_127
} map_obj_t;


/* A map space can be OPEN which means that
   a building can be constructed in the space.
   A FILLED space can be passed by a serf, but
   nothing can be built in this space except roads.
   A SEMIPASSABLE space is like FILLED but no roads
   can be built. A IMPASSABLE space can neither be
   used for contructions nor passed by serfs. */
typedef enum {
  MAP_SPACE_OPEN = 0,
  MAP_SPACE_FILLED,
  MAP_SPACE_SEMIPASSABLE,
  MAP_SPACE_IMPASSABLE,
} map_space_t;

typedef enum {
  GROUND_DEPOSIT_NONE = 0,
  GROUND_DEPOSIT_GOLD,
  GROUND_DEPOSIT_IRON,
  GROUND_DEPOSIT_COAL,
  GROUND_DEPOSIT_STONE,
} ground_deposit_t;

/* map_pos_t is a compact composition of col and row values that
   uniquely identifies a vertex in the map space. It is also used
   directly as index to map data arrays. */
typedef unsigned int map_pos_t;

class save_reader_binary_t;
class save_reader_text_t;
class save_writer_text_t;

class update_map_height_handler_t {
 public:
  virtual void changed_height(map_pos_t pos) = 0;
};

class map_t {
 protected:
  typedef struct {
    uint8_t paths;
    uint8_t height;
    uint8_t type;
    uint8_t obj;
    uint16_t obj_index;
    uint8_t resource;
    uint16_t serf;
  } map_tile_t;

  /* Fundamentals */
  unsigned int size;
  map_tile_t *tiles;
  unsigned int col_size, row_size;

  /* Derived */
  map_pos_t dirs[8];
  unsigned int tile_count;
  unsigned int cols, rows;
  unsigned int col_mask, row_mask;
  unsigned int row_shift;

  uint8_t *minimap;

  int16_t water_level;
  int16_t max_lake_area;

  bool preserve_bugs;

  uint16_t regions;

  uint32_t gold_deposit;

  random_state_t rnd;

  int16_t update_map_16_loop;
  uint16_t update_map_last_tick;
  int16_t update_map_counter;
  map_pos_t update_map_initial_pos;

  /* Callback for map height changes */
  typedef std::list<update_map_height_handler_t*> change_handlers_t;
  change_handlers_t change_handlers;

  map_pos_t *spiral_pos_pattern;

 public:
  map_t();
  virtual ~map_t();

  unsigned int get_size() const { return size; }
  unsigned int get_cols() const { return cols; }
  unsigned int get_rows() const { return rows; }
  unsigned int get_col_mask() const { return col_mask; }
  unsigned int get_row_mask() const { return row_mask; }
  unsigned int get_row_shift() const { return row_shift; }

  /* Extract col and row from map_pos_t */
  int pos_col(int pos) const { return (pos & col_mask); }
  int pos_row(int pos) const { return ((pos >> row_shift) & row_mask); }

  /* Translate col, row coordinate to map_pos_t value. */
  map_pos_t pos(int x, int y) const { return ((y << row_shift) | x); }

  /* Addition of two map positions. */
  map_pos_t pos_add(map_pos_t pos_, map_pos_t off) const {
    return pos((pos_col(pos_) + pos_col(off)) & col_mask,
               (pos_row(pos_) + pos_row(off)) & row_mask); }
  map_pos_t pos_add_spirally(map_pos_t pos_, unsigned int off) const {
    return pos_add(pos_, spiral_pos_pattern[off]); }

  /* Movement of map position according to directions. */
  map_pos_t move(map_pos_t pos, dir_t dir) const {
    return pos_add(pos, dirs[dir]); }

  map_pos_t move_right(map_pos_t pos) const { return move(pos, DIR_RIGHT); }
  map_pos_t move_down_right(map_pos_t pos) const {
    return move(pos, DIR_DOWN_RIGHT); }
  map_pos_t move_down(map_pos_t pos) const { return move(pos, DIR_DOWN); }
  map_pos_t move_left(map_pos_t pos) const { return move(pos, DIR_LEFT); }
  map_pos_t move_up_left(map_pos_t pos) const { return move(pos, DIR_UP_LEFT); }
  map_pos_t move_up(map_pos_t pos) const { return move(pos, DIR_UP); }

  map_pos_t move_up_right(map_pos_t pos) const {
    return move(pos, DIR_UP_RIGHT); }
  map_pos_t move_down_left(map_pos_t pos) const {
    return move(pos, DIR_DOWN_LEFT); }

  map_pos_t move_right_n(map_pos_t pos, int n) const {
    return pos_add(pos, dirs[DIR_RIGHT]*n); }
  map_pos_t move_down_n(map_pos_t pos, int n) const {
    return pos_add(pos, dirs[DIR_DOWN]*n); }

  /* Extractors for map data. */
  unsigned int paths(map_pos_t pos) const { return (tiles[pos].paths & 0x3f); }
  bool has_path(map_pos_t pos, dir_t dir) const {
    return (BIT_TEST(tiles[pos].paths, dir) != 0); }
  void add_path(map_pos_t pos, dir_t dir) { tiles[pos].paths |= BIT(dir); }
  void del_path(map_pos_t pos, dir_t dir) { tiles[pos].paths &= ~BIT(dir); }

  bool has_owner(map_pos_t pos) const { return ((tiles[pos].height >> 7) & 1); }
  unsigned int get_owner(map_pos_t pos) const {
                                        return ((tiles[pos].height >> 5) & 3); }
  void set_owner(map_pos_t pos, unsigned int player) {
    tiles[pos].height = (1 << 7) | (player << 5) | get_height(pos); }
  void del_owner(map_pos_t pos) { tiles[pos].height &= 0x1f; }
  unsigned int get_height(map_pos_t pos) const {
    return (tiles[pos].height & 0x1f); }

  unsigned int type_up(map_pos_t pos) const {
    return ((tiles[pos].type >> 4) & 0xf); }
  unsigned int type_down(map_pos_t pos) const {
    return (tiles[pos].type & 0xf); }
  bool types_within(map_pos_t pos, unsigned int low, unsigned int high);

  map_obj_t get_obj(map_pos_t pos) const {
    return (map_obj_t)(tiles[pos].obj & 0x7f); }
  unsigned int get_idle_serf(map_pos_t pos) const {
    return ((tiles[pos].obj >> 7) & 1); }
  void set_idle_serf(map_pos_t pos) { tiles[pos].obj |= BIT(7); }
  void clear_idle_serf(map_pos_t pos) { tiles[pos].obj &= ~BIT(7); }

  unsigned int get_obj_index(map_pos_t pos) const {
    return tiles[pos].obj_index; }
  void set_obj_index(map_pos_t pos, unsigned int index) {
    tiles[pos].obj_index = index; }
  ground_deposit_t get_res_type(map_pos_t pos) const {
    return (ground_deposit_t)((tiles[pos].resource >> 5) & 7); }
  unsigned int get_res_amount(map_pos_t pos) const {
    return (tiles[pos].resource & 0x1f); }
  unsigned int get_res_fish(map_pos_t pos) const { return tiles[pos].resource; }
  unsigned int get_serf_index(map_pos_t pos) const { return tiles[pos].serf; }

  bool has_flag(map_pos_t pos) const { return (get_obj(pos) == MAP_OBJ_FLAG); }
  bool has_building(map_pos_t pos) const { return (get_obj(pos) >=
                                                   MAP_OBJ_SMALL_BUILDING &&
                                                   get_obj(pos) <=
                                                   MAP_OBJ_CASTLE); }

  /* Whether any of the two up/down tiles at this pos are water. */
  bool is_water_tile(map_pos_t pos) const { return (type_down(pos) < 4 &&
                                                    type_up(pos) < 4); }

  /* Whether the position is completely surrounded by water. */
  bool is_in_water(map_pos_t pos) const {
    return (is_water_tile(pos) &&
            is_water_tile(move_up_left(pos)) &&
            type_down(move_left(pos)) < 4 &&
            type_up(move_up(pos)) < 4); }

  /* Mapping from map_obj_t to map_space_t. */
  static const map_space_t map_space_from_obj[128];

  void set_height(map_pos_t pos, int height);
  void set_object(map_pos_t pos, map_obj_t obj, int index);
  void remove_ground_deposit(map_pos_t pos, int amount);
  void remove_fish(map_pos_t pos, int amount);
  void set_serf_index(map_pos_t pos, int index);

  unsigned int get_gold_deposit() const { return gold_deposit; }
  void add_gold_deposit(size_t delta) { gold_deposit += delta; }

  void init(unsigned int size);
  void init_dimensions();
  uint8_t *get_minimap();

  void generate(int generator, const random_state_t &rnd, bool preserve_bugs);

  void update(unsigned int tick);

  void add_change_handler(update_map_height_handler_t *handler);
  void del_change_handler(update_map_height_handler_t *handler);

  static int *get_spiral_pattern();

  /* Actually place road segments */
  bool place_road_segments(map_pos_t source, const dir_t *dirs,
                           unsigned int length);
  bool remove_road_backref_until_flag(map_pos_t pos, dir_t dir);
  bool remove_road_backrefs(map_pos_t pos);
  dir_t remove_road_segment(map_pos_t *pos, dir_t dir);
  bool road_segment_in_water(map_pos_t pos, dir_t dir);
  bool is_road_segment_valid(map_pos_t pos, dir_t dir);

  friend save_reader_binary_t&
    operator >> (save_reader_binary_t &reader, map_t &map);
  friend save_reader_text_t&
    operator >> (save_reader_text_t &reader, map_t &map);
  friend save_writer_text_t&
    operator << (save_writer_text_t &writer, map_t &map);

 public:
  uint16_t random_int();

 protected:
  void init_minimap();

  map_pos_t get_rnd_coord(int *col, int *row);
  void init_heights_squares();
  int calc_height_displacement(int avg, int base, int offset);
  void init_heights_midpoints();
  void init_heights_diamond_square();
  bool adjust_map_height(int h1, int h2, map_pos_t pos);
  void clamp_heights();

  int expand_level_area(map_pos_t pos, int limit, int r);
  void init_level_area(map_pos_t pos);
  void init_sea_level();
  void heights_rebase();
  void init_types();
  void init_types_2_sub();
  void init_types_2();
  void heights_rescale();
  void init_types_shared_sub(unsigned int old, unsigned int seed,
                             unsigned int new_);
  void init_lakes();
  void init_types4();
  map_pos_t lookup_pattern(int col, int row, int index);
  int init_desert_sub1(map_pos_t pos);
  int init_desert_sub2(map_pos_t pos);
  void init_desert();
  void init_desert_2_sub();
  void init_desert_2();
  void init_crosses();
  int init_objects_shared_sub1(map_pos_t pos, int min, int max);
  map_pos_t lookup_rnd_pattern(int col, int row, int mask);
  void init_objects_shared(int num_clusters, int objs_in_cluster, int pos_mask,
                           int type_min, int type_max, int obj_base,
                           int obj_mask);
  void init_trees_1();
  void init_trees_2();
  void init_trees_3();
  void init_trees_4();
  void init_stone_1();
  void init_stone_2();
  void init_dead_trees();
  void init_large_boulders();
  void init_water_trees();
  void init_stubs();
  void init_small_boulders();
  void init_cadavers();
  void init_cacti();
  void init_water_stones();
  void init_palms();
  void init_resources_shared_sub(int iters, int col, int row, int *index,
                                 int amount, ground_deposit_t type);
  void init_resources_shared(int num_clusters, ground_deposit_t type,
                             int min, int max);
  void init_resources();
  void init_clean_up();
  void init_sub();
  void init_ground_gold_deposit();
  void init_spiral_pos_pattern();

  void update_public(map_pos_t pos);
  void update_hidden(map_pos_t pos);
};

#endif  // SRC_MAP_H_
