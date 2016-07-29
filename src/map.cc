/*
 * map.cc - Map generators and map update functions
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

  max_lake_area = 14;
  water_level = 20;

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
map_t::get_rnd_coord(int *col, int *row) {
  int c = random_int() & col_mask;
  int r = random_int() & row_mask;

  if (col != NULL) *col = c;
  if (row != NULL) *row = r;
  return pos(c, r);
}

/* Midpoint displacement map generator. This function initialises the height
   values in the corners of 16x16 squares. */
void
map_t::init_heights_squares() {
  for (unsigned int y = 0; y < rows; y += 16) {
    for (unsigned int x = 0; x < cols; x += 16) {
      int rnd = random_int() & 0xff;
      tiles[pos(x, y)].height = std::min(rnd, 250);
    }
  }
}

int
map_t::calc_height_displacement(int avg, int base, int offset) {
  int r = random_int();
  int h = ((r * base) >> 16) - offset + avg;

  return std::max(0, std::min(h, 250));
}

#define TERRAIN_SPIKYNESS  0x9999

/* Calculate height values of the subdivisions in the
   midpoint displacement algorithm. */
void
map_t::init_heights_midpoints() {
  /* This is the central part of the midpoint displacement algorithm.
     The initial 16x16 squares are subdivided into 8x8 then 4x4 and so on,
     until all positions in the map have a height value.

     The random offset applied to the midpoints is based on r1 and r2.
     The offset is a random value in [-r2; r1-r2). r1 controls the roughness of
     the terrain; a larger value of r1 will result in rough terrain while a smaller
     value will generate smoother terrain.

     A high spikyness will result in sharp mountains and smooth valleys. A low
     spikyness will result in smooth mountains and sharp valleys.
  */

  int rnd = random_int();
  int r1 = 0x80 + (rnd & 0x7f);
  int r2 = (r1 * TERRAIN_SPIKYNESS) >> 16;

  for (int i = 8; i > 0; i >>= 1) {
    for (unsigned int y = 0; y < rows; y += 2*i) {
      for (unsigned int x = 0; x < cols; x += 2*i) {
        map_pos_t pos_ = pos(x, y);
        int h = tiles[pos_].height;

        map_pos_t pos_r = move_right_n(pos_, 2*i);
        map_pos_t pos_mid_r = move_right_n(pos_, i);
        int h_r = tiles[pos_r].height;

        if (preserve_bugs) {
          /* The intention was probably just to set h_r to the map height value,
             but the upper bits of rnd must be preserved in h_r in the first
             iteration to generate the same maps as the original game. */
          if (x == 0 && y == 0 && i == 8) h_r |= rnd & 0xff00;
        }

        tiles[pos_mid_r].height = calc_height_displacement((h + h_r)/2, r1, r2);

        map_pos_t pos_d = move_down_n(pos_, 2*i);
        map_pos_t pos_mid_d = move_down_n(pos_, i);
        int h_d = tiles[pos_d].height;
        tiles[pos_mid_d].height = calc_height_displacement((h+h_d)/2, r1, r2);

        map_pos_t pos_dr = move_right_n(move_down_n(pos_, 2*i), 2*i);
        map_pos_t pos_mid_dr = move_right_n(move_down_n(pos_, i), i);
        int h_dr = tiles[pos_dr].height;
        tiles[pos_mid_dr].height = calc_height_displacement((h+h_dr)/2, r1, r2);
      }
    }

    r1 >>= 1;
    r2 >>= 1;
  }
}

