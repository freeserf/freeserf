/*
 * flag.cc - Flag related functions.
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

#include "src/flag.h"

#include <cstdlib>
#include <cassert>
#include <algorithm>

#include "src/game.h"
#include "src/savegame.h"
#include "src/log.h"
#include "src/inventory.h"

#define SEARCH_MAX_DEPTH  0x10000

int
flag_search_t::next_search_id() {
  game.flag_search_counter += 1;

  /* If we're back at zero the counter has overflown,
     everything needs a reset to be safe. */
  if (game.flag_search_counter == 0) {
    game.flag_search_counter += 1;
    for (flags_t::iterator i = game.flags.begin();
         i != game.flags.end(); ++i) {
      flag_t *flag = *i;
      flag->search_num = 0;
    }
  }

  return game.flag_search_counter;
}

flag_search_t::flag_search_t() {
  id = next_search_id();
}

void
flag_search_t::add_source(flag_t *flag) {
  queue.push_back(flag);
  flag->search_num = id;
}

bool
flag_search_t::execute(flag_search_func *callback, bool land,
                       bool transporter, void *data) {
  for (int i = 0; i < SEARCH_MAX_DEPTH && !queue.empty(); i++) {
    flag_t *flag = queue.front();
    queue.erase(queue.begin());

    if (callback(flag, data)) {
      /* Clean up */
      queue.clear();
      return true;
    }

    for (int i = 0; i < 6; i++) {
      if ((!land || !flag->is_water_path((dir_t)(5-i))) &&
          (!transporter || flag->has_transporter((dir_t)(5-i))) &&
          flag->other_endpoint.f[5-i]->search_num != id) {
        flag->other_endpoint.f[5-i]->search_num = id;
        flag->other_endpoint.f[5-i]->search_dir = flag->search_dir;
        flag_t *other_flag = flag->other_endpoint.f[5-i];
        queue.push_back(other_flag);
      }
    }
  }

  /* Clean up */
  queue.clear();

  return false;
}

bool
flag_search_t::single(flag_t *src, flag_search_func *callback,
                      bool land, bool transporter, void *data) {
  flag_search_t search;
  search.add_source(src);
  return search.execute(callback, land, transporter, data);
}

flag_t::flag_t(unsigned int index) {
  this->index = index;
  pos = 0;
  search_num = 0;
  search_dir = DIR_RIGHT;
  path_con = 0;
  endpoint = 0;
  transporter = 0;
  for (int j = 0; j < FLAG_MAX_RES_COUNT; j++) {
    slot[j].type = RESOURCE_NONE;
  }
  bld_flags = 0;
  bld2_flags = 0;
  for (int i = 0; i < 6; i++) {
    length[i] = 0;
    other_end_dir[i] = 0;
    other_endpoint.f[i] = 0;
  }
}

void
flag_t::add_path(dir_t dir, bool water) {
  path_con |= BIT(dir);
  if (water) {
    endpoint &= ~BIT(dir);
  } else {
    endpoint |= BIT(dir);
  }
  transporter &= ~BIT(dir);
}

void
flag_t::del_path(dir_t dir) {
  path_con &= ~BIT(dir);
  endpoint &= ~BIT(dir);
  transporter &= ~BIT(dir);

  if (serf_requested(dir)) {
    cancel_serf_request(dir);
    int dest = game.map->get_obj_index(pos);

    for (serfs_t::iterator i = game.serfs.begin();
         i != game.serfs.end(); ++i) {
      serf_t *serf = *i;
      serf->path_deleted(dest, dir);
    }
  }

  other_end_dir[dir] &= 0x78;

  /* Mark resource path for recalculation if they would
   have followed the removed path. */
  invalidate_resource_path(dir);
}

bool
flag_t::pick_up_resource(int slot, resource_type_t *res, unsigned int *dest) {
  if (this->slot[slot].type == RESOURCE_NONE) {
    return false;
  }

  *res = this->slot[slot].type;
  *dest = this->slot[slot].dest;
  this->slot[slot].type = RESOURCE_NONE;
  this->slot[slot].dir = DIR_NONE;

  fix_scheduled();

  return true;
}

bool
flag_t::drop_resource(resource_type_t res, unsigned int dest) {
  for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
    if (slot[i].type == RESOURCE_NONE) {
      slot[i].type = res;
      slot[i].dest = dest;
      slot[i].dir = DIR_NONE;
      endpoint |= BIT(7);
      return true;
    }
  }

  return false;
}

