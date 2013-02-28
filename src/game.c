/*
 * game.c - Gameplay related functions
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

/* The following are functions that concern the gameplay as
   a whole. Functions that only act on a specific game object should
   go in the respective source file. */

#include <stdlib.h>
#include <assert.h>

#include "game.h"
#include "map.h"
#include "player.h"
#include "flag.h"
#include "building.h"
#include "globals.h"
#include "random.h"
#include "log.h"
#include "debug.h"

#define GROUND_ANALYSIS_RADIUS  25


/* Allocate and initialize a new flag_t object.
   Return -1 if no more flags can be allocated, otherwise 0. */
int
game_alloc_flag(flag_t **flag, int *index)
{
	for (int i = 0; i*8 < globals.max_flg_cnt; i++) {
		if (globals.flg_bitmap[i] != 0xff) {
			for (int j = 0; j < 8; j++) {
				if (!BIT_TEST(globals.flg_bitmap[i], 7-j)) {
					int ix = 8*i + j;
					if (ix >= globals.max_flg_cnt) return -1; /* TODO looks unneccesary */

					globals.flg_bitmap[i] |= BIT(7-j);

					if (ix == globals.max_ever_flag_index) globals.max_ever_flag_index += 1;

					flag_t *f = &globals.flgs[ix];
					f->pos = 0;
					f->search_num = 0;
					f->search_dir = 0;
					f->path_con = 0;
					f->endpoint = 0;
					f->transporter = 0;
					memset(&f->length, 0, sizeof(f->length));
					memset(&f->res_waiting, 0, sizeof(f->res_waiting));
					f->bld_flags = 0;
					f->bld2_flags = 0;
					memset(&f->other_end_dir, 0, sizeof(f->other_end_dir));

					if (flag != NULL) *flag = f;
					if (index != NULL) *index = ix;

					return 0;
				}
			}
		}
	}

	return -1;
}

/* Return flag_t object with index. */
flag_t *
game_get_flag(int index)
{
	assert(index > 0 && index < globals.max_flg_cnt);
	assert(FLAG_ALLOCATED(index));
	return &globals.flgs[index];
}

/* Deallocate flag_t object. */
void
game_free_flag(int index)
{
	/* Remove flag from allocation bitmap. */
	globals.flg_bitmap[index/8] &= ~BIT(7-(index&7));

	/* Decrement max_ever_flag_index as much as possible. */
	if (index == globals.max_ever_flag_index + 1) {
		while (--globals.max_ever_flag_index > 0) {
			index -= 1;
			if (FLAG_ALLOCATED(index)) break;
		}
	}
}

/* Allocate and initialize a new building_t object.
   Return -1 if no more buildings can be allocated, otherwise 0. */
int
game_alloc_building(building_t **building, int *index)
{
	for (int i = 0; i*8 < globals.max_building_cnt; i++) {
		if (globals.buildings_bitmap[i] != 0xff) {
			for (int j = 0; j < 8; j++) {
				if (!BIT_TEST(globals.buildings_bitmap[i], 7-j)) {
					int ix = 8*i + j;

					if (ix >= globals.max_building_cnt) return -1; /* TODO looks unneccesary */

					globals.buildings_bitmap[i] |= BIT(7-j);

					if (ix == globals.max_ever_building_index) globals.max_ever_building_index += 1;

					building_t *b = &globals.buildings[ix];
					b->bld = 0;
					b->flg_index = 0;
					b->serf = 0;

					b->stock[0].type = -1;
					b->stock[0].prio = 0;
					b->stock[0].available = 0;
					b->stock[0].requested = 0;

					b->stock[1].type = -1;
					b->stock[1].prio = 0;
					b->stock[1].available = 0;
					b->stock[1].requested = 0;

					b->serf_index = 0;

					if (building != NULL) *building = b;
					if (index != NULL) *index = ix;

					return 0;
				}
			}
		}
	}

	return -1;
}

/* Return building_t object with index. */
building_t *
game_get_building(int index)
{
	assert(index > 0 && index < globals.max_building_cnt);
	assert(BUILDING_ALLOCATED(index));
	return &globals.buildings[index];
}

/* Deallocate building_t object. */
void
game_free_building(int index)
{
	/* Remove building from allocation bitmap. */
	globals.buildings_bitmap[index/8] &= ~BIT(7-(index&7));

	/* Decrement max_ever_building_index as much as possible. */
	if (index == globals.max_ever_building_index + 1) {
		while (--globals.max_ever_building_index > 0) {
			index -= 1;
			if (BUILDING_ALLOCATED(index)) break;
		}
	}
}

/* Allocate and initialize a new inventory_t object.
   Return -1 if no more inventories can be allocated, otherwise 0. */
int
game_alloc_inventory(inventory_t **inventory, int *index)
{
	for (int i = 0; i*8 < globals.max_inventory_cnt; i++) {
		if (globals.inventories_bitmap[i] != 0xff) {
			for (int j = 0; j < 8; j++) {
				if (!BIT_TEST(globals.inventories_bitmap[i], 7-j)) {
					int ix = 8*i + j;

					if (ix >= globals.max_inventory_cnt) return -1; /* TODO looks unneccesary */

					globals.inventories_bitmap[i] |= BIT(7-j);

					if (ix == globals.max_ever_inventory_index) globals.max_ever_inventory_index += 1;

					inventory_t *iv = &globals.inventories[ix];
					memset(iv, 0, sizeof(inventory_t));

					iv->out_queue[0] = -1;
					iv->out_queue[1] = -1;

					if (inventory != NULL) *inventory = iv;
					if (index != NULL) *index = ix;

					return 0;
				}
			}
		}
	}

	return -1;
}

/* Return inventory_t object with index. */
inventory_t *
game_get_inventory(int index)
{
	assert(index < globals.max_inventory_cnt);
	assert(INVENTORY_ALLOCATED(index));
	return &globals.inventories[index];
}

/* Deallocate inventory_t object. */
void
game_free_inventory(int index)
{
	/* Remove inventory from allocation bitmap. */
	globals.inventories_bitmap[index/8] &= ~BIT(7-(index&7));

	/* Decrement max_ever_inventory_index as much as possible. */
	if (index == globals.max_ever_inventory_index + 1) {
		while (--globals.max_ever_inventory_index > 0) {
			index -= 1;
			if (INVENTORY_ALLOCATED(index)) break;
		}
	}
}

/* Allocate and initialize a new serf_t object.
   Return -1 if no more serfs can be allocated, otherwise 0. */
int
game_alloc_serf(serf_t **serf, int *index)
{
	for (int i = 0; i*8 < globals.max_serf_cnt; i++) {
		if (globals.serfs_bitmap[i] != 0xff) {
			for (int j = 0; j < 8; j++) {
				if (!BIT_TEST(globals.serfs_bitmap[i], 7-j)) {
					int ix = 8*i + j;

					if (ix >= globals.max_serf_cnt) return -1; /* TODO looks unneccesary */

					globals.serfs_bitmap[i] |= BIT(7-j);

					if (ix == globals.max_ever_serf_index) globals.max_ever_serf_index += 1;

					serf_t *s = &globals.serfs[ix];

					if (serf != NULL) *serf = s;
					if (index != NULL) *index = ix;

					return 0;
				}
			}
		}
	}

	return -1;
}

/* Return serf_t object with index. */
serf_t *
game_get_serf(int index)
{
	assert(index > 0 && index < globals.max_serf_cnt);
	assert(SERF_ALLOCATED(index));
	return &globals.serfs[index];
}

/* Deallocate and serf_t object. */
void
game_free_serf(int index)
{
	/* Remove serf from allocation bitmap. */
	globals.serfs_bitmap[index/8] &= ~BIT(7-(index&7));

	/* Decrement max_ever_serf_index as much as possible. */
	if (index == globals.max_ever_serf_index + 1) {
		while (--globals.max_ever_serf_index > 0) {
			index -= 1;
			if (SERF_ALLOCATED(index)) break;
		}
	}

	globals.map_max_serfs_left += 1;
}


/* Spawn new serf. Returns 0 on success.
   The serf_t object and inventory are returned if non-NULL. */
int
game_spawn_serf(player_sett_t *sett, serf_t **serf, inventory_t **inventory, int want_knight)
{
	if (!BIT_TEST(sett->build, 2)) return -1;
	if (globals.map_max_serfs_left == 0) return -1;
	if (globals.max_ever_inventory_index < 1) return -1;

	serf_t *s = NULL;
	int r = game_alloc_serf(&s, NULL);
	if (r < 0) return -1;

	building_t *building = NULL;
	inventory_t *inv = NULL;

	for (int i = 0; i < globals.max_ever_inventory_index; i++) {
		if (INVENTORY_ALLOCATED(i)) {
			inventory_t *loop_inv = game_get_inventory(i);
			if (loop_inv->player_num == sett->player_num &&
			    !BIT_TEST(loop_inv->res_dir, 2)) { /* serf IN mode */
				if (want_knight && (loop_inv->resources[RESOURCE_SWORD] == 0 ||
						    loop_inv->resources[RESOURCE_SHIELD] == 0)) {
					continue;
				} else if (loop_inv->spawn_priority == 0) {
					inv = loop_inv;
					break;
				} else if (inv == NULL ||
					   loop_inv->spawn_priority < inv->spawn_priority) {
					inv = loop_inv;
				}
			}
		}
	}

	if (inv == NULL) {
		game_free_serf(SERF_INDEX(s));
		if (want_knight) return game_spawn_serf(sett, serf, inventory, 0);
		else return -1;
	}

	building = game_get_building(inv->bld_index);
	inv->spawn_priority += 1;

	sett->serf_count[SERF_GENERIC] += 1;
	s->type = (SERF_GENERIC << 2) | sett->player_num;
	s->animation = 0;
	s->counter = 0;
	s->pos = building->pos;
	s->anim = globals.anim;
	s->state = SERF_STATE_IDLE_IN_STOCK;
	s->s.idle_in_stock.inv_index = INVENTORY_INDEX(inv);

	if (serf) *serf = s;
	if (inventory) *inventory = inv;

	return 0;
}

/* Update player game state as part of the game progression. */
static void
update_player_sett(player_sett_t *sett)
{
	if (sett->total_land_area > 0xffff0000) sett->total_land_area = 0;
	if (sett->total_military_score > 0xffff0000) sett->total_military_score = 0;
	if (sett->total_building_score > 0xffff0000) sett->total_building_score = 0;

	if (!BIT_TEST(sett->flags, 6)) return; /* Inactive */

	if (BIT_TEST(sett->flags, 7)) { /* AI */
		/*if (sett->field_1B2 != 0) sett->field_1B2 -= 1;*/
		/*if (sett->field_1B0 != 0) sett->field_1B0 -= 1;*/
	}

	if (BIT_TEST(sett->flags, 2)) {
		sett->field_170 -= 1;
		if (sett->field_170 == 0) {
			sett->flags &= ~BIT(5);
			sett->flags &= ~BIT(2);
		} else if (sett->field_170 == 1023) {
			sett->flags |= BIT(5);
			sett->flags &= ~BIT(4);
		}
	}

	if (PLAYER_HAS_CASTLE(sett)) {
		uint16_t delta = globals.anim - sett->last_anim;
		sett->last_anim = globals.anim;
		sett->reproduction_counter -= delta;

		while (sett->reproduction_counter < 0) {
			sett->serf_to_knight_counter += sett->serf_to_knight_rate;
			if (sett->serf_to_knight_counter < sett->serf_to_knight_rate) {
				sett->knights_to_spawn += 1;
				if (sett->knights_to_spawn > 2) sett->knights_to_spawn = 2;
			}

			if (sett->knights_to_spawn == 0) {
				/* Create unassigned serf */
				game_spawn_serf(sett, NULL, NULL, 0);
			} else {
				/* Create knight serf */
				serf_t *serf;
				inventory_t *inventory;
				int r = game_spawn_serf(sett, &serf, &inventory, 1);
				if (r >= 0) {
					if (inventory->resources[RESOURCE_SWORD] != 0 &&
					    inventory->resources[RESOURCE_SHIELD] != 0) {
						sett->knights_to_spawn -= 1;
						serf->type = (serf->type & 0x83) | (SERF_KNIGHT_0 << 2);
						sett->serf_count[SERF_GENERIC] -= 1;
						sett->serf_count[SERF_KNIGHT_0] += 1;
						inventory->resources[RESOURCE_SWORD] -= 1;
						inventory->resources[RESOURCE_SHIELD] -= 1;
						inventory->spawn_priority -= 1;
					}
				}
			}

			sett->reproduction_counter += sett->reproduction_reset;
		}
	}
}

static void
check_win_and_flags_buildings()
{
	/* TODO */

	/* TODO Approximately right */
	for (int i = 1; i < globals.max_ever_building_index; i++) {
		if (BUILDING_ALLOCATED(i)) {
			building_t *building = game_get_building(i);
			building->serf &= ~BIT(2);
		}
	}

	/* TODO Approximately right */
	for (int i = 1; i < globals.max_ever_flag_index; i++) {
		if (FLAG_ALLOCATED(i)) {
			flag_t *flag = game_get_flag(i);
			flag->transporter &= ~BIT(7);
		}
	}
}

static void
update_knight_morale()
{
	for (int i = 0; i < 3; i++) {
		player_sett_t *sett = globals.player_sett[i];
		int depot = sett->military_gold + sett->inventory_gold;
		sett->gold_deposited = min(depot, 0xffff);

		/* Calculate according to gold collected. */
		int map_gold = globals.map_gold_deposit;
		if (map_gold != 0) {
			while (map_gold > 0xffff) {
				map_gold >>= 1;
				depot >>= 1;
			}
			depot = min(depot, map_gold-1);
			sett->knight_morale = 1024 + (globals.map_gold_morale_factor * depot)/map_gold;
		} else {
			sett->knight_morale = 4096;
		}

		/* Adjust based on castle score. */
		int castle_score = sett->castle_score;
		if (castle_score < 0) {
			sett->knight_morale = max(1, sett->knight_morale - 1023);
		} else if (castle_score > 0) {
			sett->knight_morale = min(sett->knight_morale + 1024*castle_score, 0xffff);
		}

		int military_score = sett->total_military_score;
		int morale = sett->knight_morale >> 5;
		while (military_score > 0xffff) {
			military_score >>= 1;
			morale <<= 1;
		}

		/* TODO */

		sett->military_gold = 0;
		sett->military_max_gold = 0;
		sett->inventory_gold = 0;
	}
}

static void
update_map_and_players()
{
	check_win_and_flags_buildings();
	/* sub_1EF25(); */
	map_update();

	update_player_sett(globals.player_sett[0]);
	update_player_sett(globals.player_sett[1]);
	update_player_sett(globals.player_sett[2]);
	update_player_sett(globals.player_sett[3]);
}

typedef struct {
	int resource;
	int *max_prio;
	flag_t **flags;
} update_ai_and_more_data_t;

static int
update_ai_and_more_search_cb(flag_t *flag, update_ai_and_more_data_t *data)
{
	int inv = flag->search_dir;
	if (data->max_prio[inv] < 255 &&
	    FLAG_HAS_BUILDING(flag)) {
		building_t *building = flag->other_endpoint.b[DIR_UP_LEFT];

		for (int i = 0; i < 2; i++) {
			if (building->stock[i].type == data->resource &&
			    building->stock[i].prio >= 16 &&
			    building->stock[i].prio > data->max_prio[inv]) {
				data->max_prio[inv] = building->stock[i].prio;
				data->flags[inv] = flag;
			}
		}
	}

	return 0;
}

