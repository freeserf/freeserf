/*
 * ai-manage.cc - Computer player: building management, location
 *                validation and attacks (ported from the Amiga original)
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

#include "src/ai-manage.h"

#include <cstdint>

#include "src/ai-core.h"
#include "src/ai-rate.h"
#include "src/resource.h"

/* Height byte of the original: bit 7 has owner, bits 5-6 owner.
   Compared with (player + 4) << 5 by the original. */
#define AI_OWNED_BY(pos, num) \
  (MAP_HAS_OWNER(pos) && MAP_OWNER(pos) == static_cast<unsigned int>(num))

/* 16-bit unsigned add / double with saturation at 0xffff (the
   original's add.w; bcs -> moveq -1). */
unsigned int
AI::Manage::sat_add16(unsigned int a, unsigned int b) {
  unsigned int r = (a & 0xffff) + (b & 0xffff);
  return r > 0xffff ? 0xffff : r;
}

bool
AI::Manage::is_military_type(Building::Type type) {
  return type == Building::TypeHut || type == Building::TypeTower ||
         type == Building::TypeFortress || type == Building::TypeCastle;
}

/* The original's game.max_building_index: one past the highest allocated
   building index. The building collection does not keep it, so it is
   derived from the allocated buildings. */
unsigned int
AI::Manage::max_building_index() {
  unsigned int max = 0;
  for (Building *b : AI_GAME->buildings) {
    if (b == nullptr) continue;
    if (b->get_index() >= max) max = b->get_index() + 1;
  }
  return max;
}

/* 0x29a4c: demolish the building at pos (D3 in the original). */
void
AI::Manage::demolish_building(MapPos pos) {
  AI_GAME->demolish_building_(pos);
}

/* The handlers below get the remaining work budget of
   manage_buildings (D7w) and return the new budget. -1 then
   subtracting marks "stop" after a demolition. */

/* 0x29590 */
int
AI::Manage::manage_fisher(Player *player, Building *building, int budget) {
  for (int i = 1; i <= 64; i++) {
    MapPos pos = MAP_POS_ADD_SPIRALLY(building->pos, i);
    /* Water marker (obj bit 7), blocked bit and fish != 0
       (Amiga @0x295a4); the fish are read on water only, like
       map_update_hidden. */
    if (MAP_WATER_MARK(pos) && MAP_BLOCKED(pos) &&
        MAP_IN_WATER(pos) && MAP_RES_FISH(pos) != 0) {
      return budget - 10;
    }
  }

  demolish_building(building->pos);
  return -1 - 10;
}

/* 0x295ca */
int
AI::Manage::manage_lumberjack(Player *player, Building *building,
                              int budget) {
  for (int i = 1; i <= 128; i++) {
    MapPos pos = MAP_POS_ADD_SPIRALLY(building->pos, i);
    int obj = MAP_OBJ(pos);
    if (obj >= Map::ObjectTree0 && obj < Map::ObjectTree0 + 16) {
      return budget - 20;
    }
  }

  demolish_building(building->pos);
  return -1 - 20;
}

/* 0x29606 */
int
AI::Manage::manage_stonecutter(Player *player, Building *building,
                               int budget) {
  for (int i = 1; i <= 128; i++) {
    MapPos pos = MAP_POS_ADD_SPIRALLY(building->pos, i);
    int obj = MAP_OBJ(pos);
    if (obj >= Map::ObjectStone0 && obj < Map::ObjectStone0 + 8) {
      return budget - 20;
    }
  }

  demolish_building(building->pos);
  return -1 - 20;
}

/* Shared body of the mine handlers 0x29640 (stone), 0x29672 (coal),
   0x296a4 (iron), 0x296d6 (gold): keep the mine while one of the 32
   nearest positions still has its deposit type. */
