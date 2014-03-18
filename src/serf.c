/*
 * serf.c - Serf related functions
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


#include "serf.h"
#include "game.h"
#include "random.h"
#include "viewport.h"
#include "misc.h"
#include "debug.h"


static const int counter_from_animation[] = {
	/* Walking (0-80) */
	511, 447, 383, 319, 255, 319, 511, 767, 1023,
	511, 447, 383, 319, 255, 319, 511, 767, 1023,
	511, 447, 383, 319, 255, 319, 511, 767, 1023,
	511, 447, 383, 319, 255, 319, 511, 767, 1023,
	511, 447, 383, 319, 255, 319, 511, 767, 1023,
	511, 447, 383, 319, 255, 319, 511, 767, 1023,
	511, 447, 383, 319, 255, 319, 511, 767, 1023,
	511, 447, 383, 319, 255, 319, 511, 767, 1023,
	511, 447, 383, 319, 255, 319, 511, 767, 1023,

	/* Waiting (81-86) */
	127, 127, 127, 127, 127, 127,

	/* Digging (87-88) */
	383, 383,

	255, 223, 191, 159, 127, 159, 255, 383,	511,

	/* Building (98) */
	255,

	/* Engage defending free (99) */
	255,

	/* Building large building (100) */
	255,

	0,

	/* Building (102-105) */
	767, 511, 511, 767,

	1023, 639, 639, 1023,

	/* Transporting (turning?) (110-115) */
	63, 63, 63, 63, 63, 63,

	/* Logging (116-120) */
	1023, 31, 767, 767, 255,

	/* Planting (121-122) */
	191, 127,

	/* Stonecutting (123) */
	1535,

	/* Sawing (124) */
	2367,

	/* Mining (125-128) */
	383, 303, 303, 383,

	/* Smelting (129-130) */
	383, 383,

	/* Fishing (131-134) */
	767, 767, 127, 127,

	/* Farming (135-136) */
	1471, 1983,

	/* Milling (137) */
	383,

	/* Baking (138) */
	767,

	/* Pig farming (139) */
	383,

	/* Butchering (140) */
	1535,

	/* Sampling geology (142) */
	783, 63,

	/* Making weapon (143) */
	575,

	/* Making tool (144) */
	1535,

	/* Building boat (145-146) */
	1407, 159,

	/* Attacking (147-156) */
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127,

	/* Defending (157-166) */
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127,

	/* Engage attacking (167) */
	191,

	/* Victory attacking (168) */
	7,

	/* Dying attacking (169-173) */
	255, 255, 255, 255, 255,

	/* Dying defending (174-178) */
	255, 255, 255, 255, 255,

	/* Occupy attacking (179) */
	127,

	/* Victory defending (180) */
	7
};


static const char *serf_state_name[] = {
	[SERF_STATE_NULL] = "NULL",
	[SERF_STATE_IDLE_IN_STOCK] = "IDLE IN STOCK",
	[SERF_STATE_WALKING] = "WALKING",
	[SERF_STATE_TRANSPORTING] = "TRANSPORTING",
	[SERF_STATE_ENTERING_BUILDING] = "ENTERING BUILDING",
	[SERF_STATE_LEAVING_BUILDING] = "LEAVING BUILDING",
	[SERF_STATE_READY_TO_ENTER] = "READY TO ENTER",
	[SERF_STATE_READY_TO_LEAVE] = "READY TO LEAVE",
	[SERF_STATE_DIGGING] = "DIGGING",
	[SERF_STATE_BUILDING] = "BUILDING",
	[SERF_STATE_BUILDING_CASTLE] = "BUILDING CASTLE",
	[SERF_STATE_MOVE_RESOURCE_OUT] = "MOVE RESOURCE OUT",
	[SERF_STATE_WAIT_FOR_RESOURCE_OUT] = "WAIT FOR RESOURCE OUT",
	[SERF_STATE_DROP_RESOURCE_OUT] = "DROP RESOURCE OUT",
	[SERF_STATE_DELIVERING] = "DELIVERING",
	[SERF_STATE_READY_TO_LEAVE_INVENTORY] = "READY TO LEAVE INVENTORY",
	[SERF_STATE_FREE_WALKING] = "FREE WALKING",
	[SERF_STATE_LOGGING] = "LOGGING",
	[SERF_STATE_PLANNING_LOGGING] = "PLANNING LOGGING",
	[SERF_STATE_PLANNING_PLANTING] = "PLANNING PLANTING",
	[SERF_STATE_PLANTING] = "PLANTING",
	[SERF_STATE_PLANNING_STONECUTTING] = "PLANNING STONECUTTING",
	[SERF_STATE_STONECUTTER_FREE_WALKING] = "STONECUTTER FREE WALKING",
	[SERF_STATE_STONECUTTING] = "STONECUTTING",
	[SERF_STATE_SAWING] = "SAWING",
	[SERF_STATE_LOST] = "LOST",
	[SERF_STATE_LOST_SAILOR] = "LOST SAILOR",
	[SERF_STATE_FREE_SAILING] = "FREE SAILING",
	[SERF_STATE_ESCAPE_BUILDING] = "ESCAPE BUILDING",
	[SERF_STATE_MINING] = "MINING",
	[SERF_STATE_SMELTING] = "SMELTING",
	[SERF_STATE_PLANNING_FISHING] = "PLANNING FISHING",
	[SERF_STATE_FISHING] = "FISHING",
	[SERF_STATE_PLANNING_FARMING] = "PLANNING FARMING",
	[SERF_STATE_FARMING] = "FARMING",
	[SERF_STATE_MILLING] = "MILLING",
	[SERF_STATE_BAKING] = "BAKING",
	[SERF_STATE_PIGFARMING] = "PIGFARMING",
	[SERF_STATE_BUTCHERING] = "BUTCHERING",
	[SERF_STATE_MAKING_WEAPON] = "MAKING WEAPON",
	[SERF_STATE_MAKING_TOOL] = "MAKING TOOL",
	[SERF_STATE_BUILDING_BOAT] = "BUILDING BOAT",
	[SERF_STATE_LOOKING_FOR_GEO_SPOT] = "LOOKING FOR GEO SPOT",
	[SERF_STATE_SAMPLING_GEO_SPOT] = "SAMPLING GEO SPOT",
	[SERF_STATE_KNIGHT_ENGAGING_BUILDING] = "KNIGHT ENGAGING BUILDING",
	[SERF_STATE_KNIGHT_PREPARE_ATTACKING] = "KNIGHT PREPARE ATTACKING",
	[SERF_STATE_KNIGHT_LEAVE_FOR_FIGHT] = "KNIGHT LEAVE FOR FIGHT",
	[SERF_STATE_KNIGHT_PREPARE_DEFENDING] = "KNIGHT PREPARE DEFENDING",
	[SERF_STATE_KNIGHT_ATTACKING] = "KNIGHT ATTACKING",
	[SERF_STATE_KNIGHT_DEFENDING] = "KNIGHT DEFENDING",
	[SERF_STATE_KNIGHT_ATTACKING_VICTORY] = "KNIGHT ATTACKING VICTORY",
	[SERF_STATE_KNIGHT_ATTACKING_DEFEAT] = "KNIGHT ATTACKING DEFEAT",
	[SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING] = "KNIGHT OCCUPY ENEMY BUILDING",
	[SERF_STATE_KNIGHT_FREE_WALKING] = "KNIGHT FREE WALKING",
	[SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE] = "KNIGHT ENGAGE DEFENDING FREE",
	[SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE] = "KNIGHT ENGAGE ATTACKING FREE",
	[SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE_JOIN] = "KNIGHT ENGAGE ATTACKING FREE JOIN",
	[SERF_STATE_KNIGHT_PREPARE_ATTACKING_FREE] = "KNIGHT PREPARE ATTACKING FREE",
	[SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE] = "KNIGHT PREPARE DEFENDING FREE",
	[SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE_WAIT] = "KNIGHT PREPARE DEFENDING FREE WAIT",
	[SERF_STATE_KNIGHT_ATTACKING_FREE] = "KNIGHT ATTACKING FREE",
	[SERF_STATE_KNIGHT_DEFENDING_FREE] = "KNIGHT DEFENDING FREE",
	[SERF_STATE_KNIGHT_ATTACKING_VICTORY_FREE] = "KNIGHT ATTACKING VICTORY FREE",
	[SERF_STATE_KNIGHT_DEFENDING_VICTORY_FREE] = "KNIGHT DEFENDING VICTORY FREE",
	[SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE] = "KNIGHT ATTACKING DEFEAT FREE",
	[SERF_STATE_KNIGHT_ATTACKING_FREE_WAIT] = "KNIGHT ATTACKING FREE WAIT",
	[SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT] = "KNIGHT LEAVE FOR WALK TO FIGHT",
	[SERF_STATE_IDLE_ON_PATH] = "IDLE ON PATH",
	[SERF_STATE_WAIT_IDLE_ON_PATH] = "WAIT IDLE ON PATH",
	[SERF_STATE_WAKE_AT_FLAG] = "WAKE AT FLAG",
	[SERF_STATE_WAKE_ON_PATH] = "WAKE ON PATH",
	[SERF_STATE_DEFENDING_HUT] = "DEFENDING HUT",
	[SERF_STATE_DEFENDING_TOWER] = "DEFENDING TOWER",
	[SERF_STATE_DEFENDING_FORTRESS] = "DEFENDING FORTRESS",
	[SERF_STATE_SCATTER] = "SCATTER",
	[SERF_STATE_FINISHED_BUILDING] = "FINISHED BUILDING",
	[SERF_STATE_DEFENDING_CASTLE] = "DEFENDING CASTLE"
};


const char *
serf_get_state_name(serf_state_t state)
{
	return serf_state_name[state];
}

/* Change type of serf and update all global tables
   tracking serf types. */
void
serf_set_type(serf_t *serf, serf_type_t type)
{
	serf_type_t old_type = SERF_TYPE(serf);
	serf->type = (serf->type & 0x83) | (type << 2);

	/* Register this type as transporter */
	if (type == SERF_TRANSPORTER_INVENTORY) type = SERF_TRANSPORTER;
	if (old_type == SERF_TRANSPORTER_INVENTORY) type = SERF_TRANSPORTER;

	player_t *player = game.player[SERF_PLAYER(serf)];
	player->serf_count[old_type] -= 1;

	if (type != SERF_DEAD) {
		player->serf_count[type] += 1;
	}

	if (old_type >= SERF_KNIGHT_0 &&
	    old_type <= SERF_KNIGHT_4) {
		int value = 1 << (old_type - SERF_KNIGHT_0);
		player->total_military_score -= value;
	}
	if (type >= SERF_KNIGHT_0 &&
	    type <= SERF_KNIGHT_4) {
		int value = 1 << (type - SERF_KNIGHT_0);
		player->total_military_score += value;
	}
}

/* Change serf state to lost, but make necessary clean up
   from any earlier state first. */
void
serf_set_lost_state(serf_t *serf)
{
	if (serf->state == SERF_STATE_WALKING) {
		if (serf->s.walking.res >= 0) {
			if (serf->s.walking.res != 6) {
				dir_t dir = serf->s.walking.res;
				flag_t *flag = game_get_flag(serf->s.walking.dest);
				flag->length[dir] &= ~BIT(7);

				dir_t other_dir = FLAG_OTHER_END_DIR(flag, dir);
				flag->other_endpoint.f[dir]->length[other_dir] &= ~BIT(7);
			}
		} else if (serf->s.walking.res == -1) {
			flag_t *flag = game_get_flag(serf->s.walking.dest);
			building_t *building = flag->other_endpoint.b[DIR_UP_LEFT];

			if (BUILDING_SERF_REQUESTED(building)) {
				building->serf &= ~BIT(7);
			} else if (!BUILDING_HAS_INVENTORY(building)) {
				building->stock[0].requested -= 1;
			}
		}

		serf_log_state_change(serf, SERF_STATE_LOST);
		serf->state = SERF_STATE_LOST;
		serf->s.lost.field_B = 0;
	} else if (serf->state == SERF_STATE_TRANSPORTING ||
		   serf->state == SERF_STATE_DELIVERING) {
		if (serf->s.walking.res != 0) {
			int res = serf->s.walking.res-1;
			int dest = serf->s.walking.dest;

			game_cancel_transported_resource(res, dest);
			game_lose_resource(res);
		}

		if (SERF_TYPE(serf) != SERF_SAILOR) {
			serf_log_state_change(serf, SERF_STATE_LOST);
			serf->state = SERF_STATE_LOST;
			serf->s.lost.field_B = 0;
		} else {
			serf_log_state_change(serf, SERF_STATE_LOST_SAILOR);
			serf->state = SERF_STATE_LOST_SAILOR;
		}
	} else {
		serf_log_state_change(serf, SERF_STATE_LOST);
		serf->state = SERF_STATE_LOST;
		serf->s.lost.field_B = 0;
	}
}

/* Return true if serf is waiting for a position to be available.
   In this case, dir will be set to the desired direction of the serf,
   or DIR_NONE if the desired direction cannot be determined. */
static int
serf_is_waiting(serf_t *serf, dir_t *dir)
{
	const int dir_from_offset[] = {
		DIR_UP_LEFT, DIR_UP, -1,
		DIR_LEFT, -1, DIR_RIGHT,
		-1, DIR_DOWN, DIR_DOWN_RIGHT
	};

	if ((serf->state == SERF_STATE_TRANSPORTING ||
	     serf->state == SERF_STATE_WALKING ||
	     serf->state == SERF_STATE_DELIVERING) &&
	    serf->s.walking.dir < 0) {
		*dir = serf->s.walking.dir + 6;
		return 1;
	} else if ((serf->state == SERF_STATE_FREE_WALKING ||
		    serf->state == SERF_STATE_KNIGHT_FREE_WALKING ||
		    serf->state == SERF_STATE_STONECUTTER_FREE_WALKING) &&
		   serf->animation == 82) {
		int dx = serf->s.free_walking.dist1;
		int dy = serf->s.free_walking.dist2;

		if (abs(dx) <= 1 && abs(dy) <= 1 &&
		    dir_from_offset[(dx+1) + 3*(dy+1)] > -1) {
			*dir = dir_from_offset[(dx+1) + 3*(dy+1)];
		} else {
			*dir = DIR_NONE;
		}
		return 1;
	} else if (serf->state == SERF_STATE_DIGGING &&
		   serf->s.digging.substate < 0) {
		int d = serf->s.digging.dig_pos;
		*dir = (d == 0) ? DIR_UP : 6-d;
		return 1;
	}

	return 0;
}

/* Signal waiting serf that it is possible to move in direction
   while switching position with another serf. Returns 0 if the
   switch is not acceptable. */
static int
serf_switch_waiting(serf_t *serf, dir_t dir)
{
	if ((serf->state == SERF_STATE_TRANSPORTING ||
	     serf->state == SERF_STATE_WALKING ||
	     serf->state == SERF_STATE_DELIVERING) &&
	    serf->s.walking.dir < 0) {
		serf->s.walking.dir = DIR_REVERSE(dir);
		return 1;
	} else if ((serf->state == SERF_STATE_FREE_WALKING ||
		    serf->state == SERF_STATE_KNIGHT_FREE_WALKING ||
		    serf->state == SERF_STATE_STONECUTTER_FREE_WALKING) &&
		   serf->animation == 82) {
		int dx = ((dir < 3) ? 1 : -1)*((dir % 3) < 2);
		int dy = ((dir < 3) ? 1 : -1)*((dir % 3) > 0);

		serf->s.free_walking.dist1 -= dx;
		serf->s.free_walking.dist2 -= dy;

		if (serf->s.free_walking.dist1 == 0 &&
		    serf->s.free_walking.dist2 == 0) {
			/* Arriving to destination */
			serf->s.free_walking.flags = BIT(3);
		}
		return 1;
	} else if (serf->state == SERF_STATE_DIGGING &&
		   serf->s.digging.substate < 0) {
		return 0;
	}

	return 0;
}


static int
train_knight(serf_t *serf, int p)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		if (game_random_int() < p) {
			/* Level up */
			serf_type_t old_type = SERF_TYPE(serf);
			serf_set_type(serf, old_type+1);
			serf->counter = 6000;
			return 0;
		}
		serf->counter += 6000;
	}

	return -1;
}

static void
handle_serf_idle_in_stock_common(serf_t *serf, inventory_t *inventory)
{
	inventory->serfs[SERF_TYPE(serf)] = SERF_INDEX(serf);
}

static void
handle_knight_training_in_stock(serf_t *serf, inventory_t *inventory, int p)
{
	serf_type_t old_type = SERF_TYPE(serf);
	int r = train_knight(serf, p);
	if (r == 0) inventory->serfs[old_type] = 0;

	handle_serf_idle_in_stock_common(serf, inventory);
}

static void
handle_serf_idle_in_stock_state(serf_t *serf)
{
	inventory_t *inventory = game_get_inventory(serf->s.idle_in_stock.inv_index);
	int serf_mode = (inventory->res_dir >> 2) & 3;

	if (serf_mode == 0 || serf_mode == 1 ||  /* in, stop */
	    inventory->serfs_out >= 3) {
		switch (SERF_TYPE(serf)) {
			case SERF_KNIGHT_0:
				handle_knight_training_in_stock(serf, inventory, 4000);
				break;
			case SERF_KNIGHT_1:
				handle_knight_training_in_stock(serf, inventory, 2000);
				break;
			case SERF_KNIGHT_2:
				handle_knight_training_in_stock(serf, inventory, 1000);
				break;
			case SERF_KNIGHT_3:
				handle_knight_training_in_stock(serf, inventory, 500);
				break;
			case SERF_SMELTER: /* TODO ??? */
				break;
			default:
				handle_serf_idle_in_stock_common(serf, inventory);
				break;
		}
	} else { /* out */
		if (inventory->serfs[SERF_TYPE(serf)] == SERF_INDEX(serf)) {
			inventory->serfs[SERF_TYPE(serf)] = 0;
		}

		inventory->serfs_out += 1;

		serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE_INVENTORY);
		serf->state = SERF_STATE_READY_TO_LEAVE_INVENTORY;
		serf->s.ready_to_leave_inventory.mode = -3;
		serf->s.ready_to_leave_inventory.inv_index = INVENTORY_INDEX(inventory);
		/* TODO immediate switch to next state. */
	}
}

static int
get_walking_animation(int h_diff, dir_t dir, int switch_pos)
{
	int d = dir;
	if (switch_pos && d < 3) d += 6;
	return 4 + h_diff + 9*d;
}

/* Preconditon: serf is in WALKING or TRANSPORTING state */
static void
serf_change_direction(serf_t *serf, int dir, int alt_end)
{
	map_pos_t new_pos = MAP_MOVE(serf->pos, dir);

	if (MAP_SERF_INDEX(new_pos) == 0) {
		/* Change direction, not occupied. */
		map_set_serf_index(serf->pos, 0);
		serf->animation = get_walking_animation(MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos), dir, 0);
		serf->s.walking.dir = DIR_REVERSE(dir);
	} else {
		/* Direction is occupied. */
		serf_t *other_serf = game_get_serf(MAP_SERF_INDEX(new_pos));
		dir_t other_dir;

		if (serf_is_waiting(other_serf, &other_dir) &&
		    (other_dir == DIR_REVERSE(dir) || other_dir == DIR_NONE) &&
		    serf_switch_waiting(other_serf, DIR_REVERSE(dir))) {
			/* Do the switch */
			other_serf->pos = serf->pos;
			map_set_serf_index(other_serf->pos, SERF_INDEX(other_serf));
			other_serf->animation = get_walking_animation(MAP_HEIGHT(other_serf->pos) - MAP_HEIGHT(new_pos),
								      DIR_REVERSE(dir), 1);
			other_serf->counter = counter_from_animation[other_serf->animation];

			serf->animation = get_walking_animation(MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos), dir, 1);
			serf->s.walking.dir = DIR_REVERSE(dir);
		} else {
			/* Wait for other serf */
			serf->animation = 81 + dir;
			serf->counter = counter_from_animation[serf->animation];
			serf->s.walking.dir = dir-6;
			return;
		}
	}

	if (!alt_end) serf->s.walking.wait_counter = 0;
	serf->pos = new_pos;
	map_set_serf_index(serf->pos, SERF_INDEX(serf));
	serf->counter += counter_from_animation[serf->animation];
	if (alt_end && serf->counter < 0) {
		if (MAP_HAS_FLAG(new_pos)) serf->counter = 0;
		else LOGD("serf", "unhandled jump to 31B82.");
	}
}

static int
flag_search_inventory_search_cb(flag_t *flag, int *dest_index)
{
	if (FLAG_ACCEPTS_SERFS(flag)) {
		building_t *building = flag->other_endpoint.b[DIR_UP_LEFT];
		*dest_index = building->flag;
		return 1;
	}

	return 0;
}

static int
flag_search_inventory(int flag_index)
{
	flag_t *src = game_get_flag(flag_index);

	int dest_index = -1;
	flag_search_single(src, (flag_search_func *)flag_search_inventory_search_cb, 1, 0, &dest_index);

	return dest_index;
}

/* Precondition: serf state is in WALKING or TRANSPORTING state */
static void
serf_transporter_move_to_flag(serf_t *serf, flag_t *flag)
{
	int dir = serf->s.walking.dir;
	if (FLAG_IS_SCHEDULED(flag, dir)) {
		/* Fetch resource from flag */
		serf->s.walking.wait_counter = 0;
		int res_index = FLAG_SCHEDULED_SLOT(flag, dir);

		if (serf->s.walking.res == 0) {
			/* Pick up resource. */
			serf->s.walking.res = flag->slot[res_index].type+1;
			serf->s.walking.dest = flag->slot[res_index].dest;
			flag->slot[res_index].type = RESOURCE_NONE;
			flag->slot[res_index].dir = DIR_NONE;
		} else {
			/* Switch resources and destination. */
			flag->endpoint |= BIT(7);

			int res = serf->s.walking.res;
			serf->s.walking.res = flag->slot[res_index].type+1;
			flag->slot[res_index].type = res-1;
			flag->slot[res_index].dir = DIR_NONE;

			int dest = serf->s.walking.dest;
			serf->s.walking.dest = flag->slot[res_index].dest;
			flag->slot[res_index].dest = dest;
		}

		/* Find next resource to be picked up */
		player_t *player = game.player[SERF_PLAYER(serf)];
		flag_prioritize_pickup(flag, dir, player->flag_prio);
	} else if (serf->s.walking.res != 0) {
		/* Drop resource at flag */
		int free_slot = -1;
		for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
			if (flag->slot[i].type == RESOURCE_NONE) {
				free_slot = i;
				break;
			}
		}

		if (free_slot > -1) {
			flag->endpoint |= BIT(7);
			flag->slot[free_slot].type = serf->s.walking.res-1;
			flag->slot[free_slot].dest = serf->s.walking.dest;
			flag->slot[free_slot].dir = DIR_NONE;
			serf->s.walking.res = 0;
		}
	}

	serf_change_direction(serf, dir, 1);
}

static int
handle_serf_walking_state_search_cb(flag_t *flag, serf_t *serf)
{
	flag_t *dest = game_get_flag(serf->s.walking.dest);
	if (flag == dest) {
		LOGV("serf", " dest found: %i.", dest->search_dir);
		serf_change_direction(serf, dest->search_dir, 0);
		return 1;
	}

	return 0;
}