static void
update_ai_and_more()
{
	const int arr_1[] = {
		RESOURCE_PLANK,
		RESOURCE_STONE,
		RESOURCE_STEEL,
		RESOURCE_COAL,
		RESOURCE_LUMBER,
		RESOURCE_IRONORE,
		RESOURCE_GROUP_FOOD,
		RESOURCE_PIG,
		RESOURCE_FLOUR,
		RESOURCE_WHEAT,
		RESOURCE_GOLDBAR,
		RESOURCE_GOLDORE,
		-1
	};

	const int arr_2[] = {
		RESOURCE_STONE,
		RESOURCE_IRONORE,
		RESOURCE_GOLDORE,
		RESOURCE_COAL,
		RESOURCE_STEEL,
		RESOURCE_GOLDBAR,
		RESOURCE_GROUP_FOOD,
		RESOURCE_PIG,
		RESOURCE_FLOUR,
		RESOURCE_WHEAT,
		RESOURCE_LUMBER,
		RESOURCE_PLANK,
		-1
	};

	const int arr_3[] = {
		RESOURCE_GROUP_FOOD,
		RESOURCE_WHEAT,
		RESOURCE_PIG,
		RESOURCE_FLOUR,
		RESOURCE_GOLDBAR,
		RESOURCE_STONE,
		RESOURCE_PLANK,
		RESOURCE_STEEL,
		RESOURCE_COAL,
		RESOURCE_LUMBER,
		RESOURCE_GOLDORE,
		RESOURCE_IRONORE,
		-1
	};

	globals.next_index += 1;
	if (globals.next_index >= globals.max_next_index) {
		globals.next_index = 0;
	}

	if (globals.next_index == 32) {
		/* 1FC1D */
		update_knight_morale();
		if (globals.game_speed == 0) return;
		/* update functions */

		const int *arr = NULL;
		switch (random_int() & 7) {
		case 0: arr = arr_2; break;
		case 1: arr = arr_3; break;
		default: arr = arr_1; break;
		}

		while (arr[0] >= 0) {
			for (int p = 0; p < 4; p++) {
				/*player_sett_t *sett = globals.player_sett[p];*/
				inventory_t *invs[256];
				int n = 0;
				for (int i = 0; i < globals.max_ever_inventory_index; i++) {
					if (INVENTORY_ALLOCATED(i)) {
						inventory_t *inventory = game_get_inventory(i);
						if (inventory->player_num == p &&
						    inventory->out_queue[1] == -1) {
							int res_dir = inventory->res_dir & 3;
							if (res_dir == 0 || res_dir == 1) { /* In mode, stop mode */
								if (arr[0] == RESOURCE_GROUP_FOOD) {
									if (inventory->resources[RESOURCE_FISH] != 0 ||
									    inventory->resources[RESOURCE_MEAT] != 0 ||
									    inventory->resources[RESOURCE_BREAD] != 0) {
										invs[n++] = inventory;
										if (n == 256) break;
									}
								} else if (inventory->resources[arr[0]] != 0) {
									invs[n++] = inventory;
									if (n == 256) break;
								}
							} else {
								/* TODO */
							}
						}
					}
				}

				if (n > 0) {
					flag_search_t search;
					flag_search_init(&search);

					int max_prio[256];
					flag_t *flags[256];

					for (int i = 0; i < n; i++) {
						max_prio[i] = 0;
						flags[i] = NULL;
						flag_t *flag = game_get_flag(invs[i]->flg_index);
						flag->search_dir = i;
						flag_search_add_source(&search, flag);
					}

					update_ai_and_more_data_t data;
					data.resource = arr[0];
					data.max_prio = max_prio;
					data.flags = flags;
					flag_search_execute(&search, (flag_search_func *)update_ai_and_more_search_cb, 0, 1, &data);

					for (int i = 0; i < n; i++) {
						if (max_prio[i] > 0) {
							LOGV("game", " dest for inventory %i found", i);
							building_t *dest_bld = flags[i]->other_endpoint.b[DIR_UP_LEFT];
							inventory_t *src_inv = invs[i];
							for (int j = 0; j < 2; j++) {
								if (dest_bld->stock[j].type == arr[0]) {
									dest_bld->stock[j].prio = 0;
									dest_bld->stock[j].requested += 1;
								}
							}

							resource_type_t res = arr[0];
							if (res == RESOURCE_GROUP_FOOD) {
								/* Select the food resource with highest amount available */
								if (src_inv->resources[RESOURCE_MEAT] > src_inv->resources[RESOURCE_BREAD]) {
									if (src_inv->resources[RESOURCE_MEAT] > src_inv->resources[RESOURCE_FISH]) {
										res = RESOURCE_MEAT;
									} else {
										res = RESOURCE_FISH;
									}
								} else if (src_inv->resources[RESOURCE_BREAD] > src_inv->resources[RESOURCE_FISH]) {
									res = RESOURCE_BREAD;
								} else {
									res = RESOURCE_FISH;
								}
							}

							/* Put resource in out queue */
							src_inv->resources[res] -= 1;
							if (src_inv->out_queue[0] == -1) {
								LOGV("game", " added resource %i to front of queue", res);
								src_inv->out_queue[0] = res;
								src_inv->out_dest[0] = dest_bld->flg_index;
							} else {
								LOGV("game", " added resource %i next in queue", res);
								src_inv->out_queue[1] = res;
								src_inv->out_dest[1] = dest_bld->flg_index;
							}

							if (src_inv->serfs[SERF_4] == 0) {
								/*serf_t *serf = game_get_serf(dest_bld->serf_index);*/
								/* TODO */
							}
						}
					}
				}
			}
			arr += 1;
		}
	} else if (globals.next_index > 32) {
		while (globals.next_index < globals.max_next_index) {
			int i = 33 - globals.next_index;
			player_sett_t *sett = globals.player_sett[i & 3];
			if (BIT_TEST(sett->flags, 6) && BIT_TEST(sett->flags, 7)) { /* Active and AI */
				/* AI */
				/* TODO */
			}
			globals.next_index += 1;
		}
	} else if (globals.game_speed > 0 &&
		   globals.max_ever_flag_index < 50) {
		player_sett_t *sett = globals.player_sett[globals.next_index & 3];
		if (BIT_TEST(sett->flags, 6) && BIT_TEST(sett->flags, 7)) { /* Active and AI */
			/* AI */
			/* TODO */
		}
	}
}

typedef struct {
	inventory_t *inventory;
	int serf_index;
	int water;
} send_serf_to_road_data_t;

static int
send_serf_to_road_search_cb(flag_t *flag, send_serf_to_road_data_t *data)
{
	if (FLAG_HAS_INVENTORY(flag)) {
		/* Inventory reached */
		building_t *building = flag->other_endpoint.b[DIR_UP_LEFT];
		inventory_t *inventory = building->u.inventory;
		if (!data->water) {
			if (inventory->serfs[SERF_TRANSPORTER] != 0) {
				data->inventory = inventory;
				data->serf_index = inventory->serfs[SERF_TRANSPORTER];
				inventory->serfs[SERF_TRANSPORTER] = 0;
				return 1;
			}
		} else {
			if (inventory->serfs[SERF_SAILOR] != 0) {
				data->inventory = inventory;
				data->serf_index = inventory->serfs[SERF_SAILOR];
				inventory->serfs[SERF_SAILOR] = 0;
				return 1;
			}
		}

		if (data->inventory == NULL && inventory->serfs[SERF_GENERIC] != 0 &&
		    (!data->water || inventory->resources[RESOURCE_BOAT] > 0)) {
			data->inventory = inventory;
			/*player_sett_t *sett = globals.player_sett[inventory->player_num];
			globals.field_340 = sett->cont_search_after_non_optimal_find;*/
		}
	}

	return 0;
}

static int
send_serf_to_road(flag_t *src, dir_t dir, int water)
{
	flag_t *src_2 = src->other_endpoint.f[dir];
	dir_t dir_2 = FLAG_OTHER_END_DIR(src, dir);

	src->search_dir = 0;
	src_2->search_dir = 1;

	flag_search_t search;
	flag_search_init(&search);
	flag_search_add_source(&search, src);
	flag_search_add_source(&search, src_2);

	send_serf_to_road_data_t data;
	data.inventory = NULL;
	data.serf_index = -1;
	data.water = water;

	int r = flag_search_execute(&search, (flag_search_func *)send_serf_to_road_search_cb, 1, 0, &data);
	inventory_t *inventory = data.inventory;
	int serf_index = data.serf_index;
	if (r < 0) {
		if (inventory == NULL) return -1;

		player_sett_t *sett = globals.player_sett[inventory->player_num];

		sett->serf_count[SERF_GENERIC] -= 1;
		serf_index = inventory->serfs[SERF_GENERIC];
		inventory->serfs[SERF_GENERIC] = 0;

		serf_t *serf = game_get_serf(serf_index);

		if (!water) {
			serf->type = (serf->type & 0x83) | (SERF_TRANSPORTER << 2);
			sett->serf_count[SERF_TRANSPORTER] += 1;
		} else {
			serf->type = (serf->type & 0x83) | (SERF_SAILOR << 2);
			sett->serf_count[SERF_SAILOR] += 1;
			inventory->resources[RESOURCE_BOAT] -= 1;
		}

		inventory->spawn_priority -= 1;
	}

	inventory->serfs[SERF_4] += 1;
	flag_t *dest_flag = game_get_flag(inventory->flg_index);

	src->length[dir] |= BIT(7);
	src_2->length[dir_2] |= BIT(7);

	if (dest_flag->search_dir == src_2->search_dir) {
		src = src_2;
		dir = dir_2;
	}

	serf_t *serf = game_get_serf(serf_index);

	serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE_INVENTORY);
	serf->state = SERF_STATE_READY_TO_LEAVE_INVENTORY;
	serf->s.ready_to_leave_inventory.mode = dir;
	serf->s.ready_to_leave_inventory.dest = FLAG_INDEX(src);
	serf->s.ready_to_leave_inventory.inv_index = INVENTORY_INDEX(inventory);

	return 0;
}

static int
find_nearest_inventory_search_cb(flag_t *flag, flag_t **dest)
{
	if (FLAG_ACCEPTS_RESOURCES(flag)) {
		*dest = flag;
		return 1;
	}
	return 0;
}

/* Return the flag index of the inventory nearest to flag. */
static int
find_nearest_inventory(flag_t *flag)
{
	flag_t *dest = NULL;
	flag_search_single(flag, (flag_search_func *)find_nearest_inventory_search_cb,
			   0, 1, &dest);
	if (dest != NULL) return FLAG_INDEX(dest);

	return -1;
}

typedef struct {
	flag_t *src;
	flag_t *dest;
	int res;
} update_flags_search_data_t;

static int
update_flags_search_cb(flag_t *flag, update_flags_search_data_t *data)
{
	flag_t *src = data->src;
	if (flag == data->dest) {
		LOGV("game", "update flags: dest found: %i", flag->search_dir);

		if (flag->search_dir != 6) {
			int other_dir = src->other_end_dir[flag->search_dir];
			if (!BIT_TEST(other_dir, 7)) {
				src->other_end_dir[flag->search_dir] = BIT(7) | (src->other_end_dir[flag->search_dir] & 0x78) | data->res;
				LOGV("game", "update flags: item %i is requesting fetch", data->res);
			} else {
				player_sett_t *sett = globals.player_sett[FLAG_PLAYER(flag)];
				int prio_old = sett->flag_prio[(src->res_waiting[other_dir & 7] & 0x1f)-1];
				int prio_new = sett->flag_prio[(src->res_waiting[data->res] & 0x1f)-1];
				if (prio_new > prio_old) {
					src->other_end_dir[flag->search_dir] = (src->other_end_dir[flag->search_dir] & 0xf8) | data->res;
					LOGV("game", "update flags: item %i has priority now", data->res);
				}
				src->res_waiting[data->res] = ((flag->search_dir + 1) << 5) | (src->res_waiting[data->res] & 0x1f);
			}
		}
		return 1;
	}

	return 0;
}

typedef struct {
	int resource;
	int max_prio;
	flag_t *flag;
} update_flags_search2_t;

static int
update_flags_search2_cb(flag_t *flag, update_flags_search2_t *data)
{
	if (FLAG_HAS_BUILDING(flag)) {
		building_t *building = flag->other_endpoint.b[DIR_UP_LEFT];

		for (int i = 0; i < 2; i++) {
			if (building->stock[i].type == data->resource &&
			    building->stock[i].prio > data->max_prio) {
				data->max_prio = building->stock[i].prio;
				data->flag = flag;
			}
		}

		if (data->max_prio > 204) return 1;
	}

	return 0;
}

/* Update flags as part of the game progression. */
static void
update_flags()
{
	const int max_transporters[] = { 1, 2, 3, 4, 6, 8, 11, 15 };

	const int routable[] = {
		1, 1, 1, 1, 1, 1, 1, 1,
		0, 1, 1, 1, 1, 1, 1, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0
	};

	if (globals.next_index >= 32) return;

	int index = globals.next_index << 5;
	for (int i = index ? index : 1; i < globals.max_ever_flag_index; i++) {
		if (FLAG_ALLOCATED(i)) {
			flag_t *flag = game_get_flag(i);

			for (int j = 0; j < 4; j++) globals.field_218[j] = 0;

			for (int j = 0; j < 8; j++) {
				int res_dir = (flag->res_waiting[j] >> 5) - 1;
				if (res_dir >= 0) {
					for (int k = 3; k >= 0; k--) {
						if (!BIT_TEST(globals.field_218[k], res_dir)) {
							globals.field_218[k] |= BIT(res_dir);
							break;
						}
					}
				}
			}

			globals.field_24E = 0;

			if (FLAG_HAS_RESOURCES(flag)) {
				flag->endpoint &= ~BIT(7);
				for (int slot = 7; slot >= 0; slot--) {
					if (flag->res_waiting[slot] != 0) {
						globals.field_24E += 1;

						/* Only schedule the slot if it has not already
						   been scheduled for fetch. */
						if (((flag->res_waiting[slot] >> 5) & 7) == 0) {
							if (flag->res_dest[slot] != 0) { /* Destination is known */
								flag_search_t search;
								flag_search_init(&search);

								flag->search_num = search.id;
								flag->search_dir = 6;
								int tr = FLAG_TRANSPORTERS(flag);

								int sources = 0;
								int flags = (globals.field_218[3] ^ 0x3f) & flag->transporter;
								if (flags != 0) {
									for (int k = 0; k < 6; k++) {
										if (BIT_TEST(flags, 5-k)) {
											tr &= ~BIT(5-k);
											flag_t *other_flag = flag->other_endpoint.f[5-k];
											if (other_flag->search_num != search.id) {
												other_flag->search_dir = 5-k;
												flag_search_add_source(&search, other_flag);
												sources += 1;
											}
										}
									}
								}

								if (tr != 0) {
									for (int j = 0; j < 3; j++) {
										flags = (globals.field_218[3-j] ^ globals.field_218[2-j]);
										for (int k = 0; k < 6; k++) {
											if (BIT_TEST(flags, 5-k)) {
												tr &= ~BIT(5-k);
												flag_t *other_flag = flag->other_endpoint.f[5-k];
												if (other_flag->search_num != search.id) {
													other_flag->search_dir = 5-k;
													flag_search_add_source(&search, other_flag);
													sources += 1;
												}
											}
										}
									}

									if (tr != 0) {
										flags = globals.field_218[0];
										for (int k = 0; k < 6; k++) {
											if (BIT_TEST(flags, 5-k)) {
												tr &= ~BIT(5-k);
												flag_t *other_flag = flag->other_endpoint.f[5-k];
												if (other_flag->search_num != search.id) {
													other_flag->search_dir = 5-k;
													flag_search_add_source(&search, other_flag);
													sources += 1;
												}
											}
										}
										if (flags == 0) return;
									}
								}

								if (sources > 0) {
									update_flags_search_data_t data;
									data.src = flag;
									data.dest = game_get_flag(flag->res_dest[slot]);
									data.res = slot;
									int r = flag_search_execute(&search,
												    (flag_search_func *)update_flags_search_cb,
												    0, 1, &data);
									if (r < 0 || data.dest->search_dir == 6) {
										LOGD("game", "update flags: unable to deliver.");
										flag_cancel_transported_stock(data.dest, flag->res_waiting[slot] & 0x1f);
										flag->res_dest[slot] = 0;
										flag->endpoint |= BIT(7);
									}
								} else {
									flag->endpoint |= BIT(7);
								}
							} else { /* Destination is not known */
								resource_type_t res = (flag->res_waiting[slot] & 0x1f)-1;
								if (routable[res]) {
									flag_search_t search;
									flag_search_init(&search);
									flag_search_add_source(&search, flag);

									update_flags_search2_t data;
									data.resource = res;
									data.flag = NULL;
									data.max_prio = 0;

									flag_search_execute(&search,
											    (flag_search_func *)update_flags_search2_cb,
											    0, 1, &data);
									if (data.flag != NULL) {
										LOGV("game", "dest for flag %u res %i found: flag %u",
										     FLAG_INDEX(flag), slot, FLAG_INDEX(data.flag));
										building_t *dest_bld = data.flag->other_endpoint.b[DIR_UP_LEFT];
										int prio = 0;
										for (int i = 0; i < 2; i++) {
											if (dest_bld->stock[i].type == res) {
												prio = dest_bld->stock[i].prio;
												if ((prio & 1) == 0) prio = 0;
												dest_bld->stock[i].prio = prio >> 1;
												dest_bld->stock[i].requested += 1;
											}
										}

										flag->res_dest[slot] = dest_bld->flg_index;
										flag->endpoint |= BIT(7);
									} else { /* No flag requests this resource */
										int r = find_nearest_inventory(flag);
										if (r < 0) {
											/* TODO */
										} else {
											flag->res_dest[slot] = r;
											flag->endpoint |= BIT(7);
										}
									}
								} else { /* This resource can not be requested by a flag */
									int r = find_nearest_inventory(flag);
									if (r < 0) {
										/* TODO */
									} else {
										flag->res_dest[slot] = r;
										flag->endpoint |= BIT(7);
									}
								}
							}
						}
					}
				}
			}

			/* Update transporter flags, decide if serf needs to be sent to road */
			int tr = flag->transporter;
			int flags = globals.field_218[1];
			int path = FLAG_PATHS(flag);
			if (globals.field_24E >= 7) path |= BIT(7);

			for (int j = 0; j < 6; j++) {
				if (BIT_TEST(path, 5-j)) {
					if (FLAG_SERF_REQUESTED(flag, 5-j)) {
						if (BIT_TEST(flags, 5-j)) {
							if (BIT_TEST(path, 7)) tr &= BIT(5-j);
						} else {
							if (FLAG_TRANSPORTER_COUNT(flag, 5-j) != 0) tr |= BIT(5-j);
						}
					} else if (FLAG_TRANSPORTER_COUNT(flag, 5-j) == 0) {
						if (!BIT_TEST(tr, 7)) {
							int r = send_serf_to_road(flag, 5-j, FLAG_IS_WATER_PATH(flag, 5-j));
							if (r < 0) tr |= BIT(7);
						}
						if (BIT_TEST(path, 7)) tr &= BIT(5-j);
					} else if (BIT_TEST(flags, 5-j)) {
						int max_tr = max_transporters[FLAG_LENGTH_CATEGORY(flag, 5-j)];
						if (FLAG_TRANSPORTER_COUNT(flag, 5-j) != max_tr) {
							if (!BIT_TEST(tr, 7)) {
								int r = send_serf_to_road(flag, 5-j, FLAG_IS_WATER_PATH(flag, 5-j));
								if (r < 0) tr |= BIT(7);
							}
						}
						if (BIT_TEST(path, 7)) tr &= BIT(5-j);
					} else {
						tr |= BIT(5-j);
					}
				}
			}

			flag->transporter = tr;
		}
	}
}