int
AI::Manage::manage_mine(Player *player, Building *building, int budget,
                        Map::Minerals deposit) {
  for (int i = 1; i <= 32; i++) {
    MapPos pos = MAP_POS_ADD_SPIRALLY(building->pos, i);
    if (MAP_RES_TYPE(pos) == deposit) return budget - 5;
  }

  demolish_building(building->pos);
  return -1 - 5;
}

/* 0x29708 */
int
AI::Manage::manage_forester(Player *player, Building *building, int budget) {
  /* Original bug: the loop reuses D3 (the building position) as a
     scratch byte, so after the first position it scans around a
     corrupted base and finally demolishes at a corrupted position.
     The port scans around the building and demolishes it. */
  for (int i = 1; i <= 128; i++) {
    MapPos pos = MAP_POS_ADD_SPIRALLY(building->pos, i);
    /* Paths byte & 0x7f == 0: no paths (bits 0-5) and not blocked
       (bit 6); type byte 0x55: both triangles of terrain type 5. */
    if (MAP_PATHS(pos) == 0 && !MAP_BLOCKED(pos) &&
        MAP_OBJ(pos) == Map::ObjectNone &&
        MAP_TYPE_UP(pos) == 5 && MAP_TYPE_DOWN(pos) == 5) {
      return budget - 20;
    }
  }

  demolish_building(building->pos);
  return -1 - 20;
}

/* Thresholds of manage_stock @0x298f0, indexed by the military
   state of the nearby military building: resource sum out / stop,
   generic serfs out / stop. */
static const unsigned int stock_thresholds[4][4] = {
  { 15000, 10000, 200, 100 },
  {  4000,  3000, 200, 100 },
  {  1000,   600, 200, 100 },
  {   500,     0,   0,   0 }
};

bool
AI::Manage::find_res_dest_cb(Flag *flag, void *data) {
  return FLAG_ACCEPTS_RESOURCES(flag);
}

bool
AI::Manage::find_serf_dest_cb(Flag *flag, void *data) {
  return FLAG_ACCEPTS_SERFS(flag);
}

/* 0x239fa as used by the AI: < 0 if the flag itself accepts resources
   or no other flag accepting resources is reachable over roads with
   transporters. (Legacy schedule_slot_to_unknown_dest is a different
   function.) */
int
AI::Manage::find_other_resource_inventory(Flag *flag) {
  if (FLAG_ACCEPTS_RESOURCES(flag)) return -1;
  return FlagSearch::single(flag, find_res_dest_cb, false, true, nullptr) ?
    0 : -1;
}

/* 0x238fa as used by the AI: < 0 if the flag itself accepts serfs or
   no other flag accepting serfs is reachable over land paths.
   Original bug: manage_stock (@0x29924) passes the flag in A1 while
   0x238fa reads the flag index from D0, which is left over from the
   caller, so the original searches from an unrelated flag; and 0x238fa
   counts the source flag itself as found. Here the search starts at
   the stock's flag and, like the resource variant 0x239fa, looks for
   another inventory. */
int
AI::Manage::find_other_serf_inventory(Flag *flag) {
  if (FLAG_ACCEPTS_SERFS(flag)) return -1;
  return FlagSearch::single(flag, find_serf_dest_cb, true, false, nullptr) ?
    0 : -1;
}

