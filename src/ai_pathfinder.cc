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
  // hmm it seems flipping this breaks the iter_swap stuff in the find_flag_and_tile_dist code
  //  which rusults in a lot of problems.  I am reverting this and need to investigate if 
  //  find_flag_and_tile_dist are doing depth first or if it is only a problem with the new find_flag_path_between_flags
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
  AILogDebug["plot_road"] << "inside plot_road for Player" << player_index << " with start " << start_pos << ", end " << end_pos;
  
  //ai_mark_pos.clear();
  //ai_mark_road->start(end_pos);  //this doesn't actually work here

  std::vector<PSearchNode> open;
  std::list<PSearchNode> closed;
  PSearchNode node(new SearchNode);

  // prevent path in the potential building spot be inserting into the closed list!
  if (hold_building_pos == true){
    //AILogDebug["plot_road"] << "debug - hold_building_pos is TRUE!";
    PSearchNode hold_building_pos_node(new SearchNode);
    hold_building_pos_node->pos = map->move_up_left(start_pos);
    closed.push_front(hold_building_pos_node);
    //AILogDebug["plot_road"] << "debug - hold_building_pos is TRUE, added hold_building_pos " << hold_building_pos_node->pos << " which is up-left of start_pos " << start_pos;
  }

  // allow pathfinding to *potential* new flags outside of the fake_flags process
  // if the specified end_pos has no flag
  //  oh wait this is already allowed, keep the sanity check and FYI note anyway though
  if (!map->has_flag(end_pos)){
    AILogDebug["plot_road"] << "the specified end_pos has no flag, allowing a Direct Road plot to this endpoint as if it had one";
    if (!game->can_build_flag(end_pos, player)){
      AILogWarn["plot_road"] << "sanity check failed!  plot_road was requested to non-flag end_pos " << end_pos << ", but a flag cannot even be built there!  returning bad road";
      Road failed_road;
      return failed_road;
    }
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
        //sleep_speed_adjusted(100);
        node = node->parent;
      }
      unsigned int new_length = static_cast<unsigned int>(breadcrumb_solution.get_length());
      AILogDebug["plot_road"] << "plot_road: solution found, new segment length is " << breadcrumb_solution.get_length();
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
      //sleep_speed_adjusted(100);
      // Check if neighbour is valid
      if (!map->is_road_segment_valid(node->pos, d) ||
        (map->get_obj(new_pos) == Map::ObjectFlag && new_pos != end_pos)) {
        //(avoid_castle && AI::is_near_castle(game, new_pos))) {
        //
        // if the pathfinder hits an existing road point where a flag could be built,
        //   create a "fake flag" solution and include it as a potential road
        //
        if (fake_flags_count > max_fake_flags) {
          AILogDebug["plot_road"] << "reached max_fake_flags count " << max_fake_flags << ", not considering any more fake flag solutions";
          continue;
        }
        // split road found if can build a flag, and that flag is already part of a road (meaning it has at least one path)
        if (game->can_build_flag(new_pos, player) && map->has_any_path(new_pos)){
          fake_flags_count++;
          AILogDebug["plot_road"] << "plot_road: alternate/split_road solution found while pathfinding to " << end_pos << ", a new flag could be built at pos " << new_pos;
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
            //sleep_speed_adjusted(100);
            split_flag_node = split_flag_node->parent;
          }
          // restore original node so search can resume
          node = orig_node;
          unsigned int new_length = static_cast<unsigned int>(split_flag_breadcrumb_solution.get_length());
          AILogDebug["plot_road"] << "plot_road: split_flag_breadcrumb_solution found, new segment length is " << split_flag_breadcrumb_solution.get_length();
          // now inverse the entire road so that it stats at start_pos and ends at end_pos, so the Road object is logical instead of backwards
          Road split_flag_solution = reverse_road(map, split_flag_breadcrumb_solution);
          AILogDebug["plot_road"] << "plot_road: inserting alternate/fake flag Road solution to PotentialRoads, new segment length would be " << split_flag_solution.get_length();
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
          //sleep_speed_adjusted(100);
          break;
        }
      }
      //ai_mark_pos.erase(new_pos);
      //ai_mark_pos.insert(ColorDot(new_pos, "green"));
      //sleep_speed_adjusted(100);

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
          //sleep_speed_adjusted(100);
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

  AILogDebug["plot_road"] << "done plot_road, found_direct_road: " << std::to_string(found_direct_road) << ". additional potential_roads (split_roads): " << split_road_solutions;
  if (direct_road.get_source() == bad_map_pos) {
    AILogDebug["plot_road"] << "NO DIRECT SOLUTION FOUND for start_pos " << start_pos << " to end_pos " << end_pos;
  }
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["plot_road"] << "plot road call took " << duration;
  return direct_road;
}


// count how many tiles apart two MapPos are
int
AI::get_straightline_tile_dist(PMap map, MapPos start_pos, MapPos end_pos) {
  //AILogDebug["get_straightline_tile_dist"] << "inside Pathfinder::tile_dist with start_pos " << start_pos << ", end_pos " << end_pos;
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
  //AILogDebug["get_straightline_tile_dist"] << "returning tile_dist: " << tile_dist;
  return tile_dist;
}


