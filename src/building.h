/*
 * building.h - Building related functions.
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


#ifndef SRC_BUILDING_H_
#define SRC_BUILDING_H_

#include "src/map.h"
#include "src/resource.h"
#include "src/misc.h"
#include "src/serf.h"
#include "src/objects.h"

/* Max number of different types of resources accepted by buildings. */
#define BUILDING_MAX_STOCK  2

class Inventory;
class Serf;
class SaveReaderBinary;
class SaveReaderText;
class SaveWriterText;

class Building : public GameObject {
 public:
  typedef enum Type {
    TypeNone = 0,
    TypeFisher,
    TypeLumberjack,
    TypeBoatbuilder,
    TypeStonecutter,
    TypeStoneMine,
    TypeCoalMine,
    TypeIronMine,
    TypeGoldMine,
    TypeForester,
    TypeStock,
    TypeHut,
    TypeFarm,
    TypeButcher,
    TypePigFarm,
    TypeMill,
    TypeBaker,
    TypeSawmill,
    TypeSteelSmelter,
    TypeToolMaker,
    TypeWeaponSmith,
    TypeTower,
    TypeFortress,
    TypeGoldSmelter,
    TypeCastle
  } Type;

 protected:
  typedef struct Stock {
    Resource::Type type;
    int prio;
    int available;
    int requested;
    int maximum;
  } Stock;

  /* Map position of building */
  MapPos pos;
  /* Type of building */
  Type type;
  /* Building owner */
  unsigned int owner;
  /* Building under construction */
  bool constructing;
  /* Flags */
  size_t threat_level;
  bool playing_sfx;
  bool serf_request_failed;
  bool serf_requested;
  bool burning;
  bool active;
  bool holder;
  /* Index of flag connected to this building */
  unsigned int flag;
  /* Stock of this building */
  Stock stock[BUILDING_MAX_STOCK];
  unsigned int first_knight;
  int burning_counter;
  int progress;
  union u {
    Inventory *inventory;
    unsigned int tick; /* Used for burning building. */
    unsigned int level;
  } u;

 public:
  Building(Game *game, unsigned int index);

  MapPos get_position() const { return pos; }
  void set_position(MapPos position) { pos = position; }

  unsigned int get_flag_index() const { return flag; }
  void link_flag(unsigned int flag_index) { flag = flag_index; }

  bool has_knight() const { return (first_knight != 0); }
  unsigned int get_first_knight() const { return first_knight; }
  void set_first_knight(unsigned int serf);

  int get_burning_counter() const { return burning_counter; }
  void set_burning_counter(int counter) { burning_counter = counter; }
  void decrease_burning_counter(int delta) { burning_counter -= delta; }

  /* Type of building. */
  Type get_type() const { return type; }
  bool is_military() const { return (type == TypeHut) ||
                                    (type == TypeTower) ||
                                    (type == TypeFortress) ||
                                    (type == TypeCastle); }
  /* Owning player of the building. */
  unsigned int get_owner() const { return owner; }
  void set_owner(unsigned int new_owner) { owner = new_owner; }
  /* Whether construction of the building is finished. */
  bool is_done() const { return !constructing; }
  bool is_leveling() const { return (!is_done() && progress == 0); }
  void done_leveling();
  Map::Object start_building(Type type);
  int get_progress() const { return progress; }
  bool build_progress();
  void increase_mining(int res);
  void set_under_attack() { progress |= BIT(0); }
  bool is_under_attack() const { return BIT_TEST(progress, 0); }

  /* The threat level of the building. Higher values mean that
   the building is closer to the enemy. */
  size_t get_threat_level() const { return threat_level; }
  /* Building is currently playing back a sound effect. */
  bool is_playing_sfx() const { return playing_sfx; }
  void start_playing_sfx() { playing_sfx = true; }
  void stop_playing_sfx() { playing_sfx = false; }
  /* Building is active (specifics depend on building type). */
  bool is_active() const { return active; }
  void start_activity() { active = true; }
  void stop_activity() { active = false; }
  /* Building is burning. */
  bool is_burning() const { return burning; }
  bool burnup();
  /* Building has an associated serf. */
  bool has_serf() const { return holder; }
  /* Building has succesfully requested a serf. */
  void serf_request_granted() { serf_requested = true; }
  void requested_serf_lost();
  void requested_serf_reached(Serf *serf);
  /* Building has requested a serf but none was available. */
  void clear_serf_request_failure() { serf_request_failed = false; }
  void knight_request_granted();

