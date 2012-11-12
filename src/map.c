/* map.c */

/* Map generator and map update functions.
   Basically the map is constructed from a regular, square grid, with
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
   defined by these points in (col, row, height)-space. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "map.h"
#include "viewport.h"
#include "random.h"
#include "globals.h"
#include "misc.h"
#include "debug.h"


/* Map map_obj_t to map_space_t. */
const map_space_t map_space_from_obj[] = {
	[MAP_OBJ_FLAG] = MAP_SPACE_FLAG,
	[MAP_OBJ_SMALL_BUILDING] = MAP_SPACE_SMALL_BUILDING,
	[MAP_OBJ_LARGE_BUILDING] = MAP_SPACE_LARGE_BUILDING,
	[MAP_OBJ_CASTLE] = MAP_SPACE_CASTLE,

	[MAP_OBJ_TREE_0] = MAP_SPACE_FILLED,
	[MAP_OBJ_TREE_1] = MAP_SPACE_FILLED,
	[MAP_OBJ_TREE_2] = MAP_SPACE_FILLED,
	[MAP_OBJ_TREE_3] = MAP_SPACE_FILLED,
	[MAP_OBJ_TREE_4] = MAP_SPACE_FILLED,
	[MAP_OBJ_TREE_5] = MAP_SPACE_FILLED,
	[MAP_OBJ_TREE_6] = MAP_SPACE_FILLED,
	[MAP_OBJ_TREE_7] = MAP_SPACE_FILLED,

	[MAP_OBJ_PINE_0] = MAP_SPACE_FILLED,
	[MAP_OBJ_PINE_1] = MAP_SPACE_FILLED,
	[MAP_OBJ_PINE_2] = MAP_SPACE_FILLED,
	[MAP_OBJ_PINE_3] = MAP_SPACE_FILLED,
	[MAP_OBJ_PINE_4] = MAP_SPACE_FILLED,
	[MAP_OBJ_PINE_5] = MAP_SPACE_FILLED,
	[MAP_OBJ_PINE_6] = MAP_SPACE_FILLED,
	[MAP_OBJ_PINE_7] = MAP_SPACE_FILLED,

	[MAP_OBJ_PALM_0] = MAP_SPACE_FILLED,
	[MAP_OBJ_PALM_1] = MAP_SPACE_FILLED,
	[MAP_OBJ_PALM_2] = MAP_SPACE_FILLED,
	[MAP_OBJ_PALM_3] = MAP_SPACE_FILLED,

	[MAP_OBJ_WATER_TREE_0] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_WATER_TREE_1] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_WATER_TREE_2] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_WATER_TREE_3] = MAP_SPACE_IMPASSABLE,

	[MAP_OBJ_STONE_0] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_STONE_1] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_STONE_2] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_STONE_3] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_STONE_4] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_STONE_5] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_STONE_6] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_STONE_7] = MAP_SPACE_IMPASSABLE,

	[MAP_OBJ_SANDSTONE_0] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_SANDSTONE_1] = MAP_SPACE_IMPASSABLE,

	[MAP_OBJ_CROSS] = MAP_SPACE_FILLED,

	[MAP_OBJ_WATER_STONE_0] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_WATER_STONE_1] = MAP_SPACE_IMPASSABLE,

	[MAP_OBJ_CACTUS_0] = MAP_SPACE_FILLED,
	[MAP_OBJ_CACTUS_1] = MAP_SPACE_FILLED,

	[MAP_OBJ_DEAD_TREE] = MAP_SPACE_FILLED,

	[MAP_OBJ_FELLED_PINE_0] = MAP_SPACE_FILLED,
	[MAP_OBJ_FELLED_PINE_1] = MAP_SPACE_FILLED,
	[MAP_OBJ_FELLED_PINE_2] = MAP_SPACE_FILLED,
	[MAP_OBJ_FELLED_PINE_3] = MAP_SPACE_FILLED,
	[MAP_OBJ_FELLED_PINE_4] = MAP_SPACE_OPEN,

	[MAP_OBJ_FELLED_TREE_0] = MAP_SPACE_FILLED,
	[MAP_OBJ_FELLED_TREE_1] = MAP_SPACE_FILLED,
	[MAP_OBJ_FELLED_TREE_2] = MAP_SPACE_FILLED,
	[MAP_OBJ_FELLED_TREE_3] = MAP_SPACE_FILLED,
	[MAP_OBJ_FELLED_TREE_4] = MAP_SPACE_OPEN,

	[MAP_OBJ_NEW_PINE] = MAP_SPACE_FILLED,
	[MAP_OBJ_NEW_TREE] = MAP_SPACE_FILLED,

	[MAP_OBJ_SEEDS_0] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_SEEDS_1] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_SEEDS_2] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_SEEDS_3] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_SEEDS_4] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_SEEDS_5] = MAP_SPACE_IMPASSABLE,

	[MAP_OBJ_FIELD_0] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_FIELD_1] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_FIELD_2] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_FIELD_3] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_FIELD_4] = MAP_SPACE_IMPASSABLE,
	[MAP_OBJ_FIELD_5] = MAP_SPACE_IMPASSABLE
};


/* Return a random map position.
   Returned as map_pos_t and also as col and row if not NULL. */
static map_pos_t
get_rnd_map_coord(int *col, int *row)
{
	int c = random_int() & globals.map_col_mask;
	int r = random_int() & globals.map_row_mask;

	if (col != NULL) *col = c;
	if (row != NULL) *row = r;
	return MAP_POS(c, r);
}

/* Midpoint displacement map generator. This function initialises the height
   values in the corners of 16x16 squares. */
static void
init_map_heights_squares()
{
	map_1_t *map = globals.map_mem2_ptr;

	for (int y = 0; y < globals.map_rows; y += 16) {
		for (int x = 0; x < globals.map_cols; x += 16) {
			int rnd = random_int() & 0xff;
			map[MAP_POS(x,y)].height = min(rnd, 250);
		}
	}
}

static int
calc_height_displacement(int avg, int base, int offset)
{
	int r = random_int();
	int h = ((r * base) >> 16) - offset + avg;

	return max(0,min(h,250));
}

