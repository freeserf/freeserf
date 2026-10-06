/*
 * ai-road.cc - Computer player: road building
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

/* The road builder works on a 19x19 grid of map positions centered on
   the AI cursor (grid index = row * 19 + col, center 180). Each cell
   holds a byte:
     0x00..0x3f  cost of walking over the cell,
     0x40..0x4f  target: a flag (category 0),
     0x50..0x7e  target: a path tile or shore tile where a flag can
                 be built (category 1),
     0x7f        blocked, 0xff unreachable / not evaluated.
   A layered shortest path search (at most 21 steps) records the best
   path to each target; the roads are then built in order of cost. */

#include "src/ai-road.h"

#include <cstring>

#include "src/ai-core.h"
#include "src/misc.h"
#include "src/log.h"

#define GRID_SIZE    19
#define GRID_CELLS   (GRID_SIZE*GRID_SIZE)
#define GRID_CENTER  180
#define MAX_TARGETS  63

/* Amiga player->build bit 4: build water roads. */
#define AI_BUILD_WATER(player)  ((player)->water_roads)

/* Grid index offsets of the six directions (right, down right,
   down, left, up left, up). */
static const int grid_dir_offset[6] = { 1, 20, 19, -1, -20, -19 };

/* Extra cost around own farms (7x7, @0x2b152) and foresters
   (13x13, @0x2b183). */
static const uint8_t cost_farm[7*7] = {
  7, 7, 7, 7, 0, 0, 0,
  7, 8, 8, 8, 7, 0, 0,
  7, 8, 9, 9, 8, 7, 0,
  7, 8, 9, 9, 9, 8, 7,
  0, 7, 8, 9, 9, 8, 7,
  0, 0, 7, 8, 8, 8, 7,
  0, 0, 0, 7, 7, 7, 7
};

static const uint8_t cost_forester[13*13] = {
  4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0,
  4, 5, 5, 5, 5, 5, 5, 4, 0, 0, 0, 0, 0,
  4, 5, 6, 6, 6, 6, 6, 5, 4, 0, 0, 0, 0,
  4, 5, 6, 7, 7, 7, 7, 6, 5, 4, 0, 0, 0,
  4, 5, 6, 7, 8, 8, 8, 7, 6, 5, 4, 0, 0,
  4, 5, 6, 7, 8, 9, 9, 8, 7, 6, 5, 4, 0,
  4, 5, 6, 7, 8, 9, 9, 9, 8, 7, 6, 5, 4,
  0, 4, 5, 6, 7, 8, 9, 9, 8, 7, 6, 5, 4,
  0, 0, 4, 5, 6, 7, 8, 8, 8, 7, 6, 5, 4,
  0, 0, 0, 4, 5, 6, 7, 7, 7, 7, 6, 5, 4,
  0, 0, 0, 0, 4, 5, 6, 6, 6, 6, 6, 5, 4,
  0, 0, 0, 0, 0, 4, 5, 5, 5, 5, 5, 5, 4,
  0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4
};

/* Work tables of ai_build_road (Amiga: inside game->land_influence,
   the cost grid itself is ai_game.land_influence[0..360]). */
static MapPos grid_pos[GRID_CELLS];             /* +0x16c */
static uint16_t grid_best[GRID_CELLS];          /* +0x710 */
static uint64_t target_path[MAX_TARGETS+1];     /* +0x9e2 */
static uint16_t target_cost[MAX_TARGETS+1];     /* +0xbda */

/* Search queue entry (Amiga 12 bytes at game+0x120/0x124). The path is
   a list of 3-bit direction codes (dir + 1), first step in the most
   significant used group. */
typedef struct RoadQueue {
  uint16_t cost;
  int index;
  uint64_t path;
} RoadQueue;

#define ROAD_QUEUE_SIZE  (GRID_CELLS*6)
static RoadQueue road_queue[2][ROAD_QUEUE_SIZE];