void
map_t::init_heights_diamond_square() {
  /* This is the central part of the diamond-square algorithm.
     The squares are first subdivided into four new squares and
     the height of the midpoint is calculated by averaging the corners and
     adding a random offset. Each "diamond" that appears is then processed
     in the same way.

     The random offset applied to the midpoints is based on r1 and r2.
     The offset is a random value in [-r2; r1-r2). r1 controls the roughness of
     the terrain; a larger value of r1 will result in rough terrain while a smaller
     value will generate smoother terrain.

     A high spikyness will result in sharp mountains and smooth valleys. A low
     spikyness will result in smooth mountains and sharp valleys.
  */

  int rnd = random_int();
  int r1 = 0x80 + (rnd & 0x7f);
  int r2 = (r1 * TERRAIN_SPIKYNESS) >> 16;

  for (int i = 8; i > 0; i >>= 1) {
    /* Diamond step */
    for (unsigned int y = 0; y < rows; y += 2*i) {
      for (unsigned int x = 0; x < cols; x += 2*i) {
        map_pos_t pos_ = pos(x, y);
        int h = tiles[pos_].height;

        map_pos_t pos_r = move_right_n(pos_, 2*i);
        int h_r = tiles[pos_r].height;

        map_pos_t pos_d = move_down_n(pos_, 2*i);
        int h_d = tiles[pos_d].height;

        map_pos_t pos_dr = move_right_n(move_down_n(pos_, 2*i), 2*i);
        int h_dr = tiles[pos_dr].height;

        map_pos_t pos_mid_dr = move_right_n(move_down_n(pos_, i), i);
        int avg = (h + h_r + h_d + h_dr) / 4;
        tiles[pos_mid_dr].height = calc_height_displacement(avg, r1, r2);
      }
    }

    /* Square step */
    for (unsigned int y = 0; y < rows; y += 2*i) {
      for (unsigned int x = 0; x < cols; x += 2*i) {
        map_pos_t pos_ = pos(x, y);
        int h = tiles[pos_].height;

        map_pos_t pos_r = move_right_n(pos_, 2*i);
        int h_r = tiles[pos_r].height;

        map_pos_t pos_d = move_down_n(pos_, 2*i);
        int h_d = tiles[pos_d].height;

        map_pos_t pos_ur = move_right_n(move_down_n(pos_, -i), i);
        int h_ur = tiles[pos_ur].height;

        map_pos_t pos_dr = move_right_n(move_down_n(pos_, i), i);
        int h_dr = tiles[pos_dr].height;

        map_pos_t pos_dl = move_right_n(move_down_n(pos_, i), -i);
        int h_dl = tiles[pos_dl].height;

        map_pos_t pos_mid_r = move_right_n(pos_, i);
        int avg_r = (h + h_r + h_ur + h_dr) / 4;
        tiles[pos_mid_r].height = calc_height_displacement(avg_r, r1, r2);

        map_pos_t pos_mid_d = move_down_n(pos_, i);
        int avg_d = (h + h_d + h_dl + h_dr) / 4;
        tiles[pos_mid_d].height = calc_height_displacement(avg_d, r1, r2);
      }
    }

    r1 >>= 1;
    r2 >>= 1;
  }
}

bool
map_t::adjust_map_height(int h1, int h2, map_pos_t pos) {
  if (abs(h1 - h2) > 32) {
    tiles[pos].height = h1 + ((h1 < h2) ? 32 : -32);
    return true;
  }

  return false;
}

/* Ensure that map heights of adjacent fields are not too far apart. */
void
map_t::clamp_heights() {
  bool changed = true;
  while (changed) {
    changed = false;
    for (unsigned int y = 0; y < rows; y++) {
      for (unsigned int x = 0; x < cols; x++) {
        map_pos_t pos_ = pos(x, y);
        int h = tiles[pos_].height;

        map_pos_t pos_d = move_down(pos_);
        int h_d = tiles[pos_d].height;
        changed |= adjust_map_height(h, h_d, pos_d);

        map_pos_t pos_dr = move_down_right(pos_);
        int h_dr = tiles[pos_dr].height;
        changed |= adjust_map_height(h, h_dr, pos_dr);

        map_pos_t pos_r = move_right(pos_);
        int h_r = tiles[pos_r].height;
        changed |= adjust_map_height(h, h_r, pos_r);
      }
    }
  }
}

int
map_t::expand_level_area(map_pos_t pos_, int limit, int r) {
  int flag = 0;

  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    map_pos_t new_pos = move(pos_, (dir_t)d);
    if (tiles[new_pos].height < 254) {
      if (tiles[new_pos].height > limit) return r;
    } else if (tiles[new_pos].height == 255) {
      flag = 1;
    }
  }

  if (flag) {
    tiles[pos_].height = 255;

    for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
      map_pos_t new_pos = move(pos_, (dir_t)d);
      if (tiles[new_pos].height != 255) tiles[new_pos].height = 254;
    }

    return 1;
  }

  return r;
}

