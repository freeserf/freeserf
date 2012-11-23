/* serf.c */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "serf.h"
#include "globals.h"
#include "game.h"
#include "random.h"
#include "viewport.h"
#include "misc.h"
#include "debug.h"


static const int counter_from_animation[] = {
	511, 447, 383, 319, 255, 319, 511, 767,
	1023, 511, 447, 383, 319, 255, 319, 511,
	767, 1023, 511, 447, 383, 319, 255, 319,
	511, 767, 1023, 511, 447, 383, 319, 255,
	319, 511, 767, 1023, 511, 447, 383, 319,
	255, 319, 511, 767, 1023, 511, 447, 383,
	319, 255, 319, 511, 767, 1023, 511, 447,
	383, 319, 255, 319, 511, 767, 1023, 511,
	447, 383, 319, 255, 319, 511, 767, 1023,
	511, 447, 383, 319, 255, 319, 511, 767,
	1023, 127, 127, 127, 127, 127, 127, 383,
	383, 255, 223, 191, 159, 127, 159, 255,
	383, 511, 255, 255, 255, 0, 767, 511,
	511, 767, 1023, 639, 639, 1023, 63, 63,
	63, 63, 63, 63, 1023, 31, 767, 767,
	255, 191, 127, 1535, 2367, 383, 303, 303,
	383, 383, 383, 767, 767, 127, 127, 1471,
	1983, 383, 767, 383, 1535, 783, 63, 575,
	1535, 1407, 159, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 127, 191,
	7, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 127, 7
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
	[SERF_STATE_22] = "STATE 22",
	[SERF_STATE_STONECUTTING] = "STONECUTTING",
	[SERF_STATE_SAWING] = "SAWING",
	[SERF_STATE_LOST] = "LOST",
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
	[SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT] = "KNIGHT LEAVE FOR WALK TO FIGHT",
	[SERF_STATE_IDLE_ON_PATH] = "IDLE ON PATH",
	[SERF_STATE_WAIT_IDLE_ON_PATH] = "WAIT IDLE ON PATH",
	[SERF_STATE_WAKE_AT_FLAG] = "WAKE AT FLAG",
	[SERF_STATE_WAKE_ON_PATH] = "WAKE ON PATH",
	[SERF_STATE_DEFENDING_HUT] = "DEFENDING HUT",
	[SERF_STATE_DEFENDING_TOWER] = "DEFENDING TOWER",
	[SERF_STATE_DEFENDING_FORTRESS] = "DEFENDING FORTRESS",
	[SERF_STATE_73] = "STATE 73",
	[SERF_STATE_FINISHED_BUILDING] = "FINISHED BUILDING",
	[SERF_STATE_DEFENDING_CASTLE] = "DEFENDING CASTLE"
};


const char *
serf_get_state_name(serf_state_t state)
{
	return serf_state_name[state];
}

static int
train_knight(serf_t *serf, int p)
{
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
	serf->counter -= delta;

	while (serf->counter < 0) {
		if (random_int() < p) {
			/* Level up */
			serf_type_t old_type = SERF_TYPE(serf);
			serf->type = (serf->type & 0x83) | ((old_type + 1) << 2);
			player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
			sett->serf_count[old_type] -= 1;
			sett->serf_count[old_type+1] += 1;
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
	    inventory->serfs[SERF_4] >= 3) {
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

		inventory->serfs[SERF_4] += 1;

		serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE_INVENTORY);
		serf->state = SERF_STATE_READY_TO_LEAVE_INVENTORY;
		serf->s.ready_to_leave_inventory.mode = -3;
		serf->s.ready_to_leave_inventory.inv_index = INVENTORY_INDEX(inventory);
		/* TODO immediate switch to next state. */
	}
}

static int
get_walking_animation(int h_diff, dir_t dir)
{
	return 4 + h_diff + 9*dir;
}

/* Preconditon: serf is in WALKING or TRANSPORTING state */
static void
serf_change_direction(serf_t *serf, int dir, int alt_end)
{
	map_pos_t new_pos = MAP_MOVE(serf->pos, dir);
	int animation = 0;

	if (MAP_SERF_INDEX(new_pos) == 0) {
		/* Change direction, not occupied. */
		map_set_serf_index(serf->pos, 0);
		animation = get_walking_animation(MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos), dir);
		serf->s.walking.dir = DIR_REVERSE(dir);
	} else {
		/* Direction is occupied. */
		serf_t *other_serf = game_get_serf(MAP_SERF_INDEX(new_pos));
		if (dir == DIR_LEFT || dir == DIR_UP_LEFT || dir == DIR_UP ||
		    (other_serf->state != SERF_STATE_TRANSPORTING &&
		     other_serf->state != SERF_STATE_WALKING) ||
		    other_serf->s.walking.dir != DIR_REVERSE(dir)-6) {
			/* Wait for other serf */
			serf->animation = 81 + dir;
			serf->counter = counter_from_animation[serf->animation];
			serf->s.walking.dir = dir-6;
			return;
		}

		/* Do the switch */
		other_serf->pos = serf->pos;
		map_set_serf_index(other_serf->pos, MAP_SERF_INDEX(new_pos));
		other_serf->animation = get_walking_animation(MAP_HEIGHT(other_serf->pos) - MAP_HEIGHT(new_pos),
							      DIR_REVERSE(dir));
		other_serf->s.walking.dir = dir;
		other_serf->counter = counter_from_animation[other_serf->animation];

		animation = get_walking_animation(MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos), 6+dir);
		serf->s.walking.dir = DIR_REVERSE(dir);
	}

	if (!alt_end) serf->s.walking.wait_counter = 0;
	serf->pos = new_pos;
	map_set_serf_index(new_pos, SERF_INDEX(serf));
	serf->counter += counter_from_animation[animation];
	if (alt_end && serf->counter < 0) {
		if (MAP_HAS_FLAG(new_pos)) serf->counter = 0;
		else LOGD("serf", "unhandled jump to 31B82.");
	}
	serf->animation = animation;
}

static int
flag_search_inventory_search_cb(flag_t *flag, int *dest_index)
{
	if (BIT_TEST(flag->bld_flags, 7)) { /* Has inventory */
		building_t *building = flag->other_endpoint.b[DIR_UP_LEFT];
		*dest_index = building->flg_index;
		return 1;
	}

	return 0;
}

static int
flag_search_inventory(int flag_index)
{
	flag_t *src = game_get_flag(flag_index);

	int dest_index = -1;
	flag_search_single(src, (flag_search_func *)flag_search_inventory_search_cb, 0, &dest_index);

	return dest_index;
}

/* Precondition: serf state is in WALKING or TRANSPORTING state */
static void
serf_transporter_move_to_flag(serf_t *serf, flag_t *flag)
{
	int dir = serf->s.walking.dir;
	if (BIT_TEST(flag->other_end_dir[dir], 7)) {
		/* Fetch resource from flag */
		serf->s.walking.wait_counter = 0;
		int res_index = flag->other_end_dir[dir] & 7;

		if (serf->s.walking.res == 0) {
			/* Pick up resource. */
			serf->s.walking.res = flag->res_waiting[res_index] & 0x1f;
			serf->s.walking.dest = flag->res_dest[res_index];
			flag->res_waiting[res_index] = 0;
		} else {
			/* Switch resources and destination. */
			flag->endpoint |= BIT(7);

			int res = serf->s.walking.res;
			serf->s.walking.res = flag->res_waiting[res_index] & 0x1f;
			flag->res_waiting[res_index] = res;

			int dest = serf->s.walking.dest;
			serf->s.walking.dest = flag->res_dest[res_index];
			flag->res_dest[res_index] = dest;
		}

		/* Find next resource to be picked up */
		player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
		flag_prioritize_pickup(flag, dir, sett->flag_prio);
	} else if (serf->s.walking.res != 0) {
		/* Drop resource at flag */
		int free_slot = -1;
		for (int i = 0; i < 8; i++) {
			if (flag->res_waiting[i] == 0) {
				free_slot = i;
				break;
			}
		}

		if (free_slot > -1) {
			flag->endpoint |= BIT(7);
			flag->res_waiting[free_slot] = serf->s.walking.res;
			flag->res_dest[free_slot] = serf->s.walking.dest;
			serf->s.walking.res = 0;
		}
	}

	serf_change_direction(serf, dir, 1);
}

static const int road_bld_slope_arr[] = {
	/* Finished building */
	5, 18, 18, 15, 18, 22, 22, 22,
	22, 18, 16, 18, 1, 10, 1, 15,
	15, 16, 15, 15, 10, 15, 20, 15,
	18, 0, 0, 0, 0, 0, 0, 0,

	/* Unfinished */
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1
};

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
serf_start_walking(serf_t *serf, dir_t dir, int slope)
{
	map_pos_t new_pos = MAP_MOVE(serf->pos, dir);
	map_set_serf_index(serf->pos, 0);
	map_set_serf_index(new_pos, SERF_INDEX(serf));
	serf->animation = get_walking_animation(MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos), dir);
	serf->counter += (slope * counter_from_animation[serf->animation]) >> 5;
	serf->pos = new_pos;
}