bool
flag_t::has_empty_slot() {
  int scheduled_slots = 0;
  for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
    if (slot[i].type != RESOURCE_NONE) {
      scheduled_slots++;
    }
  }

  return (scheduled_slots != FLAG_MAX_RES_COUNT);
}

void
flag_t::remove_all_resources() {
  for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
    if (slot[i].type != RESOURCE_NONE) {
      int res = slot[i].type;
      unsigned int dest = slot[i].dest;
      game_cancel_transported_resource((resource_type_t)res, dest);
      game_lose_resource((resource_type_t)res);
    }
  }
}

resource_type_t
flag_t::get_resource_at_slot(int slot_) {
  return slot[slot_].type;
}

void
flag_t::fix_scheduled() {
  int scheduled_slots = 0;
  for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
    if (slot[i].type != RESOURCE_NONE) {
      scheduled_slots++;
    }
  }

  if (scheduled_slots) {
    endpoint |= BIT(7);
  } else {
    endpoint &= ~BIT(7);
  }
}

typedef struct {
  resource_type_t resource;
  int max_prio;
  flag_t *flag;
} schedule_unknown_dest_data_t;

static bool
schedule_unknown_dest_cb(flag_t *flag, void *data) {
  schedule_unknown_dest_data_t *dest_data =
    static_cast<schedule_unknown_dest_data_t*>(data);
  if (flag->has_building()) {
    building_t *building = flag->get_building();

    int bld_prio = building->get_max_priority_for_resource(dest_data->resource);
    if (bld_prio > dest_data->max_prio) {
      dest_data->max_prio = bld_prio;
      dest_data->flag = flag;
    }

    if (dest_data->max_prio > 204) return true;
  }

  return false;
}

void
flag_t::schedule_slot_to_unknown_dest(int slot_num) {
  /* Resources which should be routed directly to
   buildings requesting them. Resources not listed
   here will simply be moved to an inventory. */
  const int routable[] = {
    1,  // RESOURCE_FISH
    1,  // RESOURCE_PIG
    1,  // RESOURCE_MEAT
    1,  // RESOURCE_WHEAT
    1,  // RESOURCE_FLOUR
    1,  // RESOURCE_BREAD
    1,  // RESOURCE_LUMBER
    1,  // RESOURCE_PLANK
    0,  // RESOURCE_BOAT
    1,  // RESOURCE_STONE
    1,  // RESOURCE_IRONORE
    1,  // RESOURCE_STEEL
    1,  // RESOURCE_COAL
    1,  // RESOURCE_GOLDORE
    1,  // RESOURCE_GOLDBAR
    0,  // RESOURCE_SHOVEL
    0,  // RESOURCE_HAMMER
    0,  // RESOURCE_ROD
    0,  // RESOURCE_CLEAVER
    0,  // RESOURCE_SCYTHE
    0,  // RESOURCE_AXE
    0,  // RESOURCE_SAW
    0,  // RESOURCE_PICK
    0,  // RESOURCE_PINCER
    0,  // RESOURCE_SWORD
    0,  // RESOURCE_SHIELD
    0,  // RESOURCE_GROUP_FOOD
  };

  resource_type_t res = slot[slot_num].type;
  if (routable[res]) {
    flag_search_t search;
    search.add_source(this);

    /* Handle food as one resource group */
    if (res == RESOURCE_MEAT ||
        res == RESOURCE_FISH ||
        res == RESOURCE_BREAD) {
      res = RESOURCE_GROUP_FOOD;
    }

    schedule_unknown_dest_data_t data;
    data.resource = res;
    data.flag = NULL;
    data.max_prio = 0;

    search.execute(schedule_unknown_dest_cb, false, true, &data);
    if (data.flag != NULL) {
      LOGV("game", "dest for flag %u res %i found: flag %u",
           index, slot, data.flag->get_index());
      building_t *dest_bld = data.flag->other_endpoint.b[DIR_UP_LEFT];

      assert(dest_bld->add_requested_resource(res, true));

      slot[slot_num].dest = dest_bld->get_flag_index();
      endpoint |= BIT(7);
      return;
    }
  }

  /* Either this resource cannot be routed to a destination
   other than an inventory or such destination could not be
   found. Send to inventory instead. */
  int r = find_nearest_inventory_for_resource();
  if (r < 0 || r == index) {
    /* No path to inventory was found, or
     resource is already at destination.
     In the latter case we need to move it
     forth and back once before it can be delivered. */
    if (transporters() == 0) {
      endpoint |= BIT(7);
    } else {
      int dir = -1;
      for (int i = DIR_RIGHT; i <= DIR_UP; i++) {
        int d = DIR_UP - i;
        if (has_transporter((dir_t)d)) {
          dir = d;
          break;
        }
      }

      assert(dir >= 0);

      if (!is_scheduled((dir_t)dir)) {
        other_end_dir[dir] = BIT(7) |
        (other_end_dir[dir] & 0x38) | slot_num;
      }
      slot[slot_num].dir = (dir_t)dir;
    }
  } else {
    this->slot[slot_num].dest = r;
    endpoint |= BIT(7);
  }
}

