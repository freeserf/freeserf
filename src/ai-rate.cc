/*
 * ai-rate.cc - Computer player: site and attack ratings, area statistics
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

/* Ported from the Amiga original, 0x2b830 and 0x2d316..0x2edd6.

   ai_scan_points_of_interest() collects weighted statistics of the
   hexagonal rings around the AI cursor into player->ai.poi[]. poi[0]
   holds all rings, poi[1..3] snapshots taken after the outer, middle
   and inner ring groups (poi[3] = innermost). The evaluators combine
   these into a 16-bit score by a chain of 16x16 fixed point
   multiplications, starting from 0xffff (the D7 register of the
   original: mulu.w + swap).

   The poi fields are addressed by their Amiga byte offset (POI(p, off))
   so the evaluators can be compared with the disassembly:
     0x00 own land, 0x02 free land, 0x04 enemy land,
     0x06 grass (down triangle 4..7), 0x08 mountain (down 11..14),
     0x0a water vertices,
     0x0a + 2 * t buildings of type t (1..24):
       0c fisher, 0e lumberjack, 10 boatbuilder, 12 stonecutter,
       14 stonemine, 16 coalmine, 18 ironmine, 1a goldmine,
       1c forester, 1e stock, 20 hut, 22 farm, 24 butcher, 26 pigfarm,
       28 mill, 2a baker, 2c sawmill, 2e steelsmelter, 30 toolmaker,
       32 weaponsmith, 34 tower, 36 fortress, 38 goldsmelter, 3a castle,
     0x3c trees, 0x3e meadow (type 0x55), 0x40 stones,
     0x42..0x48 gold/iron/coal/stone signs,
     0x4a roads (or shore state, see poi_check_shore),
     0x4c..0x52 ground deposits gold/iron/coal/stone (castle scan),
     0x54 other objects, 0x56 empty signs. */

#include "src/ai-rate.h"

#include <cstring>

/* The poi struct is 44 words in the original, 44 ints here. */
static_assert(sizeof(AIPoi) == 44 * sizeof(int), "AIPoi layout");

#define POI(p, off)  (reinterpret_cast<int *>(p)[(off) >> 1])
/* Read a poi field as unsigned 16-bit word. */
#define W(p, off)  (static_cast<unsigned int>(POI(p, off)) & 0xffff)

/* Value clamped like "cmpi.w #n; bcs; move.w #n-1". */
unsigned int
AI::Rate::clampw(unsigned int v, unsigned int n) {
  v &= 0xffff;
  return (v < n) ? v : n - 1;
}

/* Decreasing factor: lsl.w #s; not.w */
#define FN(v, s)  ((~(static_cast<unsigned int>(v) << (s))) & 0xffff)
/* Increasing factor: lsl.w #s; addi.w #a */
#define FA(v, s, a) \
  (((static_cast<unsigned int>(v) << (s)) + static_cast<unsigned int>(a)) \
  & 0xffff)

/* mulu.w D6w,D7; swap D7 */
uint32_t
AI::Rate::mul(uint32_t d7, unsigned int d6) {
  uint32_t r = (d7 & 0xffff) * (d6 & 0xffff);
  return (r >> 16) | (r << 16);
}

/* ((p3 * 2 + p2) * 2 + p1) * 2 + p0 of one field. */
unsigned int
AI::Rate::wsum(AIPoi *const p[4], int off) {
  unsigned int s = W(p[3], off);
  s = s + s + W(p[2], off);
  s = s + s + W(p[1], off);
  s = s + s + W(p[0], off);
  return s & 0xffff;
}

#define POIS(player) \
  AIPoi *a0 = &(player)->ai.poi[0]; \
  AIPoi *a1 = &(player)->ai.poi[1]; \
  AIPoi *a2 = &(player)->ai.poi[2]; \
  AIPoi *a3 = &(player)->ai.poi[3]; \
  AIPoi *const ap[4] = { a0, a1, a2, a3 }; \
  (void)a0; (void)a1; (void)a2; (void)a3; (void)ap


/* ---- Attack target evaluators ---- */

/* attack_score_base @0x2d316 */
uint32_t
AI::Rate::attack_score_base(Player *player) {
  POIS(player);
  uint32_t d7 = 0xffffffff;
  unsigned int d6;

  /* Military buildings (fortress + castle, tower, hut). */
  d6 = (((W(a2, 0x36) + W(a2, 0x3a)) * 2 + W(a2, 0x34)) * 2 + W(a2, 0x20));
  d7 = mul(d7, FN(clampw(d6, 0xc8), 8));
  d6 = (((W(a1, 0x36) + W(a1, 0x3a)) * 2 + W(a1, 0x34)) * 2 + W(a1, 0x20));
  d7 = mul(d7, FN(clampw(d6, 0x15e), 7));
  d6 = (((W(a0, 0x36) + W(a0, 0x3a)) * 2 + W(a0, 0x34)) * 2 + W(a0, 0x20));
  d7 = mul(d7, FN(clampw(d6, 0x258), 6));
  /* Stocks. */
  d6 = wsum(ap, 0x1e);
  d7 = mul(d7, FA(clampw(d6, 0x8c), 7, -0x4601));
  /* Land of the target's owner. */
  d7 = mul(d7, FA(clampw(W(a0, 0x00), 0x384), 6, 0x1eff));
  return d7;
}

/* rate_attack_steel_weapons @0x2d44c */
uint32_t
AI::Rate::rate_attack_steel_weapons(Player *player) {
  POIS(player);
  uint32_t d7 = attack_score_base(player);
  unsigned int d3 = wsum(ap, 0x16) + wsum(ap, 0x18) + wsum(ap, 0x2e) +
    wsum(ap, 0x32);
  d7 = mul(d7, FA(clampw(d3, 0xc8), 8, 0x37ff));
  return d7 & 0xffff;
}

/* rate_attack_tools @0x2d4ca */
uint32_t
AI::Rate::rate_attack_tools(Player *player) {
  POIS(player);
  uint32_t d7 = attack_score_base(player);
  unsigned int d3 = wsum(ap, 0x16) + wsum(ap, 0x18) + wsum(ap, 0x2e) +
    wsum(ap, 0x30) + wsum(ap, 0x0e) + wsum(ap, 0x1c) +
    wsum(ap, 0x2c);
  d7 = mul(d7, FA(clampw(d3, 0x190), 7, 0x37ff));
  return d7 & 0xffff;
}