// find best flag-path from flag_pos to target_pos and store flag and tile count in roadbuilder
//  return true if solution found, false if couldn't be found
// this function will accept "fake flags" / splitting-flags that do not actually exist
//  and score them based on the best score of their adjacent flags
bool
AI::score_flag(PMap map, unsigned int player_index, RoadBuilder *rb, RoadOptions road_options, MapPos flag_pos, MapPos castle_flag_pos, ColorDotMap *ai_mark_pos) {
  AILogDebug["score_flag"] << "inside score_flag";
  MapPos target_pos = rb->get_target_pos();
  AILogDebug["score_flag"] << "preparing to score_flag for Player" << player_index << " for flag at flag_pos " << flag_pos << " to target_pos " << target_pos;

  // handle split_road / fake flag solutions
  if (!map->has_flag(flag_pos)) {
    AILogDebug["score_flag"] << "flag not found at flag_pos " << flag_pos << ", assuming this is a fake flag/split road solution and using closest adjacent flag + tile dist from fake flag";
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
      //AILogDebug["score_flag"] << "looking for a path in dir " << NameDirection[dir];
      if (!map->has_path_IMPROVED(splitting_flag_pos, dir)) {
        continue;
      }
      AILogDebug["score_flag"] << "found a path in dir " << NameDirection[dir] << ", tracing road";
      Road split_road = trace_existing_road(map, splitting_flag_pos, dir);
      adjacent_flag_pos = split_road.get_end(map.get());
      unsigned int tiles_to_adjacent_flag = static_cast<unsigned int>(split_road.get_length());
      AILogDebug["score_flag"] << "split road in dir " << NameDirection[dir] << " has length " << tiles_to_adjacent_flag << " to adjacent flag at pos " << adjacent_flag_pos;
      unsigned int tile_dist = bad_score;
      unsigned int flag_dist = bad_score;
      unsigned int adjusted_score = bad_score;
      bool contains_castle_flag = false;
      // go score the adjacent flags if they aren't already known (sometimes they will already have been checked)
      if (!rb->has_score(adjacent_flag_pos)) {
        AILogDebug["score_flag"] << "score_flag, rb doesn't have score yet for adjacent flag_pos " << adjacent_flag_pos << ", will try to score it";
        // USING NEW FUNCTION
        //if (find_flag_and_tile_dist(map, player_index, rb, adjacent_flag_pos, castle_flag_pos, ai_mark_pos)) {
        MapPosVector found_flags = {};
        unsigned int tile_dist = 0;
        if(find_flag_path_and_tile_dist_between_flags(map, adjacent_flag_pos, target_pos, &found_flags, &tile_dist, ai_mark_pos)){
          AILogDebug["score_flag"] << "score_flag, splitting_flag find_flag_path_and_tile_dist_between_flags() returned true from flag_pos " << adjacent_flag_pos << " to target_pos " << target_pos;
          // manually set rb stuff because the new function doesn't use it
          unsigned int flag_dist = found_flags.size();
          for(MapPos flag_pos : found_flags){
            if (flag_pos == castle_flag_pos){
              AILogDebug["score_flag"] << "this solution contains the castle flag!";
              contains_castle_flag = true;
              break;
            }
          }
          AILogDebug["score_flag"] << "score_flag, splitting_flag's adjacent_flag_pos " << adjacent_flag_pos << " has flag_dist " << flag_dist << ", tile_dist " << tile_dist << ".  setting this score in rb";
          rb->set_score(adjacent_flag_pos, flag_dist, tile_dist, contains_castle_flag);
        }
        else{
          AILogDebug["score_flag"] << "score_flag, splitting_flag find_flag_path_and_tile_dist_between_flags() returned false, cannot find flag-path solution from flag_pos " << adjacent_flag_pos << " to target_pos " << target_pos << ".  Returning false";
          // if not found just let rb->get_score call fail and it wlil use the default bad_scores given.
          AILogDebug["score_flag"] << "score_flag returned false for adjacent flag at pos " << adjacent_flag_pos;
          AILogDebug["score_flag"] << "for now, leaving default bogus super-high score for adjacent flag";
          //std::this_thread::sleep_for(std::chrono::milliseconds(0));
        }
      }
      FlagScore score = rb->get_score(adjacent_flag_pos);
      tile_dist = score.get_tile_dist();
      flag_dist = score.get_flag_dist();
      contains_castle_flag = score.get_contains_castle_flag();   // keep whatever value the adjacent flag has, it's flag-path could very well contain the castle flag
      AILogDebug["score_flag"] << "adjacent flag at " << adjacent_flag_pos << " has tile_dist " << tile_dist << ", flag dist " << flag_dist << ", contains_castle_flag " << contains_castle_flag;
      tile_dist += tiles_to_adjacent_flag;
      flag_dist += 1;
      AILogDebug["score_flag"] << "splitting_flag has tile_dist: " << tile_dist << ", flag_dist: " << flag_dist;
      adjusted_score = tile_dist + tiles_to_adjacent_flag + flag_dist;
      if (road_options.test(RoadOption::PenalizeCastleFlag) && contains_castle_flag == true) {
        if (target_pos == castle_flag_pos){
          AILogDebug["score_flag"] << "not applying contains_castle_flag penalty because the target *is* the castle flag pos!";
        }else{
          adjusted_score += contains_castle_flag_penalty;
          AILogDebug["score_flag"] << "applying contains_castle_flag penalty of " << contains_castle_flag_penalty;
        }
      }
      AILogDebug["score_flag"] << "adjacent flag at " << adjacent_flag_pos << " has adjusted_score " << adjusted_score;
      //rb->set_score(best_adjacent_flag_pos, tile_dist, flag_dist, contains_castle_flag);
      // use the score from whichever adjacent flag is better (because the game will use the best route serfs/resources)
      if (adjusted_score < best_adjacent_flag_adjusted_score) {
        AILogDebug["score_flag"] << "this score " << adjusted_score << " is better than best_adjacent_flag_adjusted_score of " << best_adjacent_flag_adjusted_score << ", setting splitting_flag_pos " << splitting_flag_pos << "'s score to that";
        best_adjacent_flag_pos = adjacent_flag_pos;
        best_adjacent_flag_adjusted_score = adjusted_score;
        rb->set_score(splitting_flag_pos, tile_dist, flag_dist, contains_castle_flag);
      }
      AILogDebug["score_flag"] << "best adjacent flag right now is best_adjacent_flag_pos " << best_adjacent_flag_pos << " with adjusted_score " << best_adjacent_flag_adjusted_score;
    }
    AILogDebug["score_flag"] << "done with flag flag/split road solution at flag_pos " << flag_pos << "'s adjacent_flag scoring";
    return true;
  } // if split road / fake flag

  // handle direct road to target_pos - perfect score
  if (target_pos == flag_pos) {
    // this is a *direct route* to the target flag with no intermediate flags
    //    so the only scoring factor will be the new segment's tile length
    //     ??  need to add penalties??
    AILogDebug["score_flag"] << "score_flag, flag_pos *IS* target_pos, setting values 0,0";
    //ai_mark_pos->erase(flag_pos);
    //ai_mark_pos->insert(ColorDot(flag_pos, "coral"));
    //std::this_thread::sleep_for(std::chrono::milliseconds(0));
    // note that this blindly ignores if castle flag / area part of solution, FIX!
    AILogDebug["score_flag"] << "inserting perfect score 0,0 for target_pos flag at " << flag_pos << " into rb";
    rb->set_score(flag_pos, 0, 0, false);
    AILogDebug["score_flag"] << "score_flag, flag_pos *IS* target_pos, returning true";
    return true;
  }

  // handle most common case, regular scoring
  // USING NEW FUNCTION
  //if (!find_flag_and_tile_dist(map, player_index, rb, flag_pos, castle_flag_pos, ai_mark_pos)) {
  //  AILogDebug["score_flag"] << "score_flag, find_flag_and_tile_dist() returned false, cannot find flag-path solution from flag_pos " << flag_pos << " to target_pos " << target_pos << ".  Returning false";
  //  return false;
  //}
  MapPosVector found_flags = {};
  unsigned int tile_dist = 0;
  bool contains_castle_flag = false;
  if(find_flag_path_and_tile_dist_between_flags(map, flag_pos, target_pos, &found_flags, &tile_dist, ai_mark_pos)){
    AILogDebug["score_flag"] << "score_flag, find_flag_path_and_tile_dist_between_flags() returned true from flag_pos " << flag_pos << " to target_pos " << target_pos;
    // manually set rb stuff because the new function doesn't use it
    unsigned int flag_dist = found_flags.size();
    for(MapPos flag_pos : found_flags){
      if (flag_pos == castle_flag_pos){
        AILogDebug["score_flag"] << "this solution contains the castle flag!";
        contains_castle_flag = true;
        break;
      }
    }
    AILogDebug["score_flag"] << "score_flag, setting score for flag_pos " << flag_pos << " to flag_dist " << flag_dist << ", tile_dist " << tile_dist;
    rb->set_score(flag_pos, flag_dist, tile_dist, contains_castle_flag);
  }else{
    AILogDebug["score_flag"] << "score_flag, find_flag_path_and_tile_dist_between_flags() returned false, cannot find flag-path solution from flag_pos " << flag_pos << " to target_pos " << target_pos << ".  Returning false";
    return false;
  }



  // debug only
  FlagScore score = rb->get_score(flag_pos);
  unsigned int debug_flag_dist = score.get_flag_dist();
  unsigned int debug_tile_dist = score.get_tile_dist();
  AILogDebug["score_flag"] << "score_flag, find_flag_path_and_tile_dist_between_flags() from flag_pos " << flag_pos << " to target_pos " << target_pos << " found flag_dist " << debug_flag_dist << ", tile_dist " << debug_tile_dist;

  AILogDebug["score_flag"] << "done score_flag, found solution, returning true";
  return true;
}


