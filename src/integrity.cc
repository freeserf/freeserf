/*
 * integrity.cc - Consistency check of the game state
 *
 * Copyright (C) 2026  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/integrity.h"

#include <map>
#include <utility>

#include "src/game.h"
#include "src/flag.h"
#include "src/building.h"
#include "src/serf.h"
#include "src/inventory.h"
#include "src/map.h"
#include "src/log.h"

unsigned int
IntegrityCheck::run(Game *game, bool fix) {
  unsigned int problems = 0;
  problems += check_map(game);
  problems += check_flags(game, fix);
  problems += check_requests(game, fix);
  problems += check_serfs(game);
  if (problems != 0) {
    Log::Warn["integrity"] << problems << " problem(s) found in the game state"
                           << (fix ? ", repaired where possible." : ".");
  }
  return problems;
}

/* Flags and buildings are on their map tiles, and their links agree. */
unsigned int
IntegrityCheck::check_map(Game *game) {
  PMap map = game->get_map();
  unsigned int problems = 0;

  for (Flag *flag : game->flags) {
    if (flag->get_index() == 0) continue;
    MapPos pos = flag->get_position();
    if (map->get_obj(pos) != Map::ObjectFlag ||
        map->get_obj_index(pos) != flag->get_index()) {
      Log::Warn["integrity"] << "flag " << flag->get_index()
                             << " is not on its map tile " << pos;
      problems++;
    }
  }

  for (Building *building : game->buildings) {
    if (building->get_index() == 0) continue;
    MapPos pos = building->get_position();
    Map::Object obj = map->get_obj(pos);
    if (obj < Map::ObjectSmallBuilding || obj > Map::ObjectCastle ||
        map->get_obj_index(pos) != building->get_index()) {
      Log::Warn["integrity"] << "building " << building->get_index()
                             << " is not on its map tile " << pos;
      problems++;
    }

    if (building->flag != 0) {
      Flag *flag = game->flags[building->flag];
      if (flag == nullptr ||
          flag->get_position() != map->move_down_right(pos)) {
        Log::Warn["integrity"] << "building " << building->get_index()
                               << " has a wrong flag " << building->flag;
        problems++;
      }
    }

    if (building->has_inventory() && !building->is_burning() &&
        building->inventory != nullptr &&
        building->inventory->building != building->get_index()) {
      Log::Warn["integrity"] << "inventory of building "
                             << building->get_index()
                             << " belongs to building "
                             << building->inventory->building;
      problems++;
    }
  }

  return problems;
}

/* Resources at flags are scheduled to existing roads, and a resource
   without a destination is not forgotten: a flag that is not marked to
   schedule it keeps it forever (#81). */
unsigned int
IntegrityCheck::check_flags(Game *game, bool fix) {
  unsigned int problems = 0;

  for (Flag *flag : game->flags) {
    if (flag->get_index() == 0) continue;

    for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
      if (flag->slot[i].type == Resource::TypeNone) continue;
      Direction dir = flag->slot[i].dir;
      if (dir != DirectionNone && !flag->has_path(dir)) {
        Log::Warn["integrity"] << "flag " << flag->get_index() << " slot "
                               << i << " is scheduled to a missing road";
        problems++;
        if (fix) {
          flag->slot[i].dir = DirectionNone;
          flag->resources_waiting = true;
        }
      } else if (dir == DirectionNone && flag->slot[i].dest == 0 &&
                 !flag->has_resources()) {
        /* A resource without a destination is either scheduled to a road
           or keeps the flag marked until it is; otherwise it is never
           looked at again. */
        Log::Warn["integrity"] << "flag " << flag->get_index() << " slot "
                               << i << " is never scheduled";
        problems++;
        if (fix) flag->resources_waiting = true;
      }
    }
  }

  return problems;
}

/* The resources a building has requested are all on their way to it: at
   flags, carried by transporters, queued in inventories or carried out
   of them. A request without its resource would block the building's
   stock for good, a resource without a request would fail on delivery. */
