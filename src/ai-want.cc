/*
 * ai-want.cc - Computer player: build wants per building type
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

/* Port of the Amiga build-choice functions 0x2b8c4..0x2ccc6. Each
   ai_want_<type>() writes player->ai.build_want[type - 1] (and some
   write a preliminary value for a related type that a later call
   finishes). All arithmetic is unsigned 16-bit as in the original:
   W() wraps, sadd() saturates (the original's add + bcs -> 0xffff),
   mulhi() is mulu.w + swap. The inputs come from
   ai_update_building_stats (0x2ccc8):
     ptr+0x33c..0x364  (poi[4] words 0..20): stock fill ratios
                       (amount << 16) / capacity, 0xffff when full.
     ptr+0x366..0x39a  (poi[4] words 21..43, u_394..u_39a): number of
                       serfs per serf type idle in inventories.
     ptr+0x39c..0x3ce  (u_39c..u_3ce): resources per resource type in
                       all inventories of the player (saturated). */

#include "src/ai-want.h"

#include <cstddef>
#include <cstdint>

#include "src/resource.h"

#define W(x)  (static_cast<unsigned int>(x) & 0xffff)

/* Stock fill ratio slots (ptr+0x33c + 2 * slot). */
#define FILL_BOATBUILDER_PLANK  0
#define FILL_STONEMINE_FOOD     1
#define FILL_COALMINE_FOOD      2
#define FILL_IRONMINE_FOOD      3
#define FILL_GOLDMINE_FOOD      4
#define FILL_TOOLMAKER_PLANK   11

#define DONE(type)        W(player->completed_building_count[type])
#define INCOMPLETE(type)  W(player->incomplete_building_count[type])
#define BOTH(type)        W(DONE(type) + INCOMPLETE(type))
#define FILL(slot)        W(*stat(player, 0x33c + 2*(slot)))
#define IDLE(serf)        W(*stat(player, 0x366 + 2*(serf)))
#define STOCK(res)        W(*stat(player, 0x39c + 2*(res)))
#define WANT(type)        (player->ai.build_want[(type)-1])
#define DAMP(type)        W(player->ai.build_damp[(type)-1])
#define LOC(cat)          (player->ai.locations[cat])

/* Idle generic serfs (ptr+0x390). */
#define IDLE_GENERIC  IDLE(Serf::TypeGeneric)

/* Word at Amiga pointer-relative offset off (0x33c..0x3ce) of the
   statistics area: poi[4] and u_394..u_3ce. */
int *AI::Want::stat(Player *player, int off) {
  static const size_t u_offset[] = {
    offsetof(PlayerAI, u_394), offsetof(PlayerAI, u_396),
    offsetof(PlayerAI, u_398), offsetof(PlayerAI, u_39a),
    offsetof(PlayerAI, u_39c), offsetof(PlayerAI, u_39e),
    offsetof(PlayerAI, u_3a0), offsetof(PlayerAI, u_3a2),
    offsetof(PlayerAI, u_3a4), offsetof(PlayerAI, u_3a6),
    offsetof(PlayerAI, u_3a8), offsetof(PlayerAI, u_3aa),
    offsetof(PlayerAI, u_3ac), offsetof(PlayerAI, u_3ae),
    offsetof(PlayerAI, u_3b0), offsetof(PlayerAI, u_3b2),
    offsetof(PlayerAI, u_3b4), offsetof(PlayerAI, u_3b6),
    offsetof(PlayerAI, u_3b8), offsetof(PlayerAI, u_3ba),
    offsetof(PlayerAI, u_3bc), offsetof(PlayerAI, u_3be),
    offsetof(PlayerAI, u_3c0), offsetof(PlayerAI, u_3c2),
    offsetof(PlayerAI, u_3c4), offsetof(PlayerAI, u_3c6),
    offsetof(PlayerAI, u_3c8), offsetof(PlayerAI, u_3ca),
    offsetof(PlayerAI, u_3cc), offsetof(PlayerAI, u_3ce)
  };
  AIPoi *p = &player->ai.poi[4];
  int w;

  if (off >= 0x394) {
    return reinterpret_cast<int *>(reinterpret_cast<char *>(&player->ai) +
                                   u_offset[(off - 0x394) / 2]);
  }

  w = (off - 0x33c) / 2;
  if (w >= 5 && w < 30) return &p->bld_count[w - 5];
  if (w >= 33 && w < 37) return &p->deposit[w - 33];
  switch (w) {
    case 0: return &p->field_0;
    case 1: return &p->field_2;
    case 2: return &p->field_4;
    case 3: return &p->field_6;
    case 4: return &p->field_8;
    case 30: return &p->trees;
    case 31: return &p->field_3e;
    case 32: return &p->stones;
    case 37: return &p->field_4a;
    case 38: return &p->field_4c;
    case 39: return &p->field_4e;
    case 40: return &p->field_50;
    case 41: return &p->field_52;
    case 42: return &p->field_54;
    default: return &p->field_56;
  }
}

unsigned int AI::Want::sadd(unsigned int a, unsigned int b) {
  a = W(a) + W(b);
  return a > 0xffff ? 0xffff : a;
}

unsigned int AI::Want::mulhi(unsigned int a, unsigned int b) {
  return (W(a) * W(b)) >> 16;
}

unsigned int AI::Want::min16(unsigned int a, unsigned int b) {
  return W(a) < W(b) ? W(a) : W(b);
}