#define TERRAIN_SPIKYNESS  0x9999

/* Calculate height values of the subdivisions in the
   midpoint displacement algorithm. */
static void
init_map_heights_midpoints()
{
	map_1_t *map = globals.map_mem2_ptr;

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
		for (int y = 0; y < globals.map_rows; y += 2*i) {
			for (int x = 0; x < globals.map_cols; x += 2*i) {
				map_pos_t pos = MAP_POS(x, y);
				int h = map[pos].height;

				map_pos_t pos_r = MAP_MOVE_RIGHT_N(pos, 2*i);
				map_pos_t pos_mid_r = MAP_MOVE_RIGHT_N(pos, i);
				int h_r = map[pos_r].height;

				if (globals.map_preserve_bugs) {
					/* The intention was probably just to set h_r to the map height value,
					   but the upper bits of rnd must be preserved in h_r in the first
					   iteration to generate the same maps as the original game. */
					if (x == 0 && y == 0 && i == 8) h_r |= rnd & 0xff00;
				}

				map[pos_mid_r].height = calc_height_displacement((h + h_r)/2, r1, r2);

				map_pos_t pos_d = MAP_MOVE_DOWN_N(pos, 2*i);
				map_pos_t pos_mid_d = MAP_MOVE_DOWN_N(pos, i);
				int h_d = map[pos_d].height;
				map[pos_mid_d].height = calc_height_displacement((h+h_d)/2, r1, r2);

				map_pos_t pos_dr = MAP_MOVE_RIGHT_N(MAP_MOVE_DOWN_N(pos, 2*i), 2*i);
				map_pos_t pos_mid_dr = MAP_MOVE_RIGHT_N(MAP_MOVE_DOWN_N(pos, i), i);
				int h_dr = map[pos_dr].height;
				map[pos_mid_dr].height = calc_height_displacement((h+h_dr)/2, r1, r2);
			}
		}

		r1 >>= 1;
		r2 >>= 1;
	}
}

static void
init_map_heights_diamond_square()
{
	map_1_t *map = globals.map_mem2_ptr;

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
		for (int y = 0; y < globals.map_rows; y += 2*i) {
			for (int x = 0; x < globals.map_cols; x += 2*i) {
				map_pos_t pos = MAP_POS(x, y);
				int h = map[pos].height;

				map_pos_t pos_r = MAP_MOVE_RIGHT_N(pos, 2*i);
				int h_r = map[pos_r].height;

				map_pos_t pos_d = MAP_MOVE_DOWN_N(pos, 2*i);
				int h_d = map[pos_d].height;

				map_pos_t pos_dr = MAP_MOVE_RIGHT_N(MAP_MOVE_DOWN_N(pos, 2*i), 2*i);
				int h_dr = map[pos_dr].height;

				map_pos_t pos_mid_dr = MAP_MOVE_RIGHT_N(MAP_MOVE_DOWN_N(pos, i), i);
				int avg = (h + h_r + h_d + h_dr) / 4;
				map[pos_mid_dr].height = calc_height_displacement(avg, r1, r2);
			}
		}

		/* Square step */
		for (int y = 0; y < globals.map_rows; y += 2*i) {
			for (int x = 0; x < globals.map_cols; x += 2*i) {
				map_pos_t pos = MAP_POS(x, y);
				int h = map[pos].height;

				map_pos_t pos_r = MAP_MOVE_RIGHT_N(pos, 2*i);
				int h_r = map[pos_r].height;

				map_pos_t pos_d = MAP_MOVE_DOWN_N(pos, 2*i);
				int h_d = map[pos_d].height;

				map_pos_t pos_ur = MAP_MOVE_RIGHT_N(MAP_MOVE_DOWN_N(pos, -i), i);
				int h_ur = map[pos_ur].height;

				map_pos_t pos_dr = MAP_MOVE_RIGHT_N(MAP_MOVE_DOWN_N(pos, i), i);
				int h_dr = map[pos_dr].height;

				map_pos_t pos_dl = MAP_MOVE_RIGHT_N(MAP_MOVE_DOWN_N(pos, i), -i);
				int h_dl = map[pos_dl].height;

				map_pos_t pos_mid_r = MAP_MOVE_RIGHT_N(pos, i);
				int avg_r = (h + h_r + h_ur + h_dr) / 4;
				map[pos_mid_r].height = calc_height_displacement(avg_r, r1, r2);

				map_pos_t pos_mid_d = MAP_MOVE_DOWN_N(pos, i);
				int avg_d = (h + h_d + h_dl + h_dr) / 4;
				map[pos_mid_d].height = calc_height_displacement(avg_d, r1, r2);
			}
		}

		r1 >>= 1;
		r2 >>= 1;
	}
}

static int
adjust_map_height(int h1, int h2, map_1_t *map)
{
	if (abs(h1 - h2) > 32) {
		map->height = h1 + ((h1 < h2) ? 32 : -32);
		return 1;
	}

	return 0;
}

/* Ensure that map heights of adjacent fields are not too far apart. */
static void
clamp_map_heights()
{
	map_1_t *map = globals.map_mem2_ptr;

	int changed = 1;
	while (changed) {
		changed = 0;
		for (int y = 0; y < globals.map_rows; y++) {
			for (int x = 0; x < globals.map_cols; x++) {
				map_pos_t pos = MAP_POS(x, y);
				int h = map[pos].height;

				map_pos_t pos_d = MAP_MOVE_DOWN(pos);
				int h_d = map[pos_d].height;
				changed |= adjust_map_height(h, h_d, &map[pos_d]);

				map_pos_t pos_dr = MAP_MOVE_DOWN_RIGHT(pos);
				int h_dr = map[pos_dr].height;
				changed |= adjust_map_height(h, h_dr, &map[pos_dr]);

				map_pos_t pos_r = MAP_MOVE_RIGHT(pos);
				int h_r = map[pos_r].height;
				changed |= adjust_map_height(h, h_r, &map[pos_r]);
			}
		}
	}
}

