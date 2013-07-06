/*
 * savegame.c - Loading and saving of save games
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
#include <string.h>

#include "game.h"
#include "map.h"
#include "version.h"
#include "list.h"
#include "debug.h"


typedef struct {
	uint rows, cols;
	uint row_shift;
} v0_map_t;

static map_pos_t
load_v0_map_pos(const v0_map_t *map, uint32_t value)
{
	return MAP_POS((value >> 2) & (map->cols-1),
		       (value >> (2 + map->row_shift)) & (map->rows-1));
}

/* Load main game state from save game. */
static int
load_v0_game_state(FILE *f, v0_map_t *map)
{
	uint8_t *data = malloc(210);
	if (data == NULL) return -1;

	size_t rd = fread(data, sizeof(uint8_t), 210, f);
	if (rd < 210) {
		free(data);
		return -1;
	}

	/* Load these first so map dimensions can be reconstructed.
	   This is necessary to load map positions. */
	game.map_size = *(uint16_t *)&data[190];

	map->row_shift = *(uint16_t *)&data[42];
	map->cols = *(uint16_t *)&data[62];
	map->rows = *(uint16_t *)&data[64];

	/* Init the rest of map dimensions. */
	game.map.col_size = 5 + game.map_size/2;
	game.map.row_size = 5 + (game.map_size - 1)/2;
	game.map.cols = 1 << game.map.col_size;
	game.map.rows = 1 << game.map.row_size;
	map_init_dimensions(&game.map);

	/* Allocate game objects */
	const int max_map_size = 10;
	game.serf_limit = (0x1f84 * (1 << max_map_size) - 4) / 0x81;
	game.flag_limit = (0x2314 * (1 << max_map_size) - 4) / 0x231;
	game.building_limit = (0x54c * (1 << max_map_size) - 4) / 0x91;
	game.inventory_limit = (0x54c * (1 << max_map_size) - 4) / 0x3c1;
	game_allocate_objects();

	/* OBSOLETE may be needed to load map data correctly?
	map->index_mask = *(uint32_t *)&data[0] >> 2;

	game.map_dirs[DIR_RIGHT] = *(uint32_t *)&data[4] >> 2;
	game.map_dirs[DIR_DOWN_RIGHT] = *(uint32_t *)&data[8] >> 2;
	game.map_dirs[DIR_DOWN] = *(uint32_t *)&data[12] >> 2;
	game.map_move_left_2 = *(uint16_t *)&data[16] >> 2;
	game.map_dirs[DIR_UP_LEFT] = *(uint32_t *)&data[18] >> 2;
	game.map_dirs[DIR_UP] = *(uint32_t *)&data[22] >> 2;
	game.map_dirs[DIR_UP_RIGHT] = *(uint32_t *)&data[26] >> 2;
	game.map_dirs[DIR_DOWN_LEFT] = *(uint32_t *)&data[30] >> 2;

	game.map_col_size = *(uint32_t *)&data[34] >> 2;
	game.map_elms = *(uint32_t *)&data[38];
	game.map_row_shift = *(uint16_t *)&data[42];
	game.map_col_mask = *(uint16_t *)&data[44];
	game.map_row_mask = *(uint16_t *)&data[46];
	game.map_data_offset = *(uint32_t *)&data[48] >> 2;
	game.map_shifted_col_mask = *(uint16_t *)&data[52] >> 2;
	game.map_shifted_row_mask = *(uint32_t *)&data[54] >> 2;
	game.map_col_pairs = *(uint16_t *)&data[58];
	game.map_row_pairs = *(uint16_t *)&data[60];*/

	game.split = *(uint8_t *)&data[66];
	/* game.field_37F = *(uint8_t *)&data[67]; */
	game.update_map_initial_pos = load_v0_map_pos(map, *(uint32_t *)&data[68]);

	game.game_type = *(uint16_t *)&data[74];
	game.tick = *(uint32_t *)&data[76];
	game.game_stats_counter = 0;
	game.history_counter = 0;

	game.rnd.state[0] = *(uint16_t *)&data[84];
	game.rnd.state[1] = *(uint16_t *)&data[86];
	game.rnd.state[2] = *(uint16_t *)&data[88];

	game.max_flag_index = *(uint16_t *)&data[90];
	game.max_building_index = *(uint16_t *)&data[92];
	game.max_serf_index = *(uint16_t *)&data[94];

	game.next_index = *(uint16_t *)&data[96];
	game.flag_search_counter = *(uint16_t *)&data[98];
	game.update_map_last_tick = *(uint16_t *)&data[100];
	game.update_map_counter = *(uint16_t *)&data[102];

	for (int i = 0; i < 4; i++) {
		game.player_history_index[i] = *(uint16_t *)&data[104 + i*2];
	}

	for (int i = 0; i < 3; i++) {
		game.player_history_counter[i] = *(uint16_t *)&data[112 + i*2];
	}

	game.resource_history_index = *(uint16_t *)&data[118];

	game.map_regions = *(uint16_t *)&data[120];

	if (0/*game.game_type == GAME_TYPE_TUTORIAL*/) {
		game.tutorial_level = *(uint16_t *)&data[122];
	} else if (0/*game.game_type == GAME_TYPE_MISSION*/) {
		game.mission_level = *(uint16_t *)&data[124];
		/*game.max_mission_level = *(uint16_t *)&data[126];*/
		/* memcpy(game.mission_code, &data[128], 8); */
	} else if (1/*game.game_type == GAME_TYPE_1_PLAYER*/) {
		/*game.menu_map_size = *(uint16_t *)&data[136];*/
		/*game.rnd_init_1 = *(uint16_t *)&data[138];
		game.rnd_init_2 = *(uint16_t *)&data[140];
		game.rnd_init_3 = *(uint16_t *)&data[142];*/

		/*
		memcpy(game.menu_ai_face, &data[144], 4);
		memcpy(game.menu_ai_intelligence, &data[148], 4);
		memcpy(game.menu_ai_supplies, &data[152], 4);
		memcpy(game.menu_ai_reproduction, &data[156], 4);
		*/

		/*
		memcpy(game.menu_human_supplies, &data[160], 2);
		memcpy(game.menu_human_reproduction, &data[162], 2);
		*/
	}

	/*
	game.saved_pl1_map_cursor_col = *(uint16_t *)&data[164];
	game.saved_pl1_map_cursor_row = *(uint16_t *)&data[166];

	game.saved_pl1_pl_sett_100 = *(uint8_t *)&data[168];
	game.saved_pl1_pl_sett_101 = *(uint8_t *)&data[169];
	game.saved_pl1_pl_sett_102 = *(uint16_t *)&data[170];

	game.saved_pl1_build = *(uint8_t *)&data[172];
	game.field_17B = *(uint8_t *)&data[173];
	*/

	game.max_inventory_index = *(uint16_t *)&data[174];
	/*game.map_max_serfs_left = *(uint16_t *)&data[176];*/
	/* game.max_stock_buildings = *(uint16_t *)&data[178]; */
	game.max_next_index = *(uint16_t *)&data[180];
	game.max_serfs_from_land = *(uint16_t *)&data[182];
	game.map_gold_deposit = *(uint32_t *)&data[184];
	game.update_map_16_loop = *(uint16_t *)&data[188];

	/*
	game.map_field_52 = *(uint16_t *)&data[192];
	game.field_54 = *(uint16_t *)&data[194];
	game.field_56 = *(uint16_t *)&data[196];
	*/

	game.max_serfs_per_player = *(uint16_t *)&data[198];
	game.map_gold_morale_factor = *(uint16_t *)&data[200];
	game.winning_player = *(uint16_t *)&data[202];
	game.player_score_leader = *(uint8_t *)&data[204];
	/*
	game.show_game_end_box = *(uint8_t *)&data[205];
	*/

	/*game.map_dirs[DIR_LEFT] = *(uint32_t *)&data[206] >> 2;*/

	free(data);

	/* Skip unused section. */
	int r = fseek(f, 40, SEEK_CUR);
	if (r < 0) return -1;

	return 0;
}

/* Load player state from save game. */
static int
load_v0_player_state(FILE *f)
{
	const int default_player_colors[] = {
		64, 72, 68, 76
	};

	uint8_t *data = malloc(8628);
	if (data == NULL) return -1;

	for (int i = 0; i < 4; i++) {
		size_t rd = fread(data, sizeof(uint8_t), 8628, f);
		if (rd < 8628) {
			free(data);
			return -1;
		}

		if (!BIT_TEST(data[130], 6)) continue;;

		game.player[i] = calloc(1, sizeof(player_t));
		if (game.player[i] == NULL) {
			free(data);
			return -1;
		}

		player_t *player = game.player[i];

		for (int j = 0; j < 9; j++) {
			player->tool_prio[j] = *(uint16_t *)&data[2*j];
		}

		for (int j = 0; j < 26; j++) {
			player->resource_count[j] = data[18+j];
			player->flag_prio[j] = data[44+j];
		}

		for (int j = 0; j < 27; j++) {
			player->serf_count[j] = *(uint16_t *)&data[70+2*j];
		}

		for (int j = 0; j < 4; j++) {
			player->knight_occupation[j] = data[124+j];
		}

		player->player_num = *(uint16_t *)&data[128];
		player->color = default_player_colors[i];
		player->flags = data[130];
		player->build = data[131];

		for (int j = 0; j < 23; j++) {
			player->completed_building_count[j] = *(uint16_t *)&data[132+2*j];
			player->incomplete_building_count[j] = *(uint16_t *)&data[178+2*j];
		}

		for (int j = 0; j < 26; j++) {
			player->inventory_prio[j] = data[224+j];
		}

		for (int j = 0; j < 64; j++) {
			player->attacking_buildings[j] = *(uint16_t *)&data[250+2*j];
		}

		player->current_sett_5_item = *(uint16_t *)&data[378];
		player->building = *(uint16_t *)&data[388];

		player->castle_flag = *(uint16_t *)&data[390];
		player->castle_inventory = *(uint16_t *)&data[392];

		player->cont_search_after_non_optimal_find = *(uint16_t *)&data[394];
		player->knights_to_spawn = *(uint16_t *)&data[396];
		/*player->field_110 = *(uint16_t *)&data[400];*/

		player->total_building_score = *(uint32_t *)&data[406];
		player->total_military_score = *(uint32_t *)&data[410];

		player->last_tick = *(uint16_t *)&data[414];

		player->reproduction_counter = *(uint16_t *)&data[416];
		player->reproduction_reset = *(uint16_t *)&data[418];
		player->serf_to_knight_rate = *(uint16_t *)&data[420];
		player->serf_to_knight_counter = *(uint16_t *)&data[422];

		player->attacking_building_count = *(uint16_t *)&data[424];
		for (int j = 0; j < 4; j++) {
			player->attacking_knights[j] = *(uint16_t *)&data[426+2*j];
		}
		player->total_attacking_knights = *(uint16_t *)&data[434];
		player->building_attacked = *(uint16_t *)&data[436];
		player->knights_attacking = *(uint16_t *)&data[438];

		player->analysis_goldore = *(uint16_t *)&data[440];
		player->analysis_ironore = *(uint16_t *)&data[442];
		player->analysis_coal = *(uint16_t *)&data[444];
		player->analysis_stone = *(uint16_t *)&data[446];

		player->food_stonemine = *(uint16_t *)&data[448];
		player->food_coalmine = *(uint16_t *)&data[450];
		player->food_ironmine = *(uint16_t *)&data[452];
		player->food_goldmine = *(uint16_t *)&data[454];

		player->planks_construction = *(uint16_t *)&data[456];
		player->planks_boatbuilder = *(uint16_t *)&data[458];
		player->planks_toolmaker = *(uint16_t *)&data[460];

		player->steel_toolmaker = *(uint16_t *)&data[462];
		player->steel_weaponsmith = *(uint16_t *)&data[464];

		player->coal_steelsmelter = *(uint16_t *)&data[466];
		player->coal_goldsmelter = *(uint16_t *)&data[468];
		player->coal_weaponsmith = *(uint16_t *)&data[470];

		player->wheat_pigfarm = *(uint16_t *)&data[472];
		player->wheat_mill = *(uint16_t *)&data[474];

		player->current_sett_6_item = *(uint16_t *)&data[476];

		player->castle_score = *(int16_t *)&data[478];

		/* TODO */
	}

	free(data);

	return 0;
}