static void
serf_start_walking(serf_t *serf, dir_t dir, int slope, int change_pos)
{
	map_pos_t new_pos = MAP_MOVE(serf->pos, dir);
	serf->animation = get_walking_animation(MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos), dir, 0);
	serf->counter += (slope * counter_from_animation[serf->animation]) >> 5;

	if (change_pos) {
		map_set_serf_index(serf->pos, 0);
		map_set_serf_index(new_pos, SERF_INDEX(serf));
	}

	serf->pos = new_pos;
}

static const int road_building_slope[] = {
	/* Finished building */
	5, 18, 18, 15, 18, 22, 22, 22,
	22, 18, 16, 18, 1, 10, 1, 15,
	15, 16, 15, 15, 10, 15, 20, 15,
	18
};

/* Start entering building in direction up-left.
   If join_pos is set the serf is assumed to origin from
   a joined position so the source position will not have it's
   serf index cleared. */
static void
serf_enter_building(serf_t *serf, int field_B, int join_pos)
{
	serf_log_state_change(serf, SERF_STATE_ENTERING_BUILDING);
	serf->state = SERF_STATE_ENTERING_BUILDING;

	serf_start_walking(serf, DIR_UP_LEFT, 32, !join_pos);
	if (join_pos) map_set_serf_index(serf->pos, SERF_INDEX(serf));

	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
	int slope = road_building_slope[building->type];
	if (!BUILDING_IS_DONE(building)) slope = 1;
	serf->s.entering_building.slope_len = (slope * serf->counter) >> 5;
	serf->s.entering_building.field_B = field_B;
}

/* Start leaving building by switching to LEAVING BUILDING and
   setting appropriate state. */
static void
serf_leave_building(serf_t *serf, int join_pos)
{
	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
	int slope = 31 - road_building_slope[building->type];
	if (!BUILDING_IS_DONE(building)) slope = 30;

	if (join_pos) map_set_serf_index(serf->pos, 0);
	serf_start_walking(serf, DIR_DOWN_RIGHT, slope, !join_pos);

	serf_log_state_change(serf, SERF_STATE_LEAVING_BUILDING);
	serf->state = SERF_STATE_LEAVING_BUILDING;
}

static void
handle_serf_walking_state_dest_reached(serf_t *serf)
{
	/* Destination reached. */
	if (serf->s.walking.res < 0) {
		building_t *building = game_get_building(MAP_OBJ_INDEX(MAP_MOVE_UP_LEFT(serf->pos)));
		building->serf |= BIT(6);
		if (BUILDING_SERF_REQUESTED(building)) building->serf_index = SERF_INDEX(serf);
		building->serf &= ~BIT(7);

		if (MAP_SERF_INDEX(MAP_MOVE_UP_LEFT(serf->pos)) != 0) {
			serf->animation = 85;
			serf->counter = 0;
			serf_log_state_change(serf, SERF_STATE_READY_TO_ENTER);
			serf->state = SERF_STATE_READY_TO_ENTER;
		} else {
			serf_enter_building(serf, serf->s.walking.res, 0);
		}
	} else if (serf->s.walking.res == 6) {
		serf_log_state_change(serf, SERF_STATE_LOOKING_FOR_GEO_SPOT);
		serf->state = SERF_STATE_LOOKING_FOR_GEO_SPOT;
		serf->counter = 0;
	} else {
		flag_t *flag = game_get_flag(MAP_OBJ_INDEX(serf->pos));
		dir_t dir = serf->s.walking.res;
		flag_t *other_flag = flag->other_endpoint.f[dir];
		dir_t other_dir = FLAG_OTHER_END_DIR(flag, dir);

		/* Increment transport serf count */
		flag->length[dir] &= ~BIT(7);
		flag->length[dir] += 1;
		other_flag->length[other_dir] &= ~BIT(7);
		other_flag->length[other_dir] += 1;

		serf_log_state_change(serf, SERF_STATE_TRANSPORTING);
		serf->state = SERF_STATE_TRANSPORTING;
		serf->s.walking.dir = dir;
		serf->s.walking.res = 0;
		serf->s.walking.wait_counter = 0;

		serf_transporter_move_to_flag(serf, flag);
	}
}

static void
handle_serf_walking_state_waiting(serf_t *serf)
{
	/* Waiting for other serf. */
	dir_t dir = serf->s.walking.dir + 6;

	/* Only check for loops once in a while. */
	serf->s.walking.wait_counter += 1;
	if ((!MAP_HAS_FLAG(serf->pos) && serf->s.walking.wait_counter >= 10) ||
	    serf->s.walking.wait_counter >= 50) {
		map_pos_t pos = serf->pos;

		/* Follow the chain of serfs waiting for each other and
		   see if there is a loop. */
		for (int i = 0; i < 100; i++) {
			pos = MAP_MOVE(pos, dir);

			if (MAP_SERF_INDEX(pos) == 0) {
				break;
			} else if (MAP_SERF_INDEX(pos) == SERF_INDEX(serf)) {
				/* We have found a loop, try a different direction. */
				serf_change_direction(serf, DIR_REVERSE(dir), 0);
				return;
			}

			/* Get next serf and follow the chain */
			serf_t *other_serf = game_get_serf(MAP_SERF_INDEX(pos));
			if (other_serf->state != SERF_STATE_WALKING &&
			    other_serf->state != SERF_STATE_TRANSPORTING) {
				break;
			}

			if (other_serf->s.walking.dir >= 0 ||
			    (other_serf->s.walking.dir + 6) == DIR_REVERSE(dir)) {
				break;
			}

			dir = other_serf->s.walking.dir + 6;
		}
	}

	/* Stick to the same direction */
	serf->s.walking.wait_counter = 0;
	serf_change_direction(serf, serf->s.walking.dir + 6, 0);
}

static void
handle_serf_walking_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		if (serf->s.walking.dir < 0) {
			handle_serf_walking_state_waiting(serf);
			continue;
		}

		/* 301F0 */
		if (MAP_HAS_FLAG(serf->pos)) {
			/* Serf has reached a flag.
			   Search for a destination if none is known. */
			if (serf->s.walking.dest == 0) {
				int r = flag_search_inventory(MAP_OBJ_INDEX(serf->pos));
				if (r < 0) {
					serf_log_state_change(serf, SERF_STATE_LOST);
					serf->state = SERF_STATE_LOST;
					serf->s.lost.field_B = 1;
					serf->counter = 0;
					return;
				}
				serf->s.walking.dest = r;
			}

			/* Check whether destination has been reached.
			   If not, find out which direction to move in
			   to reach the destination. */
			if (serf->s.walking.dest == MAP_OBJ_INDEX(serf->pos)) {
				handle_serf_walking_state_dest_reached(serf);
				return;
			} else {
				flag_t *src = game_get_flag(MAP_OBJ_INDEX(serf->pos));
				flag_search_t search;
				flag_search_init(&search);
				for (int i = 0; i < 6; i++) {
					if (!FLAG_IS_WATER_PATH(src, 5-i)) {
						flag_t *other_flag = src->other_endpoint.f[5-i];
						other_flag->search_dir = 5-i;
						flag_search_add_source(&search, other_flag);
					}
				}
				int r = flag_search_execute(&search,
							    (flag_search_func *)handle_serf_walking_state_search_cb,
							    1, 0, serf);
				if (r == 0) continue;
			}
		} else {
			/* 30A37 */
			/* Serf is not at a flag. Just follow the road. */
			int paths = MAP_PATHS(serf->pos) & ~BIT(serf->s.walking.dir);
			int dir = -1;
			for (dir_t d = 0; d < 6; d++) {
				if (paths == BIT(d)) {
					dir = d;
					break;
				}
			}

			if (dir >= 0) {
				serf_change_direction(serf, dir, 0);
				continue;
			}

			serf->counter = 0;
		}

		/* Either the road is a dead end; or
		   we are at a flag, but the flag search for
		   the destination failed. */
		if (serf->s.walking.res < 0) {
			if (serf->s.walking.res < -1) {
				serf_log_state_change(serf, SERF_STATE_LOST);
				serf->state = SERF_STATE_LOST;
				serf->s.lost.field_B = 1;
				serf->counter = 0;
				return;
			}

			flag_t *flag = game_get_flag(serf->s.walking.dest);
			building_t *building = flag->other_endpoint.b[DIR_UP_LEFT];

			building->serf &= ~BIT(7);
			if (!BUILDING_HAS_INVENTORY(building)) building->stock[0].requested -= 1;
		} else if (serf->s.walking.res != 6) {
			flag_t *flag = game_get_flag(serf->s.walking.dest);
			dir_t d = serf->s.walking.res;
			flag->length[d] &= ~BIT(7);
			flag->other_endpoint.f[d]->length[FLAG_OTHER_END_DIR(flag, d)] &= ~BIT(7);
		}

		serf->s.walking.res = -2;
		serf->s.walking.dest = 0;
		serf->counter = 0;
	}
}

static void
handle_serf_transporting_state(serf_t *serf)
{
	map_tile_t *tiles = game.map.tiles;

	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	if (serf->counter >= 0) return;

	if (serf->s.walking.dir < 0) {
		serf_change_direction(serf, serf->s.walking.dir+6, 1);
	} else {
		/* 31549 */
		if (MAP_HAS_FLAG(serf->pos)) {
			/* Current position occupied by waiting transporter */
			if (serf->s.walking.wait_counter < 0) {
				serf_log_state_change(serf, SERF_STATE_WALKING);
				serf->state = SERF_STATE_WALKING;
				serf->s.walking.wait_counter = 0;
				serf->s.walking.res = -2;
				serf->s.walking.dest = 0;
				serf->counter = 0;
				return;
			}

			/* 31590 */
			if (serf->s.walking.res != 0 &&
			    MAP_OBJ_INDEX(serf->pos) == serf->s.walking.dest) {
				/* At resource destination */
				serf_log_state_change(serf, SERF_STATE_DELIVERING);
				serf->state = SERF_STATE_DELIVERING;
				serf->s.walking.wait_counter = 0;

				map_pos_t new_pos = MAP_MOVE_UP_LEFT(serf->pos);
				serf->animation = 3 + MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos) + (DIR_UP_LEFT+6)*9;
				serf->counter = counter_from_animation[serf->animation];
				/* TODO next call is actually into the middle of handle_serf_delivering_state().
				   Why is a nice and clean state switch not enough???
				   Just ignore this call and we'll be safe, I think... */
				/* handle_serf_delivering_state(serf); */
				return;
			}

			flag_t *flag = game_get_flag(MAP_OBJ_INDEX(serf->pos));
			serf_transporter_move_to_flag(serf, flag);
		} else {
			int paths = MAP_PATHS(serf->pos) & ~BIT(serf->s.walking.dir);
			int dir = -1;
			for (dir_t d = 0; d < 6; d++) {
				if (paths == BIT(d)) {
					dir = d;
					break;
				}
			}

			if (dir < 0) {
				serf_log_state_change(serf, SERF_STATE_LOST);
				serf->state = SERF_STATE_LOST;
				serf->counter = 0;
				return;
			}

			if (!MAP_HAS_FLAG(MAP_MOVE(serf->pos, dir)) ||
			    serf->s.walking.res != 0 ||
			    serf->s.walking.wait_counter < 0) {
				serf_change_direction(serf, dir, 1);
				return;
			}

			flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE(serf->pos, dir)));
			int rev_dir = DIR_REVERSE(dir);
			flag_t *other_flag = flag->other_endpoint.f[rev_dir];
			int other_dir = FLAG_OTHER_END_DIR(flag, rev_dir);

			if (FLAG_IS_SCHEDULED(flag, rev_dir)) {
				serf_change_direction(serf, dir, 1);
				return;
			}

			serf->animation = 110 + serf->s.walking.dir;
			serf->counter = counter_from_animation[serf->animation];
			serf->s.walking.dir -= 6;

			if (FLAG_TRANSPORTER_COUNT(flag, rev_dir) > 1) {
				serf->s.walking.wait_counter += 1;
				if (serf->s.walking.wait_counter > 3) {
					flag->length[rev_dir] -= 1;
					other_flag->length[other_dir] -= 1;
					serf->s.walking.wait_counter = -1;
				}
			} else {
				if (!FLAG_IS_SCHEDULED(other_flag, other_dir)) {
					/* TODO Don't use anim as state var */
					serf->tick = (serf->tick & 0xff00) | (serf->s.walking.dir & 0xff);
					serf_log_state_change(serf, SERF_STATE_IDLE_ON_PATH);
					serf->state = SERF_STATE_IDLE_ON_PATH;
					serf->s.idle_on_path.rev_dir = rev_dir;
					serf->s.idle_on_path.flag = flag;
					tiles[serf->pos].obj |= BIT(7);
					map_set_serf_index(serf->pos, 0);
					return;
				}
			}
		}
	}
}

static void
serf_enter_inventory(serf_t *serf)
{
	map_set_serf_index(serf->pos, 0);
	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
	serf_log_state_change(serf, SERF_STATE_IDLE_IN_STOCK);
	serf->state = SERF_STATE_IDLE_IN_STOCK;
	/*serf->s.idle_in_stock.field_B = 0;
	  serf->s.idle_in_stock.field_C = 0;*/
	serf->s.idle_in_stock.inv_index = INVENTORY_INDEX(building->u.inventory);
}

static void
handle_serf_entering_building_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	if (serf->counter < 0 || serf->counter <= serf->s.entering_building.slope_len) {
		if (MAP_OBJ_INDEX(serf->pos) == 0 ||
		    BIT_TEST(game_get_building(MAP_OBJ_INDEX(serf->pos))->serf, 5)) { /* Burning */
			serf_log_state_change(serf, SERF_STATE_LOST);
			serf->state = SERF_STATE_LOST;
			serf->s.lost.field_B = 0;
			serf->counter = 0;
			return;
		}

		serf->counter = serf->s.entering_building.slope_len;
		switch (SERF_TYPE(serf)) {
		case SERF_TRANSPORTER:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				map_set_serf_index(serf->pos, 0);
				int flag_index = MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos));
				flag_t *flag = game_get_flag(flag_index);

				/* Mark as inventory accepting resources and serfs. */
				flag->bld_flags = BIT(7) | BIT(6);
				flag->bld2_flags = BIT(7);

				serf_log_state_change(serf, SERF_STATE_WAIT_FOR_RESOURCE_OUT);
				serf->state = SERF_STATE_WAIT_FOR_RESOURCE_OUT;
				serf->counter = 63;
				serf_set_type(serf, SERF_TRANSPORTER_INVENTORY);
			}
			break;
		case SERF_SAILOR:
			serf_enter_inventory(serf);
			break;
		case SERF_DIGGER:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				serf_log_state_change(serf, SERF_STATE_DIGGING);
				serf->state = SERF_STATE_DIGGING;
				serf->s.digging.h_index = 15;

				building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
				serf->s.digging.dig_pos = 6;
				serf->s.digging.target_h = building->u.level;
				serf->s.digging.substate = 1;
			}
			break;
		case SERF_BUILDER:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				serf_log_state_change(serf, SERF_STATE_BUILDING);
				serf->state = SERF_STATE_BUILDING;
				serf->animation = 98;
				serf->counter = 127;
				serf->s.building.mode = 1;
				serf->s.building.bld_index = MAP_OBJ_INDEX(serf->pos);
				serf->s.building.material_step = 0;

				building_t *building = game_get_building(serf->s.building.bld_index);
				switch (BUILDING_TYPE(building)) {
				case BUILDING_STOCK:
				case BUILDING_SAWMILL:
				case BUILDING_TOOLMAKER:
				case BUILDING_FORTRESS:
					serf->s.building.material_step |= BIT(7);
					serf->animation = 100;
					break;
				default:
					break;
				}
			}
			break;
		case SERF_TRANSPORTER_INVENTORY:
			map_set_serf_index(serf->pos, 0);
			serf_log_state_change(serf, SERF_STATE_WAIT_FOR_RESOURCE_OUT);
			serf->state = SERF_STATE_WAIT_FOR_RESOURCE_OUT;
			serf->counter = 63;
			break;
		case SERF_LUMBERJACK:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				map_set_serf_index(serf->pos, 0);
				serf_log_state_change(serf, SERF_STATE_PLANNING_LOGGING);
				serf->state = SERF_STATE_PLANNING_LOGGING;
			}
			break;
		case SERF_SAWMILLER:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				map_set_serf_index(serf->pos, 0);
				if (serf->s.entering_building.field_B != 0) {
					building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
					int flag_index = MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos));
					flag_t *flag = game_get_flag(flag_index);
					flag->bld_flags = 0;
					flag->bld2_flags = 0;
					building->stock[1].type = RESOURCE_LUMBER;
					building->stock[1].prio = 0;
					building->stock[1].maximum = 8;
				}
				serf_log_state_change(serf, SERF_STATE_SAWING);
				serf->state = SERF_STATE_SAWING;
				serf->s.sawing.mode = 0;
			}
			break;
		case SERF_STONECUTTER:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				map_set_serf_index(serf->pos, 0);
				serf_log_state_change(serf, SERF_STATE_PLANNING_STONECUTTING);
				serf->state = SERF_STATE_PLANNING_STONECUTTING;
			}
			break;
		case SERF_FORESTER:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				map_set_serf_index(serf->pos, 0);
				serf_log_state_change(serf, SERF_STATE_PLANNING_PLANTING);
				serf->state = SERF_STATE_PLANNING_PLANTING;
			}
			break;
		case SERF_MINER:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				map_set_serf_index(serf->pos, 0);
				building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
				building_type_t bld_type = BUILDING_TYPE(building);

				if (serf->s.entering_building.field_B != 0) {
					building->serf |= BIT(4);
					building->serf &= ~BIT(3);

					flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
					flag->bld_flags = 0;
					flag->bld2_flags = 0;
					building->stock[0].type = RESOURCE_GROUP_FOOD;
					building->stock[0].prio = 0;
					building->stock[0].maximum = 8;
				}

				serf_log_state_change(serf, SERF_STATE_MINING);
				serf->state = SERF_STATE_MINING;
				serf->s.mining.substate = 0;
				serf->s.mining.deposit = 4 - (bld_type - BUILDING_STONEMINE);
				/*serf->s.mining.field_C = 0;*/
				serf->s.mining.res = 0;
			}
			break;
		case SERF_SMELTER:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				map_set_serf_index(serf->pos, 0);

				building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));

				if (serf->s.entering_building.field_B != 0) {
					flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
					flag->bld_flags = 0;
					flag->bld2_flags = 0;
					building->stock[0].type = RESOURCE_COAL;
					building->stock[0].prio = 0;
					building->stock[0].maximum = 8;

					if (BUILDING_TYPE(building) == BUILDING_STEELSMELTER) {
						building->stock[1].type = RESOURCE_IRONORE;
					} else {
						building->stock[1].type = RESOURCE_GOLDORE;
					}
					building->stock[1].prio = 0;
					building->stock[1].maximum = 8;
				}

				/* Switch to smelting state to begin work. */
				serf_log_state_change(serf, SERF_STATE_SMELTING);
				serf->state = SERF_STATE_SMELTING;

				if (BUILDING_TYPE(building) == BUILDING_STEELSMELTER) {
					serf->s.smelting.type = 0;
				} else {
					serf->s.smelting.type = -1;
				}

				serf->s.smelting.mode = 0;
			}
			break;
		case SERF_FISHER:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				map_set_serf_index(serf->pos, 0);
				serf_log_state_change(serf, SERF_STATE_PLANNING_FISHING);
				serf->state = SERF_STATE_PLANNING_FISHING;
			}
			break;
		case SERF_PIGFARMER:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				map_set_serf_index(serf->pos, 0);

				if (serf->s.entering_building.field_B != 0) {
					building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
					flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));

					building->stock[1].available = 1;

					flag->bld_flags = 0;
					flag->bld2_flags = 0;
					building->stock[0].type = RESOURCE_WHEAT;
					building->stock[0].prio = 0;
					building->stock[0].maximum = 8;

					serf_log_state_change(serf, SERF_STATE_PIGFARMING);
					serf->state = SERF_STATE_PIGFARMING;
					serf->s.pigfarming.mode = 0;
				} else {
					serf_log_state_change(serf, SERF_STATE_PIGFARMING);
					serf->state = SERF_STATE_PIGFARMING;
					serf->s.pigfarming.mode = 6;
					serf->counter = 0;
				}
			}
			break;
		case SERF_BUTCHER:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				map_set_serf_index(serf->pos, 0);

				if (serf->s.entering_building.field_B != 0) {
					building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
					flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
					flag->bld_flags = 0;
					flag->bld2_flags = 0;
					building->stock[0].type = RESOURCE_PIG;
					building->stock[0].prio = 0;
					building->stock[0].maximum = 8;
				}

				serf_log_state_change(serf, SERF_STATE_BUTCHERING);
				serf->state = SERF_STATE_BUTCHERING;
				serf->s.butchering.mode = 0;
			}
			break;
		case SERF_FARMER:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				map_set_serf_index(serf->pos, 0);
				serf_log_state_change(serf, SERF_STATE_PLANNING_FARMING);
				serf->state = SERF_STATE_PLANNING_FARMING;
			}
			break;
		case SERF_MILLER:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				map_set_serf_index(serf->pos, 0);

				if (serf->s.entering_building.field_B != 0) {
					building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
					flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
					flag->bld_flags = 0;
					flag->bld2_flags = 0;
					building->stock[0].type = RESOURCE_WHEAT;
					building->stock[0].prio = 0;
					building->stock[0].maximum = 8;
				}

				serf_log_state_change(serf, SERF_STATE_MILLING);
				serf->state = SERF_STATE_MILLING;
				serf->s.milling.mode = 0;
			}
			break;
		case SERF_BAKER:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				map_set_serf_index(serf->pos, 0);

				if (serf->s.entering_building.field_B != 0) {
					building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
					flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
					flag->bld_flags = 0;
					flag->bld2_flags = 0;
					building->stock[0].type = RESOURCE_FLOUR;
					building->stock[0].prio = 0;
					building->stock[0].maximum = 8;
				}

				serf_log_state_change(serf, SERF_STATE_BAKING);
				serf->state = SERF_STATE_BAKING;
				serf->s.baking.mode = 0;
			}
			break;
		case SERF_BOATBUILDER:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				map_set_serf_index(serf->pos, 0);
				if (serf->s.entering_building.field_B != 0) {
					building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
					flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
					flag->bld_flags = 0;
					flag->bld2_flags = 0;
					building->stock[0].type = RESOURCE_PLANK;
					building->stock[0].prio = 0;
					building->stock[0].maximum = 8;
				}

				serf_log_state_change(serf, SERF_STATE_BUILDING_BOAT);
				serf->state = SERF_STATE_BUILDING_BOAT;
				serf->s.building_boat.mode = 0;
			}
			break;
		case SERF_TOOLMAKER:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				map_set_serf_index(serf->pos, 0);
				if (serf->s.entering_building.field_B != 0) {
					building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
					flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
					flag->bld_flags = 0;
					flag->bld2_flags = 0;
					building->stock[0].type = RESOURCE_PLANK;
					building->stock[0].prio = 0;
					building->stock[0].maximum = 8;
					building->stock[1].type = RESOURCE_STEEL;
					building->stock[1].prio = 0;
					building->stock[1].maximum = 8;
				}

				serf_log_state_change(serf, SERF_STATE_MAKING_TOOL);
				serf->state = SERF_STATE_MAKING_TOOL;
				serf->s.making_tool.mode = 0;
			}
			break;
		case SERF_WEAPONSMITH:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				map_set_serf_index(serf->pos, 0);
				if (serf->s.entering_building.field_B != 0) {
					building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
					flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
					flag->bld_flags = 0;
					flag->bld2_flags = 0;
					building->stock[0].type = RESOURCE_COAL;
					building->stock[0].prio = 0;
					building->stock[0].maximum = 8;
					building->stock[1].type = RESOURCE_STEEL;
					building->stock[1].prio = 0;
					building->stock[1].maximum = 8;
				}

				serf_log_state_change(serf, SERF_STATE_MAKING_WEAPON);
				serf->state = SERF_STATE_MAKING_WEAPON;
				serf->s.making_weapon.mode = 0;
			}
			break;
		case SERF_GEOLOGIST:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				serf_log_state_change(serf, SERF_STATE_LOOKING_FOR_GEO_SPOT);
				serf->state = SERF_STATE_LOOKING_FOR_GEO_SPOT; /* TODO Should never be reached */
				serf->counter = 0;
			}
			break;
		case SERF_GENERIC:
			map_set_serf_index(serf->pos, 0);

			building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
			inventory_t *inventory = building->u.inventory;
			inventory->generic_count += 1;

			serf_log_state_change(serf, SERF_STATE_IDLE_IN_STOCK);
			serf->state = SERF_STATE_IDLE_IN_STOCK;
			/*serf->s.idle_in_stock.field_B = 0;
			  serf->s.idle_in_stock.field_C = 0;*/
			serf->s.idle_in_stock.inv_index = INVENTORY_INDEX(inventory);
			break;
		case SERF_KNIGHT_0:
		case SERF_KNIGHT_1:
		case SERF_KNIGHT_2:
		case SERF_KNIGHT_3:
		case SERF_KNIGHT_4:
			if (serf->s.entering_building.field_B == -2) {
				serf_enter_inventory(serf);
			} else {
				building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
				if (BUILDING_IS_BURNING(building)) {
					serf_log_state_change(serf, SERF_STATE_LOST);
					serf->state = SERF_STATE_LOST;
					serf->counter = 0;
				} else {
					map_set_serf_index(serf->pos, 0);

					if (BUILDING_HAS_INVENTORY(building)) {
						serf_log_state_change(serf, SERF_STATE_DEFENDING_CASTLE);
						serf->state = SERF_STATE_DEFENDING_CASTLE;
						serf->counter = 6000;

						/* Prepend to knight list */
						serf->s.defending.next_knight = building->serf_index;
						building->serf_index = SERF_INDEX(serf);

						game.player[BUILDING_PLAYER(building)]->castle_knights += 1;
						return;
					}

					building->stock[0].available += 1;
					building->stock[0].requested -= 1;

					serf_state_t next_state = -1;
					switch (BUILDING_TYPE(building)) {
					case BUILDING_HUT: next_state = SERF_STATE_DEFENDING_HUT; break;
					case BUILDING_TOWER: next_state = SERF_STATE_DEFENDING_TOWER; break;
					case BUILDING_FORTRESS: next_state = SERF_STATE_DEFENDING_FORTRESS; break;
					default: NOT_REACHED(); break;
					}

					/* Switch to defending state */
					serf_log_state_change(serf, next_state);
					serf->state = next_state;
					serf->counter = 6000;

					/* Prepend to knight list */
					serf->s.defending.next_knight = building->serf_index;
					building->serf_index = SERF_INDEX(serf);

					/* Test whether building is already occupied by knights */
					if (!BUILDING_IS_ACTIVE(building)) {
						building->serf |= BIT(4);

						int mil_type = -1;
						int max_gold = -1;
						switch (BUILDING_TYPE(building)) {
						case BUILDING_HUT:
							mil_type = 0;
							max_gold = 2;
							break;
						case BUILDING_TOWER:
							mil_type = 1;
							max_gold = 4;
							break;
						case BUILDING_FORTRESS:
							mil_type = 2;
							max_gold = 8;
							break;
						default:
							NOT_REACHED();
							break;
						}

						player_add_notification(game.player[BUILDING_PLAYER(building)],
									(mil_type << 5) | 6, building->pos);

						flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(building->pos)));
						flag->bld_flags = 0;
						flag->bld2_flags = 0;
						building->stock[1].type = RESOURCE_GOLDBAR;
						building->stock[1].prio = 0;
						building->stock[1].maximum = max_gold;

						/* Save amount of land and buildings for each player */
						uint land_before[GAME_MAX_PLAYER_COUNT] = {0};
						uint buildings_before[GAME_MAX_PLAYER_COUNT] = {0};
						for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
							player_t *player = game.player[i];
							if (!PLAYER_IS_ACTIVE(game.player[i])) continue;

							land_before[i] = player->total_land_area;
							buildings_before[i] = player->total_building_score;
						}

						/* Update land ownership */
						game_update_land_ownership(building->pos);

						/* Create notfications for lost land and buildings */
						for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
							player_t *player = game.player[i];
							if (!PLAYER_IS_ACTIVE(game.player[i])) continue;

							if (buildings_before[i] > player->total_building_score) {
								player_add_notification(player, (BUILDING_PLAYER(building) << 5) | 9,
											building->pos);
							} else if (land_before[i] > player->total_land_area) {
								player_add_notification(player, (BUILDING_PLAYER(building) << 5) | 8,
											building->pos);
							}
						}
					}
				}
			}
			break;
		default:
			NOT_REACHED();
			break;
		}
	}
}