static int
map_expand_level_area(map_1_t *map, map_pos_t pos, int limit, int r)
{
	int flag = 0;

	for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
		map_pos_t new_pos = MAP_MOVE(pos, d);
		if (map[new_pos].height < 254) {
			if (map[new_pos].height > limit) return r;
		} else if (map[new_pos].height == 255) {
			flag = 1;
		}
	}

	if (flag) {
		map[pos].height = 255;

		for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
			map_pos_t new_pos = MAP_MOVE(pos, d);
			if (map[new_pos].height != 255) map[new_pos].height = 254;
		}

		return 1;
	}

	return r;
}

static void
map_init_level_area(map_1_t *map, map_pos_t pos)
{
	int limit = globals.map_water_level;

	if (limit >= map[MAP_MOVE_RIGHT(pos)].height &&
	    limit >= map[MAP_MOVE_DOWN_RIGHT(pos)].height &&
	    limit >= map[MAP_MOVE_DOWN(pos)].height &&
	    limit >= map[MAP_MOVE_LEFT(pos)].height &&
	    limit >= map[MAP_MOVE_UP_LEFT(pos)].height &&
	    limit >= map[MAP_MOVE_UP(pos)].height) {
		map[pos].height = 255;
		map[MAP_MOVE_RIGHT(pos)].height = 254;
		map[MAP_MOVE_DOWN_RIGHT(pos)].height = 254;
		map[MAP_MOVE_DOWN(pos)].height = 254;
		map[MAP_MOVE_LEFT(pos)].height = 254;
		map[MAP_MOVE_UP_LEFT(pos)].height = 254;
		map[MAP_MOVE_UP(pos)].height = 254;

		for (int i = 0; i < globals.map_max_lake_area; i++) {
			int flag = 0;

			map_pos_t new_pos = MAP_MOVE_RIGHT_N(pos, i+1);
			for (int k = 0; k < 6; k++) {
				dir_t d = (k + DIR_DOWN) % 6;
				for (int j = 0; j <= i; j++) {
					flag = map_expand_level_area(map, new_pos, limit, flag);
					new_pos = MAP_MOVE(new_pos, d);
				}
			}

			if (!flag) break;
		}

		if (map[pos].height > 253) map[pos].height -= 2;

		for (int i = 0; i < globals.map_max_lake_area + 1; i++) {
			map_pos_t new_pos = MAP_MOVE_RIGHT_N(pos, i+1);
			for (int k = 0; k < 6; k++) {
				dir_t d = (k + DIR_DOWN) % 6;
				for (int j = 0; j <= i; j++) {
					if (map[new_pos].height > 253) map[new_pos].height -= 2;
					new_pos = MAP_MOVE(new_pos, d);
				}
			}
		}
	} else {
		map[pos].height = 0;
	}
}

/* Create level land that will later be filled with water.
   It is created in areas that are below a certain threshold.
   The areas are also limited in size. */
static void
map_init_sea_level()
{
	map_1_t *map = globals.map_mem2_ptr;
	map_2_t *map_data = MAP_2_DATA(map);

	if (globals.map_water_level < 0) return;

	for (int h = 0; h <= globals.map_water_level; h++) {
		for (int y = 0; y < globals.map_rows; y++) {
			for (int x = 0; x < globals.map_cols; x++) {
				map_pos_t pos = MAP_POS(x, y);
				if (map[pos].height == h) {
					map_init_level_area(map, pos);
				}
			}
		}
	}

	/* Map positions are marked in the previous loop.
	   0: Above water level.
	   252: Land at water level.
	   253: Water. */

	for (int y = 0; y < globals.map_rows; y++) {
		for (int x = 0; x < globals.map_cols; x++) {
			map_pos_t pos = MAP_POS(x, y);
			int h = map[pos].height;
			switch (h) {
				case 0:
					map[pos].height = globals.map_water_level + 1;
					break;
				case 252:
					map[pos].height = globals.map_water_level;
					break;
				case 253:
					map[pos].height = globals.map_water_level - 1;
					map[pos].flags |= BIT(6);
					map_data[pos].u.s.resource = random_int() & 7; /* Fish (?) */
					break;
			}
		}
	}
}

/* Adjust heights so zero height is sea level. */
static void
map_heights_rebase()
{
	map_1_t *map = globals.map_mem2_ptr;
	int h = globals.map_water_level - 1;

	for (int y = 0; y < globals.map_rows; y++) {
		for (int x = 0; x < globals.map_cols; x++) {
			map[MAP_POS(x, y)].height -= h;
		}
	}
}

static int
calc_map_type(int h_sum)
{
	if (h_sum < 3) return 0;
	else if (h_sum < 384) return 5;
	else if (h_sum < 416) return 6;
	else if (h_sum < 448) return 11;
	else if (h_sum < 480) return 12;
	else if (h_sum < 528) return 13;
	else if (h_sum < 560) return 14;
	return 15;
}

/* Set type of map fields based on the height value. */
static void
init_map_types()
{
	map_1_t *map = globals.map_mem2_ptr;

	for (int y = 0; y < globals.map_rows; y++) {
		for (int x = 0; x < globals.map_cols; x++) {
			map_pos_t pos = MAP_POS(x, y);
			int h1 = map[pos].height;
			int h2 = map[MAP_MOVE_RIGHT(pos)].height;
			int h3 = map[MAP_MOVE_DOWN_RIGHT(pos)].height;
			int h4 = map[MAP_MOVE_DOWN(pos)].height;
			map[pos].type = (calc_map_type(h1 + h3 + h4) << 4) | calc_map_type(h1 + h2 + h3);
		}
	}
}

static void
init_map_types_2_sub()
{
	map_1_t *map = globals.map_mem2_ptr;

	for (int y = 0; y < globals.map_rows; y++) {
		for (int x = 0; x < globals.map_cols; x++) {
			map[MAP_POS(x, y)].obj = 0;
		}
	}
}

