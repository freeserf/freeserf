/*
 * ai_pathfinder.h - extra pathfinder functions used for AI
 *	   Copyright 2019-2020 tlongstretch
 */

#ifndef SRC_AI_PATHFINDER_H_
#define SRC_AI_PATHFINDER_H_

#include "src/map.h"
#include "src/lookup.h"
#include "src/ai_roadbuilder.h"

static const unsigned int contains_castle_flag_penalty = 10;  // fixed penalty for a non-direct road that contains the castle flag (but doesn't start/end there)

Road plot_road(PMap map, unsigned int player_index, MapPos start, MapPos end, Roads * const &potential_roads);
bool can_build_flag(PMap map, MapPos pos, unsigned int player_index);  // copied from game.cc so the entire game.h doesn't have to be included
int get_straightline_tile_dist(PMap map, MapPos start_pos, MapPos end_pos);
bool score_flag(PMap map, unsigned int player_index, RoadBuilder *rb, RoadOptions road_options, MapPos flag_pos, MapPos castle_flag_pos, ColorDotMap *ai_mark_pos);
bool find_flag_and_tile_dist(PMap map, unsigned int player_index, RoadBuilder *rb, MapPos flag_pos, MapPos castle_flag_pos, ColorDotMap *ai_mark_pos);
Road trace_existing_road(PMap map, MapPos start_pos, Direction dir);
RoadEnds get_roadends(PMap map, Road road);
Road reverse_road(PMap map, Road road);


#endif  // SRC_AI_PATHFINDER_H_
