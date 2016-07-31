/*
 * map-generator.cc - Map generator
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

#include "src/map-generator.h"

#include <cstdlib>
#include <algorithm>

#include "src/debug.h"

const int ClassicMapGenerator::default_max_lake_area = 14;
const int ClassicMapGenerator::default_water_level = 20;
const int ClassicMapGenerator::default_terrain_spikyness = 0x9999;

ClassicMapGenerator::ClassicMapGenerator(const Map& map, const Random& random)
  : map(map), rnd(random) {}

ClassicMapGenerator::~ClassicMapGenerator() {
  if (tiles != NULL) {
    delete[] tiles;
  }
}

void ClassicMapGenerator::init(
    int height_generator, bool preserve_bugs, int max_lake_area,
    int water_level, int terrain_spikyness) {
  tile_count = map.get_cols() * map.get_rows();
  tiles = new MapTile[tile_count]();

  this->height_generator = height_generator;
  this->preserve_bugs = preserve_bugs;
  this->max_lake_area = max_lake_area;
  this->water_level = water_level;
  this->terrain_spikyness = terrain_spikyness;
}

/* Whether any of the two up/down tiles at this pos are water. */
bool ClassicMapGenerator::is_water_tile(MapPos pos) const {
  return tiles[pos].type_down <= Map::TerrainWater3 &&
    tiles[pos].type_up <= Map::TerrainWater3;
}

/* Whether the position is completely surrounded by water. */
bool ClassicMapGenerator::is_in_water(MapPos pos) const {
  return (is_water_tile(pos) &&
          is_water_tile(map.move_up_left(pos)) &&
          tiles[map.move_left(pos)].type_down <= Map::TerrainWater3 &&
          tiles[map.move_up(pos)].type_up <= Map::TerrainWater3);
}