static bool
find_nearest_inventory_search_cb(flag_t *flag, void *data) {
  flag_t **dest = reinterpret_cast<flag_t**>(data);
  if (flag->accepts_resources()) {
    *dest = flag;
    return true;
  }
  return false;
}

/* Return the flag index of the inventory nearest to flag. */
int
flag_t::find_nearest_inventory_for_resource() {
  flag_t *dest = NULL;
  flag_search_t::single(this, find_nearest_inventory_search_cb,
                        false, true, &dest);
  if (dest != NULL) return dest->get_index();

  return -1;
}

static bool
flag_search_inventory_search_cb(flag_t *flag, void *data) {
  int *dest_index = static_cast<int*>(data);
  if (flag->accepts_serfs()) {
    building_t *building = flag->get_building();
    *dest_index = building->get_flag_index();
    return 1;
  }

  return false;
}

int
flag_t::find_nearest_inventory_for_serf() {
  int dest_index = -1;
  flag_search_t::single(this, flag_search_inventory_search_cb,
                        true, false, &dest_index);

  return dest_index;
}

typedef struct {
  flag_t *src;
  flag_t *dest;
  int slot;
} schedule_known_dest_data_t;

static bool
schedule_known_dest_cb(flag_t *flag, void *data) {
  schedule_known_dest_data_t *dest_data =
    static_cast<schedule_known_dest_data_t*>(data);
  return (flag->schedule_known_dest_cb_(dest_data->src,
                                        dest_data->dest,
                                        dest_data->slot) != 0);
}

int
flag_t::schedule_known_dest_cb_(flag_t *src, flag_t *dest, int _slot) {
  if (this == dest) {
    /* Destination found */
    if (this->search_dir != 6) {
      if (!src->is_scheduled(this->search_dir)) {
        /* Item is requesting to be fetched */
        src->other_end_dir[this->search_dir] =
          BIT(7) | (src->other_end_dir[this->search_dir] & 0x78) | _slot;
      } else {
        player_t *player = game.players[this->get_player()];
        int other_dir = src->other_end_dir[this->search_dir];
        int prio_old = player->get_flag_prio(src->slot[other_dir & 7].type);
        int prio_new = player->get_flag_prio(src->slot[_slot].type);
        if (prio_new > prio_old) {
          /* This item has the highest priority now */
          src->other_end_dir[this->search_dir] =
            (src->other_end_dir[this->search_dir] & 0xf8) | _slot;
        }
        src->slot[_slot].dir = this->search_dir;
      }
    }
    return 1;
  }

  return 0;
}

void
flag_t::schedule_slot_to_known_dest(int slot, unsigned int res_waiting[4]) {
  flag_search_t search;

  search_num = search.get_id();
  search_dir = DIR_UP_RIGHT;
  int tr = transporters();

  int sources = 0;

  /* Directions where transporters are idle (zero slots waiting) */
  int flags = (res_waiting[0] ^ 0x3f) & transporter;

  if (flags != 0) {
    for (int k = 0; k < 6; k++) {
      if (BIT_TEST(flags, 5-k)) {
        tr &= ~BIT(5-k);
        flag_t *other_flag = other_endpoint.f[5-k];
        if (other_flag->search_num != search.get_id()) {
          other_flag->search_dir = (dir_t)(5-k);
          search.add_source(other_flag);
          sources += 1;
        }
      }
    }
  }

  if (tr != 0) {
    for (int j = 0; j < 3; j++) {
      flags = res_waiting[j] ^ res_waiting[j+1];
      for (int k = 0; k < 6; k++) {
        if (BIT_TEST(flags, 5-k)) {
          tr &= ~BIT(5-k);
          flag_t *other_flag = other_endpoint.f[5-k];
          if (other_flag->search_num != search.get_id()) {
            other_flag->search_dir = (dir_t)(5-k);
            search.add_source(other_flag);
            sources += 1;
          }
        }
      }
    }

    if (tr != 0) {
      flags = res_waiting[3];
      for (int k = 0; k < 6; k++) {
        if (BIT_TEST(flags, 5-k)) {
          tr &= ~BIT(5-k);
          flag_t *other_flag = other_endpoint.f[5-k];
          if (other_flag->search_num != search.get_id()) {
            other_flag->search_dir = (dir_t)(5-k);
            search.add_source(other_flag);
            sources += 1;
          }
        }
      }
      if (flags == 0) return;
    }
  }

  if (sources > 0) {
    schedule_known_dest_data_t data;
    data.src = this;
    data.dest = game.flags[this->slot[slot].dest];
    data.slot = slot;
    bool r = search.execute(schedule_known_dest_cb, false, true, &data);
    if (!r || data.dest->search_dir == 6) {
      /* Unable to deliver */
      game_cancel_transported_resource(this->slot[slot].type,
                                       this->slot[slot].dest);
      this->slot[slot].dest = 0;
      endpoint |= BIT(7);
    }
  } else {
    endpoint |= BIT(7);
  }
}

