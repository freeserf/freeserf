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

#include "src/version.h" // for tick_length

#define GROUND_ANALYSIS_RADIUS  25

// deFINE the global game option bools that were deCLARED in game-options.h
bool option_EnableAutoSave = false;
//bool option_ImprovedPigFarms = false;  // removing this as it turns out the default behavior for pig farms is to require almost no grain
bool option_CanTransportSerfsInBoats = false;  // leaving this off by default because it still has occasional bugs
bool option_QuickDemoEmptyBuildSites = true;
//bool option_AdvancedDemolition = true;  // this needs more playtesting  */
bool option_TreesReproduce = true;
bool option_BabyTreesMatureSlowly = false;  // the AI needs to be improved to handle this being on, it relies too much on Foresters/Rangers
bool option_ResourceRequestsTimeOut = true;  // this is forced true to indicate that the code to make them optional isn't added yet
bool option_PrioritizeUsableResources = true;    // this is forced true to indicate that the code to make them optional isn't added yet
bool option_LostTransportersClearFaster = true;
bool option_FourSeasons = true;
bool option_AdvancedFarming = true;
bool option_FishSpawnSlowly = true;
bool option_FogOfWar = true;
//bool option_EastSlopesShadeObjects = true;   // make this an option, maybe
bool option_InvertMouse = false;
bool option_InvertWheelZoom = false;
bool option_SpecialClickBoth = true;
bool option_SpecialClickMiddle = true;
bool option_SpecialClickDouble = true;
bool option_SailorsMoveFaster = true;
bool option_WaterDepthLuminosity = true;
bool option_RandomizeInstruments = true;  // only affects DOS music
bool option_ForesterMonoculture = false;  // this looks bad in Spring and Winter, not making default anymore
bool option_CheckPathBeforeAttack = true;  // this is forced on

// map generator settings
/*
    // reasonable values for trees are 0.00-4.00, so divide max slider 65500 by 4 to get 16375 and let 1.00 == 16375
    interface->set_custom_map_generator_trees(uint16_t(16375 * 1.00));
    interface->set_custom_map_generator_stonepile_dense(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_stonepile_sparse(slider_double_to_uint16(1.00)); 
    //interface->set_custom_map_generator_fish(slider_double_to_uint16(1.00)); 
    // reasonable values for fish are 0.00-4.00, so divide max slider 65500 by 4 to get 16375 and let 1.00 == 16375
    interface->set_custom_map_generator_fish(uint16_t(16375 * 1.00));
    interface->set_custom_map_generator_mountain_gold(slider_mineral_double_to_uint16(2.00));   // 2
    interface->set_custom_map_generator_mountain_iron(slider_mineral_double_to_uint16(4.00));   // 4
    interface->set_custom_map_generator_mountain_coal(slider_mineral_double_to_uint16(9.00));   // 9
    interface->set_custom_map_generator_mountain_stone(slider_mineral_double_to_uint16(2.00));  // 2
    interface->set_custom_map_generator_desert_frequency(slider_double_to_uint16(1.00)); 
    //interface->set_custom_map_generator_lakes_size(slider_double_to_uint16(1.00)); 
    //interface->set_custom_map_generator_lakes_water_level(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_lakes_water_level(uint16_t(8188 * 1.00)); 
    interface->set_custom_map_generator_junk_grass_sandstone(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_junk_grass_small_boulders(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_junk_grass_stub_trees(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_junk_grass_dead_trees(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_junk_water_boulders(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_junk_water_trees(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_junk_desert_cadavers(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_junk_desert_cacti(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_junk_desert_palm_trees(slider_double_to_uint16(1.00)); 
    */
unsigned int mapgen_size = 3;
uint16_t mapgen_trees = 16375;
uint16_t mapgen_stonepile_dense = 32750;
uint16_t mapgen_stonepile_sparse = 32750;
uint16_t mapgen_fish = 16375;
uint16_t mapgen_mountain_gold = 7278 * 2;
uint16_t mapgen_mountain_iron = 7278 * 4;
uint16_t mapgen_mountain_coal = 7278 * 9;
uint16_t mapgen_mountain_stone = 7278 * 2;
uint16_t mapgen_desert_frequency = 32750;
uint16_t mapgen_water_level = 8188;
uint16_t mapgen_junk_grass_sandstone = 32750;
uint16_t mapgen_junk_grass_small_boulders = 32750;
uint16_t mapgen_junk_grass_stub_trees = 32750;
uint16_t mapgen_junk_grass_dead_trees = 32750;
uint16_t mapgen_junk_water_boulders = 32750;
uint16_t mapgen_junk_water_trees = 32750;
uint16_t mapgen_junk_desert_cadavers = 32750;
uint16_t mapgen_junk_desert_cacti = 32750;
uint16_t mapgen_junk_desert_palm_trees = 32750;
uint16_t mapgen_junk_water_reeds_cattails = 4096;


int season = 1;  // default to Summer
int last_season = 1;  // four seasons
int subseason = 0;  // 1/16th of a season
int last_subseason = 0;
bool is_list_in_focus = false;
typedef enum Season {
  SeasonSpring = 0,
  SeasonSummer = 1,
  SeasonFall = 2,
  SeasonWinter = 3,
} Season;
// the Custom data map_objects offset for tree sprites
int season_sprite_offset[4] = {
  1200, // Spring
     0, // Summer  (use original sprites)
  1400, // Fall
  1300, // Winter
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

  debug_mark_road = {};

  ai_locked = true;
  signal_ai_exit = false;
  ai_threads_remaining = 0;
  mutex_message = "";
  mutex_timer_start = 0;
  must_redraw_frame = false;  // part of hack for option_FogOfWar
  
  MapPos desired_cursor_pos = bad_map_pos;  // to allow Game to set the Interface/Viewport player cursor pos (during Interface::update)
}

Game::~Game() {
  serfs.clear();
  inventories.clear();
  buildings.clear();
  flags.clear();
  players.clear();
  desired_cursor_pos = bad_map_pos; // I think this was causing issues on new game?
}

