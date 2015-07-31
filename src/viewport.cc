/*
 * viewport.cc - Viewport GUI component
 *
 * Copyright (C) 2013-2014  Jon Lund Steffensen <jonlst@gmail.com>
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

#include <cassert>
#include <algorithm>

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
#include "src/data-source.h"

#define MAP_TILE_WIDTH   32
#define MAP_TILE_HEIGHT  20

#define MAP_TILE_TEXTURES  33
#define MAP_TILE_MASKS     81

#define VIEWPORT_COLS(viewport)  (2*((viewport)->width / MAP_TILE_WIDTH) + 1)

/* Number of cols,rows in each landscape tile */
#define MAP_TILE_COLS  16
#define MAP_TILE_ROWS  16

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
viewport_t::draw_triangle_up(int x, int y, int m, int left, int right,
                             map_pos_t pos, frame_t *frame) {
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

  assert(left - m >= -4 && left - m <= 4);
  assert(right - m >= -4 && right - m <= 4);

  int mask = 4 + m - left + 9*(4 + m - right);
  assert(tri_mask[mask] >= 0);

  int type = MAP_TYPE_UP(MAP_MOVE_UP(pos));
  int index = (type << 3) | tri_mask[mask];
  assert(index < 128);

  int sprite = tri_spr[index];

  frame->draw_masked_sprite(x, y,
                            DATA_MAP_MASK_UP_BASE + mask,
                            DATA_MAP_GROUND_BASE + sprite);
}

void
viewport_t::draw_triangle_down(int x, int y, int m, int left, int right,
                               map_pos_t pos, frame_t *frame) {
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

  assert(left - m >= -4 && left - m <= 4);
  assert(right - m >= -4 && right - m <= 4);

  int mask = 4 + left - m + 9*(4 + right - m);
  assert(tri_mask[mask] >= 0);

  int type = MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(pos));
  int index = (type << 3) | tri_mask[mask];
  assert(index < 128);

  int sprite = tri_spr[index];

  frame->draw_masked_sprite(x, y + MAP_TILE_HEIGHT,
                            DATA_MAP_MASK_DOWN_BASE + mask,
                            DATA_MAP_GROUND_BASE + sprite);
}

/* Draw a column (vertical) of tiles, starting at an up pointing tile. */
void
viewport_t::draw_up_tile_col(map_pos_t pos, int x_base, int y_base, int max_y,
                             frame_t *frame) {
  int m = MAP_HEIGHT(pos);
  int left, right;

  /* Loop until a tile is inside the frame (y >= 0). */
  while (1) {
    /* move down */
    pos = MAP_MOVE_DOWN(pos);

    left = MAP_HEIGHT(pos);
    right = MAP_HEIGHT(MAP_MOVE_RIGHT(pos));

    int t = std::min(left, right);
    /*if (left == right) t -= 1;*/ /* TODO ? */

    if (y_base + MAP_TILE_HEIGHT - 4*t >= 0) break;

    y_base += MAP_TILE_HEIGHT;

    /* move down right */
    pos = MAP_MOVE_DOWN_RIGHT(pos);

    m = MAP_HEIGHT(pos);

    if (y_base + MAP_TILE_HEIGHT - 4*m >= 0) goto down;

    y_base += MAP_TILE_HEIGHT;
  }

  /* Loop until a tile is completely outside the frame (y >= max_y). */
  while (1) {
    if (y_base - 2*MAP_TILE_HEIGHT - 4*m >= max_y) break;

    draw_triangle_up(x_base, y_base - 4*m, m, left, right, pos, frame);

    y_base += MAP_TILE_HEIGHT;

    /* move down right */
    pos = MAP_MOVE_DOWN_RIGHT(pos);
    m = MAP_HEIGHT(pos);

    if (y_base - 2*MAP_TILE_HEIGHT - 4*std::max(left, right) >= max_y) break;

  down:
    draw_triangle_down(x_base, y_base - 4*m, m, left, right, pos, frame);

    y_base += MAP_TILE_HEIGHT;

    /* move down */
    pos = MAP_MOVE_DOWN(pos);

    left = MAP_HEIGHT(pos);
    right = MAP_HEIGHT(MAP_MOVE_RIGHT(pos));
  }
}

/* Draw a column (vertical) of tiles, starting at a down pointing tile. */
void
viewport_t::draw_down_tile_col(map_pos_t pos, int x_base, int y_base,
                               int max_y, frame_t *frame) {
  int left = MAP_HEIGHT(pos);
  int right = MAP_HEIGHT(MAP_MOVE_RIGHT(pos));
  int m;

  /* Loop until a tile is inside the frame (y >= 0). */
  while (1) {
    /* move down right */
    pos = MAP_MOVE_DOWN_RIGHT(pos);

    m = MAP_HEIGHT(pos);

    if (y_base + MAP_TILE_HEIGHT - 4*m >= 0) goto down;

    y_base += MAP_TILE_HEIGHT;

    /* move down */
    pos = MAP_MOVE_DOWN(pos);

    left = MAP_HEIGHT(pos);
    right = MAP_HEIGHT(MAP_MOVE_RIGHT(pos));

    int t = std::min(left, right);
    /*if (left == right) t -= 1;*/ /* TODO ? */

    if (y_base + MAP_TILE_HEIGHT - 4*t >= 0) break;

    y_base += MAP_TILE_HEIGHT;
  }

  /* Loop until a tile is completely outside the frame (y >= max_y). */
  while (1) {
    if (y_base - 2*MAP_TILE_HEIGHT - 4*m >= max_y) break;

    draw_triangle_up(x_base, y_base - 4*m, m, left, right, pos, frame);

    y_base += MAP_TILE_HEIGHT;

    /* move down right */
    pos = MAP_MOVE_DOWN_RIGHT(pos);
    m = MAP_HEIGHT(pos);

    if (y_base - 2*MAP_TILE_HEIGHT - 4*std::max(left, right) >= max_y) break;

  down:
    draw_triangle_down(x_base, y_base - 4*m, m, left, right, pos, frame);

    y_base += MAP_TILE_HEIGHT;

    /* move down */
    pos = MAP_MOVE_DOWN(pos);

    left = MAP_HEIGHT(pos);
    right = MAP_HEIGHT(MAP_MOVE_RIGHT(pos));
  }
}

/* Deallocate global allocations for landscape tiles */
void
viewport_t::map_deinit() {
  while (landscape_tiles.size()) {
    tiles_map_t::iterator it = landscape_tiles.begin();
    delete it->second;
    landscape_tiles.erase(it);
  }
}

/* Reinitialize landscape tiles */
void
viewport_t::map_reinit() {
  map_deinit();
}

void
viewport_t::layout() {
  map_deinit();
}

void
viewport_t::redraw_map_pos(map_pos_t pos) {
  int mx, my;
  map_pix_from_map_coord(pos, MAP_HEIGHT(pos), &mx, &my);

  int horiz_tiles = game.map.cols/MAP_TILE_COLS;
  int vert_tiles = game.map.rows/MAP_TILE_ROWS;

  int tile_width = MAP_TILE_COLS*MAP_TILE_WIDTH;
  int tile_height = MAP_TILE_ROWS*MAP_TILE_HEIGHT;

  int tc = (mx / tile_width) % horiz_tiles;
  int tr = (my / tile_height) % vert_tiles;
  int tid = tc + horiz_tiles*tr;

  tiles_map_t::iterator it = landscape_tiles.find(tid);
  if (it != landscape_tiles.end()) {
    frame_t *frame = it->second;
    landscape_tiles.erase(tid);
    delete frame;
  }
}

frame_t *
viewport_t::get_tile_frame(unsigned int tid, int tc, int tr) {
  tiles_map_t::iterator it = landscape_tiles.find(tid);
  if (it != landscape_tiles.end()) {
    return it->second;
  }

  int tile_width = MAP_TILE_COLS*MAP_TILE_WIDTH;
  int tile_height = MAP_TILE_ROWS*MAP_TILE_HEIGHT;

  frame_t *tile_frame = gfx_t::get_instance()->create_frame(tile_width,
                                                            tile_height);
  tile_frame->fill_rect(0, 0, tile_width, tile_height, 0);

  int col = (tc*MAP_TILE_COLS + (tr*MAP_TILE_ROWS)/2) % game.map.cols;
  int row = tr*MAP_TILE_ROWS;
  map_pos_t pos = MAP_POS(col, row);

  int x_base = -(MAP_TILE_WIDTH/2);

  /* Draw one extra column as half a column will be outside the
   map tile on both right and left side.. */
  for (int col = 0; col < MAP_TILE_COLS+1; col++) {
    draw_up_tile_col(pos, x_base, 0, tile_height, tile_frame);
    draw_down_tile_col(pos, x_base + MAP_TILE_WIDTH/2, 0, tile_height,
                       tile_frame);

    pos = MAP_MOVE_RIGHT(pos);
    x_base += MAP_TILE_WIDTH;
  }

#if 0
  /* Draw a border around the tile for debug. */
  tile_frame->draw_rect(0, 0, tile_width, tile_height, 76);
#endif

  LOGV("viewport", "map: %i,%i, cols,rows: %i,%i, tc,tr: %i,%i, tw,th: %i,%i",
       game.map.cols*MAP_TILE_WIDTH, game.map.rows*MAP_TILE_HEIGHT,
       game.map.cols, game.map.rows,
       tc, tr,
       tile_width, tile_height);

  landscape_tiles[tid] = tile_frame;

  return tile_frame;
}

