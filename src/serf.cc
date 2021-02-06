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

  /* Register this type as transporter */
  if (new_type == TypeTransporterInventory) new_type = TypeTransporter;
  if (old_type == TypeTransporterInventory) old_type = TypeTransporter;

  Player *player = game->get_player(get_owner());
  if (old_type != Serf::TypeNone && old_type != Serf::TypeDead) {
    player->decrease_serf_count(old_type);
  }
  if (type != TypeDead) {
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
//  and I need this for AIPlusOption::CanTransportSerfsInBoats
//   and probably other things eventually
void
Serf::set_serf_state(Serf::State new_state){
  //Log::Info["serf"] << "inside Serf::set_serf_state to state " << new_state << " for serf that has current state " << get_state();
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
  if (state == StateWalking) {
    if (s.walking.dest == flag_1 && s.walking.dir1 == dir_1) {
      select = 0;
      return true;
    } else if (s.walking.dest == flag_2 && s.walking.dir1 == dir_2) {
      *select = 1;
      return true;
    }
  } else if (state == StateReadyToLeaveInventory) {
    if (s.ready_to_leave_inventory.dest == flag_1 &&
        s.ready_to_leave_inventory.mode == dir_1) {
      select = 0;
      return true;
    } else if (s.ready_to_leave_inventory.dest == flag_2 &&
               s.ready_to_leave_inventory.mode == dir_2) {
      *select = 1;
      return true;
    }
  } else if ((state == StateReadyToLeave || state == StateLeavingBuilding) &&
             s.leaving_building.next_state == StateWalking) {
    if (s.leaving_building.dest == flag_1 &&
        s.leaving_building.field_B == dir_1) {
      select = 0;
      return true;
    } else if (s.leaving_building.dest == flag_2 &&
               s.leaving_building.field_B == dir_2) {
      *select = 1;
      return true;
    }
  }

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
        set_state(StateLost);
      }
      break;
    default:
      break;
  }
}

