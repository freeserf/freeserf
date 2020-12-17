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

#include <algorithm>

#include "src/game.h"
#include "src/savegame.h"
#include "src/log.h"
#include "src/inventory.h"

#define SEARCH_MAX_DEPTH  0x10000

FlagSearch::FlagSearch(Game *game_) {
  game = game_;
  id = game->next_search_id();
}

void
FlagSearch::add_source(Flag *flag) {
  queue.push_back(flag);
  flag->search_num = id;
}

bool
FlagSearch::execute(flag_search_func *callback, bool land,
                    bool transporter, void *data) {
  for (int i = 0; i < SEARCH_MAX_DEPTH && !queue.empty(); i++) {
    Flag *flag = queue.front();
    queue.erase(queue.begin());

    if (callback(flag, data)) {
      /* Clean up */
      queue.clear();
      return true;
    }

    for (Direction i : cycle_directions_ccw()) {
      if ((!land || !flag->is_water_path(i)) &&
          (!transporter || flag->has_transporter(i)) &&
          flag->other_endpoint.f[i]->search_num != id) {
        flag->other_endpoint.f[i]->search_num = id;
        flag->other_endpoint.f[i]->search_dir = flag->search_dir;
        Flag *other_flag = flag->other_endpoint.f[i];
        queue.push_back(other_flag);
      }
    }
  }

  /* Clean up */
  queue.clear();

  return false;
}

bool
FlagSearch::single(Flag *src, flag_search_func *callback, bool land,
                   bool transporter, void *data) {
  FlagSearch search(src->get_game());
  search.add_source(src);
  return search.execute(callback, land, transporter, data);
}

Flag::Flag(Game *game, unsigned int index)
  : GameObject(game, index)
  , owner(-1)
  , pos(0)
  , path_con(0)
  , endpoint(0)
  , slot{}
  , search_num(0)
  , search_dir(DirectionRight)
  , transporter(0)
  , length{}
  , other_endpoint{}
  , other_end_dir{}
  , bld_flags(0)
  , bld2_flags(0) {
  for (int j = 0; j < FLAG_MAX_RES_COUNT; j++) {
    slot[j].type = Resource::TypeNone;
    slot[j].dest = 0;
    slot[j].dir = DirectionNone;
  }
}

void
Flag::add_path(Direction dir, bool water) {
  path_con |= BIT(dir);
  if (water) {
    endpoint &= ~BIT(dir);
  } else {
    endpoint |= BIT(dir);
  }
  transporter &= ~BIT(dir);
}

void
Flag::del_path(Direction dir) {
  path_con &= ~BIT(dir);
  endpoint &= ~BIT(dir);
  transporter &= ~BIT(dir);

  if (serf_requested(dir)) {
    cancel_serf_request(dir);
    unsigned int dest = game->get_map()->get_obj_index(pos);
    for (Serf *serf : game->get_serfs_related_to(dest, dir)) {
      serf->path_deleted(dest, dir);
    }
  }

  other_end_dir[dir] &= 0x78;
  other_endpoint.f[dir] = NULL;

  /* Mark resource path for recalculation if they would
   have followed the removed path. */
  invalidate_resource_path(dir);
}

bool
Flag::pick_up_resource(unsigned int from_slot, Resource::Type *res,
                       unsigned int *dest) {
  if (from_slot >= FLAG_MAX_RES_COUNT) {
    throw ExceptionFreeserf("Wrong flag slot index.");
  }

  if (slot[from_slot].type == Resource::TypeNone) {
    return false;
  }

  *res = slot[from_slot].type;
  *dest = slot[from_slot].dest;
  slot[from_slot].type = Resource::TypeNone;
  slot[from_slot].dir = DirectionNone;

  fix_scheduled();

  return true;
}

