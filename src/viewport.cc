/*
 * viewport.cc - Viewport GUI component
 *
 * Copyright (C) 2013-2019  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/viewport.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <sstream>
#include <string>
#include <vector>

#include "src/misc.h"
#include "src/game.h"
#include "src/log.h"
#include "src/debug.h"
#include "src/data.h"
#include "src/audio.h"
#include "src/gfx.h"
#include "src/interface.h"
#include "src/popup.h"
#include "src/pathfinder.h"

#define MAP_TILE_WIDTH   32
#define MAP_TILE_HEIGHT  20

#define MAP_TILE_TEXTURES  33
#define MAP_TILE_MASKS     81

/* Number of cols,rows in each landscape tile */
//#define MAP_TILE_COLS  16  // this is original value from Freeserf
//#define MAP_TILE_ROWS  16  // this is original value from Freeserf

// I tried experimenting with changing these
//  values above 64x64 result in a division error somewhere, didn't investigate further
//  values above 16x16 up to 64x64 result in modest performance games, got 4-5 more FPS in Debug build on speed 12 (max "physics warp")
//  values below 8x8 result in modestly worse performance
// didn't do extensive testing, posible that with Release build these differences are eliminated
//   possible that the landscape tile drawing is insignificant compared to the active serfs/building being drawn on screen
// also, at normal game tick speed (game speed 2 or 13+ "time warp") the frame rate is unchanged (stays 50fps)
// consider that there could be intermittent performance hits when tiles are invalidated?  I tried doing the popups and weather changes
//  that trigger this, but it didn't seem to have any effect.  Will leave it at 64x64 for some time since it appears to give better perf
#define MAP_TILE_COLS  64
#define MAP_TILE_ROWS  64

MapPos debug_overlay_clicked_pos = bad_map_pos;

// this array is used to get the map_ground sprite id for a given
//  Map::Terrain type (ex. TypeSnow0)  and luminosity level
// Notice that 32 (water) is all same because it only has one luminosity levle
//  buy others have varying levels
static const uint8_t tri_spr[] = {
  32, 32, 32, 32, 32, 32, 32, 32,  // water
  32, 32, 32, 32, 32, 32, 32, 32,  // water
  32, 32, 32, 32, 32, 32, 32, 32,  // water
  32, 32, 32, 32, 32, 32, 32, 32,  // water
  0, 1, 2, 3, 4, 5, 6, 7,          // grass
  0, 1, 2, 3, 4, 5, 6, 7,          // grass
  0, 1, 2, 3, 4, 5, 6, 7,          // grass
  0, 1, 2, 3, 4, 5, 6, 7,          // grass
  24, 25, 26, 27, 28, 29, 30, 31,  // desert
  24, 25, 26, 27, 28, 29, 30, 31,  // desert
  24, 25, 26, 27, 28, 29, 30, 31,  // desert
  8, 9, 10, 11, 12, 13, 14, 15,    // tundra (brown mountain)
  8, 9, 10, 11, 12, 13, 14, 15,    // tundra (brown mountain)
  8, 9, 10, 11, 12, 13, 14, 15,    // tundra (brown mountain)
  16, 17, 18, 19, 20, 21, 22, 23,  // snow
  16, 17, 18, 19, 20, 21, 22, 23   // snow
};

// return the new type (or original type)
//  as modified by any special new option
//  logic.  This function exists to avoid
//  having to constantly sync identical logic
//  between draw_triangle_up & _down
Map::Terrain
Viewport::special_terrain_type(MapPos pos, Map::Terrain type){
  //Log::Debug["viewport.cc"] << "start of Viewport::special_terrain_type(), pos " << pos << ", orig terrain type " << type;
  Map::Terrain new_type = type;

  if (option_FourSeasons){
    // MID-WINTER
    // make snow cover a bit more of mountains than usual
    if (season == 3 && subseason >= 3 && subseason <= 14){      
      if (type >= Map::TerrainTundra2){  // a bit more snow on mountains
        // do a partial change for one subseason
        if ((subseason == 3 || subseason == 14) && (pos % 7 == 0 || pos % 11 == 0 || pos % 13 == 0 || pos % 17 == 0 || pos % 19 == 0)){  // sort of "is prime?"
          // skip every other pos
        }else{
          new_type = Map::TerrainSnow0;
        }
      }
    }
    // MID-SUMMER - a bit less snow on mountains than usual
    if (season == 1 && subseason >= 3 && subseason <= 14){
      if (type == Map::TerrainSnow0){    // a bit less snow on mountains
        // do a partial change for one subseason
        if ((subseason == 3 || subseason == 14) && (pos % 7 == 0 || pos % 11 == 0 || pos % 13 == 0 || pos % 17 == 0 || pos % 19 == 0)){  // sort of "is prime?"
          // skip every other pos
        }else{
          new_type = Map::TerrainTundra2;
        }  
      }
    }
    //
    // other times, normal amount
    //

    // SNOWSTORM!!!!!
    //  need to add custom snow-on-buildings and trees for this to look acceptable
    //if (season == 3 && type > Map::TerrainWater3){
    //  new_type = Map::TerrainSnow0;
    //}
  }

  //Log::Debug["viewport.cc"] << "start of Viewport::special_terrain_type(), pos " << pos << ", orig terrain type " << type << ", new type " << new_type;
  return new_type;
}

// return the new sprite id (or original sprite id)
//  as modified by any special new option
//  logic.  This function exists to avoid
//  having to constantly sync identical logic
//  between draw_triangle_up & _down
int
Viewport::special_terrain_sprite(MapPos pos, int sprite_index){
  Log::Debug["viewport.cc"] << "start of Viewport::special_terrain_sprite(), pos " << pos << ", orig terrain sprite_index " << sprite_index;

  // option_FogOfWar
  //  draw revealed-but-not-currently-visible areas darker than normal
  /* no longer using prerendered custom PNG sprites,
      switching to using mutated sprites derived directly from base sprites
  if (option_FogOfWar){
    static const int8_t tri_fog[] = {
      34, 34, 36,  0,  1,  2,  3,  4,  // grass   (default  0-7)  lower # is darker
      37, 38, 39,  8,  9, 10, 11, 12,  // tundra  (default  8-15)
      40, 41, 42, 16, 17, 18, 19, 20,  // snow    (default  16-23)
      43, 44, 45, 24, 25, 26, 27, 28,  // desert  (default  24-31)
      46,                              // water   (default has only has one luminosity, sprite 32)
      33,                              // shroud  (does not exist in default, no luminosity change here. Added for simplicity)
    };  // 34 values in array ([0-33])
    if (!map->is_visible(pos, interface->get_player()->get_index())){
      sprite_index = tri_fog[sprite_index];
    }
  }
  */

  // use 100+base to indicate darker sprites for FogOfWar
  //  the SpriteDosSolid constructor has added logic to handle it
  if (option_FogOfWar){
    if (!map->is_visible(pos, interface->get_player()->get_index())){
      sprite_index += 100;
    }
  }

  Log::Debug["viewport.cc"] << "start of Viewport::special_terrain_sprite(), pos " << pos << ", new sprite_index " << sprite_index;
  return sprite_index;
}

void
Viewport::draw_triangle_up(int lx, int ly, int m, int left, int right,
                           MapPos pos, Frame *tile) {
// NOTE THAT THIS IS NOT THE SAME LUMINOSITY tri_mask AS draw_triangle_down!
  static const int8_t tri_mask[] = {
     0,  1,  3,  6,  7, -1, -1, -1, -1,
     0,  1,  2,  5,  6,  7, -1, -1, -1,
     0,  1,  2,  3,  5,  6,  7, -1, -1,
     0,  1,  2,  3,  4,  5,  6,  7, -1,
     0,  1,  2,  3,  4,  4,  5,  6,  7,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,
    -1, -1,  0,  1,  2,  4,  5,  6,  7,
    -1, -1, -1,  0,  1,  2,  5,  6,  7,
    -1, -1, -1, -1,  0,  1,  4,  6,  7
  };  // this is a 9x9 array, so 81 values in array ([0-80])

  if (((left - m) < -4) || ((left - m) > 4)) {
    throw ExceptionFreeserf("Failed to draw triangle up (1).");
  }
  if (((right - m) < -4) || ((right - m) > 4)) {
    throw ExceptionFreeserf("Failed to draw triangle up (2).");
  }

  int mask = 4 + m - left + 9*(4 + m - right);
  if (tri_mask[mask] < 0) {
    throw ExceptionFreeserf("Failed to draw triangle up (3).");
  }

  Map::Terrain type = map->type_up(map->move_up(pos));
  
  // apply any hack-ish modifications from various game options
  type = special_terrain_type(pos, Map::Terrain(type));

  int sprite = -1;

  int index = (type << 3) | tri_mask[mask];
  if (index >= 128) {
    throw ExceptionFreeserf("Failed to draw triangle up (4).");
  }

  // this array is used to get the map_ground sprite id for a given
  //  Map::Terrain type (ex. TypeSnow0)  and luminosity level
  // note that there are 15 terrain types but 33 terrain sprites
  //  because all terrain types except water have varying luminosity
  //int sprite = tri_spr[index];
  sprite = tri_spr[index];

  // apply any hack-ish modifications from various game options
  //sprite = special_terrain_sprite(pos, sprite);

  bool darken = false;
  if (option_FogOfWar){
    //f (false){  // TEMP DISABLED
    if (!map->is_revealed(pos,interface->get_player()->get_index())){ 
      //sprite = -1;
      //sprite = 200;
      // this doesn't work... try just not drawing the sprite at all!
      return;
    } else if (!map->is_visible(pos, interface->get_player()->get_index())){
      // SPECIAL CATCH TO SMOOTH THE EDGES OF THE TOP-LEFT 
      //  AND DOWN-RIGHT EDGES OF FOW HEXAGON - DRAW A HALF TRIANGLE
      //  AT THE EDGE BE CHECKING IF LAST POS WAS DARKENED!
      //if (false){
        // NEVERMIND, this works for the initial castle hexagon but breaks down
        //  once more activity happens, not going to bother with it now
      //if (map->is_visible(map->move_right(pos), interface->get_player()->get_index())
      // && !map->is_visible(map->move_up(pos), interface->get_player()->get_index()) 
      // && !map->is_visible(map->move_down(pos), interface->get_player()->get_index()) ){
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // skip this Up triangle only DO NOT COPY THIS TO draw_triangle_down!!!!
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      //}else{
        darken = true;
      //}
    }
  }

  //tile->draw_masked_sprite(lx, ly, Data::AssetMapMaskUp, mask, Data::AssetMapGround, sprite);
  tile->draw_masked_sprite(lx, ly, Data::AssetMapMaskUp, mask, Data::AssetMapGround, sprite, darken);
}

void
Viewport::draw_triangle_down(int lx, int ly, int m, int left, int right,
                             MapPos pos, Frame *tile) {
// NOTE THAT THIS IS NOT THE SAME LUMINOSITY tri_mask AS draw_triangle_up!                               
  static const int8_t tri_mask[] = {
     0,  0,  0,  0,  0, -1, -1, -1, -1,
     1,  1,  1,  1,  1,  0, -1, -1, -1,
     3,  2,  2,  2,  2,  1,  0, -1, -1,
     6,  5,  3,  3,  3,  2,  1,  0, -1,
     7,  6,  5,  4,  4,  3,  2,  1,  0,
    -1,  7,  6,  5,  4,  4,  4,  2,  1,
    -1, -1,  7,  6,  5,  5,  5,  5,  4,
    -1, -1, -1,  7,  6,  6,  6,  6,  6,
    -1, -1, -1, -1,  7,  7,  7,  7,  7
  };

  if (((left - m) < -4) || ((left - m) > 4)) {
    throw ExceptionFreeserf("Failed to draw triangle down (1).");
  }
  if (((right - m) < -4) || ((right - m) > 4)) {
    throw ExceptionFreeserf("Failed to draw triangle down (2).");
  }

  int mask = 4 + left - m + 9*(4 + right - m);
  if (tri_mask[mask] < 0) {
    throw ExceptionFreeserf("Failed to draw triangle down (3).");
  }

  int type = map->type_down(map->move_up_left(pos));

  // apply any hack-ish modifications from various game options
  type = special_terrain_type(pos, Map::Terrain(type));
  
  // handle new terrain types (for option_FogOfWar shroud)
  int sprite = -1;

  int index = (type << 3) | tri_mask[mask];
  if (index >= 128) {
    throw ExceptionFreeserf("Failed to draw triangle up (4).");
  }

  // this array is used to get the map_ground sprite id for a given
  //  Map::Terrain type (ex. TypeSnow0)  and luminosity level
  // note that there are 15 terrain types but 33 terrain sprites
  //  because all terrain types except water have varying luminosity
  //int sprite = tri_spr[index];
  sprite = tri_spr[index];

  // apply any hack-ish modifications from various game options
  //sprite = special_terrain_sprite(pos, sprite);

  bool darken = false;
  if (option_FogOfWar){
    //f (false){  // TEMP DISABLED
    if (!map->is_revealed(pos,interface->get_player()->get_index())){ 
      //sprite = -1;
      //sprite = 200;
      // this doesn't work... try just not drawing the sprite at all!
      return;
    }else if (!map->is_visible(pos, interface->get_player()->get_index())){
      // SPECIAL CATCH TO SMOOTH THE EDGES OF THE TOP-LEFT 
      //  AND DOWN-RIGHT EDGES OF FOW HEXAGON - DRAW A HALF TRIANGLE
      //  AT THE EDGE BE CHECKING IF LAST POS WAS DARKENED!
      //if (false){
        // NEVERMIND, this works for the initial castle hexagon but breaks down
        //  once more activity happens, not going to bother with it now
      //if (map->is_visible(map->move_left(pos), interface->get_player()->get_index())
      // && !map->is_visible(map->move_up(pos), interface->get_player()->get_index()) 
      // && !map->is_visible(map->move_down(pos), interface->get_player()->get_index()) ){
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // skip this Down triangle only DO NOT COPY THIS TO draw_triangle_up!!!!
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      //}else{
        darken = true;
      //}
    }
  }

  tile->draw_masked_sprite(lx, ly + MAP_TILE_HEIGHT, Data::AssetMapMaskDown, mask, Data::AssetMapGround, sprite, darken);
}


// return the 0-7 luminosity value corresponding
//  to the higher (brighter) triangle (up or down)
int
Viewport::get_brighter_triangle_luminosity(MapPos pos) {
  Log::Debug["viewport.cc"] << "inside Viewport::get_brighter_triangle_luminosity, pos " << pos;
  // copied from draw_triangle_up
  static const int8_t up_tri_mask[] = {
     0,  1,  3,  6,  7, -1, -1, -1, -1,
     0,  1,  2,  5,  6,  7, -1, -1, -1,
     0,  1,  2,  3,  5,  6,  7, -1, -1,
     0,  1,  2,  3,  4,  5,  6,  7, -1,
     0,  1,  2,  3,  4,  4,  5,  6,  7,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,
    -1, -1,  0,  1,  2,  4,  5,  6,  7,
    -1, -1, -1,  0,  1,  2,  5,  6,  7,
    -1, -1, -1, -1,  0,  1,  4,  6,  7
  };  // this is a 9x9 array, so 81 values in array ([0-80])
  // copied from draw_triangle_up
  static const int8_t down_tri_mask[] = {
     0,  0,  0,  0,  0, -1, -1, -1, -1,
     1,  1,  1,  1,  1,  0, -1, -1, -1,
     3,  2,  2,  2,  2,  1,  0, -1, -1,
     6,  5,  3,  3,  3,  2,  1,  0, -1,
     7,  6,  5,  4,  4,  3,  2,  1,  0,
    -1,  7,  6,  5,  4,  4,  4,  2,  1,
    -1, -1,  7,  6,  5,  5,  5,  5,  4,
    -1, -1, -1,  7,  6,  6,  6,  6,  6,
    -1, -1, -1, -1,  7,  7,  7,  7,  7
  };

  // here is how draw_up_tile_col does it
  /*  starts a col beginning with up pointing triangle
  int m = map->get_height(pos);
  int left, right;
  // Loop until a tile is inside the frame (y >= 0).
  while (1) {
    pos = map->move_down(pos);
    left = map->get_height(pos);
    right = map->get_height(map->move_right(pos));
    int t = std::min(left, right);
    if (y_base + MAP_TILE_HEIGHT - 4*t >= 0) break;
    y_base += MAP_TILE_HEIGHT;
    pos = map->move_down_right(pos);
    m = map->get_height(pos);
    if (y_base + MAP_TILE_HEIGHT - 4*m >= 0) goto down;
    y_base += MAP_TILE_HEIGHT;
  }

  // Loop until a tile is completely outside the frame (y >= max_y). 
  while (1) {
    if (y_base - 2*MAP_TILE_HEIGHT - 4*m >= max_y) break;
    draw_triangle_up(x_base, y_base - 4*m, m, left, right, pos, tile);
    y_base += MAP_TILE_HEIGHT;
    pos = map->move_down_right(pos);
    m = map->get_height(pos);
    if (y_base - 2*MAP_TILE_HEIGHT - 4*std::max(left, right) >= max_y) break;
  down:
    draw_triangle_down(x_base, y_base - 4*m, m, left, right, pos, tile);
    y_base += MAP_TILE_HEIGHT;
    pos = map->move_down(pos);
    left = map->get_height(pos);
    right = map->get_height(map->move_right(pos));
  }
  */

  // the important bits seem to be:
  // for up triangle on up_col
  int uptri_m = map->get_height(pos);
  int uptri_posx = map->move_down(pos);
  int uptri_left = map->get_height(uptri_posx);
  int uptri_right = map->get_height(map->move_right(uptri_posx));
    
  int up_mask   = 4 +  uptri_m - uptri_left + 9*(4 + uptri_m - uptri_right);

  // for down triangle on up_col
  int dntri_left = map->get_height(pos);
  int dntri_right = map->get_height(map->move_right(pos));
  int dntri_posx = map->move_down_right(pos);
  int dntri_m = map->get_height(dntri_posx);
  dntri_posx = map->move_down_right(dntri_posx);
  dntri_m = map->get_height(dntri_posx);

  int down_mask = 4 +  dntri_left - dntri_m + 9*(4 + dntri_right - dntri_m);

  Log::Debug["viewport.cc"] << "inside Viewport::get_brighter_triangle_luminosity, pos " << pos << ", up_mask " << up_mask << ", down_mask " << down_mask;
  if (up_tri_mask[up_mask] < 0) {
    throw ExceptionFreeserf("get_brighter_triangle_luminosity found a up-triangle level below zero!");
  }
  if (down_tri_mask[down_mask] < 0) {
    throw ExceptionFreeserf("get_brighter_triangle_luminosity found a up-triangle level below zero!");
  }
  Log::Debug["viewport.cc"] << "inside Viewport::get_brighter_triangle_luminosity, pos " << pos << ", up_mask " << up_mask << ", down_mask " << down_mask << ", up luminosity " << int(up_tri_mask[up_mask]) << ", down luminosity " << int(down_tri_mask[down_mask]);
  // return the higher value
  if (up_tri_mask[up_mask] >= down_tri_mask[down_mask]){
    Log::Debug["viewport.cc"] << "inside Viewport::get_brighter_triangle_luminosity, pos " << pos << ", up_mask " << up_mask << ", down_mask " << down_mask << ", returning up luminosity " << int(up_tri_mask[up_mask]);
    return up_tri_mask[up_mask];
  }
  Log::Debug["viewport.cc"] << "inside Viewport::get_brighter_triangle_luminosity, pos " << pos << ", up_mask " << up_mask << ", down_mask " << down_mask << ", returning down luminosity " << int(down_tri_mask[down_mask]);
  return down_tri_mask[down_mask];
}


/* Draw a column (vertical) of tiles, starting at an up pointing tile. */
// NOTE - the visual lighting effect where tiles on the left/west side of
//  a terrain peak are lighter while tiles on right/east side are darker
//  appears to be based on the height difference between tiles... I think
//  that if the tile to the left has higher height than tile to the right
//  the right tile will be drawn darker (by using a darker sprite)
//  the data files contain different sprites for each lighting level
//  that are identically except for the brightness (for DOS, Amiga is dif)
// 
void
Viewport::draw_up_tile_col(MapPos pos, int x_base, int y_base, int max_y,
                           Frame *tile) {
  int m = map->get_height(pos);
  int left, right;

  /* Loop until a tile is inside the frame (y >= 0). */
  while (1) {
    /* move down */
    pos = map->move_down(pos);

    left = map->get_height(pos);
    right = map->get_height(map->move_right(pos));

    int t = std::min(left, right);
    /*if (left == right) t -= 1;*/ /* TODO ? */

    if (y_base + MAP_TILE_HEIGHT - 4*t >= 0) break;

    y_base += MAP_TILE_HEIGHT;

    /* move down right */
    pos = map->move_down_right(pos);

    m = map->get_height(pos);

    if (y_base + MAP_TILE_HEIGHT - 4*m >= 0) goto down;

    y_base += MAP_TILE_HEIGHT;
  }

  /* Loop until a tile is completely outside the frame (y >= max_y). */
  while (1) {
    if (y_base - 2*MAP_TILE_HEIGHT - 4*m >= max_y) break;

    draw_triangle_up(x_base, y_base - 4*m, m, left, right, pos, tile);

    y_base += MAP_TILE_HEIGHT;

    /* move down right */
    pos = map->move_down_right(pos);
    m = map->get_height(pos);

    if (y_base - 2*MAP_TILE_HEIGHT - 4*std::max(left, right) >= max_y) break;

  down:
    draw_triangle_down(x_base, y_base - 4*m, m, left, right, pos, tile);

    y_base += MAP_TILE_HEIGHT;

    /* move down */
    pos = map->move_down(pos);

    left = map->get_height(pos);
    right = map->get_height(map->move_right(pos));
  }
}

/* Draw a column (vertical) of tiles, starting at a down pointing tile. */
void
Viewport::draw_down_tile_col(MapPos pos, int x_base, int y_base,
                             int max_y, Frame *tile) {
  int left = map->get_height(pos);
  int right = map->get_height(map->move_right(pos));
  int m;

  /* Loop until a tile is inside the frame (y >= 0). */
  while (true) {
    /* move down right */
    pos = map->move_down_right(pos);

    m = map->get_height(pos);

    if (y_base + MAP_TILE_HEIGHT - 4*m >= 0) goto down;

    y_base += MAP_TILE_HEIGHT;

    /* move down */
    pos = map->move_down(pos);

    left = map->get_height(pos);
    right = map->get_height(map->move_right(pos));

    int t = std::min(left, right);
    /*if (left == right) t -= 1;*/ /* TODO ? */

    if (y_base + MAP_TILE_HEIGHT - 4*t >= 0) break;

    y_base += MAP_TILE_HEIGHT;
  }

  /* Loop until a tile is completely outside the frame (y >= max_y). */
  while (1) {
    if (y_base - 2*MAP_TILE_HEIGHT - 4*m >= max_y) break;

    draw_triangle_up(x_base, y_base - 4*m, m, left, right, pos, tile);

    y_base += MAP_TILE_HEIGHT;

    /* move down right */
    pos = map->move_down_right(pos);
    m = map->get_height(pos);

    if (y_base - 2*MAP_TILE_HEIGHT - 4*std::max(left, right) >= max_y) break;

  down:
    draw_triangle_down(x_base, y_base - 4*m, m, left, right, pos, tile);

    y_base += MAP_TILE_HEIGHT;

    /* move down */
    pos = map->move_down(pos);

    left = map->get_height(pos);
    right = map->get_height(map->move_right(pos));
  }
}

// flush the entire landscape tile cache, 16x16 tiles
//  will be redrawn and re-cached as they come into
//  view of player Viewport?
void
Viewport::layout() {
  Log::Debug["viewport.cc"] << "inside Viewport::layout(), flushing entire landscape_tiles cache";
  landscape_tiles.clear();
}

