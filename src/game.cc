/*
 * game.cc - Gameplay related functions
 *
 * Copyright (C) 2013-2017  Jon Lund Steffensen <jonlst@gmail.com>
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

/* The following are functions that concern the gameplay as
   a whole. Functions that only act on a specific game object should
   go in the respective source file. */

#include "src/game.h"

#include <string>
#include <algorithm>
#include <map>
#include <memory>
#include <sstream>
#include <thread>   //NOLINT (build/c++11) this is a Google Chromium req, not relevant to general C++.  // for AI threads

#include "src/savegame.h"
#include "src/debug.h"
#include "src/log.h"
#include "src/misc.h"
#include "src/inventory.h"
#include "src/map.h"
#include "src/map-generator.h"
#include "src/map-geometry.h"

#define GROUND_ANALYSIS_RADIUS  25

// deFINE the global game option bools that were deCLARED in game-options.h
bool option_EnableAutoSave = false;
//bool option_ImprovedPigFarms = false;  // removing this as it turns out the default behavior for pig farms is to require almost no grain
bool option_CanTransportSerfsInBoats = false;
bool option_QuickDemoEmptyBuildSites = false;
bool option_TreesReproduce = false;
bool option_BabyTreesMatureSlowly = false;
bool option_ResourceRequestsTimeOut = true;  // this is forced true to indicate that the code to make them optional isn't added yet
bool option_PrioritizeUsableResources = true;    // this is forced true to indicate that the code to make them optional isn't added yet
bool option_LostTransportersClearFaster = false;
bool option_FourSeasons = false;
bool option_FishSpawnSlowly = false;
int season = 1;  // default to Summer
int last_season = 1;
int subseason = 0;  // for tree progression
int last_subseason = 0;
typedef enum Season {
  SeasonSpring = 0,
  SeasonSummer = 1,
  SeasonFall = 2,
  SeasonWinter = 3,
} Season;
// the Custom data map_objects offset for tree sprites
int season_offset[4] = {
  200, // Spring
    0, // Summer
  400, // Fall
  300, // Winter
};


Game::Game()
  : map_gold_morale_factor(0)
  , game_speed_save(0)
  , last_tick(0)
  , field_340(0)
  , field_342(0)
  , field_344(0)
  , tutorial_level(0)
  , mission_level(0)
  , map_preserve_bugs(0)
  , player_score_leader(0) {
  players = Players(this);
  flags = Flags(this);
  inventories = Inventories(this);
  buildings = Buildings(this);
  serfs = Serfs(this);
  std::mutex mutex;

  /* Create NULL-serf */
  serfs.allocate();

  /* Create NULL-building (index 0 is undefined) */
  buildings.allocate();

  /* Create NULL-flag (index 0 is undefined) */
  flags.allocate();

  /* Initialize global lookup tables */
  game_speed = DEFAULT_GAME_SPEED;

  update_map_last_tick = 0;
  update_map_counter = 0;
  update_map_16_loop = 0;
  update_map_initial_pos = 0;
  next_index = 0;

  std::fill(std::begin(player_history_index),
            std::end(player_history_index), 0);
  std::fill(std::begin(player_history_counter),
            std::end(player_history_counter), 0);
  resource_history_index = 0;

  tick = 0;
  const_tick = 0;
  tick_diff = 0;

  max_next_index = 0;
  game_type = 0;
  flag_search_counter = 0;
  game_stats_counter = 0;
  history_counter = 0;

  knight_morale_counter = 0;
  inventory_schedule_counter = 0;

  gold_total = 0;

  ai_locked = true;
  signal_ai_exit = false;
  ai_threads_remaining = 0;
}

Game::~Game() {
  serfs.clear();
  inventories.clear();
  buildings.clear();
  flags.clear();
  players.clear();
}

/* Clear the serf request bit of all flags and buildings.
   This allows the flag or building to try and request a
   serf again. */
void
Game::clear_serf_request_failure() {
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex inside Game::clear_serf_request_failure";
  mutex.lock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " has locking mutex inside Game::clear_serf_request_failure";
  for (Building *building : buildings) {
    building->clear_serf_request_failure();
  }

  // dec10 2021 got vector iterators incompatible at for(Flag *flag : flags)
  // again same day
  // this is very likely because AI is invaliding game->Flags, added some extra mutex and
  //  copying of Flags when iterating so avoid this, see if it fixes it
  for (Flag *flag : flags) {
    flag->serf_request_clear();
  }
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is unlocking mutex inside Game::clear_serf_request_failure";
  mutex.unlock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " has unlocked mutex inside Game::clear_serf_request_failure";
}

void
Game::update_knight_morale() {
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex for Game::update_knight_morale";
  mutex.lock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " has locked mutex for Game::update_knight_morale";
  for (Player *player : players) {
    player->update_knight_morale();
  }
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is unlocking mutex for Game::update_knight_morale";
  mutex.unlock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " has unlocked mutex for Game::update_knight_morale";
}

typedef struct UpdateInventoriesData {
  Resource::Type resource;
  int *max_prio;
  Flag **flags;
  // adding support for requested resource timeouts:
  // - when requesting a resource, count the total tile distance it must travel
  //    as determined by adding up the length of the path-dirs followed in the
  //    flag search.
  // - set an "expected by" tick based on the delivery distance times some multiplier
  //    that allows for reasonable traffic and steep roads
  // - inside the Building update, add code to re-request a resource that has not 
  //    arrived by the expected-by tick
  // - if this expected_by tick is not made part of the save/load game, it should
  //    be harmless as the expected_by tick will not be set and not trigger on load
  //    but should still be set and work normally for any further reqs from the building
  // I don't understand how these are pointers when being defined in the struct
  //  but not when assigned to data.xxxx ?
  //
  // array of dists_from_inv, index matches with inv[i], assigned once
  //  an inventory is found (assigns the current running total so far)
  int *dists_from_inv;
  // current accumulated tile dist so far since the start of the entire flag search
  int dist_so_far;
  // needed to determine the path Direction between the current and previous flags
  //  so the flag length[dir] field can be checked
  Flag *prev_flag;
} UpdateInventoriesData;

bool
Game::update_inventories_cb(Flag *flag, void *d) {
  UpdateInventoriesData *data = reinterpret_cast<UpdateInventoriesData*>(d);
  int inv = flag->get_search_dir();
  //  NOTE - the search_dir doesn't actually refer to a Direction at all
  //  when used by Game::update_inventories_cb, it looks to be re-used to store
  //   the index number (in the context of THIS CURRENT SEARCH)
  //   of the Inventory (castle, warehouse/stock) that the flag is attached to 
  //   and it is only always 0/East/DirectionRight for the *castle* inventory, 
  //   but for other warehouse will be 1, 2, 100, and so on
  //Log::Info["game"] << "debug: inside update_inventories_cb, flag pos = " << flag->get_position() << ", \"flag->search_dir\" i.e. inv[#] = " << inv << ", for resource " << NameResource[data->resource];

  if (data->max_prio[inv] < 255 && flag->has_building()) {
    Building *building = flag->get_building();

    int bld_prio = building->get_max_priority_for_resource(data->resource, 16);
    if (bld_prio > data->max_prio[inv]) {
      data->max_prio[inv] = bld_prio;
      data->flags[inv] = flag;
      //Log::Info["game"] << "debug: case1 setting data->dists_so_far[" << inv << "] = " << data->dist_so_far;
      data->dists_from_inv[inv] = data->dist_so_far;
    }
  }
  // adding support for requested resource timeouts
  //
  // this needs to be outside of the if() block above because that if() is only true
  //  when the flag being checked is one of the valid inventories
  //  For all non-inventory flags, we need to determine the flag dist between this and the
  //   previous flag, and increment the current running dist_from_inv.
  //  Once and inventory is reached, the current dist_from_inv is stored in reference to
  //   that inv[i] Inventory in the dists_from_inv[] array
  //  The number is never reset, it keeps increasing until the last inv is found or the search ends
  //
  // quick hack to avoid 0 result?
  bool found = false;
  // for some reason data->prev_flag->get_other_end_flag(d)->get_index() seems to cause a crash
  //  if prev_flag is flag 0 (castle flag? or no flag?) so try skipping if that is the case
  //Log::Info["game"] << "debug: inside Game::update_inventories_cb, foo";
  //Log::Info["game"] << "debug: inside Game::update_inventories_cb, ZZ THIS flag index " << flag->get_index();
  // hmm it seems like prev_flag is somehow set to ...something resulting in super high prev_flag->get_index
  //  when I would expect it to be a nullptr.
  if (data->prev_flag != nullptr && data->prev_flag->get_index() > 0){
    for (Direction d : cycle_directions_ccw()) {
      //Log::Info["game"] << "debug: inside Game::update_inventories_cb, checking dir " << d;
      //// exception debug
      //Log::Info["game"] << "debug: inside Game::update_inventories_cb, 2";
      //Log::Info["game"] << "debug: inside Game::update_inventories_cb,Y1 prev flag index " << data->prev_flag->get_index();
      //Log::Info["game"] << "debug: inside Game::update_inventories_cb,Y2 THIS flag index " << flag->get_index();
      //if (!data->prev_flag->has_path(d)){
      //  Log::Info["game"] << "debug: inside Game::update_inventories_cb, B";
      //  continue;
      //}
      //
      //Log::Info["game"] << "debug: inside Game::update_inventories_cb, 3";
      //Log::Info["game"] << "debug: inside Game::update_inventories_cb,z1 " << data->prev_flag->get_other_end_flag(d)->get_index();
      //Log::Info["game"] << "debug: inside Game::update_inventories_cb,z2 " << flag->get_index();
      //Log::Info["game"] << "debug: inside Game::update_inventories_cb,z3";
      //if (data->prev_flag->get_other_end_flag(d)->get_index() != flag->get_index()) {
      //  Log::Info["game"] << "debug: inside Game::update_inventories_cb, C";
      //  continue;
      //}
      //Log::Info["game"] << "debug: inside Game::update_inventories_cb, 4";
      if (data->prev_flag->has_path_IMPROVED(d) && flag != nullptr 
       && data->prev_flag->get_other_end_flag(d)->get_index() == flag->get_index()) {
        // could also use flag->get_other_end_flag(d)? but this reads better
        // to try to approximate the original road length, bit-shift >>4, or divide by 16
        //  and then triple it to get pretty close the reversing the above table
        if(data->prev_flag->get_road_length((Direction)d) == 0){
          //Log::Info["game"] << "debug: inside update_inventories, it seems get_road_length can be zero, using +1 for dist_from_inv addition";
          data->dist_so_far += 1;
        }else{
          //Log::Info["game"] << "debug: inside update_inventories, data->prev_flag->get_road_length((Direction)" << d << ") = " << data->prev_flag->get_road_length((Direction)d) << "... which is then divided by 16 then multiplied by 3 to get " << (data->prev_flag->get_road_length((Direction)d) / 16) * 3;
          data->dist_so_far += (data->prev_flag->get_road_length((Direction)d) / 16) * 3;
        }
        //Log::Info["game"] << "debug: inside update_inventories, data->dist_so_far is currently " << data->dist_so_far;
        found = true;
        break;
      }
    }
  }
  if (!found){
    //Log::Info["game"] << "debug: inside update_inventories, no prev_flag/dir found, using +1 for dist_from_inv addition";
    data->dist_so_far += 1;
  }
  data->prev_flag = flag;

  // adding this to address what seems like a bug, but could cause a new one
  //Log::Info["game"] << "debug: case2 setting data->dists_so_far[" << inv << "] = " << data->dist_so_far;
  data->dists_from_inv[inv] = data->dist_so_far;

  // it seems this callback cannot return true, it doesn't have a return true condition
  //  instead, the search exits once all inventories in the search are considered(?)
  //Log::Info["game"] << "inside update_inventories_cb, flag search has completed";
  return false;
}

/* Update inventories as part of the game progression. Moves the appropriate
   resources that are needed outside of the inventory into the out queue. */
