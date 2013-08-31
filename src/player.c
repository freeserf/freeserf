/*
 * player.c - Player related functions
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
#include <assert.h>

#include "player.h"
#include "freeserf.h"
#include "game.h"
#include "debug.h"


/* Enqueue a new notification message for player. */
void
player_add_notification(player_t *player, int type, map_pos_t pos)
{
	player->flags |= BIT(3); /* Message in queue. */
	for (int i = 0; i < 64; i++) {
		if (player->msg_queue_type[i] == 0) {
			player->msg_queue_type[i] = type;
			player->msg_queue_pos[i] = pos;
			break;
		}
	}
}

/* Set defaults for food distribution priorities. */
void
player_reset_food_priority(player_t *player)
{
	player->food_stonemine = 13100;
	player->food_coalmine = 45850;
	player->food_ironmine = 45850;
	player->food_goldmine = 65500;
}

/* Set defaults for planks distribution priorities. */
void
player_reset_planks_priority(player_t *player)
{
	player->planks_construction = 65500;
	player->planks_boatbuilder = 3275;
	player->planks_toolmaker = 19650;
}

/* Set defaults for steel distribution priorities. */
void
player_reset_steel_priority(player_t *player)
{
	player->steel_toolmaker = 45850;
	player->steel_weaponsmith = 65500;
}

/* Set defaults for coal distribution priorities. */
void
player_reset_coal_priority(player_t *player)
{
	player->coal_steelsmelter = 32750;
	player->coal_goldsmelter = 65500;
	player->coal_weaponsmith = 52400;
}

/* Set defaults for coal distribution priorities. */
void
player_reset_wheat_priority(player_t *player)
{
	player->wheat_pigfarm = 65500;
	player->wheat_mill = 32750;
}

/* Set defaults for tool production priorities. */
void
player_reset_tool_priority(player_t *player)
{
	player->tool_prio[0] = 9825; /* SHOVEL */
	player->tool_prio[1] = 65500; /* HAMMER */
	player->tool_prio[2] = 13100; /* ROD */
	player->tool_prio[3] = 6550; /* CLEAVER */
	player->tool_prio[4] = 13100; /* SCYTHE */
	player->tool_prio[5] = 26200; /* AXE */
	player->tool_prio[6] = 32750; /* SAW */
	player->tool_prio[7] = 45850; /* PICK */
	player->tool_prio[8] = 6550; /* PINCER */
}

/* Set defaults for flag priorities. */
void
player_reset_flag_priority(player_t *player)
{
	player->flag_prio[RESOURCE_GOLDORE] = 1;
	player->flag_prio[RESOURCE_GOLDBAR] = 2;
	player->flag_prio[RESOURCE_WHEAT] = 3;
	player->flag_prio[RESOURCE_FLOUR] = 4;
	player->flag_prio[RESOURCE_PIG] = 5;

	player->flag_prio[RESOURCE_BOAT] = 6;
	player->flag_prio[RESOURCE_PINCER] = 7;
	player->flag_prio[RESOURCE_SCYTHE] = 8;
	player->flag_prio[RESOURCE_ROD] = 9;
	player->flag_prio[RESOURCE_CLEAVER] = 10;

	player->flag_prio[RESOURCE_SAW] = 11;
	player->flag_prio[RESOURCE_AXE] = 12;
	player->flag_prio[RESOURCE_PICK] = 13;
	player->flag_prio[RESOURCE_SHOVEL] = 14;
	player->flag_prio[RESOURCE_HAMMER] = 15;

	player->flag_prio[RESOURCE_SHIELD] = 16;
	player->flag_prio[RESOURCE_SWORD] = 17;
	player->flag_prio[RESOURCE_BREAD] = 18;
	player->flag_prio[RESOURCE_MEAT] = 19;
	player->flag_prio[RESOURCE_FISH] = 20;

	player->flag_prio[RESOURCE_IRONORE] = 21;
	player->flag_prio[RESOURCE_LUMBER] = 22;
	player->flag_prio[RESOURCE_COAL] = 23;
	player->flag_prio[RESOURCE_STEEL] = 24;
	player->flag_prio[RESOURCE_STONE] = 25;
	player->flag_prio[RESOURCE_PLANK] = 26;
}