/* 0x2974c: move resources and serfs out of a stock near the front. */
int
AI::Manage::manage_stock(Player *player, Building *building, int budget) {
  if (!BUILDING_HAS_SERF(building)) return budget;

  for (int i = 7; i < 7 + 0x102; i++) {
    MapPos pos = MAP_POS_ADD_SPIRALLY(building->pos, i);
    int obj = MAP_OBJ(pos);
    if (obj < Map::ObjectSmallBuilding || obj > Map::ObjectCastle) continue;

    Building *mil = AI_BUILDING(MAP_OBJ_INDEX(pos));
    if (BUILDING_PLAYER(mil) != PLAYER_NUM(player) ||
        !BUILDING_IS_ACTIVE(mil) ||
        !is_military_type(BUILDING_TYPE(mil))) {
      continue;
    }

    const unsigned int *thr = stock_thresholds[BUILDING_STATE(mil)];
    Flag *flag = AI_FLAG(building->flag);
    Inventory *inv = building->inventory;
    ResourceMap &r = inv->resources;

    /* Weighted resource sum, saturating at 0xffff. */
    unsigned int sum = r[Resource::TypeGoldBar];
    sum = sat_add16(sum, sum);
    sum = sat_add16(sum, r[Resource::TypeSword]);
    sum = sat_add16(sum, r[Resource::TypeShield]);
    sum = sat_add16(sum, r[Resource::TypeGoldOre]);
    sum = sat_add16(sum, sum);
    for (int res = Resource::TypeShovel; res <= Resource::TypePincer; res++) {
      sum = sat_add16(sum, r[static_cast<Resource::Type>(res)]);
    }
    sum = sat_add16(sum, sum);
    sum = sat_add16(sum, r[Resource::TypeIronOre]);
    sum = sat_add16(sum, r[Resource::TypeSteel]);
    sum = sat_add16(sum, r[Resource::TypeCoal]);
    sum = sat_add16(sum, sum);
    sum = sat_add16(sum, r[Resource::TypeBoat]);
    sum = sat_add16(sum, r[Resource::TypeStone]);
    sum = sat_add16(sum, sum);
    sum = sat_add16(sum, sum);

    /* Resource mode (res_dir bits 0-1; the original sets the
       bits directly, without the panel's delivery cancelling). */
    int mode; /* 0 in, 1 stop, 3 out */
    if (sum >= thr[0]) {
      budget -= 100;
      if (find_other_resource_inventory(flag) < 0) {
        mode = 0;
      } else if (inv->res_dir & BIT(1)) {
        mode = 3;
      } else if (player->ai.u_1b2 >= 10000) {
        /* Original bug: the original reads and writes
           (0x1b2,A4) with A4 = inventory pointer here.
           The player's counter ai.u_1b2 (decremented
           each tick at 0xa554) is meant. */
        mode = 1;
      } else {
        player->ai.u_1b2 += 9000;
        mode = 3;
      }
    } else if (sum >= thr[1]) {
      mode = 1;
    } else {
      mode = 0;
    }

    inv->res_dir = (inv->res_dir & ~3) | mode;
    if (mode == 0) {
      flag->bld2_flags |= BIT(7);
    } else {
      flag->bld2_flags &= ~BIT(7);
    }

    /* Serf mode (res_dir bits 2-3) from the generic serfs. */
    unsigned int serfs = inv->generic_count & 0xffff;
    if (serfs >= thr[2]) {
      budget -= 100;
      /* Original bug: find_nearest_inventory takes a flag
         index in D0, which here still holds the building
         index of manage_buildings. The stock's flag is
         meant (A1 is set up for it). */
      if (find_other_serf_inventory(flag) < 0) {
        mode = 0;
      } else if (inv->res_dir & BIT(3)) {
        mode = 3;
      } else if (player->ai.u_1b2 >= 10000) {
        /* Original bug: A4 = inventory, as above. */
        mode = 1;
      } else {
        player->ai.u_1b2 += 3000;
        mode = 3;
      }
    } else if (serfs >= thr[3]) {
      mode = 1;
    } else {
      mode = 0;
    }

    inv->res_dir = (inv->res_dir & ~0xc) | (mode << 2);
    if (mode == 0) {
      flag->bld_flags |= BIT(7);
    } else {
      flag->bld_flags &= ~BIT(7);
    }

    return budget - 10;
  }

  return budget - 10;
}