void
Game::update_inventories() {
	//Log::Debug["game"] << "debug: inside Game::update_inventories(), start of function";

  // this is the "serf transport priority" i.e. flag_prio but because it is listed 
  // statically here and not updated to match the actual "flag_prio" list
  // I am thinking that this is a way of randomly iterating through
  // the list of resources to be delivered (according to the actual flag_prio)
  // and it is varied to avoid becoming stuck on a single resource type for too long
  // ??? but wouldn't it always use the highest priority one anyway??
  //  I do not understand why this uses three seemingly random lists
  const Resource::Type arr_1[] = {
    Resource::TypePlank,
    Resource::TypeStone,
    Resource::TypeSteel,
    Resource::TypeCoal,
    Resource::TypeLumber,
    Resource::TypeIronOre,
    Resource::GroupFood,
    Resource::TypePig,
    Resource::TypeFlour,
    Resource::TypeWheat,
    Resource::TypeGoldBar,
    Resource::TypeGoldOre,
    Resource::TypeNone,
  };

  // I have no idea what this list represents, I don't see it anywhere in the game
  const Resource::Type arr_2[] = {
    Resource::TypeStone,
    Resource::TypeIronOre,
    Resource::TypeGoldOre,
    Resource::TypeCoal,
    Resource::TypeSteel,
    Resource::TypeGoldBar,
    Resource::GroupFood,
    Resource::TypePig,
    Resource::TypeFlour,
    Resource::TypeWheat,
    Resource::TypeLumber,
    Resource::TypePlank,
    Resource::TypeNone,
  };

  // I have no idea what this list represents, I don't see it anywhere in the game
  const Resource::Type arr_3[] = {
    Resource::GroupFood,
    Resource::TypeWheat,
    Resource::TypePig,
    Resource::TypeFlour,
    Resource::TypeGoldBar,
    Resource::TypeStone,
    Resource::TypePlank,
    Resource::TypeSteel,
    Resource::TypeCoal,
    Resource::TypeLumber,
    Resource::TypeGoldOre,
    Resource::TypeIronOre,
    Resource::TypeNone,
  };

  /* AI: TODO */

  // this randomly selecting one of three orderings ??? why???
  const Resource::Type *arr = NULL;
  switch (random_int() & 7) {
    case 0: arr = arr_2; break;
    case 1: arr = arr_3; break;
    default: arr = arr_1; break;
  }

  while (arr[0] != Resource::TypeNone) {
    for (Player *player : players) {
      // the ONLY VALID "Inventories" are the castle and warehouse/stocks!
      // Other buildings which may have  stock[0] and stock[1], including ones that produce
      //  new resources, do NOT count as valid inventories for this search (I think)

      // an array of inventories that could service requests for this resource
      Inventory *invs[256];
      // n is the number of inventories that could supply this type of resource
      //  that is, the last valid invs[] array index
      int n = 0;
      // this may not require mutex locking because only the game can "bless" a new Stock or truly destroy a castle/stock
      //Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex inside Game::update_inventories";
      //mutex.lock();
      //Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " has locked mutex inside Game::update_inventories";
      for (Inventory *inventory : inventories) {

        // debug
        //  it seems pretty normal for Inventory out queues to be full
        //if (inventory->get_owner() == player->get_index() && inventory->is_queue_full()) {
        //  Building *foo = get_building(inventory->get_building_index());
        //  Log::Info["game"] << "debug: OUT QUEUE FULL for player" << player->get_index() << "'s inventory at " << NameBuilding[foo->get_type()] << " at pos " << foo->get_position();
        //}
        
        // got nullptr when player's castle destroyed, add nullptr check
        if (inventory == nullptr){
          // do NOT log this, once any Inventory is destroyed its index will always be a nullptr and it will repeat the message constantly
          //Log::Warn["game.cc"] << "inside Game::update_inventories for Player" << player->get_index() << ", inventory is nullptr!  was it just destroyed?  NOTE THAT THIS INVENTORY COULD HAVE BEEN OWNED BY ANY PLAYER NOT JUST Player" << player->get_index();
          continue;
        }

        // find inventories (whose out-queue is not full) 
        //  that have this selected resource type, and add
        //  as a pointer to the inv[] array
        if (inventory->get_owner() == player->get_index() &&
            !inventory->is_queue_full()) {
          Inventory::Mode res_dir = inventory->get_res_mode();
            // if Inventory is in normal operation (NOT in evacuation mode)
            //  use the flag_prio which is the "serf carrying" priority list
            //  drawn in sett_5 popup
          if (res_dir == Inventory::ModeIn || res_dir == Inventory::ModeStop) {
            if (arr[0] == Resource::GroupFood) {
              if (inventory->has_food()) {
                invs[n++] = inventory;
                if (n == 256) break;
              }
            } else if (inventory->get_count_of(arr[0]) != 0) {
              invs[n++] = inventory;
              if (n == 256) break;
            }
          } else {
            // Inventory is in evacuation mode!
            //  use the inventory_prio which is the "evacuate building" priority list
            //  drawn in sett_6 popup
            int prio = 0;
            Resource::Type type = Resource::TypeNone;
            for (int i = 0; i < 26; i++) {
              if (inventory->get_count_of((Resource::Type)i) != 0 &&
                  player->get_inventory_prio(i) >= prio) {
                prio = player->get_inventory_prio(i);
                type = (Resource::Type)i;
              }
            }

            if (type != Resource::TypeNone) {
              inventory->add_to_queue(type, 0);
            }
          }
        }
      }

      //Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is unlocking mutex inside Game::update_inventories";
      //mutex.unlock();
      //Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " has unlocked mutex inside Game::update_inventories";

      //
      // NOTE - so far nothing in this function is paying attention to where resources are REQUESTED
      //   it is only iterating over each Resource type, checking Inventories that could supply it,
      //    (or directly adding the Resource to the queue if Inventory in evacuation mode)
      //    
      //  later it will check where the Resource might be needed at 
      //       search.execute(update_inventories_cb
      //

      // if there are no inventories that could supply this type of resource
      //  skip this resource type and move on to the next resource type
      if (n == 0) continue;

      // if there ARE inventories that could supply this type of resource
      //  start a new search
      FlagSearch search(this);

      // each array item will map to one of the invs[], which are the
      //  valid inventories that could supply this type of resource.
      // Up to 256 inventories could be considered, but usually much fewer!
      int max_prio[256];
      Flag *flags_[256];
      // adding support for requested resource timeouts
      int dists_from_inv[256];

      // set the initial values for each inventory
      //  'n' is the number of inventories found that could supply
      //  this type of resource... i.e. the highest element
      //   of array inv[]
      for (int i = 0; i < n; i++) {
        //Log::Info["game"] << "debug: inside Game::update_inventories, wtf0, i = " << i << ", n = " << n;
        max_prio[i] = 0;
        flags_[i] = NULL;  // this is populated by the search callback update_inventories_cb - it is set to the flag of the nearest building
                           //  found that desires this resource (as indicated by the building setting its 'prio' for that stock0/1)
        // adding support for requested resource timeouts
        dists_from_inv[i] = -1;
        // get the game->Flag* attached to the inventory building...
        Flag *flag = flags[invs[i]->get_flag_index()];
        // and set its search dir and add it as a source to the FlagSearch
        // NOTE - Directions only go from 0-5, but this is setting 0-256!
        //  this may explain why elsewhere I see invalid dirs, and various
        //  bitwise operators doing 'AND 255' on Direction integers
        //
        // I no longer think that search_dir actually refers to a direction at all in some cases
        //  it looks like it is simply used to store the INVENTORY INDEX FOR THIS CURRENT SEARCH
        //   rather than any valid Direction 0-5
        //
        //Log::Info["game"] << "debug: inside Game::update_inventories, wtf1, i = " << i << ", n = " << n << ", (Direction)i = " << (Direction)i;
        flag->set_search_dir((Direction)i);
        //Log::Info["game"] << "debug: inside Game::update_inventories, wtf1, i = " << i << ", n = " << n << ", flag->get_search_dir = " << flag->get_search_dir();
        search.add_source(flag);
      }

      UpdateInventoriesData data;
      data.resource = arr[0];
      data.max_prio = max_prio;
      data.flags = flags_;
      // adding support for requested resource timeouts
      data.dists_from_inv = dists_from_inv;
      data.dist_so_far = 0;
      // I guess I need to set this explicitly?  was seeing weird behavior
      data.prev_flag = nullptr;

      //
      // the update_inventories_cb populates above variables
      // it always *returns* false, but the return code is meaningless
      //
      // NOTE - the update_inventories_cb finds buildings that desire (by way of stock[x].prio value)
      //  the currently selected resource (arr[x]) AND have over 16 priority.  Because buildings
      //  request priority decreases as its stored+requested res count increases, it quickly falls
      //  under the minimum 16 and so will not have any more sent.  However, other sources
      //  of the same resource will allow the building's stocks to fill up, I am not yet
      //  sure exactly how
      // it looks like the schedule_unknown_dest_cb does not have a minimum prio
      //  and so it can fill a processing building up by directly sourcing from producers
      //
      //Log::Info["game"] << "debug: starting Game::update_inventories flagsearch";
      search.execute(update_inventories_cb, false, true, &data);

      //  'n' is the number of inventories found that could supply
      //  this type of resource... i.e. the highest element
      //   of array inv[]
      for (int i = 0; i < n; i++) {
		    //Log::Info["game"] << "debug: inside update_inventories, i == " << i << ", expecting it to be way under 256";

        // if max_prio >0 that means there is a building at this
        //  flag which can request resources
        if (max_prio[i] > 0) {
          Resource::Type res = (Resource::Type)arr[0];
          //Log::Verbose["game"] << "destination found for resource type " << NameResource[res] << from inventory " << i;
		  
          // this will have been populated by the search callback update_inventories_cb - it is set to the flag of the nearest building
          //  found that desires this resource (as indicated by the building setting its 'prio' for that stock0/1)
          Building *dest_bld = flags_[i]->get_building();  
          
          //Log::Info["flag"] << "inside Game::update_inventories, about to call add_requested_resource for dest_bld of type " << NameBuilding[dest_bld->get_type()];
          // adding support for requested resource timeouts
          //if (!dest_bld->add_requested_resource(res, false)) {
          int dist_from_inv = dists_from_inv[i];
          //Log::Info["flag"] << "inside Game::update_inventories, about to call dest_bld->add_requested_resource(" << NameResource[res] << ", false, " << dist_from_inv << ") for dest_bld of type " << NameBuilding[dest_bld->get_type()];
          if (!dest_bld->add_requested_resource(res, false, dist_from_inv)) {
            throw ExceptionFreeserf("Failed to request resource.");
          }

          /* Put resource in out queue */
          // note that the out queue only has two slots per inventory!  so it is usually full
          Inventory *src_inv = invs[i];
          src_inv->add_to_queue(res, dest_bld->get_flag_index());
        }
      }
    }
    arr += 1;  // advance to the next resource in the randomly selected one-of-three list
  }
}

/* Update flags as part of the game progression. */
void
Game::update_flags() {
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex for Game::update_flags";
  mutex.lock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " has locked mutex for Game::update_flags";
  // replacing with p1plp1 way, this works reliably
  Flags::Iterator i = flags.begin();
  Flags::Iterator prev = flags.begin();
  while (i != flags.end()) {
    prev = i;
    Flag *flag = *i;
    if (flag != NULL) {
      if (flag->get_index() != 0) {
        flag->update();
        if (flag == NULL)
          Log::Debug["game"] << "Game::update_flags, flag is NULL after update";
      }
    }
    if (flag == NULL) {
      Log::Debug["game"] << "Game::update_flags, flag is NULL so set i=prev";
      i = prev;
    }
    else {
      if (i != flags.end())
        ++i;
    }
  }
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is unlocking mutex for Game::update_flags";
  mutex.unlock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " has unlocked mutex for Game::update_flags";
}

typedef struct SendSerfToFlagData {
  Inventory *inventory;
  Building *building;
  int serf_type;
  int dest_index;
  Resource::Type res1;
  Resource::Type res2;
} SendSerfToFlagData;

bool
Game::send_serf_to_flag_search_cb(Flag *flag, void *d) {
  if (!flag->has_inventory()) {
    return false;
  }

  SendSerfToFlagData *data = reinterpret_cast<SendSerfToFlagData*>(d);
  /* Inventory reached */
  Building *building = flag->get_building();
  Inventory *inv = building->get_inventory();

  int type = data->serf_type;
  if (type < 0) {
    int knight_type = -1;
    for (int i = 4; i >= -type-1; i--) {
      if (inv->have_serf((Serf::Type)(Serf::TypeKnight0+i))) {
        knight_type = i;
        break;
      }
    }

    if (knight_type >= 0) {
      /* Knight of appropriate type was found. */
      Serf *serf =
              inv->call_out_serf((Serf::Type)(Serf::TypeKnight0+knight_type));

      data->building->knight_request_granted();

      serf->go_out_from_inventory(inv->get_index(),
                                  data->building->get_flag_index(), -1);

      return true;
    } else if (type == -1) {
      /* See if a knight can be created here. */
      if (inv->have_serf(Serf::TypeGeneric) &&
          inv->get_count_of(Resource::TypeSword) > 0 &&
          inv->get_count_of(Resource::TypeShield) > 0) {
        data->inventory = inv;
        return true;
      }
    }
  } else {
    if (inv->have_serf((Serf::Type)type)) {
      if (type != Serf::TypeGeneric || inv->free_serf_count() > 4) {
        Serf *serf = inv->call_out_serf((Serf::Type)type);

        int mode = 0;

        if (type == Serf::TypeGeneric) {
          mode = -2;
        } else if (type == Serf::TypeGeologist) {
          mode = 6;
        } else {
          Building *dest_bld =
                    flag->get_game()->flags[data->dest_index]->get_building();
          dest_bld->serf_request_granted();
          mode = -1;
        }
        serf->go_out_from_inventory(inv->get_index(), data->dest_index, mode);

        return true;
      }
    } else {
      if (data->inventory == NULL &&
          inv->have_serf(Serf::TypeGeneric) &&
          (data->res1 == -1 || inv->get_count_of(data->res1) > 0) &&
          (data->res2 == -1 || inv->get_count_of(data->res2) > 0)) {

        data->inventory = inv;
        /* player_t *player = globals->player[SERF_PLAYER(serf)]; */
        /* game.field_340 = player->cont_search_after_non_optimal_find; */
        return true;
      }
    }
  }
  if (data->serf_type == Serf::TypeGeologist){
    //Log::Info["game"] << "debug: inside Game::send_serf_to_flag_search_cb with GEOLOGIST, F false";
  }
  return false;
}

