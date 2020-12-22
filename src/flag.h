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
  int path_con;
  int endpoint;
  ResourceSlot slot[FLAG_MAX_RES_COUNT];

  int search_num;
  Direction search_dir;
  int transporter;
  size_t length[6];
  union other_endpoint {
    Building *b[6];
    Flag *f[6];
    void *v[6];
  } other_endpoint;
  int other_end_dir[6];

  int bld_flags;
  int bld2_flags;

 public:
  Flag(Game *game, unsigned int index);

  MapPos get_position() const { return pos; }
  void set_position(MapPos pos) { this->pos = pos; }

  /* Bitmap of all directions with outgoing paths. */
  // to understand, convert this to binary and read it last to first
  // ex: 44 = 101100 = -,-,+,+,-,+ or has paths in 3 dirs:  DownLeft, Left, Up
  int paths() const { return path_con & 0x3f; }
  void add_path(Direction dir, bool water);
  void del_path(Direction dir);
  /* Whether a path exists in a given direction. */
  bool has_path(Direction dir) const {
    // got a nullptr here, never seen before.  oct30 2020
    return ((path_con & (1 << (dir))) != 0); }

  void prioritize_pickup(Direction dir, Player *player);

  /* Owner of this flag. */
  unsigned int get_owner() const { return owner; }
  void set_owner(unsigned int _owner) { owner = _owner; }

  /* Bitmap showing whether the outgoing paths are land paths. */
  int land_paths() const { return endpoint & 0x3f; }
  /* Whether the path in the given direction is a water path. */
  bool is_water_path(Direction dir) const {
    return !(endpoint & (1 << (dir))); }
  /* Whether a building is connected to this flag. If so, the pointer to
   the other endpoint is a valid building pointer.
   (Always at UP LEFT direction). */
  bool has_building() const { return (endpoint >> 6) & 1; }

  /* Whether resources exist that are not yet scheduled. */
  bool has_resources() const { return (endpoint >> 7) & 1; }

  /* Bitmap showing whether the outgoing paths have transporters
   servicing them. */
  // same as path con, to understand convert to binary and reverse order of bits
  int transporters() const { return transporter & 0x3f; }
  /* Whether the path in the given direction has a transporter
   serving it. */
  bool has_transporter(Direction dir) const {
    return ((transporter & (1 << (dir))) != 0); }
  /* Whether this flag has tried to request a transporter without success. */
  bool serf_request_fail() const { return (transporter >> 7) & 1; }
  void serf_request_clear() { transporter &= ~BIT(7); }

  /* Current number of transporters on path. */
  unsigned int free_transporter_count(Direction dir) const {
    return length[dir] & 0xf; }
  void transporter_to_serve(Direction dir) { length[dir] -= 1; }
  /* Length category of path determining max number of transporters. */
  unsigned int length_category(Direction dir) const {
    return (length[dir] >> 4) & 7; }
  /* Whether a transporter serf was successfully requested for this path. */
  bool serf_requested(Direction dir) const { return (length[dir] >> 7) & 1; }
  void cancel_serf_request(Direction dir) { length[dir] &= ~BIT(7); }
  void complete_serf_request(Direction dir) {
    length[dir] &= ~BIT(7);
    length[dir] += 1;
  }

  /* The slot that is scheduled for pickup by the given path. */
  unsigned int scheduled_slot(Direction dir) const {
    return other_end_dir[dir] & 7; }
  /* The direction from the other endpoint leading back to this flag. */
  Direction get_other_end_dir(Direction dir) const {
    return (Direction)((other_end_dir[dir] >> 3) & 7); }
  Flag *get_other_end_flag(Direction dir) const {
    return other_endpoint.f[dir]; }
  /* Whether the given direction has a resource pickup scheduled. */
  bool is_scheduled(Direction dir) const {
    return (other_end_dir[dir] >> 7) & 1; }
  bool pick_up_resource(unsigned int slot, Resource::Type *res,
                        unsigned int *dest);
  bool drop_resource(Resource::Type res, unsigned int dest);
  bool has_empty_slot() const;
  void remove_all_resources();
  Resource::Type get_resource_at_slot(int slot) const;

  /* Whether this flag has an inventory building. */
  bool has_inventory() const { return ((bld_flags >> 6) & 1); }
  /* Whether this inventory accepts resources. */
  bool accepts_resources() const { return ((bld2_flags >> 7) & 1); }
  /* Whether this inventory accepts serfs. */
  bool accepts_serfs() const { return ((bld_flags >> 7) & 1); }

  void set_has_inventory() { bld_flags |= BIT(6); }
  void set_accepts_resources(bool accepts) { accepts ? bld2_flags |= BIT(7) :
                                                       bld2_flags &= ~BIT(7); }
  void set_accepts_serfs(bool accepts) { accepts ? bld_flags |= BIT(7) :
                                                   bld_flags &= ~BIT(7); }
  void clear_flags() { bld_flags = 0; bld2_flags = 0; }

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
  bool is_connected() const;

  void merge_paths(MapPos pos);

  static void fill_path_serf_info(Game *game, MapPos pos, Direction dir,
                                  SerfPathInfo *data);
  // copied from protected, so AI can call it to work around missing transporter bug
  bool call_transporter(Direction dir, bool water);

 protected:
  void fix_scheduled();

  void schedule_slot_to_unknown_dest(int slot);
  void schedule_slot_to_known_dest(int slot, unsigned int res_waiting[4]);
  // moved to public so AI can call it to work around missing transporter bug
  //bool call_transporter(Direction dir, bool water);

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
