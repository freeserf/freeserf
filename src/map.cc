/*
 * map.cc - Map data and map update functions
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

/* Basically the map is constructed from a regular, square grid, with
   rows and columns, except that the grid is actually sheared like this:
   http://mathworld.wolfram.com/Polyrhomb.html
   This is the foundational 2D grid for the map, where each vertex can be
   identified by an integer col and row (commonly encoded as map_pos_t).

   Each tile has the shape of a rhombus:
      A ______ B
       /\    /
      /  \  /
   C /____\/ D

   but is actually composed of two triangles called "up" (a,c,d) and
   "down" (a,b,d). A serf can move on the perimeter of any of these
   triangles. Each vertex has various properties associated with it,
   among others a height value which means that the 3D landscape is
   defined by these points in (col, row, height)-space.

   Map elevation and type
   ----------------------
   The type of terrain is determined by either the elevation or the adjacency
   to other terrain types when the map is generated. The type is encoded
   separately from the elevation so it is only the map generator enforcing this
   correlation. The elevation (height) values range in 0-31 while the type
   ranges in 0-15.

   Terrain types:
   - 0-3: Water (range encodes adjacency to shore)
   - 4-7: Grass (4=adjacency to water, 5=only tile that allows large buildings,
                 6-7=elevation based)
   - 8-10: Desert (range encodes adjacency to grass)
   - 11-13: Tundra (elevation based)
   - 14-15: Snow (elevation based)

   For water tiles, desert tiles and grass tile 4, the ranges of values are
   used to encode distance to other terrains. For example, type 4 grass is
   adjacent to at least one water tile and type 3 water is adjacent to at
   least one grass tile. Type 2 water is adjacent to at least one type 3 water,
   type 1 water is adjacent to at least one type 2 water, and so on. The lower
   desert tiles (8) are close to grass while the higher (10) are at the center
   of the desert. The higher grass tiles (5-7), tundra tiles, and snow tiles
   are determined by elevation and _not_ be adjacency.
*/

#include "src/map.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <utility>

#include "src/debug.h"
#include "src/savegame.h"
#include "src/map-generator.h"

/* Facilitates quick lookup of offsets following a spiral pattern in the map data.
 The columns following the second are filled out by setup_spiral_pattern(). */
