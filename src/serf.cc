/*
 * serf.cc - Serf related functions
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

#include "src/serf.h"

#include <cassert>
#include <algorithm>
#include <map>

#include "src/game.h"
#include "src/log.h"
#include "src/debug.h"
#include "src/misc.h"
#include "src/inventory.h"
#include "src/notification.h"
#include "src/savegame.h"

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
};


const char *
serf_t::get_state_name(serf_state_t state) {
  return serf_state_name[state];
}

static const char *serf_type_name[] = {
  "TRANSPORTER",  // SERF_TRANSPORTER = 0,
  "SAILOR",  // SERF_SAILOR,
  "DIGGER",  // SERF_DIGGER,
  "BUILDER",  // SERF_BUILDER,
  "TRANSPORTER_INVENTORY",  // SERF_TRANSPORTER_INVENTORY,
  "LUMBERJACK",  // SERF_LUMBERJACK,
  "SAWMILLER",  // SERF_SAWMILLER,
  "STONECUTTER",  // SERF_STONECUTTER,
  "FORESTER",  // SERF_FORESTER,
  "MINER",  // SERF_MINER,
  "SMELTER",  // SERF_SMELTER,
  "FISHER",  // SERF_FISHER,
  "PIGFARMER",  // SERF_PIGFARMER,
  "BUTCHER",  // SERF_BUTCHER,
  "FARMER",  // SERF_FARMER,
  "MILLER",  // SERF_MILLER,
  "BAKER",  // SERF_BAKER,
  "BOATBUILDER",  // SERF_BOATBUILDER,
  "TOOLMAKER",  // SERF_TOOLMAKER,
  "WEAPONSMITH",  // SERF_WEAPONSMITH,
  "GEOLOGIST",  // SERF_GEOLOGIST,
  "GENERIC",  // SERF_GENERIC,
  "KNIGHT_0",  // SERF_KNIGHT_0,
  "KNIGHT_1",  // SERF_KNIGHT_1,
  "KNIGHT_2",  // SERF_KNIGHT_2,
  "KNIGHT_3",  // SERF_KNIGHT_3,
  "KNIGHT_4",  // SERF_KNIGHT_4,
  "DEAD",  // SERF_DEAD
};

const char *
serf_t::get_type_name(serf_type_t type) {
  return serf_type_name[type];
}

serf_t::serf_t(game_t *game, unsigned int index) : game_object_t(game, index) {
  state = SERF_STATE_NULL;
  owner = -1;
  type = SERF_NONE;
  sound = false;
  animation = 0;
  counter = 0;
  pos = -1;
}

/* Change type of serf and update all global tables
   tracking serf types. */
void
serf_t::set_type(serf_type_t new_type) {
  serf_type_t old_type = get_type();
  type = new_type;

  /* Register this type as transporter */
  if (new_type == SERF_TRANSPORTER_INVENTORY) new_type = SERF_TRANSPORTER;
  if (old_type == SERF_TRANSPORTER_INVENTORY) old_type = SERF_TRANSPORTER;

  player_t *player = game->get_player(get_player());
  player->decrease_serf_count(old_type);

  if (type != SERF_DEAD) {
    player->increase_serf_count(new_type);
  }

  if (old_type >= SERF_KNIGHT_0 &&
      old_type <= SERF_KNIGHT_4) {
    int value = 1 << (old_type - SERF_KNIGHT_0);
    player->decrease_military_score(value);
  }
  if (new_type >= SERF_KNIGHT_0 &&
      new_type <= SERF_KNIGHT_4) {
    int value = 1 << (type - SERF_KNIGHT_0);
    player->increase_military_score(value);
  }
  if (new_type == SERF_TRANSPORTER) {
    counter = 0;
  }
}

void
serf_t::add_to_defending_queue(unsigned int next_knight_index, bool pause) {
  serf_log_state_change(this, SERF_STATE_DEFENDING_CASTLE);
  state = SERF_STATE_DEFENDING_CASTLE;
  s.defending.next_knight = next_knight_index;
  if (pause) {
    counter = 6000;
  }
}

void
serf_t::init_generic(inventory_t *inventory) {
  set_type(SERF_GENERIC);
  set_player(inventory->get_owner());
  building_t *building = game->get_building(inventory->get_building_index());
  pos = building->get_position();
  tick = game->get_tick();
  state = SERF_STATE_IDLE_IN_STOCK;
  s.idle_in_stock.inv_index = inventory->get_index();
}

void
serf_t::init_inventory_transporter(inventory_t *inventory) {
  serf_log_state_change(this, SERF_STATE_BUILDING_CASTLE);
  state = SERF_STATE_BUILDING_CASTLE;
  s.building_castle.inv_index = inventory->get_index();
}

void
serf_t::reset_transport(flag_t *flag) {
  if (state == SERF_STATE_WALKING &&
      s.walking.dest == flag->get_index() &&
      s.walking.res < 0) {
      s.walking.res = -2;
      s.walking.dest = 0;
  } else if (state == SERF_STATE_READY_TO_LEAVE_INVENTORY &&
             s.ready_to_leave_inventory.dest == flag->get_index() &&
             s.ready_to_leave_inventory.mode < 0) {
    s.ready_to_leave_inventory.mode = -2;
    s.ready_to_leave_inventory.dest = 0;
  } else if ((state == SERF_STATE_LEAVING_BUILDING ||
              state == SERF_STATE_READY_TO_LEAVE) &&
             s.leaving_building.next_state == SERF_STATE_WALKING &&
             s.leaving_building.dest == flag->get_index() &&
             s.leaving_building.field_B < 0) {
    s.leaving_building.field_B = -2;
    s.leaving_building.dest = 0;
  } else if (state == SERF_STATE_TRANSPORTING &&
             s.walking.dest == flag->get_index()) {
    s.walking.dest = 0;
  } else if (state == SERF_STATE_MOVE_RESOURCE_OUT &&
             s.move_resource_out.next_state == SERF_STATE_DROP_RESOURCE_OUT &&
             s.move_resource_out.res_dest == flag->get_index()) {
    s.move_resource_out.res_dest = 0;
  } else if (state == SERF_STATE_DROP_RESOURCE_OUT &&
             s.move_resource_out.res_dest == flag->get_index()) {
    s.move_resource_out.res_dest = 0;
  } else if (state == SERF_STATE_LEAVING_BUILDING &&
             s.leaving_building.next_state == SERF_STATE_DROP_RESOURCE_OUT &&
             s.leaving_building.dest == flag->get_index()) {
    s.leaving_building.dest = 0;
  }
}