// flush a single tile, which is 16x16 block of MapPos triangle-pairs
//  from the tile cache so it will be redrawn by get_tile_frame on next update
void
Viewport::redraw_map_pos(MapPos pos) {
  // changing this to use new function
  int tid = tile_id_from_map_pos(pos);
  /*
  int mx, my;
  map_pix_from_map_coord(pos, map->get_height(pos), &mx, &my);

  int horiz_tiles = map->get_cols()/MAP_TILE_COLS;
  int vert_tiles = map->get_rows()/MAP_TILE_ROWS;

  int tile_width = MAP_TILE_COLS*MAP_TILE_WIDTH;
  int tile_height = MAP_TILE_ROWS*MAP_TILE_HEIGHT;

  int tc = (mx / tile_width) % horiz_tiles;
  int tr = (my / tile_height) % vert_tiles;
  int tid = tc + horiz_tiles*tr;
  */
  TilesMap::iterator it = landscape_tiles.find(tid);
  if (it != landscape_tiles.end()) {
    landscape_tiles.erase(tid);
  }
}

// return the MapPos of a get_tile_frame type
//  coordinate set
// I think that tc and tr are absolute coordinates that can
//   directly translate to MapPos
// NOTE - tid is *derived from* tc and tr, they are not separable
//  and so the only reason to pass all three is for convient/performance?
// THIS FUNCTION IS NOT YET USED!!! I created it thinking it would be 
//  needed, but that was when I misunderstood how the 16x16 tile cache worked
MapPos
Viewport::map_pos_from_tile_frame_coord(int tc, int tr) {
  int col = (tc*MAP_TILE_COLS + (tr*MAP_TILE_ROWS)/2) % map->get_cols();
  int row = tr*MAP_TILE_ROWS;
  MapPos pos = map->pos(col, row);
  return pos;
}

// return the tile id # (tid) of the
//  the 16x16 tile that contains the given MapPos
//  (so it can be fetched from the landscape_tiles cache)
unsigned int
Viewport::tile_id_from_map_pos(MapPos pos) {
  int mx, my;
  map_pix_from_map_coord(pos, map->get_height(pos), &mx, &my);
  int horiz_tiles = map->get_cols()/MAP_TILE_COLS;
  int vert_tiles = map->get_rows()/MAP_TILE_ROWS;
  int tile_width = MAP_TILE_COLS*MAP_TILE_WIDTH;
  int tile_height = MAP_TILE_ROWS*MAP_TILE_HEIGHT;
  int tc = (mx / tile_width) % horiz_tiles;
  int tr = (my / tile_height) % vert_tiles;
  int tid = tc + horiz_tiles*tr;
  return tid;
}

// this is NOT refreshed constantly
//  it is only loaded once at the start of a map, and again
//  when certain popups are open/closed or any time
//  viewport->set_size(width, height) is called, which is used
//  as the "magic refresh" by the weather graphics
// during normal game time, it simply returns the cached landscape_tiles item
// it seems to load the tile cache in large chunks, I think if any tile is not
//  found in cache it reloads the entire game-viewport-sized area that contains it

//
// IMPORTANT - a "tile" appears to be an entire 16x16 block of MapPos as defined
//  by these defines at the top of viewport.cc.  This value NEVER CHANGES regardless
//  of map size, screen resolution, window size, etc.  A frame tile is always a group
//  of 16x16 MapPos, and I believe that thefixed cache works in these units also so it is
//  not possible to load or flush a single fixedMapPos, only an entire 16x16 pos tile"
//fixed
// I HAD ALWAYS ASSUMED a "tile" as referenced in Viewport was a single MapPos
//  representing pair of up/down equilateral triangles combined into a "skewed-square"
//  parallelogram, where the actual MapPos coord is one of the triangle corners...
//                      ____ 
//    like this:   down \  /\
//                       \/__\ up
// 
//                                ... BUT THIS IS NOT CORRECT!!!!!
//
// I wonder what effect changing from 16x16 would have?  It seems the original game
//  has smaller than 16x16 MapPos shown at once in the low-res VGA mode.  It is nearly 
//  certain that at any given time the viewport spans multiple tile blocks so I am
//  thinking the tile size is just a cache unit size chosen for performance reasons
//  as a middleground between refreshing every single MapPos individually and
//  refreshing the entire map at once?  I think this is a common video game pattern
//

//#define MAP_TILE_WIDTH   32
//#define MAP_TILE_HEIGHT  20
//
//#define MAP_TILE_TEXTURES  33
//#define MAP_TILE_MASKS     81
//
///* Number of cols,rows in each landscape tile */
//#define MAP_TILE_COLS  16                      <-- this
//#define MAP_TILE_ROWS  16                      <-- this
//


// NOTE - tid is *derived from* tc and tr, they are not separable
//  and so the only reason to pass all three is for convenience/performance?
Frame *
Viewport::get_tile_frame(unsigned int tid, int tc, int tr) {
  //Log::Debug["viewport.cc"] << "start of Viewport::get_tile_frame()";

  MapPos pos = map_pos_from_tile_frame_coord(tc, tr);

/*
  //
  // to implement FogOfWar, create a second tile cache that shows the fogged
  //  tiles, and choose which to use when returning each tile
  //
  Log::Debug["viewport.cc"] << "inside of Viewport::get_tile_frame(), got pos " << pos << " from_tile_frame_coord " << tc << "," << tr;
  if (map->get_owner(pos) < 0){
    // return from shrouded FoW cache
    Log::Debug["viewport.cc"] << "inside of Viewport::get_tile_frame(), pos " << pos << " has no owner, using alternate tile cache";
    return nullptr;  // FAIL TEST
  }
  */

  //
  // if the requested 16x16 tile is already cached, return the cached tile_frame
  //
  TilesMap::iterator it = landscape_tiles.find(tid);
  if (it != landscape_tiles.end()) {
    //Log::Debug["viewport.cc"] << "start of Viewport::get_tile_frame(), tid " << tid << " found in cache, returning from tile cache";
    return it->second.get();
  }

  //
  // otherwise (re-)load the cache for this entire 16x16 tile?
  //
  //Log::Debug["viewport.cc"] << "inside of Viewport::get_tile_frame(), tid " << tid << " not found in cache, INITALIZING CACHE FOR ENTIRE AREA AROUND TILE";

  int tile_width = MAP_TILE_COLS*MAP_TILE_WIDTH;
  int tile_height = MAP_TILE_ROWS*MAP_TILE_HEIGHT;

  // create a new frame, fill with black pixels
  std::unique_ptr<Frame> tile_frame(
    Graphics::get_instance().create_frame(tile_width, tile_height));
  tile_frame->fill_rect(0, 0, tile_width, tile_height, Color::black);

/*
  // TEST
  // skip every other 16x16 tile, in a checkerboard pattern
  if ((tr + tc) % 2 == 0){
    //return tile_frame.get();
    landscape_tiles[tid] = std::move(tile_frame);
    return landscape_tiles[tid].get();
  }
  */


  // no longer needed if map_pos_from_tile_frame_coord used to set MapPos pos earlier
  //int col = (tc*MAP_TILE_COLS + (tr*MAP_TILE_ROWS)/2) % map->get_cols();
  //int row = tr*MAP_TILE_ROWS;
  //MapPos pos = map->pos(col, row);

  int x_base = -(MAP_TILE_WIDTH/2);

  /* Draw one extra column as half a column will be outside the
   map tile on both right and left side.. */
  for (int col = 0; col < MAP_TILE_COLS+1; col++) {
    //Log::Debug["viewport.cc"] << "inside of Viewport::get_tile_frame(), loading tiles for col " << col << ", pos " << pos;
    draw_up_tile_col(pos, x_base, 0, tile_height, tile_frame.get());
    draw_down_tile_col(pos, x_base + MAP_TILE_WIDTH/2, 0, tile_height,
                       tile_frame.get());

    pos = map->move_right(pos);
    x_base += MAP_TILE_WIDTH;
    
    // NOTE - it seems that the landscape tiles are only drawn once and saved
    //  so moving the map around does NOT reload them, so it is not possible to
    //  check what terrain types are in view this way!  Instead, I am going to try
    //  triggering the ambient desert and water sounds by the junk objects within them
    //  being in view, like trees/birds do
    // ambient wind gust sounds triggered by amount of desert tiles in view
    /*
    if (map->is_desert_tile(pos)){
      interface->desert_in_view += 1;
      Log::Info["viewport.cc"] << "desert tiles in view: " << interface->desert_in_view;
    }
    */

  }

  //DEBUG
  /* Draw a border around the tile for debug. */
  //
  // this is confusing because while it shows the general concept of the tile frame
  //  well, it is clear that any given 16x16 triangle-pairs are far from straight
  //  because they can slope up/down for sometimes the entire length of the 16x16 tile!
  // so the red borders I think don't actually show the exact area of the tile frame?
  //  or, they overlap significantly to avoid gaps?"
  // they DO show the exact area of the tile frame it seems, so then
  //  it must be the case that there is significant overlap.  As a test, if I only draw
  //  every other tile, there is no "bleed over" where a tile contains terrain
  //  triangles outside of the rigid rectangle.  It seems like some modest performance
  //  gain could be made by increasing the tile size to reduce the amount of duplicate
  //  rendering?  experiment with that
  //tile_frame->draw_rect(0, 0, tile_width, tile_height, Color(0xff, 0x00, 0x00));
  //END DEBUG

  Log::Verbose["viewport"] << "map: " << map->get_cols()*MAP_TILE_WIDTH << ","
                           << map->get_rows()*MAP_TILE_HEIGHT << ", cols,rows: "
                           << map->get_cols() << "," << map->get_rows()
                           << ", tc,tr: " << tc << "," << tr << ", tw,th: "
                           << tile_width << "," << tile_height;

  landscape_tiles[tid] = std::move(tile_frame);

  return landscape_tiles[tid].get();
}

void
Viewport::draw_landscape() {
  //Log::Debug["viewport.cc"] << "start of Viewport::draw_landscape()";

  int horiz_tiles = map->get_cols()/MAP_TILE_COLS;
  int vert_tiles = map->get_rows()/MAP_TILE_ROWS;

  int tile_width = MAP_TILE_COLS*MAP_TILE_WIDTH;
  int tile_height = MAP_TILE_ROWS*MAP_TILE_HEIGHT;

  int map_width = map->get_cols()*MAP_TILE_WIDTH;
  int map_height = map->get_rows()*MAP_TILE_HEIGHT;

  int my = offset_y;
  int ly = 0;
  int x_base = 0;
  while (ly < height) {
    while (my >= map_height) {
      my -= map_height;
      x_base += (map->get_rows()*MAP_TILE_WIDTH)/2;
    }

    int ty = my % tile_height;

    int lx = 0;
    int mx = (offset_x + x_base) % map_width;
    while (lx < width) {
      int tx = mx % tile_width;

      int tc = (mx / tile_width) % horiz_tiles;
      int tr = (my / tile_height) % vert_tiles;
      int tid = tc + horiz_tiles*tr;

      // fetch the 16x16 tile_frame
      //  from the landscape_tiles cache, or
      //  if not found draw it and cache it
      Frame *tile_frame = get_tile_frame(tid, tc, tr);

      int w = tile_width - tx;
      if (lx+w > width) {
        w = width - lx;
      }
      int h = tile_height - ty;
      if (ly+h > height) {
        h = height - ly;
      }

      frame->draw_frame(lx, ly, tx, ty, tile_frame, w, h);
      lx += tile_width - tx;
      mx += tile_width - tx;
    }

    ly += tile_height - ty;
    my += tile_height - ty;
  }
}


void
Viewport::draw_path_segment(int lx, int ly, MapPos pos, Direction dir) {
  int h1 = map->get_height(pos);
  int h2 = map->get_height(map->move(pos, dir));
  int h_diff = h1 - h2;

  Map::Terrain t1, t2;
  int h3, h4, h_diff_2;

  switch (dir) {
  case DirectionRight:
    ly -= 4*std::max(h1, h2) + 2;
    t1 = map->type_down(pos);
    t2 = map->type_up(map->move_up(pos));
    h3 = map->get_height(map->move_up(pos));
    h4 = map->get_height(map->move_down_right(pos));
    h_diff_2 = (h3 - h4) - 4*h_diff;
    break;
  case DirectionDownRight:
    ly -= 4*h1 + 2;
    t1 = map->type_up(pos);
    t2 = map->type_down(pos);
    h3 = map->get_height(map->move_right(pos));
    h4 = map->get_height(map->move_down(pos));
    h_diff_2 = 2*(h3 - h4);
    break;
  case DirectionDown:
    lx -= MAP_TILE_WIDTH/2;
    ly -= 4*h1 + 2;
    t1 = map->type_up(pos);
    t2 = map->type_down(map->move_left(pos));
    h3 = map->get_height(map->move_left(pos));
    h4 = map->get_height(map->move_down(pos));
    h_diff_2 = 4*h_diff - h3 + h4;
    break;
  default:
    NOT_REACHED();
    break;
  }

  int mask = h_diff + 4 + dir*9;
  int sprite = 0;
  Map::Terrain type = std::max(t1, t2);

  if (h_diff_2 > 4) {
    sprite = 0;
  } else if (h_diff_2 > -6) {
    sprite = 1;
  } else {
    sprite = 2;
  }

  if (type <= Map::TerrainWater3) {
    sprite = 9;
  } else if (type >= Map::TerrainSnow0) {
    sprite += 6;
  } else if (type >= Map::TerrainDesert0) {
    sprite += 3;
  }

  frame->draw_masked_sprite(lx, ly,
                            Data::AssetPathMask, mask,
                            Data::AssetPathGround, sprite);
}

void
Viewport::draw_border_segment(int lx, int ly, MapPos pos, Direction dir) {
  int h1 = map->get_height(pos);
  int h2 = map->get_height(map->move(pos, dir));
  int h_diff = h2 - h1;

  Map::Terrain t1, t2;
  int h3, h4, h_diff_2;

  switch (dir) {
  case DirectionRight:
    lx += MAP_TILE_WIDTH/2;
    ly -= 2*(h1 + h2) + 4;
    t1 = map->type_down(pos);
    t2 = map->type_up(map->move_up(pos));
    h3 = map->get_height(map->move_up(pos));
    h4 = map->get_height(map->move_down_right(pos));
    h_diff_2 = h3 - h4 + 4*h_diff;
    break;
  case DirectionDownRight:
    lx += MAP_TILE_WIDTH/4;
    ly -= 2*(h1 + h2) - 6;
    t1 = map->type_up(pos);
    t2 = map->type_down(pos);
    h3 = map->get_height(map->move_right(pos));
    h4 = map->get_height(map->move_down(pos));
    h_diff_2 = 2*(h3 - h4);
    break;
  case DirectionDown:
    lx -= MAP_TILE_WIDTH/4;
    ly -= 2*(h1 + h2) - 6;
    t1 = map->type_up(pos);
    t2 = map->type_down(map->move_left(pos));
    h3 = map->get_height(map->move_left(pos));
    h4 = map->get_height(map->move_down(pos));
    h_diff_2 = 4*h_diff - h3 + h4;
    break;
  default:
    NOT_REACHED();
    break;
  }

  int sprite = 0;
  Map::Terrain type = std::max(t1, t2);

  if (h_diff_2 > 1) {
    sprite = 0;
  } else if (h_diff_2 > -9) {
    sprite = 1;
  } else {
    sprite = 2;
  }

  if (type <= Map::TerrainWater3) {
    sprite = 9; /* Bouy */
  } else if (type >= Map::TerrainSnow0) {
    sprite += 6;
  } else if (type >= Map::TerrainDesert0) {
    sprite += 3;
  }

  //frame->draw_sprite(lx, ly, Data::AssetMapBorder, sprite, false);
  // avoid needing overloaded funtions, false is default anyway just omit it
  frame->draw_sprite(lx, ly, Data::AssetMapBorder, sprite);
}

MapPos
Viewport::get_offset(int *x_off, int *y_off, int *col, int *row) {
  if (x_off != nullptr) {
    *x_off = -(offset_x + 16 * (offset_y / 20)) % 32;
  }
  if (y_off != nullptr) {
    *y_off = -offset_y % 20;
  }
  int col_0 = (offset_x / 16 + offset_y / 20) / 2 & map->get_col_mask();
  int row_0 = (offset_y / MAP_TILE_HEIGHT) & map->get_row_mask();

  if (col != nullptr) {
    *col = col_0;
  }
  if (row != nullptr) {
    *row = row_0;
  }

  return map->pos(col_0, row_0);
}

void
Viewport::draw_paths_and_borders() {
  int x_off = 0;
  int y_off = 0;
  MapPos base_pos = get_offset(&x_off, &y_off);

  for (int x_base = x_off; x_base < width + MAP_TILE_WIDTH;
       x_base += MAP_TILE_WIDTH) {
    MapPos pos = base_pos;

    int y_base = y_off;
    int row = 0;

    while (1) {
      int lx;
      if (row % 2 == 0) {
        lx = x_base;
      } else {
        lx = x_base - MAP_TILE_WIDTH/2;
      }

      int ly = y_base - 4* map->get_height(pos);
      if (ly >= height) break;

      // option_FogOfWar
      //   do not draw paths/borders outside of shroud/FoW
      if (option_FogOfWar && !map->is_visible(pos, interface->get_player()->get_index())){
        // don't draw
      }else{
        // draw

        /* For each direction right, down right and down,
          draw the corresponding paths and borders. */
        for (Direction d : cycle_directions_cw(DirectionRight, 3)) {
          MapPos other_pos = map->move(pos, d);
          if (map->has_path(pos, d)) {
            draw_path_segment(lx, y_base, pos, d);
          } else if (map->has_owner(pos) != map->has_owner(other_pos) ||
                    map->get_owner(pos) != map->get_owner(other_pos)) {
            draw_border_segment(lx, y_base, pos, d);
          }
        }
      }
#if 0
      for (int d = DIR_RIGHT; d <= DIR_UP_RIGHT; d++) {
        if (map->has_path(pos, (dir_t)d)) {
          int x, y;
          screen_pix_from_map_coord(pos, &x, &y);
          int x1, y1;
          screen_pix_from_map_coord(map->move(pos, (dir_t)d), &x1, &y1);
          frame->draw_line(x, y, x1, y1, 76);
        }
      }
#endif
      if (row % 2 == 0) {
        pos = map->move_down(pos);
      } else {
        pos = map->move_down_right(pos);
      }

      y_base += MAP_TILE_HEIGHT;
      row += 1;
    }

    base_pos = map->move_right(base_pos);
  }

  /* If we're in road construction mode, also draw
     the temporarily placed roads. */
  if (interface->is_building_road()) {
    Road road = interface->get_building_road();
    MapPos pos = road.get_source();
    for (Direction dir : road.get_dirs()) {
      MapPos draw_pos = pos;
      Direction draw_dir = dir;
      if (draw_dir > DirectionDown) {
        draw_pos = map->move(pos, dir);
        draw_dir = reverse_direction(dir);
      }

      int sx, sy;
      screen_pix_from_map_coord(draw_pos, &sx, &sy);

      draw_path_segment(sx, sy + 4 * map->get_height(draw_pos), draw_pos,
                        draw_dir);

      pos = map->move(pos, dir);
    }
  }
}

void
Viewport::draw_game_sprite(int lx, int ly, int index) {
  //frame->draw_sprite(lx, ly, Data::AssetGameObject, index-1, true);
  // to avoid needing overloaded functions, just specify the defaults for other values
  frame->draw_sprite(lx, ly, Data::AssetGameObject, index-1, true, Color::transparent, 1.f);
}

void
Viewport::draw_serf(int lx, int ly, const Color &color, int head, int body) {
  // this is the actual serf_torso sprite # for the torso/body (and possibly head if all-in-one)
  //frame->draw_sprite(lx, ly, Data::AssetSerfTorso, body, true, color);
  frame->draw_sprite(lx, ly, Data::AssetSerfTorso, body, true, color, 1.f);
  //Log::Debug["viewport.cc"] << "inside Viewport::draw_serf, body/base = " << body << ", head = " << head;
  // if serf has a separate head (some torso sprites include head, such as active working serfs, draw it
  if (head >= 0) {
    frame->draw_sprite_relatively(lx, ly, Data::AssetSerfHead, head, Data::AssetSerfTorso, body);
  }
}

// this says shadow and building but it seems to include ANY map object sprite that has a shadow
//  such as trees, stones, junk objects?
void
Viewport::draw_shadow_and_building_sprite(int lx, int ly, int index, const Color &color, bool darken) {
  
  //Log::Info["viewport"] << "inside Viewport::draw_shadow_and_building_sprite for sprite index " << index << ", darken bool is " << darken;
  //frame->draw_sprite(lx, ly, Data::AssetMapShadow, index, true);
  frame->draw_sprite(lx, ly, Data::AssetMapShadow, index, true, Color::transparent, 1.f);
  //frame->draw_sprite(lx, ly, Data::AssetMapObject, index, true, color);
  //frame->draw_sprite(lx, ly, Data::AssetMapObject, index, true, color, 1.f);
  frame->draw_sprite(lx, ly, Data::AssetMapObject, index, true, color, 1.f, darken);
}

/*
// to try showing a "rubble" building with option_AdvancedDemolition
void
Viewport::draw_shadow_and_custom_building_sprite(int lx, int ly, int index, const Color &color) {
  // this says shadow and building but it seems to include ANY map object sprite such as trees, stones
  Log::Info["viewport"] << "inside Viewport::draw_shadow_and_custom_building_sprite for sprite index " << index;
  //frame->draw_sprite(lx, ly, Data::AssetMapShadow, index, true);  // call Frame::draw_sprite#3
  // draw the shadow closer so it seems smaller
  //frame->draw_sprite(lx -2, ly, Data::AssetMapShadow, index, true);  // call Frame::draw_sprite#3
  // don't draw a shadow
  
  // force to rubble image 500
  index = 500;
  //frame->draw_sprite(lx, ly, Data::AssetMapObject, index, true, color);  // call Frame::draw_sprite#5
  frame->draw_sprite_special2(lx, ly, Data::AssetMapObject, index, true, color, bad_map_pos, Map::ObjectNone);
}
*/

// this function only exists as a hack to work around the fact that custom PNG shadow transparency
//  doesn't seem to be working right, needs more investigation
// ALSO THE ORIGINAL TREE SHADOWS are dynamically animated, they move like the tree does!  
//  AND THESE CUSTOM TREE SHADOWS ARE STATIC, THIS NEEDS TO BE FIXED!!
// UPDATE - IT HAS BEEN FIXED!  - now get rid of / merge this function
void
Viewport::draw_map_sprite_special(int lx, int ly, int index, unsigned int pos, unsigned int obj, const Color &color) {
  // this is only used by draw_map_objects_row, added passing of pos and object type to support sprite replacement
  //  THIS FUNCTION MAY NOT BE NEEDED ANYMORE, could integration into normal draw sprite functions?
  //Log::Debug["viewport"] << "inside Viewport::draw_map_sprite_special for sprite index " << index;

  // draw the tree's shadow
  if ((index >= 1220 && index <= 1223)    // SPRING Tree2 (the white flowered tree)
   || (index >= 1400 && index <= 1499)) { // FALL trees all have full shadows
    // use the default "full" deciduous tree shadow for certain custom tree sprites
    frame->draw_sprite(lx, ly, Data::AssetMapShadow, index % 10, true, Color::transparent, 1.f);
  }else if (index >= 1120 && index <= 1199
         || index >= 2120 && index <= 2199){   // dark flowers
    // flowers have no shadow, do not draw one
  }else{
    // for other "non-full" trees, use custom shadow, derived from tree sprite using ImageMagick
    frame->draw_sprite(lx, ly, Data::AssetMapShadow, index, true, Color::transparent, 1.f);
  }

  // draw the tree itself
  frame->draw_sprite(lx, ly, Data::AssetMapObject, index, true, color); // pos and obj only needed for debugging
}