static int spiral_pattern[] = {
  0, 0,
  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  5, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  5, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  5, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  6, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  6, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  6, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  6, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  6, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  7, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  7, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  7, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  7, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  7, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  7, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  8, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  8, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  8, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  8, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  8, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  8, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  8, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  16, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  24, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  24, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int spiral_pattern_initialized = 0;

/* Initialize the global spiral_pattern. */
static void
init_spiral_pattern() {
  if (spiral_pattern_initialized) {
    return;
  }

  static const int spiral_matrix[] = {
    1,  0,  0,  1,
    1,  1, -1,  0,
    0,  1, -1, -1,
    -1,  0,  0, -1,
    -1, -1,  1,  0,
    0, -1,  1,  1
  };

  for (int i = 0; i < 49; i++) {
    int x = spiral_pattern[2 + 12*i];
    int y = spiral_pattern[2 + 12*i + 1];

    for (int j = 0; j < 6; j++) {
      spiral_pattern[2+12*i+2*j] = x*spiral_matrix[4*j+0] +
                                   y*spiral_matrix[4*j+2];
      spiral_pattern[2+12*i+2*j+1] = x*spiral_matrix[4*j+1] +
                                     y*spiral_matrix[4*j+3];
    }
  }

  spiral_pattern_initialized = 1;
}

int *
map_t::get_spiral_pattern() {
  return spiral_pattern;
}

/* Map map_obj_t to map_space_t. */
const map_space_t map_t::map_space_from_obj[] = {
  MAP_SPACE_OPEN,        // MAP_OBJ_NONE = 0,
  MAP_SPACE_FILLED,      // MAP_OBJ_FLAG,
  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_SMALL_BUILDING,
  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_LARGE_BUILDING,
  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_CASTLE,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,

  MAP_SPACE_FILLED,      // MAP_OBJ_TREE_0 = 8,
  MAP_SPACE_FILLED,      // MAP_OBJ_TREE_1,
  MAP_SPACE_FILLED,      // MAP_OBJ_TREE_2, /* 10 */
  MAP_SPACE_FILLED,      // MAP_OBJ_TREE_3,
  MAP_SPACE_FILLED,      // MAP_OBJ_TREE_4,
  MAP_SPACE_FILLED,      // MAP_OBJ_TREE_5,
  MAP_SPACE_FILLED,      // MAP_OBJ_TREE_6,
  MAP_SPACE_FILLED,      // MAP_OBJ_TREE_7, /* 15 */

  MAP_SPACE_FILLED,      // MAP_OBJ_PINE_0,
  MAP_SPACE_FILLED,      // MAP_OBJ_PINE_1,
  MAP_SPACE_FILLED,      // MAP_OBJ_PINE_2,
  MAP_SPACE_FILLED,      // MAP_OBJ_PINE_3,
  MAP_SPACE_FILLED,      // MAP_OBJ_PINE_4, /* 20 */
  MAP_SPACE_FILLED,      // MAP_OBJ_PINE_5,
  MAP_SPACE_FILLED,      // MAP_OBJ_PINE_6,
  MAP_SPACE_FILLED,      // MAP_OBJ_PINE_7,

  MAP_SPACE_FILLED,      // MAP_OBJ_PALM_0,
  MAP_SPACE_FILLED,      // MAP_OBJ_PALM_1, /* 25 */
  MAP_SPACE_FILLED,      // MAP_OBJ_PALM_2,
  MAP_SPACE_FILLED,      // MAP_OBJ_PALM_3,

  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_WATER_TREE_0,
  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_WATER_TREE_1,
  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_WATER_TREE_2, /* 30 */
  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_WATER_TREE_3,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,
  MAP_SPACE_OPEN,

  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_STONE_0 = 72,
  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_STONE_1,
  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_STONE_2,
  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_STONE_3, /* 75 */
  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_STONE_4,
  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_STONE_5,
  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_STONE_6,
  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_STONE_7,

  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_SANDSTONE_0, /* 80 */
  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_SANDSTONE_1,

  MAP_SPACE_FILLED,      // MAP_OBJ_CROSS,
  MAP_SPACE_OPEN,        // MAP_OBJ_STUB,

  MAP_SPACE_OPEN,        // MAP_OBJ_STONE,
  MAP_SPACE_OPEN,        // MAP_OBJ_SANDSTONE_3, /* 85 */

  MAP_SPACE_OPEN,        // MAP_OBJ_CADAVER_0,
  MAP_SPACE_OPEN,        // MAP_OBJ_CADAVER_1,

  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_WATER_STONE_0,
  MAP_SPACE_IMPASSABLE,    // MAP_OBJ_WATER_STONE_1,

  MAP_SPACE_FILLED,      // MAP_OBJ_CACTUS_0, /* 90 */
  MAP_SPACE_FILLED,      // MAP_OBJ_CACTUS_1,

  MAP_SPACE_FILLED,      // MAP_OBJ_DEAD_TREE,

  MAP_SPACE_FILLED,      // MAP_OBJ_FELLED_PINE_0,
  MAP_SPACE_FILLED,      // MAP_OBJ_FELLED_PINE_1,
  MAP_SPACE_FILLED,      // MAP_OBJ_FELLED_PINE_2, /* 95 */
  MAP_SPACE_FILLED,      // MAP_OBJ_FELLED_PINE_3,
  MAP_SPACE_OPEN,        // MAP_OBJ_FELLED_PINE_4,

  MAP_SPACE_FILLED,      // MAP_OBJ_FELLED_TREE_0,
  MAP_SPACE_FILLED,      // MAP_OBJ_FELLED_TREE_1,
  MAP_SPACE_FILLED,      // MAP_OBJ_FELLED_TREE_2, /* 100 */
  MAP_SPACE_FILLED,      // MAP_OBJ_FELLED_TREE_3,
  MAP_SPACE_OPEN,        // MAP_OBJ_FELLED_TREE_4,

  MAP_SPACE_FILLED,      // MAP_OBJ_NEW_PINE,
  MAP_SPACE_FILLED,      // MAP_OBJ_NEW_TREE,

  MAP_SPACE_SEMIPASSABLE,    // MAP_OBJ_SEEDS_0, /* 105 */
  MAP_SPACE_SEMIPASSABLE,    // MAP_OBJ_SEEDS_1,
  MAP_SPACE_SEMIPASSABLE,    // MAP_OBJ_SEEDS_2,
  MAP_SPACE_SEMIPASSABLE,    // MAP_OBJ_SEEDS_3,
  MAP_SPACE_SEMIPASSABLE,    // MAP_OBJ_SEEDS_4,
  MAP_SPACE_SEMIPASSABLE,    // MAP_OBJ_SEEDS_5, /* 110 */
  MAP_SPACE_OPEN,        // MAP_OBJ_FIELD_EXPIRED,

  MAP_SPACE_OPEN,        // MAP_OBJ_SIGN_LARGE_GOLD,
  MAP_SPACE_OPEN,        // MAP_OBJ_SIGN_SMALL_GOLD,
  MAP_SPACE_OPEN,        // MAP_OBJ_SIGN_LARGE_IRON,
  MAP_SPACE_OPEN,        // MAP_OBJ_SIGN_SMALL_IRON, /* 115 */
  MAP_SPACE_OPEN,        // MAP_OBJ_SIGN_LARGE_COAL,
  MAP_SPACE_OPEN,        // MAP_OBJ_SIGN_SMALL_COAL,
  MAP_SPACE_OPEN,        // MAP_OBJ_SIGN_LARGE_STONE,
  MAP_SPACE_OPEN,        // MAP_OBJ_SIGN_SMALL_STONE,

  MAP_SPACE_OPEN,        // MAP_OBJ_SIGN_EMPTY, /* 120 */

  MAP_SPACE_SEMIPASSABLE,    // MAP_OBJ_FIELD_0,
  MAP_SPACE_SEMIPASSABLE,    // MAP_OBJ_FIELD_1,
  MAP_SPACE_SEMIPASSABLE,    // MAP_OBJ_FIELD_2,
  MAP_SPACE_SEMIPASSABLE,    // MAP_OBJ_FIELD_3,
  MAP_SPACE_SEMIPASSABLE,    // MAP_OBJ_FIELD_4, /* 125 */
  MAP_SPACE_SEMIPASSABLE,    // MAP_OBJ_FIELD_5,
  MAP_SPACE_OPEN,        // MAP_OBJ_127
};

map_t::map_t() {
  tiles = NULL;
  minimap = NULL;
  spiral_pos_pattern = NULL;
}

map_t::~map_t() {
  if (tiles != NULL) {
    delete[] tiles;
    tiles = NULL;
  }

  if (minimap != NULL) {
    delete[] minimap;
    minimap = NULL;
  }

  if (spiral_pos_pattern != NULL) {
    delete[] spiral_pos_pattern;
    spiral_pos_pattern = NULL;
  }
}

void
map_t::init(unsigned int size) {
  if (tiles != NULL) {
    delete[] tiles;
    tiles = NULL;
  }

  if (minimap != NULL) {
    delete[] minimap;
    minimap = NULL;
  }

  if (spiral_pos_pattern != NULL) {
    delete[] spiral_pos_pattern;
    spiral_pos_pattern = NULL;
  }

  update_map_last_tick = 0;
  update_map_counter = 0;
  update_map_16_loop = 0;
  update_map_initial_pos = 0;

  this->size = size;

  col_size = 5 + size/2;
  row_size = 5 + (size - 1)/2;
  cols = 1 << col_size;
  rows = 1 << row_size;

  init_dimensions();

  regions = (cols >> 5) * (rows >> 5);
}

/* Return a random map position.
   Returned as map_pos_t and also as col and row if not NULL. */
map_pos_t
map_t::get_rnd_coord(int *col, int *row, random_state_t *rnd) {
  int c = rnd->random() & col_mask;
  int r = rnd->random() & row_mask;

  if (col != NULL) *col = c;
  if (row != NULL) *row = r;
  return pos(c, r);
}

/* Initialize global count of gold deposits. */
void
map_t::init_ground_gold_deposit() {
  int total_gold = 0;

  for (unsigned int y = 0; y < rows; y++) {
    for (unsigned int x = 0; x < cols; x++) {
      map_pos_t pos_ = pos(x, y);
      if (get_res_type(pos_) == GROUND_DEPOSIT_GOLD) {
        total_gold += get_res_amount(pos_);
      }
    }
  }

  gold_deposit = total_gold;
}

/* Initialize minimap data. */
void
map_t::init_minimap() {
  static const int color_offset[] = {
    0, 85, 102, 119, 17, 17, 17, 17,
    34, 34, 34, 51, 51, 51, 68, 68
  };

  static const int colors[] = {
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
    31, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
    63, 63, 62, 61, 61, 60, 59, 59, 58, 57, 57, 56, 55, 55, 54, 53, 53,
    61, 61, 60, 60, 59, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48,
    47, 47, 46, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11
  };

  if (minimap != NULL) {
    delete[] minimap;
    minimap = NULL;
  }
  minimap = new uint8_t[rows * cols];
  if (minimap == NULL) abort();

  uint8_t *mpos = minimap;
  for (unsigned int y = 0; y < rows; y++) {
    for (unsigned int x = 0; x < cols; x++) {
      map_pos_t pos_ = pos(x, y);
      int type_off = color_offset[tiles[pos_].type >> 4];

      pos_ = move_right(pos_);
      int h1 = get_height(pos_);

      pos_ = move_down_left(pos_);
      int h2 = get_height(pos_);

      int h_off = h2 - h1 + 8;
      *(mpos++) = colors[type_off + h_off];
    }
  }
}

uint8_t*
map_t::get_minimap() {
  if (minimap == NULL) {
    init_minimap();
  }

  return minimap;
}

/* Initialize spiral_pos_pattern from spiral_pattern. */
void
map_t::init_spiral_pos_pattern() {
  if (spiral_pos_pattern == NULL) {
    spiral_pos_pattern = new map_pos_t[295];
    if (spiral_pos_pattern == NULL) abort();
  }

  for (int i = 0; i < 295; i++) {
    int x = spiral_pattern[2*i] & col_mask;
    int y = spiral_pattern[2*i+1] & row_mask;

    spiral_pos_pattern[i] = pos(x, y);
  }
}

/* Set all map fields except cols/rows and col/row_size
   which must be set. */
void
map_t::init_dimensions() {
  /* Initialize global lookup tables */
  init_spiral_pattern();

  tile_count = cols * rows;

  col_mask = (1 << col_size) - 1;
  row_mask = (1 << row_size) - 1;
  row_shift = col_size;

  /* Setup direction offsets. */
  dirs[DIR_RIGHT] = 1 & col_mask;
  dirs[DIR_LEFT] = -1 & col_mask;
  dirs[DIR_DOWN] = (1 & row_mask) << row_shift;
  dirs[DIR_UP] = (-1 & row_mask) << row_shift;

  dirs[DIR_DOWN_RIGHT] = dirs[DIR_RIGHT] | dirs[DIR_DOWN];
  dirs[DIR_UP_RIGHT] = dirs[DIR_RIGHT] | dirs[DIR_UP];
  dirs[DIR_DOWN_LEFT] = dirs[DIR_LEFT] | dirs[DIR_DOWN];
  dirs[DIR_UP_LEFT] = dirs[DIR_LEFT] | dirs[DIR_UP];

  /* Allocate map */
  if (tiles != NULL) {
    delete[] tiles;
    tiles = NULL;
  }
  tiles = new map_tile_t[tile_count]();
  if (tiles == NULL) abort();

  init_spiral_pos_pattern();
}

/* Copy tile data from map generator into map tile data. */
void
map_t::init_tiles(const MapGenerator &generator) {
  for (unsigned int y = 0; y < rows; y++) {
    for (unsigned int x = 0; x < cols; x++) {
      map_pos_t pos_ = pos(x, y);
      tiles[pos_].height = generator.get_height(pos_) & 0x1f;
      tiles[pos_].type = (generator.get_type_up(pos_) & 0xf) << 4 |
        (generator.get_type_down(pos_) & 0xf);
      tiles[pos_].obj = generator.get_obj(pos_) & 0x7f;
      if (generator.get_resource_type(pos_) != GROUND_DEPOSIT_NONE) {
        tiles[pos_].resource = (generator.get_resource_type(pos_) & 7) << 5 |
          (generator.get_resource_amount(pos_) & 0x1f);
      } else {
        tiles[pos_].resource = generator.get_resource_amount(pos_);
      }
    }
  }

  init_ground_gold_deposit();
}

/* Change the height of a map position. */
void
map_t::set_height(map_pos_t pos, int height) {
  tiles[pos].height = (tiles[pos].height & 0xe0) | (height & 0x1f);

  /* Mark landscape dirty */
  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    for (change_handlers_t::iterator it = change_handlers.begin();
         it != change_handlers.end(); ++it) {
      (*it)->changed_height(move(pos, (dir_t)d));
    }
  }
}

