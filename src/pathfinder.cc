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

#include <list>
#include <vector>
#include <algorithm>
#include <memory>

#include <chrono>   // for function performance timing
#include <ctime>  // for function performance timing

// moved to pathfinder.h
/*
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
*/

/* Find the shortest path from start to end (using A*) considering that
   the walking time for a serf walking in any direction of the path
   should be minimized. Returns a malloc'ed array of directions and
   the size of this array in length. */
   //https://en.wikipedia.org/wiki/A*_search_algorithm
// this function is used for plotting Roads, and so is not appropriate
//  for pure pathfinding for freewalking serfs
Road
pathfinder_map(Map *map, MapPos start, MapPos end, const Road *building_road) {
  // Unfortunately the STL priority_queue cannot be used since we
  // would need access to the underlying sequence to determine if
  // a node is already in the open list. We keep instead open as
  // a vector and apply std::pop_heap and std::push_heap to keep
  // it heapified.
  std::vector<PSearchNode> open;
  std::list<PSearchNode> closed;

  /* Create start node */
  PSearchNode node(new SearchNode);
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
      Road solution;
      solution.start(start);

      while (node->parent) {
        Direction dir = node->dir;
        solution.extend(reverse_direction(dir));
        node = node->parent;
      }

      return solution;
    }

    /* Put current node on closed list. */
    closed.push_front(node);

    for (Direction d : cycle_directions_cw()) {
      MapPos new_pos = map->move(node->pos, d);
      unsigned int cost = actual_cost(map, node->pos, d);

      /* Check if neighbour is valid. */
      if (!map->is_road_segment_valid(node->pos, d) ||
          (map->get_obj(new_pos) == Map::ObjectFlag && new_pos != start)) {
        continue;
      }

      if ((building_road != nullptr) && building_road->has_pos(map, new_pos) &&
          (new_pos != end) && (new_pos != start)) {
        continue;
      }

      /* Check if neighbour is in closed list. */
      bool in_closed = false;
      for (PSearchNode closed_node : closed) {
        if (closed_node->pos == new_pos) {
          in_closed = true;
          break;
        }
      }

      if (in_closed) continue;

      /* See if neighbour is already in open list. */
      bool in_open = false;
      for (std::vector<PSearchNode>::iterator it = open.begin();
           it != open.end(); ++it) {
        PSearchNode n = *it;
        if (n->pos == new_pos) {
          in_open = true;
          if (n->g_score >= node->g_score + cost) {
            n->g_score = node->g_score + cost;
            n->f_score = n->g_score + heuristic_cost(map, new_pos, start);
            n->parent = node;
            n->dir = d;

            // Move element to the back and heapify
            iter_swap(it, open.rbegin());
            std::make_heap(open.begin(), open.end(), search_node_less);
          }
          break;
        }
      }

      /* If not found in the open set, create a new node. */
      if (!in_open) {
        PSearchNode new_node(new SearchNode);

        new_node->pos = new_pos;
        new_node->g_score = node->g_score + cost;
        new_node->f_score = new_node->g_score +
                            heuristic_cost(map, new_pos, start);
        new_node->parent = node;
        new_node->dir = d;

        open.push_back(new_node);
        std::push_heap(open.begin(), open.end(), search_node_less);
      }
    }
  }

  return Road();
}



/* Find the shortest path from start to end (using A*) considering that
   the walking time for a serf walking in any direction of the path
   should be minimized.   <--- this matters little here, but leave in place for now
   Returns a malloc'ed array of directions and
   the size of this array in length. */
   //https://en.wikipedia.org/wiki/A*_search_algorithm