typedef struct {
	inventory_t *inventory;
	building_t *building;
	int serf_type;
	int dest_index;
	resource_type_t res1;
	resource_type_t res2;
} send_serf_to_flag_data_t;

static int
send_serf_to_flag_search_cb(flag_t *flag, send_serf_to_flag_data_t *data)
{
	if (FLAG_HAS_INVENTORY(flag)) {
		/* Inventory reached */
		building_t *building = flag->other_endpoint.b[DIR_UP_LEFT];
		inventory_t *inv = building->u.inventory;

		int type = data->serf_type;
		if (type < 0) {
			int knight_type = -1;
			for (int i = 4; i >= -type-1; i--) {
				if (inv->serfs[SERF_KNIGHT_0+i] != 0) {
					knight_type = i;
					break;
				}
			}

			if (knight_type >= 0) {
				/* Knight of appropriate type was found. */
				serf_t *serf = game_get_serf(inv->serfs[SERF_KNIGHT_0+knight_type]);
				inv->serfs[SERF_KNIGHT_0+knight_type] = 0;

				data->building->stock[0].requested += 1;
				data->building->serf &= ~BIT(7);

				serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE_INVENTORY);
				serf->state = SERF_STATE_READY_TO_LEAVE_INVENTORY;
				serf->s.ready_to_leave_inventory.mode = -1;
				serf->s.ready_to_leave_inventory.dest = data->building->flg_index;
				serf->s.ready_to_leave_inventory.inv_index = INVENTORY_INDEX(inv);

				inv->serfs[SERF_4] += 1;

				return 1;
			} else if (type == -1) {
				/* See if a knight can be created here. */
				if (/*globals.field_342 == 0*/1 &&
				    inv->serfs[SERF_GENERIC] != 0 &&
				    inv->resources[RESOURCE_SWORD] > 0 &&
				    inv->resources[RESOURCE_SHIELD] > 0) {
					data->inventory = inv;
					/* player_sett_t *sett = globals->player_sett[SERF_PLAYER(serf)]; */
					/* globals.field_340 = sett->cont_search_after_non_optimal_find; */
				}
			}
		} else {
			if (inv->serfs[type] != 0) {
				if (type != SERF_GENERIC || inv->spawn_priority > 4) {
					serf_t *serf = game_get_serf(inv->serfs[type]);

					serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE_INVENTORY);
					serf->state = SERF_STATE_READY_TO_LEAVE_INVENTORY;
					serf->s.ready_to_leave_inventory.inv_index = INVENTORY_INDEX(inv);
					serf->s.ready_to_leave_inventory.dest = data->dest_index;

					inv->serfs[type] = 0;
					if (type == SERF_GENERIC) {
						serf->s.ready_to_leave_inventory.mode = -2;
						inv->spawn_priority -= 1;
					} else if (type == SERF_GEOLOGIST) {
						serf->s.ready_to_leave_inventory.mode = 6;
					} else {
						building_t *dest_bld = game_get_flag(data->dest_index)->other_endpoint.b[DIR_UP_LEFT];
						dest_bld->serf |= BIT(7);
						serf->s.ready_to_leave_inventory.mode = -1;
					}

					inv->serfs[SERF_4] += 1;
					return 1;
				}
			} else {
				if (data->inventory == NULL &&
				    inv->serfs[SERF_GENERIC] != 0 &&
				    (data->res1 == -1 || inv->resources[data->res1] > 0) &&
				    (data->res2 == -1 || inv->resources[data->res2] > 0)) {
					data->inventory = inv;
					/* player_sett_t *sett = globals->player_sett[SERF_PLAYER(serf)]; */
					/* globals.field_340 = sett->cont_search_after_non_optimal_find; */
				}
			}
		}
	}

	return 0;
}

/* Dispatch serf from (nearest?) inventory to flag. */
static int
send_serf_to_flag(flag_t *dest, building_t *building, int dest_index,
		  int type, resource_type_t res1, resource_type_t res2)
{
	/* If type is negative, building is non-NULL. */
	if (type < 0) {
		player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
		if (BIT_TEST(sett->flags, 5)) {
			type = -((sett->field_170 >> 8) + 1);
		}
	}

	send_serf_to_flag_data_t data;
	data.inventory = NULL;
	data.building = building;
	data.serf_type = type;
	data.dest_index = dest_index;
	data.res1 = res1;
	data.res2 = res2;

	int r = flag_search_single(dest, (flag_search_func *)send_serf_to_flag_search_cb, 1, 0, &data);
	if (r == 0) {
		return 0;
	} else if (data.inventory != NULL) {
		inventory_t *inventory = data.inventory;
		serf_t *serf = game_get_serf(inventory->serfs[SERF_GENERIC]);

		inventory->serfs[SERF_GENERIC] = 0;
		inventory->spawn_priority -= 1;

		if (type < 0) {
			/* Knight */
			building->stock[0].requested += 1;
			building->serf &= ~BIT(7);

			serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE_INVENTORY);
			serf->state = SERF_STATE_READY_TO_LEAVE_INVENTORY;
			serf->s.ready_to_leave_inventory.mode = -1;
			serf->s.ready_to_leave_inventory.dest = building->flg_index;
			serf->s.ready_to_leave_inventory.inv_index = INVENTORY_INDEX(inventory);
			serf->type = (serf->type & 0x83) | (SERF_KNIGHT_0 << 2);

			inventory->resources[RESOURCE_SWORD] -= 1;
			inventory->resources[RESOURCE_SHIELD] -= 1;
			inventory->serfs[SERF_4] += 1;

			player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
			sett->serf_count[SERF_GENERIC] -= 1;
			sett->serf_count[SERF_KNIGHT_0] += 1;
			sett->total_military_score += 1;
		} else {
			serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE_INVENTORY);
			serf->state = SERF_STATE_READY_TO_LEAVE_INVENTORY;
			serf->s.ready_to_leave_inventory.inv_index = INVENTORY_INDEX(inventory);

			if (type == SERF_GEOLOGIST) {
				serf->s.ready_to_leave_inventory.mode = 6;
				serf->s.ready_to_leave_inventory.dest = dest_index;
			} else {
				building_t *dest_bld = game_get_flag(dest_index)->other_endpoint.b[DIR_UP_LEFT];
				dest_bld->serf |= BIT(7);
				serf->s.ready_to_leave_inventory.mode = -1;
				serf->s.ready_to_leave_inventory.dest = dest_index;
			}
			serf->type = (serf->type & 0x83) | (type << 2);

			if (res1 != -1) inventory->resources[res1] -= 1;
			if (res2 != -1) inventory->resources[res2] -= 1;

			inventory->serfs[SERF_4] += 1;

			player_sett_t *sett = globals.player_sett[SERF_PLAYER(serf)];
			sett->serf_count[SERF_GENERIC] -= 1;
			sett->serf_count[type] += 1;
		}

		return 0;
	}

	return -1;
}

/* Dispatch geologist to flag. */
int
game_send_geologist(flag_t *dest, int dest_index)
{
	return send_serf_to_flag(dest, NULL, dest_index, SERF_GEOLOGIST, RESOURCE_HAMMER, -1);
}

/* Dispatch serf to building. */
static int
send_serf_to_building(building_t *building, int type, resource_type_t res1, resource_type_t res2)
{
	flag_t *dest = game_get_flag(building->flg_index);
	return send_serf_to_flag(dest, building, building->flg_index, type, res1, res2);
}

/* Update unfinished building as part of the game progression. */
static void
update_unfinished_building(building_t *building)
{
	player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];

	/* Request builder serf */
	if (!BUILDING_SERF_REQUEST_FAIL(building) &&
	    !BUILDING_HAS_SERF(building) &&
	    !BUILDING_SERF_REQUESTED(building)) {
		building->progress = 1;
		/*if (BIT_TEST(sett->field_163, 6) &&
				sett->lumberjack_index != BUILDING_INDEX(building) &&
				sett->sawmill_index != BUILDING_INDEX(building) &&
				sett->stonecutter_index != BUILDING_INDEX(building)) {
			building->serf |= BIT(2);
			return;
		}*/

		int r = send_serf_to_building(building, SERF_BUILDER, RESOURCE_HAMMER, -1);
		if (r < 0) building->serf |= BIT(2);
	}

	/* Request planks */
	/*if (BIT_TEST(sett->field_163, 1) &&
			sett->lumberjack_index != BUILDING_INDEX(building) &&
			sett->sawmill_index != BUILDING_INDEX(building) &&
			sett->stonecutter_index != BUILDING_INDEX(building)) {
		TODO
	}*/

	int total_planks = building->stock[0].requested + building->stock[0].available;
	if (total_planks < 8 && total_planks != building->u.s.planks_needed) {
		int planks_prio = sett->planks_construction >> (8 + total_planks);
		if (!BUILDING_HAS_SERF(building)) planks_prio >>= 2;
		building->stock[0].prio = planks_prio & ~BIT(0);
	} else {
		building->stock[0].prio = 0;
	}

	/* Request stone */
	/*if (BIT_TEST(sett->field_163, 2) &&
			sett->lumberjack_index != BUILDING_INDEX(building) &&
			sett->sawmill_index != BUILDING_INDEX(building) &&
			sett->stonecutter_index != BUILDING_INDEX(building)) {
		TODO
	}*/

	int total_stone = building->stock[1].requested + building->stock[1].available;
	if (total_stone < 8 && total_stone != building->u.s.stone_needed) {
		int stone_prio = 0xff >> total_stone;
		if (!BUILDING_HAS_SERF(building)) stone_prio >>= 2;
		building->stock[1].prio = stone_prio & ~BIT(0);
	} else {
		building->stock[1].prio = 0;
	}
}

static void
update_unfinished_adv_building(building_t *building)
{
	/*player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];*/

	if (building->progress > 0) {
		update_unfinished_building(building);
		return;
	}

	if (BUILDING_HAS_SERF(building) ||
	    BUILDING_SERF_REQUESTED(building)) {
		return;
	}

	/* TODO */

	if (!BUILDING_SERF_REQUEST_FAIL(building)) {
		/*if (BIT_TEST(sett->field_163, 6) &&
		  BUILDING_TYPE(building) != BUILDING_LUMBERJACK &&
		  BUILDING_TYPE(building) != BUILDING_STONECUTTER &&
		  BUILDING_TYPE(building) != BUILDING_SAWMILL) {
			building->serf |= BIT(2);
			return;
		}*/
		int r = send_serf_to_building(building, SERF_DIGGER, RESOURCE_SHOVEL, -1);
		if (r < 0) building->serf |= BIT(2);
	}
}

/* Update castle as part of the game progression. */
static void
update_building_castle(building_t *building)
{
	player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
	if (sett->castle_knights == sett->castle_knights_wanted) {
		/* TODO ... */
	} else if (sett->castle_knights < sett->castle_knights_wanted) {
		inventory_t *inventory = building->u.inventory;
		int type = -1;
		for (serf_type_t t = SERF_KNIGHT_4; t >= SERF_KNIGHT_0; t--) {
			if (inventory->serfs[t] != 0) {
				type = t;
				break;
			}
		}

		if (type < 0) { /* None found */
			/* TODO */
		} else {
			/* Prepend to knights list */
			int serf_index = inventory->serfs[type];
			serf_t *serf = game_get_serf(serf_index);

			serf_log_state_change(serf, SERF_STATE_DEFENDING_CASTLE);
			serf->state = SERF_STATE_DEFENDING_CASTLE;
			serf->s.defending.next_knight = building->serf_index;
			serf->counter = 6000;
			building->serf_index = serf_index;
			sett->castle_knights += 1;
			inventory->serfs[type] = 0; /* Clear inventory pointer */
		}
	} else {
		/* TODO ... */
	}

	/* TODO */

	inventory_t *inventory = building->u.inventory;

	if (BUILDING_HAS_SERF(building) &&
	    (inventory->res_dir & 0xa) == 0 && /* Not serf or res OUT mode */
	    inventory->spawn_priority == 0) {
		/* sett->field_160 -= 1; */
		if (0/*sett->field_160 < 0*/) {
			/* sett->field_160 = 5; */
			send_serf_to_building(building, SERF_GENERIC, -1, -1);
		}
	}

	sett->inventory_gold += inventory->resources[RESOURCE_GOLDBAR];

	map_pos_t flag_pos = MAP_MOVE_DOWN_RIGHT(building->pos);
	if (MAP_SERF_INDEX(flag_pos) != 0) {
		serf_t *serf = game_get_serf(MAP_SERF_INDEX(flag_pos));
		if (serf->pos != flag_pos) map_set_serf_index(flag_pos, 0);
	}
}