/* Change the object at a map position. If index is non-negative
   also change this. The index should be reset to zero when a flag or
   building is removed. */
void
map_t::set_object(map_pos_t pos, map_obj_t obj, int index) {
  tiles[pos].obj = (tiles[pos].obj & 0x80) | (obj & 0x7f);
  if (index >= 0) tiles[pos].obj_index = index;

  /* Notify about object change */
  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    for (change_handlers_t::iterator it = change_handlers.begin();
         it != change_handlers.end(); ++it) {
      (*it)->changed_object(pos);
    }
  }
}

/* Remove resources from the ground at a map position. */
void
map_t::remove_ground_deposit(map_pos_t pos, int amount) {
  tiles[pos].resource -= amount;

  if (get_res_amount(pos) == 0) {
    /* Also sets the ground deposit type to none. */
    tiles[pos].resource = 0;
  }
}

/* Remove fish at a map position (must be water). */
void
map_t::remove_fish(map_pos_t pos, int amount) {
  tiles[pos].resource -= amount;
}

/* Set the index of the serf occupying map position. */
void
map_t::set_serf_index(map_pos_t pos, int index) {
  tiles[pos].serf = index;

  /* TODO Mark dirty in viewport. */
}

/* Update public parts of the map data. */
void
map_t::update_public(map_pos_t pos, random_state_t *rnd) {
  /* Update other map objects */
  int r;
  switch (get_obj(pos)) {
  case MAP_OBJ_STUB:
    if ((rnd->random() & 3) == 0) {
      set_object(pos, MAP_OBJ_NONE, -1);
    }
    break;
  case MAP_OBJ_FELLED_PINE_0: case MAP_OBJ_FELLED_PINE_1:
  case MAP_OBJ_FELLED_PINE_2: case MAP_OBJ_FELLED_PINE_3:
  case MAP_OBJ_FELLED_PINE_4:
  case MAP_OBJ_FELLED_TREE_0: case MAP_OBJ_FELLED_TREE_1:
  case MAP_OBJ_FELLED_TREE_2: case MAP_OBJ_FELLED_TREE_3:
  case MAP_OBJ_FELLED_TREE_4:
    set_object(pos, MAP_OBJ_STUB, -1);
    break;
  case MAP_OBJ_NEW_PINE:
    r = rnd->random();
    if ((r & 0x300) == 0) {
      set_object(pos, (map_obj_t)(MAP_OBJ_PINE_0 + (r & 7)), -1);
    }
    break;
  case MAP_OBJ_NEW_TREE:
    r = rnd->random();
    if ((r & 0x300) == 0) {
      set_object(pos, (map_obj_t)(MAP_OBJ_TREE_0 + (r & 7)), -1);
    }
    break;
  case MAP_OBJ_SEEDS_0: case MAP_OBJ_SEEDS_1:
  case MAP_OBJ_SEEDS_2: case MAP_OBJ_SEEDS_3:
  case MAP_OBJ_SEEDS_4:
  case MAP_OBJ_FIELD_0: case MAP_OBJ_FIELD_1:
  case MAP_OBJ_FIELD_2: case MAP_OBJ_FIELD_3:
  case MAP_OBJ_FIELD_4:
    set_object(pos, (map_obj_t)(get_obj(pos) + 1), -1);
    break;
  case MAP_OBJ_SEEDS_5:
    set_object(pos, MAP_OBJ_FIELD_0, -1);
    break;
  case MAP_OBJ_FIELD_EXPIRED:
    set_object(pos, MAP_OBJ_NONE, -1);
    break;
  case MAP_OBJ_SIGN_LARGE_GOLD: case MAP_OBJ_SIGN_SMALL_GOLD:
  case MAP_OBJ_SIGN_LARGE_IRON: case MAP_OBJ_SIGN_SMALL_IRON:
  case MAP_OBJ_SIGN_LARGE_COAL: case MAP_OBJ_SIGN_SMALL_COAL:
  case MAP_OBJ_SIGN_LARGE_STONE: case MAP_OBJ_SIGN_SMALL_STONE:
  case MAP_OBJ_SIGN_EMPTY:
    if (update_map_16_loop == 0) {
      set_object(pos, MAP_OBJ_NONE, -1);
    }
    break;
  case MAP_OBJ_FIELD_5:
    set_object(pos, MAP_OBJ_FIELD_EXPIRED, -1);
    break;
  default:
    break;
  }
}