bool
Serf::building_deleted(MapPos building_pos, bool escape) {
  if (pos == building_pos &&
      (state == StateIdleInStock || state == StateReadyToLeaveInventory)) {
    if (escape) {
      /* Serf is escaping. */
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

void
Serf::castle_deleted(MapPos castle_pos, bool transporter) {
  if ((!transporter || (get_type() == TypeTransporterInventory)) &&
      pos == castle_pos) {
    if (transporter) {
      set_type(TypeTransporter);
    }
  }

  counter = 0;

  if (game->get_map()->get_serf_index(pos) == index) {
    set_state(StateLost);
    s.lost.field_B = 0;
  } else {
    set_state(StateEscapeBuilding);
  }
}


bool
Serf::change_transporter_state_at_pos(MapPos pos_, Serf::State _state) {
  if (pos == pos_ &&
      (_state == StateWakeAtFlag || _state == StateWakeOnPath ||
       _state == StateWaitIdleOnPath || _state == StateIdleOnPath)) {
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

void
Serf::insert_before(Serf *knight) {
  s.defending.next_knight = knight->get_index();
}

void
Serf::go_out_from_inventory(unsigned int inventory, MapPos dest, int mode) {
  set_state(StateReadyToLeaveInventory);
  s.ready_to_leave_inventory.mode = mode;
  s.ready_to_leave_inventory.dest = dest;
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
      set_state(StateLost);
      s.lost.field_B = 0;
    } else {
      set_state(StateLostSailor);
    }
  } else {
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
      case TypeSmelter: /* TODO ??? */
        break;
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
 //Log::Info["serf"] << "debug: a serf inside change_direction, starting pos " << pos << ", dir: " << NameDirection[dir];
  PMap map = game->get_map();
  MapPos new_pos = map->move(pos, dir);

  //
  // adding support for AIPlusOption::CanTransportSerfsInBoats
  //

  // BUG - I am seeing serfs walking on water paths as of jan14 2021
  //   this is happening when CanTransportSerfsInBoats is *not* true!
  // hmm... I cannot reproduce this now using a human player  jan14 2021
  //  I could reproduce, and fixed it by adding to ensure the AIPlusOption::CanTransportSerfsInBoats is set
  // however I still see some issues, such as a land serf "switching" positions with a sailor!

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
  /*  MOVED ELSEWHERE FOR IMMEDIATE DROPOFF, DELETE THIS SECTION ONCE VALIDATED  
  // handle sailor just dropped a passenger off at a flag *and sailor is now one pos away*
  //  now that the sailor is free of the flag pos, the serf must be placed there and the sailor dropped_serf_ vars cleared
  //  the !map->has_flag(pos) here check prevents this from triggering the second the passenger is dropped
  if (type == Serf::TypeSailor && state == State::StateTransporting
    && s.transporting.dropped_serf_type > Type::TypeNone && s.transporting.dropped_serf_index > 0
    && !map->has_flag(pos)){
     //Log::Info["serf"] << "debug: a transporting sailor inside change_direction, just reached the next water-path pos after dropping a passenger off";
      
      // retrace back one tile to the flag where serf was dropped
      Direction tmpdir = DirectionNone;
      for (Direction d : cycle_directions_cw()) {
        // look for the path in the dir that is not the current dir
        if (map->has_path(pos, d) && d != dir) {
          tmpdir = d;
          break;
        }
      }
      if (tmpdir == DirectionNone){
        throw ExceptionFreeserf("sailor dropped serf at at flag and is now one tile away could not retrace water path to find flag!");
      }

      MapPos dropped_pos = map->move(pos, tmpdir);
      if (map->has_serf(dropped_pos)){
        Serf *blocking_error_serf = game->get_serf_at_pos(dropped_pos);
        Log::Error["serf"] << "sailor dropped serf at at flag and is now one tile away but flag pos is not empty! a serf with type " << NameSerf[blocking_error_serf->get_type()] << ", index " << blocking_error_serf->get_index() << ", state " << blocking_error_serf->get_state();
        throw ExceptionFreeserf("sailor dropped serf at at flag and is now one tile away but flag pos is not empty, so cannot make dropped serf appear there!");
      }
      if (!map->has_flag(dropped_pos)){
        throw ExceptionFreeserf("sailor dropped serf at at flag and is now one tile away map thinks there is no flag at the dropped pos!");
      }
      // place the serf
      // is s.transporting.xxxxx_serf_type even needed?  could just use serf->get_type() if the type is even ever needed?
      //  I think only viewport uses it but even that already looks all serf types up by *Serf pointer
      map->set_serf_index(dropped_pos, s.transporting.dropped_serf_index);
      Serf *dropped_serf = game->get_serf(s.transporting.dropped_serf_index);
      if (dropped_serf == nullptr){
        Log::Warn["serf"] << "sailor dropped a serf off at flag at pos " << dropped_pos << " but got nullptr for the serf!  serf index: " << s.transporting.dropped_serf_index << " serf type: " << s.transporting.dropped_serf_type;
      } else {
        //Log::Info["serf"] << "sailor awakened serf that was dropped off at pos " << dropped_pos;
        // is StateWalking a good default to use here?
        dropped_serf->set_serf_state(StateWalking);
        // serf->pos is not derived from map, it is also stored separately in Serf* object
        //  need to set it
        dropped_serf->pos = dropped_pos;
        // I dunno why these walking states all seem backwards, ignoring for now??
        //  set the dropped serf's dir to the reverse of the sailor's dir
        //   so it isn't facing back towards the water road
        // If this is not done, and the serf is still facing the water road, it will think it is waiting for a boat
        // wait, this must be done earlier, right as the serf is dropped off NOT as the sailor reaches one tile away
        //dropped_serf->s.walking.dir = reverse_direction((Direction)s.walking.dir);
        // update the dropped serf's stored tick to current game tick or its next animation will cut to its end
        dropped_serf->tick = game->get_tick();
      }

      // clear the hold on this flag position
      //  THIS IS NOT IMPLEMENTED YET!
      // it would be cleaner to simply set/clear the dropoff and pickup at flag bools rather than having tiny flag->functions that doe it
      Flag *dropped_flag = game->get_flag_at_pos(dropped_pos);
      dropped_flag->drop_off_serf();

      // clear the sailor dropped_serf vars
      s.transporting.dropped_serf_type = Type::TypeNone;
      s.transporting.dropped_serf_index = 0;

      // the dropped off passenger serf should now fully exist on the map at the flag at 
      //  the end of the water path and resume going to his original destination
  }
      */


  // if *this* serf is a sailor (who is already in transporting state and so must be in his boat)...
  //  and if another serf is at the next pos in StateWaitForBoat AND is waiting in the direction of this serf
  //   continue "into" the waiting serf to pick him up
  //
  bool sailor_pickup_serf = false;
  /*
    // debugging
  if (type == Serf::TypeSailor && state == State::StateTransporting){
   //Log::Info["serf"] << "debug: a transporting sailor inside change_direction, heading in dir " << NameDirection[dir];
    if (game->get_serf_at_pos(new_pos)->get_state() == Serf::StateWaitForBoat){
     //Log::Info["serf"] << "debug: a transporting sailor inside change_direction, there is a serf in wait_for_boat state in next pos with dir " << NameDirection[game->get_serf_at_pos(new_pos)->get_walking_dir()];
     //Log::Info["serf"] << "debug: a transporting sailor inside change_direction, comparing sailor reversed-dir " << NameDirection[reverse_direction(Direction(dir))] << " to boat-wait serf dir " << NameDirection[game->get_serf_at_pos(new_pos)->get_walking_dir()];
    }
  }
  */

  if (type == Serf::TypeSailor && state == State::StateTransporting
     && game->get_serf_at_pos(new_pos)->get_state() == Serf::StateWaitForBoat
     && game->get_serf_at_pos(new_pos)->get_walking_dir() == reverse_direction(Direction(dir))){
       //Log::Info["serf"] << "debug: a transporting sailor inside change_direction, next pos " << new_pos << ", setting sailor_pickup_serf to TRUE";
       sailor_pickup_serf = true;
  }

  // handle serf entering WaitForBoat state because it's change_direction dir is into a waiting sailor's water path
  // if next pos is in water and has no blocking serf, and current pos is NOT in water... (or else it triggers for active sailors)
  // WARNING - is_water_path can only be uses to check a path that is certain to exist, it will return true if there is no path at all!
  if (game->get_ai_options_ptr()->test(AIPlusOption::CanTransportSerfsInBoats) &&
      map->is_in_water(new_pos) && !map->has_serf(new_pos) && !map->is_in_water(pos)){
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
      // I think this may be fixed now
     //Log::Info["serf"] << "debug: inside Serf::change_direction, water_flag->has_transporter(" << NameDirection[dir] << ") == " << water_flag->has_transporter(dir);
     //Log::Info["serf"] << "debug: inside Serf::change_direction, game->get_serf_at_pos(" << pos << ")->get_walking_dir() = " << NameDirection[game->get_serf_at_pos(pos)->get_walking_dir()] << ", compare to reverse-dir " << NameDirection[reverse_direction(dir)];
      // ugh, because serf->get_walking_dir seems to always be backwards, we should actually reject the pickup
      //  if get_walking_dir == dir, which is equivalent to "corrected" reverse_dir[get_walking_dir] != reverse_dir(dir)
      // trying workaround for above issue...
      //if (water_flag->has_transporter(dir) && game->get_serf_at_pos(pos)->get_walking_dir() != dir){
      //Log::Info["serf"] << "debugX: THIS serf_type " << get_type() << ", s.walking.dir " << s.walking.dir << ", get_walking_dir " << get_walking_dir();
      //Log::Info["serf"] << "debugY: game->get_serf_at_pos serf_type " << game->get_serf_at_pos(pos)->get_type() << ", s.walking.dir " << game->get_serf_at_pos(pos)->s.walking.dir << ", get_walking_dir " << game->get_serf_at_pos(pos)->get_walking_dir();
      if (water_flag->has_transporter(dir) && s.walking.dir != dir){
       //Log::Info["serf"] << "debug: inside Serf::change_direction, setting passenger-to-be into WaitForBoat state";
        set_state(Serf::StateWaitForBoat);
       //Log::Info["serf"] << "debug: inside Serf::change_direction, setting flag at pos " << pos << " to has_serf_waiting_for_boat";
        water_flag->set_serf_waiting_for_boat();
        // still need to actually set the serf walking direction to the dir of the water path/boat it is waiting for
        s.walking.dir = dir;
        return;
      }
      if (get_type() == Serf::TypeSailor){
        // this sailor is probably the new transporter for this water path
        //  let him go on, and do not put him in WaitForBoat state
      }else{
        Log::Error["serf"] << "serf of type " << NameSerf[get_type()] << " at pos " << pos << " with state " << get_state() << " expecting flag with water path to have sailor! but dose not, crashing";
        throw ExceptionFreeserf("expecting flag with water path to have a (sailor) transporter!  to direct serf to WaitForBoat state");
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
    }

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
      if (map->has_path(pos, dir) && map->is_in_water(new_pos)){
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
      Log::Debug["serf"] << "unhandled jump to 31B82.";
    }
  }
}

/* Precondition: serf state is in WALKING or TRANSPORTING state */
void
Serf::transporter_move_to_flag(Flag *flag) {
  // is s.transporting.dir the direction that the transporter is travelling?
  //  or, is it the direction that the res at the reached flag is heading (i.e. the reverse dir)?
  //   it seems to be the latter, but that could be faulty logic in sailor transport serf code
  Direction dir = (Direction)s.transporting.dir;
  if (get_type() == Serf::TypeSailor){
   //Log::Info["serf"] << "debug: sailor inside Serf::transporter_move_to_flag, dir: " << NameDirection[dir];

   //Log::Info["serf"] << "debug: sailor inside Serf::transporter_move_to_flag A, this serf's type == " << NameSerf[type];
   //Log::Info["serf"] << "debug: sailor inside Serf::transporter_move_to_flag A, flag->is_water_path " << NameDirection[dir] << " = " << flag->is_water_path(dir);
   //Log::Info["serf"] << "debug: sailor inside Serf::transporter_move_to_flag A, flag->has_serf_waiting_for_boat() == " << flag->has_serf_waiting_for_boat();
    
    // adding support for AIPlusOption::CanTransportSerfsInBoats
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
        if (flag->drop_resource(s.transporting.res, s.transporting.dest)) {
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
    //if (get_type() == Serf::TypeSailor){
     //Log::Info["serf"] << "debug: sailor inside Serf::transporter_move_to_flag, flag->is_scheduled in dir " << NameDirection[dir];
    //}
    /* Fetch resource from flag */
    s.transporting.wait_counter = 0;
    int res_index = flag->scheduled_slot(dir);

    if (s.transporting.res == Resource::TypeNone) {
      /* Pick up resource. */
      flag->pick_up_resource(res_index, &s.transporting.res,
                             &s.transporting.dest);
    } else {
      /* Switch resources and destination. */
      Resource::Type temp_res = s.transporting.res;
      int temp_dest = s.transporting.dest;

      flag->pick_up_resource(res_index, &s.transporting.res,
                             &s.transporting.dest);

      flag->drop_resource(temp_res, temp_dest);
    }

    /* Find next resource to be picked up */
    Player *player = game->get_player(get_owner());
    flag->prioritize_pickup((Direction)dir, player);
  } else if (s.transporting.res != Resource::TypeNone) {
    //if (get_type() == Serf::TypeSailor){
     //Log::Info["serf"] << "debug: inside Serf::transporter_move_to_flag C";
    //}
      /* Drop resource at flag */
      if (flag->drop_resource(s.transporting.res, s.transporting.dest)) {
        s.transporting.res = Resource::TypeNone;
      }
    //}
  }
  //if (get_type() == Serf::TypeSailor){
   //Log::Info["serf"] << "debug: inside Serf::transporter_move_to_flag D";
  //}

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
    Log::Verbose["serf"] << " dest found: " << dest->get_search_dir();
    serf->change_direction(dest->get_search_dir(), 0);
    return true;
  }
 //Log::Info["serf"] << "debug: inside Serf::handle_serf_walking_state_search_cb, dest NOT found";

  return false;
}

void
Serf::start_walking(Direction dir, int slope, int change_pos) {
  //Log::Info["serf"] << "debug: inside start_walking";
  PMap map = game->get_map();
  MapPos new_pos = map->move(pos, dir);
  //Log::Info["serf"] << "debug: inside start_walking, old pos: " << pos << ", new pos: " << new_pos;
  animation = get_walking_animation(map->get_height(new_pos) -
                                    map->get_height(pos), dir, 0);
  counter += (slope * counter_from_animation[animation]) >> 5;

  //Log::Info["serf"] << "debug: inside start_walking, animation = " << animation;
  if (change_pos) {
    //Log::Info["serf"] << "debug: inside start_walking, change_pos is TRUE, moving serf to new_pos " << new_pos;
    map->set_serf_index(pos, 0);
    map->set_serf_index(new_pos, get_index());
  }

  pos = new_pos;
}

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
void
Serf::enter_building(int field_B, int join_pos) {
  //Log::Info["serf"] << "debug: inside enter_building, setting state to StateEnteringBuilding";
  set_state(StateEnteringBuilding);
  //Log::Info["serf"] << "debug: inside enter_building, calling start_walking, join_pos = " << join_pos;
  start_walking(DirectionUpLeft, 32, !join_pos);
  //Log::Info["serf"] << "debug: inside enter_building, back from start_walking";
  if (join_pos) game->get_map()->set_serf_index(pos, get_index());

  Building *building = game->get_building_at_pos(pos);
  int slope = road_building_slope[building->get_type()];
  if (!building->is_done()) slope = 1;
  s.entering_building.slope_len = (slope * counter) >> 5;
  s.entering_building.field_B = field_B;
}

/* Start leaving building by switching to LEAVING BUILDING and
   setting appropriate state. */
void
Serf::leave_building(int join_pos) {
  Building *building = game->get_building_at_pos(pos);
  int slope = 31 - road_building_slope[building->get_type()];
  if (!building->is_done()) slope = 30;

  if (join_pos) game->get_map()->set_serf_index(pos, 0);
  start_walking(DirectionDownRight, slope, !join_pos);

  set_state(StateLeavingBuilding);
}

void
Serf::handle_serf_walking_state_dest_reached() {
  /* Destination reached. */
 //Log::Info["serf"] << "debug: inside handle_serf_walking_state_dest_reached, A";
  if (s.walking.dir1 < 0) {
   //Log::Info["serf"] << "debug: inside handle_serf_walking_state_dest_reached, B";
    PMap map = game->get_map();
    Building *building = game->get_building_at_pos(map->move_up_left(pos));
    building->requested_serf_reached(this);

    if (map->has_serf(map->move_up_left(pos))) {
     //Log::Info["serf"] << "debug: inside handle_serf_walking_state_dest_reached, C";
      animation = 85;
      counter = 0;
      set_state(StateReadyToEnter);
    } else {
     //Log::Info["serf"] << "debug: inside handle_serf_walking_state_dest_reached, D";
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
    set_state(StateTransporting);
    s.transporting.res = Resource::TypeNone;
    s.transporting.dir = dir;
    s.transporting.wait_counter = 0;

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
  if ((!map->has_flag(pos) && s.walking.wait_counter >= 10) ||
      s.walking.wait_counter >= 50) {
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
  }

  /* Stick to the same direction */
  s.walking.wait_counter = 0;
  change_direction((Direction)(s.walking.dir + 6), 0);
}

void
Serf::handle_serf_walking_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
   //Log::Info["serf"] << "debug: inside handle_serf_walking_state, start of while(counter < 0) loop, counter = " << counter;
   //Log::Info["serf"] << "debug: inside handle_serf_walking_state, s.walking.dir = " << s.walking.dir;
    if (s.walking.dir < 0) {
     //Log::Info["serf"] << "debug: inside handle_serf_walking_state, calling handle_serf_walking_state_waiting()";
      handle_serf_walking_state_waiting();
      continue;
    }
   //Log::Info["serf"] << "debug: inside handle_serf_walking_state, s.walking.dir is " << s.walking.dir;
    /* 301F0 */
    if (game->get_map()->has_flag(pos)) {
     //Log::Info["serf"] << "debug: inside handle_serf_walking_state, serf has reached a flag at pos " << pos;
      /* Serf has reached a flag.
         Search for a destination if none is known. */
      if (s.walking.dest == 0) {
       //Log::Info["serf"] << "debug: inside handle_serf_walking_state, s.walking.dest == 0";

        int flag_index = game->get_map()->get_obj_index(pos);
        Flag *src = game->get_flag(flag_index);

        // support allowing Lost serfs to enter any nearby friendly building
        //  EXCEPT mines, as they can deadlock when the miner runs out of food and 
        //  holds the pos, disallowing serfs entry
        int r = -1;
        if ((get_type() == Serf::TypeTransporter || get_type() == Serf::TypeGeneric || get_type() == Serf::TypeNone)){
          r = src->find_nearest_building_for_lost_generic_serf();
        }else{
          r = src->find_nearest_inventory_for_serf();
        }

        if (r < 0) {
         //Log::Info["serf"] << "debug: inside handle_serf_walking_state, making serf lost";
          set_state(StateLost);
          s.lost.field_B = 1;
          counter = 0;
          return;
        }
       //Log::Info["serf"] << "debug: inside handle_serf_walking_state, setting s.walking.dest to " << NameDirection[r];
        s.walking.dest = r;
      }
     //Log::Info["serf"] << "debug: inside handle_serf_walking_state, checking whether destination has been reached";
      /* Check whether destination has been reached.
         If not, find out which direction to move in
         to reach the destination. */
      if (s.walking.dest == game->get_map()->get_obj_index(pos)) {
       //Log::Info["serf"] << "debug: inside handle_serf_walking_state, at a flag, dest reached";
        handle_serf_walking_state_dest_reached();
        return;
      } else {
       //Log::Info["serf"] << "debug: inside handle_serf_walking_state, at a flag, dest NOT reached";
        Flag *src = game->get_flag_at_pos(pos);
        FlagSearch search(game);
        for (Direction i : cycle_directions_ccw()) {
          /*
          //
          // THIS WHOLE SECTION NEEDS TO BE MOVED
          //  into Serf::change_direction, I think
          //  because the serf needs to be allowed to run the flag search normally or else ANY serf that touches a water road
          //  will be force into WaitForBoat state, which is wrong.  Instead, let the flag search callback complete normally,
          //   which itself calls Serf::change_direction
          //   and do the test for setting serf state to WaitForBoat there
          //    AND this also lets me remove the flagsearch cleanup junk that had to be added to avoid the fact that
          ///     the flagsearch is not actually running!
          //
          // adding support for AIPlusOption::CanTransportSerfsInBoats
          // WARNING - is_water_path can only be uses to check a path that is certain to exist, it will return true if there is no path at all!
          if (src->is_water_path(i)){
           //Log::Info["serf"] << "debug: inside handle_serf_walking_state, serf at pos " << get_pos() << " found water path in dir " << NameDirection[i];
            if (src->has_transporter(i)){
             //Log::Info["serf"] << "debug: inside handle_serf_walking_state, setting flag at pos " << src->get_position() << " to has_serf_waiting_for_boat";
              src->set_serf_waiting_for_boat();
             //Log::Info["serf"] << "debug: inside handle_serf_walking_state, about to set serf at pos " << get_pos() << " state to Serf::StateWaitForBoat";
              set_state(Serf::StateWaitForBoat);
              // set the serf walking direction to the dir of the water path/boat it is waiting for
              s.walking.dir = i;
              // call the sailor out (by telling the sailor a resource was delivered?)
              Flag *other_flag = src->get_other_end_flag(i);
             //Log::Info["serf"] << "debug: inside handle_serf_walking_state, telling transporter(sailor?) at pos " << other_flag->get_position() << " that a resource dropped(?) in dir " << NameDirection[i];
              other_flag->set_search_dir(i);
              search.add_source(other_flag);
             //Log::Info["serf"] << "debug: inside handle_serf_walking_state, done telling transporter(sailor?) at pos " << other_flag->get_position() << " that a resource dropped(?) in dir " << NameDirection[i];
             //Log::Info["serf"] << "debug: inside handle_serf_walking_state, RETURNING EARLY TO AVOID calling handle_serf_walking_state search";
              // because this search is never actually executed it will cause the sailor, after dropping off his passenger, to go back
              //  to the flag where the passenger was first waiting (WaitForBoat state) and get stuck there.
              // I think this is because the search.execute clears the flag queue.
              //  as a solution, will manually call the Flag queue.clear() function as the sailor picks up the passenger at the flag
              return;
            }
          }
          */

          // adding support for AIPlusOption::CanTransportSerfsInBoats
          // WARNING - is_water_path can only be uses to check a path that is certain to exist, it will return true if there is no path at all!
          //if (src->has_path(i)){
            //if (!src->is_water_path(i)){
             //Log::Info["serf"] << "debug: a NON-water PATH AT FLAG POS " << src->get_position() << " WITH DIR " << NameDirection[i] << " BEING ADDED TO FLAG SEARCH";
            //}
            //if (src->is_water_path(i) && src->has_transporter(i)){
             //Log::Info["serf"] << "debug: VALID WATER PATH AT FLAG POS " << src->get_position() << " WITH DIR " << NameDirection[i] << " BEING ADDED TO FLAG SEARCH";
            //}
          //}
          // WARNING - is_water_path can only be uses to check a path that is certain to exist, it will return true if there is no path at all!
          //if (!src->is_water_path(i)) {
          if (!src->is_water_path(i) ||
             // adding support for AIPlusOption::CanTransportSerfsInBoats
             (game->get_ai_options_ptr()->test(AIPlusOption::CanTransportSerfsInBoats) &&
              src->has_path(i) && src->is_water_path(i) && src->has_transporter(i))){
            Flag *other_flag = src->get_other_end_flag(i);
            other_flag->set_search_dir(i);
            search.add_source(other_flag);
          }
        }
       //Log::Info["serf"] << "debug: inside handle_serf_walking_state, before search.execute";
        bool r = search.execute(handle_serf_walking_state_search_cb,
                                true, false, this);
       //Log::Info["serf"] << "debug: inside handle_serf_walking_state, after search.execute (before check), r=" << r;
       //Log::Info["serf"] << "debug: inside handle_serf_walking_state, after search.execute (before check), serf pos=" << pos;
        // I can't figure out what it is that triggers this loop to end,
        //  so just cut it short if serf is waiting for a boat
        if (r && get_state() == Serf::StateWaitForBoat){
         //Log::Info["serf"] << "debug: inside handle_serf_walking_state, after search.execute serf is in StateWaitForBoat, returning";
          return;
        }
        if (r) continue;
       //Log::Info["serf"] << "debug: inside handle_serf_walking_state, after search.execute (after check failed)";
      }
    } else {
     //Log::Info["serf"] << "debug: inside handle_serf_walking_state, Wug0";
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

   //Log::Info["serf"] << "debug: inside handle_serf_walking_state, Wug1";

    /* Either the road is a dead end; or
       we are at a flag, but the flag search for
       the destination failed. */
    if (s.walking.dir1 < 0) {
      if (s.walking.dir1 < -1) {
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
   //Log::Info["serf"] << "debug: inside handle_serf_walking_state, Wug2";
  }
  //Log::Info["serf"] << "debug: inside handle_serf_walking_state, end of while(counter < 0) loop, counter = " << counter;
}

void
Serf::handle_serf_transporting_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  //if (type == Serf::TypeSailor){
  // //Log::Info["serf"] << "debug: a sailor is in transporting state 000";
  //}

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
      // this crashes if Dir > 5 !
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
      
      // add support for AIPlusOption::CanTransportSerfsInBoats
      // if *this* serf is a sailor (who is already in transporting state and so must be in his boat)...
      //  and if another serf is at the next pos in StateWaitForBoat AND is waiting in the direction of this serf
      //   continue "through" the waiting serf to pick him up
      //
      bool sailor_pickup_serf = false;
     //Log::Info["serf"] << "debug: a serf in transporting state H B";
      MapPos new_pos = map->move(pos, dir);
     //Log::Info["serf"] << "debug: a serf in transporting state H C";
      // debugging
      //if (type == Serf::TypeSailor){
       //Log::Info["serf"] << "debug: a sailor inside handle_transporting_state, heading in dir " << NameDirection[dir];
        //if (game->get_serf_at_pos(new_pos)->get_state() == Serf::StateWaitForBoat){
         //Log::Info["serf"] << "debug: a sailor inside handle_serf_transporting_state, there is a serf in wait_for_boat state in next pos with dir " << NameDirection[game->get_serf_at_pos(new_pos)->get_walking_dir()];
         //Log::Info["serf"] << "debug: a sailor inside handle_serf_transporting_state, comparing sailor reversed-dir " << NameDirection[reverse_direction(Direction(dir))] << " to boat-wait serf dir " << NameDirection[game->get_serf_at_pos(new_pos)->get_walking_dir()];
        //}
      //}

     //Log::Info["serf"] << "debug: a serf in transporting state H ?";

      if (type == Serf::TypeSailor 
          && game->get_serf_at_pos(new_pos)->get_state() == Serf::StateWaitForBoat
          && game->get_serf_at_pos(new_pos)->get_walking_dir() == reverse_direction(Direction(dir))){
           //Log::Info["serf"] << "debug: a sailor inside handle_serf_transporting_state, next pos " << new_pos << ", setting sailor_pickup_serf to TRUE";
            sailor_pickup_serf = true;
      }

     //Log::Info["serf"] << "debug: a serf in transporting state H ??";

      if (!map->has_flag(map->move(pos, dir)) ||
          s.transporting.res != Resource::TypeNone ||
          s.transporting.wait_counter < 0 ||
          sailor_pickup_serf == true) {
            //if (type == Serf::TypeSailor){
             //Log::Info["serf"] << "debug: a sailor in transporting state H1, because at least one of previous H0s true, calling change_direction to dir " << NameDirection[dir];
            //}
        // this says change_direction but it is really "check the next direction in the path"
        change_direction(dir, 1);
        return;
      }
      //if (type == Serf::TypeSailor){
       //Log::Info["serf"] << "debug: a sailor in transporting state I";
      //}

      // a flag is at the next pos (or previous check would have returned)
      Flag *flag = game->get_flag_at_pos(map->move(pos, dir));
      Direction rev_dir = reverse_direction(dir);
      Flag *other_flag = flag->get_other_end_flag(rev_dir);
      Direction other_dir = flag->get_other_end_dir(rev_dir);

      if (flag->is_scheduled(rev_dir)) {
        //if (type == Serf::TypeSailor){
         //Log::Info["serf"] << "debug: a sailor in transporting state J0 - flag->is_scheduled in rev_dir " << NameDirection[rev_dir];
        //}
        change_direction(dir, 1);
        return;
      }
      //if (type == Serf::TypeSailor){
       //Log::Info["serf"] << "debug: a sailor in transporting state J";
      //}

      animation = 110 + s.walking.dir;
      counter = counter_from_animation[animation];
      s.walking.dir -= 6;

      if (flag->free_transporter_count(rev_dir) > 1) {
        //if (type == Serf::TypeSailor){
         //Log::Info["serf"] << "debug: a sailor in transporting state K";
        //}
        s.transporting.wait_counter += 1;
        if (s.transporting.wait_counter > 3) {
          flag->transporter_to_serve(rev_dir);
          other_flag->transporter_to_serve(other_dir);
          s.transporting.wait_counter = -1;
        }
      } else {
        //if (type == Serf::TypeSailor){
         //Log::Info["serf"] << "debug: a sailor in transporting state L";
        //}
        if (!other_flag->is_scheduled(other_dir)) {
          //if (type == Serf::TypeSailor){
           //Log::Info["serf"] << "debug: a sailor in transporting state M is about to be set to idle StateIdleOnPath";
          //}
          /* TODO Don't use anim as state var */
         //Log::Info["serf"] << "debug: inside handle_serf_transporting_state, setting serf to idle StateIdleOnPath, tick was " << tick << ", s.walking.dir was " << s.walking.dir;
          tick = (tick & 0xff00) | (s.walking.dir & 0xff);
         //Log::Info["serf"] << "debug: inside handle_serf_transporting_state, setting serf to idle StateIdleOnPath, tick is now " << tick;
          set_state(StateIdleOnPath);
          s.idle_on_path.rev_dir = rev_dir;
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
  game->get_map()->set_serf_index(pos, 0);
  Building *building = game->get_building_at_pos(pos);
  set_state(StateIdleInStock);
  /*serf->s.idle_in_stock.field_B = 0;
    serf->s.idle_in_stock.field_C = 0;*/
  s.idle_in_stock.inv_index = building->get_inventory()->get_index();
}

void
Serf::handle_serf_entering_building_state() {
  //Log::Info["serf"] << "debug: inside handle_serf_entering_building_state, counter was = " << counter;
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;
  //Log::Info["serf"] << "debug: inside handle_serf_entering_building_state, counter now = " << counter;
  if (counter < 0 || counter <= s.entering_building.slope_len) {
    //Log::Info["serf"] << "debug: inside handle_serf_entering_building_state, counter <0 or <= s.entering_building.slope_len";
    if (game->get_map()->get_obj_index(pos) == 0 ||
        game->get_building_at_pos(pos)->is_burning()) {
      //Log::Info["serf"] << "debug: inside handle_serf_entering_building_state, setting Lost state and returning";
      /* Burning */
      set_state(StateLost);
      s.lost.field_B = 0;
      counter = 0;
      return;
    }

    counter = s.entering_building.slope_len;
    PMap map = game->get_map();
    //Log::Info["serf"] << "debug: inside handle_serf_entering_building_state, switching, this serf type is " << NameSerf[get_type()];

    // support allowing Lost serfs to enter any nearby friendly building
    if ((get_type() == Serf::TypeTransporter || get_type() == Serf::TypeGeneric || get_type() == Serf::TypeNone)){
      PMap map = game->get_map();
      if (map->has_building(pos)){
        Building *building = game->get_building_at_pos(pos);
        if (building->is_done() && !building->is_burning() &&
            building->get_type() != Building::TypeStock && building->get_type() != Building::TypeCastle){
          Log::Info["serf"] << "Debug - a generic serf at pos " << pos << " has arrived in a non-inventory building of type " << NameBuilding[building->get_type()] << ", assuming this was a Lost Serf";
          //game->delete_serf(this);  // this crashes, didn't check why
          //enter_inventory();  // this crashes because it calls get_inventory() on attached building, but it isn't an Inventory
          // this is needed so the serf stops appearing at the building entrance, but does it affect a serf already
          //  occupying this building???   so far with knight huts it causes no issue
          game->get_map()->set_serf_index(pos, 0);
          // this might not be necessary, but seems like a decent idea to make it clear this serf is in limbo
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
          set_state(StateDigging);
          s.digging.h_index = 15;

          Building *building = game->get_building_at_pos(pos);
          s.digging.dig_pos = 6;
          s.digging.target_h = building->get_level();
          s.digging.substate = 1;
        }
        break;
      case TypeBuilder:
        //Log::Info["serf"] << "debug: inside handle_serf_entering_building_state, case TypeBuilder";
        if (s.entering_building.field_B == -2) {
          //Log::Info["serf"] << "debug: inside handle_serf_entering_building_state, case TypeBuilder, calling enter_inventory";
          enter_inventory();
        } else {
          //Log::Info["serf"] << "debug: inside handle_serf_entering_building_state, case TypeBuilder, setting StateBuilding";
          set_state(StateBuilding);
          animation = 98;
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
            // start with two pigs if AIPlusOption::ImprovedPigFarms is set
            //   how else could they reproduce?
            if (game->get_ai_options_ptr()->test(AIPlusOption::ImprovedPigFarms)) {
              building->set_initial_res_in_stock(1, 2);
            } else {
              // game default
              building->set_initial_res_in_stock(1, 1);
            }

            flag->clear_flags();
            // resource #0 is the wheat that pigs consume (though it is bugged)
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
          enter_inventory();
        } else {
          Building *building = game->get_building_at_pos(pos);
          if (building->is_burning()) {
            set_state(StateLost);
            counter = 0;
          } else {
            map->set_serf_index(pos, 0);

            if (building->has_inventory()) {
              set_state(StateDefendingCastle);
              counter = 6000;

              /* Prepend to knight list */
              s.defending.next_knight = building->get_first_knight();
              building->set_first_knight(get_index());

              game->get_player(
                              building->get_owner())->increase_castle_knights();
              return;
            }

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

            /* Prepend to knight list */
            s.defending.next_knight = building->get_first_knight();
            building->set_first_knight(get_index());
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
  MapPos new_pos = game->get_map()->move_up_left(pos);

  if (game->get_map()->has_serf(new_pos)) {
    animation = 85;
    counter = 0;
    return;
  }

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
            Log::Verbose["serf"] << "  found at: " << s.digging.dig_pos << ".";
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
        animation = 98;
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
  Inventory *inventory = building->get_inventory();
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

  while (counter < 0) {
    if (s.transporting.wait_counter != 0) {
      set_state(StateTransporting);
      s.transporting.wait_counter = 0;
      Flag *flag = game->get_flag(game->get_map()->get_obj_index(pos));
     //Log::Info["serf"] << "debug: inside handle_serf_delivering_state, about to call transporter_move_to_flag at pos " << flag->get_position();
      transporter_move_to_flag(flag);
      return;
    }

    if (s.transporting.res != Resource::TypeNone) {
      Resource::Type res = s.transporting.res;
      s.transporting.res = Resource::TypeNone;
      Building *building =
                  game->get_building_at_pos(game->get_map()->move_up_left(pos));
      building->requested_resource_delivered(res);
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

  PMap map = game->get_map();
  if (map->has_serf(pos) || map->has_serf(map->move_down_right(pos))) {
    animation = 82;
    counter = 0;
    return;
  }

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

  Inventory *inventory =
                      game->get_inventory(s.ready_to_leave_inventory.inv_index);
  inventory->serf_away();

  Serf::State next_state = StateWalking;
  if (s.ready_to_leave_inventory.mode == -3) {
    next_state = StateScatter;
  }

  int mode = s.ready_to_leave_inventory.mode;
  unsigned int dest = s.ready_to_leave_inventory.dest;

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

  set_state(StateLost);
  s.lost.field_B = 0;
  counter = 0;
}

void
Serf::handle_serf_free_walking_state_dest_reached() {
  if (s.free_walking.neg_dist1 == -128 &&
      s.free_walking.neg_dist2 < 0) {
    Log::Info["serf"] << "debug : Serf::handle_serf_free_walking_state_dest_reached s.free_walking.neg_dist1: " << s.free_walking.neg_dist1 << ", s.free_walking.neg_dist2: " << s.free_walking.neg_dist2;
    // support allowing Lost serfs to enter any nearby friendly building
    if ((get_type() == Serf::TypeTransporter || get_type() == Serf::TypeGeneric || get_type() == Serf::TypeNone)){
      PMap map = game->get_map();
      MapPos upleft_pos = map->move_up_left(pos);
      if (map->has_building(upleft_pos)){
        Building *building = game->get_building_at_pos(upleft_pos);
        if (building->is_done() && !building->is_burning() &&
            building->get_type() != Building::TypeStock && building->get_type() != Building::TypeCastle){
          Log::Info["serf"] << "Debug - a generic serf at pos " << pos << " is about to enter a non-inventory building of type " << NameBuilding[building->get_type()] << " at pos " << upleft_pos << ", assuming this was a Lost Serf";
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

  if (BIT_TEST(s.free_walking.flags, 3)) {
    /* Follow right-hand edge */
    dir_arr = dir_left_edge;
    dir_index = (s.free_walking.flags & 7)-1;
  } else {
    /* Follow right-hand edge */
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

/* this doesn't work because these serfs are already in Walking state once
  they reach a road, not FreeWalking!  move this code there
  // support allowing Lost serfs to enter any nearby friendly building
  // hack, doing this at the beginning because I can't figure out how the FreeWalking
  // dest/pathfinding works.  It seems serfs will go to the nearest flag that has a road
  //  and then start walking to nearest Inventory that accepts serfs.
  // because I cannot figure out how to route them to a nearby non-Inventory building
  // I should at least be able to have them try to enter any building they happen across
  // while walking on paths back to nearest Inventory building
  if ((get_type() == Serf::TypeTransporter || get_type() == Serf::TypeGeneric || get_type() == Serf::TypeNone)){
    PMap map = game->get_map();
    MapPos upleft_pos = map->move_up_left(pos);
    if (map->has_flag(pos) && map->has_building(upleft_pos) && map->get_owner(pos) == get_owner()){
      Building *building = game->get_building_at_pos(upleft_pos);
      //if (building->is_done() && building->get_type() != Building::TypeStock && building->get_type() != Building::TypeCastle){
      if (building->is_done()){
        Log::Info["serf"] << "Debug - handle_free_walking_common_hack - a generic serf at pos " << pos << " is about to enter a non-inventory building of type " << NameBuilding[building->get_type()] << " at pos " << upleft_pos << ", assuming this was a Lost Serf";
        set_state(StateReadyToEnter);
        s.ready_to_enter.field_B = 0;
        counter = 0;
        return;
      }
    }
  }
  */

  const Direction dir_from_offset[] = {
    DirectionUpLeft, DirectionUp,   DirectionNone,
    DirectionLeft,   DirectionNone, DirectionRight,
    DirectionNone,   DirectionDown, DirectionDownRight
  };

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
    int r = handle_free_walking_follow_edge();
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
              other_serf->set_lost_state();
            }
          } else {
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
  if (BIT_TEST(dir_index ^ i0, 0)) edge = 1;
  int upper = (i0/2) + 1;

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
    Map::Object new_obj = (Map::Object)(Map::ObjectNewPine +
                                    (game->random_int() & 1));

    if (map->paths(pos) == 0 && map->get_obj(pos) == Map::ObjectNone) {
      map->set_object(pos, new_obj, -1);
    }

    s.free_walking.neg_dist2 = -s.free_walking.neg_dist2 - 1;
    counter += 128;
  }
}

void
Serf::handle_serf_planning_stonecutting() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  PMap map = game->get_map();
  while (counter < 0) {
    int dist = (game->random_int() & 0x7f) + 1;
    MapPos pos_ = map->pos_add_spirally(pos, dist);
    int obj = map->get_obj(map->move_up_left(pos_));
    if (obj >= Map::ObjectStone0 && obj <= Map::ObjectStone7 &&
        can_pass_map_pos(pos_)) {
      set_state(StateReadyToLeave);
      s.leaving_building.field_B = Map::get_spiral_pattern()[2 * dist] - 1;
      s.leaving_building.dest = Map::get_spiral_pattern()[2 * dist + 1] - 1;
      s.leaving_building.dest2 = -Map::get_spiral_pattern()[2 * dist] + 1;
      s.leaving_building.dir = -Map::get_spiral_pattern()[2 * dist + 1] + 1;
      s.leaving_building.next_state = StateStoneCutterFreeWalking;
      Log::Verbose["serf"] << "planning stonecutting: stone found, dist "
                           << s.leaving_building.field_B << ", "
                           << s.leaving_building.dest << ".";
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

void
Serf::handle_serf_lost_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  PMap map = game->get_map();
  while (counter < 0) {
    /* Try to find a suitable destination. */
    for (int i = 0; i < 258; i++) {
      int dist = (s.lost.field_B == 0) ? 1+i : 258-i;
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
            // tlongstretch - allow generic serfs to enter ANY completed friendly building
            //  just to get them off the map.  For now, simply delete them on arrival,
            //  could also be possible to "teleport" them to the nearest Inventory
            // generic serfs go to nearest building
            ///
            /// UGH - it seems that the destination is not handled how you would expect
            ///   Rather, the serf enters FreeWalking state here, and FreeWalking state
            ///   does not go by a MapPos dest, but instead uses a "dist from col/row" 
            ///   that it constantly updates using bit-math.  Cannot simply set a serf.dest
            ///   because FreeWalking state ignores it
            ///  MAYBE... is it possible to simply get the col/row of the MapPos dest we 
            ///   desire?  Or is it relative to the serf's current position???
            /// 
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

          //*****************************************************************
          // wait, is it really this simple?  just set it here?  adding this
          // no... this results in all the serfs walking southwest ??
          // try doing it once they reach a road instead
          //s.walking.dest = flag->get_index();
          //*****************************************************************

          counter = 0;
          return;
        }
      }
    }

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
      set_state(StateLost);
      s.lost.field_B = 0;
      return;
    }

    handle_free_walking_common();
  }
}

void
Serf::handle_serf_escape_building_state() {
  if (!game->get_map()->has_serf(pos)) {
    game->get_map()->set_serf_index(pos, index);
    animation = 82;
    counter = 0;
    tick = game->get_tick();

    set_state(StateLost);
    s.lost.field_B = 0;
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
        /* There is a small chance that the miner will
           not require food and skip to state 2. */
        int r = game->random_int();
        if ((r & 7) == 0) {
          s.mining.substate = 2;
        } else {
          s.mining.substate = 1;
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
          animation = 98;
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
  if (counter > 0) {
    return;
  }

  PMap map = game->get_map();
  while (true) {
    int dist = ((game->random_int() >> 2) & 0x1f) + 7;
    MapPos dest = map->pos_add_spirally(pos, dist);

    /* If destination doesn't have an object it must be
       of the correct type and the surrounding spaces
       must not be occupied by large buildings.
       If it _has_ an object it must be an existing field. */
    if ((map->get_obj(dest) == Map::ObjectNone &&
         (map->type_up(dest) == Map::TerrainGrass1 &&
          map->type_down(dest) == Map::TerrainGrass1 &&
          map->paths(dest) == 0 &&
          map->get_obj(map->move_right(dest)) != Map::ObjectLargeBuilding &&
          map->get_obj(map->move_right(dest)) != Map::ObjectCastle &&
         map->get_obj(map->move_down_right(dest)) != Map::ObjectLargeBuilding &&
          map->get_obj(map->move_down_right(dest)) != Map::ObjectCastle &&
          map->get_obj(map->move_down(dest)) != Map::ObjectLargeBuilding &&
          map->get_obj(map->move_down(dest)) != Map::ObjectCastle &&
          map->type_down(map->move_left(dest)) == Map::TerrainGrass1 &&
          map->get_obj(map->move_left(dest)) != Map::ObjectLargeBuilding &&
          map->get_obj(map->move_left(dest)) != Map::ObjectCastle &&
          map->type_up(map->move_up_left(dest)) == Map::TerrainGrass1 &&
          map->type_down(map->move_up_left(dest)) == Map::TerrainGrass1 &&
          map->get_obj(map->move_up_left(dest)) != Map::ObjectLargeBuilding &&
          map->get_obj(map->move_up_left(dest)) != Map::ObjectCastle &&
          map->type_up(map->move_up(dest)) == Map::TerrainGrass1 &&
          map->get_obj(map->move_up(dest)) != Map::ObjectLargeBuilding &&
          map->get_obj(map->move_up(dest)) != Map::ObjectCastle)) ||
        map->get_obj(dest) == Map::ObjectSeeds5 ||
        (map->get_obj(dest) >= Map::ObjectField0 &&
         map->get_obj(dest) <= Map::ObjectField5)) {
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

    counter += 500;
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

  if (counter >= 0) return;

  PMap map = game->get_map();
  Map::Object object = map->get_obj(pos);
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
      object = Map::ObjectField0;
    } else if (object == Map::ObjectSignLargeGold || object == Map::Object127) {
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
  //  In it, pig farms only ever use a single wheat resource, 
  //  yet they will store up to four (or eight?) forever.
  //  Freeserf replicates this bug.
  const int breeding_prob[] = {
    6000, 8000, 10000, 11000, 12000, 13000, 14000, 0
  };

  Building *building = game->get_building_at_pos(pos);
  if (s.pigfarming.mode == 0) {

    if (game->get_ai_options_ptr()->test(AIPlusOption::ImprovedPigFarms)
      || building->use_resource_in_stock(0)) {
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
        if (building->pigs_count() < 8 &&
            game->random_int() < breeding_prob[building->pigs_count()-1]) {
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

  Building *building =
                  game->get_building_at_pos(game->get_map()->move_up_left(pos));
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

  /* Something is wrong. */
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
              if (!building->has_inventory()) {
                building->requested_knight_attacking_on_walk();
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

void
Serf::handle_serf_idle_on_path_state() {
  Flag *flag = game->get_flag(s.idle_on_path.flag);
  if (flag == nullptr) {
    set_state(StateLost);
    return;
  }
  Direction rev_dir = s.idle_on_path.rev_dir;

  //if (get_type() == Serf::TypeSailor){Log::Info["serf"] << "debug: sailor inside handle_serf_idle_on_path_state";}

  // check the flag in each direction to see if it needs a transporter 
  //  to come pick up a resource, or a serf if this is a sailor and 
  //   AIPlusOption::CanTransportSerfsInBoats is set
  // * Set walking dir in field_E. */

  //Log::Info["serf"] << "debug: before idle transporter checking flags, s.idle_on_path.field_E = " << s.idle_on_path.field_E;
  //Log::Info["serf"] << "debug: inside handle_serf_idle_on_path_state, checking flag at pos " << flag->get_position();
  // check the flag closest to the idle transporter(?) - assuming s.idle_on_path.flag means that

  //
  // this could be squashed back into two statements, but for now it is easier to debug with them separated
  //
  if (get_type() == Serf::TypeSailor && flag->has_serf_waiting_for_boat()){
   //Log::Info["serf"] << "debug: sailor inside handle_serf_idle_on_path_state, sailor found flag at pos " << flag->get_position() << " with serf_waiting_for_boat, setting dir this way";
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
    s.idle_on_path.field_E = (tick & 0xff) + 6;
    //s.idle_on_path.field_E = rev_dir;
  } else if (flag->is_scheduled(rev_dir)) {
   //Log::Info["serf"] << "debug: serf inside handle_serf_idle_on_path_state, this flag is scheduled";
    s.idle_on_path.field_E = (tick & 0xff) + 6;
    //s.idle_on_path.field_E = rev_dir;
  } else {
    // check the other flag
    Flag *other_flag = flag->get_other_end_flag(rev_dir);
    //Log::Info["serf"] << "debug: inside handle_serf_idle_on_path_state, checking other_flag at pos " << other_flag->get_position();
    Direction other_dir = flag->get_other_end_dir((Direction)rev_dir);
    if (get_type() == Serf::TypeSailor && other_flag->has_serf_waiting_for_boat()){
     //Log::Info["serf"] << "debug: inside handle_serf_idle_on_path_state, sailor found other_flag at pos " << other_flag->get_position() << " with serf_waiting_for_boat, setting dir that way";
      s.idle_on_path.field_E = reverse_direction(rev_dir);
    } else if (other_flag && other_flag->is_scheduled(other_dir)) {
      //Log::Info["serf"] << "serf: sailor inside handle_serf_idle_on_path_state, other flag is scheduled";
      s.idle_on_path.field_E = reverse_direction(rev_dir);
    } else {
      //Log::Info["serf"] << "debug: serf inside handle_serf_idle_on_path_state, nothing is true, returning";
      return;
    }
  }
  //Log::Info["serf"] << "debug: after idle transporter checking flags, s.idle_on_path.field_E = " << s.idle_on_path.field_E;

  PMap map = game->get_map();
  if (!map->has_serf(pos)) {
    //if (get_type() == Serf::TypeSailor){
       //Log::Info["serf"] << "debug: inside handle_serf_idle_on_path_state, sailor end 1";
    //}
    map->clear_idle_serf(pos);
    map->set_serf_index(pos, index);
    //if (get_type() == Serf::TypeSailor){
       //Log::Info["serf"] << "debug: inside handle_serf_idle_on_path_state, sailor end 2";
    //}
    int dir = s.idle_on_path.field_E;
    //if (get_type() == Serf::TypeSailor){
       //Log::Info["serf"] << "debug: inside handle_serf_idle_on_path_state, sailor end 3";
    //}
    set_state(StateTransporting);
    s.transporting.res = Resource::TypeNone;
    s.transporting.wait_counter = 0;
    s.transporting.dir = dir;
    tick = game->get_tick();
    counter = 0;
    //if (get_type() == Serf::TypeSailor){
       //Log::Info["serf"] << "debug: inside handle_serf_idle_on_path_state, sailor end 4";
    //}
  } else {
    //if (get_type() == Serf::TypeSailor){
       //Log::Info["serf"] << "debug: inside handle_serf_idle_on_path_state, sailor end 5";
    //}
   //Log::Info["serf"] << "debug: inside handle_serf_idle_on_path_state, a serf of any type is about to be set to StateWaitIdleOnPath !";
    set_state(StateWaitIdleOnPath);
  }
}

void
Serf::handle_serf_wait_idle_on_path_state() {
  //if (get_type() == Serf::TypeSailor){
   //Log::Info["serf"] << "debug: inside handle_serf_wait_idle_on_path_state, a Sailor has reached destination";
  //}
  PMap map = game->get_map();
  if (!map->has_serf(pos)) {
    /* Duplicate code from handle_serf_idle_on_path_state() */
    map->clear_idle_serf(pos);
    map->set_serf_index(pos, index);

    int dir = s.idle_on_path.field_E;

    set_state(StateTransporting);
    s.transporting.res = Resource::TypeNone;
    s.transporting.wait_counter = 0;
    s.transporting.dir = dir;
    tick = game->get_tick();
    counter = 0;
  }
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

    if (get_type() == TypeSailor) {
      set_state(StateLostSailor);
    } else {
      set_state(StateLost);
      s.lost.field_B = 0;
    }
  }
}

void
Serf::handle_serf_wake_on_path_state() {
  set_state(StateWaitIdleOnPath);

  for (Direction d : cycle_directions_ccw()) {
    if (BIT_TEST(game->get_map()->paths(pos), d)) {
      s.idle_on_path.field_E = d;
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
  //Log::Info["serf"] << "debug: a serf at pos " << pos << " is in state Serf::BoatPassenger";
  // do nothing
  // in fact, this state should never be checked because serfs in BoatPassenger 
  //  state do not have an attached map pos and so should never be checked for drawing/update?
}


void
Serf::update() {
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
    handle_serf_planning_planting_state();
    break;
  case StatePlanting: /* 20 */
    handle_serf_planting_state();
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