/* 0x299b0: demolish farms when there is enough food. */
int
AI::Manage::manage_farm(Player *player, Building *building, int budget) {
  /* ai.u_39c.. = resource totals by type (food: 0x39c..0x3a6). */
  unsigned int food = player->ai.u_39c & 0xffff;
  food = sat_add16(food, player->ai.u_39e);
  food = sat_add16(food, player->ai.u_3a0);
  food = sat_add16(food, player->ai.u_3a2);
  food = sat_add16(food, player->ai.u_3a4);
  food = sat_add16(food, player->ai.u_3a6);

  if (food < 500) return budget;

  unsigned int max_farms;
  if (food < 600) {
    max_farms = 8;
  } else if (food < 700) {
    max_farms = 7;
  } else if (food < 800) {
    max_farms = 6;
  } else if (food < 900) {
    max_farms = 5;
  } else if (food < 1000) {
    max_farms = 4;
  } else if (food < 1500) {
    max_farms = 3;
  } else if (food < 2000) {
    max_farms = 2;
  } else {
    max_farms = 1;
  }

  unsigned int farms =
    player->completed_building_count[Building::TypeFarm] & 0xffff;
  if (max_farms < farms) demolish_building(building->pos);

  return budget;
}

/* 0x294ae: walk up to 500 finished buildings of the player starting at
   the cursor ai.u_19a and call the handler of their type (jump table
   0x2952a: types without a handler point to an rts). */
void
AI::Manage::manage_buildings(Player *player) {
  int budget = 500;
  unsigned int index = player->ai.u_19a & 0xffff;
  /* The handlers only start burning buildings, which keeps them
     allocated, so the limit does not change during the walk. */
  unsigned int max_index = max_building_index();

  if (index > max_index) {
    /* The original checks the (unallocated) entry, then wraps. */
    player->ai.u_19a = 0;
    return;
  }

  while (1) {
    if (index != 0 && BUILDING_ALLOCATED(index)) {
      Building *building = AI_BUILDING(index);
      if (!BUILDING_IS_BURNING(building) &&
          BUILDING_IS_DONE(building) &&
          BUILDING_PLAYER(building) == PLAYER_NUM(player)) {
        switch (BUILDING_TYPE(building)) {
          case Building::TypeFisher:
            budget = manage_fisher(player, building, budget);
            break;
          case Building::TypeLumberjack:
            budget = manage_lumberjack(player, building, budget);
            break;
          case Building::TypeStonecutter:
            budget = manage_stonecutter(player, building, budget);
            break;
          case Building::TypeStoneMine:
            budget = manage_mine(player, building, budget,
                                 Map::MineralsStone);
            break;
          case Building::TypeCoalMine:
            budget = manage_mine(player, building, budget,
                                 Map::MineralsCoal);
            break;
          case Building::TypeIronMine:
            budget = manage_mine(player, building, budget,
                                 Map::MineralsIron);
            break;
          case Building::TypeGoldMine:
            budget = manage_mine(player, building, budget,
                                 Map::MineralsGold);
            break;
          case Building::TypeForester:
            budget = manage_forester(player, building, budget);
            break;
          case Building::TypeStock:
            budget = manage_stock(player, building, budget);
            break;
          case Building::TypeFarm:
            budget = manage_farm(player, building, budget);
            break;
          default:
            break;
        }
      }
    }

    index += 1;
    budget -= 1;
    if (budget < 0) break;
    if (index > max_index) {
      index = 0;
      break;
    }
  }

  player->ai.u_19a = index;
}


/* Location lists of ai.locations, addressed as the original does: n
   consecutive entries starting at category first. */
AILocation *
AI::Manage::location_entry(Player *player, int first, int k) {
  int n = first * AI_LOCATIONS_PER_CATEGORY + k;
  return &player->ai.locations[n / AI_LOCATIONS_PER_CATEGORY]
    [n % AI_LOCATIONS_PER_CATEGORY];
}

