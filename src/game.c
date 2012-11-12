/* game.c */

/* Functions that concern the game play as a whole,
   instead of any specific game object or across multiple
   different game objects. */

#include <stdlib.h>
#include <assert.h>

#include "map.h"
#include "player.h"
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
					f->stock1_prio = 0;
					f->bld2_flags = 0;
					f->stock2_prio = 0;
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
	assert(globals.flg_bitmap[index/8] & BIT(7-(index&7)));
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
			if (globals.flg_bitmap[index/8] & BIT(7-(index&7))) break;
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
					b->stock1 = 0;
					b->stock2 = 0;
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
	assert(globals.buildings_bitmap[index/8] & BIT(7-(index&7)));
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
			if (globals.buildings_bitmap[index/8] & BIT(7-(index&7))) break;
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
	assert(globals.inventories_bitmap[index/8] & BIT(7-(index&7)));
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
			if (globals.inventories_bitmap[index/8] & BIT(7-(index&7))) break;
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
	assert(globals.serfs_bitmap[index/8] & BIT(7-(index&7)));
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
			if (globals.serfs_bitmap[index/8] & BIT(7-(index&7))) break;
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
		if (BIT_TEST(globals.inventories_bitmap[i>>3], 7-(i&7))) {
			inventory_t *loop_inv = game_get_inventory(i);
			if (loop_inv->player_num == sett->player_num ||
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

	if (BIT_TEST(sett->flags, 0)) { /* Has castle */
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
		if (BIT_TEST(globals.buildings_bitmap[i>>3], 7-(i&7))) {
			building_t *building = game_get_building(i);
			building->serf &= ~BIT(2);
		}
	}

	/* TODO Approximately right */
	for (int i = 1; i < globals.max_ever_flag_index; i++) {
		if (BIT_TEST(globals.flg_bitmap[i>>3], 7-(i&7))) {
			flag_t *flag = game_get_flag(i);
			flag->transporter &= ~BIT(7);
		}
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
	const int *arr;
	int *max_prio;
	flag_t **flags;
} update_ai_and_more_data_t;

static int
update_ai_and_more_search_cb(flag_t *flag, update_ai_and_more_data_t *data)
{
	int inv = flag->search_dir;
	if (data->max_prio[inv] < 255) {
		if ((data->arr[0] == 66 && BIT_TEST(flag->bld_flags, data->arr[1]))) {
			if (flag->stock1_prio >= 16 &&
			    flag->stock1_prio > data->max_prio[inv]) {
				data->max_prio[inv] = flag->stock1_prio;
				data->flags[inv] = flag;
			}
		} else if ((data->arr[0] == 68 && BIT_TEST(flag->bld2_flags, data->arr[1]))) {
			if (flag->stock2_prio >= 16 &&
			    flag->stock2_prio > data->max_prio[inv]) {
				data->max_prio[inv] = flag->stock2_prio;
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
		66, 1, RESOURCE_PLANK,
		68, 4, RESOURCE_STONE,
		68, 2, RESOURCE_STEEL,
		66, 2, RESOURCE_COAL,
		68, 5, RESOURCE_LUMBER,
		68, 1, RESOURCE_IRONORE,
		66, 0, -1,
		66, 3, RESOURCE_PIG,
		66, 5, RESOURCE_FLOUR,
		66, 4, RESOURCE_WHEAT,
		68, 3, RESOURCE_GOLDBAR,
		68, 0, RESOURCE_GOLDORE,
		-1
	};

	const int arr_2[] = {
		68, 4, RESOURCE_STONE,
		68, 1, RESOURCE_IRONORE,
		68, 0, RESOURCE_GOLDORE,
		66, 2, RESOURCE_COAL,
		68, 2, RESOURCE_STEEL,
		68, 3, RESOURCE_GOLDBAR,
		66, 0, -1,
		66, 3, RESOURCE_PIG,
		66, 5, RESOURCE_FLOUR,
		66, 4, RESOURCE_WHEAT,
		68, 5, RESOURCE_LUMBER,
		66, 1, RESOURCE_PLANK,
		-1
	};

	const int arr_3[] = {
		66, 0, -1,
		66, 4, RESOURCE_WHEAT,
		66, 3, RESOURCE_PIG,
		66, 5, RESOURCE_FLOUR,
		68, 3, RESOURCE_GOLDBAR,
		68, 4, RESOURCE_STONE,
		66, 1, RESOURCE_PLANK,
		68, 2, RESOURCE_STEEL,
		66, 2, RESOURCE_COAL,
		68, 5, RESOURCE_LUMBER,
		68, 0, RESOURCE_GOLDORE,
		68, 1, RESOURCE_IRONORE,
		-1
	};

	globals.next_index += 1;
	if (globals.next_index == globals.field_286) globals.next_index = 0;

	if (globals.next_index >= 32) {
		/* 1F89E */
		if (globals.next_index == 32) {
			/* 1FC1D */
			/*calc_knight_morale();*/
			if (globals.game_speed == 0) return;

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
						if (BIT_TEST(globals.inventories_bitmap[i>>3], 7-(i&7))) {
							inventory_t *inventory = game_get_inventory(i);
							if (inventory->player_num == p &&
							    inventory->out_queue[1] == -1) {
								int res_dir = inventory->res_dir & 3;
								if (res_dir == 0 || res_dir == 1) { /* In mode, stop mode */
									if (arr[2] < 0) {
										if (inventory->resources[RESOURCE_FISH] != 0 ||
										    inventory->resources[RESOURCE_MEAT] != 0 ||
										    inventory->resources[RESOURCE_BREAD] != 0) {
											invs[n++] = inventory;
											if (n == 256) break;
										}
									} else if (inventory->resources[arr[2]] != 0) {
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
						data.arr = arr;
						data.max_prio = max_prio;
						data.flags = flags;
						flag_search_execute(&search, (flag_search_func *)update_ai_and_more_search_cb, 1, &data);

						for (int i = 0; i < n; i++) {
							if (max_prio[i] > 0) {
								LOGV("game", " dest for inventory %i found", i);
								building_t *dest_bld = flags[i]->other_endpoint.b[DIR_UP_LEFT];
								inventory_t *src_inv = invs[i];
								if (arr[0] == 66) {
									flags[i]->stock1_prio = 0;
									dest_bld->stock1 += 1;
								} else if (arr[0] == 68) {
									flags[i]->stock2_prio = 0;
									dest_bld->stock2 += 1;
								}

								resource_type_t res = arr[2];
								if (arr[2] < 0) {
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
				arr += 3;
			}
		} else {
			/* TODO */
		}
	} else if (globals.game_speed > 0 && globals.max_ever_flag_index < 50) {
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
	if (BIT_TEST(flag->bld_flags, 6)) { /* Castle flag */
		/* Castle reached */
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
	dir_t dir_2 = (src->other_end_dir[dir] >> 3) & 7;

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

	int r = flag_search_execute(&search, (flag_search_func *)send_serf_to_road_search_cb, 0, &data);
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

	serf_t *serf = game_get_serf(serf_index);

	inventory->serfs[SERF_4] += 1;
	flag_t *dest_flag = game_get_flag(inventory->flg_index);

	src->length[dir] |= BIT(7);
	src_2->length[dir_2] |= BIT(7);

	if (dest_flag->search_dir == src_2->search_dir) {
		src = src_2;
		dir = dir_2;
	}

	serf = game_get_serf(serf_index);

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
	if (BIT_TEST(flag->bld2_flags, 7)) {
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
			   1, &dest);
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
				player_sett_t *sett = globals.player_sett[(flag->path_con >> 6) & 3];
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
	const int *arr;
	int max_prio;
	flag_t *flag;
} update_flags_search2_t;

static int
update_flags_search2_cb(flag_t *flag, update_flags_search2_t *data)
{
	if (data->arr[1] == 66 && BIT_TEST(flag->bld_flags, data->arr[0])) {
		if (flag->stock1_prio > data->max_prio) {
			data->max_prio = flag->stock1_prio;
			data->flag = flag;
		}
	} else if (data->arr[1] == 68 && BIT_TEST(flag->bld2_flags, data->arr[0])) {
		if (flag->stock2_prio > data->max_prio) {
			data->max_prio = flag->stock2_prio;
			data->flag = flag;
		}
	}

	if (data->max_prio > 204) return 1;

	return 0;
}

/* Update flags as part of the game progression. */
static void
update_flags()
{
	const int arr[] = { 1, 2, 3, 4, 6, 8, 11, 15 };

	const int arr2[] = {
		-1, -1, 0, 66, 3, 66, 0, 66, 4, 66,
		5, 66, 0, 66, 5, 68, 1, 66, -1, -1,
		4, 68, 1, 68, 2, 68, 2, 66, 0, 68,
		3, 68, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1
	};

	if (globals.next_index >= 32) return;

	int index = globals.next_index << 5;
	for (int i = index ? index : 1; i < globals.max_ever_flag_index; i++) {
		if (BIT_TEST(globals.flg_bitmap[i>>3], 7-(i&7))) {
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

			if (BIT_TEST(flag->endpoint, 7)) { /* Resources waiting */
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
								int tr = flag->transporter & 0x3f;

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
												    1, &data);
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
								int res = flag->res_waiting[slot] & 0x1f;
								if (arr2[2*res] >= 0) {
									flag_search_t search;
									flag_search_init(&search);
									flag_search_add_source(&search, flag);

									update_flags_search2_t data;
									data.arr = &arr2[2*res];
									data.flag = NULL;
									data.max_prio = 0;

									flag_search_execute(&search,
											    (flag_search_func *)update_flags_search2_cb,
											    1, &data);
									if (data.flag != NULL) {
										LOGV("game", "dest for flag %u res %i found: flag %u",
										     FLAG_INDEX(flag), slot, FLAG_INDEX(data.flag));
										building_t *dest_bld = data.flag->other_endpoint.b[DIR_UP_LEFT];
										int prio = (arr2[2*res+1] == 66) ? data.flag->stock1_prio :
											data.flag->stock2_prio;
										if ((prio & 1) == 0) prio = 0;
										if (arr2[2*res+1] == 66) {
											data.flag->stock1_prio = prio >> 1;
											dest_bld->stock1 += 1;
										} else {
											data.flag->stock2_prio = prio >> 1;
											dest_bld->stock2 += 1;
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
			int path = flag->path_con & 0x3f;
			if (globals.field_24E >= 7) path |= BIT(7);

			for (int j = 0; j < 6; j++) {
				if (BIT_TEST(path, 5-j)) {
					if (BIT_TEST(flag->length[5-j], 7)) {
						if (BIT_TEST(flags, 5-j)) {
							if (BIT_TEST(path, 7)) tr &= BIT(5-j);
						} else {
							if ((flag->length[5-j] & 0xf) != 0) tr |= BIT(5-j);
						}
					} else if ((flag->length[5-j] & 0xf) == 0) {
						if (!BIT_TEST(tr, 7)) {
							int r = 0;
							if (BIT_TEST(flag->endpoint, 5-j)) r = send_serf_to_road(flag, 5-j, 0);
							else r = send_serf_to_road(flag, 5-j, 1);
							if (r < 0) tr |= BIT(7);
						}
						if (BIT_TEST(path, 7)) tr &= BIT(5-j);
					} else if (BIT_TEST(flags, 5-j)) {
						if (arr[(flag->length[5-j] >> 4) & 7] != (flag->length[5-j] & 0xf)) {
							if (!BIT_TEST(tr, 7)) {
								int r = 0;
								if (BIT_TEST(flag->endpoint, 5-j)) r = send_serf_to_road(flag, 5-j, 0);
								else r = send_serf_to_road(flag, 5-j, 1);
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
	if (BIT_TEST(flag->bld_flags, 6)) { /* Castle flag */
		/* Castle reached */
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

				data->building->stock1 += 1;
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

	int r = flag_search_single(dest, (flag_search_func *)send_serf_to_flag_search_cb, 0, &data);
	if (r == 0) {
		return 0;
	} else if (data.inventory != NULL) {
		inventory_t *inventory = data.inventory;
		serf_t *serf = game_get_serf(inventory->serfs[SERF_GENERIC]);

		inventory->serfs[SERF_GENERIC] = 0;
		inventory->spawn_priority -= 1;

		if (type < 0) {
			/* Knight */
			building->stock1 += 1;
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
	if (!BIT_TEST(building->serf, 2) &&
			!BIT_TEST(building->serf, 6) &&
			!BIT_TEST(building->serf, 7)) {
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

	flag_t *flag = game_get_flag(building->flg_index);

	/* Request planks */
	/*if (BIT_TEST(sett->field_163, 1) &&
			sett->lumberjack_index != BUILDING_INDEX(building) &&
			sett->sawmill_index != BUILDING_INDEX(building) &&
			sett->stonecutter_index != BUILDING_INDEX(building)) {
		TODO
	}*/

	int total_planks = ((building->stock1 >> 4) & 0xf) + (building->stock1 & 0xf);
	if (total_planks < 8 && total_planks != building->u.s.planks_needed) {
		int planks_prio = sett->planks_construction >> (8 + total_planks);
		if (!BIT_TEST(building->serf, 6)) planks_prio >>= 2;
		flag->stock1_prio = planks_prio & ~BIT(0);
	} else {
		flag->stock1_prio = 0;
	}

	/* Request stone */
	/*if (BIT_TEST(sett->field_163, 2) &&
			sett->lumberjack_index != BUILDING_INDEX(building) &&
			sett->sawmill_index != BUILDING_INDEX(building) &&
			sett->stonecutter_index != BUILDING_INDEX(building)) {
		TODO
	}*/

	int total_stone = ((building->stock2 >> 4) & 0xf) + (building->stock2 & 0xf);
	if (total_stone < 8 && total_stone != building->u.s.stone_needed) {
		int stone_prio = 0xff >> total_stone;
		if (!BIT_TEST(building->serf, 6)) stone_prio >>= 2;
		flag->stock2_prio = stone_prio & ~BIT(0);
	} else {
		flag->stock2_prio = 0;
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

	if (BIT_TEST(building->serf, 6) ||
			BIT_TEST(building->serf, 7)) {
		return;
	}

	/* TODO */

	if (!BIT_TEST(building->serf, 2)) {
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

static void
handle_building_update(building_t *building)
{
	building_type_t type = BUILDING_TYPE(building);
	if (!BIT_TEST(building->bld, 7)) { /* Finished */
		switch (type) {
		case BUILDING_NONE:
			break;
		case BUILDING_FISHER:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_FISHER,
							      RESOURCE_ROD, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			break;
		case BUILDING_LUMBERJACK:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_LUMBERJACK,
							      RESOURCE_AXE, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			break;
		case BUILDING_BOATBUILDER:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_BOATBUILDER,
							      RESOURCE_HAMMER, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BIT_TEST(building->serf, 6)) {
				player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
				int total_tree = ((building->stock1 >> 4) & 0xf) + (building->stock1 & 0xf);
				if (total_tree < 8 && 1/*!BIT_TEST(sett->field_163, 1)*/) {
					building->u.flag->stock1_prio = sett->planks_boatbuilder >> (8 + total_tree);
				} else {
					building->u.flag->stock1_prio = 0;
				}
			}
			break;
		case BUILDING_STONECUTTER:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_STONECUTTER,
							      RESOURCE_PICK, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			break;
		case BUILDING_STONEMINE:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_MINER,
							      RESOURCE_PICK, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BIT_TEST(building->serf, 6)) {
				int total_food = ((building->stock1 >> 4) & 0xf) + (building->stock1 & 0xf);
				if (total_food < 8) {
					player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
					building->u.flag->stock1_prio = sett->food_stonemine >> (8 + total_food);
				} else {
					building->u.flag->stock1_prio = 0;
				}
			}
			break;
		case BUILDING_COALMINE:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_MINER,
							      RESOURCE_PICK, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BIT_TEST(building->serf, 6)) {
				int total_food = ((building->stock1 >> 4) & 0xf) + (building->stock1 & 0xf);
				if (total_food < 8) {
					player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
					building->u.flag->stock1_prio = sett->food_coalmine >> (8 + total_food);
				} else {
					building->u.flag->stock1_prio = 0;
				}
			}
			break;
		case BUILDING_IRONMINE:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_MINER,
							      RESOURCE_PICK, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BIT_TEST(building->serf, 6)) {
				int total_food = ((building->stock1 >> 4) & 0xf) + (building->stock1 & 0xf);
				if (total_food < 8) {
					player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
					building->u.flag->stock1_prio = sett->food_ironmine >> (8 + total_food);
				} else {
					building->u.flag->stock1_prio = 0;
				}
			}
			break;
		case BUILDING_GOLDMINE:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_MINER,
							      RESOURCE_PICK, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BIT_TEST(building->serf, 6)) {
				int total_food = ((building->stock1 >> 4) & 0xf) + (building->stock1 & 0xf);
				if (total_food < 8) {
					player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
					building->u.flag->stock1_prio = sett->food_goldmine >> (8 + total_food);
				} else {
					building->u.flag->stock1_prio = 0;
				}
			}
			break;
		case BUILDING_FORESTER:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_FORESTER, -1, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			break;
		case BUILDING_STOCK:
			if (!BIT_TEST(building->serf, 4)) {
				inventory_t *inv;
				int r = game_alloc_inventory(&inv, NULL);
				if (r < 0) return;

				inv->player_num = BUILDING_PLAYER(building);
				inv->bld_index = BUILDING_INDEX(building);
				inv->flg_index = building->flg_index;

				building->u.inventory = inv;
				building->stock1 = 0xffff;
				building->serf |= BIT(4);

				player_add_notification(globals.player_sett[BUILDING_PLAYER(building)],
							7, building->pos);
			} else {
				if ((building->serf & 0xc0) == 0) {
					send_serf_to_building(building, SERF_TRANSPORTER, -1, -1);
				}

				/*player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];*/
				inventory_t *inv = building->u.inventory;
				if (BIT_TEST(building->serf, 6) &&
				    (inv->res_dir & 0xa) == 0 && /* Not serf or res OUT mode */
				    inv->spawn_priority == 0) {
					/* sett->field_160 -= 1; */
					if (0/*sett->field_160 < 0*/) {
						/* sett->field_160 = 5; */
						send_serf_to_building(building, SERF_GENERIC, -1, -1);
					}
				}

				/* sett->field_180 += inv->resources[RESOURCE_GOLDBAR]; */

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
			int max_occ_level = (sett->knight_occupation[building->serf & 3] >> 4) & 0xf;
			if (BIT_TEST(sett->flags, 4)) max_occ_level += 5;

			int needed_occupants = hut_occupants_from_level[max_occ_level];
			int max_gold = 2;

			int total_knights = (building->stock1 & 0xf) + ((building->stock1 >> 4) & 0xf);
			int present_knights = (building->stock1 >> 4) & 0xf;
			if (total_knights < needed_occupants) {
				if (!BIT_TEST(building->serf, 2)) {
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
					}
				}

				/* Update serf state. */
				serf_log_state_change(leaving_serf, SERF_STATE_READY_TO_LEAVE);
				leaving_serf->state = SERF_STATE_READY_TO_LEAVE;
				leaving_serf->s.leaving_building.field_B = -2;
				leaving_serf->s.leaving_building.dest = 0;
				leaving_serf->s.leaving_building.dir = 0;
				leaving_serf->s.leaving_building.next_state = SERF_STATE_WALKING;

				building->stock1 -= 0x10;
			}

			/* Request gold */
			if (BIT_TEST(building->serf, 6)) {
				int total_gold = (building->stock2 & 0xf) + ((building->stock2 >> 4) & 0xf);
				/* TODO Update player sett */
				if (total_gold < max_gold) {
					building->u.flag->stock2_prio = ((0xfe >> total_gold) + 1) & 0xfe;
				} else {
					building->u.flag->stock2_prio = 0;
				}
			}
			break;
		}
		case BUILDING_FARM:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_FARMER, RESOURCE_SCYTHE, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			break;
		case BUILDING_BUTCHER:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_BUTCHER,
							      RESOURCE_CLEAVER, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BIT_TEST(building->serf, 6)) {
				/* Request more of that delicious meat. */
				int total_stock = (building->stock1 & 0xf) + ((building->stock1 >> 4) & 0xf);
				if (total_stock < 8) {
					building->u.flag->stock1_prio = (0xff >> total_stock);
				} else {
					building->u.flag->stock1_prio = 0;
				}
			}
			break;
		case BUILDING_PIGFARM:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_PIGFARMER, -1, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BIT_TEST(building->serf, 6)) {
				/* Request more wheat. */
				int total_stock = (building->stock1 & 0xf) + ((building->stock1 >> 4) & 0xf);
				if (total_stock < 8) {
					player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
					building->u.flag->stock1_prio = sett->wheat_pigfarm >> (8 + total_stock);
				} else {
					building->u.flag->stock1_prio = 0;
				}
			}
			break;
		case BUILDING_MILL:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_MILLER, -1, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BIT_TEST(building->serf, 6)) {
				/* Request more wheat. */
				int total_stock = (building->stock1 & 0xf) + ((building->stock1 >> 4) & 0xf);
				if (total_stock < 8) {
					player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
					building->u.flag->stock1_prio = sett->wheat_mill >> (8 + total_stock);
				} else {
					building->u.flag->stock1_prio = 0;
				}
			}
			break;
		case BUILDING_BAKER:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_BAKER, -1, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BIT_TEST(building->serf, 6)) {
				/* Request more flour. */
				int total_stock = (building->stock1 & 0xf) + ((building->stock1 >> 4) & 0xf);
				if (total_stock < 8) {
					building->u.flag->stock1_prio = 0xff >> total_stock;
				} else {
					building->u.flag->stock1_prio = 0;
				}
			}
			break;
		case BUILDING_SAWMILL:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_SAWMILLER,
							      RESOURCE_SAW, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BIT_TEST(building->serf, 6)) {
				/* Request more lumber */
				int total_stock = (building->stock2 & 0xf) + ((building->stock2 >> 4) & 0xf);
				if (total_stock < 8) {
					building->u.flag->stock2_prio = 0xff >> total_stock;
				} else {
					building->u.flag->stock2_prio = 0;
				}
			}
			break;
		case BUILDING_STEELSMELTER:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_SMELTER,
							      -1, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BIT_TEST(building->serf, 6)) {
				/* Request more coal */
				int total_coal = (building->stock1 & 0xf) + ((building->stock1 >> 4) & 0xf);
				if (total_coal < 8) {
					player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
					building->u.flag->stock1_prio = sett->coal_steelsmelter >> (8 + total_coal);
				} else {
					building->u.flag->stock1_prio = 0;
				}

				/* Request more iron ore */
				int total_ironore = (building->stock2 & 0xf) + ((building->stock2 >> 4) & 0xf);
				if (total_ironore < 8) {
					building->u.flag->stock2_prio = 0xff >> total_ironore;
				} else {
					building->u.flag->stock2_prio = 0;
				}
			}
			break;
		case BUILDING_TOOLMAKER:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_TOOLMAKER,
							      RESOURCE_HAMMER, RESOURCE_SAW);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BIT_TEST(building->serf, 6)) {
				/* Request more planks. */
				player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
				int total_tree = ((building->stock1 >> 4) & 0xf) + (building->stock1 & 0xf);
				if (total_tree < 8 && 1/*!BIT_TEST(sett->field_163, 1)*/) {
					building->u.flag->stock1_prio = sett->planks_toolmaker >> (8 + total_tree);
				} else {
					building->u.flag->stock1_prio = 0;
				}

				/* Request more steel. */
				int total_steel = ((building->stock2 >> 4) & 0xf) + (building->stock2 & 0xf);
				if (total_steel < 8) {
					building->u.flag->stock2_prio = sett->steel_toolmaker >> (8 + total_steel);
				} else {
					building->u.flag->stock2_prio = 0;
				}
			}
			break;
		case BUILDING_WEAPONSMITH:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_WEAPONSMITH,
							      RESOURCE_HAMMER, RESOURCE_PINCER);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BIT_TEST(building->serf, 6)) {
				/* Request more coal. */
				player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
				int total_coal = ((building->stock1 >> 4) & 0xf) + (building->stock1 & 0xf);
				if (total_coal < 8) {
					building->u.flag->stock1_prio = sett->coal_weaponsmith >> (8 + total_coal);
				} else {
					building->u.flag->stock1_prio = 0;
				}

				/* Request more steel. */
				int total_steel = ((building->stock2 >> 4) & 0xf) + (building->stock2 & 0xf);
				if (total_steel < 8) {
					building->u.flag->stock2_prio = sett->steel_weaponsmith >> (8 + total_steel);
				} else {
					building->u.flag->stock2_prio = 0;
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
			int max_occ_level = (sett->knight_occupation[building->serf & 3] >> 4) & 0xf;
			if (BIT_TEST(sett->flags, 4)) max_occ_level += 5;

			int needed_occupants = tower_occupants_from_level[max_occ_level];
			int max_gold = 4;

			int total_knights = (building->stock1 & 0xf) + ((building->stock1 >> 4) & 0xf);
			int present_knights = (building->stock1 >> 4) & 0xf;
			if (total_knights < needed_occupants) {
				if (!BIT_TEST(building->serf, 2)) {
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
					}
				}

				/* Update serf state. */
				serf_log_state_change(leaving_serf, SERF_STATE_READY_TO_LEAVE);
				leaving_serf->state = SERF_STATE_READY_TO_LEAVE;
				leaving_serf->s.leaving_building.field_B = -2;
				leaving_serf->s.leaving_building.dest = 0;
				leaving_serf->s.leaving_building.dir = 0;
				leaving_serf->s.leaving_building.next_state = SERF_STATE_WALKING;

				building->stock1 -= 0x10;
			}

			/* Request gold */
			if (BIT_TEST(building->serf, 6)) {
				int total_gold = (building->stock2 & 0xf) + ((building->stock2 >> 4) & 0xf);
				/* TODO Update player sett */
				if (total_gold < max_gold) {
					building->u.flag->stock2_prio = ((0xfe >> total_gold) + 1) & 0xfe;
				} else {
					building->u.flag->stock2_prio = 0;
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
			int max_occ_level = (sett->knight_occupation[building->serf & 3] >> 4) & 0xf;
			if (BIT_TEST(sett->flags, 4)) max_occ_level += 5;

			int needed_occupants = fortress_occupants_from_level[max_occ_level];
			int max_gold = 8;

			int total_knights = (building->stock1 & 0xf) + ((building->stock1 >> 4) & 0xf);
			int present_knights = (building->stock1 >> 4) & 0xf;
			if (total_knights < needed_occupants) {
				/* Send a knight to this building. */
				if (!BIT_TEST(building->serf, 2)) {
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
					}
				}

				/* Update serf state. */
				serf_log_state_change(leaving_serf, SERF_STATE_READY_TO_LEAVE);
				leaving_serf->state = SERF_STATE_READY_TO_LEAVE;
				leaving_serf->s.leaving_building.field_B = -2;
				leaving_serf->s.leaving_building.dest = 0;
				leaving_serf->s.leaving_building.dir = 0;
				leaving_serf->s.leaving_building.next_state = SERF_STATE_WALKING;

				building->stock1 -= 0x10;
			}

			/* Request gold */
			if (BIT_TEST(building->serf, 6)) {
				int total_gold = (building->stock2 & 0xf) + ((building->stock2 >> 4) & 0xf);
				/* TODO Update player sett */
				if (total_gold < max_gold) {
					building->u.flag->stock2_prio = ((0xfe >> total_gold) + 1) & 0xfe;
				} else {
					building->u.flag->stock2_prio = 0;
				}
			}
			break;
		}
		case BUILDING_GOLDSMELTER:
			if ((building->serf & 0xc4) == 0) {
				int r = send_serf_to_building(building, SERF_SMELTER,
							      -1, -1);
				if (r < 0) building->serf |= BIT(2);
			}
			if (BIT_TEST(building->serf, 6)) {
				/* Request more coal. */
				player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
				int total_coal = ((building->stock1 >> 4) & 0xf) + (building->stock1 & 0xf);
				if (total_coal < 8) {
					building->u.flag->stock1_prio = sett->coal_goldsmelter >> (8 + total_coal);
				} else {
					building->u.flag->stock1_prio = 0;
				}

				/* Request more gold ore. */
				int total_goldore = ((building->stock2 >> 4) & 0xf) + (building->stock2 & 0xf);
				if (total_goldore < 8) {
					building->u.flag->stock2_prio = 0xff >> total_goldore;
				} else {
					building->u.flag->stock2_prio = 0;
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
	map_1_t *map = globals.map_mem2_ptr;
	map_2_t *map_data = MAP_2_DATA(globals.map_mem2_ptr);

	if (globals.next_index >= 32) return;

	int index = globals.next_index << 5;
	for (int i = index ? index : 1; i < globals.max_ever_building_index; i++) {
		if (BIT_TEST(globals.buildings_bitmap[i>>3], 7-(i&7))) {
			building_t *building = game_get_building(i);
			if (BIT_TEST(building->serf, 5)) { /* Building is burning */
				uint16_t delta = globals.anim - building->u.anim;
				building->u.anim = globals.anim;
				if (building->serf_index >= delta) {
					building->serf_index -= delta;
				} else {
					/* 2355E */
					map_pos_t pos = building->pos;
					int p = building->u.s.planks_needed;

					map[pos].flags &= ~BIT(6);
					map[pos].obj &= 0x80;
					map_data[pos].u.index = 0;
					game_free_building(i);

					if ((p & 0x1f) != 0) {
						/* TODO */
					}
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
		if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
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
		if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
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
		if (BIT_TEST(globals.flg_bitmap[i>>3], 7-(i&7))) {
			flag_t *flag = game_get_flag(i);

			for (int i = 0; i < 8; i++) {
				if (flag->res_waiting[i] != 0 &&
				    flag->res_dest[i] == FLAG_INDEX(flag)) {
					flag->res_dest[i] = 0;
					flag->endpoint |= BIT(7);

					if (((flag->res_waiting[i] >> 5) & 3) != 0) {
						dir_t dir = ((flag->res_waiting[i] >> 5) & 3)-1;
						player_sett_t *sett = globals.player_sett[FLAG_PLAYER(flag)];
						flag_prioritize_pickup(flag, dir, sett->flag_prio);
					}
				}
			}
		}
	}

	/* Inventories. */
	for (int i = 0; i < globals.max_ever_inventory_index; i++) {
		if (BIT_TEST(globals.inventories_bitmap[i>>3], 7-(i&7))) {
			inventory_t *inventory = game_get_inventory(i);
			if (inventory->out_dest[1] == FLAG_INDEX(flag)) {
				inventory->out_queue[1] = 0;
			}
			if (inventory->out_dest[0] == FLAG_INDEX(flag)) {
				inventory->out_queue[0] = inventory->out_queue[1];
				inventory->out_dest[0] = inventory->out_dest[1];
				inventory->out_queue[1] = 0;
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
	map_1_t *map = globals.map_mem2_ptr;

	while (1) {
		pos = MAP_MOVE(pos, dir);

		/* Clear backreference */
		map[pos].flags &= ~BIT(DIR_REVERSE(dir));

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
		if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
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
			if (stock_type[res] == 0) building->stock1 -= 1;
			else building->stock2 -= 1;
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

				dir_t other_dir = (flag->other_end_dir[dir] >> 3) & 7;
				flag->other_endpoint.f[dir]->length[other_dir] &= ~BIT(7);
			}
		} else if (serf->s.walking.res == -1) {
			flag_t *flag = game_get_flag(serf->s.walking.dest);
			building_t *building = flag->other_endpoint.b[DIR_UP_LEFT];

			if (BIT_TEST(building->serf, 7)) {
				building->serf &= ~BIT(7);
			} else if (building->stock1 != 0xff) {
				building->stock1 -= 1;
				if (building->stock1 < 0) building->stock1 = 0xff; /* Should probably just be a signed int. */
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

		if (serf->type != SERF_SAILOR) {
			serf_log_state_change(serf, SERF_STATE_LOST);
			serf->state = SERF_STATE_LOST;
			serf->s.lost.field_B = 0;
		} else {
			serf_log_state_change(serf, SERF_STATE_26);
			serf->state = SERF_STATE_26;
		}
	}
}

static void
remove_road_forwards(map_pos_t pos, dir_t dir)
{
	map_1_t *map = globals.map_mem2_ptr;

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
			dir_t rev_dir = DIR_REVERSE(dir);

			flag->path_con &= ~BIT(rev_dir);
			flag->transporter &= ~BIT(rev_dir);
			flag->endpoint &= ~BIT(rev_dir);

			if (BIT_TEST(flag->length[rev_dir], 7)) {
				flag->length[rev_dir] &= ~BIT(7);

				for (int i = 1; i < globals.max_ever_serf_index; i++) {
					if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
						int dest = MAP_OBJ_INDEX(pos);
						serf_t *serf = game_get_serf(i);

						switch (serf->state) {
						case SERF_STATE_WALKING:
							if (serf->s.walking.dest == dest &&
							    serf->s.walking.res == rev_dir) {
								serf->s.walking.res = 0xfe;
								serf->s.walking.dest = 0;
							}
							break;
						case SERF_STATE_READY_TO_LEAVE_INVENTORY:
							if (serf->s.ready_to_leave_inventory.dest == dest &&
							    serf->s.ready_to_leave_inventory.mode == rev_dir) {
								serf->s.ready_to_leave_inventory.dest = 0xfe;
								serf->s.ready_to_leave_inventory.dest = 0;
							}
							break;
						case SERF_STATE_LEAVING_BUILDING:
						case SERF_STATE_READY_TO_LEAVE:
							if (serf->s.leaving_building.dest == dest &&
							    serf->s.leaving_building.field_B == rev_dir &&
							    serf->s.leaving_building.next_state == SERF_STATE_WALKING) {
								serf->s.leaving_building.field_B = 0xfe;
								serf->s.leaving_building.dest = 0;
							}
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
		map[pos].flags &= ~BIT(dir);
		pos = MAP_MOVE(pos, dir);

		/* Clear backreference. */
		map[pos].flags &= ~BIT(DIR_REVERSE(dir));

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

/* Demolish building at pos. */
void
game_demolish_building(map_pos_t pos)
{
	/* request redraw at pos */

	building_t *building = game_get_building(MAP_OBJ_INDEX(pos));
	building_remove_pl_sett_refs(building);

	player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
	map_1_t *map = globals.map_mem2_ptr;

	if (BIT_TEST(building->serf, 5)) return; /* Already burning */

	building->serf |= BIT(5);

	/* Remove path to building. */
	map[pos].flags &= ~BIT(1);
	map[MAP_MOVE_DOWN_RIGHT(pos)].flags &= ~BIT(4);

	/* Remove lost gold stock from total count. */
	if (BUILDING_IS_DONE(building) &&
	    (BUILDING_TYPE(building) == BUILDING_HUT ||
	     BUILDING_TYPE(building) == BUILDING_TOWER ||
	     BUILDING_TYPE(building) == BUILDING_FORTRESS ||
	     BUILDING_TYPE(building) == BUILDING_GOLDSMELTER)) {
		int gold_stock = (building->stock2 >> 4) & 0xf;
		globals.map_gold_deposit -= gold_stock;
	}

	/* Update land owner ship if the building is military. */
	if (BUILDING_IS_DONE(building) &&
	    (BUILDING_TYPE(building) == BUILDING_HUT ||
	     BUILDING_TYPE(building) == BUILDING_TOWER ||
	     BUILDING_TYPE(building) == BUILDING_FORTRESS ||
	     BUILDING_TYPE(building) == BUILDING_CASTLE)) {
		update_land_ownership(MAP_COORD_ARGS(building->pos));
	}

	if (BUILDING_IS_DONE(building) &&
	    (BUILDING_TYPE(building) == BUILDING_CASTLE ||
	     BUILDING_TYPE(building) == BUILDING_STOCK)) {
		/* Cancel resources in the out queue and remove gold
		   from map total. */
		if (BIT_TEST(building->serf, 4)) {
			inventory_t *inventory = building->u.inventory;

			for (int i = 0; i < 2 && inventory->out_queue[i] != 0; i++) {
				int res = inventory->out_queue[i] - 1;
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
			if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
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
	building->stock1 = 0;
	building->stock2 = 0;

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

	if (BIT_TEST(building->serf, 6)) {
		building->serf &= ~BIT(6);

		if (BUILDING_IS_DONE(building) &&
		    BUILDING_TYPE(building) == BUILDING_CASTLE) {
			sett->build &= ~BIT(3);
			/* sett->field_15E -= 1; */

			building->serf_index = 8191;

			if (sett->serf_index != 0) {
				serf_t *serf = game_get_serf(sett->serf_index);
				serf->type = (0x83 & serf->type) | SERF_TRANSPORTER;
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
				serf->type = (0x83 & serf->type) | SERF_TRANSPORTER;
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

	if (MAP_PATHS(MAP_MOVE_DOWN_RIGHT(pos)) == 0 &&
	    MAP_OBJ(MAP_MOVE_DOWN_RIGHT(pos)) == MAP_OBJ_FLAG) {
		/* TODO */
	}
}
