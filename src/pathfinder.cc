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

extern "C" {
  #include "pathfinder.h"
  #include "game.h"
}

#include <cstdlib>
#include <list>
#include <vector>
#include <algorithm>

typedef struct search_node search_node_t;

struct search_node {
  map_pos_t pos;
  uint g_score;
  uint f_score;
  search_node_t *parent;
  dir_t dir;
};

static bool
search_node_less(const search_node_t* left, const search_node_t* right)
{
  // A search node is considered less than the other if
  // it has a larger f-score. This means that in the max-heap
  // the lower score will go to the top.
  return left->f_score > right->f_score;
}


static const uint walk_cost[] = { 255, 319, 383, 447, 511 };

static uint
heuristic_cost(map_pos_t start, map_pos_t end)
{
  /* Calculate distance to target. */
  int dist_col = (MAP_POS_COL(start) - MAP_POS_COL(end)) & game.map.col_mask;
  if (dist_col >= (int)(game.map.cols/2)) dist_col -= game.map.cols;

  int dist_row = (MAP_POS_ROW(start) - MAP_POS_ROW(end)) & game.map.row_mask;
  if (dist_row >= (int)(game.map.rows/2)) dist_row -= game.map.rows;

  int h_diff = abs(MAP_HEIGHT(start) - MAP_HEIGHT(end));
  int dist = 0;

  if ((dist_col > 0 && dist_row > 0) ||
      (dist_col < 0 && dist_row < 0)) {
    dist = std::max(abs(dist_col), abs(dist_row));
  } else {
    dist = abs(dist_col) + abs(dist_row);
  }

  return dist > 0 ? dist*walk_cost[h_diff/dist] : 0;
}

static uint
actual_cost(map_pos_t pos, dir_t dir)
{
  map_pos_t other_pos = MAP_MOVE(pos, dir);
  int h_diff = abs(MAP_HEIGHT(pos) - MAP_HEIGHT(other_pos));
  return walk_cost[h_diff];
}

/* Find the shortest path from start to end (using A*) considering that
   the walking time for a serf walking in any direction of the path
   should be minimized. Returns a malloc'ed array of directions and
   the size of this array in length. */
dir_t *
pathfinder_map(map_pos_t start, map_pos_t end, uint *length)
{
  // Unfortunately the STL priority_queue cannot be used since we
  // would need access to the underlying sequence to determine if
  // a node is already in the open list. We keep instead open as
  // a vector and apply std::pop_heap and std::push_heap to keep
  // it heapified.
  std::vector<search_node_t*> open;
  std::list<search_node_t*> closed;

  dir_t *solution = NULL;

  /* Create start node */
  search_node_t *node = new search_node_t;
  node->pos = start;
  node->g_score = 0;
  node->f_score = heuristic_cost(start, end);
  node->parent = NULL;

  open.push_back(node);

  while (!open.empty()) {
    std::pop_heap(open.begin(), open.end(), search_node_less);
    node = open.back();
    open.pop_back();

    if (node->pos == end) {
      /* Construct solution */
      *length = 0;
      search_node_t *n = node;
      while (n->parent != NULL) {
        *length += 1;
        n = n->parent;
      }

      if (*length == 0) break;

      solution = (dir_t*)malloc(*length*sizeof(dir_t));
      if (solution == NULL) abort();

      for (int i = *length-1; i >= 0; i--) {
        solution[i] = node->dir;
        node = node->parent;
      }
      break;
    }

    /* Put current node on closed list. */
    closed.push_front(node);

    for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
      map_pos_t new_pos = MAP_MOVE(node->pos, d);
      uint cost = actual_cost(node->pos, (dir_t)d);

      /* Check if neighbour is valid. */
      if (!game_road_segment_valid(node->pos, (dir_t)d) ||
          (MAP_OBJ(new_pos) == MAP_OBJ_FLAG && new_pos != end)) {
        continue;
      }

      /* Check if neighbour is in closed list. */
      bool in_closed = false;
      for (std::list<search_node_t*>::iterator it = closed.begin();
           it != closed.end(); ++it) {
        if ((*it)->pos == new_pos) {
          in_closed = true;
          break;
        }
      }

      if (in_closed) continue;

      /* See if neighbour is already in open list. */
      bool in_open = false;
      for (std::vector<search_node_t*>::iterator it = open.begin();
           it != open.end(); ++it) {
        search_node_t *n = *it;
        if (n->pos == new_pos) {
          in_open = true;
          if (n->g_score >= node->g_score + cost) {
            n->g_score = node->g_score + cost;
            n->f_score = n->g_score + heuristic_cost(new_pos, end);
            n->parent = node;
            n->dir = (dir_t)d;

            // Move element to the back and heapify
            iter_swap(it, open.rbegin());
            std::push_heap(open.begin(), open.end(), search_node_less);
          }
          break;
        }
      }

      /* If not found in the open set, create a new node. */
      if (!in_open) {
        search_node_t *new_node = new search_node_t;

        new_node->pos = new_pos;
        new_node->g_score = node->g_score + cost;
        new_node->f_score = new_node->g_score + heuristic_cost(new_pos, end);
        new_node->parent = node;
        new_node->dir = (dir_t)d;

        open.push_back(new_node);
        std::push_heap(open.begin(), open.end(), search_node_less);
      }
    }
  }

  /* Clean up */
  for (std::vector<search_node_t*>::iterator it = open.begin();
       it != open.end(); ++it) {
    delete *it;
  }

  for (std::list<search_node_t*>::iterator it = closed.begin();
       it != closed.end(); ++it) {
    delete *it;
  }

  return solution;
}
