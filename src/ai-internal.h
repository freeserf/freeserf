/*
 * ai-internal.h - Computer player of the Amiga original, shared parts
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

/* The AI modules follow the original closely; these macros give them the
   original's view of the map and the game objects on top of the game
   classes. */

#ifndef SRC_AI_INTERNAL_H_
#define SRC_AI_INTERNAL_H_

#include <cstdint>

#include "src/ai.h"
#include "src/game.h"
#include "src/player.h"
#include "src/flag.h"
#include "src/building.h"
#include "src/serf.h"
#include "src/inventory.h"
#include "src/map.h"

/* Values of PlayerAI::map_cursor_type (= Interface::CursorType). */
#define AI_CURSOR_NONE            0
#define AI_CURSOR_FLAG            1
#define AI_CURSOR_REMOVABLE_FLAG  2
#define AI_CURSOR_BUILDING        3
#define AI_CURSOR_PATH            4
#define AI_CURSOR_CLEAR_BY_FLAG   5
#define AI_CURSOR_CLEAR_BY_PATH   6
#define AI_CURSOR_CLEAR           7

/* Values of PlayerAI::panel_btn_type (= Interface::BuildPossibility). */
#define AI_CAN_BUILD_NONE    0
#define AI_CAN_BUILD_FLAG    1
#define AI_CAN_BUILD_MINE    2
#define AI_CAN_BUILD_SMALL   3
#define AI_CAN_BUILD_LARGE   4
#define AI_CAN_BUILD_CASTLE  5

/* Space taken by a map object, in the original's encoding (table
   @0x1f42). An open space allows constructions, a filled space (trees)
   roads but no constructions, an impassable space (stones, water
   objects, fields and seeds) neither. */
typedef enum AISpace {
  AI_SPACE_OPEN = 0,
  AI_SPACE_FILLED,
  AI_SPACE_IMPASSABLE,
  AI_SPACE_FLAG,
  AI_SPACE_SMALL_BUILDING,
  AI_SPACE_LARGE_BUILDING,
  AI_SPACE_CASTLE,
  AI_SPACE_INVALID = 0xff  /* object 127 */
} AISpace;

extern const AISpace ai_space_from_obj[128];

/* The game, map and AI work fields of the current AI call. */
#define AI_GAME   (AI::game)
#define AI_MAP    (AI::game->get_map())
#define ai_game   (AI::game->get_ai_game())

/* Original's "blocked" bit (paths bit 6): lake water, the impassable
   objects the map generator keeps, and buildings. The game classes do
   not store it, so it is derived from those. */
bool ai_map_blocked(MapPos pos);

/* Map positions. */
#define MAP_POS(x, y)            (AI_MAP->pos((x), (y)))
#define MAP_POS_COL(pos)         (AI_MAP->pos_col(pos))
#define MAP_POS_ROW(pos)         (AI_MAP->pos_row(pos))
#define MAP_POS_ADD(pos, off)    (AI_MAP->pos_add((pos), (off)))
#define MAP_POS_ADD_SPIRALLY(pos, i)  (AI_MAP->pos_add_spirally((pos), (i)))
#define MAP_MOVE(pos, dir)       (AI_MAP->move((pos), (Direction)(dir)))
#define MAP_MOVE_RIGHT(pos)      (AI_MAP->move_right(pos))
#define MAP_MOVE_DOWN_RIGHT(pos) (AI_MAP->move_down_right(pos))
#define MAP_MOVE_DOWN(pos)       (AI_MAP->move_down(pos))
#define MAP_MOVE_LEFT(pos)       (AI_MAP->move_left(pos))
#define MAP_MOVE_UP_LEFT(pos)    (AI_MAP->move_up_left(pos))
#define MAP_MOVE_UP(pos)         (AI_MAP->move_up(pos))

/* Map tiles. */
#define MAP_OBJ(pos)             (AI_MAP->get_obj(pos))
#define MAP_OBJ_INDEX(pos)       (AI_MAP->get_obj_index(pos))
#define MAP_PATHS(pos)           (AI_MAP->paths(pos))
#define MAP_HAS_FLAG(pos)        (AI_MAP->has_flag(pos))
#define MAP_HAS_OWNER(pos)       (AI_MAP->has_owner(pos))
#define MAP_OWNER(pos)           (AI_MAP->get_owner(pos))
#define MAP_HEIGHT(pos)          (AI_MAP->get_height(pos))
#define MAP_IDLE_SERF(pos)       (AI_MAP->get_idle_serf(pos))
#define MAP_SERF_INDEX(pos)      (AI_MAP->get_serf_index(pos))
#define MAP_TYPE_UP(pos) \
  (static_cast<unsigned int>(AI_MAP->type_up(pos)))
