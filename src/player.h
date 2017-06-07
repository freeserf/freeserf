/*
 * player.h - Player related functions
 *
 * Copyright (C) 2013-2016  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef SRC_PLAYER_H_
#define SRC_PLAYER_H_

#include <queue>
#include <vector>

#include "src/map.h"
#include "src/serf.h"
#include "src/objects.h"
#include "src/mission.h"

class Serf;
class Inventory;
class Building;
class SaveReaderBinary;
class SaveReaderText;
class SaveWriterText;

class Message {
 public:
  typedef enum Type {
    TypeNone = 0,
    TypeUnderAttack = 1,
    TypeLoseFight = 2,
    TypeWinFight = 3,
    TypeMineEmpty = 4,
    TypeCallToLocation = 5,
    TypeKnightOccupied = 6,
    TypeNewStock = 7,
    TypeLostLand = 8,
    TypeLostBuildings = 9,
    TypeEmergencyActive = 10,
    TypeEmergencyNeutral = 11,
    TypeFoundGold = 12,
    TypeFoundIron = 13,
    TypeFoundCoal = 14,
    TypeFoundStone = 15,
    TypeCallToMenu = 16,
    Type30MSinceSave = 17,
    Type1HSinceSave = 18,
    TypeCallToStock = 19
  } Type;

 public:
  Type type;
  MapPos pos;
  unsigned int data;
};
typedef std::queue<Message> Messages;

class PosTimer {
 public:
  int timeout;
  MapPos pos;
};
typedef std::vector<PosTimer> PosTimers;

/* Player object. Holds the game state of a player. */
class Player : public GameObject {
 protected:
  int tool_prio[9];
  int resource_count[26];
  int flag_prio[26];
  int serf_count[27];
  int knight_occupation[4];

  PlayerColor color; /* ADDED */
  size_t face;
  int flags;
  int build;
  int completed_building_count[24];
  int incomplete_building_count[24];
  int inventory_prio[26];
  int attacking_buildings[64];

  Messages messages;
  PosTimers timers;

  int building;
  int castle_inventory;
  int cont_search_after_non_optimal_find;
  int knights_to_spawn;
  unsigned int total_land_area;
  unsigned int total_building_score;
  unsigned int total_military_score;
  uint16_t last_tick;

  int reproduction_counter;
  size_t reproduction_reset;
  int serf_to_knight_rate;
  uint16_t serf_to_knight_counter; /* Overflow is important */
  int analysis_goldore;
  int analysis_ironore;
  int analysis_coal;
  int analysis_stone;

  int food_stonemine; /* Food delivery priority of food for mines. */
  int food_coalmine;
  int food_ironmine;
  int food_goldmine;
  int planks_construction; /* Planks delivery priority. */
  int planks_boatbuilder;
  int planks_toolmaker;
  int steel_toolmaker;
  int steel_weaponsmith;
  int coal_steelsmelter;
  int coal_goldsmelter;
  int coal_weaponsmith;
  int wheat_pigfarm;
  int wheat_mill;

  /* +1 for every castle defeated,
     -1 for own castle lost. */
  int castle_score;
  int send_generic_delay;
  unsigned int initial_supplies;
  int serf_index;
  int knight_cycle_counter;
  int send_knight_delay;
  int military_max_gold;

  int knight_morale;
  int gold_deposited;
  int castle_knights_wanted;
  int castle_knights;
  int ai_value_0;
  int ai_value_1;
  int ai_value_2;
  int ai_value_3;
  int ai_value_4;
  int ai_value_5;
  size_t ai_intelligence;

  int player_stat_history[16][112];
  int resource_count_history[26][120];

 public:
  // TODO(Digger): remove it to UI
  int building_attacked;
  int knights_attacking;
  int attacking_building_count;
  int attacking_knights[4];
  int total_attacking_knights;
  unsigned int temp_index;

 public:
  Player(Game *game, unsigned int index);

