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

#include "src/game.h"
#include "src/inventory.h"
#include "src/debug.h"
#include "src/savegame.h"

Building::Building(Game *game, unsigned int index)
  : GameObject(game, index)
  , stock{} {
  type = TypeNone;
  constructing = true; /* Unfinished building */
  flag = 0;
  playing_sfx = false;
  threat_level = 0;
  owner = 0;
  serf_requested = false;
  serf_request_failed = false;
  burning = false;
  active = false;
  holder = false;
  pos = 0;
  progress = 0;
  u = { 0 };
  inventory = nullptr;

  for (int j = 0; j < kMaxStock; j++) {
    stock[j].type = Resource::TypeNone;
    stock[j].prio = 0;
    stock[j].available = 0;
    stock[j].requested = 0;
    stock[j].maximum = 0;
  }

  first_knight = 0;
  burning_counter = 0;
}

typedef struct ConstructionInfo {
  Map::Object map_obj;
  int planks;
  int stones;
  int phase_1;
  int phase_2;
} ConstructionInfo;

const ConstructionInfo const_info[] = {
  { Map::ObjectNone,          0, 0,    0,    0},  // BUILDING_NONE
  { Map::ObjectSmallBuilding, 2, 0, 4096, 4096},  // BUILDING_FISHER
  { Map::ObjectSmallBuilding, 2, 0, 4096, 4096},  // BUILDING_LUMBERJACK
  { Map::ObjectSmallBuilding, 3, 0, 4096, 2048},  // BUILDING_BOATBUILDER
  { Map::ObjectSmallBuilding, 2, 0, 4096, 4096},  // BUILDING_STONECUTTER
  { Map::ObjectSmallBuilding, 4, 1, 2048, 1366},  // BUILDING_STONEMINE
  { Map::ObjectSmallBuilding, 5, 0, 2048, 1366},  // BUILDING_COALMINE
  { Map::ObjectSmallBuilding, 5, 0, 2048, 1366},  // BUILDING_IRONMINE
  { Map::ObjectSmallBuilding, 5, 0, 2048, 1366},  // BUILDING_GOLDMINE
  { Map::ObjectSmallBuilding, 2, 0, 4096, 4096},  // BUILDING_FORESTER
  { Map::ObjectLargeBuilding, 4, 3, 1366, 1024},  // BUILDING_STOCK
  { Map::ObjectSmallBuilding, 1, 1, 4096, 4096},  // BUILDING_HUT
  { Map::ObjectLargeBuilding, 4, 1, 2048, 1366},  // BUILDING_FARM
  { Map::ObjectLargeBuilding, 2, 1, 4096, 2048},  // BUILDING_BUTCHER
  { Map::ObjectLargeBuilding, 4, 1, 2048, 1366},  // BUILDING_PIGFARM
  { Map::ObjectSmallBuilding, 3, 1, 2048, 2048},  // BUILDING_MILL
  { Map::ObjectLargeBuilding, 2, 1, 4096, 2048},  // BUILDING_BAKER
  { Map::ObjectLargeBuilding, 3, 2, 2048, 1366},  // BUILDING_SAWMILL
  { Map::ObjectLargeBuilding, 3, 2, 2048, 1366},  // BUILDING_STEELSMELTER
  { Map::ObjectLargeBuilding, 3, 3, 2048, 1024},  // BUILDING_TOOLMAKER
  { Map::ObjectLargeBuilding, 2, 1, 4096, 2048},  // BUILDING_WEAPONSMITH
  { Map::ObjectLargeBuilding, 2, 3, 2048, 1366},  // BUILDING_TOWER
  { Map::ObjectLargeBuilding, 5, 5, 1024,  683},  // BUILDING_FORTRESS
  { Map::ObjectLargeBuilding, 4, 1, 2048, 1366},  // BUILDING_GOLDSMELTER
  { Map::ObjectCastle,        0, 0,  256,  256},  // BUILDING_CASTLE
};