static void
init_map_types_2()
{
	init_map_types_2_sub();

	map_1_t *map = globals.map_mem2_ptr;

	for (int y = 0; y < globals.map_rows; y++) {
		for (int x = 0; x < globals.map_cols; x++) {
			map_pos_t pos = MAP_POS(x, y);

			if (map[pos].height > 0) {
				map[pos].obj = 1;

				int num = 0;
				int changed = 1;
				while (changed) {
					changed = 0;
					for (int y = 0; y < globals.map_rows; y++) {
						for (int x = 0; x < globals.map_cols; x++) {
							map_pos_t pos = MAP_POS(x, y);

							if (map[pos].obj == 1) {
								num += 1;
								map[pos].obj = 2;

								int flags = 0;
								if (map[pos].type & 0xc) flags |= 3;
								if (map[pos].type & 0xc0) flags |= 6;
								if (map[MAP_MOVE_LEFT(pos)].type & 0xc) flags |= 0xc;
								if (map[MAP_MOVE_UP_LEFT(pos)].type & 0xc0) flags |= 0x18;
								if (map[MAP_MOVE_UP_LEFT(pos)].type & 0xc) flags |= 0x30;
								if (map[MAP_MOVE_UP(pos)].type & 0xc0) flags |= 0x21;

								for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
									if (BIT_TEST(flags, d)) {
										if (map[MAP_MOVE(pos, d)].obj == 0) {
											map[MAP_MOVE(pos, d)].obj = 1;
											changed = 1;
										}
									}
								}
							}
						}
					}
				}

				if (4*num >= globals.map_elms) goto break_loop;
			}
		}
	}

	break_loop:

	for (int y = 0; y < globals.map_rows; y++) {
		for (int x = 0; x < globals.map_cols; x++) {
			map_pos_t pos = MAP_POS(x, y);

			if (map[pos].height > 0 && map[pos].obj == 0) {
				map[pos].height = 0;
				map[pos].type = 0;

				map[MAP_MOVE_LEFT(pos)].type &= 0xf0;
				map[MAP_MOVE_UP_LEFT(pos)].type = 0;
				map[MAP_MOVE_UP(pos)].type &= 0xf;
			}
		}
	}

	init_map_types_2_sub();
}

/* Rescale height values to be in [0;31]. */
static void
map_heights_rescale()
{
	map_1_t *map = globals.map_mem2_ptr;

	for (int y = 0; y < globals.map_rows; y++) {
		for (int x = 0; x < globals.map_cols; x++) {
			map_pos_t pos = MAP_POS(x, y);
			map[pos].height = (map[pos].height + 6) >> 3;
		}
	}
}

static void
init_map_types_shared_sub(int old, int seed, int new)
{
	map_1_t *map = globals.map_mem2_ptr;

	for (int y = 0; y < globals.map_rows; y++) {
		for (int x = 0; x < globals.map_cols; x++) {
			map_pos_t pos = MAP_POS(x, y);

			if (MAP_TYPE_UP(pos) == old &&
			    (seed == MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(pos)) ||
			     seed == MAP_TYPE_UP(MAP_MOVE_UP_LEFT(pos)) ||
			     seed == MAP_TYPE_UP(MAP_MOVE_UP(pos)) ||
			     seed == MAP_TYPE_DOWN(MAP_MOVE_LEFT(pos)) ||
			     seed == MAP_TYPE_UP(MAP_MOVE_LEFT(pos)) ||
			     seed == MAP_TYPE_DOWN(pos) ||
			     seed == MAP_TYPE_UP(MAP_MOVE_RIGHT(pos)) ||
			     seed == MAP_TYPE_DOWN(MAP_MOVE_DOWN_LEFT(pos)) ||
			     seed == MAP_TYPE_DOWN(MAP_MOVE_DOWN(pos)) ||
			     seed == MAP_TYPE_UP(MAP_MOVE_DOWN(pos)) ||
			     seed == MAP_TYPE_DOWN(MAP_MOVE_DOWN_RIGHT(pos)) ||
			     seed == MAP_TYPE_UP(MAP_MOVE_DOWN_RIGHT(pos)))) {
				map[pos].type = (new << 4) | (map[pos].type & 0xf);
			}

			if (MAP_TYPE_DOWN(pos) == old &&
			    (seed == MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(pos)) ||
			     seed == MAP_TYPE_UP(MAP_MOVE_UP_LEFT(pos)) ||
			     seed == MAP_TYPE_DOWN(MAP_MOVE_UP(pos)) ||
			     seed == MAP_TYPE_UP(MAP_MOVE_UP(pos)) ||
			     seed == MAP_TYPE_UP(MAP_MOVE_UP_RIGHT(pos)) ||
			     seed == MAP_TYPE_DOWN(MAP_MOVE_LEFT(pos)) ||
			     seed == MAP_TYPE_UP(pos) ||
			     seed == MAP_TYPE_DOWN(MAP_MOVE_RIGHT(pos)) ||
			     seed == MAP_TYPE_UP(MAP_MOVE_RIGHT(pos)) ||
			     seed == MAP_TYPE_DOWN(MAP_MOVE_DOWN(pos)) ||
			     seed == MAP_TYPE_DOWN(MAP_MOVE_DOWN_RIGHT(pos)) ||
			     seed == MAP_TYPE_UP(MAP_MOVE_DOWN_RIGHT(pos)))) {
				map[pos].type = (map[pos].type & 0xf0) | new;
			}
		}
	}
}

static void
init_map_lakes()
{
	init_map_types_shared_sub(0, 5, 3);
	init_map_types_shared_sub(0, 3, 2);
	init_map_types_shared_sub(0, 2, 1);
}

static void
init_map_types4()
{
	init_map_types_shared_sub(5, 3, 4);
}

/* Use spiral pattern to lookup a new position based on col, row. */
static map_pos_t
lookup_pattern(int col, int row, int index)
{
	int c = (col + globals.spiral_pattern[2*index]) & globals.map_col_mask;
	int r = (row + globals.spiral_pattern[2*index+1]) & globals.map_row_mask;
	return MAP_POS(c, r);
}


static int
init_map_desert_sub1(map_pos_t pos)
{
	int type_d = MAP_TYPE_DOWN(pos);
	int type_u = MAP_TYPE_UP(pos);

	if (type_d != 5 && type_d != 10) return -1;
	if (type_u != 5 && type_u != 10) return -1;

	type_d = MAP_TYPE_DOWN(MAP_MOVE_LEFT(pos));
	if (type_d != 5 && type_d != 10) return -1;

	type_d = MAP_TYPE_DOWN(MAP_MOVE_DOWN(pos));
	if (type_d != 5 && type_d != 0xa) return -1;

	return 0;
}