/* Load map state from save game. */
static int
load_v0_map_state(FILE *f, const v0_map_t *map)
{
	uint tile_count = map->cols*map->rows;

	uint8_t *data = malloc(8*tile_count);
	if (data == NULL) return -1;

	size_t rd = fread(data, sizeof(uint8_t), 8*tile_count, f);
	if (rd < 8*tile_count) {
		free(data);
		return -1;
	}

	map_tile_t *tiles = game.map.tiles;

	for (int y = 0; y < game.map.rows; y++) {
		for (int x = 0; x < game.map.cols; x++) {
			map_pos_t pos = MAP_POS(x, y);
			uint8_t *field_1_data = &data[4*(x + (y << map->row_shift))];
			uint8_t *field_2_data = &data[4*(x + (y << map->row_shift)) + 4*map->cols];

			tiles[pos].paths = field_1_data[0] & 0x3f;
			tiles[pos].height = field_1_data[1] & 0x1f;
			tiles[pos].type = field_1_data[2];
			tiles[pos].obj = field_1_data[3] & 0x7f;

			if (MAP_OBJ(pos) >= MAP_OBJ_FLAG &&
			    MAP_OBJ(pos) <= MAP_OBJ_CASTLE) {
				tiles[pos].resource = 0;
				tiles[pos].obj_index = *(uint16_t *)&field_2_data[0];
			} else {
				tiles[pos].resource = field_2_data[0];
				tiles[pos].obj_index = 0;
			}

			tiles[pos].serf = *(uint16_t *)&field_2_data[2];
		}
	}

	free(data);

	return 0;
}

/* Load serf state from save game. */
static int
load_v0_serf_state(FILE *f, const v0_map_t *map)
{
	/* Load serf bitmap. */
	int bitmap_size = 4*((game.max_serf_index + 31)/32);
	uint8_t *bitmap = malloc(bitmap_size);
	if (bitmap == NULL) return -1;

	size_t rd = fread(bitmap, sizeof(uint8_t), bitmap_size, f);
	if (rd < bitmap_size) {
		free(bitmap);
		return -1;
	}

	memset(game.serf_bitmap, '\0', (game.serf_limit+31)/32);
	memcpy(game.serf_bitmap, bitmap, bitmap_size);

	free(bitmap);

	/* Load serf data. */
	uint8_t *data = malloc(16*game.max_serf_index);
	if (data == NULL) return -1;

	rd = fread(data, 16*sizeof(uint8_t), game.max_serf_index, f);
	if (rd < game.max_serf_index) {
		free(data);
		return -1;
	}

	for (int i = 0; i < game.max_serf_index; i++) {
		uint8_t *serf_data = &data[16*i];
		serf_t *serf = &game.serfs[i];

		serf->type = serf_data[0];
		serf->animation = serf_data[1];
		serf->counter = *(uint16_t *)&serf_data[2];
		serf->pos = load_v0_map_pos(map, *(uint32_t *)&serf_data[4]);
		serf->tick = *(uint16_t *)&serf_data[8];
		serf->state = serf_data[10];

		LOGV("savegame", "load serf %i: %s", i, serf_get_state_name(serf->state));

		switch (serf->state) {
		case SERF_STATE_IDLE_IN_STOCK:
			serf->s.idle_in_stock.inv_index = *(uint16_t *)&serf_data[14];
			break;

		case SERF_STATE_WALKING:
		case SERF_STATE_TRANSPORTING:
		case SERF_STATE_DELIVERING:
			serf->s.walking.res = *(int8_t *)&serf_data[11];
			serf->s.walking.dest = *(uint16_t *)&serf_data[12];
			serf->s.walking.dir = *(int8_t *)&serf_data[14];
			serf->s.walking.wait_counter = *(int8_t *)&serf_data[15];
			break;

		case SERF_STATE_ENTERING_BUILDING:
			serf->s.entering_building.field_B = *(int8_t *)&serf_data[11];
			serf->s.entering_building.slope_len = *(uint16_t *)&serf_data[12];
			break;

		case SERF_STATE_LEAVING_BUILDING:
		case SERF_STATE_READY_TO_LEAVE:
			serf->s.leaving_building.field_B = *(int8_t *)&serf_data[11];
			serf->s.leaving_building.dest = *(int8_t *)&serf_data[12];
			serf->s.leaving_building.dest2 = *(int8_t *)&serf_data[13];
			serf->s.leaving_building.dir = *(int8_t *)&serf_data[14];
			serf->s.leaving_building.next_state = serf_data[15];
			break;

		case SERF_STATE_READY_TO_ENTER:
			serf->s.ready_to_enter.field_B = *(int8_t *)&serf_data[11];
			break;

		case SERF_STATE_DIGGING:
			serf->s.digging.h_index = *(int8_t *)&serf_data[11];
			serf->s.digging.target_h = serf_data[12];
			serf->s.digging.dig_pos = *(int8_t *)&serf_data[13];
			serf->s.digging.substate = *(int8_t *)&serf_data[14];
			break;

		case SERF_STATE_BUILDING:
			serf->s.building.mode = *(int8_t *)&serf_data[11];
			serf->s.building.bld_index = *(uint16_t *)&serf_data[12];
			serf->s.building.material_step = serf_data[14];
			serf->s.building.counter = serf_data[15];
			break;

		case SERF_STATE_BUILDING_CASTLE:
			serf->s.building_castle.inv_index = *(uint16_t *)&serf_data[12];
			break;

		case SERF_STATE_MOVE_RESOURCE_OUT:
		case SERF_STATE_DROP_RESOURCE_OUT:
			serf->s.move_resource_out.res = serf_data[11];
			serf->s.move_resource_out.res_dest = *(uint16_t *)&serf_data[12];
			serf->s.move_resource_out.next_state = serf_data[15];
			break;

		case SERF_STATE_READY_TO_LEAVE_INVENTORY:
			serf->s.ready_to_leave_inventory.mode = *(int8_t *)&serf_data[11];
			serf->s.ready_to_leave_inventory.dest = *(uint16_t *)&serf_data[12];
			serf->s.ready_to_leave_inventory.inv_index = *(uint16_t *)&serf_data[14];
			break;

		case SERF_STATE_FREE_WALKING:
		case SERF_STATE_LOGGING:
		case SERF_STATE_PLANTING:
		case SERF_STATE_STONECUTTING:
		case SERF_STATE_STONECUTTER_FREE_WALKING:
		case SERF_STATE_FISHING:
		case SERF_STATE_FARMING:
		case SERF_STATE_SAMPLING_GEO_SPOT:
			serf->s.free_walking.dist1 = *(int8_t *)&serf_data[11];
			serf->s.free_walking.dist2 = *(int8_t *)&serf_data[12];
			serf->s.free_walking.neg_dist1 = *(int8_t *)&serf_data[13];
			serf->s.free_walking.neg_dist2 = *(int8_t *)&serf_data[14];
			serf->s.free_walking.flags = *(int8_t *)&serf_data[15];
			break;

		case SERF_STATE_SAWING:
			serf->s.sawing.mode = *(int8_t *)&serf_data[11];
			break;

		case SERF_STATE_LOST:
			serf->s.lost.field_B = *(int8_t *)&serf_data[11];
			break;

		case SERF_STATE_MINING:
			serf->s.mining.substate = serf_data[11];
			serf->s.mining.res = serf_data[13];
			serf->s.mining.deposit = serf_data[14];
			break;

		case SERF_STATE_SMELTING:
			serf->s.smelting.mode = *(int8_t *)&serf_data[11];
			serf->s.smelting.counter = *(int8_t *)&serf_data[12];
			serf->s.smelting.type = serf_data[13];
			break;

		case SERF_STATE_MILLING:
			serf->s.milling.mode = *(int8_t *)&serf_data[11];
			break;

		case SERF_STATE_BAKING:
			serf->s.baking.mode = *(int8_t *)&serf_data[11];
			break;

		case SERF_STATE_PIGFARMING:
			serf->s.pigfarming.mode = *(int8_t *)&serf_data[11];
			break;

		case SERF_STATE_BUTCHERING:
			serf->s.butchering.mode = *(int8_t *)&serf_data[11];
			break;

		case SERF_STATE_MAKING_WEAPON:
			serf->s.making_weapon.mode = *(int8_t *)&serf_data[11];
			break;

		case SERF_STATE_MAKING_TOOL:
			serf->s.making_tool.mode = *(int8_t *)&serf_data[11];
			break;

		case SERF_STATE_BUILDING_BOAT:
			serf->s.building_boat.mode = *(int8_t *)&serf_data[11];
			break;

		case SERF_STATE_KNIGHT_DEFENDING_VICTORY_FREE:
			/* TODO This will be tricky to load since the
			   function of this state has been changed to one
			   that is driven by the attacking serf instead
			   (SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE). */
			break;

		case SERF_STATE_IDLE_ON_PATH:
		case SERF_STATE_WAIT_IDLE_ON_PATH:
		case SERF_STATE_WAKE_AT_FLAG:
		case SERF_STATE_WAKE_ON_PATH:
			serf->s.idle_on_path.rev_dir = *(int8_t *)&serf_data[11];
			serf->s.idle_on_path.flag = &game.flags[*(uint32_t *)&serf_data[12]/70];
			serf->s.idle_on_path.field_E = serf_data[14];
			break;

		case SERF_STATE_DEFENDING_HUT:
		case SERF_STATE_DEFENDING_TOWER:
		case SERF_STATE_DEFENDING_FORTRESS:
		case SERF_STATE_DEFENDING_CASTLE:
			serf->s.defending.next_knight = *(uint16_t *)&serf_data[14];
			break;

		default: break;
		}
	}

	free(data);

	return 0;
}

/* Load flag state from save game. */
static int
load_v0_flag_state(FILE *f)
{
	/* Load flag bitmap. */
	int bitmap_size = 4*((game.max_flag_index + 31)/32);
	uint8_t *flag_bitmap = malloc(bitmap_size);
	if (flag_bitmap == NULL) return -1;

	size_t rd = fread(flag_bitmap, sizeof(uint8_t), bitmap_size, f);
	if (rd < bitmap_size) {
		free(flag_bitmap);
		return -1;
	}

	memset(game.flag_bitmap, '\0', (game.flag_limit+31)/32);
	memcpy(game.flag_bitmap, flag_bitmap, bitmap_size);

	free(flag_bitmap);

	/* Load flag data. */
	uint8_t *data = malloc(70*game.max_flag_index);
	if (data == NULL) return -1;

	rd = fread(data, 70*sizeof(uint8_t), game.max_flag_index, f);
	if (rd < game.max_flag_index) {
		free(data);
		return -1;
	}

	for (int i = 0; i < game.max_flag_index; i++) {
		uint8_t *flag_data = &data[70*i];
		flag_t *flag = &game.flags[i];

		flag->pos = MAP_POS(0, 0); /* Set correctly later. */
		flag->search_num = *(uint16_t *)&flag_data[0];
		flag->search_dir = flag_data[2];
		flag->path_con = flag_data[3];
		flag->endpoint = flag_data[4];
		flag->transporter = flag_data[5];

		for (int j = 0; j < 6; j++) {
			flag->length[j] = flag_data[6+j];
		}

		for (int j = 0; j < 8; j++) {
			flag->slot[j].type = (flag_data[12+j] & 0x1f)-1;
			flag->slot[j].dir = ((flag_data[12+j] >> 5) & 7)-1;
			flag->slot[j].dest = *(uint16_t *)&flag_data[20+2*j];
		}

		for (int j = 0; j < 6; j++) {
			int offset = *(uint32_t *)&flag_data[36+4*j];
			flag->other_endpoint.f[j] = &game.flags[offset/70];

			/* Other endpoint could be a building in direction up left. */
			if (j == 4 && FLAG_HAS_BUILDING(flag)) {
				flag->other_endpoint.b[j] = &game.buildings[offset/18];
			}

			flag->other_end_dir[j] = flag_data[60+j];
		}

		if (FLAG_HAS_BUILDING(flag)) {
			flag->other_endpoint.b[DIR_UP_LEFT]->stock[0].prio = flag_data[67];
			flag->other_endpoint.b[DIR_UP_LEFT]->stock[1].prio = flag_data[69];
		}

		flag->bld_flags = flag_data[66];
		flag->bld2_flags = flag_data[68];
	}

	free(data);

	/* Set flag positions. */
	for (int y = 0; y < game.map.rows; y++) {
		for (int x = 0; x < game.map.cols; x++) {
			map_pos_t pos = MAP_POS(x, y);
			if (MAP_OBJ(pos) == MAP_OBJ_FLAG) {
				flag_t *flag = game_get_flag(MAP_OBJ_INDEX(pos));
				flag->pos = pos;
			}
		}
	}

	return 0;
}