void
map_t::init_level_area(map_pos_t pos) {
  int limit = water_level;

  if (limit >= tiles[move_right(pos)].height &&
      limit >= tiles[move_down_right(pos)].height &&
      limit >= tiles[move_down(pos)].height &&
      limit >= tiles[move_left(pos)].height &&
      limit >= tiles[move_up_left(pos)].height &&
      limit >= tiles[move_up(pos)].height) {
    tiles[pos].height = 255;
    tiles[move_right(pos)].height = 254;
    tiles[move_down_right(pos)].height = 254;
    tiles[move_down(pos)].height = 254;
    tiles[move_left(pos)].height = 254;
    tiles[move_up_left(pos)].height = 254;
    tiles[move_up(pos)].height = 254;

    for (int i = 0; i < max_lake_area; i++) {
      int flag = 0;

      map_pos_t new_pos = move_right_n(pos, i+1);
      for (int k = 0; k < 6; k++) {
        dir_t d = (dir_t)((k + DIR_DOWN) % 6);
        for (int j = 0; j <= i; j++) {
          flag = expand_level_area(new_pos, limit, flag);
          new_pos = move(new_pos, d);
        }
      }

      if (!flag) break;
    }

    if (tiles[pos].height > 253) tiles[pos].height -= 2;

    for (int i = 0; i < max_lake_area + 1; i++) {
      map_pos_t new_pos = move_right_n(pos, i+1);
      for (int k = 0; k < 6; k++) {
        dir_t d = (dir_t)((k + DIR_DOWN) % 6);
        for (int j = 0; j <= i; j++) {
          if (tiles[new_pos].height > 253) tiles[new_pos].height -= 2;
          new_pos = move(new_pos, d);
        }
      }
    }
  } else {
    tiles[pos].height = 0;
  }
}

/* Create level land that will later be filled with water.
   It is created in areas that are below a certain threshold.
   The areas are also limited in size. */
void
map_t::init_sea_level() {
  if (water_level < 0) return;

  for (int h = 0; h <= water_level; h++) {
    for (unsigned int y = 0; y < rows; y++) {
      for (unsigned int x = 0; x < cols; x++) {
        map_pos_t pos_ = pos(x, y);
        if (tiles[pos_].height == h) {
          init_level_area(pos_);
        }
      }
    }
  }

  /* Map positions are marked in the previous loop.
     0: Above water level.
     252: Land at water level.
     253: Water. */

  for (unsigned int y = 0; y < rows; y++) {
    for (unsigned int x = 0; x < cols; x++) {
      map_pos_t pos_ = pos(x, y);
      int h = tiles[pos_].height;
      switch (h) {
        case 0:
          tiles[pos_].height = water_level + 1;
          break;
        case 252:
          tiles[pos_].height = (uint8_t)water_level;
          break;
        case 253:
          tiles[pos_].height = water_level - 1;
          tiles[pos_].resource = random_int() & 7; /* Fish */
          break;
      }
    }
  }
}

/* Adjust heights so zero height is sea level. */
void
map_t::heights_rebase() {
  int h = water_level - 1;

  for (unsigned int y = 0; y < rows; y++) {
    for (unsigned int x = 0; x < cols; x++) {
      tiles[pos(x, y)].height -= h;
    }
  }
}

static map_terrain_t
calc_map_type(int h_sum) {
  if (h_sum < 3) return MAP_TERRAIN_WATER_0;
  else if (h_sum < 384) return MAP_TERRAIN_GRASS_1;
  else if (h_sum < 416) return MAP_TERRAIN_GRASS_2;
  else if (h_sum < 448) return MAP_TERRAIN_TUNDRA_0;
  else if (h_sum < 480) return MAP_TERRAIN_TUNDRA_1;
  else if (h_sum < 528) return MAP_TERRAIN_TUNDRA_2;
  else if (h_sum < 560) return MAP_TERRAIN_SNOW_0;
  return MAP_TERRAIN_SNOW_1;
}

/* Set type of map fields based on the height value. */
void
map_t::init_types() {
  for (unsigned int y = 0; y < rows; y++) {
    for (unsigned int x = 0; x < cols; x++) {
      map_pos_t pos_ = pos(x, y);
      int h1 = tiles[pos_].height;
      int h2 = tiles[move_right(pos_)].height;
      int h3 = tiles[move_down_right(pos_)].height;
      int h4 = tiles[move_down(pos_)].height;
      tiles[pos_].type = (calc_map_type(h1 + h3 + h4) << 4) |
                          calc_map_type(h1 + h2 + h3);
    }
  }
}

void
map_t::init_types_2_sub() {
  for (unsigned int y = 0; y < rows; y++) {
    for (unsigned int x = 0; x < cols; x++) {
      tiles[pos(x, y)].obj = 0;
    }
  }
}