/* Set defaults for inventory priorities. */
void
player_reset_inventory_priority(player_t *player)
{
	player->inventory_prio[RESOURCE_WHEAT] = 1;
	player->inventory_prio[RESOURCE_FLOUR] = 2;
	player->inventory_prio[RESOURCE_PIG] = 3;
	player->inventory_prio[RESOURCE_BREAD] = 4;
	player->inventory_prio[RESOURCE_FISH] = 5;

	player->inventory_prio[RESOURCE_MEAT] = 6;
	player->inventory_prio[RESOURCE_LUMBER] = 7;
	player->inventory_prio[RESOURCE_PLANK] = 8;
	player->inventory_prio[RESOURCE_BOAT] = 9;
	player->inventory_prio[RESOURCE_STONE] = 10;

	player->inventory_prio[RESOURCE_COAL] = 11;
	player->inventory_prio[RESOURCE_IRONORE] = 12;
	player->inventory_prio[RESOURCE_STEEL] = 13;
	player->inventory_prio[RESOURCE_SHOVEL] = 14;
	player->inventory_prio[RESOURCE_HAMMER] = 15;

	player->inventory_prio[RESOURCE_ROD] = 16;
	player->inventory_prio[RESOURCE_CLEAVER] = 17;
	player->inventory_prio[RESOURCE_SCYTHE] = 18;
	player->inventory_prio[RESOURCE_AXE] = 19;
	player->inventory_prio[RESOURCE_SAW] = 20;

	player->inventory_prio[RESOURCE_PICK] = 21;
	player->inventory_prio[RESOURCE_PINCER] = 22;
	player->inventory_prio[RESOURCE_SHIELD] = 23;
	player->inventory_prio[RESOURCE_SWORD] = 24;
	player->inventory_prio[RESOURCE_GOLDORE] = 25;
	player->inventory_prio[RESOURCE_GOLDBAR] = 26;
}

void
player_change_knight_occupation(player_t *player, int index, int adjust_max, int delta)
{
	int max = (player->knight_occupation[index] >> 4) & 0xf;
	int min = player->knight_occupation[index] & 0xf;

	if (adjust_max) {
		max = clamp(min, max + delta, 4);
	} else {
		min = clamp(0, min + delta, max);
	}

	player->knight_occupation[index] = (max << 4) | min;
}

/* Turn a number of serfs into knight for the given player. */
int
player_promote_serfs_to_knights(player_t *player, int number)
{
	int promoted = 0;

	for (uint i = 1; i < game.max_serf_index && number > 0; i++) {
		if (SERF_ALLOCATED(i)) {
			serf_t *serf = game_get_serf(i);
			if (serf->state == SERF_STATE_IDLE_IN_STOCK &&
			    SERF_PLAYER(serf) == player->player_num &&
			    SERF_TYPE(serf) == SERF_GENERIC) {
				inventory_t *inv = game_get_inventory(serf->s.idle_in_stock.inv_index);
				if (inv->resources[RESOURCE_SWORD] > 0 &&
				    inv->resources[RESOURCE_SHIELD] > 0) {
					inv->resources[RESOURCE_SWORD] -= 1;
					inv->resources[RESOURCE_SHIELD] -= 1;
					inv->generic_count -= 1;
					inv->serfs[SERF_GENERIC] = 0;

					serf_set_type(serf, SERF_KNIGHT_0);

					promoted += 1;
					number -= 1;
				}
			}
		}
	}

	return promoted;
}

