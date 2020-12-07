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
#define MAP_TILE_COLS  16
#define MAP_TILE_ROWS  16

MapPos ai_overlay_clicked_pos = bad_map_pos;

static const uint8_t tri_spr[] = {
  32, 32, 32, 32, 32, 32, 32, 32,
  32, 32, 32, 32, 32, 32, 32, 32,
  32, 32, 32, 32, 32, 32, 32, 32,
  32, 32, 32, 32, 32, 32, 32, 32,
  0, 1, 2, 3, 4, 5, 6, 7,
  0, 1, 2, 3, 4, 5, 6, 7,
  0, 1, 2, 3, 4, 5, 6, 7,
  0, 1, 2, 3, 4, 5, 6, 7,
  24, 25, 26, 27, 28, 29, 30, 31,
  24, 25, 26, 27, 28, 29, 30, 31,
  24, 25, 26, 27, 28, 29, 30, 31,
  8, 9, 10, 11, 12, 13, 14, 15,
  8, 9, 10, 11, 12, 13, 14, 15,
  8, 9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23,
  16, 17, 18, 19, 20, 21, 22, 23
};

void
Viewport::draw_triangle_up(int lx, int ly, int m, int left, int right,
                           MapPos pos, Frame *tile) {
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
  };

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
  int index = (type << 3) | tri_mask[mask];
  if (index >= 128) {
    throw ExceptionFreeserf("Failed to draw triangle up (4).");
  }

  int sprite = tri_spr[index];

  tile->draw_masked_sprite(lx, ly, Data::AssetMapMaskUp, mask,
                            Data::AssetMapGround, sprite);
}

void
Viewport::draw_triangle_down(int lx, int ly, int m, int left, int right,
                             MapPos pos, Frame *tile) {
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
  int index = (type << 3) | tri_mask[mask];
  if (index >= 128) {
    throw ExceptionFreeserf("Failed to draw triangle down (4).");
  }

  int sprite = tri_spr[index];

  tile->draw_masked_sprite(lx, ly + MAP_TILE_HEIGHT,
                            Data::AssetMapMaskDown, mask,
                            Data::AssetMapGround, sprite);
}

/* Draw a column (vertical) of tiles, starting at an up pointing tile. */
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

void
Viewport::layout() {
  landscape_tiles.clear();
}

void
Viewport::redraw_map_pos(MapPos pos) {
  int mx, my;
  map_pix_from_map_coord(pos, map->get_height(pos), &mx, &my);

  int horiz_tiles = map->get_cols()/MAP_TILE_COLS;
  int vert_tiles = map->get_rows()/MAP_TILE_ROWS;

  int tile_width = MAP_TILE_COLS*MAP_TILE_WIDTH;
  int tile_height = MAP_TILE_ROWS*MAP_TILE_HEIGHT;

  int tc = (mx / tile_width) % horiz_tiles;
  int tr = (my / tile_height) % vert_tiles;
  int tid = tc + horiz_tiles*tr;

  TilesMap::iterator it = landscape_tiles.find(tid);
  if (it != landscape_tiles.end()) {
    landscape_tiles.erase(tid);
  }
}

Frame *
Viewport::get_tile_frame(unsigned int tid, int tc, int tr) {
  TilesMap::iterator it = landscape_tiles.find(tid);
  if (it != landscape_tiles.end()) {
    return it->second.get();
  }

  int tile_width = MAP_TILE_COLS*MAP_TILE_WIDTH;
  int tile_height = MAP_TILE_ROWS*MAP_TILE_HEIGHT;

  std::unique_ptr<Frame> tile_frame(
    Graphics::get_instance().create_frame(tile_width, tile_height));
  tile_frame->fill_rect(0, 0, tile_width, tile_height, Color::black);

  int col = (tc*MAP_TILE_COLS + (tr*MAP_TILE_ROWS)/2) % map->get_cols();
  int row = tr*MAP_TILE_ROWS;
  MapPos pos = map->pos(col, row);

  int x_base = -(MAP_TILE_WIDTH/2);

  /* Draw one extra column as half a column will be outside the
   map tile on both right and left side.. */
  for (int col = 0; col < MAP_TILE_COLS+1; col++) {
    draw_up_tile_col(pos, x_base, 0, tile_height, tile_frame.get());
    draw_down_tile_col(pos, x_base + MAP_TILE_WIDTH/2, 0, tile_height,
                       tile_frame.get());

    pos = map->move_right(pos);
    x_base += MAP_TILE_WIDTH;
  }

#if 0
  /* Draw a border around the tile for debug. */
  tile_frame->draw_rect(0, 0, tile_width, tile_height, Color(0xff, 0x00, 0x00));
#endif

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

  frame->draw_sprite(lx, ly, Data::AssetMapBorder, sprite, false);
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
  frame->draw_sprite(lx, ly, Data::AssetGameObject, index-1, true);
}

