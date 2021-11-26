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
#include "src/game-options.h"

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
  //Log::Info["flag"] << "thread #" << std::this_thread::get_id() << "debug: inside FlagSearch::execute with current flag queue.front()" << queue.front()->get_position();
  for (int i = 0; i < SEARCH_MAX_DEPTH && !queue.empty(); i++) {
    Flag *flag = queue.front();
    queue.erase(queue.begin());

    if (callback(flag, data)) {
      /* Clean up */
      queue.clear();
      return true;
    }

    /*
    // here is the original Flagsearch::execute function 
    //   before I added support for option_CanTransportSerfsInBoats
    // I broke it up and inverted all the negative checks to make it more readable
    // NOTE - this original function never actually checks flag->has_path(i)
    //  which is confusing!
    //   because it is designed to ONLY work for:
    //    routing serfs - disallowing water routes
    //    routing resources - disallowing routes without transporters

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
    */

    // adding support for option_CanTransportSerfsOnBoats
    //
    // the function argument 'bool land' and 'bool transporter are used as follows':
    // if a resource is being routed, land=false and transporter=true
    //    which means "does not have to be a land route, uses transporter(?)"
    // if a serf being is routed, land=true and transporter=false
    //    which means "must be a land route, does not need transporter(?)"...
    //  ... unless there is already a boat and option_CanTransportSerfsInBoats is set
    //Log::Info["flag"] << "debug a";
    for (Direction i : cycle_directions_ccw()) {
      //Log::Info["flag"] << "debug: inside FlagSearch::execute, checking dir: " << i;
      // sanity check my assumption that 'land' and 'transporter' are exclusive (two sides of the same coin)
      if (land == true && transporter == true){
        throw ExceptionFreeserf("faulty Flagsearch::execute logic!  both 'land' and 'transporter' bools are true but I assume this can never happen!");
      }
      
      if (!flag->has_path(i)){
        //Log::Info["flag"] << "debug: inside FlagSearch::execute, no path in dir " << i << ", skipping this dir";
        // ... skip this dir/flag
        continue;
      }

      // if a serf is being routed... ('land = true' suggests it is a serf)
      if (land == true){
        //Log::Info["flag"] << "debug: inside FlagSearch::execute, checking dir: " << i << ", 'land' == true";
        // ...and this is a water route (which requires a sailor to transport serfs)
        // WARNING - is_water_path can only be uses to check a path that is certain to exist, it will return true if there is no path at all!
        //if (flag->has_path(i) && flag->is_water_path(i)){
        // checking flag->has_path earlier now
        if (flag->is_water_path(i)){
          // debug - I am seeing serfs walking on water paths when CanTransportSerfsInBoats is *OFF* 
          // hmm... I cannot reproduce this now using a human player
          //Log::Info["flag"] << "debug SERFS WALKING ON WATER, skipping flag " << flag->get_index() << " dir " << NameDirection[i] << " is water path";
          // ...but CanTransportSerfsInBoats option is not on...
          if (!option_CanTransportSerfsInBoats){
            // ... skip this dir/flag
            // debug - I am seeing serfs walking on water paths when CanTransportSerfsInBoats is *OFF*
            // hmm... I cannot reproduce this now using a human player
            //Log::Info["flag"] << "debug SERFS WALKING ON WATER, skipping flag " << flag->get_index() << " dir " << NameDirection[i] << " because is water path but CanTransportSerfsInBoats is false";
            continue;
          }
          //Log::Info["flag"] << "debug e";
          // ...but no sailor is on the water route...
          if (!flag->has_transporter(i)){
            //Log::Info["flag"] << "debug: inside FlagSearch::execute, checking dir: " << i << ", 'land' == true, this flag has a water path, but this flag-dir has no sailor!  skipping this dir";
            // ... skip this dir/flag
            continue;
          }
        }
      }
      
      // if this is a resource being routed... ('transporter = true' suggests it is a resource)
      //  ... but there is no transporter on this flag-path-dir...
      //   NOTE it should be possible to remove the 'transporter == true' check if we are sure that land and transporter bools are exclusive
      //Log::Info["flag"] << "debug g";
      if (transporter && !flag->has_transporter(i)){
        //Log::Info["flag"] << "debug: inside FlagSearch::execute, checking dir: " << i << ", 'transporter' == true but this flag-dir has no transporter!  skipping this dir";
        // ... skip this dir/flag
        continue;
      }
      
      // if this is the same flag we just checked(?)
      //Log::Info["flag"] << "debug: inside FlagSearch::execute, checking dir: " << i << ", for flag at pos " << flag->get_position() << ", about to check flag->other_endpoint.f[" << i << "]->search_num to see if it matches id " << id;
	  // got a crash here, access violation for flag->other_endpoint.f[i]
	  // again
      if (flag->other_endpoint.f[i]->search_num == id) {
        //Log::Info["flag"] << "debug: inside FlagSearch::execute, checking dir: " << i << " flag->other_endpoint.f[" << i << "]->search_num matches id " << id << " (meaning already visited this?), skipping this dir";
        // ... skip this dir/flag
        continue;
      }
      //Log::Info["flag"] << "debug: inside FlagSearch::execute, continuing search";

      // if NONE of the above exclusions were true, continue the search
      flag->other_endpoint.f[i]->search_num = id;
      flag->other_endpoint.f[i]->search_dir = flag->search_dir;
      Flag *other_flag = flag->other_endpoint.f[i];
      queue.push_back(other_flag);
      //Log::Info["flag"] << "debug: inside FlagSearch::execute, done adding new node at end of loop";
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
  //Log::Info["flag"] << "debug: inside FlagSearch::single with src flag at pos " << src->get_position();
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
  , bld2_flags(0)
  , serf_waiting_for_boat(false) {
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

// support option_CanTransportSerfsInBoats
//  is this function even necessary?  could just reset serf_waiting_for_boat directly
bool
Flag::pick_up_serf() {
  if (!serf_waiting_for_boat){
    throw ExceptionFreeserf("No serf is waiting for boat pickup at this flag!");
  }
  serf_waiting_for_boat = false;
  // clear the flag search queue that was set to fake a resource schedule
  //  which is the mechanism used to trigger a sailor to come to the serf in WaitForBoat state
  // if this queue is not cleared the sailor will, after dropping off the passenger, come back
  //  to the original flag where the serf in WaitForBoat state was waiting... and become stuck
  // We want the sailor to return to idle (inactive) transporter on road state

  // it is more complicated than simply calling queue.clear(), 
  //  the flag's search_num and search_dir must be cleared
  //queue.clear();
  // this still isn't working, the sailor still thinks there is a resource waiting at the flag
  //
  // wait... maybe this is not a problem at all and the sailor simply NORMALLY returns to the 
  //  flag where the last resource CAME FROM before returning to idle state
  // Now I am thinking the issue is that a sailor cannot pick up a WaitForBoat passenger
  //  if he is already adjacent to flag where the passenger is waiting!
  // YES, I just confirmed in original SerfCity/Settlers1 DOS game that after a sailor drops
  //  off a resource delivery he returns to the side the the first delivery originated from
  //   and not the side he originally was idle at 
  //  STILL NEED TO VERIFY THAT HE CAN THEN PICK UP ANOTHER RES FROM "near" FLAG!  
  //   though I am sure it will work
  //
  ///search_dir = DirectionNone;
  ///search_num = 0;
  /// trying this:
  ///fix_scheduled();

//  *res = slot[from_slot].type;
//  *dest = slot[from_slot].dest;
//  slot[from_slot].type = Resource::TypeNone;
//  slot[from_slot].dir = DirectionNone;
//  fix_scheduled();   look into what this does
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
  // adding support for requested resource timeouts
  int dist_from_inv;
} ScheduleUnknownDestData;

static bool
schedule_unknown_dest_cb(Flag *flag, void *data) {
  ScheduleUnknownDestData *dest_data =
    static_cast<ScheduleUnknownDestData*>(data);
  //Log::Info["flag"] << "debug: inside schedule_unknown_dest_cb with flag index " << flag->get_index() << ", pos " << flag->get_position() << ", search_dir " << NameDirection[flag->get_search_dir()];
  if (flag->has_building()) {
    Building *building = flag->get_building();
    //Log::Info["flag"] << "debug: inside schedule_unknown_dest_cb with flag index " << flag->get_index() << ", pos " << flag->get_position() << ", search_dir " << flag->get_search_dir() << ". Flag has building of type " << NameBuilding[building->get_type()];

    int bld_prio = building->get_max_priority_for_resource(dest_data->resource);
    if (bld_prio > dest_data->max_prio) {
      //Log::Info["flag"] << "debug: inside schedule_unknown_dest_cb with flag index " << flag->get_index() << ", pos " << flag->get_position() << ", search_dir " << flag->get_search_dir() << ". acceptable prio";
      dest_data->max_prio = bld_prio;
      //dest_data->flag = flag;  // don't do this yet
      // adding support for requested resource timeouts
      // wait... UNKNOWN_dest search doesn't use flag->search_dirs because it does not care
      //  about the destination, it just keeps searching outward until it finds a compatible building(?)
      //  so the search_dir cannot be used to figure out the length of the roads
      // need to check the PREVIOUS flag to determine which dirs they connect between and check that length

      // quick hack to avoid 0 result
      bool found = false;
      for (Direction d : cycle_directions_ccw()) {
        if (dest_data->flag != nullptr && dest_data->flag->has_path(d) && dest_data->flag->get_other_end_flag(d)->get_index() == flag->get_index()) {
          // could also use flag->get_other_end_flag(d)? but this reads better
          /* the flag->length[dir] values are calculated by taking the true length of the path
          // and then bastardizing it by first feeding it into this get_road_length_value() 
          // function where it is changed to a 1-7 value (corresponding to the max number of
          // serfs that could service the road?) 
          // and then that1-7 value is bit-shifted << 4, effectively multiplying by 16
          // so the only possible flag->length[dir] values
          // are 16,32,48,64,80,96, and 112.
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
          // to try to approximate the original road length, bit-shift >>4, or divide by 16
          //  and then triple it to get pretty close the reversing the above table
          //  and set a minimum added value of 1 because it can actually be zero if flags are close?
            */
          if(dest_data->flag->get_road_length((Direction)d) == 0){
            Log::Info["flag"] << "debug: it get_road_length can be zero, using +1 for dist_from_inv addition";
            dest_data->dist_from_inv += 1;
          }else{
            dest_data->dist_from_inv += (dest_data->flag->get_road_length((Direction)d) / 16) * 3;
          }
          found = true;
          break;
        }
      }
      if (!found){
        //Log::Info["game"] << "debug: no prev_flag/dir found, using +1 for dist_from_inv addition";
        dest_data->dist_from_inv += 1;
      }
      // NOW we can update this
      dest_data->flag = flag;
    }

    if (dest_data->max_prio > 204){
      //Log::Info["flag"] << "debug: inside schedule_unknown_dest_cb with flag index " << flag->get_index() << ", pos " << flag->get_position() << ", search_dir " << flag->get_search_dir() << ". max_prio >204, returning true";
      return true;
    }

  }
  return false;
}

void
Flag::schedule_slot_to_unknown_dest(int slot_num) {
  //Log::Info["flag"] << "debug: inside Flag::schedule_slot_to_unknown_dest";
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
    // adding support for requested resource timeouts
    //  in order to figure out which dir we just searched in,
    //  as search_dir is not used for unknown_dest searches,
    //  I am checking which dir connects the previous flag in the search
    //  to the current one.  But the very first flag is null, so this breaks
    //  Trying this: set data.flag to the current flag
    //data.flag = NULL;
    // wait this doesn't work, because it TESTS to see if data.flag is nullptr below
    ///  hmm, changing below check to fix
    data.flag = this;
    data.max_prio = 0;
    data.dist_from_inv = 0;

    //Log::Info["flag"] << "inside Flag::schedule_slot_to_unknown_dest, about to start search";
    search.execute(schedule_unknown_dest_cb, false, true, &data);
    //  this should work...
    //if (data.flag != nullptr) {
    if (data.flag != this) {
      Log::Verbose["game"] << "dest for flag " << index << " res " << slot
                           << " found: flag " << data.flag->get_index();
      Building *dest_bld = data.flag->other_endpoint.b[DirectionUpLeft];
      // adding support for requested resource timeouts
      //if (!dest_bld->add_requested_resource(res, true)) {
      if (!dest_bld->add_requested_resource(res, true, data.dist_from_inv)) {
        Log::Error["flag"] << "inside Flag::schedule_slot_to_unknown_dest, add_requested_resource returned false, resource: " << NameResource[res] << ", building type " << NameBuilding[dest_bld->get_type()];
        throw ExceptionFreeserf("Failed to request resource.");
      }

      slot[slot_num].dest = dest_bld->get_flag_index();
      endpoint |= BIT(7);
      return;
    }
    //Log::Info["flag"] << "inside Flag::schedule_slot_to_unknown_dest, dest not found?";
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
  //Log::Info["flag"] << "debug: inside Flag::find_nearest_inventory_search_cb, considering flag at pos " << flag->get_position();
  if (flag->accepts_resources()) {
    //Log::Info["flag"] << "debug: inside Flag::find_nearest_inventory_search_cb FOOTRUE";
    *dest = flag;
    return true;
  }
  //Log::Info["flag"] << "debug: inside Flag::find_nearest_inventory_search_cb FOOFALSE";
  return false;
}

/* Return the flag index of the inventory nearest to flag. */
//  this search requires that transporters are already in place along the route
int
Flag::find_nearest_inventory_for_resource() {
  //Log::Info["flag"] << "debug: inside Flag::find_nearest_inventory_for_resource";
  
  Flag *dest = NULL;
  FlagSearch::single(this, find_nearest_inventory_search_cb, false, true,
                     &dest);
                     //if (dest == nullptr){
                        //Log::Info["flag"] << "debug: inside Flag::find_nearest_inventory_for_resource, returned nullptr!";
                     //}else{
                        //Log::Info["flag"] << "debug: inside Flag::find_nearest_inventory_for_resource, returned dest flag pos " << dest->get_position();        
                     //}

  if (dest != NULL) return dest->get_index();

  return -1;
}

/* Return the flag index of the inventory nearest to flag. */
//  this search does NOT require that transporters are already in place along the route
//  meant for checking which Inventory a building will send its non-directly-routable 
//   resources to (i.e. where they will pile up in storage) - for congestion planning

//
// POSSIBLE MAJOR AI BUG - if this function uses the flag->search_num or search_dir it will collide
//  with the main game flag updates unless mutex locked... probably need to have the AI implement its 
//   own copy
//
//  ... and it definitely does!
//   NEED TO MAKE THIS ITS OWN FUNCTION IN AI AND NOT USE THE flag->search_xxx vars at all!
	/*
	  flag->other_endpoint.f[i]->search_num = id;
	  flag->other_endpoint.f[i]->search_dir = flag->search_dir;
	  */
/* replacing this with updated AI::find_nearest_inventoryXXXXX() functions
int
Flag::find_nearest_inventory_for_res_producer() {
  Log::Info["flag"] << "thread #" << std::this_thread::get_id() << " debug: inside Flag::find_nearest_inventory_for_res_producer";
  Log::Info["flag"] << "debug: inside Flag::find_nearest_inventory_for_res_producer";
  
  Flag *dest = NULL;
  FlagSearch::single(this, find_nearest_inventory_search_cb, false, false,
                     &dest);
                     if (dest == nullptr){
                        Log::Info["flag"] << "debug: inside Flag::find_nearest_inventory_for_res_producer, returned nullptr!";
                     }else{
                        Log::Info["flag"] << "debug: inside Flag::find_nearest_inventory_for_res_producer, returned dest flag pos " << dest->get_position();        
                     }

  if (dest != NULL) return dest->get_index();

  return -1;
}
*/

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

static bool
flag_search_building_for_lost_generic_serf_search_cb(Flag *flag, void *data) {
  int *dest_index = static_cast<int*>(data);
  if (flag->has_building()) {
    Building *building = flag->get_building();
    if (building->is_done() && !building->is_burning() &&

      /* try allowing mines now since adding code to ignore active serf at building when Lost transporter arrives
      // disallow mines because they can deadlock when miner runs out of food and holds the pos
       (building->get_type() < Building::TypeStoneMine || building->get_type() > Building::TypeGoldMine)){
         */
        true ){
      *dest_index = building->get_flag_index();
      return true;
    }
  }

  return false;
}

// support allowing LostSerfs of generic type to enter ANY friendly building
//  so they can get off the map faster
//  EXCEPT mines, as they can deadlock when the miner runs out of food and 
//  holds the pos, disallowing serfs entry
int
Flag::find_nearest_building_for_lost_generic_serf() {
  int dest_index = -1;
  FlagSearch::single(this, flag_search_building_for_lost_generic_serf_search_cb, true, false,
                     &dest_index);
  return dest_index;
}

typedef struct ScheduleKnownDestData {
  Flag *src;
  Flag *dest;
  int slot;
  // wait this is not needed here
  //// add support for requested resource timeouts
  //int dist_from_inv;
} ScheduleKnownDestData;

static bool
schedule_known_dest_cb(Flag *flag, void *data) {
  ScheduleKnownDestData *dest_data =
    static_cast<ScheduleKnownDestData*>(data);
  //Log::Info["flag"] << "debug: inside Flag::schedule_known_dest_cb  stub function";
  return (flag->schedule_known_dest_cb_(dest_data->src,
                                        dest_data->dest,
                                        dest_data->slot) != 0);
}

bool
Flag::schedule_known_dest_cb_(Flag *src, Flag *dest, int _slot) {
  // I think this is one of the few (only?) functions where the
  //  flag->search_dir is actually used the way you would expect
  //Log::Info["flag"] << "debug: inside Flag::schedule_known_dest_cb_";
  if (this == dest) {
    //Log::Info["flag"] << "debug: inside Flag::schedule_known_dest_cb_, destination found";
    /* Destination found */
    if (this->search_dir != 6) {
      //Log::Info["flag"] << "debug: inside Flag::schedule_known_dest_cb_ 1";
      if (!src->is_scheduled(this->search_dir)) {
        //Log::Info["flag"] << "debug: inside Flag::schedule_known_dest_cb_ 2";
        /* Item is requesting to be fetched */
        src->other_end_dir[this->search_dir] =
          BIT(7) | (src->other_end_dir[this->search_dir] & 0x78) | _slot;
      } else {
        //Log::Info["flag"] << "debug: inside Flag::schedule_known_dest_cb_ 3";
        Player *player = game->get_player(this->get_owner());
        int other_dir = src->other_end_dir[this->search_dir];
        int prio_old = player->get_flag_prio(src->slot[other_dir & 7].type);
        int prio_new = player->get_flag_prio(src->slot[_slot].type);
        if (prio_new > prio_old) {
          //Log::Info["flag"] << "debug: inside Flag::schedule_known_dest_cb_ 4";
          /* This item has the highest priority now */
          src->other_end_dir[this->search_dir] =
            (src->other_end_dir[this->search_dir] & 0xf8) | _slot;
        }
        //Log::Info["flag"] << "debug: inside Flag::schedule_known_dest_cb_ 5";
        // add support for requested resource timeouts
        // wait, is this the wrong place???
        //  yes, it isn't needed at all here I think, this function is triggered
        //   by the flag variables set in Game::update_flags
        src->slot[_slot].dir = this->search_dir;
      }
    }
    //Log::Info["flag"] << "debug: inside Flag::schedule_known_dest_cb_, returning true";
    return true;
  }
  //Log::Info["flag"] << "debug: inside Flag::schedule_known_dest_cb_, returning false";
  return false;
}

void
Flag::schedule_slot_to_known_dest(int slot_, unsigned int res_waiting[4]) {
  //Log::Info["flag"] << "debug: inside Flag::schedule_slot_to_known_dest";
  FlagSearch search(game);

  search_num = search.get_id();
  search_dir = DirectionNone;
  int tr = transporters();

  int sources = 0;

  /* Directions where transporters are idle (zero slots waiting) */
  int flags = (res_waiting[0] ^ 0x3f) & transporter;

  //Log::Info["flag"] << "debug: inside Flag::schedule_slot_to_known_dest 1";
  if (flags != 0) {
    //Log::Info["flag"] << "debug: inside Flag::schedule_slot_to_known_dest 2";
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
  //Log::Info["flag"] << "debug: inside Flag::schedule_slot_to_known_dest 3";
  if (tr != 0) {
    //Log::Info["flag"] << "debug: inside Flag::schedule_slot_to_known_dest 4 tr";
    for (int j = 0; j < 3; j++) {
      flags = res_waiting[j] ^ res_waiting[j+1];
      for (Direction k : cycle_directions_ccw()) {
        if (BIT_TEST(flags, k)) {
          tr &= ~BIT(k);
          Flag *other_flag = other_endpoint.f[k];
		      //Log::Info["flag"] << "debug: inside Flag::schedule_slot_to_known_dest 4a, dir: " << k;
          if (other_flag->search_num != search.get_id()) {
            other_flag->search_dir = k;
            search.add_source(other_flag);
            sources += 1;
          }
        }
      }
    }
    //Log::Info["flag"] << "debug: inside Flag::schedule_slot_to_known_dest 5";
    if (tr != 0) {
      //Log::Info["flag"] << "debug: inside Flag::schedule_slot_to_known_dest 6";
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
  //Log::Info["flag"] << "debug: inside Flag::schedule_slot_to_known_dest 7";
  if (sources > 0) {
    //Log::Info["flag"] << "debug: inside Flag::schedule_slot_to_known_dest 8";
    ScheduleKnownDestData data;
    data.src = this;
    data.dest = game->get_flag(this->slot[slot_].dest);
    data.slot = slot_;
    // wait this is not needed here
    //// add support for requested resource timeouts
    //data.dist_from_inv = 0;
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
  //Log::Info["flag"] << "debug: inside Flag::schedule_slot_to_known_dest 9";
}

void
Flag::prioritize_pickup(Direction dir, Player *player) {
  //Log::Info["flag"] << "debug: inside Flag::prioritize_pickup";
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
        // de-prioritize any resource that is destined for castle/warehouse reserves
        //  this should exclude resources that are "activated" by being in castle/warehouse,
        //   such as gold bars (morale), sword/shield (knights), or tools (professionals)
        // priorities are 1-26 for flag_prio and inventory_prio, higher number == higher priority
        //  slot[x].dest is a Flag index, not a MapPos
        // ranges are based on the const int routable[] array in Flag::schedule_slot_to_unknown_dest
        // but instead of copying the table just check for FISH<>PLANK, STONE<>GOLDBAR
        //
        // if this resource has a known dest (flag index)...
        if (slot[i].dest != 0){
          // ... and is routable (meaning it CAN be delivered directly to a requesting building)...
          //if (routable[res_type]){
          if ((res_type >= Resource::TypeFish && res_type <= Resource::TypePlank) ||
              (res_type >= Resource::TypeStone && res_type <= Resource::TypeGoldBar)){
            Flag *dest_flag = game->get_flag(slot[i].dest);
            if (dest_flag == nullptr){
              Log::Warn["flag"] << "inside Flag::prioritize_pickup, got nullptr for flag";
            }else{
              // if dest is a castle/warehouse
              if (dest_flag->has_inventory() && dest_flag->accepts_serfs()){
                // ...this resource is going to pile up in a warehouse, do not adjust its priority
                //Log::Info["flag"] << "debug: Player" << player->get_index() << " a resource of type " << NameResource[res_type] << " is destined for castle/warehouse";
              }else{
                // ...this resource is being sent directly to a requesting building, increase its priority
                //Log::Info["flag"] << "debug: Player" << player->get_index() << " a resource of type " << NameResource[res_type] << " with dest pos " << dest_flag->get_position() << " and dest building type " << NameBuilding[dest_flag->get_building()->get_type()] << " is being directly routed, increasing priority";
                res_prio += 26;
              }
            }
          }else{
            // this resource is not routable, meaning it is "activated" at castle/warehouse
            //  increase its priority
            //Log::Info["flag"] << "debug: Player" << player->get_index() << " a resource of type " << NameResource[res_type] << " is not routable, increasing priority";
            res_prio += 26;
          }
        }
        // if this resource does NOT have a known dest, it will be assigned one when
        //  a serf picks it up from a flag, then then this check can run
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

/* Return true if a flag has any roads connected to it. */
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
    // originally this function was trying to call change_transporter_state_at_pos on ANY serf
    //  at the post that had one of the four wait states, occasionally causing non-transporters to become stuck forever
    if (!serf->get_type() == Serf::TypeTransporter)
      continue;
    if (serf->change_transporter_state_at_pos(pos, state)) {
      return serf->get_index();
    }
  }

  return -1;
}

static int
wake_transporter_at_flag(Game *game, MapPos pos) {
 //Log::Info["serf"] << "debug: inside Serf::wake_transporter_at_flag, pos " << pos;
  return change_transporter_state_at_pos(game, pos, Serf::StateWakeAtFlag);
}

static int
wake_transporter_on_path(Game *game, MapPos pos) {
 //Log::Info["serf"] << "debug: inside Serf::wake_transporter_on_path, pos " << pos;
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
      //Log::Info["flag"] << "debug: calling wake_transporter_on_path for pos " << pos;
      if (index >= 0) data->serfs[serf_count++] = index;
    }

    /* Check if there is a serf occupying this space. */
    if (map->has_serf(pos)) {
      //Log::Info["flag"] << "debug: found serf at pos " << pos;
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
          //Log::Info["flag"] << "debug: inside flag::update, about to schedule a slot";
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
  for (Direction j : cycle_directions_ccw()) {
    if (has_path(j)) {
      if (serf_requested(j)) {
        //Log::Debug["flag"] << "inside Flag::update, flag at pos " << pos << ", dir " << j << NameDirection[j] << ", serf_requested(j) is true";
        if (BIT_TEST(res_waiting[2], j)) {
          //Log::Debug["flag"] << "inside Flag::update, flag at pos " << pos << ", dir " << j << NameDirection[j] << ", BIT_TEST(res_waiting[2], j is true";
          if (waiting_count >= 7) {
            //Log::Debug["flag"] << "inside Flag::update, flag at pos " << pos << ", dir " << j << NameDirection[j] << ", waiting_count of " << waiting_count <<" is >=7";
            transporter &= BIT(j);
          }
        } else if (free_transporter_count(j) != 0) {
          //Log::Debug["flag"] << "inside Flag::update, flag at pos " << pos << ", dir " << j << NameDirection[j] << ", res_waiting[2] not true, but free_transporter_count(j) is true";
          transporter |= BIT(j);
        }
        //Log::Debug["flag"] << "inside Flag::update, flag at pos " << pos << ", dir " << j << NameDirection[j] << ", ELSE 1";
      } else if (free_transporter_count(j) == 0 ||
                 BIT_TEST(res_waiting[2], j)) {
                   //Log::Debug["flag"] << "inside Flag::update, flag at pos " << pos << ", dir " << j << NameDirection[j] << ", serf_requested(j) is false but elseif true";
        int max_tr = max_transporters[length_category(j)];
        if (free_transporter_count(j) < (unsigned int)max_tr &&
            !serf_request_fail()) {
          //Log::Debug["flag"] << "inside Flag::update, flag at pos " << pos << ", dir " << j << NameDirection[j] << ", about to call_transporter";
          bool r = call_transporter(j, is_water_path(j));
          //Log::Debug["flag"] << "inside Flag::update, flag at pos " << pos << ", dir " << j << NameDirection[j] << ", done to call_transporter, result was " << r;
          if (!r) transporter |= BIT(7);
        }
        //Log::Debug["flag"] << "inside Flag::update, flag at pos " << pos << ", dir " << j << NameDirection[j] << ", foo5";
        if (waiting_count >= 7) {
          //Log::Debug["flag"] << "inside Flag::update, flag at pos " << pos << ", dir " << j << NameDirection[j] << ", foo6";
          transporter &= BIT(j);
        }
        //Log::Debug["flag"] << "inside Flag::update, flag at pos " << pos << ", dir " << j << NameDirection[j] << ", foo7";
      } else {
        //Log::Debug["flag"] << "inside Flag::update, flag at pos " << pos << ", dir " << j << NameDirection[j] << ", serf_requested(j) is false, ELSE";
        transporter |= BIT(j);
      }
    }
  }
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
  // got a write access violation here, src_2 was nullptr
  //  I think this is an AI- specific issue related to the missing transporter work-around
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

  //Log::Debug["flag"] << "inside Flag::call_transporter, about to call serf->go_out_from_inventory to flag index " << this->get_index() << " and map pos " << this->get_position() << " which should be " <<  src->get_position() << " and dir " << dir << " / " << NameDirection[dir] << ", this serf is currently at pos " << serf->get_pos();

  // the go_out_from_inventory declaration says arg2 is a MapPos, but src->get_index() returns a Flag index not a Map Pos, wtf??
  serf->go_out_from_inventory(inventory->get_index(), src->get_index(), dir);

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
