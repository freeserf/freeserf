/*
 * serf.h - Serf related functions
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

#ifndef SRC_SERF_H_
#define SRC_SERF_H_

#include <map>

#include "src/map.h"
#include "src/resource.h"
#include "src/misc.h"
#include "src/objects.h"

#define serf_log_state_change(serf, new_state)  \
  LOGV("serf", "serf %i (%s): state %s -> %s (%s:%i)", serf->get_index(), \
       serf_t::get_type_name(serf->get_type()), \
       serf_t::get_state_name((serf)->state),      \
       serf_t::get_state_name((new_state)), __FUNCTION__, __LINE__)


typedef enum {
  SERF_NONE = -1,
  SERF_TRANSPORTER = 0,
  SERF_SAILOR,
  SERF_DIGGER,
  SERF_BUILDER,
  SERF_TRANSPORTER_INVENTORY,
  SERF_LUMBERJACK,
  SERF_SAWMILLER,
  SERF_STONECUTTER,
  SERF_FORESTER,
  SERF_MINER,
  SERF_SMELTER,
  SERF_FISHER,
  SERF_PIGFARMER,
  SERF_BUTCHER,
  SERF_FARMER,
  SERF_MILLER,
  SERF_BAKER,
  SERF_BOATBUILDER,
  SERF_TOOLMAKER,
  SERF_WEAPONSMITH,
  SERF_GEOLOGIST,
  SERF_GENERIC,
  SERF_KNIGHT_0,
  SERF_KNIGHT_1,
  SERF_KNIGHT_2,
  SERF_KNIGHT_3,
  SERF_KNIGHT_4,
  SERF_DEAD
} serf_type_t;

typedef std::map<serf_type_t, unsigned int> serf_map_t;

/* The term FREE is used loosely in the following
   names to denote a state where the serf is not
   bound to a road or a flag. */
typedef enum {
  SERF_STATE_NULL = 0,
  SERF_STATE_IDLE_IN_STOCK,
  SERF_STATE_WALKING,
  SERF_STATE_TRANSPORTING,
  SERF_STATE_ENTERING_BUILDING,
  SERF_STATE_LEAVING_BUILDING, /* 5 */
  SERF_STATE_READY_TO_ENTER,
  SERF_STATE_READY_TO_LEAVE,
  SERF_STATE_DIGGING,
  SERF_STATE_BUILDING,
  SERF_STATE_BUILDING_CASTLE, /* 10 */
  SERF_STATE_MOVE_RESOURCE_OUT,
  SERF_STATE_WAIT_FOR_RESOURCE_OUT,
  SERF_STATE_DROP_RESOURCE_OUT,
  SERF_STATE_DELIVERING,
  SERF_STATE_READY_TO_LEAVE_INVENTORY, /* 15 */
  SERF_STATE_FREE_WALKING,
  SERF_STATE_LOGGING,
  SERF_STATE_PLANNING_LOGGING,
  SERF_STATE_PLANNING_PLANTING,
  SERF_STATE_PLANTING, /* 20 */
  SERF_STATE_PLANNING_STONECUTTING,
  SERF_STATE_STONECUTTER_FREE_WALKING,
  SERF_STATE_STONECUTTING,
  SERF_STATE_SAWING,
  SERF_STATE_LOST, /* 25 */
  SERF_STATE_LOST_SAILOR,
  SERF_STATE_FREE_SAILING,
  SERF_STATE_ESCAPE_BUILDING,
  SERF_STATE_MINING,
  SERF_STATE_SMELTING, /* 30 */
  SERF_STATE_PLANNING_FISHING,
  SERF_STATE_FISHING,
  SERF_STATE_PLANNING_FARMING,
  SERF_STATE_FARMING,
  SERF_STATE_MILLING, /* 35 */
  SERF_STATE_BAKING,
  SERF_STATE_PIGFARMING,
  SERF_STATE_BUTCHERING,
  SERF_STATE_MAKING_WEAPON,
  SERF_STATE_MAKING_TOOL, /* 40 */
  SERF_STATE_BUILDING_BOAT,
  SERF_STATE_LOOKING_FOR_GEO_SPOT,
  SERF_STATE_SAMPLING_GEO_SPOT,
  SERF_STATE_KNIGHT_ENGAGING_BUILDING,
  SERF_STATE_KNIGHT_PREPARE_ATTACKING, /* 45 */
  SERF_STATE_KNIGHT_LEAVE_FOR_FIGHT,
  SERF_STATE_KNIGHT_PREPARE_DEFENDING,
  SERF_STATE_KNIGHT_ATTACKING,
  SERF_STATE_KNIGHT_DEFENDING,
  SERF_STATE_KNIGHT_ATTACKING_VICTORY, /* 50 */
  SERF_STATE_KNIGHT_ATTACKING_DEFEAT,
  SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING,
  SERF_STATE_KNIGHT_FREE_WALKING,
  SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE,
  SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE, /* 55 */
  SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE_JOIN,
  SERF_STATE_KNIGHT_PREPARE_ATTACKING_FREE,
  SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE,
  SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE_WAIT,
  SERF_STATE_KNIGHT_ATTACKING_FREE, /* 60 */
  SERF_STATE_KNIGHT_DEFENDING_FREE,
  SERF_STATE_KNIGHT_ATTACKING_VICTORY_FREE,
  SERF_STATE_KNIGHT_DEFENDING_VICTORY_FREE,
  SERF_STATE_KNIGHT_ATTACKING_FREE_WAIT,
  SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT, /* 65 */
  SERF_STATE_IDLE_ON_PATH,
  SERF_STATE_WAIT_IDLE_ON_PATH,
  SERF_STATE_WAKE_AT_FLAG,
  SERF_STATE_WAKE_ON_PATH,
  SERF_STATE_DEFENDING_HUT, /* 70 */
  SERF_STATE_DEFENDING_TOWER,
  SERF_STATE_DEFENDING_FORTRESS,
  SERF_STATE_SCATTER,
  SERF_STATE_FINISHED_BUILDING,
  SERF_STATE_DEFENDING_CASTLE, /* 75 */

  /* Additional state: goes at the end to ease loading of
     original save game. */
  SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE
} serf_state_t;