void
Viewport::draw_shadow_and_building_unfinished(int lx, int ly, int index,
                                              int progress) {
  //Log::Info["viewport"] << "inside Viewport::draw_shadow_and_building_unfinished for sprite index " << index;
  float p = static_cast<float>(progress) / static_cast<float>(0xFFFF);
  frame->draw_sprite(lx, ly, Data::AssetMapShadow, index, true, Color::transparent, p);
  frame->draw_sprite(lx, ly, Data::AssetMapObject, index, true, Color::transparent, p);
}

static const int map_building_frame_sprite[] = {
  0, 0xba, 0xba, 0xba, 0xba,
  0xb9, 0xb9, 0xb9, 0xb9,
  0xba, 0xc1, 0xba, 0xb1, 0xb8, 0xb1, 0xbb,
  0xb7, 0xb5, 0xb6, 0xb0, 0xb8, 0xb3, 0xaf, 0xb4
};

void
Viewport::draw_building_unfinished(Building *building, Building::Type bld_type,
                                   int lx, int ly) {
  if (building->get_progress() == 0) { /* Draw cross */
    draw_shadow_and_building_sprite(lx, ly, 0x90);
  } else {
    /* Stone waiting */
    int stone = building->waiting_stone();
    for (int i = 0; i < stone; i++) {
      draw_game_sprite(lx+10 - i*3, ly-8 + i, 1 + Resource::TypeStone);
    }

    /* Planks waiting */
    int planks = building->waiting_planks();
    for (int i = 0; i < planks; i++) {
      draw_game_sprite(lx+12 - i*3, ly-6 + i, 1 + Resource::TypePlank);
    }

    if (BIT_TEST(building->get_progress(), 15)) { /* Frame finished */
      draw_shadow_and_building_sprite(lx, ly,
                                      map_building_frame_sprite[bld_type]);
      draw_shadow_and_building_unfinished(lx, ly, map_building_sprite[bld_type],
                                         2*(building->get_progress() & 0x7fff));
    } else {
      draw_shadow_and_building_sprite(lx, ly, 0x91); /* corner stone */
      if (building->get_progress() > 1) {
        draw_shadow_and_building_unfinished(lx, ly,
                                            map_building_frame_sprite[bld_type],
                                            2*building->get_progress());
      }
    }
  }
}

void
Viewport::draw_ocupation_flag(Building *building, int lx, int ly, float mul) {
  // building->has_knight() doesn't actually check if the holder is a knight type!
  if (building->has_knight()) {
    //  to avoid drawing the occupation_flag for the demo serf entering building to demo
    //  must check serf type
    //Serf *holder_serf = interface->get_game->get_serf(building->get_holder_or_first_knight());
    // wait... instead just check to see if it was marked for demo, that's simpler
    /* disabling this as knights are now NOT immediately evicted
    if (building->is_pending_demolition()){
      // don't draw the occupation_flag
      return;
    }
    */
    draw_game_sprite(lx, ly -
                     static_cast<int>(mul * building->get_knight_count()),
                     182 + ((interface->get_game()->get_tick() >> 3) & 3) +
                     4 * static_cast<int>(building->get_threat_level()));
  }
}

void
Viewport::draw_unharmed_building(Building *building, int lx, int ly) {
  Random random;

  static const int pigfarm_anim[] = {
    0xa2, 0, 0xa2, 0, 0xa2, 0, 0xa2, 0, 0xa2, 0, 0xa3, 0,
    0xa2, 1, 0xa3, 1, 0xa2, 2, 0xa3, 2, 0xa2, 3, 0xa3, 3,
    0xa2, 4, 0xa3, 4, 0xa6, 4, 0xa6, 4, 0xa6, 4, 0xa6, 4,
    0xa4, 4, 0xa5, 4, 0xa4, 3, 0xa5, 3, 0xa4, 2, 0xa5, 2,
    0xa4, 1, 0xa5, 1, 0xa4, 0, 0xa5, 0, 0xa2, 0, 0xa2, 0,
    0xa6, 0, 0xa6, 0, 0xa6, 0, 0xa2, 0, 0xa7, 0, 0xa8, 0,
    0xa7, 0, 0xa8, 0, 0xa7, 0, 0xa8, 0, 0xa7, 0, 0xa8, 0,
    0xa7, 0, 0xa8, 0, 0xa7, 0, 0xa8, 0, 0xa7, 0, 0xa8, 0,
    0xa7, 0, 0xa8, 0, 0xa7, 0, 0xa2, 0, 0xa2, 0, 0xa2, 0,
    0xa2, 0, 0xa6, 0, 0xa6, 0, 0xa6, 0, 0xa6, 0, 0xa6, 0,
    0xa6, 0, 0xa2, 0, 0xa2, 0, 0xa7, 0, 0xa8, 0, 0xa9, 0,
    0xaa, 0, 0xab, 0, 0xac, 0, 0xad, 0, 0xac, 0, 0xad, 0,
    0xac, 0, 0xad, 0, 0xac, 0, 0xad, 0, 0xac, 0, 0xad, 0,
    0xac, 0, 0xad, 0, 0xac, 0, 0xad, 0, 0xac, 0, 0xab, 0,
    0xaa, 0, 0xa9, 0, 0xa8, 0, 0xa7, 0, 0xa2, 0, 0xa2, 0,
    0xa2, 0, 0xa2, 0, 0xa3, 0, 0xa2, 1, 0xa3, 1, 0xa2, 1,
    0xa3, 2, 0xa2, 2, 0xa3, 2, 0xa7, 2, 0xa8, 2, 0xa7, 2,
    0xa8, 2, 0xa7, 2, 0xa8, 2, 0xa7, 2, 0xa8, 2, 0xa7, 2,
    0xa8, 2, 0xa7, 2, 0xa8, 2, 0xa7, 2, 0xa8, 2, 0xa7, 2,
    0xa2, 2, 0xa2, 2, 0xa6, 2, 0xa6, 2, 0xa6, 2, 0xa6, 2,
    0xa4, 2, 0xa5, 2, 0xa4, 1, 0xa5, 1, 0xa4, 0, 0xa5, 0,
    0xa2, 0, 0xa2, 0
  };

  if (building->is_done()) {
    Building::Type type = building->get_type();
    switch (type) {
    case Building::TypeFisher:
    case Building::TypeLumberjack:
    case Building::TypeStonecutter:
    case Building::TypeForester:
    case Building::TypeStock:
    case Building::TypeFarm:
    case Building::TypeButcher:
    case Building::TypeSawmill:
    case Building::TypeToolMaker:
    case Building::TypeCastle:
      draw_shadow_and_building_sprite(lx, ly, map_building_sprite[type]);
      break;
    case Building::TypeBoatbuilder:
      draw_shadow_and_building_sprite(lx, ly, map_building_sprite[type]);
      if (building->get_res_count_in_stock(1) > 0) {
        /* TODO x might not be correct */
        draw_game_sprite(lx+3, ly + 13,
                         174 + building->get_res_count_in_stock(1));
      }
      break;
    case Building::TypeStoneMine:
    case Building::TypeCoalMine:
    case Building::TypeIronMine:
    case Building::TypeGoldMine:
      if (building->is_active()) { /* Draw elevator up */
        draw_game_sprite(lx-6, ly-39, 152);
      }
      if (building->is_playing_sfx()) { /* Draw elevator down */
        draw_game_sprite(lx-6, ly-39, 153);
        MapPos pos = building->get_position();
        if ((((interface->get_game()->get_tick() +
               reinterpret_cast<uint8_t*>(&pos)[1]) >> 3) & 7) == 0
            && random.random() < 40000) {
          play_sound(Audio::TypeSfxElevator);
        }
      }
      draw_shadow_and_building_sprite(lx, ly, map_building_sprite[type]);
      break;
    case Building::TypeHut:
      draw_shadow_and_building_sprite(lx, ly, map_building_sprite[type]);
      draw_ocupation_flag(building, lx - 14, ly + 2, 2);
      break;
    case Building::TypePigFarm:
      //Log::Info["viewport.cc"] << "pig debug: tick " << interface->get_game()->get_tick() << ", const tick " << interface->get_game()->get_const_tick() << ", building tick " << building->get_tick() << ", random.random " << std::to_string(random.random()) << ", rand() " << std::to_string(rand());
      draw_shadow_and_building_sprite(lx, ly, map_building_sprite[type]);
      if (building->get_res_count_in_stock(1) > 0) {
        int pigs_count = building->get_res_count_in_stock(1);

        //
        // THE PROBLEM is that there is no state that allows keeping track of how many times
        //  this building has been "visited" this "random tick" because the random value only
        //   updates every so many loops (5-12 or so at normal game speed) and so there is no
        //   way to keep track of how many oinks have been played this cycle
        //  need to add some kind of "last updated" or "last tick" or "oinks played" value for
        //  this building!
        // it looks like 'tick' and 'const tick' change every loop, only rand stuff does not
        // it seems ticks are always even!
        //uint16_t tick_rand = (random.random() * interface->get_game()->get_const_tick()) & 0x7f;
        //uint16_t semiconst_rand = (random.random() & 0x7f);  // this is NOT always even
        uint16_t tick_rand = (random.random() + interface->get_game()->get_const_tick()) & 0x7f;
        uint16_t limit_tick = interface->get_game()->get_const_tick() & 0x7f;
        //Log::Info["viewport.cc"] << "pig debug: pigs_count " << pigs_count << ", limit_tick " << limit_tick << ", const tick " << interface->get_game()->get_const_tick() << ", semiconst_rand " << semiconst_rand << ", tick_rand " << tick_rand;
        if (building->is_playing_sfx()){
          // throttle oinking so it doesn't happen too fast
          if (limit_tick % 8 == 0){
            play_sound(Audio::TypeSfxPigOink);
          }
          // allow up to a few oinks if >1 pig in stock
          if (pigs_count == 1 || limit_tick % 4 == 0){
            building->stop_playing_sfx();
          }
        }else{
          // begin a period of oinking, chance of oinking period increases with pigs in stock
          if (tick_rand > 0 && tick_rand < (pigs_count / 2 + 2) * 2){
            building->start_playing_sfx();
          }
        }

        int pigs_layout[] = {
          0,   0,   0,  0,
          1,  40,   2, 11,
          2, 460,   0, 17,
          3, 420, -11,  8,
          4,  90, -11, 19,
          5, 280,   8,  8,
          6, 140,  -2,  6,
          7, 180,  -8, 13,
          8, 320,  13, 14,
        };

        for (int p = 1; p <= pigs_count; p++) {
          if (pigs_count >= pigs_layout[p * 4]) {
            int i = (pigs_layout[p * 4 + 1]
                     + (interface->get_game()->get_tick() >> 3)) & 0xfe;
            draw_game_sprite(lx + pigfarm_anim[i + 1] + pigs_layout[p * 4 + 2],
                             ly + pigs_layout[p * 4 + 3], pigfarm_anim[i]);
          }
        }
      }
      break;
    case Building::TypeMill:
      if (building->is_active()) {
        if ((interface->get_game()->get_tick() >> 4) & 3) {
          building->stop_playing_sfx();
        } else if (!building->is_playing_sfx()) {
          building->start_playing_sfx();
          play_sound(Audio::TypeSfxMillGrinding);
        }
        draw_shadow_and_building_sprite(lx, ly, map_building_sprite[type] +
                                ((interface->get_game()->get_tick() >> 4) & 3));
      } else {
        draw_shadow_and_building_sprite(lx, ly, map_building_sprite[type]);
      }
      break;
    case Building::TypeBaker:
      draw_shadow_and_building_sprite(lx, ly, map_building_sprite[type]);
      if (building->is_active()) {
        draw_game_sprite(lx + 5, ly-21,
                         154 + ((interface->get_game()->get_tick() >> 3) & 7));
      }
      break;
    case Building::TypeSteelSmelter:
      draw_shadow_and_building_sprite(lx, ly, map_building_sprite[type]);
      if (building->is_active()) {
        int i = (interface->get_game()->get_tick() >> 3) & 7;
        if (i == 0 || (i == 7 && !building->is_playing_sfx())) {
          building->start_playing_sfx();
          //play_sound(Audio::TypeSfxGoldBoils);
          play_sound(Audio::TypeSfxGoldBoils, DataSourceType::Amiga);  // the DOS sound is bugged in freeserf, Amiga sound is better anyway
        } else if (i != 7) {
          building->stop_playing_sfx();
        }

        draw_game_sprite(lx+6, ly-32, 128+i);
      }
      break;
    case Building::TypeWeaponSmith:
      draw_shadow_and_building_sprite(lx, ly, map_building_sprite[type]);
      if (building->is_active()) {
        draw_game_sprite(lx-16, ly-21,
                         128 + ((interface->get_game()->get_tick() >> 3) & 7));
      }
      break;
    case Building::TypeTower:
      draw_shadow_and_building_sprite(lx, ly, map_building_sprite[type]);
      draw_ocupation_flag(building, lx + 13, ly - 18, 1.f);
      break;
    case Building::TypeFortress:
      draw_shadow_and_building_sprite(lx, ly, map_building_sprite[type]);
      draw_ocupation_flag(building, lx - 12, ly - 21, 0.5f);
      if (building->has_knight()) {
        draw_game_sprite(lx+22, ly - 34 - (building->get_knight_count()+1)/2,
                    182 + (((interface->get_game()->get_tick() >> 3) + 2) & 3) +
                         4 * static_cast<int>(building->get_threat_level()));
      }
      break;
    case Building::TypeGoldSmelter:
      draw_shadow_and_building_sprite(lx, ly, map_building_sprite[type]);
      if (building->is_active()) {
        int i = (interface->get_game()->get_tick() >> 3) & 7;
        if (i == 0 || (i == 7 && !building->is_playing_sfx())) {
          building->start_playing_sfx();
          //play_sound(Audio::TypeSfxGoldBoils);
          play_sound(Audio::TypeSfxGoldBoils, DataSourceType::Amiga);  // the DOS sound is bugged in freeserf, Amiga sound is better anyway
        } else if (i != 7) {
          building->stop_playing_sfx();
        }

        draw_game_sprite(lx-7, ly-33, 128+i);
      }
      break;
    default:
      NOT_REACHED();
      break;
    }
  } else { /* unfinished building */
    if (building->get_type() != Building::TypeCastle) {
      draw_building_unfinished(building, building->get_type(), lx, ly);
    } else {
      draw_shadow_and_building_unfinished(lx, ly, 0xb2,
                                          building->get_progress());
    }
  }
}

void
Viewport::draw_burning_building(Building *building, int lx, int ly) {
  const int building_anim_offset_from_type[] = {
    0, 10, 26, 39, 49, 62, 78, 97, 97, 116,
    129, 157, 167, 198, 211, 236, 255, 277, 305, 324,
    349, 362, 381, 418, 446
  };

  const int building_burn_animation[] = {
    /* Unfinished */
    0, -8, 2,
    8, 0, 4,
    0, 8, 2, -1,

    /* Fisher */
    0, -8, -10,
    0, 4, -12,
    8, -8, 4,
    8, 7, 4,
    0, -2, 8, -1,

    /* Lumberjack */
    0, 3, -13,
    0, -8, -10,
    8, 9, 3,
    8, -6, 3, -1,

    /* Boat builder */
    0, -1, -12,
    8, -8, 11,
    8, 7, 5, -1,

    /* Stone cutter */
    0, 6, -14,
    0, -9, -11,
    8, -8, 5,
    8, 6, 5, -1,

    /* Stone mine */
    8, -4, -40,
    8, -8, -15,
    8, 3, -14,
    8, -9, 4,
    8, 6, 5, -1,

    /* Coal mine */
    8, -4, -40,
    8, -1, -18,
    8, -8, -15,
    8, 6, 2,
    8, 0, 8,
    8, -8, 9, -1,

    /* Iron mine, gold mine */
    8, -4, -40,
    8, -2, -19,
    8, -9, -14,
    8, -8, 2,
    8, 6, 2,
    0, -4, 8, -1,

    /* Forester */
    0, 8, -11,
    0, -6, -8,
    8, -8, 4,
    8, 6, 4, -1,

    /* Stock */
    0, -2, -25,
    0, 6, -17,
    0, -9, -16,
    8, -21, 1,
    8, 21, 2,
    0, 15, 18,
    0, -16, 10,
    8, -8, 15,
    8, 5, 15, -1,

    /* Hut */
    0, 0, -11,
    8, -8, 5,
    8, 8, 5, -1,

    /* Farm */
    8, 22, -2,
    8, 7, -5,
    8, -3, -1,
    8, -23, 0,
    8, -12, 4,
    0, 25, 5,
    0, 21, 13,
    0, -17, 8,
    0, -10, 15,
    0, -2, 15, -1,

    /* Butcher */
    8, -15, 3,
    8, 20, 3,
    8, 7, 3,
    8, -4, 3, -1,

    /* Pig farm */
    8, 0, -2,
    8, 22, 1,
    8, 15, 5,
    8, -20, -1,
    8, -11, 3,
    0, 20, 12,
    0, -16, 7,
    0, -12, 14, -1,

    /* Mill */
    0, 7, -33,
    0, 5, -20,
    8, -2, -24,
    8, -6, 1,
    8, 4, 2,
    0, -3, 6, -1,

    /* Baker */
    0, -15, -16,
    0, -4, -19,
    0, 3, -16,
    8, -13, 2,
    8, -9, 7,
    8, 6, 7,
    0, 17, 1, -1,

    /* Saw mill */
    0, 7, -19,
    0, -1, -14,
    0, 16, -13,
    0, 5, -8,
    8, 14, 4,
    0, 10, 9,
    0, -17, 8,
    8, -11, 10,
    8, -1, 12, -1,

    /* Steel smelter */
    0, 5, -19,
    0, 16, -16,
    8, -14, 2,
    8, -10, 5,
    8, 15, 5,
    8, 2, 5, -1,

    /* Tool maker */
    8, 7, -19,
    0, -11, -17,
    0, -4, -11,
    0, 12, -10,
    8, -20, 0,
    8, -15, 7,
    8, 1, 7,
    8, 15, 7, -1,

    /* Weapon smith */
    8, -15, 1,
    8, -10, 3,
    8, 20, 3,
    8, 5, 3, -1,

    /* Tower */
    0, -6, -30,
    0, 7, -14,
    8, -11, -3,
    0, -8, 4,
    8, 9, 5,
    8, -4, 5, -1,

    /* Fortress */
    0, -3, -30,
    0, -15, -26,
    0, 21, -29,
    0, -13, -17,
    8, 4, -11,
    8, -2, -6,
    8, -22, 0,
    8, -17, 8,
    8, 20, 1,
    8, 10, 8,
    8, 4, 13,
    8, -11, 15, -1,

    /* Gold smelter */
    0, -15, -20,
    0, 10, -22,
    0, -3, -25,
    0, -8, -10,
    0, 7, -10,
    0, -13, 2,
    8, -8, 5,
    8, 6, 5,
    0, 16, 6, -1,

    /* Castle */
    0, 11, -46,
    0, -19, -42,
    8, 1, -27,
    8, 10, -13,
    0, -7, -24,
    8, -16, -6,
    0, -23, 4,
    8, -2, 0,
    8, 12, 12,
    8, -14, 17,
    8, -4, 19,
    0, 13, 19, -1
  };

  /* Play sound effect. */
  if (((building->get_burning_counter() >> 3) & 3) == 3 &&
      !building->is_playing_sfx()) {
    building->start_playing_sfx();
    play_sound(Audio::TypeSfxBurning);
  } else {
    building->stop_playing_sfx();
  }

  uint16_t delta = interface->get_game()->get_tick() - building->get_tick();
  building->set_tick(interface->get_game()->get_tick());

  if (building->get_burning_counter() >= delta) {
    building->decrease_burning_counter(delta);  // TODO(jonls): this is also
                                                // done in update_buildings().
    draw_unharmed_building(building, lx, ly);

    int type = 0;
    if (building->is_done() ||
        building->get_progress() >= 16000) {
      type = building->get_type();
    }

    int offset = ((building->get_burning_counter() >> 3) & 7) ^ 7;
    const int *anim = building_burn_animation +
                      building_anim_offset_from_type[type];
    while (anim[0] >= 0) {
      draw_game_sprite(lx+anim[1], ly+anim[2], 136 + anim[0] + offset);
      offset = (offset + 3) & 7;
      anim += 3;
    }
  } else {
    building->set_burning_counter(0);
  }
}

void
Viewport::draw_building(MapPos pos, int lx, int ly) {
  Building *building = interface->get_game()->get_building_at_pos(pos);

  if (building->is_burning()) {
    draw_burning_building(building, lx, ly);
  } else {
    draw_unharmed_building(building, lx, ly);
  }
}

void
Viewport::draw_water_waves(MapPos pos, int lx, int ly) {
  int sprite = (((pos ^ 5) + (interface->get_game()->get_tick() >> 3)) & 0xf);

  if (map->type_down(pos) <= Map::TerrainWater3 &&
      map->type_up(pos) <= Map::TerrainWater3) {
    frame->draw_waves_sprite(lx - 16, ly, Data::AssetNone, 0,
                             Data::AssetMapWaves, sprite);
  } else if (map->type_down(pos) <= Map::TerrainWater3) {
    frame->draw_waves_sprite(lx, ly + 16, Data::AssetMapMaskDown, 40,
                             Data::AssetMapWaves, sprite);
  } else {
    frame->draw_waves_sprite(lx - 16, ly, Data::AssetMapMaskUp, 40,
                             Data::AssetMapWaves, sprite);
  }
}

void
Viewport::draw_water_waves_row(MapPos pos, int y_base, int cols,
                                 int x_base) {
  for (int i = 0; i < cols; i++, x_base += MAP_TILE_WIDTH,
       pos = map->move_right(pos)) {
    if (map->type_up(pos) <= Map::TerrainWater3 ||
        map->type_down(pos) <= Map::TerrainWater3) {
      /*player->water_in_view += 1;*/

      // option_FogOfWar
      //   do not draw waves outside of shroud/FoW
      if (option_FogOfWar && !map->is_visible(pos, interface->get_player()->get_index())){
        continue;
      }

      draw_water_waves(pos, x_base, y_base);
    }
  }
}

void
Viewport::draw_flag_and_res(MapPos pos, int lx, int ly) {
  Flag *flag = interface->get_game()->get_flag_at_pos(pos);

  // debug - highlight all flags on Debug overlay
  //interface->get_game()->set_debug_mark_pos(pos, "red");

  int res_pos[] = {  6, -4,
                    10, -2,
                    -4, -4,
                    10,  2,
                    -8, -2,
                     6,  4,
                    -8,  2,
                    -4,  4 };

  for (unsigned int i = 0; i < 3; i++) {
    if (flag->get_resource_at_slot(i) != Resource::TypeNone) {
      draw_game_sprite(lx + res_pos[i*2], ly + res_pos[i * 2 + 1],
                       flag->get_resource_at_slot(i) + 1);
    }
  }

  int pl_num = flag->get_owner();
  // seeing exceptions when loading a game and switching players where
  //  ths player index here is -1 and so the get_player_color call fails
  //  added a work-around to get_player_color because I am not sure why
  //  a flag owner is being set to invalid.   Might be a save/load thing?
  // Also, check and throw error here
  if (pl_num < 0 || pl_num > 3){
    Log::Error["viewport.cc"] << "inside Viewpot::draw_flag_and_res for MapPos "<< pos << ", flag->get_owner returned invalid player index " << pl_num << "!  get_player_color will return 'black' as a fallback to avoid crashing";
  }
  Color player_color = interface->get_player_color(pl_num);

  int spr = 0x80 + ((interface->get_game()->get_tick() >> 3) & 3);

  draw_shadow_and_building_sprite(lx, ly, spr, player_color);

  for (unsigned int i = 3; i < 8; i++) {
    if (flag->get_resource_at_slot(i) != Resource::TypeNone) {
      draw_game_sprite(lx + res_pos[i * 2], ly + res_pos[i * 2 + 1],
        flag->get_resource_at_slot(i) + 1);
    }
  }
}