void
viewport_t::draw_landscape() {
  int horiz_tiles = game.map.cols/MAP_TILE_COLS;
  int vert_tiles = game.map.rows/MAP_TILE_ROWS;

  int tile_width = MAP_TILE_COLS*MAP_TILE_WIDTH;
  int tile_height = MAP_TILE_ROWS*MAP_TILE_HEIGHT;

  int map_width = game.map.cols*MAP_TILE_WIDTH;
  int map_height = game.map.rows*MAP_TILE_HEIGHT;

  int my = offset_y;
  int y = 0;
  int x_base = 0;
  while (y < height) {
    while (my >= map_height) {
      my -= map_height;
      x_base += (game.map.rows*MAP_TILE_WIDTH)/2;
    }

    int ty = my % tile_height;

    int x = 0;
    int mx = (offset_x + x_base) % map_width;
    while (x < width) {
      int tx = mx % tile_width;

      int tc = (mx / tile_width) % horiz_tiles;
      int tr = (my / tile_height) % vert_tiles;
      int tid = tc + horiz_tiles*tr;

      frame_t *tile_frame = get_tile_frame(tid, tc, tr);

      int w = tile_width - tx;
      if (x+w > width) {
        w = width - x;
      }
      int h = tile_height - ty;
      if (y+h > height) {
        h = height - y;
      }

      frame->draw_frame(x, y, tx, ty, tile_frame, w, h);
      x += tile_width - tx;
      mx += tile_width - tx;
    }

    y += tile_height - ty;
    my += tile_height - ty;
  }
}


void
viewport_t::draw_path_segment(int x, int y, map_pos_t pos, dir_t dir) {
  int h1 = MAP_HEIGHT(pos);
  int h2 = MAP_HEIGHT(MAP_MOVE(pos, dir));
  int h_diff = h1 - h2;

  int t1, t2, h3, h4, h_diff_2;

  switch (dir) {
  case DIR_RIGHT:
    y -= 4*std::max(h1, h2) + 2;
    t1 = MAP_TYPE_DOWN(pos);
    t2 = MAP_TYPE_UP(MAP_MOVE_UP(pos));
    h3 = MAP_HEIGHT(MAP_MOVE_UP(pos));
    h4 = MAP_HEIGHT(MAP_MOVE_DOWN_RIGHT(pos));
    h_diff_2 = (h3 - h4) - 4*h_diff;
    break;
  case DIR_DOWN_RIGHT:
    y -= 4*h1 + 2;
    t1 = MAP_TYPE_UP(pos);
    t2 = MAP_TYPE_DOWN(pos);
    h3 = MAP_HEIGHT(MAP_MOVE_RIGHT(pos));
    h4 = MAP_HEIGHT(MAP_MOVE_DOWN(pos));
    h_diff_2 = 2*(h3 - h4);
    break;
  case DIR_DOWN:
    x -= MAP_TILE_WIDTH/2;
    y -= 4*h1 + 2;
    t1 = MAP_TYPE_UP(pos);
    t2 = MAP_TYPE_DOWN(MAP_MOVE_LEFT(pos));
    h3 = MAP_HEIGHT(MAP_MOVE_LEFT(pos));
    h4 = MAP_HEIGHT(MAP_MOVE_DOWN(pos));
    h_diff_2 = 4*h_diff - h3 + h4;
    break;
  default:
    NOT_REACHED();
    break;
  }

  int mask = h_diff + 4 + dir*9;
  int sprite = 0;
  int type = std::max(t1, t2);

  if (h_diff_2 > 4) {
    sprite = 0;
  } else if (h_diff_2 > -6) {
    sprite = 1;
  } else {
    sprite = 2;
  }

  if (type < 4) {
    sprite = 9;
  } else if (type > 13) {
    sprite += 6;
  } else if (type > 7) {
    sprite += 3;
  }

  frame->draw_masked_sprite(x, y,
                            DATA_PATH_MASK_BASE + mask,
                            DATA_PATH_GROUND_BASE + sprite);
}

void
viewport_t::draw_border_segment(int x, int y, map_pos_t pos, dir_t dir) {
  int h1 = MAP_HEIGHT(pos);
  int h2 = MAP_HEIGHT(MAP_MOVE(pos, dir));
  int h_diff = h2 - h1;

  int t1, t2, h3, h4, h_diff_2;

  switch (dir) {
  case DIR_RIGHT:
    x += MAP_TILE_WIDTH/2;
    y -= 2*(h1 + h2) + 4;
    t1 = MAP_TYPE_DOWN(pos);
    t2 = MAP_TYPE_UP(MAP_MOVE_UP(pos));
    h3 = MAP_HEIGHT(MAP_MOVE_UP(pos));
    h4 = MAP_HEIGHT(MAP_MOVE_DOWN_RIGHT(pos));
    h_diff_2 = h3 - h4 + 4*h_diff;
    break;
  case DIR_DOWN_RIGHT:
    x += MAP_TILE_WIDTH/4;
    y -= 2*(h1 + h2) - 6;
    t1 = MAP_TYPE_UP(pos);
    t2 = MAP_TYPE_DOWN(pos);
    h3 = MAP_HEIGHT(MAP_MOVE_RIGHT(pos));
    h4 = MAP_HEIGHT(MAP_MOVE_DOWN(pos));
    h_diff_2 = 2*(h3 - h4);
    break;
  case DIR_DOWN:
    x -= MAP_TILE_WIDTH/4;
    y -= 2*(h1 + h2) - 6;
    t1 = MAP_TYPE_UP(pos);
    t2 = MAP_TYPE_DOWN(MAP_MOVE_LEFT(pos));
    h3 = MAP_HEIGHT(MAP_MOVE_LEFT(pos));
    h4 = MAP_HEIGHT(MAP_MOVE_DOWN(pos));
    h_diff_2 = 4*h_diff - h3 + h4;
    break;
  default:
    NOT_REACHED();
    break;
  }

  int sprite = 0;
  int type = std::max(t1, t2);

  if (h_diff_2 > 1) {
    sprite = 0;
  } else if (h_diff_2 > -9) {
    sprite = 1;
  } else {
    sprite = 2;
  }

  if (type < 4) {
    sprite = 9; /* Bouy */
  } else if (type > 13) {
    sprite += 6;
  } else if (type > 7) {
    sprite += 3;
  }

  frame->draw_transp_sprite(x, y, DATA_MAP_BORDER_BASE + sprite, false);
}

void
viewport_t::draw_paths_and_borders() {
  int x_off = -(offset_x + 16*(offset_y/20)) % 32;
  int y_off = -offset_y % 20;

  int col_0 = (offset_x/16 + offset_y/20)/2 & game.map.col_mask;
  int row_0 = (offset_y/MAP_TILE_HEIGHT) & game.map.row_mask;
  map_pos_t base_pos = MAP_POS(col_0, row_0);

  for (int x_base = x_off; x_base < width + MAP_TILE_WIDTH;
       x_base += MAP_TILE_WIDTH) {
    map_pos_t pos = base_pos;
    int y_base = y_off;
    int row = 0;

    while (1) {
      int x;
      if (row % 2 == 0) {
        x = x_base;
      } else {
        x = x_base - MAP_TILE_WIDTH/2;
      }

      int y = y_base - 4*MAP_HEIGHT(pos);
      if (y >= height) break;

      /* For each direction right, down right and down,
         draw the corresponding paths and borders. */
      for (int d = DIR_RIGHT; d <= DIR_DOWN; d++) {
        map_tile_t *tiles = game.map.tiles;
        map_pos_t other_pos = MAP_MOVE(pos, d);

        if (BIT_TEST(tiles[pos].paths, d)) {
          draw_path_segment(x, y_base, pos, (dir_t)d);
        } else if (MAP_HAS_OWNER(pos) != MAP_HAS_OWNER(other_pos) ||
             MAP_OWNER(pos) != MAP_OWNER(other_pos)) {
          draw_border_segment(x, y_base, pos, (dir_t)d);
        }
      }

      if (row % 2 == 0) {
        pos = MAP_MOVE_DOWN(pos);
      } else {
        pos = MAP_MOVE_DOWN_RIGHT(pos);
      }

      y_base += MAP_TILE_HEIGHT;
      row += 1;
    }

    base_pos = MAP_MOVE_RIGHT(base_pos);
  }

  /* If we're in road construction mode, also draw
     the temporarily placed roads. */
  if (interface->is_building_road()) {
    map_pos_t pos = interface->get_building_road_source();
    for (int i = 0; i < interface->get_building_road_length(); i++) {
      dir_t dir = interface->get_building_road_dir(i);

      map_pos_t draw_pos = pos;
      dir_t draw_dir = dir;
      if (draw_dir > DIR_DOWN) {
        draw_pos = MAP_MOVE(pos, dir);
        draw_dir = DIR_REVERSE(dir);
      }

      int mx, my;
      map_pix_from_map_coord(draw_pos, MAP_HEIGHT(draw_pos), &mx, &my);

      int sx, sy;
      screen_pix_from_map_pix(mx, my, &sx, &sy);

      draw_path_segment(sx, sy + 4*MAP_HEIGHT(draw_pos), draw_pos, draw_dir);

      pos = MAP_MOVE(pos, dir);
    }
  }
}

void
viewport_t::draw_game_sprite(int x, int y, int index) {
  frame->draw_transp_sprite(x, y, DATA_GAME_OBJECT_BASE + index, true);
}

