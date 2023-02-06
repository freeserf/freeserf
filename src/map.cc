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
   identified by an integer col and row (commonly encoded as MapPos).

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

#include <algorithm>
#include <utility>

#include "src/debug.h"
#include "src/savegame.h"
#include "src/map-generator.h"
#include "src/map-geometry.h"
#include "src/game-options.h"

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

  /*
  // dump the pattern to help understand it
  Log::Debug["map"] << "spiral pattern: ";
  Log::Debug["map"] << "0, 0";
  for (int x = 0; x < 49; x++) {
    std::string row;
    for (int y = 0; y < 12; y++) {
      int val = spiral_pattern[2 + (12 * x) + y];
      row.append(std::to_string(val) + ",");
    }
    Log::Debug["map"] << row;
  }
  exit(1);
  */

  /* here it is:
  Debug: [map] spiral pattern:
  Debug: [map] 0, 0
  Debug: [map] 1,0,1,1,0,1,-1,0,-1,-1,0,-1,
  Debug: [map] 2,1,1,2,-1,1,-2,-1,-1,-2,1,-1,
  Debug: [map] 2,0,2,2,0,2,-2,0,-2,-2,0,-2,
  Debug: [map] 3,1,2,3,-1,2,-3,-1,-2,-3,1,-2,
  Debug: [map] 3,2,1,3,-2,1,-3,-2,-1,-3,2,-1,
  Debug: [map] 3,0,3,3,0,3,-3,0,-3,-3,0,-3,
  Debug: [map] 4,2,2,4,-2,2,-4,-2,-2,-4,2,-2,
  Debug: [map] 4,1,3,4,-1,3,-4,-1,-3,-4,1,-3,
  Debug: [map] 4,3,1,4,-3,1,-4,-3,-1,-4,3,-1,
  Debug: [map] 4,0,4,4,0,4,-4,0,-4,-4,0,-4,
  Debug: [map] 5,2,3,5,-2,3,-5,-2,-3,-5,2,-3,
  Debug: [map] 5,3,2,5,-3,2,-5,-3,-2,-5,3,-2,
  Debug: [map] 5,1,4,5,-1,4,-5,-1,-4,-5,1,-4,
  Debug: [map] 5,4,1,5,-4,1,-5,-4,-1,-5,4,-1,
  Debug: [map] 5,0,5,5,0,5,-5,0,-5,-5,0,-5,
  Debug: [map] 6,3,3,6,-3,3,-6,-3,-3,-6,3,-3,
  Debug: [map] 6,2,4,6,-2,4,-6,-2,-4,-6,2,-4,
  Debug: [map] 6,4,2,6,-4,2,-6,-4,-2,-6,4,-2,
  Debug: [map] 6,1,5,6,-1,5,-6,-1,-5,-6,1,-5,
  Debug: [map] 6,5,1,6,-5,1,-6,-5,-1,-6,5,-1,
  Debug: [map] 6,0,6,6,0,6,-6,0,-6,-6,0,-6,
  Debug: [map] 7,3,4,7,-3,4,-7,-3,-4,-7,3,-4,
  Debug: [map] 7,4,3,7,-4,3,-7,-4,-3,-7,4,-3,
  Debug: [map] 7,2,5,7,-2,5,-7,-2,-5,-7,2,-5,
  Debug: [map] 7,5,2,7,-5,2,-7,-5,-2,-7,5,-2,
  Debug: [map] 7,1,6,7,-1,6,-7,-1,-6,-7,1,-6,
  Debug: [map] 7,6,1,7,-6,1,-7,-6,-1,-7,6,-1,
  Debug: [map] 7,0,7,7,0,7,-7,0,-7,-7,0,-7,
  Debug: [map] 8,4,4,8,-4,4,-8,-4,-4,-8,4,-4,
  Debug: [map] 8,3,5,8,-3,5,-8,-3,-5,-8,3,-5,
  Debug: [map] 8,5,3,8,-5,3,-8,-5,-3,-8,5,-3,
  Debug: [map] 8,2,6,8,-2,6,-8,-2,-6,-8,2,-6,
  Debug: [map] 8,6,2,8,-6,2,-8,-6,-2,-8,6,-2,
  Debug: [map] 8,1,7,8,-1,7,-8,-1,-7,-8,1,-7,
  Debug: [map] 8,7,1,8,-7,1,-8,-7,-1,-8,7,-1,
  Debug: [map] 8,0,8,8,0,8,-8,0,-8,-8,0,-8,
  Debug: [map] 9,4,5,9,-4,5,-9,-4,-5,-9,4,-5,
  Debug: [map] 9,5,4,9,-5,4,-9,-5,-4,-9,5,-4,
  Debug: [map] 9,3,6,9,-3,6,-9,-3,-6,-9,3,-6,
  Debug: [map] 9,6,3,9,-6,3,-9,-6,-3,-9,6,-3,
  Debug: [map] 9,2,7,9,-2,7,-9,-2,-7,-9,2,-7,
  Debug: [map] 9,7,2,9,-7,2,-9,-7,-2,-9,7,-2,
  Debug: [map] 9,1,8,9,-1,8,-9,-1,-8,-9,1,-8,
  Debug: [map] 9,0,9,9,0,9,-9,0,-9,-9,0,-9,
  Debug: [map] 16,0,16,16,0,16,-16,0,-16,-16,0,-16,
  Debug: [map] 16,8,8,16,-8,8,-16,-8,-8,-16,8,-8,
  Debug: [map] 24,0,24,24,0,24,-24,0,-24,-24,0,-24,
  Debug: [map] 24,8,16,24,-8,16,-24,-8,-16,-24,8,-16,
  Debug: [map] 24,16,8,24,-16,8,-24,-16,-8,-24,16,-8,
  */
}

int *
Map::get_spiral_pattern() {
  return spiral_pattern;
}


// extended spiral uses alternate method, I don't understand how the first 2 cols of the
//  original method were determined, so cannot extend it
std::vector<int> extended_spiral_coord_vector;
//
// NOTE that if the spiral_dist radius/shells is increased further, you must figure out how many
//   mappos are found by checking the extended_spiral_coord_vector.size() and setting the
//   extended_spiral_pattern[] int array to that size (maybe plus 1?)
// you can NOT simply double/times-X increase it, it increases exponentially because the shells 
//  larger the further out you go  24 shells was 3268 pos, 48 shells is 13445 pos 
//
static int extended_spiral_pattern[13445] = { 0 };  
static int extended_spiral_pattern_initialized = 0;
static int directional_fill_pattern[629] = { 0 };
static int directional_fill_pattern_initialized = 0;

static void
init_extended_spiral_pattern() {
  if (extended_spiral_pattern_initialized) {
    return;
  }
  int x = 0;
  int y = 0;
  Map::next_extended_spiral_coord(x, y, &extended_spiral_coord_vector); // add the center pos coords
  for (int shells = 1; shells < 48; ++shells) {
    for (int i = 0; i < shells; ++i) { Map::next_extended_spiral_coord(++x, y, &extended_spiral_coord_vector); } // RIGHT
    for (int i = 0; i < shells - 1; ++i) { Map::next_extended_spiral_coord(x, --y, &extended_spiral_coord_vector); } // DOWN
    for (int i = 0; i < shells; ++i) { Map::next_extended_spiral_coord(--x, --y, &extended_spiral_coord_vector); } // DOWN-LEFT
    for (int i = 0; i < shells; ++i) { Map::next_extended_spiral_coord(--x, y, &extended_spiral_coord_vector); } // LEFT
    for (int i = 0; i < shells; ++i) { Map::next_extended_spiral_coord(x, ++y, &extended_spiral_coord_vector); } // UP
    for (int i = 0; i < shells; ++i) { Map::next_extended_spiral_coord(++x, ++y, &extended_spiral_coord_vector); } // UP-RIGHT
  }
  //Log::Debug["map.cc"] << "inside init_extended_spiral_pattern, extended_spiral_coord_vector.size() is " << extended_spiral_coord_vector.size();
  std::copy(extended_spiral_coord_vector.begin(), extended_spiral_coord_vector.end(), extended_spiral_pattern);
  extended_spiral_pattern_initialized = 1;
  /*
  Log::Debug["map"] << "new extended spiral pattern: ";
  Log::Debug["map"] << "0, 0";
  for (int x = 0; x < 272; x++) {
    std::string row;
    for (int y = 0; y < 12; y++) {
      int val = extended_spiral_pattern[2 + (12 * x) + y];
      row.append(std::to_string(val) + ",");
    }
    Log::Debug["map"] << row;
  }
  exit(1);
  */

  // best explanation of original spiral_pattern method...
  //  an example, for this row #49
  //
  // 24, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  // i = 49  # row id
  // x = 24  # 1st hardcoded value
  // y = 16  # 2nd hardcoded value
  // if j=0...
  //  12*i = 588
  //  2 + 588 + 0     (2*0 = 0 = j)
  //  or, 1st hardcoded value of 49th row
  //     is overwritten to the value 24*(pos/neg/zero?) + 16*(pos/neg/zero?)
  //  and 2nd hardcoded value of 49th row
  //     is overwritten to the value 24*(pos/neg/zero?) + 16*(pos/neg/zero?) also, but with different -1/1/0
  // then as j=1,2,3..., the next pairs of values in same row are set to same pattern,
  //     only change is the -1/1/0 spiral_matrix entry used
}