static void
handle_serf_walking_state_dest_reached(serf_t *serf)
{
	/* Destination reached. */
	if (serf->s.walking.res < 0) {
		building_t *building = game_get_building(MAP_OBJ_INDEX(MAP_MOVE_UP_LEFT(serf->pos)));
		building->serf |= BIT(6);
		if (BIT_TEST(building->serf, 7)) building->serf_index = SERF_INDEX(serf);
		building->serf &= ~BIT(7);

		if (MAP_SERF_INDEX(MAP_MOVE_UP_LEFT(serf->pos)) != 0) {
			serf->animation = 85;
			serf->counter = 0;
			serf_log_state_change(serf, SERF_STATE_READY_TO_ENTER);
			serf->state = SERF_STATE_READY_TO_ENTER;
		} else {
			serf->counter = 0;
			serf_start_walking(serf, DIR_UP_LEFT, 32);
			serf->anim = globals.anim;
			serf_log_state_change(serf, SERF_STATE_ENTERING_BUILDING);
			serf->state = SERF_STATE_ENTERING_BUILDING;
			int slope = road_bld_slope_arr[(building->bld >> 2) & 0x3f];
			serf->s.entering_building.slope_len = (slope * serf->counter) >> 5;
		}
	} else if (serf->s.walking.res == 6) {
		serf_log_state_change(serf, SERF_STATE_LOOKING_FOR_GEO_SPOT);
		serf->state = SERF_STATE_LOOKING_FOR_GEO_SPOT;
		serf->counter = 0;
	} else {
		flag_t *flag = game_get_flag(MAP_OBJ_INDEX(serf->pos));
		dir_t dir = serf->s.walking.res;
		flag_t *other_flag = flag->other_endpoint.f[dir];
		dir_t other_dir = (flag->other_end_dir[dir] >> 3) & 7;

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

	serf->s.walking.wait_counter += 1;
	if ((!MAP_HAS_FLAG(serf->pos) && serf->s.walking.wait_counter >= 10) ||
	    serf->s.walking.wait_counter >= 50) {
		map_pos_t pos = serf->pos;
		for (int i = 0; i < 100; i++) {
			pos = MAP_MOVE(pos, dir);

			if (MAP_SERF_INDEX(pos) == 0) break;
			else if (MAP_SERF_INDEX(pos) == SERF_INDEX(serf)) {
				dir = DIR_REVERSE(dir);
				break;
			}

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

		dir = serf->s.walking.dir + 6;
	}

	serf_change_direction(serf, dir, 0);
}

static void
handle_serf_walking_state(serf_t *serf)
{
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
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
					if (BIT_TEST(src->endpoint, 5-i)) {
						flag_t *other_flag = src->other_endpoint.f[5-i];
						other_flag->search_dir = 5-i;
						flag_search_add_source(&search, other_flag);
					}
				}
				int r = flag_search_execute(&search,
							    (flag_search_func *)handle_serf_walking_state_search_cb,
							    0, serf);
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
			if (building->stock1 != 0xff) building->stock1 -= 1;
		} else if (serf->s.walking.res != 6) {
			flag_t *flag = game_get_flag(serf->s.walking.dest);
			dir_t d = serf->s.walking.res;
			flag->length[d] &= ~BIT(7);
			flag->other_endpoint.f[d]->length[(flag->other_end_dir[d] >> 3) & 7] &= ~BIT(7);
		}

		serf->s.walking.res = -2;
		serf->s.walking.dest = 0;
		serf->counter = 0;
	}
}

static void
handle_serf_transporting_state(serf_t *serf)
{
	map_2_t *map_data = MAP_2_DATA(globals.map_mem2_ptr);

	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
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
			int other_dir = (flag->other_end_dir[rev_dir] >> 3) & 7;

			if (BIT_TEST(flag->other_end_dir[rev_dir], 7)) {
				serf_change_direction(serf, dir, 1);
				return;
			}

			serf->animation = 110 + serf->s.walking.dir;
			serf->counter = counter_from_animation[serf->animation];
			serf->s.walking.dir -= 6;

			if ((flag->length[rev_dir] & 0xf) > 1) {
				serf->s.walking.wait_counter += 1;
				if (serf->s.walking.wait_counter > 3) {
					flag->length[rev_dir] -= 1;
					other_flag->length[other_dir] -= 1;
					serf->s.walking.wait_counter = -1;
				}
			} else {
				if (!BIT_TEST(other_flag->other_end_dir[other_dir], 7)) {
					/* TODO Don't use anim as state var */
					serf->anim = (serf->anim & 0xff00) | (serf->s.walking.dir & 0xff);
					serf_log_state_change(serf, SERF_STATE_IDLE_ON_PATH);
					serf->state = SERF_STATE_IDLE_ON_PATH;
					serf->s.idle_on_path.rev_dir = rev_dir;
					serf->s.idle_on_path.flag = flag;
					map_data[serf->pos].u.s.field_1 = BIT(7) | SERF_PLAYER(serf);
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
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
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
				flag->bld_flags = BIT(7) | BIT(6); /* Why set these here? */
				flag->bld2_flags = BIT(7);

				serf_log_state_change(serf, SERF_STATE_WAIT_FOR_RESOURCE_OUT);
				serf->state = SERF_STATE_WAIT_FOR_RESOURCE_OUT;
				serf->counter = 63;
				serf->type = (SERF_4 << 2) | (serf->type & 83);
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
				serf->s.digging.target_h = building->u.s.level;
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
		case SERF_4:
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
					int flag_index = MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos));
					flag_t *flag = game_get_flag(flag_index);
					flag->bld_flags = 0;
					flag->bld2_flags = BIT(5); /* request lumber */
					flag->stock2_prio = 0;
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

				if (bld_type == BUILDING_STONEMINE) {
					/*player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
					  sett->field_163 |= BIT(5);*/
				}

				if (serf->s.entering_building.field_B != 0) {
					building->serf |= BIT(4);
					building->serf &= ~BIT(3);

					flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
					flag->bld_flags = BIT(0); /* Want food delivered */
					flag->bld2_flags = 0;
					flag->stock1_prio = 0;
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
					flag->bld_flags = BIT(2); /* Request coal */
					flag->stock1_prio = 0;

					if (BUILDING_TYPE(building) == BUILDING_STEELSMELTER) {
						flag->bld2_flags = BIT(1); /* Request iron ore */
					} else {
						flag->bld2_flags = BIT(0); /* Request gold ore */
					}

					flag->stock2_prio = 0;
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

					building->stock2 = 1;
					flag->bld_flags = BIT(4); /* Request wheat */
					flag->bld2_flags = 0;
					flag->stock1_prio = 0;

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
					flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
					flag->bld_flags = BIT(3); /* Request pigs */
					flag->bld2_flags = 0;
					flag->stock1_prio = 0;
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
					flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
					flag->bld_flags = BIT(4); /* Request wheat */
					flag->bld2_flags = 0;
					flag->stock1_prio = 0;
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
					flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
					flag->bld_flags = BIT(5); /* Request flour */
					flag->bld2_flags = 0;
					flag->stock1_prio = 0;
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
					flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
					flag->bld_flags = BIT(1); /* Request planks */
					flag->bld2_flags = 0;
					flag->stock1_prio = 0;
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
					flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
					flag->bld_flags = BIT(1); /* Request planks */
					flag->bld2_flags = BIT(2); /* Request steel */
					flag->stock1_prio = 0;
					flag->stock2_prio = 0;
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
					flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
					flag->bld_flags = BIT(2); /* Request coal */
					flag->bld2_flags = BIT(2); /* Request steel */
					flag->stock1_prio = 0;
					flag->stock2_prio = 0;
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
			inventory->spawn_priority += 1;

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
				if (BIT_TEST(building->serf, 5)) { /* Burning */
					serf_log_state_change(serf, SERF_STATE_LOST);
					serf->state = SERF_STATE_LOST;
					serf->counter = 0;
				} else {
					map_set_serf_index(serf->pos, 0);

					/* Prepend to knight list - TODO accessing state before state change */
					serf->s.defending.next_knight = building->serf_index;
					building->serf_index = SERF_INDEX(serf);

					if (building->stock1 == 0xff) { /* Castle */
						serf_log_state_change(serf, SERF_STATE_DEFENDING_CASTLE);
						serf->state = SERF_STATE_DEFENDING_CASTLE;
						serf->counter = 6000;

						globals.player_sett[BUILDING_PLAYER(building)]->castle_knights += 1;
						return;
					}

					building->stock1 += 0xf;

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

					/* Test whether building is already occupied by knights */
					if (!BIT_TEST(building->serf, 4)) { /* Not occupied */
						building->serf |= BIT(4);

						int mil_type = -1;
						switch (BUILDING_TYPE(building)) {
						case BUILDING_HUT: mil_type = 0; break;
						case BUILDING_TOWER: mil_type = 1; break;
						case BUILDING_FORTRESS: mil_type = 2; break;
						default: NOT_REACHED(); break;
						}

						player_add_notification(globals.player_sett[BUILDING_PLAYER(building)],
									(mil_type << 5) | 6, building->pos);

						flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(building->pos)));
						flag->bld_flags = 0;
						flag->bld2_flags = BIT(3);
						flag->stock2_prio = 0;

						/* TODO Save total land amount and building count for each player. */
						game_update_land_ownership(MAP_COORD_ARGS(building->pos));
						/* TODO Create notifications if land amount or building count changed. */
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
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
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
			   serf->state == SERF_STATE_KNIGHT_FREE_WALKING) {
			int dist1 = serf->s.leaving_building.field_B;
			int dist2 = serf->s.leaving_building.dest;
			int neg_dist1 = serf->s.leaving_building.dest2;
			int neg_dist2 = serf->s.leaving_building.dir;
			serf->s.free_walking.dist1 = dist1;
			serf->s.free_walking.dist2 = dist2;
			serf->s.free_walking.neg_dist1 = neg_dist1;
			serf->s.free_walking.neg_dist2 = neg_dist2;
			serf->s.free_walking.flags = 0;
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

	serf->counter = 0;
	serf_start_walking(serf, DIR_UP_LEFT, 32);
	serf->anim = globals.anim;

	building_t *building = game_get_building(MAP_OBJ_INDEX(new_pos));

	int slope = road_bld_slope_arr[(building->bld >> 2) & 0x3f];
	int field_B = serf->s.ready_to_enter.field_B;

	serf_log_state_change(serf, SERF_STATE_ENTERING_BUILDING);
	serf->state = SERF_STATE_ENTERING_BUILDING;
	serf->s.entering_building.slope_len = (slope * serf->counter) >> 5;
	serf->s.entering_building.field_B = field_B;
}

static void
handle_serf_ready_to_leave_state(serf_t *serf)
{
	map_pos_t new_pos = MAP_MOVE_DOWN_RIGHT(serf->pos);

	if ((MAP_SERF_INDEX(serf->pos) != SERF_INDEX(serf) &&
	     MAP_SERF_INDEX(serf->pos) != 0) ||
	    MAP_SERF_INDEX(new_pos) != 0) {
		serf->animation = 82;
		serf->counter = 0;
		return;
	}

	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
	int slope = 31 - road_bld_slope_arr[(building->bld >> 2) & 0x3f];
	serf->counter = 0;
	serf_start_walking(serf, DIR_DOWN_RIGHT, slope);
	serf->anim = globals.anim;

	serf_log_state_change(serf, SERF_STATE_LEAVING_BUILDING);
	serf->state = SERF_STATE_LEAVING_BUILDING;
}

static void
handle_serf_digging_state(serf_t *serf)
{
	const int h_diff[] = {
		-1, 1, -2, 2, -3, 3, -4, 4,
		-5, 5, -6, 6, -7, 7, -8, 8
	};

	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
	serf->counter -= delta;

	while (serf->counter < 0) {
		serf->s.digging.substate -= 1;
		if (serf->s.digging.substate < 0) {
			LOGV("serf", "substate -1: wait for serf.");
			int d = serf->s.digging.dig_pos;
			dir_t dir = (d == 0) ? DIR_UP : 6-d;
			map_pos_t new_pos = MAP_MOVE(serf->pos, dir);

			if (MAP_SERF_INDEX(new_pos) != 0) {
				serf->counter = 127;
				serf->s.digging.substate = 0;
				return;
			}

			map_set_serf_index(serf->pos, 0);
			map_set_serf_index(new_pos, SERF_INDEX(serf));
			if (d != 0) {
				serf->animation = get_walking_animation(MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos), dir);
			} else {
				serf->animation = MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos);
			}
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
				serf_start_walking(serf, dir, 32);
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
						serf_start_walking(serf, dir, 32);
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

	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
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

				flag_t *flag = game_get_flag(building->flg_index);
				building->u.flag = flag;

				switch (type) {
				case BUILDING_HUT:
				case BUILDING_TOWER:
				case BUILDING_FORTRESS:
					game_calculate_military_flag_state(building);
					break;
				default:
					break;
				}

				flag->bld_flags = 0;
				flag->bld2_flags = 0;

				/* Update player_sett fields. */
				player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
				sett->total_building_score += building_get_score_from_type(type);
				sett->completed_building_count[type] += 1;
				sett->incomplete_building_count[type] -= 1;

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
					if (((building->stock1 >> 4) & 0xf) == 0) {
						serf->counter += 256;
						if (serf->counter < 0) serf->counter = 255;
						return;
					}

					building->stock1 -= 0x10;
					building->u.s.planks_needed -= 1;
				} else {
					/* Stone */
					if (((building->stock2 >> 4) & 0xf) == 0) {
						serf->counter += 256;
						if (serf->counter < 0) serf->counter = 255;
						return;
					}

					building->stock2 -= 0x10;
					building->u.s.stone_needed -= 1;
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
				if (((building->stock1 >> 4) & 0xf) == 0) {
					serf->counter += 256;
					if (serf->counter < 0) serf->counter = 255;
					return;
				}

				building->stock1 -= 0x10;
				building->u.s.planks_needed -= 1;
			} else {
				/* Stone */
				if (((building->stock2 >> 4) & 0xf) == 0) {
					serf->counter += 256;
					if (serf->counter < 0) serf->counter = 255;
					return;
				}

				building->stock2 -= 0x10;
				building->u.s.stone_needed -= 1;
			}

			serf->s.building.material_step += 1;
			serf->s.building.counter = 8;
			serf->s.building.mode = -1;
		}

		int rnd = (random_int() & 3) + 102;
		if (BIT_TEST(serf->s.building.material_step, 7)) rnd += 4;
		serf->animation = rnd;
		serf->counter += counter_from_animation[serf->animation];
	}
}