/* Clear the serf request bit of all flags and buildings.
   This allows the flag or building to try and request a
   serf again. */
void
Game::clear_serf_request_failure() {
  mutex_lock("Game::clear_serf_request_failure");
  for (Building *building : buildings) {
    building->clear_serf_request_failure();
  }
  for (Flag *flag : flags) {
    flag->serf_request_clear();
  }
  mutex_unlock();
}

void
Game::update_knight_morale() {
  mutex_lock("Game::update_knight_morale");
  for (Player *player : players) {
    player->update_knight_morale();
  }
  mutex_unlock();
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
      //mutex_lock("Game::update_inventories");
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

      //mutex_unlock();

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
  mutex_lock("Game::update_flags");
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
  mutex_unlock();
}

typedef struct SendSerfToFlagData {
  Inventory *inventory;
  Building *building;
  int serf_type;
  int dest_index;
  Resource::Type res1;
  Resource::Type res2;
} SendSerfToFlagData;


// Return true if this flag is connected to an Inventory
//  that can provide a serf of this type, either because an idle
//  serf of this type is available or by creating a new one, possibly
//  consuming tools if required
// The "if have tools" check is done here, and the tool consumption
//  happens at the calling function send_sert_to_flag
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
    //
    // knights, either idle-in-stock or can be created
    //
    int knight_type = -1;
    for (int i = 4; i >= -type-1; i--) {
      if (inv->have_serf((Serf::Type)(Serf::TypeKnight0+i))) {
        knight_type = i;
        break;
      }
    }
    if (knight_type >= 0) {
      // Knight of appropriate type was found
      Serf *serf = inv->call_out_serf((Serf::Type)(Serf::TypeKnight0+knight_type));
      data->building->knight_request_granted();
      serf->go_out_from_inventory(inv->get_index(), data->building->get_flag_index(), -1);
      return true;
    } else if (type == -1) {
      // See if a knight can be created here
      if (inv->have_serf(Serf::TypeGeneric) &&
          inv->get_count_of(Resource::TypeSword) > 0 &&
          inv->get_count_of(Resource::TypeShield) > 0) {
        data->inventory = inv;
        return true;
      }
    }
  } else {
    //
    // non-knight specialist serfs
    //
    if (inv->have_serf((Serf::Type)type)) {
      // ... already idle in inventory, call out
      // if an idle serf of this type already exists, or 5+ free serfs
      //  available to be specialized (I guess the intent is to reserve
      //  four to be made available as transporters?  or knights?)
      if (type != Serf::TypeGeneric || inv->free_serf_count() > 4) {
        Serf *serf = inv->call_out_serf((Serf::Type)type);
        int mode = 0;
        if (type == Serf::TypeGeneric) {
          // don't know what this means
          mode = -2;
        } else if (type == Serf::TypeGeologist) {
          // don't know what this means
          mode = 6;
        } else {
          // notify the calling building that its request for
          //  a new Holder serf was granted
          Building *dest_bld = flag->get_game()->flags[data->dest_index]->get_building();
          dest_bld->serf_request_granted();
          mode = -1;
        }
        serf->go_out_from_inventory(inv->get_index(), data->dest_index, mode);
        return true;
      }
    } else {
      // ...not idle-in-stock, must be created
      // check to see if available tool[s] to specialize a generic serf
      //  into a new professional
      // NOTE - this check is here, instead of much earlier as might seem logical,
      //  because the tool needs to come from the same inventory the generic serf
      //  is specialized from, and to figure out which Inventory that is, the 
      //  flagsearch must be done!
      if (data->inventory == NULL &&
          inv->have_serf(Serf::TypeGeneric) &&
          (data->res1 == -1 || inv->get_count_of(data->res1) > 0) &&
          (data->res2 == -1 || inv->get_count_of(data->res2) > 0)) {
        data->inventory = inv;

        // I have no idea what this stuff is, it was commented out this way
        //  in the Freeserf code, I have not modified at all
        /* player_t *player = globals->player[SERF_PLAYER(serf)]; */
        /* game.field_340 = player->cont_search_after_non_optimal_find; */

        return true;
      }
    }
  }
  // serf could not be dispatched
  return false;
}

