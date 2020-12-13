/*
 * ai_util.cc - AI related functions
 *   tlongstretch 2019-2020
 */

#include "src/ai.h"

// convert distance-from-center-in-rows to spiral distance (for map->add_spirally)
unsigned int
AI::spiral_dist(int distance) {
	/* NOTE - the original game map->pos_add_spirally function this is ultimately 
	    used to call only goes up to 295!!! after that it gets a random MapPos!
		// Initialize spiral_pos_pattern from spiral_pattern. 
		void
		Map::init_spiral_pos_pattern() {
		for (int i = 0; i < 295; i++) {
			int x = spiral_pattern[2 * i] & geom_.col_mask();
			int y = spiral_pattern[2 * i + 1] & geom_.row_mask();

			spiral_pos_pattern[i] = pos(x, y);
		}
	}*/

	// FIX - added a new function that has larger array:  map->pos_add_extended_spirally
	//   AI is now using this, allows up to spiral_dist[24]
	if (distance > 24) {
		Log::Error["ai_util"] << "CANNOT USE spiral_dist() GREATER THAN 24!!!";
	}

	// game spirals are hexagonal,
	//  one spiral row count equals (6*rows from center) + (previous row's value)
	// row   spiral-distance
	// 0    =1 (center)
	// 1    =6
	// 2    =18
	// 3    =36
	// 4    =60
	// 5    =90
	// 6    =36+90=126
	// 7    =42+126 ... and so on
	
	// instead of calculating this every time, initialize it once and use an array
	/*
	// add one for the center pos
	int spiral_dist = 1;
	for (int x = 1; x < (distance + 1); x++) {
		spiral_dist += 6 * x;
	}
	AILogLogger["ai_util"] << "spiral distance of rows " << distance << " is " << spiral_dist;
	return spiral_dist;
	*/
	return _spiral_dist[distance];
}


// return true if *any* of the four points contain the requested terrain type
bool
AI::has_terrain_type(PGame game, MapPos pos, Map::Terrain res_start_index, Map::Terrain res_end_index) {
	// static
	//AILogLogger["has_terrain_type"] << name << " inside AI::has_terrain_type";
	Log::Debug["util_has_terrain_type"] << " inside AI::has_terrain_type";
	PMap map = game->get_map();
	Map::Object obj = map->get_obj(pos);
	Map::Terrain t1 = map->type_down(pos);
	Map::Terrain t2 = map->type_up(pos);
	Map::Terrain t3 = map->type_down(map->move_up_left(pos));
	Map::Terrain t4 = map->type_up(map->move_up_left(pos));
	if ((t1 >= res_start_index && t1 <= res_end_index) ||
		(t2 >= res_start_index && t2 <= res_end_index) ||
		(t3 >= res_start_index && t3 <= res_end_index) ||
		(t4 >= res_start_index && t4 <= res_end_index)) {
		return true;
	}
	return false;
}

// score specified area in terms of castle placement
//   initially this is just ensuring enough wood, stones, and building sites
//   long term it should also care about resources in the surrounding areas!  including mountains, fishable waters, etc.
bool
AI::place_castle(PGame game, MapPos center_pos, unsigned int distance) {
	AILogLogger["util_place_castle"] << name << " inside AI::place_castle, center_pos " << center_pos << ", distance " << distance;
	PMap map = game->get_map();
	unsigned int trees = 0;
	unsigned int stones = 0;
	unsigned int building_sites = 0;
	for (unsigned int i = 0; i < distance; i++) {
		MapPos pos = map->pos_add_extended_spirally(center_pos, i);
		Map::Object obj = map->get_obj(pos);
		if (obj >= Map::ObjectTree0 && obj <= Map::ObjectPine7) {
			trees += 1;
			//AILogLogger["util_place_castle"] << name << " adding trees count 1";
		}
		if (obj >= Map::ObjectStone0 && obj <= Map::ObjectStone7) {
			int stonepile_value = 1 + (-1 * (obj - Map::ObjectStone7));
			stones += stonepile_value;
			//AILogLogger["util_place_castle"] << name << " adding stones count " << stonepile_value;
		}
		if (game->can_build_large(pos)) {
			building_sites += 3;
			//AILogLogger["util_place_castle"] << name << " adding large building value 3";
		}
		else if (game->can_build_small(pos)) {
			building_sites += 1;
			//AILogLogger["util_place_castle"] << name << " adding small building value 1";
		}
	}

	AILogLogger["util_place_castle"] << name << " found trees: " << trees << ", stones: " << stones << ", building_sites: " << building_sites << " in area " << center_pos;
	if (trees < (near_trees_min * 3)) {
		AILogLogger["util_place_castle"] << name << " not enough trees, min is " << near_trees_min * 3 << ", returning false";
		return false;
	}
	if (stones < near_stones_min) {
		AILogLogger["util_place_castle"] << name << " not enough stones, min is " << near_stones_min << ", returning false";
		return false;
	}
	if (building_sites < near_building_sites_min) {
		AILogLogger["util_place_castle"] << name << " not enough building_sites, min is " << near_building_sites_min << ", returning false";
		return false;
	}
	AILogLogger["util_place_castle"] << name << " center_pos: " << center_pos << " is an acceptable building site for a castle";
	AILogLogger["util_place_castle"] << name << " done AI::place_castle";
	return true;
}


