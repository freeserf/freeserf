/*
 * ai.cc - Computer player of the Amiga original
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

#include "src/ai.h"

#include "src/ai-internal.h"
#include "src/ai-core.h"
#include "src/ai-manage.h"

Game *AI::game = nullptr;

/* Space taken by a map object (Amiga map_space_from_obj @0x1f42). */
const AISpace ai_space_from_obj[128] = {
  AI_SPACE_OPEN,    // NONE = 0,
  AI_SPACE_FLAG,   // FLAG,
  AI_SPACE_SMALL_BUILDING,  // SMALL_BUILDING,
  AI_SPACE_LARGE_BUILDING,  // LARGE_BUILDING,
  AI_SPACE_CASTLE,  // CASTLE,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,

  AI_SPACE_FILLED,   // TREE_0 = 8,
  AI_SPACE_FILLED,   // TREE_1,
  AI_SPACE_FILLED,   // TREE_2, /* 10 */
  AI_SPACE_FILLED,   // TREE_3,
  AI_SPACE_FILLED,   // TREE_4,
  AI_SPACE_FILLED,   // TREE_5,
  AI_SPACE_FILLED,   // TREE_6,
  AI_SPACE_FILLED,   // TREE_7, /* 15 */

  AI_SPACE_FILLED,   // PINE_0,
  AI_SPACE_FILLED,   // PINE_1,
  AI_SPACE_FILLED,   // PINE_2,
  AI_SPACE_FILLED,   // PINE_3,
  AI_SPACE_FILLED,   // PINE_4, /* 20 */
  AI_SPACE_FILLED,   // PINE_5,
  AI_SPACE_FILLED,   // PINE_6,
  AI_SPACE_FILLED,   // PINE_7,

  AI_SPACE_FILLED,   // PALM_0,
  AI_SPACE_FILLED,   // PALM_1, /* 25 */
  AI_SPACE_FILLED,   // PALM_2,
  AI_SPACE_FILLED,   // PALM_3,

  AI_SPACE_IMPASSABLE,  // WATER_TREE_0,
  AI_SPACE_IMPASSABLE,  // WATER_TREE_1,
  AI_SPACE_IMPASSABLE,  // WATER_TREE_2, /* 30 */
  AI_SPACE_IMPASSABLE,  // WATER_TREE_3,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,
  AI_SPACE_OPEN,

  AI_SPACE_IMPASSABLE,  // STONE_0 = 72,
  AI_SPACE_IMPASSABLE,  // STONE_1,
  AI_SPACE_IMPASSABLE,  // STONE_2,
  AI_SPACE_IMPASSABLE,  // STONE_3, /* 75 */
  AI_SPACE_IMPASSABLE,  // STONE_4,
  AI_SPACE_IMPASSABLE,  // STONE_5,
  AI_SPACE_IMPASSABLE,  // STONE_6,
  AI_SPACE_IMPASSABLE,  // STONE_7,

  AI_SPACE_IMPASSABLE,  // SANDSTONE_0, /* 80 */
  AI_SPACE_IMPASSABLE,  // SANDSTONE_1,
  //
  AI_SPACE_FILLED,   // CROSS,
  AI_SPACE_OPEN,    // STUB,

  AI_SPACE_OPEN,    // STONE,
  AI_SPACE_OPEN,    // SANDSTONE_3, /* 85 */

  AI_SPACE_OPEN,    // CADAVER_0,
  AI_SPACE_OPEN,    // CADAVER_1,

  AI_SPACE_IMPASSABLE,  // WATER_STONE_0,
  AI_SPACE_IMPASSABLE,  // WATER_STONE_1,

  AI_SPACE_FILLED,   // CACTUS_0, /* 90 */
  AI_SPACE_FILLED,   // CACTUS_1,

  AI_SPACE_FILLED,   // DEAD_TREE,

  AI_SPACE_FILLED,   // FELLED_PINE_0,
  AI_SPACE_FILLED,   // FELLED_PINE_1,
  AI_SPACE_FILLED,   // FELLED_PINE_2, /* 95 */
  AI_SPACE_FILLED,   // FELLED_PINE_3,
  AI_SPACE_OPEN,    // FELLED_PINE_4,

  AI_SPACE_FILLED,   // FELLED_TREE_0,
  AI_SPACE_FILLED,   // FELLED_TREE_1,
  AI_SPACE_FILLED,   // FELLED_TREE_2, /* 100 */
  AI_SPACE_FILLED,   // FELLED_TREE_3,
  AI_SPACE_OPEN,    // FELLED_TREE_4,

  AI_SPACE_FILLED,   // NEW_PINE,
  AI_SPACE_FILLED,   // NEW_TREE,

  AI_SPACE_IMPASSABLE,  // SEEDS_0, /* 105 */
  AI_SPACE_IMPASSABLE,  // SEEDS_1,
  AI_SPACE_IMPASSABLE,  // SEEDS_2,
  AI_SPACE_IMPASSABLE,  // SEEDS_3,
  AI_SPACE_IMPASSABLE,  // SEEDS_4,
  AI_SPACE_IMPASSABLE,  // SEEDS_5, /* 110 */
  AI_SPACE_OPEN,    // FIELD_EXPIRED,

  AI_SPACE_OPEN,    // SIGN_LARGE_GOLD,
  AI_SPACE_OPEN,    // SIGN_SMALL_GOLD,
  AI_SPACE_OPEN,    // SIGN_LARGE_IRON,
  AI_SPACE_OPEN,    // SIGN_SMALL_IRON, /* 115 */
  AI_SPACE_OPEN,    // SIGN_LARGE_COAL,
  AI_SPACE_OPEN,    // SIGN_SMALL_COAL,
  AI_SPACE_OPEN,    // SIGN_LARGE_STONE,
  AI_SPACE_OPEN,    // SIGN_SMALL_STONE,

  AI_SPACE_OPEN,    // SIGN_EMPTY, /* 120 */

  AI_SPACE_IMPASSABLE,  // FIELD_0,
  AI_SPACE_IMPASSABLE,  // FIELD_1,
  AI_SPACE_IMPASSABLE,  // FIELD_2,
  AI_SPACE_IMPASSABLE,  // FIELD_3,
  AI_SPACE_IMPASSABLE,  // FIELD_4, /* 125 */
  AI_SPACE_IMPASSABLE,  // FIELD_5,
  AI_SPACE_INVALID,   // 127
};