#define FLAG_QUEUE_SIZE  1024
static Flag *flag_queue[2][FLAG_QUEUE_SIZE];


/* Move the AI cursor to pos and evaluate it (determine_map_cursor_type
   @0x19368 with ptr+0xfc/0xfe). */
void
AI::Road::cursor_at(Player *player, MapPos pos) {
  AI_SET_CURSOR(player, pos);
  AI::Core::determine_map_cursor_type(player);
}

/* Amiga obj bit 7 (map_mark_water_tiles): the vertex' own up or down
   triangle is water. legacy keeps no marker. */
int
AI::Road::water_marker(MapPos pos) {
  return MAP_TYPE_UP(pos) < 4 || MAP_TYPE_DOWN(pos) < 4;
}

/* Amiga paths bit 6 ("blocked": lake water, kept impassable objects,
   buildings). */
int
AI::Road::tile_blocked(MapPos pos) {
  return MAP_BLOCKED(pos);
}

int
AI::Road::owned_by(MapPos pos, const Player *player) {
  return MAP_HAS_OWNER(pos) &&
         static_cast<int>(MAP_OWNER(pos)) == PLAYER_NUM(player);
}

/* ai_road_tile_cost @0x2b4d8: evaluate grid cell index at map pos in
   ring ring. Clears *found when the cell became passable. */
void
AI::Road::road_tile_cost(Player *player, int index, MapPos pos, int ring,
                         int *found) {
  uint8_t *grid = ai_game.land_influence;

  /* Only cells next to an already passable cell. */
  if (grid[index-1] >= 0x40 && grid[index-19] >= 0x40 &&
      grid[index-20] >= 0x40 && grid[index+1] >= 0x40 &&
      grid[index+19] >= 0x40 && grid[index+20] >= 0x40) {
    goto out;
  }

  if (!owned_by(pos, player)) goto out;

  if (MAP_HAS_FLAG(pos)) {
    /* Flag: a category 0 target. */
    if (static_cast<int16_t>(player->ai.u_1a4) < 0) {
      /* Connecting a flag (ai_find_flag_connection): skip
         flags of its own network; with u_1a4 = -2 only
         those found in the first three search layers. */
      Flag *flag = AI_FLAG(MAP_OBJ_INDEX(pos));
      if (static_cast<int16_t>(player->ai.u_1a4) == -1 ||
          flag->search_dir == 0) {
        if ((flag->search_num & 0xffff) ==
            (player->ai.u_1a6 & 0xffff)) {
          goto out;
        }
      }
    }

    if (ai_game.g_24a == 0x50) goto out;
    grid[index] = ai_game.g_24a;
    ai_game.g_24a += 1;
    goto out;
  }

  if (AI_BUILD_WATER(player)) {
    /* Water road. */
    if (MAP_PATHS(pos) != 0) goto out;
    if (MAP_OBJ(pos) != 0) goto out;

    /* Original bug: the test of the UP neighbour's up triangle
       (@0x2b57a) loads the type into D7 and tests the stale D6,
       so it always passes. Test all six triangles. */
    if (MAP_IN_WATER(pos)) {
      grid[index] = 2;
      *found = 0;
      goto out;
    }

    /* Shore: a category 1 target where a flag can be built. */
    if (ring < 3) goto out;
    cursor_at(player, pos);
    if ((player->ai.map_cursor_type == AI_CURSOR_CLEAR ||
         player->ai.map_cursor_type == AI_CURSOR_PATH) &&
        player->ai.panel_btn_type == AI_CAN_BUILD_FLAG &&
        PLAYER_ALLOW_FLAG(player) &&
        ai_game.g_24c != 0x7f) {
      grid[index] = ai_game.g_24c;
      ai_game.g_24c += 1;
    }
    goto out;
  }

  if (tile_blocked(pos)) goto out;
  if (MAP_OBJ(pos) != 0 &&
      MAP_SPACE(pos) >= AI_SPACE_IMPASSABLE) {
    goto out;
  }

  if (MAP_PATHS(pos) != 0) {
    /* Path: a category 1 target (a flag splits the road). */
    if (static_cast<int16_t>(player->ai.u_1a4) < 0) goto out;
    cursor_at(player, pos);
    if (player->ai.map_cursor_type == AI_CURSOR_PATH &&
        player->ai.panel_btn_type == AI_CAN_BUILD_FLAG &&
        PLAYER_ALLOW_FLAG(player) &&
        ai_game.g_24c != 0x7f) {
      grid[index] = ai_game.g_24c;
      ai_game.g_24c += 1;
    }
    goto out;
  }

  /* Free tile. */
  cursor_at(player, pos);
  if (player->ai.map_cursor_type >= AI_CURSOR_CLEAR_BY_FLAG &&
      player->ai.panel_btn_type >= AI_CAN_BUILD_MINE) {
    /* Keep building sites free (mine and large sites more). */
    if (player->ai.panel_btn_type >= AI_CAN_BUILD_LARGE) {
      grid[index] = 20;
    } else if (player->ai.panel_btn_type >= AI_CAN_BUILD_SMALL) {
      grid[index] = 3;
    } else {
      grid[index] = 20;
    }
  } else if (water_marker(pos) ||
             water_marker(MAP_MOVE_UP_LEFT(pos))) {
    grid[index] = 4;
  } else if (owned_by(MAP_MOVE_UP_LEFT(pos), player) &&
             owned_by(MAP_MOVE_UP(pos), player) &&
             owned_by(MAP_MOVE_RIGHT(pos), player) &&
             owned_by(MAP_MOVE_DOWN_RIGHT(pos), player) &&
             owned_by(MAP_MOVE_DOWN(pos), player) &&
             owned_by(MAP_MOVE_LEFT(pos), player)) {
    /* Inside own land. */
    grid[index] = 0;
  } else {
    /* At the border. */
    grid[index] = 3;
  }
  *found = 0;

out:
  grid_pos[index] = pos;
}

