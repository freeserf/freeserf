/*
 * freeserf.h - Various definitions.
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

#ifndef _FREESERF_H
#define _FREESERF_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "gfx.h"
#include "map.h"
#include "random.h"
#include "building.h"
#include "serf.h"


/* The length between game updates in miliseconds. */
#define TICK_LENGTH  20
#define TICKS_PER_SEC  (1000/20)


#define INVENTORY_INDEX(ptr)  ((int)((ptr) - game.inventories))
#define INVENTORY_ALLOCATED(i)  BIT_TEST(game.inventory_bitmap[(i)>>3], 7-((i)&7))

#define DIR_REVERSE(dir)  (((dir) + 3) % 6)


typedef enum {
	DIR_RIGHT = 0,
	DIR_DOWN_RIGHT,
	DIR_DOWN,
	DIR_LEFT,
	DIR_UP_LEFT,
	DIR_UP,

	DIR_UP_RIGHT,
	DIR_DOWN_LEFT
} dir_t;


typedef enum {
	PANEL_BTN_BUILD_INACTIVE = 0,
	PANEL_BTN_BUILD_FLAG,
	PANEL_BTN_BUILD_MINE,
	PANEL_BTN_BUILD_SMALL,
	PANEL_BTN_BUILD_LARGE,
	PANEL_BTN_BUILD_CASTLE,
	PANEL_BTN_DESTROY,
	PANEL_BTN_DESTROY_INACTIVE,
	PANEL_BTN_BUILD_ROAD,
	PANEL_BTN_MAP_INACTIVE,
	PANEL_BTN_MAP,
	PANEL_BTN_STATS_INACTIVE,
	PANEL_BTN_STATS,
	PANEL_BTN_SETT_INACTIVE,
	PANEL_BTN_SETT,
	PANEL_BTN_DESTROY_ROAD,
	PANEL_BTN_GROUND_ANALYSIS,
	PANEL_BTN_BUILD_SMALL_STARRED,
	PANEL_BTN_BUILD_LARGE_STARRED,
	PANEL_BTN_MAP_STARRED,
	PANEL_BTN_STATS_STARRED,
	PANEL_BTN_SETT_STARRED,
	PANEL_BTN_GROUND_ANALYSIS_STARRED,
	PANEL_BTN_BUILD_MINE_STARRED,
	PANEL_BTN_BUILD_ROAD_STARRED
} panel_btn_t;

typedef enum {
	BOX_MAP = 1,
	BOX_MAP_OVERLAY,
	BOX_MINE_BUILDING,
	BOX_BASIC_BLD,
	BOX_BASIC_BLD_FLIP,
	BOX_ADV_1_BLD,
	BOX_ADV_2_BLD,
	BOX_STAT_SELECT,
	BOX_STAT_4,
	BOX_STAT_BLD_1,
	BOX_STAT_BLD_2,
	BOX_STAT_BLD_3,
	BOX_STAT_BLD_4,
	BOX_STAT_8,
	BOX_STAT_7,
	BOX_STAT_1,
	BOX_STAT_2,
	BOX_STAT_6,
	BOX_STAT_3,
	BOX_START_ATTACK,
	BOX_START_ATTACK_REDRAW,
	BOX_GROUND_ANALYSIS,
	BOX_LOAD_ARCHIVE,
	BOX_LOAD_SAVE,
	BOX_25,
	BOX_DISK_MSG,
	BOX_SETT_SELECT,
	BOX_SETT_1,
	BOX_SETT_2,
	BOX_SETT_3,
	BOX_KNIGHT_LEVEL,
	BOX_SETT_4,
	BOX_SETT_5,
	BOX_QUIT_CONFIRM,
	BOX_NO_SAVE_QUIT_CONFIRM,
	BOX_SETT_SELECT_FILE,
	BOX_OPTIONS,
	BOX_CASTLE_RES,
	BOX_MINE_OUTPUT,
	BOX_ORDERED_BLD,
	BOX_DEFENDERS,
	BOX_TRANSPORT_INFO,
	BOX_CASTLE_SERF,
	BOX_RESDIR,
	BOX_SETT_8,
	BOX_SETT_6,
	BOX_BLD_1,
	BOX_BLD_2,
	BOX_BLD_3,
	BOX_BLD_4,
	BOX_MESSAGE,
	BOX_BLD_STOCK,
	BOX_PLAYER_FACES,
	BOX_GAME_END,
	BOX_DEMOLISH,
	BOX_JS_CALIB,
	BOX_JS_CALIB_UPLEFT,
	BOX_JS_CALIB_DOWNRIGHT,
	BOX_JS_CALIB_CENTER,
	BOX_CTRLS_INFO
} box_t;

typedef enum {
	RESOURCE_FISH = 0,
	RESOURCE_PIG,
	RESOURCE_MEAT,
	RESOURCE_WHEAT,
	RESOURCE_FLOUR,
	RESOURCE_BREAD,
	RESOURCE_LUMBER,
	RESOURCE_PLANK,
	RESOURCE_BOAT,
	RESOURCE_STONE,
	RESOURCE_IRONORE,
	RESOURCE_STEEL,
	RESOURCE_COAL,
	RESOURCE_GOLDORE,
	RESOURCE_GOLDBAR,
	RESOURCE_SHOVEL,
	RESOURCE_HAMMER,
	RESOURCE_ROD,
	RESOURCE_CLEAVER,
	RESOURCE_SCYTHE,
	RESOURCE_AXE,
	RESOURCE_SAW,
	RESOURCE_PICK,
	RESOURCE_PINCER,
	RESOURCE_SWORD,
	RESOURCE_SHIELD,

	RESOURCE_GROUP_FOOD
} resource_type_t;

typedef struct {
	int sprite;
	int x, y;
} sprite_loc_t;

typedef struct {
	int face;
	int supplies;
	int intelligence;
	int reproduction;
} player_init_t;

typedef struct {
	/* 8 */
	random_state_t rnd;
	int pl_0_supplies;
	int pl_0_reproduction;
	int pl_1_face;
	int pl_1_intelligence;
	int pl_1_supplies;
	int pl_1_reproduction;
	int pl_2_face;
	int pl_2_intelligence;
	int pl_2_supplies;
	int pl_2_reproduction;
	int pl_3_face;
	int pl_3_intelligence;
	int pl_3_supplies;
	int pl_3_reproduction;
	/* ... */
} map_spec_t;

typedef struct inventory inventory_t;

struct inventory {
	int player_num;
	int res_dir;
	int flg_index;
	int bld_index;
	int resources[26];
	int out_queue[2];
	int out_dest[2];
	int spawn_priority;
	int serfs[27];
};


static const int map_building_sprite[] = {
	0, 0xa7, 0xa8, 0xae, 0xa9,
	0xa3, 0xa4, 0xa5, 0xa6,
	0xaa, 0xc0, 0xab, 0x9a, 0x9c, 0x9b, 0xbc,
	0xa2, 0xa0, 0xa1, 0x99, 0x9d, 0x9e, 0x98, 0x9f, 0xb2
};

void game_loop_quit();

#endif /* ! _FREESERF_H */