static int
init_map_desert_sub2(map_pos_t pos)
{
	int type_d = MAP_TYPE_DOWN(pos);
	int type_u = MAP_TYPE_UP(pos);

	if (type_d != 5 && type_d != 10) return -1;
	if (type_u != 5 && type_u != 10) return -1;

	type_u = MAP_TYPE_UP(MAP_MOVE_RIGHT(pos));
	if (type_u != 5 && type_u != 10) return -1;

	type_u = MAP_TYPE_UP(MAP_MOVE_UP(pos));
	if (type_u != 5 && type_u != 10) return -1;

	return 0;
}

/* Create deserts on the map. */
static void
init_map_desert()
{
	map_1_t *map = globals.map_mem2_ptr;

	for (int i = 0; i < globals.map_regions; i++) {
		for (int try = 0; try < 200; try++) {
			int col, row;
			map_pos_t rnd_pos = get_rnd_map_coord(&col, &row);

			if (MAP_TYPE_UP(rnd_pos) == 5 &&
			    MAP_TYPE_DOWN(rnd_pos) == 5) {
				for (int index = 255; index >= 0; index--) {
					map_pos_t pos = lookup_pattern(col, row, index);

					int r = init_map_desert_sub1(pos);
					if (r == 0) map[pos].type = (10 << 4) | (map[pos].type & 0xf);

					r = init_map_desert_sub2(pos);
					if (r == 0) map[pos].type = (map[pos].type & 0xf0) | 10;
				}
				break;
			}
		}
	}
}

static void
init_map_desert_2_sub()
{
	map_1_t *map = globals.map_mem2_ptr;

	for (int y = 0; y < globals.map_rows; y++) {
		for (int x = 0; x < globals.map_cols; x++) {
			map_pos_t pos = MAP_POS(x, y);
			int type_d = MAP_TYPE_DOWN(pos);
			int type_u = MAP_TYPE_UP(pos);

			if (type_d >= 7 && type_d < 10) type_d = 5;
			if (type_u >= 7 && type_u < 10) type_u = 5;

			map[pos].type = (type_u << 4) | type_d;
		}
	}
}

static void
init_map_desert_2()
{
	init_map_types_shared_sub(10, 5, 7);
	init_map_types_shared_sub(10, 7, 8);
	init_map_types_shared_sub(10, 8, 9);

	init_map_desert_2_sub();

	init_map_types_shared_sub(5, 10, 9);
	init_map_types_shared_sub(5, 9, 8);
	init_map_types_shared_sub(5, 8, 7);
}

/* Put crosses on top of mountains. */
static void
init_map_crosses()
{
	map_1_t *map = globals.map_mem2_ptr;

	for (int y = 0; y < globals.map_rows; y++) {
		for (int x = 0; x < globals.map_cols; x++) {
			map_pos_t pos = MAP_POS(x, y);
			int h = MAP_HEIGHT(pos);
			if (h >= 26 &&
			    h >= MAP_HEIGHT(MAP_MOVE_RIGHT(pos)) &&
			    h >= MAP_HEIGHT(MAP_MOVE_DOWN_RIGHT(pos)) &&
			    h >= MAP_HEIGHT(MAP_MOVE_DOWN(pos)) &&
			    h > MAP_HEIGHT(MAP_MOVE_LEFT(pos)) &&
			    h > MAP_HEIGHT(MAP_MOVE_UP_LEFT(pos)) &&
			    h > MAP_HEIGHT(MAP_MOVE_UP(pos))) {
				map[pos].obj = MAP_OBJ_CROSS;
			}
		}
	}
}

/* Check whether the hexagon at pos has triangles of types
   between min and max. Return -1 if not all triangles are
   in this range. */
static int
init_map_objects_shared_sub1(map_pos_t pos, int min, int max)
{
	int type_d = MAP_TYPE_DOWN(pos);
	int type_u = MAP_TYPE_UP(pos);

	if (type_d < min || type_d >= max) return -1;
	if (type_u < min || type_u >= max) return -1;

	type_d = MAP_TYPE_DOWN(MAP_MOVE_LEFT(pos));
	if (type_d < min || type_d >= max) return -1;

	type_d = MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(pos));
	type_u = MAP_TYPE_UP(MAP_MOVE_UP_LEFT(pos));
	if (type_d < min || type_d >= max) return -1;
	if (type_u < min || type_u >= max) return -1;

	/* Should be checkeing the up tri type. */
	if (globals.map_preserve_bugs) {
		type_d = MAP_TYPE_DOWN(MAP_MOVE_UP(pos));
		if (type_d < min || type_d >= max) return -1;
	} else {
		type_u = MAP_TYPE_UP(MAP_MOVE_UP(pos));
		if (type_u < min || type_u >= max) return -1;
	}

	return 0;
}

/* Get a random position in the spiral pattern based at col, row. */
static map_pos_t
lookup_rnd_pattern(int col, int row, int mask)
{
	int index = (random_int() & mask) * 2;
	int c = (col + globals.spiral_pattern[index]) & globals.map_col_mask;
	int r = (row + globals.spiral_pattern[index+1]) & globals.map_row_mask;
	return MAP_POS(c, r);
}

static void
init_map_objects_shared(int num_clusters, int objs_in_cluster, int pos_mask,
			int type_min, int type_max, int obj_base, int obj_mask)
{
	map_1_t *map = globals.map_mem2_ptr;

	for (int i = 0; i < num_clusters; i++) {
		for (int try = 0; try < 100; try++) {
			int col, row;
			map_pos_t rnd_pos = get_rnd_map_coord(&col, &row);
			int r = init_map_objects_shared_sub1(rnd_pos, type_min, type_max);
			if (r == 0) {
				for (int j = 0; j < objs_in_cluster; j++) {
					map_pos_t pos = lookup_rnd_pattern(col, row, pos_mask);
					int r = init_map_objects_shared_sub1(pos, type_min, type_max);
					if (r == 0 && MAP_OBJ(pos) == MAP_OBJ_NONE) {
						map[pos].obj = (random_int() & obj_mask) + obj_base;
					}
				}
				break;
			}
		}
	}
}

