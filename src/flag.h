/*
 * flag.h - Flag related functions.
 *
 * Copyright (C) 2013-2018  Jon Lund Steffensen <jonlst@gmail.com>
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
#include "src/objects.h"

typedef struct SerfPathInfo {
  int path_len;
  int serf_count;
  int flag_index;
  Direction flag_dir;
  int serfs[16];
} SerfPathInfo;

/* Max number of resources waiting at a flag */
#define FLAG_MAX_RES_COUNT  8

class Building;
class Player;
class SaveReaderBinary;
class SaveReaderText;
class SaveWriterText;

class Flag : public GameObject {
  /* The computer player reads the fields as the original does. */
  friend class AI;
  /* The consistency check after loading reads them too. */
  friend class IntegrityCheck;
 protected:
  class ResourceSlot {
   public:
    Resource::Type type;
    Direction dir;
    unsigned int dest;
  };

 protected:
  unsigned int owner;
  MapPos pos; /* ADDITION */
  int path_con;               /* Directions with paths. */
  int land_path_set;          /* Directions with land paths. */
  bool building_connected;    /* A building at the up left. */
  bool resources_waiting;     /* Resources not yet scheduled. */
  ResourceSlot slot[FLAG_MAX_RES_COUNT];

  int search_num;
  Direction search_dir;
  int transporter_set;        /* Directions with transporters. */
  bool serf_request_failed;   /* A transporter was requested in vain. */
  /* Of each path. */
  unsigned int free_transporters[6];
  unsigned int path_length_category[6];
  bool transporter_requested[6];
  union other_endpoint {
    Building *b[6];
    Flag *f[6];
    void *v[6];
  } other_endpoint;
  unsigned int pickup_slot[6];     /* The slot scheduled for pickup. */
  Direction other_end_direction[6];
  bool pickup_scheduled[6];

  /* Of the inventory of the building. */
  bool inventory_building;
  bool serfs_accepted;
  bool resources_accepted;

  /* The fields as the bytes of the original, for the saves. */
  int get_endpoint_bits() const;
  void set_endpoint_bits(int bits);
  int get_transporter_bits() const;
  void set_transporter_bits(int bits);
  int get_length_bits(int dir) const;
  void set_length_bits(int dir, int bits);
  int get_other_end_bits(int dir) const;
  void set_other_end_bits(int dir, int bits);

 public:
  Flag(Game *game, unsigned int index);

  MapPos get_position() const { return pos; }
  void set_position(MapPos pos) { this->pos = pos; }

  /* Bitmap of all directions with outgoing paths. */
  int paths() const { return path_con & 0x3f; }
  void add_path(Direction dir, bool water);
  void del_path(Direction dir);
  /* Whether a path exists in a given direction. */
  bool has_path(Direction dir) const {
    return ((path_con & (1 << (dir))) != 0); }

  void prioritize_pickup(Direction dir, Player *player);

  /* Owner of this flag. */
  unsigned int get_owner() const { return owner; }
  void set_owner(unsigned int _owner) { owner = _owner; }

  /* Bitmap showing whether the outgoing paths are land paths. */
  int land_paths() const { return land_path_set; }
  /* Whether the path in the given direction is a water path. */
  bool is_water_path(Direction dir) const {
    return !(land_path_set & (1 << (dir))); }
  /* Whether a building is connected to this flag. If so, the pointer to
   the other endpoint is a valid building pointer.
   (Always at UP LEFT direction). */
  bool has_building() const { return building_connected; }

  /* Whether resources exist that are not yet scheduled. */
  bool has_resources() const { return resources_waiting; }

  /* Bitmap showing whether the outgoing paths have transporters
   servicing them. */
  int transporters() const { return transporter_set; }
  /* Whether the path in the given direction has a transporter
   serving it. */
  bool has_transporter(Direction dir) const {
    return ((transporter_set & (1 << (dir))) != 0); }
  /* Whether this flag has tried to request a transporter without success. */
  bool serf_request_fail() const { return serf_request_failed; }
  void serf_request_clear() { serf_request_failed = false; }

