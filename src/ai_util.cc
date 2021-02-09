/*
 * ai_util.cc - AI related functions
 *   Copyright 2019-2021 tlongstretch
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
  AILogDebug["ai_util"] << "spiral distance of rows " << distance << " is " << spiral_dist;
  return spiral_dist;
  */
  return _spiral_dist[distance];
}

// return true if *any* of the four points contain the requested terrain type
bool
AI::has_terrain_type(PGame game, MapPos pos, Map::Terrain res_start_index, Map::Terrain res_end_index) {
  Log::Verbose["util_has_terrain_type"] << " inside AI::has_terrain_type";
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
  AILogDebug["util_place_castle"] << name << " inside AI::place_castle, center_pos " << center_pos << ", distance " << distance;
  PMap map = game->get_map();
  unsigned int trees = 0;
  unsigned int stones = 0;
  unsigned int building_sites = 0;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    Map::Object obj = map->get_obj(pos);
    if (obj >= Map::ObjectTree0 && obj <= Map::ObjectPine7) {
      trees += 1;
      //AILogDebug["util_place_castle"] << name << " adding trees count 1";
    }
    if (obj >= Map::ObjectStone0 && obj <= Map::ObjectStone7) {
      int stonepile_value = 1 + (-1 * (obj - Map::ObjectStone7));
      stones += stonepile_value;
      //AILogDebug["util_place_castle"] << name << " adding stones count " << stonepile_value;
    }
    if (game->can_build_large(pos)) {
      building_sites += 3;
      //AILogDebug["util_place_castle"] << name << " adding large building value 3";
    }
    else if (game->can_build_small(pos)) {
      building_sites += 1;
      //AILogDebug["util_place_castle"] << name << " adding small building value 1";
    }
  }

  AILogDebug["util_place_castle"] << name << " found trees: " << trees << ", stones: " << stones << ", building_sites: " << building_sites << " in area " << center_pos;
  if (trees < (near_trees_min * 4)) {
    AILogDebug["util_place_castle"] << name << " not enough trees, min is " << near_trees_min * 4 << ", returning false";
    return false;
  }
  if (stones < near_stones_min) {
    AILogDebug["util_place_castle"] << name << " not enough stones, min is " << near_stones_min << ", returning false";
    return false;
  }
  if (building_sites < near_building_sites_min) {
    AILogDebug["util_place_castle"] << name << " not enough building_sites, min is " << near_building_sites_min << ", returning false";
    return false;
  }
  AILogDebug["util_place_castle"] << name << " center_pos: " << center_pos << " is an acceptable building site for a castle";
  AILogDebug["util_place_castle"] << name << " done AI::place_castle";
  return true;
}