void
viewport_t::draw_serf(int x, int y, unsigned char color, int head, int body) {
  frame->draw_transp_sprite(x, y, DATA_SERF_ARMS_BASE + body, true);
  frame->draw_transp_sprite(x, y, DATA_SERF_TORSO_BASE + body, true, color);

  if (head >= 0) {
    frame->draw_transp_sprite_relatively(x, y, DATA_SERF_HEAD_BASE + head,
                                         DATA_SERF_ARMS_BASE + body);
  }
}

void
viewport_t::draw_shadow_and_building_sprite(int x, int y, int index) {
  frame->draw_overlay_sprite(x, y, DATA_MAP_SHADOW_BASE + index);
  frame->draw_transp_sprite(x, y, DATA_MAP_OBJECT_BASE + index, true);
}

void
viewport_t::draw_shadow_and_building_unfinished(int x, int y, int index,
                                                int progress) {
  float p = static_cast<float>(progress) / static_cast<float>(0xFFFF);
  frame->draw_overlay_sprite(x, y, DATA_MAP_SHADOW_BASE + index, p);
  frame->draw_transp_sprite(x, y, DATA_MAP_OBJECT_BASE + index, true, p);
}

static const int map_building_frame_sprite[] = {
  0, 0xba, 0xba, 0xba, 0xba,
  0xb9, 0xb9, 0xb9, 0xb9,
  0xba, 0xc1, 0xba, 0xb1, 0xb8, 0xb1, 0xbb,
  0xb7, 0xb5, 0xb6, 0xb0, 0xb8, 0xb3, 0xaf, 0xb4
};

void
viewport_t::draw_building_unfinished(building_t *building,
                                     building_type_t bld_type,
                                     int x, int y) {
  if (building->get_progress() == 0) { /* Draw cross */
    draw_shadow_and_building_sprite(x, y, 0x90);
  } else {
    /* Stone waiting */
    int stone = building->waiting_stone();
    for (int i = 0; i < stone; i++) {
      draw_game_sprite(x+10 - i*3, y-8 + i, 1 + RESOURCE_STONE);
    }

    /* Planks waiting */
    int planks = building->waiting_planks();
    for (int i = 0; i < planks; i++) {
      draw_game_sprite(x+12 - i*3, y-6 + i, 1 + RESOURCE_PLANK);
    }

    if (BIT_TEST(building->get_progress(), 15)) { /* Frame finished */
      draw_shadow_and_building_sprite(x, y,
                                      map_building_frame_sprite[bld_type]);
      draw_shadow_and_building_unfinished(x, y, map_building_sprite[bld_type],
                                         2*(building->get_progress() & 0x7fff));
    } else {
      draw_shadow_and_building_sprite(x, y, 0x91); /* corner stone */
      if (building->get_progress() > 1) {
        draw_shadow_and_building_unfinished(x, y,
                                            map_building_frame_sprite[bld_type],
                                            2*building->get_progress());
      }
    }
  }
}

void
viewport_t::draw_unharmed_building(building_t *building, int x, int y) {
  random_state_t *random = interface->get_random();

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
    building_type_t type = building->get_type();
    switch (type) {
    case BUILDING_FISHER:
    case BUILDING_LUMBERJACK:
    case BUILDING_STONECUTTER:
    case BUILDING_FORESTER:
    case BUILDING_STOCK:
    case BUILDING_FARM:
    case BUILDING_BUTCHER:
    case BUILDING_SAWMILL:
    case BUILDING_TOOLMAKER:
    case BUILDING_CASTLE:
      draw_shadow_and_building_sprite(x, y, map_building_sprite[type]);
      break;
    case BUILDING_BOATBUILDER:
      draw_shadow_and_building_sprite(x, y, map_building_sprite[type]);
      if (building->get_res_count_in_stock(1) > 0) {
        /* TODO x might not be correct */
        draw_game_sprite(x+3, y + 13,
                         174 + building->get_res_count_in_stock(1));
      }
      break;
    case BUILDING_STONEMINE:
    case BUILDING_COALMINE:
    case BUILDING_IRONMINE:
    case BUILDING_GOLDMINE:
      if (building->is_active()) { /* Draw elevator up */
        draw_game_sprite(x-6, y-39, 152);
      }
      if (building->playing_sfx()) { /* Draw elevator down */
        draw_game_sprite(x-6, y-39, 153);
        map_pos_t pos = building->get_position();
        if ((((game.tick +
               reinterpret_cast<uint8_t*>(&pos)[1]) >> 3) & 7) == 0
            && random_int(random) < 40000) {
          play_sound(SFX_ELEVATOR);
        }
      }
      draw_shadow_and_building_sprite(x, y, map_building_sprite[type]);
      break;
    case BUILDING_HUT:
      draw_shadow_and_building_sprite(x, y, map_building_sprite[type]);
      if (building->has_main_serf()) {
        draw_game_sprite(x-14, y+2 - 2*building->get_knight_count(),
             182 + ((game.tick >> 3) & 3) + 4*building->get_state());
      }
      break;
    case BUILDING_PIGFARM:
      draw_shadow_and_building_sprite(x, y, map_building_sprite[type]);
      if (building->get_res_count_in_stock(1) > 0) {
        if ((random_int(random) & 0x7f) <
            static_cast<int>(building->get_res_count_in_stock(1))) {
          play_sound(SFX_PIG_OINK);
        }

        int pigs_count = building->get_res_count_in_stock(1);

        if (pigs_count >= 6) {
          int i = (140 + (game.tick >> 3)) & 0xfe;
          draw_game_sprite(x + pigfarm_anim[i+1] - 2, y+6, pigfarm_anim[i]);
        }

        if (pigs_count >= 5) {
          int i = (280 + (game.tick >> 3)) & 0xfe;
          draw_game_sprite(x + pigfarm_anim[i+1] + 8, y+8, pigfarm_anim[i]);
        }

        if (pigs_count >= 3) {
          int i = (420 + (game.tick >> 3)) & 0xfe;
          draw_game_sprite(x + pigfarm_anim[i+1] - 11, y+8, pigfarm_anim[i]);
        }

        int i = (40 + (game.tick >> 3)) & 0xfe;
        draw_game_sprite(x + pigfarm_anim[i+1] + 2, y+11, pigfarm_anim[i]);

        if (pigs_count >= 7) {
          int i = (180 + (game.tick >> 3)) & 0xfe;
          draw_game_sprite(x + pigfarm_anim[i+1] - 8, y+13, pigfarm_anim[i]);
        }

        if (pigs_count >= 8) {
          int i = (320 + (game.tick >> 3)) & 0xfe;
          draw_game_sprite(x + pigfarm_anim[i+1] + 13, y+14, pigfarm_anim[i]);
        }

        if (pigs_count >= 2) {
          int i = (460 + (game.tick >> 3)) & 0xfe;
          draw_game_sprite(x + pigfarm_anim[i+1], y+17, pigfarm_anim[i]);
        }

        if (pigs_count >= 4) {
          int i = (90 + (game.tick >> 3)) & 0xfe;
          draw_game_sprite(x + pigfarm_anim[i+1] - 11, y+19, pigfarm_anim[i]);
        }
      }
      break;
    case BUILDING_MILL:
      if (building->is_active()) {
        if ((game.tick >> 4) & 3) {
          building->stop_playing_sfx();
        } else if (!building->playing_sfx()) {
          building->start_playing_sfx();
          play_sound(SFX_MILL_GRINDING);
        }
        draw_shadow_and_building_sprite(x, y, map_building_sprite[type] +
                                        ((game.tick >> 4) & 3));
      } else {
        draw_shadow_and_building_sprite(x, y, map_building_sprite[type]);
      }
      break;
    case BUILDING_BAKER:
      draw_shadow_and_building_sprite(x, y, map_building_sprite[type]);
      if (building->is_active()) {
        draw_game_sprite(x + 5, y-21, 154 + ((game.tick >> 3) & 7));
      }
      break;
    case BUILDING_STEELSMELTER:
      draw_shadow_and_building_sprite(x, y, map_building_sprite[type]);
      if (building->is_active()) {
        int i = (game.tick >> 3) & 7;
        if (i == 0 || (i == 7 && !building->playing_sfx())) {
          building->start_playing_sfx();
          play_sound(SFX_GOLD_BOILS);
        } else if (i != 7) {
          building->stop_playing_sfx();
        }

        draw_game_sprite(x+6, y-32, 128+i);
      }
      break;
    case BUILDING_WEAPONSMITH:
      draw_shadow_and_building_sprite(x, y, map_building_sprite[type]);
      if (building->is_active()) {
        draw_game_sprite(x-16, y-21, 128 + ((game.tick >> 3) & 7));
      }
      break;
    case BUILDING_TOWER:
      draw_shadow_and_building_sprite(x, y, map_building_sprite[type]);
      if (building->has_main_serf()) {
        draw_game_sprite(x+13, y - 18 - building->get_knight_count(),
                     182 + ((game.tick >> 3) & 3) + 4*building->get_state());
      }
      break;
    case BUILDING_FORTRESS:
      draw_shadow_and_building_sprite(x, y, map_building_sprite[type]);
      if (building->has_main_serf()) {
        draw_game_sprite(x-12, y - 21 - building->get_knight_count()/2,
             182 + ((game.tick >> 3) & 3) + 4*building->get_state());
        draw_game_sprite(x+22, y - 34 - (building->get_knight_count()+1)/2,
             182 + (((game.tick >> 3) + 2) & 3) + 4*building->get_state());
      }
      break;
    case BUILDING_GOLDSMELTER:
      draw_shadow_and_building_sprite(x, y, map_building_sprite[type]);
      if (building->is_active()) {
        int i = (game.tick >> 3) & 7;
        if (i == 0 || (i == 7 && !building->playing_sfx())) {
          building->start_playing_sfx();
          play_sound(SFX_GOLD_BOILS);
        } else if (i != 7) {
          building->stop_playing_sfx();
        }

        draw_game_sprite(x-7, y-33, 128+i);
      }
      break;
    default:
      NOT_REACHED();
      break;
    }
  } else { /* unfinished building */
    if (building->get_type() != BUILDING_CASTLE) {
      draw_building_unfinished(building, building->get_type(), x, y);
    } else {
      draw_shadow_and_building_unfinished(x, y, 0xb2, building->get_progress());
    }
  }
}