  void init(PPlayerInfo player_info);

  PlayerColor get_color() const { return color; }
  size_t get_face() const { return face; }

  /* Whether player has built the initial castle. */
  bool has_castle() const { return (flags & 1); }
  /* Whether the strongest knight should be sent to fight. */
  bool send_strongest() const { return ((flags >> 1) & 1); }
  void drop_send_strongest() { flags &= ~BIT(1); }
  void set_send_strongest() { flags |= BIT(1); }
  /* Whether cycling of knights is in progress. */
  bool cycling_knight() const { return ((flags >> 2) & 1); }
  /* Whether a message is queued for this player. */
  bool has_message() const { return ((flags >> 3) & 1); }
  void drop_message() { flags &= ~BIT(3); }
  /* Whether the knight level of military buildings is temporarily
   reduced bacause of cycling of the knights. */
  bool reduced_knight_level() const { return ((flags >> 4) & 1); }
  /* Whether the cycling of knights is in the second phase. */
  bool cycling_second() const { return ((flags >> 5) & 1); }
  /* Whether this player is a computer controlled opponent. */
  bool is_ai() const { return ((flags >> 7) & 1); }

  /* Whether player is prohibited from building military
   buildings at current position. */
  bool allow_military() const { return !(build & 1); }
  /* Whether player is prohibited from building flag at
   current position. */
  bool allow_flag() const { return !((build >> 1) & 1); }
  /* Whether player can spawn new serfs. */
  bool can_spawn() const { return ((build >> 2) & 1); }

  unsigned int get_serf_count(int type) const { return serf_count[type]; }
  int get_flag_prio(int res) const { return flag_prio[res]; }

  void add_notification(Message::Type type, MapPos pos, unsigned int data);
  bool has_notification();
  Message pop_notification();
  Message peek_notification();

  void add_timer(int timeout, MapPos pos);

  void reset_food_priority();
  void reset_planks_priority();
  void reset_steel_priority();
  void reset_coal_priority();
  void reset_wheat_priority();
  void reset_tool_priority();

  void reset_flag_priority();
  void reset_inventory_priority();

  int get_knight_occupation(size_t threat_level) const {
    return knight_occupation[threat_level]; }
  void change_knight_occupation(int index, int adjust_max, int delta);
  void increase_castle_knights() { castle_knights++; }
  void decrease_castle_knights() { castle_knights--; }
  int get_castle_knights() const { return castle_knights; }
  int get_castle_knights_wanted() const { return castle_knights_wanted; }
  void increase_castle_knights_wanted();
  void decrease_castle_knights_wanted();
  int get_knight_morale() const { return knight_morale; }
  int get_gold_deposited() const { return gold_deposited; }

  int promote_serfs_to_knights(int number);
  int knights_available_for_attack(MapPos pos);
  void start_attack();
  void cycle_knights();

  void create_initial_castle_serfs(Building *castle);
  Serf *spawn_serf_generic();
  int spawn_serf(Serf **serf, Inventory **inventory, bool want_knight);
  bool tick_send_generic_delay();
  bool tick_send_knight_delay();
  Serf::Type get_cycling_sert_type(Serf::Type type) const;

  void increase_serf_count(int type) { serf_count[type]++; }
  void decrease_serf_count(int type) { serf_count[type]--; }
  int *get_serfs() { return reinterpret_cast<int*>(serf_count); }

  void increase_res_count(int type) { serf_count[type]++; }
  void decrease_res_count(int type) { serf_count[type]--; }

  void building_founded(Building *building);
  void building_built(Building *building);
  void building_captured(Building *building);
  void building_demolished(Building *building);
  int get_completed_building_count(int type) const {
    return completed_building_count[type]; }
  int get_incomplete_building_count(int type) const {
    return incomplete_building_count[type]; }