/* Dispatch serf from (nearest?) inventory to flag. */
// it seems that this function is NOT used for calling transporters to roads, 
//  if I create a new road I see a transporter sent and arrive but never see this called for it
// instead, it looks like Inventory->call_transporter is called
bool
Game::send_serf_to_flag(Flag *dest, Serf::Type type, Resource::Type res1,
                        Resource::Type res2) {
  //Log::Debug["game"] << "debug: inside Game::send_serf_to_flag, AA, dest flag is at pos " << dest->get_position();
  //Log::Info["game"] << "debug: inside Game::send_serf_to_flag, serf type " << type << ", res1 " << res1 << ", res2 " << res2;         
  Building *building = NULL;
  if (dest->has_building()) {
    building = dest->get_building();
  }
  //Log::Info["game"] << "debug: inside Game::send_serf_to_flag, A";

  /* If type is negative, building is non-NULL. */
  if ((type < 0) && (building != NULL)) {
    Player *player = players[building->get_owner()];
    type = player->get_cycling_serf_type(type);
  }

  //Log::Info["game"] << "debug: inside Game::send_serf_to_flag, B";
  //Log::Debug["game"] << "debug: inside Game::send_serf_to_flag, B, dest flag is at pos " << dest->get_position();

  SendSerfToFlagData data;
  data.inventory = NULL;
  data.building = building;
  data.serf_type = type;
  data.dest_index = dest->get_index();
  data.res1 = res1;
  data.res2 = res2;


  bool r = FlagSearch::single(dest, send_serf_to_flag_search_cb, true, false,
                              &data);

  if (!r) {
    if (type == Serf::TypeGeologist) {Log::Info["game"] << "inside send_serf_to_flag, serf type " << NameSerf[type] << ", FAILED because flagsearch failed!";}
    return false;
  } else if (data.inventory != NULL) {
    if (type == Serf::TypeGeologist) {Log::Info["game"] << "inside send_serf_to_flag, serf type " << NameSerf[type] << ", data.inventory != null";}
    Inventory *inventory = data.inventory;
    Serf *serf = inventory->call_out_serf(Serf::TypeGeneric);
    if ((type < 0) && (building != NULL)) {
      /* Knight */
      building->knight_request_granted();

      serf->set_type(Serf::TypeKnight0);
      serf->go_out_from_inventory(inventory->get_index(),
                                  building->get_flag_index(), -1);

      inventory->pop_resource(Resource::TypeSword);
      inventory->pop_resource(Resource::TypeShield);
    } else {

      serf->set_type((Serf::Type)type);

      int mode = 0;

      if (type == Serf::TypeGeologist) {
        mode = 6;
      } else {
        if (building == NULL) {
          return false;
        }
        building->serf_request_granted();
        mode = -1;
      }

      serf->go_out_from_inventory(inventory->get_index(), dest->get_index(),
                                  mode);
      if (res1 != Resource::TypeNone) inventory->pop_resource(res1);
      if (res2 != Resource::TypeNone) inventory->pop_resource(res2);
    }

    return true;
  }
  return true;
}

/* Dispatch geologist to flag. */
bool
Game::send_geologist(Flag *dest) {
  return send_serf_to_flag(dest, Serf::TypeGeologist, Resource::TypeHammer,
                           Resource::TypeNone);
}

/* Update buildings as part of the game progression. */
void
Game::update_buildings() {
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex for Game::update_buildings";
  mutex.lock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " has locked mutex for Game::update_buildings";
  Buildings blds(buildings);
  Buildings::Iterator i = blds.begin();
  while (i != blds.end()) {
    Building *building = *i;
    ++i;
    building->update(tick);
  }
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is unlocking mutex for Game::update_buildings";
  mutex.unlock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " has unlocked mutex for Game::update_buildings";
}

/* Update serfs as part of the game progression. */
void
Game::update_serfs() {
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex for Game::update_serfs";
  mutex.lock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " has locked mutex for Game::update_serfs";
  Serfs::Iterator i = serfs.begin();
  while (i != serfs.end()) {
    Serf *serf = *i;
    ++i;
    if (serf == nullptr){  // saw a crash here dec 2021, is this a reasonable response?
      continue;
    }
    if (serf->get_index() != 0) {
      serf->update();
    }
  }
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is unlocking mutex for Game::update_serfs";
  mutex.unlock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " has unlocked mutex for Game::update_serfs";
}


/* Update historical player statistics for one measure. */
void
Game::record_player_history(int max_level, int aspect,
                            const int history_index[],
                            const Values &values) {
  unsigned int total = 0;
  for (auto value : values) {
    total += value.second;
  }
  total = std::max(1u, total);

  for (int i = 0; i < max_level+1; i++) {
    int mode = (aspect << 2) | i;
    int index = history_index[i];
    for (auto value : values) {
      uint64_t val = value.second;
      players[value.first]->set_player_stat_history(mode, index,
                                           static_cast<int>((100*val)/total));
    }
  }
}

/* Calculate whether one player has enough advantage to be
   considered a clear winner regarding one aspect.
   Return -1 if there is no clear winner. */
int
Game::calculate_clear_winner(const Values &values) {
  int total = 0;
  for (auto value : values) {
    total += value.second;
  }
  total = std::max(1, total);

  for (auto value : values) {
    uint64_t val = value.second;
    if ((100*val)/total >= 75) return value.first;
  }

  return -1;
}

/* Update statistics of the game. */
void
Game::update_game_stats() {
  if (static_cast<int>(game_stats_counter) > tick_diff) {
    game_stats_counter -= tick_diff;
  } else {
    game_stats_counter += 1500 - tick_diff;

    player_score_leader = 0;

    int update_level = 0;

    /* Update first level index */
    player_history_index[0] = player_history_index[0]+1 < 112 ?
                              player_history_index[0]+1 : 0;

    player_history_counter[0] -= 1;
    if (player_history_counter[0] < 0) {
      update_level = 1;
      player_history_counter[0] = 3;

      /* Update second level index */
      player_history_index[1] = player_history_index[1]+1 < 112 ?
                                player_history_index[1]+1 : 0;

      player_history_counter[1] -= 1;
      if (player_history_counter[1] < 0) {
        update_level = 2;
        player_history_counter[1] = 4;

        /* Update third level index */
        player_history_index[2] = player_history_index[2]+1 < 112 ?
                                  player_history_index[2]+1 : 0;

        player_history_counter[2] -= 1;
        if (player_history_counter[2] < 0) {
          update_level = 3;

          player_history_counter[2] = 4;

          /* Update fourth level index */
          player_history_index[3] = player_history_index[3]+1 < 112 ?
                                    player_history_index[3]+1 : 0;
        }
      }
    }

    std::map<unsigned int, unsigned int> values;

    /* Store land area stats in history. */
    for (Player *player : players) {
      values[player->get_index()] = player->get_land_area();
    }
    record_player_history(update_level, 1, player_history_index, values);
    // ToDo (Digger): What is this? BIT(-1)?
    player_score_leader |= BIT(calculate_clear_winner(values));

    /* Store building stats in history. */
    for (Player *player : players) {
      values[player->get_index()] = player->get_building_score();
    }
    record_player_history(update_level, 2, player_history_index, values);

    /* Store military stats in history. */
    for (Player *player : players) {
      values[player->get_index()] = player->get_military_score();
    }
    record_player_history(update_level, 3, player_history_index, values);
    player_score_leader |= BIT(calculate_clear_winner(values)) << 4;

    /* Store condensed score of all aspects in history. */
    for (Player *player : players) {
      values[player->get_index()] = player->get_score();
    }
    record_player_history(update_level, 0, player_history_index, values);

    /* TODO Determine winner based on game.player_score_leader */
  }

  if (static_cast<int>(history_counter) > tick_diff) {
    history_counter -= tick_diff;
  } else {
    history_counter += 6000 - tick_diff;

    int index = resource_history_index;

    for (int res = 0; res < 26; res++) {
      for (Player *player : players) {
        player->update_stats(res);
      }
    }

    resource_history_index = index+1 < 120 ? index+1 : 0;
  }
}

/* Update game state after tick increment. */
void
Game::update() {

/*
  Log::Info["game"] << "option_EnableAutoSave is " << option_EnableAutoSave;
  Log::Info["game"] << "option_ImprovedPigFarms is " << option_ImprovedPigFarms;   // removing this as it turns out the default behavior for pig farms is to require almost no grain
  Log::Info["game"] << "option_CanTransportSerfsInBoats is " << option_CanTransportSerfsInBoats;
  Log::Info["game"] << "option_QuickDemoEmptyBuildSites is " << option_QuickDemoEmptyBuildSites;
  Log::Info["game"] << "option_TreesReproduce is " << option_TreesReproduce;
  Log::Info["game"] << "option_BabyTreesMatureSlowly is " << option_BabyTreesMatureSlowly;
  Log::Info["game"] << "option_ResourceRequestsTimeOut is " << option_ResourceRequestsTimeOut;
  //PrioritizeUsableResources
  Log::Info["game"] << "option_LostTransportersClearFaster is " << option_LostTransportersClearFaster;
  Log::Info["game"] << "option_FourSeasons is " << option_FourSeasons;
  Log::Info["game"] << "option_FishSpawnSlowly is " << option_FishSpawnSlowly;
  */

  /* Increment tick counters */
  const_tick += 1;

  /* Update tick counters based on game speed */
  last_tick = tick;
  tick += game_speed;
  tick_diff = tick - last_tick;

  //Log::Info["game"] << "current speed-adjusted tick: " << tick;

  clear_serf_request_failure();
  map->update(tick, &init_map_rnd);

  /* Update players */
  // this must be mutex locked because new serfs are born during this step, and allocating new serfs invalidates any other serf iterators
  //  player->update calls spawn_serf which calls Game::create_serf which calls serfs.allocate()
  //  the mutex lock is INSIDE player->update, go look there
  for (Player *player : players) {
    player->update();
  }

  /* Update knight morale */
  knight_morale_counter -= tick_diff;
  if (knight_morale_counter < 0) {
    update_knight_morale();
    knight_morale_counter += 256;
  }

  /* Schedule resources to go out of inventories */
  inventory_schedule_counter -= tick_diff;
  if (inventory_schedule_counter < 0) {
    update_inventories();
    inventory_schedule_counter += 64;
  }

#if 0
  /* AI related updates */
  game.next_index = (game.next_index + 1) % game.max_next_index;
  if (game.next_index > 32) {
    for (int i = 0; i < game.max_next_index) {
      int i = 33 - game.next_index;
      player_t *player = game.player[i & 3];
      if (PLAYER_IS_ACTIVE(player) && PLAYER_IS_AI(player)) {
        /* AI */
        /* TODO */
      }
      game.next_index += 1;
    }
  } else if (game.game_speed > 0 &&
       game.max_flag_index < 50) {
    player_t *player = game.player[game.next_index & 3];
    if (PLAYER_IS_ACTIVE(player) && PLAYER_IS_AI(player)) {
      /* AI */
      /* TODO */
    }
  }
#endif

  update_flags();
  update_buildings();
  update_serfs();
  update_game_stats();
}

/* Pause or unpause the game. */
void
Game::pause() {
  if (game_speed != 0) {
    game_speed_save = game_speed;
    game_speed = 0;
  } else {
    game_speed = game_speed_save;
  }

  Log::Info["game"] << "Game speed: " << game_speed;
}

void
Game::speed_increase() {
  if (game_speed < 40) {
    game_speed += 1;
    Log::Info["game"] << "Game speed: " << game_speed;
  }
}

void
Game::speed_decrease() {
  if (game_speed >= 1) {
    game_speed -= 1;
    Log::Info["game"] << "Game speed: " << game_speed;
  }
}

void
Game::speed_reset() {
  game_speed = DEFAULT_GAME_SPEED;
  Log::Info["game"] << "Game speed: " << game_speed;
}

/* Generate an estimate of the amount of resources in the ground at map pos.*/
void
Game::get_resource_estimate(MapPos pos, int weight, int estimates[5]) {
  if ((map->get_obj(pos) == Map::ObjectNone ||
       map->get_obj(pos) >= Map::ObjectTree0) &&
       map->get_res_type(pos) != Map::MineralsNone) {
    int value = weight * map->get_res_amount(pos);
    estimates[map->get_res_type(pos)] += value;
  }
}

/* Prepare a ground analysis at position. */
void
Game::prepare_ground_analysis(MapPos pos, int estimates[5]) {
  for (int i = 0; i < 5; i++) estimates[i] = 0;

  /* Sample the cursor position with maximum weighting. */
  get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS, estimates);

  /* Move outward in a spiral around the initial pos.
     The weighting of the samples attenuates linearly
     with the distance to the center. */
  for (int i = 0; i < GROUND_ANALYSIS_RADIUS-1; i++) {
    pos = map->move_right(pos);
    // debug - show area considered on debug overlay 
    set_debug_mark_pos(pos, "cyan");
    for (Direction d : cycle_directions_cw(DirectionDown)) {
      for (int j = 0; j < i+1; j++) {
        get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, estimates);
        pos = map->move(pos, d);
        set_debug_mark_pos(pos, "cyan");
      }
    }
  }

  /* Process the samples. */
  for (int i = 0; i < 5; i++) {
    estimates[i] >>= 4;
    estimates[i] = std::min(estimates[i], 999);
  }
}

bool
Game::road_segment_in_water(MapPos pos, Direction dir) const {
  if (dir > DirectionDown) {
    pos = map->move(pos, dir);
    dir = reverse_direction(dir);
  }

  bool water = false;

  switch (dir) {
    case DirectionRight:
      if (map->type_down(pos) <= Map::TerrainWater3 &&
          map->type_up(map->move_up(pos)) <= Map::TerrainWater3) {
        water = true;
      }
      break;
    case DirectionDownRight:
      if (map->type_up(pos) <= Map::TerrainWater3 &&
          map->type_down(pos) <= Map::TerrainWater3) {
        water = true;
      }
      break;
    case DirectionDown:
      if (map->type_up(pos) <= Map::TerrainWater3 &&
          map->type_down(map->move_left(pos)) <= Map::TerrainWater3) {
        water = true;
      }
      break;
    default:
      NOT_REACHED();
      break;
  }

  return water;
}

/* Test whether a given road can be constructed by player. The final
   destination will be returned in dest, and water will be set if the
   resulting path is a water path.
   This will return success even if the destination does _not_ contain
   a flag, and therefore partial paths can be validated with this function. */
