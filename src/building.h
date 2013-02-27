/*
 * building.h - Building related functions.
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


#ifndef _BUILDING_H
#define _BUILDING_H

#include <stdint.h>

#include "map.h"


#define BUILDING_INDEX(ptr)  ((int)((ptr) - globals.buildings))
#define BUILDING_ALLOCATED(i)  BIT_TEST(globals.buildings_bitmap[(i)>>3], 7-((i)&7))

#define BUILDING_PLAYER(building)  ((int)((building)->bld & 3))
#define BUILDING_TYPE(building)  ((building_type_t)(((building)->bld >> 2) & 0x1f))
#define BUILDING_IS_DONE(building)  (!(((building)->bld >> 7) & 1))

#define BUILDING_STATE(building)  ((int)((building)->serf & 3))
#define BUILDING_SERF_REQUEST_FAIL(building)  ((int)(((building)->serf >> 2) & 1))
#define BUILDING_PLAYING_SFX(building)  ((int)(((building)->serf >> 3) & 1))
#define BUILDING_IS_ACTIVE(building)  ((int)(((building)->serf >> 4) & 1))
#define BUILDING_IS_BURNING(building)  ((int)(((building)->serf >> 5) & 1))
#define BUILDING_HAS_SERF(building)  ((int)(((building)->serf >> 6) & 1))
#define BUILDING_SERF_REQUESTED(building)  ((int)(((building)->serf >> 7) & 1))

#define BUILDING_HAS_INVENTORY(building)  ((building)->stock[0].requested == 0xff)


typedef enum {
	BUILDING_NONE = 0,
	BUILDING_FISHER,
	BUILDING_LUMBERJACK,
	BUILDING_BOATBUILDER,
	BUILDING_STONECUTTER,
	BUILDING_STONEMINE,
	BUILDING_COALMINE,
	BUILDING_IRONMINE,
	BUILDING_GOLDMINE,
	BUILDING_FORESTER,
	BUILDING_STOCK,
	BUILDING_HUT,
	BUILDING_FARM,
	BUILDING_BUTCHER,
	BUILDING_PIGFARM,
	BUILDING_MILL,
	BUILDING_BAKER,
	BUILDING_SAWMILL,
	BUILDING_STEELSMELTER,
	BUILDING_TOOLMAKER,
	BUILDING_WEAPONSMITH,
	BUILDING_TOWER,
	BUILDING_FORTRESS,
	BUILDING_GOLDSMELTER,
	BUILDING_CASTLE
} building_type_t;


typedef struct {
	int available;
	int requested;
} building_stock_t;

typedef struct building building_t;

struct building {
	map_pos_t pos;
	int bld;
	int serf;
	int flg_index;
	building_stock_t stock[2];
	int serf_index; /* Also used for burning building counter. */
	int progress;
	union {
		struct inventory *inventory;
		struct flag *flag;
		uint16_t anim; /* Used for burning building. */
		struct {
			uint16_t level;
			uint8_t planks_needed;
			uint8_t stone_needed;
		} s;
	} u;
};


int building_get_score_from_type(building_type_t type);


#endif /* ! _BUILDING_H */