void
//Viewport::draw_map_objects_row(MapPos pos, int y_base, int cols, int x_base) {
Viewport::draw_map_objects_row(MapPos pos, int y_base, int cols, int x_base, int ly) {
  //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row";
  // for determining "in view" objects for ambient sound generation,
  //  only consider a roughly 640x480px area in the center of the 
  //  viewport, otherwise there are too many ambient sounds and for
  //  things such as deserts, water, that are near edges of viewport
  // note that X is in cols, and Y is in pixels, because it is easier to code this way
  int focus_cols = 18;
  int center_col = cols / 2;
  int leftmost_focus_col = center_col - (focus_cols / 2);
  int rightmost_focus_col = center_col + (focus_cols / 2);
  int focus_y_pixels = 480;
  int center_y = height / 2;
  int topmost_focus_y = center_y - (focus_y_pixels / 2);
  int lowest_focus_y = center_y + (focus_y_pixels / 2);

  bool darken = false;

  //Log::Debug["viewport.cc"] << "inside draw_map_objects, pos " << pos << ", cols: " << cols << ", focus_cols: " << focus_cols << ", center_col: " << center_col << ", left: " << leftmost_focus_col << ", right: " << rightmost_focus_col;
  //Log::Debug["viewport.cc"] << "inside draw_map_objects, pos " << pos << ", map height: " << height << ", ly: " << ly << ", center_y: " << center_y << ", top: " << topmost_focus_y << ", bottom: " << lowest_focus_y;
  for (int i = 0; i < cols;
       i++, x_base += MAP_TILE_WIDTH, pos = map->move_right(pos)) {

    if (map->get_obj(pos) == Map::ObjectNone){continue;}  // comment this out if trying to draw red dot overlay to show focus area

    // option_FogOfWar
    //   do not draw objects outside of shroud/FoW
    if ( option_FogOfWar && !map->is_visible(pos, interface->get_player()->get_index()) ){
      // EXCEPT - always draw any Castle that has been revealed, even if it is not currently visible
      if ( map->get_obj(pos) != Map::ObjectCastle && map->is_revealed(pos, interface->get_player()->get_index()) ){
        // draw as usual, do not continue
      }
      continue;
    }

    int ly = y_base - 4 * map->get_height(pos);
    bool in_ambient_focus = false;
    if (i >= leftmost_focus_col && i <= rightmost_focus_col && ly >= topmost_focus_y && ly <= lowest_focus_y){
      // debug - show area in focus for ambient sounds with red dots
      //   note for this to show a full rectangle the "continue if ObjectNone" must be moved to BELOW this if statement
      //frame->fill_rect(x_base - 7, ly + 0, 6, 6, colors.at("red"));
      //frame->fill_rect(x_base - 9, ly + 1, 12, 12, colors.at("red"));
      in_ambient_focus = true;
    }
    //if (map->get_obj(pos) == Map::ObjectNone) continue;  // uncomment this if trying to draw red dot overlay to show focus area
    if (map->get_obj(pos) < Map::ObjectTree0) {
      if (map->get_obj(pos) == Map::ObjectFlag) {
        draw_flag_and_res(pos, x_base, ly);
      } else if (map->get_obj(pos) <= Map::ObjectCastle) {
        draw_building(pos, x_base, ly);
      }
    } else {
      int sprite = map->get_obj(pos) - Map::ObjectTree0;  // THIS IS IMPORTANT - sprite index is always 8 lower (-8) than map_object index!
      bool use_custom_set = false;  // messing with weather/seasons/palette tiles
      // if this is some kind of tree...
      if (sprite < 24) {  // Tree/Pine/Palm/SubmergedTree are sprites <24, these have waving animations

        // ambient sound triggers, only consider objects that are in the focus area
        if (in_ambient_focus){
          /* Trees */
          // only allow bird chirps from Tree and Pine (not palm or submerged, cactus, etc.)
          if (map->get_obj(pos) >= Map::ObjectTree0 && map->get_obj(pos) <= Map::ObjectPine7) {
            interface->trees_in_view += 1;
            //Log::Info["viewport.cc"] << "trees in view: " << interface->trees_in_view;
          }
          // use junk objects that only appear in deserts to trigger desert wind sounds
          //  cannot use desert terrain in view because landscape is not constantly redrawn
          if (map->get_obj(pos) == Map::ObjectCactus0   || map->get_obj(pos) == Map::ObjectCactus1
          || map->get_obj(pos) == Map::ObjectCadaver0  || map->get_obj(pos) == Map::ObjectCadaver1
          || (map->get_obj(pos) >= Map::ObjectPalm0 && map->get_obj(pos) <= Map::ObjectPalm3)) {
            interface->desert_in_view += 1;
            //Log::Info["viewport.cc"] << "desert junk objects in view: " << interface->desert_in_view;
          }
          // there's a bug with water tile map generation, seems only one water tree is rendered
          //  and always top row, top left??  include all anyway in case I eventually fix that
          if ((map->get_obj(pos) >= Map::ObjectWaterTree0 && map->get_obj(pos) <= Map::ObjectWaterTree3)
            || map->get_obj(pos) >= Map::ObjectWaterStone0 || map->get_obj(pos) >= Map::ObjectWaterStone1) {
            interface->water_in_view += 1;
            //Log::Info["viewport.cc"] << "water junk objects in view: " << interface->water_in_view;
          }
        }

        // Adding sprite number to animation ensures
        //   that the tree animation won't be synchronized
        //   for all trees on the map.  
        //  WAIT A MINUTE - IT SURE SEEMS LIKE ALL TREES *ARE* SYNCED ON THE MAP, IS THIS A MISTAKE?
        //  COMPARE TO ORIGINAL GAME.   Serflings is synched too.  I think the Freeserf comment is wrong.  
        //
        // NOTE  -  this is where the wind-waving Tree animation effect happens for ObjectTree0 through ObjectPine7
        //
        // tree_anim is basically just a slower version of game tick, the bit shift right effectively reduces the rate of increment
        // This is done because if the animation moved as fast as the tick rate it would be way too fast.  
        // This means that something like >>5 should result in a slower rate, which is just what I need for winter trees!
        int tree_anim = (interface->get_game()->get_tick() + sprite) >> 4;
        //Log::Info["viewport"] << "bitmath debug, sprite " << sprite << ", tick " << interface->get_game()->get_tick() << ", tree_anim " << tree_anim;
        int slow_tree_anim = (interface->get_game()->get_tick() + sprite) >> 5;  // YES this works perfectly!  it advances at about 40% of the >>4 rate
        //Log::Info["viewport"] << "bitmath debug, sprite " << sprite << ", tick " << interface->get_game()->get_tick() << ", tree_anim " << tree_anim << ", slow_tree_anim " << slow_tree_anim;
        // IDEA - could vary the wind speed by changing the shift rate!

        // to support custom sprites and "crammed" animation, a numbering system is used
        // where each season has a set of trees, each with their own animation set
        //
        // NORMALLY, the game has Tree (and Pine, and Palm and SubmergedTree) numbers as map objects
        //  and to animate them the spite drawn is an offset of the base '0' object for that set
        //  and only that one set of animation exists for each tree type (it cycles through the sprites as frames)
        //
        // for FourSeasons, the number of unique trees of each type (Tree/Pine/etc)is limited to the
        //  number of the original type/frames (8xTree,8xPine,4xPalm,4xSubmergedTree), however the 
        //  animation frames for each is unlimited.  For now I have used a numbering system that limits 
        //  frame count to <=9 but the numbering system could be changed to allow any number of frames per tree type.  
        //
        // #... is season_sprite_offset (NOT same as 'season' because they are out of order, offset are
        //          Spring = 1200, Summer = 000 (none), Fall = 1400, Winter = 1300
        // ..#. is tree#, 0-3 for some, 0-7 for others
        // ...# is frame#, 0-3 for some, 0-7 for others
        // so for example, Fall Tree#5 with 8 frames of animation is index 450 through 457

        int tree = 10*sprite;
        int frame = 0;
        // spring 2nd transition (to summer/full foliage) order
              // 1whiteflower
              // 2pinkflower
              // 0tree
              // 3tree
              // 4tree
              // .. and so on
        // THIS WILL NOT WORK THIS WAY!! NEED TO REORDER ACTUAL SPRITES
        // NEED TO RENAME both spring AND winter TREES SO THAT
        //  1whiteflower and 2pinkflower are tree5+ 
        // and then somehow get them to become full-summer-leaves in reverse order??
        //  maybe inverse the subseason >7 so 8 affects tree7, 9 affects tree6, and so on
        //int spring_2nd_order[8] = { 10, 20, 0, 30, 40, 50, 60, 70};
        // fall tree# colors:
              // Tree 0 greenyellow
              // Tree10 yellow
              // Tree20 orangeyellow
              // Tree30 orangered
              // Tree40 redbright
              // Tree50 reddull
              // Tree60 magenta
              // Tree70 brown
        // fall transition pattern:
              ///summer -> fall 1st -> fall 2nd
              // green->0greenyellow->2orangeyellow
              // green->1yellow->3orangered
              // green->2orangeyellow->5reddull
              // green->6magenta->2orangeyellow
              // green->1yellow->7brown
              // green->0greenyellow->3orangered
              // green->5reddull->7brown
              // green->4redbright->5reddull
        int fall_1st_colors[8] = {  0, 10, 20, 60, 10,  0, 50, 40};              
        int fall_2nd_colors[8] = { 20, 30, 50, 20, 70, 30, 70, 50};              
        if (sprite < 8){
          // these are deciduous trees, apply special FourSeasons rules
          if (option_FourSeasons){
            switch (season) {
            case 0:  // SPRING
              if (subseason*10 > tree + 80){
                // use summer/normal coloration for this Tree#
                sprite = (sprite & ~7) + (tree_anim & 7);  // 8 frames of animation
              }else if (subseason*10 > tree){
                // use early-spring coloration for this Tree#
                frame = (sprite & ~7) + (slow_tree_anim & 3);  // spring trees have 8 types, two sets per type, each with 4 frames of animation
                sprite = season_sprite_offset[season] + tree + frame;
                use_custom_set = true;
              }else{
                // use previous (winter) coloration for this Tree#.  It is important that the Tree#s are synced between winter & spring because the trunks need to match!!
                frame = (sprite & ~7) + (slow_tree_anim & 3);  // winter trees have 8 types, each with 4 frames of animation
                sprite = season_sprite_offset[3] + tree + frame;
                use_custom_set = true;
              }
              break;
            case 1:  // SUMMER
              // continue using normal/summer green shifting method, no custom_set
              sprite = (sprite & ~7) + (tree_anim & 7);  // 8 frames of animation
              break;
            case 2:  // FALL
              frame = (sprite & ~7) + (tree_anim & 7);  // 8 frames of animation
              
              if (subseason*10 > tree + 80){
                // use second fall color
                tree = fall_2nd_colors[tree/10];
                sprite = season_sprite_offset[season] + tree + frame;
                use_custom_set = true;
              }else if (subseason*10 > tree){  // if subseason is 0, no tree changes yet
                // use first fall color
                tree = fall_1st_colors[tree/10];
                sprite = season_sprite_offset[season] + tree + frame;
                use_custom_set = true;
              }else{
                // continue using normal/summer green for this Tree#, no custom_set
                sprite = (sprite & ~7) + (tree_anim & 7);  // 8 frames of animation
              }
              break;
            case 3:  // WINTER
              // FIGURE OUT HOW TO SLOW DOWN THE WINTER TREE ANIMATIONS 2-3x
              if (subseason*10 > tree){  // if subseason is 0, no tree changes yet.  trees all lose leaves in first half of 16 subseasons
                // use winter coloration for this Tree#
                frame = (sprite & ~7) + (slow_tree_anim & 3);  // winter trees have 8 types, each with 4 frames of animation
                sprite = season_sprite_offset[season] + tree + frame;
              }else{
                // use previous (fall 2nd set) coloration for this Tree#.  Because the fall trunks are not visible, it doesn't matter if the tree#s are swapped here
                tree = fall_2nd_colors[tree/10];
                frame = (sprite & ~7) + (tree_anim & 7);  // fall trees have 8 types, each with 8 frames of animation
                sprite = season_sprite_offset[2] + tree + frame;
              }
              use_custom_set = true;  // winter always uses custom_set, for winter and fall trees shown
              break;
            default:
              NOT_REACHED();
              break;
            } // end case-switch (season)
          }else{  // if option_FourSeasons
            // if option_FourSeasons is NOT on, apply normal shift rules
            sprite = (sprite & ~7) + (tree_anim & 7);
          }
        }else if (sprite >= 8 && sprite < 16) {
          // these are pine trees, shift.  8 frames of animation
          sprite = (sprite & ~7) + (tree_anim & 7); 
        }else if (sprite >=16){
          // these are ... palm and submerged trees.  4 frames of animation per type
          sprite = (sprite & ~3) + (tree_anim & 3);
        }

      } // if sprite < 24 (various tree types)

      /* disabling this, instead modifying the rgb palette color of the original sprites

      // FourSeasons - in WINTER make Seed# objects (immature fields) appear as spent fields
      //   though they are just dormant and will revert to their original graphics in spring
      //   THIS IS SOLELY BECAUSE THE Seeds0/1/2 are too GREEN and it clashes
      //  a much better approach is to set a custom graphic that is less green but looks like Seed0
      //   otherwise and use that for all new fields
      // ObjectSeeds0, // #105  - 8
      // ObjectSeeds1,
      // ObjectSeeds2,
      // ObjectSeeds3,  <-- these are mature enough to not be "seeds" anymore, leave these
      // ObjectSeeds4,
      // ObjectSeeds5, // #110  - 8
      // ObjectFieldExpired, // #111   - 8
      if (option_FourSeasons && season == 3){
        // REMEMBER THAT
        //  'sprite' is reduced by 8 / Map::ObjectTree0 earlier in this function because 0-7 are some placeholders?
        if ((sprite >= Map::ObjectSeeds0 - Map::ObjectTree0) && (sprite <= Map::ObjectSeeds2 - Map::ObjectTree0)){  // a bit more snow on mountains in winter
          Log::Debug["viewport.cc"] << "using alternate sprite 111 ObjectFieldExpired for ObjectSeeds sprite " << sprite;
          sprite = Map::ObjectFieldExpired - Map::ObjectTree0;
        }else{
          Log::Debug["viewport.cc"] << "using original " << sprite << " for non-ObjectSeeds object type #" << sprite;
        }
      } 
      */

      //int pos_luminosity = 0;
      //int pos_luminosity = get_brighter_triangle_luminosity(pos);
      //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, pos " << pos << ", obj type " << map->get_obj(pos) << ", pos_luminosity is " << pos_luminosity; 

      // normal bright Flowers
      bool flower = false;  // dumb work-around to avoid being foild by sprite += 1000
      //if (( sprite >= Map::ObjectFlowerGroupA0 - Map::ObjectTree0 && sprite <= Map::ObjectFlowerGroupA6 - Map::ObjectTree0 )
      //||  ( sprite >= Map::ObjectFlowerGroupB0 - Map::ObjectTree0 && sprite <= Map::ObjectFlowerGroupB6 - Map::ObjectTree0 ) ){
      if ( ( sprite >= Map::ObjectFlowerGroupA0 - Map::ObjectTree0 && sprite <= Map::ObjectFlowerGroupC6 - Map::ObjectTree0 ) ){
        if (option_FourSeasons && season == 0){
          sprite += 1000;
          //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, found a flower, using sprite# + 1000, new sprite #" << sprite;
          use_custom_set = true;
          flower = true;
        }else{
          continue;
        }
      }

      // check pos two left and one right
      int left2  = map->get_height(map->move_left(map->move_left(pos)));
      //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, checking terr height, left2 with pos " << map->move_left(map->move_left(pos)) << " has height " << left2;
      int left1  = map->get_height(map->move_left(pos));
      //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, checking terr height, left1 with pos " << map->move_left(pos) << " has height " << left1;
      int here   = map->get_height(pos);
      //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, checking terr height, cur pos " << pos << " has height " << map->get_height(pos);
      int right1 = map->get_height(map->move_right(pos));
      //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, checking terr height, right1 with pos " << map->move_right(pos) << " has height " << right1;
      int downleft1 = map->get_height(map->move_down(pos));
      //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, checking terr height, downleft1 with pos " << map->move_down(pos) << " has height " << downleft1;

      // I have categorized all sprites into Short or Tall based on their appearance
      //  short sprites have lower requirements to become shaded
      //  taller sprites have higher requirement to be shaded
      //  Currently, short == 0 and tall == 1
      int obj_sprite_height = Map::obj_height_for_slope_darken[map->get_obj(pos)];
      //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, pos " << pos << ", obj type " << map->get_obj(pos) << ", obj_sprite_height for sprite #" << sprite << " is " << obj_sprite_height; 


      bool shaded = false;

      while (true){
        if (right1 > here){
          //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, checking terr height, cur pos " << pos << ", rejected because right1-here is higher than here";
          break;
        }
        if (downleft1 < here){
          //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, checking terr height, cur pos " << pos << ", rejected because downleft1-here is lower than here";
          break;
        }
        if (left1 < here){
          //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, checking terr height, cur pos " << pos << ", rejected because left1 is lower than here";
          break;
        }
        if (left1 - here > obj_sprite_height){
          //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, checking terr height, cur pos " << pos << ", accepted because left1-here is > obj_sprite_height";
          shaded = true;
          break;
        }
        if ( (left2 - left1) / 3   // pos left2 has reduced contribution to accumulated decline
           +  left1 - here         // pos left has full contribution
                > obj_sprite_height ){
          //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, checking terr height, cur pos " << pos << ", accepted because acumulated height over threshold";
          shaded = true;
          break;
        }
        //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, checking terr height, cur pos " << pos << ", no conditions were true. leaving shaded at " << shaded;
        break;
      }

      darken = false;
  
      if (shaded){

        //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, shaded is true";
        
        // shaded Flowers    // can't use MapObject at this point because it was earlier set to +1000 for flowers
        //if (( sprite >= Map::ObjectFlowerGroupA0 - Map::ObjectTree0 && sprite <= Map::ObjectFlowerGroupA6 - Map::ObjectTree0 )
        //||  ( sprite >= Map::ObjectFlowerGroupB0 - Map::ObjectTree0 && sprite <= Map::ObjectFlowerGroupB6 - Map::ObjectTree0 ) ){
        if (flower){
          sprite += 1000;
          //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, found a darker flower, using sprite# + 2000, new sprite #" << sprite;
          use_custom_set = true;
        //}else{
          //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, not a flower";
        }

        // shaded existing map objects
        if ((sprite >= Map::ObjectTree0 - Map::ObjectTree0) && (sprite <= Map::ObjectField5 - Map::ObjectTree0)){
          //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, found existing map objects on downward left->right slopes (darken)";
          // DO NOT use_custom_set, these are not custom PNGs but mutated original sprites from SPAx.PA
          darken = true;
        }

      }

      if (use_custom_set){
        //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, calling draw_map_sprite_special()";
        draw_map_sprite_special(x_base, ly, sprite, pos, map->get_obj(pos));
      }else{
        //Log::Debug["viewport.cc"] << "inside Viewport::draw_map_objects_row, calling draw_shadow_and_building_sprite(), darken bool is " << darken;
        //draw_shadow_and_building_sprite(x_base, ly, sprite);
        draw_shadow_and_building_sprite(x_base, ly, sprite, Color::transparent, darken);
      }

    } // if not a Tree or junk object
  } // foreach column in this row
} // end Viewport::draw_map_objects_row

/* Draw one individual serf in the row. */
void
Viewport::draw_row_serf(int lx, int ly, bool shadow, const Color &color,
                        int body) {
  const int index1[] = {
    0, 0, 48, 6, 96, -1, 48, 24,
    240, -1, 48, 30, 248, -1, 48, 12,
    48, 18, 96, 306, 96, 300, 48, 54,
    48, 72, 48, 36, 0, 48, 272, -1,
    48, 60, 264, -1, 48, 42, 280, -1,
    48, 66, 96, 312, 500, 600, 48, 318,
    48, 78, 0, 84, 48, 90, 48, 96,
    48, 102, 48, 108, 48, 114, 96, 324,
    96, 330, 96, 336, 96, 342, 96, 348,
    48, 354, 48, 360, 48, 366, 48, 372,
    48, 378, 48, 384, 504, 604, 509, -1,
    48, 120, 288, -1, 288, 420, 48, 126,
    48, 132, 96, 426, 0, 138, 304, -1,
    48, 390, 48, 144, 96, 432, 48, 198,
    510, 608, 48, 204, 48, 402, 48, 150,
    96, 438, 48, 156, 312, -1, 320, -1,
    48, 162, 48, 168, 96, 444, 0, 174,
    513, -1, 48, 408, 48, 180, 96, 450,
    0, 186, 520, -1, 48, 414, 48, 192,
    96, 456, 328, -1, 48, 210, 344, -1,
    48, 6, 48, 6, 48, 216, 528, -1,
    48, 534, 48, 528, 48, 288, 48, 282,
    48, 222, 533, -1, 48, 540, 48, 546,
    48, 552, 48, 558, 48, 564, 96, 468,
    96, 462, 48, 570, 48, 576, 48, 582,
    48, 396, 48, 228, 48, 234, 48, 240,
    48, 246, 48, 252, 48, 258, 48, 264,
    48, 270, 48, 276, 96, 474, 96, 480,
    96, 486, 96, 492, 96, 498, 96, 504,
    96, 510, 96, 516, 96, 522, 96, 612,
    144, 294, 144, 588, 144, 594, 144, 618,
    144, 624, 401, 294, 352, 297, 401, 588,
    352, 591, 401, 594, 352, 597, 401, 618,
    352, 621, 401, 624, 352, 627, 450, -1,
    192, -1
  };  // there are 274 items in this array

  const int index2[] = {
    0, 0, 1, 0, 2, 0, 3, 0,
    4, 0, 5, 0, 6, 0, 7, 0,
    8, 1, 9, 1, 10, 1, 11, 1,
    12, 1, 13, 1, 14, 1, 15, 1,
    16, 2, 17, 2, 18, 2, 19, 2,
    20, 2, 21, 2, 22, 2, 23, 2,
    24, 3, 25, 3, 26, 3, 27, 3,
    28, 3, 29, 3, 30, 3, 31, 3,
    32, 4, 33, 4, 34, 4, 35, 4,
    36, 4, 37, 4, 38, 4, 39, 4,
    40, 5, 41, 5, 42, 5, 43, 5,
    44, 5, 45, 5, 46, 5, 47, 5,
    0, 0, 1, 0, 2, 0, 3, 0,
    4, 0, 5, 0, 6, 0, 2, 0,
    0, 1, 1, 1, 2, 1, 3, 1,
    4, 1, 5, 1, 6, 1, 2, 1,
    0, 2, 1, 2, 2, 2, 3, 2,
    4, 2, 5, 2, 6, 2, 2, 2,
    0, 3, 1, 3, 2, 3, 3, 3,
    4, 3, 5, 3, 6, 3, 2, 3,
    0, 0, 1, 0, 2, 0, 3, 0,
    4, 0, 5, 0, 6, 0, 7, 0,
    8, 0, 9, 0, 10, 0, 11, 0,
    12, 0, 13, 0, 14, 0, 15, 0,
    16, 0, 17, 0, 18, 0, 19, 0,
    20, 0, 21, 0, 22, 0, 23, 0,
    24, 0, 25, 0, 26, 0, 27, 0,
    28, 0, 29, 0, 30, 0, 31, 0,
    32, 0, 33, 0, 34, 0, 35, 0,
    36, 0, 37, 0, 38, 0, 39, 0,
    40, 0, 41, 0, 42, 0, 43, 0,
    44, 0, 45, 0, 46, 0, 47, 0,
    48, 0, 49, 0, 50, 0, 51, 0,
    52, 0, 53, 0, 54, 0, 55, 0,
    56, 0, 57, 0, 58, 0, 59, 0,
    60, 0, 61, 0, 62, 0, 63, 0,
    64, 0
  };  // there are 290 items in this array

  /* Shadow */
  if (shadow) {
    //frame->draw_sprite(lx, ly, Data::AssetSerfShadow, 0, true);
    frame->draw_sprite(lx, ly, Data::AssetSerfShadow, 0, true, Color::transparent, 1.f);
  }

  int hi = ((body >> 8) & 0xff) * 2;
  int lo = (body & 0xff) * 2;

  int base = index1[hi];
  int head = index1[hi+1];
  //Log::Debug["viewport.cc"] << "inside Viewport::draw_row_serfA, body = " << body << ", head = " << head << ", lo = " << lo << ", hi = " << hi << ", base = " << base;

  // if serf has a separate head (some torso sprites include head, such as active working serfs, look it up
  if (head < 0) {
    // for "torso only" sprites where head is already included in the torso sprite
    //  for animations that only one serf type can do (ex. digging, building, harvesting, fishing)
    base += index2[lo];
  } else {
    // for sprites with separate torso + head (for animations that any serf type can do (ex. walking, carrying)
    base += index2[lo];
    head += index2[lo+1];
  }

  //Log::Debug["viewport.cc"] << "inside Viewport::draw_row_serfB, body = " << body << ", head = " << head << ", lo = " << lo << ", hi = " << hi << ", base = " << base;

  draw_serf(lx, ly, color, head, base);
}

