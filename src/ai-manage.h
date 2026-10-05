/*
 * ai-manage.h - Computer player: building management, stocks, attacks
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

#ifndef SRC_AI_MANAGE_H_
#define SRC_AI_MANAGE_H_

#include "src/ai-internal.h"

class AI::Manage {
 public:
  static void manage_buildings(Player *player);  /* 0x294ae */
  static void validate_locations(Player *player);  /* 0x29a70 */
  static void attack(Player *player);  /* 0x29de2 */
  /* Military ratio of player_update_knight_morale @0xb4bc. */
  static int calc_military_ratio(Player *player);

 private:
  static unsigned int sat_add16(unsigned int a, unsigned int b);
  static bool is_military_type(Building::Type type);
  static unsigned int max_building_index();
  static void demolish_building(MapPos pos);  /* 0x29a4c */
  static int manage_fisher(Player *player, Building *building,
                           int budget);  /* 0x29590 */
  static int manage_lumberjack(Player *player, Building *building,
                               int budget);  /* 0x295ca */
  static int manage_stonecutter(Player *player, Building *building,
                                int budget);  /* 0x29606 */
  static int manage_mine(Player *player, Building *building, int budget,
                         Map::Minerals deposit);
  static int manage_forester(Player *player, Building *building,
                             int budget);  /* 0x29708 */
  static bool find_res_dest_cb(Flag *flag, void *data);
  static bool find_serf_dest_cb(Flag *flag, void *data);
  static int find_other_resource_inventory(Flag *flag);  /* 0x239fa */
  static int find_other_serf_inventory(Flag *flag);  /* 0x238fa */
  static int manage_stock(Player *player, Building *building,
                          int budget);  /* 0x2974c */
  static int manage_farm(Player *player, Building *building,
                         int budget);  /* 0x299b0 */
  static AILocation *location_entry(Player *player, int first, int k);
  static void validate_locations_small(Player *player, int first,
                                       int count);  /* 0x29ac8 */
  static void validate_locations_large(Player *player, int first,
                                       int count);  /* 0x29b92 */
  static void validate_locations_end(Player *player, int first,
                                     int count);  /* 0x29d80 */
};

#endif  // SRC_AI_MANAGE_H_