static void
init_directional_fill_pattern() {
  if (directional_fill_pattern_initialized) {
    return;
  }
  // I generated this pattern inside an AI loop using the marked as "used to generate Map::init_directional_fill_pattern"
   std::vector<int> pregenerated_pattern = { -1,-4,0,-3,1,-2,2,-1,3,0,3,1,3,2,3,3,3,4,-2,-4,-1,-3,0,-2,1,-1,2,0,2,1,2,2,2,3,2,4,-3,-4,-2,-3,-1,-2,0,-1,1,0,1,1,1,2,1,3,1,4,-4,-4,-3,-3,-2,-2,-1,-1,0,0,0,1,0,2,0,3,0,4,-4,-3,-3,-2,-2,-1,-1,0,-1,1,-1,2,-1,3,-4,-2,-3,-1,-2,0,-2,1,-2,2,-4,-1,-3,0,-3,1,-4,0,
3,-1,3,0,3,1,3,2,3,3,2,3,1,3,0,3,-1,3,2,-2,2,-1,2,0,2,1,2,2,1,2,0,2,-1,2,-2,2,1,-3,1,-2,1,-1,1,0,1,1,0,1,-1,1,-2,1,-3,1,0,-4,0,-3,0,-2,0,-1,0,0,-1,0,-2,0,-3,0,-4,0,-1,-4,-1,-3,-1,-2,-1,-1,-2,-1,-3,-1,-4,-1,-2,-4,-2,-3,-2,-2,-3,-2,-4,-2,-3,-4,-3,-3,-4,-3,-4,-4,
4,3,3,3,2,3,1,3,0,3,-1,2,-2,1,-3,0,-4,-1,4,2,3,2,2,2,1,2,0,2,-1,1,-2,0,-3,-1,-4,-2,4,1,3,1,2,1,1,1,0,1,-1,0,-2,-1,-3,-2,-4,-3,4,0,3,0,2,0,1,0,0,0,-1,-1,-2,-2,-3,-3,-4,-4,3,-1,2,-1,1,-1,0,-1,-1,-2,-2,-3,-3,-4,2,-2,1,-2,0,-2,-1,-3,-2,-4,1,-3,0,-3,-1,-4,0,-4, 
1,4,0,3,-1,2,-2,1,-3,0,-3,-1,-3,-2,-3,-3,-3,-4,2,4,1,3,0,2,-1,1,-2,0,-2,-1,-2,-2,-2,-3,-2,-4,3,4,2,3,1,2,0,1,-1,0,-1,-1,-1,-2,-1,-3,-1,-4,4,4,3,3,2,2,1,1,0,0,0,-1,0,-2,0,-3,0,-4,4,3,3,2,2,1,1,0,1,-1,1,-2,1,-3,4,2,3,1,2,0,2,-1,2,-2,4,1,3,0,3,-1,4,0,
-3,-3,-2,-3,-1,-3,0,-3,1,-3,-3,1,-3,0,-3,-1,-3,-2,-2,-2,-1,-2,0,-2,1,-2,2,-2,-2,2,-2,1,-2,0,-2,-1,-1,-1,0,-1,1,-1,2,-1,3,-1,-1,3,-1,2,-1,1,-1,0,0,0,1,0,2,0,3,0,4,0,0,4,0,3,0,2,0,1,1,1,2,1,3,1,4,1,1,4,1,3,1,2,2,2,3,2,4,2,2,4,2,3,3,3,4,3,3,4,4,4,
-4,-3,-3,-3,-2,-3,-1,-3,0,-3,1,-2,2,-1,3,0,4,1,-4,-2,-3,-2,-2,-2,-1,-2,0,-2,1,-1,2,0,3,1,4,2,-4,-1,-3,-1,-2,-1,-1,-1,0,-1,1,0,2,1,3,2,4,3,-4,0,-3,0,-2,0,-1,0,0,0,1,1,2,2,3,3,4,4,-3,1,-2,1,-1,1,0,1,1,2,2,3,3,4,-2,2,-1,2,0,2,1,3,2,4,-1,3,0,3,1,4,0,4 };
// orig, starts at Dir 2 instead of 0
//  std::vector<int> pregenerated_pattern = { 1,4,0,3,-1,2,-2,1,-3,0,-3,-1,-3,-2,-3,-3,-3,-4,2,4,1,3,0,2,-1,1,-2,0,-2,-1,-2,-2,-2,-3,-2,-4,3,4,2,3,1,2,0,1,-1,0,-1,-1,-1,-2,-1,-3,-1,-4,4,4,3,3,2,2,1,1,0,0,0,-1,0,-2,0,-3,0,-4,4,3,3,2,2,1,1,0,1,-1,1,-2,1,-3,4,2,3,1,2,0,2,-1,2,-2,4,1,3,0,3,-1,4,0,
//-3,-3,-2,-3,-1,-3,0,-3,1,-3,-3,1,-3,0,-3,-1,-3,-2,-2,-2,-1,-2,0,-2,1,-2,2,-2,-2,2,-2,1,-2,0,-2,-1,-1,-1,0,-1,1,-1,2,-1,3,-1,-1,3,-1,2,-1,1,-1,0,0,0,1,0,2,0,3,0,4,0,0,4,0,3,0,2,0,1,1,1,2,1,3,1,4,1,1,4,1,3,1,2,2,2,3,2,4,2,2,4,2,3,3,3,4,3,3,4,4,4,
//-4,-3,-3,-3,-2,-3,-1,-3,0,-3,1,-2,2,-1,3,0,4,1,-4,-2,-3,-2,-2,-2,-1,-2,0,-2,1,-1,2,0,3,1,4,2,-4,-1,-3,-1,-2,-1,-1,-1,0,-1,1,0,2,1,3,2,4,3,-4,0,-3,0,-2,0,-1,0,0,0,1,1,2,2,3,3,4,4,-3,1,-2,1,-1,1,0,1,1,2,2,3,3,4,-2,2,-1,2,0,2,1,3,2,4,-1,3,0,3,1,4,0,4,
//-1,-4,0,-3,1,-2,2,-1,3,0,3,1,3,2,3,3,3,4,-2,-4,-1,-3,0,-2,1,-1,2,0,2,1,2,2,2,3,2,4,-3,-4,-2,-3,-1,-2,0,-1,1,0,1,1,1,2,1,3,1,4,-4,-4,-3,-3,-2,-2,-1,-1,0,0,0,1,0,2,0,3,0,4,-4,-3,-3,-2,-2,-1,-1,0,-1,1,-1,2,-1,3,-4,-2,-3,-1,-2,0,-2,1,-2,2,-4,-1,-3,0,-3,1,-4,0,
//3,-1,3,0,3,1,3,2,3,3,2,3,1,3,0,3,-1,3,2,-2,2,-1,2,0,2,1,2,2,1,2,0,2,-1,2,-2,2,1,-3,1,-2,1,-1,1,0,1,1,0,1,-1,1,-2,1,-3,1,0,-4,0,-3,0,-2,0,-1,0,0,-1,0,-2,0,-3,0,-4,0,-1,-4,-1,-3,-1,-2,-1,-1,-2,-1,-3,-1,-4,-1,-2,-4,-2,-3,-2,-2,-3,-2,-4,-2,-3,-4,-3,-3,-4,-3,-4,-4,
//4,3,3,3,2,3,1,3,0,3,-1,2,-2,1,-3,0,-4,-1,4,2,3,2,2,2,1,2,0,2,-1,1,-2,0,-3,-1,-4,-2,4,1,3,1,2,1,1,1,0,1,-1,0,-2,-1,-3,-2,-4,-3,4,0,3,0,2,0,1,0,0,0,-1,-1,-2,-2,-3,-3,-4,-4,3,-1,2,-1,1,-1,0,-1,-1,-2,-2,-3,-3,-4,2,-2,1,-2,0,-2,-1,-3,-2,-4,1,-3,0,-3,-1,-4,0,-4};
  std::copy(pregenerated_pattern.begin(), pregenerated_pattern.end(), directional_fill_pattern);
  directional_fill_pattern_initialized = 1;
}
  