static void
handle_building_update(building_t *building)
{
	building_type_t type = BUILDING_TYPE(building);
	if (BUILDING_IS_DONE(building)) {
		switch (type) {
		case BUILDING_NONE:
			break;
		case BUILDING_FISHER:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_FISHER,
							      RESOURCE_ROD, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			break;
		case BUILDING_LUMBERJACK:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_LUMBERJACK,
							      RESOURCE_AXE, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			break;
		case BUILDING_BOATBUILDER:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_BOATBUILDER,
							      RESOURCE_HAMMER, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BUILDING_HAS_SERF(building)) {
				player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
				int total_tree = building->stock[0].requested + building->stock[0].available;
				if (total_tree < 8 && 1/*!BIT_TEST(sett->field_163, 1)*/) {
					building->stock[0].prio = sett->planks_boatbuilder >> (8 + total_tree);
				} else {
					building->stock[0].prio = 0;
				}
			}
			break;
		case BUILDING_STONECUTTER:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_STONECUTTER,
							      RESOURCE_PICK, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			break;
		case BUILDING_STONEMINE:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_MINER,
							      RESOURCE_PICK, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BUILDING_HAS_SERF(building)) {
				int total_food = building->stock[0].requested + building->stock[0].available;
				if (total_food < 8) {
					player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
					building->stock[0].prio = sett->food_stonemine >> (8 + total_food);
				} else {
					building->stock[0].prio = 0;
				}
			}
			break;
		case BUILDING_COALMINE:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_MINER,
							      RESOURCE_PICK, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BUILDING_HAS_SERF(building)) {
				int total_food = building->stock[0].requested + building->stock[0].available;
				if (total_food < 8) {
					player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
					building->stock[0].prio = sett->food_coalmine >> (8 + total_food);
				} else {
					building->stock[0].prio = 0;
				}
			}
			break;
		case BUILDING_IRONMINE:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_MINER,
							      RESOURCE_PICK, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BUILDING_HAS_SERF(building)) {
				int total_food = building->stock[0].requested + building->stock[0].available;
				if (total_food < 8) {
					player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
					building->stock[0].prio = sett->food_ironmine >> (8 + total_food);
				} else {
					building->stock[0].prio = 0;
				}
			}
			break;
		case BUILDING_GOLDMINE:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_MINER,
							      RESOURCE_PICK, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BUILDING_HAS_SERF(building)) {
				int total_food = building->stock[0].requested + building->stock[0].available;
				if (total_food < 8) {
					player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
					building->stock[0].prio = sett->food_goldmine >> (8 + total_food);
				} else {
					building->stock[0].prio = 0;
				}
			}
			break;
		case BUILDING_FORESTER:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_FORESTER, -1, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			break;
		case BUILDING_STOCK:
			if (!BUILDING_IS_ACTIVE(building)) {
				inventory_t *inv;
				int r = game_alloc_inventory(&inv, NULL);
				if (r < 0) return;

				inv->player_num = BUILDING_PLAYER(building);
				inv->bld_index = BUILDING_INDEX(building);
				inv->flg_index = building->flg_index;

				building->u.inventory = inv;
				building->stock[0].requested = 0xff;
				building->stock[0].available = 0xff;
				building->stock[1].requested = 0xff;
				building->stock[1].available = 0xff;
				building->serf |= BIT(4);

				player_add_notification(globals.player_sett[BUILDING_PLAYER(building)],
							7, building->pos);
			} else {
				if (!BUILDING_SERF_REQUEST_FAIL(building) &&
				    !BUILDING_HAS_SERF(building) &&
				    !BUILDING_SERF_REQUESTED(building)) {
					send_serf_to_building(building, SERF_TRANSPORTER, -1, -1);
				}

				player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
				inventory_t *inv = building->u.inventory;
				if (BUILDING_HAS_SERF(building) &&
				    (inv->res_dir & 0xa) == 0 && /* Not serf or res OUT mode */
				    inv->spawn_priority == 0) {
					/* sett->field_160 -= 1; */
					if (0/*sett->field_160 < 0*/) {
						/* sett->field_160 = 5; */
						send_serf_to_building(building, SERF_GENERIC, -1, -1);
					}
				}

				sett->inventory_gold += inv->resources[RESOURCE_GOLDBAR];

				/* TODO Following code looks like a hack */
				map_pos_t flag_pos = MAP_MOVE_DOWN_RIGHT(building->pos);
				if (MAP_SERF_INDEX(flag_pos) != 0) {
					serf_t *serf = game_get_serf(MAP_SERF_INDEX(flag_pos));
					if (serf->pos != flag_pos) map_set_serf_index(flag_pos, 0);
				}
			}
			break;
		case BUILDING_HUT:
		{
			const int hut_occupants_from_level[] = {
				1, 1, 2, 2, 3,
				1, 1, 1, 1, 2
			};

			player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
			int max_occ_level = (sett->knight_occupation[BUILDING_STATE(building)] >> 4) & 0xf;
			if (BIT_TEST(sett->flags, 4)) max_occ_level += 5;

			int needed_occupants = hut_occupants_from_level[max_occ_level];
			int max_gold = 2;

			int total_knights = building->stock[0].requested + building->stock[0].available;
			int present_knights = building->stock[0].available;
			if (total_knights < needed_occupants) {
				if (!BUILDING_SERF_REQUEST_FAIL(building)) {
					int r = send_serf_to_building(building, -1, -1, -1);
					if (r < 0) building->serf |= BIT(2);
				}
			} else if (needed_occupants < present_knights &&
				   MAP_SERF_INDEX(MAP_MOVE_DOWN_RIGHT(building->pos)) == 0) {
				/* Kick least trained knight out. */
				serf_t *leaving_serf = NULL;
				int serf_index = building->serf_index;
				while (serf_index != 0) {
					serf_t *serf = game_get_serf(serf_index);
					if (leaving_serf == NULL ||
					    SERF_TYPE(serf) < SERF_TYPE(leaving_serf)) {
						leaving_serf = serf;
					}
					serf_index = serf->s.defending.next_knight;
				}

				/* Remove leaving serf from list. */
				if (SERF_INDEX(leaving_serf) == building->serf_index) {
					building->serf_index = leaving_serf->s.defending.next_knight;
				} else {
					serf_index = building->serf_index;
					while (serf_index != 0) {
						serf_t *serf = game_get_serf(serf_index);
						if (serf->s.defending.next_knight == SERF_INDEX(leaving_serf)) {
							serf->s.defending.next_knight = leaving_serf->s.defending.next_knight;
							break;
						}
						serf_index = serf->s.defending.next_knight;
					}
				}

				/* Update serf state. */
				serf_log_state_change(leaving_serf, SERF_STATE_READY_TO_LEAVE);
				leaving_serf->state = SERF_STATE_READY_TO_LEAVE;
				leaving_serf->s.leaving_building.field_B = -2;
				leaving_serf->s.leaving_building.dest = 0;
				leaving_serf->s.leaving_building.dir = 0;
				leaving_serf->s.leaving_building.next_state = SERF_STATE_WALKING;

				building->stock[0].available -= 1;
			}

			/* Request gold */
			if (BUILDING_HAS_SERF(building)) {
				int total_gold = building->stock[1].requested + building->stock[1].available;
				sett->military_gold += building->stock[1].available;
				sett->military_max_gold += max_gold;

				if (total_gold < max_gold) {
					building->stock[1].prio = ((0xfe >> total_gold) + 1) & 0xfe;
				} else {
					building->stock[1].prio = 0;
				}
			}
			break;
		}
		case BUILDING_FARM:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_FARMER, RESOURCE_SCYTHE, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			break;
		case BUILDING_BUTCHER:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_BUTCHER,
							      RESOURCE_CLEAVER, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BUILDING_HAS_SERF(building)) {
				/* Request more of that delicious meat. */
				int total_stock = building->stock[0].requested + building->stock[0].available;
				if (total_stock < 8) {
					building->stock[0].prio = (0xff >> total_stock);
				} else {
					building->stock[0].prio = 0;
				}
			}
			break;
		case BUILDING_PIGFARM:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_PIGFARMER, -1, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BUILDING_HAS_SERF(building)) {
				/* Request more wheat. */
				int total_stock = building->stock[0].requested + building->stock[0].available;
				if (total_stock < 8) {
					player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
					building->stock[0].prio = sett->wheat_pigfarm >> (8 + total_stock);
				} else {
					building->stock[0].prio = 0;
				}
			}
			break;
		case BUILDING_MILL:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_MILLER, -1, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BUILDING_HAS_SERF(building)) {
				/* Request more wheat. */
				int total_stock = building->stock[0].requested + building->stock[0].available;
				if (total_stock < 8) {
					player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
					building->stock[0].prio = sett->wheat_mill >> (8 + total_stock);
				} else {
					building->stock[0].prio = 0;
				}
			}
			break;
		case BUILDING_BAKER:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_BAKER, -1, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BUILDING_HAS_SERF(building)) {
				/* Request more flour. */
				int total_stock = building->stock[0].requested + building->stock[0].available;
				if (total_stock < 8) {
					building->stock[0].prio = 0xff >> total_stock;
				} else {
					building->stock[0].prio = 0;
				}
			}
			break;
		case BUILDING_SAWMILL:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_SAWMILLER,
							      RESOURCE_SAW, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BUILDING_HAS_SERF(building)) {
				/* Request more lumber */
				int total_stock = building->stock[1].requested + building->stock[1].available;
				if (total_stock < 8) {
					building->stock[1].prio = 0xff >> total_stock;
				} else {
					building->stock[1].prio = 0;
				}
			}
			break;
		case BUILDING_STEELSMELTER:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_SMELTER,
							      -1, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BUILDING_HAS_SERF(building)) {
				/* Request more coal */
				int total_coal = building->stock[0].requested + building->stock[0].available;
				if (total_coal < 8) {
					player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
					building->stock[0].prio = sett->coal_steelsmelter >> (8 + total_coal);
				} else {
					building->stock[0].prio = 0;
				}

				/* Request more iron ore */
				int total_ironore = building->stock[1].requested + building->stock[1].available;
				if (total_ironore < 8) {
					building->stock[1].prio = 0xff >> total_ironore;
				} else {
					building->stock[1].prio = 0;
				}
			}
			break;
		case BUILDING_TOOLMAKER:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_TOOLMAKER,
							      RESOURCE_HAMMER, RESOURCE_SAW);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BUILDING_HAS_SERF(building)) {
				/* Request more planks. */
				player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
				int total_tree = building->stock[0].requested + building->stock[0].available;
				if (total_tree < 8 && 1/*!BIT_TEST(sett->field_163, 1)*/) {
					building->stock[0].prio = sett->planks_toolmaker >> (8 + total_tree);
				} else {
					building->stock[0].prio = 0;
				}

				/* Request more steel. */
				int total_steel = building->stock[1].requested + building->stock[1].available;
				if (total_steel < 8) {
					building->stock[1].prio = sett->steel_toolmaker >> (8 + total_steel);
				} else {
					building->stock[1].prio = 0;
				}
			}
			break;
		case BUILDING_WEAPONSMITH:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_WEAPONSMITH,
							      RESOURCE_HAMMER, RESOURCE_PINCER);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BUILDING_HAS_SERF(building)) {
				/* Request more coal. */
				player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
				int total_coal = building->stock[0].requested + building->stock[0].available;
				if (total_coal < 8) {
					building->stock[0].prio = sett->coal_weaponsmith >> (8 + total_coal);
				} else {
					building->stock[0].prio = 0;
				}

				/* Request more steel. */
				int total_steel = building->stock[1].requested + building->stock[1].available;
				if (total_steel < 8) {
					building->stock[1].prio = sett->steel_weaponsmith >> (8 + total_steel);
				} else {
					building->stock[1].prio = 0;
				}
			}
			break;
		case BUILDING_TOWER:
		{
			const int tower_occupants_from_level[] = {
				1, 2, 3, 4, 6,
				1, 1, 2, 3, 4
			};

			player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
			int max_occ_level = (sett->knight_occupation[BUILDING_STATE(building)] >> 4) & 0xf;
			if (BIT_TEST(sett->flags, 4)) max_occ_level += 5;

			int needed_occupants = tower_occupants_from_level[max_occ_level];
			int max_gold = 4;

			int total_knights = building->stock[0].requested + building->stock[0].available;
			int present_knights = building->stock[0].available;
			if (total_knights < needed_occupants) {
				if (!BUILDING_SERF_REQUEST_FAIL(building)) {
					int r = send_serf_to_building(building, -1, -1, -1);
					if (r < 0) building->serf |= BIT(2);
				}
			} else if (needed_occupants < present_knights &&
				   MAP_SERF_INDEX(MAP_MOVE_DOWN_RIGHT(building->pos)) == 0) {
				/* Kick least trained knight out. */
				serf_t *leaving_serf = NULL;
				int serf_index = building->serf_index;
				while (serf_index != 0) {
					serf_t *serf = game_get_serf(serf_index);
					if (leaving_serf == NULL ||
					    SERF_TYPE(serf) < SERF_TYPE(leaving_serf)) {
						leaving_serf = serf;
					}
					serf_index = serf->s.defending.next_knight;
				}

				/* Remove leaving serf from list. */
				if (SERF_INDEX(leaving_serf) == building->serf_index) {
					building->serf_index = leaving_serf->s.defending.next_knight;
				} else {
					serf_index = building->serf_index;
					while (serf_index != 0) {
						serf_t *serf = game_get_serf(serf_index);
						if (serf->s.defending.next_knight == SERF_INDEX(leaving_serf)) {
							serf->s.defending.next_knight = leaving_serf->s.defending.next_knight;
							break;
						}
						serf_index = serf->s.defending.next_knight;
					}
				}

				/* Update serf state. */
				serf_log_state_change(leaving_serf, SERF_STATE_READY_TO_LEAVE);
				leaving_serf->state = SERF_STATE_READY_TO_LEAVE;
				leaving_serf->s.leaving_building.field_B = -2;
				leaving_serf->s.leaving_building.dest = 0;
				leaving_serf->s.leaving_building.dir = 0;
				leaving_serf->s.leaving_building.next_state = SERF_STATE_WALKING;

				building->stock[0].available -= 1;
			}

			/* Request gold */
			if (BUILDING_HAS_SERF(building)) {
				int total_gold = building->stock[1].requested + building->stock[1].available;
				sett->military_gold += building->stock[1].available;
				sett->military_max_gold += max_gold;

				if (total_gold < max_gold) {
					building->stock[1].prio = ((0xfe >> total_gold) + 1) & 0xfe;
				} else {
					building->stock[1].prio = 0;
				}
			}
			break;
		}
		case BUILDING_FORTRESS:
		{
			const int fortress_occupants_from_level[] = {
				1, 3, 6, 9, 12,
				1, 2, 4, 6, 8
			};

			player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
			int max_occ_level = (sett->knight_occupation[BUILDING_STATE(building)] >> 4) & 0xf;
			if (BIT_TEST(sett->flags, 4)) max_occ_level += 5;

			int needed_occupants = fortress_occupants_from_level[max_occ_level];
			int max_gold = 8;

			int total_knights = building->stock[0].requested + building->stock[0].available;
			int present_knights = building->stock[0].available;
			if (total_knights < needed_occupants) {
				/* Send a knight to this building. */
				if (!BUILDING_SERF_REQUEST_FAIL(building)) {
					int r = send_serf_to_building(building, -1, -1, -1);
					if (r < 0) building->serf |= BIT(2);
				}
			} else if (needed_occupants < present_knights &&
				   MAP_SERF_INDEX(MAP_MOVE_DOWN_RIGHT(building->pos)) == 0) {
				/* Kick least trained knight out. */
				serf_t *leaving_serf = NULL;
				int serf_index = building->serf_index;
				while (serf_index != 0) {
					serf_t *serf = game_get_serf(serf_index);
					if (leaving_serf == NULL ||
					    SERF_TYPE(serf) < SERF_TYPE(leaving_serf)) {
						leaving_serf = serf;
					}
					serf_index = serf->s.defending.next_knight;
				}

				/* Remove leaving serf from list. */
				if (SERF_INDEX(leaving_serf) == building->serf_index) {
					building->serf_index = leaving_serf->s.defending.next_knight;
				} else {
					serf_index = building->serf_index;
					while (serf_index != 0) {
						serf_t *serf = game_get_serf(serf_index);
						if (serf->s.defending.next_knight == SERF_INDEX(leaving_serf)) {
							serf->s.defending.next_knight = leaving_serf->s.defending.next_knight;
							break;
						}
						serf_index = serf->s.defending.next_knight;
					}
				}

				/* Update serf state. */
				serf_log_state_change(leaving_serf, SERF_STATE_READY_TO_LEAVE);
				leaving_serf->state = SERF_STATE_READY_TO_LEAVE;
				leaving_serf->s.leaving_building.field_B = -2;
				leaving_serf->s.leaving_building.dest = 0;
				leaving_serf->s.leaving_building.dir = 0;
				leaving_serf->s.leaving_building.next_state = SERF_STATE_WALKING;

				building->stock[0].available -= 1;
			}

			/* Request gold */
			if (BUILDING_HAS_SERF(building)) {
				int total_gold = building->stock[1].requested + building->stock[1].available;
				sett->military_gold += building->stock[1].available;
				sett->military_max_gold += max_gold;

				if (total_gold < max_gold) {
					building->stock[1].prio = ((0xfe >> total_gold) + 1) & 0xfe;
				} else {
					building->stock[1].prio = 0;
				}
			}
			break;
		}
		case BUILDING_GOLDSMELTER:
			if (!BUILDING_SERF_REQUEST_FAIL(building) &&
			    !BUILDING_HAS_SERF(building) &&
			    !BUILDING_SERF_REQUESTED(building)) {
				int r = send_serf_to_building(building, SERF_SMELTER,
							      -1, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BUILDING_HAS_SERF(building)) {
				/* Request more coal. */
				player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
				int total_coal = building->stock[0].requested + building->stock[0].available;
				if (total_coal < 8) {
					building->stock[0].prio = sett->coal_goldsmelter >> (8 + total_coal);
				} else {
					building->stock[0].prio = 0;
				}

				/* Request more gold ore. */
				int total_goldore = building->stock[1].requested + building->stock[1].available;
				if (total_goldore < 8) {
					building->stock[1].prio = 0xff >> total_goldore;
				} else {
					building->stock[1].prio = 0;
				}
			}
			break;
		case BUILDING_CASTLE:
			update_building_castle(building);
			break;
		default:
			NOT_REACHED();
			break;
		}
	} else { /* Unfinished */
		switch (type) {
			case BUILDING_NONE:
			case BUILDING_CASTLE:
				break;
			case BUILDING_FISHER:
			case BUILDING_LUMBERJACK:
			case BUILDING_BOATBUILDER:
			case BUILDING_STONECUTTER:
			case BUILDING_STONEMINE:
			case BUILDING_COALMINE:
			case BUILDING_IRONMINE:
			case BUILDING_GOLDMINE:
			case BUILDING_FORESTER:
			case BUILDING_HUT:
			case BUILDING_MILL:
				update_unfinished_building(building);
				break;
			case BUILDING_STOCK:
			case BUILDING_FARM:
			case BUILDING_BUTCHER:
			case BUILDING_PIGFARM:
			case BUILDING_BAKER:
			case BUILDING_SAWMILL:
			case BUILDING_STEELSMELTER:
			case BUILDING_TOOLMAKER:
			case BUILDING_WEAPONSMITH:
			case BUILDING_TOWER:
			case BUILDING_FORTRESS:
			case BUILDING_GOLDSMELTER:
				update_unfinished_adv_building(building);
				break;
			default:
				NOT_REACHED();
				break;
		}
	}
}

/* Update buildings as part of the game progression. */
static void
update_buildings()
{
	map_tile_t *tiles = globals.map.tiles;

	if (globals.next_index >= 32) return;

	int index = globals.next_index << 5;
	for (int i = index ? index : 1; i < globals.max_ever_building_index; i++) {
		if (BUILDING_ALLOCATED(i)) {
			building_t *building = game_get_building(i);
			if (BUILDING_IS_BURNING(building)) {
				uint16_t delta = globals.anim - building->u.anim;
				building->u.anim = globals.anim;
				if (building->serf_index >= delta) {
					building->serf_index -= delta;
				} else {
					tiles[building->pos].flags &= ~BIT(6);
					map_set_object(building->pos, MAP_OBJ_NONE, 0);
					game_free_building(i);
				}
			} else {
				handle_building_update(building);
			}
		}
	}
}

/* Update serfs as part of the game progression. */
static void
update_serfs()
{
	if (globals.next_index >= 32) return;

	/*int index = globals.next_index & 0xf;
	  for (int i = index ? index : 1; i < globals.max_ever_serf_index; i++) {*/
	for (int i = 1; i < globals.max_ever_serf_index; i++) {
		if (SERF_ALLOCATED(i)) {
			serf_t *serf = game_get_serf(i);
			update_serf(serf);
		}
	}
}

/* Update historical player statistics for one measure. */
static void
record_player_history(player_sett_t *sett[], int pl_count, int max_level, int aspect,
		      const int history_index[], const int values[])
{
	int total = 0;
	for (int i = 0; i < pl_count; i++) total += values[i];
	total = max(1, total);

	for (int i = 0; i < max_level+1; i++) {
		int mode = (aspect << 2) | i;
		int index = history_index[i];
		for (int j = 0; j < pl_count; j++) {
			sett[j]->player_stat_history[mode][index] = (100*values[j])/total;
		}
	}
}

/* Calculate whether one player has enough advantage to be
   considered a clear winner regarding one aspect.
   Return -1 if there is no clear winner. */