#define MAP_TYPE_DOWN(pos) \
  (static_cast<unsigned int>(AI_MAP->type_down(pos)))
#define MAP_WATER_MARK(pos) \
  (MAP_TYPE_UP(pos) < 4 || MAP_TYPE_DOWN(pos) < 4)
#define MAP_IN_WATER(pos)        (AI_MAP->is_in_water(pos))
#define MAP_RES_TYPE(pos)        (AI_MAP->get_res_type(pos))
#define MAP_RES_FISH(pos)        (AI_MAP->get_res_fish(pos))
#define MAP_BLOCKED(pos)         (ai_map_blocked(pos))
#define MAP_SPACE(pos)           (ai_space_from_obj[MAP_OBJ(pos)])

/* Game objects by index; index 0 is never allocated. */
#define AI_BUILDING(i)           (AI_GAME->get_building(i))
#define AI_FLAG(i)               (AI_GAME->get_flag(i))
#define AI_SERF(i)               (AI_GAME->get_serf(i))
#define AI_INVENTORY(i)          (AI_GAME->get_inventory(i))
#define BUILDING_ALLOCATED(i)    ((i) != 0 && AI_BUILDING(i) != nullptr)
#define SERF_ALLOCATED(i)        ((i) != 0 && AI_SERF(i) != nullptr)
#define INVENTORY_ALLOCATED(i)   (AI_INVENTORY(i) != nullptr)

/* Buildings. */
#define BUILDING_TYPE(b)         ((b)->get_type())
#define BUILDING_PLAYER(b)       (static_cast<int>((b)->get_owner()))
#define BUILDING_IS_DONE(b)      ((b)->is_done())
#define BUILDING_IS_BURNING(b)   ((b)->is_burning())
#define BUILDING_IS_ACTIVE(b)    ((b)->is_active())
#define BUILDING_HAS_SERF(b)     ((b)->has_serf())
/* The military state: higher values are closer to the enemy. */
#define BUILDING_STATE(b)        (static_cast<int>((b)->get_threat_level()))

/* Flags. */
#define FLAG_PATHS(f)            ((f)->path_con & 0x3f)
#define FLAG_HAS_PATH(f, d)      ((f)->has_path((Direction)(d)))
#define FLAG_IS_WATER_PATH(f, d) ((f)->is_water_path((Direction)(d)))
#define FLAG_HAS_BUILDING(f)     ((f)->has_building())
#define FLAG_HAS_INVENTORY(f)    ((f)->has_inventory())
#define FLAG_ACCEPTS_SERFS(f)    ((f)->accepts_serfs())
#define FLAG_ACCEPTS_RESOURCES(f)  ((f)->accepts_resources())
#define FLAG_SERF_REQUESTED(f, d)  ((f)->serf_requested((Direction)(d)))

/* Serfs. */
#define SERF_TYPE(s)             ((s)->get_type())
#define SERF_PLAYER(s)           (static_cast<int>((s)->get_owner()))
#define SERF_INDEX(s)            (static_cast<int>((s)->get_index()))

/* Players. */
#define PLAYER_NUM(p)            (static_cast<int>((p)->get_index()))
#define PLAYER_IS_ACTIVE(p)      ((p) != nullptr)
#define PLAYER_IN_GAME(p)        ((p) != nullptr && (p)->is_in_game())
#define PLAYER_IS_AI(p)          ((p)->is_ai())
#define PLAYER_HAS_CASTLE(p)     ((p)->has_castle())
#define PLAYER_ALLOW_FLAG(p)     ((p)->allow_flag())

/* Cursor of the computer player. */
#define AI_CURSOR_POS(player) \
  MAP_POS((player)->ai.cursor_col, (player)->ai.cursor_row)
#define AI_SET_CURSOR(player, pos) do { \
    (player)->ai.cursor_col = MAP_POS_COL(pos); \
    (player)->ai.cursor_row = MAP_POS_ROW(pos); \
  } while (0)

#endif  // SRC_AI_INTERNAL_H_
