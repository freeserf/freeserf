/*
 * serf.h - Serf related declarations
 *
 * Copyright (C) 2013-2019  Jon Lund Steffensen <jonlst@gmail.com>
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
#include <string>

#include "src/map.h"
#include "src/resource.h"
#include "src/objects.h"

class Flag;
class Inventory;
class SaveReaderBinary;
class SaveReaderText;
class SaveWriterText;

class Serf : public GameObject {
 public:
  typedef enum Type {
    TypeNone = -1,
    TypeTransporter = 0,  // 0
    TypeSailor,
    TypeDigger,
    TypeBuilder,
    TypeTransporterInventory,
    TypeLumberjack,  // 5
    TypeSawmiller,
    TypeStonecutter,
    TypeForester,
    TypeMiner,
    TypeSmelter,  // 10
    TypeFisher,
    TypePigFarmer,
    TypeButcher,
    TypeFarmer,
    TypeMiller,  // 15
    TypeBaker,
    TypeBoatBuilder,
    TypeToolmaker,
    TypeWeaponSmith,
    TypeGeologist,  // 20
    TypeGeneric,
    TypeKnight0,
    TypeKnight1,
    TypeKnight2,
    TypeKnight3,  // 25
    TypeKnight4,
    TypeDead  // 27
  } Type;

  typedef std::map<Type, unsigned int> SerfMap;

  /* The term FREE is used loosely in the following
   names to denote a state where the serf is not
   bound to a road or a flag. */
  typedef enum State {
    StateNull = 0,
    StateIdleInStock,
    StateWalking,
    StateTransporting,
    StateEnteringBuilding,
    StateLeavingBuilding, /* 5 */
    StateReadyToEnter,
    StateReadyToLeave,
    StateDigging,
    StateBuilding,
    StateBuildingCastle, /* 10 */
    StateMoveResourceOut,
    StateWaitForResourceOut,
    StateDropResourceOut,
    StateDelivering,
    StateReadyToLeaveInventory, /* 15 */
    StateFreeWalking,
    StateLogging,
    StatePlanningLogging,
    StatePlanningPlanting,
    StatePlanting, /* 20 */
    StatePlanningStoneCutting,
    StateStoneCutterFreeWalking,
    StateStoneCutting,
    StateSawing,
    StateLost, /* 25 */
    StateLostSailor,
    StateFreeSailing,
    StateEscapeBuilding,
    StateMining,
    StateSmelting, /* 30 */
    StatePlanningFishing,
    StateFishing,
    StatePlanningFarming,
    StateFarming,
    StateMilling, /* 35 */
    StateBaking,
    StatePigFarming,
    StateButchering,
    StateMakingWeapon,
    StateMakingTool, /* 40 */
    StateBuildingBoat,
    StateLookingForGeoSpot,
    StateSamplingGeoSpot,
    StateKnightEngagingBuilding,
    StateKnightPrepareAttacking, /* 45 */
    StateKnightLeaveForFight,
    StateKnightPrepareDefending,
    StateKnightAttacking,
    StateKnightDefending,
    StateKnightAttackingVictory, /* 50 */
    StateKnightAttackingDefeat,
    StateKnightOccupyEnemyBuilding,
    StateKnightFreeWalking,
    StateKnightEngageDefendingFree,
    StateKnightEngageAttackingFree, /* 55 */
    StateKnightEngageAttackingFreeJoin,
    StateKnightPrepareAttackingFree,
    StateKnightPrepareDefendingFree,
    StateKnightPrepareDefendingFreeWait,
    StateKnightAttackingFree, /* 60 */
    StateKnightDefendingFree,
    StateKnightAttackingVictoryFree,
    StateKnightDefendingVictoryFree,
    StateKnightAttackingFreeWait,
    StateKnightLeaveForWalkToFight, /* 65 */
    StateIdleOnPath,
    StateWaitIdleOnPath,
    StateWakeAtFlag,
    StateWakeOnPath,
    StateDefendingHut, /* 70 */
    StateDefendingTower,
    StateDefendingFortress,
    StateScatter,
    StateFinishedBuilding,
    StateDefendingCastle, /* 75 */

    /* Additional state: goes at the end to ease loading of
     original save game. */
    StateKnightAttackingDefeatFree,
    StateWaitForBoat,   // to support option_CanTransportSerfsInBoats
    StateBoatPassenger  // to support option_CanTransportSerfsInBoats
  } State;

 protected:
  unsigned int owner;
  Type type;
  bool sound;
  int animation; /* Index to animation table in data file. */
  int counter;
  MapPos pos;
  uint16_t tick;
  State state;
  // tlongstretch, added to support lost serf clearing to non-Inventory buildings
  // this could be done inside the states union, but that would require passing it along the various
  //  relevant states, which is annoying
  bool was_lost;
  //
  // DEBUG stuck serf WaitIdleOnPath issues
  //
  //bool split_merge_tainted;    // note if serf has ever been involved in a fill_path_data call from a merged/split road
  unsigned int recent_dest;  // store the most recent destination for each serf, in case they become Lost, try to send another serf.  Flag index

  union s {
    struct {
      unsigned int inv_index; /* E */
    } idle_in_stock;

    /* States: walking, transporting, delivering */
    /* res: resource carried (when transporting), otherwise direction. */
    struct {
      int dir1; /* B */
      unsigned int dest; /* C */
      int dir; /* E */
      int wait_counter; /* F */
    } walking;

    /* States: transporting, delivering */
    struct {
      Resource::Type res; /* B */
      unsigned int dest; /* C */
      int dir; /* E */
      int wait_counter; /* F */
      // add support for option_CanTransportSerfsInBoats
      Type pickup_serf_type;            // serf at flag one tile away, about to be picked up by sailor
      unsigned int pickup_serf_index;   // serf at flag one tile away, about to be picked up by sailor
      Type passenger_serf_type;         // serf currently in the sailor's boat
      unsigned int passenger_serf_index;  // serf currently in the sailor's boat
      Type dropped_serf_type;           // serf at flag one tile away, just dropped off by sailor
      unsigned int dropped_serf_index;  // serf at flag one tile away, just dropped off by sailor
    } transporting;

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
      State next_state; /* F */
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
      State next_state; /* F */
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
      int dist_col; /* B */
      int dist_row; /* C */
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
      int res; /* D */
      Map::Minerals deposit; /* E */
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
      int move; /* B */
      int attacker_won; /* C */
      int field_D; /* D */
      int def_index; /* E */
    } attacking;

    /* States: knight_attacking_victory_free,
     */
    struct {
      int move; /* B */
      int dist_col; /* C */
      int dist_row; /* D */
      int def_index; /* E */
    } attacking_victory_free;

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
      State next_state; /* F */
    } leave_for_walk_to_fight;

    /* States: idle_on_path, wait_idle_on_path,
       wake_at_flag, wake_on_path. */
    struct {
      int flag; /* C */
      int field_E; /* E */
      Direction rev_dir; /* B */
    } idle_on_path;

    /* No state data: finished_building */

    /* States: defending_hut, defending_tower,
       defending_fortress, defending_castle */
    struct {
      int next_knight; /* E */
    } defending;
  } s;

 public:
  Serf(Game *game, unsigned int index);

  unsigned int get_owner() const { return owner; }
  void set_owner(unsigned int player_num) { owner = player_num; }

  Type get_type() const { return type; }
  void set_type(Type type);
  void set_serf_state(Serf::State state);

  bool playing_sfx() const { return sound; }
  void start_playing_sfx() { sound = true; }
  void stop_playing_sfx() { sound = false; }

  State get_state() const { return state; }

  //DEBUG stuck serfs issue, provide the associated Flag index the serf is supposedly at
  int debug_get_idle_on_path_flag() const { return s.idle_on_path.flag; }
  void debug_set_pos(MapPos new_pos) { pos = new_pos; }

  int get_animation() const { return animation; }
  int get_counter() const { return counter; }

  MapPos get_pos() const { return pos; }

  int train_knight(int p);

  void set_lost_state();

  void add_to_defending_queue(unsigned int next_knight_index, bool pause);
  void init_generic(Inventory *inventory);
  void init_inventory_transporter(Inventory *inventory);
  void reset_transport(Flag *flag);
  bool path_splited(unsigned int flag_1, Direction dir_1,
                    unsigned int flag_2, Direction dir_2,
                    int *select);
  bool is_related_to(unsigned int dest, Direction dir);
  void path_deleted(unsigned int dest, Direction dir);
  void path_merged(Flag *flag);
  void path_merged2(unsigned int flag_1, Direction dir_1,
                    unsigned int flag_2, Direction dir_2);
  void flag_deleted(MapPos flag_pos);
  bool building_deleted(MapPos building_pos, bool escape);
  void castle_deleted(MapPos castle_pos, bool transporter);
  bool change_transporter_state_at_pos(MapPos pos, State state);
  void restore_path_serf_info();
  void clear_destination(unsigned int dest);
  void clear_destination2(unsigned int dest);
  bool idle_to_wait_state(MapPos pos);

  int get_delivery() const;
  // add support for requested resource search debugging - returns destination flag index for carried resource
  int get_transporting_dest() const { return s.transporting.dest; }
  // add support for option_CanTransportSerfsInBoats
  Type get_pickup_serf_type() const { return s.transporting.pickup_serf_type; }
  unsigned int get_pickup_serf_index() const { return s.transporting.pickup_serf_index; }
  Type get_passenger_serf_type() const { return s.transporting.passenger_serf_type; }
  unsigned int get_passenger_serf_index() const { return s.transporting.passenger_serf_index; }
  Type get_dropped_serf_type() const { return s.transporting.dropped_serf_type; }
  unsigned int get_dropped_serf_index() const { return s.transporting.dropped_serf_index; }
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
  void set_walking_wait_counter(int new_counter) {
    s.walking.wait_counter = new_counter; }
  int get_walking_dir() const { return s.walking.dir; }
  unsigned int get_idle_in_stock_inv_index() const {
                                             return s.idle_in_stock.inv_index; }
  int get_mining_substate() const { return s.mining.substate; }

  Serf *extract_last_knight_from_list();
  void insert_before(Serf *knight);
  unsigned int get_next() const { return s.defending.next_knight; }
  void set_next(int next) { s.defending.next_knight = next; }

  // AI addition to help debug lack of serf transporter issue/bug
  unsigned int get_walking_dest() const { return s.walking.dest; }
  unsigned int get_recent_dest() const { return recent_dest; } // flag index of the most recent dest flag when serf last left an Inv

  // Commands
  // the go_out_from_inventory declaration says arg2 is a MapPos, but src->get_index() returns a Flag index not a Map Pos, wtf??
  void go_out_from_inventory(unsigned int inventory, MapPos dest, int mode);
  void send_off_to_fight(int dist_col, int dist_row);
  void stay_idle_in_stock(unsigned int inventory);
  void go_out_from_building(MapPos dest, int dir, int field_B);

  void update();

  static const char *get_state_name(State state);
  static const char *get_type_name(Type type);

  friend SaveReaderBinary&
    operator >> (SaveReaderBinary &reader, Serf &serf);
  friend SaveReaderText&
    operator >> (SaveReaderText &reader, Serf &serf);
  friend SaveWriterText&
    operator << (SaveWriterText &writer, Serf &serf);

  std::string print_state();
  // moved from protected
  //   wait, does this still need to be moved?  I think I changed the approach to
  //   serf wait timers
  bool is_waiting(Direction *dir);

 protected:
  // moved to public so AI can use it to check for stuck serfs
  //   wait, does this still need to be moved?  I think I changed the approach to
  //   serf wait timers
  //bool is_waiting(Direction *dir);
  int switch_waiting(Direction dir);
  int get_walking_animation(int h_diff, Direction dir, int switch_pos);
  void change_direction(Direction dir, int alt_end);
  void transporter_move_to_flag(Flag *flag);
  void start_walking(Direction dir, int slope, int change_pos);
  void enter_building(int field_B, int join_pos);
  void leave_building(int join_pos);
  void enter_inventory();
  void drop_resource(Resource::Type res);
  void find_inventory();
  bool can_pass_map_pos(MapPos pos);
  void set_fight_outcome(Serf *attacker, Serf *defender);

  static bool handle_serf_walking_state_search_cb(Flag *flag, void *data);

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
  void handle_serf_free_walking_switch_on_dir(Direction dir);
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
  void handle_serf_wait_for_boat_state();
  void handle_serf_boat_passenger_state();
};

#endif  // SRC_SERF_H_
