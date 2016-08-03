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

Building::Building(Game *game, unsigned int index) : GameObject(game, index) {
  type = TypeNone;
  bld = BIT(7); /* bit 7: Unfinished building */
  flag = 0;
  serf = 0;

  for (int j = 0; j < BUILDING_MAX_STOCK; j++) {
    stock[j].type = Resource::TypeNone;
    stock[j].prio = 0;
    stock[j].available = 0;
    stock[j].requested = 0;
    stock[j].maximum = 0;
  }

  serf_index = 0;
}

Map::Object
Building::start_building(Type type) {
  const int construction_cost[] = {
    0, 0, 2, 0, 2, 0, 3, 0, 2, 0,
    4, 1, 5, 0, 5, 0, 5, 0,
    2, 0, 4, 3, 1, 1, 4, 1, 2, 1, 4, 1, 3, 1, 2, 1,
    3, 2, 3, 2, 3, 3, 2, 1, 2, 3, 5, 5, 4, 1
  };

  const Map::Object obj_types[] = {
    Map::ObjectNone,            // BUILDING_NONE
    Map::ObjectSmallBuilding,  // BUILDING_FISHER
    Map::ObjectSmallBuilding,  // BUILDING_LUMBERJACK
    Map::ObjectSmallBuilding,  // BUILDING_BOATBUILDER
    Map::ObjectSmallBuilding,  // BUILDING_STONECUTTER
    Map::ObjectSmallBuilding,  // BUILDING_STONEMINE
    Map::ObjectSmallBuilding,  // BUILDING_COALMINE
    Map::ObjectSmallBuilding,  // BUILDING_IRONMINE
    Map::ObjectSmallBuilding,  // BUILDING_GOLDMINE
    Map::ObjectSmallBuilding,  // BUILDING_FORESTER
    Map::ObjectLargeBuilding,  // BUILDING_STOCK
    Map::ObjectSmallBuilding,  // BUILDING_HUT
    Map::ObjectLargeBuilding,  // BUILDING_FARM
    Map::ObjectLargeBuilding,  // BUILDING_BUTCHER
    Map::ObjectLargeBuilding,  // BUILDING_PIGFARM
    Map::ObjectSmallBuilding,  // BUILDING_MILL
    Map::ObjectLargeBuilding,  // BUILDING_BAKER
    Map::ObjectLargeBuilding,  // BUILDING_SAWMILL
    Map::ObjectLargeBuilding,  // BUILDING_STEELSMELTER
    Map::ObjectLargeBuilding,  // BUILDING_TOOLMAKER
    Map::ObjectLargeBuilding,  // BUILDING_WEAPONSMITH
    Map::ObjectLargeBuilding,  // BUILDING_TOWER
    Map::ObjectLargeBuilding,  // BUILDING_FORTRESS
    Map::ObjectLargeBuilding,  // BUILDING_GOLDSMELTER
    Map::ObjectCastle,          // BUILDING_CASTLE
  };

  Map::Object map_obj = obj_types[type];

  set_type(type);
  progress = 0;
  if (map_obj != Map::ObjectLargeBuilding) progress = 1;

  if (type == TypeCastle) {
    stock[0].available = 0xff;
    stock[0].requested = 0xff;
    stock[1].available = 0xff;
    stock[1].requested = 0xff;
  } else {
    stock[0].type = Resource::TypePlank;
    stock[0].prio = 0;
    stock[0].maximum = construction_cost[2*type];
    stock[1].type = Resource::TypeStone;
    stock[1].prio = 0;
    stock[1].maximum = construction_cost[2*type+1];
  }

  return map_obj;
}

Serf*
Building::call_defender_out() {
  /* Remove knight from stats of defending building */
  if (has_inventory()) { /* Castle */
    game->get_player(get_owner())->decrease_castle_knights();
  } else {
    stock[0].available -= 1;
    stock[0].requested += 1;
  }

  /* The last knight in the list has to defend. */
  Serf *first_serf = game->get_serf(serf_index);
  Serf *def_serf = first_serf->extract_last_knight_from_list();

  if (def_serf->get_index() == serf_index) {
    serf_index = 0;
  }

  return def_serf;
}