/* Recurring curve of the original: have (D1) against need (D0).
   have < need: ~((have << 16 >> shift) / need), else falling from
   base with the surplus. */
unsigned int AI::Want::want_curve(unsigned int have, unsigned int need,
                                  int shift, int step, unsigned int base) {
  if (have < need) return W(~(((have << 16) >> shift) / need));
  have -= need;
  if (have >= 0x10) return 0;
  return W(~(have << step) + base);
}

/* Shared tail of ai_want_fisher and ai_want_pigfarm: split want between
   the type itself (return value, D1) and the related type (*second,
   D2) by the site scales s1, s2. */
unsigned int AI::Want::want_share(unsigned int want, unsigned int s1,
                                  unsigned int s2, int *second) {
  if (s1 == 0) {
    *second = (s2 != 0) ? want : 0;
    return 0;
  }
  if (s2 == 0) {
    /* The original leaves the site scale in D1 here, so the
       type gets the scale as want instead of want. */
    *second = 0;
    return s1;
  }
  if (s1 == s2) {
    *second = want;
    return want;
  }
  if (s2 > s1) {
    *second = want;
    return mulhi((s1 << 16) / s2, want);
  }
  *second = mulhi((s2 << 16) / s1, want);
  return want;
}

/* Stone needed by the incomplete buildings (0x2bc8c/0x2bdae). */
unsigned int AI::Want::stone_needed(Player *player) {
  unsigned int d1 = sadd(INCOMPLETE(Building::TypeFortress),
                         INCOMPLETE(Building::TypeFortress));
  d1 = sadd(d1, INCOMPLETE(Building::TypeStock));
  d1 = sadd(d1, INCOMPLETE(Building::TypeFarm));
  d1 = sadd(d1, INCOMPLETE(Building::TypeButcher));
  d1 = sadd(d1, INCOMPLETE(Building::TypePigFarm));
  d1 = sadd(d1, INCOMPLETE(Building::TypeMill));
  d1 = sadd(d1, INCOMPLETE(Building::TypeBaker));
  d1 = sadd(d1, INCOMPLETE(Building::TypeSawmill));
  d1 = sadd(d1, INCOMPLETE(Building::TypeSteelSmelter));
  d1 = sadd(d1, INCOMPLETE(Building::TypeToolMaker));
  d1 = sadd(d1, INCOMPLETE(Building::TypeWeaponSmith));
  d1 = sadd(d1, INCOMPLETE(Building::TypeTower));
  d1 = sadd(d1, INCOMPLETE(Building::TypeGoldSmelter));
  d1 = sadd(d1, d1);
  d1 = sadd(d1, INCOMPLETE(Building::TypeStoneMine));
  d1 = sadd(d1, INCOMPLETE(Building::TypeHut));
  d1 = sadd(d1, 8);
  return d1;
}

/* Miners available for mine type with the stonecutter correction
   (common head of the four mine functions, 0x2bd4e). */
unsigned int AI::Want::miners_available(Player *player) {
  unsigned int d0 = min16(IDLE_GENERIC, STOCK(Resource::TypePick));
  unsigned int d1 = INCOMPLETE(Building::TypeStonecutter);
  if (d1 >= IDLE(Serf::TypeStonecutter)) {
    d1 -= IDLE(Serf::TypeStonecutter);
    d0 = (d0 >= d1) ? d0 - d1 : 0;
  }
  return W(d0 + IDLE(Serf::TypeMiner));
}

/* Incomplete mines of all four kinds. */
unsigned int AI::Want::incomplete_mines(Player *player) {
  return W(INCOMPLETE(Building::TypeStoneMine) +
           INCOMPLETE(Building::TypeCoalMine) +
           INCOMPLETE(Building::TypeIronMine) +
           INCOMPLETE(Building::TypeGoldMine));
}

/* Knights available for military buildings (0x2c154). */
unsigned int AI::Want::knights_available(Player *player) {
  unsigned int d0 = min16(IDLE_GENERIC, STOCK(Resource::TypeSword));
  d0 = min16(d0, STOCK(Resource::TypeShield));
  d0 = W(d0 + IDLE(Serf::TypeKnight0) + IDLE(Serf::TypeKnight1) +
         IDLE(Serf::TypeKnight2) + IDLE(Serf::TypeKnight3) +
         IDLE(Serf::TypeKnight4));
  return d0;
}

unsigned int AI::Want::incomplete_military(Player *player) {
  return W(INCOMPLETE(Building::TypeHut) + INCOMPLETE(Building::TypeTower) +
           INCOMPLETE(Building::TypeFortress));
}

/* ai_want_scale @0x2cc16: mean value of the 8 stored sites of a
   category. */
unsigned int AI::Want::want_scale(AILocation *loc) {
  uint32_t sum = 0;
  int i;
  for (i = 0; i < 8; i++) sum += W(loc[i].value);
  return W(sum >> 3);
}

/* ai_want_scale_mine @0x2cc5c: mean of the two best stored sites of a
   category. */
unsigned int AI::Want::want_scale_mine(AILocation *loc) {
  unsigned int d6 = 0, d7 = 0;
  int i;
  /* Original bug: the unrolled loop reads (A3) eight times without
     advancing, so the result is just the value of the first site.
     Each of the 8 sites is used here. */
  for (i = 0; i < 8; i++) {
    unsigned int v = W(loc[i].value);
    if (v > d6) {
      d6 = v;
      if (d6 > d7) {
        unsigned int t = d6;
        d6 = d7;
        d7 = t;
      }
    }
  }
  return W((d6 >> 1) + (d7 >> 1));
}