// build a RoadEnds object from a Road object by checking the first and
//    last dir, to determine the Direction leading back to each end
//    until another flag is found.  The start pos doesn't have to be a real flag,
//     which means fake flags will work
RoadEnds
AI::get_roadends(PMap map, Road road) {
  AILogDebug["get_roadends"] << "inside get_roadends";
  std::list<Direction> dirs = road.get_dirs();
  std::list<Direction>::iterator i;
  for (i = dirs.begin(); i != dirs.end(); i++) {
    Direction dir = *i;
    AILogVerbose["get_roadends"] << "get_roadends - Direction " << dir << " / " << NameDirection[dir];
  }
  MapPos start_pos = road.get_source();
  MapPos end_pos = road.get_end(map.get());  // this function just traces the road along the existing path anyway
  Direction start_dir = road.get_dirs().front();
  // the Direction of the path leading back to the start is the reverse of the last dir in the road (the dir that leads to the end flag)
  Direction end_dir = reverse_direction(road.get_dirs().back());
  AILogDebug["get_roadends"] << "inside get_roadends, start_pos " << start_pos << ", start_dir: " << NameDirection[start_dir] << ", end_pos " << end_pos << ", end_dir: " << NameDirection[end_dir];
  RoadEnds ends = std::make_tuple(start_pos, start_dir, end_pos, end_dir);
  return ends;
}