void
flag_t::prioritize_pickup(dir_t dir, player_t *player) {
  int res_next = -1;
  int res_prio = -1;

  for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
    if (slot[i].type != RESOURCE_NONE) {
      /* Use flag_prio to prioritize resource pickup. */
      dir_t res_dir = slot[i].dir;
      resource_type_t res_type = slot[i].type;
      if (res_dir == dir && player->get_flag_prio(res_type) > res_prio) {
        res_next = i;
        res_prio = player->get_flag_prio(res_type);
      }
    }
  }

  other_end_dir[dir] &= 0x78;
  if (res_next > -1) other_end_dir[dir] |= BIT(7) | res_next;
}

void
flag_t::invalidate_resource_path(dir_t dir) {
  for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
    if (slot[i].type != RESOURCE_NONE &&
        slot[i].dir == dir) {
      slot[i].dir = DIR_NONE;
      endpoint |= BIT(7);
    }
  }
}

/* Get road length category value for real length.
 Determines number of serfs servicing the path segment.(?) */
int
flag_t::get_road_length_value(int length) {
  if (length >= 24) return 7;
  else if (length >= 18) return 6;
  else if (length >= 13) return 5;
  else if (length >= 10) return 4;
  else if (length >= 7) return 3;
  else if (length >= 6) return 2;
  else if (length >= 4) return 1;
  return 0;
}

void
flag_t::link_with_flag(flag_t *dest_flag, bool water_path,
                       int length, dir_t in_dir, dir_t out_dir) {
  dest_flag->add_path(in_dir, water_path);
  add_path(out_dir, water_path);

  dest_flag->other_end_dir[in_dir] =
    (dest_flag->other_end_dir[in_dir] & 0xc7) | (out_dir << 3);
  other_end_dir[out_dir] = (other_end_dir[out_dir] & 0xc7) | (in_dir << 3);

  int len = get_road_length_value(length);

  dest_flag->length[in_dir] = len << 4;
  this->length[out_dir] = len << 4;

  dest_flag->other_endpoint.f[in_dir] = this;
  other_endpoint.f[out_dir] = dest_flag;
}

void
flag_t::restore_path_serf_info(dir_t dir, serf_path_info_t *data) {
  const int max_path_serfs[] = { 1, 2, 3, 4, 6, 8, 11, 15 };

  flag_t *other_flag = game.flags[data->flag_index];
  dir_t other_dir = data->flag_dir;

  add_path(dir, other_flag->is_water_path(other_dir));

  other_flag->transporter &= ~BIT(other_dir);

  int len = flag_t::get_road_length_value(data->path_len);

  length[dir] = len << 4;
  other_flag->length[other_dir] =
    (0x80 & other_flag->length[other_dir]) | (len << 4);

  if (other_flag->serf_requested(other_dir)) {
    length[dir] |= BIT(7);
  }

  other_end_dir[dir] = (other_end_dir[dir] & 0xc7) | (other_dir << 3);
  other_flag->other_end_dir[other_dir] =
    (other_flag->other_end_dir[other_dir] & 0xc7) | (dir << 3);

  other_endpoint.f[dir] = other_flag;
  other_flag->other_endpoint.f[other_dir] = this;

  int max_serfs = max_path_serfs[len];
  if (serf_requested(dir)) max_serfs -= 1;

  if (data->serf_count > max_serfs) {
    for (int i = 0; i < data->serf_count - max_serfs; i++) {
      serf_t *serf = game.serfs[data->serfs[i]];
      serf->restore_path_serf_info();
    }
  }

  if (std::min(data->serf_count, max_serfs) > 0) {
    /* There are still transporters on the paths. */
    transporter |= BIT(dir);
    other_flag->transporter |= BIT(other_dir);

    length[dir] |= std::min(data->serf_count, max_serfs);
    other_flag->length[other_dir] |= std::min(data->serf_count, max_serfs);
  }
}

