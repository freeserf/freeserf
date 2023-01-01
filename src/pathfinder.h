/*
 * pathfinder.h - Path finder functions
 *
 * Copyright (C) 2012  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef SRC_PATHFINDER_H_
#define SRC_PATHFINDER_H_

#include <memory>       // to satisfy cpplint
#include <algorithm>       // to satisfy cpplint

#include "src/map.h"

class SearchNode;

typedef std::shared_ptr<SearchNode> PSearchNode;

class SearchNode {
 public:
  PSearchNode parent;
  unsigned int g_score;
  unsigned int f_score;
  MapPos pos;
  Direction dir;

  SearchNode()
    : g_score(0)
    , f_score(0)
    , pos(0)
    , dir(DirectionNone) {
  }
};

static bool
search_node_less(const PSearchNode &left, const PSearchNode &right) {
  // A search node is considered less than the other if
  // it has a larger f-score. This means that in the max-heap
  // the lower score will go to the top.
  return left->f_score > right->f_score;
}

static const unsigned int walk_cost[] = { 255, 319, 383, 447, 511 };

static unsigned int
heuristic_cost(Map *map, MapPos start, MapPos end) {
  // Calculate distance to target.
  int dist_col = map->dist_x(start, end);
  int dist_row = map->dist_y(start, end);

  int h_diff = abs(static_cast<int>(map->get_height(start)) -
                   static_cast<int>(map->get_height(end)));
  int dist = 0;

  if ((dist_col > 0 && dist_row > 0) ||
      (dist_col < 0 && dist_row < 0)) {
    dist = std::max(abs(dist_col), abs(dist_row));
  } else {
    dist = abs(dist_col) + abs(dist_row);
  }

  return dist > 0 ? dist*walk_cost[h_diff/dist] : 0;
}

static unsigned int
actual_cost(Map *map, MapPos pos, Direction dir) {
  MapPos other_pos = map->move(pos, dir);
  int h_diff = abs(static_cast<int>(map->get_height(pos)) -
                   static_cast<int>(map->get_height(other_pos)));
  return walk_cost[h_diff];
}

Road pathfinder_map(Map *map, MapPos start, MapPos end, const Road *building_road = nullptr);
//Road pathfinder_freewalking_serf(Map *map, MapPos start, MapPos end, const Road *building_road = nullptr);
Road pathfinder_freewalking_serf(Map *map, MapPos start, MapPos end);

#endif  // SRC_PATHFINDER_H_