/* 0x2b8c4: type 25 (pseudo building type). */
void AI::Want::want_type25(Player *player) {
  if (W(IDLE(Serf::TypeTransporter) + IDLE_GENERIC) < 3) return;
  if (want_scale(LOC(0)) == 0) return;
  WANT(25) = mulhi(0xdac0, DAMP(25));
}

/* 0x2b8f0: fisher; also the preliminary want of farm (finished by
   ai_want_farm). */
void AI::Want::want_fisher(Player *player) {
  unsigned int d0, d1, d2, d3, s1, s2;
  int farm;

  /* Food demand of the mines weighted by their food stock fill. */
  d1 = BOTH(Building::TypeStoneMine);
  d3 = (d1 < 0x2000 ? d1 : 0x1fff) << 3;
  d0 = mulhi(d3, FILL(FILL_STONEMINE_FOOD));

  d2 = BOTH(Building::TypeCoalMine);
  d1 = W(d1 + d2);
  d3 = (d2 < 0x2000 ? d2 : 0x1fff) << 3;
  d0 = W(d0 + mulhi(d3, FILL(FILL_COALMINE_FOOD)));

  d2 = BOTH(Building::TypeIronMine);
  d1 = W(d1 + d2);
  d3 = (d2 < 0x2000 ? d2 : 0x1fff) << 3;
  d0 = W(d0 + mulhi(d3, FILL(FILL_IRONMINE_FOOD)));

  d2 = BOTH(Building::TypeGoldMine);
  d1 = W(d1 + d2);
  d3 = (d2 < 0x2000 ? d2 : 0x1fff) << 3;
  d0 = sadd(d0, mulhi(d3, FILL(FILL_GOLDMINE_FOOD)));

  /* Food in stock. */
  d0 = sadd(d0, STOCK(Resource::TypeFish));
  d0 = sadd(d0, STOCK(Resource::TypeMeat));
  d0 = sadd(d0, STOCK(Resource::TypeBread));
  if (DONE(Building::TypeButcher) != 0) d0 = sadd(d0, STOCK(Resource::TypePig));
  if (DONE(Building::TypeBaker) != 0) {
    d0 = sadd(d0, STOCK(Resource::TypeFlour));
    if (DONE(Building::TypeMill) != 0) {
      d0 = sadd(d0, STOCK(Resource::TypeWheat));
    }
  }

  if (d1 < 8) {
    d0 = W(d0 + d0);
    if (d1 < 4) {
      d0 = W(d0 + d0);
      if (d1 < 2) d0 = W(d0 + d0);
    }
  }

  if (d1 >= 0x2000) d1 = 0x1fff;
  d1 <<= 3;
  d0 = (d0 >= d1) ? d0 - d1 : 0;

  if (d0 < 0x20) {
    d0 = W(~(d0 << 10));
  } else if (d0 < 0x50) {
    d0 = W(~(d0 << 9) - 0x4000);
  } else if (d0 < 0x70) {
    d0 = W(~(d0 << 8) + 0x7001);
  } else {
    d0 = 0;
  }

  s1 = want_scale(LOC(1));
  s2 = want_scale(LOC(12));
  d1 = want_share(d0, s1, s2, &farm);
  WANT(Building::TypeFarm) = farm;

  d0 = min16(IDLE_GENERIC, STOCK(Resource::TypeRod));
  d0 = W(d0 + IDLE(Serf::TypeFisher));
  if (INCOMPLETE(Building::TypeFisher) >= d0) return;

  WANT(Building::TypeFisher) = mulhi(d1, DAMP(Building::TypeFisher));
}