/* Extracted from obsolete update_map_serf_rows(). */
/* Translate serf type into the corresponding sprite code. */
int
Viewport::serf_get_body(Serf *serf) {
  /*
  const int transporter_type[] = {
    0, 0x3000, 0x3500, 0x3b00, 0x4100, 0x4600, 0x4b00, 0x1400,
    0x700, 0x5100, 0x800, 0x1c00, 0x1d00, 0x1e00, 0x1a00, 0x1b00,
    0x6800, 0x6d00, 0x6500, 0x6700, 0x6b00, 0x6a00, 0x6600, 0x6900,
    0x6c00, 0x5700, 0x5600, 0, 0, 0, 0, 0
  };

  // need to add more zeros here for option_CanTransportSerfsInBoats
  const int sailor_type[] = {
    0, 0x3100, 0x3600, 0x3c00, 0x4200, 0x4700, 0x4c00, 0x1500,
    0x900, 0x7700, 0xa00, 0x2100, 0x2200, 0x2300, 0x1f00, 0x2000,
    0x6e00, 0x6f00, 0x7000, 0x7100, 0x7200, 0x7300, 0x7400, 0x7500,
    0x7600, 0x5f00, 0x6000, 0, 0, 0, 0, 0
  };
  */

  // extra zeros for added fake Resource types 
  const int transporter_type[] = {
    0, 0x3000, 0x3500, 0x3b00, 0x4100, 0x4600, 0x4b00, 0x1400,
    0x700, 0x5100, 0x800, 0x1c00, 0x1d00, 0x1e00, 0x1a00, 0x1b00,
    0x6800, 0x6d00, 0x6500, 0x6700, 0x6b00, 0x6a00, 0x6600, 0x6900,
    0x6c00, 0x5700, 0x5600, 0, 0, 0, 0, 0, 0, 0
  };

  const int sailor_type[] = {
    0, 0x3100, 0x3600, 0x3c00, 0x4200, 0x4700, 0x4c00, 0x1500,
    0x900, 0x7700, 0xa00, 0x2100, 0x2200, 0x2300, 0x1f00, 0x2000,
    0x6e00, 0x6f00, 0x7000, 0x7100, 0x7200, 0x7300, 0x7400, 0x7500,
    0x7600, 0x5f00, 0x6000, 0, 0, 0, 0, 0, 0, 0
  };

  Data::Animation animation = data_source->get_animation(serf->get_animation(), serf->get_counter());
  int t = animation.sprite;  // it seems that the sprite values show in the animation table in FSStudio may be
                             //  adjusted and are not always literally mapped to the serf_torso sprite # you might expect!
                             //  The 't' value here is the serf_torso base/body

  switch (serf->get_type()) {
  case Serf::TypeTransporter:
  case Serf::TypeGeneric:
    if (serf->get_state() == Serf::StateIdleOnPath) {
      return -1;
    } else if ((serf->get_state() == Serf::StateTransporting ||
                serf->get_state() == Serf::StateDelivering) &&
               serf->get_delivery() != 0) {
      t += transporter_type[serf->get_delivery()];
    }
    break;
  case Serf::TypeSailor:
    if (serf->get_state() == Serf::StateTransporting && t < 0x80) {
      if (((t & 7) == 4 && !serf->playing_sfx()) ||
          (t & 7) == 3) {
        serf->start_playing_sfx();
        //play_sound(Audio::TypeSfxRowing);
        play_sound(Audio::TypeSfxRowing, DataSourceType::DOS);  // DOS sound is a little better
      } else {
        serf->stop_playing_sfx();
      }
    }

    if ((serf->get_state() == Serf::StateTransporting &&
         serf->get_delivery() == 0) ||
        serf->get_state() == Serf::StateLostSailor ||
        serf->get_state() == Serf::StateFreeSailing) {
      if (t < 0x80) {
        if (((t & 7) == 4 && !serf->playing_sfx()) ||
            (t & 7) == 3) {
          serf->start_playing_sfx();
          //play_sound(Audio::TypeSfxRowing);
          play_sound(Audio::TypeSfxRowing, DataSourceType::DOS);  // DOS sound is a little better
        } else {
          serf->stop_playing_sfx();
        }
      }
      t += 0x200;
    } else if (serf->get_state() == Serf::StateTransporting) {
      // add support for option_CanTransportSerfsInBoats
      /// need to figure out this -1 +1 resource type stuff
      if (serf->get_delivery() >= Resource::TypeSerf - 1){
        // set the 0x200 "rowing empty boat" animation
        //Log::Info["viewport"] << "debug: sailor is transporting a serf, setting default +0x200 animation";
        t += 0x200;
      } else {
        // I think this sets the animation/sprite to the right transported resource?
        //  yes, that looks right.  Try simply adding another ,0,0 to that list
        //  for now, try this hack
        //Log::Info["viewport"] << "debug: sailor is NOT transporting a serf, change animation to match the resource type";
        t += sailor_type[serf->get_delivery()];
      }
      // try this way, now that extra zeroes added for fake resource types 26+
      // nope this doesn't work... hmm figure it out later
      //t += sailor_type[serf->get_delivery()];
    } else if (option_CanTransportSerfsInBoats && serf->get_state() == Serf::StateWalking && map->is_in_water(serf->get_pos())) {
      // if this is a Sailor that is traversing an existing water path that may be worked by another sailor, on way to reach
      //  this new sailor's water path destination!  Show this sailor as padding animation otherwise he will walk on water
      // Do NOT want sailors to become passengers in other sailors' boats at this time
      // copying this animation/sound stuff from the Transporting section, could instead move this logic into there to avoid duplication
      if (t < 0x80) {
        if (((t & 7) == 4 && !serf->playing_sfx()) ||
            (t & 7) == 3) {
          serf->start_playing_sfx();
          //play_sound(Audio::TypeSfxRowing);
          play_sound(Audio::TypeSfxRowing, DataSourceType::DOS);  // DOS sound is a little better
        } else {
          serf->stop_playing_sfx();
        }
      }
      t += 0x200;
    } else {
      // normal walking?
      t += 0x100;
    }
    break;
  case Serf::TypeDigger:
    if (t < 0x80) {
      t += 0x300;
    } else if (t == 0x83 || t == 0x84) {
      if (t == 0x83 || !serf->playing_sfx()) {
        serf->start_playing_sfx();
        play_sound(Audio::TypeSfxDigging);
      }
      t += 0x380;
    } else {
      serf->stop_playing_sfx();
      t += 0x380;
    }
    // note - the 'base' aka 'torso' number for the Digger doing actual digging
    //  is 1024 through 1031, which are mapped in hi/lo/index1 to become serf_torso sprites #240-247
    //  and the animation is animation #87 though it doesn't seem to be the right number of steps
    //  I wonder if the animation # is also offset somewhere and it isn't truly animiation#87 frames
    break;
  case Serf::TypeBuilder:
    if (t < 0x80) {
      t += 0x500;
    } else if ((t & 7) == 4 || (t & 7) == 5) {
      if ((t & 7) == 4 || !serf->playing_sfx()) {
        serf->start_playing_sfx();
        play_sound(Audio::TypeSfxHammerBlow);
      }
      t += 0x580;
    } else {
      serf->stop_playing_sfx();
      t += 0x580;
    }
    break;
  case Serf::TypeTransporterInventory:
    if (serf->get_state() == Serf::StateBuildingCastle) {
      return -1;
    } else {
      int res = serf->get_delivery();
      t += transporter_type[res];
    }
    break;
  case Serf::TypeLumberjack:
    if (t < 0x80) {
      if (serf->get_state() == Serf::StateFreeWalking &&
          serf->get_free_walking_neg_dist1() == -128 &&
          serf->get_free_walking_neg_dist2() == 1) {
        t += 0x1000;
      } else {
        t += 0xb00;
      }
    } else if ((t == 0x86 && !serf->playing_sfx()) ||
         t == 0x85) {
      serf->start_playing_sfx();
      play_sound(Audio::TypeSfxAxBlow);
      /* TODO Dangerous reference to unknown state vars.
         It is probably free walking. */
      if (serf->get_free_walking_neg_dist2() == 0 &&
          serf->get_counter() < 64) {
        //play_sound(Audio::TypeSfxTreeFall);
        play_sound(Audio::TypeSfxTreeFall, DataSourceType::DOS);  // DOS sound is better
      }
      t += 0xe80;
    } else if (t != 0x86) {
      serf->stop_playing_sfx();
      t += 0xe80;
    }
    break;
  case Serf::TypeSawmiller:
    if (t < 0x80) {
      if (serf->get_state() == Serf::StateLeavingBuilding &&
          serf->get_leaving_building_next_state() ==
            Serf::StateDropResourceOut) {
        t += 0x1700;
      } else {
        t += 0xc00;
      }
    } else {
      /* player_num += 4; ??? */
      if (t == 0xb3 || t == 0xbb || t == 0xc3 || t == 0xcb ||
          (!serf->playing_sfx() && (t == 0xb7 || t == 0xbf ||
                t == 0xc7 || t == 0xcf))) {
        serf->start_playing_sfx();
        //play_sound(Audio::TypeSfxSawing);
        //play_sound(Audio::TypeSfxSawing, DataSourceType::Amiga);  // Amiga sound is better for sawmill
        //  actually, I am now thinking the Amiga sound is too grating, trying DOS again..
        play_sound(Audio::TypeSfxSawing, DataSourceType::DOS);
      } else if (t != 0xb7 && t != 0xbf && t != 0xc7 && t != 0xcf) {
        serf->stop_playing_sfx();
      }
      t += 0x1580;
    }
    break;
  case Serf::TypeStonecutter:
    if (t < 0x80) {
      if ((serf->get_state() == Serf::StateFreeWalking &&
           serf->get_free_walking_neg_dist1() == -128 &&
           serf->get_free_walking_neg_dist2() == 1) ||
          (serf->get_state() == Serf::StateStoneCutting &&
           serf->get_free_walking_neg_dist1() == 2)) {
        t += 0x1200;
      } else {
        t += 0xd00;
      }
    } else if (t == 0x85 || (t == 0x86 && !serf->playing_sfx())) {
      serf->start_playing_sfx();
      play_sound(Audio::TypeSfxPickBlow);
      t += 0x1280;
    } else if (t != 0x86) {
      serf->stop_playing_sfx();
      t += 0x1280;
    }
    break;
  case Serf::TypeForester:
    if (t < 0x80) {
      t += 0xe00;
    } else if (t == 0x86 || (t == 0x87 && !serf->playing_sfx())) {
      serf->start_playing_sfx();
      play_sound(Audio::TypeSfxPlanting);
      t += 0x1080;
    } else if (t != 0x87) {
      serf->stop_playing_sfx();
      t += 0x1080;
    }
    break;
  case Serf::TypeMiner:
    if (t < 0x80) {
      if ((serf->get_state() != Serf::StateMining ||
           serf->get_mining_res() == 0) &&
          (serf->get_state() != Serf::StateLeavingBuilding ||
           serf->get_leaving_building_next_state() !=
           Serf::StateDropResourceOut)) {
        t += 0x1800;
      } else {
        Resource::Type res = Resource::TypeNone;

        switch (serf->get_state()) {
        case Serf::StateMining:
          res = (Resource::Type)(serf->get_mining_res() - 1);
          break;
        case Serf::StateLeavingBuilding:
          res = (Resource::Type)(serf->get_leaving_building_field_B() - 1);
          break;
        default:
          NOT_REACHED();
          break;
        }

        switch (res) {
          case Resource::TypeStone: t += 0x2700; break;
          case Resource::TypeIronOre: t += 0x2500; break;
          case Resource::TypeCoal: t += 0x2600; break;
          case Resource::TypeGoldOre: t += 0x2400; break;
          default: NOT_REACHED(); break;
        }
      }
    } else {
      t += 0x2a80;
    }
    break;
  case Serf::TypeSmelter:
    if (t < 0x80) {
      if (serf->get_state() == Serf::StateLeavingBuilding &&
          serf->get_leaving_building_next_state() ==
          Serf::StateDropResourceOut) {
        if (serf->get_leaving_building_field_B() == 1 + Resource::TypeSteel) {
          t += 0x2900;
        } else {
          t += 0x2800;
        }
      } else {
        t += 0x1900;
      }
    } else {
      /* edi10 += 4; */
      t += 0x2980;
    }
    break;
  case Serf::TypeFisher:
    if (t < 0x80) {
      if (serf->get_state() == Serf::StateFreeWalking &&
          serf->get_free_walking_neg_dist1() == -128 &&
          serf->get_free_walking_neg_dist2() == 1) {
        t += 0x2f00;
      } else {
        t += 0x2c00;
      }
    } else {
      if (t != 0x80 && t != 0x87 && t != 0x88 && t != 0x8f) {
        play_sound(Audio::TypeSfxFishingRodReel);
      }

      /* TODO no check for state */
      if (serf->get_free_walking_neg_dist2() == 1) {
        t += 0x2d80;
      } else {
        t += 0x2c80;
      }
    }
    break;
  case Serf::TypePigFarmer:
    if (t < 0x80) {
      if (serf->get_state() == Serf::StateLeavingBuilding &&
          serf->get_leaving_building_next_state() ==
            Serf::StateDropResourceOut) {
        t += 0x3400;
      } else {
        t += 0x3200;
      }
    } else {
      t += 0x3280;
    }
    break;
  case Serf::TypeButcher:
    if (t < 0x80) {
      if (serf->get_state() == Serf::StateLeavingBuilding &&
          serf->get_leaving_building_next_state() ==
            Serf::StateDropResourceOut) {
        t += 0x3a00;
      } else {
        t += 0x3700;
      }
    } else {
      /* edi10 += 4; */
      if ((t == 0xb2 || t == 0xba || t == 0xc2 || t == 0xca) &&
          !serf->playing_sfx()) {
        serf->start_playing_sfx();
        play_sound(Audio::TypeSfxBackswordBlow);
      } else if (t != 0xb2 && t != 0xba && t != 0xc2 && t != 0xca) {
        serf->stop_playing_sfx();
      }
      t += 0x3780;
    }
    break;
  case Serf::TypeFarmer:
    if (t < 0x80) {
      if (serf->get_state() == Serf::StateFreeWalking &&
          serf->get_free_walking_neg_dist1() == -128 &&
          serf->get_free_walking_neg_dist2() == 1) {
        t += 0x4000;
      } else {
        t += 0x3d00;
      }
    } else {
      /* TODO access to state without state check */
      if (serf->get_free_walking_neg_dist1() == 0) {
        t += 0x3d80;
      } else if (t == 0x83 || (t == 0x84 && !serf->playing_sfx())) {
        serf->start_playing_sfx();
        play_sound(Audio::TypeSfxMowing);
        t += 0x3e80;
      } else if (t != 0x83 && t != 0x84) {
        serf->stop_playing_sfx();
        t += 0x3e80;
      }
    }
    break;
  case Serf::TypeMiller:
    if (t < 0x80) {
      if (serf->get_state() == Serf::StateLeavingBuilding &&
          serf->get_leaving_building_next_state() ==
            Serf::StateDropResourceOut) {
        t += 0x4500;
      } else {
        t += 0x4300;
      }
    } else {
      /* edi10 += 4; */
      t += 0x4380;
    }
    break;
  case Serf::TypeBaker:
    if (t < 0x80) {
      if (serf->get_state() == Serf::StateLeavingBuilding &&
          serf->get_leaving_building_next_state() ==
            Serf::StateDropResourceOut) {
        t += 0x4a00;
      } else {
        t += 0x4800;
      }
    } else {
      /* edi10 += 4; */
      t += 0x4880;
    }
    break;
  case Serf::TypeBoatBuilder:
    if (t < 0x80) {
      if (serf->get_state() == Serf::StateLeavingBuilding &&
          serf->get_leaving_building_next_state() ==
            Serf::StateDropResourceOut) {
        t += 0x5000;
      } else {
        t += 0x4e00;
      }
    } else if (t == 0x84 || t == 0x85) {
      if (t == 0x84 || !serf->playing_sfx()) {
        serf->start_playing_sfx();
        play_sound(Audio::TypeSfxWoodHammering);
      }
      t += 0x4e80;
    } else {
      serf->stop_playing_sfx();
      t += 0x4e80;
    }
    break;
  case Serf::TypeToolmaker:
    if (t < 0x80) {
      if (serf->get_state() == Serf::StateLeavingBuilding &&
          serf->get_leaving_building_next_state() ==
            Serf::StateDropResourceOut) {
        switch (serf->get_leaving_building_field_B() - 1) {
          case Resource::TypeShovel: t += 0x5a00; break;
          case Resource::TypeHammer: t += 0x5b00; break;
          case Resource::TypeRod: t += 0x5c00; break;
          case Resource::TypeCleaver: t += 0x5d00; break;
          case Resource::TypeScythe: t += 0x5e00; break;
          case Resource::TypeAxe: t += 0x6100; break;
          case Resource::TypeSaw: t += 0x6200; break;
          case Resource::TypePick: t += 0x6300; break;
          case Resource::TypePincer: t += 0x6400; break;
          default: NOT_REACHED(); break;
        }
      } else {
        t += 0x5800;
      }
    } else {
      /* edi10 += 4; */
      if (t == 0x83 || (t == 0xb2 && !serf->playing_sfx())) {
        serf->start_playing_sfx();
        //play_sound(Audio::TypeSfxSawing); 
        play_sound(Audio::TypeSfxSawing, DataSourceType::DOS);  // DOS sound better for hand-saw?  uses same sound ID as sawmill
      } else if (t == 0x87 || (t == 0xb6 && !serf->playing_sfx())) {
        serf->start_playing_sfx();
        play_sound(Audio::TypeSfxWoodHammering);
      } else if (t != 0xb2 && t != 0xb6) {
        serf->stop_playing_sfx();
      }
      t += 0x5880;
    }
    break;
  case Serf::TypeWeaponSmith:
    if (t < 0x80) {
      if (serf->get_state() == Serf::StateLeavingBuilding &&
          serf->get_leaving_building_next_state() ==
            Serf::StateDropResourceOut) {
        if (serf->get_leaving_building_field_B() == 1+Resource::TypeSword) {
          t += 0x5500;
        } else {
          t += 0x5400;
        }
      } else {
        t += 0x5200;
      }
    } else {
      /* edi10 += 4; */
      if (t == 0x83 || (t == 0x84 && !serf->playing_sfx())) {
        serf->start_playing_sfx();
        play_sound(Audio::TypeSfxMetalHammering);
      } else if (t != 0x84) {
        serf->stop_playing_sfx();
      }
      t += 0x5280;
    }
    break;
  case Serf::TypeGeologist:
    if (t < 0x80) {
      t += 0x3900;
    } else if (t == 0x83 || t == 0x84 || t == 0x86) {
      if (t == 0x83 || !serf->playing_sfx()) {
        serf->start_playing_sfx();
        //play_sound(Audio::TypeSfxGeologistSampling);
        play_sound(Audio::TypeSfxGeologistSampling, DataSourceType::DOS);  // I thought I prefered the Amiga sound but it is grating, annoying.  use DOS
      }
      t += 0x4c80;
    } else if (t == 0x8c || t == 0x8d) {
      if (t == 0x8c || !serf->playing_sfx()) {
        serf->start_playing_sfx();
        play_sound(Audio::TypeSfxResourceFound);
      }
      t += 0x4c80;
    } else {
      serf->stop_playing_sfx();
      t += 0x4c80;
    }
    break;
  case Serf::TypeKnight0:
  case Serf::TypeKnight1:
  case Serf::TypeKnight2:
  case Serf::TypeKnight3:
  case Serf::TypeKnight4: {
    int k = serf->get_type() - Serf::TypeKnight0;

    if (t < 0x80) {
      t += 0x7800 + 0x100*k;
    } else if (t < 0xc0) {
      if (serf->get_state() == Serf::StateKnightAttacking ||
          serf->get_state() == Serf::StateKnightAttackingFree) {
        if (serf->get_counter() >= 24 || serf->get_counter() < 8) {
          serf->stop_playing_sfx();
        } else if (!serf->playing_sfx()) {
          serf->start_playing_sfx();
          if (serf->get_attacking_field_D() == 0 ||
              serf->get_attacking_field_D() == 4) {
            play_sound(Audio::TypeSfxFight01);
          } else if (serf->get_attacking_field_D() == 2) {
            /* TODO when is TypeSfxFight02 played? */
            play_sound(Audio::TypeSfxFight03);
          } else {
            play_sound(Audio::TypeSfxFight04);
          }
        }
      }

      t += 0x7cd0 + 0x200*k;
    } else {
      t += 0x7d90 + 0x200*k;
    }
  }
    break;
  case Serf::TypeDead:
    if ((!serf->playing_sfx() &&
         (t == 2 || t == 5)) ||
        (t == 1 || t == 4)) {
      serf->start_playing_sfx();
      play_sound(Audio::TypeSfxSerfDying);
    } else {
      serf->stop_playing_sfx();
    }
    t += 0x8700;
    break;
  default:
    NOT_REACHED();
    break;
  }

  return t;
}

