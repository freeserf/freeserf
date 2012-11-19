/* savegame.c */

#include <stdio.h>
#include <stdlib.h>

#include "game.h"
#include "globals.h"


/* Load global state from save game. */
static int
load_v0_globals_state(FILE *f)
{
	uint8_t *data = malloc(210);
	if (data == NULL) return -1;

	size_t rd = fread(data, sizeof(uint8_t), 210, f);
	if (rd < 210) {
		free(data);
		return -1;
	}

	globals.map_index_mask = *(uint32_t *)&data[0] >> 2;

	globals.map_dirs[DIR_RIGHT] = *(uint32_t *)&data[4] >> 2;
	globals.map_dirs[DIR_DOWN_RIGHT] = *(uint32_t *)&data[8] >> 2;
	globals.map_dirs[DIR_DOWN] = *(uint32_t *)&data[12] >> 2;
	globals.map_move_left_2 = *(uint16_t *)&data[16] >> 2;
	globals.map_dirs[DIR_UP_LEFT] = *(uint32_t *)&data[18] >> 2;
	globals.map_dirs[DIR_UP] = *(uint32_t *)&data[22] >> 2;
	globals.map_dirs[DIR_UP_RIGHT] = *(uint32_t *)&data[26] >> 2;
	globals.map_dirs[DIR_DOWN_LEFT] = *(uint32_t *)&data[30] >> 2;

	globals.map_col_size = *(uint32_t *)&data[34] >> 2;
	globals.map_elms = *(uint32_t *)&data[38];
	globals.map_row_shift = *(uint16_t *)&data[42];
	globals.map_col_mask = *(uint16_t *)&data[44];
	globals.map_row_mask = *(uint16_t *)&data[46];
	globals.map_data_offset = *(uint32_t *)&data[48] >> 2;
	globals.map_shifted_col_mask = *(uint16_t *)&data[52] >> 2;
	globals.map_shifted_row_mask = *(uint32_t *)&data[54] >> 2;
	globals.map_col_pairs = *(uint16_t *)&data[58];
	globals.map_row_pairs = *(uint16_t *)&data[60];
	globals.map_cols = *(uint16_t *)&data[62];
	globals.map_rows = *(uint16_t *)&data[64];

	globals.split = *(uint8_t *)&data[66];
	/* globals.field_37F = *(uint8_t *)&data[67]; */
	globals.update_map_initial_pos = *(uint32_t *)&data[68] >> 2;

	globals.cfg_left = *(uint8_t *)&data[72];
	globals.cfg_right = *(uint8_t *)&data[73];

	globals.game_type = *(uint16_t *)&data[74];
	globals.game_tick = *(uint32_t *)&data[76];
	/* globals.field_20E = *(uint16_t *)&data[80]; */
	/* globals.field_210 = *(uint16_t *)&data[82]; */

	globals.rnd_1 = *(uint16_t *)&data[84];
	globals.rnd_2 = *(uint16_t *)&data[86];
	globals.rnd_3 = *(uint16_t *)&data[88];

	globals.max_ever_flag_index = *(uint16_t *)&data[90];
	globals.max_ever_building_index = *(uint16_t *)&data[92];
	globals.max_ever_serf_index = *(uint16_t *)&data[94];

	globals.next_index = *(uint16_t *)&data[96];
	globals.flag_search_counter = *(uint16_t *)&data[98];
	globals.update_map_last_anim = *(uint16_t *)&data[100];
	globals.update_map_counter = *(uint16_t *)&data[102];

	/*
	globals.field_320 = *(uint16_t *)&data[104];
	globals.field_322 = *(uint16_t *)&data[106];
	globals.field_324 = *(uint16_t *)&data[108];
	globals.field_326 = *(uint16_t *)&data[110];
	globals.field_328 = *(uint16_t *)&data[112];
	globals.field_32A = *(uint16_t *)&data[114];
	globals.field_32C = *(uint16_t *)&data[116];
	globals.field_32E = *(uint16_t *)&data[118];
	*/

	globals.map_regions = *(uint16_t *)&data[120];

	if (0/*globals.game_type == GAME_TYPE_TUTORIAL*/) {
		globals.tutorial_level = *(uint16_t *)&data[122];
	} else if (0/*globals.game_type == GAME_TYPE_MISSION*/) {
		globals.mission_level = *(uint16_t *)&data[124];
		/*globals.max_mission_level = *(uint16_t *)&data[126];*/
		/* memcpy(globals.mission_code, &data[128], 8); */
	} else if (1/*globals.game_type == GAME_TYPE_1_PLAYER*/) {
		/*globals.menu_map_size = *(uint16_t *)&data[136];*/
		/*globals.rnd_init_1 = *(uint16_t *)&data[138];
		globals.rnd_init_2 = *(uint16_t *)&data[140];
		globals.rnd_init_3 = *(uint16_t *)&data[142];*/

		/*
		memcpy(globals.menu_ai_face, &data[144], 4);
		memcpy(globals.menu_ai_intelligence, &data[148], 4);
		memcpy(globals.menu_ai_supplies, &data[152], 4);
		memcpy(globals.menu_ai_reproduction, &data[156], 4);
		*/

		/*
		memcpy(globals.menu_human_supplies, &data[160], 2);
		memcpy(globals.menu_human_reproduction, &data[162], 2);
		*/
	}

	/*
	globals.saved_pl1_map_cursor_col = *(uint16_t *)&data[164];
	globals.saved_pl1_map_cursor_row = *(uint16_t *)&data[166];

	globals.saved_pl1_pl_sett_100 = *(uint8_t *)&data[168];
	globals.saved_pl1_pl_sett_101 = *(uint8_t *)&data[169];
	globals.saved_pl1_pl_sett_102 = *(uint16_t *)&data[170];

	globals.saved_pl1_build = *(uint8_t *)&data[172];
	globals.field_17B = *(uint8_t *)&data[173];
	*/

	globals.max_ever_inventory_index = *(uint16_t *)&data[174];
	globals.map_max_serfs_left = *(uint16_t *)&data[176];
	/* globals.max_stock_buildings = *(uint16_t *)&data[178]; */
	globals.field_286 = *(uint16_t *)&data[180];
	/* globals.field_4A = *(uint16_t *)&data[182]; */
	globals.map_gold_deposit = *(uint32_t *)&data[184];
	globals.update_map_16_loop = *(uint16_t *)&data[188];
	globals.map_size = *(uint16_t *)&data[190];

	/*
	globals.field_52 = *(uint16_t *)&data[192];
	globals.field_54 = *(uint16_t *)&data[194];
	globals.field_56 = *(uint16_t *)&data[196];
	*/

	globals.map_62_5_times_regions = *(uint16_t *)&data[198];
	/*
	globals.field_5A = *(uint16_t *)&data[200];
	globals.field_5E = *(uint16_t *)&data[202];
	globals.field_380 = *(uint8_t *)&data[204];
	globals.show_game_end_box = *(uint8_t *)&data[205];
	*/

	globals.map_dirs[DIR_LEFT] = *(uint32_t *)&data[206] >> 2;

	free(data);

	/* Skip unused section. */
	int r = fseek(f, 40, SEEK_CUR);
	if (r < 0) return -1;

	return 0;
}