/* 0x2ba88: lumberjack. */
void AI::Want::want_lumberjack(Player *player) {
  unsigned int d0, d1, d2;

  d0 = min16(IDLE_GENERIC, STOCK(Resource::TypeAxe));
  d0 = W(d0 + IDLE(Serf::TypeLumberjack));
  if (INCOMPLETE(Building::TypeLumberjack) >= d0) return;

  if (BOTH(Building::TypeLumberjack) == 0) {
    WANT(Building::TypeLumberjack) = 0xffff;
    return;
  }

  /* Planks and lumber available. */
  d0 = sadd(STOCK(Resource::TypeLumber), STOCK(Resource::TypePlank));
  d2 = DONE(Building::TypeBoatbuilder);
  d2 = (d2 < 0x2000 ? d2 : 0x1fff) << 3;
  d0 = sadd(d0, mulhi(d2, FILL(FILL_BOATBUILDER_PLANK)));
  d2 = DONE(Building::TypeToolMaker);
  d2 = (d2 < 0x2000 ? d2 : 0x1fff) << 3;
  d0 = sadd(d0, mulhi(d2, FILL(FILL_TOOLMAKER_PLANK)));

  /* Planks needed. */
  d1 = sadd(DONE(Building::TypeToolMaker), INCOMPLETE(Building::TypeFortress));
  d1 = sadd(d1, d1);
  d1 = sadd(d1, DONE(Building::TypeBoatbuilder));
  d1 = sadd(d1, INCOMPLETE(Building::TypeStoneMine));
  d1 = sadd(d1, INCOMPLETE(Building::TypeCoalMine));
  d1 = sadd(d1, INCOMPLETE(Building::TypeIronMine));
  d1 = sadd(d1, INCOMPLETE(Building::TypeGoldMine));
  d1 = sadd(d1, INCOMPLETE(Building::TypeStock));
  d1 = sadd(d1, INCOMPLETE(Building::TypeFarm));
  d1 = sadd(d1, INCOMPLETE(Building::TypeButcher));
  d1 = sadd(d1, INCOMPLETE(Building::TypePigFarm));
  d1 = sadd(d1, INCOMPLETE(Building::TypeMill));
  d1 = sadd(d1, INCOMPLETE(Building::TypeBaker));
  d1 = sadd(d1, INCOMPLETE(Building::TypeSawmill));
  d1 = sadd(d1, INCOMPLETE(Building::TypeSteelSmelter));
  d1 = sadd(d1, INCOMPLETE(Building::TypeToolMaker));
  d1 = sadd(d1, INCOMPLETE(Building::TypeWeaponSmith));
  d1 = sadd(d1, INCOMPLETE(Building::TypeTower));
  d1 = sadd(d1, INCOMPLETE(Building::TypeGoldSmelter));
  d1 = sadd(d1, d1);
  d1 = sadd(d1, INCOMPLETE(Building::TypeFisher));
  d1 = sadd(d1, INCOMPLETE(Building::TypeLumberjack));
  d1 = sadd(d1, INCOMPLETE(Building::TypeBoatbuilder));
  d1 = sadd(d1, INCOMPLETE(Building::TypeStonecutter));
  d1 = sadd(d1, INCOMPLETE(Building::TypeForester));
  d1 = sadd(d1, INCOMPLETE(Building::TypeHut));

  d0 = (d0 >= d1) ? d0 - d1 : 0;

  if (d0 < 0x40) {
    d0 = W(~(d0 << 9));
  } else if (d0 < 0x70) {
    /* Original bug: subtracts 0x4000 here as well (copied from
       ai_want_fisher), which wraps for 0x60..0x6f to nearly
       0xffff. Without it the curve joins both neighbours. */
    d0 = W(~(d0 << 9));
  } else if (d0 < 0x90) {
    d0 = W(~(d0 << 8) - 0x6fff);
  } else {
    d0 = 0;
  }

  WANT(Building::TypeLumberjack) = mulhi(d0, DAMP(Building::TypeLumberjack));
}

/* 0x2bbe0: boatbuilder; also sets the planks priority of
   boatbuilders. */
void AI::Want::want_boatbuilder(Player *player) {
  unsigned int d0;

  d0 = min16(IDLE_GENERIC, STOCK(Resource::TypeHammer));
  d0 = W(d0 + IDLE(Serf::TypeBoatBuilder));
  if (INCOMPLETE(Building::TypeBoatbuilder) >= d0) return;

  d0 = STOCK(Resource::TypeBoat);
  if (d0 == 0) {
    d0 = 0x88b8;
  } else if (d0 < 2) {
    d0 = 0x4e20;
  } else if (d0 < 8) {
    d0 = W(~(d0 << 10) + 0x2800);
  } else {
    d0 = 0;
  }

  player->planks_boatbuilder = d0;
  WANT(Building::TypeBoatbuilder) = mulhi(d0, DAMP(Building::TypeBoatbuilder));
}

/* 0x2bc38: stonecutter. */
void AI::Want::want_stonecutter(Player *player) {
  unsigned int d0, d1;

  /* Picks are kept for missing coal and iron mines and for the
     miners needed by incomplete mines. */
  d0 = min16(IDLE_GENERIC, STOCK(Resource::TypePick));
  if (DONE(Building::TypeCoalMine) == 0 && d0 > 0) d0 -= 1;
  if (DONE(Building::TypeIronMine) == 0 && d0 > 0) d0 -= 1;
  d1 = incomplete_mines(player);
  if (d1 >= IDLE(Serf::TypeMiner)) {
    d1 -= IDLE(Serf::TypeMiner);
    d0 = (d0 >= d1) ? d0 - d1 : 0;
  }
  d0 = W(d0 + IDLE(Serf::TypeStonecutter));
  if (INCOMPLETE(Building::TypeStonecutter) >= d0) return;

  d0 = STOCK(Resource::TypeStone);
  d1 = stone_needed(player);
  d0 = (d0 >= d1) ? d0 - d1 : 0;

  if (d0 < 0x10) {
    d0 = W(~(d0 << 11));
  } else if (d0 < 0x40) {
    d0 = W(~(d0 << 9) - 0x6000);
  } else if (d0 < 0x140) {
    d0 = W(~(d0 << 5) + 0x2801);
  } else {
    d0 = 0;
  }

  d1 = want_scale(LOC(4));
  if (d1 >= 0x400) d1 = 0x3ff;
  d0 = mulhi(d0, d1 << 6);
  WANT(Building::TypeStonecutter) = mulhi(d0, DAMP(Building::TypeStonecutter));
}