void
Viewport::draw_serf(int lx, int ly, const Color &color, int head, int body) {
  frame->draw_sprite(lx, ly, Data::AssetSerfTorso, body, true, color);

  if (head >= 0) {
    frame->draw_sprite_relatively(lx, ly, Data::AssetSerfHead, head,
                                  Data::AssetSerfTorso, body);
  }
}

void
Viewport::draw_shadow_and_building_sprite(int lx, int ly, int index,
                                          const Color &color) {
  frame->draw_sprite(lx, ly, Data::AssetMapShadow, index, true);
  frame->draw_sprite(lx, ly, Data::AssetMapObject, index, true, color);
}

void
Viewport::draw_shadow_and_building_unfinished(int lx, int ly, int index,
                                              int progress) {
  float p = static_cast<float>(progress) / static_cast<float>(0xFFFF);
  frame->draw_sprite(lx, ly, Data::AssetMapShadow, index, true, p);
  frame->draw_sprite(lx, ly, Data::AssetMapObject, index, true, p);
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
  if (building->has_knight()) {
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
      draw_shadow_and_building_sprite(lx, ly, map_building_sprite[type]);
      if (building->get_res_count_in_stock(1) > 0) {
        if ((random.random() & 0x7f) <
            static_cast<int>(building->get_res_count_in_stock(1))) {
          play_sound(Audio::TypeSfxPigOink);
        }

        int pigs_count = building->get_res_count_in_stock(1);

        int pigs_layout[] = {
          0,   0,   0,  0,
          6, 140,  -2,  6,
          5, 280,   8,  8,
          3, 420, -11,  8,
          1,  40,   2, 11,
          7, 180,  -8, 13,
          8, 320,  13, 14,
          2, 460,   0, 17,
          4,  90, -11, 19,
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
          play_sound(Audio::TypeSfxGoldBoils);
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
          play_sound(Audio::TypeSfxGoldBoils);
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
      draw_water_waves(pos, x_base, y_base);
    }
  }
}

void
Viewport::draw_flag_and_res(MapPos pos, int lx, int ly) {
  Flag *flag = interface->get_game()->get_flag_at_pos(pos);

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
Viewport::draw_map_objects_row(MapPos pos, int y_base, int cols, int x_base) {
  for (int i = 0; i < cols;
       i++, x_base += MAP_TILE_WIDTH, pos = map->move_right(pos)) {
    if (map->get_obj(pos) == Map::ObjectNone) continue;

    int ly = y_base - 4 * map->get_height(pos);
    if (map->get_obj(pos) < Map::ObjectTree0) {
      if (map->get_obj(pos) == Map::ObjectFlag) {
        draw_flag_and_res(pos, x_base, ly);
      } else if (map->get_obj(pos) <= Map::ObjectCastle) {
        draw_building(pos, x_base, ly);
      }
    } else {
      int sprite = map->get_obj(pos) - Map::ObjectTree0;
      if (sprite < 24) {
        /* Trees */
        /*player->trees_in_view += 1;*/

        /* Adding sprite number to animation ensures
           that the tree animation won't be synchronized
           for all trees on the map. */
        int tree_anim = (interface->get_game()->get_tick() + sprite) >> 4;
        if (sprite < 16) {
          sprite = (sprite & ~7) + (tree_anim & 7);
        } else {
          sprite = (sprite & ~3) + (tree_anim & 3);
        }
      }
      draw_shadow_and_building_sprite(x_base, ly, sprite);
    }
  }
}

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
  };

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
  };

  /* Shadow */
  if (shadow) {
    frame->draw_sprite(lx, ly, Data::AssetSerfShadow, 0, true);
  }

  int hi = ((body >> 8) & 0xff) * 2;
  int lo = (body & 0xff) * 2;

  int base = index1[hi];
  int head = index1[hi+1];

  if (head < 0) {
    base += index2[lo];
  } else {
    base += index2[lo];
    head += index2[lo+1];
  }

  draw_serf(lx, ly, color, head, base);
}