/* Number of steps of a path. */
int
AI::Road::road_path_length(uint64_t path) {
  int length = 0;
  while (path != 0) {
    path >>= 3;
    length += 1;
  }
  return length;
}

/* ai_road_path_step @0x2b44a: decode the direction list. */
int
AI::Road::road_path_dirs(uint64_t path, Direction dirs[]) {
  int length = road_path_length(path);
  for (int i = 0; i < length; i++) {
    dirs[i] = static_cast<Direction>(((path >> (3*(length-1-i))) & 7) - 1);
  }
  return length;
}

/* ai_build_road_along_path @0x2b22c: build the road from the cursor
   along path. category (game+0x248) is 1 if the end is a path or
   shore tile that needs a flag first. The original writes the path
   bits itself (ai_road_finish @0x2b424 only redraws the tiles,
   redraw_map_pos_panels @0x1d20e); here Game::build_road builds the
   road. Returns < 0 on failure. */
int
AI::Road::build_road_along_path(Player *player, uint64_t path,
                                int category) {
  Direction dirs[22];
  int length = road_path_dirs(path, dirs);
  MapPos source = AI_CURSOR_POS(player);

  /* The tiles between the ends must be free of paths. */
  MapPos pos = source;
  for (int i = 0; i < length; i++) {
    pos = MAP_MOVE(pos, dirs[i]);
    if (i < length-1 && MAP_PATHS(pos) != 0) return -1;
  }
  MapPos dest = pos;

  /* Not in the original: the game checks the segments (objects, owner,
     water/land changes) more strictly than the AI cost grid. Check
     the part that does not depend on the end flag before anything
     is changed. */
  if (category == 0) {
    ::Road road;
    road.start(source);
    for (int i = 0; i < length; i++) road.extend(dirs[i]);
    if (AI_GAME->can_build_road(road, player, nullptr, nullptr) <= 0) {
      return -1;
    }
  } else if (length > 1) {
    ::Road road;
    road.start(source);
    for (int i = 0; i < length-1; i++) road.extend(dirs[i]);
    if (AI_GAME->can_build_road(road, player, nullptr, nullptr) <= 0) {
      return -1;
    }
  }

  if (category != 0) {
    /* Place the end flag. */
    ai_game.g_24a = player->ai.cursor_col;
    ai_game.g_24c = player->ai.cursor_row;
    cursor_at(player, dest);

    int ok = 0;
    if (player->ai.map_cursor_type == AI_CURSOR_PATH) {
      if (player->ai.panel_btn_type >= AI_CAN_BUILD_FLAG &&
          PLAYER_ALLOW_FLAG(player)) {
        AI_GAME->build_flag(dest, player);
        ok = 1;
      }
    } else if (AI_BUILD_WATER(player) &&
               player->ai.map_cursor_type == AI_CURSOR_CLEAR &&
               player->ai.panel_btn_type >= AI_CAN_BUILD_FLAG &&
               PLAYER_ALLOW_FLAG(player)) {
      AI_GAME->build_flag(dest, player);
      pull_roads_through_flag(player);
      ok = 1;
    }

    player->ai.cursor_col = ai_game.g_24a;
    player->ai.cursor_row = ai_game.g_24c;
    if (!ok) return -1;
  }

  /* Adjust the limits for the next roads of this call. */
  if (static_cast<int16_t>(player->ai.u_1a4) > 0) {
    player->ai.u_1a4 -= 1;
    if (player->ai.u_1a4 == 0) {
      player->ai.u_1ba = 70;
      player->ai.u_19c = 10;
    }
  } else {
    if ((player->ai.u_1ba & 0xffff) == 0) {
      int limit = (ai_game.g_24e * 2) & 0xffff;
      if (limit >= 70) limit = 70;
      player->ai.u_1ba = limit;
    }
    if ((player->ai.u_19c & 0xffff) == 0) {
      player->ai.u_19c = player->ai.u_1a8;
    }
  }

  /* The original cannot fail here; the game may still refuse the last
     segment (the end flag stays then). */
  ::Road road;
  road.start(source);
  for (int i = 0; i < length; i++) road.extend(dirs[i]);
  if (!AI_GAME->build_road(road, player)) {
    Log::Debug["ai"] << "player " << PLAYER_NUM(player)
                     << ": road from " << source << " failed";
    return -1;
  }

  return 0;
}