/* 0x29ac8: small building sites must still be owned and free. */
void
AI::Manage::validate_locations_small(Player *player, int first, int count) {
  for (int k = 0; k < count; k++) {
    AILocation *loc = location_entry(player, first, k);
    if (loc->value == 0) continue;

    MapPos pos = MAP_POS(loc->col, loc->row);
    int ok = 0;
    if (AI_OWNED_BY(pos, PLAYER_NUM(player)) && MAP_SPACE(pos) < 2) {
      pos = MAP_MOVE_RIGHT(pos);
      if (MAP_SPACE(pos) < 3) {
        pos = MAP_MOVE_DOWN(pos);
        if (MAP_SPACE(pos) < 3) {
          pos = MAP_MOVE_LEFT(pos);
          if (MAP_SPACE(pos) < 3) {
            pos = MAP_MOVE_UP_LEFT(pos);
            if (MAP_SPACE(pos) < 4) {
              pos = MAP_MOVE_UP(pos);
              if (MAP_SPACE(pos) < 4) {
                pos = MAP_MOVE_RIGHT(pos);
                if (MAP_SPACE(pos) < 4) ok = 1;
              }
            }
          }
        }
      }
    }

    if (!ok) loc->value = 0;
  }
}

/* 0x29b92: large building sites. */
void
AI::Manage::validate_locations_large(Player *player, int first, int count) {
  /* Steps from the site and the space limit after each step. */
  static const struct { Direction dir; unsigned int limit; } steps[] = {
    { DirectionRight, 2 }, { DirectionDown, 2 }, { DirectionLeft, 2 },
    { DirectionUpLeft, 2 }, { DirectionUp, 2 }, { DirectionRight, 2 },
    { DirectionRight, 4 }, { DirectionDownRight, 4 }, { DirectionDown, 4 },
    { DirectionDown, 4 }, { DirectionLeft, 4 }, { DirectionLeft, 4 },
    { DirectionUpLeft, 4 }, { DirectionUpLeft, 4 }, { DirectionUp, 4 },
    { DirectionUp, 4 }, { DirectionRight, 4 }, { DirectionRight, 4 }
  };

  for (int k = 0; k < count; k++) {
    AILocation *loc = location_entry(player, first, k);
    if (loc->value == 0) continue;

    MapPos pos = MAP_POS(loc->col, loc->row);
    int ok = AI_OWNED_BY(pos, PLAYER_NUM(player)) && MAP_SPACE(pos) < 2;
    for (unsigned int s = 0; ok && s < sizeof(steps)/sizeof(steps[0]); s++) {
      pos = MAP_MOVE(pos, steps[s].dir);
      if (static_cast<unsigned int>(MAP_SPACE(pos)) >= steps[s].limit) ok = 0;
    }

    if (!ok) loc->value = 0;
  }
}

/* 0x29d80: attack targets must still be enemy buildings. */
void
AI::Manage::validate_locations_end(Player *player, int first, int count) {
  for (int k = 0; k < count; k++) {
    AILocation *loc = location_entry(player, first, k);
    if (loc->value == 0) continue;

    MapPos pos = MAP_POS(loc->col, loc->row);
    if (!MAP_HAS_OWNER(pos) ||
        MAP_OWNER(pos) == static_cast<unsigned int>(PLAYER_NUM(player)) ||
        MAP_SPACE(pos) < 4) {
      loc->value = 0;
    }
  }
}

/* 0x29a70: drop stored locations that are no longer usable. */
void
AI::Manage::validate_locations(Player *player) {
  validate_locations_small(player, 1, 72);   /* 0x464: 1..9 */
  validate_locations_small(player, 11, 8);   /* 0x644 */
  validate_locations_small(player, 15, 8);   /* 0x704 */
  validate_locations_large(player, 10, 8);   /* 0x614 */
  validate_locations_large(player, 12, 24);  /* 0x674: 12..14 */
  validate_locations_large(player, 16, 64);  /* 0x734: 16..23 */
  validate_locations_end(player, 26, 72);    /* 0x914: 26..34 */
}