int *
Map::get_extended_spiral_pattern() {
  return extended_spiral_pattern;
}

int *
Map::get_directional_fill_pattern() {
  return directional_fill_pattern;
}


/* Map Object to Space. */
const Map::Space
Map::map_space_from_obj[] = {
  SpaceOpen,        // ObjectNone = 0,
  SpaceFilled,      // ObjectFlag,
  SpaceImpassable,    // ObjectSmallBuilding,
  SpaceImpassable,    // ObjectLargeBuilding,
  SpaceImpassable,    // ObjectCastle,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,

  SpaceFilled,      // ObjectTree0 = 8,
  SpaceFilled,      // ObjectTree1,
  SpaceFilled,      // ObjectTree2, /* 10 */
  SpaceFilled,      // ObjectTree3,
  SpaceFilled,      // ObjectTree4,
  SpaceFilled,      // ObjectTree5,
  SpaceFilled,      // ObjectTree6,
  SpaceFilled,      // ObjectTree7, /* 15 */

  SpaceFilled,      // ObjectPine0,
  SpaceFilled,      // ObjectPine1,
  SpaceFilled,      // ObjectPine2,
  SpaceFilled,      // ObjectPine3,
  SpaceFilled,      // ObjectPine4, /* 20 */
  SpaceFilled,      // ObjectPine5,
  SpaceFilled,      // ObjectPine6,
  SpaceFilled,      // ObjectPine7,

  SpaceFilled,      // ObjectPalm0,
  SpaceFilled,      // ObjectPalm1, /* 25 */
  SpaceFilled,      // ObjectPalm2,
  SpaceFilled,      // ObjectPalm3,

  SpaceImpassable,    // ObjectWaterTree0,
  SpaceImpassable,    // ObjectWaterTree1,
  SpaceImpassable,    // ObjectWaterTree2, /* 30 */
  SpaceImpassable,    // ObjectWaterTree3,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,
  SpaceOpen,

  SpaceImpassable,    // ObjectStone0 = 72,
  SpaceImpassable,    // ObjectStone1,
  SpaceImpassable,    // ObjectStone2,
  SpaceImpassable,    // ObjectStone3, /* 75 */
  SpaceImpassable,    // ObjectStone4,
  SpaceImpassable,    // ObjectStone5,
  SpaceImpassable,    // ObjectStone6,
  SpaceImpassable,    // ObjectStone7,

  SpaceImpassable,    // ObjectSandstone0, /* 80 */
  SpaceImpassable,    // ObjectSandstone1,

  SpaceFilled,      // ObjectCross,
  SpaceOpen,        // ObjectStub,

  SpaceOpen,        // ObjectStone,
  SpaceOpen,        // ObjectSandstone3, /* 85 */

  SpaceOpen,        // ObjectCadaver0,
  SpaceOpen,        // ObjectCadaver1,

  SpaceImpassable,    // ObjectWaterStone0,
  SpaceImpassable,    // ObjectWaterStone1,

  SpaceFilled,      // ObjectCactus0, /* 90 */
  SpaceFilled,      // ObjectCactus1,

  SpaceFilled,      // ObjectDeadTree,

  SpaceFilled,      // ObjectFelledPine0,
  SpaceFilled,      // ObjectFelledPine1,
  SpaceFilled,      // ObjectFelledPine2, /* 95 */
  SpaceFilled,      // ObjectFelledPine3,
  SpaceOpen,        // ObjectFelledPine4,

  SpaceFilled,      // ObjectFelledTree0,
  SpaceFilled,      // ObjectFelledTree1,
  SpaceFilled,      // ObjectFelledTree2, /* 100 */
  SpaceFilled,      // ObjectFelledTree3,
  SpaceOpen,        // ObjectFelledTree4,

  SpaceFilled,      // ObjectNewPine,
  SpaceFilled,      // ObjectNewTree,

  SpaceSemipassable,    // ObjectSeeds0, /* 105 */
  SpaceSemipassable,    // ObjectSeeds1,
  SpaceSemipassable,    // ObjectSeeds2,
  SpaceSemipassable,    // ObjectSeeds3,
  SpaceSemipassable,    // ObjectSeeds4,
  SpaceSemipassable,    // ObjectSeeds5, /* 110 */
  SpaceOpen,        // ObjectFieldExpired,

  SpaceOpen,        // ObjectSignLargeGold,
  SpaceOpen,        // ObjectSignSmallGold,
  SpaceOpen,        // ObjectSignLargeIron,
  SpaceOpen,        // ObjectSignSmallIron, /* 115 */
  SpaceOpen,        // ObjectSignLargeCoal,
  SpaceOpen,        // ObjectSignSmallCoal,
  SpaceOpen,        // ObjectSignLargeStone,
  SpaceOpen,        // ObjectSignSmallStone,

  SpaceOpen,        // ObjectSignEmpty, /* 120 */

  SpaceSemipassable,    // ObjectField0,
  SpaceSemipassable,    // ObjectField1,
  SpaceSemipassable,    // ObjectField2,
  SpaceSemipassable,    // ObjectField3,
  SpaceSemipassable,    // ObjectField4, /* 125 */
  SpaceSemipassable,    // ObjectField5,
  SpaceOpen,        // Object127

  SpaceOpen,        // Flower0
  SpaceOpen,        // Flower1
  SpaceOpen,        // Flower2
  SpaceOpen,  //ObjectFlowerGroupA3,  //     to the full 193 sprites (object 185) which would allow adding new objects 186+ without running into issues with objects that 
  SpaceOpen,  //ObjectFlowerGroupA4,  //     cannot be "resolved" in enum (which might not even matter at all?)
  SpaceOpen,  //ObjectFlowerGroupA5,  // 133, sprite 125
  SpaceOpen,  //ObjectFlowerGroupA6,  // 134, sprite 126
  SpaceOpen,  //ObjectFlowerGroupB0,  // 135, sprite 127
  SpaceOpen,  //ObjectFlowerGroupB1,  // 136, sprite 128 <-- start of Flag sprites!
  SpaceOpen,  //ObjectFlowerGroupB2,  // 137, sprite 129
  SpaceOpen,  //ObjectFlowerGroupB3,  // 138, sprite 130
  SpaceOpen,  //ObjectFlowerGroupB4,  // 139, sprite 131
  SpaceOpen,  //ObjectFlowerGroupB5,  // 140, sprite 132
  SpaceOpen,  //ObjectFlowerGroupB6,  // 141, sprite 133
  SpaceOpen,  //ObjectFlowerGroupC0,  // 142, sprite 134
  SpaceOpen,  //ObjectFlowerGroupC1,  // 143, sprite 135
  SpaceOpen,  //ObjectFlowerGroupC2,  // 144, sprite 136
  SpaceOpen,  //ObjectFlowerGroupC3,  // 145, sprite 137
  SpaceOpen,  //ObjectFlowerGroupC4,  // 146, sprite 138
  SpaceOpen,  //ObjectFlowerGroupC5,  // 147, sprite 139
  SpaceOpen,  //ObjectFlowerGroupC6,  // 148, sprite 140
  SpaceOpen,  //ObjectCattail0,       // 149, sprite 141
  SpaceOpen,  //ObjectCattail1,       // 150, sprite 142
  SpaceFilled,   // 151, sprite 143  (NewTree0+ don't actually have new sprites)
  SpaceFilled,  //ObjectNewTree1,
  SpaceFilled,  //ObjectNewTree2,
  SpaceFilled,  //ObjectNewTree3,
  SpaceFilled,  //ObjectNewTree4,   // these are to support option_ForesterMonoculture, deciduous trees are now born with a type
  SpaceFilled,  //ObjectNewTree5,   //  the original 'ObjectNewTree' CANNOT serve as ObjectNewTree0 because it must result in
  SpaceFilled,  //ObjectNewTree6,   //  maturing into a random subtype for when this option is not enabled to maintain orig behavior
  SpaceFilled,  //ObjectNewTree7,   // 158, sprite 150
  SpaceImpassable,  //ObjectNewWaterStone0,  // 159, sprite 151    these values don't even matter for water objects because it is flat
  SpaceImpassable,  //ObjectNewWaterStone1,  
  SpaceImpassable,  //ObjectNewWaterStone2,  
  SpaceImpassable,  //ObjectNewWaterStone3,  
  SpaceImpassable,  //ObjectNewWaterStone4,  
  SpaceImpassable,  //ObjectNewWaterStone5,  
  SpaceImpassable,  //ObjectNewWaterStone6,  
  SpaceImpassable,  //ObjectNewWaterStone7,  // 166, sprite 158
};