/* Load player state from save game. */
static int
load_v0_player_sett_state(FILE *f)
{
	uint8_t *data = malloc(8628);
	if (data == NULL) return -1;

	for (int i = 0; i < 4; i++) {
		size_t rd = fread(data, sizeof(uint8_t), 8628, f);
		if (rd < 8628) {
			free(data);
			return -1;
		}

		player_sett_t *sett = globals.player_sett[i];

		for (int j = 0; j < 9; j++) {
			sett->tool_prio[j] = *(uint16_t *)&data[2*j];
		}

		for (int j = 0; j < 26; j++) {
			sett->resource_count[j] = data[18+j];
			sett->flag_prio[j] = data[44+j];
		}

		for (int j = 0; j < 27; j++) {
			sett->serf_count[j] = *(uint16_t *)&data[70+2*j];
		}

		for (int j = 0; j < 4; j++) {
			sett->knight_occupation[j] = data[124+j];
		}

		sett->player_num = *(uint16_t *)&data[128];
		sett->flags = data[130];
		sett->build = data[131];

		for (int j = 0; j < 23; j++) {
			sett->completed_building_count[j] = *(uint16_t *)&data[132+2*j];
			sett->incomplete_building_count[j] = *(uint16_t *)&data[178+2*j];
		}

		for (int j = 0; j < 26; j++) {
			sett->inventory_prio[j] = data[224+j];
		}

		for (int j = 0; j < 64; j++) {
			sett->attacking_buildings[j] = *(uint16_t *)&data[250+2*j];
		}

		sett->current_sett_5_item = *(uint16_t *)&data[378];
		sett->map_cursor_col = *(uint16_t *)&data[380];
		sett->map_cursor_row = *(uint16_t *)&data[382];
		sett->map_cursor_type = data[384];
		sett->panel_btn_type = data[385];
		sett->building_height_after_level = *(uint16_t *)&data[386];
		sett->building = *(uint16_t *)&data[388];

		sett->castle_flag = *(uint16_t *)&data[390];
		sett->castle_inventory = *(uint16_t *)&data[392];

		sett->cont_search_after_non_optimal_find = *(uint16_t *)&data[394];
		sett->knights_to_spawn = *(uint16_t *)&data[396];
		sett->field_110 = *(uint16_t *)&data[400];

		sett->total_land_area = *(uint32_t *)&data[402];
		sett->total_building_score = *(uint32_t *)&data[406];
		sett->total_military_score = *(uint32_t *)&data[410];

		sett->last_anim = *(uint16_t *)&data[414];

		sett->reproduction_counter = *(uint16_t *)&data[416];
		sett->reproduction_reset = *(uint16_t *)&data[418];
		sett->serf_to_knight_rate = *(uint16_t *)&data[420];
		sett->serf_to_knight_counter = *(uint16_t *)&data[422];

		sett->attacking_building_count = *(uint16_t *)&data[424];
		for (int j = 0; j < 4; j++) {
			sett->attacking_knights[j] = *(uint16_t *)&data[426+2*j];
		}
		sett->total_attacking_knights = *(uint16_t *)&data[434];
		sett->building_attacked = *(uint16_t *)&data[436];
		sett->knights_attacking = *(uint16_t *)&data[438];

		sett->analysis_goldore = *(uint16_t *)&data[440];
		sett->analysis_ironore = *(uint16_t *)&data[442];
		sett->analysis_coal = *(uint16_t *)&data[444];
		sett->analysis_stone = *(uint16_t *)&data[446];

		sett->food_stonemine = *(uint16_t *)&data[448];
		sett->food_coalmine = *(uint16_t *)&data[450];
		sett->food_ironmine = *(uint16_t *)&data[452];
		sett->food_goldmine = *(uint16_t *)&data[454];

		sett->planks_construction = *(uint16_t *)&data[456];
		sett->planks_boatbuilder = *(uint16_t *)&data[458];
		sett->planks_toolmaker = *(uint16_t *)&data[460];

		sett->steel_toolmaker = *(uint16_t *)&data[462];
		sett->steel_weaponsmith = *(uint16_t *)&data[464];

		sett->coal_steelsmelter = *(uint16_t *)&data[466];
		sett->coal_goldsmelter = *(uint16_t *)&data[468];
		sett->coal_weaponsmith = *(uint16_t *)&data[470];

		sett->wheat_pigfarm = *(uint16_t *)&data[472];
		sett->wheat_mill = *(uint16_t *)&data[474];

		sett->current_sett_6_item = *(uint16_t *)&data[476];

		/* TODO */
	}

	free(data);

	return 0;
}