class flag_t;
class inventory_t;
class save_reader_binary_t;
class save_reader_text_t;
class save_writer_text_t;

class serf_t : public game_object_t {
 protected:
  unsigned int owner;
  serf_type_t type;
  bool sound;
  int animation; /* Index to animation table in data file. */
  int counter;
  map_pos_t pos;
  uint16_t tick;
  serf_state_t state;

  union {
    struct {
      unsigned int inv_index; /* E */
    } idle_in_stock;

    /* States: walking, transporting, delivering */
    /* res: resource carried (when transporting),
       otherwise direction. */
    struct {
      int res; /* B */
      unsigned int dest; /* C */
      int dir; /* E */
      int wait_counter; /* F */
    } walking;

    struct {
      int field_B; /* B */
      int slope_len; /* C */
    } entering_building;

    /* States: leaving_building, ready_to_leave,
       leave_for_fight */
    struct {
      int field_B; /* B */
      unsigned int dest; /* C */
      int dest2; /* D */
      int dir; /* E */
      serf_state_t next_state; /* F */
    } leaving_building;

    struct {
      int field_B; /* B */
    } ready_to_enter;

    struct {
      int h_index; /* B */
      unsigned int target_h; /* C */
      int dig_pos; /* D */
      int substate; /* E */
    } digging;

    /* mode: one of three substates (negative, positive, zero).
       bld_index: index of building. */
    struct {
      int mode; /* B */
      unsigned int bld_index; /* C */
      unsigned int material_step; /* E */
      unsigned int counter; /* F */
    } building;

    struct {
      unsigned int inv_index; /* C */
    } building_castle;

    /* States: move_resource_out, drop_resource_out */
    struct {
      unsigned int res; /* B */
      unsigned int res_dest; /* C */
      serf_state_t next_state; /* F */
    } move_resource_out;

    /* No state: wait_for_resource_out */

    struct {
      int mode; /* B */
      unsigned int dest; /* C */
      unsigned int inv_index; /* E */
    } ready_to_leave_inventory;

    /* States: free_walking, logging,
       planting, stonecutting, fishing,
       farming, sampling_geo_spot,
       knight_free_walking,
       knight_attacking_free,
       knight_attacking_free_wait */
    struct {
      int dist1; /* B */
      int dist2; /* C */
      int neg_dist1; /* D */
      int neg_dist2; /* E */
      int flags; /* F */
    } free_walking;

    /* No state data: planning_logging,
       planning_planting, planning_stonecutting */

    struct {
      int mode; /* B */
    } sawing;

    struct {
      int field_B; /* B */
    } lost;

    struct {
      unsigned int substate; /* B */
      unsigned int res; /* D */
      ground_deposit_t deposit; /* E */
    } mining;

    /* type: Type of smelter (0 is steel, else gold). */
    struct {
      int mode; /* B */
      int counter; /* C */
      unsigned int type; /* D */
    } smelting;

    /* No state data: planning_fishing,
       planning_farming */

    struct {
      int mode; /* B */
    } milling;

    struct {
      int mode; /* B */
    } baking;

    struct {
      int mode; /* B */
    } pigfarming;

    struct {
      int mode; /* B */
    } butchering;

    struct {
      int mode; /* B */
    } making_weapon;