/* Load building state from save game. */
static int
load_v0_building_state(FILE *f, const v0_map_t *map)
{
	/* Load building bitmap. */
	int bitmap_size = 4*((game.max_building_index + 31)/32);
	uint8_t *bitmap = malloc(bitmap_size);
	if (bitmap == NULL) return -1;

	size_t rd = fread(bitmap, sizeof(uint8_t), bitmap_size, f);
	if (rd < bitmap_size) {
		free(bitmap);
		return -1;
	}

	memset(game.building_bitmap, '\0', (game.building_limit+31)/32);
	memcpy(game.building_bitmap, bitmap, bitmap_size);

	free(bitmap);

	/* Load building data. */
	uint8_t *data = malloc(18*game.max_building_index);
	if (data == NULL) return -1;

	rd = fread(data, 18*sizeof(uint8_t), game.max_building_index, f);
	if (rd < game.max_building_index) {
		free(data);
		return -1;
	}

	for (int i = 0; i < game.max_building_index; i++) {
		uint8_t *building_data = &data[18*i];
		building_t *building = &game.buildings[i];

		building->pos = load_v0_map_pos(map, *(uint32_t *)&building_data[0]);
		building->type = (building_data[4] >> 2) & 0x1f;
		building->bld = building_data[4] & 0x83;
		building->serf = building_data[5];
		building->flag = *(uint16_t *)&building_data[6];

		building->stock[0].type = RESOURCE_NONE;
		building->stock[0].available = (building_data[8] >> 4) & 0xf;
		building->stock[0].requested = building_data[8] & 0xf;

		building->stock[1].type = RESOURCE_NONE;
		building->stock[1].available = (building_data[9] >> 4) & 0xf;
		building->stock[1].requested = building_data[9] & 0xf;

		building->serf_index = *(uint16_t *)&building_data[10];
		building->progress = *(uint16_t *)&building_data[12];

		if (!BUILDING_IS_BURNING(building) &&
		    (BUILDING_IS_DONE(building) ||
		     BUILDING_TYPE(building) == BUILDING_CASTLE)) {
			int offset = *(uint32_t *)&building_data[14];
			if (BUILDING_TYPE(building) == BUILDING_STOCK ||
			    BUILDING_TYPE(building) == BUILDING_CASTLE) {
				building->u.inventory = &game.inventories[offset/120];
				building->stock[0].requested = 0xff;
			}
		} else {
			building->u.level = *(uint16_t *)&building_data[14];
		}

		if (!BUILDING_IS_DONE(building)) {
			building->stock[0].type = RESOURCE_PLANK;
			building->stock[0].maximum = building_data[16];
			building->stock[1].type = RESOURCE_STONE;
			building->stock[1].maximum = building_data[17];
		} else if (BUILDING_HAS_SERF(building)) {
			switch (BUILDING_TYPE(building)) {
			case BUILDING_BOATBUILDER:
				building->stock[0].type = RESOURCE_PLANK;
				building->stock[0].maximum = 8;
				break;
			case BUILDING_STONEMINE:
			case BUILDING_COALMINE:
			case BUILDING_IRONMINE:
			case BUILDING_GOLDMINE:
				building->stock[0].type = RESOURCE_GROUP_FOOD;
				building->stock[0].maximum = 8;
				break;
			case BUILDING_HUT:
				building->stock[1].type = RESOURCE_GOLDBAR;
				building->stock[1].maximum = 2;
				break;
			case BUILDING_TOWER:
				building->stock[1].type = RESOURCE_GOLDBAR;
				building->stock[1].maximum = 4;
				break;
			case BUILDING_FORTRESS:
				building->stock[1].type = RESOURCE_GOLDBAR;
				building->stock[1].maximum = 8;
				break;
			case BUILDING_BUTCHER:
				building->stock[0].type = RESOURCE_PIG;
				building->stock[0].maximum = 8;
				break;
			case BUILDING_PIGFARM:
				building->stock[0].type = RESOURCE_WHEAT;
				building->stock[0].maximum = 8;
				break;
			case BUILDING_MILL:
				building->stock[0].type = RESOURCE_WHEAT;
				building->stock[0].maximum = 8;
				break;
			case BUILDING_BAKER:
				building->stock[0].type = RESOURCE_FLOUR;
				building->stock[0].maximum = 8;
				break;
			case BUILDING_SAWMILL:
				building->stock[1].type = RESOURCE_LUMBER;
				building->stock[1].maximum = 8;
				break;
			case BUILDING_STEELSMELTER:
				building->stock[0].type = RESOURCE_COAL;
				building->stock[0].maximum = 8;
				building->stock[1].type = RESOURCE_IRONORE;
				building->stock[1].maximum = 8;
				break;
			case BUILDING_TOOLMAKER:
				building->stock[0].type = RESOURCE_PLANK;
				building->stock[0].maximum = 8;
				building->stock[1].type = RESOURCE_STEEL;
				building->stock[1].maximum = 8;
				break;
			case BUILDING_WEAPONSMITH:
				building->stock[0].type = RESOURCE_COAL;
				building->stock[0].maximum = 8;
				building->stock[1].type = RESOURCE_STEEL;
				building->stock[1].maximum = 8;
				break;
			case BUILDING_GOLDSMELTER:
				building->stock[0].type = RESOURCE_COAL;
				building->stock[0].maximum = 8;
				building->stock[1].type = RESOURCE_GOLDORE;
				building->stock[1].maximum = 8;
				break;
			default:
				break;
			}
		}
	}

	free(data);

	return 0;
}

/* Load inventory state from save game. */
static int
load_v0_inventory_state(FILE *f)
{
	/* Load inventory bitmap. */
	int bitmap_size = 4*((game.max_inventory_index + 31)/32);
	uint8_t *bitmap = malloc(bitmap_size);
	if (bitmap == NULL) return -1;

	size_t rd = fread(bitmap, sizeof(uint8_t), bitmap_size, f);
	if (rd < bitmap_size) {
		free(bitmap);
		return -1;
	}

	memset(game.inventory_bitmap, '\0', (game.inventory_limit+31)/32);
	memcpy(game.inventory_bitmap, bitmap, bitmap_size);

	free(bitmap);

	/* Load inventory data. */
	uint8_t *data = malloc(120*game.max_inventory_index);
	if (data == NULL) return -1;

	rd = fread(data, 120*sizeof(uint8_t), game.max_inventory_index, f);
	if (rd < game.max_inventory_index) {
		free(data);
		return -1;
	}

	for (int i = 0; i < game.max_inventory_index; i++) {
		uint8_t *inventory_data = &data[120*i];
		inventory_t *inventory = &game.inventories[i];

		inventory->player_num = inventory_data[0];
		inventory->res_dir = inventory_data[1];
		inventory->flag = *(uint16_t *)&inventory_data[2];
		inventory->building = *(uint16_t *)&inventory_data[4];

		for (int j = 0; j < 26; j++) {
			inventory->resources[j] = *(uint16_t *)&inventory_data[6+2*j];
		}

		for (int j = 0; j < 2; j++) {
			inventory->out_queue[j].type = inventory_data[58+j]-1;
			inventory->out_queue[j].dest = *(uint16_t *)&inventory_data[60+2*j];
		}

		inventory->spawn_priority = *(uint16_t *)&inventory_data[64];

		for (int j = 0; j < 27; j++) {
			inventory->serfs[j] = *(uint16_t *)&inventory_data[66+2*j];
		}
	}

	free(data);

	return 0;
}

/* Load a save game. */
int
load_v0_state(FILE *f)
{
	int r;
	v0_map_t map;

	r = load_v0_game_state(f, &map);
	if (r < 0) return -1;

	r = load_v0_player_state(f);
	if (r < 0) return -1;

	r = load_v0_map_state(f, &map);
	if (r < 0) return -1;

	r = load_v0_serf_state(f, &map);
	if (r < 0) return -1;

	r = load_v0_flag_state(f);
	if (r < 0) return -1;

	r = load_v0_building_state(f, &map);
	if (r < 0) return -1;

	r = load_v0_inventory_state(f);
	if (r < 0) return -1;

	game.game_speed = 0;
	game.game_speed_save = DEFAULT_GAME_SPEED;

	return 0;
}


static void
save_text_write_value(FILE *f, const char *name, int value)
{
	fprintf(f, "%s=%i\n", name, value);
}

static void
save_text_write_string(FILE *f, const char *name, const char *value)
{
	fprintf(f, "%s=%s\n", name, value);
}

static void
save_text_write_map_pos(FILE *f, const char *name, map_pos_t pos)
{
	fprintf(f, "%s=%u,%u\n", name,
		MAP_POS_COL(pos), MAP_POS_ROW(pos));
}

static void
save_text_write_array(FILE *f, const char *name,
		      const int values[], uint size)
{
	fprintf(f, "%s=", name);
	for (int i = 0; i < size-1; i++) fprintf(f, "%i,", values[i]);
	if (size > 0) fprintf(f, "%i", values[size-1]);
	fprintf(f, "\n");
}

static int
save_text_game_state(FILE *f)
{
	fprintf(f, "[game]\n");

	save_text_write_string(f, "version", FREESERF_VERSION);

	save_text_write_value(f, "map.col_size", game.map.col_size);
	save_text_write_value(f, "map.row_size", game.map.row_size);

	save_text_write_value(f, "split", game.split);
	save_text_write_map_pos(f, "update_map_initial_pos", game.update_map_initial_pos);

	save_text_write_value(f, "game_type", game.game_type);
	save_text_write_value(f, "tick", game.tick);
	save_text_write_value(f, "game_stats_counter", game.game_stats_counter);
	save_text_write_value(f, "history_counter", game.history_counter);

	int rnd[3] = { game.rnd.state[0],
		       game.rnd.state[1],
		       game.rnd.state[2] };
	save_text_write_array(f, "rnd", rnd, 3);

	save_text_write_value(f, "serf_limit", game.serf_limit);
	save_text_write_value(f, "flag_limit", game.flag_limit);
	save_text_write_value(f, "building_limit", game.building_limit);
	save_text_write_value(f, "inventory_limit", game.inventory_limit);

	save_text_write_value(f, "next_index", game.next_index);
	save_text_write_value(f, "flag_search_counter", game.flag_search_counter);
	save_text_write_value(f, "update_map_last_tick", game.update_map_last_tick);
	save_text_write_value(f, "update_map_counter", game.update_map_counter);

	save_text_write_array(f, "player_history_index", game.player_history_index, 4);
	save_text_write_array(f, "player_history_counter", game.player_history_counter, 3);
	save_text_write_value(f, "resource_history_index", game.resource_history_index);

	save_text_write_value(f, "map.regions", game.map_regions);

	save_text_write_value(f, "max_next_index", game.max_next_index);
	save_text_write_value(f, "max_serfs_from_land", game.max_serfs_from_land);
	save_text_write_value(f, "map.gold_deposit", game.map_gold_deposit);
	save_text_write_value(f, "update_map_16_loop", game.update_map_16_loop);

	save_text_write_value(f, "max_serfs_per_player", game.max_serfs_per_player);
	save_text_write_value(f, "map.gold_morale_factor", game.map_gold_morale_factor);
	save_text_write_value(f, "winning_player", game.winning_player);
	save_text_write_value(f, "player_score_leader", game.player_score_leader);
	fprintf(f, "\n");

	return 0;
}

static int
save_text_player_state(FILE *f)
{
	for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
		player_t *player = game.player[i];
		if (!PLAYER_IS_ACTIVE(player)) continue;

		fprintf(f, "[player %i]\n", i);

		save_text_write_value(f, "flags", player->flags);
		save_text_write_value(f, "build", player->build);
		save_text_write_value(f, "color", player->color);
		save_text_write_value(f, "face", player->face);

		save_text_write_array(f, "tool_prio", player->tool_prio, 9);
		save_text_write_array(f, "resource_count", player->resource_count, 26);
		save_text_write_array(f, "flag_prio", player->flag_prio, 26);
		save_text_write_array(f, "serf_count", player->serf_count, 27);
		save_text_write_array(f, "knight_occupation", player->knight_occupation, 4);

		save_text_write_array(f, "completed_building_count", player->completed_building_count, 23);
		save_text_write_array(f, "incomplete_building_count", player->incomplete_building_count, 23);

		save_text_write_array(f, "inventory_prio", player->inventory_prio, 26);
		save_text_write_array(f, "attacking_buildings", player->attacking_buildings, 64);

		save_text_write_value(f, "initial_supplies", player->initial_supplies);
		save_text_write_value(f, "knights_to_spawn", player->knights_to_spawn);

		save_text_write_value(f, "total_building_score", player->total_building_score);
		save_text_write_value(f, "total_military_score", player->total_military_score);

		save_text_write_value(f, "last_tick", player->last_tick);

		save_text_write_value(f, "reproduction_counter", player->reproduction_counter);
		save_text_write_value(f, "reproduction_reset", player->reproduction_reset);
		save_text_write_value(f, "serf_to_knight_rate", player->serf_to_knight_rate);
		save_text_write_value(f, "serf_to_knight_counter", player->serf_to_knight_counter);

		save_text_write_value(f, "attacking_building_count", player->attacking_building_count);
		save_text_write_array(f, "attacking_knights", player->attacking_knights, 4);
		save_text_write_value(f, "total_attacking_knights", player->total_attacking_knights);
		save_text_write_value(f, "building_attacked", player->building_attacked);
		save_text_write_value(f, "knights_attacking", player->knights_attacking);

		save_text_write_value(f, "food_stonemine", player->food_stonemine);
		save_text_write_value(f, "food_coalmine", player->food_coalmine);
		save_text_write_value(f, "food_ironmine", player->food_ironmine);
		save_text_write_value(f, "food_goldmine", player->food_goldmine);

		save_text_write_value(f, "planks_construction", player->planks_construction);
		save_text_write_value(f, "planks_boatbuilder", player->planks_boatbuilder);
		save_text_write_value(f, "planks_toolmaker", player->planks_toolmaker);

		save_text_write_value(f, "steel_toolmaker", player->steel_toolmaker);
		save_text_write_value(f, "steel_weaponsmith", player->steel_weaponsmith);

		save_text_write_value(f, "coal_steelsmelter", player->coal_steelsmelter);
		save_text_write_value(f, "coal_goldsmelter", player->coal_goldsmelter);
		save_text_write_value(f, "coal_weaponsmith", player->coal_weaponsmith);

		save_text_write_value(f, "wheat_pigfarm", player->wheat_pigfarm);
		save_text_write_value(f, "wheat_mill", player->wheat_mill);

		save_text_write_value(f, "castle_score", player->castle_score);

		save_text_write_value(f, "castle_knights", player->castle_knights);
		save_text_write_value(f, "castle_knights_wanted", player->castle_knights_wanted);

		/* TODO */

		fprintf(f, "\n");
	}

	return 0;
}