bool
flag_t::can_demolish() {
  int connected = 0;
  void *other_end = NULL;

  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    if (has_path((dir_t)d)) {
      if (is_water_path((dir_t)d)) return false;

      connected += 1;

      if (other_end != NULL) {
        if (other_endpoint.v[d] == other_end) {
          return false;
        }
      } else {
        other_end = other_endpoint.v[d];
      }
    }
  }

  if (connected == 2) return true;

  return false;
}

/* Find a transporter at pos and change it to state. */
static int
change_transporter_state_at_pos(map_pos_t pos, serf_state_t state) {
  for (serfs_t::iterator i = game.serfs.begin();
       i != game.serfs.end(); ++i) {
    serf_t *serf = *i;
    if (serf->change_transporter_state_at_pos(pos, state)) {
      return serf->get_index();
    }
  }

  return -1;
}

static int
wake_transporter_at_flag(map_pos_t pos) {
  return change_transporter_state_at_pos(pos, SERF_STATE_WAKE_AT_FLAG);
}

static int
wake_transporter_on_path(map_pos_t pos) {
  return change_transporter_state_at_pos(pos, SERF_STATE_WAKE_ON_PATH);
}

void
flag_t::fill_path_serf_info(map_pos_t pos, dir_t dir, serf_path_info_t *data) {
  if (game.map->get_idle_serf(pos)) wake_transporter_at_flag(pos);

  int serf_count = 0;
  int path_len = 0;

  /* Handle first position. */
  if (game.map->get_serf_index(pos) != 0) {
    serf_t *serf = game.serfs[game.map->get_serf_index(pos)];
    if (serf->get_state() == SERF_STATE_TRANSPORTING &&
        serf->get_walking_wait_counter() != -1) {
      int d = serf->get_walking_dir();
      if (d < 0) d += 6;

      if (dir == d) {
        serf->set_walking_wait_counter(0);
        data->serfs[serf_count++] = serf->get_index();
      }
    }
  }

  /* Trace along the path to the flag at the other end. */
  int paths = 0;
  while (1) {
    path_len += 1;
    pos = game.map->move(pos, dir);
    paths = game.map->paths(pos);
    paths &= ~BIT(DIR_REVERSE(dir));

    if (game.map->has_flag(pos)) break;

    /* Find out which direction the path follows. */
    for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
      if (BIT_TEST(paths, d)) {
        dir = (dir_t)d;
        break;
      }
    }

    /* Check if there is a transporter waiting here. */
    if (game.map->get_idle_serf(pos)) {
      int index = wake_transporter_on_path(pos);
      if (index >= 0) data->serfs[serf_count++] = index;
    }

    /* Check if there is a serf occupying this space. */
    if (game.map->get_serf_index(pos) != 0) {
      serf_t *serf = game.serfs[game.map->get_serf_index(pos)];
      if (serf->get_state() == SERF_STATE_TRANSPORTING &&
          serf->get_walking_wait_counter() != -1) {
        serf->set_walking_wait_counter(0);
        data->serfs[serf_count++] = serf->get_index();
      }
    }
  }

  /* Handle last position. */
  if (game.map->get_serf_index(pos) != 0) {
    serf_t *serf = game.serfs[game.map->get_serf_index(pos)];
    if ((serf->get_state() == SERF_STATE_TRANSPORTING &&
         serf->get_walking_wait_counter() != -1) ||
        serf->get_state() == SERF_STATE_DELIVERING) {
      int d = serf->get_walking_dir();
      if (d < 0) d += 6;

      if (d == DIR_REVERSE(dir)) {
        serf->set_walking_wait_counter(0);
        data->serfs[serf_count++] = serf->get_index();
      }
    }
  }

  /* Fill the rest of the struct. */
  data->path_len = path_len;
  data->serf_count = serf_count;
  data->flag_index = game.map->get_obj_index(pos);
  data->flag_dir = DIR_REVERSE(dir);
}