static void
handle_serf_leaving_building_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	if (serf->counter < 0) {
		serf->counter = 0;
		serf_log_state_change(serf, serf->s.leaving_building.next_state);
		serf->state = serf->s.leaving_building.next_state;

		/* Set field_F to 0, do this for individual states if necessary */
		if (serf->state == SERF_STATE_WALKING) {
			int mode = serf->s.leaving_building.field_B;
			uint dest = serf->s.leaving_building.dest;
			serf->s.walking.res = mode;
			serf->s.walking.dest = dest;
			serf->s.walking.wait_counter = 0;
		} else if (serf->state == SERF_STATE_DROP_RESOURCE_OUT) {
			uint res = serf->s.leaving_building.field_B;
			uint res_dest = serf->s.leaving_building.dest;
			serf->s.move_resource_out.res = res;
			serf->s.move_resource_out.res_dest = res_dest;
		} else if (serf->state == SERF_STATE_FREE_WALKING ||
			   serf->state == SERF_STATE_KNIGHT_FREE_WALKING ||
			   serf->state == SERF_STATE_STONECUTTER_FREE_WALKING) {
			int dist1 = serf->s.leaving_building.field_B;
			int dist2 = serf->s.leaving_building.dest;
			int neg_dist1 = serf->s.leaving_building.dest2;
			int neg_dist2 = serf->s.leaving_building.dir;
			serf->s.free_walking.dist1 = dist1;
			serf->s.free_walking.dist2 = dist2;
			serf->s.free_walking.neg_dist1 = neg_dist1;
			serf->s.free_walking.neg_dist2 = neg_dist2;
			serf->s.free_walking.flags = 0;
		} else if (serf->state == SERF_STATE_KNIGHT_PREPARE_DEFENDING ||
			   serf->state == SERF_STATE_SCATTER) {
			/* No state. */
		} else {
			LOGD("serf", "unhandled next state when leaving building.");
		}
	}
}

static void
handle_serf_ready_to_enter_state(serf_t *serf)
{
	map_pos_t new_pos = MAP_MOVE_UP_LEFT(serf->pos);
	
	if (MAP_SERF_INDEX(new_pos) != 0) {
		serf->animation = 85;
		serf->counter = 0;
		return;
	}

	serf_enter_building(serf, serf->s.ready_to_enter.field_B, 0);
}

static void
handle_serf_ready_to_leave_state(serf_t *serf)
{
	serf->tick = game.tick;
	serf->counter = 0;

	map_pos_t new_pos = MAP_MOVE_DOWN_RIGHT(serf->pos);

	if ((MAP_SERF_INDEX(serf->pos) != SERF_INDEX(serf) &&
	     MAP_SERF_INDEX(serf->pos) != 0) ||
	    MAP_SERF_INDEX(new_pos) != 0) {
		serf->animation = 82;
		serf->counter = 0;
		return;
	}

	serf_leave_building(serf, 0);
}

static void
handle_serf_digging_state(serf_t *serf)
{
	const int h_diff[] = {
		-1, 1, -2, 2, -3, 3, -4, 4,
		-5, 5, -6, 6, -7, 7, -8, 8
	};

	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		serf->s.digging.substate -= 1;
		if (serf->s.digging.substate < 0) {
			LOGV("serf", "substate -1: wait for serf.");
			int d = serf->s.digging.dig_pos;
			dir_t dir = (d == 0) ? DIR_UP : 6-d;
			map_pos_t new_pos = MAP_MOVE(serf->pos, dir);

			if (MAP_SERF_INDEX(new_pos) != 0) {
				serf_t *other_serf = game_get_serf(MAP_SERF_INDEX(new_pos));
				dir_t other_dir;

				if (serf_is_waiting(other_serf, &other_dir) &&
				    other_dir == DIR_REVERSE(dir) &&
				    serf_switch_waiting(other_serf, other_dir)) {
					/* Do the switch */
					other_serf->pos = serf->pos;
					map_set_serf_index(other_serf->pos, SERF_INDEX(other_serf));
					other_serf->animation = get_walking_animation(MAP_HEIGHT(other_serf->pos) - MAP_HEIGHT(new_pos),
										      DIR_REVERSE(dir), 1);
					other_serf->counter = counter_from_animation[other_serf->animation];

					if (d != 0) {
						serf->animation = get_walking_animation(MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos), dir, 1);
					} else {
						serf->animation = MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos);
					}
				} else {
					serf->counter = 127;
					serf->s.digging.substate = 0;
					return;
				}
			} else {
				map_set_serf_index(serf->pos, 0);
				if (d != 0) {
					serf->animation = get_walking_animation(MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos), dir, 0);
				} else {
					serf->animation = MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos);
				}
			}

			map_set_serf_index(new_pos, SERF_INDEX(serf));
			serf->pos = new_pos;
			serf->s.digging.substate = 3;
			serf->counter += counter_from_animation[serf->animation];
		} else if (serf->s.digging.substate == 1) {
			/* 34CD6: Change height, head back to center */
			int h = MAP_HEIGHT(serf->pos);
			h += (serf->s.digging.h_index & 1) ? -1 : 1;
			LOGV("serf", "substate 1: change height %s.", (serf->s.digging.h_index & 1) ? "down" : "up");
			map_set_height(serf->pos, h);

			if (serf->s.digging.dig_pos == 0) {
				serf->s.digging.substate = 1;
			} else {
				dir_t dir = DIR_REVERSE(6-serf->s.digging.dig_pos);
				serf_start_walking(serf, dir, 32, 1);
			}
		} else if (serf->s.digging.substate > 1) {
			LOGV("serf", "substate 2: dig.");
			/* 34E89 */
			serf->animation = 88 - (serf->s.digging.h_index & 1);
			serf->counter += 383;
		} else {
			/* 34CDC: Looking for a place to dig */
			LOGV("serf", "substate 0: looking for place to dig %i, %i.",
			       serf->s.digging.dig_pos, serf->s.digging.h_index);
			do {
				int h = h_diff[serf->s.digging.h_index] + serf->s.digging.target_h;
				if (serf->s.digging.dig_pos >= 0 && h >= 0 && h < 32) {
					if (serf->s.digging.dig_pos == 0) {
						if (MAP_HEIGHT(serf->pos) != h) {
							serf->s.digging.dig_pos -= 1;
							continue;
						}
						/* Dig here */
						serf->s.digging.substate = 2;
						if (serf->s.digging.h_index & 1) serf->animation = 87;
						else serf->animation = 88;
						serf->counter += 383;
					} else {
						dir_t dir = 6-serf->s.digging.dig_pos;
						map_pos_t new_pos = MAP_MOVE(serf->pos, dir);
						if (MAP_HEIGHT(new_pos) != h) {
							serf->s.digging.dig_pos -= 1;
							continue;
						}
						LOGV("serf", "  found at: %i.", serf->s.digging.dig_pos);
						/* Digging spot found */
						if (MAP_SERF_INDEX(new_pos) != 0) {
							/* Occupied by other serf, wait */
							serf->s.digging.substate = 0;
							serf->animation = 87 - serf->s.digging.dig_pos;
							serf->counter = counter_from_animation[serf->animation];
							return;
						}

						/* Go to dig there */
						serf_start_walking(serf, dir, 32, 1);
						serf->s.digging.substate = 3;
					}
					break;
				}

				serf->s.digging.dig_pos = 6;
				serf->s.digging.h_index -= 1;
			} while (serf->s.digging.h_index >= 0);

			if (serf->s.digging.h_index < 0) {
				/* Done digging */
				building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
				building->progress = 1;
				building->serf &= ~BIT(6);
				building->serf_index = 0;
				serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE);
				serf->state = SERF_STATE_READY_TO_LEAVE;
				serf->s.leaving_building.dest = 0;
				serf->s.leaving_building.field_B = -2;
				serf->s.leaving_building.dir = 0;
				serf->s.leaving_building.next_state = SERF_STATE_WALKING;
				/* handle_serf_ready_to_leave_state(serf); */ /* TODO why isn't a state switch enough? */
				return;
			}
		}
	}
}

static void
handle_serf_building_state(serf_t *serf)
{
	const int material_order[] = {
		0, 0, 0, 0, 0, 4, 0, 0,
		0, 0, 0x38, 2, 8, 2, 8, 4,
		4, 0xc, 0x14, 0x2c, 2, 0x1c, 0x1f0, 4,
		0, 0, 0, 0, 0, 0, 0, 0
	};

	const int construction_progress[] = {
		0, 0, 4096, 4096, 4096, 4096, 4096, 2048,
		4096, 4096, 2048, 1366, 2048, 1366, 2048, 1366,
		2048, 1366, 4096, 4096, 1366, 1024, 4096, 4096,
		2048, 1366, 4096, 2048, 2048, 1366, 2048, 2048,
		4096, 2048, 2048, 1366, 2048, 1366, 2048, 1024,
		4096, 2048, 2048, 1366, 1024, 683, 2048, 1366
	};

	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		building_t *building = game_get_building(serf->s.building.bld_index);
		if (serf->s.building.mode < 0) {
			building_type_t type = BUILDING_TYPE(building);
			int frame_finished = !!BIT_TEST(building->progress, 15);
			building->progress += construction_progress[2*type+frame_finished];

			if (building->progress > 0xffff) {
				building->progress = 0;
				building->serf &= ~BIT(6);
				building->serf_index = 0;
				building->bld &= ~BIT(7); /* Building finished */

				switch (type) {
				case BUILDING_HUT:
				case BUILDING_TOWER:
				case BUILDING_FORTRESS:
					game_calculate_military_flag_state(building);
					break;
				default:
					break;
				}

				flag_t *flag = game_get_flag(building->flag);

				building->stock[0].type = RESOURCE_NONE;
				building->stock[1].type = RESOURCE_NONE;
				flag->bld_flags = 0;
				flag->bld2_flags = 0;

				/* Update player fields. */
				player_t *player = game.player[SERF_PLAYER(serf)];
				player->total_building_score += building_get_score_from_type(type);
				player->completed_building_count[type] += 1;
				player->incomplete_building_count[type] -= 1;

				serf->counter = 0;

				serf_log_state_change(serf, SERF_STATE_FINISHED_BUILDING);
				serf->state = SERF_STATE_FINISHED_BUILDING;
				return;
			}

			serf->s.building.counter -= 1;
			if (serf->s.building.counter == 0) {
				serf->s.building.mode = 1;
				serf->animation = 98;
				if (BIT_TEST(serf->s.building.material_step, 7)) serf->animation = 100;

				/* 353A5 */
				int material_step = serf->s.building.material_step & 0xf;
				if (!BIT_TEST(material_order[BUILDING_TYPE(building)], material_step)) {
					/* Planks */
					if (building->stock[0].available == 0) {
						serf->counter += 256;
						if (serf->counter < 0) serf->counter = 255;
						return;
					}

					building->stock[0].available -= 1;
					building->stock[0].maximum -= 1;
				} else {
					/* Stone */
					if (building->stock[1].available == 0) {
						serf->counter += 256;
						if (serf->counter < 0) serf->counter = 255;
						return;
					}

					building->stock[1].available -= 1;
					building->stock[1].maximum -= 1;
				}

				serf->s.building.material_step += 1;
				serf->s.building.counter = 8;
				serf->s.building.mode = -1;
			}
		} else {
			if (serf->s.building.mode == 0) {
				serf->s.building.mode = 1;
				serf->animation = 98;
				if (BIT_TEST(serf->s.building.material_step, 7)) serf->animation = 100;
			}

			/* 353A5: Duplicate code */
			int material_step = serf->s.building.material_step & 0xf;
			if (!BIT_TEST(material_order[BUILDING_TYPE(building)], material_step)) {
				/* Planks */
				if (building->stock[0].available == 0) {
					serf->counter += 256;
					if (serf->counter < 0) serf->counter = 255;
					return;
				}

				building->stock[0].available -= 1;
				building->stock[0].maximum -= 1;
			} else {
				/* Stone */
				if (building->stock[1].available == 0) {
					serf->counter += 256;
					if (serf->counter < 0) serf->counter = 255;
					return;
				}

				building->stock[1].available -= 1;
				building->stock[1].maximum -= 1;
			}

			serf->s.building.material_step += 1;
			serf->s.building.counter = 8;
			serf->s.building.mode = -1;
		}

		int rnd = (game_random_int() & 3) + 102;
		if (BIT_TEST(serf->s.building.material_step, 7)) rnd += 4;
		serf->animation = rnd;
		serf->counter += counter_from_animation[serf->animation];
	}
}

static void
handle_serf_building_castle_state(serf_t *serf)
{
	int progress_delta = (uint16_t)(game.tick - serf->tick) << 7;
	serf->tick = game.tick;

	inventory_t *inventory = game_get_inventory(serf->s.building_castle.inv_index);
	building_t *building = game_get_building(inventory->building);
	building->progress += progress_delta;

	if (building->progress >= 0x10000) { /* Finished */
		serf_log_state_change(serf, SERF_STATE_WAIT_FOR_RESOURCE_OUT);
		serf->state = SERF_STATE_WAIT_FOR_RESOURCE_OUT;
		map_set_serf_index(serf->pos, 0);
		building->bld &= ~BIT(7); /* Building finished */
		building->serf_index = 0;
	}
}

static void
handle_serf_move_resource_out_state(serf_t *serf)
{
	serf->tick = game.tick;
	serf->counter = 0;

	if ((MAP_SERF_INDEX(serf->pos) != SERF_INDEX(serf) &&
	     MAP_SERF_INDEX(serf->pos) != 0) ||
	    MAP_SERF_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)) != 0) {
		/* Occupied by serf, wait */
		serf->animation = 82;
		serf->counter = 0;
		return;
	}

	flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
	if (flag->slot[0].type != RESOURCE_NONE && flag->slot[1].type != RESOURCE_NONE &&
	    flag->slot[2].type != RESOURCE_NONE && flag->slot[3].type != RESOURCE_NONE &&
	    flag->slot[4].type != RESOURCE_NONE && flag->slot[5].type != RESOURCE_NONE &&
	    flag->slot[6].type != RESOURCE_NONE && flag->slot[7].type != RESOURCE_NONE) {
		/* All resource slots at flag are occupied, wait */
		serf->animation = 82;
		serf->counter = 0;
		return;
	}

	uint res = serf->s.move_resource_out.res;
	uint res_dest = serf->s.move_resource_out.res_dest;
	serf_state_t next_state = serf->s.move_resource_out.next_state;

	serf_leave_building(serf, 0);
	serf->s.leaving_building.next_state = next_state;
	serf->s.leaving_building.field_B = res;
	serf->s.leaving_building.dest = res_dest;
}

static void
handle_serf_wait_for_resource_out_state(serf_t *serf)
{
	if (serf->counter != 0) {
		uint16_t delta = game.tick - serf->tick;
		serf->tick = game.tick;
		serf->counter -= delta;

		if (serf->counter >= 0) return;

		serf->counter = 0;
	}

	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
	inventory_t *inventory = building->u.inventory;
	if (inventory->serfs_out > 0 ||
	    inventory->out_queue[0].type == RESOURCE_NONE) {
		return;
	}

	serf_log_state_change(serf, SERF_STATE_MOVE_RESOURCE_OUT);
	serf->state = SERF_STATE_MOVE_RESOURCE_OUT;
	serf->s.move_resource_out.res = inventory->out_queue[0].type + 1;
	serf->s.move_resource_out.res_dest = inventory->out_queue[0].dest;
	serf->s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

	inventory->out_queue[0].type = inventory->out_queue[1].type;
	inventory->out_queue[1].type = RESOURCE_NONE;
	inventory->out_queue[0].dest = inventory->out_queue[1].dest;

	/*handle_serf_move_resource_out_state(serf);*//* why isn't a state switch enough? */
}

static void
handle_serf_drop_resource_out_state(serf_t *serf)
{
	flag_t *flag = game_get_flag(MAP_OBJ_INDEX(serf->pos));
	int i = -1;
	for (i = 0; i < FLAG_MAX_RES_COUNT; i++) {
		/* Guaranteed to find a free slot because
		   the map position has been reserved since
		   a free position was found. */
		if (flag->slot[i].type == RESOURCE_NONE) break;
	}

	assert(i >= 0);

	flag->slot[i].type = serf->s.move_resource_out.res-1;
	flag->slot[i].dest = serf->s.move_resource_out.res_dest;
	flag->slot[i].dir = DIR_NONE;
	flag->endpoint |= BIT(7); /* Resources waiting */

	serf_log_state_change(serf, SERF_STATE_READY_TO_ENTER);
	serf->state = SERF_STATE_READY_TO_ENTER;
	serf->s.ready_to_enter.field_B = 0;
}

static void
handle_serf_delivering_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		if (serf->s.walking.wait_counter != 0) {
			serf_log_state_change(serf, SERF_STATE_TRANSPORTING);
			serf->state = SERF_STATE_TRANSPORTING;
			serf->s.walking.wait_counter = 0;
			flag_t *flag = game_get_flag(MAP_OBJ_INDEX(serf->pos));
			serf_transporter_move_to_flag(serf, flag);
			return;
		}

		if (serf->s.walking.res != 0) {
			int res = serf->s.walking.res - 1; /* Offset by one, because 0 means none. */
			serf->s.walking.res = 0;
			building_t *building = game_get_building(MAP_OBJ_INDEX(MAP_MOVE_UP_LEFT(serf->pos)));
			if (!BUILDING_IS_BURNING(building)) {
				if (BUILDING_HAS_INVENTORY(building)) {
					inventory_t *inventory = building->u.inventory;
					inventory->resources[res] = min(inventory->resources[res]+1, 50000);
				} else {
					if (res == RESOURCE_FISH ||
					    res == RESOURCE_MEAT ||
					    res == RESOURCE_BREAD) {
						res = RESOURCE_GROUP_FOOD;
					}

					/* Add to building stock */
					int stock = -1;
					for (int i = 0; i < BUILDING_MAX_STOCK; i++) {
						if (building->stock[i].type == res) {
							stock = i;
							break;
						}
					}

					assert(stock >= 0);
					building->stock[stock].available += 1;
					building->stock[stock].requested -= 1;
					assert(building->stock[stock].requested >= 0);
				}
			}
		}

		serf->animation = 4 + 9 - (serf->animation - (3 + 10*9));
		serf->s.walking.wait_counter = -serf->s.walking.wait_counter - 1;
		serf->counter += counter_from_animation[serf->animation] >> 1;
	}
}