static void
handle_serf_building_castle_state(serf_t *serf)
{
	int progress_delta = (uint16_t)(globals.anim - serf->anim) << 7;
	serf->anim = globals.anim;

	inventory_t *inventory = game_get_inventory(serf->s.building_castle.inv_index);
	building_t *building = game_get_building(inventory->bld_index);
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
	if ((MAP_SERF_INDEX(serf->pos) != SERF_INDEX(serf) &&
	     MAP_SERF_INDEX(serf->pos) != 0) ||
	    MAP_SERF_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)) != 0) {
		/* Occupied by serf, wait */
		serf->animation = 82;
		serf->counter = 0;
		return;
	}

	flag_t *flag = game_get_flag(MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)));
	if (flag->res_waiting[0] != 0 && flag->res_waiting[1] != 0 &&
	    flag->res_waiting[2] != 0 && flag->res_waiting[3] != 0 &&
	    flag->res_waiting[4] != 0 && flag->res_waiting[5] != 0 &&
	    flag->res_waiting[6] != 0 && flag->res_waiting[7] != 0) {
		/* All resource slots at flag are occupied, wait */
		serf->animation = 82;
		serf->counter = 0;
		return;
	}

	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
	int slope = 31 - road_bld_slope_arr[(building->bld >> 2) & 0x3f];
	serf->counter = 0;
	serf_start_walking(serf, DIR_DOWN_RIGHT, slope);
	serf->anim = globals.anim;

	uint res = serf->s.move_resource_out.res;
	uint res_dest = serf->s.move_resource_out.res_dest;
	serf_state_t next_state = serf->s.move_resource_out.next_state;

	serf_log_state_change(serf, SERF_STATE_LEAVING_BUILDING);
	serf->state = SERF_STATE_LEAVING_BUILDING;
	serf->s.leaving_building.next_state = next_state;
	serf->s.leaving_building.field_B = res;
	serf->s.leaving_building.dest = res_dest;
}

static void
handle_serf_wait_for_resource_out_state(serf_t *serf)
{
	if (serf->counter != 0) {
		uint16_t delta = globals.anim - serf->anim;
		serf->anim = globals.anim;
		serf->counter -= delta;

		if (serf->counter >= 0) return;

		serf->counter = 0;
	}

	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
	inventory_t *inventory = building->u.inventory;
	if (inventory->serfs[SERF_4] != 0 || inventory->out_queue[0] == -1) return;

	serf_log_state_change(serf, SERF_STATE_MOVE_RESOURCE_OUT);
	serf->state = SERF_STATE_MOVE_RESOURCE_OUT;
	serf->s.move_resource_out.res = inventory->out_queue[0] + 1;
	serf->s.move_resource_out.res_dest = inventory->out_dest[0];
	serf->s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

	inventory->out_queue[0] = inventory->out_queue[1];
	inventory->out_queue[1] = -1;
	inventory->out_dest[0] = inventory->out_dest[1];

	/*handle_serf_move_resource_out_state(serf);*//* why isn't a state switch enough? */
}

static void
handle_serf_drop_resource_out_state(serf_t *serf)
{
	flag_t *flag = game_get_flag(MAP_OBJ_INDEX(serf->pos));
	int i = -1;
	for (i = 0; i < 8; i++) {
		/* Guaranteed to find a free slot because
		   the map position has been reserved since
		   a free position was found. */
		if (flag->res_waiting[i] == 0) break;
	}

	assert(i >= 0);

	flag->res_waiting[i] = serf->s.move_resource_out.res;
	flag->res_dest[i] = serf->s.move_resource_out.res_dest;
	flag->endpoint |= BIT(7); /* Resources waiting */

	serf_log_state_change(serf, SERF_STATE_READY_TO_ENTER);
	serf->state = SERF_STATE_READY_TO_ENTER;
	serf->s.ready_to_enter.field_B = 0;
}