    struct {
      int mode; /* B */
    } making_tool;

    struct {
      int mode; /* B */
    } building_boat;

    /* No state data: looking_for_geo_spot */

    /* States: knight_engaging_building,
       knight_prepare_attacking,
       knight_prepare_defending_free_wait,
       knight_attacking_defeat_free,
       knight_attacking,
       knight_attacking_victory,
       knight_engage_attacking_free,
       knight_engage_attacking_free_join,
       knight_attacking_victory_free,
    */
    struct {
      int field_B; /* B */
      int field_C; /* C */
      int field_D; /* D */
      int def_index; /* E */
    } attacking;

    /* States: knight_defending_free,
       knight_engage_defending_free */
    struct {
      int dist_col; /* B */
      int dist_row; /* C */
      int field_D; /* D */
      int other_dist_col; /* E */
      int other_dist_row; /* F */
    } defending_free;

    struct {
      int dist_col; /* B */
      int dist_row; /* C */
      int field_D; /* D */
      int field_E; /* E */
      serf_state_t next_state; /* F */
    } leave_for_walk_to_fight;

    /* States: idle_on_path, wait_idle_on_path,
       wake_at_flag, wake_on_path. */
    struct {
      dir_t rev_dir; /* B */
      flag_t *flag; /* C */
      int field_E; /* E */
    } idle_on_path;

    /* No state data: finished_building */

    /* States: defending_hut, defending_tower,
       defending_fortress, defending_castle */
    struct {
      int next_knight; /* E */
    } defending;
  } s;

 public:
  serf_t(game_t *game, unsigned int index);

  unsigned int get_player() { return owner; }
  void set_player(unsigned int player_num) { owner = player_num; }

  serf_type_t get_type() { return type; }
  void set_type(serf_type_t type);

  bool playing_sfx() { return sound; }
  void start_playing_sfx() { sound = true; }
  void stop_playing_sfx() { sound = false; }

  serf_state_t get_state() { return state; }
  int get_animation() { return animation; }
  int get_counter() { return counter; }

  map_pos_t get_pos() { return pos; }

  int train_knight(int p);

  void set_lost_state();

  void add_to_defending_queue(unsigned int next_knight_index, bool pause);
  void init_generic(inventory_t *inventory);
  void init_inventory_transporter(inventory_t *inventory);
  void reset_transport(flag_t *flag);
  bool path_splited(unsigned int flag_1, dir_t dir_1,
                    unsigned int flag_2, dir_t dir_2,
                    int *select);
  bool is_related_to(unsigned int dest, dir_t dir);
  void path_deleted(unsigned int dest, dir_t dir);
  void path_merged(flag_t *flag);
  void path_merged2(unsigned int flag_1, dir_t dir_1,
                    unsigned int flag_2, dir_t dir_2);
  void flag_deleted(map_pos_t flag_pos);
  bool building_deleted(map_pos_t building_pos, bool escape);
  void castle_deleted(map_pos_t castle_pos, bool transporter);
  bool change_transporter_state_at_pos(map_pos_t pos, serf_state_t state);
  void restore_path_serf_info();
  void clear_destination(unsigned int dest);
  void clear_destination2(unsigned int dest);
  bool idle_to_wait_state(map_pos_t pos);

  int get_delivery() const;
  int get_free_walking_neg_dist1() const { return s.free_walking.neg_dist1; }
  int get_free_walking_neg_dist2() const { return s.free_walking.neg_dist2; }
  int get_leaving_building_next_state() const {
    return s.leaving_building.next_state; }
  int get_leaving_building_field_B() const {
    return s.leaving_building.field_B; }
  int get_mining_res() const { return s.mining.res; }
  int get_attacking_field_D() const { return s.attacking.field_D; }
  int get_attacking_def_index() const { return s.attacking.def_index; }
  int get_walking_wait_counter() const { return s.walking.wait_counter; }
  void set_walking_wait_counter(int counter) {
    s.walking.wait_counter = counter; }
  int get_walking_dir() const { return s.walking.dir; }
  unsigned int get_idle_in_stock_inv_index() const {
                                             return s.idle_in_stock.inv_index; }
  int get_mining_substate() const { return s.mining.substate; }

  serf_t *extract_last_knight_from_list();
  void insert_before(serf_t *knight);
  unsigned int get_next() const { return s.defending.next_knight; }
  void set_next(int next) { s.defending.next_knight = next; }

  // Commands
  void go_out_from_inventory(unsigned int inventory,
                             map_pos_t dest, int dir);
  void send_off_to_fight(int dist_col, int dist_row);
  void stay_idle_in_stock(unsigned int inventory);
  void go_out_from_building(map_pos_t dest, int dir, int field_B);

  void update();