// this is a copy of the pathfinder_map logic but modified to
//  be appropriate for freewalking serfs, so it ignores where road segments
//  can be built but still cares about impassible terrain (which includes water)
//  though it might be fun to allow serfs to traverse shallow water!
//
// REMINDER - this (like the original pathfinder_map function), does a REVERSE SEARCH 
//  starting at the end pos and working its way back to start pos to complete the road!
//
//
//  NOTE - MAKE SURE THAT THE end POS IS NOT ACTUALLY A BUILDING AS IT WILL BE REJECTED!
//   if the target is a building, the building's flag should be used as the end pos!!!
//
// note that this ignores terrain height heuristic
Road
pathfinder_freewalking_serf(Map *map, MapPos start, MapPos end, int max_dist) {
 // Log::Debug["pathfinder.cc"] << "inside pathfinder_freewalking_serf, start pos " << start << ", dest pos " << end << ", max_dist " << max_dist << ", remember this is a REVERSE SEARCH";

  if (start == bad_map_pos || end == bad_map_pos){
    Log::Error["pathfinder.cc"] << "inside pathfinder_freewalking_serf, either start pos " << start << " or end pos " << end << " is bad_map_pos " << bad_map_pos;
    throw ExceptionFreeserf("inside pathfinder_freewalking_serf, either start pos or end pos is bad_map_pos!");
  }

  // time this function for debugging
  //std::clock_t start_pathfinder_freewalking_serf = std::clock();

  std::vector<PSearchNode> open;
  std::list<PSearchNode> closed;

  /* Create start node */
  PSearchNode node(new SearchNode);
  node->pos = end;
  node->g_score = 0;
  //node->f_score = heuristic_cost(map, start, end);
  node->f_score = 0;

  open.push_back(node);

  // limit the number of *positions* considered,
  //  though this is NOT the same as maximum possible length!!
  //  want to find a way to also check current road length to reject
  //  based on length without rejecting other, shorter roads as part of 
  //  same solution!

  //unsigned int new_closed_nodes = 0;
  //unsigned int new_open_nodes = 0;
  unsigned int total_pos_considered = 0;
  // reducing the values that were copied from ai_pathfinder, as this is more sensitive
  static const unsigned int plot_road_max_pos_considered = 5000;  // the maximum number of "nodes" (MapPos) considered as part of a single plot_road call before giving up
  //static const unsigned int plot_road_max_length = 100;  // the maximum length of a road solution for plot_road before giving up
  static const unsigned int plot_road_max_length = max_dist;  // this is now a configurable argument
  // max ratio of actual road length compared to ideal straight-line length to determine if road is acceptably short
  //   example, 3.00 means a road of up to 3x the length of a perfectly straight road is acceptable
  // this only looks at the actual Road.get_length() in tiles for convolution checks
  static constexpr double max_convolution = 3.00;

  Road solution;
  solution.start(start);

  //game->clear_debug_mark_pos();

  while (!open.empty()) {
    //Log::Debug["pathfinder.cc"] << "inside pathfinder_freewalking_serf, A";
    std::pop_heap(open.begin(), open.end(), search_node_less);
    node = open.back();
    open.pop_back();

    if (node->pos == bad_map_pos){
      Log::Error["pathfinder.cc"] << "inside pathfinder_freewalking_serf, node->pos " << node->pos << " is bad_map_pos " << bad_map_pos;
      throw ExceptionFreeserf("inside pathfinder_freewalking_serf, node->pos is bad_map_pos!");
    }


    //game->set_debug_mark_pos(node->pos(), "green");

    if (total_pos_considered >= plot_road_max_pos_considered){
      //Log::Info["pathfinder.cc"] << "inside pathfinder_freewalking_serf, maximum MapPos POSITIONS-checked reached (plot_road_max_pos) " << total_pos_considered << ", ending search early";
      break;
    }
    //// PERFORMANCE - try pausing for a very brief time every thousand pos checked to give the CPU a break, see if it fixes frame rate lag
  // if (total_pos_considered > 0 && total_pos_considered % 1000 == 0){
    //  Log::Debug["pathfinder.cc"] << "inside pathfinder_freewalking_serf, taking a quick sleep at " << total_pos_considered << " to give CPU a break";
    //  std::this_thread::sleep_for(std::chrono::milliseconds(200));
    //}
    total_pos_considered++;

    // limit the *length* considered
    unsigned int current_length = 0;  // this is calculated in full each time, not cumulative, so it can be declared every time
    PSearchNode tmp_length_check_node = node;
    while (tmp_length_check_node->parent) {
      current_length++;
      tmp_length_check_node = tmp_length_check_node->parent;
    }
    //Log::Debug["pathfinder.cc"] << "inside pathfinder_freewalking_serf, current road-length so far for this solution is " << current_length;
    if (current_length >= plot_road_max_length){
      //Log::Info["pathfinder.cc"] << "inside pathfinder_freewalking_serf, maximum road-length reached (plot_road_max_length) " << current_length << ", ending search early";
      break;
    }

    if (node->pos == start) {
      //Log::Debug["pathfinder.cc"] << "inside pathfinder_freewalking_serf, B  FOUND SOLUTION";
      /* Construct solution */
      //Road solution;
      //solution.start(start);

      while (node->parent) {
        Direction dir = node->dir;
        solution.extend(reverse_direction(dir));
        node = node->parent;
      }


       //double duration = (std::clock() - start_pathfinder_freewalking_serf) / static_cast<double>(CLOCKS_PER_SEC);
       //Log::Debug["pathfinder_freewalking_serf"] << "done pathfinder_freewalking_serf, call took " << duration << ", considered " << total_pos_considered << " positions";

      return solution;
    }

    //Log::Debug["pathfinder.cc"] << "inside pathfinder_freewalking_serf, C  KEEP SEARCHING, node->pos " << node->pos << " is not yet the start pos " << start;

    /* Put current node on closed list. */
    closed.push_front(node);

    for (Direction d : cycle_directions_cw()) {
      //Log::Debug["pathfinder.cc"] << "inside pathfinder_freewalking_serf, D  Dir: " << d;
      MapPos new_pos = map->move(node->pos, d);
      unsigned int cost = actual_cost(map, node->pos, d);
      // default to lowest value (255), ignore heuristics
      //unsigned int cost = 255;

      //
      // I THINK THIS IS THE ONLY PART THAT NEEDS TO CHANGE
      //  maybe just make this an if statement inside pathfinder_map
      //  instead of its own function?
      //
      /* Check if neighbour is valid. */
      /*
        MapPos other_pos = move(pos, dir);
        Object obj = get_obj(other_pos);
        if ((paths(other_pos) != 0 && obj != ObjectFlag) ||
            Map::map_space_from_obj[obj] >= SpaceSemipassable) {
          return false;
        }

        if (!has_owner(other_pos) ||
            get_owner(other_pos) != get_owner(pos)) {
          return false;
        }

        if (is_in_water(pos) != is_in_water(other_pos) &&
            !(has_flag(pos) || has_flag(other_pos))) {
          return false;
        }
      */

      //if (!map->is_road_segment_valid(node->pos, d) ||
      //    (map->get_obj(new_pos) == Map::ObjectFlag && new_pos != start)) {
      if (!map->can_serf_step_into(node->pos, d)){
        //Log::Debug["pathfinder.cc"] << "inside pathfinder_freewalking_serf, E  serf CANNOT step from node->pos " << node->pos << " into next pos in dir " << d;
        continue;
      }
      //Log::Debug["pathfinder.cc"] << "inside pathfinder_freewalking_serf, E  serf can step from node->pos " << node->pos << " into next pos in dir " << d;

      //
      //if ((building_road != nullptr) && building_road->has_pos(map, new_pos) &&
      //    (new_pos != end) && (new_pos != start)) {
      //  continue;
      //}

      /* Check if neighbour is in closed list. */
      bool in_closed = false;
      for (PSearchNode closed_node : closed) {
        if (closed_node->pos == new_pos) {
          in_closed = true;
          break;
        }
      }

      if (in_closed) continue;

      /* See if neighbour is already in open list. */
      bool in_open = false;
      for (std::vector<PSearchNode>::iterator it = open.begin();
           it != open.end(); ++it) {
        PSearchNode n = *it;
        if (n->pos == new_pos) {
          in_open = true;
          if (n->g_score >= node->g_score + cost) {
            n->g_score = node->g_score + cost;
            n->f_score = n->g_score + heuristic_cost(map, new_pos, start);
            //n->f_score = n->g_score;
            n->parent = node;
            n->dir = d;

            // Move element to the back and heapify
            iter_swap(it, open.rbegin());
            std::make_heap(open.begin(), open.end(), search_node_less);
          }
          break;
        }
      }

      /* If not found in the open set, create a new node. */
      if (!in_open) {
        PSearchNode new_node(new SearchNode);

        new_node->pos = new_pos;
        new_node->g_score = node->g_score + cost;
        new_node->f_score = new_node->g_score + heuristic_cost(map, new_pos, start);
        //new_node->f_score = new_node->g_score;
        new_node->parent = node;
        new_node->dir = d;

        open.push_back(new_node);
        std::push_heap(open.begin(), open.end(), search_node_less);
      }
    }
  }

  //double duration = (std::clock() - start_pathfinder_freewalking_serf) / static_cast<double>(CLOCKS_PER_SEC);
  //Log::Debug["pathfinder_freewalking_serf"] << "done pathfinder_freewalking_serf, call took " << duration;

  return Road();
}
