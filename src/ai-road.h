/*
 * ai-road.h - Computer player: road building
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

#ifndef SRC_AI_ROAD_H_
#define SRC_AI_ROAD_H_

#include "src/ai-internal.h"

class AI::Road {
 public:
  /* ai_build_road @0x2a8f2: road from the cursor; parameters in
     ai.u_19c, u_19e, u_1a4, u_1a8, u_1ba and player->build bit 4.
     Returns < 0 on failure (the original's N flag). */
  static int build_road(Player *player);
  /* ai_find_flag_connection @0x29316. */
  static int find_flag_connection(Player *player);
  /* ai_pull_roads_through_flag @0x271d4 at the cursor; < 0 on failure. */
  static int pull_roads_through_flag(Player *player);

 private:
  static void cursor_at(Player *player, MapPos pos);
  static int water_marker(MapPos pos);
  static int tile_blocked(MapPos pos);
  static int owned_by(MapPos pos, const Player *player);
  static void road_tile_cost(Player *player, int index, MapPos pos, int ring,
                             int *found);
  static int road_path_length(uint64_t path);
  static int road_path_dirs(uint64_t path, Direction dirs[]);
  static int build_road_along_path(Player *player, uint64_t path,
                                   int category);
};

#endif  // SRC_AI_ROAD_H_