bool
Flag::drop_resource(Resource::Type res, unsigned int dest) {
  if (res < Resource::TypeNone || res > Resource::GroupFood) {
    throw ExceptionFreeserf("Wrong resource type.");
  }

  for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
    if (slot[i].type == Resource::TypeNone) {
      slot[i].type = res;
      slot[i].dest = dest;
      slot[i].dir = DirectionNone;
      endpoint |= BIT(7);
      return true;
    }
  }

  return false;
}

bool
Flag::has_empty_slot() const {
  int scheduled_slots = 0;
  for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
    if (slot[i].type != Resource::TypeNone) {
      scheduled_slots++;
    }
  }

  return (scheduled_slots != FLAG_MAX_RES_COUNT);
}

void
Flag::remove_all_resources() {
  for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
    if (slot[i].type != Resource::TypeNone) {
      int res = slot[i].type;
      unsigned int dest = slot[i].dest;
      game->cancel_transported_resource((Resource::Type)res, dest);
      game->lose_resource((Resource::Type)res);
    }
  }
}

Resource::Type
Flag::get_resource_at_slot(int slot_) const {
  return slot[slot_].type;
}

void
Flag::fix_scheduled() {
  int scheduled_slots = 0;
  for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
    if (slot[i].type != Resource::TypeNone) {
      scheduled_slots++;
    }
  }

  if (scheduled_slots) {
    endpoint |= BIT(7);
  } else {
    endpoint &= ~BIT(7);
  }
}

typedef struct ScheduleUnknownDestData {
  Resource::Type resource;
  int max_prio;
  Flag *flag;
} ScheduleUnknownDestData;

