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

#include "map.h"
#include "resource.h"

/* Max number of different types of resources accepted by buildings. */
#define BUILDING_MAX_STOCK  2

#define BUILDING_INDEX(ptr)  ((int)((ptr) - game.buildings))
#define BUILDING_ALLOCATED(i)  BIT_TEST(game.building_bitmap[(i)>>3], 7-((i)&7))

/* Type of building. */
#define BUILDING_TYPE(building)  ((building)->type)
/* Owning player of the building. */
#define BUILDING_PLAYER(building)  ((int)((building)->bld & 3))
/* Whether construction of the building is finished. */
#define BUILDING_IS_DONE(building)  (!(((building)->bld >> 7) & 1))

/* The military state of the building. Higher values mean that
   the building is closer to the enemy. */
#define BUILDING_STATE(building)  ((int)((building)->serf & 3))
/* Building is currently playing back a sound effect. */
#define BUILDING_PLAYING_SFX(building)  ((int)(((building)->serf >> 3) & 1))
/* Building is active (specifics depend on building type). */
#define BUILDING_IS_ACTIVE(building)  ((int)(((building)->serf >> 4) & 1))
/* Building is burning. */
#define BUILDING_IS_BURNING(building)  ((int)(((building)->serf >> 5) & 1))
/* Building has an associated serf. */
#define BUILDING_HAS_SERF(building)  ((int)(((building)->serf >> 6) & 1))
/* Building has succesfully requested a serf. */
#define BUILDING_SERF_REQUESTED(building)  ((int)(((building)->serf >> 7) & 1))
/* Building has requested a serf but none was available. */
#define BUILDING_SERF_REQUEST_FAIL(building)  ((int)(((building)->serf >> 2) & 1))

/* Building has inventory and the inventory pointer
   is valid. */
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
	resource_type_t type;
	int prio;
	int available;
	int requested;
	int maximum;
} building_stock_t;

typedef struct {
	/* Map position of building */
	map_pos_t pos;
	/* Type of building */
	building_type_t type;
	/* Flags */
	int bld;
	int serf;
	/* Index of flag connected to this building */
	int flag;
	/* Stock of this building */
	building_stock_t stock[BUILDING_MAX_STOCK];
	int serf_index; /* Also used for burning building counter. */
	int progress;
	union {
		inventory_t *inventory;
		uint16_t tick; /* Used for burning building. */
		uint16_t level;
	} u;
} building_t;


int building_get_score_from_type(building_type_t type);


#endif /* ! _BUILDING_H */