static int
save_text_flag_state(FILE *f)
{
	for (int i = 1; i < game.max_flag_index; i++) {
		if (FLAG_ALLOCATED(i)) {
			flag_t *flag = game_get_flag(i);

			fprintf(f, "[flag %i]\n", i);

			save_text_write_map_pos(f, "pos", flag->pos);
			save_text_write_value(f, "search_num", flag->search_num);
			save_text_write_value(f, "search_dir", flag->search_dir);
			save_text_write_value(f, "path_con", flag->path_con);
			save_text_write_value(f, "endpoints", flag->endpoint);
			save_text_write_value(f, "transporter", flag->transporter);

			save_text_write_array(f, "length", flag->length, 6);

			int values[FLAG_MAX_RES_COUNT];
			for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) values[i] = flag->slot[i].type;
			save_text_write_array(f, "slot.type", values, FLAG_MAX_RES_COUNT);

			for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) values[i] = flag->slot[i].dir;
			save_text_write_array(f, "slot.dir", values, FLAG_MAX_RES_COUNT);

			for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) values[i] = flag->slot[i].dest;
			save_text_write_array(f, "slot.dest", values, FLAG_MAX_RES_COUNT);

			int indices[6];
			for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
				if (FLAG_HAS_PATH(flag, d)) {
					indices[d] = FLAG_INDEX(flag->other_endpoint.f[d]);
				} else {
					indices[d] = 0;
				}
			}

			if (FLAG_HAS_BUILDING(flag)) {
				indices[DIR_UP_LEFT] = BUILDING_INDEX(flag->other_endpoint.b[DIR_UP_LEFT]);
			}

			save_text_write_array(f, "other_endpoint", indices, 6);
			save_text_write_array(f, "other_end_dir", flag->other_end_dir, 6);

			save_text_write_value(f, "bld_flags", flag->bld_flags);
			save_text_write_value(f, "bld2_flags", flag->bld2_flags);

			fprintf(f, "\n");
		}
	}

	return 0;
}

static int
save_text_building_state(FILE *f)
{
	for (int i = 1; i < game.max_building_index; i++) {
		if (BUILDING_ALLOCATED(i)) {
			building_t *building = game_get_building(i);

			fprintf(f, "[building %i]\n", i);

			save_text_write_map_pos(f, "pos", building->pos);
			save_text_write_value(f, "type", building->type);
			save_text_write_value(f, "bld", building->bld);
			save_text_write_value(f, "serf", building->serf);
			save_text_write_value(f, "flag", building->flag);

			save_text_write_value(f, "stock[0].type", building->stock[0].type);
			save_text_write_value(f, "stock[0].prio", building->stock[0].prio);
			save_text_write_value(f, "stock[0].available", building->stock[0].available);
			save_text_write_value(f, "stock[0].requested", building->stock[0].requested);
			save_text_write_value(f, "stock[0].maximum", building->stock[0].maximum);

			save_text_write_value(f, "stock[1].type", building->stock[1].type);
			save_text_write_value(f, "stock[1].prio", building->stock[1].prio);
			save_text_write_value(f, "stock[1].available", building->stock[1].available);
			save_text_write_value(f, "stock[1].requested", building->stock[1].requested);
			save_text_write_value(f, "stock[1].maximum", building->stock[1].maximum);

			save_text_write_value(f, "serf_index", building->serf_index);
			save_text_write_value(f, "progress", building->progress);

			if (!BUILDING_IS_BURNING(building) &&
			    (BUILDING_IS_DONE(building) ||
			     BUILDING_TYPE(building) == BUILDING_CASTLE)) {
				if (BUILDING_TYPE(building) == BUILDING_STOCK ||
				    BUILDING_TYPE(building) == BUILDING_CASTLE) {
					save_text_write_value(f, "inventory", INVENTORY_INDEX(building->u.inventory));
				}
			} else if (BUILDING_IS_BURNING(building)) {
				save_text_write_value(f, "tick", building->u.tick);
			} else {
				save_text_write_value(f, "level", building->u.level);
			}

			fprintf(f, "\n");
		}
	}

	return 0;
}

static int
save_text_inventory_state(FILE *f)
{
	for (int i = 0; i < game.max_inventory_index; i++) {
		if (INVENTORY_ALLOCATED(i)) {
			inventory_t *inventory = game_get_inventory(i);

			fprintf(f, "[inventory %i]\n", i);

			save_text_write_value(f, "player", inventory->player_num);
			save_text_write_value(f, "res_dir", inventory->res_dir);
			save_text_write_value(f, "flag", inventory->flag);
			save_text_write_value(f, "building", inventory->building);

			resource_type_t types[2];
			for (int i = 0; i < 2; i++) types[i] = inventory->out_queue[i].type;
			save_text_write_array(f, "queue.type", types, 2);

			int dests[2];
			for (int i = 0; i < 2; i++) dests[i] = inventory->out_queue[i].dest;
			save_text_write_array(f, "queue.dest", dests, 2);

			save_text_write_value(f, "spawn_priority", inventory->spawn_priority);

			save_text_write_array(f, "resources", inventory->resources, 26);
			save_text_write_array(f, "serfs", inventory->serfs, 27);

			fprintf(f, "\n");
		}
	}

	return 0;
}

static int
save_text_serf_state(FILE *f)
{
	for (int i = 1; i < game.max_serf_index; i++) {
		if (!SERF_ALLOCATED(i)) continue;

		serf_t *serf = game_get_serf(i);

		fprintf(f, "[serf %i]\n", i);

		save_text_write_value(f, "type", serf->type);
		save_text_write_value(f, "animation", serf->animation);
		save_text_write_value(f, "counter", serf->counter);
		save_text_write_map_pos(f, "pos", serf->pos);
		save_text_write_value(f, "tick", serf->tick);
		save_text_write_value(f, "state", serf->state);

		switch (serf->state) {
		case SERF_STATE_IDLE_IN_STOCK:
			save_text_write_value(f, "state.inventory", serf->s.idle_in_stock.inv_index);
			break;

		case SERF_STATE_WALKING:
		case SERF_STATE_TRANSPORTING:
		case SERF_STATE_DELIVERING:
			save_text_write_value(f, "state.res", serf->s.walking.res);
			save_text_write_value(f, "state.dest", serf->s.walking.dest);
			save_text_write_value(f, "state.dir", serf->s.walking.dir);
			save_text_write_value(f, "state.wait_counter", serf->s.walking.wait_counter);
			break;

		case SERF_STATE_ENTERING_BUILDING:
			save_text_write_value(f, "state.field_B", serf->s.entering_building.field_B);
			save_text_write_value(f, "state.slope_len", serf->s.entering_building.slope_len);
			break;

		case SERF_STATE_LEAVING_BUILDING:
		case SERF_STATE_READY_TO_LEAVE:
		case SERF_STATE_KNIGHT_LEAVE_FOR_FIGHT:
			save_text_write_value(f, "state.field_B", serf->s.leaving_building.field_B);
			save_text_write_value(f, "state.dest", serf->s.leaving_building.dest);
			save_text_write_value(f, "state.dest2", serf->s.leaving_building.dest2);
			save_text_write_value(f, "state.dir", serf->s.leaving_building.dir);
			save_text_write_value(f, "state.next_state", serf->s.leaving_building.next_state);
			break;

		case SERF_STATE_READY_TO_ENTER:
			save_text_write_value(f, "state.field_B", serf->s.ready_to_enter.field_B);
			break;

		case SERF_STATE_DIGGING:
			save_text_write_value(f, "state.h_index", serf->s.digging.h_index);
			save_text_write_value(f, "state.target_h", serf->s.digging.target_h);
			save_text_write_value(f, "state.dig_pos", serf->s.digging.dig_pos);
			save_text_write_value(f, "state.substate", serf->s.digging.substate);
			break;

		case SERF_STATE_BUILDING:
			save_text_write_value(f, "state.mode", serf->s.building.mode);
			save_text_write_value(f, "state.bld_index", serf->s.building.bld_index);
			save_text_write_value(f, "state.material_step", serf->s.building.material_step);
			save_text_write_value(f, "state.counter", serf->s.building.counter);
			break;

		case SERF_STATE_BUILDING_CASTLE:
			save_text_write_value(f, "state.inv_index", serf->s.building_castle.inv_index);
			break;

		case SERF_STATE_MOVE_RESOURCE_OUT:
		case SERF_STATE_DROP_RESOURCE_OUT:
			save_text_write_value(f, "state.res", serf->s.move_resource_out.res);
			save_text_write_value(f, "state.res_dest", serf->s.move_resource_out.res_dest);
			save_text_write_value(f, "state.next_state", serf->s.move_resource_out.next_state);
			break;

		case SERF_STATE_READY_TO_LEAVE_INVENTORY:
			save_text_write_value(f, "state.mode", serf->s.ready_to_leave_inventory.mode);
			save_text_write_value(f, "state.dest", serf->s.ready_to_leave_inventory.dest);
			save_text_write_value(f, "state.inv_index", serf->s.ready_to_leave_inventory.inv_index);
			break;

		case SERF_STATE_FREE_WALKING:
		case SERF_STATE_LOGGING:
		case SERF_STATE_PLANTING:
		case SERF_STATE_STONECUTTING:
		case SERF_STATE_STONECUTTER_FREE_WALKING:
		case SERF_STATE_FISHING:
		case SERF_STATE_FARMING:
		case SERF_STATE_SAMPLING_GEO_SPOT:
		case SERF_STATE_KNIGHT_FREE_WALKING:
		case SERF_STATE_KNIGHT_ATTACKING_FREE:
		case SERF_STATE_KNIGHT_ATTACKING_FREE_WAIT:
			save_text_write_value(f, "state.dist1", serf->s.free_walking.dist1);
			save_text_write_value(f, "state.dist2", serf->s.free_walking.dist2);
			save_text_write_value(f, "state.neg_dist", serf->s.free_walking.neg_dist1);
			save_text_write_value(f, "state.neg_dist2", serf->s.free_walking.neg_dist2);
			save_text_write_value(f, "state.flags", serf->s.free_walking.flags);
			break;

		case SERF_STATE_SAWING:
			save_text_write_value(f, "state.mode", serf->s.sawing.mode);
			break;

		case SERF_STATE_LOST:
			save_text_write_value(f, "state.field_B", serf->s.lost.field_B);
			break;

		case SERF_STATE_MINING:
			save_text_write_value(f, "state.substate", serf->s.mining.substate);
			save_text_write_value(f, "state.res", serf->s.mining.res);
			save_text_write_value(f, "state.deposit", serf->s.mining.deposit);
			break;

		case SERF_STATE_SMELTING:
			save_text_write_value(f, "state.mode", serf->s.smelting.mode);
			save_text_write_value(f, "state.counter", serf->s.smelting.counter);
			save_text_write_value(f, "state.type", serf->s.smelting.type);
			break;

		case SERF_STATE_MILLING:
			save_text_write_value(f, "state.mode", serf->s.milling.mode);
			break;

		case SERF_STATE_BAKING:
			save_text_write_value(f, "state.mode", serf->s.baking.mode);
			break;

		case SERF_STATE_PIGFARMING:
			save_text_write_value(f, "state.mode", serf->s.pigfarming.mode);
			break;

		case SERF_STATE_BUTCHERING:
			save_text_write_value(f, "state.mode", serf->s.butchering.mode);
			break;

		case SERF_STATE_MAKING_WEAPON:
			save_text_write_value(f, "state.mode", serf->s.making_weapon.mode);
			break;

		case SERF_STATE_MAKING_TOOL:
			save_text_write_value(f, "state.mode", serf->s.making_tool.mode);
			break;

		case SERF_STATE_BUILDING_BOAT:
			save_text_write_value(f, "state.mode", serf->s.building_boat.mode);
			break;

		case SERF_STATE_KNIGHT_ENGAGING_BUILDING:
		case SERF_STATE_KNIGHT_PREPARE_ATTACKING:
		case SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE_WAIT:
		case SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE:
		case SERF_STATE_KNIGHT_ATTACKING:
		case SERF_STATE_KNIGHT_ATTACKING_VICTORY:
		case SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE:
		case SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE_JOIN:
		case SERF_STATE_KNIGHT_ATTACKING_VICTORY_FREE:
			save_text_write_value(f, "state.field_B", serf->s.attacking.field_B);
			save_text_write_value(f, "state.field_C", serf->s.attacking.field_C);
			save_text_write_value(f, "state.field_D", serf->s.attacking.field_D);
			save_text_write_value(f, "state.def_index", serf->s.attacking.def_index);
			break;

		case SERF_STATE_KNIGHT_DEFENDING_FREE:
		case SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE:
			save_text_write_value(f, "state.dist_col", serf->s.defending_free.dist_col);
			save_text_write_value(f, "state.dist_row", serf->s.defending_free.dist_row);
			save_text_write_value(f, "state.field_D", serf->s.defending_free.field_D);
			save_text_write_value(f, "state.other_dist_col", serf->s.defending_free.other_dist_col);
			save_text_write_value(f, "state.other_dist_row", serf->s.defending_free.other_dist_row);
			break;

		case SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT:
			save_text_write_value(f, "state.dist_col", serf->s.leave_for_walk_to_fight.dist_col);
			save_text_write_value(f, "state.dist_row", serf->s.leave_for_walk_to_fight.dist_row);
			save_text_write_value(f, "state.field_D", serf->s.leave_for_walk_to_fight.field_D);
			save_text_write_value(f, "state.field_E", serf->s.leave_for_walk_to_fight.field_E);
			save_text_write_value(f, "state.next_state", serf->s.leave_for_walk_to_fight.next_state);
			break;

		case SERF_STATE_IDLE_ON_PATH:
		case SERF_STATE_WAIT_IDLE_ON_PATH:
		case SERF_STATE_WAKE_AT_FLAG:
		case SERF_STATE_WAKE_ON_PATH:
			save_text_write_value(f, "state.rev_dir", serf->s.idle_on_path.rev_dir);
			save_text_write_value(f, "state.flag", FLAG_INDEX(serf->s.idle_on_path.flag));
			save_text_write_value(f, "state.field_E", serf->s.idle_on_path.field_E);
			break;

		case SERF_STATE_DEFENDING_HUT:
		case SERF_STATE_DEFENDING_TOWER:
		case SERF_STATE_DEFENDING_FORTRESS:
		case SERF_STATE_DEFENDING_CASTLE:
			save_text_write_value(f, "state.next_knight", serf->s.defending.next_knight);
			break;

		default: break;
		}

		fprintf(f, "\n");
	}

	return 0;
}

