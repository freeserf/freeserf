/*
 * ai-core.cc - Computer player of the Amiga original, core
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

/* Port of the Amiga AI core: the phase machine update (0x28ee2) with
   its slot dispatch, the random site scan (0x2d162), the computer
   player's map cursor evaluation (0x19368), the settings/priority
   updates, the building statistics and the build step (0x2a5c4).
   All values are unsigned 16-bit as in the original: W() wraps, sadd()
   saturates (add + bcs -> 0xffff). */

#include "src/ai-core.h"

#include <cstddef>

#include "src/ai-rate.h"
#include "src/ai-road.h"
#include "src/ai-want.h"
#include "src/ai-manage.h"
#include "src/misc.h"

#define W(x)  (static_cast<unsigned int>(x) & 0xffff)

/* Saturating unsigned 16-bit add. */
unsigned int
AI::Core::sadd(unsigned int a, unsigned int b) {
  unsigned int r = W(a) + W(b);
  return r > 0xffff ? 0xffff : r;
}

/* Unsigned 16-bit subtract clamped at 0 (sub + bcs -> 0). */
unsigned int
AI::Core::ssub(unsigned int a, unsigned int b) {
  return W(a) < W(b) ? 0 : W(a) - W(b);
}


/* ---- Amiga data ---- */


/* Site categories allowed per panel_btn_type 1..5 (0x2d234). */
static const uint32_t site_categories[5] = {
  0x1, 0x20001e0, 0x8a1e, 0xfffe1e, 0x1000000
};

/* Knight occupation table of adjust_flags (0x2a4b0): 17 rows of
   occupation values for threat levels 3, 2, 1, 0. */
static const uint8_t occupation_rows[17][4] = {
  {4, 4, 4, 4}, {4, 4, 4, 3}, {4, 4, 4, 2}, {4, 4, 3, 2},
  {4, 4, 3, 1}, {4, 4, 2, 1}, {4, 3, 2, 1}, {4, 3, 2, 0},
  {4, 3, 1, 0}, {4, 3, 0, 0}, {4, 2, 0, 0}, {4, 1, 0, 0},
  {3, 1, 0, 0}, {2, 1, 0, 0}, {2, 0, 0, 0}, {1, 0, 0, 0},
  {0, 0, 0, 0}
};

/* Knights per occupation value for hut, tower, fortress (0x2a4f4). */
static const uint8_t knights_per_level[3][5] = {
  {1, 1, 2, 2, 3}, {1, 2, 3, 4, 6}, {1, 3, 6, 9, 12}
};


/* ---- Access to the statistics words ---- */

/* Word at Amiga pointer-relative offset off (0x33c..0x3ce): poi[4]
   (used as statistics array) and u_394..u_3ce. Same layout as
   ai-want.cc reads. */