/* Load map state from save game. */
static int
load_v0_map_state(FILE *f)
{
	uint8_t *data = malloc(8*globals.map_elms);
	if (data == NULL) return -1;

	size_t rd = fread(data, sizeof(uint8_t), 8*globals.map_elms, f);
	if (rd < 8*globals.map_elms) {
		free(data);
		return -1;
	}

	map_1_t *map = globals.map_mem2_ptr;
	map_2_t *map_data = MAP_2_DATA(globals.map_mem2_ptr);

	memset(globals.map_mem2_ptr, '\0', 8*globals.map_elms);

	for (int y = 0; y < globals.map_rows; y++) {
		for (int x = 0; x < globals.map_cols; x++) {
			map_pos_t pos = MAP_POS(x, y);
			uint8_t *field_1_data = &data[4*(x + (y << globals.map_row_shift))];
			uint8_t *field_2_data = &data[4*(x + (y << globals.map_row_shift)) + 4*globals.map_cols];

			map[pos].flags = field_1_data[0];
			map[pos].height = field_1_data[1];
			map[pos].type = field_1_data[2];
			map[pos].obj = field_1_data[3];

			if (MAP_OBJ(pos) >= MAP_OBJ_FLAG &&
			    MAP_OBJ(pos) <= MAP_OBJ_CASTLE) {
				map_data[pos].u.index = *(uint16_t *)&field_2_data[0];
			} else {
				map_data[pos].u.s.resource = field_2_data[0];
				map_data[pos].u.s.field_1 = field_2_data[1];
			}

			map_data[pos].serf_index = *(uint16_t *)&field_2_data[2];
		}
	}

	free(data);

	return 0;
}

