/*
 * flag.h - Flag related functions.
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

#ifndef SRC_FLAG_H_
#define SRC_FLAG_H_

#include <vector>

#include "src/building.h"
#include "src/misc.h"
#include "src/objects.h"

typedef struct {
  int path_len;
  int serf_count;
  int flag_index;
  dir_t flag_dir;
  int serfs[16];
} serf_path_info_t;

/* Max number of resources waiting at a flag */
#define FLAG_MAX_RES_COUNT  8

class building_t;
class player_t;
class save_reader_binary_t;
class save_reader_text_t;
class save_writer_text_t;

class resource_slot_t {
 public:
  resource_type_t type;
  dir_t dir;
  unsigned int dest;
};

class flag_t : public game_object_t {
 protected:
  map_pos_t pos; /* ADDITION */
  int path_con;
  int endpoint;
  resource_slot_t slot[FLAG_MAX_RES_COUNT];

  int search_num;
  dir_t search_dir;
  int transporter;
  int length[6];
  union {
    building_t *b[6];
    flag_t *f[6];
    void *v[6];
  } other_endpoint;
  int other_end_dir[6];

  int bld_flags;
  int bld2_flags;

 public:
  flag_t(game_t *game, unsigned int index);

  map_pos_t get_position() { return pos; }
  void set_position(map_pos_t pos) { this->pos = pos; }

  /* Bitmap of all directions with outgoing paths. */
  int paths() { return path_con & 0x3f; }
  void add_path(dir_t dir, bool water);
  void del_path(dir_t dir);
  /* Whether a path exists in a given direction. */
  bool has_path(dir_t dir) { return ((path_con & (1 << (dir))) != 0); }

  void prioritize_pickup(dir_t dir, player_t *player);

  /* Owner of this flag. */
  unsigned int get_owner() { return (path_con >> 6) & 3; }
  void set_owner(unsigned int owner) { path_con = (owner << 6) |
                                               (path_con & 0x3f); }

  /* Bitmap showing whether the outgoing paths are land paths. */
  int land_paths() { return endpoint & 0x3f; }
  /* Whether the path in the given direction is a water path. */
  bool is_water_path(dir_t dir) { return !(endpoint & (1 << (dir))); }
  /* Whether a building is connected to this flag. If so, the pointer to
   the other endpoint is a valid building pointer. (Always at UP LEFT direction). */
  bool has_building() { return (endpoint >> 6) & 1; }

  /* Whether resources exist that are not yet scheduled. */
  bool has_resources() { return (endpoint >> 7) & 1; }

  /* Bitmap showing whether the outgoing paths have transporters
   servicing them. */
  int transporters() { return transporter & 0x3f; }
  /* Whether the path in the given direction has a transporter
   serving it. */
  bool has_transporter(dir_t dir) {
    return ((transporter & (1 << (dir))) != 0); }
  /* Whether this flag has tried to request a transporter without success. */
  bool serf_request_fail() { return (transporter >> 7) & 1; }
  void serf_request_clear() { transporter &= ~BIT(7); }

  /* Current number of transporters on path. */
  unsigned int free_transporter_count(dir_t dir) { return length[dir] & 0xf; }
  void transporter_to_serve(dir_t dir) { length[dir] -= 1; }
  /* Length category of path determining max number of transporters. */
  unsigned int length_category(dir_t dir) { return (length[dir] >> 4) & 7; }
  /* Whether a transporter serf was successfully requested for this path. */
  bool serf_requested(dir_t dir) { return (length[dir] >> 7) & 1; }
  void cancel_serf_request(dir_t dir) { length[dir] &= ~BIT(7); }
  void complete_serf_request(dir_t dir) {
    length[dir] &= ~BIT(7);
    length[dir] += 1;
  }

  /* The slot that is scheduled for pickup by the given path. */
  unsigned int scheduled_slot(dir_t dir) { return other_end_dir[dir] & 7; }
  /* The direction from the other endpoint leading back to this flag. */
  dir_t get_other_end_dir(dir_t dir) {
    return (dir_t)((other_end_dir[dir] >> 3) & 7); }
  flag_t *get_other_end_flag(dir_t dir) { return other_endpoint.f[dir]; }
  /* Whether the given direction has a resource pickup scheduled. */
  bool is_scheduled(dir_t dir) { return (other_end_dir[dir] >> 7) & 1; }
  bool pick_up_resource(int slot, resource_type_t *res, unsigned int *dest);
  bool drop_resource(resource_type_t res, unsigned int dest);
  bool has_empty_slot();
  void remove_all_resources();
  resource_type_t get_resource_at_slot(int slot);

  /* Whether this flag has an inventory building. */
  bool has_inventory() { return ((bld_flags >> 6) & 1); }
  /* Whether this inventory accepts resources. */
  bool accepts_resources() { return ((bld2_flags >> 7) & 1); }
  /* Whether this inventory accepts serfs. */
  bool accepts_serfs() { return ((bld_flags >> 7) & 1); }

  void set_has_inventory() { bld_flags |= BIT(6); }
  void set_accepts_resources(bool accepts) { accepts ? bld2_flags |= BIT(7) :
                                                       bld2_flags &= ~BIT(7); }
  void set_accepts_serfs(bool accepts) { accepts ? bld_flags |= BIT(7) :
                                                   bld_flags &= ~BIT(7); }
  void clear_flags() { bld_flags = 0; bld2_flags = 0; }

  friend save_reader_binary_t&
    operator >> (save_reader_binary_t &reader, flag_t &flag);
  friend save_reader_text_t&
    operator >> (save_reader_text_t &reader, flag_t &flag);
  friend save_writer_text_t&
    operator << (save_writer_text_t &writer, flag_t &flag);

  int schedule_known_dest_cb_(flag_t *src, flag_t *dest, int slot);

  void reset_transport(flag_t *other);

  void reset_destination_of_stolen_resources();

  void link_building(building_t *building);
  void unlink_building();
  building_t *get_building() { return other_endpoint.b[DIR_UP_LEFT]; }

  void invalidate_resource_path(dir_t dir);

  int find_nearest_inventory_for_resource();
  int find_nearest_inventory_for_serf();

  void link_with_flag(flag_t *dest_flag, bool water_path, int length,
                      dir_t in_dir, dir_t out_dir);

  void update();

  /* Get road length category value for real length.
   Determines number of serfs servicing the path segment.(?) */
  static int get_road_length_value(int length);

  void restore_path_serf_info(dir_t dir, serf_path_info_t *data);

  void set_search_dir(dir_t dir) { search_dir = dir; }
  dir_t get_search_dir() { return search_dir; }
  void clear_search_id() { search_num = 0; }

  bool can_demolish();

  void merge_paths(map_pos_t pos);

  static void fill_path_serf_info(game_t *game, map_pos_t pos, dir_t dir,
                                  serf_path_info_t *data);

 protected:
  void fix_scheduled();

  void schedule_slot_to_unknown_dest(int slot);
  void schedule_slot_to_known_dest(int slot, unsigned int res_waiting[4]);
  bool call_transporter(dir_t dir, bool water);

  friend class flag_search_t;
};

typedef bool flag_search_func(flag_t *flag, void *data);

class flag_search_t {
 protected:
  game_t *game;
  std::vector<flag_t*> queue;
  int id;

 public:
  explicit flag_search_t(game_t *game);

  int get_id() { return id; }
  void add_source(flag_t *flag);
  bool execute(flag_search_func *callback,
               bool land, bool transporter, void *data);

  static bool single(flag_t *src, flag_search_func *callback,
                     bool land, bool transporter, void *data);
};

#endif  // SRC_FLAG_H_