void
Viewport::draw_active_serf(Serf *serf, MapPos pos, int x_base, int y_base) {
  const int arr_4[] = {
     9, 5,
    10, 7,
    10, 2,
     8, 6,
    11, 8,
     9, 6,
     9, 8,
     0, 0,
     0, 0,
     0, 0,
     5, 5,
     4, 7,
     4, 2,
     7, 5,
     3, 8,
     5, 6,
     5, 8,
     0, 0,
     0, 0,
     0, 0
  };

  if ((serf->get_animation() < 0) || (serf->get_animation() > 199) ||
      (serf->get_counter() < 0)) {
    Log::Error["viewport"] << "bad animation for serf #" << serf->get_index()
                           << " (" << Serf::get_state_name(serf->get_state())
                           << "): " << serf->get_animation()
                           << "," << serf->get_counter();
    return;
  }

  Data::Animation animation = data_source->get_animation(serf->get_animation(),
                                                         serf->get_counter());

  int lx = x_base + animation.x;
  int ly = y_base + animation.y - 4 * map->get_height(pos);
  int body = serf_get_body(serf);

  if (body > -1) {
    Color color = interface->get_player_color(serf->get_owner());

    // add support for option_CanTransportSerfsInBoats
    //
    // this whole section should probably be moved outside of viewport and into serf, and instead update
    //  viewport to support drawing multiple serfs per pos
    //
    if (serf->get_type() == Serf::TypeSailor && serf->get_state() == Serf::StateTransporting &&
        (serf->get_pickup_serf_type() > Serf::TypeNone && serf->get_pickup_serf_index() > 0) ||
        (serf->get_passenger_serf_type() > Serf::TypeNone && serf->get_passenger_serf_index() > 0) ||
        (serf->get_dropped_serf_type() > Serf::TypeNone && serf->get_dropped_serf_index() > 0)){

      // if a boat passenger is being picked up, draw the passenger waiting at the flag
      //  or he will disappear once the sailor enters the tile
      int draw_boat_pickup = false;
      int pickup_body;
      int pickup_x;
      int pickup_y;
      if (serf->get_type() == Serf::TypeSailor && serf->get_state() == Serf::StateTransporting &&
          serf->get_pickup_serf_type() > Serf::TypeNone && serf->get_pickup_serf_index() > 0){
        Serf::Type pickup_type = serf->get_pickup_serf_type();
        unsigned int pickup_index = serf->get_pickup_serf_index();
        Serf *pickup_serf = interface->get_game()->get_serf(pickup_index);
        //Log::Info["viewport"] << "inside draw_active_serf: transporting sailor is about to pick up a serf of type " << pickup_type << " with serf index " << pickup_index;
        if (pickup_serf == nullptr){
          Log::Warn["viewport"] << "got nullptr for pickup boat passenger serf of type " << pickup_type << " with serf index " << pickup_index << "!";
        }else{
          // sanity check
          PGame game = interface->get_game();
          if (map->has_flag(pos) && game->get_flag_at_pos(pos)->has_serf_waiting_for_boat()){
            //Log::Info["viewport"] << "inside draw_active_serf: transporting sailor: a serf is waiting for a boat at flag pos " << pos;
            draw_boat_pickup = true;
            pickup_body = serf_get_body(pickup_serf);
            // draw the dropped passenger at the flag, which is the x/y_base and requires no offset
            pickup_x = x_base;
            pickup_y = y_base;
          }else{
            throw ExceptionFreeserf("serf->get_pickup_serf vars are set but either map has no flag at pos or game says no waiting serf!");
          }
        }
      }

      // if a sailor already has a passenger in his boat, draw the passenger in the boat
      int draw_boat_passenger = false;
      int passenger_body;
      int passenger_x;
      int passenger_y;
      if (serf->get_type() == Serf::TypeSailor && serf->get_state() == Serf::StateTransporting &&
          serf->get_passenger_serf_type() > Serf::TypeNone && serf->get_passenger_serf_index() > 0){
        Serf::Type passenger_type = serf->get_passenger_serf_type();
        unsigned int passenger_index = serf->get_passenger_serf_index();
        Serf *passenger_serf = interface->get_game()->get_serf(passenger_index);
        if (passenger_serf == nullptr){
          Log::Warn["viewport"] << "got nullptr for current boat passenger serf of type " << passenger_type << " with serf index " << passenger_index << "!";
        }
        else{
          //Log::Info["viewport"] << "inside draw_active_serf: transporting sailor: a serf has a passenger in his boat at flag pos " << pos;
          draw_boat_passenger = true;
          passenger_body = serf_get_body(passenger_serf);
          int passenger_sprite_offset[] = {
            // these seem backwards - north/south reversed
            // x, y pixel offset from sailor sprite
             5,  1,   // dir 0  "East / Right"
             5,  5,   // dir 1  "SouthEast / DownRight"
            -5,  5,   // dir 2  "SouthWest / Down"
            -5,  1,   // dir 3  "West / Left"
            -3, -3,   // dir 4  "NorthWest / UpLeft"
             3, -3,   // dir 5  "NorthEast / Up"
          };
          int passenger_dir = passenger_serf->get_walking_dir();
          // draw the passenger in the boat, a dynamic offset that follows the boat along the tile
          passenger_x = lx + passenger_sprite_offset[passenger_dir * 2];
          passenger_y = ly + passenger_sprite_offset[passenger_dir * 2 + 1];
        }
      }

      // if a sailor has just dropped off a passenger, draw the passenger standing at the flag
      //  or he will not appear until the sailor moves one tile away from the flag
      int draw_boat_dropoff = false;
      int dropoff_body;
      int dropoff_x;
      int dropoff_y;
      if (serf->get_type() == Serf::TypeSailor && serf->get_state() == Serf::StateTransporting &&
          serf->get_dropped_serf_type() > Serf::TypeNone && serf->get_dropped_serf_index() > 0){
        Serf::Type dropoff_type = serf->get_dropped_serf_type();
        unsigned int dropoff_index = serf->get_dropped_serf_index();
        Serf *dropoff_serf = interface->get_game()->get_serf(dropoff_index);
        if (dropoff_serf == nullptr){
          Log::Warn["viewport"] << "got nullptr for dropped boat passenger serf of type " << dropoff_type << " with serf index " << dropoff_index << "!";
        }
        else{
          //Log::Info["viewport"] << "inside draw_active_serf: transporting sailor: a passenger has just been dropped off at flag pos " << pos;
          draw_boat_dropoff = true;
          dropoff_body = serf_get_body(dropoff_serf);
          //  because the x/y_base is the sailor pos, it needs to be adjusted 
          //  the position is normally figured out by draw_serf_row and includes map height adjustment
          //  however, because water and coast tiles are always flag and lowest height, it should be
          //   safe to use a hardcoded table to find the relative flag pos in the next tile
          int dropoff_sprite_offset[] = {
            // these seem backwards - north/south reversed
            // x, y pixel offset from sailor sprite
            -31,  0,   // dir 0  "East / Right"
            -15,-15,   // dir 1  "SouthEast / DownRight"
             15,-17,   // dir 2  "SouthWest / Down"
             31,  0,   // dir 3  "West / Left"
             16, 20,   // dir 4  "NorthWest / UpLeft"
            -15, 15,   // dir 5  "NorthEast / Up"
          };
          int dropoff_dir = dropoff_serf->get_walking_dir();
          // draw the dropped passenger at the flag, a fixed offset relative to the sailor's base pos
          dropoff_x = x_base + dropoff_sprite_offset[dropoff_dir * 2];
          dropoff_y = y_base + dropoff_sprite_offset[dropoff_dir * 2 + 1];
        }
      }

      // draw order of the sailor/passenger spites based on the direction the flag meets the water.
      Direction flag_dir = DirectionNone;
      // if picking up, the sailor is heading to the flag so check the sailor's dir
      if (draw_boat_pickup){ flag_dir = (Direction)serf->get_walking_dir(); }
      // if dropped off, the sailor is heading away from the flag so invert the sailor's dir
      if (draw_boat_dropoff){ flag_dir = reverse_direction((Direction)serf->get_walking_dir()); }

      if (flag_dir < 4){
        // If the flag is north of the water path the sailor/boat/passenger should occlude any pickup/dropoff serf
        //  for dir left and right it looks better when the sailor/boat/passenger occludes any pickup/dropoff serf
        if (draw_boat_pickup){ draw_row_serf(pickup_x, pickup_y, true, color, pickup_body);}
        if (draw_boat_dropoff){ draw_row_serf(dropoff_x, dropoff_y, true, color, dropoff_body);}
        draw_row_serf(lx, ly, true, color, body);  // the sailor
        if (draw_boat_passenger){ draw_row_serf(passenger_x, passenger_y, false, color, passenger_body);}
      }else{
        // If the flag is south of the water path any pickup/dropoff serf should occlude the sailor/boat/passenger
        //Log::Info["viewport"] << "sailor is heading SOUTH or EAST/WEST with a passenger";
        draw_row_serf(lx, ly, true, color, body);  // the sailor
        if (draw_boat_passenger){ draw_row_serf(passenger_x, passenger_y, false, color, passenger_body);}
        if (draw_boat_pickup){ draw_row_serf(pickup_x, pickup_y, true, color, pickup_body);}
        if (draw_boat_dropoff){ draw_row_serf(dropoff_x, dropoff_y, true, color, dropoff_body);}
      }

    } else {
      //
      // **** draw any normal serf as usual ****
      //
      //
      draw_row_serf(lx, ly, true, color, body);
    }

    //  
    // 'g' Freeserf debug grid showing map lines and serf states/info
    //
    if (layers & Layer::LayerGrid) {
      frame->draw_number(lx, ly, serf->get_index(), Color(0, 0, 128));
      frame->draw_string(lx, ly + 8, serf->print_state(),  Color(0, 0, 128));
    }
    
    //
    // 'y' AI overlay used for debugging AI
    //
    if (layers & Layer::LayerAI) {
      unsigned int current_player_index = interface->get_player()->get_index();
      AI *ai = interface->get_ai_ptr(current_player_index);
      if (ai == NULL) {
        // no AI running for this player, do nothing
        //  it would be nice if there was a separate view that showed the clicked-on-pos and game speed even if AI is off
      }
      else {
        std::vector<int> ai_mark_serf = *(interface->get_ai_ptr(current_player_index)->get_ai_mark_serf());

        // automatically mark waiting serfs
        bool auto_mark_this_serf = false;

        //
        // help debug lost serf clearing issues, auto-mark Lost serfs
        //   Nov 2021
        //if (serf->get_state() == Serf::StateLost){
        //  auto_mark_this_serf = true;
        //}

        /*
        if (serf->get_state() != Serf::StateIdleInStock) {
                // direction is unknown until set by serf->is_waiting() in the ptr created here
                //  actually, for this debugging we don't even care which dir is waiting but it seems to be mandatory
                Direction dir = DirectionNone;
                Direction *dir_ptr = &dir;
                if (serf->is_waiting(dir_ptr)) {
                        Direction dir = *(dir_ptr);
                        MapPos serf_pos = bad_map_pos;
                        serf_pos = serf->get_pos();
                        //Log::Info["ai"] << "serf at pos " << serf_pos << " is waiting for direction " << dir << " / " << NameDirection[dir];
                        //Log::Info["ai"] << "serf state: " << serf->print_state();
                        //Log::Info["ai"] << "marking serf on AI overlay";
                        //ai_mark_serf->push_back(serf->get_index());
                        auto_mark_this_serf = true;
                }
        }
        */

        if (std::count(ai_mark_serf.begin(), ai_mark_serf.end(), serf->get_index()) > 0
          || auto_mark_this_serf == true) {
          //frame->draw_number(lx, ly, serf->get_index(), Color(50, 50, 200));
          std::string state_details = "";
          //frame->draw_string(lx, ly + 8, serf->print_state(), Color(125, 125, 200));
          // print state has some weird corruption at the end... try just getting the values we want
          //state_details = serf->print_state();
          state_details += serf->get_state_name(serf->get_state());
          state_details += "\n";
          //frame->draw_string(lx, ly + 16, "counter: " + std::to_string(serf->get_counter()) + "\n", Color(125, 125, 200));
          // it seems "counter" is just animation frame counter and doesn't provide any useful information in detecting stuck serfs
          //   as it keeps rolling even if serf is static
          //state_details += "counter: " + std::to_string(serf->get_counter()) + "\n";

          /* I don't think this is needed anymore, it seems that only serf->is_waiting check matters
          int wait_counter = 0;
          std::string wait_string;
          if (serf->get_state() == Serf::StateWalking) {
              wait_counter = serf->get_walking_wait_counter();
              wait_string = "walking wait: ";
          }
          if (serf->get_state() == Serf::StateTransporting || serf->get_state() == Serf::StateDelivering) {
              wait_counter = serf->get_transporting_wait_counter();
              wait_string = "transporting wait: ";
          }
          // make text increasingly red as wait increases
          int red = 150 + (25 * wait_counter);
          if (red > 255) {
              red = 255'y';
          }
          int green_blue = 150 - (25 * wait_counter);
          if (green_blue < 0) {
              green_blue = 0;
                  }

                  state_details += wait_string + std::to_string(wait_counter) + "\n";
                  */

                  // draw text box above the marked serf with its status
                  //  this red/green_blue stuff is related to darkening color effect as wait_counter increases, but is not actually needed
                  //frame->draw_string(lx, ly + 8, state_details, Color(red, green_blue, green_blue));
                  // just use a fixed color
          frame->draw_string(lx, ly + 8, state_details, colors.at("white"));
          //frame->draw_string(1, 1, status, ai->get_mark_color("white"));
        }
      }
    } // end LayerAI

  } // end if (body > -1)

  /* Draw additional serf */
  if (serf->get_state() == Serf::StateKnightEngagingBuilding ||
      serf->get_state() == Serf::StateKnightPrepareAttacking ||
      serf->get_state() == Serf::StateKnightAttacking ||
      serf->get_state() == Serf::StateKnightPrepareAttackingFree ||
      serf->get_state() == Serf::StateKnightAttackingFree ||
      serf->get_state() == Serf::StateKnightAttackingVictoryFree ||
      serf->get_state() == Serf::StateKnightAttackingDefeatFree ||
      serf->get_state() == Serf::StateKnightAttackingVictory) {
    int index = serf->get_attacking_def_index();
    if (index != 0) {
      Serf *def_serf = interface->get_game()->get_serf(index);

      Data::Animation animation =
                           data_source->get_animation(def_serf->get_animation(),
                                                      def_serf->get_counter());

      int lx = x_base + animation.x;
      int ly = y_base + animation.y - 4 * map->get_height(pos);
      int body = serf_get_body(def_serf);
     
      if (body > -1) {
        Color color = interface->get_player_color(def_serf->get_owner());
        draw_row_serf(lx, ly, true, color, body);
      }
    }
  }

  /* Draw extra objects for fight */
  if ((serf->get_state() == Serf::StateKnightAttacking ||
       serf->get_state() == Serf::StateKnightAttackingFree) &&
      animation.sprite >= 0x80 && animation.sprite < 0xc0) {
    int index = serf->get_attacking_def_index();
    if (index != 0) {
      Serf *def_serf = interface->get_game()->get_serf(index);

      if (serf->get_animation() >= 146 && serf->get_animation() < 156) {
        if ((serf->get_attacking_field_D() == 0 ||
             serf->get_attacking_field_D() == 4) &&
            serf->get_counter() < 32) {
          int anim = -1;
          if (serf->get_attacking_field_D() == 0) {
            anim = serf->get_animation() - 147;
          } else {
            anim = def_serf->get_animation() - 147;
          }

          int sprite = 198 + ((serf->get_counter() >> 3) ^ 3);
          draw_game_sprite(lx + arr_4[2*anim], ly - arr_4[2*anim+1], sprite);
        }
      }
    }
  }
}


/* Draw one row of serfs. The serfs are composed of two or three transparent
   sprites (arm, torso, possibly head). A shadow is also drawn if appropriate.
   Note that idle serfs do not have their serf_t object linked from the map
   so they are drawn seperately from active serfs. */
void
Viewport::
draw_serf_row(MapPos pos, int y_base, int cols, int x_base) {
  const int arr_1[] = {
    0x240, 0x40, 0x380, 0x140, 0x300, 0x80, 0x180, 0x200,
    0, 0x340, 0x280, 0x100, 0x1c0, 0x2c0, 0x3c0, 0xc0
  };

  const int arr_2[] = {
    0x8800, 0x8800, 0x8800, 0x8800, 0x8801, 0x8802, 0x8803, 0x8804,
    0x8804, 0x8804, 0x8804, 0x8804, 0x8803, 0x8802, 0x8801, 0x8800,
    0x8800, 0x8800, 0x8800, 0x8800, 0x8801, 0x8802, 0x8803, 0x8804,
    0x8805, 0x8806, 0x8807, 0x8808, 0x8809, 0x8808, 0x8809, 0x8808,
    0x8809, 0x8807, 0x8806, 0x8805, 0x8804, 0x8804, 0x8804, 0x8804,
    0x28, 0x29, 0x2a, 0x2b, 0x4, 0x5, 0xe, 0xf,
    0x10, 0x11, 0x1a, 0x1b, 0x23, 0x25, 0x26, 0x27,
    0x8800, 0x8800, 0x8800, 0x8800, 0x8801, 0x8802, 0x8803, 0x8804,
    0x8803, 0x8802, 0x8801, 0x8800, 0x8800, 0x8800, 0x8800, 0x8800,
    0x8801, 0x8802, 0x8803, 0x8804, 0x8804, 0x8804, 0x8804, 0x8804,
    0x8805, 0x8806, 0x8807, 0x8808, 0x8809, 0x8807, 0x8806, 0x8805,
    0x8804, 0x8803, 0x8802, 0x8802, 0x8802, 0x8802, 0x8801, 0x8800,
    0x8800, 0x8800, 0x8800, 0x8801, 0x8802, 0x8803, 0x8803, 0x8803,
    0x8803, 0x8804, 0x8804, 0x8804, 0x8805, 0x8806, 0x8807, 0x8808,
    0x8809, 0x8808, 0x8809, 0x8808, 0x8809, 0x8807, 0x8806, 0x8805,
    0x8803, 0x8803, 0x8803, 0x8802, 0x8802, 0x8801, 0x8801, 0x8801
  };

  const int arr_3[] = {
    0, 0, 0, 0, 0, 0, -2, 1, 0, 0, 2, 2, 0, 5, 0, 0,
    0, 0, 0, 3, -2, 2, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0,
    0, 0, -1, 2, -2, 1, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0,
    1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -1, 2, -2, 1, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0,
    1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };

  for (int i = 0; i < cols;
       i++, x_base += MAP_TILE_WIDTH, pos = map->move_right(pos)) {
#if 0
    /* Draw serf marker */
    if (map->has_serf(pos)) {
      serf_t *serf = game->get_serf_at_pos(pos);
      int color = game.player[map->get_owner(pos)]->color;
      frame->fill_rect(x_base - 2, y_base - 4*MAP_HEIGHT(pos) - 2, 4, 4, color);
    }
#endif

    // option_FogOfWar
    //   do not draw serfs outside of shroud/FoW
    if (option_FogOfWar && !map->is_visible(pos, interface->get_player()->get_index())){
      continue;
    }

    /* Active serf */
    if (map->has_serf(pos)) {

      Serf *serf = interface->get_game()->get_serf_at_pos(pos);

/* removing AdvancedDemolition for now, see https://github.com/forkserf/forkserf/issues/180
        // for option_AdvancedDemolition
        //  a demolishing serf must be drawn one pos down-right
        //  of his indicated pos so he can stand outside the
        //  burning building without holding & blocking the flag pos
      if (serf->get_state() == Serf::StateExitBuildingForDemolition){
        // draw demo serf down-right one pos from building (i.e. flag pos)
        Log::Info["viewport.cc"] << "inside Viewport::draw_row_serf, option_AdvancedDemolition related, serf in state StateExitBuildingForDemolition is being drawn one pos down-right";
        draw_active_serf(serf, map->move_down_right(pos), x_base + MAP_TILE_WIDTH / 2, y_base + MAP_TILE_HEIGHT);
        //draw_active_serf(serf, pos, x_base + MAP_TILE_WIDTH / 2, y_base + MAP_TILE_HEIGHT);
      }else{
        */
        // handle exceptions to normal serf drawing
        if (serf->get_state() == Serf::StateMining &&
          (serf->get_mining_substate() == 3 ||
           serf->get_mining_substate() == 4 ||
           serf->get_mining_substate() == 9 ||
           serf->get_mining_substate() == 10)) {
          //  this is "any serf that is not walking to his mine elevator"
          //   because that is drawn in draw_serf_row_behind instead of here
          //do nothing, skip this serf

        /* removing AdvancedDemolition for now, see https://github.com/forkserf/forkserf/issues/180
        } else if (serf->get_state() == Serf::StateCleaningRubble){
          // for option_AdvancedDemolition
          //  to make a "burned building rubble" sprite appear
          //  without having to define a new MapObject, simply draw the building sprite
          //  any time a serf is in StateCleaningRubble
          Log::Info["viewport.cc"] << "inside Viewport::draw_row_serf, option_AdvancedDemolition related, serf in state StateCleaningRubble, drawing rubble behind the serf";
          //draw_shadow_and_custom_building_sprite(lx, ly, 500,0,0);
          // try directly drawing it
          //  this is such a hack, instead at least try drawing it inside draw_map_objects_row and testing for demo/digger serf there
          //  I have no idea why the y_base offset needs to be 2.5x map height, I just messed with it until it fit
          //   it is likely that when building height changes it won't line up anymore, yet another reason to do this inside draw_map_object_row

          //frame->draw_sprite_special2(x_base, y_base - MAP_TILE_HEIGHT * 2.5, Data::AssetMapObject, 500, true, Color::transparent, bad_map_pos, Map::ObjectNone);
          frame->draw_sprite(x_base, y_base - MAP_TILE_HEIGHT * 2.5, Data::AssetMapObject, 500, true, Color::transparent);
          if (serf->get_animation() == 87 ) {
            // for option_AdvancedDemolition, shift the x_base to show the
            //  serf moving sideways as he digs.  In the original Digger animation
            //  the serf moves to a real MapPos and digs in the center of it, so
            //  no base shifting is required, but with this special state the 
            //  digging animation frames are re-used but the serf must stay in the
            //  former-building-site's pos
            // NOTE the if animation==87 check, it only offsets the serf while in the digging
            //  animation #87 state, not when he switches to #4 walking-right at the end
            // move four pixels left every dig

            int x_dig_offset = serf->get_digging_substate() * 4;
            draw_active_serf(serf, pos, x_base - x_dig_offset, y_base);
          }*/
        } else {
          // draw normal active serf
          draw_active_serf(serf, pos, x_base, y_base);
        }
      }
    // }  /* removing AdvancedDemolition for now, see https://github.com/forkserf/forkserf/issues/180 */

    /* Idle serf */
    if (map->get_idle_serf(pos)) {
      //Log::Debug["viewport.cc"] << "inside Viewport::draw_row_serf, and idle serf is at pos " << pos;
      int lx, ly, body;
      if (map->is_in_water(pos)) { /* Sailor */
        lx = x_base;
        ly = y_base - 4 * map->get_height(pos);
        body = 0x203;
      } else { /* Transporter */
        lx = x_base + arr_3[2* map->paths(pos)];
        ly = y_base - 4 * map->get_height(pos) +
            arr_3[2 * map->paths(pos) + 1];
        body = arr_2[((interface->get_game()->get_tick() +
                       arr_1[pos & 0xf]) >> 3) & 0x7f];
      }
      // when deleting lots of buildings, seeing exception here
      //  on get_player_color->get_color because player/owner index is -1
      //  which is "nobody".  Not sure of cause, thinking I could just have
      //  get_color return black as default??
      // actually, for now just don't draw this serf it pos has owner of -1
      if (!map->has_owner(pos)) {
        Log::Warn["viewport"] << "got owner nobody / -1 for pos " << pos << " inside draw_row_serf call, not drawing this serf to avoid crash on get_color";
      }else{
        Color color = interface->get_player_color(map->get_owner(pos));
        draw_row_serf(lx, ly, true, color, body);
      }
    }
  }
}

/* Draw serfs that should appear behind the building at their
   current position. */
// it appears that ONLY mining serfs walking to their mine elevator
//  are drawn here, nothing else
// the other serfs-working-in-buildings are drawn as building sprites
//  and not serf_torso sprites
void
Viewport::draw_serf_row_behind(MapPos pos, int y_base, int cols, int x_base) {
  for (int i = 0; i < cols;
       i++, x_base += MAP_TILE_WIDTH, pos = map->move_right(pos)) {

    // option_FogOfWar
    //   do not draw serfs outside of shroud/FoW
    if (option_FogOfWar && !map->is_visible(pos, interface->get_player()->get_index())){
      continue;
    }

    /* Active serf */
    if (map->has_serf(pos)) {
      Serf *serf = interface->get_game()->get_serf_at_pos(pos);
      // got a nullptr here once, adding check
      if (serf == nullptr){
        Log::Warn["viewport.cc"] << "inside draw_serf_row_behind, got nullptr for serf!  not drawing this one";
        // got a segfault here after the "got nullptr" message, maybe the whole draw call needs to be cancelled?  I guess try that if it happens repeatedly
        continue;
      }
      if (serf->get_state() == Serf::StateMining &&
          (serf->get_mining_substate() == 3 ||
           serf->get_mining_substate() == 4 ||
           serf->get_mining_substate() == 9 ||
           serf->get_mining_substate() == 10)) {
        draw_active_serf(serf, pos, x_base, y_base);
      }
    }
  }
}

