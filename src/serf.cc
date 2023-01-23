/*
 * serf.cc - Serf related implementation
 *
 * Copyright (C) 2013-2019  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/serf.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <string>

#include "src/game.h"
#include "src/log.h"
#include "src/debug.h"
#include "src/misc.h"
#include "src/inventory.h"
#include "src/savegame.h"
#include "src/game-options.h"

/* tlongstretch note
 pyrdacor's freeserf.net has added a bunch of nullptr checks, and converted
 the serf drop_resource function from void to bool, returning true is succesful
 and false if the flag serf is dropping to became a nullptr.  I do not think I 
 have ever seen these bugs myself, so not adding them but worth noting
 see details here: 
https://github.com/Pyrdacor/freeserf.net/commit/101de41d3933664a4db83f7858a0fa1b9f50f61a#diff-5c6a168729c704f96b9cad040fb9d1b3d1911ce244a61a9ccb9f0d4e926a0c6c
*/

#define set_state(new_state)  \
  Log::Verbose["serf"] << "serf " << index  \
                       << " (" << Serf::get_type_name(get_type()) << "): " \
                       << "state " << Serf::get_state_name(state) \
                       << " -> " << Serf::get_state_name((new_state)) \
                       << " (" << __FUNCTION__ << ":" << __LINE__ << ")"; \
  state = new_state;

#define set_other_state(other_serf, new_state)  \
  Log::Verbose["serf"] << "serf " << other_serf->index \
                       << " (" << Serf::get_type_name(other_serf->get_type()) \
                       << "): state " \
                       << Serf::get_state_name(other_serf->state) \
                       << " -> " << Serf::get_state_name((new_state)) \
                       << "(" << __FUNCTION__ << ":" << __LINE__ << ")"; \
  other_serf->state = new_state;


static const int counter_from_animation[] = {
  /* Walking (0-80) */
  511, 447, 383, 319, 255, 319, 511, 767, 1023,
  511, 447, 383, 319, 255, 319, 511, 767, 1023,
  511, 447, 383, 319, 255, 319, 511, 767, 1023,
  511, 447, 383, 319, 255, 319, 511, 767, 1023,
  511, 447, 383, 319, 255, 319, 511, 767, 1023,
  511, 447, 383, 319, 255, 319, 511, 767, 1023,
  511, 447, 383, 319, 255, 319, 511, 767, 1023,
  511, 447, 383, 319, 255, 319, 511, 767, 1023,
  511, 447, 383, 319, 255, 319, 511, 767, 1023,

  /* Waiting (81-86) */
  127, 127, 127, 127, 127, 127,

  /* Digging (87-88) */
  383, 383,

  255, 223, 191, 159, 127, 159, 255, 383,  511,

  /* Building (98) */
  255,

  /* Engage defending free (99) */
  255,

  /* Building large building (100) */
  255,

  0,

  /* Building (102-105) */
  767, 511, 511, 767,

  1023, 639, 639, 1023,

  /* Transporting (turning?) (110-115) */
  63, 63, 63, 63, 63, 63,

  /* Logging (116-120) */
  1023, 31, 767, 767, 255,

  /* Planting (121-122) */
  191, 127,

  /* Stonecutting (123) */
  1535,

  /* Sawing (124) */
  2367,

  /* Mining (125-128) */
  383, 303, 303, 383,

  /* Smelting (129-130) */
  383, 383,

  /* Fishing (131-134) */
  767, 767, 127, 127,

  /* Farming (135-136) */
  1471, 1983,

  /* Milling (137) */
  383,

  /* Baking (138) */
  767,

  /* Pig farming (139) */
  383,

  /* Butchering (140) */
  1535,

  /* Sampling geology (142) */
  783, 63,

  /* Making weapon (143) */
  575,

  /* Making tool (144) */
  1535,

  /* Building boat (145-146) */
  1407, 159,

  /* Attacking (147-156) */
  127, 127, 127, 127, 127, 127, 127, 127, 127, 127,

  /* Defending (157-166) */
  127, 127, 127, 127, 127, 127, 127, 127, 127, 127,

  /* Engage attacking (167) */
  191,

  /* Victory attacking (168) */
  7,

  /* Dying attacking (169-173) */
  255, 255, 255, 255, 255,

  /* Dying defending (174-178) */
  255, 255, 255, 255, 255,

  /* Occupy attacking (179) */
  127,

  /* Victory defending (180) */
  7
};


// this ref table is annoying and useless
//   remove it entirely from the game
static const char *serf_state_name[] = {
  "NULL",  // SERF_STATE_NULL
  "IDLE IN STOCK",  // SERF_STATE_IDLE_IN_STOCK
  "WALKING",  // SERF_STATE_WALKING
  "TRANSPORTING",  // SERF_STATE_TRANSPORTING
  "ENTERING BUILDING",  // SERF_STATE_ENTERING_BUILDING
  "LEAVING BUILDING",  // SERF_STATE_LEAVING_BUILDING
  "READY TO ENTER",  // SERF_STATE_READY_TO_ENTER
  "READY TO LEAVE",  // SERF_STATE_READY_TO_LEAVE
  "DIGGING",  // SERF_STATE_DIGGING
  "BUILDING",  // SERF_STATE_BUILDING
  "BUILDING CASTLE",  // SERF_STATE_BUILDING_CASTLE
  "MOVE RESOURCE OUT",  // SERF_STATE_MOVE_RESOURCE_OUT
  "WAIT FOR RESOURCE OUT",  // SERF_STATE_WAIT_FOR_RESOURCE_OUT
  "DROP RESOURCE OUT",  // SERF_STATE_DROP_RESOURCE_OUT
  "DELIVERING",  // SERF_STATE_DELIVERING
  "READY TO LEAVE INVENTORY",  // SERF_STATE_READY_TO_LEAVE_INVENTORY
  "FREE WALKING",  // SERF_STATE_FREE_WALKING
  "LOGGING",  // SERF_STATE_LOGGING
  "PLANNING LOGGING",  // SERF_STATE_PLANNING_LOGGING
  "PLANNING PLANTING",  // SERF_STATE_PLANNING_PLANTING
  "PLANTING",  // SERF_STATE_PLANTING
  "PLANNING STONECUTTING",  // SERF_STATE_PLANNING_STONECUTTING
  "STONECUTTER FREE WALKING",  // SERF_STATE_STONECUTTER_FREE_WALKING
  "STONECUTTING",  // SERF_STATE_STONECUTTING
  "SAWING",  // SERF_STATE_SAWING
  "LOST",  // SERF_STATE_LOST
  "LOST SAILOR",  // SERF_STATE_LOST_SAILOR
  "FREE SAILING",  // SERF_STATE_FREE_SAILING
  "ESCAPE BUILDING",  // SERF_STATE_ESCAPE_BUILDING
  "MINING",  // SERF_STATE_MINING
  "SMELTING",  // SERF_STATE_SMELTING
  "PLANNING FISHING",  // SERF_STATE_PLANNING_FISHING
  "FISHING",  // SERF_STATE_FISHING
  "PLANNING FARMING",  // SERF_STATE_PLANNING_FARMING
  "FARMING",  // SERF_STATE_FARMING
  "MILLING",  // SERF_STATE_MILLING
  "BAKING",  // SERF_STATE_BAKING
  "PIGFARMING",  // SERF_STATE_PIGFARMING
  "BUTCHERING",  // SERF_STATE_BUTCHERING
  "MAKING WEAPON",  // SERF_STATE_MAKING_WEAPON
  "MAKING TOOL",  // SERF_STATE_MAKING_TOOL
  "BUILDING BOAT",  // SERF_STATE_BUILDING_BOAT
  "LOOKING FOR GEO SPOT",  // SERF_STATE_LOOKING_FOR_GEO_SPOT
  "SAMPLING GEO SPOT",  // SERF_STATE_SAMPLING_GEO_SPOT
  "KNIGHT ENGAGING BUILDING",  // SERF_STATE_KNIGHT_ENGAGING_BUILDING
  "KNIGHT PREPARE ATTACKING",  // SERF_STATE_KNIGHT_PREPARE_ATTACKING
  "KNIGHT LEAVE FOR FIGHT",  // SERF_STATE_KNIGHT_LEAVE_FOR_FIGHT
  "KNIGHT PREPARE DEFENDING",  // SERF_STATE_KNIGHT_PREPARE_DEFENDING
  "KNIGHT ATTACKING",  // SERF_STATE_KNIGHT_ATTACKING
  "KNIGHT DEFENDING",  // SERF_STATE_KNIGHT_DEFENDING
  "KNIGHT ATTACKING VICTORY",  // SERF_STATE_KNIGHT_ATTACKING_VICTORY
  "KNIGHT ATTACKING DEFEAT",  // SERF_STATE_KNIGHT_ATTACKING_DEFEAT
  "KNIGHT OCCUPY ENEMY BUILDING",  // SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING
  "KNIGHT FREE WALKING",  // SERF_STATE_KNIGHT_FREE_WALKING
  "KNIGHT ENGAGE DEFENDING FREE",  // SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE
  "KNIGHT ENGAGE ATTACKING FREE",  // SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE
  "KNIGHT ENGAGE ATTACKING FREE JOIN",
                                 // SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE_JOIN
  "KNIGHT PREPARE ATTACKING FREE",  // SERF_STATE_KNIGHT_PREPARE_ATTACKING_FREE
  "KNIGHT PREPARE DEFENDING FREE",  // SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE
  "KNIGHT PREPARE DEFENDING FREE WAIT",
                                // SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE_WAIT
  "KNIGHT ATTACKING FREE",  // SERF_STATE_KNIGHT_ATTACKING_FREE
  "KNIGHT DEFENDING FREE",  // SERF_STATE_KNIGHT_DEFENDING_FREE
  "KNIGHT ATTACKING VICTORY FREE",  // SERF_STATE_KNIGHT_ATTACKING_VICTORY_FREE
  "KNIGHT DEFENDING VICTORY FREE",  // SERF_STATE_KNIGHT_DEFENDING_VICTORY_FREE
  "KNIGHT ATTACKING FREE WAIT",  // SERF_STATE_KNIGHT_ATTACKING_FREE_WAIT
  "KNIGHT LEAVE FOR WALK TO FIGHT",
                                    // SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT
  "IDLE ON PATH",  // SERF_STATE_IDLE_ON_PATH
  "WAIT IDLE ON PATH",  // SERF_STATE_WAIT_IDLE_ON_PATH
  "WAKE AT FLAG",  // SERF_STATE_WAKE_AT_FLAG
  "WAKE ON PATH",  // SERF_STATE_WAKE_ON_PATH
  "DEFENDING HUT",  // SERF_STATE_DEFENDING_HUT
  "DEFENDING TOWER",  // SERF_STATE_DEFENDING_TOWER
  "DEFENDING FORTRESS",  // SERF_STATE_DEFENDING_FORTRESS
  "SCATTER",  // SERF_STATE_SCATTER
  "FINISHED BUILDING",  // SERF_STATE_FINISHED_BUILDING
  "DEFENDING CASTLE",  // SERF_STATE_DEFENDING_CASTLE
  "KNIGHT ATTACKING DEFEAT FREE",  // SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE
  "WAIT FOR BOAT",  // SERF_STATE_WAIT_FOR_BOAT
  "PASSENGER IN BOAT",  // SERF_STATE_PASSENGER_IN_BOAT
  "EXITING BUILDING TO DEMO",
  "OBSERVING DEMOLITION",
  "CLEANING UP RUBBLE"
};


const char *
Serf::get_state_name(Serf::State state) {
  return serf_state_name[state];
}

static const char *serf_type_name[] = {
  "TRANSPORTER",  // SERF_TRANSPORTER = 0,
  "SAILOR",  // SERF_SAILOR,
  "DIGGER",  // SERF_DIGGER,
  "BUILDER",  // SERF_BUILDER,
  "TRANSPORTER_INVENTORY",  // SERF_TRANSPORTER_INVENTORY,
  "LUMBERJACK",  // SERF_LUMBERJACK,
  "SAWMILLER",  // TypeSawmiller,
  "STONECUTTER",  // TypeStonecutter,
  "FORESTER",  // TypeForester,
  "MINER",  // TypeMiner,
  "SMELTER",  // TypeSmelter,
  "FISHER",  // TypeFisher,
  "PIGFARMER",  // TypePigFarmer,
  "BUTCHER",  // TypeButcher,
  "FARMER",  // TypeFarmer,
  "MILLER",  // TypeMiller,
  "BAKER",  // TypeBaker,
  "BOATBUILDER",  // TypeBoatBuilder,
  "TOOLMAKER",  // TypeToolmaker,
  "WEAPONSMITH",  // TypeWeaponSmith,
  "GEOLOGIST",  // TypeGeologist,
  "GENERIC",  // TypeGeneric,
  "KNIGHT_0",  // TypeKnight0,
  "KNIGHT_1",  // TypeKnight1,
  "KNIGHT_2",  // TypeKnight2,
  "KNIGHT_3",  // TypeKnight3,
  "KNIGHT_4",  // TypeKnight4,
  "DEAD",  // 
};

const char *
Serf::get_type_name(Serf::Type type) {
  return serf_type_name[type];
}

Serf::Serf(Game *game, unsigned int index) : GameObject(game, index) {
  state = StateNull;
  owner = -1;
  type = TypeNone;
  sound = false;
  animation = 0;
  counter = 0;
  pos = -1;
  tick = 0;
  s = { { 0 } };
}

/* Change type of serf and update all global tables
   tracking serf types. */
void
Serf::set_type(Serf::Type new_type) {
  if (new_type == type) {
    return;
  }

  Serf::Type old_type = type;
  type = new_type;

  //Log::Debug["serf"] << "inside set_type, old_type " << old_type << ", new_type " << new_type;

  /* Register this type as transporter */
  if (new_type == TypeTransporterInventory) new_type = TypeTransporter;
  if (old_type == TypeTransporterInventory) old_type = TypeTransporter;

  Player *player = game->get_player(get_owner());
  if (old_type != Serf::TypeNone && old_type != Serf::TypeDead) {
    //Log::Debug["serf"] << "inside set_type, calling decrease_serf_count for old_type " << old_type;
    player->decrease_serf_count(old_type);
  }
  if (type != TypeDead) {
    //Log::Debug["serf"] << "inside set_type, calling increase_serf_count for new_type " << new_type;
    player->increase_serf_count(new_type);
  }

  if (old_type >= TypeKnight0 &&
      old_type <= TypeKnight4) {
    int value = 1 << (old_type - TypeKnight0);
    player->decrease_military_score(value);
  }
  if (new_type >= TypeKnight0 &&
      new_type <= TypeKnight4) {
    int value = 1 << (type - TypeKnight0);
    player->increase_military_score(value);
  }
  if (new_type == TypeTransporter) {
    counter = 0;
  }
}

// added because set_state is actually some kind of macro definition I don't understand
//  and I need this for option_CanTransportSerfsInBoats
//   and probably other things eventually
void
Serf::set_serf_state(Serf::State new_state){
  set_state(new_state);
}

void
Serf::add_to_defending_queue(unsigned int next_knight_index, bool pause) {
  set_state(StateDefendingCastle);
  s.defending.next_knight = next_knight_index;
  if (pause) {
    counter = 6000;
  }
}

void
Serf::init_generic(Inventory *inventory) {
  //Log::Debug["serf"] << "inside init_generic, about to call set_type";
  set_type(TypeGeneric);
  set_owner(inventory->get_owner());
  Building *building = game->get_building(inventory->get_building_index());
  pos = building->get_position();
  tick = game->get_tick();
  state = StateIdleInStock;
  s.idle_in_stock.inv_index = inventory->get_index();
}

void
Serf::init_inventory_transporter(Inventory *inventory) {
  set_state(StateBuildingCastle);
  s.building_castle.inv_index = inventory->get_index();
}

void
Serf::reset_transport(Flag *flag) {
  if (state == StateWalking && s.walking.dest == flag->get_index() &&
      s.walking.dir1 < 0) {
    s.walking.dir1 = -2;
    s.walking.dest = 0;
  } else if (state == StateReadyToLeaveInventory &&
             s.ready_to_leave_inventory.dest == flag->get_index() &&
             s.ready_to_leave_inventory.mode < 0) {
    s.ready_to_leave_inventory.mode = -2;
    s.ready_to_leave_inventory.dest = 0;
  } else if ((state == StateLeavingBuilding || state == StateReadyToLeave) &&
             s.leaving_building.next_state == StateWalking &&
             s.leaving_building.dest == flag->get_index() &&
             s.leaving_building.field_B < 0) {
    s.leaving_building.field_B = -2;
    s.leaving_building.dest = 0;
  } else if (state == StateTransporting &&
             s.walking.dest == flag->get_index()) {
    s.walking.dest = 0;
  } else if (state == StateMoveResourceOut &&
             s.move_resource_out.next_state == StateDropResourceOut &&
             s.move_resource_out.res_dest == flag->get_index()) {
    s.move_resource_out.res_dest = 0;
  } else if (state == StateDropResourceOut &&
             s.move_resource_out.res_dest == flag->get_index()) {
    s.move_resource_out.res_dest = 0;
  } else if (state == StateLeavingBuilding &&
             s.leaving_building.next_state == StateDropResourceOut &&
             s.leaving_building.dest == flag->get_index()) {
    s.leaving_building.dest = 0;
  }
}

bool
Serf::path_splited(unsigned int flag_1, Direction dir_1,
                   unsigned int flag_2, Direction dir_2,
                   int *select) {

  //
  // debug stuck serf issues with StateWaitIdleOnPath
  //Log::Debug["serf.cc"] << "debug StateWaitIdleOnPath issues: inside Serf::path_splited, a serf of type " << get_type() << " at pos " << get_pos() << " is MERGE/SPLIT TAINTED";
  split_merge_tainted = true;
  //  WHY AM I SEEING THIS MESSAGE REPEATED MANY TIMES OVER FOR THE SAME FLAG/SERF FOR THE SAME SPLIT??  is this some kind of bug?
  //   why would path_splited be called repeatedly?  this was at game speed 30 if it matters
  //

  //Log::Debug["serf"] << "inside path_splited for serf with index " << get_index() << " and state " << get_state();
  if (state == StateWalking) {
    //Log::Debug["serf"] << "inside path_splited for serf with index " << get_index() << ", fooA";
    if (s.walking.dest == flag_1 && s.walking.dir1 == dir_1) {
      *select = 0;
      //Log::Debug["serf"] << "inside path_splited for serf with index " << get_index() << ", fooB";
      return true;
    } else if (s.walking.dest == flag_2 && s.walking.dir1 == dir_2) {
      *select = 1;
      //Log::Debug["serf"] << "inside path_splited for serf with index " << get_index() << ", fooC";
      return true;
    }
  } else if (state == StateReadyToLeaveInventory) {
    //Log::Debug["serf"] << "inside path_splited for serf with index " << get_index() << ", fooD";
    if (s.ready_to_leave_inventory.dest == flag_1 &&
        s.ready_to_leave_inventory.mode == dir_1) {
          //Log::Debug["serf"] << "inside path_splited for serf with index " << get_index() << ", fooE";
      *select = 0;
      return true;
    } else if (s.ready_to_leave_inventory.dest == flag_2 &&
               s.ready_to_leave_inventory.mode == dir_2) {
      *select = 1;
      //Log::Debug["serf"] << "inside path_splited for serf with index " << get_index() << ", fooF";
      return true;
    }
  } else if ((state == StateReadyToLeave || state == StateLeavingBuilding) &&
             s.leaving_building.next_state == StateWalking) {
    if (s.leaving_building.dest == flag_1 &&
        s.leaving_building.field_B == dir_1) {
      *select = 0;
      //Log::Debug["serf"] << "inside path_splited for serf with index " << get_index() << ", fooG";
      return true;
    } else if (s.leaving_building.dest == flag_2 &&
               s.leaving_building.field_B == dir_2) {
      *select = 1;
      //Log::Debug["serf"] << "inside path_splited for serf with index " << get_index() << ", fooH";
      return true;
    }
  }
  //Log::Debug["serf"] << "inside path_splited for serf with index " << get_index() << ", fooZ";
  return false;
}

bool
Serf::is_related_to(unsigned int dest, Direction dir) {
  bool result = false;

  switch (state) {
    case StateWalking:
      if (s.walking.dest == dest && s.walking.dir1 == dir) {
        result = true;
      }
      break;
    case StateReadyToLeaveInventory:
      if (s.ready_to_leave_inventory.dest == dest &&
          s.ready_to_leave_inventory.mode == dir) {
        result = true;
      }
      break;
    case StateLeavingBuilding:
    case StateReadyToLeave:
      if (s.leaving_building.dest == dest &&
          s.leaving_building.field_B == dir &&
          s.leaving_building.next_state == StateWalking) {
        result = true;
      }
      break;
    default:
      break;
  }

  return result;
}

void
Serf::path_deleted(unsigned int dest, Direction dir) {
  switch (state) {
    case StateWalking:
      if (s.walking.dest == dest && s.walking.dir1 == dir) {
        s.walking.dir1 = -2;
        s.walking.dest = 0;
      }
      break;
    case StateReadyToLeaveInventory:
      if (s.ready_to_leave_inventory.dest == dest &&
          s.ready_to_leave_inventory.mode == dir) {
        s.ready_to_leave_inventory.mode = -2;
        s.ready_to_leave_inventory.dest = 0;
      }
      break;
    case StateLeavingBuilding:  
    case StateReadyToLeave:
      if (s.leaving_building.dest == dest &&
          s.leaving_building.field_B == dir &&
          s.leaving_building.next_state == StateWalking) {
        s.leaving_building.field_B = -2;
        s.leaving_building.dest = 0;
      }
      break;
    default:
      break;
  }
}

void
Serf::path_merged(Flag *flag) {
  if (state == StateReadyToLeaveInventory &&
      s.ready_to_leave_inventory.dest == flag->get_index()) {
    s.ready_to_leave_inventory.dest = 0;
    s.ready_to_leave_inventory.mode = -2;
  } else if (state == StateWalking && s.walking.dest == flag->get_index()) {
    s.walking.dest = 0;
    s.walking.dir1 = -2;
  } else if (state == StateIdleInStock && 1/*...*/) {
    /* TODO */
  } else if ((state == StateLeavingBuilding || state == StateReadyToLeave) &&
           s.leaving_building.dest == flag->get_index() &&
           s.leaving_building.next_state == StateWalking) {
    s.leaving_building.dest = 0;
    s.leaving_building.field_B = -2;
  }
}

void
Serf::path_merged2(unsigned int flag_1, Direction dir_1,
                   unsigned int flag_2, Direction dir_2) {

  //
  // debug stuck serf issues with StateWaitIdleOnPath
  //Log::Debug["serf.cc"] << "debugging StateWaitIdleOnPath issues: inside Serf::path_merged2, a serf of type " << get_type() << " at pos " << get_pos() << " is MERGE/SPLIT TAINTED";
  split_merge_tainted = true;
  //
  //
  if (state == StateReadyToLeaveInventory &&
      ((s.ready_to_leave_inventory.dest == flag_1 &&
        s.ready_to_leave_inventory.mode == dir_1) ||
       (s.ready_to_leave_inventory.dest == flag_2 &&
        s.ready_to_leave_inventory.mode == dir_2))) {
    s.ready_to_leave_inventory.dest = 0;
    s.ready_to_leave_inventory.mode = -2;
  } else if (state == StateWalking &&
             ((s.walking.dest == flag_1 && s.walking.dir1 == dir_1) ||
              (s.walking.dest == flag_2 && s.walking.dir1 == dir_2))) {
    s.walking.dest = 0;
    s.walking.dir1 = -2;
  } else if (state == StateIdleInStock) {
    /* TODO */
  } else if ((state == StateLeavingBuilding || state == StateReadyToLeave) &&
             ((s.leaving_building.dest == flag_1 &&
               s.leaving_building.field_B == dir_1) ||
              (s.leaving_building.dest == flag_2 &&
               s.leaving_building.field_B == dir_2)) &&
             s.leaving_building.next_state == StateWalking) {
    s.leaving_building.dest = 0;
    s.leaving_building.field_B = -2;
  }
}

void
Serf::flag_deleted(MapPos flag_pos) {
  switch (state) {
    case StateReadyToLeave:
    case StateLeavingBuilding:
      s.leaving_building.next_state = StateLost;
      break;
    case StateFinishedBuilding:
    case StateWalking:
      if (game->get_map()->paths(flag_pos) == 0) {

        ////debug
        //if (game->get_flag(recent_dest) != nullptr){
        //  Log::Debug["serf"] << "inside flag_deleted, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which has pos " << game->get_flag(recent_dest)->get_position();
        //}else{
        //  Log::Debug["serf"] << "inside flag_deleted, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which is a nullptr";
        //}
        Log::Debug["serf"] << "inside flag_deleted, serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
        set_state(StateLost);
      }
      break;
    default:
      break;
  }
}

// when a Castle or Stock is destroyed
//  up to 12 serfs are allowed to escape
//  the rest are killed
bool
Serf::castle_deleted(MapPos castle_pos, bool escape) {
  // I'm not sure what other state serfs in a castle could have, maybe this is to avoid
  //  breaking serfs that are in the out-queue or something like that?
  if (pos == castle_pos && (state == StateIdleInStock || state == StateReadyToLeaveInventory)) {
    if (escape) {
      /* Serf is escaping. */
      // he will walk to his building flag and then go back to an Inv
      state = StateEscapeBuilding;
    } else {
      /* Kill this serf. */
      set_type(TypeDead);
      game->delete_serf(this);
    }
    return true;
  }
  return false;
}

// removing the 'bool transporter' arg as I think it confuses things
//  and the only time it is needed is for Stock/Castle destruction
//  and that is already handled prior to calling this function (inside Building::burnup)
void
//Serf::building_deleted(MapPos building_pos, bool transporter) {
Serf::building_deleted(MapPos building_pos) {
  Log::Debug["serf.cc"] << "inside Serf::building_deleted, pos " << building_pos << ", serf with index #" << get_index();
  // I think this sets holder of a destroyed Stock or Castle from 
  //  TypeTransporterInventory back to a normal Transporter serf
  //  because TypeTransporterInventory isn't a "real" serf occupation
  // UPDATE - it seems the code that calls building_deleted already checks for
  //  TypeTransporterInventory and is less convolute than this, simply removing this check
  //  which was part of Freeserf code
  //if ((!transporter || (get_type() == TypeTransporterInventory)) && pos == building_pos) {
  //  if (transporter) {
  //    set_type(TypeTransporter);
  //  }
  //}

  Building *deleted_building = game->get_building_at_pos(this->get_pos());
  if (deleted_building == nullptr){
    Log::Debug["serf.cc"] << "inside Serf::building_deleted, pos " << building_pos << ", serf with index #" << get_index() << " and type " << NameSerf[this->get_type()] << ".  building at serf pos is nullptr!";
  }else{
    Log::Debug["serf.cc"] << "inside Serf::building_deleted, pos " << building_pos << ", serf with index #" << get_index() << " and type " << NameSerf[this->get_type()] << ".  building at serf pos has index #" << deleted_building->get_index();
  }
  Log::Debug["serf.cc"] << "inside Serf::building_deleted, pos " << building_pos << ", serf with index #" << get_index() << " and type " << NameSerf[this->get_type()] << ".  setting serf->building_held to 0";
  Log::Debug["serf.cc"] << "inside Serf::building_deleted, pos " << building_pos << ", serf with index #" << get_index() << " and type " << NameSerf[this->get_type()] << ".  current building_held if any is #" << this->get_building_held();
  this->set_building_held(0);
  Log::Debug["serf.cc"] << "inside Serf::building_deleted, pos " << building_pos << ", serf with index #" << get_index() << " and type " << NameSerf[this->get_type()] << ".  current building_held if any is #" << this->get_building_held() << " THIS BETTER BE ZERO!!!";

  counter = 0;
  // if serf Holder is out working somewhere...
  if (game->get_map()->get_serf_index(pos) == index) {
    Log::Debug["serf.cc"] << "inside Serf::building_deleted, pos " << building_pos << ", serf with index #" << get_index() << ".  Holder is not home, making him lost wherever he is";
    // ...set him to Lost, he will return to his building flag and then go back to an Inv
    set_state(StateLost);
    s.lost.field_B = 0;
  } else {
    // ...otherwise have him leave building, he will walk to his building flag and then go back to an Inv
    Log::Debug["serf.cc"] << "inside Serf::building_deleted, pos " << building_pos << ", serf with index #" << get_index() << ".  Occupant being set to StateEscapingbuilding";
    set_state(StateEscapeBuilding);
  }
}

// this function appears to ONLY be used for road/splitting, and not normal transporter activity
bool
Serf::change_transporter_state_at_pos(MapPos pos_, Serf::State _state) {
  if (pos == pos_ &&
      //!!!! is this the cause of the WaitIdleOnRoad bug?   !!!!!
      // it seems silly that this function would check if the ARGUMENT is a valid state
      //  before assigning it, but it WOULD make sense if it is meant to be checking the
      //  serf's CURRENT STATE to see if it is a valid state to change
      // trying this...
      //(_state == StateWakeAtFlag || _state == StateWakeOnPath ||
      // _state == StateWaitIdleOnPath || _state == StateIdleOnPath)) {
      (get_state() == StateWakeAtFlag || get_state() == StateWakeOnPath ||
       get_state() == StateWaitIdleOnPath || get_state() == StateIdleOnPath)) {
    set_state(_state);
    return true;
  }
  return false;
}

void
Serf::restore_path_serf_info() {
  if (state != StateWakeOnPath) {
    s.transporting.wait_counter = -1;
    if (s.transporting.res != Resource::TypeNone) {
      Resource::Type res = s.transporting.res;
      s.transporting.res = Resource::TypeNone;

      game->cancel_transported_resource(res, s.transporting.dest);
      game->lose_resource(res);
    }
  } else {
    set_state(StateWakeAtFlag);
  }
}

void
Serf::clear_destination(unsigned int dest) {
  switch (state) {
    case StateWalking:
      if (s.walking.dest == dest && s.walking.dir1 < 0) {
        s.walking.dir1 = -2;
        s.walking.dest = 0;
      }
      break;
    case StateReadyToLeaveInventory:
      if (s.ready_to_leave_inventory.dest == dest &&
          s.ready_to_leave_inventory.mode < 0) {
        s.ready_to_leave_inventory.mode = -2;
        s.ready_to_leave_inventory.dest = 0;
      }
      break;
    case StateLeavingBuilding:
    case StateReadyToLeave:
      if (s.leaving_building.dest == dest &&
          s.leaving_building.field_B < 0 &&
          s.leaving_building.next_state == StateWalking) {
        s.leaving_building.field_B = -2;
        s.leaving_building.dest = 0;
      }
      break;
    default:
      break;
  }
}

void
Serf::clear_destination2(unsigned int dest) {
  switch (state) {
    case StateTransporting:
      if (s.walking.dest == dest) {
        s.walking.dest = 0;
      }
      break;
    case StateDropResourceOut:
      if (s.move_resource_out.res_dest == dest) {
        s.move_resource_out.res_dest = 0;
      }
      break;
    case StateLeavingBuilding:
      if (s.leaving_building.dest == dest &&
          s.leaving_building.next_state == StateDropResourceOut) {
        s.leaving_building.dest = 0;
      }
      break;
    case StateMoveResourceOut:
      if (s.move_resource_out.res_dest == dest &&
          s.move_resource_out.next_state == StateDropResourceOut) {
        s.move_resource_out.res_dest = 0;
      }
      break;
    default:
      break;
  }
}

bool
Serf::idle_to_wait_state(MapPos pos_) {
  if (pos == pos_ &&
      (get_state() == StateIdleOnPath || get_state() == StateWaitIdleOnPath ||
       get_state() == StateWakeAtFlag || get_state() == StateWakeOnPath)) {
    set_state(StateWakeAtFlag);
    return true;
  }
  return false;
}

int
Serf::get_delivery() const {
  int res = 0;

  switch (state) {
    case StateDelivering:
    case StateTransporting:
      res = s.transporting.res + 1;
      break;
    case StateEnteringBuilding:
      res = s.entering_building.field_B;
      break;
    case StateLeavingBuilding:
      res = s.leaving_building.field_B;
      break;
    case StateReadyToEnter:
      res = s.ready_to_enter.field_B;
      break;
    case StateMoveResourceOut:
    case StateDropResourceOut:
      res = s.move_resource_out.res;
      break;

    default:
      break;
  }

  return res;
}

// used only by Building::call_attacker_out
//          and Building::call_defender_out
Serf*
Serf::extract_last_knight_from_list() {
  int serf_index = index;
  int *def_index = &serf_index;
  Serf *def_serf = game->get_serf(*def_index);
  while (def_serf->s.defending.next_knight != 0) {
    def_index = &def_serf->s.defending.next_knight;
    def_serf = game->get_serf(*def_index);
  }
  *def_index = 0;

  return def_serf;
}

// used only by Building::knight_come_back_from_fight
void
Serf::insert_before(Serf *knight) {
  s.defending.next_knight = knight->get_index();
}

// this might not actually be needed, added this logic to Player::available_knights_at_pos
//  keeping to use for other FreeWalking types

// work in progress, adding to handle
//  cases where a serf cannot reach target within reasonable path
//   such as knights attacking across river
bool
Serf::can_reach_pos(MapPos dest_pos, int max_dist){
  //Log::Debug["serf.cc"] << "inside Serf::can_reach_pos, a serf of type " << get_type() << " at pos " << get_pos() << " is considering going to dest pos " << dest_pos << ", max_dist " << max_dist;
  // Road road = pathfinder_map(map.get(), pos, clk_pos,
  // &interface->get_building_road());

  // it is common for the serf to be in a building, if so use its flag
  MapPos start_pos = get_pos();  // this is serf's pos on map
  if (game->get_map()->has_building(start_pos)){
    start_pos = game->get_map()->move_down_right(get_pos());
    //Log::Debug["serf.cc"] << "inside Serf::can_reach_pos, a serf of type " << get_type() << " at pos " << get_pos() << " is in a building, using its flag pos " << start_pos << " as start pos";
  }

  //Road junk_road = pathfinder_freewalking_serf(game->get_map().get(), get_pos(), dest);
  Road junk_road = pathfinder_freewalking_serf(game->get_map().get(), start_pos, dest_pos, 20);

  if (junk_road.get_length() > 0){
    //Log::Debug["serf.cc"] << "inside Serf::can_reach_pos, a serf of type " << get_type() << " found freewalking solution to dest pos " << dest_pos;
    //game->set_debug_mark_road(junk_road);
    return true;
  }else{
    //Log::Debug["serf.cc"] << "inside Serf::can_reach_pos, a serf of type " << get_type() << " cannot reach dest pos " << dest_pos << " in less than max_dist " << max_dist << " tiles";
    return false;
  }
}

// this says MapPos dest but I think it is actually a flag index!
void
Serf::go_out_from_inventory(unsigned int inventory, MapPos dest, int mode) {
  //Log::Debug["serf.cc"] << "inside Serf::go_out_from_inventory, a serf of type " << get_type() << " is being sent to dest pos " << pos;
  set_state(StateReadyToLeaveInventory);
  // 'mode' seems to be simply the initial Dir that the serf goes as it exists the Inventory Flag, or <1 for special cases maybe?  Such as flagsearch not finding a dest??
  //  this is rabbit hole, not sure I understand it and don't feel like figuring it out 
  s.ready_to_leave_inventory.mode = mode; 
  // this appears to be the index of the destination Flag, which is what you would expect, but the Declaration of this function 
  //  says it is a MapPos, which I think is wrong. Further confirmation that this is really a Flag index is that the 
  //  handle_serf_ready_to_leave_inventory_state function does a flag lookup by index on this value!
  s.ready_to_leave_inventory.dest = dest; 
  // DEBUGGING - store the destination of the serf in case it becomes Lost!  to help mitigate
  recent_dest = dest;
  s.ready_to_leave_inventory.inv_index = inventory;
}

void
Serf::send_off_to_fight(int dist_col, int dist_row) {
  /* Send this serf off to fight. */
  set_state(StateKnightLeaveForWalkToFight);
  s.leave_for_walk_to_fight.dist_col = dist_col;
  s.leave_for_walk_to_fight.dist_row = dist_row;
  s.leave_for_walk_to_fight.field_D = 0;
  s.leave_for_walk_to_fight.field_E = 0;
  s.leave_for_walk_to_fight.next_state = StateKnightFreeWalking;
}

void
Serf::stay_idle_in_stock(unsigned int inventory) {
  Log::Debug["serf.cc"] << "inside Serf::stay_idle_in_stock";
  set_state(StateIdleInStock);
  s.idle_in_stock.inv_index = inventory;
}

void
Serf::go_out_from_building(MapPos dest, int dir, int field_B) {
  set_state(StateReadyToLeave);
  s.leaving_building.field_B = field_B;
  s.leaving_building.dest = dest;
  s.leaving_building.dir = dir;
  s.leaving_building.next_state = StateWalking;
}

/* Change serf state to lost, but make necessary clean up
   from any earlier state first. */
void
Serf::set_lost_state() {
  if (state == StateWalking) {
    if (s.walking.dir1 >= 0) {
      if (s.walking.dir1 != 6) {
        Direction dir = (Direction)s.walking.dir1;
        Flag *flag = game->get_flag(s.walking.dest);
        flag->cancel_serf_request(dir);

        Direction other_dir = flag->get_other_end_dir(dir);
        flag->get_other_end_flag(dir)->cancel_serf_request(other_dir);
      }
    } else if (s.walking.dir1 == -1) {
      Flag *flag = game->get_flag(s.walking.dest);
      Building *building = flag->get_building();
      building->requested_serf_lost();
    }

    //debug
    ///if (game->get_flag(recent_dest) != nullptr){
    //  Log::Debug["serf"] << "inside set_lost_state A, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which has pos " << game->get_flag(recent_dest)->get_position();
    //}else{
    //  Log::Debug["serf"] << "inside set_lost_state A, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which is a nullptr";
    //}
    Log::Debug["serf"] << "inside set_lost_state A, serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
    set_state(StateLost);
    s.lost.field_B = 0;
  } else if (state == StateTransporting || state == StateDelivering) {
    if (s.transporting.res != Resource::TypeNone) {
      Resource::Type res = s.transporting.res;
      int dest = s.walking.dest;

      game->cancel_transported_resource(res, dest);
      game->lose_resource(res);
    }

    if (get_type() != TypeSailor) {

      //debug
      //if (game->get_flag(recent_dest) != nullptr){
      //  Log::Debug["serf"] << "inside set_lost_state, B serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which has pos " << game->get_flag(recent_dest)->get_position();
      //}else{
      //  Log::Debug["serf"] << "inside set_lost_state, B serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which is a nullptr";
      //}

      Log::Debug["serf"] << "inside set_lost_state B, serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
      set_state(StateLost);
      s.lost.field_B = 0;
    } else {
      set_state(StateLostSailor);
    }
  } else {

    //debug
    //if (game->get_flag(recent_dest) != nullptr){
    //  Log::Debug["serf"] << "inside set_lost_state C, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which has pos " << game->get_flag(recent_dest)->get_position();
    //}else{
    //  Log::Debug["serf"] << "inside set_lost_state C, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which is a nullptr";
    //}
    Log::Debug["serf"] << "inside set_lost_state C, serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
    // check to see if this serf is Holder to some building, and if free it
    if (this->get_building_held() != 0){
      Log::Debug["serf"] << "inside set_lost_state C, serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost, this serf is holder to building #" << this->get_building_held() << ", trying to free it";
      Building *building = game->get_building(this->get_building_held());
      if (building == nullptr){
        Log::Debug["serf"] << "inside set_lost_state C, serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost, this serf is holder to building #" << this->get_building_held() << " but the building is nullptr, nothing to do";
      }else{
        Log::Debug["serf"] << "inside set_lost_state C, serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost, this serf is holder to building #" << this->get_building_held() << " calling building->requested_serf_lost();";
        building->requested_serf_lost();
        Log::Debug["serf"] << "inside set_lost_state C, serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost, this serf is holder to building #" << this->get_building_held() << " calling building->clear_holder();";
        building->clear_holder();
      }
    }else{
      Log::Debug["serf"] << "inside set_lost_state C, serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost, this serf is not holder to any building";
    }
    set_state(StateLost);
    s.lost.field_B = 0;
  }
}

/* Return true if serf is waiting for a position to be available.
   In this case, dir will be set to the desired direction of the serf,
   or DirectionNone if the desired direction cannot be determined. */
bool
Serf::is_waiting(Direction *dir) {
  const Direction dir_from_offset[] = {
    DirectionUpLeft, DirectionUp,   DirectionNone,
    DirectionLeft,   DirectionNone, DirectionRight,
    DirectionNone,   DirectionDown, DirectionDownRight
  };

  if ((state == StateTransporting || state == StateWalking ||
       state == StateDelivering) &&
      s.walking.dir < 0) {
    *dir = (Direction)(s.walking.dir + 6);
    return true;
  } else if ((state == StateFreeWalking ||
              state == StateKnightFreeWalking ||
              state == StateStoneCutterFreeWalking) &&
             animation == 82) {
    int dx = s.free_walking.dist_col;
    int dy = s.free_walking.dist_row;

    if (abs(dx) <= 1 && abs(dy) <= 1 &&
        dir_from_offset[(dx+1) + 3*(dy+1)] > DirectionNone) {
      *dir = dir_from_offset[(dx+1) + 3*(dy+1)];
    } else {
      *dir = DirectionNone;
    }
    return true;
  } else if (state == StateDigging && s.digging.substate < 0) {
    int d = s.digging.dig_pos;
    *dir = (Direction)((d == 0) ? DirectionUp : 6-d);
    return true;
  }

  return false;
}

/* Signal waiting serf that it is possible to move in direction
   while switching position with another serf. Returns 0 if the
   switch is not acceptable. */
int
Serf::switch_waiting(Direction dir) {
  if ((state == StateTransporting || state == StateWalking ||
       state == StateDelivering) &&
      s.walking.dir < 0) {
    s.walking.dir = reverse_direction(dir);
    return 1;
  } else if ((state == StateFreeWalking ||
              state == StateKnightFreeWalking ||
              state == StateStoneCutterFreeWalking) &&
             animation == 82) {
    int dx = ((dir < 3) ? 1 : -1)*((dir % 3) < 2);
    int dy = ((dir < 3) ? 1 : -1)*((dir % 3) > 0);

    s.free_walking.dist_col -= dx;
    s.free_walking.dist_row -= dy;

    if (s.free_walking.dist_col == 0 && s.free_walking.dist_row == 0) {
      /* Arriving to destination */
      s.free_walking.flags = BIT(3);
    }
    return 1;
  } else if (state == StateDigging && s.digging.substate < 0) {
    return 0;
  }

  return 0;
}

int
Serf::train_knight(int p) {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    if (game->random_int() < p) {
      /* Level up */
      Serf::Type old_type = get_type();
      //Log::Debug["serf"] << "inside train_knight, about to call set_type";
      set_type((Serf::Type)(old_type + 1));
      counter = 6000;
      return 0;
    }
    counter += 6000;
  }

  return -1;
}

void
Serf::handle_serf_idle_in_stock_state() {
  Inventory *inventory = game->get_inventory(s.idle_in_stock.inv_index);

  if (inventory->get_serf_mode() == 0
      || inventory->get_serf_mode() == 1 /* in, stop */
      || inventory->get_serf_queue_length() >= 3) {
    switch (get_type()) {
      case TypeKnight0:
        inventory->knight_training(this, 4000);
        break;
      case TypeKnight1:
        inventory->knight_training(this, 2000);
        break;
      case TypeKnight2:
        inventory->knight_training(this, 1000);
        break;
      case TypeKnight3:
        inventory->knight_training(this, 500);
        break;
      //case TypeSmelter: /* TODO ??? */    // I think this was a bug in the original game code, it results in idle Smelter's not being dispatched from an Inventory
      //  break;
      default:
        inventory->serf_idle_in_stock(this);
        break;
    }
  } else { /* out */
    inventory->call_out_serf(this);

    set_state(StateReadyToLeaveInventory);
    s.ready_to_leave_inventory.mode = -3;
    s.ready_to_leave_inventory.inv_index = inventory->get_index();
    /* TODO immediate switch to next state. */
  }
}

int
Serf::get_walking_animation(int h_diff, Direction dir, int switch_pos) {
  int d = dir;
  if (switch_pos && d < 3) d += 6;
  return 4 + h_diff + 9*d;
}

/* Preconditon: serf is in WALKING or TRANSPORTING state */
void
Serf::change_direction(Direction dir, int alt_end) {
  //Log::Info["serf"] << "debug: a serf with index " << get_index() << " inside change_direction, starting pos " << pos << ", dir: " << NameDirection[dir];
  PMap map = game->get_map();
  MapPos new_pos = map->move(pos, dir);

  //
  // adding support for option_CanTransportSerfsInBoats
  //

  // handle sailor carrying a passenger in his boat
  if (type == Serf::TypeSailor && state == State::StateTransporting
    && s.transporting.res == Resource::TypeSerf){
     //Log::Info["serf"] << "debug: a transporting sailor inside change_direction, already has a passenger in his boat";
      Serf *passenger = game->get_serf(s.transporting.passenger_serf_index);
      if (passenger == nullptr){
        Log::Warn["serf"] << "got nullptr when fetching boat passenger serf with index " << s.transporting.passenger_serf_index << "!";
      }else{
        passenger->animation = get_walking_animation(map->get_height(new_pos) -
                                      map->get_height(pos), (Direction)dir,
                                      0);
        // set the passenger's walking (facing) direction to match the sailor so its animation looks right
        passenger->s.walking.dir = reverse_direction(dir);
        // normally this would be handled in handle_serf_passenger_in_boat but that is never called because
        ///  boat passengers are not attached to any map position
      }
  }

  // if *this* serf is a sailor (who is already in transporting state and so must be in his boat)...
  //  and if another serf is at the next pos in StateWaitForBoat AND is waiting in the direction of this serf
  //   continue "into" the waiting serf to pick him up
  //
  bool sailor_pickup_serf = false;
  /*
  // debugging
  if(type == Serf::TypeSailor && state == State::StateTransporting){
    Log::Info["serf"] << "debug: a transporting sailor inside change_direction, heading in dir " << NameDirection[dir];
    if (game->get_serf_at_pos(new_pos)->get_state() == Serf::StateWaitForBoat){
      Log::Info["serf"] << "debug: a transporting sailor inside change_direction, there is a serf in wait_for_boat state in next pos with dir " << NameDirection[game->get_serf_at_pos(new_pos)->get_walking_dir()];
      Log::Info["serf"] << "debug: a transporting sailor inside change_direction, comparing sailor reversed-dir " << NameDirection[reverse_direction(Direction(dir))] << " to boat-wait serf dir " << NameDirection[game->get_serf_at_pos(new_pos)->get_walking_dir()];
    }
  }
  */

  if (type == Serf::TypeSailor && state == State::StateTransporting
     && game->get_serf_at_pos(new_pos)->get_state() == Serf::StateWaitForBoat
     && game->get_serf_at_pos(new_pos)->get_walking_dir() == reverse_direction(Direction(dir))){
       Log::Debug["serf"] << "a transporting sailor inside change_direction, next pos " << new_pos << ", setting sailor_pickup_serf to TRUE";
       sailor_pickup_serf = true;
  }

  // handle serf entering WaitForBoat state because it's change_direction dir is into a waiting sailor's water path
  // if next pos is in water and has no blocking serf, and current pos is NOT in water... (or else it triggers for active sailors)
  // WARNING - is_water_path can only be uses to check a path that is certain to exist, it will return true if there is no path at all!
  if (option_CanTransportSerfsInBoats && map->is_in_water(new_pos) && !map->has_serf(new_pos) && !map->is_in_water(pos)){
    // this water path must have a sailor or else the search callback should not have given 
    //  this as a valid dir, but do some sanity checks anyway
    Flag *water_flag = game->get_flag_at_pos(pos);
    if (water_flag == nullptr){
      Log::Warn["serf"] << "expecting a flag (at water's edge) at pos " << pos << " (which should be next to new_pos " << new_pos << "), but no flag was found!";
    } else {
      // WARNING - is_water_path can only be uses to check a path that is certain to exist, it will return true if there is no path at all!
      if (!(water_flag->has_path(dir) && water_flag->is_water_path(dir))){
        throw ExceptionFreeserf("expecting flag to have water path!  to direct serf to WaitForBoat state");
      }

      // NOTE: if this serf IS a sailor, but the the water path has no transporter (sailor),
      //   then this sailor likely *is* that path's transporter
      //    though I guess it is possible that sailor is actually en route to another water path, but that seems unlikely
      //     and it all might still work anyway
      // AH, it should be possible to check if call_transporter stuff was done for this water path to determine
      //  if this is the sailor meant for the path??  figure that out later

      // if this flag has a transporter (sailor) 
      //  and this serf's dir *prior to change_direction* was NOT from the water path...
      //***this prevents a sailor who is picking up a WaitForBoat serf from becoming a WaitForBoat serf himself!***
      //
      // jan03 2021, saw this happen anyway, a sailor became stuck in WaitForBoat state on his own path after dropping some res or serf off
      // same day just saw it again, it seems to happen when a land transporter serf approaches the flag at the same time as the sailor
      //  ?? maybe the issue is that the pos->new_pos switch hasn't been done yet and the get_serf_at_pos(pos) check is returning the wrong
      //      serf?  try changing it to just be this serf's dir
      // NOTE - it should be possible to avoid this by disallowing sailors from ever becoming boat passengers, 
      //         but I am still not sure I want that
      //       YES THIS SEEMS LIKE A GOOD FIX, DOING IT
      // I think this may be fixed now
      //Log::Info["serf"] << "debug: inside Serf::change_direction, water_flag->has_transporter(" << NameDirection[dir] << ") == " << water_flag->has_transporter(dir);
      //Log::Info["serf"] << "debug: inside Serf::change_direction, game->get_serf_at_pos(" << pos << ")->get_walking_dir() = " << NameDirection[game->get_serf_at_pos(pos)->get_walking_dir()] << ", compare to reverse-dir " << NameDirection[reverse_direction(dir)];
      // ugh, because serf->get_walking_dir seems to always be backwards, we should actually reject the pickup
      //  if get_walking_dir == dir, which is equivalent to "corrected" reverse_dir[get_walking_dir] != reverse_dir(dir)
      // trying workaround for above issue...
      //if (water_flag->has_transporter(dir) && game->get_serf_at_pos(pos)->get_walking_dir() != dir){
      //Log::Info["serf"] << "debugX: THIS serf_type " << get_type() << ", s.walking.dir " << s.walking.dir << ", get_walking_dir " << get_walking_dir();
      //Log::Info["serf"] << "debugY: game->get_serf_at_pos serf_type " << game->get_serf_at_pos(pos)->get_type() << ", s.walking.dir " << game->get_serf_at_pos(pos)->s.walking.dir << ", get_walking_dir " << game->get_serf_at_pos(pos)->get_walking_dir();
      
      /*
      // debug
      Log::Info["serf"] << "debug: inside Serf::change_direction, s.walking.dir is " << s.walking.dir;
      for (Direction d : cycle_directions_ccw()) {
        Log::Info["serf"] << "debug: inside Serf::change_direction, flag at pos " << pos << " dir " << d << " has_transporter bool is " << water_flag->has_transporter(d);
      }
      */

      //if (water_flag->has_transporter(dir) && s.walking.dir != dir){
      // bugfix:
      //  do NOT check serf walking dir, it only works when the serf is already walking on land in the same dir
      //  as the water path continues, often the serf will need to change direction at the flag, but must still
      //  be eligible to become a boat passenger or else the "no sailor transporter in dir" exception will trigger
      if (water_flag->has_transporter(dir)){
        if (get_type() == Serf::TypeSailor){
          // this IS the transporting sailor about to pick up passenger, do NOT set this sailor to WaitForBoat state
          Log::Debug["serf"] << "inside Serf::change_direction, a sailor has has arrived at pos " << pos << ", a water path that already has_transporter in this dir, assuming this is the sailor working the path and is picking up a passenger";
        }else{
          Log::Debug["serf"] << "inside Serf::change_direction, setting passenger-to-be at pos " << pos << " into WaitForBoat state";
          set_state(Serf::StateWaitForBoat);
          Log::Debug["serf"] << "inside Serf::change_direction, setting flag at pos " << pos << " to has_serf_waiting_for_boat";
          water_flag->set_serf_waiting_for_boat();
          // still need to actually set the serf walking direction to the dir of the water path/boat it is waiting for
          s.walking.dir = dir;
          return;
        }
      }
      if (get_type() == Serf::TypeSailor){
        // this sailor is probably the new transporter for this water path
        //  let him go on, and do not put him in WaitForBoat state
      }else{
        Log::Error["serf"] << "serf of type " << NameSerf[get_type()] << " at pos " << pos << " with state " << get_state() << " expecting flag with water path to have sailor! but does not, crashing";
        throw ExceptionFreeserf("expecting flag with water path to have a (sailor) transporter!  to direct serf to WaitForBoat state");
        //Log::Error["serf"] << "marking pos " << pos << " in magenta and pausing game";
        //game->set_debug_mark_pos(pos, "magenta");
        //game->pause();
      }
    }
  }

  if (!map->has_serf(new_pos) || sailor_pickup_serf){

    /* Change direction (i.e. continue), not occupied. */

    //if (type == Serf::TypeSailor && state == State::StateTransporting){
     //Log::Info["serf"] << "debug: a transporting sailor inside change_direction, next pos " << new_pos << " does NOT have a serf blocking";
    //}

    if (sailor_pickup_serf){
     //Log::Info["serf"] << "debug: transporting sailor_pickup_serf is TRUE";
      Serf *pickup_serf = game->get_serf_at_pos(new_pos);
      if (pickup_serf == nullptr){
        Log::Warn["serf"] << "got nullptr for passenger_serf during sailor boat pickup at pos " << pos << " and new_pos " << new_pos << "!";
      }else{
       //Log::Info["serf"] << "debug: transporting sailor_pickup_serf is TRUE, storing passenger_serf info";
        // store passenger info for in-boat animation and for restoring passenger serf after dropoff 
        s.transporting.pickup_serf_index = pickup_serf->get_index();
        s.transporting.pickup_serf_type = pickup_serf->get_type();
      }
    }
    //if (type == Serf::TypeSailor && state == State::StateTransporting){
      // this crashes if s.walking.dir out of range
      //Log::Info["serf"] << "debug: a transporting sailor inside change_direction, about to change s.walking.dir from " << NameDirection[s.walking.dir];
     //Log::Info["serf"] << "debug: a transporting sailor inside change_direction, about to change s.walking.dir from " << s.walking.dir;
    //}

    // this empties the current pos, this serf no longer exists on the map until the MapPos pos = new_pos
    //  assignment is made at the end of this function!
    map->set_serf_index(pos, 0);
    animation = get_walking_animation(map->get_height(new_pos) -
                                      map->get_height(pos), (Direction)dir,
                                      0);
    s.walking.dir = reverse_direction(dir);

    // handle sailor dropping a passenger serf off at a flag
    if (type == Serf::TypeSailor && state == State::StateTransporting
      && s.transporting.dropped_serf_type > Type::TypeNone && s.transporting.dropped_serf_index > 0){
        if (!map->has_flag(pos)){
          throw ExceptionFreeserf("sailor dropping serf at at flag but map thinks there is no flag at this pos!");
        }
        //Log::Info["serf"] << "debug: a transporting sailor inside change_direction, preparing to drop off a passenger serf";
        // is s.transporting.xxxxx_serf_type even needed?  could just use serf->get_type() if the type is even ever needed?
        //  I think only viewport uses it but even that already looks all serf types up by *Serf pointer
        Serf *dropped_serf = game->get_serf(s.transporting.dropped_serf_index);
        if (dropped_serf == nullptr){
          Log::Warn["serf"] << "sailor dropped a serf off at flag at pos " << pos << " but got nullptr for the serf!  serf index: " << s.transporting.dropped_serf_index << " serf type: " << s.transporting.dropped_serf_type;
        } else {
          map->set_serf_index(pos, s.transporting.dropped_serf_index);
          //Log::Info["serf"] << "sailor just dropped a passenger serf off at pos " << pos;
          // is StateWalking a good default to use here?
          dropped_serf->set_serf_state(StateWalking);
          // serf->pos is not derived from map, it is also stored separately in Serf* object
          //  need to set it
          dropped_serf->pos = pos;
          // set the dropped serf's walking dir to face away from the water path (i.e. the dir the sailor was facing before he turns back)
          dropped_serf->s.walking.dir = dir;
          // update the dropped serf's stored tick to current game tick or its next animation will cut to its end
          dropped_serf->tick = game->get_tick();
        }

        // clear the sailor dropped_serf vars
        s.transporting.dropped_serf_type = Type::TypeNone;
        s.transporting.dropped_serf_index = 0;

        // the dropped off passenger serf should now fully exist on the map at the flag at 
        //  the end of the water path and resume going to his original destination
        // the sailor himself currently does NOT exist on the map, but will at the end of
        //  this function once MapPos pos = new_pos is done;
    }// end pickup boat passengers 

    //if (type == Serf::TypeSailor && state == State::StateTransporting){
     //Log::Info["serf"] << "debug: a transporting sailor inside change_direction, just set s.walking.dir = reverse_direction(dir)" << ", was " << NameDirection[dir] << ", now is " << NameDirection[reverse_direction(dir)];
    //}
  } else {
    //if (type == Serf::TypeSailor && state == State::StateTransporting){
     //Log::Info["serf"] << "debug: a transporting sailor inside change_direction, next pos " << new_pos << " DOES have a serf blocking";
    //}
    /* Direction is occupied. */
    Serf *other_serf = game->get_serf_at_pos(new_pos);
    Direction other_dir;

    if (other_serf->is_waiting(&other_dir) &&
        (other_dir == reverse_direction(dir) || other_dir == DirectionNone) &&
        other_serf->switch_waiting(reverse_direction(dir))) {
        //if (type == Serf::TypeSailor && state == State::StateTransporting){
         //Log::Info["serf"] << "debug: a transporting sailor inside change_direction, serf at next pos is eligible for switching";
        //}
      // trying to fix issue where land serfs are "switching" with sailor rather than entering
      //  WaitForBoat state
      //    note, do not use is_water_tile because it returns true if any adjacent tile is water!
      //    but is_in_water only returns true if pos is entirely in water
      if (map->has_path_IMPROVED(pos, dir) && map->is_in_water(new_pos)){
        Log::Info["serf"] << "debug: NOT ALLOWING serf switch because path from pos " << pos << " in dir " << NameDirection[dir] << " to new_pos " << new_pos << " is_water_tile";
      }else{
        other_serf->pos = pos;
        map->set_serf_index(other_serf->pos, other_serf->get_index());
        other_serf->animation =
            get_walking_animation(map->get_height(other_serf->pos) -
                                  map->get_height(new_pos),
                                  reverse_direction(dir), 1);
        other_serf->counter = counter_from_animation[other_serf->animation];

        animation = get_walking_animation(map->get_height(new_pos) -
                                          map->get_height(pos),
                                          (Direction)dir, 1);
        s.walking.dir = reverse_direction(dir);
      }
    } else {
      //if (type == Serf::TypeSailor && state == State::StateTransporting){
       //Log::Info["serf"] << "debug: a transporting sailor inside change_direction, serf at next pos is NOT eligible for switching, waiting";
      //}
      /* Wait for other serf */
      animation = 81 + dir;
      counter = counter_from_animation[animation];
      s.walking.dir = dir-6;
      return;
    }
  }

  //if (type == Serf::TypeSailor && state == State::StateTransporting){
   //Log::Info["serf"] << "debug: a transporting sailor inside change_direction, FOO 1";
  //}

  if (!alt_end) s.walking.wait_counter = 0;
  pos = new_pos;
  map->set_serf_index(pos, get_index());
  counter += counter_from_animation[animation];
  if (alt_end && counter < 0) {
    if (map->has_flag(new_pos)) {
      counter = 0;
    } else {
      Log::Warn["serf"] << "unhandled jump to 31B82.";
    }
  }
}

/* Precondition: serf state is in WALKING or TRANSPORTING state */
void
Serf::transporter_move_to_flag(Flag *flag) {
  //Log::Debug["serf.cc"] << "inside Serf::transporter_move_to_flag(), for a serf with index " << get_index() << ", moving to flag at pos " << flag->get_position();
  // is s.transporting.dir the direction that the transporter is travelling?
  //  or, is it the direction that the res at the reached flag is heading (i.e. the reverse dir)?
  //   it seems to be the latter, but that could be faulty logic in sailor transport serf code
  Direction dir = (Direction)s.transporting.dir;
  if (get_type() == Serf::TypeSailor){
   //Log::Info["serf"] << "debug: sailor inside Serf::transporter_move_to_flag, dir: " << NameDirection[dir];

   //Log::Info["serf"] << "debug: sailor inside Serf::transporter_move_to_flag A, this serf's type == " << NameSerf[type];
   //Log::Info["serf"] << "debug: sailor inside Serf::transporter_move_to_flag A, flag->is_water_path " << NameDirection[dir] << " = " << flag->is_water_path(dir);
   //Log::Info["serf"] << "debug: sailor inside Serf::transporter_move_to_flag A, flag->has_serf_waiting_for_boat() == " << flag->has_serf_waiting_for_boat();
    
    // adding support for option_CanTransportSerfsInBoats
    Direction other_dir = flag->get_other_end_dir(dir);
    //Log::Info["serf"] << "debug: sailor inside Serf::transporter_move_to_flag A, flag other_end_dir = " << NameDirection[other_dir];
    // WARNING - is_water_path can only be uses to check a path that is certain to exist, it will return true if there is no path at all!
    if (flag->is_water_path(dir) && get_type() == Serf::TypeSailor){

      // handle dropping a passenger serf off at flag
      if (s.transporting.res == Resource::TypeSerf){
       //Log::Info["serf"] << "debug: inside Serf::transporter_move_to_flag, dropping passenger off at flag at pos " << flag->get_position();
        // because the map only allows one serf per tile, the dropped off serf cannot actually be placed yet
        //  it remains a record in the sailor's s.transporting struct until the sailor reaches the next pos away from the flag
        //  Also, this must support picking up another passenger at the same time the previous one is still in dropoff limbo!
        s.transporting.dropped_serf_index = s.transporting.passenger_serf_index;
        s.transporting.dropped_serf_type = s.transporting.passenger_serf_type;
        //  set the dropped serf's dir so it isn't facing back towards the water road
        //  If this is not done, and the serf is still facing the water road, it will think it is waiting for a boat
        // wait this doesn't fix the problem either... maybe something else is going on
        //game->get_serf(s.transporting.dropped_serf_index)->s.walking.dir = s.transporting.dir;
        // clear the passenger serf values
        s.transporting.passenger_serf_index = 0;
        s.transporting.passenger_serf_type = Serf::TypeNone;
        s.transporting.res = Resource::TypeNone;
      }
      // need to handle dropping off any resource here or else it will get wiped during a potential passenger pickup
      //  as the passenger becomes s.transporting.res TypeSerf
      else if (s.transporting.res != Resource::TypeNone) {
        /* Drop resource at flag */
        //Log::Info["serf.cc"] << "inside transporter_move_to_flag(water), calling flag->drop_resource for s.transporting.res of type " << NameResource[s.transporting.res];
        if (flag->drop_resource(s.transporting.res, s.transporting.dest)) {
          //Log::Info["serf.cc"] << "inside transporter_move_to_flag(water), flag->drop_resource call returned true";
          s.transporting.res = Resource::TypeNone;
        }
      }

      // handle picking a passenger serf up at flag
      if (flag->has_serf_waiting_for_boat()){
       //Log::Info["serf"] << "debug: sailor: a serf is waiting for a boat at pos " << flag->get_position() << ", in dir " << NameDirection[other_dir];
        // get in the boat!
        //  I don't think this flag->pick_up_serf() function is needed after all, get rid of it
        flag->pick_up_serf();
        Serf *passenger = game->get_serf(s.transporting.pickup_serf_index);
        if (passenger == nullptr){
          Log::Warn["serf"] << "failed to get passenger serf from sailor's s.transporting.pickup_serf_index value!";
        }else{
          //Log::Info["serf"] << "found passenger serf from sailor's s.transporting.pickup_serf_index value";
          passenger->set_serf_state(Serf::StateBoatPassenger);
          passenger->pos = bad_map_pos;
          s.transporting.res = Resource::TypeSerf;
          s.transporting.passenger_serf_index = s.transporting.pickup_serf_index;
          s.transporting.passenger_serf_type = s.transporting.pickup_serf_type;
        }
        // clear the picked-up serf values
        s.transporting.pickup_serf_index = 0;
        s.transporting.pickup_serf_type = Serf::TypeNone;
		    // clear the resource destination so the sailor doesn't try to "deliver a serf" to a flag/building!
		    s.transporting.dest = -1;
        change_direction(dir, 1);
        return;
      }

    }
   //Log::Info["serf"] << "debug: sailor inside Serf::transporter_move_to_flag B";
  }
  if (flag->is_scheduled(dir)) {
    int res_index = flag->scheduled_slot(dir);
    if (flag->get_resource_at_slot(res_index) == Resource::TypeNone){
      Log::Warn["serf.cc"] << "inside Serf::transporter_move_to_flag, flag at pos " << flag->get_position() << ", flag->is_scheduled in dir " << dir << ", slot #" << res_index << ", to pickup res, but resource type is TypeNone!";
    }
    //Log::Info["serf.cc"] << "debug: inside Serf::transporter_move_to_flag, flag at pos " << flag->get_position() << ", flag->is_scheduled in dir " << dir << ", slot #" << res_index << ", to pickup res type " << NameResource[flag->get_resource_at_slot(res_index)];
    //Log::Info["serf.cc"] << "debug: inside Serf::transporter_move_to_flag, flag at pos " << flag->get_position() << ", flag->is_scheduled in dir " << dir << ", slot #" << res_index << ", to pickup res type #" << flag->get_resource_at_slot(res_index);
    //if (get_type() == Serf::TypeSailor){
     //Log::Info["serf.cc"] << "debug: sailor inside Serf::transporter_move_to_flag, flag->is_scheduled in dir " << NameDirection[dir];
    //}
    /* Fetch resource from flag */
    s.transporting.wait_counter = 0;

    if (s.transporting.res == Resource::TypeNone) {
      /* Pick up resource. */
      //Log::Info["serf.cc"] << "debug: inside Serf::transporter_move_to_flag, flag at pos " << flag->get_position() << ", flag->is_scheduled in dir " << dir << ", no res carried, calling flag->pick_up_resource, slot #" << res_index << ", to pickup res type " << NameResource[flag->get_resource_at_slot(res_index)];
      //Log::Info["serf.cc"] << "debug: inside Serf::transporter_move_to_flag, flag at pos " << flag->get_position() << ", flag->is_scheduled in dir " << dir << ", no res carried, calling flag->pick_up_resource, slot #" << res_index << ", to pickup res type #" << flag->get_resource_at_slot(res_index);
      if (!flag->pick_up_resource(res_index, &s.transporting.res, &s.transporting.dest)){
        Log::Warn["serf.cc"] << "inside Serf::transporter_move_to_flag, flag at pos " << flag->get_position() << ", flag->is_scheduled in dir " << dir << ", no res carried, calling flag->pick_up_resource, slot #" << res_index << ", to pickup res type #" << flag->get_resource_at_slot(res_index) << ", flag->pick_up_resource call returned false!  this is ignored";
        // NOTE - pyrdacor's Freeserf.NET seems to check the result of the flag->pick_up_resource call
        //  and if it returns false, it sets the equivalent of s.transporting.res to TypeNone again (if that matters)
        //   and also sets the equivalent of s.transporting.dest to '0u' which I'm sure has some binary meaning
        // I am not sure if this is to work around a bug in Freeserf's logic (which Freeserf.NET is based on) or if
        //  it is a fix to something unique to Freeserf.NET (which uses s.Walking instead of s.transporting for this work)
        
        // looking more closely, it seems that when flag->pick_up_resource returns false, it does not change the values
        //  of .dest or .res, so I am not seeing the need to do this yet...
      }
    } else {
      //Log::Info["serf.cc"] << "debug: inside Serf::transporter_move_to_flag, flag at pos " << flag->get_position() << ", flag->is_scheduled in dir " << dir << ", res " << NameResource[s.transporting.res] << " currently carried, switching resource at flag with slot #" << res_index << ", res type #" << flag->get_resource_at_slot(res_index);
      /* Switch resources and destination. */
      Resource::Type temp_res = s.transporting.res;
      int temp_dest = s.transporting.dest;

      flag->pick_up_resource(res_index, &s.transporting.res,
                             &s.transporting.dest);

      flag->drop_resource(temp_res, temp_dest);
    }

    /* Find next resource to be picked up */
    Player *player = game->get_player(get_owner());
    //Log::Info["serf.cc"] << "debug: inside Serf::transporter_move_to_flag, flag at pos " << flag->get_position() << ", calling flag->prioritize_pickup for flag at pos " << flag->get_position() << ", ";
    flag->prioritize_pickup((Direction)dir, player);
  } else if (s.transporting.res != Resource::TypeNone) {
    //Log::Info["serf.cc"] << "debug: inside Serf::transporter_move_to_flag, flag at pos " << flag->get_position() << ", s.transporting.res != TypeNone";
    //if (get_type() == Serf::TypeSailor){
     //Log::Info["serf"] << "debug: inside Serf::transporter_move_to_flag C";
    //}
      /* Drop resource at flag */
      //Log::Info["serf.cc"] << "inside transporter_move_to_flag, flag at pos " << flag->get_position() << ", calling flag->drop_resource for s.transporting.res of type " << NameResource[s.transporting.res];
      if (flag->drop_resource(s.transporting.res, s.transporting.dest)) {
        //Log::Info["serf.cc"] << "inside transporter_move_to_flag, flag at pos " << flag->get_position() << ", flag->drop_resource returned true, dropping resource call returned true";
        s.transporting.res = Resource::TypeNone;
      }
    //}
  }
  //if (get_type() == Serf::TypeSailor){
   //Log::Info["serf"] << "debug: inside Serf::transporter_move_to_flag D";
  //}

  //Log::Info["serf.cc"] << "debug: inside Serf::transporter_move_to_flag,  flag at pos " << flag->get_position() << ", calling change_direction";
  change_direction(dir, 1);
  //if (get_type() == Serf::TypeSailor){
   //Log::Info["serf"] << "debug: inside Serf::transporter_move_to_flag E";
  //}
}

bool
Serf::handle_serf_walking_state_search_cb(Flag *flag, void *data) {
  //Log::Info["serf"] << "debug: inside Serf::handle_serf_walking_state_search_cb with flag at pos " << flag->get_position();

  Serf *serf = static_cast<Serf*>(data);
  //Log::Info["serf"] << "debug: inside Serf::handle_serf_walking_state_search_cb with serf at pos " << serf->get_pos();
  Flag *dest = flag->get_game()->get_flag(serf->s.walking.dest);
  if (flag == dest) {
    //Log::Info["serf"] << "debug: inside Serf::handle_serf_walking_state_search_cb, DEST FOUND!";
    //Log::Verbose["serf"] << "dest found: " << dest->get_search_dir();
    serf->change_direction(dest->get_search_dir(), 0);
    return true;
  }
  //Log::Info["serf"] << "debug: inside Serf::handle_serf_walking_state_search_cb, dest NOT found";

  return false;
}

void
Serf::start_walking(Direction dir, int slope, int change_pos) {
  //Log::Debug["serf.cc"] << "start of Serf::start_walking";
  PMap map = game->get_map();
  MapPos new_pos = map->move(pos, dir);
  //Log::Info["serf"] << "debug: inside start_walking, old pos: " << pos << ", new pos: " << new_pos;
  animation = get_walking_animation(map->get_height(new_pos) -
                                    map->get_height(pos), dir, 0);
  counter += (slope * counter_from_animation[animation]) >> 5;

  //Log::Info["serf"] << "debug: inside start_walking, animation = " << animation;
  if (change_pos) {
    //Log::Debug["serf.cc"] << "inside Serf::start_walking, change_pos is TRUE, moving serf to new_pos " << new_pos;
    map->set_serf_index(pos, 0);
    map->set_serf_index(new_pos, get_index());
  }

  pos = new_pos;
  //Log::Debug["serf.cc"] << "done Serf::start_walking, serf #" << get_index() << " now occupies pos " << pos;
}

// I don't understand what this is for... is it
//  saying that the walking time to enter a building
//  varies based on building *type*???  and this is
//  adjusted by faking a slope value as if it were
//  a steeper road?
static const int road_building_slope[] = {
  /* Finished building */
  5, 18, 18, 15, 18, 22, 22, 22,
  22, 18, 16, 18, 1, 10, 1, 15,
  15, 16, 15, 15, 10, 15, 20, 15,
  18
};

/* Start entering building in direction up-left.
   If join_pos is set the serf is assumed to origin from
   a joined position so the source position will not have it's
   serf index cleared. */
// it seems that join_pos is only set when it is a knight leaving
//  a building for a fight, meaning the knight is joining the 
//  attacking serf's pos without actually taking the pos as far
//  as serf-pos and map-serf-at-pos tracking goes
// Note that Viewport:draw_serf_row function handles drawing both
//  knights in the same pos as a special case
// NOTE - by the time enter_building is called, the Building already
//  has holder=true because Building::requested_serf_reached sets
//  holder to true, before enter_building is called
void
Serf::enter_building(int field_B, int join_pos) {
  //Log::Debug["serf.cc"] << "inside Serf::enter_building, setting serf #" << get_index() << " state to StateEnteringBuilding";
  set_state(StateEnteringBuilding);
  //Log::Info["serf"] << "debug: inside enter_building, calling start_walking, join_pos = " << join_pos;
  //
  // start_walking function is where serf actually takes the new pos!  it is easy to miss
  //
  start_walking(DirectionUpLeft, 32, !join_pos);
  //Log::Info["serf"] << "debug: inside enter_building, back from start_walking";
  if (join_pos) game->get_map()->set_serf_index(pos, get_index());

  Building *building = game->get_building_at_pos(pos);
  if (building == nullptr){
    Log::Warn["serf.cc"] << "inside Serf::enter_building, serf with index #" << get_index() << " at pos " << get_pos() << " is trying to enter a building, but Building is unexpectedly a nullptr! setting serf to Lost";
    //throw ExceptionFreeserf("inside Serf::enter_building, building is unexpectedly a nullptr!");
    set_lost_state();
    return;
  }
  int slope = road_building_slope[building->get_type()];
  if (!building->is_done()) slope = 1;
  s.entering_building.slope_len = (slope * counter) >> 5;
  s.entering_building.field_B = field_B;
}

/* Start leaving building by switching to LEAVING BUILDING and
   setting appropriate state. */
// it seems that join_pos is only set when it is a knight leaving
//  a building for a fight, meaning the knight is joining the 
//  attacking serf's pos without actually taking the pos as far
//  as serf-pos and map-serf-at-pos tracking goes
// Note that Viewport:draw_serf_row function handles drawing both
//  knights in the same pos as a special case
void
Serf::leave_building(int join_pos) {
  Building *building = game->get_building_at_pos(pos);
  int slope;
  if (building == nullptr){
    Log::Warn["serf"] << "got nullptr for building at pos " << pos << " inside Serf::leave_building, trying a safe default slope value";
    slope = 30;
  }else{
    slope = 31 - road_building_slope[building->get_type()];
    if (!building->is_done()) slope = 30;
  }

  if (join_pos) game->get_map()->set_serf_index(pos, 0);
  start_walking(DirectionDownRight, slope, !join_pos);

  set_state(StateLeavingBuilding);
}

void
Serf::handle_serf_walking_state_dest_reached() {
  //Log::Debug["serf"] << "inside Serf::handle_serf_walking_state_dest_reached(), for a serf with index " << get_index() << ", s.walking.dir1 is " << s.walking.dir1;
  /* Destination reached. */
  //Log::Info["serf"] << "debug: inside handle_serf_walking_state_dest_reached, A";
  if (s.walking.dir1 < 0) {
   //Log::Info["serf"] << "debug: inside handle_serf_walking_state_dest_reached, B";
    PMap map = game->get_map();
    Building *building = game->get_building_at_pos(map->move_up_left(pos));
    if (building == nullptr){
      Log::Warn["serf.cc"] << "inside Serf::handle_serf_walking_state_dest_reached(), building is nullptr!  cannot enter, setting serf to Lost";
      Log::Debug["serf"] << "inside Serf::handle_serf_walking_state_dest_reached(), building is nullptr!, serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
      set_state(Serf::StateLost);
      //s.lost.field_B = 1;  // not sure what this does, copied from another set Lost segment. It seems to control whether closest or farther away flag is preferred as next dest?
      s.lost.field_B = 0;  // this seems to be correct for "the building I am about to enter is no longer valid"
      counter = 0;
      return;
    }

/* removing AdvancedDemolition for now, see https://github.com/forkserf/forkserf/issues/180
    // if a requested serf was already dispatched to this building before it was marked for demo
    //  he will arrive at its flag and attempt to enter.  Prevent this (unless it is the demo serf himself!)
    // ALSO, handle knights to be evicted
    if (option_AdvancedDemolition && building->is_pending_demolition()){
      if (get_type() == Serf::TypeDigger){
        // if this *is* the arriving demo serf, need to check to see if this is a military building whose knights need to be 
        //  evicted.  Because this serf hasn't occupied the building pos yet it can't evict the knights now because this serf
        //  is blocking the escape/flag pos.  But, once the serf enters the building pos he becomes the holder and the "get next_knight"
        //  logic will fail when it comes time to evict the knights.  So, as a work-around, save the current holder knight's index
        //  so that an illegal, temporary situation can be set where the Digger/demo serf is the Holder and the knights inside are 
        //  usual next_knights.  If a game were saved/loaded in this state it might crash, but it should only last for a single game update
        if (building->is_military() && building->is_active()){
          Log::Debug["serf.cc"] << "inside handle_serf_walking_state_dest_reached, option_AdvancedDemolition, demo serf with index #" << get_index() << " has arrived at an occupied military building, trying possibly-illegal setting this serf's s.defending.next_knight as current holder, knight with serf index# " << building->get_holder_or_first_knight();
          s.defending.next_knight = building->get_holder_or_first_knight();
        }
      }else{  
        // type is NOT Digger/demo serf
        Log::Debug["serf.cc"] << "inside handle_serf_walking_state_dest_reached, option_AdvancedDemolition, a requested serf OTHER THAN THE demo serf has arrived at the dest flag for a building pending_demolition at pos " << pos << ", setting him to Lost and returning early";
        
        //set_state(Serf::StateLost);
        ////s.lost.field_B = 1;  // not sure what this does, copied from another set Lost segment. It seems to control whether closest or farther away flag is preferred as next dest?
        //s.lost.field_B = 0;  // this seems to be correct for "the building I am about to enter is no longer valid"
        //counter = 0;
        
        set_state(Serf::StateWalking);
        s.walking.dest = 0;  // with dest 0, handle_serf_walking_state should find a new dest (an Inventory to return to)
        counter = 0;
        return;
      }
    }
    */

    building->requested_serf_reached(this);  // this is the only place in the entire code that this function is called and holder bool is set to true

    if (map->has_serf(map->move_up_left(pos))) {
      // if there is a serf blocking, wait (?)
      //Log::Info["serf"] << "debug: inside handle_serf_walking_state_dest_reached, C";
      animation = 85;
      counter = 0;
      //Log::Debug["serf.cc"] << "inside Serf::handle_serf_walking_state_dest_reached, at least one serf already in this Building pos (blocking?), setting StateReadyToEnter";
      set_state(StateReadyToEnter);
    } else {
      // otherwise enter the building now
      //Log::Info["serf"] << "debug: inside handle_serf_walking_state_dest_reached, D";
      //Log::Debug["serf.cc"] << "inside Serf::handle_serf_walking_state_dest_reached, no serf currently occupies this Building pos, calling Serf::enter_building with no join_pos";
      enter_building(s.walking.dir1, 0);
    }
   //Log::Info["serf"] << "debug: inside handle_serf_walking_state_dest_reached, E";
  } else if (s.walking.dir1 == 6) {
   //Log::Info["serf"] << "debug: inside handle_serf_walking_state_dest_reached, F";
    set_state(StateLookingForGeoSpot);
    counter = 0;
  } else {
   //Log::Info["serf"] << "debug: inside handle_serf_walking_state_dest_reached, G";
    Flag *flag = game->get_flag_at_pos(pos);
    if (flag == nullptr) {
      throw ExceptionFreeserf("Flag expected as destination of walking serf.");
    }
    Direction dir = (Direction)s.walking.dir1;
    Flag *other_flag = flag->get_other_end_flag(dir);
    if (other_flag == nullptr) {
      throw ExceptionFreeserf("Path has no other end flag in selected dir.");
    }
    Direction other_dir = flag->get_other_end_dir(dir);

    /* Increment transport serf count */
    flag->complete_serf_request(dir);
    other_flag->complete_serf_request(other_dir);

    //if (get_type() == Serf::TypeSailor){
     //Log::Info["serf"] << "debug: inside handle_serf_walking_state_dest_reached, a Sailor has reached destination";
    //}else{
     //Log::Info["serf"] << "debug: inside handle_serf_walking_state_dest_reached, non-Sailor has reached destination";
    //}
    //Log::Info["serf"] << "debug: inside handle_serf_walking_state_dest_reached, H";
    //Log::Debug["serf"] << "inside Serf::handle_serf_walking_state_dest_reached(), for a serf with index " << get_index() << ", setting StateTransporting";
    set_state(StateTransporting);
    s.transporting.res = Resource::TypeNone;
    s.transporting.dir = dir;
    s.transporting.wait_counter = 0;
    //Log::Debug["serf"] << "inside Serf::handle_serf_walking_state_dest_reached(), for a serf with index " << get_index() << ", already set StateTransporting, about to call transporter_move_to_flag";
   //Log::Info["serf"] << "debug: at the end of handle_serf_walking_state_dest_reached, about to call transporter_move_to_flag at pos " << flag->get_position();
    transporter_move_to_flag(flag);
   //Log::Info["serf"] << "debug: inside handle_serf_walking_state_dest_reached, I";
  }
}

void
Serf::handle_serf_walking_state_waiting() {
  /* Waiting for other serf. */
  Direction dir = (Direction)(s.walking.dir + 6);

  PMap map = game->get_map();
  /* Only check for loops once in a while. */
  s.walking.wait_counter += 1;

  // from nicymike:
  /* The original code does a reverse check:
    if ((map->has_flag(pos) && s.walking.wait_counter < 10) ||
      (!map->has_flag(pos) && s.walking.wait_counter < 50)) {
   ... do nothing here
  } else{
   ... check for loop here
  }
  Basically if a serf is at a flag just count to 10 to prioritize him because the chance is higher that this will solve the deadlock.
  If a serf is not at at flag count to 50.
  */
  // nicymike fixed if check here:
  if ((!map->has_flag(pos) || s.walking.wait_counter >= 10) &&
      (map->has_flag(pos) || s.walking.wait_counter >= 50)) {
    MapPos pos_ = pos;
    /* Follow the chain of serfs waiting for each other and
       see if there is a loop. */
    for (int i = 0; i < 100; i++) {
      pos_ = map->move(pos_, dir);

      if (!map->has_serf(pos_)) {
        break;
      } else if (map->get_serf_index(pos_) == index) {
        /* We have found a loop, try a different direction. */
        change_direction(reverse_direction(dir), 0);
        return;
      }

      /* Get next serf and follow the chain */
      Serf *other_serf = game->get_serf_at_pos(pos);
      if (other_serf->state != StateWalking &&
          other_serf->state != StateTransporting) {
        break;
      }

      if (other_serf->s.walking.dir >= 0 ||
          (other_serf->s.walking.dir + 6) == reverse_direction(dir)) {
        break;
      }

      dir = (Direction)(other_serf->s.walking.dir + 6);
    }
    // bugfix from nicymike:
    /* Wait counter should only be reset inside the if. If not it will never be increased above one. */
    // it was originally outside the if block, right alongside change_direction below
    s.walking.wait_counter = 0;
  }

  /* Stick to the same direction */
  change_direction((Direction)(s.walking.dir + 6), 0);
}

void
Serf::handle_serf_walking_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  //Log::Debug["serf"] << "inside handle_serf_walking_state, a serf with index " << get_index() << " has dest s.walking.dest " << s.walking.dest;
  while (counter < 0) {
    if (s.walking.dir < 0) {
      handle_serf_walking_state_waiting();
      continue;
    } 
    /* 301F0 */
    if (game->get_map()->has_flag(pos)) {
      /* Serf has reached a flag 
         Search for a destination if none is known. */
      if (s.walking.dest == 0) {
        // serf HAS NO DESTINATION
        int flag_index = game->get_map()->get_obj_index(pos);
        Flag *src = game->get_flag(flag_index);
        // support allowing Lost serfs to enter any nearby friendly building
        //  EXCEPT mines, as they can deadlock when the miner runs out of food and 
        //  holds the pos, disallowing serfs entry
        int r = -1;
        if ( option_LostTransportersClearFaster && was_lost
          && (get_type() == Serf::TypeTransporter || get_type() == Serf::TypeGeneric || get_type() == Serf::TypeNone) ){
          //Log::Debug["serf"] << "inside Serf::handle_serf_walking_state(), a generic serf is looking for an inventory and was_lost recently, using special non-Inventory clearing function";
          //// unset the was_lost bool so it doesn't stay forever
          // NOOOO don't do this yet it still needs to be true when serf goes into entering building state, to prevent it from
          //  going to entering_INVENTORY state!
          //was_lost = false;
          r = src->find_nearest_building_for_lost_generic_serf();
        }else{
          //Log::Debug["serf.cc"] << "inside Serf::handle_serf_walking_state(), a serf of type " << get_type() << " is looking for an inventory and was not recently Lost, using normal Inventory clearing function";
          r = src->find_nearest_inventory_for_serf();
        }
        if (r < 0) {
          //Log::Warn["serf"] << "inside Serf::handle_serf_walking_state(), a serf at pos " << get_pos() << " is being set to StateLost! because the result of the previous find_nearest_inventory_for_serf call returned negative, r: " << r;

          //debug
          //if (game->get_flag(recent_dest) != nullptr){
          //  Log::Debug["serf.cc"] << "inside handle_serf_walking_state A, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which has pos " << game->get_flag(recent_dest)->get_position();
          //}else{
          //  Log::Debug["serf.cc"] << "inside handle_serf_walking_state A, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which is a nullptr";
          //}
          Log::Debug["serf"] << "inside Serf::handle_serf_walking_state A, serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
          set_state(StateLost);
          s.lost.field_B = 1;
          counter = 0;
          return;
        }
        s.walking.dest = r;
      }
      // serf either already had or should have been assigned a KNOWN destination in prior block
      /* Check whether KNOWN destination has been reached.
         If not, find out which direction to move in
         to reach the destination. */
      if (s.walking.dest == game->get_map()->get_obj_index(pos)) {
        //Log::Debug["serf"] << "inside Serf::handle_serf_walking_state(), a serf with index " << get_index() << " has reached s.walking.dest " << s.walking.dest << ", about to call handle_serf_walking_state_dest_reached";
        handle_serf_walking_state_dest_reached();
        return;
      } else {
        Flag *src = game->get_flag_at_pos(pos);
        FlagSearch search(game);
        for (Direction i : cycle_directions_ccw()) {
          // WARNING - is_water_path can only be uses to check a path that is certain to exist, it will return true if there is no path at all!
          //if (!src->is_water_path(i)) {
          if (!src->is_water_path(i) ||
             // adding support for option_CanTransportSerfsInBoats
             (option_CanTransportSerfsInBoats && src->has_path(i) && src->is_water_path(i) && src->has_transporter(i))
             ){
            Flag *other_flag = src->get_other_end_flag(i);
            other_flag->set_search_dir(i);
            search.add_source(other_flag);
          }
        }
        bool r = search.execute(handle_serf_walking_state_search_cb, true, false, this);
        // I can't figure out what it is that triggers this loop to end,
        //  so just cut it short if serf is waiting for a boat
        if (r && get_state() == Serf::StateWaitForBoat){
          Log::Info["serf"] << "debug: inside handle_serf_walking_state, after search.execute serf is in StateWaitForBoat, returning";
          return;
        }
        if (r){
          continue;
        }
      }
    } else {
      /* 30A37 */
      /* Serf is not at a flag. Just follow the road. */
      int paths = game->get_map()->paths(pos) & ~BIT(s.walking.dir);
      Direction dir = DirectionNone;
      for (Direction d : cycle_directions_cw()) {
        if (paths == BIT(d)) {
          dir = d;
          break;
        }
      }

      if (dir >= 0) {
        change_direction(dir, 0);
        continue;
      }

      counter = 0;
    }

    /* Either the road is a dead end; or
       we are at a flag, but the flag search for
       the destination failed. */
    if (s.walking.dir1 < 0) {
      if (s.walking.dir1 < -1) {
        Log::Warn["serf"] << "inside Serf::handle_serf_walking_state(), a serf at pos " << get_pos() << " is being set to StateLost! because Either the road is a dead end; or we are at a flag, but the flag search for the destination failed.";

        //debug
        //if (game->get_flag(recent_dest) != nullptr){
        //  Log::Debug["serf"] << "inside serf_walking_state B, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which has pos " << game->get_flag(recent_dest)->get_position();
        //}else{
        //  Log::Debug["serf"] << "inside serf_walking_state B, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which is a nullptr";
        //}
        Log::Debug["serf"] << "inside Serf::handle_serf_walking_state B, serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
        set_state(StateLost);
        s.lost.field_B = 1;
        counter = 0;
        return;
      }

      Flag *flag = game->get_flag(s.walking.dest);
      Building *building = flag->get_building();
      building->requested_serf_lost();
    } else if (s.walking.dir1 != 6) {
      Flag *flag = game->get_flag(s.walking.dest);
      Direction d = (Direction)s.walking.dir1;
      flag->cancel_serf_request(d);
      flag->get_other_end_flag(d)->cancel_serf_request(
                                     flag->get_other_end_dir(d));
    }

    s.walking.dir1 = -2;
    s.walking.dest = 0;
    counter = 0;
  }
}

void
Serf::handle_serf_transporting_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  //Log::Debug["serf"] << "a transporting serf with index " << get_index() << " has values: dest " << s.transporting.dest << ", dir" << s.transporting.dir << ", res " << s.transporting.res << ", wait_counter " << s.transporting.wait_counter;

  if (option_SailorsMoveFaster){
    if (type == Serf::TypeSailor){
      //Log::Info["serf"] << "debug: a sailor is in transporting state 000, doubling his movement!";
      //if (game->get_tick() % 4 == 0){   // 1.5x speed
      if (game->get_tick() % 4 == 0 || game->get_tick() % 6 == 0){   // 1.66x speed
        //Log::Info["serf"] << "debug: a sailor is in transporting state 000, increase his movement by 66%!";
        counter -= delta;
      //}else{
      //  Log::Info["serf"] << "debug: a sailor is in transporting state 000, not increase his movement this tick";
      }
    }
  }

  if (counter >= 0) return;

  //if (type == Serf::TypeSailor){
   //Log::Info["serf"] << "debug: a sailor at pos " << pos << " is in transporting state 00, transporting dir: " << s.transporting.dir << ", walking dir: " << s.walking.dir;
  //}

  if (s.transporting.dir < 0) {
    //Log::Info["serf"] << "debug: a serf in transporting state 0 is changing direction because s.transporting.dir < 0 (-1?)";
    //if (type == Serf::TypeSailor){
     //Log::Info["serf"] << "debug: a sailor in transporting state 0 is changing direction because s.transporting.dir < 0 (-1?)";
    //}
    change_direction((Direction)(s.transporting.dir + 6), 1);
  } else {
    //Log::Info["serf"] << "debug: a serf in transporting state A, with s.transporting.dir " << s.transporting.dir;
    //if (type == Serf::TypeSailor){
      // this crashes if Dir > 5 !  (probably because of NameDirection overrun)
      //Log::Info["serf"] << "debug: a sailor in transporting state A, with s.transporting.dir " << NameDirection[s.transporting.dir];
     //Log::Info["serf"] << "debug: a sailor in transporting state A, with s.transporting.dir " << s.transporting.dir;
    //}
    PMap map = game->get_map();
    /* 31549 */
    if (map->has_flag(pos)) {
      //if (type == Serf::TypeSailor){
       //Log::Info["serf"] << "debug: a sailor in transporting state B, reached a flag";
      //}
      /* Current position occupied by waiting transporter */
      if (s.transporting.wait_counter < 0) {
        set_state(StateWalking);
        s.walking.wait_counter = 0;
        s.walking.dir1 = -2;
        s.walking.dest = 0;
        counter = 0;
       //Log::Info["serf"] << "debug: a serf in transporting state B0, returning";
        return;
      }
     //Log::Info["serf"] << "debug: a serf in transporting state, s.transporting.res = " << s.transporting.res;
     //Log::Info["serf"] << "debug: a serf in transporting state, s.transporting.dest = " << s.transporting.dest;
	   //Log::Info["serf"] << "debug: a serf in transporting state, map->get_obj_index(pos) = " << map->get_obj_index(pos);
      /* 31590 */
      if (s.transporting.res != Resource::TypeNone &&
          map->get_obj_index(pos) == s.transporting.dest) {
            //if (type == Serf::TypeSailor){
             //Log::Info["serf"] << "debug: a sailor in transporting state C";
            //}
        /* At resource destination */
        set_state(StateDelivering);
        s.transporting.wait_counter = 0;

        MapPos new_pos = map->move_up_left(pos);
        animation = 3 + map->get_height(new_pos) - map->get_height(pos) +
                    (DirectionUpLeft + 6) * 9;
        counter = counter_from_animation[animation];
        /* TODO next call is actually into the middle of
           handle_serf_delivering_state().
           Why is a nice and clean state switch not enough???
           Just ignore this call and we'll be safe, I think... */
        /* handle_serf_delivering_state(serf); */
        return;
      }

      Flag *flag = game->get_flag_at_pos(pos);
      //if (type == Serf::TypeSailor){
       //Log::Info["serf"] << "debug: a sailor in transporting state D, about to call transporter_move_to_flag at pos " << flag->get_position();
      //}
      transporter_move_to_flag(flag);
      //if (type == Serf::TypeSailor){
       //Log::Info["serf"] << "debug: a sailor in transporting state E";
      //}
    } else {
      //if (type == Serf::TypeSailor){
       //Log::Info["serf"] << "debug: a sailor in transporting state F";
      //}
      int paths = map->paths(pos) & ~BIT(s.walking.dir);
      Direction dir = DirectionNone;
      for (Direction d : cycle_directions_cw()) {
        if (paths == BIT(d)) {
          dir = d;
          break;
        }
      }
      //if (type == Serf::TypeSailor){
       //Log::Info["serf"] << "debug: a sailor in transporting state G, found a path in dir: " << NameDirection[dir];
      //}

      if (dir < 0) {
        //if (type == Serf::TypeSailor){
         //Log::Info["serf"] << "debug: a sailor in transporting state G2, dir is < 0 (-1?), setting serf to lost state";
        //}
        Log::Warn["serf"] << "inside handling_serf_transporting_state(), a serf at pos " << get_pos() << " is being set to StateLost! because dir of " << dir << " is less than zero";

        /*
        //debug
        if (game->get_flag(recent_dest) != nullptr){
          Log::Debug["serf"] << "inside handle_serf_transporting_state, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which has pos " << game->get_flag(recent_dest)->get_position();
        }else{
          Log::Debug["serf"] << "inside handle_serf_transporting_state, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which is a nullptr";
        }
        */
        Log::Debug["serf"] << "inside Serf::handle_serf_transporting_state(), serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
        set_state(StateLost);
        counter = 0;
        return;
      }
      //if (type == Serf::TypeSailor){
       //Log::Info["serf"] << "debug: a sailor in transporting state H";
      // //Log::Info["serf"] << "debug: map->has_flag(map->move(pos, dir) = " << map->has_flag(map->move(pos, dir));
      // //Log::Info["serf"] << "debug: s.transporting.res = " << s.transporting.res;
        //Log::Info["serf"] << "debug: s.transporting.res = " << NameResource[s.transporting.res];
      // //Log::Info["serf"] << "debug: s.transporting.wait_counter = " << s.transporting.wait_counter;
        //if (!map->has_flag(map->move(pos, dir))){
         //Log::Info["serf"] << "debug: a sailor in transporting state H000, map in dir " << NameDirection[dir] << " from pos " << pos << " is NOT a flag";
        //}
        // i think it is crashing here, skipping this part for now
        /*
        if (s.transporting.res != Resource::TypeNone){
         //Log::Info["serf"] << "debug: a sailor in transporting state H00, s.transporter.res is NOT TypeNone, but is: " << NameResource[s.transporting.res];
        }
        if (s.transporting.wait_counter < 0) {
         //Log::Info["serf"] << "debug: a sailor in transporting state H0, wait_counter IS less than zero, it is: " << s.transporting.wait_counter;
        }
        */
      //}
      
      // add support for option_CanTransportSerfsInBoats
      // if *this* serf is a sailor (who is already in transporting state and so must be in his boat)...
      //  and if another serf is at the next pos in StateWaitForBoat AND is waiting in the direction of this serf
      //   continue "through" the waiting serf to pick him up
      //
      bool sailor_pickup_serf = false;
      //Log::Info["serf"] << "debug: a serf in transporting state H B";
      MapPos new_pos = map->move(pos, dir);
      //Log::Info["serf"] << "debug: a serf in transporting state H C";
      /*
      // debugging
      if (type == Serf::TypeSailor){
        Log::Info["serf"] << "debug: a sailor inside handle_transporting_state, heading in dir " << NameDirection[dir];
        if (game->get_serf_at_pos(new_pos)->get_state() == Serf::StateWaitForBoat){
          Log::Info["serf"] << "debug: a sailor inside handle_serf_transporting_state, there is a serf in wait_for_boat state in next pos with dir " << NameDirection[game->get_serf_at_pos(new_pos)->get_walking_dir()];
          Log::Info["serf"] << "debug: a sailor inside handle_serf_transporting_state, comparing sailor reversed-dir " << NameDirection[reverse_direction(Direction(dir))] << " to boat-wait serf dir " << NameDirection[game->get_serf_at_pos(new_pos)->get_walking_dir()];
        }
      }
      */

      if (type == Serf::TypeSailor 
          && game->get_serf_at_pos(new_pos)->get_state() == Serf::StateWaitForBoat
          && game->get_serf_at_pos(new_pos)->get_walking_dir() == reverse_direction(Direction(dir))){
          Log::Debug["serf"] << "a sailor inside handle_serf_transporting_state, next pos " << new_pos << ", setting sailor_pickup_serf to TRUE";
            sailor_pickup_serf = true;
      }

      if (!map->has_flag(map->move(pos, dir)) ||
          s.transporting.res != Resource::TypeNone ||
          s.transporting.wait_counter < 0 ||
          sailor_pickup_serf == true) {
        //if (type == Serf::TypeSailor){
        //  Log::Info["serf"] << "debug: a sailor in transporting state H1, because at least one of previous H0s true, calling change_direction to dir " << NameDirection[dir];
        //}
        // this says change_direction but it is really "check the next direction in the path"
        change_direction(dir, 1);
        return;
      }
      //if (type == Serf::TypeSailor){
      //  Log::Info["serf"] << "debug: a sailor in transporting state I";
      //}

      // a flag is at the next pos (or previous check would have returned)
      Flag *flag = game->get_flag_at_pos(map->move(pos, dir));
      Direction rev_dir = reverse_direction(dir);
      Flag *other_flag = flag->get_other_end_flag(rev_dir);
      Direction other_dir = flag->get_other_end_dir(rev_dir);

      if (flag->is_scheduled(rev_dir)) {
        //if (type == Serf::TypeSailor){
        //  Log::Info["serf"] << "debug: a sailor in transporting state J0 - flag->is_scheduled in rev_dir " << NameDirection[rev_dir];
        //}
        change_direction(dir, 1);
        return;
      }
      //if (type == Serf::TypeSailor){
      //  Log::Info["serf"] << "debug: a sailor in transporting state J";
      //}

      animation = 110 + s.walking.dir;
      counter = counter_from_animation[animation];
      s.walking.dir -= 6;

      if (flag->free_transporter_count(rev_dir) > 1) {
        //if (type == Serf::TypeSailor){
        //  Log::Info["serf"] << "debug: a sailor in transporting state K";
        //}
        s.transporting.wait_counter += 1;
        if (s.transporting.wait_counter > 3) {
          flag->transporter_to_serve(rev_dir);
          other_flag->transporter_to_serve(other_dir);
          s.transporting.wait_counter = -1;
        }
      } else {
        //if (type == Serf::TypeSailor){
        //  Log::Info["serf"] << "debug: a sailor in transporting state L";
        //}
        if (!other_flag->is_scheduled(other_dir)) {
          //if (type == Serf::TypeSailor){
          //  Log::Info["serf"] << "debug: a sailor in transporting state M is about to be set to idle StateIdleOnPath";
          //}
          /* TODO Don't use anim as state var */
          //Log::Info["serf"] << "debug: inside handle_serf_transporting_state, setting serf to idle StateIdleOnPath, tick was " << tick << ", s.walking.dir was " << s.walking.dir;
          tick = (tick & 0xff00) | (s.walking.dir & 0xff);
          //Log::Info["serf"] << "debug: inside handle_serf_transporting_state, setting serf to idle StateIdleOnPath, tick is now " << tick;
          set_state(StateIdleOnPath);
          s.idle_on_path.rev_dir = rev_dir;

          //
          // DEBUG stuck serf issue with StateWaitIdleOnPath 
          //  I have a savegame where this happens, and it is reproduced on game load
          //  the save file lists two serfs (indexes far apart) who are listed as occupying the same
          //  pos, both in StateWaitIdleOnPath, but one is listed as having state.flag = 1 (CASTLE flag, right?)
          //  while the other is listed as having the correct state.flag of the flag it is serving
          //  I suspect that a totally wrong serf is being woken to here, do to a bug in unrelated serf loops
          //  or some timing thing, not sure.  For now, note when any serf becomes idle_on_path with a flag index of 1
          //  as that sounds like it should never happen, or would be highly unlikely to happen normally
          // this unfortunately triggers for normal serfs operating adjacent to castle flag, don't have a good way to filter
          // that out yet, need to look for duplicate serfs
          /* can't use this because it triggers every time a castle-flag working serf goes in and out of idle
          if (flag->get_index() == 1){
            Log::Error["serf"] << "BUG DETECTION!  a serf with index " << get_index() << " and pos " << get_pos() << " is being set to StateIdleOnPath with flag index of 1!  this should only happen to Serfs working castle flag, see if that is the case";
            game->pause();
          }
          */

          s.idle_on_path.flag = flag->get_index();
          map->set_idle_serf(pos);
          map->set_serf_index(pos, 0);
          return;
        }
        //Log::Info["serf"] << "debug: a sailor in transporting state N";
      }
    }
  }
}      

void
Serf::enter_inventory() {
  //Log::Debug["serf.cc"] << " inside Serf::enter_inventory, for a serf with index " << get_index() << ", and type " << NameSerf[get_type()];
  game->get_map()->set_serf_index(pos, 0);
  Building *building = game->get_building_at_pos(pos);
  if (building == nullptr){
    Log::Warn["serf.cc"] << "inside Serf::enter_inventory, serf with index " << get_index() << " at pos " << get_pos() << " is trying to enter an INVENTORY, but Building is unexpectedly a nullptr! setting serf to Lost";
    //throw ExceptionFreeserf("inside Serf::enter_building, building is unexpectedly a nullptr!");
    set_lost_state();
    return;
  }

  if (building->get_inventory() == nullptr){
    Log::Warn["serf.cc"] << "inside Serf::enter_inventory, serf with index #" << get_index() << " at pos " << get_pos() << " is trying to enter an INVENTORY, but Building's INVENTORY object is unexpectedly a nullptr! setting serf to Lost";
    //throw ExceptionFreeserf("inside Serf::enter_building, building is unexpectedly a nullptr!");
    set_lost_state();
    return;
  }

  set_state(StateIdleInStock);
  /*serf->s.idle_in_stock.field_B = 0;
    serf->s.idle_in_stock.field_C = 0;*/
  s.idle_in_stock.inv_index = building->get_inventory()->get_index();
}

// NOTE - by the time enter_building is called, the Building already
//  has holder=true because Building::requested_serf_reached sets
//  holder to true, before enter_building is called (which results in
//  the serf moving to Serf::StateEnteringBuilding which triggers this
void
Serf::handle_serf_entering_building_state() {
  //Log::Debug["serf.cc"] << "start of Serf::handle_serf_entering_building_state, serf #" << index << " has type " << type;
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;
  if (counter < 0 || counter <= s.entering_building.slope_len) {
    if (game->get_map()->get_obj_index(pos) == 0 ||
        game->get_building_at_pos(pos)->is_burning()) {
      /* Burning */
      Log::Debug["serf"] << "inside Serf::handle_serf_entering_building_state B, building is burning, serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
      set_state(StateLost);
      s.lost.field_B = 0;
      counter = 0;
      return;
    }

    counter = s.entering_building.slope_len;
    PMap map = game->get_map();

    // support allowing recently Lost serfs to enter any nearby friendly building
    //  EXCEPT MINES, because they can deadlock when the miner paces outside waiting for food
    //if ( option_LostTransportersClearFaster && was_lost 
    // I noticed a crash when testing toggling this option mid-game.  I suspect that if this option is on and a serf is
    //   sent to a non-Inventory destination, disabling the option before they arrive is causing the crash.
    //   so, I am removing the check for the option here and only using the other reqs so if the option is disabled
    //   a serf should still clear if it was already on its way (even though it violates the option rule)
    // NOTE - I tried very hard to allow serfs to clear to mines that have miners striking but it was a mess
    //  the miner has to be defined in map->get_serf_at_pos(pos) to be drawn, and a Lost serf cannot enter
    //   the pos while the miner is there.  I tried various tricks like faking the pos, but it was a mess so I reverted it all
    //  somehow Delivering serfs handle this just fine, but I haven't figured out yet how they do it, I don't see any logic
    //   that allows them to be drawn relatively even if they still hold the flag pos.  Maybe they have a special animation id
    //   that does it that is not easy to identify, as it is just some modification of the walking animation & sprites
    if ( was_lost
      && (get_type() == Serf::TypeTransporter || get_type() == Serf::TypeGeneric || get_type() == Serf::TypeNone)){
      PMap map = game->get_map();
      if (map->has_building(pos)){
        Building *building = game->get_building_at_pos(pos);
        if (building->is_done() && !building->is_burning() &&
            building->get_type() != Building::TypeStock && building->get_type() != Building::TypeCastle){
          //Log::Debug["serf.cc"] << "a generic serf at pos " << pos << " has arrived in a non-inventory building of type " << NameBuilding[building->get_type()] << ", assuming this was a Lost Serf";
          //
          // try to find a way to nullify the serf, as deleting it seems hard.  "teleporting" it back to the castle might work also
          //
          //game->delete_serf(this);  // this crashes, didn't check why
          //enter_inventory();  // this crashes because it calls get_inventory() on attached building, but it isn't an Inventory
          //
          // this is needed so the serf stops appearing at the building entrance, but does it affect a serf already
          //  occupying this building???   so far with knight huts it causes no issue
          game->get_map()->set_serf_index(pos, 0);
          // this might not be necessary, but seems like a decent idea to make it clear this serf is in limbo
          // unset the was_lost bool so it doesn't stay forever
          was_lost = false;
          set_state(StateNull);
          // should we set the serf's pos to bad_map_pos or something?
          // might be easier to just teleport it back to the nearest inv
          // or find a way to delete it
          return;
        }
      }
    }

    switch (get_type()) {
      case TypeTransporter:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          map->set_serf_index(pos, 0);
          int flag_index = map->get_obj_index(map->move_down_right(pos));
          Flag *flag = game->get_flag(flag_index);

          /* Mark as inventory accepting resources and serfs. */
          flag->set_has_inventory();
          flag->set_accepts_resources(true);
          flag->set_accepts_serfs(true);

          set_state(StateWaitForResourceOut);
          counter = 63;
          //Log::Debug["serf"] << "inside handle_serf_entering_building_state, about to call set_type";
          set_type(TypeTransporterInventory);
        }
        break;
      case TypeSailor:
        enter_inventory();
        break;
      case TypeDigger:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          Building *building = game->get_building_at_pos(pos);
          
          /* removing AdvancedDemolition for now, see https://github.com/forkserf/forkserf/issues/180
          // handle option_AdvancedDemolition
          if (building->is_pending_demolition()){
            Log::Debug["serf.cc"] << "inside Serf::handle_serf_entering_building_state, building is_pending_demolition";
            // sanity check
            if (building->get_progress() > 1 || !option_AdvancedDemolition){
              Log::Warn["serf.cc"] << "inside Serf::handle_serf_entering_building_state, a Digger about to enter a building is being set to StateLost because the building is not eligible for demo!";
              Log::Warn["serf.cc"] << "inside Serf::handle_serf_entering_building_state, a Digger about to enter a building is being set to StateLost because the building is not eligible for demo! progress is " << building->get_progress();
              set_state(StateLost);
              break;
            }

            if (building->is_military() && building->is_active()){
              Log::Debug["serf.cc"] << "inside Serf::handle_serf_entering_building_state, a demo serf has arrived at an active military building at pos " << pos << ", calling evict_knights()";
              building->evict_knights();
            }

            Log::Debug["serf.cc"] << "inside Serf::handle_serf_entering_building_state, building is_pending_demolition, setting this Digger to StateExitBuildingForDemolition";
            counter = 0;  // this is the normal starting counter for a serf leaving a building, it is then set by counter += (slope * counter_from_animation[animation]) >> 5;
            tick = game->get_tick();
            int slope;
            Building *building = game->get_building_at_pos(pos);
            if (building == nullptr){
              Log::Warn["serf"] << "inside Serf::handle_serf_entering_building_state, got nullptr for building at pos " << pos << ", trying a safe default slope value";
              slope = 30;
            }else{
              slope = 31 - road_building_slope[building->get_type()];
            }
            PMap map = game->get_map();
            animation = get_walking_animation(map->get_height(pos) - map->get_height(map->move_down_right(pos)), Direction::DirectionDownRight, 0);
            //counter += (slope * counter_from_animation[animation]) >> 5;  // original value, walks to center of new pos (i.e. the building's flag pos)
            //counter += (slope * counter_from_animation[animation]) >> 6;  // only walk a fraction of the way, we want him to simply exit the building
            //counter += 5;
            // _________________________-
            //  NOW WITH USING THE WALK-BACK-AND-FORTH ANIMATION
            //   the serf doesn't need to walk as far, tweak the counter and height until it looks right
            // *******************************
            Log::Debug["serf.cc"] << "inside Serf::handle_serf_entering_building_state, counter = " << counter << ", slope = " << slope;
            set_state(StateExitBuildingForDemolition);
            break;
          }else{
            Log::Warn["serf.cc"] << "inside Serf::handle_serf_entering_building_state, building IS NOT pending_demolition";
          }
          */

          // handle normal "digging/leveling a new Large building site" behavior
          set_state(StateDigging);
          s.digging.h_index = 15;

          s.digging.dig_pos = 6;
          s.digging.target_h = building->get_level();
          s.digging.substate = 1;
        }
        break;
      case TypeBuilder:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          set_state(StateBuilding);
          animation = 98;  // this is the walking-back-and-forth animation used to show Builder constructing, and also Miner striking/refusing to work without food
          counter = 127;
          s.building.mode = 1;
          s.building.bld_index = map->get_obj_index(pos);
          s.building.material_step = 0;

          Building *building = game->get_building(s.building.bld_index);
          switch (building->get_type()) {
            case Building::TypeStock:
            case Building::TypeSawmill:
            case Building::TypeToolMaker:
            case Building::TypeFortress:
              s.building.material_step |= BIT(7);
              animation = 100;
              break;
            default:
              break;
          }
        }
        break;
      case TypeTransporterInventory:
        map->set_serf_index(pos, 0);
        set_state(StateWaitForResourceOut);
        counter = 63;
        break;
      case TypeLumberjack:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          map->set_serf_index(pos, 0);
          set_state(StatePlanningLogging);
        }
        break;
      case TypeSawmiller:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          map->set_serf_index(pos, 0);
          if (s.entering_building.field_B != 0) {
            Building *building = game->get_building_at_pos(pos);
            int flag_index = map->get_obj_index(map->move_down_right(pos));
            Flag *flag = game->get_flag(flag_index);
            flag->clear_flags();
            building->stock_init(1, Resource::TypeLumber, 8);
          }
          set_state(StateSawing);
          s.sawing.mode = 0;
        }
        break;
      case TypeStonecutter:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          map->set_serf_index(pos, 0);
          set_state(StatePlanningStoneCutting);
        }
        break;
      case TypeForester:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          map->set_serf_index(pos, 0);
          set_state(StatePlanningPlanting);
        }
        break;
      case TypeMiner:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          map->set_serf_index(pos, 0);
          Building *building = game->get_building_at_pos(pos);
          Building::Type bld_type = building->get_type();

          if (s.entering_building.field_B != 0) {
            building->start_activity();
            building->stop_playing_sfx();

            Flag *flag = game->get_flag_at_pos(map->move_down_right(pos));
            flag->clear_flags();
            building->stock_init(0, Resource::GroupFood, 8);
          }

          set_state(StateMining);
          s.mining.substate = 0;
          s.mining.deposit = (Map::Minerals)(4 - (bld_type -
                                                     Building::TypeStoneMine));
          /*s.mining.field_C = 0;*/
          s.mining.res = 0;
        }
        break;
      case TypeSmelter:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          map->set_serf_index(pos, 0);

          Building *building = game->get_building_at_pos(pos);

          if (s.entering_building.field_B != 0) {
            Flag *flag = game->get_flag_at_pos(map->move_down_right(pos));
            flag->clear_flags();
            building->stock_init(0, Resource::TypeCoal, 8);

            if (building->get_type() == Building::TypeSteelSmelter) {
              building->stock_init(1, Resource::TypeIronOre, 8);
            } else {
              building->stock_init(1, Resource::TypeGoldOre, 8);
            }
          }

          /* Switch to smelting state to begin work. */
          set_state(StateSmelting);

          if (building->get_type() == Building::TypeSteelSmelter) {
            s.smelting.type = 0;
          } else {
            s.smelting.type = -1;
          }

          s.smelting.mode = 0;
        }
        break;
      case TypeFisher:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          map->set_serf_index(pos, 0);
          set_state(StatePlanningFishing);
        }
        break;
      case TypePigFarmer:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          map->set_serf_index(pos, 0);

          if (s.entering_building.field_B != 0) {
            Building *building = game->get_building_at_pos(pos);
            Flag *flag = game->get_flag_at_pos(map->move_down_right(pos));

            // resource #1 is the pigs themselves
            // start with two pigs if option_ImprovedPigFarms is set
            //   how else could they reproduce?
            // NO LONGER STARTING WITH TWO PIGS since moving towards
            //  advanced farming which tweaks pig breeding rate
            //if (option_ImprovedPigFarms) {    //removing this as it turns out the default behavior for pig farms is to require almost no grain
            //  building->set_initial_res_in_stock(1, 2);
            //} else {
              // game default
              building->set_initial_res_in_stock(1, 1);
            //}

            flag->clear_flags();
            // resource #0 is the wheat that pigs consume (though it is bugged?)
            building->stock_init(0, Resource::TypeWheat, 8);

            set_state(StatePigFarming);
            s.pigfarming.mode = 0;
          } else {
            set_state(StatePigFarming);
            s.pigfarming.mode = 6;
            counter = 0;
          }
        }
        break;
      case TypeButcher:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          map->set_serf_index(pos, 0);

          if (s.entering_building.field_B != 0) {
            Building *building = game->get_building_at_pos(pos);
            Flag *flag = game->get_flag_at_pos(map->move_down_right(pos));
            flag->clear_flags();
            building->stock_init(0, Resource::TypePig, 8);
          }

          set_state(StateButchering);
          s.butchering.mode = 0;
        }
        break;
      case TypeFarmer:
        /*  debug example of new farmer state
        Verbose: [serf] serf 56 (FARMER): state WALKING -> ENTERING BUILDING (enter_building:1551)
        Verbose: [serf] serf 56 (FARMER): state ENTERING BUILDING -> PLANNING FARMING (handle_serf_entering_building_state:2437)
        Verbose: [serf] serf 56 (FARMER): state PLANNING FARMING -> READY TO LEAVE (handle_serf_planning_farming_state:4716)
        Verbose: [serf] planning farming: field spot found, dist -3, 4294967293.
        Verbose: [serf] serf 56 (FARMER): state READY TO LEAVE -> LEAVING BUILDING (leave_building:1581)
        Verbose: [serf] serf 56 (FARMER): state LEAVING BUILDING -> FREE WALKING (handle_serf_leaving_building_state:2621)
        Verbose: [serf] serf 56: free walking: dest -3, -3, move 0, -1
        Verbose: [serf] serf 56: free walking: dest -3, -2, move -1, -1
        Verbose: [serf] serf 56: free walking: dest -2, -1, move -1, 0
        Verbose: [serf] serf 56: free walking: dest -1, -1, move -1, -1
        Verbose: [serf] serf 56 (FARMER): state FREE WALKING -> FARMING (handle_serf_free_walking_state_dest_reached:3412)
        */
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          map->set_serf_index(pos, 0);
          set_state(StatePlanningFarming);
        }
        break;
      case TypeMiller:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          map->set_serf_index(pos, 0);

          if (s.entering_building.field_B != 0) {
            Building *building = game->get_building_at_pos(pos);
            Flag *flag = game->get_flag_at_pos(map->move_down_right(pos));
            flag->clear_flags();
            building->stock_init(0, Resource::TypeWheat, 8);
          }

          set_state(StateMilling);
          s.milling.mode = 0;
        }
        break;
      case TypeBaker:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          map->set_serf_index(pos, 0);

          if (s.entering_building.field_B != 0) {
            Building *building = game->get_building_at_pos(pos);
            Flag *flag = game->get_flag_at_pos(map->move_down_right(pos));
            flag->clear_flags();
            building->stock_init(0, Resource::TypeFlour, 8);
          }

          set_state(StateBaking);
          s.baking.mode = 0;
        }
        break;
      case TypeBoatBuilder:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          map->set_serf_index(pos, 0);
          if (s.entering_building.field_B != 0) {
            Building *building = game->get_building_at_pos(pos);
            Flag *flag = game->get_flag_at_pos(map->move_down_right(pos));
            flag->clear_flags();
            building->stock_init(0, Resource::TypePlank, 8);
          }

          set_state(StateBuildingBoat);
          s.building_boat.mode = 0;
        }
        break;
      case TypeToolmaker:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          map->set_serf_index(pos, 0);
          if (s.entering_building.field_B != 0) {
            Building *building = game->get_building_at_pos(pos);
            Flag *flag = game->get_flag_at_pos(map->move_down_right(pos));
            flag->clear_flags();
            building->stock_init(0, Resource::TypePlank, 8);
            building->stock_init(1, Resource::TypeSteel, 8);
          }

          set_state(StateMakingTool);
          s.making_tool.mode = 0;
        }
        break;
      case TypeWeaponSmith:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          map->set_serf_index(pos, 0);
          if (s.entering_building.field_B != 0) {
            Building *building = game->get_building_at_pos(pos);
            Flag *flag = game->get_flag_at_pos(map->move_down_right(pos));
            flag->clear_flags();
            building->stock_init(0, Resource::TypeCoal, 8);
            building->stock_init(1, Resource::TypeSteel, 8);
          }

          set_state(StateMakingWeapon);
          s.making_weapon.mode = 0;
        }
        break;
      case TypeGeologist:
        if (s.entering_building.field_B == -2) {
          enter_inventory();
        } else {
          set_state(StateLookingForGeoSpot); /* TODO Should never be reached */
          counter = 0;
        }
        break;
      case TypeGeneric: {
        map->set_serf_index(pos, 0);

        Building *building = game->get_building_at_pos(pos);
        Inventory *inventory = building->get_inventory();
        if (inventory == nullptr) {
          throw ExceptionFreeserf("Not inventory.");
        }
        inventory->serf_come_back();

        set_state(StateIdleInStock);
        s.idle_in_stock.inv_index = inventory->get_index();
        break;
      }
      case TypeKnight0:
      case TypeKnight1:
      case TypeKnight2:
      case TypeKnight3:
      case TypeKnight4:
        if (s.entering_building.field_B == -2) {
          // knight entering a Castle or Stock Inventory and becoming IdleInStock
          //Log::Debug["serf.cc"] << "inside Serf::handle_serf_entering_building_state(), knight entering a Castle or Stock Inventory and becoming IdleInStock";
          enter_inventory();
        } else {
          // knight entering a military building as a defender
          //Log::Debug["serf.cc"] << "inside Serf::handle_serf_entering_building_state(), knight entering a military building as a defender";
          Building *building = game->get_building_at_pos(pos);
          if (building->is_burning()) {
            Log::Debug["serf"] << "inside Serf::handle_serf_entering_building_state(), knight, building is burning, serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
            set_state(StateLost);
            counter = 0;
          } else {
            map->set_serf_index(pos, 0);

            // knight entering Castle as a Defender (NOT IdleInStock)
            if (building->has_inventory()) {
              set_state(StateDefendingCastle);
              counter = 6000;
              /* Prepend to knight list */
              s.defending.next_knight = building->get_holder_or_first_knight();
              building->set_holder_or_first_knight(get_index());
              game->get_player(building->get_owner())->increase_castle_knights();
              return;
            }

            // knight entering a Hut/Tower/Garrison as a defender
            building->requested_knight_arrived();

            Serf::State next_state = (Serf::State)-1;
            switch (building->get_type()) {
              case Building::TypeHut:
                next_state = StateDefendingHut;
                break;
              case Building::TypeTower:
                next_state = StateDefendingTower;
                break;
              case Building::TypeFortress:
                next_state = StateDefendingFortress;
                break;
              default:
                NOT_REACHED();
                break;
            }

            /* Switch to defending state */
            set_state(next_state);
            counter = 6000;

            // NOTE - by the time this code executes the Building already has
            //  holder set to true (from way-upstream function) so it is too late to check
            //  if this knight is occupying a new building or an already occupied one
            // ACTUALLY - because the upstream function only sets holder=true, I think 
            //  it can be tested if the holder_or_first_knight is valid and equal to this knight?
            // NO - the upstream function ALSO sets holder_or_first_knight to the serf index
            //  and the below code may be redundant 

            /* Prepend to knight list */
            s.defending.next_knight = building->get_holder_or_first_knight();
            building->set_holder_or_first_knight(get_index());
          }
        }
        break;
      case TypeDead:
        break;
      default:
        NOT_REACHED();
        break;
    }
  }
}

void
Serf::handle_serf_leaving_building_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  //Log::Debug["serf"] << "a serf with index " << get_index() << " leaving_building has values: dest " << s.leaving_building.dest << ", dest2 " << s.leaving_building.dest2 << ", field_B " << s.leaving_building.field_B << ", next_state " << s.leaving_building.next_state;

  if (counter < 0) {
    counter = 0;
    set_state(s.leaving_building.next_state);

    /* Set field_F to 0, do this for individual states if necessary */
    if (state == StateWalking) {
      int mode = s.leaving_building.field_B;
      unsigned int dest = s.leaving_building.dest;
      s.walking.dir1 = mode;
      s.walking.dest = dest;
      s.walking.wait_counter = 0;
    } else if (state == StateDropResourceOut) {
      unsigned int res = s.leaving_building.field_B;
      unsigned int res_dest = s.leaving_building.dest;
      s.move_resource_out.res = res;
      s.move_resource_out.res_dest = res_dest;
    } else if (state == StateFreeWalking || state == StateKnightFreeWalking ||
               state == StateStoneCutterFreeWalking) {
      int dist1 = s.leaving_building.field_B;
      int dist2 = s.leaving_building.dest;
      int neg_dist1 = s.leaving_building.dest2;
      int neg_dist2 = s.leaving_building.dir;
      s.free_walking.dist_col = dist1;
      s.free_walking.dist_row = dist2;
      s.free_walking.neg_dist1 = neg_dist1;
      s.free_walking.neg_dist2 = neg_dist2;
      s.free_walking.flags = 0;
    } else if (state == StateKnightPrepareDefending || state == StateScatter) {
      /* No state. */
    } else {
      Log::Debug["serf"] << "unhandled next state when leaving building.";
    }
  }
}

void
Serf::handle_serf_ready_to_enter_state() {
  //Log::Debug["serf.cc"] << "inside Serf::handle_serf_ready_to_enter_state";
  MapPos new_pos = game->get_map()->move_up_left(pos);

  if (game->get_map()->has_serf(new_pos)) {
    animation = 85;
    counter = 0;
    return;
  }

  //Log::Debug["serf.cc"] << "inside Serf::handle_serf_ready_to_enter_state, calling Serf::enter_building";
  enter_building(s.ready_to_enter.field_B, 0);
}

void
Serf::handle_serf_ready_to_leave_state() {
  tick = game->get_tick();
  counter = 0;

  PMap map = game->get_map();
  MapPos new_pos = map->move_down_right(pos);

  if ((map->get_serf_index(pos) != index && map->has_serf(pos))
      || map->has_serf(new_pos)) {
    animation = 82;
    counter = 0;
    return;
  }

  leave_building(0);
}

void
Serf::handle_serf_digging_state() {
  const int h_diff[] = {
    -1, 1, -2, 2, -3, 3, -4, 4,
    -5, 5, -6, 6, -7, 7, -8, 8
  };

  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  PMap map = game->get_map();

  while (counter < 0) {
    s.digging.substate -= 1;
    if (s.digging.substate < 0) {
      Log::Verbose["serf"] << "substate -1: wait for serf.";
      int d = s.digging.dig_pos;
      Direction dir = (Direction)((d == 0) ? DirectionUp : 6-d);
      MapPos new_pos = map->move(pos, dir);

      if (map->has_serf(new_pos)) {
        Serf *other_serf = game->get_serf_at_pos(new_pos);
        Direction other_dir;

        if (other_serf->is_waiting(&other_dir) &&
            other_dir == reverse_direction(dir) &&
            other_serf->switch_waiting(other_dir)) {
          /* Do the switch */
          other_serf->pos = pos;
          map->set_serf_index(other_serf->pos,
                                          other_serf->get_index());
          other_serf->animation =
            get_walking_animation(map->get_height(other_serf->pos) -
                                  map->get_height(new_pos),
                                  reverse_direction(dir), 1);
          other_serf->counter = counter_from_animation[other_serf->animation];

          if (d != 0) {
            animation =
                    get_walking_animation(map->get_height(new_pos) -
                                          map->get_height(pos), dir, 1);
          } else {
            animation = map->get_height(new_pos) - map->get_height(pos);
          }
        } else {
          counter = 127;
          s.digging.substate = 0;
          return;
        }
      } else {
        map->set_serf_index(pos, 0);
        if (d != 0) {
          animation =
                    get_walking_animation(map->get_height(new_pos) -
                                          map->get_height(pos), dir, 0);
        } else {
          animation = map->get_height(new_pos) - map->get_height(pos);
        }
      }

      map->set_serf_index(new_pos, get_index());
      pos = new_pos;
      s.digging.substate = 3;
      counter += counter_from_animation[animation];
    } else if (s.digging.substate == 1) {
      /* 34CD6: Change height, head back to center */
      int h = map->get_height(pos);
      h += (s.digging.h_index & 1) ? -1 : 1;
      Log::Verbose["serf"] << "substate 1: change height "
                           << ((s.digging.h_index & 1) ? "down." : "up.");
      map->set_height(pos, h);

      if (s.digging.dig_pos == 0) {
        s.digging.substate = 1;
      } else {
        Direction dir = reverse_direction((Direction)(6-s.digging.dig_pos));
        start_walking(dir, 32, 1);
      }
    } else if (s.digging.substate > 1) {
      Log::Verbose["serf"] << "substate 2: dig.";
      /* 34E89 */
      animation = 88 - (s.digging.h_index & 1);
      counter += 383;
    } else {
      /* 34CDC: Looking for a place to dig */
      Log::Verbose["serf"] << "substate 0: looking for place to dig "
                           << s.digging.dig_pos << ", " << s.digging.h_index;
      do {
        int h = h_diff[s.digging.h_index] + s.digging.target_h;
        if (s.digging.dig_pos >= 0 && h >= 0 && h < 32) {
          if (s.digging.dig_pos == 0) {
            int height = map->get_height(pos);
            if (height != h) {
              s.digging.dig_pos -= 1;
              continue;
            }
            /* Dig here */
            s.digging.substate = 2;
            // I'm not sure what animation # 87 vs 88 means here
            //  Initially I assumed it was "dig facing left" and "dig facing right"
            //  but I now think Digger can only face left while digging, no sprites
            //  exist for "dig facing right" and watching normal Digger action he
            //  always faces left while digging
            if (s.digging.h_index & 1) {
              animation = 87;
            } else {
              animation = 88;
            }
            counter += 383;
          } else {
            Direction dir = (Direction)(6-s.digging.dig_pos);
            MapPos new_pos = map->move(pos, dir);
            int new_height = map->get_height(new_pos);
            if (new_height != h) {
              s.digging.dig_pos -= 1;
              continue;
            }
            Log::Verbose["serf"] << " found at: " << s.digging.dig_pos << ".";
            /* Digging spot found */
            if (map->has_serf(new_pos)) {
              /* Occupied by other serf, wait */
              s.digging.substate = 0;
              animation = 87 - s.digging.dig_pos;
              counter = counter_from_animation[animation];
              return;
            }

            /* Go to dig there */
            start_walking(dir, 32, 1);
            s.digging.substate = 3;
          }
          break;
        }

        s.digging.dig_pos = 6;
        s.digging.h_index -= 1;
      } while (s.digging.h_index >= 0);

      if (s.digging.h_index < 0) {
        /* Done digging */
        Building *building =
                        game->get_building(game->get_map()->get_obj_index(pos));
        building->done_leveling();
        set_state(StateReadyToLeave);
        s.leaving_building.dest = 0;
        s.leaving_building.field_B = -2;
        s.leaving_building.dir = 0;
        s.leaving_building.next_state = StateWalking;
        handle_serf_ready_to_leave_state();  // TODO(jonls): why isn't a
                                             // state switch enough?
        return;
      }
    }
  }
}

void
Serf::handle_serf_building_state() {
  const int material_order[] = {
    0, 0, 0, 0, 0, 4, 0, 0,
    0, 0, 0x38, 2, 8, 2, 8, 4,
    4, 0xc, 0x14, 0x2c, 2, 0x1c, 0x1f0, 4,
    0, 0, 0, 0, 0, 0, 0, 0
  };

  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    Building *building = game->get_building(s.building.bld_index);
    if (s.building.mode < 0) {
      if (building->build_progress()) {
        counter = 0;
        set_state(StateFinishedBuilding);
        return;
      }

      s.building.counter -= 1;
      if (s.building.counter == 0) {
        s.building.mode = 1;
        animation = 98;  // this is the walking-back-and-forth animation used to show Builder constructing, and also Miner striking/refusing to work without food
        if (BIT_TEST(s.building.material_step, 7)) animation = 100;

        /* 353A5 */
        int material_step = s.building.material_step & 0xf;
        if (!BIT_TEST(material_order[building->get_type()], material_step)) {
          /* Planks */
          if (building->get_res_count_in_stock(0) == 0) {
            counter += 256;
            if (counter < 0) counter = 255;
            return;
          }

          building->plank_used_for_build();
        } else {
          /* Stone */
          if (building->get_res_count_in_stock(1) == 0) {
            counter += 256;
            if (counter < 0) counter = 255;
            return;
          }

          building->stone_used_for_build();
        }

        s.building.material_step += 1;
        s.building.counter = 8;
        s.building.mode = -1;
      }
    } else {
      if (s.building.mode == 0) {
        s.building.mode = 1;
        animation = 98;
        if (BIT_TEST(s.building.material_step, 7)) animation = 100;
      }

      /* 353A5: Duplicate code */
      int material_step = s.building.material_step & 0xf;
      if (!BIT_TEST(material_order[building->get_type()], material_step)) {
        /* Planks */
        if (building->get_res_count_in_stock(0) == 0) {
          counter += 256;
          if (counter < 0) counter = 255;
          return;
        }

        building->plank_used_for_build();
      } else {
        /* Stone */
        if (building->get_res_count_in_stock(1) == 0) {
          counter += 256;
          if (counter < 0) counter = 255;
          return;
        }

        building->stone_used_for_build();
      }

      s.building.material_step += 1;
      s.building.counter = 8;
      s.building.mode = -1;
    }

    int rnd = (game->random_int() & 3) + 102;
    if (BIT_TEST(s.building.material_step, 7)) rnd += 4;
    animation = rnd;
    counter += counter_from_animation[animation];
  }
}

void
Serf::handle_serf_building_castle_state() {
  tick = game->get_tick();

  Inventory *inventory = game->get_inventory(s.building_castle.inv_index);
  Building *building = game->get_building(inventory->get_building_index());

  if (building->build_progress()) { /* Finished */
    game->get_map()->set_serf_index(pos, 0);
    set_state(StateWaitForResourceOut);
  }
}

void
Serf::handle_serf_move_resource_out_state() {
  tick = game->get_tick();
  counter = 0;

  PMap map = game->get_map();
  if ((map->get_serf_index(pos) != index && map->has_serf(pos)) ||
    map->has_serf(map->move_down_right(pos))) {
    /* Occupied by serf, wait */
    animation = 82;
    counter = 0;
    return;
  }

  Flag *flag = game->get_flag_at_pos(map->move_down_right(pos));
  if (!flag->has_empty_slot()) {
    /* All resource slots at flag are occupied, wait */
    animation = 82;
    counter = 0;
    return;
  }

  unsigned int res = s.move_resource_out.res;
  unsigned int res_dest = s.move_resource_out.res_dest;
  Serf::State next_state = s.move_resource_out.next_state;

  leave_building(0);
  s.leaving_building.next_state = next_state;
  s.leaving_building.field_B = res;
  s.leaving_building.dest = res_dest;
}

void
Serf::handle_serf_wait_for_resource_out_state() {
  if (counter != 0) {
    uint16_t delta = game->get_tick() - tick;
    tick = game->get_tick();
    counter -= delta;

    if (counter >= 0) return;

    counter = 0;
  }

  unsigned int obj_index = game->get_map()->get_obj_index(pos);
  Building *building = game->get_building(obj_index);
  if (building == nullptr){
    Log::Warn["serf.cc"] << "this serf's Building is nullptr!";
    return;
  }
  Inventory *inventory = building->get_inventory();
  if (inventory == nullptr){
    Log::Warn["serf.cc"] << "this serf's Building's Inventory is nullptr!";
    return;
  }
  if (inventory->get_serf_queue_length() > 0 ||
      !inventory->has_resource_in_queue()) {
    return;
  }

  set_state(StateMoveResourceOut);
  Resource::Type res = Resource::TypeNone;
  int dest = 0;
  inventory->get_resource_from_queue(&res, &dest);
  s.move_resource_out.res = res + 1;
  s.move_resource_out.res_dest = dest;
  s.move_resource_out.next_state = StateDropResourceOut;

  /* why isn't a state switch enough? */
  /*handle_serf_move_resource_out_state(serf);*/
}

void
Serf::handle_serf_drop_resource_out_state() {
  Flag *flag = game->get_flag(game->get_map()->get_obj_index(pos));

  bool res = flag->drop_resource((Resource::Type)(s.move_resource_out.res-1),
                                 s.move_resource_out.res_dest);
  if (!res) {
    throw ExceptionFreeserf("Failed to drop resource.");
  }

  set_state(StateReadyToEnter);
  s.ready_to_enter.field_B = 0;
}

void
Serf::handle_serf_delivering_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  // adding this to support sending back excess resources (which is needed because of resource timeouts)
  bool attempted_delivery = false;

  while (counter < 0) {
    if (s.transporting.wait_counter != 0) {
      set_state(StateTransporting);
      s.transporting.wait_counter = 0;
      Flag *flag = game->get_flag(game->get_map()->get_obj_index(pos));
      transporter_move_to_flag(flag);
      return;
    }

    /* original logic, before I started messing with "send back excess unrequested resources"
    if (s.transporting.res != Resource::TypeNone) {
      Resource::Type res = s.transporting.res;
      s.transporting.res = Resource::TypeNone;
      Building *building =
                  game->get_building_at_pos(game->get_map()->move_up_left(pos));
      if (building == nullptr){
        Log::Warn["serf.cc"] << "inside handle_serf_delivering_state, building is nullptr";
      }else{
        building->requested_resource_delivered(res);
      }
    }
    */

    // new logic that sends back excess resources
    //  it actually results in the serf dropping the excess res at the building flag
    //  which looks a bit odd but may actually make sense if there are more important
    //  resources to be transported?
    if (s.transporting.res != Resource::TypeNone) {
      Resource::Type res = s.transporting.res;
      Building *building = game->get_building_at_pos(game->get_map()->move_up_left(pos));
      if (building == nullptr || building->requested_resource_delivered(res)){
        s.transporting.res = Resource::TypeNone;
      }else{
        Log::Debug["serf.cc"] << " serf's attempted delivery of resource type " << NameResource[res] << " was rejected, dropping it at flag and/or sending it back";
      }
    }

    animation = 4 + 9 - (animation - (3 + 10*9));
    s.transporting.wait_counter = -s.transporting.wait_counter - 1;
    counter += counter_from_animation[animation] >> 1;
  }
}

void
Serf::handle_serf_ready_to_leave_inventory_state() {
  tick = game->get_tick();
  counter = 0;

  //
  // first, check to see if the serf is blocked from exiting the Inventory now... (I think)
  //

  PMap map = game->get_map();
  // I think this is saying, "if the map agrees that there is a serf at this serf's pos, or down-right from this pos"...
  // which I think means "is there a blocking serf preventing this serf from exiting the inventory"
  if (map->has_serf(pos) || map->has_serf(map->move_down_right(pos))) {
    // I assume this means.. "wait until the blocking serf moves" ?
    animation = 82;
    counter = 0;
    return;
  }
  
  // additional check based on pyrdacor's freeserf.net:
  // "// Check if there is a serf that waits to approach the flag.
  //  // If so, we wait inside."
  Flag *inv_flag = game->get_flag_at_pos(map->move_down_right(pos));
  if (inv_flag != nullptr){
    for (Direction i : cycle_directions_cw()) {
      MapPos other_pos = map->move(inv_flag->get_position(), i);
      if (map->has_serf(other_pos)) {
        Serf *other_serf = game->get_serf_at_pos(other_pos);
        Direction other_dir = DirectionNone;
        if ( (other_serf->get_state() == Serf::StateWalking || other_serf->get_state() == Serf::StateTransporting || other_serf->get_state() == Serf::StateKnightEngagingBuilding) 
               && other_serf->is_waiting(&other_dir) ){
          if (! (other_serf->get_state() != Serf::StateTransporting
                 && other_serf->get_walking_dest() == inv_flag->get_position()
                 && other_serf->get_walking_dir() == reverse_direction(i))) {
            // setting this to Info so I can see if it ever happens
            //Log::Info["serf"] << "inside handle_serf_ready_to_leave_inventory_state(), pyrdacor check found a serf at pos " << other_pos << " waiting to approach this Inventory flag at pos " << inv_flag->get_position() << ", exiting serf must wait";
            animation = 82;
            counter = 0;
            return;
          }
        }
      }
    }
  } else {
    Log::Warn["serf"] << "inside handle_serf_ready_to_leave_inventory_state(), got nullptr when looking for inventory Flag down-right from pos " << pos << ", this might be a crash bug";
  }

  // mode appears to be the Dir that the flagsearch directs the serf to exit the Inventory Flag in,
  // if it is -1, that must mean something special, maybe flagsearch returned not found?  not sure
  // In any case, it seems like it is ultimately testing to see if this serf needs to wait before going out
  if (s.ready_to_leave_inventory.mode == -1) {
    Flag *flag = game->get_flag(s.ready_to_leave_inventory.dest);
    if (flag->has_building()) {
      Building *building = flag->get_building();
      if (map->has_serf(building->get_position())) {
        animation = 82;
        counter = 0;
        return;
      }
    }
  }

  //
  // serf is ready to leave
  //

  Inventory *inventory =
                      game->get_inventory(s.ready_to_leave_inventory.inv_index);
  inventory->serf_away();

  Serf::State next_state = StateWalking;
  // again, 'mode' seems to be the Dir returned from the Flagsearch done to direct the serf out in the first place
  //  I don't know what Dir -3 means or what StateScatter is used for
  if (s.ready_to_leave_inventory.mode == -3) {
    next_state = StateScatter;
  }

  int mode = s.ready_to_leave_inventory.mode;
  unsigned int dest = s.ready_to_leave_inventory.dest;  // again, this seems to be a Flag index NOT a MapPos

  //Log::Debug["serf"] << "inside Serf::handle_serf_ready_to_leave_inventory_state, calling leave_building(0) on this serf, with dest " << dest << " which should be a Flag with MapPos " << game->get_flag(dest)->get_position();
  leave_building(0);
  s.leaving_building.next_state = next_state;
  s.leaving_building.field_B = mode;
  s.leaving_building.dest = dest;
  s.leaving_building.dir = 0;
}

void
Serf::drop_resource(Resource::Type res) {
  Flag *flag = game->get_flag(game->get_map()->get_obj_index(pos));

  /* Resource is lost if no free slot is found */
  bool result = flag->drop_resource(res, 0);
  if (result) {
    Player *player = game->get_player(get_owner());
    player->increase_res_count(res);
  }else{
    Log::Debug["serf.cc"] << "inside drop_resource, a resource of type " << NameResource[res] << " was removed from the game because there was no free slot for flag at pos " << flag->get_position() << ".  This is normal game behavior";
  }
}

/* Serf will try to find the closest inventory from current position, either
   by following the roads if it is already at a flag, otherwise it will try
   to find a flag nearby. */
void
Serf::find_inventory() {
  PMap map = game->get_map();
  if (map->has_flag(pos)) {
    Flag *flag = game->get_flag(map->get_obj_index(pos));
    if ((flag->land_paths() != 0 ||
         (flag->has_inventory() && flag->accepts_serfs())) &&
          map->get_owner(pos) == get_owner()) {
      set_state(StateWalking);
      s.walking.dir1 = -2;
      s.walking.dest = 0;
      s.walking.dir = 0;
      counter = 0;
      return;
    }
  }

  //Log::Debug["serf"] << "inside find_inventory, a serf at pos " << get_pos() << " is being set to StateLost, because an Inventory to return to was not found";

  //debug
  //if (game->get_flag(recent_dest) != nullptr){
  //  Log::Debug["serf"] << "inside find_inventory, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which has pos " << game->get_flag(recent_dest)->get_position();
  //}else{
  //  Log::Debug["serf"] << "inside find_inventory, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which is a nullptr";
  //}

  Log::Debug["serf"] << "inside Serf::find_inventory(), serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
  set_state(StateLost);
  s.lost.field_B = 0;
  counter = 0;
}

void
Serf::handle_serf_free_walking_state_dest_reached() {
  if (s.free_walking.neg_dist1 == -128 &&
      s.free_walking.neg_dist2 < 0) {
    //NOTE - I originally left this code in place so that if a serf was already on way to a non-Inventory building to clear
    //  but option_LostTransportersClearFaster was off (either because of game load, or simply turned off), the serf would 
    //  still be able to exit into the non-Inv building to avoid contention and possibly crash bugs.  But, what I am noticing
    //  is that Lost generic serfs that simply *approach* a building flag by random chance will be able to enter it even
    //  though option_LostTransportersClearFaster was never turned on.  So, seeing what happens if a check for the option
    //  is actually enforced... hoping that if it is turned off after serf on way that they will simply become Lost again
    //  with a new destination once they are rejected from entering
    // ah, I am thinking this will be okay because Serf::handle_serf_entering_building_state allows it without the option set
    //  but checking it here should prevent the serf from attempting to enter at all
    //
    //  NOOOO this was all wrong, was_lost was often being set true because the was_lost bool was not properly initialized to zero
    //    and was getting random values that returned true even if option_LostTransportersClearFaster was never once enabled!
    ///  fixed that bug, see how this works now...
    //
    //Log::Info["serf"] << "debug : Serf::handle_serf_free_walking_state_dest_reached s.free_walking.neg_dist1: " << s.free_walking.neg_dist1 << ", s.free_walking.neg_dist2: " << s.free_walking.neg_dist2;
    // support allowing Lost serfs to enter any nearby friendly building
    //if (option_LostTransportersClearFaster && (this->get_type() == Serf::TypeTransporter || this->get_type() == Serf::TypeGeneric || this->get_type() == Serf::TypeNone) && was_lost){
    if ((this->get_type() == Serf::TypeTransporter || this->get_type() == Serf::TypeGeneric || this->get_type() == Serf::TypeNone) && was_lost){
      PMap map = game->get_map();
      MapPos upleft_pos = map->move_up_left(pos);
      if (map->has_building(upleft_pos)){
        Building *building = game->get_building_at_pos(upleft_pos);
        if (building->is_done() && !building->is_burning() &&
            // disallow Inventory buildings because they are handled by the default function
            building->get_type() != Building::TypeStock && building->get_type() != Building::TypeCastle &&
            // disallow mines because they can deadlock when miner runs out of food and holds the pos
            (building->get_type() < Building::TypeStoneMine || building->get_type() > Building::TypeGoldMine)){
          //Log::Debug["serf"] << "a generic serf of type " << this->get_type() << " at pos " << pos << " is about to enter a non-inventory building of type " << NameBuilding[building->get_type()] << " at pos " << upleft_pos << ", assuming this was a Lost Serf and letting him in (to be disappeared)";
          set_state(StateReadyToEnter);
          s.ready_to_enter.field_B = 0;
          counter = 0;
          return;
        }
      }
    }
    // otherwise, find nearest inventory and send the serf there
    find_inventory();
    return;
  }

  PMap map = game->get_map();
  switch (get_type()) {
    case TypeLumberjack:
      if (s.free_walking.neg_dist1 == -128) {
        if (s.free_walking.neg_dist2 > 0) {
          drop_resource(Resource::TypeLumber);
        }

        set_state(StateReadyToEnter);
        s.ready_to_enter.field_B = 0;
        counter = 0;
      } else {
        s.free_walking.dist_col = s.free_walking.neg_dist1;
        s.free_walking.dist_row = s.free_walking.neg_dist2;
        int obj = map->get_obj(pos);
        if (obj >= Map::ObjectTree0 &&
            obj <= Map::ObjectPine7) {
          set_state(StateLogging);
          s.free_walking.neg_dist1 = 0;
          s.free_walking.neg_dist2 = 0;
          if (obj < 16) s.free_walking.neg_dist1 = -1;
          animation = 116;
          counter = counter_from_animation[animation];
        } else {
          /* The expected tree is gone */
          s.free_walking.neg_dist1 = -128;
          s.free_walking.neg_dist2 = 0;
          s.free_walking.flags = 0;
          counter = 0;
        }
      }
      break;
    case TypeStonecutter:
      if (s.free_walking.neg_dist1 == -128) {
        if (s.free_walking.neg_dist2 > 0) {
          drop_resource(Resource::TypeStone);
        }

        set_state(StateReadyToEnter);
        s.ready_to_enter.field_B = 0;
        counter = 0;
      } else {
        s.free_walking.dist_col = s.free_walking.neg_dist1;
        s.free_walking.dist_row = s.free_walking.neg_dist2;

        MapPos new_pos = map->move_up_left(pos);
        int obj = map->get_obj(new_pos);
        if (!map->has_serf(new_pos) &&
            obj >= Map::ObjectStone0 &&
            obj <= Map::ObjectStone7) {
          counter = 0;
          start_walking(DirectionUpLeft, 32, 1);

          set_state(StateStoneCutting);
          s.free_walking.neg_dist2 = counter >> 2;
          s.free_walking.neg_dist1 = 0;
        } else {
          /* The expected stone is gone or unavailable */
          s.free_walking.neg_dist1 = -128;
          s.free_walking.neg_dist2 = 0;
          s.free_walking.flags = 0;
          counter = 0;
        }
      }
      break;
    case TypeForester:
      if (s.free_walking.neg_dist1 == -128) {
        set_state(StateReadyToEnter);
        s.ready_to_enter.field_B = 0;
        counter = 0;
      } else {
        s.free_walking.dist_col = s.free_walking.neg_dist1;
        s.free_walking.dist_row = s.free_walking.neg_dist2;
        if (map->get_obj(pos) == Map::ObjectNone) {
          set_state(StatePlanting);
          s.free_walking.neg_dist2 = 0;
          animation = 121;
          counter = counter_from_animation[animation];
        } else {
          /* The expected free space is no longer empty */
          s.free_walking.neg_dist1 = -128;
          s.free_walking.neg_dist2 = 0;
          s.free_walking.flags = 0;
          counter = 0;
        }
      }
      break;
    case TypeFisher:
      if (s.free_walking.neg_dist1 == -128) {
        if (s.free_walking.neg_dist2 > 0) {
          drop_resource(Resource::TypeFish);
        }

        set_state(StateReadyToEnter);
        s.ready_to_enter.field_B = 0;
        counter = 0;
      } else {
        s.free_walking.dist_col = s.free_walking.neg_dist1;
        s.free_walking.dist_row = s.free_walking.neg_dist2;

        int a = -1;
        if (map->paths(pos) == 0) {
          if (map->type_down(pos) <= Map::TerrainWater3 &&
              map->type_up(map->move_up_left(pos)) >= Map::TerrainGrass0) {
            a = 132;
          } else if (
              map->type_down(map->move_left(pos)) <= Map::TerrainWater3 &&
              map->type_up(map->move_up(pos)) >= Map::TerrainGrass0) {
            a = 131;
          }
        }

        if (a < 0) {
          /* Cannot fish here after all. */
          s.free_walking.neg_dist1 = -128;
          s.free_walking.neg_dist2 = 0;
          s.free_walking.flags = 0;
          counter = 0;
        } else {
          set_state(StateFishing);
          s.free_walking.neg_dist1 = 0;
          s.free_walking.neg_dist2 = 0;
          s.free_walking.flags = 0;
          animation = a;
          counter = counter_from_animation[a];
        }
      }
      break;
    case TypeFarmer:
      if (s.free_walking.neg_dist1 == -128) {
        if (s.free_walking.neg_dist2 > 0) {
          drop_resource(Resource::TypeWheat);
        }

        set_state(StateReadyToEnter);
        s.ready_to_enter.field_B = 0;
        counter = 0;
      } else {
        s.free_walking.dist_col = s.free_walking.neg_dist1;
        s.free_walking.dist_row = s.free_walking.neg_dist2;

        if (option_AdvancedFarming){
                
          if (season != 3 &&  // don't even try harvesting in winter
              map->get_obj(pos) == Map::ObjectSeeds5 ||
              (map->get_obj(pos) >= Map::ObjectField0 &&
              map->get_obj(pos) <= Map::ObjectField5)) {
            /* Existing field. */
            // harvest the field
            animation = 136;
            s.free_walking.neg_dist1 = 1;
            counter = counter_from_animation[animation];
          } else if (season != 1 && !(season == 3 && subseason >= 6) && !(season == 0 && subseason >= 12) && // don't sow in summer or mid/late winter or late spring
                    map->get_obj(pos) == Map::ObjectNone &&
                    map->paths(pos) == 0) {
            /* Empty space. */
            // sow a new field
            animation = 135;
            s.free_walking.neg_dist1 = 0;
            counter = counter_from_animation[animation];
          } else {
            /* Space not available after all. */
            s.free_walking.neg_dist1 = -128;
            s.free_walking.neg_dist2 = 0;
            s.free_walking.flags = 0;
            counter = 0;
            break;
          }
        }else{  
                // normal farming
          if (map->get_obj(pos) == Map::ObjectSeeds5 ||
              (map->get_obj(pos) >= Map::ObjectField0 &&
              map->get_obj(pos) <= Map::ObjectField5)) {
            /* Existing field. */
            animation = 136;
            s.free_walking.neg_dist1 = 1;
            counter = counter_from_animation[animation];
          } else if (map->get_obj(pos) == Map::ObjectNone &&
                    map->paths(pos) == 0) {
            /* Empty space. */
            animation = 135;
            s.free_walking.neg_dist1 = 0;
            counter = counter_from_animation[animation];
          } else {
            /* Space not available after all. */
            s.free_walking.neg_dist1 = -128;
            s.free_walking.neg_dist2 = 0;
            s.free_walking.flags = 0;
            counter = 0;
            break;
          }
        } //if option_AdvancedFarming

        set_state(StateFarming);
        s.free_walking.neg_dist2 = 0;
      }
      break;
    case TypeGeologist:
      if (s.free_walking.neg_dist1 == -128) {
        if (map->get_obj(pos) == Map::ObjectFlag &&
            map->get_owner(pos) == get_owner()) {
          set_state(StateLookingForGeoSpot);
          counter = 0;
        } else {
          Log::Warn["serf"] << "inside handle_free_walking_state_dest_reached(), a Geologist at pos " << get_pos() << " is being set to StateLost! because the flag he was sent to and is working around was deleted?";
          Log::Debug["serf"] << "inside Serf::handle_free_walking_state_dest_reached(), geologist serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
          set_state(StateLost);
          s.lost.field_B = 0;
          counter = 0;
        }
      } else {
        s.free_walking.dist_col = s.free_walking.neg_dist1;
        s.free_walking.dist_row = s.free_walking.neg_dist2;
        if (map->get_obj(pos) == Map::ObjectNone) {
          set_state(StateSamplingGeoSpot);
          s.free_walking.neg_dist1 = 0;
          animation = 141;
          counter = counter_from_animation[animation];
        } else {
          /* Destination is not a free space after all. */
          s.free_walking.neg_dist1 = -128;
          s.free_walking.neg_dist2 = 0;
          s.free_walking.flags = 0;
          counter = 0;
        }
      }
      break;
    case TypeKnight0:
    case TypeKnight1:
    case TypeKnight2:
    case TypeKnight3:
    case TypeKnight4:
      if (s.free_walking.neg_dist1 == -128) {
        find_inventory();
      } else {
        set_state(StateKnightOccupyEnemyBuilding);
        counter = 0;
      }
      break;
    default:
      find_inventory();
      break;
  }
}

void
Serf::handle_serf_free_walking_switch_on_dir(Direction dir) {
  // A suitable direction has been found; walk.
  if (dir < DirectionRight) {
    throw ExceptionFreeserf("Wrong direction.");
  }
  int dx = ((dir < 3) ? 1 : -1)*((dir % 3) < 2);
  int dy = ((dir < 3) ? 1 : -1)*((dir % 3) > 0);

  Log::Verbose["serf"] << "serf " << index << ": free walking: dest "
                       << s.free_walking.dist_col << ", "
                       << s.free_walking.dist_row
                       << ", move " << dx << ", " << dy;

  s.free_walking.dist_col -= dx;
  s.free_walking.dist_row -= dy;

  start_walking(dir, 32, 1);

  if (s.free_walking.dist_col == 0 && s.free_walking.dist_row == 0) {
    /* Arriving to destination */
    s.free_walking.flags = BIT(3);
  }
}

void
Serf::handle_serf_free_walking_switch_with_other() {
  /* No free position can be found. Switch with
     other serf. */
  MapPos new_pos = 0;
  Direction dir = DirectionNone;
  Serf *other_serf = NULL;
  PMap map = game->get_map();
  for (Direction i : cycle_directions_cw()) {
    new_pos = map->move(pos, i);
    if (map->has_serf(new_pos)) {
      other_serf = game->get_serf_at_pos(new_pos);
      Direction other_dir;

      if (other_serf->is_waiting(&other_dir) &&
          other_dir == reverse_direction(i) &&
          other_serf->switch_waiting(other_dir)) {
        dir = i;
        break;
      }
    }
  }

  if (dir > DirectionNone) {
    int dx = ((dir < 3) ? 1 : -1)*((dir % 3) < 2);
    int dy = ((dir < 3) ? 1 : -1)*((dir % 3) > 0);

    Log::Verbose["serf"] << "free walking (switch): dest "
                         << s.free_walking.dist_col << ", "
                         << s.free_walking.dist_row << ", move "
                         << dx << ", " << dy;

    s.free_walking.dist_col -= dx;
    s.free_walking.dist_row -= dy;

    if (s.free_walking.dist_col == 0 &&
        s.free_walking.dist_row == 0) {
      /* Arriving to destination */
      s.free_walking.flags = BIT(3);
    }

    /* Switch with other serf. */
    map->set_serf_index(pos, other_serf->index);
    map->set_serf_index(new_pos, index);

    other_serf->animation = get_walking_animation(map->get_height(pos) -
                                               map->get_height(other_serf->pos),
                                          reverse_direction((Direction)dir), 1);
    animation = get_walking_animation(map->get_height(new_pos) -
                                      map->get_height(pos),
                                      (Direction)dir, 1);

    other_serf->counter = counter_from_animation[other_serf->animation];
    counter = counter_from_animation[animation];

    other_serf->pos = pos;
    pos = new_pos;
  } else {
    animation = 82;
    counter = counter_from_animation[animation];
  }
}

bool
Serf::can_pass_map_pos(MapPos test_pos) {
  return Map::map_space_from_obj[game->get_map()->get_obj(test_pos)] <=
           Map::SpaceSemipassable;
}

int
Serf::handle_free_walking_follow_edge() {
  const Direction dir_from_offset[] = {
    DirectionUpLeft, DirectionUp,   DirectionNone,
    DirectionLeft,   DirectionNone, DirectionRight,
    DirectionNone,   DirectionDown, DirectionDownRight
  };

  /* Follow right-hand edge */
  const Direction dir_right_edge[] = {
    DirectionDown, DirectionDownRight, DirectionRight, DirectionUp,
    DirectionUpLeft, DirectionLeft, DirectionLeft, DirectionDown,
    DirectionDownRight, DirectionRight, DirectionUp, DirectionUpLeft,
    DirectionUpLeft, DirectionLeft, DirectionDown, DirectionDownRight,
    DirectionRight, DirectionUp, DirectionUp, DirectionUpLeft, DirectionLeft,
    DirectionDown, DirectionDownRight, DirectionRight, DirectionRight,
    DirectionUp, DirectionUpLeft, DirectionLeft, DirectionDown,
    DirectionDownRight, DirectionDownRight, DirectionRight, DirectionUp,
    DirectionUpLeft, DirectionLeft, DirectionDown,
  };

  /* Follow left-hand edge */
  const Direction dir_left_edge[] = {
    DirectionUpLeft, DirectionUp, DirectionRight, DirectionDownRight,
    DirectionDown, DirectionLeft, DirectionUp, DirectionRight,
    DirectionDownRight, DirectionDown, DirectionLeft, DirectionUpLeft,
    DirectionRight, DirectionDownRight, DirectionDown, DirectionLeft,
    DirectionUpLeft, DirectionUp, DirectionDownRight, DirectionDown,
    DirectionLeft, DirectionUpLeft, DirectionUp, DirectionRight, DirectionDown,
    DirectionLeft, DirectionUpLeft, DirectionUp, DirectionRight,
    DirectionDownRight, DirectionLeft, DirectionUpLeft, DirectionUp,
    DirectionRight, DirectionDownRight, DirectionDown,
  };

  int water = (state == StateFreeSailing);
  int dir_index = -1;
  const Direction *dir_arr = NULL;

  // it seems that even serfs that are not blocked by any large obstacles like lakes are
  //  very often put into this follow_edge state, possibly all of them?  this isn't useful to debug that issue

  // "what if I just flip it?"
  //flipping the left/right follow seems to fix it in all the cases I tried,
  //  except it results in a stuck serf sometimes. 
  // I think it isn't possible to just flip it, when a serf encounters water, the left or right hand direction
  //  he should follow should be determined by a A* pathfind call and not by whatever the internal logic is
  //  that just tries to "get a step closer"
  //
  //here is example of serf being stuck after flipping left/right hand arrays:
  //freewalking-edge-test.save.gz
  //
  if (BIT_TEST(s.free_walking.flags, 3)) {
    // Follow left-hand edge
    //Log::Debug["serf.cc"] << "serf with index # " << get_index() << " in FreeWalking follow_edge state is following LEFTHAND edge";
    dir_arr = dir_left_edge;
    dir_index = (s.free_walking.flags & 7)-1;
  } else {
    // Follow right-hand edge
    //Log::Debug["serf.cc"] << "serf with index # " << get_index() << " in FreeWalking follow_edge state is following RIGHTHAND edge";
    dir_arr = dir_right_edge;
    dir_index = (s.free_walking.flags & 7)-1;
  }

  int d1 = s.free_walking.dist_col;
  int d2 = s.free_walking.dist_row;

  /* Check if dest is only one step away. */
  if (!water && abs(d1) <= 1 && abs(d2) <= 1 &&
      dir_from_offset[(d1+1) + 3*(d2+1)] > DirectionNone) {
    /* Convert offset in two dimensions to
       direction variable. */
    Direction dir = dir_from_offset[(d1+1) + 3*(d2+1)];
    MapPos new_pos = game->get_map()->move(pos, dir);

    if (!can_pass_map_pos(new_pos)) {
      if (state != StateKnightFreeWalking && s.free_walking.neg_dist1 != -128) {
        s.free_walking.dist_col += s.free_walking.neg_dist1;
        s.free_walking.dist_row += s.free_walking.neg_dist2;
        s.free_walking.neg_dist1 = 0;
        s.free_walking.neg_dist2 = 0;
        s.free_walking.flags = 0;
        animation = 82;
        counter = counter_from_animation[animation];
      } else {
        Log::Warn["serf"] << "inside handle_free_walking_follow_edge(), a serf at pos " << get_pos() << " is being set to StateLost! he cannot pass map_pos " << new_pos << " and various other checks failed?";

        //debug
        //if (game->get_flag(recent_dest) != nullptr){
        //  Log::Debug["serf"] << "inside handle_free_walking_state_follow_edge, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which has pos " << game->get_flag(recent_dest)->get_position();
        //}else{
        //  Log::Debug["serf"] << "inside handle_free_walking_state_follow_edge, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which is a nullptr";
        //}

        Log::Debug["serf"] << "inside Serf::handle_free_walking_state_follow_edge(), serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
        set_state(StateLost);
        s.lost.field_B = 0;
        counter = 0;
      }
      return 0;
    }

    if (state == StateKnightFreeWalking && s.free_walking.neg_dist1 != -128 &&
        game->get_map()->has_serf(new_pos)) {
      /* Wait for other serfs */
      s.free_walking.flags = 0;
      animation = 82;
      counter = counter_from_animation[animation];
      return 0;
    }
  }

  const Direction *a0 = &dir_arr[6*dir_index];
  Direction i0 = DirectionNone;
  Direction dir = DirectionNone;
  PMap map = game->get_map();
  for (Direction i : cycle_directions_cw()) {
    MapPos new_pos = map->move(pos, a0[i]);
    if (((water && map->get_obj(new_pos) == 0) ||
         (!water && !map->is_in_water(new_pos) &&
          can_pass_map_pos(new_pos))) && !map->has_serf(new_pos)) {
      dir = a0[i];
      i0 = i;
      break;
    }
  }

  if (i0 > DirectionNone) {
    int upper = ((s.free_walking.flags >> 4) & 0xf) + i0 - 2;
    if (i0 < 2 && upper < 0) {
      s.free_walking.flags = 0;
      handle_serf_free_walking_switch_on_dir(dir);
      return 0;
    } else if (i0 > 2 && upper > 15) {
      s.free_walking.flags = 0;
    } else {
      int dir_index = dir+1;
      s.free_walking.flags = (upper << 4) |
                             (s.free_walking.flags & 0x8) | dir_index;
      handle_serf_free_walking_switch_on_dir(dir);
      return 0;
    }
  } else {
    int dir_index = 0;
    s.free_walking.flags = (s.free_walking.flags & 0xf8) | dir_index;
    s.free_walking.flags &= ~BIT(3);
    handle_serf_free_walking_switch_with_other();
    return 0;
  }

  return -1;
}

void
Serf::handle_free_walking_common() {

  const Direction dir_from_offset[] = {
    DirectionUpLeft, DirectionUp,   DirectionNone,
    DirectionLeft,   DirectionNone, DirectionRight,
    DirectionNone,   DirectionDown, DirectionDownRight
  };
  
  //
  // EXPLANATION OF FREE WALKING POS TRACKING -
  //
  //  When a serf is first set to a FreeWalking state:
  //     The dist_col and dist_row are set to the coord of the destination pos, relative to the pos the serf
  //      is in when it was placed into FreeWalking state (calling "start pos" here) 
  //     The neg_dist and neg_dist2 are set to the return coordinates, relative to the *destination pos*
  //      This is how the serf remembers the pos to return to once he reaches his destination
  //  As a serf travels from start pos to dest pos:
  //     the dist_col and dist_row coordinates are constantly updated as the serf changes pos,
  //      so they always indicate the current offset for the serf to reach the dest pos
  //     The neg_dist and neg_dist2 are *NOT* updated as the serf moves!  They are only relevant once destination reached
  //  When the serf reaches his destination:
  //     The dist_col and dist_row, which originally represented the offset to the dest pos, are replaced
  //      with the values from neg_dist and neg_dist2.  These become the new dest pos.
  //     The neg_dist and neg_dist2 values are set to -128, 0, or possibly some other values, to indicate
  //      that the serf has no return pos and to indicate some other state/serf specific information(?)
  //

  /* Directions for moving forwards. Each of the 12 lines represents
     a general direction as shown in the diagram below.
     The lines list the local directions in order of preference for that
     general direction.

     *         1    0
     *    2   ________   11
     *       /\      /\
     *      /  \    /  \
     *  3  /    \  /    \  10
     *    /______\/______\
     *    \      /\      /
     *  4  \    /  \    /  9
     *      \  /    \  /
     *       \/______\/
     *    5             8
     *         6    7
     */
  const Direction dir_forward[] = {
    DirectionUp, DirectionUpLeft, DirectionRight, DirectionLeft,
    DirectionDownRight, DirectionDown, DirectionUpLeft, DirectionUp,
    DirectionLeft, DirectionRight, DirectionDown, DirectionDownRight,
    DirectionUpLeft, DirectionLeft, DirectionUp, DirectionDown, DirectionRight,
    DirectionDownRight, DirectionLeft, DirectionUpLeft, DirectionDown,
    DirectionUp, DirectionDownRight, DirectionRight, DirectionLeft,
    DirectionDown, DirectionUpLeft, DirectionDownRight, DirectionUp,
    DirectionRight, DirectionDown, DirectionLeft, DirectionDownRight,
    DirectionUpLeft, DirectionRight, DirectionUp, DirectionDown,
    DirectionDownRight, DirectionLeft, DirectionRight, DirectionUpLeft,
    DirectionUp, DirectionDownRight, DirectionDown, DirectionRight,
    DirectionLeft, DirectionUp, DirectionUpLeft, DirectionDownRight,
    DirectionRight, DirectionDown, DirectionUp, DirectionLeft, DirectionUpLeft,
    DirectionRight, DirectionDownRight, DirectionUp, DirectionDown,
    DirectionUpLeft, DirectionLeft, DirectionRight, DirectionUp,
    DirectionDownRight, DirectionUpLeft, DirectionDown, DirectionLeft,
    DirectionUp, DirectionRight, DirectionUpLeft, DirectionDownRight,
    DirectionLeft, DirectionDown
  };

  int water = (state == StateFreeSailing);

  if (BIT_TEST(s.free_walking.flags, 3) &&
      (s.free_walking.flags & 7) == 0) {
    /* Destination reached */
    handle_serf_free_walking_state_dest_reached();
    return;
  }

  if ((s.free_walking.flags & 7) != 0) {
    /* Obstacle encountered, follow along the edge */
    // which edge?  if BIT_TEST(3) it goes left, if not, goes right
    int r = handle_free_walking_follow_edge();
    // this happens WAY more often than I initially suspected, basically ANY obstacle, not just large ones like lakes
    //Log::Debug["serf.cc"] << "found a serf with index # " << get_index() << " in FreeWalking follow_edge state, auto-marking on debug_serf display";
    //game->get_debug_mark_serf()->push_back(get_index());
    if (r >= 0) return;
  }

  /* Move fowards */
  int dir_index = -1;
  int d1 = s.free_walking.dist_col;
  int d2 = s.free_walking.dist_row;
  if (d1 < 0) {
    if (d2 < 0) {
      if (-d2 < -d1) {
        if (-2*d2 < -d1) {
          dir_index = 3;
        } else {
          dir_index = 2;
        }
      } else {
        if (-d2 < -2*d1) {
          dir_index = 1;
        } else {
          dir_index = 0;
        }
      }
    } else {
      if (d2 >= -d1) {
        dir_index = 5;
      } else {
        dir_index = 4;
      }
    }
  } else {
    if (d2 < 0) {
      if (-d2 >= d1) {
        dir_index = 11;
      } else {
        dir_index = 10;
      }
    } else {
      if (d2 < d1) {
        if (2*d2 < d1) {
          dir_index = 9;
        } else {
          dir_index = 8;
        }
      } else {
        if (d2 < 2*d1) {
          dir_index = 7;
        } else {
          dir_index = 6;
        }
      }
    }
  }

  /* Try to move directly in the preferred direction */
  const Direction *a0 = &dir_forward[6*dir_index];
  Direction dir = (Direction)a0[0];
  PMap map = game->get_map();
  MapPos new_pos = map->move(pos, dir);
  //
  // NEW LOGIC TO PREVENT SERFS FROM GETTING CONFUSED WHEN ENCOUNTERING BODIES OF WATER
  //
  // if this is NOT a sailor, but the next pos is in water...
  bool found_water_obstacle = false;
  if (!water && map->is_in_water(new_pos)){
    //trigger new logic here
    Log::Debug["serf.cc"] << "inside Serf::handle_free_walking_common, a free-walking serf with index #" << this->get_index() << " and pos " << this->get_pos() << " has encountered water!  ADD LOGIC TO HELP HIM REACH HIS DESTINATION!";
    found_water_obstacle = true;
  }
  if (((water && map->get_obj(new_pos) == 0) ||
       (!water && !map->is_in_water(new_pos) &&
        can_pass_map_pos(new_pos))) &&
      !map->has_serf(new_pos)) {
    handle_serf_free_walking_switch_on_dir(dir);
    return;
  }

  /* Check if dest is only one step away. */
  if (!water && abs(d1) <= 1 && abs(d2) <= 1 &&
      dir_from_offset[(d1+1) + 3*(d2+1)] > DirectionNone) {
    /* Convert offset in two dimensions to
       direction variable. */
    Direction d = dir_from_offset[(d1+1) + 3*(d2+1)];
    MapPos new_pos = map->move(pos, d);

    if (!can_pass_map_pos(new_pos)) {
      if (state != StateKnightFreeWalking && s.free_walking.neg_dist1 != -128) {
        s.free_walking.dist_col += s.free_walking.neg_dist1;
        s.free_walking.dist_row += s.free_walking.neg_dist2;
        s.free_walking.neg_dist1 = 0;
        s.free_walking.neg_dist2 = 0;
        s.free_walking.flags = 0;
      } else {
        Log::Warn["serf"] << "inside handle_free_walking_common(), a serf at pos " << get_pos() << " is being set to StateLost! he cannot pass map_pos " << new_pos << " and various other checks failed?";
        Log::Debug["serf"] << "inside Serf::handle_free_walking_common(), serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
        set_state(StateLost);
        s.lost.field_B = 0;
        counter = 0;
      }
      return;
    }

    if (state == StateKnightFreeWalking && s.free_walking.neg_dist1 != -128
        && map->has_serf(new_pos)) {
      Serf *other_serf = game->get_serf_at_pos(new_pos);
      Direction other_dir;

      if (other_serf->is_waiting(&other_dir) &&
          (other_dir == reverse_direction(d) || other_dir == DirectionNone) &&
          other_serf->switch_waiting(reverse_direction(d))) {
        /* Do the switch */
        other_serf->pos = pos;
        map->set_serf_index(other_serf->pos,
                                        other_serf->get_index());
        other_serf->animation =
          get_walking_animation(map->get_height(other_serf->pos) -
            map->get_height(new_pos),
                                reverse_direction(d), 1);
        other_serf->counter = counter_from_animation[other_serf->animation];

        animation = get_walking_animation(map->get_height(new_pos) -
                                          map->get_height(pos), d, 1);
        counter = counter_from_animation[animation];

        pos = new_pos;
        map->set_serf_index(pos, index);
        return;
      }

      if (other_serf->state == StateWalking ||
          other_serf->state == StateTransporting) {
        s.free_walking.neg_dist2 += 1;
        if (s.free_walking.neg_dist2 >= 10) {
          s.free_walking.neg_dist2 = 0;
          if (other_serf->state == StateTransporting) {
            if (map->has_flag(new_pos)) {
              if (other_serf->s.walking.wait_counter != -1) {
//                int dir = other_serf->s.walking.dir;
//                if (dir < 0) dir += 6;
                Log::Debug["serf"] << "TODO remove " << other_serf->get_index()
                                   << " from path";
              }
              Log::Debug["serf.cc"] << "inside Serf::handle_freewalking_common, a serf with index #" << this->get_index() << " and pos " << this->get_pos() << " is causing OTHER SERF #" << other_serf->get_index() << " with pos " << other_serf->get_pos() << " to become lost because... wait counter?";
              other_serf->set_lost_state();
            }
          } else {
            Log::Debug["serf.cc"] << "inside Serf::handle_freewalking_common, a serf with index #" << this->get_index() << " and pos " << this->get_pos() << " is causing OTHER SERF #" << other_serf->get_index() << " with pos " << other_serf->get_pos() << " to become lost because... wait counter2?";
            other_serf->set_lost_state();
          }
        }
      }

      animation = 82;
      counter = counter_from_animation[animation];
      return;
    }
  }

  /* Look for another direction to go in. */
  Direction i0 = DirectionNone;
  for (int i = 0; i < 5; i++) {
    dir = a0[1+i];
    MapPos new_pos = map->move(pos, dir);
    if (((water && map->get_obj(new_pos) == 0) ||
         (!water && !map->is_in_water(new_pos) &&
          can_pass_map_pos(new_pos))) && !map->has_serf(new_pos)) {
      i0 = (Direction)i;
      break;
    }
  }

  if (i0 < 0) {
    handle_serf_free_walking_switch_with_other();
    return;
  }

  int edge = 0;
  if (BIT_TEST(dir_index ^ i0, 0)) edge = 1;  // it looks like this is where left vs right dir is set
  int upper = (i0/2) + 1;


  //
  // fix for original game "defect" 
  //
  //  because the original game doesn't seem to actually have any pathfinding
  //  it instead relies on step-by-step "am I getting closer to the target?"
  //  while freewalking, and when a large obstacle such as a lake is encountered
  //  the game cannot tell which way around is faster, so it just picks one
  //  and sometimes sends the serf the long way around.
  // FURTHER, even if the serf goes the best way around, it is often unreasonably
  //  long distance and the serf should reconsider if possible
  //
  // for now, the fix is to detect a large water obstacle that puts a serf into
  //  follow left/right edge pattern, and run a pathfinder check.  Force the serf
  //  to follow the shortest way around
  //
  // NEXT, need to add a check if even the best path around is unreasonably long
  //  and have the serf take some other action if possible
  //
  // FINALLY, it seems sometimes lost serfs appear to choose totally random
  //  positions with zero logic that send them all over the map.  Figure out
  //  what is responsible for this and try to fix it
  //  it may be totally unrelated to this edge problem
  //
  MapPos chosen_pos = map->move(pos, Direction(dir));
  if (found_water_obstacle){
    //game->set_debug_mark_pos(chosen_pos, "cyan");


    //int current_col = map->pos_col(pos);
    int current_col = map->pos_col(chosen_pos);
    int highest_row = map->get_rows();  // rows/cols wrap around once they hit max, cannot do map->pos(col/row) with negative values!)
    //int current_row = map->pos_row(pos);
    int current_row = map->pos_row(chosen_pos);
    int highest_col = map->get_cols();  // rows/cols wrap around once they hit max, cannot do map->pos(col/row) with negative values!)
    int destination_col = current_col + s.free_walking.dist_col;
    if (destination_col < 0){
      destination_col += highest_col;
    }
    if (destination_col > highest_col){
      destination_col -= highest_col;
    }
    int destination_row = current_row + s.free_walking.dist_row;
    if (destination_row < 0){
      destination_row += highest_row;
    }
    if (destination_row > highest_row){
      destination_row -= highest_row;
    }
    MapPos dest_pos = map->pos(destination_col, destination_row);
    if (dest_pos > map->get_landscape_tiles_size()){
      //Log::Error["serf.cc"] << "inside Serf::handle_free_walking_common, dest_pos is out of range!  dest_pos " << dest_pos << ", landscape_tiles size " << map->get_landscape_tiles_size() << ", current_col " << current_col  << ", current_row " << current_row << ", dest_col " << destination_col << ", dest_row " << destination_row;
      //
      //  there is some issue here where the dest pos
      //  is slightly off from where it is expected, probably because
      //  I am not checking the dist_col/dist_row at the right time
      //  and it results in being one-off.  When this happens, normally it doesn't matter because
      //  the one-off coords are close enough to the correst dest_pos that the pathfinding works
      //  just as well. HOWEVER, when the serf is near the first/last col/row it runs into problems
      //  with the coords resulting in an invalid map pos, out of range.  
      //  For now, I am just going to skip the check if that happens, because I don't feel like spending
      //  a ton of time tracking down the cause when it is very unlikely to matter, and even if it does
      //  the original behavior was a minor annoyance, not a game breaking problem
      //
      Log::Warn["serf.cc"] << "inside Serf::handle_free_walking_common, dest_pos is out of range!  dest_pos " << dest_pos << ", landscape_tiles size " << map->get_landscape_tiles_size() << ", current_col " << current_col  << ", current_row " << current_row << ", dest_col " << destination_col << ", dest_row " << destination_row << ".  Ignoring this as it doesn't really matter much"; 
      //game->get_debug_mark_serf()->push_back(get_index());
      //game->set_debug_mark_pos(get_pos(), "green");
      //game->set_debug_mark_pos(dest_pos, "red");
      //game->pause();
      // just don't do the pathfinder check and hope the serf goes the correct way around!
    }else{
      //if (edge == 1){
      //  game->set_debug_mark_pos(pos, "green");
      //}else{
      //  game->set_debug_mark_pos(pos, "yellow");
      //}
      //game->set_debug_mark_pos(dest_pos, "red");
      // logic has been updated for Fisher to not even attempt to send a serf to a fishing pos that is more than 20 tiles-by-pathfinding away, so it is
      //  okay for this to be a higher value in case a serf is free walking for some other reason (lost?)
      Road path_around_lake = pathfinder_freewalking_serf(map.get(), pos, dest_pos, 100);   
      if (path_around_lake.get_length() > 0){
        //game->set_debug_mark_road(path_around_lake);
        Direction dir_to_go = path_around_lake.get_first();  
        if (dir != dir_to_go){
          //Log::Debug["serf.cc"] << "inside Serf::handle_free_walking_common, a free-walking serf has encountered water and chosen the wrong way around it, correcting him";
          dir = dir_to_go;
          //game->set_debug_mark_pos(map->move(pos, dir), "coral");
          // reverse the follow_edge direction by flipping "edge"
          edge = !edge;
        }
      }else{
        // saw this once, changing to Warn for now
        //throw ExceptionFreeserf("inside Serf::handle_free_walking_common, could not find a path around water/lake obstacle for freewalking serf!");
        Log::Warn["serf.cc"] << "inside Serf::handle_free_walking_common, a free-walking serf with index #" << this->get_index() << " and pos " << this->get_pos() << ", could not find a path around water/lake obstacle for freewalking serf of type " << NameSerf[this->get_type()] << "!";
      }
    }
  } // end of defect fix

  s.free_walking.flags = (upper << 4) | (edge << 3) | (dir+1);

  handle_serf_free_walking_switch_on_dir(dir);
}

void
Serf::handle_serf_free_walking_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    handle_free_walking_common();
  }
}

void
Serf::handle_serf_logging_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    s.free_walking.neg_dist2 += 1;

    int new_obj = -1;
    if (s.free_walking.neg_dist1 != 0) {
      new_obj = Map::ObjectFelledTree0 + s.free_walking.neg_dist2 - 1;
    } else {
      new_obj = Map::ObjectFelledPine0 + s.free_walking.neg_dist2 - 1;
    }

    /* Change map object. */
    game->get_map()->set_object(pos, (Map::Object)new_obj, -1);

    if (s.free_walking.neg_dist2 < 5) {
      animation = 116 + s.free_walking.neg_dist2;
      counter += counter_from_animation[animation];
    } else {
      set_state(StateFreeWalking);
      counter = 0;
      s.free_walking.neg_dist1 = -128;
      s.free_walking.neg_dist2 = 1;
      s.free_walking.flags = 0;
      return;
    }
  }
}

void
Serf::handle_serf_planning_logging_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    int dist = (game->random_int() & 0x7f) + 1;
    MapPos pos_ = game->get_map()->pos_add_spirally(pos, dist);
    int obj = game->get_map()->get_obj(pos_);
    if (obj >= Map::ObjectTree0 && obj <= Map::ObjectPine7) {
      // sanity check to ensure the dest pos is reachable within reasonable distance of this hut
      if (!can_reach_pos(pos_, 20)){
        Log::Debug["serf.cc"] << "inside Serf::handle_serf_planning_logging_state, serf cannot reach the chosen dest pos " << pos_ << " in less than 20 tiles, skipping this pos";
        counter += 400;
        continue;
      }
      set_state(StateReadyToLeave);
      s.leaving_building.field_B = Map::get_spiral_pattern()[2 * dist] - 1;
      s.leaving_building.dest = Map::get_spiral_pattern()[2 * dist + 1] - 1;
      s.leaving_building.dest2 = -Map::get_spiral_pattern()[2 * dist] + 1;
      s.leaving_building.dir = -Map::get_spiral_pattern()[2 * dist + 1] + 1;
      s.leaving_building.next_state = StateFreeWalking;
      
      Log::Verbose["serf"] << "planning logging: tree found, dist "
                           << s.leaving_building.field_B << ", "
                           << s.leaving_building.dest << ".";
      return;
    }

    counter += 400;
  }
}

void
Serf::handle_serf_planning_planting_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  PMap map = game->get_map();
  while (counter < 0) {
    int dist = (game->random_int() & 0x7f) + 1;
    MapPos pos_ = map->pos_add_spirally(pos, dist);
    if (map->paths(pos_) == 0 &&
        map->get_obj(pos_) == Map::ObjectNone &&
        map->type_up(pos_) == Map::TerrainGrass1 &&
        map->type_down(pos_) == Map::TerrainGrass1 &&
        map->type_up(map->move_up_left(pos_)) == Map::TerrainGrass1 &&
        map->type_down(map->move_up_left(pos_)) == Map::TerrainGrass1) {
      // sanity check to ensure the dest pos is reachable within reasonable distance of this hut
      if (!can_reach_pos(pos_, 20)){
        Log::Debug["serf.cc"] << "inside Serf::handle_serf_planning_planting_state, serf at pos " << get_pos() << " cannot reach the chosen dest pos " << pos_ << " in less than 20 tiles, skipping this pos";
        counter += 700;
        continue;
      }
      set_state(StateReadyToLeave);
      s.leaving_building.field_B = Map::get_spiral_pattern()[2 * dist] - 1;
      s.leaving_building.dest = Map::get_spiral_pattern()[2 * dist + 1] - 1;
      s.leaving_building.dest2 = -Map::get_spiral_pattern()[2 * dist] + 1;
      s.leaving_building.dir = -Map::get_spiral_pattern()[2 * dist + 1] + 1;
      s.leaving_building.next_state = StateFreeWalking;
      Log::Verbose["serf"] << "planning planting: free space found, dist "
                           << s.leaving_building.field_B << ", "
                           << s.leaving_building.dest << ".";
      return;
    }

    counter += 700;
  }
}

void
Serf::handle_serf_planting_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  PMap map = game->get_map();
  while (counter < 0) {
    if (s.free_walking.neg_dist2 != 0) {
      set_state(StateFreeWalking);
      s.free_walking.neg_dist1 = -128;
      s.free_walking.neg_dist2 = 0;
      s.free_walking.flags = 0;
      counter = 0;
      return;
    }

    /* Plant a tree */
    animation = 122;
    // tlongstretch - I had always thought that Forester/rangers only plant Pine trees, but
    //  it seems I was mis-remembering, I just verified in the original Settlers 1 game that 
    //  both Pine and deciduous Trees are planted by ranger/forester
    //Map::Object new_obj = (Map::Object)(Map::ObjectNewPine + (game->random_int() & 1));

    // option_ForesterMonoculture
    //  each individual forester always plants trees of the same type
    //   though others serfs may plant other types
    // to avoid having to create a new variable, and add to save/load, use
    //  the serf's index to determine the tree type
    Map::Object new_obj;
    if (option_ForesterMonoculture){
      Map::Object new_obj = (Map::Object)(get_index()); // this is the SERF's index, being used as a consistent random number
      if (new_obj % 16 > 7){
        // pine, only one type
        new_obj = (Map::Object)(Map::ObjectNewPine);
      }else{
        // deciduous tree, choose from one of 8 types (orig == 0, plus 7 new types)
        new_obj = (Map::Object)(Map::ObjectNewTree0 + (new_obj % 8));
      }
    }else{
      // normal behavior, random chance of either tree type, which will mature into random subtype
      new_obj = (Map::Object)(Map::ObjectNewPine + (game->random_int() & 1));
    }

    if (map->paths(pos) == 0 && map->get_obj(pos) == Map::ObjectNone) {
      map->set_object(pos, new_obj, -1);
    }

    s.free_walking.neg_dist2 = -s.free_walking.neg_dist2 - 1;
    counter += 128;
  }
}

void
Serf::handle_serf_planning_stonecutting() {
  //Log::Debug["serf.cc"] << "inside Serf::handle_serf_planning_stonecutting, for serf with index " << get_index() << " and pos " << get_pos();
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  PMap map = game->get_map();
  while (counter < 0) {
    //Log::Debug["serf.cc"] << "inside Serf::handle_serf_planning_stonecutting, for serf with index " << get_index() << " and pos " << get_pos() << ", counter < 0";
    int dist = (game->random_int() & 0x7f) + 1;
    MapPos pos_ = map->pos_add_spirally(pos, dist);
    int obj = map->get_obj(map->move_up_left(pos_));
    if (obj >= Map::ObjectStone0 && obj <= Map::ObjectStone7 &&
        can_pass_map_pos(pos_)) {
      // sanity check to ensure the dest pos is reachable within reasonable distance of this hut
      if (!can_reach_pos(pos_, 20)){
        Log::Debug["serf.cc"] << "inside Serf::handle_serf_planning_stonecutting, serf cannot reach the chosen dest pos " << pos_ << " in less than 20 tiles, skipping this pos";
        counter += 100;
        continue;
      }
      set_state(StateReadyToLeave);
      s.leaving_building.field_B = Map::get_spiral_pattern()[2 * dist] - 1;
      s.leaving_building.dest = Map::get_spiral_pattern()[2 * dist + 1] - 1;
      s.leaving_building.dest2 = -Map::get_spiral_pattern()[2 * dist] + 1;
      s.leaving_building.dir = -Map::get_spiral_pattern()[2 * dist + 1] + 1;
      s.leaving_building.next_state = StateStoneCutterFreeWalking;
      Log::Verbose["serf"] << "planning stonecutting: stone found, dist "
                           << s.leaving_building.field_B << ", "
                           << s.leaving_building.dest << ".";
      //Log::Debug["serf.cc"] << "inside Serf::handle_serf_planning_stonecutting, for serf with index " << get_index() << " and pos " << get_pos() << ", heading to pos_" << pos_;
      return;
    }

    counter += 100;
  }
}

void
Serf::handle_stonecutter_free_walking() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  PMap map = game->get_map();
  while (counter < 0) {
    MapPos pos_ = map->move_up_left(pos);
    if (!map->has_serf(pos) && map->get_obj(pos_) >= Map::ObjectStone0 &&
        map->get_obj(pos_) <= Map::ObjectStone7) {
      s.free_walking.neg_dist1 += s.free_walking.dist_col;
      s.free_walking.neg_dist2 += s.free_walking.dist_row;
      s.free_walking.dist_col = 0;
      s.free_walking.dist_row = 0;
      s.free_walking.flags = 8;
    }

    handle_free_walking_common();
  }
}

void
Serf::handle_serf_stonecutting_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (s.free_walking.neg_dist1 == 0) {
    if (counter > s.free_walking.neg_dist2) return;
    counter -= s.free_walking.neg_dist2 + 1;
    s.free_walking.neg_dist1 = 1;
    animation = 123;
    counter += 1536;
  }

  while (counter < 0) {
    if (s.free_walking.neg_dist1 != 1) {
      set_state(StateFreeWalking);
      s.free_walking.neg_dist1 = -128;
      s.free_walking.neg_dist2 = 1;
      s.free_walking.flags = 0;
      counter = 0;
      return;
    }

    PMap map = game->get_map();
    if (map->has_serf(map->move_down_right(pos))) {
      counter = 0;
      return;
    }

    /* Decrement stone quantity or remove entirely if this
       was the last piece. */
    int obj = map->get_obj(pos);
    if (obj <= Map::ObjectStone6) {
      map->set_object(pos, (Map::Object)(obj + 1), -1);
    } else {
      map->set_object(pos, Map::ObjectNone, -1);
    }

    counter = 0;
    start_walking(DirectionDownRight, 24, 1);
    tick = game->get_tick();

    s.free_walking.neg_dist1 = 2;
  }
}

void
Serf::handle_serf_sawing_state() {
  if (s.sawing.mode == 0) {
    Building *building =
                        game->get_building(game->get_map()->get_obj_index(pos));
    if (building->use_resource_in_stock(1)) {
      s.sawing.mode = 1;
      animation = 124;
      counter = counter_from_animation[animation];
      tick = game->get_tick();
      game->get_map()->set_serf_index(pos, index);
    }
  } else {
    uint16_t delta = game->get_tick() - tick;
    tick = game->get_tick();
    counter -= delta;

    if (counter >= 0) return;

    game->get_map()->set_serf_index(pos, 0);
    set_state(StateMoveResourceOut);
    s.move_resource_out.res = 1 + Resource::TypePlank;
    s.move_resource_out.res_dest = 0;
    s.move_resource_out.next_state = StateDropResourceOut;

    /* Update resource stats. */
    Player *player = game->get_player(get_owner());
    player->increase_res_count(Resource::TypePlank);
  }
}


//
// adding support for option_LostTransportersClearFaster to allow generic (non-knights, non-profressional) serfs to enter
//  any nearby friendly building (except mines, because the miners often block the entrace when waiting for food)
//  if they become Lost.  Normally, serfs must return to an Inventory building (i.e. Castle or a Stock).  If the building
//  is currently active, the arriving Lost transporter will wait at the flag until the building becomes inactive and then
//  enter
//
//  NOTE - I tried modifying the enter_building code so that a Lost transporter could enter an *active* building,
//   which would also allow it to enter a mine with a striking worker, but because the game only draws one serf per tile
//   unless a special hack is put in place to draw multiple serfs in one tile, such as during fights or during my 
//   CanTransportSerfsInBoats feature, it makes the building-active-serf disappear and resets their animation and overall
//   looks sloppy.  So I reverted it back to the above described state
//
// original freeserf code logic:
// - when a serf becomes Lost, random directions are searched some distance until a flag is found within own territory, 
//    if one is found, that is set as a pseudo-destination by way of dist_col and dist_row I think, and serf walks one tile in that direction
// - every time a new tile is reached, game checks spirally around the lost serf for a _nearby_ "suitable flag" destination, 
//    and seems to further direct the serf to walk towards that "suitable flag" (note that serf is still off-path)
// - a "suitable flag" destination is a flag with any path, or a flag that the single flag-attached-to-an-Inventory.  Note that my new logic allows 
//    flag-attached-to-any-completed-building not just Inventories
// - once the serf reaches any land path the serf state is changed from Lost to FreeWalking state, 
//    which will then trigger the find_nearest_inventory call 
// - this means that my idea of an extra attribute that identifies serfs that were Lost is the best way of handling this, because the Inventory
//    destination is NOT set while serf is Lost, only when Serf reaches a "suitable flag" inside own borders
// - my update is to add a 'was_lost' bool to the Serf class objects, which is set when a serf becomes Lost and is checked
//    to determine if a serf is directed to a non-Inventory friendly complete building to enter to clear

void
Serf::handle_serf_lost_state() {
  if (this->get_building_held() != 0){
    Log::Warn["serf.cc"] << "inside Serf::handle_serf_lost_state, serf #" << this->get_index() << " of type " << NameSerf[this->get_type()] << " at pos " << this->get_pos() << " is Lost but also is holder to building #" << this->get_building_held() << "!";
    //Log::Warn["serf.cc"] << "inside Serf::handle_serf_lost_state, marking serf #" << this->get_index() << " in lavender";
    //game->set_debug_mark_pos(this->get_pos(), "lavender");
    Building *held_building = game->get_building(this->get_building_held());
    if (held_building == nullptr){
      Log::Warn["serf.cc"] << "inside Serf::handle_serf_lost_state, serf #" << this->get_index() << " of type " << NameSerf[this->get_type()] << " at pos " << this->get_pos() << " is Lost but also is holder to building #" << this->get_building_held() << " and held building is nullptr!";
    }else{
      Log::Warn["serf.cc"] << "inside Serf::handle_serf_lost_state, serf #" << this->get_index() << " of type " << NameSerf[this->get_type()] << " at pos " << this->get_pos() << " is Lost but also is holder to building #" << this->get_building_held() << " which has building pos " << held_building->get_position();
      //Log::Warn["serf.cc"] << "inside Serf::handle_serf_lost_state, marking held building in seafoam";
      //game->set_debug_mark_pos(held_building->get_position(), "seafoam");
    }

    //game->speed_reset();
    //game->speed_decrease();
    //game->pause();
  }
  //
  // general note about Lost state, it seems that serfs are only Lost very briefly and then transition to another state such as
  //  FreeWalking as soon as they reach any adjacent tile while Lost.  They can become lost again, but Lost is a very short lived state
  // 
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  PMap map = game->get_map();
  while (counter < 0) {
    /* Try to find a suitable destination. */
    for (int i = 0; i < 258; i++) {
      int dist = (s.lost.field_B == 0) ? 1+i : 258-i;  // I think this means, if s.lost.field_B == 0, do a normal spiral from center-out
                                                       // but if s.lost.field_B == 1, do a reverse spiral from outside-in
      MapPos dest = map->pos_add_spirally(pos, dist);

      if (map->has_flag(dest)) {
        Flag *flag = game->get_flag(map->get_obj_index(dest));
        // this check is basically
        // "if this serf has reached a PATH (land only), enter StateFreeWalking"
        // "OR if this is an Inventory flag with NO PATHS (such as a lone castle), that is okay too"
        // 99% of the time this will be "any land path reached"
        // more important stuff happens at Serf::handle_serf_free_walking_state_dest_reached
        if ((flag->land_paths() != 0 ||
              /*
              (flag->has_inventory() && flag->accepts_serfs())) &&
              map->has_owner(dest) &&
              map->get_owner(dest) == get_owner()) {
                */
            ((type == Serf::TypeGeneric || type == Serf::TypeTransporter || type == Serf::TypeNone) && flag->has_building() && flag->get_building()->is_done()) || 
            // non-generic serfs still have to go to nearest Inventory, we can't afford to delete them 
            //  and teleporting them doesn't seem fair
            (flag->has_inventory() && flag->accepts_serfs())) &&           
            // usual test to ensure this is a friendly pos/building
            map->has_owner(dest) && map->get_owner(dest) == get_owner()) {
          if (get_type() >= TypeKnight0 &&
              get_type() <= TypeKnight4) {
            set_state(StateKnightFreeWalking);
          } else {
            set_state(StateFreeWalking);
          }

          s.free_walking.dist_col = Map::get_spiral_pattern()[2 * dist];
          s.free_walking.dist_row = Map::get_spiral_pattern()[2 * dist +1];
          s.free_walking.neg_dist1 = -128;
          s.free_walking.neg_dist2 = -1;
          s.free_walking.flags = 0;

          if (option_LostTransportersClearFaster){
            was_lost = true;  // store this information so the handle_walking state and onward can allow the serf to clear from non-Inventory buildings
          }

          counter = 0;
          return;
        }
      }
    }

    Log::Info["serf.cc"] << "serf #" << get_index() << " at pos " << get_pos() << " is lost and could not find a friendly flag within 8 tiles, he will walk randomly forever until he does!  This is normal game behavior";

    /* Choose a random destination */
    unsigned int size = 16;
    int tries = 10;

    while (1) {
      tries -= 1;
      if (tries < 0) {
        if (size < 64) {
          tries = 19;
          size *= 2;
        } else {
          tries = -1;
          size = 16;
        }
      }

      int r = game->random_int();
      int col = ((r & (size-1)) - (size/2));
      int row = (((r >> 8) & (size-1)) - (size/2));

      MapPos dest = map->pos_add(pos, col, row);
      if ((map->get_obj(dest) == 0 && map->get_height(dest) > 0) ||
          (map->has_flag(dest) &&
           (map->has_owner(dest) &&
             map->get_owner(dest) == get_owner()))) {
        if (get_type() >= TypeKnight0 && get_type() <= TypeKnight4) {
          set_state(StateKnightFreeWalking);
        } else {
          set_state(StateFreeWalking);
        }

        s.free_walking.dist_col = col;
        s.free_walking.dist_row = row;
        s.free_walking.neg_dist1 = -128;
        s.free_walking.neg_dist2 = -1;
        s.free_walking.flags = 0;
        counter = 0;
        return;
      }
    }
  }
}

void
Serf::handle_lost_sailor() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  PMap map = game->get_map();
  while (counter < 0) {
    /* Try to find a suitable destination. */
    for (int i = 0; i < 258; i++) {
      MapPos dest = map->pos_add_spirally(pos, i);

      if (map->has_flag(dest)) {
        Flag *flag = game->get_flag(map->get_obj_index(dest));
        if (flag->land_paths() != 0 &&
            map->has_owner(dest) &&
            map->get_owner(dest) == get_owner()) {
          set_state(StateFreeSailing);

          s.free_walking.dist_col = Map::get_spiral_pattern()[2*i];
          s.free_walking.dist_row = Map::get_spiral_pattern()[2*i+1];
          s.free_walking.neg_dist1 = -128;
          s.free_walking.neg_dist2 = -1;
          s.free_walking.flags = 0;
          counter = 0;
          return;
        }
      }
    }

    /* Choose a random, empty destination */
    while (1) {
      int r = game->random_int();
      int col = (r & 0x1f) - 16;
      int row = ((r >> 8) & 0x1f) - 16;

      MapPos dest = map->pos_add(pos, col, row);
      if (map->get_obj(dest) == 0) {
        set_state(StateFreeSailing);

        s.free_walking.dist_col = col;
        s.free_walking.dist_row = row;
        s.free_walking.neg_dist1 = -128;
        s.free_walking.neg_dist2 = -1;
        s.free_walking.flags = 0;
        counter = 0;
        return;
      }
    }
  }
}

void
Serf::handle_free_sailing() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    if (!game->get_map()->is_in_water(pos)) {
      Log::Warn["serf"] << "inside handle_free_sailing(), a serf at pos " << get_pos() << " is being set to StateLost! because he is no longer in water at pos " << pos;
      Log::Debug["serf"] << "inside Serf::handle_free_sailing(), serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
      set_state(StateLost);
      s.lost.field_B = 0;
      return;
    }

    handle_free_walking_common();
  }
}

void
Serf::handle_serf_escape_building_state() {
  //Log::Debug["serf.cc"] << "inside Serf::handle_serf_escape_building_state(), serf #" << get_index();
  if (!game->get_map()->has_serf(pos)) {
    //Log::Debug["serf.cc"] << "inside Serf::handle_serf_escape_building_state(), serf #" << get_index() << " game says no serf at this pos?  setting this serf to Lost??";
    game->get_map()->set_serf_index(pos, index);
    animation = 82;
    counter = 0;
    tick = game->get_tick();
    Log::Debug["serf"] << "inside Serf::handle_serf_escape_building_state(), serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
    set_state(StateLost);
    s.lost.field_B = 0;
  //}else{
    //Log::Debug["serf.cc"] << "inside Serf::handle_serf_escape_building_state(), serf #" << get_index() << " game says serf at this pos, doing nothing";
  }
}

void
Serf::handle_serf_mining_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  PMap map = game->get_map();
  while (counter < 0) {
    Building *building = game->get_building(map->get_obj_index(pos));

    Log::Verbose["serf"] << "mining substate: " << s.mining.substate << ".";
    switch (s.mining.substate) {
      case 0: {

        // WARNING - this is a huge deal, see: https://github.com/freeserf/freeserf/commit/9d0b353215ab8a011259e77e56bd2327435c67a5
        //   and here: https://github.com/forkserf/forkserf/issues/86

        // ************ VERY IMPORTANT *************
        //  jonls intentionally flipped this from 1/8 chance of
        // consuming food (as in original game) to 1/8 chance of
        // NOT consuming food, greatly increasing the food consumption
        // of miners.
        //https://github.com/freeserf/freeserf/commit/9d0b353215ab8a011259e77e56bd2327435c67a5
        // this seems like too big of change from expectations to allow 
        // as a default.  Instead, I am thinking of changing it back to original 7/8 consumption
        // and instead making a higher food consumption a checkbox (initially)
        // but eventually part of a sliding game difficulty/handicap option
        // for human vs AI players
        // Given that 7/8 is arbitrary, a lower value is probably appropriate
        // as with this rate of consumption it is very difficult to stock the
        // mine fully with food, as it is consumed quickly and even if sufficient
        // food items are available, serfs generally cannot transport it quickly
        // enough to the mine even with an efficient road system
        // perhaps if ALL item consumptions are "1" instead of chances (such
        // as pig farmer having tiny chance of consuming wheat?) then 
        // other methods can be used to rebalance the game
        //
        // nicymike confirms this, and I think (I forget now) I was able to confirm original behavior
        // in original game on DosBox
        //

        /* There is a (????) chance that the miner will
           not require food and skip to state 2. */

        // UPDATE - this is now an option.  If option_HighMinerFoodConsumption is set, the Freeserf 7/8 chance of food consumption used
        //  if not set, the original Settlers1/SerfCity 1/8 chancege used

                  // this is the original SerfCity/Settlers1 value, where 1/8th chance of requiring food (i.e. rarely consuming food)
                  //   per nicymike this is the accurate original value
                  // if ((r & 7) != 0) {

                  // this is the jonls-freeserf "enhanced food consumption" change, where 7/8th chance of NOT requiring food (i.e. almost always consuming food)
                  //    freeserf commit   https://github.com/freeserf/freeserf/commit/9d0b353215ab8a011259e77e56bd2327435c67a5
                  // if ((r & 7) == 0) {  

        int r = game->random_int();
        //
        // making this overly verbose/redundant to eliminate any confusion
        //
        if (option_HighMinerFoodConsumption){
          // jonls-freeserf "enhanced food consumption" change, where 7/8th chance of NOT requiring food (i.e. almost always consuming food)
          if ((r & 7) == 0) {  
            s.mining.substate = 2; // do not require food
          } else {
            s.mining.substate = 1;
          }
        }else{
          // normal original SerfCity/Settlers1 value, where 1/8th chance of requiring food (i.e. rarely consuming food)
          if ((r & 7) != 0) {  
            s.mining.substate = 2; // do not require food
          } else {
            s.mining.substate = 1;
          }
        }
        counter += 100 + (r & 0x1ff);
        break;
      }
      case 1:
        if (building->use_resource_in_stock(0)) {
          /* Eat the food. */
          s.mining.substate = 3;
          map->set_serf_index(pos, index);
          animation = 125;
          counter = counter_from_animation[animation];
        } else {
          map->set_serf_index(pos, index);
          animation = 98; // this is the walking-back-and-forth animation used to show striking/refusing to work without food (and also for Builder constructing)
          counter += 256;
          if (counter < 0) counter = 255;
        }
        break;
      case 2:
        s.mining.substate = 3;
        map->set_serf_index(pos, index);
        animation = 125;
        counter = counter_from_animation[animation];
        break;
      case 3:
        s.mining.substate = 4;
        building->stop_activity();
        animation = 126;
        counter = counter_from_animation[animation];
        break;
      case 4: {
        building->start_playing_sfx();
        map->set_serf_index(pos, 0);
        /* fall through */
      }
      case 5:
      case 6:
      case 7: {
        s.mining.substate += 1;

        /* Look for resource in ground. */
        MapPos dest = map->pos_add_spirally(pos,
                                            (game->random_int() >> 2) & 0x1f);
        if ((map->get_obj(dest) == Map::ObjectNone ||
             map->get_obj(dest) > Map::ObjectCastle) &&
            map->get_res_type(dest) == s.mining.deposit &&
            map->get_res_amount(dest) > 0) {
          /* Decrement resource count in ground. */
          map->remove_ground_deposit(dest, 1);

          /* Hand resource to miner. */
          const Resource::Type res_from_mine_type[] = {
            Resource::TypeGoldOre, Resource::TypeIronOre,
            Resource::TypeCoal, Resource::TypeStone
          };

          s.mining.res = res_from_mine_type[s.mining.deposit-1] + 1;
          s.mining.substate = 8;
        }

        counter += 1000;
        break;
      }
      case 8:
        map->set_serf_index(pos, index);
        s.mining.substate = 9;
        building->stop_playing_sfx();
        animation = 127;
        counter = counter_from_animation[animation];
        break;
      case 9:
        s.mining.substate = 10;
        building->increase_mining(s.mining.res);
        animation = 128;
        counter = counter_from_animation[animation];
        break;
      case 10:
        map->set_serf_index(pos, 0);
        if (s.mining.res == 0) {
          s.mining.substate = 0;
          counter = 0;
        } else {
          unsigned int res = s.mining.res;
          map->set_serf_index(pos, 0);

          set_state(StateMoveResourceOut);
          s.move_resource_out.res = res;
          s.move_resource_out.res_dest = 0;
          s.move_resource_out.next_state = StateDropResourceOut;

          /* Update resource stats. */
          Player *player = game->get_player(get_owner());
          player->increase_res_count(static_cast<Resource::Type>(res-1));
          return;
        }
        break;
      default:
        NOT_REACHED();
        break;
    }
  }
}

void
Serf::handle_serf_smelting_state() {
  Building *building = game->get_building(game->get_map()->get_obj_index(pos));

  if (s.smelting.mode == 0) {
    if (building->use_resources_in_stocks()) {
      building->start_activity();

      s.smelting.mode = 1;
      if (s.smelting.type == 0) {
        animation = 130;
      } else {
        animation = 129;
      }
      s.smelting.counter = 20;
      counter = counter_from_animation[animation];
      tick = game->get_tick();

      game->get_map()->set_serf_index(pos, index);
    }
  } else {
    uint16_t delta = game->get_tick() - tick;
    tick = game->get_tick();
    counter -= delta;

    while (counter < 0) {
      s.smelting.counter -= 1;
      if (s.smelting.counter < 0) {
        building->stop_activity();

        int res = -1;
        if (s.smelting.type == 0) {
          res = 1 + Resource::TypeSteel;
        } else {
          res = 1 + Resource::TypeGoldBar;
        }

        set_state(StateMoveResourceOut);

        s.move_resource_out.res = res;
        s.move_resource_out.res_dest = 0;
        s.move_resource_out.next_state = StateDropResourceOut;

        /* Update resource stats. */
        Player *player = game->get_player(get_owner());
        player->increase_res_count(static_cast<Resource::Type>(res-1));
        return;
      } else if (s.smelting.counter == 0) {
        game->get_map()->set_serf_index(pos, 0);
      }

      counter += 384;
    }
  }
}

void
Serf::handle_serf_planning_fishing_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  PMap map = game->get_map();
  while (counter < 0) {
    int dist = ((game->random_int() >> 2) & 0x3f) + 1;
    MapPos dest = map->pos_add_spirally(pos, dist);

    if (map->get_obj(dest) == Map::ObjectNone &&
        map->paths(dest) == 0 &&
        ((map->type_down(dest) <= Map::TerrainWater3 &&
          map->type_up(map->move_up_left(dest)) >= Map::TerrainGrass0) ||
         (map->type_down(map->move_left(dest)) <= Map::TerrainWater3 &&
          map->type_up(map->move_up(dest)) >= Map::TerrainGrass0))) {
      // sanity check to ensure the dest pos is reachable within reasonable distance of this hut
      if (!can_reach_pos(dest, 20)){
        Log::Debug["serf.cc"] << "inside Serf::handle_serf_planning_fishing_state, serf cannot reach the chosen dest pos " << dest << " in less than 20 tiles, skipping this pos";
        counter += 100;
        continue;
      }
      set_state(StateReadyToLeave);
      s.leaving_building.field_B = Map::get_spiral_pattern()[2 * dist] - 1;
      s.leaving_building.dest = Map::get_spiral_pattern()[2 * dist + 1] - 1;
      s.leaving_building.dest2 = -Map::get_spiral_pattern()[2 * dist] + 1;
      s.leaving_building.dir = -Map::get_spiral_pattern()[2 * dist +1] + 1;
      s.leaving_building.next_state = StateFreeWalking;
      Log::Verbose["serf"] << "planning fishing: lake found, dist "
                           << s.leaving_building.field_B << ","
                           << s.leaving_building.dest;
      return;
    }

    counter += 100;
  }
}

void
Serf::handle_serf_fishing_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    if (s.free_walking.neg_dist2 != 0 ||
        s.free_walking.flags == 10) {
      /* Stop fishing. Walk back. */
      set_state(StateFreeWalking);
      s.free_walking.neg_dist1 = -128;
      s.free_walking.flags = 0;
      counter = 0;
      return;
    }

    s.free_walking.neg_dist1 += 1;
    if ((s.free_walking.neg_dist1 % 2) == 0) {
      animation -= 2;
      counter += 768;
      continue;
    }

    PMap map = game->get_map();
    Direction dir = DirectionNone;
    if (animation == 131) {
      if (map->is_in_water(map->move_left(pos))) {
        dir = DirectionLeft;
      } else {
        dir = DirectionDown;
      }
    } else {
      if (map->is_in_water(map->move_right(pos))) {
        dir = DirectionRight;
      } else {
        dir = DirectionDownRight;
      }
    }

    int res = map->get_res_fish(map->move(pos, dir));
    if (res > 0 && (game->random_int() & 0x3f) + 4 < res) {
      /* Caught a fish. */
      map->remove_fish(map->move(pos, dir), 1);
      s.free_walking.neg_dist2 = 1 + Resource::TypeFish;
    }

    s.free_walking.flags += 1;
    animation += 2;
    counter += 128;
  }
}

void
Serf::handle_serf_planning_farming_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  // in normal farming, farmer only becomes active every
  //  65500 ticks, and remains active until no more work
  //  can be done, then goes back into inactive state for 65500 ticks
  // with AdvancedFarming, farmer must always be active
  //  but will only sow/harvest during appropriate seasons
  if (option_AdvancedFarming){
    // keep working
  }else{
    // take rest until 65500 speed-adjusted game ticks have passed?
    if (counter > 0) {
      return;
    }
  }

  // try random pos around the Farm and check
  // if any are suitable for new fields, or if
  // they have mature fields that can be harvested
  // and if either is true, send the farmer out
  PMap map = game->get_map();
  while (true) {
    int dist = ((game->random_int() >> 2) & 0x1f) + 7;
    MapPos dest = map->pos_add_spirally(pos, dist);

    bool send_farmer_out = false;
    if (option_AdvancedFarming) {
              // advanced farming logic - prefer harvesting, only sow during appropriate seasons
      // if this is a mature field
      if (map->get_obj(dest) == Map::ObjectSeeds5 ||
          (map->get_obj(dest) >= Map::ObjectField0 && map->get_obj(dest) <= Map::ObjectField5)) {
        send_farmer_out = true;   
      } else if (counter > 32750 && game->can_build_field(dest) &&  // prioritize harvesting by only allowing sowing during the second half of the 131 possible checks
          season != 1 && !(season == 3 && subseason >= 6) && !(season == 0 && subseason >= 12)) { // don't sow in summer or mid/late winter or late spring
        send_farmer_out = true;
      }
    }else{    // original farming logic - no preference for harvesting vs sowing
      if (game->can_build_field(dest) ||
          map->get_obj(dest) == Map::ObjectSeeds5 ||
          (map->get_obj(dest) >= Map::ObjectField0 &&
          map->get_obj(dest) <= Map::ObjectField5)) {
        send_farmer_out = true;
      }
    }

    // sanity check to ensure the dest pos is reachable within reasonable distance of this hut
    if (send_farmer_out){
      if (!can_reach_pos(dest, 20)){
        Log::Debug["serf.cc"] << "inside Serf::handle_serf_planning_farming_state, serf cannot reach the chosen dest pos " << dest << " in less than 20 tiles, skipping this pos";
        send_farmer_out = false;
      }
    }

    if (send_farmer_out){
      set_state(StateReadyToLeave);
      s.leaving_building.field_B = Map::get_spiral_pattern()[2 * dist] - 1;
      s.leaving_building.dest = Map::get_spiral_pattern()[2 * dist + 1] - 1;
      s.leaving_building.dest2 = -Map::get_spiral_pattern()[2 * dist] + 1;
      s.leaving_building.dir = -Map::get_spiral_pattern()[2 * dist + 1] + 1;
      s.leaving_building.next_state = StateFreeWalking;
      Log::Verbose["serf"] << "planning farming: field spot found, dist "
                          << s.leaving_building.field_B << ", "
                          << s.leaving_building.dest << ".";
      return;
    }

    // avoid incrementing the counter BEYOND 65500 when AdvancedFarming is on, in case it is turned back off (or overflows?)
    if (option_AdvancedFarming && counter >= 65500){
      // do not increment
    }else{
      counter += 500;
    }
    if (counter >= 65500) {
      return;
    }
  }
}

void
Serf::handle_serf_farming_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  PMap map = game->get_map();
  Map::Object object = map->get_obj(pos);

  if (counter >= 0) return;

  if (s.free_walking.neg_dist1 == 0) {
    // Sowing
    if (object == Map::ObjectNone && map->paths(pos) == 0) {
      map->set_object(pos, Map::ObjectSeeds0, -1);
    }
  } else {
    // Harvesting
    s.free_walking.neg_dist2 = 1;
    object = static_cast<Map::Object>(static_cast<int>(object) + 1);
    if (object == Map::ObjectFieldExpired) {
      // why would this reset to Field0???  is this to prevent serf from farming an expiring field?
      // yes I think that is it
      //object = Map::ObjectField0;
      // try setting a later field so it expires sooner instead of resetting so far back
      //object = Map::ObjectField4;  // Field4 works fine, try Field5
      object = Map::ObjectField5;  // I expect this will leave plenty of time for the farmer to finish reaping.
    } else if (object == Map::ObjectSignLargeGold || object == Map::Object127) { 
      // WTF is this???
      object = Map::ObjectFieldExpired;
    }else if (option_AdvancedFarming){
      // immediately Expire a field when harvested when AdvancedFarming on
      object = Map::ObjectFieldExpired;
    }
    map->set_object(pos, object, -1);
  }

  set_state(StateFreeWalking);
  s.free_walking.neg_dist1 = -128;
  s.free_walking.flags = 0;
  counter = 0;
}

void
Serf::handle_serf_milling_state() {
  Building *building = game->get_building(game->get_map()->get_obj_index(pos));

  if (s.milling.mode == 0) {
    if (building->use_resource_in_stock(0)) {
      building->start_activity();

      s.milling.mode = 1;
      animation = 137;
      counter = counter_from_animation[animation];
      tick = game->get_tick();

      game->get_map()->set_serf_index(pos, index);
    }
  } else {
    uint16_t delta = game->get_tick() - tick;
    tick = game->get_tick();
    counter -= delta;

    while (counter < 0) {
      s.milling.mode += 1;
      if (s.milling.mode == 5) {
        /* Done milling. */
        building->stop_activity();
        set_state(StateMoveResourceOut);
        s.move_resource_out.res = 1 + Resource::TypeFlour;
        s.move_resource_out.res_dest = 0;
        s.move_resource_out.next_state = StateDropResourceOut;

        Player *player = game->get_player(get_owner());
        player->increase_res_count(Resource::TypeFlour);
        return;
      } else if (s.milling.mode == 3) {
        game->get_map()->set_serf_index(pos, index);
        animation = 137;
        counter = counter_from_animation[animation];
      } else {
        game->get_map()->set_serf_index(pos, 0);
        counter += 1500;
      }
    }
  }
}

void
Serf::handle_serf_baking_state() {
  Building *building = game->get_building(game->get_map()->get_obj_index(pos));

  if (s.baking.mode == 0) {
    if (building->use_resource_in_stock(0)) {
      s.baking.mode = 1;
      animation = 138;
      counter = counter_from_animation[animation];
      tick = game->get_tick();

      game->get_map()->set_serf_index(pos, index);
    }
  } else {
    uint16_t delta = game->get_tick() - tick;
    tick = game->get_tick();
    counter -= delta;

    while (counter < 0) {
      s.baking.mode += 1;
      if (s.baking.mode == 3) {
        /* Done baking. */
        building->stop_activity();

        set_state(StateMoveResourceOut);
        s.move_resource_out.res = 1 + Resource::TypeBread;
        s.move_resource_out.res_dest = 0;
        s.move_resource_out.next_state = StateDropResourceOut;

        Player *player = game->get_player(get_owner());
        player->increase_res_count(Resource::TypeBread);
        return;
      } else {
        building->start_activity();
        game->get_map()->set_serf_index(pos, 0);
        counter += 1500;
      }
    }
  }
}

void
Serf::handle_serf_pigfarming_state() {
  /* When the serf is present there is also at least one
     pig present and at most eight. */
  // NOTE - the original settlers1/serfcity game is bugged
  //  In it, pig farms only ever use a single wheat resource (I think, is this actually true??) 
  //  yet they will store up to eight units of wheat forever.
  //  Freeserf replicates this bug(?)
  //
  //  ACTUALLY, nicymike says that this is intentional part of the original game code, and that
  //   the chance of consuming a unit of grain increases over time, so it does use more than the
  //   first unit of grain.  HOWEVER, my experience when I tested the original DOS game (only once)
  //   was that it didn't seem to ever use more than one, at least for as long as I was waiting
  //   not sure, I will trust he is right on this for now
  //
  //const int breeding_prob[] = { 6000, 8000, 10000, 11000, 12000, 13000, 14000, 0 };
  
  const int normal_breeding_prob[]  = { 6000, 8000, 10000, 11000, 12000, 13000, 14000, 0 };
  // AdvancedFarming - reduce pig breeding rate to keep wheat farm's advantage (when space available)
  //   INSTEAD, HOW ABOUT SEASONAL PIG BREEDING/SLAUGHTERING??  
  //    maybe have pigs rapidly born in spring, then nearly all slaughtered in late fall, early winter
  //    working on this as part of pannage effort, for now just reduce breeding rate
  const int reduced_breeding_prob[] = { 4000, 6000,  6666,  7333,  8000,  8666,  9333, 0 };  // these values seem to result in pig production 2/3 the rate of wheat, which is reasonable 

  Building *building = game->get_building_at_pos(pos);
  if (s.pigfarming.mode == 0) {
    //if (option_ImprovedPigFarms || building->use_resource_in_stock(0)) {   // removing this as it turns out the default behavior for pig farms is to require almost no grain
    if (building->use_resource_in_stock(0)) {
      s.pigfarming.mode = 1;
      animation = 139;
      counter = counter_from_animation[animation];
      tick = game->get_tick();

      game->get_map()->set_serf_index(pos, index);
    }
  } else {
    uint16_t delta = game->get_tick() - tick;
    tick = game->get_tick();
    counter -= delta;
    while (counter < 0) {
      s.pigfarming.mode += 1;
      if (s.pigfarming.mode & 1) {
        if (s.pigfarming.mode != 7) {
          game->get_map()->set_serf_index(pos, index);
          animation = 139;
          counter = counter_from_animation[animation];
        } else if (building->pigs_count() == 8 ||
                   (building->pigs_count() > 3 &&
                    ((20*game->random_int()) >> 16) < building->pigs_count())) {
          /* Pig is ready for the butcher. */
          building->send_pig_to_butcher();
          set_state(StateMoveResourceOut);
          s.move_resource_out.res = 1 + Resource::TypePig;
          s.move_resource_out.res_dest = 0;
          s.move_resource_out.next_state = StateDropResourceOut;

          /* Update resource stats. */
          Player *player = game->get_player(get_owner());
          player->increase_res_count(Resource::TypePig);
        } else if (game->random_int() & 0xf) {
          s.pigfarming.mode = 1;
          animation = 139;
          counter = counter_from_animation[animation];
          tick = game->get_tick();
          game->get_map()->set_serf_index(pos, index);
        } else {
          s.pigfarming.mode = 0;
        }
        return;
      } else {
        game->get_map()->set_serf_index(pos, 0);

        // AdvancedFarming - reduce pig breeding rate to keep wheat farm's advantage (when space available)
        int breeding_prob_val = 0;
        if (option_AdvancedFarming){
          breeding_prob_val = reduced_breeding_prob[building->pigs_count()-1];      
        }else{
          breeding_prob_val = normal_breeding_prob[building->pigs_count()-1];  
        }
        if (building->pigs_count() < 8 &&
            game->random_int() < breeding_prob_val) {
          building->place_new_pig();
        }
        counter += 2048;
      }
    }
  }
}

void
Serf::handle_serf_butchering_state() {
  Building *building = game->get_building(game->get_map()->get_obj_index(pos));

  if (s.butchering.mode == 0) {
    if (building->use_resource_in_stock(0)) {
      s.butchering.mode = 1;
      animation = 140;
      counter = counter_from_animation[animation];
      tick = game->get_tick();

      game->get_map()->set_serf_index(pos, index);
    }
  } else {
    uint16_t delta = game->get_tick() - tick;
    tick = game->get_tick();
    counter -= delta;

    if (counter < 0) {
      /* Done butchering. */
      game->get_map()->set_serf_index(pos, 0);

      set_state(StateMoveResourceOut);
      s.move_resource_out.res = 1 + Resource::TypeMeat;
      s.move_resource_out.res_dest = 0;
      s.move_resource_out.next_state = StateDropResourceOut;

      /* Update resource stats. */
      Player *player = game->get_player(get_owner());
      player->increase_res_count(Resource::TypeMeat);
    }
  }
}

void
Serf::handle_serf_making_weapon_state() {
  Building *building = game->get_building(game->get_map()->get_obj_index(pos));

  if (s.making_weapon.mode == 0) {
    /* One of each resource makes a sword and a shield.
       Bit 3 is set if a sword has been made and a
       shield can be made without more resources. */
    /* TODO Use of this bit overlaps with sfx check bit. */
    if (!building->is_playing_sfx()) {
      if (!building->use_resources_in_stocks()) {
        return;
      }
    }

    building->start_activity();

    s.making_weapon.mode = 1;
    animation = 143;
    counter = counter_from_animation[animation];
    tick = game->get_tick();

    game->get_map()->set_serf_index(pos, index);
  } else {
    uint16_t delta = game->get_tick() - tick;
    tick = game->get_tick();
    counter -= delta;

    while (counter < 0) {
      s.making_weapon.mode += 1;
      if (s.making_weapon.mode == 7) {
        /* Done making sword or shield. */
        building->stop_activity();
        game->get_map()->set_serf_index(pos, 0);

        Resource::Type res = building->is_playing_sfx() ? Resource::TypeShield :
                                                          Resource::TypeSword;
        if (building->is_playing_sfx()) {
          building->stop_playing_sfx();
        } else {
          building->start_playing_sfx();
        }

        set_state(StateMoveResourceOut);
        s.move_resource_out.res = 1 + res;
        s.move_resource_out.res_dest = 0;
        s.move_resource_out.next_state = StateDropResourceOut;

        /* Update resource stats. */
        Player *player = game->get_player(get_owner());
        player->increase_res_count(res);
        return;
      } else {
        counter += 576;
      }
    }
  }
}

void
Serf::handle_serf_making_tool_state() {
  Building *building = game->get_building(game->get_map()->get_obj_index(pos));

  if (s.making_tool.mode == 0) {
    if (building->use_resources_in_stocks()) {
      s.making_tool.mode = 1;
      animation = 144;
      counter = counter_from_animation[animation];
      tick = game->get_tick();

      game->get_map()->set_serf_index(pos, index);
    }
  } else {
    uint16_t delta = game->get_tick() - tick;
    tick = game->get_tick();
    counter -= delta;

    while (counter < 0) {
      s.making_tool.mode += 1;
      if (s.making_tool.mode == 4) {
        /* Done making tool. */
        game->get_map()->set_serf_index(pos, 0);

        Player *player = game->get_player(get_owner());
        int total_tool_prio = 0;
        for (int i = 0; i < 9; i++) total_tool_prio += player->get_tool_prio(i);
        total_tool_prio >>= 4;

        int res = -1;
        if (total_tool_prio > 0) {
          /* Use defined tool priorities. */
          int prio_offset = (total_tool_prio*game->random_int()) >> 16;
          for (int i = 0; i < 9; i++) {
            prio_offset -= player->get_tool_prio(i) >> 4;
            if (prio_offset < 0) {
              res = Resource::TypeShovel + i;
              break;
            }
          }
        } else {
          /* Completely random. */
          res = Resource::TypeShovel + ((9*game->random_int()) >> 16);
        }

        set_state(StateMoveResourceOut);
        s.move_resource_out.res = 1 + res;
        s.move_resource_out.res_dest = 0;
        s.move_resource_out.next_state = StateDropResourceOut;

        /* Update resource stats. */
        player->increase_res_count(static_cast<Resource::Type>(res));
        return;
      } else {
        counter += 1536;
      }
    }
  }
}

void
Serf::handle_serf_building_boat_state() {
  PMap map = game->get_map();
  Building *building = game->get_building(map->get_obj_index(pos));

  if (s.building_boat.mode == 0) {
    if (!building->use_resource_in_stock(0)) return;
    building->boat_clear();

    s.building_boat.mode = 1;
    animation = 146;
    counter = counter_from_animation[animation];
    tick = game->get_tick();

    map->set_serf_index(pos, index);
  } else {
    uint16_t delta = game->get_tick() - tick;
    tick = game->get_tick();
    counter -= delta;

    while (counter < 0) {
      s.building_boat.mode += 1;
      if (s.building_boat.mode == 9) {
        /* Boat done. */
        MapPos new_pos = map->move_down_right(pos);
        if (map->has_serf(new_pos)) {
          /* Wait for flag to be free. */
          s.building_boat.mode -= 1;
          counter = 0;
        } else {
          /* Drop boat at flag. */
          building->boat_clear();
          map->set_serf_index(pos, 0);

          set_state(StateMoveResourceOut);
          s.move_resource_out.res = 1 + Resource::TypeBoat;
          s.move_resource_out.res_dest = 0;
          s.move_resource_out.next_state = StateDropResourceOut;

          /* Update resource stats. */
          Player *player = game->get_player(get_owner());
          player->increase_res_count(Resource::TypeBoat);

          break;
        }
      } else {
        /* Continue building. */
        building->boat_do();
        animation = 145;
        counter += 1408;
      }
    }
  }
}

void
Serf::handle_serf_looking_for_geo_spot_state() {
  int tries = 2;
  PMap map = game->get_map();
  for (int i = 0; i < 8; i++) {
    int dist = ((game->random_int() >> 2) & 0x3f) + 1;
    MapPos dest = map->pos_add_spirally(pos, dist);

    int obj = map->get_obj(dest);
    if (obj == Map::ObjectNone) {
      Map::Terrain t1 = map->type_down(dest);
      Map::Terrain t2 = map->type_up(dest);
      Map::Terrain t3 = map->type_down(map->move_up_left(dest));
      Map::Terrain t4 = map->type_up(map->move_up_left(dest));
      if ((t1 >= Map::TerrainTundra0 && t1 <= Map::TerrainSnow0) ||
          (t2 >= Map::TerrainTundra0 && t2 <= Map::TerrainSnow0) ||
          (t3 >= Map::TerrainTundra0 && t3 <= Map::TerrainSnow0) ||
          (t4 >= Map::TerrainTundra0 && t4 <= Map::TerrainSnow0)) {
        set_state(StateFreeWalking);
        s.free_walking.dist_col = Map::get_spiral_pattern()[2 * dist];
        s.free_walking.dist_row = Map::get_spiral_pattern()[2 * dist + 1];
        s.free_walking.neg_dist1 = -Map::get_spiral_pattern()[2 * dist];
        s.free_walking.neg_dist2 = -Map::get_spiral_pattern()[2 * dist + 1];
        s.free_walking.flags = 0;
        tick = game->get_tick();
        Log::Verbose["serf"] << "looking for geo spot: found, dist "
                             << s.free_walking.dist_col << ", "
                             << s.free_walking.dist_row << ".";
        return;
      }
    } else if (obj >= Map::ObjectSignLargeGold &&
               obj <= Map::ObjectSignEmpty) {
      tries -= 1;
      if (tries == 0) break;
    }
  }

  set_state(StateWalking);
  s.walking.dest = 0;
  s.walking.dir1 = -2;
  s.walking.dir = 0;
  s.walking.wait_counter = 0;
  counter = 0;
}

void
Serf::handle_serf_sampling_geo_spot_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  PMap map = game->get_map();
  while (counter < 0) {
    if (s.free_walking.neg_dist1 == 0 &&
      map->get_obj(pos) == Map::ObjectNone) {
      if (map->get_res_type(pos) == Map::MineralsNone ||
          map->get_res_amount(pos) == 0) {
        /* No available resource here. Put empty sign. */
        map->set_object(pos, Map::ObjectSignEmpty, -1);
      } else {
        s.free_walking.neg_dist1 = -1;
        animation = 142;

        /* Select small or large sign with the right resource depicted. */
        int obj = Map::ObjectSignLargeGold +
          2*(map->get_res_type(pos)-1) +
          (map->get_res_amount(pos) < 12 ? 1 : 0);
        map->set_object(pos, (Map::Object)obj, -1);

        /* Check whether a new notification should be posted. */
        int show_notification = 1;
        for (int i = 0; i < 60; i++) {
          MapPos pos_ = map->pos_add_spirally(pos, 1+i);
          if ((map->get_obj(pos_) >> 1) == (obj >> 1)) {
            show_notification = 0;
            break;
          }
        }

        /* Create notification for found resource. */
        if (show_notification) {
          Message::Type mtype;
          switch (map->get_res_type(pos)) {
            case Map::MineralsCoal:
              mtype = Message::TypeFoundCoal;
              break;
            case Map::MineralsIron:
              mtype = Message::TypeFoundIron;
              break;
            case Map::MineralsGold:
              mtype = Message::TypeFoundGold;
              break;
            case Map::MineralsStone:
              mtype = Message::TypeFoundStone;
              break;
            default:
              NOT_REACHED();
          }
          game->get_player(get_owner())->add_notification(mtype, pos,
                                                    map->get_res_type(pos) - 1);
        }

        counter += 64;
        continue;
      }
    }

    set_state(StateFreeWalking);
    s.free_walking.neg_dist1 = -128;
    s.free_walking.neg_dist2 = 0;
    s.free_walking.flags = 0;
    counter = 0;
  }
}

void
Serf::handle_serf_knight_engaging_building_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter < 0) {
    PMap map = game->get_map();
    Map::Object obj = map->get_obj(map->move_up_left(pos));
    if (obj >= Map::ObjectSmallBuilding &&
        obj <= Map::ObjectCastle) {
      Building *building = game->get_building(map->get_obj_index(
                                              map->move_up_left(pos)));
      if (building->is_done() &&
          building->is_military() &&
          building->get_owner() != get_owner() &&
          building->has_knight()) {
        if (building->is_under_attack()) {
          game->get_player(building->get_owner())->add_notification(
                                                 Message::TypeUnderAttack,
                                                       building->get_position(),
                                                                   get_owner());
        }

        /* Change state of attacking knight */
        counter = 0;
        state = StateKnightPrepareAttacking;
        animation = 168;

        Serf *def_serf = building->call_defender_out();

        s.attacking.def_index = def_serf->get_index();

        /* Change state of defending knight */
        set_other_state(def_serf, StateKnightLeaveForFight);
        def_serf->s.leaving_building.next_state = StateKnightPrepareDefending;
        def_serf->counter = 0;
        return;
      }
    }

    /* No one to defend this building. Occupy it. */
    set_state(StateKnightOccupyEnemyBuilding);
    animation = 179;
    counter = counter_from_animation[animation];
    tick = game->get_tick();
  }
}

void
Serf::set_fight_outcome(Serf *attacker, Serf *defender) {
  /* Calculate "morale" for attacker. */
  int exp_factor = 1 << (attacker->get_type() - TypeKnight0);
  int land_factor = 0x1000;
  if (attacker->get_owner() != game->get_map()->get_owner(attacker->pos)) {
    land_factor = game->get_player(attacker->get_owner())->get_knight_morale();
  }

  int morale = (0x400*exp_factor * land_factor) >> 16;

  /* Calculate "morale" for defender. */
  int def_exp_factor = 1 << (defender->get_type() - TypeKnight0);
  int def_land_factor = 0x1000;
  if (defender->get_owner() != game->get_map()->get_owner(defender->pos)) {
    def_land_factor =
                   game->get_player(defender->get_owner())->get_knight_morale();
  }

  int def_morale = (0x400*def_exp_factor * def_land_factor) >> 16;

  int player = -1;
  int value = -1;
  Type ktype = TypeNone;
  int r = ((morale + def_morale)*game->random_int()) >> 16;
  if (r < morale) {
    player = defender->get_owner();
    value = def_exp_factor;
    ktype = defender->get_type();
    attacker->s.attacking.attacker_won = 1;
    Log::Debug["serf"] << "Fight: " << morale << " vs " << def_morale
    << " (" << r << "). Attacker winning.";
  } else {
    player = attacker->get_owner();
    value = exp_factor;
    ktype = attacker->get_type();
    attacker->s.attacking.attacker_won = 0;
    Log::Debug["serf"] << "Fight: " << morale << " vs " << def_morale
                       << " (" << r << "). Defender winning.";
  }

  game->get_player(player)->decrease_military_score(value);
  attacker->s.attacking.move = game->random_int() & 0x70;
}

void
Serf::handle_serf_knight_prepare_attacking() {
  Serf *def_serf = game->get_serf(s.attacking.def_index);
  if (def_serf->state == StateKnightPrepareDefending) {
    /* Change state of attacker. */
    set_state(StateKnightAttacking);
    counter = 0;
    tick = game->get_tick();

    /* Change state of defender. */
    set_other_state(def_serf, StateKnightDefending);
    def_serf->counter = 0;

    set_fight_outcome(this, def_serf);
  }
}

void
Serf::handle_serf_knight_leave_for_fight_state() {
  tick = game->get_tick();
  counter = 0;

  if (game->get_map()->get_serf_index(pos) == index ||
      !game->get_map()->has_serf(pos)) {
    leave_building(1);
  }
}

void
Serf::handle_serf_knight_prepare_defending_state() {
  counter = 0;
  animation = 84;
}

void
Serf::handle_knight_attacking() {
  const int moves[] =  {
    1, 2, 4, 2, 0, 2, 4, 2, 1, 0, 2, 2, 3, 0, 0, -1,
    3, 2, 2, 3, 0, 4, 1, 3, 2, 4, 2, 2, 3, 0, 0, -1,
    2, 1, 4, 3, 2, 2, 2, 3, 0, 3, 1, 2, 0, 2, 0, -1,
    2, 1, 3, 2, 4, 2, 3, 0, 0, 4, 2, 0, 2, 1, 0, -1,
    3, 1, 0, 2, 2, 1, 0, 2, 4, 2, 2, 3, 0, 0, -1,
    0, 3, 1, 2, 3, 4, 2, 1, 2, 0, 2, 4, 0, 2, 0, -1,
    0, 2, 1, 2, 4, 2, 3, 0, 2, 4, 3, 2, 0, 0, -1,
    0, 0, 1, 4, 3, 2, 2, 1, 2, 0, 0, 4, 3, 0, -1
  };

  const int fight_anim[] = {
    24, 35, 41, 56, 67, 72, 83, 89, 100, 121, 0, 0, 0, 0, 0, 0,
    26, 40, 42, 57, 73, 74, 88, 104, 106, 120, 122, 0, 0, 0, 0, 0,
    17, 18, 23, 33, 34, 38, 39, 98, 102, 103, 113, 114, 118, 119, 0, 0,
    130, 133, 134, 135, 147, 148, 161, 162, 164, 166, 167, 0, 0, 0, 0, 0,
    50, 52, 53, 70, 129, 131, 132, 146, 149, 151, 0, 0, 0, 0, 0, 0
  };

  const int fight_anim_max[] = { 10, 11, 14, 11, 10 };

  Serf *def_serf = game->get_serf(s.attacking.def_index);

  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  def_serf->tick = tick;
  counter -= delta;
  def_serf->counter = counter;

  while (counter < 0) {
    int move = moves[s.attacking.move];
    if (move < 0) {
      if (s.attacking.attacker_won == 0) {
        /* Defender won. */
        if (state == StateKnightAttackingFree) {
          set_other_state(def_serf, StateKnightDefendingVictoryFree);

          def_serf->animation = 180;
          def_serf->counter = 0;

          /* Attacker dies. */
          set_state(StateKnightAttackingDefeatFree);
          animation = 152 + get_type();
          counter = 255;
          set_type(TypeDead);
        } else {
          /* Defender returns to building. */
          def_serf->enter_building(-1, 1);

          /* Attacker dies. */
          set_state(StateKnightAttackingDefeat);
          animation = 152 + get_type();
          counter = 255;
          set_type(TypeDead);
        }
      } else {
        /* Attacker won. */
        if (state == StateKnightAttackingFree) {
          set_state(StateKnightAttackingVictoryFree);
          animation = 168;
          counter = 0;

          // pyrdacor's Freeserf.NET has an extra variable here s.AttackingVictoryFree.DefenderIndex
          //  which is set to the index of the Defender who dies, I assume to properly clear the defender?
          //  this is reported to be a crash bug, investigate further how this action works and copy his solution
          s.attacking_victory_free.move = def_serf->s.defending_free.field_D;
          s.attacking_victory_free.dist_col =
                                      def_serf->s.defending_free.other_dist_col;
          s.attacking_victory_free.dist_row =
                                      def_serf->s.defending_free.other_dist_row;
        } else {
          set_state(StateKnightAttackingVictory);
          animation = 168;
          counter = 0;

          int obj = game->get_map()->get_obj_index(
                                  game->get_map()->move_up_left(def_serf->pos));
          Building *building = game->get_building(obj);
          building->requested_knight_defeat_on_walk();
        }

        /* Defender dies. */
        def_serf->tick = game->get_tick();
        def_serf->animation = 147 + get_type();
        def_serf->counter = 255;
        def_serf->set_type(TypeDead);
      }
    } else {
      /* Go to next move in fight sequence. */
      s.attacking.move += 1;
      if (s.attacking.attacker_won == 0) move = 4 - move;
      s.attacking.field_D = move;

      int off = (game->random_int() * fight_anim_max[move]) >> 16;
      int a = fight_anim[move*16 + off];

      animation = 146 + ((a >> 4) & 0xf);
      def_serf->animation = 156 + (a & 0xf);
      counter = 72 + (game->random_int() & 0x18);
      def_serf->counter = counter;
    }
  }
}

void
Serf::handle_serf_knight_attacking_victory_state() {
  Serf *def_serf = game->get_serf(s.attacking.def_index);

  uint16_t delta = game->get_tick() - def_serf->tick;
  def_serf->tick = game->get_tick();
  def_serf->counter -= delta;

  if (def_serf->counter < 0) {
    game->delete_serf(def_serf);
    s.attacking.def_index = 0;

    set_state(StateKnightEngagingBuilding);
    tick = game->get_tick();
    counter = 0;
  }
}

void
Serf::handle_serf_knight_attacking_defeat_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter < 0) {
    game->get_map()->set_serf_index(pos, 0);
    game->delete_serf(this);
  }
}

void
Serf::handle_knight_occupy_enemy_building() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter >= 0) {
    return;
  }

  Building *building = game->get_building_at_pos(game->get_map()->move_up_left(pos));
  if (building != NULL) {
    if (!building->is_burning() && building->is_military()) {
      if (building->get_owner() == owner) {
        /* Enter building if there is space. */
        if (building->get_type() == Building::TypeCastle) {
          enter_building(-2, 0);
          return;
        } else {
          if (building->is_enough_place_for_knight()) {
            /* Enter building */
            enter_building(-1, 0);
            building->knight_occupy();
            return;
          }
        }
      } else if (!building->has_knight()) {
        /* Occupy the building. */
        game->occupy_enemy_building(building, get_owner());

        if (building->get_type() == Building::TypeCastle) {
          counter = 0;
        } else {
          /* Enter building */
          enter_building(-1, 0);
          building->knight_occupy();
        }
        return;
      } else {
        set_state(StateKnightEngagingBuilding);
        animation = 167;
        counter = 191;
        return;
      }
    }
  }

  // the building is a nullptr, likely it was demolished while knight attempting to enter
  Log::Info["serf"] << "inside handle_knight_occupy_enemy_building(), a knight at pos " << pos << " is being set to StateLost because the enemy building he is attempting to occupy is a nullptr.  The building was probably demolished";
  Log::Debug["serf"] << "inside Serf::handle_knight_occupy_enemy_building(), serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
  set_state(StateLost);
  s.lost.field_B = 0;
  counter = 0;
}

void
Serf::handle_state_knight_free_walking() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  PMap map = game->get_map();
  while (counter < 0) {
    /* Check for enemy knights nearby. */
    for (Direction d : cycle_directions_cw()) {
      MapPos pos_ = map->move(pos, d);

      if (map->has_serf(pos_)) {
        Serf *other = game->get_serf_at_pos(pos_);
        if (get_owner() != other->get_owner()) {
          if (other->state == StateKnightFreeWalking) {
            pos_ = map->move_left(pos_);  // added fix from sternezaehler for Freeserf issue#460, pos becomes pos_
            if (can_pass_map_pos(pos_)) {
              int dist_col = s.free_walking.dist_col;
              int dist_row = s.free_walking.dist_row;

              set_state(StateKnightEngageDefendingFree);

              s.defending_free.dist_col = dist_col;
              s.defending_free.dist_row = dist_row;
              s.defending_free.other_dist_col = other->s.free_walking.dist_col;
              s.defending_free.other_dist_row = other->s.free_walking.dist_row;
              s.defending_free.field_D = 1;
              animation = 99;
              counter = 255;

              set_other_state(other, StateKnightEngageAttackingFree);
              other->s.attacking.field_D = d;
              other->s.attacking.def_index = get_index();
              return;
            }
          } else if (other->state == StateWalking &&
                     other->get_type() >= TypeKnight0 &&
                     other->get_type() <= TypeKnight4) {
            pos_ = map->move_left(pos_);
            if (can_pass_map_pos(pos_)) {
              int dist_col = s.free_walking.dist_col;
              int dist_row = s.free_walking.dist_row;

              set_state(StateKnightEngageDefendingFree);
              s.defending_free.dist_col = dist_col;
              s.defending_free.dist_row = dist_row;
              s.defending_free.field_D = 0;
              animation = 99;
              counter = 255;

              Flag *dest = game->get_flag(other->s.walking.dest);
              Building *building = dest->get_building();
              // got exception here, adding nullptr check
              //  it is definitely a nullptr issue, seeing this triggered
              // it seems reasonable that the building could be destroyed while the
              //  attacking knight is still out.  Unless this is supposed to be handled
              //  by the building being destroyed finding knights attacking it and somehow
              //  updating them... it seems okay to simply skip this error
              //  and see what happens to the attacking knight next
              if (building == nullptr){
                //Log::Error["serf"] << "inside handle_state_knight_free_walking, building is nullptr when trying to call building->has_inventory and possibly building->requested_knight_attacking_on_walk ! crashing";
                //throw ExceptionFreeserf("inside handle_state_knight_free_walking, building is nullptr!");
                Log::Warn["serf.cc"] << "inside handle_state_knight_free_walking, building is nullptr when trying to call building->has_inventory and possibly building->requested_knight_attacking_on_walk.  Ignore that call for now, will fight enemy knight here";
              }else{
                if(!building->has_inventory()) {
                  // all this does is set stock[0].requested -= 1; on the attacking knight's dest building
                  building->requested_knight_attacking_on_walk();
                }
              }

              set_other_state(other, StateKnightEngageAttackingFree);
              other->s.attacking.field_D = d;
              other->s.attacking.def_index = get_index();
              return;
            }
          }
        }
      }
    }

    handle_free_walking_common();
  }
}

void
Serf::handle_state_knight_engage_defending_free() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) counter += 256;
}

void
Serf::handle_state_knight_engage_attacking_free() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter < 0) {
    set_state(StateKnightEngageAttackingFreeJoin);
    animation = 167;
    counter += 191;
  }
}

void
Serf::handle_state_knight_engage_attacking_free_join() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter < 0) {
    set_state(StateKnightPrepareAttackingFree);
    animation = 168;
    counter = 0;

    Serf *other = game->get_serf(s.attacking.def_index);
    MapPos other_pos = other->pos;
    set_other_state(other, StateKnightPrepareDefendingFree);
    other->counter = counter;

    /* Adjust distance to final destination. */
    Direction d = (Direction)s.attacking.field_D;
    if (d == DirectionRight || d == DirectionDownRight) {
      other->s.defending_free.dist_col -= 1;
    } else if (d == DirectionLeft || d == DirectionUpLeft) {
      other->s.defending_free.dist_col += 1;
    }

    if (d == DirectionDownRight || d == DirectionDown) {
      other->s.defending_free.dist_row -= 1;
    } else if (d == DirectionUpLeft || d == DirectionUp) {
      other->s.defending_free.dist_row += 1;
    }

    other->start_walking(d, 32, 0);
    game->get_map()->set_serf_index(other_pos, 0);
  }
}

void
Serf::handle_state_knight_prepare_attacking_free() {
  Serf *other = game->get_serf(s.attacking.def_index);
  if (other->state == StateKnightPrepareDefendingFreeWait) {
    set_state(StateKnightAttackingFree);
    counter = 0;

    set_other_state(other, StateKnightDefendingFree);
    other->counter = 0;

    set_fight_outcome(this, other);
  }
}

void
Serf::handle_state_knight_prepare_defending_free() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter < 0) {
    set_state(StateKnightPrepareDefendingFreeWait);
    counter = 0;
  }
}

void
Serf::handle_knight_attacking_victory_free() {
  Serf *other = game->get_serf(s.attacking_victory_free.def_index);

  uint16_t delta = game->get_tick() - other->tick;
  other->tick = game->get_tick();
  other->counter -= delta;

  if (other->counter < 0) {
    game->delete_serf(other);

    int dist_col = s.attacking_victory_free.dist_col;
    int dist_row = s.attacking_victory_free.dist_row;

    set_state(StateKnightAttackingFreeWait);

    s.free_walking.dist_col = dist_col;
    s.free_walking.dist_row = dist_row;
    s.free_walking.neg_dist1 = 0;
    s.free_walking.neg_dist2 = 0;

    if (s.attacking.move != 0) {
      s.free_walking.flags = 1;
    } else {
      s.free_walking.flags = 0;
    }

    animation = 179;
    counter = 127;
    tick = game->get_tick();
  }
}

void
Serf::handle_knight_defending_victory_free() {
  animation = 180;
  counter = 0;
}

void
Serf::handle_serf_knight_attacking_defeat_free_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter < 0) {
    /* Change state of other. */
    Serf *other = game->get_serf(s.attacking.def_index);
    int dist_col = other->s.defending_free.dist_col;
    int dist_row = other->s.defending_free.dist_row;

    set_other_state(other, StateKnightFreeWalking);

    other->s.free_walking.dist_col = dist_col;
    other->s.free_walking.dist_row = dist_row;
    other->s.free_walking.neg_dist1 = 0;
    other->s.free_walking.neg_dist2 = 0;
    other->s.free_walking.flags = 0;

    other->animation = 179;
    other->counter = 0;
    other->tick = game->get_tick();

    /* Remove itself. */
    game->get_map()->set_serf_index(pos, other->index);
    game->delete_serf(this);
  }
}

void
Serf::handle_knight_attacking_free_wait() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter < 0) {
    if (s.free_walking.flags != 0) {
      set_state(StateKnightFreeWalking);
    } else {
      Log::Warn["serf"] << "inside handle_knight_attacking_free_wait(), a knight at pos " << get_pos() << " is being set to StateLost! because counter < 0 and free_walking = 0?";
      Log::Debug["serf"] << "inside Serf::handle_knight_attacking_free_wait(), serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
      set_state(StateLost);
    }

    counter = 0;
  }
}

void
Serf::handle_serf_state_knight_leave_for_walk_to_fight() {
  tick = game->get_tick();
  counter = 0;

  PMap map = game->get_map();
  if (map->get_serf_index(pos) != index && map->has_serf(pos)) {
    animation = 82;
    counter = 0;
    return;
  }

  Building *building = game->get_building(map->get_obj_index(pos));
  MapPos new_pos = map->move_down_right(pos);

  if (!map->has_serf(new_pos)) {
    /* For clean state change, save the values first. */
    /* TODO maybe knight_leave_for_walk_to_fight can
       share leaving_building state vars. */
    int dist_col = s.leave_for_walk_to_fight.dist_col;
    int dist_row = s.leave_for_walk_to_fight.dist_row;
    int field_D = s.leave_for_walk_to_fight.field_D;
    int field_E = s.leave_for_walk_to_fight.field_E;
    Serf::State next_state = s.leave_for_walk_to_fight.next_state;

    leave_building(pos);
    /* TODO names for leaving_building vars make no sense here. */
    s.leaving_building.field_B = dist_col;
    s.leaving_building.dest = dist_row;
    s.leaving_building.dest2 = field_D;
    s.leaving_building.dir = field_E;
    s.leaving_building.next_state = next_state;
  } else {
    Serf *other = game->get_serf_at_pos(new_pos);
    if (get_owner() == other->get_owner()) {
      animation = 82;
      counter = 0;
    } else {
      /* Go back to defending the building. */
      switch (building->get_type()) {
        case Building::TypeHut:
          set_state(StateDefendingHut);
          break;
        case Building::TypeTower:
          set_state(StateDefendingTower);
          break;
        case Building::TypeFortress:
          set_state(StateDefendingFortress);
          break;
        default:
          NOT_REACHED();
          break;
      }

      if (!building->knight_come_back_from_fight(this)) {
        animation = 82;
        counter = 0;
      }
    }
  }
}

// an "idle on path" serf appears to be one who is waiting for some work to do.  In
//  this state, the serf is shown as looking back and forth, but doesn't actually exist
//  on the map in terns of map->has_serf(pos) calls.  Other serfs can walk right through
//  it as though this serf is not even there.  
//
// when there is some work to do, such as to move a "scheduled" resource that was dropped
//  at an adjacent flag, this serf needs to "wake up" and start moving towards the flag
//
// however, in order for the serf to wake up, it needs to occupy a real pos.  But, because
//  only one serf can be in one tile at a time, if there is another serf occupying the pos
//  where this serf is, this serf cannot immediately wake, it must WaitIdleOnPath until the
//  blocking serf move 
// 
// in FreeSerf this is a game-breaking bug because sometimes the serf is never able to exit
//  the state, stopping all traffic forever on that road
//
    //
    // Bitmath explanation:
    //  Serf::tick is an unsigned 16bit short integer          xxxxxxxx xxxxxxxx
    //  0xff is a 32bit(?) signed(?) integer 00000000 00000000 00000000 11111111
    //
    //    a bitwise AND against a bunch of 1's does nothing to them
    //    a bitwise AND against a bunch of 0's erases them
    // 
    //  so ANDing some mix of 1s and 0s in Serf::tick with 0xff effectively wipes any value > 1111 1111 (0xff is unsigned 255 or signed -1)
    //  I assume this is either because some extra information is being stored in the most significant (leftmost) digits, and this wipes it
    //  or, this is a way to use 'tick' as a 0-255 (or -127 to 127) rolling counter and wiping the most significant digits ensures
    //   that it is never >255 during conversion?
    //  so Serf::tick's new value after AND conversion is      00000000 xxxxxxxx  
    //
    //  then 6 is added.   Because field_E seems to be used as a Direction, 
    //    the only valid values can be 0-5 (East/Right through NorthEast/UpRight)
    //  this means that Serf::tick & 0xff must result in -6 through -1, which after +6 results in 0 through 5, the valid Directions
    //  negative numbers in a signed 32bit integer look like
    //                                       1xxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
    //                                       ^ most significant bit
    //                     -6 looks like:    11111111 11111111 11111111 11111010
    //                     -1 looks like:    11111111 11111111 11111111 11111111
    //                      0 looks like:    00000000 00000000 00000000 00000000
    //                      5 looks like:    00000000 00000000 00000000 00000101
    //
    //                      6 looks like:    00000000 00000000 00000000 00000110
    //
    //   but this should be IMPOSSIBLE because the only possible results of & 255 should be:
    //                                       00000000 00000000 00000000 xxxxxxxx
    //  ??????????????????????????
    //
    // because this makes no sense, what if I just set field_E to the direction that makes sense?
    //  that is how "other_flag" does it.  Instead of this crazy bit math
    // no I think this is causing issues, need to put a check in place to flag when these two values diff
void
Serf::handle_serf_idle_on_path_state() {
  // first, check the Flag the serf is closer to
  Flag *flag = game->get_flag(s.idle_on_path.flag);
  if (flag == nullptr) {
    // if this serf's Flag is deleted, make it Lost 
    Log::Warn["serf"] << "inside handle_serf_idle_on_path_state(), a serf at pos " << get_pos() << " is being set to StateLost! because the serf's s.idle_on_path.flag Flag is a nullptr!";

    //debug
    //if (game->get_flag(recent_dest) != nullptr){
    //  Log::Debug["serf"] << "inside handle_serf_idle_on_path_state, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which has pos " << game->get_flag(recent_dest)->get_position();
    //}else{
    //  Log::Debug["serf"] << "inside handle_serf_idle_on_path_state, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which is a nullptr";
    //}
    Log::Debug["serf"] << "inside Serf::handle_serf_idle_on_path_state(), serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
    set_state(StateLost);
    return;
  }
  // check the flag in each direction to see if it needs a transporter 
  //  to come pick up a resource, or a serf if this is a sailor and 
  //  option_CanTransportSerfsInBoats is set
  Direction rev_dir = s.idle_on_path.rev_dir;
  if (get_type() == Serf::TypeSailor && flag->has_serf_waiting_for_boat()){
    Log::Debug["serf"] << "debug: sailor inside handle_serf_idle_on_path_state, sailor found flag at pos " << flag->get_position() << " with serf_waiting_for_boat, setting dir this way";
    // this statement appears to instruct the sailor to wake and travel towards the flag that needs pickup
    //  but I don't understand the details
    // * Set walking dir in field_E. */
    s.idle_on_path.field_E = (tick & 0xff) + 6;
  } else if (flag->is_scheduled(rev_dir)) {
    //Log::Debug["serf"] << "debug: serf inside handle_serf_idle_on_path_state, this flag is scheduled";
    s.idle_on_path.field_E = (tick & 0xff) + 6;
  } else {
    // check the other flag, the one this serf is farther from
    Flag *other_flag = flag->get_other_end_flag(rev_dir);
    Direction other_dir = flag->get_other_end_dir((Direction)rev_dir);
    if (get_type() == Serf::TypeSailor && other_flag->has_serf_waiting_for_boat()){
      //Log::Debug["serf"] << "debug: inside handle_serf_idle_on_path_state, sailor found other_flag at pos " << other_flag->get_position() << " with serf_waiting_for_boat, setting dir that way";
      s.idle_on_path.field_E = reverse_direction(rev_dir);
    } else if (other_flag && other_flag->is_scheduled(other_dir)) {
      //Log::Debug["serf"] << "debug: inside handle_serf_idle_on_path_state, other flag is scheduled";
      s.idle_on_path.field_E = reverse_direction(rev_dir);
    } else {
      // nothing for this serf to do, no reason to wake, remain idle on path
      return;
    }
  }

  // if this point is reached, this idle serf must wake up and start heading to one of the two flags
  //Log::Info["serf"] << "debug: idle/passthru transporter requested to wake at pos " << pos <<", field_E: " << s.idle_on_path.field_E;

  PMap map = game->get_map();
  // if there is no serf already at this pos, this serf can wake and take
  //  this pos and start heading to the flag for pickup 
  if (!map->has_serf(pos)) {
    map->clear_idle_serf(pos);
    map->set_serf_index(pos, index);
    int dir = s.idle_on_path.field_E;  // what is the purpose of setting int dir if it isn't used?
    set_state(StateTransporting);
    s.transporting.res = Resource::TypeNone;
    s.transporting.wait_counter = 0;
    s.transporting.dir = dir;
    tick = game->get_tick();
    counter = 0;
  } else {
    // there is already a serf at this pos, so this idle/passthru serf cannot take this spot and must wait
    //  this is what StateWaitIdleOnPath indicates
    Log::Debug["serf.cc"] << "inside Serf::handle_serf_idle_on_path_state, a serf of type " << NameSerf[get_type()] << " at pos " << pos << " is about to be set to StateWaitIdleOnPath because it was called to wake but there is another serf at this pos.  The other serf has type " << NameSerf[game->get_serf_at_pos(pos)->get_type()] << ", index " << game->get_serf_at_pos(pos)->get_index() << ", pos " << game->get_serf_at_pos(pos)->get_pos();
    set_state(StateWaitIdleOnPath);

    // DEBUG stuck serfs 
    //if (split_merge_tainted){
      //Log::Debug["serf.cc"] << "inside Serf::handle_serf_idle_on_path_state, a serf of type " << NameSerf[get_type()] << " at pos " << pos << " is MERGE/SPLIT TAINTED";
    //}
  }
}

// this state appears to be used for an idle/passthru serf
//  that was called to wake and walk to flag to pick up a resource
//  but its pos was occupied by another serf so it could not wake 
//  and take the pos
//
// there is some bug in FreeSerf related to this logic
//  as often the serf in this state never becomes freed
//  It appears to happen when two other serfs are travelling
//  from opposite ends of the road and the WaitIdleOnPath serf is
//  trying to wake/appear in the pos between them
//
// Wait.. I thought I remembered seeing this happen to non-transporter
//  serf types (resulting in them never reaching their buildings, causing its own issues)
//  but this logic below can only be applied to transporters.  I am thinking it is worth flagging
//  when any non-transporter serf is in this state
void
Serf::handle_serf_wait_idle_on_path_state() {
  PMap map = game->get_map();

  // to help debug serfs stuck in WaitIdleOnPath issues
  if (get_type() != Serf::TypeTransporter){
    Log::Warn["serf.cc"] << "inside Serf::handle_serf_wait_idle_on_path_state, WARNING a non-transporter Serf of type " << get_type() << " at pos " << get_pos() << " is in WaitIdleOnPath state but this state should only happen to Transporters!";
    game->set_debug_mark_pos(get_pos(), "blue");
    game->pause();
  }

  
  // if there is no longer a serf blocking this pos, wake and head
  //  towards the flag to pick up resource
  if (!map->has_serf(pos)) {
    /* Duplicate code from handle_serf_idle_on_path_state() */
    map->clear_idle_serf(pos);
    map->set_serf_index(pos, index);
    int dir = s.idle_on_path.field_E;  // what is the point of setting int dir if it is not used?
    set_state(StateTransporting);
    s.transporting.res = Resource::TypeNone;
    s.transporting.wait_counter = 0;
    s.transporting.dir = dir;
    tick = game->get_tick();
    counter = 0;
  }
  // otherwise, remain in this state (forever?) until the position
  //  opens
}

void
Serf::handle_scatter_state() {
  /* Choose a random, empty destination */
  while (1) {
    int r = game->random_int();
    int col = (r & 0xf);
    if (col < 8) col -= 16;
    int row = ((r >> 8) & 0xf);
    if (row < 8) row -= 16;

    PMap map = game->get_map();
    MapPos dest = map->pos_add(pos, col, row);
    if (map->get_obj(dest) == 0 && map->get_height(dest) > 0) {
      if (get_type() >= TypeKnight0 && get_type() <= TypeKnight4) {
        set_state(StateKnightFreeWalking);
      } else {
        set_state(StateFreeWalking);
      }

      s.free_walking.dist_col = col;
      s.free_walking.dist_row = row;
      s.free_walking.neg_dist1 = -128;
      s.free_walking.neg_dist2 = -1;
      s.free_walking.flags = 0;
      counter = 0;
      return;
    }
  }
}

void
Serf::handle_serf_finished_building_state() {
  PMap map = game->get_map();
  if (!map->has_serf(map->move_down_right(pos))) {
    set_state(StateReadyToLeave);
    s.leaving_building.dest = 0;
    s.leaving_building.field_B = -2;
    s.leaving_building.dir = 0;
    s.leaving_building.next_state = StateWalking;

    if (map->get_serf_index(pos) != index && map->has_serf(pos)) {
      animation = 82;
    }
  }
}

void
Serf::handle_serf_wake_at_flag_state() {
  PMap map = game->get_map();
  if (!map->has_serf(pos)) {
    map->clear_idle_serf(pos);
    map->set_serf_index(pos, index);
    tick = game->get_tick();
    counter = 0;

    // I suspect this is simply the result of a road being deleted, the idle transporter serf is woken 
    //  and sent home, change this to Debug loglevel if kept
    //Log::Warn["serf"] << "inside handle_serf_wake_at_flag_state(), a serf at pos " << get_pos() << " is being set to StateLost! because map->has_serf(" << pos << ") says there is no serf here!";
    if (get_type() == TypeSailor) {
      set_state(StateLostSailor);
    } else {

      //debug
      //  not logging this here because it appears to happen to transporters working roads that have just been deleted, which is common
      //if (game->get_flag(recent_dest) != nullptr){
      //  Log::Debug["serf"] << "inside handle_serf_wake_at_flag_state, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which has pos " << game->get_flag(recent_dest)->get_position();
      //}else{
      //  Log::Debug["serf"] << "inside handle_serf_take_at_flag_state, serf of type " << NameSerf[type] << " being set to Lost, dest when it exited Inventory was flag #" << recent_dest << " which is a nullptr";
      //}
      Log::Debug["serf"] << "inside Serf::handle_serf_wake_at_flag_state(), serf #" << this->get_index() << " of type " << NameSerf[type] << " being set to Lost";
      set_state(StateLost);
      s.lost.field_B = 0;
    }
  }
}

void
Serf::handle_serf_wake_on_path_state() {
  //Log::Info["serf"] << "debug: inside handle_serf_wake_on_path_state, a serf of type " << NameSerf[get_type()] << " at pos " << get_pos() << " is about to be set to StateWaitIdleOnPath !";
  set_state(StateWaitIdleOnPath);
  
  // DEBUG stuck serfs 
  //if (split_merge_tainted){
    //Log::Debug["serf"] << "debugging StateWaitIdleOnPath issues: inside handle_serf_wake_on_path_state, a serf of type " << NameSerf[get_type()] << " at pos " << get_pos() << " is MERGE/SPLIT TAINTED";
  //}

  for (Direction d : cycle_directions_ccw()) {
    if (BIT_TEST(game->get_map()->paths(pos), d)) {
      s.idle_on_path.field_E = d;  // this appears to be the Dir that the serf is to face/start walking on wake to go to one of the end flags
                                   // how does it know which of the two directions is the correct one???
      break;
    }
  }
}

void
Serf::handle_serf_defending_state(const int training_params[]) {
  switch (get_type()) {
  case TypeKnight0:
  case TypeKnight1:
  case TypeKnight2:
  case TypeKnight3:
    train_knight(training_params[get_type()-TypeKnight0]);
  case TypeKnight4: /* Cannot train anymore. */
    break;
  default:
    NOT_REACHED();
    break;
  }
}

void
Serf::handle_serf_defending_hut_state() {
  const int training_params[] = { 250, 125, 62, 31 };
  handle_serf_defending_state(training_params);
}

void
Serf::handle_serf_defending_tower_state() {
  const int training_params[] = { 1000, 500, 250, 125 };
  handle_serf_defending_state(training_params);
}

void
Serf::handle_serf_defending_fortress_state() {
  const int training_params[] = { 2000, 1000, 500, 250 };
  handle_serf_defending_state(training_params);
}

void
Serf::handle_serf_defending_castle_state() {
  const int training_params[] = { 4000, 2000, 1000, 500 };
  handle_serf_defending_state(training_params);
}

void
Serf::handle_serf_wait_for_boat_state() {
  //Log::Info["serf"] << "debug: a serf at pos " << pos << " is in state Serf::StateWaitForBoat";
  /* Wait for the sailor to paddle over */
  animation = 81 + s.walking.dir;
  counter = 0;
}

void
Serf::handle_serf_boat_passenger_state() {
  //Log::Info["serf"] << "debug: a serf at pos " << pos << " is in state Serf::StateBoatPassenger";
  // do nothing
  // in fact, this state should never be checked because serfs in BoatPassenger 
  //  state do not have an attached map pos and so should never be checked for drawing/update?
}

/* removing AdvancedDemolition for now, see https://github.com/forkserf/forkserf/issues/180
void
Serf::handle_serf_exit_building_for_demolition_state() {
  Log::Info["serf.cc"] << "inside Serf::handle_serf_exit_building_for_demolition_state, a serf at pos " << pos << " is in state Serf::StateExitBuildingForDemolition";
  // copied leaving_building state for now...
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter < 20) {
    counter = 0;
    Log::Info["serf.cc"] << "inside Serf::handle_serf_exit_building_for_demolition_state, triggering next state";
    // set animation & counter for the *NEXT STATE*
    //int slope;
    Building *building = game->get_building_at_pos(pos);
    if (building == nullptr){
      Log::Warn["serf.cc"] << "inside Serf::handle_serf_exit_building_for_demolition_state, building is nullptr! setting serf to state Lost";
      set_state(Serf::StateLost);
      return;
    }

    // 
    // slope = 31 - road_building_slope[building->get_type()];
    // PMap map = game->get_map();
    // //// serf is to stop and turn around, facing UpLeft towards the building
    // //animation = get_walking_animation(map->get_height(pos) - map->get_height(map->move_up_left(pos)), Direction::DirectionUpLeft, 0);
    // // it seems there is no good "stopped" diagonal walking animation, instead try DirectionLeft(3)
    // animation = get_walking_animation(map->get_height(pos) - map->get_height(map->move_up_left(pos)), Direction::DirectionLeft, 0);
    // counter += (slope * counter_from_animation[animation]) >> 5;  // original value
    // //counter += 1;
    // //counter += (slope * counter_from_animation[animation]) >> 6;  // this is the counter for the NEXT STATE
    // //counter += counter_from_animation[animation];
    // Log::Info["serf.cc"] << "inside Serf::handle_serf_exit_building_for_demolition_state, animation = " << animation << ", counter = " << counter << ", slope = " << slope;
    // 

    // fire it up
    //building->burnup();
    // need to call game->demolish_building, which calls building->burnup() 
    //  and also clears the path into building and Flag<>Building association
    Player *player = game->get_player(get_owner());
    if (player == nullptr){
      Log::Warn["serf.cc"] << "inside Serf::handle_serf_exit_building_for_demolition_state, got nullptr from game->get_player for this serf's owning player with index " << get_owner() << "! returning early";
      return;
    }
    game->demolish_building(building->get_position(), player);
    // use the miner/builder walking-back-and-forth animation for now
    animation = 98;
    counter += 256;
    Log::Info["serf.cc"] << "inside Serf::handle_serf_exit_building_for_demolition_state, done with building->burnup(), about to call set_state(Serf::StateObservingDemolition)";
    set_state(Serf::StateObservingDemolition);
  }
}
*/

/* removing AdvancedDemolition for now, see https://github.com/forkserf/forkserf/issues/180
void
Serf::handle_serf_observing_demolition_state() {
  //
  // SERF NEEDS TO BE DRAWN a bit more up-left, find the appropriate animation step (walking up-left animation)?
  //
  //Log::Debug["serf.cc"] << "start of Serf::handle_serf_observing_demolition_state, a serf at pos " << pos << " is in state Serf::StateObservingDemolition";
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter < 0){
    // while the building is burning, it is a valid pointer with burning = true
    //  however once it finishes burning it is deleted and becomes a nullptr
    //  So, rather than checking for burning state, just wait until it becomes
    //  a nullptr and then handle the next state
    Building *building = game->get_building_at_pos(pos);
    //if (building == nullptr){
    //  Log::Warn["serf.cc"] << "inside inside Serf::handle_serf_observing_demolition_state, building-to-be-demolished expected at pos " << pos << " is nullptr!  setting this serf to StateLost";
    //  set_state(Serf::StateLost);
    //  return;
    //}
    // wait until the building is burnt, however long that takes
    //if (building->is_burning()){
    if (building != nullptr && building->is_burning()){
      Log::Debug["serf.cc"] << "inside inside Serf::handle_serf_observing_demolition_state, building is still burning, check again next update";
      counter += 256;  // reset counter so serf walks back and forth again
      return;
    }
    // ______________________
    //
    // do something here to gracefully move to cleanup/digging
    // maybe increase counter and let serf stand in place for a second
    // with animation "walking left (or right) / standing"
    //
    // ((((((((((((((((((((((
    
    // building has finished burning, go clean it up
    Log::Debug["serf.cc"] << "inside inside Serf::handle_serf_observing_demolition_state, building has finished burning, changing to Serf::StateCleaningRubble";
    counter = 383;    // this is the proper counter for "walk up-right a bit then dig, then walk left a bit" animation
    s.digging.substate = 0; // re-using this
    animation = 87; // start the next state with the digging animation
    set_state(Serf::StateCleaningRubble);
  }
}
*/

/* removing AdvancedDemolition for now, see https://github.com/forkserf/forkserf/issues/180
void
Serf::handle_serf_cleaning_rubble_state() {
  Log::Info["serf.cc"] << "inside Serf::handle_serf_cleaning_rubble_state, a serf at pos " << pos << " is in state Serf::StateCleaningRubble";
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;
  // re-using the s.digging struct and s.digging.substate as a counter
  //  indicating how many dig-then-move-left actions have been done
  if (s.digging.substate < 2 && counter < 0){
    animation = 87; // digging animation
    // repeat twice, for 3 total digs
    counter = 318;  // skip the "walk up-right a few steps into dig position" frames
    s.digging.substate++;
  }else if (s.digging.substate > 1 && counter < 78 && animation == 87){
    // after the final dig animation, walk right a few steps back to center of pos
    animation = 4;  // switch to the walking right animation
    counter = 22;  // this sets the proper distance from center (when counter = 0 reached)
  //}else if (s.digging.substate > 1 && counter < 0){
  }else if (s.digging.substate > 1 && counter < 8 && animation == 4){
    // done digging, back to center pos, move to next state
    counter = 0;
    set_state(Serf::StateFinishedBuilding); // re-using this state as it has desired effect
  }
}
*/

//
// explanation of how serf animations are handled:
///
// 'animation' value is which animation set (an array containing a series of sprites & their x/y pixel coordinates within the MapPos)
//      to play for a Serf in that state.  The serf sprites are actually drawn the graphics Frame in Viewport:draw_serf_row[_behind]
//           **The various animations can be seen in wdigger's FSStudio QT app https://github.com/wdigger/fsstudio**
//     For serf-travelling animations such as WALKING/BOATING/CARRYING there is a set of animations for each Direction travelled.
//      Additionally, there is a slope-adjusted animation for *each slope-Direction combination*
//      for example, "walking" animation is 82 for DirectionRight/0, 82 + 1 for DirectionDownRight/1,  82 + 2 for Dir 2, and so on
// 'counter' value represents how far BACKWARDS in time (from the last frame) the animation is playing right now. i.e how much *remains*
//      A counter of 0 or less than 0 means the last frame of the animation will be displayed this Game::update -> Viewport::draw_serf_row
//      the initial value of the counter is set for the *next serf state* as that new state is assigned, NOT for the current serf state
//      and when the serf enters that new state the appropriate handle_serf_XXXX_state code will begin counting down to zero from that
//      initial value.  The length of the initial counter value is normally equal to the number of frames in the animation, and the lookup
//      array 'counter_from_animation[]' provides the appropriate starting counter for a full play of each animation index.  
//    NON-walking animation example: for an animation with 150 frames, counter = 300 means that the first animation frame will be shown,
//      and every game update at normal speed 2 ticks will pass so counter will decrement by two.  A current counter of 200 means this
//      animation is a third complete, a counter of 0 or below means the animation complete and the counter should be set to zero (if below
//      zero) to leave the last frame shown until the next serf state can be entered.
//    WALKING/BOATING/CARRYING animation example: I think these animations repeat a small number of SPRITES (showing one step-pair or
//      on row of sailor oar) repeatedly, but the counter determines the position the serf is drawn, starting from one corner (max counter
//      value) and moving to the *center of MapPos* which is at counter 0.  Once the serf reaches the center of the MapPos, he must move
//      into the subsequent MapPos to continue his walk.  Again, the serf starts at far corner (max counter) and arrives at center of the 
//      pos (at 0 counter).  The get_walking_animation function gives the value as "4 + h_diff + 9*d" where h_diff is the difference in
//      height in pixels between the old & new MapPos traversed, and 'd' is the Direction number 0-5 (DirectionRight to DirectionUpRight)
//     Sometimes a serf must wait in a state for some time, or pause in the middle of a state, this is done by not decrementing the counter
//      and/or not entering the next serf state until eligible, keeping the counter static so the animation frame/sprite does not change.
//     To start a travelling serf at a x/y pixel-pos-within-MapPos anywhere other than the far corner of the MapPos he is walking from,
//      increment the 'counter' to the desired offset (lower for closer to center of MapPos, higher for closer to starting corner)
//     To stop a travelling serf early, stop decrementing the counter and/or changeinto next serf state before the counter reaches 0.
// 'height' values (of MapPos) are used for two things:  
//    NON-walking/sailing animations adjust the y axis offset so an animation is drawn one pixel higher or lower for each height-diff to
//      match the terrain slope.  (actually, is this even done? might just be travelling serfs)
//    WALKING/BOATING/CARRYING animations compare the difference in height between two MapPos affects how long it takes for a
//      serf to walk across the pos, steeper is slower.  The diffent slopes are handled by changing the animation
//      (NOT the counter) as there is a unique animation # for each Dir-Slope combination.  Steeper slopes animations have more frames,
//      longer max/initial counter, and the serf spends more time at each x/y pixel (i.e. they move more slowly).  The x/y coordinate 
//      change is handled by the progression of the animation, the coordinates increase/decrease continuously to show the serf moving 
//      across the MapPos.  Again, serf moves from far-corner to center of the current pos, then considers moving to next pos, usually in
//      the Serf::change_direction function (which would be better renamed to something like Serf::consider_next_pos_dir)
// 'tick' value is simply storing the game->tick of the last time the Game::update -> Serf::update ran for this serf to compare to the
//      current game tick.  The temporary 'delta' var represents this, the number of ticks that have passed since the last update.  The tick
//      delta is used to decrement the 'counter' which updates the animation frame *adjusted for game speed*.  At higher game speeds more
//      ticks pass per game update, this results in animations playing faster as expected.  At speed 1 (slower than default speed 2), only
//      a single tick passes per game update, but because the animations are designed for a tick of 2, the animations only actually change
//      every 2 ticks (i.e. there is only a unique animation frame for every 2 ticks worth of game time) so speed 1 appears choppy instead
//      of smoother than normal speed.
// 'if (counter < 0)' code blocks are "if the animation is done playing (i.e. the serf has completed the action or arrived at his walking 
//      destination), prepare to switch to the next Serf State - and set the appropriate initial counter value to start the *next* state's
//      animation at the necessary frame (which is usually the beginning, but might not be)
//
void
Serf::update() {
  //Log::Debug["serf.cc"] << "inside Serf::update, serf with index " << get_index() << " has type " << NameSerf[get_type()] << " and state name " << get_state_name(get_state());
  switch (state) {
  case StateNull: /* 0 */
    break;
  case StateWalking:
    handle_serf_walking_state();
    break;
  case StateTransporting:
    handle_serf_transporting_state();
    break;
  case StateIdleInStock:
    handle_serf_idle_in_stock_state();
    break;
  case StateEnteringBuilding:
    handle_serf_entering_building_state();
    break;
  case StateLeavingBuilding: /* 5 */
    handle_serf_leaving_building_state();
    break;
  case StateReadyToEnter:
    handle_serf_ready_to_enter_state();
    break;
  case StateReadyToLeave:
    handle_serf_ready_to_leave_state();
    break;
  case StateDigging:
    handle_serf_digging_state();
    break;
  case StateBuilding:
    handle_serf_building_state();
    break;
  case StateBuildingCastle: /* 10 */
    handle_serf_building_castle_state();
    break;
  case StateMoveResourceOut:
    handle_serf_move_resource_out_state();
    break;
  case StateWaitForResourceOut:
    handle_serf_wait_for_resource_out_state();
    break;
  case StateDropResourceOut:
    handle_serf_drop_resource_out_state();
    break;
  case StateDelivering:
    handle_serf_delivering_state();
    break;
  case StateReadyToLeaveInventory: /* 15 */
    handle_serf_ready_to_leave_inventory_state();
    break;
  case StateFreeWalking:
    handle_serf_free_walking_state();
    break;
  case StateLogging:
    handle_serf_logging_state();
    break;
  case StatePlanningLogging:
    handle_serf_planning_logging_state();
    break;
  case StatePlanningPlanting:
    handle_serf_planning_planting_state();  // this is Forester/Ranger planting trees
    break;
  case StatePlanting: /* 20 */ 
    handle_serf_planting_state();  // this is Forester/Ranger planting trees
    break;
  case StatePlanningStoneCutting:
    handle_serf_planning_stonecutting();
    break;
  case StateStoneCutterFreeWalking:
    handle_stonecutter_free_walking();
    break;
  case StateStoneCutting:
    handle_serf_stonecutting_state();
    break;
  case StateSawing:
    handle_serf_sawing_state();
    break;
  case StateLost: /* 25 */
    handle_serf_lost_state();
    break;
  case StateLostSailor:
    handle_lost_sailor();
    break;
  case StateFreeSailing:
    handle_free_sailing();
    break;
  case StateEscapeBuilding:
    handle_serf_escape_building_state();
    break;
  case StateMining:
    handle_serf_mining_state();
    break;
  case StateSmelting: /* 30 */
    handle_serf_smelting_state();
    break;
  case StatePlanningFishing:
    handle_serf_planning_fishing_state();
    break;
  case StateFishing:
    handle_serf_fishing_state();
    break;
  case StatePlanningFarming:
    handle_serf_planning_farming_state();
    break;
  case StateFarming:
    handle_serf_farming_state();
    break;
  case StateMilling: /* 35 */
    handle_serf_milling_state();
    break;
  case StateBaking:
    handle_serf_baking_state();
    break;
  case StatePigFarming:
    handle_serf_pigfarming_state();
    break;
  case StateButchering:
    handle_serf_butchering_state();
    break;
  case StateMakingWeapon:
    handle_serf_making_weapon_state();
    break;
  case StateMakingTool: /* 40 */
    handle_serf_making_tool_state();
    break;
  case StateBuildingBoat:
    handle_serf_building_boat_state();
    break;
  case StateLookingForGeoSpot:
    handle_serf_looking_for_geo_spot_state();
    break;
  case StateSamplingGeoSpot:
    handle_serf_sampling_geo_spot_state();
    break;
  case StateKnightEngagingBuilding:
    handle_serf_knight_engaging_building_state();
    break;
  case StateKnightPrepareAttacking: /* 45 */
    handle_serf_knight_prepare_attacking();
    break;
  case StateKnightLeaveForFight:
    handle_serf_knight_leave_for_fight_state();
    break;
  case StateKnightPrepareDefending:
    handle_serf_knight_prepare_defending_state();
    break;
  case StateKnightAttacking:
  case StateKnightAttackingFree:
    handle_knight_attacking();
    break;
  case StateKnightDefending:
  case StateKnightDefendingFree:
    /* The actual fight update is handled for the attacking knight. */
    break;
  case StateKnightAttackingVictory: /* 50 */
    handle_serf_knight_attacking_victory_state();
    break;
  case StateKnightAttackingDefeat:
    handle_serf_knight_attacking_defeat_state();
    break;
  case StateKnightOccupyEnemyBuilding:
    handle_knight_occupy_enemy_building();
    break;
  case StateKnightFreeWalking:
    handle_state_knight_free_walking();
    break;
  case StateKnightEngageDefendingFree:
    handle_state_knight_engage_defending_free();
    break;
  case StateKnightEngageAttackingFree:
    handle_state_knight_engage_attacking_free();
    break;
  case StateKnightEngageAttackingFreeJoin:
    handle_state_knight_engage_attacking_free_join();
    break;
  case StateKnightPrepareAttackingFree:
    handle_state_knight_prepare_attacking_free();
    break;
  case StateKnightPrepareDefendingFree:
    handle_state_knight_prepare_defending_free();
    break;
  case StateKnightPrepareDefendingFreeWait:
    /* Nothing to do for this state. */
    break;
  case StateKnightAttackingVictoryFree:
    handle_knight_attacking_victory_free();
    break;
  case StateKnightDefendingVictoryFree:
    handle_knight_defending_victory_free();
    break;
  case StateKnightAttackingDefeatFree:
    handle_serf_knight_attacking_defeat_free_state();
    break;
  case StateKnightAttackingFreeWait:
    handle_knight_attacking_free_wait();
    break;
  case StateKnightLeaveForWalkToFight: /* 65 */
    handle_serf_state_knight_leave_for_walk_to_fight();
    break;
  case StateIdleOnPath:
    handle_serf_idle_on_path_state();
    break;
  case StateWaitIdleOnPath:
    handle_serf_wait_idle_on_path_state();
    break;
  case StateWakeAtFlag:
    handle_serf_wake_at_flag_state();
    break;
  case StateWakeOnPath:
    handle_serf_wake_on_path_state();
    break;
  case StateDefendingHut: /* 70 */
    handle_serf_defending_hut_state();
    break;
  case StateDefendingTower:
    handle_serf_defending_tower_state();
    break;
  case StateDefendingFortress:
    handle_serf_defending_fortress_state();
    break;
  case StateScatter:
    handle_scatter_state();
    break;
  case StateFinishedBuilding:
    handle_serf_finished_building_state();
    break;
  case StateDefendingCastle: /* 75 */
    handle_serf_defending_castle_state();
    break;
  //
  //StateKnightAttackingDefeatFree would go here?
  //
  case StateWaitForBoat:
    handle_serf_wait_for_boat_state();
    break;
  case StateBoatPassenger:
    handle_serf_boat_passenger_state();
    break;
  /* removing AdvancedDemolition for now, see https://github.com/forkserf/forkserf/issues/180
  case StateExitBuildingForDemolition:
    handle_serf_exit_building_for_demolition_state();
    break;
  case StateObservingDemolition:
    handle_serf_observing_demolition_state();
    break;
  case StateCleaningRubble:
    handle_serf_cleaning_rubble_state();
    break;
  */
  default:
    Log::Debug["serf"] << "Serf state " << state << " isn't processed";
    state = StateNull;
  }
}

SaveReaderBinary&
operator >> (SaveReaderBinary &reader, Serf &serf) {
  uint8_t v8;
  reader >> v8;  // 0
  serf.owner = v8 & 3;
  serf.type = (Serf::Type)((v8 >> 2) & 0x1F);
  serf.sound = ((v8 >> 7) != 0);
  reader >> v8;  // 1
  serf.animation = v8;
  uint16_t v16;
  reader >> v16;  // 2
  serf.counter = v16;
  uint32_t v32;
  reader >> v32;  // 4
  serf.pos = v32;
  if (serf.pos != 0xFFFFFFFF) {
    serf.pos = serf.get_game()->get_map()->pos_from_saved_value(serf.pos);
  }
  reader >> v16;  // 8
  serf.tick = v16;
  reader >> v8;  // 10
  serf.state = (Serf::State)v8;

  Log::Verbose["savegame"] << "load serf " << serf.index << ": "
                           << Serf::get_state_name(serf.state);

  switch (serf.state) {
    case Serf::StateIdleInStock:
      reader >> v16;  // 11
      reader >> v8;  // 13
      reader >> v16;  // 14
      serf.s.idle_in_stock.inv_index = v16;
      break;

    case Serf::StateWalking: {
      reader >> v8;  // 11
      serf.s.walking.dir1 = (int8_t)v8;
      reader >> v16;  // 12
      serf.s.walking.dest = v16;
      reader >> v8;  // 14
      serf.s.walking.dir = (int8_t)v8;
      reader >> v8;  // 15
      serf.s.walking.wait_counter = v8;
      break;
    }
    case Serf::StateTransporting:
    case Serf::StateDelivering: {
      reader >> v8;  // 11
      serf.s.transporting.res = (Resource::Type)(((int8_t)v8) - 1);
      reader >> v16;  // 12
      serf.s.transporting.dest = v16;
      reader >> v8;  // 14
      serf.s.transporting.dir = (int8_t)v8;
      reader >> v8;  // 15
      serf.s.transporting.wait_counter = v8;
      break;
    }
    case Serf::StateEnteringBuilding:
      reader >> v8;  // 11
      serf.s.entering_building.field_B = (int8_t)v8;
      reader >> v16;  // 12
      serf.s.entering_building.slope_len = v16;
      break;

    case Serf::StateLeavingBuilding:
    case Serf::StateReadyToLeave: {
      reader >> v8;  // 11
      serf.s.leaving_building.field_B = (int8_t)v8;
      uint8_t dest = 0;
      reader >> dest;  // 12
      serf.s.leaving_building.dest = (int8_t)dest;
      reader >> v8;  // 13
      serf.s.leaving_building.dest2 = (int8_t)v8;
      reader >> v8;  // 14
      serf.s.leaving_building.dir = v8;
      reader >> v8;  // 15
      serf.s.leaving_building.next_state = (Serf::State)v8;
      if (serf.s.leaving_building.next_state == Serf::StateWalking) {
        serf.s.leaving_building.dest = dest;
      }
      break;
    }

    case Serf::StateReadyToEnter:
      reader >> v8;  // 11
      serf.s.ready_to_enter.field_B = v8;
      break;

    case Serf::StateDigging:
      reader >> v8;  // 11
      serf.s.digging.h_index = v8;
      reader >> v8;  // 12
      serf.s.digging.target_h = v8;
      reader >> v8;  // 13
      serf.s.digging.dig_pos = v8;
      reader >> v8;  // 14
      serf.s.digging.substate = v8;
      break;

    case Serf::StateBuilding:
      reader >> v8;  // 11
      serf.s.building.mode = (int8_t)v8;
      reader >> v16;  // 12
      serf.s.building.bld_index = v16;
      reader >> v8;  // 14
      serf.s.building.material_step = v8;
      reader >> v8;  // 15
      serf.s.building.counter = v8;
      break;

    case Serf::StateBuildingCastle:
      reader >> v8;  // 11
      reader >> v16;  // 12
      serf.s.building_castle.inv_index = v16;
      break;

    case Serf::StateMoveResourceOut:
    case Serf::StateDropResourceOut:
      reader >> v8;  // 11
      serf.s.move_resource_out.res = v8;
      reader >> v16;  // 12
      serf.s.move_resource_out.res_dest = v16;
      reader >> v8;  // 14
      reader >> v8;  // 15
      serf.s.move_resource_out.next_state = (Serf::State)v8;
      break;

    case Serf::StateReadyToLeaveInventory:
      reader >> v8;  // 11
      serf.s.ready_to_leave_inventory.mode = (int8_t)v8;
      reader >> v16;  // 12
      serf.s.ready_to_leave_inventory.dest = v16;
      reader >> v16;  // 14
      serf.s.ready_to_leave_inventory.inv_index = v16;
      break;

    case Serf::StateFreeWalking:
    case Serf::StateLogging:
    case Serf::StatePlanting:
    case Serf::StateStoneCutting:
    case Serf::StateStoneCutterFreeWalking:
    case Serf::StateFishing:
    case Serf::StateFarming:
    case Serf::StateSamplingGeoSpot:
    case Serf::StateKnightFreeWalking:
      reader >> v8;  // 11
      serf.s.free_walking.dist_col = (int8_t)v8;
      reader >> v8;  // 12
      serf.s.free_walking.dist_row = (int8_t)v8;
      reader >> v8;  // 13
      serf.s.free_walking.neg_dist1 = (int8_t)v8;
      reader >> v8;  // 14
      serf.s.free_walking.neg_dist2 = (int8_t)v8;
      reader >> v8;  // 15
      serf.s.free_walking.flags = v8;
      break;

    case Serf::StateSawing:
      reader >> v8;  // 11
      serf.s.sawing.mode = v8;
      break;

    case Serf::StateLost:
      reader >> v8;  // 11
      serf.s.lost.field_B = v8;
      break;

    case Serf::StateMining:
      reader >> v8;  // 11
      serf.s.mining.substate = v8;
      reader >> v8;  // 12
      reader >> v8;  // 13
      serf.s.mining.res = (int8_t)v8;
      reader >> v8;  // 14
      serf.s.mining.deposit = (Map::Minerals)v8;
      break;

    case Serf::StateSmelting:
      reader >> v8;  // 11
      serf.s.smelting.mode = v8;
      reader >> v8;  // 12
      serf.s.smelting.counter = v8;
      reader >> v8;  // 13
      serf.s.smelting.type = v8;
      break;

    case Serf::StateMilling:
      reader >> v8;  // 11
      serf.s.milling.mode = v8;
      break;

    case Serf::StateBaking:
      reader >> v8;  // 11
      serf.s.baking.mode = v8;
      break;

    case Serf::StatePigFarming:
      reader >> v8;  // 11
      serf.s.pigfarming.mode = v8;
      break;

    case Serf::StateButchering:
      reader >> v8;  // 11
      serf.s.butchering.mode = v8;
      break;

    case Serf::StateMakingWeapon:
      reader >> v8;  // 11
      serf.s.making_weapon.mode = v8;
      break;

    case Serf::StateMakingTool:
      reader >> v8;  // 11
      serf.s.making_tool.mode = v8;
      break;

    case Serf::StateBuildingBoat:
      reader >> v8;  // 11
      serf.s.building_boat.mode = v8;
      break;

    case Serf::StateKnightDefendingVictoryFree:
      /* TODO This will be tricky to load since the
       function of this state has been changed to one
       that is driven by the attacking serf instead
       (SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE). */
      break;

    case Serf::StateKnightLeaveForWalkToFight:
      reader >> v8;  // 11
      serf.s.leave_for_walk_to_fight.dist_col = (int8_t)v8;
      reader >> v8;  // 12
      serf.s.leave_for_walk_to_fight.dist_row = (int8_t)v8;
      reader >> v8;  // 13
      serf.s.leave_for_walk_to_fight.field_D = (int8_t)v8;
      reader >> v8;  // 14
      serf.s.leave_for_walk_to_fight.field_E = (int8_t)v8;
      reader >> v8;  // 15
      serf.s.leave_for_walk_to_fight.next_state = (Serf::State)v8;
      break;

    case Serf::StateIdleOnPath:
    case Serf::StateWaitIdleOnPath:
    case Serf::StateWakeAtFlag:
    case Serf::StateWakeOnPath: {
      reader >> v8;  // 11
      serf.s.idle_on_path.rev_dir = (Direction)v8;
      reader >> v16;  // 12
      unsigned int index = v16/70;
      serf.s.idle_on_path.flag = index;
      reader >> v8;  // 14
      serf.s.idle_on_path.field_E = v8;
      break;
    }

    case Serf::StateDefendingHut:
    case Serf::StateDefendingTower:
    case Serf::StateDefendingFortress:
    case Serf::StateDefendingCastle:
      reader >> v16;  // 11
      reader >> v8;  // 13
      reader >> v16;  // 14
      serf.s.defending.next_knight = v16;
      break;

    case Serf::StateKnightEngagingBuilding:
    case Serf::StateKnightPrepareAttacking:
    case Serf::StateKnightPrepareDefendingFreeWait:
    case Serf::StateKnightAttackingDefeatFree:
    case Serf::StateKnightAttacking:
    case Serf::StateKnightAttackingVictory:
    case Serf::StateKnightEngageAttackingFree:
    case Serf::StateKnightEngageAttackingFreeJoin:
      reader >> v8;  // 11
      serf.s.attacking.move = v8;
      reader >> v8;  // 12
      serf.s.attacking.attacker_won = v8;
      reader >> v8;  // 13
      serf.s.attacking.field_D = v8;
      reader >> v16;  // 14
      serf.s.attacking.def_index = v16;
      break;

    default: break;
  }

  return reader;
}

SaveReaderText&
operator >> (SaveReaderText &reader, Serf &serf) {
  int type;
  reader.value("type") >> type;
  try {
    reader.value("owner") >> serf.owner;
    serf.type = (Serf::Type)type;
  } catch(...) {
    serf.type = (Serf::Type)((type >> 2) & 0x1f);
    serf.owner = type & 3;
  }

  reader.value("animation") >> serf.animation;
  reader.value("counter") >> serf.counter;
  int x, y;
  reader.value("pos")[0] >> x;
  reader.value("pos")[1] >> y;
  serf.pos = serf.get_game()->get_map()->pos(x, y);
  reader.value("tick") >> serf.tick;
  reader.value("state") >> serf.state;

  switch (serf.state) {
    case Serf::StateIdleInStock:
      reader.value("state.inventory") >> serf.s.idle_in_stock.inv_index;
      break;

    case Serf::StateWalking:
      reader.value("state.dest") >> serf.s.walking.dest;
      reader.value("state.dir") >> serf.s.walking.dir;
      reader.value("state.wait_counter") >> serf.s.walking.wait_counter;
      reader.value("state.other_dir") >> serf.s.walking.dir1;
      break;

    case Serf::StateTransporting:
    case Serf::StateDelivering:
      reader.value("state.res") >> serf.s.transporting.res;
      reader.value("state.dest") >> serf.s.transporting.dest;
      reader.value("state.dir") >> serf.s.transporting.dir;
      reader.value("state.wait_counter") >> serf.s.transporting.wait_counter;
      break;

    case Serf::StateEnteringBuilding:
      reader.value("state.field_B") >> serf.s.entering_building.field_B;
      reader.value("state.slope_len") >> serf.s.entering_building.slope_len;
      break;

    case Serf::StateLeavingBuilding:
    case Serf::StateReadyToLeave:
    case Serf::StateKnightLeaveForFight:
      reader.value("state.field_B") >> serf.s.leaving_building.field_B;
      reader.value("state.dest") >> serf.s.leaving_building.dest;
      reader.value("state.dest2") >> serf.s.leaving_building.dest2;
      reader.value("state.dir") >> serf.s.leaving_building.dir;
      reader.value("state.next_state") >> serf.s.leaving_building.next_state;
      break;

    case Serf::StateReadyToEnter:
      reader.value("state.field_B") >> serf.s.ready_to_enter.field_B;
      break;

    case Serf::StateDigging:
      reader.value("state.h_index") >> serf.s.digging.h_index;
      reader.value("state.target_h") >> serf.s.digging.target_h;
      reader.value("state.dig_pos") >> serf.s.digging.dig_pos;
      reader.value("state.substate") >> serf.s.digging.substate;
      break;

    case Serf::StateBuilding:
      reader.value("state.mode") >> serf.s.building.mode;
      reader.value("state.bld_index") >> serf.s.building.bld_index;
      reader.value("state.material_step") >> serf.s.building.material_step;
      reader.value("state.counter") >> serf.s.building.counter;
      break;

    case Serf::StateBuildingCastle:
      reader.value("state.inv_index") >> serf.s.building_castle.inv_index;
      break;

    case Serf::StateMoveResourceOut:
    case Serf::StateDropResourceOut:
      reader.value("state.res") >> serf.s.move_resource_out.res;
      reader.value("state.res_dest") >> serf.s.move_resource_out.res_dest;
      reader.value("state.next_state") >> serf.s.move_resource_out.next_state;
      break;

    case Serf::StateReadyToLeaveInventory:
      reader.value("state.mode") >> serf.s.ready_to_leave_inventory.mode;
      reader.value("state.dest") >> serf.s.ready_to_leave_inventory.dest;
      reader.value("state.inv_index") >>
        serf.s.ready_to_leave_inventory.inv_index;
      break;

    case Serf::StateFreeWalking:
    case Serf::StateLogging:
    case Serf::StatePlanting:
    case Serf::StateStoneCutting:
    case Serf::StateStoneCutterFreeWalking:
    case Serf::StateFishing:
    case Serf::StateFarming:
    case Serf::StateSamplingGeoSpot:
    case Serf::StateKnightFreeWalking:
    case Serf::StateKnightAttackingFree:
    case Serf::StateKnightAttackingFreeWait:
      if (reader.has_value("state.dist_col")) {
        reader.value("state.dist_col") >> serf.s.free_walking.dist_col;
      } else {
        reader.value("state.dist1") >> serf.s.free_walking.dist_col;
      }
      if (reader.has_value("state.dist_row")) {
        reader.value("state.dist_row") >> serf.s.free_walking.dist_row;
      } else {
        reader.value("state.dist2") >> serf.s.free_walking.dist_row;
      }
      reader.value("state.neg_dist") >> serf.s.free_walking.neg_dist1;
      reader.value("state.neg_dist2") >> serf.s.free_walking.neg_dist2;
      reader.value("state.flags") >> serf.s.free_walking.flags;
      break;

    case Serf::StateSawing:
      reader.value("state.mode") >> serf.s.sawing.mode;
      break;

    case Serf::StateLost:
      reader.value("state.field_B") >> serf.s.lost.field_B;
      break;

    case Serf::StateMining:
      reader.value("state.substate") >> serf.s.mining.substate;
      reader.value("state.res") >> serf.s.mining.res;
      unsigned int deposit;
      reader.value("state.deposit") >> deposit;
      serf.s.mining.deposit = (Map::Minerals)deposit;
      break;

    case Serf::StateSmelting:
      reader.value("state.mode") >> serf.s.smelting.mode;
      reader.value("state.counter") >> serf.s.smelting.counter;
      reader.value("state.type") >> serf.s.smelting.type;
      break;

    case Serf::StateMilling:
      reader.value("state.mode") >> serf.s.milling.mode;
      break;

    case Serf::StateBaking:
      reader.value("state.mode") >> serf.s.baking.mode;
      break;

    case Serf::StatePigFarming:
      reader.value("state.mode") >> serf.s.pigfarming.mode;
      break;

    case Serf::StateButchering:
      reader.value("state.mode") >> serf.s.butchering.mode;
      break;

    case Serf::StateMakingWeapon:
      reader.value("state.mode") >> serf.s.making_weapon.mode;
      break;

    case Serf::StateMakingTool:
      reader.value("state.mode") >> serf.s.making_tool.mode;
      break;

    case Serf::StateBuildingBoat:
      reader.value("state.mode") >> serf.s.building_boat.mode;
      break;

    case Serf::StateKnightEngagingBuilding:
    case Serf::StateKnightPrepareAttacking:
    case Serf::StateKnightPrepareDefendingFreeWait:
    case Serf::StateKnightAttackingDefeatFree:
    case Serf::StateKnightAttacking:
    case Serf::StateKnightAttackingVictory:
    case Serf::StateKnightEngageAttackingFree:
    case Serf::StateKnightEngageAttackingFreeJoin:
      if (reader.has_value("state.move")) {
        reader.value("state.move") >> serf.s.attacking.move;
      } else {
        reader.value("state.field_B") >> serf.s.attacking.move;
      }
      if (reader.has_value("state.attacker_won")) {
        reader.value("state.attacker_won") >> serf.s.attacking.attacker_won;
      } else {
        reader.value("state.field_C") >> serf.s.attacking.attacker_won;
      }
      reader.value("state.field_D") >> serf.s.attacking.field_D;
      reader.value("state.def_index") >> serf.s.attacking.def_index;
      break;

    case Serf::StateKnightAttackingVictoryFree:
      if (reader.has_value("state.move")) {
        reader.value("state.move") >> serf.s.attacking_victory_free.move;
      } else {
        reader.value("state.field_B") >> serf.s.attacking_victory_free.move;
      }
      if (reader.has_value("state.dist_col")) {
        reader.value("state.dist_col") >>
                                         serf.s.attacking_victory_free.dist_col;
      } else {
        reader.value("state.field_C") >> serf.s.attacking_victory_free.dist_col;
      }
      if (reader.has_value("state.dist_row")) {
        reader.value("state.dist_row") >>
                                         serf.s.attacking_victory_free.dist_row;
      } else {
        reader.value("state.field_D") >> serf.s.attacking_victory_free.dist_row;
      }
      reader.value("state.def_index") >>
                                        serf.s.attacking_victory_free.def_index;
      break;

    case Serf::StateKnightDefendingFree:
    case Serf::StateKnightEngageDefendingFree:
      reader.value("state.dist_col") >> serf.s.defending_free.dist_col;
      reader.value("state.dist_row") >> serf.s.defending_free.dist_row;
      reader.value("state.field_D") >> serf.s.defending_free.field_D;
      reader.value("state.other_dist_col") >>
        serf.s.defending_free.other_dist_col;
      reader.value("state.other_dist_row") >>
        serf.s.defending_free.other_dist_row;
      break;

    case Serf::StateKnightLeaveForWalkToFight:
      reader.value("state.dist_col") >> serf.s.leave_for_walk_to_fight.dist_col;
      reader.value("state.dist_row") >> serf.s.leave_for_walk_to_fight.dist_row;
      reader.value("state.field_D") >> serf.s.leave_for_walk_to_fight.field_D;
      reader.value("state.field_E") >> serf.s.leave_for_walk_to_fight.field_E;
      reader.value("state.next_state") >>
        serf.s.leave_for_walk_to_fight.next_state;
      break;

    case Serf::StateIdleOnPath:
    case Serf::StateWaitIdleOnPath:
    case Serf::StateWakeAtFlag:
    case Serf::StateWakeOnPath:
      reader.value("state.rev_dir") >> serf.s.idle_on_path.rev_dir;
      unsigned int flag_idex;
      reader.value("state.flag") >> flag_idex;
      serf.s.idle_on_path.flag = flag_idex;
      reader.value("state.field_E") >> serf.s.idle_on_path.field_E;
      break;

    case Serf::StateDefendingHut:
    case Serf::StateDefendingTower:
    case Serf::StateDefendingFortress:
    case Serf::StateDefendingCastle:
      reader.value("state.next_knight") >> serf.s.defending.next_knight;
      break;

    default:
      break;
  }

  return reader;
}

SaveWriterText&
operator << (SaveWriterText &writer, Serf &serf) {
  writer.value("type") << serf.type;
  writer.value("owner") << serf.owner;
  writer.value("animation") << serf.animation;
  writer.value("counter") << serf.counter;
  writer.value("pos") << serf.get_game()->get_map()->pos_col(serf.pos);
  writer.value("pos") << serf.get_game()->get_map()->pos_row(serf.pos);
  writer.value("tick") << serf.tick;
  writer.value("state") << serf.state;

  switch (serf.state) {
    case Serf::StateIdleInStock:
      writer.value("state.inventory") << serf.s.idle_in_stock.inv_index;
      break;

    case Serf::StateWalking:
      writer.value("state.dest") << serf.s.walking.dest;
      writer.value("state.dir") << serf.s.walking.dir;
      writer.value("state.wait_counter") << serf.s.walking.wait_counter;
      writer.value("state.other_dir") << serf.s.walking.dir1;
      break;

    case Serf::StateTransporting:
    case Serf::StateDelivering:
      writer.value("state.res") << serf.s.transporting.res;
      writer.value("state.dest") << serf.s.transporting.dest;
      writer.value("state.dir") << serf.s.transporting.dir;
      writer.value("state.wait_counter") << serf.s.transporting.wait_counter;
      break;

    case Serf::StateEnteringBuilding:
      writer.value("state.field_B") << serf.s.entering_building.field_B;
      writer.value("state.slope_len") << serf.s.entering_building.slope_len;
      break;

    case Serf::StateLeavingBuilding:
    case Serf::StateReadyToLeave:
    case Serf::StateKnightLeaveForFight:
      writer.value("state.field_B") << serf.s.leaving_building.field_B;
      writer.value("state.dest") << serf.s.leaving_building.dest;
      writer.value("state.dest2") << serf.s.leaving_building.dest2;
      writer.value("state.dir") << serf.s.leaving_building.dir;
      writer.value("state.next_state") << serf.s.leaving_building.next_state;
      break;

    case Serf::StateReadyToEnter:
      writer.value("state.field_B") << serf.s.ready_to_enter.field_B;
      break;

    case Serf::StateDigging:
      writer.value("state.h_index") << serf.s.digging.h_index;
      writer.value("state.target_h") << serf.s.digging.target_h;
      writer.value("state.dig_pos") << serf.s.digging.dig_pos;
      writer.value("state.substate") << serf.s.digging.substate;
      break;

    case Serf::StateBuilding:
      writer.value("state.mode") << serf.s.building.mode;
      writer.value("state.bld_index") << serf.s.building.bld_index;
      writer.value("state.material_step") << serf.s.building.material_step;
      writer.value("state.counter") << serf.s.building.counter;
      break;

    case Serf::StateBuildingCastle:
      writer.value("state.inv_index") << serf.s.building_castle.inv_index;
      break;

    case Serf::StateMoveResourceOut:
    case Serf::StateDropResourceOut:
      writer.value("state.res") << serf.s.move_resource_out.res;
      writer.value("state.res_dest") << serf.s.move_resource_out.res_dest;
      writer.value("state.next_state") << serf.s.move_resource_out.next_state;
      break;

    case Serf::StateReadyToLeaveInventory:
      writer.value("state.mode") << serf.s.ready_to_leave_inventory.mode;
      writer.value("state.dest") << serf.s.ready_to_leave_inventory.dest;
      writer.value("state.inv_index") <<
        serf.s.ready_to_leave_inventory.inv_index;
      break;

    case Serf::StateFreeWalking:
    case Serf::StateLogging:
    case Serf::StatePlanting:
    case Serf::StateStoneCutting:
    case Serf::StateStoneCutterFreeWalking:
    case Serf::StateFishing:
    case Serf::StateFarming:
    case Serf::StateSamplingGeoSpot:
    case Serf::StateKnightFreeWalking:
    case Serf::StateKnightAttackingFree:
    case Serf::StateKnightAttackingFreeWait:
      writer.value("state.dist_col") << serf.s.free_walking.dist_col;
      writer.value("state.dist_row") << serf.s.free_walking.dist_row;
      writer.value("state.neg_dist") << serf.s.free_walking.neg_dist1;
      writer.value("state.neg_dist2") << serf.s.free_walking.neg_dist2;
      writer.value("state.flags") << serf.s.free_walking.flags;
      break;

    case Serf::StateSawing:
      writer.value("state.mode") << serf.s.sawing.mode;
      break;

    case Serf::StateLost:
      writer.value("state.field_B") << serf.s.lost.field_B;
      break;

    case Serf::StateMining:
      writer.value("state.substate") << serf.s.mining.substate;
      writer.value("state.res") << serf.s.mining.res;
      writer.value("state.deposit") << serf.s.mining.deposit;
      break;

    case Serf::StateSmelting:
      writer.value("state.mode") << serf.s.smelting.mode;
      writer.value("state.counter") << serf.s.smelting.counter;
      writer.value("state.type") << serf.s.smelting.type;
      break;

    case Serf::StateMilling:
      writer.value("state.mode") << serf.s.milling.mode;
      break;

    case Serf::StateBaking:
      writer.value("state.mode") << serf.s.baking.mode;
      break;

    case Serf::StatePigFarming:
      writer.value("state.mode") << serf.s.pigfarming.mode;
      break;

    case Serf::StateButchering:
      writer.value("state.mode") << serf.s.butchering.mode;
      break;

    case Serf::StateMakingWeapon:
      writer.value("state.mode") << serf.s.making_weapon.mode;
      break;

    case Serf::StateMakingTool:
      writer.value("state.mode") << serf.s.making_tool.mode;
      break;

    case Serf::StateBuildingBoat:
      writer.value("state.mode") << serf.s.building_boat.mode;
      break;

    case Serf::StateKnightEngagingBuilding:
    case Serf::StateKnightPrepareAttacking:
    case Serf::StateKnightPrepareDefendingFreeWait:
    case Serf::StateKnightAttackingDefeatFree:
    case Serf::StateKnightAttacking:
    case Serf::StateKnightAttackingVictory:
    case Serf::StateKnightEngageAttackingFree:
    case Serf::StateKnightEngageAttackingFreeJoin:
    case Serf::StateKnightAttackingVictoryFree:
      writer.value("state.move") << serf.s.attacking.move;
      writer.value("state.attacker_won") << serf.s.attacking.attacker_won;
      writer.value("state.field_D") << serf.s.attacking.field_D;
      writer.value("state.def_index") << serf.s.attacking.def_index;
      break;

    case Serf::StateKnightDefendingFree:
    case Serf::StateKnightEngageDefendingFree:
      writer.value("state.dist_col") << serf.s.defending_free.dist_col;
      writer.value("state.dist_row") << serf.s.defending_free.dist_row;
      writer.value("state.field_D") << serf.s.defending_free.field_D;
      writer.value("state.other_dist_col") <<
        serf.s.defending_free.other_dist_col;
      writer.value("state.other_dist_row") <<
        serf.s.defending_free.other_dist_row;
      break;

    case Serf::StateKnightLeaveForWalkToFight:
      writer.value("state.dist_col") << serf.s.leave_for_walk_to_fight.dist_col;
      writer.value("state.dist_row") << serf.s.leave_for_walk_to_fight.dist_row;
      writer.value("state.field_D") << serf.s.leave_for_walk_to_fight.field_D;
      writer.value("state.field_E") << serf.s.leave_for_walk_to_fight.field_E;
      writer.value("state.next_state") <<
        serf.s.leave_for_walk_to_fight.next_state;
      break;

    case Serf::StateIdleOnPath:
    case Serf::StateWaitIdleOnPath:
    case Serf::StateWakeAtFlag:
    case Serf::StateWakeOnPath:
      writer.value("state.rev_dir") << serf.s.idle_on_path.rev_dir;
      writer.value("state.flag") << serf.s.idle_on_path.flag;
      writer.value("state.field_E") << serf.s.idle_on_path.field_E;
      break;

    case Serf::StateDefendingHut:
    case Serf::StateDefendingTower:
    case Serf::StateDefendingFortress:
    case Serf::StateDefendingCastle:
      writer.value("state.next_knight") << serf.s.defending.next_knight;
      break;

    default: break;
  }

  return writer;
}

std::string
Serf::print_state() {
  std::stringstream res;

  res << get_state_name(state) << "\n";

  switch (state) {
    case Serf::StateIdleInStock:
      res << "inventory" << "\t" << s.idle_in_stock.inv_index << "\n";
      break;

    case Serf::StateWalking:
      res << "dest" << "\t" << s.walking.dest << "\n";
      res << "dir" << "\t" << s.walking.dir << "\n";
      res << "wait_counter" << "\t" << s.walking.wait_counter << "\n";
      res << "other_dir" << "\t" << s.walking.dir1 << "\n";
      break;

    case Serf::StateTransporting:
    case Serf::StateDelivering:
      res << "res" << "\t" << s.transporting.res << "\n";
      res << "dest" << "\t" << s.transporting.dest << "\n";
      res << "dir" << "\t" << s.transporting.dir << "\n";
      res << "wait_counter" << "\t" << s.transporting.wait_counter << "\n";
      break;

    case Serf::StateEnteringBuilding:
      res << "field_B" << "\t" << s.entering_building.field_B << "\n";
      res << "slope_len" << "\t" << s.entering_building.slope_len << "\n";
      break;

    case Serf::StateLeavingBuilding:
    case Serf::StateReadyToLeave:
    case Serf::StateKnightLeaveForFight:
      res << "field_B" << "\t" << s.leaving_building.field_B << "\n";
      res << "dest" << "\t" << s.leaving_building.dest << "\n";
      res << "dest2" << "\t" << s.leaving_building.dest2 << "\n";
      res << "dir" << "\t" << s.leaving_building.dir << "\n";
      res << "next_state" << "\t" << s.leaving_building.next_state << "\n";
      break;

    case Serf::StateReadyToEnter:
      res << "field_B" << "\t" << s.ready_to_enter.field_B << "\n";
      break;

    case Serf::StateDigging:
      res << "h_index" << "\t" << s.digging.h_index << "\n";
      res << "target_h" << "\t" << s.digging.target_h << "\n";
      res << "dig_pos" << "\t" << s.digging.dig_pos << "\n";
      res << "substate" << "\t" << s.digging.substate << "\n";
      break;

    case Serf::StateBuilding:
      res << "mode" << "\t" << s.building.mode << "\n";
      res << "bld_index" << "\t" << s.building.bld_index << "\n";
      res << "material_step" << "\t" << s.building.material_step << "\n";
      res << "counter" << "\t" << s.building.counter << "\n";
      break;

    case Serf::StateBuildingCastle:
      res << "inv_index" << "\t" << s.building_castle.inv_index << "\n";
      break;

    case Serf::StateMoveResourceOut:
    case Serf::StateDropResourceOut:
      res << "res" << "\t" << s.move_resource_out.res << "\n";
      res << "res_dest" << "\t" << s.move_resource_out.res_dest << "\n";
      res << "next_state" << "\t" << s.move_resource_out.next_state << "\n";
      break;

    case Serf::StateReadyToLeaveInventory:
      res << "mode" << "\t" << s.ready_to_leave_inventory.mode << "\n";
      res << "dest" << "\t" << s.ready_to_leave_inventory.dest << "\n";
      res << "inv_index" << "\t" << s.ready_to_leave_inventory.inv_index
          << "\n";
      break;

    case Serf::StateFreeWalking:
    case Serf::StateLogging:
    case Serf::StatePlanting:
    case Serf::StateStoneCutting:
    case Serf::StateStoneCutterFreeWalking:
    case Serf::StateFishing:
    case Serf::StateFarming:
    case Serf::StateSamplingGeoSpot:
    case Serf::StateKnightFreeWalking:
    case Serf::StateKnightAttackingFree:
    case Serf::StateKnightAttackingFreeWait:
      res << "dist_col" << "\t" << s.free_walking.dist_col << "\n";
      res << "dist_row" << "\t" << s.free_walking.dist_row << "\n";
      res << "neg_dist" << "\t" << s.free_walking.neg_dist1 << "\n";
      res << "neg_dist2" << "\t" << s.free_walking.neg_dist2 << "\n";
      res << "flags" << "\t" << s.free_walking.flags << "\n";
      // help determine direction
      res << "animation" << "\t" << animation << "\n";
      break;

    case Serf::StateSawing:
      res << "mode" << "\t" << s.sawing.mode << "\n";
      break;

    case Serf::StateLost:
      res << "field_B" << "\t" << s.lost.field_B << "\n";
      break;

    case Serf::StateMining:
      res << "substate" << "\t" << s.mining.substate << "\n";
      res << "res" << "\t" << s.mining.res << "\n";
      res << "deposit" << "\t" << s.mining.deposit << "\n";
      break;

    case Serf::StateSmelting:
      res << "mode" << "\t" << s.smelting.mode << "\n";
      res << "counter" << "\t" << s.smelting.counter << "\n";
      res << "type" << "\t" << s.smelting.type << "\n";
      break;

    case Serf::StateMilling:
      res << "mode" << "\t" << s.milling.mode << "\n";
      break;

    case Serf::StateBaking:
      res << "mode" << "\t" << s.baking.mode << "\n";
      break;

    case Serf::StatePigFarming:
      res << "mode" << "\t" << s.pigfarming.mode << "\n";
      break;

    case Serf::StateButchering:
      res << "mode" << "\t" << s.butchering.mode << "\n";
      break;

    case Serf::StateMakingWeapon:
      res << "mode" << "\t" << s.making_weapon.mode << "\n";
      break;

    case Serf::StateMakingTool:
      res << "mode" << "\t" << s.making_tool.mode << "\n";
      break;

    case Serf::StateBuildingBoat:
      res << "mode" << "\t" << s.building_boat.mode << "\n";
      break;

    case Serf::StateKnightEngagingBuilding:
    case Serf::StateKnightPrepareAttacking:
    case Serf::StateKnightPrepareDefendingFreeWait:
    case Serf::StateKnightAttackingDefeatFree:
    case Serf::StateKnightAttacking:
    case Serf::StateKnightAttackingVictory:
    case Serf::StateKnightEngageAttackingFree:
    case Serf::StateKnightEngageAttackingFreeJoin:
    case Serf::StateKnightAttackingVictoryFree:
      res << "move" << "\t" << s.attacking.move << "\n";
      res << "attacker_won" << "\t" << s.attacking.attacker_won << "\n";
      res << "field_D" << "\t" << s.attacking.field_D << "\n";
      res << "def_index" << "\t" << s.attacking.def_index << "\n";
      break;

    case Serf::StateKnightDefendingFree:
    case Serf::StateKnightEngageDefendingFree:
      res << "dist_col" << "\t" << s.defending_free.dist_col << "\n";
      res << "dist_row" << "\t" << s.defending_free.dist_row << "\n";
      res << "field_D" << "\t" << s.defending_free.field_D << "\n";
      res << "other_dist_col" << "\t" << s.defending_free.other_dist_col
          << "\n";
      res << "other_dist_row" << "\t" << s.defending_free.other_dist_row
          << "\n";
      break;

    case Serf::StateKnightLeaveForWalkToFight:
      res << "dist_col" << "\t" << s.leave_for_walk_to_fight.dist_col << "\n";
      res << "dist_row" << "\t" << s.leave_for_walk_to_fight.dist_row << "\n";
      res << "field_D" << "\t" << s.leave_for_walk_to_fight.field_D << "\n";
      res << "field_E" << "\t" << s.leave_for_walk_to_fight.field_E << "\n";
      res << "next_state" << "\t" << s.leave_for_walk_to_fight.next_state
          << "\n";
      break;

    case Serf::StateIdleOnPath:
    case Serf::StateWaitIdleOnPath:
    case Serf::StateWakeAtFlag:
    case Serf::StateWakeOnPath:
      res << "rev_dir" << "\t" << s.idle_on_path.rev_dir << "\n";
      res << "flag" << "\t" << s.idle_on_path.flag << "\n";
      res << "field_E" << "\t" << s.idle_on_path.field_E << "\n";
      break;

    case Serf::StateDefendingHut:
    case Serf::StateDefendingTower:
    case Serf::StateDefendingFortress:
    case Serf::StateDefendingCastle:
      res << "next_knight" << "\t" << s.defending.next_knight << "\n";
      break;

    default: break;
  }
  return res.str();
}