int
Game::can_build_road(const Road &road, const Player *player, MapPos *dest,
                     bool *water) const {
  /* Follow along path to other flag. Test along the way
     whether the path is on ground or in water. */
  MapPos pos = road.get_source();
  int test = 0;

  if (!map->has_owner(pos) || map->get_owner(pos) != player->get_index() ||
      !map->has_flag(pos)) {
    return 0;
  }

  Road::Dirs dirs = road.get_dirs();
  Road::Dirs::const_iterator it = dirs.begin();
  for (; it != dirs.end(); ++it) {
    if (!map->is_road_segment_valid(pos, *it)) {
      return -1;
    }

    if (map->road_segment_in_water(pos, *it)) {
      test |= BIT(1);
    } else {
      test |= BIT(0);
    }

    pos = map->move(pos, *it);

    /* Check that owner is correct, and that only the destination has a flag. */
    if (!map->has_owner(pos) || map->get_owner(pos) != player->get_index() ||
        (map->has_flag(pos) && it != --dirs.end())) {
      return 0;
    }
  }

  MapPos d = pos;
  if (dest != NULL) *dest = d;

  /* Bit 0 indicates a ground path, bit 1 indicates
     water path. Abort if path went through both
     ground and water. */
  bool w = false;
  if (BIT_TEST(test, 1)) {
    w = true;
    if (BIT_TEST(test, 0)) return 0;
  }
  if (water != NULL) *water = w;

  return 1;
}

/* Construct a road spefified by a source and a list of directions. */
bool
Game::build_road(const Road &road, const Player *player) {
  if (road.get_length() == 0) return false;

  MapPos dest = 0;
  bool water_path = false;
  if (!can_build_road(road, player, &dest, &water_path)) {
    Log::Warn["game"] << "inside build_road, failed because can_build_road returned false!";
    return false;
  }
  if (!map->has_flag(dest)){
    Log::Warn["game"] << "inside build_road, failed because dest " << dest << " found by can_build_road does not have a flag!";
    return false;
  }


  Road::Dirs dirs = road.get_dirs();
  Direction out_dir = dirs.front();
  Direction in_dir = reverse_direction(dirs.back());

  /* Actually place road segments */
  if (!map->place_road_segments(road)){
    Log::Warn["game"] << "inside build_road, failed because place_road_segments failed!";
    return false;
  }

  /* Connect flags */
  Flag *src_flag = get_flag_at_pos(road.get_source());
  Flag *dest_flag = get_flag_at_pos(dest);

  src_flag->link_with_flag(dest_flag, water_path, road.get_length(),
                           in_dir, out_dir);

  return true;
}

void
Game::flag_reset_transport(Flag *flag) {
  /* Clear destination for any serf with resources for this flag. */
  for (Serf *serf : serfs) {
    serf->reset_transport(flag);
  }

  /* Flag. */
  for (Flag *other_flag : flags) {
    flag->reset_transport(other_flag);
  }

  /* Inventories. */
  for (Inventory *inventory : inventories) {
    if (inventory == nullptr){
      Log::Warn["game.cc"] << "inside Game::flag_reset_transport, inventory is nullptr!  was this inventory just destroyed?  skipping it";
      continue;
    }
    inventory->reset_queue_for_dest(flag);
  }
}

void
Game::building_remove_player_refs(Building *building) {
  for (Player *player : players) {
    if (player->temp_index == building->get_index()) {
      player->temp_index = 0;
    }
  }
}

bool
Game::path_serf_idle_to_wait_state(MapPos pos) {
  /* Look through serf array for the corresponding serf. */
  for (Serf *serf : serfs) {
    if (serf->idle_to_wait_state(pos)) {
      return true;
    }
  }

  return false;
}

void
Game::remove_road_forwards(MapPos pos, Direction dir) {
  Direction in_dir = DirectionNone;

  while (true) {
    if (map->get_idle_serf(pos)) {
      path_serf_idle_to_wait_state(pos);
    }

    if (map->has_serf(pos)) {
      Serf *serf = get_serf_at_pos(pos);
      if (!map->has_flag(pos)) {
        serf->set_lost_state();
        Log::Debug["game"] << "about to call set_lost_state on serf at pos " << pos << " case1, map says no flag here";;
      } else {
        /* Handle serf close to flag, where
           it should only be lost if walking
           in the wrong direction. */
        int d = serf->get_walking_dir();
        if (d < 0) d += 6;
        if (d == reverse_direction(dir)) {
          Log::Debug["game"] << "about to call set_lost_state on serf at pos " << pos << " case2, maps says flag here but note says serf is walking in the 'wrong direction'";
          Log::Error["game"] << "ATTEMPTING TO WORK AROUND BUG BY NOT SETTING THIS SERF TO LOST, instead doing... nothing";
          // dec10 2021 saw this trigger once and it seems to work with no obvious side effects, it triggered for flag adjacent to castle flag
          //serf->set_lost_state();
        }
      }
    }

    if (map->has_flag(pos)) {
      Flag *flag = flags[map->get_obj_index(pos)];
      flag->del_path(reverse_direction(in_dir));
      break;
    }

    in_dir = dir;
    dir = map->remove_road_segment(&pos, dir);
  }
}

bool
Game::demolish_road_(MapPos pos) {
  /* TODO necessary?
  game.player[0]->flags |= BIT(4);
  game.player[1]->flags |= BIT(4);
  */

  if (!map->remove_road_backrefs(pos)) {
    /* TODO */
    return false;
  }

  /* Find directions of path segments to be split. */
  Direction path_1_dir = DirectionNone;
  for (Direction d : cycle_directions_cw()) {
    if (map->has_path(pos, d)) {
      path_1_dir = d;
      break;
    }
  }

  Direction path_2_dir = DirectionNone;
  for (int d = path_1_dir+1; d <= DirectionUp; d++) {
    if (map->has_path(pos, (Direction)d)) {
      path_2_dir = (Direction)d;
      break;
    }
  }

  /* If last segment direction is UP LEFT it could
     be to a building and the real path is at UP. */
  if (path_2_dir == DirectionUpLeft && map->has_path(pos, DirectionUp)) {
    path_2_dir = DirectionUp;
  }

  remove_road_forwards(pos, path_1_dir);
  remove_road_forwards(pos, path_2_dir);

  return true;
}

/* Demolish road at position. */
bool
Game::demolish_road(MapPos pos, Player *player) {
  if (!can_demolish_road(pos, player)) return false;

  return demolish_road_(pos);
}

/* Build flag on existing path. Path must be split in two segments. */
void
Game::build_flag_split_path(MapPos pos) {
  /* Find directions of path segments to be split. */
  // the first path/flag is determined simply by the order of cycled_directions_cw
  //  around the newly created flag, starting with Right/0/East
  Direction path_1_dir = DirectionNone;
  const auto cycle = cycle_directions_cw();
  auto it = cycle.begin();
  for (; it != cycle.end(); ++it) {
    if (map->has_path(pos, *it)) {
      path_1_dir = *it;
      break;
    }
  }
  //Log::Debug["game"] << "inside build_split_flag_path at pos " << pos << ", found path_1 dir of " << path_1_dir << " / " << NameDirection[path_1_dir];
  // the second path/flag is the next one found
  //   (unless it is UpLeft/4/NorthWest and turns out to be a building - see next check below)
  // because it reuses the same cycle iterator from theprevious cycle_directions_cw,
  //  it should continue where the first one left off
  Direction path_2_dir = DirectionNone;
  ++it;
  for (; it != cycle.end(); ++it) {
    if (map->has_path(pos, *it)) {
      path_2_dir = *it;
      break;
    }
  }
  //Log::Debug["game"] << "inside build_split_flag_path at pos " << pos << ", found path_2 dir of " << path_2_dir << " / " << NameDirection[path_2_dir];

  /* If last segment direction is UP LEFT it could
     be to a building and the real path is at UP. */
  if (path_2_dir == DirectionUpLeft && map->has_path(pos, DirectionUp)) {
    //Log::Debug["game"] << "inside build_split_flag_path at pos " << pos << ", special condition, path_2_dir is UpLeft/4/NorthWest but there is also a path Up/5/NorthEast which means the UpLeft/4/NorthWest path should be a building, skiping it and assigning Up/5/NorthEast";
    path_2_dir = DirectionUp;
  }

  SerfPathInfo path_1_data;
  SerfPathInfo path_2_data;

  //Log::Debug["game"] << "inside build_flag_split_path, about to call fill_path_serf_info for new splitting flag at pos " << pos;
  Flag::fill_path_serf_info(this, pos, path_1_dir, &path_1_data);
  Flag::fill_path_serf_info(this, pos, path_2_dir, &path_2_data);
  

  Flag *flag_2 = flags[path_2_data.flag_index];
  Direction dir_2 = path_2_data.flag_dir;

  // if flag_2 (how does it decide which one becomes flag_2?) has
  //  an outstanding serf_request (transporter?), find the serf that
  //  appears to be servicing the request (what about long roads that request multiple serfs? 
  //    do they request more than one at a time??))
  //  to identify the serf destined for the flag_2, look for a serf whose path data indicates 
  //   a destination of flag 2 (and other checks done inside path_splitted function)
  //  the "select" 0/1 appears to indicate which flag's path data to copy from
  //   ... that is, which of the two ends is the correct one
  int select = -1;
  if (flag_2->serf_requested(dir_2)) {
    for (Serf *serf : serfs) {
      if (serf->path_splited(path_1_data.flag_index, path_1_data.flag_dir,
                             path_2_data.flag_index, path_2_data.flag_dir,
                             &select)) {
        //Log::Debug["game"] << "inside build_flag_split_path, path_splited checked returned true for serf " << serf->get_index() << ", select is " << select;
        break;
      }
    }

    //Log::Debug["game"] << "inside build_flag_split_path, getting read to copy SerfPathInfo, select is " << select;
    SerfPathInfo *path_data;
    if (select == 0) {
      //Log::Debug["game"] << "inside build_flag_split_path, getting read to copy SerfPathInfo, assiging path_2_data";
      path_data = &path_2_data;
    }else{
      //Log::Debug["game"] << "inside build_flag_split_path, getting read to copy SerfPathInfo, assiging path_1_data";
      path_data = &path_1_data;
    }

    Flag *selected_flag = flags[path_data->flag_index];
    //Log::Debug["game"] << "inside build_flag_split_path, about to call cancel_serf_request on flag index " << path_data->flag_index << " in dir " << path_data->flag_dir;
    selected_flag->cancel_serf_request(path_data->flag_dir);
  }

  Flag *flag = flags[map->get_obj_index(pos)];

  flag->restore_path_serf_info(path_1_dir, &path_1_data);
  flag->restore_path_serf_info(path_2_dir, &path_2_data);
}

/* Check whether player can build flag at pos. */
bool
Game::can_build_flag(MapPos pos, const Player *player) const {
  /* Check owner of land */
  if (!map->has_owner(pos) || map->get_owner(pos) != player->get_index()) {
    return false;
  }

  /* Check that land is clear */
  if (Map::map_space_from_obj[map->get_obj(pos)] != Map::SpaceOpen) {
    return false;
  }

  /* Check whether cursor is in water */
  if (map->type_up(pos) <= Map::TerrainWater3 &&
      map->type_down(pos) <= Map::TerrainWater3 &&
      map->type_down(map->move_left(pos)) <= Map::TerrainWater3 &&
      map->type_up(map->move_up_left(pos)) <= Map::TerrainWater3 &&
      map->type_down(map->move_up_left(pos)) <= Map::TerrainWater3 &&
      map->type_up(map->move_up(pos)) <= Map::TerrainWater3) {
    return false;
  }

  /* Check that no flags are nearby */
  for (Direction d : cycle_directions_cw()) {
    if (map->get_obj(map->move(pos, d)) == Map::ObjectFlag) {
      return false;
    }
  }

  return true;
}

/* Build flag at pos. */
bool
Game::build_flag(MapPos pos, Player *player) {
  if (!can_build_flag(pos, player)) {
    return false;
  }

  Flag *flag = flags.allocate();
  if (flag == NULL) return false;

  flag->set_owner(player->get_index());
  flag->set_position(pos);
  map->set_object(pos, Map::ObjectFlag, flag->get_index());

  if (map->paths(pos) != 0) {
    build_flag_split_path(pos);
  }

  return true;
}

/* Check whether military buildings are allowed at pos. */
bool
Game::can_build_military(MapPos pos) const {
  /* Check that no military buildings are nearby */
  for (int i = 0; i < 1+6+12; i++) {
    MapPos p = map->pos_add_spirally(pos, i);
    if (map->get_obj(p) >= Map::ObjectSmallBuilding &&
        map->get_obj(p) <= Map::ObjectCastle) {
      const Building *bld = buildings[map->get_obj_index(p)];
      if (bld->is_military()) {
        return false;
      }
    }
  }

  return true;
}

/* Return the height that is needed before a large building can be built.
   Returns negative if the needed height cannot be reached. */
int
Game::get_leveling_height(MapPos pos) const {
  /* Find min and max height */
  int h_min = 31;
  int h_max = 0;
  for (int i = 0; i < 12; i++) {
    MapPos p = map->pos_add_spirally(pos, 7+i);
    int h = map->get_height(p);
    if (h_min > h) h_min = h;
    if (h_max < h) h_max = h;
  }

  /* Adjust for height of adjacent unleveled buildings */
  for (int i = 0; i < 18; i++) {
    MapPos p = map->pos_add_spirally(pos, 19+i);
    if (map->get_obj(p) == Map::ObjectLargeBuilding) {
      const Building *bld = buildings[map->get_obj_index(p)];
      if (bld->is_leveling()) { /* Leveling in progress */
        int h = bld->get_level();
        if (h_min > h) h_min = h;
        if (h_max < h) h_max = h;
      }
    }
  }

  /* Return if height difference is too big */
  if (h_max - h_min >= 9) return -1;

  /* Calculate "mean" height. Height of center is added twice. */
  int h_mean = map->get_height(pos);
  for (int i = 0; i < 7; i++) {
    MapPos p = map->pos_add_spirally(pos, i);
    h_mean += map->get_height(p);
  }
  h_mean >>= 3;

  /* Calcualte height after leveling */
  int h_new_min = std::max((h_max > 4) ? (h_max - 4) : 1, 1);
  int h_new_max = h_min + 4;
  int h_new = clamp(h_new_min, h_mean, h_new_max);

  return h_new;
}