/* ai_build_road @0x2a8f2: build roads from the flag at the AI cursor.
   Parameters:
     u_1ba  max. cost of a road (0 = no limit; set from the first road),
     u_19c  max. average cost per step (0 = no limit),
     u_1a8  value for u_19c after the first road if it was 0,
     u_1a4  > 0: number of roads until the limits become 70/10,
            -1/-2: connecting a flag (ai_find_flag_connection, the
            flag search id is in u_1a6),
     u_19e  set to 0 when a road was built (callers set -1),
     player->water_roads (build bit 4): water roads.
   Returns u_19e (< 0: no road built). */
int
AI::Road::build_road(Player *player) {
  uint8_t *grid = ai_game.land_influence;

  memset(grid, 0xff, GRID_CELLS);
  /* The original keeps these tables in a scratch area shared with
     other routines; start from a defined state so that a loaded
     game continues exactly like the saved one. */
  memset(grid_pos, 0, sizeof(grid_pos));
  memset(grid_best, 0, sizeof(grid_best));
  memset(target_path, 0, sizeof(target_path));
  memset(target_cost, 0, sizeof(target_cost));

  ai_game.g_24e = player->ai.cursor_col;
  ai_game.g_246 = player->ai.cursor_row;

  MapPos pos = AI_CURSOR_POS(player);
  int index = GRID_CENTER;

  ai_game.g_24a = 0x40;
  ai_game.g_24c = 0x50;
  grid[index] = 0;
  grid_pos[index] = pos;

  /* Evaluate rings around the cursor while they have passable
     cells (at most 8 rings). */
  for (int ring = 0; ring < 8; ring++) {
    int found = -1;
    pos = MAP_MOVE_RIGHT(pos);
    index += 1;

    for (int i = 0; i <= ring; i++) {
      road_tile_cost(player, index, pos, ring, &found);
      pos = MAP_MOVE_DOWN(pos);
      index += 19;
    }
    for (int i = 0; i <= ring; i++) {
      road_tile_cost(player, index, pos, ring, &found);
      pos = MAP_MOVE_LEFT(pos);
      index -= 1;
    }
    for (int i = 0; i <= ring; i++) {
      road_tile_cost(player, index, pos, ring, &found);
      pos = MAP_MOVE_UP_LEFT(pos);
      index -= 20;
    }
    for (int i = 0; i <= ring; i++) {
      road_tile_cost(player, index, pos, ring, &found);
      pos = MAP_MOVE_UP(pos);
      index -= 19;
    }
    for (int i = 0; i <= ring; i++) {
      road_tile_cost(player, index, pos, ring, &found);
      pos = MAP_MOVE_RIGHT(pos);
      index += 1;
    }
    for (int i = 0; i <= ring; i++) {
      road_tile_cost(player, index, pos, ring, &found);
      pos = MAP_MOVE_DOWN_RIGHT(pos);
      index += 20;
    }

    if (found < 0) break;
  }

  /* The start is blocked, its neighbours (where no flag can be
     placed anyway) get 30 more. */
  grid[GRID_CENTER] = 0x7f;
  for (int d = 0; d < 6; d++) {
    static const int order[6] = { 1, 20, 19, -1, -20, -19 };
    uint8_t *v = &grid[GRID_CENTER + order[d]];
    if (*v < 0x80) {
      int c = *v + 30;
      if (c >= 0x40) c = 0x3f;
      *v = c;
    }
  }

  /* Extra cost around own foresters and farms. */
  int start_col = ai_game.g_24e;
  int start_row = ai_game.g_246;
  for (int y = -6; y < 25; y++) {
    for (int x = -6; x < 25; x++) {
      MapPos p = MAP_POS((start_col + x - 9) & AI_MAP->get_col_mask(),
                         (start_row + y - 9) & AI_MAP->get_row_mask());
      const uint8_t *table;
      int width;

      if (MAP_OBJ(p) == Map::ObjectSmallBuilding) {
        Building *b = AI_BUILDING(MAP_OBJ_INDEX(p));
        /* Original bug: compares (type<<2|player) & 0x3f
           with 0x24 and then the player, so only player 0
           ever matched. */
        if (BUILDING_TYPE(b) != Building::TypeForester ||
            BUILDING_PLAYER(b) != PLAYER_NUM(player)) {
          continue;
        }
        table = cost_forester;
        width = 13;
      } else if (MAP_OBJ(p) == Map::ObjectLargeBuilding) {
        Building *b = AI_BUILDING(MAP_OBJ_INDEX(p));
        /* Original bug: same as above (0x30). */
        if (BUILDING_TYPE(b) != Building::TypeFarm ||
            BUILDING_PLAYER(b) != PLAYER_NUM(player)) {
          continue;
        }
        table = cost_farm;
        width = 7;
      } else {
        continue;
      }

      int half = width/2;
      for (int ty = 0; ty < width; ty++) {
        int gy = y - half + ty;
        if (gy < 0 || gy >= GRID_SIZE) continue;
        for (int tx = 0; tx < width; tx++) {
          int gx = x - half + tx;
          if (gx < 0 || gx >= GRID_SIZE) continue;
          uint8_t *v = &grid[gy*GRID_SIZE + gx];
          if (*v < 0x40) {
            int c = *v + table[ty*width + tx];
            if (c >= 0x40) c = 0x3f;
            *v = c;
          }
        }
      }
    }
  }

  /* Shortest paths, one layer per step, at most 21 steps. */
  for (int i = 0; i < GRID_CELLS; i++) grid_best[i] = 0xffff;
  memset(target_cost, 0, sizeof(target_cost));

  int sel = 0;
  int count = 1;
  road_queue[0][0].cost = 0xffff;
  road_queue[0][0].index = GRID_CENTER;
  road_queue[0][0].path = 0;

  int layers = 21;
  while (1) {
    RoadQueue *in = road_queue[sel];
    RoadQueue *out = road_queue[!sel];
    int out_count = 0;

    for (int e = 0; e < count; e++) {
      int idx = in[e].index;
      if (grid_best[idx] != in[e].cost) continue;

      /* 16 bit: the start entry (0xffff) wraps to 2. */
      uint16_t cost = static_cast<uint16_t>(in[e].cost + 3);
      uint64_t path = in[e].path << 3;
      int h = MAP_HEIGHT(grid_pos[idx]);

      for (int d = 0; d < 6; d++) {
        int nb = idx + grid_dir_offset[d];
        uint8_t c = grid[nb];
        if (c >= 0x7f) continue;
        if (cost >= grid_best[nb]) continue;

        uint16_t new_cost = static_cast<uint16_t>(cost + c);
        int dh = static_cast<int>(MAP_HEIGHT(grid_pos[nb])) - h;
        if (dh < 0) dh = -dh;
        if (dh == 2) {
          new_cost += 1;
        } else if (dh == 3) {
          new_cost += 3;
        } else if (dh > 3) {
          new_cost += 8;
        }

        if (new_cost >= grid_best[nb]) continue;
        grid_best[nb] = new_cost;

        uint64_t new_path = path | (d+1);
        if (c >= 0x40) {
          target_cost[c-0x40] = new_cost;
          target_path[c-0x40] = new_path;
        } else if (out_count < ROAD_QUEUE_SIZE) {
          out[out_count].cost = new_cost;
          out[out_count].index = nb;
          out[out_count].path = new_path;
          out_count += 1;
        }
      }
    }

    sel = !sel;
    layers -= 1;
    if (layers == 0) break;
    count = out_count;
    if (count == 0) break;
  }
  ai_game.g_24a = -1;
  ai_game.g_248 = layers;

  player->ai.cursor_col = ai_game.g_24e;
  player->ai.cursor_row = ai_game.g_246;

  /* Candidates: cost minus target code, path/shore targets 1.5x. */
  uint16_t cand_cost[MAX_TARGETS];
  int cand_cat[MAX_TARGETS];
  uint64_t cand_path[MAX_TARGETS];
  int cand_count = 0;
  for (int k = 0; k < MAX_TARGETS; k++) {
    if (target_cost[k] == 0) continue;
    uint16_t cost = static_cast<uint16_t>(target_cost[k] - (0x40 + k));
    int cat = 0;
    if (0x40 + k >= 0x50) {
      cost += cost >> 1;
      cat = 1;
    }
    cand_cost[cand_count] = cost;
    cand_cat[cand_count] = cat;
    cand_path[cand_count] = target_path[k];
    cand_count += 1;
  }

  /* Build the roads, cheapest first. */
  while (1) {
    int best = -1;
    uint16_t best_cost = 0xffff;
    for (int i = 0; i < cand_count; i++) {
      if (cand_cost[i] & 0x8000) break; /* end marker */
      if (cand_cost[i] == 0) continue;
      if (cand_cost[i] < best_cost) {
        best_cost = cand_cost[i];
        best = i;
      }
    }
    if (best < 0 || (best_cost & 0x8000)) break;

    if ((player->ai.u_1ba & 0xffff) != 0 &&
        best_cost >= (player->ai.u_1ba & 0xffff)) {
      break;
    }

    if ((player->ai.u_19c & 0xffff) != 0) {
      int length = road_path_length(cand_path[best]);
      uint16_t limit = (player->ai.u_19c * length) & 0xffff;
      if (best_cost >= limit) {
        /* Too much detour. */
        cand_cost[best] = 0;
        continue;
      }
    }

    ai_game.g_24e = best_cost;
    cand_cost[best] = 0;
    ai_game.g_248 = cand_cat[best];
    if (build_road_along_path(player, cand_path[best],
                              cand_cat[best]) >= 0) {
      player->ai.u_19e = 0;
    }
  }

  return static_cast<int16_t>(player->ai.u_19e);
}