// update current AI player's inventory of buildings of all types for lookup by various functions
void
AI::update_building_counts() {
	// time this function for debugging
	std::clock_t start;
	double duration;
	start = std::clock();

	AILogLogger["util_update_building_counts"] << name << " inside AI::update_building_counts";
	AILogLogger["util_update_building_counts"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex inside AI::update_building_counts";
	game->get_mutex()->lock();
	AILogLogger["util_update_building_counts"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex inside AI::update_building_counts";
	Game::ListBuildings buildings = game->get_player_buildings(player);
	AILogLogger["util_update_building_counts"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex inside AI::update_building_counts";
	game->get_mutex()->unlock();
	AILogLogger["util_update_building_counts"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex inside AI::update_building_counts";
	// reset all to zero
	memset(building_count, 0, sizeof(building_count));
	memset(completed_building_count, 0, sizeof(completed_building_count));
	//memset(incomplete_building_count, 0, sizeof(incomplete_building_count));
	memset(occupied_building_count, 0, sizeof(occupied_building_count));
	memset(connected_building_count, 0, sizeof(connected_building_count));
	for (MapPos stock_pos : stocks_pos) {
		AILogLogger["util_update_building_counts"] << name << " inside AI::update_building_counts, clearing counts for stock_pos " << stock_pos;
		memset(stock_buildings.at(stock_pos).count, 0, sizeof(stock_buildings.at(stock_pos).count));
		memset(stock_buildings.at(stock_pos).completed_count, 0, sizeof(stock_buildings.at(stock_pos).completed_count));
		memset(stock_buildings.at(stock_pos).occupied_count, 0, sizeof(stock_buildings.at(stock_pos).occupied_count));
		memset(stock_buildings.at(stock_pos).connected_count, 0, sizeof(stock_buildings.at(stock_pos).connected_count));
		stock_buildings.at(stock_pos).occupied_military_pos.clear();
		stock_buildings.at(stock_pos).unfinished_count = 0;
		stock_buildings.at(stock_pos).unfinished_hut_count = 0;
		AILogLogger["util_update_building_counts"] << name << " RESET unfinished_hut_count for stock_pos " << stock_pos << ", is now: " << stock_buildings.at(stock_pos).unfinished_hut_count;
		AILogLogger["util_update_building_counts"] << name << " RESET unfinished_building_count, for stock_pos " << stock_pos << ", is now: " << stock_buildings.at(stock_pos).unfinished_count;
	}

	for (Building *building : buildings) {
		//AILogLogger["util_update_building_counts"] << name << " has a building";
		if (building == nullptr)
			continue;
		//AILogLogger["util_update_building_counts"] << name << " has a building that is not nullptr";
		if (building->is_burning())
			continue;
		//AILogLogger["util_update_building_counts"] << name << " has a building that is not on fire";
		Building::Type type = building->get_type();
		AILogLogger["util_update_building_counts"] << name << " has a building of type " << NameBuilding[type];
		if (type == Building::TypeNone) {
			AILogLogger["util_update_building_counts"] << name << " has a building of TypeNone!  why does this happen??";
			continue;
		}
		if (type == Building::TypeCastle) {
			//AILogLogger["util_update_building_counts"] << name << " has a castle";
			if (!building->is_done()) {
				AILogLogger["util_update_building_counts"] << name << "'s castle isn't finished building yet";
				while (!building->is_done()) {
					AILogLogger["util_update_building_counts"] << name << "'s castle isn't finished building yet.  Sleeping a bit";
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				}
				AILogLogger["util_update_building_counts"] << name << "'s castle is now built, updating stocks";
				update_stocks_pos();
			}
			//AILogLogger["util_update_building_counts"] << name << " has a completed castle at pos " << building->get_position() << " with flag pos " << map->move_down_right(building->get_position());
			realm_occupied_military_pos.push_back(building->get_position());
			stock_buildings.at(map->move_down_right(building->get_position())).occupied_military_pos.push_back(building->get_position());
			continue;
		}
		if (type == Building::TypeStock && building->is_done())
			continue;
		AILogLogger["util_update_building_counts"] << name << " about to call find_nearest_stock for building at pos " << building->get_position() << " with type " << NameBuilding[type];
		MapPos nearest_stock = find_nearest_stock(building->get_position());
		AILogLogger["util_update_building_counts"] << name << " nearest stock to this building is " << nearest_stock;
		if (!building->is_done()) {
			if (type == Building::TypeHut) {
				//unfinished_hut_count++;
				stock_buildings.at(nearest_stock).unfinished_hut_count++;
				//AILogLogger["util_update_building_counts"] << name << " incrementing unfinished_hut_count, is now: " << unfinished_hut_count;
				AILogLogger["util_update_building_counts"] << name << " incrementing unfinished_hut_count for stock_pos " << nearest_stock << ", is now: " << stock_buildings.at(nearest_stock).unfinished_hut_count;
			}
			else if (building->get_type() == Building::TypeCoalMine
				|| building->get_type() == Building::TypeIronMine
				|| building->get_type() == Building::TypeGoldMine
				|| building->get_type() == Building::TypeStoneMine) {
				AILogLogger["util_update_building_counts"] << name << " unfinished building is a Mine, not incrementing unfinished_building_count";
			}
			else {
				//unfinished_building_count++;
				stock_buildings.at(nearest_stock).unfinished_count++;
				//AILogLogger["util_update_building_counts"] << name << " incrementing unfinished_building_count, is now: " << unfinished_building_count;
				AILogLogger["util_update_building_counts"] << name << " incrementing unfinished_building_count for stock_pos " << nearest_stock << ", is now: " << stock_buildings.at(nearest_stock).unfinished_count;
			}
		}
		building_count[type]++;
		stock_buildings.at(nearest_stock).count[type]++;
		if (game->get_flag(building->get_flag_index())->is_connected()) {
			connected_building_count[type]++;
			stock_buildings.at(nearest_stock).connected_count[type]++;
		}
		if (building->is_done()){
			completed_building_count[type]++;
			stock_buildings.at(nearest_stock).completed_count[type]++;
			// has_serf is not a good enough test alone to see if occupied, as it seems to be true when a builder is constructing the building!  
			//  so moved this check to inside building->is_done because if building is done the only serf there should be the professional (I think)
			if (building->has_serf()) {
				occupied_building_count[type]++;
				stock_buildings.at(nearest_stock).occupied_count[type]++;
			}
			if (building->is_military() && building->is_active()) {
				AILogLogger["util_update_building_counts"] << name << " adding occupied military building at " << building->get_position() << " to list for stock_pos " << nearest_stock;
				realm_occupied_military_pos.push_back(building->get_position());
				stock_buildings.at(nearest_stock).occupied_military_pos.push_back(building->get_position());
			}
		}
	}
	// debug
	for (MapPos stock_pos : stocks_pos) {
		AILogLogger["util_update_building_counts"] << name << " stock at pos " << stock_pos << " has all/completed/occupied buildings: ";
		int type = 0;
		for (int count : building_count) {
			AILogLogger["util_update_building_counts"] << name << " type " << type << " / " << NameBuilding[type] << ": " << stock_buildings.at(stock_pos).count[type]
				<< "/" << stock_buildings.at(stock_pos).completed_count[type] << "/" << stock_buildings.at(stock_pos).occupied_count[type];
			type++;
		}
	}
	AILogLogger["util_update_building_counts"] << name << " done AI::update_building_counts";
	duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;
	AILogLogger["util_update_building_counts"] << name << " done AI::update_building_counts call took " << duration;
}


// update current AI player's vector of built warehouse/stock positions 
//   INCLUDING the castle.  this should be the FLAG of the stock, not the stock itself
void
AI::update_stocks_pos() {
	// time this function for debugging
	std::clock_t start;
	double duration;
	start = std::clock();

	AILogLogger["util_update_stocks_pos"] << name << " inside AI::update_stocks_pos";
	AILogLogger["util_update_stocks_pos"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex inside AI::update_stocks_pos";
	game->get_mutex()->lock();
	AILogLogger["util_update_stocks_pos"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex inside AI::update_stocks_pos";
	Game::ListBuildings buildings = game->get_player_buildings(player);
	AILogLogger["util_update_stocks_pos"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex inside AI::update_stocks_pos";
	game->get_mutex()->unlock();
	AILogLogger["util_update_stocks_pos"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex inside AI::update_stocks_pos";
	stocks_pos = {};
	for (Building *building : buildings) {
		if (building == nullptr)
			continue;
		if (building->is_burning())
			continue;
		Building::Type type = building->get_type();
		if (type != Building::TypeCastle && type != Building::TypeStock)
			continue;
		if (type == Building::TypeStock && (!building->is_done() || !game->get_flag(building->get_flag_index())->is_connected()))
			continue;
		MapPos stock_flag_pos = map->move_down_right(building->get_position());
		AILogLogger["util_update_stocks_pos"] << name << " the castle or a completed, connected warehouse/stock was found at pos " << building->get_position() << ", with its flag at pos " << stock_flag_pos;
		stocks_pos.push_back(stock_flag_pos);
		//stock_buildings[stock_flag_pos] = { {0},{0},{0},{0},0,0,{} }
		//stock_buildings.insert(std::make_pair<MapPos, AI::StockBuildings>(stock_flag_pos,AI::StockBuildings{ {0},{0},{0},{0},0,0,{} }));
		stock_buildings[stock_flag_pos];
		//stock_buildings.insert(stock_flag_pos, *(new AI::StockBuildings));
	}
	AILogLogger["util_update_stocks_pos"] << name << " done AI::update_stocks_pos";
	duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;
	AILogLogger["util_update_stocks_pos"] << name << " done AI::update_stocks_pos call took " << duration;
}


// destroy all roads in realm, and all disconnected flags, then reconnect them in priority order
//   attempt to optimize roads once economy is complete.  not working correctly yet
//     this is a test/hack and NOT used normally (or ever right now)
void
AI::rebuild_all_roads() {
	AILogLogger["util_rebuild_all_roads"] << name << " inside AI::rebuild_all_roads";
	MapPosVector flag_positions;
	AILogLogger["util_rebuild_all_roads"] << name << " destroying all paths";
	AILogLogger["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex for entire rebuild_all_roads function before destroying all roads";
	game->get_mutex()->lock();
	AILogLogger["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex for entire rebuild_all_roads function before destroying all roads";
	flags_static_copy = *(game->get_flags());
	flags = &flags_static_copy;
	for (Flag *flag : *flags) {
		//// exclude flags connected to knight huts, keep these because they tend to be at the ends of resource chains?
		//if (!flag->has_building() || flag->get_building()->get_type() != Building::TypeHut) {
		//}
		//
		// only include roads with some distance from the castle??
		//
		if (flag == nullptr || flag->get_owner() != player_index) 
			continue;
		for (Direction dir : cycle_directions_cw()) {
			if (map->has_path(flag->get_position(), dir))
				game->demolish_road(map->move(flag->get_position(), dir), player);
		}
		flag_positions.push_back(flag->get_position());
	}
	AILogLogger["util_rebuild_all_roads"] << name << " destroying all unattached flags";
	for (MapPos flag_pos : flag_positions) {
		if (map->has_flag(flag_pos))
			game->demolish_flag(flag_pos, player);
	}

	AILogLogger["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex for entire rebuild_all_roads function after destroying all roads";
	game->get_mutex()->unlock();
	AILogLogger["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex for entire rebuild_all_roads function after destroying all roads";

	AILogLogger["util_rebuild_all_roads"] << name << "sleeping to see roads destroyed before starting rebuild";
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	//road_options.reset(RoadOption::PenalizeNewLength);
	road_options.set(RoadOption::ReducedNewLengthPenalty);

	std::vector<Building::Type> rebuild_order = {
			Building::TypeSawmill,
			Building::TypeLumberjack,
			Building::TypeWeaponSmith,
			Building::TypeSteelSmelter,
			Building::TypeGoldSmelter,
			Building::TypeFarm,
			Building::TypeMill,
			//Building::TypeBaker,
			//Building::TypeCoalMine,
			//Building::TypeIronMine,
			//Building::TypeGoldMine
	};

	// IDEAS...
	//  connect bakers to ALL mines
	//  connect warehouse/stocks to ALL very-nearby flags (until all paths full)

	AILogLogger["util_rebuild_all_roads"] << name << " trying to connect high priority buildings first";
	//AILogLogger["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex for entire rebuild_all_roads function before rebuilding high-priority building roads";
	//game->get_mutex()->lock();
	//AILogLogger["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex for entire rebuild_all_roads function before rebuilding high-priority building roads";
	for (Building::Type type : rebuild_order) {
		AILogLogger["util_rebuild_all_roads"] << name << " trying to connect buildings of type " << NameBuilding[type];
		for (Building *building : game->get_player_buildings(player)) {
			if (building->get_type() != type) {
				continue;
			}
			if (building->is_burning()) {
				AILogLogger["util_rebuild_all_roads"] << name << " this building is on fire!  skipping";
				continue;
			}
			// skip mines that aren't yet built
			if (!building->is_done() && building->get_type() >= Building::TypeStoneMine && building->get_type() <= Building::TypeGoldMine)
				continue;
			ai_mark_pos.insert(ColorDot(building->get_position(), "blue"));
			if (!AI::build_best_road(map->move_down_right(building->get_position()), road_options)) {
				AILogLogger["util_rebuild_all_roads"] << name << " failed to connect high priority building at pos " << building->get_position() << " to affinity building / road network!";
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			//ai_mark_pos.clear();
			ai_mark_pos.erase(building->get_position());

		}
	}

	//game->get_mutex()->unlock();
	//std::this_thread::sleep_for(std::chrono::milliseconds(5000));
	//game->get_mutex()->lock();
	AILogLogger["util_rebuild_all_roads"] << name << " trying to connect bakers to mines";
	for (Building *building : game->get_player_buildings(player)) {
		if (building->get_type() != Building::TypeBaker) {
			continue;
		}
		if (building->is_burning()) {
			AILogLogger["util_rebuild_all_roads"] << name << " this building is on fire!  skipping";
			continue;
		}
		AILogLogger["util_rebuild_all_roads"] << name << " found a baker at pos " << map->move_down_right(building->get_position());
		ai_mark_pos.insert(ColorDot(building->get_position(), "orange"));
		if (!AI::build_best_road(map->move_down_right(building->get_position()), road_options, Building::TypeCoalMine)) {
			AILogLogger["util_rebuild_all_roads"] << name << " failed to connect baker at pos " << building->get_position() << " to coal mine / road network!";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		ai_mark_pos.clear();
		if (!AI::build_best_road(map->move_down_right(building->get_position()), road_options, Building::TypeIronMine)) {
			AILogLogger["util_rebuild_all_roads"] << name << " failed to connect baker at pos " << building->get_position() << " to iron mine / road network!";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		ai_mark_pos.clear();
		if (!AI::build_best_road(map->move_down_right(building->get_position()), road_options, Building::TypeGoldMine)) {
			AILogLogger["util_rebuild_all_roads"] << name << " failed to connect baker at pos " << building->get_position() << " to gold mine / road network!";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		ai_mark_pos.clear();
	}

	//
	// second run, try mines again
	//
	//road_options.reset(RoadOption::PenalizeNewLength);
	road_options.set(RoadOption::ReducedNewLengthPenalty);

	std::vector<Building::Type> rebuild_order2 = {
			Building::TypeCoalMine,
			Building::TypeIronMine,
			Building::TypeGoldMine
	};

	AILogLogger["util_rebuild_all_roads"] << name << " trying to connect 2nd run high priority buildings";
	//AILogLogger["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex for entire rebuild_all_roads function before rebuilding high-priority building roads";
	//game->get_mutex()->lock();
	//AILogLogger["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex for entire rebuild_all_roads function before rebuilding high-priority building roads";
	for (Building::Type type : rebuild_order2) {
		AILogLogger["util_rebuild_all_roads"] << name << " trying to connect buildings of type " << NameBuilding[type];
		for (Building *building : game->get_player_buildings(player)) {
			if (building->get_type() != type) {
				continue;
			}
			if (building->is_burning()) {
				AILogLogger["util_rebuild_all_roads"] << name << " this building is on fire!  skipping";
				continue;
			}
			// skip mines that aren't yet built
			if (!building->is_done() && building->get_type() >= Building::TypeStoneMine && building->get_type() <= Building::TypeGoldMine)
				continue;
			ai_mark_pos.insert(ColorDot(building->get_position(), "purple"));
			if (!AI::build_best_road(map->move_down_right(building->get_position()), road_options)) {
				AILogLogger["util_rebuild_all_roads"] << name << " failed to connect high priority building at pos " << building->get_position() << " to affinity building / road network!";
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			//ai_mark_pos.clear();
			ai_mark_pos.erase(building->get_position());

		}
	}


	//game->get_mutex()->unlock();
	//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	//game->get_mutex()->lock();
	//AILogLogger["util_rebuild_all_roads"] << name << " breaking early to debug";
	//return;

	road_options.set(RoadOption::PenalizeNewLength);
	road_options.reset(RoadOption::ReducedNewLengthPenalty);

	AILogLogger["util_rebuild_all_roads"] << name << " trying to connect remaining civilian buildings (except rangers and incomplete mines)";
	for (Building *building : game->get_player_buildings(player)) {
		if (building->is_military())
			continue;
		if (building->get_type() == Building::TypeForester && building->is_done())
			continue;
		if (building->is_burning()) {
			AILogLogger["util_rebuild_all_roads"] << name << " this civilian building is on fire!  skipping";
			continue;
		}
		// skip mines that aren't yet built
		if (!building->is_done() && building->get_type() >= Building::TypeStoneMine && building->get_type() <= Building::TypeGoldMine)
			continue;
		// skip buildings that already have paths (from earlier high priority buildings connections)
		if (game->get_flag(building->get_flag_index())->is_connected())
			continue;
		//ai_mark_pos.insert(ColorDot(building->get_position(), "green"));
		if (!AI::build_best_road(map->move_down_right(building->get_position()), road_options)) {
			AILogLogger["util_rebuild_all_roads"] << name << " failed to connect civilian building at pos " << building->get_position() << " to road network!";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		//ai_mark_pos.clear();
		//ai_mark_pos.erase(building->get_position());
	}
	//game->get_mutex()->unlock();
	//std::this_thread::sleep_for(std::chrono::milliseconds(5000));
	//game->get_mutex()->lock();
	AILogLogger["util_rebuild_all_roads"] << name << " trying to connect military buildings";
	for (Building *building : game->get_player_buildings(player)) {
		if (building->is_military()) {
			if (building->is_burning()) {
				AILogLogger["util_rebuild_all_roads"] << name << " this military building is on fire!  skipping";
				continue;
			}
			ai_mark_pos.insert(ColorDot(building->get_position(), "coral"));
			if (!AI::build_best_road(map->move_down_right(building->get_position()), road_options)) {
				AILogLogger["util_rebuild_all_roads"] << name << " failed to connect military building at pos " << building->get_position() << " to road network!";
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			//ai_mark_pos.clear();
		}
	}
	//AILogLogger["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex for entire rebuild_all_roads function";
	//game->get_mutex()->unlock();
	//AILogLogger["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex for entire rebuild_all_roads function";
}


// try to build a road from the specified flag pos to the best end flag, return success/failure
//  If any roads already exist from specified flag to each affinity building, 
//    build a better road if one is found.  Do not remove the old one
bool
AI::build_best_road(MapPos start_pos, RoadOptions road_options, Building::Type optional_affinity, MapPos optional_target) {
	// time this function for debugging
	std::clock_t start;
	double duration;
	start = std::clock();

	AILogLogger["util_build_best_road"] << name << " inside AI::build_best_road with start pos " << start_pos << ", optional_affinity " << NameBuilding[optional_affinity] << ", optional_target " << optional_target;
	// print RoadOptions for debugging
	for (int i = 0; i < road_options.size(); i++) {
		AILogLogger["util_build_best_road"] << name << " RoadOption: " << NameRoadOption[i] << " = " << bool(road_options.test(i));
	}
	// sanity check request
	if (!game->get_flag_at_pos(start_pos)) {
		AILogLogger["util_build_best_road"] << name << " No flag exists at start_pos " << start_pos << "!  returning false";
		return false;
	}
	if (map->get_owner(start_pos) != player_index) {
		AILogLogger["util_build_best_road"] << name << " Flag at start_pos " << start_pos << " is not owned by Player" << player_index << "!  returning false";
		return false;
	}
	// check if this start_flag already has any paths
	if (game->get_flag_at_pos(start_pos)->is_connected()) {
		AILogLogger["util_build_best_road"] << name << " at least one path already exists from flag at start_pos";
		// do not cancel if this condition happens, because it is normal when doing rebuild_all_roads
		//if (!road_options.test(RoadOption::Improve)) {
		//	AILogLogger["util_build_best_road"] << name << " a path already exists from start_pos " << start_pos << " but RoadOption::Improve is false!  this is unexpected.  returning false";
		//	return false;
		//}
		if (!road_options.test(RoadOption::Direct)) {
			AILogLogger["util_build_best_road"] << name << " build_best_road will try to build a better road than the current best one";
		}
		// skip if no more paths can be built from starting flag
		bool can_build_another_path = false;
		for (Direction dir : cycle_directions_ccw()) {
			if (!game->get_flag_at_pos(start_pos)->has_path(dir) && map->is_road_segment_valid(start_pos, dir)) {
				can_build_another_path = true;
				break;
			}
		}
		if (!can_build_another_path) {
			AILogLogger["util_build_best_road"] << name << "no more paths can be build from this building's flag!  returning false";
			return false;
		}
	}
	else {
		AILogLogger["util_build_best_road"] << name << " start_pos has no paths, will try to connect it to road network";
	}

	// check BuildingAffinity table to see if the start_pos is attached to a building that should prioritize connection to another existing building type
	//  if there is no affinity the default target is the player's castle or nearest stock

	MapPosVector targets;  // these are flag positions

	if (optional_affinity != Building::TypeNone) {
		// for rebuild all roads (at least), override affinity table and instead use the specified affinity building type
		AILogLogger["util_build_best_road"] << name << " using optional_affinity " << NameBuilding[optional_affinity] << " specified in build_best_road call";
		// ************************** **************************
		// make this a function, also do same inside AI::get_affinity it is repetative copy/paste
		// ************************** **************************
		AILogLogger["util_build_best_road"] << name << " looking for nearest completed building of type " << NameBuilding[optional_affinity];
		Building *building = AI::find_nearest_completed_building(start_pos, AI::spiral_dist(15), optional_affinity);
		if (building != nullptr) {
			AILogLogger["util_build_best_road"] << name << " found completed optional_affinity building at pos " << building->get_position();
			// get the flag position of the building
			MapPos building_flag_pos = map->move_down_right(building->get_position());
			AILogLogger["util_build_best_road"] << name << " setting road_to target to building_flag_pos " << building_flag_pos;
			targets.push_back(building_flag_pos);
		}
		else {
			AILogLogger["util_build_best_road"] << name << " couldn't find any completed optional_affinity building nearby, checking entire realm";
			bool found = false;
			AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for optional_affinity)";
			game->get_mutex()->lock();
			AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for optional_affinity)";
			Game::ListBuildings buildings = game->get_player_buildings(player);
			AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player) (for optional_affinity)";
			game->get_mutex()->unlock();
			AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player) (for optional_affinity)";
			for (Building *building : buildings) {
				if (building->get_type() != optional_affinity)
					continue;
				if (building->is_burning())
					continue;
				if (!building->is_done())
					continue;
				MapPos building_flag_pos = map->move_down_right(building->get_position());
				AILogLogger["util_build_best_road"] << name << " found completed one with flag pos " << building_flag_pos;
				// allow connecting to disconnected flag for optional_affinity, this is so far only used by rebuild_all_buildings
				//if (!game->get_flag_at_pos(first_building_flag_pos)->is_connected()) {
				//	AILogLogger["util_build_best_road"] << name << " affinity building flag is not connected!  skipping";
				//}
				//else {
					AILogLogger["util_build_best_road"] << name << " setting road_to target to affinity building_flag_pos " << building_flag_pos;
					targets.push_back(building_flag_pos);
					found = true;
					break;
				//}
			}
			if (!found) {
				AILogLogger["util_build_best_road"] << name << " could not find any completed building of specified optional_affinity type, returning false!";
				return false;
			}
		}
	}
	else if (optional_target != bad_map_pos) {
		// only for specific road improvements, target a specific flag or building pos rather than "connect to road system"
		AILogLogger["util_build_best_road"] << name << " using optional_target pos " << optional_target << " specified in build_best_road call";
		// check to if target is a building or a flag
		if (map->has_flag(optional_target)) {
			AILogLogger["util_build_best_road"] << name << " optional_target pos " << optional_target << " is a flag.  checking to see if it has an attached building";
			// this is only for debugging
			if (map->has_building(map->move_up_left(optional_target))) {
				AILogLogger["util_build_best_road"] << name << " flag at optional_target pos " << optional_target << " has an attached building of type " << NameBuilding[game->get_building_at_pos(map->move_up_left(optional_target))->get_type()];
			}
			AILogLogger["util_build_best_road"] << name << " setting optional_target flag pos " << optional_target << " as road_to target";
			targets.push_back(optional_target);
		}
		else if (map->has_building(optional_target)) {
			AILogLogger["util_build_best_road"] << name << " optional_target pos " << optional_target << " is a building of type " << NameBuilding[game->get_building_at_pos(optional_target)->get_type()];
			AILogLogger["util_build_best_road"] << name << " setting optional_target pos " << optional_target << " building's flag at pos " << map->move_down_right(optional_target) << " as road_to target";
			targets.push_back(map->move_up_left(optional_target));
		}
		else {
			AILogLogger["util_build_best_road"] << name << " optional_target pos has neither a building nor flag!  this is unexpected, returning false";
			return false;
		}
	}
	else {
		// most common - use the building affinity table to figure out what to connect to
		targets = AI::get_affinity(start_pos);
	}
	AILogLogger["util_build_best_road"] << name << " targets contains " << targets.size() << " affinity / target buildings";
	unsigned int roads_built = 0;
	unsigned int target_count = static_cast<unsigned int>(targets.size());
	//
	// change this to its own function so there isn't a foreach loop for hundreds of lines to handle two targets
	//
	for (MapPos target_pos : targets) {
		if (target_pos == bad_map_pos) {
			AILogLogger["util_build_best_road"] << name << " no more affinity targets left";
			break;
		}
		AILogLogger["util_build_best_road"] << name << " considering target_pos " << target_pos;
		AILogLogger["util_build_best_road"] << name << " plotting road to connect flag at pos " << start_pos << " to target_pos " << target_pos << " via road network";
		Roads split_roads;
		//
		// ##   Direct   ## roads are simple A->B connection from start_pos to target_pos of FIRST affinity building, no second roads
		//
		if (road_options.test(RoadOption::Direct)) {
			AILogLogger["util_build_best_road"] << name << " direct road requested, trying to build direct road from flag at " << start_pos << " to flag at " << target_pos;
			// Just build a single direct road
			//   RoadOption::Improve is ignored, any existing road will not be compared.  
			//split_roads list is not actually used for direct roads.  It is required/included but ignored

			Road proposed_direct_road = plot_road(map, player_index, start_pos, target_pos, &split_roads);
			AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->build_road (direct road)";
			game->get_mutex()->lock();
			AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->build_road (direct road)";
			bool was_built = game->build_road(proposed_direct_road, player);
			AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->build_road (direct road)";
			game->get_mutex()->unlock();
			AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->build_road (direct road)";
			if (was_built) {
				AILogLogger["util_build_best_road"] << name << " successfully built direct road directly from flag at " << start_pos << " to flag at " << target_pos;
				//roads_built++;
				return true;
			}
			else {
				// should have a series of fallback plans if a flag cannot be connected?   NO
				AILogLogger["util_build_best_road"] << name << " failed to connect specified flag to road system! returning false";
				return false;
			}
		}

		//
		// ## non-Direct ## roads terminate at best scoring acceptable end_pos flag, which could be a direct route to the target flag or join an existing road
		//
		AILogLogger["util_build_best_road"] << name << " non-direct road requested, trying to connect flag at pos " << start_pos << " to road network using scoring system";
		RoadBuilder rb;
		rb.set_start_pos(start_pos);
		rb.set_target_pos(target_pos);
		MapPosVector nearby_flags;

		// if RoadOption::Improve is set and any paths already exist from start_pos, trace these roads and save for later scoring
		//   NOTE - if road caching disabled, and Improve not set, eroads aren't even used by roadbuilding code, only proads which are directly scored
		//     as eroads are traced each time they are considered
		if (game->get_flag_at_pos(start_pos)->is_connected() && road_options.test(RoadOption::Improve)) {
			AILogLogger["util_build_best_road"] << name << " finding existing roads from start_pos " << start_pos << " for potential improvement";
			for (Direction dir : cycle_directions_cw()) {
				if (!map->has_path(start_pos, dir)) {
					continue;
				}
				AILogLogger["util_build_best_road"] << name << " found path from start_pos " << start_pos << " in dir " << NameDirection[dir];
				Road existing_road = trace_existing_road(map, start_pos, dir);
				RoadEnds ends = get_roadends(map, existing_road);
				rb.new_eroad(ends, existing_road);
				// add the other end flag to nearby_flags now to ensure it is scored
				MapPos end_pos = std::get<2>(ends);
				nearby_flags.push_back(end_pos);
			}
			AILogLogger["util_build_best_road"] << name << " done finding existing roads from start_pos " << start_pos;
		}

		// compile list of potential end flag MapPos to connect to (no Roads yet)
		//   only consider flags in a radius of the source, to avoid trying to connect to faraway flags
		// IMPROVEMENT - determine the halfway point between the start_pos and target_pos, and center the radius there instead of using start_pos
		//        potential bug, if the target_pos is really far away from the start_pos, it might not find anything... that seems unlikely though. 
		//         ...and that should happen even with the original method anyway so this isn't worse
		//          could consider a "tunnel" of multiple radiuses based on distance from start to target, but don't feel like coding that now
		AILogLogger["util_build_best_road"] << name << " finding halfway point between start_pos and target_pos to center nearby_flag search";
		MapPos halfway_pos = get_halfway_pos(start_pos, target_pos);
		// for rebuild_all_roads, and possibly other reasons, always push the target_pos onto the nearby_flags list
		AILogLogger["util_build_best_road"] << name << " as a fallback, adding target_pos " << target_pos << " to nearby_flags list";
		nearby_flags.push_back(target_pos);
		// because occasionally no flags are found within 6 tiles, will search up to 15 tiles
		//   BUT will quit any time after 6 tiles that at least one flag is found
		for (unsigned int i = 0; i < AI::spiral_dist(15); i++) {
			MapPos pos = map->pos_add_extended_spirally(halfway_pos, i);
			//ai_mark_pos.insert(std::make_pair(pos, "dk_brown"));
			//std::this_thread::sleep_for(std::chrono::milliseconds(5));
			// skip if no flag, or pos not owned by this player
			if (!map->has_flag(pos) || map->get_owner(pos) != player_index) {
				continue;
			}
			if (start_pos == pos) {
				AILogLogger["util_build_best_road"] << name << " flag at pos " << pos << " is the same as start flag, skipping";
				continue;
			}
			// avoid disconnected flags, unless they are the flag of the target building
			// NOTE that this isn't smart enough to ignore flags that have some paths but are overall disconnected from the road system!
			//  this is a real bug that will need fixing otherwise disconnected roads will never be rejoined to road network
			/*
			if (!game->get_flag_at_pos(pos)->is_connected() && pos != castle_flag_pos && pos != target_pos) {
				AILogLogger["util_build_best_road"] << name << " flag at pos " << pos << " is itself disconnected and is neither the castle_flag_pos nor the target_pos, skipping";
				continue;
			}*/
			// removing "allow target_pos" exception, it causes problems and is only used for rebuild_all_roads which doesn't work well
			if (!game->get_flag_at_pos(pos)->is_connected() && pos != castle_flag_pos) {
				AILogLogger["util_build_best_road"] << name << " flag at pos " << pos << " is itself disconnected and is not the castle_flag_pos, skipping";
				continue;
			}

			AILogLogger["util_build_best_road"] << name << " found valid nearby_flag at pos " << pos;
			if (std::find(nearby_flags.begin(), nearby_flags.end(), pos) != nearby_flags.end()) {
				// skip if already in nearby_flags list, which should only happen if it is part of existing road and RoadOption::Improve roads set;
				AILogLogger["util_build_best_road"] << name << " this pos is already in nearby_flag list, skipping";
				continue;
			}
			nearby_flags.push_back(pos);
			//ai_mark_pos.erase(pos);
			//ai_mark_pos.insert(std::make_pair(pos, "orange"));
			//std::this_thread::sleep_for(std::chrono::milliseconds(10));

			// quit search after spiral_dist(6) tiles if at least one flag found
			//  this allows reaching a faraway flag, but avoids finding too many flags
			if (i >= AI::spiral_dist(6)) {
				// nearby_flags includes the target_pos flag so it is always at least 1, check for 2+
				if (nearby_flags.size() >= 2) {
					AILogLogger["util_build_best_road"] << name << " at least one flag found within 6 tiles of halfway_pos, quitting search";
					break;
				}
				else {
					AILogLogger["util_build_best_road"] << name << " no nearby flag found within 6 tiles of halfway_pos!  continuing expanded search until one found";
				}
			}
		}
		ai_mark_pos.clear();

		AILogLogger["util_build_best_road"] << name << " found " << nearby_flags.size() << " nearby_flags";
		if (nearby_flags.size() < 1) {
			AILogLogger["util_build_best_road"] << name << " no nearby_flags found!  maybe castle_flag should always be added as a last resort...";
			AILogLogger["util_build_best_road"] << name << " because no nearby_flags found, no roads can be between start_pos " << start_pos << " and target_pos " << target_pos;
			// it seems likely that if the first target can't be reached, any second affinity target can't either
			//  likely this flag can't be connected to the road system at all now.  Just quit here
			return false;
		}

		//
		// plot Roads to the found nearby flags ends and score them in terms of NEW LENGTH
		//    store any that are "good enough" (under the max_convolution limit)
		//      quit plotting roads once max_plotting_steps_per_target limit is reached,     NOT IMPLEMENTED YET
		//		 which is the aggregate tile_dist of ALL plotted roads so far to this target  NOT IMPLEMENTED YET
		//        this is used to limit the amount of time spent plotting roads as realm becomes crowded    NOT IMPLEMENTED YET
		//			
		// use straight-line-distance to the actual target_pos as the measuring stick to judge how convoluted actual roads are be in comparison
		AILogLogger["util_build_best_road"] << name << " checking tile_dist from start_pos " << start_pos << " to end_pos " << target_pos << " for an ideal road";
		int ideal_length = get_straightline_tile_dist(map, start_pos, target_pos);
		AILogLogger["util_build_best_road"] << name << " flag target_pos at " << target_pos << " has straight-line tile distance " << ideal_length << " from start_pos " << start_pos;
		// for each nearby_flag:
		//   - use FlagNodeSearch to find the shortest flag-path from each nearby_flag pos to the target_pos/flag, apply any penalties
		//   - add new_length score + existing road score, store complete solutions
		//   - quit early if max_road_solutions reached ?
		//   - plot and score any additional potential_road solutions made possible by creating 
		//      flags & splitting existing roads, unless RoadOption::SplitRoads if set to false
		//   - sort the results by 'adjusted_score' ("adjusted length", or "overall length including existing, new length, and penalties"
		//   - build the best road (if better than best existing road)
		for (MapPos end_pos : nearby_flags) {
			AILogLogger["util_build_best_road"] << name << " preparing to plot road from start_pos " << start_pos << " to nearby_flag pos " << end_pos;
			// it might be faster to have plot_road return the RoadEnds, but it seems messier
			Road potential_road = plot_road(map, player_index, start_pos, end_pos, &split_roads);
			if (potential_road.get_length() == 0) {
				AILogLogger["util_build_best_road"] << name << " unable to plot_road from start_pos " << start_pos << " to nearby_flag pos " << end_pos << ", skipping";
				continue;
			}
			AILogLogger["util_build_best_road"] << name << " potential road from start_pos " << start_pos << " to nearby_flag pos " << end_pos << " has new segment length " << potential_road.get_length();
			double convolution = double(potential_road.get_length()) / double(ideal_length);
			AILogLogger["util_build_best_road"] << name << " potential road NEW length: " << potential_road.get_length() << ", ideal TOTAL length: " << ideal_length << ", convolution ratio: " << convolution;
			if (convolution >= max_convolution) {
				AILogLogger["util_build_best_road"] << name << " this potential road solution is already too convoluted from new length alone, max is " << max_convolution;
				continue;
			}
			AILogLogger["util_build_best_road"] << name << " this potential road solution is acceptable so far in terms of NEW length only, adding to RoadBuilder potential_roads";
			RoadEnds potential_road_ends = get_roadends(map, potential_road);
			rb.new_proad(potential_road_ends, potential_road);
			AILogLogger["util_build_best_road"] << name << " there are currently " << rb.get_proads().size() << " potential_roads in the list";
			//
			// now do the same thing for any potential_roads found (flag-splitting), could make this a recursive function call instead...
			// 
			if (road_options.test(RoadOption::SplitRoads == false)) {
				continue;
			}
			AILogLogger["util_build_best_road"] << name << " RoadOption SplitRoads is true, preparing to iterate over potential split roads found in previous plot_road call";
			AILogLogger["util_build_best_road"] << name << " there are " << split_roads.size() << " Road entries in the split_roads list (that was provide by the last plot_roads call to nearby map pos " << end_pos;
			for (Road split_road : split_roads) {
				MapPos split_end_pos = split_road.get_end(map.get());
				AILogLogger["util_build_best_road"] << name << " a potential split_road has end_pos: " << split_road.get_end(map.get()) << ", for comparison, start_pos is " << start_pos << " and split_road source is " << split_road.get_source();
				if (split_road.get_length() == 0) {
					AILogLogger["util_build_best_road"] << name << " road unexpectedly has zero length!  this should not happen, find out why!  skipping";
					continue;
				}
				AILogLogger["util_build_best_road"] << name << " split_road from start_pos " << start_pos << " to new-flag pos " << split_end_pos << " (on way to nearby_flag pos " << end_pos << ") has new segment length " << split_road.get_length();
				double convolution = double(split_road.get_length()) / double(ideal_length);
				AILogLogger["util_build_best_road"] << name << " split_road NEW length: " << split_road.get_length() << ", ideal TOTAL length: " << ideal_length << ", convolution ratio: " << convolution;
				if (convolution >= max_convolution) {
					AILogLogger["util_build_best_road"] << name << " this split_road solution is already too convoluted from new length alone, max is " << max_convolution;
					continue;
				}
				AILogLogger["util_build_best_road"] << name << " this split_road road solution is acceptable so far in terms of NEW length only, adding to RoadBuilder potential_roads";
				RoadEnds split_road_ends = get_roadends(map, split_road);
				rb.new_proad(split_road_ends, split_road);
				AILogLogger["util_build_best_road"] << name << " there are currently " << rb.get_proads().size() << " potential_roads in the list";
			}

		}
		//=====================================================================================
		// score the potential new roads to nearby_flags in terms of:
		//     tile_dist (from end_flag to target flag)
		//     flag_dist (from end_flag to target flag);
		//     new_length (of the potential road to be built from start_pos to nearby_flag pos
		AILogLogger["util_build_best_road"] << name << " preparing to score rb.get_proads() list";

		//
		// FIRST, score any existing roads(eroad) already attached to start_pos for later comparison to potential new
		//   These should only be here if RoadOption::Improve was set earlier in upstream functions
		//     
		//MapPosSet scored_eroads;
		MapPos best_eroad_pos = bad_map_pos;
		unsigned int best_eroad_score = bad_score;
		if (game->get_flag_at_pos(start_pos)->is_connected() && road_options.test(RoadOption::Improve)) {
			AILogLogger["util_build_best_road"] << name << " checking eroads because start_pos already has paths and RoadOption::Improve is set";
			for (std::pair<RoadEnds, RoadBuilderRoad*> er : rb.get_eroads()) {
				RoadEnds ends = er.first;
				MapPos start_pos = er.second->get_end1();
				MapPos start_dir = er.second->get_dir1();
				MapPos nearby_eroad_flag_pos = er.second->get_end2();
				AILogLogger["util_build_best_road"] << name << " found an existing road from start_pos " << start_pos << " in dir " << NameDirection[start_dir] << " to nearby_flag_pos " << nearby_eroad_flag_pos << ", checking score to target_pos " << target_pos;
				AILogLogger["util_build_best_road"] << name << " debug - checking score for nearby_eroad_flag_pos " << nearby_eroad_flag_pos << " to target_pos " << target_pos << " BEFORE find_flag_and_tile_dist is called to see if there will be a collision";
				AILogLogger["util_build_best_road"] << name << " debug - BEFORE score: flag_dist " << rb.get_score(nearby_eroad_flag_pos).get_flag_dist() << ", tile dist " << rb.get_score(nearby_eroad_flag_pos).get_tile_dist();
				if (!find_flag_and_tile_dist(map, player_index, &rb, nearby_eroad_flag_pos, target_pos, &ai_mark_pos)) {
					AILogLogger["util_build_best_road"] << name << " eroad: find_flag_and_tile_dist returned false for nearby_eroad_flag_pos " << nearby_eroad_flag_pos << "!";
					continue;
				}
				AILogLogger["util_build_best_road"] << name << " eroad: find_flag_and_tile_dist returned true for nearby_flag_pos " << nearby_eroad_flag_pos;

				AILogLogger["util_build_best_road"] << name << " debug - checking score for nearby_eroad_flag_pos " << nearby_eroad_flag_pos << " to target_pos " << target_pos << " AFTER find_flag_and_tile_dist is called to see if there was a collision";
				AILogLogger["util_build_best_road"] << name << " debug - AFTER score: flag_dist " << rb.get_score(nearby_eroad_flag_pos).get_flag_dist() << ", tile dist " << rb.get_score(nearby_eroad_flag_pos).get_tile_dist();

				AILogLogger["util_build_best_road"] << name << " preparing to apply penalties to eroad from start_pos " << start_pos << " in dir " << NameDirection[start_dir] << " to nearby_eroad_flag_pos " << nearby_eroad_flag_pos;
				FlagScore nearby_eroad_flag_score = rb.get_score(nearby_eroad_flag_pos);
				unsigned int tile_dist = nearby_eroad_flag_score.get_tile_dist();
				unsigned int flag_dist = nearby_eroad_flag_score.get_flag_dist();
				bool contains_castle_flag = nearby_eroad_flag_score.get_contains_castle_flag();
				// no new length for an existing road, this means it will be preferred over a similiar
				//   potential new road after new_length penalty applied.  A new road must be significantly better to be built
				unsigned int new_length = 0;
				AILogLogger["util_build_best_road"] << name << " BEFORE applying any penalties, potential road from start_pos " << start_pos << " in dir " << NameDirection[start_dir] << " to nearby_eroad_flag_pos " << nearby_eroad_flag_pos <<
					" has tile_dist " << tile_dist << ", flag_dist " << flag_dist << ", new_length " << new_length << " to target_pos " << target_pos;
				//----------------------------------------------------------------------------------------------
				// RoadOption penalties applied here!   * 1 means no penalty, only in place for easy adjustment
				//----------------------------------------------------------------------------------------------
				unsigned int adjusted_score = static_cast<unsigned int>(double(tile_dist * 1) + double(flag_dist * 1));
				//----------------------------------------------------------------------------------------------
				if (road_options.test(RoadOption::PenalizeCastleFlag) && contains_castle_flag == true) {
					adjusted_score += contains_castle_flag_penalty;
					AILogLogger["pathfinder"] << "applying contains_castle_flag penalty of " << contains_castle_flag_penalty;
				}
				AILogLogger["util_build_best_road"] << name << " AFTER penalties, existing road from start_pos " << start_pos << " in dir " << NameDirection[start_dir] << " to nearby_eroad_flag_pos " << nearby_eroad_flag_pos <<
					" has tile_dist " << tile_dist << ", flag_dist " << flag_dist << ", new_length " << new_length << ", adjusted_score " << adjusted_score << " to target_pos " << target_pos;
				if (adjusted_score < best_eroad_score) {
					AILogLogger["util_build_best_road"] << name << " new best_eroad_score found: " << adjusted_score << " from pos " << nearby_eroad_flag_pos;
					best_eroad_score = adjusted_score;
					best_eroad_pos = nearby_eroad_flag_pos;
				}
			}
			AILogLogger["util_build_best_road"] << name << " done finding eroads, best_eroad_score found: " << best_eroad_score << " from pos " << best_eroad_pos;
		}
		else {
			AILogLogger["util_build_best_road"] << name << " start_pos " << start_pos << " has no paths, or RoadOption::Improve not set, not checking for eroads";
		} // if start_pos has any paths/eroads already

		// foreach potential new road (proad) to be built to a nearby flag
		//	score the existing road (eroad) that begins at the flag/pos where the proad ends
		AILogLogger["util_build_best_road"] << name << " preparing to score potential new roads from start_pos to nearby_flag positions";
		MapPosSet scored_proads;
		for (std::pair<RoadEnds, RoadBuilderRoad*> pr : rb.get_proads()) {
			RoadEnds ends = pr.first;
			RoadBuilderRoad *rbroad = pr.second;
			MapPos start_pos = pr.second->get_end1();
			MapPos nearby_flag_pos = pr.second->get_end2();
			AILogLogger["util_build_best_road"] << name << " found an potential new road to nearby_flag_pos " << nearby_flag_pos;
			// why is this scoring route to castle_flag pos?  shouldn't it be to target_pos for affinity building??
			//if (!score_flag(map, player_index, &rb, road_options, nearby_flag_pos, castle_flag_pos, &ai_mark_pos)) {
			// trying this instead - may04 2020
			//  WAIT WAIT WAIT, i think castle_flag_pos is only used to pass the location of castle flag so the contains_castle_flag bool can be set!
			//     it isn't actually using it as the target!  setting it back
			//if (!score_flag(map, player_index, &rb, road_options, nearby_flag_pos, target_pos, &ai_mark_pos)) {
			if (!score_flag(map, player_index, &rb, road_options, nearby_flag_pos, castle_flag_pos, &ai_mark_pos)) {
				AILogLogger["util_build_best_road"] << name << " proad: score_flag returned false for nearby_flag_pos " << nearby_flag_pos << "!";
				continue;
			}
			AILogLogger["util_build_best_road"] << name << " proad: score_flag returned true for nearby_flag_pos " << nearby_flag_pos;
			// then add the score of the new segment and apply any penalties 
			//    and insert adjusted_scores into MapPosSet for sorting later
			AILogLogger["util_build_best_road"] << name << " preparing to apply penalties to proad from start_pos " << start_pos << " to nearby_flag_pos " << nearby_flag_pos;
			FlagScore nearby_flag_score = rb.get_score(nearby_flag_pos);
			unsigned int tile_dist = nearby_flag_score.get_tile_dist();
			unsigned int flag_dist = nearby_flag_score.get_flag_dist();
			bool contains_castle_flag = nearby_flag_score.get_contains_castle_flag();
			unsigned int new_length = static_cast<unsigned int>(rbroad->get_road().get_length());
			AILogLogger["util_build_best_road"] << name << " BEFORE applying any penalties, potential road from start_pos " << start_pos << " to nearby_flag_pos " << nearby_flag_pos <<
				" has tile_dist " << tile_dist << ", flag_dist " << flag_dist << ", new_length " << new_length;
			//----------------------------------------------------------------------------------------------
			// RoadOption penalties applied here!   * 1 means no penalty, only in place for easy adjustment
			//----------------------------------------------------------------------------------------------
			// apr13 2020 increased new_length penalty from 2.0 to 2.5
			double new_length_penalty;
			if (road_options.test(RoadOption::PenalizeNewLength)) {
				new_length_penalty = 2.5;
			}
			else {
				new_length_penalty = 1.0; // no penalty
			}
			if (road_options.test(RoadOption::ReducedNewLengthPenalty)) {
				new_length_penalty = 1.75;
			}
			unsigned int adjusted_score = static_cast<unsigned int>(double(tile_dist * 1) + double(new_length) * new_length_penalty + double(flag_dist * 1));
			if (road_options.test(RoadOption::PenalizeCastleFlag) && contains_castle_flag == true) {
				adjusted_score += contains_castle_flag_penalty;
				AILogLogger["util_build_best_road"] << "applying contains_castle_flag penalty of " << contains_castle_flag_penalty;
			}
			AILogLogger["util_build_best_road"] << name << " AFTER penalties, potential road from start_pos " << start_pos << " to nearby_flag_pos " << nearby_flag_pos <<
				" has tile_dist " << tile_dist << ", flag_dist " << flag_dist << ", new_length " << new_length << ", adjusted_score " << adjusted_score;
			scored_proads.insert(std::make_pair(nearby_flag_pos, adjusted_score));
		}
		AILogLogger["util_build_best_road"] << name << " done scoring proads";


		// sort the potential new roads by their score, best (lowest) score first
		AILogLogger["util_build_best_road"] << name << " preparing to sort scored_proads into a MapPosVector";
		MapPosVector sorted_scored_proads = sort_by_val_asc(scored_proads);

		// try to build a road, starting with the best scoring potential_road end flag, until successful
		AILogLogger["util_build_best_road"] << name << " preparing to iterate over sorted_scored_proads to try to build roads to best nearby_flag";
		for (MapPos end_pos : sorted_scored_proads) {
			AILogLogger["util_build_best_road"] << name << " considering building road from start_pos " << start_pos << " to end_pos " << end_pos;
			if (roads_built >= target_count) {
				AILogLogger["util_build_best_road"] << name << " roads_built " << roads_built << " >= target_count " << target_count << ", done building roads";
				return true;
			}
			RoadBuilderRoad *proad = rb.get_proad(end_pos);

			if (game->get_flag_at_pos(start_pos)->is_connected() && road_options.test(RoadOption::Improve)) {
				// check and see if this solution is actually better than the existing best complete
				//   solution from start_pos to target_pos (eroad score)
				bool skip_target = false;
				for (std::pair<MapPos, unsigned int> ppair : scored_proads) {
					if (ppair.first != end_pos) {
						continue;
					}
					AILogLogger["util_build_best_road"] << name << " found adjusted_score of " << ppair.second << " for this proad ending at end_pos " << end_pos;
					// it must be SIGNIFICANTLY better, say 50% + 2?
					double modified_prod_score = double(ppair.second) * double(1.50) + 2;
					//if (best_eroad_score <= ppair.second) {
					if (best_eroad_score <= modified_prod_score) {
						AILogLogger["util_build_best_road"] << name << " proad from start_pos " << start_pos << " to end_pos " << end_pos << " has +50%+2 score " << modified_prod_score << " which is not significantly better than best_eroad_score " << best_eroad_score << ", skipping this target";
						skip_target = true;
						break;
					}
					else {
						AILogLogger["util_build_best_road"] << name << " proad from start_pos " << start_pos << " to end_pos " << end_pos << " has +50%+2 score " << modified_prod_score << " which is significantly better than best_eroad_score " << best_eroad_score << ", will build new road";
						break;
					}
				}
				if (skip_target == true) {
					AILogLogger["util_build_best_road"] << name << " skip_target is true, breaking from foreach sorted_scored_proads - not building road to this target";
					break;
				}
			}
			else {
				AILogLogger["util_build_best_road"] << name << " start_pos " << start_pos << " has no paths, or RoadOption::Improve not set, not comparing this solution to any best_eroad_score";
			} // if start_pos has any paths/eroads already

			Road road = proad->get_road();
			AILogLogger["util_build_best_road"] << name << " trying to build road from " << start_pos << " to " << end_pos << " as specified in proad";
			bool created_new_flag = false;
			if (game->get_flag_at_pos(end_pos) == nullptr) {
				AILogLogger["util_build_best_road"] << name << " end_pos " << end_pos << " has no flag, must be fake flag/split road, trying to create a real flag";
				AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->build_flag (split road)";
				game->get_mutex()->lock();
				AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->build_flag (split road)";
				bool was_built = game->build_flag(end_pos, player);
				AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->build_flag (split road)";
				game->get_mutex()->unlock();
				AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->build_flag (split road)";
				if (was_built) {
					AILogLogger["util_build_best_road"] << name << " successfully built new flag at end_pos " << end_pos << ", splitting the road";
					created_new_flag = true;
				}
				else {
					AILogLogger["util_build_best_road"] << name << " failed to build flag at end_pos " << end_pos << ", FIND OUT WHY!  not trying to build this road.  marking pos in blue";
					ai_mark_pos.insert(ColorDot(end_pos, "blue"));
					std::this_thread::sleep_for(std::chrono::milliseconds(5000));
					continue;
				}
			}
			//
			// insert code here to check if the second road is actually better than the first road built (if one was built) !!
			//   otherwise, don't build.  Adding this because often a stubby second road is built when the first road is already optimal
			//      to connect to the second affinity building
			//
			if (true) {
				if (roads_built >= 1) {
					AILogLogger["util_build_best_road"] << name << " TODO - check to see if this road is better than the first one built!";
				}
				AILogLogger["util_build_best_road"] << name << " about to build road, dumping some road stats.  source=" << road.get_source() << ", end=" << road.get_end(game->get_map().get());
				AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->build_road (non-direct road)";
				game->get_mutex()->lock();
				AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->build_road (non-direct road)";
				bool was_built = game->build_road(road, game->get_player(player_index));
				AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->build_road (non-direct road)";
				game->get_mutex()->unlock();
				AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->build_road (non-direct road)";
				if (was_built) {
					AILogLogger["util_build_best_road"] << name << " successfully built road from " << start_pos << " to " << end_pos << " as specified in PotentialRoad";
					roads_built++;
					continue;
				}
				else {
					AILogLogger["util_build_best_road"] << name << " ERROR - failed to build road from " << start_pos << " to " << end_pos << ", FIND OUT WHY!   trying next best solution...";
				}
			}
			else {
				AILogLogger["util_build_best_road"] << name << " last road built is better than this proposed one, not building second road (should only happy for dual-affinity buildings)";
			}
			if (created_new_flag) {
				AILogLogger["util_build_best_road"] << name << " removing the newly created flag at end_pos so it doesn't screw up the rest of the road solutions";
				AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->demolish_flag (couldn't connect new flag)";
				game->get_mutex()->lock();
				AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->demolish_flag (couldn't connect new flag)";
				game->demolish_flag(end_pos, game->get_player(player_index));
				AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->demolish_flag (couldn't connect new flag)";
				game->get_mutex()->unlock();
				AILogLogger["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->demolish_flag (couldn't connect new flag)";
			}
			AILogLogger["util_build_best_road"] << name << " done trying to build road from " << start_pos << " to " << end_pos;
		} // end foreach end_pos
		AILogLogger["util_build_best_road"] << name << " done building roads to target_pos " << target_pos;
	} //foreach target
	// return success if at least one road built, even if two targets found

	duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;
	AILogLogger["util_build_best_road"] << name << " build_best_road call took " << duration;

	if (roads_built < 1) {
		AILogLogger["util_build_best_road"] << name << " build_best_road: Failed to connect specified flag to road system!";
		return false;
	}
	AILogLogger["util_build_best_road"] << name << " Done with build_best_road, roads built: " << roads_built << ", out of target_count: " << target_count;
	return true;
}

MapPosVector
// return a vector of MapPos of affinity building[s], or a default if none defined in BuildingAffinity table (castle)
AI::get_affinity(MapPos flag_pos) {
	// time this function for debugging
	std::clock_t start;
	double duration;
	start = std::clock();
	AILogLogger["util_get_affinity"] << name << " inside AI::get_affinity for flag pos " << flag_pos;
	// find castle pos  (maybe only if castle is the target found? - break this out to separate function)
	MapPosVector affinity;
	// no affinity buildings - use castle/stock
	MapPos nearest_stock = find_nearest_stock(flag_pos);
	if (!game->get_flag_at_pos(flag_pos)->has_building()) {
		AILogLogger["util_get_affinity"] << name << " flag has no attached building, setting destination to nearest_stock " << nearest_stock;
		affinity.push_back(nearest_stock);
		return affinity;
	}
	//AILogLogger["util_get_affinity"] << name << " flag at flag_pos " << flag_pos << " is attached to a building";
	Building::Type building_type = game->get_flag_at_pos(flag_pos)->get_building()->get_type();
	AILogLogger["util_get_affinity"] << name << " flag at flag_pos " << flag_pos << " is attached to a building of type " << NameBuilding[building_type];
	if (BuildingAffinity[building_type][0] == Building::TypeNone && BuildingAffinity[building_type][1] == Building::TypeNone) {
		AILogLogger["util_get_affinity"] << name << " building type " << NameBuilding[building_type] << " has no BuildingAffinity, connect to nearest_stock (" << nearest_stock << ")";
		affinity.push_back(nearest_stock);
	}
	// single affinity building
	else if (BuildingAffinity[building_type][0] != Building::TypeNone && BuildingAffinity[building_type][1] == Building::TypeNone) {
		Building::Type single_affinity = BuildingAffinity[building_type][0];
		AILogLogger["util_get_affinity"] << name << " building type " << NameBuilding[building_type] << " has single BuildingAffinity, with " << NameBuilding[single_affinity];
		update_building_counts();
		if (completed_building_count[single_affinity] < 1) {
			AILogLogger["util_get_affinity"] << name << " player has no completed buildings of single affinity type " << NameBuilding[single_affinity] << ", setting road_to target to nearest_stock " << nearest_stock;
			affinity.push_back(nearest_stock);
		}
		else {
			AILogLogger["util_get_affinity"] << name << " looking for nearest building of type " << NameBuilding[single_affinity];
			Building *building = AI::find_nearest_building(flag_pos, AI::spiral_dist(9), single_affinity);
			if (building != nullptr) {
				AILogLogger["util_get_affinity"] << name << " found affinity building at pos " << building->get_position();
				// get the flag position of the building
				MapPos building_flag_pos = game->get_flag(building->get_flag_index())->get_position();
				AILogLogger["util_get_affinity"] << name << " setting road_to target to building_flag_pos " << building_flag_pos;
				affinity.push_back(building_flag_pos);
			}
			else {
				AILogLogger["util_get_affinity"] << name << " couldn't find any affinity building";
				AILogLogger["util_get_affinity"] << name << " setting road_to target to nearest_stock " << nearest_stock;
				affinity.push_back(nearest_stock);
			}
		}
	}
	//  two affinity buildings
	else if (BuildingAffinity[building_type][0] != Building::TypeNone && BuildingAffinity[building_type][1] != Building::TypeNone) {
		Building::Type first_affinity = BuildingAffinity[building_type][0];
		Building::Type second_affinity = BuildingAffinity[building_type][1];
		AILogLogger["util_get_affinity"] << name << " building type " << NameBuilding[building_type] << " has dual BuildingAffinity, with " << NameBuilding[first_affinity] << " and " << NameBuilding[second_affinity];
		int notfound = 0;
		update_building_counts();
		if (completed_building_count[first_affinity] < 1) {
			AILogLogger["util_get_affinity"] << name << " player has no completed buildings of first affinity type " << NameBuilding[first_affinity] << ", skipping";
			notfound++;
		}
		else {
			AILogLogger["util_get_affinity"] << name << " looking for nearest building of first type " << NameBuilding[first_affinity];
			Building *first_building = AI::find_nearest_building(flag_pos, AI::spiral_dist(9), first_affinity);
			if (first_building != nullptr) {
				AILogLogger["util_get_affinity"] << name << " found first_affinity building at pos " << first_building->get_position();
				// get the flag position of the building
				MapPos first_building_flag_pos = game->get_flag(first_building->get_flag_index())->get_position();
				// got a read access violation here when checking flag->has_path (inside flag->is_connected) - how?  how could flag have disappeared since the previous call??
				//    especially when inside mutex lock
				// maybe stop changing the call path used to do these checks and make it consistent?  like, change below to game->get_flag(first_building->get_flag_index())->is_connected()?
				// allow disconnected buildings to make rebuild_all_roads work, instead we are now checking that they are COMPLETED
				//if (!game->get_flag_at_pos(first_building_flag_pos)->is_connected()) {
				//	AILogLogger["util_get_affinity"] << name << " affinity building flag is not connected!  skipping";
				//}
				//else {
					AILogLogger["util_get_affinity"] << name << " setting road_to target to affinity building_flag_pos " << first_building_flag_pos;
					affinity.push_back(first_building_flag_pos);
				//}
			}
			else {
				AILogLogger["util_get_affinity"] << name << " couldn't find any first_affinity building nearby, checking entire realm";
				bool found = false;
				AILogLogger["util_get_affinity"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for get_affinity)";
				game->get_mutex()->lock();
				AILogLogger["util_get_affinity"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for get_affinity)";
				Game::ListBuildings buildings = game->get_player_buildings(player);
				AILogLogger["util_get_affinity"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player) (for get_affinity)";
				game->get_mutex()->unlock();
				AILogLogger["util_get_affinity"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player) (for get_affinity)";
				for (Building *building : buildings) {
					if (building->get_type() == first_affinity) {
						if (building->is_burning()) {
							AILogLogger["util_get_affinity"] << name << " this building is on fire!  skipping";
							continue;
						}
						MapPos first_building_flag_pos = map->move_down_right(building->get_position());
						AILogLogger["util_get_affinity"] << name << " found one with flag pos " << first_building_flag_pos;
						if (!game->get_flag_at_pos(first_building_flag_pos)->is_connected()) {
							AILogLogger["util_get_affinity"] << name << " affinity building flag is not connected!  skipping";
						}
						else {
							AILogLogger["util_get_affinity"] << name << " setting road_to target to affinity building_flag_pos " << first_building_flag_pos;
							affinity.push_back(first_building_flag_pos);
							found = true;
							break;
						}
					}
				}
				if (!found) {
					notfound++;
				}
			}
		}
		if (completed_building_count[second_affinity] < 1) {
			AILogLogger["util_get_affinity"] << name << " player has no completed buildings of second affinity type " << NameBuilding[second_affinity] << ", skipping";
			notfound++;
		}
		// this should become a function called twice instead of copy/paste
		else {
			AILogLogger["util_get_affinity"] << name << " looking for nearest building of second type " << NameBuilding[second_affinity];
			Building *second_building = AI::find_nearest_building(flag_pos, AI::spiral_dist(9), second_affinity);
			if (second_building != nullptr) {
				AILogLogger["util_get_affinity"] << name << " found second_affinity building at pos " << second_building->get_position();
				// get the flag position of the building
				MapPos second_building_flag_pos = game->get_flag(second_building->get_flag_index())->get_position();
				// allow disconnected buildings to make rebuild_all_roads work, instead we are now checking that they are COMPLETED
				//if (!game->get_flag_at_pos(second_building_flag_pos)->is_connected()) {
				//	AILogLogger["util_get_affinity"] << name << " affinity building flag is not connected!  skipping";
				//}
				//else {
					AILogLogger["util_get_affinity"] << name << " setting road_to target to affinity building_flag_pos " << second_building_flag_pos;
					affinity.push_back(second_building_flag_pos);
				//}
			}
			else {
				AILogLogger["util_get_affinity"] << name << " couldn't find any second_affinity building nearby, checking entire realm";
				bool found = false;
				AILogLogger["util_get_affinity"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for get_affinity)";
				game->get_mutex()->lock();
				AILogLogger["util_get_affinity"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for get_affinity)";
				Game::ListBuildings buildings = game->get_player_buildings(player);
				AILogLogger["util_get_affinity"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player) (for get_affinity)";
				game->get_mutex()->unlock();
				AILogLogger["util_get_affinity"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player) (for get_affinity)";
				for (Building *building : buildings) {
					if (building->get_type() == second_affinity) {
						if (building->is_burning()) {
							AILogLogger["util_get_affinity"] << name << " this building is on fire!  skipping";
							continue;
						}
						MapPos second_building_flag_pos = map->move_down_right(building->get_position());
						AILogLogger["util_get_affinity"] << name << " found one at pos " << second_building_flag_pos;
						if (!game->get_flag_at_pos(second_building_flag_pos)->is_connected()) {
							AILogLogger["util_get_affinity"] << name << " affinity building flag is not connected!  skipping";
						}
						else {
							AILogLogger["util_get_affinity"] << name << " setting road_to target to affinity building_flag_pos " << second_building_flag_pos;
							affinity.push_back(second_building_flag_pos);
							found = true;
							break;
						}
					}
				}
				if (!found) {
					notfound++;
				}
			}
		}
		if (notfound >= 2) {
			AILogLogger["util_get_affinity"] << name << " couldn't find any of either affinity building";
			AILogLogger["util_get_affinity"] << name << " setting road_to target to nearest_stock " << nearest_stock;
			affinity.push_back(nearest_stock);
		}
	}
	AILogLogger["util_get_affinity"] << name << " returning " << affinity.size() << " affinity building targets";
	return affinity;
	duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;
	AILogLogger["util_get_affinity"] << name << " done util_get_affinity call took " << duration;
}


// return Building* for first building of the specified type exists within specified distance from pos
Building*
AI::find_nearest_building(MapPos center_pos, unsigned int distance, Building::Type building_type) {
	AILogLogger["util_find_nearest_building"] << name << " inside find_nearest_building " << NameBuilding[building_type] << ", distance " << distance << ", and target pos " << center_pos;
	for (unsigned int i = 0; i < distance; i++) {
		MapPos pos = map->pos_add_extended_spirally(center_pos, i);
		if (!map->has_building(pos))
			continue;
		if (game->get_building_at_pos(pos)->get_type() == building_type) {
			AILogLogger["util_find_nearest_building"] << name << " found a building of type " << NameBuilding[building_type] << " at pos " << pos;
			return game->get_building_at_pos(pos);
		}
	}

	AILogLogger["util_find_nearest_building"] << name << " no nearby building found of type " << NameBuilding[building_type] << ", returning nullptr";
	return nullptr;
}

// return Building* for first building of the specified type exists within specified distance from pos
//   THAT IS FULLY BUILT
Building*
AI::find_nearest_completed_building(MapPos center_pos, unsigned int distance, Building::Type building_type) {
	AILogLogger["util_find_nearest_completed_building"] << name << " inside find_nearest_completed_building " << NameBuilding[building_type] << ", distance " << distance << ", and target pos " << center_pos;
	for (unsigned int i = 0; i < distance; i++) {
		MapPos pos = map->pos_add_extended_spirally(center_pos, i);
		if (!map->has_building(pos))
			continue;
		if (game->get_building_at_pos(pos)->get_type() != building_type)
			continue;
		if (!game->get_building_at_pos(pos)->is_done())
			continue;
		AILogLogger["util_find_nearest_completed_building"] << name << " found a completed building of type " << NameBuilding[building_type] << " at pos " << pos;
		return game->get_building_at_pos(pos);
	}

	AILogLogger["util_find_nearest_completed_building"] << name << " no nearby completed building found of type " << NameBuilding[building_type] << ", returning nullptr";
	return nullptr;
}

// return true if any building of the specified type exists within specified distance from pos
//  only considers this player's buildings
bool
AI::building_exists_near_pos(MapPos center_pos, unsigned int distance, Building::Type building_type) {
	AILogLogger["util_building_exists_near_pos"] << name << " inside AI::building_exists_near_pos with type " << NameBuilding[building_type] << ", distance " << distance << ", and target pos " << center_pos;
	for (unsigned int i = 0; i < distance; i++) {
		//AILogLogger["util_building_exists_near_pos"] << name << " inside AI::building_exists_near_pos debug2, i=" << i;
		MapPos pos = map->pos_add_extended_spirally(center_pos, i);
		//AILogLogger["util_building_exists_near_pos"] << name << " inside AI::building_exists_near_pos debug3";
		if (!map->has_building(pos) || map->get_owner(pos) != player_index)
			continue;
		if (game->get_building_at_pos(pos)->get_type() == building_type) {
			AILogLogger["util_building_exists_near_pos"] << name << " found a building of type " << NameBuilding[building_type] << " at pos " << pos;
			return true;
		}
	}

	AILogLogger["util_building_exists_near_pos"] << name << " no building found of type " << NameBuilding[building_type] << " near center_pos " << center_pos << ", returning false";
	return false;
}


// find the "best" of two building types, in preference order: occupied->completed->any
//   and find the halfway point between those two and return it
// for trying to build between two buildings, such as building a SteelSmelter halfway between CoalMine and IronMine
//   currently this is ONLY used for placing a steelsmelter between coal and iron mines
//  since enabling multiple economies, this function only considers buildings attached to the current stock_pos
MapPos
AI::find_halfway_pos_between_buildings(Building::Type first, Building::Type second) {
	AILogLogger["util_find_halfway_pos_between_buildings"] << name << " inside get_halfway_pos_between_buildings, type1 " << NameBuilding[first] << " type2 " << NameBuilding[second] << " for stock_pos " << stock_pos;
	update_building_counts();
	Building::Type type[2] = { first, second };
	MapPos found_pos[2] = { bad_map_pos, bad_map_pos };
	for (int x = 0; x < 2; x++) {
		AILogLogger["util_find_halfway_pos_between_buildings"] << name << " searching this stock area for a building of type" << x << " " << NameBuilding[type[x]];
		if (stock_buildings.at(stock_pos).occupied_count[type[x]] >= 1) {
			AILogLogger["util_find_halfway_pos_between_buildings"] << name << " searching for an OCCUPIED building of type" << x << " " << NameBuilding[type[x]];
			for (MapPos center_pos : stock_buildings.at(stock_pos).occupied_military_pos) {
				for (unsigned int i = 0; i < spiral_dist(9); i++) {
					MapPos pos = map->pos_add_extended_spirally(center_pos, i);
					if (!map->has_building(pos))
						continue;
					AILogLogger["util_find_halfway_pos_between_buildings"] << name << " found a building at pos " << pos;
					Building *building = game->get_building_at_pos(pos);
					AILogLogger["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " is of type " << NameBuilding[building->get_type()];
					if (building->get_type() != type[x]) {
						AILogLogger["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is not the right type, continuing";
						continue;
					}
					AILogLogger["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is the right type, checking its status";
					if (building->is_done() && building->has_serf()) {
						found_pos[x] = building->get_position();
						AILogLogger["util_find_halfway_pos_between_buildings"] << name << " found an acceptable OCCUPIED building of type" << x << " " << NameBuilding[type[x]] << " at pos " << found_pos[x];
						break;
					}
				}
				
			}
		}
		else if (completed_building_count[type[x]] >= 1) {
			AILogLogger["util_find_halfway_pos_between_buildings"] << name << " searching for a COMPLETED building of type" << x << " " << NameBuilding[type[x]];
			for (MapPos center_pos : stock_buildings.at(stock_pos).occupied_military_pos) {
				for (unsigned int i = 0; i < spiral_dist(9); i++) {
					MapPos pos = map->pos_add_extended_spirally(center_pos, i);
					if (!map->has_building(pos))
						continue;
					AILogLogger["util_find_halfway_pos_between_buildings"] << name << " found a building at pos " << pos;
					Building *building = game->get_building_at_pos(pos);
					AILogLogger["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " is of type " << NameBuilding[building->get_type()];
					if (building->get_type() != type[x]) {
						AILogLogger["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is not the right type, continuing";
						continue;
					}
					AILogLogger["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is the right type, checking its status";
					if (building->is_done()) {
						found_pos[x] = building->get_position();
						AILogLogger["util_find_halfway_pos_between_buildings"] << name << " found an acceptable COMPLETED building of type" << x << " " << NameBuilding[type[x]] << " at pos " << found_pos[x];
						break;
					}
				}

			}
		}
		else if (building_count[type[x]] >= 1) {
			AILogLogger["util_find_halfway_pos_between_buildings"] << name << " searching for ANY building of type" << x << " " << NameBuilding[type[x]];
			for (MapPos center_pos : stock_buildings.at(stock_pos).occupied_military_pos) {
				AILogLogger["util_find_halfway_pos_between_buildings"] << name << " searching around center_pos " << center_pos;
				for (unsigned int i = 0; i < spiral_dist(9); i++) {
					MapPos pos = map->pos_add_extended_spirally(center_pos, i);
					if (!map->has_building(pos))
						continue;
					AILogLogger["util_find_halfway_pos_between_buildings"] << name << " found a building at pos " << pos;
					Building *building = game->get_building_at_pos(pos);
					AILogLogger["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " is of type " << NameBuilding[building->get_type()];
					if (building->get_type() != type[x]) {
						AILogLogger["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is not the right type, continuing";
						continue;
					}
					AILogLogger["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is the right type, checking its status";
					if (building->has_serf()) {
						found_pos[x] = building->get_position();
						AILogLogger["util_find_halfway_pos_between_buildings"] << name << " found an acceptable ANY building of type" << x << " " << NameBuilding[type[x]] << " at pos " << found_pos[x];
						break;
					}
				}

			}
		}
		else {
			AILogLogger["util_find_halfway_pos_between_buildings"] << name << " no buildings of type" << x << " " << NameBuilding[type[x]] << " known in realm, returning bad_map_pos";
			return bad_map_pos;
		}
		// this shouldn't happen if update_building_counts() is accurate unless building was destroyed while searching
		if (found_pos[x] == bad_map_pos) {
			AILogLogger["util_find_halfway_pos_between_buildings"] << name << " could not find expected building of type" << x << " " << NameBuilding[type[x]] << " in realm despite being known! returning bad_map_pos";
			return bad_map_pos;
		}
		AILogLogger["util_find_halfway_pos_between_buildings"] << name << " found best building of type" << x << " " << NameBuilding[type[x]] << " at pos " << found_pos[x];
	}
	AILogLogger["util_find_halfway_pos_between_buildings"] << name << " found both buildings, " << NameBuilding[type[0]] << " at " << found_pos[0] << ", and " << NameBuilding[type[1]] << " at " << found_pos[1];
	AILogLogger["util_find_halfway_pos_between_buildings"] << name << " finding halfway point";
	MapPos halfway_pos = get_halfway_pos(found_pos[0], found_pos[1]);
	AILogLogger["util_find_halfway_pos_between_buildings"] << name << " done get_halfway_pos_between_buildings, returning halfway_pos " << halfway_pos;
	return halfway_pos;
}


//
// build a Road object by following the paths from start_pos in specified direction
//    until another flag is found.  The start pos doesn't have to be a real flag
Road
AI::trace_existing_road(PMap map, MapPos start_pos, Direction dir) {
	AILogLogger["util_trace_existing_road"] << name << "  inside trace_existing_road, start_pos " << start_pos << ", dir: " << NameDirection[dir];
	Road road;
	if (!map->has_path(start_pos, dir)) {
		AILogLogger["util_trace_existing_road"] << name << " no path found at " << start_pos << " in direction " << NameDirection[dir] << "!  FIND OUT WHY";
		return road;
	}
	road.start(start_pos);
	//ai_mark_road.start(start_pos);
	MapPos pos = start_pos;
	while (true) {
		road.extend(dir);
		//ai_mark_road->extend(dir);
		pos = map->move(pos, dir);
		std::this_thread::sleep_for(std::chrono::milliseconds(0));
		if (map->has_flag(pos) && pos != start_pos) {
			//AILogLogger["util_trace_existing_road"] << name << " flag found at pos " << pos << ", returning road (which has length " << road.get_length() << ")";
			//ai_mark_road->invalidate();
			return road;
		}
		for (Direction new_dir : cycle_directions_cw()) {
			//AILogLogger["util_trace_existing_road"] << name << " looking for path from pos " << pos << " in dir " << NameDirection[new_dir];
			if (map->has_path(pos, new_dir) && new_dir != reverse_direction(dir)) {
				//AILogLogger["util_trace_existing_road"] << name << " found path from pos " << pos << " in dir " << NameDirection[new_dir];
				dir = new_dir;
				break;
			}
		}
	}
}

// create vector of the six hexagonal "corner" positions (N,NE,SE,S,SW,NW),
//   about 4 tiles out spirally from the specified position
MapPosVector
AI::get_corners(MapPos center) {
	AILogLogger["util_get_corners"] << name << " inside AI::get_corners(pos)";
	MapPosVector positions;
	MapPosVector::iterator it;
	for (Direction dir : cycle_directions_rand_cw()) {
		MapPos pos = center;
		for (int x = 0; x < 5; x++) {
			pos = map->move(pos, dir);
		}
		positions.push_back(pos);
	}
	return positions;
}

// create vector of the six hexagonal "corner" positions (N,NE,SE,S,SW,NW),
//   an arbitrary distance out - NOTE this is straight line NOT spirally distance!!!
MapPosVector
AI::get_corners(MapPos center, unsigned int distance) {
	AILogLogger["util_get_corners"] << name << " inside AI::get_corners(pos, dist)";
	MapPosVector positions;
	if (distance > 24) {
		AILogLogger["util_get_corners2"] << name << " get_corners only supports up to distance 24!  this is straight line not spirally distance.  Capping it at 24";
		distance = 24;
	}
	MapPosVector::iterator it;
	for (Direction dir : cycle_directions_rand_cw()) {
		MapPos pos = center;
		for (unsigned int x = 0; x < distance; x++) {
			pos = map->move(pos, dir);
		}
		positions.push_back(pos);
	}
	return positions;
}


// return count of terrain tiles of the specified type range, like hills (Tundra)
unsigned int
AI::count_terrain_near_pos(MapPos center_pos, unsigned int distance, Map::Terrain res_start_index, Map::Terrain res_end_index, std::string color) {
	AILogLogger["util_count_terrain_near_pos"] << name << " inside count_terrain_near_pos";
	//AILogLogger["util_count_terrain_near_pos"] << name << " AI: inside AI::count_terrain_near_pos";
	//AILogLogger["util_count_terrain_near_pos"] << name << " AI: center_pos " << center_pos << ", distance " << distance << ", res_start_index " << NameTerrain[res_start_index] << ", res_end_index " << NameTerrain[res_end_index];
	unsigned int count = 0;
	for (unsigned int i = 0; i < distance; i++) {
		MapPos pos = map->pos_add_extended_spirally(center_pos, i);
		//AILogLogger["util_count_terrain_near_pos"] << name << " AI: terrain at pos " << pos << " has type " << terrain;
		if (AI::has_terrain_type(game, pos, res_start_index, res_end_index)) {
			//AILogLogger["util_count_terrain_near_pos"] << name << " AI: found matching terrain at pos " << pos;
			++count;
		}
	}
	//AILogLogger["util_count_terrain_near_pos"] << name << " AI: found count " << count << " matching terrain of types " << NameTerrain[res_start_index] << " - " << NameTerrain[res_end_index];
	return count;
}

// return count of terrain tiles of the specified type range, like hills (Tundra)...
//   ... that have NO hard objects on them (flags, buildings, stones, etc.).  Paths are acceptable
//  initially only used for hill/sign ratios
// this function is almost identical to count_farmable_land except it doesn't include existing wheat fiels
//  maybe merge them?  Or make this one geologist/hill-specific
unsigned int
AI::count_empty_terrain_near_pos(MapPos center_pos, unsigned int distance, Map::Terrain res_start_index, Map::Terrain res_end_index, std::string color) {
	AILogLogger["util_count_empty_terrain_near_pos"] << name << " inside count_terrain_near_pos";
	//AILogLogger["util_count_empty_terrain_near_pos"] << name << " AI: inside AI::count_empty_terrain_near_pos";
	//AILogLogger["util_count_empty_terrain_near_pos"] << name << " AI: center_pos " << center_pos << ", distance " << distance << ", res_start_index " << NameTerrain[res_start_index] << ", res_end_index " << NameTerrain[res_end_index];
	unsigned int count = 0;
	for (unsigned int i = 0; i < distance; i++) {
		MapPos pos = map->pos_add_extended_spirally(center_pos, i);
		//AILogLogger["util_count_empty_terrain_near_pos"] << name << " AI: terrain at pos " << pos << " has type " << terrain;
		if (AI::has_terrain_type(game, pos, res_start_index, res_end_index)) {
			Map::Object obj_type = map->get_obj(pos);
			// exclude tiles with blocking objects (anything not on this list)
			if (obj_type == Map::ObjectNone
				|| (obj_type >= Map::ObjectFieldExpired && obj_type <= Map::ObjectSignSmallStone)) {
				//AILogLogger["util_count_empty_terrain_near_pos"] << name << " AI: found matching empty terrain at pos " << pos;
				++count;
			}
		}
	}
	//AILogLogger["util_count_empty_terrain_near_pos"] << name << " AI: found count " << count << " matching empty terrain of types " << NameTerrain[res_start_index] << " - " << NameTerrain[res_end_index];
	return count;
}

// count farmable land  - count the number of grass tiles, with no paths, 
//     and no obstacles (other than existing wheat fields)
unsigned int
AI::count_farmable_land(MapPos center_pos, unsigned int distance, std::string color) {
	Map::Terrain res_start_index = Map::TerrainGrass0;
	Map::Terrain res_end_index = Map::TerrainGrass3;
	AILogLogger["util_count_farmable_land"] << name << " inside AI::count_farmable_land";
	//AILogLogger["util_count_farmable_land"] << name << " center_pos " << center_pos << ", distance " << distance << ", res_start_index " << NameTerrain[res_start_index] << ", res_end_index " << NameTerrain[res_end_index];
	unsigned int count = 0;
	for (unsigned int i = 0; i < distance; i++) {
		MapPos pos = map->pos_add_extended_spirally(center_pos, i);
		//AILogLogger["util_count_farmable_land"] << name << " AI: terrain at pos " << pos << " has type " << terrain;
		if (AI::has_terrain_type(game, pos, res_start_index, res_end_index)) {
			Map::Object obj_type = map->get_obj(pos);
			// exclude tiles with blocking objects (anything not on this list)
			if (obj_type == Map::ObjectNone
				|| (obj_type >= Map::ObjectSeeds0 && obj_type <= Map::ObjectFieldExpired)
				|| (obj_type >= Map::ObjectField0 && obj_type <= Map::ObjectField5)
				|| (obj_type >= Map::ObjectFelledPine0 && obj_type <= Map::ObjectFelledTree4) ) {
			}
			else {
				continue;
			}
			// exclude tiles with paths
			if (map->paths(pos)) {
				continue;
			}

			++count;
		}
	}
	//AILogLogger["util_count_farmable_land"] << name << " AI: found count " << count << " matching terrain of types " << NameTerrain[res_start_index] << " - " << NameTerrain[res_end_index];
	AILogLogger["util_count_farmable_land"] << name << " done count_farmable_land, returning count " << count;
	return count;
}


// sort a MapPosSet by value, ascending, and return as sorted vector of the keys (throwing the values away)
MapPosVector
AI::sort_by_val_asc(MapPosSet set) {
	AILogLogger["util_sort_by_val_asc"] << name << " inside AI::sort_by_val_asc";
	// this is C++14 only?  worked on visual studio 2017 on windows but not VC code w/gcc on linux
	auto cmp = [](const auto &p1, const auto &p2) { return p2.second > p1.second || !(p1.second > p2.second) && p1.first > p2.first; };
	std::set< std::pair<MapPos, unsigned int>, decltype(cmp)> sorted_set(set.begin(), set.end(), cmp);
	MapPosVector sorted_vector;
	for (auto pair : sorted_set) {
		//AILogLogger["util_sort_by_val_asc"] << name << " sorted MapPosSet pos: " << pair.first << ", val: " << pair.second;
		sorted_vector.push_back(pair.first);
	}
	return sorted_vector;
}

// sort a MapPosSet by value, descending, and return as sorted vector of the keys (throwing the values away)
MapPosVector
AI::sort_by_val_desc(MapPosSet set) {
	AILogLogger["util_sort_by_val_desc"] << name << " inside AI::sort_by_val_desc";
	auto cmp = [](const auto &p1, const auto &p2) { return p2.second < p1.second || !(p1.second < p2.second) && p1.first < p2.first; };
	std::set< std::pair<MapPos, unsigned int>, decltype(cmp)> sorted_set(set.begin(), set.end(), cmp);
	MapPosVector sorted_vector;
	for (auto pair : sorted_set) {
		//AILogLogger["util_sort_by_val_desc"] << name << " sorted MapPosSet pos: " << pair.first << ", val: " << pair.second;
		sorted_vector.push_back(pair.first);
	}
	return sorted_vector;
}

// return count of individual objects of the specified type range, such as trees or geologist signs
unsigned int
AI::count_objects_near_pos(MapPos center_pos, unsigned int distance, Map::Object res_start_index, Map::Object res_end_index, std::string color) {
	AILogLogger["util_count_objects_near_pos"] << name << " inside AI::count_objects_near_pos";
	//AILogLogger["util_count_objects_near_pos"] << name << " AI: center_pos " << center_pos << ", distance " << distance << ", res_start_index " << res_start_index << "(" << NameObject[res_start_index] << ")"
	//      << ", res_end_index " << res_end_index << "(" << NameObject[res_end_index] << ")";
	unsigned int count = 0;
	for (unsigned int i = 0; i < distance; i++) {
		MapPos pos = map->pos_add_extended_spirally(center_pos, i);
		if (map->get_obj(pos) >= res_start_index && map->get_obj(pos) <= res_end_index) {
			++count;
			//AILogLogger["util_count_objects_near_pos"] << name << " AI: found matching object at pos " << pos << ", type " << map->get_obj(pos);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(0));
	}
	//AILogLogger["util_count_objects_near_pos"] << name << " AI: found count " << count << " matching objects of types " << NameObject[res_start_index] << " - " << NameObject[res_end_index];
	return count;
}


// build specified building in first valid pos near center_pos, return pos where built
//   also connect building to road system.  if it cannot be connected, BURN IT DOWN
//     and the flag too.  Then resume trying to build it until search path exhausted
MapPos
AI::build_near_pos(MapPos center_pos, unsigned int distance, Building::Type building_type) {
	// time this function for debugging
	std::clock_t start;
	double duration;
	start = std::clock();

	AILogLogger["util_build_near_pos"] << name << " inside AI::build_near_pos with building type index " << NameBuilding[building_type] << ", distance " << distance << ", and target pos " << center_pos;

	MapPos nearest_stock = find_nearest_stock(center_pos);
	if (nearest_stock != stock_pos) {
		AILogLogger["util_build_near_pos"] << name << " this center_pos " << center_pos << " is closer to another stock (at pos " << nearest_stock << ") than the current stock_pos " << stock_pos << ", returning notplaced_pos";
		return notplaced_pos;
	}

	if (building_type == Building::TypeHut) {
		if (stock_buildings.at(nearest_stock).unfinished_hut_count >= max_unfinished_huts) {
			AILogLogger["util_build_near_pos"] << name << " max unfinished huts limit " << max_unfinished_huts << " reached, not building";
			// should be returning not_built_pos or bad_map pos?? stopbuilding is less appropriate with separate counts set up for huts vs other buildings
			return stopbuilding_pos;
		}
	}

	// always be willing to build wood/stone buildings and knight huts or everything can grind to a halt
	//   also Mines, but don't connect these to the road network yet
	if (stock_buildings.at(nearest_stock).unfinished_count >= max_unfinished_buildings && building_type != Building::TypeSawmill
	//if (unfinished_building_count >= max_unfinished_buildings && building_type != Building::TypeSawmill
		&& building_type != Building::TypeLumberjack && building_type != Building::TypeStonecutter
		&& building_type != Building::TypeHut && building_type != Building::TypeCoalMine
		&& building_type != Building::TypeIronMine && building_type != Building::TypeGoldMine
		&& building_type != Building::TypeStoneMine && building_type != Building::TypeForester
		) {
		AILogLogger["util_build_near_pos"] << name << " max unfinished buildings limit " << max_unfinished_buildings << " reached, not building";
		return stopbuilding_pos;
	}

	MapPos built_pos = bad_map_pos;
	unsigned int total = 0;
	for (unsigned int i = 0; i < distance; i++) {
		MapPos pos = map->pos_add_extended_spirally(center_pos, i);
		if (map->get_owner(pos) != player_index) {
			continue;
		}
		// special bug work-around: do not build a Stonecutter down-right from a Stone deposit because the stonecutter cannot use it and stonecutting logic breaks
		if (building_type == Building::TypeStonecutter) {
			// check to see if there are stones up-left from this pos
			MapPos upleft_pos = map->move_up_left(pos);
			int obj_type = map->get_obj(upleft_pos);
			if (obj_type >= Map::ObjectStone0 && obj_type <= Map::ObjectStone7) {
				AILogLogger["util_build_near_pos"] << name << " SPECIAL BUG WORKAROUND - pos up-left from potential stonecutter at pos " << pos << " is type " << NameObject[obj_type] << ", not building here";
				continue;
			}
		}
		if (game->can_build_building(pos, building_type, player)) {
			AILogLogger["util_build_near_pos"] << name << " inside AI::build_near_pos, can build " << NameBuilding[building_type] << " at pos " << pos;
		}
		else {
			// simply can't build here
			//AILogLogger["util_build_near_pos"] << name << " cannot build " << NameBuilding[building_type] << " of sizetype " << building_sizes[building_type] << " at pos " << pos;
			continue;
		}
		// avoid known bad positions for this building type.  note this list is NOT persisted if AI thread goes away or game load/restart
		if (is_bad_building_pos(pos, building_type)) {
			AILogLogger["util_build_near_pos"] << name << " skipping this pos, it is marked in bad_building_pos list";
			continue;
		}
		// avoid building knight huts too close together
		if (building_type == Building::TypeHut && AI::building_exists_near_pos(pos, AI::spiral_dist(5), Building::TypeHut)) {
			AILogLogger["util_build_near_pos"] << name << " a knight hut already exists near pos " << pos << ", skipping this pos";
			continue;
		}
		// try to build it
		AILogLogger["util_build_near_pos"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->build_building (build_near_pos) of type " << NameBuilding[building_type];
		game->get_mutex()->lock();
		AILogLogger["util_build_near_pos"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->build_building (build_near_pos) of type " << NameBuilding[building_type];
		bool was_built = game->build_building(pos, building_type, player);
		AILogLogger["util_build_near_pos"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->build_building (build_near_pos) of type " << NameBuilding[building_type];
		game->get_mutex()->unlock();
		AILogLogger["util_build_near_pos"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->build_building (build_near_pos) of type " << NameBuilding[building_type];
		if (!was_built) {
			AILogLogger["util_build_near_pos"] << name << " failed to build building of type " << NameBuilding[building_type] << " despite can_build being true!  WAITING 10sec - look at the pos in cyan!";
			ai_mark_pos.erase(pos);
			ai_mark_pos.insert(std::make_pair(pos, "cyan"));
			std::this_thread::sleep_for(std::chrono::milliseconds(10000));
			continue;
		}

		AILogLogger["util_build_near_pos"] << name << " successfully built a building of type " << NameBuilding[building_type] << " at pos " << pos;
		// Mines are to be built, but not connected immediately so they can be placed before geologist signs fade,
		//   even if player's economy doesn't yet require the mine.  Used to establish good mine locations early
		if (building_type == Building::TypeCoalMine
			|| building_type == Building::TypeIronMine
			|| building_type == Building::TypeGoldMine
			|| building_type == Building::TypeStoneMine) {
			AILogLogger["util_build_near_pos"] << name << " not connecting road or including in unfinished_building_count because this is a Mine";
			return pos;
		}
		AILogLogger["util_build_near_pos"] << name << " trying to build road to connect it";
		MapPos flag_pos = map->move_down_right(pos);
		bool road_built = false;
		if (!game->get_flag_at_pos(flag_pos)->is_connected()) {
			road_built = AI::build_best_road(flag_pos, road_options);
		}
		if (!road_built && !game->get_flag_at_pos(flag_pos)->is_connected()) {
			AILogLogger["util_build_near_pos"] << name << " failed to connect building (of type " << NameBuilding[building_type] << ")'s flag to road network!  burning it down";
			AILogLogger["util_build_near_pos"] << name << " LOOK AT BUILDING AT POS " << pos << ", MARKED IN CYAN";
			ai_mark_pos.erase(pos);
			ai_mark_pos.insert(std::make_pair(pos, "cyan"));
			std::this_thread::sleep_for(std::chrono::milliseconds(5000));
			AILogLogger["util_build_near_pos"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->demolish_building/flag (build_near_pos failed to connect)";
			game->get_mutex()->lock();
			AILogLogger["util_build_near_pos"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->demolish_building/flag (build_near_pos failed to connect)";
			game->demolish_building(pos, player);
			game->demolish_flag(flag_pos, player);
			AILogLogger["util_build_near_pos"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->demolish_building/flag (build_near_pos failed to connect)";
			game->get_mutex()->unlock();
			AILogLogger["util_build_near_pos"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->demolish_building/flag (build_near_pos failed to connect)";
			// mark as bad pos, to avoid repeateadly rebuilding same building in same spot
			bad_building_pos.insert(std::make_pair(pos, building_type));
		}
		else {
			AILogLogger["util_build_near_pos"] << name << " successfully connected building at pos " << pos << ", with flag_pos " << flag_pos << ", to road network";
			if (building_type == Building::TypeHut) {
				stock_buildings.at(nearest_stock).unfinished_hut_count++;
				AILogLogger["util_build_near_pos"] << name << " incrementing unfinished_hut_count, is now: " << unfinished_hut_count;
			}
			else {
				stock_buildings.at(nearest_stock).unfinished_count++;
				AILogLogger["util_build_near_pos"] << name << " found unfinished " << NameBuilding[building_type] << ", incrementing unfinished_building_count, is now: " << unfinished_building_count;
			}
			duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;
			AILogLogger["util_build_near_pos"] << name << " util_build_near_pos call took " << duration;
			return pos;
		}
	}
	AILogLogger["util_build_near_pos"] << name << " could not find a place to build, or failed to build, type " << NameBuilding[building_type] << " near pos " << center_pos << " after checking " << distance << " positions";
	duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;
	AILogLogger["util_build_near_pos"] << name << " util_build_near_pos call took " << duration;
	return notplaced_pos;
}

// return count of total stones in the area.  A stone pile can have 1-8 stones
// ALSO, have to work around the original-game bug where a stonecutter cannot harvest stones if a building is directly down-right from a stone pile
//   To avoid this, do not count stones that have a building down-right from the pile
unsigned int
AI::count_stones_near_pos(MapPos center_pos, unsigned int distance) {
	AILogLogger["util_count_stones_near_pos"] << name << " inside count_stones_near_pos";
	Map::Object res_start_index = Map::ObjectStone0;
	Map::Object res_end_index = Map::ObjectStone7;
	std::string color = "gray";
	//AILogLogger["util_count_stones_near_pos"] << name << " AI: center_pos " << center_pos << ", distance " << distance << ", res_start_index " << NameObject[res_start_index] << ", res_end_index " << NameObject[res_end_index];
	unsigned int total = 0;
	for (unsigned int i = 0; i < distance; i++) {
		MapPos pos = map->pos_add_extended_spirally(center_pos, i);
		if (map->get_obj(pos) >= res_start_index && map->get_obj(pos) <= res_end_index) {
			//AILogLogger["util_count_stones_near_pos"] << name << " getting value of map object at pos: " << pos << " with map obj type " << NameObject[map->get_obj(pos)];
			// because the higher MapObject indexes represent smaller resource values, the difference is negative
			//   so invert the difference and add one, because it starts at zero
			int value = 1 + (-1 * (map->get_obj(pos) - res_end_index));
			//AILogLogger["util_count_stones_near_pos"] << name << " map object has resource value of " << value << " at pos " << pos;
			// bug work-around - ignore stones that cannot be harvested because a building is down-right from the pile
			if (map->has_building(map->move_down_right(pos))) {
				// this seems to be working as expected
				AILogLogger["util_count_stones_near_pos"] << name << " stones bug workaround - ignoring stone pile at pos " << pos << " because a building is down-right from it so it cannot be harvested";
				value = 0;
			}
			total += value;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(0));
	}
	//AILogLogger["util_count_stones_near_pos"] << name << " AI: found total value " << total << " of objects types " << NameObject[res_start_index] << " - " << NameObject[res_end_index];
	return total;
}

// count the number of knights that would be sent out if occupation level is changed, to avoid depleting the reserve of idle_knights
//  this function only counts the **TARGET** levels, ignoring if that many knights are actually available and/or currently in buildings
unsigned int
AI::count_knights_affected_by_occupation_level_change(unsigned int current_level, unsigned int new_level) {
	AILogLogger["util_count_knights_affected_by_occupation_level_change"] << name << " inside count_knights_affected_by_occupation_level_change, level " << current_level << " -> " << new_level;
	// calling update_building_counts here is too slow because of mutex locking and causes do_manage_knight_occupation_levels to leave things at zero
	//   for long enough that the game update actually starts moving knights around.  Having a super accurate count is not critical here, it should 
	//   be able to use whatever values were found during the most recent update, which I believe is happening at the start of each AI loop anyway
	//update_building_counts();
	unsigned int hut_count = building_count[Building::TypeHut];
	unsigned int tower_count = building_count[Building::TypeTower];
	unsigned int garrison_count = building_count[Building::TypeFortress];
	AILogLogger["util_count_knights_affected_by_occupation_level_change"] << name << " military building counts: huts " << hut_count << ", towers " << tower_count << ", garrisons " << garrison_count;
	unsigned int total_current = 0;
	total_current += slots_hut[current_level] * hut_count;
	total_current += slots_tower[current_level] * tower_count;
	total_current += slots_garrison[current_level] * garrison_count;
	unsigned int total_new = 0;
	total_new += slots_hut[new_level] * hut_count;
	total_new += slots_tower[new_level] * tower_count;
	total_new += slots_garrison[new_level] * garrison_count;
	// diff must be signed as it can be negative!
	//unsigned int diff = total_new - total_current;
	int diff = unsigned(total_new) - unsigned(total_current);
	if (diff < 0) {
		AILogLogger["util_count_knights_affected_by_occupation_level_change"] << name << " diff " << diff << " is negative, setting to zero instead";
		diff = 0;
	}
	AILogLogger["util_count_knights_affected_by_occupation_level_change"] << name << " knight counts if at fully-occupied-*target* staffing level: current " << total_current << ", new " << total_new << ", diff " << diff;
	return diff;
}

// this is only considering expanding the borders of the military buildings associated with this stock
//   should it be done this way per-stock, or once globally using realm_occupied_military_buildings ?
MapPos
AI::expand_borders(MapPos center_pos) {
	// time this function for debugging
	std::clock_t start;
	double duration;
	start = std::clock();
	AILogLogger["util_expand_borders"] << name << " inside AI::expand_borders for stock_pos " << stock_pos;
	ai_status.assign("EXPANDING BORDERS");
	MapPos built_pos = bad_map_pos;
	MapPosSet count_by_corner;
	for (std::string goal : expand_towards) {
		AILogLogger["util_expand_borders"] << name << " expand_towards goal list includes item: " << goal;
	}
	// get list of military buildings as centers to look around for borders
	AILogLogger["util_expand_borders"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for expand_borders)";
	game->get_mutex()->lock();
	AILogLogger["util_expand_borders"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for expand_borders)";
	Game::ListBuildings buildings = game->get_player_buildings(player);
	AILogLogger["util_expand_borders"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player) (for expand_borders)";
	game->get_mutex()->unlock();
	AILogLogger["util_expand_borders"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player) (for expand_borders)";
	for (MapPos center_pos : stock_buildings.at(stock_pos).occupied_military_pos) {
		duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;
		AILogLogger["util_expand_borders"] << name << " about to check around pos " << center_pos << ", SO FAR util_expand_borders call took " << duration;
		// find territory edge in each direction
		for (Direction dir : cycle_directions_rand_cw()) {
			MapPos pos = center_pos;
			AILogLogger["util_expand_borders"] << name << " looking for territory edge from pos " << pos << " in direction " << dir << " / " << NameDirection[dir];
			int tiles_moved = 0;
			while (map->get_owner(pos) == player_index) {
				pos = map->move(pos, dir);
				tiles_moved++;
				// give up after 10 tiles because we should only be looking from huts that are right on the borders, not internal ones
				//  these huts should be at most 8 tiles or so from the border
				// Without this optimization every single dir from every hut is followed to borders and area scored, resulting
				//  in a lot of duplicate scoring
				// Also, it makes the "check for circumnavigating the globe" obsolete because that is way more than ten tiles
				if (tiles_moved >= 10) {
					AILogLogger["util_expand_borders"] << name << " did not find border within ten tiles from center in this dir, this must not be a border hut";
					break;
				}
			}
			unsigned int score = AI::score_area(pos, AI::spiral_dist(6));
			AILogLogger["util_expand_borders"] << name << " border in direction " << dir << " has score " << score;
			// include all corners so we'll at least expand *somewhere* even if no goal resources found
			//   the sort function will still prefer areas with resources
			count_by_corner.insert(std::make_pair(pos, score));
		}
	} // end foreach occupied military building
	MapPosVector search_positions = AI::sort_by_val_desc(count_by_corner);
	for (MapPos corner_pos : search_positions) {
		AILogLogger["util_expand_borders"] << name << " try to build knight hut near pos " << corner_pos;
		built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(4), Building::TypeHut);

		if (built_pos == stopbuilding_pos) {
			duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;
			AILogLogger["util_expand_borders"] << name << " done util_expand_borders call took " << duration;
			return stopbuilding_pos;
		}
	}
	duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;
	AILogLogger["util_expand_borders"] << name << " done util_expand_borders call took " << duration;
	if (built_pos == bad_map_pos || built_pos == notplaced_pos || built_pos == stopbuilding_pos) {
		AILogLogger["util_expand_borders"] << name << " couldn't place knight hut";
		return notplaced_pos;
	}

	return built_pos;

}




unsigned int
AI::score_area(MapPos center_pos, unsigned int distance) {
	// time this function for debugging
	std::clock_t start;
	double duration;
	start = std::clock();
	AILogLogger["util_score_area"] << name << " inside AI::score_area, center_pos " << center_pos << ", distance " << distance;
	unsigned int total_value = 0;
	for (unsigned int i = 0; i < distance; i++) {
		MapPos pos = map->pos_add_extended_spirally(center_pos, i);
		Map::Object obj = map->get_obj(pos);
		//AILogLogger["util_score_area"] << name << " at pos " << pos << " with object type " << NameObject[obj];
		size_t pos_value = 0;  // easier to make this size_t than static_cast all the .size values
		//
		// terrain and resources
		//
		unsigned int gold_signs = 0;
		unsigned int iron_signs = 0;
		unsigned int coal_signs = 0;
		unsigned int stone_signs = 0;
		// if grass or water with obstacles, or already having a field...
		if (obj == Map::ObjectNone &&
			(AI::has_terrain_type(game, pos, Map::TerrainGrass0, Map::TerrainGrass3) ||
				AI::has_terrain_type(game, pos, Map::TerrainWater0, Map::TerrainWater3)) ||
			obj >= Map::ObjectSeeds0 && obj <= Map::ObjectFieldExpired ||
			obj >= Map::ObjectField0 && obj <= Map::ObjectField5) {
			pos_value += expand_towards.count("foods") * foods_weight;
			AILogLogger["util_score_area"] << name << " adding foods count 1 with value " << expand_towards.count("foods") * foods_weight;
		}
		if (obj >= Map::ObjectTree0 && obj <= Map::ObjectPine7) {
			pos_value += expand_towards.count("trees") * trees_weight;
			AILogLogger["util_score_area"] << name << " adding trees count 1 with value " << expand_towards.count("trees") * trees_weight;
		}
		if (obj >= Map::ObjectStone0 && obj <= Map::ObjectStone7) {
			int stonepile_value = 1 + (-1 * (obj - Map::ObjectStone7));
			pos_value += expand_towards.count("stones") * stones_weight * stonepile_value;
			AILogLogger["util_score_area"] << name << " adding stones count " << stonepile_value << " with value " << expand_towards.count("stones") * stones_weight;
		}
		if (AI::has_terrain_type(game, pos, Map::TerrainTundra0, Map::TerrainSnow0)) {
			if (obj >= Map::ObjectSignEmpty && obj <= Map::ObjectSignSmallStone) {
				AILogLogger["util_score_area"] << name << " found a sign (of type " << NameObject[obj] << "), not counting this hill";
			}
			else {
				pos_value += expand_towards.count("hills") * hills_weight;
				AILogLogger["util_score_area"] << name << " adding hills count 1 with value " << expand_towards.count("hills") * hills_weight;
			}
		}
		if (obj == Map::ObjectSignLargeGold) { gold_signs += 3; }
		if (obj == Map::ObjectSignSmallGold) { gold_signs += 1; }
		if (obj == Map::ObjectSignSmallGold || obj == Map::ObjectSignLargeGold) {
			pos_value += expand_towards.count("gold_ore") * gold_ore_weight * gold_signs;
			AILogLogger["util_score_area"] << name << " adding gold_ore count " << gold_signs << " with value " << expand_towards.count("gold_ore") * gold_ore_weight * gold_signs;
		}
		if (obj == Map::ObjectSignLargeIron) { iron_signs += 3; }
		if (obj == Map::ObjectSignSmallIron) { iron_signs += 1; }
		if (obj == Map::ObjectSignSmallIron || obj == Map::ObjectSignLargeIron) {
			pos_value += expand_towards.count("iron_ore") * iron_ore_weight * iron_signs;
			AILogLogger["util_score_area"] << name << " adding iron_ore count " << iron_signs << " with value " << expand_towards.count("iron_ore") * iron_ore_weight * iron_signs;
		}
		if (obj == Map::ObjectSignLargeCoal) { coal_signs += 3; }
		if (obj == Map::ObjectSignSmallCoal) { coal_signs += 1; }
		if (obj == Map::ObjectSignSmallCoal || obj == Map::ObjectSignLargeCoal) {
			pos_value += expand_towards.count("coal")      * coal_weight * coal_signs;
			AILogLogger["util_score_area"] << name << " adding coal count " << coal_signs << " with value " << expand_towards.count("coal_ore") * coal_weight * coal_signs;
		}
		if (obj == Map::ObjectSignLargeStone) { stone_signs += 3; }
		if (obj == Map::ObjectSignSmallStone) { stone_signs += 1; }
		if (obj == Map::ObjectSignSmallStone || obj == Map::ObjectSignLargeStone) {
			pos_value += expand_towards.count("stones") * stone_signs_weight * stone_signs;
			AILogLogger["util_score_area"] << name << " adding stones count " << stone_signs << " with value " << expand_towards.count("stones") * stone_signs_weight * stone_signs;
		}
		//
		// defense - build knight huts to buffer borders
		//    prioritizing areas with own civ buildings, or any place that enemy territory found
		//
		if (map->get_owner(pos) == player_index) {
			if (obj == Map::ObjectLargeBuilding && !game->get_building_at_pos(pos)->is_military()) {
				pos_value += expand_towards.count("create_buffer") * 3;
				AILogLogger["util_score_area"] << name << " adding defensive_buffer building value x3 for large civilian building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
			}
			if (obj == Map::ObjectSmallBuilding && !game->get_building_at_pos(pos)->is_military()) {
				pos_value += expand_towards.count("create_buffer") * 1;
				AILogLogger["util_score_area"] << name << " adding defensive_buffer building value x1 for small civilian building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
			}
		}
		// does -1 mean unowned?   need to check
		//  this code does seem to work so that must be the case
		if (map->get_owner(pos) != player_index && map->get_owner(pos) != -1) {
			pos_value += expand_towards.count("create_buffer") * 1;
			AILogLogger["util_score_area"] << name << " adding defensive_buffer value for enemy territory";
		}
		//
		// offense
		//
		if (scoring_attack
			&& map->get_owner(pos) != player_index
			&& map->get_owner(pos) != -1
			&& map->has_building(pos)
			) {
			//AILogLogger["util_score_area"] << name << " potential attack object is " << NameObject[obj] << " with building type " << NameBuilding[game->get_building_at_pos(pos)->get_type()];
			if (obj == Map::ObjectLargeBuilding && !game->get_building_at_pos(pos)->is_military()) {
				pos_value += 3;
				AILogLogger["util_score_area"] << name << " adding attack value x3 for large civilian building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
			}
			if (obj == Map::ObjectSmallBuilding && game->get_building_at_pos(pos)->get_type() == Building::TypeCoalMine){
				pos_value += 2;
				AILogLogger["util_score_area"] << name << " adding additional attack value x2 for building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
			}
			if (obj == Map::ObjectSmallBuilding && game->get_building_at_pos(pos)->get_type() == Building::TypeIronMine) {
				pos_value += 4;
				AILogLogger["util_score_area"] << name << " adding additional attack value x4 for building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
			}
			if (obj == Map::ObjectSmallBuilding && game->get_building_at_pos(pos)->get_type() == Building::TypeGoldMine) {
				pos_value += 6;
				AILogLogger["util_score_area"] << name << " adding additional attack value x6 for building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
			}
			if (obj == Map::ObjectSmallBuilding && !game->get_building_at_pos(pos)->is_military()) {
				pos_value += 1;
				AILogLogger["util_score_area"] << name << " adding attack value x1 for small civilian building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
			}
		}
		//
		// total it up
		//
		total_value += static_cast<unsigned int>(pos_value);
		AILogLogger["util_score_area"] << name << " score total so far: " << total_value;

	}
	AILogLogger["util_score_area"] << name << " found total score_area value " << total_value << " of terrain & objects in area " << center_pos;
	duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;
	AILogLogger["util_score_area"] << name << " done util_score_area call took " << duration;
	return total_value;
}


bool
AI::is_bad_building_pos(MapPos pos, Building::Type building_type) {
	AILogLogger["util_is_bad_building_pos"] << name << " inside AI::is_bad_building_pos";
	if (bad_building_pos.find(std::make_pair(pos, building_type)) != bad_building_pos.end()) {
		return true;
	}
	return false;
}


// get halfway point between two MapPos
MapPos
AI::get_halfway_pos(MapPos start_pos, MapPos end_pos) {
	AILogLogger["util_get_halfway_pos"] << name << " inside get_halfway_pos";
	MapPos halfway_pos;
	int start_col = map->pos_col(start_pos);
	int start_row = map->pos_row(start_pos);
	AILogLogger["util_get_halfway_pos"] << name << " start_pos is " << start_pos << ", start_col is " << start_col << ", start_row is " << start_row;
	int end_col = map->pos_col(end_pos);
	int end_row = map->pos_row(end_pos);
	AILogLogger["util_get_halfway_pos"] << name << " end_col is " << end_col << ", end_row is " << end_row;
	// it seems these are reversed start/end pos
	int col_dist = map->dist_x(start_pos, end_pos);
	AILogLogger["util_get_halfway_pos"] << name << " col_dist is " << col_dist;
	int row_dist = map->dist_y(start_pos, end_pos);
	AILogLogger["util_get_halfway_pos"] << name << " row_dist is " << row_dist;
	int halfway_col_dist = (map->dist_x(start_pos, end_pos) / 2);
	AILogLogger["util_get_halfway_pos"] << name << " halfway_col_dist is " << halfway_col_dist;
	int halfway_row_dist = (map->dist_y(start_pos, end_pos) / 2);
	AILogLogger["util_get_halfway_pos"] << name << " halfway_row_dist is " << halfway_row_dist;
	halfway_pos = map->pos(end_col + halfway_col_dist, end_row + halfway_row_dist);
	AILogLogger["util_get_halfway_pos"] << name << " halfway_pos is " << halfway_pos;
	return halfway_pos;
}


// return the MapPos of the stock nearest to the specified MapPos, including castle
//   if not found, default to castle_pos?
MapPos
AI::find_nearest_stock(MapPos pos) {
	// time this function for debugging
	std::clock_t start;
	double duration;
	start = std::clock();
	AILogLogger["util_find_nearest_stock"] << name << " inside find_nearest_stock to pos " << pos;
	// try searching by straightline pos instead of spirally
	unsigned int best_dist = bad_score;
	MapPos closest_stock = bad_map_pos;
	for (MapPos stock_pos : stocks_pos) {
		AILogLogger["util_find_nearest_stock"] << name << " considering stock_pos " << stock_pos;
		unsigned int dist = get_straightline_tile_dist(map, pos, stock_pos);
		AILogLogger["util_find_nearest_stock"] << name << " straightline tile dist from pos " << pos << " to stock_pos " << stock_pos << " is " << dist;
		if (dist < best_dist) {
			AILogLogger["util_find_nearest_stock"] << name << " stock at stock_pos " << stock_pos << " is the closest so far to pos " << pos << " , with dist " << dist;
			best_dist = dist;
			closest_stock = stock_pos;
		}
	}
	if (closest_stock == bad_map_pos || best_dist == bad_score) {
		AILogLogger["util_find_nearest_stock"] << name << " not found??  closest_stock: " << closest_stock << ", best_dist: " << best_dist;
	}
	AILogLogger["util_find_nearest_stock"] << name << " done find_nearest_stock to pos " << pos << ", closest stock is " << closest_stock << " with straightline dist " << best_dist;
	duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;
	AILogLogger["util_find_nearest_stock"] << name << " done find_nearest_stock call took " << duration;
	return closest_stock;
}



void
AI::score_enemy_targets(MapPosSet *scored_targets) {
	// time this function for debugging
	std::clock_t start;
	double duration;
	start = std::clock();
	AILogLogger["util_score_enemy_targets"] << name << " inside score_enemy_targets";
	//
	// MAJOR BUG - this is running before expansion goals are set (later in the AI loop), so it is not looking at resources that could
	//      be taken, ONLY at enemy buildings.  Does it make sense to persist expansion goals between AI loops?  Or move attacks to the end of the loop?
	//  temp fix -  stupid hack to work-around it
	expand_towards = last_expand_towards;
	update_building_counts();
	// foreach my hut in range of attacking enemy
	//		foreach enemy hut within range of attack
	//			score
	// on game load, castle_pos is unknown even though castle exists, need to find it
	AILogLogger["util_score_enemy_targets"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for score_enemy_targets)";
	game->get_mutex()->lock();
	AILogLogger["util_score_enemy_targets"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for score_enemy_targets)";
	Game::ListBuildings buildings = game->get_player_buildings(player);
	AILogLogger["util_score_enemy_targets"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex before calling game->get_player_buildings(player) (for score_enemy_targets)";
	game->get_mutex()->unlock();
	AILogLogger["util_score_enemy_targets"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex before calling game->get_player_buildings(player) (for score_enemy_targets)";
	std::set<MapPos> unique_enemy_targets;
	for (Building *building : buildings) {
		if (!building->is_done() ||
			!building->is_military() ||
			!building->is_active() ||
			!(building->get_threat_level() == 3)) {
			continue;
		}
		ai_mark_pos.clear();
		AILogLogger["util_score_enemy_targets"] << name << " looking for enemy buildings to attack near to my building of " << NameBuilding[building->get_type()] << " at pos " << building->get_position();
		MapPos attacker_pos = building->get_position();
		// score each enemy building in attackable radius (which is?? NEED TO FIND OUT) 
		//  at 14, seeing some unattackable targets, lowering to 13
		//  13 seems right so far 
		for (unsigned int i = 0; i < AI::spiral_dist(13); i++) {
			MapPos pos = map->pos_add_extended_spirally(attacker_pos, i);
			if (!map->has_building(pos) ||
				map->get_owner(pos) == player_index) {
				continue;
			}
			Building *target_building = game->get_building_at_pos(pos);
			if (target_building == nullptr) {
				AILogLogger["util_score_enemy_targets"] << name << " target_building is nullptr!  at pos " << pos;
				continue;
			}
			if (!target_building->is_military() || !target_building->is_active()) {
				continue;
			}
			MapPos target_pos = pos;
			Building::Type target_building_type = building->get_type();
			size_t target_player_index = map->get_owner(pos);
			const std::string target_player_face = NamePlayerFace[game->get_player(map->get_owner(pos))->get_face()];
			AILogLogger["util_score_enemy_targets"] << name << " found attackable building of type " << NameBuilding[target_building_type] << " at pos " << target_pos << " belonging to player " << target_player_index << " / " << target_player_face;
			// does this belong here?
			int max_knights = 0;
			// limit attacking knights count based on building type?  What purpose?  is this attacking or defending knights?
			//   is this copy & paste from original freeserf code?  Is this actually used by AI???
			switch (target_building_type) {
			case Building::TypeHut: max_knights = 3; break;
			case Building::TypeTower: max_knights = 6; break;
			case Building::TypeFortress: max_knights = 12; break;
			case Building::TypeCastle: max_knights = 20; break;
			default: NOT_REACHED(); break;
			}
			int attacking_knights = player->knights_available_for_attack(target_pos);
			attacking_knights = std::min(attacking_knights, max_knights);
			AILogLogger["util_score_enemy_targets"] << name << " send up to " << attacking_knights << " knights to attack enemy building of type " << NameBuilding[target_building_type]
				<< " at pos " << target_pos << " belonging to player " << target_player_index << " / " << target_player_face;
			if (attacking_knights == 0) {
				AILogLogger["util_score_enemy_targets"] << name << " cannot send any knights, not marking target for scoring";
				continue;
			}
			AILogLogger["util_score_enemy_targets"] << name << " adding target_pos " << target_pos << " to unique_enemy_targets set to score";
			unique_enemy_targets.insert(target_pos);

		}
	}
	scoring_attack = true;
	for (MapPos target_pos : unique_enemy_targets){
		AILogLogger["util_score_enemy_targets"] << name << " scoring attackable building at pos " << target_pos;
		// debug
		AILogLogger["util_score_enemy_targets"] << name << " dumping attackable debug expand_towards";
		for (std::string goal : last_expand_towards) {
			AILogLogger["util_score_enemy_targets"] << name << " attackable ATTACKING (last_) last_expand_towards goal list includes item: " << goal;
		}
		for (std::string goal : expand_towards) {
			AILogLogger["util_score_enemy_targets"] << name << " attackable ATTACKING (last_) expand_towards goal list includes item: " << goal;
		}
		unsigned int score = AI::score_area(target_pos, AI::spiral_dist(8));
		AILogLogger["util_score_enemy_targets"] << name << " attackable enemy target at pos " << target_pos << " has score " << score;
		scored_targets->insert(std::make_pair(target_pos, score));
	}
	// second part of stupid hack to work-around it
	expand_towards.clear();
	AILogLogger["util_score_enemy_targets"] << name << " done score_enemy_targets";
	duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;
	AILogLogger["util_score_enemy_targets"] << name << " done score_enemy_targets call took " << duration;
}



void
AI::attack_nearest_target(MapPosSet *scored_targets) {
	AILogLogger["util_attack_nearest_target"] << name << " inside AI::attack_nearest_target";
	for (std::pair<MapPos, unsigned int> target : *(scored_targets)) {
		MapPos target_pos = target.first;
		unsigned int target_score = target.second;
		AILogLogger["util_attack_nearest_target"] << name << " inside AI::attack_nearest_target, found target with target_pos " << target_pos << " and score " << target_score;
		int attacking_knights = player->knights_available_for_attack(target_pos);
		//AILogLogger["util_attack_nearest_target"] << name << " debug send up to " << attacking_knights << " knights to attack before max_knights";
		//int max_knights = 0;  // what is this?
		//attacking_knights = std::min(attacking_knights, max_knights);
		AILogLogger["util_attack_nearest_target"] << name << " send up to " << attacking_knights << " knights to attack enemy building at pos " << target_pos;
		AILogLogger["util_attack_nearest_target"] << name << " my military_score = " << player->get_military_score() << ", knight_morale = " << player->get_knight_morale();
		//AILogLogger["util_attack_nearest_target"] << name << " ENEMY military_score = " << game->get_player(target_player_index)->get_military_score() << ",  ENEMY knight_morale = " << game->get_player(target_player_index)->get_knight_morale();
		AILogLogger["util_attack_nearest_target"] << name << " my morale is " << player->get_knight_morale() << ", min_knight_morale_attack is " << min_knight_morale_attack;
		if (player->get_knight_morale() > min_knight_morale_attack) {
			AILogLogger["util_attack_nearest_target"] << name << " my morale " << player->get_knight_morale() << " is at least min_knight_morale_attack of " << min_knight_morale_attack;
			int defending_knights = 2;  // TEMP hardcoded
			double attack_ratio = double(attacking_knights) / double(defending_knights);
			AILogLogger["util_attack_nearest_target"] << name << " attacking_knights=" << attacking_knights << ", defending_knights=" << defending_knights << ", attack_ratio=" << attack_ratio;
			if (attack_ratio >= min_knight_ratio_attack) {
				AILogLogger["util_attack_nearest_target"] << name << " attack_ratio " << attack_ratio << " is >= to min_knight_ratio_attack " << min_knight_ratio_attack << ", PROCEEDING WITH THE ATTACK!";
				player->building_attacked = game->get_building_at_pos(target_pos)->get_index();
				player->attacking_building_count = attacking_knights;
				AILogLogger["util_attack_nearest_target"] << name << " calling player->start_attack()";
				player->start_attack();
				AILogLogger["util_attack_nearest_target"] << name << " DONE calling player->start_attack()";
			}
		}
	}
	AILogLogger["util_attack_nearest_target"] << name << " done AI::attack_nearest_target";
}
