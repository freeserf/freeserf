/*
 * ai_roadbuilder.cc - class to store the state of a single pathfinding attempt
 *   tlongstretch 2019-2020
 *
 *
 * this class was originally created thinking I would build and maintain a cache of all roads
 *   but I never bothered with the caching piece because it was such a nightmare to work on 
 *   this code.  It seems to work fine but could improved
 */

#include "src/ai_roadbuilder.h"

RoadBuilderRoad::RoadBuilderRoad() {
	end1 = bad_map_pos;
	end2 = bad_map_pos;
	dir1 = DirectionNone;
	dir2 = DirectionNone;
	contains_castle_flag = false;
}

/*
RoadBuilderRoad::RoadBuilderRoad(MapPos e1, Direction d1, MapPos e2, Direction d2) {
	MapPos end1 = e1;
	MapPos end2 = e2;
	Direction dir1 = d1;
	Direction dir2 = d2;
	bool contains_castle_flag = false;
}
*/

RoadBuilderRoad::RoadBuilderRoad(RoadEnds ends, Road r) {
	Log::Info["ai_util"] << " inside RoadBuilderRoad::RoadBuilderRoad";
	end1 = std::get<0>(ends);
	dir1 = std::get<1>(ends);
	end2 = std::get<2>(ends);
	dir2 = std::get<3>(ends);
	road = r;
	contains_castle_flag = false;
	Log::Info["ai_util"] << "constructed RoadBuilderRoad::RoadBuilderRoad with end1 " << end1 <<
		", dir1 " << NameDirection[dir1] << ", end2 " << end2 << ", dir2 " << NameDirection[dir2] << ", road length " << road.get_length();
}


FlagScore::FlagScore() {
	unsigned int flag_dist = 0;
	unsigned int tile_dist = 0;
	bool contains_castle_flag = false;
}

FlagScore::FlagScore(unsigned int fd, unsigned int td) {
	unsigned int flag_dist = fd;
	unsigned int tile_dist = td;
	bool contains_castle_flag = false;
}

RoadBuilder::RoadBuilder() {
	start_pos = bad_map_pos;
	target_pos = bad_map_pos;
	//roads_built = 0;
}
RoadBuilder::RoadBuilder(MapPos sp, MapPos tp) {
	start_pos = sp;
	target_pos = tp;
	//roads_built = 0;
}