static int
available_knights_at_pos(player_t *player, map_pos_t pos, int index, int dist)
{
	const int min_level_hut[] = { 1, 1, 2, 2, 3 };
	const int min_level_tower[] = { 1, 2, 3, 4, 6 };
	const int min_level_fortress[] = { 1, 3, 6, 9, 12 };

	if (MAP_OWNER(pos) != player->player_num ||
	    MAP_TYPE_UP(pos) < 4 || MAP_TYPE_DOWN(pos) < 4 ||
	    MAP_OBJ(pos) < MAP_OBJ_SMALL_BUILDING ||
	    MAP_OBJ(pos) > MAP_OBJ_CASTLE) {
		return index;
	}

	int bld_index = MAP_OBJ_INDEX(pos);
	for (int i = 0; i < index; i++) {
		if (player->attacking_buildings[i] == bld_index) {
			return index;
		}
	}

	building_t *building = game_get_building(bld_index);
	if (!BUILDING_IS_DONE(building) ||
	    BUILDING_IS_BURNING(building)) {
		return index;
	}

	const int *min_level = NULL;
	switch (BUILDING_TYPE(building)) {
	case BUILDING_HUT: min_level = min_level_hut; break;
	case BUILDING_TOWER: min_level = min_level_tower; break;
	case BUILDING_FORTRESS: min_level = min_level_fortress; break;
	default: return index; break;
	}

	if (index >= 64) return index;

	player->attacking_buildings[index] = bld_index;

	int state = BUILDING_STATE(building);
	int knights_present = building->stock[0].available;
	int to_send = knights_present - min_level[player->knight_occupation[state] & 0xf];

	if (to_send > 0) player->attacking_knights[dist] += to_send;

	return index + 1;
}

int
player_knights_available_for_attack(player_t *player, map_pos_t pos)
{
	/* Reset counters. */
	for (int i = 0; i < 4; i++) player->attacking_knights[i] = 0;

	int index = 0;

	/* Iterate each shell around the position.*/
	for (int i = 0; i < 32; i++) {
		pos = MAP_MOVE_RIGHT(pos);
		for (int j = 0; j < i+1; j++) {
			index = available_knights_at_pos(player, pos, index, i >> 3);
			pos = MAP_MOVE_DOWN(pos);
		}
		for (int j = 0; j < i+1; j++) {
			index = available_knights_at_pos(player, pos, index, i >> 3);
			pos = MAP_MOVE_LEFT(pos);
		}
		for (int j = 0; j < i+1; j++) {
			index = available_knights_at_pos(player, pos, index, i >> 3);
			pos = MAP_MOVE_UP_LEFT(pos);
		}
		for (int j = 0; j < i+1; j++) {
			index = available_knights_at_pos(player, pos, index, i >> 3);
			pos = MAP_MOVE_UP(pos);
		}
		for (int j = 0; j < i+1; j++) {
			index = available_knights_at_pos(player, pos, index, i >> 3);
			pos = MAP_MOVE_RIGHT(pos);
		}
		for (int j = 0; j < i+1; j++) {
			index = available_knights_at_pos(player, pos, index, i >> 3);
			pos = MAP_MOVE_DOWN_RIGHT(pos);
		}
	}

	player->attacking_building_count = index;

	player->total_attacking_knights = 0;
	for (int i = 0; i < 4; i++) {
		player->total_attacking_knights +=
			player->attacking_knights[i];
	}

	return player->total_attacking_knights;
}