Map::Object
Building::start_building(Building::Type _type) {
  type = _type;
  Map::Object map_obj = const_info[type].map_obj;
  progress = (map_obj == Map::ObjectLargeBuilding) ? 0 : 1;

  if (type == TypeCastle) {
    active = true;
    holder = true;

    stock[0].available = 0xff;
    stock[0].requested = 0xff;
    stock[1].available = 0xff;
    stock[1].requested = 0xff;
  } else {
    stock[0].type = Resource::TypePlank;
    stock[0].prio = 0;
    stock[0].maximum = const_info[type].planks;
    stock[1].type = Resource::TypeStone;
    stock[1].prio = 0;
    stock[1].maximum = const_info[type].stones;
  }

  return map_obj;
}

void
Building::done_leveling() {
  progress = 1;
  holder = false;
  first_knight = 0;
}

bool
Building::build_progress() {
  int frame_finished = !!BIT_TEST(progress, 15);
  progress += (frame_finished == 0) ? const_info[type].phase_1
                                    : const_info[type].phase_2;

  if (progress <= 0xffff) {
    // Not finished yet
    return false;
  }

  progress = 0;
  constructing = false; /* Building finished */
  first_knight = 0;

  if (type == TypeCastle) {
    return true;
  }

  holder = false;

  if (is_military()) {
    update_military_flag_state();
  }

  Flag *f = game->get_flag(get_flag_index());

  stock_init(0, Resource::TypeNone, 0);
  stock_init(1, Resource::TypeNone, 0);
  f->clear_flags();

  /* Update player fields. */
  Player *player = game->get_player(owner);
  player->building_built(this);

  return true;
}

void
Building::increase_mining(int res) {
  active = true;

  if (progress == 0x8000) {
    /* Handle empty mine. */
    Player *player = game->get_player(owner);
    if (player->is_ai()) {
      /* TODO Burn building. */
    }

    player->add_notification(Message::TypeMineEmpty, pos, type - TypeStoneMine);
  }

  progress = (progress << 1) & 0xffff;
  if (res > 0) {
    progress++;
  }
}

void
Building::set_first_knight(unsigned int serf) {
  first_knight = serf;

  /* Test whether building is already occupied by knights */
  if (!active) {
    active = true;

    int mil_type = -1;
    int max_gold = -1;
    switch (type) {
      case TypeHut:
        mil_type = 0;
        max_gold = 2;
        break;
      case TypeTower:
        mil_type = 1;
        max_gold = 4;
        break;
      case TypeFortress:
        mil_type = 2;
        max_gold = 8;
        break;
      default:
        NOT_REACHED();
        break;
    }

    game->get_player(owner)->add_notification(Message::TypeKnightOccupied, pos,
                                              mil_type);

    Flag *f = game->get_flag_at_pos(game->get_map()->move_down_right(pos));
    f->clear_flags();
    stock_init(1, Resource::TypeGoldBar, max_gold);
    game->building_captured(this);
  }
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
  Serf *first_serf = game->get_serf(first_knight);
  Serf *def_serf = first_serf->extract_last_knight_from_list();

  if (def_serf->get_index() == first_knight) {
    first_knight = 0;
  }

  return def_serf;
}

Serf*
Building::call_attacker_out(int) {
  stock[0].available -= 1;

  /* Unlink knight from list. */
  Serf *first_serf = game->get_serf(first_knight);
  Serf *def_serf = first_serf->extract_last_knight_from_list();

  if (def_serf->get_index() == first_knight) {
    first_knight = 0;
  }

  return def_serf;
}

unsigned int
Building::military_gold_count() const {
  unsigned int count = 0;

  if (get_type() == TypeHut ||
      get_type() == TypeTower ||
      get_type() == TypeFortress) {
    for (int j = 0; j < kMaxStock; j++) {
      if (stock[j].type == Resource::TypeGoldBar) {
        count += stock[j].available;
      }
    }
  }

  return count;
}

