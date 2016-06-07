/*
 * pathfinder.cc - Path finder functions
 *
 * Copyright (C) 2013-2014  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/pathfinder.h"

#include <cstdlib>
#include <list>
#include <vector>
#include <algorithm>

#include "src/smart_ptr.h"

class search_node_t;

typedef smart_ptr_t<search_node_t> search_node_p;

class search_node_t {
 public:
  map_pos_t pos;
  unsigned int g_score;
  unsigned int f_score;
  search_node_p parent;
  dir_t dir;
};

static bool
search_node_less(const search_node_p &left, const search_node_p &right) {
  // A search node is considered less than the other if
  // it has a larger f-score. This means that in the max-heap
  // the lower score will go to the top.
  return left->f_score > right->f_score;
}

static const unsigned int walk_cost[] = { 255, 319, 383, 447, 511 };

static unsigned int
heuristic_cost(map_t *map, map_pos_t start, map_pos_t end) {
  /* Calculate distance to target. */
  int dist_col = (map->pos_col(start) - map->pos_col(end)) &
                 map->get_col_mask();
  if (dist_col >= static_cast<int>(map->get_cols()/2.0)) {
    dist_col -= map->get_cols();
  }

  int dist_row = (map->pos_row(start) - map->pos_row(end)) &
                 map->get_row_mask();
  if (dist_row >= static_cast<int>(map->get_rows()/2.0)) {
    dist_row -= map->get_rows();
  }

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
actual_cost(map_t *map, map_pos_t pos, dir_t dir) {
  map_pos_t other_pos = map->move(pos, dir);
  int h_diff = abs(static_cast<int>(map->get_height(pos)) -
                   static_cast<int>(map->get_height(other_pos)));
  return walk_cost[h_diff];
}

/* Find the shortest path from start to end (using A*) considering that
   the walking time for a serf walking in any direction of the path
   should be minimized. Returns a malloc'ed array of directions and
   the size of this array in length. */
road_t
pathfinder_map(map_t *map, map_pos_t start, map_pos_t end) {
  // Unfortunately the STL priority_queue cannot be used since we
  // would need access to the underlying sequence to determine if
  // a node is already in the open list. We keep instead open as
  // a vector and apply std::pop_heap and std::push_heap to keep
  // it heapified.
  std::vector<search_node_p> open;
  std::list<search_node_p> closed;

  /* Create start node */
  search_node_p node(new search_node_t);
  node->pos = end;
  node->g_score = 0;
  node->f_score = heuristic_cost(map, start, end);

  open.push_back(node);

  while (!open.empty()) {
    std::pop_heap(open.begin(), open.end(), search_node_less);
    node = open.back();
    open.pop_back();

    if (node->pos == start) {
      /* Construct solution */
      road_t solution;
      solution.start(start);

      std::list<dir_t> dirs;
      while (node->parent != NULL) {
        dir_t dir = node->dir;
        solution.extand(DIR_REVERSE(dir));
        node = node->parent;
      }

      return solution;
    }

    /* Put current node on closed list. */
    closed.push_front(node);

    for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
      map_pos_t new_pos = map->move(node->pos, (dir_t)d);
      unsigned int cost = actual_cost(map, node->pos, static_cast<dir_t>(d));

      /* Check if neighbour is valid. */
      if (!map->is_road_segment_valid(node->pos, static_cast<dir_t>(d)) ||
          (map->get_obj(new_pos) == MAP_OBJ_FLAG && new_pos != start)) {
        continue;
      }

      /* Check if neighbour is in closed list. */
      bool in_closed = false;
      for (std::list<search_node_p>::iterator it = closed.begin();
           it != closed.end(); ++it) {
        if ((*it)->pos == new_pos) {
          in_closed = true;
          break;
        }
      }

      if (in_closed) continue;

      /* See if neighbour is already in open list. */
      bool in_open = false;
      for (std::vector<search_node_p>::iterator it = open.begin();
           it != open.end(); ++it) {
        search_node_p n = *it;
        if (n->pos == new_pos) {
          in_open = true;
          if (n->g_score >= node->g_score + cost) {
            n->g_score = node->g_score + cost;
            n->f_score = n->g_score + heuristic_cost(map, new_pos, start);
            n->parent = node;
            n->dir = static_cast<dir_t>(d);

            // Move element to the back and heapify
            iter_swap(it, open.rbegin());
            std::make_heap(open.begin(), open.end(), search_node_less);
          }
          break;
        }
      }

      /* If not found in the open set, create a new node. */
      if (!in_open) {
        search_node_p new_node(new search_node_t);

        new_node->pos = new_pos;
        new_node->g_score = node->g_score + cost;
        new_node->f_score = new_node->g_score +
                            heuristic_cost(map, new_pos, start);
        new_node->parent = node;
        new_node->dir = static_cast<dir_t>(d);

        open.push_back(new_node);
        std::push_heap(open.begin(), open.end(), search_node_less);
      }
    }
  }

  return road_t();
}