// used for drawing shaded objects on downward slopes (left->right)
const uint8_t
Map::obj_height_for_slope_darken[] = {
  0,  //ObjectNone = 0,         
  2,   //ObjectFlag,
  5,   //ObjectSmallBuilding,    
  5,   //ObjectLargeBuilding,    
  9,   //ObjectCastle,  
  0,  // nothing at all here // 5
  0,  // nothing at all here
  0,  // nothing at all here // 7
  4,   //ObjectTree0 = 8,
  4,   //ObjectTree1,
  4,   //ObjectTree2, /* 10 */
  4,   //ObjectTree3,
  4,   //ObjectTree4,
  4,   //ObjectTree5,
  4,   //ObjectTree6,
  4,   //ObjectTree7, /* 15 */
  4,   //ObjectPine0,
  4,   //ObjectPine1,
  4,   //ObjectPine2,
  4,   //ObjectPine3,
  4,   //ObjectPine4, /* 20 */
  4,   //ObjectPine5,
  4,   //ObjectPine6,
  4,   //ObjectPine7,
  4,   //ObjectPalm0,
  4,   //ObjectPalm1, /* 25 */
  4,   //ObjectPalm2,
  4,   //ObjectPalm3,
  3,   //ObjectWaterTree0,
  3,   //ObjectWaterTree1,
  3,   //ObjectWaterTree2, /* 30 */
  3,   //ObjectWaterTree3, // 31
  0,  // nothing at all here //32
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here // 35
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here // 40
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here // 45
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here // 50
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here // 55
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here // 60
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here // 65
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here
  0,  // nothing at all here // 70
  0,  // nothing at all here //71
  4,   //ObjectStone0 = 72,
  4,   //ObjectStone1,
  4,   //ObjectStone2,
  4,   //ObjectStone3, /* 75 */
  3,   //ObjectStone4,
  3,   //ObjectStone5,
  3,   //ObjectStone6,
  3,   //ObjectStone7,
  5,   //ObjectSandstone0, /* 80 */
  2,   //ObjectSandstone1,
  2,   //ObjectCross,
  1,  //ObjectStub,
  0,  //ObjectStone,
  1,  //ObjectSandstone3, /* 85 */
  0,  //ObjectCadaver0,
  0,  //ObjectCadaver1,
  2,  //ObjectWaterStone0,
  2,  //ObjectWaterStone1,
  3,   //ObjectCactus0, /* 90 */
  3,   //ObjectCactus1,
  3,   //ObjectDeadTree,
  4,   //ObjectFelledPine0,
  2,   //ObjectFelledPine1,
  2,   //ObjectFelledPine2, /* 95 */
  1,   //ObjectFelledPine3,
  1,   //ObjectFelledPine4,
  4,   //ObjectFelledTree0,
  3,   //ObjectFelledTree1,
  3,   //ObjectFelledTree2, /* 100 */
  1,   //ObjectFelledTree3,
  1,   //ObjectFelledTree4,
  1,  //ObjectNewPine,
  1,  //ObjectNewTree,
  0,  //ObjectSeeds0, /* 105 */
  0,  //ObjectSeeds1,
  0,  //ObjectSeeds2,
  1,  //ObjectSeeds3,
  2,  //ObjectSeeds4,
  2,  //ObjectSeeds5, /* 110 */
  0,  //ObjectFieldExpired,
  2,  //ObjectSignLargeGold,
  2,  //ObjectSignSmallGold,
  2,  //ObjectSignLargeIron,
  2,  //ObjectSignSmallIron, /* 115 */
  2,  //ObjectSignLargeCoal,
  2,  //ObjectSignSmallCoal,
  2,  //ObjectSignLargeStone,
  2,  //ObjectSignSmallStone,
  2,  //ObjectSignEmpty, /* 120 */
  2,  //ObjectField0, // 121, sprite 113   // NOTE there is no visual difference between Field0-5, they all contain a copy of the same sprite
  2,  //ObjectField1,                      //   the reason there are six types is to easily track the progression from Seeds to Field to FieldExpired
  2,  //ObjectField2,                      //   fully inside the Map object (and outside the Game object) 
  2,  //ObjectField3,
  2,  //ObjectField4, 
  2,  //ObjectField5, // 126, sprite 118
  0,  //Object127,    // what is this?  I am thinking it is a spillover value so that a Field5 can still be advanced and will be set to FieldExpired if detected?
  0,  //ObjectFlowerGroupA0,  // 128, sprite 120
  0,  //ObjectFlowerGroupA1,  //   IMPORTANT - because sprite 128 (corresponding to undefined MapObject 136) is the start of Flag sprites
  0,  //ObjectFlowerGroupA2,  //     there are only 8 empty sprite indexes that could be used for new objects!  Unless, this enum could be extended all the way out
  0,  //ObjectFlowerGroupA3,  //     to the full 193 sprites (object 185) which would allow adding new objects 186+ without running into issues with objects that 
  0,  //ObjectFlowerGroupA4,  //     cannot be "resolved" in enum (which might not even matter at all?)
  0,  //ObjectFlowerGroupA5,  // 133, sprite 125
  0,  //ObjectFlowerGroupA6,  // 134, sprite 126
  0,  //ObjectFlowerGroupB0,  // 135, sprite 127
  0,  //ObjectFlowerGroupB1,  // 136, sprite 128 <-- start of Flag sprites!
  0,  //ObjectFlowerGroupB2,  // 137, sprite 129
  1,  //ObjectFlowerGroupB3,  // 138, sprite 130
  1,  //ObjectFlowerGroupB4,  // 139, sprite 131
  1,  //ObjectFlowerGroupB5,  // 140, sprite 132
  1,  //ObjectFlowerGroupB6,  // 141, sprite 133
  0,  //ObjectFlowerGroupC0,  // 142, sprite 134
  0,  //ObjectFlowerGroupC1,  // 143, sprite 135
  0,  //ObjectFlowerGroupC2,  // 144, sprite 136
  0,  //ObjectFlowerGroupC3,  // 145, sprite 137
  0,  //ObjectFlowerGroupC4,  // 146, sprite 138
  0,  //ObjectFlowerGroupC5,  // 147, sprite 139
  0,  //ObjectFlowerGroupC6,  // 148, sprite 140
  1,  //ObjectCattail0,       // 149, sprite 141
  1,  //ObjectCattail1,       // 150, sprite 142
  1,  //ObjectNewTree0,   // 151, sprite 143  (NewTree0+ don't actually have new sprites)
  1,  //ObjectNewTree1,
  1,  //ObjectNewTree2,
  1,  //ObjectNewTree3,
  1,  //ObjectNewTree4,   // these are to support option_ForesterMonoculture, deciduous trees are now born with a type
  1,  //ObjectNewTree5,   //  the original 'ObjectNewTree' CANNOT serve as ObjectNewTree0 because it must result in
  1,  //ObjectNewTree6,   //  maturing into a random subtype for when this option is not enabled to maintain orig behavior
  1,  //ObjectNewTree7,   // 158, sprite 150
  3,  //ObjectNewWaterStone0,  // 159, sprite 151    these values don't even matter for water objects because it is flat
  3,  //ObjectNewWaterStone1,  
  3,  //ObjectNewWaterStone2,  
  3,  //ObjectNewWaterStone3,  
  3,  //ObjectNewWaterStone4,  
  3,  //ObjectNewWaterStone5,  
  3,  //ObjectNewWaterStone6,  
  3,  //ObjectNewWaterStone7,  // 166, sprite 158
};