/* rate_attack_gold @0x2d590 */
uint32_t
AI::Rate::rate_attack_gold(Player *player) {
  POIS(player);
  uint32_t d7 = attack_score_base(player);
  unsigned int d3 = wsum(ap, 0x16) + wsum(ap, 0x1a) + wsum(ap, 0x38);
  d7 = mul(d7, FA(clampw(d3, 0xc8), 8, 0x37ff));
  return d7 & 0xffff;
}

/* rate_attack_food @0x2d5f6 */
uint32_t
AI::Rate::rate_attack_food(Player *player) {
  POIS(player);
  uint32_t d7 = attack_score_base(player);
  unsigned int d3 = wsum(ap, 0x0c) + wsum(ap, 0x22) + wsum(ap, 0x24) +
    wsum(ap, 0x26) + wsum(ap, 0x28) + wsum(ap, 0x2a);
  d7 = mul(d7, FA(clampw(d3, 0x190), 7, 0x37ff));
  return d7 & 0xffff;
}

/* rate_attack_wood_stone @0x2d6a4 */
uint32_t
AI::Rate::rate_attack_wood_stone(Player *player) {
  POIS(player);
  uint32_t d7 = attack_score_base(player);
  unsigned int d3 = wsum(ap, 0x0e) + wsum(ap, 0x12) + wsum(ap, 0x1c) +
    wsum(ap, 0x14);
  d7 = mul(d7, FA(clampw(d3, 0xc8), 8, 0x37ff));
  return d7 & 0xffff;
}

/* rate_attack_gold_deposit @0x2d722 */
uint32_t
AI::Rate::rate_attack_gold_deposit(Player *player) {
  POIS(player);
  uint32_t d7 = attack_score_base(player);
  unsigned int d6 = wsum(ap, 0x42) + wsum(ap, 0x1a);
  d7 = mul(d7, FA(clampw(d6, 0x7d), 9, 0x5ff));
  return d7 & 0xffff;
}

/* rate_attack_iron_deposit @0x2d770 */
uint32_t
AI::Rate::rate_attack_iron_deposit(Player *player) {
  POIS(player);
  uint32_t d7 = attack_score_base(player);
  unsigned int d6 = wsum(ap, 0x44) + wsum(ap, 0x18);
  d7 = mul(d7, FA(clampw(d6, 0xfa), 8, 0x5ff));
  return d7 & 0xffff;
}

/* rate_attack_coal_deposit @0x2d7bc */
uint32_t
AI::Rate::rate_attack_coal_deposit(Player *player) {
  POIS(player);
  uint32_t d7 = attack_score_base(player);
  unsigned int d6 = wsum(ap, 0x46) + wsum(ap, 0x16);
  d7 = mul(d7, FA(clampw(d6, 0x1f4), 7, 0x5ff));
  return d7 & 0xffff;
}

/* rate_attack_stone_deposit @0x2d808 */
uint32_t
AI::Rate::rate_attack_stone_deposit(Player *player) {
  POIS(player);
  uint32_t d7 = attack_score_base(player);
  unsigned int d6 = wsum(ap, 0x48) + wsum(ap, 0x14) + W(a2, 0x40);
  d7 = mul(d7, FA(clampw(d6, 0xfa), 8, 0x5ff));
  return d7 & 0xffff;
}


/* ---- Location store ---- */

/* store_location @0x2da3e: remember the cursor with the given value
   in category's list of 8 locations. Updates the entry at the same
   position, else replaces the lowest entry if value is not lower. */
void
AI::Rate::store_location(Player *player, int category, uint32_t value) {
  AILocation *loc = player->ai.locations[category];
  int col = player->ai.cursor_col;
  int row = player->ai.cursor_row;
  AILocation *min_loc = nullptr;
  unsigned int min = 0xffff;

  value &= 0xffff;
  for (int i = 0; i < AI_LOCATIONS_PER_CATEGORY; i++) {
    unsigned int v = static_cast<unsigned int>(loc[i].value) & 0xffff;
    if (!(min < v)) {
      min = v;
      min_loc = &loc[i];
    }
    if (loc[i].col == col && loc[i].row == row) {
      loc[i].value = value;
      return;
    }
  }

  if (value >= (static_cast<unsigned int>(min_loc->value) & 0xffff)) {
    min_loc->value = value;
    min_loc->col = col;
    min_loc->row = row;
  }
}


/* ---- Common site factors ---- */

/* Military buildings near: ((((3.fortress * 2 + 3.tower + 2.fortress) * 2
   + 3.hut + 2.tower) * 2 + 2.hut. */
unsigned int
AI::Rate::military_near(AIPoi *a2, AIPoi *a3) {
  unsigned int d6 = W(a3, 0x36);
  d6 = d6 + d6 + W(a3, 0x34) + W(a2, 0x36);
  d6 = d6 + d6 + W(a3, 0x20) + W(a2, 0x34);
  d6 = d6 + d6 + W(a2, 0x20);
  return d6 & 0xffff;
}

/* Economic buildings in poi a1 (tower/fortress rating). */
unsigned int
AI::Rate::economy_near(AIPoi *a1) {
  unsigned int d6 = W(a1, 0x1e);
  d6 = d6 + d6 + W(a1, 0x3a) + W(a1, 0x32) + W(a1, 0x30) +
    W(a1, 0x16) + W(a1, 0x18) + W(a1, 0x1a);
  d6 = d6 + d6 + W(a1, 0x22) + W(a1, 0x24) + W(a1, 0x26) +
    W(a1, 0x2a) + W(a1, 0x2c) + W(a1, 0x2e) + W(a1, 0x38) +
    W(a1, 0x14);
  d6 = d6 + d6 + W(a1, 0x0c) + W(a1, 0x0e) + W(a1, 0x10) +
    W(a1, 0x12) + W(a1, 0x1c) + W(a1, 0x28);
  return d6 & 0xffff;
}