static int
save_text_map_state(FILE *f)
{
	for (int y = 0; y < game.map.rows; y++) {
		for (int x = 0; x < game.map.cols; x++) {
			map_pos_t pos = MAP_POS(x, y);

			fprintf(f, "[map %i %i]\n", x, y);

			save_text_write_value(f, "height", MAP_HEIGHT(pos));
			save_text_write_value(f, "type.up", MAP_TYPE_UP(pos));
			save_text_write_value(f, "type.down", MAP_TYPE_DOWN(pos));

			if (MAP_PATHS(pos) != 0) {
				save_text_write_value(f, "paths", MAP_PATHS(pos));
			}

			if (MAP_OBJ(pos) != MAP_OBJ_NONE) {
				save_text_write_value(f, "object", MAP_OBJ(pos));
			}

			if (MAP_SERF_INDEX(pos) != 0) {
				save_text_write_value(f, "serf", MAP_SERF_INDEX(pos));
			}

			if (MAP_IN_WATER(pos)) {
				if (MAP_RES_FISH(pos) > 0) {
					save_text_write_value(f, "fish", MAP_RES_FISH(pos));
				}
			} else if (MAP_RES_TYPE(pos) != GROUND_DEPOSIT_NONE) {
				save_text_write_value(f, "resource.type", MAP_RES_TYPE(pos));
				save_text_write_value(f, "resource.amount", MAP_RES_AMOUNT(pos));
			}

			fprintf(f, "\n");
		}
	}

	return 0;
}

int
save_text_state(FILE *f)
{
	int r;

	r = save_text_game_state(f);
	if (r < 0) return -1;

	r = save_text_player_state(f);
	if (r < 0) return -1;

	r = save_text_flag_state(f);
	if (r < 0) return -1;

	r = save_text_building_state(f);
	if (r < 0) return -1;

	r = save_text_inventory_state(f);
	if (r < 0) return -1;

	r = save_text_serf_state(f);
	if (r < 0) return -1;

	r = save_text_map_state(f);
	if (r < 0) return -1;

	return 0;
}


static char *
trim_whitespace(char *str, size_t *len)
{
	/* Left */
	while (isspace(str[0])) {
		str += 1;
		*len -= 1;
	}

	/* Right */
	if (*len > 0) {
		char *last = str + *len - 1;
		while (isspace(last[0])) {
			last[0] = '\0';
			last -= 1;
			*len -= 1;
		}
	}

	return str;
}

/* Read line from f, realloc'ing the buffer
   if necessary. Return bytes read (0 on EOF). */
static size_t
load_text_readline(char **buffer, size_t *len, FILE *f)
{
	size_t i = 0;

	while (1) {
		if (i >= *len-1) {
			/* Double buffer size, always keep enough
			   space for input character plus zero-byte. */
			*len = 2*(*len);
			*buffer = realloc(*buffer, *len);
		}

		/* Read character */
		int c = fgetc(f);
		if (c == EOF) {
			(*buffer)[i] = '\0';
			break;
		}

		(*buffer)[i++] = c;

		if (c == '\n') {
			(*buffer)[i] = '\0';
			break;
		}
	}

	return i;
}


typedef struct {
	list_elm_t elm;
	char *name;
	char *param;
	list_t settings;
} section_t;

typedef struct {
	list_elm_t elm;
	char *key;
	char *value;
} setting_t;


static int
load_text_parse(FILE *f, list_t *sections)
{
	size_t buffer_len = 256;
	char *buffer = malloc(buffer_len*sizeof(char));

	section_t *section = NULL;

	while (1) {
		/* Read line from file */
		char *line = NULL;
		size_t line_len = 0;
		size_t len = load_text_readline(&buffer, &buffer_len, f);
		if (len == 0) {
			if (ferror(f)) return -1;
			break;
		}

		line_len = len;
		line = buffer;

		/* Skip leading whitespace */
		line = trim_whitespace(line, &line_len);

		/* Skip blank lines */
		if (line_len == 0) continue;

		if (line[0] == '[')  {
			/* Section header */
			char *header = &line[1];
			char *header_end = strchr(line, ']');
			if (header_end == NULL) {
				LOGW("savegame", "Malformed section header: `%s'.",
				     header);
				continue;
			}

			size_t header_len = header_end - header;
			header_end[0] = '\0';
			header = trim_whitespace(header, &header_len);

			/* Extract parameter */
			char *param = header;
			while (param[0] != '\0' &&
			       !isspace(param[0])) {
				param += 1;
			}

			if (isspace(param[0])) {
				param[0] = '\0';
				param += 1;
				header_len = param - header;
				while (isspace(param[0])) param += 1;
			}

			/* Create section */
			section = malloc(sizeof(section_t));
			if (section == NULL) abort();

			section->name = strdup(header);
			section->param = strdup(param);
			list_init(&section->settings);
			list_append(sections, (list_elm_t *)section);
		} else if (section != NULL) {
			/* Setting line */
			char *value = strchr(line, '=');
			if (value == NULL) {
				LOGW("savegame", "Malformed setting line: `%s'.", line);
				continue;
			}

			/* Key */
			char *key = line;
			size_t key_len = value - key;
			key = trim_whitespace(key, &key_len);

			/* Value */
			value[0] = '\0';
			value += 1;
			while (isspace(value[0])) value += 1;

			setting_t *setting = malloc(sizeof(setting_t));
			if (setting == NULL) abort();

			setting->key = strdup(key);
			setting->value = strdup(value);
			list_append(&section->settings, (list_elm_t *)setting);
		}
	}

	free(buffer);

	return 0;
}

static void
load_text_free_sections(list_t *sections)
{
	while (!list_is_empty(sections)) {
		section_t *section =
			(section_t *)list_remove_head(sections);
		free(section->name);
		free(section->param);

		while (!list_is_empty(&section->settings)) {
			setting_t *setting =
				(setting_t *)list_remove_head(&section->settings);
			free(setting->key);
			free(setting->value);
			free(setting);
		}

		free(section);
	}
}

static char *
load_text_get_setting(const section_t *section, const char *key)
{
	list_elm_t *elm;
	list_foreach(&section->settings, elm) {
		setting_t *s = (setting_t *)elm;
		if (!strcmp(s->key, key)) {
			return s->value;
		}
	}

	return NULL;
}

static map_pos_t
parse_map_pos(char *str)
{
	char *s = strchr(str, ',');
	if (s == NULL) return 0;

	s[0] = '\0';
	s += 1;

	return MAP_POS(atoi(str), atoi(s));
}

static char *
parse_array_value(char **str)
{
	char *value = *str;
	char *next = strchr(*str, ',');
	if (next == NULL) {
		*str = NULL;
		return value;
	}

	*str = next+1;
	return value;
}

static int
load_text_game_state(list_t *sections)
{
	const char *value;

	/* Find the game section */
	section_t *section = NULL;
	list_elm_t *elm;
	list_foreach(sections, elm) {
		section_t *s = (section_t *)elm;
		if (!strcmp(s->name, "game")) {
			section = s;
			break;
		}
	}

	if (section == NULL) return -1;

	/* Load essential values for calculating map positions
	   so that map positions can be loaded properly. */
	value = load_text_get_setting(section, "map.col_size");
	if (value == NULL) return -1;
	game.map.col_size = atoi(value);

	value = load_text_get_setting(section, "map.row_size");
	if (value == NULL) return -1;
	game.map.row_size = atoi(value);

	game.map_size = (game.map.col_size + game.map.row_size) - 9;

	/* Initialize remaining map dimensions. */
	game.map.cols = 1 << game.map.col_size;
	game.map.rows = 1 << game.map.row_size;
	map_init_dimensions(&game.map);

	/* Load the remaining game state. */
	list_foreach(&section->settings, elm) {
		setting_t *s = (setting_t *)elm;
		if (!strcmp(s->key, "version")) {
			LOGV("savegame", "Loading save game from version %s.", s->value);
		} else if (!strcmp(s->key, "map.col_size") ||
			   !strcmp(s->key, "map.row_size")) {
			/* Already loaded above. */
		} else if (!strcmp(s->key, "split")) {
			game.split = atoi(s->value);
		} else if (!strcmp(s->key, "update_map_initial_pos")) {
			game.update_map_initial_pos = parse_map_pos(s->value);
		} else if (!strcmp(s->key, "game_type")) {
			game.game_type = atoi(s->value);
		} else if (!strcmp(s->key, "tick")) {
			game.tick = atoi(s->value);
		} else if (!strcmp(s->key, "game_stats_counter")) {
			game.game_stats_counter = atoi(s->value);
		} else if (!strcmp(s->key, "history_counter")) {
			game.history_counter = atoi(s->value);
		} else if (!strcmp(s->key, "rnd")) {
			char *array = s->value;
			for (int i = 0; i < 3 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				game.rnd.state[i] = atoi(v);
			}
		} else if (!strcmp(s->key, "serf_limit")) {
			game.serf_limit = atoi(s->value);
		} else if (!strcmp(s->key, "flag_limit")) {
			game.flag_limit = atoi(s->value);
		} else if (!strcmp(s->key, "building_limit")) {
			game.building_limit = atoi(s->value);
		} else if (!strcmp(s->key, "inventory_limit")) {
			game.inventory_limit = atoi(s->value);
		} else if (!strcmp(s->key, "next_index")) {
			game.next_index = atoi(s->value);
		} else if (!strcmp(s->key, "flag_search_counter")) {
			game.flag_search_counter = atoi(s->value);
		} else if (!strcmp(s->key, "update_map_last_tick")) {
			game.update_map_last_tick = atoi(s->value);
		} else if (!strcmp(s->key, "update_map_counter")) {
			game.update_map_counter = atoi(s->value);
		} else if (!strcmp(s->key, "player_history_index")) {
			char *array = s->value;
			for (int i = 0; i < 4 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				game.player_history_index[i] = atoi(v);
			}
		} else if (!strcmp(s->key, "player_history_counter")) {
			char *array = s->value;
			for (int i = 0; i < 3 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				game.player_history_counter[i] = atoi(v);
			}
		} else if (!strcmp(s->key, "resource_history_index")) {
			game.resource_history_index = atoi(s->value);
		} else if (!strcmp(s->key, "map.regions")) {
			game.map_regions = atoi(s->value);
		} else if (!strcmp(s->key, "max_next_index")) {
			game.max_next_index = atoi(s->value);
		} else if (!strcmp(s->key, "max_serfs_from_land")) {
			game.max_serfs_from_land = atoi(s->value);
		} else if (!strcmp(s->key, "map.gold_deposit")) {
			game.map_gold_deposit = atoi(s->value);
		} else if (!strcmp(s->key, "update_map_16_loop")) {
			game.update_map_16_loop = atoi(s->value);
		} else if (!strcmp(s->key, "max_serfs_per_player")) {
			game.max_serfs_per_player = atoi(s->value);
		} else if (!strcmp(s->key, "map.gold_morale_factor")) {
			game.map_gold_morale_factor = atoi(s->value);
		} else if (!strcmp(s->key, "winning_player")) {
			game.winning_player = atoi(s->value);
		} else if (!strcmp(s->key, "player_score_leader")) {
			game.player_score_leader = atoi(s->value);
		} else {
			LOGD("savegame", "Unhandled game setting: `%s'.", s->key);
		}
	}

	/* Allocate game objects */
	game_allocate_objects();

	return 0;
}

