/*
 * game.h - Gameplay related functions
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

#ifndef SRC_GAME_H_
#define SRC_GAME_H_

#include <vector>
#include <map>
#include <string>
#include <list>

#include "src/player.h"
#include "src/flag.h"
#include "src/serf.h"
#include "src/map.h"
#include "src/random.h"
#include "src/objects.h"
#include "src/event_loop.h"

#define DEFAULT_GAME_SPEED  2

#define GAME_MAX_PLAYER_COUNT  4

typedef collection_t<flag_t> flags_t;
typedef collection_t<inventory_t> inventories_t;
typedef collection_t<building_t> buildings_t;
typedef collection_t<serf_t> serfs_t;
typedef collection_t<player_t> players_t;

typedef std::list<serf_t*> list_serfs_t;
typedef std::list<building_t*> list_buildings_t;
typedef std::list<inventory_t*> list_inventories_t;

class save_reader_binary_t;
class save_reader_text_t;
class save_writer_text_t;

class game_t : public event_handler_t {
 protected:
  map_t *map;

  typedef std::map<unsigned int, unsigned int> values_t;
  int map_gold_morale_factor;

  players_t players;
  flags_t flags;
  inventories_t inventories;
  buildings_t buildings;
  serfs_t serfs;

  random_state_t init_map_rnd;
  unsigned int game_speed_save;
  unsigned int game_speed;
  unsigned int tick;
  unsigned int last_tick;
  unsigned int const_tick;
  unsigned int game_stats_counter;
  unsigned int history_counter;
  random_state_t rnd;
  uint16_t next_index;
  uint16_t flag_search_counter;

  uint16_t update_map_last_tick;
  int16_t update_map_counter;
  map_pos_t update_map_initial_pos;
  int tick_diff;
  uint16_t max_next_index;
  int16_t update_map_16_loop;
  int player_history_index[4];
  int player_history_counter[3];
  int resource_history_index;
  uint16_t field_340;
  uint16_t field_342;
  inventory_t *field_344;
  int game_type;
  int tutorial_level;
  int mission_level;
  int map_generator;
  int map_preserve_bugs;
  int player_score_leader;

  int knight_morale_counter;
  int inventory_schedule_counter;

 public:
  explicit game_t(int map_generator);
  virtual ~game_t();

  map_t *get_map() { return map; }

  unsigned int get_tick() const { return tick; }
  unsigned int get_const_tick() const { return const_tick; }
  unsigned int get_gold_morale_factor() const { return map_gold_morale_factor; }

  building_t *get_building_at_pos(map_pos_t pos);
  flag_t *get_flag_at_pos(map_pos_t pos);
  serf_t *get_serf_at_pos(map_pos_t pos);

  /* External interface */
  void init();
  unsigned int add_player(size_t face, unsigned int color, size_t supplies,
                          size_t reproduction, size_t intelligence);
  bool load_mission_map(int m);
  bool load_random_map(int size, const random_state_t &rnd);
  bool load_save_game(const std::string &path);

  void update();
  void pause();
  void speed_increase();
  void speed_decrease();
  void speed_reset();

  void prepare_ground_analysis(map_pos_t pos, int estimates[5]);
  bool send_geologist(flag_t *dest);

  int get_leveling_height(map_pos_t pos);

  bool can_build_military(map_pos_t pos);
  bool can_build_small(map_pos_t pos);
  bool can_build_mine(map_pos_t pos);
  bool can_build_large(map_pos_t pos);
  bool can_build_building(map_pos_t pos, building_type_t type,
                          const player_t *player);
  bool can_build_castle(map_pos_t pos, const player_t *player);
  bool can_build_flag(map_pos_t pos, const player_t *player);
  bool can_player_build(map_pos_t pos, const player_t *player);

  int can_build_road(const road_t &road, const player_t *player,
                     map_pos_t *dest, bool *water);

  bool can_demolish_flag(map_pos_t pos, const player_t *player);
  bool can_demolish_road(map_pos_t pos, const player_t *player);

  bool build_road(const road_t &road, const player_t *player);

  bool build_flag(map_pos_t pos, player_t *player);
  bool build_building(map_pos_t pos, building_type_t type, player_t *player);
  bool build_castle(map_pos_t pos, player_t *player);

  bool demolish_road(map_pos_t pos, player_t *player);
  bool demolish_flag(map_pos_t pos, player_t *player);
  bool demolish_building(map_pos_t pos, player_t *player);

  void set_inventory_resource_mode(inventory_t *inventory, int mode);
  void set_inventory_serf_mode(inventory_t *inventory, int mode);


  /* Internal interface */
  void calculate_military_flag_state(building_t *building);
  void init_land_ownership();
  void update_land_ownership(map_pos_t pos);
  void occupy_enemy_building(building_t *building, int player);

  void cancel_transported_resource(resource_type_t type, unsigned int dest);
  void lose_resource(resource_type_t type);

  uint16_t random_int();

  bool send_serf_to_flag(flag_t *dest, serf_type_t type, resource_type_t res1,
                         resource_type_t res2);

  int get_player_history_index(size_t scale) const {
    return player_history_index[scale]; }
  int get_resource_history_index() const { return resource_history_index; }

  int next_search_id();

  serf_t *create_serf(int index = -1);
  void delete_serf(serf_t *serf);
  flag_t *create_flag(int index = -1);
  inventory_t *create_inventory(int index = -1);
  building_t *create_building(int index = -1);
  void delete_building(building_t *building);

  serf_t *get_serf(unsigned int index) { return serfs[index]; }
  flag_t *get_flag(unsigned int index) { return flags[index]; }
  inventory_t *get_inventory(unsigned int index) { return inventories[index]; }
  building_t *get_building(unsigned int index) { return buildings[index]; }
  player_t *get_player(unsigned int index) { return players[index]; }

  list_serfs_t get_player_serfs(player_t *player);
  list_buildings_t get_player_buildings(player_t *player);
  list_serfs_t get_serfs_in_inventory(inventory_t *inventory);
  list_serfs_t get_serfs_related_to(unsigned int dest, dir_t dir);
  list_inventories_t get_player_inventories(player_t *player);

  list_serfs_t get_serfs_at_pos(map_pos_t pos);
  flag_t *gat_flag_at_pos(map_pos_t pos);

  player_t *get_next_player(player_t *player);
  unsigned int get_enemy_score(player_t *player);
  void building_captured(building_t *building);
  void clear_search_id();

 protected:
  void allocate_objects();
  void deinit();

  void clear_serf_request_failure();
  void update_knight_morale();
  static bool update_inventories_cb(flag_t *flag, void *data);
  void update_inventories();
  void update_flags();
  static bool send_serf_to_flag_search_cb(flag_t *flag, void *data);
  void update_buildings();
  void update_serfs();
  void record_player_history(int max_level, int aspect,
                             const int history_index[], const values_t &values);
  int calculate_clear_winner(const values_t &values);
  void update_game_stats();
  void get_resource_estimate(map_pos_t pos, int weight, int estimates[5]);
  int road_segment_in_water(map_pos_t pos, dir_t dir);
  void flag_reset_transport(flag_t *flag);
  void building_remove_player_refs(building_t *building);
  bool path_serf_idle_to_wait_state(map_pos_t pos);
  void remove_road_forwards(map_pos_t pos, dir_t dir);
  bool demolish_road_(map_pos_t pos);
  void build_flag_split_path(map_pos_t pos);
  bool map_types_within(map_pos_t pos, map_terrain_t low, map_terrain_t high);
  void flag_remove_player_refs(flag_t *flag);
  bool demolish_flag_(map_pos_t pos);
  bool demolish_building_(map_pos_t pos);
  void surrender_land(map_pos_t pos);
  void demolish_flag_and_roads(map_pos_t pos);
  void init_map(int size, const random_state_t &rnd, bool preserve_bugs);

 public:
  virtual bool handle_event(const event_t *event);

  friend save_reader_binary_t&
    operator >> (save_reader_binary_t &reader, game_t &game);
  friend save_reader_text_t&
    operator >> (save_reader_text_t &reader, game_t &game);
  friend save_writer_text_t&
    operator << (save_writer_text_t &writer, game_t &game);

 protected:
  bool load_serfs(save_reader_binary_t *reader, int max_serf_index);
  bool load_flags(save_reader_binary_t *reader, int max_flag_index);
  bool load_buildings(save_reader_binary_t *reader, int max_building_index);
  bool load_inventories(save_reader_binary_t *reader, int max_inventory_index);
};

#endif  // SRC_GAME_H_