/* site_score_base_b @0x2da84 */
uint32_t
AI::Rate::site_score_base_b(Player *player, uint32_t d7) {
  POIS(player);
  unsigned int d6;

  /* The original also shifts D5 here (lsl.w #2,D5w @0x2da88), a
     register that none of the callers uses afterwards. */
  d6 = (W(a3, 0x04) + W(a2, 0x04)) * 4 + W(a2, 0x02);
  d7 = mul(d7, FN(clampw(d6, 0xfa0), 4));
  d6 = W(a0, 0x04) * 4 + W(a0, 0x02);
  d7 = mul(d7, FN(clampw(d6, 0x4e20), 1));
  d7 = mul(d7, FA(clampw(military_near(a2, a3), 0x6e), 8, -0x6e01));
  d7 = mul(d7, FN(clampw(W(a3, 0x22), 0x10), 12));
  d7 = mul(d7, FN(clampw(W(a2, 0x1c), 0x20), 11));
  d6 = W(a1, 0x1e) + W(a1, 0x3a);
  d7 = mul(d7, FA(clampw(d6, 0x14), 10, -0x5001));
  return d7;
}

/* site_score_base_a @0x2db44 */
uint32_t
AI::Rate::site_score_base_a(Player *player, uint32_t d7) {
  POIS(player);
  unsigned int d6;

  d7 = mul(d7, FN(clampw(W(a1, 0x02), 0x800), 4));
  d7 = mul(d7, FN(clampw(W(a2, 0x04), 0x384), 6));
  d7 = mul(d7, FN(clampw(W(a1, 0x04), 0x400), 5));
  d6 = W(a3, 0x22) * 8 + W(a2, 0x22);
  d7 = mul(d7, FN(clampw(d6, 0x80), 9));
  d7 = mul(d7, FN(clampw(W(a2, 0x1c), 0x20), 11));
  return d7;
}

/* site_score_base_mine @0x2dbbe */
uint32_t
AI::Rate::site_score_base_mine(Player *player, uint32_t d7) {
  POIS(player);
  unsigned int d6;

  d7 = mul(d7, FN(clampw(W(a1, 0x02), 0x1000), 2));
  /* The original computes poi[2] enemy land * 4 here and overwrites
     it unused (@0x2dbd4). */
  d7 = mul(d7, FN(clampw(W(a1, 0x04), 0x1fff), 3));
  d6 = W(a1, 0x1e) + W(a1, 0x3a);
  d7 = mul(d7, FA(clampw(d6, 0x14), 10, -0x5001));
  return d7;
}


/* ---- Building site evaluators ----
   Each returns the D7 register of the original. The caller stores the
   low word if the returned value is non-zero (most evaluators end with
   tst.w D7w and return only the low word; the mine evaluators end with
   swap D7, so the whole register is tested). */

/* rate_site_none @0x2dc12: flag site (uses poi[0] only). */
uint32_t
AI::Rate::rate_site_none(Player *player) {
  POIS(player);
  unsigned int d6 = (W(a0, 0x02) + W(a0, 0x04)) & 0xffff;
  if (d6 != 0) return 0;

  d6 = W(a0, 0x4a);
  if (d6 == 0) return 0x9c40;
  if (d6 != 0xffff) return 0;
  if (W(a0, 0x0a) < 0x0c) return 0;
  /* (0x368,A4) = poi[4] sawmill count, (0x3ac,A4) = u_3ac. */
  if (W(&player->ai.poi[4], 0x2c) == 0 &&
      (player->ai.u_3ac & 0xffff) == 0) {
    return 0;
  }
  return 0x88b8;
}

/* rate_site_fisher @0x2dc4c */
uint32_t
AI::Rate::rate_site_fisher(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_a(player, 0xffffffff);
  unsigned int d6 = W(a3, 0x0a);
  if (d6 < 0x50) return 0;
  d6 -= 0x50;
  d7 = mul(d7, FA(clampw(d6, 0x100), 8, 0));
  d7 = mul(d7, FA(clampw(W(a2, 0x0a), 0x200), 6, 0x7fff));
  d7 = mul(d7, FA(clampw(W(a1, 0x0a), 0x400), 4, -0x4001));
  d7 = mul(d7, FN(clampw(W(a2, 0x0c), 0x10), 12));
  return d7 & 0xffff;
}

/* rate_site_lumberjack @0x2dcbc */
uint32_t
AI::Rate::rate_site_lumberjack(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_a(player, 0xffffffff);
  unsigned int d6;

  d6 = W(a1, 0x1e) + W(a1, 0x3a);
  d7 = mul(d7, FA(clampw(d6, 0x14), 8, -0x1401));
  d6 = (W(a3, 0x1c) * 8 + W(a2, 0x1c)) * 2 + W(a1, 0x1c);
  d7 = mul(d7, FA(clampw(d6, 0x100), 6, -0x4001));
  d6 = (W(a3, 0x0e) * 8 + W(a2, 0x0e)) * 2 + W(a1, 0x0e);
  d7 = mul(d7, FN(clampw(d6, 0x100), 6));
  d7 = mul(d7, FA(clampw(W(a1, 0x2c), 0x14), 10, -0x5001));
  d7 = mul(d7, FN(clampw(W(a2, 0x4a), 0x7d0), 5));
  d7 = mul(d7, FA(clampw(W(a2, 0x3c), 0x800), 5, 0));
  return d7 & 0xffff;
}

/* rate_site_boatbuilder @0x2dd6c */
uint32_t
AI::Rate::rate_site_boatbuilder(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_a(player, 0xffffffff);
  unsigned int d6 = W(a1, 0x1e) + W(a1, 0x3a);
  d7 = mul(d7, FA(clampw(d6, 0x14), 11, 0x5fff));
  d7 = mul(d7, FA(clampw(W(a1, 0x2c), 0x14), 11, 0x5fff));
  return d7 & 0xffff;
}

/* rate_site_stonecutter @0x2ddae */
uint32_t
AI::Rate::rate_site_stonecutter(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_a(player, 0xffffffff);
  unsigned int d6 = W(a1, 0x1e) + W(a1, 0x3a);
  d7 = mul(d7, FA(clampw(d6, 0x14), 8, -0x1401));
  d7 = mul(d7, FA(clampw(W(a2, 0x40), 0x100), 8, 0));
  if (W(a3, 0x12) != 0) d7 = 0;
  d7 = mul(d7, FN(clampw(W(a2, 0x12), 0x20), 12));
  return d7 & 0xffff;
}

