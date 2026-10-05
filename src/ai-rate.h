/*
 * ai-rate.h - Computer player: site and attack ratings, area statistics
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

#ifndef SRC_AI_RATE_H_
#define SRC_AI_RATE_H_

#include "src/ai-internal.h"

class AI::Rate {
 public:
  /* ai_scan_points_of_interest @0x2e88e: fills player->ai.poi[] around
     the cursor, depending on ai.panel_btn_type. */
  static void scan_points_of_interest(Player *player);
  /* ai_rate_building_site @0x2d858: categories = bit mask of allowed
     site categories (D2 in the original). */
  static void rate_building_site(Player *player, uint32_t categories);
  /* ai_rate_attack_target @0x2d3de. */
  static void rate_attack_target(Player *player);
  /* ai_rate_site_dispatch @0x2b830: rating of the cursor site for
     category ai_game.build_building_type (returned D7). */
  static int rate_site_dispatch(Player *player);
  /* ai_rate_site_castle @0x2e68a, called directly by the castle
     placement. */
  static int rate_site_castle(Player *player);

 private:
  /* Evaluator: returns the D7 register of the original. */
  typedef uint32_t RateFn(Player *player);

  /* Ring walk state (D1 position, D3 side length - 1, D4 weight, D7
     owner of the original). It carries over between the ring groups. */
  typedef struct Ring {
    MapPos pos;
    int side;
    int weight;
    int owner;
    AIPoi *poi;
  } Ring;

  typedef void RingTileFn(Ring *r, MapPos pos);

  /* Number of location categories rated (0..34). */
  static const int category_count = 35;
  /* Evaluators by location category (the index of ai.locations[] and
     of the bits of rate_building_site's mask). */
  static RateFn *const rate_by_category[category_count];

  /* Fixed point helpers. */
  static unsigned int clampw(unsigned int v, unsigned int n);
  static uint32_t mul(uint32_t d7, unsigned int d6);
  static unsigned int wsum(AIPoi *const p[4], int off);

  /* Attack target evaluators. */
  static uint32_t attack_score_base(Player *player);
  static uint32_t rate_attack_steel_weapons(Player *player);
  static uint32_t rate_attack_tools(Player *player);
  static uint32_t rate_attack_gold(Player *player);
  static uint32_t rate_attack_food(Player *player);
  static uint32_t rate_attack_wood_stone(Player *player);
  static uint32_t rate_attack_gold_deposit(Player *player);
  static uint32_t rate_attack_iron_deposit(Player *player);
  static uint32_t rate_attack_coal_deposit(Player *player);
  static uint32_t rate_attack_stone_deposit(Player *player);

  /* Location store. */
  static void store_location(Player *player, int category, uint32_t value);

  /* Common site factors. */
  static unsigned int military_near(AIPoi *a2, AIPoi *a3);
  static unsigned int economy_near(AIPoi *a1);
  static uint32_t site_score_base_b(Player *player, uint32_t d7);
  static uint32_t site_score_base_a(Player *player, uint32_t d7);
  static uint32_t site_score_base_mine(Player *player, uint32_t d7);

  /* Building site evaluators. */
  static uint32_t rate_site_none(Player *player);
  static uint32_t rate_site_fisher(Player *player);
  static uint32_t rate_site_lumberjack(Player *player);
  static uint32_t rate_site_boatbuilder(Player *player);
  static uint32_t rate_site_stonecutter(Player *player);
  static uint32_t rate_site_mine(Player *player, int sign_off, int mine_off);
  static uint32_t rate_site_stonemine(Player *player);
  static uint32_t rate_site_coalmine(Player *player);
  static uint32_t rate_site_ironmine(Player *player);
  static uint32_t rate_site_goldmine(Player *player);
  static uint32_t rate_site_forester(Player *player);
  static uint32_t rate_site_stock(Player *player);
  static uint32_t rate_site_hut(Player *player);
  static uint32_t rate_site_farm(Player *player);
  static uint32_t rate_site_butcher(Player *player);
  static uint32_t rate_site_pigfarm(Player *player);
  static uint32_t rate_site_mill(Player *player);
  static uint32_t rate_site_baker(Player *player);
  static uint32_t rate_site_sawmill(Player *player);
  static uint32_t rate_site_steelsmelter(Player *player);
  static uint32_t rate_site_toolmaker(Player *player);
  static uint32_t rate_site_weaponsmith(Player *player);
  static uint32_t rate_site_tower(Player *player);
  static uint32_t rate_site_fortress(Player *player);
  static uint32_t rate_site_goldsmelter(Player *player);
  static uint32_t rate_site_geologist(Player *player);
  /* The legacy static rate_site_castle (whole D7), renamed so that it
     does not clash with the public rate_site_castle. */
  static uint32_t rate_site_castle_d7(Player *player);

  /* Area statistics. */
  static void poi_add(AIPoi *poi, int off, int w);
  static int water_vertex(MapPos pos);
  static void ring_count_owner(Ring *r, MapPos pos);
  static void ring_count_building(Ring *r, MapPos pos, int done_only);
  static void ring_tile_deposits(Ring *r, MapPos pos);
  static void ring_tile_full(Ring *r, MapPos pos);
  static void ring_tile_no_grass(Ring *r, MapPos pos);
  static void ring_tile_owner(Ring *r, MapPos pos);
  static void ring_tile_objects(Ring *r, MapPos pos);
  static void ring_tile_shore(Ring *r, MapPos pos);
  static void poi_count_rings_generic(Ring *r, int rings, RingTileFn *fn);
  static void poi_check_shore(Ring *r, int rings);
  static void poi_copy_to_slot(Player *player, int slot);
};

#endif  // SRC_AI_RATE_H_