/* Update hidden parts of the map data. */
void
map_t::update_hidden(map_pos_t pos, random_state_t *rnd) {
  /* Update fish resources in water */
  if (is_in_water(pos) &&
      tiles[pos].resource > 0) {
    int r = rnd->random();

    if (tiles[pos].resource < 10 && (r & 0x3f00)) {
      /* Spawn more fish. */
      tiles[pos].resource += 1;
    }

    /* Move in a random direction of: right, down right, left, up left */
    map_pos_t adj_pos = pos;
    switch ((r >> 2) & 3) {
    case 0: adj_pos = move_right(adj_pos); break;
    case 1: adj_pos = move_down_right(adj_pos); break;
    case 2: adj_pos = move_left(adj_pos); break;
    case 3: adj_pos = move_up_left(adj_pos); break;
    default: NOT_REACHED(); break;
    }

    if (is_in_water(adj_pos)) {
      /* Migrate a fish to adjacent water space. */
      tiles[pos].resource -= 1;
      tiles[adj_pos].resource += 1;
    }
  }
}

/* Update map data as part of the game progression. */
void
map_t::update(unsigned int tick, random_state_t *rnd) {
  uint16_t delta = tick - update_map_last_tick;
  update_map_last_tick = tick;
  update_map_counter -= delta;

  int iters = 0;
  while (update_map_counter < 0) {
    iters += regions;
    update_map_counter += 20;
  }

  map_pos_t pos = update_map_initial_pos;

  for (int i = 0; i < iters; i++) {
    update_map_16_loop -= 1;
    if (update_map_16_loop < 0) update_map_16_loop = 16;

    /* Test if moving 23 positions right crosses map boundary. */
    if (pos_col(pos) + 23 < static_cast<int>(cols)) {
      pos = move_right_n(pos, 23);
    } else {
      pos = move_right_n(pos, 23);
      pos = move_down(pos);
    }

    /* Update map at position. */
    update_hidden(pos, rnd);
    update_public(pos, rnd);
  }

  update_map_initial_pos = pos;
}