static void
init_map_trees_1()
{
	/* Add either tree or pine. */
	init_map_objects_shared(globals.map_regions << 3, 10, 0xff, 5, 7, MAP_OBJ_TREE_0, 0xf);
}

static void
init_map_trees_2()
{
	/* Add only trees. */
	init_map_objects_shared(globals.map_regions, 45, 0x3f, 5, 7, MAP_OBJ_TREE_0, 0x7);
}

static void
init_map_trees_3()
{
	/* Add only pines. */
	init_map_objects_shared(globals.map_regions, 30, 0x3f, 4, 7, MAP_OBJ_PINE_0, 0x7);
}

static void
init_map_trees_4()
{
	/* Add either tree or pine. */
	init_map_objects_shared(globals.map_regions, 20, 0x7f, 5, 7, MAP_OBJ_TREE_0, 0xf);
}

static void
init_map_stone_1()
{
	init_map_objects_shared(globals.map_regions, 40, 0x3f, 5, 7, MAP_OBJ_STONE_0, 0x7);
}

static void
init_map_stone_2()
{
	init_map_objects_shared(globals.map_regions, 15, 0xff, 5, 7, MAP_OBJ_STONE_0, 0x7);
}

static void
init_map_dead_trees()
{
	init_map_objects_shared(globals.map_regions, 2, 0xff, 5, 7, MAP_OBJ_DEAD_TREE, 0);
}

static void
init_map_large_boulders()
{
	init_map_objects_shared(globals.map_regions, 6, 0xff, 5, 7, MAP_OBJ_SANDSTONE_0, 0x1);
}

static void
init_map_water_trees()
{
	init_map_objects_shared(globals.map_regions, 50, 0x7f, 2, 4, MAP_OBJ_WATER_TREE_0, 0x3);
}

static void
init_map_stubs()
{
	init_map_objects_shared(globals.map_regions, 5, 0xff, 5, 7, MAP_OBJ_STUB, 0);
}

static void
init_map_small_boulders()
{
	init_map_objects_shared(globals.map_regions, 10, 0xff, 5, 7, MAP_OBJ_STONE, 0x1);
}

static void
init_map_cadavers()
{
	init_map_objects_shared(globals.map_regions, 2, 0xf, 10, 11, MAP_OBJ_CADAVER_0, 0x1);
}

static void
init_map_cacti()
{
	init_map_objects_shared(globals.map_regions, 6, 0x7f, 8, 11, MAP_OBJ_CACTUS_0, 0x1);
}

static void
init_map_water_stones()
{
	init_map_objects_shared(globals.map_regions, 8, 0x7f, 0, 3, MAP_OBJ_WATER_STONE_0, 0x1);
}

static void
init_map_palms()
{
	init_map_objects_shared(globals.map_regions, 6, 0x3f, 10, 11, MAP_OBJ_PALM_0, 0x3);
}

static void
init_map_resources_shared_sub(map_2_t *map_data, int iters, int col, int row, int *index, int amount, int res_type)
{
	for (int i = 0; i < iters; i++) {
		map_pos_t pos = lookup_pattern(col, row, *index);
		*index += 1;

		int res = map_data[pos].u.s.resource;
		if (res == 0 || (res & 0x1f) < amount) {
			map_data[pos].u.s.resource = (res_type << 5) + amount;
		}
	}
}

static void
init_map_resources_shared(int num_clusters, int res_type, int min, int max)
{
	map_1_t *map = globals.map_mem2_ptr;
	map_2_t *map_data = MAP_2_DATA(map);

	for (int i = 0; i < num_clusters; i++) {
		for (int try = 0; try < 100; try++) {
			int col, row;
			map_pos_t pos = get_rnd_map_coord(&col, &row);

			if (map_data[pos].u.s.field_1 == 0 &&
			    init_map_objects_shared_sub1(pos, min, max) == 0) {
				int index = 0;
				int amount = 8 + (random_int() & 0xc);
				init_map_resources_shared_sub(map_data, 1, col, row, &index, amount, res_type);
				amount -= 4;
				if (amount == 0) break;

				init_map_resources_shared_sub(map_data, 6, col, row, &index, amount, res_type);
				amount -= 4;
				if (amount == 0) break;

				init_map_resources_shared_sub(map_data, 12, col, row, &index, amount, res_type);
				amount -= 4;
				if (amount == 0) break;

				init_map_resources_shared_sub(map_data, 18, col, row, &index, amount, res_type);
				amount -= 4;
				if (amount == 0) break;

				init_map_resources_shared_sub(map_data, 24, col, row, &index, amount, res_type);
				amount -= 4;
				if (amount == 0) break;

				init_map_resources_shared_sub(map_data, 30, col, row, &index, amount, res_type);
				break;
			}
		}
	}
}

/* Initialize resources in the ground. */
static void
init_map_resources_1()
{
	init_map_resources_shared(globals.map_regions * 9, 3, 11, 15);
}

static void
init_map_resources_2()
{
	init_map_resources_shared(globals.map_regions * 4, 2, 11, 15);
}

static void
init_map_resources_3()
{
	init_map_resources_shared(globals.map_regions * 2, 1, 11, 15);
}

static void
init_map_resources_4()
{
	init_map_resources_shared(globals.map_regions * 2, 4, 11, 15);
}

static void
init_map_clean_up()
{
	map_1_t *map = globals.map_mem2_ptr;

	for (int y = 0; y < globals.map_rows; y++) {
		for (int x = 0; x < globals.map_cols; x++) {
			map_pos_t pos = MAP_POS(x, y);
			map_space_t s = map_space_from_obj[MAP_OBJ(pos)];
			if (s >= MAP_SPACE_IMPASSABLE) {
				if (!BIT_TEST(map[MAP_MOVE_LEFT(pos)].flags, 6) &&
				    !BIT_TEST(map[MAP_MOVE_UP_LEFT(pos)].flags, 6) &&
				    !BIT_TEST(map[MAP_MOVE_UP(pos)].flags, 6)) {
					map[pos].flags |= BIT(6);
				} else {
					map[pos].obj &= 0x80;
				}
			}
		}
	}
}

