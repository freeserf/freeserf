/*
 * ai_pathfinder.cc - extra pathfinder functions used for AI
 *     Copyright 2019-2021 tlongstretch
 *
 *     FlagSearch function mostly copied from Pyrdacor's Freeserf.NET
 *        C# implementation
 *
 */
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
  Direction dir = DirectionNone;          // dir to child flag   (are these still used?)
  Direction parent_dir = DirectionNone;  // dir back to parent flag   (are these still used?)
};

static bool
flagsearch_node_less(const PFlagSearchNode &left, const PFlagSearchNode &right) {
  // A search node is considered less than the other if
  // it has a *lower* flag distance.  This means that in the max-heap
// the shorter distance will go to the top
  return left->flag_dist < right->flag_dist;
}




// plot a Road between two pos and return it
//  *In addition*, while plotting that road, use any existing flags and anyplace that new flags
//    could be built on existing roads along the way to populate a set of additional potential Roads.
//    Any valid existing or potential flag pos encountered "in the way" during pathfinding
//    will be included, but not positions that are not organically checked by the direct pathfinding logic.
//    The set will include the original-specified-end direct Road if it is valid
// is road_options actually used here??? or only by build_best_road??
Road
AI::plot_road(PMap map, unsigned int player_index, MapPos start_pos, MapPos end_pos, Roads * const &potential_roads) {
  AILogDebug["plot_road"] << name << " inside plot_road for Player" << player_index << " with start " << start_pos << ", end " << end_pos;
  std::vector<PSearchNode> open;
  std::list<PSearchNode> closed;
  PSearchNode node(new SearchNode);
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
        std::this_thread::sleep_for(std::chrono::milliseconds(0));
        node = node->parent;
      }
      unsigned int new_length = static_cast<unsigned int>(breadcrumb_solution.get_length());
      AILogDebug["plot_road"] << name << "plot_road: solution found, new segment length is " << breadcrumb_solution.get_length();
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
          AILogDebug["plot_road"] << name << "reached max_fake_flags count " << max_fake_flags << ", not considering any more fake flag solutions";
          continue;
        }
        // split road found if can build a flag, and that flag is already part of a road (meaning it has at least one path)
        if (game->can_build_flag(new_pos, player) &&
          (map->has_path(new_pos, Direction(0)) || map->has_path(new_pos, Direction(1)) || map->has_path(new_pos, Direction(2)) ||
            map->has_path(new_pos, Direction(3)) || map->has_path(new_pos, Direction(4)) || map->has_path(new_pos, Direction(5)))) {
          fake_flags_count++;
          AILogDebug["plot_road"] << name << "plot_road: alternate/split_road solution found while pathfinding to " << end_pos << ", a new flag could be built at pos " << new_pos;
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
          AILogDebug["plot_road"] << name << "plot_road: split_flag_breadcrumb_solution found, new segment length is " << split_flag_breadcrumb_solution.get_length();
          // now inverse the entire road so that it stats at start_pos and ends at end_pos, so the Road object is logical instead of backwards
          Road split_flag_solution = reverse_road(map, split_flag_breadcrumb_solution);
          AILogDebug["plot_road"] << name << "plot_road: inserting alternate/fake flag Road solution to PotentialRoads, new segment length would be " << split_flag_solution.get_length();
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

  AILogDebug["plot_road"] << name << "done plot_road, found_direct_road: " << std::to_string(found_direct_road) << ".  additional potential_roads (split_roads): " << split_road_solutions;
  if (direct_road.get_source() == bad_map_pos) {
    AILogDebug["plot_road"] << name << "NO DIRECT SOLUTION FOUND for start_pos " << start_pos << " to end_pos " << end_pos;
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
  AILogDebug["get_straightline_tile_dist"] << name << "returning tile_dist: " << tile_dist;
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
    AILogDebug["score_flag"] << name << "flag not found at flag_pos " << flag_pos << ", assuming this is a fake flag/split road solution and using closest adjacent flag + tile dist from fake flag";
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
      AILogDebug["score_flag"] << name << "looking for a path in dir " << NameDirection[dir];
      if (!map->has_path(splitting_flag_pos, dir)) {
        continue;
      }
      AILogDebug["score_flag"] << name << "found a path in dir " << NameDirection[dir] << name << ", tracing road";
      Road split_road = trace_existing_road(map, splitting_flag_pos, dir);
      adjacent_flag_pos = split_road.get_end(map.get());
      unsigned int tiles_to_adjacent_flag = static_cast<unsigned int>(split_road.get_length());
      AILogDebug["score_flag"] << name << "split road in dir " << NameDirection[dir] << name << " has length " << tiles_to_adjacent_flag << " to adjacent flag at pos " << adjacent_flag_pos;
      unsigned int tile_dist = bad_score;
      unsigned int flag_dist = bad_score;
      unsigned int adjusted_score = bad_score;
      bool contains_castle_flag = false;
      // go score the adjacent flags if they aren't already known (sometimes they will already have been checked)
      if (!rb->has_score(adjacent_flag_pos)) {
        AILogDebug["score_flag"] << name << "score_flag, rb doesn't have score yet for adjacent flag_pos " << adjacent_flag_pos << ", will try to score it";
        if (find_flag_and_tile_dist(map, player_index, rb, adjacent_flag_pos, castle_flag_pos, ai_mark_pos)) {
          AILogDebug["score_flag"] << name << "score_flag, splitting_flag find_flag_and_tile_dist() returend true from flag_pos " << adjacent_flag_pos << " to target_pos " << target_pos;
        }
        else{
          AILogDebug["score_flag"] << name << "score_flag, splitting_flag find_flag_and_tile_dist() returned false, cannot find flag-path solution from flag_pos " << adjacent_flag_pos << " to target_pos " << target_pos << ".  Returning false";
          // if not found just let rb->get_score call fail and it wlil use the default bad_scores given.
          AILogDebug["score_flag"] << name << "score_flag returned false for adjacent flag at pos " << adjacent_flag_pos;
          AILogDebug["score_flag"] << name << "for now, leaving default bogus super-high score for adjacent flag";
          std::this_thread::sleep_for(std::chrono::milliseconds(0));
        }
      }
      FlagScore score = rb->get_score(adjacent_flag_pos);
      tile_dist = score.get_tile_dist();
      flag_dist = score.get_flag_dist();
      contains_castle_flag = score.get_contains_castle_flag();   // keep whatever value the adjacent flag has, it's flag-path could very well contain the castle flag
      AILogDebug["score_flag"] << name << "adjacent flag at " << adjacent_flag_pos << " has tile_dist " << tile_dist << ", flag dist " << flag_dist << ", contains_castle_flag " << contains_castle_flag;
      tile_dist += tiles_to_adjacent_flag;
      flag_dist += 1;
      AILogDebug["score_flag"] << name << "splitting_flag has tile_dist: " << tile_dist << ", flag_dist: " << flag_dist;
      adjusted_score = tile_dist + tiles_to_adjacent_flag + flag_dist;
      if (road_options.test(RoadOption::PenalizeCastleFlag) && contains_castle_flag == true) {
        adjusted_score += contains_castle_flag_penalty;
        AILogDebug["score_flag"] << name << "applying contains_castle_flag penalty of " << contains_castle_flag_penalty;
      }
      AILogDebug["score_flag"] << name << "adjacent flag at " << adjacent_flag_pos << " has adjusted_score " << adjusted_score;
      //rb->set_score(best_adjacent_flag_pos, tile_dist, flag_dist, contains_castle_flag);
      // use the score from whichever adjacent flag is better (because the game will use the best route serfs/resources)
      if (adjusted_score < best_adjacent_flag_adjusted_score) {
        AILogDebug["score_flag"] << name << " this score " << adjusted_score << " is better than best_adjacent_flag_adjusted_score of " << best_adjacent_flag_adjusted_score << ", setting splitting_flag_pos " << splitting_flag_pos << "'s score to that";
        best_adjacent_flag_pos = adjacent_flag_pos;
        best_adjacent_flag_adjusted_score = adjusted_score;
        rb->set_score(splitting_flag_pos, tile_dist, flag_dist, contains_castle_flag);
      }
      AILogDebug["score_flag"] << name << "best adjacent flag right now is best_adjacent_flag_pos " << best_adjacent_flag_pos << " with adjusted_score " << best_adjacent_flag_adjusted_score;
    }
    AILogDebug["score_flag"] << name << "done with flag flag/split road solution at flag_pos " << flag_pos << "'s adjacent_flag scoring";
    return true;
  } // if split road / fake flag

  // handle direct road to target_pos - perfect score
  if (target_pos == flag_pos) {
    // this is a *direct route* to the target flag with no intermediate flags
    //    so the only scoring factor will be the new segment's tile length
    //     ??  need to add penalties??
    AILogDebug["score_flag"] << name << "score_flag, flag_pos *IS* target_pos, setting values 0,0";
    //ai_mark_pos->erase(flag_pos);
    //ai_mark_pos->insert(ColorDot(flag_pos, "coral"));
    std::this_thread::sleep_for(std::chrono::milliseconds(0));
    // note that this blindly ignores if castle flag / area part of solution, FIX!
    rb->set_score(flag_pos, 0, 0, false);
    AILogDebug["score_flag"] << name << "score_flag, flag_pos *IS* target_pos, returning true";
    return true;
  }

  // handle most common case, regular scoring
  if (!find_flag_and_tile_dist(map, player_index, rb, flag_pos, castle_flag_pos, ai_mark_pos)) {
    AILogDebug["score_flag"] << name << "score_flag, find_flag_and_tile_dist() returned false, cannot find flag-path solution from flag_pos " << flag_pos << " to target_pos " << target_pos << ".  Returning false";
    return false;
  }

  // debug only
  FlagScore score = rb->get_score(flag_pos);
  unsigned int flag_dist = score.get_flag_dist();
  unsigned int tile_dist = score.get_tile_dist();
  AILogDebug["score_flag"] << name << "score_flag, find_flag_and_tile_dist() from flag_pos " << flag_pos << " to target_pos " << target_pos << " found flag_dist " << flag_dist << ", tile_dist " << tile_dist;

  AILogDebug["score_flag"] << name << "done score_flag, found solution, returning true";
  return true;
}