  /* Building has inventory and the inventory pointer is valid. */
  bool has_inventory() const { return (stock[0].requested == 0xff); }
  Inventory *get_inventory() { return u.inventory; }
  void set_inventory(Inventory *inventory) { u.inventory = inventory; }

  unsigned int get_level() const { return u.level; }
  void set_level(unsigned int level) { u.level = level; }

  unsigned int get_tick() const { return u.tick; }
  void set_tick(unsigned int tick) { u.tick = tick; }

  unsigned int get_knight_count() const { return waiting_planks(); }

  unsigned int waiting_stone() const {
    return stock[1].available; }  // Stone allways in stock #1
  unsigned int waiting_planks() const {
    return stock[0].available; }  // Planks allways in stock #0
  unsigned int military_gold_count() const;

  void cancel_transported_resource(Resource::Type res);

  Serf *call_defender_out();
  Serf *call_attacker_out(int knight_index);

  bool add_requested_resource(Resource::Type res, bool fix_priority);
  bool is_stock_active(int stock_num) const {
    return (stock[stock_num].type > 0); }
  unsigned int get_res_count_in_stock(int stock_num) const {
    return stock[stock_num].available; }
  Resource::Type get_res_type_in_stock(int stock_num) const {
    return stock[stock_num].type; }
  void stock_init(unsigned int stock_num, Resource::Type type,
                  unsigned int maximum);
  void remove_stock();
  int get_max_priority_for_resource(
    Resource::Type resource, int minimum = 0) const;
  int get_maximum_in_stock(int stock_num) const {
    return stock[stock_num].maximum; }
  int get_requested_in_stock(int stock_num) const {
    return stock[stock_num].requested; }
  void set_priority_in_stock(int stock_num, int priority) {
    stock[stock_num].prio = priority; }
  void set_initial_res_in_stock(int stock_num, int count) {
    stock[stock_num].available = count; }
  void requested_resource_delivered(Resource::Type resource);
  void plank_used_for_build() {
    stock[0].available -= 1; stock[0].maximum -= 1; }
  void stone_used_for_build() {
    stock[1].available -= 1; stock[1].maximum -= 1; }
  bool use_resource_in_stock(int stock_num);
  bool use_resources_in_stocks();
  void decrease_requested_for_stock(int stock_num) {
    stock[stock_num].requested -= 1; }

  int pigs_count() const { return stock[1].available; }
  void send_pig_to_butcher() { stock[1].available -= 1; }
  void place_new_pig() { stock[1].available += 1; }

  void boat_clear() { stock[1].available = 0; }
  void boat_do() { stock[1].available++; }

  void requested_knight_arrived();
  void requested_knight_attacking_on_walk() { stock[0].requested -= 1; }
  void requested_knight_defeat_on_walk() {
    if (!has_inventory()) stock[0].requested -= 1; }
  bool is_enough_place_for_knight() const;
  bool knight_come_back_from_fight(Serf *knight);
  void knight_occupy();

  void update_military_flag_state();

  void update(unsigned int tick);

  friend SaveReaderBinary&
    operator >> (SaveReaderBinary &reader, Building &building);
  friend SaveReaderText&
    operator >> (SaveReaderText &reader, Building &building);
  friend SaveWriterText&
    operator << (SaveWriterText &writer, Building &building);

 private:
  void update();
  void update_unfinished();
  void update_unfinished_adv();
  void update_castle();
  void update_military();

  void request_serf_if_needed();

  bool send_serf_to_building(Serf::Type type, Resource::Type res1,
                             Resource::Type res2);
};


int building_get_score_from_type(Building::Type type);


#endif  // SRC_BUILDING_H_