/* 0x2bd4e: stonemine. */
void AI::Want::want_stonemine(Player *player) {
  unsigned int d0, d1, d2;

  d0 = miners_available(player);
  if (DONE(Building::TypeCoalMine) == 0) {
    if (d0 == 0) return;
    d0 -= 1;
  }
  if (DONE(Building::TypeIronMine) == 0) {
    if (d0 == 0) return;
    d0 -= 1;
  }
  if (d0 <= incomplete_mines(player)) return;

  d0 = want_scale_mine(LOC(5));
  d2 = STOCK(Resource::TypeStone);
  d1 = stone_needed(player);
  if (d2 < d1) {
    if (d0 >= 0xff0) d0 = 0xfef;
    d0 <<= 4;
  } else {
    if (d0 >= 0x1f40) d0 = 0x1f3f;
    d1 = W(STOCK(Resource::TypeStone) << 4);
    d0 = (d0 >= d1) ? d0 - d1 : 0;
    d0 = W(d0 << 3);
  }

  WANT(Building::TypeStoneMine) = mulhi(d0, DAMP(Building::TypeStoneMine));
}

/* 0x2be4a: coalmine. */
void AI::Want::want_coalmine(Player *player) {
  unsigned int d0, d1;

  d0 = miners_available(player);
  if (DONE(Building::TypeIronMine) == 0) {
    if (d0 == 0) return;
    d0 -= 1;
  }
  if (incomplete_mines(player) >= d0) return;

  d0 = want_scale_mine(LOC(6));
  if (d0 >= 0x1f40) d0 = 0x1f3f;
  d1 = W(STOCK(Resource::TypeCoal) << 2);
  d0 = (d0 >= d1) ? d0 - d1 : 0;
  d0 <<= 3;
  if (d0 > 0xffff) d0 = 0xffff;

  WANT(Building::TypeCoalMine) = mulhi(d0, DAMP(Building::TypeCoalMine));
}

/* 0x2bec0: ironmine. */
void AI::Want::want_ironmine(Player *player) {
  unsigned int d0, d1;

  d0 = miners_available(player);
  if (DONE(Building::TypeCoalMine) == 0) {
    if (d0 == 0) return;
    d0 -= 1;
  }
  if (incomplete_mines(player) >= d0) return;

  d0 = want_scale_mine(LOC(7));
  if (d0 >= 0x1f40) d0 = 0x1f3f;
  d1 = W(STOCK(Resource::TypeIronOre) << 3);
  d0 = (d0 >= d1) ? d0 - d1 : 0;
  d0 <<= 3;
  if (d0 > 0xffff) d0 = 0xffff;

  WANT(Building::TypeIronMine) = mulhi(d0, DAMP(Building::TypeIronMine));
}

/* 0x2bf36: goldmine. */
void AI::Want::want_goldmine(Player *player) {
  unsigned int d0, d1;

  d0 = miners_available(player);
  if (DONE(Building::TypeCoalMine) == 0) {
    if (d0 == 0) return;
    d0 -= 1;
  }
  if (DONE(Building::TypeIronMine) == 0) {
    if (d0 == 0) return;
    d0 -= 1;
  }
  if (incomplete_mines(player) >= d0) return;

  d0 = want_scale_mine(LOC(8));
  if (d0 >= 0x7f8) d0 = 0x7f7;
  d1 = STOCK(Resource::TypeGoldOre);
  d0 = (d0 >= d1) ? d0 - d1 : 0;
  d0 <<= 5;
  if (d0 > 0xffff) d0 = 0xffff;

  WANT(Building::TypeGoldMine) = mulhi(d0, DAMP(Building::TypeGoldMine));
}

/* 0x2bfb6: forester. */
void AI::Want::want_forester(Player *player) {
  unsigned int d0, d1;

  d0 = W(IDLE_GENERIC + IDLE(Serf::TypeForester));
  if (INCOMPLETE(Building::TypeForester) >= d0) return;

  d0 = BOTH(Building::TypeLumberjack);
  d1 = BOTH(Building::TypeForester);
  if (d1 < d0) {
    d0 = want_scale(LOC(9));
    if (d0 >= 0x7d0) d0 = 0x7cf;
    d0 <<= 5;
  } else if (d1 == 0) {
    WANT(Building::TypeForester) = 0;
    return;
  } else if (d0 == 0) {
    /* Original bug: ((0 << 16) - 1) / d1 overflows divu, which
       leaves 0xffffffff and gives a want of 0x7ff * damp. No
       lumberjacks means no foresters are wanted. */
    d0 = 0;
  } else {
    d0 = (((static_cast<uint32_t>(d0) << 16) - 1) / d1) >> 5;
  }

  WANT(Building::TypeForester) = mulhi(d0, DAMP(Building::TypeForester));
}