void
Building::cancel_transported_resource(Resource::Type res) {
  int in_stock = -1;
  for (int i = 0; i < kMaxStock; i++) {
    if (stock[i].type == res) {
      in_stock = i;
      break;
    }
  }
  if ((in_stock < 0) && (res == Resource::TypeFish || res == Resource::TypeMeat || res == Resource::TypeBread)) {
	Log::Info["Building::cancel_transported_resource"] << "fixing food " << res;
	for (int i = 0; i < kMaxStock; i++) {
	  if ((stock[i].type == Resource::TypeFish && stock[i].requested > 0) ||
          (stock[i].type == Resource::TypeMeat && stock[i].requested > 0) ||
	      (stock[i].type == Resource::TypeBread && stock[i].requested > 0)) {
        in_stock = i;
        break;
	  }
    }
  }
  if (in_stock >= 0) {
	if (stock[in_stock].requested > 0) {
	  Log::Info["building"] << "cancel_transported_resource - stock[in_stock].requested - 1";
      stock[in_stock].requested -= 1;
	}
    if (stock[in_stock].requested < 0) {
		//p1plp1_food_type_unrequested_resource_fix
	  Log::Info["building"] << "cancel_transported_resource - Failed to cancel unrequested resource delivery" << res << " stock index " << in_stock << " requested " << stock[in_stock].requested;
      throw ExceptionFreeserf("Failed to cancel unrequested resource delivery.");
    }
  }
}