// reverse a Road object - end_pos becomes start_pos, vice versa, and all directions inside reversed to match
Road
AI::reverse_road(PMap map, Road road) {
  //AILogDebug["reverse_road"] << "inside reverse_road, for a road with source " << road.get_source() << ", end " << road.get_end(map.get()) << ", and length " << road.get_length();
  Road reversed_road;
  reversed_road.start(road.get_end(map.get()));
  std::list<Direction> dirs = road.get_dirs();
  std::list<Direction>::reverse_iterator r;
  for (r = dirs.rbegin(); r != dirs.rend(); r++) {
    reversed_road.extend(reverse_direction(*r));
  }
  //AILogDebug["reverse_road"] << "returning reversed_road which has source " << reversed_road.get_source() << ", end " << reversed_road.get_end(map.get()) << ", and length " << reversed_road.get_length();
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
	AILogDebug["find_nearest_inventories_to_military_building"] << "inside find_nearest_inventory_to_military_building to pos " << pos << ", overlap_threshold " << overlap_threshold << ", currently selected inventory_pos is " << inventory_pos;
	MapPosVector closest_inventories = {};
	// get inventory distances by straight-line map distance only, ignoring roads, flags, obstacles, etc.
	unsigned int best_dist = bad_score;
	MapPosSet inventory_dists = {};
	for (MapPos inv_pos : stocks_pos) {
		unsigned int dist = AI::get_straightline_tile_dist(map, pos, inv_pos);
		AILogDebug["find_nearest_inventories_to_military_building"] << "straightline tile dist from pos " << pos << " to inventory_pos " << inv_pos << " is " << dist;
		inventory_dists.insert(std::make_pair(inv_pos, dist));
		if (dist < best_dist){
			AILogDebug["find_nearest_inventories_to_military_building"] << "inventory at inventory_pos " << inv_pos << " is the closest so far to pos " << pos << " , with dist " << dist;
			best_dist = dist;
		}
	}
	if (inventory_dists.size() == 0 || best_dist == bad_score) {
		AILogDebug["find_nearest_inventories_to_military_building"] << "no inventories found!  returning empty MapPosVector";
		return closest_inventories;
	}
	// create a vector of all inventories within the threshold of the best_dist
	for (std::pair<MapPos, unsigned int> pair : inventory_dists) {
    unsigned int inv_pos = pair.first;
		unsigned int dist = pair.second;
		if (dist < best_dist + overlap_threshold) {
			AILogDebug["find_nearest_inventories_to_military_building"] << "inventory at " << inv_pos << " has dist " << dist << ", which is within " << best_dist + overlap_threshold << " of pos " << pos << ", including in list";
			closest_inventories.push_back(inv_pos);
		}
	}
	AILogDebug["find_nearest_inventories_to_military_building"] << "done, closest_inventories list has " << std::to_string(closest_inventories.size()) << " items, best_dist is " << best_dist;
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
  AILogDebug["util_find_nearest_inventory"] << "inside find_nearest_inventory to pos " << pos << " with dist_type " << NameDistType[dist_type];
  if (pos == bad_map_pos){
    AILogWarn["util_find_nearest_inventory"] << "the called pos is a bad_map_pos!  returning bad_map_pos";
    return bad_map_pos;
  }
  if (!map->has_flag(pos) && !map->has_building(pos)) {
		AILogWarn["util_find_nearest_inventory"] << "no flag or building found at pos " << pos << " as expected!  Cannot run search, returning bad_map_pos";
		return bad_map_pos;
	}
	MapPos flag_pos = bad_map_pos;
	if (map->has_building(pos)) {
		flag_pos = map->move_down_right(pos);
		AILogDebug["util_find_nearest_inventory"] << "request pos " << pos << " is a building pos, using its flag pos " << flag_pos << " instead";
	}
	else {
		flag_pos = pos;
	}
	Flag *flag = game->get_flag_at_pos(flag_pos);
	if (flag == nullptr) {
		AILogWarn["util_find_nearest_inventory"] << "got nullptr for Flag at flag_pos " << flag_pos << "!  Cannot run search, returning bad_map_pos";
		return bad_map_pos;
	}

  MapPos by_straightline_dist = bad_map_pos;
  if (dist_type == DistType::StraightLineOnly || dist_type == DistType::FlagAndStraightLine){
    by_straightline_dist = find_nearest_inventory_by_straightline(map, player_index, flag_pos, ai_mark_pos);
    if (by_straightline_dist == bad_map_pos){
      AILogDebug["util_find_nearest_inventory"] << "got bad_map_pos for by_straightline_dist, can't continue, returning bad_map_pos";
      return bad_map_pos;
    }
  }

  MapPos by_flag_dist = bad_map_pos;
  if (dist_type == DistType::FlagOnly || dist_type == DistType::FlagAndStraightLine){
    by_flag_dist = find_nearest_inventory_by_flag(map, player_index, flag_pos, ai_mark_pos);
    if (by_flag_dist == bad_map_pos){
      AILogDebug["util_find_nearest_inventory"] << "got bad_map_pos for by_flag_dist, can't continue, returning bad_map_pos";
      return bad_map_pos;
    }
  }

  if (dist_type == DistType::StraightLineOnly){
    AILogDebug["util_find_nearest_inventory"] << "returning nearest Inventory pos by_straightline_dist: " << by_straightline_dist;
    return by_straightline_dist;
  }

  if (dist_type == DistType::FlagOnly){
    AILogDebug["util_find_nearest_inventory"] << "returning nearest Inventory pos by_flag_dist: " << by_flag_dist;
    return by_flag_dist;
  }

  // what purpose does this have???  I can no longer remember why this combination is important
  // maybe it was to try to avoid buildings being "stolen" or moved between inventory "owners" 
  // because of straight-line stuff and/or road changes?  
  // I changed some places that used this to FlagOnly, see if any remain and reconsider if
  // this is really needed, I might be making a mistake by not using it now
  //
  // so the only place this seems appropriate is for helping ensure buildings being placed
  //  are located near their Inv so they are less likely to be "stolen" by other Invs.  I only
  //  see this being used by the food buildings function, not sure if it is redundant or if
  //  the other do_build functions should be using it too
  if (dist_type == DistType::FlagAndStraightLine){
    if (by_flag_dist != by_straightline_dist){
      AILogDebug["util_find_nearest_inventory"] << "nearest Inventory to flag_pos " << flag_pos << " by_flag_dist is " << by_flag_dist << ", but by_straightline_dist is " << by_straightline_dist << ", returning bad_map_pos";
      return bad_map_pos;
    }else{
      AILogDebug["util_find_nearest_inventory"] << "returning both matching pos " << by_straightline_dist;
      return by_straightline_dist;  // or could return by_flag_dist, same result
    }
  }
  AILogError["util_find_nearest_inventory"] << "this should never happen, returning bad_map_pos";
  return bad_map_pos;
}

MapPos
AI::find_nearest_inventory_by_straightline(PMap map, unsigned int player_index, MapPos flag_pos, ColorDotMap *ai_mark_pos) {
  AILogDebug["find_nearest_inventory_by_straightline"] << "inside find_nearest_inventory_by_straightline for flag_pos " << flag_pos;
  unsigned int shortest_dist = bad_score;
  MapPos closest_inv = bad_map_pos;
  //AILogVerbose["find_nearest_inventory_by_straightline"] << "thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings";
  //game->get_mutex()->lock();
  //AILogVerbose["find_nearest_inventory_by_straightline"] << "thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings";
  Game::ListBuildings buildings = game->get_player_buildings(player);
  //AILogVerbose["find_nearest_inventory_by_straightline"] << "thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings";
  //game->get_mutex()->unlock();
  //AILogVerbose["find_nearest_inventory_by_straightline"] << "thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings";
  for (Building *building : buildings) {
    if (building == nullptr)
      continue;
    if (building->is_burning())
      continue;
    if (building->get_type() != Building::TypeCastle && building->get_type() != Building::TypeStock)
      continue;
    Flag *building_flag = game->get_flag(building->get_flag_index());
    if (building_flag == nullptr)
      continue;
    if (!building_flag->accepts_resources())
      continue;
    MapPos building_flag_pos = building_flag->get_position();
    unsigned int dist = (unsigned)get_straightline_tile_dist(map, flag_pos, building_flag_pos);
    if (dist >= shortest_dist)
      continue;
    AILogDebug["find_nearest_inventory_by_straightline"] << "SO FAR, the closest Inventory building to flag_pos " << flag_pos << " found at " << building_flag_pos;
    shortest_dist = dist;
    closest_inv = building_flag_pos;
    continue;
  }
  if (closest_inv != bad_map_pos){
    AILogDebug["find_nearest_inventory_by_straightline"] << "closest Inventory building to flag_pos " << flag_pos << " found at " << closest_inv;
  }else{
    AILogWarn["find_nearest_inventory_by_straightline"] << "closest Inventory building to flag_pos " << flag_pos << " did not find ANY valid Inventory building - was Castle and all Stocks destroyed???";
  }
  return closest_inv;
}