/* Military strength ratio of the player against all others, the word
   the original keeps at player ptr+0x186 (computed by
   player_update_knight_morale @0xb4bc..0xb542). update_knight_morale
   stores the result in player->ai.military_ratio. */
int
AI::Manage::calc_military_ratio(Player *player) {
  uint32_t own = player->total_military_score;
  unsigned int morale = (player->knight_morale & 0xffff) >> 5;
  while (own >= 0x10000) {
    own >>= 1;
    morale = (morale << 1) & 0xffff;
  }
  own = (own * morale) >> 7;

  uint32_t other = 0;
  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    Player *p = AI_GAME->get_player(i);
    if (p == nullptr || p == player) continue;
    other += p->total_military_score;
  }

  while (own >= 0x10000) {
    own >>= 1;
    other >>= 1;
  }
  while (other >= 0x10000) {
    own >>= 1;
    other >>= 1;
  }

  own = (own & 0xffff) >> 1;
  if (own == 0 || other == 0) return 0;
  if (((other - own) & 0x8000) != 0) return 0xffff;

  /* Original bug: divu overflows when other == own and stores 0. */
  uint32_t q = (own << 16) / other;
  return q > 0xffff ? 0xffff : q;
}

/* Attack chance factor by ai.u_1aa, table @0x29dc0. */
static const unsigned int attack_factor[17] = {
  500, 700, 1000, 1400, 1900, 2500, 3000, 3500, 4096,
  5000, 7000, 10000, 15000, 21000, 28000, 36000, 45000
};