static int
load_text_player_section(section_t *section)
{
	/* Parse player number. */
	int n = atoi(section->param);
	if (n < 0 || n >= GAME_MAX_PLAYER_COUNT) return -1;

	game.player[n] = calloc(1, sizeof(player_t));
	if (game.player[n] == NULL) return -1;

	player_t *player = game.player[n];
	player->player_num = n;

	/* Load the player state. */
	list_elm_t *elm;
	list_foreach(&section->settings, elm) {
		setting_t *s = (setting_t *)elm;
		if (!strcmp(s->key, "flags")) {
			player->flags = atoi(s->value);
		} else if (!strcmp(s->key, "build")) {
			player->build = atoi(s->value);
		} else if (!strcmp(s->key, "color")) {
			player->color = atoi(s->value);
		} else if (!strcmp(s->key, "face")) {
			player->face = atoi(s->value);
		} else if (!strcmp(s->key, "tool_prio")) {
			char *array = s->value;
			for (int i = 0; i < 9 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				player->tool_prio[i] = atoi(v);
			}
		} else if (!strcmp(s->key, "resource_count")) {
			char *array = s->value;
			for (int i = 0; i < 26 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				player->resource_count[i] = atoi(v);
			}
		} else if (!strcmp(s->key, "flag_prio")) {
			char *array = s->value;
			for (int i = 0; i < 26 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				player->flag_prio[i] = atoi(v);
			}
		} else if (!strcmp(s->key, "serf_count")) {
			char *array = s->value;
			for (int i = 0; i < 27 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				player->serf_count[i] = atoi(v);
			}
		} else if (!strcmp(s->key, "knight_occupation")) {
			char *array = s->value;
			for (int i = 0; i < 4 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				player->knight_occupation[i] = atoi(v);
			}
		} else if (!strcmp(s->key, "completed_building_count")) {
			char *array = s->value;
			for (int i = 0; i < 23 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				player->completed_building_count[i] = atoi(v);
			}
		} else if (!strcmp(s->key, "incomplete_building_count")) {
			char *array = s->value;
			for (int i = 0; i < 23 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				player->incomplete_building_count[i] = atoi(v);
			}
		} else if (!strcmp(s->key, "inventory_prio")) {
			char *array = s->value;
			for (int i = 0; i < 26 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				player->inventory_prio[i] = atoi(v);
			}
		} else if (!strcmp(s->key, "attacking_buildings")) {
			char *array = s->value;
			for (int i = 0; i < 64 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				player->attacking_buildings[i] = atoi(v);
			}
		} else if (!strcmp(s->key, "initial_supplies")) {
			player->initial_supplies = atoi(s->value);
		} else if (!strcmp(s->key, "knights_to_spawn")) {
			player->knights_to_spawn = atoi(s->value);
		} else if (!strcmp(s->key, "total_building_score")) {
			player->total_building_score = atoi(s->value);
		} else if (!strcmp(s->key, "total_military_score")) {
			player->total_military_score = atoi(s->value);
		} else if (!strcmp(s->key, "last_tick")) {
			player->last_tick = atoi(s->value);
		} else if (!strcmp(s->key, "reproduction_counter")) {
			player->reproduction_counter = atoi(s->value);
		} else if (!strcmp(s->key, "reproduction_reset")) {
			player->reproduction_reset = atoi(s->value);
		} else if (!strcmp(s->key, "serf_to_knight_rate")) {
			player->serf_to_knight_rate = atoi(s->value);
		} else if (!strcmp(s->key, "serf_to_knight_counter")) {
			player->serf_to_knight_counter = atoi(s->value);
		} else if (!strcmp(s->key, "attacking_building_count")) {
			player->attacking_building_count = atoi(s->value);
		} else if (!strcmp(s->key, "attacking_knights")) {
			char *array = s->value;
			for (int i = 0; i < 4 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				player->attacking_knights[i] = atoi(v);
			}
		} else if (!strcmp(s->key, "total_attacking_knights")) {
			player->total_attacking_knights = atoi(s->value);
		} else if (!strcmp(s->key, "building_attacked")) {
			player->building_attacked = atoi(s->value);
		} else if (!strcmp(s->key, "knights_attacking")) {
			player->knights_attacking = atoi(s->value);
		} else if (!strcmp(s->key, "food_stonemine")) {
			player->food_stonemine = atoi(s->value);
		} else if (!strcmp(s->key, "food_coalmine")) {
			player->food_coalmine = atoi(s->value);
		} else if (!strcmp(s->key, "food_ironmine")) {
			player->food_ironmine = atoi(s->value);
		} else if (!strcmp(s->key, "food_goldmine")) {
			player->food_goldmine = atoi(s->value);
		} else if (!strcmp(s->key, "planks_construction")) {
			player->planks_construction = atoi(s->value);
		} else if (!strcmp(s->key, "planks_boatbuilder")) {
			player->planks_boatbuilder = atoi(s->value);
		} else if (!strcmp(s->key, "planks_toolmaker")) {
			player->planks_toolmaker = atoi(s->value);
		} else if (!strcmp(s->key, "steel_toolmaker")) {
			player->steel_toolmaker = atoi(s->value);
		} else if (!strcmp(s->key, "steel_weaponsmith")) {
			player->steel_weaponsmith = atoi(s->value);
		} else if (!strcmp(s->key, "coal_steelsmelter")) {
			player->coal_steelsmelter = atoi(s->value);
		} else if (!strcmp(s->key, "coal_goldsmelter")) {
			player->coal_goldsmelter = atoi(s->value);
		} else if (!strcmp(s->key, "coal_weaponsmith")) {
			player->coal_weaponsmith = atoi(s->value);
		} else if (!strcmp(s->key, "wheat_pigfarm")) {
			player->wheat_pigfarm = atoi(s->value);
		} else if (!strcmp(s->key, "wheat_mill")) {
			player->wheat_mill = atoi(s->value);
		} else if (!strcmp(s->key, "castle_score")) {
			player->castle_score = atoi(s->value);
		} else if (!strcmp(s->key, "castle_knights")) {
			player->castle_knights = atoi(s->value);
		} else if (!strcmp(s->key, "castle_knights_wanted")) {
			player->castle_knights_wanted = atoi(s->value);
		} else {
			LOGD("savegame", "Unhandled player setting: `%s'.", s->key);
		}
	}

	return 0;
}

static int
load_text_player_state(list_t *sections)
{
	list_elm_t *elm;
	list_foreach(sections, elm) {
		section_t *s = (section_t *)elm;
		if (!strcmp(s->name, "player")) {
			int r = load_text_player_section(s);
			if (r < 0) return -1;
		}
	}

	return 0;
}

static int
load_text_flag_section(section_t *section)
{
	/* Parse flag number. */
	int n = atoi(section->param);
	if (n >= game.flag_limit) return -1;

	flag_t *flag = &game.flags[n];
	game.flag_bitmap[n/8] |= BIT(7-(n&7));

	/* Load the flag state. */
	list_elm_t *elm;
	list_foreach(&section->settings, elm) {
		setting_t *s = (setting_t *)elm;
		if (!strcmp(s->key, "pos")) {
			flag->pos = parse_map_pos(s->value);
		} else if (!strcmp(s->key, "search_num")) {
			flag->search_num = atoi(s->value);
		} else if (!strcmp(s->key, "search_dir")) {
			flag->search_dir = atoi(s->value);
		} else if (!strcmp(s->key, "path_con")) {
			flag->path_con = atoi(s->value);
		} else if (!strcmp(s->key, "endpoints")) {
			flag->endpoint = atoi(s->value);
		} else if (!strcmp(s->key, "transporter")) {
			flag->transporter = atoi(s->value);
		} else if (!strcmp(s->key, "length")) {
			char *array = s->value;
			for (int i = 0; i < 6 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				flag->length[i] = atoi(v);
			}
		} else if (!strcmp(s->key, "slot.type")) {
			char *array = s->value;
			for (int i = 0; i < 8 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				flag->slot[i].type = atoi(v);
			}
		} else if (!strcmp(s->key, "slot.dir")) {
			char *array = s->value;
			for (int i = 0; i < 8 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				flag->slot[i].dir = atoi(v);
			}
		} else if (!strcmp(s->key, "slot.dest")) {
			char *array = s->value;
			for (int i = 0; i < 8 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				flag->slot[i].dest = atoi(v);
			}
		} else if (!strcmp(s->key, "other_endpoint")) {
			/* TODO make sure these pointers are valid. Maybe postpone this
			   linking until all flags are loaded. */
			char *array = s->value;
			for (int i = 0; i < 6 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				flag->other_endpoint.f[i] = &game.flags[atoi(v)];
			}
		} else if (!strcmp(s->key, "other_end_dir")) {
			char *array = s->value;
			for (int i = 0; i < 6 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				flag->other_end_dir[i] = atoi(v);
			}
		} else if (!strcmp(s->key, "bld_flags")) {
			flag->bld_flags = atoi(s->value);
		} else if (!strcmp(s->key, "bld2_flags")) {
			flag->bld2_flags = atoi(s->value);
		} else {
			LOGD("savegame", "Unhandled flag setting: `%s'.", s->key);
		}
	}

	/* Fix link if connected to building */
	if (FLAG_HAS_BUILDING(flag)) {
		char *array = load_text_get_setting(section, "other_endpoint");
		char *v = NULL;
		for (dir_t d = DIR_RIGHT; d <= DIR_UP_LEFT && array != NULL; d++) {
			char *r = parse_array_value(&array);
			if (d == DIR_UP_LEFT) v = r;
		}

		if (v == NULL) {
			LOGD("savegame", "Unable to link flag %i to building", FLAG_INDEX(flag));
			return -1;
		}

		flag->other_endpoint.b[DIR_UP_LEFT] = &game.buildings[atoi(v)];
	}

	return 0;
}

static int
load_text_flag_state(list_t *sections)
{
	list_elm_t *elm;
	list_foreach(sections, elm) {
		section_t *s = (section_t *)elm;
		if (!strcmp(s->name, "flag")) {
			int r = load_text_flag_section(s);
			if (r < 0) return -1;
		}
	}

	for (int i = 0; i < game.flag_limit; i++) {
		if (FLAG_ALLOCATED(i)) {
			/* Restore max flag index */
			game.max_flag_index = i+1;
		}
	}

	return 0;
}