void
flag_t::merge_paths(map_pos_t pos) {
  const int max_transporters[] = { 1, 2, 3, 4, 6, 8, 11, 15 };

  if (!game.map->paths(pos)) {
    return;
  }

  dir_t path_1_dir = DIR_RIGHT;
  dir_t path_2_dir = DIR_RIGHT;

  /* Find first direction */
  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    if (BIT_TEST(game.map->paths(pos), d)) {
      path_1_dir = (dir_t)d;
      break;
    }
  }

  /* Find second direction */
  for (int d = DIR_UP; d >= DIR_RIGHT; d--) {
    if (BIT_TEST(game.map->paths(pos), d)) {
      path_2_dir = (dir_t)d;
      break;
    }
  }

  serf_path_info_t path_1_data;
  serf_path_info_t path_2_data;

  fill_path_serf_info(pos, path_1_dir, &path_1_data);
  fill_path_serf_info(pos, path_2_dir, &path_2_data);

  flag_t *flag_1 = game.flags[path_1_data.flag_index];
  flag_t *flag_2 = game.flags[path_2_data.flag_index];
  dir_t dir_1 = path_1_data.flag_dir;
  dir_t dir_2 = path_2_data.flag_dir;

  flag_1->other_end_dir[dir_1] =
    (flag_1->other_end_dir[dir_1] & 0xc7) | (dir_2 << 3);
  flag_2->other_end_dir[dir_2] =
    (flag_2->other_end_dir[dir_2] & 0xc7) | (dir_1 << 3);

  flag_1->other_endpoint.f[dir_1] = flag_2;
  flag_2->other_endpoint.f[dir_2] = flag_1;

  flag_1->transporter &= ~BIT(dir_1);
  flag_2->transporter &= ~BIT(dir_2);

  int len = flag_t::get_road_length_value(path_1_data.path_len +
                                          path_2_data.path_len);
  flag_1->length[dir_1] = len << 4;
  flag_2->length[dir_2] = len << 4;

  int max_serfs = max_transporters[flag_1->length_category(dir_1)];
  int serf_count = path_1_data.serf_count + path_2_data.serf_count;
  if (serf_count > 0) {
    flag_1->transporter |= BIT(dir_1);
    flag_2->transporter |= BIT(dir_2);

    if (serf_count > max_serfs) {
      /* TODO 59B8B */
    }

    flag_1->length[dir_1] += serf_count;
    flag_2->length[dir_2] += serf_count;
  }

  /* Update serfs with reference to this flag. */
  for (serfs_t::iterator i = game.serfs.begin();
       i != game.serfs.end(); ++i) {
    serf_t *serf = *i;
    serf->path_merged2(flag_1->get_index(), dir_1,
                       flag_2->get_index(), dir_2);
  }
}

void
flag_t::update() {
  const int max_transporters[] = { 1, 2, 3, 4, 6, 8, 11, 15 };

  /* Count and store in bitfield which directions
   have strictly more than 0,1,2,3 slots waiting. */
  unsigned int res_waiting[4] = {0};
  for (int j = 0; j < FLAG_MAX_RES_COUNT; j++) {
    if (slot[j].type != RESOURCE_NONE &&
        slot[j].dir != DIR_NONE) {
      dir_t res_dir = slot[j].dir;
      for (int k = 0; k < 4; k++) {
        if (!BIT_TEST(res_waiting[k], res_dir)) {
          res_waiting[k] |= BIT(res_dir);
          break;
        }
      }
    }
  }

  /* Count of total resources waiting at flag */
  int waiting_count = 0;

  if (has_resources()) {
    endpoint &= ~BIT(7);
    for (int slot_ = 0; slot_ < FLAG_MAX_RES_COUNT; slot_++) {
      if (slot[slot_].type != RESOURCE_NONE) {
        waiting_count += 1;

        /* Only schedule the slot if it has not already
         been scheduled for fetch. */
        int res_dir = slot[slot_].dir;
        if (res_dir < 0) {
          if (slot[slot_].dest != 0) {
            /* Destination is known */
            schedule_slot_to_known_dest(slot_, res_waiting);
          } else {
            /* Destination is not known */
            schedule_slot_to_unknown_dest(slot_);
          }
        }
      }
    }
  }

  /* Update transporter flags, decide if serf needs to be sent to road */
  for (int j = 0; j < 6; j++) {
    if (has_path((dir_t)(5-j))) {
      if (serf_requested((dir_t)(5-j))) {
        if (BIT_TEST(res_waiting[2], 5-j)) {
          if (waiting_count >= 7) {
            transporter &= BIT(5-j);
          }
        } else if (free_transporter_count((dir_t)(5-j)) != 0) {
          transporter |= BIT(5-j);
        }
      } else if (free_transporter_count((dir_t)(5-j)) == 0 ||
                 BIT_TEST(res_waiting[2], 5-j)) {
        int max_tr = max_transporters[length_category((dir_t)(5-j))];
        if (free_transporter_count((dir_t)(5-j)) < (unsigned int)max_tr &&
            !serf_request_fail()) {
          bool r = call_transporter((dir_t)(5-j),
                                    is_water_path((dir_t)(5-j)));
          if (!r) transporter |= BIT(7);
        }
        if (waiting_count >= 7) {
          transporter &= BIT(5-j);
        }
      } else {
        transporter |= BIT(5-j);
      }
    }
  }
}