/* Mines @0x2de08..0x2dede: signs in poi[3] (sign_off), mines of the
   same type in poi[2] (mine_off). */
uint32_t
AI::Rate::rate_site_mine(Player *player, int sign_off, int mine_off) {
  POIS(player);
  uint32_t d7 = site_score_base_mine(player, 0xffffffff);
  d7 = mul(d7, FA(clampw(W(a3, sign_off), 0x10), 12, 0));
  d7 = mul(d7, FN(clampw(W(a2, mine_off), 0x1f), 11));
  return d7;
}

/* rate_site_stonemine @0x2de08 */
uint32_t
AI::Rate::rate_site_stonemine(Player *player) {
  return rate_site_mine(player, 0x48, 0x14);
}

/* rate_site_coalmine @0x2de3e */
uint32_t
AI::Rate::rate_site_coalmine(Player *player) {
  return rate_site_mine(player, 0x46, 0x16);
}

/* rate_site_ironmine @0x2de74 */
uint32_t
AI::Rate::rate_site_ironmine(Player *player) {
  return rate_site_mine(player, 0x44, 0x18);
}

/* rate_site_goldmine @0x2deaa */
uint32_t
AI::Rate::rate_site_goldmine(Player *player) {
  return rate_site_mine(player, 0x42, 0x1a);
}

/* rate_site_forester @0x2dee0 */
uint32_t
AI::Rate::rate_site_forester(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_b(player, 0xffffffff);
  unsigned int d6;

  d6 = W(a2, 0x1c) * 16 + W(a1, 0x1c);
  d7 = mul(d7, FN(clampw(d6, 0x100), 8));
  d6 = (W(a3, 0x0e) * 8 + W(a2, 0x0e)) * 2 + W(a1, 0x0e);
  d7 = mul(d7, FA(clampw(d6, 0x100), 7, 0x7fff));
  d7 = mul(d7, FA(clampw(W(a3, 0x0e), 0x0f), 12, 0xfff));
  d7 = mul(d7, FA(clampw(W(a1, 0x2c), 0x14), 9, -0x2801));
  d7 = mul(d7, FN(clampw(W(a2, 0x4a), 0x400), 6));
  d7 = mul(d7, FN(clampw(W(a2, 0x3c), 0x800), 3));
  return d7 & 0xffff;
}

/* rate_site_stock @0x2df8a */
uint32_t
AI::Rate::rate_site_stock(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_b(player, 0xffffffff);
  unsigned int d6;

  d6 = W(a2, 0x1e) * 2 + W(a1, 0x1e) + W(a1, 0x3a);
  d7 = mul(d7, FN(clampw(d6, 0x1e), 11));
  d6 = (W(a2, 0x04) * 2 + W(a2, 0x02)) & 0xffff;
  if (d6 != 0) d7 = 0;
  d6 = W(a1, 0x04) * 2 + W(a1, 0x02);
  d7 = mul(d7, FN(clampw(d6, 0x1ff), 7));
  d6 = W(a0, 0x04) * 2 + W(a0, 0x02);
  d7 = mul(d7, FN(clampw(d6, 0x1000), 4));
  return d7 & 0xffff;
}

/* rate_site_hut @0x2dffc */
uint32_t
AI::Rate::rate_site_hut(Player *player) {
  POIS(player);
  uint32_t d7 = 0xffffffff;
  unsigned int d6;

  d6 = W(a3, 0x02) + W(a3, 0x04);
  d7 = mul(d7, FA(clampw(d6, 0x7f), 9, 0x1ff));
  d6 = W(a2, 0x02) + W(a2, 0x04);
  d7 = mul(d7, FA(clampw(d6, 0x1f4), 7, 0x5ff));
  d7 = mul(d7, FA(clampw(W(a3, 0x02), 0x64), 8, -0x6401));
  d7 = mul(d7, FA(clampw(W(a2, 0x02), 0x1f4), 6, -0x7d01));
  if (W(a3, 0x20) != 0) d7 = 0;
  d7 = mul(d7, FN(clampw(W(a2, 0x20), 0x50), 8));
  return d7 & 0xffff;
}

/* rate_site_farm @0x2e08a */
uint32_t
AI::Rate::rate_site_farm(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_b(player, 0xffffffff);
  unsigned int d6;

  d7 = mul(d7, FN(clampw(W(a3, 0x4a), 0x1f4), 7));
  d7 = mul(d7, FA(clampw(W(a3, 0x3e), 0x1f4), 7, 0x5ff));
  d7 = mul(d7, FN(clampw(W(a2, 0x22), 0x1e), 11));
  d7 = mul(d7, FA(clampw(W(a1, 0x28), 0x28), 9, -0x5001));
  d7 = mul(d7, FA(clampw(W(a1, 0x26), 0x28), 8, -0x2801));
  d6 = W(a1, 0x24) + W(a1, 0x2a);
  d7 = mul(d7, FA(clampw(d6, 0x28), 6, -0xa01));
  return d7 & 0xffff;
}

/* rate_site_butcher @0x2e128 */
uint32_t
AI::Rate::rate_site_butcher(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_b(player, 0xffffffff);
  d7 = mul(d7, FA(clampw(W(a1, 0x26), 0x14), 11, 0x5fff));
  d7 = mul(d7, FA(clampw(W(a1, 0x22), 0x14), 9, -0x2801));
  d7 = mul(d7, FN(clampw(W(a1, 0x24), 0x14), 11));
  return d7 & 0xffff;
}

/* rate_site_pigfarm @0x2e17e */
uint32_t
AI::Rate::rate_site_pigfarm(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_b(player, 0xffffffff);
  d7 = mul(d7, FA(clampw(W(a1, 0x22), 0x14), 11, 0x5fff));
  d7 = mul(d7, FA(clampw(W(a1, 0x24), 0x14), 10, -0x5001));
  return d7 & 0xffff;
}