// update current AI player's inventory of buildings of all types for lookup by various functions
void
AI::update_building_counts() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();

  AILogDebug["util_update_building_counts"] << name << " inside AI::update_building_counts";
  AILogDebug["util_update_building_counts"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex inside AI::update_building_counts";
  game->get_mutex()->lock();
  AILogDebug["util_update_building_counts"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex inside AI::update_building_counts";
  Game::ListBuildings buildings = game->get_player_buildings(player);
  AILogDebug["util_update_building_counts"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex inside AI::update_building_counts";
  game->get_mutex()->unlock();
  AILogDebug["util_update_building_counts"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex inside AI::update_building_counts";
  // reset all to zero
  memset(realm_building_count, 0, sizeof(realm_building_count));
  memset(realm_completed_building_count, 0, sizeof(realm_completed_building_count));
  //memset(realm_incomplete_building_count, 0, sizeof(incomplete_building_count));
  memset(realm_occupied_building_count, 0, sizeof(realm_occupied_building_count));
  memset(realm_connected_building_count, 0, sizeof(realm_connected_building_count));
  for (MapPos inventory_pos : stocks_pos) {
    AILogDebug["util_update_building_counts"] << name << " inside AI::update_building_counts, clearing counts for inventory_pos " << inventory_pos;
    memset(stock_buildings.at(inventory_pos).count, 0, sizeof(stock_buildings.at(inventory_pos).count));
    memset(stock_buildings.at(inventory_pos).completed_count, 0, sizeof(stock_buildings.at(inventory_pos).completed_count));
    memset(stock_buildings.at(inventory_pos).occupied_count, 0, sizeof(stock_buildings.at(inventory_pos).occupied_count));
    memset(stock_buildings.at(inventory_pos).connected_count, 0, sizeof(stock_buildings.at(inventory_pos).connected_count));
    stock_buildings.at(inventory_pos).occupied_military_pos.clear();
    stock_buildings.at(inventory_pos).unfinished_count = 0;
    stock_buildings.at(inventory_pos).unfinished_hut_count = 0;
    AILogDebug["util_update_building_counts"] << name << " RESET unfinished_hut_count for inventory_pos " << inventory_pos << ", is now: " << stock_buildings.at(inventory_pos).unfinished_hut_count;
    AILogDebug["util_update_building_counts"] << name << " RESET unfinished_building_count, for inventory_pos " << inventory_pos << ", is now: " << stock_buildings.at(inventory_pos).unfinished_count;
  }

  // now that a flagsearch is being used to find the nearest Inventory rather than straight-line
  //  distance, it might be possible to optimize this process by searching outward from each
  //  inventory once?  or caching results or something like that?  
  //  only bother if this function is taking a significant amount of time
  //  it does get called a lot, though
  for (Building *building : buildings) {

    if (building == nullptr)
      continue;
    if (building->is_burning())
      continue;
    
    Building::Type type = building->get_type();
	  if (type == Building::TypeNone) {
		  AILogDebug["util_update_building_counts"] << name << " has a building of TypeNone!  why does this happen??";
		  continue;
	  }

	  MapPos pos = building->get_position();
	  MapPos flag_pos = map->move_down_right(building->get_position());
	  AILogDebug["util_update_building_counts"] << name << " has a building of type " << NameBuilding[type] << " at pos " << pos << ", with flag_pos " << flag_pos;

      if (type == Building::TypeCastle) {
        if (!building->is_done()) {
          AILogDebug["util_update_building_counts"] << name << "'s castle isn't finished building yet";
          while (!building->is_done()) {
            AILogDebug["util_update_building_counts"] << name << "'s castle isn't finished building yet.  Sleeping a bit";
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
          }
          AILogDebug["util_update_building_counts"] << name << "'s castle is now built, updating stocks";
          // does the stocks_pos logic rely on the Castle always being the first building?  does it matter?
          update_stocks_pos();
        }
        realm_occupied_military_pos.push_back(flag_pos);
        stock_buildings.at(flag_pos).occupied_military_pos.push_back(flag_pos);
        continue;
      }
    
      // skip completed Stocks, they are tracked elsewhere
      if (type == Building::TypeStock && building->is_done())
        continue;

	  // for military buildings, track the nearest Inventory by straight-line distance
	  //  as these are used for general selection of nearby huts when looking for things
	  //  in a certain area around an Inventory and its borders, duplicates are allowed.
	  if (building->is_military() && building->is_done() && building->is_active()) {
		  AILogDebug["util_update_building_counts"] << name << " adding occupied military building at " << pos << " to realm_occupied_military_pos list";
		  realm_occupied_military_pos.push_back(flag_pos);
		  AILogDebug["util_update_building_counts"] << name << " about to call find_nearest_inventories_to_military_building for connected building of type " << NameBuilding[type] << " at pos " << pos << ", with flag_pos " << flag_pos;
		  for (MapPos inv_flag_pos : find_nearest_inventories_to_military_building(flag_pos)) {
			  AILogDebug["util_update_building_counts"] << name << " adding occupied military building at " << pos << " to stock_buildings.at(" << inv_flag_pos << ")";
			  stock_buildings.at(inv_flag_pos).occupied_military_pos.push_back(flag_pos);
		  }
		  // don't quit here, we still want to try to do a FlagSearch for military buildings so
		  //  if they are incomplete, we can associate the unfinished building to an Inventory
		  //  because the builder and building material source will come via FlagSearch. 
		  // It might not be the closest-by-straightline-dist Inventory that is used
		  //  to track occupied military buildings for border determination reasons
	  }

	  // skip disconnected buildings, they cannot complete a FlagSearch to find nearest Inventory
	  if (!game->get_flag_at_pos(flag_pos)->is_connected()) {
		  AILogDebug["util_update_building_counts"] << name << " building of type " << NameBuilding[type] << " at pos " << pos << ", with flag_pos " << flag_pos << " has no paths, cannot do FlagSearch, skipping";
		  continue;
	  }

	  // do the FlagSearch
	  AILogDebug["util_update_building_counts"] << name << " about to call find_nearest_inventory for connected building of type " << NameBuilding[type] << " at pos " << pos << ", with flag_pos " << flag_pos;
	  MapPos inventory_pos = find_nearest_inventory(map, player_index, building->get_position(), DistType::FlagOnly, &ai_mark_pos);
	  if (inventory_pos == bad_map_pos) {
		  AILogDebug["util_update_building_counts"] << name << " find_nearest_inventory call for building at pos " << pos << ", with flag_pos " << flag_pos << " returned bad_map_pos, skipping this building";
		  continue;
	  }
	  AILogDebug["util_update_building_counts"] << name << " nearest Inventory (by flagsearch) to this connected building is " << inventory_pos;

	  // count incomplete buildings (so AI can limit the number of outstanding unfinished buildings)
    if (!building->is_done()) {
      if (type == Building::TypeHut) {
        stock_buildings.at(inventory_pos).unfinished_hut_count++;
        AILogDebug["util_update_building_counts"] << name << " incrementing unfinished_hut_count for inventory_pos " << inventory_pos << ", is now: " << stock_buildings.at(inventory_pos).unfinished_hut_count;
      }
      else if (building->get_type() == Building::TypeCoalMine
        || building->get_type() == Building::TypeIronMine
        || building->get_type() == Building::TypeGoldMine
        || building->get_type() == Building::TypeStoneMine) {
        AILogDebug["util_update_building_counts"] << name << " unfinished building is a Mine, not incrementing unfinished_building_count";
      }
      else {
        stock_buildings.at(inventory_pos).unfinished_count++;
        AILogDebug["util_update_building_counts"] << name << " found unfinished building of type " << NameBuilding[type] << ", incrementing unfinished_building_count for inventory_pos " << inventory_pos << ", is now: " << stock_buildings.at(inventory_pos).unfinished_count;
      }
    }
    
    // should this exclude military buildings to avoid confusion?
    //  leaving it for now, the count of military buildings here
    //  is based on FlagSearch dist, NOT straight-line dist as
    //  is used for OCCUPIED_military_buildings counts
	  realm_building_count[type]++;   // move this to before the FlagSearch so it has a chance to include buildings that are not connected?  nah, leave as-is for now
    stock_buildings.at(inventory_pos).count[type]++;
    // the difference between 'buildings' and 'connected_buildings' is now moot because the nearest inventory pos
    //  is now determined by FlagSearch which requires that they be connected already to do the search
    //  Should probably remove the 'building_count' entirely.  See if any of this is even used anymore, I don't think it is
	  realm_connected_building_count[type]++;
    stock_buildings.at(inventory_pos).connected_count[type]++;
    if (building->is_done()){
	    realm_completed_building_count[type]++;
      stock_buildings.at(inventory_pos).completed_count[type]++;
      // has_serf is not a good enough test alone to see if occupied, as it seems to be true when a builder is constructing the building!
      //  so moved this check to inside building->is_done because if building is done the only serf there should be the professional (I think)
      if (building->has_serf()) {
        // this will include military buildings occupied by knights, also.  Which I guess is fine
        //  however, again the counts may not match the occupied_military_building counts
        //  which are determined by straight-line distance only
	      realm_occupied_building_count[type]++;
        stock_buildings.at(inventory_pos).occupied_count[type]++;
      }
    }
  } // foreach Building : get_player_buildings

  // debug, dump all inventory-to-building counts
  for (MapPos inventory_pos : stocks_pos) {
    // this skips 'connected', which is moot anyway
    AILogDebug["util_update_building_counts"] << name << " Inventory at pos " << inventory_pos << " has all/completed/occupied buildings: ";
    int type = 0;
    // why is this using realm_building_count instead of ... Types 0-25?
    for (int count : realm_building_count) {
      AILogDebug["util_update_building_counts"] << name << " type " << type << " / " << NameBuilding[type] << ": " << stock_buildings.at(inventory_pos).count[type]
        << "/" << stock_buildings.at(inventory_pos).completed_count[type] << "/" << stock_buildings.at(inventory_pos).occupied_count[type];
      type++;
    }
  }

  //AILogDebug["util_update_building_counts"] << name << " done AI::update_building_counts";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_update_building_counts"] << name << " done AI::update_building_counts call took " << duration;
}


// update current AI player's vector of built warehouse/stock positions
//   INCLUDING the castle.  this should be the FLAG of the stock, not the stock itself
void
AI::update_stocks_pos() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();

  AILogDebug["util_update_stocks_pos"] << name << " inside AI::update_stocks_pos";
  AILogDebug["util_update_stocks_pos"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex inside AI::update_stocks_pos";
  game->get_mutex()->lock();
  AILogDebug["util_update_stocks_pos"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex inside AI::update_stocks_pos";
  Game::ListBuildings buildings = game->get_player_buildings(player);
  AILogDebug["util_update_stocks_pos"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex inside AI::update_stocks_pos";
  game->get_mutex()->unlock();
  AILogDebug["util_update_stocks_pos"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex inside AI::update_stocks_pos";
  stocks_pos = {};
  for (Building *building : buildings) {
    if (building == nullptr)
      continue;
    if (building->is_burning())
      continue;
    Building::Type type = building->get_type();
    if (type != Building::TypeCastle && type != Building::TypeStock)
      continue;
    if (type == Building::TypeStock){
      if (!building->is_done())
        continue;
      if (!building->has_serf())
        continue;
      if (!game->get_flag(building->get_flag_index())->is_connected())
        continue;
    }
    MapPos stock_flag_pos = map->move_down_right(building->get_position());
    if (type == Building::TypeCastle) {
      AILogDebug["util_update_stocks_pos"] << name << " the castle was found at pos " << building->get_position() << ", with its flag at pos " << stock_flag_pos;
    }else {
      AILogDebug["util_update_stocks_pos"] << name << " a completed, connected, serf-occupied warehouse/stock was found at pos " << building->get_position() << ", with its flag at pos " << stock_flag_pos;
    }
    stocks_pos.push_back(stock_flag_pos);
    //stock_buildings[stock_flag_pos];
    /*
    struct StockBuildings {
      int count[25] = { 0 };
      int connected_count[25] = { 0 };
      int completed_count[25] = { 0 };
      int occupied_count[25] = { 0 };
      int unfinished_count;
      int unfinished_hut_count;
      MapPosVector occupied_military_pos;
    };*/
    stock_buildings[stock_flag_pos] = { {0},{0},{0},{0},0,0,{} };
  }
  AILogDebug["util_update_stocks_pos"] << name << " done AI::update_stocks_pos";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_update_stocks_pos"] << name << " done AI::update_stocks_pos call took " << duration;
}


// destroy all roads in realm, and all disconnected flags, then reconnect them in priority order
//   attempt to optimize roads once economy is complete.  not working correctly yet
//     this is a test/hack and NOT used normally (or ever right now)
void
AI::rebuild_all_roads() {
  AILogDebug["util_rebuild_all_roads"] << name << " inside AI::rebuild_all_roads";
  MapPosVector flag_positions;
  AILogDebug["util_rebuild_all_roads"] << name << " destroying all paths";
  AILogDebug["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex for entire rebuild_all_roads function before destroying all roads";
  game->get_mutex()->lock();
  AILogDebug["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex for entire rebuild_all_roads function before destroying all roads";
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
  AILogDebug["util_rebuild_all_roads"] << name << " destroying all unattached flags";
  for (MapPos flag_pos : flag_positions) {
    if (map->has_flag(flag_pos))
      game->demolish_flag(flag_pos, player);
  }

  AILogDebug["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex for entire rebuild_all_roads function after destroying all roads";
  game->get_mutex()->unlock();
  AILogDebug["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex for entire rebuild_all_roads function after destroying all roads";

  AILogDebug["util_rebuild_all_roads"] << name << "sleeping to see roads destroyed before starting rebuild";
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

  AILogDebug["util_rebuild_all_roads"] << name << " trying to connect high priority buildings first";
  //AILogDebug["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex for entire rebuild_all_roads function before rebuilding high-priority building roads";
  //game->get_mutex()->lock();
  //AILogDebug["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex for entire rebuild_all_roads function before rebuilding high-priority building roads";
  for (Building::Type type : rebuild_order) {
    AILogDebug["util_rebuild_all_roads"] << name << " trying to connect buildings of type " << NameBuilding[type];
    for (Building *building : game->get_player_buildings(player)) {
      if (building->get_type() != type) {
        continue;
      }
      if (building->is_burning()) {
        AILogDebug["util_rebuild_all_roads"] << name << " this building is on fire!  skipping";
        continue;
      }
      // skip mines that aren't yet built
      if (!building->is_done() && building->get_type() >= Building::TypeStoneMine && building->get_type() <= Building::TypeGoldMine)
        continue;
      ai_mark_pos.insert(ColorDot(building->get_position(), "blue"));
      if (!AI::build_best_road(map->move_down_right(building->get_position()), road_options)) {
        AILogDebug["util_rebuild_all_roads"] << name << " failed to connect high priority building at pos " << building->get_position() << " to affinity building / road network!";
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      //ai_mark_pos.clear();
      ai_mark_pos.erase(building->get_position());

    }
  }

  //game->get_mutex()->unlock();
  //std::this_thread::sleep_for(std::chrono::milliseconds(5000));
  //game->get_mutex()->lock();
  AILogDebug["util_rebuild_all_roads"] << name << " trying to connect bakers to mines";
  for (Building *building : game->get_player_buildings(player)) {
    if (building->get_type() != Building::TypeBaker) {
      continue;
    }
    if (building->is_burning()) {
      AILogDebug["util_rebuild_all_roads"] << name << " this building is on fire!  skipping";
      continue;
    }
    AILogDebug["util_rebuild_all_roads"] << name << " found a baker at pos " << map->move_down_right(building->get_position());
    ai_mark_pos.insert(ColorDot(building->get_position(), "orange"));
    if (!AI::build_best_road(map->move_down_right(building->get_position()), road_options, Building::TypeCoalMine)) {
      AILogDebug["util_rebuild_all_roads"] << name << " failed to connect baker at pos " << building->get_position() << " to coal mine / road network!";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ai_mark_pos.clear();
    if (!AI::build_best_road(map->move_down_right(building->get_position()), road_options, Building::TypeIronMine)) {
      AILogDebug["util_rebuild_all_roads"] << name << " failed to connect baker at pos " << building->get_position() << " to iron mine / road network!";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ai_mark_pos.clear();
    if (!AI::build_best_road(map->move_down_right(building->get_position()), road_options, Building::TypeGoldMine)) {
      AILogDebug["util_rebuild_all_roads"] << name << " failed to connect baker at pos " << building->get_position() << " to gold mine / road network!";
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

  AILogDebug["util_rebuild_all_roads"] << name << " trying to connect 2nd run high priority buildings";
  //AILogDebug["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex for entire rebuild_all_roads function before rebuilding high-priority building roads";
  //game->get_mutex()->lock();
  //AILogDebug["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex for entire rebuild_all_roads function before rebuilding high-priority building roads";
  for (Building::Type type : rebuild_order2) {
    AILogDebug["util_rebuild_all_roads"] << name << " trying to connect buildings of type " << NameBuilding[type];
    for (Building *building : game->get_player_buildings(player)) {
      if (building->get_type() != type) {
        continue;
      }
      if (building->is_burning()) {
        AILogDebug["util_rebuild_all_roads"] << name << " this building is on fire!  skipping";
        continue;
      }
      // skip mines that aren't yet built
      if (!building->is_done() && building->get_type() >= Building::TypeStoneMine && building->get_type() <= Building::TypeGoldMine)
        continue;
      ai_mark_pos.insert(ColorDot(building->get_position(), "purple"));
      if (!AI::build_best_road(map->move_down_right(building->get_position()), road_options)) {
        AILogDebug["util_rebuild_all_roads"] << name << " failed to connect high priority building at pos " << building->get_position() << " to affinity building / road network!";
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      //ai_mark_pos.clear();
      ai_mark_pos.erase(building->get_position());

    }
  }


  //game->get_mutex()->unlock();
  //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  //game->get_mutex()->lock();
  //AILogDebug["util_rebuild_all_roads"] << name << " breaking early to debug";
  //return;

  road_options.set(RoadOption::PenalizeNewLength);
  road_options.reset(RoadOption::ReducedNewLengthPenalty);

  AILogDebug["util_rebuild_all_roads"] << name << " trying to connect remaining civilian buildings (except rangers and incomplete mines)";
  for (Building *building : game->get_player_buildings(player)) {
    if (building->is_military())
      continue;
    if (building->get_type() == Building::TypeForester && building->is_done())
      continue;
    if (building->is_burning()) {
      AILogDebug["util_rebuild_all_roads"] << name << " this civilian building is on fire!  skipping";
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
      AILogDebug["util_rebuild_all_roads"] << name << " failed to connect civilian building at pos " << building->get_position() << " to road network!";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    //ai_mark_pos.clear();
    //ai_mark_pos.erase(building->get_position());
  }
  //game->get_mutex()->unlock();
  //std::this_thread::sleep_for(std::chrono::milliseconds(5000));
  //game->get_mutex()->lock();
  AILogDebug["util_rebuild_all_roads"] << name << " trying to connect military buildings";
  for (Building *building : game->get_player_buildings(player)) {
    if (building->is_military()) {
      if (building->is_burning()) {
        AILogDebug["util_rebuild_all_roads"] << name << " this military building is on fire!  skipping";
        continue;
      }
      ai_mark_pos.insert(ColorDot(building->get_position(), "coral"));
      if (!AI::build_best_road(map->move_down_right(building->get_position()), road_options)) {
        AILogDebug["util_rebuild_all_roads"] << name << " failed to connect military building at pos " << building->get_position() << " to road network!";
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      //ai_mark_pos.clear();
    }
  }
  //AILogDebug["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex for entire rebuild_all_roads function";
  //game->get_mutex()->unlock();
  //AILogDebug["util_rebuild_all_roads"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex for entire rebuild_all_roads function";
}


// try to build a road from the specified flag pos to the best end flag, return success/failure
//  If any roads already exist from specified flag to each affinity building,
//    build a better road if one is found.  Do not remove the old one
bool
AI::build_best_road(MapPos start_pos, RoadOptions road_options, Building::Type optional_building_type, Building::Type optional_affinity, MapPos optional_target, bool verify_stock) {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();

  AILogDebug["util_build_best_road"] << name << " inside AI::build_best_road with start pos " << start_pos << ", optional_building_type " << NameBuilding[optional_building_type] << ", optional_affinity " << NameBuilding[optional_affinity] << ", optional_target " << optional_target;
  // print RoadOptions for debugging
  for (unsigned int i = 0; i < road_options.size(); i++) {
    AILogDebug["util_build_best_road"] << name << " RoadOption: " << NameRoadOption[i] << " = " << bool(road_options.test(i));
  }

  // sanity check request
  if (map->get_owner(start_pos) != player_index) {
    AILogDebug["util_build_best_road"] << name << " start_pos " << start_pos << " is not owned by Player" << player_index << "!  returning false";
    return false;
  }
  
  if (map->has_flag(start_pos)) {
    // check if this start_flag already has any paths
    if (game->get_flag_at_pos(start_pos)->is_connected()) {
      AILogDebug["util_build_best_road"] << name << " at least one path already exists from flag at start_pos";
      // do not cancel if this condition happens, because it is normal when doing rebuild_all_roads
      //if (!road_options.test(RoadOption::Improve)) {
      //  AILogDebug["util_build_best_road"] << name << " a path already exists from start_pos " << start_pos << " but RoadOption::Improve is false!  this is unexpected.  returning false";
      //  return false;
      //}
      if (!road_options.test(RoadOption::Direct)) {
        AILogDebug["util_build_best_road"] << name << " build_best_road will try to build a better road than the current best one";
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
        // hmm.. considering changing this to return true because a path already exists... BUT I think all build_road
        //  calls should be first preceded by a flag->is_connected check first
        AILogDebug["util_build_best_road"] << name << "no more paths can be build from this building's flag!  returning false";
        return false;
      }
    }
    else {
      AILogDebug["util_build_best_road"] << name << " start_pos has no paths, will try to connect it to road network";
    }
  }else{
    // changing this to support plotting best road BEFORE building a flag & building
    //AILogDebug["util_build_best_road"] << name << " No flag exists at start_pos " << start_pos << "!  returning false";
    //return false;
    AILogDebug["util_build_best_road"] << name << " no flag exists at start_pos " << start_pos << ", this must be a pre-building check, continuing";
  }

  // if an optional_building_type is set, this must be a pre-building road
  //  so we must enable HoldBuildingPos to ensure the road doesn't block the planned building
  if (optional_building_type != Building::TypeNone){
    AILogDebug["util_build_best_road"] << name << " optional_building_type is set, this must be a pre-building road.  Setting RoadOption::HoldbuildingPos to true";
    road_options.set(RoadOption::HoldBuildingPos);
  }

  // check BuildingAffinity table to see if the start_pos is attached to a building that should prioritize connection to another existing building type
  //  if there is no affinity the default target is the player's castle or nearest stock

  MapPosVector targets;  // these are flag positions

  //if (optional_affinity != Building::TypeNone) {

    /* this whole section is redundant, isn't it?  because optional_building_type
    //  is now an option to get_affinity, so it already happens

    // for rebuild all roads (at least), override affinity table and instead use the specified affinity building type
    AILogDebug["util_build_best_road"] << name << " using optional_affinity " << NameBuilding[optional_affinity] << " specified in build_best_road call";
    // ************************** **************************
    // make this a function, also do same inside AI::get_affinity it is repetative copy/paste
    // ************************** **************************
    AILogDebug["util_build_best_road"] << name << " looking for nearest connected building of type " << NameBuilding[optional_affinity];
    Building *building = AI::find_nearest_building(start_pos, CompletionLevel::Connected, optional_affinity, 15);
    if (building != nullptr) {
      AILogDebug["util_build_best_road"] << name << " found connected optional_affinity building at pos " << building->get_position();
      // get the flag position of the building
      MapPos building_flag_pos = map->move_down_right(building->get_position());
      AILogDebug["util_build_best_road"] << name << " setting road_to target to building_flag_pos " << building_flag_pos;
      targets.push_back(building_flag_pos);
    }
    else {
      AILogDebug["util_build_best_road"] << name << " couldn't find any connected optional_affinity building nearby, checking entire realm";
      bool found = false;
      AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for optional_affinity)";
      game->get_mutex()->lock();
      AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for optional_affinity)";
      Game::ListBuildings buildings = game->get_player_buildings(player);
      AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player) (for optional_affinity)";
      game->get_mutex()->unlock();
      AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player) (for optional_affinity)";
      for (Building *building : buildings) {
        if (building->get_type() != optional_affinity)
          continue;
        if (building->is_burning())
          continue;
        MapPos building_flag_pos = map->move_down_right(building->get_position());
        if (!map->has_flag(building_flag_pos))
          continue;
        if (!game->get_flag_at_pos(building_flag_pos)->is_connected())
          continue;
        AILogDebug["util_build_best_road"] << name << " found connected one with flag pos " << building_flag_pos;
        // allow connecting to disconnected flag for optional_affinity, this is so far only used by rebuild_all_buildings
        //if (!game->get_flag_at_pos(first_building_flag_pos)->is_connected()) {
        //  AILogDebug["util_build_best_road"] << name << " affinity building flag is not connected!  skipping";
        //}
        //else {
          AILogDebug["util_build_best_road"] << name << " setting road_to target to affinity building_flag_pos " << building_flag_pos;
          targets.push_back(building_flag_pos);
          found = true;
          break;
        //}
      }
      if (!found) {
        AILogDebug["util_build_best_road"] << name << " could not find any connected building of specified optional_affinity type, returning false!";
        return false;
      }
    }
  }
  */
  //else if (optional_target != bad_map_pos) {
  if (optional_target != bad_map_pos) {
    // only for specific road improvements, target a specific flag or building pos rather than "connect to road system"
    AILogDebug["util_build_best_road"] << name << " using optional_target pos " << optional_target << " specified in build_best_road call";
    // check to if target is a building or a flag
    if (map->has_flag(optional_target)) {
      AILogDebug["util_build_best_road"] << name << " optional_target pos " << optional_target << " is a flag.  checking to see if it has an attached building";
      // this is only for debugging
      if (map->has_building(map->move_up_left(optional_target))) {
        AILogDebug["util_build_best_road"] << name << " flag at optional_target pos " << optional_target << " has an attached building of type " << NameBuilding[game->get_building_at_pos(map->move_up_left(optional_target))->get_type()];
      }
      AILogDebug["util_build_best_road"] << name << " setting optional_target flag pos " << optional_target << " as road_to target";
      targets.push_back(optional_target);
    }
    else if (map->has_building(optional_target)) {
      AILogDebug["util_build_best_road"] << name << " optional_target pos " << optional_target << " is a building of type " << NameBuilding[game->get_building_at_pos(optional_target)->get_type()];
      AILogDebug["util_build_best_road"] << name << " setting optional_target pos " << optional_target << " building's flag at pos " << map->move_down_right(optional_target) << " as road_to target";
      targets.push_back(map->move_up_left(optional_target));
    }
    else {
      AILogDebug["util_build_best_road"] << name << " optional_target pos has neither a building nor flag!  this is unexpected, returning false";
      return false;
    }
  }
  else {
    // most common - use the building affinity table to figure out what to connect to
    targets = AI::get_affinity(start_pos, optional_building_type);
    AILogDebug["util_build_best_road"] << name << " targets contains " << targets.size() << " affinity / target buildings";
    // if no targets found, fall back to nearest or selected inv
    if (targets.size() == 0){
      AILogDebug["util_build_best_road"] << name << " no valid target found from get_affinity, trying to find a nearest connected inventory";
      MapPos fallback_inv_pos = bad_map_pos;
      if (map->has_flag(start_pos) && map->has_building(map->move_up_left(start_pos))){
        MapPos nearest_inv_pos = find_nearest_inventory(map, player_index, start_pos, DistType::FlagAndStraightLine, &ai_mark_pos);
        if (nearest_inv_pos != bad_map_pos){
          AILogDebug["util_build_best_road"] << name << " no valid target found, setting target to nearest_inv_pos " << nearest_inv_pos;
          fallback_inv_pos = nearest_inv_pos;
        }
      }
      if (fallback_inv_pos == bad_map_pos){
        fallback_inv_pos = inventory_pos;
        AILogDebug["util_build_best_road"] << name << " no valid target found and could not find nearest_inv_pos, setting target to current inventory_pos " << inventory_pos;
      }
      targets.push_back(fallback_inv_pos);
    }
  }

  unsigned int roads_built = 0;
  unsigned int target_count = static_cast<unsigned int>(targets.size());
  //
  // change this to its own function so there isn't a foreach loop for hundreds of lines to handle two targets
  //
  //
  // CONSIDER MOVING THE get_affinity CALL TO INSIDE THIS FUNCTION SO THAT IT CAN TRY A SERIES OF AFFINITY BUILDINGS
  //  INSTEAD OF JUST THE CLOSEST ONE
  //

  int target_num = 0;  // TEMP UNTIL REVAMP
  for (MapPos target_pos : targets) {
    //
    // TEMP UNTIL REVAMP
    //
    target_num++;
    if (target_num > 2){
      AILogDebug["util_build_best_road"] << name << " TEMPORARY -  NOT ALLOWING MORE THAN 2 TARGETS - breaking";
      break;
    }
    if (target_pos == bad_map_pos) {
      AILogDebug["util_build_best_road"] << name << " no more affinity targets left";
      break;
    }
    AILogDebug["util_build_best_road"] << name << " considering target_pos " << target_pos;
    AILogDebug["util_build_best_road"] << name << " plotting road to connect flag at pos " << start_pos << " to target_pos " << target_pos << " via road network";
    Roads split_roads;
    //
    // ##   Direct   ## roads are simple A->B connection from start_pos to target_pos of FIRST affinity building, no second roads
    //
    if (road_options.test(RoadOption::Direct)) {
      AILogDebug["util_build_best_road"] << name << " direct road requested, trying to build direct road from flag at " << start_pos << " to flag at " << target_pos;
      // Just build a single direct road
      //   RoadOption::Improve is ignored, any existing road will not be compared.
      // split_roads list is not actually used for direct roads.  It is required/included but ignored
      // nearest stock check is ignored also
      // HoldBuildingPos ignored
      Road proposed_direct_road = plot_road(map, player_index, start_pos, target_pos, &split_roads);
      AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->build_road (direct road)";
      game->get_mutex()->lock();
      AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->build_road (direct road)";
      bool was_built = game->build_road(proposed_direct_road, player);
      AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->build_road (direct road)";
      game->get_mutex()->unlock();
      AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->build_road (direct road)";
      if (was_built) {
        AILogDebug["util_build_best_road"] << name << " successfully built direct road directly from flag at " << start_pos << " to flag at " << target_pos;
        //roads_built++;
        return true;
      }
      else {
        // should have a series of fallback plans if a flag cannot be connected?   NO
        AILogDebug["util_build_best_road"] << name << " failed to connect specified flag to road system! returning false";
        return false;
      }
    }

    //
    // ## non-Direct ## roads terminate at best scoring acceptable end_pos flag, which could be a direct route to the target flag or join an existing road
    //
    AILogDebug["util_build_best_road"] << name << " non-direct road requested, trying to connect flag at pos " << start_pos << " to road network using scoring system";
    RoadBuilder rb;
    rb.set_start_pos(start_pos);
    rb.set_target_pos(target_pos);
    MapPosVector nearby_flags;

    // if RoadOption::Improve is set and any paths already exist from start_pos, trace these roads and save for later scoring
    //   NOTE - if road caching disabled, and Improve not set, eroads aren't even used by roadbuilding code, only proads which are directly scored
    //     as eroads are traced each time they are considered
    if (map->has_flag(start_pos)){
      if (game->get_flag_at_pos(start_pos)->is_connected() && road_options.test(RoadOption::Improve)) {
        AILogDebug["util_build_best_road"] << name << " finding existing roads from start_pos " << start_pos << " for potential improvement";
        for (Direction dir : cycle_directions_cw()) {
          if (!map->has_path(start_pos, dir)) {
            continue;
          }
          AILogDebug["util_build_best_road"] << name << " found path from start_pos " << start_pos << " in dir " << NameDirection[dir];
          Road existing_road = trace_existing_road(map, start_pos, dir);
          RoadEnds ends = get_roadends(map, existing_road);
          rb.new_eroad(ends, existing_road);
          // add the other end flag to nearby_flags now to ensure it is scored
          MapPos end_pos = std::get<2>(ends);
          nearby_flags.push_back(end_pos);
        }
        AILogDebug["util_build_best_road"] << name << " done finding existing roads from start_pos " << start_pos;
      }
    }else{
      // should this return, or throw an exception maybe?  It seems wrong to try to Improve a nonexistent road, but I guess it is possible
      //  to set Improve to true for all road builds for other reasons?
      AILogWarn["util_build_best_road"] << name << " no flag exists at start_pos " << start_pos << " but RoadOption::Improve is set, this must be a pre-building check, continuing";
    }

    // compile list of potential end flag MapPos to connect to (no Roads yet)
    //   only consider flags in a radius of the source, to avoid trying to connect to faraway flags
    // IMPROVEMENT - determine the halfway point between the start_pos and target_pos, and center the radius there instead of using start_pos
    //        potential bug, if the target_pos is really far away from the start_pos, it might not find anything... that seems unlikely though.
    //         ...and that should happen even with the original method anyway so this isn't worse
    //          could consider a "tunnel" of multiple radiuses based on distance from start to target, but don't feel like coding that now
    AILogDebug["util_build_best_road"] << name << " finding halfway point between start_pos and target_pos to center nearby_flag search";
    MapPos halfway_pos = get_halfway_pos(start_pos, target_pos);
    // for rebuild_all_roads, and possibly other reasons, always push the target_pos onto the nearby_flags list
    AILogDebug["util_build_best_road"] << name << " as a fallback, adding target_pos " << target_pos << " to nearby_flags list";
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
        AILogDebug["util_build_best_road"] << name << " flag at pos " << pos << " is the same as start flag, skipping";
        continue;
      }
      // avoid disconnected flags, unless they are the flag of the target building
      // NOTE that this isn't smart enough to ignore flags that have some paths but are overall disconnected from the road system!
      //  this is a real bug that will need fixing otherwise disconnected roads will never be rejoined to road network
      /*
      if (!game->get_flag_at_pos(pos)->is_connected() && pos != castle_flag_pos && pos != target_pos) {
        AILogDebug["util_build_best_road"] << name << " flag at pos " << pos << " is itself disconnected and is neither the castle_flag_pos nor the target_pos, skipping";
        continue;
      }*/
      // removing "allow target_pos" exception, it causes problems and is only used for rebuild_all_roads which doesn't work well
      if (!game->get_flag_at_pos(pos)->is_connected() && pos != castle_flag_pos) {
        AILogDebug["util_build_best_road"] << name << " flag at pos " << pos << " is itself disconnected and is not the castle_flag_pos, skipping";
        continue;
      }

      AILogDebug["util_build_best_road"] << name << " found valid nearby_flag at pos " << pos;
      if (std::find(nearby_flags.begin(), nearby_flags.end(), pos) != nearby_flags.end()) {
        // skip if already in nearby_flags list, which should only happen if it is part of existing road and RoadOption::Improve roads set;
        AILogDebug["util_build_best_road"] << name << " this pos is already in nearby_flag list, skipping";
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
          AILogDebug["util_build_best_road"] << name << " at least one flag found within 6 tiles of halfway_pos, quitting search";
          break;
        }
        else {
          AILogDebug["util_build_best_road"] << name << " no nearby flag found within 6 tiles of halfway_pos!  continuing expanded search until one found";
        }
      }
    }
    ai_mark_pos.clear();

    AILogDebug["util_build_best_road"] << name << " found " << nearby_flags.size() << " nearby_flags";
    if (nearby_flags.size() < 1) {
      AILogDebug["util_build_best_road"] << name << " no nearby_flags found!  maybe castle_flag should always be added as a last resort...";
      AILogDebug["util_build_best_road"] << name << " because no nearby_flags found, no roads can be between start_pos " << start_pos << " and target_pos " << target_pos;
      // it seems likely that if the first target can't be reached, any second affinity target can't either
      //  likely this flag can't be connected to the road system at all now.  Just quit here
      return false;
    }

    //
    // plot Roads to the found nearby flags ends and score them in terms of NEW LENGTH
    //    store any that are "good enough" (under the max_convolution limit)
    //      quit plotting roads once max_plotting_steps_per_target limit is reached,     NOT IMPLEMENTED YET
    //     which is the aggregate tile_dist of ALL plotted roads so far to this target  NOT IMPLEMENTED YET
    //        this is used to limit the amount of time spent plotting roads as realm becomes crowded    NOT IMPLEMENTED YET
    //
    // use straight-line-distance to the actual target_pos as the measuring stick to judge how convoluted actual roads are be in comparison
    AILogDebug["util_build_best_road"] << name << " checking tile_dist from start_pos " << start_pos << " to end_pos " << target_pos << " for an ideal road";
    int ideal_length = AI::get_straightline_tile_dist(map, start_pos, target_pos);
    AILogDebug["util_build_best_road"] << name << " flag target_pos at " << target_pos << " has straight-line tile distance " << ideal_length << " from start_pos " << start_pos;
    // for each nearby_flag:
    //   - if this is "tracked economy building", ensure the flag is closest to the currently selected Inventory (castle/warehouse) 
    //      in terms of Flag distance so that any resources produced will be stored there and not some other Inventory where it might 
    //      not be needed.  This is necessary to prevent functions that demolish excess buildings from never-triggering because the 
    //      buildings are piling their products up in some *other* Inventory
    // ** NOTE ** it could be an optimization to have the FlagNodeSearch do the "closest Inventory" check also?
    //   maybe run the search until both the target_pos AND the nearest inventory are found, so it is a single search?
    //   - use FlagNodeSearch to find the shortest flag-path from each nearby_flag pos to the target_pos/flag, apply any penalties
    //   - add new_length score + existing road score, store complete solutions
    //   - quit early if max_road_solutions reached ?
    //   - plot and score any additional potential_road solutions made possible by creating
    //      flags & splitting existing roads, unless RoadOption::SplitRoads if set to false
    //   - sort the results by 'adjusted_score' ("adjusted length", or "overall length including existing, new length, and penalties"
    //   - build the best road (if better than best existing road)
    // NOTE - it seems like it would be cleaner to iterate over the entire list of direct and indirect roads at once instead of having
    //  two sections, but I don't feel like messing with this logic right now 
    for (MapPos end_pos : nearby_flags) {
      AILogDebug["util_build_best_road"] << name << " preparing to plot road from start_pos " << start_pos << " to nearby_flag pos " << end_pos;
      // it might be faster to have plot_road return the RoadEnds, but it seems messier
      //
      // check for direct road first (store any split-road solutions later)
      //
      Road potential_road = plot_road(map, player_index, start_pos, end_pos, &split_roads, road_options.test(RoadOption::HoldBuildingPos));
      // this while(true) loop looks goofy to me but it was the only clean way to do it without a GOTO statement
      while (true){
        if (potential_road.get_length() == 0) {
          AILogDebug["util_build_best_road"] << name << " unable to plot_road a DIRECT road from start_pos " << start_pos << " to nearby_flag pos " << end_pos << ", skipping direct road";
          break;
        }
        AILogDebug["util_build_best_road"] << name << " potential DIRECT road from start_pos " << start_pos << " to nearby_flag pos " << end_pos << " has new segment length " << potential_road.get_length();
        double convolution = static_cast<double>(potential_road.get_length()) / static_cast<double>(ideal_length);
        AILogDebug["util_build_best_road"] << name << " potential DIRECT road NEW length: " << potential_road.get_length() << ", ideal TOTAL length: " << ideal_length << ", convolution ratio: " << convolution;
        if (convolution >= max_convolution) {
          AILogDebug["util_build_best_road"] << name << " this potential DIRECT road solution is already too convoluted from new length alone, max is " << max_convolution;
          break;
        }
        // if this is "tracked economy building", ensure the end_pos flag is closest to the currently selected Inventory (castle/warehouse)
        if (verify_stock == true){
          if (find_nearest_inventory(map, player_index, end_pos, DistType::FlagAndStraightLine, &ai_mark_pos) != inventory_pos){
            AILogDebug["util_build_best_road"] << name << " DIRECT road - flag at end_pos " << end_pos << " is not closest to current inventory_pos " << inventory_pos << ", skipping";
            break;
          }
        }
        AILogDebug["util_build_best_road"] << name << " this potential DIRECT road solution is acceptable so far in terms of NEW length only, adding to RoadBuilder potential_roads";
        RoadEnds potential_road_ends = get_roadends(map, potential_road);
        rb.new_proad(potential_road_ends, potential_road);
        AILogDebug["util_build_best_road"] << name << " there are currently " << rb.get_proads().size() << " potential_roads in the list";
        break;
      }
      //
      // now do the same thing for any potential_roads found (flag-splitting), could make this a recursive function call instead...
      //
      if (road_options.test(RoadOption::SplitRoads == false)) {
        continue;
      }
      AILogDebug["util_build_best_road"] << name << " RoadOption SplitRoads is true, preparing to iterate over potential split roads found in previous plot_road call";
      AILogDebug["util_build_best_road"] << name << " there are " << split_roads.size() << " Road entries in the split_roads list (that was provide by the last plot_roads call to nearby map pos " << end_pos;
      for (Road split_road : split_roads) {
        MapPos split_end_pos = split_road.get_end(map.get());
        AILogDebug["util_build_best_road"] << name << " a potential split_road has end_pos: " << split_road.get_end(map.get()) << ", for comparison, start_pos is " << start_pos << " and split_road source is " << split_road.get_source();
        if (split_road.get_length() == 0) {
          AILogDebug["util_build_best_road"] << name << " road unexpectedly has zero length!  this should not happen, find out why!  skipping";
          continue;
        }

        AILogDebug["util_build_best_road"] << name << " split_road from start_pos " << start_pos << " to new-flag pos " << split_end_pos << " (on way to nearby_flag pos " << end_pos << ") has new segment length " << split_road.get_length();
        double convolution = static_cast<double>(split_road.get_length()) / static_cast<double>(ideal_length);
        AILogDebug["util_build_best_road"] << name << " split_road NEW length: " << split_road.get_length() << ", ideal TOTAL length: " << ideal_length << ", convolution ratio: " << convolution;
        if (convolution >= max_convolution) {
          AILogDebug["util_build_best_road"] << name << " this split_road solution is already too convoluted from new length alone, max is " << max_convolution;
          continue;
        }
        // if this is "tracked economy building", ensure the end_pos flag is closest to the currently selected Inventory (castle/warehouse)
        // because this is a split_flag solution, the flag doesn't exist yet.  Instead, check to ensure that BOTH of its adjacent flags
        // are closest to the current Inventory.  I am not sure what happens if two Inventories are equal flag distance apart, but I suspect
        // whichever is first in terms of path-Direction (0-5) wins
        int disqualified = 0;
        if (verify_stock == true){
          for (Direction dir : cycle_directions_cw()) {
            AILogVerbose["util_build_best_road"] << name << " looking for a path for splitting flag at split_end_pos " << split_end_pos << " in dir " << NameDirection[dir];
            if (map->has_path(split_end_pos, dir)) {
              Road split_road = trace_existing_road(map, split_end_pos, dir);
              AILogDebug["util_build_best_road"] << name << " found a path for splitting flag at split_end_pos " << split_end_pos << " in dir " << NameDirection[dir];
              MapPos adjacent_flag_pos = split_road.get_end(map.get());
              AILogDebug["util_build_best_road"] << name << " path for splitting flag at split_end_pos " << split_end_pos << " in dir " << NameDirection[dir] << " ends at flag at pos " << adjacent_flag_pos;
              if (find_nearest_inventory(map, player_index, adjacent_flag_pos, DistType::FlagAndStraightLine, &ai_mark_pos) != inventory_pos){
                AILogDebug["util_build_best_road"] << name << " potential split_road flag at split_end_pos " << split_end_pos << " is not closest to current inventory_pos " << inventory_pos << ", skipping";
                disqualified++;
              }
            }
          }
        }
        if (disqualified > 0){
          AILogDebug["util_build_best_road"] << name << " this split_end_pos is disqualified, skipping it";
          continue;
        }
        AILogDebug["util_build_best_road"] << name << " this split_road road solution is acceptable so far in terms of NEW length only, adding to RoadBuilder potential_roads";
        RoadEnds split_road_ends = get_roadends(map, split_road);
        rb.new_proad(split_road_ends, split_road);
        AILogDebug["util_build_best_road"] << name << " there are currently " << rb.get_proads().size() << " potential_roads in the list";
      }

    }
    //=====================================================================================
    // score the potential new roads to nearby_flags in terms of:
    //     tile_dist (from end_flag to target flag)
    //     flag_dist (from end_flag to target flag);
    //     new_length (of the potential road to be built from start_pos to nearby_flag pos
    AILogDebug["util_build_best_road"] << name << " preparing to score rb.get_proads() list";

    //
    // FIRST, score any existing roads(eroad) already attached to start_pos for later comparison to potential new
    //   These should only be here if RoadOption::Improve was set earlier in upstream functions
    //
    //MapPosSet scored_eroads;
    MapPos best_eroad_pos = bad_map_pos;
    unsigned int best_eroad_score = bad_score;
    if (map->has_flag(start_pos) && game->get_flag_at_pos(start_pos)->is_connected() && road_options.test(RoadOption::Improve)) {
      AILogDebug["util_build_best_road"] << name << " checking eroads because start_pos already has paths and RoadOption::Improve is set";
      for (std::pair<RoadEnds, RoadBuilderRoad*> er : rb.get_eroads()) {
        RoadEnds ends = er.first;
        MapPos start_pos = er.second->get_end1();
        MapPos start_dir = er.second->get_dir1();
        MapPos nearby_eroad_flag_pos = er.second->get_end2();
        AILogDebug["util_build_best_road"] << name << " found an existing road from start_pos " << start_pos << " in dir " << NameDirection[start_dir] << " to nearby_flag_pos " << nearby_eroad_flag_pos << ", checking score to target_pos " << target_pos;
        AILogDebug["util_build_best_road"] << name << " debug - checking score for nearby_eroad_flag_pos " << nearby_eroad_flag_pos << " to target_pos " << target_pos << " BEFORE find_flag_and_tile_dist is called to see if there will be a collision";
        AILogDebug["util_build_best_road"] << name << " debug - BEFORE score: flag_dist " << rb.get_score(nearby_eroad_flag_pos).get_flag_dist() << ", tile dist " << rb.get_score(nearby_eroad_flag_pos).get_tile_dist();
        if (!find_flag_and_tile_dist(map, player_index, &rb, nearby_eroad_flag_pos, target_pos, &ai_mark_pos)) {
          AILogDebug["util_build_best_road"] << name << " eroad: find_flag_and_tile_dist returned false for nearby_eroad_flag_pos " << nearby_eroad_flag_pos << "!";
          continue;
        }
        AILogDebug["util_build_best_road"] << name << " eroad: find_flag_and_tile_dist returned true for nearby_flag_pos " << nearby_eroad_flag_pos;

        AILogDebug["util_build_best_road"] << name << " debug - checking score for nearby_eroad_flag_pos " << nearby_eroad_flag_pos << " to target_pos " << target_pos << " AFTER find_flag_and_tile_dist is called to see if there was a collision";
        AILogDebug["util_build_best_road"] << name << " debug - AFTER score: flag_dist " << rb.get_score(nearby_eroad_flag_pos).get_flag_dist() << ", tile dist " << rb.get_score(nearby_eroad_flag_pos).get_tile_dist();

        AILogDebug["util_build_best_road"] << name << " preparing to apply penalties to eroad from start_pos " << start_pos << " in dir " << NameDirection[start_dir] << " to nearby_eroad_flag_pos " << nearby_eroad_flag_pos;
        FlagScore nearby_eroad_flag_score = rb.get_score(nearby_eroad_flag_pos);
        unsigned int tile_dist = nearby_eroad_flag_score.get_tile_dist();
        unsigned int flag_dist = nearby_eroad_flag_score.get_flag_dist();
        bool contains_castle_flag = nearby_eroad_flag_score.get_contains_castle_flag();
        // no new length for an existing road, this means it will be preferred over a similiar
        //   potential new road after new_length penalty applied.  A new road must be significantly better to be built
        unsigned int new_length = 0;
        AILogDebug["util_build_best_road"] << name << " BEFORE applying any penalties, potential road from start_pos " << start_pos << " in dir " << NameDirection[start_dir] << " to nearby_eroad_flag_pos " << nearby_eroad_flag_pos <<
          " has tile_dist " << tile_dist << ", flag_dist " << flag_dist << ", new_length " << new_length << " to target_pos " << target_pos;
        //----------------------------------------------------------------------------------------------
        // RoadOption penalties applied here!   * 1 means no penalty, only in place for easy adjustment
        //----------------------------------------------------------------------------------------------
        unsigned int adjusted_score = static_cast<unsigned int>(static_cast<double>(tile_dist * 1) + static_cast<double>(flag_dist * 1));
        //----------------------------------------------------------------------------------------------
        if (road_options.test(RoadOption::PenalizeCastleFlag) && contains_castle_flag == true) {
          adjusted_score += contains_castle_flag_penalty;
          AILogDebug["pathfinder"] << "applying contains_castle_flag penalty of " << contains_castle_flag_penalty;
        }
        AILogDebug["util_build_best_road"] << name << " AFTER penalties, existing road from start_pos " << start_pos << " in dir " << NameDirection[start_dir] << " to nearby_eroad_flag_pos " << nearby_eroad_flag_pos <<
          " has tile_dist " << tile_dist << ", flag_dist " << flag_dist << ", new_length " << new_length << ", adjusted_score " << adjusted_score << " to target_pos " << target_pos;
        if (adjusted_score < best_eroad_score) {
          AILogDebug["util_build_best_road"] << name << " new best_eroad_score found: " << adjusted_score << " from pos " << nearby_eroad_flag_pos;
          best_eroad_score = adjusted_score;
          best_eroad_pos = nearby_eroad_flag_pos;
        }
      }
      AILogDebug["util_build_best_road"] << name << " done finding eroads, best_eroad_score found: " << best_eroad_score << " from pos " << best_eroad_pos;
    }
    else {
      AILogDebug["util_build_best_road"] << name << " start_pos " << start_pos << " has no flag, no paths, or RoadOption::Improve not set, not checking for eroads";
    } // if start_pos has any paths/eroads already

    // foreach potential new road (proad) to be built to a nearby flag
    //  score the existing road (eroad) that begins at the flag/pos where the proad ends
    AILogDebug["util_build_best_road"] << name << " preparing to score potential new roads from start_pos to nearby_flag positions";
    MapPosSet scored_proads;
    for (std::pair<RoadEnds, RoadBuilderRoad*> pr : rb.get_proads()) {
      RoadEnds ends = pr.first;
      RoadBuilderRoad *rbroad = pr.second;
      MapPos start_pos = pr.second->get_end1();
      MapPos nearby_flag_pos = pr.second->get_end2();
      AILogDebug["util_build_best_road"] << name << " found an potential new road to nearby_flag_pos " << nearby_flag_pos;
      // why is this scoring route to castle_flag pos?  shouldn't it be to target_pos for affinity building??
      //if (!score_flag(map, player_index, &rb, road_options, nearby_flag_pos, castle_flag_pos, &ai_mark_pos)) {
      // trying this instead - may04 2020
      //  WAIT WAIT WAIT, i think castle_flag_pos is only used to pass the location of castle flag so the contains_castle_flag bool can be set!
      //     it isn't actually using it as the target!  setting it back
      //if (!score_flag(map, player_index, &rb, road_options, nearby_flag_pos, target_pos, &ai_mark_pos)) {
      if (!score_flag(map, player_index, &rb, road_options, nearby_flag_pos, castle_flag_pos, &ai_mark_pos)) {
        AILogDebug["util_build_best_road"] << name << " proad: score_flag returned false for nearby_flag_pos " << nearby_flag_pos << "!";
        continue;
      }
      AILogDebug["util_build_best_road"] << name << " proad: score_flag returned true for nearby_flag_pos " << nearby_flag_pos;
      // then add the score of the new segment and apply any penalties
      //    and insert adjusted_scores into MapPosSet for sorting later
      AILogDebug["util_build_best_road"] << name << " preparing to apply penalties to proad from start_pos " << start_pos << " to nearby_flag_pos " << nearby_flag_pos;
      FlagScore nearby_flag_score = rb.get_score(nearby_flag_pos);
      unsigned int tile_dist = nearby_flag_score.get_tile_dist();
      unsigned int flag_dist = nearby_flag_score.get_flag_dist();
      bool contains_castle_flag = nearby_flag_score.get_contains_castle_flag();
      unsigned int new_length = static_cast<unsigned int>(rbroad->get_road().get_length());
      AILogDebug["util_build_best_road"] << name << " BEFORE applying any penalties, potential road from start_pos " << start_pos << " to nearby_flag_pos " << nearby_flag_pos <<
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
      unsigned int adjusted_score = static_cast<unsigned int>(static_cast<double>(tile_dist * 1) + static_cast<double>(new_length) * new_length_penalty + static_cast<double>(flag_dist * 1));
      if (road_options.test(RoadOption::PenalizeCastleFlag) && contains_castle_flag == true) {
        adjusted_score += contains_castle_flag_penalty;
        AILogDebug["util_build_best_road"] << "applying contains_castle_flag penalty of " << contains_castle_flag_penalty;
      }
      AILogDebug["util_build_best_road"] << name << " AFTER penalties, potential road from start_pos " << start_pos << " to nearby_flag_pos " << nearby_flag_pos <<
        " has tile_dist " << tile_dist << ", flag_dist " << flag_dist << ", new_length " << new_length << ", adjusted_score " << adjusted_score;
      scored_proads.insert(std::make_pair(nearby_flag_pos, adjusted_score));
    }
    AILogDebug["util_build_best_road"] << name << " done scoring proads";


    // sort the potential new roads by their score, best (lowest) score first
    AILogDebug["util_build_best_road"] << name << " preparing to sort scored_proads into a MapPosVector";
    MapPosVector sorted_scored_proads = sort_by_val_asc(scored_proads);

    // try to build a road, starting with the best scoring potential_road end flag, until successful
    AILogDebug["util_build_best_road"] << name << " preparing to iterate over sorted_scored_proads to try to build roads to best nearby_flag";
    for (MapPos end_pos : sorted_scored_proads) {
      AILogDebug["util_build_best_road"] << name << " considering building road from start_pos " << start_pos << " to end_pos " << end_pos;
      if (roads_built >= target_count) {
        AILogDebug["util_build_best_road"] << name << " roads_built " << roads_built << " >= target_count " << target_count << ", done building roads";
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
          AILogDebug["util_build_best_road"] << name << " found adjusted_score of " << ppair.second << " for this proad ending at end_pos " << end_pos;
          // it must be SIGNIFICANTLY better, say 50% + 2?
          double modified_prod_score = static_cast<double>(ppair.second) * static_cast<double>(1.50) + 2;
          //if (best_eroad_score <= ppair.second) {
          if (best_eroad_score <= modified_prod_score) {
            AILogDebug["util_build_best_road"] << name << " proad from start_pos " << start_pos << " to end_pos " << end_pos << " has +50%+2 score " << modified_prod_score << " which is not significantly better than best_eroad_score " << best_eroad_score << ", skipping this target";
            skip_target = true;
            break;
          }
          else {
            AILogDebug["util_build_best_road"] << name << " proad from start_pos " << start_pos << " to end_pos " << end_pos << " has +50%+2 score " << modified_prod_score << " which is significantly better than best_eroad_score " << best_eroad_score << ", will build new road";
            break;
          }
        }
        if (skip_target == true) {
          AILogDebug["util_build_best_road"] << name << " skip_target is true, breaking from foreach sorted_scored_proads - not building road to this target";
          break;
        }
      }
      else {
        AILogDebug["util_build_best_road"] << name << " start_pos " << start_pos << " has no paths, or RoadOption::Improve not set, not comparing this solution to any best_eroad_score";
      } // if start_pos has any paths/eroads already

      Road road = proad->get_road();
      AILogDebug["util_build_best_road"] << name << " trying to build road from " << start_pos << " to " << end_pos << " as specified in proad";

      // check to see if this is a water road about to be built
      //  it should only be possible for BOTH ends to be in water, but checking both just to be sure
      bool water_road = false;
      if (map->road_segment_in_water(end_pos, reverse_direction(road.get_last()))){
        AILogDebug["util_build_best_road"] << name << " end_pos is in water!";
        water_road = true;
      }
      if (map->road_segment_in_water(start_pos, road.get_first())){
        AILogDebug["util_build_best_road"] << name << " start_pos is in water!";
        water_road = true;
      }
      if (water_road) {
        if (!road_options.test(RoadOption::AllowWaterRoad)) {
          AILogDebug["util_build_best_road"] << name << " this solution is a water road, but AllowWaterRoad RoadOption is not set, skipping";
          continue;
        }
        RoadOptions recursive_land_road_options = road_options;
        recursive_land_road_options.reset(RoadOption::AllowWaterRoad);
        // does this return true if an existing (better?) land road already exists?? or only if it creates one?
        bool land_road_was_built = AI::build_best_road(start_pos, recursive_land_road_options);
        if (land_road_was_built) {
          AILogDebug["util_build_best_road"] << name << " successfully built land road, allowing a water road to " << start_pos;
        }
        else {
          AILogDebug["util_build_best_road"] << name << " failed to build land road, NOT allowing a water road to " << start_pos;
          continue;
        }
      }


      bool created_new_flag = false;
      if (game->get_flag_at_pos(end_pos) == nullptr) {
        AILogDebug["util_build_best_road"] << name << " end_pos " << end_pos << " has no flag, must be fake flag/split road, trying to create a real flag";
        AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->build_flag (split road)";
        game->get_mutex()->lock();
        AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->build_flag (split road)";
        bool was_built = game->build_flag(end_pos, player);
        AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->build_flag (split road)";
        game->get_mutex()->unlock();
        AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->build_flag (split road)";
        if (was_built) {
          AILogDebug["util_build_best_road"] << name << " successfully built new flag at end_pos " << end_pos << ", splitting the road";
          created_new_flag = true;
        }
        else {
          AILogDebug["util_build_best_road"] << name << " failed to build flag at end_pos " << end_pos << ", FIND OUT WHY!  not trying to build this road.  marking pos in blue";
          ai_mark_pos.insert(ColorDot(end_pos, "blue"));
          std::this_thread::sleep_for(std::chrono::milliseconds(5000));
          continue;
        }
      }
      //
      // insert code here to check if the second road is actually better than the first road built (if one was built) !!
      //   otherwise, don't build. 
      //
      if (true) {
        if (roads_built >= 1) {
          AILogDebug["util_build_best_road"] << name << " TODO - check to see if this road is better than the first one built!";
        }
        AILogDebug["util_build_best_road"] << name << " about to build road, dumping some road stats.  source=" << road.get_source() << ", end=" << road.get_end(game->get_map().get());
        AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->build_road (non-direct road)";
        game->get_mutex()->lock();
        AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->build_road (non-direct road)";
        bool was_built = game->build_road(road, game->get_player(player_index));
        AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->build_road (non-direct road)";
        game->get_mutex()->unlock();
        AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->build_road (non-direct road)";
        if (was_built) {
          AILogDebug["util_build_best_road"] << name << " successfully built road from " << start_pos << " to " << end_pos << " as specified in PotentialRoad";
          roads_built++;
          continue;
        }
        else {
          AILogDebug["util_build_best_road"] << name << " ERROR - failed to build road from " << start_pos << " to " << end_pos << ", FIND OUT WHY!   trying next best solution...";
        }
      }
      else {
        AILogDebug["util_build_best_road"] << name << " last road built is better than this proposed one, not building second road (should only happy for dual-affinity buildings)";
      }
      if (created_new_flag) {
        AILogDebug["util_build_best_road"] << name << " removing the newly created flag at end_pos so it doesn't screw up the rest of the road solutions";
        AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->demolish_flag (couldn't connect new flag)";
        game->get_mutex()->lock();
        AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->demolish_flag (couldn't connect new flag)";
        game->demolish_flag(end_pos, game->get_player(player_index));
        AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->demolish_flag (couldn't connect new flag)";
        game->get_mutex()->unlock();
        AILogDebug["util_build_best_road"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->demolish_flag (couldn't connect new flag)";
      }
      AILogDebug["util_build_best_road"] << name << " done trying to build road from " << start_pos << " to " << end_pos;
    } // end foreach end_pos
    AILogDebug["util_build_best_road"] << name << " done building roads to target_pos " << target_pos;
  } //foreach target
  // return success if at least one road built, even if two targets found

  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_build_best_road"] << name << " build_best_road call took " << duration;

  if (roads_built < 1) {
    AILogDebug["util_build_best_road"] << name << " build_best_road: Failed to connect specified flag to road system!";
    return false;
  }
  AILogDebug["util_build_best_road"] << name << " Done with build_best_road, roads built: " << roads_built << ", out of target_count: " << target_count;
  return true;
}

MapPosVector
// return a vector of MapPos of affinity building[s], or a default if none defined in BuildingAffinity table (castle)
//***************************************
// THIS FUNCTION SHOULD EITHER RETURN TWO FIXED ITEMS OR TWO LISTS OF POTENTIAL TARGET BUILDINGS IN ORDER OF DIST/BUILD PROGRESS!
//((((((((((((((((((()))))))))))))))))))
//
AI::get_affinity(MapPos flag_pos, Building::Type optional_building_type){
  AILogDebug["util_get_affinity"] << name << " inside AI::get_affinity for flag_pos " << flag_pos;

  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();

  MapPosVector affinity;

/* no, DON'T do this, let the calling function deal with fallback to inventory_pos
// this results in the wrong ordering anyway, the better results need to be added to the
// front of the vector, not the end
  // always have inventory_pos pos as the fallback in case no affinity
  // ***** THIS NEEDS TO BE CHANGED TO FIND NEAREST STOCK WITH A FLAG SEARCH!!  ******
  // wait this doesn't need to be down-right because inventory_pos IS the flag pos, right???
  //MapPos nearest_inventory_flag = map->move_down_right(find_nearest_inventory(flag_pos));
  MapPos nearest_inventory_flag = find_nearest_inventory(flag_pos);
  // does this need to push the stock pos TWICE in case no affinity buildings found?
  //  I think it does, if the call to this function is expecting two results
  // OR, is it working through a whole list that could have multiple buildings of each type?? I forget
  //   NEED TO CHECK ON THIS
  AILogDebug["util_get_affinity"] << name << " adding flag pos of nearest_inventory_flag pos " << nearest_inventory_flag << " to affinity list as a fallback";
  affinity.push_back(nearest_inventory_flag);
  // just do it twice for now
  affinity.push_back(nearest_inventory_flag);
*/

  // find out what kind of building we are getting affinity for
  Building::Type request_type = Building::TypeNone;
  if (optional_building_type != Building::TypeNone){
    request_type = optional_building_type;
    AILogDebug["util_get_affinity"] << name << " using specified optional_building_type " << NameBuilding[request_type];
  }else{
    if (!map->has_flag(flag_pos)){
      AILogDebug["util_get_affinity"] << name << " no flag at flag_pos " << flag_pos << " and no optional_affinity building specified, returning empty vector";
      return affinity;
    }
    if (!map->has_building(map->move_up_left(flag_pos))){
      AILogDebug["util_get_affinity"] << name << " no building attached to flag_pos " << flag_pos << " and no optional_affinity building specified, returning empty vector";
      return affinity;
    }
    if (game->get_flag_at_pos(flag_pos) != nullptr ){
      if (game->get_flag_at_pos(flag_pos)->get_building() != nullptr){
        request_type = game->get_flag_at_pos(flag_pos)->get_building()->get_type();
      }
    }
    AILogDebug["util_get_affinity"] << name << " flag at flag_pos " << flag_pos << " is attached to a building of type " << NameBuilding[request_type];
  }
  if (request_type == Building::TypeNone){
    AILogDebug["util_get_affinity"] << name << " request_type is TypeNone, maybe flag or building removed?";
  }

  // up to two affinity types for a given request type
  for (int i = 0; i < 2; i++){
    Building::Type affinity_type = BuildingAffinity[request_type][i];
    if (affinity_type == Building::TypeNone){
      AILogDebug["util_get_affinity"] << name << " requested building of type " << NameBuilding[request_type] << " has no affinity type #" << i << ", returning affinity list so far";
      return affinity;
    }
    AILogDebug["util_get_affinity"] << name << " looking for nearest connected building of affinity type#" << i << " " << NameBuilding[affinity_type];
    Building *building = AI::find_nearest_building(flag_pos, CompletionLevel::Connected, affinity_type);
    if (building == nullptr) {
      AILogDebug["util_get_affinity"] << name << " could not find any connected affinity building of type#" << i << " " << NameBuilding[affinity_type] << " for this building of request_type " << NameBuilding[request_type] << ", returning affinity list so far";
      return affinity;
    }
    MapPos found_pos = building->get_position();
    AILogDebug["util_get_affinity"] << name << " closest connected affinity building of affinity type#" << i << " " << NameBuilding[affinity_type] << " found at found_pos " << found_pos << ", adding its flag to affinity list";
    MapPos building_flag_pos = game->get_flag(building->get_flag_index())->get_position();
    AILogDebug["util_get_affinity"] << name << " adding found building's flag_pos " << building_flag_pos << " to affinity list";
    affinity.push_back(building_flag_pos);
  }

  AILogDebug["util_get_affinity"] << name << " found buildings for two affinity types, returning the flag_pos of both";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_get_affinity"] << name << " done util_get_affinity call took " << duration;
  return affinity;
}

// return Building* for the nearest building of the specified type 
//  that meets the minimum specified completion level
// this function uses straight-line distance instead of spiral-dist
//  because it considers the entire realm which is too far for
//  even extended_spiral_dist
//
// NOTE - adding a FlagSearch after all!! to only allow
//  returning affinity buildings that are closest-by-flag-dist to
//  the currently Inventory.  Otherwise, the roadbuilding logic
//  will usually disqualify all roads if the affinity building 
//  returned is not already closest to the currently selected stock
//
// burning buildings are always excluded from this search
//
// note that max_dist -1 translates to integer max as an unsigned int
//  so if not set it should never be a limiting factor
Building*
AI::find_nearest_building(MapPos pos, CompletionLevel level, Building::Type building_type, unsigned int max_dist) {
  AILogDebug["util_find_nearest_building"] << name << " inside find_nearest_building of type " << NameBuilding[building_type] << ", completion_level " << NameCompletionLevel[level] << ", max_dist " << (signed)max_dist << ", and pos " << pos;
  unsigned int shortest_dist = bad_score;
  Building *closest_building = nullptr;
  AILogDebug["util_find_nearest_building"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings";
  game->get_mutex()->lock();
  AILogDebug["util_find_nearest_building"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings";
  Game::ListBuildings buildings = game->get_player_buildings(player);
  AILogDebug["util_find_nearest_building"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings";
  game->get_mutex()->unlock();
  AILogDebug["util_find_nearest_building"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings";
  for (Building *building : buildings) {
    if (building == nullptr)
      continue;
    if (building->is_burning())
      continue;
    if (building->get_type() != building_type)
      continue;
    MapPos building_flag_pos = map->move_down_right(building->get_position());
    unsigned int dist = (unsigned)get_straightline_tile_dist(map, pos, building_flag_pos);
    if (dist > max_dist || dist >= shortest_dist)
      continue;
    if ((level <= Unfinished) ||
        (level == Connected && game->get_flag_at_pos(building_flag_pos)) ||
        (level >= Completed && building->is_done())){
      AILogDebug["util_find_nearest_building"] << name << " SO FAR, the closest " << NameCompletionLevel[level] << " building of type " << NameBuilding[building_type] << " within max_dist " << max_dist << " to center pos " << pos << " found at " << building_flag_pos;

      // ensure the building_flag_pos is closest-by-flag-dist to the currently selected Inventory (castle/warehouse)
      //  or else it will try to connect to buildings that aren't part of the same "economy" and fail to 
      //  connect a road because of the same check done when placing roads
      if (building_type == Building::TypeStock){
        AILogDebug["util_find_nearest_building"] << name << " not performing flagsearch because this is a Stock";
      }else{
        AILogDebug["util_find_nearest_building"] << name << " performing flagsearch to find nearest inventory to this building of type " << NameBuilding[building_type] << " found at " << building_flag_pos;
        int nearest_inventory = find_nearest_inventory(map, player_index, building_flag_pos, DistType::FlagAndStraightLine, &ai_mark_pos);
        if (nearest_inventory < 0){
          AILogDebug["util_find_nearest_building"] << name << " inventory not found for flag at building_flag_pos " << building_flag_pos << ", maybe this flag isn't part of the road system??";
          continue;
        }
        if (nearest_inventory != inventory_pos){
          AILogDebug["util_find_nearest_building"] << name << " flag at building_flag_pos " << building_flag_pos << " is not closest to current inventory_pos " << inventory_pos << ", skipping";
          continue;
        }
      }
      // mark this building as the best one so far
      shortest_dist = dist;
      closest_building = building;
      continue;
    }
  }
  if (closest_building != nullptr){
    AILogDebug["util_find_nearest_building"] << name << " closest " << NameCompletionLevel[level] << " building of type " << NameBuilding[building_type] << " within max_dist " << (signed)max_dist << " to center pos " << pos << " found at " << closest_building->get_position();
  }else{
    AILogDebug["util_find_nearest_building"] << name << " did not find any building of type " << NameBuilding[building_type] << " with CompletionLevel " << NameCompletionLevel[level] << " within max_dist " << (signed)max_dist << " and nearest to inventory_pos " << inventory_pos << " in this player's realm!";
  }
  return closest_building;
}

/*
// return Building* for first building of the specified type exists within specified distance from pos
Building*
AI::find_nearest_building(MapPos center_pos, unsigned int distance, Building::Type building_type) {
  AILogDebug["util_find_nearest_building"] << name << " inside find_nearest_building " << NameBuilding[building_type] << ", distance " << distance << ", and target pos " << center_pos;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    if (!map->has_building(pos))
      continue;
    if (game->get_building_at_pos(pos)->is_burning())
      continue;
    if (game->get_building_at_pos(pos)->get_type() == building_type) {
      AILogDebug["util_find_nearest_building"] << name << " found a building of type " << NameBuilding[building_type] << " at pos " << pos;
      return game->get_building_at_pos(pos);
    }
  }

  AILogDebug["util_find_nearest_building"] << name << " no nearby building found of type " << NameBuilding[building_type] << ", returning nullptr";
  return nullptr;
}

// return Building* for first building of the specified type exists within specified distance from pos
//   THAT HAS AT LEAST ONE PATH.  This building does not have to be completed
Building*
AI::find_nearest_connected_building(MapPos center_pos, unsigned int distance, Building::Type building_type) {
  AILogDebug["find_nearest_connected_building"] << name << " inside find_nearest_connected_building " << NameBuilding[building_type] << ", distance " << distance << ", and target pos " << center_pos;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    if (!map->has_building(pos))
      continue;
    if (!map->has_flag(map->move_down_right(pos)))
      continue;
    if (!game->get_flag_at_pos(map->move_down_right(pos))->is_connected())
      continue;
    if (game->get_building_at_pos(pos)->is_burning())
      continue;
    if (game->get_building_at_pos(pos)->get_type() == building_type) {
      AILogDebug["find_nearest_connected_building"] << name << " found a connected building of type " << NameBuilding[building_type] << " at pos " << pos;
      return game->get_building_at_pos(pos);
    }
  }

  AILogDebug["util_find_nearest_building"] << name << " no nearby connected building found of type " << NameBuilding[building_type] << ", returning nullptr";
  return nullptr;
}

// return Building* for first building of the specified type exists within specified distance from pos
//   THAT IS FULLY BUILT
Building*
AI::find_nearest_completed_building(MapPos center_pos, unsigned int distance, Building::Type building_type) {
  AILogDebug["util_find_nearest_completed_building"] << name << " inside find_nearest_completed_building " << NameBuilding[building_type] << ", distance " << distance << ", and target pos " << center_pos;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    if (!map->has_building(pos))
      continue;
    if (game->get_building_at_pos(pos)->is_burning())
      continue;
    if (game->get_building_at_pos(pos)->get_type() != building_type)
      continue;
    if (!game->get_building_at_pos(pos)->is_done())
      continue;
    AILogDebug["util_find_nearest_completed_building"] << name << " found a completed building of type " << NameBuilding[building_type] << " at pos " << pos;
    return game->get_building_at_pos(pos);
  }

  AILogDebug["util_find_nearest_completed_building"] << name << " no nearby completed building found of type " << NameBuilding[building_type] << ", returning nullptr";
  return nullptr;
}
*/

// return true if any building of the specified type exists within specified spiral 
//  distance from pos only considers this player's buildings
//  IS THIS DEPRECATED NOW???
bool
AI::building_exists_near_pos(MapPos center_pos, unsigned int distance, Building::Type building_type) {
  AILogDebug["util_building_exists_near_pos"] << name << " inside AI::building_exists_near_pos with type " << NameBuilding[building_type] << ", distance " << distance << ", and target pos " << center_pos;
  for (unsigned int i = 0; i < distance; i++) {
    //AILogDebug["util_building_exists_near_pos"] << name << " inside AI::building_exists_near_pos debug2, i=" << i;
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    //AILogDebug["util_building_exists_near_pos"] << name << " inside AI::building_exists_near_pos debug3";
    if (!map->has_building(pos) || map->get_owner(pos) != player_index)
      continue;
    if (game->get_building_at_pos(pos)->is_burning())
      continue;
    if (game->get_building_at_pos(pos)->get_type() == building_type) {
      AILogDebug["util_building_exists_near_pos"] << name << " found a building of type " << NameBuilding[building_type] << " at pos " << pos;
      return true;
    }
  }

  AILogDebug["util_building_exists_near_pos"] << name << " no building found of type " << NameBuilding[building_type] << " near center_pos " << center_pos << ", returning false";
  return false;
}


/*
// find the "best" of two building types, in preference order: occupied->completed->any
//   and find the halfway point between those two and return it
// for trying to build between two buildings, such as building a SteelSmelter halfway between CoalMine and IronMine
//   currently this is ONLY used for placing a steelsmelter between coal and iron mines

// CHANGING THIS SIGNIFICANTLY jan19 2021
// actaully... I am going to eliminate this for now.  Since moving to flagsearch checks
//  it isn't likely to accomplish much
MapPos
AI::find_halfway_pos_between_buildings(Building::Type first, Building::Type second) {
  AILogDebug["util_find_halfway_pos_between_buildings"] << name << " inside get_halfway_pos_between_buildings, type1 " << NameBuilding[first] << " type2 " << NameBuilding[second] << " for inventory_pos " << inventory_pos;
  update_building_counts();
  Building::Type type[2] = { first, second };
  MapPos found_pos[2] = { bad_map_pos, bad_map_pos };
  for (int x = 0; x < 2; x++) {
    AILogDebug["util_find_halfway_pos_between_buildings"] << name << " searching this stock area for a building of type" << x << " " << NameBuilding[type[x]];
    // change to connected_count?  should be same since flagsearch added
    if (stock_buildings.at(inventory_pos).count[type[x]] >= 1) {
      AILogDebug["util_find_halfway_pos_between_buildings"] << name << " stock has at least one connected building of type" << x << " " << NameBuilding[type[x]];
      find_nearest_building()


//  since enabling multiple economies, this function only considers buildings attached to the current inventory_pos

// ***** THIS NEEDS TO BE CHANGED TO FIND NEAREST STOCK WITH A FLAG SEARCH!!  ******

MapPos
AI::find_halfway_pos_between_buildings(Building::Type first, Building::Type second) {
  AILogDebug["util_find_halfway_pos_between_buildings"] << name << " inside get_halfway_pos_between_buildings, type1 " << NameBuilding[first] << " type2 " << NameBuilding[second] << " for inventory_pos " << inventory_pos;
  update_building_counts();
  Building::Type type[2] = { first, second };
  MapPos found_pos[2] = { bad_map_pos, bad_map_pos };
  for (int x = 0; x < 2; x++) {
    AILogDebug["util_find_halfway_pos_between_buildings"] << name << " searching this stock area for a building of type" << x << " " << NameBuilding[type[x]];
    if (stock_buildings.at(inventory_pos).occupied_count[type[x]] >= 1) {
      AILogDebug["util_find_halfway_pos_between_buildings"] << name << " searching for an OCCUPIED building of type" << x << " " << NameBuilding[type[x]];
      for (MapPos center_pos : stock_buildings.at(inventory_pos).occupied_military_pos) {
        for (unsigned int i = 0; i < spiral_dist(9); i++) {
          MapPos pos = map->pos_add_extended_spirally(center_pos, i);
          if (!map->has_building(pos))
            continue;
          if (game->get_building_at_pos(pos)->is_burning())
            continue;
          AILogDebug["util_find_halfway_pos_between_buildings"] << name << " found a building at pos " << pos;
          Building *building = game->get_building_at_pos(pos);
          AILogDebug["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " is of type " << NameBuilding[building->get_type()];
          if (building->get_type() != type[x]) {
            AILogDebug["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is not the right type, continuing";
            continue;
          }
          AILogDebug["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is the right type, checking its status";
          if (building->is_done() && building->has_serf()) {
            found_pos[x] = building->get_position();
            AILogDebug["util_find_halfway_pos_between_buildings"] << name << " found an acceptable OCCUPIED building of type" << x << " " << NameBuilding[type[x]] << " at pos " << found_pos[x];
            break;
          }
        }

      }
    }
    else if (realm_completed_building_count[type[x]] >= 1) {
      AILogDebug["util_find_halfway_pos_between_buildings"] << name << " searching for a COMPLETED building of type" << x << " " << NameBuilding[type[x]];
      for (MapPos center_pos : stock_buildings.at(inventory_pos).occupied_military_pos) {
        for (unsigned int i = 0; i < spiral_dist(9); i++) {
          MapPos pos = map->pos_add_extended_spirally(center_pos, i);
          if (!map->has_building(pos))
            continue;
          if (game->get_building_at_pos(pos)->is_burning())
            continue;
          AILogDebug["util_find_halfway_pos_between_buildings"] << name << " found a building at pos " << pos;
          Building *building = game->get_building_at_pos(pos);
          AILogDebug["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " is of type " << NameBuilding[building->get_type()];
          if (building->get_type() != type[x]) {
            AILogDebug["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is not the right type, continuing";
            continue;
          }
          AILogDebug["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is the right type, checking its status";
          if (building->is_done()) {
            found_pos[x] = building->get_position();
            AILogDebug["util_find_halfway_pos_between_buildings"] << name << " found an acceptable COMPLETED building of type" << x << " " << NameBuilding[type[x]] << " at pos " << found_pos[x];
            break;
          }
        }

      }
    }
    else if (realm_building_count[type[x]] >= 1) {
      AILogDebug["util_find_halfway_pos_between_buildings"] << name << " searching for ANY building of type" << x << " " << NameBuilding[type[x]];
      for (MapPos center_pos : stock_buildings.at(inventory_pos).occupied_military_pos) {
        AILogDebug["util_find_halfway_pos_between_buildings"] << name << " searching around center_pos " << center_pos;
        for (unsigned int i = 0; i < spiral_dist(9); i++) {
          MapPos pos = map->pos_add_extended_spirally(center_pos, i);
          if (!map->has_building(pos))
            continue;
          if (game->get_building_at_pos(pos)->is_burning())
            continue;
          AILogDebug["util_find_halfway_pos_between_buildings"] << name << " found a building at pos " << pos;
          Building *building = game->get_building_at_pos(pos);
          AILogDebug["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " is of type " << NameBuilding[building->get_type()];
          if (building->get_type() != type[x]) {
            AILogDebug["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is not the right type, continuing";
            continue;
          }
          AILogDebug["util_find_halfway_pos_between_buildings"] << name << " building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is the right type, checking its status";
          if (building->has_serf()) {
            found_pos[x] = building->get_position();
            AILogDebug["util_find_halfway_pos_between_buildings"] << name << " found an acceptable ANY building of type" << x << " " << NameBuilding[type[x]] << " at pos " << found_pos[x];
            break;
          }
        }

      }
    }
    else {
      AILogDebug["util_find_halfway_pos_between_buildings"] << name << " no buildings of type" << x << " " << NameBuilding[type[x]] << " known in realm, returning bad_map_pos";
      return bad_map_pos;
    }
    // this shouldn't happen if update_building_counts() is accurate unless building was destroyed while searching
    if (found_pos[x] == bad_map_pos) {
      AILogDebug["util_find_halfway_pos_between_buildings"] << name << " could not find expected building of type" << x << " " << NameBuilding[type[x]] << " in realm despite being known! returning bad_map_pos";
      return bad_map_pos;
    }
    AILogDebug["util_find_halfway_pos_between_buildings"] << name << " found best building of type" << x << " " << NameBuilding[type[x]] << " at pos " << found_pos[x];
  }
  AILogDebug["util_find_halfway_pos_between_buildings"] << name << " found both buildings, " << NameBuilding[type[0]] << " at " << found_pos[0] << ", and " << NameBuilding[type[1]] << " at " << found_pos[1];
  AILogDebug["util_find_halfway_pos_between_buildings"] << name << " finding halfway point";
  MapPos halfway_pos = get_halfway_pos(found_pos[0], found_pos[1]);
  AILogDebug["util_find_halfway_pos_between_buildings"] << name << " done get_halfway_pos_between_buildings, returning halfway_pos " << halfway_pos;
  return halfway_pos;
}
*/


//
// build a Road object by following the paths from start_pos in specified direction
//    until another flag is found.  The start pos doesn't have to be a real flag
Road
AI::trace_existing_road(PMap map, MapPos start_pos, Direction dir) {
  //AILogDebug["util_trace_existing_road"] << name << " inside trace_existing_road, start_pos " << start_pos << ", dir: " << NameDirection[dir];
  Road road;
  if (!map->has_path(start_pos, dir)) {
    AILogDebug["util_trace_existing_road"] << name << " no path found at " << start_pos << " in direction " << NameDirection[dir] << "!  FIND OUT WHY";
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
      //AILogDebug["util_trace_existing_road"] << name << " flag found at pos " << pos << ", returning road (which has length " << road.get_length() << ")";
      //ai_mark_road->invalidate();
      return road;
    }
    for (Direction new_dir : cycle_directions_cw()) {
      //AILogDebug["util_trace_existing_road"] << name << " looking for path from pos " << pos << " in dir " << NameDirection[new_dir];
      if (map->has_path(pos, new_dir) && new_dir != reverse_direction(dir)) {
        //AILogDebug["util_trace_existing_road"] << name << " found path from pos " << pos << " in dir " << NameDirection[new_dir];
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
  AILogDebug["util_get_corners"] << name << " inside AI::get_corners(pos)";
  MapPosVector positions;
  MapPosVector::iterator it;
  for (Direction dir : cycle_directions_rand_cw()) {
  //for (Direction dir : cycle_directions_cw()) {
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
  AILogDebug["util_get_corners"] << name << " inside AI::get_corners(pos, dist)";
  MapPosVector positions;
  if (distance > 24) {
    AILogDebug["util_get_corners2"] << name << " get_corners only supports up to distance 24!  this is straight line not spirally distance.  Capping it at 24";
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

// silly function to determine the Direction 
//  of a corner, relative to the center_pos,
//  so that a directional fill pattern can be used
//
// ISSUE - because a series of centers are used to find corners
//  and the centers-to-corner association is lost
//  this function only ever works if the center pos happens to
//  be the one associated with the corner.  Usually this is only
//  at the castle pos during the early game.  That is the
//  only time when this really matters much.  For now I am going to just
//  let it quietly fail in those cases and return DirectionNone which
//  results in the normal spiral search pattern anyway
Direction
AI::get_dir_from_corner(MapPos center_pos, MapPos corner_pos) {
  AILogDebug["util_get_dir_from_corner"] << name << " inside AI::get_dir_from_corner(pos) with center_pos " << center_pos << " and corner_pos " << corner_pos;
  for (Direction dir : cycle_directions_cw()) {
    MapPos pos = center_pos;
    for (int x = 0; x < 5; x++) {
      pos = map->move(pos, dir);
    }
    if (pos == corner_pos){
      AILogDebug["util_get_dir_from_corner"] << name << " found direction from center_pos " << center_pos << " to corner_pos " << corner_pos << ": " << NameDirection[dir] << " / " << dir;
      return dir;
    }
  }
  // if we reached this point it wasn't found, throw an exception as this should never happen
  //  WRONG - this happens all the time, see ISSUE above, it is okay
  //AILogError["util_get_dir_from_corner"] << name << " failed to find the Direction of the corner_pos " << corner_pos << " associated with center_pos " << center_pos;
  //ai_mark_pos.insert(ColorDot(center_pos, "green"));
  //ai_mark_pos.insert(ColorDot(corner_pos, "red"));
  //ai_status.assign("LOOK AT green RED POS ERROR");
  //std::this_thread::sleep_for(std::chrono::milliseconds(300000));
  //throw ExceptionFreeserf("util_get_dir_from_corner failed to find the Direction from center_pos to corner_pos!  This should never happen");
  return DirectionNone;
}


// return count of terrain tiles of the specified type range, like hills (Tundra)
unsigned int
AI::count_terrain_near_pos(MapPos center_pos, unsigned int distance, Map::Terrain res_start_index, Map::Terrain res_end_index, std::string color) {
  AILogDebug["util_count_terrain_near_pos"] << name << " inside count_terrain_near_pos";
  //AILogDebug["util_count_terrain_near_pos"] << name << " AI: inside AI::count_terrain_near_pos";
  //AILogDebug["util_count_terrain_near_pos"] << name << " AI: center_pos " << center_pos << ", distance " << distance << ", res_start_index " << NameTerrain[res_start_index] << ", res_end_index " << NameTerrain[res_end_index];
  unsigned int count = 0;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    //AILogDebug["util_count_terrain_near_pos"] << name << " AI: terrain at pos " << pos << " has type " << terrain;
    if (AI::has_terrain_type(game, pos, res_start_index, res_end_index)) {
      //AILogDebug["util_count_terrain_near_pos"] << name << " AI: found matching terrain at pos " << pos;
      ++count;
    }
  }
  //AILogDebug["util_count_terrain_near_pos"] << name << " AI: found count " << count << " matching terrain of types " << NameTerrain[res_start_index] << " - " << NameTerrain[res_end_index];
  return count;
}

// return count of terrain tiles of the specified type range, like hills (Tundra)...
//   ... that have NO hard objects on them (flags, buildings, stones, etc.).  Paths are acceptable
//  initially only used for hill/sign ratios
// this function is almost identical to count_farmable_land except it doesn't include existing wheat fiels
//  maybe merge them?  Or make this one geologist/hill-specific
unsigned int
AI::count_empty_terrain_near_pos(MapPos center_pos, unsigned int distance, Map::Terrain res_start_index, Map::Terrain res_end_index, std::string color) {
  AILogDebug["util_count_empty_terrain_near_pos"] << name << " inside count_terrain_near_pos";
  //AILogDebug["util_count_empty_terrain_near_pos"] << name << " AI: inside AI::count_empty_terrain_near_pos";
  //AILogDebug["util_count_empty_terrain_near_pos"] << name << " AI: center_pos " << center_pos << ", distance " << distance << ", res_start_index " << NameTerrain[res_start_index] << ", res_end_index " << NameTerrain[res_end_index];
  unsigned int count = 0;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    //AILogDebug["util_count_empty_terrain_near_pos"] << name << " AI: terrain at pos " << pos << " has type " << terrain;
    if (AI::has_terrain_type(game, pos, res_start_index, res_end_index)) {
      Map::Object obj_type = map->get_obj(pos);
      // exclude tiles with blocking objects (anything not on this list)
      if (obj_type == Map::ObjectNone
        || (obj_type >= Map::ObjectFieldExpired && obj_type <= Map::ObjectSignSmallStone)) {
        //AILogDebug["util_count_empty_terrain_near_pos"] << name << " AI: found matching empty terrain at pos " << pos;
        ++count;
      }
    }
  }
  //AILogDebug["util_count_empty_terrain_near_pos"] << name << " AI: found count " << count << " matching empty terrain of types " << NameTerrain[res_start_index] << " - " << NameTerrain[res_end_index];
  return count;
}

// count farmable land  - count the number of grass tiles, with no paths,
//     and no obstacles (other than existing wheat fields)
unsigned int
AI::count_farmable_land(MapPos center_pos, unsigned int distance, std::string color) {
  Map::Terrain res_start_index = Map::TerrainGrass0;
  Map::Terrain res_end_index = Map::TerrainGrass3;
  AILogDebug["util_count_farmable_land"] << name << " inside AI::count_farmable_land";
  //AILogDebug["util_count_farmable_land"] << name << " center_pos " << center_pos << ", distance " << distance << ", res_start_index " << NameTerrain[res_start_index] << ", res_end_index " << NameTerrain[res_end_index];
  unsigned int count = 0;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    //AILogDebug["util_count_farmable_land"] << name << " AI: terrain at pos " << pos << " has type " << terrain;
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
  //AILogDebug["util_count_farmable_land"] << name << " AI: found count " << count << " matching terrain of types " << NameTerrain[res_start_index] << " - " << NameTerrain[res_end_index];
  AILogDebug["util_count_farmable_land"] << name << " done count_farmable_land, returning count " << count;
  return count;
}


// sort a MapPosSet by value, ascending, and return as sorted vector of the keys (throwing the values away)
MapPosVector
AI::sort_by_val_asc(MapPosSet set) {
  AILogDebug["util_sort_by_val_asc"] << name << " inside AI::sort_by_val_asc";
  // this is C++14 only?  worked on visual studio 2017 on windows but not VC code w/gcc on linux
  auto cmp = [](const auto &p1, const auto &p2) { return p2.second > p1.second || !(p1.second > p2.second) && p1.first > p2.first; };
  std::set< std::pair<MapPos, unsigned int>, decltype(cmp)> sorted_set(set.begin(), set.end(), cmp);
  MapPosVector sorted_vector;
  for (auto pair : sorted_set) {
    //AILogDebug["util_sort_by_val_asc"] << name << " sorted MapPosSet pos: " << pair.first << ", val: " << pair.second;
    sorted_vector.push_back(pair.first);
  }
  return sorted_vector;
}

// sort a MapPosSet by value, descending, and return as sorted vector of the keys (throwing the values away)
MapPosVector
AI::sort_by_val_desc(MapPosSet set) {
  AILogDebug["util_sort_by_val_desc"] << name << " inside AI::sort_by_val_desc";
  auto cmp = [](const auto &p1, const auto &p2) { return p2.second < p1.second || !(p1.second < p2.second) && p1.first < p2.first; };
  std::set< std::pair<MapPos, unsigned int>, decltype(cmp)> sorted_set(set.begin(), set.end(), cmp);
  MapPosVector sorted_vector;
  for (auto pair : sorted_set) {
    //AILogDebug["util_sort_by_val_desc"] << name << " sorted MapPosSet pos: " << pair.first << ", val: " << pair.second;
    sorted_vector.push_back(pair.first);
  }
  return sorted_vector;
}

// return count of individual objects of the specified type range, such as trees or geologist signs
unsigned int
AI::count_objects_near_pos(MapPos center_pos, unsigned int distance, Map::Object res_start_index, Map::Object res_end_index, std::string color) {
  AILogDebug["util_count_objects_near_pos"] << name << " inside AI::count_objects_near_pos";
  //AILogDebug["util_count_objects_near_pos"] << name << " AI: center_pos " << center_pos << ", distance " << distance << ", res_start_index " << res_start_index << "(" << NameObject[res_start_index] << ")"
  //      << ", res_end_index " << res_end_index << "(" << NameObject[res_end_index] << ")";
  unsigned int count = 0;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    if (map->get_obj(pos) >= res_start_index && map->get_obj(pos) <= res_end_index) {
      ++count;
      //AILogDebug["util_count_objects_near_pos"] << name << " AI: found matching object at pos " << pos << ", type " << map->get_obj(pos);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(0));
  }
  //AILogDebug["util_count_objects_near_pos"] << name << " AI: found count " << count << " matching objects of types " << NameObject[res_start_index] << " - " << NameObject[res_end_index];
  return count;
}


// build specified building in first valid pos near center_pos, return pos where built
//  BEFORE ACTUALLY PLACING THE BUILDING, plan how to connect it to the road system. 
//  if it cannot be connected, do not build in that spot. try to build until search exhausted
//
// BUG - the road pathfinding must be made to exclude the pos that the building would be placed in!
//  otherwise, the road meant for the building can itself prevent the building from being placed!
//  adding a new roadoption for this
//
MapPos
AI::build_near_pos(MapPos center_pos, unsigned int distance, Building::Type building_type, Direction optional_fill_dir) {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();

  AILogDebug["util_build_near_pos"] << name << " inside AI::build_near_pos with building type index " << NameBuilding[building_type] << ", distance " << distance << ", target pos " << center_pos << ", inventory_pos " << inventory_pos;

  // this function is being changed from using the *nearest* Inventory to using the *current*
  //  inventory, does that introduce any problems?
  if (building_type == Building::TypeHut) {
    if (stock_buildings.at(inventory_pos).unfinished_hut_count >= max_unfinished_huts) {
      AILogDebug["util_build_near_pos"] << name << " max unfinished huts limit " << max_unfinished_huts << " reached, not building";
      // should be returning not_built_pos or bad_map pos?? stopbuilding is less appropriate with separate counts set up for huts vs other buildings
      return stopbuilding_pos;
    }
  }

  // always be willing to build wood/stone buildings and knight huts or everything can grind to a halt
  //   also Mines, but don't connect these to the road network yet
  if (stock_buildings.at(inventory_pos).unfinished_count >= max_unfinished_buildings && building_type != Building::TypeSawmill
  //if (unfinished_building_count >= max_unfinished_buildings && building_type != Building::TypeSawmill
    && building_type != Building::TypeLumberjack && building_type != Building::TypeStonecutter
    && building_type != Building::TypeHut && building_type != Building::TypeCoalMine
    && building_type != Building::TypeIronMine && building_type != Building::TypeGoldMine
    && building_type != Building::TypeStoneMine && building_type != Building::TypeForester
    ) {
    AILogDebug["util_build_near_pos"] << name << " max unfinished buildings limit " << max_unfinished_buildings << " reached, not building";
    return stopbuilding_pos;
  }

  // don't build large civilian buildings, including stocks, near enemy borders
  if (building_type >= Building::TypeStock && building_type != Building::TypeHut &&
      building_type != Building::TypeTower && building_type != Building::TypeForester){
    AILogDebug["util_build_near_pos"] << name << " this is a Large Civilian building, checking to see if enemy borders are near";
    for (unsigned int i = 0; i < AI::spiral_dist(8); i++) {
      MapPos pos = map->pos_add_extended_spirally(center_pos, i);
      // if not mine and not unowned (i.e. owned by somebody else)
      if (map->get_owner(pos) != player_index && map->get_owner(pos) != -1) {
        AILogDebug["util_build_near_pos"] << name << " found enemy borders near this potential build_near_pos center and this is a Large Civilian building!  Not building in this area, returning";
        return bad_map_pos;
      }
    }
  }
  // if this is an economy building (except Fisher which isn't counted), make sure the actual building position 
  //  is closest to the currently "selected" stock so that this building becomes "attached" to the stock.
  //  Otherwise, the stock doesn't see this building as part of its range and will keep building more
  bool verify_stock = false;
  if (building_type != Building::TypeFisher && building_type != Building::TypeStock && 
      building_type != Building::TypeHut && building_type != Building::TypeTower && 
      building_type != Building::TypeFortress && building_type != Building::TypeCastle){
    AILogDebug["util_build_near_pos"] << name << " this is a tracked economy building of type " << NameBuilding[building_type] << ", setting verify_stock to true";
    verify_stock = true;
  }

  bool is_mine = false;
  // Mines are to be built, but not connected immediately so they can be placed before geologist signs fade,
  //   even if player's economy doesn't yet require the mine.  Used to establish good mine locations early
  if (building_type == Building::TypeCoalMine
    || building_type == Building::TypeIronMine
    || building_type == Building::TypeGoldMine
    || building_type == Building::TypeStoneMine) {
    AILogDebug["util_build_near_pos"] << name << " this is a Mine, setting is_mine to true";
    is_mine = true;
  }

  MapPos built_pos = bad_map_pos;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = bad_map_pos;
    // normally use spiral, but instead use directional fill (outside first)
    //  if a direction is specified. 
    // if you wanted to do inside first fill, or some rotated pattern, you could just specify
    //  the dir that corresponds to that dir's "outside first" fill pattern
    if (optional_fill_dir != DirectionNone){
      // directional fill only supports ~spiral4 right now (52 positions)
      //  quit early with a warning if a larger number specified
      if (i > DIRECTIONAL_FILL_POS_MAX){
        AILogWarn["build_near_pos"] << name << " DIRECTIONAL FILL ONLY WORKS FOR ~SPIRAL-4 DIST! " << DIRECTIONAL_FILL_POS_MAX << " POSITIONS, limit reached, quitting here";
        return bad_map_pos;
      }
      pos = map->pos_add_directional_fill(center_pos, i, optional_fill_dir);
    }else{
      pos = map->pos_add_extended_spirally(center_pos, i);
    }
    if (map->get_owner(pos) != player_index) {
      continue;
    }
    // special bug work-around: do not build a Stonecutter down-right from a Stone deposit because the stonecutter cannot use it and stonecutting logic breaks
    //
    // THIS SHOULD BE REMOVED - because it isn't just if the stonecutter hut is down-right from the stone pile,
    //  if ANY building is down-right from the stone pile it cannot be harvested.  So, I believe I now have 
    //  logic in the stone pile counting that accounts for any building blocking the pile, so it is not counted,
    //  which means that this check should no longer be needed
    //
    /* removing this
    if (building_type == Building::TypeStonecutter) {
      // check to see if there are stones up-left from this pos
      MapPos upleft_pos = map->move_up_left(pos);
      int obj_type = map->get_obj(upleft_pos);
      if (obj_type >= Map::ObjectStone0 && obj_type <= Map::ObjectStone7) {
        AILogDebug["util_build_near_pos"] << name << " SPECIAL BUG WORKAROUND - pos up-left from potential stonecutter at pos " << pos << " is type " << NameObject[obj_type] << ", not building here";
        continue;
      }
    }
    */
    if (game->can_build_building(pos, building_type, player)) {
      AILogDebug["util_build_near_pos"] << name << " inside AI::build_near_pos, can build " << NameBuilding[building_type] << " at pos " << pos;
    }
    else {
      // simply can't build here
      //AILogDebug["util_build_near_pos"] << name << " cannot build " << NameBuilding[building_type] << " of sizetype " << building_sizes[building_type] << " at pos " << pos;
      continue;
    }
    // avoid known bad positions for this building type.  note this list is NOT persisted if AI thread goes away or game load/restart
    if (is_bad_building_pos(pos, building_type)) {
      AILogDebug["util_build_near_pos"] << name << " skipping this pos, it is marked in bad_building_pos list";
      continue;
    }
    // avoid building knight huts too close together
    if (building_type == Building::TypeHut && AI::building_exists_near_pos(pos, AI::spiral_dist(5), Building::TypeHut)) {
      AILogDebug["util_build_near_pos"] << name << " a knight hut already exists near pos " << pos << ", skipping this pos";
      continue;
    }

    // FIRST, try to place a FLAG that results in a good road connection
    AILogDebug["util_build_near_pos"] << name << " trying to place a flag that can connect this building";
    MapPos flag_pos = map->move_down_right(pos);
    bool road_built = false;

    if (is_mine){
      AILogDebug["util_build_near_pos"] << name << " this is a mine, skipping flag/road checks as this will not be connected to the road system yet";
    }else{

      if (!map->has_flag(flag_pos)){
        AILogDebug["util_build_near_pos"] << name << " no flag yet exists at flag_pos " << flag_pos << " for potential new building of type " << NameBuilding[building_type] << " at pos " << pos << ", trying to build one";
        if (!game->build_flag(flag_pos, player)){
          AILogWarn["util_build_near_pos"] << name << " failed to build flag at flag_pos " << flag_pos << " for potential new building of type " << NameBuilding[building_type] << " at pos " << pos << "!  skipping this pos";
          continue;
        }
        AILogDebug["util_build_near_pos"] << name << " successfully built flag at flag_pos " << flag_pos << " for potential new building of type " << NameBuilding[building_type] << " at pos " << pos;
      }

      if (!game->get_flag_at_pos(flag_pos)->is_connected()) {
        AILogDebug["util_build_near_pos"] << name << " the flag already at flag_pos " << flag_pos << " is not yet connected.  For potential new building of type " << NameBuilding[building_type] << " at pos " << pos << ", trying to connect it to road system";
        //
        // build road to connect this potential building
        //
        //  this may include a check for nearest inventory and disqualify the build if
        //  the closest Inventory building by flagsearch is not the current one
        //
        road_built = AI::build_best_road(flag_pos, road_options, building_type, Building::TypeNone, bad_map_pos, verify_stock);
      }else{
        AILogDebug["util_build_near_pos"] << name << " the flag already at flag_pos " << flag_pos << " is already connected to road system.  For potential new building of type " << NameBuilding[building_type] << " at pos " << pos;
      }

      if (!road_built && !game->get_flag_at_pos(flag_pos)->is_connected()) {
        AILogDebug["util_build_near_pos"] << name << " failed to connect flag at flag_pos " << flag_pos << " to road network! removing it.  For potential new building of type " << NameBuilding[building_type];
        // disabling this for now until I see more significant road building issues
        //  jan19 2021
        //AILogDebug["util_build_near_pos"] << name << " LOOK AT BUILDING AT POS " << pos << ", MARKED IN CYAN";
        //ai_mark_pos.erase(pos);
        //ai_mark_pos.insert(std::make_pair(pos, "cyan"));
        //std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        AILogDebug["util_build_near_pos"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->demolish_flag (build_near_pos failed to connect)";
        game->get_mutex()->lock();
        AILogDebug["util_build_near_pos"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->demolish_flag (build_near_pos failed to connect)";
        game->demolish_flag(flag_pos, player);
        AILogDebug["util_build_near_pos"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->demolish_flag (build_near_pos failed to connect)";
        game->get_mutex()->unlock();
        AILogDebug["util_build_near_pos"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->demolish_flag (build_near_pos failed to connect)";
        // mark as bad pos, to avoid repeateadly rebuilding same building in same spot
        bad_building_pos.insert(std::make_pair(pos, building_type));
		// try the next position
		continue;
      }else{
        AILogDebug["util_build_near_pos"] << name << " successfully connected flag at flag_pos " << flag_pos << " to road network.  For potential new building of type " << NameBuilding[building_type];
        //if (building_type == Building::TypeHut) {
        //  stock_buildings.at(inventory_pos).unfinished_hut_count++;
        //  AILogDebug["util_build_near_pos"] << name << " incrementing unfinished_hut_count, is now: " << unfinished_hut_count;
        //}
        //else {
        //  stock_buildings.at(inventory_pos).unfinished_count++;
        //  AILogDebug["util_build_near_pos"] << name << " found unfinished " << NameBuilding[building_type] << ", incrementing unfinished_building_count, is now: " << unfinished_building_count;
        //}
        //duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
        //AILogDebug["util_build_near_pos"] << name << " successful util_build_near_pos for building of type " << NameBuilding[building_type] << " call took " << duration;
        //return pos;
      }
    } // if is_mine

    // try to build it
    AILogDebug["util_build_near_pos"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->build_building (build_near_pos) of type " << NameBuilding[building_type];
    game->get_mutex()->lock();
    AILogDebug["util_build_near_pos"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->build_building (build_near_pos) of type " << NameBuilding[building_type];
    bool was_built = game->build_building(pos, building_type, player);
    AILogDebug["util_build_near_pos"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->build_building (build_near_pos) of type " << NameBuilding[building_type];
    game->get_mutex()->unlock();
    AILogDebug["util_build_near_pos"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->build_building (build_near_pos) of type " << NameBuilding[building_type];
    if (!was_built) {
      AILogDebug["util_build_near_pos"] << name << " failed to build building of type " << NameBuilding[building_type] << " despite can_build being true!  WAITING 10sec - look at the pos in coral!";
      ai_mark_pos.erase(pos);
      ai_mark_pos.insert(std::make_pair(pos, "coral"));
      std::this_thread::sleep_for(std::chrono::milliseconds(10000));
      continue;
    }

    AILogDebug["util_build_near_pos"] << name << " successfully built and connected a building of type " << NameBuilding[building_type] << " at pos " << pos;

    // increment unfinished building counts
    if (building_type == Building::TypeHut) {
      stock_buildings.at(inventory_pos).unfinished_hut_count++;
      AILogDebug["util_build_near_pos"] << name << " incrementing unfinished_hut_count, is now: " << stock_buildings.at(inventory_pos).unfinished_hut_count;
    }
    else {
      stock_buildings.at(inventory_pos).unfinished_count++;
      AILogDebug["util_build_near_pos"] << name << " found unfinished " << NameBuilding[building_type] << ", incrementing unfinished_building_count, is now: " << stock_buildings.at(inventory_pos).unfinished_count;
    }

    duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    AILogDebug["util_build_near_pos"] << name << " successful util_build_near_pos, built building of type " << NameBuilding[building_type] << " at pos " << pos << ", call took " << duration;

    // sleep a bit to be more human-like
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    return pos;
  }

  AILogDebug["util_build_near_pos"] << name << " could not find a place to build, or failed to build, type " << NameBuilding[building_type] << " near pos " << center_pos << " after checking " << distance << " positions";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_build_near_pos"] << name << " failed util_build_near_pos for building of type " << NameBuilding[building_type] << " call took " << duration;
  return notplaced_pos;

}

// return count of total stones in the area.  A stone pile can have 1-8 stones
// ALSO, have to work around the original-game bug where a stonecutter cannot harvest stones if a building is directly down-right from a stone pile
//   To avoid this, do not count stones that have a building down-right from the pile
unsigned int
AI::count_stones_near_pos(MapPos center_pos, unsigned int distance) {
  AILogDebug["util_count_stones_near_pos"] << name << " inside count_stones_near_pos";
  Map::Object res_start_index = Map::ObjectStone0;
  Map::Object res_end_index = Map::ObjectStone7;
  std::string color = "gray";
  //AILogDebug["util_count_stones_near_pos"] << name << " AI: center_pos " << center_pos << ", distance " << distance << ", res_start_index " << NameObject[res_start_index] << ", res_end_index " << NameObject[res_end_index];
  unsigned int total = 0;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    if (map->get_obj(pos) >= res_start_index && map->get_obj(pos) <= res_end_index) {
      //AILogDebug["util_count_stones_near_pos"] << name << " getting value of map object at pos: " << pos << " with map obj type " << NameObject[map->get_obj(pos)];
      // because the higher MapObject indexes represent smaller resource values, the difference is negative
      //   so invert the difference and add one, because it starts at zero
      int value = 1 + (-1 * (map->get_obj(pos) - res_end_index));
      //AILogDebug["util_count_stones_near_pos"] << name << " map object has resource value of " << value << " at pos " << pos;
      // bug work-around - ignore stones that cannot be harvested because a building is down-right from the pile
      if (map->has_building(map->move_down_right(pos))) {
        // this seems to be working as expected
        AILogDebug["util_count_stones_near_pos"] << name << " stones bug workaround - ignoring stone pile at pos " << pos << " because a building is down-right from it so it cannot be harvested";
        value = 0;
      }
      total += value;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(0));
  }
  //AILogDebug["util_count_stones_near_pos"] << name << " AI: found total value " << total << " of objects types " << NameObject[res_start_index] << " - " << NameObject[res_end_index];
  return total;
}

// count the number of knights that would be sent out if occupation level is changed, to avoid depleting the reserve of idle_knights
//  this function only counts the **TARGET** levels, ignoring if that many knights are actually available and/or currently in buildings
unsigned int
AI::count_knights_affected_by_occupation_level_change(unsigned int current_level, unsigned int new_level) {
  AILogDebug["util_count_knights_affected_by_occupation_level_change"] << name << " inside count_knights_affected_by_occupation_level_change, level " << current_level << " -> " << new_level;
  // calling update_building_counts here is too slow because of mutex locking and causes do_manage_knight_occupation_levels to leave things at zero
  //   for long enough that the game update actually starts moving knights around.  Having a super accurate count is not critical here, it should
  //   be able to use whatever values were found during the most recent update, which I believe is happening at the start of each AI loop anyway
  //update_building_counts();
  // note this includes incomplete buildings? I think.  I doubt it matters much, though
  unsigned int hut_count = realm_building_count[Building::TypeHut];
  unsigned int tower_count = realm_building_count[Building::TypeTower];
  unsigned int garrison_count = realm_building_count[Building::TypeFortress];
  AILogDebug["util_count_knights_affected_by_occupation_level_change"] << name << " military building counts: huts " << hut_count << ", towers " << tower_count << ", garrisons " << garrison_count;
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
    AILogDebug["util_count_knights_affected_by_occupation_level_change"] << name << " diff " << diff << " is negative, setting to zero instead";
    diff = 0;
  }
  AILogDebug["util_count_knights_affected_by_occupation_level_change"] << name << " knight counts if at fully-occupied-*target* staffing level: current " << total_current << ", new " << total_new << ", diff " << diff;
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
  AILogDebug["util_expand_borders"] << name << " inside AI::expand_borders for inventory_pos " << inventory_pos;
  ai_status.assign("EXPANDING BORDERS");

  if (stock_buildings.at(inventory_pos).unfinished_hut_count >= max_unfinished_buildings) {
    AILogDebug["util_expand_borders"] << name << " max unfinished huts limit " << max_unfinished_huts << " reached, not building";
    // should be returning not_built_pos or bad_map pos?? stopbuilding is less appropriate with separate counts set up for huts vs other buildings
    return stopbuilding_pos;
  }
  if (cannot_expand_borders_this_loop) {
    AILogDebug["util_expand_borders"] << name << " cannot_expand_borders_this_loop is true, not trying again until next loop";
    // should be returning not_built_pos or bad_map pos?? stopbuilding is less appropriate with separate counts set up for huts vs other buildings
    return stopbuilding_pos;
  }
  // don't expand borders if running out of knights AND already have all 3 mine types
  unsigned int idle_knights = serfs_idle[Serf::TypeKnight0] + serfs_idle[Serf::TypeKnight1] + serfs_idle[Serf::TypeKnight2] + serfs_idle[Serf::TypeKnight3] + serfs_idle[Serf::TypeKnight4];
  if (idle_knights <= knights_min) {
    AILogDebug["util_expand_borders"] << name << " running low on idle_knights, checking to see if already have all three mine types";
    if (realm_building_count[Building::TypeCoalMine] > 0 &&
        realm_building_count[Building::TypeIronMine] > 0 &&
        realm_building_count[Building::TypeGoldMine] > 0){
      AILogDebug["util_expand_borders"] << name << " running low on idle_knights and have at least one of each mine type, not expanding borders";
      return stopbuilding_pos;
    }
  }

  MapPos built_pos = bad_map_pos;
  MapPosSet count_by_corner;

  for (std::string goal : expand_towards) {
    AILogDebug["util_expand_borders"] << name << " expand_towards goal list includes item: " << goal;
  }

  // search outward from each military building until border pos reached
  //  then score that area
  for (MapPos center_pos : stock_buildings.at(inventory_pos).occupied_military_pos) {
    duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    AILogDebug["util_expand_borders"] << name << " about to check around pos " << center_pos << ", SO FAR util_expand_borders call took " << duration;
    // find territory edge in each direction
    for (Direction dir : cycle_directions_rand_cw()) {
      MapPos pos = center_pos;
      AILogDebug["util_expand_borders"] << name << " looking for territory edge from pos " << pos << " in direction " << dir << " / " << NameDirection[dir];
      // give up after 10 tiles because we should only be looking from huts that are right on the borders, not internal ones
      //  these huts should be at most 8 tiles or so from the border
      // Without this optimization every single dir from every hut is followed to borders and area scored, resulting
      //  in a lot of duplicate scoring
      // Also, it makes the "check for circumnavigating the globe" obsolete because that is way more than ten tiles
      for (int tiles_moved = 0; tiles_moved < 11; tiles_moved++){
        pos = map->move(pos, dir);
        if (map->get_owner(pos) != player_index){
          unsigned int score = AI::score_area(pos, AI::spiral_dist(6));
          AILogDebug["util_expand_borders"] << name << " border in direction " << dir << " has score " << score;
          // include all corners so we'll at least expand *somewhere* even if no goal resources found
          //   the sort function will still prefer areas with resources
          count_by_corner.insert(std::make_pair(pos, score));
          break;
        }
      }
    }
  }
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  MapPosVector search_positions = AI::sort_by_val_desc(count_by_corner);
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  for (MapPos corner_pos : search_positions) {
    AILogDebug["util_expand_borders"] << name << " try to build knight hut near pos " << corner_pos;

    duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    AILogDebug["util_expand_borders"] << name << " SO FAR util_expand_borders call took " << duration;

    built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(4), Building::TypeHut);
    if (built_pos == stopbuilding_pos) {
      cannot_expand_borders_this_loop = true;

      duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
      AILogDebug["util_expand_borders"] << name << " done util_expand_borders call took " << duration;

      return stopbuilding_pos;
    }
    
  }

  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_expand_borders"] << name << " done util_expand_borders call took " << duration;
  if (built_pos == bad_map_pos || built_pos == notplaced_pos || built_pos == stopbuilding_pos) {
    AILogDebug["util_expand_borders"] << name << " couldn't place knight hut";
    cannot_expand_borders_this_loop = true;
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
  AILogDebug["util_score_area"] << name << " inside AI::score_area, center_pos " << center_pos << ", distance " << distance;
  unsigned int total_value = 0;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    Map::Object obj = map->get_obj(pos);
    //AILogDebug["util_score_area"] << name << " at pos " << pos << " with object type " << NameObject[obj];
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
      AILogDebug["util_score_area"] << name << " adding foods count 1 with value " << expand_towards.count("foods") * foods_weight;
    }
    if (obj >= Map::ObjectTree0 && obj <= Map::ObjectPine7) {
      pos_value += expand_towards.count("trees") * trees_weight;
      AILogDebug["util_score_area"] << name << " adding trees count 1 with value " << expand_towards.count("trees") * trees_weight;
    }
    if (obj >= Map::ObjectStone0 && obj <= Map::ObjectStone7) {
      int stonepile_value = 1 + (-1 * (obj - Map::ObjectStone7));
      pos_value += expand_towards.count("stones") * stones_weight * stonepile_value;
      AILogDebug["util_score_area"] << name << " adding stones count " << stonepile_value << " with value " << expand_towards.count("stones") * stones_weight;
    }
    if (AI::has_terrain_type(game, pos, Map::TerrainTundra0, Map::TerrainSnow0)) {
      if (obj >= Map::ObjectSignEmpty && obj <= Map::ObjectSignSmallStone) {
        AILogDebug["util_score_area"] << name << " found a sign (of type " << NameObject[obj] << "), not counting this hill";
      }
      else {
        pos_value += expand_towards.count("hills") * hills_weight;
        AILogDebug["util_score_area"] << name << " adding hills count 1 with value " << expand_towards.count("hills") * hills_weight;
      }
    }
    if (obj == Map::ObjectSignLargeGold) { gold_signs += 3; }
    if (obj == Map::ObjectSignSmallGold) { gold_signs += 1; }
    if (obj == Map::ObjectSignSmallGold || obj == Map::ObjectSignLargeGold) {
      pos_value += expand_towards.count("gold_ore") * gold_ore_weight * gold_signs;
      AILogDebug["util_score_area"] << name << " adding gold_ore count " << gold_signs << " with value " << expand_towards.count("gold_ore") * gold_ore_weight * gold_signs;
    }
    if (obj == Map::ObjectSignLargeIron) { iron_signs += 3; }
    if (obj == Map::ObjectSignSmallIron) { iron_signs += 1; }
    if (obj == Map::ObjectSignSmallIron || obj == Map::ObjectSignLargeIron) {
      pos_value += expand_towards.count("iron_ore") * iron_ore_weight * iron_signs;
      AILogDebug["util_score_area"] << name << " adding iron_ore count " << iron_signs << " with value " << expand_towards.count("iron_ore") * iron_ore_weight * iron_signs;
    }
    if (obj == Map::ObjectSignLargeCoal) { coal_signs += 3; }
    if (obj == Map::ObjectSignSmallCoal) { coal_signs += 1; }
    if (obj == Map::ObjectSignSmallCoal || obj == Map::ObjectSignLargeCoal) {
      pos_value += expand_towards.count("coal")      * coal_weight * coal_signs;
      AILogDebug["util_score_area"] << name << " adding coal count " << coal_signs << " with value " << expand_towards.count("coal_ore") * coal_weight * coal_signs;
    }
    if (obj == Map::ObjectSignLargeStone) { stone_signs += 3; }
    if (obj == Map::ObjectSignSmallStone) { stone_signs += 1; }
    if (obj == Map::ObjectSignSmallStone || obj == Map::ObjectSignLargeStone) {
      pos_value += expand_towards.count("stones") * stone_signs_weight * stone_signs;
      AILogDebug["util_score_area"] << name << " adding stones count " << stone_signs << " with value " << expand_towards.count("stones") * stone_signs_weight * stone_signs;
    }
    // I think scoring_warehouse is deprecated for now, could bring it back if it really helps
    if (!scoring_warehouse) {
      //
      // defense - build knight huts to buffer borders
      //    prioritizing areas with own civ buildings, or any place that enemy territory found
      //
      if (map->get_owner(pos) == player_index) {
        if (obj == Map::ObjectLargeBuilding && !game->get_building_at_pos(pos)->is_military()) {
          pos_value += expand_towards.count("create_buffer") * 3;
          AILogDebug["util_score_area"] << name << " adding defensive_buffer building value x3 for large civilian building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
        }
        if (obj == Map::ObjectSmallBuilding && !game->get_building_at_pos(pos)->is_military()) {
          pos_value += expand_towards.count("create_buffer") * 1;
          AILogDebug["util_score_area"] << name << " adding defensive_buffer building value x1 for small civilian building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
        }
      }
      // does -1 mean unowned?   need to check
      //  this code does seem to work so that must be the case
      if (map->get_owner(pos) != player_index && map->get_owner(pos) != -1) {
        pos_value += expand_towards.count("create_buffer") * 1;
        AILogDebug["util_score_area"] << name << " adding defensive_buffer value for enemy territory";
      }
    }
    //
    // offense
    //
    if (scoring_attack
      && map->get_owner(pos) != player_index
      && map->get_owner(pos) != -1
      && map->has_building(pos)
      ) {
      //AILogDebug["util_score_area"] << name << " potential attack object is " << NameObject[obj] << " with building type " << NameBuilding[game->get_building_at_pos(pos)->get_type()];
      if (obj == Map::ObjectLargeBuilding && !game->get_building_at_pos(pos)->is_military()) {
        pos_value += 3;
        AILogDebug["util_score_area"] << name << " adding attack value x3 for large civilian building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
      }
      if (obj == Map::ObjectSmallBuilding && game->get_building_at_pos(pos)->get_type() == Building::TypeCoalMine){
        pos_value += 2;
        AILogDebug["util_score_area"] << name << " adding additional attack value x2 for building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
      }
      if (obj == Map::ObjectSmallBuilding && game->get_building_at_pos(pos)->get_type() == Building::TypeIronMine) {
        pos_value += 4;
        AILogDebug["util_score_area"] << name << " adding additional attack value x4 for building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
      }
      if (obj == Map::ObjectSmallBuilding && game->get_building_at_pos(pos)->get_type() == Building::TypeGoldMine) {
        pos_value += 6;
        AILogDebug["util_score_area"] << name << " adding additional attack value x6 for building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
      }
      if (obj == Map::ObjectSmallBuilding && !game->get_building_at_pos(pos)->is_military()) {
        pos_value += 1;
        AILogDebug["util_score_area"] << name << " adding attack value x1 for small civilian building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
      }
    }
    //
    // total it up
    //
    total_value += static_cast<unsigned int>(pos_value);
    AILogDebug["util_score_area"] << name << " score total so far: " << total_value;

  }
  AILogDebug["util_score_area"] << name << " found total score_area value " << total_value << " of terrain & objects in area " << center_pos;
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_score_area"] << name << " done util_score_area call took " << duration;
  return total_value;
}


bool
AI::is_bad_building_pos(MapPos pos, Building::Type building_type) {
  AILogDebug["util_is_bad_building_pos"] << name << " inside AI::is_bad_building_pos";
  if (bad_building_pos.find(std::make_pair(pos, building_type)) != bad_building_pos.end()) {
    return true;
  }
  return false;
}


// get halfway point between two MapPos
MapPos
AI::get_halfway_pos(MapPos start_pos, MapPos end_pos) {
  AILogDebug["util_get_halfway_pos"] << name << " inside get_halfway_pos";
  MapPos halfway_pos;
  int start_col = map->pos_col(start_pos);
  int start_row = map->pos_row(start_pos);
  AILogDebug["util_get_halfway_pos"] << name << " start_pos is " << start_pos << ", start_col is " << start_col << ", start_row is " << start_row;
  int end_col = map->pos_col(end_pos);
  int end_row = map->pos_row(end_pos);
  AILogDebug["util_get_halfway_pos"] << name << " end_col is " << end_col << ", end_row is " << end_row;
  // it seems these are reversed start/end pos
  int col_dist = map->dist_x(start_pos, end_pos);
  AILogDebug["util_get_halfway_pos"] << name << " col_dist is " << col_dist;
  int row_dist = map->dist_y(start_pos, end_pos);
  AILogDebug["util_get_halfway_pos"] << name << " row_dist is " << row_dist;
  int halfway_col_dist = (map->dist_x(start_pos, end_pos) / 2);
  AILogDebug["util_get_halfway_pos"] << name << " halfway_col_dist is " << halfway_col_dist;
  int halfway_row_dist = (map->dist_y(start_pos, end_pos) / 2);
  AILogDebug["util_get_halfway_pos"] << name << " halfway_row_dist is " << halfway_row_dist;
  halfway_pos = map->pos(end_col + halfway_col_dist, end_row + halfway_row_dist);
  AILogDebug["util_get_halfway_pos"] << name << " halfway_pos is " << halfway_pos;
  return halfway_pos;
}


/* deprecating this and replacing it with an "internal game style" FlagSearch of the new
//   type find_nearest_inventory_for_res_producer()
//
// return the MapPos of the stock nearest to the specified MapPos, including castle
//   if not found, default to castle_pos?
MapPos
AI::find_nearest_inventory(MapPos pos) {
  AILogDebug["util_find_nearest_inventory"] << name << " inside find_nearest_inventory to pos " << pos;
  // try searching by straightline pos instead of spirally
  unsigned int best_dist = bad_score;
  MapPos closest_stock = bad_map_pos;
  for (MapPos inventory_pos : stocks_pos) {
    AILogDebug["util_find_nearest_inventory"] << name << " considering inventory_pos " << inventory_pos;
    unsigned int dist = AI::get_straightline_tile_dist(map, pos, inventory_pos);
    AILogDebug["util_find_nearest_inventory"] << name << " straightline tile dist from pos " << pos << " to inventory_pos " << inventory_pos << " is " << dist;
    if (dist < best_dist) {
      AILogDebug["util_find_nearest_inventory"] << name << " stock at inventory_pos " << inventory_pos << " is the closest so far to pos " << pos << ", with dist " << dist;
      best_dist = dist;
      closest_stock = inventory_pos;
    }
  }
  if (closest_stock == bad_map_pos || best_dist == bad_score) {
    AILogDebug["util_find_nearest_inventory"] << name << " not found??  closest_stock: " << closest_stock << ", best_dist: " << best_dist;
  }
  AILogDebug["util_find_nearest_inventory"] << name << " done find_nearest_inventory to pos " << pos << ", closest stock is " << closest_stock << " with straightline dist " << best_dist;
  return closest_stock;
}
*/

/* moving this to ai_pathfinder.cc
// nevermind, for now replacing this with a function that has the same input/output of MapPos
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
//  - no consideration of tile/straightline distance, only Flag distance matters
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
MapPos
AI::find_nearest_inventory(MapPos pos) {
  AILogDebug["util_find_nearest_inventory"] << name << " inside find_nearest_inventory to pos " << pos;
  if (!map->has_flag(pos) && !map->has_building(pos)){
    AILogWarn["util_find_nearest_inventory"] << name << " no flag or building found at pos " << pos << " as expected!  Cannot run search, returning bad_map_pos";
    return bad_map_pos;
  }
  MapPos flag_pos = bad_map_pos;
  if (map->has_building(pos)){
	flag_pos = map->move_down_right(pos);
    AILogDebug["util_find_nearest_inventory"] << name << " request pos " << pos << " is a building pos, using its flag pos " << flag_pos << " instead";
  }else{
    flag_pos = pos;
  }
  Flag *flag = game->get_flag_at_pos(flag_pos);
  if (flag == nullptr){
    AILogWarn["util_find_nearest_inventory"] << name << " got nullptr for Flag at flag_pos " << flag_pos << "!  Cannot run search, returning bad_map_pos";
    return bad_map_pos;
  }
  int inv_flag_index = flag->find_nearest_inventory_for_res_producer();
  if (inv_flag_index < 0){
    AILogDebug["util_find_nearest_inventory"] << name << " find_nearest_inventory_for_res_producer returned invalid result (flag index " << inv_flag_index << ") for flag_pos " << flag_pos << "!  maybe this flag isn't part of the road system??";
    return bad_map_pos;
  }
  Flag *inv_flag = game->get_flag(inv_flag_index);
  if (inv_flag == nullptr){
    AILogWarn["util_find_nearest_inventory"] << name << " got nullptr for Inventory Flag with index " << inv_flag_index << " found by flagsearch starting at flag_pos " << flag_pos << "!  returning bad_map_pos";
    return bad_map_pos;
  }
  MapPos inv_flag_pos = inv_flag->get_position();
  AILogDebug["util_find_nearest_inventory"] << name << " successful find_nearest_inventory_for_res_producer search for flag_pos " << flag_pos << ", returning Inventory flag pos " << inv_flag_pos;
  return inv_flag_pos;
}
*/


void
AI::score_enemy_targets(MapPosSet *scored_targets) {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  AILogDebug["util_score_enemy_targets"] << name << " inside score_enemy_targets";
  //
  // MAJOR BUG - this is running before expansion goals are set (later in the AI loop), so it is not looking at resources that could
  //      be taken, ONLY at enemy buildings.  Does it make sense to persist expansion goals between AI loops?  Or move attacks to the end of the loop?
  //  temp fix -  stupid hack to work-around it
  expand_towards = last_expand_towards;
  update_building_counts();
  // foreach my hut in range of attacking enemy
  //    foreach enemy hut within range of attack
  //      score
  // on game load, castle_pos is unknown even though castle exists, need to find it
  AILogDebug["util_score_enemy_targets"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for score_enemy_targets)";
  game->get_mutex()->lock();
  AILogDebug["util_score_enemy_targets"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for score_enemy_targets)";
  Game::ListBuildings buildings = game->get_player_buildings(player);
  AILogDebug["util_score_enemy_targets"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex before calling game->get_player_buildings(player) (for score_enemy_targets)";
  game->get_mutex()->unlock();
  AILogDebug["util_score_enemy_targets"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex before calling game->get_player_buildings(player) (for score_enemy_targets)";
  std::set<MapPos> unique_enemy_targets;
  for (Building *building : buildings) {
    if (!building->is_done() ||
      !building->is_military() ||
      !building->is_active() ||
      !(building->get_threat_level() == 3)) {
      continue;
    }
    ai_mark_pos.clear();
    AILogDebug["util_score_enemy_targets"] << name << " looking for enemy buildings to attack near to my building of " << NameBuilding[building->get_type()] << " at pos " << building->get_position();
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
        AILogDebug["util_score_enemy_targets"] << name << " target_building is nullptr!  at pos " << pos;
        continue;
      }
      if (!target_building->is_military() || !target_building->is_active()) {
        continue;
      }
      MapPos target_pos = pos;
      Building::Type target_building_type = building->get_type();
      size_t target_player_index = map->get_owner(pos);
      const std::string target_player_face = NamePlayerFace[game->get_player(map->get_owner(pos))->get_face()];
      AILogDebug["util_score_enemy_targets"] << name << " found attackable building of type " << NameBuilding[target_building_type] << " at pos " << target_pos << " belonging to player " << target_player_index << " / " << target_player_face;
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
      AILogDebug["util_score_enemy_targets"] << name << " send up to " << attacking_knights << " knights to attack enemy building of type " << NameBuilding[target_building_type]
        << " at pos " << target_pos << " belonging to player " << target_player_index << " / " << target_player_face;
      if (attacking_knights == 0) {
        AILogDebug["util_score_enemy_targets"] << name << " cannot send any knights, not marking target for scoring";
        continue;
      }
      AILogDebug["util_score_enemy_targets"] << name << " adding target_pos " << target_pos << " to unique_enemy_targets set to score";
      unique_enemy_targets.insert(target_pos);

    }
  }
  scoring_attack = true;
  for (MapPos target_pos : unique_enemy_targets){
    AILogDebug["util_score_enemy_targets"] << name << " scoring attackable building at pos " << target_pos;
    // debug
    AILogDebug["util_score_enemy_targets"] << name << " dumping attackable debug expand_towards";
    for (std::string goal : last_expand_towards) {
      AILogDebug["util_score_enemy_targets"] << name << " attackable ATTACKING (last_) last_expand_towards goal list includes item: " << goal;
    }
    for (std::string goal : expand_towards) {
      AILogDebug["util_score_enemy_targets"] << name << " attackable ATTACKING (last_) expand_towards goal list includes item: " << goal;
    }
    unsigned int score = AI::score_area(target_pos, AI::spiral_dist(8));
    AILogDebug["util_score_enemy_targets"] << name << " attackable enemy target at pos " << target_pos << " has score " << score;
    scored_targets->insert(std::make_pair(target_pos, score));
  }
  scoring_attack = false;
  // second part of stupid hack to work-around it
  expand_towards.clear();
  AILogDebug["util_score_enemy_targets"] << name << " done score_enemy_targets";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_score_enemy_targets"] << name << " done score_enemy_targets call took " << duration;
}



void
AI::attack_nearest_target(MapPosSet *scored_targets) {
  AILogDebug["util_attack_nearest_target"] << name << " inside AI::attack_nearest_target";
  for (std::pair<MapPos, unsigned int> target : *(scored_targets)) {
    MapPos target_pos = target.first;
    unsigned int target_score = target.second;
    AILogDebug["util_attack_nearest_target"] << name << " inside AI::attack_nearest_target, found target with target_pos " << target_pos << " and score " << target_score;
    int attacking_knights = player->knights_available_for_attack(target_pos);
    //AILogDebug["util_attack_nearest_target"] << name << " debug send up to " << attacking_knights << " knights to attack before max_knights";
    //int max_knights = 0;  // what is this?
    //attacking_knights = std::min(attacking_knights, max_knights);
    AILogDebug["util_attack_nearest_target"] << name << " send up to " << attacking_knights << " knights to attack enemy building at pos " << target_pos;
    AILogDebug["util_attack_nearest_target"] << name << " my military_score = " << player->get_military_score() << ", knight_morale = " << player->get_knight_morale();
    //AILogDebug["util_attack_nearest_target"] << name << " ENEMY military_score = " << game->get_player(target_player_index)->get_military_score() << ",  ENEMY knight_morale = " << game->get_player(target_player_index)->get_knight_morale();
    AILogDebug["util_attack_nearest_target"] << name << " my morale is " << player->get_knight_morale() << ", min_knight_morale_attack is " << min_knight_morale_attack;
   //// TEMPORARILY DISABLING THIS to debug fighting bugs
   //if (true){
   if (player->get_knight_morale() > min_knight_morale_attack) {
      AILogDebug["util_attack_nearest_target"] << name << " my morale " << player->get_knight_morale() << " is at least min_knight_morale_attack of " << min_knight_morale_attack;
      int defending_knights = 2;  // TEMP hardcoded
      double attack_ratio = static_cast<double>(attacking_knights) / static_cast<double>(defending_knights);
      AILogDebug["util_attack_nearest_target"] << name << " attacking_knights=" << attacking_knights << ", defending_knights=" << defending_knights << ", attack_ratio=" << attack_ratio;
      if (attack_ratio >= min_knight_ratio_attack) {
        AILogDebug["util_attack_nearest_target"] << name << " attack_ratio " << attack_ratio << " is >= to min_knight_ratio_attack " << min_knight_ratio_attack << ", PROCEEDING WITH THE ATTACK!";
        player->building_attacked = game->get_building_at_pos(target_pos)->get_index();
        player->attacking_building_count = attacking_knights;
        AILogDebug["util_attack_nearest_target"] << name << " calling player->start_attack()";
        player->start_attack();
        AILogDebug["util_attack_nearest_target"] << name << " DONE calling player->start_attack()";
      }
    }
  }
  AILogDebug["util_attack_nearest_target"] << name << " done AI::attack_nearest_target";
}


// count the number of signs of any type, and the number of hills that have no blocking objects (signs are okay, they are not blocking)
// NOTE - signs can be placed on full-snow tiles which cannot actually have mines, so sign_density is unlikely to reach 100%
double
AI::count_geologist_sign_density(MapPos pos, unsigned int distance) {
  AILogDebug["count_geologist_sign_density"] << name << " inside count_geologist_sign_density, around pos " << pos;
  double signs_count = AI::count_objects_near_pos(pos, distance, Map::ObjectSignLargeGold, Map::ObjectSignSmallStone, "dk_orange");
  double empty_hills_count = AI::count_empty_terrain_near_pos(pos, distance, Map::TerrainTundra0, Map::TerrainSnow1, "orange");
  double sign_density = signs_count / empty_hills_count;
  AILogDebug["count_geologist_sign_density"] << name << " done, area around pos " << pos << " has signs_count: " << signs_count << ", empty_hills_count: " << empty_hills_count << ", sign_density: " << sign_density << ", deprioritize at " << geologist_sign_density_deprio;
  return sign_density;
}

// set any serf that is part of current ai_mark_serf to LostState
//  for debugging
void
AI::set_serf_lost() {
  AILogInfo["set_serf_lost"] << " inside set_serf_lost";
  for (int serf_index : ai_mark_serf){
    Serf *serf = game->get_serf(serf_index);
    if (serf == nullptr){
      Log::Info["set_serf_lost"] << "ai_mark_serf with index " << serf_index << " is nullptr!";
      continue;
    }
    Log::Info["set_serf_lost"] << "setting lost state for ai_mark_serf with index " << serf_index << " at pos " << serf->get_pos();
    serf->set_lost_state();
    Log::Info["set_serf_lost"] << "debug - done setting lost state for ai_mark_serf with index " << serf_index << " at pos " << serf->get_pos();
  }
  Log::Info["set_serf_lost"] << "debug - done setting lost state for ai_mark_serf list";
}