/* ai_find_flag_connection @0x29316: connect the flag at the AI cursor
   to another flag. A flag search over the land roads first looks for
   an inventory within the network (three layers, then layer by
   layer). Found: road to a flag outside the near part of the network
   (u_1a4 = -2, limits 100/20). Not found: road to a flag of another
   network (u_1a4 = -1).
   The original gets the flag in A0 and its position in D0/D1 and sets
   the cursor to it; here the caller sets the cursor to the flag.
   Returns 1 if the flag already has all six paths (nothing done, the
   caller's D7 is unchanged), 0 if a road was built (the original sets
   D7 = 0), -1 if not (the original subtracts 600 from D7). */
int
AI::Road::find_flag_connection(Player *player) {
  MapPos pos = AI_CURSOR_POS(player);
  Flag *flag = AI_FLAG(MAP_OBJ_INDEX(pos));

  if (FLAG_PATHS(flag) == 0x3f) return 1;

  FlagSearch search(AI_GAME);
  int id = search.get_id();

  int sel = 0;
  int count = 1;
  flag_queue[0][0] = flag;

  int layers = 2;
  Direction layer_dir = DirectionRight;  /* 0 */
  int found = 0;
  int connected = 0;

  while (1) {
    Flag **in = flag_queue[sel];
    Flag **out = flag_queue[!sel];
    int out_count = 0;

    for (int i = 0; i < count; i++) {
      Flag *f = in[i];
      f->search_dir = layer_dir;
      for (int d = DirectionUp; d >= DirectionRight; d--) {
        if (!BIT_TEST(f->land_paths(), d)) continue;
        Flag *other = f->other_endpoint.f[d];
        if (other->search_num == id) continue;
        if (FLAG_HAS_INVENTORY(other)) found = 1;
        other->search_num = id;
        out[out_count++] = other;
      }
      if (out_count > 0x3e2) break;
    }

    sel = !sel;
    layers -= 1;
    if (layers < 0) {
      /* Original stores 0xff; only != 0 is tested. */
      layer_dir = DirectionNone;
      if (found) {
        connected = 1;
        break;
      }
    }

    count = out_count;
    if (count == 0) break;
  }

  if (connected) {
    player->ai.u_1ba = 100;
    player->ai.u_19c = 20;
    player->ai.u_1a4 = -2;
    player->ai.u_1a6 = id;
    player->ai.u_19e = 0;
  } else {
    player->ai.u_1ba = 0;
    player->ai.u_19c = 0;
    player->ai.u_1a8 = 12;
    player->ai.u_1a4 = -1;
    player->ai.u_1a6 = id;
    player->ai.u_19e = -1;
  }

  AI_SET_CURSOR(player, pos);
  player->water_roads = false;
  if (build_road(player) < 0) return -1;
  return 0;
}

/* ai_pull_roads_through_flag @0x271d4: roads passing the flag at the
   AI cursor around a corner are led through the flag (the game part,
   flag_has_road_corner @0x270fe and the rerouting @0x2738c, is
   Game::pull_roads_through_flag). Returns 0 if a road was rerouted,
   -1 if not. */
int
AI::Road::pull_roads_through_flag(Player *player) {
  return AI_GAME->pull_roads_through_flag(AI_CURSOR_POS(player), player) ?
         0 : -1;
}