int *
AI::Core::stat(Player *player, int off) {
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

  if (off >= 0x394) {
    return reinterpret_cast<int *>(reinterpret_cast<char *>(&player->ai) +
                                   u_offset[(off - 0x394) / 2]);
  }

  int w = (off - 0x33c) / 2;
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

/* ptr+0x33c + 2*slot: stock fill ratio. */
#define FILL(slot)   (*stat(player, 0x33c + 2*(slot)))
/* ptr+0x366 + 2*serf_type: serfs idle in inventories. */
#define IDLE(type)   (*stat(player, 0x366 + 2*(type)))
/* ptr+0x39c + 2*resource: resources in all inventories. */
#define STOCK(res)   (*stat(player, 0x39c + 2*(res)))

/* Pending road connections (ptr+0x1bc..0x1da): 8 x {col, row}, empty
   when col has bit 15 set (the original's tst.l bmi). Filled by the
   game when a knight of the AI player occupies an enemy building. */
int *
AI::Core::pending(Player *player, int i, int row) {
  PlayerAI *ai = &player->ai;
  int *tab[16] = {
    &ai->u_1bc, &ai->u_1be, &ai->u_1c0, &ai->u_1c2,
    &ai->u_1c4, &ai->u_1c6, &ai->u_1c8, &ai->u_1ca,
    &ai->u_1cc, &ai->u_1ce, &ai->u_1d0, &ai->u_1d2,
    &ai->u_1d4, &ai->u_1d6, &ai->u_1d8, &ai->u_1da
  };
  return tab[2*i + row];
}


/* ---- Map helpers (Amiga tile bytes) ---- */

/* Owner bits of the original's height byte: bit 7 has owner, bits 5-6
   owner. */
unsigned int
AI::Core::owner_bits(MapPos pos) {
  if (!MAP_HAS_OWNER(pos)) return 0;
  return 0x80 | ((MAP_OWNER(pos) & 3) << 5);
}

/* Type byte of the original: up triangle in the high nibble. */
unsigned int
AI::Core::type_byte(MapPos pos) {
  return (MAP_TYPE_UP(pos) << 4) | MAP_TYPE_DOWN(pos);
}

#define PATHBITS(pos)   (static_cast<unsigned int>(MAP_PATHS(pos) & 0x3f))

/* Owner pattern of the height byte for the player. */
unsigned int
AI::Core::own_bits(const Player *player) {
  return 0x80 | ((PLAYER_NUM(player) & 3) << 5);
}

static bool
is_building_obj(int obj) {
  return obj >= Map::ObjectSmallBuilding && obj <= Map::ObjectCastle;
}


/* ---- Map cursor of the original (determine_map_cursor_type @0x19368,
   shared with the human player's panel) ---- */

/* Owner pattern for player; a player without castle builds on land
   without owner. */
unsigned int
AI::Core::cursor_own(const Player *player) {
  if (!PLAYER_HAS_CASTLE(player)) return 0;
  return own_bits(player);
}

/* determine_map_cursor_type_sub @0x196d8: 0 grass/desert, 1 mountain,
   2 water or other. */
int
AI::Core::cursor_triangle_class(unsigned int type) {
  if (type < 4) return 2;
  if (type < 8) return 0;
  if (type >= 11 && type < 15) return 1;
  return 2;
}

/* Military buildings in the second shell only (spiral 7..18). */
int
AI::Core::cursor_military_near(MapPos pos) {
  for (int i = 7; i <= 18; i++) {
    MapPos p = MAP_POS_ADD_SPIRALLY(pos, i);
    if (is_building_obj(MAP_OBJ(p))) {
      Building *b = AI_BUILDING(MAP_OBJ_INDEX(p));
      Building::Type t = BUILDING_TYPE(b);
      if (t == Building::TypeHut || t == Building::TypeTower ||
          t == Building::TypeFortress || t == Building::TypeCastle) {
        return 1;
      }
    }
  }
  return 0;
}

/* determine_possible_building @0x19660. */
void
AI::Core::cursor_possible_building(const Player *player, MapPos pos,
                                   unsigned int own, Cursor *c) {
  bool has_castle = PLAYER_HAS_CASTLE(player);

  for (int i = 1; i <= 6; i++) {
    if (owner_bits(MAP_POS_ADD_SPIRALLY(pos, i)) != own) return;
  }

  MapPos p4 = MAP_POS_ADD_SPIRALLY(pos, 4);
  MapPos p5 = MAP_POS_ADD_SPIRALLY(pos, 5);
  MapPos p6 = MAP_POS_ADD_SPIRALLY(pos, 6);
  int cls = cursor_triangle_class(type_byte(pos) >> 4) |
            cursor_triangle_class(type_byte(pos) & 0xf) |
            cursor_triangle_class(type_byte(p4) & 0xf) |
            cursor_triangle_class(type_byte(p5) >> 4) |
            cursor_triangle_class(type_byte(p5) & 0xf) |
            cursor_triangle_class(type_byte(p6) >> 4);
  if (cls >= 2) return;
  if (cls == 1) {
    /* Some mountain, the rest grass: a mine. */
    if (has_castle) c->possibility = AI_CAN_BUILD_MINE;
    return;
  }
  if (has_castle) c->possibility = AI_CAN_BUILD_SMALL;

  c->military_known = 1;
  c->no_military = cursor_military_near(pos);

  /* Large: the first shell passable (flags allowed), no large
     building or castle in the second, all grass, small enough
     height differences. */
  for (int i = 1; i <= 6; i++) {
    AISpace sp = MAP_SPACE(MAP_POS_ADD_SPIRALLY(pos, i));
    if (sp >= AI_SPACE_IMPASSABLE && sp != AI_SPACE_FLAG) return;
  }
  for (int i = 7; i <= 18; i++) {
    if (MAP_SPACE(MAP_POS_ADD_SPIRALLY(pos, i)) >= AI_SPACE_LARGE_BUILDING) {
      return;
    }
  }

  if (type_byte(pos) != 0x55) return;
  if ((type_byte(p4) & 0xf) != 5) return;
  if (type_byte(p5) != 0x55) return;
  if ((type_byte(p6) & 0xf0) != 0x50) return;

  unsigned int h_min = 31, h_max = 0;
  for (int i = 7; i <= 18; i++) {
    unsigned int h = MAP_HEIGHT(MAP_POS_ADD_SPIRALLY(pos, i));
    if (h <= h_min) h_min = h;
    if (h > h_max) h_max = h;
  }
  for (int i = 19; i <= 36; i++) {
    MapPos p = MAP_POS_ADD_SPIRALLY(pos, i);
    if (MAP_OBJ(p) != Map::ObjectLargeBuilding) continue;
    Building *b = AI_BUILDING(MAP_OBJ_INDEX(p));
    if (BUILDING_IS_DONE(b) || b->progress != 0) continue;
    unsigned int h = b->u.level & 0xff;
    if (h <= h_min) h_min = h;
    if (h > h_max) h_max = h;
  }
  if (((h_max - h_min) & 0xffff) >= 9) return;

  c->possibility = has_castle ? AI_CAN_BUILD_LARGE : AI_CAN_BUILD_CASTLE;
}

/* Tail of determine_map_cursor_type from 0x194ee: flag and building
   possibilities once the cursor type is known. */
void
AI::Core::cursor_build_possibility(const Player *player, MapPos pos,
                                   unsigned int own, Cursor *c) {
  if (MAP_SPACE(pos) != AI_SPACE_OPEN) return;

  /* All six triangles water? */
  if (!(type_byte(pos) & 0xcc) &&
      !(type_byte(MAP_POS_ADD_SPIRALLY(pos, 4)) & 0x0c) &&
      !(type_byte(MAP_POS_ADD_SPIRALLY(pos, 5)) & 0xcc) &&
      !(type_byte(MAP_POS_ADD_SPIRALLY(pos, 6)) & 0xc0)) {
    return;
  }

  bool flag_near = false;
  for (int i = 1; i <= 6; i++) {
    if (MAP_SPACE(MAP_POS_ADD_SPIRALLY(pos, i)) == AI_SPACE_FLAG) {
      flag_near = true;
      break;
    }
  }
  if (!flag_near) {
    c->no_flag = 0;
    if (PLAYER_HAS_CASTLE(player)) c->possibility = AI_CAN_BUILD_FLAG;
  }
  if (c->cursor_type == AI_CURSOR_PATH) return;

  for (int i = 1; i <= 6; i++) {
    if (MAP_SPACE(MAP_POS_ADD_SPIRALLY(pos, i)) >= AI_SPACE_SMALL_BUILDING) {
      return;
    }
  }
  if (c->cursor_type != AI_CURSOR_CLEAR_BY_FLAG &&
      MAP_SPACE(MAP_POS_ADD_SPIRALLY(pos, 2)) != AI_SPACE_OPEN) {
    return;
  }

  /* No other flag next to the building's flag. */
  static const int flag_ring[] = { 7, 8, 14, 1, 3 };
  for (int i = 0; i < 5; i++) {
    if (MAP_SPACE(MAP_POS_ADD_SPIRALLY(pos, flag_ring[i])) == AI_SPACE_FLAG) {
      return;
    }
  }

  /* Triangles around the building flag must be land (L12). */
  if (!(type_byte(MAP_POS_ADD_SPIRALLY(pos, 1)) & 0xc0) ||
      !(type_byte(MAP_POS_ADD_SPIRALLY(pos, 3)) & 0x0c) ||
      !(type_byte(MAP_POS_ADD_SPIRALLY(pos, 2)) & 0xc0) ||
      !(type_byte(MAP_POS_ADD_SPIRALLY(pos, 2)) & 0x0c)) {
    return;
  }

  cursor_possible_building(player, pos, own, c);
}

/* get_map_cursor_type @0x194ba: cursor on a free vertex. */
void
AI::Core::cursor_clear(const Player *player, MapPos pos, unsigned int own,
                       Cursor *c) {
  MapPos p2 = MAP_POS_ADD_SPIRALLY(pos, 2);

  if (MAP_SPACE(p2) == AI_SPACE_FLAG) {
    c->cursor_type = AI_CURSOR_CLEAR_BY_FLAG;
  } else if (PATHBITS(p2) != 0) {
    c->cursor_type = AI_CURSOR_CLEAR_BY_PATH;
  } else {
    c->cursor_type = AI_CURSOR_CLEAR;
  }
  cursor_build_possibility(player, pos, own, c);
}

void
AI::Core::cursor_reset(Cursor *c) {
  c->cursor_type = AI_CURSOR_NONE;
  c->possibility = AI_CAN_BUILD_NONE;
  c->no_flag = 1;
  c->no_military = 0;
  c->military_known = 0;
}

/* What the player can do at pos, as determine_map_cursor_type of the
   original decides it for the panel and the build actions. */
void
AI::Core::get_map_cursor(const Player *player, MapPos pos, Cursor *c) {
  unsigned int own = cursor_own(player);
  cursor_reset(c);

  if (owner_bits(pos) != own) return;

  AISpace sp = MAP_SPACE(pos);
  if (sp == AI_SPACE_FLAG) {
    if ((MAP_PATHS(pos) & BIT(DirectionUpLeft)) &&
        MAP_SPACE(MAP_POS_ADD_SPIRALLY(pos, 5)) >= AI_SPACE_SMALL_BUILDING) {
      c->cursor_type = AI_CURSOR_FLAG;
      return;
    }
    if (PATHBITS(pos) == 0) {
      c->cursor_type = AI_CURSOR_REMOVABLE_FLAG;
      return;
    }

    Flag *flag = AI_FLAG(MAP_OBJ_INDEX(pos));
    Flag *other = nullptr;
    int paths = 0;
    for (int d = 5; d >= 0; d--) {
      if (!(flag->path_con & BIT(d))) continue;
      if (!(flag->endpoint & BIT(d))) {
        /* Water path */
        c->cursor_type = AI_CURSOR_FLAG;
        return;
      }
      paths += 1;
      if (other == nullptr) {
        other = flag->other_endpoint.f[d];
      } else if (other == flag->other_endpoint.f[d]) {
        c->cursor_type = AI_CURSOR_FLAG;
        return;
      }
    }
    c->cursor_type = (paths == 2) ? AI_CURSOR_REMOVABLE_FLAG :
                                    AI_CURSOR_FLAG;
    return;
  } else if (sp >= AI_SPACE_SMALL_BUILDING) {
    /* Original bug: object 127 is taken for a building. */
    if (sp == AI_SPACE_CASTLE || sp == AI_SPACE_INVALID) return;
    Building *b = AI_BUILDING(MAP_OBJ_INDEX(pos));
    if (BUILDING_IS_BURNING(b)) return;
    c->cursor_type = AI_CURSOR_BUILDING;
    /* What could be built here instead (replace building). */
    cursor_possible_building(player, pos, own, c);
    return;
  }

  unsigned int paths = PATHBITS(pos);
  if (paths == 0) {
    cursor_clear(player, pos, own, c);
    return;
  }
  if (paths == BIT(DirectionDownRight) || paths == BIT(DirectionUpLeft)) {
    return;
  }
  c->cursor_type = AI_CURSOR_PATH;
  cursor_build_possibility(player, pos, own, c);
}

void
AI::Core::set_cursor_result(Player *player, const Cursor *c) {
  player->ai.map_cursor_type = c->cursor_type;
  player->ai.panel_btn_type = c->possibility;
  if (c->no_flag) {
    player->build |= BIT(1);
  } else {
    player->build &= ~BIT(1);
  }
  if (c->military_known) {
    if (c->no_military) {
      player->build |= BIT(0);
    } else {
      player->build &= ~BIT(0);
    }
  }
}

/* determine_map_cursor_type @0x19368 for the AI cursor. */
void
AI::Core::determine_map_cursor_type(Player *player) {
  Cursor c;
  get_map_cursor(player, AI_CURSOR_POS(player), &c);
  set_cursor_result(player, &c);
}

/* get_map_cursor_type_at @0x1932e: get_map_cursor_type for pos
   without the checks of the center (the caller scan_sites made them);
   the owner pattern own is the caller's D3. The cursor is not moved. */
void
AI::Core::get_map_cursor_type_at(Player *player, MapPos pos,
                                 unsigned int own) {
  Cursor c;
  cursor_reset(&c);
  cursor_clear(player, pos, own, &c);
  set_cursor_result(player, &c);
}


/* ---- Site scan and start ---- */

/* player_ai @0x2d162: map_regions * 8 random positions. The first own
   free site that allows a building is rated as building site, the
   first finished, active enemy military building of state 3 near own
   land as attack target; then the scan ends. */
void
AI::Core::scan_sites(Player *player) {
  PlayerAI *ai = &player->ai;
  int n = W(AI_MAP->get_region_count() * 8);
  unsigned int own = PLAYER_HAS_CASTLE(player) ? own_bits(player) : 0;

  for (int i = 0; i < n; i++) {
    uint32_t r = static_cast<uint32_t>(AI_GAME->random_int()) << 16;
    r |= AI_GAME->random_int();
    MapPos pos = MAP_POS((r >> 2) & AI_MAP->get_col_mask(),
                         (r >> (AI_MAP->get_row_shift() + 3)) &
                         AI_MAP->get_row_mask());

    unsigned int owner = owner_bits(pos);
    if (owner == own) {
      if (MAP_SPACE(pos) != 0 || PATHBITS(pos) != 0) continue;

      get_map_cursor_type_at(player, pos, own);
      if (ai->map_cursor_type < AI_CURSOR_CLEAR_BY_FLAG) continue;
      if (ai->panel_btn_type == AI_CAN_BUILD_NONE) continue;

      AI_SET_CURSOR(player, pos);
      uint32_t cat = site_categories[ai->panel_btn_type - 1];
      if (player->build & BIT(0)) cat &= ~0x600800; /* no military */
      if (player->build & BIT(1)) cat &= ~1; /* no flag */
      AI::Rate::scan_points_of_interest(player);
      AI::Rate::rate_building_site(player, cat);
      return;
    }

    if (!(owner & 0x80)) continue;
    if (!is_building_obj(MAP_OBJ(pos))) continue;

    Building *b = AI_BUILDING(MAP_OBJ_INDEX(pos));
    Building::Type t = BUILDING_TYPE(b);
    if (!BUILDING_IS_DONE(b) ||
        (t != Building::TypeHut && t != Building::TypeTower &&
         t != Building::TypeFortress && t != Building::TypeCastle)) {
      continue;
    }
    if (BUILDING_STATE(b) != 3 || !BUILDING_IS_ACTIVE(b)) continue;

    bool near_own = false;
    for (int j = 7; j < 7 + 0x102; j++) {
      if (owner_bits(MAP_POS_ADD_SPIRALLY(b->get_position(), j)) ==
          own_bits(player)) {
        near_own = true;
        break;
      }
    }
    if (!near_own) continue;

    ai->panel_btn_type = AI_CAN_BUILD_NONE;
    AI_SET_CURSOR(player, pos);
    AI::Rate::scan_points_of_interest(player);
    AI::Rate::rate_attack_target(player);
    return;
  }
}

/* ai_wait @0x2d082: phase 1. */
void
AI::Core::wait(Player *player) {
  scan_sites(player);
  player->ai.counter = W(player->ai.counter - 1);
  if (player->ai.counter == 0) {
    player->ai.phase = 2;
    player->ai.counter = 0xffff;
    player->ai.build_threshold = 0;
  }
}

/* Best stored site of a category: highest value (first one wins),
   nullptr if all are 0. */
AILocation *
AI::Core::best_location(Player *player, int category) {
  AILocation *best = nullptr;
  unsigned int value = 0;
  for (int i = 0; i < AI_LOCATIONS_PER_CATEGORY; i++) {
    AILocation *loc = &player->ai.locations[category][i];
    if (W(loc->value) > value) {
      value = W(loc->value);
      best = loc;
    }
  }
  return best;
}

/* ai_place_castle @0x2d09e: phase 0. */
void
AI::Core::place_castle(Player *player) {
  scan_sites(player);

  unsigned int tick = W(AI_GAME->get_tick());
  if (tick < 2000) return;
  if (tick < 10000) {
    unsigned int r = AI_GAME->random_int();
    if (tick < 6000) {
      if (r & 0x3f) return;
    } else if (tick < 9000) {
      if (r & 0x1f) return;
    } else {
      if (r & 0xf) return;
    }
  }

  while (true) {
    AILocation *loc = best_location(player, Building::TypeCastle);
    if (loc == nullptr) return;

    ai_game.some_location = loc;
    loc->value = 0;
    player->ai.cursor_col = loc->col;
    player->ai.cursor_row = loc->row;
    determine_map_cursor_type(player);
    if (player->ai.panel_btn_type != AI_CAN_BUILD_CASTLE) continue;

    AI::Rate::scan_points_of_interest(player);
    unsigned int rating = W(AI::Rate::rate_site_castle(player));
    if (rating < W(ai_game.some_location->value)) {
      /* Never taken: the value was just cleared. */
      bool better = false;
      for (int i = 0; i < AI_LOCATIONS_PER_CATEGORY; i++) {
        AILocation *l = &player->ai.locations[Building::TypeCastle][i];
        if (rating < W(l->value)) {
          better = true;
          break;
        }
      }
      if (better) {
        ai_game.some_location->value = rating;
        continue;
      }
    }

    player->ai.phase = 1;
    player->ai.counter = 24;
    AI_GAME->build_castle(AI_CURSOR_POS(player), player);
    return;
  }
}


/* ---- Build damping (0xb094) ---- */

/* ai_update_build_damping @0xb0c0: lower the build threshold by the
   elapsed ticks (but not below a value from the land/building ratio)
   and let the damping of every building type recover. */
void
AI::Core::update_build_damping(Player *player, unsigned int elapsed) {
  /* Shift of the elapsed ticks per building type 1..25
     (-1 = doubled). */
  static const int shift[25] = {
    3, 2, 4, 3, 1, 1, 1, 1, 2, 3, -1, 2, 2, 2, 2, 2,
    2, 2, 3, 2, 2, 2, 2, -1, 2
  };
  PlayerAI *ai = &player->ai;

  if (!PLAYER_IS_AI(player)) return;

  ai->build_threshold = ssub(ai->build_threshold, elapsed);

  if (player->total_building_score != 0) {
    unsigned int div = player->total_building_score & 0xffff;
    uint32_t land = static_cast<uint32_t>(player->total_land_area) << 7;
    /* Original bug: divu.w traps on a zero low word and leaves
       the dividend on overflow; both are skipped here. */
    if (div != 0 && land / div <= 0xffff) {
      unsigned int q = land / div;
      if (q < 0x400) {
        unsigned int v = W((q ^ 0x3ff) << 6);
        if (v >= W(ai->build_threshold)) {
          ai->build_threshold = v;
        }
      }
    }
  }

  for (int i = 0; i < 25; i++) {
    unsigned int inc = shift[i] < 0 ? W(elapsed << 1) :
                                      W(elapsed) >> shift[i];
    ai->build_damp[i] = sadd(ai->build_damp[i], inc);
  }
}

/* ai_update_build_damping_all @0xb094 (scheduler slot 32). */
void
AI::Core::update_build_damping_all() {
  unsigned int elapsed = W(ai_game.ticks_288); /* game+0x28a */
  ai_game.ticks_288 = 0;

  for (int i = 0; i < 4; i++) {
    Player *player = AI_GAME->get_player(i);
    if (PLAYER_IS_ACTIVE(player)) {
      update_build_damping(player, elapsed);
    }
  }
}


/* ---- Settings (slot 0) ---- */

/* ai_adjust_flags @0x2a238: send-strongest flag, knight cycling,
   promotion of serfs to knights, castle knights and the knight
   occupation of the military buildings. */
void
AI::Core::adjust_flags(Player *player) {
  PlayerAI *ai = &player->ai;

  if (AI_GAME->random_int() < W(player->ai_value_4)) {
    player->flags |= BIT(1);
  } else {
    player->flags &= ~BIT(1);
  }

  if (ai->u_1b0 == 0 && player->knight_cycle_counter == 0) {
    unsigned int k0 = W(player->serf_count[Serf::TypeKnight0]);
    unsigned int k1 = W(player->serf_count[Serf::TypeKnight1]);
    unsigned int k2 = W(player->serf_count[Serf::TypeKnight2]);
    unsigned int v, d0, d1, d2;

    /* Knights 2..4 idle in inventories. */
    v = W(IDLE(Serf::TypeKnight4)) << 1;
    if (v <= 0xffff) v += W(IDLE(Serf::TypeKnight3));
    if (v <= 0xffff) v <<= 1;
    if (v <= 0xffff) v += W(IDLE(Serf::TypeKnight2));
    d0 = v > 0xffff ? 0xffff : v;

    /* Knights 0..2 outside the inventories. */
    v = W(k0 - W(IDLE(Serf::TypeKnight0))) << 1;
    if (v <= 0xffff) v += k1;
    if (v <= 0xffff) {
      v = W(W(v - W(IDLE(Serf::TypeKnight1))) << 1) + k2;
      if (v <= 0xffff) v = W(v - W(IDLE(Serf::TypeKnight2)));
    }
    d1 = v > 0xffff ? 0xffff : v;

    /* Capacity of the military buildings. */
    v = W(player->completed_building_count[Building::TypeFortress]) << 1;
    if (v <= 0xffff) {
      v += W(player->completed_building_count[Building::TypeTower]);
    }
    if (v <= 0xffff) v <<= 1;
    if (v <= 0xffff) {
      v += W(player->completed_building_count[Building::TypeHut]);
    }
    if (v <= 0xffff) v <<= 1;
    d2 = v > 0xffff ? 0xffff : v;

    if (d2 < d1 && d2 < d0) {
      /* Start cycling the knights. */
      player->flags |= BIT(2) | BIT(4);
      player->knight_cycle_counter = 1200;
      ai->u_1b0 = 15000;
    }
  }

  /* Promote generic serfs if swords and shields are there. */
  unsigned int generic = 0, armed = 0;
  for (Inventory *inv : AI_GAME->inventories) {
    if (static_cast<int>(inv->owner) != PLAYER_NUM(player)) continue;
    unsigned int n = W(inv->generic_count);
    generic = W(generic + n);
    if (n >= W(inv->resources[Resource::TypeSword])) {
      n = W(inv->resources[Resource::TypeSword]);
    }
    if (n >= W(inv->resources[Resource::TypeShield])) {
      n = W(inv->resources[Resource::TypeShield]);
    }
    armed = W(armed + n);
  }
  if (armed != 0 && generic >= 10) {
    generic = W(generic - armed);
    if (generic >= 10) {
      player->promote_serfs_to_knights(armed);
    } else {
      generic = 10 - generic;
      if (armed > generic) {
        player->promote_serfs_to_knights(armed - generic);
      }
    }
  }

  /* Military buildings by type (hut, tower, fortress) and state
     (game+0x12c scratch in the original). */
  unsigned int count[3][4] = {{0}};
  for (Building *b : AI_GAME->buildings) {
    /* Original bug: the bitmap bit is tested with D1 while D7
       (left from game_random_int) is counted down, so the wrong
       allocation bits are read. */
    if (BUILDING_PLAYER(b) != PLAYER_NUM(player)) continue;
    /* Type byte & 0xfc: unfinished buildings never match. */
    if (!BUILDING_IS_DONE(b)) continue;
    int k;
    switch (BUILDING_TYPE(b)) {
      case Building::TypeHut: k = 0; break;
      case Building::TypeTower: k = 1; break;
      case Building::TypeFortress: k = 2; break;
      default: continue;
    }
    count[k][BUILDING_STATE(b)] = W(count[k][BUILDING_STATE(b)] + 1);
  }

  /* Knights wanted in the castle. */
  unsigned int knights = W(player->serf_count[Serf::TypeKnight0] +
                           player->serf_count[Serf::TypeKnight1] +
                           player->serf_count[Serf::TypeKnight2] +
                           player->serf_count[Serf::TypeKnight3] +
                           player->serf_count[Serf::TypeKnight4]);
  ai->u_1ac = knights;
  unsigned int wanted = (knights >> 2) + 3;
  if (wanted >= 30) {
    wanted = (wanted >> 1) + 15;
    if (wanted >= 50) wanted = (wanted >> 1) + 25;
  }
  if (wanted >= 100) wanted = 99;
  player->castle_knights_wanted = wanted;
  knights = ssub(knights, wanted);
  knights -= knights >> 3;

  /* Highest occupation row the remaining knights can fill. */
  int level = 0;
  for (int r = 0; r < 16; r++) {
    unsigned int need = 0;
    for (int k = 0; k < 4; k++) {
      int threat = 3 - k;
      int v = occupation_rows[r][k];
      need += knights_per_level[0][v] * count[0][threat];
      need += knights_per_level[1][v] * count[1][threat];
      need += knights_per_level[2][v] * count[2][threat];
      need = W(need);
    }
    if (knights >= need) {
      level = 16 - r;
      break;
    }
  }
  ai->u_1aa = level;
  if (W(player->ai_value_0) < static_cast<unsigned int>(level)) {
    level = W(player->ai_value_0);
  }

  const uint8_t *row = occupation_rows[16 - level];
  for (int i = 3; i >= 0; i--) {
    int v = row[3 - i];
    int max = (v << 4) & 0xff;
    int min = v + i - 4;
    if (min < 0) min = 0;
    if (i == 3 && max == 0x30) min = 1;
    player->knight_occupation[i] = max | min;
  }
}

/* Tool priority from the stock of one tool (part of 0x2c906). */
unsigned int
AI::Core::tool_value(unsigned int base, int shift, unsigned int stock) {
  if (W(stock) == 0) return 16;
  return ssub((base >> shift) + 1, stock);
}

/* ai_adjust_priorities @0x2c906: tool, planks, steel, coal, wheat
   and food distribution. */
void
AI::Core::adjust_priorities(Player *player) {
  static const int tool_shift[9] = { 2, 0, 2, 3, 2, 2, 3, 1, 3 };
  unsigned int d0, d1, d2, d3, d4;

  d0 = player->total_land_area >= 0x1000 ? 0xfff :
                                           W(player->total_land_area);
  if (d0 < 0x400) d0 = 0x400;
  d0 >>= 8;

  d2 = 0;
  for (int i = 0; i < 9; i++) {
    unsigned int v = tool_value(d0, tool_shift[i],
                                STOCK(Resource::TypeShovel + i));
    if (i == 0 || v > d2) d2 = v;
    player->tool_prio[i] = W(v * 0xfff);
  }

  d1 = W(d2 * 0xfff);
  player->planks_toolmaker = d1;
  player->steel_toolmaker = d1;
  d1 = W(~d1);
  d3 = sadd(d1, 0xfa0);

  d2 = 0xffff;
  switch (player->knight_occupation[3] & 0xf0) {
    case 0x40: d0 = 0; break;
    case 0x30: d0 = 0x7530; d2 = 0x7530; break;
    case 0x20: d0 = 0xc350; d2 = 0x4e20; break;
    case 0x10: d0 = 0xea60; d2 = 0x2710; break;
    default: d0 = 0xffff; d2 = 0x1388; break;
  }
  if (d1 >= d0) d0 = d1;
  player->steel_weaponsmith = d0;
  if (d3 >= d2) d2 = d3;
  if (W(STOCK(Resource::TypeSteel)) >= 10) d2 = 0xffff;
  player->coal_goldsmelter = d2;
  d4 = d2;
  player->coal_weaponsmith = 0xafc8;

  d0 = W(STOCK(Resource::TypeSteel));
  if (d0 >= 0x80) d0 = 0x7f;
  player->coal_steelsmelter = W(((0x7f - d0) << 8) + 0x8000);

  d0 = sadd(sadd(STOCK(Resource::TypeFish), STOCK(Resource::TypeMeat)),
            STOCK(Resource::TypeBread));
  if (d0 >= 0x80) d0 = 0x7f;
  d0 = W(d0 << 9);
  if (d0 < 0x8000) {
    player->wheat_mill = 0xffff;
    player->wheat_pigfarm = W(d0 + 0x8000);
  } else {
    player->wheat_pigfarm = 0xffff;
    player->wheat_mill = W(W(~d0) + 0x8000);
  }

  d1 = 0xffff;
  d2 = W(ssub(8, STOCK(Resource::TypeBoat)) << 11);
  d0 = W(player->planks_toolmaker);
  if (d0 >= 0xc000) {
    d2 = 0;
    d1 = W(~W((d0 - 0xc000) << 1));
  }
  player->planks_construction = d1;
  player->planks_boatbuilder = d2;

  /* Food for the mines: iron ore vs. gold ore (coal read but
     overwritten in the original). */
  d1 = W(STOCK(Resource::TypeIronOre));
  d0 = d1 >> 2;
  d1 = W(W(d1 << 1) - d0);
  d1 = sadd(d1, STOCK(Resource::TypeGoldOre));
  d2 = W(STOCK(Resource::TypeSteel));
  d2 = W(d2 - (d2 >> 2));
  d1 = W(d1 + d2);
  if (d1 >= d0) {
    d1 -= d0;
    if (d1 >= 0xdc) d1 = 0xdb;
    d1 = W(~(d1 << 8));
    d0 = 0xffff;
  } else {
    d0 -= d1;
    if (d0 >= 0xdc) d0 = 0xdb;
    d0 = W(~(d0 << 8));
    d1 = 0xffff;
  }

  d2 = W(STOCK(Resource::TypeStone));
  if (d2 >= 0x18) d2 = 0x17;
  d2 = W(~(d2 << 11));
  player->food_stonemine = d2;
  if (d2 >= 0xafc8) {
    d0 >>= 1;
    d1 >>= 1;
    if (d2 >= 0xea60) {
      d0 >>= 1;
      d1 >>= 1;
    }
  }
  player->food_coalmine = d0;
  player->food_ironmine = d1;
  player->food_goldmine = d4 < d1 ? d4 : d1;
}

/* ai_update_settings @0x2c8fc: slot 0. */
void
AI::Core::update_settings(Player *player) {
  adjust_flags(player);
  adjust_priorities(player);
}


/* ---- Statistics (slots 1, 5, 9, 13) ---- */

/* ai_stock*_fill_* @0x2cef2..0x2cf7e: requested + 2 * available. */
unsigned int
AI::Core::stock_fill(const Building *b, int i) {
  return W(b->stock[i].requested + 2 * b->stock[i].available);
}

/* ai_update_building_stats @0x2ccc8. */
void
AI::Core::update_building_stats(Player *player) {
  /* Fill slot per building type: stock 0 and stock 1
     (-1 none), capacity of stock 1 for the military buildings. */
  static const struct { int slot0, slot1, cap1; } bld_slots[24] = {
    {-1, -1, 0},   /* NONE */
    {-1, -1, 0},   /* FISHER */
    {-1, -1, 0},   /* LUMBERJACK */
    {0, -1, 0},    /* BOATBUILDER */
    {-1, -1, 0},   /* STONECUTTER */
    {1, -1, 0},    /* STONEMINE */
    {2, -1, 0},    /* COALMINE */
    {3, -1, 0},    /* IRONMINE */
    {4, -1, 0},    /* GOLDMINE */
    {-1, -1, 0},   /* FORESTER */
    {-1, -1, 0},   /* STOCK */
    {-1, 5, 4},    /* HUT */
    {-1, -1, 0},   /* FARM */
    {7, -1, 0},    /* BUTCHER */
    {6, -1, 0},    /* PIGFARM */
    {8, -1, 0},    /* MILL */
    {9, -1, 0},    /* BAKER */
    {-1, 10, 16},  /* SAWMILL */
    {15, 16, 16},  /* STEELSMELTER */
    {11, 12, 16},  /* TOOLMAKER */
    {14, 13, 16},  /* WEAPONSMITH */
    {-1, 5, 8},    /* TOWER */
    {-1, 5, 16},   /* FORTRESS */
    {17, 18, 16}   /* GOLDSMELTER */
  };
  unsigned int fill[21] = {0}, cap[21] = {0};

  for (Building *b : AI_GAME->buildings) {
    if (BUILDING_PLAYER(b) != PLAYER_NUM(player)) continue;

    if (!BUILDING_IS_DONE(b)) {
      /* Construction material: planks, stone. */
      fill[19] = W(fill[19] + stock_fill(b, 0));
      cap[19] = W(cap[19] + ((2 * b->stock[0].maximum) & 0xff));
      fill[20] = W(fill[20] + stock_fill(b, 1));
      cap[20] = W(cap[20] + ((2 * b->stock[1].maximum) & 0xff));
      continue;
    }

    int t = BUILDING_TYPE(b);
    if (t >= 24) continue;
    if (bld_slots[t].slot0 >= 0) {
      int s = bld_slots[t].slot0;
      fill[s] = W(fill[s] + stock_fill(b, 0));
      cap[s] = W(cap[s] + 16);
    }
    if (bld_slots[t].slot1 >= 0) {
      int s = bld_slots[t].slot1;
      fill[s] = W(fill[s] + stock_fill(b, 1));
      cap[s] = W(cap[s] + bld_slots[t].cap1);
    }
  }

  for (int s = 0; s < 21; s++) {
    unsigned int v;
    if (fill[s] == cap[s]) {
      v = 0xffff;
    } else if (cap[s] == 0 || fill[s] > cap[s]) {
      /* Original bug: divu.w traps on 0 and on overflow
         (fill > capacity) leaves 0. Taken as full here. */
      v = 0xffff;
    } else {
      v = (static_cast<uint32_t>(fill[s]) << 16) / cap[s];
    }
    FILL(s) = v;
  }

  /* Serfs idle in inventories by type. */
  for (int t = 0; t < 27; t++) IDLE(t) = 0;
  for (Serf *serf : AI_GAME->serfs) {
    if (serf->state != Serf::StateIdleInStock) continue;
    if (SERF_PLAYER(serf) != PLAYER_NUM(player)) continue;
    IDLE(SERF_TYPE(serf)) = W(IDLE(SERF_TYPE(serf)) + 1);
  }

  /* Resources in all inventories, plus the emergency reserve. */
  for (int r = 0; r < 26; r++) STOCK(r) = 0;
  /* Original bug: the original adds these to build_want[7] and
     build_want[9] (A1 already past the cleared array). */
  STOCK(Resource::TypePlank) = W(STOCK(Resource::TypePlank) +
                                 (player->extra_planks & 0xff));
  STOCK(Resource::TypeStone) = W(STOCK(Resource::TypeStone) +
                                 (player->extra_stone & 0xff));
  for (Inventory *inv : AI_GAME->inventories) {
    if (static_cast<int>(inv->owner) != PLAYER_NUM(player)) continue;
    for (int r = 25; r >= 0; r--) {
      STOCK(r) = sadd(STOCK(r),
                      inv->resources[static_cast<Resource::Type>(r)]);
    }
  }
}


/* ---- Building ---- */

/* ai_check_construction_limit @0x2a504: < 0 if no builder can be
   had or too many buildings are under construction. */
int
AI::Core::check_construction_limit(Player *player) {
  unsigned int transporters = W(IDLE(Serf::TypeTransporter));
  unsigned int generic = W(IDLE(Serf::TypeGeneric));

  if (W(IDLE(Serf::TypeBuilder)) != 0) {
    if (W(transporters + generic) < 2) return -1;
  } else {
    if (W(STOCK(Resource::TypeHammer)) == 0) return -1;
    if (generic == 0) return -1;
    if (W(generic + transporters) < 3) return -1;
  }

  unsigned int incomplete = 0;
  for (int t = Building::TypeFisher; t <= Building::TypeGoldSmelter; t++) {
    incomplete = W(incomplete + player->incomplete_building_count[t]);
  }

  unsigned int limit =
    W((W(player->completed_building_count[Building::TypeStock]) + 3) << 2);
  unsigned int v = (W(STOCK(Resource::TypePlank)) >> 2) + 6;
  if (v < limit) limit = v;
  v = (W(STOCK(Resource::TypeStone)) >> 1) + 8;
  if (v < limit) limit = v;

  return limit < incomplete ? -1 : 0;
}

/* Road builder parameters set before Road::build_road. */
void
AI::Core::road_params(Player *player, int u_1ba, int u_19c, int u_1a8,
                      int u_1a4, int u_19e) {
  player->ai.u_1ba = u_1ba;
  player->ai.u_19c = u_19c;
  player->ai.u_1a8 = u_1a8;
  player->ai.u_1a4 = u_1a4;
  player->ai.u_19e = u_19e;
}

/* Move the AI cursor by (dc, dr) with wrap-around. */
void
AI::Core::move_cursor(Player *player, int dc, int dr) {
  player->ai.cursor_col = (player->ai.cursor_col + dc) &
                          AI_MAP->get_col_mask();
  player->ai.cursor_row = (player->ai.cursor_row + dr) &
                          AI_MAP->get_row_mask();
}

/* ai_build_building @0x2a5c4: build ai_game.build_building_type at
   the best stored site that is still possible. Type 25 builds only a
   flag (category 0), type 24 sends a geologist (category 25) once the
   castle exists. */
void
AI::Core::build_building(Player *player) {
  PlayerAI *ai = &player->ai;
  int mode = 2;              /* D5: -1 flag, 0 geologist, 2/3 building */
  unsigned int allowed = 0x18;  /* D6: panel_btn_type bits */
  int type = ai_game.build_building_type;

  if ((0x3ff7400 >> (type & 31)) & 1) {
    if (type >= 24) {
      if (type == 25) {
        ai_game.build_building_type = 0;
        mode = -1;
        allowed = BIT(AI_CAN_BUILD_FLAG);
      } else if (PLAYER_HAS_CASTLE(player)) {
        ai_game.build_building_type = 25;
        mode = 0;
        allowed = BIT(AI_CAN_BUILD_MINE);
      } else {
        mode = 3;
        allowed = BIT(AI_CAN_BUILD_LARGE);
      }
    } else {
      mode = 3;
      allowed = BIT(AI_CAN_BUILD_LARGE);
    }
  } else if (type >= 5 && type < 9) {
    allowed = BIT(AI_CAN_BUILD_MINE);
  }
  ai_game.g_24a = mode;
  ai_game.g_24c = allowed;

  unsigned int rating;
  while (true) {
    int category = ai_game.build_building_type;
    AILocation *loc = best_location(player, category);
    if (loc == nullptr) return;

    ai_game.some_location = loc;
    loc->value = 0;
    ai->cursor_col = loc->col;
    ai->cursor_row = loc->row;
    determine_map_cursor_type(player);
    if (ai->map_cursor_type < AI_CURSOR_CLEAR_BY_FLAG) continue;
    if (!(allowed & BIT(ai->panel_btn_type & 7))) continue;
    if ((player->build & BIT(0)) &&
        (category == Building::TypeHut || category == Building::TypeTower ||
         category == Building::TypeFortress)) {
      continue;
    }

    AI::Rate::scan_points_of_interest(player);
    rating = W(AI::Rate::rate_site_dispatch(player));
    if (rating < W(ai_game.some_location->value)) {
      /* Never taken: the value was just cleared. */
      bool better = false;
      for (int i = 0; i < AI_LOCATIONS_PER_CATEGORY; i++) {
        if (rating < W(ai->locations[category][i].value)) {
          better = true;
          break;
        }
      }
      if (better) {
        ai_game.some_location->value = rating;
        continue;
      }
    }
    break;
  }

  if (rating == 0) return;

  if (mode < 0) {
    /* Flag only */
    AI_GAME->build_flag(AI_CURSOR_POS(player), player);
    if (rating < 0x9470) {
      AI::Road::pull_roads_through_flag(player);
      player->build |= BIT(4);
      road_params(player, 0, 0, 0, 0, -1);
    } else {
      player->build &= ~BIT(4);
      road_params(player, 0, 0, 0, 6, -1);
    }
    AI::Road::build_road(player);
    return;
  }

  road_params(player, 0, 0, 12, ai->u_1a4, -1);
  if (ai->map_cursor_type != AI_CURSOR_CLEAR) {
    bool flag_with_paths = true;
    if (ai->map_cursor_type == AI_CURSOR_CLEAR_BY_FLAG) {
      MapPos fp = MAP_POS((ai->cursor_col + 1) & AI_MAP->get_col_mask(),
                          (ai->cursor_row + 1) & AI_MAP->get_row_mask());
      flag_with_paths = PATHBITS(fp) != 0;
    }
    if (flag_with_paths) {
      ai->u_19e = 0;
      ai->u_1ba = 30;
      ai->u_19c = 12;
    }
  }

  if (mode == 0) {
    /* Geologist: flag at the building flag position. */
    move_cursor(player, 1, 1);
    if (ai->map_cursor_type >= AI_CURSOR_CLEAR_BY_PATH) {
      if (ai->map_cursor_type == AI_CURSOR_CLEAR_BY_PATH) {
        /* build_flag splits the path. */
        ai->map_cursor_type = AI_CURSOR_PATH;
      }
      AI_GAME->build_flag(AI_CURSOR_POS(player), player);
      if (AI::Road::pull_roads_through_flag(player) >= 0) {
        ai->u_1ba = 50;
        ai->u_19c = 8;
        ai->u_19e = 0;
      }
      ai->u_1a4 = 0;
      if (ai->u_19c != 0) {
        ai->u_19c = 8;
      } else {
        ai->u_1a8 = 8;
      }
      player->build &= ~BIT(4);
      if (AI::Road::build_road(player) < 0) {
        /* build_flag may have refused. */
        MapPos fpos = AI_CURSOR_POS(player);
        if (MAP_OBJ(fpos) == Map::ObjectFlag &&
            !FLAG_HAS_BUILDING(AI_FLAG(MAP_OBJ_INDEX(fpos)))) {
          AI_GAME->demolish_flag_(fpos);
        }
        return;
      }
    }
    MapPos pos = AI_CURSOR_POS(player);
    if (MAP_HAS_FLAG(pos)) {
      AI_GAME->send_geologist(AI_FLAG(MAP_OBJ_INDEX(pos)));
    }
    return;
  }

  /* Building: raise the threshold for the next one. */
  unsigned int land = player->total_land_area >= 0x1000 ? 0xfff :
                                                  W(player->total_land_area);
  ai->build_threshold = sadd(ai->build_threshold, W(0x3000 - 2 * land));

  AI_GAME->build_building(
    AI_CURSOR_POS(player),
    static_cast<Building::Type>(ai_game.build_building_type), player);
  move_cursor(player, 1, 1);
  if (AI::Road::pull_roads_through_flag(player) >= 0) {
    ai->u_1ba = 70;
    ai->u_19c = 12;
    ai->u_19e = 0;
  }
  ai->u_1a4 = 0;
  player->build &= ~BIT(4);
  if (AI::Road::build_road(player) < 0) {
    MapPos pos = MAP_POS((ai->cursor_col - 1) & AI_MAP->get_col_mask(),
                         (ai->cursor_row - 1) & AI_MAP->get_row_mask());
    /* build_building may have refused. */
    if (is_building_obj(MAP_OBJ(pos))) {
      AI_GAME->demolish_building_(pos);
    }
  }
}

/* Slots 2, 6, 10, 11, 14 of update (0x28f66): compute the build
   wants and build the most wanted type. */
void
AI::Core::want_and_build(Player *player) {
  PlayerAI *ai = &player->ai;

  if (check_construction_limit(player) < 0) {
    /* Original bug: the stale want of type 25 is tested here. */
    ai->build_want[24] = 0;
    AI::Want::want_type25(player);
    if (ai->build_want[24] != 0) {
      ai_game.build_building_type = 25;
      build_building(player);
      return;
    }
    AI::Want::want_castle(player);
    if (W(ai->build_want[23]) < 10000) {
      scan_sites(player);
      return;
    }
    ai_game.build_building_type = 24;
    build_building(player);
    return;
  }

  /* Original bug: only the first 24 words are cleared (0x28f9e), so
     the want of type 25 stays set and wins every later choice. */
  for (int i = 0; i < 25; i++) ai->build_want[i] = 0;

  if (BIT_TEST(player->emergency_flags, 6)) {
    if (!BIT_TEST(player->emergency_flags, 3) &&
        W(player->lumberjack_index) == 0) {
      AI::Want::want_lumberjack(player);
    }
    if (!BIT_TEST(player->emergency_flags, 4) &&
        W(player->sawmill_index) == 0) {
      AI::Want::want_sawmill(player);
    }
    if (!BIT_TEST(player->emergency_flags, 5) &&
        W(player->stonecutter_index) == 0) {
      AI::Want::want_stonecutter(player);
    }
  } else {
    AI::Want::want_type25(player);
    AI::Want::want_castle(player);
    AI::Want::want_fisher(player);
    AI::Want::want_lumberjack(player);
    AI::Want::want_boatbuilder(player);
    AI::Want::want_stonecutter(player);
    AI::Want::want_stonemine(player);
    AI::Want::want_goldmine(player);
    AI::Want::want_coalmine(player);
    AI::Want::want_ironmine(player);
    AI::Want::want_forester(player);
    AI::Want::want_hut(player);
    AI::Want::want_farm(player);
    AI::Want::want_pigfarm(player);
    AI::Want::want_mill(player);
    if (W(IDLE(Serf::TypeDigger)) == 0 &&
        W(STOCK(Resource::TypeShovel)) == 0) {
      /* No leveling possible. */
      ai->build_want[Building::TypeFarm - 1] = 0;
      ai->build_want[Building::TypePigFarm - 1] = 0;
    } else {
      AI::Want::want_stock(player);
      AI::Want::want_butcher(player);
      AI::Want::want_baker(player);
      AI::Want::want_sawmill(player);
      AI::Want::want_steelsmelter(player);
      AI::Want::want_toolmaker(player);
      AI::Want::want_weaponsmith(player);
      AI::Want::want_tower(player);
      AI::Want::want_fortress(player);
      AI::Want::want_goldsmelter(player);
    }
  }

  unsigned int best = 0;
  int type = -1;
  for (int t = 1; t <= 25; t++) {
    if (W(ai->build_want[t - 1]) > best) {
      best = W(ai->build_want[t - 1]);
      type = t;
    }
  }
  if (type < 0) return;
  if (best < W(ai->build_threshold)) return;

  ai_game.build_building_type = type;
  ai->build_damp[type - 1] = W(ai->build_damp[type - 1]) >> 1;
  build_building(player);
}

/* Road::find_flag_connection on the flag at pos (the original passes
   it in A0 and D0/D1). budget is the caller's D7: 0 when a road was
   built, -600 on failure, unchanged if the flag has all paths. */
void
AI::Core::find_flag_connection_at(Player *player, MapPos pos,
                                  int *budget) {
  AI_SET_CURSOR(player, pos);
  int r = AI::Road::find_flag_connection(player);
  if (r == 0) {
    *budget = 0;
  } else if (r < 0) {
    *budget = static_cast<int16_t>(W(*budget - 600));
  }
}

int
AI::Core::asr1(int v) {
  return v < 0 ? -((1 - v) / 2) : v / 2;
}

/* Slots 7 and 15 of update (0x290b0): connect the pending
   positions (conquered buildings) to the own road network, then try
   to connect own flags found by a linear map scan. */
void
AI::Core::connect_roads(Player *player) {
  PlayerAI *ai = &player->ai;
  int budget = 1000;  /* D7 */
  int cols = AI_MAP->get_cols();
  int rows = AI_MAP->get_rows();

  for (int e = 0; e < 8; e++) {
    int *pcol = pending(player, e, 0);
    int *prow = pending(player, e, 1);
    if (*pcol & 0x8000) break;

    int ecol = *pcol & AI_MAP->get_col_mask();
    int erow = *prow & AI_MAP->get_row_mask();
    MapPos epos = MAP_POS(ecol, erow);

    /* Nearest own flag on the rings 1..16. */
    MapPos pos = epos;
    int ring = -1;
    for (int r = 0; r < 16 && ring < 0; r++) {
      pos = MAP_MOVE_DOWN_RIGHT(pos);
      for (int d = DirectionUp; d >= DirectionRight && ring < 0; d--) {
        for (int s = 0; s <= r; s++) {
          pos = MAP_MOVE(pos, d);
          if (MAP_HAS_FLAG(pos) && owner_bits(pos) == own_bits(player)) {
            ring = r;
            break;
          }
        }
      }
    }

    if (ring < 0) {
      budget = static_cast<int16_t>(W(budget - 50));
    } else if (ring < 8) {
      ai->cursor_col = ecol;
      ai->cursor_row = erow;
      road_params(player, 0, 0, 12, 0, -1);
      player->build &= ~BIT(4);
      AI::Road::build_road(player);
      budget = -1;
    } else {
      /* Flag halfway to the found flag. */
      int dc = (MAP_POS_COL(pos) - ecol) & AI_MAP->get_col_mask();
      if (dc >= cols / 2) dc -= cols;
      int dr = (MAP_POS_ROW(pos) - erow) & AI_MAP->get_row_mask();
      if (dr >= rows / 2) dr -= rows;
      MapPos mid = MAP_POS((ecol + asr1(dc)) & AI_MAP->get_col_mask(),
                           (erow + asr1(dr)) & AI_MAP->get_row_mask());

      bool built = false;
      for (int i = 0; i < 37; i++) {
        MapPos p = MAP_POS_ADD_SPIRALLY(mid, i);
        AI_SET_CURSOR(player, p);
        determine_map_cursor_type(player);
        if (ai->map_cursor_type < AI_CURSOR_PATH ||
            ai->panel_btn_type < AI_CAN_BUILD_FLAG ||
            (player->build & BIT(1))) {
          continue;
        }

        AI_GAME->build_flag(p, player);
        ai->cursor_col = ecol;
        ai->cursor_row = erow;
        road_params(player, 0, 0, 12, 0, -1);
        player->build &= ~BIT(4);
        AI::Road::build_road(player);
        if (MAP_HAS_FLAG(p)) {
          int dummy = budget;
          find_flag_connection_at(player, p, &dummy);
        }
        budget = -1;
        built = true;
        break;
      }
      if (!built) budget = static_cast<int16_t>(W(budget - 50));
    }

    *pcol = -1;
    *prow = -1;
  }

  if (budget < 0) return;

  if (W(IDLE(Serf::TypeTransporter) + IDLE(Serf::TypeGeneric)) < 2) {
    scan_sites(player);
    return;
  }

  /* Linear scan of budget + 1 positions from ptr+0x1a0. */
  MapPos pos = static_cast<MapPos>(ai->u_1a0) & (AI_MAP->get_size() - 1);
  do {
    if (MAP_HAS_FLAG(pos) && owner_bits(pos) == own_bits(player)) {
      find_flag_connection_at(player, pos, &budget);
    }
    pos = (pos + 1) & (AI_MAP->get_size() - 1);
    budget = static_cast<int16_t>(W(budget - 1));
  } while (budget >= 0);
  ai->u_1a0 = pos;
}


/* ---- Phase machine ---- */

/* ai_update @0x28ee2: one step of the computer player. */
void
AI::Core::update(Player *player) {
  PlayerAI *ai = &player->ai;

  if (AI_GAME->game_speed == 0) return;

  switch (ai->phase) {
    case 0: place_castle(player); return;
    case 1: wait(player); return;
    case 2: break;
    default: return;
  }

  ai->counter = W(ai->counter + 1);
  if (ai->counter & 7) {
    scan_sites(player);
    return;
  }

  switch ((ai->counter & 0x78) >> 3) {
    case 0: update_settings(player); break;
    case 1: case 5: case 9: case 13:
      update_building_stats(player);
      break;
    case 2: case 6: case 10: case 11: case 14:
      want_and_build(player);
      break;
    case 3: AI::Manage::manage_buildings(player); break;
    case 4: case 12: AI::Manage::attack(player); break;
    case 7: case 15: connect_roads(player); break;
    case 8: AI::Manage::validate_locations(player); break;
  }
}