static int
load_text_building_section(section_t *section)
{
	/* Parse building number. */
	int n = atoi(section->param);
	if (n >= game.building_limit) return -1;

	building_t *building = &game.buildings[n];
	game.building_bitmap[n/8] |= BIT(7-(n&7));

	/* Load the building state. */
	list_elm_t *elm;
	list_foreach(&section->settings, elm) {
		setting_t *s = (setting_t *)elm;
		if (!strcmp(s->key, "pos")) {
			building->pos = parse_map_pos(s->value);
		} else if (!strcmp(s->key, "type")) {
			building->type = atoi(s->value);
		} else if (!strcmp(s->key, "bld")) {
			building->bld = atoi(s->value);
		} else if (!strcmp(s->key, "serf")) {
			building->serf = atoi(s->value);
		} else if (!strcmp(s->key, "flag")) {
			building->flag = atoi(s->value);
		} else if (!strcmp(s->key, "stock[0].type")) {
			building->stock[0].type = atoi(s->value);
		} else if (!strcmp(s->key, "stock[0].prio")) {
			building->stock[0].prio = atoi(s->value);
		} else if (!strcmp(s->key, "stock[0].available")) {
			building->stock[0].available = atoi(s->value);
		} else if (!strcmp(s->key, "stock[0].requested")) {
			building->stock[0].requested = atoi(s->value);
		} else if (!strcmp(s->key, "stock[0].maximum")) {
			building->stock[0].maximum = atoi(s->value);
		} else if (!strcmp(s->key, "stock[1].type")) {
			building->stock[1].type = atoi(s->value);
		} else if (!strcmp(s->key, "stock[1].prio")) {
			building->stock[1].prio = atoi(s->value);
		} else if (!strcmp(s->key, "stock[1].available")) {
			building->stock[1].available = atoi(s->value);
		} else if (!strcmp(s->key, "stock[1].requested")) {
			building->stock[1].requested = atoi(s->value);
		} else if (!strcmp(s->key, "stock[1].maximum")) {
			building->stock[1].maximum = atoi(s->value);
		} else if (!strcmp(s->key, "serf_index")) {
			building->serf_index = atoi(s->value);
		} else if (!strcmp(s->key, "progress")) {
			building->progress = atoi(s->value);
		} else if (!strcmp(s->key, "inventory") ||
			   !strcmp(s->key, "flag") ||
			   !strcmp(s->key, "level") ||
			   !strcmp(s->key, "tick")) {
			/* Handled later */
		} else {
			LOGD("savegame", "Unhandled building setting: `%s'.", s->key);
		}
	}

	/* Load various values that depend on the building type. */
	/* TODO Check validity of pointers when loading. */
	if (!BUILDING_IS_BURNING(building) &&
	    (BUILDING_IS_DONE(building) ||
	     BUILDING_TYPE(building) == BUILDING_CASTLE)) {
		if (BUILDING_TYPE(building) == BUILDING_STOCK ||
		    BUILDING_TYPE(building) == BUILDING_CASTLE) {
			char *value = load_text_get_setting(section, "inventory");
			if (value == NULL) return -1;
			building->u.inventory = &game.inventories[atoi(value)];
		}
	} else if (BUILDING_IS_BURNING(building)) {
		char *value = load_text_get_setting(section, "tick");
		building->u.tick = atoi(value);
	} else {
		char *value = load_text_get_setting(section, "level");
		building->u.level = atoi(value);
	}

	return 0;
}

static int
load_text_building_state(list_t *sections)
{
	list_elm_t *elm;
	list_foreach(sections, elm) {
		section_t *s = (section_t *)elm;
		if (!strcmp(s->name, "building")) {
			int r = load_text_building_section(s);
			if (r < 0) return -1;
		}
	}

	for (int i = 1; i < game.building_limit; i++) {
		if (BUILDING_ALLOCATED(i)) {
			/* Restore max building index */
			game.max_building_index = i+1;

			/* Restore pointer to castle flag */
			building_t *building = game_get_building(i);
			if (BUILDING_TYPE(building) == BUILDING_CASTLE) {
				game.player[BUILDING_PLAYER(building)]->castle_flag = building->flag;
			}
		}
	}

	return 0;
}

static int
load_text_inventory_section(section_t *section)
{
	/* Parse building number. */
	int n = atoi(section->param);
	if (n >= game.inventory_limit) return -1;

	inventory_t *inventory = &game.inventories[n];
	game.inventory_bitmap[n/8] |= BIT(7-(n&7));

	/* Load the inventory state. */
	list_elm_t *elm;
	list_foreach(&section->settings, elm) {
		setting_t *s = (setting_t *)elm;
		if (!strcmp(s->key, "player")) {
			inventory->player_num = atoi(s->value);
		} else if (!strcmp(s->key, "res_dir")) {
			inventory->res_dir = atoi(s->value);
		} else if (!strcmp(s->key, "flag")) {
			inventory->flag = atoi(s->value);
		} else if (!strcmp(s->key, "building")) {
			inventory->building = atoi(s->value);
		} else if (!strcmp(s->key, "queue.type")) {
			char *array = s->value;
			for (int i = 0; i < 2 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				inventory->out_queue[i].type = atoi(v);
			}
		} else if (!strcmp(s->key, "queue.dest")) {
			char *array = s->value;
			for (int i = 0; i < 2 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				inventory->out_queue[i].dest = atoi(v);
			}
		} else if (!strcmp(s->key, "spawn_priority")) {
			inventory->spawn_priority = atoi(s->value);
		} else if (!strcmp(s->key, "resources")) {
			char *array = s->value;
			for (int i = 0; i < 26 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				inventory->resources[i] = atoi(v);
			}
		} else if (!strcmp(s->key, "serfs")) {
			char *array = s->value;
			for (int i = 0; i < 27 && array != NULL; i++) {
				char *v = parse_array_value(&array);
				inventory->serfs[i] = atoi(v);
			}
		} else {
			LOGD("savegame", "Unhandled inventory setting: `%s'.", s->key);
		}
	}

	return 0;
}

static int
load_text_inventory_state(list_t *sections)
{
	list_elm_t *elm;
	list_foreach(sections, elm) {
		section_t *s = (section_t *)elm;
		if (!strcmp(s->name, "inventory")) {
			int r = load_text_inventory_section(s);
			if (r < 0) return -1;
		}
	}

	for (int i = 0; i < game.inventory_limit; i++) {
		if (INVENTORY_ALLOCATED(i)) {
			/* Restore max inventory index */
			game.max_inventory_index = i+1;
		}
	}

	return 0;
}

static int
load_text_serf_section(section_t *section)
{
	/* Parse serf number. */
	int n = atoi(section->param);
	if (n >= game.serf_limit) return -1;

	serf_t *serf = &game.serfs[n];
	game.serf_bitmap[n/8] |= BIT(7-(n&7));

	/* Load the serf state. */
	list_elm_t *elm;
	list_foreach(&section->settings, elm) {
		setting_t *s = (setting_t *)elm;
		if (!strcmp(s->key, "type")) {
			serf->type = atoi(s->value);
		} else if (!strcmp(s->key, "animation")) {
			serf->animation = atoi(s->value);
		} else if (!strcmp(s->key, "counter")) {
			serf->counter = atoi(s->value);
		} else if (!strcmp(s->key, "pos")) {
			serf->pos = parse_map_pos(s->value);
		} else if (!strcmp(s->key, "tick")) {
			serf->tick = atoi(s->value);
		} else if (!strcmp(s->key, "state")) {
			serf->state = atoi(s->value);
		} else if (!strncmp(s->key, "state.", strlen("state."))) {
			/* Handled later */
		} else {
			LOGD("savegame", "Unhandled serf setting: `%s'.", s->key);
		}
	}

	/* Load state variables */
	list_foreach(&section->settings, elm) {
		setting_t *s = (setting_t *)elm;
		switch (serf->state) {
		case SERF_STATE_IDLE_IN_STOCK:
			if (!strcmp(s->key, "state.inventory")) {
				serf->s.idle_in_stock.inv_index = atoi(s->value);
			}
			break;

		case SERF_STATE_WALKING:
		case SERF_STATE_TRANSPORTING:
		case SERF_STATE_DELIVERING:
			if (!strcmp(s->key, "state.res")) {
				serf->s.walking.res = atoi(s->value);
			} else if (!strcmp(s->key, "state.dest")) {
				serf->s.walking.dest = atoi(s->value);
			} else if (!strcmp(s->key, "state.dir")) {
				serf->s.walking.dir = atoi(s->value);
			} else if (!strcmp(s->key, "state.wait_counter")) {
				serf->s.walking.wait_counter = atoi(s->value);
			}
			break;

		case SERF_STATE_ENTERING_BUILDING:
			if (!strcmp(s->key, "state.field_B")) {
				serf->s.entering_building.field_B = atoi(s->value);
			} else if (!strcmp(s->key, "state.slope_len")) {
				serf->s.entering_building.slope_len = atoi(s->value);
			}
			break;

		case SERF_STATE_LEAVING_BUILDING:
		case SERF_STATE_READY_TO_LEAVE:
		case SERF_STATE_KNIGHT_LEAVE_FOR_FIGHT:
			if (!strcmp(s->key, "state.field_B")) {
				serf->s.leaving_building.field_B = atoi(s->value);
			} else if (!strcmp(s->key, "state.dest")) {
				serf->s.leaving_building.dest = atoi(s->value);
			} else if (!strcmp(s->key, "state.dest2")) {
				serf->s.leaving_building.dest2 = atoi(s->value);
			} else if (!strcmp(s->key, "state.dir")) {
				serf->s.leaving_building.dir = atoi(s->value);
			} else if (!strcmp(s->key, "state.next_state")) {
				serf->s.leaving_building.next_state = atoi(s->value);
			}
			break;

		case SERF_STATE_READY_TO_ENTER:
			if (!strcmp(s->key, "state.field_B")) {
				serf->s.ready_to_enter.field_B = atoi(s->value);
			}
			break;

		case SERF_STATE_DIGGING:
			if (!strcmp(s->key, "state.h_index")) {
				serf->s.digging.h_index = atoi(s->value);
			} else if (!strcmp(s->key, "state.target_h")) {
				serf->s.digging.target_h = atoi(s->value);
			} else if (!strcmp(s->key, "state.dig_pos")) {
				serf->s.digging.dig_pos = atoi(s->value);
			} else if (!strcmp(s->key, "state.substate")) {
				serf->s.digging.substate = atoi(s->value);
			}
			break;

		case SERF_STATE_BUILDING:
			if (!strcmp(s->key, "state.mode")) {
				serf->s.building.mode = atoi(s->value);
			} else if (!strcmp(s->key, "state.bld_index")) {
				serf->s.building.bld_index = atoi(s->value);
			} else if (!strcmp(s->key, "state.material_step")) {
				serf->s.building.material_step = atoi(s->value);
			} else if (!strcmp(s->key, "state.counter")) {
				serf->s.building.counter = atoi(s->value);
			}
			break;

		case SERF_STATE_BUILDING_CASTLE:
			if (!strcmp(s->key, "state.inv_index")) {
				serf->s.building_castle.inv_index = atoi(s->value);
			}
			break;

		case SERF_STATE_MOVE_RESOURCE_OUT:
		case SERF_STATE_DROP_RESOURCE_OUT:
			if (!strcmp(s->key, "state.res")) {
				serf->s.move_resource_out.res = atoi(s->value);
			} else if (!strcmp(s->key, "state.res_dest")) {
				serf->s.move_resource_out.res_dest = atoi(s->value);
			} else if (!strcmp(s->key, "state.next_state")) {
				serf->s.move_resource_out.next_state = atoi(s->value);
			}
			break;

		case SERF_STATE_READY_TO_LEAVE_INVENTORY:
			if (!strcmp(s->key, "state.mode")) {
				serf->s.ready_to_leave_inventory.mode = atoi(s->value);
			} else if (!strcmp(s->key, "state.dest")) {
				serf->s.ready_to_leave_inventory.dest = atoi(s->value);
			} else if (!strcmp(s->key, "state.inv_index")) {
				serf->s.ready_to_leave_inventory.inv_index = atoi(s->value);
			}
			break;

		case SERF_STATE_FREE_WALKING:
		case SERF_STATE_LOGGING:
		case SERF_STATE_PLANTING:
		case SERF_STATE_STONECUTTING:
		case SERF_STATE_STONECUTTER_FREE_WALKING:
		case SERF_STATE_FISHING:
		case SERF_STATE_FARMING:
		case SERF_STATE_SAMPLING_GEO_SPOT:
		case SERF_STATE_KNIGHT_FREE_WALKING:
		case SERF_STATE_KNIGHT_ATTACKING_FREE:
		case SERF_STATE_KNIGHT_ATTACKING_FREE_WAIT:
			if (!strcmp(s->key, "state.dist1")) {
				serf->s.free_walking.dist1 = atoi(s->value);
			} else if (!strcmp(s->key, "state.dist2")) {
				serf->s.free_walking.dist2 = atoi(s->value);
			} else if (!strcmp(s->key, "state.neg_dist")) {
				serf->s.free_walking.neg_dist1 = atoi(s->value);
			} else if (!strcmp(s->key, "state.neg_dist2")) {
				serf->s.free_walking.neg_dist2 = atoi(s->value);
			} else if (!strcmp(s->key, "state.flags")) {
				serf->s.free_walking.flags = atoi(s->value);
			}
			break;

		case SERF_STATE_SAWING:
			if (!strcmp(s->key, "state.mode")) {
				serf->s.sawing.mode = atoi(s->value);
			}
			break;

		case SERF_STATE_LOST:
			if (!strcmp(s->key, "state.field_B")) {
				serf->s.lost.field_B = atoi(s->value);
			}
			break;

		case SERF_STATE_MINING:
			if (!strcmp(s->key, "state.substate")) {
				serf->s.mining.substate = atoi(s->value);
			} else if (!strcmp(s->key, "state.res")) {
				serf->s.mining.res = atoi(s->value);
			} else if (!strcmp(s->key, "state.deposit")) {
				serf->s.mining.deposit = atoi(s->value);
			}
			break;

		case SERF_STATE_SMELTING:
			if (!strcmp(s->key, "state.mode")) {
				serf->s.smelting.mode = atoi(s->value);
			} else if (!strcmp(s->key, "state.counter")) {
				serf->s.smelting.counter = atoi(s->value);
			} else if (!strcmp(s->key, "state.type")) {
				serf->s.smelting.type = atoi(s->value);
			}
			break;

		case SERF_STATE_MILLING:
			if (!strcmp(s->key, "state.mode")) {
				serf->s.milling.mode = atoi(s->value);
			}
			break;

		case SERF_STATE_BAKING:
			if (!strcmp(s->key, "state.mode")) {
				serf->s.baking.mode = atoi(s->value);
			}
			break;

		case SERF_STATE_PIGFARMING:
			if (!strcmp(s->key, "state.mode")) {
				serf->s.pigfarming.mode = atoi(s->value);
			}
			break;

		case SERF_STATE_BUTCHERING:
			if (!strcmp(s->key, "state.mode")) {
				serf->s.butchering.mode = atoi(s->value);
			}
			break;

		case SERF_STATE_MAKING_WEAPON:
			if (!strcmp(s->key, "state.mode")) {
				serf->s.making_weapon.mode = atoi(s->value);
			}
			break;

		case SERF_STATE_MAKING_TOOL:
			if (!strcmp(s->key, "state.mode")) {
				serf->s.making_tool.mode = atoi(s->value);
			}
			break;

		case SERF_STATE_BUILDING_BOAT:
			if (!strcmp(s->key, "state.mode")) {
				serf->s.building_boat.mode = atoi(s->value);
			}
			break;

		case SERF_STATE_KNIGHT_ENGAGING_BUILDING:
		case SERF_STATE_KNIGHT_PREPARE_ATTACKING:
		case SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE_WAIT:
		case SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE:
		case SERF_STATE_KNIGHT_ATTACKING:
		case SERF_STATE_KNIGHT_ATTACKING_VICTORY:
		case SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE:
		case SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE_JOIN:
		case SERF_STATE_KNIGHT_ATTACKING_VICTORY_FREE:
			if (!strcmp(s->key, "state.field_B")) {
				serf->s.attacking.field_B = atoi(s->value);
			} else if (!strcmp(s->key, "state.field_C")) {
				serf->s.attacking.field_C = atoi(s->value);
			} else if (!strcmp(s->key, "state.field_D")) {
				serf->s.attacking.field_D = atoi(s->value);
			} else if (!strcmp(s->key, "state.def_index")) {
				serf->s.attacking.def_index = atoi(s->value);
			}
			break;

		case SERF_STATE_KNIGHT_DEFENDING_FREE:
		case SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE:
			if (!strcmp(s->key, "state.dist_col")) {
				serf->s.defending_free.dist_col = atoi(s->value);
			} else if (!strcmp(s->key, "state.dist_row")) {
				serf->s.defending_free.dist_row = atoi(s->value);
			} else if (!strcmp(s->key, "state.field_D")) {
				serf->s.defending_free.field_D = atoi(s->value);
			} else if (!strcmp(s->key, "state.other_dist_col")) {
				serf->s.defending_free.other_dist_col = atoi(s->value);
			} else if (!strcmp(s->key, "state.other_dist_row")) {
				serf->s.defending_free.other_dist_row = atoi(s->value);
			}
			break;

		case SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT:
			if (!strcmp(s->key, "state.dist_col")) {
				serf->s.leave_for_walk_to_fight.dist_col = atoi(s->value);
			} else if (!strcmp(s->key, "state.dist_row")) {
				serf->s.leave_for_walk_to_fight.dist_row = atoi(s->value);
			} else if (!strcmp(s->key, "state.field_D")) {
				serf->s.leave_for_walk_to_fight.field_D = atoi(s->value);
			} else if (!strcmp(s->key, "state.field_E")) {
				serf->s.leave_for_walk_to_fight.field_E = atoi(s->value);
			} else if (!strcmp(s->key, "state.next_state")) {
				serf->s.leave_for_walk_to_fight.next_state = atoi(s->value);
			}
			break;

		case SERF_STATE_IDLE_ON_PATH:
		case SERF_STATE_WAIT_IDLE_ON_PATH:
		case SERF_STATE_WAKE_AT_FLAG:
		case SERF_STATE_WAKE_ON_PATH:
			if (!strcmp(s->key, "state.rev_dir")) {
				serf->s.idle_on_path.rev_dir = atoi(s->value);
			} else if (!strcmp(s->key, "state.flag")) {
				serf->s.idle_on_path.flag = &game.flags[atoi(s->value)];
			} else if (!strcmp(s->key, "state.field_E")) {
				serf->s.idle_on_path.field_E = atoi(s->value);
			}
			break;

		case SERF_STATE_DEFENDING_HUT:
		case SERF_STATE_DEFENDING_TOWER:
		case SERF_STATE_DEFENDING_FORTRESS:
		case SERF_STATE_DEFENDING_CASTLE:
			if (!strcmp(s->key, "state.next_knight")) {
				serf->s.defending.next_knight = atoi(s->value);
			}
			break;

		default:
			break;
		}
	}

	return 0;
}