bool
Building::add_requested_resource(Resource::Type res, bool fix_priority) {
  for (int j = 0; j < kMaxStock; j++) {
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
Building::stock_init(unsigned int stock_num, Resource::Type res_type,
                     unsigned int maximum) {
  stock[stock_num].type = res_type;
  stock[stock_num].prio = 0;
  stock[stock_num].maximum = maximum;
}

void
Building::requested_resource_delivered(Resource::Type resource) {
  if (burning) {
    return;
  }
  if (has_inventory()) {
    inventory->push_resource(resource);
  } else {
    /* Add to building stock */
    for (int i = 0; i < kMaxStock; i++) {
      if ((stock[i].type == Resource::GroupFood) && (resource == Resource::TypeFish ||
        resource == Resource::TypeMeat || resource == Resource::TypeBread)) {
         stock[i].available += 1;
         stock[i].requested -= 1;
         if (stock[i].requested < 0) {
           stock[i].requested = 0;
           Log::Debug["building"] << "Building::requested_resource_delivered, Fixing req res delivered FOOD type requested below zero.";
         }
         Log::Debug["building"] << "Building::requested_resource_delivered, Fixing req res delivered FOOD type.";
         return;
      }
      if (stock[i].type == resource) {
        if (stock[i].requested > 0) {
          stock[i].available += 1;
          stock[i].requested -= 1;
        } else {
          Log::Debug["building"] << "Building::requested_resource_delivered, Delivered more resources than requested: " << stock[i].requested << " available: "<< stock[i].available << " of type: " << resource;
          return;
        }
        return;
      }
    }
    Log::Debug["building"] << "Building::requested_resource_delivered, Delivered unexpected resource: " << resource;
    throw ExceptionFreeserf("Delivered unexpected resource.");
  }
}

void
Building::requested_knight_arrived() {
  stock[0].available += 1;
  stock[0].requested -= 1;
}

bool
Building::is_enough_place_for_knight() const {
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
    Serf *serf = game->get_serf(first_knight);
    knight->insert_before(serf);
    first_knight = knight->get_index();
    return true;
  }

  return false;
}

void
Building::knight_occupy() {
  if (!has_knight()) {
    stock[0].available = 0;
    stock[0].requested = 1;
  } else {
    stock[0].requested += 1;
  }
}

bool
Building::burnup() {
  if (is_burning()) {
    return true;
  }

  burning = true;

  /* Remove lost gold stock from total count. */
  if (!constructing &&
      (get_type() == TypeHut ||
       get_type() == TypeTower ||
       get_type() == TypeFortress ||
       get_type() == TypeGoldSmelter)) {
        int gold_stock = get_res_count_in_stock(1);
        game->add_gold_total(-gold_stock);
  }

  /* Update land owner ship if the building is military. */
  if (!constructing && active && is_military()) {
    game->update_land_ownership(pos);
  }

  if (!constructing && (type == TypeCastle || type == TypeStock)) {
    /* Cancel resources in the out queue and remove gold from map total. */
    if (active) {
      game->delete_inventory(inventory);
      inventory = nullptr;
    }

    /* Let some serfs escape while the building is burning. */
    unsigned int escaping_serfs = 0;
    for (Serf *serf : game->get_serfs_at_pos(pos)) {
      if (serf->building_deleted(pos, escaping_serfs < 12)) {
        escaping_serfs++;
      }
    }
  } else {
    active = false;
  }

  /* Remove stock from building. */
  remove_stock();

  stop_playing_sfx();

  unsigned int _serf_index = first_knight;
  burning_counter = 2047;
  u.tick = game->get_tick();

  Player *player = game->get_player(owner);
  player->building_demolished(this);

  if (holder) {
    holder = false;

    if (!constructing && (type == TypeCastle)) {
      set_burning_counter(8191);

      for (Serf *serf : game->get_serfs_at_pos(pos)) {
        serf->castle_deleted(pos, true);
      }
    }

    if (!constructing && is_military()) {
      while (_serf_index != 0) {
        Serf *serf = game->get_serf(_serf_index);
        _serf_index = serf->get_next();

        serf->castle_deleted(pos, false);
      }
    } else {
      Serf *serf = game->get_serf(_serf_index);
      if (serf->get_type() == Serf::TypeTransporterInventory) {
        serf->set_type(Serf::TypeTransporter);
      }

      serf->castle_deleted(pos, false);
    }
  }

  PMap map = game->get_map();
  MapPos flag_pos = map->move_down_right(pos);
  if (map->paths(flag_pos) == 0 &&
    map->get_obj(flag_pos) == Map::ObjectFlag) {
    game->demolish_flag(flag_pos, player);
  }

  return true;
}

/* Calculate the flag state of military buildings (distance to enemy). */
void
Building::update_military_flag_state() {
  const int border_check_offsets[] = {
    31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
    100, 101, 102, 103, 104, 105, 106, 107, 108,
    259, 260, 261, 262, 263, 264,
    241, 242, 243, 244, 245, 246,
    217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228,
    247, 248, 249, 250, 251, 252,
    -1,

    265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276,
    -1,

    277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288,
    289, 290, 291, 292, 293, 294,
    -1
  };

  int f, k;
  PMap map = game->get_map();
  for (f = 3, k = 0; f > 0; f--) {
    int offset;
    while ((offset = border_check_offsets[k++]) >= 0) {
      MapPos check_pos = map->pos_add_spirally(get_position(), offset);
      if (map->has_owner(check_pos) && map->get_owner(check_pos) != owner) {
        threat_level = f;
        return;
      }
    }
  }
}

void
Building::update(unsigned int tick) {
  if (burning) {
    uint16_t delta = tick - u.tick;
    u.tick = tick;
    if (burning_counter >= delta) {
      burning_counter -= delta;
    } else {
      game->delete_building(this);
    }
  } else {
    update();
  }
}

void
Building::requested_serf_lost() {
  if (serf_requested) {
    serf_requested = false;
  } else if (!has_inventory()) {
    decrease_requested_for_stock(0);
  }
}

void
Building::requested_serf_reached(Serf *serf) {
  //got nullptr exception here for first time, dec09 2020
  holder = true;
  if (serf_requested) {
    first_knight = serf->get_index();
  }
  serf_requested = false;
}

void
Building::knight_request_granted() {
  stock[0].requested += 1;
  serf_requested = false;
}

void
Building::remove_stock() {
  stock[0].available = 0;
  stock[0].requested = 0;
  stock[1].available = 0;
  stock[1].requested = 0;
}

int
Building::get_max_priority_for_resource(
    Resource::Type resource, int minimum) const {
  int max_prio = -1;

  for (int i = 0; i < kMaxStock; i++) {
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
  if (stock[0].available > 0 && stock[1].available > 0) {
    stock[0].available -= 1;
    stock[1].available -= 1;
    return true;
  }
  return false;
}

void
Building::request_serf_if_needed() {
  typedef struct Request {
    Serf::Type serf_type;
    Resource::Type res_type_1;
    Resource::Type res_type_2;
  } Request;

  Request requests[] = {
    {Serf::TypeNone       , Resource::TypeNone   , Resource::TypeNone  },
    {Serf::TypeFisher     , Resource::TypeRod    , Resource::TypeNone  },
    {Serf::TypeLumberjack , Resource::TypeAxe    , Resource::TypeNone  },
    {Serf::TypeBoatBuilder, Resource::TypeHammer , Resource::TypeNone  },
    {Serf::TypeStonecutter, Resource::TypePick   , Resource::TypeNone  },
    {Serf::TypeMiner      , Resource::TypePick   , Resource::TypeNone  },
    {Serf::TypeMiner      , Resource::TypePick   , Resource::TypeNone  },
    {Serf::TypeMiner      , Resource::TypePick   , Resource::TypeNone  },
    {Serf::TypeMiner      , Resource::TypePick   , Resource::TypeNone  },
    {Serf::TypeForester   , Resource::TypeNone   , Resource::TypeNone  },
    {Serf::TypeNone       , Resource::TypeNone   , Resource::TypeNone  },
    {Serf::TypeNone       , Resource::TypeNone   , Resource::TypeNone  },
    {Serf::TypeFarmer     , Resource::TypeScythe , Resource::TypeNone  },
    {Serf::TypeButcher    , Resource::TypeCleaver, Resource::TypeNone  },
    {Serf::TypePigFarmer  , Resource::TypeNone   , Resource::TypeNone  },
    {Serf::TypeMiller     , Resource::TypeNone   , Resource::TypeNone  },
    {Serf::TypeBaker      , Resource::TypeNone   , Resource::TypeNone  },
    {Serf::TypeSawmiller  , Resource::TypeSaw    , Resource::TypeNone  },
    {Serf::TypeSmelter    , Resource::TypeNone   , Resource::TypeNone  },
    {Serf::TypeToolmaker  , Resource::TypeHammer , Resource::TypeSaw   },
    {Serf::TypeWeaponSmith, Resource::TypeHammer , Resource::TypePincer},
    {Serf::TypeNone       , Resource::TypeNone   , Resource::TypeNone  },
    {Serf::TypeNone       , Resource::TypeNone   , Resource::TypeNone  },
    {Serf::TypeSmelter    , Resource::TypeNone   , Resource::TypeNone  },
    {Serf::TypeNone       , Resource::TypeNone   , Resource::TypeNone  },
  };

  if (!serf_request_failed && !holder && !serf_requested) {
    if (requests[type].serf_type != Serf::TypeNone) {
      serf_request_failed = !send_serf_to_building(requests[type].serf_type,
                                                   requests[type].res_type_1,
                                                   requests[type].res_type_2);
    }
  }
}

void
Building::update() {
  if (!constructing) {
    request_serf_if_needed();

    switch (get_type()) {
      case TypeBoatbuilder:
        if (holder) {
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
      case TypeStoneMine:
        if (holder) {
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
        if (holder) {
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
        if (holder) {
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
        if (holder) {
          int total_food = stock[0].requested + stock[0].available;
          if (total_food < stock[0].maximum) {
            Player *player = game->get_player(get_owner());
            stock[0].prio = player->get_food_goldmine() >> (8 + total_food);
          } else {
            stock[0].prio = 0;
          }
        }
        break;
      case TypeStock:
        if (!is_active()) {
          Inventory *inv = game->create_inventory();
          if (inv == NULL) return;

          inv->set_owner(get_owner());
          inv->set_building_index(get_index());
          inv->set_flag_index(flag);

          inventory = inv;
          stock[0].requested = 0xff;
          stock[0].available = 0xff;
          stock[1].requested = 0xff;
          stock[1].available = 0xff;
          active = true;

          game->get_player(get_owner())->add_notification(
                                 Message::TypeNewStock, pos, 0);
        } else {
          if (!serf_request_failed && !holder && !serf_requested) {
            send_serf_to_building(Serf::TypeTransporter,
                                  Resource::TypeNone,
                                  Resource::TypeNone);
          }

          Player *player = game->get_player(get_owner());
          if (holder &&
              !inventory->have_any_out_mode() == 0 &&  // Not serf or
                                                       // res OUT mode
              inventory->free_serf_count() == 0) {
            if (player->tick_send_generic_delay()) {
              send_serf_to_building(Serf::TypeGeneric,
                                    Resource::TypeNone,
                                    Resource::TypeNone);
            }
          }

          /* TODO Following code looks like a hack */
          PMap map = game->get_map();
          MapPos flag_pos = map->move_down_right(pos);
          if (map->has_serf(flag_pos)) {
            Serf *serf = game->get_serf_at_pos(flag_pos);
            if (serf->get_pos() != flag_pos) {
              map->set_serf_index(flag_pos, 0);
            }
          }
        }
        break;
      case TypeHut:
      case TypeTower:
      case TypeFortress:
        update_military();
        break;
      case TypeButcher:
        if (holder) {
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
        if (holder) {
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
        if (holder) {
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
        if (holder) {
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
        if (holder) {
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
        if (holder) {
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
        if (holder) {
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
        if (holder) {
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
        if (holder) {
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
  if (!serf_request_failed && !holder && !serf_requested) {
    progress = 1;
    serf_request_failed = !send_serf_to_building(Serf::TypeBuilder,
                                                 Resource::TypeHammer,
                                                 Resource::TypeNone);
  }

  /* Request planks */
  int total_planks = stock[0].requested + stock[0].available;
  if (total_planks < stock[0].maximum) {
    int planks_prio = player->get_planks_construction() >> (8 + total_planks);
    if (!holder) planks_prio >>= 2;
    stock[0].prio = planks_prio & ~BIT(0);
  } else {
    stock[0].prio = 0;
  }

  /* Request stone */
  int total_stone = stock[1].requested + stock[1].available;
  if (total_stone < stock[1].maximum) {
    int stone_prio = 0xff >> total_stone;
    if (!holder) stone_prio >>= 2;
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

  if (holder || serf_requested) {
    return;
  }

  /* Check whether building needs leveling */
  int need_leveling = 0;
  unsigned int height = game->get_leveling_height(pos);
  for (int i = 0; i < 7; i++) {
    MapPos _pos = game->get_map()->pos_add_spirally(pos, i);
    if (game->get_map()->get_height(_pos) != height) {
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
  if (!serf_request_failed) {
    serf_request_failed = !send_serf_to_building(Serf::TypeDigger,
                                                 Resource::TypeShovel,
                                                 Resource::TypeNone);
  }
}

/* Dispatch serf to building. */
bool
Building::send_serf_to_building(Serf::Type serf_type, Resource::Type res1,
                                Resource::Type res2) {
  Flag *dest = game->get_flag(flag);
  return game->send_serf_to_flag(dest, serf_type, res1, res2);
}

/* Update castle as part of the game progression. */
void
Building::update_castle() {
  Player *player = game->get_player(get_owner());
  if (player->get_castle_knights() == player->get_castle_knights_wanted()) {
    Serf *best_knight = NULL;
    Serf *last_knight = NULL;
    unsigned int next_serf_index = first_knight;
    while (next_serf_index != 0) {
      Serf *serf = game->get_serf(next_serf_index);
      if (serf == nullptr) {
        throw ExceptionFreeserf("Index of nonexistent serf in the queue.");
      }
      if ((best_knight == NULL) || serf->get_type() < best_knight->get_type()) {
        best_knight = serf;
      }
      last_knight = serf;
      next_serf_index = serf->get_next();
    }

    if (best_knight != nullptr) {
      Serf::Type knight_type = best_knight->get_type();
      for (int t = Serf::TypeKnight0; t <= Serf::TypeKnight4; t++) {
        if (knight_type > t) {
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
    Serf::Type knight_type = Serf::TypeNone;
    for (int t = Serf::TypeKnight4; t >= Serf::TypeKnight0; t--) {
      if (inventory->have_serf((Serf::Type)t)) {
        knight_type = (Serf::Type)t;
        break;
      }
    }

    if (knight_type < 0) {
      /* None found */
      if (inventory->have_serf(Serf::TypeGeneric) &&
          inventory->get_count_of(Resource::TypeSword) != 0 &&
          inventory->get_count_of(Resource::TypeShield) != 0) {
        Serf *serf = inventory->specialize_free_serf(Serf::TypeKnight0);
        inventory->call_internal(serf);

        serf->add_to_defending_queue(first_knight, false);
        first_knight = serf->get_index();
        player->increase_castle_knights();
      } else {
        if (player->tick_send_knight_delay()) {
          send_serf_to_building(Serf::TypeNone,
                                Resource::TypeNone,
                                Resource::TypeNone);
        }
      }
    } else {
      /* Prepend to knights list */
      Serf *serf = inventory->call_internal(knight_type);
      serf->add_to_defending_queue(first_knight, true);
      first_knight = serf->get_index();
      player->increase_castle_knights();
    }
  } else {
    player->decrease_castle_knights();

    int _serf_index = first_knight;
    Serf *serf = game->get_serf(_serf_index);
    first_knight = serf->get_next();

    serf->stay_idle_in_stock(inventory->get_index());
  }

  if (holder &&
      !inventory->have_any_out_mode() && /* Not serf or res OUT mode */
      inventory->free_serf_count() == 0) {
    if (player->tick_send_generic_delay()) {
      send_serf_to_building(Serf::TypeGeneric,
                            Resource::TypeNone,
                            Resource::TypeNone);
    }
  }

  PMap map = game->get_map();
  MapPos flag_pos = map->move_down_right(pos);
  if (map->has_serf(flag_pos)) {
    Serf *serf = game->get_serf_at_pos(flag_pos);
	// trying skip if nullptr...
	if (serf == nullptr) {
	  Log::Warn["building"] << " inside Building::update_castle - Serf *serf is nullptr!, skipping it.  This is normally a crash bug";
	}else if (serf->get_pos() != flag_pos) {
      map->set_serf_index(flag_pos, 0);
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
  size_t max_occ_level =
                       (player->get_knight_occupation(threat_level) >> 4) & 0xf;
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
    if (!serf_request_failed) {
      serf_request_failed = !send_serf_to_building(Serf::TypeNone,
                                                   Resource::TypeNone,
                                                   Resource::TypeNone);
    }
  } else if (needed_occupants < present_knights &&
             !game->get_map()->has_serf(
                                       game->get_map()->move_down_right(pos))) {
    /* Kick least trained knight out. */
    Serf *leaving_serf = NULL;
    int _serf_index = first_knight;
    while (_serf_index != 0) {
      Serf *serf = game->get_serf(_serf_index);
      if (serf == nullptr) {
        throw ExceptionFreeserf("Index of nonexistent serf in the queue.");
      }
      if (leaving_serf == NULL || serf->get_type() < leaving_serf->get_type()) {
        leaving_serf = serf;
      }
      _serf_index = serf->get_next();
    }

    if (leaving_serf != NULL) {
      /* Remove leaving serf from list. */
      if (leaving_serf->get_index() == first_knight) {
        first_knight = leaving_serf->get_next();
      } else {
        _serf_index = first_knight;
        while (_serf_index != 0) {
          Serf *serf = game->get_serf(_serf_index);
          if (serf->get_next() == leaving_serf->get_index()) {
            serf->set_next(leaving_serf->get_next());
            break;
          }
          _serf_index = serf->get_next();
        }
      }

      /* Update serf state. */
      leaving_serf->go_out_from_building(0, 0, -2);

      stock[0].available -= 1;
    }
  }

  /* Request gold */
  if (holder) {
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
  building.pos = building.get_game()->get_map()->pos_from_saved_value(v32);

  uint8_t v8;
  reader >> v8;  // 4
  building.type = (Building::Type)((v8 >> 2) & 0x1f);
  building.owner = v8 & 3;
  building.constructing = ((v8 & 0x80) != 0);

  reader >> v8;  // 5
  building.threat_level = v8 & 3;
  building.serf_request_failed = ((v8 & 4) != 0);;
  building.playing_sfx = ((v8 & 8) != 0);;
  building.active = ((v8 & 16) != 0);;
  building.burning = ((v8 & 32) != 0);;
  building.holder = ((v8 & 64) != 0);;
  building.serf_requested = ((v8 & 128) != 0);;

  uint16_t v16;
  reader >> v16;  // 6
  building.flag = v16;

  bool has_inventory = false;

  for (int i = 0; i < 2; i++) {
    reader >> v8;  // 8, 9
    building.stock[i].type = Resource::TypeNone;
    building.stock[i].available = 0;
    building.stock[i].requested = 0;
    if (v8 != 0xff) {
      building.stock[i].available = (v8 >> 4) & 0xf;
      building.stock[i].requested = v8 & 0xf;
    } else if (i == 0) {
      has_inventory = true;
    }
  }

  if (building.type == Building::TypeStock) {
    has_inventory = true;
  }

  reader >> v16;  // 10
  building.first_knight = v16;
  reader >> v16;  // 12
  building.progress = v16;

  if (has_inventory && !building.constructing) {
    reader >> v32;  // 14
    int offset = v32;
    building.inventory = building.game->create_inventory(offset/120);
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
  } else if (building.holder) {
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
  try {
    reader.value("owner") >> building.owner;
    int temp;
    reader.value("constructing") >> temp;
    building.constructing = (temp != 0);
  } catch (...) {
    unsigned int n;
    reader.value("bld") >> n;
    building.owner = n & 3;
    building.constructing = ((n & 0x80) != 0);
  }
  try {
    reader.value("military_state") >> building.threat_level;
    int temp;
    reader.value("serf_request_failed") >> temp;
    building.serf_request_failed = (temp != 0);
    reader.value("playing_sfx") >> temp;
    building.playing_sfx = (temp != 0);
    reader.value("active") >> temp;
    building.active = (temp != 0);
    reader.value("burning") >> temp;
    building.burning = (temp != 0);
    reader.value("holder") >> temp;
    building.holder = (temp != 0);
    reader.value("serf_requested") >> temp;
    building.serf_requested = (temp != 0);
  } catch (...) {
    unsigned int n;
    reader.value("serf") >> n;
    building.threat_level = n & 3;
    building.serf_request_failed = ((n & 4) != 0);
    building.playing_sfx = ((n & 8) != 0);
    building.active = ((n & 16) != 0);
    building.burning = ((n & 32) != 0);
    building.holder = ((n & 64) != 0);
    building.serf_requested = ((n & 128) != 0);
  }
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

  reader.value("serf_index") >> building.first_knight;
  reader.value("progress") >> building.progress;

  if (reader.has_value("inventory")) {
    unsigned int inventory_index;
    reader.value("inventory") >> inventory_index;
    building.inventory = building.game->create_inventory(inventory_index);
  } else if (building.burning) {
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
  writer.value("owner") << building.owner;
  writer.value("constructing") << building.constructing;

  writer.value("military_state") << building.threat_level;
  writer.value("playing_sfx") << building.playing_sfx;
  writer.value("serf_request_failed") << building.serf_request_failed;
  writer.value("serf_requested") << building.serf_requested;
  writer.value("burning") << building.burning;
  writer.value("active") << building.active;
  writer.value("holder") << building.holder;

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

  writer.value("serf_index") << building.first_knight;
  writer.value("progress") << building.progress;

  if (building.inventory != nullptr) {
    writer.value("inventory") << building.inventory->get_index();
  } else if (building.is_burning()) {
    writer.value("tick") << building.u.tick;
  } else {
    writer.value("level") << building.u.level;
  }

  return writer;
}