/* 0x2c012: stock. */
void AI::Want::want_stock(Player *player) {
  unsigned int d0, d1;

  if (INCOMPLETE(Building::TypeStock) >= IDLE_GENERIC) return;

  /* Large buildings count double. */
  d0 = W(BOTH(Building::TypeStoneMine) + BOTH(Building::TypeCoalMine) +
         BOTH(Building::TypeIronMine) + BOTH(Building::TypeGoldMine) +
         BOTH(Building::TypeSteelSmelter) + BOTH(Building::TypeToolMaker) +
         BOTH(Building::TypeWeaponSmith) + BOTH(Building::TypeGoldSmelter));
  d0 = sadd(d0, d0);
  d0 = sadd(d0, INCOMPLETE(Building::TypeFisher));
  d0 = sadd(d0, DONE(Building::TypeFisher));
  d0 = sadd(d0, INCOMPLETE(Building::TypeLumberjack));
  d0 = sadd(d0, DONE(Building::TypeLumberjack));
  d0 = sadd(d0, INCOMPLETE(Building::TypeBoatbuilder));
  d0 = sadd(d0, DONE(Building::TypeBoatbuilder));
  d0 = sadd(d0, INCOMPLETE(Building::TypeStonecutter));
  d0 = sadd(d0, DONE(Building::TypeStonecutter));
  d0 = sadd(d0, INCOMPLETE(Building::TypeFarm));
  d0 = sadd(d0, DONE(Building::TypeFarm));
  d0 = sadd(d0, INCOMPLETE(Building::TypeButcher));
  d0 = sadd(d0, DONE(Building::TypeButcher));
  d0 = sadd(d0, INCOMPLETE(Building::TypePigFarm));
  d0 = sadd(d0, DONE(Building::TypePigFarm));
  d0 = sadd(d0, INCOMPLETE(Building::TypeMill));
  d0 = sadd(d0, DONE(Building::TypeMill));
  d0 = sadd(d0, INCOMPLETE(Building::TypeBaker));
  d0 = sadd(d0, DONE(Building::TypeBaker));
  d0 = sadd(d0, INCOMPLETE(Building::TypeSawmill));
  d0 = sadd(d0, DONE(Building::TypeSawmill));

  if (STOCK(Resource::TypePlank) >= 0x50) {
    d0 = (d0 >> 4) + 4;
  } else if (STOCK(Resource::TypePlank) >= 0x28) {
    d0 = (d0 >> 4) + 2;
  } else if (d0 < 0x20) {
    d0 = 0;
  } else if (d0 < 0x40) {
    d0 = ((d0 - 0x20) >> 3) + 2;
  } else {
    d0 = (d0 >> 4) + 2;
  }

  d1 = BOTH(Building::TypeStock);
  d1 = want_curve(d1, d0, 2, 8, 0x1001);
  if (d1 >= 0xfeb0) d1 = 0xfeaf;

  WANT(Building::TypeStock) = mulhi(d1, DAMP(Building::TypeStock));
}

/* 0x2c154: hut. */
void AI::Want::want_hut(Player *player) {
  unsigned int d0;

  if (incomplete_military(player) >= knights_available(player)) {
    return;
  }

  d0 = want_scale(LOC(11));
  if (d0 >= 0x4000) d0 = 0x3fff;
  d0 <<= 2;
  if (d0 >= W(player->ai_value_5)) d0 = W(player->ai_value_5);

  WANT(Building::TypeHut) = mulhi(d0, DAMP(Building::TypeHut));
}

/* 0x2c1c0: farm (want prepared by ai_want_fisher). */
void AI::Want::want_farm(Player *player) {
  unsigned int d0;

  d0 = min16(IDLE_GENERIC, STOCK(Resource::TypeScythe));
  d0 = W(d0 + IDLE(Serf::TypeFarmer));
  if (INCOMPLETE(Building::TypeFarm) >= d0) {
    WANT(Building::TypeFarm) = 0;
    return;
  }

  WANT(Building::TypeFarm) = mulhi(WANT(Building::TypeFarm),
                                   DAMP(Building::TypeFarm));
}

/* 0x2c1f0: butcher. */
void AI::Want::want_butcher(Player *player) {
  unsigned int d0, d1;

  d0 = min16(IDLE_GENERIC, STOCK(Resource::TypeCleaver));
  d0 = W(d0 + IDLE(Serf::TypeButcher));
  if (INCOMPLETE(Building::TypeButcher) >= d0) return;

  d0 = BOTH(Building::TypePigFarm);
  d1 = BOTH(Building::TypeButcher);
  if (d1 >= 0x4000) d1 = 0x3fff;
  d1 <<= 2;
  d1 = want_curve(d1, d0, 2, 7, 0x801);

  d0 = STOCK(Resource::TypePig);
  if (d0 >= 0x200) d0 = 0x1ff;
  d0 = sadd(d0 << 7, d1);

  WANT(Building::TypeButcher) = mulhi(d0, DAMP(Building::TypeButcher));
}

/* 0x2c26e: pigfarm; also the preliminary want of mill (finished by
   ai_want_mill). */
void AI::Want::want_pigfarm(Player *player) {
  unsigned int d0, d1, d2, s1, s2;
  int mill;

  d0 = W(IDLE_GENERIC + IDLE(Serf::TypePigFarmer));
  if (INCOMPLETE(Building::TypePigFarm) >= d0) return;

  /* Wheat production. */
  d0 = BOTH(Building::TypeFarm);
  if (d0 >= 0x4000) d0 = 0x3fff;
  if (d0 == 0) return;
  d0 <<= 2;
  d1 = sadd(STOCK(Resource::TypeWheat), 0x20) >> 6;
  d0 = sadd(d0, d1);

  /* Wheat consumption: 12 per mill, 3 per pigfarm. */
  d1 = BOTH(Building::TypeMill);
  d1 = sadd(d1, d1);
  d1 = sadd(d1, d1);
  d2 = d1;
  d1 = sadd(d1, d1);
  d1 = sadd(d1, d2);
  d2 = BOTH(Building::TypePigFarm) * 3;
  d1 = (d2 > 0xffff) ? 0xffff : sadd(d1, d2);

  d1 = want_curve(d1, d0, 2, 7, 0x801);

  d0 = STOCK(Resource::TypeWheat);
  if (d0 >= 0x200) d0 = 0x1ff;
  d0 = sadd(d0 << 7, d1);

  s1 = want_scale(LOC(14));
  s2 = want_scale(LOC(15));
  d1 = want_share(d0, s1, s2, &mill);
  WANT(Building::TypeMill) = mill;

  WANT(Building::TypePigFarm) = mulhi(d1, DAMP(Building::TypePigFarm));
}