/* Return non-zero if the road segment from pos in direction dir
 can be successfully constructed at the current time. */
bool
map_t::is_road_segment_valid(map_pos_t pos, dir_t dir) {
  map_pos_t other_pos = move(pos, dir);

  map_obj_t obj = get_obj(other_pos);
  if ((paths(other_pos) != 0 && obj != MAP_OBJ_FLAG) ||
      map_t::map_space_from_obj[obj] >= MAP_SPACE_SEMIPASSABLE) {
    return false;
  }

  if (!has_owner(other_pos) ||
      get_owner(other_pos) != get_owner(pos)) {
    return false;
  }

  if (is_in_water(pos) != is_in_water(other_pos) &&
      !(has_flag(pos) || has_flag(other_pos))) {
    return false;
  }

  return true;
}

/* Actually place road segments */
bool
map_t::place_road_segments(const road_t &road) {
  map_pos_t pos_ = road.get_source();
  road_t::dirs_t dirs = road.get_dirs();
  road_t::dirs_t::const_iterator it = dirs.begin();
  for (; it != dirs.end(); ++it) {
    dir_t rev_dir = DIR_REVERSE(*it);

    if (!is_road_segment_valid(pos_, *it)) {
      /* Not valid after all. Backtrack and abort.
       This is needed to check that the road
       does not cross itself. */
      for (; it != dirs.begin();) {
        --it;
        dir_t rev_dir = *it;
        dir_t dir = DIR_REVERSE(rev_dir);

        tiles[pos_].paths &= ~BIT(dir);
        tiles[move(pos_, dir)].paths &= ~BIT(rev_dir);

        pos_ = move(pos_, dir);
      }

      return false;
    }

    tiles[pos_].paths |= BIT(*it);
    tiles[move(pos_, *it)].paths |= BIT(rev_dir);

    pos_ = move(pos_, *it);
  }

  return true;
}