/* rate_site_mill @0x2e1bc */
uint32_t
AI::Rate::rate_site_mill(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_a(player, 0xffffffff);
  unsigned int d6 = W(a1, 0x1e) + W(a1, 0x3a);
  d7 = mul(d7, FA(clampw(d6, 0x1e), 10, -0x7801));
  d7 = mul(d7, FA(clampw(W(a1, 0x22), 0x14), 11, 0x5fff));
  d7 = mul(d7, FA(clampw(W(a1, 0x2a), 0x14), 10, -0x5001));
  d7 = mul(d7, FN(clampw(W(a1, 0x28), 0x14), 11));
  return d7 & 0xffff;
}

/* rate_site_baker @0x2e230 */
uint32_t
AI::Rate::rate_site_baker(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_b(player, 0xffffffff);
  d7 = mul(d7, FA(clampw(W(a1, 0x28), 0x14), 11, 0x5fff));
  d7 = mul(d7, FA(clampw(W(a1, 0x22), 0x14), 9, -0x2801));
  d7 = mul(d7, FN(clampw(W(a1, 0x2a), 0x14), 11));
  return d7 & 0xffff;
}

/* rate_site_sawmill @0x2e286 */
uint32_t
AI::Rate::rate_site_sawmill(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_b(player, 0xffffffff);
  d7 = mul(d7, FA(clampw(W(a1, 0x0e), 0x14), 10, -0x5001));
  d7 = mul(d7, FA(clampw(W(a1, 0x1c), 0x14), 9, -0x2801));
  d7 = mul(d7, FN(clampw(W(a1, 0x2c), 0x14), 11));
  return d7 & 0xffff;
}

/* rate_site_steelsmelter @0x2e2dc */
uint32_t
AI::Rate::rate_site_steelsmelter(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_b(player, 0xffffffff);
  d7 = mul(d7, FA(clampw(W(a1, 0x18), 0x1a), 11, 0x2fff));
  d7 = mul(d7, FA(clampw(W(a1, 0x16), 0x1a), 11, 0x2fff));
  d7 = mul(d7, FN(clampw(W(a2, 0x2e), 0x14), 11));
  d7 = mul(d7, FA(clampw(W(a1, 0x30), 0x14), 10, -0x5001));
  d7 = mul(d7, FA(clampw(W(a1, 0x32), 0x14), 10, -0x5001));
  return d7 & 0xffff;
}

/* rate_site_toolmaker @0x2e366 */
uint32_t
AI::Rate::rate_site_toolmaker(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_b(player, 0xffffffff);
  d7 = mul(d7, FA(clampw(W(a1, 0x2c), 0x12), 11, 0x6fff));
  d7 = mul(d7, FA(clampw(W(a1, 0x2e), 0x12), 11, 0x6fff));
  d7 = mul(d7, FN(clampw(W(a2, 0x30), 0x14), 11));
  return d7 & 0xffff;
}

/* rate_site_weaponsmith @0x2e3bc */
uint32_t
AI::Rate::rate_site_weaponsmith(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_b(player, 0xffffffff);
  d7 = mul(d7, FA(clampw(W(a1, 0x16), 0x1a), 11, 0x2fff));
  d7 = mul(d7, FA(clampw(W(a1, 0x2e), 0x16), 11, 0x4fff));
  d7 = mul(d7, FN(clampw(W(a2, 0x32), 0x14), 11));
  return d7 & 0xffff;
}

/* rate_site_tower @0x2e412 */
uint32_t
AI::Rate::rate_site_tower(Player *player) {
  POIS(player);
  uint32_t d7 = 0xffffffff;
  unsigned int d6;

  d6 = W(a2, 0x04) * 4 + W(a2, 0x02);
  d7 = mul(d7, FN(clampw(d6, 0x3b6), 6));
  d6 = W(a0, 0x04) * 4 + W(a0, 0x02);
  d7 = mul(d7, FN(clampw(d6, 0xfa0), 3));
  d7 = mul(d7, FA(clampw(military_near(a2, a3), 0x6e), 9, 0x23ff));
  d7 = mul(d7, FA(clampw(economy_near(a1), 0x200), 7, 0));
  return d7 & 0xffff;
}

/* rate_site_fortress @0x2e4ee */
uint32_t
AI::Rate::rate_site_fortress(Player *player) {
  POIS(player);
  uint32_t d7 = 0xffffffff;
  unsigned int d6;

  d6 = W(a2, 0x04) * 4 + W(a2, 0x02);
  d7 = mul(d7, FN(clampw(d6, 0x1f4), 7));
  d6 = W(a0, 0x04) * 4 + W(a0, 0x02);
  d7 = mul(d7, FN(clampw(d6, 0xbb8), 4));
  d7 = mul(d7, FA(clampw(military_near(a2, a3), 0x32), 10, 0x37ff));
  d7 = mul(d7, FA(clampw(economy_near(a1), 0x200), 7, 0));
  return d7 & 0xffff;
}

/* rate_site_goldsmelter @0x2e5ca */
uint32_t
AI::Rate::rate_site_goldsmelter(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_b(player, 0xffffffff);
  d7 = mul(d7, FA(clampw(W(a1, 0x1a), 0x1a), 11, 0x2fff));
  d7 = mul(d7, FA(clampw(W(a1, 0x16), 0x1a), 11, 0x2fff));
  d7 = mul(d7, FN(clampw(W(a2, 0x38), 0x14), 11));
  return d7 & 0xffff;
}

/* rate_site_geologist @0x2e620 */
uint32_t
AI::Rate::rate_site_geologist(Player *player) {
  POIS(player);
  uint32_t d7 = site_score_base_mine(player, 0xffffffff);
  unsigned int d6;

  d6 = W(a3, 0x14) + W(a3, 0x16) + W(a3, 0x18) + W(a3, 0x1a);
  d7 = mul(d7, FN(clampw(d6, 0x40), 9));
  d6 = W(a3, 0x42) + W(a3, 0x44) + W(a3, 0x46) + W(a3, 0x48) +
    W(a3, 0x56);
  d6 = d6 + d6 + W(a2, 0x42) + W(a2, 0x44) + W(a2, 0x46) +
    W(a2, 0x48) + W(a2, 0x56);
  d7 = mul(d7, FN(clampw(d6, 0xff), 8));
  return d7 & 0xffff;
}