static void
handle_serf_ready_to_leave_inventory_state(serf_t *serf)
{
	serf->tick = game.tick;
	serf->counter = 0;

	if (MAP_SERF_INDEX(serf->pos) != 0 ||
	    MAP_SERF_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)) != 0) {
		serf->animation = 82;
		serf->counter = 0;
		return;
	}

	if (serf->s.ready_to_leave_inventory.mode == -1) {
		flag_t *flag = game_get_flag(serf->s.ready_to_leave_inventory.dest);
		if (FLAG_HAS_BUILDING(flag)) {
			building_t *building = flag->other_endpoint.b[DIR_UP_LEFT];
			if (MAP_SERF_INDEX(building->pos) != 0) {
				serf->animation = 82;
				serf->counter = 0;
				return;
			}
		}
	}

	inventory_t *inventory = game_get_inventory(serf->s.ready_to_leave_inventory.inv_index);
	inventory->serfs_out -= 1;

	serf_state_t next_state = SERF_STATE_WALKING;
	if (serf->s.ready_to_leave_inventory.mode == -3) {
		next_state = SERF_STATE_SCATTER;
	}

	int mode = serf->s.ready_to_leave_inventory.mode;
	uint dest = serf->s.ready_to_leave_inventory.dest;

	serf_leave_building(serf, 0);
	serf->s.leaving_building.next_state = next_state;
	serf->s.leaving_building.field_B = mode;
	serf->s.leaving_building.dest = dest;
	serf->s.leaving_building.dir = 0;
}

static void
serf_drop_resource(serf_t *serf, resource_type_t res)
{
	flag_t *flag = game_get_flag(MAP_OBJ_INDEX(serf->pos));

	int slot = -1;
	for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
		if (flag->slot[i].type == RESOURCE_NONE) {
			slot = i;
			break;
		}
	}

	/* Resource is lost if no free slot is found */
	if (slot >= 0) {
		flag->slot[slot].type = res;
		flag->slot[slot].dest = 0;
		flag->slot[slot].dir = DIR_NONE;
		flag->endpoint |= BIT(7);

		player_t *player = game.player[SERF_PLAYER(serf)];
		player->resource_count[res] += 1;
	}
}

/* Serf will try to find the closest inventory from current position, either
   by following the roads if it is already at a flag, otherwise it will try
   to find a flag nearby. */
static void
serf_find_inventory(serf_t *serf)
{
	if (MAP_HAS_FLAG(serf->pos)) {
		flag_t *flag = game_get_flag(MAP_OBJ_INDEX(serf->pos));
		if ((FLAG_LAND_PATHS(flag) != 0 ||
		     (FLAG_HAS_INVENTORY(flag) && FLAG_ACCEPTS_SERFS(flag))) &&
		     MAP_OWNER(serf->pos) == SERF_PLAYER(serf)) {
			serf_log_state_change(serf, SERF_STATE_WALKING);
			serf->state = SERF_STATE_WALKING;
			serf->s.walking.res = -2;
			serf->s.walking.dest = 0;
			serf->s.walking.dir = 0;
			serf->counter = 0;
			return;
		}
	}

	serf_log_state_change(serf, SERF_STATE_LOST);
	serf->state = SERF_STATE_LOST;
	serf->s.lost.field_B = 0;
	serf->counter = 0;
}

static void
handle_serf_free_walking_state_dest_reached(serf_t *serf)
{
	if (serf->s.free_walking.neg_dist1 == -128 &&
	    serf->s.free_walking.neg_dist2 < 0) {
		serf_find_inventory(serf);
		return;
	}

	switch (SERF_TYPE(serf)) {
	case SERF_LUMBERJACK:
		if (serf->s.free_walking.neg_dist1 == -128) {
			if (serf->s.free_walking.neg_dist2 > 0) {
				serf_drop_resource(serf, RESOURCE_LUMBER);
			}

			serf_log_state_change(serf, SERF_STATE_READY_TO_ENTER);
			serf->state = SERF_STATE_READY_TO_ENTER;
			serf->s.ready_to_enter.field_B = 0;
			serf->counter = 0;
		} else {
			serf->s.free_walking.dist1 = serf->s.free_walking.neg_dist1;
			serf->s.free_walking.dist2 = serf->s.free_walking.neg_dist2;
			int obj = MAP_OBJ(serf->pos);
			if (obj >= MAP_OBJ_TREE_0 &&
			    obj <= MAP_OBJ_PINE_7) {
				serf_log_state_change(serf, SERF_STATE_LOGGING);
				serf->state = SERF_STATE_LOGGING;
				serf->s.free_walking.neg_dist1 = 0;
				serf->s.free_walking.neg_dist2 = 0;
				if (obj < 16) serf->s.free_walking.neg_dist1 = -1;
				serf->animation = 116;
				serf->counter = counter_from_animation[serf->animation];
			} else {
				/* The expected tree is gone */
				serf->s.free_walking.neg_dist1 = -128;
				serf->s.free_walking.neg_dist2 = 0;
				serf->s.free_walking.flags = 0;
				serf->counter = 0;
			}
		}
		break;
	case SERF_STONECUTTER:
		if (serf->s.free_walking.neg_dist1 == -128) {
			if (serf->s.free_walking.neg_dist2 > 0) {
				serf_drop_resource(serf, RESOURCE_STONE);
			}

			serf_log_state_change(serf, SERF_STATE_READY_TO_ENTER);
			serf->state = SERF_STATE_READY_TO_ENTER;
			serf->s.ready_to_enter.field_B = 0;
			serf->counter = 0;
		} else {
			serf->s.free_walking.dist1 = serf->s.free_walking.neg_dist1;
			serf->s.free_walking.dist2 = serf->s.free_walking.neg_dist2;

			map_pos_t new_pos = MAP_MOVE_UP_LEFT(serf->pos);
			int obj = MAP_OBJ(new_pos);
			if (MAP_SERF_INDEX(new_pos) == 0 &&
			    obj >= MAP_OBJ_STONE_0 &&
			    obj <= MAP_OBJ_STONE_7) {
				serf->counter = 0;
				serf_start_walking(serf, DIR_UP_LEFT, 32, 1);

				serf_log_state_change(serf, SERF_STATE_STONECUTTING);
				serf->state = SERF_STATE_STONECUTTING;
				serf->s.free_walking.neg_dist2 = serf->counter >> 2;
				serf->s.free_walking.neg_dist1 = 0;
			} else {
				/* The expected stone is gone or unavailable */
				serf->s.free_walking.neg_dist1 = -128;
				serf->s.free_walking.neg_dist2 = 0;
				serf->s.free_walking.flags = 0;
				serf->counter = 0;
			}
		}
		break;
	case SERF_FORESTER:
		if (serf->s.free_walking.neg_dist1 == -128) {
			serf_log_state_change(serf, SERF_STATE_READY_TO_ENTER);
			serf->state = SERF_STATE_READY_TO_ENTER;
			serf->s.ready_to_enter.field_B = 0;
			serf->counter = 0;
		} else {
			serf->s.free_walking.dist1 = serf->s.free_walking.neg_dist1;
			serf->s.free_walking.dist2 = serf->s.free_walking.neg_dist2;
			if (MAP_OBJ(serf->pos) == MAP_OBJ_NONE) {
				serf_log_state_change(serf, SERF_STATE_PLANTING);
				serf->state = SERF_STATE_PLANTING;
				serf->s.free_walking.neg_dist2 = 0;
				serf->animation = 121;
				serf->counter = counter_from_animation[serf->animation];
			} else {
				/* The expected free space is no longer empty */
				serf->s.free_walking.neg_dist1 = -128;
				serf->s.free_walking.neg_dist2 = 0;
				serf->s.free_walking.flags = 0;
				serf->counter = 0;
			}
		}
		break;
	case SERF_FISHER:
		if (serf->s.free_walking.neg_dist1 == -128) {
			if (serf->s.free_walking.neg_dist2 > 0) {
				serf_drop_resource(serf, RESOURCE_FISH);
			}

			serf_log_state_change(serf, SERF_STATE_READY_TO_ENTER);
			serf->state = SERF_STATE_READY_TO_ENTER;
			serf->s.ready_to_enter.field_B = 0;
			serf->counter = 0;
		} else {
			serf->s.free_walking.dist1 = serf->s.free_walking.neg_dist1;
			serf->s.free_walking.dist2 = serf->s.free_walking.neg_dist2;

			int a = -1;
			if (MAP_PATHS(serf->pos) == 0) {
				if ((MAP_TYPE_DOWN(serf->pos) & 0xc) == 0 &&
				    (MAP_TYPE_UP(MAP_MOVE_UP_LEFT(serf->pos)) & 0xc) != 0) {
					a = 132;
				} else if ((MAP_TYPE_DOWN(MAP_MOVE_LEFT(serf->pos)) & 0xc) == 0 &&
					   (MAP_TYPE_UP(MAP_MOVE_UP(serf->pos)) & 0xc) != 0) {
					a = 131;
				}
			}

			if (a < 0) {
				/* Cannot fish here after all. */
				serf->s.free_walking.neg_dist1 = -128;
				serf->s.free_walking.neg_dist2 = 0;
				serf->s.free_walking.flags = 0;
				serf->counter = 0;
			} else {
				serf_log_state_change(serf, SERF_STATE_FISHING);
				serf->state = SERF_STATE_FISHING;
				serf->s.free_walking.neg_dist1 = 0;
				serf->s.free_walking.neg_dist2 = 0;
				serf->s.free_walking.flags = 0;
				serf->animation = a;
				serf->counter = counter_from_animation[a];
			}
		}
		break;
	case SERF_FARMER:
		if (serf->s.free_walking.neg_dist1 == -128) {
			if (serf->s.free_walking.neg_dist2 > 0) {
				serf_drop_resource(serf, RESOURCE_WHEAT);
			}

			serf_log_state_change(serf, SERF_STATE_READY_TO_ENTER);
			serf->state = SERF_STATE_READY_TO_ENTER;
			serf->s.ready_to_enter.field_B = 0;
			serf->counter = 0;
		} else {
			serf->s.free_walking.dist1 = serf->s.free_walking.neg_dist1;
			serf->s.free_walking.dist2 = serf->s.free_walking.neg_dist2;

			if (MAP_OBJ(serf->pos) == MAP_OBJ_SEEDS_5 ||
			    (MAP_OBJ(serf->pos) >= MAP_OBJ_FIELD_0 &&
			     MAP_OBJ(serf->pos) <= MAP_OBJ_FIELD_5)) {
				/* Existing field. */
				serf->animation = 136;
				serf->s.free_walking.neg_dist1 = 1;
				serf->counter = counter_from_animation[serf->animation];
			} else if (MAP_OBJ(serf->pos) == MAP_OBJ_NONE &&
				   MAP_PATHS(serf->pos) == 0) {
				/* Empty space. */
				serf->animation = 135;
				serf->s.free_walking.neg_dist1 = 0;
				serf->counter = counter_from_animation[serf->animation];
			} else {
				/* Space not available after all. */
				serf->s.free_walking.neg_dist1 = -128;
				serf->s.free_walking.neg_dist2 = 0;
				serf->s.free_walking.flags = 0;
				serf->counter = 0;
				break;
			}

			serf_log_state_change(serf, SERF_STATE_FARMING);
			serf->state = SERF_STATE_FARMING;
			serf->s.free_walking.neg_dist2 = 0;
		}
		break;
	case SERF_GEOLOGIST:
		if (serf->s.free_walking.neg_dist1 == -128) {
			if (MAP_OBJ(serf->pos) == MAP_OBJ_FLAG &&
			    MAP_OWNER(serf->pos) == SERF_PLAYER(serf)) {
				serf_log_state_change(serf, SERF_STATE_LOOKING_FOR_GEO_SPOT);
				serf->state = SERF_STATE_LOOKING_FOR_GEO_SPOT;
				serf->counter = 0;
			} else {
				serf_log_state_change(serf, SERF_STATE_LOST);
				serf->state = SERF_STATE_LOST;
				serf->s.lost.field_B = 0;
				serf->counter = 0;
			}
		} else {
			serf->s.free_walking.dist1 = serf->s.free_walking.neg_dist1;
			serf->s.free_walking.dist2 = serf->s.free_walking.neg_dist2;
			if (MAP_OBJ(serf->pos) == MAP_OBJ_NONE) {
				serf_log_state_change(serf, SERF_STATE_SAMPLING_GEO_SPOT);
				serf->state = SERF_STATE_SAMPLING_GEO_SPOT;
				serf->s.free_walking.neg_dist1 = 0;
				serf->animation = 141;
				serf->counter = counter_from_animation[serf->animation];
			} else {
				/* Destination is not a free space after all. */
				serf->s.free_walking.neg_dist1 = -128;
				serf->s.free_walking.neg_dist2 = 0;
				serf->s.free_walking.flags = 0;
				serf->counter = 0;
			}
		}
		break;
	case SERF_KNIGHT_0:
	case SERF_KNIGHT_1:
	case SERF_KNIGHT_2:
	case SERF_KNIGHT_3:
	case SERF_KNIGHT_4:
		if (serf->s.free_walking.neg_dist1 == -128) {
			serf_find_inventory(serf);
		} else {
			serf_log_state_change(serf, SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING);
			serf->state = SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING;
			serf->counter = 0;
		}
		break;
	default:
		serf_find_inventory(serf);
		break;
	}
}

static void
handle_serf_free_walking_switch_on_dir(serf_t *serf, int dir)
{
	/* A suitable direction has been found; walk. */
	assert(dir > -1);
	int dx = ((dir < 3) ? 1 : -1)*((dir % 3) < 2);
	int dy = ((dir < 3) ? 1 : -1)*((dir % 3) > 0);

	LOGV("serf", "serf %i: free walking: dest %i, %i, move %i, %i.",
	     SERF_INDEX(serf),
	     serf->s.free_walking.dist1,
	     serf->s.free_walking.dist2, dx, dy);

	serf->s.free_walking.dist1 -= dx;
	serf->s.free_walking.dist2 -= dy;

	serf_start_walking(serf, dir, 32, 1);

	if (serf->s.free_walking.dist1 == 0 &&
	    serf->s.free_walking.dist2 == 0) {
		/* Arriving to destination */
		serf->s.free_walking.flags = BIT(3);
	}
}

static void
handle_serf_free_walking_switch_with_other(serf_t *serf)
{
	/* No free position can be found. Switch with
	   other serf. */
	map_pos_t new_pos = 0;
	int dir = -1;
	serf_t *other_serf = NULL;
	for (int i = 0; i < 6; i++) {
		new_pos = MAP_MOVE(serf->pos, i);
		if (MAP_SERF_INDEX(new_pos) != 0) {
			other_serf = game_get_serf(MAP_SERF_INDEX(new_pos));
			dir_t other_dir;

			if (serf_is_waiting(other_serf, &other_dir) &&
			    other_dir == DIR_REVERSE(i) &&
			    serf_switch_waiting(other_serf, other_dir)) {
				dir = i;
				break;
			}
		}
	}

	if (dir > -1) {
		int dx = ((dir < 3) ? 1 : -1)*((dir % 3) < 2);
		int dy = ((dir < 3) ? 1 : -1)*((dir % 3) > 0);

		LOGV("serf", "free walking (switch): dest %i, %i, move %i, %i.",
		     serf->s.free_walking.dist1,
		     serf->s.free_walking.dist2, dx, dy);

		serf->s.free_walking.dist1 -= dx;
		serf->s.free_walking.dist2 -= dy;

		if (serf->s.free_walking.dist1 == 0 &&
		    serf->s.free_walking.dist2 == 0) {
			/* Arriving to destination */
			serf->s.free_walking.flags = BIT(3);
		}

		/* Switch with other serf. */
		map_set_serf_index(serf->pos, SERF_INDEX(other_serf));
		map_set_serf_index(new_pos, SERF_INDEX(serf));

		other_serf->animation = get_walking_animation(MAP_HEIGHT(serf->pos) - MAP_HEIGHT(other_serf->pos),
							      DIR_REVERSE(dir), 1);
		serf->animation = get_walking_animation(MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos), dir, 1);

		other_serf->counter = counter_from_animation[other_serf->animation];
		serf->counter = counter_from_animation[serf->animation];

		other_serf->pos = serf->pos;
		serf->pos = new_pos;
	} else {
		serf->animation = 82;
		serf->counter = counter_from_animation[serf->animation];
	}
}

static int
serf_can_pass_map_pos(map_pos_t pos)
{
	return map_space_from_obj[MAP_OBJ(pos)] <= MAP_SPACE_SEMIPASSABLE;
}

static int
handle_free_walking_follow_edge(serf_t *serf)
{
	const int dir_from_offset[] = {
		DIR_UP_LEFT, DIR_UP, -1,
		DIR_LEFT, -1, DIR_RIGHT,
		-1, DIR_DOWN, DIR_DOWN_RIGHT
	};

	/* Follow right-hand edge */
	const dir_t dir_right_edge[] = {
		DIR_DOWN, DIR_DOWN_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP_LEFT, DIR_LEFT,
		DIR_LEFT, DIR_DOWN, DIR_DOWN_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP_LEFT,
		DIR_UP_LEFT, DIR_LEFT, DIR_DOWN, DIR_DOWN_RIGHT, DIR_RIGHT, DIR_UP,
		DIR_UP, DIR_UP_LEFT, DIR_LEFT, DIR_DOWN, DIR_DOWN_RIGHT, DIR_RIGHT,
		DIR_RIGHT, DIR_UP, DIR_UP_LEFT, DIR_LEFT, DIR_DOWN, DIR_DOWN_RIGHT,
		DIR_DOWN_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP_LEFT, DIR_LEFT, DIR_DOWN,
	};

	/* Follow left-hand edge */
	const dir_t dir_left_edge[] = {
		DIR_UP_LEFT, DIR_UP, DIR_RIGHT, DIR_DOWN_RIGHT, DIR_DOWN, DIR_LEFT,
		DIR_UP, DIR_RIGHT, DIR_DOWN_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP_LEFT,
		DIR_RIGHT, DIR_DOWN_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP_LEFT, DIR_UP,
		DIR_DOWN_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP_LEFT, DIR_UP, DIR_RIGHT,
		DIR_DOWN, DIR_LEFT, DIR_UP_LEFT, DIR_UP, DIR_RIGHT, DIR_DOWN_RIGHT,
		DIR_LEFT, DIR_UP_LEFT, DIR_UP, DIR_RIGHT, DIR_DOWN_RIGHT, DIR_DOWN,
	};

	int water = (serf->state == SERF_STATE_FREE_SAILING);
	int dir_index = -1;
	const int *dir_arr = NULL;

	if (BIT_TEST(serf->s.free_walking.flags, 3)) {
		/* Follow right-hand edge */
		dir_arr = dir_left_edge;
		dir_index = (serf->s.free_walking.flags & 7)-1;
	} else {
		/* Follow right-hand edge */
		dir_arr = dir_right_edge;
		dir_index = (serf->s.free_walking.flags & 7)-1;
	}

	int d1 = serf->s.free_walking.dist1;
	int d2 = serf->s.free_walking.dist2;

	/* Check if dest is only one step away. */
	if (!water && abs(d1) <= 1 && abs(d2) <= 1 &&
	    dir_from_offset[(d1+1) + 3*(d2+1)] > -1) {
		/* Convert offset in two dimensions to
		   direction variable. */
		dir_t dir = dir_from_offset[(d1+1) + 3*(d2+1)];
		map_pos_t new_pos = MAP_MOVE(serf->pos, dir);

		if (!serf_can_pass_map_pos(new_pos)) {
			if (serf->state != SERF_STATE_KNIGHT_FREE_WALKING &&
			    serf->s.free_walking.neg_dist1 != -128) {
				serf->s.free_walking.dist1 += serf->s.free_walking.neg_dist1;
				serf->s.free_walking.dist2 += serf->s.free_walking.neg_dist2;
				serf->s.free_walking.neg_dist1 = 0;
				serf->s.free_walking.neg_dist2 = 0;
				serf->s.free_walking.flags = 0;
				serf->animation = 82;
				serf->counter = counter_from_animation[serf->animation];
			} else {
				serf_log_state_change(serf, SERF_STATE_LOST);
				serf->state = SERF_STATE_LOST;
				serf->s.lost.field_B = 0;
				serf->counter = 0;
			}
			return 0;
		}

		if (serf->state == SERF_STATE_KNIGHT_FREE_WALKING &&
		    serf->s.free_walking.neg_dist1 != -128 &&
		    MAP_SERF_INDEX(new_pos) != 0) {
			/* Wait for other serfs */
			serf->s.free_walking.flags = 0;
			serf->animation = 82;
			serf->counter = counter_from_animation[serf->animation];
			return 0;
		}
	}

	const int *a0 = &dir_arr[6*dir_index];
	int i0 = -1;
	dir_t dir = DIR_NONE;
	for (int i = 0; i < 6; i++) {
		map_pos_t new_pos = MAP_MOVE(serf->pos, a0[i]);
		if (((water && MAP_OBJ(new_pos) == 0) ||
		     (!water && !MAP_IN_WATER(new_pos) &&
		      serf_can_pass_map_pos(new_pos))) &&
		    MAP_SERF_INDEX(new_pos) == 0) {
			dir = a0[i];
			i0 = i;
			break;
		}
	}

	if (i0 > -1) {
		int upper = ((serf->s.free_walking.flags >> 4) & 0xf) + i0 - 2;
		if (i0 < 2 && upper < 0) {
			serf->s.free_walking.flags = 0;
			handle_serf_free_walking_switch_on_dir(serf, dir);
			return 0;
		} else if (i0 > 2 && upper > 15) {
			serf->s.free_walking.flags = 0;
		} else {
			int dir_index = dir+1;
			serf->s.free_walking.flags = (upper << 4) | (serf->s.free_walking.flags & 0x8) | dir_index;
			handle_serf_free_walking_switch_on_dir(serf, dir);
			return 0;
		}
	} else {
		int dir_index = 0;
		serf->s.free_walking.flags = (serf->s.free_walking.flags & 0xf8) | dir_index;
		serf->s.free_walking.flags &= ~BIT(3);
		handle_serf_free_walking_switch_with_other(serf);
		return 0;
	}

	return -1;
}

