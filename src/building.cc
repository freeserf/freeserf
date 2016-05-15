/*
 * building.cc - Building related functions.
 *
 * Copyright (C) 2012  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/building.h"

#include <cassert>

#include "src/game.h"
#include "src/inventory.h"
#include "src/debug.h"
#include "src/savegame.h"

building_t::building_t(game_t *game, unsigned int index)
  : game_object_t(game, index) {
  type = BUILDING_NONE;
  bld = BIT(7); /* bit 7: Unfinished building */
  flag = 0;
  serf = 0;

  for (int j = 0; j < BUILDING_MAX_STOCK; j++) {
    stock[j].type = RESOURCE_NONE;
    stock[j].prio = 0;
    stock[j].available = 0;
    stock[j].requested = 0;
    stock[j].maximum = 0;
  }

  serf_index = 0;
}

map_obj_t
building_t::start_building(building_type_t type) {
  const int construction_cost[] = {
    0, 0, 2, 0, 2, 0, 3, 0, 2, 0,
    4, 1, 5, 0, 5, 0, 5, 0,
    2, 0, 4, 3, 1, 1, 4, 1, 2, 1, 4, 1, 3, 1, 2, 1,
    3, 2, 3, 2, 3, 3, 2, 1, 2, 3, 5, 5, 4, 1
  };

  const map_obj_t obj_types[] = {
    MAP_OBJ_NONE,            // BUILDING_NONE
    MAP_OBJ_SMALL_BUILDING,  // BUILDING_FISHER
    MAP_OBJ_SMALL_BUILDING,  // BUILDING_LUMBERJACK
    MAP_OBJ_SMALL_BUILDING,  // BUILDING_BOATBUILDER
    MAP_OBJ_SMALL_BUILDING,  // BUILDING_STONECUTTER
    MAP_OBJ_SMALL_BUILDING,  // BUILDING_STONEMINE
    MAP_OBJ_SMALL_BUILDING,  // BUILDING_COALMINE
    MAP_OBJ_SMALL_BUILDING,  // BUILDING_IRONMINE
    MAP_OBJ_SMALL_BUILDING,  // BUILDING_GOLDMINE
    MAP_OBJ_SMALL_BUILDING,  // BUILDING_FORESTER
    MAP_OBJ_LARGE_BUILDING,  // BUILDING_STOCK
    MAP_OBJ_SMALL_BUILDING,  // BUILDING_HUT
    MAP_OBJ_LARGE_BUILDING,  // BUILDING_FARM
    MAP_OBJ_LARGE_BUILDING,  // BUILDING_BUTCHER
    MAP_OBJ_LARGE_BUILDING,  // BUILDING_PIGFARM
    MAP_OBJ_SMALL_BUILDING,  // BUILDING_MILL
    MAP_OBJ_LARGE_BUILDING,  // BUILDING_BAKER
    MAP_OBJ_LARGE_BUILDING,  // BUILDING_SAWMILL
    MAP_OBJ_LARGE_BUILDING,  // BUILDING_STEELSMELTER
    MAP_OBJ_LARGE_BUILDING,  // BUILDING_TOOLMAKER
    MAP_OBJ_LARGE_BUILDING,  // BUILDING_WEAPONSMITH
    MAP_OBJ_LARGE_BUILDING,  // BUILDING_TOWER
    MAP_OBJ_LARGE_BUILDING,  // BUILDING_FORTRESS
    MAP_OBJ_LARGE_BUILDING,  // BUILDING_GOLDSMELTER
    MAP_OBJ_CASTLE,          // BUILDING_CASTLE
  };

  map_obj_t map_obj = obj_types[type];

  set_type(type);
  progress = 0;
  if (map_obj != MAP_OBJ_LARGE_BUILDING) progress = 1;

  if (type == BUILDING_CASTLE) {
    stock[0].available = 0xff;
    stock[0].requested = 0xff;
    stock[1].available = 0xff;
    stock[1].requested = 0xff;
  } else {
    stock[0].type = RESOURCE_PLANK;
    stock[0].prio = 0;
    stock[0].maximum = construction_cost[2*type];
    stock[1].type = RESOURCE_STONE;
    stock[1].prio = 0;
    stock[1].maximum = construction_cost[2*type+1];
  }

  return map_obj;
}

serf_t*
building_t::call_defender_out() {
  /* Remove knight from stats of defending building */
  if (has_inventory()) { /* Castle */
    game->get_player(get_owner())->decrease_castle_knights();
  } else {
    stock[0].available -= 1;
    stock[0].requested += 1;
  }

  /* The last knight in the list has to defend. */
  serf_t *first_serf = game->get_serf(serf_index);
  serf_t *def_serf = first_serf->extract_last_knight_from_list();

  if (def_serf->get_index() == serf_index) {
    serf_index = 0;
  }

  return def_serf;
}