Serf*
Building::call_attacker_out(int knight_index) {
  stock[0].available -= 1;

  /* Unlink knight from list. */
  Serf *first_serf = game->get_serf(serf_index);
  Serf *def_serf = first_serf->extract_last_knight_from_list();

  if (def_serf->get_index() == serf_index) {
    serf_index = 0;
  }

  return def_serf;
}

unsigned int
Building::military_gold_count() {
  unsigned int count = 0;

  if (get_type() == TypeHut ||
      get_type() == TypeTower ||
      get_type() == TypeFortress) {
    for (int j = 0; j < BUILDING_MAX_STOCK; j++) {
      if (stock[j].type == Resource::TypeGoldBar) {
        count += stock[j].available;
      }
    }
  }

  return count;
}

void
Building::cancel_transported_resource(Resource::Type res) {
  if (res == Resource::TypeFish ||
      res == Resource::TypeMeat ||
      res == Resource::TypeBread) {
    res = Resource::GroupFood;
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
Building::add_requested_resource(Resource::Type res, bool fix_priority) {
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
Building::stock_init(unsigned int stock_num, Resource::Type type,
                       unsigned int maximum) {
  stock[stock_num].type = type;
  stock[stock_num].prio = 0;
  stock[stock_num].maximum = maximum;
}

void
Building::requested_resource_delivered(Resource::Type resource) {
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
Building::requested_knight_arrived() {
  stock[0].available += 1;
  stock[0].requested -= 1;
}

bool
Building::is_enough_place_for_knight() {
  int max_capacity = -1;
  switch (get_type()) {
    case TypeHut: max_capacity = 3; break;
    case TypeTower: max_capacity = 6; break;
    case TypeFortress: max_capacity = 12; break;
    default: NOT_REACHED(); break;
  }

  int total_knights = stock[0].requested + stock[0].available;
  return (total_knights < max_capacity);
}

bool
Building::knight_come_back_from_fight(Serf *knight) {
  if (is_enough_place_for_knight()) {
    stock[0].available += 1;
    Serf *serf = game->get_serf(serf_index);
    knight->insert_before(serf);
    set_main_serf(knight->get_index());
    return true;
  }

  return false;
}

void
Building::knight_occupy() {
  if (!has_main_serf()) {
    stock[0].available = 0;
    stock[0].requested = 1;
  } else {
    stock[0].requested += 1;
  }
}

void
Building::update(unsigned int tick) {
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
Building::knight_request_granted() {
  stock[0].requested += 1;
  serf_request_complete();
}

void
Building::remove_stock() {
  stock[0].available = 0;
  stock[0].requested = 0;
  stock[1].available = 0;
  stock[1].requested = 0;
}

int
Building::get_max_priority_for_resource(Resource::Type resource, int minimum) {
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
Building::use_resource_in_stock(int stock_num) {
  if (stock[stock_num].available > 0) {
    stock[stock_num].available -= 1;
    return true;
  }
  return false;
}

bool
Building::use_resources_in_stocks() {
  if (stock[0].available > 0 &&
      stock[1].available > 0) {
    stock[0].available -= 1;
    stock[1].available -= 1;
    return true;
  }
  return false;
}

void
Building::update() {
  if (is_done()) {
    switch (get_type()) {
      case TypeNone:
        break;
      case TypeFisher:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeFisher,
                                         Resource::TypeRod,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
        }
        break;
      case TypeLumberjack:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeLumberjack,
                                         Resource::TypeAxe,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
        }
        break;
      case TypeBoatbuilder:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeBoatBuilder,
                                         Resource::TypeHammer,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
        }
        if (has_serf()) {
          Player *player = game->get_player(get_owner());
          int total_tree = stock[0].requested + stock[0].available;
          if (total_tree < stock[0].maximum) {
            stock[0].prio =
              player->get_planks_boatbuilder() >> (8 + total_tree);
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case TypeStonecutter:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeStonecutter,
                                         Resource::TypePick,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
        }
        break;
      case TypeStoneMine:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeMiner,
                                         Resource::TypePick,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
        }
        if (has_serf()) {
          int total_food = stock[0].requested + stock[0].available;
          if (total_food < stock[0].maximum) {
            Player *player = game->get_player(get_owner());
            stock[0].prio = player->get_food_stonemine() >> (8 + total_food);
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case TypeCoalMine:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeMiner,
                                         Resource::TypePick,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
        }
        if (has_serf()) {
          int total_food = stock[0].requested + stock[0].available;
          if (total_food < stock[0].maximum) {
            Player *player = game->get_player(get_owner());
            stock[0].prio = player->get_food_coalmine() >> (8 + total_food);
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case TypeIronMine:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeMiner,
                                         Resource::TypePick,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
        }
        if (has_serf()) {
          int total_food = stock[0].requested + stock[0].available;
          if (total_food < stock[0].maximum) {
            Player *player = game->get_player(get_owner());
            stock[0].prio = player->get_food_ironmine() >> (8 + total_food);
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case TypeGoldMine:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeMiner,
                                         Resource::TypePick,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
        }
        if (has_serf()) {
          int total_food = stock[0].requested + stock[0].available;
          if (total_food < stock[0].maximum) {
            Player *player = game->get_player(get_owner());
            stock[0].prio = player->get_food_goldmine() >> (8 + total_food);
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case TypeForester:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeForester,
                                         Resource::TypeNone,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
        }
        break;
      case TypeStock:
        if (!is_active()) {
          Inventory *inv = game->create_inventory();
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
          if (!serf_request_fail() && !has_serf() && !serf_requested()) {
            send_serf_to_building(this, Serf::TypeTransporter,
                                  Resource::TypeNone, Resource::TypeNone);
          }

          Player *player = game->get_player(get_owner());
          Inventory *inv = u.inventory;
          if (has_serf() &&
              !inv->have_any_out_mode() == 0 && /* Not serf or res OUT mode */
              inv->free_serf_count() == 0) {
            if (player->tick_send_generic_delay()) {
              send_serf_to_building(this, Serf::TypeGeneric,
                                    Resource::TypeNone, Resource::TypeNone);
            }
          }

          /* TODO Following code looks like a hack */
          MapPos flag_pos = game->get_map()->move_down_right(pos);
          if (game->get_map()->has_serf(flag_pos)) {
            Serf *serf = game->get_serf_at_pos(flag_pos);
            if (serf->get_pos() != flag_pos) {
              game->get_map()->set_serf_index(flag_pos, 0);
            }
          }
        }
        break;
      case TypeHut:
      case TypeTower:
      case TypeFortress:
        update_military();
        break;
      case TypeFarm:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeFarmer,
                                         Resource::TypeScythe,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
        }
        break;
      case TypeButcher:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeButcher,
                                         Resource::TypeCleaver,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
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
      case TypePigFarm:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypePigFarmer,
                                         Resource::TypeNone,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
        }
        if (has_serf()) {
          /* Request more wheat. */
          int total_stock = stock[0].requested + stock[0].available;
          if (total_stock < stock[0].maximum) {
            Player *player = game->get_player(get_owner());
            stock[0].prio = player->get_wheat_pigfarm() >> (8 + total_stock);
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case TypeMill:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeMiller,
                                         Resource::TypeNone,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
        }
        if (has_serf()) {
          /* Request more wheat. */
          int total_stock = stock[0].requested + stock[0].available;
          if (total_stock < stock[0].maximum) {
            Player *player = game->get_player(get_owner());
            stock[0].prio = player->get_wheat_mill() >> (8 + total_stock);
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case TypeBaker:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeBaker,
                                         Resource::TypeNone,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
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
      case TypeSawmill:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeSawmiller,
                                         Resource::TypeSaw,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
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
      case TypeSteelSmelter:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeSmelter,
                                         Resource::TypeNone,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
        }
        if (has_serf()) {
          /* Request more coal */
          int total_coal = stock[0].requested + stock[0].available;
          if (total_coal < stock[0].maximum) {
            Player *player = game->get_player(get_owner());
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
      case TypeToolMaker:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeToolmaker,
                                         Resource::TypeHammer,
                                         Resource::TypeSaw);
          if (!r) serf |= BIT(2);
        }
        if (has_serf()) {
          /* Request more planks. */
          Player *player = game->get_player(get_owner());
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
      case TypeWeaponSmith:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeWeaponSmith,
                                         Resource::TypeHammer,
                                         Resource::TypePincer);
          if (!r) serf |= BIT(2);
        }
        if (has_serf()) {
          /* Request more coal. */
          Player *player = game->get_player(get_owner());
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
      case TypeGoldSmelter:
        if (!serf_request_fail() && !has_serf() && !serf_requested()) {
          bool r = send_serf_to_building(this, Serf::TypeSmelter,
                                         Resource::TypeNone,
                                         Resource::TypeNone);
          if (!r) serf |= BIT(2);
        }
        if (has_serf()) {
          /* Request more coal. */
          Player *player = game->get_player(get_owner());
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
      case TypeCastle:
        update_castle();
        break;
      default:
        NOT_REACHED();
        break;
    }
  } else { /* Unfinished */
    switch (type) {
      case TypeNone:
      case TypeCastle:
        break;
      case TypeFisher:
      case TypeLumberjack:
      case TypeBoatbuilder:
      case TypeStonecutter:
      case TypeStoneMine:
      case TypeCoalMine:
      case TypeIronMine:
      case TypeGoldMine:
      case TypeForester:
      case TypeHut:
      case TypeMill:
        update_unfinished();
        break;
      case TypeStock:
      case TypeFarm:
      case TypeButcher:
      case TypePigFarm:
      case TypeBaker:
      case TypeSawmill:
      case TypeSteelSmelter:
      case TypeToolMaker:
      case TypeWeaponSmith:
      case TypeTower:
      case TypeFortress:
      case TypeGoldSmelter:
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
Building::update_unfinished() {
  Player *player = game->get_player(get_owner());

  /* Request builder serf */
  if (!serf_request_fail() &&
      !has_serf() &&
      !serf_requested()) {
    progress = 1;
    bool r = send_serf_to_building(this, Serf::TypeBuilder,
                                   Resource::TypeHammer, Resource::TypeNone);
    if (!r) serf |= BIT(2);
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
Building::update_unfinished_adv() {
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
    MapPos pos = game->get_map()->pos_add_spirally(this->pos, i);
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
    bool r = send_serf_to_building(this, Serf::TypeDigger,
                                   Resource::TypeShovel, Resource::TypeNone);
    if (!r) serf |= BIT(2);
  }
}

/* Dispatch serf to building. */
bool
Building::send_serf_to_building(Building *building, Serf::Type type,
                                Resource::Type res1, Resource::Type res2) {
  Flag *dest = game->get_flag(building->flag);
  return game->send_serf_to_flag(dest, type, res1, res2);
}

/* Update castle as part of the game progression. */
void
Building::update_castle() {
  Player *player = game->get_player(get_owner());
  if (player->get_castle_knights() == player->get_castle_knights_wanted()) {
    Serf *best_knight = NULL;
    Serf *last_knight = NULL;
    int next_serf_index = this->serf_index;
    while (next_serf_index != 0) {
      Serf *serf = game->get_serf(next_serf_index);
      assert(serf != NULL);
      if (best_knight == NULL ||
          serf->get_type() < best_knight->get_type()) {
        best_knight = serf;
      }
      last_knight = serf;
      next_serf_index = serf->get_next();
    }

    if (best_knight != NULL) {
      Inventory *inventory = u.inventory;
      int type = best_knight->get_type();
      for (int t = Serf::TypeKnight0; t <= Serf::TypeKnight4; t++) {
        if (type > t) {
          inventory->call_internal(best_knight);
        }
      }

      /* Switch types */
      Serf::Type tmp = best_knight->get_type();
      best_knight->set_type(last_knight->get_type());
      last_knight->set_type(tmp);
    }
  } else if (player->get_castle_knights() <
             player->get_castle_knights_wanted()) {
    Inventory *inventory = u.inventory;
    int type = -1;
    for (int t = Serf::TypeKnight4; t >= Serf::TypeKnight0; t--) {
      if (inventory->have_serf((Serf::Type)t)) {
        type = t;
        break;
      }
    }

    if (type < 0) {
      /* None found */
      if (inventory->have_serf(Serf::TypeGeneric) &&
          inventory->get_count_of(Resource::TypeSword) != 0 &&
          inventory->get_count_of(Resource::TypeShield) != 0) {
        Serf *serf = inventory->specialize_free_serf(Serf::TypeKnight0);
        inventory->call_internal(serf);

        serf->add_to_defending_queue(serf_index, false);
        serf_index = serf->get_index();
        player->increase_castle_knights();
      } else {
        if (player->tick_send_knight_delay()) {
          send_serf_to_building(this, Serf::TypeNone,
                                Resource::TypeNone, Resource::TypeNone);
        }
      }
    } else {
      /* Prepend to knights list */
      Serf *serf = inventory->call_internal((Serf::Type)type);
      serf->add_to_defending_queue(serf_index, true);
      serf_index = serf->get_index();
      player->increase_castle_knights();
    }
  } else {
    player->decrease_castle_knights();

    int serf_index = this->serf_index;
    Serf *serf = game->get_serf(serf_index);
    this->serf_index = serf->get_next();

    serf->stay_idle_in_stock(u.inventory->get_index());
  }

  Inventory *inventory = u.inventory;

  if (has_serf() &&
      !inventory->have_any_out_mode() && /* Not serf or res OUT mode */
      inventory->free_serf_count() == 0) {
    if (player->tick_send_generic_delay()) {
      send_serf_to_building(this, Serf::TypeGeneric, Resource::TypeNone,
                            Resource::TypeNone);
    }
  }

  MapPos flag_pos = game->get_map()->move_down_right(pos);
  if (game->get_map()->has_serf(flag_pos)) {
    Serf *serf = game->get_serf_at_pos(flag_pos);
    if (serf->get_pos() != flag_pos) {
      game->get_map()->set_serf_index(flag_pos, 0);
    }
  }
}

void
Building::update_military() {
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

  Player *player = game->get_player(get_owner());
  int max_occ_level = (player->get_knight_occupation(get_state()) >> 4) & 0xf;
  if (player->reduced_knight_level()) max_occ_level += 5;
  if (max_occ_level > 9) max_occ_level = 9;

  int needed_occupants = -1;
  int max_gold = -1;
  switch (get_type()) {
    case TypeHut:
      needed_occupants = hut_occupants_from_level[max_occ_level];
      max_gold = 2;
      break;
    case TypeTower:
      needed_occupants = tower_occupants_from_level[max_occ_level];
      max_gold = 4;
      break;
    case TypeFortress:
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
      bool r = send_serf_to_building(this, Serf::TypeNone,
                                     Resource::TypeNone, Resource::TypeNone);
      if (!r) serf |= BIT(2);
    }
  } else if (needed_occupants < present_knights &&
             !game->get_map()->has_serf(
                                       game->get_map()->move_down_right(pos))) {
    /* Kick least trained knight out. */
    Serf *leaving_serf = NULL;
    int serf_index = this->serf_index;
    while (serf_index != 0) {
      Serf *serf = game->get_serf(serf_index);
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
          Serf *serf = game->get_serf(serf_index);
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
building_get_score_from_type(Building::Type type) {
  const int building_score_from_type[] = {
    2, 2, 2, 2, 5, 5, 5, 5, 2, 10,
    3, 6, 4, 6, 5, 4, 7, 7, 9, 4,
    8, 15, 6, 20
  };

  return building_score_from_type[type-1];
}

SaveReaderBinary&
operator >> (SaveReaderBinary &reader, Building &building) {
  uint32_t v32;
  reader >> v32;
  building.pos = v32;

  uint8_t v8;
  reader >> v8;  // 4
  building.type = (Building::Type)((v8 >> 2) & 0x1f);
  building.bld = v8 & 0x83;

  reader >> v8;  // 5
  building.serf = v8;

  uint16_t v16;
  reader >> v16;  // 6
  building.flag = v16;

  for (int i = 0; i < 2; i++) {
    reader >> v8;  // 8, 9
    building.stock[i].type = Resource::TypeNone;
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
      (building.get_type() == Building::TypeStock ||
      building.get_type() == Building::TypeCastle)) {
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
    building.stock[0].type = Resource::TypePlank;
    reader >> v8;  // 16
    building.stock[0].maximum = v8;
    building.stock[1].type = Resource::TypeStone;
    reader >> v8;  // 17
    building.stock[1].maximum = v8;
  } else if (building.has_serf()) {
    switch (building.get_type()) {
      case Building::TypeBoatbuilder:
        building.stock[0].type = Resource::TypePlank;
        building.stock[0].maximum = 8;
        break;
      case Building::TypeStoneMine:
      case Building::TypeCoalMine:
      case Building::TypeIronMine:
      case Building::TypeGoldMine:
        building.stock[0].type = Resource::GroupFood;
        building.stock[0].maximum = 8;
        break;
      case Building::TypeHut:
        building.stock[1].type = Resource::TypeGoldBar;
        building.stock[1].maximum = 2;
        break;
      case Building::TypeTower:
        building.stock[1].type = Resource::TypeGoldBar;
        building.stock[1].maximum = 4;
        break;
      case Building::TypeFortress:
        building.stock[1].type = Resource::TypeGoldBar;
        building.stock[1].maximum = 8;
        break;
      case Building::TypeButcher:
        building.stock[0].type = Resource::TypePig;
        building.stock[0].maximum = 8;
        break;
      case Building::TypePigFarm:
        building.stock[0].type = Resource::TypeWheat;
        building.stock[0].maximum = 8;
        break;
      case Building::TypeMill:
        building.stock[0].type = Resource::TypeWheat;
        building.stock[0].maximum = 8;
        break;
      case Building::TypeBaker:
        building.stock[0].type = Resource::TypeFlour;
        building.stock[0].maximum = 8;
        break;
      case Building::TypeSawmill:
        building.stock[1].type = Resource::TypeLumber;
        building.stock[1].maximum = 8;
        break;
      case Building::TypeSteelSmelter:
        building.stock[0].type = Resource::TypeCoal;
        building.stock[0].maximum = 8;
        building.stock[1].type = Resource::TypeIronOre;
        building.stock[1].maximum = 8;
        break;
      case Building::TypeToolMaker:
        building.stock[0].type = Resource::TypePlank;
        building.stock[0].maximum = 8;
        building.stock[1].type = Resource::TypeSteel;
        building.stock[1].maximum = 8;
        break;
      case Building::TypeWeaponSmith:
        building.stock[0].type = Resource::TypeCoal;
        building.stock[0].maximum = 8;
        building.stock[1].type = Resource::TypeSteel;
        building.stock[1].maximum = 8;
        break;
      case Building::TypeGoldSmelter:
        building.stock[0].type = Resource::TypeCoal;
        building.stock[0].maximum = 8;
        building.stock[1].type = Resource::TypeGoldOre;
        building.stock[1].maximum = 8;
        break;
      default:
        break;
    }
  }

  return reader;
}

SaveReaderText&
operator >> (SaveReaderText &reader, Building &building) {
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
      (building.is_done() || building.get_type() == Building::TypeCastle)) {
    if (building.get_type() == Building::TypeStock ||
        building.get_type() == Building::TypeCastle) {
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

SaveWriterText&
operator << (SaveWriterText &writer, Building &building) {
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
      (building.is_done() || building.get_type() == Building::TypeCastle)) {
    if (building.get_type() == Building::TypeStock ||
        building.get_type() == Building::TypeCastle) {
      writer.value("inventory") << building.u.inventory->get_index();
    }
  } else if (building.is_burning()) {
    writer.value("tick") << building.u.tick;
  } else {
    writer.value("level") << building.u.level;
  }

  return writer;
}