void
map_t::init_types_2() {
  init_types_2_sub();

  for (unsigned int y = 0; y < rows; y++) {
    for (unsigned int x = 0; x < cols; x++) {
      map_pos_t pos_ = pos(x, y);

      if (tiles[pos_].height > 0) {
        tiles[pos_].obj = 1;

        unsigned int num = 0;
        int changed = 1;
        while (changed) {
          changed = 0;
          for (unsigned int y = 0; y < rows; y++) {
            for (unsigned int x = 0; x < cols; x++) {
              map_pos_t pos_ = pos(x, y);

              if (tiles[pos_].obj == 1) {
                num += 1;
                tiles[pos_].obj = 2;

                int flags = 0;
                if (type_down(pos_) >= MAP_TERRAIN_GRASS_0) {
                  flags |= 3;
                }
                if (type_up(pos_) >= MAP_TERRAIN_GRASS_0) {
                  flags |= 6;
                }
                if (type_down(move_left(pos_)) >= MAP_TERRAIN_GRASS_0) {
                  flags |= 0xc;
                }
                if (type_up(move_up_left(pos_)) >= MAP_TERRAIN_GRASS_0) {
                  flags |= 0x18;
                }
                if (type_down(move_up_left(pos_)) >= MAP_TERRAIN_GRASS_0) {
                  flags |= 0x30;
                }
                if (type_up(move_up(pos_)) >= MAP_TERRAIN_GRASS_0) {
                  flags |= 0x21;
                }

                for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
                  if (BIT_TEST(flags, d)) {
                    if (tiles[move(pos_, (dir_t)d)].obj == 0) {
                      tiles[move(pos_, (dir_t)d)].obj = 1;
                      changed = 1;
                    }
                  }
                }
              }
            }
          }
        }

        if (4*num >= tile_count) goto break_loop;
      }
    }
  }

  break_loop:

  for (unsigned int y = 0; y < rows; y++) {
    for (unsigned int x = 0; x < cols; x++) {
      map_pos_t pos_ = pos(x, y);

      if (tiles[pos_].height > 0 && tiles[pos_].obj == 0) {
        tiles[pos_].height = 0;
        tiles[pos_].type = 0;

        tiles[move_left(pos_)].type &= 0xf0;
        tiles[move_up_left(pos_)].type = 0;
        tiles[move_up(pos_)].type &= 0xf;
      }
    }
  }

  init_types_2_sub();
}

/* Rescale height values to be in [0;31]. */
void
map_t::heights_rescale() {
  for (unsigned int y = 0; y < rows; y++) {
    for (unsigned int x = 0; x < cols; x++) {
      map_pos_t pos_ = pos(x, y);
      tiles[pos_].height = (tiles[pos_].height + 6) >> 3;
    }
  }
}

void
map_t::init_types_shared_sub(map_terrain_t old, map_terrain_t seed,
                             map_terrain_t new_) {
  for (unsigned int y = 0; y < rows; y++) {
    for (unsigned int x = 0; x < cols; x++) {
      map_pos_t pos_ = pos(x, y);

      if (type_up(pos_) == old &&
          (seed == type_down(move_up_left(pos_)) ||
           seed == type_up(move_up_left(pos_)) ||
           seed == type_up(move_up(pos_)) ||
           seed == type_down(move_left(pos_)) ||
           seed == type_up(move_left(pos_)) ||
           seed == type_down(pos_) ||
           seed == type_up(move_right(pos_)) ||
           seed == type_down(move_down_left(pos_)) ||
           seed == type_down(move_down(pos_)) ||
           seed == type_up(move_down(pos_)) ||
           seed == type_down(move_down_right(pos_)) ||
           seed == type_up(move_down_right(pos_)))) {
        tiles[pos_].type = (new_ << 4) | (tiles[pos_].type & 0xf);
      }

      if (type_down(pos_) == old &&
          (seed == type_down(move_up_left(pos_)) ||
           seed == type_up(move_up_left(pos_)) ||
           seed == type_down(move_up(pos_)) ||
           seed == type_up(move_up(pos_)) ||
           seed == type_up(move_up_right(pos_)) ||
           seed == type_down(move_left(pos_)) ||
           seed == type_up(pos_) ||
           seed == type_down(move_right(pos_)) ||
           seed == type_up(move_right(pos_)) ||
           seed == type_down(move_down(pos_)) ||
           seed == type_down(move_down_right(pos_)) ||
           seed == type_up(move_down_right(pos_)))) {
        tiles[pos_].type = (tiles[pos_].type & 0xf0) | new_;
      }
    }
  }
}

void
map_t::init_lakes() {
  init_types_shared_sub(
    MAP_TERRAIN_WATER_0, MAP_TERRAIN_GRASS_1, MAP_TERRAIN_WATER_3);
  init_types_shared_sub(
    MAP_TERRAIN_WATER_0, MAP_TERRAIN_WATER_3, MAP_TERRAIN_WATER_2);
  init_types_shared_sub(
    MAP_TERRAIN_WATER_0, MAP_TERRAIN_WATER_2, MAP_TERRAIN_WATER_1);
}