bool
ai_map_blocked(MapPos pos) {
  if (MAP_IN_WATER(pos)) {
    return true;
  }
  AISpace space = MAP_SPACE(pos);
  return space >= AI_SPACE_IMPASSABLE && space != AI_SPACE_FLAG;
}

void
AI::update(Game *game_, Player *player) {
  game = game_;
  Core::update(player);
}

void
AI::scan_sites(Game *game_, Player *player) {
  game = game_;
  Core::scan_sites(player);
}

void
AI::update_build_damping_all(Game *game_) {
  game = game_;
  Core::update_build_damping_all();
}

int
AI::calc_military_ratio(Game *game_, Player *player) {
  game = game_;
  return Manage::calc_military_ratio(player);
}

void
AI::building_conquered(Game *game_, Player *player, MapPos flag_pos) {
  game = game_;
  if (!PLAYER_IS_AI(player)) return;
  int *pending = &player->ai.u_1bc;
  for (int i = 0; i < 8; i++) {
    if (pending[2*i] & 0x8000) {
      pending[2*i] = MAP_POS_COL(flag_pos);
      pending[2*i+1] = MAP_POS_ROW(flag_pos);
      break;
    }
  }
}

void
AI::castle_given(Game *game_, Player *player, MapPos pos) {
  game = game_;
  AI_SET_CURSOR(player, pos);
  if (PLAYER_IS_AI(player)) {
    player->ai.phase = 1;
    player->ai.counter = 0x18;
  }
}