Map::Map(const MapGeometry& geom)
  : geom_(geom)
  , spiral_pos_pattern(new MapPos[295])
  // NOTE that if the spiral_dist radius/shells is increased further, you must figure out how many
  //   mappos are found by checking the extended_spiral_coord_vector.size() and setting the
  //   extended_spiral_pattern[] int array to that size (maybe plus 1?)
  // you can NOT simply double/times-X increase it, it increases exponentially because the shells 
  //  larger the further out you go  24 shells was 3268 pos, 48 shells is 13445 pos
  , extended_spiral_pos_pattern(new MapPos[13445])
  , directional_fill_pos_pattern(new MapPos[313]) {
  // Some code may still assume that map has at least size 3.
  if (geom.size() < 3) {
    throw ExceptionFreeserf("Failed to create map with size less than 3.");
  }

  landscape_tiles.resize(geom_.tile_count());
  game_tiles.resize(geom_.tile_count());

  update_state.last_tick = 0;
  update_state.counter = 0;
  update_state.remove_signs_counter = 0;
  update_state.initial_pos = 0;

  regions = (geom.cols() >> 5) * (geom.rows() >> 5);

  init_spiral_pattern();
  init_extended_spiral_pattern();
  init_directional_fill_pattern();

  init_spiral_pos_pattern();
  init_extended_spiral_pos_pattern();
  init_directional_fill_pos_pattern();
}

/* Return a random map position.
   Returned as map_pos_t and also as col and row if not NULL. */
MapPos
Map::get_rnd_coord(int *col, int *row, Random *rnd) const {
  int c = rnd->random() & geom_.col_mask();
  int r = rnd->random() & geom_.row_mask();

  if (col != NULL) *col = c;
  if (row != NULL) *row = r;
  return pos(c, r);
}

// Return a truly random map position, NOT the default one that only changes once per second
//   Returned as map_pos_t and also as col and row if not NULL. */
// WAIT THIS SUCKS TOO, use newer c++11 functions
MapPos
Map::get_better_rnd_coord() const {
  int foo = rand();
  int c = rand() & geom_.col_mask();
  int r = rand() & geom_.row_mask();
  Log::Debug["map"] << "inside get_better_rnd_coord, rand map pos is " << pos(c, r) << ", foo = " << foo;
  return pos(c, r);
}

// Get count of gold mineral deposits in the map.
unsigned int
Map::get_gold_deposit() const {
  int count = 0;

  for (MapPos pos_ : geom_) {
    if (get_res_type(pos_) == MineralsGold) {
      count += get_res_amount(pos_);
    }
  }

  return count;
}

/* Initialize spiral_pos_pattern from spiral_pattern. */
void
Map::init_spiral_pos_pattern() {
  for (int i = 0; i < 295; i++) {
    int x = spiral_pattern[2*i] & geom_.col_mask();
    int y = spiral_pattern[2*i+1] & geom_.row_mask();

    spiral_pos_pattern[i] = pos(x, y);
  }
}

/* Initialize extended spiral_pos_pattern for AI functions */
void
Map::init_extended_spiral_pos_pattern() {
    // NOTE, originally had this set to <1801 but g++ threw warning:
//[build] map.cc:522:40: warning: iteration 1634 invokes undefined behavior [-Waggressive-loop-optimizations]
//[build]    int x = extended_spiral_pattern[2 * i] & geom_.col_mask();
//[build]            ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~^
//[build] map.cc:521:20: note: within this loop
//[build]   for (int i = 0; i < 1801; i++) {
//[build]                   ~~^~~~~~
//  going to try changing it to 1633 to avoid... this might shrink the possible spiral
//   *OR* it might actually fix a bug

//  for (int i = 0; i < 1801; i++) {
// NOTE that if the spiral_dist radius/shells is increased further, you must figure out how many
//   mappos are found by checking the extended_spiral_coord_vector.size() and setting the
//   extended_spiral_pattern[] int array to that size (maybe plus 1?)
// you can NOT simply double/times-X increase it, it increases exponentially because the shells 
//  larger the further out you go  24 shells was 3268 pos, 48 shells is 13445 pos
    for (int i = 0; i < 6720; i++) {
    int x = extended_spiral_pattern[2 * i] & geom_.col_mask();
    int y = extended_spiral_pattern[2 * i + 1] & geom_.row_mask();

    extended_spiral_pos_pattern[i] = pos(x, y);
  }
}

/* Initialize outwards-first fill/search pattern for AI functions */
//  see https://github.com/tlongstretch/freeserf-with-AI-plus/issues/58
void
Map::init_directional_fill_pos_pattern() {
  // for each Direction (+1 because *0 = 0)
  for (int dir = 0; dir < 6; dir++){
    for (int i = 0; i < 52; i++) {
      int dir_pattern_offset = 104 * dir;
      int dir_pos_offset = 52 * dir;
      int x = directional_fill_pattern[dir_pattern_offset + i * 2 ] & geom_.col_mask();
      int y = directional_fill_pattern[dir_pattern_offset + i * 2 + 1] & geom_.row_mask();
      directional_fill_pos_pattern[dir_pos_offset + i] = pos(x, y);
    }
  }
}

/* Copy tile data from map generator into map tile data. */
void
Map::init_tiles(const MapGenerator &generator) {
  landscape_tiles = generator.get_landscape();
}

/* Change the height of a map position. */
void
Map::set_height(MapPos pos, int height) {
  landscape_tiles[pos].height = height;

  /* Mark landscape dirty */
  for (Direction d : cycle_directions_cw()) {
    for (Handler *handler : change_handlers) {
      handler->on_height_changed(move(pos, d));
    }
  }
}

// tlongstretch - hack to work around crash bug during castle
//   placement at game start, https://github.com/tlongstretch/freeserf-with-AI-plus/issues/38
void
Map::set_height_no_refresh(MapPos pos, int height) {
  landscape_tiles[pos].height = height;
  // don't Mark landscape dirty, I guess it will be updated on next refresh?
  //  not sure, it might not even matter.  It seems to work fine
}

/* Change the object at a map position. If index is non-negative
   also change this. The index should be reset to zero when a flag or
   building is removed. */
void
Map::set_object(MapPos pos, Object obj, int index) {
  landscape_tiles[pos].obj = obj;
  if (index >= 0) game_tiles[pos].obj_index = index;
  Log::Debug["map"] << "inside set_object, setting pos " << pos << " to object " << obj << ", index " << index;

  /* Notify about object change */
  for (Direction d : cycle_directions_cw()) {
    for (Handler *handler : change_handlers) {
      handler->on_object_changed(move(pos, d));
    }
  }
}

/* Remove resources from the ground at a map position. */
void
Map::remove_ground_deposit(MapPos pos, int amount) {
  landscape_tiles[pos].resource_amount -= amount;

  if (landscape_tiles[pos].resource_amount <= 0) {
    /* Also sets the ground deposit type to none. */
    landscape_tiles[pos].mineral = MineralsNone;
  }
}

/* Remove fish at a map position (must be water). */
void
Map::remove_fish(MapPos pos, int amount) {
  landscape_tiles[pos].resource_amount -= amount;
}

/* Set the index of the serf occupying map position. */
void
Map::set_serf_index(MapPos pos, int index) {
  game_tiles[pos].serf = index;

  /* TODO Mark dirty in viewport. */
}