void
map_t::init_types4() {
  init_types_shared_sub(
    MAP_TERRAIN_GRASS_1, MAP_TERRAIN_WATER_3, MAP_TERRAIN_GRASS_0);
}

/* Use spiral pattern to lookup a new position based on col, row. */
map_pos_t
map_t::lookup_pattern(int col, int row, int index) {
  return pos_add(pos(col, row), spiral_pos_pattern[index]);
}

int
map_t::init_desert_sub1(map_pos_t pos_) {
  map_terrain_t type_d = type_down(pos_);
  map_terrain_t type_u = type_up(pos_);

  if (type_d != MAP_TERRAIN_GRASS_1 && type_d != MAP_TERRAIN_DESERT_2) {
    return -1;
  }
  if (type_u != MAP_TERRAIN_GRASS_1 && type_u != MAP_TERRAIN_DESERT_2) {
    return -1;
  }

  type_d = type_down(move_left(pos_));
  if (type_d != MAP_TERRAIN_GRASS_1 && type_d != MAP_TERRAIN_DESERT_2) {
    return -1;
  }

  type_d = type_down(move_down(pos_));
  if (type_d != MAP_TERRAIN_GRASS_1 && type_d != MAP_TERRAIN_DESERT_2) {
    return -1;
  }

  return 0;
}

int
map_t::init_desert_sub2(map_pos_t pos_) {
  map_terrain_t type_d = type_down(pos_);
  map_terrain_t type_u = type_up(pos_);

  if (type_d != MAP_TERRAIN_GRASS_1 && type_d != MAP_TERRAIN_DESERT_2) {
    return -1;
  }
  if (type_u != MAP_TERRAIN_GRASS_1 && type_u != MAP_TERRAIN_DESERT_2) {
    return -1;
  }

  type_u = type_up(move_right(pos_));
  if (type_u != MAP_TERRAIN_GRASS_1 && type_u != MAP_TERRAIN_DESERT_2) {
    return -1;
  }

  type_u = type_up(move_up(pos_));
  if (type_u != MAP_TERRAIN_GRASS_1 && type_u != MAP_TERRAIN_DESERT_2) {
    return -1;
  }

  return 0;
}

/* Create deserts on the map. */
void
map_t::init_desert() {
  for (int i = 0; i < regions; i++) {
    for (int try_ = 0; try_ < 200; try_++) {
      int col, row;
      map_pos_t rnd_pos = get_rnd_coord(&col, &row);

      if (type_up(rnd_pos) == MAP_TERRAIN_GRASS_1 &&
          type_down(rnd_pos) == MAP_TERRAIN_GRASS_1) {
        for (int index = 255; index >= 0; index--) {
          map_pos_t pos = lookup_pattern(col, row, index);

          int r = init_desert_sub1(pos);
          if (r == 0) {
            tiles[pos].type =
              (MAP_TERRAIN_DESERT_2 << 4) | (tiles[pos].type & 0xf);
          }

          r = init_desert_sub2(pos);
          if (r == 0) {
            tiles[pos].type = (tiles[pos].type & 0xf0) | MAP_TERRAIN_DESERT_2;
          }
        }
        break;
      }
    }
  }
}

void
map_t::init_desert_2_sub() {
  for (unsigned int y = 0; y < rows; y++) {
    for (unsigned int x = 0; x < cols; x++) {
      map_pos_t pos_ = pos(x, y);
      map_terrain_t type_d = type_down(pos_);
      map_terrain_t type_u = type_up(pos_);

      if (type_d >= MAP_TERRAIN_GRASS_3 && type_d <= MAP_TERRAIN_DESERT_1) {
        type_d = MAP_TERRAIN_GRASS_1;
      }
      if (type_u >= MAP_TERRAIN_GRASS_3 && type_u <= MAP_TERRAIN_DESERT_1) {
        type_u = MAP_TERRAIN_GRASS_1;
      }

      tiles[pos_].type = (type_u << 4) | type_d;
    }
  }
}

void
map_t::init_desert_2() {
  init_types_shared_sub(
    MAP_TERRAIN_DESERT_2, MAP_TERRAIN_GRASS_1, MAP_TERRAIN_GRASS_3);
  init_types_shared_sub(
    MAP_TERRAIN_DESERT_2, MAP_TERRAIN_GRASS_3, MAP_TERRAIN_DESERT_0);
  init_types_shared_sub(
    MAP_TERRAIN_DESERT_2, MAP_TERRAIN_DESERT_0, MAP_TERRAIN_DESERT_1);

  init_desert_2_sub();

  init_types_shared_sub(
    MAP_TERRAIN_GRASS_1, MAP_TERRAIN_DESERT_2, MAP_TERRAIN_DESERT_1);
  init_types_shared_sub(
    MAP_TERRAIN_GRASS_1, MAP_TERRAIN_DESERT_1, MAP_TERRAIN_DESERT_0);
  init_types_shared_sub(
    MAP_TERRAIN_GRASS_1, MAP_TERRAIN_DESERT_0, MAP_TERRAIN_GRASS_3);
}