static void
init_map_sub()
{
	init_map_lakes();

	/* draw_progress_bar(2); */

	init_map_types4();

	/* draw_progress_bar(1); */

	init_map_desert();

	/* draw_progress_bar(1); */

	init_map_desert_2();

	/* draw_progress_bar(3); */

	init_map_crosses();

	/* draw_progress_bar(1); */

	init_map_trees_1();
	init_map_trees_2();

	/* draw_progress_bar(1); */

	init_map_trees_3();
	init_map_trees_4();

	/* draw_progress_bar(1); */

	init_map_stone_1();
	init_map_stone_2();

	/* draw_progress_bar(1); */

	init_map_dead_trees();
	init_map_large_boulders();
	init_map_water_trees();
	init_map_stubs();

	/* draw_progress_bar(1); */

	init_map_small_boulders();
	init_map_cadavers();
	init_map_cacti();
	init_map_water_stones();
	init_map_palms();

	/* draw_progress_bar(1); */

	init_map_resources_1();

	/* draw_progress_bar(1); */

	init_map_resources_2();

	/* draw_progress_bar(1); */

	init_map_resources_3();

	/* draw_progress_bar(1); */

	init_map_resources_4();

	/* draw_progress_bar(1); */

	init_map_clean_up();

	/* draw_progress_bar(1); */
}

/* Mark tiles that are to have waves painted. */
static void
init_map_waves()
{
	map_1_t *map = globals.map_mem2_ptr;
	for (int y = 0; y < globals.map_rows; y++) {
		for (int x = 0; x < globals.map_cols; x++) {
			map_pos_t pos = MAP_POS(x, y);
			if (MAP_TYPE_UP(pos) < 4 || MAP_TYPE_DOWN(pos) < 4) {
				map[pos].obj |= BIT(7);
			}
		}
	}
}

/* Initialize global count of gold deposits. */
static void
init_map_ground_gold_deposit()
{
	int total_gold = 0;

	for (int y = 0; y < globals.map_rows; y++) {
		for (int x = 0; x < globals.map_cols; x++) {
			map_pos_t pos = MAP_POS(x, y);
			if (MAP_RES_TYPE(pos) == GROUND_DEPOSIT_GOLD) {
				total_gold += MAP_RES_AMOUNT(pos);
			}
		}
	}

	globals.map_gold_deposit = total_gold;
}

/* Initialize minimap data. */
static void
init_minimap()
{
	static const uint16_t w_arr[] = {
		0, 85, 102, 119, 17, 17, 17, 17,
		34, 34, 34, 51, 51, 51, 68, 68
	};

	static const uint8_t b_arr[] = {
		 8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
		31, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
		63, 63, 62, 61, 61, 60, 59, 59, 58, 57, 57, 56, 55, 55, 54, 53, 53,
		61, 61, 60, 60, 59, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48,
		47, 47, 46, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33,
		 9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
		10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
		11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11
	};

	globals.minimap = malloc(globals.map_rows * globals.map_cols);
	map_1_t *map = globals.map_mem2_ptr;
	uint8_t *minimap = globals.minimap;

	for (int y = 0; y < globals.map_rows; y++) {
		for (int x = 0; x < globals.map_cols; x++) {
			int pos = y * globals.map_col_size + x;
			uint32_t edi14 = w_arr[map[pos].type >> 4];

			pos = MAP_MOVE_RIGHT(pos);
			uint32_t edi18 = map[pos].height & 0x1f;
		
			pos = MAP_MOVE_DOWN_LEFT(pos);
			int32_t edi1C_2 = map[pos].height & 0x1f;
		
			edi1C_2 = edi1C_2 - edi18 + 8;
			edi14 += edi1C_2;

			*(minimap++) = b_arr[edi14];
		}
	}
}

/* Clear map data. */
static void
reset_mem_map2()
{
	memset(globals.map_mem2_ptr, '\0', globals.map_elms * 8);
}

void
map_init()
{
	/* globals.svga &= ~BIT(5); */

	/* initialize rnd state */
	globals.rnd_1 = globals.init_map_rnd_1;
	globals.rnd_2 = globals.init_map_rnd_2;
	globals.rnd_3 = globals.init_map_rnd_3;

	random_int();
	random_int();
	/* progress_reset(); */

	/* draw_progress_bar(1); */

	reset_mem_map2();

	/* draw_progress_bar(1); */

	init_map_heights_squares();
	switch (globals.map_generator) {
	case 0:
		init_map_heights_midpoints(); /* Midpoint displacement algorithm */
		break;
	case 1:
		init_map_heights_diamond_square(); /* Diamond square algorithm */
		break;
	}

	/* draw_progress_bar(3); */

	clamp_map_heights();

	/* draw_progress_bar(4); */

	map_init_sea_level();

	/* draw_progress_bar(3); */

	map_heights_rebase();

	/* draw_progress_bar(1); */

	init_map_types();

	/* draw_progress_bar(1); */

	init_map_types_2();

	/* draw_progress_bar(5); */

	map_heights_rescale();

	/* draw_progress_bar(1); */

	init_map_sub();
	init_map_waves();
	init_map_ground_gold_deposit();

	/* draw_progress_bar(1); */

	init_minimap();

	/* draw_progress_bar(1); */

	/* globals.svga |= BIT(5); */
}

/* Change the height of a map position. */
void
map_set_height(map_pos_t pos, int height)
{
	map_1_t *map = globals.map_mem2_ptr;
	map[pos].height = (map[pos].height & 0xe0) | (height & 0x1f);

	/* Mark landscape dirty in viewport. */
	viewport_redraw_map_pos(pos);
}

/* Change the object at a map position. */
void
map_set_object(map_pos_t pos, map_obj_t obj)
{
	map_1_t *map = globals.map_mem2_ptr;
	map[pos].obj = (map[pos].obj & 0x80) | (obj & 0x7f);

	/* TODO Mark dirty in viewport. */
}

/* Remove resources from the ground at a map position. */
void
map_remove_ground_deposit(map_pos_t pos, int amount)
{
	map_2_t *map_data = MAP_2_DATA(globals.map_mem2_ptr);
	map_data[pos].u.s.resource -= amount;

	if (MAP_RES_AMOUNT(pos) == 0) {
		/* Also sets the ground deposit type to none. */
		map_data[pos].u.s.resource = 0;
	}
}