/* Update public parts of the map data. */
// any individual pos is only updated about every 20k ticks
// every 20 ticks a new pos is updated (i.e. this function is called)
void
Map::update_public(MapPos pos, Random *rnd) {

  /* Update other map objects */
  int r;
  switch (get_obj(pos)) {
  case ObjectStub:
    if ((rnd->random() & 3) == 0) {
      set_object(pos, ObjectNone, -1);
    }
    break;
  case ObjectFelledPine0: case ObjectFelledPine1:
  case ObjectFelledPine2: case ObjectFelledPine3:
  case ObjectFelledPine4:
  case ObjectFelledTree0: case ObjectFelledTree1:
  case ObjectFelledTree2: case ObjectFelledTree3:
  case ObjectFelledTree4:
    set_object(pos, ObjectStub, -1);
    break;
  case ObjectNewPine:
    // new trees randomly grow into mature trees
    r = rnd->random();
    if ((r & 0x300) == 0) {
      if (option_BabyTreesMatureSlowly) {
        // if set, it is half as likely for a tree to grow up
        //  regardless if it was planted or spontaneously grew from TreesReproduce feature
        if ((r % 2 == 0)){ 
          //Log::Debug["map"] << "inside Map::update_public, BabyTreesMatureSlowly feature on, r = " << r << ", NOT maturing this Pine at pos " << pos;
          break;
        }
        //Log::Debug["map"] << "inside Map::update_public, BabyTreesMatureSlowly feature on, r = " << r << ", maturing this Pine at pos " << pos;
      }
      // I believe this r & 7 sets a random "animation-state" subtype of Pine0 through Pine7
      set_object(pos, (Object)(ObjectPine0 + (r & 7)), -1);
    }
    break;
  case ObjectNewTree:
  case ObjectNewTree1:
  case ObjectNewTree2:
  case ObjectNewTree3:
  case ObjectNewTree4:
  case ObjectNewTree5:
  case ObjectNewTree6:
  case ObjectNewTree7:
    // new trees randomly grow into mature trees
    r = rnd->random();
    if ((r & 0x300) == 0) {
      if (option_BabyTreesMatureSlowly) {
        // if set, it is half as likely for a tree to grow up
        //  regardless if it was planted or spontaneously grew from TreesReproduce feature
        if ((r % 2 == 0)){ 
          //Log::Debug["map"] << "inside Map::update_public, BabyTreesMatureSlowly feature on, r = " << r << ", NOT maturing this Tree at pos " << pos;
          break;
        }
        //Log::Debug["map"] << "inside Map::update_public, BabyTreesMatureSlowly feature on, r = " << r << ", maturing this Tree at pos " << pos;
      }

      if (get_obj(pos) >= ObjectNewTree0 && get_obj(pos) <= ObjectNewTree7){
        // option_ForesterMonoculture
        //  mature into the matching adult type
        set_object(pos, (Object)(ObjectTree0 + (get_obj(pos) - ObjectNewTree0)), -1);
      }else{
        // original behavior, mature into random subtype
        // I believe this r & 7 sets a random "animation-state" subtype of Tree0 through Tree7
        set_object(pos, (Object)(ObjectTree0 + (r & 7)), -1);
      }
    }
    break;
  // at 20k ticks per update, it should take 200k ticks
  // for a freshly sown field Seed0 to mature and expire
  // at FieldExpired
  // in real life, spring wheat takes about 1.5 seasons to mature
  // and so about 1.6 seasons from planting to harvest or 125k ticks
  // I have adjusted the season length for FourSeasons to be 125k ticks
  // I am thinking for AdvancedFarming that only simple rules are needed:
  // - Seeds do not progress in winter, but they survive and resume in spring
  // - Fields die by mid-fall even if not harvested, from cold
  // - very young Seeds0-1 die in summer from heat
  // Farmer logic changes required:
  // - Farmer should not sow in late spring nor summer, nor winter
  // - Farmer should only sow in early-mid spring, and anytime in fall
  // - Harvested fields are destroyed immediately?
  case ObjectSeeds0: case ObjectSeeds1:
    if (option_AdvancedFarming && season == 1){
      //Log::Debug["map"] << "option_AdvancedFarming on, and it is Summer, this young Seed-field at pos " << pos << " is now being destroyed (too hot for immature seedlings)";
      set_object(pos, ObjectFieldExpired, -1);
    }else if (option_AdvancedFarming && season == 3){
      //Log::Debug["map"] << "option_AdvancedFarming on, and it is Winter, this Seed-field at pos " << pos << " is not advancing this update (too cold for seeds to grow)";
    }else{
      set_object(pos, (Object)(get_obj(pos) + 1), -1);
    }
    break;
  case ObjectSeeds2: case ObjectSeeds3:
  case ObjectSeeds4:
    if (option_AdvancedFarming && season == 3){
      //Log::Debug["map"] << "option_AdvancedFarming on, and it is Winter, this Seed-field at pos " << pos << " is not advancing this update (too cold for seeds to grow)";
    }else{
      set_object(pos, (Object)(get_obj(pos) + 1), -1);
    }
    break;
  case ObjectField0: case ObjectField1:
  case ObjectField2: case ObjectField3:
  case ObjectField4:
    if (option_AdvancedFarming && (season >= 3 || (season >=2 && subseason >= 8))){
      //Log::Debug["map"] << "option_AdvancedFarming on, and it is past mid-Fall, this Field at pos " << pos << " is now being destroyed";
      set_object(pos, ObjectFieldExpired, -1);
    }else{
      set_object(pos, (Object)(get_obj(pos) + 1), -1);
    }
    break;
  case ObjectSeeds5:
    if (option_AdvancedFarming && (season >= 3 || (season >=2 && subseason >= 8))){
      //Log::Debug["map"] << "option_AdvancedFarming on, and it is past mid-Fall, this Seeds5-field at pos " << pos << " is now being destroyed instead of progressing to Field0";
      set_object(pos, ObjectFieldExpired, -1);
    }else{
      set_object(pos, ObjectField0, -1);
    }
    break;
  case ObjectFieldExpired:
    set_object(pos, ObjectNone, -1);
    break;
  case ObjectSignLargeGold: case ObjectSignSmallGold:
  case ObjectSignLargeIron: case ObjectSignSmallIron:
  case ObjectSignLargeCoal: case ObjectSignSmallCoal:
  case ObjectSignLargeStone: case ObjectSignSmallStone:
  case ObjectSignEmpty:
    if (update_state.remove_signs_counter == 0) {
      set_object(pos, ObjectNone, -1);
    }
    break;
  case ObjectField5:
    set_object(pos, ObjectFieldExpired, -1);
    break;
  default:
    break;
  }
}

/* Update hidden parts of the map data. */
//  which seems to be only Fish
// modified this to happen far less often
//  so on a size 3 map with default mapgen and average amount of water
//  about one fish will spawn per minute
// UPDATE this still results in too many fish, reducing further
// UPDATE - wait, the amount of fish looks okay but the fishman are never 
//   catching any... I think their success chance may be very slow unless
//   there are a lot of fish... trying pushing this back up...
//  yes, there needs to be a good amount of fish for fisherman to have
//   reasonable chance of success, changing approach to allow spawning 
//   much more fish initially as part of mapgen, adding option_FishSpawnSlowly
//   to make this optional
void
Map::update_hidden(MapPos pos, Random *rnd) {
  /* Update fish resources in water */
  if (is_in_water(pos) && landscape_tiles[pos].resource_amount > 0) {

    int r = rnd->random();

    // adding support for option_FishSpawnSlowly 
    //  limit the rate of new fish *spawning* (they still move often)
    double doubrand = double(rnd->random());
    double roll = 100.00 * doubrand / double(UINT16_MAX);
    if (option_FishSpawnSlowly && roll < 98.50) {  // ~1fish every 2min on map3
      // don't even consider spawning
    }else{
      // execute normal spawn chance roll
      if (landscape_tiles[pos].resource_amount < 10 && (r & 0x3f00)) {
        /* Spawn more fish. */
        //Log::Debug["map"] << "inside update_hidden, spawning a new fish";
        landscape_tiles[pos].resource_amount += 1;
      }
    }

    /* Move in a random direction of: right, down right, left, up left */
    MapPos adj_pos = pos;
    switch ((r >> 2) & 3) {
      case 0: adj_pos = move_right(adj_pos); break;
      case 1: adj_pos = move_down_right(adj_pos); break;
      case 2: adj_pos = move_left(adj_pos); break;
      case 3: adj_pos = move_up_left(adj_pos); break;
      default: NOT_REACHED(); break;
    }

    if (is_in_water(adj_pos)) {
      /* Migrate a fish to adjacent water space. */
      landscape_tiles[pos].resource_amount -= 1;
      landscape_tiles[adj_pos].resource_amount += 1;
    }
  }
}