/* Extracted from obsolete update_map_serf_rows(). */
/* Translate serf type into the corresponding sprite code. */
int
Viewport::serf_get_body(Serf *serf) {
  const int transporter_type[] = {
    0, 0x3000, 0x3500, 0x3b00, 0x4100, 0x4600, 0x4b00, 0x1400,
    0x700, 0x5100, 0x800, 0x1c00, 0x1d00, 0x1e00, 0x1a00, 0x1b00,
    0x6800, 0x6d00, 0x6500, 0x6700, 0x6b00, 0x6a00, 0x6600, 0x6900,
    0x6c00, 0x5700, 0x5600, 0, 0, 0, 0, 0
  };

  const int sailor_type[] = {
    0, 0x3100, 0x3600, 0x3c00, 0x4200, 0x4700, 0x4c00, 0x1500,
    0x900, 0x7700, 0xa00, 0x2100, 0x2200, 0x2300, 0x1f00, 0x2000,
    0x6e00, 0x6f00, 0x7000, 0x7100, 0x7200, 0x7300, 0x7400, 0x7500,
    0x7600, 0x5f00, 0x6000, 0, 0, 0, 0, 0
  };

  Data::Animation animation = data_source->get_animation(serf->get_animation(),
                                                         serf->get_counter());
  int t = animation.sprite;

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
        play_sound(Audio::TypeSfxRowing);
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
          play_sound(Audio::TypeSfxRowing);
        } else {
          serf->stop_playing_sfx();
        }
      }
      t += 0x200;
    } else if (serf->get_state() == Serf::StateTransporting) {
      t += sailor_type[serf->get_delivery()];
    } else {
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
        play_sound(Audio::TypeSfxTreeFall);
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
        play_sound(Audio::TypeSfxSawing);
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
        play_sound(Audio::TypeSfxSawing);
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
        play_sound(Audio::TypeSfxGeologistSampling);
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
    //object self fix animation
    serf->fix_bad_animation();
    return;
  }

  Data::Animation animation = data_source->get_animation(serf->get_animation(),
                                                         serf->get_counter());

  int lx = x_base + animation.x;
  int ly = y_base + animation.y - 4 * map->get_height(pos);
  int body = serf_get_body(serf);

  if (body > -1) {
    Color color = interface->get_player_color(serf->get_owner());
    draw_row_serf(lx, ly, true, color, body);
    if (layers & Layer::LayerGrid) {
      frame->draw_number(lx, ly, serf->get_index(), Color(0, 0, 128));
      frame->draw_string(lx, ly + 8, serf->print_state(),  Color(0, 0, 128));
    }
	//
// AI overlay used for debugging AI
//
	if (layers & Layer::LayerAI) {
		unsigned int current_player_index = interface->get_player()->get_index();
		AI *ai = interface->get_ai_ptr(current_player_index);
		if (ai == NULL) {
			// no AI running for this player, do nothing
		}
		else {
			std::vector<int> ai_mark_serf = *(interface->get_ai_ptr(current_player_index)->get_ai_mark_serf());

			// automatically mark waiting serfs
			bool auto_mark_this_serf = false;
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
						red = 255;
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
	}
	// end LayerAI

  }

  /* Draw additional serf */
  if (serf->get_state() == Serf::StateKnightEngagingBuilding ||
      serf->get_state() == Serf::StateKnightPrepareAttacking ||
      serf->get_state() == Serf::StateKnightAttacking ||
      serf->get_state() == Serf::StateKnightPrepareAttackingFree ||
      serf->get_state() == Serf::StateKnightAttackingFree ||
      serf->get_state() == Serf::StateKnightAttackingVictoryFree ||
      serf->get_state() == Serf::StateKnightAttackingDefeatFree) {
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
Viewport::draw_serf_row(MapPos pos, int y_base, int cols, int x_base) {
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

    /* Active serf */
    if (map->has_serf(pos)) {
      Serf *serf = interface->get_game()->get_serf_at_pos(pos);

      if (serf->get_state() != Serf::StateMining ||
          (serf->get_mining_substate() != 3 &&
           serf->get_mining_substate() != 4 &&
           serf->get_mining_substate() != 9 &&
           serf->get_mining_substate() != 10)) {
            draw_active_serf(serf, pos, x_base, y_base);
          }
    }

    /* Idle serf */
    if (map->get_idle_serf(pos)) {
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

      Color color = interface->get_player_color(map->get_owner(pos));
      draw_row_serf(lx, ly, true, color, body);
    }
  }
}

/* Draw serfs that should appear behind the building at their
   current position. */
void
Viewport::draw_serf_row_behind(MapPos pos, int y_base, int cols, int x_base) {
  for (int i = 0; i < cols;
       i++, x_base += MAP_TILE_WIDTH, pos = map->move_right(pos)) {
    /* Active serf */
    if (map->has_serf(pos)) {
      Serf *serf = interface->get_game()->get_serf_at_pos(pos);

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
  /*player->water_in_view = 0;
  player->trees_in_view = 0;*/

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
      draw_map_objects_row(pos, ly, short_row_len, lx);
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
      draw_map_objects_row(pos, ly, long_row_len, lx - 16);
    }
    if (draw_serfs) {
      draw_serf_row(pos, ly, long_row_len, lx - 16);
    }

    ly += MAP_TILE_HEIGHT;
    if (ly >= height + 6*MAP_TILE_HEIGHT) break;

    pos = map->move_down_right(pos);
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
Viewport::draw_ai_grid_overlay() {
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
	ColorDotMap ai_mark_pos = *(ai->get_ai_mark_pos());
	Road *ai_mark_road = ai->get_ai_mark_road();

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
					frame->fill_rect(lx - 2, ly + 0, 5, 5, ai->get_mark_color(ai_mark_pos.at(pos)));
					frame->fill_rect(lx - 3, ly + 1, 7, 3, ai->get_mark_color(ai_mark_pos.at(pos)));
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

	/*
	// draw road pathfinding
	//Road *p_mark_road = game->get_ai_mark_road();
	MapPos prevpos = p_mark_road->get_source();
	//for (Direction dir : p_mark_road->get_dirs()) {
	for (const auto &dir : p_mark_road->get_dirs()) {
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

					frame->draw_line(prev_sx, prev_sy, this_sx, this_sy, game->get_mark_color("white"));
					prevpos = thispos;
	}
	*/

	// draw AI status text box
	std::string status = ai->get_ai_status();
	//frame->draw_string(50, 50, "FOO\n", game->get_mark_color("white"));
	frame->draw_string(1, 1, "Player" + std::to_string(current_player_index) + " " + status, ai->get_mark_color("white"));

	// draw AI expansion goals text box
	int row = 1;   // text rows are 10 pixels apart, start at row 1 (2nd row, after ai_status row)
	frame->draw_string(1, row * 10, "expansion_goals:", ai->get_mark_color("white"));
	for (std::string goal : ai->get_ai_expansion_goals()) {
		row++;
		frame->draw_string(1, row * 10, "   " + goal, ai->get_mark_color("white"));
	}

	// draw cursor map click position
	if (ai_overlay_clicked_pos != bad_map_pos) {
		frame->draw_string(500, 1, "clicked on " + std::to_string(ai_overlay_clicked_pos), colors.at("white"));
	}

	// draw current game speed
	frame->draw_string(800, 1, "game speed: " + std::to_string(ai->get_game_speed()), colors.at("white"));
	// draw current loop count
	frame->draw_string(800, 10, "AI loop: " + std::to_string(ai->get_loop_count()), colors.at("white"));

}



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
  }
  if (layers & LayerPaths) {
    draw_paths_and_borders();
  }
  if (layers & LayerAI) {
	  draw_ai_grid_overlay();
  }
  draw_game_objects(layers);
  if (layers & LayerCursor) {
    draw_map_cursor();
  }
}

bool
Viewport::handle_click_left(int lx, int ly) {
  set_redraw();

  MapPos clk_pos = map_pos_from_screen_pix(lx, ly);
  Log::Debug["interface"] << "======= clicked on pos " << clk_pos << " with x,y " << map->pos_col(clk_pos) << "," << map->pos_row(clk_pos) << " =======";
  ai_overlay_clicked_pos = clk_pos;

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