typedef struct {
  inventory_t *inventory;
  int water;
} send_serf_to_road_data_t;

static bool
send_serf_to_road_search_cb(flag_t *flag, void *data) {
  send_serf_to_road_data_t *road_data =
    static_cast<send_serf_to_road_data_t*>(data);
  if (flag->has_inventory()) {
    /* Inventory reached */
    building_t *building = flag->get_building();
    inventory_t *inventory = building->get_inventory();
    if (!road_data->water) {
      if (inventory->have_serf(SERF_TRANSPORTER)) {
        road_data->inventory = inventory;
        return true;
      }
    } else {
      if (inventory->have_serf(SERF_SAILOR)) {
        road_data->inventory = inventory;
        return true;
      }
    }

    if (road_data->inventory == NULL && inventory->have_serf(SERF_GENERIC) &&
        (!road_data->water || inventory->get_count_of(RESOURCE_BOAT) > 0)) {
      road_data->inventory = inventory;
    }
  }

  return false;
}

bool
flag_t::call_transporter(dir_t dir, bool water) {
  flag_t *src_2 = other_endpoint.f[dir];
  dir_t dir_2 = get_other_end_dir(dir);

  search_dir = DIR_RIGHT;
  src_2->search_dir = DIR_DOWN_RIGHT;

  flag_search_t search;
  search.add_source(this);
  search.add_source(src_2);

  send_serf_to_road_data_t data;
  data.inventory = NULL;
  data.water = water;
  search.execute(send_serf_to_road_search_cb, true, false, &data);
  inventory_t *inventory = data.inventory;
  if (inventory == NULL) {
    return false;
  }

  serf_t *serf = data.inventory->call_transporter(water);

  flag_t *dest_flag = game.flags[inventory->get_flag_index()];

  length[dir] |= BIT(7);
  src_2->length[dir_2] |= BIT(7);

  flag_t *src = this;
  if (dest_flag->search_dir == src_2->search_dir) {
    src = src_2;
    dir = dir_2;
  }

  serf->go_out_from_inventory(inventory->get_index(), src->get_index(), dir);

  return true;
}

void
flag_t::reset_transport(flag_t *other) {
  for (int slot_ = 0; slot_ < FLAG_MAX_RES_COUNT; slot_++) {
    if (other->slot[slot_].type != RESOURCE_NONE &&
        other->slot[slot_].dest == index) {
      other->slot[slot_].dest = 0;
      other->endpoint |= BIT(7);

      if (other->slot[slot_].dir != DIR_NONE) {
        dir_t dir = other->slot[slot_].dir;
        player_t *player = game.players[other->get_player()];
        other->prioritize_pickup(dir, player);
      }
    }
  }
}

void
flag_t::reset_destination_of_stolen_resources() {
  for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
    if (slot[i].type != RESOURCE_NONE) {
      resource_type_t res = slot[i].type;
      game_cancel_transported_resource(res, slot[i].dest);
      slot[i].dest = 0;
    }
  }
}

void
flag_t::link_building(building_t *building) {
  other_endpoint.b[DIR_UP_LEFT] = building;
  endpoint |= BIT(6);
}

void
flag_t::unlink_building() {
  other_endpoint.b[DIR_UP_LEFT] = NULL;
  endpoint &= ~BIT(6);
  clear_flags();
}

