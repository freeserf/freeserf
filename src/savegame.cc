/*
 * savegame.cc - Loading and saving of save games
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

#include "src/savegame.h"

#include <cstring>
#include <cctype>
#include <ctime>
#include <sstream>
#include <vector>
#include <map>

#include "src/game.h"
#include "src/map.h"
#include "src/version.h"
#include "src/log.h"
#include "src/misc.h"
#include "src/inventory.h"

#define SAVE_MAP_TILE_SIZE   16
#define SAVE_MAP_TILE_COUNT  (SAVE_MAP_TILE_SIZE*SAVE_MAP_TILE_SIZE)


typedef struct {
  unsigned int rows, cols;
  unsigned int row_shift;
} v0_map_t;

static unsigned int max_flag_index = 0;
static unsigned int max_inventory_index = 0;
static unsigned int max_building_index = 0;

static map_pos_t
load_v0_map_pos(const v0_map_t *map, uint32_t value) {
  return MAP_POS((value >> 2) & (map->cols-1),
           (value >> (2 + map->row_shift)) & (map->rows-1));
}

/* Load main game state from save game. */
static int
load_v0_game_state(FILE *f, v0_map_t *map) {
  uint8_t *data = reinterpret_cast<uint8_t*>(malloc(210));
  if (data == NULL) return -1;

  size_t rd = fread(data, sizeof(uint8_t), 210, f);
  if (rd < 210) {
    free(data);
    return -1;
  }

  /* Load these first so map dimensions can be reconstructed.
     This is necessary to load map positions. */
  game.map_size = *reinterpret_cast<uint16_t*>(&data[190]);

  map->row_shift = *reinterpret_cast<uint16_t*>(&data[42]);
  map->cols = *reinterpret_cast<uint16_t*>(&data[62]);
  map->rows = *reinterpret_cast<uint16_t*>(&data[64]);

  /* Init the rest of map dimensions. */
  game.map.col_size = 5 + game.map_size/2;
  game.map.row_size = 5 + (game.map_size - 1)/2;
  game.map.cols = 1 << game.map.col_size;
  game.map.rows = 1 << game.map.row_size;
  map_init_dimensions(&game.map);

  /* Allocate game objects */
  const int max_map_size = 10;
  game.serf_limit = (0x1f84 * (1 << max_map_size) - 4) / 0x81;
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

  game.split = *reinterpret_cast<uint8_t*>(&data[66]);
  /* game.field_37F = *(uint8_t *)&data[67]; */
  game.update_map_initial_pos = load_v0_map_pos(map,
                                       *reinterpret_cast<uint32_t*>(&data[68]));

  game.game_type = *reinterpret_cast<uint16_t*>(&data[74]);
  game.tick = *reinterpret_cast<uint32_t*>(&data[76]);
  game.game_stats_counter = 0;
  game.history_counter = 0;

  game.rnd.state[0] = *reinterpret_cast<uint16_t*>(&data[84]);
  game.rnd.state[1] = *reinterpret_cast<uint16_t*>(&data[86]);
  game.rnd.state[2] = *reinterpret_cast<uint16_t*>(&data[88]);

  max_flag_index = *reinterpret_cast<uint16_t*>(&data[90]);
  max_building_index = *reinterpret_cast<uint16_t*>(&data[92]);
  game.max_serf_index = *reinterpret_cast<uint16_t*>(&data[94]);

  game.next_index = *reinterpret_cast<uint16_t*>(&data[96]);
  game.flag_search_counter = *reinterpret_cast<uint16_t*>(&data[98]);
  game.update_map_last_tick = *reinterpret_cast<uint16_t*>(&data[100]);
  game.update_map_counter = *reinterpret_cast<uint16_t*>(&data[102]);

  for (int i = 0; i < 4; i++) {
    game.player_history_index[i] =
                                 *reinterpret_cast<uint16_t*>(&data[104 + i*2]);
  }

  for (int i = 0; i < 3; i++) {
    game.player_history_counter[i] =
                                 *reinterpret_cast<uint16_t*>(&data[112 + i*2]);
  }

  game.resource_history_index = *reinterpret_cast<uint16_t*>(&data[118]);

  game.map_regions = *reinterpret_cast<uint16_t*>(&data[120]);

  if (0/*game.game_type == GAME_TYPE_TUTORIAL*/) {
    game.tutorial_level = *reinterpret_cast<uint16_t*>(&data[122]);
  } else if (0/*game.game_type == GAME_TYPE_MISSION*/) {
    game.mission_level = *reinterpret_cast<uint16_t*>(&data[124]);
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

  max_inventory_index = *reinterpret_cast<uint16_t*>(&data[174]);
  /*game.map_max_serfs_left = *(uint16_t *)&data[176];*/
  /* game.max_stock_buildings = *(uint16_t *)&data[178]; */
  game.max_next_index = *reinterpret_cast<uint16_t*>(&data[180]);
  game.max_serfs_from_land = *reinterpret_cast<uint16_t*>(&data[182]);
  game.map_gold_deposit = *reinterpret_cast<uint32_t*>(&data[184]);
  game.update_map_16_loop = *reinterpret_cast<uint16_t*>(&data[188]);

  /*
  game.map_field_52 = *(uint16_t *)&data[192];
  game.field_54 = *(uint16_t *)&data[194];
  game.field_56 = *(uint16_t *)&data[196];
  */

  game.max_serfs_per_player = *reinterpret_cast<uint16_t*>(&data[198]);
  game.map_gold_morale_factor = *reinterpret_cast<uint16_t*>(&data[200]);
  game.winning_player = *reinterpret_cast<uint16_t*>(&data[202]);
  game.player_score_leader = *reinterpret_cast<uint8_t*>(&data[204]);
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
load_v0_player_state(FILE *f) {
  const int default_player_colors[] = {
    64, 72, 68, 76
  };

  uint8_t *data = reinterpret_cast<uint8_t*>(malloc(8628));
  if (data == NULL) return -1;

  for (int i = 0; i < 4; i++) {
    size_t rd = fread(data, sizeof(uint8_t), 8628, f);
    if (rd < 8628) {
      free(data);
      return -1;
    }

    if (!BIT_TEST(data[130], 6)) continue;

    game.player[i] = reinterpret_cast<player_t*>(calloc(1, sizeof(player_t)));
    if (game.player[i] == NULL) {
      free(data);
      return -1;
    }

    player_t *player = game.player[i];

    for (int j = 0; j < 9; j++) {
      player->tool_prio[j] = *reinterpret_cast<uint16_t*>(&data[2*j]);
    }

    for (int j = 0; j < 26; j++) {
      player->resource_count[j] = data[18+j];
      player->flag_prio[j] = data[44+j];
    }

    for (int j = 0; j < 27; j++) {
      player->serf_count[j] = *reinterpret_cast<uint16_t*>(&data[70+2*j]);
    }

    for (int j = 0; j < 4; j++) {
      player->knight_occupation[j] = data[124+j];
    }

    player->player_num = *reinterpret_cast<uint16_t*>(&data[128]);
    player->color = default_player_colors[i];
    player->flags = data[130];
    player->build = data[131];

    for (int j = 0; j < 23; j++) {
      player->completed_building_count[j] =
                                   *reinterpret_cast<uint16_t*>(&data[132+2*j]);
      player->incomplete_building_count[j] =
                                   *reinterpret_cast<uint16_t*>(&data[178+2*j]);
    }

    for (int j = 0; j < 26; j++) {
      player->inventory_prio[j] = data[224+j];
    }

    for (int j = 0; j < 64; j++) {
      player->attacking_buildings[j] =
                                   *reinterpret_cast<uint16_t*>(&data[250+2*j]);
    }

    player->current_sett_5_item = *reinterpret_cast<uint16_t*>(&data[378]);
    player->building = *reinterpret_cast<uint16_t*>(&data[388]);

    player->castle_flag = *reinterpret_cast<uint16_t*>(&data[390]);
    player->castle_inventory = *reinterpret_cast<uint16_t*>(&data[392]);

    player->cont_search_after_non_optimal_find =
                                       *reinterpret_cast<uint16_t*>(&data[394]);
    player->knights_to_spawn = *reinterpret_cast<uint16_t*>(&data[396]);
    /*player->field_110 = *(uint16_t *)&data[400];*/

    player->total_building_score = *reinterpret_cast<uint32_t*>(&data[406]);
    player->total_military_score = *reinterpret_cast<uint32_t*>(&data[410]);

    player->last_tick = *reinterpret_cast<uint16_t*>(&data[414]);

    player->reproduction_counter = *reinterpret_cast<uint16_t*>(&data[416]);
    player->reproduction_reset = *reinterpret_cast<uint16_t*>(&data[418]);
    player->serf_to_knight_rate = *reinterpret_cast<uint16_t*>(&data[420]);
    player->serf_to_knight_counter = *reinterpret_cast<uint16_t*>(&data[422]);

    player->attacking_building_count = *reinterpret_cast<uint16_t*>(&data[424]);
    for (int j = 0; j < 4; j++) {
      player->attacking_knights[j] =
                                   *reinterpret_cast<uint16_t*>(&data[426+2*j]);
    }
    player->total_attacking_knights = *reinterpret_cast<uint16_t*>(&data[434]);
    player->building_attacked = *reinterpret_cast<uint16_t*>(&data[436]);
    player->knights_attacking = *reinterpret_cast<uint16_t*>(&data[438]);

    player->analysis_goldore = *reinterpret_cast<uint16_t*>(&data[440]);
    player->analysis_ironore = *reinterpret_cast<uint16_t*>(&data[442]);
    player->analysis_coal = *reinterpret_cast<uint16_t*>(&data[444]);
    player->analysis_stone = *reinterpret_cast<uint16_t*>(&data[446]);

    player->food_stonemine = *reinterpret_cast<uint16_t*>(&data[448]);
    player->food_coalmine = *reinterpret_cast<uint16_t*>(&data[450]);
    player->food_ironmine = *reinterpret_cast<uint16_t*>(&data[452]);
    player->food_goldmine = *reinterpret_cast<uint16_t*>(&data[454]);

    player->planks_construction = *reinterpret_cast<uint16_t*>(&data[456]);
    player->planks_boatbuilder = *reinterpret_cast<uint16_t*>(&data[458]);
    player->planks_toolmaker = *reinterpret_cast<uint16_t*>(&data[460]);

    player->steel_toolmaker = *reinterpret_cast<uint16_t*>(&data[462]);
    player->steel_weaponsmith = *reinterpret_cast<uint16_t*>(&data[464]);

    player->coal_steelsmelter = *reinterpret_cast<uint16_t*>(&data[466]);
    player->coal_goldsmelter = *reinterpret_cast<uint16_t*>(&data[468]);
    player->coal_weaponsmith = *reinterpret_cast<uint16_t*>(&data[470]);

    player->wheat_pigfarm = *reinterpret_cast<uint16_t*>(&data[472]);
    player->wheat_mill = *reinterpret_cast<uint16_t*>(&data[474]);

    player->current_sett_6_item = *reinterpret_cast<uint16_t*>(&data[476]);

    player->castle_score = *reinterpret_cast<uint16_t*>(&data[478]);

    /* TODO */

    player->timers_count = 0;
  }

  free(data);

  return 0;
}

/* Load map state from save game. */
static int
load_v0_map_state(FILE *f, const v0_map_t *map) {
  unsigned int tile_count = map->cols*map->rows;

  uint8_t *data = reinterpret_cast<uint8_t*>(malloc(8*tile_count));
  if (data == NULL) return -1;

  size_t rd = fread(data, sizeof(uint8_t), 8*tile_count, f);
  if (rd < 8*tile_count) {
    free(data);
    return -1;
  }

  map_tile_t *tiles = game.map.tiles;

  for (unsigned int y = 0; y < game.map.rows; y++) {
    for (unsigned int x = 0; x < game.map.cols; x++) {
      map_pos_t pos = MAP_POS(x, y);
      uint8_t *field_1_data = &data[4*(x + (y << map->row_shift))];
      uint8_t *field_2_data = &data[4*(x + (y << map->row_shift)) +
                                    4*map->cols];

      tiles[pos].paths = field_1_data[0] & 0x3f;
      tiles[pos].height = field_1_data[1] & 0x1f;
      tiles[pos].type = field_1_data[2];
      tiles[pos].obj = field_1_data[3] & 0x7f;

      if (MAP_OBJ(pos) >= MAP_OBJ_FLAG &&
          MAP_OBJ(pos) <= MAP_OBJ_CASTLE) {
        tiles[pos].resource = 0;
        tiles[pos].obj_index = *reinterpret_cast<uint16_t*>(&field_2_data[0]);
      } else {
        tiles[pos].resource = field_2_data[0];
        tiles[pos].obj_index = 0;
      }

      tiles[pos].serf = *reinterpret_cast<uint16_t*>(&field_2_data[2]);
    }
  }

  free(data);

  return 0;
}

/* Load serf state from save game. */
static int
load_v0_serf_state(FILE *f, const v0_map_t *map) {
  /* Load serf bitmap. */
  int bitmap_size = 4*((game.max_serf_index + 31)/32);
  uint8_t *bitmap = reinterpret_cast<uint8_t*>(malloc(bitmap_size));
  if (bitmap == NULL) return -1;

  size_t rd = fread(bitmap, sizeof(uint8_t), bitmap_size, f);
  if (rd < (size_t)bitmap_size) {
    free(bitmap);
    return -1;
  }

  memset(game.serf_bitmap, '\0', (game.serf_limit+31)/32);
  memcpy(game.serf_bitmap, bitmap, bitmap_size);

  free(bitmap);

  /* Load serf data. */
  uint8_t *data = reinterpret_cast<uint8_t*>(malloc(16*game.max_serf_index));
  if (data == NULL) return -1;

  rd = fread(data, 16*sizeof(uint8_t), game.max_serf_index, f);
  if (rd < game.max_serf_index) {
    free(data);
    return -1;
  }

  for (unsigned int i = 0; i < game.max_serf_index; i++) {
    uint8_t *serf_data = &data[16*i];
    serf_t *serf = &game.serfs[i];

    serf->type = serf_data[0];
    serf->animation = serf_data[1];
    serf->counter = *reinterpret_cast<uint16_t*>(&serf_data[2]);
    serf->pos = load_v0_map_pos(map,
                                *reinterpret_cast<uint32_t*>(&serf_data[4]));
    serf->tick = *reinterpret_cast<uint16_t*>(&serf_data[8]);
    serf->state = (serf_state_t)serf_data[10];

    LOGV("savegame", "load serf %i: %s", i, serf_get_state_name(serf->state));

    switch (serf->state) {
    case SERF_STATE_IDLE_IN_STOCK:
      serf->s.idle_in_stock.inv_index =
                                   *reinterpret_cast<uint16_t*>(&serf_data[14]);
      break;

    case SERF_STATE_WALKING:
    case SERF_STATE_TRANSPORTING:
    case SERF_STATE_DELIVERING:
      serf->s.walking.res = *reinterpret_cast<uint8_t*>(&serf_data[11]);
      serf->s.walking.dest = *reinterpret_cast<uint16_t*>(&serf_data[12]);
      serf->s.walking.dir = *reinterpret_cast<uint8_t*>(&serf_data[14]);
      serf->s.walking.wait_counter =
                                    *reinterpret_cast<uint8_t*>(&serf_data[15]);
      break;

    case SERF_STATE_ENTERING_BUILDING:
      serf->s.entering_building.field_B =
                                    *reinterpret_cast<uint8_t*>(&serf_data[11]);
      serf->s.entering_building.slope_len =
                                   *reinterpret_cast<uint16_t*>(&serf_data[12]);
      break;

    case SERF_STATE_LEAVING_BUILDING:
    case SERF_STATE_READY_TO_LEAVE:
      serf->s.leaving_building.field_B =
                                    *reinterpret_cast<uint8_t*>(&serf_data[11]);
      serf->s.leaving_building.dest =
                                    *reinterpret_cast<uint8_t*>(&serf_data[12]);
      serf->s.leaving_building.dest2 =
                                    *reinterpret_cast<uint8_t*>(&serf_data[13]);
      serf->s.leaving_building.dir =
                                    *reinterpret_cast<uint8_t*>(&serf_data[14]);
      serf->s.leaving_building.next_state = (serf_state_t)serf_data[15];
      break;

    case SERF_STATE_READY_TO_ENTER:
      serf->s.ready_to_enter.field_B =
                                    *reinterpret_cast<uint8_t*>(&serf_data[11]);
      break;

    case SERF_STATE_DIGGING:
      serf->s.digging.h_index = *reinterpret_cast<uint8_t*>(&serf_data[11]);
      serf->s.digging.target_h = serf_data[12];
      serf->s.digging.dig_pos = *reinterpret_cast<uint8_t*>(&serf_data[13]);
      serf->s.digging.substate = *reinterpret_cast<uint8_t*>(&serf_data[14]);
      break;

    case SERF_STATE_BUILDING:
      serf->s.building.mode = *reinterpret_cast<uint8_t*>(&serf_data[11]);
      serf->s.building.bld_index = *reinterpret_cast<uint16_t*>(&serf_data[12]);
      serf->s.building.material_step = serf_data[14];
      serf->s.building.counter = serf_data[15];
      break;

    case SERF_STATE_BUILDING_CASTLE:
      serf->s.building_castle.inv_index =
                                   *reinterpret_cast<uint16_t*>(&serf_data[12]);
      break;

    case SERF_STATE_MOVE_RESOURCE_OUT:
    case SERF_STATE_DROP_RESOURCE_OUT:
      serf->s.move_resource_out.res = serf_data[11];
      serf->s.move_resource_out.res_dest =
                                   *reinterpret_cast<uint16_t*>(&serf_data[12]);
      serf->s.move_resource_out.next_state = (serf_state_t)serf_data[15];
      break;

    case SERF_STATE_READY_TO_LEAVE_INVENTORY:
      serf->s.ready_to_leave_inventory.mode =
                                    *reinterpret_cast<uint8_t*>(&serf_data[11]);
      serf->s.ready_to_leave_inventory.dest =
                                   *reinterpret_cast<uint16_t*>(&serf_data[12]);
      serf->s.ready_to_leave_inventory.inv_index =
                                   *reinterpret_cast<uint16_t*>(&serf_data[14]);
      break;

    case SERF_STATE_FREE_WALKING:
    case SERF_STATE_LOGGING:
    case SERF_STATE_PLANTING:
    case SERF_STATE_STONECUTTING:
    case SERF_STATE_STONECUTTER_FREE_WALKING:
    case SERF_STATE_FISHING:
    case SERF_STATE_FARMING:
    case SERF_STATE_SAMPLING_GEO_SPOT:
      serf->s.free_walking.dist1 = *reinterpret_cast<uint8_t*>(&serf_data[11]);
      serf->s.free_walking.dist2 = *reinterpret_cast<uint8_t*>(&serf_data[12]);
      serf->s.free_walking.neg_dist1 =
                                    *reinterpret_cast<uint8_t*>(&serf_data[13]);
      serf->s.free_walking.neg_dist2 =
                                    *reinterpret_cast<uint8_t*>(&serf_data[14]);
      serf->s.free_walking.flags = *reinterpret_cast<uint8_t*>(&serf_data[15]);
      break;

    case SERF_STATE_SAWING:
      serf->s.sawing.mode = *reinterpret_cast<uint8_t*>(&serf_data[11]);
      break;

    case SERF_STATE_LOST:
      serf->s.lost.field_B = *reinterpret_cast<uint8_t*>(&serf_data[11]);
      break;

    case SERF_STATE_MINING:
      serf->s.mining.substate = serf_data[11];
      serf->s.mining.res = serf_data[13];
      serf->s.mining.deposit = (ground_deposit_t)serf_data[14];
      break;

    case SERF_STATE_SMELTING:
      serf->s.smelting.mode = *reinterpret_cast<uint8_t*>(&serf_data[11]);
      serf->s.smelting.counter = *reinterpret_cast<uint8_t*>(&serf_data[12]);
      serf->s.smelting.type = serf_data[13];
      break;

    case SERF_STATE_MILLING:
      serf->s.milling.mode = *reinterpret_cast<uint8_t*>(&serf_data[11]);
      break;

    case SERF_STATE_BAKING:
      serf->s.baking.mode = *reinterpret_cast<uint8_t*>(&serf_data[11]);
      break;

    case SERF_STATE_PIGFARMING:
      serf->s.pigfarming.mode = *reinterpret_cast<uint8_t*>(&serf_data[11]);
      break;

    case SERF_STATE_BUTCHERING:
      serf->s.butchering.mode = *reinterpret_cast<uint8_t*>(&serf_data[11]);
      break;

    case SERF_STATE_MAKING_WEAPON:
      serf->s.making_weapon.mode = *reinterpret_cast<uint8_t*>(&serf_data[11]);
      break;

    case SERF_STATE_MAKING_TOOL:
      serf->s.making_tool.mode = *reinterpret_cast<uint8_t*>(&serf_data[11]);
      break;

    case SERF_STATE_BUILDING_BOAT:
      serf->s.building_boat.mode = *reinterpret_cast<uint8_t*>(&serf_data[11]);
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
      serf->s.idle_on_path.rev_dir =
                             (dir_t)*reinterpret_cast<uint8_t*>(&serf_data[11]);
      serf->s.idle_on_path.flag =
                    game.flags[*reinterpret_cast<uint32_t*>(&serf_data[12])/70];
      serf->s.idle_on_path.field_E = serf_data[14];
      break;

    case SERF_STATE_DEFENDING_HUT:
    case SERF_STATE_DEFENDING_TOWER:
    case SERF_STATE_DEFENDING_FORTRESS:
    case SERF_STATE_DEFENDING_CASTLE:
      serf->s.defending.next_knight =
                                   *reinterpret_cast<uint16_t*>(&serf_data[14]);
      break;

    default: break;
    }
  }

  free(data);

  return 0;
}

/* Load flag state from save game. */
static int
load_v0_flag_state(FILE *f) {
  /* Load flag bitmap. */
  int bitmap_size = 4*((max_flag_index + 31)/32);
  uint8_t *flag_bitmap = reinterpret_cast<uint8_t*>(malloc(bitmap_size));
  if (flag_bitmap == NULL) return -1;

  size_t rd = fread(flag_bitmap, sizeof(uint8_t), bitmap_size, f);
  if (rd < (size_t)bitmap_size) {
    free(flag_bitmap);
    return -1;
  }

  /* Load flag data. */
  void *flag_data = malloc(70);
  if (flag_data == NULL) {
    free(flag_bitmap);
    return -1;
  }

  for (unsigned int i = 0; i < max_flag_index; i++) {
    rd = fread(flag_data, 70, 1, f);
    if (rd != 1) {
      free(flag_data);
      free(flag_bitmap);
      return -1;
    }

    if (BIT_TEST(flag_bitmap[(i)>>3], 7-((i)&7))) {
      flag_t *flag = game.flags.get_or_insert(i);
      save_reader_binary_t reader(flag_data, 70);
      reader >> *flag;
    }
  }
  free(flag_data);
  free(flag_bitmap);

  /* Set flag positions. */
  for (unsigned int y = 0; y < game.map.rows; y++) {
    for (unsigned int x = 0; x < game.map.cols; x++) {
      map_pos_t pos = MAP_POS(x, y);
      if (MAP_OBJ(pos) == MAP_OBJ_FLAG) {
        flag_t *flag = game.flags[MAP_OBJ_INDEX(pos)];
        flag->set_position(pos);
      }
    }
  }

  return 0;
}

/* Load building state from save game. */
static int
load_v0_building_state(FILE *f, const v0_map_t *map) {
  /* Load building bitmap. */
  int bitmap_size = 4*((max_building_index + 31)/32);
  uint8_t *bitmap = reinterpret_cast<uint8_t*>(malloc(bitmap_size));
  if (bitmap == NULL) return -1;

  size_t rd = fread(bitmap, sizeof(uint8_t), bitmap_size, f);
  if (rd < (size_t)bitmap_size) {
    free(bitmap);
    return -1;
  }

  /* Load building data. */
  uint8_t *data = reinterpret_cast<uint8_t*>(malloc(18));
  if (data == NULL) {
    free(bitmap);
    return -1;
  }

  for (unsigned int i = 0; i < max_building_index; i++) {
    rd = fread(data, 18*sizeof(uint8_t), 1, f);
    if (rd != 1) {
      free(data);
      free(bitmap);
      return -1;
    }

    if (BIT_TEST(bitmap[(i)>>3], 7-((i)&7))) {
      building_t *building = game.buildings.get_or_insert(i);
      save_reader_binary_t reader(data, 18);
      reader >> *building;
    }
  }

  free(data);
  free(bitmap);

  return 0;
}

/* Load inventory state from save game. */
static int
load_v0_inventory_state(FILE *f) {
  /* Load inventory bitmap. */
  int bitmap_size = 4*((max_inventory_index + 31)/32);
  uint8_t *bitmap = reinterpret_cast<uint8_t*>(malloc(bitmap_size));
  if (bitmap == NULL) return -1;

  size_t rd = fread(bitmap, sizeof(uint8_t), bitmap_size, f);
  if (rd < (size_t)bitmap_size) {
    free(bitmap);
    return -1;
  }

  /* Load inventory data. */
  uint8_t *inventory_data = reinterpret_cast<uint8_t*>(malloc(120));
  if (inventory_data == NULL) {
    free(bitmap);
    return -1;
  }

  for (unsigned int i = 0; i < max_inventory_index; i++) {
    rd = fread(inventory_data, 120, 1, f);
    if (rd != 1) {
      free(inventory_data);
      free(bitmap);
      return -1;
    }

    if (BIT_TEST(bitmap[(i)>>3], 7-((i)&7))) {
      inventory_t *inventory = game.inventories.get_or_insert(i);
      save_reader_binary_t reader(inventory_data, 120);
      reader >> *inventory;
    }
  }

  free(inventory_data);
  free(bitmap);

  return 0;
}

/* Load a save game. */
int
load_v0_state(FILE *f) {
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
save_text_write_value(FILE *f, const char *name, int value) {
  fprintf(f, "%s=%i\n", name, value);
}

static void
save_text_write_string(FILE *f, const char *name, const char *value) {
  fprintf(f, "%s=%s\n", name, value);
}

static void
save_text_write_map_pos(FILE *f, const char *name, map_pos_t pos) {
  fprintf(f, "%s=%u,%u\n", name,
    MAP_POS_COL(pos), MAP_POS_ROW(pos));
}

static void
save_text_write_array(FILE *f, const char *name, const int values[],
                      unsigned int size) {
  fprintf(f, "%s=", name);
  for (unsigned int i = 0; i < size - 1; i++) {
    fprintf(f, "%i,", values[i]);
  }
  if (size > 0) fprintf(f, "%i", values[size-1]);
  fprintf(f, "\n");
}

static int
save_text_game_state(FILE *f) {
  fprintf(f, "[game]\n");

  save_text_write_string(f, "version", FREESERF_VERSION);

  save_text_write_value(f, "map.col_size", game.map.col_size);
  save_text_write_value(f, "map.row_size", game.map.row_size);

  save_text_write_value(f, "split", game.split);
  save_text_write_map_pos(f, "update_map_initial_pos",
                          game.update_map_initial_pos);

  save_text_write_value(f, "game_type", game.game_type);
  save_text_write_value(f, "tick", game.tick);
  save_text_write_value(f, "game_stats_counter", game.game_stats_counter);
  save_text_write_value(f, "history_counter", game.history_counter);

  int rnd[3] = { game.rnd.state[0],
           game.rnd.state[1],
           game.rnd.state[2] };
  save_text_write_array(f, "rnd", rnd, 3);

  save_text_write_value(f, "serf_limit", game.serf_limit);

  save_text_write_value(f, "next_index", game.next_index);
  save_text_write_value(f, "flag_search_counter", game.flag_search_counter);
  save_text_write_value(f, "update_map_last_tick", game.update_map_last_tick);
  save_text_write_value(f, "update_map_counter", game.update_map_counter);

  save_text_write_array(f, "player_history_index",
                        game.player_history_index, 4);
  save_text_write_array(f, "player_history_counter",
                        game.player_history_counter, 3);
  save_text_write_value(f, "resource_history_index",
                        game.resource_history_index);

  save_text_write_value(f, "map.regions", game.map_regions);

  save_text_write_value(f, "max_next_index", game.max_next_index);
  save_text_write_value(f, "max_serfs_from_land", game.max_serfs_from_land);
  save_text_write_value(f, "map.gold_deposit", game.map_gold_deposit);
  save_text_write_value(f, "update_map_16_loop", game.update_map_16_loop);

  save_text_write_value(f, "max_serfs_per_player", game.max_serfs_per_player);
  save_text_write_value(f, "map.gold_morale_factor",
                        game.map_gold_morale_factor);
  save_text_write_value(f, "winning_player", game.winning_player);
  save_text_write_value(f, "player_score_leader", game.player_score_leader);
  fprintf(f, "\n");

  return 0;
}

static int
save_text_player_state(FILE *f) {
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

    save_text_write_array(f, "completed_building_count",
                          player->completed_building_count, 23);
    save_text_write_array(f, "incomplete_building_count",
                          player->incomplete_building_count, 23);

    save_text_write_array(f, "inventory_prio", player->inventory_prio, 26);
    save_text_write_array(f, "attacking_buildings",
                          player->attacking_buildings, 64);

    save_text_write_value(f, "initial_supplies", player->initial_supplies);
    save_text_write_value(f, "knights_to_spawn", player->knights_to_spawn);

    save_text_write_value(f, "total_building_score",
                          player->total_building_score);
    save_text_write_value(f, "total_military_score",
                          player->total_military_score);

    save_text_write_value(f, "last_tick", player->last_tick);

    save_text_write_value(f, "reproduction_counter",
                          player->reproduction_counter);
    save_text_write_value(f, "reproduction_reset", player->reproduction_reset);
    save_text_write_value(f, "serf_to_knight_rate",
                          player->serf_to_knight_rate);
    save_text_write_value(f, "serf_to_knight_counter",
                          player->serf_to_knight_counter);

    save_text_write_value(f, "attacking_building_count",
                          player->attacking_building_count);
    save_text_write_array(f, "attacking_knights", player->attacking_knights, 4);
    save_text_write_value(f, "total_attacking_knights",
                          player->total_attacking_knights);
    save_text_write_value(f, "building_attacked", player->building_attacked);
    save_text_write_value(f, "knights_attacking", player->knights_attacking);

    save_text_write_value(f, "food_stonemine", player->food_stonemine);
    save_text_write_value(f, "food_coalmine", player->food_coalmine);
    save_text_write_value(f, "food_ironmine", player->food_ironmine);
    save_text_write_value(f, "food_goldmine", player->food_goldmine);

    save_text_write_value(f, "planks_construction",
                          player->planks_construction);
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
    save_text_write_value(f, "castle_knights_wanted",
                          player->castle_knights_wanted);

    /* TODO */

    fprintf(f, "\n");
  }

  return 0;
}

typedef std::map<std::string, save_writer_text_value_t> values_t;

class save_writer_text_section_t : public save_writer_text_t {
 protected:
  std::string name;
  unsigned int number;
  values_t values;

 public:
  save_writer_text_section_t(std::string name, unsigned int number) {
    this->name = name;
    this->number = number;
  }

  virtual save_writer_text_value_t &value(std::string name) {
    values_t::iterator i = values.find(name);
    if (i != values.end()) {
      return i->second;
    }

    values[name] = save_writer_text_value_t();
    i = values.find(name);
    return i->second;
  }

  bool save(FILE *file) {
    fprintf(file, "[%s %i]\n", name.c_str(), number);

    for (values_t::iterator i = values.begin(); i != values.end(); i++) {
      fprintf(file, "%s=%s\n", i->first.c_str(), i->second.get_value().c_str());
    }

    fprintf(file, "\n");
    return true;
  }
};

static bool
save_text_flag_state(flag_t *flag, FILE *f) {
  save_writer_text_section_t writer("flag", flag->get_index());
  writer << *flag;
  writer.save(f);

  return true;
}

static int
save_text_building_state(building_t *building, FILE *f) {
  save_writer_text_section_t writer("building", building->get_index());
  writer << *building;
  writer.save(f);

  return true;
}

static bool
save_text_inventory_state(inventory_t *inventory, FILE *f) {
  save_writer_text_section_t writer("inventory", inventory->get_index());
  writer << *inventory;
  writer.save(f);

  return true;
}

static int
save_text_serf_state(FILE *f) {
  for (unsigned int i = 1; i < game.max_serf_index; i++) {
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
      save_text_write_value(f, "state.inventory",
                            serf->s.idle_in_stock.inv_index);
      break;

    case SERF_STATE_WALKING:
    case SERF_STATE_TRANSPORTING:
    case SERF_STATE_DELIVERING:
      save_text_write_value(f, "state.res", serf->s.walking.res);
      save_text_write_value(f, "state.dest", serf->s.walking.dest);
      save_text_write_value(f, "state.dir", serf->s.walking.dir);
      save_text_write_value(f, "state.wait_counter",
                            serf->s.walking.wait_counter);
      break;

    case SERF_STATE_ENTERING_BUILDING:
      save_text_write_value(f, "state.field_B",
                            serf->s.entering_building.field_B);
      save_text_write_value(f, "state.slope_len",
                            serf->s.entering_building.slope_len);
      break;

    case SERF_STATE_LEAVING_BUILDING:
    case SERF_STATE_READY_TO_LEAVE:
    case SERF_STATE_KNIGHT_LEAVE_FOR_FIGHT:
      save_text_write_value(f, "state.field_B",
                            serf->s.leaving_building.field_B);
      save_text_write_value(f, "state.dest", serf->s.leaving_building.dest);
      save_text_write_value(f, "state.dest2", serf->s.leaving_building.dest2);
      save_text_write_value(f, "state.dir", serf->s.leaving_building.dir);
      save_text_write_value(f, "state.next_state",
                            serf->s.leaving_building.next_state);
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
      save_text_write_value(f, "state.material_step",
                            serf->s.building.material_step);
      save_text_write_value(f, "state.counter", serf->s.building.counter);
      break;

    case SERF_STATE_BUILDING_CASTLE:
      save_text_write_value(f, "state.inv_index",
                            serf->s.building_castle.inv_index);
      break;

    case SERF_STATE_MOVE_RESOURCE_OUT:
    case SERF_STATE_DROP_RESOURCE_OUT:
      save_text_write_value(f, "state.res", serf->s.move_resource_out.res);
      save_text_write_value(f, "state.res_dest",
                            serf->s.move_resource_out.res_dest);
      save_text_write_value(f, "state.next_state",
                            serf->s.move_resource_out.next_state);
      break;

    case SERF_STATE_READY_TO_LEAVE_INVENTORY:
      save_text_write_value(f, "state.mode",
                            serf->s.ready_to_leave_inventory.mode);
      save_text_write_value(f, "state.dest",
                            serf->s.ready_to_leave_inventory.dest);
      save_text_write_value(f, "state.inv_index",
                            serf->s.ready_to_leave_inventory.inv_index);
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
      save_text_write_value(f, "state.neg_dist",
                            serf->s.free_walking.neg_dist1);
      save_text_write_value(f, "state.neg_dist2",
                            serf->s.free_walking.neg_dist2);
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
      save_text_write_value(f, "state.dist_col",
                            serf->s.defending_free.dist_col);
      save_text_write_value(f, "state.dist_row",
                            serf->s.defending_free.dist_row);
      save_text_write_value(f, "state.field_D", serf->s.defending_free.field_D);
      save_text_write_value(f, "state.other_dist_col",
                            serf->s.defending_free.other_dist_col);
      save_text_write_value(f, "state.other_dist_row",
                            serf->s.defending_free.other_dist_row);
      break;

    case SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT:
      save_text_write_value(f, "state.dist_col",
                            serf->s.leave_for_walk_to_fight.dist_col);
      save_text_write_value(f, "state.dist_row",
                            serf->s.leave_for_walk_to_fight.dist_row);
      save_text_write_value(f, "state.field_D",
                            serf->s.leave_for_walk_to_fight.field_D);
      save_text_write_value(f, "state.field_E",
                            serf->s.leave_for_walk_to_fight.field_E);
      save_text_write_value(f, "state.next_state",
                            serf->s.leave_for_walk_to_fight.next_state);
      break;

    case SERF_STATE_IDLE_ON_PATH:
    case SERF_STATE_WAIT_IDLE_ON_PATH:
    case SERF_STATE_WAKE_AT_FLAG:
    case SERF_STATE_WAKE_ON_PATH:
      save_text_write_value(f, "state.rev_dir", serf->s.idle_on_path.rev_dir);
      save_text_write_value(f, "state.flag",
                            serf->s.idle_on_path.flag->get_index());
      save_text_write_value(f, "state.field_E", serf->s.idle_on_path.field_E);
      break;

    case SERF_STATE_DEFENDING_HUT:
    case SERF_STATE_DEFENDING_TOWER:
    case SERF_STATE_DEFENDING_FORTRESS:
    case SERF_STATE_DEFENDING_CASTLE:
      save_text_write_value(f, "state.next_knight",
                            serf->s.defending.next_knight);
      break;

    default: break;
    }

    fprintf(f, "\n");
  }

  return 0;
}

static int
save_text_map_state(FILE *f) {
  int height[SAVE_MAP_TILE_COUNT];
  int type_up[SAVE_MAP_TILE_COUNT];
  int type_down[SAVE_MAP_TILE_COUNT];
  int paths[SAVE_MAP_TILE_COUNT];
  int obj[SAVE_MAP_TILE_COUNT];
  int serfs[SAVE_MAP_TILE_COUNT];
  int resources[SAVE_MAP_TILE_COUNT];
  int resource_type[SAVE_MAP_TILE_COUNT];

  for (unsigned int ty = 0; ty < game.map.rows; ty += SAVE_MAP_TILE_SIZE) {
    for (unsigned int tx = 0; tx < game.map.cols; tx += SAVE_MAP_TILE_SIZE) {
      map_pos_t pos = MAP_POS(tx, ty);

      fprintf(f, "[map]\n");

      save_text_write_map_pos(f, "pos", pos);

      for (int y = 0; y < SAVE_MAP_TILE_SIZE; y++) {
        for (int x = 0; x < SAVE_MAP_TILE_SIZE; x++) {
          map_pos_t pos = MAP_POS(tx+x, ty+y);
          int i = y*SAVE_MAP_TILE_SIZE + x;
          height[i] = MAP_HEIGHT(pos);
          type_up[i] = MAP_TYPE_UP(pos);
          type_down[i] = MAP_TYPE_DOWN(pos);
          paths[i] = MAP_PATHS(pos);
          obj[i] = MAP_OBJ(pos);
          serfs[i] = MAP_SERF_INDEX(pos);

          if (MAP_IN_WATER(pos)) {
            resource_type[i] = 0;
            resources[i] = MAP_RES_FISH(pos);
          } else {
            resource_type[i] = MAP_RES_TYPE(pos);
            resources[i] = MAP_RES_AMOUNT(pos);
          }
        }
      }

      save_text_write_array(f, "height", height, SAVE_MAP_TILE_COUNT);
      save_text_write_array(f, "type.up", type_up, SAVE_MAP_TILE_COUNT);
      save_text_write_array(f, "type.down", type_down, SAVE_MAP_TILE_COUNT);
      save_text_write_array(f, "paths", paths, SAVE_MAP_TILE_COUNT);
      save_text_write_array(f, "object", obj, SAVE_MAP_TILE_COUNT);
      save_text_write_array(f, "serf", serfs, SAVE_MAP_TILE_COUNT);
      save_text_write_array(f, "resource.type",
                            resource_type, SAVE_MAP_TILE_COUNT);
      save_text_write_array(f, "resource.amount",
                            resources, SAVE_MAP_TILE_COUNT);

      fprintf(f, "\n");
    }
  }

  return 0;
}

int
save_text_state(FILE *f) {
  int r;

  r = save_text_game_state(f);
  if (r < 0) return -1;

  r = save_text_player_state(f);
  if (r < 0) return -1;

  for (flags_t::iterator i = game.flags.begin();
       i != game.flags.end(); ++i) {
    if (!save_text_flag_state(*i, f)) return -1;
  }

  for (buildings_t::iterator i = game.buildings.begin();
       i != game.buildings.end(); ++i) {
    if (!save_text_building_state(*i, f)) return -1;
  }

  for (inventories_t::iterator i = game.inventories.begin();
       i != game.inventories.end(); ++i) {
    if (!save_text_inventory_state(*i, f)) return -1;
  }

  r = save_text_serf_state(f);
  if (r < 0) return -1;

  r = save_text_map_state(f);
  if (r < 0) return -1;

  return 0;
}


static char *
trim_whitespace(char *str, size_t *len) {
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
load_text_readline(char **buffer, size_t *len, FILE *f) {
  size_t i = 0;

  while (1) {
    if (i >= *len-1) {
      /* Double buffer size, always keep enough
         space for input character plus zero-byte. */
      *len = 2*(*len);
      *buffer = reinterpret_cast<char*>(realloc(*buffer, *len));
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
  char *key;
  char *value;
} setting_t;

typedef struct {
  list_elm_t elm;
  char *name;
  char *param;
  list_t settings;
} section_t;

static int
load_text_parse(FILE *f, list_t *sections) {
  size_t buffer_len = 256;
  char *buffer = reinterpret_cast<char*>(malloc(buffer_len*sizeof(char)));

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
      section = reinterpret_cast<section_t*>(malloc(sizeof(section_t)));
      if (section == NULL) abort();

      section->name = strdup(header);
      section->param = strdup(param);
      list_init(&section->settings);
      list_append(sections, reinterpret_cast<list_elm_t*>(section));
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

      setting_t *setting =
                        reinterpret_cast<setting_t*>(malloc(sizeof(setting_t)));
      if (setting == NULL) abort();

      setting->key = strdup(key);
      setting->value = strdup(value);
      list_append(&section->settings, reinterpret_cast<list_elm_t*>(setting));
    }
  }

  free(buffer);

  return 0;
}

static void
load_text_free_sections(list_t *sections) {
  while (!list_is_empty(sections)) {
    section_t *section =
                       reinterpret_cast<section_t*>(list_remove_head(sections));
    free(section->name);
    free(section->param);

    while (!list_is_empty(&section->settings)) {
      setting_t *setting =
             reinterpret_cast<setting_t*>(list_remove_head(&section->settings));
      free(setting->key);
      free(setting->value);
      free(setting);
    }

    free(section);
  }
}

static char *
load_text_get_setting(const section_t *section, const char *key) {
  list_elm_t *elm;
  list_foreach(&section->settings, elm) {
    setting_t *s = reinterpret_cast<setting_t*>(elm);
    if (!strcmp(s->key, key)) {
      return s->value;
    }
  }

  return NULL;
}

static map_pos_t
parse_map_pos(char *str) {
  char *s = strchr(str, ',');
  if (s == NULL) return 0;

  s[0] = '\0';
  s += 1;

  return MAP_POS(atoi(str), atoi(s));
}

static char *
parse_array_value(char **str) {
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
load_text_game_state(list_t *sections) {
  const char *value;

  /* Find the game section */
  section_t *section = NULL;
  list_elm_t *elm;
  list_foreach(sections, elm) {
    section_t *s = reinterpret_cast<section_t*>(elm);
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
    setting_t *s = reinterpret_cast<setting_t*>(elm);
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
load_text_player_section(section_t *section) {
  /* Parse player number. */
  int n = atoi(section->param);
  if (n < 0 || n >= GAME_MAX_PLAYER_COUNT) return -1;

  game.player[n] = reinterpret_cast<player_t*>(calloc(1, sizeof(player_t)));
  if (game.player[n] == NULL) return -1;

  player_t *player = game.player[n];
  player->player_num = n;

  /* Load the player state. */
  list_elm_t *elm;
  list_foreach(&section->settings, elm) {
    setting_t *s = reinterpret_cast<setting_t*>(elm);
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

  player->timers_count = 0;

  return 0;
}

static int
load_text_player_state(list_t *sections) {
  list_elm_t *elm;
  list_foreach(sections, elm) {
    section_t *s = reinterpret_cast<section_t*>(elm);
    if (!strcmp(s->name, "player")) {
      int r = load_text_player_section(s);
      if (r < 0) return -1;
    }
  }

  return 0;
}

class save_reader_text_section_t : public save_reader_text_t {
 protected:
  section_t *section;

 public:
  explicit save_reader_text_section_t(section_t *section) {
    this->section = section; }

  virtual save_reader_text_value_t value(std::string name) const {
    list_elm_t *elm;
    list_foreach(&section->settings, elm) {
      setting_t *s = reinterpret_cast<setting_t*>(elm);
      if (name == s->key) {
        return save_reader_text_value_t(s->value);
      }
    }

    return save_reader_text_value_t("");
  }
};

static int
load_text_flag_section(section_t *section) {
  /* Parse flag number. */
  int n = atoi(section->param);

  save_reader_text_section_t reader(section);

  flag_t *flag = game.flags.get_or_insert(n);
  reader >> *flag;

  return 0;
}

static int
load_text_flag_state(list_t *sections) {
  list_elm_t *elm;
  list_foreach(sections, elm) {
    section_t *s = reinterpret_cast<section_t*>(elm);
    if (!strcmp(s->name, "flag")) {
      int r = load_text_flag_section(s);
      if (r < 0) return -1;
    }
  }

  return 0;
}

static int
load_text_building_section(section_t *section) {
  /* Parse building number. */
  int n = atoi(section->param);

  save_reader_text_section_t reader(section);

  building_t *building = game.buildings.get_or_insert(n);
  reader >> *building;

  return 0;
}

static int
load_text_building_state(list_t *sections) {
  list_elm_t *elm;
  list_foreach(sections, elm) {
    section_t *s = reinterpret_cast<section_t*>(elm);
    if (!strcmp(s->name, "building")) {
      int r = load_text_building_section(s);
      if (r < 0) return -1;
    }
  }

  for (buildings_t::iterator i = game.buildings.begin();
       i != game.buildings.end(); ++i) {
    /* Restore pointer to castle flag */
    building_t *building = *i;
    if (building->get_type() == BUILDING_CASTLE) {
      game.player[building->get_player()]->castle_flag =
                                                     building->get_flag_index();
      break;
    }
  }

  return 0;
}

static int
load_text_inventory_section(section_t *section) {
  /* Parse building number. */
  int n = atoi(section->param);

  save_reader_text_section_t reader(section);

  inventory_t *inventory = game.inventories.get_or_insert(n);
  reader >> *inventory;

  return 0;
}

static int
load_text_inventory_state(list_t *sections) {
  list_elm_t *elm;
  list_foreach(sections, elm) {
    section_t *s = reinterpret_cast<section_t*>(elm);
    if (!strcmp(s->name, "inventory")) {
      int r = load_text_inventory_section(s);
      if (r < 0) return -1;
    }
  }

  return 0;
}

static int
load_text_serf_section(section_t *section) {
  /* Parse serf number. */
  int n = atoi(section->param);
  if (n >= static_cast<int>(game.serf_limit)) return -1;

  serf_t *serf = &game.serfs[n];
  game.serf_bitmap[n/8] |= BIT(7-(n&7));

  /* Load the serf state. */
  list_elm_t *elm;
  list_foreach(&section->settings, elm) {
    setting_t *s = reinterpret_cast<setting_t*>(elm);
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
      serf->state = (serf_state_t)atoi(s->value);
    } else if (!strncmp(s->key, "state.", strlen("state."))) {
      /* Handled later */
    } else {
      LOGD("savegame", "Unhandled serf setting: `%s'.", s->key);
    }
  }

  /* Load state variables */
  list_foreach(&section->settings, elm) {
    setting_t *s = reinterpret_cast<setting_t*>(elm);
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
        serf->s.leaving_building.next_state = (serf_state_t)atoi(s->value);
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
        serf->s.move_resource_out.next_state = (serf_state_t)atoi(s->value);
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
        serf->s.mining.deposit = (ground_deposit_t)atoi(s->value);
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
        serf->s.leave_for_walk_to_fight.next_state =
                                                   (serf_state_t)atoi(s->value);
      }
      break;

    case SERF_STATE_IDLE_ON_PATH:
    case SERF_STATE_WAIT_IDLE_ON_PATH:
    case SERF_STATE_WAKE_AT_FLAG:
    case SERF_STATE_WAKE_ON_PATH:
      if (!strcmp(s->key, "state.rev_dir")) {
        serf->s.idle_on_path.rev_dir = (dir_t)atoi(s->value);
      } else if (!strcmp(s->key, "state.flag")) {
        serf->s.idle_on_path.flag = game.flags[atoi(s->value)];
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
load_text_serf_state(list_t *sections) {
  list_elm_t *elm;
  list_foreach(sections, elm) {
    section_t *s = reinterpret_cast<section_t*>(elm);
    if (!strcmp(s->name, "serf")) {
      int r = load_text_serf_section(s);
      if (r < 0) return -1;
    }
  }

  for (unsigned int i = 0; i < game.serf_limit; i++) {
    if (SERF_ALLOCATED(i)) {
      /* Restore max serf index */
      game.max_serf_index = i+1;
    }
  }

  return 0;
}

static int
load_text_map_section(section_t *section) {
  char *value = load_text_get_setting(section, "pos");
  if (value == NULL) return -1;

  map_pos_t pos = parse_map_pos(value);

  map_tile_t *tiles = game.map.tiles;

  /* Load the map tile. */
  list_elm_t *elm;
  list_foreach(&section->settings, elm) {
    setting_t *s = reinterpret_cast<setting_t*>(elm);
    if (!strcmp(s->key, "pos")) {
      /* Already handled */
    } else if (!strcmp(s->key, "paths")) {
      char *array = s->value;
      for (int y = 0; y < SAVE_MAP_TILE_SIZE; y++) {
        for (int x = 0; x < SAVE_MAP_TILE_SIZE; x++) {
          if (array == NULL) return -1;
          map_pos_t p = MAP_POS_ADD(pos, MAP_POS(x, y));
          char *v = parse_array_value(&array);
          tiles[p].paths = atoi(v) & 0x3f;
        }
      }
    } else if (!strcmp(s->key, "height")) {
      char *array = s->value;
      for (int y = 0; y < SAVE_MAP_TILE_SIZE; y++) {
        for (int x = 0; x < SAVE_MAP_TILE_SIZE; x++) {
          if (array == NULL) return -1;
          map_pos_t p = MAP_POS_ADD(pos, MAP_POS(x, y));
          char *v = parse_array_value(&array);
          tiles[p].height = atoi(v) & 0x1f;
        }
      }
    } else if (!strcmp(s->key, "type.up")) {
      char *array = s->value;
      for (int y = 0; y < SAVE_MAP_TILE_SIZE; y++) {
        for (int x = 0; x < SAVE_MAP_TILE_SIZE; x++) {
          if (array == NULL) return -1;
          map_pos_t p = MAP_POS_ADD(pos, MAP_POS(x, y));
          char *v = parse_array_value(&array);
          tiles[p].type = ((atoi(v) & 0xf) << 4) | (tiles[p].type & 0xf);
        }
      }
    } else if (!strcmp(s->key, "type.down")) {
      char *array = s->value;
      for (int y = 0; y < SAVE_MAP_TILE_SIZE; y++) {
        for (int x = 0; x < SAVE_MAP_TILE_SIZE; x++) {
          if (array == NULL) return -1;
          map_pos_t p = MAP_POS_ADD(pos, MAP_POS(x, y));
          char *v = parse_array_value(&array);
          tiles[p].type = (tiles[p].type & 0xf0) | (atoi(v) & 0xf);
        }
      }
    } else if (!strcmp(s->key, "object")) {
      char *array = s->value;
      for (int y = 0; y < SAVE_MAP_TILE_SIZE; y++) {
        for (int x = 0; x < SAVE_MAP_TILE_SIZE; x++) {
          if (array == NULL) return -1;
          map_pos_t p = MAP_POS_ADD(pos, MAP_POS(x, y));
          char *v = parse_array_value(&array);
          tiles[p].obj = atoi(v) & 0x7f;
        }
      }
    } else if (!strcmp(s->key, "serf")) {
      char *array = s->value;
      for (int y = 0; y < SAVE_MAP_TILE_SIZE; y++) {
        for (int x = 0; x < SAVE_MAP_TILE_SIZE; x++) {
          if (array == NULL) return -1;
          map_pos_t p = MAP_POS_ADD(pos, MAP_POS(x, y));
          char *v = parse_array_value(&array);
          tiles[p].serf = atoi(v);
        }
      }
    } else if (!strcmp(s->key, "resource.type")) {
      char *array = s->value;
      for (int y = 0; y < SAVE_MAP_TILE_SIZE; y++) {
        for (int x = 0; x < SAVE_MAP_TILE_SIZE; x++) {
          if (array == NULL) return -1;
          map_pos_t p = MAP_POS_ADD(pos, MAP_POS(x, y));
          char *v = parse_array_value(&array);
          tiles[p].resource = ((atoi(v) & 7) << 5) | (tiles[p].resource & 0x1f);
        }
      }
    } else if (!strcmp(s->key, "resource.amount")) {
      char *array = s->value;
      for (int y = 0; y < SAVE_MAP_TILE_SIZE; y++) {
        for (int x = 0; x < SAVE_MAP_TILE_SIZE; x++) {
          if (array == NULL) return -1;
          map_pos_t p = MAP_POS_ADD(pos, MAP_POS(x, y));
          char *v = parse_array_value(&array);
          tiles[p].resource = (tiles[p].resource & 0xe0) | (atoi(v) & 0x1f);
        }
      }
    } else {
      LOGD("savegame", "Unhandled map setting: `%s'.", s->key);
    }
  }

  return 0;
}

static int
load_text_map_state(list_t *sections) {
  list_elm_t *elm;
  list_foreach(sections, elm) {
    section_t *s = reinterpret_cast<section_t*>(elm);
    if (!strcmp(s->name, "map")) {
      int r = load_text_map_section(s);
      if (r < 0) return -1;
    }
  }

  /* Restore idle serf flag */
  for (unsigned int i = 1; i < game.max_serf_index; i++) {
    if (!SERF_ALLOCATED(i)) continue;

    serf_t *serf = game_get_serf(i);
    if (serf->state == SERF_STATE_IDLE_ON_PATH ||
        serf->state == SERF_STATE_WAIT_IDLE_ON_PATH) {
      game.map.tiles[serf->pos].obj |= BIT(7);
    }
  }

  /* Restore building index */
  for (buildings_t::iterator i = game.buildings.begin();
       i != game.buildings.end(); ++i) {
    building_t *building = *i;
    if (MAP_OBJ(building->get_position()) < MAP_OBJ_SMALL_BUILDING ||
        MAP_OBJ(building->get_position()) > MAP_OBJ_CASTLE) {
      return -1;
    }

    game.map.tiles[building->get_position()].obj_index = building->get_index();
  }

  /* Restore flag index */
  for (flags_t::iterator i = game.flags.begin();
       i != game.flags.end(); ++i) {
    flag_t *flag = *i;

    if (MAP_OBJ(flag->get_position()) != MAP_OBJ_FLAG) {
      return false;
    }

    game.map.tiles[flag->get_position()].obj_index = flag->get_index();
  }

  return 0;
}

int
load_text_state(FILE *f) {
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
load_state(const char *path) {
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
save_state(const char *path) {
  FILE *f = fopen(path, "wb");
  if (f == NULL) return -1;

  int r = save_text_state(f);
  fclose(f);

  return r;
}

save_reader_binary_t::save_reader_binary_t(void *data, size_t size) {
  current = reinterpret_cast<uint8_t*>(data);
  end = current + size;
}

save_reader_binary_t& save_reader_binary_t::operator >> (uint8_t &val) {
  val = *current;
  current++;
  return *this;
}

save_reader_binary_t& save_reader_binary_t::operator >> (uint16_t &val) {
  val = *reinterpret_cast<uint16_t*>(current);
  current += 2;
  return *this;
}

save_reader_binary_t& save_reader_binary_t::operator >> (uint32_t &val) {
  val = *reinterpret_cast<uint32_t*>(current);
  current += 4;
  return *this;
}

save_reader_text_value_t::save_reader_text_value_t(std::string value) {
  this->value = value;
}

save_reader_text_value_t&
save_reader_text_value_t::operator >> (int &val) {
  int result = atoi(value.c_str());
  val = result;

  return *this;
}

save_reader_text_value_t&
save_reader_text_value_t::operator >> (unsigned int &val) {
  int result = atoi(value.c_str());
  val = result;

  return *this;
}

save_reader_text_value_t&
save_reader_text_value_t::operator >> (dir_t &val) {
  int result = atoi(value.c_str());
  val = (dir_t)result;

  return *this;
}

save_reader_text_value_t&
save_reader_text_value_t::operator >> (resource_type_t &val) {
  int result = atoi(value.c_str());
  val = (resource_type_t)result;

  return *this;
}

save_reader_text_value_t&
save_reader_text_value_t::operator >> (building_type_t &val) {
  int result = atoi(value.c_str());
  val = (building_type_t)result;

  return *this;
}

save_reader_text_value_t
save_reader_text_value_t::operator[] (size_t pos) {
  std::vector<std::string> parts;
  std::istringstream iss(value);
  std::string item;
  while (std::getline(iss, item, ',')) {
    parts.push_back(item);
  }

  if (pos >= parts.size()) {
    return save_reader_text_value_t("");
  }

  return save_reader_text_value_t(parts[pos]);
}

save_writer_text_value_t&
save_writer_text_value_t::operator << (int val) {
  if (!value.empty()) {
    value += ",";
  }

  std::ostringstream ss;
  ss << val;
  value += ss.str();

  return *this;
}

save_writer_text_value_t&
save_writer_text_value_t::operator << (unsigned int val) {
  if (!value.empty()) {
    value += ",";
  }

  std::ostringstream ss;
  ss << val;
  value += ss.str();

  return *this;
}

save_writer_text_value_t&
save_writer_text_value_t::operator << (dir_t val) {
  if (!value.empty()) {
    value += ",";
  }

  std::ostringstream ss;
  ss << static_cast<int>(val);
  value += ss.str();

  return *this;
}

save_writer_text_value_t&
save_writer_text_value_t::operator << (resource_type_t val) {
  if (!value.empty()) {
    value += ",";
  }

  std::ostringstream ss;
  ss << static_cast<int>(val);
  value += ss.str();

  return *this;
}