/* rate_site_castle @0x2e68a */
uint32_t
AI::Rate::rate_site_castle_d7(Player *player) {
  POIS(player);
  uint32_t d7 = 0xffffffff;
  unsigned int d6;

  d7 = mul(d7, FN(clampw(W(a0, 0x04), 0x3b6), 6));
  d7 = mul(d7, FN(clampw(W(a2, 0x04), 0xfc), 8));
  d6 = W(a0, 0x06) + W(a0, 0x08);
  d7 = mul(d7, FA(clampw(d6, 0x1b58), 2, -0x6d61));
  d7 = mul(d7, FA(clampw(W(a0, 0x0a), 0xa0), 7, -0x5001));
  d7 = mul(d7, FA(clampw(W(a1, 0x3c), 0x800), 5, 0));
  d7 = mul(d7, FA(clampw(W(a0, 0x3c), 0x800), 4, 0x7fff));
  d7 = mul(d7, FA(clampw(W(a2, 0x3e), 0x800), 5, 0));
  d7 = mul(d7, FA(clampw(W(a0, 0x3e), 0x1770), 2, -0x5dc1));
  d6 = (W(a1, 0x52) >> 1) + W(a1, 0x40);
  d7 = mul(d7, FA(clampw(d6, 0xff), 8, 0xff));
  d6 = (W(a0, 0x52) >> 1) + W(a0, 0x40);
  d7 = mul(d7, FA(clampw(d6, 0x400), 4, -0x4001));
  d7 = mul(d7, FA(clampw(W(a1, 0x40), 0x40), 10, 0));
  d7 = mul(d7, FA(clampw(W(a0, 0x4c), 0xfa), 8, 0x5ff));
  d7 = mul(d7, FA(clampw(W(a0, 0x4e), 0xfa), 8, 0x5ff));
  d7 = mul(d7, FA(clampw(W(a0, 0x50), 0x1f4), 7, 0x5ff));
  d7 = mul(d7, FA(clampw(W(a0, 0x52), 0x64), 7, -0x3201));

  /* (0x162,A4) byte = initial supplies. */
  unsigned int supplies =
    static_cast<unsigned int>(player->initial_supplies) & 0xff;
  if (supplies < 0x0f) {
    if (supplies >= 0x08) {
      d7 = mul(d7, FA(clampw(W(a1, 0x4e), 0x7f), 8, -0x7f01));
      d7 = mul(d7, FA(clampw(W(a1, 0x50), 0xff), 7, -0x7f81));
    } else {
      d7 = mul(d7, FA(clampw(W(a2, 0x4e), 0x3f), 9, -0x7e01));
      d7 = mul(d7, FA(clampw(W(a2, 0x50), 0x7f), 8, -0x7f01));
      d6 = W(a2, 0x0a) * 4 + W(a1, 0x0a);
      d7 = mul(d7, FA(clampw(d6, 0xff), 8, 0xff));
    }
  }
  return d7 & 0xffff;
}

/* rate_site_castle @0x2e68a, for ai_place_castle @0x2d09e. Rates
   the cursor site from player->ai.poi[] (scan with
   ai_scan_points_of_interest first) and returns D7w. */
int
AI::Rate::rate_site_castle(Player *player) {
  return static_cast<int>(rate_site_castle_d7(player) & 0xffff);
}


/* ---- Dispatch ---- */

/* Evaluators by location category (the index of ai.locations[] and
   of the bits of rate_building_site's mask). */
AI::Rate::RateFn *const AI::Rate::rate_by_category[category_count] = {
  rate_site_none,    /* 0 flag */
  rate_site_fisher,
  rate_site_lumberjack,
  rate_site_boatbuilder,
  rate_site_stonecutter,
  rate_site_stonemine,    /* 5 */
  rate_site_coalmine,
  rate_site_ironmine,
  rate_site_goldmine,
  rate_site_forester,
  rate_site_stock,    /* 10 */
  rate_site_hut,
  rate_site_farm,
  rate_site_butcher,
  rate_site_pigfarm,
  rate_site_mill,    /* 15 */
  rate_site_baker,
  rate_site_sawmill,
  rate_site_steelsmelter,
  rate_site_toolmaker,
  rate_site_weaponsmith,  /* 20 */
  rate_site_tower,
  rate_site_fortress,
  rate_site_goldsmelter,
  rate_site_castle_d7,
  rate_site_geologist,    /* 25 */
  rate_attack_steel_weapons,
  rate_attack_tools,
  rate_attack_gold,
  rate_attack_food,
  rate_attack_wood_stone,  /* 30 */
  rate_attack_gold_deposit,
  rate_attack_iron_deposit,
  rate_attack_coal_deposit,
  rate_attack_stone_deposit  /* 34 */
};

/* rate_site_dispatch @0x2b830: rating of the cursor site (after
   ai_scan_points_of_interest) for category ai_game.build_building_type.
   The callers compare the result (D7w) unsigned with the values of
   ai.locations[build_building_type]. */
int
AI::Rate::rate_site_dispatch(Player *player) {
  int type = ai_game.build_building_type;

  /* Original bug: the jump table has 34 entries without the castle
     (24 = geologist, 25..33 = the attack evaluators), while the
     stores (rate_building_site, rate_attack_target) and the
     callers' location lists use 24 = castle, 25 = geologist,
     26..34 = attack. So 24..33 rated with the evaluator of the
     neighbouring category and 34 (set by ai_attack @0x2a016) jumped
     past the table into ai_want_type25. Dispatched here by the
     store numbering instead. */
  if (type < 0 || type >= category_count) {
    return 0;
  }
  return static_cast<int>(rate_by_category[type](player) & 0xffff);
}


/* ---- Rating of the cursor site ---- */

/* rate_attack_target @0x2d3de: rates the enemy building at the
   cursor for the attack categories 26..34. */
void
AI::Rate::rate_attack_target(Player *player) {
  for (int cat = 26; cat <= 34; cat++) {
    uint32_t d7 = rate_by_category[cat](player);
    if (d7 & 0xffff) store_location(player, cat, d7);
  }
}

/* rate_building_site @0x2d858: rates the cursor site for every
   category whose bit is set in categories (bit 0 flag, bits 1..23
   building types, 24 castle, 25 geologist). */
void
AI::Rate::rate_building_site(Player *player, uint32_t categories) {
  for (int cat = 0; cat <= 25; cat++) {
    if (!((categories >> cat) & 1)) continue;
    uint32_t d7 = rate_by_category[cat](player);
    /* beq after the evaluator: tests the whole register for the
       mine evaluators (they end with swap), else the low word. */
    if (d7 != 0) store_location(player, cat, d7);
  }
}