static void
handle_free_walking_common(serf_t *serf)
{
	const int dir_from_offset[] = {
		DIR_UP_LEFT, DIR_UP, -1,
		DIR_LEFT, -1, DIR_RIGHT,
		-1, DIR_DOWN, DIR_DOWN_RIGHT
	};

	/* Directions for moving forwards. Each of the 12 lines represents
	   a general direction as shown in the diagram below.
	   The lines list the local directions in order of preference for that
	   general direction.

	   *         1    0
	   *    2   ________   11
	   *       /\      /\
	   *      /  \    /  \
	   *  3  /    \  /    \  10
	   *    /______\/______\
	   *    \      /\      /
	   *  4  \    /  \    /  9
	   *      \  /    \  /
	   *       \/______\/
	   *    5             8
	   *         6    7
	   */
	const int dir_forward[] = {
		DIR_UP, DIR_UP_LEFT, DIR_RIGHT, DIR_LEFT, DIR_DOWN_RIGHT, DIR_DOWN,
		DIR_UP_LEFT, DIR_UP, DIR_LEFT, DIR_RIGHT, DIR_DOWN, DIR_DOWN_RIGHT,
		DIR_UP_LEFT, DIR_LEFT, DIR_UP, DIR_DOWN, DIR_RIGHT, DIR_DOWN_RIGHT,
		DIR_LEFT, DIR_UP_LEFT, DIR_DOWN, DIR_UP, DIR_DOWN_RIGHT, DIR_RIGHT,
		DIR_LEFT, DIR_DOWN, DIR_UP_LEFT, DIR_DOWN_RIGHT, DIR_UP, DIR_RIGHT,
		DIR_DOWN, DIR_LEFT, DIR_DOWN_RIGHT, DIR_UP_LEFT, DIR_RIGHT, DIR_UP,
		DIR_DOWN, DIR_DOWN_RIGHT, DIR_LEFT, DIR_RIGHT, DIR_UP_LEFT, DIR_UP,
		DIR_DOWN_RIGHT, DIR_DOWN, DIR_RIGHT, DIR_LEFT, DIR_UP, DIR_UP_LEFT,
		DIR_DOWN_RIGHT, DIR_RIGHT, DIR_DOWN, DIR_UP, DIR_LEFT, DIR_UP_LEFT,
		DIR_RIGHT, DIR_DOWN_RIGHT, DIR_UP, DIR_DOWN, DIR_UP_LEFT, DIR_LEFT,
		DIR_RIGHT, DIR_UP, DIR_DOWN_RIGHT, DIR_UP_LEFT, DIR_DOWN, DIR_LEFT,
		DIR_UP, DIR_RIGHT, DIR_UP_LEFT, DIR_DOWN_RIGHT, DIR_LEFT, DIR_DOWN
	};

	int water = (serf->state == SERF_STATE_FREE_SAILING);

	if (BIT_TEST(serf->s.free_walking.flags, 3) &&
	    (serf->s.free_walking.flags & 7) == 0) {
		/* Destination reached */
		handle_serf_free_walking_state_dest_reached(serf);
		return;
	}

	if ((serf->s.free_walking.flags & 7) != 0) {
		/* Obstacle encountered, follow along the edge */
		int r = handle_free_walking_follow_edge(serf);
		if (r >= 0) return;
	}

	/* Move fowards */
	int dir_index = -1;
	int d1 = serf->s.free_walking.dist1;
	int d2 = serf->s.free_walking.dist2;
	if (d1 < 0) {
		if (d2 < 0) {
			if (-d2 < -d1) {
				if (-2*d2 < -d1) dir_index = 3;
				else dir_index = 2;
			} else {
				if (-d2 < -2*d1) dir_index = 1;
				else dir_index = 0;
			}
		} else {
			if (d2 >= -d1) dir_index = 5;
			else dir_index = 4;
		}
	} else {
		if (d2 < 0) {
			if (-d2 >= d1) dir_index = 11;
			else dir_index = 10;
		} else {
			if (d2 < d1) {
				if (2*d2 < d1) dir_index = 9;
				else dir_index = 8;
			} else {
				if (d2 < 2*d1) dir_index = 7;
				else dir_index = 6;
			}
		}
	}

	/* Try to move directly in the preferred direction */
	const int *a0 = &dir_forward[6*dir_index];
	dir_t dir = a0[0];
	map_pos_t new_pos = MAP_MOVE(serf->pos, dir);
	if (((water && MAP_OBJ(new_pos) == 0) ||
	     (!water && !MAP_IN_WATER(new_pos) &&
	      serf_can_pass_map_pos(new_pos))) &&
	    MAP_SERF_INDEX(new_pos) == 0) {
		handle_serf_free_walking_switch_on_dir(serf, dir);
		return;
	}

	/* Check if dest is only one step away. */
	if (!water && abs(d1) <= 1 && abs(d2) <= 1 &&
	    dir_from_offset[(d1+1) + 3*(d2+1)] > -1) {
		/* Convert offset in two dimensions to
		   direction variable. */
		dir_t d = dir_from_offset[(d1+1) + 3*(d2+1)];
		map_pos_t new_pos = MAP_MOVE(serf->pos, d);

		if (!serf_can_pass_map_pos(new_pos)) {
			if (serf->state != SERF_STATE_KNIGHT_FREE_WALKING &&
			    serf->s.free_walking.neg_dist1 != -128) {
				serf->s.free_walking.dist1 += serf->s.free_walking.neg_dist1;
				serf->s.free_walking.dist2 += serf->s.free_walking.neg_dist2;
				serf->s.free_walking.neg_dist1 = 0;
				serf->s.free_walking.neg_dist2 = 0;
				serf->s.free_walking.flags = 0;
			} else {
				serf_log_state_change(serf, SERF_STATE_LOST);
				serf->state = SERF_STATE_LOST;
				serf->s.lost.field_B = 0;
				serf->counter = 0;
			}
			return;
		}

		if (serf->state == SERF_STATE_KNIGHT_FREE_WALKING &&
		    serf->s.free_walking.neg_dist1 != -128 &&
		    MAP_SERF_INDEX(new_pos) != 0) {
			serf_t *other_serf = game_get_serf(MAP_SERF_INDEX(new_pos));
			dir_t other_dir;

			if (serf_is_waiting(other_serf, &other_dir) &&
			    (other_dir == DIR_REVERSE(d) || other_dir == DIR_NONE) &&
			    serf_switch_waiting(other_serf, DIR_REVERSE(d))) {
				/* Do the switch */
				other_serf->pos = serf->pos;
				map_set_serf_index(other_serf->pos, SERF_INDEX(other_serf));
				other_serf->animation = get_walking_animation(MAP_HEIGHT(other_serf->pos) - MAP_HEIGHT(new_pos),
									      DIR_REVERSE(d), 1);
				other_serf->counter = counter_from_animation[other_serf->animation];

				serf->animation = get_walking_animation(MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos), d, 1);
				serf->counter = counter_from_animation[serf->animation];

				serf->pos = new_pos;
				map_set_serf_index(serf->pos, SERF_INDEX(serf));
				return;
			}

			if (other_serf->state == SERF_STATE_WALKING ||
			    other_serf->state == SERF_STATE_TRANSPORTING) {
				serf->s.free_walking.neg_dist2 += 1;
				if (serf->s.free_walking.neg_dist2 >= 10) {
					serf->s.free_walking.neg_dist2 = 0;
					if (other_serf->state == SERF_STATE_TRANSPORTING) {
						if (MAP_HAS_FLAG(new_pos)) {
							if (other_serf->s.walking.wait_counter != -1) {
								int dir = other_serf->s.walking.dir;
								if (dir < 0) dir += 6;
								LOGD("serf", "TODO remove %i from path", SERF_INDEX(other_serf));
							}
							serf_set_lost_state(other_serf);
						}
					} else {
						serf_set_lost_state(other_serf);
					}
				}
			}

			serf->animation = 82;
			serf->counter = counter_from_animation[serf->animation];
			return;
		}
	}

	/* Look for another direction to go in. */
	int i0 = -1;
	for (int i = 0; i < 5; i++) {
		dir = a0[1+i];
		map_pos_t new_pos = MAP_MOVE(serf->pos, dir);
		if (((water && MAP_OBJ(new_pos) == 0) ||
		     (!water && !MAP_IN_WATER(new_pos) &&
		      serf_can_pass_map_pos(new_pos))) &&
		    MAP_SERF_INDEX(new_pos) == 0) {
			i0 = i;
			break;
		}
	}

	if (i0 < 0) {
		handle_serf_free_walking_switch_with_other(serf);
		return;
	}

	int edge = 0;
	if (BIT_TEST(dir_index ^ i0, 0)) edge = 1;
	int upper = (i0/2) + 1;

	serf->s.free_walking.flags = (upper << 4) | (edge << 3) | (dir+1);

	handle_serf_free_walking_switch_on_dir(serf, dir);
}

static void
handle_serf_free_walking_state(serf_t *serf)
{

	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		handle_free_walking_common(serf);
	}
}

static void
handle_serf_logging_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		serf->s.free_walking.neg_dist2 += 1;

		int new_obj = -1;
		if (serf->s.free_walking.neg_dist1 != 0) {
			new_obj = MAP_OBJ_FELLED_TREE_0 + serf->s.free_walking.neg_dist2 - 1;
		} else {
			new_obj = MAP_OBJ_FELLED_PINE_0 + serf->s.free_walking.neg_dist2 - 1;
		}

		/* Change map object. */
		map_set_object(serf->pos, new_obj, -1);

		if (serf->s.free_walking.neg_dist2 < 5) {
			serf->animation = 116 + serf->s.free_walking.neg_dist2;
			serf->counter += counter_from_animation[serf->animation];
		} else {
			serf_log_state_change(serf, SERF_STATE_FREE_WALKING);
			serf->state = SERF_STATE_FREE_WALKING;
			serf->counter = 0;
			serf->s.free_walking.neg_dist1 = -128;
			serf->s.free_walking.neg_dist2 = 1;
			serf->s.free_walking.flags = 0;
			return;
		}
	}
}

static void
handle_serf_planning_logging_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		int index = (game_random_int() & 0x7f) + 1;
		map_pos_t pos = MAP_POS_ADD(serf->pos,
					    game.spiral_pos_pattern[index]);
		int obj = MAP_OBJ(pos);
		if (obj >= MAP_OBJ_TREE_0 && obj <= MAP_OBJ_PINE_7) {
			serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE);
			serf->state = SERF_STATE_READY_TO_LEAVE;
			serf->s.leaving_building.field_B = game.spiral_pattern[2*index] - 1;
			serf->s.leaving_building.dest = game.spiral_pattern[2*index+1] - 1;
			serf->s.leaving_building.dest2 = -game.spiral_pattern[2*index] + 1;
			serf->s.leaving_building.dir = -game.spiral_pattern[2*index+1] + 1;
			serf->s.leaving_building.next_state = SERF_STATE_FREE_WALKING;
			LOGV("serf", "planning logging: tree found, dist %i, %i.",
			     serf->s.leaving_building.field_B,
			     serf->s.leaving_building.dest);
			return;
		}

		serf->counter += 400;
	}
}

static void
handle_serf_planning_planting_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		int index = (game_random_int() & 0x7f) + 1;
		map_pos_t pos = MAP_POS_ADD(serf->pos,
					    game.spiral_pos_pattern[index]);
		if (MAP_PATHS(pos) == 0 &&
		    MAP_OBJ(pos) == MAP_OBJ_NONE &&
		    MAP_TYPE_UP(pos) == 5 &&
		    MAP_TYPE_DOWN(pos) == 5 &&
		    MAP_TYPE_UP(MAP_MOVE_UP_LEFT(pos)) == 5 &&
		    MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(pos)) == 5) {
			serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE);
			serf->state = SERF_STATE_READY_TO_LEAVE;
			serf->s.leaving_building.field_B = game.spiral_pattern[2*index] - 1;
			serf->s.leaving_building.dest = game.spiral_pattern[2*index+1] - 1;
			serf->s.leaving_building.dest2 = -game.spiral_pattern[2*index] + 1;
			serf->s.leaving_building.dir = -game.spiral_pattern[2*index+1] + 1;
			serf->s.leaving_building.next_state = SERF_STATE_FREE_WALKING;
			LOGV("serf", "planning planting: free space found, dist %i, %i.",
			     serf->s.leaving_building.field_B,
			     serf->s.leaving_building.dest);
			return;
		}

		serf->counter += 700;
	}	
}

static void
handle_serf_planting_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		if (serf->s.free_walking.neg_dist2 != 0) {
			serf_log_state_change(serf, SERF_STATE_FREE_WALKING);
			serf->state = SERF_STATE_FREE_WALKING;
			serf->s.free_walking.neg_dist1 = -128;
			serf->s.free_walking.neg_dist2 = 0;
			serf->s.free_walking.flags = 0;
			serf->counter = 0;
			return;
		}

		/* Plant a tree */
		serf->animation = 122;
		map_obj_t new_obj = MAP_OBJ_NEW_PINE + (game_random_int() & 1);

		if (MAP_PATHS(serf->pos) == 0 &&
		    MAP_OBJ(serf->pos) == MAP_OBJ_NONE) {
			map_set_object(serf->pos, new_obj, -1);
		}

		serf->s.free_walking.neg_dist2 = -serf->s.free_walking.neg_dist2 - 1;
		serf->counter += 128;
	}
}

static void
handle_serf_planning_stonecutting(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		int index = (game_random_int() & 0x7f) + 1;
		map_pos_t pos = MAP_POS_ADD(serf->pos,
					    game.spiral_pos_pattern[index]);;
		int obj = MAP_OBJ(MAP_MOVE_UP_LEFT(pos));
		if (obj >= MAP_OBJ_STONE_0 &&
		    obj <= MAP_OBJ_STONE_7 &&
		    serf_can_pass_map_pos(pos)) {
			serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE);
			serf->state = SERF_STATE_READY_TO_LEAVE;
			serf->s.leaving_building.field_B = game.spiral_pattern[2*index] - 1;
			serf->s.leaving_building.dest = game.spiral_pattern[2*index+1] - 1;
			serf->s.leaving_building.dest2 = -game.spiral_pattern[2*index] + 1;
			serf->s.leaving_building.dir = -game.spiral_pattern[2*index+1] + 1;
			serf->s.leaving_building.next_state = SERF_STATE_STONECUTTER_FREE_WALKING;
			LOGV("serf", "planning stonecutting: stone found, dist %i, %i.",
			     serf->s.leaving_building.field_B,
			     serf->s.leaving_building.dest);
			return;
		}

		serf->counter += 100;
	}
}

static void
handle_stonecutter_free_walking(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		map_pos_t pos = MAP_MOVE_UP_LEFT(serf->pos);
		if (MAP_SERF_INDEX(pos) == 0 &&
		    MAP_OBJ(pos) >= MAP_OBJ_STONE_0 &&
		    MAP_OBJ(pos) <= MAP_OBJ_STONE_7) {
			serf->s.free_walking.neg_dist1 += serf->s.free_walking.dist1;
			serf->s.free_walking.neg_dist2 += serf->s.free_walking.dist2;
			serf->s.free_walking.dist1 = 0;
			serf->s.free_walking.dist2 = 0;
			serf->s.free_walking.flags = 8;
		}

		handle_free_walking_common(serf);
	}
}

static void
handle_serf_stonecutting_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	if (serf->s.free_walking.neg_dist1 == 0) {
		if (serf->counter > serf->s.free_walking.neg_dist2) return;
		serf->counter -= serf->s.free_walking.neg_dist2 + 1;
		serf->s.free_walking.neg_dist1 = 1;
		serf->animation = 123;
		serf->counter += 1536;
	}

	while (serf->counter < 0) {
		if (serf->s.free_walking.neg_dist1 != 1) {
			serf_log_state_change(serf, SERF_STATE_FREE_WALKING);
			serf->state = SERF_STATE_FREE_WALKING;
			serf->s.free_walking.neg_dist1 = -128;
			serf->s.free_walking.neg_dist2 = 1;
			serf->s.free_walking.flags = 0;
			serf->counter = 0;
			return;
		}

		if (MAP_SERF_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)) != 0) {
			serf->counter = 0;
			return;
		}

		/* Decrement stone quantity or remove entirely if this
		   was the last piece. */
		int obj = MAP_OBJ(serf->pos);
		if (obj <= MAP_OBJ_STONE_6) map_set_object(serf->pos, obj+1, -1);
		else map_set_object(serf->pos, MAP_OBJ_NONE, -1);

		serf->counter = 0;
		serf_start_walking(serf, DIR_DOWN_RIGHT, 24, 1);
		serf->tick = game.tick;

		serf->s.free_walking.neg_dist1 = 2;
	}
}

static void
handle_serf_sawing_state(serf_t *serf)
{
	if (serf->s.sawing.mode == 0) {
		building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
		if (building->stock[1].available > 0) {
			building->stock[1].available -= 1;
			serf->s.sawing.mode = 1;
			serf->animation = 124;
			serf->counter = counter_from_animation[serf->animation];
			serf->tick = game.tick;
			map_set_serf_index(serf->pos, SERF_INDEX(serf));
		}
	} else {
		uint16_t delta = game.tick - serf->tick;
		serf->tick = game.tick;
		serf->counter -= delta;

		if (serf->counter >= 0) return;

		map_set_serf_index(serf->pos, 0);
		serf_log_state_change(serf, SERF_STATE_MOVE_RESOURCE_OUT);
		serf->state = SERF_STATE_MOVE_RESOURCE_OUT;
		serf->s.move_resource_out.res = 1 + RESOURCE_PLANK;
		serf->s.move_resource_out.res_dest = 0;
		serf->s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

		/* Update resource stats. */
		player_t *player = game.player[SERF_PLAYER(serf)];
		player->resource_count[RESOURCE_PLANK] += 1;
	}
}

static void
handle_serf_lost_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		/* Try to find a suitable destination. */
		for (int i = 0; i < 258; i++) {
			int index = (serf->s.lost.field_B == 0) ? 1+i : 258-i;
			map_pos_t dest = MAP_POS_ADD(serf->pos,
						     game.spiral_pos_pattern[index]);

			if (MAP_HAS_FLAG(dest)) {
				flag_t *flag = game_get_flag(MAP_OBJ_INDEX(dest));
				if ((FLAG_LAND_PATHS(flag) != 0 ||
				     (FLAG_HAS_INVENTORY(flag) && FLAG_ACCEPTS_SERFS(flag))) &&
				    MAP_HAS_OWNER(dest) && MAP_OWNER(dest) == SERF_PLAYER(serf)) {
					if (SERF_TYPE(serf) >= SERF_KNIGHT_0 &&
					    SERF_TYPE(serf) <= SERF_KNIGHT_4) {
						serf_log_state_change(serf, SERF_STATE_KNIGHT_FREE_WALKING);
						serf->state = SERF_STATE_KNIGHT_FREE_WALKING;
					} else {
						serf_log_state_change(serf, SERF_STATE_FREE_WALKING);
						serf->state = SERF_STATE_FREE_WALKING;
					}

					serf->s.free_walking.dist1 = game.spiral_pattern[2*index];
					serf->s.free_walking.dist2 = game.spiral_pattern[2*index+1];
					serf->s.free_walking.neg_dist1 = -128;
					serf->s.free_walking.neg_dist2 = -1;
					serf->s.free_walking.flags = 0;
					serf->counter = 0;
					return;
				}
			}
		}

		/* Choose a random destination */
		uint size = 16;
		int tries = 10;

		while (1) {
			tries -= 1;
			if (tries < 0) {
				if (size < 64) {
					tries = 19;
					size *= 2;
				} else {
					tries = -1;
					size = 16;
				}
			}

			int r = game_random_int();
			int col = ((r & (size-1)) - (size/2)) & game.map.col_mask;
			int row = (((r >> 8) & (size-1)) - (size/2)) & game.map.row_mask;

			map_pos_t dest = MAP_POS_ADD(serf->pos,
						     MAP_POS(col, row));
			if ((MAP_OBJ(dest) == 0 &&
			     MAP_HEIGHT(dest) > 0) ||
			    (MAP_HAS_FLAG(dest) &&
			     (MAP_HAS_OWNER(dest) &&
			      MAP_OWNER(dest) == SERF_PLAYER(serf)))) {
				if (SERF_TYPE(serf) >= SERF_KNIGHT_0 &&
				    SERF_TYPE(serf) <= SERF_KNIGHT_4) {
					serf_log_state_change(serf, SERF_STATE_KNIGHT_FREE_WALKING);
					serf->state = SERF_STATE_KNIGHT_FREE_WALKING;
				} else {
					serf_log_state_change(serf, SERF_STATE_FREE_WALKING);
					serf->state = SERF_STATE_FREE_WALKING;
				}

				serf->s.free_walking.dist1 = col;
				serf->s.free_walking.dist2 = row;
				serf->s.free_walking.neg_dist1 = -128;
				serf->s.free_walking.neg_dist2 = -1;
				serf->s.free_walking.flags = 0;
				serf->counter = 0;
				return;
			}
		}
	}
}

static void
handle_lost_sailor(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		/* Try to find a suitable destination. */
		for (int i = 0; i < 258; i++) {
			map_pos_t dest = MAP_POS_ADD(serf->pos,
						     game.spiral_pos_pattern[i]);

			if (MAP_HAS_FLAG(dest)) {
				flag_t *flag = game_get_flag(MAP_OBJ_INDEX(dest));
				if (FLAG_LAND_PATHS(flag) != 0 &&
				    MAP_HAS_OWNER(dest) && MAP_OWNER(dest) == SERF_PLAYER(serf)) {
					serf_log_state_change(serf, SERF_STATE_FREE_SAILING);
					serf->state = SERF_STATE_FREE_SAILING;

					serf->s.free_walking.dist1 = game.spiral_pattern[2*i];
					serf->s.free_walking.dist2 = game.spiral_pattern[2*i+1];
					serf->s.free_walking.neg_dist1 = -128;
					serf->s.free_walking.neg_dist2 = -1;
					serf->s.free_walking.flags = 0;
					serf->counter = 0;
					return;
				}
			}
		}

		/* Choose a random, empty destination */
		while (1) {
			int r = game_random_int();
			int col = ((r & 0x1f) - 16) & game.map.col_mask;
			int row = (((r >> 8) & 0x1f) - 16) & game.map.row_mask;

			map_pos_t dest = MAP_POS_ADD(serf->pos,
						     MAP_POS(col, row));
			if (MAP_OBJ(dest) == 0) {
				serf_log_state_change(serf, SERF_STATE_FREE_SAILING);
				serf->state = SERF_STATE_FREE_SAILING;

				serf->s.free_walking.dist1 = col;
				serf->s.free_walking.dist2 = row;
				serf->s.free_walking.neg_dist1 = -128;
				serf->s.free_walking.neg_dist2 = -1;
				serf->s.free_walking.flags = 0;
				serf->counter = 0;
				return;
			}
		}
	}
}

static void
handle_free_sailing(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		if (!MAP_IN_WATER(serf->pos)) {
			serf_log_state_change(serf, SERF_STATE_LOST);
			serf->state = SERF_STATE_LOST;
			serf->s.lost.field_B = 0;
			return;
		}

		handle_free_walking_common(serf);
	}
}

static void
handle_serf_escape_building_state(serf_t *serf)
{
	if (MAP_SERF_INDEX(serf->pos) == 0) {
		map_set_serf_index(serf->pos, SERF_INDEX(serf));
		serf->animation = 82;
		serf->counter = 0;
		serf->tick = game.tick;

		serf_log_state_change(serf, SERF_STATE_LOST);
		serf->state = SERF_STATE_LOST;
		serf->s.lost.field_B = 0;
	}
}