// Dispatch a non-road-transporter serf from the nearest
//  *capable* inventory to flags/buildings.  Includes both
//  knights and specialized/professioal serfs that may require
//  tools.  
// Creates a new specialist/professioal serf or knight if necessary,
//  which will consume tools/weapons
// Returns true if a serf was sent, false if it could not be sent
//
// The "if have tools" check is done here inside the flagsearch call back
//  send_serf_to_flag_cb, and the tool consumption
//  happens here
// NOTE - this function is NOT used for calling transporters to roads, 
//  if I create a new road I see a transporter sent and arrive but never see this called for it
//  instead, it looks like Inventory->call_transporter is called
// NOTE - specialize_serf and specialize_free_serf functios are NOT USED HERE
//  it seems those are only used during initial castle creation and when
//  new knights are spawned inside the castle/stock/warehouse
bool
Game::send_serf_to_flag(Flag *dest, Serf::Type type, Resource::Type res1,
                        Resource::Type res2) {
  //Log::Debug["game"] << "inside Game::send_serf_to_flag, serf type " << type << ", res1 " << res1 << ", res2 " << res2;         
  Building *building = NULL;
  if (dest->has_building()) {
    building = dest->get_building();
  }

  /* If type is negative, building is non-NULL. */
  if ((type < 0) && (building != NULL)) {
    Player *player = players[building->get_owner()];
    type = player->get_cycling_serf_type(type);
  }

  SendSerfToFlagData data;
  data.inventory = NULL;
  data.building = building;
  data.serf_type = type;
  data.dest_index = dest->get_index();
  data.res1 = res1;  // tool1
  data.res2 = res2;  // tool2

  // this callback returns true if either an existing idle serf can be sent out from an Inv
  //  or if a new serf can be created (consuming tools or weapons) and sent out from an Inv
  bool r = FlagSearch::single(dest, send_serf_to_flag_search_cb, true, false, &data);

  if (!r) {
    // cannot send a serf of this type. previous flagsearch could find no reachable, capable inventory 
    //  that has either a serf of requested type sitting idle, or tools to create one
    //Log::Info["game"] << "inside send_serf_to_flag, serf type " << NameSerf[type] << ", request no reachable, capable Inventory found to provide this type of serf";
    return false;
  } else if (data.inventory != NULL) {
    // an Inventory was found that can create a new serf of this type (no idle serf of this type available to call)
    //  NOTE - if an existing idle serf of requested type was found, and a new one does NOT need to be created,
    //   then data.inventory would be NULL.  It seems that == NULL means "have one" and != NULL means "create one"
    //Log::Debug["game"] << "inside send_serf_to_flag, serf type " << NameSerf[type] << ", an Inventory was found that can create a new serf of this type";}
    Inventory *inventory = data.inventory;
    Serf *serf = inventory->call_out_serf(Serf::TypeGeneric);
    if ((type < 0) && (building != NULL)) {
      // Serf::Type -1 / TypeNone
      //  means a knight was requested to a building
      //  but no knight is available, so create a new one
      building->knight_request_granted();
      serf->set_type(Serf::TypeKnight0);
      serf->go_out_from_inventory(inventory->get_index(), building->get_flag_index(), -1);
      inventory->pop_resource(Resource::TypeSword);
      inventory->pop_resource(Resource::TypeShield);
    } else {
      // it *seems* like this condition is "non-knight serf requested and/or invalid building destination"
      //  but what happens if the building has no destination?  is this an edge case that could cause a bug?
      //  will add a warning to see if this happens
      // NO - this is allowed for Geologists being sent out to mountain flags to do work
      //  it is only if the serf being sent is NOT a geologist that there is a problem to Warn about
      // debug
      //if (building == NULL){
      //  Log::Warn["game.cc"] << "inside Game::send_serf_to_flag, possible edge case detected, type is " << type << ", but building is NULLptr!  Is this a problem?";
      //}

      serf->set_type((Serf::Type)type);
      // 'mode' seems to set s.ready_to_leave_inventory.mode but it
      //  is not clear at all how it is used
      int mode = 0;
      if (type == Serf::TypeGeologist) {
        mode = 6;
      } else {
        if (building == NULL) {
          Log::Warn["game.cc"] << "inside Game::send_serf_to_flag, a valid destination building was found to dispatch this non-geologist serf to, but the destination building is now a nullptr!  returning false";
          return false;
        }
        building->serf_request_granted();
        mode = -1;
      }
      serf->go_out_from_inventory(inventory->get_index(), dest->get_index(), mode);
      // consume Tool[s]
      if (res1 != Resource::TypeNone) inventory->pop_resource(res1);
      if (res2 != Resource::TypeNone) inventory->pop_resource(res2);
    }
    // an existing idle serf/knight of the requested type was dispatched
    //  a new one was NOT required to be created and so no tools consumed
    return true;
  }
  return true;
}

/* Dispatch geologist to flag. */
bool
Game::send_geologist(Flag *dest) {
  return send_serf_to_flag(dest, Serf::TypeGeologist, Resource::TypeHammer, Resource::TypeNone);
}

/* Update buildings as part of the game progression. */
void
Game::update_buildings() {
  mutex_lock("Game::update_buildings");
  Buildings blds(buildings);
  Buildings::Iterator i = blds.begin();
  while (i != blds.end()) {
    Building *building = *i;
    ++i;
    building->update(tick);
  }
  mutex_unlock();
}

/* Update serfs as part of the game progression. */
void
Game::update_serfs() {
  mutex_lock("Game::update_serfs");
  Serfs::Iterator i = serfs.begin();
  while (i != serfs.end()) {
    Serf *serf = *i;
    ++i;
    if (serf == nullptr){
      continue;
    }
    if (serf->get_index() != 0) {
      serf->update();
    }
  }
  mutex_unlock();
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
  //AdvancedDemolition
  Log::Info["game"] << "option_LostTransportersClearFaster is " << option_LostTransportersClearFaster;
  Log::Info["game"] << "option_FourSeasons is " << option_FourSeasons;
  //option_AdvancedFarming
  Log::Info["game"] << "option_FishSpawnSlowly is " << option_FishSpawnSlowly;
  //option_FogOfWar
  //option_InvertMouse
  */

  /* Increment tick counters */
  const_tick += 1;  // NOTE!!! anything that was using const_tick to keep realtime is now accelerated by "cpu warp" game speeds 2-10!
                    //   FIND ALL PLACES THAT const_tick IS USED AND ADJUST TO USE SDL_GetTick INSTEAD!

  /* Update tick counters based on game speed */
  last_tick = tick;
  //tick += game_speed;
  // cpu warp vs time warp
  //  at game speed 0 the game is paused, and AI will also pause at start of next loop
  //  at game speed 1 the game runs at half normal speed by reducing game_ticks per event loop.  This causes choppy graphics
  //   I don't think it is possible to reduce choppiness because there simply aren't any in-between-tick animation frames to show
  //   the AI runs still at normal speed, I think
  //  at game speeds 2-10 , the frequency of game updates (and apparent FPS) is increased by reducing the SDL_Timer sleep time
  //   from base of 20 milliseconds, down 10 milliseconds where it seems to hit a wall (maybe because my CPU is limited?)
  //  at game speeds 11-40, the frequency of game updates (and apparent FPS) is restored back to the default SDL_Timer, but
  //   instead the "game_ticks" is increased by more than 2 at a time, speeding up animations and game progresion without
  //   actually running the various Game::update_XXX calls any more frequently.  Supposedly this can cause bugs where
  //   events are missed or mis-timed because their tick is missed as a result of this?  I haven't actually seen anything
  //   like this, but it sounds possible.
  //  NOTE - in Freeserf there is only "time warp" and the game always runs with SDL_Timer of 20 (20ms between updates)
  //   which results in about 50 "FPS" (i.e. updates per second) maximum.  Adding "cpu warp" is new to Forkserf.
  //int game_ticks_per_update = 2;  // this is a global/extern now
  game_ticks_per_update = 2;
  if (game_speed == 0){
    game_ticks_per_update = 0;  // paused
  }else if (game_speed == 1){
    game_ticks_per_update = 1;  // choppy slow motion
  }else if (game_speed > 12){
    // ex,
    // game_speed 13 is 2x normal game speed
    // game_speed 14 is 3x normal game speed
    // game_speed 20 is 10x normal game speed (20 + 2 - 12 = 10 * 2 = 20 game_ticks/update, 10x normal game speed)
    // game_speed 30 is 20x normal game speed, this was the maximum warp speed possible in the Freeserf code and has been extensively tested
    // game_speed 40 is 30x normal game speed, this has never been tested!!!
    game_ticks_per_update = (game_speed - 10) * 2;
  }
  //tick += 1;  // setting a tick of 1 results in "slow motion" even if SDL_Timer is increased, don't do this!
  tick += game_ticks_per_update;
  tick_diff = tick - last_tick;

  //Log::Info["game"] << "current game_speed " << game_speed << " SDL_Timer " << tick_length << "ms, progression/game tick " << game_ticks_per_update;

  clear_serf_request_failure();
  map->update(tick, &init_map_rnd);

  /* Update players */
  // this must be mutex locked because new serfs are born during this step, and allocating new serfs invalidates any other serf iterators
  //  player->update calls spawn_serf which calls Game::create_serf which calls serfs.allocate()
  //  the mutex lock is INSIDE player->update, go look there
  for (Player *player : players) {
    // for option_FogOfWar, if a human player doesn't have a castle yet, build it for them
    //  using the same logic as AI uses (copied AI functions to Game, maybe combine them?)
    if (option_FogOfWar && !player->has_castle() && (player->get_face() == 12 || player->get_face() == 13)){
      MapPos built_pos = auto_place_castle(player);
      set_update_viewport_cursor_pos(built_pos);
    }
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
    // it seems that FPS stops increasing once tick_length is reduced to 10ms
    //  not sure if this is the performance limit of my PC or something else
    //  for now, make 10 the lower limit of "physics warp"
    //if (tick_length > 1){
    if (game_speed < 13 && game_speed > 2 && tick_length > 10){
      tick_length -= 1;
    }else{
      tick_length = DEFAULT_TICK_LENGTH;
    }
    Log::Info["game"] << "Game speed: " << game_speed;
  }
}