static int
load_text_serf_state(list_t *sections)
{
	list_elm_t *elm;
	list_foreach(sections, elm) {
		section_t *s = (section_t *)elm;
		if (!strcmp(s->name, "serf")) {
			int r = load_text_serf_section(s);
			if (r < 0) return -1;
		}
	}

	for (int i = 0; i < game.serf_limit; i++) {
		if (SERF_ALLOCATED(i)) {
			/* Restore max serf index */
			game.max_serf_index = i+1;
		}
	}

	return 0;
}

static int
load_text_map_section(section_t *section)
{
	char *param = section->param;

	/* Parse map position. */
	int col = atoi(param);
	if (col < 0 || col >= game.map.cols) return -1;

	while (!isspace(*param) && *param != '\0') param += 1;
	while (isspace(*param) && *param != '\0') param += 1;
	if (*param == '\0') return -1;

	int row = atoi(param);
	if (row < 0 || row >= game.map.rows) return -1;

	map_pos_t pos = MAP_POS(col,row);
	map_tile_t *tiles = game.map.tiles;

	uint paths = 0;
	uint height = 0;
	uint type_up = 0;
	uint type_down = 0;
	map_obj_t obj = MAP_OBJ_NONE;

	uint fish = 0;
	uint resource_type = 0;
	uint resource_amount = 0;

	/* Load the map tile. */
	list_elm_t *elm;
	list_foreach(&section->settings, elm) {
		setting_t *s = (setting_t *)elm;
		if (!strcmp(s->key, "paths")) {
			paths = atoi(s->value);
		} else if (!strcmp(s->key, "height")) {
			height = atoi(s->value);
		} else if (!strcmp(s->key, "type.up")) {
			type_up = atoi(s->value);
		} else if (!strcmp(s->key, "type.down")) {
			type_down = atoi(s->value);
		} else if (!strcmp(s->key, "object")) {
			obj = atoi(s->value);
		} else if (!strcmp(s->key, "serf")) {
			tiles[pos].serf = atoi(s->value);
		} else if (!strcmp(s->key, "fish")) {
			fish = atoi(s->value);
		} else if (!strcmp(s->key, "resource.type")) {
			resource_type = atoi(s->value);
		} else if (!strcmp(s->key, "resource.amount")) {
			resource_amount = atoi(s->value);
		} else {
			LOGD("savegame", "Unhandled map setting: `%s'.", s->key);
		}
	}

	tiles[pos].paths = paths & 0x3f;
	tiles[pos].height = height & 0x1f;
	tiles[pos].type = ((type_up & 0xf) << 4) | (type_down & 0xf);
	tiles[pos].obj = obj & 0x7f;

	tiles[pos].obj_index = 0;

	if (MAP_IN_WATER(pos)) {
		tiles[pos].resource = fish;
	} else {
		tiles[pos].resource = ((resource_type & 7) << 5) |
			(resource_amount & 0x1f);
	}

	return 0;
}

static int
load_text_map_state(list_t *sections)
{
	list_elm_t *elm;
	list_foreach(sections, elm) {
		section_t *s = (section_t *)elm;
		if (!strcmp(s->name, "map")) {
			int r = load_text_map_section(s);
			if (r < 0) return -1;
		}
	}

	/* Restore idle serf flag */
	for (int i = 1; i < game.max_serf_index; i++) {
		if (!SERF_ALLOCATED(i)) continue;

		serf_t *serf = game_get_serf(i);
		if (serf->state == SERF_STATE_IDLE_ON_PATH ||
		    serf->state == SERF_STATE_WAIT_IDLE_ON_PATH) {
			game.map.tiles[serf->pos].obj |= BIT(7);
		}
	}

	/* Restore building index */
	for (int i = 1; i < game.max_building_index; i++) {
		if (!BUILDING_ALLOCATED(i)) continue;

		building_t *building = game_get_building(i);
		if (MAP_OBJ(building->pos) < MAP_OBJ_SMALL_BUILDING ||
		    MAP_OBJ(building->pos) > MAP_OBJ_CASTLE) {
			return -1;
		}

		game.map.tiles[building->pos].obj_index = BUILDING_INDEX(building);
	}

	/* Restore flag index */
	for (int i = 1; i < game.max_flag_index; i++) {
		if (!FLAG_ALLOCATED(i)) continue;

		flag_t *flag = game_get_flag(i);
		if (MAP_OBJ(flag->pos) != MAP_OBJ_FLAG) {
			return -1;
		}

		game.map.tiles[flag->pos].obj_index = FLAG_INDEX(flag);
	}

	return 0;
}

int
load_text_state(FILE *f)
{
	int r;

	list_t sections;
	list_init(&sections);

	load_text_parse(f, &sections);

	r = load_text_game_state(&sections);
	if (r < 0) {
		LOGD("savegame", "Error loading game state");
		goto error;
	}

	r = load_text_player_state(&sections);
	if (r < 0) {
		LOGD("savegame", "Error loading player state");
		goto error;
	}

	r = load_text_flag_state(&sections);
	if (r < 0) {
		LOGD("savegame", "Error loading flag state");
		goto error;
	}

	r = load_text_building_state(&sections);
	if (r < 0) {
		LOGD("savegame", "Error loading building state");
		goto error;
	}

	r = load_text_inventory_state(&sections);
	if (r < 0) {
		LOGD("savegame", "Error loading inventory state");
		goto error;
	}

	r = load_text_serf_state(&sections);
	if (r < 0) {
		LOGD("savegame", "Error loading serf state");
		goto error;
	}

	r = load_text_map_state(&sections);
	if (r < 0) {
		LOGD("savegame", "Error loading map state");
		goto error;
	}

	game.game_speed = 0;
	game.game_speed_save = DEFAULT_GAME_SPEED;

	load_text_free_sections(&sections);
	return 0;

error:
	load_text_free_sections(&sections);
	return -1;
}


int
load_state(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL) {
		LOGE("savegame", "Unable to open save game file: `%s'.", path);
		return -1;
	}

	int r = load_text_state(f);
	if (r < 0) {
		LOGW("savegame", "Unable to load save game, trying compatability mode...");

		fseek(f, 0, SEEK_SET);
		r = load_v0_state(f);
		if (r < 0) {
			LOGE("savegame", "Failed to load save game.");
			fclose(f);
			return -1;
		}
	}

	fclose(f);

	return 0;
}

int
save_state(const char *path)
{
	FILE *f = fopen(path, "wb");
	if (f == NULL) return -1;

	int r = save_text_state(f);
	fclose(f);

	return r;
}
