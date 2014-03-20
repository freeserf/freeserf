/*
 * viewport.c - Viewport GUI component
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "viewport.h"
#include "interface.h"
#include "panel.h"
#include "gfx.h"
#include "data.h"
#include "map.h"
#include "random.h"
#include "game.h"
#include "misc.h"
#include "debug.h"
#include "audio.h"
#include "pathfinder.h"

#define MAP_TILE_WIDTH   32
#define MAP_TILE_HEIGHT  20

#define MAP_TILE_TEXTURES  33
#define MAP_TILE_MASKS     81

#define VIEWPORT_COLS(viewport)  (2*((viewport)->obj.width / MAP_TILE_WIDTH) + 1)


/* Cache prerendered tiles of the landscape. */
typedef struct {
	frame_t frame;
	int dirty;
} landscape_tile_t;

/* Number of cols,rows in each landscape tile */
#define MAP_TILE_COLS  16
#define MAP_TILE_ROWS  16

static uint landscape_tile_count;
static landscape_tile_t *landscape_tile;


static const uint8_t tri_spr[] = {
	32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32, 32, 32, 32,	32, 32,
	32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32, 32,	32, 32, 32, 32,
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

static void
draw_triangle_up(int x, int y, int m, int left, int right, map_pos_t pos, frame_t *frame)
{
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

	gfx_draw_masked_sprite(x, y,
		      DATA_MAP_MASK_UP_BASE + mask,
		      DATA_MAP_GROUND_BASE + sprite, frame);
}

static void
draw_triangle_down(int x, int y, int m, int left, int right, map_pos_t pos, frame_t *frame)
{
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

	gfx_draw_masked_sprite(x, y + MAP_TILE_HEIGHT,
		      DATA_MAP_MASK_DOWN_BASE + mask,
		      DATA_MAP_GROUND_BASE + sprite, frame);
}

/* Draw a column (vertical) of tiles, starting at an up pointing tile. */
static void
draw_up_tile_col(map_pos_t pos, int x_base, int y_base, int max_y, frame_t *frame)
{
	int m = MAP_HEIGHT(pos);
	int left, right;

	/* Loop until a tile is inside the frame (y >= 0). */
	while (1) {
		/* move down */
		pos = MAP_MOVE_DOWN(pos);

		left = MAP_HEIGHT(pos);
		right = MAP_HEIGHT(MAP_MOVE_RIGHT(pos));

		int t = min(left, right);
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

		int t = max(left, right);
		if (y_base - 2*MAP_TILE_HEIGHT - 4*t >= max_y) break;

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
static void
draw_down_tile_col(map_pos_t pos, int x_base, int y_base, int max_y, frame_t *frame)
{
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

		int t = min(left, right);
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

		int t = max(left, right);
		/*if (left == right) t += 1;*/ /* TODO ? */

		if (y_base - 2*MAP_TILE_HEIGHT - 4*t >= max_y) break;

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
viewport_map_deinit()
{
	if (landscape_tile != NULL) {
		for (int i = 0; i < landscape_tile_count; i++) {
			gfx_frame_deinit(&landscape_tile[i].frame);
		}
		free(landscape_tile);
		landscape_tile = NULL;
	}
}

/* Reinitialize landscape tiles */
void
viewport_map_reinit()
{
	viewport_map_deinit();

	int horiz_tiles = game.map.cols/MAP_TILE_COLS;
	int vert_tiles = game.map.rows/MAP_TILE_ROWS;
	landscape_tile_count = horiz_tiles*vert_tiles;

	int tile_width = MAP_TILE_COLS*MAP_TILE_WIDTH;
	int tile_height = MAP_TILE_ROWS*MAP_TILE_HEIGHT;

	landscape_tile = malloc(landscape_tile_count*sizeof(landscape_tile_t));
	if (landscape_tile == NULL) abort();

	LOGV("viewport", "map: %i,%i, cols,rows: %i,%i, tcs,trs: %i,%i, tw,th: %i,%i",
	     game.map.cols*MAP_TILE_WIDTH, game.map.rows*MAP_TILE_HEIGHT,
	     game.map.cols, game.map.rows, horiz_tiles, vert_tiles,
	     tile_width, tile_height);

	for (int i = 0; i < landscape_tile_count; i++) {
		gfx_frame_init(&landscape_tile[i].frame, 0, 0, tile_width, tile_height, NULL);
		gfx_fill_rect(0, 0, tile_width, tile_height, 0, &landscape_tile[i].frame);
		landscape_tile[i].dirty = 1;
	}
}

void
viewport_redraw_map_pos(viewport_t *viewport, map_pos_t pos)
{
	if (landscape_tile == NULL) return;

	int mx, my;
	viewport_map_pix_from_map_coord(viewport, pos, MAP_HEIGHT(pos),
					&mx, &my);

	int horiz_tiles = game.map.cols/MAP_TILE_COLS;
	int vert_tiles = game.map.rows/MAP_TILE_ROWS;

	int tile_width = MAP_TILE_COLS*MAP_TILE_WIDTH;
	int tile_height = MAP_TILE_ROWS*MAP_TILE_HEIGHT;

	int tc = (mx / tile_width) % horiz_tiles;
	int tr = (my / tile_height) % vert_tiles;
	int tid = tc + horiz_tiles*tr;

	landscape_tile[tid].dirty = 1;
}

static void
draw_landscape(viewport_t *viewport, frame_t *frame)
{
	int horiz_tiles = game.map.cols/MAP_TILE_COLS;
	int vert_tiles = game.map.rows/MAP_TILE_ROWS;

	int tile_width = MAP_TILE_COLS*MAP_TILE_WIDTH;
	int tile_height = MAP_TILE_ROWS*MAP_TILE_HEIGHT;

	int map_width = game.map.cols*MAP_TILE_WIDTH;
	int map_height = game.map.rows*MAP_TILE_HEIGHT;

	int my = viewport->offset_y;
	int y = 0;
	int x_base = 0;
	while (y < viewport->obj.height) {
		while (my >= map_height) {
			my -= map_height;
			x_base += (game.map.rows*MAP_TILE_WIDTH)/2;
		}

		int ty = my % tile_height;

		int x = 0;
		while (x < viewport->obj.width) {
			int mx = (viewport->offset_x + x_base + x) % map_width;
			int tx = mx % tile_width;

			int tc = (mx / tile_width) % horiz_tiles;
			int tr = (my / tile_height) % vert_tiles;
			int tid = tc + horiz_tiles*tr;

			/* Redraw tile if marked dirty */
			if (landscape_tile[tid].dirty) {
				int col = (tc*MAP_TILE_COLS + (tr*MAP_TILE_ROWS)/2) % game.map.cols;
				int row = tr*MAP_TILE_ROWS;
				map_pos_t pos = MAP_POS(col, row);

				int x_base = -(MAP_TILE_WIDTH/2);

				/* Draw one extra column as half a column will be outside the
				   map tile on both right and left side.. */
				for (int col = 0; col < MAP_TILE_COLS+1; col++) {
					draw_up_tile_col(pos, x_base, 0, tile_height,
							 &landscape_tile[tid].frame);
					draw_down_tile_col(pos, x_base + 16, 0, tile_height,
							   &landscape_tile[tid].frame);

					pos = MAP_MOVE_RIGHT(pos);
					x_base += MAP_TILE_WIDTH;
				}

#if 0
				/* Draw a border around the tile for debug. */
				sdl_draw_rect(0, 0, tile_width, tile_height,
					      76, &landscape_tile[tid].frame);
#endif

				landscape_tile[tid].dirty = 0;
			}

			gfx_draw_frame(x, y, frame, tx, ty,
				       &landscape_tile[tid].frame,
				       viewport->obj.width - x,
				       viewport->obj.height - y);
			x += tile_width - tx;
		}

		y += tile_height - ty;
		my += tile_height - ty;
	}
}


static void
draw_path_segment(int x, int y, map_pos_t pos, dir_t dir, frame_t *frame)
{
	int h1 = MAP_HEIGHT(pos);
	int h2 = MAP_HEIGHT(MAP_MOVE(pos, dir));
	int h_diff = h1 - h2;

	int t1, t2, h3, h4, h_diff_2;

	switch (dir) {
	case DIR_RIGHT:
		y -= 4*max(h1, h2) + 2;
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
	int type = max(t1, t2);

	if (h_diff_2 > 4) sprite = 0;
	else if (h_diff_2 > -6) sprite = 1;
	else sprite = 2;

	if (type < 4) sprite = 9;
	else if (type > 13) sprite += 6;
	else if (type > 7) sprite += 3;

	gfx_draw_masked_sprite(x, y, DATA_PATH_MASK_BASE + mask,
		      DATA_PATH_GROUND_BASE + sprite, frame);
}

static void
draw_border_segment(int x, int y, map_pos_t pos, dir_t dir, frame_t *frame)
{
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
	int type = max(t1, t2);

	if (h_diff_2 > 1) sprite = 0;
	else if (h_diff_2 > -9) sprite = 1;
	else sprite = 2;

	if (type < 4) sprite = 9; /* Bouy */
	else if (type > 13) sprite += 6;
	else if (type > 7) sprite += 3;

	gfx_draw_transp_sprite(x, y, DATA_MAP_BORDER_BASE + sprite, 0, 0, 0,
			       frame);
}

static void
draw_paths_and_borders(viewport_t *viewport, frame_t *frame)
{
	int x_off = -(viewport->offset_x + 16*(viewport->offset_y/20)) % 32;
	int y_off = -viewport->offset_y % 20;

	int col_0 = (viewport->offset_x/16 + viewport->offset_y/20)/2 & game.map.col_mask;
	int row_0 = (viewport->offset_y/MAP_TILE_HEIGHT) & game.map.row_mask;
	map_pos_t base_pos = MAP_POS(col_0, row_0);

	for (int x_base = x_off; x_base < viewport->obj.width + MAP_TILE_WIDTH; x_base += MAP_TILE_WIDTH) {
		map_pos_t pos = base_pos;
		int y_base = y_off;
		int row = 0;

		while (1) {
			int x;
			if (row % 2 == 0) x = x_base;
			else x = x_base - MAP_TILE_WIDTH/2;

			int y = y_base - 4*MAP_HEIGHT(pos);
			if (y >= viewport->obj.height) break;

			/* For each direction right, down right and down,
			   draw the corresponding paths and borders. */
			for (dir_t d = DIR_RIGHT; d <= DIR_DOWN; d++) {
				map_tile_t *tiles = game.map.tiles;
				map_pos_t other_pos = MAP_MOVE(pos, d);

				if (BIT_TEST(tiles[pos].paths, d)) {
					draw_path_segment(x, y_base, pos, d, frame);
				} else if (MAP_HAS_OWNER(pos) != MAP_HAS_OWNER(other_pos) ||
					   MAP_OWNER(pos) != MAP_OWNER(other_pos)) {
					draw_border_segment(x, y_base, pos, d, frame);
				}
			}

			if (row % 2 == 0) pos = MAP_MOVE_DOWN(pos);
			else pos = MAP_MOVE_DOWN_RIGHT(pos);

			y_base += MAP_TILE_HEIGHT;
			row += 1;
		}

		base_pos = MAP_MOVE_RIGHT(base_pos);
	}

	/* If we're in road construction mode, also draw
	   the temporarily placed roads. */
	interface_t *interface = viewport->interface;
	if (interface->building_road) {
		map_pos_t pos = interface->building_road_source;
		for (int i = 0; i < interface->building_road_length; i++) {
			dir_t dir = interface->building_road_dirs[i];

			map_pos_t draw_pos = pos;
			dir_t draw_dir = dir;
			if (draw_dir > DIR_DOWN) {
				draw_pos = MAP_MOVE(pos, dir);
				draw_dir = DIR_REVERSE(dir);
			}

			int mx, my;
			viewport_map_pix_from_map_coord(viewport, draw_pos,
							MAP_HEIGHT(draw_pos),
							&mx, &my);

			int sx, sy;
			viewport_screen_pix_from_map_pix(viewport, mx, my, &sx, &sy);

			draw_path_segment(sx, sy + 4*MAP_HEIGHT(draw_pos), draw_pos,
					  draw_dir, frame);

			pos = MAP_MOVE(pos, dir);
		}
	}
}

static void
draw_game_sprite(int x, int y, int index, frame_t *frame)
{
	gfx_draw_transp_sprite(x, y, DATA_GAME_OBJECT_BASE + index, 1, 0, 0, frame);
}

static void
draw_serf(int x, int y, int color, int head, int body, frame_t *frame)
{
	gfx_draw_transp_sprite(x, y, DATA_SERF_ARMS_BASE + body, 1, 0, 0, frame);
	gfx_draw_transp_sprite(x, y, DATA_SERF_TORSO_BASE + body, 1, 0, color, frame);

	if (head >= 0) {
		int ax, ay;
		gfx_get_sprite_offset(DATA_SERF_ARMS_BASE + body, &ax, &ay);
		x += ax;
		y += ay;
		gfx_draw_transp_sprite(x, y, DATA_SERF_HEAD_BASE + head, 1, 0, 0, frame);
	}
}

static void
draw_shadow_and_building_sprite(int x, int y, int index, frame_t *frame)
{
	gfx_draw_overlay_sprite(x, y, DATA_MAP_SHADOW_BASE + index, 0, frame);
	gfx_draw_transp_sprite(x, y, DATA_MAP_OBJECT_BASE + index, 1, 0, 0, frame);
}

static void
draw_shadow_and_building_unfinished(int x, int y, int index, int progress, frame_t *frame)
{
	int bw, bh;
	gfx_get_sprite_size(DATA_MAP_OBJECT_BASE + index, &bw, &bh);
	int h = ((bh * progress) >> 16) + 1;
	int y_off = bh - h;

	gfx_draw_overlay_sprite(x, y, DATA_MAP_SHADOW_BASE + index, y_off, frame);
	gfx_draw_transp_sprite(x, y, DATA_MAP_OBJECT_BASE + index, 1, y_off, 0, frame);
}

static const int map_building_frame_sprite[] = {
	0, 0xba, 0xba, 0xba, 0xba,
	0xb9, 0xb9, 0xb9, 0xb9,
	0xba, 0xc1, 0xba, 0xb1, 0xb8, 0xb1, 0xbb,
	0xb7, 0xb5, 0xb6, 0xb0, 0xb8, 0xb3, 0xaf, 0xb4
};


static void
draw_building_unfinished(building_t *building, building_type_t bld_type, int x, int y, frame_t *frame)
{
	if (building->progress == 0) { /* Draw cross */
		draw_shadow_and_building_sprite(x, y, 0x90, frame);
	} else {
		/* Stone waiting */
		int stone = building->stock[1].available;
		for (int i = 0; i < stone; i++) {
			draw_game_sprite(x+10 - i*3, y-8 + i, 1 + RESOURCE_STONE, frame);
		}

		/* Planks waiting */
		int planks = building->stock[0].available;
		for (int i = 0; i < planks; i++) {
			draw_game_sprite(x+12 - i*3, y-6 + i, 1 + RESOURCE_PLANK, frame);
		}

		if (BIT_TEST(building->progress, 15)) { /* Frame finished */
			draw_shadow_and_building_sprite(x, y, map_building_frame_sprite[bld_type], frame);
			draw_shadow_and_building_unfinished(x, y, map_building_sprite[bld_type], 2*(building->progress & 0x7fff), frame);
		} else {
			draw_shadow_and_building_sprite(x, y, 0x91, frame); /* corner stone */
			if (building->progress > 1) {
				draw_shadow_and_building_unfinished(x, y, map_building_frame_sprite[bld_type],
								    2*building->progress, frame);
			}
		}
	}
}

static void
draw_unharmed_building(viewport_t *viewport, building_t *building,
		       int x, int y, frame_t *frame)
{
	random_state_t *random = &viewport->interface->random;

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

	if (BUILDING_IS_DONE(building)) {
		building_type_t type = building->type;
		switch (building->type) {
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
			draw_shadow_and_building_sprite(x, y, map_building_sprite[type], frame);
			break;
		case BUILDING_BOATBUILDER:
			draw_shadow_and_building_sprite(x, y, map_building_sprite[type], frame);
			if (building->stock[1].available > 0) {
				/* TODO x might not be correct */
				draw_game_sprite(x+3, y + 13, 174 + building->stock[1].available, frame);
			}
			break;
		case BUILDING_STONEMINE:
		case BUILDING_COALMINE:
		case BUILDING_IRONMINE:
		case BUILDING_GOLDMINE:
			if (BUILDING_IS_ACTIVE(building)) { /* Draw elevator up */
				draw_game_sprite(x-6, y-39, 152, frame);
			}
			if (BUILDING_PLAYING_SFX(building)) { /* Draw elevator down */
				draw_game_sprite(x-6, y-39, 153, frame);
				if ((((game.tick + ((uint8_t *)&building->pos)[1]) >> 3) & 7) == 0 &&
				    random_int(random) < 40000) {
					sfx_play_clip(SFX_ELEVATOR);
				}
			}
			draw_shadow_and_building_sprite(x, y, map_building_sprite[type], frame);
			break;
		case BUILDING_HUT:
			draw_shadow_and_building_sprite(x, y, map_building_sprite[type], frame);
			if (building->serf_index != 0) {
				draw_game_sprite(x-14, y+2 - 2*building->stock[0].available,
						 182 + ((game.tick >> 3) & 3) + 4*BUILDING_STATE(building),
						 frame);
			}
			break;
		case BUILDING_PIGFARM:
			draw_shadow_and_building_sprite(x, y, map_building_sprite[type], frame);
			if (building->stock[1].available > 0) {
				if ((random_int(random) & 0x7f) < building->stock[1].available) {
					sfx_play_clip(SFX_PIG_OINK);
				}

				if (building->stock[1].available >= 6) {
					int i = (140 + (game.tick >> 3)) & 0xfe;
					draw_game_sprite(x + pigfarm_anim[i+1] - 2, y+6, pigfarm_anim[i], frame);
				}

				if (building->stock[1].available >= 5) {
					int i = (280 + (game.tick >> 3)) & 0xfe;
					draw_game_sprite(x + pigfarm_anim[i+1] + 8, y+8, pigfarm_anim[i], frame);
				}

				if (building->stock[1].available >= 3) {
					int i = (420 + (game.tick >> 3)) & 0xfe;
					draw_game_sprite(x + pigfarm_anim[i+1] - 11, y+8, pigfarm_anim[i], frame);
				}

				int i = (40 + (game.tick >> 3)) & 0xfe;
				draw_game_sprite(x + pigfarm_anim[i+1] + 2, y+11, pigfarm_anim[i], frame);

				if (building->stock[1].available >= 7) {
					int i = (180 + (game.tick >> 3)) & 0xfe;
					draw_game_sprite(x + pigfarm_anim[i+1] - 8, y+13, pigfarm_anim[i], frame);
				}

				if (building->stock[1].available >= 8) {
					int i = (320 + (game.tick >> 3)) & 0xfe;
					draw_game_sprite(x + pigfarm_anim[i+1] + 13, y+14, pigfarm_anim[i], frame);
				}

				if (building->stock[1].available >= 2) {
					int i = (460 + (game.tick >> 3)) & 0xfe;
					draw_game_sprite(x + pigfarm_anim[i+1], y+17, pigfarm_anim[i], frame);
				}

				if (building->stock[1].available >= 4) {
					int i = (90 + (game.tick >> 3)) & 0xfe;
					draw_game_sprite(x + pigfarm_anim[i+1] - 11, y+19, pigfarm_anim[i], frame);
				}
			}
			break;
		case BUILDING_MILL:
			if (BUILDING_IS_ACTIVE(building)) {
				if ((game.tick >> 4) & 3) {
					building->serf &= ~BIT(3);
				} else if (!BUILDING_PLAYING_SFX(building)) {
					building->serf |= BIT(3);
					sfx_play_clip(SFX_MILL_GRINDING);
				}
				draw_shadow_and_building_sprite(x, y, map_building_sprite[type] +
								((game.tick >> 4) & 3), frame);
			} else {
				draw_shadow_and_building_sprite(x, y, map_building_sprite[type], frame);
			}
			break;
		case BUILDING_BAKER:
			draw_shadow_and_building_sprite(x, y, map_building_sprite[type], frame);
			if (BUILDING_IS_ACTIVE(building)) {
				draw_game_sprite(x + 5, y-21, 154 + ((game.tick >> 3) & 7), frame);
			}
			break;
		case BUILDING_STEELSMELTER:
			draw_shadow_and_building_sprite(x, y, map_building_sprite[type], frame);
			if (BUILDING_IS_ACTIVE(building)) {
				int i = (game.tick >> 3) & 7;
				if (i == 0 || (i == 7 && !BUILDING_PLAYING_SFX(building))) {
					building->serf |= BIT(3);
					sfx_play_clip(SFX_GOLD_BOILS);
				} else if (i != 7) {
					building->serf &= ~BIT(3);
				}

				draw_game_sprite(x+6, y-32, 128+i, frame);
			}
			break;
		case BUILDING_WEAPONSMITH:
			draw_shadow_and_building_sprite(x, y, map_building_sprite[type], frame);
			if (BUILDING_IS_ACTIVE(building)) {
				draw_game_sprite(x-16, y-21, 128 + ((game.tick >> 3) & 7), frame);
			}
			break;
		case BUILDING_TOWER:
			draw_shadow_and_building_sprite(x, y, map_building_sprite[type], frame);
			if (building->serf_index != 0) {
				draw_game_sprite(x+13, y - 18 - building->stock[0].available,
						 182 + ((game.tick >> 3) & 3) + 4*BUILDING_STATE(building),
						 frame);
			}
			break;
		case BUILDING_FORTRESS:
			draw_shadow_and_building_sprite(x, y, map_building_sprite[type], frame);
			if (building->serf_index != 0) {
				draw_game_sprite(x-12, y - 21 - building->stock[0].available/2,
						 182 + ((game.tick >> 3) & 3) + 4*BUILDING_STATE(building),
						 frame);
				draw_game_sprite(x+22, y - 34 - (building->stock[0].available+1)/2,
						 182 + (((game.tick >> 3) + 2) & 3) + 4*BUILDING_STATE(building),
						 frame);
			}
			break;
		case BUILDING_GOLDSMELTER:
			draw_shadow_and_building_sprite(x, y, map_building_sprite[type], frame);
			if (BUILDING_IS_ACTIVE(building)) {
				int i = (game.tick >> 3) & 7;
				if (i == 0 || (i == 7 && !BUILDING_PLAYING_SFX(building))) {
					building->serf |= BIT(3);
					sfx_play_clip(SFX_GOLD_BOILS);
				} else if (i != 7) {
					building->serf &= ~BIT(3);
				}

				draw_game_sprite(x-7, y-33, 128+i, frame);
			}
			break;
		default:
			NOT_REACHED();
			break;
		}
	} else { /* unfinished building */
		if (building->type != BUILDING_CASTLE) {
			draw_building_unfinished(building, building->type, x, y, frame);
		} else {
			draw_shadow_and_building_unfinished(x, y, 0xb2, building->progress, frame);
		}
	}
}

static void
draw_burning_building(viewport_t *viewport, building_t *building,
		      int x, int y, frame_t *frame)
{
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
	if (((building->serf_index >> 3) & 3) == 3 &&
	    !BUILDING_PLAYING_SFX(building)) {
		building->serf |= BIT(3);
		sfx_play_clip(SFX_BURNING);
	} else {
		building->serf &= ~BIT(3);
	}

	uint16_t delta = game.tick - building->u.tick;
	building->u.tick = game.tick;

	if (building->serf_index >= delta) {
		building->serf_index -= delta; /* TODO this is also done in update_buildings(). */
		draw_unharmed_building(viewport, building, x, y, frame);

		int type = 0;
		if (BUILDING_IS_DONE(building) ||
		    building->progress >= 16000) {
			type = BUILDING_TYPE(building);
		}

		int offset = ((building->serf_index >> 3) & 7) ^ 7;
		const int *anim = building_burn_animation + building_anim_offset_from_type[type];
		while (anim[0] >= 0) {
			draw_game_sprite(x+anim[1], y+anim[2], 136 + anim[0] + offset, frame);
			offset = (offset + 3) & 7;
			anim += 3;
		}
	} else {
		building->serf_index = 0;
	}
}

static void
draw_building(viewport_t *viewport, map_pos_t pos, int x, int y, frame_t *frame)
{
	building_t *building = game_get_building(MAP_OBJ_INDEX(pos));

	if (BUILDING_IS_BURNING(building)) {
		draw_burning_building(viewport, building, x, y, frame);
	} else {
		draw_unharmed_building(viewport, building, x, y, frame);
	}
}

static void
draw_water_waves(map_pos_t pos, int x, int y, frame_t *frame)
{
	int sprite = DATA_MAP_WAVES_BASE + (((pos ^ 5) + (game.tick >> 3)) & 0xf);

	if (MAP_TYPE_DOWN(pos) < 4 && MAP_TYPE_UP(pos) < 4) {
		gfx_draw_waves_sprite(x - 16, y, 0, sprite, 0, frame);
	} else if (MAP_TYPE_DOWN(pos) < 4) {
		int mask = DATA_MAP_MASK_DOWN_BASE + 40;
		gfx_draw_waves_sprite(x, y, mask, sprite, 16, frame);
	} else {
		int mask = DATA_MAP_MASK_UP_BASE + 40;
		gfx_draw_waves_sprite(x - 16, y, mask, sprite, 0, frame);
	}
}

static void
draw_water_waves_row(map_pos_t pos, int y_base, int cols, int x_base, frame_t *frame)
{
	for (int i = 0; i < cols; i++, x_base += MAP_TILE_WIDTH, pos = MAP_MOVE_RIGHT(pos)) {
		if (MAP_TYPE_UP(pos) < 4 || MAP_TYPE_DOWN(pos) < 4) {
			/*player->water_in_view += 1;*/
			draw_water_waves(pos, x_base, y_base, frame);
		}
	}
}

static void
draw_flag_and_res(map_pos_t pos, int x, int y, frame_t *frame)
{
	flag_t *flag = game_get_flag(MAP_OBJ_INDEX(pos));

	if (flag->slot[0].type != RESOURCE_NONE) draw_game_sprite(x+6, y-4, flag->slot[0].type + 1, frame);
	if (flag->slot[1].type != RESOURCE_NONE) draw_game_sprite(x+10, y-2, flag->slot[1].type + 1, frame);
	if (flag->slot[2].type != RESOURCE_NONE) draw_game_sprite(x-4, y-4, flag->slot[2].type + 1, frame);

	int pl_num = FLAG_PLAYER(flag);
	int spr = 0x80 + (pl_num << 2) + ((game.tick >> 3) & 3);

	draw_shadow_and_building_sprite(x, y, spr, frame);

	if (flag->slot[3].type != RESOURCE_NONE) draw_game_sprite(x+10, y+2, flag->slot[3].type + 1, frame);
	if (flag->slot[4].type != RESOURCE_NONE) draw_game_sprite(x-8, y-2, flag->slot[4].type + 1, frame);
	if (flag->slot[5].type != RESOURCE_NONE) draw_game_sprite(x+6, y+4, flag->slot[5].type + 1, frame);
	if (flag->slot[6].type != RESOURCE_NONE) draw_game_sprite(x-8, y+2, flag->slot[6].type + 1, frame);
	if (flag->slot[7].type != RESOURCE_NONE) draw_game_sprite(x-4, y+4, flag->slot[7].type + 1, frame);
}

static void
draw_map_objects_row(viewport_t *viewport, map_pos_t pos, int y_base,
		     int cols, int x_base, frame_t *frame)
{
	for (int i = 0; i < cols; i++, x_base += MAP_TILE_WIDTH, pos = MAP_MOVE_RIGHT(pos)) {
		if (MAP_OBJ(pos) == MAP_OBJ_NONE) continue;

		int y = y_base - 4*MAP_HEIGHT(pos);
		if (MAP_OBJ(pos) < MAP_OBJ_TREE_0) {
			if (MAP_OBJ(pos) == MAP_OBJ_FLAG) {
				draw_flag_and_res(pos, x_base, y, frame);
			} else if (MAP_OBJ(pos) <= MAP_OBJ_CASTLE) {
				draw_building(viewport, pos, x_base, y, frame);
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
			draw_shadow_and_building_sprite(x_base, y, sprite, frame);
		}
	}
}

/* Draw one individual serf in the row. */
static void
draw_row_serf(int x, int y, int shadow, int color, int body, frame_t *frame)
{
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
		gfx_draw_overlay_sprite(x, y, DATA_SERF_SHADOW, 0, frame);
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

	draw_serf(x, y, color, head, base, frame);
}

/* Extracted from obsolete update_map_serf_rows(). */
/* Translate serf type into the corresponding sprite code. */
static int
serf_get_body(serf_t *serf, uint32_t *animation_table)
{
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

	uint8_t *tbl_ptr = ((uint8_t *)animation_table) +
		animation_table[serf->animation] +
		3*(serf->counter >> 3);
	int t = tbl_ptr[0];

	switch (SERF_TYPE(serf)) {
	case SERF_TRANSPORTER:
	case SERF_GENERIC:
		if (serf->state == SERF_STATE_IDLE_ON_PATH) return -1;
		else if ((serf->state == SERF_STATE_TRANSPORTING ||
			  serf->state == SERF_STATE_DELIVERING) && serf->s.walking.res != 0) {
			t += transporter_type[serf->s.walking.res];
		}
		break;
	case SERF_SAILOR:
		if (serf->state == SERF_STATE_TRANSPORTING && t < 0x80) {
			if (((t & 7) == 4 && !BIT_TEST(serf->type, 7)) ||
			    (t & 7) == 3) {
				serf->type |= BIT(7);
				sfx_play_clip(SFX_ROWING);
			} else {
				serf->type &= ~BIT(7);
			}
		}

		if ((serf->state == SERF_STATE_TRANSPORTING &&
		     serf->s.walking.res == 0) ||
		    serf->state == SERF_STATE_LOST_SAILOR ||
		    serf->state == SERF_STATE_FREE_SAILING) {
			if (t < 0x80) {
				if (((t & 7) == 4 && !BIT_TEST(serf->type, 7)) ||
				    (t & 7) == 3) {
					serf->type |= BIT(7);
					sfx_play_clip(SFX_ROWING);
				} else {
					serf->type &= ~BIT(7);
				}
			}
			t += 0x200;
		} else if (serf->state == SERF_STATE_TRANSPORTING) {
			t += sailor_type[serf->s.walking.res];
		} else {
			t += 0x100;
		}
		break;
	case SERF_DIGGER:
		if (t < 0x80) {
			t += 0x300;
		} else if (t == 0x83 || t == 0x84) {
			if (t == 0x83 || !BIT_TEST(serf->type, 7)) {
				serf->type |= BIT(7);
				sfx_play_clip(SFX_DIGGING);
			}
			t += 0x380;
		} else {
			serf->type &= ~BIT(7);
			t += 0x380;
		}
		break;
	case SERF_BUILDER:
		if (t < 0x80) {
			t += 0x500;
		} else if ((t & 7) == 4 || (t & 7) == 5) {
			if ((t & 7) == 4 || !BIT_TEST(serf->type, 7)) {
				serf->type |= BIT(7);
				sfx_play_clip(SFX_HAMMER_BLOW);
			}
			t += 0x580;
		} else {
			serf->type &= ~BIT(7);
			t += 0x580;
		}
		break;
	case SERF_TRANSPORTER_INVENTORY:
		if (serf->state == SERF_STATE_BUILDING_CASTLE) {
			return -1;
		} else {
			int res = -1;
			switch (serf->state) {
			case SERF_STATE_ENTERING_BUILDING:
				res = serf->s.entering_building.field_B;
				break;
			case SERF_STATE_LEAVING_BUILDING:
				res = serf->s.leaving_building.field_B;
				break;
			case SERF_STATE_READY_TO_ENTER:
				res = serf->s.ready_to_enter.field_B;
				break;
			case SERF_STATE_MOVE_RESOURCE_OUT:
			case SERF_STATE_DROP_RESOURCE_OUT:
				res = serf->s.move_resource_out.res;
				break;
			case SERF_STATE_WAIT_FOR_RESOURCE_OUT:
				res = 0; /* TODO */
				break;
			default:
				NOT_REACHED();
				break;
			}

			t += transporter_type[res];
		}
		break;
	case SERF_LUMBERJACK:
		if (t < 0x80) {
			if (serf->state == SERF_STATE_FREE_WALKING &&
			    serf->s.free_walking.neg_dist1 == -128 &&
			    serf->s.free_walking.neg_dist2 == 1) {
				t += 0x1000;
			} else {
				t += 0xb00;
			}
		} else if ((t == 0x86 && !BIT_TEST(serf->type, 7)) ||
			   t == 0x85) {
			serf->type |= BIT(7);
			sfx_play_clip(SFX_AX_BLOW);
			/* TODO Dangerous reference to unknown state vars.
			   It is probably free walking. */
			if (serf->s.free_walking.neg_dist2 == 0 &&
			    serf->counter < 64) {
				sfx_play_clip(SFX_TREE_FALL);
			}
			t += 0xe80;
		} else if (t != 0x86) {
			serf->type &= ~BIT(7);
			t += 0xe80;
		}
		break;
	case SERF_SAWMILLER:
		if (t < 0x80) {
			if (serf->state == SERF_STATE_LEAVING_BUILDING &&
			    serf->s.leaving_building.next_state == SERF_STATE_DROP_RESOURCE_OUT) {
				t += 0x1700;
			} else {
				t += 0xc00;
			}
		} else {
			/* player_num += 4; ??? */
			if (t == 0xb3 || t == 0xbb || t == 0xc3 || t == 0xcb ||
			    (!BIT_TEST(serf->type, 7) && (t == 0xb7 || t == 0xbf ||
							  t == 0xc7 || t == 0xcf))) {
				serf->type |= BIT(7);
				sfx_play_clip(SFX_SAWING);
			} else if (t != 0xb7 && t != 0xbf && t != 0xc7 && t != 0xcf) {
				serf->type &= ~BIT(7);
			}
			t += 0x1580;
		}
		break;
	case SERF_STONECUTTER:
		if (t < 0x80) {
			if ((serf->state == SERF_STATE_FREE_WALKING &&
			     serf->s.free_walking.neg_dist1 == -128 &&
			     serf->s.free_walking.neg_dist2 == 1) ||
			    (serf->state == SERF_STATE_STONECUTTING &&
			     serf->s.free_walking.neg_dist1 == 2)) {
				t += 0x1200;
			} else {
				t += 0xd00;
			}
		} else if (t == 0x85 || (t == 0x86 && !BIT_TEST(serf->type, 7))) {
			serf->type |= BIT(7);
			sfx_play_clip(SFX_PICK_BLOW);
			t += 0x1280;
		} else if (t != 0x86) {
			serf->type &= ~BIT(7);
			t += 0x1280;
		}
		break;
	case SERF_FORESTER:
		if (t < 0x80) {
			t += 0xe00;
		} else if (t == 0x86 || (t == 0x87 && !BIT_TEST(serf->type, 7))) {
			serf->type |= BIT(7);
			sfx_play_clip(SFX_PLANTING);
			t += 0x1080;
		} else if (t != 0x87) {
			serf->type &= ~BIT(7);
			t += 0x1080;
		}
		break;
	case SERF_MINER:
		if (t < 0x80) {
			if ((serf->state != SERF_STATE_MINING ||
			     serf->s.mining.res == 0) &&
			    (serf->state != SERF_STATE_LEAVING_BUILDING ||
			     serf->s.leaving_building.next_state != SERF_STATE_DROP_RESOURCE_OUT)) {
				t += 0x1800;
			} else {
				resource_type_t res = 0;

				switch (serf->state) {
				case SERF_STATE_MINING:
					res = serf->s.mining.res - 1;
					break;
				case SERF_STATE_LEAVING_BUILDING:
					res = serf->s.leaving_building.field_B - 1;
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
			if (serf->state == SERF_STATE_LEAVING_BUILDING &&
			    serf->s.leaving_building.next_state == SERF_STATE_DROP_RESOURCE_OUT) {
				if (serf->s.leaving_building.field_B == 1+RESOURCE_STEEL) t += 0x2900;
				else t += 0x2800;
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
			if (serf->state == SERF_STATE_FREE_WALKING &&
			    serf->s.free_walking.neg_dist1 == -128 &&
			    serf->s.free_walking.neg_dist2 == 1) {
				t += 0x2f00;
			} else {
				t += 0x2c00;
			}
		} else {
			if (t != 0x80 && t != 0x87 && t != 0x88 && t != 0x8f) {
				sfx_play_clip(SFX_FISHING_ROD_REEL);
			}

			/* TODO no check for state */
			if (serf->s.free_walking.neg_dist2 == 1) {
				t += 0x2d80;
			} else {
				t += 0x2c80;
			}
		}
		break;
	case SERF_PIGFARMER:
		if (t < 0x80) {
			if (serf->state == SERF_STATE_LEAVING_BUILDING &&
			    serf->s.leaving_building.next_state == SERF_STATE_DROP_RESOURCE_OUT) {
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
			if (serf->state == SERF_STATE_LEAVING_BUILDING &&
			    serf->s.leaving_building.next_state == SERF_STATE_DROP_RESOURCE_OUT) {
				t += 0x3a00;
			} else {
				t += 0x3700;
			}
		} else {
			/* edi10 += 4; */
			if ((t == 0xb2 || t == 0xba || t == 0xc2 || t == 0xca) &&
			    !BIT_TEST(serf->type, 7)) {
				serf->type |= BIT(7);
				sfx_play_clip(SFX_BACKSWORD_BLOW);
			} else if (t != 0xb2 && t != 0xba && t != 0xc2 && t != 0xca) {
				serf->type  &= ~BIT(7);
			}
			t += 0x3780;
		}
		break;
	case SERF_FARMER:
		if (t < 0x80) {
			if (serf->state == SERF_STATE_FREE_WALKING &&
			    serf->s.free_walking.neg_dist1 == -128 &&
			    serf->s.free_walking.neg_dist2 == 1) {
				t += 0x4000;
			} else {
				t += 0x3d00;
			}
		} else {
			/* TODO access to state without state check */
			if (serf->s.free_walking.neg_dist1 == 0) {
				t += 0x3d80;
			} else if (t == 0x83 || (t == 0x84 && !BIT_TEST(serf->type, 7))) {
				serf->type |= BIT(7);
				sfx_play_clip(SFX_MOWING);
				t += 0x3e80;
			} else if (t != 0x83 && t != 0x84) {
				serf->type &= ~BIT(7);
				t += 0x3e80;
			}
		}
		break;
	case SERF_MILLER:
		if (t < 0x80) {
			if (serf->state == SERF_STATE_LEAVING_BUILDING &&
			    serf->s.leaving_building.next_state == SERF_STATE_DROP_RESOURCE_OUT) {
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
			if (serf->state == SERF_STATE_LEAVING_BUILDING &&
			    serf->s.leaving_building.next_state == SERF_STATE_DROP_RESOURCE_OUT) {
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
			if (serf->state == SERF_STATE_LEAVING_BUILDING &&
			    serf->s.leaving_building.next_state == SERF_STATE_DROP_RESOURCE_OUT) {
				t += 0x5000;
			} else {
				t += 0x4e00;
			}
		} else if (t == 0x84 || t == 0x85) {
			if (t == 0x84 || !BIT_TEST(serf->type, 7)) {
				serf->type |= BIT(7);
				sfx_play_clip(SFX_WOOD_HAMMERING);
			}
			t += 0x4e80;
		} else {
			serf->type &= ~BIT(7);
			t += 0x4e80;
		}
		break;
	case SERF_TOOLMAKER:
		if (t < 0x80) {
			if (serf->state == SERF_STATE_LEAVING_BUILDING &&
			    serf->s.leaving_building.next_state == SERF_STATE_DROP_RESOURCE_OUT) {
				switch (serf->s.leaving_building.field_B-1) {
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
			if (t == 0x83 || (t == 0xb2 && !BIT_TEST(serf->type, 7))) {
				serf->type |= BIT(7);
				sfx_play_clip(SFX_SAWING);
			} else if (t == 0x87 || (t == 0xb6 && !BIT_TEST(serf->type, 7))) {
				serf->type |= BIT(7);
				sfx_play_clip(SFX_WOOD_HAMMERING);
			} else if (t != 0xb2 && t != 0xb6) {
				serf->type &= ~BIT(7);
			}
			t += 0x5880;
		}
		break;
	case SERF_WEAPONSMITH:
		if (t < 0x80) {
			if (serf->state == SERF_STATE_LEAVING_BUILDING &&
			    serf->s.leaving_building.next_state == SERF_STATE_DROP_RESOURCE_OUT) {
				if (serf->s.leaving_building.field_B == 1+RESOURCE_SWORD) {
					t += 0x5500;
				} else {
					t += 0x5400;
				}
			} else {
				t += 0x5200;
			}
		} else {
			/* edi10 += 4; */
			if (t == 0x83 || (t == 0x84 && !BIT_TEST(serf->type, 7))) {
				serf->type |= BIT(7);
				sfx_play_clip(SFX_METAL_HAMMERING);
			} else if (t != 0x84) {
				serf->type &= ~BIT(7);
			}
			t += 0x5280;
		}
		break;
	case SERF_GEOLOGIST:
		if (t < 0x80) {
			t += 0x3900;
		} else if (t == 0x83 || t == 0x84 || t == 0x86) {
			if (t == 0x83 || !BIT_TEST(serf->type, 7)) {
				serf->type |= BIT(7);
				sfx_play_clip(SFX_GEOLOGIST_SAMPLING);
			}
			t += 0x4c80;
		} else if (t == 0x8c || t == 0x8d) {
			if (t == 0x8c || !BIT_TEST(serf->type, 7)) {
				serf->type |= BIT(7);
				sfx_play_clip(SFX_RESOURCE_FOUND);
			}
			t += 0x4c80;
		} else {
			serf->type &= ~BIT(7);
			t += 0x4c80;
		}
		break;
	case SERF_KNIGHT_0:
	case SERF_KNIGHT_1:
	case SERF_KNIGHT_2:
	case SERF_KNIGHT_3:
	case SERF_KNIGHT_4:
	{
		int k = SERF_TYPE(serf) - SERF_KNIGHT_0;

		if (t < 0x80) {
			t += 0x7800 + 0x100*k;
		} else if (t < 0xc0) {
			if (serf->state == SERF_STATE_KNIGHT_ATTACKING ||
			    serf->state == SERF_STATE_KNIGHT_ATTACKING_FREE) {
				if (serf->counter >= 24 || serf->counter < 8) {
					serf->type &= ~BIT(7);
				} else if (!BIT_TEST(serf->type, 7)) {
					serf->type |= BIT(7);
					if (serf->s.attacking.field_D == 0 ||
					    serf->s.attacking.field_D == 4){
						sfx_play_clip(SFX_FIGHT_01);
					} else if (serf->s.attacking.field_D == 2) {
						/* TODO when is SFX_FIGHT_02 played? */
						sfx_play_clip(SFX_FIGHT_03);
					} else {
						sfx_play_clip(SFX_FIGHT_04);
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
		if ((!BIT_TEST(serf->type, 7) &&
		     (t == 2 || t == 5)) ||
		    (t == 1 || t == 4)) {
			serf->type |= BIT(7);
			sfx_play_clip(SFX_SERF_DYING);
		} else {
			serf->type &= ~BIT(7);
		}
		t += 0x8700;
		break;
	default:
		NOT_REACHED();
		break;
	}

	return t;
}

static void
draw_active_serf(serf_t *serf, map_pos_t pos, int x_base, int y_base,
		 uint32_t *animation_table, frame_t *frame)
{
	const int arr_4[] = {
		9, 5, 10, 7, 10, 2, 8, 6,
		11, 8, 9, 6, 9, 8, 0, 0,
		0, 0, 0, 0, 5, 5, 4, 7,
		4, 2, 7, 5, 3, 8, 5, 6,
		5, 8, 0, 0, 0, 0, 0, 0
	};

	uint8_t *tbl_ptr = ((uint8_t *)animation_table) +
		animation_table[serf->animation] +
		3*(serf->counter >> 3);

	int x = x_base + ((int8_t *)tbl_ptr)[1];
	int y = y_base + ((int8_t *)tbl_ptr)[2] - 4*MAP_HEIGHT(pos);
	int body = serf_get_body(serf, animation_table);

	if (body > -1) {
		int color = game.player[SERF_PLAYER(serf)]->color;
		draw_row_serf(x, y, 1, color, body, frame);
	}

	/* Draw additional serf */
	if (serf->state == SERF_STATE_KNIGHT_ENGAGING_BUILDING ||
	    serf->state == SERF_STATE_KNIGHT_PREPARE_ATTACKING ||
	    serf->state == SERF_STATE_KNIGHT_ATTACKING ||
	    serf->state == SERF_STATE_KNIGHT_PREPARE_ATTACKING_FREE ||
	    serf->state == SERF_STATE_KNIGHT_ATTACKING_FREE ||
	    serf->state == SERF_STATE_KNIGHT_ATTACKING_VICTORY_FREE ||
	    serf->state == SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE) {
		int index = serf->s.attacking.def_index;
		if (index != 0) {
			serf_t *def_serf = game_get_serf(index);

			uint8_t *tbl_ptr = ((uint8_t *)animation_table) +
				animation_table[def_serf->animation] +
				3*(def_serf->counter >> 3);

			int x = x_base + ((int8_t *)tbl_ptr)[1];
			int y = y_base + ((int8_t *)tbl_ptr)[2] - 4*MAP_HEIGHT(pos);
			int body = serf_get_body(def_serf, animation_table);

			if (body > -1) {
				int color = game.player[SERF_PLAYER(def_serf)]->color;
				draw_row_serf(x, y, 1, color, body, frame);
			}
		}
	}

	/* Draw extra objects for fight */
	if ((serf->state == SERF_STATE_KNIGHT_ATTACKING ||
	     serf->state == SERF_STATE_KNIGHT_ATTACKING_FREE) &&
	    tbl_ptr[0] >= 0x80 && tbl_ptr[0] < 0xc0) {
		int index = serf->s.attacking.def_index;
		if (index != 0) {
			serf_t *def_serf = game_get_serf(index);

			if (serf->animation >= 146 &&
			    serf->animation < 156) {
				if ((serf->s.attacking.field_D == 0 ||
				     serf->s.attacking.field_D == 4) &&
				    serf->counter < 32) {
					int anim = -1;
					if (serf->s.attacking.field_D == 0) {
						anim = serf->animation - 147;
					} else {
						anim = def_serf->animation - 147;
					}

					int sprite = 198 + ((serf->counter >> 3) ^ 3);
					draw_game_sprite(x + arr_4[2*anim],
							 y - arr_4[2*anim+1],
							 sprite, frame);
				}
			}
		}
	}
}

/* Draw one row of serfs. The serfs are composed of two or three transparent
   sprites (arm, torso, possibly head). A shadow is also drawn if appropriate.
   Note that idle serfs do not have their serf_t object linked from the map
   so they are drawn seperately from active serfs. */
static void
draw_serf_row(map_pos_t pos, int y_base, int cols, int x_base,
	      uint32_t *animation_table, frame_t *frame)
{
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

	for (int i = 0; i < cols; i++, x_base += MAP_TILE_WIDTH, pos = MAP_MOVE_RIGHT(pos)) {
#if 0
		/* Draw serf marker */
		if (MAP_SERF_INDEX(pos) != 0) {
			serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));
			int color = game.player[SERF_PLAYER(serf)]->color;
			gfx_fill_rect(x_base - 2, y_base - 4*MAP_HEIGHT(pos) - 2, 4, 4,
				      color, frame);
		}
#endif

		/* Active serf */
		if (MAP_SERF_INDEX(pos) != 0) {
			serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));

			if (serf->state != SERF_STATE_MINING ||
			    (serf->s.mining.substate != 3 &&
			     serf->s.mining.substate != 4 &&
			     serf->s.mining.substate != 9 &&
			     serf->s.mining.substate != 10)) {
				draw_active_serf(serf, pos, x_base, y_base,
						 animation_table, frame);
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

			int color = game.player[MAP_OWNER(pos)]->color;
			draw_row_serf(x, y, 1, color, body, frame);
		}
	}
}

/* Draw serfs that should appear behind the building at their
   current position. */
static void
draw_serf_row_behind(map_pos_t pos, int y_base, int cols, int x_base,
		     uint32_t *animation_table, frame_t *frame)
{
	for (int i = 0; i < cols; i++, x_base += MAP_TILE_WIDTH, pos = MAP_MOVE_RIGHT(pos)) {
		/* Active serf */
		if (MAP_SERF_INDEX(pos) != 0) {
			serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));

			if (serf->state == SERF_STATE_MINING &&
			    (serf->s.mining.substate == 3 ||
			     serf->s.mining.substate == 4 ||
			     serf->s.mining.substate == 9 ||
			     serf->s.mining.substate == 10)) {
				draw_active_serf(serf, pos, x_base, y_base,
						 animation_table, frame);
			}
		}
	}
}

static void
draw_game_objects(viewport_t *viewport, int layers, frame_t *frame)
{
	/*player->water_in_view = 0;
	player->trees_in_view = 0;*/

	int draw_landscape = layers & VIEWPORT_LAYER_LANDSCAPE;
	int draw_objects = layers & VIEWPORT_LAYER_OBJECTS;
	int draw_serfs = layers & VIEWPORT_LAYER_SERFS;
	if (!draw_landscape && !draw_objects && !draw_serfs) return;

	int cols = VIEWPORT_COLS(viewport);
	int short_row_len = ((cols + 1) >> 1) + 1;
	int long_row_len = ((cols + 2) >> 1) + 1;

	int x = -(viewport->offset_x + 16*(viewport->offset_y/20)) % 32;
	int y = -(viewport->offset_y) % 20;

	int col_0 = (viewport->offset_x/16 + viewport->offset_y/20)/2 & game.map.col_mask;
	int row_0 = (viewport->offset_y/MAP_TILE_HEIGHT) & game.map.row_mask;
	map_pos_t pos = MAP_POS(col_0, row_0);

	uint32_t *animation_table = viewport->interface->serf_animation_table;

	/* Loop until objects drawn fall outside the frame. */
	while (1) {
		/* short row */
		if (draw_landscape) draw_water_waves_row(pos, y, short_row_len, x, frame);
		if (draw_serfs) {
			draw_serf_row_behind(pos, y, short_row_len, x, animation_table, frame);
		}
		if (draw_objects) {
			draw_map_objects_row(viewport, pos, y, short_row_len, x, frame);
		}
		if (draw_serfs) {
			draw_serf_row(pos, y, short_row_len, x, animation_table, frame);
		}

		y += MAP_TILE_HEIGHT;
		if (y >= viewport->obj.height + 6*MAP_TILE_HEIGHT) break;

		pos = MAP_MOVE_DOWN(pos);

		/* long row */
		if (draw_landscape) draw_water_waves_row(pos, y, long_row_len, x - 16, frame);
		if (draw_serfs) {
			draw_serf_row_behind(pos, y, long_row_len, x - 16, animation_table, frame);
		}
		if (draw_objects) {
			draw_map_objects_row(viewport, pos, y, long_row_len, x - 16, frame);
		}
		if (draw_serfs) {
			draw_serf_row(pos, y, long_row_len, x - 16, animation_table, frame);
		}

		y += MAP_TILE_HEIGHT;
		if (y >= viewport->obj.height + 6*MAP_TILE_HEIGHT) break;

		pos = MAP_MOVE_DOWN_RIGHT(pos);
	}
}

static void
draw_map_cursor_sprite(viewport_t *viewport, map_pos_t pos, int sprite, frame_t *frame)
{
	int mx, my;
	viewport_map_pix_from_map_coord(viewport, pos, MAP_HEIGHT(pos), &mx, &my);

	int sx, sy;
	viewport_screen_pix_from_map_pix(viewport, mx, my, &sx, &sy);

	draw_game_sprite(sx, sy, sprite, frame);
}

static void
draw_map_cursor_possible_build(viewport_t *viewport, interface_t *interface, frame_t *frame)
{
	int x_off = -(viewport->offset_x + 16*(viewport->offset_y/20)) % 32;
	int y_off = -viewport->offset_y % 20;

	int col_0 = (viewport->offset_x/16 + viewport->offset_y/20)/2 & game.map.col_mask;
	int row_0 = (viewport->offset_y/MAP_TILE_HEIGHT) & game.map.row_mask;
	map_pos_t base_pos = MAP_POS(col_0, row_0);

	for (int x_base = x_off; x_base < viewport->obj.width + MAP_TILE_WIDTH; x_base += MAP_TILE_WIDTH) {
		map_pos_t pos = base_pos;
		int y_base = y_off;
		int row = 0;

		while (1) {
			int x;
			if (row % 2 == 0) x = x_base;
			else x = x_base - MAP_TILE_WIDTH/2;

			int y = y_base - 4*MAP_HEIGHT(pos);
			if (y >= viewport->obj.height) break;

			/* Draw possible building */
			int sprite = -1;
			if (game_can_build_castle(pos, interface->player)) {
				sprite = 50;
			} else if (game_can_player_build(pos, interface->player) &&
				   map_space_from_obj[MAP_OBJ(pos)] == MAP_SPACE_OPEN &&
				   (game_can_build_flag(MAP_MOVE_DOWN_RIGHT(pos), interface->player) ||
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
				draw_game_sprite(x, y, sprite, frame);
			}

			if (row % 2 == 0) pos = MAP_MOVE_DOWN(pos);
			else pos = MAP_MOVE_DOWN_RIGHT(pos);

			y_base += MAP_TILE_HEIGHT;
			row += 1;
		}

		base_pos = MAP_MOVE_RIGHT(base_pos);
	}
}

static void
draw_map_cursor(viewport_t *viewport, interface_t *interface, frame_t *frame)
{
	if (viewport->show_possible_build) {
		draw_map_cursor_possible_build(viewport, interface, frame);
	}

	draw_map_cursor_sprite(viewport, interface->map_cursor_pos,
			       interface->map_cursor_sprites[0].sprite, frame);

	for (dir_t d = 0; d < 6; d++) {
		draw_map_cursor_sprite(viewport, MAP_MOVE(interface->map_cursor_pos, d),
				       interface->map_cursor_sprites[1+d].sprite, frame);
	}
}

static void
draw_base_grid_overlay(viewport_t *viewport, int color, frame_t *frame)
{
	int x_base = -(viewport->offset_x + 16*(viewport->offset_y/20)) % 32;
	int y_base = -viewport->offset_y % 20;

	int row = 0;
	for (int y = y_base; y < viewport->obj.height; y += MAP_TILE_HEIGHT, row++) {
		gfx_fill_rect(0, y, viewport->obj.width, 1, color, frame);
		for (int x = x_base + ((row % 2 == 0) ? 0 : -MAP_TILE_WIDTH/2);
		     x < viewport->obj.width; x += MAP_TILE_WIDTH) {
			gfx_fill_rect(x, y + y - 2, 1, 5, color, frame);
		}
	}
}

static void
draw_height_grid_overlay(viewport_t *viewport, int color, frame_t *frame)
{
	int x_off = -(viewport->offset_x + 16*(viewport->offset_y/20)) % 32;
	int y_off = -viewport->offset_y % 20;

	int col_0 = (viewport->offset_x/16 + viewport->offset_y/20)/2 & game.map.col_mask;
	int row_0 = (viewport->offset_y/MAP_TILE_HEIGHT) & game.map.row_mask;
	map_pos_t base_pos = MAP_POS(col_0, row_0);

	for (int x_base = x_off; x_base < viewport->obj.width + MAP_TILE_WIDTH; x_base += MAP_TILE_WIDTH) {
		map_pos_t pos = base_pos;
		int y_base = y_off;
		int row = 0;

		while (1) {
			int x;
			if (row % 2 == 0) x = x_base;
			else x = x_base - MAP_TILE_WIDTH/2;

			int y = y_base - 4*MAP_HEIGHT(pos);
			if (y >= viewport->obj.height) break;

			/* Draw cross. */
			if (pos != MAP_POS(0, 0)) {
				gfx_fill_rect(x, y - 1, 1, 3, color, frame);
				gfx_fill_rect(x - 1, y, 3, 1, color, frame);
			} else {
				gfx_fill_rect(x, y - 2, 1, 5, color, frame);
				gfx_fill_rect(x - 2, y, 5, 1, color, frame);
			}

			if (row % 2 == 0) pos = MAP_MOVE_DOWN(pos);
			else pos = MAP_MOVE_DOWN_RIGHT(pos);

			y_base += MAP_TILE_HEIGHT;
			row += 1;
		}

		base_pos = MAP_MOVE_RIGHT(base_pos);
	}
}

static void
viewport_draw(viewport_t *viewport, frame_t *frame)
{
	viewport_layer_t layers = viewport->layers;
	if (layers & VIEWPORT_LAYER_LANDSCAPE) draw_landscape(viewport, frame);
	if (layers & VIEWPORT_LAYER_GRID) {
		draw_base_grid_overlay(viewport, 72, frame);
		draw_height_grid_overlay(viewport, 76, frame);
	}
	if (layers & VIEWPORT_LAYER_PATHS) draw_paths_and_borders(viewport, frame);
	draw_game_objects(viewport, layers, frame);
	if (layers & VIEWPORT_LAYER_CURSOR) draw_map_cursor(viewport, viewport->interface, frame);
}

static int
viewport_handle_event_click(viewport_t *viewport, int x, int y, gui_event_button_t button)
{
	if (button != GUI_EVENT_BUTTON_LEFT) return 0;

	gui_object_set_redraw((gui_object_t *)viewport);

	interface_t *interface = viewport->interface;

	map_pos_t clk_pos = viewport_map_pos_from_screen_pix(viewport, x, y);
	int clk_col = MAP_POS_COL(clk_pos);
	int clk_row = MAP_POS_ROW(clk_pos);

	if (interface->building_road) {
		int x = (clk_col - MAP_POS_COL(interface->map_cursor_pos) + 1) & game.map.col_mask;
		int y = (clk_row - MAP_POS_ROW(interface->map_cursor_pos) + 1) & game.map.row_mask;
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

		if (BIT_TEST(interface->building_road_valid_dir, dir)) {
			int length = interface->building_road_length;
			dir_t last_dir = 0;
			if (length > 0) last_dir = interface->building_road_dirs[length-1];

			if (length > 0 && DIR_REVERSE(last_dir) == dir) {
				/* Delete existing path */
				int r = interface_remove_road_segment(interface);
				if (r < 0) {
					sfx_play_clip(SFX_NOT_ACCEPTED);
				} else {
					sfx_play_clip(SFX_CLICK);
				}
			} else {
				/* Build new road segment */
				int r = interface_build_road_segment(interface, dir);
				if (r < 0) {
					sfx_play_clip(SFX_NOT_ACCEPTED);
				} else if (r == 0) {
					sfx_play_clip(SFX_CLICK);
				} else {
					sfx_play_clip(SFX_ACCEPTED);
				}
			}
		}
	} else {
		interface_update_map_cursor_pos(viewport->interface, clk_pos);
		sfx_play_clip(SFX_CLICK);
	}

	return 0;
}

int
viewport_handle_event_dbl_click(viewport_t *viewport, int x, int y,
				gui_event_button_t button)
{
	if (button != GUI_EVENT_BUTTON_LEFT) return 0;

	gui_object_set_redraw((gui_object_t *)viewport);

	interface_t *interface = viewport->interface;

	map_pos_t clk_pos = viewport_map_pos_from_screen_pix(viewport, x, y);

	if (interface->building_road) {
		if (clk_pos != interface->map_cursor_pos) {
			map_pos_t pos = interface->building_road_source;
			uint length;
			dir_t *dirs = pathfinder_map(pos, clk_pos, &length);
			if (dirs != NULL) {
				interface->building_road_length = 0;
				int r = interface_extend_road(interface, dirs, length);
				if (r < 0) sfx_play_clip(SFX_NOT_ACCEPTED);
				else if (r == 1) sfx_play_clip(SFX_ACCEPTED);
				else sfx_play_clip(SFX_CLICK);
				free(dirs);
			} else {
				sfx_play_clip(SFX_NOT_ACCEPTED);
			}
		} else {
			int r = game_build_flag(interface->map_cursor_pos,
						interface->player);
			if (r < 0) {
				sfx_play_clip(SFX_NOT_ACCEPTED);
			} else {
				r = game_build_road(interface->building_road_source,
						    interface->building_road_dirs,
						    interface->building_road_length,
						    interface->player);
				if (r < 0) {
					sfx_play_clip(SFX_NOT_ACCEPTED);
					game_demolish_flag(interface->map_cursor_pos,
							   interface->player);
				} else {
					sfx_play_clip(SFX_ACCEPTED);
					interface_build_road_end(interface);
				}
			}
		}
	} else {
		if (MAP_OBJ(clk_pos) == MAP_OBJ_NONE ||
		    MAP_OBJ(clk_pos) > MAP_OBJ_CASTLE) {
			return 0;
		}

		if (MAP_OBJ(clk_pos) == MAP_OBJ_FLAG) {
			if (BIT_TEST(game.split, 5) || /* Demo mode */
			    MAP_OWNER(clk_pos) == interface->player->player_num) {
				interface_open_popup(interface, BOX_TRANSPORT_INFO);
			}

			interface->player->index = MAP_OBJ_INDEX(clk_pos);
		} else { /* Building */
			if (BIT_TEST(game.split, 5) || /* Demo mode */
			    MAP_OWNER(clk_pos) == interface->player->player_num) {
				building_t *building = game_get_building(MAP_OBJ_INDEX(clk_pos));
				if (!BUILDING_IS_DONE(building)) {
					interface_open_popup(interface, BOX_ORDERED_BLD);
					} else if (BUILDING_TYPE(building) == BUILDING_CASTLE) {
					interface_open_popup(interface, BOX_CASTLE_RES);
				} else if (BUILDING_TYPE(building) == BUILDING_STOCK) {
					if (!BUILDING_IS_ACTIVE(building)) return 0;
					interface_open_popup(interface, BOX_CASTLE_RES);
				} else if (BUILDING_TYPE(building) == BUILDING_HUT ||
					   BUILDING_TYPE(building) == BUILDING_TOWER ||
					   BUILDING_TYPE(building) == BUILDING_FORTRESS) {
					interface_open_popup(interface, BOX_DEFENDERS);
				} else if (BUILDING_TYPE(building) == BUILDING_STONEMINE ||
					   BUILDING_TYPE(building) == BUILDING_COALMINE ||
					   BUILDING_TYPE(building) == BUILDING_IRONMINE ||
					   BUILDING_TYPE(building) == BUILDING_GOLDMINE) {
					interface_open_popup(interface, BOX_MINE_OUTPUT);
				} else {
					interface_open_popup(interface, BOX_BLD_STOCK);
				}

				interface->player->index = MAP_OBJ_INDEX(clk_pos);
			} else if (BIT_TEST(game.split, 5)) { /* Demo mode*/
				return 0;
			} else { /* Foreign building */
				/* TODO handle coop mode*/
				building_t *building = game_get_building(MAP_OBJ_INDEX(clk_pos));
				interface->player->building_attacked = BUILDING_INDEX(building);

				if (BUILDING_IS_DONE(building) &&
				    (BUILDING_TYPE(building) == BUILDING_HUT ||
				     BUILDING_TYPE(building) == BUILDING_TOWER ||
				     BUILDING_TYPE(building) == BUILDING_FORTRESS ||
				     BUILDING_TYPE(building) == BUILDING_CASTLE)) {
					if (!BUILDING_IS_ACTIVE(building) ||
					    BUILDING_STATE(building) != 3) {
						/* It is not allowed to attack
						   if currently not occupied or
						   is too far from the border. */
						sfx_play_clip(SFX_NOT_ACCEPTED);
						return 0;
					}

					const map_pos_t *p = &game.spiral_pos_pattern[7];
					int found = 0;
					for (int i = 257; i >= 0; i--) {
						map_pos_t pos = MAP_POS_ADD(building->pos, p[257-i]);
						if (MAP_HAS_OWNER(pos) &&
						    MAP_OWNER(pos) == interface->player->player_num) {
							found = 1;
							break;
						}
					}

					if (!found) {
						sfx_play_clip(SFX_NOT_ACCEPTED);
						return 0;
					}

					/* Action accepted */
					sfx_play_clip(SFX_CLICK);

					int max_knights = 0;
					switch (BUILDING_TYPE(building)) {
					case BUILDING_HUT: max_knights = 3; break;
					case BUILDING_TOWER: max_knights = 6; break;
					case BUILDING_FORTRESS: max_knights = 12; break;
					case BUILDING_CASTLE: max_knights = 20; break;
					default: NOT_REACHED(); break;
					}

					int knights = player_knights_available_for_attack(interface->player,
											  building->pos);
					interface->player->knights_attacking = min(knights, max_knights);
					interface_open_popup(interface, BOX_START_ATTACK);
				}
			}
		}

		interface->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
		interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
		interface->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
		interface->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
		interface->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
	}

	return 0;
}

static int
viewport_handle_drag(viewport_t *viewport, int x, int y,
		     gui_event_button_t button)
{
	if (button == GUI_EVENT_BUTTON_RIGHT ||
	    button == GUI_EVENT_BUTTON_LEFT) {
		if (x != 0 || y != 0) {
			viewport_move_by_pixels(viewport, x, y);
		}
	}

	return 0;
}

static int
viewport_handle_event(viewport_t *viewport, const gui_event_t *event)
{
	int x = event->x;
	int y = event->y;

	switch (event->type) {
	case GUI_EVENT_TYPE_CLICK:
		return viewport_handle_event_click(viewport, x, y,
						   event->button);
	case GUI_EVENT_TYPE_DBL_CLICK:
		return viewport_handle_event_dbl_click(viewport, x, y,
						       event->button);
		break;
	case GUI_EVENT_TYPE_DRAG_START:
		viewport->interface->cursor_lock_target = (gui_object_t *)viewport;
		return 0;
	case GUI_EVENT_TYPE_DRAG_MOVE:
		return viewport_handle_drag(viewport, x, y,
					    event->button);
	case GUI_EVENT_TYPE_DRAG_END:
		viewport->interface->cursor_lock_target = NULL;
		return 0;
	default:
		break;
	}

	return 0;
}

void
viewport_init(viewport_t *viewport, interface_t *interface)
{
	gui_object_init((gui_object_t *)viewport);
	viewport->obj.draw = (gui_draw_func *)viewport_draw;
	viewport->obj.handle_event = (gui_handle_event_func *)viewport_handle_event;

	viewport->interface = interface;
	viewport->layers = VIEWPORT_LAYER_ALL;

	viewport->last_tick = 0;
	viewport->show_possible_build = 0;
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
viewport_screen_pix_from_map_pix(viewport_t *viewport, int mx, int my, int *sx, int *sy)
{
	int width = game.map.cols*MAP_TILE_WIDTH;
	int height = game.map.rows*MAP_TILE_HEIGHT;

	*sx = mx - viewport->offset_x;
	*sy = my - viewport->offset_y;

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
viewport_map_pix_from_map_coord(viewport_t *viewport, map_pos_t pos, int h, int *mx, int *my)
{
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
viewport_map_pos_from_screen_pix(viewport_t *viewport, int sx, int sy)
{
	int x_off = -(viewport->offset_x + 16*(viewport->offset_y/20)) % 32;
	int y_off = -viewport->offset_y % 20;

	int col = (viewport->offset_x/16 + viewport->offset_y/20)/2 & game.map.col_mask;
	int row = (viewport->offset_y/MAP_TILE_HEIGHT) & game.map.row_mask;

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
viewport_get_current_map_pos(viewport_t *viewport)
{
	return viewport_map_pos_from_screen_pix(viewport,
						viewport->obj.width/2,
						viewport->obj.height/2);
}

void
viewport_move_to_map_pos(viewport_t *viewport, map_pos_t pos)
{
	int mx, my;
	viewport_map_pix_from_map_coord(viewport, pos,
					MAP_HEIGHT(pos),
					&mx, &my);

	int map_width = game.map.cols*MAP_TILE_WIDTH;
	int map_height = game.map.rows*MAP_TILE_HEIGHT;

	/* Center screen. */
	mx -= viewport->obj.width/2;
	my -= viewport->obj.height/2;

	if (my < 0) {
		mx -= (game.map.rows*MAP_TILE_WIDTH)/2;
		my += map_height;
	}

	if (mx < 0) mx += map_width;
	else if (mx >= map_width) mx -= map_width;

	viewport->offset_x = mx;
	viewport->offset_y = my;

	gui_object_set_redraw((gui_object_t *)viewport);
}

void
viewport_move_by_pixels(viewport_t *viewport, int x, int y)
{
	int width = game.map.cols*MAP_TILE_WIDTH;
	int height = game.map.rows*MAP_TILE_HEIGHT;

	viewport->offset_x += x;
	viewport->offset_y += y;

	if (viewport->offset_y < 0) {
		viewport->offset_y += height;
		viewport->offset_x -= (game.map.rows*MAP_TILE_WIDTH)/2;
	} else if (viewport->offset_y >= height) {
		viewport->offset_y -= height;
		viewport->offset_x += (game.map.rows*MAP_TILE_WIDTH)/2;
	}

	if (viewport->offset_x >= width) viewport->offset_x -= width;
	else if (viewport->offset_x < 0) viewport->offset_x += width;

	gui_object_set_redraw((gui_object_t *)viewport);
}


/* Called periodically when the game progresses. */
void
viewport_update(viewport_t *viewport)
{
	int tick_xor = game.tick ^ viewport->last_tick;
	viewport->last_tick = game.tick;

	/* Viewport animation does not care about low bits in anim */
	if (tick_xor >= 1 << 3) gui_object_set_redraw((gui_object_t *)viewport);
}
