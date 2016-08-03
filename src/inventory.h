/*
 * inventory.h - Resources related definitions.
 *
 * Copyright (C) 2015  Wicked_Digger <wicked_digger@mail.ru>
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

#ifndef SRC_INVENTORY_H_
#define SRC_INVENTORY_H_

#include "src/resource.h"
#include "src/serf.h"
#include "src/objects.h"

class Flag;
class SaveReaderBinary;
class SaveReaderText;
class SaveWriterText;

class Inventory : public GameObject {
 public:
  typedef enum Mode {
    ModeIn = 0,    // 00
    ModeStop = 1,  // 01
    ModeOut = 3,   // 11
  } Mode;

 protected:
  unsigned int owner;
  /* Index of flag connected to this inventory */
  unsigned int flag;
  /* Index of building containing this inventory */
  unsigned int building;
  /* Count of resources */
  resource_map_t resources;
  /* Resources waiting to be moved out */
  struct out_queue {
    Resource::Type type;
    unsigned int dest;
  } out_queue[2];
  /* Count of serfs waiting to move out */
  unsigned int serfs_out;
  /* Count of generic serfs */
  int generic_count;
  int res_dir;
  /* Indices to serfs of each type */
  Serf::SerfMap serfs;

 public:
  Inventory(Game *game, unsigned int index);

  unsigned int get_owner() { return owner; }
  void set_owner(unsigned int owner) { this->owner = owner; }

  int get_flag_index() { return flag; }
  void set_flag_index(int flag_index) { flag = flag_index; }

  int get_building_index() { return building; }
  void set_building_index(int building_index) { building = building_index; }

  Inventory::Mode get_res_mode() { return (Inventory::Mode)(res_dir & 3); }
  void set_res_mode(Inventory::Mode mode) { res_dir = (res_dir & 0xFC) | mode; }
  Inventory::Mode get_serf_mode() {
    return (Inventory::Mode)((res_dir >> 2) & 3); }
  void set_serf_mode(Inventory::Mode mode) {
    res_dir = (res_dir & 0xF3) | (mode << 2); }
  bool have_any_out_mode() { return ((res_dir & 0x0A) != 0); }

  int get_serf_queue_length() { return serfs_out; }
  void serf_away() { serfs_out--; }
  bool call_out_serf(Serf *serf);
  Serf *call_out_serf(Serf::Type type);
  bool call_internal(Serf *serf);
  Serf *call_internal(Serf::Type type);
  void serf_come_back() { generic_count++; }
  size_t free_serf_count() { return generic_count; }
  bool have_serf(Serf::Type type) { return (serfs[type] != 0); }

  unsigned int get_count_of(Resource::Type resource) {
    return resources[resource]; }
  resource_map_t get_all_resources() { return resources; }
  void pop_resource(Resource::Type resource) { resources[resource]--; }
  void push_resource(Resource::Type resource);

  bool has_resource_in_queue() {
    return (out_queue[0].type != Resource::TypeNone); }
  bool is_queue_full() {
    return (out_queue[1].type != Resource::TypeNone); }
  void get_resource_from_queue(Resource::Type *res, int *dest);
  void reset_queue_for_dest(Flag *flag);
  void lose_queue();

  bool has_food() { return (resources[Resource::TypeFish] != 0 ||
                            resources[Resource::TypeMeat] != 0 ||
                            resources[Resource::TypeBread] != 0); }

  /* Take resource from inventory and put in out queue.
   The resource must be present.*/
  void add_to_queue(Resource::Type type, unsigned int dest);

  /* Create initial resources */
  void apply_supplies_preset(unsigned int supplies);

  Serf *call_transporter(bool water);

  bool promote_serf_to_knight(Serf *serf);

  Serf *spawn_serf_generic();
  bool specialize_serf(Serf *serf, Serf::Type type);
  Serf *specialize_free_serf(Serf::Type type);
  unsigned int serf_potential_count(Serf::Type type);

  void serf_idle_in_stock(Serf *serf);
  void knight_training(Serf *serf, int p);

  friend SaveReaderBinary&
    operator >> (SaveReaderBinary &reader, Inventory &inventory);
  friend SaveReaderText&
    operator >> (SaveReaderText &reader, Inventory &inventory);
  friend SaveWriterText&
    operator << (SaveWriterText &writer, Inventory &inventory);
};

#endif  // SRC_INVENTORY_H_