/* Remove fish at a map position (must be water). */
void
map_remove_fish(map_pos_t pos, int amount)
{
	map_2_t *map_data = MAP_2_DATA(globals.map_mem2_ptr);
	map_data[pos].u.s.resource -= amount;
}

/* Set the index of the serf occupying map position. */
void
map_set_serf_index(map_pos_t pos, int index)
{
	map_2_t *map_data = MAP_2_DATA(globals.map_mem2_ptr);
	map_data[pos].serf_index = index;

	/* TODO Mark dirty in viewport. */
}

/* Update public parts of the map data. */
static void
map_update_public(map_pos_t pos)
{
	/* Update other map objects */
	int r;
	switch (MAP_OBJ(pos)) {
	case MAP_OBJ_STUB:
		if ((random_int() & 3) == 0) map_set_object(pos, MAP_OBJ_NONE);
		break;
	case MAP_OBJ_FELLED_PINE_0: case MAP_OBJ_FELLED_PINE_1:
	case MAP_OBJ_FELLED_PINE_2: case MAP_OBJ_FELLED_PINE_3:
	case MAP_OBJ_FELLED_PINE_4:
	case MAP_OBJ_FELLED_TREE_0: case MAP_OBJ_FELLED_TREE_1:
	case MAP_OBJ_FELLED_TREE_2: case MAP_OBJ_FELLED_TREE_3:
	case MAP_OBJ_FELLED_TREE_4:
		map_set_object(pos, MAP_OBJ_STUB);
		break;
	case MAP_OBJ_NEW_PINE:
		r = random_int();
		if ((r & 0x300) == 0) map_set_object(pos, MAP_OBJ_PINE_0 + (r & 7));
		break;
	case MAP_OBJ_NEW_TREE:
		r = random_int();
		if ((r & 0x300) == 0) map_set_object(pos, MAP_OBJ_TREE_0 + (r & 7));
		break;
	case MAP_OBJ_SEEDS_0: case MAP_OBJ_SEEDS_1:
	case MAP_OBJ_SEEDS_2: case MAP_OBJ_SEEDS_3:
	case MAP_OBJ_SEEDS_4:
	case MAP_OBJ_FIELD_0: case MAP_OBJ_FIELD_1:
	case MAP_OBJ_FIELD_2: case MAP_OBJ_FIELD_3:
	case MAP_OBJ_FIELD_4:
		map_set_object(pos, MAP_OBJ(pos)+1);
		break;
	case MAP_OBJ_SEEDS_5:
		map_set_object(pos, MAP_OBJ_FIELD_0);
		break;
	case MAP_OBJ_FIELD_EXPIRED:
		map_set_object(pos, MAP_OBJ_NONE);
		break;
	case MAP_OBJ_SIGN_LARGE_GOLD: case MAP_OBJ_SIGN_SMALL_GOLD:
	case MAP_OBJ_SIGN_LARGE_IRON: case MAP_OBJ_SIGN_SMALL_IRON:
	case MAP_OBJ_SIGN_LARGE_COAL: case MAP_OBJ_SIGN_SMALL_COAL:
	case MAP_OBJ_SIGN_LARGE_STONE: case MAP_OBJ_SIGN_SMALL_STONE:
	case MAP_OBJ_SIGN_EMPTY:
		if (globals.update_map_16_loop == 0) {
			map_set_object(pos, MAP_OBJ_NONE);
		}
		break;
	case MAP_OBJ_FIELD_5:
		map_set_object(pos, MAP_OBJ_FIELD_EXPIRED);
		break;
	default:
		break;
	}
}

/* Update hidden parts of the map data. */
static void
map_update_hidden(map_pos_t pos)
{
	map_2_t *map_data = MAP_2_DATA(globals.map_mem2_ptr);

	/* Update fish resources in water */
	if (MAP_WATER_2(pos) && MAP_WATER_1(pos)) {
		if (map_data[pos].u.s.resource) {
			int r = random_int();

			if (map_data[pos].u.s.resource < 10 && (r & 0x3f00)) {
				/* Spawn more fish. */
				map_data[pos].u.s.resource += 1;
			}

			/* Move in a random direction of: right, down right, left, up left */
			map_pos_t adj_pos = pos;
			switch ((r >> 2) & 3) {
			case 0: adj_pos = MAP_MOVE_RIGHT(adj_pos); break;
			case 1: adj_pos = MAP_MOVE_DOWN_RIGHT(adj_pos); break;
			case 2: adj_pos = MAP_MOVE_LEFT(adj_pos); break;
			case 3: adj_pos = MAP_MOVE_UP_LEFT(adj_pos); break;
			default: NOT_REACHED(); break;
			}

			if (MAP_WATER_1(adj_pos)) {
				/* Migrate a fish to adjacent water space. */
				map_data[pos].u.s.resource -= 1;
				map_data[adj_pos].u.s.resource += 1;
			}
		}
	}
}

/* Update map data as part of the game progression. */
void
map_update()
{
	uint16_t delta = globals.anim - globals.update_map_last_anim;
	globals.update_map_last_anim = globals.anim;
	globals.update_map_counter -= delta;

	int iters = 0;
	while (globals.update_map_counter < 0) {
		iters += globals.map_regions;
		globals.update_map_counter += 20;
	}

	map_pos_t pos = globals.update_map_initial_pos;

	for (int i = 0; i < iters; i++) {
		globals.update_map_16_loop -= 1;
		if (globals.update_map_16_loop < 0) globals.update_map_16_loop = 16;

		/* Test if moving 23 positions right crosses map boundary. */
		if ((pos & globals.map_col_mask) + 23 < globals.map_cols) {
			pos = MAP_MOVE_RIGHT_N(pos, 23);
		} else {
			pos = MAP_MOVE_RIGHT_N(pos, 23);
			pos = MAP_MOVE_DOWN(pos);
		}

		/* Update map at position. */
		map_update_hidden(pos);
		map_update_public(pos);
	}

	globals.update_map_initial_pos = pos;
}