bool
map_t::remove_road_backref_until_flag(map_pos_t pos_, dir_t dir) {
  while (1) {
    pos_ = move(pos_, dir);

    /* Clear backreference */
    tiles[pos_].paths &= ~BIT(DIR_REVERSE(dir));

    if (get_obj(pos_) == MAP_OBJ_FLAG) break;

    /* Find next direction of path. */
    dir = DIR_NONE;
    for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
      if (BIT_TEST(paths(pos_), d)) {
        dir = (dir_t)d;
        break;
      }
    }

    if (dir == -1) return false;
  }

  return true;
}

bool
map_t::remove_road_backrefs(map_pos_t pos_) {
  if (paths(pos_) == 0) return false;

  /* Find directions of path segments to be split. */
  dir_t path_1_dir = DIR_NONE;
  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    if (BIT_TEST(paths(pos_), d)) {
      path_1_dir = (dir_t)d;
      break;
    }
  }

  dir_t path_2_dir = DIR_NONE;
  for (int d = path_1_dir+1; d <= DIR_UP; d++) {
    if (BIT_TEST(paths(pos_), d)) {
      path_2_dir = (dir_t)d;
      break;
    }
  }

  if (path_1_dir == -1 || path_2_dir == -1) return false;

  if (!remove_road_backref_until_flag(pos_, path_1_dir)) return false;
  if (!remove_road_backref_until_flag(pos_, path_2_dir)) return false;

  return true;
}