static void
handle_serf_mining_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));

		LOGV("serf", "mining substate: %i.", serf->s.mining.substate);
		switch (serf->s.mining.substate) {
		case 0:
		{
			/* There is a small chance that the miner will
			   not require food and skip to state 2. */
			int r = game_random_int();
			if ((r & 7) == 0) serf->s.mining.substate = 2;
			else serf->s.mining.substate = 1;
			serf->counter += 100 + (r & 0x1ff);
		}
		break;
		case 1:
			if (building->stock[0].available == 0) {
				map_set_serf_index(serf->pos, SERF_INDEX(serf));
				serf->animation = 98;
				serf->counter += 256;
				if (serf->counter < 0) serf->counter = 255;
			} else {
				/* Eat the food. */
				building->stock[0].available -= 1;
				serf->s.mining.substate = 3;
				map_set_serf_index(serf->pos, SERF_INDEX(serf));
				serf->animation = 125;
				serf->counter = counter_from_animation[serf->animation];
			}
			break;
		case 2:
			serf->s.mining.substate = 3;
			map_set_serf_index(serf->pos, SERF_INDEX(serf));
			serf->animation = 125;
			serf->counter = counter_from_animation[serf->animation];
			break;
		case 3:
			serf->s.mining.substate = 4;
			building->serf &= ~BIT(4);
			serf->animation = 126;
			serf->counter = 304; /* TODO counter_from_animation[126] == 303 */
			break;
		case 4:
		{
			building->serf |= BIT(3);
			map_set_serf_index(serf->pos, 0);
			/* fall through */
		}
		case 5:
		case 6:
		case 7:
			serf->s.mining.substate += 1;

			/* Look for resource in ground. */
			map_pos_t offset = game.spiral_pos_pattern[(game_random_int() >> 2) & 0x1f];
			map_pos_t dest = MAP_POS_ADD(serf->pos, offset);
			if ((MAP_OBJ(dest) == MAP_OBJ_NONE || MAP_OBJ(dest) > MAP_OBJ_CASTLE) &&
			    MAP_RES_TYPE(dest) == serf->s.mining.deposit &&
			    MAP_RES_AMOUNT(dest) > 0) {
				/* Decrement resource count in ground. */
				map_remove_ground_deposit(dest, 1);

				/* Hand resource to miner. */
				const resource_type_t res_from_mine_type[] = {
					RESOURCE_GOLDORE, RESOURCE_IRONORE,
					RESOURCE_COAL, RESOURCE_STONE
				};

				serf->s.mining.res = res_from_mine_type[serf->s.mining.deposit-1] + 1;
				serf->s.mining.substate = 8;
			}

			serf->counter += 1000;
		break;
		case 8:
			map_set_serf_index(serf->pos, SERF_INDEX(serf));
			serf->s.mining.substate = 9;
			building->serf &= ~BIT(3);
			serf->animation = 127;
			serf->counter = counter_from_animation[serf->animation];
			break;
		case 9:
			serf->s.mining.substate = 10;
			building->serf |= BIT(4);

			if (building->progress == 0x8000) {
				/* Handle empty mine. */
				player_t *player = game.player[SERF_PLAYER(serf)];
				if (PLAYER_IS_AI(player)) {
					/* TODO Burn building. */
				}

				int type = ((BUILDING_TYPE(building)-BUILDING_STONEMINE) << 5) | 4;
				player_add_notification(game.player[BUILDING_PLAYER(building)],
							type, building->pos);
			}

			building->progress = (building->progress << 1) & 0xffff;
			if (serf->s.mining.res > 0) building->progress += 1;

			serf->animation = 128;
			serf->counter = 384; /* TODO counter_from_animation[128] == 383 */
			break;
		case 10:
			map_set_serf_index(serf->pos, 0);
			if (serf->s.mining.res == 0) {
				serf->s.mining.substate = 0;
				serf->counter = 0;
			} else {
				uint res = serf->s.mining.res;
				map_set_serf_index(serf->pos, 0);

				serf_log_state_change(serf, SERF_STATE_MOVE_RESOURCE_OUT);
				serf->state = SERF_STATE_MOVE_RESOURCE_OUT;
				serf->s.move_resource_out.res = res;
				serf->s.move_resource_out.res_dest = 0;
				serf->s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

				/* Update resource stats. */
				player_t *player = game.player[SERF_PLAYER(serf)];
				player->resource_count[res-1] += 1;
				return;
			}
			break;
		default:
			NOT_REACHED();
			break;
		}
	}
}

static void
handle_serf_smelting_state(serf_t *serf)
{
	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));

	if (serf->s.smelting.mode == 0) {
		if (building->stock[0].available != 0 &&
		    building->stock[1].available != 0) {
			building->serf |= BIT(4);
			building->stock[0].available -= 1;
			building->stock[1].available -= 1;

			serf->s.smelting.mode = 1;
			if (serf->s.smelting.type == 0) {
				serf->animation = 130;
			} else {
				serf->animation = 129;
			}
			serf->s.smelting.counter = 20;
			serf->counter = counter_from_animation[serf->animation];
			serf->tick = game.tick;

			map_set_serf_index(serf->pos, SERF_INDEX(serf));
		}
	} else {
		uint16_t delta = game.tick - serf->tick;
		serf->tick = game.tick;
		serf->counter -= delta;

		while (serf->counter < 0) {
			serf->s.smelting.counter -= 1;
			if (serf->s.smelting.counter < 0) {
				building->serf &= ~BIT(4);

				int res = -1;
				if (serf->s.smelting.type == 0) res = 1 + RESOURCE_STEEL;
				else res = 1 + RESOURCE_GOLDBAR;

				serf_log_state_change(serf, SERF_STATE_MOVE_RESOURCE_OUT);
				serf->state = SERF_STATE_MOVE_RESOURCE_OUT;

				serf->s.move_resource_out.res = res;
				serf->s.move_resource_out.res_dest = 0;
				serf->s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

				/* Update resource stats. */
				player_t *player = game.player[SERF_PLAYER(serf)];
				player->resource_count[res-1] += 1;
				return;
			} else if (serf->s.smelting.counter == 0) {
				map_set_serf_index(serf->pos, 0);
			}

			serf->counter += 384;
		}
	}
}

static void
handle_serf_planning_fishing_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		int index = ((game_random_int() >> 2) & 0x3f) + 1;
		map_pos_t dest = MAP_POS_ADD(serf->pos,
					     game.spiral_pos_pattern[index]);

		if (MAP_OBJ(dest) == MAP_OBJ_NONE &&
		    MAP_PATHS(dest) == 0 &&
		    (((MAP_TYPE_DOWN(dest) & 0xc) == 0 &&
		      (MAP_TYPE_UP(MAP_MOVE_UP_LEFT(dest)) & 0xc) != 0) ||
		     ((MAP_TYPE_DOWN(MAP_MOVE_LEFT(dest)) & 0xc) == 0 &&
		      (MAP_TYPE_UP(MAP_MOVE_UP(dest)) & 0xc) != 0))) {
			serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE);
			serf->state = SERF_STATE_READY_TO_LEAVE;
			serf->s.leaving_building.field_B = game.spiral_pattern[2*index] - 1;
			serf->s.leaving_building.dest = game.spiral_pattern[2*index+1] - 1;
			serf->s.leaving_building.dest2 = -game.spiral_pattern[2*index] + 1;
			serf->s.leaving_building.dir = -game.spiral_pattern[2*index+1] + 1;
			serf->s.leaving_building.next_state = SERF_STATE_FREE_WALKING;
			LOGV("serf", "planning fishing: lake found, dist %i, %i.",
			     serf->s.leaving_building.field_B,
			     serf->s.leaving_building.dest);
			return;
		}

		serf->counter += 100;
	}
}

static void
handle_serf_fishing_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		if (serf->s.free_walking.neg_dist2 != 0 ||
		    serf->s.free_walking.flags == 10) {
			/* Stop fishing. Walk back. */
			serf_log_state_change(serf, SERF_STATE_FREE_WALKING);
			serf->state = SERF_STATE_FREE_WALKING;
			serf->s.free_walking.neg_dist1 = -128;
			serf->s.free_walking.flags = 0;
			serf->counter = 0;
			return;
		}

		serf->s.free_walking.neg_dist1 += 1;
		if ((serf->s.free_walking.neg_dist1 % 2) == 0) {
			serf->animation -= 2;
			serf->counter += 768;
			continue;
		}

		dir_t dir = -1;
		if (serf->animation == 131) {
			if (MAP_IN_WATER(MAP_MOVE_LEFT(serf->pos))) dir = DIR_LEFT;
			else dir = DIR_DOWN;
		} else {
			if (MAP_IN_WATER(MAP_MOVE_RIGHT(serf->pos))) dir = DIR_RIGHT;
			else dir = DIR_DOWN_RIGHT;
		}

		int res = MAP_RES_FISH(MAP_MOVE(serf->pos, dir));
		if (res > 0 && (game_random_int() & 0x3f) + 4 < res) {
			/* Caught a fish. */
			map_remove_fish(MAP_MOVE(serf->pos, dir), 1);
			serf->s.free_walking.neg_dist2 = 1+RESOURCE_FISH;
		}

		serf->s.free_walking.flags += 1;
		serf->animation += 2;
		serf->counter += 128;
	}
}

static void
handle_serf_planning_farming_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		int index = ((game_random_int() >> 2) & 0x1f) + 7;
		map_pos_t dest = MAP_POS_ADD(serf->pos,
					     game.spiral_pos_pattern[index]);

		/* If destination doesn't have an object it must be
		   of the correct type and the surrounding spaces
		   must not be occupied by large buildings.
		   If it _has_ an object it must be an existing field. */
		if ((MAP_OBJ(dest) == MAP_OBJ_NONE &&
		     (MAP_TYPE_UP(dest) == 5 && MAP_TYPE_DOWN(dest) == 5 &&
		      MAP_PATHS(dest) == 0 &&
		      MAP_OBJ(MAP_MOVE_RIGHT(dest)) != MAP_OBJ_LARGE_BUILDING &&
		      MAP_OBJ(MAP_MOVE_RIGHT(dest)) != MAP_OBJ_CASTLE &&
		      MAP_OBJ(MAP_MOVE_DOWN_RIGHT(dest)) != MAP_OBJ_LARGE_BUILDING &&
		      MAP_OBJ(MAP_MOVE_DOWN_RIGHT(dest)) != MAP_OBJ_CASTLE &&
		      MAP_OBJ(MAP_MOVE_DOWN(dest)) != MAP_OBJ_LARGE_BUILDING &&
		      MAP_OBJ(MAP_MOVE_DOWN(dest)) != MAP_OBJ_CASTLE &&
		      MAP_TYPE_DOWN(MAP_MOVE_LEFT(dest)) == 5 &&
		      MAP_OBJ(MAP_MOVE_LEFT(dest)) != MAP_OBJ_LARGE_BUILDING &&
		      MAP_OBJ(MAP_MOVE_LEFT(dest)) != MAP_OBJ_CASTLE &&
		      MAP_TYPE_UP(MAP_MOVE_UP_LEFT(dest)) == 5 &&
		      MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(dest)) == 5 &&
		      MAP_OBJ(MAP_MOVE_UP_LEFT(dest)) != MAP_OBJ_LARGE_BUILDING &&
		      MAP_OBJ(MAP_MOVE_UP_LEFT(dest)) != MAP_OBJ_CASTLE &&
		      MAP_TYPE_UP(MAP_MOVE_UP(dest)) == 5 &&
		      MAP_OBJ(MAP_MOVE_UP(dest)) != MAP_OBJ_LARGE_BUILDING &&
		      MAP_OBJ(MAP_MOVE_UP(dest)) != MAP_OBJ_CASTLE)) ||
		    MAP_OBJ(dest) == MAP_OBJ_SEEDS_5 ||
		    (MAP_OBJ(dest) >= MAP_OBJ_FIELD_0 &&
		     MAP_OBJ(dest) <= MAP_OBJ_FIELD_5)) {
			serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE);
			serf->state = SERF_STATE_READY_TO_LEAVE;
			serf->s.leaving_building.field_B = game.spiral_pattern[2*index] - 1;
			serf->s.leaving_building.dest = game.spiral_pattern[2*index+1] - 1;
			serf->s.leaving_building.dest2 = -game.spiral_pattern[2*index] + 1;
			serf->s.leaving_building.dir = -game.spiral_pattern[2*index+1] + 1;
			serf->s.leaving_building.next_state = SERF_STATE_FREE_WALKING;
			LOGV("serf", "planning farming: field spot found, dist %i, %i.",
			     serf->s.leaving_building.field_B,
			     serf->s.leaving_building.dest);
			return;
		}

		serf->counter += 500;
	}
}

static void
handle_serf_farming_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	if (serf->counter >= 0) return;

	if (serf->s.free_walking.neg_dist1 == 0) {
		/* Sowing. */
		if (MAP_OBJ(serf->pos) == 0 &&
		    MAP_PATHS(serf->pos) == 0) {
			map_set_object(serf->pos, MAP_OBJ_SEEDS_0, -1);
		}
	} else {
		/* Harvesting. */
		serf->s.free_walking.neg_dist2 = 1;
		if (MAP_OBJ(serf->pos) == MAP_OBJ_SEEDS_5) {
			map_set_object(serf->pos, MAP_OBJ_FIELD_0, -1);
		} else if (MAP_OBJ(serf->pos) == MAP_OBJ_FIELD_5) {
			map_set_object(serf->pos, MAP_OBJ_FIELD_EXPIRED, -1);
		} else if (MAP_OBJ(serf->pos) != MAP_OBJ_FIELD_EXPIRED) {
			map_set_object(serf->pos, MAP_OBJ(serf->pos) + 1, -1);
		}
	}

	serf_log_state_change(serf, SERF_STATE_FREE_WALKING);
	serf->state = SERF_STATE_FREE_WALKING;
	serf->s.free_walking.neg_dist1 = -128;
	serf->s.free_walking.flags = 0;
	serf->counter = 0;
}

static void
handle_serf_milling_state(serf_t *serf)
{
	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));

	if (serf->s.milling.mode == 0) {
		if (building->stock[0].available > 0) {
			building->serf |= BIT(4);
			building->stock[0].available -= 1;

			serf->s.milling.mode = 1;
			serf->animation = 137;
			serf->counter = counter_from_animation[serf->animation];
			serf->tick = game.tick;

			map_set_serf_index(serf->pos, SERF_INDEX(serf));
		}
	} else {
		uint16_t delta = game.tick - serf->tick;
		serf->tick = game.tick;
		serf->counter -= delta;

		while (serf->counter < 0) {
			serf->s.milling.mode += 1;
			if (serf->s.milling.mode == 5) {
				/* Done milling. */
				building->serf &= ~BIT(4);
				serf_log_state_change(serf, SERF_STATE_MOVE_RESOURCE_OUT);
				serf->state = SERF_STATE_MOVE_RESOURCE_OUT;
				serf->s.move_resource_out.res = 1 + RESOURCE_FLOUR;
				serf->s.move_resource_out.res_dest = 0;
				serf->s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

				player_t *player = game.player[SERF_PLAYER(serf)];
				player->resource_count[RESOURCE_FLOUR] += 1;
				return;
			} else if (serf->s.milling.mode == 3) {
				map_set_serf_index(serf->pos, SERF_INDEX(serf));
				serf->animation = 137;
				serf->counter = counter_from_animation[serf->animation];
			} else {
				map_set_serf_index(serf->pos, 0);
				serf->counter += 1500;
			}
		}		
	}
}

static void
handle_serf_baking_state(serf_t *serf)
{
	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));

	if (serf->s.baking.mode == 0) {
		if (building->stock[0].available > 0) {
			building->stock[0].available -= 1;

			serf->s.baking.mode = 1;
			serf->animation = 138;
			serf->counter = counter_from_animation[serf->animation];
			serf->tick = game.tick;

			map_set_serf_index(serf->pos, SERF_INDEX(serf));
		}
	} else {
		uint16_t delta = game.tick - serf->tick;
		serf->tick = game.tick;
		serf->counter -= delta;

		while (serf->counter < 0) {
			serf->s.baking.mode += 1;
			if (serf->s.baking.mode == 3) {
				/* Done baking. */
				building->serf &= ~BIT(4);

				serf_log_state_change(serf, SERF_STATE_MOVE_RESOURCE_OUT);
				serf->state = SERF_STATE_MOVE_RESOURCE_OUT;
				serf->s.move_resource_out.res = 1 + RESOURCE_BREAD;
				serf->s.move_resource_out.res_dest = 0;
				serf->s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

				player_t *player = game.player[SERF_PLAYER(serf)];
				player->resource_count[RESOURCE_BREAD] += 1;
				return;
			} else {
				building->serf |= BIT(4);
				map_set_serf_index(serf->pos, 0);
				serf->counter += 1500;
			}
		}		
	}
}

static void
handle_serf_pigfarming_state(serf_t *serf)
{
	/* When the serf is present there is also at least one
	   pig present and at most eight. */
	const int breeding_prob[] = {
		6000, 8000, 10000, 11000,
		12000, 13000, 14000, 0
	};

	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));

	if (serf->s.pigfarming.mode == 0) {
		if (building->stock[0].available > 0) {
			building->stock[0].available -= 1;

			serf->s.pigfarming.mode = 1;
			serf->animation = 139;
			serf->counter = counter_from_animation[serf->animation];
			serf->tick = game.tick;

			map_set_serf_index(serf->pos, SERF_INDEX(serf));
		}
	} else {
		uint16_t delta = game.tick - serf->tick;
		serf->tick = game.tick;
		serf->counter -= delta;

		while (serf->counter < 0) {
			serf->s.pigfarming.mode += 1;
			if (serf->s.pigfarming.mode & 1) {
				if (serf->s.pigfarming.mode != 7) {
					map_set_serf_index(serf->pos, SERF_INDEX(serf));
					serf->animation = 139;
					serf->counter = counter_from_animation[serf->animation];
				} else if (building->stock[1].available == 8 ||
					   (building->stock[1].available > 3 &&
					    ((20*game_random_int()) >> 16) < building->stock[1].available)) {
					/* Pig is ready for the butcher. */
					building->stock[1].available -= 1;

					serf_log_state_change(serf, SERF_STATE_MOVE_RESOURCE_OUT);
					serf->state = SERF_STATE_MOVE_RESOURCE_OUT;
					serf->s.move_resource_out.res = 1 + RESOURCE_PIG;
					serf->s.move_resource_out.res_dest = 0;
					serf->s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

					/* Update resource stats. */
					player_t *player = game.player[SERF_PLAYER(serf)];
					player->resource_count[RESOURCE_PIG] += 1;
				} else if (game_random_int() & 0xf) {
					serf->s.pigfarming.mode = 1;
					serf->animation = 139;
					serf->counter = counter_from_animation[serf->animation];
					serf->tick = game.tick;
					map_set_serf_index(serf->pos, SERF_INDEX(serf));
				} else {
					serf->s.pigfarming.mode = 0;
				}
				return;
			} else {
				map_set_serf_index(serf->pos, 0);
				if (building->stock[1].available < 8 &&
				    game_random_int() < breeding_prob[building->stock[1].available-1]) {
					building->stock[1].available += 1;
				}
				serf->counter += 2048;
			}
		}		
	}
}

static void
handle_serf_butchering_state(serf_t *serf)
{
	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));

	if (serf->s.butchering.mode == 0) {
		if (building->stock[0].available > 0) {
			building->stock[0].available -= 1;

			serf->s.butchering.mode = 1;
			serf->animation = 140;
			serf->counter = counter_from_animation[serf->animation];
			serf->tick = game.tick;

			map_set_serf_index(serf->pos, SERF_INDEX(serf));
		}
	} else {
		uint16_t delta = game.tick - serf->tick;
		serf->tick = game.tick;
		serf->counter -= delta;

		if (serf->counter < 0) {
			/* Done butchering. */
			map_set_serf_index(serf->pos, 0);

			serf_log_state_change(serf, SERF_STATE_MOVE_RESOURCE_OUT);
			serf->state = SERF_STATE_MOVE_RESOURCE_OUT;
			serf->s.move_resource_out.res = 1 + RESOURCE_MEAT;
			serf->s.move_resource_out.res_dest = 0;
			serf->s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

			/* Update resource stats. */
			player_t *player = game.player[SERF_PLAYER(serf)];
			player->resource_count[RESOURCE_MEAT] += 1;
		}		
	}
}

static void
handle_serf_making_weapon_state(serf_t *serf)
{
	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));

	if (serf->s.making_weapon.mode == 0) {
		/* One of each resource makes a sword and a shield.
		   Bit 3 is set if a sword has been made and a
		   shield can be made without more resources. */
		/* TODO Use of this bit overlaps with sfx check bit. */
		if (!BIT_TEST(building->serf, 3)) {
			if (building->stock[0].available == 0 ||
			    building->stock[1].available == 0) {
				return;
			}
			building->stock[0].available -= 1;
			building->stock[1].available -= 1;
		}

		building->serf |= BIT(4);

		serf->s.making_weapon.mode = 1;
		serf->animation = 143;
		serf->counter = counter_from_animation[serf->animation];
		serf->tick = game.tick;

		map_set_serf_index(serf->pos, SERF_INDEX(serf));
	} else {
		uint16_t delta = game.tick - serf->tick;
		serf->tick = game.tick;
		serf->counter -= delta;

		while (serf->counter < 0) {
			serf->s.making_weapon.mode += 1;
			if (serf->s.making_weapon.mode == 7) {
				/* Done making sword or shield. */
				building->serf &= ~BIT(4);
				map_set_serf_index(serf->pos, 0);

				resource_type_t res = BIT_TEST(building->serf, 3) ? RESOURCE_SHIELD : RESOURCE_SWORD;
				building->serf ^= BIT(3);

				serf_log_state_change(serf, SERF_STATE_MOVE_RESOURCE_OUT);
				serf->state = SERF_STATE_MOVE_RESOURCE_OUT;
				serf->s.move_resource_out.res = 1 + res;
				serf->s.move_resource_out.res_dest = 0;
				serf->s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

				/* Update resource stats. */
				player_t *player = game.player[SERF_PLAYER(serf)];
				player->resource_count[res] += 1;
				return;
			} else {
				serf->counter += 576;
			}
		}		
	}
}

static void
handle_serf_making_tool_state(serf_t *serf)
{
	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));

	if (serf->s.making_tool.mode == 0) {
		if (building->stock[0].available > 0 &&
		    building->stock[1].available > 0) {
			building->stock[0].available -= 1;
			building->stock[1].available -= 1;

			serf->s.making_tool.mode = 1;
			serf->animation = 144;
			serf->counter = counter_from_animation[serf->animation];
			serf->tick = game.tick;

			map_set_serf_index(serf->pos, SERF_INDEX(serf));
		}
	} else {
		uint16_t delta = game.tick - serf->tick;
		serf->tick = game.tick;
		serf->counter -= delta;

		while (serf->counter < 0) {
			serf->s.making_tool.mode += 1;
			if (serf->s.making_tool.mode == 4) {
				/* Done making tool. */
				map_set_serf_index(serf->pos, 0);

				player_t *player = game.player[SERF_PLAYER(serf)];
				int total_tool_prio = 0;
				for (int i = 0; i < 9; i++) total_tool_prio += player->tool_prio[i];
				total_tool_prio >>= 4;

				int res = -1;
				if (total_tool_prio > 0) {
					/* Use defined tool priorities. */
					int prio_offset = (total_tool_prio*game_random_int()) >> 16;
					for (int i = 0; i < 9; i++) {
						prio_offset -= player->tool_prio[i] >> 4;
						if (prio_offset < 0) {
							res = RESOURCE_SHOVEL + i;
							break;
						}
					}
				} else {
					/* Completely random. */
					res = RESOURCE_SHOVEL + ((9*game_random_int()) >> 16);
				}

				serf_log_state_change(serf, SERF_STATE_MOVE_RESOURCE_OUT);
				serf->state = SERF_STATE_MOVE_RESOURCE_OUT;
				serf->s.move_resource_out.res = 1 + res;
				serf->s.move_resource_out.res_dest = 0;
				serf->s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

				/* Update resource stats. */
				player->resource_count[res] += 1;
				return;
			} else {
				serf->counter += 1536;
			}
		}		
	}
}

