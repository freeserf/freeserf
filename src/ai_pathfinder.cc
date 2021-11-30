/*
 * ai_pathfinder.cc - extra pathfinder functions used for AI
 *     Copyright 2019-2021 tlongstretch
 *
 *     FlagSearch function mostly copied from Pyrdacor's Freeserf.NET
 *        C# implementation
 *
 */

//
//  jan07 2021 - NOTE - it looks like the game stores the length of each path in the flag's length[] array.
//    This could be used instead of tracing and counting the tiles if the actual paths
//    are not needed, and it should be faster
//  NEVERMIND - the flag->length[dir] values are not true road lengths, though they start out that way,
//   they are banded into certain ranges, bit-shifted, and used for storing other state
//   I am thinking of modifying the Flag vars to store the true length and Dirs (i.e. the Road object)
//

#include "src/ai.h"

#include "src/pathfinder.h"  // for original tile SearchNode

class FlagSearchNode;

typedef std::shared_ptr<FlagSearchNode> PFlagSearchNode;

class FlagSearchNode {
 public:
  PFlagSearchNode parent;
  //unsigned int g_score;
  //unsigned int f_score;
  unsigned int flag_dist = 0;
  //unsigned int tile_dist;
  //Flag *flag = nullptr;   // try to make this work without using any Flag* objects, only MapPos where flag is
  MapPos pos = bad_map_pos;
  Direction dir = DirectionNone;  // this supports a search method I think is BROKEN!!! investigate and use the below way instead
  // the flag_pos of any path in each Direction from this fnode
  MapPos child_dir[6] = { bad_map_pos, bad_map_pos, bad_map_pos, bad_map_pos, bad_map_pos, bad_map_pos };
  // used for arterial_road flagsearch only, because it traces from first to last 
  //  when a solution is found, rather than last to first. This is so it can handle
  //   forks because it is not a single path but a network ending at an Inventory flag pos
  PFlagSearchNode child[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
};

static bool
flagsearch_node_less(const PFlagSearchNode &left, const PFlagSearchNode &right) {
  // A search node is considered less than the other if
  // it has a *lower* flag distance.  This means that in the max-heap
// the shorter distance will go to the top
  //return left->flag_dist < right->flag_dist;
  // testing reversing this to see if it fixes depth-first issue
  // YES I think it does... so using above seems to do depth-first and using this below does breadth first
  return right->flag_dist < left->flag_dist;
}




// plot a Road between two pos and return it
//  *In addition*, while plotting that road, use any existing flags and anyplace that new flags
//    could be built on existing roads along the way to populate a set of additional potential Roads.
//    Any valid existing or potential flag pos encountered "in the way" during pathfinding
//    will be included, but not positions that are not organically checked by the direct pathfinding logic.
//    The set will include the original-specified-end direct Road if it is valid
// is road_options actually used here??? or only by build_best_road??
//   seems that RoadOptions is not used, but I think I need it to be for HoldBuildingPos
// the road returned is the DIRECT road if found
//  however, if no direct road is found the calling function could still check for the new-flag splitting
//   options found
Road
// adding support for HoldBuildingPos
//AI::plot_road(PMap map, unsigned int player_index, MapPos start_pos, MapPos end_pos, Roads * const &potential_roads) {
AI::plot_road(PMap map, unsigned int player_index, MapPos start_pos, MapPos end_pos, Roads * const &potential_roads, bool hold_building_pos) {
  AILogDebug["plot_road"] << name << " inside plot_road for Player" << player_index << " with start " << start_pos << ", end " << end_pos;
  
  std::vector<PSearchNode> open;
  std::list<PSearchNode> closed;
  PSearchNode node(new SearchNode);

  if (hold_building_pos == true){
    AILogDebug["plot_road"] << name << " DEBUG - hold_building_pos is TRUE!";
    PSearchNode hold_building_pos_node(new SearchNode);
    hold_building_pos_node->pos = map->move_up_left(start_pos);
    closed.push_front(hold_building_pos_node);
    AILogDebug["plot_road"] << name << " DEBUG - hold_building_pos is TRUE, added hold_building_pos " << hold_building_pos_node->pos << " which is up-left of start_pos " << start_pos;
  }

  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  //
  // pathfinding direction is now REVERSED from original code!  this function starts at the start_pos and ends at the end_pos, originally it was the opposite.
  //    As far as I can tell there is no downside to this, but flipping it makes fake flag solutions easy because when a potential new flag position
  //    is found on an existing road, the search path so far can be used to reach this new flag.  If the original (backwards) method is used, the
  //    search path "bread crumb trail" takes you back to the target_pos which isn't helpful - a path is needed between start_pos and new flag pos
  //
  node->pos = start_pos;
  node->g_score = 0;
  node->f_score = heuristic_cost(map.get(), end_pos, start_pos);
  open.push_back(node);
  Road direct_road;
  bool found_direct_road = false;
  //putting this here for now
  unsigned int max_fake_flags = 10;
  unsigned int fake_flags_count = 0;
  unsigned int split_road_solutions = 0;

  // this is a TILE search, finding open PATHs to build a single Road between two Flags
  while (!open.empty()) {
    std::pop_heap(open.begin(), open.end(), search_node_less);
    node = open.back();
    open.pop_back();
    // if end_pos is reached, a Road solution is found
    if (node->pos == end_pos) {
      Road breadcrumb_solution;
      breadcrumb_solution.start(end_pos);
      // retrace the "bread crumb trail" tile by tile from end to start
      //   this creates a Road with 'source' of end_pos and 'last' of start_pos
      //     which is backwards from the direction the pathfinding ran
      while (node->parent) {
        // if ai_overlay is on, ai_mark_road will show the road being traced backwards from end_pos to start_pos
        Direction dir = node->dir;
        breadcrumb_solution.extend(reverse_direction(dir));
        //ai_mark_road->extend(reverse_direction(dir));
        //ai_mark_pos.insert(ColorDot(node->pos, "black"));
        //std::this_thread::sleep_for(std::chrono::milliseconds(0));
        node = node->parent;
      }
      unsigned int new_length = static_cast<unsigned int>(breadcrumb_solution.get_length());
      AILogDebug["plot_road"] << name << " plot_road: solution found, new segment length is " << breadcrumb_solution.get_length();
      // now inverse the entire road so that it stats at start_pos and ends at end_pos, so the Road object is logical instead of backwards
      Road solution = reverse_road(map, breadcrumb_solution);
      // copy the solution to Road direct_road to be returned.  This isn't necessary but seems clearer,
      //   could just return 'solution' and move its declaration to the start of the function
      direct_road = solution;
      found_direct_road = true;
      break;
    }

    // otherwise, close this node and continue
    closed.push_front(node);
    // consider each direction for extending road
    // oct21 2020 - changing to this use RANDOM start direction to avoid issue where a road
    //   is never built because of an obstacle in one major direction, but a road could have been built if
    //   a different direction was chosen.  This means if many nearby spots are tried one should eventually succeed
    //for (Direction d : cycle_directions_cw()) {
    for (Direction d : cycle_directions_rand_cw()) {
      MapPos new_pos = map->move(node->pos, d);
      unsigned int cost = actual_cost(map.get(), node->pos, d);
      //ai_mark_road->extend(d);
      //ai_mark_pos.insert(ColorDot(new_pos, "blue"));
      //std::this_thread::sleep_for(std::chrono::milliseconds(10));
      // Check if neighbour is valid
      if (!map->is_road_segment_valid(node->pos, d) ||
        (map->get_obj(new_pos) == Map::ObjectFlag && new_pos != end_pos)) {
        //(avoid_castle && AI::is_near_castle(game, new_pos))) {
        //
        // if the pathfinder hits an existing road point where a flag could be built,
        //   create a "fake flag" solution and include it as a potential road
        //
        if (fake_flags_count > max_fake_flags) {
          AILogDebug["plot_road"] << name << " reached max_fake_flags count " << max_fake_flags << ", not considering any more fake flag solutions";
          continue;
        }
        // split road found if can build a flag, and that flag is already part of a road (meaning it has at least one path)
        if (game->can_build_flag(new_pos, player) &&
          (map->has_path(new_pos, Direction(0)) || map->has_path(new_pos, Direction(1)) || map->has_path(new_pos, Direction(2)) ||
            map->has_path(new_pos, Direction(3)) || map->has_path(new_pos, Direction(4)) || map->has_path(new_pos, Direction(5)))) {
          fake_flags_count++;
          AILogDebug["plot_road"] << name << " plot_road: alternate/split_road solution found while pathfinding to " << end_pos << ", a new flag could be built at pos " << new_pos;
          // retrace the "bread crumb trail" tile by tile from end to start
          //   this creates a Road with 'source' of end_pos and 'last' of start_pos
          //     which is backwards from the direction the pathfinding ran

          // we still have to create a new node for the final step or the retrace won't work right
          PSearchNode split_flag_node(new SearchNode);
          split_flag_node->pos = new_pos;
          // these don't matter for this stub node
          //split_flag_node->g_score = node->g_score + cost;
          //split_flag_node->f_score = split_flag_node->g_score +
          //  heuristic_cost(map.get(), new_pos, end_pos);
          split_flag_node->parent = node;
          split_flag_node->dir = d;
          // and DON'T put it on any lists becase it isn't part of the original solution, it will get marked as in closed later
          //open.push_back(new_node);
          //std::push_heap(open.begin(), open.end(), search_node_less);

          // store the current node so it can be restored after this part is done
          PSearchNode orig_node = node;

          // start the road at new_pos (where potential flag could be built), and follow the 'node' path back to start_pos
          Road split_flag_breadcrumb_solution;
          split_flag_breadcrumb_solution.start(split_flag_node->pos);   // same as new_pos

          while (split_flag_node->parent) {
            // if ai_overlay is on, ai_mark_road will show the road being traced backwards from end_pos to start_pos
            Direction dir = split_flag_node->dir;
            split_flag_breadcrumb_solution.extend(reverse_direction(dir));
            //ai_mark_road->extend(reverse_direction(dir));
            //ai_mark_pos.insert(ColorDot(node->pos, "black"));
            //std::this_thread::sleep_for(std::chrono::milliseconds(0));
            split_flag_node = split_flag_node->parent;
          }
          // restore original node so search can resume
          node = orig_node;
          unsigned int new_length = static_cast<unsigned int>(split_flag_breadcrumb_solution.get_length());
          AILogDebug["plot_road"] << name << " plot_road: split_flag_breadcrumb_solution found, new segment length is " << split_flag_breadcrumb_solution.get_length();
          // now inverse the entire road so that it stats at start_pos and ends at end_pos, so the Road object is logical instead of backwards
          Road split_flag_solution = reverse_road(map, split_flag_breadcrumb_solution);
          AILogDebug["plot_road"] << name << " plot_road: inserting alternate/fake flag Road solution to PotentialRoads, new segment length would be " << split_flag_solution.get_length();
          potential_roads->push_back(split_flag_solution);
          split_road_solutions++;
        }
        // if we don't 'continue' here, the pathfinder will think this node is valid and won't
        //  complete the original requested solution
        continue;
      }

      // WARNING - commenting this out to break dependency on interface->building_road
      //   it looks to prevent road from being drawn over itself?
      //  find a way to implement this that doesn't depend on interface->building_road?
      ///  maybe it doesn't matter after all?  I did a year of AI runs without it
      //if ((building_road != nullptr) && building_road->has_pos(map, new_pos) &&
      //    (new_pos != end) && (new_pos != start)) {
      //  continue;
      //}

      /* Check if neighbour is in closed list. */
      bool in_closed = false;
      for (PSearchNode closed_node : closed) {
        if (closed_node->pos == new_pos) {
          in_closed = true;
          //ai_mark_pos.erase(new_pos);
          //ai_mark_pos.insert(ColorDot(new_pos, "magenta"));
          //std::this_thread::sleep_for(std::chrono::milliseconds(0));
          break;
        }
      }
      //ai_mark_pos.erase(new_pos);
      //ai_mark_pos.insert(ColorDot(new_pos, "green"));
      //std::this_thread::sleep_for(std::chrono::milliseconds(0));

      if (in_closed) continue;

      /* See if neighbour is already in open list. */
      bool in_open = false;
      for (std::vector<PSearchNode>::iterator it = open.begin();
        it != open.end(); ++it) {
        PSearchNode n = *it;
        if (n->pos == new_pos) {
          in_open = true;
          //ai_mark_pos.erase(new_pos);
          //ai_mark_pos.insert(ColorDot(new_pos, "seafoam"));
          if (n->g_score >= node->g_score + cost) {
            n->g_score = node->g_score + cost;
            n->f_score = n->g_score + heuristic_cost(map.get(), new_pos, end_pos);
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
          heuristic_cost(map.get(), new_pos, end_pos);
        new_node->parent = node;
        new_node->dir = d;

        open.push_back(new_node);
        std::push_heap(open.begin(), open.end(), search_node_less);
      }
    } // foreach direction
  } // while nodes left to search

  AILogDebug["plot_road"] << name << " done plot_road, found_direct_road: " << std::to_string(found_direct_road) << ". additional potential_roads (split_roads): " << split_road_solutions;
  if (direct_road.get_source() == bad_map_pos) {
    AILogDebug["plot_road"] << name << " NO DIRECT SOLUTION FOUND for start_pos " << start_pos << " to end_pos " << end_pos;
  }
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["plot_road"] << name << " plot road call took " << duration;
  return direct_road;
}


// count how many tiles apart two MapPos are
int
AI::get_straightline_tile_dist(PMap map, MapPos start_pos, MapPos end_pos) {
  AILogDebug["get_straightline_tile_dist"] << name << " inside Pathfinder::tile_dist with start_pos " << start_pos << ", end_pos " << end_pos;
  // function copied from heuristic_cost but ignores height diff and walking_cost
  int dist_col = map->dist_x(start_pos, end_pos);
  int dist_row = map->dist_y(start_pos, end_pos);
  int tile_dist = 0;
  if ((dist_col > 0 && dist_row > 0) || (dist_col < 0 && dist_row < 0)) {
    tile_dist = std::max(abs(dist_col), abs(dist_row));
  }
  else {
    tile_dist = abs(dist_col) + abs(dist_row);
  }
  AILogDebug["get_straightline_tile_dist"] << name << " returning tile_dist: " << tile_dist;
  return tile_dist;
}


// find best flag-path from flag_pos to target_pos and store flag and tile count in roadbuilder
//  return true if solution found, false if couldn't be found
// this function will accept "fake flags" / splitting-flags that do not actually exist
//  and score them based on the best score of their adjacent flags
bool
AI::score_flag(PMap map, unsigned int player_index, RoadBuilder *rb, RoadOptions road_options, MapPos flag_pos, MapPos castle_flag_pos, ColorDotMap *ai_mark_pos) {
  AILogDebug["score_flag"] << name << " inside score_flag";
  MapPos target_pos = rb->get_target_pos();
  AILogDebug["score_flag"] << name << " preparing to score_flag for Player" << player_index << " for flag at flag_pos " << flag_pos << " to target_pos " << target_pos;

  // handle split_road / fake flag solutions
  if (!map->has_flag(flag_pos)) {
    AILogDebug["score_flag"] << name << " flag not found at flag_pos " << flag_pos << ", assuming this is a fake flag/split road solution and using closest adjacent flag + tile dist from fake flag";
    // fake_flags can't be scored normally because their SerfPathData isn't complete so the flag->get_other_end_dir used in flag-path finding doesn't work
    // instead, of the two flags adjacent to the fake flag, the best-scoring one will be used and then add 1 flag dist and actual traced tile dist between fake flag and that flag
    //    to get the correct score for evaluation
    // renaming the MapPos here to reduce confusion about what they mean
    MapPos splitting_flag_pos = flag_pos;
    MapPos adjacent_flag_pos;
    //unsigned int tile_dist = bad_score;
    //unsigned int lowest_tile_dist = bad_score;
    //unsigned int flag_dist = bad_score;
    //unsigned int lowest_flag_dist = bad_score;
    unsigned int best_adjacent_flag_adjusted_score = bad_score;
    MapPos best_adjacent_flag_pos = bad_map_pos;
    for (Direction dir : cycle_directions_cw()) {
      AILogDebug["score_flag"] << name << " looking for a path in dir " << NameDirection[dir];
      if (!map->has_path(splitting_flag_pos, dir)) {
        continue;
      }
      AILogDebug["score_flag"] << name << " found a path in dir " << NameDirection[dir] << name << ", tracing road";
      Road split_road = trace_existing_road(map, splitting_flag_pos, dir);
      adjacent_flag_pos = split_road.get_end(map.get());
      unsigned int tiles_to_adjacent_flag = static_cast<unsigned int>(split_road.get_length());
      AILogDebug["score_flag"] << name << " split road in dir " << NameDirection[dir] << name << " has length " << tiles_to_adjacent_flag << " to adjacent flag at pos " << adjacent_flag_pos;
      unsigned int tile_dist = bad_score;
      unsigned int flag_dist = bad_score;
      unsigned int adjusted_score = bad_score;
      bool contains_castle_flag = false;
      // go score the adjacent flags if they aren't already known (sometimes they will already have been checked)
      if (!rb->has_score(adjacent_flag_pos)) {
        AILogDebug["score_flag"] << name << " score_flag, rb doesn't have score yet for adjacent flag_pos " << adjacent_flag_pos << ", will try to score it";
        if (find_flag_and_tile_dist(map, player_index, rb, adjacent_flag_pos, castle_flag_pos, ai_mark_pos)) {
          AILogDebug["score_flag"] << name << " score_flag, splitting_flag find_flag_and_tile_dist() returend true from flag_pos " << adjacent_flag_pos << " to target_pos " << target_pos;
        }
        else{
          AILogDebug["score_flag"] << name << " score_flag, splitting_flag find_flag_and_tile_dist() returned false, cannot find flag-path solution from flag_pos " << adjacent_flag_pos << " to target_pos " << target_pos << ".  Returning false";
          // if not found just let rb->get_score call fail and it wlil use the default bad_scores given.
          AILogDebug["score_flag"] << name << " score_flag returned false for adjacent flag at pos " << adjacent_flag_pos;
          AILogDebug["score_flag"] << name << " for now, leaving default bogus super-high score for adjacent flag";
          //std::this_thread::sleep_for(std::chrono::milliseconds(0));
        }
      }
      FlagScore score = rb->get_score(adjacent_flag_pos);
      tile_dist = score.get_tile_dist();
      flag_dist = score.get_flag_dist();
      contains_castle_flag = score.get_contains_castle_flag();   // keep whatever value the adjacent flag has, it's flag-path could very well contain the castle flag
      AILogDebug["score_flag"] << name << " adjacent flag at " << adjacent_flag_pos << " has tile_dist " << tile_dist << ", flag dist " << flag_dist << ", contains_castle_flag " << contains_castle_flag;
      tile_dist += tiles_to_adjacent_flag;
      flag_dist += 1;
      AILogDebug["score_flag"] << name << " splitting_flag has tile_dist: " << tile_dist << ", flag_dist: " << flag_dist;
      adjusted_score = tile_dist + tiles_to_adjacent_flag + flag_dist;
      if (road_options.test(RoadOption::PenalizeCastleFlag) && contains_castle_flag == true) {
        adjusted_score += contains_castle_flag_penalty;
        AILogDebug["score_flag"] << name << " applying contains_castle_flag penalty of " << contains_castle_flag_penalty;
      }
      AILogDebug["score_flag"] << name << " adjacent flag at " << adjacent_flag_pos << " has adjusted_score " << adjusted_score;
      //rb->set_score(best_adjacent_flag_pos, tile_dist, flag_dist, contains_castle_flag);
      // use the score from whichever adjacent flag is better (because the game will use the best route serfs/resources)
      if (adjusted_score < best_adjacent_flag_adjusted_score) {
        AILogDebug["score_flag"] << name << " this score " << adjusted_score << " is better than best_adjacent_flag_adjusted_score of " << best_adjacent_flag_adjusted_score << ", setting splitting_flag_pos " << splitting_flag_pos << "'s score to that";
        best_adjacent_flag_pos = adjacent_flag_pos;
        best_adjacent_flag_adjusted_score = adjusted_score;
        rb->set_score(splitting_flag_pos, tile_dist, flag_dist, contains_castle_flag);
      }
      AILogDebug["score_flag"] << name << " best adjacent flag right now is best_adjacent_flag_pos " << best_adjacent_flag_pos << " with adjusted_score " << best_adjacent_flag_adjusted_score;
    }
    AILogDebug["score_flag"] << name << " done with flag flag/split road solution at flag_pos " << flag_pos << "'s adjacent_flag scoring";
    return true;
  } // if split road / fake flag

  // handle direct road to target_pos - perfect score
  if (target_pos == flag_pos) {
    // this is a *direct route* to the target flag with no intermediate flags
    //    so the only scoring factor will be the new segment's tile length
    //     ??  need to add penalties??
    AILogDebug["score_flag"] << name << " score_flag, flag_pos *IS* target_pos, setting values 0,0";
    //ai_mark_pos->erase(flag_pos);
    //ai_mark_pos->insert(ColorDot(flag_pos, "coral"));
    //std::this_thread::sleep_for(std::chrono::milliseconds(0));
    // note that this blindly ignores if castle flag / area part of solution, FIX!
    rb->set_score(flag_pos, 0, 0, false);
    AILogDebug["score_flag"] << name << " score_flag, flag_pos *IS* target_pos, returning true";
    return true;
  }

  // handle most common case, regular scoring
  if (!find_flag_and_tile_dist(map, player_index, rb, flag_pos, castle_flag_pos, ai_mark_pos)) {
    AILogDebug["score_flag"] << name << " score_flag, find_flag_and_tile_dist() returned false, cannot find flag-path solution from flag_pos " << flag_pos << " to target_pos " << target_pos << ".  Returning false";
    return false;
  }

  // debug only
  FlagScore score = rb->get_score(flag_pos);
  unsigned int flag_dist = score.get_flag_dist();
  unsigned int tile_dist = score.get_tile_dist();
  AILogDebug["score_flag"] << name << " score_flag, find_flag_and_tile_dist() from flag_pos " << flag_pos << " to target_pos " << target_pos << " found flag_dist " << flag_dist << ", tile_dist " << tile_dist;

  AILogDebug["score_flag"] << name << " done score_flag, found solution, returning true";
  return true;
}


// build a RoadEnds object from a Road object by checking the first and
//    last dir, to determine the Direction leading back to each end
//    until another flag is found.  The start pos doesn't have to be a real flag,
//     which means fake flags will work
RoadEnds
AI::get_roadends(PMap map, Road road) {
  AILogDebug["get_roadends"] << name << " inside get_roadends";
  std::list<Direction> dirs = road.get_dirs();
  std::list<Direction>::iterator i;
  for (i = dirs.begin(); i != dirs.end(); i++) {
    Direction dir = *i;
    AILogVerbose["get_roadends"] << name << " get_roadends - Direction " << dir << " / " << NameDirection[dir];
  }
  MapPos start_pos = road.get_source();
  MapPos end_pos = road.get_end(map.get());  // this function just traces the road along the existing path anyway
  Direction start_dir = road.get_dirs().front();
  // the Direction of the path leading back to the start is the reverse of the last dir in the road (the dir that leads to the end flag)
  Direction end_dir = reverse_direction(road.get_dirs().back());
  AILogDebug["get_roadends"] << name << " inside get_roadends, start_pos " << start_pos << ", start_dir: " << NameDirection[start_dir] << name << ", end_pos " << end_pos << ", end_dir: " << NameDirection[end_dir];
  RoadEnds ends = std::make_tuple(start_pos, start_dir, end_pos, end_dir);
  return ends;
}


// reverse a Road object - end_pos becomes start_pos, vice versa, and all directions inside reversed to match
Road
AI::reverse_road(PMap map, Road road) {
  AILogDebug["reverse_road"] << name << " inside reverse_road, for a road with source " << road.get_source() << ", end " << road.get_end(map.get()) << ", and length " << road.get_length();
  Road reversed_road;
  reversed_road.start(road.get_end(map.get()));
  std::list<Direction> dirs = road.get_dirs();
  std::list<Direction>::reverse_iterator r;
  for (r = dirs.rbegin(); r != dirs.rend(); r++) {
    reversed_road.extend(reverse_direction(*r));
  }
  AILogDebug["reverse_road"] << name << " returning reversed_road which has source " << reversed_road.get_source() << ", end " << reversed_road.get_end(map.get()) << ", and length " << reversed_road.get_length();
  return reversed_road;
}

// *********************************************
//  WAIT AGAIN!  I think flagsearch_node_less is correct, but something else is wrong
//  it is not using flag->length it is using node->flag_length which should be correct
// flag dist being set by me in the function
// *********************************************
//
// NOTE - understand why flagsearch_node_less does!  it chooses the flag with the lower flag->length value
//   BUT, this function actually does a more accurate check than flag->length, it counts exact tile dist
//  MAKE SURE THAT the flagsearch_node_less is replaced with a function that compares TILE DIST, and one
//   that correctly compares FLAG DIST in case it is being messed up by that
// here's the flagsearch_node_less function which returns a bool for the one that is higher/lower than the other:
//
// ALSO, it seems to be corrupting things... switching flags?   after it runs I am seeing flag->get_building->get_type return
//  the wrong building for the pos it is associated with!!!!!
//
/*
static bool
flagsearch_node_less(const PFlagSearchNode &left, const PFlagSearchNode &right) {
  // A search node is considered less than the other if
  // it has a *lower* flag distance.  This means that in the max-heap
// the shorter distance will go to the top
  return left->flag_dist < right->flag_dist;
}
*/
//

// perform FlagSearch to find best flag-path from flag_pos to target_pos and determine tile path along the way.
// return true if solution found, false if not
// store the solution/scores in the RoadBuilder object passed into function
//  this function will NOT work for fake flags / splitting flags
bool
AI::find_flag_and_tile_dist(PMap map, unsigned int player_index, RoadBuilder *rb, MapPos flag_pos, MapPos castle_flag_pos, ColorDotMap *ai_mark_pos) {
  AILogDebug["find_flag_and_tile_dist"] << name << " inside find_flag_and_tile_dist";
  MapPos target_pos = rb->get_target_pos();
  AILogDebug["find_flag_and_tile_dist"] << name << " preparing to find_flag_and_tile_dist from flag at flag_pos " << flag_pos << " to target_pos " << target_pos;

  //ai_mark_pos->erase(flag_pos);
  //ai_mark_pos->insert(ColorDot(flag_pos, "dk_blue"));
  //std::this_thread::sleep_for(std::chrono::milliseconds(0));
  std::vector<PFlagSearchNode> open;
  std::list<PFlagSearchNode> closed;
  PFlagSearchNode fnode(new FlagSearchNode);

  fnode->pos = flag_pos;

  unsigned int flag_dist = 0;
  unsigned int tile_dist = 0;
  bool contains_castle_flag = false;

  open.push_back(fnode);
  AILogVerbose["find_flag_and_tile_dist"] << name << " fsearchnode - starting fnode search for flag_pos " << flag_pos;
  while (!open.empty()) {
    std::pop_heap(open.begin(), open.end(), flagsearch_node_less);
    fnode = open.back();
    open.pop_back();
    AILogVerbose["find_flag_and_tile_dist"] << name << " fsearchnode - inside fnode search for flag_pos " << flag_pos << ", inside while-open-list-not-empty loop";
    if (fnode->pos == target_pos) {
      //ai_mark_pos->erase(fnode->pos);
      //ai_mark_pos->insert(ColorDot(fnode->pos, "cyan"));
      //std::this_thread::sleep_for(std::chrono::milliseconds(0));
      //
      // target_pos flag reached!
      //
      AILogVerbose["find_flag_and_tile_dist"] << name << " fsearchnode - fnode->pos " << fnode->pos << " *is now* target_pos " << target_pos;
      AILogDebug["find_flag_and_tile_dist"] << name << " plotted a complete multi-road solution from flag_pos " << flag_pos << " to target_pos " << target_pos;
      // record the solution
      //
      //  wait, don't I already have the solution?? the score... is all that matters.
      //   There is no need to store the flag-node steps, unless maybe to cache for later
      //     I think this just means search is complete and scores should be recorded
      //
      while (fnode->parent) {
        // because fake flags aren't found when following existing paths
        //  the pathfinding must be done by moving away from the target_pos one flag at a time...
        //    and tracing the path forwards to the flag that we just came from
        //    ??   is this even true with new fake-flag solution of just checking the adjacent flags??
        MapPos end1 = fnode->parent->pos;
        // wait a minute... after tracing complex bug it seems like end2 should not be fnode->pos but instead
        //  should be determined by the result of the trace_existing_road call??
        // I think I am getting bogus end2 results in some cases (maybe all?)
        //MapPos end2 = fnode->pos;
        MapPos end2 = bad_map_pos;
        Direction dir1 = fnode->parent->dir;
        // not sure why this is happening, since reversing the flagsearch_node_less started seeing dir1 being invalid occasionally
        if (dir1 < 0 || dir1 > 5){
          AILogError["find_flag_and_tile_dist"] << name << " score_flag: end1 " << end1 << "'s dir1 " << dir1 << " is an invalid direction!  sleeping 10sec then crashing";
          ai_mark_pos->erase(end1);
          ai_mark_pos->insert(ColorDot(end1, "yellow"));
          std::this_thread::sleep_for(std::chrono::milliseconds(10000));
          throw ExceptionFreeserf("find_flag_and_tile_dist, end1 has invalid direction for dir1");
        }
        // Direction dir2 =  complicated: see below...
        Direction dir2 = DirectionNone;

        // unless Road is already known inside RoadBuilder
        //   build Road object by tracing paths between flags
        Road existing_road;
        if (false) {
          // keeping road cache disabled for now
        }
        else {
          // trace the road to get the tile-path (we already have the flag-path in the FlagSearch object
          existing_road = trace_existing_road(map, end1, dir1);

          // trying to avoid a bug where trace_existing_road can't find a path in the specified dir
          // and so it returns a basically null road.  Not sure why it happens
          if (existing_road.get_source() == bad_map_pos){
            AILogWarn["find_flag_and_tile_dist"] << name << " trace_existing_road for end1 " << end1 << " and dir1 " << dir1 << " returned null Road!";
          }else{
            //after tracing complex bug it seems like end2 should not be fnode->pos but instead
            //  should be determined by the result of the trace_existing_road call??
            // I think I am getting bogus end2 results in some cases (maybe all?)
            // instead, now getting end2 from traced existing road
            end2 = existing_road.get_end(map.get());

            //// old junk:
            // store information about the road
            //    ??
            //  is eroad actually needed?  can I just get the road.last and invert it?  ???
            //   ??
            // dir2, the dir of the path leading back to the parent flag-node, is the inverse of the last dir in the Road that was just traced!

            //dir2 = reverse_direction(existing_road.get_last());

            //rb->new_eroad(end1, dir1, end2, dir2, existing_road);
            //
            // why not just have new_eroad run get_roadends itself??
            //   and I guess new_proad also
            //
            // ??? actually, could have new_eroad also trace_existing_road???
            //
            RoadEnds ends = get_roadends(map, existing_road);
            rb->new_eroad(ends, existing_road);
          } // if traced road failed
        }
        unsigned int existing_road_length = static_cast<unsigned int>(existing_road.get_length());
        fnode = fnode->parent;
        // should be able to use the last node's flag_dist and get same result... maybe compare both as sanity check
        flag_dist++;
        tile_dist += existing_road_length;
        AILogDebug["find_flag_and_tile_dist"] << name << " existing road segment with end1 " << end1 << ", end2 " << end2 << " has length " << existing_road_length;
        if (end1 == castle_flag_pos){
          AILogDebug["find_flag_and_tile_dist"] << name << " this solution contains the castle flag!";
          contains_castle_flag = true;
        }
        AILogDebug["find_flag_and_tile_dist"] << name << " total so far road from flag_pos " << flag_pos << " to target_pos " << target_pos << " is flag_dist " << flag_dist << ", tile_dist " << tile_dist;
      } // while fnode->parent / retrace flag-solution
      AILogDebug["find_flag_and_tile_dist"] << name << " final total for road from flag_pos " << flag_pos << " to target_pos " << target_pos << " is flag_dist " << flag_dist << ", tile_dist " << tile_dist;
      rb->set_score(flag_pos, flag_dist, tile_dist, contains_castle_flag);
      AILogDebug["find_flag_and_tile_dist"] << name << " found solution, returning true";
      //return std::make_tuple(flag_dist, tile_dist, contains_castle_flag);
      return true;
    } // if found solution

    AILogDebug["find_flag_and_tile_dist"] << name << " fsearchnode - node->pos is not target_pos " << target_pos << " yet";
    closed.push_front(fnode);
    // for each direction that has a path, trace the path until a flag is reached
    for (Direction d : cycle_directions_cw()) {
      // attempt to fix bug, setting fnode->dir to d much earlier in the loop because it should be set in all cases, right?
      //  and if the loop exits before reaching here it is never set??? i dunno
      // this seems to be fixing it, actually
      fnode->dir = d;

      // attempt to work around the issue with castle flag appearing to have a path UpLeft from its flag into
      //  the castle, which breaks pathfinding
      //  is this an issue for Warehouses/Stocks, too??  should instead check for "is Inventory" or similar??
      if (fnode->pos == castle_flag_pos && d == DirectionUpLeft) {
      AILogDebug["find_flag_and_tile_dist"] << name << " skipping UpLeft dir because fnode->pos == castle_flag_pos, to avoid what appears to be a bug";
        continue;
      }
      if (map->has_path(fnode->pos, d)) {
        //ai_mark_pos->erase(map->move(fnode->pos, d));
        //ai_mark_pos->insert(ColorDot(map->move(fnode->pos, d), "gray"));
        //std::this_thread::sleep_for(std::chrono::milliseconds(0));

        // NOTE - if the solution is found here... can't we just quit and return it rather than continuing with this node?  there can't be a better one, right?

        AILogDebug["find_flag_and_tile_dist"] << name << " about to call trace_existing_road for fnode->pos " << fnode->pos << ", dir " << d;
        Road fsearch_road = trace_existing_road(map, fnode->pos, d);
        MapPos new_pos = fsearch_road.get_end(map.get());
        //ai_mark_pos->erase(new_pos);
        //ai_mark_pos->insert(ColorDot(new_pos, "dk_gray"));
        //std::this_thread::sleep_for(std::chrono::milliseconds(0));
        //Direction end_dir = reverse_direction(fsearch_road.get_last());
        AILogDebug["find_flag_and_tile_dist"] << name << "fsearchnode - fsearch from fnode->pos " << fnode->pos << " and dir " << NameDirection[d] << name << " found flag at pos " << new_pos << " with return dir " << reverse_direction(fsearch_road.get_last());

        // break early if this is the target_flag, run the usual new-node code here that would otherwise be run after in_closed and in_open checks
        //   wait, why duplicate this and break?  it should find that it is NOT in_closed and NOT in_open, move on to creating a new node and complete
        //    maybe comment this stuff out
        //   YES this fixes a huge bug!  leaving it this way
        if (new_pos == target_pos) {
          //AILogVerbose["find_flag_and_tile_dist"] << name << "fsearchnode - fnode at new_pos " << new_pos << " *IS* target_pos " << target_pos << ", breaking early";
          AILogDebug["find_flag_and_tile_dist"] << name << "fsearchnode - fnode at new_pos " << new_pos << " *IS* target_pos " << target_pos << ", FYI only, NOT breaking early (debug)";
          //ai_mark_pos->erase(new_pos);
          //ai_mark_pos->insert(ColorDot(new_pos, "green"));
          //std::this_thread::sleep_for(std::chrono::milliseconds(0));
          /*  trying this.. commenting this out and letting it flow to new node as usual
          PFlagSearchNode new_fnode(new FlagSearchNode);
          new_fnode->pos = new_pos;
          new_fnode->parent = fnode;
          new_fnode->flag_dist = new_fnode->parent->flag_dist + 1;
          // when doing a flag search, the parent node's direction *cannot* be inferred by reversing its child node dir
          //   like when doing tile pathfinding, so it must be set explicitly set. it will be reused and reset into each explored direction
          //   but that should be okay because once an end solution is found the other directions no longer matter
          fnode->dir = d;
          open.push_back(new_fnode);
          std::push_heap(open.begin(), open.end(), flagsearch_node_less);
          break;
          */
        }
        // check if this flag is already in closed list
        bool in_closed = false;
        for (PFlagSearchNode closed_node : closed) {
          if (closed_node->pos == new_pos) {
            in_closed = true;
            AILogDebug["find_flag_and_tile_dist"] << name << "fsearchnode - fnode at new_pos " << new_pos << ", breaking because in fnode already in_closed";
            //ai_mark_pos->erase(new_pos);
            //ai_mark_pos->insert(ColorDot(flag_pos, "dk_red"));
            //std::this_thread::sleep_for(std::chrono::milliseconds(0));
            break;
          }
        }
        if (in_closed) continue;

        AILogDebug["find_flag_and_tile_dist"] << name << "fsearchnode - fnode at new_pos " << new_pos << ", continuing because fnode NOT already in_closed";

        // check if this flag is already in open list
        bool in_open = false;
        for (std::vector<PFlagSearchNode>::iterator it = open.begin();
          it != open.end(); ++it) {
          PFlagSearchNode n = *it;
          if (n->pos == new_pos) {
            in_open = true;
            AILogDebug["find_flag_and_tile_dist"] << name << "fnodesearch - fnode at new_pos " << new_pos << " is already in_open ";
            if (n->flag_dist >= fnode->flag_dist + 1) {
              AILogDebug["find_flag_and_tile_dist"] << name << "fnodesearch - doing some flag dist compare I don't understand, sorting by flag_dist??";
              n->flag_dist += 1;
              n->parent = fnode;
              iter_swap(it, open.rbegin());
              std::make_heap(open.begin(), open.end(), flagsearch_node_less);
            }
            //ai_mark_pos->erase(new_pos);
            //ai_mark_pos->insert(ColorDot(new_pos, "dk_green"));
            //std::this_thread::sleep_for(std::chrono::milliseconds(0));
            break;  // should this be continue like if (in_closed) ???  NO, because it means we've checked every direction ... right?
          }
        }
        // this pos is not known yet, create a new fnode for it
        if (!in_open) {
          AILogDebug["find_flag_and_tile_dist"] << name << "fnodesearch - fnode at new_pos " << new_pos << " is NOT already in_open, creating a new fnode ";
          //ai_mark_pos->erase(new_pos);
          //ai_mark_pos->insert(ColorDot(new_pos, "lt_green"));
          //std::this_thread::sleep_for(std::chrono::milliseconds(0));
          PFlagSearchNode new_fnode(new FlagSearchNode);
          new_fnode->pos = new_pos;
          new_fnode->parent = fnode;
          new_fnode->flag_dist = new_fnode->parent->flag_dist + 1;
          // when doing a flag search, the parent node's direction *cannot* be inferred by reversing its child node dir
          //   like when doing tile pathfinding, so it must be set explicitly set. it will be reused and reset into each explored direction
          //   but that should be okay because once an end solution is found the other directions no longer matter
          AILogError["find_flag_and_tile_dist"] << name << " debugging BLEG1, creating new node, current fnode->pos " << fnode->pos << " fnode->dir was " << fnode->dir << ", new_fnode->pos " << new_fnode->pos << " setting fnode->dir to d " << d;
          // attempt to fix bug, setting fnode->dir to d much earlier in the loop because it should be set in all cases, right?
          //  and if the loop exits before reaching here it is never set??? i dunno
          //fnode->dir = d;
          open.push_back(new_fnode);
          std::push_heap(open.begin(), open.end(), flagsearch_node_less);
          AILogDebug["find_flag_and_tile_dist"] << name << "fnodesearch - fnode at new_pos " << new_pos << " is NOT already in_open, DONE CREATING new fnode ";
        } // if !in_open
        AILogDebug["find_flag_and_tile_dist"] << name << "fnodesearch end of if(map->has_path)";
      } // if map->has_path(node->pos, d)
      AILogDebug["find_flag_and_tile_dist"] << name << "fnodesearch end of if dir has path Direction - did I find it??";
    } // foreach direction
    AILogDebug["find_flag_and_tile_dist"] << name << "fnodesearch end of foreach Direction cycle_dirs";
  } // while open flag-nodes remain to be checked
  AILogDebug["find_flag_and_tile_dist"] << name << "fnodesearch end of while open flag-nodes remain";

  AILogDebug["find_flag_and_tile_dist"] << name << " no flag-path solution found from flag_pos " << flag_pos << " to target_pos " << target_pos << ".  returning false";
  //return std::make_tuple(bad_score, bad_score, false);
  return false;
}



// return MapPosVector of the Inventory buildings nearest to the specified MapPos,
//  including castle, by straightline distance.  This allows "sharing" of a building by 
//  multiple Inventories if the building is not significantly closer to one (by some threshold)
// the purpose of this function is to allow tracking of military buildings that hold the
//  borders around a given Inventory (castle, warehouse/stock) so that they can be iterated
//  over.  Because it is not imperative that they map 1-to-1, overlap is allowed
MapPosVector
AI::find_nearest_inventories_to_military_building(MapPos pos) {
	// hardcoding this here for now, put it in some tuning vars later?
	unsigned int overlap_threshold = 8;  // 8 tiles
	AILogDebug["find_nearest_inventories_to_military_building"] << name << " inside find_nearest_inventory_to_military_building to pos " << pos << ", overlap_threshold " << overlap_threshold << ", currently selected inventory_pos is " << inventory_pos;
	MapPosVector closest_inventories = {};
	// get inventory distances by straight-line map distance only, ignoring roads, flags, obstacles, etc.
	unsigned int best_dist = bad_score;
	MapPosSet inventory_dists = {};
	for (MapPos inv_pos : stocks_pos) {
		unsigned int dist = AI::get_straightline_tile_dist(map, pos, inv_pos);
		AILogDebug["find_nearest_inventories_to_military_building"] << name << " straightline tile dist from pos " << pos << " to inventory_pos " << inv_pos << " is " << dist;
		inventory_dists.insert(std::make_pair(inv_pos, dist));
		if (dist < best_dist){
			AILogDebug["find_nearest_inventories_to_military_building"] << name << " inventory at inventory_pos " << inv_pos << " is the closest so far to pos " << pos << " , with dist " << dist;
			best_dist = dist;
		}
	}
	if (inventory_dists.size() == 0 || best_dist == bad_score) {
		AILogDebug["find_nearest_inventories_to_military_building"] << name << " no inventories found!  returning empty MapPosVector";
		return closest_inventories;
	}
	// create a vector of all inventories within the threshold of the best_dist
	for (std::pair<MapPos, unsigned int> pair : inventory_dists) {
    unsigned int inv_pos = pair.first;
		unsigned int dist = pair.second;
		if (dist < best_dist + overlap_threshold) {
			AILogDebug["find_nearest_inventories_to_military_building"] << name << " inventory at " << inv_pos << " has dist " << dist << ", which is within " << best_dist + overlap_threshold << " of pos " << pos << ", including in list";
			closest_inventories.push_back(inv_pos);
		}
	}
	AILogDebug["find_nearest_inventories_to_military_building"] << name << " done, closest_inventories list has " << std::to_string(closest_inventories.size()) << " items, best_dist is " << best_dist;
	return closest_inventories;
}



// replacing the original AI find_nearest_inventory() function with this one that has the same input/output of MapPos
//  instead of having to change all the calling code to use Flag* type or having all of the ugly
//  conversion code being done in the calling code
//
// return the MapPos of the Flag* of the Inventory (castle, stock/warehouse) that is the shortest
//  number of Flags away from the MapPos of the requested Flag or Building
//
//  this function accepts EITHER a Flag pos, OR a Building pos (for which it will check the flag)
//
//  - the Inventory must be accepting Resources
//  - no consideration for if the Inventory is accepting Serfs
//  ----- CHANGED ---------- no consideration of tile/straightline distance, only Flag distance matters-------
//      as of jan31 2021, now only returning true if Inventory is closest by BOTH flag and straightline dist
//       NEED TO ENSURE THIS DOESN'T BREAK THINGS THAT REQUIRE THIS FUNCTION TO FIND AN INVENTORY!
//       It is likely to fail a lot as both conditions might not be true for many Flags
//  - no check to see if transporters/sailors are already in place along the path
//
// the intention of this function is to help "pair" most buildings in the AI player's realm to a
// given Inventory so that each Inventory can have accurate counts and limits of the buildings
// "paired" with that Inventory. The reason that straightline distance cannot be used (as it 
//  originally was) is that only Flag distance determines which Inventory a resource-producing
//  -building will send non-directly-routable resources to (i.e. ones piling up in storage)
//  If straightline-dist is used when determining nearest Inventory, but flag distance is used when
//   serfs transport resources to Inventories, the Inventory that receives the resouce may not be 
//   the Inventory "paired" with the building, and so the "stop XXX when resource limit reached"
//   checks would never be triggered because the check is running against one Inventory while
//   some other Inventory is actually piling up resources and may have already passed its limit.
//
//  the AI does not currently change these stock accept/reject/purge resource/serf settings so
//   I do not think these should introduce any issues, unless a human player modifies the settings
//    of an AI player's game?  Not sure if it matters much
//
//
// this will return bad_map_pos if there is no Flag at the requested pos
//  AND no Building at the requested pos

// this is now a stub that calls the two underlying functions depending on what DistType is set
MapPos
AI::find_nearest_inventory(PMap map, unsigned int player_index, MapPos pos, DistType dist_type, ColorDotMap *ai_mark_pos) {
  AILogDebug["util_find_nearest_inventory"] << name << " inside find_nearest_inventory to pos " << pos << " with dist_type " << NameDistType[dist_type];
  if (!map->has_flag(pos) && !map->has_building(pos)) {
		AILogWarn["util_find_nearest_inventory"] << name << " no flag or building found at pos " << pos << " as expected!  Cannot run search, returning bad_map_pos";
		return bad_map_pos;
	}
	MapPos flag_pos = bad_map_pos;
	if (map->has_building(pos)) {
		flag_pos = map->move_down_right(pos);
		AILogDebug["util_find_nearest_inventory"] << name << " request pos " << pos << " is a building pos, using its flag pos " << flag_pos << " instead";
	}
	else {
		flag_pos = pos;
	}
	Flag *flag = game->get_flag_at_pos(flag_pos);
	if (flag == nullptr) {
		AILogWarn["util_find_nearest_inventory"] << name << " got nullptr for Flag at flag_pos " << flag_pos << "!  Cannot run search, returning bad_map_pos";
		return bad_map_pos;
	}

  MapPos by_straightline_dist = bad_map_pos;
  if (dist_type == DistType::StraightLineOnly || dist_type == DistType::FlagAndStraightLine){
    by_straightline_dist = find_nearest_inventory_by_straightline(map, player_index, flag_pos, ai_mark_pos);
    if (by_straightline_dist == bad_map_pos){
      AILogDebug["util_find_nearest_inventory"] << name << " got bad_map_pos for by_straightline_dist, can't continue, returning bad_map_pos";
      return bad_map_pos;
    }
  }

  MapPos by_flag_dist = bad_map_pos;
  if (dist_type == DistType::FlagOnly || dist_type == DistType::FlagAndStraightLine){
    by_flag_dist = find_nearest_inventory_by_flag(map, player_index, flag_pos, ai_mark_pos);
    if (by_flag_dist == bad_map_pos){
      AILogDebug["util_find_nearest_inventory"] << name << " got bad_map_pos for by_flag_dist, can't continue, returning bad_map_pos";
      return bad_map_pos;
    }
  }

  if (dist_type == DistType::StraightLineOnly){
    AILogDebug["util_find_nearest_inventory"] << name << " returning by_straightline_dist " << by_straightline_dist;
    return by_straightline_dist;
  }

  if (dist_type == DistType::FlagOnly){
    AILogDebug["util_find_nearest_inventory"] << name << " returning by_flag_dist " << by_flag_dist;
    return by_flag_dist;
  }

  if (dist_type == DistType::FlagAndStraightLine){
    if (by_flag_dist != by_straightline_dist){
      AILogDebug["util_find_nearest_inventory"] << name << " nearest Inventory to flag_pos " << flag_pos << " by_flag_dist is " << by_flag_dist << ", but by_straightline_dist is " << by_straightline_dist << ", returning bad_map_pos";
      return bad_map_pos;
    }else{
      AILogDebug["util_find_nearest_inventory"] << name << " returning both matching pos " << by_straightline_dist;
      return by_straightline_dist;  // or could return by_flag_dist, same result
    }
  }
  AILogError["util_find_nearest_inventory"] << name << " this should never happen, returning bad_map_pos";
  return bad_map_pos;
}

MapPos
AI::find_nearest_inventory_by_straightline(PMap map, unsigned int player_index, MapPos flag_pos, ColorDotMap *ai_mark_pos) {
  AILogDebug["find_nearest_inventory_by_straightline"] << name << " inside find_nearest_inventory_by_straightline for flag_pos " << flag_pos;
  unsigned int shortest_dist = bad_score;
  MapPos closest_inv = bad_map_pos;
  AILogDebug["find_nearest_inventory_by_straightline"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings";
  game->get_mutex()->lock();
  AILogDebug["find_nearest_inventory_by_straightline"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings";
  Game::ListBuildings buildings = game->get_player_buildings(player);
  AILogDebug["find_nearest_inventory_by_straightline"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings";
  game->get_mutex()->unlock();
  AILogDebug["find_nearest_inventory_by_straightline"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings";
  for (Building *building : buildings) {
    if (building == nullptr)
      continue;
    if (building->is_burning())
      continue;
    if (building->get_type() != Building::TypeCastle && building->get_type() != Building::TypeStock)
      continue;
    Flag *building_flag = game->get_flag(building->get_flag_index());
    if (!building_flag->accepts_resources())
      continue;
    MapPos building_flag_pos = building_flag->get_position();
    unsigned int dist = (unsigned)get_straightline_tile_dist(map, flag_pos, building_flag_pos);
    if (dist >= shortest_dist)
      continue;
    AILogDebug["find_nearest_inventory_by_straightline"] << name << " SO FAR, the closest Inventory building to flag_pos " << flag_pos << " found at " << building_flag_pos;
    shortest_dist = dist;
    closest_inv = building_flag_pos;
    continue;
  }
  if (closest_inv != bad_map_pos){
    AILogDebug["find_nearest_inventory_by_straightline"] << name << " closest Inventory building to flag_pos " << flag_pos << " found at " << closest_inv;
  }else{
    AILogWarn["find_nearest_inventory_by_straightline"] << name << " closest Inventory building to flag_pos " << flag_pos << " did not find ANY valid Inventory building - was Castle and all Stocks destroyed???";
  }
  return closest_inv;
}


MapPos
AI::find_nearest_inventory_by_flag(PMap map, unsigned int player_index, MapPos flag_pos, ColorDotMap *ai_mark_pos) {
	AILogDebug["find_nearest_inventory_by_flag"] << name << " inside find_nearest_inventory_by_flag to flag_pos " << flag_pos;
	// now that this function uses the FlagSearchNode search instead of "internal" flag search
	//  it should probably be switched to operate on Flag* objects instead of MapPos
	//  to avoid so many MapPos<>Flag* conversions
	std::vector<PFlagSearchNode> open;
	std::list<PFlagSearchNode> closed;
	PFlagSearchNode fnode(new FlagSearchNode);
	fnode->pos = flag_pos;
	unsigned int flag_dist = 0;
	unsigned int tile_dist = 0;
	open.push_back(fnode);
	//AILogVerbose["find_nearest_inventory_by_flag"] << name << " fsearchnode - starting fnode search for flag_pos " << flag_pos;
	while (!open.empty()) {
		std::pop_heap(open.begin(), open.end(), flagsearch_node_less);
		fnode = open.back();
		open.pop_back();
    //AILogDebug["find_nearest_inventory_by_flag"] << name << " fsearchnode - inside fnode search for flag_pos " << flag_pos << ", inside while-open-list-not-empty loop";

		if (game->get_flag_at_pos(fnode->pos)->accepts_resources()) {
      // to avoid crashes, handle discovering a newly built warehouse that just now became active
      //  after the most recent update_stocks run, and doesn't exist in stocks_pos yet
      if (stock_buildings.count(fnode->pos) == 0){
        //update_stocks_pos();
        // hmm this seems like a bad place to put this.. for now just skip this Inventory
        //  and let the next AI loop find it
        AILogDebug["find_nearest_inventory_by_flag"] << name << " found a newly active Inventory building at " << fnode->pos << " that is not tracked yet, skipping it for now.";
      }else{
        // an Inventory building's flag reached, solution found
        AILogDebug["find_nearest_inventory_by_flag"] << name << " flagsearch complete, found solution from flag_pos " << flag_pos << " to an Inventory building's flag";

        // NONE of this backtracking is required for this type of search because all that matters is the dest pos, NOT the path to reach it
        // HOWEVER, if we want to track the flag_dist to the Inventory all that needs to be done is:
        // ***********************************
        //		while (fnode->parent){
        //			fnode = fnode->parent;
        //			flag_dist++;
        //		}
        // ***********************************
        //while (fnode->parent) {
          //MapPos end1 = fnode->parent->pos;
          //MapPos end2 = bad_map_pos;
          //Direction dir1 = fnode->parent->dir;
          //Direction dir2 = DirectionNone;
          // is this needed?
          //existing_road = trace_existing_road(map, end1, dir1);
          //end2 = existing_road.get_end(map.get());
          //fnode = fnode->parent;
          //flag_dist++;
        //}

        AILogDebug["find_nearest_inventory_by_flag"] << name << " done find_nearest_inventory_by_flag, found solution, returning Inventory flag pos " << fnode->pos;
        //return true;
        // this needs to return the MapPos of the inventory flag (update it later to get rid of all these conversions)
        return fnode->pos;
      }
		}

    //AILogDebug["find_nearest_inventory_by_flag"] << name << " fsearchnode - fnode->pos " << fnode->pos << " is not at an Inventory building flag yet, adding fnode to closed list";
		closed.push_front(fnode);

		// for each direction that has a path, trace the path until a flag is reached
		for (Direction d : cycle_directions_cw()) {
			if (map->has_path(fnode->pos, d)) {
				// couldn't this use the internal flag->other_end_dir stuff instead of tracing it?
				// maybe...  try that after this is stable
				Road fsearch_road = trace_existing_road(map, fnode->pos, d);
				MapPos new_pos = fsearch_road.get_end(map.get());
				//AILogDebug["find_nearest_inventory_by_flag"] << name << " fsearchnode - fsearch from fnode->pos " << fnode->pos << " and dir " << NameDirection[d] << " found flag at pos " << new_pos << " with return dir " << reverse_direction(fsearch_road.get_last());
				// check if this flag is already in closed list
				bool in_closed = false;
				for (PFlagSearchNode closed_node : closed) {
					if (closed_node->pos == new_pos) {
						in_closed = true;
            //AILogDebug["find_nearest_inventory_by_flag"] << name << " fsearchnode - fnode at new_pos " << new_pos << ", breaking because in fnode already in_closed";
						break;
					}
				}
				if (in_closed) continue;

        //AILogDebug["find_nearest_inventory_by_flag"] << name << " fsearchnode - fnode at new_pos " << new_pos << ", continuing because fnode NOT already in_closed";

				// check if this flag is already in open list
				bool in_open = false;
				for (std::vector<PFlagSearchNode>::iterator it = open.begin(); it != open.end(); ++it) {
					PFlagSearchNode n = *it;
					if (n->pos == new_pos) {
						in_open = true;
            //AILogDebug["find_nearest_inventory_by_flag"] << name << " fnodesearch - fnode at new_pos " << new_pos << " is already in_open ";
						if (n->flag_dist >= fnode->flag_dist + 1) {
              //AILogDebug["find_nearest_inventory_by_flag"] << name << " fnodesearch - new_pos " << new_pos << "'s flag_dist is >= fnode->flag_dist + 1";
							n->flag_dist += 1;
							n->parent = fnode;
							iter_swap(it, open.rbegin());
							std::make_heap(open.begin(), open.end(), flagsearch_node_less);
						}
						break;
					}
				}
				// this pos has not been seen before, create a new fnode for it
        //
        // SHOULD WE NOT CHECK FOR THE SUCCESS CONDITION (found inventory) AND QUIT IF SO???  OR AT LEAST BREAK EARLY
        //   maybe, but that could screw up the whole in_open  and flagsearch_node_less compare stuff?
				if (!in_open) {
          //AILogDebug["find_nearest_inventory_by_flag"] << name << " fnodesearch - fnode at new_pos " << new_pos << " is NOT already in_open, creating a new fnode ";
					PFlagSearchNode new_fnode(new FlagSearchNode);
					new_fnode->pos = new_pos;
					new_fnode->parent = fnode;
					new_fnode->flag_dist = new_fnode->parent->flag_dist + 1;
					// when doing a flag search, the parent node's direction *cannot* be inferred by reversing its child node dir
					//   like when doing tile pathfinding, so it must be set explicitly set. it will be reused and reset into each explored direction
					//   but that should be okay because once an end solution is found the other directions no longer matter
					fnode->dir = d;
					open.push_back(new_fnode);
					std::push_heap(open.begin(), open.end(), flagsearch_node_less);
					//AILogVerbose["find_nearest_inventory_by_flag"] << name << " fnodesearch - fnode at new_pos " << new_pos << " is NOT already in_open, DONE CREATING new fnode ";
				} // if !in_open
				//AILogVerbose["find_nearest_inventory_by_flag"] << name << " fnodesearch end of if(map->has_path)";
			} // if map->has_path(node->pos, d)
			//AILogVerbose["find_nearest_inventory_by_flag"] << name << " fnodesearch end of if dir has path Direction - did I find it??";
		} // foreach direction
		//AILogVerbose["find_nearest_inventory_by_flag"] << name << " fnodesearch end of foreach Direction cycle_dirs";
	} // while open flag-nodes remain to be checked
	//AILogVerbose["find_nearest_inventory_by_flag"] << name << " fnodesearch end of while open flag-nodes remain";

	// if the search ended it means nothing was found, return bad_map_pos
	AILogDebug["find_nearest_inventory_by_flag"] << name << " no flag-path solution found from flag_pos " << flag_pos << " to an Inventory building's flag.  returning false";
	return bad_map_pos;
}


// For every flag in this realm
//  - do a flagsearch starting from the flag to the nearest Inventory
//  - trace the flag-path between the starting flag and Inventory flag
//     and note the last Direction into the flag
//     and increment the "found" counter for each flag pos in the path
//  - keep a separate flag score count for each Dir into the Inventory flag
//  the resulting numbers should indicate the flags that are seen most often, meaning
//  they are part of the aterial road for that Inventory Flag-Direction
// Also
//  - store this overall Road (even though it passes through multiple flags - a Road is just an ordered array of Dirs
//    and maybe also store the Flag path taken?
//  - build a list of separate road networks to identify
//     road segments that are disconnected from rest of the road system[s]
//
// would it make more sense to have each solution just walk backwards and add + to the flag as it retraces?
//  rather than just adding +1 to every flag touched?
//  yes I think so! trying that instead
void
AI::identify_arterial_roads(PMap map){
  /*

  IMPORTANT - I suspect there is a significant bug in all of my Flagsearch logic
   When the flagsearch_node_less re-orders the list, it breaks the relationship
   between a fnode->dir parent and its child fnode, because the fnode->dir is
   re-used constantly.  At first I was thinking the child should store the dir
   that the parent reached it from, but I think that could also change(?)
   to be safe and clear, 
    //  MAYBE - faster?? change the flagsearch node stuff to use actual Flag
     objects, and the flag->get_other_end_dir //
   store the flag_pos in each valid Direction on a parent fnode, instead
   of only a single direction

  Once this is solved... go back and see if this bug exists on the existing code I wrote

  nov 2021 - might have fixed this when doing two things, 1) reversed the flagsearch_node_less
    and 2) added assigning fnode->dir = d earlier in those functions in case it quits early 
    there won't be child nodes whose parent dir is -1 DirectionNone (i.e. null default)

  */

  AILogDebug["util_identify_arterial_roads"] << name << " inside AI::identify_arterial_roads";
  // time this function for debugging
  std::clock_t start;
  double duration;

  // store the number of times each flag appears in the best path to the
  //  Inventory approach in this Direction
  // key MapPos           - of Inventory flag
  //  key Direction       - of path into Inventory flag
  //   key MapPos         - of Flag being counted
  //    val unsigned int  - number of times Flag appears in this InvFlag-Path combination
  std::map<MapPos,std::map<Direction,std::map<MapPos, unsigned int>>> flag_counts = {};

  flags_static_copy = *(game->get_flags());
  flags = &flags_static_copy;
  for (Flag *flag : *flags) {
    if (flag == nullptr)
      continue;
    if (flag->get_owner() != player_index)
      continue;
    if (!flag->is_connected())
      continue;
    // don't do searches on Inventory flags (castle, warehouse/stocks)
    if (flag->accepts_resources())
      continue;
    MapPos flag_pos = flag->get_position();
    //AILogDebug["util_identify_arterial_roads"] << name << " checking flag at pos " << flag_pos;

    std::vector<PFlagSearchNode> open = {};
    std::list<PFlagSearchNode> closed = {};
    PFlagSearchNode fnode(new FlagSearchNode);
    
    fnode->pos = flag_pos;
    open.push_back(fnode);
    std::push_heap(open.begin(), open.end(), flagsearch_node_less);
    int flag_dist = 0;
    bool found_inv = false;

    while (!open.empty()) {
      std::pop_heap(open.begin(), open.end(), flagsearch_node_less);
      fnode = open.back();
      open.pop_back();
      //AILogDebug["util_identify_arterial_roads"] << name << " fsearchnode - inside fnode search for flag_pos " << flag_pos << ", inside while-open-list-not-empty loop";

      if (game->get_flag_at_pos(fnode->pos)->accepts_resources()) {
        // to avoid crashes, handle discovering a newly built warehouse that just now became active
        //  after the most recent update_stocks run, and doesn't exist in stocks_pos yet
        if (stock_buildings.count(fnode->pos) == 0){
          //update_stocks_pos();
          // hmm this seems like a bad place to put this.. for now just skip this Inventory
          //  and let the next AI loop find it
          AILogDebug["util_identify_arterial_roads"] << name << " found a newly active Inventory building at " << fnode->pos << " that is not tracked yet, skipping it for now.";
        }else{
          //AILogDebug["util_identify_arterial_roads"] << name << " flagsearch solution found from flag_pos " << flag_pos << " to an Inventory building's flag";
          found_inv = true;
          // uniquely identify the connection point of each artery to the Flag-Dir it is coming from
          // this is the dir of 2nd last node, which leads to the last node (which has no dir)
          if (fnode->parent == nullptr){
            AILogDebug["util_identify_arterial_roads"] << name << " no parent flag node found!  this must be the Inventory pos, breaking";
            break;
          }
          MapPos inv_flag_pos = fnode->pos;
          //AILogDebug["util_identify_arterial_roads"] << name << " DEBUG:::::::::   inv_flag_pos = " << inv_flag_pos << " when recording flag dirs with paths from inv";
          /* wait, this doesn't even matter
            at this point we don't actually need to track the dirs between flags
            as we can just check the dir between the last two nodes
            THOUGH I guess it is possible that there could be multiple direct
            connections from the second to last flag to the end Inv flag, but
            if so the end result would be pretty much the same, right? so what
            if the very last flag segment is wrong or random*/
          // NEVERMIND, keeping it this way for now
          Direction inv_flag_conn_dir = DirectionNone;
          for (Direction d : cycle_directions_ccw()){
            if (fnode->parent->child_dir[d] == fnode->pos){
              // this is the dir from the Flag of 2nd-to-last to Inv pos, 
              //  NOT the dir at the very last tile/path to Inv pos!
              //inv_flag_conn_dir = d;
              // get the actual connection dir on the Inv flag side
              //  by trusting that the Flag->get_other_end_dir works
              //AILogDebug["util_identify_arterial_roads"] << name << " the 2nd-to-last flag at pos " << fnode->parent->pos << " has path TO inv_flag_pos " << inv_flag_pos << " in dir " << NameDirection[d] << " / " << d;
              inv_flag_conn_dir = game->get_flag_at_pos(fnode->parent->pos)->get_other_end_dir(d);
            }
          }
          //
          // NOTE - I started seeing this error appear after I "fixed" the flagsearch_node_less function
          //  where I reversed the ordering so it does breadth-frist instead of depth-first
          //  there is probably a bug in this code, or in the flagsearch logic, but because I am not actually using
          //  arterial roads for anything yet I will simply disable it for now and figure the bug out later
          // MIGHT BE FIXED NOW - renabled and testing
          // nope still broken, trying same fix as used elsewhere, setting fnode->dir = d earlier
          // wait that isn't even done here, this function is written differently.  look into why, one of them is probably wrong
          //
          if (inv_flag_conn_dir == DirectionNone){
            AILogError["util_identify_arterial_roads"] << name << " could not find the Dir from fnode->parent->pos " << fnode->parent->pos << " to child fnode->pos " << fnode->pos << "! throwing exception";
            //std::this_thread::sleep_for(std::chrono::milliseconds(120000));
            throw ExceptionFreeserf("in AI::util_identify_arterial_roads, could not find the Dir from fnode->parent->pos to child !fnode->pos!  it should be known");
          }
          // actually, I take it back, tracking it the entire way is fine
          //  if the alternative is checking 6 dirs and potentially buggy
          //Direction inv_flag_conn_dir = trace...
          //AILogDebug["util_identify_arterial_roads"] << name << " reached an Inventory building's flag at pos " << inv_flag_pos << ", connecting from dir " << NameDirection[inv_flag_conn_dir] << " / " << inv_flag_conn_dir;
          while (fnode->parent){
            //AILogDebug["util_identify_arterial_roads"] << name << " fnode->pos = " << fnode->pos << ", fnode->parent->pos " << fnode->parent->pos << ", fnode->parent->dir = " << NameDirection[fnode->parent->dir] << " / " << fnode->parent->dir;
            fnode = fnode->parent;
            flag_counts[inv_flag_pos][inv_flag_conn_dir][fnode->pos]++;
            flag_dist++;
          }
          //AILogDebug["util_identify_arterial_roads"] << name << " flag_dist from flag_pos " << flag_pos << " to nearest Inventory pos " << inv_flag_pos << " is " << flag_dist;
          break;
        }
      }

      //AILogDebug["util_identify_arterial_roads"] << name << " fsearchnode - fnode->pos " << fnode->pos << " is not at an Inventory building flag yet, adding fnode to closed list";
      closed.push_front(fnode);

      // for each direction that has a path, trace the path until a flag is reached
      for (Direction d : cycle_directions_cw()) {
        if (map->has_path(fnode->pos, d)) {
          // NOTE!!!!
          // does this need teh "castle appears to have a path" fix?
          //

          // couldn't this use the internal flag->other_end_dir stuff instead of tracing it?
          // maybe...  try that after this is stable
          Road fsearch_road = trace_existing_road(map, fnode->pos, d);
          MapPos new_pos = fsearch_road.get_end(map.get());
          //AILogDebug["util_identify_arterial_roads"] << name << " fsearchnode - fsearch from fnode->pos " << fnode->pos << " and dir " << NameDirection[d] << " found flag at pos " << new_pos << " with return dir " << reverse_direction(fsearch_road.get_last());
          // check if this flag is already in closed list
          bool in_closed = false;
          for (PFlagSearchNode closed_node : closed) {
            if (closed_node->pos == new_pos) {
              in_closed = true;
              //AILogDebug["util_identify_arterial_roads"] << name << " fsearchnode - fnode at new_pos " << new_pos << ", breaking because in fnode already in_closed";
              break;
            }
          }
          if (in_closed) continue;

          //AILogDebug["util_identify_arterial_roads"] << name << " fsearchnode - fnode at new_pos " << new_pos << ", continuing because fnode NOT already in_closed";

          // check if this flag is already in open list
          bool in_open = false;
          for (std::vector<PFlagSearchNode>::iterator it = open.begin(); it != open.end(); ++it) {
            PFlagSearchNode n = *it;
            if (n->pos == new_pos) {
              in_open = true;
              //AILogDebug["util_identify_arterial_roads"] << name << " fnodesearch - fnode at new_pos " << new_pos << " is already in_open ";
              if (n->flag_dist >= fnode->flag_dist + 1) {
                AILogDebug["util_identify_arterial_roads"] << name << " fnodesearch - new_pos " << new_pos << "'s flag_dist is >= fnode->flag_dist + 1";
                n->flag_dist += 1;
                n->parent = fnode;
                iter_swap(it, open.rbegin());
                std::make_heap(open.begin(), open.end(), flagsearch_node_less);
              }
              break;
            }
          }
          // this pos has not been seen before, create a new fnode for it
          //
          // SHOULD WE NOT CHECK FOR THE SUCCESS CONDITION (found inventory) AND QUIT IF SO???  OR AT LEAST BREAK EARLY
          //   maybe, but that could screw up the whole in_open  and flagsearch_node_less compare stuff?
          if (!in_open) {
            //AILogDebug["util_identify_arterial_roads"] << name << " fnodesearch - fnode at new_pos " << new_pos << " is NOT already in_open, creating a new fnode ";
            PFlagSearchNode new_fnode(new FlagSearchNode);
            new_fnode->pos = new_pos;
            new_fnode->parent = fnode;
            new_fnode->flag_dist = new_fnode->parent->flag_dist + 1;
            // when doing a flag search, the parent node's direction *cannot* be inferred by reversing its child node dir
            //   like when doing tile pathfinding, so it must be set explicitly set by associating the parent node Dir with the child node pos
            fnode->child_dir[d] = new_pos; // wait WHY IS THIS DONE DIFFERENTLY THAN IN MY OTHER SEARCHES?  one is likely wrong!
            open.push_back(new_fnode);
            std::push_heap(open.begin(), open.end(), flagsearch_node_less);
          } // if !in_open
        } // if map->has_path(node->pos, d)
      } // foreach direction
    } // while open flag-nodes remain to be checked

    // if the search ended it means no other Inventory building was found, so this road is likely not connected to the main
    //  road system.  That might be okay
    if (!found_inv)
      AILogDebug["util_identify_arterial_roads"] << name << " flagsearch never completed, flag at pos " << flag_pos << " is not connected to any valid Inventory!";

  } // foreach Flag pos

  // dump the entire search results
  for (std::pair<MapPos,std::map<Direction,std::map<MapPos, unsigned int>>>  inv_pair : flag_counts){
    MapPos inv_flag_pos = inv_pair.first;
    AILogDebug["util_identify_arterial_roads"] << name << " DUMP inv_pos " << inv_flag_pos;
    for (std::pair<Direction,std::map<MapPos, unsigned int>> dir_pair : inv_pair.second){
      Direction dir = dir_pair.first;
      AILogDebug["util_identify_arterial_roads"] << name << " DUMP         dir " << NameDirection[dir] << " / " << dir;
      for (std::pair<MapPos, unsigned int> flag_count_pair : dir_pair.second){
        MapPos flag_pos = flag_count_pair.first;
        unsigned int count = flag_count_pair.second;
        AILogDebug["util_identify_arterial_roads"] << name << " DUMP                 flag_pos " << flag_pos << " seen " << count << " times";
      }
    }
  }

  // record the arterial flags
  for (std::pair<MapPos,std::map<Direction,std::map<MapPos, unsigned int>>>  inv_pair : flag_counts){
    MapPos inv_flag_pos = inv_pair.first;
    AILogDebug["util_identify_arterial_roads"] << name << " MEDIAN inv_flag_pos " << inv_flag_pos;
    for (std::pair<Direction,std::map<MapPos, unsigned int>> dir_pair : inv_pair.second){
      Direction dir = dir_pair.first;
      AILogDebug["util_identify_arterial_roads"] << name << " MEDIAN         dir " << NameDirection[dir] << " / " << dir;

      // find the median "number of times a flag appears in the Inventory flag's road network in this direction"
      //  and use this as the cutoff for a flag to be "arterial"
      std::vector<unsigned int> counts = {};
      for (std::pair<MapPos, unsigned int> flag_count_pair : dir_pair.second){
        MapPos flag_pos = flag_count_pair.first;
        unsigned int count = flag_count_pair.second;
        counts.push_back(count);
        AILogDebug["util_identify_arterial_roads"] << name << " MEDIAN                 flag_pos " << flag_pos << " count " << count;
      }
      sort(counts.begin(), counts.end());
      size_t size = counts.size();
      size_t median = 0;
      // middle record is median value
      //median = counts[size / 2];
      // changing this to 70th percentile
      median = counts[size * 0.7];
      AILogDebug["util_identify_arterial_roads"] << name << " the median count " << median;

      // build MapPosVector of all flags that are > median, this is the arterial flag-path 
      //  in this direction from this Inventory
      MapPosVector art_flags = {};
      for (std::pair<MapPos, unsigned int> flag_count_pair : dir_pair.second){
        MapPos flag_pos = flag_count_pair.first;
        unsigned int count = flag_count_pair.second;
        if (count > median){
          AILogDebug["util_identify_arterial_roads"] << name << " MEDIAN                 flag_pos " << flag_pos << " count " << count << " is above median, including";
          art_flags.push_back(flag_pos);
        }
      }
      //    key pos/dir -> val unordered(?) list of flags, assume starting at the Inventory
      //    typedef std::map<std::pair<MapPos, Direction>, MapPosVector> FlagDirToFlagVectorMap;
      ai_mark_arterial_road_flags->insert(std::make_pair(std::make_pair(inv_flag_pos, dir), art_flags));
      //    key pos/dir -> val ordered list of flag-dir pairs, assume starting at the Inventory
      //    typedef std::map<std::pair<MapPos, Direction>, std::vector<std::pair<MapPos,Direction>>> FlagDirToFlagDirVectorMap;
      //ai_mark_arterial_road_pairs->insert(std::make_pair(std::make_pair(inv_flag_pos, dir), new std::vector<std::pair<MapPos,Direction>>));
      std::vector<std::pair<MapPos,Direction>> foo = {};
      ai_mark_arterial_road_pairs->insert(std::make_pair(std::make_pair(inv_flag_pos, dir), foo));

    } // foreach dir_pair : inv_pair.second
  } // foreach inv_pair : flag_counts

  // record the flag->Dir combinations so they can be traced along tile-paths
  //  and highlighted inside Viewport
  // to do this, run a special depth-first flagsearch from the inv_pos until
  //  all arterial flags reached and mark the Dir from each parent to child
  // WHAT ABOUT CIRCULAR PATHS??  I dunno, let's see what happens
  for (std::pair<std::pair<MapPos, Direction>, MapPosVector> record : *(ai_mark_arterial_road_flags)){
    std::pair<MapPos, Direction> flag_dir = record.first;
    MapPos inv_flag_pos = flag_dir.first;
    Direction dir = flag_dir.second;
    AILogDebug["util_identify_arterial_roads"] << name << " begin retracing solutions - Inventory at pos " << inv_flag_pos << " has a path in Dir " << NameDirection[dir] << " / " << dir;

    Flag *inv_flag = game->get_flag_at_pos(inv_flag_pos);
    if (inv_flag == nullptr)
      continue;
    if (inv_flag->get_owner() != player_index)
      continue;
    if (!inv_flag->is_connected())
      continue;

    AILogDebug["util_identify_arterial_roads"] << name << " retracing solutions for inv_flag_pos " << inv_flag_pos << " dir " << NameDirection[dir] << " / " << dir << " - starting flagsearch from inv_flag_pos " << inv_flag_pos;
    MapPosVector closed = {};
    MapPosVector *closed_ptr = &closed;
    int depth = 0;
    int *depth_ptr = &depth;
    arterial_road_depth_first_recursive_flagsearch(inv_flag_pos, flag_dir, closed_ptr, depth_ptr);

    AILogDebug["util_identify_arterial_roads"] << name << " done retracing solutions - Inventory at pos " << inv_flag_pos << " has a path in Dir " << NameDirection[dir] << " / " << dir;
  }

  start = std::clock();
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_identify_arterial_roads"] << name << " done AI::identify_arterial_roads, call took " << duration;
}

//  instead of retracing backwards from last node to parent node as is usual,
//    start at oldest node and trace downwards to each child until it dead ends
//    using a depth-first recursive walk
//     and record the Set as parent->child flag pos  so each line only appears once
//   otherwise, if we did something like this:
//  for each arterial flag (start at Inventory) 
//    foreach cycle_dirs
//      if flag has path in dir that ends at another arterial flag
//         draw the line
//  .. this would result in drawing lines in both directions for each flag
void
AI::arterial_road_depth_first_recursive_flagsearch(MapPos flag_pos, std::pair<MapPos,Direction> inv_dir, MapPosVector *closed, int *depth){
  (*depth)++;
  //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << name << " starting with flag_pos " << flag_pos << ", recusion depth " << *(depth);
  MapPosVector arterial_flags = ai_mark_arterial_road_flags->at(inv_dir);
  //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << name << " foo0";
  // find the Dir from this flag to the next flag and see if it is an arterial flag
  for (Direction dir : cycle_directions_cw()){
    //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << name << " foo1, dir " << dir;
    if (!map->has_path(flag_pos, dir))
      continue;
    if (!map->has_flag(flag_pos)){
      AILogError["arterial_road_depth_first_recursive_flagsearch"] << name << " expecting that start_pos provided to this function is a flag pos, but game->get_flag_at_pos(" << flag_pos << ") returned nullptr!  throwing exception";
      throw ExceptionFreeserf("expecting that start_pos provided to this function is a flag pos, but game->get_flag_at_pos(flag_pos)!");
    }
    Flag *flag = game->get_flag_at_pos(flag_pos);
    Flag *other_end_flag = flag->get_other_end_flag(dir);
    if (other_end_flag == nullptr){
      AILogWarn["arterial_road_depth_first_recursive_flagsearch"] << name << " got nullptr for game->get_other_end_flag(" << NameDirection[dir] << ") from flag at pos " << flag_pos << " skipping this dir";
      continue;
    }
    //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << name << " foo2";
    MapPos other_end_flag_pos = other_end_flag->get_position();
    // if other_end_flag_pos is on the arterial flag list for this Inventory-Dir
    //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << name << " foo3";
    if (std::find(arterial_flags.begin(), arterial_flags.end(), other_end_flag_pos) != arterial_flags.end()){
      //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << name << " foo4";
      if (std::find(closed->begin(), closed->end(), other_end_flag_pos) == closed->end()){
        //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << name << " foo5+ closing pos";
        closed->push_back(other_end_flag_pos);
        //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << name << " foo5+ pushing pair to vector";
        ai_mark_arterial_road_pairs->at(inv_dir).push_back(std::make_pair(flag_pos,dir));
        // recursively search this new flag_pos the same way
        //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << name << " foo6 entering recurse call";
        arterial_road_depth_first_recursive_flagsearch(other_end_flag_pos, inv_dir, closed, depth);
      }else{
        //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << name << " foo5- pos is already on closed list";
      }
    } 
  }
  //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << name << " done with node pos " << flag_pos;
}



//// instead try using the existing Flagsearch::execute function
////OH WAIT, now I remember why AI can't ever use the original FlagSearch function, because
////the game actually modifies the Flag data as it does a search because the original game
////is single-threaded!  Another thread cannot use the original Flagsearch function because 
////it will break the game.  Instead, must use my own Flagsearch function that does not modify Flags

// perform AI-style FlagSearch to find best flag-path from flag_pos to target_pos
//  return true if solution found, false if not
// ALSO provide the list of all Flags involved in the solution
// I think this function will NOT work for fake flags / splitting flags???

// after watching this a while, it seems to be doing a depth-first search which is not right
// fix it, and also check if the main find_flag_and_tile_dist function i think I copied from is
// also doing this wrong
// YES I think that is the answer... sometimes it seems to check the whole road network and othertimes it
//  just works, and it looks like it just works when the target flag happens to be on the next branch clockwise
// this needs to be changed to a breadth-first flag search!
// AND VERY LIKELY, THE FUNCTIONS I COPIED THIS FROM ARE ALSO WRONG!!!
bool
AI::find_flag_path_between_flags(PMap map, unsigned int player_index, MapPos end1, MapPos end2, MapPosVector *flags_found, ColorDotMap *ai_mark_pos) {
  AILogDebug["find_flag_path_between_flags"] << name << " inside find_flag_path_between_flags, end1 " << end1 << ", end2 " << end2;
  //ai_mark_pos->clear();
  //ai_mark_pos->erase(end1);
  //ai_mark_pos->insert(ColorDot(end1, "dk_green"));
  //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  std::vector<PFlagSearchNode> open;
  std::list<PFlagSearchNode> closed;
  PFlagSearchNode fnode(new FlagSearchNode);
  unsigned int flag_dist = 0;  // could just use size of flags_found, or compare both for sanity
  fnode->pos = end1;
  //ai_mark_pos->erase(end2);
  //ai_mark_pos->insert(ColorDot(end2, "red"));
  //std::this_thread::sleep_for(std::chrono::milliseconds(5000));

  open.push_back(fnode);
  while (!open.empty()) {
    std::pop_heap(open.begin(), open.end(), flagsearch_node_less);
    fnode = open.back();
    open.pop_back();

    AILogDebug["find_flag_path_between_flags"] << name << " start of while-not-done loop, fnode pos is " << fnode->pos;

    // check if solution found
    if (fnode->pos == end2) {
      AILogDebug["find_flag_path_between_flags"] << name << " found solution!";
      //ai_mark_pos->clear();
      //std::this_thread::sleep_for(std::chrono::milliseconds(500));
      //ai_mark_pos->erase(fnode->pos);
      //ai_mark_pos->insert(ColorDot(fnode->pos, "dk_red"));
      //std::this_thread::sleep_for(std::chrono::milliseconds(500));
      flags_found->push_back(fnode->pos);
      while (fnode->parent) {
        AILogDebug["find_flag_path_between_flags"] << name << " fnode has parent";
        //ai_mark_pos->erase(fnode->pos);
        //ai_mark_pos->insert(ColorDot(fnode->pos, "dk_red"));
        //std::this_thread::sleep_for(std::chrono::milliseconds(500));
        flags_found->push_back(fnode->parent->pos);
        fnode = fnode->parent;
        flag_dist++;  // should be able to use the last node's flag_dist and get same result... maybe compare both as sanity check
      }
      //std::this_thread::sleep_for(std::chrono::milliseconds(500));
      AILogDebug["find_flag_path_between_flags"] << name << " found solution, returning true, dist for road from end1 " << end1 << " to end2 " << end2 << " is " << flag_dist;
      return true;
    } // if found solution

    AILogDebug["find_flag_path_between_flags"] << name << " solution not yet found, putting this fnode in closed list";
    closed.push_front(fnode);

    for (Direction d : cycle_directions_cw()) {
      // attempt to fix bug, setting fnode->dir to d much earlier in the loop because it should be set in all cases, right?
      //  and if the loop exits before reaching here it is never set??? i dunno
      // this seems to be fixing it, actually
      fnode->dir = d;

      // attempt to work around the issue with castle flag appearing to have a path UpLeft from its flag into
      //  the castle, which breaks pathfinding
      //  is this an issue for Warehouses/Stocks, too??  should instead check for "is Inventory" or similar??
      if (fnode->pos == castle_flag_pos && d == DirectionUpLeft) {
      AILogDebug["find_flag_path_between_flags"] << name << " skipping UpLeft dir because fnode->pos == castle_flag_pos, to avoid what appears to be a bug";
        continue;
      }
      if (map->has_path(fnode->pos, d)) {
        Road fsearch_road = trace_existing_road(map, fnode->pos, d);
        MapPos new_pos = fsearch_road.get_end(map.get());
        //// don't mark if this is the start pos
        //if (new_pos != end1){
        //  ai_mark_pos->erase(new_pos);
        //  ai_mark_pos->insert(ColorDot(new_pos, "dk_gray"));
        //  std::this_thread::sleep_for(std::chrono::milliseconds(500));
        //}
        AILogVerbose["find_flag_path_between_flags"] << name << " fsearchnode - fsearch from fnode->pos " << fnode->pos << " and dir " << NameDirection[d] << name << " found flag at pos " << new_pos << " with return dir " << reverse_direction(fsearch_road.get_last());

        // break early if this is the target_flag, run the usual new-node code here that would otherwise be run after in_closed and in_open checks
        //   wait, why duplicate this and break?  it should find that it is NOT in_closed and NOT in_open, move on to creating a new node and complete
        //    maybe comment this stuff out
        //   YES this fixes a huge bug!  leaving it this way

        // NOTE that the search is still progressing a bit beyond when the target is reached and it should be possible to quit early,
        //  I think this logic should be added carefully, possibly skip straight to "create new node" and 'continue'?
        
        // check if this flag is already in closed list
        bool in_closed = false;
        for (PFlagSearchNode closed_node : closed) {
          if (closed_node->pos == new_pos) {
            in_closed = true;
            AILogVerbose["find_flag_path_between_flags"] << name << " fsearchnode - fnode at new_pos " << new_pos << ", breaking because in fnode already in_closed";
            //// don't mark if this is the start pos
            //if (new_pos != end1){
            //  ai_mark_pos->erase(new_pos);
            //  ai_mark_pos->insert(ColorDot(new_pos, "brown"));
            //  std::this_thread::sleep_for(std::chrono::milliseconds(500));
            //}
            break;
          }
        }
        if (in_closed) continue;

        AILogDebug["find_flag_path_between_flags"] << name << " fsearchnode - fnode at new_pos " << new_pos << ", continuing because fnode NOT already in_closed";

        // check if this flag is already in open list
        bool in_open = false;
        for (std::vector<PFlagSearchNode>::iterator it = open.begin();
          it != open.end(); ++it) {
          PFlagSearchNode n = *it;
          if (n->pos == new_pos) {
            in_open = true;
            AILogDebug["find_flag_path_between_flags"] << name << " fnodesearch - fnode at new_pos " << new_pos << " is already in_open ";
            if (n->flag_dist >= fnode->flag_dist + 1) {
              AILogDebug["find_flag_path_between_flags"] << name << " fnodesearch - doing some flag dist compare I don't understand, sorting by flag_dist??";
              n->flag_dist += 1;
              n->parent = fnode;
              iter_swap(it, open.rbegin());
              std::make_heap(open.begin(), open.end(), flagsearch_node_less);
            }
            //// don't mark if this is the start pos
            //if (new_pos != end1){
            //  ai_mark_pos->erase(new_pos);
            //  ai_mark_pos->insert(ColorDot(new_pos, "magenta"));
            //  std::this_thread::sleep_for(std::chrono::milliseconds(500));
            //}
            break;  // should this be continue like if (in_closed) ???  NO, because it means we've checked every direction ... right?
          }
        }
        // this pos is not known yet, create a new fnode for it
        if (!in_open) {
          AILogDebug["find_flag_path_between_flags"] << name << " fnodesearch - fnode at new_pos " << new_pos << " is NOT already in_open, creating a new fnode ";
          //// don't mark if this is the start pos
          // if (new_pos != end1){
          //  ai_mark_pos->erase(new_pos);
          //  ai_mark_pos->insert(ColorDot(new_pos, "yellow"));
          //  std::this_thread::sleep_for(std::chrono::milliseconds(500));
          //}
          PFlagSearchNode new_fnode(new FlagSearchNode);
          new_fnode->pos = new_pos;
          new_fnode->parent = fnode;
          new_fnode->flag_dist = new_fnode->parent->flag_dist + 1;
          // when doing a flag search, the parent node's direction *cannot* be inferred by reversing its child node dir
          //   like when doing tile pathfinding, so it must be set explicitly set. it will be reused and reset into each explored direction
          //   but that should be okay because once an end solution is found the other directions no longer matter
          fnode->dir = d;
          open.push_back(new_fnode);
          std::push_heap(open.begin(), open.end(), flagsearch_node_less);
          AILogDebug["find_flag_path_between_flags"] << name << " fnodesearch - fnode at new_pos " << new_pos << " is NOT already in_open, DONE CREATING new fnode ";
        } // if !in_open
        AILogDebug["find_flag_path_between_flags"] << name << " fnodesearch end of if(map->has_path)";
      } // if map->has_path(node->pos, d)
      AILogDebug["find_flag_path_between_flags"] << name << " fnodesearch end of if dir has path Direction - did I find it??";
    } // foreach direction
    AILogDebug["find_flag_path_between_flags"] << name << " fnodesearch end of foreach Direction cycle_dirs";
  } // while open flag-nodes remain to be checked
  AILogDebug["find_flag_path_between_flags"] << name << " fnodesearch end of while open flag-nodes remain";

  AILogDebug["find_flag_path_between_flags"] << name << " no flag-path solution found from end1 " << end1 << " to end2 " << end2 << ".  returning false";
  //return std::make_tuple(bad_score, bad_score, false);
  return false;
}