  int get_tool_prio(int type) const { return tool_prio[type]; }
  void set_tool_prio(int type, int prio) { tool_prio[type] = prio; }
  int *get_flag_prio() { return flag_prio; }

  int get_inventory_prio(int type) const { return inventory_prio[type]; }
  int *get_inventory_prio() { return inventory_prio; }

  int get_total_military_score() const { return total_military_score; }

  void update();
  void update_stats(int res);

  // Stats
  void update_knight_morale();
  int get_land_area() const { return total_land_area; }
  void increase_land_area() { total_land_area++; }
  void decrease_land_area() { total_land_area--; }
  int get_building_score() const { return total_building_score; }
  int get_military_score() const;
  void increase_military_score(int val) { total_military_score += val; }
  void decrease_military_score(int val) { total_military_score -= val; }
  void increase_military_max_gold(int val) { military_max_gold += val; }
  int get_score() const;
  unsigned int get_initial_supplies() const { return initial_supplies; }
  int *get_resource_count_history(Resource::Type type) {
    return resource_count_history[type]; }
  void set_player_stat_history(int mode, int index, int val) {
    player_stat_history[mode][index] = val; }
  int *get_player_stat_history(int mode) { return player_stat_history[mode]; }

  ResourceMap get_stats_resources();
  Serf::SerfMap get_stats_serfs_idle();
  Serf::SerfMap get_stats_serfs_potential();

  // Settings
  int get_serf_to_knight_rate() const { return serf_to_knight_rate; }
  void set_serf_to_knight_rate(int rate) { serf_to_knight_rate = rate; }
  int get_food_stonemine() const { return food_stonemine; }
  void set_food_stonemine(int val) { food_stonemine = val; }
  int get_food_coalmine() const { return food_coalmine; }
  void set_food_coalmine(int val) { food_coalmine = val; }
  int get_food_ironmine() const { return food_ironmine; }
  void set_food_ironmine(int val) { food_ironmine = val; }
  int get_food_goldmine() const { return food_goldmine; }
  void set_food_goldmine(int val) { food_goldmine = val; }
  int get_planks_construction() const { return planks_construction; }
  void set_planks_construction(int val) { planks_construction = val; }
  int get_planks_boatbuilder() const { return planks_boatbuilder; }
  void set_planks_boatbuilder(int val) { planks_boatbuilder = val; }
  int get_planks_toolmaker() const { return planks_toolmaker; }
  void set_planks_toolmaker(int val) { planks_toolmaker = val; }
  int get_steel_toolmaker() const { return steel_toolmaker; }
  void set_steel_toolmaker(int val) { steel_toolmaker = val; }
  int get_steel_weaponsmith() const { return steel_weaponsmith; }
  void set_steel_weaponsmith(int val) { steel_weaponsmith = val; }
  int get_coal_steelsmelter() const { return coal_steelsmelter; }
  void set_coal_steelsmelter(int val) { coal_steelsmelter = val; }
  int get_coal_goldsmelter() const { return coal_goldsmelter; }
  void set_coal_goldsmelter(int val) { coal_goldsmelter = val; }
  int get_coal_weaponsmith() const { return coal_weaponsmith; }
  void set_coal_weaponsmith(int val) { coal_weaponsmith = val; }
  int get_wheat_pigfarm() const { return wheat_pigfarm; }
  void set_wheat_pigfarm(int val) { wheat_pigfarm = val; }
  int get_wheat_mill() const { return wheat_mill; }
  void set_wheat_mill(int val) { wheat_mill = val; }

  friend SaveReaderBinary&
    operator >> (SaveReaderBinary &reader, Player &player);
  friend SaveReaderText&
    operator >> (SaveReaderText &reader, Player &player);
  friend SaveWriterText&
    operator << (SaveWriterText &writer, Player &player);

 protected:
  void init_ai_values(size_t face);

  int available_knights_at_pos(MapPos pos, int index, int dist);
};

#endif  // SRC_PLAYER_H_