static void
handle_serf_building_boat_state(serf_t *serf)
{
	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));

	if (serf->s.building_boat.mode == 0) {
		if (building->stock[0].available == 0) return;
		building->stock[0].available -= 1;
		building->stock[1].available = 0;

		serf->s.building_boat.mode = 1;
		serf->animation = 146;
		serf->counter = counter_from_animation[serf->animation];
		serf->tick = game.tick;

		map_set_serf_index(serf->pos, SERF_INDEX(serf));
	} else {
		uint16_t delta = game.tick - serf->tick;
		serf->tick = game.tick;
		serf->counter -= delta;

		while (serf->counter < 0) {
			serf->s.building_boat.mode += 1;
			if (serf->s.building_boat.mode == 9) {
				/* Boat done. */
				map_pos_t new_pos = MAP_MOVE_DOWN_RIGHT(serf->pos);
				if (MAP_SERF_INDEX(new_pos) != 0) {
					/* Wait for flag to be free. */
					serf->s.building_boat.mode -= 1;
					serf->counter = 0;
				} else {
					/* Drop boat at flag. */
					building->stock[1].available = 0;
					map_set_serf_index(serf->pos, 0);

					serf_log_state_change(serf, SERF_STATE_MOVE_RESOURCE_OUT);
					serf->state = SERF_STATE_MOVE_RESOURCE_OUT;
					serf->s.move_resource_out.res = 1 + RESOURCE_BOAT;
					serf->s.move_resource_out.res_dest = 0;
					serf->s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

					/* Update resource stats. */
					player_t *player = game.player[SERF_PLAYER(serf)];
					player->resource_count[RESOURCE_BOAT] += 1;

					break;
				}
			} else {
				/* Continue building. */
				building->stock[1].available += 1;
				serf->animation = 145;
				serf->counter += 1408;
			}
		}
	}
}

static void
handle_serf_looking_for_geo_spot_state(serf_t *serf)
{
	int tries = 2;
	for (int i = 0; i < 8; i++) {
		int index = ((game_random_int() >> 2) & 0x3f) + 1;
		map_pos_t dest = MAP_POS_ADD(serf->pos,
					     game.spiral_pos_pattern[index]);

		int obj = MAP_OBJ(dest);
		if (obj == MAP_OBJ_NONE) {
			int t1 = MAP_TYPE_DOWN(dest);
			int t2 = MAP_TYPE_UP(dest);
			int t3 = MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(dest));
			int t4 = MAP_TYPE_UP(MAP_MOVE_UP_LEFT(dest));
			if ((t1 >= 11 && t1 < 15) || (t2 >= 11 && t2 < 15) ||
			    (t3 >= 11 && t3 < 15) || (t4 >= 11 && t4 < 15)) {	
				serf_log_state_change(serf, SERF_STATE_FREE_WALKING);
				serf->state = SERF_STATE_FREE_WALKING;
				serf->s.free_walking.dist1 = game.spiral_pattern[2*index];
				serf->s.free_walking.dist2 = game.spiral_pattern[2*index+1];
				serf->s.free_walking.neg_dist1 = -game.spiral_pattern[2*index];
				serf->s.free_walking.neg_dist2 = -game.spiral_pattern[2*index+1];
				serf->s.free_walking.flags = 0;
				serf->tick = game.tick;
				LOGV("serf", "looking for geo spot: found, dist %i, %i.",
				     serf->s.free_walking.dist1,
				     serf->s.free_walking.dist2);
				return;
			}
		} else if (obj >= MAP_OBJ_SIGN_LARGE_GOLD &&
			   obj <= MAP_OBJ_SIGN_EMPTY) {
			tries -= 1;
			if (tries == 0) break;
		}
	}

	serf_log_state_change(serf, SERF_STATE_WALKING);
	serf->state = SERF_STATE_WALKING;
	serf->s.walking.dest = 0;
	serf->s.walking.res = -2;
	serf->s.walking.dir = 0;
	serf->s.walking.wait_counter = 0;
	serf->counter = 0;
}

static void
handle_serf_sampling_geo_spot_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		if (serf->s.free_walking.neg_dist1 == 0 &&
		    MAP_OBJ(serf->pos) == MAP_OBJ_NONE) {
			if (MAP_RES_TYPE(serf->pos) == GROUND_DEPOSIT_NONE ||
			    MAP_RES_AMOUNT(serf->pos) == 0) {
				/* No available resource here. Put empty sign. */
				map_set_object(serf->pos, MAP_OBJ_SIGN_EMPTY, -1);
			} else {
				serf->s.free_walking.neg_dist1 = -1;
				serf->animation = 142;

				/* Select small or large sign with the right resource depicted. */
				int obj = MAP_OBJ_SIGN_LARGE_GOLD +
					2*(MAP_RES_TYPE(serf->pos)-1) +
					(MAP_RES_AMOUNT(serf->pos) < 12 ? 1 : 0);
				map_set_object(serf->pos, obj, -1);

				/* Check whether a new notification should be posted. */
				int show_notification = 1;
				for (int i = 0; i < 60; i++) {
					map_pos_t pos = MAP_POS_ADD(serf->pos,
								    game.spiral_pos_pattern[1+i]);
					if ((MAP_OBJ(pos) >> 1) == (obj >> 1)) {
						show_notification = 0;
						break;
					}
				}

				/* Create notification for found resource. */
				if (show_notification) {
					player_add_notification(game.player[SERF_PLAYER(serf)],
								12 + MAP_RES_TYPE(serf->pos)-1, serf->pos);
				}

				serf->counter += 64;
				continue;
			}
		}

		serf_log_state_change(serf, SERF_STATE_FREE_WALKING);
		serf->state = SERF_STATE_FREE_WALKING;
		serf->s.free_walking.neg_dist1 = -128;
		serf->s.free_walking.neg_dist2 = 0;
		serf->s.free_walking.flags = 0;
		serf->counter = 0;
	}
}

static void
handle_serf_knight_engaging_building_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	if (serf->counter < 0) {
		map_obj_t obj = MAP_OBJ(MAP_MOVE_UP_LEFT(serf->pos));
		if (obj >= MAP_OBJ_SMALL_BUILDING &&
		    obj <= MAP_OBJ_CASTLE) {
			building_t *building = game_get_building(MAP_OBJ_INDEX(MAP_MOVE_UP_LEFT(serf->pos)));
			if (BUILDING_IS_DONE(building) &&
			    (BUILDING_TYPE(building) == BUILDING_HUT ||
			     BUILDING_TYPE(building) == BUILDING_TOWER ||
			     BUILDING_TYPE(building) == BUILDING_FORTRESS ||
			     BUILDING_TYPE(building) == BUILDING_CASTLE) &&
			    BUILDING_PLAYER(building) != SERF_PLAYER(serf) &&
			    building->serf_index != 0) {
				if (BIT_TEST(building->progress, 0)) {
					player_add_notification(game.player[BUILDING_PLAYER(building)],
								(SERF_PLAYER(serf) << 5) | 1, building->pos);
				}

				/* Change state of attacking knight */
				serf->counter = 0;
				serf->state = SERF_STATE_KNIGHT_PREPARE_ATTACKING;
				serf->animation = 168;

				/* Remove knight from stats of defending building */
				if (BUILDING_HAS_INVENTORY(building)) { /* Castle */
					game.player[BUILDING_PLAYER(building)]->castle_knights -= 1;
				} else {
					building->stock[0].available -= 1;
					building->stock[0].requested += 1;
				}

				/* The last knight in the list has to defend. */
				int *def_index = &building->serf_index;
				serf_t *def_serf = game_get_serf(*def_index);
				while (def_serf->s.defending.next_knight != 0) {
					def_index = &def_serf->s.defending.next_knight;
					def_serf = game_get_serf(*def_index);
				}
				*def_index = 0;

				serf->s.attacking.def_index = SERF_INDEX(def_serf);

				/* Change state of defending knight */
				serf_log_state_change(def_serf, SERF_STATE_KNIGHT_LEAVE_FOR_FIGHT);
				def_serf->state = SERF_STATE_KNIGHT_LEAVE_FOR_FIGHT;
				def_serf->s.leaving_building.next_state = SERF_STATE_KNIGHT_PREPARE_DEFENDING;
				def_serf->counter = 0;
				return;
			}
		}

		/* No one to defend this building. Occupy it. */
		serf_log_state_change(serf, SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING);
		serf->state = SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING;
		serf->animation = 179;
		serf->counter = counter_from_animation[serf->animation];
		serf->tick = game.tick;
	}
}

static void
serf_set_fight_outcome(serf_t *attacker, serf_t *defender)
{
	/* Calculate "morale" for attacker. */
	int exp_factor = 1 << (SERF_TYPE(attacker) - SERF_KNIGHT_0);
	int land_factor = 0x1000;
	if (SERF_PLAYER(attacker) != MAP_OWNER(attacker->pos)) {
		land_factor = game.player[SERF_PLAYER(attacker)]->knight_morale;
	}

	int morale = (0x400*exp_factor * land_factor) >> 16;

	/* Calculate "morale" for defender. */
	int def_exp_factor = 1 << (SERF_TYPE(defender) - SERF_KNIGHT_0);
	int def_land_factor = 0x1000;
	if (SERF_PLAYER(defender) != MAP_OWNER(defender->pos)) {
		def_land_factor = game.player[SERF_PLAYER(defender)]->knight_morale;
	}

	int def_morale = (0x400*def_exp_factor * def_land_factor) >> 16;

	int player = -1;
	int value = -1;
	serf_type_t type = -1;
	int r = ((morale + def_morale)*game_random_int()) >> 16;
	if (r < morale) {
		player = SERF_PLAYER(defender);
		value = def_exp_factor;
		type = SERF_TYPE(defender);
		attacker->s.attacking.field_C = 1;
		LOGD("serf", "Fight: %i vs %i (%i). Attacker winning.", morale, def_morale, r);
	} else {
		player = SERF_PLAYER(attacker);
		value = exp_factor;
		type = SERF_TYPE(attacker);
		attacker->s.attacking.field_C = 0;
		LOGD("serf", "Fight: %i vs %i (%i). Defender winning.", morale, def_morale, r);
	}

	game.player[player]->total_military_score -= value;
	game.player[player]->serf_count[type] -= 1;
	attacker->s.attacking.field_B = game_random_int() & 0x70;
}

static void
handle_serf_knight_prepare_attacking(serf_t *serf)
{
	serf_t *def_serf = game_get_serf(serf->s.attacking.def_index);
	if (def_serf->state == SERF_STATE_KNIGHT_PREPARE_DEFENDING) {
		/* Change state of attacker. */
		serf_log_state_change(serf, SERF_STATE_KNIGHT_ATTACKING);
		serf->state = SERF_STATE_KNIGHT_ATTACKING;
		serf->counter = 0;
		serf->tick = game.tick;

		/* Change state of defender. */
		serf_log_state_change(def_serf, SERF_STATE_KNIGHT_DEFENDING);
		def_serf->state = SERF_STATE_KNIGHT_DEFENDING;
		def_serf->counter = 0;

		serf_set_fight_outcome(serf, def_serf);
	}
}

static void
handle_serf_knight_leave_for_fight_state(serf_t *serf)
{
	serf->tick = game.tick;
	serf->counter = 0;

	if (MAP_SERF_INDEX(serf->pos) == SERF_INDEX(serf) ||
	    MAP_SERF_INDEX(serf->pos) == 0) {
		serf_leave_building(serf, 1);
	}
}

static void
handle_serf_knight_prepare_defending_state(serf_t *serf)
{
	serf->counter = 0;
	serf->animation = 84;
}

static void
handle_knight_attacking(serf_t *serf)
{
	const int moves[] =  {
		1, 2, 4, 2, 0, 2, 4, 2, 1, 0, 2, 2, 3, 0, 0, -1,
		3, 2, 2, 3, 0, 4, 1, 3, 2, 4, 2, 2, 3, 0, 0, -1,
		2, 1, 4, 3, 2, 2, 2, 3, 0, 3, 1, 2, 0, 2, 0, -1,
		2, 1, 3, 2, 4, 2, 3, 0, 0, 4, 2, 0, 2, 1, 0, -1,
		3, 1, 0, 2, 2, 1, 0, 2, 4, 2, 2, 3, 0, 0, -1,
		0, 3, 1, 2, 3, 4, 2, 1, 2, 0, 2, 4, 0, 2, 0, -1,
		0, 2, 1, 2, 4, 2, 3, 0, 2, 4, 3, 2, 0, 0, -1,
		0, 0, 1, 4, 3, 2, 2, 1, 2, 0, 0, 4, 3, 0, -1
	};

	const int fight_anim[] = {
		24, 35, 41, 56, 67, 72, 83, 89, 100, 121, 0, 0, 0, 0, 0, 0,
		26, 40, 42, 57, 73, 74, 88, 104, 106, 120, 122, 0, 0, 0, 0, 0,
		17, 18, 23, 33, 34, 38, 39, 98, 102, 103, 113, 114, 118, 119, 0, 0,
		130, 133, 134, 135, 147, 148, 161, 162, 164, 166, 167, 0, 0, 0, 0, 0,
		50, 52, 53, 70, 129, 131, 132, 146, 149, 151, 0, 0, 0, 0, 0, 0
	};

	const int fight_anim_max[] = { 10, 11, 14, 11, 10 };

	serf_t *def_serf = game_get_serf(serf->s.attacking.def_index);

	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	def_serf->tick = game.tick;
	serf->counter -= delta;
	def_serf->counter = serf->counter;

	while (serf->counter < 0) {
		int move = moves[serf->s.attacking.field_B];
		if (move < 0) {
			if (serf->s.attacking.field_C == 0) {
				/* Defender won. */
				if (serf->state == SERF_STATE_KNIGHT_ATTACKING_FREE) {
					serf_log_state_change(def_serf, SERF_STATE_KNIGHT_DEFENDING_VICTORY_FREE);
					def_serf->state = SERF_STATE_KNIGHT_DEFENDING_VICTORY_FREE;

					def_serf->animation = 180;
					def_serf->counter = 0;

					/* Attacker dies. */
					serf_log_state_change(serf, SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE);
					serf->state = SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE;
					serf->animation = 152 + SERF_TYPE(serf);
					serf->counter = 255;
					serf_set_type(serf, SERF_DEAD);
				} else {
					/* Defender returns to building. */
					serf_enter_building(def_serf, -1, 1);

					/* Attacker dies. */
					serf_log_state_change(serf, SERF_STATE_KNIGHT_ATTACKING_DEFEAT);
					serf->state = SERF_STATE_KNIGHT_ATTACKING_DEFEAT;
					serf->animation = 152 + SERF_TYPE(serf);
					serf->counter = 255;
					serf_set_type(serf, SERF_DEAD);
				}
			} else {
				/* Attacker won. */
				if (serf->state == SERF_STATE_KNIGHT_ATTACKING_FREE) {
					serf_log_state_change(serf, SERF_STATE_KNIGHT_ATTACKING_VICTORY_FREE);
					serf->state = SERF_STATE_KNIGHT_ATTACKING_VICTORY_FREE;
					serf->animation = 168;
					serf->counter = 0;

					serf->s.attacking.field_B = def_serf->s.defending_free.field_D;
					serf->s.attacking.field_C = def_serf->s.defending_free.other_dist_col;
					serf->s.attacking.field_D = def_serf->s.defending_free.other_dist_row;
				} else {
					serf_log_state_change(serf, SERF_STATE_KNIGHT_ATTACKING_VICTORY);
					serf->state = SERF_STATE_KNIGHT_ATTACKING_VICTORY;
					serf->animation = 168;
					serf->counter = 0;

					int index = MAP_OBJ_INDEX(MAP_MOVE_UP_LEFT(def_serf->pos));
					building_t *building = game_get_building(index);
					if (!BUILDING_HAS_INVENTORY(building)) building->stock[0].requested -= 1;
				}

				/* Defender dies. */
				def_serf->tick = game.tick;
				def_serf->animation = 147 + SERF_TYPE(serf);
				def_serf->counter = 255;
				serf_set_type(def_serf, SERF_DEAD);
			}
		} else {
			/* Go to next move in fight sequence. */
			serf->s.attacking.field_B += 1;
			if (serf->s.attacking.field_C == 0) move = 4 - move;
			serf->s.attacking.field_D = move;

			int off = (game_random_int() * fight_anim_max[move]) >> 16;
			int a = fight_anim[move*16 + off];

			serf->animation = 146 + ((a >> 4) & 0xf);
			def_serf->animation = 156 + (a & 0xf);
			serf->counter = 72 + (game_random_int() & 0x18);
			def_serf->counter = serf->counter;
		}
	}
}

static void
handle_serf_knight_attacking_victory_state(serf_t *serf)
{
	serf_t *def_serf = game_get_serf(serf->s.attacking.def_index);

	uint16_t delta = game.tick - def_serf->tick;
	def_serf->tick = game.tick;
	def_serf->counter -= delta;

	if (def_serf->counter < 0) {
		game_free_serf(SERF_INDEX(def_serf));
		serf->s.attacking.def_index = 0;

		serf_log_state_change(serf, SERF_STATE_KNIGHT_ENGAGING_BUILDING);
		serf->state = SERF_STATE_KNIGHT_ENGAGING_BUILDING;
		serf->tick = game.tick;
		serf->counter = 0;
	}
}

static void
handle_serf_knight_attacking_defeat_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	if (serf->counter < 0) {
		map_set_serf_index(serf->pos, 0);
		game_free_serf(SERF_INDEX(serf));
	}
}

static void
handle_knight_occupy_enemy_building(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		map_pos_t pos = MAP_MOVE_UP_LEFT(serf->pos);
		if (MAP_OBJ(pos) >= MAP_OBJ_SMALL_BUILDING &&
		    MAP_OBJ(pos) <= MAP_OBJ_CASTLE) {
			building_t *building = game_get_building(MAP_OBJ_INDEX(pos));
			if (!BUILDING_IS_BURNING(building) &&
			    (BUILDING_TYPE(building) == BUILDING_HUT ||
			     BUILDING_TYPE(building) == BUILDING_TOWER ||
			     BUILDING_TYPE(building) == BUILDING_FORTRESS ||
			     BUILDING_TYPE(building) == BUILDING_CASTLE)) {
				if (BUILDING_PLAYER(building) == SERF_PLAYER(serf)) {
					/* Enter building if there is space. */
					if (BUILDING_TYPE(building) != BUILDING_CASTLE) {
						int max_knights = -1;
						switch (BUILDING_TYPE(building)) {
						case BUILDING_HUT: max_knights = 3; break;
						case BUILDING_TOWER: max_knights = 6; break;
						case BUILDING_FORTRESS: max_knights = 12; break;
						default: NOT_REACHED(); break;
						}

						int current = building->stock[0].requested +
							building->stock[0].available;
						if (current < max_knights) {
							/* Enter building */
							serf_enter_building(serf, -1, 0);
							building->stock[0].requested += 1;
							return;
						}
					} else {
						serf_enter_building(serf, -2, 0);
						return;
					}
				} else if (building->serf_index == 0) {
					/* Occupy the building. */
					game_occupy_enemy_building(building, SERF_PLAYER(serf));

					if (BUILDING_TYPE(building) == BUILDING_CASTLE) {
						serf->counter = 0;
					} else {
						/* Enter building */
						serf_enter_building(serf, -1, 0);
						building->stock[0].available = 0;
						building->stock[0].requested = 1;
					}
					return;
				} else {
					serf_log_state_change(serf, SERF_STATE_KNIGHT_ENGAGING_BUILDING);
					serf->state = SERF_STATE_KNIGHT_ENGAGING_BUILDING;
					serf->animation = 167;
					serf->counter = 191;
					return;
				}
			}
		}

		/* Something is wrong. */
		serf_log_state_change(serf, SERF_STATE_LOST);
		serf->state = SERF_STATE_LOST;
		serf->s.lost.field_B = 0;
		serf->counter = 0;
	}
}

static void
handle_state_knight_free_walking(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		/* Check for enemy knights nearby. */
		for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
			map_pos_t pos = MAP_MOVE(serf->pos, d);

			if (MAP_SERF_INDEX(pos) != 0) {
				serf_t *other = game_get_serf(MAP_SERF_INDEX(pos));
				if (SERF_PLAYER(serf) != SERF_PLAYER(other)) {
					if (other->state == SERF_STATE_KNIGHT_FREE_WALKING) {
						pos = MAP_MOVE_LEFT(pos);
						if (serf_can_pass_map_pos(pos)) {
							int dist_col = serf->s.free_walking.dist1;
							int dist_row = serf->s.free_walking.dist2;

							serf_log_state_change(serf, SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE);
							serf->state = SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE;

							serf->s.defending_free.dist_col = dist_col;
							serf->s.defending_free.dist_row = dist_row;
							serf->s.defending_free.other_dist_col = other->s.free_walking.dist1;
							serf->s.defending_free.other_dist_row = other->s.free_walking.dist2;
							serf->s.defending_free.field_D = 1;
							serf->animation = 99;
							serf->counter = 255;

							serf_log_state_change(other, SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE);
							other->state = SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE;
							other->s.attacking.field_D = d;
							other->s.attacking.def_index = SERF_INDEX(serf);
							return;
						}
					} else if (other->state == SERF_STATE_WALKING &&
						   SERF_TYPE(other) >= SERF_KNIGHT_0 &&
						   SERF_TYPE(other) <= SERF_KNIGHT_4) {
						pos = MAP_MOVE_LEFT(pos);
						if (serf_can_pass_map_pos(pos)) {
							int dist_col = serf->s.free_walking.dist1;
							int dist_row = serf->s.free_walking.dist2;

							serf_log_state_change(serf, SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE);
							serf->state = SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE;
							serf->s.defending_free.dist_col = dist_col;
							serf->s.defending_free.dist_row = dist_row;
							serf->s.defending_free.field_D = 0;
							serf->animation = 99;
							serf->counter = 255;

							flag_t *dest = game_get_flag(other->s.walking.dest);
							building_t *building = dest->other_endpoint.b[DIR_UP_LEFT];
							if (!BUILDING_HAS_INVENTORY(building)) {
								building->stock[0].requested -= 1;
							}

							serf_log_state_change(other, SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE);
							other->state = SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE;
							other->s.attacking.field_D = d;
							other->s.attacking.def_index = SERF_INDEX(serf);
							return;
						}
					}
				}
			}
		}

		handle_free_walking_common(serf);
	}
}

static void
handle_state_knight_engage_defending_free(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) serf->counter += 256;
}

static void
handle_state_knight_engage_attacking_free(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		serf_log_state_change(serf, SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE_JOIN);
		serf->state = SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE_JOIN;
		serf->animation = 167;
		serf->counter += 191;
		return;
	}
}