static bool
schedule_unknown_dest_cb(Flag *flag, void *data) {
  ScheduleUnknownDestData *dest_data =
    static_cast<ScheduleUnknownDestData*>(data);
  if (flag->has_building()) {
    Building *building = flag->get_building();

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
Flag::schedule_slot_to_unknown_dest(int slot_num) {
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

  Resource::Type res = slot[slot_num].type;
  if (routable[res]) {
    FlagSearch search(game);
    search.add_source(this);

    /* Handle food as one resource group */
    if (res == Resource::TypeMeat ||
        res == Resource::TypeFish ||
        res == Resource::TypeBread) {
      res = Resource::GroupFood;
    }

    ScheduleUnknownDestData data;
    data.resource = res;
    data.flag = NULL;
    data.max_prio = 0;

    search.execute(schedule_unknown_dest_cb, false, true, &data);
    if (data.flag != nullptr) {
      Log::Verbose["game"] << "dest for flag " << index << " res " << slot
                           << " found: flag " << data.flag->get_index();
      Building *dest_bld = data.flag->other_endpoint.b[DirectionUpLeft];

      if (!dest_bld->add_requested_resource(res, true)) {
        throw ExceptionFreeserf("Failed to request resource.");
      }

      slot[slot_num].dest = dest_bld->get_flag_index();
      endpoint |= BIT(7);
      return;
    }
  }

  /* Either this resource cannot be routed to a destination
   other than an inventory or such destination could not be
   found. Send to inventory instead. */
  int r = find_nearest_inventory_for_resource();
  if (r < 0 || r == static_cast<int>(index)) {
    /* No path to inventory was found, or
     resource is already at destination.
     In the latter case we need to move it
     forth and back once before it can be delivered. */
    if (transporters() == 0) {
      endpoint |= BIT(7);
    } else {
      Direction dir = DirectionNone;
      for (Direction d : cycle_directions_ccw()) {
        if (has_transporter(d)) {
          dir = d;
          break;
        }
      }

      if ((dir < DirectionRight) || (dir > DirectionUp)) {
        throw ExceptionFreeserf("Failed to request resource.");
      }

      if (!is_scheduled(dir)) {
        other_end_dir[dir] = BIT(7) |
          (other_end_dir[dir] & 0x38) | slot_num;
      }
      slot[slot_num].dir = dir;
    }
  } else {
    this->slot[slot_num].dest = r;
    endpoint |= BIT(7);
  }
}

static bool
find_nearest_inventory_search_cb(Flag *flag, void *data) {
  Flag **dest = reinterpret_cast<Flag**>(data);
  if (flag->accepts_resources()) {
    *dest = flag;
    return true;
  }
  return false;
}

/* Return the flag index of the inventory nearest to flag. */
int
Flag::find_nearest_inventory_for_resource() {
  Flag *dest = NULL;
  FlagSearch::single(this, find_nearest_inventory_search_cb, false, true,
                     &dest);
  if (dest != NULL) return dest->get_index();

  return -1;
}

static bool
flag_search_inventory_search_cb(Flag *flag, void *data) {
  int *dest_index = static_cast<int*>(data);
  if (flag->accepts_serfs()) {
    Building *building = flag->get_building();
    *dest_index = building->get_flag_index();
    return true;
  }

  return false;
}

int
Flag::find_nearest_inventory_for_serf() {
  int dest_index = -1;
  FlagSearch::single(this, flag_search_inventory_search_cb, true, false,
                     &dest_index);

  return dest_index;
}

typedef struct ScheduleKnownDestData {
  Flag *src;
  Flag *dest;
  int slot;
} ScheduleKnownDestData;

static bool
schedule_known_dest_cb(Flag *flag, void *data) {
  ScheduleKnownDestData *dest_data =
    static_cast<ScheduleKnownDestData*>(data);
  return (flag->schedule_known_dest_cb_(dest_data->src,
                                        dest_data->dest,
                                        dest_data->slot) != 0);
}

bool
Flag::schedule_known_dest_cb_(Flag *src, Flag *dest, int _slot) {
  if (this == dest) {
    /* Destination found */
    if (this->search_dir != 6) {
      if (!src->is_scheduled(this->search_dir)) {
        /* Item is requesting to be fetched */
        src->other_end_dir[this->search_dir] =
          BIT(7) | (src->other_end_dir[this->search_dir] & 0x78) | _slot;
      } else {
        Player *player = game->get_player(this->get_owner());
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
    return true;
  }

  return false;
}

void
Flag::schedule_slot_to_known_dest(int slot_, unsigned int res_waiting[4]) {
  FlagSearch search(game);

  search_num = search.get_id();
  search_dir = DirectionNone;
  int tr = transporters();

  int sources = 0;

  /* Directions where transporters are idle (zero slots waiting) */
  int flags = (res_waiting[0] ^ 0x3f) & transporter;

  if (flags != 0) {
    for (Direction k : cycle_directions_ccw()) {
      if (BIT_TEST(flags, k)) {
        tr &= ~BIT(k);
        Flag *other_flag = other_endpoint.f[k];
        if (other_flag->search_num != search.get_id()) {
          other_flag->search_dir = k;
          search.add_source(other_flag);
          sources += 1;
        }
      }
    }
  }

  if (tr != 0) {
    for (int j = 0; j < 3; j++) {
      flags = res_waiting[j] ^ res_waiting[j+1];
      for (Direction k : cycle_directions_ccw()) {
        if (BIT_TEST(flags, k)) {
          tr &= ~BIT(k);
          Flag *other_flag = other_endpoint.f[k];
          if (other_flag->search_num != search.get_id()) {
            other_flag->search_dir = k;
            search.add_source(other_flag);
            sources += 1;
          }
        }
      }
    }

    if (tr != 0) {
      flags = res_waiting[3];
      for (Direction k : cycle_directions_ccw()) {
        if (BIT_TEST(flags, k)) {
          tr &= ~BIT(k);
          Flag *other_flag = other_endpoint.f[k];
          if (other_flag->search_num != search.get_id()) {
            other_flag->search_dir = k;
            search.add_source(other_flag);
            sources += 1;
          }
        }
      }
      if (flags == 0) return;
    }
  }

  if (sources > 0) {
    ScheduleKnownDestData data;
    data.src = this;
    data.dest = game->get_flag(this->slot[slot_].dest);
    data.slot = slot_;
    bool r = search.execute(schedule_known_dest_cb, false, true, &data);
    if (!r || data.dest == this) {
      /* Unable to deliver */
      game->cancel_transported_resource(this->slot[slot_].type,
                                        this->slot[slot_].dest);
      this->slot[slot_].dest = 0;
      endpoint |= BIT(7);
    }
  } else {
    endpoint |= BIT(7);
  }
}

void
Flag::prioritize_pickup(Direction dir, Player *player) {
  int res_next = -1;
  int res_prio = -1;

  for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
    if (slot[i].type != Resource::TypeNone) {
      /* Use flag_prio to prioritize resource pickup. */
      Direction res_dir = slot[i].dir;
      Resource::Type res_type = slot[i].type;
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
Flag::invalidate_resource_path(Direction dir) {
  for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
    if (slot[i].type != Resource::TypeNone && slot[i].dir == dir) {
      slot[i].dir = DirectionNone;
      endpoint |= BIT(7);
    }
  }
}

/* Get road length category value for real length.
 Determines number of serfs servicing the path segment.(?) */
size_t
Flag::get_road_length_value(size_t length) {
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
Flag::link_with_flag(Flag *dest_flag, bool water_path, size_t length_,
                     Direction in_dir, Direction out_dir) {
  dest_flag->add_path(in_dir, water_path);
  add_path(out_dir, water_path);

  dest_flag->other_end_dir[in_dir] =
    (dest_flag->other_end_dir[in_dir] & 0xc7) | (out_dir << 3);
  other_end_dir[out_dir] = (other_end_dir[out_dir] & 0xc7) | (in_dir << 3);

  size_t len = get_road_length_value(length_);

  dest_flag->length[in_dir] = len << 4;
  this->length[out_dir] = len << 4;

  dest_flag->other_endpoint.f[in_dir] = this;
  other_endpoint.f[out_dir] = dest_flag;
}

void
Flag::restore_path_serf_info(Direction dir, SerfPathInfo *data) {
  const int max_path_serfs[] = { 1, 2, 3, 4, 6, 8, 11, 15 };

  Flag *other_flag = game->get_flag(data->flag_index);
  Direction other_dir = data->flag_dir;

  add_path(dir, other_flag->is_water_path(other_dir));

  other_flag->transporter &= ~BIT(other_dir);

  size_t len = Flag::get_road_length_value(data->path_len);

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
      Serf *serf = game->get_serf(data->serfs[i]);
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
Flag::can_demolish() const {
  int connected = 0;
  void *other_end = NULL;

  for (Direction d : cycle_directions_cw()) {
    if (has_path(d)) {
      if (is_water_path(d)) return false;

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

// return true if a flag has any roads connected to it
bool
Flag::is_connected() const {
	for (Direction d : cycle_directions_cw()) {
		if (has_path(d))
			return true;
	}
	return false;
}

/* Find a transporter at pos and change it to state. */
static int
change_transporter_state_at_pos(Game *game, MapPos pos, Serf::State state) {
  for (Serf *serf : game->get_serfs_at_pos(pos)) {
    if (serf->change_transporter_state_at_pos(pos, state)) {
      return serf->get_index();
    }
  }

  return -1;
}

static int
wake_transporter_at_flag(Game *game, MapPos pos) {
  return change_transporter_state_at_pos(game, pos, Serf::StateWakeAtFlag);
}

static int
wake_transporter_on_path(Game *game, MapPos pos) {
  return change_transporter_state_at_pos(game, pos, Serf::StateWakeOnPath);
}

void
Flag::fill_path_serf_info(Game *game, MapPos pos, Direction dir,
                          SerfPathInfo *data) {
  PMap map = game->get_map();
  if (map->get_idle_serf(pos)) wake_transporter_at_flag(game, pos);

  int serf_count = 0;
  int path_len = 0;

  /* Handle first position. */
  if (map->has_serf(pos)) {
    Serf *serf = game->get_serf_at_pos(pos);
    if (serf->get_state() == Serf::StateTransporting &&
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
  while (true) {
    path_len += 1;
    pos = map->move(pos, dir);
    paths = map->paths(pos);
    paths &= ~BIT(reverse_direction(dir));

    if (map->has_flag(pos)) break;

    /* Find out which direction the path follows. */
    for (Direction d : cycle_directions_cw()) {
      if (BIT_TEST(paths, d)) {
        dir = d;
        break;
      }
    }

    /* Check if there is a transporter waiting here. */
    if (map->get_idle_serf(pos)) {
      int index = wake_transporter_on_path(game, pos);
      if (index >= 0) data->serfs[serf_count++] = index;
    }

    /* Check if there is a serf occupying this space. */
    if (map->has_serf(pos)) {
      Serf *serf = game->get_serf_at_pos(pos);
      if (serf->get_state() == Serf::StateTransporting &&
          serf->get_walking_wait_counter() != -1) {
        serf->set_walking_wait_counter(0);
        data->serfs[serf_count++] = serf->get_index();
      }
    }
  }

  /* Handle last position. */
  if (map->has_serf(pos)) {
    Serf *serf = game->get_serf_at_pos(pos);
    if ((serf->get_state() == Serf::StateTransporting &&
         serf->get_walking_wait_counter() != -1) ||
        serf->get_state() == Serf::StateDelivering) {
      int d = serf->get_walking_dir();
      if (d < 0) d += 6;

      if (d == reverse_direction(dir)) {
        serf->set_walking_wait_counter(0);
        data->serfs[serf_count++] = serf->get_index();
      }
    }
  }

  /* Fill the rest of the struct. */
  data->path_len = path_len;
  data->serf_count = serf_count;
  data->flag_index = map->get_obj_index(pos);
  data->flag_dir = reverse_direction(dir);
}

void
Flag::merge_paths(MapPos pos_) {
  const int max_transporters[] = { 1, 2, 3, 4, 6, 8, 11, 15 };

  PMap map = game->get_map();
  if (!map->paths(pos_)) {
    return;
  }

  Direction path_1_dir = DirectionRight;
  Direction path_2_dir = DirectionRight;

  /* Find first direction */
  for (Direction d : cycle_directions_cw()) {
    if (map->has_path(pos_, d)) {
      path_1_dir = d;
      break;
    }
  }

  /* Find second direction */
  for (Direction d : cycle_directions_ccw()) {
    if (map->has_path(pos_, d)) {
      path_2_dir = d;
      break;
    }
  }

  SerfPathInfo path_1_data;
  SerfPathInfo path_2_data;

  fill_path_serf_info(game, pos_, path_1_dir, &path_1_data);
  fill_path_serf_info(game, pos_, path_2_dir, &path_2_data);

  Flag *flag_1 = game->get_flag(path_1_data.flag_index);
  Flag *flag_2 = game->get_flag(path_2_data.flag_index);
  Direction dir_1 = path_1_data.flag_dir;
  Direction dir_2 = path_2_data.flag_dir;

  flag_1->other_end_dir[dir_1] =
    (flag_1->other_end_dir[dir_1] & 0xc7) | (dir_2 << 3);
  flag_2->other_end_dir[dir_2] =
    (flag_2->other_end_dir[dir_2] & 0xc7) | (dir_1 << 3);

  flag_1->other_endpoint.f[dir_1] = flag_2;
  flag_2->other_endpoint.f[dir_2] = flag_1;

  flag_1->transporter &= ~BIT(dir_1);
  flag_2->transporter &= ~BIT(dir_2);

  size_t len = Flag::get_road_length_value((size_t)path_1_data.path_len +
                                           (size_t)path_2_data.path_len);
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
  Game::ListSerfs serfs = game->get_serfs_related_to(flag_1->get_index(),
                                                     dir_1);
  Game::ListSerfs serfs2 = game->get_serfs_related_to(flag_2->get_index(),
                                                      dir_2);
  serfs.insert(serfs.end(), serfs2.begin(), serfs2.end());
  for (Serf *serf : serfs) {
    serf->path_merged2(flag_1->get_index(), dir_1,
                       flag_2->get_index(), dir_2);
  }
}

void
Flag::update() {
  const int max_transporters[] = { 1, 2, 3, 4, 6, 8, 11, 15 };
  Log::Verbose["flag"] << " inside Flag::update() - Count and store in bitfield which directions";
  /* Count and store in bitfield which directions
   have strictly more than 0,1,2,3 slots waiting. */
  unsigned int res_waiting[4] = {0};
  for (int j = 0; j < FLAG_MAX_RES_COUNT; j++) {
    if (slot[j].type != Resource::TypeNone && slot[j].dir != DirectionNone) {
      Direction res_dir = slot[j].dir;
      for (int k = 0; k < 4; k++) {
        if (!BIT_TEST(res_waiting[k], res_dir)) {
          res_waiting[k] |= BIT(res_dir);
          break;
        }
      }
    }
  }

  Log::Verbose["flag"] << " inside Flag::update() - Count of total resources waiting at flag";
  /* Count of total resources waiting at flag */
  int waiting_count = 0;

  if (has_resources()) {
    endpoint &= ~BIT(7);
    for (int slot_ = 0; slot_ < FLAG_MAX_RES_COUNT; slot_++) {
      if (slot[slot_].type != Resource::TypeNone) {
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

  Log::Verbose["flag"] << " inside Flag::update() - Update transporter flags, decide if serf needs to be sent to road";
  /* Update transporter flags, decide if serf needs to be sent to road */
  for (Direction j : cycle_directions_ccw()) {
    if (has_path(j)) {
      if (serf_requested(j)) {
        if (BIT_TEST(res_waiting[2], j)) {
          if (waiting_count >= 7) {
            transporter &= BIT(j);
          }
        } else if (free_transporter_count(j) != 0) {
          transporter |= BIT(j);
        }
      } else if (free_transporter_count(j) == 0 ||
                 BIT_TEST(res_waiting[2], j)) {
        int max_tr = max_transporters[length_category(j)];
        if (free_transporter_count(j) < (unsigned int)max_tr &&
            !serf_request_fail()) {
          bool r = call_transporter(j, is_water_path(j));
          if (!r) transporter |= BIT(7);
        }
        if (waiting_count >= 7) {
          transporter &= BIT(j);
        }
      } else {
        transporter |= BIT(j);
      }
    }
  }
  Log::Verbose["flag"] << "end of Flag::update()";
}

typedef struct SendSerfToRoadData {
  Inventory *inventory;
  int water;
} SendSerfToRoadData;

static bool
send_serf_to_road_search_cb(Flag *flag, void *data) {
  SendSerfToRoadData *road_data =
    static_cast<SendSerfToRoadData*>(data);
  if (flag->has_inventory()) {
    /* Inventory reached */
    Building *building = flag->get_building();
	// got read access violation here first time dec13 2020:   'Unhandled exception thrown: read access violation. **this** was 0xFFFFFFFFFFFFFF77. occurred"
    Inventory *inventory = building->get_inventory();
    if (!road_data->water) {
      if (inventory->have_serf(Serf::TypeTransporter)) {
        road_data->inventory = inventory;
        return true;
      }
    } else {
      if (inventory->have_serf(Serf::TypeSailor)) {
        road_data->inventory = inventory;
        return true;
      }
    }

    if (road_data->inventory == NULL &&
        inventory->have_serf(Serf::TypeGeneric) &&
        (!road_data->water ||
         inventory->get_count_of(Resource::TypeBoat) > 0)) {
      road_data->inventory = inventory;
    }
  }
  return false;
}

bool
Flag::call_transporter(Direction dir, bool water) {
  Flag *src_2 = other_endpoint.f[dir];
  Direction dir_2 = get_other_end_dir(dir);

  search_dir = DirectionRight;
  // got a write access violation here, src_2 was nullptr... check first?
  // got it again
  // couple more times
  // I think this is because I wasn't checking that this player owns the flag!  added that code.  hopefully fixes it
  // nope still getting it
  // try just ignoring this if it happens?
  // I think this is happening every time... look into it
  if (src_2 == nullptr) {
	  Log::Warn["flag"] << " Flag::call_transporter - nullptr found when doing Flag *src_2 = other_endpoint.f[dir], returning false  FIND OUT WHY!";
	  return false;
  }
  src_2->search_dir = DirectionDownRight;

  FlagSearch search(game);
  search.add_source(this);
  search.add_source(src_2);

  SendSerfToRoadData data;
  data.inventory = NULL;
  data.water = water;
  search.execute(send_serf_to_road_search_cb, true, false, &data);
  Inventory *inventory = data.inventory;
  if (inventory == NULL) {
	  // this happens if castle not built yet, changing from Warn to Verbose
	  Log::Verbose["flag"] << " Flag::call_transporter - inventory is NULL, returning false - it could not find nearest inventory?";
    return false;
  }

  Serf *serf = data.inventory->call_transporter(water);

  Flag *dest_flag = game->get_flag(inventory->get_flag_index());

  length[dir] |= BIT(7);
  src_2->length[dir_2] |= BIT(7);

  Flag *src = this;
  if (dest_flag->search_dir == src_2->search_dir) {
    src = src_2;
    dir = dir_2;
  }

  serf->go_out_from_inventory(inventory->get_index(), src->get_index(), dir);
  Log::Debug["flag"] << " Flag::call_transporter - successfully called transporter";
  return true;
}

void
Flag::reset_transport(Flag *other) {
  for (int slot_ = 0; slot_ < FLAG_MAX_RES_COUNT; slot_++) {
    if (other->slot[slot_].type != Resource::TypeNone &&
        other->slot[slot_].dest == index) {
      other->slot[slot_].dest = 0;
      other->endpoint |= BIT(7);

      if (other->slot[slot_].dir != DirectionNone) {
        Direction dir = other->slot[slot_].dir;
        Player *player = game->get_player(other->get_owner());
        other->prioritize_pickup(dir, player);
      }
    }
  }
}

void
Flag::reset_destination_of_stolen_resources() {
  for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
    if (slot[i].type != Resource::TypeNone) {
      Resource::Type res = slot[i].type;
      game->cancel_transported_resource(res, slot[i].dest);
      slot[i].dest = 0;
    }
  }
}

void
Flag::link_building(Building *building) {
  other_endpoint.b[DirectionUpLeft] = building;
  endpoint |= BIT(6);
}

void
Flag::unlink_building() {
  other_endpoint.b[DirectionUpLeft] = nullptr;
  endpoint &= ~BIT(6);
  clear_flags();
}

SaveReaderBinary&
operator >> (SaveReaderBinary &reader, Flag &flag) {
  flag.pos = 0; /* Set correctly later. */
  uint16_t val16;
  reader >> val16;  // 0
  flag.search_num = val16;

  uint8_t val8;
  reader >> val8;  // 2
  flag.search_dir = (Direction)val8;

  reader >> val8;  // 3
  flag.owner = (val8 >> 6) & 3;
  flag.path_con = val8 & 0x3f;

  reader >> val8;  // 4
  flag.endpoint = val8;

  reader >> val8;  // 5
  flag.transporter = val8;

  for (Direction j : cycle_directions_cw()) {
    reader >> val8;  // 6+j
    flag.length[j] = val8;
  }

  for (int j = 0; j < 8; j++) {
    reader >> val8;  // 12+j
    flag.slot[j].type = (Resource::Type)((val8 & 0x1f)-1);
    flag.slot[j].dir = (Direction)(((val8 >> 5) & 7)-1);
  }
  for (int j = 0; j < 8; j++) {
    reader >> val16;  // 20+j*2
    flag.slot[j].dest = val16;
  }

  // base + 36
  for (Direction j : cycle_directions_cw()) {
    uint32_t val32;
    reader >> val32;
    int offset = val32;

    /* Other endpoint could be a building in direction up left. */
    if (j == DirectionUpLeft && flag.has_building()) {
      unsigned int index = offset/18;
      flag.other_endpoint.b[j] = flag.get_game()->create_building(index);
    } else {
      if (!flag.has_path(j)) {
        flag.other_endpoint.f[j] = NULL;
        continue;
      }
      if (offset < 0) {
        flag.other_endpoint.f[j] = NULL;
      } else {
        unsigned int index = offset/70;
        flag.other_endpoint.f[j] = flag.get_game()->create_flag(index);
      }
    }
  }

  // base + 60
  for (Direction j : cycle_directions_cw()) {
    reader >> val8;
    flag.other_end_dir[j] = val8;
  }

  reader >> val8;  // 66
  flag.bld_flags = val8;

  reader >> val8;  // 67
  if (flag.has_building()) {
    flag.other_endpoint.b[DirectionUpLeft]->set_priority_in_stock(0, val8);
  }

  reader >> val8;  // 68
  flag.bld2_flags = val8;

  reader >> val8;  // 69
  if (flag.has_building()) {
    flag.other_endpoint.b[DirectionUpLeft]->set_priority_in_stock(1, val8);
  }

  return reader;
}

SaveReaderText&
operator >> (SaveReaderText &reader, Flag &flag) {
  unsigned int x = 0;
  unsigned int y = 0;
  reader.value("pos")[0] >> x;
  reader.value("pos")[1] >> y;
  flag.pos = flag.get_game()->get_map()->pos(x, y);
  reader.value("search_num") >> flag.search_num;
  reader.value("search_dir") >> flag.search_dir;
  unsigned int val;
  reader.value("path_con") >> val;
  if (reader.has_value("owner")) {
    flag.path_con = val;
    reader.value("owner") >> flag.owner;
  } else {
    flag.path_con = (val & 0x3f);
    flag.owner = ((val >> 6) & 3);
  }
  reader.value("endpoints") >> flag.endpoint;
  reader.value("transporter") >> flag.transporter;

  for (Direction i : cycle_directions_cw()) {
    int len;
    reader.value("length")[i] >> len;
    flag.length[i] = len;
    unsigned int obj_index;
    reader.value("other_endpoint")[i] >> obj_index;
    if (flag.has_building() && (i == DirectionUpLeft)) {
      flag.other_endpoint.b[DirectionUpLeft] =
                                    flag.get_game()->create_building(obj_index);
    } else {
      Flag *other_flag = NULL;
      if (obj_index != 0) {
        other_flag = flag.get_game()->create_flag(obj_index);
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

SaveWriterText&
operator << (SaveWriterText &writer, Flag &flag) {
  writer.value("pos") << flag.game->get_map()->pos_col(flag.pos);
  writer.value("pos") << flag.game->get_map()->pos_row(flag.pos);
  writer.value("search_num") << flag.search_num;
  writer.value("search_dir") << flag.search_dir;
  writer.value("path_con") << flag.path_con;
  writer.value("owner") << flag.owner;
  writer.value("endpoints") << flag.endpoint;
  writer.value("transporter") << flag.transporter;

  for (Direction d : cycle_directions_cw()) {
    writer.value("length") << static_cast<int>(flag.length[d]);
    if (d == DirectionUpLeft && flag.has_building()) {
      writer.value("other_endpoint") <<
        flag.other_endpoint.b[DirectionUpLeft]->get_index();
    } else {
      if (flag.has_path((Direction)d)) {
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