void
viewport_t::draw_burning_building(building_t *building, int x, int y) {
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
      !building->playing_sfx()) {
    building->start_playing_sfx();
    play_sound(SFX_BURNING);
  } else {
    building->stop_playing_sfx();
  }

  uint16_t delta = game.tick - building->get_tick();
  building->set_tick(game.tick);

  if (building->get_burning_counter() >= delta) {
    building->decrease_burning_counter(delta);  // TODO(jonls): this is also
                                                // done in update_buildings().
    draw_unharmed_building(building, x, y);

    int type = 0;
    if (building->is_done() ||
        building->get_progress() >= 16000) {
      type = building->get_type();
    }

    int offset = ((building->get_burning_counter() >> 3) & 7) ^ 7;
    const int *anim = building_burn_animation +
                      building_anim_offset_from_type[type];
    while (anim[0] >= 0) {
      draw_game_sprite(x+anim[1], y+anim[2], 136 + anim[0] + offset);
      offset = (offset + 3) & 7;
      anim += 3;
    }
  } else {
    building->set_burning_counter(0);
  }
}

void
viewport_t::draw_building(map_pos_t pos, int x, int y) {
  building_t *building = game.buildings[MAP_OBJ_INDEX(pos)];

  if (building->is_burning()) {
    draw_burning_building(building, x, y);
  } else {
    draw_unharmed_building(building, x, y);
  }
}

void
viewport_t::draw_water_waves(map_pos_t pos, int x, int y) {
  int sprite = DATA_MAP_WAVES_BASE + (((pos ^ 5) + (game.tick >> 3)) & 0xf);

  if (MAP_TYPE_DOWN(pos) < 4 && MAP_TYPE_UP(pos) < 4) {
    frame->draw_waves_sprite(x - 16, y, 0, sprite);
  } else if (MAP_TYPE_DOWN(pos) < 4) {
    int mask = DATA_MAP_MASK_DOWN_BASE + 40;
    frame->draw_waves_sprite(x, y + 16, mask, sprite);
  } else {
    int mask = DATA_MAP_MASK_UP_BASE + 40;
    frame->draw_waves_sprite(x - 16, y, mask, sprite);
  }
}

void
viewport_t::draw_water_waves_row(map_pos_t pos, int y_base, int cols,
                                 int x_base) {
  for (int i = 0; i < cols; i++, x_base += MAP_TILE_WIDTH,
       pos = MAP_MOVE_RIGHT(pos)) {
    if (MAP_TYPE_UP(pos) < 4 || MAP_TYPE_DOWN(pos) < 4) {
      /*player->water_in_view += 1;*/
      draw_water_waves(pos, x_base, y_base);
    }
  }
}

void
viewport_t::draw_flag_and_res(map_pos_t pos, int x, int y) {
  flag_t *flag = game.flags[MAP_OBJ_INDEX(pos)];

  if (flag->get_resource_at_slot(0) != RESOURCE_NONE) {
    draw_game_sprite(x+6 , y-4, flag->get_resource_at_slot(0) + 1);
  }
  if (flag->get_resource_at_slot(1) != RESOURCE_NONE) {
    draw_game_sprite(x+10, y-2, flag->get_resource_at_slot(1) + 1);
  }
  if (flag->get_resource_at_slot(2) != RESOURCE_NONE) {
    draw_game_sprite(x-4 , y-4, flag->get_resource_at_slot(2) + 1);
  }

  int pl_num = flag->get_player();
  int spr = 0x80 + (pl_num << 2) + ((game.tick >> 3) & 3);

  draw_shadow_and_building_sprite(x, y, spr);

  if (flag->get_resource_at_slot(3) != RESOURCE_NONE) {
    draw_game_sprite(x+10, y+2, flag->get_resource_at_slot(3) + 1);
  }
  if (flag->get_resource_at_slot(4) != RESOURCE_NONE) {
    draw_game_sprite(x-8 , y-2, flag->get_resource_at_slot(4) + 1);
  }
  if (flag->get_resource_at_slot(5) != RESOURCE_NONE) {
    draw_game_sprite(x+6 , y+4, flag->get_resource_at_slot(5) + 1);
  }
  if (flag->get_resource_at_slot(6) != RESOURCE_NONE) {
    draw_game_sprite(x-8 , y+2, flag->get_resource_at_slot(6) + 1);
  }
  if (flag->get_resource_at_slot(7) != RESOURCE_NONE) {
    draw_game_sprite(x-4 , y+4, flag->get_resource_at_slot(7) + 1);
  }
}

void
viewport_t::draw_map_objects_row(map_pos_t pos, int y_base, int cols,
                                 int x_base) {
  for (int i = 0; i < cols; i++, x_base += MAP_TILE_WIDTH,
       pos = MAP_MOVE_RIGHT(pos)) {
    if (MAP_OBJ(pos) == MAP_OBJ_NONE) continue;

    int y = y_base - 4*MAP_HEIGHT(pos);
    if (MAP_OBJ(pos) < MAP_OBJ_TREE_0) {
      if (MAP_OBJ(pos) == MAP_OBJ_FLAG) {
        draw_flag_and_res(pos, x_base, y);
      } else if (MAP_OBJ(pos) <= MAP_OBJ_CASTLE) {
        draw_building(pos, x_base, y);
      }
    } else {
      int sprite = MAP_OBJ(pos) - MAP_OBJ_TREE_0;
      if (sprite < 24) {
        /* Trees */
        /*player->trees_in_view += 1;*/

        /* Adding sprite number to animation ensures
           that the tree animation won't be synchronized
           for all trees on the map. */
        int tree_anim = (game.tick + sprite) >> 4;
        if (sprite < 16) {
          sprite = (sprite & ~7) + (tree_anim & 7);
        } else {
          sprite = (sprite & ~3) + (tree_anim & 3);
        }
      }
      draw_shadow_and_building_sprite(x_base, y, sprite);
    }
  }
}

/* Draw one individual serf in the row. */
void
viewport_t::draw_row_serf(int x, int y, int shadow, int color, int body) {
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
    frame->draw_overlay_sprite(x, y, DATA_SERF_SHADOW);
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

  draw_serf(x, y, color, head, base);
}