static void
handle_serf_delivering_state(serf_t *serf)
{
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
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
			if (!BIT_TEST(building->serf, 5)) { /* Not burning */
				if (res == RESOURCE_COAL || (res < RESOURCE_BOAT && res != RESOURCE_LUMBER)) {
					building->stock1 += 15;
				} else {
					building->stock2 += 15;
				}
				if (building->stock1 > 255 || building->stock2 > 255) {
					/* Too many resources ? */
					building->stock1 = 255;
					building->stock2 = 255;
					inventory_t *inventory = building->u.inventory;
					inventory->resources[res] = min(inventory->resources[res]+1, 50000);
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
	if (MAP_SERF_INDEX(serf->pos) != 0 ||
	    MAP_SERF_INDEX(MAP_MOVE_DOWN_RIGHT(serf->pos)) != 0) {
		serf->animation = 82;
		serf->counter = 0;
		return;
	}

	if (serf->s.ready_to_leave_inventory.mode == -1) {
		flag_t *flag = game_get_flag(serf->s.ready_to_leave_inventory.dest);
		if (BIT_TEST(flag->endpoint, 6)) {
			building_t *building = flag->other_endpoint.b[DIR_UP_LEFT];
			if (MAP_SERF_INDEX(building->pos) != 0) {
				serf->animation = 82;
				serf->counter = 0;
				return;
			}
		}
	}

	game_get_inventory(serf->s.ready_to_leave_inventory.inv_index)->serfs[SERF_4] -= 1;

	serf_state_t next_state = SERF_STATE_WALKING;
	if (serf->s.ready_to_leave_inventory.mode == -3) next_state = SERF_STATE_73;
	LOGV("serf", "serf %i: next state is %s.", SERF_INDEX(serf), serf_state_name[next_state]);

	map_set_serf_index(serf->pos, 0);
	map_set_serf_index(MAP_MOVE_DOWN_RIGHT(serf->pos), SERF_INDEX(serf));

	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
	int slope = 31 - road_bld_slope_arr[(building->bld >> 2) & 0x3f];
	serf->counter = 0;
	serf_start_walking(serf, DIR_DOWN_RIGHT, slope);
	serf->anim = globals.anim;

	int mode = serf->s.ready_to_leave_inventory.mode;
	uint dest = serf->s.ready_to_leave_inventory.dest;

	serf_log_state_change(serf, SERF_STATE_LEAVING_BUILDING);
	serf->state = SERF_STATE_LEAVING_BUILDING;
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
	for (int i = 0; i < 7; i++) {
		if (flag->res_waiting[i] == 0) {
			slot = i;
			break;
		}
	}

	/* Resource is lost if no free slot is found */
	if (slot >= 0) {
		flag->res_waiting[slot] = 1 + res;
		flag->res_dest[slot] = 0;
		flag->endpoint |= BIT(7);

		player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
		sett->resource_count[res] += 1;
	}
}

static void
handle_serf_free_walking_state_dest_reached(serf_t *serf)
{
	switch (SERF_TYPE(serf)) {
	case SERF_LUMBERJACK:
		if (serf->s.free_walking.neg_dist1 == -128) {
			if (serf->s.free_walking.neg_dist2 < 0) {
				goto other_type;
			} else if (serf->s.free_walking.neg_dist2 > 0) {
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
			if (serf->s.free_walking.neg_dist2 < 0) {
				goto other_type;
			} else if (serf->s.free_walking.neg_dist2 > 0) {
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
				serf_start_walking(serf, DIR_UP_LEFT, 32);

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
			if (serf->s.free_walking.neg_dist2 < 0) goto other_type;

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
			if (serf->s.free_walking.neg_dist2 < 0) {
				goto other_type;
			} else if (serf->s.free_walking.neg_dist2 > 0) {
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
			if (serf->s.free_walking.neg_dist2 < 0) {
				goto other_type;
			} else if (serf->s.free_walking.neg_dist2 > 0) {
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
			if (serf->s.free_walking.neg_dist2 < 0) goto other_type;
			if (MAP_OBJ(serf->pos) == MAP_OBJ_FLAG &&
			    MAP_OWNER(serf->pos) == SERF_PLAYER(serf)) {
				serf_log_state_change(serf, SERF_STATE_LOOKING_FOR_GEO_SPOT);
				serf->state = SERF_STATE_LOOKING_FOR_GEO_SPOT;
				serf->counter = 0;
			} else {
				serf_log_state_change(serf, SERF_STATE_LOST);
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
			goto other_type;
		} else {
			serf_log_state_change(serf, SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING);
			serf->state = SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING;
			serf->counter = 0;
		}
		break;
	default:
	other_type:
		if (MAP_HAS_FLAG(serf->pos) &&
		    game_get_flag(MAP_OBJ_INDEX(serf->pos))->endpoint & 0x3f &&
		    MAP_OWNER(serf->pos) == SERF_PLAYER(serf)) {
			serf_log_state_change(serf, SERF_STATE_WALKING);
			serf->state = SERF_STATE_WALKING;
			serf->s.walking.res = -2;
			serf->s.walking.dest = 0;
			serf->s.walking.dir = 0;
			serf->counter = 0;
		} else {
			serf_log_state_change(serf, SERF_STATE_LOST);
			serf->state = SERF_STATE_LOST;
			serf->s.lost.field_B = 0;
			serf->counter = 0;
		}
		break;
	}
}

static void
handle_free_walking_common(serf_t *serf)
{
	const int dir_from_offset[] = {
		DIR_UP_LEFT, DIR_UP, -1,
		DIR_LEFT, -1, DIR_RIGHT,
		-1, DIR_DOWN, DIR_DOWN_RIGHT
	};

	const int dir_arr[] = {
		DIR_DOWN, DIR_DOWN_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP_LEFT, DIR_LEFT, -1, -1,
		DIR_LEFT, DIR_DOWN, DIR_DOWN_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP_LEFT, -1, -1,
		DIR_UP_LEFT, DIR_LEFT, DIR_DOWN, DIR_DOWN_RIGHT, DIR_RIGHT, DIR_UP, -1, -1,
		DIR_UP, DIR_UP_LEFT, DIR_LEFT, DIR_DOWN, DIR_DOWN_RIGHT, DIR_RIGHT, -1, -1,
		DIR_RIGHT, DIR_UP, DIR_UP_LEFT, DIR_LEFT, DIR_DOWN, DIR_DOWN_RIGHT, -1, -1,
		DIR_DOWN_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP_LEFT, DIR_LEFT, DIR_DOWN, -1, -1,

		DIR_UP_LEFT, DIR_UP, DIR_RIGHT, DIR_DOWN_RIGHT, DIR_DOWN, DIR_LEFT, -1, -1,
		DIR_UP, DIR_RIGHT, DIR_DOWN_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP_LEFT, -1, -1,
		DIR_RIGHT, DIR_DOWN_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP_LEFT, DIR_UP, -1, -1,
		DIR_DOWN_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP_LEFT, DIR_UP, DIR_RIGHT, -1, -1,
		DIR_DOWN, DIR_LEFT, DIR_UP_LEFT, DIR_UP, DIR_RIGHT, DIR_DOWN_RIGHT, -1, -1,
		DIR_LEFT, DIR_UP_LEFT, DIR_UP, DIR_RIGHT, DIR_DOWN_RIGHT, DIR_DOWN, -1, -1,

		DIR_UP, DIR_UP_LEFT, DIR_RIGHT, DIR_LEFT, DIR_DOWN_RIGHT, DIR_DOWN, -1, -1,
		DIR_UP_LEFT, DIR_UP, DIR_LEFT, DIR_RIGHT, DIR_DOWN, DIR_DOWN_RIGHT, -1, -1,
		DIR_UP_LEFT, DIR_LEFT, DIR_UP, DIR_DOWN, DIR_RIGHT, DIR_DOWN_RIGHT, -1, -1,
		DIR_LEFT, DIR_UP_LEFT, DIR_DOWN, DIR_UP, DIR_DOWN_RIGHT, DIR_RIGHT, -1, -1,
		DIR_LEFT, DIR_DOWN, DIR_UP_LEFT, DIR_DOWN_RIGHT, DIR_UP, DIR_RIGHT, -1, -1,
		DIR_DOWN, DIR_LEFT, DIR_DOWN_RIGHT, DIR_UP_LEFT, DIR_RIGHT, DIR_UP, -1, -1,
		DIR_DOWN, DIR_DOWN_RIGHT, DIR_LEFT, DIR_RIGHT, DIR_UP_LEFT, DIR_UP, -1, -1,
		DIR_DOWN_RIGHT, DIR_DOWN, DIR_RIGHT, DIR_LEFT, DIR_UP, DIR_UP_LEFT, -1, -1,
		DIR_DOWN_RIGHT, DIR_RIGHT, DIR_DOWN, DIR_UP, DIR_LEFT, DIR_UP_LEFT, -1, -1,
		DIR_RIGHT, DIR_DOWN_RIGHT, DIR_UP, DIR_DOWN, DIR_UP_LEFT, DIR_LEFT, -1, -1,
		DIR_RIGHT, DIR_UP, DIR_DOWN_RIGHT, DIR_UP_LEFT, DIR_DOWN, DIR_LEFT, -1, -1,
		DIR_UP, DIR_RIGHT, DIR_UP_LEFT, DIR_DOWN_RIGHT, DIR_LEFT, DIR_DOWN, -1, -1
	};

	int dir = -1;
	map_pos_t new_pos = 0;

	if (BIT_TEST(serf->s.free_walking.flags, 3) &&
	    (serf->s.free_walking.flags & 7) == 0) {
		handle_serf_free_walking_state_dest_reached(serf);
		return;
	} else if ((serf->s.free_walking.flags & 7) != 0) {
		int flags = serf->s.free_walking.flags & 7;
		if (BIT_TEST(serf->s.free_walking.flags, 3)) flags += 5;
		else flags -= 1;

		int d1 = serf->s.free_walking.dist1;
		int d2 = serf->s.free_walking.dist2;

		/* Check if dest is only one step away. */
		if (abs(d1) <= 1 && abs(d2) <= 1 &&
		    dir_from_offset[(d1+1) + 3*(d2+1)] > -1) {
			/* Convert offset in two dimensions to
			   direction variable. */
			dir_t dir = dir_from_offset[(d1+1) + 3*(d2+1)];

			if (MAP_OCCUPIED(MAP_MOVE(serf->pos, dir))) {
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
				return;
			}

			if (serf->state == SERF_STATE_KNIGHT_FREE_WALKING &&
			    serf->s.free_walking.neg_dist1 != -128 &&
			    MAP_SERF_INDEX(MAP_MOVE(serf->pos, dir)) != 0) {
				serf->s.free_walking.flags = 0;
				serf->animation = 82;
				serf->counter = counter_from_animation[serf->animation];
				return;
			}
		}

		const int *a0 = &dir_arr[8*flags];
		int d = -1;
		for (int i = 0; i < 6; i++) {
			new_pos = MAP_MOVE(serf->pos, a0[i]);
			if (!MAP_OCCUPIED(new_pos) &&
			    MAP_SERF_INDEX(new_pos) == 0) {
				dir = a0[i];
				d = 5 - i;
				break;
			}
		}

		if (d > -1) {
			d -= 3;
			serf->s.free_walking.flags -= (d << 4);
			if (d > 0 && serf->s.free_walking.flags < 0) {
				/* TODO Sometimes the direction chosen here
				   seems weird. */
				serf->s.free_walking.flags = 0;
				goto switch_on_dir;
			} else if (d < 0 && serf->s.free_walking.flags > 255) { /* overflow byte */
				serf->s.free_walking.flags = 0;
				goto next_handler;
			}

			serf->s.free_walking.flags = (serf->s.free_walking.flags & 0xf8) | (dir+1);
			goto switch_on_dir;
		} else {
			serf->s.free_walking.flags &= 0xf0;
			goto switch_with_other;
		}
	}

next_handler:;
	int offset = 12;
	int d1 = serf->s.free_walking.dist1;
	int d2 = serf->s.free_walking.dist2;
	if (d1 < 0) {
		d1 = -d1;
		if (d2 < 0) {
			d2 = -d2;
			if (d2 < d1) {
				offset += 2;
				d2 *= 2;
				if (d2 < d1) offset += 1;
			} else {
				d1 *= 2;
				if (d2 < d1) offset += 1;
			}
		} else {
			offset += 4;
			if (d2 >= d1) offset += 1;
		}
	} else {
		offset += 6;
		if (d2 < 0) {
			d2 = -d2;
			offset += 4;
			if (d2 >= d1) offset += 1;
		} else {
			if (d2 < d1) {
				offset += 2;
				d2 *= 2;
				if (d2 < d1) offset += 1;
			} else {
				d1 *= 2;
				if (d2 < d1) offset += 1;
			}
		}
	}

	const int *a0 = &dir_arr[8*offset];
	dir = a0[0];
	new_pos = MAP_MOVE(serf->pos, dir);
	if (!MAP_OCCUPIED(new_pos) &&
	    MAP_SERF_INDEX(new_pos) == 0) {
		goto switch_on_dir;
	}

	d1 = serf->s.free_walking.dist1;
	d2 = serf->s.free_walking.dist2;

	/* Check if dest is only one step away. */
	if (abs(d1) <= 1 && abs(d2) <= 1 &&
	    dir_from_offset[(d1+1) + 3*(d2+1)] > -1) {
		/* Convert offset in two dimensions to
		   direction variable. */
		dir_t d = dir_from_offset[(d1+1) + 3*(d2+1)];
		new_pos = MAP_MOVE(serf->pos, d);

		if (MAP_OCCUPIED(new_pos)) {
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
			if (other_serf->state == SERF_STATE_WALKING ||
			    other_serf->state == SERF_STATE_TRANSPORTING) {
				serf->s.free_walking.neg_dist2 += 1;
				if (serf->s.free_walking.neg_dist2 >= 10) {
					serf->s.free_walking.neg_dist2 = 0;
					if (MAP_HAS_FLAG(new_pos)) {
						if (other_serf->state == SERF_STATE_TRANSPORTING &&
						    other_serf->s.walking.wait_counter >= 0) {
							/* TODO Remove other serf from path - really necessary? */
						}
						/* sub_5AE56(); */
						LOGD("serf", "free walking: unhandled sub_5AE56() call.");
					}
				}
			}

			serf->animation = 82;
			serf->counter = counter_from_animation[serf->animation];
		}
	}

	/* Look for another direction to go in. */
	int i0 = -1;
	for (int i = 0; i < 5; i++) {
		dir = a0[1+i];
		new_pos = MAP_MOVE(serf->pos, dir);
		if (!MAP_OCCUPIED(new_pos) &&
		    MAP_SERF_INDEX(new_pos) == 0) {
			i0 = 4-i;
			break;
		}
	}

	if (i0 > -1) {
		int d0 = dir + 1;
		if (BIT_TEST(offset ^ i0, 0)) d0 += 8; /* ? */
		serf->s.free_walking.flags = (((6 - i0) & ~1) << 3) | d0;
	} else {
		goto switch_with_other;
	}

switch_on_dir:;
	/* A suitable direction has been found; walk. */
	assert(dir > -1);
	int dx = ((dir < 3) ? 1 : -1)*((dir % 3) < 2);
	int dy = ((dir < 3) ? 1 : -1)*((dir % 3) > 0);

#if 0
	LOGV("serf", "free walking: dest %i, %i, move %i, %i.",
	     serf->s.free_walking.dist1,
	     serf->s.free_walking.dist2, dx, dy);
#endif

	serf->s.free_walking.dist1 -= dx;
	serf->s.free_walking.dist2 -= dy;

	serf_start_walking(serf, dir, 32);

	if (serf->s.free_walking.dist1 == 0 &&
	    serf->s.free_walking.dist2 == 0) {
		serf->s.free_walking.flags = BIT(3);
	}
	return;

switch_with_other:;
	/* No free position can be found. Switch with
	   other serf. */
	dir = -1;
	serf_t *other_serf = NULL;
	for (int i = 0; i < 6; i++) {
		new_pos = MAP_MOVE(serf->pos, i);
		if (MAP_SERF_INDEX(new_pos) != 0) {
			other_serf = game_get_serf(MAP_SERF_INDEX(new_pos));

			if ((other_serf->state == SERF_STATE_WALKING ||
			     other_serf->state == SERF_STATE_TRANSPORTING) &&
			    other_serf->s.walking.dir == DIR_REVERSE(i)-6) {
				/* Move other walking serf in opposite direction. */
				other_serf->s.walking.dir = i;
				dir = i;
				break;
			} else if ((other_serf->state == SERF_STATE_FREE_WALKING ||
				    other_serf->state == SERF_STATE_KNIGHT_FREE_WALKING ||
				    other_serf->state == SERF_STATE_22) &&
				   other_serf->animation == 82) {
				/* Move other free walking serf in opposite direction. */
				int dx = ((dir < 3) ? 1 : -1)*((dir % 3) < 2);
				int dy = ((dir < 3) ? 1 : -1)*((dir % 3) > 0);
				other_serf->s.free_walking.dist1 += dx;
				other_serf->s.free_walking.dist2 += dy;

				if (other_serf->s.free_walking.dist1 == 0 &&
				    other_serf->s.free_walking.dist2 == 0) {
					other_serf->s.free_walking.flags = BIT(3);
				}
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
			serf->s.free_walking.flags = BIT(3);
		}

		/* Switch with other serf. */
		map_set_serf_index(serf->pos, SERF_INDEX(other_serf));
		map_set_serf_index(new_pos, SERF_INDEX(serf));

		other_serf->animation = get_walking_animation(MAP_HEIGHT(serf->pos) - MAP_HEIGHT(other_serf->pos),
							      6+DIR_REVERSE(dir));
		serf->animation = get_walking_animation(MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos), 6+dir);

		other_serf->counter = counter_from_animation[other_serf->animation];
		serf->counter = counter_from_animation[serf->animation];

		other_serf->pos = serf->pos;
		serf->pos = new_pos;
	} else {
		serf->animation = 82;
		serf->counter = counter_from_animation[serf->animation];
	}
}

static void
handle_serf_free_walking_state(serf_t *serf)
{

	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
	serf->counter -= delta;

	while (serf->counter < 0) {
		handle_free_walking_common(serf);
	}
}

static void
handle_serf_logging_state(serf_t *serf)
{
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
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
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
	serf->counter -= delta;

	while (serf->counter < 0) {
		int index = (random_int() & 0x7f) + 1;
		map_pos_t pos = (serf->pos + globals.spiral_pos_pattern[index]) & globals.map_index_mask;
		int obj = MAP_OBJ(pos);
		if (obj >= MAP_OBJ_TREE_0 && obj <= MAP_OBJ_PINE_7) {
			serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE);
			serf->state = SERF_STATE_READY_TO_LEAVE;
			serf->s.leaving_building.field_B = globals.spiral_pattern[2*index] - 1;
			serf->s.leaving_building.dest = globals.spiral_pattern[2*index+1] - 1;
			serf->s.leaving_building.dest2 = -globals.spiral_pattern[2*index] + 1;
			serf->s.leaving_building.dir = -globals.spiral_pattern[2*index+1] + 1;
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
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
	serf->counter -= delta;

	while (serf->counter < 0) {
		int index = (random_int() & 0x7f) + 1;
		map_pos_t pos = (serf->pos + globals.spiral_pos_pattern[index]) & globals.map_index_mask;
		if (MAP_PATHS(pos) == 0 &&
		    !MAP_OCCUPIED(pos) &&
		    MAP_OBJ(pos) == MAP_OBJ_NONE &&
		    MAP_TYPE_UP(pos) == 5 &&
		    MAP_TYPE_DOWN(pos) == 5 &&
		    MAP_TYPE_UP(MAP_MOVE_UP_LEFT(pos)) == 5 &&
		    MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(pos)) == 5) {
			serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE);
			serf->state = SERF_STATE_READY_TO_LEAVE;
			serf->s.leaving_building.field_B = globals.spiral_pattern[2*index] - 1;
			serf->s.leaving_building.dest = globals.spiral_pattern[2*index+1] - 1;
			serf->s.leaving_building.dest2 = -globals.spiral_pattern[2*index] + 1;
			serf->s.leaving_building.dir = -globals.spiral_pattern[2*index+1] + 1;
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
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
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
		map_obj_t new_obj = MAP_OBJ_NEW_PINE + (random_int() & 1);

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
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
	serf->counter -= delta;

	while (serf->counter < 0) {
		int index = (random_int() & 0x7f) + 1;
		map_pos_t pos = (serf->pos + globals.spiral_pos_pattern[index]) & globals.map_index_mask;
		int obj = MAP_OBJ(MAP_MOVE_UP_LEFT(pos));
		if (obj >= MAP_OBJ_STONE_0 &&
		    obj <= MAP_OBJ_STONE_7 &&
		    !MAP_OCCUPIED(pos)) {
			serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE);
			serf->state = SERF_STATE_READY_TO_LEAVE;
			serf->s.leaving_building.field_B = globals.spiral_pattern[2*index] - 1;
			serf->s.leaving_building.dest = globals.spiral_pattern[2*index+1] - 1;
			serf->s.leaving_building.dest2 = -globals.spiral_pattern[2*index] + 1;
			serf->s.leaving_building.dir = -globals.spiral_pattern[2*index+1] + 1;
			serf->s.leaving_building.next_state = SERF_STATE_FREE_WALKING/*SERF_STATE_22*/;
			LOGV("serf", "planning stonecutting: stone found, dist %i, %i.",
			     serf->s.leaving_building.field_B,
			     serf->s.leaving_building.dest);
			return;
		}

		serf->counter += 100;
	}
}

static void
handle_serf_stonecutting_state(serf_t *serf)
{
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
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
		serf_start_walking(serf, DIR_DOWN_RIGHT, 24);
		serf->anim = globals.anim;

		serf->s.free_walking.neg_dist1 = 2;
	}
}

static void
handle_serf_sawing_state(serf_t *serf)
{
	if (serf->s.sawing.mode == 0) {
		building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
		int stock = (building->stock2 >> 4) & 0xf;
		if (stock > 0) {
			building->stock2 -= (1 << 4);
			serf->s.sawing.mode = 1;
			serf->animation = 124;
			serf->counter = counter_from_animation[serf->animation];
			serf->anim = globals.anim;
			map_set_serf_index(serf->pos, SERF_INDEX(serf));
		}
	} else {
		uint16_t delta = globals.anim - serf->anim;
		serf->anim = globals.anim;
		serf->counter -= delta;

		if (serf->counter >= 0) return;

		map_set_serf_index(serf->pos, 0);
		serf_log_state_change(serf, SERF_STATE_MOVE_RESOURCE_OUT);
		serf->state = SERF_STATE_MOVE_RESOURCE_OUT;
		serf->s.move_resource_out.res = 1 + RESOURCE_PLANK;
		serf->s.move_resource_out.res_dest = 0;
		serf->s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

		/* Update resource stats. */
		player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
		sett->resource_count[RESOURCE_PLANK] += 1;
	}
}

static void
handle_serf_lost_state(serf_t *serf)
{
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
	serf->counter -= delta;

	while (serf->counter < 0) {
		/* Try to find a suitable destination. */
		for (int i = 0; i < 258; i++) {
			int index = (serf->s.lost.field_B == 0) ? 1+i : 258-i;
			map_pos_t dest = (serf->pos + globals.spiral_pos_pattern[index]) & globals.map_index_mask;

			if (MAP_HAS_FLAG(dest)) {
				flag_t *flag = game_get_flag(MAP_OBJ_INDEX(dest));
				if ((flag->endpoint & 0x3f) != 0 &&
				    MAP_HAS_OWNER(dest) && MAP_OWNER(dest) == SERF_PLAYER(serf)) {
					if (SERF_TYPE(serf) >= SERF_KNIGHT_0 &&
					    SERF_TYPE(serf) <= SERF_KNIGHT_4) {
						serf_log_state_change(serf, SERF_STATE_KNIGHT_FREE_WALKING);
						serf->state = SERF_STATE_KNIGHT_FREE_WALKING;
					} else {
						serf_log_state_change(serf, SERF_STATE_FREE_WALKING);
						serf->state = SERF_STATE_FREE_WALKING;
					}

					serf->s.free_walking.dist1 = globals.spiral_pattern[2*index];
					serf->s.free_walking.dist2 = globals.spiral_pattern[2*index+1];
					serf->s.free_walking.neg_dist1 = -128;
					serf->s.free_walking.neg_dist2 = -1;
					serf->s.free_walking.flags = 0;
					serf->counter = 0;
					return;
				}
			}
		}

		/* TODO choose a random direction */
		LOGD("serf", "serf %i lost: no dest found.", SERF_INDEX(serf));
		serf->state = SERF_STATE_NULL;
		serf->counter = 0;
	}
}

static void
handle_serf_escape_building_state(serf_t *serf)
{
	if (MAP_SERF_INDEX(serf->pos) == 0) {
		map_set_serf_index(serf->pos, SERF_INDEX(serf));
		serf->animation = 82;
		serf->counter = 0;
		serf->anim = globals.anim;

		serf_log_state_change(serf, SERF_STATE_LOST);
		serf->state = SERF_STATE_LOST;
		serf->s.lost.field_B = 0;
	}
}

static void
handle_serf_mining_state(serf_t *serf)
{
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
	serf->counter -= delta;

	while (serf->counter < 0) {
		building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));

		LOGV("serf", "mining substate: %i.", serf->s.mining.substate);
		switch (serf->s.mining.substate) {
		case 0:
		{
			/* There is a small chance that the miner will
			   require food and go to state 1. */
			int r = random_int();
			if ((r & 7) != 0) serf->s.mining.substate = 2;
			else serf->s.mining.substate = 1;
			serf->counter += 100 + (r & 0x1ff);
		}
		break;
		case 1:
			if ((building->stock1 & 0xf0) == 0) {
				map_set_serf_index(serf->pos, SERF_INDEX(serf));
				serf->animation = 98;
				serf->counter += 256;
				if (serf->counter < 0) serf->counter = 255;
			} else {
				/* Eat the food. */
				building->stock1 -= 0x10;
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
			int offset = globals.spiral_pos_pattern[(random_int() >> 2) & 0x1f];
			map_pos_t dest = (serf->pos + offset) & globals.map_index_mask;
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
				player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
				if (BIT_TEST(sett->flags, 7)) { /* AI */
					/* TODO Burn building. */
				}

				int type = ((BUILDING_TYPE(building)-BUILDING_STONEMINE) << 5) | 4;
				player_add_notification(globals.player_sett[BUILDING_PLAYER(building)],
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
				player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
				sett->resource_count[res-1] += 1;
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
		if ((building->stock1 & 0xf0) != 0 &&
		    (building->stock2 & 0xf0) != 0) {
			building->serf |= BIT(4);
			building->stock1 -= 0x10;
			building->stock2 -= 0x10;

			serf->s.smelting.mode = 1;
			if (serf->s.smelting.type == 0) {
				serf->animation = 130;
			} else {
				serf->animation = 129;
			}
			serf->s.smelting.counter = 20;
			serf->counter = counter_from_animation[serf->animation];
			serf->anim = globals.anim;

			map_set_serf_index(serf->pos, SERF_INDEX(serf));
		}
	} else {
		uint16_t delta = globals.anim - serf->anim;
		serf->anim = globals.anim;
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
				player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
				sett->resource_count[res-1] += 1;
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
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
	serf->counter -= delta;

	while (serf->counter < 0) {
		int index = ((random_int() >> 2) & 0x3f) + 1;
		map_pos_t dest = (serf->pos + globals.spiral_pos_pattern[index]) & globals.map_index_mask;

		if (MAP_OBJ(dest) == MAP_OBJ_NONE &&
		    MAP_PATHS(dest) == 0 &&
		    (((MAP_TYPE_DOWN(dest) & 0xc) == 0 &&
		      (MAP_TYPE_UP(MAP_MOVE_UP_LEFT(dest)) & 0xc) != 0) ||
		     ((MAP_TYPE_DOWN(MAP_MOVE_LEFT(dest)) & 0xc) == 0 &&
		      (MAP_TYPE_UP(MAP_MOVE_UP(dest)) & 0xc) != 0))) {
			serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE);
			serf->state = SERF_STATE_READY_TO_LEAVE;
			serf->s.leaving_building.field_B = globals.spiral_pattern[2*index] - 1;
			serf->s.leaving_building.dest = globals.spiral_pattern[2*index+1] - 1;
			serf->s.leaving_building.dest2 = -globals.spiral_pattern[2*index] + 1;
			serf->s.leaving_building.dir = -globals.spiral_pattern[2*index+1] + 1;
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
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
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
			if (MAP_OCCUPIED(MAP_MOVE_LEFT(serf->pos))) dir = DIR_LEFT;
			else dir = DIR_DOWN;
		} else {
			if (MAP_OCCUPIED(MAP_MOVE_RIGHT(serf->pos))) dir = DIR_RIGHT;
			else dir = DIR_DOWN_RIGHT;
		}

		int res = MAP_RES_FISH(MAP_MOVE(serf->pos, dir));
		if (res > 0 && (random_int() & 0x3f) + 4 < res) {
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
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
	serf->counter -= delta;

	while (serf->counter < 0) {
		int index = ((random_int() >> 2) & 0x1f) + 7;
		map_pos_t dest = (serf->pos + globals.spiral_pos_pattern[index]) & globals.map_index_mask;

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
			serf->s.leaving_building.field_B = globals.spiral_pattern[2*index] - 1;
			serf->s.leaving_building.dest = globals.spiral_pattern[2*index+1] - 1;
			serf->s.leaving_building.dest2 = -globals.spiral_pattern[2*index] + 1;
			serf->s.leaving_building.dir = -globals.spiral_pattern[2*index+1] + 1;
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
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
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
		if (building->stock1 & 0xf0) {
			building->serf |= BIT(4);
			building->stock1 -= 0x10;

			serf->s.milling.mode = 1;
			serf->animation = 137;
			serf->counter = counter_from_animation[serf->animation];
			serf->anim = globals.anim;

			map_set_serf_index(serf->pos, SERF_INDEX(serf));
		}
	} else {
		uint16_t delta = globals.anim - serf->anim;
		serf->anim = globals.anim;
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

				player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
				sett->resource_count[RESOURCE_FLOUR] += 1;
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
		if (building->stock1 & 0xf0) {
			building->stock1 -= 0x10;

			serf->s.baking.mode = 1;
			serf->animation = 138;
			serf->counter = counter_from_animation[serf->animation];
			serf->anim = globals.anim;

			map_set_serf_index(serf->pos, SERF_INDEX(serf));
		}
	} else {
		uint16_t delta = globals.anim - serf->anim;
		serf->anim = globals.anim;
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

				player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
				sett->resource_count[RESOURCE_BREAD] += 1;
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
		if (building->stock1 & 0xf0) {
			building->stock1 -= 0x10;

			serf->s.pigfarming.mode = 1;
			serf->animation = 139;
			serf->counter = counter_from_animation[serf->animation];
			serf->anim = globals.anim;

			map_set_serf_index(serf->pos, SERF_INDEX(serf));
		}
	} else {
		uint16_t delta = globals.anim - serf->anim;
		serf->anim = globals.anim;
		serf->counter -= delta;

		while (serf->counter < 0) {
			serf->s.pigfarming.mode += 1;
			if (serf->s.pigfarming.mode & 1) {
				if (serf->s.pigfarming.mode != 7) {
					map_set_serf_index(serf->pos, SERF_INDEX(serf));
					serf->animation = 139;
					serf->counter = counter_from_animation[serf->animation];
				} else if (building->stock2 == 8 ||
					   (building->stock2 > 3 &&
					    ((20*random_int()) >> 16) < building->stock2)) {
					/* Pig is ready for the butcher. */
					building->stock2 -= 1;

					serf_log_state_change(serf, SERF_STATE_MOVE_RESOURCE_OUT);
					serf->state = SERF_STATE_MOVE_RESOURCE_OUT;
					serf->s.move_resource_out.res = 1 + RESOURCE_PIG;
					serf->s.move_resource_out.res_dest = 0;
					serf->s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

					/* Update resource stats. */
					player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
					sett->resource_count[RESOURCE_PIG] += 1;
				} else if (random_int() & 0xf) {
					serf->s.pigfarming.mode = 1;
					serf->animation = 139;
					serf->counter = counter_from_animation[serf->animation];
					serf->anim = globals.anim;
					map_set_serf_index(serf->pos, SERF_INDEX(serf));
				} else {
					serf->s.pigfarming.mode = 0;
				}
				return;
			} else {
				map_set_serf_index(serf->pos, 0);
				if (building->stock2 < 8 &&
				    random_int() < breeding_prob[building->stock2-1]) {
					building->stock2 += 1;
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
		if (building->stock1 & 0xf0) {
			building->stock1 -= 0x10;

			serf->s.butchering.mode = 1;
			serf->animation = 140;
			serf->counter = counter_from_animation[serf->animation];
			serf->anim = globals.anim;

			map_set_serf_index(serf->pos, SERF_INDEX(serf));
		}
	} else {
		uint16_t delta = globals.anim - serf->anim;
		serf->anim = globals.anim;
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
			player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
			sett->resource_count[RESOURCE_MEAT] += 1;
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
		if (!BIT_TEST(building->serf, 3)) {
			if ((building->stock1 & 0xf0) == 0 ||
			    (building->stock2 & 0xf0) == 0) {
				return;
			}
			building->stock1 -= 0x10;
			building->stock2 -= 0x10;
		}

		building->serf |= BIT(4);

		serf->s.making_weapon.mode = 1;
		serf->animation = 143;
		serf->counter = counter_from_animation[serf->animation];
		serf->anim = globals.anim;

		map_set_serf_index(serf->pos, SERF_INDEX(serf));
	} else {
		uint16_t delta = globals.anim - serf->anim;
		serf->anim = globals.anim;
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
				player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
				sett->resource_count[res] += 1;
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
		if (building->stock1 & 0xf0 &&
		    building->stock2 & 0xf0) {
			building->stock1 -= 0x10;
			building->stock2 -= 0x10;

			serf->s.making_tool.mode = 1;
			serf->animation = 144;
			serf->counter = counter_from_animation[serf->animation];
			serf->anim = globals.anim;

			map_set_serf_index(serf->pos, SERF_INDEX(serf));
		}
	} else {
		uint16_t delta = globals.anim - serf->anim;
		serf->anim = globals.anim;
		serf->counter -= delta;

		while (serf->counter < 0) {
			serf->s.making_tool.mode += 1;
			if (serf->s.making_tool.mode == 4) {
				/* Done making tool. */
				map_set_serf_index(serf->pos, 0);

				player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
				int total_tool_prio = 0;
				for (int i = 0; i < 9; i++) total_tool_prio += sett->tool_prio[i];
				total_tool_prio >>= 4;

				int res = -1;
				if (total_tool_prio > 0) {
					/* Use defined tool priorities. */
					int prio_offset = (total_tool_prio*random_int()) >> 16;
					for (int i = 0; i < 9; i++) {
						prio_offset -= sett->tool_prio[i] >> 4;
						if (prio_offset < 0) {
							res = RESOURCE_SHOVEL + i;
							break;
						}
					}
				} else {
					/* Completely random. */
					res = RESOURCE_SHOVEL + ((9*random_int()) >> 16);
				}

				serf_log_state_change(serf, SERF_STATE_MOVE_RESOURCE_OUT);
				serf->state = SERF_STATE_MOVE_RESOURCE_OUT;
				serf->s.move_resource_out.res = 1 + res;
				serf->s.move_resource_out.res_dest = 0;
				serf->s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

				/* Update resource stats. */
				sett->resource_count[res] += 1;
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
		if ((building->stock1 & 0xf0) == 0) return;
		building->stock1 -= 0x10;
		building->stock2 = 0;

		serf->s.building_boat.mode = 1;
		serf->animation = 146;
		serf->counter = counter_from_animation[serf->animation];
		serf->anim = globals.anim;

		map_set_serf_index(serf->pos, SERF_INDEX(serf));
	} else {
		uint16_t delta = globals.anim - serf->anim;
		serf->anim = globals.anim;
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
					building->stock2 = 0;
					map_set_serf_index(serf->pos, 0);

					serf_log_state_change(serf, SERF_STATE_MOVE_RESOURCE_OUT);
					serf->state = SERF_STATE_MOVE_RESOURCE_OUT;
					serf->s.move_resource_out.res = 1 + RESOURCE_BOAT;
					serf->s.move_resource_out.res_dest = 0;
					serf->s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

					/* Update resource stats. */
					player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
					sett->resource_count[RESOURCE_BOAT] += 1;
				}
			} else {
				/* Continue building. */
				building->stock2 += 1;
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
		int index = ((random_int() >> 2) & 0x3f) + 1;
		map_pos_t dest = (serf->pos + globals.spiral_pos_pattern[index]) & globals.map_index_mask;

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
				serf->s.free_walking.dist1 = globals.spiral_pattern[2*index];
				serf->s.free_walking.dist2 = globals.spiral_pattern[2*index+1];
				serf->s.free_walking.neg_dist1 = -globals.spiral_pattern[2*index];
				serf->s.free_walking.neg_dist2 = -globals.spiral_pattern[2*index+1];
				serf->s.free_walking.flags = 0;
				serf->anim = globals.anim;
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
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
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
					map_pos_t pos = (serf->pos + globals.spiral_pos_pattern[1+i]) & globals.map_index_mask;
					if ((MAP_OBJ(pos) >> 1) == (obj >> 1)) {
						show_notification = 0;
						break;
					}
				}

				/* Create notification for found resource. */
				if (show_notification) {
					player_add_notification(globals.player_sett[SERF_PLAYER(serf)],
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
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
	serf->counter -= delta;

	if (serf->counter < 0) {
		map_obj_t obj = MAP_OBJ(MAP_MOVE_UP_LEFT(serf->pos));
		if (obj >= MAP_OBJ_SMALL_BUILDING ||
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
					player_add_notification(globals.player_sett[BUILDING_PLAYER(building)],
								(SERF_PLAYER(serf) << 5) | 1, building->pos);
				}

				/* Change state of attacking knight */
				serf->counter = 0;
				serf->state = SERF_STATE_KNIGHT_PREPARE_ATTACKING;
				serf->animation = 168;

				/* Remove knight from stats of defending building */
				if (building->stock1 == 0xff) { /* Castle */
					globals.player_sett[BUILDING_PLAYER(building)]->castle_knights -= 1;
				} else {
					building->stock1 -= 0xf;
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
				return;
			}
		}

		/* No one to defend this building. Occupy it. */
		serf_log_state_change(serf, SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING);
		serf->state = SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING;
		serf->animation = 179;
		serf->counter = counter_from_animation[serf->animation];
		serf->anim = globals.anim;
	}
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
		serf->anim = globals.anim;

		/* Change state of defender. */
		serf_log_state_change(def_serf, SERF_STATE_KNIGHT_DEFENDING);
		def_serf->state = SERF_STATE_KNIGHT_DEFENDING;
		def_serf->counter = 0;

		/* Calculate "morale" for attacker. */
		int exp_factor = 1 << (SERF_TYPE(serf) - SERF_KNIGHT_0);
		int land_factor = 0x1000;
		if (SERF_PLAYER(serf) != MAP_OWNER(serf->pos)) {
			land_factor = globals.player_sett[SERF_PLAYER(serf)]->knight_morale;
		}

		int morale = (0x400*exp_factor * land_factor) >> 16;

		/* Calculate "morale" for defender. */
		int def_exp_factor = 1 << (SERF_TYPE(def_serf) - SERF_KNIGHT_0);
		int def_land_factor = 0x1000;
		if (SERF_PLAYER(def_serf) != MAP_OWNER(def_serf->pos)) {
			def_land_factor = globals.player_sett[SERF_PLAYER(def_serf)]->knight_morale;
		}

		int def_morale = (0x400*def_exp_factor * def_land_factor) >> 16;

		int player = -1;
		int value = -1;
		serf_type_t type = -1;
		if ((((morale+def_morale)*random_int()) >> 16) < morale) {
			player = SERF_PLAYER(def_serf);
			value = def_exp_factor;
			type = SERF_TYPE(def_serf);
			serf->s.attacking.field_C = 1;
		} else {
			player = SERF_PLAYER(serf);
			value = exp_factor;
			type = SERF_TYPE(serf);
			serf->s.attacking.field_C = 0;
		}

		globals.player_sett[player]->total_military_score -= value;
		globals.player_sett[player]->serf_count[type] -= 1;
		serf->s.attacking.field_B = random_int() & 0x70;
	}
}

static void
handle_serf_knight_leave_for_fight_state(serf_t *serf)
{
	if (MAP_SERF_INDEX(serf->pos) == SERF_INDEX(serf) ||
	    MAP_SERF_INDEX(serf->pos) == 0) {
		building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
		int slope = road_bld_slope_arr[(building->bld >> 2) & 0x3f];
		serf->counter = 0;
		serf_start_walking(serf, DIR_DOWN_RIGHT, slope);
		serf->anim = globals.anim;

		serf_log_state_change(serf, SERF_STATE_LEAVING_BUILDING);
		serf->state = SERF_STATE_LEAVING_BUILDING;
	}
}

static void
handle_serf_knight_prepare_defending_state(serf_t *serf)
{
	serf->counter = 0;
	serf->animation = 84;
}

static void
handle_serf_knight_attacking_victory_state(serf_t *serf)
{
	serf_t *def_serf = game_get_serf(serf->s.attacking.def_index);

	uint16_t delta = globals.anim - def_serf->anim;
	def_serf->anim = globals.anim;
	def_serf->counter -= delta;

	if (def_serf->counter < 0) {
		game_free_serf(SERF_INDEX(def_serf));
		serf->s.attacking.def_index = 0;

		serf_log_state_change(serf, SERF_STATE_KNIGHT_ENGAGING_BUILDING);
		serf->state = SERF_STATE_KNIGHT_ENGAGING_BUILDING;
		serf->anim = globals.anim;
		serf->counter = 0;
	}
}

static void
handle_serf_knight_attacking_defeat_state(serf_t *serf)
{
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
	serf->counter -= delta;

	if (serf->counter < 0) {
		map_set_serf_index(serf->pos, 0);
		game_free_serf(SERF_INDEX(serf));
	}
}

static void
handle_state_knight_free_walking(serf_t *serf)
{
	uint16_t delta = globals.anim - serf->anim;
	serf->anim = globals.anim;
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
						if (!MAP_OCCUPIED(pos)) {
							serf_log_state_change(serf, SERF_STATE_54);
							serf->state = SERF_STATE_54;
							/* TODO copy some state from other serf. */
							/* serf->s.field_E = other->s.field_B; */
							/* serf->s.field_F = other->s.field_C; */
							/* serf->s.field_D = 1; */
							serf->animation = 99;
							serf->counter = 255;

							serf_log_state_change(other, SERF_STATE_55);
							other->state = SERF_STATE_55;
							/* TODO set some state */
							/* other->s.field_D = d; */
							/* other->s.field_E = SERF_INDEX(serf); */
							return;
						}
					} else if (other->state == SERF_STATE_WALKING &&
						   SERF_TYPE(other) >= SERF_KNIGHT_0 &&
						   SERF_TYPE(other) <= SERF_KNIGHT_4) {
						pos = MAP_MOVE_LEFT(pos);
						if (!MAP_OCCUPIED(pos)) {
							serf_log_state_change(serf, SERF_STATE_54);
							serf->state = SERF_STATE_54;
							/* TODO set some state */
							/* serf->s.field_D = 0; */
							serf->animation = 99;
							serf->counter = 255;

							flag_t *dest = game_get_flag(other->s.walking.dest);
							building_t *building = dest->other_endpoint.b[DIR_UP_LEFT];
							building->stock1 -= 1;

							serf_log_state_change(other, SERF_STATE_55);
							other->state = SERF_STATE_55;
							/* TODO set some state */
							/* other->s.field_D = d; */
							/* other->s.field_E = SERF_INDEX(serf); */
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
handle_serf_state_knight_leave_for_walk_to_fight(serf_t *serf)
{
	if (MAP_SERF_INDEX(serf->pos) != SERF_INDEX(serf) &&
	    MAP_SERF_INDEX(serf->pos) != 0) {
		serf->animation = 82;
		serf->counter = 0;
		return;
	}

	building_t *building = game_get_building(MAP_OBJ_INDEX(serf->pos));
	map_pos_t new_pos = MAP_MOVE_DOWN_RIGHT(serf->pos);

	if (MAP_SERF_INDEX(new_pos) == 0) {
		map_set_serf_index(serf->pos, 0);
		map_set_serf_index(new_pos, SERF_INDEX(serf));
		serf->pos = new_pos;

		int slope = 31 - road_bld_slope_arr[(building->bld >> 2) & 0x3f];
		serf->animation = get_walking_animation(MAP_HEIGHT(new_pos) - MAP_HEIGHT(serf->pos), DIR_DOWN_RIGHT);
		serf->counter = (slope * counter_from_animation[serf->animation]) >> 5;
		serf->anim = globals.anim;

		/* For clean state change, save the values first. */
		/* TODO maybe knight_leave_for_walk_to_fight can
		   share leaving_building state vars. */
		int dist_col = serf->s.leave_for_walk_to_fight.dist_col;
		int dist_row = serf->s.leave_for_walk_to_fight.dist_row;
		int field_D = serf->s.leave_for_walk_to_fight.field_D;
		int field_E = serf->s.leave_for_walk_to_fight.field_E;
		serf_state_t next_state = serf->s.leave_for_walk_to_fight.next_state;

		serf_log_state_change(serf, SERF_STATE_LEAVING_BUILDING);
		serf->state = SERF_STATE_LEAVING_BUILDING;
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

			int total_knights = (building->stock1 & 0xf) + ((building->stock1 >> 4) & 0xf);
			if (total_knights < max_capacity) {
				building->stock1 += 0x10;
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
	map_2_t *map_data = MAP_2_DATA(globals.map_mem2_ptr);

	flag_t *flag = serf->s.idle_on_path.flag;
	int rev_dir = serf->s.idle_on_path.rev_dir;

	/* Set walking dir in field_E. */
	if (BIT_TEST(flag->other_end_dir[rev_dir], 7)) {
		serf->s.idle_on_path.field_E = (serf->anim & 0xff) + 6;
	} else {
		flag_t *other_flag = flag->other_endpoint.f[rev_dir];
		int other_dir = (flag->other_end_dir[rev_dir] >> 3) & 7;
		if (BIT_TEST(other_flag->other_end_dir[other_dir], 7)) {
			serf->s.idle_on_path.field_E = DIR_REVERSE(rev_dir);
		} else {
			return;
		}
	}

	if (MAP_SERF_INDEX(serf->pos) == 0) {
		map_data[serf->pos].u.s.field_1 = 0;
		map_set_serf_index(serf->pos, SERF_INDEX(serf));

		int dir = serf->s.idle_on_path.field_E;

		serf_log_state_change(serf, SERF_STATE_TRANSPORTING);
		serf->state = SERF_STATE_TRANSPORTING;
		serf->s.walking.res = 0;
		serf->s.walking.wait_counter = 0;
		serf->s.walking.dir = dir;
		serf->anim = globals.anim;
		serf->counter = 0;
	} else {
		serf_log_state_change(serf, SERF_STATE_WAIT_IDLE_ON_PATH);
		serf->state = SERF_STATE_WAIT_IDLE_ON_PATH;
	}
}

static void
handle_serf_wait_idle_on_path_state(serf_t *serf)
{
	map_2_t *map_data = MAP_2_DATA(globals.map_mem2_ptr);

	if (MAP_SERF_INDEX(serf->pos) == 0) {
		/* Duplicate code from handle_serf_idle_on_path_state() */
		map_data[serf->pos].u.s.field_1 = 0;
		map_set_serf_index(serf->pos, SERF_INDEX(serf));

		int dir = serf->s.idle_on_path.field_E;

		serf_log_state_change(serf, SERF_STATE_TRANSPORTING);
		serf->state = SERF_STATE_TRANSPORTING;
		serf->s.walking.res = 0;
		serf->s.walking.wait_counter = 0;
		serf->s.walking.dir = dir;
		serf->anim = globals.anim;
		serf->counter = 0;
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
	map_2_t *map_data = MAP_2_DATA(globals.map_mem2_ptr);

	if (MAP_SERF_INDEX(serf->pos) == 0) {
		map_data[serf->pos].u.s.field_1 = 0;
		map_set_serf_index(serf->pos, SERF_INDEX(serf));
		serf->anim = globals.anim;
		serf->counter = 0;

		if (SERF_TYPE(serf) == SERF_DIGGER) {
			serf_log_state_change(serf, SERF_STATE_26);
			serf->state = SERF_STATE_26;
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
	case SERF_STATE_22:
		/* TODO */
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
	case SERF_STATE_26:
		/* TODO */
		break;
	case SERF_STATE_27:
		/* TODO */
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
		/* TODO */
		break;
	case SERF_STATE_KNIGHT_DEFENDING:
		break;
	case SERF_STATE_KNIGHT_ATTACKING_VICTORY: /* 50 */
		handle_serf_knight_attacking_victory_state(serf);
		break;
	case SERF_STATE_KNIGHT_ATTACKING_DEFEAT:
		handle_serf_knight_attacking_defeat_state(serf);
		break;
	case SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING:
		/* TODO */
		break;
	case SERF_STATE_KNIGHT_FREE_WALKING:
		handle_state_knight_free_walking(serf);
		break;
	case SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT:
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