static int
calculate_clear_winner(int pl_count, const int values[])
{
	int total = 0;
	for (int i = 0; i < pl_count; i++) total += values[i];
	total = max(1, total);

	for (int i = 0; i < pl_count; i++) {
		if ((100*values[i])/total >= 75) return i;
	}

	return -1;
}

/* Calculate condensed score from military score and knight morale. */
static int
calculate_military_score(int military, int morale)
{
	return (2048 + (morale >> 1)) * (military >> 6);
}

/* Update statistics of the game. */
static void
update_game_stats()
{
	if (globals.anim - globals.game_stats_counter >= 1500) {
		globals.game_stats_counter += 1500;
		globals.player_score_leader = 0;

		int update_level = 0;

		/* Update first level index */
		globals.player_history_index[0] =
			globals.player_history_index[0]+1 < 112 ?
			globals.player_history_index[0]+1 : 0;

		globals.player_history_counter[0] -= 1;
		if (globals.player_history_counter[0] < 0) {
			update_level = 1;
			globals.player_history_counter[0] = 3;

			/* Update second level index */
			globals.player_history_index[1] =
				globals.player_history_index[1]+1 < 112 ?
				globals.player_history_index[1]+1 : 0;

			globals.player_history_counter[1] -= 1;
			if (globals.player_history_counter[1] < 0) {
				update_level = 2;
				globals.player_history_counter[1] = 4;

				/* Update third level index */
				globals.player_history_index[2] =
					globals.player_history_index[2]+1 < 112 ?
					globals.player_history_index[2]+1 : 0;

				globals.player_history_counter[2] -= 1;
				if (globals.player_history_counter[2] < 0) {
					update_level = 3;

					globals.player_history_counter[2] = 4;

					/* Update fourth level index */
					globals.player_history_index[3] =
						globals.player_history_index[3]+1 < 112 ?
						globals.player_history_index[3]+1 : 0;
				}
			}
		}

		int values[4];

		/* Store land area stats in history. */
		for (int i = 0; i < 4; i++) values[i] = globals.player_sett[i]->total_land_area;
		record_player_history(globals.player_sett, 4, update_level, 1,
				      globals.player_history_index, values);
		globals.player_score_leader |= BIT(calculate_clear_winner(4, values));

		/* Store building stats in history. */
		for (int i = 0; i < 4; i++) values[i] = globals.player_sett[i]->total_building_score;
		record_player_history(globals.player_sett, 4, update_level, 2,
				      globals.player_history_index, values);

		/* Store military stats in history. */
		for (int i = 0; i < 4; i++) {
			values[i] = calculate_military_score(globals.player_sett[i]->total_military_score,
							     globals.player_sett[i]->knight_morale);
		}
		record_player_history(globals.player_sett, 4, update_level, 3,
				      globals.player_history_index, values);
		globals.player_score_leader |= BIT(calculate_clear_winner(4, values)) << 4;

		/* Store condensed score of all aspects in history. */
		for (int i = 0; i < 4; i++) {
			int mil_score = calculate_military_score(globals.player_sett[i]->total_military_score,
								 globals.player_sett[i]->knight_morale);
			values[i] = globals.player_sett[i]->total_building_score +
				((globals.player_sett[i]->total_land_area + mil_score) >> 4);
		}
		record_player_history(globals.player_sett, 4, update_level, 0,
				      globals.player_history_index, values);

		/* TODO Determine winner based on globals.player_score_leader */
	}

	if (globals.anim - globals.history_counter >= 6000) {
		globals.history_counter += 6000;

		int index = globals.resource_history_index;

		for (int res = 0; res < 26; res++) {
			for (int i = 0; i < 4; i++) {
				player_sett_t *sett = globals.player_sett[i];
				sett->resource_count_history[res][index] = sett->resource_count[res];
				sett->resource_count[res] = 0;
			}
		}

		globals.resource_history_index = index+1 < 120 ? index+1 : 0;
	}
}

/* Update game state after tick increment. */
void
game_update()
{
	update_map_and_players();
	update_ai_and_more();
	update_flags();
	update_buildings();
	update_serfs();
	/*update_visible_serfs(); OBSOLETE */
	update_game_stats();
}

/* Pause or unpause the game. */
void
game_pause(int enable)
{
	if (enable) {
		globals.game_speed_save = globals.game_speed;
		globals.game_speed = 0;
	} else {
		globals.game_speed = globals.game_speed_save;
	}

	LOGI("game", "Game speed: %u", globals.game_speed >> 16);
}

/* Generate an estimate of the amount of resources in the ground at map pos.*/
static void
get_resource_estimate(map_pos_t pos, int weight, player_t *player)
{
	if ((MAP_OBJ(pos) == MAP_OBJ_NONE ||
	     MAP_OBJ(pos) >= MAP_OBJ_TREE_0) &&
	     MAP_RES_TYPE(pos) != GROUND_DEPOSIT_NONE) {
		int value = weight*MAP_RES_AMOUNT(pos);

		switch (MAP_RES_TYPE(pos)) {
		case GROUND_DEPOSIT_GOLD:
			player->sett->analysis_goldore += value;
			break;
		case GROUND_DEPOSIT_IRON:
			player->sett->analysis_ironore += value;
			break;
		case GROUND_DEPOSIT_COAL:
			player->sett->analysis_coal += value;
			break;
		case GROUND_DEPOSIT_STONE:
			player->sett->analysis_stone += value;
			break;
		default:
			NOT_REACHED();
			break;
		}
	}
}

/* Prepare a ground analysis for player.
   The cursor position is the center of the analysis. */
void
game_prepare_ground_analysis(player_t *player)
{
	player->sett->analysis_goldore = 0;
	player->sett->analysis_ironore = 0;
	player->sett->analysis_coal = 0;
	player->sett->analysis_stone = 0;

	/* Use cursor position, not viewport position as
	   was used in the original game. */
	map_pos_t pos = MAP_POS(player->sett->map_cursor_col,
				player->sett->map_cursor_row);

	/* Sample the cursor position with maximum weighting. */
	get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS, player);

	/* Move outward in a spiral around the initial pos.
	   The weighting of the samples attenuates linearly
	   with the distance to the center. */
	for (int i = 0; i < GROUND_ANALYSIS_RADIUS-1; i++) {
		pos = MAP_MOVE_RIGHT(pos);

		for (int j = 0; j < i+1; j++) {
			get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, player);
			pos = MAP_MOVE_DOWN(pos);
		}

		for (int j = 0; j < i+1; j++) {
			get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, player);
			pos = MAP_MOVE_LEFT(pos);
		}

		for (int j = 0; j < i+1; j++) {
			get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, player);
			pos = MAP_MOVE_UP_LEFT(pos);
		}

		for (int j = 0; j < i+1; j++) {
			get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, player);
			pos = MAP_MOVE_UP(pos);
		}

		for (int j = 0; j < i+1; j++) {
			get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, player);
			pos = MAP_MOVE_RIGHT(pos);
		}

		for (int j = 0; j < i+1; j++) {
			get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, player);
			pos = MAP_MOVE_DOWN_RIGHT(pos);
		}
	}

	/* Process the samples. */
	player->sett->analysis_goldore >>= 4;
	player->sett->analysis_goldore = min(player->sett->analysis_goldore, 999);

	player->sett->analysis_ironore >>= 4;
	player->sett->analysis_ironore = min(player->sett->analysis_ironore, 999);

	player->sett->analysis_coal >>= 4;
	player->sett->analysis_coal = min(player->sett->analysis_coal, 999);

	player->sett->analysis_stone >>= 4;
	player->sett->analysis_stone = min(player->sett->analysis_stone, 999);
}

/* Return non-zero if the road segment from pos in direction dir
   can be successfully constructed at the current time. */
int
game_road_segment_valid(map_pos_t pos, dir_t dir)
{
	map_pos_t other_pos = MAP_MOVE(pos, dir);

	map_obj_t obj = MAP_OBJ(other_pos);
	if ((MAP_PATHS(other_pos) != 0 && obj != MAP_OBJ_FLAG) ||
	    (obj >= MAP_OBJ_SMALL_BUILDING &&
	     obj <= MAP_OBJ_CASTLE) ||
	    map_space_from_obj[obj] == MAP_SPACE_IMPASSABLE) {
		return 0;
	}

	if (!MAP_HAS_OWNER(other_pos) ||
	    MAP_OWNER(other_pos) != MAP_OWNER(pos)) {
		return 0;
	}

	if (map_is_deep_water(pos) != map_is_deep_water(other_pos) &&
	    !(MAP_HAS_FLAG(pos) || MAP_HAS_FLAG(other_pos))) {
		return 0;
	}

	return 1;
}

/* Get road length category value for real length.
   Determines number of serfs servicing the path segment.(?) */
int
game_get_road_length_value(int length)
{
	if (length >= 24) return 7 << 4;
	else if (length >= 18) return 6 << 4;
	else if (length >= 13) return 5 << 4;
	else if (length >= 10) return 4 << 4;
	else if (length >= 7) return 3 << 4;
	else if (length >= 6) return 2 << 4;
	else if (length >= 4) return 1 << 4;
	return 0;
}

static void
flag_reset_transport(flag_t *flag)
{
	/* Clear destination for any serf with resources for this flag. */
	for (int i = 1; i < globals.max_ever_serf_index; i++) {
		if (SERF_ALLOCATED(i)) {
			serf_t *serf = game_get_serf(i);

			if (serf->state == SERF_STATE_WALKING &&
			    serf->s.walking.dest == FLAG_INDEX(flag) &&
			    serf->s.walking.res < 0) {
				serf->s.walking.res = -2;
				serf->s.walking.dest = 0;
			} else if (serf->state == SERF_STATE_READY_TO_LEAVE_INVENTORY &&
				   serf->s.ready_to_leave_inventory.dest == FLAG_INDEX(flag) &&
				   serf->s.ready_to_leave_inventory.mode < 0) {
				serf->s.ready_to_leave_inventory.mode = -2;
				serf->s.ready_to_leave_inventory.dest = 0;
			} else if ((serf->state == SERF_STATE_LEAVING_BUILDING ||
				    serf->state == SERF_STATE_READY_TO_LEAVE) &&
				   serf->s.leaving_building.next_state == SERF_STATE_WALKING &&
				   serf->s.leaving_building.dest == FLAG_INDEX(flag) &&
				   serf->s.leaving_building.field_B < 0) {
				serf->s.leaving_building.field_B = -2;
				serf->s.leaving_building.dest = 0;
			} else if (serf->state == SERF_STATE_TRANSPORTING &&
				   serf->s.walking.dest == FLAG_INDEX(flag)) {
				serf->s.walking.dest = 0;
			} else if (serf->state == SERF_STATE_MOVE_RESOURCE_OUT &&
				   serf->s.move_resource_out.next_state == SERF_STATE_DROP_RESOURCE_OUT &&
				   serf->s.move_resource_out.res_dest == FLAG_INDEX(flag)) {
				serf->s.move_resource_out.res_dest = 0;
			} else if (serf->state == SERF_STATE_DROP_RESOURCE_OUT &&
				   serf->s.move_resource_out.res_dest == FLAG_INDEX(flag)) {
				serf->s.move_resource_out.res_dest = 0;
			} else if (serf->state == SERF_STATE_LEAVING_BUILDING &&
				   serf->s.leaving_building.next_state == SERF_STATE_DROP_RESOURCE_OUT &&
				   serf->s.leaving_building.dest == FLAG_INDEX(flag)) {
				serf->s.leaving_building.dest = 0;
			}
		}
	}

	/* Flag. */
	for (int i = 1; i < globals.max_ever_flag_index; i++) {
		if (FLAG_ALLOCATED(i)) {
			flag_t *other = game_get_flag(i);

			for (int slot = 0; slot < 8; slot++) {
				if (other->res_waiting[slot] != 0 &&
				    other->res_dest[slot] == FLAG_INDEX(flag)) {
					other->res_dest[slot] = 0;
					other->endpoint |= BIT(7);

					if (((other->res_waiting[slot] >> 5) & 3) != 0) {
						dir_t dir = ((other->res_waiting[slot] >> 5) & 3)-1;
						player_sett_t *sett = globals.player_sett[FLAG_PLAYER(other)];
						flag_prioritize_pickup(other, dir, sett->flag_prio);
					}
				}
			}
		}
	}

	/* Inventories. */
	for (int i = 0; i < globals.max_ever_inventory_index; i++) {
		if (INVENTORY_ALLOCATED(i)) {
			inventory_t *inventory = game_get_inventory(i);
			if (inventory->out_dest[1] == FLAG_INDEX(flag)) {
				inventory->out_queue[1] = -1;
			}
			if (inventory->out_dest[0] == FLAG_INDEX(flag)) {
				inventory->out_queue[0] = inventory->out_queue[1];
				inventory->out_dest[0] = inventory->out_dest[1];
				inventory->out_queue[1] = -1;
			}
		}
	}
}

static void
building_remove_pl_sett_refs(building_t *building)
{
	for (int i = 0; i < 4; i++) {
		if (globals.player_sett[i]->index == BUILDING_INDEX(building)) {
			globals.player_sett[i]->index = 0;
		}
	}

	player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];

	if (sett->sawmill_index == BUILDING_INDEX(building)) {
		sett->sawmill_index = 0;
	}

	if (sett->stonecutter_index == BUILDING_INDEX(building)) {
		sett->stonecutter_index = 0;
	}

	if (sett->lumberjack_index == BUILDING_INDEX(building)) {
		sett->lumberjack_index = 0;
	}
}

static int
remove_road_backref_until_flag(map_pos_t pos, dir_t dir)
{
	map_tile_t *tiles = globals.map.tiles;

	while (1) {
		pos = MAP_MOVE(pos, dir);

		/* Clear backreference */
		tiles[pos].flags &= ~BIT(DIR_REVERSE(dir));

		if (MAP_OBJ(pos) == MAP_OBJ_FLAG) break;

		/* Find next direction of path. */
		dir = -1;
		for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
			if (BIT_TEST(MAP_PATHS(pos), d)) {
				dir = d;
				break;
			}
		}

		if (dir == -1) return -1;
	}

	return 0;
}

static int
remove_road_backrefs(map_pos_t pos)
{
	if (MAP_PATHS(pos) == 0) return -1;

	/* Find directions of path segments to be split. */
	dir_t path_1_dir = -1;
	for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
		if (BIT_TEST(MAP_PATHS(pos), d)) {
			path_1_dir = d;
			break;
		}
	}

	dir_t path_2_dir = -1;
	for (dir_t d = path_1_dir+1; d <= DIR_UP; d++) {
		if (BIT_TEST(MAP_PATHS(pos), d)) {
			path_2_dir = d;
			break;
		}
	}

	if (path_1_dir == -1 || path_2_dir == -1) return -1;

	int r = remove_road_backref_until_flag(pos, path_1_dir);
	if (r < 0) return -1;

	r = remove_road_backref_until_flag(pos, path_2_dir);
	if (r < 0) return -1;

	return 0;
}

static int
path_serf_idle_to_wait_state(map_pos_t pos)
{
	/* Look through serf array for the corresponding serf. */
	for (int i = 1; i < globals.max_ever_serf_index; i++) {
		if (SERF_ALLOCATED(i)) {
			serf_t *serf = game_get_serf(i);
			if (serf->pos == pos &&
			    (serf->state == SERF_STATE_IDLE_ON_PATH ||
			     serf->state == SERF_STATE_WAIT_IDLE_ON_PATH ||
			     serf->state == SERF_STATE_WAKE_AT_FLAG ||
			     serf->state == SERF_STATE_WAKE_ON_PATH)) {
				serf_log_state_change(serf, SERF_STATE_WAKE_AT_FLAG);
				serf->state = SERF_STATE_WAKE_AT_FLAG;
				return 0;
			}
		}
	}

	return -1;
}

static void
lose_transported_resource(resource_type_t res, uint dest)
{
	static const int stock_type[] = {
		0, 0, 0, 0, 0, 0,
		1, 0, -1, 1, 1, 1,
		0, 1, 1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1,
		-1, -1, -1
	};

	if (res == RESOURCE_GOLDORE ||
	    res == RESOURCE_GOLDBAR) {
		globals.map_gold_deposit -= 1;
	}

	if (stock_type[res] >= 0 && dest != 0) {
		flag_t *flag = game_get_flag(dest);
		building_t *building = flag->other_endpoint.b[DIR_UP_LEFT];
		if (!(BUILDING_IS_DONE(building) &&
		      (BUILDING_TYPE(building) == BUILDING_CASTLE ||
		       BUILDING_TYPE(building) == BUILDING_STOCK))) {
			assert(stock_type[res] >= 0);
			building->stock[stock_type[res]].requested -= 1;
		}
	}
}


/* ADDITION: Removed precondition that serf is in state walking or transporting. */
static void
mark_serf_as_lost(serf_t *serf)
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

			lose_transported_resource(res, dest);
		}

		if (SERF_TYPE(serf) != SERF_SAILOR) {
			serf_log_state_change(serf, SERF_STATE_LOST);
			serf->state = SERF_STATE_LOST;
			serf->s.lost.field_B = 0;
		} else {
			serf_log_state_change(serf, SERF_STATE_LOST_SAILOR);
			serf->state = SERF_STATE_LOST_SAILOR;
		}
	}
}