/* Extracted from obsolete update_map_serf_rows(). */
/* Translate serf type into the corresponding sprite code. */
int
viewport_t::serf_get_body(serf_t *serf) {
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

  animation_t *animation = data_source->get_animation(serf->get_animation(),
                                                      serf->get_counter());
  int t = animation->time;

  switch (serf->get_type()) {
  case SERF_TRANSPORTER:
  case SERF_GENERIC:
    if (serf->get_state() == SERF_STATE_IDLE_ON_PATH) {
      return -1;
    } else if ((serf->get_state() == SERF_STATE_TRANSPORTING ||
                serf->get_state() == SERF_STATE_DELIVERING) &&
               serf->get_delivery() != 0) {
      t += transporter_type[serf->get_delivery()];
    }
    break;
  case SERF_SAILOR:
    if (serf->get_state() == SERF_STATE_TRANSPORTING && t < 0x80) {
      if (((t & 7) == 4 && !serf->playing_sfx()) ||
          (t & 7) == 3) {
        serf->start_playing_sfx();
        play_sound(SFX_ROWING);
      } else {
        serf->stop_playing_sfx();
      }
    }

    if ((serf->get_state() == SERF_STATE_TRANSPORTING &&
         serf->get_delivery() == 0) ||
        serf->get_state() == SERF_STATE_LOST_SAILOR ||
        serf->get_state() == SERF_STATE_FREE_SAILING) {
      if (t < 0x80) {
        if (((t & 7) == 4 && !serf->playing_sfx()) ||
            (t & 7) == 3) {
          serf->start_playing_sfx();
          play_sound(SFX_ROWING);
        } else {
          serf->stop_playing_sfx();
        }
      }
      t += 0x200;
    } else if (serf->get_state() == SERF_STATE_TRANSPORTING) {
      t += sailor_type[serf->get_delivery()];
    } else {
      t += 0x100;
    }
    break;
  case SERF_DIGGER:
    if (t < 0x80) {
      t += 0x300;
    } else if (t == 0x83 || t == 0x84) {
      if (t == 0x83 || !serf->playing_sfx()) {
        serf->start_playing_sfx();
        play_sound(SFX_DIGGING);
      }
      t += 0x380;
    } else {
      serf->stop_playing_sfx();
      t += 0x380;
    }
    break;
  case SERF_BUILDER:
    if (t < 0x80) {
      t += 0x500;
    } else if ((t & 7) == 4 || (t & 7) == 5) {
      if ((t & 7) == 4 || !serf->playing_sfx()) {
        serf->start_playing_sfx();
        play_sound(SFX_HAMMER_BLOW);
      }
      t += 0x580;
    } else {
      serf->stop_playing_sfx();
      t += 0x580;
    }
    break;
  case SERF_TRANSPORTER_INVENTORY:
    if (serf->get_state() == SERF_STATE_BUILDING_CASTLE) {
      return -1;
    } else {
      int res = serf->get_delivery();
      t += transporter_type[res];
    }
    break;
  case SERF_LUMBERJACK:
    if (t < 0x80) {
      if (serf->get_state() == SERF_STATE_FREE_WALKING &&
          serf->get_free_walking_neg_dist1() == -128 &&
          serf->get_free_walking_neg_dist2() == 1) {
        t += 0x1000;
      } else {
        t += 0xb00;
      }
    } else if ((t == 0x86 && !serf->playing_sfx()) ||
         t == 0x85) {
      serf->start_playing_sfx();
      play_sound(SFX_AX_BLOW);
      /* TODO Dangerous reference to unknown state vars.
         It is probably free walking. */
      if (serf->get_free_walking_neg_dist2() == 0 &&
          serf->get_counter() < 64) {
        play_sound(SFX_TREE_FALL);
      }
      t += 0xe80;
    } else if (t != 0x86) {
      serf->stop_playing_sfx();
      t += 0xe80;
    }
    break;
  case SERF_SAWMILLER:
    if (t < 0x80) {
      if (serf->get_state() == SERF_STATE_LEAVING_BUILDING &&
          serf->get_leaving_building_next_state() ==
            SERF_STATE_DROP_RESOURCE_OUT) {
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
        play_sound(SFX_SAWING);
      } else if (t != 0xb7 && t != 0xbf && t != 0xc7 && t != 0xcf) {
        serf->stop_playing_sfx();
      }
      t += 0x1580;
    }
    break;
  case SERF_STONECUTTER:
    if (t < 0x80) {
      if ((serf->get_state() == SERF_STATE_FREE_WALKING &&
           serf->get_free_walking_neg_dist1() == -128 &&
           serf->get_free_walking_neg_dist2() == 1) ||
          (serf->get_state() == SERF_STATE_STONECUTTING &&
           serf->get_free_walking_neg_dist1() == 2)) {
        t += 0x1200;
      } else {
        t += 0xd00;
      }
    } else if (t == 0x85 || (t == 0x86 && !serf->playing_sfx())) {
      serf->start_playing_sfx();
      play_sound(SFX_PICK_BLOW);
      t += 0x1280;
    } else if (t != 0x86) {
      serf->stop_playing_sfx();
      t += 0x1280;
    }
    break;
  case SERF_FORESTER:
    if (t < 0x80) {
      t += 0xe00;
    } else if (t == 0x86 || (t == 0x87 && !serf->playing_sfx())) {
      serf->start_playing_sfx();
      play_sound(SFX_PLANTING);
      t += 0x1080;
    } else if (t != 0x87) {
      serf->stop_playing_sfx();
      t += 0x1080;
    }
    break;
  case SERF_MINER:
    if (t < 0x80) {
      if ((serf->get_state() != SERF_STATE_MINING ||
           serf->get_mining_res() == 0) &&
          (serf->get_state() != SERF_STATE_LEAVING_BUILDING ||
           serf->get_leaving_building_next_state() !=
           SERF_STATE_DROP_RESOURCE_OUT)) {
        t += 0x1800;
      } else {
        resource_type_t res = RESOURCE_NONE;

        switch (serf->get_state()) {
        case SERF_STATE_MINING:
          res = (resource_type_t)(serf->get_mining_res() - 1);
          break;
        case SERF_STATE_LEAVING_BUILDING:
          res = (resource_type_t)(serf->get_leaving_building_field_B() - 1);
          break;
        default:
          NOT_REACHED();
          break;
        }

        switch (res) {
        case RESOURCE_STONE: t += 0x2700; break;
        case RESOURCE_IRONORE: t += 0x2500; break;
        case RESOURCE_COAL: t += 0x2600; break;
        case RESOURCE_GOLDORE: t += 0x2400; break;
        default: NOT_REACHED(); break;
        }
      }
    } else {
      t += 0x2a80;
    }
    break;
  case SERF_SMELTER:
    if (t < 0x80) {
      if (serf->get_state() == SERF_STATE_LEAVING_BUILDING &&
          serf->get_leaving_building_next_state() ==
          SERF_STATE_DROP_RESOURCE_OUT) {
        if (serf->get_leaving_building_field_B() == 1+RESOURCE_STEEL) {
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
  case SERF_FISHER:
    if (t < 0x80) {
      if (serf->get_state() == SERF_STATE_FREE_WALKING &&
          serf->get_free_walking_neg_dist1() == -128 &&
          serf->get_free_walking_neg_dist2() == 1) {
        t += 0x2f00;
      } else {
        t += 0x2c00;
      }
    } else {
      if (t != 0x80 && t != 0x87 && t != 0x88 && t != 0x8f) {
        play_sound(SFX_FISHING_ROD_REEL);
      }

      /* TODO no check for state */
      if (serf->get_free_walking_neg_dist2() == 1) {
        t += 0x2d80;
      } else {
        t += 0x2c80;
      }
    }
    break;
  case SERF_PIGFARMER:
    if (t < 0x80) {
      if (serf->get_state() == SERF_STATE_LEAVING_BUILDING &&
          serf->get_leaving_building_next_state() ==
            SERF_STATE_DROP_RESOURCE_OUT) {
        t += 0x3400;
      } else {
        t += 0x3200;
      }
    } else {
      t += 0x3280;
    }
    break;
  case SERF_BUTCHER:
    if (t < 0x80) {
      if (serf->get_state() == SERF_STATE_LEAVING_BUILDING &&
          serf->get_leaving_building_next_state() ==
            SERF_STATE_DROP_RESOURCE_OUT) {
        t += 0x3a00;
      } else {
        t += 0x3700;
      }
    } else {
      /* edi10 += 4; */
      if ((t == 0xb2 || t == 0xba || t == 0xc2 || t == 0xca) &&
          !serf->playing_sfx()) {
        serf->start_playing_sfx();
        play_sound(SFX_BACKSWORD_BLOW);
      } else if (t != 0xb2 && t != 0xba && t != 0xc2 && t != 0xca) {
        serf->stop_playing_sfx();
      }
      t += 0x3780;
    }
    break;
  case SERF_FARMER:
    if (t < 0x80) {
      if (serf->get_state() == SERF_STATE_FREE_WALKING &&
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
        play_sound(SFX_MOWING);
        t += 0x3e80;
      } else if (t != 0x83 && t != 0x84) {
        serf->stop_playing_sfx();
        t += 0x3e80;
      }
    }
    break;
  case SERF_MILLER:
    if (t < 0x80) {
      if (serf->get_state() == SERF_STATE_LEAVING_BUILDING &&
          serf->get_leaving_building_next_state() ==
            SERF_STATE_DROP_RESOURCE_OUT) {
        t += 0x4500;
      } else {
        t += 0x4300;
      }
    } else {
      /* edi10 += 4; */
      t += 0x4380;
    }
    break;
  case SERF_BAKER:
    if (t < 0x80) {
      if (serf->get_state() == SERF_STATE_LEAVING_BUILDING &&
          serf->get_leaving_building_next_state() ==
            SERF_STATE_DROP_RESOURCE_OUT) {
        t += 0x4a00;
      } else {
        t += 0x4800;
      }
    } else {
      /* edi10 += 4; */
      t += 0x4880;
    }
    break;
  case SERF_BOATBUILDER:
    if (t < 0x80) {
      if (serf->get_state() == SERF_STATE_LEAVING_BUILDING &&
          serf->get_leaving_building_next_state() ==
            SERF_STATE_DROP_RESOURCE_OUT) {
        t += 0x5000;
      } else {
        t += 0x4e00;
      }
    } else if (t == 0x84 || t == 0x85) {
      if (t == 0x84 || !serf->playing_sfx()) {
        serf->start_playing_sfx();
        play_sound(SFX_WOOD_HAMMERING);
      }
      t += 0x4e80;
    } else {
      serf->stop_playing_sfx();
      t += 0x4e80;
    }
    break;
  case SERF_TOOLMAKER:
    if (t < 0x80) {
      if (serf->get_state() == SERF_STATE_LEAVING_BUILDING &&
          serf->get_leaving_building_next_state() ==
            SERF_STATE_DROP_RESOURCE_OUT) {
        switch (serf->get_leaving_building_field_B() - 1) {
        case RESOURCE_SHOVEL: t += 0x5a00; break;
        case RESOURCE_HAMMER: t += 0x5b00; break;
        case RESOURCE_ROD: t += 0x5c00; break;
        case RESOURCE_CLEAVER: t += 0x5d00; break;
        case RESOURCE_SCYTHE: t += 0x5e00; break;
        case RESOURCE_AXE: t += 0x6100; break;
        case RESOURCE_SAW: t += 0x6200; break;
        case RESOURCE_PICK: t += 0x6300; break;
        case RESOURCE_PINCER: t += 0x6400; break;
        default: NOT_REACHED(); break;
        }
      } else {
        t += 0x5800;
      }
    } else {
      /* edi10 += 4; */
      if (t == 0x83 || (t == 0xb2 && !serf->playing_sfx())) {
        serf->start_playing_sfx();
        play_sound(SFX_SAWING);
      } else if (t == 0x87 || (t == 0xb6 && !serf->playing_sfx())) {
        serf->start_playing_sfx();
        play_sound(SFX_WOOD_HAMMERING);
      } else if (t != 0xb2 && t != 0xb6) {
        serf->stop_playing_sfx();
      }
      t += 0x5880;
    }
    break;
  case SERF_WEAPONSMITH:
    if (t < 0x80) {
      if (serf->get_state() == SERF_STATE_LEAVING_BUILDING &&
          serf->get_leaving_building_next_state() ==
            SERF_STATE_DROP_RESOURCE_OUT) {
        if (serf->get_leaving_building_field_B() == 1+RESOURCE_SWORD) {
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
        play_sound(SFX_METAL_HAMMERING);
      } else if (t != 0x84) {
        serf->stop_playing_sfx();
      }
      t += 0x5280;
    }
    break;
  case SERF_GEOLOGIST:
    if (t < 0x80) {
      t += 0x3900;
    } else if (t == 0x83 || t == 0x84 || t == 0x86) {
      if (t == 0x83 || !serf->playing_sfx()) {
        serf->start_playing_sfx();
        play_sound(SFX_GEOLOGIST_SAMPLING);
      }
      t += 0x4c80;
    } else if (t == 0x8c || t == 0x8d) {
      if (t == 0x8c || !serf->playing_sfx()) {
        serf->start_playing_sfx();
        play_sound(SFX_RESOURCE_FOUND);
      }
      t += 0x4c80;
    } else {
      serf->stop_playing_sfx();
      t += 0x4c80;
    }
    break;
  case SERF_KNIGHT_0:
  case SERF_KNIGHT_1:
  case SERF_KNIGHT_2:
  case SERF_KNIGHT_3:
  case SERF_KNIGHT_4:
  {
    int k = serf->get_type() - SERF_KNIGHT_0;

    if (t < 0x80) {
      t += 0x7800 + 0x100*k;
    } else if (t < 0xc0) {
      if (serf->get_state() == SERF_STATE_KNIGHT_ATTACKING ||
          serf->get_state() == SERF_STATE_KNIGHT_ATTACKING_FREE) {
        if (serf->get_counter() >= 24 || serf->get_counter() < 8) {
          serf->stop_playing_sfx();
        } else if (!serf->playing_sfx()) {
          serf->start_playing_sfx();
          if (serf->get_attacking_field_D() == 0 ||
              serf->get_attacking_field_D() == 4) {
            play_sound(SFX_FIGHT_01);
          } else if (serf->get_attacking_field_D() == 2) {
            /* TODO when is SFX_FIGHT_02 played? */
            play_sound(SFX_FIGHT_03);
          } else {
            play_sound(SFX_FIGHT_04);
          }
        }
      }

      t += 0x7cd0 + 0x200*k;
    } else {
      t += 0x7d90 + 0x200*k;
    }
  }
    break;
  case SERF_DEAD:
    if ((!serf->playing_sfx() &&
         (t == 2 || t == 5)) ||
        (t == 1 || t == 4)) {
      serf->start_playing_sfx();
      play_sound(SFX_SERF_DYING);
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
viewport_t::draw_active_serf(serf_t *serf, map_pos_t pos,
                             int x_base, int y_base) {
  const int arr_4[] = {
    9, 5, 10, 7, 10, 2, 8, 6,
    11, 8, 9, 6, 9, 8, 0, 0,
    0, 0, 0, 0, 5, 5, 4, 7,
    4, 2, 7, 5, 3, 8, 5, 6,
    5, 8, 0, 0, 0, 0, 0, 0
  };

  if ((serf->get_animation() < 0) || (serf->get_animation() > 199) ||
      (serf->get_counter() < 0)) {
    LOGE("viewport", "bad animation for serf #%i (%s): %d,%d",
         serf->get_index(), serf_t::get_state_name(serf->get_state()),
         serf->get_animation(), serf->get_counter());
    return;
  }

  animation_t *animation = data_source->get_animation(serf->get_animation(),
                                                      serf->get_counter());

  int x = x_base + animation->x;
  int y = y_base + animation->y - 4*MAP_HEIGHT(pos);
  int body = serf_get_body(serf);

  if (body > -1) {
    int color = game.players[serf->get_player()]->get_color();
    draw_row_serf(x, y, 1, color, body);
  }

  /* Draw additional serf */
  if (serf->get_state() == SERF_STATE_KNIGHT_ENGAGING_BUILDING ||
      serf->get_state() == SERF_STATE_KNIGHT_PREPARE_ATTACKING ||
      serf->get_state() == SERF_STATE_KNIGHT_ATTACKING ||
      serf->get_state() == SERF_STATE_KNIGHT_PREPARE_ATTACKING_FREE ||
      serf->get_state() == SERF_STATE_KNIGHT_ATTACKING_FREE ||
      serf->get_state() == SERF_STATE_KNIGHT_ATTACKING_VICTORY_FREE ||
      serf->get_state() == SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE) {
    int index = serf->get_attacking_def_index();
    if (index != 0) {
      serf_t *def_serf = game.serfs[index];

      animation_t *animation =
                           data_source->get_animation(def_serf->get_animation(),
                                                      def_serf->get_counter());

      int x = x_base + animation->x;
      int y = y_base + animation->y - 4*MAP_HEIGHT(pos);
      int body = serf_get_body(def_serf);

      if (body > -1) {
        int color = game.players[def_serf->get_player()]->get_color();
        draw_row_serf(x, y, 1, color, body);
      }
    }
  }

  /* Draw extra objects for fight */
  if ((serf->get_state() == SERF_STATE_KNIGHT_ATTACKING ||
       serf->get_state() == SERF_STATE_KNIGHT_ATTACKING_FREE) &&
      animation->time >= 0x80 && animation->time < 0xc0) {
    int index = serf->get_attacking_def_index();
    if (index != 0) {
      serf_t *def_serf = game.serfs[index];

      if (serf->get_animation() >= 146 &&
          serf->get_animation() < 156) {
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
          draw_game_sprite(x + arr_4[2*anim], y - arr_4[2*anim+1], sprite);
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
viewport_t::draw_serf_row(map_pos_t pos, int y_base, int cols, int x_base) {
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
       i++, x_base += MAP_TILE_WIDTH, pos = MAP_MOVE_RIGHT(pos)) {
#if 0
    /* Draw serf marker */
    if (MAP_SERF_INDEX(pos) != 0) {
      serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));
      int color = game.player[SERF_PLAYER(serf)]->color;
      frame->fill_rect(x_base - 2, y_base - 4*MAP_HEIGHT(pos) - 2, 4, 4, color);
    }
#endif

    /* Active serf */
    if (MAP_SERF_INDEX(pos) != 0) {
      serf_t *serf = game.serfs[MAP_SERF_INDEX(pos)];

      if (serf->get_state() != SERF_STATE_MINING ||
          (serf->get_mining_substate() != 3 &&
           serf->get_mining_substate() != 4 &&
           serf->get_mining_substate() != 9 &&
           serf->get_mining_substate() != 10)) {
            draw_active_serf(serf, pos, x_base, y_base);
          }
    }

    /* Idle serf */
    if (MAP_IDLE_SERF(pos)) {
      int x, y, body;
      if (MAP_IN_WATER(pos)) { /* Sailor */
        x = x_base;
        y = y_base - 4*MAP_HEIGHT(pos);
        body = 0x203;
      } else { /* Transporter */
        x = x_base + arr_3[2*MAP_PATHS(pos)];
        y = y_base - 4*MAP_HEIGHT(pos) + arr_3[2*MAP_PATHS(pos)+1];
        body = arr_2[((game.tick + arr_1[pos & 0xf]) >> 3) & 0x7f];
      }

      int color = game.players[MAP_OWNER(pos)]->get_color();
      draw_row_serf(x, y, 1, color, body);
    }
  }
}

/* Draw serfs that should appear behind the building at their
   current position. */
void
viewport_t::draw_serf_row_behind(map_pos_t pos, int y_base, int cols,
                                 int x_base) {
  for (int i = 0; i < cols;
       i++, x_base += MAP_TILE_WIDTH, pos = MAP_MOVE_RIGHT(pos)) {
    /* Active serf */
    if (MAP_SERF_INDEX(pos) != 0) {
      serf_t *serf = game.serfs[MAP_SERF_INDEX(pos)];

      if (serf->get_state() == SERF_STATE_MINING &&
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
viewport_t::draw_game_objects(int layers) {
  /*player->water_in_view = 0;
  player->trees_in_view = 0;*/

  int draw_landscape = layers & VIEWPORT_LAYER_LANDSCAPE;
  int draw_objects = layers & VIEWPORT_LAYER_OBJECTS;
  int draw_serfs = layers & VIEWPORT_LAYER_SERFS;
  if (!draw_landscape && !draw_objects && !draw_serfs) return;

  int cols = VIEWPORT_COLS(this);
  int short_row_len = ((cols + 1) >> 1) + 1;
  int long_row_len = ((cols + 2) >> 1) + 1;

  int x = -(offset_x + 16*(offset_y/20)) % 32;
  int y = -(offset_y) % 20;

  int col_0 = (offset_x/16 + offset_y/20)/2 & game.map.col_mask;
  int row_0 = (offset_y/MAP_TILE_HEIGHT) & game.map.row_mask;
  map_pos_t pos = MAP_POS(col_0, row_0);

  /* Loop until objects drawn fall outside the frame. */
  while (1) {
    /* short row */
    if (draw_landscape) draw_water_waves_row(pos, y, short_row_len, x);
    if (draw_serfs) {
      draw_serf_row_behind(pos, y, short_row_len, x);
    }
    if (draw_objects) {
      draw_map_objects_row(pos, y, short_row_len, x);
    }
    if (draw_serfs) {
      draw_serf_row(pos, y, short_row_len, x);
    }

    y += MAP_TILE_HEIGHT;
    if (y >= height + 6*MAP_TILE_HEIGHT) break;

    pos = MAP_MOVE_DOWN(pos);

    /* long row */
    if (draw_landscape) {
      draw_water_waves_row(pos, y, long_row_len, x - 16);
    }
    if (draw_serfs) {
      draw_serf_row_behind(pos, y, long_row_len, x - 16);
    }
    if (draw_objects) {
      draw_map_objects_row(pos, y, long_row_len, x - 16);
    }
    if (draw_serfs) {
      draw_serf_row(pos, y, long_row_len, x - 16);
    }

    y += MAP_TILE_HEIGHT;
    if (y >= height + 6*MAP_TILE_HEIGHT) break;

    pos = MAP_MOVE_DOWN_RIGHT(pos);
  }
}

void
viewport_t::draw_map_cursor_sprite(map_pos_t pos, int sprite) {
  int mx, my;
  map_pix_from_map_coord(pos, MAP_HEIGHT(pos), &mx, &my);

  int sx, sy;
  screen_pix_from_map_pix(mx, my, &sx, &sy);

  draw_game_sprite(sx, sy, sprite);
}

void
viewport_t::draw_map_cursor_possible_build() {
  int x_off = -(offset_x + 16*(offset_y/20)) % 32;
  int y_off = -offset_y % 20;

  int col_0 = (offset_x/16 + offset_y/20)/2 & game.map.col_mask;
  int row_0 = (offset_y/MAP_TILE_HEIGHT) & game.map.row_mask;
  map_pos_t base_pos = MAP_POS(col_0, row_0);

  for (int x_base = x_off; x_base < width + MAP_TILE_WIDTH;
       x_base += MAP_TILE_WIDTH) {
    map_pos_t pos = base_pos;
    int y_base = y_off;
    int row = 0;

    while (1) {
      int x;
      if (row % 2 == 0) {
        x = x_base;
      } else {
        x = x_base - MAP_TILE_WIDTH/2;
      }

      int y = y_base - 4*MAP_HEIGHT(pos);
      if (y >= height) break;

      /* Draw possible building */
      int sprite = -1;
      if (game_can_build_castle(pos, interface->get_player())) {
        sprite = 50;
      } else if (game_can_player_build(pos, interface->get_player()) &&
           map_space_from_obj[MAP_OBJ(pos)] == MAP_SPACE_OPEN &&
           (game_can_build_flag(MAP_MOVE_DOWN_RIGHT(pos),
                                interface->get_player()) ||
            MAP_HAS_FLAG(MAP_MOVE_DOWN_RIGHT(pos)))) {
        if (game_can_build_mine(pos)) {
          sprite = 48;
        } else if (game_can_build_large(pos)) {
          sprite = 50;
        } else if (game_can_build_small(pos)) {
          sprite = 49;
        }
      }

      if (sprite >= 0) {
        draw_game_sprite(x, y, sprite);
      }

      if (row % 2 == 0) {
        pos = MAP_MOVE_DOWN(pos);
      } else {
        pos = MAP_MOVE_DOWN_RIGHT(pos);
      }

      y_base += MAP_TILE_HEIGHT;
      row += 1;
    }

    base_pos = MAP_MOVE_RIGHT(base_pos);
  }
}

void
viewport_t::draw_map_cursor() {
  if (layers & VIEWPORT_LAYER_BUILDS) {
    draw_map_cursor_possible_build();
  }

  draw_map_cursor_sprite(interface->get_map_cursor_pos(),
                         interface->get_map_cursor_sprite(0));

  for (int d = 0; d < 6; d++) {
    draw_map_cursor_sprite(MAP_MOVE(interface->get_map_cursor_pos(), d),
                           interface->get_map_cursor_sprite(1+d));
  }
}

void
viewport_t::draw_base_grid_overlay(int color) {
  int x_base = -(offset_x + 16*(offset_y/20)) % 32;
  int y_base = -offset_y % 20;

  int row = 0;
  for (int y = y_base; y < height; y += MAP_TILE_HEIGHT, row++) {
    frame->fill_rect(0, y, width, 1, color);
    for (int x = x_base + ((row % 2 == 0) ? 0 : -MAP_TILE_WIDTH/2);
         x < width; x += MAP_TILE_WIDTH) {
      frame->fill_rect(x, y + y - 2, 1, 5, color);
    }
  }
}

void
viewport_t::draw_height_grid_overlay(int color) {
  int x_off = -(offset_x + 16*(offset_y/20)) % 32;
  int y_off = -offset_y % 20;

  int col_0 = (offset_x/16 + offset_y/20)/2 & game.map.col_mask;
  int row_0 = (offset_y/MAP_TILE_HEIGHT) & game.map.row_mask;
  map_pos_t base_pos = MAP_POS(col_0, row_0);

  for (int x_base = x_off; x_base < width + MAP_TILE_WIDTH;
       x_base += MAP_TILE_WIDTH) {
    map_pos_t pos = base_pos;
    int y_base = y_off;
    int row = 0;

    while (1) {
      int x;
      if (row % 2 == 0) {
        x = x_base;
      } else {
        x = x_base - MAP_TILE_WIDTH/2;
      }

      int y = y_base - 4*MAP_HEIGHT(pos);
      if (y >= height) break;

      /* Draw cross. */
      if (pos != MAP_POS(0, 0)) {
        frame->fill_rect(x, y - 1, 1, 3, color);
        frame->fill_rect(x - 1, y, 3, 1, color);
      } else {
        frame->fill_rect(x, y - 2, 1, 5, color);
        frame->fill_rect(x - 2, y, 5, 1, color);
      }

      if (row % 2 == 0) {
        pos = MAP_MOVE_DOWN(pos);
      } else {
        pos = MAP_MOVE_DOWN_RIGHT(pos);
      }

      y_base += MAP_TILE_HEIGHT;
      row += 1;
    }

    base_pos = MAP_MOVE_RIGHT(base_pos);
  }
}

void
viewport_t::internal_draw() {
  if (layers & VIEWPORT_LAYER_LANDSCAPE) {
    draw_landscape();
  }
  if (layers & VIEWPORT_LAYER_GRID) {
    draw_base_grid_overlay(72);
    draw_height_grid_overlay(76);
  }
  if (layers & VIEWPORT_LAYER_PATHS) {
    draw_paths_and_borders();
  }
  draw_game_objects(layers);
  if (layers & VIEWPORT_LAYER_CURSOR) {
    draw_map_cursor();
  }
}

bool
viewport_t::handle_click_left(int x, int y) {
  set_redraw();

  map_pos_t clk_pos = map_pos_from_screen_pix(x, y);
  int clk_col = MAP_POS_COL(clk_pos);
  int clk_row = MAP_POS_ROW(clk_pos);

  if (interface->is_building_road()) {
    int x = (clk_col - MAP_POS_COL(interface->get_map_cursor_pos()) + 1) &
            game.map.col_mask;
    int y = (clk_row - MAP_POS_ROW(interface->get_map_cursor_pos()) + 1) &
            game.map.row_mask;
    int dir = -1;

    if (x == 0) {
      if (y == 1) dir = DIR_LEFT;
      else if (y == 0) dir = DIR_UP_LEFT;
    } else if (x == 1) {
      if (y == 2) dir = DIR_DOWN;
      else if (y == 0) dir = DIR_UP;
    } else if (x == 2) {
      if (y == 1) dir = DIR_RIGHT;
      else if (y == 2) dir = DIR_DOWN_RIGHT;
    }

    if (interface->build_roid_is_valid_dir((dir_t)dir)) {
      int length = interface->get_building_road_length();
      dir_t last_dir = DIR_RIGHT;
      if (length > 0) last_dir = interface->get_building_road_dir(length-1);

      if (length > 0 && DIR_REVERSE(last_dir) == dir) {
        /* Delete existing path */
        int r = interface->remove_road_segment();
        if (r < 0) {
          play_sound(SFX_NOT_ACCEPTED);
        } else {
          play_sound(SFX_CLICK);
        }
      } else {
        /* Build new road segment */
        int r = interface->build_road_segment((dir_t)dir);
        if (r < 0) {
          play_sound(SFX_NOT_ACCEPTED);
        } else if (r == 0) {
          play_sound(SFX_CLICK);
        } else {
          play_sound(SFX_ACCEPTED);
        }
      }
    }
  } else {
    interface->update_map_cursor_pos(clk_pos);
    play_sound(SFX_CLICK);
  }

  return true;
}

bool
viewport_t::handle_dbl_click(int x, int y, event_button_t button) {
  if (button != EVENT_BUTTON_LEFT) return 0;

  set_redraw();

  map_pos_t clk_pos = map_pos_from_screen_pix(x, y);

  if (interface->is_building_road()) {
    if (clk_pos != interface->get_map_cursor_pos()) {
      map_pos_t pos = interface->get_building_road_source();
      unsigned int length = 0;
      dir_t *dirs = pathfinder_map(pos, clk_pos, &length);
      if (dirs != NULL) {
        interface->build_road_reset();
        int r = interface->extend_road(dirs, length);
        if (r < 0) {
          play_sound(SFX_NOT_ACCEPTED);
        } else if (r == 1) {
          play_sound(SFX_ACCEPTED);
        } else {
          play_sound(SFX_CLICK);
        }
        free(dirs);
      } else {
        play_sound(SFX_NOT_ACCEPTED);
      }
    } else {
      int r = game_build_flag(interface->get_map_cursor_pos(),
                              interface->get_player());
      if (r < 0) {
        play_sound(SFX_NOT_ACCEPTED);
      } else {
        interface->build_road();
      }
    }
  } else {
    if (MAP_OBJ(clk_pos) == MAP_OBJ_NONE ||
        MAP_OBJ(clk_pos) > MAP_OBJ_CASTLE) {
      return false;
    }

    if (MAP_OBJ(clk_pos) == MAP_OBJ_FLAG) {
      if (MAP_OWNER(clk_pos) == interface->get_player()->get_index()) {
        interface->open_popup(BOX_TRANSPORT_INFO);
      }

      interface->get_player()->temp_index = MAP_OBJ_INDEX(clk_pos);
    } else { /* Building */
      if (MAP_OWNER(clk_pos) == interface->get_player()->get_index()) {
        building_t *building = game.buildings[MAP_OBJ_INDEX(clk_pos)];
        if (!building->is_done()) {
          interface->open_popup(BOX_ORDERED_BLD);
          } else if (building->get_type() == BUILDING_CASTLE) {
          interface->open_popup(BOX_CASTLE_RES);
        } else if (building->get_type() == BUILDING_STOCK) {
          if (!building->is_active()) return 0;
          interface->open_popup(BOX_CASTLE_RES);
        } else if (building->get_type() == BUILDING_HUT ||
             building->get_type() == BUILDING_TOWER ||
             building->get_type() == BUILDING_FORTRESS) {
          interface->open_popup(BOX_DEFENDERS);
        } else if (building->get_type() == BUILDING_STONEMINE ||
             building->get_type() == BUILDING_COALMINE ||
             building->get_type() == BUILDING_IRONMINE ||
             building->get_type() == BUILDING_GOLDMINE) {
          interface->open_popup(BOX_MINE_OUTPUT);
        } else {
          interface->open_popup(BOX_BLD_STOCK);
        }

        interface->get_player()->temp_index = MAP_OBJ_INDEX(clk_pos);
      } else if (BIT_TEST(game.split, 5)) { /* Demo mode*/
        return false;
      } else { /* Foreign building */
        /* TODO handle coop mode*/
        building_t *building = game.buildings[MAP_OBJ_INDEX(clk_pos)];
        interface->get_player()->building_attacked = building->get_index();

        if (building->is_done() &&
            building->is_military()) {
          if (!building->is_active() ||
              building->get_state() != 3) {
            /* It is not allowed to attack
               if currently not occupied or
               is too far from the border. */
            play_sound(SFX_NOT_ACCEPTED);
            return false;
          }

          const map_pos_t *p = &game.spiral_pos_pattern[7];
          int found = 0;
          for (int i = 257; i >= 0; i--) {
            map_pos_t pos = MAP_POS_ADD(building->get_position(), p[257-i]);
            if (MAP_HAS_OWNER(pos) &&
                MAP_OWNER(pos) == interface->get_player()->get_index()) {
              found = 1;
              break;
            }
          }

          if (!found) {
            play_sound(SFX_NOT_ACCEPTED);
            return false;
          }

          /* Action accepted */
          play_sound(SFX_CLICK);

          int max_knights = 0;
          switch (building->get_type()) {
          case BUILDING_HUT: max_knights = 3; break;
          case BUILDING_TOWER: max_knights = 6; break;
          case BUILDING_FORTRESS: max_knights = 12; break;
          case BUILDING_CASTLE: max_knights = 20; break;
          default: NOT_REACHED(); break;
          }

          int knights = interface->get_player()->
                         knights_available_for_attack(building->get_position());
          interface->get_player()->knights_attacking =
                                                 std::min(knights, max_knights);
          interface->open_popup(BOX_START_ATTACK);
        }
      }
    }
  }

  return false;
}

bool
viewport_t::handle_drag(int x, int y) {
  if (x != 0 || y != 0) {
    move_by_pixels(x, y);
  }

  return true;
}

viewport_t::viewport_t(interface_t *interface) {
  this->interface = interface;
  layers = VIEWPORT_LAYER_ALL;

  last_tick = 0;

  data_t *data = data_t::get_instance();
  data_source = data->get_data_source();
}

viewport_t::~viewport_t() {
  map_deinit();
}

/* Space transformations. */
/* The game world space is a three dimensional space with the axes
   named "column", "row" and "height". The (column, row) coordinate
   can be encoded as a map_pos_t.

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
viewport_t::screen_pix_from_map_pix(int mx, int my, int *sx, int *sy) {
  int width = game.map.cols*MAP_TILE_WIDTH;
  int height = game.map.rows*MAP_TILE_HEIGHT;

  *sx = mx - offset_x;
  *sy = my - offset_y;

  while (*sy < 0) {
    *sx -= (game.map.rows*MAP_TILE_WIDTH)/2;
    *sy += height;
  }

  while (*sy >= height) {
    *sx += (game.map.rows*MAP_TILE_WIDTH)/2;
    *sy -= height;
  }

  while (*sx < 0) *sx += width;
  while (*sx >= width) *sx -= width;
}

void
viewport_t::map_pix_from_map_coord(map_pos_t pos, int h, int *mx, int *my) {
  int width = game.map.cols*MAP_TILE_WIDTH;
  int height = game.map.rows*MAP_TILE_HEIGHT;

  *mx = MAP_TILE_WIDTH*MAP_POS_COL(pos) - (MAP_TILE_WIDTH/2)*MAP_POS_ROW(pos);
  *my = MAP_TILE_HEIGHT*MAP_POS_ROW(pos) - 4*h;

  if (*my < 0) {
    *mx -= (game.map.rows*MAP_TILE_WIDTH)/2;
    *my += height;
  }

  if (*mx < 0) *mx += width;
  else if (*mx >= width) *mx -= width;
}

map_pos_t
viewport_t::map_pos_from_screen_pix(int sx, int sy) {
  int x_off = -(offset_x + 16*(offset_y/20)) % 32;
  int y_off = -offset_y % 20;

  int col = (offset_x/16 + offset_y/20)/2 & game.map.col_mask;
  int row = (offset_y/MAP_TILE_HEIGHT) & game.map.row_mask;

  sx -= x_off;
  sy -= y_off;

  int y_base = -4;
  int col_offset = (sx + 24) >> 5;
  if (!BIT_TEST(sx + 24, 4)) {
    row += 1;
    y_base = 16;
  }

  col = (col + col_offset) & game.map.col_mask;
  row = row & game.map.row_mask;

  int y;
  int last_y = -100;

  while (1) {
    y = y_base - 4*MAP_HEIGHT(MAP_POS(col, row));
    if (sy < y) break;

    last_y = y;
    col = (col + 1) & game.map.col_mask;
    row = (row + 2) & game.map.row_mask;
    y_base += 2*MAP_TILE_HEIGHT;
  }

  if (sy < (y + last_y)/2) {
    col = (col - 1) & game.map.col_mask;
    row = (row - 2) & game.map.row_mask;
  }

  return MAP_POS(col, row);
}

map_pos_t
viewport_t::get_current_map_pos() {
  return map_pos_from_screen_pix(width/2, height/2);
}

void
viewport_t::move_to_map_pos(map_pos_t pos) {
  int mx, my;
  map_pix_from_map_coord(pos, MAP_HEIGHT(pos), &mx, &my);

  int map_width = game.map.cols*MAP_TILE_WIDTH;
  int map_height = game.map.rows*MAP_TILE_HEIGHT;

  /* Center screen. */
  mx -= width/2;
  my -= height/2;

  if (my < 0) {
    mx -= (game.map.rows*MAP_TILE_WIDTH)/2;
    my += map_height;
  }

  if (mx < 0) mx += map_width;
  else if (mx >= map_width) mx -= map_width;

  offset_x = mx;
  offset_y = my;

  set_redraw();
}

void
viewport_t::move_by_pixels(int x, int y) {
  int width = game.map.cols*MAP_TILE_WIDTH;
  int height = game.map.rows*MAP_TILE_HEIGHT;

  offset_x += x;
  offset_y += y;

  if (offset_y < 0) {
    offset_y += height;
    offset_x -= (game.map.rows*MAP_TILE_WIDTH)/2;
  } else if (offset_y >= height) {
    offset_y -= height;
    offset_x += (game.map.rows*MAP_TILE_WIDTH)/2;
  }

  if (offset_x >= width) offset_x -= width;
  else if (offset_x < 0) offset_x += width;

  set_redraw();
}


/* Called periodically when the game progresses. */
void
viewport_t::update() {
  int tick_xor = game.tick ^ last_tick;
  last_tick = game.tick;

  /* Viewport animation does not care about low bits in anim */
  if (tick_xor >= 1 << 3) {
    set_redraw();
  }
}