/* 0x2c382: mill (want prepared by ai_want_pigfarm). */
void AI::Want::want_mill(Player *player) {
  unsigned int d0;

  d0 = W(IDLE_GENERIC + IDLE(Serf::TypeMiller));
  if (INCOMPLETE(Building::TypeMill) >= d0) {
    WANT(Building::TypeMill) = 0;
    return;
  }

  WANT(Building::TypeMill) = mulhi(WANT(Building::TypeMill),
                                   DAMP(Building::TypeMill));
}

/* 0x2c3a8: baker. */
void AI::Want::want_baker(Player *player) {
  unsigned int d0, d1;

  d0 = W(IDLE_GENERIC + IDLE(Serf::TypeBaker));
  if (INCOMPLETE(Building::TypeBaker) >= d0) return;

  d0 = BOTH(Building::TypeMill);
  d1 = BOTH(Building::TypeBaker);
  d1 = want_curve(d1, d0, 2, 7, 0x801);

  d0 = STOCK(Resource::TypeFlour);
  if (d0 >= 0x200) d0 = 0x1ff;
  d0 = sadd(d0 << 7, d1);

  WANT(Building::TypeBaker) = mulhi(d0, DAMP(Building::TypeBaker));
}

/* 0x2c410: sawmill. */
void AI::Want::want_sawmill(Player *player) {
  unsigned int d0, d1;

  d0 = min16(IDLE_GENERIC, STOCK(Resource::TypeSaw));
  d0 = W(d0 + IDLE(Serf::TypeSawmiller));
  if (INCOMPLETE(Building::TypeSawmill) >= d0) return;

  d0 = BOTH(Building::TypeLumberjack);
  d1 = sadd(STOCK(Resource::TypeLumber), 0x20) >> 5;
  d0 = sadd(d0, d1);

  d1 = BOTH(Building::TypeSawmill) * 3;
  if (d1 > 0xffff) d1 = 0xffff;
  d1 = want_curve(d1, d0, 2, 7, 0x801);

  d0 = STOCK(Resource::TypeLumber);
  if (d0 >= 0x200) d0 = 0x1ff;
  d0 = sadd(d0 << 7, d1);

  WANT(Building::TypeSawmill) = mulhi(d0, DAMP(Building::TypeSawmill));
}

/* 0x2c4a2: steelsmelter. */
void AI::Want::want_steelsmelter(Player *player) {
  unsigned int d0, d1;

  d0 = W(IDLE_GENERIC + IDLE(Serf::TypeSmelter));
  d1 = W(INCOMPLETE(Building::TypeSteelSmelter) +
         INCOMPLETE(Building::TypeGoldSmelter));
  if (d1 >= d0) return;

  d0 = min16(BOTH(Building::TypeCoalMine), BOTH(Building::TypeIronMine));
  d1 = min16(STOCK(Resource::TypeIronOre), STOCK(Resource::TypeCoal)) >> 4;
  d0 = W(d0 + d1 + 1) >> 1;

  d1 = BOTH(Building::TypeSteelSmelter);
  d1 = want_curve(d1, d0, 2, 7, 0x801);

  d0 = STOCK(Resource::TypeIronOre);
  if (d0 >= 0x400) d0 = 0x3ff;
  d0 = sadd(d0 << 6, d1);

  WANT(Building::TypeSteelSmelter) =
    mulhi(d0, DAMP(Building::TypeSteelSmelter));
}

/* 0x2c532: toolmaker. */
void AI::Want::want_toolmaker(Player *player) {
  unsigned int d0, d1, d2;
  int i;

  d0 = min16(IDLE_GENERIC, STOCK(Resource::TypeSaw));
  d0 = min16(d0, STOCK(Resource::TypeHammer));
  d0 = W(d0 + IDLE(Serf::TypeToolmaker));
  if (INCOMPLETE(Building::TypeToolMaker) >= d0) return;

  /* Highest tool priority. */
  d2 = W(player->tool_prio[0]);
  for (i = 1; i < 9; i++) {
    if (W(player->tool_prio[i]) > d2) d2 = W(player->tool_prio[i]);
  }

  d0 = d2;
  if (BOTH(Building::TypeToolMaker) != 0) {
    d1 = min16(STOCK(Resource::TypePlank), STOCK(Resource::TypeSteel));
    d2 = min16(BOTH(Building::TypeLumberjack), BOTH(Building::TypeIronMine));
    d1 = W(d1 + W(d2 << 4));
    d2 = W(BOTH(Building::TypeToolMaker) << 6);
    d1 = (d1 >= d2) ? d1 - d2 : 0;
    if (d1 >= 0x10) d1 = 0xf;
    d0 = mulhi(d0, d1 << 12);
  }

  WANT(Building::TypeToolMaker) = mulhi(d0, DAMP(Building::TypeToolMaker));
}