/* ---- Area statistics ---- */

/* Table at 0x2e9d2: poi byte offset counted for each map object.
   0 = nothing (meadow test), 0xff = building (counted by type). */
static const uint8_t poi_obj_field[128] = {
  0x00, 0x54, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00,  /* 0 */
  0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,  /* 8 trees */
  0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,  /* 16 pines */
  0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54,  /* 24 palms, water trees */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 32 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 40 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 48 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 56 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 64 */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* 72 stones */
  0x54, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 80 */
  0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54,  /* 88 */
  0x54, 0x00, 0x54, 0x54, 0x54, 0x54, 0x00, 0x54,  /* 96 */
  0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0x00,  /* 104 */
  0x42, 0x42, 0x44, 0x44, 0x46, 0x46, 0x48, 0x48,  /* 112 signs */
  0x56, 0x54, 0x54, 0x54, 0x54, 0x54, 0x54, 0xff  /* 120 */
};

void
AI::Rate::poi_add(AIPoi *poi, int off, int w) {
  POI(poi, off) = (POI(poi, off) + w) & 0xffff;
}

/* Amiga obj bit 7 (set by map_mark_water_tiles): own up or down
   triangle is water. */
int
AI::Rate::water_vertex(MapPos pos) {
  return MAP_TYPE_UP(pos) < 4 || MAP_TYPE_DOWN(pos) < 4;
}

/* Ownership part shared by all ring counters. */
void
AI::Rate::ring_count_owner(Ring *r, MapPos pos) {
  if (!MAP_HAS_OWNER(pos)) {
    poi_add(r->poi, 0x02, r->weight);
  } else if (static_cast<int>(MAP_OWNER(pos)) == r->owner) {
    poi_add(r->poi, 0x00, r->weight);
  } else {
    poi_add(r->poi, 0x04, r->weight);
  }
}

/* Count a building at pos by its type; done_only skips unfinished. */
void
AI::Rate::ring_count_building(Ring *r, MapPos pos, int done_only) {
  /* Table entry 0xff of obj 127 would read a bogus index. */
  if (MAP_OBJ(pos) < Map::ObjectSmallBuilding ||
      MAP_OBJ(pos) > Map::ObjectCastle) {
    return;
  }
  Building *building = AI_BUILDING(MAP_OBJ_INDEX(pos));
  if (done_only && !BUILDING_IS_DONE(building)) return;
  poi_add(r->poi, 0x0a + 2 * static_cast<int>(BUILDING_TYPE(building)),
          r->weight);
}

/* Tile of poi_count_rings_deposits @0x2ea52 */
void
AI::Rate::ring_tile_deposits(Ring *r, MapPos pos) {
  ring_count_owner(r, pos);

  if (water_vertex(pos)) {
    poi_add(r->poi, 0x0a, r->weight);
  } else {
    int field = poi_obj_field[MAP_OBJ(pos)];
    int deposits = 1;
    if (field == 0) {
      if (MAP_TYPE_UP(pos) == 5 && MAP_TYPE_DOWN(pos) == 5) {
        poi_add(r->poi, 0x3e, r->weight);
      }
    } else if (field & 0x80) {
      deposits = 0;
    } else {
      poi_add(r->poi, field, r->weight);
    }

    /* Original bug: on a flag the original reads the low byte
       of the flag index (field-2 word 0) as the deposit byte;
       its deposit was moved to the neighbours when the flag
       was built. Nothing is counted for flags here. */
    if (MAP_OBJ(pos) == Map::ObjectFlag) deposits = 0;

    if (deposits) {
      int res = MAP_RES_TYPE(pos);
      /* Types above 4 do not occur (0x4a + 2 * 7 would
         reach the next poi in the original). */
      if (res != 0 && res <= 6) {
        poi_add(r->poi, 0x4a + 2 * res, r->weight);
      }
    }
  }

  int down = MAP_TYPE_DOWN(pos);
  if (down >= 4 && down < 8) {
    poi_add(r->poi, 0x06, r->weight);
  } else if (down >= 11 && down < 15) {
    poi_add(r->poi, 0x08, r->weight);
  }
}

/* Tile of poi_count_rings @0x2eaf4 */
void
AI::Rate::ring_tile_full(Ring *r, MapPos pos) {
  ring_count_owner(r, pos);

  if (water_vertex(pos)) {
    poi_add(r->poi, 0x0a, r->weight);
  } else {
    int field = poi_obj_field[MAP_OBJ(pos)];
    if (field == 0) {
      if (MAP_TYPE_UP(pos) == 5 && MAP_TYPE_DOWN(pos) == 5) {
        poi_add(r->poi, 0x3e, r->weight);
      }
    } else if (field & 0x80) {
      ring_count_building(r, pos, 0);
    } else {
      poi_add(r->poi, field, r->weight);
    }
  }

  if (MAP_PATHS(pos) != 0) poi_add(r->poi, 0x4a, r->weight);
}

/* Tile of poi_count_rings_no_grass @0x2eb86 */
void
AI::Rate::ring_tile_no_grass(Ring *r, MapPos pos) {
  ring_count_owner(r, pos);

  if (water_vertex(pos)) {
    poi_add(r->poi, 0x0a, r->weight);
  } else {
    int field = poi_obj_field[MAP_OBJ(pos)];
    if (field == 0) {
      /* nothing */
    } else if (field & 0x80) {
      ring_count_building(r, pos, 0);
    } else {
      poi_add(r->poi, field, r->weight);
    }
  }

  if (MAP_PATHS(pos) != 0) poi_add(r->poi, 0x4a, r->weight);
}

/* Tile of poi_count_rings_owner @0x2ec0a */
void
AI::Rate::ring_tile_owner(Ring *r, MapPos pos) {
  ring_count_owner(r, pos);
}

/* Tile of poi_count_rings_objects @0x2ec48 */
void
AI::Rate::ring_tile_objects(Ring *r, MapPos pos) {
  ring_count_owner(r, pos);

  if (water_vertex(pos)) {
    poi_add(r->poi, 0x0a, r->weight);
  } else {
    int field = poi_obj_field[MAP_OBJ(pos)];
    if (field == 0) {
      /* nothing */
    } else if (field & 0x80) {
      ring_count_building(r, pos, 1);
    } else {
      poi_add(r->poi, field, r->weight);
    }
  }
}