static void
remove_road_forwards(map_pos_t pos, dir_t dir)
{
	map_tile_t *tiles = globals.map.tiles;
	dir_t in_dir = -1;

	while (1) {
		if (MAP_IDLE_SERF(pos)) {
			path_serf_idle_to_wait_state(pos);
		}

		if (MAP_SERF_INDEX(pos) != 0) {
			serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));
			if (!MAP_HAS_FLAG(pos)) {
				mark_serf_as_lost(serf);
			} else {
				/* Handle serf close to flag, where
				   it should only be lost if walking
				   in the wrong direction. */
				int d = serf->s.walking.dir;
				if (d < 0) d += 6;
				if (d == DIR_REVERSE(dir)) {
					mark_serf_as_lost(serf);
				}
			}
		}

		if (MAP_HAS_FLAG(pos)) {
			flag_t *flag = game_get_flag(MAP_OBJ_INDEX(pos));
			dir_t rev_dir = DIR_REVERSE(in_dir);

			flag->path_con &= ~BIT(rev_dir);
			flag->transporter &= ~BIT(rev_dir);
			flag->endpoint &= ~BIT(rev_dir);

			if (FLAG_SERF_REQUESTED(flag, rev_dir)) {
				flag->length[rev_dir] &= ~BIT(7);

				for (int i = 1; i < globals.max_ever_serf_index; i++) {
					if (SERF_ALLOCATED(i)) {
						int dest = MAP_OBJ_INDEX(pos);
						serf_t *serf = game_get_serf(i);

						switch (serf->state) {
						case SERF_STATE_WALKING:
							if (serf->s.walking.dest == dest &&
							    serf->s.walking.res == rev_dir) {
								serf->s.walking.res = -2;
								serf->s.walking.dest = 0;
							}
							break;
						case SERF_STATE_READY_TO_LEAVE_INVENTORY:
							if (serf->s.ready_to_leave_inventory.dest == dest &&
							    serf->s.ready_to_leave_inventory.mode == rev_dir) {
								serf->s.ready_to_leave_inventory.mode = -2;
								serf->s.ready_to_leave_inventory.dest = 0;
							}
							break;
						case SERF_STATE_LEAVING_BUILDING:
						case SERF_STATE_READY_TO_LEAVE:
							if (serf->s.leaving_building.dest == dest &&
							    serf->s.leaving_building.field_B == rev_dir &&
							    serf->s.leaving_building.next_state == SERF_STATE_WALKING) {
								serf->s.leaving_building.field_B = -2;
								serf->s.leaving_building.dest = 0;
							}
							break;
						default:
							break;
						}
					}
				}
			}

			flag->other_end_dir[rev_dir] &= 0x78;

			/* Mark resource path for recalculation if they would
			   have followed the removed path. */
			for (int i = 0; i < 8; i++) {
				if (flag->res_waiting[i] != 0 &&
				    (flag->res_waiting[i] >> 5) == rev_dir+1) {
					flag->res_waiting[i] &= 0x1f;
					flag->endpoint |= BIT(7);
				}
			}
			break;
		}

		/* Clear forward reference. */
		tiles[pos].flags &= ~BIT(dir);
		pos = MAP_MOVE(pos, dir);
		in_dir = dir;

		/* Clear backreference. */
		tiles[pos].flags &= ~BIT(DIR_REVERSE(dir));

		/* Find next direction of path. */
		dir = -1;
		for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
			if (BIT_TEST(MAP_PATHS(pos), d)) {
				dir = d;
				break;
			}
		}
	}
}

/* Demolish road at position. */
void
game_demolish_road(map_pos_t pos)
{
	globals.player[0]->flags |= BIT(4);
	globals.player[1]->flags |= BIT(4);

	int r = remove_road_backrefs(pos);
	if (r < 0) {
		/* TODO */
	}

	/* Find directions of path segments to be split. */
	dir_t path_1_dir = -1;
	for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
		if (BIT_TEST(MAP_PATHS(pos), d)) {
			path_1_dir = d;
			break;
		}
	}

	dir_t path_2_dir = -1;
	for (dir_t d = path_1_dir+1; d <= DIR_UP; d++) {
		if (BIT_TEST(MAP_PATHS(pos), d)) {
			path_2_dir = d;
			break;
		}
	}

	/* If last segment direction is UP LEFT it could
	   be to a building and the real path is at UP. */
	if (path_2_dir == DIR_UP_LEFT &&
	    BIT_TEST(MAP_PATHS(pos), DIR_UP)) {
		path_2_dir = DIR_UP;
	}

	remove_road_forwards(pos, path_1_dir);
	remove_road_forwards(pos, path_2_dir);
}

/* Find a transporter at pos and change it to state. */
static int
change_transporter_state_at_pos(map_pos_t pos, serf_state_t state)
{
	for (int i = 1; i < globals.max_ever_serf_index; i++) {
		if (SERF_ALLOCATED(i)) {
			serf_t *serf = game_get_serf(i);
			if (serf->pos == pos &&
			    (serf->state == SERF_STATE_WAKE_AT_FLAG ||
			     serf->state == SERF_STATE_WAKE_ON_PATH ||
			     serf->state == SERF_STATE_WAIT_IDLE_ON_PATH ||
			     serf->state == SERF_STATE_IDLE_ON_PATH)) {
				serf_log_state_change(serf, state);
				serf->state = state;
				return SERF_INDEX(serf);
			}
		}
	}

	return -1;
}

static int
wake_transporter_at_flag(map_pos_t pos)
{
	return change_transporter_state_at_pos(pos, SERF_STATE_WAKE_AT_FLAG);
}

static int
wake_transporter_on_path(map_pos_t pos)
{
	return change_transporter_state_at_pos(pos, SERF_STATE_WAKE_ON_PATH);
}

typedef struct {
	int path_len;
	int serf_count;
	int flag_index;
	dir_t flag_dir;
	int serfs[16];
} serf_path_info_t;

static void
fill_path_serf_info(map_pos_t pos, dir_t dir, serf_path_info_t *data)
{
	if (MAP_IDLE_SERF(pos)) wake_transporter_at_flag(pos);

	int serf_count = 0;
	int path_len = 0;

	/* Handle first position. */
	if (MAP_SERF_INDEX(pos) != 0) {
		serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));
		if (serf->state == SERF_STATE_TRANSPORTING &&
		    serf->s.walking.wait_counter != -1) {
			int d = serf->s.walking.dir;
			if (d < 0) d += 6;

			if (dir == d) {
				serf->s.walking.wait_counter = 0;
				data->serfs[serf_count++] = SERF_INDEX(serf);
			}
		}
	}

	/* Trace along the path to the flag at the other end. */
	int paths = 0;
	while (1) {
		path_len += 1;
		pos = MAP_MOVE(pos, dir);
		paths = MAP_PATHS(pos);
		paths &= ~BIT(DIR_REVERSE(dir));

		if (MAP_HAS_FLAG(pos)) break;

		/* Find out which direction the path follows. */
		for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
			if (BIT_TEST(paths, d)) {
				dir = d;
				break;
			}
		}

		/* Check if there is a transporter waiting here. */
		if (MAP_IDLE_SERF(pos)) {
			int index = wake_transporter_on_path(pos);
			if (index >= 0) data->serfs[serf_count++] = index;
		}

		/* Check if there is a serf occupying this space. */
		if (MAP_SERF_INDEX(pos) != 0) {
			serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));
			if (serf->state == SERF_STATE_TRANSPORTING &&
			    serf->s.walking.wait_counter != -1) {
				serf->s.walking.wait_counter = 0;
				data->serfs[serf_count++] = SERF_INDEX(serf);
			}
		}
	}

	/* Handle last position. */
	if (MAP_SERF_INDEX(pos) != 0) {
		serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));
		if ((serf->state == SERF_STATE_TRANSPORTING &&
		     serf->s.walking.wait_counter != -1) ||
		    serf->state == SERF_STATE_DELIVERING) {
			int d = serf->s.walking.dir;
			if (d < 0) d += 6;

			if (d == DIR_REVERSE(dir)) {
				serf->s.walking.wait_counter = 0;
				data->serfs[serf_count++] = SERF_INDEX(serf);
			}
		}
	}

	/* Fill the rest of the struct. */
	data->path_len = path_len;
	data->serf_count = serf_count;
	data->flag_index = MAP_OBJ_INDEX(pos);
	data->flag_dir = DIR_REVERSE(dir);
}

static void
restore_path_serf_info(flag_t *flag, dir_t dir, serf_path_info_t *data)
{
	const int max_path_serfs[] = { 1, 2, 3, 4, 6, 8, 11, 15 };

	flag_t *other_flag = game_get_flag(data->flag_index);
	dir_t other_dir = data->flag_dir;

	flag->path_con |= BIT(dir);
	flag->endpoint &= ~BIT(dir);

	if (!FLAG_IS_WATER_PATH(other_flag, other_dir)) {
		flag->endpoint |= BIT(dir);
	}

	other_flag->transporter &= ~BIT(other_dir);
	flag->transporter &= ~BIT(dir);

	int len = game_get_road_length_value(data->path_len);

	flag->length[dir] = len;
	other_flag->length[other_dir] = (0x80 & other_flag->length[other_dir]) | len;

	if (FLAG_SERF_REQUESTED(other_flag, other_dir)) {
		flag->length[dir] |= BIT(7);
	}

	flag->other_end_dir[dir] = (flag->other_end_dir[dir] & 0xc7) | (other_dir << 3);
	other_flag->other_end_dir[other_dir] = (other_flag->other_end_dir[other_dir] & 0xc7) | (dir << 3);

	flag->other_endpoint.f[dir] = other_flag;
	other_flag->other_endpoint.f[other_dir] = flag;

	int max_serfs = max_path_serfs[(len >> 4) & 7];
	if (FLAG_SERF_REQUESTED(flag, dir)) max_serfs -= 1;

	if (data->serf_count > max_serfs) {
		for (int i = 0; i < data->serf_count - max_serfs; i++) {
			serf_t *serf = game_get_serf(data->serfs[i]);
			if (serf->state != SERF_STATE_WAKE_ON_PATH) {
				serf->s.walking.wait_counter = -1;
				if (serf->s.walking.res != 0) {
					resource_type_t res = serf->s.walking.res-1;
					serf->s.walking.res = 0;

					/* Remove gold from total count. */
					if (res == RESOURCE_GOLDBAR ||
					    res == RESOURCE_GOLDORE) {
						globals.map_gold_deposit -= 1;
					}

					flag_cancel_transported_stock(game_get_flag(serf->s.walking.dest), res+1);
				}
			} else {
				serf_log_state_change(serf, SERF_STATE_WAKE_AT_FLAG);
				serf->state = SERF_STATE_WAKE_AT_FLAG;
			}
		}
	}

	if (min(data->serf_count, max_serfs) > 0) {
		/* There is still transporters on the paths. */
		flag->transporter |= BIT(dir);
		other_flag->transporter |= BIT(other_dir);

		flag->length[dir] |= min(data->serf_count, max_serfs);
		other_flag->length[other_dir] |= min(data->serf_count, max_serfs);
	}
}

/* Build flag on existing path. Path must be split in two segments. */
static void
build_flag_split_path(map_pos_t pos)
{
	/* Find directions of path segments to be split. */
	dir_t path_1_dir = -1;
	for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
		if (BIT_TEST(MAP_PATHS(pos), d)) {
			path_1_dir = d;
			break;
		}
	}

	dir_t path_2_dir = -1;
	for (dir_t d = path_1_dir+1; d <= DIR_UP; d++) {
		if (BIT_TEST(MAP_PATHS(pos), d)) {
			path_2_dir = d;
			break;
		}
	}

	/* If last segment direction is UP LEFT it could
	   be to a building and the real path is at UP. */
	if (path_2_dir == DIR_UP_LEFT &&
	    BIT_TEST(MAP_PATHS(pos), DIR_UP)) {
		path_2_dir = DIR_UP;
	}

	serf_path_info_t path_1_data;
	serf_path_info_t path_2_data;

	fill_path_serf_info(pos, path_1_dir, &path_1_data);
	fill_path_serf_info(pos, path_2_dir, &path_2_data);

	flag_t *flag_2 = game_get_flag(path_2_data.flag_index);
	dir_t dir_2 = path_2_data.flag_dir;

	int select = -1;
	if (FLAG_SERF_REQUESTED(flag_2, dir_2)) {
		for (int i = 1; i < globals.max_ever_serf_index; i++) {
			if (SERF_ALLOCATED(i)) {
				serf_t *serf = game_get_serf(i);

				if (serf->state == SERF_STATE_WALKING) {
					if (serf->s.walking.dest == path_1_data.flag_index &&
					    serf->s.walking.res == path_1_data.flag_dir) {
						select = 0;
						break;
					} else if (serf->s.walking.dest == path_2_data.flag_index &&
						   serf->s.walking.res == path_2_data.flag_dir) {
						select = 1;
						break;
					}
				} else if (serf->state == SERF_STATE_READY_TO_LEAVE_INVENTORY) {
					if (serf->s.ready_to_leave_inventory.dest == path_1_data.flag_index &&
					    serf->s.ready_to_leave_inventory.mode == path_1_data.flag_dir) {
						select = 0;
						break;
					} else if (serf->s.ready_to_leave_inventory.dest == path_2_data.flag_index &&
						   serf->s.ready_to_leave_inventory.mode == path_2_data.flag_dir) {
						select = 1;
						break;
					}
				} else if ((serf->state == SERF_STATE_READY_TO_LEAVE ||
					    serf->state == SERF_STATE_LEAVING_BUILDING) &&
					   serf->s.leaving_building.next_state == SERF_STATE_WALKING) {
					if (serf->s.leaving_building.dest == path_1_data.flag_index &&
					    serf->s.leaving_building.field_B == path_1_data.flag_dir) {
						select = 0;
						break;
					} else if (serf->s.leaving_building.dest == path_2_data.flag_index &&
						   serf->s.leaving_building.field_B == path_2_data.flag_dir) {
						select = 1;
						break;
					}
				}
			}
		}

		serf_path_info_t *path_data = &path_1_data;
		if (select == 0) path_data = &path_2_data;

		flag_t *selected_flag = game_get_flag(path_data->flag_index);
		selected_flag->length[path_data->flag_dir] &= ~BIT(7);
	}

	flag_t *flag = game_get_flag(MAP_OBJ_INDEX(pos));

	restore_path_serf_info(flag, path_1_dir, &path_1_data);
	restore_path_serf_info(flag, path_2_dir, &path_2_data);
}

/* Build flag at pos. */
void
game_build_flag(map_pos_t pos, player_sett_t *sett)
{
	flag_t *flag;
	int flg_index;
	int r = game_alloc_flag(&flag, &flg_index);
	if (r < 0) return;

	flag->path_con = sett->player_num << 6;

	map_tile_t *tiles = globals.map.tiles;

	flag->pos = pos;
	map_set_object(pos, MAP_OBJ_FLAG, flg_index);
	tiles[pos].flags |= BIT(7);
	/* move_map_resources(..); */

	if (MAP_PATHS(pos) != 0) {
		build_flag_split_path(pos);
	}
}