bool
Game::map_types_within(MapPos pos, Map::Terrain low, Map::Terrain high) const {
  if ((map->type_up(pos) >= low &&
       map->type_up(pos) <= high) &&
      (map->type_down(pos) >= low &&
       map->type_down(pos) <= high) &&
      (map->type_down(map->move_left(pos)) >= low &&
       map->type_down(map->move_left(pos)) <= high) &&
      (map->type_up(map->move_up_left(pos)) >= low &&
       map->type_up(map->move_up_left(pos)) <= high) &&
      (map->type_down(map->move_up_left(pos)) >= low &&
       map->type_down(map->move_up_left(pos)) <= high) &&
      (map->type_up(map->move_up(pos)) >= low &&
       map->type_up(map->move_up(pos)) <= high)) {
    return true;
  }

  return false;
}

/* Checks whether a small building is possible at position.*/
bool
Game::can_build_small(MapPos pos) const {
  return map_types_within(pos, Map::TerrainGrass0, Map::TerrainGrass3);
}

/* Checks whether a mine is possible at position. */
bool
Game::can_build_mine(MapPos pos) const {
  bool can_build = false;

  Map::Terrain types[] = {
    map->type_down(pos),
    map->type_up(pos),
    map->type_down(map->move_left(pos)),
    map->type_up(map->move_up_left(pos)),
    map->type_down(map->move_up_left(pos)),
    map->type_up(map->move_up(pos))
  };

  for (int i = 0; i < 6; i++) {
    if (types[i] >= Map::TerrainTundra0 && types[i] <= Map::TerrainSnow0) {
      can_build = true;
    } else if (!(types[i] >= Map::TerrainGrass0 &&
                 types[i] <= Map::TerrainGrass3)) {
      return false;
    }
  }

  return can_build;
}

/* Checks whether a large building is possible at position. */
bool
Game::can_build_large(MapPos pos) const {
  /* Check that surroundings are passable by serfs. */
  for (int i = 0; i < 6; i++) {
    MapPos p = map->pos_add_spirally(pos, 1+i);
    Map::Space s = Map::map_space_from_obj[map->get_obj(p)];
    if (s >= Map::SpaceSemipassable) return false;
  }

  /* Check that buildings in the second shell aren't large or castle. */
  for (int i = 0; i < 12; i++) {
    MapPos p = map->pos_add_spirally(pos, 7+i);
    if (map->get_obj(p) >= Map::ObjectLargeBuilding &&
        map->get_obj(p) <= Map::ObjectCastle) {
      return false;
    }
  }

  /* Check if center hexagon is not type grass. */
  if (map->type_up(pos) != Map::TerrainGrass1 ||
      map->type_down(pos) != Map::TerrainGrass1 ||
      map->type_down(map->move_left(pos)) != Map::TerrainGrass1 ||
      map->type_up(map->move_up_left(pos)) != Map::TerrainGrass1 ||
      map->type_down(map->move_up_left(pos)) != Map::TerrainGrass1 ||
      map->type_up(map->move_up(pos)) != Map::TerrainGrass1) {
    return false;
  }

  /* Check that leveling is possible */
  int r = get_leveling_height(pos);
  if (r < 0) return false;

  return true;
}

/* Checks whether a castle can be built by player at position. */
bool
Game::can_build_castle(MapPos pos, const Player *player) const {
  if (player->has_castle()) return false;

  /* Check owner of land around position */
  for (int i = 0; i < 7; i++) {
    MapPos p = map->pos_add_spirally(pos, i);
    if (map->has_owner(p)) return false;
  }

  /* Check that land is clear at position */
  if (Map::map_space_from_obj[map->get_obj(pos)] != Map::SpaceOpen ||
      map->paths(pos) != 0) {
    return false;
  }

  MapPos flag_pos = map->move_down_right(pos);

  /* Check that land is clear at position */
  if (Map::map_space_from_obj[map->get_obj(flag_pos)] != Map::SpaceOpen ||
      map->paths(flag_pos) != 0) {
    return false;
  }

  if (!can_build_large(pos)) return false;

  return true;
}

/* Check whether player is allowed to build anything
   at position. To determine if the initial castle can
   be built use can_build_castle() instead.

   TODO Existing buildings at position should be
   disregarded so this can be used to determine what
   can be built after the existing building has been
   demolished. */
bool
Game::can_player_build(MapPos pos, const Player *player) const {
  if (!player->has_castle()) return false;

  /* Check owner of land around position */
  for (int i = 0; i < 7; i++) {
    MapPos p = map->pos_add_spirally(pos, i);
    if (!map->has_owner(p) || map->get_owner(p) != player->get_index()) {
      return false;
    }
  }

  /* Check whether cursor is in water */
  if (map->type_up(pos) <= Map::TerrainWater3 &&
      map->type_down(pos) <= Map::TerrainWater3 &&
      map->type_down(map->move_left(pos)) <= Map::TerrainWater3 &&
      map->type_up(map->move_up_left(pos)) <= Map::TerrainWater3 &&
      map->type_down(map->move_up_left(pos)) <= Map::TerrainWater3 &&
      map->type_up(map->move_up(pos)) <= Map::TerrainWater3) {
    return false;
  }

  /* Check that no paths are blocking. */
  if (map->paths(pos) != 0) return false;

  return true;
}

// checks whether a farmer can sow a new what farm field here
// note that there is no Player or land ownership check, any 
// player's farmer can so in any valid pos even outside own
// borders or within enemy borders
bool
Game::can_build_field(MapPos pos) const {
  if (map->get_obj(pos) == Map::ObjectNone &&
      map->type_up(pos) == Map::TerrainGrass1 &&
      map->type_down(pos) == Map::TerrainGrass1 &&
      map->paths(pos) == 0 &&
      map->get_obj(map->move_right(pos)) != Map::ObjectLargeBuilding &&
      map->get_obj(map->move_right(pos)) != Map::ObjectCastle &&
      map->get_obj(map->move_down_right(pos)) != Map::ObjectLargeBuilding &&
      map->get_obj(map->move_down_right(pos)) != Map::ObjectCastle &&
      map->get_obj(map->move_down(pos)) != Map::ObjectLargeBuilding &&
      map->get_obj(map->move_down(pos)) != Map::ObjectCastle &&
      map->type_down(map->move_left(pos)) == Map::TerrainGrass1 &&
      map->get_obj(map->move_left(pos)) != Map::ObjectLargeBuilding &&
      map->get_obj(map->move_left(pos)) != Map::ObjectCastle &&
      map->type_up(map->move_up_left(pos)) == Map::TerrainGrass1 &&
      map->type_down(map->move_up_left(pos)) == Map::TerrainGrass1 &&
      map->get_obj(map->move_up_left(pos)) != Map::ObjectLargeBuilding &&
      map->get_obj(map->move_up_left(pos)) != Map::ObjectCastle &&
      map->type_up(map->move_up(pos)) == Map::TerrainGrass1 &&
      map->get_obj(map->move_up(pos)) != Map::ObjectLargeBuilding &&
      map->get_obj(map->move_up(pos)) != Map::ObjectCastle) {
    //Log::Debug["game"] << "inside can_build_field, this pos " << pos << " is suitable for a new field";
    return true;
  }
  return false;
}

/* Checks whether a building of the specified type is possible at
   position. */
bool
Game::can_build_building(MapPos pos, Building::Type type,
                         const Player *player) const {
  if (!can_player_build(pos, player)) return false;

  /* Check that space is clear */
  if (Map::map_space_from_obj[map->get_obj(pos)] != Map::SpaceOpen) {
    return false;
  }

  /* Check that building flag is possible if it
     doesn't already exist. */
  MapPos flag_pos = map->move_down_right(pos);
  if (!map->has_flag(flag_pos) && !can_build_flag(flag_pos, player)) {
    return false;
  }

  /* Check if building size is possible. */
  switch (type) {
    case Building::TypeFisher:
    case Building::TypeLumberjack:
    case Building::TypeBoatbuilder:
    case Building::TypeStonecutter:
    case Building::TypeForester:
    case Building::TypeHut:
    case Building::TypeMill:
      if (!can_build_small(pos)) return false;
      break;
    case Building::TypeStoneMine:
    case Building::TypeCoalMine:
    case Building::TypeIronMine:
    case Building::TypeGoldMine:
      if (!can_build_mine(pos)) return false;
      break;
    case Building::TypeStock:
    case Building::TypeFarm:
    case Building::TypeButcher:
    case Building::TypePigFarm:
    case Building::TypeBaker:
    case Building::TypeSawmill:
    case Building::TypeSteelSmelter:
    case Building::TypeToolMaker:
    case Building::TypeWeaponSmith:
    case Building::TypeTower:
    case Building::TypeFortress:
    case Building::TypeGoldSmelter:
      if (!can_build_large(pos)) return false;
      break;
    default:
      NOT_REACHED();
      break;
  }

  /* Check if military building is possible */
  if ((type == Building::TypeHut ||
       type == Building::TypeTower ||
       type == Building::TypeFortress) &&
      !can_build_military(pos)) {
    return false;
  }

  return true;
}

/* Build building at position. */
bool
Game::build_building(MapPos pos, Building::Type type, Player *player) {
  if (!can_build_building(pos, type, player)) {
    return false;
  }

  if (type == Building::TypeStock) {
    /* TODO Check that more stocks are allowed to be built */
  }

  Building *bld = buildings.allocate();
  if (bld == NULL) {
    return false;
  }

  Flag *flag = get_flag_at_pos(map->move_down_right(pos));
  if (flag == NULL) {
    if (!build_flag(map->move_down_right(pos), player)) {
      buildings.erase(bld->get_index());
      return false;
    }
    flag = get_flag_at_pos(map->move_down_right(pos));
  }
  unsigned int flg_index = flag->get_index();

  bld->set_level(get_leveling_height(pos));
  bld->set_position(pos);
  Map::Object map_obj = bld->start_building(type);
  player->building_founded(bld);

  bool split_path = false;
  if (map->get_obj(map->move_down_right(pos)) != Map::ObjectFlag) {
    flag->set_owner(player->get_index());
    split_path = (map->paths(map->move_down_right(pos)) != 0);
  } else {
    flg_index = map->get_obj_index(map->move_down_right(pos));
    flag = flags[flg_index];
  }

  flag->set_position(map->move_down_right(pos));

  bld->link_flag(flg_index);
  flag->link_building(bld);

  flag->clear_flags();

  map->clear_idle_serf(pos);

  map->set_object(pos, map_obj, bld->get_index());
  map->add_path(pos, DirectionDownRight);

  if (map->get_obj(map->move_down_right(pos)) != Map::ObjectFlag) {
    map->set_object(map->move_down_right(pos), Map::ObjectFlag, flg_index);
    map->add_path(map->move_down_right(pos), DirectionUpLeft);
  }

  if (split_path) build_flag_split_path(map->move_down_right(pos));

  return true;
}

/* Build castle at position. */
bool
Game::build_castle(MapPos pos, Player *player) {
  if (!can_build_castle(pos, player)) {
    return false;
  }

  Inventory *inventory = inventories.allocate();
  if (inventory == NULL) {
    return false;
  }

  Building *castle = buildings.allocate();
  if (castle == NULL) {
    inventories.erase(inventory->get_index());
    return false;
  }

  Flag *flag = flags.allocate();
  if (flag == NULL) {
    buildings.erase(castle->get_index());
    inventories.erase(inventory->get_index());
    return false;
  }

  castle->set_inventory(inventory);

  inventory->set_building_index(castle->get_index());
  inventory->set_flag_index(flag->get_index());
  inventory->set_owner(player->get_index());
  inventory->apply_supplies_preset(player->get_initial_supplies());

  add_gold_total(static_cast<int>(
    inventory->get_count_of(Resource::TypeGoldBar)));
  add_gold_total(static_cast<int>(
    inventory->get_count_of(Resource::TypeGoldOre)));

  castle->set_position(pos);
  flag->set_position(map->move_down_right(pos));
  castle->set_owner(player->get_index());
  castle->start_building(Building::TypeCastle);

  flag->set_owner(player->get_index());
  flag->set_accepts_serfs(true);
  flag->set_has_inventory();
  flag->set_accepts_resources(true);
  castle->link_flag(flag->get_index());
  flag->link_building(castle);

  map->set_object(pos, Map::ObjectCastle, castle->get_index());
  map->add_path(pos, DirectionDownRight);

  map->set_object(map->move_down_right(pos), Map::ObjectFlag,
                  flag->get_index());
  map->add_path(map->move_down_right(pos), DirectionUpLeft);

  /* Level land in hexagon below castle */
  int h = get_leveling_height(pos);
  // tlongstretch - hack to work around crash on castle placement issue: https://github.com/tlongstretch/freeserf-with-AI-plus/issues/38
  //map->set_height(pos, h);
  map->set_height_no_refresh(pos, h);
  for (Direction d : cycle_directions_cw()) {
    //map->set_height(map->move(pos, d), h);
    map->set_height_no_refresh(map->move(pos, d), h);
  }

  update_land_ownership(pos);

  player->building_founded(castle);

  castle->update_military_flag_state();

  return true;
}

void
Game::flag_remove_player_refs(Flag *flag) {
  for (Player *player : players) {
    if (player->temp_index == flag->get_index()) {
      player->temp_index = 0;
    }
  }
}