// added features
void
Map::update_environment(MapPos pos, Random *rnd) { 
  //Log::Debug["map"] << "inside Map::update_environment()";
  //
  // new baby trees spontaneously grow if a mature trees of same type nearby
  //
  // NOTES: 
  //  every tree on the entire map has a chance to spawn a new baby tree of same type each time this function is called
  //  this function is called many times per second, around ten times per sec on a freshly started game (might change over time)
  //  the more mature trees on a map, the more baby trees will spawn
  //  the larger the map, the more trees will spawn (because there are more total trees on the map)
  //  trees will spawn even outside the player viewport window
  //  suggest tuning the %roll chance to adjust the rate of tree spawning, but fix the linear relationship of mature trees to new tree rate

  if (!option_TreesReproduce){
    return;
  }

  Map::Object type = get_obj(pos);
  Map::Object newtype = Map::ObjectNone;
  //Log::Debug["map"] << "inside Map::update_environment(), random pos " << pos << " has object type " << NameObject[type];

  if (type >= Map::ObjectTree0 && type <= Map::ObjectTree7){
    newtype = Map::ObjectNewTree;
  }
  else if (type >= Map::ObjectPine0 && type <= Map::ObjectPine7){
    newtype = Map::ObjectNewPine;
  }
  if (newtype == Map::ObjectNone){
    //Log::Debug["map"] << "inside Map::update_environment(), random pos " << pos << " has no mature tree, skipping pos";
    return;
  }
  // limit the rate of baby trees appearing
  double doubrand = double(rnd->random());
  double roll = 100.00 * doubrand / double(UINT16_MAX);
  //Log::Debug["map"] << "inside Map::update_environment(), doubrand " << doubrand << ", roll " << roll;
  if (roll < 99.80) {
    //Log::Debug["map"] << "inside Map::update_environment(), doubrand " << doubrand << ", roll " << roll << ", RETURNING";
    return;  // in most cases, do NOT place a tree
  }
  //Log::Debug["map"] << "inside Map::update_environment(), random pos " << pos << " has a mature tree, newtype is " << NameObject[newtype];
  for (int i = 0; i < _spiral_dist[4]; i++) {
    // this should be a random pos spirally, a function that I don't think exists yet, or might only exist in AI functions
    //  could use a lazy trick such as rand (hah not really random!) chance of actually trying each spot
    //  or could write new function to make it random, or pick a random direction and travel outwards instead of spirally
    MapPos p = pos_add_spirally(pos, i);
    //Log::Debug["map"] << "inside Map::update_environment(), random pos " << pos << ", considering spirally pos " << p;
    // I was thinking I could re-use the Forester/ranger tree-placement rules here, but I don't understand them
    //  instead, using my own rules
    if (get_obj(p) > Map::ObjectNone){
      // another object here, can't place tree
      continue;
    }
    // in original game and Freeserf, no trees can be placed on mountains (Tundra/Snow terrain types)
    //  however, I intend to add a feature to allow Pines on mountains, but haven't done it yet.
    //  when it is done, allow Pines to grow on mountains!
    //if ( map_types_within(pos, Map::TerrainGrass0, Map::TerrainGrass3) && newtype == Map::ObjectNewTree)
    //  || map_types_within(pos, Map::TerrainGrass0, Map::TerrainGrass3)
    if (types_within(p, Map::TerrainGrass0, Map::TerrainGrass3)){
      //Log::Debug["map"] << "option_TreesReproduce is on, placing a baby tree of type " << NameObject[newtype] << ", at pos " << p << ", which is near parent tree pos " << pos;
      set_object(p, newtype, 0);
      return;
    }
  }
  //Log::Debug["map"] << "done Map::update_environment()";
}

/* Update map data as part of the game progression. */
// it seems that the map is updated one pos at a time, and not very often
// for example, one update is the time it takes for a felled tree to change
// into a stump, and one update for each incremental transition of farm fields
//
// pos are not strictly updated in serial, it seems to do every 92 tiles? 
//
// one pos is updated every 20 ticks on map size 3, I think it scales with map size
// game->update runs every tick (2 ticks at Normal speed 2, 40 ticks at 40x...)
//
// it takes ~20480 ticks for same pos to update!  on map size 3

void
Map::update(unsigned int tick, Random *rnd) {
  uint16_t delta = tick - update_state.last_tick;
  update_state.last_tick = tick;
  update_state.counter -= delta;

  int iters = 0;
  while (update_state.counter < 0) {
    iters += regions;
    update_state.counter += 20;
  }

  MapPos pos = update_state.initial_pos;

  for (int i = 0; i < iters; i++) {
    update_state.remove_signs_counter -= 1;
    if (update_state.remove_signs_counter < 0) {
      update_state.remove_signs_counter = 16;
    }

    /* Test if moving 23 positions right crosses map boundary. */
    if (pos_col(pos) + 23 < static_cast<int>(geom_.cols())) {
      pos = move_right_n(pos, 23);
    } else {
      pos = move_right_n(pos, 23);
      pos = move_down(pos);
    }

    //Log::Debug["map"] << "inside Map::update, about to call update_xxxx on pos " << pos << ", tick " << tick;

    /* Update map at position. */
    update_hidden(pos, rnd);  // this is happening way too often, it only affects fish and too many fish are spawning
    update_public(pos, rnd);
    update_environment(pos, rnd);
  }

  update_state.initial_pos = pos;
}

// work in progress, used for pathfinder_freewalking_serf
bool
Map::can_serf_step_into(MapPos check_pos) const {
  if (Map::map_space_from_obj[get_obj(check_pos)] >= SpaceSemipassable) {
    //Log::Debug["map.cc"] << "inside Map::can_serf_step_into, check_pos " << check_pos << ", is impassible because of blocking object";
    return false;
  }
  if (is_in_water(check_pos)) {
    //Log::Debug["map.cc"] << "inside Map::can_serf_step_into, check_pos " << check_pos << ", is impassible because it is_in_water";
    return false;
  }
  //Log::Debug["map.cc"] << "inside Map::can_serf_step_into, check_pos " << check_pos << ", is crossable";
  return true;
}

/* Return non-zero if the road segment from pos in direction dir
 can be successfully constructed at the current time. */