  static const char *get_state_name(serf_state_t state);
  static const char *get_type_name(serf_type_t type);

  friend save_reader_binary_t&
    operator >> (save_reader_binary_t &reader, serf_t &serf);
  friend save_reader_text_t&
    operator >> (save_reader_text_t &reader, serf_t &serf);
  friend save_writer_text_t&
    operator << (save_writer_text_t &writer, serf_t &serf);

 protected:
  int is_waiting(dir_t *dir);
  int switch_waiting(dir_t dir);
  int get_walking_animation(int h_diff, dir_t dir, int switch_pos);
  void change_direction(dir_t dir, int alt_end);
  void transporter_move_to_flag(flag_t *flag);
  void start_walking(dir_t dir, int slope, int change_pos);
  void enter_building(int field_B, int join_pos);
  void leave_building(int join_pos);
  void enter_inventory();
  void drop_resource(resource_type_t res);
  void find_inventory();
  bool can_pass_map_pos(map_pos_t pos);
  void set_fight_outcome(serf_t *attacker, serf_t *defender);

  static bool handle_serf_walking_state_search_cb(flag_t *flag, void *data);


  void handle_serf_idle_in_stock_state();
  void handle_serf_walking_state_dest_reached();
  void handle_serf_walking_state_waiting();
  void handle_serf_walking_state();
  void handle_serf_transporting_state();
  void handle_serf_entering_building_state();
  void handle_serf_leaving_building_state();
  void handle_serf_ready_to_enter_state();
  void handle_serf_ready_to_leave_state();
  void handle_serf_digging_state();
  void handle_serf_building_state();
  void handle_serf_building_castle_state();
  void handle_serf_move_resource_out_state();
  void handle_serf_wait_for_resource_out_state();
  void handle_serf_drop_resource_out_state();
  void handle_serf_delivering_state();
  void handle_serf_ready_to_leave_inventory_state();
  void handle_serf_free_walking_state_dest_reached();
  void handle_serf_free_walking_switch_on_dir(dir_t dir);
  void handle_serf_free_walking_switch_with_other();
  int handle_free_walking_follow_edge();
  void handle_free_walking_common();
  void handle_serf_free_walking_state();
  void handle_serf_logging_state();
  void handle_serf_planning_logging_state();
  void handle_serf_planning_planting_state();
  void handle_serf_planting_state();
  void handle_serf_planning_stonecutting();
  void handle_stonecutter_free_walking();
  void handle_serf_stonecutting_state();
  void handle_serf_sawing_state();
  void handle_serf_lost_state();
  void handle_lost_sailor();
  void handle_free_sailing();
  void handle_serf_escape_building_state();
  void handle_serf_mining_state();
  void handle_serf_smelting_state();
  void handle_serf_planning_fishing_state();
  void handle_serf_fishing_state();
  void handle_serf_planning_farming_state();
  void handle_serf_farming_state();
  void handle_serf_milling_state();
  void handle_serf_baking_state();
  void handle_serf_pigfarming_state();
  void handle_serf_butchering_state();
  void handle_serf_making_weapon_state();
  void handle_serf_making_tool_state();
  void handle_serf_building_boat_state();
  void handle_serf_looking_for_geo_spot_state();
  void handle_serf_sampling_geo_spot_state();
  void handle_serf_knight_engaging_building_state();
  void handle_serf_knight_prepare_attacking();
  void handle_serf_knight_leave_for_fight_state();
  void handle_serf_knight_prepare_defending_state();
  void handle_knight_attacking();
  void handle_serf_knight_attacking_victory_state();
  void handle_serf_knight_attacking_defeat_state();
  void handle_knight_occupy_enemy_building();
  void handle_state_knight_free_walking();
  void handle_state_knight_engage_defending_free();
  void handle_state_knight_engage_attacking_free();
  void handle_state_knight_engage_attacking_free_join();
  void handle_state_knight_prepare_attacking_free();
  void handle_state_knight_prepare_defending_free();
  void handle_knight_attacking_victory_free();
  void handle_knight_defending_victory_free();
  void handle_serf_knight_attacking_defeat_free_state();
  void handle_knight_attacking_free_wait();
  void handle_serf_state_knight_leave_for_walk_to_fight();
  void handle_serf_idle_on_path_state();
  void handle_serf_wait_idle_on_path_state();
  void handle_scatter_state();
  void handle_serf_finished_building_state();
  void handle_serf_wake_at_flag_state();
  void handle_serf_wake_on_path_state();
  void handle_serf_defending_state(const int training_params[]);
  void handle_serf_defending_hut_state();
  void handle_serf_defending_tower_state();
  void handle_serf_defending_fortress_state();
  void handle_serf_defending_castle_state();
};

#endif  // SRC_SERF_H_