  /* Current number of transporters on path. */
  unsigned int free_transporter_count(Direction dir) const {
    return free_transporters[dir]; }
  void transporter_to_serve(Direction dir) { free_transporters[dir] -= 1; }
  /* Length category of path determining max number of transporters. */
  unsigned int length_category(Direction dir) const {
    return path_length_category[dir]; }
  /* Whether a transporter serf was successfully requested for this path. */
  bool serf_requested(Direction dir) const {
    return transporter_requested[dir]; }
  void cancel_serf_request(Direction dir) {
    transporter_requested[dir] = false; }
  void complete_serf_request(Direction dir) {
    transporter_requested[dir] = false;
    free_transporters[dir] += 1;
  }

  /* The slot that is scheduled for pickup by the given path. */
  unsigned int scheduled_slot(Direction dir) const {
    return pickup_slot[dir]; }
  /* The direction from the other endpoint leading back to this flag. */
  Direction get_other_end_dir(Direction dir) const {
    return other_end_direction[dir]; }
  Flag *get_other_end_flag(Direction dir) const {
    return other_endpoint.f[dir]; }
  /* Whether the given direction has a resource pickup scheduled. */
  bool is_scheduled(Direction dir) const {
    return pickup_scheduled[dir]; }
  bool pick_up_resource(unsigned int slot, Resource::Type *res,
                        unsigned int *dest);
  bool drop_resource(Resource::Type res, unsigned int dest);
  void switch_resource(unsigned int slot, Resource::Type *res,
                       unsigned int *dest);
  bool has_empty_slot() const;
  void remove_all_resources();
  Resource::Type get_resource_at_slot(int slot) const;

  /* Whether this flag has an inventory building. */
  bool has_inventory() const { return inventory_building; }
  /* Whether this inventory accepts resources. */
  bool accepts_resources() const { return resources_accepted; }
  /* Whether this inventory accepts serfs. */
  bool accepts_serfs() const { return serfs_accepted; }

  void set_has_inventory() { inventory_building = true; }
  void set_accepts_resources(bool accepts) { resources_accepted = accepts; }
  void set_accepts_serfs(bool accepts) { serfs_accepted = accepts; }
  void clear_flags() {
    inventory_building = false;
    serfs_accepted = false;
    resources_accepted = false;
  }

  friend SaveReaderBinary&
    operator >> (SaveReaderBinary &reader, Flag &flag);
  friend SaveReaderText&
    operator >> (SaveReaderText &reader, Flag &flag);
  friend SaveWriterText&
    operator << (SaveWriterText &writer, Flag &flag);

  bool schedule_known_dest_cb_(Flag *src, Flag *dest, int slot);

  void reset_transport(Flag *other);

  void reset_destination_of_stolen_resources();

  void link_building(Building *building);
  void unlink_building();
  Building *get_building() { return other_endpoint.b[DirectionUpLeft]; }

  void invalidate_resource_path(Direction dir);

  int find_nearest_inventory_for_resource();
  int find_nearest_inventory_for_serf();

  void link_with_flag(Flag *dest_flag, bool water_path, size_t length,
                      Direction in_dir, Direction out_dir);

  void update();

  /* Get road length category value for real length.
   Determines number of serfs servicing the path segment.(?) */
  static size_t get_road_length_value(size_t length);

  void restore_path_serf_info(Direction dir, SerfPathInfo *data);

  void set_search_dir(Direction dir) { search_dir = dir; }
  Direction get_search_dir() const { return search_dir; }
  void clear_search_id() { search_num = 0; }

  bool can_demolish() const;

  void merge_paths(MapPos pos);

  static void fill_path_serf_info(Game *game, MapPos pos, Direction dir,
                                  SerfPathInfo *data);

 protected:
  void fix_scheduled();

  void schedule_slot_to_unknown_dest(int slot);
  void schedule_slot_to_known_dest(int slot, unsigned int res_waiting[4]);
  bool call_transporter(Direction dir, bool water);

  friend class FlagSearch;
};

typedef bool flag_search_func(Flag *flag, void *data);

class FlagSearch {
 protected:
  Game *game;
  std::vector<Flag*> queue;
  int id;

 public:
  explicit FlagSearch(Game *game);

  int get_id() { return id; }
  void add_source(Flag *flag);
  bool execute(flag_search_func *callback,
               bool land, bool transporter, void *data);

  static bool single(Flag *src, flag_search_func *callback,
                     bool land, bool transporter, void *data);
};

#endif  // SRC_FLAG_H_
