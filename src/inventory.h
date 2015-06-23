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

class flag_t;
class save_reader_binary_t;
class save_reader_text_t;
class save_writer_text_t;

typedef enum {
  mode_in = 0,    // 00
  mode_stop = 1,  // 01
  mode_out = 3,   // 11
} inventory_mode_t;

class inventory_t {
 protected:
  unsigned int index;

  int player_num;
  /* Index of flag connected to this inventory */
  int flag;
  /* Index of building containing this inventory */
  int building;
  /* Count of resources */
  int resources[26];
  /* Resources waiting to be moved out */
  struct {
    resource_type_t type;
    unsigned int dest;
  } out_queue[2];
  /* Count of serfs waiting to move out */
  unsigned int serfs_out;
  /* Count of generic serfs */
  int generic_count;
  int res_dir;
  /* Indices to serfs of each type */
  int serfs[27];

 public:
  explicit inventory_t(unsigned int index);

  unsigned int get_index() { return index; }

  int get_player_num() { return player_num; }
  void set_player_num(int player_num) { this->player_num = player_num; }

  int get_flag_index() { return flag; }
  void set_flag_index(int flag_index) { flag = flag_index; }

  int get_building_index() { return building; }
  void set_building_index(int building_index) { building = building_index; }

  inventory_mode_t get_res_mode() { return (inventory_mode_t)(res_dir & 3); }
  void set_res_mode(inventory_mode_t mode) {
    res_dir = (res_dir & 0xFC) | mode; }
  inventory_mode_t get_serf_mode() {
    return (inventory_mode_t)((res_dir >> 2) & 3); }
  void set_serf_mode(inventory_mode_t mode) {
    res_dir = (res_dir & 0xF3) | (mode << 2); }
  bool have_any_out_mode() { return ((res_dir & 0x0A) != 0); }

  int get_serf_queue_length() { return serfs_out; }
  void sers_away() { serfs_out--; }
  bool call_out_serf(serf_t *serf);
  serf_t *call_out_serf(serf_type_t type);
  bool call_internal(serf_t *serf);
  serf_t *call_internal(serf_type_t type);
  void serf_come_back() { generic_count++; }
  int free_serf_count() { return generic_count; }
  bool have_serf(serf_type_t type) { return (serfs[type] != 0); }

  int get_count_of(resource_type_t resource) { return resources[resource]; }
  int *get_all_resources() { return resources; }
  void pop_resource(resource_type_t resource) { resources[resource]--; }
  void push_resource(resource_type_t resource);

  bool has_resource_in_queue() { return (out_queue[0].type != RESOURCE_NONE); }
  bool is_queue_full() { return (out_queue[1].type != RESOURCE_NONE); }
  void get_resource_from_queue(resource_type_t *res, int *dest);
  void reset_queue_for_dest(flag_t *flag);
  void lose_queue();

  bool has_food() { return (resources[RESOURCE_FISH] != 0 ||
                            resources[RESOURCE_MEAT] != 0 ||
                            resources[RESOURCE_BREAD] != 0); }

  /* Take resource from inventory and put in out queue.
   The resource must be present.*/
  void add_to_queue(resource_type_t type, unsigned int dest);

  /* Create initial resources */
  void apply_supplies_preset(int supplies);

  serf_t *call_transporter(bool water);

  bool promote_serf_to_knight(serf_t *serf);

  serf_t *spawn_serf_generic();
  bool specialize_serf(serf_t *serf, serf_type_t type);
  serf_t *specialize_free_serf(serf_type_t type);
  int serf_potencial_count(serf_type_t type);

  void serf_idle_in_stock(serf_t *serf);
  void knight_training(serf_t *serf, int p);

  friend save_reader_binary_t&
    operator >> (save_reader_binary_t &reader, inventory_t &inventory);
  friend save_reader_text_t&
    operator >> (save_reader_text_t &reader, inventory_t &inventory);
  friend save_writer_text_t&
    operator << (save_writer_text_t &writer, inventory_t &inventory);
};

#endif  // SRC_INVENTORY_H_