bool
serf_t::path_splited(unsigned int flag_1, dir_t dir_1,
                     unsigned int flag_2, dir_t dir_2,
                     int *select) {
  if (state == SERF_STATE_WALKING) {
    if (s.walking.dest == flag_1 &&
        s.walking.res == dir_1) {
      select = 0;
      return true;
    } else if (s.walking.dest == flag_2 &&
               s.walking.res == dir_2) {
      *select = 1;
      return true;
    }
  } else if (state == SERF_STATE_READY_TO_LEAVE_INVENTORY) {
    if (s.ready_to_leave_inventory.dest == flag_1 &&
        s.ready_to_leave_inventory.mode == dir_1) {
      select = 0;
      return true;
    } else if (s.ready_to_leave_inventory.dest == flag_2 &&
               s.ready_to_leave_inventory.mode == dir_2) {
      *select = 1;
      return true;
    }
  } else if ((state == SERF_STATE_READY_TO_LEAVE ||
              state == SERF_STATE_LEAVING_BUILDING) &&
             s.leaving_building.next_state == SERF_STATE_WALKING) {
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
serf_t::is_related_to(unsigned int dest, dir_t dir) {
  bool result = false;

  switch (state) {
    case SERF_STATE_WALKING:
      if (s.walking.dest == dest && s.walking.res == dir) {
        result = true;
      }
      break;
    case SERF_STATE_READY_TO_LEAVE_INVENTORY:
      if (s.ready_to_leave_inventory.dest == dest &&
          s.ready_to_leave_inventory.mode == dir) {
        result = true;
      }
      break;
    case SERF_STATE_LEAVING_BUILDING:
    case SERF_STATE_READY_TO_LEAVE:
      if (s.leaving_building.dest == dest &&
          s.leaving_building.field_B == dir &&
          s.leaving_building.next_state == SERF_STATE_WALKING) {
        result = true;
      }
      break;
    default:
      break;
  }

  return result;
}

void
serf_t::path_deleted(unsigned int dest, dir_t dir) {
  switch (state) {
    case SERF_STATE_WALKING:
      if (s.walking.dest == dest &&
          s.walking.res == dir) {
        s.walking.res = -2;
        s.walking.dest = 0;
      }
      break;
    case SERF_STATE_READY_TO_LEAVE_INVENTORY:
      if (s.ready_to_leave_inventory.dest == dest &&
          s.ready_to_leave_inventory.mode == dir) {
        s.ready_to_leave_inventory.mode = -2;
        s.ready_to_leave_inventory.dest = 0;
      }
      break;
    case SERF_STATE_LEAVING_BUILDING:
    case SERF_STATE_READY_TO_LEAVE:
      if (s.leaving_building.dest == dest &&
          s.leaving_building.field_B == dir &&
          s.leaving_building.next_state == SERF_STATE_WALKING) {
        s.leaving_building.field_B = -2;
        s.leaving_building.dest = 0;
      }
      break;
    default:
      break;
  }
}

void
serf_t::path_merged(flag_t *flag) {
  if (state == SERF_STATE_READY_TO_LEAVE_INVENTORY &&
      s.ready_to_leave_inventory.dest == flag->get_index()) {
    s.ready_to_leave_inventory.dest = 0;
    s.ready_to_leave_inventory.mode = -2;
  } else if (state == SERF_STATE_WALKING &&
           s.walking.dest == flag->get_index()) {
    s.walking.dest = 0;
    s.walking.res = -2;
  } else if (state == SERF_STATE_IDLE_IN_STOCK && 1/*...*/) {
    /* TODO */
  } else if ((state == SERF_STATE_LEAVING_BUILDING ||
            state == SERF_STATE_READY_TO_LEAVE) &&
           s.leaving_building.dest == flag->get_index() &&
           s.leaving_building.next_state == SERF_STATE_WALKING) {
    s.leaving_building.dest = 0;
    s.leaving_building.field_B = -2;
  }
}

void
serf_t::path_merged2(unsigned int flag_1, dir_t dir_1,
                     unsigned int flag_2, dir_t dir_2) {
  if (state == SERF_STATE_READY_TO_LEAVE_INVENTORY &&
      ((s.ready_to_leave_inventory.dest == flag_1 &&
        s.ready_to_leave_inventory.mode == dir_1) ||
       (s.ready_to_leave_inventory.dest == flag_2 &&
        s.ready_to_leave_inventory.mode == dir_2))) {
    s.ready_to_leave_inventory.dest = 0;
    s.ready_to_leave_inventory.mode = -2;
  } else if (state == SERF_STATE_WALKING &&
             ((s.walking.dest == flag_1 &&
               s.walking.res == dir_1) ||
              (s.walking.dest == flag_2 &&
               s.walking.res == dir_2))) {
    s.walking.dest = 0;
    s.walking.res = -2;
  } else if (state == SERF_STATE_IDLE_IN_STOCK) {
    /* TODO */
  } else if ((state == SERF_STATE_LEAVING_BUILDING ||
              state == SERF_STATE_READY_TO_LEAVE) &&
             ((s.leaving_building.dest == flag_1 &&
               s.leaving_building.field_B == dir_1) ||
              (s.leaving_building.dest == flag_2 &&
               s.leaving_building.field_B == dir_2)) &&
             s.leaving_building.next_state == SERF_STATE_WALKING) {
    s.leaving_building.dest = 0;
    s.leaving_building.field_B = -2;
  }
}

void
serf_t::flag_deleted(map_pos_t flag_pos) {
  switch (state) {
    case SERF_STATE_READY_TO_LEAVE:
    case SERF_STATE_LEAVING_BUILDING:
      s.leaving_building.next_state = SERF_STATE_LOST;
      break;
    case SERF_STATE_FINISHED_BUILDING:
    case SERF_STATE_WALKING:
      if (game->get_map()->paths(flag_pos) == 0) {
        serf_log_state_change(this, SERF_STATE_LOST);
        state = SERF_STATE_LOST;
      }
      break;
    default:
      break;
  }
}

bool
serf_t::building_deleted(map_pos_t building_pos, bool escape) {
  if (pos == building_pos &&
      (state == SERF_STATE_IDLE_IN_STOCK ||
       state == SERF_STATE_READY_TO_LEAVE_INVENTORY)) {
    if (escape) {
      /* Serf is escaping. */
      state = SERF_STATE_ESCAPE_BUILDING;
    } else {
      /* Kill this serf. */
      set_type(SERF_DEAD);
      game->delete_serf(this);
    }
    return true;
  }

  return false;
}

void
serf_t::castle_deleted(map_pos_t castle_pos, bool transporter) {
  if ((!transporter || (get_type() == SERF_TRANSPORTER_INVENTORY)) &&
      pos == castle_pos) {
    if (transporter) {
      set_type(SERF_TRANSPORTER);
    }

    if (game->get_map()->get_serf_index(pos) == index) {
      serf_log_state_change(this, SERF_STATE_LOST);
      state = SERF_STATE_LOST;
      s.lost.field_B = 0;
    } else {
      serf_log_state_change(this, SERF_STATE_ESCAPE_BUILDING);
      state = SERF_STATE_ESCAPE_BUILDING;
    }
  }
}


bool
serf_t::change_transporter_state_at_pos(map_pos_t pos_,
                                        serf_state_t state) {
  if (pos == pos_ &&
      (state == SERF_STATE_WAKE_AT_FLAG ||
       state == SERF_STATE_WAKE_ON_PATH ||
       state == SERF_STATE_WAIT_IDLE_ON_PATH ||
       state == SERF_STATE_IDLE_ON_PATH)) {
    serf_log_state_change(this, state);
    state = state;
    return true;
  }
  return false;
}

void
serf_t::restore_path_serf_info() {
  if (state != SERF_STATE_WAKE_ON_PATH) {
    s.walking.wait_counter = -1;
    if (s.walking.res != 0) {
      resource_type_t res = (resource_type_t)(s.walking.res-1);
      s.walking.res = 0;

      game->cancel_transported_resource(res, s.walking.dest);
      game->lose_resource(res);
    }
  } else {
    serf_log_state_change(this, SERF_STATE_WAKE_AT_FLAG);
    state = SERF_STATE_WAKE_AT_FLAG;
  }
}

void
serf_t::clear_destination(unsigned int dest) {
  switch (state) {
    case SERF_STATE_WALKING:
      if (s.walking.dest == dest &&
          s.walking.res < 0) {
        s.walking.res = -2;
        s.walking.dest = 0;
      }
      break;
    case SERF_STATE_READY_TO_LEAVE_INVENTORY:
      if (s.ready_to_leave_inventory.dest == dest &&
          s.ready_to_leave_inventory.mode < 0) {
        s.ready_to_leave_inventory.mode = -2;
        s.ready_to_leave_inventory.dest = 0;
      }
      break;
    case SERF_STATE_LEAVING_BUILDING:
    case SERF_STATE_READY_TO_LEAVE:
      if (s.leaving_building.dest == dest &&
          s.leaving_building.field_B < 0 &&
          s.leaving_building.next_state == SERF_STATE_WALKING) {
        s.leaving_building.field_B = -2;
        s.leaving_building.dest = 0;
      }
      break;
    default:
      break;
  }
}

void
serf_t::clear_destination2(unsigned int dest) {
  switch (state) {
    case SERF_STATE_TRANSPORTING:
      if (s.walking.dest == dest) {
        s.walking.dest = 0;
      }
      break;
    case SERF_STATE_DROP_RESOURCE_OUT:
      if (s.move_resource_out.res_dest == dest) {
        s.move_resource_out.res_dest = 0;
      }
      break;
    case SERF_STATE_LEAVING_BUILDING:
      if (s.leaving_building.dest == dest &&
          s.leaving_building.next_state == SERF_STATE_DROP_RESOURCE_OUT) {
        s.leaving_building.dest = 0;
      }
      break;
    case SERF_STATE_MOVE_RESOURCE_OUT:
      if (s.move_resource_out.res_dest == dest &&
          s.move_resource_out.next_state == SERF_STATE_DROP_RESOURCE_OUT) {
        s.move_resource_out.res_dest = 0;
      }
      break;
    default:
      break;
  }
}

bool
serf_t::idle_to_wait_state(map_pos_t pos_) {
  if (pos == pos_ &&
      (get_state() == SERF_STATE_IDLE_ON_PATH ||
       get_state() == SERF_STATE_WAIT_IDLE_ON_PATH ||
       get_state() == SERF_STATE_WAKE_AT_FLAG ||
       get_state() == SERF_STATE_WAKE_ON_PATH)) {
    serf_log_state_change(this, SERF_STATE_WAKE_AT_FLAG);
    state = SERF_STATE_WAKE_AT_FLAG;
    return true;
  }
  return false;
}

int
serf_t::get_delivery() const {
  int res = 0;

  switch (state) {
    case SERF_STATE_DELIVERING:
    case SERF_STATE_TRANSPORTING:
      res = s.walking.res;
      break;
    case SERF_STATE_ENTERING_BUILDING:
      res = s.entering_building.field_B;
      break;
    case SERF_STATE_LEAVING_BUILDING:
      res = s.leaving_building.field_B;
      break;
    case SERF_STATE_READY_TO_ENTER:
      res = s.ready_to_enter.field_B;
      break;
    case SERF_STATE_MOVE_RESOURCE_OUT:
    case SERF_STATE_DROP_RESOURCE_OUT:
      res = s.move_resource_out.res;
      break;

    default:
      break;
  }

  return res;
}

serf_t*
serf_t::extract_last_knight_from_list() {
  int serf_index = index;
  int *def_index = &serf_index;
  serf_t *def_serf = game->get_serf(*def_index);
  while (def_serf->s.defending.next_knight != 0) {
    def_index = &def_serf->s.defending.next_knight;
    def_serf = game->get_serf(*def_index);
  }
  *def_index = 0;

  return def_serf;
}

void
serf_t::insert_before(serf_t *knight) {
  s.defending.next_knight = knight->get_index();
}

void
serf_t::go_out_from_inventory(unsigned int inventory,
                              map_pos_t dest, int dir) {
  serf_log_state_change(this, SERF_STATE_READY_TO_LEAVE_INVENTORY);
  state = SERF_STATE_READY_TO_LEAVE_INVENTORY;
  s.ready_to_leave_inventory.mode = dir;
  s.ready_to_leave_inventory.dest = dest;
  s.ready_to_leave_inventory.inv_index = inventory;
}

void
serf_t::send_off_to_fight(int dist_col, int dist_row) {
  /* Send this serf off to fight. */
  serf_log_state_change(this, SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT);
  state = SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT;
  s.leave_for_walk_to_fight.dist_col = dist_col;
  s.leave_for_walk_to_fight.dist_row = dist_row;
  s.leave_for_walk_to_fight.field_D = 0;
  s.leave_for_walk_to_fight.field_E = 0;
  s.leave_for_walk_to_fight.next_state = SERF_STATE_KNIGHT_FREE_WALKING;
}

void
serf_t::stay_idle_in_stock(unsigned int inventory) {
  serf_log_state_change(this, SERF_STATE_IDLE_IN_STOCK);
  state = SERF_STATE_IDLE_IN_STOCK;
  s.idle_in_stock.inv_index = inventory;
}

void
serf_t::go_out_from_building(map_pos_t dest, int dir, int field_B) {
  serf_log_state_change(this, SERF_STATE_READY_TO_LEAVE);
  state = SERF_STATE_READY_TO_LEAVE;
  s.leaving_building.field_B = field_B;
  s.leaving_building.dest = dest;
  s.leaving_building.dir = dir;
  s.leaving_building.next_state = SERF_STATE_WALKING;
}

/* Change serf state to lost, but make necessary clean up
   from any earlier state first. */
void
serf_t::set_lost_state() {
  if (state == SERF_STATE_WALKING) {
    if (s.walking.res >= 0) {
      if (s.walking.res != 6) {
        dir_t dir = (dir_t)s.walking.res;
        flag_t *flag = game->get_flag(s.walking.dest);
        flag->cancel_serf_request(dir);

        dir_t other_dir = flag->get_other_end_dir(dir);
        flag->get_other_end_flag(dir)->cancel_serf_request(other_dir);
      }
    } else if (s.walking.res == -1) {
      flag_t *flag = game->get_flag(s.walking.dest);
      building_t *building = flag->get_building();

      if (building->serf_requested()) {
        building->serf_request_failed();
      } else if (!building->has_inventory()) {
        building->decrease_requested_for_stock(0);
      }
    }

    serf_log_state_change(this, SERF_STATE_LOST);
    state = SERF_STATE_LOST;
    s.lost.field_B = 0;
  } else if (state == SERF_STATE_TRANSPORTING ||
       state == SERF_STATE_DELIVERING) {
    if (s.walking.res != 0) {
      int res = s.walking.res-1;
      int dest = s.walking.dest;

      game->cancel_transported_resource((resource_type_t)res, dest);
      game->lose_resource((resource_type_t)res);
    }

    if (get_type() != SERF_SAILOR) {
      serf_log_state_change(this, SERF_STATE_LOST);
      state = SERF_STATE_LOST;
      s.lost.field_B = 0;
    } else {
      serf_log_state_change(this, SERF_STATE_LOST_SAILOR);
      state = SERF_STATE_LOST_SAILOR;
    }
  } else {
    serf_log_state_change(this, SERF_STATE_LOST);
    state = SERF_STATE_LOST;
    s.lost.field_B = 0;
  }
}

/* Return true if serf is waiting for a position to be available.
   In this case, dir will be set to the desired direction of the serf,
   or DIR_NONE if the desired direction cannot be determined. */
int
serf_t::is_waiting(dir_t *dir) {
  const int dir_from_offset[] = {
    DIR_UP_LEFT, DIR_UP, -1,
    DIR_LEFT, -1, DIR_RIGHT,
    -1, DIR_DOWN, DIR_DOWN_RIGHT
  };

  if ((state == SERF_STATE_TRANSPORTING ||
       state == SERF_STATE_WALKING ||
       state == SERF_STATE_DELIVERING) &&
      s.walking.dir < 0) {
    *dir = (dir_t)(s.walking.dir + 6);
    return 1;
  } else if ((state == SERF_STATE_FREE_WALKING ||
        state == SERF_STATE_KNIGHT_FREE_WALKING ||
        state == SERF_STATE_STONECUTTER_FREE_WALKING) &&
       animation == 82) {
    int dx = s.free_walking.dist1;
    int dy = s.free_walking.dist2;

    if (abs(dx) <= 1 && abs(dy) <= 1 &&
        dir_from_offset[(dx+1) + 3*(dy+1)] > -1) {
      *dir = (dir_t)dir_from_offset[(dx+1) + 3*(dy+1)];
    } else {
      *dir = DIR_NONE;
    }
    return 1;
  } else if (state == SERF_STATE_DIGGING &&
       s.digging.substate < 0) {
    int d = s.digging.dig_pos;
    *dir = (dir_t)((d == 0) ? DIR_UP : 6-d);
    return 1;
  }

  return 0;
}

/* Signal waiting serf that it is possible to move in direction
   while switching position with another serf. Returns 0 if the
   switch is not acceptable. */
int
serf_t::switch_waiting(dir_t dir) {
  if ((state == SERF_STATE_TRANSPORTING ||
       state == SERF_STATE_WALKING ||
       state == SERF_STATE_DELIVERING) &&
      s.walking.dir < 0) {
    s.walking.dir = DIR_REVERSE(dir);
    return 1;
  } else if ((state == SERF_STATE_FREE_WALKING ||
        state == SERF_STATE_KNIGHT_FREE_WALKING ||
        state == SERF_STATE_STONECUTTER_FREE_WALKING) &&
       animation == 82) {
    int dx = ((dir < 3) ? 1 : -1)*((dir % 3) < 2);
    int dy = ((dir < 3) ? 1 : -1)*((dir % 3) > 0);

    s.free_walking.dist1 -= dx;
    s.free_walking.dist2 -= dy;

    if (s.free_walking.dist1 == 0 &&
        s.free_walking.dist2 == 0) {
      /* Arriving to destination */
      s.free_walking.flags = BIT(3);
    }
    return 1;
  } else if (state == SERF_STATE_DIGGING &&
       s.digging.substate < 0) {
    return 0;
  }

  return 0;
}

int
serf_t::train_knight(int p) {
  int delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    if (game->random_int() < p) {
      /* Level up */
      serf_type_t old_type = get_type();
      set_type((serf_type_t)(old_type + 1));
      counter = 6000;
      return 0;
    }
    counter += 6000;
  }

  return -1;
}

void
serf_t::handle_serf_idle_in_stock_state() {
  inventory_t *inventory = game->get_inventory(s.idle_in_stock.inv_index);

  if (inventory->get_serf_mode() == 0
      || inventory->get_serf_mode() == 1 /* in, stop */
      || inventory->get_serf_queue_length() >= 3) {
    switch (get_type()) {
      case SERF_KNIGHT_0:
        inventory->knight_training(this, 4000);
        break;
      case SERF_KNIGHT_1:
        inventory->knight_training(this, 2000);
        break;
      case SERF_KNIGHT_2:
        inventory->knight_training(this, 1000);
        break;
      case SERF_KNIGHT_3:
        inventory->knight_training(this, 500);
        break;
      case SERF_SMELTER: /* TODO ??? */
        break;
      default:
        inventory->serf_idle_in_stock(this);
        break;
    }
  } else { /* out */
    inventory->call_out_serf(this);

    serf_log_state_change(this, SERF_STATE_READY_TO_LEAVE_INVENTORY);
    state = SERF_STATE_READY_TO_LEAVE_INVENTORY;
    s.ready_to_leave_inventory.mode = -3;
    s.ready_to_leave_inventory.inv_index = inventory->get_index();
    /* TODO immediate switch to next state. */
  }
}

int
serf_t::get_walking_animation(int h_diff, dir_t dir, int switch_pos) {
  int d = dir;
  if (switch_pos && d < 3) d += 6;
  return 4 + h_diff + 9*d;
}

/* Preconditon: serf is in WALKING or TRANSPORTING state */
void
serf_t::change_direction(dir_t dir, int alt_end) {
  map_pos_t new_pos = game->get_map()->move(pos, dir);

  if (game->get_map()->get_serf_index(new_pos) == 0) {
    /* Change direction, not occupied. */
    game->get_map()->set_serf_index(pos, 0);
    animation = get_walking_animation(game->get_map()->get_height(new_pos) -
                                   game->get_map()->get_height(pos), (dir_t)dir,
                                      0);
    s.walking.dir = DIR_REVERSE(dir);
  } else {
    /* Direction is occupied. */
    serf_t *other_serf = game->get_serf_at_pos(new_pos);
    dir_t other_dir;

    if (other_serf->is_waiting(&other_dir) &&
        (other_dir == DIR_REVERSE(dir) || other_dir == DIR_NONE) &&
        other_serf->switch_waiting(DIR_REVERSE(dir))) {
      /* Do the switch */
      other_serf->pos = pos;
      game->get_map()->set_serf_index(other_serf->pos, other_serf->get_index());
      other_serf->animation =
           get_walking_animation(game->get_map()->get_height(other_serf->pos) -
                                 game->get_map()->get_height(new_pos),
                                 DIR_REVERSE(dir), 1);
      other_serf->counter = counter_from_animation[other_serf->animation];

      animation = get_walking_animation(game->get_map()->get_height(new_pos) -
                                        game->get_map()->get_height(pos),
                                        (dir_t)dir, 1);
      s.walking.dir = DIR_REVERSE(dir);
    } else {
      /* Wait for other serf */
      animation = 81 + dir;
      counter = counter_from_animation[animation];
      s.walking.dir = dir-6;
      return;
    }
  }

  if (!alt_end) s.walking.wait_counter = 0;
  pos = new_pos;
  game->get_map()->set_serf_index(pos, get_index());
  counter += counter_from_animation[animation];
  if (alt_end && counter < 0) {
    if (game->get_map()->has_flag(new_pos)) {
      counter = 0;
    } else {
      LOGD("serf", "unhandled jump to 31B82.");
    }
  }
}

/* Precondition: serf state is in WALKING or TRANSPORTING state */
void
serf_t::transporter_move_to_flag(flag_t *flag) {
  dir_t dir = (dir_t)s.walking.dir;
  if (flag->is_scheduled((dir_t)dir)) {
    /* Fetch resource from flag */
    s.walking.wait_counter = 0;
    int res_index = flag->scheduled_slot((dir_t)dir);

    if (s.walking.res == 0) {
      /* Pick up resource. */
      resource_type_t res;
      flag->pick_up_resource(res_index, &res, &s.walking.dest);
      s.walking.res = res + 1;
    } else {
      /* Switch resources and destination. */
      resource_type_t temp_res = (resource_type_t)(s.walking.res-1);
      int temp_dest = s.walking.dest;

      resource_type_t pick_res;
      flag->pick_up_resource(res_index, &pick_res, &s.walking.dest);
      s.walking.res = pick_res + 1;

      flag->drop_resource(temp_res, temp_dest);
    }

    /* Find next resource to be picked up */
    player_t *player = game->get_player(get_player());
    flag->prioritize_pickup((dir_t)dir, player);
  } else if (s.walking.res != 0) {
    /* Drop resource at flag */
    if (flag->drop_resource((resource_type_t)(s.walking.res-1),
                            s.walking.dest)) {
      s.walking.res = 0;
    }
  }

  change_direction(dir, 1);
}

bool
serf_t::handle_serf_walking_state_search_cb(flag_t *flag, void *data) {
  serf_t *serf = static_cast<serf_t*>(data);
  flag_t *dest = flag->get_game()->get_flag(serf->s.walking.dest);
  if (flag == dest) {
    LOGV("serf", " dest found: %i.", dest->get_search_dir());
    serf->change_direction(dest->get_search_dir(), 0);
    return 1;
  }

  return 0;
}

void
serf_t::start_walking(dir_t dir, int slope, int change_pos) {
  map_pos_t new_pos = game->get_map()->move(pos, dir);
  animation = get_walking_animation(game->get_map()->get_height(new_pos) -
                                    game->get_map()->get_height(pos), dir, 0);
  counter += (slope * counter_from_animation[animation]) >> 5;

  if (change_pos) {
    game->get_map()->set_serf_index(pos, 0);
    game->get_map()->set_serf_index(new_pos, get_index());
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
serf_t::enter_building(int field_B, int join_pos) {
  serf_log_state_change(this, SERF_STATE_ENTERING_BUILDING);
  state = SERF_STATE_ENTERING_BUILDING;

  start_walking(DIR_UP_LEFT, 32, !join_pos);
  if (join_pos) game->get_map()->set_serf_index(pos, get_index());

  building_t *building = game->get_building_at_pos(pos);
  int slope = road_building_slope[building->get_type()];
  if (!building->is_done()) slope = 1;
  s.entering_building.slope_len = (slope * counter) >> 5;
  s.entering_building.field_B = field_B;
}

/* Start leaving building by switching to LEAVING BUILDING and
   setting appropriate state. */
void
serf_t::leave_building(int join_pos) {
  building_t *building = game->get_building_at_pos(pos);
  int slope = 31 - road_building_slope[building->get_type()];
  if (!building->is_done()) slope = 30;

  if (join_pos) game->get_map()->set_serf_index(pos, 0);
  start_walking(DIR_DOWN_RIGHT, slope, !join_pos);

  serf_log_state_change(this, SERF_STATE_LEAVING_BUILDING);
  state = SERF_STATE_LEAVING_BUILDING;
}

void
serf_t::handle_serf_walking_state_dest_reached() {
  /* Destination reached. */
  if (s.walking.res < 0) {
    building_t *building =
                  game->get_building_at_pos(game->get_map()->move_up_left(pos));
    building->serf_arrive();
    if (building->serf_requested()) building->set_main_serf(get_index());
    building->serf_request_complete();

    if (game->get_map()->get_serf_index(game->get_map()->move_up_left(pos)) !=
        0) {
      animation = 85;
      counter = 0;
      serf_log_state_change(this, SERF_STATE_READY_TO_ENTER);
      state = SERF_STATE_READY_TO_ENTER;
    } else {
      enter_building(s.walking.res, 0);
    }
  } else if (s.walking.res == 6) {
    serf_log_state_change(this, SERF_STATE_LOOKING_FOR_GEO_SPOT);
    state = SERF_STATE_LOOKING_FOR_GEO_SPOT;
    counter = 0;
  } else {
    flag_t *flag = game->get_flag_at_pos(pos);
    dir_t dir = (dir_t)s.walking.res;
    flag_t *other_flag = flag->get_other_end_flag(dir);
    dir_t other_dir = flag->get_other_end_dir(dir);

    /* Increment transport serf count */
    flag->complete_serf_request(dir);
    other_flag->complete_serf_request(other_dir);

    serf_log_state_change(this, SERF_STATE_TRANSPORTING);
    state = SERF_STATE_TRANSPORTING;
    s.walking.dir = dir;
    s.walking.res = 0;
    s.walking.wait_counter = 0;

    transporter_move_to_flag(flag);
  }
}

void
serf_t::handle_serf_walking_state_waiting() {
  /* Waiting for other serf. */
  dir_t dir = (dir_t)(s.walking.dir + 6);

  /* Only check for loops once in a while. */
  s.walking.wait_counter += 1;
  if ((!game->get_map()->has_flag(pos) && s.walking.wait_counter >= 10) ||
      s.walking.wait_counter >= 50) {
    map_pos_t pos_ = pos;

    /* Follow the chain of serfs waiting for each other and
       see if there is a loop. */
    for (int i = 0; i < 100; i++) {
      pos_ = game->get_map()->move(pos_, dir);

      if (game->get_map()->get_serf_index(pos_) == 0) {
        break;
      } else if (game->get_map()->get_serf_index(pos_) == index) {
        /* We have found a loop, try a different direction. */
        change_direction(DIR_REVERSE(dir), 0);
        return;
      }

      /* Get next serf and follow the chain */
      serf_t *other_serf = game->get_serf_at_pos(pos);
      if (other_serf->state != SERF_STATE_WALKING &&
          other_serf->state != SERF_STATE_TRANSPORTING) {
        break;
      }

      if (other_serf->s.walking.dir >= 0 ||
          (other_serf->s.walking.dir + 6) == DIR_REVERSE(dir)) {
        break;
      }

      dir = (dir_t)(other_serf->s.walking.dir + 6);
    }
  }

  /* Stick to the same direction */
  s.walking.wait_counter = 0;
  change_direction((dir_t)(s.walking.dir + 6), 0);
}

void
serf_t::handle_serf_walking_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    if (s.walking.dir < 0) {
      handle_serf_walking_state_waiting();
      continue;
    }

    /* 301F0 */
    if (game->get_map()->has_flag(pos)) {
      /* Serf has reached a flag.
         Search for a destination if none is known. */
      if (s.walking.dest == 0) {
        int flag_index = game->get_map()->get_obj_index(pos);
        flag_t *src = game->get_flag(flag_index);
        int r = src->find_nearest_inventory_for_serf();
        if (r < 0) {
          serf_log_state_change(this, SERF_STATE_LOST);
          state = SERF_STATE_LOST;
          s.lost.field_B = 1;
          counter = 0;
          return;
        }
        s.walking.dest = r;
      }

      /* Check whether destination has been reached.
         If not, find out which direction to move in
         to reach the destination. */
      if (s.walking.dest == game->get_map()->get_obj_index(pos)) {
        handle_serf_walking_state_dest_reached();
        return;
      } else {
        flag_t *src = game->get_flag_at_pos(pos);
        flag_search_t search(game);
        for (int i = 0; i < 6; i++) {
          if (!src->is_water_path((dir_t)(5-i))) {
            flag_t *other_flag = src->get_other_end_flag((dir_t)(5-i));
            other_flag->set_search_dir((dir_t)(5-i));
            search.add_source(other_flag);
          }
        }
        bool r = search.execute(handle_serf_walking_state_search_cb,
                                true, false, this);
        if (r) continue;
      }
    } else {
      /* 30A37 */
      /* Serf is not at a flag. Just follow the road. */
      int paths = game->get_map()->paths(pos) & ~BIT(s.walking.dir);
      dir_t dir = DIR_NONE;
      for (int d = 0; d < 6; d++) {
        if (paths == BIT(d)) {
          dir = (dir_t)d;
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
    if (s.walking.res < 0) {
      if (s.walking.res < -1) {
        serf_log_state_change(this, SERF_STATE_LOST);
        state = SERF_STATE_LOST;
        s.lost.field_B = 1;
        counter = 0;
        return;
      }

      flag_t *flag = game->get_flag(s.walking.dest);
      building_t *building = flag->get_building();

      building->serf_request_failed();
      if (!building->has_inventory()) building->decrease_requested_for_stock(0);
    } else if (s.walking.res != 6) {
      flag_t *flag = game->get_flag(s.walking.dest);
      dir_t d = (dir_t)s.walking.res;
      flag->cancel_serf_request(d);
      flag->get_other_end_flag(d)->cancel_serf_request(
                                     flag->get_other_end_dir(d));
    }

    s.walking.res = -2;
    s.walking.dest = 0;
    counter = 0;
  }
}

void
serf_t::handle_serf_transporting_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter >= 0) return;

  if (s.walking.dir < 0) {
    change_direction((dir_t)(s.walking.dir+6), 1);
  } else {
    /* 31549 */
    if (game->get_map()->has_flag(pos)) {
      /* Current position occupied by waiting transporter */
      if (s.walking.wait_counter < 0) {
        serf_log_state_change(this, SERF_STATE_WALKING);
        state = SERF_STATE_WALKING;
        s.walking.wait_counter = 0;
        s.walking.res = -2;
        s.walking.dest = 0;
        counter = 0;
        return;
      }

      /* 31590 */
      if (s.walking.res != 0 &&
          game->get_map()->get_obj_index(pos) == s.walking.dest) {
        /* At resource destination */
        serf_log_state_change(this, SERF_STATE_DELIVERING);
        state = SERF_STATE_DELIVERING;
        s.walking.wait_counter = 0;

        map_pos_t new_pos = game->get_map()->move_up_left(pos);
        animation = 3 + game->get_map()->get_height(new_pos) -
                    game->get_map()->get_height(pos) + (DIR_UP_LEFT+6)*9;
        counter = counter_from_animation[animation];
        /* TODO next call is actually into the middle of handle_serf_delivering_state().
           Why is a nice and clean state switch not enough???
           Just ignore this call and we'll be safe, I think... */
        /* handle_serf_delivering_state(serf); */
        return;
      }

      flag_t *flag = game->get_flag_at_pos(pos);
      transporter_move_to_flag(flag);
    } else {
      int paths = game->get_map()->paths(pos) & ~BIT(s.walking.dir);
      dir_t dir = DIR_NONE;
      for (int d = 0; d < 6; d++) {
        if (paths == BIT(d)) {
          dir = (dir_t)d;
          break;
        }
      }

      if (dir < 0) {
        serf_log_state_change(this, SERF_STATE_LOST);
        state = SERF_STATE_LOST;
        counter = 0;
        return;
      }

      if (!game->get_map()->has_flag(game->get_map()->move(pos, dir)) ||
          s.walking.res != 0 ||
          s.walking.wait_counter < 0) {
        change_direction(dir, 1);
        return;
      }

      flag_t *flag = game->get_flag_at_pos(game->get_map()->move(pos, dir));
      dir_t rev_dir = DIR_REVERSE(dir);
      flag_t *other_flag = flag->get_other_end_flag(rev_dir);
      dir_t other_dir = flag->get_other_end_dir(rev_dir);

      if (flag->is_scheduled(rev_dir)) {
        change_direction(dir, 1);
        return;
      }

      animation = 110 + s.walking.dir;
      counter = counter_from_animation[animation];
      s.walking.dir -= 6;

      if (flag->free_transporter_count(rev_dir) > 1) {
        s.walking.wait_counter += 1;
        if (s.walking.wait_counter > 3) {
          flag->transporter_to_serve(rev_dir);
          other_flag->transporter_to_serve(other_dir);
          s.walking.wait_counter = -1;
        }
      } else {
        if (!other_flag->is_scheduled(other_dir)) {
          /* TODO Don't use anim as state var */
          tick = (tick & 0xff00) | (s.walking.dir & 0xff);
          serf_log_state_change(this, SERF_STATE_IDLE_ON_PATH);
          state = SERF_STATE_IDLE_ON_PATH;
          s.idle_on_path.rev_dir = rev_dir;
          s.idle_on_path.flag = flag;
          game->get_map()->set_idle_serf(pos);
          game->get_map()->set_serf_index(pos, 0);
          return;
        }
      }
    }
  }
}

void
serf_t::enter_inventory() {
  game->get_map()->set_serf_index(pos, 0);
  building_t *building = game->get_building_at_pos(pos);
  serf_log_state_change(this, SERF_STATE_IDLE_IN_STOCK);
  state = SERF_STATE_IDLE_IN_STOCK;
  /*serf->s.idle_in_stock.field_B = 0;
    serf->s.idle_in_stock.field_C = 0;*/
  s.idle_in_stock.inv_index = building->get_inventory()->get_index();
}

void
serf_t::handle_serf_entering_building_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter < 0 || counter <= s.entering_building.slope_len) {
    if (game->get_map()->get_obj_index(pos) == 0 ||
        game->get_building_at_pos(pos)->is_burning()) {
      /* Burning */
      serf_log_state_change(this, SERF_STATE_LOST);
      state = SERF_STATE_LOST;
      s.lost.field_B = 0;
      counter = 0;
      return;
    }

    counter = s.entering_building.slope_len;
    switch (get_type()) {
    case SERF_TRANSPORTER:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        game->get_map()->set_serf_index(pos, 0);
        int flag_index =
          game->get_map()->get_obj_index(game->get_map()->move_down_right(pos));
        flag_t *flag = game->get_flag(flag_index);

        /* Mark as inventory accepting resources and serfs. */
        flag->set_has_inventory();
        flag->set_accepts_resources(true);
        flag->set_accepts_serfs(true);

        serf_log_state_change(this, SERF_STATE_WAIT_FOR_RESOURCE_OUT);
        state = SERF_STATE_WAIT_FOR_RESOURCE_OUT;
        counter = 63;
        set_type(SERF_TRANSPORTER_INVENTORY);
      }
      break;
    case SERF_SAILOR:
      enter_inventory();
      break;
    case SERF_DIGGER:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        serf_log_state_change(this, SERF_STATE_DIGGING);
        state = SERF_STATE_DIGGING;
        s.digging.h_index = 15;

        building_t *building = game->get_building_at_pos(pos);
        s.digging.dig_pos = 6;
        s.digging.target_h = building->get_level();
        s.digging.substate = 1;
      }
      break;
    case SERF_BUILDER:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        serf_log_state_change(this, SERF_STATE_BUILDING);
        state = SERF_STATE_BUILDING;
        animation = 98;
        counter = 127;
        s.building.mode = 1;
        s.building.bld_index = game->get_map()->get_obj_index(pos);
        s.building.material_step = 0;

        building_t *building = game->get_building(s.building.bld_index);
        switch (building->get_type()) {
        case BUILDING_STOCK:
        case BUILDING_SAWMILL:
        case BUILDING_TOOLMAKER:
        case BUILDING_FORTRESS:
          s.building.material_step |= BIT(7);
          animation = 100;
          break;
        default:
          break;
        }
      }
      break;
    case SERF_TRANSPORTER_INVENTORY:
      game->get_map()->set_serf_index(pos, 0);
      serf_log_state_change(this, SERF_STATE_WAIT_FOR_RESOURCE_OUT);
      state = SERF_STATE_WAIT_FOR_RESOURCE_OUT;
      counter = 63;
      break;
    case SERF_LUMBERJACK:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        game->get_map()->set_serf_index(pos, 0);
        serf_log_state_change(this, SERF_STATE_PLANNING_LOGGING);
        state = SERF_STATE_PLANNING_LOGGING;
      }
      break;
    case SERF_SAWMILLER:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        game->get_map()->set_serf_index(pos, 0);
        if (s.entering_building.field_B != 0) {
          building_t *building = game->get_building_at_pos(pos);
          int flag_index =
          game->get_map()->get_obj_index(game->get_map()->move_down_right(pos));
          flag_t *flag = game->get_flag(flag_index);
          flag->clear_flags();
          building->stock_init(1, RESOURCE_LUMBER, 8);
        }
        serf_log_state_change(this, SERF_STATE_SAWING);
        state = SERF_STATE_SAWING;
        s.sawing.mode = 0;
      }
      break;
    case SERF_STONECUTTER:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        game->get_map()->set_serf_index(pos, 0);
        serf_log_state_change(this, SERF_STATE_PLANNING_STONECUTTING);
        state = SERF_STATE_PLANNING_STONECUTTING;
      }
      break;
    case SERF_FORESTER:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        game->get_map()->set_serf_index(pos, 0);
        serf_log_state_change(this, SERF_STATE_PLANNING_PLANTING);
        state = SERF_STATE_PLANNING_PLANTING;
      }
      break;
    case SERF_MINER:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        game->get_map()->set_serf_index(pos, 0);
        building_t *building = game->get_building_at_pos(pos);
        building_type_t bld_type = building->get_type();

        if (s.entering_building.field_B != 0) {
          building->start_activity();
          building->stop_playing_sfx();

          flag_t *flag =
                   game->get_flag_at_pos(game->get_map()->move_down_right(pos));
          flag->clear_flags();
          building->stock_init(0, RESOURCE_GROUP_FOOD, 8);
        }

        serf_log_state_change(this, SERF_STATE_MINING);
        state = SERF_STATE_MINING;
        s.mining.substate = 0;
        s.mining.deposit =
          (ground_deposit_t)(4 - (bld_type - BUILDING_STONEMINE));
        /*s.mining.field_C = 0;*/
        s.mining.res = 0;
      }
      break;
    case SERF_SMELTER:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        game->get_map()->set_serf_index(pos, 0);

        building_t *building = game->get_building_at_pos(pos);

        if (s.entering_building.field_B != 0) {
          flag_t *flag =
                   game->get_flag_at_pos(game->get_map()->move_down_right(pos));
          flag->clear_flags();
          building->stock_init(0, RESOURCE_COAL, 8);

          if (building->get_type() == BUILDING_STEELSMELTER) {
            building->stock_init(1, RESOURCE_IRONORE, 8);
          } else {
            building->stock_init(1, RESOURCE_GOLDORE, 8);
          }
        }

        /* Switch to smelting state to begin work. */
        serf_log_state_change(this, SERF_STATE_SMELTING);
        state = SERF_STATE_SMELTING;

        if (building->get_type() == BUILDING_STEELSMELTER) {
          s.smelting.type = 0;
        } else {
          s.smelting.type = -1;
        }

        s.smelting.mode = 0;
      }
      break;
    case SERF_FISHER:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        game->get_map()->set_serf_index(pos, 0);
        serf_log_state_change(this, SERF_STATE_PLANNING_FISHING);
        state = SERF_STATE_PLANNING_FISHING;
      }
      break;
    case SERF_PIGFARMER:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        game->get_map()->set_serf_index(pos, 0);

        if (s.entering_building.field_B != 0) {
          building_t *building = game->get_building_at_pos(pos);
          flag_t *flag =
                   game->get_flag_at_pos(game->get_map()->move_down_right(pos));

          building->set_initial_res_in_stock(1, 1);

          flag->clear_flags();
          building->stock_init(0, RESOURCE_WHEAT, 8);

          serf_log_state_change(this, SERF_STATE_PIGFARMING);
          state = SERF_STATE_PIGFARMING;
          s.pigfarming.mode = 0;
        } else {
          serf_log_state_change(this, SERF_STATE_PIGFARMING);
          state = SERF_STATE_PIGFARMING;
          s.pigfarming.mode = 6;
          counter = 0;
        }
      }
      break;
    case SERF_BUTCHER:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        game->get_map()->set_serf_index(pos, 0);

        if (s.entering_building.field_B != 0) {
          building_t *building = game->get_building_at_pos(pos);
          flag_t *flag =
                   game->get_flag_at_pos(game->get_map()->move_down_right(pos));
          flag->clear_flags();
          building->stock_init(0, RESOURCE_PIG, 8);
        }

        serf_log_state_change(this, SERF_STATE_BUTCHERING);
        state = SERF_STATE_BUTCHERING;
        s.butchering.mode = 0;
      }
      break;
    case SERF_FARMER:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        game->get_map()->set_serf_index(pos, 0);
        serf_log_state_change(this, SERF_STATE_PLANNING_FARMING);
        state = SERF_STATE_PLANNING_FARMING;
      }
      break;
    case SERF_MILLER:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        game->get_map()->set_serf_index(pos, 0);

        if (s.entering_building.field_B != 0) {
          building_t *building = game->get_building_at_pos(pos);
          flag_t *flag =
                   game->get_flag_at_pos(game->get_map()->move_down_right(pos));
          flag->clear_flags();
          building->stock_init(0, RESOURCE_WHEAT, 8);
        }

        serf_log_state_change(this, SERF_STATE_MILLING);
        state = SERF_STATE_MILLING;
        s.milling.mode = 0;
      }
      break;
    case SERF_BAKER:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        game->get_map()->set_serf_index(pos, 0);

        if (s.entering_building.field_B != 0) {
          building_t *building = game->get_building_at_pos(pos);
          flag_t *flag =
                   game->get_flag_at_pos(game->get_map()->move_down_right(pos));
          flag->clear_flags();
          building->stock_init(0, RESOURCE_FLOUR, 8);
        }

        serf_log_state_change(this, SERF_STATE_BAKING);
        state = SERF_STATE_BAKING;
        s.baking.mode = 0;
      }
      break;
    case SERF_BOATBUILDER:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        game->get_map()->set_serf_index(pos, 0);
        if (s.entering_building.field_B != 0) {
          building_t *building = game->get_building_at_pos(pos);
          flag_t *flag =
                   game->get_flag_at_pos(game->get_map()->move_down_right(pos));
          flag->clear_flags();
          building->stock_init(0, RESOURCE_PLANK, 8);
        }

        serf_log_state_change(this, SERF_STATE_BUILDING_BOAT);
        state = SERF_STATE_BUILDING_BOAT;
        s.building_boat.mode = 0;
      }
      break;
    case SERF_TOOLMAKER:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        game->get_map()->set_serf_index(pos, 0);
        if (s.entering_building.field_B != 0) {
          building_t *building = game->get_building_at_pos(pos);
          flag_t *flag =
                   game->get_flag_at_pos(game->get_map()->move_down_right(pos));
          flag->clear_flags();
          building->stock_init(0, RESOURCE_PLANK, 8);
          building->stock_init(1, RESOURCE_STEEL, 8);
        }

        serf_log_state_change(this, SERF_STATE_MAKING_TOOL);
        state = SERF_STATE_MAKING_TOOL;
        s.making_tool.mode = 0;
      }
      break;
    case SERF_WEAPONSMITH:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        game->get_map()->set_serf_index(pos, 0);
        if (s.entering_building.field_B != 0) {
          building_t *building = game->get_building_at_pos(pos);
          flag_t *flag =
                   game->get_flag_at_pos(game->get_map()->move_down_right(pos));
          flag->clear_flags();
          building->stock_init(0, RESOURCE_COAL, 8);
          building->stock_init(1, RESOURCE_STEEL, 8);
        }

        serf_log_state_change(this, SERF_STATE_MAKING_WEAPON);
        state = SERF_STATE_MAKING_WEAPON;
        s.making_weapon.mode = 0;
      }
      break;
    case SERF_GEOLOGIST:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        serf_log_state_change(this, SERF_STATE_LOOKING_FOR_GEO_SPOT);
        state =
          SERF_STATE_LOOKING_FOR_GEO_SPOT; /* TODO Should never be reached */
        counter = 0;
      }
      break;
    case SERF_GENERIC: {
      game->get_map()->set_serf_index(pos, 0);

      building_t *building = game->get_building_at_pos(pos);
      inventory_t *inventory = building->get_inventory();
      assert(inventory);
      inventory->serf_come_back();

      serf_log_state_change(this, SERF_STATE_IDLE_IN_STOCK);
      state = SERF_STATE_IDLE_IN_STOCK;
      s.idle_in_stock.inv_index = inventory->get_index();
      break;
    }
    case SERF_KNIGHT_0:
    case SERF_KNIGHT_1:
    case SERF_KNIGHT_2:
    case SERF_KNIGHT_3:
    case SERF_KNIGHT_4:
      if (s.entering_building.field_B == -2) {
        enter_inventory();
      } else {
        building_t *building = game->get_building_at_pos(pos);
        if (building->is_burning()) {
          serf_log_state_change(this, SERF_STATE_LOST);
          state = SERF_STATE_LOST;
          counter = 0;
        } else {
          game->get_map()->set_serf_index(pos, 0);

          if (building->has_inventory()) {
            serf_log_state_change(this, SERF_STATE_DEFENDING_CASTLE);
            state = SERF_STATE_DEFENDING_CASTLE;
            counter = 6000;

            /* Prepend to knight list */
            s.defending.next_knight = building->get_main_serf();
            building->set_main_serf(get_index());

            game->get_player(building->get_owner())->increase_castle_knights();
            return;
          }

          building->requested_knight_arrived();

          serf_state_t next_state = (serf_state_t)-1;
          switch (building->get_type()) {
            case BUILDING_HUT:
              next_state = SERF_STATE_DEFENDING_HUT;
              break;
            case BUILDING_TOWER:
              next_state = SERF_STATE_DEFENDING_TOWER;
              break;
            case BUILDING_FORTRESS:
              next_state = SERF_STATE_DEFENDING_FORTRESS;
              break;
            default:
              NOT_REACHED();
              break;
          }

          /* Switch to defending state */
          serf_log_state_change(this, next_state);
          state = next_state;
          counter = 6000;

          /* Prepend to knight list */
          s.defending.next_knight = building->get_main_serf();
          building->set_main_serf(get_index());

          /* Test whether building is already occupied by knights */
          if (!building->is_active()) {
            building->start_activity();

            int mil_type = -1;
            int max_gold = -1;
            switch (building->get_type()) {
            case BUILDING_HUT:
              mil_type = 0;
              max_gold = 2;
              break;
            case BUILDING_TOWER:
              mil_type = 1;
              max_gold = 4;
              break;
            case BUILDING_FORTRESS:
              mil_type = 2;
              max_gold = 8;
              break;
            default:
              NOT_REACHED();
              break;
            }

            game->get_player(building->get_owner())->add_notification(6,
                                                       building->get_position(),
                                                                   mil_type);

            flag_t *flag = game->get_flag_at_pos(
                    game->get_map()->move_down_right(building->get_position()));
            flag->clear_flags();
            building->stock_init(1, RESOURCE_GOLDBAR, max_gold);
            game->building_captured(building);
          }
        }
      }
      break;
    default:
      NOT_REACHED();
      break;
    }
  }
}