MapPos
AI::find_nearest_inventory_by_flag(PMap map, unsigned int player_index, MapPos flag_pos, ColorDotMap *ai_mark_pos) {
	AILogDebug["find_nearest_inventory_by_flag"] << "inside find_nearest_inventory_by_flag to flag_pos " << flag_pos;
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
	//AILogVerbose["find_nearest_inventory_by_flag"] << "fsearchnode - starting fnode search for flag_pos " << flag_pos;
	while (!open.empty()) {
		std::pop_heap(open.begin(), open.end(), flagsearch_node_less);
		fnode = open.back();
		open.pop_back();
    //AILogDebug["find_nearest_inventory_by_flag"] << "fsearchnode - inside fnode search for flag_pos " << flag_pos << ", inside while-open-list-not-empty loop";

		if (game->get_flag_at_pos(fnode->pos)->accepts_resources()) {
      // to avoid crashes, handle discovering a newly built warehouse that just now became active
      //  after the most recent update_stocks run, and doesn't exist in stocks_pos yet
      if (stock_building_counts.count(fnode->pos) == 0){
        //update_stocks_pos();
        // hmm this seems like a bad place to put this.. for now just skip this Inventory
        //  and let the next AI loop find it
        AILogDebug["find_nearest_inventory_by_flag"] << "found a newly active Inventory building at " << fnode->pos << " that is not tracked yet, skipping it for now.";
      }else{
        // an Inventory building's flag reached, solution found
        AILogDebug["find_nearest_inventory_by_flag"] << "flagsearch complete, found solution from flag_pos " << flag_pos << " to an Inventory building's flag";

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
          //Direction dir1 = fnode->parent->dir;  // DONT USE ->dir, instead use child_dir[d] !!!
          //Direction dir2 = DirectionNone;
          // is this needed?
          //existing_road = trace_existing_road(map, end1, dir1);
          //end2 = existing_road.get_end(map.get());
          //fnode = fnode->parent;
          //flag_dist++;
        //}

        AILogDebug["find_nearest_inventory_by_flag"] << "done find_nearest_inventory_by_flag, found solution, returning Inventory flag pos " << fnode->pos;
        //return true;
        // this needs to return the MapPos of the inventory flag (update it later to get rid of all these conversions)
        return fnode->pos;
      }
		}

    //AILogDebug["find_nearest_inventory_by_flag"] << "fsearchnode - fnode->pos " << fnode->pos << " is not at an Inventory building flag yet, adding fnode to closed list";
		closed.push_front(fnode);

		// for each direction that has a path, trace the path until a flag is reached
		for (Direction d : cycle_directions_cw()) {
			if (map->has_path_IMPROVED(fnode->pos, d)) {
				// couldn't this use the internal flag->other_end_dir stuff instead of tracing it?
				// maybe...  try that after this is stable
				Road fsearch_road = trace_existing_road(map, fnode->pos, d);
				MapPos new_pos = fsearch_road.get_end(map.get());
				//AILogDebug["find_nearest_inventory_by_flag"] << "fsearchnode - fsearch from fnode->pos " << fnode->pos << " and dir " << NameDirection[d] << " found flag at pos " << new_pos << " with return dir " << reverse_direction(fsearch_road.get_last());
				// check if this flag is already in closed list
				bool in_closed = false;
				for (PFlagSearchNode closed_node : closed) {
					if (closed_node->pos == new_pos) {
						in_closed = true;
            //AILogDebug["find_nearest_inventory_by_flag"] << "fsearchnode - fnode at new_pos " << new_pos << ", breaking because in fnode already in_closed";
						break;
					}
				}
				if (in_closed) continue;

        //AILogDebug["find_nearest_inventory_by_flag"] << "fsearchnode - fnode at new_pos " << new_pos << ", continuing because fnode NOT already in_closed";

				// check if this flag is already in open list
				bool in_open = false;
				for (std::vector<PFlagSearchNode>::iterator it = open.begin(); it != open.end(); ++it) {
					PFlagSearchNode n = *it;
					if (n->pos == new_pos) {
						in_open = true;
            //AILogDebug["find_nearest_inventory_by_flag"] << "fnodesearch - fnode at new_pos " << new_pos << " is already in_open ";
						if (n->flag_dist >= fnode->flag_dist + 1) {
              //AILogDebug["find_nearest_inventory_by_flag"] << "fnodesearch - new_pos " << new_pos << "'s flag_dist is >= fnode->flag_dist + 1";
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
          //AILogDebug["find_nearest_inventory_by_flag"] << "fnodesearch - fnode at new_pos " << new_pos << " is NOT already in_open, creating a new fnode ";
					PFlagSearchNode new_fnode(new FlagSearchNode);
					new_fnode->pos = new_pos;
					new_fnode->parent = fnode;
					new_fnode->flag_dist = new_fnode->parent->flag_dist + 1;
					// when doing a flag search, the parent node's direction *cannot* be inferred by reversing its child node dir
					//   like when doing tile pathfinding, so it must be set explicitly set. it will be reused and reset into each explored direction
					//   but that should be okay because once an end solution is found the other directions no longer matter
					//fnode->dir = d;
          fnode->child_dir[d] = new_pos;
					open.push_back(new_fnode);
					std::push_heap(open.begin(), open.end(), flagsearch_node_less);
					//AILogVerbose["find_nearest_inventory_by_flag"] << "fnodesearch - fnode at new_pos " << new_pos << " is NOT already in_open, DONE CREATING new fnode ";
				} // if !in_open
				//AILogVerbose["find_nearest_inventory_by_flag"] << "fnodesearch end of if(map->has_path_IMPROVED)";
			} // if map->has_path_IMPROVED(node->pos, d)
			//AILogVerbose["find_nearest_inventory_by_flag"] << "fnodesearch end of if dir has path Direction - did I find it??";
		} // foreach direction
		//AILogVerbose["find_nearest_inventory_by_flag"] << "fnodesearch end of foreach Direction cycle_dirs";
	} // while open flag-nodes remain to be checked
	//AILogVerbose["find_nearest_inventory_by_flag"] << "fnodesearch end of while open flag-nodes remain";

	// if the search ended it means nothing was found, return bad_map_pos
	AILogDebug["find_nearest_inventory_by_flag"] << "no flag-path solution found from flag_pos " << flag_pos << " to an Inventory building's flag.  returning false";
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

  AILogDebug["util_identify_arterial_roads"] << "inside AI::identify_arterial_roads";
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

  Flags flags_copy = *(game->get_flags());  // create a copy so we don't conflict with the game thread, and don't want to mutex lock for a long function
  for (Flag *flag : flags_copy) {
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
    //AILogDebug["util_identify_arterial_roads"] << "checking flag at pos " << flag_pos;

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
      //AILogDebug["util_identify_arterial_roads"] << "fsearchnode - inside fnode search for flag_pos " << flag_pos << ", inside while-open-list-not-empty loop";

      if (game->get_flag_at_pos(fnode->pos)->accepts_resources()) {
        // to avoid crashes, handle discovering a newly built warehouse that just now became active
        //  after the most recent update_stocks run, and doesn't exist in stocks_pos yet
        if (stock_building_counts.count(fnode->pos) == 0){
          //update_stocks_pos();
          // hmm this seems like a bad place to put this.. for now just skip this Inventory
          //  and let the next AI loop find it
          AILogDebug["util_identify_arterial_roads"] << "found a newly active Inventory building at " << fnode->pos << " that is not tracked yet, skipping it for now.";
        }else{
          //AILogDebug["util_identify_arterial_roads"] << "flagsearch solution found from flag_pos " << flag_pos << " to an Inventory building's flag";
          found_inv = true;
          // uniquely identify the connection point of each artery to the Flag-Dir it is coming from
          // this is the dir of 2nd last node, which leads to the last node (which has no dir)
          if (fnode->parent == nullptr){
            AILogDebug["util_identify_arterial_roads"] << "no parent flag node found!  this must be the Inventory pos, breaking";
            break;
          }
          MapPos inv_flag_pos = fnode->pos;
          //AILogDebug["util_identify_arterial_roads"] << "debug:::::::::   inv_flag_pos = " << inv_flag_pos << " when recording flag dirs with paths from inv";
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
              //AILogDebug["util_identify_arterial_roads"] << "the 2nd-to-last flag at pos " << fnode->parent->pos << " has path TO inv_flag_pos " << inv_flag_pos << " in dir " << NameDirection[d] << " / " << d;
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
            AILogError["util_identify_arterial_roads"] << "could not find the Dir from fnode->parent->pos " << fnode->parent->pos << " to child fnode->pos " << fnode->pos << "! throwing exception";
            std::this_thread::sleep_for(std::chrono::milliseconds(120000));
            throw ExceptionFreeserf("in AI::util_identify_arterial_roads, could not find the Dir from fnode->parent->pos to child !fnode->pos!  it should be known");
          }
          // actually, I take it back, tracking it the entire way is fine
          //  if the alternative is checking 6 dirs and potentially buggy
          //Direction inv_flag_conn_dir = trace...
          //AILogDebug["util_identify_arterial_roads"] << "reached an Inventory building's flag at pos " << inv_flag_pos << ", connecting from dir " << NameDirection[inv_flag_conn_dir] << " / " << inv_flag_conn_dir;
          while (fnode->parent){
            //AILogDebug["util_identify_arterial_roads"] << "fnode->pos = " << fnode->pos << ", fnode->parent->pos " << fnode->parent->pos << ", fnode->parent->dir = " << NameDirection[fnode->parent->dir] << " / " << fnode->parent->dir;
            fnode = fnode->parent;
            flag_counts[inv_flag_pos][inv_flag_conn_dir][fnode->pos]++;
            flag_dist++;
          }
          //AILogDebug["util_identify_arterial_roads"] << "flag_dist from flag_pos " << flag_pos << " to nearest Inventory pos " << inv_flag_pos << " is " << flag_dist;
          break;
        }
      }

      //AILogDebug["util_identify_arterial_roads"] << "fsearchnode - fnode->pos " << fnode->pos << " is not at an Inventory building flag yet, adding fnode to closed list";
      closed.push_front(fnode);

      // for each direction that has a path, trace the path until a flag is reached
      for (Direction d : cycle_directions_cw()) {
        if (map->has_path_IMPROVED(fnode->pos, d)) {
          // couldn't this use the internal flag->other_end_dir stuff instead of tracing it?
          // maybe...  try that after this is stable
          Road fsearch_road = trace_existing_road(map, fnode->pos, d);
          MapPos new_pos = fsearch_road.get_end(map.get());
          //AILogDebug["util_identify_arterial_roads"] << "fsearchnode - fsearch from fnode->pos " << fnode->pos << " and dir " << NameDirection[d] << " found flag at pos " << new_pos << " with return dir " << reverse_direction(fsearch_road.get_last());
          // check if this flag is already in closed list
          bool in_closed = false;
          for (PFlagSearchNode closed_node : closed) {
            if (closed_node->pos == new_pos) {
              in_closed = true;
              //AILogDebug["util_identify_arterial_roads"] << "fsearchnode - fnode at new_pos " << new_pos << ", breaking because in fnode already in_closed";
              break;
            }
          }
          if (in_closed) continue;

          //AILogDebug["util_identify_arterial_roads"] << "fsearchnode - fnode at new_pos " << new_pos << ", continuing because fnode NOT already in_closed";

          // check if this flag is already in open list
          bool in_open = false;
          for (std::vector<PFlagSearchNode>::iterator it = open.begin(); it != open.end(); ++it) {
            PFlagSearchNode n = *it;
            if (n->pos == new_pos) {
              in_open = true;
              //AILogDebug["util_identify_arterial_roads"] << "fnodesearch - fnode at new_pos " << new_pos << " is already in_open ";
              if (n->flag_dist >= fnode->flag_dist + 1) {
                AILogDebug["util_identify_arterial_roads"] << "fnodesearch - new_pos " << new_pos << "'s flag_dist is >= fnode->flag_dist + 1";
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
            //AILogDebug["util_identify_arterial_roads"] << "fnodesearch - fnode at new_pos " << new_pos << " is NOT already in_open, creating a new fnode ";
            PFlagSearchNode new_fnode(new FlagSearchNode);
            new_fnode->pos = new_pos;
            new_fnode->parent = fnode;
            new_fnode->flag_dist = new_fnode->parent->flag_dist + 1;
            // when doing a flag search, the parent node's direction *cannot* be inferred by reversing its child node dir
            //   like when doing tile pathfinding, so it must be set explicitly set
            fnode->child_dir[d] = new_pos; // wait WHY IS THIS DONE DIFFERENTLY THAN IN MY OTHER SEARCHES?  one is likely wrong!
            open.push_back(new_fnode);
            std::push_heap(open.begin(), open.end(), flagsearch_node_less);
          } // if !in_open
        } // if map->has_path_IMPROVED(node->pos, d)
      } // foreach direction
    } // while open flag-nodes remain to be checked

    // if the search ended it means no other Inventory building was found, so this road is likely not connected to the main
    //  road system.  That might be okay
    if (!found_inv)
      AILogDebug["util_identify_arterial_roads"] << "flagsearch never completed, flag at pos " << flag_pos << " is not connected to any valid Inventory!";

  } // foreach Flag pos

  // dump the entire search results
  for (std::pair<MapPos,std::map<Direction,std::map<MapPos, unsigned int>>>  inv_pair : flag_counts){
    MapPos inv_flag_pos = inv_pair.first;
    AILogDebug["util_identify_arterial_roads"] << "DUMP inv_pos " << inv_flag_pos;
    for (std::pair<Direction,std::map<MapPos, unsigned int>> dir_pair : inv_pair.second){
      Direction dir = dir_pair.first;
      AILogDebug["util_identify_arterial_roads"] << "DUMP         dir " << NameDirection[dir] << " / " << dir;
      for (std::pair<MapPos, unsigned int> flag_count_pair : dir_pair.second){
        MapPos flag_pos = flag_count_pair.first;
        unsigned int count = flag_count_pair.second;
        AILogDebug["util_identify_arterial_roads"] << "DUMP                 flag_pos " << flag_pos << " seen " << count << " times";
      }
    }
  }

  // record the arterial flags
  for (std::pair<MapPos,std::map<Direction,std::map<MapPos, unsigned int>>>  inv_pair : flag_counts){
    MapPos inv_flag_pos = inv_pair.first;
    AILogDebug["util_identify_arterial_roads"] << "MEDIAN inv_flag_pos " << inv_flag_pos;
    for (std::pair<Direction,std::map<MapPos, unsigned int>> dir_pair : inv_pair.second){
      Direction dir = dir_pair.first;
      AILogDebug["util_identify_arterial_roads"] << "MEDIAN         dir " << NameDirection[dir] << " / " << dir;

      // find the median "number of times a flag appears in the Inventory flag's road network in this direction"
      //  and use this as the cutoff for a flag to be "arterial"
      std::vector<unsigned int> counts = {};
      for (std::pair<MapPos, unsigned int> flag_count_pair : dir_pair.second){
        MapPos flag_pos = flag_count_pair.first;
        unsigned int count = flag_count_pair.second;
        counts.push_back(count);
        AILogDebug["util_identify_arterial_roads"] << "MEDIAN                 flag_pos " << flag_pos << " count " << count;
      }
      sort(counts.begin(), counts.end());
      size_t size = counts.size();
      size_t median = 0;
      // middle record is median value
      //median = counts[size / 2];
      // changing this to 70th percentile
      median = counts[size * 0.7];
      AILogDebug["util_identify_arterial_roads"] << "the median count " << median;

      // build MapPosVector of all flags that are > median, this is the arterial flag-path 
      //  in this direction from this Inventory
      MapPosVector art_flags = {};
      for (std::pair<MapPos, unsigned int> flag_count_pair : dir_pair.second){
        MapPos flag_pos = flag_count_pair.first;
        unsigned int count = flag_count_pair.second;
        if (count > median){
          AILogDebug["util_identify_arterial_roads"] << "MEDIAN                 flag_pos " << flag_pos << " count " << count << " is above median, including";
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
    AILogDebug["util_identify_arterial_roads"] << "begin retracing solutions - Inventory at pos " << inv_flag_pos << " has a path in Dir " << NameDirection[dir] << " / " << dir;

    Flag *inv_flag = game->get_flag_at_pos(inv_flag_pos);
    if (inv_flag == nullptr)
      continue;
    if (inv_flag->get_owner() != player_index)
      continue;
    if (!inv_flag->is_connected())
      continue;

    AILogDebug["util_identify_arterial_roads"] << "retracing solutions for inv_flag_pos " << inv_flag_pos << " dir " << NameDirection[dir] << " / " << dir << " - starting flagsearch from inv_flag_pos " << inv_flag_pos;
    MapPosVector closed = {};
    MapPosVector *closed_ptr = &closed;
    int depth = 0;
    int *depth_ptr = &depth;
    arterial_road_depth_first_recursive_flagsearch(inv_flag_pos, flag_dir, closed_ptr, depth_ptr);

    AILogDebug["util_identify_arterial_roads"] << "done retracing solutions - Inventory at pos " << inv_flag_pos << " has a path in Dir " << NameDirection[dir] << " / " << dir;
  }

  start = std::clock();
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_identify_arterial_roads"] << "done AI::identify_arterial_roads, call took " << duration;
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
  //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "starting with flag_pos " << flag_pos << ", recusion depth " << *(depth);
  MapPosVector arterial_flags = ai_mark_arterial_road_flags->at(inv_dir);
  //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo0";
  // find the Dir from this flag to the next flag and see if it is an arterial flag
  for (Direction dir : cycle_directions_cw()){
    //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo1, dir " << dir;
    if (!map->has_path_IMPROVED(flag_pos, dir))
      continue;
    if (!map->has_flag(flag_pos)){
      AILogError["arterial_road_depth_first_recursive_flagsearch"] << "expecting that start_pos provided to this function is a flag pos, but game->get_flag_at_pos(" << flag_pos << ") returned nullptr!  throwing exception";
      throw ExceptionFreeserf("expecting that start_pos provided to this function is a flag pos, but game->get_flag_at_pos(flag_pos)!");
    }
    Flag *flag = game->get_flag_at_pos(flag_pos);
    Flag *other_end_flag = flag->get_other_end_flag(dir);
    if (other_end_flag == nullptr){
      AILogWarn["arterial_road_depth_first_recursive_flagsearch"] << "got nullptr for game->get_other_end_flag(" << NameDirection[dir] << ") from flag at pos " << flag_pos << " skipping this dir";
      continue;
    }
    //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo2";
    MapPos other_end_flag_pos = other_end_flag->get_position();
    // if other_end_flag_pos is on the arterial flag list for this Inventory-Dir
    //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo3";
    if (std::find(arterial_flags.begin(), arterial_flags.end(), other_end_flag_pos) != arterial_flags.end()){
      //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo4";
      if (std::find(closed->begin(), closed->end(), other_end_flag_pos) == closed->end()){
        //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo5+ closing pos";
        closed->push_back(other_end_flag_pos);
        //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo5+ pushing pair to vector";
        ai_mark_arterial_road_pairs->at(inv_dir).push_back(std::make_pair(flag_pos,dir));
        // recursively search this new flag_pos the same way
        //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo6 entering recurse call";
        arterial_road_depth_first_recursive_flagsearch(other_end_flag_pos, inv_dir, closed, depth);
      }else{
        //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo5- pos is already on closed list";
      }
    } 
  }
  //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "done with node pos " << flag_pos;
}



//
// perform breadth-first FlagSearch to find best flag-path from start_pos 
//  to target_pos and determine flag path along the way.
//
// return true if solution found, false if not
//
// ALSO SET:
//
// MapPosVector *solution_flags
//  vector of the MapPos of all of the Flags in the solution, in *reverse* order,
//  including both start and end flags
// unsigned int *tile_dist
//  total TILE length of the complete solution, which may pass through many flags
//
//  this function will NOT work for fake flags / splitting flags
//
// this function is NOT optimal and is written for understanding and visualizing
//  the search.  A more common search would nodes to open before checking them
//  but when visualizing a search it appears that the search is going past the
//  target.  Instead, this search checks every new node as it is first found
//  so that it can quit and return solution immediately
//
// also, note that this is not a "A*" priority queue search like the Pathfinder.cc, 
//  there is no node comparisons because flags all have equal priority aside from their
//  distance from start_pos, and there is no heuristic because general direction towards
//  goal does not matter, only flag dist.  The distance from start pas is effectively handled in
//  correct priority order because it does breadth-first and if(closed) checking should prevent looping
//
//
// OPTIMIZATION - because this call is repeated many times in quick succession for a single
//  build_best_road call, especially for ReconnectNetworks, it makes sense to cache
//  the results for the some duration?
//
bool
AI::find_flag_path_and_tile_dist_between_flags(PMap map, MapPos start_pos, MapPos target_pos, 
              MapPosVector *solution_flags, unsigned int *tile_dist, ColorDotMap *ai_mark_pos){

  AILogDebug["find_flag_path_and_tile_dist_between_flags"] << "start_pos " << start_pos << ", target_pos " << target_pos;

  // sanity check - this excludes fake flag solution starts (could change that later though)
  if (!map->has_flag(start_pos)){
    /*
    AILogError["find_flag_path_and_tile_dist_between_flags"] << "expecting that start_pos " << start_pos << " provided to this function is a flag pos, marking start_pos blue and throwing exception";
    ai_mark_pos->erase(start_pos);
    ai_mark_pos->insert(ColorDot(start_pos, "blue"));
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    throw ExceptionFreeserf("expecting that start_pos provided to this function is a flag pos");
    */
    // I think this could simply be a result of lost territory, not an exception.  Only started seeing it with multiple players
    AILogWarn["find_flag_path_and_tile_dist_between_flags"] << "expecting that start_pos " << start_pos << " provided to this function is a flag pos, maybe it was removed?  returng bad_score";
    return false;
  }

  // handle start=end
  if (start_pos == target_pos){
    AILogWarn["find_flag_path_and_tile_dist_between_flags"] << "start_pos " << start_pos << " IS target_pos " << target_pos << ", why even make this call?";
    return true;
  }

  std::list<PFlagSearchNode> open;   // flags discovered but not checked for adjacent flags.  List is of Nodes instead of MapPos because needs parent, dist, etc.
  std::list<MapPos> closed;          // flags already checked in all Dirs (by MapPos).  List of MapPos is fine because it only exists to avoid re-checking
  
  // set the first node of the search to the start_pos
  //  and put it in the open list
  PFlagSearchNode fnode(new FlagSearchNode);
  fnode->pos = start_pos;
  open.push_back(fnode);

  while (!open.empty()) {

    fnode = open.front(); // read the first element
    open.pop_front(); // remove the first element that was just read and put into fnode

    // get this Flag
    Flag *flag = game->get_flag_at_pos(fnode->pos);
    if (flag == nullptr){
      AILogWarn["find_flag_path_and_tile_dist_between_flags"] << "got nullptr for game->get_flag_at_pos " << fnode->pos << " skipping this dir";
      throw ExceptionFreeserf("got nullptr for game->get_flag_at_pos");
      continue;
    }
    
    // find adjacent flags to check
    for (Direction dir : cycle_directions_cw()){

      // skip dir if no path
      if (!map->has_path_IMPROVED(fnode->pos, dir))
        continue;

      // avoid "building that acccepts resources appears to have a path UpLeft" issue
      // could check if building accepts resources, but does it matter? reject any building
      if (dir == DirectionUpLeft && flag->has_building())
        continue;

      // get the other Flag in this dir
      Flag *other_end_flag = flag->get_other_end_flag(dir);
      if (other_end_flag == nullptr){
        AILogError["find_flag_path_and_tile_dist_between_flags"] << "got nullptr for game->get_other_end_flag(" << NameDirection[dir] << ") from flag at pos " << fnode->pos << ", marking in coral and throwing exception";
        ai_mark_pos->erase(map->move(fnode->pos, dir));
        ai_mark_pos->insert(ColorDot(map->move(fnode->pos, dir), "coral"));
        std::this_thread::sleep_for(std::chrono::milliseconds(30000));
        throw ExceptionFreeserf("got nullptr for game->get_other_end_flag");
      }
      MapPos other_end_flag_pos = other_end_flag->get_position();
      AILogDebug["find_flag_path_and_tile_dist_between_flags"] << "other_end_flag_pos is " << other_end_flag_pos;

      // skip dir if adjacent flag pos is already in the closed list
      if (std::find(closed.begin(), closed.end(), other_end_flag_pos) != closed.end())
        continue;

      // skip dir if adjacent flag pos is already in the open list
      bool already_in_open = false;
      for (PFlagSearchNode tmp_fnode : open){
        if (tmp_fnode->pos == other_end_flag_pos){
          already_in_open = true;
          break;
        }
      }
      if (already_in_open)
        continue;

      // this flag pos is not yet known, create a new node
      PFlagSearchNode new_fnode(new FlagSearchNode);
      new_fnode->pos = other_end_flag_pos;
      new_fnode->parent = fnode;
      // this is the dir from the PARENT to this child fnode
      //  but it shouldn't be set in the parent because the parent can have multiple children
      //  so it would require storing multiple dirs which is more complex.  So, again, the
      //  CHILD->dir represents the dir that the child's PARENT connects to the CHILD node
      // so far this is only used for trace_existing_road call, if road tracing is removed
      //  we don't even need this dir anymore
      new_fnode->dir = dir;  

      // stop here to check if the new pos is the target_pos
      //  and if so retrace it and record the solution
      if (new_fnode->pos == target_pos){
        AILogDebug["find_flag_path_and_tile_dist_between_flags"] << "found solution, reached " << target_pos;
        PFlagSearchNode solution_node = new_fnode;
        while(solution_node->parent){
          AILogDebug["find_flag_path_and_tile_dist_between_flags"] << "solution contains " << solution_node->pos;
          solution_flags->push_back(solution_node->pos);

          // also trace the road to determine length in tiles of the solution
          Road tmp_road = trace_existing_road(map, solution_node->parent->pos, solution_node->dir);
          *(tile_dist) += tmp_road.get_length();

          solution_node = solution_node->parent;
          
        }
        // push the last node too, and return
        AILogDebug["find_flag_path_and_tile_dist_between_flags"] << "solution last node " << solution_node->pos;
        solution_flags->push_back(solution_node->pos);
        return true;
      }

      // this new node is not the target_pos, add to open list
      //  to continue the search
      open.push_back(new_fnode);

    } //end foreach Direction

    // all dirs checked for this node, put this pos on closed list
    closed.push_back(fnode->pos);

  } //end while(!open.empty)

  AILogDebug["find_flag_path_and_tile_dist_between_flags"] << "search completed with no solution found, returning false";
  return false;
} //end function