/* Build building at position. */
void
game_build_building(map_pos_t pos, building_type_t type, player_sett_t *sett)
{
	const int construction_cost[] = {
		0, 0, 2, 0, 2, 0, 3, 0, 2, 0,
		4, 1, 5, 0, 5, 0, 5, 0,
		2, 0, 4, 3, 1, 1, 4, 1, 2, 1, 4, 1, 3, 1, 2, 1,
		3, 2, 3, 2, 3, 3, 2, 1, 2, 3, 5, 5, 4, 1
	};

	const map_obj_t obj_types[] = {
		[BUILDING_NONE] = MAP_OBJ_NONE,
		[BUILDING_FISHER] = MAP_OBJ_SMALL_BUILDING,
		[BUILDING_LUMBERJACK] = MAP_OBJ_SMALL_BUILDING,
		[BUILDING_BOATBUILDER] = MAP_OBJ_SMALL_BUILDING,
		[BUILDING_STONECUTTER] = MAP_OBJ_SMALL_BUILDING,
		[BUILDING_STONEMINE] = MAP_OBJ_SMALL_BUILDING,
		[BUILDING_COALMINE] = MAP_OBJ_SMALL_BUILDING,
		[BUILDING_IRONMINE] = MAP_OBJ_SMALL_BUILDING,
		[BUILDING_GOLDMINE] = MAP_OBJ_SMALL_BUILDING,
		[BUILDING_FORESTER] = MAP_OBJ_SMALL_BUILDING,
		[BUILDING_STOCK] = MAP_OBJ_LARGE_BUILDING,
		[BUILDING_HUT] = MAP_OBJ_SMALL_BUILDING,
		[BUILDING_FARM] = MAP_OBJ_LARGE_BUILDING,
		[BUILDING_BUTCHER] = MAP_OBJ_LARGE_BUILDING,
		[BUILDING_PIGFARM] = MAP_OBJ_LARGE_BUILDING,
		[BUILDING_MILL] = MAP_OBJ_SMALL_BUILDING,
		[BUILDING_BAKER] = MAP_OBJ_LARGE_BUILDING,
		[BUILDING_SAWMILL] = MAP_OBJ_LARGE_BUILDING,
		[BUILDING_STEELSMELTER] = MAP_OBJ_LARGE_BUILDING,
		[BUILDING_TOOLMAKER] = MAP_OBJ_LARGE_BUILDING,
		[BUILDING_WEAPONSMITH] = MAP_OBJ_LARGE_BUILDING,
		[BUILDING_TOWER] = MAP_OBJ_LARGE_BUILDING,
		[BUILDING_FORTRESS] = MAP_OBJ_LARGE_BUILDING,
		[BUILDING_GOLDSMELTER] = MAP_OBJ_LARGE_BUILDING,
		[BUILDING_CASTLE] = MAP_OBJ_CASTLE
	};

	if (type == BUILDING_STOCK) {
		/* TODO Check that more stocks are allowed to be built */
	}

	building_t *bld;
	int bld_index;
	int r = game_alloc_building(&bld, &bld_index);
	if (r < 0) return;

	flag_t *flag = NULL;
	int flg_index = 0;
	if (MAP_OBJ(MAP_MOVE_DOWN_RIGHT(pos)) != MAP_OBJ_FLAG) {
		r = game_alloc_flag(&flag, &flg_index);
		if (r < 0) {
			game_free_building(bld_index);
			return;
		}
	}

	if (/*!BIT_TEST(player->sett->field_163, 0)*/0) {
		switch (type) {
		case BUILDING_LUMBERJACK:
			if (sett->lumberjack_index == 0) {
				sett->lumberjack_index = bld_index;
			}
			break;
		case BUILDING_SAWMILL:
			if (sett->sawmill_index == 0) {
				sett->sawmill_index = bld_index;
			}
			break;
		case BUILDING_STONECUTTER:
			if (sett->stonecutter_index == 0) {
				sett->stonecutter_index = bld_index;
			}
			break;
		default:
			break;
		}
	}

	map_tile_t *tiles = globals.map.tiles;

	bld->u.s.level = sett->building_height_after_level;
	bld->pos = pos;
	sett->incomplete_building_count[type] += 1;
	bld->bld = BIT(7) | (type << 2) | sett->player_num; /* bit 7: Unfinished building */
	bld->progress = 0;
	if (obj_types[type] != MAP_OBJ_SMALL_BUILDING) bld->progress = 1;

	int split_path = 0;
	if (MAP_OBJ(MAP_MOVE_DOWN_RIGHT(pos)) != MAP_OBJ_FLAG) {
		flag->path_con = sett->player_num << 6;
		if (MAP_PATHS(MAP_MOVE_DOWN_RIGHT(pos)) != 0) split_path = 1;
	} else {
		flg_index = MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(pos));
		flag = game_get_flag(flg_index);
	}

	flag->pos = MAP_MOVE_DOWN_RIGHT(pos);

	bld->flg_index = flg_index;
	flag->other_endpoint.b[DIR_UP_LEFT] = bld;
	flag->endpoint |= BIT(6);

	bld->stock[0].type = RESOURCE_PLANK;
	bld->stock[0].prio = 0;
	bld->stock[1].type = RESOURCE_STONE;
	bld->stock[1].prio = 0;

	flag->bld_flags = 0;
	flag->bld2_flags = 0;

	bld->u.s.planks_needed = construction_cost[2*type];
	bld->u.s.stone_needed = construction_cost[2*type+1];

	/* move_map_resources(pos, map_data); */
	/* TODO Resources should be moved, just set them to zero for now */
	tiles[pos].u.s.resource = 0;
	tiles[pos].u.s.field_1 = 0;

	map_set_object(pos, obj_types[type], bld_index);
	tiles[pos].flags |= BIT(1) | BIT(6);

	if (MAP_OBJ(MAP_MOVE_DOWN_RIGHT(pos)) != MAP_OBJ_FLAG) {
		/* move_map_resources(MAP_MOVE_DOWN_RIGHT(pos), map_data); */
		map_set_object(MAP_MOVE_DOWN_RIGHT(pos), MAP_OBJ_FLAG, flg_index);
		tiles[MAP_MOVE_DOWN_RIGHT(pos)].flags |= BIT(4) | BIT(7);
	}

	if (split_path) build_flag_split_path(MAP_MOVE_DOWN_RIGHT(pos));
}

static void
flag_remove_player_refs(flag_t *flag)
{
	for (int i = 0; i < 4; i++) {
		if (globals.player_sett[i]->index == FLAG_INDEX(flag)) {
			globals.player_sett[i]->index = 0;
		}
	}
}

/* Demolish flag at pos. */
void
game_demolish_flag(map_pos_t pos)
{
	const int max_transporters[] = { 1, 2, 3, 4, 6, 8, 11, 15 };

	/* Handle any serf at pos. */
	if (MAP_SERF_INDEX(pos) != 0) {
		serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));
		switch (serf->state) {
		case SERF_STATE_READY_TO_LEAVE:
		case SERF_STATE_LEAVING_BUILDING:
			serf->s.leaving_building.next_state = SERF_STATE_LOST;
			break;
		case SERF_STATE_FINISHED_BUILDING:
		case SERF_STATE_WALKING:
			if (MAP_PATHS(pos) == 0) {
				serf_log_state_change(serf, SERF_STATE_LOST);
				serf->state = SERF_STATE_LOST;
			}
			break;
		default:
			break;
		}
	}

	flag_t *flag = game_get_flag(MAP_OBJ_INDEX(pos));
	flag_remove_player_refs(flag);

	/* Handle connected flag. */
	if (MAP_PATHS(pos)) {
		dir_t path_1_dir = 0;
		dir_t path_2_dir = 0;

		/* Find first direction */
		for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
			if (BIT_TEST(MAP_PATHS(pos), d)) {
				path_1_dir = d;
				break;
			}
		}

		/* Find second direction */
		for (dir_t d = DIR_UP; d >= DIR_RIGHT; d--) {
			if (BIT_TEST(MAP_PATHS(pos), d)) {
				path_2_dir = d;
				break;
			}
		}

		serf_path_info_t path_1_data;
		serf_path_info_t path_2_data;

		fill_path_serf_info(pos, path_1_dir, &path_1_data);
		fill_path_serf_info(pos, path_2_dir, &path_2_data);

		flag_t *flag_1 = game_get_flag(path_1_data.flag_index);
		flag_t *flag_2 = game_get_flag(path_2_data.flag_index);
		dir_t dir_1 = path_1_data.flag_dir;
		dir_t dir_2 = path_2_data.flag_dir;

		flag_1->other_end_dir[dir_1] = (flag_1->other_end_dir[dir_1] & 0xc7) | (dir_2 << 3);
		flag_2->other_end_dir[dir_2] = (flag_2->other_end_dir[dir_2] & 0xc7) | (dir_1 << 3);

		flag_1->other_endpoint.f[dir_1] = flag_2;
		flag_2->other_endpoint.f[dir_2] = flag_1;

		flag_1->transporter &= ~BIT(dir_1);
		flag_2->transporter &= ~BIT(dir_2);

		int len = game_get_road_length_value(path_1_data.path_len + path_2_data.path_len);
		flag_1->length[dir_1] = len;
		flag_2->length[dir_2] = len;

		int max_serfs = max_transporters[FLAG_LENGTH_CATEGORY(flag_1, dir_1)];
		int serf_count = path_1_data.serf_count + path_2_data.serf_count;
		if (serf_count > 0) {
			flag_1->transporter |= BIT(dir_1);
			flag_2->transporter |= BIT(dir_2);

			if (serf_count > max_serfs) {
				/* TODO 59B8B */
			}

			flag_1->length[dir_1] += serf_count;
			flag_2->length[dir_2] += serf_count;
		}

		/* Update serfs with reference to this flag. */
		for (int i = 1; i < globals.max_ever_serf_index; i++) {
			if (SERF_ALLOCATED(i)) {
				serf_t *serf = game_get_serf(i);

				if (serf->state == SERF_STATE_READY_TO_LEAVE_INVENTORY &&
				    ((serf->s.ready_to_leave_inventory.dest == FLAG_INDEX(flag_1) &&
				      serf->s.ready_to_leave_inventory.mode == dir_1) ||
				     (serf->s.ready_to_leave_inventory.dest == FLAG_INDEX(flag_2) &&
				      serf->s.ready_to_leave_inventory.mode == dir_2))) {
					serf->s.ready_to_leave_inventory.dest = 0;
					serf->s.ready_to_leave_inventory.mode = -2;
				} else if (serf->state == SERF_STATE_WALKING &&
					   ((serf->s.walking.dest == FLAG_INDEX(flag_1) &&
					     serf->s.walking.res == dir_1) ||
					    (serf->s.walking.dest == FLAG_INDEX(flag_2) &&
					     serf->s.walking.res == dir_2))) {
					serf->s.walking.dest = 0;
					serf->s.walking.res = -2;
				} else if (serf->state == SERF_STATE_IDLE_IN_STOCK) {
					/* TODO */
				} else if ((serf->state == SERF_STATE_LEAVING_BUILDING ||
					    serf->state == SERF_STATE_READY_TO_LEAVE) &&
					   ((serf->s.leaving_building.dest == FLAG_INDEX(flag_1) &&
					     serf->s.leaving_building.field_B == dir_1) ||
					    (serf->s.leaving_building.dest == FLAG_INDEX(flag_2) &&
					     serf->s.leaving_building.field_B == dir_2)) &&
					   serf->s.leaving_building.next_state == SERF_STATE_WALKING) {
					serf->s.leaving_building.dest = 0;
					serf->s.leaving_building.field_B = -2;
				}
			}
		}
	}

	/* Clear map. */
	map_tile_t *tiles = globals.map.tiles;
	tiles[pos].flags &= ~BIT(7);

	/* Update serfs with reference to this flag. */
	for (int i = 1; i < globals.max_ever_serf_index; i++) {
		if (SERF_ALLOCATED(i)) {
			serf_t *serf = game_get_serf(i);

			if (serf->state == SERF_STATE_READY_TO_LEAVE_INVENTORY &&
			    serf->s.ready_to_leave_inventory.dest == FLAG_INDEX(flag)) {
				serf->s.ready_to_leave_inventory.dest = 0;
				serf->s.ready_to_leave_inventory.mode = -2;
			} else if (serf->state == SERF_STATE_WALKING &&
				   serf->s.walking.dest == FLAG_INDEX(flag)) {
				serf->s.walking.dest = 0;
				serf->s.walking.res = -2;
			} else if (serf->state == SERF_STATE_IDLE_IN_STOCK && 1/*...*/) {
				/* TODO */
			} else if ((serf->state == SERF_STATE_LEAVING_BUILDING ||
				    serf->state == SERF_STATE_READY_TO_LEAVE) &&
				   serf->s.leaving_building.dest == FLAG_INDEX(flag) &&
				   serf->s.leaving_building.next_state == SERF_STATE_WALKING) {
				serf->s.leaving_building.dest = 0;
				serf->s.leaving_building.field_B = -2;
			}
		}
	}

	map_set_object(pos, MAP_OBJ_NONE, 0);

	/* Remove resources from flag. */
	for (int i = 0; i < 8; i++) {
		if (flag->res_waiting[i] != 0) {
			int res = flag->res_waiting[i]-1;
			uint dest = flag->res_dest[i];
			lose_transported_resource(res, dest);
		}
	}

	game_free_flag(FLAG_INDEX(flag));
}

/* Demolish building at pos. */
void
game_demolish_building(map_pos_t pos)
{
	/* request redraw at pos */

	building_t *building = game_get_building(MAP_OBJ_INDEX(pos));
	building_remove_pl_sett_refs(building);

	player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
	map_tile_t *tiles = globals.map.tiles;

	if (BUILDING_IS_BURNING(building)) return;

	building->serf |= BIT(5);

	/* Remove path to building. */
	tiles[pos].flags &= ~BIT(1);
	tiles[MAP_MOVE_DOWN_RIGHT(pos)].flags &= ~BIT(4);

	/* Remove lost gold stock from total count. */
	if (BUILDING_IS_DONE(building) &&
	    (BUILDING_TYPE(building) == BUILDING_HUT ||
	     BUILDING_TYPE(building) == BUILDING_TOWER ||
	     BUILDING_TYPE(building) == BUILDING_FORTRESS ||
	     BUILDING_TYPE(building) == BUILDING_GOLDSMELTER)) {
		int gold_stock = building->stock[1].available;
		globals.map_gold_deposit -= gold_stock;
	}

	/* Update land owner ship if the building is military. */
	if (BUILDING_IS_DONE(building) &&
	    (BUILDING_TYPE(building) == BUILDING_HUT ||
	     BUILDING_TYPE(building) == BUILDING_TOWER ||
	     BUILDING_TYPE(building) == BUILDING_FORTRESS ||
	     BUILDING_TYPE(building) == BUILDING_CASTLE)) {
		game_update_land_ownership(building->pos);
	}

	if (BUILDING_IS_DONE(building) &&
	    (BUILDING_TYPE(building) == BUILDING_CASTLE ||
	     BUILDING_TYPE(building) == BUILDING_STOCK)) {
		/* Cancel resources in the out queue and remove gold
		   from map total. */
		if (BUILDING_IS_ACTIVE(building)) {
			inventory_t *inventory = building->u.inventory;

			for (int i = 0; i < 2 && inventory->out_queue[i] != -1; i++) {
				resource_type_t res = inventory->out_queue[i];
				int dest = inventory->out_dest[i];

				/* Remove gold from total count. */
				if (res == RESOURCE_GOLDBAR ||
				    res == RESOURCE_GOLDORE) {
					globals.map_gold_deposit -= 1;
				}

				flag_cancel_transported_stock(game_get_flag(dest), res+1);
			}

			globals.map_gold_deposit -= inventory->resources[RESOURCE_GOLDBAR];
			globals.map_gold_deposit -= inventory->resources[RESOURCE_GOLDORE];
		}

		/* Let some serfs escape while the building is burning. */
		int escaping_serfs = 0;
		for (int i = 1; i < globals.max_ever_serf_index; i++) {
			if (SERF_ALLOCATED(i)) {
				serf_t *serf = game_get_serf(i);

				if (serf->pos == building->pos &&
				    (serf->state == SERF_STATE_IDLE_IN_STOCK ||
				     serf->state == SERF_STATE_READY_TO_LEAVE_INVENTORY)) {
					if (escaping_serfs < 12) {
						/* Serf is escaping. */
						escaping_serfs += 1;
						serf->state = SERF_STATE_ESCAPE_BUILDING;
					} else {
						/* Kill this serf. */
						if (SERF_TYPE(serf) >= SERF_KNIGHT_0 &&
						    SERF_TYPE(serf) <= SERF_KNIGHT_4) {
							int score = 1 << (SERF_TYPE(serf)-SERF_KNIGHT_0);
							sett->total_military_score -= score;
						}
						sett->serf_count[SERF_TYPE(serf)] -= 1;
						game_free_serf(SERF_INDEX(serf));
					}
				}
			}
		}
	} else {
		building->serf &= ~BIT(4);
	}

	/* Remove stock from building. */
	building->stock[0].available = 0;
	building->stock[0].requested = 0;
	building->stock[1].available = 0;
	building->stock[1].requested = 0;

	building->serf &= ~BIT(3);

	int serf_index = building->serf_index;
	building->serf_index = 2047;
	building->u.anim = globals.anim;

	/* Update player sett fields. */
	if (BUILDING_IS_DONE(building)) {
		sett->total_building_score -= building_get_score_from_type(BUILDING_TYPE(building));

		if (BUILDING_TYPE(building) != BUILDING_CASTLE) {
			sett->completed_building_count[BUILDING_TYPE(building)] -= 1;
		}
	} else {
		sett->incomplete_building_count[BUILDING_TYPE(building)] -= 1;
	}

	if (BUILDING_HAS_SERF(building)) {
		building->serf &= ~BIT(6);

		if (BUILDING_IS_DONE(building) &&
		    BUILDING_TYPE(building) == BUILDING_CASTLE) {
			sett->build &= ~BIT(3);
			sett->castle_score -= 1;

			building->serf_index = 8191;

			if (sett->serf_index != 0) {
				serf_t *serf = game_get_serf(sett->serf_index);
				serf->type = (serf->type & 0x83) | (SERF_TRANSPORTER << 2);
				serf->counter = 0;

				if (MAP_SERF_INDEX(serf->pos) == SERF_INDEX(serf)) {
					serf_log_state_change(serf, SERF_STATE_LOST);
					serf->state = SERF_STATE_LOST;
					serf->s.lost.field_B = 0;
				} else {
					serf_log_state_change(serf, SERF_STATE_ESCAPE_BUILDING);
					serf->state = SERF_STATE_ESCAPE_BUILDING;
				}
			}
		}

		if (BUILDING_IS_DONE(building) &&
		    (BUILDING_TYPE(building) == BUILDING_HUT ||
		     BUILDING_TYPE(building) == BUILDING_TOWER ||
		     BUILDING_TYPE(building) == BUILDING_FORTRESS ||
		     BUILDING_TYPE(building) == BUILDING_CASTLE)) {
			while (serf_index != 0) {
				serf_t *serf = game_get_serf(serf_index);
				serf_index = serf->s.defending.next_knight;

				if (MAP_SERF_INDEX(serf->pos) == SERF_INDEX(serf)) {
					serf_log_state_change(serf, SERF_STATE_LOST);
					serf->state = SERF_STATE_LOST;
					serf->s.lost.field_B = 0;
				} else {
					serf_log_state_change(serf, SERF_STATE_ESCAPE_BUILDING);
					serf->state = SERF_STATE_ESCAPE_BUILDING;
				}
			}
		} else {
			serf_t *serf = game_get_serf(serf_index);
			if (SERF_TYPE(serf) == SERF_4) {
				serf->type = (serf->type & 0x83) | (SERF_TRANSPORTER << 2);
			}

			serf->counter = 0;

			if (MAP_SERF_INDEX(serf->pos) == SERF_INDEX(serf)) {
				serf_log_state_change(serf, SERF_STATE_LOST);
				serf->state = SERF_STATE_LOST;
				serf->s.lost.field_B = 0;
			} else {
				serf_log_state_change(serf, SERF_STATE_ESCAPE_BUILDING);
				serf->state = SERF_STATE_ESCAPE_BUILDING;
			}
		}
	}

	/* Flag. */
	flag_t *flag = game_get_flag(building->flg_index);
	flag->other_endpoint.b[DIR_UP_LEFT] = NULL;
	flag->endpoint &= ~BIT(6);

	flag->bld_flags = 0;
	flag->bld2_flags = 0;

	flag_reset_transport(flag);

	map_pos_t flag_pos = MAP_MOVE_DOWN_RIGHT(pos);
	if (MAP_PATHS(flag_pos) == 0 &&
	    MAP_OBJ(flag_pos) == MAP_OBJ_FLAG) {
		game_demolish_flag(flag_pos);
	}
}