void
Game::speed_decrease() {
  if (game_speed >= 1) {
    game_speed -= 1;
    // note that the default game speed is 2, which is 2 ticks passed per
    //  game event loop.  If ticks is reduced to 1 as in game_speed 1, the
    //  game slows down to a choppy slow-motion that even if event loop speed
    //  is increased ("physics warp") still looks bad. I think this is because
    //  the animations are choppy, they expect min 2 ticks per frame to play smoothly?
    //if (tick_length < DEFAULT_TICK_LENGTH){
    if (game_speed < 13 && game_speed > 2){
      tick_length = DEFAULT_TICK_LENGTH - game_speed + DEFAULT_GAME_SPEED;
    }else{
      tick_length = DEFAULT_TICK_LENGTH;
    }
    Log::Info["game"] << "Game speed: " << game_speed;
  }
}

void
Game::speed_reset() {
  game_speed = DEFAULT_GAME_SPEED;
  tick_length = DEFAULT_TICK_LENGTH;
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
        //Log::Debug["game"] << "about to call set_lost_state on serf at pos " << pos << " case1, map says no flag here";;
      } else {
        /* Handle serf close to flag, where
           it should only be lost if walking
           in the wrong direction. */
        // I think I understand what this code is doing now:
        // When a road is removed, any serfs ON the road are made lost
        // If there is a serf in the very last pos, the flag pos,
        //  he can be allowed to continue without becoming lost if
        //  he is walking away from the now-destroyed road, but if
        //  he is walking towards the now-destroyed road he should be
        //  made lost.  
        // HOWEVER, there is some bug that causes transporters to be unintentionally removed
        //  so I have suppressed this behavior
        int d = serf->get_walking_dir();
        if (d < 0) d += 6;
        if (d == reverse_direction(dir)) {
          Log::Warn["game"] << "about to call set_lost_state on serf at pos " << pos << " case2, maps says flag here but note says serf is walking in the 'wrong direction'";
          Log::Warn["game"] << "ATTEMPTING TO WORK AROUND BUG BY NOT SETTING THIS SERF TO LOST, instead doing... nothing";
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
  //Log::Debug["game.cc"] << "inside Game::can_build_flag, pos " << pos << ", map->has_owner bool is " << map->has_owner(pos) << ", map->get_owner is " << map->get_owner(pos) << ", player index " << player->get_index();

  //if (!map->has_owner(pos))
  //  Log::Debug["game.cc"] << "inside Game::can_build_flag, A";
  //if (map->get_owner(pos) != player->get_index())
  //  Log::Debug["game.cc"] << "inside Game::can_build_flag, B";

  /* Check owner of land */
  //if (!map->has_owner(pos) || map->get_owner(pos) != player->get_index()) {
  if (!map->has_owner(pos) || map->get_owner(pos) != player->get_index()) {
    //Log::Debug["game.cc"] << "inside Game::can_build_flag, player index " << player->get_index() << ", ownership check FAILED";
    return false;
  }
  //Log::Debug["game.cc"] << "inside Game::can_build_flag, player index " << player->get_index() << ", ownership check passed";

  /* Check that land is clear */
  if (Map::map_space_from_obj[map->get_obj(pos)] != Map::SpaceOpen) {
    return false;
  }
  //Log::Debug["game.cc"] << "inside Game::can_build_flag, pos " << pos << ", player index " << player->get_index() << ", clearance check passed";

  /* Check whether cursor is in water */
  if (map->type_up(pos) <= Map::TerrainWater3 &&
      map->type_down(pos) <= Map::TerrainWater3 &&
      map->type_down(map->move_left(pos)) <= Map::TerrainWater3 &&
      map->type_up(map->move_up_left(pos)) <= Map::TerrainWater3 &&
      map->type_down(map->move_up_left(pos)) <= Map::TerrainWater3 &&
      map->type_up(map->move_up(pos)) <= Map::TerrainWater3) {
    return false;
  }

  //Log::Debug["game.cc"] << "inside Game::can_build_flag, pos " << pos << ", player index " << player->get_index() << ", water check passed";

  /* Check that no flags are nearby */
  for (Direction d : cycle_directions_cw()) {
    if (map->get_obj(map->move(pos, d)) == Map::ObjectFlag) {
      return false;
    }
  }
  //Log::Debug["game.cc"] << "inside Game::can_build_flag, pos " << pos << ", player index " << player->get_index() << ", flags-neaby check passed";

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

// this returns true if ONLY the terrain specified is in range
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
// NOTE - I think this misleading!  it does not consider
//  blocking objects nor position ownership
bool
Game::can_build_small(MapPos pos) const {
  // Freeserf was missing the original game check preventing buildings from being placed
  //  with its flag pos touching edge of water
  if (!map_types_within(map->move_down_right(pos), Map::TerrainGrass0, Map::TerrainGrass3)){
    return false;
  }
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
// I saw this return true once, but no castle seemed to be placed
//  and then a castle appeared far from the suppoesdly placed
//  very strange
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

  /* removing AdvancedDemolition for now, see https://github.com/forkserf/forkserf/issues/180
// mark building for demolition
//  by a Digger serf that is dispatched to it
//  only used with option_AdvancedDemolition
bool
Game::mark_building_for_demolition(MapPos pos, Player *player) {
  Building *building = buildings[map->get_obj_index(pos)];
  if (building == nullptr){
    Log::Warn["game.cc"] << "inside Game::mark_building_for_demolition, building at pos " << pos << " is nullptr!  returning false";
    return false;
  }
  if (building->get_owner() != player->get_index()){
    Log::Debug["game.cc"] << "inside Game::mark_building_for_demolition, building at pos " << pos << " is not owned by this player!  returning false";
    return false;
  }
  if (building->is_burning()){
    Log::Debug["game.cc"] << "inside Game::mark_building_for_demolition, building at pos " << pos << " is already on fire!  returning false";
    return false;
  }
  if (building->is_pending_demolition()){
    Log::Debug["game.cc"] << "inside Game::mark_building_for_demolition, building at pos " << pos << " is already pending demolition!  returning false";
    return false;
  }
  building->call_for_demolition();
  Log::Debug["game.cc"] << "inside Game::mark_building_for_demolition, pending_demolition set for building at pos " << pos << " owned by player #" << player->get_index();
  return true;
}
*/

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
  mutex_lock("Game::init_land_ownership");
  for (Building *building : buildings) {
    if (building->is_military()) {
      update_land_ownership(building->get_position());
    }
  }
  mutex_unlock();
}

/* Update land ownership around map position. */
void
Game::update_land_ownership(MapPos init_pos) {
  //Log::Debug["game.cc"] << "start of Game::update_land_ownership around pos " << init_pos;

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

// does this mean that larger military buildings have a stronger
//  influence on player borders??  I always assumed it was identical!
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
  // tlongstretch - 33x33??? that makes no sense, that is HUGE!
  //  it is indeed a 33x33 area, confirmed with debug overlay
  //  I guess because the castle influence area is so large, all
  //  comparisons must be this large
  //
  // FYI - the shape of the "square" as seen on the game map
  //  is like this  ____    with the building pos being evaluated 
  //               /   /     at the center of the parallelogram
  //              /___/
  //
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
          //Log::Debug["game.cc"] << "start of Game::update_land_ownership around pos " << init_pos << ", a building at pos " << building->get_position() << " is done and active, type is " << building->get_type() << " check if it is military";
          switch (building->get_type()) {
            case Building::TypeHut: mil_type = 0; break;
            case Building::TypeTower: mil_type = 1; break;
            case Building::TypeFortress: mil_type = 2; break;
            default: break;
          }
        }

        //Log::Debug["game.cc"] << "start of Game::update_land_ownership around pos " << init_pos << ", mil_type is " << mil_type;

        if (mil_type >= 0 && !building->is_burning()) {
          //Log::Debug["game.cc"] << "start of Game::update_land_ownership around pos " << init_pos << ", mil_type is " << mil_type << ", building is not burning";
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
      if (map->has_owner(pos)){
        old_player = map->get_owner(pos);
      }

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

  if (option_FogOfWar){
    update_FogOfWar(init_pos);
  }

  //Log::Debug["game.cc"] << "done Game::update_land_ownership around pos " << init_pos;

}

// when a new game is started, or a game is loaded, or option_FogOfWar is enabled mid-game
//  the entire map must be updated at once

void
Game::init_FogOfWar() {
  Log::Debug["game.cc"] << "start of Game::init_FogOfWar for all military buildings in entire game";
  mutex_lock("Game::init_FogOfWar");
  for (Building *building : buildings) {
    update_FogOfWar(building->get_position());
  }
  mutex_unlock();
}

void
Game::update_FogOfWar(MapPos init_pos) {
  //Log::Debug["game.cc"] << "start of Game::update_FogOfWar around init_pos " << init_pos;

  // need to redraw terrain for FoW updates to be visible
  set_must_redraw_frame();

  //const int visible_radius_by_type[25] = {0,0,0,0,0,0,0,0,0,0,0,10,0,0,0,0,0,0,0,0,0,15,19,0,21};
  // visually I like the smaller radius, but increasing it so that attackable buildings are always visible...
  //const int visible_radius_by_type[25] = {0,0,0,0,0,0,0,0,0,0,0,15,0,0,0,0,0,0,0,0,0,20,23,0,23};
  // ugh this still isnt enough, maybe consider making attackable buildings always visible & attackable
  //  even if not actually within visible range?
  const int visible_radius_by_type[25] = {0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,20,23,0,23};
  int reveal_beyond = 6;  // added to visible_radius when setting
  // _spiral_dist / AI::spiral_dist ONLY GOES UP TO 25!!  DO NOT ALLOW REVEAL RADIUS TO BE LARGER!
  // copied from AI::spiral_dist, MAKE THIS GLOBAL!
const int _spiral_dist[49] = { 1, 7, 19, 37, 61, 91, 127, 169, 217, 271, 331, 397,
    469, 547, 631, 721, 817, 919, 1027, 1141, 1261, 1387, 1519, 1657, 1801, 1951,
    2107, 2269, 2437, 2611, 2791, 2977, 3169, 3367, 3571, 3781, 3997, 4219, 4447,
    4681, 4921, 5167, 5419, 5677, 5941, 6211, 6487, 6769 };

  // IDEA - another possible way to handle FoW is to have a counter for each pos (for each player)
  //  that indicates how many player-buildings currently contribute to the visibility of that pos
  //  and increment/decrement as buildings occupied/lost and draw based on a nonzero value
  //  this would avoid having to check the influence of nearby buildings and so may be much faster?

  // clear the visibility bit for all players for all pos within the influence_radius
  // along the way, note all other buildings in the area that can view tiles in this area
  //  so that their visibility can be set again after the area is wiped
  // NO! only unset_all_visible for the influence radius of the largest visible_radius_by_type (castle)
  //  but look for other influential buildings at DOUBLE that radius
  int influence_radius = visible_radius_by_type[Building::TypeCastle] * 2 + 1;  // THIS MUST BE AT LEAST *TWICE* LARGE AS THE LARGEST POSSIBLE visible_radius (castle's)
  //if (influence_radius > 48){ // HACK to allow castle radius 24
  //  influence_radius = 48; // extended_spiral only goes this high
  //}
  MapPosVector influential_buildings = {};
  // NOTE - if the building at init_pos exists and is occupied, it will be found here and its visible and revealed radius will be set
  //   if the building at init_pos was destroyed/lost, it will *not* be found here and so it will correctly lose visibilty
  for (int i = 0; i < _spiral_dist[influence_radius]; i++) {
    MapPos pos = map->pos_add_extended_spirally(init_pos, i);

    // TEMP DEBUG
    //set_debug_mark_pos(pos, "cyan");

    // trying this, I think this is solution to a bug
    if (i < _spiral_dist[visible_radius_by_type[Building::TypeCastle]]){
      map->unset_all_visible(pos);
    }

    // look for buildings that can see this pos, for restoration
    if (map->get_obj(pos) >= Map::ObjectSmallBuilding && map->get_obj(pos) <= Map::ObjectCastle) {
      Building *building = get_building_at_pos(pos);
      if (building == nullptr){
        Log::Error["game.cc"] << "inside Game::update_FogOfWar, expecting building at pos " << pos << ", but get_building_at_pos returned a nullptr! crashing";
        throw ExceptionFreeserf("inside Game::update_FogOfWar, expecting building at pos but get_building_at_pos returned a nullptr!");
      }
      if (!building->is_military()){
        continue;
      }
      if (building->get_type() != Building::TypeCastle && !building->is_active()){
        //Log::Debug["game.cc"] << "inside of Game::update_FogOfWar, non-castle military building of type " << NameBuilding[building->get_type()] << " is not active, skipping";
        continue;
      }
      /* removing AdvancedDemolition for now, see https://github.com/forkserf/forkserf/issues/180
      if (building->is_burning() || building->is_pending_demolition()){
        Log::Debug["game.cc"] << "inside of Game::update_FogOfWar, military building of type " << NameBuilding[building->get_type()] << " at pos " << pos << " is burning or pending_demolition, skipping";
        continue;
      }
      */
      /* don't check this, this is bad logic
      int bld_vis_rad = visible_radius_by_type[building->get_type()];
      Log::Debug["game.cc"] << "inside of Game::update_FogOfWar, military building of type " << NameBuilding[building->get_type()] << " at pos " << pos <<  " has visible radius " << bld_vis_rad;
      if (i > _spiral_dist[bld_vis_rad]){
        Log::Debug["game.cc"] << "inside of Game::update_FogOfWar, military building of type " << NameBuilding[building->get_type()] << " at pos " << pos <<  " is beyond its visual radius, skipping";
        continue;
      }
      */
      //Log::Debug["game.cc"] << "inside of Game::update_FogOfWar, military building of type " << NameBuilding[building->get_type()] << " at pos " << pos << " is within influence radius, adding to list";
      influential_buildings.push_back(pos);
    }
  }
  // restore visibility for all building in influence_radius
  // NOTE this will frequently result in map->set_visible being called on pos
  //  that are outside the influence_radius, because usually only part of an other_building's
  //  visibility radius will be within the overall influence_radius being considered here
  // It might be faster to save the list of MapPos inside the influence_radius to avoid
  //  calling map->set_visible redundantly, but I suspect that would be slower than just doing it
  for (MapPos bld_pos : influential_buildings){
    Building *bld = get_building_at_pos(bld_pos);
    if (bld == nullptr){
      Log::Error["game.cc"] << "inside Game::update_FogOfWar, expecting bld at bld_pos " << bld_pos << ", but get_building_at_pos returned a nullptr! crashing";
      throw ExceptionFreeserf("inside Game::update_FogOfWar, expecting bld at bld_pos but get_building_at_pos returned a nullptr!");
    }
    Building::Type bld_type = bld->get_type();
    if (!bld->is_military()){
      Log::Error["game.cc"] << "inside Game::update_FogOfWar, expecting bld at bld_pos " << bld_pos << " to be a military building, but instead it is type " << bld_type << "! crashing";
      throw ExceptionFreeserf("inside Game::update_FogOfWar, expecting bld at bld_pos to be a military building, but it is not!");
      continue;
    }
    if (!bld->is_active()){
      // this building is burning or otherwise not occupied
      continue;
    }
    unsigned int player_index = bld->get_owner();
    // set the visibility bit for all buildings according to their radius
    // this list should always include the initial building being considered!
    //Log::Debug["game.cc"] << "inside of Game::update_FogOfWar, calling map->set_visible() for all pos within radius " << visible_radius_by_type[bld_type] << " of this building of type " << NameBuilding[bld_type] << " at bld_pos " << bld_pos;
    for (int i = 0; i < _spiral_dist[visible_radius_by_type[bld_type]]; i++) {
      MapPos pos = map->pos_add_extended_spirally(bld_pos, i);
      //Log::Debug["game.cc"] << "inside of Game::update_FogOfWar, calling map->set_visible() for pos " << pos << ", Player" << player_index;
      map->set_visible(pos, player_index);
    }
  }
  // if there is an occupied building at init_pos,
  // call set_revealed for the reveal_radius, for this building only
  //  because this is the only one changing
  // if this building were destroyed/lost, no change required because
  //  revealed state can never be lost
  Building *building = get_building_at_pos(init_pos);
  if (building != nullptr && building->is_military()) {
    unsigned int player_index = building->get_owner();
    int reveal_radius = visible_radius_by_type[building->get_type()] + reveal_beyond;
    //Log::Debug["game.cc"] << "inside of Game::update_FogOfWar, building at init_pos " << init_pos << " of type " << NameBuilding[building->get_type()] << " has reveal_radius " << reveal_radius << ", owned by Player" << player_index;
    for (int i = 0; i < _spiral_dist[reveal_radius]; i++) {
      MapPos pos = map->pos_add_extended_spirally(init_pos, i);
      //Log::Debug["game.cc"] << "inside of Game::update_FogOfWar, calling map->set_revealed() for pos " << pos << ", Player" << player_index;
      map->set_revealed(pos, player_index);
    }
  }else{
    //Log::Debug["game.cc"] << "inside of Game::update_FogOfWar, no building found at init_pos " << init_pos << ", assuming this was a destroyed/lost building.  Not setting revealed";
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
    mutex_lock("Game::set_inventory_resource_mode");
    for (Serf *serf : serfs) {
      serf->clear_destination2(dest);
    }
    mutex_unlock();
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
   mutex_lock("Game::set_inventory_serf_mode");
    for (Serf *serf : serfs) {
      serf->clear_destination(dest);
    }
    mutex_unlock();
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

  mutex_lock("Game::get_serfs_in_inventory");
  for (Serf *serf : serfs) {
    if (serf->get_state() == Serf::StateIdleInStock &&
        inventory->get_index() == serf->get_idle_in_stock_inv_index()) {
      result.push_back(serf);
    }
  }
  mutex_unlock();
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

// THE DESCRIPTION IS HIGHLY MISLEADING
//  this is called any time an empty military building is occupied by a knight
//  including player-owned buildings.  This means the vast majority of calls
//  are from a player's own newly-built buildings and not ones captured from
//  an enemy player
void
Game::building_captured(Building *building) {
  //Log::Debug["game.cc"] << "inside Game::building_captured, an empty military building at pos " << building->get_position() << " has been occupied by a knight";
  /* Save amount of land and buildings for each player */
  std::map<int, int> land_before;
  std::map<int, int> buildings_before;
  for (Player *player : players) {
    land_before[player->get_index()] = player->get_land_area();
    buildings_before[player->get_index()] = player->get_building_score();
  }

  /* Update land ownership */
  update_land_ownership(building->get_position());

  /* Create notifications for lost land and buildings */
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

// lock and unlock mutex during non-threadsafe
// iterations and changes between game and AI threads
void
Game::mutex_lock(const char* message){
  Log::Verbose["game.cc"] << "inside Game::mutex_lock, thread #" << std::this_thread::get_id() << " about to lock mutex, message: " << message;
  clock_t start = std::clock();
  mutex.lock();
  double wait_for_mutex = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  Log::Verbose["game.cc"] << "inside Game::mutex_lock, thread #" << std::this_thread::get_id() << " has locked mutex, message: " << mutex_message << ", waited " << wait_for_mutex << "sec for lock";
  // store the lock message and start a timer so message and time-in-mutex can be printed on unlock
  mutex_message = message;
  mutex_timer_start = std::clock();
}

void
Game::mutex_unlock(){
  // message is known from lock
  //Log::Error["game.cc"] << "inside Game::mutex_unlock, thread #" << std::this_thread::get_id() << " about to unlock mutex, message: " << mutex_message;
  double time_in_mutex = (std::clock() - mutex_timer_start) / static_cast<double>(CLOCKS_PER_SEC);
  mutex.unlock();
  Log::Verbose["game.cc"] << "inside Game::mutex_unlock, thread #" << std::this_thread::get_id() << " has unlocked mutex, message: " << mutex_message << ", spent " << time_in_mutex << "sec holding lock";
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
  if (option_FogOfWar){game.init_FogOfWar();}

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


// basically a copy of AI::do_place_castle
//  consider combining them somehow
MapPos
Game::auto_place_castle(Player *player) {
  Log::Debug["game.cc"] << "inside Game::auto_place_castle()";
  if (player == nullptr){
    Log::Warn["game.cc"] << "inside Game::auto_place_castle(), Player" << player->get_index() << " is nullptr!  cannot check player->has_castle status!";
    return bad_map_pos;
  }
  if (player->has_castle()) {
    Log::Debug["game.cc"] << "inside Game::auto_place_castle(), Player" << player->get_index() << ", already has a castle, returning early";
    return bad_map_pos;
  }else{
    Log::Debug["game.cc"] << "inside Game::auto_place_castle(), Player" << player->get_index() << ", does not yet have a castle";
    // place castle
    //   improve this so that it is more intelligent about other resources than trees/stones/building_sites
    //    but have the minimum scores reduced a bit for each area scored so it eventually settles on something
    //
    // pick random spots on the map until an acceptable area found, and try building there
    ///===========================================================================================================
    int maxtries = 500;  // crash if failed to place castle after this many tries, regardless of desperation
    int lower_standards_tries = 75;  // reduce standards after this many tries (can happen repeatedly)
    int desperation = 0;  // current level of lowered standards
    int x = 0;  // current try
    while (true) {
      x++;
      if (x > maxtries) {
        Log::Debug["game.cc"] << "inside Game::auto_place_castle(), Player" << player->get_index() << ", unable to place castle after " << x << " tries, maxtries reached!";
        throw ExceptionFreeserf("inside Game::auto_place_castle(), unable to place castle for a human player with option_FogOfWar after exhausting all tries!");
      }
      if (x > lower_standards_tries * (desperation + 1)){
        desperation++;
        Log::Debug["game.cc"] << "inside Game::auto_place_castle(), Player" << player->get_index() << ", unable to place castle after " << x << " tries, lowering standards to desperation level " << desperation;
      }
      MapPos pos = map->get_rnd_coord(NULL, NULL, get_rand());
      Log::Debug["game.cc"] << "inside Game::auto_place_castle(), Player" << player->get_index() << ", considering placing castle at random pos " << pos;
      // first see if it is even possible to build large building here
      if (!can_build_castle(pos, player)) {
        Log::Debug["game.cc"] << "inside Game::auto_place_castle(), Player" << player->get_index() << ", cannot build a castle at pos " << pos;
        continue;
      }
      // check if area has acceptable resources + building pos, and if so build there
      //if (place_castle(pos, spiral_dist(8), desperation)) {
      if (place_castle(pos, player->get_index(), 217, desperation)) {  // spiral_dist(8) is 217
        Log::Debug["game.cc"] << "inside Game::auto_place_castle(), Player" << player->get_index() << ", found acceptable place to build castle, at pos: " << pos;
        mutex_lock("Game::auto_place_castle calling game->build_castle");
        bool was_built = build_castle(pos, player);
        mutex_unlock();
        if (was_built) {
          Log::Debug["game.cc"] << "inside Game::auto_place_castle(), Player" << player->get_index() << ", built castle at pos: " << pos << " after " << x << " tries";
          return pos;
        }
        Log::Debug["game.cc"] << "inside Game::auto_place_castle(), Player" << player->get_index() << ", failed to build castle at pos: " << pos << ", will keep trying";
      }
    }
  }
  Log::Error["game.cc"] << "inside Game::auto_place_castle(), Player" << player->get_index() << ", COULD NOT FIND ACCEPTABLE PLACE FOR PLAYER CASTLE!  crashing";
  throw ExceptionFreeserf("inside Game::auto_place_castle(),COULD NOT FIND ACCEPTABLE PLACE FOR A HUMAN PLAYER CASTLE WITH option_FogOfWar ENABLED!");
}


// score specified area in terms of castle placement
//   initially this is just ensuring enough wood, stones, and building sites
//   long term it should also care about resources in the surrounding areas!  including mountains, fishable waters, etc.
//  desperation is a multiplier
bool
Game::place_castle(MapPos center_pos, int player_index, unsigned int distance, unsigned int desperation) {
  Log::Debug["game.cc"] << "inside Game::place_castle(), Player" << player_index << ", center_pos " << center_pos << ", distance " << distance << ", desperation " << desperation;
  PMap map = get_map();
  unsigned int trees = 0;
  unsigned int stones = 0;
  unsigned int building_sites = 0;

  // COPIED THESE FROM AI!  maybe make global?
  static const unsigned int near_building_sites_min = 450;
  static const unsigned int near_trees_min = 5;
  static const unsigned int near_stones_min = 5;

  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);

    // don't count resouces that are inside enemy territory
    if (map->get_owner(pos) != player_index && map->has_owner(pos)) {
      //Log::Debug["game.cc"] << "inside Game::place_castle(), Player" << player_index << ", enemy territory at pos " << pos << ", not counting these resouces towards requirements";
      continue;
    }

    Map::Object obj = map->get_obj(pos);
    if (obj >= Map::ObjectTree0 && obj <= Map::ObjectPine7) {
      trees += 1;
      //AILogDebug["util_place_castle"] << "adding trees count 1";
    }
    if (obj >= Map::ObjectStone0 && obj <= Map::ObjectStone7) {
      int stonepile_value = 1 + (-1 * (obj - Map::ObjectStone7));
      stones += stonepile_value;
      //AILogDebug["util_place_castle"] << "adding stones count " << stonepile_value;
    }
    if (can_build_large(pos)) {
      building_sites += 3;  // large building sites worth 50% more than small ones, but can't use 1.5 and 1.0 because integer
      //AILogDebug["util_place_castle"] << "adding large building value 3";
    }
    else if (can_build_small(pos)) {
      building_sites += 2;
      //AILogDebug["util_place_castle"] << "adding small building value 1";
    }
  }

  //Log::Debug["game.cc"] << "inside Game::place_castle(), Player" << player_index << ", found trees: " << trees << ", stones: " << stones << ", building_sites: " << building_sites << " in area " << center_pos << ". Desperation is " << desperation;

  // if good place for castle cannot be found, lower standards by faking an increased amount of resources
  if (trees + desperation*4 < near_trees_min * 4) {
    //Log::Debug["game.cc"] << "inside Game::place_castle(), Player" << player_index << ", not enough trees, min is " << near_trees_min * 4 << ", returning false.  Desperation is " << desperation;
    return false;
  }
  if (stones + desperation < near_stones_min) {
    //Log::Debug["game.cc"] << "inside Game::place_castle(), Player" << player_index << ", not enough stones, min is " << near_stones_min << ", returning false.  Desperation is " << desperation;
    return false;
  }
  if (building_sites + desperation*90 < near_building_sites_min) {
    //Log::Debug["game.cc"] << "inside Game::place_castle(), Player" << player_index << ", not enough building_sites, min is " << near_building_sites_min << ", returning false.  Desperation is " << desperation;
    return false;
  }
  Log::Debug["game.cc"] << "inside Game::place_castle(), Player" << player_index << ", center_pos: " << center_pos << " is an acceptable building site for a castle.  Desperation is " << desperation;
  Log::Debug["game.cc"] << "inside Game::place_castle(), Player" << player_index << ", done Game::place_castle";
  return true;
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
  if (option_FogOfWar){game.init_FogOfWar();}

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