void
Viewport::draw_game_objects(int layers_) {
  interface->trees_in_view = 0;
  interface->is_playing_birdsfx = false;
  interface->desert_in_view = 0;
  interface->is_playing_desertsfx = false;
  interface->water_in_view = 0;
  interface->is_playing_watersfx = false;

  int draw_landscape = layers_ & LayerLandscape;
  int draw_objects = layers_ & LayerObjects;
  int draw_serfs = layers_ & LayerSerfs;
  if (!draw_landscape && !draw_objects && !draw_serfs) return;

  int cols = (2*(width / MAP_TILE_WIDTH) + 1);
  int short_row_len = ((cols + 1) >> 1) + 1;
  int long_row_len = ((cols + 2) >> 1) + 1;

  int lx = -(offset_x + 16*(offset_y/20)) % 32;
  int ly = -(offset_y) % 20;

  int col_0 = (offset_x/16 + offset_y/20)/2 & map->get_col_mask();
  int row_0 = (offset_y/MAP_TILE_HEIGHT) & map->get_row_mask();
  MapPos pos = map->pos(col_0, row_0);

  /* Loop until objects drawn fall outside the frame. */
  while (1) {
    /* short row */
    if (draw_landscape) draw_water_waves_row(pos, ly, short_row_len, lx);
    if (draw_serfs) {
      draw_serf_row_behind(pos, ly, short_row_len, lx);
    }
    if (draw_objects) {
      //draw_map_objects_row(pos, ly, short_row_len, lx);
      draw_map_objects_row(pos, ly, short_row_len, lx, ly);
    }
    if (draw_serfs) {
      draw_serf_row(pos, ly, short_row_len, lx);
    } 

    ly += MAP_TILE_HEIGHT;
    if (ly >= height + 6*MAP_TILE_HEIGHT) break;

    pos = map->move_down(pos);

    /* long row */
    if (draw_landscape) {
      draw_water_waves_row(pos, ly, long_row_len, lx - 16);
    }
    if (draw_serfs) {
      draw_serf_row_behind(pos, ly, long_row_len, lx - 16);
    }
    if (draw_objects) {
      //draw_map_objects_row(pos, ly, long_row_len, lx - 16);
      draw_map_objects_row(pos, ly, long_row_len, lx - 16, ly);
    }
    if (draw_serfs) {
      draw_serf_row(pos, ly, long_row_len, lx - 16);
    }

    ly += MAP_TILE_HEIGHT;
    if (ly >= height + 6*MAP_TILE_HEIGHT) break;

    pos = map->move_down_right(pos);
  }

  //
  // ambient sounds - birds near trees, waves near water (palms)
  //  wind near deserts 
  // the timing of these isn't well planned, just fiddled around with the numbers until it
  //  gave acceptable results.  could be improved
  // ALSO... would the wind sounds be better for mountains rather than deserts??
  // NOTE ambient sounds are triggered by trees & junk objects in view, so if junk objects off
  //   and on lakes with no submerged tree (because its bugged) these sounds will not trigger
  // play bird sounds if enough trees in view
  //Log::Info["viewport.cc"] << "trees in view: " << interface->trees_in_view;
  if (interface->trees_in_view > 0){
    Random random;
    uint16_t tick_rand = (random.random() + interface->get_game()->get_const_tick()) & 0x7f;
    uint16_t limit_tick = interface->get_game()->get_const_tick() & 0x7f;
    //Log::Info["viewport.cc"] << "birdsongsfx debug: trees_in_view " << interface->trees_in_view << ", limit_tick " << limit_tick << ", const tick " << interface->get_game()->get_const_tick() << ", tick_rand " << tick_rand;
    int birdsound_chance = interface->trees_in_view / 8 + 1;
    if (birdsound_chance > 16){
      birdsound_chance = 16;
    }
    if (!interface->is_playing_birdsfx && birdsound_chance > tick_rand && limit_tick % 8 == 0){
      uint16_t foo_rand = (random.random() * tick_rand) & 0x7f;
      if (foo_rand < 20) {
        play_sound(Audio::TypeSfxBirdChirp3, DataSourceType::DOS);
        //Log::Info["viewport.cc"] << "birdsongsfx debug: CHIRP!  DOS 3";  // short chirp 
      } else if (foo_rand < 40) {
        play_sound(Audio::TypeSfxBirdChirp1, DataSourceType::DOS);
        //Log::Info["viewport.cc"] << "birdsongsfx debug: CHIRP!  DOS 1";  // short chirp
      } else if (foo_rand < 60) {
        play_sound(Audio::TypeSfxBirdChirp0, DataSourceType::DOS);
        //Log::Info["viewport.cc"] << "birdsongsfx debug: CHIRP!  DOS 0";  // short chirp
      } else if (foo_rand < 75) {
        play_sound(Audio::TypeSfxBirdChirp2, DataSourceType::DOS);
        //Log::Info["viewport.cc"] << "birdsongsfx debug: CHIRP!  DOS 2";  // medium chirp
      } else if (foo_rand < 82) {
        play_sound(Audio::TypeSfxBirdChirp3, DataSourceType::Amiga);  
        //Log::Info["viewport.cc"] << "birdsongsfx debug: CHIRP!  Amiga 3";  // long chirp
      } else if (foo_rand < 90) {
        play_sound(Audio::TypeSfxBirdChirp1, DataSourceType::Amiga);  // long chirp
        //Log::Info["viewport.cc"] << "birdsongsfx debug: CHIRP!  Amiga 1";
      } else if (foo_rand < 120) {
        play_sound(Audio::TypeSfxBirdChirp0, DataSourceType::Amiga);  // short chirp
        //Log::Info["viewport.cc"] << "birdsongsfx debug: CHIRP!  Amiga 0";
      } else {
        play_sound(Audio::TypeSfxBirdChirp2, DataSourceType::Amiga);  // long chirp
        //Log::Info["viewport.cc"] << "birdsongsfx debug: CHIRP!  Amiga 2";  
      }
      // allow up to a few birdsounds
      //if (limit_tick % 36 == 0){
        interface->is_playing_birdsfx = true;
      //}
    }else{
      // begin a period of bird sounds
      int birdsound_chance = interface->trees_in_view / 4 + 1;
      if (birdsound_chance > 22){
        birdsound_chance = 22;
      }
      /*
      if (tick_rand > 0 && tick_rand % 2 == 0 && tick_rand < birdsound_chance){
        interface->is_playing_birdsfx = false;
        Log::Info["viewport.cc"] << "resetting birdsong play";
      }
      */
    }
  }
  // play wind sounds if enough desert junk objects in view
  //Log::Info["viewport.cc"] << "desert junk objects in view: " << interface->desert_in_view;
  if (interface->desert_in_view > 0){
    Random random;
    uint16_t tick_rand = (random.random() + interface->get_game()->get_const_tick()) & 0x7f;
    uint16_t limit_tick = interface->get_game()->get_const_tick() & 0x7f;
    //Log::Info["viewport.cc"] << "windsfx debug: desert_in_view " << interface->desert_in_view << ", limit_tick " << limit_tick << ", const tick " << interface->get_game()->get_const_tick() << ", tick_rand " << tick_rand;
    int windsound_chance = interface->desert_in_view * 5;
    if (windsound_chance > 30){
      windsound_chance = 30;
    }
    if (!interface->is_playing_desertsfx && windsound_chance > tick_rand && limit_tick % 8 == 0){
      uint16_t foo_rand = (random.random() * tick_rand) & 0x7f;
      if (foo_rand < 25) {
        play_sound(Audio::TypeSfxUnknown29);
      }
      interface->is_playing_desertsfx = true;
    }else{
      //
    }
  }
  // play wave sounds if enough water junk objects in view
  //Log::Info["viewport.cc"] << "water junk objects in view: " << interface->water_in_view;
  if (interface->water_in_view > 0){
    Random random;
    uint16_t tick_rand = (random.random() + interface->get_game()->get_const_tick()) & 0x7f;
    uint16_t limit_tick = interface->get_game()->get_const_tick() & 0x7f;
    //Log::Info["viewport.cc"] << "wavesfx debug: water_in_view " << interface->water_in_view << ", limit_tick " << limit_tick << ", const tick " << interface->get_game()->get_const_tick() << ", tick_rand " << tick_rand;
    int wavesound_chance = interface->water_in_view * 100;
    if (wavesound_chance > 30){
      wavesound_chance = 30;
    }
    if (!interface->is_playing_watersfx && wavesound_chance > tick_rand && limit_tick % 8 == 0){
      uint16_t foo_rand = (random.random() * tick_rand) & 0x7f;
      if (foo_rand < 25) {
        play_sound(Audio::TypeSfxUnknown28);
        //play_sound(Audio::TypeSfxUnknown28, DataSourceType::DOS);  // DOS is a little better
      }
      interface->is_playing_watersfx = true;
    }else{
      //
    }
  }
}

void
Viewport::draw_map_cursor_sprite(MapPos pos, int sprite) {
  int sx, sy;
  screen_pix_from_map_coord(pos, &sx, &sy);

  draw_game_sprite(sx, sy, sprite);
}

void
Viewport::draw_map_cursor_possible_build() {
  int x_off = 0;
  int y_off = 0;
  MapPos base_pos = get_offset(&x_off, &y_off);

  PGame game = interface->get_game();

  for (int x_base = x_off; x_base < width + MAP_TILE_WIDTH;
       x_base += MAP_TILE_WIDTH) {
    MapPos pos = base_pos;
    int y_base = y_off;
    int row = 0;

    while (1) {
      int lx;
      if (row % 2 == 0) {
        lx = x_base;
      } else {
        lx = x_base - MAP_TILE_WIDTH/2;
      }

      int ly = y_base - 4 * map->get_height(pos);
      if (ly >= height) break;

      /* Draw possible building */
      int sprite = -1;
      if (game->can_build_castle(pos, interface->get_player())) {
        sprite = 50;
      } else if (game->can_player_build(pos, interface->get_player()) &&
                 Map::map_space_from_obj[map->get_obj(pos)] == Map::SpaceOpen &&
                 (game->can_build_flag(map->move_down_right(pos),
                                       interface->get_player()) ||
                 map->has_flag(map->move_down_right(pos)))) {
        if (game->can_build_mine(pos)) {
          sprite = 48;
        } else if (game->can_build_large(pos)) {
          sprite = 50;
        } else if (game->can_build_small(pos)) {
          sprite = 49;
        }
      }

      if (sprite >= 0) {
        draw_game_sprite(lx, ly, sprite);
      }

      if (row % 2 == 0) {
        pos = map->move_down(pos);
      } else {
        pos = map->move_down_right(pos);
      }

      y_base += MAP_TILE_HEIGHT;
      row += 1;
    }

    base_pos = map->move_right(base_pos);
  }
}

void
Viewport::draw_ai_overlay() {
  int x_off = 0;
  int y_off = 0;
  MapPos base_pos = get_offset(&x_off, &y_off);
  unsigned int current_player_index = interface->get_player()->get_index();
  AI *ai = interface->get_ai_ptr(current_player_index);
  if (ai == NULL) {
    //Log::Debug["viewport"] << "Player" << current_player_index << " is human, not drawing AI overlay";
    return;
  }
  //Log::Debug["viewport"] << "Player" << current_player_index << " is an AI, enabling AI overlay";
  // got exception here for first time jan04 2022
  ColorDotMap ai_mark_pos = *(ai->get_ai_mark_pos());
  Road *ai_mark_road = ai->get_ai_mark_road();
  //Log::Debug["viewport"] << "Player" << current_player_index << " ai_mark_road has length " << ai_mark_road->get_length();

  for (int x_base = x_off; x_base < width + MAP_TILE_WIDTH;
    x_base += MAP_TILE_WIDTH) {
    MapPos pos = base_pos;
    int y_base = y_off;
    int row = 0;

    while (1) {
      int lx;
      if (row % 2 == 0) {
        lx = x_base;
      }
      else {
        lx = x_base - MAP_TILE_WIDTH / 2;
      }

      int ly = y_base - 4 * map->get_height(pos);
      if (ly >= height) break;

      if (ai_mark_pos.count(pos) > 0) {
        if (pos != map->pos(0, 0)) {
          // first two args are x/y offset, if made bigger start more negatively
          // second two args are x/y coord of corners, increase to make bigger
          // small-medium dots
          //frame->fill_rect(lx - 2, ly + 0, 5, 5, ai->get_mark_color(ai_mark_pos.at(pos)));
          //frame->fill_rect(lx - 3, ly + 1, 7, 3, ai->get_mark_color(ai_mark_pos.at(pos)));
          // large dots
          frame->fill_rect(lx - 7, ly + 0, 16, 16, ai->get_mark_color(ai_mark_pos.at(pos)));
          frame->fill_rect(lx - 9, ly + 1, 20, 14, ai->get_mark_color(ai_mark_pos.at(pos)));
        }
      }

      if (row % 2 == 0) {
        pos = map->move_down(pos);
      }
      else {
        pos = map->move_down_right(pos);
      }


      y_base += MAP_TILE_HEIGHT;
      row += 1;
    }

    base_pos = map->move_right(base_pos);
  }

  // did I forget this somewhere when copying from Freeserf to Freeserf-with-AI-Plus?
  PGame game = interface->get_game();
  
  //
  // draw ai_mark_road to highlight any road set by AI
  //
  MapPos prevpos = ai->get_ai_mark_road()->get_source();
  for (const auto &dir : ai->get_ai_mark_road()->get_dirs()) {
          MapPos thispos = map->move(prevpos, dir);
          int prev_sx = 0;
          int prev_sy = 0;
          //Log::Info["viewport"] << "calling screen_pix_from_map_coord with FROM MapPos " << prevpos << ", empty x,y " << prev_sx << "," << prev_sy;
          screen_pix_from_map_coord(prevpos, &prev_sx, &prev_sy);
          //Log::Info["viewport"] << "called screen_pix_from_map_coord with FROM MapPos " << prevpos << ", got x,y " << prev_sx << "," << prev_sy;

          int this_sx = 0;
          int this_sy = 0;
          //Log::Info["viewport"] << "calling screen_pix_from_map_coord with TO MapPos " << thispos << ", empty x,y " << this_sx << "," << this_sy;
          screen_pix_from_map_coord(thispos, &this_sx, &this_sy);
          //Log::Info["viewport"] << "called screen_pix_from_map_coord with TO MapPos " << thispos << ", got x,y " << this_sx << "," << this_sy;

          frame->draw_line(prev_sx, prev_sy, this_sx, this_sy, ai->get_mark_color("white"));
          prevpos = thispos;
  }

  /*
  //
  // highlight arterial roads
  //
  // NOTE - if there are overlapping paths in an arterial branch, the right one "wins" and is drawn over the other
  //  it would be nice to check for this and either merge the colors or better yet alternate between them flashing
  // for now, trying random ordering so that it does flash back and forth
  //
  //Log::Info["viewport"] << "inside draw_ai_grid_overlay, debug0";
  //FlagDirToFlagDirVectorMap ai_mark_arterial_road_pairs = *(interface->get_ai_ptr(current_player_index)->get_ai_mark_arterial_road_pairs());
  FlagDirToFlagDirVectorMap ai_mark_arterial_road_pairs = *(ai->get_ai_mark_arterial_road_pairs());

  // hack - randomize the starting Dir so that overlapping paths tend to flash two different colors
  //  this seems to work well enough, leaving it
  MapPosVector inv_flag_pos_v = {};
  for (std::pair<std::pair<MapPos, Direction>, std::vector<std::pair<MapPos,Direction>>> record : ai_mark_arterial_road_pairs){
    inv_flag_pos_v.push_back(record.first.first);
  }

  // non-hack way
  //for (std::pair<std::pair<MapPos, Direction>, std::vector<std::pair<MapPos,Direction>>> record : ai_mark_arterial_road_pairs){
  //for (std::pair<std::pair<MapPos, Direction>, std::vector<std::pair<MapPos,Direction>>> record : *(ai->get_ai_mark_arterial_road_pairs())){
  
  for (MapPos inv_flag_pos : inv_flag_pos_v){
    for (Direction inv_flag_dir : cycle_directions_rand_cw()) {
      if (ai_mark_arterial_road_pairs.count(std::make_pair(inv_flag_pos, inv_flag_dir)) > 0){
        //Log::Info["viewport"] << "inside draw_ai_grid_overlay, debug1";

        // non-hack way
        //std::pair<MapPos, Direction> inv_key = record.first;
        //MapPos inv_flag_pos = inv_key.first;
        //Direction inv_flag_dir = inv_key.second;

        //Log::Info["viewport"] << "inside draw_ai_grid_overlay, debug1b inv_flag_pos " << inv_flag_pos << ", inv_flag_dir " << inv_flag_dir;
        // iterate over the provided list of flag->dir pairs and walk
        //  the tile-path along each, and highlight it
        //Color rand_color = interface->get_ai_ptr(current_player_index)->get_random_mark_color();
        //Color dir_color = interface->get_ai_ptr(current_player_index)->get_dir_color(inv_flag_dir);
        Color dir_color = ai->get_dir_color(inv_flag_dir);

        // non-hack way
        //for (std::pair<MapPos,Direction> art_key : record.second) {
        
        for (std::pair<MapPos,Direction> art_key : ai_mark_arterial_road_pairs.at(std::make_pair(inv_flag_pos, inv_flag_dir))){
          //Log::Info["viewport"] << "inside draw_ai_grid_overlay, debug2";
          // trace the tile-path to the next flag
          //  and highlight each tile-path as we go
          MapPos art_pos = art_key.first;
          Direction art_dir = art_key.second;
          //Log::Info["viewport"] << "inside draw_ai_grid_overlay, debug2b, art_pos " << art_pos << ", art_dir " << art_dir;
          MapPos pos = art_pos;
          Direction dir = art_dir;
          MapPos prev_pos = art_pos;
          while (true) {

            //Log::Info["viewport"] << "inside draw_ai_grid_overlay, debug3";
            if (!map->has_path_IMPROVED(pos, dir)){
              Log::Error["viewport"] << "inside draw_ai_grid_overlay, NO PATH IN DIR " << dir << "!, crashing";
              throw ExceptionFreeserf("inside draw_ai_grid_overlay, NO PATH IN DIR");
            }
            pos = map->move(pos, dir);
            for (Direction new_dir : cycle_directions_cw()) {
              //Log::Info["viewport"] << "inside draw_ai_grid_overlay, debug4, checking dir " << new_dir;
              if (map->has_path_IMPROVED(pos, new_dir) && new_dir != reverse_direction(dir)) {
                //Log::Info["viewport"] << "inside draw_ai_grid_overlay, debug5, found path in new_dir " << new_dir;
                int prev_sx = 0;
                int prev_sy = 0;
                screen_pix_from_map_coord(prev_pos, &prev_sx, &prev_sy);
                int this_sx = 0;
                int this_sy = 0;
                screen_pix_from_map_coord(pos, &this_sx, &this_sy);
                // don't draw the line at all if any part is off the frame
                if (prev_sx > width || prev_sy > height || this_sx > width || this_sy > height){
                  // don't draw this line
                }else{
                //frame->draw_line(prev_sx, prev_sy, this_sx, this_sy, dir_color);
                frame->draw_thick_line(prev_sx, prev_sy, this_sx, this_sy, dir_color);
                }
                prev_pos = pos;
                dir = new_dir;
                break;
              }
            }

            //Log::Info["viewport"] << "inside draw_ai_grid_overlay, debugZ";
            if (map->has_flag(pos)) {
              break;
            }

          }
        }
      } // foreach foo_dir - randomization hack
    } // foreach foo_pos - randomization hack

  }
  */ //end highlight arterial roads

/*
  //
  // draw spider-web roads
  //
  //Log::Info["viewport"] << "inside draw_ai_grid_overlay, draw spiderweb roads";
  for (Road road : *(ai->get_ai_mark_spiderweb_roads())){
    MapPos pos = road.get_source();
    //Log::Debug["viewport"] << "inside draw_ai_grid_overlay, draw spiderweb roads for road with start pos " << pos;
    //MapPos prev_pos = bad_map_pos;
    MapPos prev_pos = pos;
    for (Direction dir : road.get_dirs()){
      pos = map->move(pos, dir);
      for (Direction new_dir : cycle_directions_cw()) {
        if (map->has_path_IMPROVED(pos, new_dir) && new_dir != reverse_direction(dir)) {
          int prev_sx = 0;
          int prev_sy = 0;
          screen_pix_from_map_coord(prev_pos, &prev_sx, &prev_sy);
          int this_sx = 0;
          int this_sy = 0;
          screen_pix_from_map_coord(pos, &this_sx, &this_sy);
          // don't draw the line at all if any part is off the frame
          if (prev_sx > width || prev_sy > height || this_sx > width || this_sy > height){
            // don't draw line
          }else{
            // draw line
            frame->draw_thick_line(prev_sx, prev_sy, this_sx, this_sy, ai->get_mark_color("cyan"));
          }
          prev_pos = pos;
          dir = new_dir;
          break;
        }
      }
    }
  }
  */

/*
  //
  // draw build_better_roads
  //
  //Log::Info["viewport"] << "inside draw_ai_grid_overlay, draw build_better roads";
  for (Road road : *(ai->get_ai_mark_build_better_roads())){
    MapPos pos = road.get_source();
    //Log::Debug["viewport"] << "inside draw_ai_grid_overlay, draw build_better roads for road with start pos " << pos;
    //MapPos prev_pos = bad_map_pos;
    MapPos prev_pos = pos;
    for (Direction dir : road.get_dirs()){
      pos = map->move(pos, dir);
      for (Direction new_dir : cycle_directions_cw()) {
        if (map->has_path_IMPROVED(pos, new_dir) && new_dir != reverse_direction(dir)) {
          int prev_sx = 0;
          int prev_sy = 0;
          screen_pix_from_map_coord(prev_pos, &prev_sx, &prev_sy);
          int this_sx = 0;
          int this_sy = 0;
          screen_pix_from_map_coord(pos, &this_sx, &this_sy);
          // don't draw the line at all if any part is off the frame
          if (prev_sx > width || prev_sy > height || this_sx > width || this_sy > height){
            // don't draw line
          }else{
            // draw line
            frame->draw_thick_line(prev_sx, prev_sy, this_sx, this_sy, ai->get_mark_color("lt_purple"));
          }
          prev_pos = pos;
          dir = new_dir;
          break;
        }
      }
    }
  }
  */

  // draw AI status text box
  std::string status = ai->get_ai_status();
  //    got a segfault drawing this string once, I think the status string changed while iterating over its chars?
  //  got it again same day, trying questionable work-around
  //  still getting it, work-around not fixing, check Visual Studio on windows debugger
  frame->draw_string(65, 1, status, Color::white);

/*  this is causing exceptions frequently, not sure why, disabling for now
  // draw AI expansion goals text box
  int row = 1;   // text rows are 10 pixels apart, start at row 1 (2nd row, after ai_status row)
  frame->draw_string(1, row * 10, "expansion_goals:", Color::white);
  for (std::string goal : ai->get_ai_expansion_goals()) {
    row++;
    frame->draw_string(1, row * 10, "   " + goal, Color::white);
  }
  */

  // draw Inventory pos
  frame->draw_string(450, 1, "Inv " + std::to_string(ai->get_ai_inventory_pos()), Color::white);

  // draw current loop count
  frame->draw_string(800, 10, "AI loop: " + std::to_string(ai->get_loop_count()), Color::white);

}