/* Check whether road can be demolished. */
bool
Game::can_demolish_road(MapPos pos, const Player *player) const {
  if (!map->has_owner(pos) || map->get_owner(pos) != player->get_index()) {
    return false;
  }

  if (map->paths(pos) == 0 ||
      map->has_flag(pos) ||
      map->has_building(pos)) {
    return false;
  }

  return true;
}

/* Check whether flag can be demolished. */
bool
Game::can_demolish_flag(MapPos pos, const Player *player) const {
  if (map->get_obj(pos) != Map::ObjectFlag) return false;

  const Flag *flag = flags[map->get_obj_index(pos)];

  if (flag->has_building()) {
    return false;
  }

  if (map->paths(pos) == 0) return true;

  if (flag->get_owner() != player->get_index()) return false;

  return flag->can_demolish();
}

bool
Game::demolish_flag_(MapPos pos) {

  /* Handle any serf at pos. */
  if (map->has_serf(pos)) {
    Serf *serf = get_serf_at_pos(pos);
    serf->flag_deleted(pos);
  }

  Flag *flag = flags[map->get_obj_index(pos)];
  if (flag->has_building()) {
    throw ExceptionFreeserf("Failed to demolish flag with building.");
  }

  flag_remove_player_refs(flag);

  /* Handle connected flag. */
  flag->merge_paths(pos);

  /* Update serfs with reference to this flag. */
  for (Serf *serf : serfs) {
    serf->path_merged(flag);
  }

  map->set_object(pos, Map::ObjectNone, 0);

  /* Remove resources from flag. */
  flag->remove_all_resources();

  flags.erase(flag->get_index());

  return true;
}

/* Demolish flag at pos. */
bool
Game::demolish_flag(MapPos pos, Player *player) {
  if (!can_demolish_flag(pos, player)) return false;

  return demolish_flag_(pos);
}

bool
Game::demolish_building_(MapPos pos) {
  Building *building = buildings[map->get_obj_index(pos)];
  if (building->burnup()) {
    building_remove_player_refs(building);

    /* Remove path to building. */
    map->del_path(pos, Direction::DirectionDownRight);
    map->del_path(map->move_down_right(pos), Direction::DirectionUpLeft);

    /* Disconnect flag. */
    unsigned int flag_index = building->get_flag_index();
    if (flag_index > 0) {
      Flag *flag = flags[flag_index];
      flag->unlink_building();
      building->unlink_flag();
      flag_reset_transport(flag);
    }

    /*
    // !!!
    // !!!  NEVERMIND, do not do this here.  when building is burned the game should be calling game->delete_inventory
    // !!!   which does this, investigate why it doesn't seem to be working
    // !!!
    //----------------------------------------
    // tlongstretch - delete the inventory
    //  I am seeing crash where AI thread tries to check
    //  the inventory contents, AND THE POINTER IS STILL VALID
    //  but the inventory building was destroyed already
    //  and I guess it invalidates the contents
    //  NOTE - when a Castle (and possibly Stock) is captured, its ownership
    //   is transferred to the conquering player BEFORE this triggers, meaning this code appears to delete
    //   a CONQUERING player's inventory.  Does this actually cause a problem, though?  it should be
    //   okay actually
    if (building->get_type() == Building::TypeCastle || building->get_type() == Building::TypeStock){
      Log::Warn["game.cc"] << "inside Game::demolish_building_, this is an inventory building of type " << NameBuilding[building->get_type()] << " at pos " << pos << ", deleting its index from game inventories array";
      //Inventory *destroyed_inventory = building->get_inventory();
      //destroyed_inventory = nullptr;
      //try this instead
      inventories.erase(building->get_inventory()->get_index());
      Log::Debug["game.cc"] << "inside Game::demolish_building_, this is an inventory building of type " << NameBuilding[building->get_type()] << " at pos " << pos << ", inventory has been erased from game";
    }
    */
    // sanity check
    if (building->get_type() == Building::TypeCastle || building->get_type() == Building::TypeStock){
      if (building->get_inventory() == nullptr){
        Log::Warn["game.cc"] << "inside Game::demolish_building_, this is an inventory building of type " << NameBuilding[building->get_type()] << " its inventory pointer is nullptr, which it should be";
      }else{
        Log::Warn["game.cc"] << "inside Game::demolish_building_, this is an inventory building of type " << NameBuilding[building->get_type()] << " its inventory pointer is NOT nullptr, WHY NOT???";
      }
    }

    return true;
  }

  return false;
}

/* Demolish building at pos. */
bool
Game::demolish_building(MapPos pos, Player *player) {
  Building *building = buildings[map->get_obj_index(pos)];

  if (building->get_owner() != player->get_index()) return false;
  if (building->is_burning()) return false;

  Log::Debug["game.cc"] << "inside Game::demolish_building, calling demolish_building_ on building at pos " << pos << " owned by player #" << player->get_index();
  return demolish_building_(pos);
}

/* Map pos is lost to the owner, demolish everything. */
void
Game::surrender_land(MapPos pos) {
  /* Remove building. */
  if (map->get_obj(pos) >= Map::ObjectSmallBuilding &&
      map->get_obj(pos) <= Map::ObjectCastle) {
    //Log::Debug["game.cc"] << "inside Game::surrender_land, calling demolish_building_ on building at pos " << pos;
    demolish_building_(pos);
  }

  if (!map->has_flag(pos) && map->paths(pos) != 0) {
    demolish_road_(pos);
  }

  int remove_roads = map->has_flag(pos);

  /* Remove roads and building around pos. */
  for (Direction d : cycle_directions_cw()) {
    MapPos p = map->move(pos, d);

    if (map->get_obj(p) >= Map::ObjectSmallBuilding &&
        map->get_obj(p) <= Map::ObjectCastle) {
      //Log::Debug["game.cc"] << "inside Game::surrender_land around pos " << pos << ", currently at pos " << p << " with dir " << d << ", B calling demolish_building_";
      demolish_building_(p);
    }

    if (remove_roads &&
        (map->paths(p) & BIT(reverse_direction(d)))) {
      demolish_road_(p);
    }
  }

  /* Remove flag. */
  if (map->get_obj(pos) == Map::ObjectFlag) {
    demolish_flag_(pos);
  }
}

/* Initialize land ownership for whole map. */
void
Game::init_land_ownership() {
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex inside Game::init_land_ownership";
  mutex.lock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " has locked mutex inside Game::init_land_ownership";
  for (Building *building : buildings) {
    if (building->is_military()) {
      update_land_ownership(building->get_position());
    }
  }
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is unlocking mutex inside Game::init_land_ownership";
  mutex.unlock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " has unlocking mutex inside Game::init_land_ownership";
}

/* Update land ownership around map position. */
void
Game::update_land_ownership(MapPos init_pos) {
  /* Currently the below algorithm will only work when
     both influence_radius and calculate_radius are 8. */
  const int influence_radius = 8;
  const int influence_diameter = 1 + 2*influence_radius;

  int calculate_radius = influence_radius;
  int calculate_diameter = 1 + 2*calculate_radius;

  size_t temp_arr_size = calculate_diameter * calculate_diameter *
                         players.size();
  std::unique_ptr<int[]> temp_arr =
    std::unique_ptr<int[]>(new int[temp_arr_size]());

  const int military_influence[] = {
    0, 1, 2, 4, 7, 12, 18, 29, -1, -1,  /* hut */
    0, 3, 5, 8, 11, 15, 22, 30, -1, -1,  /* tower */
    0, 6, 10, 14, 19, 23, 27, 31, -1, -1  /* fortress */
  };

  const int map_closeness[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0,
    1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, 0, 0, 0, 0, 0,
    1, 2, 3, 4, 4, 4, 4, 4, 4, 3, 2, 1, 0, 0, 0, 0, 0,
    1, 2, 3, 4, 5, 5, 5, 5, 5, 4, 3, 2, 1, 0, 0, 0, 0,
    1, 2, 3, 4, 5, 6, 6, 6, 6, 5, 4, 3, 2, 1, 0, 0, 0,
    1, 2, 3, 4, 5, 6, 7, 7, 7, 6, 5, 4, 3, 2, 1, 0, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 7, 6, 5, 4, 3, 2, 1,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 2, 1,
    0, 0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 6, 5, 4, 3, 2, 1,
    0, 0, 0, 1, 2, 3, 4, 5, 6, 6, 6, 6, 5, 4, 3, 2, 1,
    0, 0, 0, 0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 4, 3, 2, 1,
    0, 0, 0, 0, 0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 3, 2, 1,
    0, 0, 0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1,
    0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1
  };

  /* Find influence from buildings in 33*33 square
     around the center. */
  for (int i = -(influence_radius+calculate_radius);
       i <= influence_radius+calculate_radius; i++) {
    for (int j = -(influence_radius+calculate_radius);
         j <= influence_radius+calculate_radius; j++) {
      MapPos pos = map->pos_add(init_pos, j, i);

      if (map->get_obj(pos) >= Map::ObjectSmallBuilding &&
          map->get_obj(pos) <= Map::ObjectCastle &&
          map->has_path(pos,
                   DirectionDownRight)) {  // TODO(_): Why wouldn't this be set?
        Building *building = get_building_at_pos(pos);
        int mil_type = -1;

        if (building->get_type() == Building::TypeCastle) {
          /* Castle has military influence even when not done. */
          mil_type = 2;
        } else if (building->is_done() && building->is_active()) {
          switch (building->get_type()) {
            case Building::TypeHut: mil_type = 0; break;
            case Building::TypeTower: mil_type = 1; break;
            case Building::TypeFortress: mil_type = 2; break;
            default: break;
          }
        }

        if (mil_type >= 0 && !building->is_burning()) {
          const int *influence = military_influence + 10*mil_type;
          const int *closeness = map_closeness +
                                 influence_diameter*std::max(-i, 0) +
                                 std::max(-j, 0);
          int *arr = temp_arr.get() +
            (building->get_owner() * calculate_diameter*calculate_diameter) +
            calculate_diameter * std::max(i, 0) + std::max(j, 0);

          for (int k = 0; k < influence_diameter - abs(i); k++) {
            for (int l = 0; l < influence_diameter - abs(j); l++) {
              int inf = influence[*closeness];
              if (inf < 0) {
                *arr = 128;
              } else if (*arr < 128) {
                *arr = std::min(*arr + inf, 127);
              }

              closeness += 1;
              arr += 1;
            }
            closeness += abs(j);
            arr += abs(j);
          }
        }
      }
    }
  }

  /* Update owner of 17*17 square. */
  for (int i = -calculate_radius; i <= calculate_radius; i++) {
    for (int j = -calculate_radius; j <= calculate_radius; j++) {
      int max_val = 0;
      int player_index = -1;
      for (Player *player : players) {
        const int *arr = temp_arr.get() +
          player->get_index()*calculate_diameter*calculate_diameter +
          calculate_diameter*(i+calculate_radius) + (j+calculate_radius);
        if (*arr > max_val) {
          max_val = *arr;
          player_index = player->get_index();
        }
      }

      MapPos pos = map->pos_add(init_pos, j, i);
      int old_player = -1;
      if (map->has_owner(pos)) old_player = map->get_owner(pos);

      if (old_player >= 0 && player_index != old_player) {
        players[old_player]->decrease_land_area();
        surrender_land(pos);
      }

      if (player_index >= 0) {
        if (player_index != old_player) {
          players[player_index]->increase_land_area();
          map->set_owner(pos, player_index);
        }
      } else {
        map->del_owner(pos);
      }
    }
  }

  /* Update military building flag state. */
  for (int i = -25; i <= 25; i++) {
    for (int j = -25; j <= 25; j++) {
      MapPos pos = map->pos_add(init_pos, i, j);

      if (map->get_obj(pos) >= Map::ObjectSmallBuilding &&
          map->get_obj(pos) <= Map::ObjectCastle &&
          map->has_path(pos, DirectionDownRight)) {
        Building *building = buildings[map->get_obj_index(pos)];
        if (building->is_done() && building->is_military()) {
          building->update_military_flag_state();
        }
      }
    }
  }
}

void
Game::demolish_flag_and_roads(MapPos pos) {
  if (map->has_flag(pos)) {
    /* Remove roads around pos. */
    for (Direction d : cycle_directions_cw()) {
      MapPos p = map->move(pos, d);

      if (map->paths(p) & BIT(reverse_direction(d))) {
        demolish_road_(p);
      }
    }

    if (map->get_obj(pos) == Map::ObjectFlag) {
      demolish_flag_(pos);
    }
  } else if (map->paths(pos) != 0) {
    demolish_road_(pos);
  }
}

/* The given building has been defeated and is being
   occupied by player. */
void
Game::occupy_enemy_building(Building *building, int player_num) {
  // debug only
  int old_owner = building->get_owner();

  /* Take the building. */
  Player *player = players[player_num];

  player->building_captured(building);

  if (building->get_type() == Building::TypeCastle) {
    Log::Debug["game.cc"] << "inside Game::occupy_enemy_building, calling demolish_building_ on building at pos " << building->get_position() << " of type " << NameBuilding[building->get_type()] << ", which was owned by Player#" << old_owner << " and has just been taken by Player" << player->get_index();
    demolish_building_(building->get_position());
  } else {
    Flag *flag = flags[building->get_flag_index()];
    flag_reset_transport(flag);

    /* Demolish nearby buildings. */
    for (int i = 0; i < 12; i++) {
      MapPos pos = map->pos_add_spirally(building->get_position(), 7+i);
      if (map->get_obj(pos) >= Map::ObjectSmallBuilding &&
          map->get_obj(pos) <= Map::ObjectCastle) {
        demolish_building_(pos);
      }
    }

    /* Change owner of land and remove roads and flags
       except the flag associated with the building. */
    map->set_owner(building->get_position(), player_num);

    for (Direction d : cycle_directions_cw()) {
      MapPos pos = map->move(building->get_position(), d);
      map->set_owner(pos, player_num);
      if (pos != flag->get_position()) {
        demolish_flag_and_roads(pos);
      }
    }

    /* Change owner of flag. */
    flag->set_owner(player_num);

    /* Reset destination of stolen resources. */
    flag->reset_destination_of_stolen_resources();

    /* Remove paths from flag. */
    for (Direction d : cycle_directions_cw()) {
      if (flag->has_path(d)) {
        demolish_road_(map->move(flag->get_position(), d));
      }
    }

    update_land_ownership(building->get_position());
  }
}