/* 0x2c610: weaponsmith. */
void AI::Want::want_weaponsmith(Player *player) {
  unsigned int d0, d1, d2;

  d0 = min16(IDLE_GENERIC, STOCK(Resource::TypePincer));
  d0 = min16(d0, STOCK(Resource::TypeHammer));
  d0 = W(d0 + IDLE(Serf::TypeWeaponSmith));
  if (INCOMPLETE(Building::TypeWeaponSmith) >= d0) return;

  d0 = min16(STOCK(Resource::TypeSword), STOCK(Resource::TypeShield));
  if (d0 < 0x40) {
    d0 = W(~(d0 << 9));
  } else if (d0 < 0x100) {
    d0 = W(~(d0 << 7) - 0x6000);
  } else if (d0 < 0x500) {
    d0 = W(~(d0 << 3) + 0x2801);
  } else {
    d0 = 0;
  }

  d1 = W(min16(STOCK(Resource::TypeSteel), STOCK(Resource::TypeCoal)) + 7);
  d2 = min16(BOTH(Building::TypeCoalMine), BOTH(Building::TypeIronMine));
  d1 = W(d1 + W(d2 << 3));
  d2 = W(BOTH(Building::TypeWeaponSmith) << 4);
  d1 = (d1 >= d2) ? d1 - d2 : 0;
  if (d1 >= 0x10) d1 = 0xf;
  d1 = (d1 << 12) + 0xfa0;
  d0 = mulhi(d0, d1);

  WANT(Building::TypeWeaponSmith) = mulhi(d0, DAMP(Building::TypeWeaponSmith));
}

/* 0x2c6ce: tower; also the preliminary want of fortress (finished by
   ai_want_fortress). */
void AI::Want::want_tower(Player *player) {
  unsigned int d0, d1;

  if (W(player->ai.u_1aa) < 8) return;
  if (incomplete_military(player) >= knights_available(player)) {
    return;
  }

  /* Buildings to protect. */
  d0 = W(DONE(Building::TypeStoneMine) + DONE(Building::TypeCoalMine) +
         DONE(Building::TypeIronMine) + DONE(Building::TypeGoldMine) +
         DONE(Building::TypeFarm) + DONE(Building::TypeButcher) +
         DONE(Building::TypePigFarm) + DONE(Building::TypeBaker) +
         DONE(Building::TypeSawmill) + DONE(Building::TypeSteelSmelter) +
         DONE(Building::TypeToolMaker) + DONE(Building::TypeWeaponSmith) +
         DONE(Building::TypeGoldSmelter));
  d1 = DONE(Building::TypeStock);
  d1 = (d1 < 0x2000 ? d1 : 0x1fff) << 3;
  d0 = sadd(d0, d1);

  /* Large military buildings. */
  d1 = W(DONE(Building::TypeFortress) + 1);
  d1 = sadd(d1, d1);
  d1 = sadd(d1, DONE(Building::TypeTower));
  if (d1 >= 0x800) d1 = 0x7ff;
  d1 <<= 3;
  d1 = want_curve(d1, d0, 1, 7, 0x801);

  WANT(Building::TypeFortress) = d1;
  WANT(Building::TypeTower) = mulhi(d1, DAMP(Building::TypeTower));
}

/* 0x2c7b2: fortress (want prepared by ai_want_tower). */
void AI::Want::want_fortress(Player *player) {
  if (W(player->ai.u_1aa) < 10) return;
  if (incomplete_military(player) >= knights_available(player)) {
    WANT(Building::TypeFortress) = 0;
    return;
  }

  WANT(Building::TypeFortress) = mulhi(WANT(Building::TypeFortress),
                                       DAMP(Building::TypeFortress));
}

/* 0x2c80e: goldsmelter. */
void AI::Want::want_goldsmelter(Player *player) {
  unsigned int d0, d1;

  d0 = W(IDLE_GENERIC + IDLE(Serf::TypeSmelter));
  d1 = W(INCOMPLETE(Building::TypeSteelSmelter) +
         INCOMPLETE(Building::TypeGoldSmelter));
  if (d1 >= d0) return;

  d0 = min16(BOTH(Building::TypeCoalMine), BOTH(Building::TypeGoldMine));
  d1 = min16(STOCK(Resource::TypeGoldOre), STOCK(Resource::TypeCoal)) >> 4;
  d0 = W(d0 + d1 + 1) >> 1;

  d1 = BOTH(Building::TypeGoldSmelter);
  d1 = want_curve(d1, d0, 2, 7, 0x801);

  d0 = STOCK(Resource::TypeGoldOre);
  if (d0 >= 0x400) d0 = 0x3ff;
  d0 = sadd(d0 << 6, d1);

  WANT(Building::TypeGoldSmelter) = mulhi(d0, DAMP(Building::TypeGoldSmelter));
}

/* 0x2c89e: castle. */
void AI::Want::want_castle(Player *player) {
  unsigned int d0, d1;

  if (IDLE(Serf::TypeGeologist) != 0) {
    if (W(IDLE(Serf::TypeTransporter) + IDLE_GENERIC) < 2) return;
  } else {
    d0 = W((player->total_land_area >> 7) + 3);
    if (d0 < W(player->serf_count[Serf::TypeGeologist])) return;
    if (STOCK(Resource::TypeHammer) == 0) return;
    if (IDLE_GENERIC == 0) return;
    if (W(IDLE_GENERIC + IDLE(Serf::TypeTransporter)) < 3) return;
  }

  d1 = want_scale(LOC(25));
  if (d1 >= 0x3a98) d1 = 0x3a97;
  d1 <<= 2;

  WANT(Building::TypeCastle) = mulhi(d1, DAMP(Building::TypeCastle));
}