void ClassicMapGenerator::generate() {
  rnd ^= Random(0x5a5a, 0xa5a5, 0xc3c3);

  random_int();
  random_int();

  init_heights_squares();
  switch (height_generator) {
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
  create_water_bodies();
  heights_rebase();
  init_types();
  remove_islands();
  heights_rescale();
  init_sub();
}

uint16_t
ClassicMapGenerator::random_int() {
  return rnd.random();
}

/* Midpoint displacement map generator. This function initialises the height
   values in the corners of 16x16 squares. */
void
ClassicMapGenerator::init_heights_squares() {
  for (unsigned int y = 0; y < map.get_rows(); y += 16) {
    for (unsigned int x = 0; x < map.get_cols(); x += 16) {
      int rnd = random_int() & 0xff;
      tiles[map.pos(x, y)].height = std::min(rnd, 250);
    }
  }
}

int
ClassicMapGenerator::calc_height_displacement(int avg, int base, int offset) {
  int r = random_int();
  int h = ((r * base) >> 16) - offset + avg;

  return std::max(0, std::min(h, 250));
}

/* Calculate height values of the subdivisions in the
   midpoint displacement algorithm. */
void
ClassicMapGenerator::init_heights_midpoints() {
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
  int r2 = (r1 * terrain_spikyness) >> 16;

  for (int i = 8; i > 0; i >>= 1) {
    for (unsigned int y = 0; y < map.get_rows(); y += 2*i) {
      for (unsigned int x = 0; x < map.get_cols(); x += 2*i) {
        MapPos pos_ = map.pos(x, y);
        int h = tiles[pos_].height;

        MapPos pos_r = map.move_right_n(pos_, 2*i);
        MapPos pos_mid_r = map.move_right_n(pos_, i);
        int h_r = tiles[pos_r].height;

        if (preserve_bugs) {
          /* The intention was probably just to set h_r to the map height value,
             but the upper bits of rnd must be preserved in h_r in the first
             iteration to generate the same maps as the original game. */
          if (x == 0 && y == 0 && i == 8) h_r |= rnd & 0xff00;
        }

        tiles[pos_mid_r].height = calc_height_displacement((h + h_r)/2, r1, r2);

        MapPos pos_d = map.move_down_n(pos_, 2*i);
        MapPos pos_mid_d = map.move_down_n(pos_, i);
        int h_d = tiles[pos_d].height;
        tiles[pos_mid_d].height = calc_height_displacement((h+h_d)/2, r1, r2);

        MapPos pos_dr = map.move_right_n(map.move_down_n(pos_, 2*i), 2*i);
        MapPos pos_mid_dr = map.move_right_n(map.move_down_n(pos_, i), i);
        int h_dr = tiles[pos_dr].height;
        tiles[pos_mid_dr].height = calc_height_displacement((h+h_dr)/2, r1, r2);
      }
    }

    r1 >>= 1;
    r2 >>= 1;
  }
}

void
ClassicMapGenerator::init_heights_diamond_square() {
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
  int r2 = (r1 * terrain_spikyness) >> 16;

  for (int i = 8; i > 0; i >>= 1) {
    /* Diamond step */
    for (unsigned int y = 0; y < map.get_rows(); y += 2*i) {
      for (unsigned int x = 0; x < map.get_cols(); x += 2*i) {
        MapPos pos_ = map.pos(x, y);
        int h = tiles[pos_].height;

        MapPos pos_r = map.move_right_n(pos_, 2*i);
        int h_r = tiles[pos_r].height;

        MapPos pos_d = map.move_down_n(pos_, 2*i);
        int h_d = tiles[pos_d].height;

        MapPos pos_dr = map.move_right_n(map.move_down_n(pos_, 2*i), 2*i);
        int h_dr = tiles[pos_dr].height;

        MapPos pos_mid_dr = map.move_right_n(map.move_down_n(pos_, i), i);
        int avg = (h + h_r + h_d + h_dr) / 4;
        tiles[pos_mid_dr].height = calc_height_displacement(avg, r1, r2);
      }
    }

    /* Square step */
    for (unsigned int y = 0; y < map.get_rows(); y += 2*i) {
      for (unsigned int x = 0; x < map.get_cols(); x += 2*i) {
        MapPos pos_ = map.pos(x, y);
        int h = tiles[pos_].height;

        MapPos pos_r = map.move_right_n(pos_, 2*i);
        int h_r = tiles[pos_r].height;

        MapPos pos_d = map.move_down_n(pos_, 2*i);
        int h_d = tiles[pos_d].height;

        MapPos pos_ur = map.move_right_n(map.move_down_n(pos_, -i), i);
        int h_ur = tiles[pos_ur].height;

        MapPos pos_dr = map.move_right_n(map.move_down_n(pos_, i), i);
        int h_dr = tiles[pos_dr].height;

        MapPos pos_dl = map.move_right_n(map.move_down_n(pos_, i), -i);
        int h_dl = tiles[pos_dl].height;

        MapPos pos_mid_r = map.move_right_n(pos_, i);
        int avg_r = (h + h_r + h_ur + h_dr) / 4;
        tiles[pos_mid_r].height = calc_height_displacement(avg_r, r1, r2);

        MapPos pos_mid_d = map.move_down_n(pos_, i);
        int avg_d = (h + h_d + h_dl + h_dr) / 4;
        tiles[pos_mid_d].height = calc_height_displacement(avg_d, r1, r2);
      }
    }

    r1 >>= 1;
    r2 >>= 1;
  }
}

bool
ClassicMapGenerator::adjust_map_height(int h1, int h2, MapPos pos) {
  if (abs(h1 - h2) > 32) {
    tiles[pos].height = h1 + ((h1 < h2) ? 32 : -32);
    return true;
  }

  return false;
}

/* Ensure that map heights of adjacent fields are not too far apart. */
void
ClassicMapGenerator::clamp_heights() {
  bool changed = true;
  while (changed) {
    changed = false;
    for (unsigned int y = 0; y < map.get_rows(); y++) {
      for (unsigned int x = 0; x < map.get_cols(); x++) {
        MapPos pos_ = map.pos(x, y);
        int h = tiles[pos_].height;

        MapPos pos_d = map.move_down(pos_);
        int h_d = tiles[pos_d].height;
        changed |= adjust_map_height(h, h_d, pos_d);

        MapPos pos_dr = map.move_down_right(pos_);
        int h_dr = tiles[pos_dr].height;
        changed |= adjust_map_height(h, h_dr, pos_dr);

        MapPos pos_r = map.move_right(pos_);
        int h_r = tiles[pos_r].height;
        changed |= adjust_map_height(h, h_r, pos_r);
      }
    }
  }
}

// Expand water around position.
//
// Expand water area by marking shores with 254 and water positions with 255.
// Water (255) can only be expanded to a position where all six adjacent
// positions are at or lower than the water level. When a position is marked
// as water (255) the surrounding positions, that are not yet marked, are
// changed to shore (254). Returns true only if the given position was
// converted to water.
bool
ClassicMapGenerator::expand_water_position(MapPos pos_) {
  bool expanding = false;

  for (int d = DirectionRight; d <= DirectionUp; d++) {
    MapPos new_pos = map.move(pos_, (Direction)d);
    int height = tiles[new_pos].height;
    if (water_level < height && height < 254) {
      return false;
    } else if (height == 255) {
      expanding = true;
    }
  }

  if (expanding) {
    tiles[pos_].height = 255;

    for (int d = DirectionRight; d <= DirectionUp; d++) {
      MapPos new_pos = map.move(pos_, (Direction)d);
      if (tiles[new_pos].height != 255) tiles[new_pos].height = 254;
    }
  }

  return expanding;
}

// Try to expand area around position into a water body.
//
// After expanding, the water body will be tagged with the heights 253 for
// positions in water and 252 for positions on the shore.
void
ClassicMapGenerator::expand_water_body(MapPos pos) {
  // Check whether it is possible to expand from this position.
  for (int d = DirectionRight; d <= DirectionUp; d++) {
    MapPos new_pos = map.move(pos, (Direction)d);
    if (tiles[new_pos].height > water_level) {
      // Expanding water from this position was not possible. Just raise the
      // height to one above sea level.
      tiles[pos].height = 0;
      return;
    }
  }

  // Initialize expansion
  tiles[pos].height = 255;
  for (int d = DirectionRight; d <= DirectionUp; d++) {
    MapPos new_pos = map.move(pos, (Direction)d);
    tiles[new_pos].height = 254;
  }

  // Expand water until we are unable to expand any more or until the max
  // lake area limit has been reached.
  for (int i = 0; i < max_lake_area; i++) {
    bool expanded = false;

    MapPos new_pos = map.move_right_n(pos, i+1);
    for (int k = 0; k < 6; k++) {
      Direction d = turn_direction(DirectionDown, k);
      for (int j = 0; j <= i; j++) {
        expanded |= expand_water_position(new_pos);
        new_pos = map.move(new_pos, d);
      }
    }

    if (!expanded) break;
  }

  // Change the water encoding from 255,254 to 253,252. This change means that
  // when expanding another lake, this area will look like an elevated plateau
  // at heights 252/253 and the other lake will not be able to expand into this
  // area. This keeps water bodies from growing larger than the max lake area.
  tiles[pos].height -= 2;

  for (int i = 0; i < max_lake_area + 1; i++) {
    MapPos new_pos = map.move_right_n(pos, i+1);
    for (int k = 0; k < 6; k++) {
      Direction d = (Direction)((k + DirectionDown) % 6);
      for (int j = 0; j <= i; j++) {
        if (tiles[new_pos].height > 253) tiles[new_pos].height -= 2;
        new_pos = map.move(new_pos, d);
      }
    }
  }
}

// Create water bodies on the map.
//
// Try to expand every position that is at or below the water level into a
// body of water. After expanding bodies of water, the height of the positions
// are changed such that the lowest points on the map are at water_level - 1
// (marking water) and just above that the height is at water_level (marking
// shore).
void
ClassicMapGenerator::create_water_bodies() {
  for (int h = 0; h <= water_level; h++) {
    for (unsigned int y = 0; y < map.get_rows(); y++) {
      for (unsigned int x = 0; x < map.get_cols(); x++) {
        MapPos pos_ = map.pos(x, y);
        if (tiles[pos_].height == h) {
          expand_water_body(pos_);
        }
      }
    }
  }

  // Map positions are marked in the previous loop.
  // 0: Above water level.
  // 252: Land at water level.
  // 253: Water.
  for (unsigned int y = 0; y < map.get_rows(); y++) {
    for (unsigned int x = 0; x < map.get_cols(); x++) {
      MapPos pos_ = map.pos(x, y);
      int h = tiles[pos_].height;
      switch (h) {
        case 0:
          tiles[pos_].height = water_level + 1;
          break;
        case 252:
          tiles[pos_].height = water_level;
          break;
        case 253:
          tiles[pos_].height = water_level - 1;
          tiles[pos_].resource_type = Map::MineralsNone;
          tiles[pos_].resource_amount = random_int() & 7; /* Fish */
          break;
      }
    }
  }
}

/* Adjust heights so zero height is sea level. */
void
ClassicMapGenerator::heights_rebase() {
  int h = water_level - 1;

  for (unsigned int y = 0; y < map.get_rows(); y++) {
    for (unsigned int x = 0; x < map.get_cols(); x++) {
      tiles[map.pos(x, y)].height -= h;
    }
  }
}

static Map::Terrain
calc_map_type(int h_sum) {
  if (h_sum < 3) return Map::TerrainWater0;
  else if (h_sum < 384) return Map::TerrainGrass1;
  else if (h_sum < 416) return Map::TerrainGrass2;
  else if (h_sum < 448) return Map::TerrainTundra0;
  else if (h_sum < 480) return Map::TerrainTundra1;
  else if (h_sum < 528) return Map::TerrainTundra2;
  else if (h_sum < 560) return Map::TerrainSnow0;
  return Map::TerrainSnow1;
}

/* Set type of map fields based on the height value. */
void
ClassicMapGenerator::init_types() {
  for (unsigned int y = 0; y < map.get_rows(); y++) {
    for (unsigned int x = 0; x < map.get_cols(); x++) {
      MapPos pos_ = map.pos(x, y);
      int h1 = tiles[pos_].height;
      int h2 = tiles[map.move_right(pos_)].height;
      int h3 = tiles[map.move_down_right(pos_)].height;
      int h4 = tiles[map.move_down(pos_)].height;
      tiles[pos_].type_up = calc_map_type(h1 + h3 + h4);
      tiles[pos_].type_down = calc_map_type(h1 + h2 + h3);
    }
  }
}

void
ClassicMapGenerator::clear_all_objects() {
  for (unsigned int y = 0; y < map.get_rows(); y++) {
    for (unsigned int x = 0; x < map.get_cols(); x++) {
      tiles[map.pos(x, y)].obj = Map::ObjectNone;
    }
  }
}

// Remove islands.
//
// Pick an initial map position, then search from there to see which other
// positions on the map are reachable (over land) from that position. If the
// reachable positions cover at least 1/4 of the map, then stop and convert any
// position that was _not_ reached to water. Otherwise, keep trying new initial
// positions.
//
// In most cases this will eliminate any island that covers less than 1/4 of
// the map. However, since the markings are not reset after an initial
// position has failed to expand to 1/4 of the map, it is still possible for
// islands to survive if they by change happen to be in the area where the
// first initial positions are chosen (around 0, 0).
void
ClassicMapGenerator::remove_islands() {
  clear_all_objects();

  for (unsigned int y = 0; y < map.get_rows(); y++) {
    for (unsigned int x = 0; x < map.get_cols(); x++) {
      MapPos pos_ = map.pos(x, y);

      if (tiles[pos_].height > 0 && tiles[pos_].obj == 0) {
        tiles[pos_].obj = static_cast<Map::Object>(1);

        unsigned int num = 0;
        bool changed = true;
        while (changed) {
          changed = false;
          for (unsigned int y = 0; y < map.get_rows(); y++) {
            for (unsigned int x = 0; x < map.get_cols(); x++) {
              MapPos pos_ = map.pos(x, y);

              if (tiles[pos_].obj == 1) {
                num += 1;
                tiles[pos_].obj = static_cast<Map::Object>(2);

                // The i'th flag will indicate whether a path on land from
                // pos_in direction i is possible.
                int flags = 0;
                if (tiles[pos_].type_down >= Map::TerrainGrass0) {
                  flags |= 3;
                }
                if (tiles[pos_].type_up >= Map::TerrainGrass0) {
                  flags |= 6;
                }
                if (tiles[map.move_left(pos_)].type_down >=
                    Map::TerrainGrass0) {
                  flags |= 0xc;
                }
                if (tiles[map.move_up_left(pos_)].type_up >=
                    Map::TerrainGrass0) {
                  flags |= 0x18;
                }
                if (tiles[map.move_up_left(pos_)].type_down >=
                    Map::TerrainGrass0) {
                  flags |= 0x30;
                }
                if (tiles[map.move_up(pos_)].type_up >= Map::TerrainGrass0) {
                  flags |= 0x21;
                }

                // Mark positions following any valid direction on land.
                for (int d = DirectionRight; d <= DirectionUp; d++) {
                  if (BIT_TEST(flags, d)) {
                    if (tiles[map.move(pos_, (Direction)d)].obj == 0) {
                      tiles[map.move(pos_, (Direction)d)].obj =
                        static_cast<Map::Object>(1);
                      changed = true;
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

  // Change every position that was not marked to water.
  for (unsigned int y = 0; y < map.get_rows(); y++) {
    for (unsigned int x = 0; x < map.get_cols(); x++) {
      MapPos pos_ = map.pos(x, y);

      if (tiles[pos_].height > 0 && tiles[pos_].obj == 0) {
        tiles[pos_].height = 0;
        tiles[pos_].type_up = Map::TerrainWater0;
        tiles[pos_].type_up = Map::TerrainWater0;

        tiles[map.move_left(pos_)].type_down = Map::TerrainWater0;
        tiles[map.move_up_left(pos_)].type_up = Map::TerrainWater0;
        tiles[map.move_up_left(pos_)].type_down = Map::TerrainWater0;
        tiles[map.move_up(pos_)].type_up = Map::TerrainWater0;
      }
    }
  }

  clear_all_objects();
}

/* Rescale height values to be in [0;31]. */
void
ClassicMapGenerator::heights_rescale() {
  for (unsigned int y = 0; y < map.get_rows(); y++) {
    for (unsigned int x = 0; x < map.get_cols(); x++) {
      MapPos pos_ = map.pos(x, y);
      tiles[pos_].height = (tiles[pos_].height + 6) >> 3;
    }
  }
}

// Change terrain types based on a seed type in adjacent tiles.
//
// For every triangle, if the current type is old and any adjacent triangle
// has type seed, then the triangle is changed into the new_ terrain type.
void
ClassicMapGenerator::seed_terrain_type(Map::Terrain old, Map::Terrain seed,
                                       Map::Terrain new_) {
  for (unsigned int y = 0; y < map.get_rows(); y++) {
    for (unsigned int x = 0; x < map.get_cols(); x++) {
      MapPos pos_ = map.pos(x, y);

      // Check that the central triangle is of type old (*), and that any
      // adjacent triangle is of type seed:
      //     ____
      //    /\  /\
      //   /__\/__\
      //  /\  /\  /\
      // /__\/*_\/__\
      // \  /\  /\  /
      //  \/__\/__\/
      //
      if (tiles[pos_].type_up == old &&
          (seed == tiles[map.move_up_left(pos_)].type_down ||
           seed == tiles[map.move_up_left(pos_)].type_up ||
           seed == tiles[map.move_up(pos_)].type_up ||
           seed == tiles[map.move_left(pos_)].type_down ||
           seed == tiles[map.move_left(pos_)].type_up ||
           seed == tiles[pos_].type_down ||
           seed == tiles[map.move_right(pos_)].type_up ||
           seed == tiles[map.move_down_left(pos_)].type_down ||
           seed == tiles[map.move_down(pos_)].type_down ||
           seed == tiles[map.move_down(pos_)].type_up ||
           seed == tiles[map.move_down_right(pos_)].type_down ||
           seed == tiles[map.move_down_right(pos_)].type_up)) {
        tiles[pos_].type_up = new_;
      }

      // Check that the central triangle is of type old (*), and that any
      // adjacent triangle is of type seed:
      //   ________
      //  /\  /\  /\
      // /__\/__\/__\
      // \  /\* /\  /
      //  \/__\/__\/
      //   \  /\  /
      //    \/__\/
      //
      if (tiles[pos_].type_down == old &&
          (seed == tiles[map.move_up_left(pos_)].type_down ||
           seed == tiles[map.move_up_left(pos_)].type_up ||
           seed == tiles[map.move_up(pos_)].type_down ||
           seed == tiles[map.move_up(pos_)].type_up ||
           seed == tiles[map.move_up_right(pos_)].type_up ||
           seed == tiles[map.move_left(pos_)].type_down ||
           seed == tiles[pos_].type_up ||
           seed == tiles[map.move_right(pos_)].type_down ||
           seed == tiles[map.move_right(pos_)].type_up ||
           seed == tiles[map.move_down(pos_)].type_down ||
           seed == tiles[map.move_down_right(pos_)].type_down ||
           seed == tiles[map.move_down_right(pos_)].type_up)) {
        tiles[pos_].type_down = new_;
      }
    }
  }
}

// Change water type based on closeness to shore.
//
// Change type from TerrainWater0 to higher water (1-3) types based on
// closeness to the shore. The water closest to the shore will become
// TerrainWater3.
void
ClassicMapGenerator::change_shore_water_type() {
  seed_terrain_type(
    Map::TerrainWater0, Map::TerrainGrass1, Map::TerrainWater3);
  seed_terrain_type(
    Map::TerrainWater0, Map::TerrainWater3, Map::TerrainWater2);
  seed_terrain_type(
    Map::TerrainWater0, Map::TerrainWater2, Map::TerrainWater1);
}

// Change grass type of shore to TerrainGrass0.
//
// Change type from TerrainGrass1 to TerrainGrass0 where the tiles are
// adjacent to water.
void
ClassicMapGenerator::change_shore_grass_type() {
  seed_terrain_type(
    Map::TerrainGrass1, Map::TerrainWater3, Map::TerrainGrass0);
}


// Check whether large down-triangle is suitable for desert.
//
// The large down-triangle at position A is made up of the following
// triangular pieces. The method returns true only if all terrain types
// within the triangle are either TerrainGrass1 or TerrainDesert2.
//
// __ A ___
// \  /\  /
//  \/__\/
//   \  /
//    \/
//
bool
ClassicMapGenerator::check_desert_down_triangle(MapPos pos_) {
  Map::Terrain type_d = tiles[pos_].type_down;
  Map::Terrain type_u = tiles[pos_].type_up;

  if (type_d != Map::TerrainGrass1 && type_d != Map::TerrainDesert2) {
    return false;
  }
  if (type_u != Map::TerrainGrass1 && type_u != Map::TerrainDesert2) {
    return false;
  }

  type_d = tiles[map.move_left(pos_)].type_down;
  if (type_d != Map::TerrainGrass1 && type_d != Map::TerrainDesert2) {
    return false;
  }

  type_d = tiles[map.move_down(pos_)].type_down;
  if (type_d != Map::TerrainGrass1 && type_d != Map::TerrainDesert2) {
    return false;
  }

  return true;
}

// Check whether large up-triangle is suitable for desert.
//
// The large up-triangle at position A is made up of the following
// triangular pieces. The method returns true only if all terrain types
// within the triangle are either TerrainGrass1 or TerrainDesert2.
//
//      /\
//   A /__\
//    /\  /\
//   /__\/__\
//
bool
ClassicMapGenerator::check_desert_up_triangle(MapPos pos_) {
  Map::Terrain type_d = tiles[pos_].type_down;
  Map::Terrain type_u = tiles[pos_].type_up;

  if (type_d != Map::TerrainGrass1 && type_d != Map::TerrainDesert2) {
    return false;
  }
  if (type_u != Map::TerrainGrass1 && type_u != Map::TerrainDesert2) {
    return false;
  }

  type_u = tiles[map.move_right(pos_)].type_up;
  if (type_u != Map::TerrainGrass1 && type_u != Map::TerrainDesert2) {
    return false;
  }

  type_u = tiles[map.move_up(pos_)].type_up;
  if (type_u != Map::TerrainGrass1 && type_u != Map::TerrainDesert2) {
    return false;
  }

  return true;
}

/* Create deserts on the map. */
void
ClassicMapGenerator::init_desert() {
  for (int i = 0; i < map.get_region_count(); i++) {
    for (int try_ = 0; try_ < 200; try_++) {
      MapPos rnd_pos = map.get_rnd_coord(NULL, NULL, &rnd);

      if (tiles[rnd_pos].type_up == Map::TerrainGrass1 &&
          tiles[rnd_pos].type_down == Map::TerrainGrass1) {
        for (int index = 255; index >= 0; index--) {
          MapPos pos = map.pos_add_spirally(rnd_pos, index);

          if (check_desert_down_triangle(pos)) {
            tiles[pos].type_up = Map::TerrainDesert2;
          }

          if (check_desert_up_triangle(pos)) {
            tiles[pos].type_down = Map::TerrainDesert2;
          }
        }
        break;
      }
    }
  }
}

void
ClassicMapGenerator::init_desert_2_sub() {
  for (unsigned int y = 0; y < map.get_rows(); y++) {
    for (unsigned int x = 0; x < map.get_cols(); x++) {
      MapPos pos_ = map.pos(x, y);
      int type_d = tiles[pos_].type_down;
      int type_u = tiles[pos_].type_up;

      if (type_d >= Map::TerrainGrass3 && type_d <= Map::TerrainDesert1) {
        tiles[pos_].type_down = Map::TerrainGrass1;
      }
      if (type_u >= Map::TerrainGrass3 && type_u <= Map::TerrainDesert1) {
        tiles[pos_].type_up = Map::TerrainGrass1;
      }
    }
  }
}

void
ClassicMapGenerator::init_desert_2() {
  seed_terrain_type(
    Map::TerrainDesert2, Map::TerrainGrass1, Map::TerrainGrass3);
  seed_terrain_type(
    Map::TerrainDesert2, Map::TerrainGrass3, Map::TerrainDesert0);
  seed_terrain_type(
    Map::TerrainDesert2, Map::TerrainDesert0, Map::TerrainDesert1);

  init_desert_2_sub();

  seed_terrain_type(
    Map::TerrainGrass1, Map::TerrainDesert2, Map::TerrainDesert1);
  seed_terrain_type(
    Map::TerrainGrass1, Map::TerrainDesert1, Map::TerrainDesert0);
  seed_terrain_type(
    Map::TerrainGrass1, Map::TerrainDesert0, Map::TerrainGrass3);
}

/* Put crosses on top of mountains. */
void
ClassicMapGenerator::create_crosses() {
  for (unsigned int y = 0; y < map.get_rows(); y++) {
    for (unsigned int x = 0; x < map.get_cols(); x++) {
      MapPos pos_ = map.pos(x, y);
      unsigned int h = tiles[pos_].height;
      if (h >= 26 &&
          h >= tiles[map.move_right(pos_)].height &&
          h >= tiles[map.move_down_right(pos_)].height &&
          h >= tiles[map.move_down(pos_)].height &&
          h > tiles[map.move_left(pos_)].height &&
          h > tiles[map.move_up_left(pos_)].height &&
          h > tiles[map.move_up(pos_)].height) {
        tiles[pos_].obj = Map::ObjectCross;
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
ClassicMapGenerator::hexagon_types_in_range(MapPos pos_, Map::Terrain min,
                                            Map::Terrain max) {
  Map::Terrain type_d = tiles[pos_].type_down;
  Map::Terrain type_u = tiles[pos_].type_up;

  if (type_d < min || type_d > max) return false;
  if (type_u < min || type_u > max) return false;

  type_d = tiles[map.move_left(pos_)].type_down;
  if (type_d < min || type_d > max) return false;

  type_d = tiles[map.move_up_left(pos_)].type_down;
  type_u = tiles[map.move_up_left(pos_)].type_up;
  if (type_d < min || type_d > max) return false;
  if (type_u < min || type_u > max) return false;

  /* Should be checkeing the up tri type. */
  if (preserve_bugs) {
    type_d = tiles[map.move_up(pos_)].type_down;
    if (type_d < min || type_d > max) return false;
  } else {
    type_u = tiles[map.move_up(pos_)].type_up;
    if (type_u < min || type_u > max) return false;
  }

  return true;
}

/* Get a random position in the spiral pattern based at col, row. */
MapPos
ClassicMapGenerator::pos_add_spirally_random(MapPos pos, int mask) {
  return map.pos_add_spirally(pos, random_int() & mask);
}

/* Create clusters of map objects.

   Tries to create num_clusters of objects in random locations on the map.
   Each cluster has up to objs_in_cluster objects. The pos_mask is used in
   the call to pos_add_spirally_random to determine the max cluster size. The
   type_min and type_max determine the range (both inclusive) of terrain
   types that must appear around a position to be elegible for placement of
   an object. The obj_base determines the first object type that can be placed
   and the obj_mask specifies a mask on a random integer that is added to the
   base to obtain the final object type.
*/
void
ClassicMapGenerator::create_random_object_clusters(
    int num_clusters, int objs_in_cluster, int pos_mask, Map::Terrain type_min,
    Map::Terrain type_max, int obj_base, int obj_mask) {
  for (int i = 0; i < num_clusters; i++) {
    for (int try_ = 0; try_ < 100; try_++) {
      MapPos rnd_pos = map.get_rnd_coord(NULL, NULL, &rnd);
      if (hexagon_types_in_range(rnd_pos, type_min, type_max)) {
        for (int j = 0; j < objs_in_cluster; j++) {
          MapPos pos_ = pos_add_spirally_random(rnd_pos, pos_mask);
          if (hexagon_types_in_range(pos_, type_min, type_max) &&
              tiles[pos_].obj == Map::ObjectNone) {
            tiles[pos_].obj = static_cast<Map::Object>(
              (random_int() & obj_mask) + obj_base);
          }
        }
        break;
      }
    }
  }
}

void
ClassicMapGenerator::create_objects() {
  int regions = map.get_region_count();

  create_crosses();

  // Add either tree or pine.
  create_random_object_clusters(
    regions * 8, 10, 0xff, Map::TerrainGrass1, Map::TerrainGrass2,
    Map::ObjectTree0, 0xf);

  // Add only trees.
  create_random_object_clusters(
    regions, 45, 0x3f, Map::TerrainGrass1, Map::TerrainGrass2,
    Map::ObjectTree0, 0x7);

  // Add only pines.
  create_random_object_clusters(
    regions, 30, 0x3f, Map::TerrainGrass0, Map::TerrainGrass2,
    Map::ObjectPine0, 0x7);

  // Add either tree or pine.
  create_random_object_clusters(
    regions, 20, 0x7f, Map::TerrainGrass1, Map::TerrainGrass2,
    Map::ObjectTree0, 0xf);

  // Create dense clusters of stone.
  create_random_object_clusters(
    regions, 40, 0x3f, Map::TerrainGrass1, Map::TerrainGrass2,
    Map::ObjectStone0, 0x7);

  // Create sparse clusters.
  create_random_object_clusters(
    regions, 15, 0xff, Map::TerrainGrass1, Map::TerrainGrass2,
    Map::ObjectStone0, 0x7);

  // Create dead trees.
  create_random_object_clusters(
    regions, 2, 0xff, Map::TerrainGrass1, Map::TerrainGrass2,
    Map::ObjectDeadTree, 0);

  // Create sandstone boulders.
  create_random_object_clusters(
    regions, 6, 0xff, Map::TerrainGrass1, Map::TerrainGrass2,
    Map::ObjectSandstone0, 0x1);

  // Create trees submerged in water.
  create_random_object_clusters(
    regions, 50, 0x7f, Map::TerrainWater2, Map::TerrainWater3,
    Map::ObjectWaterTree0, 0x3);

  // Create tree stubs.
  create_random_object_clusters(
    regions, 5, 0xff, Map::TerrainGrass1, Map::TerrainGrass2,
    Map::ObjectStub, 0);

  // Create small boulders.
  create_random_object_clusters(
    regions, 10, 0xff, Map::TerrainGrass1, Map::TerrainGrass2,
    Map::ObjectStone, 0x1);

  // Create animal cadavers in desert.
  create_random_object_clusters(
    regions, 2, 0xf, Map::TerrainDesert2, Map::TerrainDesert2,
    Map::ObjectCadaver0, 0x1);

  // Create cacti in desert.
  create_random_object_clusters(
    regions, 6, 0x7f, Map::TerrainDesert0, Map::TerrainDesert2,
    Map::ObjectCactus0, 0x1);

  // Create boulders submerged in water.
  create_random_object_clusters(
    regions, 8, 0x7f, Map::TerrainWater0, Map::TerrainWater2,
    Map::ObjectWaterStone0, 0x1);

  // Create palm trees in desert.
  create_random_object_clusters(
    regions, 6, 0x3f, Map::TerrainDesert2, Map::TerrainDesert2,
    Map::ObjectPalm0, 0x3);
}

// Expand a cluster of minerals.
void
ClassicMapGenerator::expand_mineral_cluster(
    int iters, MapPos init_pos, int *index, int amount,
    Map::Minerals type) {
  for (int i = 0; i < iters; i++) {
    MapPos pos = map.pos_add_spirally(init_pos, *index);
    *index += 1;

    if (tiles[pos].resource_type == Map::MineralsNone ||
        tiles[pos].resource_amount < amount) {
      tiles[pos].resource_type = type;
      tiles[pos].resource_amount = amount;
    }
  }
}

// Create random clusters of mineral deposits.
//
// Tries to create num_clusters of minerals of the given type. The terrain type
// around a position must be in the min, max range (both inclusive) for a
// resource to be deposited.
void
ClassicMapGenerator::create_random_mineral_clusters(
    int num_clusters, Map::Minerals type,
    Map::Terrain min, Map::Terrain max) {
  const int iterations[] = { 1, 6, 12, 18, 24, 30 };

  for (int i = 0; i < num_clusters; i++) {
    for (int try_ = 0; try_ < 100; try_++) {
      MapPos pos_ = map.get_rnd_coord(NULL, NULL, &rnd);

      if (hexagon_types_in_range(pos_, min, max)) {
        int index = 0;
        int count = 2 + ((random_int() >> 2) & 3);

        for (int j = 0; j < count; j++) {
          int amount = 4 * (count - j);
          expand_mineral_cluster(iterations[j], pos_, &index, amount, type);
        }

        break;
      }
    }
  }
}

// Initialize mineral deposits in the ground.
void
ClassicMapGenerator::create_mineral_deposits() {
  int regions = map.get_region_count();
  create_random_mineral_clusters(
    regions * 9, Map::MineralsCoal,
    Map::TerrainTundra0, Map::TerrainSnow0);
  create_random_mineral_clusters(
    regions * 4, Map::MineralsIron,
    Map::TerrainTundra0, Map::TerrainSnow0);
  create_random_mineral_clusters(
    regions * 2, Map::MineralsGold,
    Map::TerrainTundra0, Map::TerrainSnow0);
  create_random_mineral_clusters(
    regions * 2, Map::MineralsStone,
    Map::TerrainTundra0, Map::TerrainSnow0);
}

void
ClassicMapGenerator::init_clean_up() {
  /* Make sure that it is always possible to walk around
     any impassable objects. This also clears water obstacles
     except in certain positions near the shore. */
  for (unsigned int y = 0; y < map.get_rows(); y++) {
    for (unsigned int x = 0; x < map.get_cols(); x++) {
      MapPos pos_ = map.pos(x, y);
      if (Map::map_space_from_obj[tiles[pos_].obj] >= Map::SpaceImpassable) {
        // Due to a quirk in the original game the three adjacent positions
        // were not checked directly whether they were impassable but instead
        // another flag was used to mark the position impassable. This flag
        // was only initialzed for water positions before this loop and was
        // initialized as part of this same loop for non-water positions. For
        // this reason, the check for impassable spaces would never succeed
        // under two particular conditions at the map edge:
        // 1) x == 0 && d == DirectionLeft
        // 2) y == 0 && (d == DirectionUp || d == DirectionUpLeft)
        for (int d = DirectionLeft; d <= DirectionUp; d++) {
          MapPos other_pos = map.move(pos_, (Direction)d);
          Map::Space s = Map::map_space_from_obj[tiles[other_pos].obj];

          bool check_impassable = false;
          if (!(x == 0 && d == DirectionLeft) &&
              !((d == DirectionUp || d == DirectionUpLeft) && y == 0)) {
            check_impassable = s >= Map::SpaceImpassable;
          }

          if (is_in_water(other_pos) || check_impassable) {
            tiles[pos_].obj = Map::ObjectNone;
            break;
          }
        }
      }
    }
  }
}

void
ClassicMapGenerator::init_sub() {
  // Adjust terrain types on shores
  change_shore_water_type();
  change_shore_grass_type();

  // Create deserts
  init_desert();
  init_desert_2();

  // Create map objects (trees, boulders, etc.)
  create_objects();

  create_mineral_deposits();

  init_clean_up();
}

ClassicMissionMapGenerator::ClassicMissionMapGenerator(
  const Map& map, const Random &rnd) : ClassicMapGenerator(map, rnd) {}

void ClassicMissionMapGenerator::init() {
  ClassicMapGenerator::init(0, true);
}