/* mode: 0: IN, 1: STOP, 2: OUT */
void
Game::set_inventory_resource_mode(Inventory *inventory, int mode) {
  Flag *flag = flags[inventory->get_flag_index()];

  if (mode == 0) {
    inventory->set_res_mode(Inventory::ModeIn);
  } else if (mode == 1) {
    inventory->set_res_mode(Inventory::ModeStop);
  } else {
    inventory->set_res_mode(Inventory::ModeOut);
  }

  if (mode > 0) {
    flag->set_accepts_resources(false);

    /* Clear destination of serfs with resources destined
       for this inventory. */
    int dest = flag->get_index();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex inside Game::set_inventory_resource_mode";
  mutex.lock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex inside Game::set_inventory_resource_mode";
    for (Serf *serf : serfs) {
      serf->clear_destination2(dest);
    }
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex inside Game::set_inventory_resource_mode";
  mutex.unlock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex inside Game::set_inventory_resource_mode";
  } else {
    flag->set_accepts_resources(true);
  }
}

/* mode: 0: IN, 1: STOP, 2: OUT */
void
Game::set_inventory_serf_mode(Inventory *inventory, int mode) {
  Flag *flag = flags[inventory->get_flag_index()];

  if (mode == 0) {
    inventory->set_serf_mode(Inventory::ModeIn);
  } else if (mode == 1) {
    inventory->set_serf_mode(Inventory::ModeStop);
  } else {
    inventory->set_serf_mode(Inventory::ModeOut);
  }

  if (mode > 0) {
    flag->set_accepts_serfs(false);

    /* Clear destination of serfs destined for this inventory. */
    int dest = flag->get_index();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex inside Game::set_inventory_serf_mode";
  mutex.lock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex inside Game::set_inventory_serf_mode";
    for (Serf *serf : serfs) {
      serf->clear_destination(dest);
    }
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex inside Game::set_inventory_serf_mode";
  mutex.unlock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex inside Game::set_inventory_serf_mode";
  } else {
    flag->set_accepts_serfs(true);
  }
}

// Add new player to the game. Returns the player number.
unsigned int
Game::add_player(unsigned int intelligence, unsigned int supplies,
                 unsigned int reproduction) {
  /* Allocate object */
  Player *player = players.allocate();
  if (player == nullptr) {
    throw ExceptionFreeserf("Failed to create new player.");
  }

  player->init(intelligence, supplies, reproduction);

  /* Update map values dependent on player count */
  map_gold_morale_factor = 10 * 1024 * static_cast<int>(players.size());

  return player->get_index();
}

bool
//Game::init(unsigned int map_size, const Random &random) {
Game::init(unsigned int map_size, const Random &random, const CustomMapGeneratorOptions custom_map_generator_options) {
  init_map_rnd = random;

  map.reset(new Map(MapGeometry(map_size)));

  if (game_type == GameMission) {
    ClassicMissionMapGenerator generator(*map, init_map_rnd);
    generator.init();
    generator.generate();
    map->init_tiles(generator);
  } else {
    //ClassicMapGenerator generator(*map, mission->get_random_base());
    CustomMapGenerator generator(*map, init_map_rnd);
    //for (int x = 0; x < 23; x++){
    //  Log::Info["game.cc"] << "inside Game::init,  x" << x << " = " << custom_map_generator_options.opt[x];
    //}
    // testing HeightGeneratorDiamondSquare
    //  I tried DiamondSquare but it looks the same to me, leaving it as default Midpoints setting
    // trying preserve_bugs = false
    generator.init(MapGenerator::HeightGeneratorMidpoints, false, custom_map_generator_options);
    //generator.init(MapGenerator::HeightGeneratorDiamondSquare, true, custom_map_generator_options);
    generator.generate();
    map->init_tiles(generator);
  }

  gold_total = map->get_gold_deposit();

  return true;
}

/* Cancel a resource being transported to destination. This
   ensures that the destination can request a new resource. */
void
Game::cancel_transported_resource(Resource::Type res, unsigned int dest) {
  if (dest == 0) {
    return;
  }

  Flag *flag = flags[dest];

  if (!flag->has_building()) {
    // got exception here for the first time ever jan01 2022, wonder why
    //  again jan03 2022, adding nullptr check above
    // it isn't flag that has nullptr, but that flag does not have building I guess
    // changing this to Warn for now
    //throw ExceptionFreeserf("Failed to cancel transported resource.");
    Log::Warn["game.cc"] << "inside cancel_transported_resource, flag->has_building call returned false!  building was expected, returning without cancelling request";
    return;
  }
  Building *building = flag->get_building();
  building->cancel_transported_resource(res);
}

/* Called when a resource is lost forever from the game. This will
   update any global state keeping track of that resource. */
void
Game::lose_resource(Resource::Type res) {
  if (res == Resource::TypeGoldOre || res == Resource::TypeGoldBar) {
    add_gold_total(-1);
  }
}

uint16_t
Game::random_int() {
  return rnd.random();
}

int
Game::next_search_id() {
  flag_search_counter += 1;

  /* If we're back at zero the counter has overflown,
   everything needs a reset to be safe. */
  if (flag_search_counter == 0) {
    flag_search_counter += 1;
    clear_search_id();
  }

  return flag_search_counter;
}

Serf *
Game::create_serf(int index) {
  if (index == -1) {
    return serfs.allocate();
  } else {
    return serfs.get_or_insert(index);
  }
}

void
Game::delete_serf(Serf *serf) {
  serfs.erase(serf->get_index());
}

Flag *
Game::create_flag(int index) {
  if (index == -1) {
    return flags.allocate();
  } else {
    return flags.get_or_insert(index);
  }
}

Inventory *
Game::create_inventory(int index) {
  if (index == -1) {
    return inventories.allocate();
  } else {
    return inventories.get_or_insert(index);
  }
}

void
Game::delete_inventory(Inventory *inventory) {
  inventories.erase(inventory->get_index());
}

Building *
Game::create_building(int index) {
  if (index == -1) {
    return buildings.allocate();
  } else {
    return buildings.get_or_insert(index);
  }
}

void
Game::delete_building(Building *building) {
  map->set_object(building->get_position(), Map::ObjectNone, 0);
  buildings.erase(building->get_index());
}

Game::ListSerfs
Game::get_player_serfs(Player *player) {
  ListSerfs player_serfs;

  // debug -  count how many serfs actually appear here
  //  does it include unused serfs??
  // it DOES include them as Serf objects, isn't that really inefficient??? can be 10k 
  //Log::Debug["game"] << "inside get_player_serfs, game Serfs has " << serfs.size() << " items";
  for (Serf *serf : serfs) {
    if (serf->get_owner() == player->get_index()) {
      player_serfs.push_back(serf);
    }
  }
  //Log::Debug["game"] << "inside get_player_serfs, player" << player->get_index() << " player_serfs has " << player_serfs.size() << " items";

  return player_serfs;
}

Game::ListBuildings
Game::get_player_buildings(Player *player) {
  ListBuildings player_buildings;

  for (Building *building : buildings) {
    // got exception here Nov 2022, adding nullptr check
    if (building == nullptr){
      Log::Warn["game.cc"] << "inside Game::get_player_buildings for Player" << player->get_index() << ", building is nullptr! skipping it";
      continue;
    }      
    if (building->get_owner() == player->get_index()) {
      player_buildings.push_back(building);
    }
  }

  return player_buildings;
}

Game::ListInventories
Game::get_player_inventories(Player *player) {
  ListInventories player_inventories;

  for (Inventory *inventory : inventories) {
    if (inventory == nullptr){
      // do NOT log this, once any Inventory is destroyed its index will always be a nullptr and it will repeat the message constantly
      //Log::Warn["game.cc"] << "inside Game::get_player_inventories for player #" << player->get_index() << ", an inventory is nullptr!  was it just destroyed?  NOTE THAT THIS INVENTORY COULD HAVE BEEN OWNED BY ANY PLAYER NOT JUST Player" << player->get_index();
      continue;
    }
    if (inventory->get_owner() == player->get_index()) {
      player_inventories.push_back(inventory);
    }
  }

  return player_inventories;
}

Game::ListSerfs
Game::get_serfs_at_pos(MapPos pos) {
  ListSerfs result;

  for (Serf *serf : serfs) {
    if (serf->get_pos() == pos) {
      result.push_back(serf);
    }
  }

  return result;
}

Game::ListSerfs
Game::get_serfs_in_inventory(Inventory *inventory) {
  ListSerfs result;

  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex inside Game::get_serfs_in_inventory";
  mutex.lock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex inside Game::get_serfs_in_inventory";
  for (Serf *serf : serfs) {
    if (serf->get_state() == Serf::StateIdleInStock &&
        inventory->get_index() == serf->get_idle_in_stock_inv_index()) {
      result.push_back(serf);
    }
  }

  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex inside Game::get_serfs_in_inventory";
  mutex.unlock();
  Log::Verbose["game"] << "thread #" << std::this_thread::get_id() << " is locking mutex inside Game::get_serfs_in_inventory";
  return result;
}

Game::ListSerfs
Game::get_serfs_related_to(unsigned int dest, Direction dir) {
  ListSerfs result;

  for (Serf *serf : serfs) {
    if (serf->is_related_to(dest, dir)) {
      result.push_back(serf);
    }
  }

  return result;
}

Player *
Game::get_next_player(const Player *player) {
  auto p = players.begin();
  while (*p != player) {
    ++p;
  }
  ++p;
  if (p == players.end()) {
    p = players.begin();
  }

  return (*p);
}

unsigned int
Game::get_enemy_score(const Player *player) const {
  unsigned int enemy_score = 0;

  for (const Player* p : players) {
    if (player->get_index() != p->get_index()) {
      enemy_score += p->get_total_military_score();
    }
  }

  return enemy_score;
}

void
Game::building_captured(Building *building) {
  /* Save amount of land and buildings for each player */
  std::map<int, int> land_before;
  std::map<int, int> buildings_before;
  for (Player *player : players) {
    land_before[player->get_index()] = player->get_land_area();
    buildings_before[player->get_index()] = player->get_building_score();
  }

  /* Update land ownership */
  update_land_ownership(building->get_position());

  /* Create notfications for lost land and buildings */
  for (Player *player : players) {
    if (buildings_before[player->get_index()] > player->get_building_score()) {
      player->add_notification(Message::TypeLostBuildings,
                               building->get_position(),
                               building->get_owner());
    } else if (land_before[player->get_index()] > player->get_land_area()) {
      player->add_notification(Message::TypeLostLand,
                               building->get_position(),
                               building->get_owner());
    }
  }
}

void
Game::clear_search_id() {
  for (Flag *flag : flags) {
    flag->clear_search_id();
  }
}

SaveReaderBinary&
operator >> (SaveReaderBinary &reader, Game &game) {
  /* Load these first so map dimensions can be reconstructed.
   This is necessary to load map positions. */

  reader.skip(74);
  uint16_t v16;
  reader >> v16;  // 74
  game.game_type = v16;
  reader >> v16;  // 76
  reader >> v16;  // 78
  game.tick = v16;
  game.game_stats_counter = 0;
  game.history_counter = 0;

  reader.skip(4);

  uint16_t r1, r2, r3;
  reader >> r1;  // 84
  reader >> r2;  // 86
  reader >> r3;  // 88
  game.rnd = Random(r1, r2, r3);

  reader >> v16;  // 90
  int max_flag_index = v16;
  reader >> v16;  // 92
  int max_building_index = v16;
  reader >> v16;  // 94
  int max_serf_index = v16;

  reader >> v16;  // 96
  game.next_index = v16;
  reader >> v16;  // 98
  game.flag_search_counter = v16;

  reader.skip(4);

  for (int i = 0; i < 4; i++) {
    reader >> v16;  // 104 + i*2
    game.player_history_index[i] = v16;
  }

  for (int i = 0; i < 3; i++) {
    reader >> v16;  // 112 + i*2
    game.player_history_counter[i] = v16;
  }

  reader >> v16;  // 118
  game.resource_history_index = v16;

//  if (0/*game.Gameype == GameYPE_TUTORIAL*/) {
//    game.tutorial_level = *reinterpret_cast<uint16_t*>(&data[122]);
//  } else if (0/*game.Gameype == GameYPE_MISSION*/) {
//    game.mission_level = *reinterpret_cast<uint16_t*>(&data[124]);
//  }

  reader.skip(54);

  reader >> v16;  // 174
  int max_inventory_index = v16;

  reader.skip(4);
  reader >> v16;  // 180
  game.max_next_index = v16;

  reader.skip(8);
  reader >> v16;  // 190
  int map_size = v16;

  // Avoid allocating a huge map if the input file is invalid
  if (map_size < 3 || map_size > 10) {
    throw ExceptionFreeserf("Invalid map size in file");
  }

  game.map.reset(new Map(MapGeometry(map_size)));

  reader.skip(8);
  reader >> v16;  // 200
  game.map_gold_morale_factor = v16;
  reader.skip(2);
  uint8_t v8;
  reader >> v8;  // 204
  game.player_score_leader = v8;

  reader.skip(45);

  /* Load players state from save game. */
  for (int i = 0; i < 4; i++) {
    SaveReaderBinary player_reader = reader.extract(8628);
    player_reader.skip(130);
    player_reader >> v8;
    if (BIT_TEST(v8, 6)) {
      player_reader.reset();
      Player *player = game.players.get_or_insert(i);
      player_reader >> *player;
    }
  }

  /* Load map state from save game. */
  unsigned int tile_count = game.map->get_cols() * game.map->get_rows();
  SaveReaderBinary map_reader = reader.extract(8 * tile_count);
  map_reader >> *(game.map);

  game.load_serfs(&reader, max_serf_index);
  game.load_flags(&reader, max_flag_index);
  game.load_buildings(&reader, max_building_index);
  game.load_inventories(&reader, max_inventory_index);

  game.game_speed = 0;
  game.game_speed_save = DEFAULT_GAME_SPEED;

  game.init_land_ownership();

  game.gold_total = game.map->get_gold_deposit();

  return reader;
}