/* Tile of the ring part of poi_check_shore @0x2ed38 */
void
AI::Rate::ring_tile_shore(Ring *r, MapPos pos) {
  if (water_vertex(pos)) poi_add(r->poi, 0x0a, r->weight);
  ring_count_owner(r, pos);
  if (MAP_PATHS(pos) != 0) poi_add(r->poi, 0x4a, r->weight);
}

/* Ring walk of 0x2ea52..0x2ed88: each ring starts down-right of the
   previous start and walks six sides (up, up-left, left, down,
   down-right, right) of side + 1 vertices. The weight drops by one per
   ring. The centre itself is never counted. */
void
AI::Rate::poi_count_rings_generic(Ring *r, int rings, RingTileFn *fn) {
  static const Direction sides[6] = {
    DirectionUp, DirectionUpLeft, DirectionLeft,
    DirectionDown, DirectionDownRight, DirectionRight
  };

  for (int i = 0; i < rings; i++) {
    r->pos = MAP_MOVE_DOWN_RIGHT(r->pos);
    for (int s = 0; s < 6; s++) {
      for (int j = 0; j <= r->side; j++) {
        r->pos = MAP_MOVE(r->pos, sides[s]);
        fn(r, r->pos);
      }
    }
    r->side += 1;
    r->weight = (r->weight - 1) & 0xffff;
  }
}

/* poi_check_shore @0x2ecc2: shore state of a flag site in field
   0x4a (100 = not at a shore, then no rings are counted; 0xffff =
   shore), then counts the rings. */
void
AI::Rate::poi_check_shore(Ring *r, int rings) {
  MapPos pos = r->pos;
  int down = MAP_TYPE_DOWN(pos);

  if (down < 8 || down >= 11) {
    MapPos p = MAP_MOVE_UP_LEFT(pos);
    int shore;
    if (MAP_TYPE_UP(pos) < 4) {
      if (MAP_TYPE_UP(p) >= 4 || MAP_TYPE_DOWN(p) >= 4) {
        shore = 1;
      } else {
        p = MAP_MOVE_RIGHT(p);
        shore = (MAP_TYPE_UP(p) >= 4);
      }
    } else {
      if (MAP_TYPE_UP(p) < 4 || MAP_TYPE_DOWN(p) < 4) {
        shore = 1;
      } else {
        p = MAP_MOVE_RIGHT(p);
        shore = (MAP_TYPE_UP(p) < 4);
      }
    }

    if (!shore) {
      POI(r->poi, 0x4a) = 100;
      return;
    }
    POI(r->poi, 0x4a) = 0xffff;
  }

  poi_count_rings_generic(r, rings, ring_tile_shore);
}

/* poi_copy_to_slot1/2/3 @0x2eda0/0x2ed98/0x2ed90 */
void
AI::Rate::poi_copy_to_slot(Player *player, int slot) {
  player->ai.poi[slot] = player->ai.poi[0];
}

/* ai_scan_points_of_interest @0x2e88e */
void
AI::Rate::scan_points_of_interest(Player *player) {
  Ring r;

  std::memset(&player->ai.poi[0], 0, sizeof(player->ai.poi[0]));

  r.pos = AI_CURSOR_POS(player);
  r.side = 0;
  r.weight = 18;
  r.owner = PLAYER_NUM(player);
  r.poi = &player->ai.poi[0];

  /* The original compares the byte with cmpi.b for 3 and 2 and with
     cmpi.w for 4, 1 and 5 (upper byte of D5 is 0 in practice). */
  switch (player->ai.panel_btn_type & 0xff) {
    case AI_CAN_BUILD_SMALL:
      poi_count_rings_generic(&r, 3, ring_tile_full);
      poi_copy_to_slot(player, 3);
      poi_count_rings_generic(&r, 4, ring_tile_full);
      poi_copy_to_slot(player, 2);
      poi_count_rings_generic(&r, 5, ring_tile_no_grass);
      poi_copy_to_slot(player, 1);
      break;
    case AI_CAN_BUILD_MINE:
      poi_count_rings_generic(&r, 1, ring_tile_no_grass);
      poi_copy_to_slot(player, 3);
      poi_count_rings_generic(&r, 3, ring_tile_no_grass);
      poi_copy_to_slot(player, 2);
      poi_count_rings_generic(&r, 8, ring_tile_no_grass);
      poi_copy_to_slot(player, 1);
      break;
    case AI_CAN_BUILD_LARGE:
      poi_count_rings_generic(&r, 3, ring_tile_full);
      poi_copy_to_slot(player, 3);
      poi_count_rings_generic(&r, 4, ring_tile_full);
      poi_copy_to_slot(player, 2);
      poi_count_rings_generic(&r, 5, ring_tile_no_grass);
      poi_copy_to_slot(player, 1);
      poi_count_rings_generic(&r, 6, ring_tile_owner);
      break;
    case AI_CAN_BUILD_FLAG:
      poi_check_shore(&r, 3);
      break;
    case AI_CAN_BUILD_CASTLE:
      poi_count_rings_generic(&r, 3, ring_tile_deposits);
      poi_copy_to_slot(player, 3);
      poi_count_rings_generic(&r, 4, ring_tile_deposits);
      poi_copy_to_slot(player, 2);
      poi_count_rings_generic(&r, 5, ring_tile_deposits);
      poi_copy_to_slot(player, 1);
      poi_count_rings_generic(&r, 6, ring_tile_deposits);
      break;
    default:
      /* Attack target: own = owner of the centre vertex. */
      r.owner = static_cast<int>(MAP_OWNER(r.pos));
      poi_count_rings_generic(&r, 2, ring_tile_objects);
      poi_copy_to_slot(player, 3);
      poi_count_rings_generic(&r, 2, ring_tile_objects);
      poi_copy_to_slot(player, 2);
      poi_count_rings_generic(&r, 2, ring_tile_objects);
      poi_copy_to_slot(player, 1);
      poi_count_rings_generic(&r, 3, ring_tile_objects);
      break;
  }
}
