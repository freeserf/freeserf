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
static unsigned int max_serf_index = 0;

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
//  const int max_map_size = 10;
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

  game.rnd = random_state_t(*reinterpret_cast<uint16_t*>(&data[84]),
                            *reinterpret_cast<uint16_t*>(&data[86]),
                            *reinterpret_cast<uint16_t*>(&data[88]));

  max_flag_index = *reinterpret_cast<uint16_t*>(&data[90]);
  max_building_index = *reinterpret_cast<uint16_t*>(&data[92]);
  max_serf_index = *reinterpret_cast<uint16_t*>(&data[94]);

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
//  game.max_serfs_from_land = *(uint16_t *)&data[182];
  game.map_gold_deposit = *reinterpret_cast<uint32_t*>(&data[184]);
  game.update_map_16_loop = *reinterpret_cast<uint16_t*>(&data[188]);

  /*
  game.map_field_52 = *(uint16_t *)&data[192];
  game.field_54 = *(uint16_t *)&data[194];
  game.field_56 = *(uint16_t *)&data[196];
  */

//  game.max_serfs_per_player = *(uint16_t *)&data[198];
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
  uint8_t *data = reinterpret_cast<uint8_t*>(malloc(8628));
  if (data == NULL) return -1;

  for (int i = 0; i < 4; i++) {
    size_t rd = fread(data, sizeof(uint8_t), 8628, f);
    if (rd < 8628) {
      free(data);
      return -1;
    }

    if (!BIT_TEST(data[130], 6)) continue;

    player_t *player = game.players.get_or_insert(i);
    save_reader_binary_t reader(data, 8628);
    reader >> *player;
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
  int bitmap_size = 4*((max_serf_index + 31)/32);
  uint8_t *bitmap = reinterpret_cast<uint8_t*>(malloc(bitmap_size));
  if (bitmap == NULL) return -1;

  size_t rd = fread(bitmap, sizeof(uint8_t), bitmap_size, f);
  if (rd < (size_t)bitmap_size) {
    free(bitmap);
    return -1;
  }

  /* Load serf data. */
  void *data = malloc(16);
  if (data == NULL) {
    free(bitmap);
    return -1;
  }

  for (unsigned int i = 0; i < max_serf_index; i++) {
    rd = fread(data, 16, 1, f);
    if (rd != 1) {
      free(data);
      free(bitmap);
      return -1;
    }

    if (BIT_TEST(bitmap[(i)>>3], 7-((i)&7))) {
      serf_t *serf = game.serfs.get_or_insert(i);
      save_reader_binary_t reader(data, 16);
      reader >> *serf;
    }
  }

  free(data);
  free(bitmap);

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

  std::string str = game.rnd;
  save_text_write_string(f, "rnd", str.c_str());

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
  save_text_write_value(f, "map.gold_deposit", game.map_gold_deposit);
  save_text_write_value(f, "update_map_16_loop", game.update_map_16_loop);

  save_text_write_value(f, "map.gold_morale_factor",
                        game.map_gold_morale_factor);
  save_text_write_value(f, "winning_player", game.winning_player);
  save_text_write_value(f, "player_score_leader", game.player_score_leader);
  fprintf(f, "\n");

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
save_text_player_state(FILE *f) {
  int i = 0;
  for (players_t::iterator p = game.players.begin();
       p != game.players.end(); ++p) {
    save_writer_text_section_t writer("player", i);
    writer << **p;
    writer.save(f);
    i++;
  }

  return true;
}

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
save_text_serf_state(serf_t *serf, FILE *f) {
  save_writer_text_section_t writer("serf", serf->get_index());
  writer << *serf;
  writer.save(f);

  return true;
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

  for (serfs_t::iterator i = game.serfs.begin();
       i != game.serfs.end(); ++i) {
    if (!save_text_serf_state(*i, f)) return -1;
  }

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
      game.rnd = random_state_t(s->value);
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
    } else if (!strcmp(s->key, "map.gold_deposit")) {
      game.map_gold_deposit = atoi(s->value);
    } else if (!strcmp(s->key, "update_map_16_loop")) {
      game.update_map_16_loop = atoi(s->value);
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

class save_reader_text_section_t : public save_reader_text_t {
 protected:
  section_t *section;

 public:
  explicit save_reader_text_section_t(section_t *section) {
    this->section = section;
  }

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
load_text_player_section(section_t *section) {
  /* Parse player number. */
  int n = atoi(section->param);
  if (n < 0) return -1;

  player_t *player = game.players.get_or_insert(n);

  save_reader_text_section_t reader(section);
  reader >> *player;

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

  player_t::restore_castle_flag();

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

  save_reader_text_section_t reader(section);

  serf_t *serf = game.serfs.get_or_insert(n);
  reader >> *serf;

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
  for (serfs_t::iterator i = game.serfs.begin();
       i != game.serfs.end(); ++i) {
    serf_t *serf = *i;
    if (serf->get_state() == SERF_STATE_IDLE_ON_PATH ||
        serf->get_state() == SERF_STATE_WAIT_IDLE_ON_PATH) {
      game.map.tiles[serf->get_pos()].obj |= BIT(7);
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

save_reader_text_value_t&
save_reader_text_value_t::operator >> (serf_state_t &val) {
  int result = atoi(value.c_str());
  val = (serf_state_t)result;

  return *this;
}

save_reader_text_value_t&
save_reader_text_value_t::operator >> (uint16_t &val) {
  int result = atoi(value.c_str());
  val = (uint16_t)result;

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
