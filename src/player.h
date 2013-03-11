/*
 * player.h - Player related functions
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

#ifndef _PLAYER_H
#define _PLAYER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "freeserf.h"
#include "map.h"
#include "gfx.h"


/* Whether player has built the initial castle. */
#define PLAYER_HAS_CASTLE(player)  ((int)((player)->flags & 1))
/* Whether the strongest knight should be sent to fight. */
#define PLAYER_SEND_STRONGEST(player)  ((int)(((player)->flags >> 1) & 1))
/* Whether cycling of knights is in progress. */
#define PLAYER_CYCLING_KNIGHTS(player)  ((int)(((player)->flags >> 2) & 1))
/* Whether a message is queued for this player. */
#define PLAYER_HAS_MESSAGE(player)  ((int)(((player)->flags >> 3) & 1))
/* Whether the knight level of military buildings is temporarily
   reduced bacause of cycling of the knights. */
#define PLAYER_REDUCED_KNIGHT_LEVEL(player)  ((int)(((player)->flags >> 4) & 1))
/* Whether the cycling of knights is in the second phase. */
#define PLAYER_CYCLING_SECOND(player)  ((int)(((player)->flags >> 5) & 1))
/* Whether this player is active. */
#define PLAYER_IS_ACTIVE(player)  ((int)(((player)->flags >> 6) & 1))
/* Whether this player is a computer controlled opponent. */
#define PLAYER_IS_AI(player)  ((int)(((player)->flags >> 7) & 1))


/* Whether player is prohibited from building military
   buildings at current position. */
#define PLAYER_ALLOW_MILITARY(player)  (!(int)((player)->build & 1))
/* Whether player is prohibited from building flag at
   current position. */
#define PLAYER_ALLOW_FLAG(player)  (!(int)(((player)->build >> 1) & 1))
/* Whether player can spawn new serfs. */
#define PLAYER_CAN_SPAWN(player)  ((int)(((player)->build >> 2) & 1))


typedef enum {
	MAP_CURSOR_TYPE_NONE = 0,
	MAP_CURSOR_TYPE_FLAG,
	MAP_CURSOR_TYPE_REMOVABLE_FLAG,
	MAP_CURSOR_TYPE_BUILDING,
	MAP_CURSOR_TYPE_PATH,
	MAP_CURSOR_TYPE_CLEAR_BY_FLAG,
	MAP_CURSOR_TYPE_CLEAR_BY_PATH,
	MAP_CURSOR_TYPE_CLEAR
} map_cursor_type_t;

/* player_t object. Holds the game state of a player. */
typedef struct {
	/* pl_sett_pre */
	int tool_prio[9];
	int resource_count[26];
	int flag_prio[26];
	int serf_count[27];
	int knight_occupation[4];

	/* pl_sett */
	int player_num;
	int flags;
	int build;
	int completed_building_count[24];
	int incomplete_building_count[24];
	/* 60 */
	int inventory_prio[26];
	int attacking_buildings[64];
	int current_sett_5_item;
	map_pos_t map_cursor_pos;
	/* 100 */
	map_cursor_type_t map_cursor_type;
	panel_btn_t panel_btn_type;
	int building_height_after_level;
	int building;
	int castle_flag;
	int castle_inventory;
	int cont_search_after_non_optimal_find;
	int knights_to_spawn;
	/*int spawn_serf_want_knight; OBSOLETE by local var */
	/* 110 */
	int field_110;
	uint total_land_area;
	uint total_building_score;
	uint total_military_score;
	uint16_t last_anim;
	/* 120 */
	int reproduction_counter;
	int reproduction_reset;
	int serf_to_knight_rate;
	uint16_t serf_to_knight_counter; /* Overflow is important */
	int attacking_building_count;
	int attacking_knights[4];
	int total_attacking_knights;
	int building_attacked;
	int knights_attacking;
	int analysis_goldore;
	int analysis_ironore;
	int analysis_coal;
	int analysis_stone;
	/* 140 */
	int food_stonemine; /* Food delivery priority of food for mines. */
	int food_coalmine;
	int food_ironmine;
	int food_goldmine;
	int planks_construction; /* Planks delivery priority. */
	int planks_boatbuilder;
	int planks_toolmaker;
	int steel_toolmaker;
	/* 150 */
	int steel_weaponsmith;
	int coal_steelsmelter;
	int coal_goldsmelter;
	int coal_weaponsmith;
	int wheat_pigfarm;
	int wheat_mill;
	int current_sett_6_item;

	/* +1 for every castle defeated,
	   -1 for own castle lost. */
	int castle_score;
	/* 162 */
	int initial_supplies;
	/* 164 */
	int extra_planks;
	int extra_stone;
	int lumberjack_index;
	int sawmill_index;
	int stonecutter_index;
	/* 16E */
	int serf_index;
	/* 170 */
	int knight_cycle_counter;
	int timers_count;
	/* 176 */
	int index;
	int military_max_gold;
	int military_gold;
	/* 180 */
	int inventory_gold;
	int knight_morale;
	int gold_deposited;
	int castle_knights_wanted;
	int castle_knights;
	int ai_value_0;
	int ai_value_1;
	int ai_value_2;
	int ai_value_3;
	int ai_value_4;
	int ai_value_5;
	/* 1AE */
	int ai_intelligence;
	/* AC4 */
	int player_stat_history[16][112];
	int resource_count_history[26][120];
	/* 1E34 ? */
	int msg_queue_type[64];
	map_pos_t msg_queue_pos[64];

	struct {
		int timeout;
		map_pos_t pos;
	} timers[64];

	/* ... */
} player_t;


void player_add_notification(player_t *player, int type, map_pos_t pos);

void player_reset_food_priority(player_t *player);
void player_reset_planks_priority(player_t *player);
void player_reset_steel_priority(player_t *player);
void player_reset_coal_priority(player_t *player);
void player_reset_wheat_priority(player_t *player);
void player_reset_tool_priority(player_t *player);

void player_reset_flag_priority(player_t *player);
void player_reset_inventory_priority(player_t *player);

void player_change_knight_occupation(player_t *player, int index,
				     int adjust_max, int delta);

int player_promote_serfs_to_knights(player_t *player, int number);
int player_knights_available_for_attack(player_t *player, map_pos_t pos);
void player_start_attack(player_t *player);


#endif /* ! _PLAYER_H */