static void
handle_state_knight_engage_attacking_free_join(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		serf_log_state_change(serf, SERF_STATE_KNIGHT_PREPARE_ATTACKING_FREE);
		serf->state = SERF_STATE_KNIGHT_PREPARE_ATTACKING_FREE;
		serf->animation = 168;
		serf->counter = 0;

		serf_t *other = game_get_serf(serf->s.attacking.def_index);
		map_pos_t other_pos = other->pos;
		serf_log_state_change(other, SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE);
		other->state = SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE;
		other->counter = serf->counter;

		/* Adjust distance to final destination. */
		dir_t d = serf->s.attacking.field_D;
		if (d == DIR_RIGHT || d == DIR_DOWN_RIGHT) {
			other->s.defending_free.dist_col -= 1;
		} else if (d == DIR_LEFT || d == DIR_UP_LEFT) {
			other->s.defending_free.dist_col += 1;
		}

		if (d == DIR_DOWN_RIGHT || d == DIR_DOWN) {
			other->s.defending_free.dist_row -= 1;
		} else if (d == DIR_UP_LEFT || d == DIR_UP) {
			other->s.defending_free.dist_row += 1;
		}

		serf_start_walking(other, d, 32, 0);
		map_set_serf_index(other_pos, 0);
		return;
	}
}

static void
handle_state_knight_prepare_attacking_free(serf_t *serf)
{
	serf_t *other = game_get_serf(serf->s.attacking.def_index);
	if (other->state == SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE_WAIT) {
		serf_log_state_change(serf, SERF_STATE_KNIGHT_ATTACKING_FREE);
		serf->state = SERF_STATE_KNIGHT_ATTACKING_FREE;
		serf->counter = 0;

		serf_log_state_change(other, SERF_STATE_KNIGHT_DEFENDING_FREE);
		other->state = SERF_STATE_KNIGHT_DEFENDING_FREE;
		other->counter = 0;

		serf_set_fight_outcome(serf, other);
	}
}

static void
handle_state_knight_prepare_defending_free(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		serf_log_state_change(serf, SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE_WAIT);
		serf->state = SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE_WAIT;
		serf->counter = 0;
		return;
	}
}

static void
handle_knight_attacking_victory_free(serf_t *serf)
{
	serf_t *other = game_get_serf(serf->s.attacking.def_index);

	uint16_t delta = game.tick - other->tick;
	other->tick = game.tick;
	other->counter -= delta;

	while (other->counter < 0) {
		game_free_serf(SERF_INDEX(other));

		int dist_col = serf->s.attacking.field_C;
		int dist_row = serf->s.attacking.field_D;

		serf_log_state_change(serf, SERF_STATE_KNIGHT_ATTACKING_FREE_WAIT);
		serf->state = SERF_STATE_KNIGHT_ATTACKING_FREE_WAIT;

		serf->s.free_walking.dist1 = dist_col;
		serf->s.free_walking.dist2 = dist_row;
		serf->s.free_walking.neg_dist1 = 0;
		serf->s.free_walking.neg_dist2 = 0;

		if (serf->s.attacking.field_B != 0) {
			serf->s.free_walking.flags = 1;
		} else {
			serf->s.free_walking.flags = 0;
		}

		serf->animation = 179;
		serf->counter = 127;
		serf->tick = game.tick;
		return;
	}
}

static void
handle_knight_defending_victory_free(serf_t *serf)
{
	serf->animation = 180;
	serf->counter = 0;
}

static void
handle_serf_knight_attacking_defeat_free_state(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	if (serf->counter < 0) {
		/* Change state of other. */
		serf_t *other = game_get_serf(serf->s.attacking.def_index);
		int dist_col = other->s.defending_free.dist_col;
		int dist_row = other->s.defending_free.dist_row;

		serf_log_state_change(other, SERF_STATE_KNIGHT_FREE_WALKING);
		other->state = SERF_STATE_KNIGHT_FREE_WALKING;

		other->s.free_walking.dist1 = dist_col;
		other->s.free_walking.dist2 = dist_row;
		other->s.free_walking.neg_dist1 = 0;
		other->s.free_walking.neg_dist2 = 0;
		other->s.free_walking.flags = 0;

		other->animation = 179;
		other->counter = 0;
		other->tick = game.tick;

		/* Remove itself. */
		map_set_serf_index(serf->pos, SERF_INDEX(other));
		game_free_serf(SERF_INDEX(serf));
	}
}

static void
handle_knight_attacking_free_wait(serf_t *serf)
{
	uint16_t delta = game.tick - serf->tick;
	serf->tick = game.tick;
	serf->counter -= delta;

	while (serf->counter < 0) {
		if (serf->s.free_walking.flags != 0) {
			serf_log_state_change(serf, SERF_STATE_KNIGHT_FREE_WALKING);
			serf->state = SERF_STATE_KNIGHT_FREE_WALKING;
		} else {
			serf_log_state_change(serf, SERF_STATE_LOST);
			serf->state = SERF_STATE_LOST;
		}

		serf->counter = 0;
		return;
	}
}

static void
handle_serf_state_knight_leave_for_walk_to_fight(serf_t *serf)
{
	serf->tick = game.tick;
	serf->counter = 0;

	if (MAP_SERF_INDEX(serf->pos) != SERF_INDEX(serf) &&
	    MAP_SERF_INDEX(serf->pos) != 0) {
		serf->animation = 82;
		serf->counter = 0;
		return;
	}

	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
	map_pos_t new_pos = MAP_MOVE_DOWN_RIGHT(serf->pos);

	if (MAP_SERF_INDEX(new_pos) == 0) {
		/* For clean state change, save the values first. */
		/* TODO maybe knight_leave_for_walk_to_fight can
		   share leaving_building state vars. */
		int dist_col = serf->s.leave_for_walk_to_fight.dist_col;
		int dist_row = serf->s.leave_for_walk_to_fight.dist_row;
		int field_D = serf->s.leave_for_walk_to_fight.field_D;
		int field_E = serf->s.leave_for_walk_to_fight.field_E;
		serf_state_t next_state = serf->s.leave_for_walk_to_fight.next_state;

		serf_leave_building(serf, 0);
		/* TODO names for leaving_building vars make no sense here. */
		serf->s.leaving_building.field_B = dist_col;
		serf->s.leaving_building.dest = dist_row;
		serf->s.leaving_building.dest2 = field_D;
		serf->s.leaving_building.dir = field_E;
		serf->s.leaving_building.next_state = next_state;
	} else {
		serf_t *other = game_get_serf(MAP_SERF_INDEX(new_pos));
		if (SERF_PLAYER(serf) == SERF_PLAYER(other)) {
			serf->animation = 82;
			serf->counter = 0;
		} else {
			/* Go back to defending the building. */
			int max_capacity = -1;
			switch (BUILDING_TYPE(building)) {
			case BUILDING_HUT:
				serf_log_state_change(serf, SERF_STATE_DEFENDING_HUT);
				serf->state = SERF_STATE_DEFENDING_HUT;
				max_capacity = 3;
				break;
			case BUILDING_TOWER:
				serf_log_state_change(serf, SERF_STATE_DEFENDING_TOWER);
				serf->state = SERF_STATE_DEFENDING_TOWER;
				max_capacity = 6;
				break;
			case BUILDING_FORTRESS:
				serf_log_state_change(serf, SERF_STATE_DEFENDING_FORTRESS);
				serf->state = SERF_STATE_DEFENDING_FORTRESS;
				max_capacity = 12;
				break;
			default:
				NOT_REACHED();
				break;
			}

			int total_knights = building->stock[0].requested + building->stock[0].available;
			if (total_knights < max_capacity) {
				building->stock[0].available += 1;
				serf->s.defending.next_knight = building->serf_index;
				building->serf_index = SERF_INDEX(serf);
			} else {
				serf->animation = 82;
				serf->counter = 0;
			}
		}
	}
}

static void
handle_serf_idle_on_path_state(serf_t *serf)
{
	map_tile_t *tiles = game.map.tiles;

	flag_t *flag = serf->s.idle_on_path.flag;
	int rev_dir = serf->s.idle_on_path.rev_dir;

	/* Set walking dir in field_E. */
	if (FLAG_IS_SCHEDULED(flag, rev_dir)) {
		serf->s.idle_on_path.field_E = (serf->tick & 0xff) + 6;
	} else {
		flag_t *other_flag = flag->other_endpoint.f[rev_dir];
		int other_dir = FLAG_OTHER_END_DIR(flag, rev_dir);
		if (FLAG_IS_SCHEDULED(other_flag, other_dir)) {
			serf->s.idle_on_path.field_E = DIR_REVERSE(rev_dir);
		} else {
			return;
		}
	}

	if (MAP_SERF_INDEX(serf->pos) == 0) {
		tiles[serf->pos].obj &= ~BIT(7);
		map_set_serf_index(serf->pos, SERF_INDEX(serf));

		int dir = serf->s.idle_on_path.field_E;

		serf_log_state_change(serf, SERF_STATE_TRANSPORTING);
		serf->state = SERF_STATE_TRANSPORTING;
		serf->s.walking.res = 0;
		serf->s.walking.wait_counter = 0;
		serf->s.walking.dir = dir;
		serf->tick = game.tick;
		serf->counter = 0;
	} else {
		serf_log_state_change(serf, SERF_STATE_WAIT_IDLE_ON_PATH);
		serf->state = SERF_STATE_WAIT_IDLE_ON_PATH;
	}
}

static void
handle_serf_wait_idle_on_path_state(serf_t *serf)
{
	map_tile_t *tiles = game.map.tiles;

	if (MAP_SERF_INDEX(serf->pos) == 0) {
		/* Duplicate code from handle_serf_idle_on_path_state() */
		tiles[serf->pos].obj &= ~BIT(7);
		map_set_serf_index(serf->pos, SERF_INDEX(serf));

		int dir = serf->s.idle_on_path.field_E;

		serf_log_state_change(serf, SERF_STATE_TRANSPORTING);
		serf->state = SERF_STATE_TRANSPORTING;
		serf->s.walking.res = 0;
		serf->s.walking.wait_counter = 0;
		serf->s.walking.dir = dir;
		serf->tick = game.tick;
		serf->counter = 0;
	}
}

static void
handle_scatter_state(serf_t *serf)
{
	/* Choose a random, empty destination */
	while (1) {
		int r = game_random_int();
		int col = (r & 0xf);
		if (col < 8) col -= 16;
		int row = ((r >> 8) & 0xf);
		if (row < 8) row -= 16;

		map_pos_t dest = MAP_POS_ADD(serf->pos,
					     MAP_POS(col & game.map.col_mask,
						     row & game.map.row_mask));
		if (MAP_OBJ(dest) == 0 && MAP_HEIGHT(dest) > 0) {
			if (SERF_TYPE(serf) >= SERF_KNIGHT_0 &&
			    SERF_TYPE(serf) >= SERF_KNIGHT_4) {
				serf_log_state_change(serf, SERF_STATE_KNIGHT_FREE_WALKING);
				serf->state = SERF_STATE_KNIGHT_FREE_WALKING;
			} else {
				serf_log_state_change(serf, SERF_STATE_FREE_WALKING);
				serf->state = SERF_STATE_FREE_WALKING;
			}

			serf->s.free_walking.dist1 = col;
			serf->s.free_walking.dist2 = row;
			serf->s.free_walking.neg_dist1 = -128;
			serf->s.free_walking.neg_dist2 = -1;
			serf->s.free_walking.flags = 0;
			serf->counter = 0;
			return;
		}
	}
}

static void
handle_serf_finished_building_state(serf_t *serf)
{
	if (MAP_SERF_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)) == 0) {
		serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE);
		serf->state = SERF_STATE_READY_TO_LEAVE;
		serf->s.leaving_building.dest = 0;
		serf->s.leaving_building.field_B = -2;
		serf->s.leaving_building.dir = 0;
		serf->s.leaving_building.next_state = SERF_STATE_WALKING;

		if (MAP_SERF_INDEX(serf->pos) != SERF_INDEX(serf) &&
		    MAP_SERF_INDEX(serf->pos) != 0) {
			serf->animation = 82;
		}
	}
}

static void
handle_serf_wake_at_flag_state(serf_t *serf)
{
	map_tile_t *tiles = game.map.tiles;

	if (MAP_SERF_INDEX(serf->pos) == 0) {
		tiles[serf->pos].obj &= ~BIT(7);
		map_set_serf_index(serf->pos, SERF_INDEX(serf));
		serf->tick = game.tick;
		serf->counter = 0;

		if (SERF_TYPE(serf) == SERF_SAILOR) {
			serf_log_state_change(serf, SERF_STATE_LOST_SAILOR);
			serf->state = SERF_STATE_LOST_SAILOR;
		} else {
			serf_log_state_change(serf, SERF_STATE_LOST);
			serf->state = SERF_STATE_LOST;
			serf->s.lost.field_B = 0;
		}
	}
}

static void
handle_serf_wake_on_path_state(serf_t *serf)
{
	serf_log_state_change(serf, SERF_STATE_WAIT_IDLE_ON_PATH);
	serf->state = SERF_STATE_WAIT_IDLE_ON_PATH;

	for (dir_t d = DIR_UP; d >= DIR_RIGHT; d--) {
		if (BIT_TEST(MAP_PATHS(serf->pos), d)) {
			serf->s.idle_on_path.field_E = d;
			break;
		}
	}
}

static void
handle_serf_defending_state(serf_t *serf, const int training_params[])
{
	switch (SERF_TYPE(serf)) {
	case SERF_KNIGHT_0:
	case SERF_KNIGHT_1:
	case SERF_KNIGHT_2:
	case SERF_KNIGHT_3:
		train_knight(serf, training_params[SERF_TYPE(serf)-SERF_KNIGHT_0]);
	case SERF_KNIGHT_4: /* Cannot train anymore. */
		break;
	default:
		NOT_REACHED();
		break;
	}
}

static void
handle_serf_defending_hut_state(serf_t *serf)
{
	const int training_params[] = { 250, 125, 62, 31 };
	handle_serf_defending_state(serf, training_params);
}

static void
handle_serf_defending_tower_state(serf_t *serf)
{
	const int training_params[] = { 1000, 500, 250, 125 };
	handle_serf_defending_state(serf, training_params);
}

static void
handle_serf_defending_fortress_state(serf_t *serf)
{
	const int training_params[] = { 2000, 1000, 500, 250 };
	handle_serf_defending_state(serf, training_params);
}

static void
handle_serf_defending_castle_state(serf_t *serf)
{
	const int training_params[] = { 4000, 2000, 1000, 500 };
	handle_serf_defending_state(serf, training_params);
}

void
update_serf(serf_t *serf)
{
	switch (serf->state) {
	case SERF_STATE_NULL: /* 0 */
		break;
	case SERF_STATE_WALKING:
		handle_serf_walking_state(serf);
		break;
	case SERF_STATE_TRANSPORTING:
		handle_serf_transporting_state(serf);
		break;
	case SERF_STATE_IDLE_IN_STOCK:
		handle_serf_idle_in_stock_state(serf);
		break;
	case SERF_STATE_ENTERING_BUILDING:
		handle_serf_entering_building_state(serf);
		break;
	case SERF_STATE_LEAVING_BUILDING: /* 5 */
		handle_serf_leaving_building_state(serf);
		break;
	case SERF_STATE_READY_TO_ENTER:
		handle_serf_ready_to_enter_state(serf);
		break;
	case SERF_STATE_READY_TO_LEAVE:
		handle_serf_ready_to_leave_state(serf);
		break;
	case SERF_STATE_DIGGING:
		handle_serf_digging_state(serf);
		break;
	case SERF_STATE_BUILDING:
		handle_serf_building_state(serf);
		break;
	case SERF_STATE_BUILDING_CASTLE: /* 10 */
		handle_serf_building_castle_state(serf);
		break;
	case SERF_STATE_MOVE_RESOURCE_OUT:
		handle_serf_move_resource_out_state(serf);
		break;
	case SERF_STATE_WAIT_FOR_RESOURCE_OUT:
		handle_serf_wait_for_resource_out_state(serf);
		break;
	case SERF_STATE_DROP_RESOURCE_OUT:
		handle_serf_drop_resource_out_state(serf);
		break;
	case SERF_STATE_DELIVERING:
		handle_serf_delivering_state(serf);
		break;
	case SERF_STATE_READY_TO_LEAVE_INVENTORY: /* 15 */
		handle_serf_ready_to_leave_inventory_state(serf);
		break;
	case SERF_STATE_FREE_WALKING:
		handle_serf_free_walking_state(serf);
		break;
	case SERF_STATE_LOGGING:
		handle_serf_logging_state(serf);
		break;
	case SERF_STATE_PLANNING_LOGGING:
		handle_serf_planning_logging_state(serf);
		break;
	case SERF_STATE_PLANNING_PLANTING:
		handle_serf_planning_planting_state(serf);
		break;
	case SERF_STATE_PLANTING: /* 20 */
		handle_serf_planting_state(serf);
		break;
	case SERF_STATE_PLANNING_STONECUTTING:
		handle_serf_planning_stonecutting(serf);
		break;
	case SERF_STATE_STONECUTTER_FREE_WALKING:
		handle_stonecutter_free_walking(serf);
		break;
	case SERF_STATE_STONECUTTING:
		handle_serf_stonecutting_state(serf);
		break;
	case SERF_STATE_SAWING:
		handle_serf_sawing_state(serf);
		break;
	case SERF_STATE_LOST: /* 25 */
		handle_serf_lost_state(serf);
		break;
	case SERF_STATE_LOST_SAILOR:
		handle_lost_sailor(serf);
		break;
	case SERF_STATE_FREE_SAILING:
		handle_free_sailing(serf);
		break;
	case SERF_STATE_ESCAPE_BUILDING:
		handle_serf_escape_building_state(serf);
		break;
	case SERF_STATE_MINING:
		handle_serf_mining_state(serf);
		break;
	case SERF_STATE_SMELTING: /* 30 */
		handle_serf_smelting_state(serf);
		break;
	case SERF_STATE_PLANNING_FISHING:
		handle_serf_planning_fishing_state(serf);
		break;
	case SERF_STATE_FISHING:
		handle_serf_fishing_state(serf);
		break;
	case SERF_STATE_PLANNING_FARMING:
		handle_serf_planning_farming_state(serf);
		break;
	case SERF_STATE_FARMING:
		handle_serf_farming_state(serf);
		break;
	case SERF_STATE_MILLING: /* 35 */
		handle_serf_milling_state(serf);
		break;
	case SERF_STATE_BAKING:
		handle_serf_baking_state(serf);
		break;
	case SERF_STATE_PIGFARMING:
		handle_serf_pigfarming_state(serf);
		break;
	case SERF_STATE_BUTCHERING:
		handle_serf_butchering_state(serf);
		break;
	case SERF_STATE_MAKING_WEAPON:
		handle_serf_making_weapon_state(serf);
		break;
	case SERF_STATE_MAKING_TOOL: /* 40 */
		handle_serf_making_tool_state(serf);
		break;
	case SERF_STATE_BUILDING_BOAT:
		handle_serf_building_boat_state(serf);
		break;
	case SERF_STATE_LOOKING_FOR_GEO_SPOT:
		handle_serf_looking_for_geo_spot_state(serf);
		break;
	case SERF_STATE_SAMPLING_GEO_SPOT:
		handle_serf_sampling_geo_spot_state(serf);
		break;
	case SERF_STATE_KNIGHT_ENGAGING_BUILDING:
		handle_serf_knight_engaging_building_state(serf);
		break;
	case SERF_STATE_KNIGHT_PREPARE_ATTACKING: /* 45 */
		handle_serf_knight_prepare_attacking(serf);
		break;
	case SERF_STATE_KNIGHT_LEAVE_FOR_FIGHT:
		handle_serf_knight_leave_for_fight_state(serf);
		break;
	case SERF_STATE_KNIGHT_PREPARE_DEFENDING:
		handle_serf_knight_prepare_defending_state(serf);
		break;
	case SERF_STATE_KNIGHT_ATTACKING:
	case SERF_STATE_KNIGHT_ATTACKING_FREE:
		handle_knight_attacking(serf);
		break;
	case SERF_STATE_KNIGHT_DEFENDING:
	case SERF_STATE_KNIGHT_DEFENDING_FREE:
		/* The actual fight update is handled for the attacking knight. */
		break;
	case SERF_STATE_KNIGHT_ATTACKING_VICTORY: /* 50 */
		handle_serf_knight_attacking_victory_state(serf);
		break;
	case SERF_STATE_KNIGHT_ATTACKING_DEFEAT:
		handle_serf_knight_attacking_defeat_state(serf);
		break;
	case SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING:
		handle_knight_occupy_enemy_building(serf);
		break;
	case SERF_STATE_KNIGHT_FREE_WALKING:
		handle_state_knight_free_walking(serf);
		break;
	case SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE:
		handle_state_knight_engage_defending_free(serf);
		break;
	case SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE:
		handle_state_knight_engage_attacking_free(serf);
		break;
	case SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE_JOIN:
		handle_state_knight_engage_attacking_free_join(serf);
		break;
	case SERF_STATE_KNIGHT_PREPARE_ATTACKING_FREE:
		handle_state_knight_prepare_attacking_free(serf);
		break;
	case SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE:
		handle_state_knight_prepare_defending_free(serf);
		break;
	case SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE_WAIT:
		/* Nothing to do for this state. */
		break;
	case SERF_STATE_KNIGHT_ATTACKING_VICTORY_FREE:
		handle_knight_attacking_victory_free(serf);
		break;
	case SERF_STATE_KNIGHT_DEFENDING_VICTORY_FREE:
		handle_knight_defending_victory_free(serf);
		break;
	case SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE:
		handle_serf_knight_attacking_defeat_free_state(serf);
		break;
	case SERF_STATE_KNIGHT_ATTACKING_FREE_WAIT:
		handle_knight_attacking_free_wait(serf);
		break;
	case SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT: /* 65 */
		handle_serf_state_knight_leave_for_walk_to_fight(serf);
		break;
	case SERF_STATE_IDLE_ON_PATH:
		handle_serf_idle_on_path_state(serf);
		break;
	case SERF_STATE_WAIT_IDLE_ON_PATH:
		handle_serf_wait_idle_on_path_state(serf);
		break;
	case SERF_STATE_WAKE_AT_FLAG:
		handle_serf_wake_at_flag_state(serf);
		break;
	case SERF_STATE_WAKE_ON_PATH:
		handle_serf_wake_on_path_state(serf);
		break;
	case SERF_STATE_DEFENDING_HUT: /* 70 */
		handle_serf_defending_hut_state(serf);
		break;
	case SERF_STATE_DEFENDING_TOWER:
		handle_serf_defending_tower_state(serf);
		break;
	case SERF_STATE_DEFENDING_FORTRESS:
		handle_serf_defending_fortress_state(serf);
		break;
	case SERF_STATE_SCATTER:
		handle_scatter_state(serf);
		break;
	case SERF_STATE_FINISHED_BUILDING:
		handle_serf_finished_building_state(serf);
		break;
	case SERF_STATE_DEFENDING_CASTLE: /* 75 */
		handle_serf_defending_castle_state(serf);
		break;
	default:
		LOGD("serf", "Serf state %d isn't processed", serf->state);
		serf->state = SERF_STATE_NULL;
	}
}