/* Put crosses on top of mountains. */
void
map_t::init_crosses() {
  for (unsigned int y = 0; y < rows; y++) {
    for (unsigned int x = 0; x < cols; x++) {
      map_pos_t pos_ = pos(x, y);
      unsigned int h = get_height(pos_);
      if (h >= 26 &&
          h >= get_height(move_right(pos_)) &&
          h >= get_height(move_down_right(pos_)) &&
          h >= get_height(move_down(pos_)) &&
          h > get_height(move_left(pos_)) &&
          h > get_height(move_up_left(pos_)) &&
          h > get_height(move_up(pos_))) {
        tiles[pos_].obj = MAP_OBJ_CROSS;
      }
    }
  }
}

/* Check that hexagon has tile types in range.

   Check whether the hexagon at pos has triangles of types between min and max,
   both inclusive. Return false if not all triangles are in this range,
   otherwise true.

   NOTE: This function has a quirk which is enabled by preserve_bugs. When this
   quirk is enabled, one of the tiles that is checked is not in the hexagon but
   is instead an adjacent tile. This is necessary to generate original game
   maps. */
bool
map_t::hexagon_types_in_range(
    map_pos_t pos_, map_terrain_t min, map_terrain_t max) {
  map_terrain_t type_d = type_down(pos_);
  map_terrain_t type_u = type_up(pos_);

  if (type_d < min || type_d > max) return false;
  if (type_u < min || type_u > max) return false;

  type_d = type_down(move_left(pos_));
  if (type_d < min || type_d > max) return false;

  type_d = type_down(move_up_left(pos_));
  type_u = type_up(move_up_left(pos_));
  if (type_d < min || type_d > max) return false;
  if (type_u < min || type_u > max) return false;

  /* Should be checkeing the up tri type. */
  if (preserve_bugs) {
    type_d = type_down(move_up(pos_));
    if (type_d < min || type_d > max) return false;
  } else {
    type_u = type_up(move_up(pos_));
    if (type_u < min || type_u > max) return false;
  }

  return true;
}

/* Get a random position in the spiral pattern based at col, row. */
map_pos_t
map_t::lookup_rnd_pattern(int col, int row, int mask) {
  return lookup_pattern(col, row, random_int() & mask);
}

/* Create clusters of map objects.

   Tries to create num_clusters of objects in random locations on the map.
   Each cluster has up to objs_in_cluster objects. The pos_mask is used in
   the call to lookup_rnd_pattern to determine the max cluster size. The
   type_min and type_max determine the range (both inclusive) of terrain
   types that must appear around a position to be elegible for placement of
   an object. The obj_base determines the first object type that can be placed
   and the obj_mask specifies a mask on a random integer that is added to the
   base to obtain the final object type.
*/
void
map_t::init_objects_shared(int num_clusters, int objs_in_cluster, int pos_mask,
                           map_terrain_t type_min, map_terrain_t type_max,
                           int obj_base, int obj_mask) {
  for (int i = 0; i < num_clusters; i++) {
    for (int try_ = 0; try_ < 100; try_++) {
      int col, row;
      map_pos_t rnd_pos = get_rnd_coord(&col, &row);
      if (hexagon_types_in_range(rnd_pos, type_min, type_max)) {
        for (int j = 0; j < objs_in_cluster; j++) {
          map_pos_t pos_ = lookup_rnd_pattern(col, row, pos_mask);
          if (hexagon_types_in_range(pos_, type_min, type_max) &&
              get_obj(pos_) == MAP_OBJ_NONE) {
            tiles[pos_].obj = (random_int() & obj_mask) + obj_base;
          }
        }
        break;
      }
    }
  }
}

void
map_t::init_trees_1() {
  /* Add either tree or pine. */
  init_objects_shared(
    regions << 3, 10, 0xff, MAP_TERRAIN_GRASS_1, MAP_TERRAIN_GRASS_2,
    MAP_OBJ_TREE_0, 0xf);
}

void
map_t::init_trees_2() {
  /* Add only trees. */
  init_objects_shared(
    regions, 45, 0x3f, MAP_TERRAIN_GRASS_1, MAP_TERRAIN_GRASS_2,
    MAP_OBJ_TREE_0, 0x7);
}