/* Load serf state from save game. */
bool
Game::load_serfs(SaveReaderBinary *reader, int max_serf_index) {
  /* Load serf bitmap. */
  int bitmap_size = 4*((max_serf_index + 31)/32);
  uint8_t *bitmap = reader->read(bitmap_size);
  if (bitmap == NULL) return false;

  /* Load serf data. */
  for (int i = 0; i < max_serf_index; i++) {
    SaveReaderBinary serf_reader = reader->extract(16);
    if (BIT_TEST(bitmap[(i)>>3], 7-((i)&7))) {
      Serf *serf = serfs.get_or_insert(i);
      serf_reader >> *serf;
    }
  }

  return true;
}

/* Load flags state from save game. */
bool
Game::load_flags(SaveReaderBinary *reader, int max_flag_index) {
  /* Load flag bitmap. */
  int bitmap_size = 4*((max_flag_index + 31)/32);
  uint8_t *bitmap = reader->read(bitmap_size);
  if (bitmap == NULL) return false;

  /* Load flag data. */
  for (int i = 0; i < max_flag_index; i++) {
    SaveReaderBinary flag_reader = reader->extract(70);
    if (BIT_TEST(bitmap[(i)>>3], 7-((i)&7))) {
      Flag *flag = flags.get_or_insert(i);
      flag_reader >> *flag;
    }
  }

  /* Set flag positions. */
  for (MapPos pos : map->geom()) {
    if (map->get_obj(pos) == Map::ObjectFlag) {
      Flag *flag = flags[map->get_obj_index(pos)];
      flag->set_position(pos);
    }
  }

  return true;
}

/* Load buildings state from save game. */
bool
Game::load_buildings(SaveReaderBinary *reader, int max_building_index) {
  /* Load building bitmap. */
  int bitmap_size = 4*((max_building_index + 31)/32);
  uint8_t *bitmap = reader->read(bitmap_size);
  if (bitmap == NULL) return false;

  /* Load building data. */
  for (int i = 0; i < max_building_index; i++) {
    SaveReaderBinary building_reader = reader->extract(18);
    if (BIT_TEST(bitmap[(i)>>3], 7-((i)&7))) {
      Building *building = buildings.get_or_insert(i);
      building_reader >> *building;
    }
  }

  return true;
}

void
Game::add_gold_total(int delta) {
  if (delta < 0) {
    if (static_cast<int>(gold_total) < -delta) {
      throw ExceptionFreeserf("Failed to decrease global gold counter.");
    }
  }
  gold_total += delta;
}

/* Load inventories state from save game. */
bool
Game::load_inventories(SaveReaderBinary *reader, int max_inventory_index) {
  /* Load inventory bitmap. */
  int bitmap_size = 4*((max_inventory_index + 31)/32);
  uint8_t *bitmap = reader->read(bitmap_size);
  if (bitmap == NULL) return false;

  /* Load inventory data. */
  for (int i = 0; i < max_inventory_index; i++) {
    SaveReaderBinary inventory_reader = reader->extract(120);
    if (BIT_TEST(bitmap[(i)>>3], 7-((i)&7))) {
      Inventory *inventory = inventories.get_or_insert(i);
      inventory_reader >> *inventory;
    }
  }

  return true;
}

Building *
Game::get_building_at_pos(MapPos pos) {
  Map::Object map_obj = map->get_obj(pos);
  if (map_obj >= Map::ObjectSmallBuilding && map_obj <= Map::ObjectCastle) {
    return buildings[map->get_obj_index(pos)];
  }
  return NULL;
}

Flag *
Game::get_flag_at_pos(MapPos pos) {
  if (map->get_obj(pos) != Map::ObjectFlag) {
    return NULL;
  }

  return flags[map->get_obj_index(pos)];
}

Serf *
Game::get_serf_at_pos(MapPos pos) {
  return serfs[map->get_serf_index(pos)];
}

SaveReaderText&
operator >> (SaveReaderText &reader, Game &game) {
  /* Load essential values for calculating map positions
   so that map positions can be loaded properly. */
  Readers sections = reader.get_sections("game");
  if (sections.size() == 0) {
    throw ExceptionFreeserf("Failed to find section \"game\"");
  }

  SaveReaderText *game_reader = sections.front();
  unsigned int size = 0;
  try {
    game_reader->value("map.size") >> size;
  } catch (...) {
    unsigned int col_size = 0;
    unsigned int row_size = 0;
    game_reader->value("map.col_size") >> col_size;
    game_reader->value("map.row_size") >> row_size;
    size = (col_size + row_size) - 9;
  }

  /* Initialize remaining map dimensions. */
  game.map.reset(new Map(MapGeometry(size)));
  for (SaveReaderText* subreader : reader.get_sections("map")) {
    *subreader >> *game.map;
  }

//  std::string version;
//  reader.value("version") >> version;
//  LOGV("savegame", "Loading save game from version %s.", version.c_str());

  game_reader->value("game_type") >> game.game_type;
  game_reader->value("tick") >> game.tick;
  game_reader->value("game_stats_counter") >> game.game_stats_counter;
  game_reader->value("history_counter") >> game.history_counter;
  std::string rnd_str;
  try {
    game_reader->value("random") >> rnd_str;
    game.rnd = Random(rnd_str);
  } catch (...) {
    game_reader->value("rnd") >> rnd_str;
    std::stringstream ss;
    ss << rnd_str;
    uint16_t r1, r2, r3;
    char c;
    ss >> r1 >> c >> r2 >> c >> r3;
    game.rnd = Random(r1, r2, r3);
  }
  game_reader->value("next_index") >> game.next_index;
  game_reader->value("flag_search_counter") >> game.flag_search_counter;
  for (int i = 0; i < 4; i++) {
    game_reader->value("player_history_index")[i] >>
                                                   game.player_history_index[i];
  }
  for (int i = 0; i < 3; i++) {
    game_reader->value("player_history_counter")[i] >>
                                                 game.player_history_counter[i];
  }
  game_reader->value("resource_history_index") >> game.resource_history_index;
  game_reader->value("max_next_index") >> game.max_next_index;
  game_reader->value("map.gold_morale_factor") >> game.map_gold_morale_factor;
  game_reader->value("player_score_leader") >> game.player_score_leader;

  game_reader->value("gold_deposit") >> game.gold_total;

  Map::UpdateState update_state;
  int x, y;
  game_reader->value("update_state.remove_signs_counter") >>
    update_state.remove_signs_counter;
  game_reader->value("update_state.last_tick") >> update_state.last_tick;
  game_reader->value("update_state.counter") >> update_state.counter;
  game_reader->value("update_state.initial_pos")[0] >> x;
  game_reader->value("update_state.initial_pos")[1] >> y;
  update_state.initial_pos = game.map->pos(x, y);
  game.map->set_update_state(update_state);

  for (SaveReaderText* subreader : reader.get_sections("player")) {
    Player *p = game.players.get_or_insert(subreader->get_number());
    *subreader >> *p;
  }

  for (SaveReaderText* subreader : reader.get_sections("flag")) {
    Flag *p = game.flags.get_or_insert(subreader->get_number());
    *subreader >> *p;
  }

  sections = reader.get_sections("building");
  for (SaveReaderText* subreader : reader.get_sections("building")) {
    Building *p = game.buildings.get_or_insert(subreader->get_number());
    *subreader >> *p;
  }

  for (SaveReaderText* subreader : reader.get_sections("inventory")) {
    Inventory *p = game.inventories.get_or_insert(subreader->get_number());
    *subreader >> *p;
  }

  for (SaveReaderText* subreader : reader.get_sections("serf")) {
    Serf *p = game.serfs.get_or_insert(subreader->get_number());
    *subreader >> *p;
  }

  /* Restore idle serf flag */
  for (Serf *serf : game.serfs) {
    if (serf->get_index() == 0) continue;

    if (serf->get_state() == Serf::StateIdleOnPath ||
        serf->get_state() == Serf::StateWaitIdleOnPath) {
      
      /* doing this is no better than the usual stuck serf detector logic (though it works outside of AI)
      //  and still has the side effect of the booted serf never arriving to do his job, which is generally
      //  either acting as a transporter (so would need that missing transporter detection also) or a building
      //  occupier and the building will never be filled
      // debug WaitIdleOnPath
      if (serf->get_state() == Serf::StateWaitIdleOnPath) {
        Log::Warn["game"] << "StateWaitIdleOnPath check, a serf with state " << serf->get_state() << " at pos " << serf->get_pos() << " with type " << serf->get_type() << " is being set_idle_serf";
        if (serf->debug_get_idle_on_path_flag() == 1){
          Log::Error["game"] << "StateWaitIdleOnPath check, a serf with state " << serf->get_state() << " at pos " << serf->get_pos() << " with type " << serf->get_type() << " is being set_idle_serf and has s.idle_on_path.flag index of " << serf->debug_get_idle_on_path_flag();
          // try "disappearing" this serf
          //serf->set_serf_state(Serf::StateNull);
          //delete_serf(serf);
          //game.delete_serf(serf);
          //serf->debug_set_pos(bad_map_pos);
          //Log::Error["game"] << "StateWaitIdleOnPath check, set serf to bad_map_pos";
          serf->set_lost_state();
          continue;
        }
      }
      // end debug WaitIdleOnPath
      */

      game.map->set_idle_serf(serf->get_pos());
    }
  }

  /* Restore building index */
  for (Building *building : game.buildings) {
    if (building->get_index() == 0) continue;

    if (game.map->get_obj(building->get_position()) <
          Map::ObjectSmallBuilding ||
        game.map->get_obj(building->get_position()) > Map::ObjectCastle) {
      std::ostringstream str;
      str << "Map data does not match building " << building->get_index() <<
        " position.";
      throw ExceptionFreeserf(str.str());
    }

    game.map->set_obj_index(building->get_position(), building->get_index());
  }

  /* Restore flag index */
  for (Flag *flag : game.flags) {
    if (flag->get_index() == 0) continue;

    if (game.map->get_obj(flag->get_position()) != Map::ObjectFlag) {
      std::ostringstream str;
      str << "Map data does not match flag " << flag->get_index() <<
        " position.";
      throw ExceptionFreeserf(str.str());
    }

    game.map->set_obj_index(flag->get_position(), flag->get_index());
  }

  game.game_speed = 0;
  game.game_speed_save = DEFAULT_GAME_SPEED;

  game.init_land_ownership();

  return reader;
}

SaveWriterText&
operator << (SaveWriterText &writer, Game &game) {
  writer.value("map.size") << game.map->get_size();
  writer.value("game_type") << game.game_type;
  writer.value("tick") << game.tick;
  writer.value("game_stats_counter") << game.game_stats_counter;
  writer.value("history_counter") << game.history_counter;
  writer.value("random") << (std::string)game.rnd;

  writer.value("next_index") << game.next_index;
  writer.value("flag_search_counter") << game.flag_search_counter;

  for (int i = 0; i < 4; i++) {
    writer.value("player_history_index") << game.player_history_index[i];
  }
  for (int i = 0; i < 3; i++) {
    writer.value("player_history_counter") << game.player_history_counter[i];
  }
  writer.value("resource_history_index") << game.resource_history_index;

  writer.value("max_next_index") << game.max_next_index;
  writer.value("map.gold_morale_factor") << game.map_gold_morale_factor;
  writer.value("player_score_leader") << game.player_score_leader;

  writer.value("gold_deposit") << game.gold_total;

  const Map::UpdateState& update_state = game.map->get_update_state();
  writer.value("update_state.remove_signs_counter") <<
    update_state.remove_signs_counter;
  writer.value("update_state.last_tick") << update_state.last_tick;
  writer.value("update_state.counter") << update_state.counter;
  writer.value("update_state.initial_pos") << game.map->pos_col(
    update_state.initial_pos);
  writer.value("update_state.initial_pos") << game.map->pos_row(
    update_state.initial_pos);

  for (Player *player : game.players) {
    SaveWriterText &player_writer = writer.add_section("player",
                                                       player->get_index());
    player_writer << *player;
  }

  for (Flag *flag : game.flags) {
    if (flag->get_index() == 0) continue;
    SaveWriterText &flag_writer = writer.add_section("flag", flag->get_index());
    flag_writer << *flag;
  }

  for (Building *building : game.buildings) {
    if (building->get_index() == 0) continue;
    SaveWriterText &building_writer = writer.add_section("building",
                                                         building->get_index());
    building_writer << *building;
  }

  for (Inventory *inventory : game.inventories) {
    SaveWriterText &inventory_writer = writer.add_section("inventory",
                                                        inventory->get_index());
    inventory_writer << *inventory;
  }

  for (Serf *serf : game.serfs) {
    if (serf->get_index() == 0) continue;
    SaveWriterText &serf_writer = writer.add_section("serf", serf->get_index());
    serf_writer << *serf;
  }

  writer << *game.map;

  return writer;
}