void
serf_t::handle_serf_leaving_building_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter < 0) {
    counter = 0;
    serf_log_state_change(this, s.leaving_building.next_state);
    state = s.leaving_building.next_state;

    /* Set field_F to 0, do this for individual states if necessary */
    if (state == SERF_STATE_WALKING) {
      int mode = s.leaving_building.field_B;
      unsigned int dest = s.leaving_building.dest;
      s.walking.res = mode;
      s.walking.dest = dest;
      s.walking.wait_counter = 0;
    } else if (state == SERF_STATE_DROP_RESOURCE_OUT) {
      unsigned int res = s.leaving_building.field_B;
      unsigned int res_dest = s.leaving_building.dest;
      s.move_resource_out.res = res;
      s.move_resource_out.res_dest = res_dest;
    } else if (state == SERF_STATE_FREE_WALKING ||
         state == SERF_STATE_KNIGHT_FREE_WALKING ||
         state == SERF_STATE_STONECUTTER_FREE_WALKING) {
      int dist1 = s.leaving_building.field_B;
      int dist2 = s.leaving_building.dest;
      int neg_dist1 = s.leaving_building.dest2;
      int neg_dist2 = s.leaving_building.dir;
      s.free_walking.dist1 = dist1;
      s.free_walking.dist2 = dist2;
      s.free_walking.neg_dist1 = neg_dist1;
      s.free_walking.neg_dist2 = neg_dist2;
      s.free_walking.flags = 0;
    } else if (state == SERF_STATE_KNIGHT_PREPARE_DEFENDING ||
         state == SERF_STATE_SCATTER) {
      /* No state. */
    } else {
      LOGD("serf", "unhandled next state when leaving building.");
    }
  }
}

void
serf_t::handle_serf_ready_to_enter_state() {
  map_pos_t new_pos = game->get_map()->move_up_left(pos);

  if (game->get_map()->get_serf_index(new_pos) != 0) {
    animation = 85;
    counter = 0;
    return;
  }

  enter_building(s.ready_to_enter.field_B, 0);
}

void
serf_t::handle_serf_ready_to_leave_state() {
  tick = game->get_tick();
  counter = 0;

  map_pos_t new_pos = game->get_map()->move_down_right(pos);

  if ((game->get_map()->get_serf_index(pos) != index &&
       game->get_map()->get_serf_index(pos) != 0) ||
      game->get_map()->get_serf_index(new_pos) != 0) {
    animation = 82;
    counter = 0;
    return;
  }

  leave_building(0);
}