/* Calculate the flag state of military buildings (distance to enemy). */
void
game_calculate_military_flag_state(building_t *building)
{
	const int border_check_offsets[] = {
		31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
		100, 101, 102, 103, 104, 105, 106, 107, 108,
		259, 260, 261, 262, 263, 264,
		241, 242, 243, 244, 245, 246,
		217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228,
		247, 248, 249, 250, 251, 252,
		-1,

		265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276,
		-1,

		277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288,
		289, 290, 291, 292, 293, 294,
		-1
	};

	int f, k;
	for (f = 3, k = 0; f > 0; f--) {
		int offset;
		while ((offset = border_check_offsets[k++]) >= 0) {
			map_pos_t check_pos = MAP_POS_ADD(building->pos,
							  globals.spiral_pos_pattern[offset]);
			if (MAP_HAS_OWNER(check_pos) &&
			    MAP_OWNER(check_pos) != BUILDING_PLAYER(building)) {
				goto break_loops;
			}
		}
	}

break_loops:
	building->serf = (building->serf & 0xfc) | f;
}

/* Map pos is lost to the owner, demolish everything. */
static void
game_surrender_land(map_pos_t pos)
{
	/* Remove building. */
	if (MAP_OBJ(pos) >= MAP_OBJ_SMALL_BUILDING &&
	    MAP_OBJ(pos) <= MAP_OBJ_CASTLE) {
		game_demolish_building(pos);
	}

	if (!MAP_HAS_FLAG(pos) && MAP_PATHS(pos) != 0) {
		game_demolish_road(pos);
	}

	int remove_roads = MAP_HAS_FLAG(pos);

	/* Remove roads and building around pos. */
	for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
		map_pos_t p = MAP_POS_ADD(pos, globals.spiral_pos_pattern[1+d]);

		if (MAP_OBJ(p) >= MAP_OBJ_SMALL_BUILDING &&
		    MAP_OBJ(p) <= MAP_OBJ_CASTLE) {
			game_demolish_building(p);
		}

		if (remove_roads &&
		    (MAP_PATHS(p) & BIT(DIR_REVERSE(d)))) {
			game_demolish_road(p);
		}
	}

	/* Remove flag. */
	if (MAP_OBJ(pos) == MAP_OBJ_FLAG) {
		game_demolish_flag(pos);
	}
}

/* Update land ownership around map position. */
void
game_update_land_ownership(map_pos_t init_pos)
{
	/* Currently the below algorithm will only work when
	   both influence_radius and calculate_radius are 8. */
	const int influence_radius = 8;
	const int influence_diameter = 1 + 2*influence_radius;

	int calculate_radius = influence_radius;
	int calculate_diameter = 1 + 2*calculate_radius;

	int *temp_arr = calloc(4*calculate_diameter*calculate_diameter, sizeof(int));
	if (temp_arr == NULL) abort();

	const int military_influence[] = {
		0, 1, 2, 4, 7, 12, 18, 29, -1, -1,	/* hut */
		0, 3, 5, 8, 11, 15, 22, 30, -1, -1,	/* tower */
		0, 6, 10, 14, 19, 23, 27, 31, -1, -1	/* fortress */
	};

	const int map_closeness[] = {
		1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
		1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0,
		1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, 0, 0, 0, 0, 0,
		1, 2, 3, 4, 4, 4, 4, 4, 4, 3, 2, 1, 0, 0, 0, 0, 0,
		1, 2, 3, 4, 5, 5, 5, 5, 5, 4, 3, 2, 1, 0, 0, 0, 0,
		1, 2, 3, 4, 5, 6, 6, 6, 6, 5, 4, 3, 2, 1, 0, 0, 0,
		1, 2, 3, 4, 5, 6, 7, 7, 7, 6, 5, 4, 3, 2, 1, 0, 0,
		1, 2, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 2, 1, 0,
		1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 7, 6, 5, 4, 3, 2, 1,
		0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 2, 1,
		0, 0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 6, 5, 4, 3, 2, 1,
		0, 0, 0, 1, 2, 3, 4, 5, 6, 6, 6, 6, 5, 4, 3, 2, 1,
		0, 0, 0, 0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 4, 3, 2, 1,
		0, 0, 0, 0, 0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 3, 2, 1,
		0, 0, 0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1,
		0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1,
		0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1
	};

	/* Find influence from buildings in 33*33 square
	   around the center. */
	for (int i = -(influence_radius+calculate_radius);
	     i <= influence_radius+calculate_radius; i++) {
		for (int j = -(influence_radius+calculate_radius);
		     j <= influence_radius+calculate_radius; j++) {
			map_pos_t pos = MAP_POS_ADD(init_pos,
						    MAP_POS(j & globals.map.col_mask,
							    i & globals.map.row_mask));

			if (MAP_OBJ(pos) >= MAP_OBJ_SMALL_BUILDING &&
			    MAP_OBJ(pos) <= MAP_OBJ_CASTLE &&
			    BIT_TEST(MAP_PATHS(pos), DIR_DOWN_RIGHT)) { /* TODO Why wouldn't this be set? */
				building_t *building = game_get_building(MAP_OBJ_INDEX(pos));
				int mil_type = -1;

				if (BUILDING_TYPE(building) == BUILDING_CASTLE) {
					/* Castle has military influence even when not done. */
					mil_type = 2;
				} else if (BUILDING_IS_DONE(building) &&
					   BUILDING_IS_ACTIVE(building)) {
					switch (BUILDING_TYPE(building)) {
					case BUILDING_HUT: mil_type = 0; break;
					case BUILDING_TOWER: mil_type = 1; break;
					case BUILDING_FORTRESS: mil_type = 2; break;
					default: break;
					}
				}

				if (mil_type >= 0 &&
				    !BUILDING_IS_BURNING(building)) {
					const int *influence = military_influence + 10*mil_type;
					const int *closeness = map_closeness +
						influence_diameter*max(-i, 0) + max(-j, 0);
					int *arr = temp_arr +
						(BUILDING_PLAYER(building) * calculate_diameter*calculate_diameter) +
						calculate_diameter*max(i, 0) + max(j, 0);

					for (int k = 0; k < influence_diameter - abs(i); k++) {
						for (int l = 0; l < influence_diameter - abs(j); l++) {
							int inf = influence[*closeness];
							if (inf < 0) *arr = 128;
							else if (*arr < 128) *arr = min(*arr + inf, 127);

							closeness += 1;
							arr += 1;
						}
						closeness += abs(j);
						arr += abs(j);
					}
				}
			}
		}
	}

	map_tile_t *tiles = globals.map.tiles;

	/* Update owner of 17*17 square. */
	for (int i = -calculate_radius; i <= calculate_radius; i++) {
		for (int j = -calculate_radius; j <= calculate_radius; j++) {
			int max_val = 0;
			int player = -1;
			for (int p = 0; p < 4; p++) {
				int *arr = temp_arr +
					p*calculate_diameter*calculate_diameter +
					calculate_diameter*(i+calculate_radius) + (j+calculate_radius);
				if (*arr > max_val) {
					max_val = *arr;
					player = p;
				}
			}

			map_pos_t pos = MAP_POS_ADD(init_pos,
						    MAP_POS(j & globals.map.col_mask,
							    i & globals.map.row_mask));
			if (player >= 0) {
				if (MAP_HAS_OWNER(pos) &&
				    MAP_OWNER(pos) != player) {
					int old_player = MAP_OWNER(pos);
					globals.player_sett[old_player]->total_land_area -= 1;
					game_surrender_land(pos);
				}

				globals.player_sett[player]->total_land_area += 1;
				tiles[pos].height = (1 << 7) | (player << 5) | MAP_HEIGHT(pos);
			} else {
				game_surrender_land(pos);
				tiles[pos].height = (0 << 7) | (0 << 5) | MAP_HEIGHT(pos);
			}
		}
	}

	free(temp_arr);

	/* Update military building flag state. */
	for (int i = -25; i <= 25; i++) {
		for (int j = -25; j <= 25; j++) {
			map_pos_t pos = MAP_POS_ADD(init_pos,
						    MAP_POS(i & globals.map.col_mask,
							    j & globals.map.row_mask));

			if (MAP_OBJ(pos) >= MAP_OBJ_SMALL_BUILDING &&
			    MAP_OBJ(pos) <= MAP_OBJ_CASTLE &&
			    BIT_TEST(MAP_PATHS(pos), DIR_DOWN_RIGHT)) {
				building_t *building = game_get_building(MAP_OBJ_INDEX(pos));
				if ((BUILDING_IS_DONE(building) &&
				     (BUILDING_TYPE(building) == BUILDING_HUT ||
				      BUILDING_TYPE(building) == BUILDING_TOWER ||
				      BUILDING_TYPE(building) == BUILDING_FORTRESS)) ||
				    BUILDING_TYPE(building) == BUILDING_CASTLE) {
					game_calculate_military_flag_state(building);
				}
			}
		}
	}
}

static void
game_demolish_flag_and_roads(map_pos_t pos)
{
	if (MAP_HAS_FLAG(pos)) {
		/* Remove roads around pos. */
		for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
			map_pos_t p = MAP_MOVE(pos, d);

			if (MAP_PATHS(p) & BIT(DIR_REVERSE(d))) {
				game_demolish_road(p);
			}
		}

		if (MAP_OBJ(pos) == MAP_OBJ_FLAG) {
			game_demolish_flag(pos);
		}
	} else if (MAP_PATHS(pos) != 0) {
		game_demolish_road(pos);
	}
}

/* The given building has been defeated and is being
   occupied by player. */
void
game_occupy_enemy_building(building_t *building, int player)
{
	/* Take the building. */
	player_sett_t *def_sett = globals.player_sett[BUILDING_PLAYER(building)];
	player_add_notification(def_sett, 2 + (player << 5), building->pos);

	player_sett_t *sett = globals.player_sett[player];
	player_add_notification(sett, 3 + (player << 5), building->pos);

	if (BUILDING_TYPE(building) == BUILDING_CASTLE) {
		sett->castle_score += 1;
		game_demolish_building(building->pos);
	} else {
		flag_t *flag = game_get_flag(building->flg_index);
		flag_reset_transport(flag);

		/* Update player scores. */
		def_sett->total_building_score -=
			building_get_score_from_type(BUILDING_TYPE(building));
		def_sett->total_land_area -= 7;
		def_sett->completed_building_count[BUILDING_TYPE(building)] -= 1;

		sett->total_building_score +=
			building_get_score_from_type(BUILDING_TYPE(building));
		sett->total_land_area += 7;
		sett->completed_building_count[BUILDING_TYPE(building)] += 1;

		/* Demolish nearby buildings. */
		for (int i = 0; i < 12; i++) {
			map_pos_t pos = MAP_POS_ADD(building->pos,
						    globals.spiral_pos_pattern[7+i]);
			if (MAP_OBJ(pos) >= MAP_OBJ_SMALL_BUILDING &&
			    MAP_OBJ(pos) <= MAP_OBJ_CASTLE) {
				game_demolish_building(pos);
			}
		}

		/* Change owner of land and remove roads and flags
		   except the flag associated with the building. */
		map_tile_t *tiles = globals.map.tiles;
		tiles[building->pos].height = (1 << 7) | (player << 5) |
			MAP_HEIGHT(building->pos);

		for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
			map_pos_t pos = MAP_MOVE(building->pos, d);
			tiles[pos].height = (1 << 7) | (player << 5) | MAP_HEIGHT(pos);
			if (pos != flag->pos) {
				game_demolish_flag_and_roads(pos);
			}
		}

		/* Change owner of flag. */
		flag->path_con = (player << 6) | FLAG_PATHS(flag);

		/* Reset destination of stolen resources. */
		for (int i = 0; i < 8; i++) {
			if (flag->res_waiting[i] != 0) {
				resource_type_t res = flag->res_waiting[i]-1;
				lose_transported_resource(res, flag->res_dest[i]);

				/* Since the resource is not actually lost but
				   merely changes owner, we have to readjust the total
				   map gold. */
				if (res == RESOURCE_GOLDORE ||
				    res == RESOURCE_GOLDBAR) {
					globals.map_gold_deposit += 1;
				}
				flag->res_dest[i] = 0;
			}
		}

		/* Remove paths from flag. */
		for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
			if (FLAG_HAS_PATH(flag, d)) {
				game_demolish_road(MAP_MOVE(flag->pos, d));
			}
		}

		/* Change owner of building */
		building->bld = (building->bld & 0xfc) | player;

		game_update_land_ownership(building->pos);

		if (BIT_TEST(sett->flags, 7)) {
			/* TODO AI */
		}
	}
}

/* mode: 0: IN, 1: STOP, 2: OUT */
void
game_set_inventory_resource_mode(inventory_t *inventory, int mode)
{
	flag_t *flag = game_get_flag(inventory->flg_index);

	if (mode == 0) {
		inventory->res_dir = (inventory->res_dir & 0xfc) | 0;
	} else if (mode == 1) {
		inventory->res_dir = (inventory->res_dir & 0xfc) | 1;
	} else {
		inventory->res_dir = (inventory->res_dir & 0xfc) | 3;
	}

	if (mode > 0) {
		flag->bld2_flags &= ~BIT(7);

		/* Clear destination of serfs with resources destined
		   for this inventory. */
		int dest = FLAG_INDEX(flag);
		for (int i = 1; i < globals.max_ever_serf_index; i++) {
			if (SERF_ALLOCATED(i)) {
				serf_t *serf = game_get_serf(i);

				switch (serf->state) {
				case SERF_STATE_TRANSPORTING:
					if (serf->s.walking.dest == dest) {
						serf->s.walking.dest = 0;
					}
					break;
				case SERF_STATE_DROP_RESOURCE_OUT:
					if (serf->s.move_resource_out.res_dest == dest) {
						serf->s.move_resource_out.res_dest = 0;
					}
					break;
				case SERF_STATE_LEAVING_BUILDING:
					if (serf->s.leaving_building.dest == dest &&
					    serf->s.leaving_building.next_state == SERF_STATE_DROP_RESOURCE_OUT) {
						serf->s.leaving_building.dest = 0;
					}
					break;
				case SERF_STATE_MOVE_RESOURCE_OUT:
					if (serf->s.move_resource_out.res_dest == dest &&
					    serf->s.move_resource_out.next_state == SERF_STATE_DROP_RESOURCE_OUT) {
						serf->s.move_resource_out.res_dest = 0;
					}
					break;
				default:
					break;
				}
			}
		}
	} else {
		flag->bld2_flags |= BIT(7);
	}
}

/* mode: 0: IN, 1: STOP, 2: OUT */
void
game_set_inventory_serf_mode(inventory_t *inventory, int mode)
{
	flag_t *flag = game_get_flag(inventory->flg_index);

	if (mode == 0) {
		inventory->res_dir = (inventory->res_dir & 0xf3) | (0 << 2);
	} else if (mode == 1) {
		inventory->res_dir = (inventory->res_dir & 0xf3) | (1 << 2);
	} else {
		inventory->res_dir = (inventory->res_dir & 0xf3) | (3 << 2);
	}

	if (mode > 0) {
		flag->bld_flags &= ~BIT(7);

		/* Clear destination of serfs destined for this inventory. */
		int dest = FLAG_INDEX(flag);
		for (int i = 1; i < globals.max_ever_serf_index; i++) {
			if (SERF_ALLOCATED(i)) {
				serf_t *serf = game_get_serf(i);

				switch (serf->state) {
				case SERF_STATE_WALKING:
					if (serf->s.walking.dest == dest &&
					    serf->s.walking.res < 0) {
						serf->s.walking.res = -2;
						serf->s.walking.dest = 0;
					}
					break;
				case SERF_STATE_READY_TO_LEAVE_INVENTORY:
					if (serf->s.ready_to_leave_inventory.dest == dest &&
					    serf->s.ready_to_leave_inventory.mode < 0) {
						serf->s.ready_to_leave_inventory.mode = -2;
						serf->s.ready_to_leave_inventory.dest = 0;
					}
					break;
				case SERF_STATE_LEAVING_BUILDING:
				case SERF_STATE_READY_TO_LEAVE:
					if (serf->s.leaving_building.dest == dest &&
					    serf->s.leaving_building.field_B < 0 &&
					    serf->s.leaving_building.next_state == SERF_STATE_WALKING) {
						serf->s.leaving_building.field_B = -2;
						serf->s.leaving_building.dest = 0;
					}
					break;
				default:
					break;
				}
			}
		}
	} else {
		flag->bld_flags |= BIT(7);
	}
}