bool
Map::is_road_segment_valid(MapPos pos, Direction dir) const {
  MapPos other_pos = move(pos, dir);

  Object obj = get_obj(other_pos);
  if ((paths(other_pos) != 0 && obj != ObjectFlag) ||
      Map::map_space_from_obj[obj] >= SpaceSemipassable) {
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
Map::place_road_segments(const Road &road) {
  MapPos pos_ = road.get_source();
  Road::Dirs dirs = road.get_dirs();
  Road::Dirs::const_iterator it = dirs.begin();
  for (; it != dirs.end(); ++it) {
    Direction rev_dir = reverse_direction(*it);

    if (!is_road_segment_valid(pos_, *it)) {
      /* Not valid after all. Backtrack and abort.
       This is needed to check that the road
       does not cross itself. */
      for (; it != dirs.begin();) {
        --it;
        Direction rev_dir = *it;
        Direction dir = reverse_direction(rev_dir);

        game_tiles[pos_].paths &= ~BIT(dir);
        game_tiles[move(pos_, dir)].paths &= ~BIT(rev_dir);

        pos_ = move(pos_, dir);
      }

      return false;
    }

    game_tiles[pos_].paths |= BIT(*it);
    game_tiles[move(pos_, *it)].paths |= BIT(rev_dir);

    pos_ = move(pos_, *it);
  }

  return true;
}

bool
Map::remove_road_backref_until_flag(MapPos pos_, Direction dir) {
  while (1) {
    pos_ = move(pos_, dir);

    /* Clear backreference */
    game_tiles[pos_].paths &= ~BIT(reverse_direction(dir));

    if (get_obj(pos_) == ObjectFlag) break;

    /* Find next direction of path. */
    dir = DirectionNone;
    for (Direction d : cycle_directions_cw()) {
      if (has_path(pos_, d)) {
        dir = d;
        break;
      }
    }

    if (dir == DirectionNone) return false;
  }

  return true;
}

bool
Map::remove_road_backrefs(MapPos pos_) {
  if (paths(pos_) == 0) return false;

  /* Find directions of path segments to be split. */
  Direction path_1_dir = DirectionNone;
  auto cycle = cycle_directions_cw();
  auto it = cycle.begin();
  for (; it != cycle.end(); ++it) {
    if (has_path(pos_, *it)) {
      path_1_dir = *it;
      break;
    }
  }

  Direction path_2_dir = DirectionNone;
  ++it;
  for (; it != cycle.end(); ++it) {
    if (has_path(pos_, *it)) {
      path_2_dir = *it;
      break;
    }
  }

  if (path_1_dir == DirectionNone || path_2_dir == DirectionNone) return false;

  if (!remove_road_backref_until_flag(pos_, path_1_dir)) return false;
  if (!remove_road_backref_until_flag(pos_, path_2_dir)) return false;

  return true;
}

Direction
Map::remove_road_segment(MapPos *pos, Direction dir) {
  /* Clear forward reference. */
  game_tiles[*pos].paths &= ~BIT(dir);
  *pos = move(*pos, dir);

  /* Clear backreference. */
  game_tiles[*pos].paths &= ~BIT(reverse_direction(dir));

  /* Find next direction of path. */
  dir = DirectionNone;
  for (Direction d : cycle_directions_cw()) {
    if (has_path(*pos, d)) {
      dir = d;
      break;
    }
  }

  return dir;
}

bool
Map::road_segment_in_water(MapPos pos_, Direction dir) {
  if (dir > DirectionDown) {
    pos_ = move(pos_, dir);
    dir = reverse_direction(dir);
  }

  bool water = false;

  switch (dir) {
    case DirectionRight:
      if (type_down(pos_) <= TerrainWater3 &&
          type_up(move_up(pos_)) <= TerrainWater3) {
        water = true;
      }
      break;
    case DirectionDownRight:
      if (type_up(pos_) <= TerrainWater3 &&
          type_down(pos_) <= TerrainWater3) {
        water = true;
      }
      break;
    case DirectionDown:
      if (type_up(pos_) <= TerrainWater3 &&
          type_down(move_left(pos_)) <= TerrainWater3) {
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
Map::add_change_handler(Handler *handler) {
  change_handlers.push_back(handler);
}

void
Map::del_change_handler(Handler *handler) {
  change_handlers.remove(handler);
}

// returns true only if ONLY the specified terrain
//  is found, false if any other type found
bool
Map::types_within(MapPos pos, Terrain low, Terrain high) {
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

bool
Map::operator == (const Map& rhs) const {
  // Check fundamental properties
  if (this->geom_ != rhs.geom_ ||
      this->regions != rhs.regions ||
      this->update_state != rhs.update_state) {
    return false;
  }

  // Check all tiles
  for (MapPos pos_ : geom_) {
    if (this->landscape_tiles[pos_] != rhs.landscape_tiles[pos_]) {
      return false;
    }
    if (this->game_tiles[pos_] != rhs.game_tiles[pos_]) {
      return false;
    }
  }

  return true;
}

bool
Map::operator != (const Map& rhs) const {
  return !(*this == rhs);
}

SaveReaderBinary&
operator >> (SaveReaderBinary &reader, Map &map) {
  uint8_t v8;
  uint16_t v16;

  const MapGeometry &geom = map.geom();
  for (unsigned int y = 0; y < geom.rows(); y++) {
    for (unsigned int x = 0; x < geom.cols(); x++) {
      MapPos pos = map.pos(x, y);
      Map::GameTile &game_tile = map.game_tiles[pos];
      Map::LandscapeTile &landscape_tile = map.landscape_tiles[pos];
      reader >> v8;
      game_tile.paths = v8 & 0x3f;
      reader >> v8;
      landscape_tile.height = v8 & 0x1f;
      if ((v8 >> 7) == 0x01) {
        game_tile.owner = ((v8 >> 5) & 0x03) + 1;
      }
      reader >> v8;
      landscape_tile.type_up = (Map::Terrain)((v8 >> 4) & 0x0f);
      landscape_tile.type_down = (Map::Terrain)(v8 & 0x0f);
      reader >> v8;
      landscape_tile.obj = (Map::Object)(v8 & 0x7f);
      game_tile.idle_serf = 0;  // (BIT_TEST(v8, 7) != 0);
    }
    for (unsigned int x = 0; x < geom.cols(); x++) {
      MapPos pos = map.pos(x, y);
      Map::GameTile &game_tile = map.game_tiles[pos];
      Map::LandscapeTile &landscape_tile = map.landscape_tiles[pos];
      if (map.get_obj(pos) >= Map::ObjectFlag &&
          map.get_obj(pos) <= Map::ObjectCastle) {
        landscape_tile.mineral = Map::MineralsNone;
        landscape_tile.resource_amount = 0;
        reader >> v16;
        game_tile.obj_index = v16;
      } else {
        reader >> v8;
        landscape_tile.mineral = (Map::Minerals)((v8 >> 5) & 7);
        landscape_tile.resource_amount = v8 & 0x1f;
        reader >> v8;
        game_tile.obj_index = 0;
      }

      reader >> v16;
      game_tile.serf = v16;
    }
  }

  return reader;
}

MapPos
Map::pos_from_saved_value(uint32_t val) {
  val = val >> 2;
  int x = val & get_col_mask();
  val = val >> (get_row_shift() + 1);
  int y = val & get_row_mask();
  return pos(x, y);
}

#define SAVE_MAP_TILE_SIZE (16)

SaveReaderText&
operator >> (SaveReaderText &reader, Map &map) {
  int x = 0;
  int y = 0;
  reader.value("pos")[0] >> x;
  reader.value("pos")[1] >> y;
  MapPos pos = map.pos(x, y);

  for (int y = 0; y < SAVE_MAP_TILE_SIZE; y++) {
    for (int x = 0; x < SAVE_MAP_TILE_SIZE; x++) {
      MapPos p = map.pos_add(pos, map.pos(x, y));
      Map::GameTile &game_tile = map.game_tiles[p];
      Map::LandscapeTile &landscape_tile = map.landscape_tiles[p];
      unsigned int val;

      reader.value("paths")[y*SAVE_MAP_TILE_SIZE+x] >> val;
      game_tile.paths = val & 0x3f;

      reader.value("height")[y*SAVE_MAP_TILE_SIZE+x] >> val;
      landscape_tile.height = val & 0x1f;

      reader.value("type.up")[y*SAVE_MAP_TILE_SIZE+x] >> val;
      landscape_tile.type_up = (Map::Terrain)val;

      reader.value("type.down")[y*SAVE_MAP_TILE_SIZE+x] >> val;
      landscape_tile.type_down = (Map::Terrain)val;

      try {
        reader.value("idle_serf")[y*SAVE_MAP_TILE_SIZE+x] >> val;
        game_tile.idle_serf = (val != 0);
        reader.value("object")[y*SAVE_MAP_TILE_SIZE+x] >> val;
        landscape_tile.obj = (Map::Object)val;
      } catch (...) {
        reader.value("object")[y*SAVE_MAP_TILE_SIZE+x] >> val;
        landscape_tile.obj = (Map::Object)(val & 0x7f);
        game_tile.idle_serf = (BIT_TEST(val, 7) != 0);
      }

      reader.value("serf")[y*SAVE_MAP_TILE_SIZE+x] >> val;
      game_tile.serf = val;

      reader.value("resource.type")[y*SAVE_MAP_TILE_SIZE+x] >> val;
      landscape_tile.mineral = (Map::Minerals)val;

      reader.value("resource.amount")[y*SAVE_MAP_TILE_SIZE+x] >> val;
      landscape_tile.resource_amount = val;
    }
  }

  return reader;
}

SaveWriterText&
operator << (SaveWriterText &writer, Map &map) {
  int i = 0;

  for (unsigned int ty = 0; ty < map.get_rows(); ty += SAVE_MAP_TILE_SIZE) {
    for (unsigned int tx = 0; tx < map.get_cols(); tx += SAVE_MAP_TILE_SIZE) {
      SaveWriterText &map_writer = writer.add_section("map", i++);

      map_writer.value("pos") << tx;
      map_writer.value("pos") << ty;

      for (int y = 0; y < SAVE_MAP_TILE_SIZE; y++) {
        for (int x = 0; x < SAVE_MAP_TILE_SIZE; x++) {
          MapPos pos = map.pos(tx+x, ty+y);

          map_writer.value("height") << map.get_height(pos);
          map_writer.value("type.up") << map.type_up(pos);
          map_writer.value("type.down") << map.type_down(pos);
          map_writer.value("paths") << map.paths(pos);
          map_writer.value("object") << map.get_obj(pos);
          map_writer.value("serf") << map.get_serf_index(pos);
          map_writer.value("idle_serf") << map.get_idle_serf(pos);

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

MapPos
Road::get_end(Map *map) const {
  MapPos result = begin;
  for (const Direction dir : dirs) {
    result = map->move(result, dir);
  }
  return result;
}

bool
Road::has_pos(Map *map, MapPos pos) const {
  MapPos result = begin;
  for (const Direction dir : dirs) {
    if (result == pos) {
      return true;
    }
    result = map->move(result, dir);
  }
  return (result == pos);
}

bool
Road::is_valid_extension(Map *map, Direction dir) const {
  if (is_undo(dir)) {
    return false;
  }

  /* Check that road does not cross itself. */
  MapPos extended_end = map->move(get_end(map), dir);
  MapPos pos = begin;
  bool valid = true;
  for (const Direction _dir : dirs) {
    pos = map->move(pos, _dir);
    if (pos == extended_end) {
      valid = false;
      break;
    }
  }

  return valid;
}

bool
Road::is_undo(Direction dir) const {
  return (dirs.size() > 0) && (dirs.back() == reverse_direction(dir));
}

bool
Road::extend(Direction dir) {
  if (begin == bad_map_pos) {
    return false;
  }

  // dec15 2021 got map::at out of range
  dirs.push_back(dir);

  return true;
}

bool
Road::undo() {
  if (begin == bad_map_pos) {
    return false;
  }

  dirs.pop_back();
  if (dirs.size() == 0) {
    begin = bad_map_pos;
  }

  return true;
}