void
serf_t::handle_serf_digging_state() {
  const int h_diff[] = {
    -1, 1, -2, 2, -3, 3, -4, 4,
    -5, 5, -6, 6, -7, 7, -8, 8
  };

  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    s.digging.substate -= 1;
    if (s.digging.substate < 0) {
      LOGV("serf", "substate -1: wait for serf.");
      int d = s.digging.dig_pos;
      dir_t dir = (dir_t)((d == 0) ? DIR_UP : 6-d);
      map_pos_t new_pos = game->get_map()->move(pos, dir);

      if (game->get_map()->get_serf_index(new_pos) != 0) {
        serf_t *other_serf =
                       game->get_serf(game->get_map()->get_serf_index(new_pos));
        dir_t other_dir;

        if (other_serf->is_waiting(&other_dir) &&
            other_dir == DIR_REVERSE(dir) &&
            other_serf->switch_waiting(other_dir)) {
          /* Do the switch */
          other_serf->pos = pos;
          game->get_map()->set_serf_index(other_serf->pos,
                                          other_serf->get_index());
          other_serf->animation =
            get_walking_animation(game->get_map()->get_height(other_serf->pos) -
                                  game->get_map()->get_height(new_pos),
                                  DIR_REVERSE(dir), 1);
          other_serf->counter = counter_from_animation[other_serf->animation];

          if (d != 0) {
            animation =
                    get_walking_animation(game->get_map()->get_height(new_pos) -
                                      game->get_map()->get_height(pos), dir, 1);
          } else {
            animation = game->get_map()->get_height(new_pos) -
                        game->get_map()->get_height(pos);
          }
        } else {
          counter = 127;
          s.digging.substate = 0;
          return;
        }
      } else {
        game->get_map()->set_serf_index(pos, 0);
        if (d != 0) {
          animation =
                    get_walking_animation(game->get_map()->get_height(new_pos) -
                                            game->get_map()->get_height(pos),
                                            dir, 0);
        } else {
          animation = game->get_map()->get_height(new_pos) -
                      game->get_map()->get_height(pos);
        }
      }

      game->get_map()->set_serf_index(new_pos, get_index());
      pos = new_pos;
      s.digging.substate = 3;
      counter += counter_from_animation[animation];
    } else if (s.digging.substate == 1) {
      /* 34CD6: Change height, head back to center */
      int h = game->get_map()->get_height(pos);
      h += (s.digging.h_index & 1) ? -1 : 1;
      LOGV("serf", "substate 1: change height %s.",
           (s.digging.h_index & 1) ? "down" : "up");
      game->get_map()->set_height(pos, h);

      if (s.digging.dig_pos == 0) {
        s.digging.substate = 1;
      } else {
        dir_t dir = DIR_REVERSE(6-s.digging.dig_pos);
        start_walking(dir, 32, 1);
      }
    } else if (s.digging.substate > 1) {
      LOGV("serf", "substate 2: dig.");
      /* 34E89 */
      animation = 88 - (s.digging.h_index & 1);
      counter += 383;
    } else {
      /* 34CDC: Looking for a place to dig */
      LOGV("serf", "substate 0: looking for place to dig %i, %i.",
             s.digging.dig_pos, s.digging.h_index);
      do {
        int h = h_diff[s.digging.h_index] + s.digging.target_h;
        if (s.digging.dig_pos >= 0 && h >= 0 && h < 32) {
          if (s.digging.dig_pos == 0) {
            int height = game->get_map()->get_height(pos);
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
            dir_t dir = (dir_t)(6-s.digging.dig_pos);
            map_pos_t new_pos = game->get_map()->move(pos, dir);
            int new_height = game->get_map()->get_height(new_pos);
            if (new_height != h) {
              s.digging.dig_pos -= 1;
              continue;
            }
            LOGV("serf", "  found at: %i.", s.digging.dig_pos);
            /* Digging spot found */
            if (game->get_map()->get_serf_index(new_pos) != 0) {
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
        building_t *building =
                        game->get_building(game->get_map()->get_obj_index(pos));
        building->done_leveling();
        building->serf_gone();
        building->set_main_serf(0);
        serf_log_state_change(this, SERF_STATE_READY_TO_LEAVE);
        state = SERF_STATE_READY_TO_LEAVE;
        s.leaving_building.dest = 0;
        s.leaving_building.field_B = -2;
        s.leaving_building.dir = 0;
        s.leaving_building.next_state = SERF_STATE_WALKING;
        handle_serf_ready_to_leave_state();  // TODO(jonls): why isn't a
                                             // state switch enough?
        return;
      }
    }
  }
}

void
serf_t::handle_serf_building_state() {
  const int material_order[] = {
    0, 0, 0, 0, 0, 4, 0, 0,
    0, 0, 0x38, 2, 8, 2, 8, 4,
    4, 0xc, 0x14, 0x2c, 2, 0x1c, 0x1f0, 4,
    0, 0, 0, 0, 0, 0, 0, 0
  };

  const int construction_progress[] = {
    0, 0, 4096, 4096, 4096, 4096, 4096, 2048,
    4096, 4096, 2048, 1366, 2048, 1366, 2048, 1366,
    2048, 1366, 4096, 4096, 1366, 1024, 4096, 4096,
    2048, 1366, 4096, 2048, 2048, 1366, 2048, 2048,
    4096, 2048, 2048, 1366, 2048, 1366, 2048, 1024,
    4096, 2048, 2048, 1366, 1024, 683, 2048, 1366
  };

  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    building_t *building = game->get_building(s.building.bld_index);
    if (s.building.mode < 0) {
      building_type_t type = building->get_type();
      int frame_finished = !!BIT_TEST(building->get_progress(), 15);
      building->increase_progress(construction_progress[2*type+frame_finished]);

      if (building->get_progress() > 0xffff) {
        building->drop_progress();
        building->serf_gone();
        building->set_main_serf(0);
        building->done_build(); /* Building finished */

        switch (type) {
        case BUILDING_HUT:
        case BUILDING_TOWER:
        case BUILDING_FORTRESS:
          game->calculate_military_flag_state(building);
          break;
        default:
          break;
        }

        flag_t *flag = game->get_flag(building->get_flag_index());

        building->stock_init(0, RESOURCE_NONE, 0);
        building->stock_init(1, RESOURCE_NONE, 0);
        flag->clear_flags();

        /* Update player fields. */
        player_t *player = game->get_player(get_player());
        player->building_built(building);

        counter = 0;

        serf_log_state_change(this, SERF_STATE_FINISHED_BUILDING);
        state = SERF_STATE_FINISHED_BUILDING;
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
serf_t::handle_serf_building_castle_state() {
  int progress_delta = (uint16_t)(game->get_tick() - tick) << 7;
  tick = game->get_tick();

  inventory_t *inventory = game->get_inventory(s.building_castle.inv_index);
  building_t *building = game->get_building(inventory->get_building_index());
  building->increase_progress(progress_delta);

  if (building->get_progress() >= 0x10000) { /* Finished */
    serf_log_state_change(this, SERF_STATE_WAIT_FOR_RESOURCE_OUT);
    state = SERF_STATE_WAIT_FOR_RESOURCE_OUT;
    game->get_map()->set_serf_index(pos, 0);
    building->done_build(); /* Building finished */
    building->set_main_serf(0);
  }
}

void
serf_t::handle_serf_move_resource_out_state() {
  tick = game->get_tick();
  counter = 0;

  if ((game->get_map()->get_serf_index(pos) != get_index() &&
       game->get_map()->get_serf_index(pos) != 0) ||
      game->get_map()->get_serf_index(
                                  game->get_map()->move_down_right(pos)) != 0) {
    /* Occupied by serf, wait */
    animation = 82;
    counter = 0;
    return;
  }

  flag_t *flag = game->get_flag_at_pos(game->get_map()->move_down_right(pos));
  if (!flag->has_empty_slot()) {
    /* All resource slots at flag are occupied, wait */
    animation = 82;
    counter = 0;
    return;
  }

  unsigned int res = s.move_resource_out.res;
  unsigned int res_dest = s.move_resource_out.res_dest;
  serf_state_t next_state = s.move_resource_out.next_state;

  leave_building(0);
  s.leaving_building.next_state = next_state;
  s.leaving_building.field_B = res;
  s.leaving_building.dest = res_dest;
}

void
serf_t::handle_serf_wait_for_resource_out_state() {
  if (counter != 0) {
    uint16_t delta = game->get_tick() - tick;
    tick = game->get_tick();
    counter -= delta;

    if (counter >= 0) return;

    counter = 0;
  }

  unsigned int obj_index = game->get_map()->get_obj_index(pos);
  building_t *building = game->get_building(obj_index);
  inventory_t *inventory = building->get_inventory();
  if (inventory->get_serf_queue_length() > 0 ||
      !inventory->has_resource_in_queue()) {
    return;
  }

  serf_log_state_change(this, SERF_STATE_MOVE_RESOURCE_OUT);
  state = SERF_STATE_MOVE_RESOURCE_OUT;
  resource_type_t res = RESOURCE_NONE;
  int dest = 0;
  inventory->get_resource_from_queue(&res, &dest);
  s.move_resource_out.res = res + 1;
  s.move_resource_out.res_dest = dest;
  s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

  /* why isn't a state switch enough? */
  /*handle_serf_move_resource_out_state(serf);*/
}

void
serf_t::handle_serf_drop_resource_out_state() {
  flag_t *flag = game->get_flag(game->get_map()->get_obj_index(pos));

  bool res = flag->drop_resource((resource_type_t)(s.move_resource_out.res-1),
                                 s.move_resource_out.res_dest);
  assert(res);

  serf_log_state_change(this, SERF_STATE_READY_TO_ENTER);
  state = SERF_STATE_READY_TO_ENTER;
  s.ready_to_enter.field_B = 0;
}

void
serf_t::handle_serf_delivering_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    if (s.walking.wait_counter != 0) {
      serf_log_state_change(this, SERF_STATE_TRANSPORTING);
      state = SERF_STATE_TRANSPORTING;
      s.walking.wait_counter = 0;
      flag_t *flag = game->get_flag(game->get_map()->get_obj_index(pos));
      transporter_move_to_flag(flag);
      return;
    }

    if (s.walking.res != 0) {
      resource_type_t res =
        (resource_type_t)(s.walking.res - 1);  // Offset by one,
                                               // because 0 means none.
      s.walking.res = 0;
      building_t *building = game->get_building(
            game->get_map()->get_obj_index(game->get_map()->move_up_left(pos)));
      if (!building->is_burning()) {
        if (building->has_inventory()) {
          inventory_t *inventory = building->get_inventory();
          inventory->push_resource(res);
        } else {
          if (res == RESOURCE_FISH ||
              res == RESOURCE_MEAT ||
              res == RESOURCE_BREAD) {
            res = RESOURCE_GROUP_FOOD;
          }

          /* Add to building stock */
          building->requested_resource_delivered(res);
        }
      }
    }

    animation = 4 + 9 - (animation - (3 + 10*9));
    s.walking.wait_counter = -s.walking.wait_counter - 1;
    counter += counter_from_animation[animation] >> 1;
  }
}

void
serf_t::handle_serf_ready_to_leave_inventory_state() {
  tick = game->get_tick();
  counter = 0;

  if (game->get_map()->get_serf_index(pos) != 0 ||
      game->get_map()->get_serf_index(game->get_map()->move_down_right(pos)) !=
      0) {
    animation = 82;
    counter = 0;
    return;
  }

  if (s.ready_to_leave_inventory.mode == -1) {
    flag_t *flag = game->get_flag(s.ready_to_leave_inventory.dest);
    if (flag->has_building()) {
      building_t *building = flag->get_building();
      if (game->get_map()->get_serf_index(building->get_position()) != 0) {
        animation = 82;
        counter = 0;
        return;
      }
    }
  }

  inventory_t *inventory =
                      game->get_inventory(s.ready_to_leave_inventory.inv_index);
  inventory->sers_away();

  serf_state_t next_state = SERF_STATE_WALKING;
  if (s.ready_to_leave_inventory.mode == -3) {
    next_state = SERF_STATE_SCATTER;
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
serf_t::drop_resource(resource_type_t res) {
  flag_t *flag = game->get_flag(game->get_map()->get_obj_index(pos));

  /* Resource is lost if no free slot is found */
  bool result = flag->drop_resource(res, 0);
  if (result) {
    player_t *player = game->get_player(get_player());
    player->increase_res_count(res);
  }
}

/* Serf will try to find the closest inventory from current position, either
   by following the roads if it is already at a flag, otherwise it will try
   to find a flag nearby. */
void
serf_t::find_inventory() {
  if (game->get_map()->has_flag(pos)) {
    flag_t *flag = game->get_flag(game->get_map()->get_obj_index(pos));
    if ((flag->land_paths() != 0 ||
         (flag->has_inventory() && flag->accepts_serfs())) &&
         game->get_map()->get_owner(pos) == get_player()) {
      serf_log_state_change(this, SERF_STATE_WALKING);
      state = SERF_STATE_WALKING;
      s.walking.res = -2;
      s.walking.dest = 0;
      s.walking.dir = 0;
      counter = 0;
      return;
    }
  }

  serf_log_state_change(this, SERF_STATE_LOST);
  state = SERF_STATE_LOST;
  s.lost.field_B = 0;
  counter = 0;
}

void
serf_t::handle_serf_free_walking_state_dest_reached() {
  if (s.free_walking.neg_dist1 == -128 &&
      s.free_walking.neg_dist2 < 0) {
    find_inventory();
    return;
  }

  switch (get_type()) {
  case SERF_LUMBERJACK:
    if (s.free_walking.neg_dist1 == -128) {
      if (s.free_walking.neg_dist2 > 0) {
        drop_resource(RESOURCE_LUMBER);
      }

      serf_log_state_change(this, SERF_STATE_READY_TO_ENTER);
      state = SERF_STATE_READY_TO_ENTER;
      s.ready_to_enter.field_B = 0;
      counter = 0;
    } else {
      s.free_walking.dist1 = s.free_walking.neg_dist1;
      s.free_walking.dist2 = s.free_walking.neg_dist2;
      int obj = game->get_map()->get_obj(pos);
      if (obj >= MAP_OBJ_TREE_0 &&
          obj <= MAP_OBJ_PINE_7) {
        serf_log_state_change(this, SERF_STATE_LOGGING);
        state = SERF_STATE_LOGGING;
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
  case SERF_STONECUTTER:
    if (s.free_walking.neg_dist1 == -128) {
      if (s.free_walking.neg_dist2 > 0) {
        drop_resource(RESOURCE_STONE);
      }

      serf_log_state_change(this, SERF_STATE_READY_TO_ENTER);
      state = SERF_STATE_READY_TO_ENTER;
      s.ready_to_enter.field_B = 0;
      counter = 0;
    } else {
      s.free_walking.dist1 = s.free_walking.neg_dist1;
      s.free_walking.dist2 = s.free_walking.neg_dist2;

      map_pos_t new_pos = game->get_map()->move_up_left(pos);
      int obj = game->get_map()->get_obj(new_pos);
      if (game->get_map()->get_serf_index(new_pos) == 0 &&
          obj >= MAP_OBJ_STONE_0 &&
          obj <= MAP_OBJ_STONE_7) {
        counter = 0;
        start_walking(DIR_UP_LEFT, 32, 1);

        serf_log_state_change(this, SERF_STATE_STONECUTTING);
        state = SERF_STATE_STONECUTTING;
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
  case SERF_FORESTER:
    if (s.free_walking.neg_dist1 == -128) {
      serf_log_state_change(this, SERF_STATE_READY_TO_ENTER);
      state = SERF_STATE_READY_TO_ENTER;
      s.ready_to_enter.field_B = 0;
      counter = 0;
    } else {
      s.free_walking.dist1 = s.free_walking.neg_dist1;
      s.free_walking.dist2 = s.free_walking.neg_dist2;
      if (game->get_map()->get_obj(pos) == MAP_OBJ_NONE) {
        serf_log_state_change(this, SERF_STATE_PLANTING);
        state = SERF_STATE_PLANTING;
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
  case SERF_FISHER:
    if (s.free_walking.neg_dist1 == -128) {
      if (s.free_walking.neg_dist2 > 0) {
        drop_resource(RESOURCE_FISH);
      }

      serf_log_state_change(this, SERF_STATE_READY_TO_ENTER);
      state = SERF_STATE_READY_TO_ENTER;
      s.ready_to_enter.field_B = 0;
      counter = 0;
    } else {
      s.free_walking.dist1 = s.free_walking.neg_dist1;
      s.free_walking.dist2 = s.free_walking.neg_dist2;

      int a = -1;
      map_t *map = game->get_map();
      if (map->paths(pos) == 0) {
        if (map->type_down(pos) <= MAP_TERRAIN_WATER_3 &&
            map->type_up(map->move_up_left(pos)) >= MAP_TERRAIN_GRASS_0) {
          a = 132;
        } else if (
            map->type_down(map->move_left(pos)) <= MAP_TERRAIN_WATER_3 &&
            map->type_up(map->move_up(pos)) >= MAP_TERRAIN_GRASS_0) {
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
        serf_log_state_change(this, SERF_STATE_FISHING);
        state = SERF_STATE_FISHING;
        s.free_walking.neg_dist1 = 0;
        s.free_walking.neg_dist2 = 0;
        s.free_walking.flags = 0;
        animation = a;
        counter = counter_from_animation[a];
      }
    }
    break;
  case SERF_FARMER:
    if (s.free_walking.neg_dist1 == -128) {
      if (s.free_walking.neg_dist2 > 0) {
        drop_resource(RESOURCE_WHEAT);
      }

      serf_log_state_change(this, SERF_STATE_READY_TO_ENTER);
      state = SERF_STATE_READY_TO_ENTER;
      s.ready_to_enter.field_B = 0;
      counter = 0;
    } else {
      s.free_walking.dist1 = s.free_walking.neg_dist1;
      s.free_walking.dist2 = s.free_walking.neg_dist2;

      if (game->get_map()->get_obj(pos) == MAP_OBJ_SEEDS_5 ||
          (game->get_map()->get_obj(pos) >= MAP_OBJ_FIELD_0 &&
           game->get_map()->get_obj(pos) <= MAP_OBJ_FIELD_5)) {
        /* Existing field. */
        animation = 136;
        s.free_walking.neg_dist1 = 1;
        counter = counter_from_animation[animation];
      } else if (game->get_map()->get_obj(pos) == MAP_OBJ_NONE &&
                 game->get_map()->paths(pos) == 0) {
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

      serf_log_state_change(this, SERF_STATE_FARMING);
      state = SERF_STATE_FARMING;
      s.free_walking.neg_dist2 = 0;
    }
    break;
  case SERF_GEOLOGIST:
    if (s.free_walking.neg_dist1 == -128) {
      if (game->get_map()->get_obj(pos) == MAP_OBJ_FLAG &&
          game->get_map()->get_owner(pos) == get_player()) {
        serf_log_state_change(this, SERF_STATE_LOOKING_FOR_GEO_SPOT);
        state = SERF_STATE_LOOKING_FOR_GEO_SPOT;
        counter = 0;
      } else {
        serf_log_state_change(this, SERF_STATE_LOST);
        state = SERF_STATE_LOST;
        s.lost.field_B = 0;
        counter = 0;
      }
    } else {
      s.free_walking.dist1 = s.free_walking.neg_dist1;
      s.free_walking.dist2 = s.free_walking.neg_dist2;
      if (game->get_map()->get_obj(pos) == MAP_OBJ_NONE) {
        serf_log_state_change(this, SERF_STATE_SAMPLING_GEO_SPOT);
        state = SERF_STATE_SAMPLING_GEO_SPOT;
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
  case SERF_KNIGHT_0:
  case SERF_KNIGHT_1:
  case SERF_KNIGHT_2:
  case SERF_KNIGHT_3:
  case SERF_KNIGHT_4:
    if (s.free_walking.neg_dist1 == -128) {
      find_inventory();
    } else {
      serf_log_state_change(this, SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING);
      state = SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING;
      counter = 0;
    }
    break;
  default:
    find_inventory();
    break;
  }
}

void
serf_t::handle_serf_free_walking_switch_on_dir(dir_t dir) {
  /* A suitable direction has been found; walk. */
  assert(dir > -1);
  int dx = ((dir < 3) ? 1 : -1)*((dir % 3) < 2);
  int dy = ((dir < 3) ? 1 : -1)*((dir % 3) > 0);

  LOGV("serf", "serf %i: free walking: dest %i, %i, move %i, %i.",
       index,
       s.free_walking.dist1,
       s.free_walking.dist2, dx, dy);

  s.free_walking.dist1 -= dx;
  s.free_walking.dist2 -= dy;

  start_walking(dir, 32, 1);

  if (s.free_walking.dist1 == 0 &&
      s.free_walking.dist2 == 0) {
    /* Arriving to destination */
    s.free_walking.flags = BIT(3);
  }
}

void
serf_t::handle_serf_free_walking_switch_with_other() {
  /* No free position can be found. Switch with
     other serf. */
  map_pos_t new_pos = 0;
  int dir = -1;
  serf_t *other_serf = NULL;
  for (int i = 0; i < 6; i++) {
    new_pos = game->get_map()->move(pos, (dir_t)i);
    if (game->get_map()->get_serf_index(new_pos) != 0) {
      other_serf = game->get_serf(game->get_map()->get_serf_index(new_pos));
      dir_t other_dir;

      if (other_serf->is_waiting(&other_dir) &&
          other_dir == DIR_REVERSE(i) &&
          other_serf->switch_waiting(other_dir)) {
        dir = i;
        break;
      }
    }
  }

  if (dir > -1) {
    int dx = ((dir < 3) ? 1 : -1)*((dir % 3) < 2);
    int dy = ((dir < 3) ? 1 : -1)*((dir % 3) > 0);

    LOGV("serf", "free walking (switch): dest %i, %i, move %i, %i.",
         s.free_walking.dist1,
         s.free_walking.dist2, dx, dy);

    s.free_walking.dist1 -= dx;
    s.free_walking.dist2 -= dy;

    if (s.free_walking.dist1 == 0 &&
        s.free_walking.dist2 == 0) {
      /* Arriving to destination */
      s.free_walking.flags = BIT(3);
    }

    /* Switch with other serf. */
    game->get_map()->set_serf_index(pos, other_serf->index);
    game->get_map()->set_serf_index(new_pos, index);

    other_serf->animation = get_walking_animation(
                              game->get_map()->get_height(pos) -
                              game->get_map()->get_height(other_serf->pos),
                              DIR_REVERSE(dir), 1);
    animation = get_walking_animation(game->get_map()->get_height(new_pos) -
                                      game->get_map()->get_height(pos),
                                      (dir_t)dir, 1);

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
serf_t::can_pass_map_pos(map_pos_t pos) {
  return map_t::map_space_from_obj[game->get_map()->get_obj(pos)] <=
           MAP_SPACE_SEMIPASSABLE;
}

int
serf_t::handle_free_walking_follow_edge() {
  const int dir_from_offset[] = {
    DIR_UP_LEFT, DIR_UP, -1,
    DIR_LEFT, -1, DIR_RIGHT,
    -1, DIR_DOWN, DIR_DOWN_RIGHT
  };

  /* Follow right-hand edge */
  const dir_t dir_right_edge[] = {
    DIR_DOWN, DIR_DOWN_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP_LEFT, DIR_LEFT,
    DIR_LEFT, DIR_DOWN, DIR_DOWN_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP_LEFT,
    DIR_UP_LEFT, DIR_LEFT, DIR_DOWN, DIR_DOWN_RIGHT, DIR_RIGHT, DIR_UP,
    DIR_UP, DIR_UP_LEFT, DIR_LEFT, DIR_DOWN, DIR_DOWN_RIGHT, DIR_RIGHT,
    DIR_RIGHT, DIR_UP, DIR_UP_LEFT, DIR_LEFT, DIR_DOWN, DIR_DOWN_RIGHT,
    DIR_DOWN_RIGHT, DIR_RIGHT, DIR_UP, DIR_UP_LEFT, DIR_LEFT, DIR_DOWN,
  };

  /* Follow left-hand edge */
  const dir_t dir_left_edge[] = {
    DIR_UP_LEFT, DIR_UP, DIR_RIGHT, DIR_DOWN_RIGHT, DIR_DOWN, DIR_LEFT,
    DIR_UP, DIR_RIGHT, DIR_DOWN_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP_LEFT,
    DIR_RIGHT, DIR_DOWN_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP_LEFT, DIR_UP,
    DIR_DOWN_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP_LEFT, DIR_UP, DIR_RIGHT,
    DIR_DOWN, DIR_LEFT, DIR_UP_LEFT, DIR_UP, DIR_RIGHT, DIR_DOWN_RIGHT,
    DIR_LEFT, DIR_UP_LEFT, DIR_UP, DIR_RIGHT, DIR_DOWN_RIGHT, DIR_DOWN,
  };

  int water = (state == SERF_STATE_FREE_SAILING);
  int dir_index = -1;
  const dir_t *dir_arr = NULL;

  if (BIT_TEST(s.free_walking.flags, 3)) {
    /* Follow right-hand edge */
    dir_arr = dir_left_edge;
    dir_index = (s.free_walking.flags & 7)-1;
  } else {
    /* Follow right-hand edge */
    dir_arr = dir_right_edge;
    dir_index = (s.free_walking.flags & 7)-1;
  }

  int d1 = s.free_walking.dist1;
  int d2 = s.free_walking.dist2;

  /* Check if dest is only one step away. */
  if (!water && abs(d1) <= 1 && abs(d2) <= 1 &&
      dir_from_offset[(d1+1) + 3*(d2+1)] > -1) {
    /* Convert offset in two dimensions to
       direction variable. */
    dir_t dir = (dir_t)dir_from_offset[(d1+1) + 3*(d2+1)];
    map_pos_t new_pos = game->get_map()->move(pos, dir);

    if (!can_pass_map_pos(new_pos)) {
      if (state != SERF_STATE_KNIGHT_FREE_WALKING &&
          s.free_walking.neg_dist1 != -128) {
        s.free_walking.dist1 += s.free_walking.neg_dist1;
        s.free_walking.dist2 += s.free_walking.neg_dist2;
        s.free_walking.neg_dist1 = 0;
        s.free_walking.neg_dist2 = 0;
        s.free_walking.flags = 0;
        animation = 82;
        counter = counter_from_animation[animation];
      } else {
        serf_log_state_change(this, SERF_STATE_LOST);
        state = SERF_STATE_LOST;
        s.lost.field_B = 0;
        counter = 0;
      }
      return 0;
    }

    if (state == SERF_STATE_KNIGHT_FREE_WALKING &&
        s.free_walking.neg_dist1 != -128 &&
        game->get_map()->get_serf_index(new_pos) != 0) {
      /* Wait for other serfs */
      s.free_walking.flags = 0;
      animation = 82;
      counter = counter_from_animation[animation];
      return 0;
    }
  }

  const dir_t *a0 = &dir_arr[6*dir_index];
  int i0 = -1;
  dir_t dir = DIR_NONE;
  for (int i = 0; i < 6; i++) {
    map_pos_t new_pos = game->get_map()->move(pos, a0[i]);
    if (((water && game->get_map()->get_obj(new_pos) == 0) ||
         (!water && !game->get_map()->is_in_water(new_pos) &&
          can_pass_map_pos(new_pos))) &&
        game->get_map()->get_serf_index(new_pos) == 0) {
      dir = (dir_t)a0[i];
      i0 = i;
      break;
    }
  }

  if (i0 > -1) {
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
serf_t::handle_free_walking_common() {
  const int dir_from_offset[] = {
    DIR_UP_LEFT, DIR_UP, -1,
    DIR_LEFT, -1, DIR_RIGHT,
    -1, DIR_DOWN, DIR_DOWN_RIGHT
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
  const int dir_forward[] = {
    DIR_UP, DIR_UP_LEFT, DIR_RIGHT, DIR_LEFT, DIR_DOWN_RIGHT, DIR_DOWN,
    DIR_UP_LEFT, DIR_UP, DIR_LEFT, DIR_RIGHT, DIR_DOWN, DIR_DOWN_RIGHT,
    DIR_UP_LEFT, DIR_LEFT, DIR_UP, DIR_DOWN, DIR_RIGHT, DIR_DOWN_RIGHT,
    DIR_LEFT, DIR_UP_LEFT, DIR_DOWN, DIR_UP, DIR_DOWN_RIGHT, DIR_RIGHT,
    DIR_LEFT, DIR_DOWN, DIR_UP_LEFT, DIR_DOWN_RIGHT, DIR_UP, DIR_RIGHT,
    DIR_DOWN, DIR_LEFT, DIR_DOWN_RIGHT, DIR_UP_LEFT, DIR_RIGHT, DIR_UP,
    DIR_DOWN, DIR_DOWN_RIGHT, DIR_LEFT, DIR_RIGHT, DIR_UP_LEFT, DIR_UP,
    DIR_DOWN_RIGHT, DIR_DOWN, DIR_RIGHT, DIR_LEFT, DIR_UP, DIR_UP_LEFT,
    DIR_DOWN_RIGHT, DIR_RIGHT, DIR_DOWN, DIR_UP, DIR_LEFT, DIR_UP_LEFT,
    DIR_RIGHT, DIR_DOWN_RIGHT, DIR_UP, DIR_DOWN, DIR_UP_LEFT, DIR_LEFT,
    DIR_RIGHT, DIR_UP, DIR_DOWN_RIGHT, DIR_UP_LEFT, DIR_DOWN, DIR_LEFT,
    DIR_UP, DIR_RIGHT, DIR_UP_LEFT, DIR_DOWN_RIGHT, DIR_LEFT, DIR_DOWN
  };

  int water = (state == SERF_STATE_FREE_SAILING);

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
  int d1 = s.free_walking.dist1;
  int d2 = s.free_walking.dist2;
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
  const int *a0 = &dir_forward[6*dir_index];
  dir_t dir = (dir_t)a0[0];
  map_pos_t new_pos = game->get_map()->move(pos, dir);
  if (((water && game->get_map()->get_obj(new_pos) == 0) ||
       (!water && !game->get_map()->is_in_water(new_pos) &&
        can_pass_map_pos(new_pos))) &&
      game->get_map()->get_serf_index(new_pos) == 0) {
    handle_serf_free_walking_switch_on_dir(dir);
    return;
  }

  /* Check if dest is only one step away. */
  if (!water && abs(d1) <= 1 && abs(d2) <= 1 &&
      dir_from_offset[(d1+1) + 3*(d2+1)] > -1) {
    /* Convert offset in two dimensions to
       direction variable. */
    dir_t d = (dir_t)dir_from_offset[(d1+1) + 3*(d2+1)];
    map_pos_t new_pos = game->get_map()->move(pos, d);

    if (!can_pass_map_pos(new_pos)) {
      if (state != SERF_STATE_KNIGHT_FREE_WALKING &&
          s.free_walking.neg_dist1 != -128) {
        s.free_walking.dist1 += s.free_walking.neg_dist1;
        s.free_walking.dist2 += s.free_walking.neg_dist2;
        s.free_walking.neg_dist1 = 0;
        s.free_walking.neg_dist2 = 0;
        s.free_walking.flags = 0;
      } else {
        serf_log_state_change(this, SERF_STATE_LOST);
        state = SERF_STATE_LOST;
        s.lost.field_B = 0;
        counter = 0;
      }
      return;
    }

    if (state == SERF_STATE_KNIGHT_FREE_WALKING &&
        s.free_walking.neg_dist1 != -128 &&
        game->get_map()->get_serf_index(new_pos) != 0) {
      serf_t *other_serf =
                       game->get_serf(game->get_map()->get_serf_index(new_pos));
      dir_t other_dir;

      if (other_serf->is_waiting(&other_dir) &&
          (other_dir == DIR_REVERSE(d) || other_dir == DIR_NONE) &&
          other_serf->switch_waiting(DIR_REVERSE(d))) {
        /* Do the switch */
        other_serf->pos = pos;
        game->get_map()->set_serf_index(other_serf->pos,
                                        other_serf->get_index());
        other_serf->animation =
          get_walking_animation(game->get_map()->get_height(other_serf->pos) -
                                game->get_map()->get_height(new_pos),
                                DIR_REVERSE(d), 1);
        other_serf->counter = counter_from_animation[other_serf->animation];

        animation = get_walking_animation(game->get_map()->get_height(new_pos) -
                                        game->get_map()->get_height(pos), d, 1);
        counter = counter_from_animation[animation];

        pos = new_pos;
        game->get_map()->set_serf_index(pos, index);
        return;
      }

      if (other_serf->state == SERF_STATE_WALKING ||
          other_serf->state == SERF_STATE_TRANSPORTING) {
        s.free_walking.neg_dist2 += 1;
        if (s.free_walking.neg_dist2 >= 10) {
          s.free_walking.neg_dist2 = 0;
          if (other_serf->state == SERF_STATE_TRANSPORTING) {
            if (game->get_map()->has_flag(new_pos)) {
              if (other_serf->s.walking.wait_counter != -1) {
                int dir = other_serf->s.walking.dir;
                if (dir < 0) dir += 6;
                LOGD("serf", "TODO remove %i from path",
                     other_serf->get_index());
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
  int i0 = -1;
  for (int i = 0; i < 5; i++) {
    dir = (dir_t)a0[1+i];
    map_pos_t new_pos = game->get_map()->move(pos, dir);
    if (((water && game->get_map()->get_obj(new_pos) == 0) ||
         (!water && !game->get_map()->is_in_water(new_pos) &&
          can_pass_map_pos(new_pos))) &&
        game->get_map()->get_serf_index(new_pos) == 0) {
      i0 = i;
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
serf_t::handle_serf_free_walking_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    handle_free_walking_common();
  }
}

void
serf_t::handle_serf_logging_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    s.free_walking.neg_dist2 += 1;

    int new_obj = -1;
    if (s.free_walking.neg_dist1 != 0) {
      new_obj = MAP_OBJ_FELLED_TREE_0 + s.free_walking.neg_dist2 - 1;
    } else {
      new_obj = MAP_OBJ_FELLED_PINE_0 + s.free_walking.neg_dist2 - 1;
    }

    /* Change map object. */
    game->get_map()->set_object(pos, (map_obj_t)new_obj, -1);

    if (s.free_walking.neg_dist2 < 5) {
      animation = 116 + s.free_walking.neg_dist2;
      counter += counter_from_animation[animation];
    } else {
      serf_log_state_change(this, SERF_STATE_FREE_WALKING);
      state = SERF_STATE_FREE_WALKING;
      counter = 0;
      s.free_walking.neg_dist1 = -128;
      s.free_walking.neg_dist2 = 1;
      s.free_walking.flags = 0;
      return;
    }
  }
}

void
serf_t::handle_serf_planning_logging_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    int index = (game->random_int() & 0x7f) + 1;
    map_pos_t pos_ = game->get_map()->pos_add_spirally(pos, index);
    int obj = game->get_map()->get_obj(pos_);
    if (obj >= MAP_OBJ_TREE_0 && obj <= MAP_OBJ_PINE_7) {
      serf_log_state_change(this, SERF_STATE_READY_TO_LEAVE);
      state = SERF_STATE_READY_TO_LEAVE;
      s.leaving_building.field_B = map_t::get_spiral_pattern()[2*index] - 1;
      s.leaving_building.dest = map_t::get_spiral_pattern()[2*index+1] - 1;
      s.leaving_building.dest2 = -map_t::get_spiral_pattern()[2*index] + 1;
      s.leaving_building.dir = -map_t::get_spiral_pattern()[2*index+1] + 1;
      s.leaving_building.next_state = SERF_STATE_FREE_WALKING;
      LOGV("serf", "planning logging: tree found, dist %i, %i.",
           s.leaving_building.field_B,
           s.leaving_building.dest);
      return;
    }

    counter += 400;
  }
}

void
serf_t::handle_serf_planning_planting_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  map_t *map = game->get_map();
  while (counter < 0) {
    int index = (game->random_int() & 0x7f) + 1;
    map_pos_t pos_ = map->pos_add_spirally(pos, index);
    if (map->paths(pos_) == 0 &&
        map->get_obj(pos_) == MAP_OBJ_NONE &&
        map->type_up(pos_) == MAP_TERRAIN_GRASS_1 &&
        map->type_down(pos_) == MAP_TERRAIN_GRASS_1 &&
        map->type_up(map->move_up_left(pos_)) == MAP_TERRAIN_GRASS_1 &&
        map->type_down(map->move_up_left(pos_)) == MAP_TERRAIN_GRASS_1) {
      serf_log_state_change(this, SERF_STATE_READY_TO_LEAVE);
      state = SERF_STATE_READY_TO_LEAVE;
      s.leaving_building.field_B = map_t::get_spiral_pattern()[2*index] - 1;
      s.leaving_building.dest = map_t::get_spiral_pattern()[2*index+1] - 1;
      s.leaving_building.dest2 = -map_t::get_spiral_pattern()[2*index] + 1;
      s.leaving_building.dir = -map_t::get_spiral_pattern()[2*index+1] + 1;
      s.leaving_building.next_state = SERF_STATE_FREE_WALKING;
      LOGV("serf", "planning planting: free space found, dist %i, %i.",
           s.leaving_building.field_B,
           s.leaving_building.dest);
      return;
    }

    counter += 700;
  }
}

void
serf_t::handle_serf_planting_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    if (s.free_walking.neg_dist2 != 0) {
      serf_log_state_change(this, SERF_STATE_FREE_WALKING);
      state = SERF_STATE_FREE_WALKING;
      s.free_walking.neg_dist1 = -128;
      s.free_walking.neg_dist2 = 0;
      s.free_walking.flags = 0;
      counter = 0;
      return;
    }

    /* Plant a tree */
    animation = 122;
    map_obj_t new_obj = (map_obj_t)(MAP_OBJ_NEW_PINE +
                                    (game->random_int() & 1));

    if (game->get_map()->paths(pos) == 0 &&
        game->get_map()->get_obj(pos) == MAP_OBJ_NONE) {
      game->get_map()->set_object(pos, new_obj, -1);
    }

    s.free_walking.neg_dist2 = -s.free_walking.neg_dist2 - 1;
    counter += 128;
  }
}

void
serf_t::handle_serf_planning_stonecutting() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    int index = (game->random_int() & 0x7f) + 1;
    map_pos_t pos_ = game->get_map()->pos_add_spirally(pos, index);
    int obj = game->get_map()->get_obj(game->get_map()->move_up_left(pos_));
    if (obj >= MAP_OBJ_STONE_0 &&
        obj <= MAP_OBJ_STONE_7 &&
        can_pass_map_pos(pos_)) {
      serf_log_state_change(this, SERF_STATE_READY_TO_LEAVE);
      state = SERF_STATE_READY_TO_LEAVE;
      s.leaving_building.field_B = map_t::get_spiral_pattern()[2*index] - 1;
      s.leaving_building.dest = map_t::get_spiral_pattern()[2*index+1] - 1;
      s.leaving_building.dest2 = -map_t::get_spiral_pattern()[2*index] + 1;
      s.leaving_building.dir = -map_t::get_spiral_pattern()[2*index+1] + 1;
      s.leaving_building.next_state = SERF_STATE_STONECUTTER_FREE_WALKING;
      LOGV("serf", "planning stonecutting: stone found, dist %i, %i.",
           s.leaving_building.field_B,
           s.leaving_building.dest);
      return;
    }

    counter += 100;
  }
}

void
serf_t::handle_stonecutter_free_walking() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    map_pos_t pos_ = game->get_map()->move_up_left(pos);
    if (game->get_map()->get_serf_index(pos) == 0 &&
        game->get_map()->get_obj(pos_) >= MAP_OBJ_STONE_0 &&
        game->get_map()->get_obj(pos_) <= MAP_OBJ_STONE_7) {
      s.free_walking.neg_dist1 += s.free_walking.dist1;
      s.free_walking.neg_dist2 += s.free_walking.dist2;
      s.free_walking.dist1 = 0;
      s.free_walking.dist2 = 0;
      s.free_walking.flags = 8;
    }

    handle_free_walking_common();
  }
}

void
serf_t::handle_serf_stonecutting_state() {
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
      serf_log_state_change(this, SERF_STATE_FREE_WALKING);
      state = SERF_STATE_FREE_WALKING;
      s.free_walking.neg_dist1 = -128;
      s.free_walking.neg_dist2 = 1;
      s.free_walking.flags = 0;
      counter = 0;
      return;
    }

    if (game->get_map()->get_serf_index(
                                  game->get_map()->move_down_right(pos)) != 0) {
      counter = 0;
      return;
    }

    /* Decrement stone quantity or remove entirely if this
       was the last piece. */
    int obj = game->get_map()->get_obj(pos);
    if (obj <= MAP_OBJ_STONE_6) {
      game->get_map()->set_object(pos, (map_obj_t)(obj + 1), -1);
    } else {
      game->get_map()->set_object(pos, MAP_OBJ_NONE, -1);
    }

    counter = 0;
    start_walking(DIR_DOWN_RIGHT, 24, 1);
    tick = game->get_tick();

    s.free_walking.neg_dist1 = 2;
  }
}

void
serf_t::handle_serf_sawing_state() {
  if (s.sawing.mode == 0) {
    building_t *building =
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
    serf_log_state_change(this, SERF_STATE_MOVE_RESOURCE_OUT);
    state = SERF_STATE_MOVE_RESOURCE_OUT;
    s.move_resource_out.res = 1 + RESOURCE_PLANK;
    s.move_resource_out.res_dest = 0;
    s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

    /* Update resource stats. */
    player_t *player = game->get_player(get_player());
    player->increase_res_count(RESOURCE_PLANK);
  }
}

void
serf_t::handle_serf_lost_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    /* Try to find a suitable destination. */
    for (int i = 0; i < 258; i++) {
      int index = (s.lost.field_B == 0) ? 1+i : 258-i;
      map_pos_t dest = game->get_map()->pos_add_spirally(pos, index);

      if (game->get_map()->has_flag(dest)) {
        flag_t *flag = game->get_flag(game->get_map()->get_obj_index(dest));
        if ((flag->land_paths() != 0 ||
             (flag->has_inventory() && flag->accepts_serfs())) &&
            game->get_map()->has_owner(dest) &&
            game->get_map()->get_owner(dest) == get_player()) {
          if (get_type() >= SERF_KNIGHT_0 &&
              get_type() <= SERF_KNIGHT_4) {
            serf_log_state_change(this, SERF_STATE_KNIGHT_FREE_WALKING);
            state = SERF_STATE_KNIGHT_FREE_WALKING;
          } else {
            serf_log_state_change(this, SERF_STATE_FREE_WALKING);
            state = SERF_STATE_FREE_WALKING;
          }

          s.free_walking.dist1 = map_t::get_spiral_pattern()[2*index];
          s.free_walking.dist2 = map_t::get_spiral_pattern()[2*index+1];
          s.free_walking.neg_dist1 = -128;
          s.free_walking.neg_dist2 = -1;
          s.free_walking.flags = 0;
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
      int col = ((r & (size-1)) - (size/2)) & game->get_map()->get_col_mask();
      int row = (((r >> 8) & (size-1)) - (size/2)) &
                game->get_map()->get_row_mask();

      map_pos_t dest = game->get_map()->pos_add(pos,
                                                game->get_map()->pos(col, row));
      if ((game->get_map()->get_obj(dest) == 0 &&
           game->get_map()->get_height(dest) > 0) ||
          (game->get_map()->has_flag(dest) &&
           (game->get_map()->has_owner(dest) &&
            game->get_map()->get_owner(dest) == get_player()))) {
        if (get_type() >= SERF_KNIGHT_0 &&
            get_type() <= SERF_KNIGHT_4) {
          serf_log_state_change(this, SERF_STATE_KNIGHT_FREE_WALKING);
          state = SERF_STATE_KNIGHT_FREE_WALKING;
        } else {
          serf_log_state_change(this, SERF_STATE_FREE_WALKING);
          state = SERF_STATE_FREE_WALKING;
        }

        s.free_walking.dist1 = col;
        s.free_walking.dist2 = row;
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
serf_t::handle_lost_sailor() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    /* Try to find a suitable destination. */
    for (int i = 0; i < 258; i++) {
      map_pos_t dest = game->get_map()->pos_add_spirally(pos, i);

      if (game->get_map()->has_flag(dest)) {
        flag_t *flag = game->get_flag(game->get_map()->get_obj_index(dest));
        if (flag->land_paths() != 0 &&
            game->get_map()->has_owner(dest) &&
            game->get_map()->get_owner(dest) == get_player()) {
          serf_log_state_change(this, SERF_STATE_FREE_SAILING);
          state = SERF_STATE_FREE_SAILING;

          s.free_walking.dist1 = map_t::get_spiral_pattern()[2*i];
          s.free_walking.dist2 = map_t::get_spiral_pattern()[2*i+1];
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
      int col = ((r & 0x1f) - 16) & game->get_map()->get_col_mask();
      int row = (((r >> 8) & 0x1f) - 16) & game->get_map()->get_row_mask();

      map_pos_t dest = game->get_map()->pos_add(pos,
                                                game->get_map()->pos(col, row));
      if (game->get_map()->get_obj(dest) == 0) {
        serf_log_state_change(this, SERF_STATE_FREE_SAILING);
        state = SERF_STATE_FREE_SAILING;

        s.free_walking.dist1 = col;
        s.free_walking.dist2 = row;
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
serf_t::handle_free_sailing() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    if (!game->get_map()->is_in_water(pos)) {
      serf_log_state_change(this, SERF_STATE_LOST);
      state = SERF_STATE_LOST;
      s.lost.field_B = 0;
      return;
    }

    handle_free_walking_common();
  }
}

void
serf_t::handle_serf_escape_building_state() {
  if (game->get_map()->get_serf_index(pos) == 0) {
    game->get_map()->set_serf_index(pos, index);
    animation = 82;
    counter = 0;
    tick = game->get_tick();

    serf_log_state_change(this, SERF_STATE_LOST);
    state = SERF_STATE_LOST;
    s.lost.field_B = 0;
  }
}

void
serf_t::handle_serf_mining_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    building_t *building =
                        game->get_building(game->get_map()->get_obj_index(pos));

    LOGV("serf", "mining substate: %i.", s.mining.substate);
    switch (s.mining.substate) {
    case 0:
    {
      /* There is a small chance that the miner will
         not require food and skip to state 2. */
      int r = game->random_int();
      if ((r & 7) == 0) {
        s.mining.substate = 2;
      } else {
        s.mining.substate = 1;
      }
      counter += 100 + (r & 0x1ff);
    }
    break;
    case 1:
      if (building->use_resource_in_stock(0)) {
        /* Eat the food. */
        s.mining.substate = 3;
        game->get_map()->set_serf_index(pos, index);
        animation = 125;
        counter = counter_from_animation[animation];
      } else {
        game->get_map()->set_serf_index(pos, index);
        animation = 98;
        counter += 256;
        if (counter < 0) counter = 255;
      }
      break;
    case 2:
      s.mining.substate = 3;
      game->get_map()->set_serf_index(pos, index);
      animation = 125;
      counter = counter_from_animation[animation];
      break;
    case 3:
      s.mining.substate = 4;
      building->stop_activity();
      animation = 126;
      counter = 304; /* TODO counter_from_animation[126] == 303 */
      break;
    case 4: {
      building->start_playing_sfx();
      game->get_map()->set_serf_index(pos, 0);
      /* fall through */
    }
    case 5:
    case 6:
    case 7: {
      s.mining.substate += 1;

      /* Look for resource in ground. */
      map_pos_t dest = game->get_map()->pos_add_spirally(pos,
                                  (game->random_int() >> 2) & 0x1f);
      if ((game->get_map()->get_obj(dest) == MAP_OBJ_NONE ||
           game->get_map()->get_obj(dest) > MAP_OBJ_CASTLE) &&
          game->get_map()->get_res_type(dest) == s.mining.deposit &&
          game->get_map()->get_res_amount(dest) > 0) {
        /* Decrement resource count in ground. */
        game->get_map()->remove_ground_deposit(dest, 1);

        /* Hand resource to miner. */
        const resource_type_t res_from_mine_type[] = {
          RESOURCE_GOLDORE, RESOURCE_IRONORE,
          RESOURCE_COAL, RESOURCE_STONE
        };

        s.mining.res = res_from_mine_type[s.mining.deposit-1] + 1;
        s.mining.substate = 8;
      }

      counter += 1000;
      break;
    }
    case 8:
      game->get_map()->set_serf_index(pos, index);
      s.mining.substate = 9;
      building->stop_playing_sfx();
      animation = 127;
      counter = counter_from_animation[animation];
      break;
    case 9:
      s.mining.substate = 10;
      building->start_activity();

      if (building->get_progress() == 0x8000) {
        /* Handle empty mine. */
        player_t *player = game->get_player(get_player());
        if (player->is_ai()) {
          /* TODO Burn building. */
        }

        game->get_player(building->get_owner())->add_notification(4,
                                                       building->get_position(),
                                     building->get_type() - BUILDING_STONEMINE);
      }

      building->increase_mining();
      if (s.mining.res > 0) building->increase_progress(1);

      animation = 128;
      counter = 384; /* TODO counter_from_animation[128] == 383 */
      break;
    case 10:
      game->get_map()->set_serf_index(pos, 0);
      if (s.mining.res == 0) {
        s.mining.substate = 0;
        counter = 0;
      } else {
        unsigned int res = s.mining.res;
        game->get_map()->set_serf_index(pos, 0);

        serf_log_state_change(this, SERF_STATE_MOVE_RESOURCE_OUT);
        state = SERF_STATE_MOVE_RESOURCE_OUT;
        s.move_resource_out.res = res;
        s.move_resource_out.res_dest = 0;
        s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

        /* Update resource stats. */
        player_t *player = game->get_player(get_player());
        player->increase_res_count(res-1);
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
serf_t::handle_serf_smelting_state() {
  building_t *building =
                        game->get_building(game->get_map()->get_obj_index(pos));

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
          res = 1 + RESOURCE_STEEL;
        } else {
          res = 1 + RESOURCE_GOLDBAR;
        }

        serf_log_state_change(this, SERF_STATE_MOVE_RESOURCE_OUT);
        state = SERF_STATE_MOVE_RESOURCE_OUT;

        s.move_resource_out.res = res;
        s.move_resource_out.res_dest = 0;
        s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

        /* Update resource stats. */
        player_t *player = game->get_player(get_player());
        player->increase_res_count(res-1);
        return;
      } else if (s.smelting.counter == 0) {
        game->get_map()->set_serf_index(pos, 0);
      }

      counter += 384;
    }
  }
}

void
serf_t::handle_serf_planning_fishing_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  map_t *map = game->get_map();
  while (counter < 0) {
    int index = ((game->random_int() >> 2) & 0x3f) + 1;
    map_pos_t dest = map->pos_add_spirally(pos, index);

    if (map->get_obj(dest) == MAP_OBJ_NONE &&
        map->paths(dest) == 0 &&
        ((map->type_down(dest) <= MAP_TERRAIN_WATER_3 &&
          map->type_up(map->move_up_left(dest)) >= MAP_TERRAIN_GRASS_0) ||
         (map->type_down(map->move_left(dest)) <= MAP_TERRAIN_WATER_3 &&
          map->type_up(map->move_up(dest)) >= MAP_TERRAIN_GRASS_0))) {
      serf_log_state_change(this, SERF_STATE_READY_TO_LEAVE);
      state = SERF_STATE_READY_TO_LEAVE;
      s.leaving_building.field_B = map_t::get_spiral_pattern()[2*index] - 1;
      s.leaving_building.dest = map_t::get_spiral_pattern()[2*index+1] - 1;
      s.leaving_building.dest2 = -map_t::get_spiral_pattern()[2*index] + 1;
      s.leaving_building.dir = -map_t::get_spiral_pattern()[2*index+1] + 1;
      s.leaving_building.next_state = SERF_STATE_FREE_WALKING;
      LOGV("serf", "planning fishing: lake found, dist %i, %i.",
           s.leaving_building.field_B,
           s.leaving_building.dest);
      return;
    }

    counter += 100;
  }
}

void
serf_t::handle_serf_fishing_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    if (s.free_walking.neg_dist2 != 0 ||
        s.free_walking.flags == 10) {
      /* Stop fishing. Walk back. */
      serf_log_state_change(this, SERF_STATE_FREE_WALKING);
      state = SERF_STATE_FREE_WALKING;
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

    dir_t dir = DIR_NONE;
    if (animation == 131) {
      if (game->get_map()->is_in_water(game->get_map()->move_left(pos))) {
        dir = DIR_LEFT;
      } else {
        dir = DIR_DOWN;
      }
    } else {
      if (game->get_map()->is_in_water(game->get_map()->move_right(pos))) {
        dir = DIR_RIGHT;
      } else {
        dir = DIR_DOWN_RIGHT;
      }
    }

    int res = game->get_map()->get_res_fish(game->get_map()->move(pos, dir));
    if (res > 0 && (game->random_int() & 0x3f) + 4 < res) {
      /* Caught a fish. */
      game->get_map()->remove_fish(game->get_map()->move(pos, dir), 1);
      s.free_walking.neg_dist2 = 1+RESOURCE_FISH;
    }

    s.free_walking.flags += 1;
    animation += 2;
    counter += 128;
  }
}

void
serf_t::handle_serf_planning_farming_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  map_t *map = game->get_map();
  while (counter < 0) {
    int index = ((game->random_int() >> 2) & 0x1f) + 7;
    map_pos_t dest = map->pos_add_spirally(pos, index);

    /* If destination doesn't have an object it must be
       of the correct type and the surrounding spaces
       must not be occupied by large buildings.
       If it _has_ an object it must be an existing field. */
    if ((map->get_obj(dest) == MAP_OBJ_NONE &&
         (map->type_up(dest) == MAP_TERRAIN_GRASS_1 &&
          map->type_down(dest) == MAP_TERRAIN_GRASS_1 &&
          map->paths(dest) == 0 &&
          map->get_obj(map->move_right(dest)) != MAP_OBJ_LARGE_BUILDING &&
          map->get_obj(map->move_right(dest)) != MAP_OBJ_CASTLE &&
          map->get_obj(map->move_down_right(dest)) != MAP_OBJ_LARGE_BUILDING &&
          map->get_obj(map->move_down_right(dest)) != MAP_OBJ_CASTLE &&
          map->get_obj(map->move_down(dest)) != MAP_OBJ_LARGE_BUILDING &&
          map->get_obj(map->move_down(dest)) != MAP_OBJ_CASTLE &&
          map->type_down(map->move_left(dest)) == MAP_TERRAIN_GRASS_1 &&
          map->get_obj(map->move_left(dest)) != MAP_OBJ_LARGE_BUILDING &&
          map->get_obj(map->move_left(dest)) != MAP_OBJ_CASTLE &&
          map->type_up(map->move_up_left(dest)) == MAP_TERRAIN_GRASS_1 &&
          map->type_down(map->move_up_left(dest)) == MAP_TERRAIN_GRASS_1 &&
          map->get_obj(map->move_up_left(dest)) != MAP_OBJ_LARGE_BUILDING &&
          map->get_obj(map->move_up_left(dest)) != MAP_OBJ_CASTLE &&
          map->type_up(map->move_up(dest)) == MAP_TERRAIN_GRASS_1 &&
          map->get_obj(map->move_up(dest)) != MAP_OBJ_LARGE_BUILDING &&
          map->get_obj(map->move_up(dest)) != MAP_OBJ_CASTLE)) ||
        map->get_obj(dest) == MAP_OBJ_SEEDS_5 ||
        (map->get_obj(dest) >= MAP_OBJ_FIELD_0 &&
         map->get_obj(dest) <= MAP_OBJ_FIELD_5)) {
      serf_log_state_change(this, SERF_STATE_READY_TO_LEAVE);
      state = SERF_STATE_READY_TO_LEAVE;
      s.leaving_building.field_B = map_t::get_spiral_pattern()[2*index] - 1;
      s.leaving_building.dest = map_t::get_spiral_pattern()[2*index+1] - 1;
      s.leaving_building.dest2 = -map_t::get_spiral_pattern()[2*index] + 1;
      s.leaving_building.dir = -map_t::get_spiral_pattern()[2*index+1] + 1;
      s.leaving_building.next_state = SERF_STATE_FREE_WALKING;
      LOGV("serf", "planning farming: field spot found, dist %i, %i.",
           s.leaving_building.field_B,
           s.leaving_building.dest);
      return;
    }

    counter += 500;
  }
}

void
serf_t::handle_serf_farming_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter >= 0) return;

  if (s.free_walking.neg_dist1 == 0) {
    /* Sowing. */
    if (game->get_map()->get_obj(pos) == 0 &&
        game->get_map()->paths(pos) == 0) {
      game->get_map()->set_object(pos, MAP_OBJ_SEEDS_0, -1);
    }
  } else {
    /* Harvesting. */
    s.free_walking.neg_dist2 = 1;
    if (game->get_map()->get_obj(pos) == MAP_OBJ_SEEDS_5) {
      game->get_map()->set_object(pos, MAP_OBJ_FIELD_0, -1);
    } else if (game->get_map()->get_obj(pos) == MAP_OBJ_FIELD_5) {
      game->get_map()->set_object(pos, MAP_OBJ_FIELD_EXPIRED, -1);
    } else if (game->get_map()->get_obj(pos) != MAP_OBJ_FIELD_EXPIRED) {
      game->get_map()->set_object(pos,
                                (map_obj_t)(game->get_map()->get_obj(pos) + 1),
                                  -1);
    }
  }

  serf_log_state_change(this, SERF_STATE_FREE_WALKING);
  state = SERF_STATE_FREE_WALKING;
  s.free_walking.neg_dist1 = -128;
  s.free_walking.flags = 0;
  counter = 0;
}

void
serf_t::handle_serf_milling_state() {
  building_t *building =
                        game->get_building(game->get_map()->get_obj_index(pos));

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
        serf_log_state_change(this, SERF_STATE_MOVE_RESOURCE_OUT);
        state = SERF_STATE_MOVE_RESOURCE_OUT;
        s.move_resource_out.res = 1 + RESOURCE_FLOUR;
        s.move_resource_out.res_dest = 0;
        s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

        player_t *player = game->get_player(get_player());
        player->increase_res_count(RESOURCE_FLOUR);
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
serf_t::handle_serf_baking_state() {
  building_t *building =
                        game->get_building(game->get_map()->get_obj_index(pos));

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

        serf_log_state_change(this, SERF_STATE_MOVE_RESOURCE_OUT);
        state = SERF_STATE_MOVE_RESOURCE_OUT;
        s.move_resource_out.res = 1 + RESOURCE_BREAD;
        s.move_resource_out.res_dest = 0;
        s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

        player_t *player = game->get_player(get_player());
        player->increase_res_count(RESOURCE_BREAD);
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
serf_t::handle_serf_pigfarming_state() {
  /* When the serf is present there is also at least one
     pig present and at most eight. */
  const int breeding_prob[] = {
    6000, 8000, 10000, 11000,
    12000, 13000, 14000, 0
  };

  building_t *building =
                        game->get_building(game->get_map()->get_obj_index(pos));

  if (s.pigfarming.mode == 0) {
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

          serf_log_state_change(this, SERF_STATE_MOVE_RESOURCE_OUT);
          state = SERF_STATE_MOVE_RESOURCE_OUT;
          s.move_resource_out.res = 1 + RESOURCE_PIG;
          s.move_resource_out.res_dest = 0;
          s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

          /* Update resource stats. */
          player_t *player = game->get_player(get_player());
          player->increase_res_count(RESOURCE_PIG);
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
serf_t::handle_serf_butchering_state() {
  building_t *building =
                        game->get_building(game->get_map()->get_obj_index(pos));

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

      serf_log_state_change(this, SERF_STATE_MOVE_RESOURCE_OUT);
      state = SERF_STATE_MOVE_RESOURCE_OUT;
      s.move_resource_out.res = 1 + RESOURCE_MEAT;
      s.move_resource_out.res_dest = 0;
      s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

      /* Update resource stats. */
      player_t *player = game->get_player(get_player());
      player->increase_res_count(RESOURCE_MEAT);
    }
  }
}

void
serf_t::handle_serf_making_weapon_state() {
  building_t *building =
                        game->get_building(game->get_map()->get_obj_index(pos));

  if (s.making_weapon.mode == 0) {
    /* One of each resource makes a sword and a shield.
       Bit 3 is set if a sword has been made and a
       shield can be made without more resources. */
    /* TODO Use of this bit overlaps with sfx check bit. */
    if (!building->playing_sfx()) {
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

        resource_type_t res = building->playing_sfx() ? RESOURCE_SHIELD :
                                                        RESOURCE_SWORD;
        if (building->playing_sfx()) {
          building->stop_playing_sfx();
        } else {
          building->start_playing_sfx();
        }

        serf_log_state_change(this, SERF_STATE_MOVE_RESOURCE_OUT);
        state = SERF_STATE_MOVE_RESOURCE_OUT;
        s.move_resource_out.res = 1 + res;
        s.move_resource_out.res_dest = 0;
        s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

        /* Update resource stats. */
        player_t *player = game->get_player(get_player());
        player->increase_res_count(res);
        return;
      } else {
        counter += 576;
      }
    }
  }
}

void
serf_t::handle_serf_making_tool_state() {
  building_t *building =
                        game->get_building(game->get_map()->get_obj_index(pos));

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

        player_t *player = game->get_player(get_player());
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
              res = RESOURCE_SHOVEL + i;
              break;
            }
          }
        } else {
          /* Completely random. */
          res = RESOURCE_SHOVEL + ((9*game->random_int()) >> 16);
        }

        serf_log_state_change(this, SERF_STATE_MOVE_RESOURCE_OUT);
        state = SERF_STATE_MOVE_RESOURCE_OUT;
        s.move_resource_out.res = 1 + res;
        s.move_resource_out.res_dest = 0;
        s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

        /* Update resource stats. */
        player->increase_res_count(res);
        return;
      } else {
        counter += 1536;
      }
    }
  }
}

void
serf_t::handle_serf_building_boat_state() {
  building_t *building =
                        game->get_building(game->get_map()->get_obj_index(pos));

  if (s.building_boat.mode == 0) {
    if (!building->use_resource_in_stock(0)) return;
    building->boat_clear();

    s.building_boat.mode = 1;
    animation = 146;
    counter = counter_from_animation[animation];
    tick = game->get_tick();

    game->get_map()->set_serf_index(pos, index);
  } else {
    uint16_t delta = game->get_tick() - tick;
    tick = game->get_tick();
    counter -= delta;

    while (counter < 0) {
      s.building_boat.mode += 1;
      if (s.building_boat.mode == 9) {
        /* Boat done. */
        map_pos_t new_pos = game->get_map()->move_down_right(pos);
        if (game->get_map()->get_serf_index(new_pos) != 0) {
          /* Wait for flag to be free. */
          s.building_boat.mode -= 1;
          counter = 0;
        } else {
          /* Drop boat at flag. */
          building->boat_clear();
          game->get_map()->set_serf_index(pos, 0);

          serf_log_state_change(this, SERF_STATE_MOVE_RESOURCE_OUT);
          state = SERF_STATE_MOVE_RESOURCE_OUT;
          s.move_resource_out.res = 1 + RESOURCE_BOAT;
          s.move_resource_out.res_dest = 0;
          s.move_resource_out.next_state = SERF_STATE_DROP_RESOURCE_OUT;

          /* Update resource stats. */
          player_t *player = game->get_player(get_player());
          player->increase_res_count(RESOURCE_BOAT);

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
serf_t::handle_serf_looking_for_geo_spot_state() {
  int tries = 2;
  map_t *map = game->get_map();
  for (int i = 0; i < 8; i++) {
    int index = ((game->random_int() >> 2) & 0x3f) + 1;
    map_pos_t dest = map->pos_add_spirally(pos, index);

    int obj = map->get_obj(dest);
    if (obj == MAP_OBJ_NONE) {
      map_terrain_t t1 = map->type_down(dest);
      map_terrain_t t2 = map->type_up(dest);
      map_terrain_t t3 = map->type_down(map->move_up_left(dest));
      map_terrain_t t4 = map->type_up(map->move_up_left(dest));
      if ((t1 >= MAP_TERRAIN_TUNDRA_0 && t1 <= MAP_TERRAIN_SNOW_0) ||
          (t2 >= MAP_TERRAIN_TUNDRA_0 && t2 <= MAP_TERRAIN_SNOW_0) ||
          (t3 >= MAP_TERRAIN_TUNDRA_0 && t3 <= MAP_TERRAIN_SNOW_0) ||
          (t4 >= MAP_TERRAIN_TUNDRA_0 && t4 <= MAP_TERRAIN_SNOW_0)) {
        serf_log_state_change(this, SERF_STATE_FREE_WALKING);
        state = SERF_STATE_FREE_WALKING;
        s.free_walking.dist1 = map_t::get_spiral_pattern()[2*index];
        s.free_walking.dist2 = map_t::get_spiral_pattern()[2*index+1];
        s.free_walking.neg_dist1 = -map_t::get_spiral_pattern()[2*index];
        s.free_walking.neg_dist2 = -map_t::get_spiral_pattern()[2*index+1];
        s.free_walking.flags = 0;
        tick = game->get_tick();
        LOGV("serf", "looking for geo spot: found, dist %i, %i.",
             s.free_walking.dist1,
             s.free_walking.dist2);
        return;
      }
    } else if (obj >= MAP_OBJ_SIGN_LARGE_GOLD &&
         obj <= MAP_OBJ_SIGN_EMPTY) {
      tries -= 1;
      if (tries == 0) break;
    }
  }

  serf_log_state_change(this, SERF_STATE_WALKING);
  state = SERF_STATE_WALKING;
  s.walking.dest = 0;
  s.walking.res = -2;
  s.walking.dir = 0;
  s.walking.wait_counter = 0;
  counter = 0;
}

void
serf_t::handle_serf_sampling_geo_spot_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    if (s.free_walking.neg_dist1 == 0 &&
        game->get_map()->get_obj(pos) == MAP_OBJ_NONE) {
      if (game->get_map()->get_res_type(pos) == GROUND_DEPOSIT_NONE ||
          game->get_map()->get_res_amount(pos) == 0) {
        /* No available resource here. Put empty sign. */
        game->get_map()->set_object(pos, MAP_OBJ_SIGN_EMPTY, -1);
      } else {
        s.free_walking.neg_dist1 = -1;
        animation = 142;

        /* Select small or large sign with the right resource depicted. */
        int obj = MAP_OBJ_SIGN_LARGE_GOLD +
          2*(game->get_map()->get_res_type(pos)-1) +
          (game->get_map()->get_res_amount(pos) < 12 ? 1 : 0);
        game->get_map()->set_object(pos, (map_obj_t)obj, -1);

        /* Check whether a new notification should be posted. */
        int show_notification = 1;
        for (int i = 0; i < 60; i++) {
          map_pos_t pos_ = game->get_map()->pos_add_spirally(pos, 1+i);
          if ((game->get_map()->get_obj(pos_) >> 1) == (obj >> 1)) {
            show_notification = 0;
            break;
          }
        }

        /* Create notification for found resource. */
        if (show_notification) {
          notification_type_t type;
          switch (game->get_map()->get_res_type(pos)) {
            case GROUND_DEPOSIT_COAL:
              type = NOTIFICATION_FOUND_COAL;
              break;
            case GROUND_DEPOSIT_IRON:
              type = NOTIFICATION_FOUND_IRON;
              break;
            case GROUND_DEPOSIT_GOLD:
              type = NOTIFICATION_FOUND_GOLD;
              break;
            case GROUND_DEPOSIT_STONE:
              type = NOTIFICATION_FOUND_STONE;
              break;
            default:
              NOT_REACHED();
          }
          game->get_player(get_player())->add_notification(
            type, pos, game->get_map()->get_res_type(pos)-1);
        }

        counter += 64;
        continue;
      }
    }

    serf_log_state_change(this, SERF_STATE_FREE_WALKING);
    state = SERF_STATE_FREE_WALKING;
    s.free_walking.neg_dist1 = -128;
    s.free_walking.neg_dist2 = 0;
    s.free_walking.flags = 0;
    counter = 0;
  }
}

void
serf_t::handle_serf_knight_engaging_building_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter < 0) {
    map_obj_t obj =
                   game->get_map()->get_obj(game->get_map()->move_up_left(pos));
    if (obj >= MAP_OBJ_SMALL_BUILDING &&
        obj <= MAP_OBJ_CASTLE) {
      building_t *building = game->get_building(game->get_map()->get_obj_index(
                                           game->get_map()->move_up_left(pos)));
      if (building->is_done() &&
          building->is_military() &&
          building->get_owner() != get_player() &&
          building->has_main_serf()) {
        if (building->is_under_attack()) {
          game->get_player(building->get_owner())->add_notification(1,
                                                       building->get_position(),
                                                                 get_player());
        }

        /* Change state of attacking knight */
        counter = 0;
        state = SERF_STATE_KNIGHT_PREPARE_ATTACKING;
        animation = 168;

        serf_t *def_serf = building->call_defender_out();

        s.attacking.def_index = def_serf->get_index();

        /* Change state of defending knight */
        serf_log_state_change(def_serf, SERF_STATE_KNIGHT_LEAVE_FOR_FIGHT);
        def_serf->state = SERF_STATE_KNIGHT_LEAVE_FOR_FIGHT;
        def_serf->s.leaving_building.next_state =
          SERF_STATE_KNIGHT_PREPARE_DEFENDING;
        def_serf->counter = 0;
        return;
      }
    }

    /* No one to defend this building. Occupy it. */
    serf_log_state_change(this, SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING);
    state = SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING;
    animation = 179;
    counter = counter_from_animation[animation];
    tick = game->get_tick();
  }
}

void
serf_t::set_fight_outcome(serf_t *attacker, serf_t *defender) {
  /* Calculate "morale" for attacker. */
  int exp_factor = 1 << (attacker->get_type() - SERF_KNIGHT_0);
  int land_factor = 0x1000;
  if (attacker->get_player() != game->get_map()->get_owner(attacker->pos)) {
    land_factor = game->get_player(attacker->get_player())->get_knight_morale();
  }

  int morale = (0x400*exp_factor * land_factor) >> 16;

  /* Calculate "morale" for defender. */
  int def_exp_factor = 1 << (defender->get_type() - SERF_KNIGHT_0);
  int def_land_factor = 0x1000;
  if (defender->get_player() != game->get_map()->get_owner(defender->pos)) {
    def_land_factor =
                  game->get_player(defender->get_player())->get_knight_morale();
  }

  int def_morale = (0x400*def_exp_factor * def_land_factor) >> 16;

  int player = -1;
  int value = -1;
  serf_type_t type = (serf_type_t)-1;
  int r = ((morale + def_morale)*game->random_int()) >> 16;
  if (r < morale) {
    player = defender->get_player();
    value = def_exp_factor;
    type = defender->get_type();
    attacker->s.attacking.field_C = 1;
    LOGD("serf", "Fight: %i vs %i (%i). Attacker winning.",
         morale, def_morale, r);
  } else {
    player = attacker->get_player();
    value = exp_factor;
    type = attacker->get_type();
    attacker->s.attacking.field_C = 0;
    LOGD("serf", "Fight: %i vs %i (%i). Defender winning.",
         morale, def_morale, r);
  }

  game->get_player(player)->decrease_military_score(value);
  game->get_player(player)->decrease_serf_count(type);
  attacker->s.attacking.field_B = game->random_int() & 0x70;
}

void
serf_t::handle_serf_knight_prepare_attacking() {
  serf_t *def_serf = game->get_serf(s.attacking.def_index);
  if (def_serf->state == SERF_STATE_KNIGHT_PREPARE_DEFENDING) {
    /* Change state of attacker. */
    serf_log_state_change(this, SERF_STATE_KNIGHT_ATTACKING);
    state = SERF_STATE_KNIGHT_ATTACKING;
    counter = 0;
    tick = game->get_tick();

    /* Change state of defender. */
    serf_log_state_change(def_serf, SERF_STATE_KNIGHT_DEFENDING);
    def_serf->state = SERF_STATE_KNIGHT_DEFENDING;
    def_serf->counter = 0;

    set_fight_outcome(this, def_serf);
  }
}

void
serf_t::handle_serf_knight_leave_for_fight_state() {
  tick = game->get_tick();
  counter = 0;

  if (game->get_map()->get_serf_index(pos) == index ||
      game->get_map()->get_serf_index(pos) == 0) {
    leave_building(1);
  }
}

void
serf_t::handle_serf_knight_prepare_defending_state() {
  counter = 0;
  animation = 84;
}

void
serf_t::handle_knight_attacking() {
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

  serf_t *def_serf = game->get_serf(s.attacking.def_index);

  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  def_serf->tick = game->get_tick();
  counter -= delta;
  def_serf->counter = counter;

  while (counter < 0) {
    int move = moves[s.attacking.field_B];
    if (move < 0) {
      if (s.attacking.field_C == 0) {
        /* Defender won. */
        if (state == SERF_STATE_KNIGHT_ATTACKING_FREE) {
          serf_log_state_change(def_serf,
                                SERF_STATE_KNIGHT_DEFENDING_VICTORY_FREE);
          def_serf->state = SERF_STATE_KNIGHT_DEFENDING_VICTORY_FREE;

          def_serf->animation = 180;
          def_serf->counter = 0;

          /* Attacker dies. */
          serf_log_state_change(this, SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE);
          state = SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE;
          animation = 152 + get_type();
          counter = 255;
          set_type(SERF_DEAD);
        } else {
          /* Defender returns to building. */
          def_serf->enter_building(-1, 1);

          /* Attacker dies. */
          serf_log_state_change(this, SERF_STATE_KNIGHT_ATTACKING_DEFEAT);
          state = SERF_STATE_KNIGHT_ATTACKING_DEFEAT;
          animation = 152 + get_type();
          counter = 255;
          set_type(SERF_DEAD);
        }
      } else {
        /* Attacker won. */
        if (state == SERF_STATE_KNIGHT_ATTACKING_FREE) {
          serf_log_state_change(this, SERF_STATE_KNIGHT_ATTACKING_VICTORY_FREE);
          state = SERF_STATE_KNIGHT_ATTACKING_VICTORY_FREE;
          animation = 168;
          counter = 0;

          s.attacking.field_B = def_serf->s.defending_free.field_D;
          s.attacking.field_C = def_serf->s.defending_free.other_dist_col;
          s.attacking.field_D = def_serf->s.defending_free.other_dist_row;
        } else {
          serf_log_state_change(this, SERF_STATE_KNIGHT_ATTACKING_VICTORY);
          state = SERF_STATE_KNIGHT_ATTACKING_VICTORY;
          animation = 168;
          counter = 0;

          int index = game->get_map()->get_obj_index(
                                  game->get_map()->move_up_left(def_serf->pos));
          building_t *building = game->get_building(index);
          building->requested_knight_defeat_on_walk();
        }

        /* Defender dies. */
        def_serf->tick = game->get_tick();
        def_serf->animation = 147 + get_type();
        def_serf->counter = 255;
        set_type(SERF_DEAD);
      }
    } else {
      /* Go to next move in fight sequence. */
      s.attacking.field_B += 1;
      if (s.attacking.field_C == 0) move = 4 - move;
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
serf_t::handle_serf_knight_attacking_victory_state() {
  serf_t *def_serf = game->get_serf(s.attacking.def_index);

  uint16_t delta = game->get_tick() - def_serf->tick;
  def_serf->tick = game->get_tick();
  def_serf->counter -= delta;

  if (def_serf->counter < 0) {
    game->delete_serf(def_serf);
    s.attacking.def_index = 0;

    serf_log_state_change(this, SERF_STATE_KNIGHT_ENGAGING_BUILDING);
    state = SERF_STATE_KNIGHT_ENGAGING_BUILDING;
    tick = game->get_tick();
    counter = 0;
  }
}

void
serf_t::handle_serf_knight_attacking_defeat_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter < 0) {
    game->get_map()->set_serf_index(pos, 0);
    game->delete_serf(this);
  }
}

void
serf_t::handle_knight_occupy_enemy_building() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    map_pos_t pos_ = game->get_map()->move_up_left(pos);
    if (game->get_map()->get_obj(pos_) >= MAP_OBJ_SMALL_BUILDING &&
        game->get_map()->get_obj(pos_) <= MAP_OBJ_CASTLE) {
      building_t *building =
                       game->get_building(game->get_map()->get_obj_index(pos_));
      if (!building->is_burning() &&
          building->is_military()) {
        if (building->get_owner() == get_player()) {
          /* Enter building if there is space. */
          if (building->get_type() != BUILDING_CASTLE) {
            if (building->is_enough_place_for_knight()) {
              /* Enter building */
              enter_building(-1, 0);
              building->knight_occupy();
              return;
            }
          } else {
            enter_building(-2, 0);
            return;
          }
        } else if (!building->has_main_serf()) {
          /* Occupy the building. */
          game->occupy_enemy_building(building, get_player());

          if (building->get_type() == BUILDING_CASTLE) {
            counter = 0;
          } else {
            /* Enter building */
            enter_building(-1, 0);
            building->knight_occupy();
          }
          return;
        } else {
          serf_log_state_change(this, SERF_STATE_KNIGHT_ENGAGING_BUILDING);
          state = SERF_STATE_KNIGHT_ENGAGING_BUILDING;
          animation = 167;
          counter = 191;
          return;
        }
      }
    }

    /* Something is wrong. */
    serf_log_state_change(this, SERF_STATE_LOST);
    state = SERF_STATE_LOST;
    s.lost.field_B = 0;
    counter = 0;
  }
}

void
serf_t::handle_state_knight_free_walking() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    /* Check for enemy knights nearby. */
    for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
      map_pos_t pos_ = game->get_map()->move(pos, (dir_t)d);

      if (game->get_map()->get_serf_index(pos_) != 0) {
        serf_t *other = game->get_serf(game->get_map()->get_serf_index(pos_));
        if (get_player() != other->get_player()) {
          if (other->state == SERF_STATE_KNIGHT_FREE_WALKING) {
            pos = game->get_map()->move_left(pos_);
            if (can_pass_map_pos(pos_)) {
              int dist_col = s.free_walking.dist1;
              int dist_row = s.free_walking.dist2;

              serf_log_state_change(this,
                                    SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE);
              state = SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE;

              s.defending_free.dist_col = dist_col;
              s.defending_free.dist_row = dist_row;
              s.defending_free.other_dist_col = other->s.free_walking.dist1;
              s.defending_free.other_dist_row = other->s.free_walking.dist2;
              s.defending_free.field_D = 1;
              animation = 99;
              counter = 255;

              serf_log_state_change(other,
                                    SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE);
              other->state = SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE;
              other->s.attacking.field_D = d;
              other->s.attacking.def_index = get_index();
              return;
            }
          } else if (other->state == SERF_STATE_WALKING &&
               other->get_type() >= SERF_KNIGHT_0 &&
               other->get_type() <= SERF_KNIGHT_4) {
            pos_ = game->get_map()->move_left(pos_);
            if (can_pass_map_pos(pos_)) {
              int dist_col = s.free_walking.dist1;
              int dist_row = s.free_walking.dist2;

              serf_log_state_change(this,
                                    SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE);
              state = SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE;
              s.defending_free.dist_col = dist_col;
              s.defending_free.dist_row = dist_row;
              s.defending_free.field_D = 0;
              animation = 99;
              counter = 255;

              flag_t *dest = game->get_flag(other->s.walking.dest);
              building_t *building = dest->get_building();
              if (!building->has_inventory()) {
                building->requested_knight_attacking_on_walk();
              }

              serf_log_state_change(other,
                                    SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE);
              other->state = SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE;
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
serf_t::handle_state_knight_engage_defending_free() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) counter += 256;
}

void
serf_t::handle_state_knight_engage_attacking_free() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    serf_log_state_change(this, SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE_JOIN);
    state = SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE_JOIN;
    animation = 167;
    counter += 191;
    return;
  }
}

void
serf_t::handle_state_knight_engage_attacking_free_join() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    serf_log_state_change(this, SERF_STATE_KNIGHT_PREPARE_ATTACKING_FREE);
    state = SERF_STATE_KNIGHT_PREPARE_ATTACKING_FREE;
    animation = 168;
    counter = 0;

    serf_t *other = game->get_serf(s.attacking.def_index);
    map_pos_t other_pos = other->pos;
    serf_log_state_change(other, SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE);
    other->state = SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE;
    other->counter = counter;

    /* Adjust distance to final destination. */
    dir_t d = (dir_t)s.attacking.field_D;
    if (d == DIR_RIGHT || d == DIR_DOWN_RIGHT) {
      other->s.defending_free.dist_col -= 1;
    } else if (d == DIR_LEFT || d == DIR_UP_LEFT) {
      other->s.defending_free.dist_col += 1;
    }

    if (d == DIR_DOWN_RIGHT || d == DIR_DOWN) {
      other->s.defending_free.dist_row -= 1;
    } else if (d == DIR_UP_LEFT || d == DIR_UP) {
      other->s.defending_free.dist_row += 1;
    }

    other->start_walking(d, 32, 0);
    game->get_map()->set_serf_index(other_pos, 0);
    return;
  }
}

void
serf_t::handle_state_knight_prepare_attacking_free() {
  serf_t *other = game->get_serf(s.attacking.def_index);
  if (other->state == SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE_WAIT) {
    serf_log_state_change(this, SERF_STATE_KNIGHT_ATTACKING_FREE);
    state = SERF_STATE_KNIGHT_ATTACKING_FREE;
    counter = 0;

    serf_log_state_change(other, SERF_STATE_KNIGHT_DEFENDING_FREE);
    other->state = SERF_STATE_KNIGHT_DEFENDING_FREE;
    other->counter = 0;

    set_fight_outcome(this, other);
  }
}

void
serf_t::handle_state_knight_prepare_defending_free() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    serf_log_state_change(this, SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE_WAIT);
    state = SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE_WAIT;
    counter = 0;
    return;
  }
}

void
serf_t::handle_knight_attacking_victory_free() {
  serf_t *other = game->get_serf(s.attacking.def_index);

  uint16_t delta = game->get_tick() - other->tick;
  other->tick = game->get_tick();
  other->counter -= delta;

  while (other->counter < 0) {
    game->delete_serf(other);

    int dist_col = s.attacking.field_C;
    int dist_row = s.attacking.field_D;

    serf_log_state_change(this, SERF_STATE_KNIGHT_ATTACKING_FREE_WAIT);
    state = SERF_STATE_KNIGHT_ATTACKING_FREE_WAIT;

    s.free_walking.dist1 = dist_col;
    s.free_walking.dist2 = dist_row;
    s.free_walking.neg_dist1 = 0;
    s.free_walking.neg_dist2 = 0;

    if (s.attacking.field_B != 0) {
      s.free_walking.flags = 1;
    } else {
      s.free_walking.flags = 0;
    }

    animation = 179;
    counter = 127;
    tick = game->get_tick();
    return;
  }
}

void
serf_t::handle_knight_defending_victory_free() {
  animation = 180;
  counter = 0;
}

void
serf_t::handle_serf_knight_attacking_defeat_free_state() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  if (counter < 0) {
    /* Change state of other. */
    serf_t *other = game->get_serf(s.attacking.def_index);
    int dist_col = other->s.defending_free.dist_col;
    int dist_row = other->s.defending_free.dist_row;

    serf_log_state_change(other, SERF_STATE_KNIGHT_FREE_WALKING);
    other->state = SERF_STATE_KNIGHT_FREE_WALKING;

    other->s.free_walking.dist1 = dist_col;
    other->s.free_walking.dist2 = dist_row;
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
serf_t::handle_knight_attacking_free_wait() {
  uint16_t delta = game->get_tick() - tick;
  tick = game->get_tick();
  counter -= delta;

  while (counter < 0) {
    if (s.free_walking.flags != 0) {
      serf_log_state_change(this, SERF_STATE_KNIGHT_FREE_WALKING);
      state = SERF_STATE_KNIGHT_FREE_WALKING;
    } else {
      serf_log_state_change(this, SERF_STATE_LOST);
      state = SERF_STATE_LOST;
    }

    counter = 0;
    return;
  }
}

void
serf_t::handle_serf_state_knight_leave_for_walk_to_fight() {
  tick = game->get_tick();
  counter = 0;

  if (game->get_map()->get_serf_index(pos) != index &&
      game->get_map()->get_serf_index(pos) != 0) {
    animation = 82;
    counter = 0;
    return;
  }

  building_t *building =
                        game->get_building(game->get_map()->get_obj_index(pos));
  map_pos_t new_pos = game->get_map()->move_down_right(pos);

  if (game->get_map()->get_serf_index(new_pos) == 0) {
    /* For clean state change, save the values first. */
    /* TODO maybe knight_leave_for_walk_to_fight can
       share leaving_building state vars. */
    int dist_col = s.leave_for_walk_to_fight.dist_col;
    int dist_row = s.leave_for_walk_to_fight.dist_row;
    int field_D = s.leave_for_walk_to_fight.field_D;
    int field_E = s.leave_for_walk_to_fight.field_E;
    serf_state_t next_state = s.leave_for_walk_to_fight.next_state;

    leave_building(0);
    /* TODO names for leaving_building vars make no sense here. */
    s.leaving_building.field_B = dist_col;
    s.leaving_building.dest = dist_row;
    s.leaving_building.dest2 = field_D;
    s.leaving_building.dir = field_E;
    s.leaving_building.next_state = next_state;
  } else {
    serf_t *other = game->get_serf(game->get_map()->get_serf_index(new_pos));
    if (get_player() == other->get_player()) {
      animation = 82;
      counter = 0;
    } else {
      /* Go back to defending the building. */
      switch (building->get_type()) {
      case BUILDING_HUT:
        serf_log_state_change(this, SERF_STATE_DEFENDING_HUT);
        state = SERF_STATE_DEFENDING_HUT;
        break;
      case BUILDING_TOWER:
        serf_log_state_change(this, SERF_STATE_DEFENDING_TOWER);
        state = SERF_STATE_DEFENDING_TOWER;
        break;
      case BUILDING_FORTRESS:
        serf_log_state_change(this, SERF_STATE_DEFENDING_FORTRESS);
        state = SERF_STATE_DEFENDING_FORTRESS;
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
serf_t::handle_serf_idle_on_path_state() {
  flag_t *flag = s.idle_on_path.flag;
  dir_t rev_dir = s.idle_on_path.rev_dir;

  /* Set walking dir in field_E. */
  if (flag->is_scheduled(rev_dir)) {
    s.idle_on_path.field_E = (tick & 0xff) + 6;
  } else {
    flag_t *other_flag = flag->get_other_end_flag(rev_dir);
    dir_t other_dir = flag->get_other_end_dir((dir_t)rev_dir);
    if (other_flag && other_flag->is_scheduled(other_dir)) {
      s.idle_on_path.field_E = DIR_REVERSE(rev_dir);
    } else {
      return;
    }
  }

  if (game->get_map()->get_serf_index(pos) == 0) {
    game->get_map()->clear_idle_serf(pos);
    game->get_map()->set_serf_index(pos, index);

    int dir = s.idle_on_path.field_E;

    serf_log_state_change(this, SERF_STATE_TRANSPORTING);
    state = SERF_STATE_TRANSPORTING;
    s.walking.res = 0;
    s.walking.wait_counter = 0;
    s.walking.dir = dir;
    tick = game->get_tick();
    counter = 0;
  } else {
    serf_log_state_change(this, SERF_STATE_WAIT_IDLE_ON_PATH);
    state = SERF_STATE_WAIT_IDLE_ON_PATH;
  }
}

void
serf_t::handle_serf_wait_idle_on_path_state() {
  if (game->get_map()->get_serf_index(pos) == 0) {
    /* Duplicate code from handle_serf_idle_on_path_state() */
    game->get_map()->clear_idle_serf(pos);
    game->get_map()->set_serf_index(pos, index);

    int dir = s.idle_on_path.field_E;

    serf_log_state_change(this, SERF_STATE_TRANSPORTING);
    state = SERF_STATE_TRANSPORTING;
    s.walking.res = 0;
    s.walking.wait_counter = 0;
    s.walking.dir = dir;
    tick = game->get_tick();
    counter = 0;
  }
}

void
serf_t::handle_scatter_state() {
  /* Choose a random, empty destination */
  while (1) {
    int r = game->random_int();
    int col = (r & 0xf);
    if (col < 8) col -= 16;
    int row = ((r >> 8) & 0xf);
    if (row < 8) row -= 16;

    map_pos_t dest = game->get_map()->pos_add(pos,
                     game->get_map()->pos(col & game->get_map()->get_col_mask(),
                                        row & game->get_map()->get_row_mask()));
    if (game->get_map()->get_obj(dest) == 0 &&
        game->get_map()->get_height(dest) > 0) {
      if (get_type() >= SERF_KNIGHT_0 &&
          get_type() <= SERF_KNIGHT_4) {
        serf_log_state_change(this, SERF_STATE_KNIGHT_FREE_WALKING);
        state = SERF_STATE_KNIGHT_FREE_WALKING;
      } else {
        serf_log_state_change(this, SERF_STATE_FREE_WALKING);
        state = SERF_STATE_FREE_WALKING;
      }

      s.free_walking.dist1 = col;
      s.free_walking.dist2 = row;
      s.free_walking.neg_dist1 = -128;
      s.free_walking.neg_dist2 = -1;
      s.free_walking.flags = 0;
      counter = 0;
      return;
    }
  }
}

void
serf_t::handle_serf_finished_building_state() {
  if (game->get_map()->get_serf_index(game->get_map()->move_down_right(pos)) ==
      0) {
    serf_log_state_change(this, SERF_STATE_READY_TO_LEAVE);
    state = SERF_STATE_READY_TO_LEAVE;
    s.leaving_building.dest = 0;
    s.leaving_building.field_B = -2;
    s.leaving_building.dir = 0;
    s.leaving_building.next_state = SERF_STATE_WALKING;

    if (game->get_map()->get_serf_index(pos) != index &&
        game->get_map()->get_serf_index(pos) != 0) {
      animation = 82;
    }
  }
}

void
serf_t::handle_serf_wake_at_flag_state() {
  if (game->get_map()->get_serf_index(pos) == 0) {
    game->get_map()->clear_idle_serf(pos);
    game->get_map()->set_serf_index(pos, index);
    tick = game->get_tick();
    counter = 0;

    if (get_type() == SERF_SAILOR) {
      serf_log_state_change(this, SERF_STATE_LOST_SAILOR);
      state = SERF_STATE_LOST_SAILOR;
    } else {
      serf_log_state_change(this, SERF_STATE_LOST);
      state = SERF_STATE_LOST;
      s.lost.field_B = 0;
    }
  }
}

void
serf_t::handle_serf_wake_on_path_state() {
  serf_log_state_change(this, SERF_STATE_WAIT_IDLE_ON_PATH);
  state = SERF_STATE_WAIT_IDLE_ON_PATH;

  for (int d = DIR_UP; d >= DIR_RIGHT; d--) {
    if (BIT_TEST(game->get_map()->paths(pos), d)) {
      s.idle_on_path.field_E = d;
      break;
    }
  }
}

void
serf_t::handle_serf_defending_state(const int training_params[]) {
  switch (get_type()) {
  case SERF_KNIGHT_0:
  case SERF_KNIGHT_1:
  case SERF_KNIGHT_2:
  case SERF_KNIGHT_3:
    train_knight(training_params[get_type()-SERF_KNIGHT_0]);
  case SERF_KNIGHT_4: /* Cannot train anymore. */
    break;
  default:
    NOT_REACHED();
    break;
  }
}

void
serf_t::handle_serf_defending_hut_state() {
  const int training_params[] = { 250, 125, 62, 31 };
  handle_serf_defending_state(training_params);
}

void
serf_t::handle_serf_defending_tower_state() {
  const int training_params[] = { 1000, 500, 250, 125 };
  handle_serf_defending_state(training_params);
}

void
serf_t::handle_serf_defending_fortress_state() {
  const int training_params[] = { 2000, 1000, 500, 250 };
  handle_serf_defending_state(training_params);
}

void
serf_t::handle_serf_defending_castle_state() {
  const int training_params[] = { 4000, 2000, 1000, 500 };
  handle_serf_defending_state(training_params);
}

void
serf_t::update() {
  switch (state) {
  case SERF_STATE_NULL: /* 0 */
    break;
  case SERF_STATE_WALKING:
    handle_serf_walking_state();
    break;
  case SERF_STATE_TRANSPORTING:
    handle_serf_transporting_state();
    break;
  case SERF_STATE_IDLE_IN_STOCK:
    handle_serf_idle_in_stock_state();
    break;
  case SERF_STATE_ENTERING_BUILDING:
    handle_serf_entering_building_state();
    break;
  case SERF_STATE_LEAVING_BUILDING: /* 5 */
    handle_serf_leaving_building_state();
    break;
  case SERF_STATE_READY_TO_ENTER:
    handle_serf_ready_to_enter_state();
    break;
  case SERF_STATE_READY_TO_LEAVE:
    handle_serf_ready_to_leave_state();
    break;
  case SERF_STATE_DIGGING:
    handle_serf_digging_state();
    break;
  case SERF_STATE_BUILDING:
    handle_serf_building_state();
    break;
  case SERF_STATE_BUILDING_CASTLE: /* 10 */
    handle_serf_building_castle_state();
    break;
  case SERF_STATE_MOVE_RESOURCE_OUT:
    handle_serf_move_resource_out_state();
    break;
  case SERF_STATE_WAIT_FOR_RESOURCE_OUT:
    handle_serf_wait_for_resource_out_state();
    break;
  case SERF_STATE_DROP_RESOURCE_OUT:
    handle_serf_drop_resource_out_state();
    break;
  case SERF_STATE_DELIVERING:
    handle_serf_delivering_state();
    break;
  case SERF_STATE_READY_TO_LEAVE_INVENTORY: /* 15 */
    handle_serf_ready_to_leave_inventory_state();
    break;
  case SERF_STATE_FREE_WALKING:
    handle_serf_free_walking_state();
    break;
  case SERF_STATE_LOGGING:
    handle_serf_logging_state();
    break;
  case SERF_STATE_PLANNING_LOGGING:
    handle_serf_planning_logging_state();
    break;
  case SERF_STATE_PLANNING_PLANTING:
    handle_serf_planning_planting_state();
    break;
  case SERF_STATE_PLANTING: /* 20 */
    handle_serf_planting_state();
    break;
  case SERF_STATE_PLANNING_STONECUTTING:
    handle_serf_planning_stonecutting();
    break;
  case SERF_STATE_STONECUTTER_FREE_WALKING:
    handle_stonecutter_free_walking();
    break;
  case SERF_STATE_STONECUTTING:
    handle_serf_stonecutting_state();
    break;
  case SERF_STATE_SAWING:
    handle_serf_sawing_state();
    break;
  case SERF_STATE_LOST: /* 25 */
    handle_serf_lost_state();
    break;
  case SERF_STATE_LOST_SAILOR:
    handle_lost_sailor();
    break;
  case SERF_STATE_FREE_SAILING:
    handle_free_sailing();
    break;
  case SERF_STATE_ESCAPE_BUILDING:
    handle_serf_escape_building_state();
    break;
  case SERF_STATE_MINING:
    handle_serf_mining_state();
    break;
  case SERF_STATE_SMELTING: /* 30 */
    handle_serf_smelting_state();
    break;
  case SERF_STATE_PLANNING_FISHING:
    handle_serf_planning_fishing_state();
    break;
  case SERF_STATE_FISHING:
    handle_serf_fishing_state();
    break;
  case SERF_STATE_PLANNING_FARMING:
    handle_serf_planning_farming_state();
    break;
  case SERF_STATE_FARMING:
    handle_serf_farming_state();
    break;
  case SERF_STATE_MILLING: /* 35 */
    handle_serf_milling_state();
    break;
  case SERF_STATE_BAKING:
    handle_serf_baking_state();
    break;
  case SERF_STATE_PIGFARMING:
    handle_serf_pigfarming_state();
    break;
  case SERF_STATE_BUTCHERING:
    handle_serf_butchering_state();
    break;
  case SERF_STATE_MAKING_WEAPON:
    handle_serf_making_weapon_state();
    break;
  case SERF_STATE_MAKING_TOOL: /* 40 */
    handle_serf_making_tool_state();
    break;
  case SERF_STATE_BUILDING_BOAT:
    handle_serf_building_boat_state();
    break;
  case SERF_STATE_LOOKING_FOR_GEO_SPOT:
    handle_serf_looking_for_geo_spot_state();
    break;
  case SERF_STATE_SAMPLING_GEO_SPOT:
    handle_serf_sampling_geo_spot_state();
    break;
  case SERF_STATE_KNIGHT_ENGAGING_BUILDING:
    handle_serf_knight_engaging_building_state();
    break;
  case SERF_STATE_KNIGHT_PREPARE_ATTACKING: /* 45 */
    handle_serf_knight_prepare_attacking();
    break;
  case SERF_STATE_KNIGHT_LEAVE_FOR_FIGHT:
    handle_serf_knight_leave_for_fight_state();
    break;
  case SERF_STATE_KNIGHT_PREPARE_DEFENDING:
    handle_serf_knight_prepare_defending_state();
    break;
  case SERF_STATE_KNIGHT_ATTACKING:
  case SERF_STATE_KNIGHT_ATTACKING_FREE:
    handle_knight_attacking();
    break;
  case SERF_STATE_KNIGHT_DEFENDING:
  case SERF_STATE_KNIGHT_DEFENDING_FREE:
    /* The actual fight update is handled for the attacking knight. */
    break;
  case SERF_STATE_KNIGHT_ATTACKING_VICTORY: /* 50 */
    handle_serf_knight_attacking_victory_state();
    break;
  case SERF_STATE_KNIGHT_ATTACKING_DEFEAT:
    handle_serf_knight_attacking_defeat_state();
    break;
  case SERF_STATE_KNIGHT_OCCUPY_ENEMY_BUILDING:
    handle_knight_occupy_enemy_building();
    break;
  case SERF_STATE_KNIGHT_FREE_WALKING:
    handle_state_knight_free_walking();
    break;
  case SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE:
    handle_state_knight_engage_defending_free();
    break;
  case SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE:
    handle_state_knight_engage_attacking_free();
    break;
  case SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE_JOIN:
    handle_state_knight_engage_attacking_free_join();
    break;
  case SERF_STATE_KNIGHT_PREPARE_ATTACKING_FREE:
    handle_state_knight_prepare_attacking_free();
    break;
  case SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE:
    handle_state_knight_prepare_defending_free();
    break;
  case SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE_WAIT:
    /* Nothing to do for this state. */
    break;
  case SERF_STATE_KNIGHT_ATTACKING_VICTORY_FREE:
    handle_knight_attacking_victory_free();
    break;
  case SERF_STATE_KNIGHT_DEFENDING_VICTORY_FREE:
    handle_knight_defending_victory_free();
    break;
  case SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE:
    handle_serf_knight_attacking_defeat_free_state();
    break;
  case SERF_STATE_KNIGHT_ATTACKING_FREE_WAIT:
    handle_knight_attacking_free_wait();
    break;
  case SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT: /* 65 */
    handle_serf_state_knight_leave_for_walk_to_fight();
    break;
  case SERF_STATE_IDLE_ON_PATH:
    handle_serf_idle_on_path_state();
    break;
  case SERF_STATE_WAIT_IDLE_ON_PATH:
    handle_serf_wait_idle_on_path_state();
    break;
  case SERF_STATE_WAKE_AT_FLAG:
    handle_serf_wake_at_flag_state();
    break;
  case SERF_STATE_WAKE_ON_PATH:
    handle_serf_wake_on_path_state();
    break;
  case SERF_STATE_DEFENDING_HUT: /* 70 */
    handle_serf_defending_hut_state();
    break;
  case SERF_STATE_DEFENDING_TOWER:
    handle_serf_defending_tower_state();
    break;
  case SERF_STATE_DEFENDING_FORTRESS:
    handle_serf_defending_fortress_state();
    break;
  case SERF_STATE_SCATTER:
    handle_scatter_state();
    break;
  case SERF_STATE_FINISHED_BUILDING:
    handle_serf_finished_building_state();
    break;
  case SERF_STATE_DEFENDING_CASTLE: /* 75 */
    handle_serf_defending_castle_state();
    break;
  default:
    LOGD("serf", "Serf state %d isn't processed", state);
    state = SERF_STATE_NULL;
  }
}

save_reader_binary_t&
operator >> (save_reader_binary_t &reader, serf_t &serf) {
  uint8_t v8;
  reader >> v8;  // 0
  serf.owner = v8 & 3;
  serf.type = (serf_type_t)((v8 >> 2) & 0x1F);
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
    v32 = v32 >> 2;
    int x = v32 & serf.game->get_map()->get_col_mask();
    v32 = v32 >> (serf.game->get_map()->get_row_shift() + 1);
    int y = v32 & serf.game->get_map()->get_row_mask();
    serf.pos = serf.game->get_map()->pos(x, y);
  }
  reader >> v16;  // 8
  serf.tick = v16;
  reader >> v8;  // 10
  serf.state = (serf_state_t)v8;

  LOGV("savegame", "load serf %i: %s", serf.index,
       serf_t::get_state_name(serf.state));

  switch (serf.state) {
    case SERF_STATE_IDLE_IN_STOCK:
      reader >> v16;  // 11
      reader >> v8;  // 13
      reader >> v16;  // 14
      serf.s.idle_in_stock.inv_index = v16;
      break;

    case SERF_STATE_WALKING:
    case SERF_STATE_TRANSPORTING:
    case SERF_STATE_DELIVERING:
      reader >> v8;  // 11
      serf.s.walking.res = (int8_t)v8;
      reader >> v16;  // 12
      serf.s.walking.dest = v16;
      reader >> v8;  // 14
      serf.s.walking.dir = v8;
      reader >> v8;  // 15
      serf.s.walking.wait_counter = v8;
      break;

    case SERF_STATE_ENTERING_BUILDING:
      reader >> v8;  // 11
      serf.s.entering_building.field_B = v8;
      reader >> v16;  // 12
      serf.s.entering_building.slope_len = v16;
      break;

    case SERF_STATE_LEAVING_BUILDING:
    case SERF_STATE_READY_TO_LEAVE:
      reader >> v8;  // 11
      serf.s.leaving_building.field_B = v8;
      reader >> v8;  // 12
      serf.s.leaving_building.dest = v8;
      reader >> v8;  // 13
      serf.s.leaving_building.dest2 = v8;
      reader >> v8;  // 14
      serf.s.leaving_building.dir = v8;
      reader >> v8;  // 15
      serf.s.leaving_building.next_state = (serf_state_t)v8;
      break;

    case SERF_STATE_READY_TO_ENTER:
      reader >> v8;  // 11
      serf.s.ready_to_enter.field_B = v8;
      break;

    case SERF_STATE_DIGGING:
      reader >> v8;  // 11
      serf.s.digging.h_index = v8;
      reader >> v8;  // 12
      serf.s.digging.target_h = v8;
      reader >> v8;  // 13
      serf.s.digging.dig_pos = v8;
      reader >> v8;  // 14
      serf.s.digging.substate = v8;
      break;

    case SERF_STATE_BUILDING:
      reader >> v8;  // 11
      serf.s.building.mode = (int8_t)v8;
      reader >> v16;  // 12
      serf.s.building.bld_index = v16;
      reader >> v8;  // 14
      serf.s.building.material_step = v8;
      reader >> v8;  // 15
      serf.s.building.counter = v8;
      break;

    case SERF_STATE_BUILDING_CASTLE:
      reader >> v8;  // 11
      reader >> v16;  // 12
      serf.s.building_castle.inv_index = v16;
      break;

    case SERF_STATE_MOVE_RESOURCE_OUT:
    case SERF_STATE_DROP_RESOURCE_OUT:
      reader >> v8;  // 11
      serf.s.move_resource_out.res = v8;
      reader >> v16;  // 12
      serf.s.move_resource_out.res_dest = v16;
      reader >> v8;  // 14
      reader >> v8;  // 15
      serf.s.move_resource_out.next_state = (serf_state_t)v8;
      break;

    case SERF_STATE_READY_TO_LEAVE_INVENTORY:
      reader >> v8;  // 11
      serf.s.ready_to_leave_inventory.mode = v8;
      reader >> v16;  // 12
      serf.s.ready_to_leave_inventory.dest = v16;
      reader >> v16;  // 14
      serf.s.ready_to_leave_inventory.inv_index = v16;
      break;

    case SERF_STATE_FREE_WALKING:
    case SERF_STATE_LOGGING:
    case SERF_STATE_PLANTING:
    case SERF_STATE_STONECUTTING:
    case SERF_STATE_STONECUTTER_FREE_WALKING:
    case SERF_STATE_FISHING:
    case SERF_STATE_FARMING:
    case SERF_STATE_SAMPLING_GEO_SPOT:
      reader >> v8;  // 11
      serf.s.free_walking.dist1 = v8;
      reader >> v8;  // 12
      serf.s.free_walking.dist2 = v8;
      reader >> v8;  // 13
      serf.s.free_walking.neg_dist1 = (int8_t)v8;
      reader >> v8;  // 14
      serf.s.free_walking.neg_dist2 = (int8_t)v8;
      reader >> v8;  // 15
      serf.s.free_walking.flags = v8;
      break;

    case SERF_STATE_SAWING:
      reader >> v8;  // 11
      serf.s.sawing.mode = v8;
      break;

    case SERF_STATE_LOST:
      reader >> v8;  // 11
      serf.s.lost.field_B = v8;
      break;

    case SERF_STATE_MINING:
      reader >> v8;  // 11
      serf.s.mining.substate = v8;
      reader >> v8;  // 12
      reader >> v8;  // 13
      serf.s.mining.res = v8;
      reader >> v8;  // 14
      serf.s.mining.deposit = (ground_deposit_t)v8;
      break;

    case SERF_STATE_SMELTING:
      reader >> v8;  // 11
      serf.s.smelting.mode = v8;
      reader >> v8;  // 12
      serf.s.smelting.counter = v8;
      reader >> v8;  // 13
      serf.s.smelting.type = v8;
      break;

    case SERF_STATE_MILLING:
      reader >> v8;  // 11
      serf.s.milling.mode = v8;
      break;

    case SERF_STATE_BAKING:
      reader >> v8;  // 11
      serf.s.baking.mode = v8;
      break;

    case SERF_STATE_PIGFARMING:
      reader >> v8;  // 11
      serf.s.pigfarming.mode = v8;
      break;

    case SERF_STATE_BUTCHERING:
      reader >> v8;  // 11
      serf.s.butchering.mode = v8;
      break;

    case SERF_STATE_MAKING_WEAPON:
      reader >> v8;  // 11
      serf.s.making_weapon.mode = v8;
      break;

    case SERF_STATE_MAKING_TOOL:
      reader >> v8;  // 11
      serf.s.making_tool.mode = v8;
      break;

    case SERF_STATE_BUILDING_BOAT:
      reader >> v8;  // 11
      serf.s.building_boat.mode = v8;
      break;

    case SERF_STATE_KNIGHT_DEFENDING_VICTORY_FREE:
      /* TODO This will be tricky to load since the
       function of this state has been changed to one
       that is driven by the attacking serf instead
       (SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE). */
      break;

    case SERF_STATE_IDLE_ON_PATH:
    case SERF_STATE_WAIT_IDLE_ON_PATH:
    case SERF_STATE_WAKE_AT_FLAG:
    case SERF_STATE_WAKE_ON_PATH:
      reader >> v8;  // 11
      serf.s.idle_on_path.rev_dir = (dir_t)v8;
      reader >> v16;  // 12
      serf.s.idle_on_path.flag = serf.get_game()->create_flag(v16/70);
      reader >> v8;  // 14
      serf.s.idle_on_path.field_E = v8;
      break;

    case SERF_STATE_DEFENDING_HUT:
    case SERF_STATE_DEFENDING_TOWER:
    case SERF_STATE_DEFENDING_FORTRESS:
    case SERF_STATE_DEFENDING_CASTLE:
      reader >> v16;  // 11
      reader >> v8;  // 13
      reader >> v16;  // 14
      serf.s.defending.next_knight = v16;
      break;

    default: break;
  }

  return reader;
}

save_reader_text_t&
operator >> (save_reader_text_t &reader, serf_t &serf) {
  int type;
  reader.value("type") >> type;
  try {
    reader.value("owner") >> serf.owner;
    serf.type = (serf_type_t)type;
  } catch(...) {
    serf.type = (serf_type_t)((type >> 2) & 0x1f);
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
    case SERF_STATE_IDLE_IN_STOCK:
      reader.value("state.inventory") >> serf.s.idle_in_stock.inv_index;
      break;

    case SERF_STATE_WALKING:
    case SERF_STATE_TRANSPORTING:
    case SERF_STATE_DELIVERING:
      reader.value("state.res") >> serf.s.walking.res;
      reader.value("state.dest") >> serf.s.walking.dest;
      reader.value("state.dir") >> serf.s.walking.dir;
      reader.value("state.wait_counter") >> serf.s.walking.wait_counter;
      break;

    case SERF_STATE_ENTERING_BUILDING:
      reader.value("state.field_B") >> serf.s.entering_building.field_B;
      reader.value("state.slope_len") >> serf.s.entering_building.slope_len;
      break;

    case SERF_STATE_LEAVING_BUILDING:
    case SERF_STATE_READY_TO_LEAVE:
    case SERF_STATE_KNIGHT_LEAVE_FOR_FIGHT:
      reader.value("state.field_B") >> serf.s.leaving_building.field_B;
      reader.value("state.dest") >> serf.s.leaving_building.dest;
      reader.value("state.dest2") >> serf.s.leaving_building.dest2;
      reader.value("state.dir") >> serf.s.leaving_building.dir;
      reader.value("state.next_state") >> serf.s.leaving_building.next_state;
      break;

    case SERF_STATE_READY_TO_ENTER:
      reader.value("state.field_B") >> serf.s.ready_to_enter.field_B;
      break;

    case SERF_STATE_DIGGING:
      reader.value("state.h_index") >> serf.s.digging.h_index;
      reader.value("state.target_h") >> serf.s.digging.target_h;
      reader.value("state.dig_pos") >> serf.s.digging.dig_pos;
      reader.value("state.substate") >> serf.s.digging.substate;
      break;

    case SERF_STATE_BUILDING:
      reader.value("state.mode") >> serf.s.building.mode;
      reader.value("state.bld_index") >> serf.s.building.bld_index;
      reader.value("state.material_step") >> serf.s.building.material_step;
      reader.value("state.counter") >> serf.s.building.counter;
      break;

    case SERF_STATE_BUILDING_CASTLE:
      reader.value("state.inv_index") >> serf.s.building_castle.inv_index;
      break;

    case SERF_STATE_MOVE_RESOURCE_OUT:
    case SERF_STATE_DROP_RESOURCE_OUT:
      reader.value("state.res") >> serf.s.move_resource_out.res;
      reader.value("state.res_dest") >> serf.s.move_resource_out.res_dest;
      reader.value("state.next_state") >> serf.s.move_resource_out.next_state;
      break;

    case SERF_STATE_READY_TO_LEAVE_INVENTORY:
      reader.value("state.mode") >> serf.s.ready_to_leave_inventory.mode;
      reader.value("state.dest") >> serf.s.ready_to_leave_inventory.dest;
      reader.value("state.inv_index") >>
        serf.s.ready_to_leave_inventory.inv_index;
      break;

    case SERF_STATE_FREE_WALKING:
    case SERF_STATE_LOGGING:
    case SERF_STATE_PLANTING:
    case SERF_STATE_STONECUTTING:
    case SERF_STATE_STONECUTTER_FREE_WALKING:
    case SERF_STATE_FISHING:
    case SERF_STATE_FARMING:
    case SERF_STATE_SAMPLING_GEO_SPOT:
    case SERF_STATE_KNIGHT_FREE_WALKING:
    case SERF_STATE_KNIGHT_ATTACKING_FREE:
    case SERF_STATE_KNIGHT_ATTACKING_FREE_WAIT:
      reader.value("state.dist1") >> serf.s.free_walking.dist1;
      reader.value("state.dist2") >> serf.s.free_walking.dist2;
      reader.value("state.neg_dist") >> serf.s.free_walking.neg_dist1;
      reader.value("state.neg_dist2") >> serf.s.free_walking.neg_dist2;
      reader.value("state.flags") >> serf.s.free_walking.flags;
      break;

    case SERF_STATE_SAWING:
      reader.value("state.mode") >> serf.s.sawing.mode;
      break;

    case SERF_STATE_LOST:
      reader.value("state.field_B") >> serf.s.lost.field_B;
      break;

    case SERF_STATE_MINING:
      reader.value("state.substate") >> serf.s.mining.substate;
      reader.value("state.res") >> serf.s.mining.res;
      unsigned int deposit;
      reader.value("state.deposit") >> deposit;
      serf.s.mining.deposit = (ground_deposit_t)deposit;
      break;

    case SERF_STATE_SMELTING:
      reader.value("state.mode") >> serf.s.smelting.mode;
      reader.value("state.counter") >> serf.s.smelting.counter;
      reader.value("state.type") >> serf.s.smelting.type;
      break;

    case SERF_STATE_MILLING:
      reader.value("state.mode") >> serf.s.milling.mode;
      break;

    case SERF_STATE_BAKING:
      reader.value("state.mode") >> serf.s.baking.mode;
      break;

    case SERF_STATE_PIGFARMING:
      reader.value("state.mode") >> serf.s.pigfarming.mode;
      break;

    case SERF_STATE_BUTCHERING:
      reader.value("state.mode") >> serf.s.butchering.mode;
      break;

    case SERF_STATE_MAKING_WEAPON:
      reader.value("state.mode") >> serf.s.making_weapon.mode;
      break;

    case SERF_STATE_MAKING_TOOL:
      reader.value("state.mode") >> serf.s.making_tool.mode;
      break;

    case SERF_STATE_BUILDING_BOAT:
      reader.value("state.mode") >> serf.s.building_boat.mode;
      break;

    case SERF_STATE_KNIGHT_ENGAGING_BUILDING:
    case SERF_STATE_KNIGHT_PREPARE_ATTACKING:
    case SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE_WAIT:
    case SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE:
    case SERF_STATE_KNIGHT_ATTACKING:
    case SERF_STATE_KNIGHT_ATTACKING_VICTORY:
    case SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE:
    case SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE_JOIN:
    case SERF_STATE_KNIGHT_ATTACKING_VICTORY_FREE:
      reader.value("state.field_B") >> serf.s.attacking.field_B;
      reader.value("state.field_C") >> serf.s.attacking.field_C;
      reader.value("state.field_D") >> serf.s.attacking.field_D;
      reader.value("state.def_index") >> serf.s.attacking.def_index;
      break;

    case SERF_STATE_KNIGHT_DEFENDING_FREE:
    case SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE:
      reader.value("state.dist_col") >> serf.s.defending_free.dist_col;
      reader.value("state.dist_row") >> serf.s.defending_free.dist_row;
      reader.value("state.field_D") >> serf.s.defending_free.field_D;
      reader.value("state.other_dist_col") >>
        serf.s.defending_free.other_dist_col;
      reader.value("state.other_dist_row") >>
        serf.s.defending_free.other_dist_row;
      break;

    case SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT:
      reader.value("state.dist_col") >> serf.s.leave_for_walk_to_fight.dist_col;
      reader.value("state.dist_row") >> serf.s.leave_for_walk_to_fight.dist_row;
      reader.value("state.field_D") >> serf.s.leave_for_walk_to_fight.field_D;
      reader.value("state.field_E") >> serf.s.leave_for_walk_to_fight.field_E;
      reader.value("state.next_state") >>
        serf.s.leave_for_walk_to_fight.next_state;
      break;

    case SERF_STATE_IDLE_ON_PATH:
    case SERF_STATE_WAIT_IDLE_ON_PATH:
    case SERF_STATE_WAKE_AT_FLAG:
    case SERF_STATE_WAKE_ON_PATH:
      reader.value("state.rev_dir") >> serf.s.idle_on_path.rev_dir;
      unsigned int flag_idex;
      reader.value("state.flag") >> flag_idex;
      serf.s.idle_on_path.flag = serf.get_game()->create_flag(flag_idex);
      reader.value("state.field_E") >> serf.s.idle_on_path.field_E;
      break;

    case SERF_STATE_DEFENDING_HUT:
    case SERF_STATE_DEFENDING_TOWER:
    case SERF_STATE_DEFENDING_FORTRESS:
    case SERF_STATE_DEFENDING_CASTLE:
      reader.value("state.next_knight") >> serf.s.defending.next_knight;
      break;

    default:
      break;
  }

  return reader;
}

save_writer_text_t&
operator << (save_writer_text_t &writer, serf_t &serf) {
  writer.value("type") << serf.type;
  writer.value("owner") << serf.owner;
  writer.value("animation") << serf.animation;
  writer.value("counter") << serf.counter;
  writer.value("pos") << serf.get_game()->get_map()->pos_col(serf.pos);
  writer.value("pos") << serf.get_game()->get_map()->pos_row(serf.pos);
  writer.value("tick") << serf.tick;
  writer.value("state") << serf.state;

  switch (serf.state) {
    case SERF_STATE_IDLE_IN_STOCK:
      writer.value("state.inventory") << serf.s.idle_in_stock.inv_index;
      break;

    case SERF_STATE_WALKING:
    case SERF_STATE_TRANSPORTING:
    case SERF_STATE_DELIVERING:
      writer.value("state.res") << serf.s.walking.res;
      writer.value("state.dest") << serf.s.walking.dest;
      writer.value("state.dir") << serf.s.walking.dir;
      writer.value("state.wait_counter") << serf.s.walking.wait_counter;
      break;

    case SERF_STATE_ENTERING_BUILDING:
      writer.value("state.field_B") << serf.s.entering_building.field_B;
      writer.value("state.slope_len") << serf.s.entering_building.slope_len;
      break;

    case SERF_STATE_LEAVING_BUILDING:
    case SERF_STATE_READY_TO_LEAVE:
    case SERF_STATE_KNIGHT_LEAVE_FOR_FIGHT:
      writer.value("state.field_B") << serf.s.leaving_building.field_B;
      writer.value("state.dest") << serf.s.leaving_building.dest;
      writer.value("state.dest2") << serf.s.leaving_building.dest2;
      writer.value("state.dir") << serf.s.leaving_building.dir;
      writer.value("state.next_state") << serf.s.leaving_building.next_state;
      break;

    case SERF_STATE_READY_TO_ENTER:
      writer.value("state.field_B") << serf.s.ready_to_enter.field_B;
      break;

    case SERF_STATE_DIGGING:
      writer.value("state.h_index") << serf.s.digging.h_index;
      writer.value("state.target_h") << serf.s.digging.target_h;
      writer.value("state.dig_pos") << serf.s.digging.dig_pos;
      writer.value("state.substate") << serf.s.digging.substate;
      break;

    case SERF_STATE_BUILDING:
      writer.value("state.mode") << serf.s.building.mode;
      writer.value("state.bld_index") << serf.s.building.bld_index;
      writer.value("state.material_step") << serf.s.building.material_step;
      writer.value("state.counter") << serf.s.building.counter;
      break;

    case SERF_STATE_BUILDING_CASTLE:
      writer.value("state.inv_index") << serf.s.building_castle.inv_index;
      break;

    case SERF_STATE_MOVE_RESOURCE_OUT:
    case SERF_STATE_DROP_RESOURCE_OUT:
      writer.value("state.res") << serf.s.move_resource_out.res;
      writer.value("state.res_dest") << serf.s.move_resource_out.res_dest;
      writer.value("state.next_state") << serf.s.move_resource_out.next_state;
      break;

    case SERF_STATE_READY_TO_LEAVE_INVENTORY:
      writer.value("state.mode") << serf.s.ready_to_leave_inventory.mode;
      writer.value("state.dest") << serf.s.ready_to_leave_inventory.dest;
      writer.value("state.inv_index") <<
        serf.s.ready_to_leave_inventory.inv_index;
      break;

    case SERF_STATE_FREE_WALKING:
    case SERF_STATE_LOGGING:
    case SERF_STATE_PLANTING:
    case SERF_STATE_STONECUTTING:
    case SERF_STATE_STONECUTTER_FREE_WALKING:
    case SERF_STATE_FISHING:
    case SERF_STATE_FARMING:
    case SERF_STATE_SAMPLING_GEO_SPOT:
    case SERF_STATE_KNIGHT_FREE_WALKING:
    case SERF_STATE_KNIGHT_ATTACKING_FREE:
    case SERF_STATE_KNIGHT_ATTACKING_FREE_WAIT:
      writer.value("state.dist1") << serf.s.free_walking.dist1;
      writer.value("state.dist2") << serf.s.free_walking.dist2;
      writer.value("state.neg_dist") << serf.s.free_walking.neg_dist1;
      writer.value("state.neg_dist2") << serf.s.free_walking.neg_dist2;
      writer.value("state.flags") << serf.s.free_walking.flags;
      break;

    case SERF_STATE_SAWING:
      writer.value("state.mode") << serf.s.sawing.mode;
      break;

    case SERF_STATE_LOST:
      writer.value("state.field_B") << serf.s.lost.field_B;
      break;

    case SERF_STATE_MINING:
      writer.value("state.substate") << serf.s.mining.substate;
      writer.value("state.res") << serf.s.mining.res;
      writer.value("state.deposit") << serf.s.mining.deposit;
      break;

    case SERF_STATE_SMELTING:
      writer.value("state.mode") << serf.s.smelting.mode;
      writer.value("state.counter") << serf.s.smelting.counter;
      writer.value("state.type") << serf.s.smelting.type;
      break;

    case SERF_STATE_MILLING:
      writer.value("state.mode") << serf.s.milling.mode;
      break;

    case SERF_STATE_BAKING:
      writer.value("state.mode") << serf.s.baking.mode;
      break;

    case SERF_STATE_PIGFARMING:
      writer.value("state.mode") << serf.s.pigfarming.mode;
      break;

    case SERF_STATE_BUTCHERING:
      writer.value("state.mode") << serf.s.butchering.mode;
      break;

    case SERF_STATE_MAKING_WEAPON:
      writer.value("state.mode") << serf.s.making_weapon.mode;
      break;

    case SERF_STATE_MAKING_TOOL:
      writer.value("state.mode") << serf.s.making_tool.mode;
      break;

    case SERF_STATE_BUILDING_BOAT:
      writer.value("state.mode") << serf.s.building_boat.mode;
      break;

    case SERF_STATE_KNIGHT_ENGAGING_BUILDING:
    case SERF_STATE_KNIGHT_PREPARE_ATTACKING:
    case SERF_STATE_KNIGHT_PREPARE_DEFENDING_FREE_WAIT:
    case SERF_STATE_KNIGHT_ATTACKING_DEFEAT_FREE:
    case SERF_STATE_KNIGHT_ATTACKING:
    case SERF_STATE_KNIGHT_ATTACKING_VICTORY:
    case SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE:
    case SERF_STATE_KNIGHT_ENGAGE_ATTACKING_FREE_JOIN:
    case SERF_STATE_KNIGHT_ATTACKING_VICTORY_FREE:
      writer.value("state.field_B") << serf.s.attacking.field_B;
      writer.value("state.field_C") << serf.s.attacking.field_C;
      writer.value("state.field_D") << serf.s.attacking.field_D;
      writer.value("state.def_index") << serf.s.attacking.def_index;
      break;

    case SERF_STATE_KNIGHT_DEFENDING_FREE:
    case SERF_STATE_KNIGHT_ENGAGE_DEFENDING_FREE:
      writer.value("state.dist_col") << serf.s.defending_free.dist_col;
      writer.value("state.dist_row") << serf.s.defending_free.dist_row;
      writer.value("state.field_D") << serf.s.defending_free.field_D;
      writer.value("state.other_dist_col") <<
        serf.s.defending_free.other_dist_col;
      writer.value("state.other_dist_row") <<
        serf.s.defending_free.other_dist_row;
      break;

    case SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT:
      writer.value("state.dist_col") << serf.s.leave_for_walk_to_fight.dist_col;
      writer.value("state.dist_row") << serf.s.leave_for_walk_to_fight.dist_row;
      writer.value("state.field_D") << serf.s.leave_for_walk_to_fight.field_D;
      writer.value("state.field_E") << serf.s.leave_for_walk_to_fight.field_E;
      writer.value("state.next_state") <<
        serf.s.leave_for_walk_to_fight.next_state;
      break;

    case SERF_STATE_IDLE_ON_PATH:
    case SERF_STATE_WAIT_IDLE_ON_PATH:
    case SERF_STATE_WAKE_AT_FLAG:
    case SERF_STATE_WAKE_ON_PATH:
      writer.value("state.rev_dir") << serf.s.idle_on_path.rev_dir;
      writer.value("state.flag") << serf.s.idle_on_path.flag->get_index();
      writer.value("state.field_E") << serf.s.idle_on_path.field_E;
      break;

    case SERF_STATE_DEFENDING_HUT:
    case SERF_STATE_DEFENDING_TOWER:
    case SERF_STATE_DEFENDING_FORTRESS:
    case SERF_STATE_DEFENDING_CASTLE:
      writer.value("state.next_knight") << serf.s.defending.next_knight;
      break;

    default: break;
  }

  return writer;
}