serf_t*
building_t::call_attacker_out(int knight_index) {
  stock[0].available -= 1;

  /* Unlink knight from list. */
  serf_t *first_serf = game->get_serf(serf_index);
  serf_t *def_serf = first_serf->extract_last_knight_from_list();

  if (def_serf->get_index() == serf_index) {
    serf_index = 0;
  }

  return def_serf;
}

unsigned int
building_t::military_gold_count() {
  unsigned int count = 0;

  if (get_type() == BUILDING_HUT ||
      get_type() == BUILDING_TOWER ||
      get_type() == BUILDING_FORTRESS) {
    for (int j = 0; j < BUILDING_MAX_STOCK; j++) {
      if (stock[j].type == RESOURCE_GOLDBAR) {
        count += stock[j].available;
      }
    }
  }

  return count;
}

void
building_t::cancel_transported_resource(resource_type_t res) {
  if (res == RESOURCE_FISH ||
      res == RESOURCE_MEAT ||
      res == RESOURCE_BREAD) {
    res = RESOURCE_GROUP_FOOD;
  }

  int in_stock = -1;
  for (int i = 0; i < BUILDING_MAX_STOCK; i++) {
    if (stock[i].type == res) {
      in_stock = i;
      break;
    }
  }

  if (in_stock >= 0) {
    stock[in_stock].requested -= 1;
    assert(stock[in_stock].requested >= 0);
  } else {
    assert(has_inventory());
  }
}

bool
building_t::add_requested_resource(resource_type_t res, bool fix_priority) {
  for (int j = 0; j < BUILDING_MAX_STOCK; j++) {
    if (stock[j].type == res) {
      if (fix_priority) {
        int prio = stock[j].prio;
        if ((prio & 1) == 0) prio = 0;
        stock[j].prio = prio >> 1;
      } else {
        stock[j].prio = 0;
      }
      stock[j].requested += 1;
      return true;
    }
  }

  return false;
}

void
building_t::stock_init(unsigned int stock_num, resource_type_t type,
                       unsigned int maximum) {
  stock[stock_num].type = type;
  stock[stock_num].prio = 0;
  stock[stock_num].maximum = maximum;
}

void
building_t::requested_resource_delivered(resource_type_t resource) {
  for (int i = 0; i < BUILDING_MAX_STOCK; i++) {
    if (stock[i].type == resource) {
      stock[i].available += 1;
      stock[i].requested -= 1;
      assert(stock[i].requested >= 0);
      return;
    }
  }

  assert(true);
}

void
building_t::requested_knight_arrived() {
  stock[0].available += 1;
  stock[0].requested -= 1;
}

bool
building_t::is_enough_place_for_knight() {
  int max_capacity = -1;
  switch (get_type()) {
    case BUILDING_HUT: max_capacity = 3; break;
    case BUILDING_TOWER: max_capacity = 6; break;
    case BUILDING_FORTRESS: max_capacity = 12; break;
    default: NOT_REACHED(); break;
  }

  int total_knights = stock[0].requested + stock[0].available;
  return (total_knights < max_capacity);
}

bool
building_t::knight_come_back_from_fight(serf_t *knight) {
  if (is_enough_place_for_knight()) {
    stock[0].available += 1;
    serf_t *serf = game->get_serf(serf_index);
    knight->insert_before(serf);
    set_main_serf(knight->get_index());
    return true;
  }

  return false;
}

void
building_t::knight_occupy() {
  if (!has_main_serf()) {
    stock[0].available = 0;
    stock[0].requested = 1;
  } else {
    stock[0].requested += 1;
  }
}

void
building_t::update(unsigned int tick) {
  if (is_burning()) {
    uint16_t delta = tick - u.tick;
    u.tick = tick;
    if (serf_index >= delta) {
      serf_index -= delta;
    } else {
      game->delete_building(this);
    }
  } else {
    update();
  }
}

void
building_t::knight_request_granted() {
  stock[0].requested += 1;
  serf_request_complete();
}

void
building_t::remove_stock() {
  stock[0].available = 0;
  stock[0].requested = 0;
  stock[1].available = 0;
  stock[1].requested = 0;
}

int
building_t::get_max_priority_for_resource(resource_type_t resource,
                                          int minimum) {
  int max_prio = -1;

  for (int i = 0; i < BUILDING_MAX_STOCK; i++) {
    if (stock[i].type == resource &&
        stock[i].prio >= minimum &&
        stock[i].prio > max_prio) {
      max_prio = stock[i].prio;
    }
  }

  return max_prio;
}