unsigned int
IntegrityCheck::check_requests(Game *game, bool fix) {
  typedef std::pair<unsigned int, int> Key;  // (flag index, stock type)
  std::map<Key, int> on_way;
  auto add = [&on_way](int res, unsigned int dest) {
    if (res < 0 || dest == 0) return;
    if (res == Resource::TypeFish || res == Resource::TypeMeat ||
        res == Resource::TypeBread) {
      res = Resource::GroupFood;
    }
    on_way[Key(dest, res)]++;
  };

  for (Flag *flag : game->flags) {
    for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
      add(flag->slot[i].type, flag->slot[i].dest);
    }
  }
  for (Inventory *inventory : game->inventories) {
    for (int i = 0; i < 2; i++) {
      add(inventory->out_queue[i].type, inventory->out_queue[i].dest);
    }
  }
  for (Serf *serf : game->serfs) {
    switch (serf->state) {
      case Serf::StateTransporting:
      case Serf::StateDelivering:
        add(serf->s.transporting.res, serf->s.transporting.dest);
        break;
      case Serf::StateMoveResourceOut:
      case Serf::StateDropResourceOut:
        add(static_cast<int>(serf->s.move_resource_out.res) - 1,
            serf->s.move_resource_out.res_dest);
        break;
      case Serf::StateLeavingBuilding:
      case Serf::StateReadyToLeave:
        if (serf->s.leaving_building.next_state ==
            Serf::StateDropResourceOut) {
          add(serf->s.leaving_building.field_B - 1,
              serf->s.leaving_building.dest);
        }
        break;
      default:
        break;
    }
  }

  unsigned int problems = 0;
  for (Building *building : game->buildings) {
    if (building->get_index() == 0 || building->has_inventory() ||
        building->is_burning() || building->flag == 0) {
      continue;
    }
    for (unsigned int i = 0; i < Building::kMaxStock; i++) {
      Building::Stock &stock = building->stock[i];
      if (stock.type == Resource::TypeNone) continue;
      /* The first stock of military buildings counts knights. */
      if (i == 0 && building->is_military()) continue;
      int count = on_way[Key(building->flag, stock.type)];
      if (stock.requested != count) {
        Log::Warn["integrity"] << "building " << building->get_index()
                               << " stock " << i << " requested "
                               << stock.requested << " but " << count
                               << " on the way";
        problems++;
        if (fix) stock.requested = count;
      }
    }
  }

  return problems;
}

/* Serfs refer only to existing flags, inventories and serfs. */
unsigned int
IntegrityCheck::check_serfs(Game *game) {
  unsigned int problems = 0;

  for (Serf *serf : game->serfs) {
    if (serf->get_index() == 0) continue;
    unsigned int dest = 0;
    switch (serf->state) {
      case Serf::StateWalking:
        /* -2: the destination was cleared. */
        if (serf->s.walking.dir1 != -2) dest = serf->s.walking.dest;
        break;
      case Serf::StateTransporting:
      case Serf::StateDelivering:
        /* Without a resource the destination is not used. */
        if (serf->s.transporting.res != Resource::TypeNone) {
          dest = serf->s.transporting.dest;
        }
        break;
      case Serf::StateIdleInStock:
        if (game->inventories[serf->s.idle_in_stock.inv_index] == nullptr) {
          Log::Warn["integrity"] << "serf " << serf->get_index()
                                 << " idles in missing inventory "
                                 << serf->s.idle_in_stock.inv_index;
          problems++;
        }
        break;
      default:
        break;
    }
    if (dest != 0 && game->flags[dest] == nullptr) {
      Log::Warn["integrity"] << "serf " << serf->get_index()
                             << " goes to missing flag " << dest;
      problems++;
    }
  }

  for (Building *building : game->buildings) {
    if (!building->is_military() || building->is_burning()) continue;
    unsigned int knight = building->first_knight;
    int count = 0;
    while (knight != 0 && count < 100) {
      Serf *serf = game->serfs[knight];
      if (serf == nullptr) {
        Log::Warn["integrity"] << "building " << building->get_index()
                               << " has missing knight " << knight;
        problems++;
        break;
      }
      /* Only a knight defending the building links to the next one. */
      if (serf->state != Serf::StateDefendingHut &&
          serf->state != Serf::StateDefendingTower &&
          serf->state != Serf::StateDefendingFortress &&
          serf->state != Serf::StateDefendingCastle) {
        break;
      }
      knight = serf->s.defending.next_knight;
      count++;
    }
  }

  return problems;
}