void
map_t::init_trees_3() {
  /* Add only pines. */
  init_objects_shared(
    regions, 30, 0x3f, MAP_TERRAIN_GRASS_0, MAP_TERRAIN_GRASS_2,
    MAP_OBJ_PINE_0, 0x7);
}

void
map_t::init_trees_4() {
  /* Add either tree or pine. */
  init_objects_shared(
    regions, 20, 0x7f, MAP_TERRAIN_GRASS_1, MAP_TERRAIN_GRASS_2,
    MAP_OBJ_TREE_0, 0xf);
}

void
map_t::init_stone_1() {
  init_objects_shared(
    regions, 40, 0x3f, MAP_TERRAIN_GRASS_1, MAP_TERRAIN_GRASS_2,
    MAP_OBJ_STONE_0, 0x7);
}

void
map_t::init_stone_2() {
  init_objects_shared(
    regions, 15, 0xff, MAP_TERRAIN_GRASS_1, MAP_TERRAIN_GRASS_2,
    MAP_OBJ_STONE_0, 0x7);
}

void
map_t::init_dead_trees() {
  init_objects_shared(
    regions, 2, 0xff, MAP_TERRAIN_GRASS_1, MAP_TERRAIN_GRASS_2,
    MAP_OBJ_DEAD_TREE, 0);
}

void
map_t::init_large_boulders() {
  init_objects_shared(
    regions, 6, 0xff, MAP_TERRAIN_GRASS_1, MAP_TERRAIN_GRASS_2,
    MAP_OBJ_SANDSTONE_0, 0x1);
}

void
map_t::init_water_trees() {
  init_objects_shared(
    regions, 50, 0x7f, MAP_TERRAIN_WATER_2, MAP_TERRAIN_WATER_3,
    MAP_OBJ_WATER_TREE_0, 0x3);
}

void
map_t::init_stubs() {
  init_objects_shared(
    regions, 5, 0xff, MAP_TERRAIN_GRASS_1, MAP_TERRAIN_GRASS_2,
    MAP_OBJ_STUB, 0);
}

void
map_t::init_small_boulders() {
  init_objects_shared(
    regions, 10, 0xff, MAP_TERRAIN_GRASS_1, MAP_TERRAIN_GRASS_2,
    MAP_OBJ_STONE, 0x1);
}

void
map_t::init_cadavers() {
  init_objects_shared(
    regions, 2, 0xf, MAP_TERRAIN_DESERT_2, MAP_TERRAIN_DESERT_2,
    MAP_OBJ_CADAVER_0, 0x1);
}

void
map_t::init_cacti() {
  init_objects_shared(
    regions, 6, 0x7f, MAP_TERRAIN_DESERT_0, MAP_TERRAIN_DESERT_2,
    MAP_OBJ_CACTUS_0, 0x1);
}

void
map_t::init_water_stones() {
  init_objects_shared(
    regions, 8, 0x7f, MAP_TERRAIN_WATER_0, MAP_TERRAIN_WATER_2,
    MAP_OBJ_WATER_STONE_0, 0x1);
}

void
map_t::init_palms() {
  init_objects_shared(
    regions, 6, 0x3f, MAP_TERRAIN_DESERT_2, MAP_TERRAIN_DESERT_2,
    MAP_OBJ_PALM_0, 0x3);
}

void
map_t::init_resources_shared_sub(int iters, int col, int row, int *index,
                                 int amount, ground_deposit_t type) {
  for (int i = 0; i < iters; i++) {
    map_pos_t pos = lookup_pattern(col, row, *index);
    *index += 1;

    int res = tiles[pos].resource;
    if (res == 0 || (res & 0x1f) < amount) {
      tiles[pos].resource = (type << 5) + amount;
    }
  }
}

/* Create clusters of ground resources.

   Tries to create num_clusters of ground resources of the given type. The
   terrain type around a position must be in the min, max range
   (both inclusive) for a resource to be deposited.
*/
void
map_t::init_resources_shared(int num_clusters, ground_deposit_t type,
                             map_terrain_t min, map_terrain_t max) {
  for (int i = 0; i < num_clusters; i++) {
    for (int try_ = 0; try_ < 100; try_++) {
      int col, row;
      map_pos_t pos = get_rnd_coord(&col, &row);

      if (hexagon_types_in_range(pos, min, max)) {
        int index = 0;
        int amount = 8 + (random_int() & 0xc);
        init_resources_shared_sub(1, col, row, &index, amount, type);
        amount -= 4;
        if (amount == 0) break;

        init_resources_shared_sub(6, col, row, &index, amount, type);
        amount -= 4;
        if (amount == 0) break;

        init_resources_shared_sub(12, col, row, &index, amount, type);
        amount -= 4;
        if (amount == 0) break;

        init_resources_shared_sub(18, col, row, &index, amount, type);
        amount -= 4;
        if (amount == 0) break;

        init_resources_shared_sub(24, col, row, &index, amount, type);
        amount -= 4;
        if (amount == 0) break;

        init_resources_shared_sub(30, col, row, &index, amount, type);
        break;
      }
    }
  }
}