/* 0x29de2: decide whether to attack; otherwise scan for sites. */
void
AI::Manage::attack(Player *player) {
  unsigned int ratio = player->ai.military_ratio & 0xffff;

  if (ratio < 0x8000) {
    unsigned int d0;
    if (ratio >= 0x2000) {
      d0 = (ratio - 0x2000)*2 + 0x1000;
    } else {
      d0 = ratio >> 1;
    }

    /* Original reads past the table for u_1aa > 16. */
    int fi = player->ai.u_1aa & 0xffff;
    if (fi > 16) fi = 16;
    uint32_t v = (d0 * attack_factor[fi]) >> 12;
    d0 = v >= 0x10000 ? 0xffff : v;

    /* Knights weighted by level (the first add wraps). */
    unsigned int k = (player->serf_count[Serf::TypeKnight4] +
                      player->serf_count[Serf::TypeKnight3]) & 0xffff;
    k = sat_add16(k, k);
    if (k != 0xffff) k = sat_add16(k, player->serf_count[Serf::TypeKnight2]);
    if (k != 0xffff) k = sat_add16(k, k);
    if (k != 0xffff) k = sat_add16(k, k);
    if (k != 0xffff) k = sat_add16(k, player->serf_count[Serf::TypeKnight1]);
    if (k < 0x100) d0 = ((d0 * k) >> 8) & 0xffff;

    unsigned int intel =
      ((0xffff - (player->ai_intelligence & 0xffff)) >> 1) + 0x8000;
    uint64_t p = static_cast<uint64_t>(d0) * intel * 2;
    uint32_t hi = p > 0xffffffffu ? 0xffff : static_cast<uint32_t>(p >> 16);
    uint32_t chance = ((hi * (player->ai_value_2 & 0xffff)) >> 16) * 2;
    if (chance > 0xffff) chance = 0xffff;

    if (AI_GAME->random_int() >= chance) {
      AI::Core::scan_sites(player);
      return;
    }
  }

  /* Best value of each attack category 26..34. */
  unsigned int w[9];
  for (int c = 0; c < 9; c++) {
    unsigned int best = 0;
    for (int i = 0; i < AI_LOCATIONS_PER_CATEGORY; i++) {
      unsigned int val = player->ai.locations[26+c][i].value & 0xffff;
      if (val > best) best = val;
    }
    w[c] = best;
  }

  /* Weight categories 31..34 by the resource totals
     (ai.u_3ae stone, u_3b0 iron ore, u_3b4 coal, u_3b6 gold ore). */
  unsigned int stone = player->ai.u_3ae & 0xffff;
  unsigned int ironore = player->ai.u_3b0 & 0xffff;
  unsigned int coal = player->ai.u_3b4 & 0xffff;
  unsigned int goldore = player->ai.u_3b6 & 0xffff;
  unsigned int d;
  uint32_t v;

  d = sat_add16(coal >> 1, 100);
  d = d >= goldore ? d - goldore : 0;
  if (d >= 400) d = 400;
  d = ((d + 50) << 6) & 0xffff;
  v = (d * w[5]) >> 12;
  w[5] = v >= 0x10000 ? 0xffff : v;

  d = sat_add16(coal, 100);
  d = d >= ironore ? d - ironore : 0;
  if (d >= 400) d = 400;
  d = (d << 6) & 0xffff;
  v = (d * w[6]) >> 12;
  w[6] = v >= 0x10000 ? 0xffff : v;

  d = sat_add16(ironore, goldore);
  if (d != 0xffff) d = sat_add16(d, 50);
  d = d >= coal ? d - coal : 0;
  if (d >= 400) d = 400;
  d = (d << 5) & 0xffff;
  v = (d * w[7]) >> 12;
  w[7] = v >= 0x10000 ? 0xffff : v;

  d = stone;
  if (d >= 300) d = 300;
  d = (d << 5) & 0xffff;
  v = (d * w[8]) >> 12;
  w[8] = v >= 0x10000 ? 0xffff : v;

  /* Preferred categories (bits of ai_value_3) count four times. */
  for (int c = 0; c < 9; c++) {
    if (player->ai_value_3 & BIT(c)) {
      unsigned int x = sat_add16(w[c], w[c]);
      if (x != 0xffff) x = sat_add16(x, x);
      w[c] = x;
    }
  }

  unsigned int r = AI_GAME->random_int();
  int cat;
  if (r & 1) {
    /* Best category. */
    unsigned int best = 0;
    cat = 0;
    for (int c = 0; c < 9; c++) {
      if (w[c] > best) {
        best = w[c];
        cat = c;
      }
    }
    if (best == 0) {
      AI::Core::scan_sites(player);
      return;
    }
  } else {
    /* Random category weighted by w; halve all while the sum
       overflows 16 bits. */
    unsigned int sum;
    while (1) {
      sum = 0;
      int overflow = 0;
      for (int c = 0; c < 9; c++) {
        sum += w[c];
        if (sum > 0xffff) {
          overflow = 1;
          break;
        }
      }
      if (!overflow) break;
      for (int c = 0; c < 9; c++) w[c] >>= 1;
    }
    if (sum == 0) {
      AI::Core::scan_sites(player);
      return;
    }

    unsigned int pick = (sum * (r & 0xffff)) >> 16;
    unsigned int acc = 0;
    cat = 0;
    for (int c = 0; c < 9; c++) {
      acc += w[c];
      if (pick < acc) break;
      cat += 1;
    }
  }
  ai_game.build_building_type = cat + 26;

  /* Original: endless when the rating keeps refreshing a slot; the
     port bounds the number of retries. */
  for (int tries = 0; tries < 1000; tries++) {
    AILocation *list = player->ai.locations[ai_game.build_building_type];
    AILocation *best_loc = nullptr;
    unsigned int best = 0;
    for (int i = 0; i < AI_LOCATIONS_PER_CATEGORY; i++) {
      if (static_cast<unsigned int>(list[i].value & 0xffff) > best) {
        best = list[i].value & 0xffff;
        best_loc = &list[i];
      }
    }
    if (best == 0) return;

    ai_game.some_location = best_loc;
    best_loc->value = 0;
    player->ai.cursor_col = best_loc->col;
    player->ai.cursor_row = best_loc->row;

    MapPos pos = AI_CURSOR_POS(player);
    if (!MAP_HAS_OWNER(pos) ||
        MAP_OWNER(pos) == static_cast<unsigned int>(PLAYER_NUM(player))) {
      continue;
    }
    int obj = MAP_OBJ(pos);
    if (obj < Map::ObjectSmallBuilding || obj > Map::ObjectCastle) continue;

    player->building_attacked = MAP_OBJ_INDEX(pos);
    Building *target = AI_BUILDING(player->building_attacked);
    /* Original bug: a non-military building branches into the
       middle of the maximum search (0x2a032) with stale
       registers; the port rejects it like the other checks. */
    if (!is_military_type(BUILDING_TYPE(target))) continue;
    if (!BUILDING_IS_ACTIVE(target) || BUILDING_STATE(target) != 3) {
      continue;
    }

    /* Own land must be near the target. */
    int near = 0;
    for (int i = 7; i < 7 + 0x102; i++) {
      MapPos p = MAP_POS_ADD_SPIRALLY(target->pos, i);
      if (AI_OWNED_BY(p, PLAYER_NUM(player))) {
        near = 1;
        break;
      }
    }
    if (!near) continue;

    player->ai.panel_btn_type = AI_CAN_BUILD_NONE;
    AI::Rate::scan_points_of_interest(player);
    unsigned int rating = AI::Rate::rate_site_dispatch(player) & 0xffff;

    if (rating < static_cast<unsigned int>(ai_game.some_location->value &
                                           0xffff)) {
      int lower = 0;
      for (int i = 0; i < AI_LOCATIONS_PER_CATEGORY; i++) {
        if (rating < static_cast<unsigned int>(list[i].value & 0xffff)) {
          lower = 1;
          break;
        }
      }
      if (lower) {
        ai_game.some_location->value = rating;
        continue;
      }
    }

    /* Attack (0x2a15e). */
    player->knights_available_for_attack(AI_CURSOR_POS(player));

    /* Defenders of the target, weighted 1 << knight level. */
    target = AI_BUILDING(player->building_attacked);
    unsigned int def = 0;
    int si = target->first_knight & 0xffff;
    while (si != 0) {
      Serf *serf = AI_SERF(si);
      def += 1 << ((SERF_TYPE(serf) - Serf::TypeKnight0) & 0x1f);
      si = serf->s.defending.next_knight & 0xffff;
    }
    def &= 0xffff;
    if (BUILDING_TYPE(target) == Building::TypeCastle) {
      def = (def * 2) & 0xffff;
    }

    /* Original bug: divu by knight_morale overflows (or traps
       on 0); the port saturates. */
    unsigned int morale = player->knight_morale & 0xffff;
    uint32_t q = morale ? (static_cast<uint32_t>(def) << 14) / morale : 0xffff;
    if (q > 0xffff) q = 0xffff;
    unsigned int need = (q * (player->ai_value_1 & 0xffff)) >> 16;
    if (need == 0) need = 1;

    uint32_t score = static_cast<uint32_t>(player->total_military_score) << 4;
    unsigned int knights = 0;
    for (int i = 0; i < 5; i++) {
      knights += player->serf_count[Serf::TypeKnight0 + i];
    }
    knights &= 0xffff;
    if (knights == 0) return;

    /* Original bug: these divu can overflow (and trap when the
       strength per knight is 0); the need is sign extended. The
       port uses unsigned values, saturates and does not attack
       on a zero divisor. */
    uint32_t per_knight = score / knights;
    if (per_knight > 0xffff) per_knight = 0xffff;
    if (per_knight == 0) return;
    uint32_t n = (static_cast<uint32_t>(need) << 4) / per_knight;
    if (n > 0xffff) n = 0xffff;
    n += 1;

    if (static_cast<unsigned int>(player->total_attacking_knights & 0xffff) <
        n) {
      return;
    }
    player->knights_attacking = n;
    if (player->attacking_building_count == 0) return;
    player->start_attack();
    return;
  }
}
