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

  typedef struct Stock {
    Resource::Type type;
    int prio;
    int available;
    int requested;
    int maximum;
  } Stock;

 protected:
  /* Map position of building */
  MapPos pos;
  /* Type of building */
  Type type;
  /* Flags */
  int bld;
  int serf;
  /* Index of flag connected to this building */
  int flag;
  /* Stock of this building */
  Stock stock[BUILDING_MAX_STOCK];
  unsigned int serf_index; /* Also used for burning building counter. */
  int progress;
  union u {
    Inventory *inventory;
    unsigned int tick; /* Used for burning building. */
    unsigned int level;
  } u;

 public:
  Building(Game *game, unsigned int index);

  MapPos get_position() { return pos; }
  void set_position(MapPos position) { pos = position; }

  unsigned int get_flag_index() { return flag; }
  void link_flag(unsigned int flag_index) { flag = flag_index; }

  bool has_main_serf() { return (serf_index != 0); }
  unsigned int get_main_serf() { return serf_index; }
  void set_main_serf(unsigned int serf) { serf_index = serf; }

  int get_burning_counter() { return serf_index; }
  void set_burning_counter(int counter) { serf_index = counter; }
  void decrease_burning_counter(int delta) { serf_index -= delta; }

  /* Type of building. */
  Type get_type() { return type; }
  void set_type(Type type) { this->type = type; }
  bool is_military() { return (type == TypeHut) ||
                              (type == TypeTower) ||
                              (type == TypeFortress) ||
                              (type == TypeCastle); }
  /* Owning player of the building. */
  unsigned int get_owner() { return (bld & 3); }
  void set_owner(unsigned int owner) { bld = (bld & 0xfc) | owner; }
  /* Whether construction of the building is finished. */
  bool is_done() { return !((bld >> 7) & 1); }
  bool is_leveling() { return (!is_done() && progress == 0); }
  void done_build() { bld &= ~BIT(7); }
  void done_leveling() { progress = 1; }
  Map::Object start_building(Type type);
  int get_progress() { return progress; }
  void increase_progress(int delta) { progress += delta; }
  void increase_mining() { progress = (progress << 1) & 0xffff; }
  void drop_progress() { progress = 0; }
  void set_under_attack() { progress |= BIT(0); }
  bool is_under_attack() { return BIT_TEST(progress, 0); }

  /* The military state of the building. Higher values mean that
   the building is closer to the enemy. */
  int get_state() { return (serf & 3); }
  void set_state(int state) { serf = (serf & 0xfc) | state; }
  /* Building is currently playing back a sound effect. */
  bool playing_sfx() { return (BIT_TEST(serf, 3) != 0); }
  void start_playing_sfx() { serf |= BIT(3); }
  void stop_playing_sfx() { serf &= ~BIT(3); }
  /* Building is active (specifics depend on building type). */
  bool is_active() { return (BIT_TEST(serf, 4) != 0); }
  void start_activity() { serf |= BIT(4); }
  void stop_activity() { serf &= ~BIT(4); }
  /* Building is burning. */
  bool is_burning() { return (BIT_TEST(serf, 5) != 0); }
  void burnup() { serf |= BIT(5); }
  /* Building has an associated serf. */
  bool has_serf() { return (BIT_TEST(serf, 6) != 0); }
  void serf_arrive() { serf |= BIT(6); }
  void serf_gone() { serf &= ~BIT(6); }
  /* Building has succesfully requested a serf. */
  bool serf_requested() { return (BIT_TEST(serf, 7) != 0); }
  void serf_request_complete() { serf &= ~BIT(7); }
  void serf_request_failed() { serf &= ~BIT(7); }
  void request_serf() { serf |= BIT(7); }
  /* Building has requested a serf but none was available. */
  bool serf_request_fail() { return (BIT_TEST(serf, 2) != 0); }
  void clear_serf_request_failure() { serf &= ~BIT(2); }
  void knight_request_granted();

  /* Building has inventory and the inventory pointer is valid. */
  bool has_inventory() { return (stock[0].requested == 0xff); }
  Inventory *get_inventory() { return u.inventory; }
  void set_inventory(Inventory *inventory) { u.inventory = inventory; }

  unsigned int get_level() { return u.level; }
  void set_level(unsigned int level) { u.level = level; }

  unsigned int get_tick() { return u.tick; }
  void set_tick(unsigned int tick) { u.tick = tick; }

  unsigned int get_knight_count() { return stock[0].available; }

  unsigned int waiting_stone() {
    return stock[1].available; }  // Stone allways in stock #1
  unsigned int waiting_planks() {
    return stock[0].available; }  // Planks allways in stock #0
  unsigned int military_gold_count();

  void cancel_transported_resource(Resource::Type res);

  Serf *call_defender_out();
  Serf *call_attacker_out(int knight_index);

  bool add_requested_resource(Resource::Type res, bool fix_priority);
  bool is_stock_active(int stock_num) { return (stock[stock_num].type > 0); }
  unsigned int get_res_count_in_stock(int stock_num) {
    return stock[stock_num].available; }
  Resource::Type get_res_type_in_stock(int stock_num) {
    return stock[stock_num].type; }
  void stock_init(unsigned int stock_num, Resource::Type type,
                  unsigned int maximum);
  void remove_stock();
  int get_max_priority_for_resource(Resource::Type resource, int minimum = 0);
  int get_maximum_in_stock(int stock_num) { return stock[stock_num].maximum; }
  int get_requested_in_stock(int stock_num) {
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

  int pigs_count() { return stock[1].available; }
  void send_pig_to_butcher() { stock[1].available -= 1; }
  void place_new_pig() { stock[1].available += 1; }

  void boat_clear() { stock[1].available = 0; }
  void boat_do() { stock[1].available++; }

  void requested_knight_arrived();
  void requested_knight_attacking_on_walk() { stock[0].requested -= 1; }
  void requested_knight_defeat_on_walk() {
    if (!has_inventory()) stock[0].requested -= 1; }
  bool is_enough_place_for_knight();
  bool knight_come_back_from_fight(Serf *knight);
  void knight_occupy();

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

  bool send_serf_to_building(Building *building, Serf::Type type,
                             Resource::Type res1, Resource::Type res2);
};


int building_get_score_from_type(Building::Type type);


#endif  // SRC_BUILDING_H_