void
player_start_attack(player_t *player)
{
	const int min_level_hut[] = { 1, 1, 2, 2, 3 };
	const int min_level_tower[] = { 1, 2, 3, 4, 6 };
	const int min_level_fortress[] = { 1, 3, 6, 9, 12 };

	building_t *target = game_get_building(player->building_attacked);
	if (!BUILDING_IS_DONE(target) ||
	    (BUILDING_TYPE(target) != BUILDING_HUT &&
	     BUILDING_TYPE(target) != BUILDING_TOWER &&
	     BUILDING_TYPE(target) != BUILDING_FORTRESS &&
	     BUILDING_TYPE(target) != BUILDING_CASTLE) ||
	    !BUILDING_IS_ACTIVE(target) ||
	    BUILDING_STATE(target) != 3) {
		return;
	}

	for (int i = 0; i < player->attacking_building_count; i++) {
		/* TODO building index may not be valid any more(?). */
		building_t *b = game_get_building(player->attacking_buildings[i]);
		if (BUILDING_IS_BURNING(b) ||
		    MAP_OWNER(b->pos) != player->player_num) {
			continue;
		}

		map_pos_t flag_pos = MAP_MOVE_DOWN_RIGHT(b->pos);
		if (MAP_SERF_INDEX(flag_pos) != 0) {
			/* Check if building is under siege. */
			serf_t *s = game_get_serf(MAP_SERF_INDEX(flag_pos));
			if (SERF_PLAYER(s) != player->player_num) continue;
		}

		const int *min_level = NULL;
		switch (BUILDING_TYPE(b)) {
		case BUILDING_HUT: min_level = min_level_hut; break;
		case BUILDING_TOWER: min_level = min_level_tower; break;
		case BUILDING_FORTRESS: min_level = min_level_fortress; break;
		default: continue; break;
		}

		int state = BUILDING_STATE(b);
		int knights_present = b->stock[0].available;
		int to_send = knights_present - min_level[player->knight_occupation[state] & 0xf];

		for (int j = 0; j < to_send; j++) {
			/* Find most approriate knight to send according to player settings. */
			int best_type = PLAYER_SEND_STRONGEST(player) ? SERF_KNIGHT_0 : SERF_KNIGHT_4;
			int best_index = -1;

			int knight_index = b->serf_index;
			while (knight_index != 0) {
				serf_t *knight = game_get_serf(knight_index);
				if (PLAYER_SEND_STRONGEST(player)) {
					if (SERF_TYPE(knight) >= best_type) {
						best_index = knight_index;
						best_type = SERF_TYPE(knight);
					}
				} else {
					if (SERF_TYPE(knight) <= best_type) {
						best_index = knight_index;
						best_type = SERF_TYPE(knight);
					}
				}

				knight_index = knight->s.defending.next_knight;
			}

			/* Unlink knight from list. */
			int *def_index = &b->serf_index;
			serf_t *def_serf = game_get_serf(*def_index);
			while (*def_index != best_index) {
				def_index = &def_serf->s.defending.next_knight;
				def_serf = game_get_serf(*def_index);
			}
			*def_index = def_serf->s.defending.next_knight;
			b->stock[0].available -= 1;

			target->progress |= BIT(0);

			/* Calculate distance to target. */
			int dist_col = (MAP_POS_COL(target->pos) -
					MAP_POS_COL(def_serf->pos)) & game.map.col_mask;
			if (dist_col >= (int)(game.map.cols/2)) dist_col -= game.map.cols;

			int dist_row = (MAP_POS_ROW(target->pos) -
					MAP_POS_ROW(def_serf->pos)) & game.map.row_mask;
			if (dist_row >= (int)(game.map.rows/2)) dist_row -= game.map.rows;

			/* Send this serf off to fight. */
			serf_log_state_change(def_serf, SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT);
			def_serf->state = SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT;
			def_serf->s.leave_for_walk_to_fight.dist_col = dist_col;
			def_serf->s.leave_for_walk_to_fight.dist_row = dist_row;
			def_serf->s.leave_for_walk_to_fight.field_D = 0;
			def_serf->s.leave_for_walk_to_fight.field_E = 0;
			def_serf->s.leave_for_walk_to_fight.next_state = SERF_STATE_KNIGHT_FREE_WALKING;

			player->knights_attacking -= 1;
			if (player->knights_attacking == 0) return;
		}
	}
}

/* Begin cycling knights by sending knights from military buildings
   to inventories. The knights can then be replaced by more experienced
   knights. */
void
player_cycle_knights(player_t *player)
{
	player->flags |= BIT(2) | BIT(4);
	player->knight_cycle_counter = 2400;
}