/* Initialize resources in the ground. */
void
map_t::init_resources() {
  init_resources_shared(
    regions * 9, GROUND_DEPOSIT_COAL,
    MAP_TERRAIN_TUNDRA_0, MAP_TERRAIN_SNOW_0);
  init_resources_shared(
    regions * 4, GROUND_DEPOSIT_IRON,
    MAP_TERRAIN_TUNDRA_0, MAP_TERRAIN_SNOW_0);
  init_resources_shared(
    regions * 2, GROUND_DEPOSIT_GOLD,
    MAP_TERRAIN_TUNDRA_0, MAP_TERRAIN_SNOW_0);
  init_resources_shared(
    regions * 2, GROUND_DEPOSIT_STONE,
    MAP_TERRAIN_TUNDRA_0, MAP_TERRAIN_SNOW_0);
}

void
map_t::init_clean_up() {
  /* Make sure that it is always possible to walk around
     any impassable objects. This also clears water obstacles
     except in certain positions near the shore. */
  for (unsigned int y = 0; y < rows; y++) {
    for (unsigned int x = 0; x < cols; x++) {
      map_pos_t pos_ = pos(x, y);
      if (map_t::map_space_from_obj[get_obj(pos_)] >=
          MAP_SPACE_IMPASSABLE) {
        for (int d = DIR_LEFT; d <= DIR_UP; d++) {
          map_pos_t other_pos = move(pos_, (dir_t)d);
          map_space_t s = map_t::map_space_from_obj[get_obj(other_pos)];
          if (is_in_water(other_pos) || s >= MAP_SPACE_IMPASSABLE) {
            tiles[pos_].obj &= 0x80;
            break;
          }
        }
      }
    }
  }
}

void
map_t::init_sub() {
  init_lakes();
  init_types4();
  init_desert();
  init_desert_2();
  init_crosses();
  init_trees_1();
  init_trees_2();
  init_trees_3();
  init_trees_4();
  init_stone_1();
  init_stone_2();
  init_dead_trees();
  init_large_boulders();
  init_water_trees();
  init_stubs();
  init_small_boulders();
  init_cadavers();
  init_cacti();
  init_water_stones();
  init_palms();
  init_resources();
  init_clean_up();
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
  tiles = new map_tile_t[tile_count];
  memset(tiles, 0, sizeof(map_tile_t) * tile_count);
  if (tiles == NULL) abort();

  init_spiral_pos_pattern();
}

void
map_t::generate(int generator, const random_state_t &rnd, bool preserve_bugs) {
  this->rnd = rnd;
  this->rnd ^= random_state_t(0x5a5a, 0xa5a5, 0xc3c3);
  this->preserve_bugs = preserve_bugs;

  random_int();
  random_int();

  init_heights_squares();
  switch (generator) {
    case 0:
      init_heights_midpoints(); /* Midpoint displacement algorithm */
      break;
    case 1:
      init_heights_diamond_square(); /* Diamond square algorithm */
      break;
    default:
      NOT_REACHED();
  }

  clamp_heights();
  init_sea_level();
  heights_rebase();
  init_types();
  init_types_2();
  heights_rescale();
  init_sub();
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
map_t::update_public(map_pos_t pos) {
  /* Update other map objects */
  int r;
  switch (get_obj(pos)) {
  case MAP_OBJ_STUB:
    if ((random_int() & 3) == 0) {
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
    r = random_int();
    if ((r & 0x300) == 0) {
      set_object(pos, (map_obj_t)(MAP_OBJ_PINE_0 + (r & 7)), -1);
    }
    break;
  case MAP_OBJ_NEW_TREE:
    r = random_int();
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
map_t::update_hidden(map_pos_t pos) {
  /* Update fish resources in water */
  if (is_in_water(pos) &&
      tiles[pos].resource > 0) {
    int r = random_int();

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
map_t::update(unsigned int tick) {
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
    update_hidden(pos);
    update_public(pos);
  }

  update_map_initial_pos = pos;
}

uint16_t
map_t::random_int() {
  return rnd.random();
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