// build a RoadEnds object from a Road object by checking the first and
//    last dir, to determine the Direction leading back to each end
//    until another flag is found.  The start pos doesn't have to be a real flag,
//     which means fake flags will work
RoadEnds
AI::get_roadends(PMap map, Road road) {
  AILogDebug["get_roadends"] << name << "inside get_roadends";
  std::list<Direction> dirs = road.get_dirs();
  std::list<Direction>::iterator i;
  for (i = dirs.begin(); i != dirs.end(); i++) {
    Direction dir = *i;
    AILogVerbose["get_roadends"] << name << "get_roadends - Direction " << dir << " / " << NameDirection[dir];
  }
  MapPos start_pos = road.get_source();
  MapPos end_pos = road.get_end(map.get());  // this function just traces the road along the existing path anyway
  Direction start_dir = road.get_dirs().front();
  // the Direction of the path leading back to the start is the reverse of the last dir in the road (the dir that leads to the end flag)
  Direction end_dir = reverse_direction(road.get_dirs().back());
  AILogDebug["get_roadends"] << name << "inside get_roadends, start_pos " << start_pos << ", start_dir: " << NameDirection[start_dir] << name << ", end_pos " << end_pos << ", end_dir: " << NameDirection[end_dir];
  RoadEnds ends = std::make_tuple(start_pos, start_dir, end_pos, end_dir);
  return ends;
}