void
Viewport::draw_debug_overlay() {
  int x_off = 0;
  int y_off = 0;
  MapPos base_pos = get_offset(&x_off, &y_off);
  PGame game = interface->get_game();
  unsigned int current_player_index = interface->get_player()->get_index();
  ColorDotMap debug_mark_pos = *(game->get_debug_mark_pos());
  // iterate over every MapPos in view
  for (int x_base = x_off; x_base < width + MAP_TILE_WIDTH;
    x_base += MAP_TILE_WIDTH) {
    MapPos pos = base_pos;
    int y_base = y_off;
    int row = 0;
    while (1) {
      int lx;
      if (row % 2 == 0) {
        lx = x_base;
      }
      else {
        lx = x_base - MAP_TILE_WIDTH / 2;
      }
      int ly = y_base - 4 * map->get_height(pos);
      if (ly >= height) break;
      if (debug_mark_pos.count(pos) > 0) {
        if (pos != map->pos(0, 0)) {
          // first two args are x/y offset, if made bigger start more negatively
          // second two args are x/y coord of corners, increase to make bigger
          // small-medium dots
          //frame->fill_rect(lx - 2, ly + 0, 5, 5, colors.at(debug_mark_pos.at(pos)));
          //frame->fill_rect(lx - 3, ly + 1, 7, 3, colors.at(debug_mark_pos.at(pos)));
          // large dots
          frame->fill_rect(lx - 7, ly + 0, 16, 16, colors.at(debug_mark_pos.at(pos)));
          frame->fill_rect(lx - 9, ly + 1, 20, 14, colors.at(debug_mark_pos.at(pos)));
        }
      }
      if (row % 2 == 0) {
        pos = map->move_down(pos);
      }
      else {
        pos = map->move_down_right(pos);
      }
      y_base += MAP_TILE_HEIGHT;
      row += 1;
    }
    base_pos = map->move_right(base_pos);
  }
  
  // draw player number/color text box
  frame->draw_string(1, 1, "Player" + std::to_string(current_player_index), interface->get_player_color(current_player_index));

  // draw cursor map click position
  if (debug_overlay_clicked_pos != bad_map_pos) {
    frame->draw_string(600, 1, "clicked on " + std::to_string(debug_overlay_clicked_pos), Color::white);
  }

  // draw current game speed
  frame->draw_string(800, 1, "game speed: " + std::to_string(game->get_game_speed()), Color::white);

}

void
Viewport::draw_hidden_res_overlay() {
  //Log::Info["viewport.cc"] << "inside draw_hidden_res_overlay";
  int x_off = 0;
  int y_off = 0;
  MapPos base_pos = get_offset(&x_off, &y_off);
  // iterate over each map tile, mark tiles with hidden resources
  for (int x_base = x_off; x_base < width + MAP_TILE_WIDTH; x_base += MAP_TILE_WIDTH) {
    MapPos pos = base_pos;
    int y_base = y_off;
    int row = 0;
    while (1) {
      int lx;
      if (row % 2 == 0) {
        lx = x_base;
      } else {
        lx = x_base - MAP_TILE_WIDTH / 2;
      }
      int ly = y_base - 4 * map->get_height(pos);
      if (ly >= height) break;

      if (map->get_res_amount(pos) > 0){
        // with default map generator, seems like highest amount possible is 16
        //  seeing ranges from 1 to 16.  
        //  Fish amount 1-7, mined resource amount 4+
        unsigned int amount = map->get_res_amount(pos);
        unsigned int type = map->get_res_type(pos);
        unsigned int size = 3;
        if (amount > 4){
          size = 4;
        }
        if (amount > 8){
          size = 6;
        }
        if (amount > 12){
          size = 8;
        }
        //Log::Info["viewport.cc"] << "inside draw_hidden_res_overlay, amount: " << amount << ", type " << NameMinerals[type];
        Color res_color;
        switch (type) {
          case Map::Minerals::MineralsNone:
            res_color = Color::green;
            break;
          case Map::Minerals::MineralsGold:
            res_color = Color::gold;
            break;          
          case Map::Minerals::MineralsIron:
            res_color = Color::red;
            break;          
          case Map::Minerals::MineralsCoal:
            res_color = Color::dk_gray;
            break;
          case Map::Minerals::MineralsStone:
            res_color = Color::lt_gray;
            break;
            /*
          case Map::ObjectSeeds0:
          case Map::ObjectSeeds1:
          case Map::ObjectSeeds2:
          case Map::ObjectSeeds3:
          case Map::ObjectSeeds4:
          case Map::ObjectSeeds5:
          case Map::ObjectField0:
          case Map::ObjectField1:
          case Map::ObjectField2:
          case Map::ObjectField3:
          case Map::ObjectField4:
          case Map::ObjectField5:
          case Map::ObjectFieldExpired:
            // farming debugging, highlight fields
            res_color = Color::white;
            break;
            */
        }
        //frame->fill_rect(lx - 2, ly + 0, 5, 5, res_color);
        frame->fill_rect(lx - 2, ly + 0, size, size, res_color);
        //frame->fill_rect(lx - 3, ly + 1, 7, 3, res_color);
        frame->fill_rect(lx - 3, ly + 1, size + 2, size - 2, res_color);
      }

      if (row % 2 == 0) {
        pos = map->move_down(pos);
      } else {
        pos = map->move_down_right(pos);
      }
      y_base += MAP_TILE_HEIGHT;
      row += 1;
    }
    base_pos = map->move_right(base_pos);
  }
} // end draw_hidden_res_overlay



void
Viewport::draw_map_cursor() {
  if (layers & LayerBuilds) {
    draw_map_cursor_possible_build();
  }

  MapPos pos = interface->get_map_cursor_pos();

/*
  int x = map->pos_col(pos);
  int y = map->pos_row(pos);
  std::stringstream s;
  s << x << ":" << y;
  frame->draw_string(0, 0, s.str(), 75, 0);
*/

  draw_map_cursor_sprite(pos, interface->get_map_cursor_sprite(0));

  for (Direction d : cycle_directions_cw()) {
    draw_map_cursor_sprite(map->move(pos, d),
                           interface->get_map_cursor_sprite(1+d));
  }
}

void
Viewport::draw_base_grid_overlay(const Color &color) {
  int x_base = 0;
  int y_base = 0;
  get_offset(&x_base, &y_base);

  int row = 0;
  for (int ly = y_base; ly < height; ly += MAP_TILE_HEIGHT, row++) {
    frame->draw_line(0, ly, width, ly, color);
    for (int lx = x_base + ((row % 2 == 0) ? 0 : -MAP_TILE_WIDTH/2);
         lx < width; lx += MAP_TILE_WIDTH) {
      frame->draw_line(lx, ly - 3, lx, ly + 3, color);
    }
  }
}

void
Viewport::draw_height_grid_overlay(const Color &color) {
  int x_off = 0;
  int y_off = 0;
  MapPos base_pos = get_offset(&x_off, &y_off);

  for (int x_base = x_off; x_base < width + MAP_TILE_WIDTH;
       x_base += MAP_TILE_WIDTH) {
    MapPos pos = base_pos;
    int y_base = y_off;
    int row = 0;

    while (1) {
      int lx;
      if (row % 2 == 0) {
        lx = x_base;
      } else {
        lx = x_base - MAP_TILE_WIDTH/2;
      }

      int ly = y_base - 4 * map->get_height(pos);
      if (ly >= height) break;

      /* Draw cross. */
      if (pos != map->pos(0, 0)) {
        frame->fill_rect(lx, ly - 1, 1, 3, color);
        frame->fill_rect(lx - 1, ly, 3, 1, color);
      } else {
        frame->fill_rect(lx, ly - 2, 1, 5, color);
        frame->fill_rect(lx - 2, ly, 5, 1, color);
      }

      if (row % 2 == 0) {
        pos = map->move_down(pos);
      } else {
        pos = map->move_down_right(pos);
      }

      y_base += MAP_TILE_HEIGHT;
      row += 1;
    }

    base_pos = map->move_right(base_pos);
  }
}

void
Viewport::internal_draw() {
  if (map == NULL) {
    return;
  }
  if (layers & LayerLandscape) {
    draw_landscape();
  }
  if (layers & LayerGrid) {
    draw_base_grid_overlay(Color(0xcf, 0x63, 0x63));
    draw_height_grid_overlay(Color(0xef, 0xef, 0x8f));
    // note that this also shows serf states, which is drawn elsewhere in Viewport (I think)
  }
  if (layers & LayerPaths) {
    draw_paths_and_borders();
  }
  draw_game_objects(layers);
  if (layers & LayerDebug){
    draw_debug_overlay();
  }
  if (layers & LayerAI) {
    draw_ai_overlay();
  }
  if (layers & LayerHiddenResources) {
    draw_hidden_res_overlay();
  }
  if (layers & LayerCursor) {
    draw_map_cursor();
  }
}

bool
Viewport::handle_click_left(int lx, int ly, int modifier) {
  set_redraw();
  MapPos clk_pos = map_pos_from_screen_pix(lx, ly);

  debug_overlay_clicked_pos = clk_pos;
  // CTRL-click:  KMOD_CTRL = (KMOD_LCTRL|KMOD_RCTRL) which is 64|128
  if (modifier == 64 || modifier == 128){
    Log::Debug["viewport"] << "CTRL-clicked on pos " << clk_pos << " with x,y " << map->pos_col(clk_pos) << "," << map->pos_row(clk_pos);
    // with AI overlay on, mark any serf that is clicked on (for debugging serf states)
    Serf *serf = interface->get_game()->get_serf_at_pos(clk_pos);
    if (serf == nullptr || serf->get_type() == Serf::TypeNone || serf->get_type() == Serf::TypeTransporterInventory){
      Log::Debug["viewport"] << "no active serf found at CTRL-clicked pos " << clk_pos;
    } else {
      unsigned int current_player_index = interface->get_player()->get_index();
      if (interface->get_ai_ptr(current_player_index) == nullptr) {
        Log::Debug["viewport"] << "CTRL-clicked serf at pos " << clk_pos << " is not owned by an AI player, not marking";
      }
      else {
        std::vector<int> *iface_ai_mark_serf = interface->get_ai_ptr(current_player_index)->get_ai_mark_serf();
        Log::Info["viewport"] << "adding CTRL-clicked serf of type " << NameSerf[serf->get_type()] << " at pos " << clk_pos << " to Player" << current_player_index << "'s ai_mark_serf";
        iface_ai_mark_serf->push_back(serf->get_index());
      }
    }
  } else {
    Log::Debug["viewport"] << "clicked on pos " << clk_pos << " with x,y " << map->pos_col(clk_pos) << "," << map->pos_row(clk_pos);
  }


  if (interface->is_building_road()) {
    int dx = -map->dist_x(interface->get_map_cursor_pos(), clk_pos) + 1;
    int dy = -map->dist_y(interface->get_map_cursor_pos(), clk_pos) + 1;
    Direction dir = DirectionNone;

    if (dx == 0) {
      if (dy == 1) {
        dir = DirectionLeft;
      } else if (dy == 0) {
        dir = DirectionUpLeft;
      } else {
        return false;
      }
    } else if (dx == 1) {
      if (dy == 2) {
        dir = DirectionDown;
      } else if (dy == 0) {
        dir = DirectionUp;
      } else {
        return false;
      }
    } else if (dx == 2) {
      if (dy == 1) {
        dir = DirectionRight;
      } else if (dy == 2) {
        dir = DirectionDownRight;
      } else {
        return false;
      }
    } else {
      return false;
    }

    if (interface->build_road_is_valid_dir(dir)) {
      Road road = interface->get_building_road();
      if (road.is_undo(dir)) {
        /* Delete existing path */
        int r = interface->remove_road_segment();
        if (r < 0) {
          play_sound(Audio::TypeSfxNotAccepted);
        } else {
          play_sound(Audio::TypeSfxClick);
        }
      } else {
        /* Build new road segment */
        int r = interface->build_road_segment(dir);
        if (r < 0) {
          play_sound(Audio::TypeSfxNotAccepted);
        } else if (r == 0) {
          play_sound(Audio::TypeSfxClick);
        } else {
          play_sound(Audio::TypeSfxAccepted);
        }
      }
    }
  } else {
    interface->update_map_cursor_pos(clk_pos);
    play_sound(Audio::TypeSfxClick);
  }

  return true;
}

bool
Viewport::handle_dbl_click(int lx, int ly, Event::Button button) {
  if (button != Event::ButtonLeft) return 0;

  set_redraw();

  Player *player = interface->get_player();

  MapPos clk_pos = map_pos_from_screen_pix(lx, ly);

  if (interface->is_building_road()) {
    if (clk_pos != interface->get_map_cursor_pos()) {
      MapPos pos = interface->get_building_road().get_end(map.get());
      Road road = pathfinder_map(map.get(), pos, clk_pos,
                                 &interface->get_building_road());
      if (road.get_length() != 0) {
        int r = interface->extend_road(road);
        if (r < 0) {
          play_sound(Audio::TypeSfxNotAccepted);
        } else if (r == 1) {
          play_sound(Audio::TypeSfxAccepted);
        } else {
          play_sound(Audio::TypeSfxClick);
        }
      } else {
        play_sound(Audio::TypeSfxNotAccepted);
      }
    } else {
      bool r = interface->get_game()->build_flag(
                                                interface->get_map_cursor_pos(),
                                                 player);
      if (r) {
        interface->build_road();
      } else {
        play_sound(Audio::TypeSfxNotAccepted);
      }
    }
  } else {
    if (map->get_obj(clk_pos) == Map::ObjectNone ||
        map->get_obj(clk_pos) > Map::ObjectCastle) {
      return false;
    }

    if (map->get_obj(clk_pos) == Map::ObjectFlag) {
      if (map->get_owner(clk_pos) == player->get_index()) {
        interface->open_popup(PopupBox::TypeTransportInfo);
      }

      player->temp_index = map->get_obj_index(clk_pos);
    } else { /* Building */
      Building *building = interface->get_game()->get_building_at_pos(clk_pos);
      if ((building == nullptr) || building->is_burning()) {
        return false;
      }
      if (map->get_owner(clk_pos) == player->get_index()) {
        if (!building->is_done()) {
          interface->open_popup(PopupBox::TypeOrderedBld);
        } else if (building->get_type() == Building::TypeCastle) {
          interface->open_popup(PopupBox::TypeCastleRes);
        } else if (building->get_type() == Building::TypeStock) {
          if (!building->is_active()) return 0;
          interface->open_popup(PopupBox::TypeCastleRes);
        } else if (building->get_type() == Building::TypeHut ||
                   building->get_type() == Building::TypeTower ||
                   building->get_type() == Building::TypeFortress) {
          interface->open_popup(PopupBox::TypeDefenders);
        } else if (building->get_type() == Building::TypeStoneMine ||
                   building->get_type() == Building::TypeCoalMine ||
                   building->get_type() == Building::TypeIronMine ||
                   building->get_type() == Building::TypeGoldMine) {
          interface->open_popup(PopupBox::TypeMineOutput);
        } else {
          interface->open_popup(PopupBox::TypeBldStock);
        }

        player->temp_index = map->get_obj_index(clk_pos);
      } else { /* Foreign building */
        /* TODO handle coop mode*/

        // handle option_FogOfWar
        if (option_FogOfWar && !map->is_visible(clk_pos, player->get_index())){
          Log::Debug["viewport.cc"] << "inside Viewport::handle_dbl_click(), enemy building double-clicked on is not currently visible to this player, not opening popup";
        }else{
          player->building_attacked = building->get_index();

          if (building->is_done() &&
              building->is_military()) {
            if (!building->is_active() ||
                building->get_threat_level() != 3) {
              /* It is not allowed to attack
                if currently not occupied or
                is too far from the border. */
              play_sound(Audio::TypeSfxNotAccepted);
              return false;
            }

            int found = 0;
            for (int i = 257; i >= 0; i--) {
              MapPos pos = map->pos_add_spirally(building->get_position(),
                                                        7+257-i);
              if (map->has_owner(pos)
                  && map->get_owner(pos) == player->get_index()) {
                found = 1;
                break;
              }
            }

            if (!found) {
              play_sound(Audio::TypeSfxNotAccepted);
              return false;
            }

            /* Action accepted */
            play_sound(Audio::TypeSfxClick);

            int max_knights = 0;
            switch (building->get_type()) {
              case Building::TypeHut: max_knights = 3; break;
              case Building::TypeTower: max_knights = 6; break;
              case Building::TypeFortress: max_knights = 12; break;
              case Building::TypeCastle: max_knights = 20; break;
              default: NOT_REACHED(); break;
            }

            int knights =
                  player->knights_available_for_attack(building->get_position());
            player->knights_attacking = std::min(knights, max_knights);
            interface->open_popup(PopupBox::TypeStartAttack);
          }
        } // if option_FogOfWar and not visible
      }
    }
  }

  return false;
}

bool
Viewport::handle_drag(int lx, int ly) {
  if (lx != 0 || ly != 0) {
    move_by_pixels(lx, ly);
  }

  return true;
}

Viewport::Viewport(Interface *_interface, PMap _map)
  : interface(_interface)
  , map(_map) {
  map->add_change_handler(this);
  layers = LayerAll;

  offset_x = 0;
  offset_y = 0;

  last_tick = 0;

  data_source = Data::get_instance().get_data_source();
}

Viewport::~Viewport() {
  map->del_change_handler(this);
}

void
Viewport::on_height_changed(MapPos pos) {
  redraw_map_pos(pos);
}

void
Viewport::on_object_changed(MapPos pos) {
  if (interface->get_map_cursor_pos() == pos) {
    interface->update_map_cursor_pos(pos);
  }
}

/* Space transformations. */
/* The game world space is a three dimensional space with the axes
   named "column", "row" and "height". The (column, row) coordinate
   can be encoded as a MapPos.

   The landscape is composed of a mesh of vertices in the game world
   space. There is one vertex for each integer position in the
   (column, row)-space. The height value of such a vertex is stored
   in the map data. Height values at non-integer (column, row)-points
   can be obtained by interpolation from the nearest three vertices.

   The game world can be projected onto a two-dimensional space called
   map pixel space by the following transformation:

    mx =  Tw*c  -(Tw/2)*r
    my =  Th*r  -4*h

   where (mx,my) is the coordinate in map pixel space; (c,r,h) is
   the coordinate in game world space; and (Tw,Th) is the width and
   height of a map tile in pixel units (these two values are defined
   as MAP_TILE_WIDTH and MAP_TILE_HEIGHT in the code).

   The map pixels space can be transformed into screen space by a
   simple translation.

   Note that the game world space wraps around when the edge is
   reached, i.e. decrementing the column by one when standing at
   (c,r,h) = (0,0,0) leads to the point (N-1,0,0) where N is the
   width of the map in columns. This also happens when crossing
   the row edge.

   The map pixel space also wraps around but the vertical wrap-around
   is a bit more tricky, so care must be taken when translating
   map pixel coordinates. When an edge is traversed vertically,
   the x-coordinate has to be offset by half the height of the map,
   because of the skew in the translation from game world space to
   map pixel space.
*/
void
Viewport::screen_pix_from_map_pix(int mx, int my, int *sx, int *sy) {
  int lwidth = map->get_cols()*MAP_TILE_WIDTH;
  int lheight = map->get_rows()*MAP_TILE_HEIGHT;

  *sx = mx - offset_x;
  *sy = my - offset_y;

  while (*sy < 0) {
    *sx -= (map->get_rows()*MAP_TILE_WIDTH)/2;
    *sy += lheight;
  }

  while (*sy >= lheight) {
    *sx += (map->get_rows()*MAP_TILE_WIDTH)/2;
    *sy -= lheight;
  }

  while (*sx < 0) *sx += lwidth;
  while (*sx >= lwidth) *sx -= lwidth;
}

void
Viewport::map_pix_from_map_coord(MapPos pos, int h, int *mx, int *my) {
  int lwidth = map->get_cols()*MAP_TILE_WIDTH;
  int lheight = map->get_rows()*MAP_TILE_HEIGHT;

  *mx = MAP_TILE_WIDTH * map->pos_col(pos) -
        (MAP_TILE_WIDTH/2) * map->pos_row(pos);
  *my = MAP_TILE_HEIGHT * map->pos_row(pos) - 4*h;

  if (*my < 0) {
    *mx -= (map->get_rows()*MAP_TILE_WIDTH)/2;
    *my += lheight;
  }

  if (*mx < 0) *mx += lwidth;
  else if (*mx >= lwidth) *mx -= lwidth;
}

void
Viewport::screen_pix_from_map_coord(MapPos pos, int *sx, int *sy) {
  int h = map->get_height(pos);
  int mx = 0;
  int my = 0;
  map_pix_from_map_coord(pos, h, &mx, &my);
  screen_pix_from_map_pix(mx, my, sx, sy);
}

MapPos
Viewport::map_pos_from_screen_pix(int sx, int sy) {
  int x_off = 0;
  int y_off = 0;
  int col = 0;
  int row = 0;
  get_offset(&x_off, &y_off, &col, &row);

  sx -= x_off;
  sy -= y_off;

  int y_base = -4;
  int col_offset = (sx + 24) >> 5;
  if (!BIT_TEST(sx + 24, 4)) {
    row += 1;
    y_base = 16;
  }

  col = (col + col_offset) & map->get_col_mask();
  row = row & map->get_row_mask();

  int ly;
  int last_y = -100;

  while (1) {
    ly = y_base - 4* map->get_height(map->pos(col, row));
    if (sy < ly) break;

    last_y = ly;
    col = (col + 1) & map->get_col_mask();
    row = (row + 2) & map->get_row_mask();
    y_base += 2*MAP_TILE_HEIGHT;
  }

  if (sy < (ly + last_y)/2) {
    col = (col - 1) & map->get_col_mask();
    row = (row - 2) & map->get_row_mask();
  }

  return map->pos(col, row);
}

MapPos
Viewport::get_current_map_pos() {
  return map_pos_from_screen_pix(width/2, height/2);
}

void
Viewport::move_to_map_pos(MapPos pos) {
  int mx, my;
  map_pix_from_map_coord(pos, map->get_height(pos), &mx, &my);

  int map_width = map->get_cols()*MAP_TILE_WIDTH;
  int map_height = map->get_rows()*MAP_TILE_HEIGHT;

  /* Center screen. */
  mx -= width/2;
  my -= height/2;

  if (my < 0) {
    mx -= (map->get_rows()*MAP_TILE_WIDTH)/2;
    my += map_height;
  }

  if (mx < 0) mx += map_width;
  else if (mx >= map_width) mx -= map_width;

  offset_x = mx;
  offset_y = my;

  set_redraw();
}

void
Viewport::move_by_pixels(int lx, int ly) {
  int lwidth = map->get_cols() * MAP_TILE_WIDTH;
  int lheight = map->get_rows() * MAP_TILE_HEIGHT;

  offset_x += lx;
  offset_y += ly;

  if (offset_y < 0) {
    offset_y += lheight;
    offset_x -= (map->get_rows()*MAP_TILE_WIDTH)/2;
  } else if (offset_y >= lheight) {
    offset_y -= lheight;
    offset_x += (map->get_rows()*MAP_TILE_WIDTH)/2;
  }

  if (offset_x >= lwidth) offset_x -= lwidth;
  else if (offset_x < 0) offset_x += lwidth;

  set_redraw();
}


/* Called periodically when the game progresses. */
void
Viewport::update() {
  int tick_xor = interface->get_game()->get_tick() ^ last_tick;
  last_tick = interface->get_game()->get_tick();

  /* Viewport animation does not care about low bits in anim */
  if (tick_xor >= 1 << 3) {
    set_redraw();
  }
}