save_reader_binary_t&
operator >> (save_reader_binary_t &reader, flag_t &flag) {
  flag.pos = 0; /* Set correctly later. */
  uint16_t val16;
  reader >> val16;  // 0
  flag.search_num = val16;

  uint8_t val8;
  reader >> val8;  // 2
  flag.search_dir = (dir_t)val8;

  reader >> val8;  // 3
  flag.path_con = val8;

  reader >> val8;  // 4
  flag.endpoint = val8;

  reader >> val8;  // 5
  flag.transporter = val8;

  for (int j = 0; j < 6; j++) {
    reader >> val8;  // 6+j
    flag.length[j] = val8;
  }

  for (int j = 0; j < 8; j++) {
    reader >> val8;  // 12+j
    flag.slot[j].type = (resource_type_t)((val8 & 0x1f)-1);
    flag.slot[j].dir = (dir_t)(((val8 >> 5) & 7)-1);
  }
  for (int j = 0; j < 8; j++) {
    reader >> val16;  // 20+j*2
    flag.slot[j].dest = val16;
  }

  // base + 36
  for (int j = 0; j < 6; j++) {
    uint32_t val32;
    reader >> val32;
    int offset = val32;

    /* Other endpoint could be a building in direction up left. */
    if (j == 4 && flag.has_building()) {
      flag.other_endpoint.b[j] = game.buildings.get_or_insert(offset/18);
    } else {
      if (offset < 0) {
        flag.other_endpoint.f[j] = NULL;
      } else {
        flag.other_endpoint.f[j] = game.flags.get_or_insert(offset/70);
      }
    }
  }

  // base + 60
  for (int j = 0; j < 6; j++) {
    reader >> val8;
    flag.other_end_dir[j] = val8;
  }

  reader >> val8;  // 66
  flag.bld_flags = val8;

  reader >> val8;  // 67
  if (flag.has_building()) {
    flag.other_endpoint.b[DIR_UP_LEFT]->set_priority_in_stock(0, val8);
  }

  reader >> val8;  // 68
  flag.bld2_flags = val8;

  reader >> val8;  // 69
  if (flag.has_building()) {
    flag.other_endpoint.b[DIR_UP_LEFT]->set_priority_in_stock(1, val8);
  }

  return reader;
}

save_reader_text_t&
operator >> (save_reader_text_t &reader, flag_t &flag) {
  unsigned int x = 0;
  unsigned int y = 0;
  reader.value("pos")[0] >> x;
  reader.value("pos")[1] >> y;
  flag.pos = game.map->pos(x, y);
  reader.value("search_num") >> flag.search_num;
  reader.value("search_dir") >> flag.search_dir;
  reader.value("path_con") >> flag.path_con;
  reader.value("endpoints") >> flag.endpoint;
  reader.value("transporter") >> flag.transporter;

  for (int i = 0; i < 6; i++) {
    reader.value("length")[i] >> flag.length[i];
    unsigned int obj_index;
    reader.value("other_endpoint")[i] >> obj_index;
    if (flag.has_building() && (i == DIR_UP_LEFT)) {
      flag.other_endpoint.b[DIR_UP_LEFT] = game.buildings[obj_index];
    } else {
      flag_t *other_flag = NULL;
      if (obj_index != 0) {
        other_flag = game.flags.get_or_insert(obj_index);
      }
      flag.other_endpoint.f[i] = other_flag;
    }
    reader.value("other_end_dir")[i] >> flag.other_end_dir[i];
  }

  for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
    reader.value("slot.type")[i] >> flag.slot[i].type;
    reader.value("slot.dir")[i] >> flag.slot[i].dir;
    reader.value("slot.dest")[i] >> flag.slot[i].dest;
  }

  reader.value("bld_flags") >> flag.bld_flags;
  reader.value("bld2_flags") >> flag.bld2_flags;

  return reader;
}

save_writer_text_t&
operator << (save_writer_text_t &writer, flag_t &flag) {
  writer.value("pos") << flag.pos;
  writer.value("search_num") << flag.search_num;
  writer.value("search_dir") << flag.search_dir;
  writer.value("path_con") << flag.path_con;
  writer.value("endpoints") << flag.endpoint;
  writer.value("transporter") << flag.transporter;

  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    writer.value("length") << flag.length[d];
    if (d == DIR_UP_LEFT && flag.has_building()) {
      writer.value("other_endpoint") <<
        flag.other_endpoint.b[DIR_UP_LEFT]->get_index();
    } else {
      if (flag.has_path((dir_t)d)) {
        writer.value("other_endpoint") << flag.other_endpoint.f[d]->get_index();
      } else {
        writer.value("other_endpoint") << 0;
      }
    }
    writer.value("other_end_dir") << flag.other_end_dir[d];
  }

  for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
    writer.value("slot.type") << flag.slot[i].type;
    writer.value("slot.dir") << flag.slot[i].dir;
    writer.value("slot.dest") << flag.slot[i].dest;
  }

  writer.value("bld_flags") << flag.bld_flags;
  writer.value("bld2_flags") << flag.bld2_flags;

  return writer;
}