// reverse a Road object - end_pos becomes start_pos, vice versa, and all directions inside reversed to match
Road
AI::reverse_road(PMap map, Road road) {
  AILogDebug["reverse_road"] << name << "inside reverse_road, for a road with source " << road.get_source() << ", end " << road.get_end(map.get()) << ", and length " << road.get_length();
  Road reversed_road;
  reversed_road.start(road.get_end(map.get()));
  std::list<Direction> dirs = road.get_dirs();
  std::list<Direction>::reverse_iterator r;
  for (r = dirs.rbegin(); r != dirs.rend(); r++) {
    reversed_road.extend(reverse_direction(*r));
  }
  AILogDebug["reverse_road"] << name << "returning reversed_road which has source " << reversed_road.get_source() << ", end " << reversed_road.get_end(map.get()) << ", and length " << reversed_road.get_length();
  return reversed_road;
}


// perform FlagSearch to find best flag-path from flag_pos to target_pos and determine tile path along the way.
// return true if solution found, false if not
//  this function will NOT work for fake flags / splitting flags
bool
AI::find_flag_and_tile_dist(PMap map, unsigned int player_index, RoadBuilder *rb, MapPos flag_pos, MapPos castle_flag_pos, ColorDotMap *ai_mark_pos) {
  AILogDebug["find_flag_and_tile_dist"] << name << " inside find_flag_and_tile_dist";
  MapPos target_pos = rb->get_target_pos();
  AILogDebug["find_flag_and_tile_dist"] << name << " preparing to find_flag_and_tile_dist from flag at flag_pos " << flag_pos << " to target_pos " << target_pos;

  //ai_mark_pos->erase(flag_pos);
  //ai_mark_pos->insert(ColorDot(flag_pos, "dk_blue"));
  std::this_thread::sleep_for(std::chrono::milliseconds(0));
  std::vector<PFlagSearchNode> open;
  std::list<PFlagSearchNode> closed;
  PFlagSearchNode fnode(new FlagSearchNode);

  fnode->pos = flag_pos;

  unsigned int flag_dist = 0;
  unsigned int tile_dist = 0;
  bool contains_castle_flag = false;

  open.push_back(fnode);
  AILogVerbose["find_flag_and_tile_dist"] << name << "fsearchnode - starting fnode search for flag_pos " << flag_pos;
  while (!open.empty()) {
    std::pop_heap(open.begin(), open.end(), flagsearch_node_less);
    fnode = open.back();
    open.pop_back();
    AILogVerbose["find_flag_and_tile_dist"] << name << "fsearchnode - inside fnode search for flag_pos " << flag_pos << ", inside while-open-list-not-empty loop";
    if (fnode->pos == target_pos) {
      //ai_mark_pos->erase(fnode->pos);
      //ai_mark_pos->insert(ColorDot(fnode->pos, "cyan"));
      std::this_thread::sleep_for(std::chrono::milliseconds(0));
      //
      // target_pos flag reached!
      //
      AILogVerbose["find_flag_and_tile_dist"] << name << "fsearchnode - fnode->pos " << fnode->pos << " *is now* target_pos " << target_pos;
      AILogDebug["find_flag_and_tile_dist"] << name << "score_flag: plotted a complete multi-road solution from flag_pos " << flag_pos << " to target_pos " << target_pos;
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

        }
        unsigned int existing_road_length = static_cast<unsigned int>(existing_road.get_length());
        fnode = fnode->parent;
        // should be able to use the last node's flag_dist and get same result... maybe compare both as sanity check
        flag_dist++;
        tile_dist += existing_road_length;
        AILogDebug["find_flag_and_tile_dist"] << name << "score_flag: existing road segment with end1 " << end1 << ", end2 " << end2 << " has length " << existing_road_length;
        if (end1 == castle_flag_pos){
          AILogDebug["find_flag_and_tile_dist"] << name << "score_flag: this solution contains the castle flag!";
          contains_castle_flag = true;
        }
        AILogDebug["find_flag_and_tile_dist"] << name << "score_flag: total so far road from flag_pos " << flag_pos << " to target_pos " << target_pos << " is flag_dist " << flag_dist << ", tile_dist " << tile_dist;
      } // while fnode->parent / retrace flag-solution
      AILogDebug["find_flag_and_tile_dist"] << name << "score_flag: final total for road from flag_pos " << flag_pos << " to target_pos " << target_pos << " is flag_dist " << flag_dist << ", tile_dist " << tile_dist;
      rb->set_score(flag_pos, flag_dist, tile_dist, contains_castle_flag);
      AILogDebug["find_flag_and_tile_dist"] << name << "done score_road, found solution, returning true";
      //return std::make_tuple(flag_dist, tile_dist, contains_castle_flag);
      return true;
    } // if found solution

    AILogVerbose["find_flag_and_tile_dist"] << name << "fsearchnode - node->pos is not target_pos " << target_pos << " yet";
    closed.push_front(fnode);
    // for each direction that has a path, trace the path until a flag is reached
    for (Direction d : cycle_directions_cw()) {
      if (map->has_path(fnode->pos, d)) {
        //ai_mark_pos->erase(map->move(fnode->pos, d));
        //ai_mark_pos->insert(ColorDot(map->move(fnode->pos, d), "gray"));
        std::this_thread::sleep_for(std::chrono::milliseconds(0));
        // NOTE - if the solution is found here... can't we just quit and return it rather than continuing with this node?  there can't be a better one, right?

        Road fsearch_road = trace_existing_road(map, fnode->pos, d);
        MapPos new_pos = fsearch_road.get_end(map.get());
        //ai_mark_pos->erase(new_pos);
        //ai_mark_pos->insert(ColorDot(new_pos, "dk_gray"));
        std::this_thread::sleep_for(std::chrono::milliseconds(0));
        //Direction end_dir = reverse_direction(fsearch_road.get_last());
        AILogVerbose["find_flag_and_tile_dist"] << name << "fsearchnode - fsearch from fnode->pos " << fnode->pos << " and dir " << NameDirection[d] << name << " found flag at pos " << new_pos << " with return dir " << reverse_direction(fsearch_road.get_last());

        // break early if this is the target_flag, run the usual new-node code here that would otherwise be run after in_closed and in_open checks
        //   wait, why duplicate this and break?  it should find that it is NOT in_closed and NOT in_open, move on to creating a new node and complete
        //    maybe comment this stuff out
        //   YES this fixes a huge bug!  leaving it this way
        if (new_pos == target_pos) {
          //AILogVerbose["find_flag_and_tile_dist"] << name << "fsearchnode - fnode at new_pos " << new_pos << " *IS* target_pos " << target_pos << ", breaking early";
          AILogVerbose["find_flag_and_tile_dist"] << name << "fsearchnode - fnode at new_pos " << new_pos << " *IS* target_pos " << target_pos << ", FYI only, NOT breaking early (debug)";
          //ai_mark_pos->erase(new_pos);
          //ai_mark_pos->insert(ColorDot(new_pos, "green"));
          std::this_thread::sleep_for(std::chrono::milliseconds(0));
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
            AILogVerbose["find_flag_and_tile_dist"] << name << "fsearchnode - fnode at new_pos " << new_pos << ", breaking because in fnode already in_closed";
            //ai_mark_pos->erase(new_pos);
            //ai_mark_pos->insert(ColorDot(flag_pos, "dk_red"));
            std::this_thread::sleep_for(std::chrono::milliseconds(0));
            break;
          }
        }
        if (in_closed) continue;

        AILogVerbose["find_flag_and_tile_dist"] << name << "fsearchnode - fnode at new_pos " << new_pos << ", continuing because fnode NOT already in_closed";

        // check if this flag is already in open list
        bool in_open = false;
        for (std::vector<PFlagSearchNode>::iterator it = open.begin();
          it != open.end(); ++it) {
          PFlagSearchNode n = *it;
          if (n->pos == new_pos) {
            in_open = true;
            AILogVerbose["find_flag_and_tile_dist"] << name << "fnodesearch - fnode at new_pos " << new_pos << " is already in_open ";
            if (n->flag_dist >= fnode->flag_dist + 1) {
              AILogVerbose["find_flag_and_tile_dist"] << name << "fnodesearch - doing some flag dist compare I don't understand, sorting by flag_dist??";
              n->flag_dist += 1;
              n->parent = fnode;
              iter_swap(it, open.rbegin());
              std::make_heap(open.begin(), open.end(), flagsearch_node_less);
            }
            //ai_mark_pos->erase(new_pos);
            //ai_mark_pos->insert(ColorDot(new_pos, "dk_green"));
            std::this_thread::sleep_for(std::chrono::milliseconds(0));
            break;  // should this be continue like if (in_closed) ???  NO, because it means we've checked every direction ... right?
          }
        }
        // this pos is known known yet, create a new fnode for it
        if (!in_open) {
          AILogVerbose["find_flag_and_tile_dist"] << name << "fnodesearch - fnode at new_pos " << new_pos << " is NOT already in_open, creating a new fnode ";
          //ai_mark_pos->erase(new_pos);
          //ai_mark_pos->insert(ColorDot(new_pos, "lt_green"));
          std::this_thread::sleep_for(std::chrono::milliseconds(0));
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
          AILogVerbose["find_flag_and_tile_dist"] << name << "fnodesearch - fnode at new_pos " << new_pos << " is NOT already in_open, DONE CREATING new fnode ";
        } // if !in_open
        AILogVerbose["find_flag_and_tile_dist"] << name << "fnodesearch end of if(map->has_path)";
      } // if map->has_path(node->pos, d)
      AILogVerbose["find_flag_and_tile_dist"] << name << "fnodesearch end of if dir has path Direction - did I find it??";
    } // foreach direction
    AILogVerbose["find_flag_and_tile_dist"] << name << "fnodesearch end of foreach Direction cycle_dirs";
  } // while open flag-nodes remain to be checked
  AILogVerbose["find_flag_and_tile_dist"] << name << "fnodesearch end of while open flag-nodes remain";

  AILogDebug["find_flag_and_tile_dist"] << name << " no flag-path solution found from flag_pos " << flag_pos << " to target_pos " << target_pos << ".  returning false";
  //return std::make_tuple(bad_score, bad_score, false);
  return false;
}