bool
building_t::use_resource_in_stock(int stock_num) {
  if (stock[stock_num].available > 0) {
    stock[stock_num].available -= 1;
    return true;
  }
  return false;
}

bool
building_t::use_resources_in_stocks() {
  if (stock[0].available > 0 &&
      stock[1].available > 0) {
    stock[0].available -= 1;
    stock[1].available -= 1;
    return true;
  }
  return false;
}

void
building_t::update() {
  if (is_done()) {
    switch (get_type()) {
      case BUILDING_NONE:
        break;
      case BUILDING_FISHER:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_FISHER,
                                        RESOURCE_ROD, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        break;
      case BUILDING_LUMBERJACK:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_LUMBERJACK,
                                        RESOURCE_AXE, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        break;
      case BUILDING_BOATBUILDER:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_BOATBUILDER,
                                        RESOURCE_HAMMER, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        if (has_serf()) {
          player_t *player = game->get_player(get_owner());
          int total_tree = stock[0].requested + stock[0].available;
          if (total_tree < stock[0].maximum) {
            stock[0].prio =
              player->get_planks_boatbuilder() >> (8 + total_tree);
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case BUILDING_STONECUTTER:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_STONECUTTER,
                                        RESOURCE_PICK, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        break;
      case BUILDING_STONEMINE:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_MINER,
                                        RESOURCE_PICK, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        if (has_serf()) {
          int total_food = stock[0].requested + stock[0].available;
          if (total_food < stock[0].maximum) {
            player_t *player = game->get_player(get_owner());
            stock[0].prio = player->get_food_stonemine() >> (8 + total_food);
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case BUILDING_COALMINE:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_MINER,
                                        RESOURCE_PICK, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        if (has_serf()) {
          int total_food = stock[0].requested + stock[0].available;
          if (total_food < stock[0].maximum) {
            player_t *player = game->get_player(get_owner());
            stock[0].prio = player->get_food_coalmine() >> (8 + total_food);
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case BUILDING_IRONMINE:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_MINER,
                                        RESOURCE_PICK, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        if (has_serf()) {
          int total_food = stock[0].requested + stock[0].available;
          if (total_food < stock[0].maximum) {
            player_t *player = game->get_player(get_owner());
            stock[0].prio = player->get_food_ironmine() >> (8 + total_food);
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case BUILDING_GOLDMINE:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_MINER,
                                        RESOURCE_PICK, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        if (has_serf()) {
          int total_food = stock[0].requested + stock[0].available;
          if (total_food < stock[0].maximum) {
            player_t *player = game->get_player(get_owner());
            stock[0].prio = player->get_food_goldmine() >> (8 + total_food);
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case BUILDING_FORESTER:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_FORESTER,
                                        RESOURCE_NONE, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        break;
      case BUILDING_STOCK:
        if (!is_active()) {
          inventory_t *inv = game->create_inventory();
          if (inv == NULL) return;

          inv->set_owner(get_owner());
          inv->set_building_index(get_index());
          inv->set_flag_index(flag);

          u.inventory = inv;
          stock[0].requested = 0xff;
          stock[0].available = 0xff;
          stock[1].requested = 0xff;
          stock[1].available = 0xff;
          serf |= BIT(4);

          game->get_player(get_owner())->add_notification(7, pos, 0);
        } else {
          if (!serf_request_fail() &&
              !has_serf() &&
              !serf_requested()) {
            send_serf_to_building(this, SERF_TRANSPORTER,
                                  RESOURCE_NONE, RESOURCE_NONE);
          }

          player_t *player = game->get_player(get_owner());
          inventory_t *inv = u.inventory;
          if (has_serf() &&
              !inv->have_any_out_mode() == 0 && /* Not serf or res OUT mode */
              inv->free_serf_count() == 0) {
            if (player->tick_send_generic_delay()) {
              send_serf_to_building(this, SERF_GENERIC,
                                    RESOURCE_NONE, RESOURCE_NONE);
            }
          }

          /* TODO Following code looks like a hack */
          map_pos_t flag_pos = game->get_map()->move_down_right(pos);
          if (game->get_map()->get_serf_index(flag_pos) != 0) {
            serf_t *serf = game->get_serf_at_pos(flag_pos);
            if (serf->get_pos() != flag_pos) {
              game->get_map()->set_serf_index(flag_pos, 0);
            }
          }
        }
        break;
      case BUILDING_HUT:
      case BUILDING_TOWER:
      case BUILDING_FORTRESS:
        update_military();
        break;
      case BUILDING_FARM:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_FARMER,
                                        RESOURCE_SCYTHE, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        break;
      case BUILDING_BUTCHER:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_BUTCHER,
                                        RESOURCE_CLEAVER, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        if (has_serf()) {
          /* Request more of that delicious meat. */
          int total_stock = stock[0].requested + stock[0].available;
          if (total_stock < stock[0].maximum) {
            stock[0].prio = (0xff >> total_stock);
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case BUILDING_PIGFARM:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_PIGFARMER,
                                        RESOURCE_NONE, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        if (has_serf()) {
          /* Request more wheat. */
          int total_stock = stock[0].requested + stock[0].available;
          if (total_stock < stock[0].maximum) {
            player_t *player = game->get_player(get_owner());
            stock[0].prio = player->get_wheat_pigfarm() >> (8 + total_stock);
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case BUILDING_MILL:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_MILLER,
                                        RESOURCE_NONE, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        if (has_serf()) {
          /* Request more wheat. */
          int total_stock = stock[0].requested + stock[0].available;
          if (total_stock < stock[0].maximum) {
            player_t *player = game->get_player(get_owner());
            stock[0].prio = player->get_wheat_mill() >> (8 + total_stock);
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case BUILDING_BAKER:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_BAKER,
                                        RESOURCE_NONE, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        if (has_serf()) {
          /* Request more flour. */
          int total_stock = stock[0].requested + stock[0].available;
          if (total_stock < stock[0].maximum) {
            stock[0].prio = 0xff >> total_stock;
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case BUILDING_SAWMILL:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_SAWMILLER,
                                        RESOURCE_SAW, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        if (has_serf()) {
          /* Request more lumber */
          int total_stock = stock[1].requested + stock[1].available;
          if (total_stock < stock[1].maximum) {
            stock[1].prio = 0xff >> total_stock;
          } else {
            stock[1].prio = 0;
          }
        }
        break;
      case BUILDING_STEELSMELTER:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_SMELTER,
                                        RESOURCE_NONE, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        if (has_serf()) {
          /* Request more coal */
          int total_coal = stock[0].requested + stock[0].available;
          if (total_coal < stock[0].maximum) {
            player_t *player = game->get_player(get_owner());
            stock[0].prio = player->get_coal_steelsmelter() >> (8 + total_coal);
          } else {
            stock[0].prio = 0;
          }

          /* Request more iron ore */
          int total_ironore = stock[1].requested + stock[1].available;
          if (total_ironore < stock[1].maximum) {
            stock[1].prio = 0xff >> total_ironore;
          } else {
            stock[1].prio = 0;
          }
        }
        break;
      case BUILDING_TOOLMAKER:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_TOOLMAKER,
                                        RESOURCE_HAMMER, RESOURCE_SAW);
          if (r < 0) serf |= BIT(2);
        }
        if (has_serf()) {
          /* Request more planks. */
          player_t *player = game->get_player(get_owner());
          int total_tree = stock[0].requested + stock[0].available;
          if (total_tree < stock[0].maximum) {
            stock[0].prio = player->get_planks_toolmaker() >> (8 + total_tree);
          } else {
            stock[0].prio = 0;
          }

          /* Request more steel. */
          int total_steel = stock[1].requested + stock[1].available;
          if (total_steel < stock[1].maximum) {
            stock[1].prio = player->get_steel_toolmaker() >> (8 + total_steel);
          } else {
            stock[1].prio = 0;
          }
        }
        break;
      case BUILDING_WEAPONSMITH:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_WEAPONSMITH,
                                        RESOURCE_HAMMER, RESOURCE_PINCER);
          if (r < 0) serf |= BIT(2);
        }
        if (has_serf()) {
          /* Request more coal. */
          player_t *player = game->get_player(get_owner());
          int total_coal = stock[0].requested + stock[0].available;
          if (total_coal < stock[0].maximum) {
            stock[0].prio = player->get_coal_weaponsmith() >> (8 + total_coal);
          } else {
            stock[0].prio = 0;
          }

          /* Request more steel. */
          int total_steel = stock[1].requested + stock[1].available;
          if (total_steel < stock[1].maximum) {
            stock[1].prio =
              player->get_steel_weaponsmith() >> (8 + total_steel);
          } else {
            stock[1].prio = 0;
          }
        }
        break;
      case BUILDING_GOLDSMELTER:
        if (!serf_request_fail() &&
            !has_serf() &&
            !serf_requested()) {
          int r = send_serf_to_building(this, SERF_SMELTER,
                                        RESOURCE_NONE, RESOURCE_NONE);
          if (r < 0) serf |= BIT(2);
        }
        if (has_serf()) {
          /* Request more coal. */
          player_t *player = game->get_player(get_owner());
          int total_coal = stock[0].requested + stock[0].available;
          if (total_coal < stock[0].maximum) {
            stock[0].prio = player->get_coal_goldsmelter() >> (8 + total_coal);
          } else {
            stock[0].prio = 0;
          }

          /* Request more gold ore. */
          int total_goldore = stock[1].requested + stock[1].available;
          if (total_goldore < stock[1].maximum) {
            stock[1].prio = 0xff >> total_goldore;
          } else {
            stock[1].prio = 0;
          }
        }
        break;
      case BUILDING_CASTLE:
        update_castle();
        break;
      default:
        NOT_REACHED();
        break;
    }
  } else { /* Unfinished */
    switch (type) {
      case BUILDING_NONE:
      case BUILDING_CASTLE:
        break;
      case BUILDING_FISHER:
      case BUILDING_LUMBERJACK:
      case BUILDING_BOATBUILDER:
      case BUILDING_STONECUTTER:
      case BUILDING_STONEMINE:
      case BUILDING_COALMINE:
      case BUILDING_IRONMINE:
      case BUILDING_GOLDMINE:
      case BUILDING_FORESTER:
      case BUILDING_HUT:
      case BUILDING_MILL:
        update_unfinished();
        break;
      case BUILDING_STOCK:
      case BUILDING_FARM:
      case BUILDING_BUTCHER:
      case BUILDING_PIGFARM:
      case BUILDING_BAKER:
      case BUILDING_SAWMILL:
      case BUILDING_STEELSMELTER:
      case BUILDING_TOOLMAKER:
      case BUILDING_WEAPONSMITH:
      case BUILDING_TOWER:
      case BUILDING_FORTRESS:
      case BUILDING_GOLDSMELTER:
        update_unfinished_adv();
        break;
      default:
        NOT_REACHED();
        break;
    }
  }
}

/* Update unfinished building as part of the game progression. */
void
building_t::update_unfinished() {
  player_t *player = game->get_player(get_owner());

  /* Request builder serf */
  if (!serf_request_fail() &&
      !has_serf() &&
      !serf_requested()) {
    progress = 1;
    int r = send_serf_to_building(this, SERF_BUILDER,
                                  RESOURCE_HAMMER, RESOURCE_NONE);
    if (r < 0) serf |= BIT(2);
  }

  /* Request planks */
  int total_planks = stock[0].requested + stock[0].available;
  if (total_planks < stock[0].maximum) {
    int planks_prio = player->get_planks_construction() >> (8 + total_planks);
    if (!has_serf()) planks_prio >>= 2;
    stock[0].prio = planks_prio & ~BIT(0);
  } else {
    stock[0].prio = 0;
  }

  /* Request stone */
  int total_stone = stock[1].requested + stock[1].available;
  if (total_stone < stock[1].maximum) {
    int stone_prio = 0xff >> total_stone;
    if (!has_serf()) stone_prio >>= 2;
    stock[1].prio = stone_prio & ~BIT(0);
  } else {
    stock[1].prio = 0;
  }
}

void
building_t::update_unfinished_adv() {
  if (progress > 0) {
    update_unfinished();
    return;
  }

  if (has_serf() ||
      serf_requested()) {
    return;
  }

  /* Check whether building needs leveling */
  int need_leveling = 0;
  unsigned int height = game->get_leveling_height(pos);
  for (int i = 0; i < 7; i++) {
    map_pos_t pos = game->get_map()->pos_add_spirally(this->pos, i);
    if (game->get_map()->get_height(pos) != height) {
      need_leveling = 1;
      break;
    }
  }

  if (!need_leveling) {
    /* Already at the correct level, don't send digger */
    progress = 1;
    update_unfinished();
    return;
  }

  /* Request digger */
  if (!serf_request_fail()) {
    bool r = send_serf_to_building(this, SERF_DIGGER,
                                   RESOURCE_SHOVEL, RESOURCE_NONE);
    if (!r) serf |= BIT(2);
  }
}

/* Dispatch serf to building. */
bool
building_t::send_serf_to_building(building_t *building, serf_type_t type,
                                  resource_type_t res1, resource_type_t res2) {
  flag_t *dest = game->get_flag(building->flag);
  return game->send_serf_to_flag(dest, type, res1, res2);
}

/* Update castle as part of the game progression. */
void
building_t::update_castle() {
  player_t *player = game->get_player(get_owner());
  if (player->get_castle_knights() == player->get_castle_knights_wanted()) {
    serf_t *best_knight = NULL;
    serf_t *last_knight = NULL;
    int next_serf_index = this->serf_index;
    while (next_serf_index != 0) {
      serf_t *serf = game->get_serf(next_serf_index);
      assert(serf != NULL);
      if (best_knight == NULL ||
          serf->get_type() < best_knight->get_type()) {
        best_knight = serf;
      }
      last_knight = serf;
      next_serf_index = serf->get_next();
    }

    if (best_knight != NULL) {
      inventory_t *inventory = u.inventory;
      int type = best_knight->get_type();
      for (int t = SERF_KNIGHT_0; t <= SERF_KNIGHT_4; t++) {
        if (type > t) {
          inventory->call_internal(best_knight);
        }
      }

      /* Switch types */
      serf_type_t tmp = best_knight->get_type();
      best_knight->set_type(last_knight->get_type());
      last_knight->set_type(tmp);
    }
  } else if (player->get_castle_knights() <
             player->get_castle_knights_wanted()) {
    inventory_t *inventory = u.inventory;
    int type = -1;
    for (int t = SERF_KNIGHT_4; t >= SERF_KNIGHT_0; t--) {
      if (inventory->have_serf((serf_type_t)t)) {
        type = t;
        break;
      }
    }

    if (type < 0) {
      /* None found */
      if (inventory->have_serf(SERF_GENERIC) &&
          inventory->get_count_of(RESOURCE_SWORD) != 0 &&
          inventory->get_count_of(RESOURCE_SHIELD) != 0) {
        serf_t *serf = inventory->specialize_free_serf(SERF_KNIGHT_0);
        inventory->call_internal(serf);

        serf->add_to_defending_queue(serf_index, false);
        serf_index = serf->get_index();
        player->increase_castle_knights();
      } else {
        if (player->tick_send_knight_delay()) {
          send_serf_to_building(this, SERF_NONE, RESOURCE_NONE, RESOURCE_NONE);
        }
      }
    } else {
      /* Prepend to knights list */
      serf_t *serf = inventory->call_internal((serf_type_t)type);
      serf->add_to_defending_queue(serf_index, true);
      serf_index = serf->get_index();
      player->increase_castle_knights();
    }
  } else {
    player->decrease_castle_knights();

    int serf_index = this->serf_index;
    serf_t *serf = game->get_serf(serf_index);
    this->serf_index = serf->get_next();

    serf->stay_idle_in_stock(u.inventory->get_index());
  }

  inventory_t *inventory = u.inventory;

  if (has_serf() &&
      !inventory->have_any_out_mode() && /* Not serf or res OUT mode */
      inventory->free_serf_count() == 0) {
    if (player->tick_send_generic_delay()) {
      send_serf_to_building(this, SERF_GENERIC, RESOURCE_NONE, RESOURCE_NONE);
    }
  }

  map_pos_t flag_pos = game->get_map()->move_down_right(pos);
  if (game->get_map()->get_serf_index(flag_pos) != 0) {
    serf_t *serf = game->get_serf(game->get_map()->get_serf_index(flag_pos));
    if (serf->get_pos() != flag_pos) {
      game->get_map()->set_serf_index(flag_pos, 0);
    }
  }
}

void
building_t::update_military() {
  const int hut_occupants_from_level[] = {
    1, 1, 2, 2, 3,
    1, 1, 1, 1, 2
  };

  const int tower_occupants_from_level[] = {
    1, 2, 3, 4, 6,
    1, 1, 2, 3, 4
  };

  const int fortress_occupants_from_level[] = {
    1, 3, 6, 9, 12,
    1, 2, 4, 6, 8
  };

  player_t *player = game->get_player(get_owner());
  int max_occ_level = (player->get_knight_occupation(get_state()) >> 4) & 0xf;
  if (player->reduced_knight_level()) max_occ_level += 5;
  if (max_occ_level > 9) max_occ_level = 9;

  int needed_occupants = -1;
  int max_gold = -1;
  switch (get_type()) {
    case BUILDING_HUT:
      needed_occupants = hut_occupants_from_level[max_occ_level];
      max_gold = 2;
      break;
    case BUILDING_TOWER:
      needed_occupants = tower_occupants_from_level[max_occ_level];
      max_gold = 4;
      break;
    case BUILDING_FORTRESS:
      needed_occupants = fortress_occupants_from_level[max_occ_level];
      max_gold = 8;
      break;
    default:
      NOT_REACHED();
      break;
  }

  int total_knights = stock[0].requested + stock[0].available;
  int present_knights = stock[0].available;
  if (total_knights < needed_occupants) {
    if (!serf_request_fail()) {
      bool r = send_serf_to_building(this, SERF_NONE,
                                     RESOURCE_NONE, RESOURCE_NONE);
      if (!r) serf |= BIT(2);
    }
  } else if (needed_occupants < present_knights &&
             game->get_map()->get_serf_index(
                                  game->get_map()->move_down_right(pos)) == 0) {
    /* Kick least trained knight out. */
    serf_t *leaving_serf = NULL;
    int serf_index = this->serf_index;
    while (serf_index != 0) {
      serf_t *serf = game->get_serf(serf_index);
      assert(serf != NULL);
      if (leaving_serf == NULL ||
          serf->get_type() < leaving_serf->get_type()) {
        leaving_serf = serf;
      }
      serf_index = serf->get_next();
    }

    if (leaving_serf != NULL) {
      /* Remove leaving serf from list. */
      if (leaving_serf->get_index() == this->serf_index) {
        this->serf_index = leaving_serf->get_next();
      } else {
        serf_index = this->serf_index;
        while (serf_index != 0) {
          serf_t *serf = game->get_serf(serf_index);
          if (serf->get_next() == leaving_serf->get_index()) {
            serf->set_next(leaving_serf->get_next());
            break;
          }
          serf_index = serf->get_next();
        }
      }

      /* Update serf state. */
      leaving_serf->go_out_from_building(0, 0, -2);

      stock[0].available -= 1;
    }
  }

  /* Request gold */
  if (has_serf()) {
    int total_gold = stock[1].requested + stock[1].available;
    player->increase_military_max_gold(max_gold);

    if (total_gold < max_gold) {
      stock[1].prio = ((0xfe >> total_gold) + 1) & 0xfe;
    } else {
      stock[1].prio = 0;
    }
  }
}

int
building_get_score_from_type(building_type_t type) {
  const int building_score_from_type[] = {
    2, 2, 2, 2, 5, 5, 5, 5, 2, 10,
    3, 6, 4, 6, 5, 4, 7, 7, 9, 4,
    8, 15, 6, 20
  };

  return building_score_from_type[type-1];
}

save_reader_binary_t&
operator >> (save_reader_binary_t &reader, building_t &building) {
  uint32_t v32;
  reader >> v32;
  building.pos = v32;

  uint8_t v8;
  reader >> v8;  // 4
  building.type = (building_type_t)((v8 >> 2) & 0x1f);
  building.bld = v8 & 0x83;

  reader >> v8;  // 5
  building.serf = v8;

  uint16_t v16;
  reader >> v16;  // 6
  building.flag = v16;

  for (int i = 0; i < 2; i++) {
    reader >> v8;  // 8, 9
    building.stock[i].type = RESOURCE_NONE;
    building.stock[i].available = 0;
    building.stock[i].requested = 0;
    if (v8 != 0xFF) {
      building.stock[i].available = (v8 >> 4) & 0xf;
      building.stock[i].requested = v8 & 0xf;
    }
  }

  reader >> v16;  // 10
  building.serf_index = v16;
  reader >> v16;  // 12
  building.progress = v16;

  if (!building.is_burning() &&
      building.is_done() &&
      (building.get_type() == BUILDING_STOCK ||
      building.get_type() == BUILDING_CASTLE)) {
    reader >> v32;  // 14
    int offset = v32;
    building.u.inventory = building.game->create_inventory(offset/120);
    building.stock[0].requested = 0xff;
    return reader;
  } else {
    reader >> v16;  // 14
    building.u.level = v16;
  }

  if (!building.is_done()) {
    building.stock[0].type = RESOURCE_PLANK;
    reader >> v8;  // 16
    building.stock[0].maximum = v8;
    building.stock[1].type = RESOURCE_STONE;
    reader >> v8;  // 17
    building.stock[1].maximum = v8;
  } else if (building.has_serf()) {
    switch (building.get_type()) {
      case BUILDING_BOATBUILDER:
        building.stock[0].type = RESOURCE_PLANK;
        building.stock[0].maximum = 8;
        break;
      case BUILDING_STONEMINE:
      case BUILDING_COALMINE:
      case BUILDING_IRONMINE:
      case BUILDING_GOLDMINE:
        building.stock[0].type = RESOURCE_GROUP_FOOD;
        building.stock[0].maximum = 8;
        break;
      case BUILDING_HUT:
        building.stock[1].type = RESOURCE_GOLDBAR;
        building.stock[1].maximum = 2;
        break;
      case BUILDING_TOWER:
        building.stock[1].type = RESOURCE_GOLDBAR;
        building.stock[1].maximum = 4;
        break;
      case BUILDING_FORTRESS:
        building.stock[1].type = RESOURCE_GOLDBAR;
        building.stock[1].maximum = 8;
        break;
      case BUILDING_BUTCHER:
        building.stock[0].type = RESOURCE_PIG;
        building.stock[0].maximum = 8;
        break;
      case BUILDING_PIGFARM:
        building.stock[0].type = RESOURCE_WHEAT;
        building.stock[0].maximum = 8;
        break;
      case BUILDING_MILL:
        building.stock[0].type = RESOURCE_WHEAT;
        building.stock[0].maximum = 8;
        break;
      case BUILDING_BAKER:
        building.stock[0].type = RESOURCE_FLOUR;
        building.stock[0].maximum = 8;
        break;
      case BUILDING_SAWMILL:
        building.stock[1].type = RESOURCE_LUMBER;
        building.stock[1].maximum = 8;
        break;
      case BUILDING_STEELSMELTER:
        building.stock[0].type = RESOURCE_COAL;
        building.stock[0].maximum = 8;
        building.stock[1].type = RESOURCE_IRONORE;
        building.stock[1].maximum = 8;
        break;
      case BUILDING_TOOLMAKER:
        building.stock[0].type = RESOURCE_PLANK;
        building.stock[0].maximum = 8;
        building.stock[1].type = RESOURCE_STEEL;
        building.stock[1].maximum = 8;
        break;
      case BUILDING_WEAPONSMITH:
        building.stock[0].type = RESOURCE_COAL;
        building.stock[0].maximum = 8;
        building.stock[1].type = RESOURCE_STEEL;
        building.stock[1].maximum = 8;
        break;
      case BUILDING_GOLDSMELTER:
        building.stock[0].type = RESOURCE_COAL;
        building.stock[0].maximum = 8;
        building.stock[1].type = RESOURCE_GOLDORE;
        building.stock[1].maximum = 8;
        break;
      default:
        break;
    }
  }

  return reader;
}

save_reader_text_t&
operator >> (save_reader_text_t &reader, building_t &building) {
  unsigned int x = 0;
  unsigned int y = 0;
  reader.value("pos")[0] >> x;
  reader.value("pos")[1] >> y;
  building.pos = building.game->get_map()->pos(x, y);
  reader.value("type") >> building.type;
  reader.value("bld") >> building.bld;
  reader.value("serf") >> building.serf;
  reader.value("flag") >> building.flag;

  reader.value("stock[0].type") >> building.stock[0].type;
  reader.value("stock[0].prio") >> building.stock[0].prio;
  reader.value("stock[0].available") >> building.stock[0].available;
  reader.value("stock[0].requested") >> building.stock[0].requested;
  reader.value("stock[0].maximum") >> building.stock[0].maximum;

  reader.value("stock[1].type") >> building.stock[1].type;
  reader.value("stock[1].prio") >> building.stock[1].prio;
  reader.value("stock[1].available") >> building.stock[1].available;
  reader.value("stock[1].requested") >> building.stock[1].requested;
  reader.value("stock[1].maximum") >> building.stock[1].maximum;

  reader.value("serf_index") >> building.serf_index;
  reader.value("progress") >> building.progress;

  /* Load various values that depend on the building type. */
  /* TODO Check validity of pointers when loading. */
  if (!building.is_burning() &&
      (building.is_done() ||
       building.get_type() == BUILDING_CASTLE)) {
    if (building.get_type() == BUILDING_STOCK ||
        building.get_type() == BUILDING_CASTLE) {
      unsigned int inventory;
      reader.value("inventory") >> inventory;
      building.u.inventory = building.game->create_inventory(inventory);
    }
  } else if (building.is_burning()) {
    reader.value("tick") >> building.u.tick;
  } else {
    reader.value("level") >> building.u.level;
  }

  return reader;
}

save_writer_text_t&
operator << (save_writer_text_t &writer, building_t &building) {
  writer.value("pos") << building.game->get_map()->pos_col(building.pos);
  writer.value("pos") << building.game->get_map()->pos_row(building.pos);
  writer.value("type") << building.type;
  writer.value("bld") << building.bld;

  writer.value("serf") << building.serf;
  writer.value("flag") << building.flag;

  writer.value("stock[0].type") << building.stock[0].type;
  writer.value("stock[0].prio") << building.stock[0].prio;
  writer.value("stock[0].available") << building.stock[0].available;
  writer.value("stock[0].requested") << building.stock[0].requested;
  writer.value("stock[0].maximum") << building.stock[0].maximum;

  writer.value("stock[1].type") << building.stock[1].type;
  writer.value("stock[1].prio") << building.stock[1].prio;
  writer.value("stock[1].available") << building.stock[1].available;
  writer.value("stock[1].requested") << building.stock[1].requested;
  writer.value("stock[1].maximum") << building.stock[1].maximum;

  writer.value("serf_index") << building.serf_index;
  writer.value("progress") << building.progress;

  if (!building.is_burning() &&
      (building.is_done() ||
       building.get_type() == BUILDING_CASTLE)) {
    if (building.get_type() == BUILDING_STOCK ||
        building.get_type() == BUILDING_CASTLE) {
      writer.value("inventory") << building.u.inventory->get_index();
    }
  } else if (building.is_burning()) {
    writer.value("tick") << building.u.tick;
  } else {
    writer.value("level") << building.u.level;
  }

  return writer;
}