/* Load serf state from save game. */
static int
load_v0_serf_state(FILE *f)
{
	/* Load serf bitmap. */
	int bitmap_size = 4*((globals.max_ever_serf_index + 31)/32);
	uint8_t *bitmap = malloc(bitmap_size);
	if (bitmap == NULL) return -1;

	size_t rd = fread(bitmap, sizeof(uint8_t), bitmap_size, f);
	if (rd < bitmap_size) {
		free(bitmap);
		return -1;
	}

	memset(globals.serfs_bitmap, '\0', (globals.max_serf_cnt+31)/32);
	memcpy(globals.serfs_bitmap, bitmap, bitmap_size);

	free(bitmap);

	/* Load serf data. */
	uint8_t *data = malloc(16*globals.max_ever_serf_index);
	if (data == NULL) return -1;

	rd = fread(data, 16*sizeof(uint8_t), globals.max_ever_serf_index, f);
	if (rd < globals.max_ever_serf_index) {
		free(data);
		return -1;
	}

	for (int i = 0; i < globals.max_ever_serf_index; i++) {
		uint8_t *serf_data = &data[16*i];
		serf_t *serf = &globals.serfs[i];

		serf->type = serf_data[0];
		serf->animation = serf_data[1];
		serf->counter = *(uint16_t *)&serf_data[2];
		serf->pos = *(uint32_t *)&serf_data[4] >> 2;
		serf->anim = *(uint16_t *)&serf_data[8];
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
		case SERF_STATE_WAIT_FOR_RESOURCE_OUT:
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

		case SERF_STATE_IDLE_ON_PATH:
		case SERF_STATE_WAIT_IDLE_ON_PATH:
		case SERF_STATE_WAKE_AT_FLAG:
		case SERF_STATE_WAKE_ON_PATH:
			serf->s.idle_on_path.rev_dir = *(int8_t *)&serf_data[11];
			serf->s.idle_on_path.flag = &globals.flgs[*(uint32_t *)&serf_data[12]/70];
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
	int bitmap_size = 4*((globals.max_ever_flag_index + 31)/32);
	uint8_t *flag_bitmap = malloc(bitmap_size);
	if (flag_bitmap == NULL) return -1;

	size_t rd = fread(flag_bitmap, sizeof(uint8_t), bitmap_size, f);
	if (rd < bitmap_size) {
		free(flag_bitmap);
		return -1;
	}

	memset(globals.flg_bitmap, '\0', (globals.max_flg_cnt+31)/32);
	memcpy(globals.flg_bitmap, flag_bitmap, bitmap_size);

	free(flag_bitmap);

	/* Load flag data. */
	uint8_t *data = malloc(70*globals.max_ever_flag_index);
	if (data == NULL) return -1;

	rd = fread(data, 70*sizeof(uint8_t), globals.max_ever_flag_index, f);
	if (rd < globals.max_ever_flag_index) {
		free(data);
		return -1;
	}

	for (int i = 0; i < globals.max_ever_flag_index; i++) {
		uint8_t *flag_data = &data[70*i];
		flag_t *flag = &globals.flgs[i];

		flag->pos = MAP_POS(0, 0); /* TODO */
		flag->search_num = *(uint16_t *)&flag_data[0];
		flag->search_dir = flag_data[2];
		flag->path_con = flag_data[3];
		flag->endpoint = flag_data[4];
		flag->transporter = flag_data[5];

		for (int j = 0; j < 6; j++) {
			flag->length[j] = flag_data[6+j];
		}

		for (int j = 0; j < 8; j++) {
			flag->res_waiting[j] = flag_data[12+j];
			flag->res_dest[j] = *(uint16_t *)&flag_data[20+2*j];
		}

		for (int j = 0; j < 6; j++) {
			int offset = *(uint32_t *)&flag_data[36+4*j];
			flag->other_endpoint.f[j] = &globals.flgs[offset/70];

			/* Other endpoint could be a building in direction up left. */
			if (j == 4 && BIT_TEST(flag->endpoint, 6)) {
				flag->other_endpoint.b[j] = &globals.buildings[offset/18];
			}

			flag->other_end_dir[j] = flag_data[60+j];
		}

		flag->bld_flags = flag_data[66];
		flag->stock1_prio = flag_data[67];
		flag->bld2_flags = flag_data[68];
		flag->stock2_prio = flag_data[69];
	}

	free(data);

	/* Set flag positions. */
	for (int y = 0; y < globals.map_rows; y++) {
		for (int x = 0; x < globals.map_cols; x++) {
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
load_v0_building_state(FILE *f)
{
	/* Load building bitmap. */
	int bitmap_size = 4*((globals.max_ever_building_index + 31)/32);
	uint8_t *bitmap = malloc(bitmap_size);
	if (bitmap == NULL) return -1;

	size_t rd = fread(bitmap, sizeof(uint8_t), bitmap_size, f);
	if (rd < bitmap_size) {
		free(bitmap);
		return -1;
	}

	memset(globals.buildings_bitmap, '\0', (globals.max_building_cnt+31)/32);
	memcpy(globals.buildings_bitmap, bitmap, bitmap_size);

	free(bitmap);

	/* Load building data. */
	uint8_t *data = malloc(18*globals.max_ever_building_index);
	if (data == NULL) return -1;

	rd = fread(data, 18*sizeof(uint8_t), globals.max_ever_building_index, f);
	if (rd < globals.max_ever_building_index) {
		free(data);
		return -1;
	}

	for (int i = 0; i < globals.max_ever_building_index; i++) {
		uint8_t *building_data = &data[18*i];
		building_t *building = &globals.buildings[i];

		building->pos = *(uint32_t *)&building_data[0] >> 2;
		building->bld = building_data[4];
		building->serf = building_data[5];
		building->flg_index = *(uint16_t *)&building_data[6];
		building->stock1 = building_data[8];
		building->stock2 = building_data[9];
		building->serf_index = *(uint16_t *)&building_data[10];
		building->progress = *(uint16_t *)&building_data[12];

		if (!BUILDING_IS_BURNING(building) &&
		    BUILDING_IS_DONE(building)) {
			int offset = *(uint32_t *)&building_data[14];
			if (BUILDING_TYPE(building) == BUILDING_STOCK ||
			    BUILDING_TYPE(building) == BUILDING_CASTLE) {
				building->u.inventory = &globals.inventories[offset/120];
			} else {
				building->u.flag = &globals.flgs[offset/70];
			}
		} else {
			building->u.s.level = *(uint16_t *)&building_data[14];
			building->u.s.planks_needed = building_data[16];
			building->u.s.stone_needed = building_data[17];
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
	int bitmap_size = 4*((globals.max_ever_inventory_index + 31)/32);
	uint8_t *bitmap = malloc(bitmap_size);
	if (bitmap == NULL) return -1;

	size_t rd = fread(bitmap, sizeof(uint8_t), bitmap_size, f);
	if (rd < bitmap_size) {
		free(bitmap);
		return -1;
	}

	memset(globals.inventories_bitmap, '\0', (globals.max_inventory_cnt+31)/32);
	memcpy(globals.inventories_bitmap, bitmap, bitmap_size);

	free(bitmap);

	/* Load inventory data. */
	uint8_t *data = malloc(120*globals.max_ever_inventory_index);
	if (data == NULL) return -1;

	rd = fread(data, 120*sizeof(uint8_t), globals.max_ever_inventory_index, f);
	if (rd < globals.max_ever_inventory_index) {
		free(data);
		return -1;
	}

	for (int i = 0; i < globals.max_ever_inventory_index; i++) {
		uint8_t *inventory_data = &data[120*i];
		inventory_t *inventory = &globals.inventories[i];

		inventory->player_num = inventory_data[0];
		inventory->res_dir = inventory_data[1];
		inventory->flg_index = *(uint16_t *)&inventory_data[2];
		inventory->bld_index = *(uint16_t *)&inventory_data[4];

		for (int j = 0; j < 26; j++) {
			inventory->resources[j] = *(uint16_t *)&inventory_data[6+2*j];
		}

		for (int j = 0; j < 2; j++) {
			inventory->out_queue[j] = inventory_data[58+j]-1;
			inventory->out_dest[j] = *(uint16_t *)&inventory_data[60+2*j];
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

	r = load_v0_globals_state(f);
	if (r < 0) return -1;

	r = load_v0_player_sett_state(f);
	if (r < 0) return -1;

	r = load_v0_map_state(f);
	if (r < 0) return -1;

	r = load_v0_serf_state(f);
	if (r < 0) return -1;

	r = load_v0_flag_state(f);
	if (r < 0) return -1;

	r = load_v0_building_state(f);
	if (r < 0) return -1;

	r = load_v0_inventory_state(f);
	if (r < 0) return -1;

	globals.game_speed = 0;
	globals.game_speed_save = DEFAULT_GAME_SPEED;

	return 0;
}