dir_t
map_t::remove_road_segment(map_pos_t *pos, dir_t dir) {
  /* Clear forward reference. */
  tiles[*pos].paths &= ~BIT(dir);
  *pos = move(*pos, dir);

  /* Clear backreference. */
  tiles[*pos].paths &= ~BIT(DIR_REVERSE(dir));

  /* Find next direction of path. */
  dir = DIR_NONE;
  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    if (BIT_TEST(paths(*pos), d)) {
      dir = (dir_t)d;
      break;
    }
  }

  return dir;
}

bool
map_t::road_segment_in_water(map_pos_t pos_, dir_t dir) {
  if (dir > DIR_DOWN) {
    pos_ = move(pos_, dir);
    dir = DIR_REVERSE(dir);
  }

  bool water = false;

  switch (dir) {
    case DIR_RIGHT:
      if (type_down(pos_) <= MAP_TERRAIN_WATER_3 &&
          type_up(move_up(pos_)) <= MAP_TERRAIN_WATER_3) {
        water = true;
      }
      break;
    case DIR_DOWN_RIGHT:
      if (type_up(pos_) <= MAP_TERRAIN_WATER_3 &&
          type_down(pos_) <= MAP_TERRAIN_WATER_3) {
        water = true;
      }
      break;
    case DIR_DOWN:
      if (type_up(pos_) <= MAP_TERRAIN_WATER_3 &&
          type_down(move_left(pos_)) <= MAP_TERRAIN_WATER_3) {
        water = true;
      }
      break;
    default:
      NOT_REACHED();
      break;
  }

  return water;
}

void
map_t::add_change_handler(update_map_height_handler_t *handler) {
  change_handlers.push_back(handler);
}

void
map_t::del_change_handler(update_map_height_handler_t *handler) {
  change_handlers.remove(handler);
}

bool
map_t::types_within(map_pos_t pos, map_terrain_t low, map_terrain_t high) {
  if ((type_up(pos) >= low &&
       type_up(pos) <= high) &&
      (type_down(pos) >= low &&
       type_down(pos) <= high) &&
      (type_down(move_left(pos)) >= low &&
       type_down(move_left(pos)) <= high) &&
      (type_up(move_up_left(pos)) >= low &&
       type_up(move_up_left(pos)) <= high) &&
      (type_down(move_up_left(pos)) >= low &&
       type_down(move_up_left(pos)) <= high) &&
      (type_up(move_up(pos)) >= low &&
       type_up(move_up(pos)) <= high)) {
    return true;
  }

  return false;
}

save_reader_binary_t&
operator >> (save_reader_binary_t &reader, map_t &map) {
  uint8_t v8;
  uint16_t v16;

  for (unsigned int y = 0; y < map.rows; y++) {
    for (unsigned int x = 0; x < map.cols; x++) {
      map_pos_t pos = map.pos(x, y);
      reader >> v8;
      map.tiles[pos].paths = v8 & 0x3f;
      reader >> v8;
      map.tiles[pos].height = v8;
      reader >> v8;
      map.tiles[pos].type = v8;
      reader >> v8;
      map.tiles[pos].obj = v8 & 0x7f;
    }
    for (unsigned int x = 0; x < map.cols; x++) {
      map_pos_t pos = map.pos(x, y);
      if (map.get_obj(pos) >= MAP_OBJ_FLAG &&
          map.get_obj(pos) <= MAP_OBJ_CASTLE) {
        map.tiles[pos].resource = 0;
        reader >> v16;
        map.tiles[pos].obj_index = v16;
      } else {
        reader >> v8;
        map.tiles[pos].resource = v8;
        reader >> v8;
        map.tiles[pos].obj_index = 0;
      }

      reader >> v16;
      map.tiles[pos].serf = v16;
    }
  }

  return reader;
}

#define SAVE_MAP_TILE_SIZE (16)

save_reader_text_t&
operator >> (save_reader_text_t &reader, map_t &map) {
  int x = 0;
  int y = 0;
  reader.value("pos")[0] >> x;
  reader.value("pos")[1] >> y;
  map_pos_t pos = map.pos(x, y);

  for (int y = 0; y < SAVE_MAP_TILE_SIZE; y++) {
    for (int x = 0; x < SAVE_MAP_TILE_SIZE; x++) {
      map_pos_t p = map.pos_add(pos, map.pos(x, y));
      unsigned int val;

      reader.value("paths")[y*SAVE_MAP_TILE_SIZE+x] >> val;
      map.tiles[p].paths = val & 0x3f;

      reader.value("height")[y*SAVE_MAP_TILE_SIZE+x] >> val;
      map.tiles[p].height = val & 0x1f;

      reader.value("type.up")[y*SAVE_MAP_TILE_SIZE+x] >> val;
      map.tiles[p].type = ((val & 0xf) << 4) | (map.tiles[p].type & 0xf);

      reader.value("type.down")[y*SAVE_MAP_TILE_SIZE+x] >> val;
      map.tiles[p].type = (map.tiles[p].type & 0xf0) | (val & 0xf);

      reader.value("object")[y*SAVE_MAP_TILE_SIZE+x] >> val;
      map.tiles[p].obj = val & 0x7f;

      reader.value("serf")[y*SAVE_MAP_TILE_SIZE+x] >> val;
      map.tiles[p].serf = val;

      reader.value("resource.type")[y*SAVE_MAP_TILE_SIZE+x] >> val;
      map.tiles[p].resource = ((val & 7) << 5) | (map.tiles[p].resource & 0x1f);

      reader.value("resource.amount")[y*SAVE_MAP_TILE_SIZE+x] >> val;
      map.tiles[p].resource = (map.tiles[p].resource & 0xe0) | (val & 0x1f);
    }
  }

  return reader;
}

save_writer_text_t&
operator << (save_writer_text_t &writer, map_t &map) {
  int i = 0;

  for (unsigned int ty = 0; ty < map.get_rows(); ty += SAVE_MAP_TILE_SIZE) {
    for (unsigned int tx = 0; tx < map.get_cols(); tx += SAVE_MAP_TILE_SIZE) {
      save_writer_text_t &map_writer = writer.add_section("map", i++);

      map_writer.value("pos") << tx;
      map_writer.value("pos") << ty;

      for (int y = 0; y < SAVE_MAP_TILE_SIZE; y++) {
        for (int x = 0; x < SAVE_MAP_TILE_SIZE; x++) {
          map_pos_t pos = map.pos(tx+x, ty+y);

          map_writer.value("height") << map.get_height(pos);
          map_writer.value("type.up") << map.type_up(pos);
          map_writer.value("type.down") << map.type_down(pos);
          map_writer.value("paths") << map.paths(pos);
          map_writer.value("object") << map.get_obj(pos);
          map_writer.value("serf") << map.get_serf_index(pos);

          if (map.is_in_water(pos)) {
            map_writer.value("resource.type") << 0;
            map_writer.value("resource.amount") << map.get_res_fish(pos);
          } else {
            map_writer.value("resource.type") << map.get_res_type(pos);
            map_writer.value("resource.amount") << map.get_res_amount(pos);
          }
        }
      }
    }
  }

  return writer;
}

map_pos_t
road_t::get_end(map_t *map) const {
  map_pos_t result = begin;
  dirs_t::const_iterator it = dirs.begin();
  for (; it != dirs.end(); ++it) {
    result = map->move(result, *it);
  }
  return result;
}

bool
road_t::is_valid_extension(map_t *map, dir_t dir) const {
  if (is_undo(dir)) {
    return false;
  }

  /* Check that road does not cross itself. */
  map_pos_t extended_end = map->move(get_end(map), dir);
  map_pos_t pos = begin;
  bool valid = true;
  dirs_t::const_iterator it = dirs.begin();
  for (size_t i = dirs.size(); i > 0; i--) {
    pos = map->move(pos, *it);
    if (pos == extended_end) {
      valid = false;
      break;
    }
    it++;
  }

  return valid;
}

bool
road_t::is_undo(dir_t dir) const {
  return (dirs.size() > 0) && (dirs.back() == DIR_REVERSE(dir));
}

bool
road_t::extand(dir_t dir) {
  if (begin == bad_map_pos) {
    return false;
  }

  dirs.push_back(dir);

  return true;
}

bool
road_t::undo() {
  if (begin == bad_map_pos) {
    return false;
  }

  dirs.pop_back();
  if (dirs.size() == 0) {
    begin = bad_map_pos;
  }

  return true;
}
