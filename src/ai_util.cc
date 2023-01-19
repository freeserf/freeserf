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
  //   AI is now using this, allows up to spiral_dist[48]
  if (distance > 48) {
    Log::Error["ai_util"] << "CANNOT USE spiral_dist() GREATER THAN 48!!!";
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
 //
 // NOTE - it looks like minimum map size 3 is 64x64 pos, so it should be safe to have spiral dist up to that if required
 //
  return _spiral_dist[distance];
}

// return true if *any* of the four points contain the requested terrain type
//
// NOTE - it is useful when searching for MapPos with a particular terrain type, to instead
//   search for that "does not have the wrong/other tpyes" rather than "has the desired type"
//   Should probably create a "has_ONLY_THESE_terrain_types" function!
//
bool
AI::has_terrain_type(PGame game, MapPos pos, Map::Terrain res_start_index, Map::Terrain res_end_index) {
  Log::Verbose["util_has_terrain_type"] << "inside AI::has_terrain_type";
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
//  desperation is a multiplier
bool
//AI::place_castle(PGame game, MapPos center_pos, unsigned int distance, unsigned int desperation) {
AI::place_castle(PGame game, MapPos center_pos, unsigned int desperation) {
  AILogDebug["util_place_castle"] << "inside AI::place_castle, center_pos " << center_pos << ", desperation " << desperation;
  PMap map = game->get_map();
  unsigned int trees = 0;
  unsigned int stones = 0;
  unsigned int building_sites = 0;
  unsigned int nongrass_tiles_ring0 = 0;
  unsigned int nongrass_tiles_ring1 = 0;
  unsigned int nongrass_tiles_ring2 = 0;

  //ai_mark_pos.insert(std::make_pair(center_pos, "green"));

  for (unsigned int i = 0; i < spiral_dist(15); i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);

    //
    // avoid areas with too much blocking terrain
    //

    // check right around the castle itself
    if (i > AI::spiral_dist(2) && i < AI::spiral_dist(3)){
     //ai_mark_pos.insert(std::make_pair(pos, "dk_brown"));
     //sleep_speed_adjusted(15);
      if (!map->types_within(pos, Map::TerrainGrass0, Map::TerrainGrass3)
       ||  map->get_owner(pos) != player_index && map->has_owner(pos)){   // also count enemy territory as blocking!
        //ai_mark_pos.erase(pos);
        //ai_mark_pos.insert(std::make_pair(pos, "dk_red"));
        //sleep_speed_adjusted(15);
        nongrass_tiles_ring0++;
      }
    }
    // check for in the perimeter of the would-be castle borders hexagon
    if (i > AI::spiral_dist(7) && i < AI::spiral_dist(8)){
      //ai_mark_pos.insert(std::make_pair(pos, "dk_brown"));
     //sleep_speed_adjusted(15);
      if (!map->types_within(pos, Map::TerrainGrass0, Map::TerrainGrass3)
       ||  map->get_owner(pos) != player_index && map->has_owner(pos)){   // also count enemy territory as blocking!
        //ai_mark_pos.erase(pos);
        //ai_mark_pos.insert(std::make_pair(pos, "dk_red"));
        //sleep_speed_adjusted(15);
        nongrass_tiles_ring1++;
      }
    }
    // check somewhat further out
    if (i > AI::spiral_dist(14)){
      //ai_mark_pos.insert(std::make_pair(pos, "dk_yellow"));
      //sleep_speed_adjusted(15);
      if (!map->types_within(pos, Map::TerrainGrass0, Map::TerrainGrass3)
       ||  map->get_owner(pos) != player_index && map->has_owner(pos)){   // also count enemy territory as blocking!
        //ai_mark_pos.erase(pos);
        //ai_mark_pos.insert(std::make_pair(pos, "dk_red"));
        //sleep_speed_adjusted(25);
        nongrass_tiles_ring2++;
      }
    }

    //
    // check resources within the castle borders area only
    //
    if (i > AI::spiral_dist(8)){
      continue;
    }

    // don't count resouces that are inside enemy territory
    if (map->get_owner(pos) != player_index && map->has_owner(pos)) {
      //AILogDebug["util_place_castle"] << "enemy territory at pos " << pos << ", not counting these resouces towards requirements";
      continue;
    }

    Map::Object obj = map->get_obj(pos);
    if (obj >= Map::ObjectTree0 && obj <= Map::ObjectPine7) {
      trees += 1;
      //AILogDebug["util_place_castle"] << "adding trees count 1";
    }
    if (obj >= Map::ObjectStone0 && obj <= Map::ObjectStone7) {
      int stonepile_value = 1 + (-1 * (obj - Map::ObjectStone7));
      stones += stonepile_value;
      //AILogDebug["util_place_castle"] << "adding stones count " << stonepile_value;
    }
    if (Map::map_space_from_obj[map->get_obj(pos)] == Map::SpaceOpen){
      if (game->can_build_large(pos)) {
        building_sites += 3;  // large building sites worth 50% more than small ones, but can't use 1.5 and 1.0 because integer
        //AILogDebug["util_place_castle"] << "adding large building value 3";
      }
      else if (game->can_build_small(pos)) {
        building_sites += 2;
        //AILogDebug["util_place_castle"] << "adding small building value 1";
      }
    }
  }

  int nongrass_tiles_ring0_pct = double(nongrass_tiles_ring0 / double(AI::spiral_dist(3) - AI::spiral_dist(2))) * 100;
  int nongrass_tiles_ring1_pct = double(nongrass_tiles_ring1 / double(AI::spiral_dist(8) - AI::spiral_dist(7))) * 100;
  int nongrass_tiles_ring2_pct = double(nongrass_tiles_ring2 / double(AI::spiral_dist(15) - AI::spiral_dist(14))) * 100;
  AILogDebug["util_place_castle"] << "found non-grass terrain, ring0 pct " << nongrass_tiles_ring0_pct << "%, ring1 pct " << nongrass_tiles_ring1_pct << "%, ring2 pct " << nongrass_tiles_ring2_pct << "%";
  AILogDebug["util_place_castle"] << "found trees: " << trees << ", stones: " << stones << ", building_sites: " << building_sites << " in area " << center_pos << ". Desperation is " << desperation;

  // if good place for castle cannot be found, lower standards by faking an increased amount of resources and less blocking terrain

  if (nongrass_tiles_ring0_pct > ring0_blocking_terrain_pct_max + desperation*2) {
    AILogDebug["util_place_castle"] << "too much blocking terrain in ring0, " << nongrass_tiles_ring0_pct << "% is blocked, max is " << ring0_blocking_terrain_pct_max << ", returning false.  Desperation is " << desperation;
    return false;
  }
  if (nongrass_tiles_ring1_pct > ring1_blocking_terrain_pct_max + desperation*2) {
    AILogDebug["util_place_castle"] << "too much blocking terrain in ring1, " << nongrass_tiles_ring1_pct << "% is blocked, max is " << ring1_blocking_terrain_pct_max << ", returning false.  Desperation is " << desperation;
    return false;
  }
  if (nongrass_tiles_ring2_pct > ring2_blocking_terrain_pct_max + desperation*2) {
    AILogDebug["util_place_castle"] << "too much blocking terrain in ring2, " << nongrass_tiles_ring2_pct << "% is blocked, max is " << ring2_blocking_terrain_pct_max << ", returning false.  Desperation is " << desperation;
    return false;
  }
  if (trees + desperation*4 < near_trees_min * 4) {
    AILogDebug["util_place_castle"] << "not enough trees, min is " << near_trees_min * 4 << ", returning false.  Desperation is " << desperation;
    return false;
  }
  if (stones + desperation < near_stones_min) {
    AILogDebug["util_place_castle"] << "not enough stones, min is " << near_stones_min << ", returning false.  Desperation is " << desperation;
    return false;
  }
  if (building_sites + desperation*45 < near_building_sites_min) {
    AILogDebug["util_place_castle"] << "not enough building_sites, min is " << near_building_sites_min << ", returning false.  Desperation is " << desperation;
    return false;
  }

  AILogDebug["util_place_castle"] << "center_pos: " << center_pos << " is an acceptable building site for a castle.  Desperation is " << desperation;
  AILogDebug["util_place_castle"] << "done AI::place_castle";
  return true;
}


// update current AI player's inventory of buildings of all types for lookup by various functions
//  does this need to be mutex-wrapped?  sometimes getting exceptions with seemingly invalid building
//  variable/pointer
void
AI::update_buildings() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();

  AILogDebug["util_update_buildings"] << "start";
  Game::ListBuildings buildings = game->get_player_buildings(player);
  // reset all to zero
  memset(realm_building_count, 0, sizeof(realm_building_count));
  memset(realm_completed_building_count, 0, sizeof(realm_completed_building_count));
  //memset(realm_connected_building_count, 0, sizeof(realm_connected_building_count));
  //memset(realm_incomplete_building_count, 0, sizeof(incomplete_building_count));
  memset(realm_occupied_building_count, 0, sizeof(realm_occupied_building_count));
  realm_occupied_military_pos.clear();
  realm_res_sitting_at_flags.clear();
  //realm_inv.clear();  // don't clear this here as it is not additive but explicitly set in each loop to player->inv 
  for (MapPos inventory_pos : stocks_pos) {
    AILogVerbose["util_update_buildings"] << "inside AI::update_buildings, clearing counts for inventory_pos " << inventory_pos;
    memset(stock_building_counts.at(inventory_pos).count, 0, sizeof(stock_building_counts.at(inventory_pos).count));
    memset(stock_building_counts.at(inventory_pos).completed_count, 0, sizeof(stock_building_counts.at(inventory_pos).completed_count));
    memset(stock_building_counts.at(inventory_pos).occupied_count, 0, sizeof(stock_building_counts.at(inventory_pos).occupied_count));
    memset(stock_building_counts.at(inventory_pos).connected_count, 0, sizeof(stock_building_counts.at(inventory_pos).connected_count));
    stock_building_counts.at(inventory_pos).occupied_military_pos.clear();
    stock_building_counts.at(inventory_pos).unfinished_count = 0;
    stock_building_counts.at(inventory_pos).unfinished_hut_count = 0;
    stock_attached_buildings.at(inventory_pos).clear();
  }

  // now that a flagsearch is being used to find the nearest Inventory rather than straight-line
  //  distance, it might be possible to optimize this process by searching outward from each
  //  inventory once?  or caching results or something like that?  
  //  only bother if this function is taking a significant amount of time
  for (Building *building : buildings) {

    if (building == nullptr)
      continue;
    if (building->is_burning())
      continue;
    
    Building::Type type = building->get_type();
	  if (type == Building::TypeNone) {
		  //AILogWarn["util_update_buildings"] << "has a building of TypeNone at pos " << building->get_position() << "!  why does this happen??";
		  continue;
	  }

	  MapPos pos = building->get_position();
	  MapPos flag_pos = map->move_down_right(building->get_position());
    // saw a weird exception where building 'type' was... 10117???  if this keeps happening start dumping building type before it crashes
    // again, same deal.  adding debugging
    // debugging confirms seeing buildings with weird high invalid types >1024, maybe bitmath explanation??? look at any place in code
    //  that 'type' is modified to find out
    // I can't find anywhere in the original code that building type is modified... I think this is a bug and it is pointing
    //  to a wrong object?? that is worrying
    // first time logging enabled, the building with invalid type used to have a flag at same pos, which was attached a building of TypeHut up-left one pos
    // second time, was a Hut again
    // third time, never seen before, totally invalid map pos.  this is very likely a wrong pointer
    // fourth time, just like 1st and 2nd, knight hut
    // 5th, knight hut stub road, had a spiderweb road built to it
    // 6th, can't tell.  
    // bypassing this error for now
    AILogDebug["util_update_buildings"] << "debug has a building of type " << type << " at pos " << pos << ", with flag_pos " << flag_pos;
    if (type > Building::TypeCastle){
      AILogError["util_update_buildings"] << "RECURRING BUG! has a building of invalid type " << type << " at pos " << pos << ", with flag_pos " << flag_pos << "! bypassing error";
      continue;
    }

	  //AILogVerbose["util_update_buildings"] << "has a building of type " << NameBuilding[type] << " at pos " << pos << ", with flag_pos " << flag_pos;

    if (type == Building::TypeCastle) {
      if (!building->is_done()) {
        AILogDebug["util_update_buildings"] << "player's castle isn't finished building yet";
        while (!building->is_done()) {
          AILogDebug["util_update_buildings"] << "player's castle isn't finished building yet.  Sleeping a bit";
          sleep_speed_adjusted(1000);
        }
        AILogDebug["util_update_buildings"] << "player's castle is now built, updating stocks";
        // does the stocks_pos logic rely on the Castle always being the first building?  does it matter?
        update_stocks_pos();
      }
      realm_occupied_military_pos.push_back(flag_pos);
      stock_building_counts.at(flag_pos).occupied_military_pos.push_back(flag_pos);
      continue;
    }
  
    // make completed stocks the first item in that stock's occupied_military pos, even
    //  though it isn't really military it is a good first place to build buildings that
    //  have no affinity
    if (type == Building::TypeStock && stock_building_counts.count(flag_pos)){
      AILogVerbose["util_update_buildings"] << "adding occupied Stock building at " << pos << " to stock's occupied_military_pos list, even though it isn't really military";
      stock_building_counts.at(flag_pos).occupied_military_pos.push_back(flag_pos);
      continue;
    }

	  // for military buildings, track the nearest Inventory by straight-line distance
	  //  as these are used for general selection of nearby huts when looking for things
	  //  in a certain area around an Inventory and its borders, duplicates are allowed.
	  if (building->is_military() && building->is_done() && building->is_active()) {
      // require that the building have at least one path to avoid causing AI to try to build
      //  new buildings near newly-conquered enemy buildings that are not connected to our road network
      //  Could also check if they are actually "connected to network" even if they have a path
      //  but this is probably good enough...
      if (game->get_flag(building->get_flag_index()) != nullptr && !game->get_flag(building->get_flag_index())->is_connected()){
        AILogDebug["util_update_buildings"] << "occupied military building at pos " << pos << " has no paths, not including it in occupied_military_pos lists";
        continue;
      }
		  AILogVerbose["util_update_buildings"] << "adding occupied military building at " << pos << " to realm_occupied_military_pos list";
		  realm_occupied_military_pos.push_back(flag_pos);
		  AILogVerbose["util_update_buildings"] << "about to call find_nearest_inventories_to_military_building for connected building of type " << NameBuilding[type] << " at pos " << pos << ", with flag_pos " << flag_pos;
		  for (MapPos inv_flag_pos : find_nearest_inventories_to_military_building(flag_pos)) {
			  AILogVerbose["util_update_buildings"] << "adding occupied military building at " << pos << " to stock_building_counts.at(" << inv_flag_pos << ")";
			  stock_building_counts.at(inv_flag_pos).occupied_military_pos.push_back(flag_pos);
		  }
		  // don't quit here, we still want to try to do a FlagSearch for military buildings so
		  //  if they are incomplete, we can associate the unfinished building to an Inventory
		  //  because the builder and building material source will come via FlagSearch. 
		  // It might not be the closest-by-straightline-dist Inventory that is used
		  //  to track occupied military buildings for border determination reasons
	  }

	  // skip disconnected buildings, they cannot complete a FlagSearch to find nearest Inventory
    if (game->get_flag_at_pos(flag_pos) == nullptr){
      AILogVerbose["util_update_buildings"] << "building of type " << NameBuilding[type] << " at pos " << pos << ", got nullptr on get_flag_at_pos, skipping";
      continue;
    }
	  if (!game->get_flag_at_pos(flag_pos)->is_connected()) {
		  AILogVerbose["util_update_buildings"] << "building of type " << NameBuilding[type] << " at pos " << pos << ", with flag_pos " << flag_pos << " has no paths, cannot do FlagSearch, skipping";
		  continue;
	  }

	  // do the FlagSearch
	  AILogVerbose["util_update_buildings"] << "about to call find_nearest_inventory for connected building of type " << NameBuilding[type] << " at pos " << pos << ", with flag_pos " << flag_pos;
	  MapPos nearest_inventory_pos = find_nearest_inventory(map, player_index, building->get_position(), DistType::FlagOnly, &ai_mark_pos);
	  if (nearest_inventory_pos == bad_map_pos) {
		  AILogVerbose["util_update_buildings"] << "find_nearest_inventory call for building at pos " << pos << ", with flag_pos " << flag_pos << " returned bad_map_pos, skipping this building";
		  continue;
	  }
	  /* i think this is wrong
    AILogVerbose["util_update_buildings"] << "nearest Inventory (by flagsearch) to this connected building is " << nearest_inventory_pos;
    if (nearest_inventory_pos != inventory_pos) {
		  AILogVerbose["util_update_buildings"] << "this building's nearest_inventory_pos " << nearest_inventory_pos << " is not the currently selected inventory_pos " << inventory_pos << ", skipping it";
		  continue;
	  }
    */

    // track the Building
    stock_attached_buildings.at(nearest_inventory_pos).push_back(building);

	  // count incomplete buildings (so AI can limit the number of outstanding unfinished buildings)
    if (!building->is_done()) {
      if (type == Building::TypeHut) {
        stock_building_counts.at(nearest_inventory_pos).unfinished_hut_count++;
        AILogVerbose["util_update_buildings"] << "incrementing unfinished_hut_count for nearest_inventory_pos " << nearest_inventory_pos << ", is now: " << stock_building_counts.at(nearest_inventory_pos).unfinished_hut_count;
      }
      else if (building->get_type() == Building::TypeCoalMine
        || building->get_type() == Building::TypeIronMine
        || building->get_type() == Building::TypeGoldMine
        || building->get_type() == Building::TypeStoneMine) {
        AILogVerbose["util_update_buildings"] << "unfinished building is a Mine, not incrementing unfinished_building_count";
      }
      else {
        stock_building_counts.at(nearest_inventory_pos).unfinished_count++;
        AILogDebug["util_update_buildings"] << "found unfinished building of type " << NameBuilding[type] << " at pos " << building->get_position() << ", incrementing unfinished_building_count for nearest_inventory_pos " << nearest_inventory_pos << ", is now: " << stock_building_counts.at(nearest_inventory_pos).unfinished_count;
      }
    }
    
    // should this exclude military buildings to avoid confusion?
    //  leaving it for now, the count of military buildings here
    //  is based on FlagSearch dist, NOT straight-line dist as
    //  is used for OCCUPIED_military_buildings counts
	  realm_building_count[type]++;   // move this to before the FlagSearch so it has a chance to include buildings that are not connected?  nah, leave as-is for now
    stock_building_counts.at(nearest_inventory_pos).count[type]++;
    // the difference between 'buildings' and 'connected_buildings' is now moot because the nearest inventory pos
    //  is now determined by FlagSearch which requires that they be connected already to do the search
    //  Should probably remove the 'building_count' entirely.  See if any of this is even used anymore, I don't think it is
	  //realm_connected_building_count[type]++;
    stock_building_counts.at(nearest_inventory_pos).connected_count[type]++;
    if (building->is_done()){
	    realm_completed_building_count[type]++;
      stock_building_counts.at(nearest_inventory_pos).completed_count[type]++;
      // has_serf is not a good enough test alone to see if occupied, as it seems to be true when a builder is constructing the building!
      //  so moved this check to inside building->is_done because if building is done the only serf there should be the professional (I think)
      if (building->has_serf()) {
        // this will include military buildings occupied by knights, also.  Which I guess is fine
        //  however, again the counts may not match the occupied_military_building counts
        //  which are determined by straight-line distance only
	      realm_occupied_building_count[type]++;
        stock_building_counts.at(nearest_inventory_pos).occupied_count[type]++;
      }
    }
  } // foreach Building : get_player_buildings

  // debug, dump all inventory-to-building counts
  for (MapPos an_inventory_pos : stocks_pos) {
    // this skips 'connected', which is moot anyway
    AILogVerbose["util_update_buildings"] << "Inventory at pos " << an_inventory_pos << " has all/completed/occupied buildings: ";
    int type = 0;
    // why is this using realm_building_count instead of ... Types 0-25?
    for (int count : realm_building_count) {
      AILogVerbose["util_update_buildings"] << "type " << type << " / " << NameBuilding[type] << ": " << stock_building_counts.at(an_inventory_pos).count[type]
        << "/" << stock_building_counts.at(an_inventory_pos).completed_count[type] << "/" << stock_building_counts.at(an_inventory_pos).occupied_count[type];
      type++;
    }
  }

  //AILogDebug["util_update_buildings"] << "done AI::update_buildings";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_update_buildings"] << "done AI::update_buildings call took " << duration;
}


// update current AI player's vector of built warehouse/stock positions
//   INCLUDING the castle.  this should be the FLAG of the stock, not the stock itself
void
AI::update_stocks_pos() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();

  AILogDebug["util_update_stocks_pos"] << "inside AI::update_stocks_pos";
  Game::ListBuildings buildings = game->get_player_buildings(player);
  stocks_pos = {};
  for (Building *building : buildings) {
    if (building == nullptr)
      continue;
    if (building->is_burning())
      continue;
    Building::Type type = building->get_type();
    if (type != Building::TypeCastle && type != Building::TypeStock)
      continue;
    MapPos stock_flag_pos = map->move_down_right(building->get_position());
    AILogDebug["util_update_stocks_pos"] << "found a building of type " << NameBuilding[type] << " stock flag: " << stock_flag_pos;
    
    // special - to detect when a new Stock first becomes occupied (to run various functions only once on it)
    //   we need to note any incomplete or unoccupied stocks and include in this new_stocks list, but do NOT put in stocks_list which should only have occupied stocks
    if (type == Building::TypeStock){
      if (!building->is_done()){
        AILogDebug["util_update_stocks_pos"] << "found an incomplete stock at pos " << building->get_position() << " with stock_flag_pos " << stock_flag_pos << ", adding it to incomplete stocks list";
        new_stocks.insert(stock_flag_pos);
      }
      if (!building->has_serf()){
        AILogDebug["util_update_stocks_pos"] << "found a complete but unoccupied stock at pos " << building->get_position() << " with stock_flag_pos " << stock_flag_pos << ", adding it to incomplete stocks list";
        new_stocks.insert(stock_flag_pos);
      }
    }
    
    if (type == Building::TypeStock){
      if (!building->is_done())
        continue;
      if (!building->has_serf())
        continue;
      if (game->get_flag(building->get_flag_index()) == nullptr)
        continue;
      if (!game->get_flag(building->get_flag_index())->is_connected())
        continue;
    }
    if (type == Building::TypeCastle) {
      AILogDebug["util_update_stocks_pos"] << "the castle was found at pos " << building->get_position() << ", with its flag at pos " << stock_flag_pos;
    }else {
      AILogDebug["util_update_stocks_pos"] << "a completed, connected, serf-occupied warehouse/stock was found at pos " << building->get_position() << ", with its flag at pos " << stock_flag_pos;
    }
    stocks_pos.push_back(stock_flag_pos);
    //stock_building_counts[stock_flag_pos];
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
    stock_building_counts[stock_flag_pos] = { {0},{0},{0},{0},0,0,{} };
    stock_attached_buildings[stock_flag_pos] = {};
    AI::set_forbidden_pos_around_inventory(stock_flag_pos);
  }
  AILogDebug["util_update_stocks_pos"] << "done AI::update_stocks_pos";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_update_stocks_pos"] << "done AI::update_stocks_pos call took " << duration;
}

//
// try to build a road from the specified flag pos to the "best" end flag (determined by various logic), return success/failure
//  ALSO, set the value of the Road completed (if built) to the provided memory location (though most calling functions don't use it)
//  If any roads already exist from specified flag to each affinity building,
//    build a better road if one is found.  Do not remove the old one
//
//  POSSIBLE MAJOR IMPROVEMENT - before finding flags based on nearby/halfway, 
//   first do a "flood" A* pathfinding tile search
//    exclude any flags that //   and record every flag that is reachable within some reasonable distance
//    THEN when searching for nearby flags exclude
//   any that aren't actually reachable!    or something like this.  could eval them along the way
//
//
//
bool
AI::build_best_road(MapPos start_pos, RoadOptions road_options, Road *built_road, std::string calling_function, 
          Building::Type optional_affinity_building_type, Building::Type optional_affinity, MapPos optional_target, bool verify_stock) {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();

  AILogDebug["util_build_best_road"] << "" << calling_function << " start pos " << start_pos << ", optional_affinity_building_type " << NameBuilding[optional_affinity_building_type] << ", optional_affinity " << NameBuilding[optional_affinity] << ", optional_target " << optional_target;

  // print RoadOptions for debugging
  for (unsigned int i = 0; i < road_options.size(); i++) {
    AILogDebug["util_build_best_road"] << "" << calling_function << " RoadOption: " << NameRoadOption[i] << " = " << bool(road_options.test(i));
  }

  // sanity check request
  if (map->get_owner(start_pos) != player_index) {
    AILogDebug["util_build_best_road"] << "" << calling_function << " start_pos " << start_pos << " is not owned by Player" << player_index << "!  returning false";
    return false;
  }
  
  if (map->has_flag(start_pos)) {
    // check if this start_flag already has any paths
    if (game->get_flag_at_pos(start_pos) != nullptr && game->get_flag_at_pos(start_pos)->is_connected()) {
      AILogDebug["util_build_best_road"] << "" << calling_function << " at least one path already exists from flag at start_pos";
      // do not cancel if this condition happens, because it is normal when doing rebuild_all_roads
      //  no longer using rebuild_all_roads, setting this again
      // oh wait, road_stub removal is blocked by this also, as it tries to connect a new path for Knight Huts but cannot if this stops it
      //  removing it again
      //if (!road_options.test(RoadOption::Improve)) {
      //  AILogDebug["util_build_best_road"] << "" << calling_function << " a path already exists from start_pos " << start_pos << " but RoadOption::Improve is false!  this is unexpected.  returning false";
      //  return false;
      //}
      if (!road_options.test(RoadOption::Direct)) {
        AILogDebug["util_build_best_road"] << "" << calling_function << " RoadOption::Direct is not set, build_best_road will try to build a better road than the current best one";
      }
      // skip if no more paths can be built from starting flag
      bool can_build_another_path = false;
      for (Direction dir : cycle_directions_ccw()) {
        if (game->get_flag_at_pos(start_pos) != nullptr && !game->get_flag_at_pos(start_pos)->has_path_IMPROVED(dir) && map->is_road_segment_valid(start_pos, dir)) {
          can_build_another_path = true;
          break;
        }
      }
      if (!can_build_another_path) {
        // hmm.. considering changing this to return true because a path already exists... BUT I think all build_road
        //  calls should be first preceded by a flag->is_connected check first
        AILogDebug["util_build_best_road"] << "" << calling_function << "no more paths can be build from this building's flag!  returning false";
        return false;
      }
    }
    else {
      AILogDebug["util_build_best_road"] << "" << calling_function << " start_pos " << start_pos << " has no paths, will try to connect it to road network";
    }
  }else{
    // changing this to support plotting best road BEFORE building a flag & building
    //AILogDebug["util_build_best_road"] << "" << calling_function << " No flag exists at start_pos " << start_pos << "!  returning false";
    //return false;
    AILogDebug["util_build_best_road"] << "" << calling_function << " no flag exists at start_pos " << start_pos << ", this must be a pre-building check, continuing";
  }

  // if an optional_affinity_building_type is set, this must be a pre-building road
  //  so we must enable HoldBuildingPos to ensure the road doesn't block the planned building
  if (optional_affinity_building_type != Building::TypeNone){
    // NOTE this message and setting of HoldBuildingPos is not always correct, when build_better_roads is called it sets optional_affinity_building_type but is only Improving connection
    AILogDebug["util_build_best_road"] << "" << calling_function << " optional_affinity_building_type is set, this must be a pre-building road.  Setting RoadOption::HoldbuildingPos to true";
    road_options.set(RoadOption::HoldBuildingPos);
  }

  MapPosVector targets = {};  // these are flag positions

  //double sofarduration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  //AILogDebug["util_build_best_road"] << "A. " << calling_function << " SO FAR call took " << sofarduration;


  // handle the optional_target arg if set
  if (optional_target != bad_map_pos) {
    // only for road improvements, target a specific flag or building pos rather than generic "connect to road system"
    AILogDebug["util_build_best_road"] << "" << calling_function << " using optional_target pos " << optional_target << " specified in build_best_road call";
    // check to if target is a building or a flag
    if (map->has_flag(optional_target)) {
      AILogDebug["util_build_best_road"] << "" << calling_function << " optional_target pos " << optional_target << " is a flag.  checking to see if it has an attached building";
      // this is only for debugging
      if (map->has_building(map->move_up_left(optional_target))) {
        AILogDebug["util_build_best_road"] << "" << calling_function << " flag at optional_target pos " << optional_target << " has an attached building of type " << NameBuilding[game->get_building_at_pos(map->move_up_left(optional_target))->get_type()];
      }
      AILogDebug["util_build_best_road"] << "" << calling_function << " setting optional_target flag pos " << optional_target << " as road_to target";
      targets.push_back(optional_target);
    }
    else if (map->has_building(optional_target)) {
      AILogDebug["util_build_best_road"] << "" << calling_function << " optional_target pos " << optional_target << " is a building of type " << NameBuilding[game->get_building_at_pos(optional_target)->get_type()];
      AILogDebug["util_build_best_road"] << "" << calling_function << " setting optional_target pos " << optional_target << " building's flag at pos " << map->move_down_right(optional_target) << " as road_to target";
      targets.push_back(map->move_up_left(optional_target));
    }
    else {
      //AILogDebug["util_build_best_road"] << "" << calling_function << " optional_target pos has neither a building nor flag!  this is unexpected, returning false";
      //return false;
      AILogDebug["util_build_best_road"] << "" << calling_function << " zzz optional_target pos has neither a building nor flag, maybe we are planning to create one";
      targets.push_back(optional_target);
    }
  }
  else {
    // most common - use the building affinity table to figure out what to connect to
    targets = AI::get_affinity(start_pos, optional_affinity_building_type);
    AILogDebug["util_build_best_road"] << "" << calling_function << " targets contains " << targets.size() << " affinity / target buildings";
    // if no affinity-based targets found, fall back to nearest or selected inv
    if (targets.size() == 0){
      AILogDebug["util_build_best_road"] << "" << calling_function << " get_affinity found no valid target, trying to find a nearest connected inventory";
      MapPos fallback_inv_pos = bad_map_pos;
      // not requiring a flag or building, it should be find to connect disconnected flags to nearest straight-line inventory
      //if (map->has_flag(start_pos) && map->has_building(map->move_up_left(start_pos))){
        // this should NOT use Flag dist at all because if there is no affinity, and likely no road at all,
        //  then there is no way to complete a flag search, instead use the closest by STRAIGHT LINE DIST
        //MapPos nearest_inv_pos = find_nearest_inventory(map, player_index, start_pos, DistType::FlagAndStraightLine, &ai_mark_pos);
        MapPos nearest_inv_pos = find_nearest_inventory(map, player_index, start_pos, DistType::StraightLineOnly, &ai_mark_pos);
        if (nearest_inv_pos != bad_map_pos){
          AILogDebug["util_build_best_road"] << "" << calling_function << " no affinity target found, setting target to nearest_inv_pos (by straight line) " << nearest_inv_pos;
          fallback_inv_pos = nearest_inv_pos;
        }else{
          AILogDebug["util_build_best_road"] << "" << calling_function << " find_nearest_inventory by StraightLineOnly found nearest_inv_pos " << nearest_inv_pos;
        }
      //}
      if (fallback_inv_pos == bad_map_pos){
        fallback_inv_pos = inventory_pos;
        //AILogDebug["util_build_best_road"] << "" << calling_function << " no affinity target found and could not find nearest_inv_pos, setting target to current inventory_pos " << inventory_pos;
        AILogWarn["util_build_best_road"] << "" << calling_function << " no affinity target found and could not find nearest_inv_pos " << inventory_pos << ", returning false.  NOTE this seems to only happen when player loses their castle/last stock!";
        // could change the fallback to simply be nearest flag, but I don't think this should never happen if using StraightLineOnly
        //  this seems to trigger when AI player loses their castle/last stock, don't throw exception instead return false and let AI handle it
        //throw ExceptionFreeserf("failed to find inventory by StraightLine, this should always work");
        return false;
      }
      targets.push_back(fallback_inv_pos);
    }
  }

  //sofarduration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  //AILogDebug["util_build_best_road"] << "B. " << calling_function << " SO FAR call took " << sofarduration;

  //unsigned int roads_built = 0;
  unsigned int complete_roads_built = 0;  // now that passthru enabled, changing name to avoid confusion with road segments
  unsigned int target_count = static_cast<unsigned int>(targets.size());
  //
  // change this to its own function so there isn't a foreach loop for hundreds of lines to handle two targets
  //
  //
  // CONSIDER MOVING THE get_affinity CALL TO INSIDE THIS FUNCTION SO THAT IT CAN TRY A SERIES OF AFFINITY BUILDINGS
  //  INSTEAD OF JUST THE CLOSEST ONE
  //

  int target_num = 0;  // TEMP UNTIL REVAMP
  //unsigned int first_target_road_score = bad_score;  // for comparing 2nd to 1st if multiple targets found
  MapPos first_target_road_end = bad_map_pos;  // for comparing 2nd to 1st if multiple targets found
  for (MapPos target_pos : targets) {

    //sofarduration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    //AILogDebug["util_build_best_road"] << "C. " << calling_function << " SO FAR call took " << sofarduration;
    //
    // TEMP UNTIL REVAMP
    //
    // Nov 2022 - it seems this logic has been here for a long time, maybe just leave it?
    //
    target_num++;
    if (target_num > 2){
      AILogDebug["util_build_best_road"] << "" << calling_function << " TEMPORARY -  NOT ALLOWING MORE THAN 2 TARGETS - breaking";
      break;
    }
    if (target_pos == bad_map_pos) {
      AILogDebug["util_build_best_road"] << "" << calling_function << " no more affinity targets left";
      break;
    }
    AILogDebug["util_build_best_road"] << "" << calling_function << " considering target_pos " << target_pos;
    AILogDebug["util_build_best_road"] << "" << calling_function << " plotting road to connect flag at pos " << start_pos << " to target_pos " << target_pos << " via road network";
    // Roads split_roads; moving this to INSIDE loop so it isn't accumulating ALL split_roads from ALL real flags
    //
    // ##   Direct   ## roads are simple A->B connection from start_pos to target_pos of FIRST affinity building, no second roads
    //
    if (road_options.test(RoadOption::Direct)) {
      AILogDebug["util_build_best_road"] << "" << calling_function << " zzz direct road requested, trying to build direct road from flag at " << start_pos << " to flag at " << target_pos;
      // Just build a single direct road
      //   RoadOption::Improve is ignored, any existing road will not be compared.
      // split_roads list is not actually used for direct roads.  It is required/included but ignored
      // nearest stock check is ignored also
      // HoldBuildingPos ignored
      


      // enhancement - if there is no flag at the target_pos, create one
      //  this allows the calling function to suggest termination points
      //  rather than the usual wandering discovery that non-direct roads use
      bool built_flag = false;

      // add support for RoadOption::PlotOnlyNoBuild
      if (road_options.test(RoadOption::PlotOnlyNoBuild)){
        AILogDebug["util_build_best_road"] << "" << calling_function << " RoadOption::PlotOnlyNoBuild is set, not actually building a flag for Direct road.";
      }else{
        if (!map->has_flag(target_pos)){
          AILogDebug["util_build_best_road"] << "" << calling_function << " zzz direct road requested, no flag at target_pos, trying to build one";
          if (game->can_build_flag(target_pos, player)){
            mutex_lock("AI::build_best_road calling game->build_flag (direct road target_pos has no flag)");
            built_flag = game->build_flag(target_pos, player);
            mutex_unlock();
            if (built_flag){
              AILogDebug["util_build_best_road"] << "" << calling_function << " zzz direct road requested, built new flag at target_pos";
            }else{
              AILogDebug["util_build_best_road"] << "" << calling_function << " zzz direct road requested, failed to build flag at target_pos! returning false";
              return false;
            }
          }
        }
      }

      Roads split_roads;  // moving this here inside loop so it isn't accumulating ALL split_roads from ALL real flags
      Road proposed_direct_road = plot_road(map, player_index, start_pos, target_pos, &split_roads);

      bool plotted_succesfully = false;
      if (proposed_direct_road.get_length() > 0){
        plotted_succesfully = true;
      }

      // avoid creation extremely long roads
      if (proposed_direct_road.get_length() > 30){
        AILogDebug["util_build_best_road"] << "" << calling_function << " proposed_direct_road is over 30 tiles long!  not building this road";
        continue;
      }

      // support covolution enforcement, but *only* if MostlyStraight setting is on
      // this kind of a hacky undocumented thing, but okay for now. 
      bool convolution_rejected = false;
      if (plotted_succesfully && road_options.test(RoadOption::MostlyStraight)){
        AILogDebug["util_build_best_road"] << "" << calling_function << " RoadOptions::MostlyStraight is on, checking tile_dist from start_pos " << start_pos << " to end_pos " << target_pos << " for an ideal road";
        int ideal_length = AI::get_straightline_tile_dist(map, start_pos, target_pos);
        //
        // EVEN UGLIER HACK HERE because this is specific to spiderweb roads
        ///  - really need to ditch most of RoadOptions and instead allow explicitly setting various limits
        //
        if (proposed_direct_road.get_length() > 20){
          AILogDebug["util_build_best_road"] << "" << calling_function << " RoadOptions::MostlyStraight is on, rejecting this solution because proposed_direct_road length of " << proposed_direct_road.get_length() << " is too long";
          convolution_rejected = true;  // re-using this because laziness
        }
        AILogDebug["util_build_best_road"] << "" << calling_function << " RoadOptions::MostlyStraight is on, flag target_pos at " << target_pos << " has straight-line tile distance " << ideal_length << " from start_pos " << start_pos;
        AILogDebug["util_build_best_road"] << "" << calling_function << " RoadOptions::MostlyStraight is on, proposed_direct_road from start_pos " << start_pos << " to target_pos " << target_pos << " has length " << proposed_direct_road.get_length();
        double convolution = static_cast<double>(proposed_direct_road.get_length()) / static_cast<double>(ideal_length);
        AILogDebug["util_build_best_road"] << "" << calling_function << " RoadOptions::MostlyStraight is on, proposed_direct_road length: " << proposed_direct_road.get_length() << ", ideal length: " << ideal_length << ", convolution ratio: " << convolution;
        // still reduce max_convolution ratio for MostlyStraight even though the non-adjusted max_convolution value is never tested for RoadOption::Direct roads
        if (convolution >= max_convolution * 0.50) {
          AILogDebug["util_build_best_road"] << "" << calling_function << " this proposed_direct_road solution is already too convoluted, adjusted max is " << max_convolution * 0.50;
          convolution_rejected = true;
        }
      }

      // add support for RoadOption::PlotOnlyNoBuild
      if (plotted_succesfully && road_options.test(RoadOption::PlotOnlyNoBuild)){
        AILogDebug["util_build_best_road"] << "" << calling_function << " RoadOption::PlotOnlyNoBuild is set, not actually building the Direct road.  returning Road object containing it";
        *built_road = *(&proposed_direct_road);
        return true;
      } 

      bool was_built = false;
      if (plotted_succesfully && !convolution_rejected){
        mutex_lock("AI::build_best_road calling game->build_road (direct road)");
        was_built = game->build_road(proposed_direct_road, player);
        mutex_unlock();
      }
      if (was_built) {
        AILogDebug["util_build_best_road"] << "" << calling_function << " zzz successfully built direct road from flag at " << start_pos << " to flag at " << target_pos;
        //roads_segments_built_this_target++;  // NO this is for DIRECT SOLUTIONS ONLY (how to handle passthru?  need to force it off??)
        complete_roads_built++;

        // ???????
        // how to handle this so it returns the complete road instead of a segment?
        //  AND - how is this used by calling function?  it likely needs updating
        //  ???????
        *built_road = *(&proposed_direct_road);

        // for debugging, keep track of the largest road built to get an idea of what is a reasonable limit to set for performance reasons
        //  also, consider making the limit an argument to this function, so that high limits can be allowed for certain roads but low limits
        //  for others in plot_road AI util function
        //if (proposed_direct_road.get_length() > longest_road_so_far.get_length()){
        //  AILogDebug["build_best_road"] << "this road is the new longest road built by this AI so far, with length " << proposed_direct_road.get_length() << ", highlighting it as ai_mark_road";
        //  longest_road_so_far = proposed_direct_road; 
        //  ai_mark_road = &longest_road_so_far;
        //}

        //AILogDebug["util_build_best_road"] << "" << calling_function << " spiderweb road debug, stored proposed_direct_road as built_road (which has mem addr " << built_road << "), proposed_direct_road source " << proposed_direct_road.get_source() << " built_road (should be same) source is " << built_road->get_source();
        return true;
      }
      else {
        // should have a series of fallback plans if a flag cannot be connected?   NO
        AILogDebug["util_build_best_road"] << "" << calling_function << " zzz failed to connect specified flag to road system! returning false";
        if (built_flag){
          AILogDebug["util_build_best_road"] << "" << calling_function << " zzz removing newly built flag that was intended to terminate this failed road";
          mutex_lock("AI::build_best_road calling game->demolish_flag (direct road to new flag failed)");
          game->demolish_flag(target_pos, player);
          mutex_unlock();
        }
        return false;
      }
    }

    //sofarduration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    //AILogDebug["util_build_best_road"] << "D. " << calling_function << " SO FAR call took " << sofarduration;
    //
    // ## non-Direct ## roads terminate at best scoring acceptable end_pos flag, which could be a direct route to the target flag or join an existing road
    //
    AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, trying to connect flag at start_pos " << start_pos << " to target_pos " << target_pos << " via road network using scoring system";
    RoadBuilder rb;
    rb.set_start_pos(start_pos);
    rb.set_target_pos(target_pos);
    int proad_index = 0;  // primary key for proads (and eroads!) part of this solution for unique identification of proads because can no longer use the ends/dirs to identify since adding passthru
    int eroad_index = 0;  // I don't think this is actually needed, just including to avoiding having to handle eroad RoadBuilderRoad separate from proad that does use index
    MapPosVector nearby_flags;   // CONSIDER MAKING THIS A SET instead of VECTOR because it should only have UNIQUE flags, and it is wasteful to pathfind the same solution twice!!
    // for passthru roads
    //  This is a Map of Vectors-of-Road objects comprising a full solution
    //  The key of the Map is the MapPos of the very end of entire multi-segment solution - which is nearby_flag_pos,
    //   This is the same as the proad key.  The start_pos is implied because all segments in this build_best_road call will have the same start_pos
    //  When a solution is attempted to build the Roads, the proad "end pos" will match the passthru
    //   map key and it will retrieve the ordered list of Road segments to build
    std::map<MapPos, Roads> passthru_road_segments;

    // if RoadOption::Improve is set and any paths already exist from start_pos, trace these roads and save for later scoring
    //   NOTE - if road caching disabled, and Improve not set, eroads aren't even used by roadbuilding code, only proads which are directly scored
    //     as eroads are traced each time they are considered
    if (map->has_flag(start_pos)){
      if (game->get_flag_at_pos(start_pos) != nullptr && game->get_flag_at_pos(start_pos)->is_connected() && road_options.test(RoadOption::Improve)) {
        AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, finding existing roads from start_pos " << start_pos << " for potential improvement";
        for (Direction dir : cycle_directions_cw()) {
          if (!map->has_path_IMPROVED(start_pos, dir)) {
            continue;
          }
          AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, found path from start_pos " << start_pos << " in dir " << NameDirection[dir];
          Road existing_road = trace_existing_road(map, start_pos, dir);
          RoadEnds ends = get_roadends(map, existing_road);
          //rb.new_eroad(ends, existing_road);
          rb.new_eroad(ends, existing_road, eroad_index++);  // the eroad_index isn't actually used, but I guess is created to satisfy Roadbuilder
          AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, created new eroad with eroad_index " << eroad_index;
          // add the other end flag to nearby_flags now to ensure it is scored
          MapPos end_pos = std::get<2>(ends);
          nearby_flags.push_back(end_pos);
        }
        AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, done finding existing roads from start_pos " << start_pos << ", nearby_flags currently has " << nearby_flags.size() << " elements";
      }
    }else{
      // should this return, or throw an exception maybe?  It seems wrong to try to Improve a nonexistent road, but I guess it is possible
      //  to set Improve to true for all road builds for other reasons?
      AILogWarn["util_build_best_road"] << "" << calling_function << " non-direct road requested, no flag exists at start_pos " << start_pos << " but RoadOption::Improve is set, this must be a pre-building check, continuing";
    }

    //
    // compile list of potential end flag MapPos to connect to (no Roads yet)
    //   only consider flags in a radius of the source, to avoid trying to connect to faraway flags
    //
    // IMPROVEMENT - determine the halfway point between the start_pos and target_pos, and center the radius there instead of using start_pos
    //        potential bug, if the target_pos is really far away from the start_pos, it might not find anything... that seems unlikely though.
    //         ...and that should happen even with the original method anyway so this isn't worse
    //          could consider a "tunnel" of multiple radiuses based on distance from start to target, but don't feel like coding that now
    // ISSUES - yes I think a "tunnel" would be best, seeing issues where long snakey roads are built, I believe because the flags near
    //  to the new building are not considered in the flag search, so it connects to some flag much farther away.
    //   for now, adding a second search of flags around the new building and including all those for consideration
    AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, compiling a list of nearby_flags to evaluate for connection to, nearby_flags already has " << nearby_flags.size() << " elements";

    AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, finding halfway point between start_pos and target_pos to center nearby_flag search";
    MapPos halfway_pos = get_halfway_pos(start_pos, target_pos);
    // for rebuild_all_roads, and possibly other reasons, always push the target_pos onto the nearby_flags list
    AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, as a fallback, adding target_pos " << target_pos << " to nearby_flags list";
    nearby_flags.push_back(target_pos);
    // because occasionally no flags are found within 6 tiles, will search up to 15 tiles
    //   BUT will quit any time after 6 tiles that at least one flag is found
    for (unsigned int i = 0; i < AI::spiral_dist(15); i++) {
      MapPos pos = map->pos_add_extended_spirally(halfway_pos, i);
      //ai_mark_pos.insert(std::make_pair(pos, "dk_brown"));
      //sleep_speed_adjusted(5));
      // skip if no flag, or pos not owned by this player
      if (!map->has_flag(pos) || map->get_owner(pos) != player_index) {
        continue;
      }
      if (start_pos == pos) {
        //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, nearby flags to halfway_pos - flag at pos " << pos << " is the same as start flag, skipping";
        continue;
      }
      // avoid disconnected flags, unless they are the flag of the target building
      // NOTE that this isn't smart enough to ignore flags that have some paths but are overall disconnected from the road system!
      //  this is a real bug that will need fixing otherwise disconnected roads will never be rejoined to road network
      /*
      if (!game->get_flag_at_pos(pos)->is_connected() && pos != castle_flag_pos && pos != target_pos) {
        AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, flag at pos " << pos << " is itself disconnected and is neither the castle_flag_pos nor the target_pos, skipping";
        continue;
      }*/
      // removing "allow target_pos" exception, it causes problems and is only used for rebuild_all_roads which doesn't work well
      if (game->get_flag_at_pos(pos) != nullptr && !game->get_flag_at_pos(pos)->is_connected() && pos != castle_flag_pos) {
        AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, nearby flags to halfway_pos - flag at pos " << pos << " is itself disconnected and is not the castle_flag_pos, skipping";
        continue;
      }

      // skip if already in nearby_flags list, which should only happen if it is part of existing road and RoadOption::Improve roads set;
      if (std::find(nearby_flags.begin(), nearby_flags.end(), pos) != nearby_flags.end()) {
        //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, nearby flags to halfway_pos - pos " << pos << " is already in nearby_flag list, skipping";
        continue;
      }

      // adding support for RoadOption::ReconnectNetwork
      //  CONSIDER REMOVING THIS CHECK AND ONLY CHECKING INSIDE THE for (end_pos : nearby_flags) loop!
      //   but then it will use up the "nearby flags" check with invalid options!
      if (road_options.test(RoadOption::ReconnectNetwork)){   
        MapPosVector notused = {};
        unsigned int notused2 = 0;
        if (find_flag_path_and_tile_dist_between_flags(map, start_pos, pos, &notused, &notused2, &ai_mark_pos)){
          AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, nearby flags to halfway_pos - RoadOption::ReconnectNetwork is true, flag at pos " << pos << " is on the same network, not including it";
          continue;
        }
      }

      AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, nearby flags to halfway_pos - found valid nearby_flag at pos " << pos;
      nearby_flags.push_back(pos);
      //ai_mark_pos.erase(pos);
      //ai_mark_pos.insert(std::make_pair(pos, "orange"));
      //sleep_speed_adjusted(10);

      // quit search after spiral_dist(6) tiles if at least one flag found
      //  this allows reaching a faraway flag, but avoids finding too many flags
      if (i >= AI::spiral_dist(6)) {
        // nearby_flags includes the target_pos flag so it is always at least 1, check for 2+
        if (nearby_flags.size() >= 2) {
          AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, nearby flags to halfway_pos - at least one flag found within 6 tiles of halfway_pos, quitting search";
          break;
        }
        else {
          AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, nearby flags to halfway_pos - no nearby flag found within 6 tiles of halfway_pos!  continuing expanded search until one found";
        }
      }
    } // foreach pos around halfway_pos
    AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, done finding flags near halfway_pos, nearby_flags currently has " << nearby_flags.size() << " elements";

    //sofarduration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    //AILogDebug["util_build_best_road"] << "E. " << calling_function << " SO FAR call took " << sofarduration;
    //
    // to avoid long snakey roads, also include flags around the start_pos (often a new building)
    //
    for (unsigned int i = 0; i < AI::spiral_dist(6); i++) {
      MapPos pos = map->pos_add_extended_spirally(start_pos, i);
      //ai_mark_pos.insert(std::make_pair(pos, "dk_cyan"));
      //sleep_speed_adjusted(5));
      // skip if no flag, or pos not owned by this player
      if (!map->has_flag(pos) || map->get_owner(pos) != player_index) {
        continue;
      }
      if (start_pos == pos) {
        //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, nearby flags to start_pos - flag at pos " << pos << " is the same as start flag, skipping";
        continue;
      }
      // avoid disconnected flags, unless they are the flag of the target building
      // NOTE that this isn't smart enough to ignore flags that have some paths but are overall disconnected from the road system!
      //  this is a real bug that will need fixing otherwise disconnected roads will never be rejoined to road network
      /*
      if (!game->get_flag_at_pos(pos)->is_connected() && pos != castle_flag_pos && pos != target_pos) {
        AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, nearby flags to start_pos - flag at pos " << pos << " is itself disconnected and is neither the castle_flag_pos nor the target_pos, skipping";
        continue;
      }*/

      // skip if already in nearby_flags list, which should only happen if it is part of existing road and RoadOption::Improve roads set;
      if (std::find(nearby_flags.begin(), nearby_flags.end(), pos) != nearby_flags.end()) {
        //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, nearby flags to start_pos - this pos is already in nearby_flag list, skipping";
        continue;
      }

      // adding support for RoadOption::ReconnectNetwork
      //  CONSIDER REMOVING THIS CHECK AND ONLY CHECKING INSIDE THE for (end_pos : nearby_flags) loop!
      if (road_options.test(RoadOption::ReconnectNetwork)){   
        MapPosVector notused = {};
        unsigned int notused2 = 0;
        if (find_flag_path_and_tile_dist_between_flags(map, start_pos, pos, &notused, &notused2, &ai_mark_pos)){
          AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, nearby flags to start_pos - RoadOption::ReconnectNetwork is true, flag at pos " << pos << " is on the same network, not including it";
          continue;
        }
      }

      // removing "allow target_pos" exception, it causes problems and is only used for rebuild_all_roads which doesn't work well
      if (game->get_flag_at_pos(pos) != nullptr && !game->get_flag_at_pos(pos)->is_connected() && pos != castle_flag_pos) {
        AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, nearby flags to start_pos - flag at pos " << pos << " is itself disconnected and is not the castle_flag_pos, skipping";
        continue;
      }

      //ai_mark_pos.erase(pos);
      //ai_mark_pos.insert(std::make_pair(pos, "cyan"));
      //sleep_speed_adjusted(10);
      AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, nearby flags to start_pos - found valid nearby_flag at pos " << pos;
      nearby_flags.push_back(pos);
    } // foreach pos around start_pos
    //ai_mark_pos.clear();
    AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, done finding flags near the start_pos, nearby_flags currently has " << nearby_flags.size() << " elements";

    AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, done finding nearby flags, found " << nearby_flags.size() << " nearby_flags";
    if (nearby_flags.size() < 1) {
      AILogDebug["util_build_best_road"] << "" << calling_function << " no nearby_flags found!  maybe castle_flag should always be added as a last resort...";
      AILogDebug["util_build_best_road"] << "" << calling_function << " because no nearby_flags found, no roads can be between start_pos " << start_pos << " and target_pos " << target_pos;
      // it seems likely that if the first target can't be reached, any second affinity target can't either
      //  likely this flag can't be connected to the road system at all now.  Just quit here
      return false;
    }

    //sofarduration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    //AILogDebug["util_build_best_road"] << "F. " << calling_function << " SO FAR call took " << sofarduration;
    //
    // plot Roads to the found nearby flags ends and score them in terms of NEW LENGTH
    //    store any that are "good enough" (under the max_convolution limit)
    //      quit plotting roads once max_plotting_steps_per_target limit is reached,     NOT IMPLEMENTED YET
    //     which is the aggregate tile_dist of ALL plotted roads so far to this target  NOT IMPLEMENTED YET
    //        this is used to limit the amount of time spent plotting roads as realm becomes crowded    NOT IMPLEMENTED YET
    //

    //
    //  THIS LOOKS WRONG!  It seems to be checking the ideal_length from start_pos to target_pos ONLY, but using that
    //   as the ideal length for convolution checks for OTHER NEARBY EXISTING/FOUND FLAGS!  making them appear much
    //   worse in terms of convolution than they actually are!!!  
    //  TRYING FIX - moving this logic inside the foreach loop
    // WAIT NEVERMIND!!  this should be left as-is, because ultimately the goal is to connect to the target_pos for solutioning
    //  and so a road that connects far away just to reach the target pos SHOULD BE excluded as inefficient/convoluted even if
    //  the new path itself is not convoluted.  restoring the way this was
    //
    // use straight-line-distance to the actual target_pos as the measuring stick to judge how convoluted actual roads are be in comparison
    AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, about to plot roads to nearby_flags, checking tile_dist from start_pos " << start_pos << " to end_pos " << target_pos << " for an ideal road";
    int ideal_length = AI::get_straightline_tile_dist(map, start_pos, target_pos);
    AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, about to plot roads to nearby_flags, flag target_pos at " << target_pos << " has straight-line tile distance " << ideal_length << " from start_pos " << start_pos;
    
    //sofarduration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    //AILogDebug["util_build_best_road"] << "F1. " << calling_function << " SO FAR call took " << sofarduration;
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
      AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, plotting direct roads to nearby_flags, preparing to plot road from start_pos " << start_pos << " to nearby_flag pos " << end_pos;
      
      // it might be faster to have plot_road return the RoadEnds, but it seems messier
      
      //
      // first plot a road directly from start_pos to end_pos ( note that this is not the same as RoadOption::Direct ! )
      //  if a road can be plotted directly, disqualify it if already unacceptable, otherwise include in potential solution list
      // 
      // NOTE - the split_roads vector will be filled by this plot_road call with all potential splitting Road solutions! 
      //
      Roads split_roads;  // moving this here inside loop so it isn't accumulating ALL split_roads from ALL real flags
      
      Road potential_road = plot_road(map, player_index, start_pos, end_pos, &split_roads, road_options.test(RoadOption::HoldBuildingPos));

      /* this proves the cache is effective!
      // DEBUG - performance testing plot_road caching  
      AILogDebug["util_build_best_road"] << " REPEAT cache on";
      Road perf_test_junk1 = plot_road(map, player_index, start_pos, end_pos, &split_roads, road_options.test(RoadOption::HoldBuildingPos));
      //use_plot_road_cache = false;
      AILogDebug["util_build_best_road"] << " REPEAT cache off";
      Road perf_test_junk2 = plot_road(map, player_index, start_pos, end_pos, &split_roads, road_options.test(RoadOption::HoldBuildingPos));
      AILogDebug["util_build_best_road"] << " REPEAT DONE";
      */

      // this while(true) loop looks goofy to me but it was the only clean way to do it without a GOTO statement
      while (true){
        if (potential_road.get_length() == 0) {
          AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, plotting direct roads to nearby_flags, unable to plot_road a DIRECT road from start_pos " << start_pos << " to nearby_flag pos " << end_pos << ", skipping direct road";
          break;
        }
        AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, plotting direct roads to nearby_flags, potential DIRECT road from start_pos " << start_pos << " to nearby_flag pos " << end_pos << " has new segment length " << potential_road.get_length();
        double convolution = static_cast<double>(potential_road.get_length()) / static_cast<double>(ideal_length);
        AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, plotting direct roads to nearby_flags, potential DIRECT road NEW length: " << potential_road.get_length() << ", ideal TOTAL length: " << ideal_length << ", convolution ratio: " << convolution;
        if (convolution >= max_convolution) {
          AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, plotting direct roads to nearby_flags, this potential DIRECT road solution is already too convoluted from new length alone, max is " << max_convolution;
          break;
        }
        if (road_options.test(RoadOption::MostlyStraight) && convolution >= max_convolution * 0.50) {
          AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, plotting direct roads to nearby_flags, RoadOption::MostlyStraight is set and this potential DIRECT road solution is already too convoluted from new length alone, reduced max is " << max_convolution * 0.50;
          break;
        }

        // adding support for RoadOption::ReconnectNetwork
        //  WOULD IT MAKE MORE SENSE TO HAVE THIS BE THE ONLY ReconnectNetwork CHECK AND DITCH ALL THE EARLIER ONES?
        //
        if (road_options.test(RoadOption::ReconnectNetwork)){   
          MapPosVector notused = {};
          unsigned int notused2 = 0;
          if (find_flag_path_and_tile_dist_between_flags(map, start_pos, end_pos, &notused, &notused2, &ai_mark_pos)){
            AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, plotting direct roads to nearby_flags, - RoadOption::ReconnectNetwork is true, flag at pos " << end_pos << " is on the same network, not including it";
            break;
          }
        }

        // if this is "tracked economy building", ensure the end_pos flag is closest to the currently selected Inventory (castle/warehouse)
        if (verify_stock == true){
          // this should use FLAG ONLY  (wait no, FlagAndStraightLine is more restrictive and good for placing new buildings
          if (find_nearest_inventory(map, player_index, end_pos, DistType::FlagAndStraightLine, &ai_mark_pos) != inventory_pos){
          //if (find_nearest_inventory(map, player_index, end_pos, DistType::FlagOnly, &ai_mark_pos) != inventory_pos){
            AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, plotting direct roads to nearby_flags, verify_stock - DIRECT road - flag at end_pos " << end_pos << " is not closest to current inventory_pos " << inventory_pos << ", skipping";
            break;
          }
        }
        AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, plotting direct roads to nearby_flags, this potential DIRECT road solution is acceptable so far in terms of NEW length only, adding to RoadBuilder potential_roads";
        RoadEnds potential_road_ends = get_roadends(map, potential_road);
        //rb.new_proad(potential_road_ends, potential_road);
        rb.new_proad(potential_road_ends, potential_road, proad_index++);
        AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, created new proad DIRECT to nearby_flag, proad_index = " << proad_index;
        AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, plotting direct roads to nearby_flags, there are currently " << rb.get_proads().size() << " potential_roads in the list";
        break;
      } // while true - find direct road

      //sofarduration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
     //AILogDebug["util_build_best_road"] << "F2. " << calling_function << " SO FAR call took " << sofarduration;

      //
      // now check the list of splitting flag solutions that were likely found by the plot_road call directly
      //  preceeding this, disqualify any that are already unacceptable, and include the rest as potential solutions
      //                                      maybe make this a recursive call instead?
      // IDEA TO POSSIBLY, MAJORLY IMPROVE PERFORMANCE OF ROAD SPLITTING:
      //  instead of evaluating each split individually, when there could be MANY on an existing road
      //   consider the two end flags of the existing road, if either is unacceptable then don't consider splits
      //   along this road because the split can only be worse (right?).
      //  Only consider the splits if the real road ends +2 tiles and +1 flag would be acceptable
      //
      //
      if (road_options.test(RoadOption::SplitRoads) == false) {
        AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, RoadOption SplitRoads is false, not considering split-road solutions";
        continue;
      }
      AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, RoadOption SplitRoads is true, preparing to iterate over potential split roads found in previous plot_road call";
      AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, there are " << split_roads.size() << " Road entries in the split_roads list that was provided by the last plot_roads call to nearby map pos " << end_pos;
      for (Road split_road : split_roads) {
        //sofarduration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
        //AILogDebug["util_build_best_road"] << "F3. " << calling_function << " SO FAR call took " << sofarduration;
        RoadEnds split_road_ends = get_roadends(map, split_road);
        MapPos split_end_pos = split_road.get_end(map.get());

        //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, a potential split_road has end_pos: " << split_road.get_end(map.get()) << ", for comparison, start_pos is " << start_pos << " and split_road source is " << split_road.get_source();

        // removing this check now that passthru allowed, it is no longer possible to look up a proad by its ends, only by its INDEX!
        //if (rb.has_proad(split_road_ends)){
        //  //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, this road is already on the proads list, skipping";
        //  continue;
        //}

        if (split_road.get_length() == 0) {
          AILogWarn["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, road unexpectedly has zero length!  this should not happen, find out why!  skipping";
          //AILogWarn["util_build_best_road"] << "PAUSING GAME";
          //game->pause();
          //AILogWarn["util_build_best_road"] << "SLEEPING AI FOR LONG TIME";
          //std::this_thread::sleep_for(std::chrono::milliseconds(1000000));
          //AILogWarn["util_build_best_road"] << "DONE SLEEPING";
          continue;
        }

        //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, split_road from start_pos " << start_pos << " to new-flag pos " << split_end_pos << " (on way to nearby_flag pos " << end_pos << ") has new segment length " << split_road.get_length();
        double convolution = static_cast<double>(split_road.get_length()) / static_cast<double>(ideal_length);
        //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, split_road NEW length: " << split_road.get_length() << ", ideal TOTAL length: " << ideal_length << ", convolution ratio: " << convolution;
        if (convolution >= max_convolution) {
          //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, this split_road solution is already too convoluted from new length alone, max is " << max_convolution;
          continue;
        }
        if (road_options.test(RoadOption::MostlyStraight) && convolution >= max_convolution * 0.50) {
          //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, RoadOption::MostlyStraight is set and this split_road solution is already too convoluted from new length alone, reduced max is " << max_convolution * 0.50;
          continue;
        }

        // various reasons below to disqualify a flag
        int disqualified = 0;

        // adding support for RoadOption::ReconnectNetwork
        //  this could code likely be combined with if verify_stock code below for performance
        //  but for now keeping them separate for simplicity
        //  CONSIDER REMOVING THIS CHECK AND ONLY CHECKING INSIDE THE for (end_pos : nearby_flags) loop!
        //  CONSIDER REMOVING THIS CHECK AND ONLY CHECKING INSIDE THE for (end_pos : nearby_flags) loop!
        if (road_options.test(RoadOption::ReconnectNetwork)){   
          AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, RoadOption::ReconnectNetwork is true, no flag here, checking to see if adjacent flags are part of same road network";
          // because new-splitting/fake-flag solutions may be allowed when doing ReconnectNetwork, we must check for this and use either adjacent flag to do to search
          //  as the find_flag_path_and_tile_dist_between_flags function won't work for fake flags.
          for (Direction dir : cycle_directions_cw()) {
            if (map->has_path_IMPROVED(split_end_pos, dir)) {
              Road split_road = trace_existing_road(map, split_end_pos, dir);
              MapPos adjacent_flag_pos = split_road.get_end(map.get());
              MapPosVector notused = {};
              unsigned int notused2 = 0;
              if (find_flag_path_and_tile_dist_between_flags(map, start_pos, adjacent_flag_pos, &notused, &notused2, &ai_mark_pos)){
                AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, RoadOption::ReconnectNetwork is true, splitting-flag at pos " << split_end_pos << " is on the same network, not including it";
                disqualified++;
                break;
              }
            }
          }          
        }

        // if this is "tracked economy building", ensure the end_pos flag is closest to the currently selected Inventory (castle/warehouse)
        // because this is a split_flag solution, the flag doesn't exist yet.  Instead, check to ensure that BOTH of its adjacent flags
        // are closest to the current Inventory.  I am not sure what happens if two Inventories are equal flag distance apart, but I suspect
        // whichever is first in terms of path-Direction (0-5) wins
        if (verify_stock == true){
          for (Direction dir : cycle_directions_cw()) {
            AILogVerbose["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, verify_stock - looking for a path for splitting flag at split_end_pos " << split_end_pos << " in dir " << NameDirection[dir];
            if (map->has_path_IMPROVED(split_end_pos, dir)) {
              Road split_road = trace_existing_road(map, split_end_pos, dir);
              //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, verify_stock - found a path for splitting flag at split_end_pos " << split_end_pos << " in dir " << NameDirection[dir];
              MapPos adjacent_flag_pos = split_road.get_end(map.get());
              //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, verify_stock - path for splitting flag at split_end_pos " << split_end_pos << " in dir " << NameDirection[dir] << " ends at flag at pos " << adjacent_flag_pos;
              // this should use FLAG ONLY  (wait no, FlagAndStraightLine is more restrictive and good for placing new buildings
              if (find_nearest_inventory(map, player_index, adjacent_flag_pos, DistType::FlagAndStraightLine, &ai_mark_pos) != inventory_pos){
              //if (find_nearest_inventory(map, player_index, adjacent_flag_pos, DistType::FlagOnly, &ai_mark_pos) != inventory_pos){
                //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, verify_stock - potential split_road flag at split_end_pos " << split_end_pos << " is not closest to current inventory_pos " << inventory_pos << ", skipping";
                disqualified++;
                break;
              }
            }
          }
        }
        if (disqualified > 0){
          //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, verify_stock - this split_end_pos is disqualified, skipping it";
          continue;
        }
        //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, this split_road road solution is acceptable so far in terms of NEW length only, adding to RoadBuilder potential_roads";
        // add it to the proads list
        AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, found acceptable-so-far split_road with start_pos " << split_road.get_source() << " and end_pos: " << split_road.get_end(map.get()) << ", creating new proad entry for it";
        //rb.new_proad(split_road_ends, split_road);
        rb.new_proad(split_road_ends, split_road, proad_index++);
        AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, created new splitting proad, proad_index = " << proad_index;
        //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, there are currently " << rb.get_proads().size() << " potential_roads in the list";

      } // foreach split road

      //sofarduration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
      //AILogDebug["util_build_best_road"] << "F4. " << calling_function << " SO FAR call took " << sofarduration;

      //AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, split_roads, done checking split_roads for this potential direct road, there are currently " << rb.get_proads().size() << " potential_roads in the list";

    } // foreach end_pos nearby flags

    AILogDebug["util_build_best_road"] << "" << calling_function << " non-direct road requested, done checking all direct and split_roads for start_pos " << start_pos << " to target_pos " << target_pos << ", there are currently " << rb.get_proads().size() << " potential_roads in the list";

    // END OF FINDING POTENTIAL END FLAG TO CONNECT TO
    //sofarduration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    //AILogDebug["util_build_best_road"] << "G  .  " << calling_function << " SO FAR call took " << sofarduration;

    //=====================================================================================
    // score the potential new roads to nearby_flags in terms of:
    //     tile_dist (from end_flag to target flag)
    //     flag_dist (from end_flag to target flag);
    //     new_length (of the potential road to be built from start_pos to nearby_flag pos
    AILogDebug["util_build_best_road"] << "" << calling_function << " preparing to score rb.get_proads() list";

    //
    // FIRST, score any existing roads(eroad) already attached to start_pos for later comparison to potential new
    //   These should only be here if RoadOption::Improve was set earlier in upstream functions
    //
    //MapPosSet scored_eroads;
    MapPos best_eroad_pos = bad_map_pos;
    unsigned int best_eroad_score = bad_score;
    if (map->has_flag(start_pos) && game->get_flag_at_pos(start_pos) != nullptr && game->get_flag_at_pos(start_pos)->is_connected() && road_options.test(RoadOption::Improve)) {
      AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_eroads, checking eroads because start_pos already has paths and RoadOption::Improve is set";
      for (std::pair<RoadEnds, RoadBuilderRoad*> er : rb.get_eroads()) {
        RoadEnds ends = er.first;
        MapPos start_pos = er.second->get_end1();
        MapPos start_dir = er.second->get_dir1();
        MapPos nearby_eroad_flag_pos = er.second->get_end2();
        AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_eroads, found an existing road from start_pos " << start_pos << " in dir " << NameDirection[start_dir] << " to nearby_flag_pos " << nearby_eroad_flag_pos << ", checking score to target_pos " << target_pos;
        AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_eroads, debug - checking score for nearby_eroad_flag_pos " << nearby_eroad_flag_pos << " to target_pos " << target_pos << " BEFORE find_flag_and_tile_dist is called to see if there will be a collision";
        AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_eroads, debug - BEFORE score: flag_dist " << rb.get_score(nearby_eroad_flag_pos).get_flag_dist() << ", tile dist " << rb.get_score(nearby_eroad_flag_pos).get_tile_dist();
        
        // USING NEW FUNCTION
        //if (!find_flag_and_tile_dist(map, player_index, &rb, nearby_eroad_flag_pos, target_pos, &ai_mark_pos)) {
        //  AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_eroads, eroad: find_flag_and_tile_dist returned false for nearby_eroad_flag_pos " << nearby_eroad_flag_pos << "!";
        //  continue;
        //}
        MapPosVector found_flags = {};
        unsigned int xtile_dist = 0;
        bool xcontains_castle_flag = false;
        if(find_flag_path_and_tile_dist_between_flags(map, nearby_eroad_flag_pos, target_pos, &found_flags, &xtile_dist, &ai_mark_pos)){
          AILogDebug["score_flag"] << "" << calling_function << " scoring_eroads, splitting_flag find_flag_path_and_tile_dist_between_flags() returned true from flag_pos " << nearby_eroad_flag_pos << " to target_pos " << target_pos;
          // manually set rb stuff because the new function doesn't use it
          unsigned int flag_dist = found_flags.size();
          for(MapPos flag_pos : found_flags){
            if (flag_pos == castle_flag_pos){
              AILogDebug["score_flag"] << "" << calling_function << " scoring_eroads, this solution contains the castle flag!";
              xcontains_castle_flag = true;
              break;
            }
          }
          rb.set_score(nearby_eroad_flag_pos, flag_dist, xtile_dist, xcontains_castle_flag);
        }else{
          continue;
        }
        AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_eroads, eroad: find_flag_path_and_tile_dist_between_flags returned true for nearby_flag_pos " << nearby_eroad_flag_pos;

        AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_eroads, scoring_roads, debug - checking score for nearby_eroad_flag_pos " << nearby_eroad_flag_pos << " to target_pos " << target_pos << " AFTER find_flag_and_tile_dist is called to see if there was a collision";
        AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_eroads, debug - AFTER score: flag_dist " << rb.get_score(nearby_eroad_flag_pos).get_flag_dist() << ", tile dist " << rb.get_score(nearby_eroad_flag_pos).get_tile_dist();

        AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_eroads, preparing to apply penalties to eroad from start_pos " << start_pos << " in dir " << NameDirection[start_dir] << " to nearby_eroad_flag_pos " << nearby_eroad_flag_pos;
        FlagScore nearby_eroad_flag_score = rb.get_score(nearby_eroad_flag_pos);
        unsigned int tile_dist = nearby_eroad_flag_score.get_tile_dist();
        unsigned int flag_dist = nearby_eroad_flag_score.get_flag_dist();
        bool contains_castle_flag = nearby_eroad_flag_score.get_contains_castle_flag();
        // no new length for an existing road, this means it will be preferred over a similiar
        //   potential new road after new_length penalty applied.  A new road must be significantly better to be built
        unsigned int new_length = 0;
        AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_eroads, BEFORE applying any penalties, potential road from start_pos " << start_pos << " in dir " << NameDirection[start_dir] << " to nearby_eroad_flag_pos " << nearby_eroad_flag_pos <<
          " has tile_dist " << tile_dist << ", flag_dist " << flag_dist << ", new_length " << new_length << " to target_pos " << target_pos;
        //----------------------------------------------------------------------------------------------
        // RoadOption penalties applied here!   * 1 means no penalty, only in place for easy adjustment
        //----------------------------------------------------------------------------------------------
        unsigned int adjusted_score = static_cast<unsigned int>(static_cast<double>(tile_dist * 1) + static_cast<double>(flag_dist * 1));
        //----------------------------------------------------------------------------------------------
        if (road_options.test(RoadOption::PenalizeCastleFlag) && contains_castle_flag == true) {
          if (target_pos == castle_flag_pos){
            AILogDebug["util_build_best_road"] << "not applying contains_castle_flag penalty because the target *is* the castle flag pos!";
          }else{
            adjusted_score += contains_castle_flag_penalty;
            AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_eroads, applying contains_castle_flag penalty of " << contains_castle_flag_penalty;
          }
        }
        AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_eroads, AFTER penalties, existing road from start_pos " << start_pos << " in dir " << NameDirection[start_dir] << " to nearby_eroad_flag_pos " << nearby_eroad_flag_pos <<
          " has tile_dist " << tile_dist << ", flag_dist " << flag_dist << ", new_length " << new_length << ", adjusted_score " << adjusted_score << " to target_pos " << target_pos;
        if (adjusted_score < best_eroad_score) {
          AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_eroads, new best_eroad_score found: " << adjusted_score << " from pos " << nearby_eroad_flag_pos;
          best_eroad_score = adjusted_score;
          best_eroad_pos = nearby_eroad_flag_pos;
        }
      }
      AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_eroads, done finding eroads, best_eroad_score found: " << best_eroad_score << " from pos " << best_eroad_pos;
    }
    else {
      AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_eroads, start_pos " << start_pos << " has no flag, no paths, or RoadOption::Improve not set, not checking for eroads";
    } // if start_pos has any paths/eroads already

    //sofarduration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    //AILogDebug["util_build_best_road"] << "H. " << calling_function << " SO FAR call took " << sofarduration;
    
    //
    // foreach potential new road (proad) to be built to a nearby flag
    //  score the existing road (eroad) that begins at the flag/pos where the proad ends
    //
    AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_proads, preparing to score potential new roads from start_pos to nearby_flag positions";
    //MapPosSet scored_proads;
    std::vector<std::pair<int, unsigned int>> scored_proads;  // proad_index is key, score is value
    //for (std::pair<RoadEnds, RoadBuilderRoad*> pr : rb.get_proads()) {
    for (std::pair<int, RoadBuilderRoad*> pr : rb.get_proads()) {
      //RoadEnds ends = pr.first;
      int proad_index = pr.first;
      RoadBuilderRoad *rbroad = pr.second;
      MapPos start_pos = pr.second->get_end1();
      MapPos nearby_flag_pos = pr.second->get_end2();
      
      // debug - print all scores for this solution
      //for (std::pair<MapPos, unsigned int> adjusted_score : scored_proads){
      for (std::pair<int, unsigned int> adjusted_score : scored_proads){
        AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_proads, debug printing ALL scores, found index " << adjusted_score.first << " has score " << adjusted_score.second;
      }

      AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_proads, rb.get_proads() contains a potential new road to nearby_flag_pos " << nearby_flag_pos;
      // why is this scoring route to castle_flag pos?  shouldn't it be to target_pos for affinity building??
      //if (!score_flag(map, player_index, &rb, road_options, nearby_flag_pos, castle_flag_pos, &ai_mark_pos)) {
      // trying this instead - may04 2020
      //  WAIT WAIT WAIT, i think castle_flag_pos is only used to pass the location of castle flag so the contains_castle_flag bool can be set!
      //     it isn't actually using it as the target!  setting it back
      //if (!score_flag(map, player_index, &rb, road_options, nearby_flag_pos, target_pos, &ai_mark_pos)) {
      if (!score_flag(map, player_index, &rb, road_options, nearby_flag_pos, castle_flag_pos, &ai_mark_pos)) {
        AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_proads, proad: score_flag returned false for nearby_flag_pos " << nearby_flag_pos << "!";
        continue;
      }
      AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_proads, proad: score_flag returned true for nearby_flag_pos " << nearby_flag_pos;
      // then add the score of the new segment and apply any penalties
      //    and insert adjusted_scores into MapPosSet for sorting later
      AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_proads, preparing to apply penalties to proad from start_pos " << start_pos << " to nearby_flag_pos " << nearby_flag_pos;
      FlagScore nearby_flag_score = rb.get_score(nearby_flag_pos);
      unsigned int tile_dist = nearby_flag_score.get_tile_dist();
      unsigned int flag_dist = nearby_flag_score.get_flag_dist();
      bool contains_castle_flag = nearby_flag_score.get_contains_castle_flag();
      unsigned int new_length = static_cast<unsigned int>(rbroad->get_road().get_length());
      AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_proads, BEFORE applying any penalties, potential road from start_pos " << start_pos << " to nearby_flag_pos " << nearby_flag_pos <<
        " has tile_dist " << tile_dist << ", flag_dist " << flag_dist << ", new_length " << new_length;
      //----------------------------------------------------------------------------------------------
      // RoadOption penalties applied here!   * 1 means no penalty, only in place for easy adjustment
      //----------------------------------------------------------------------------------------------
      // apr13 2020 increased new_length penalty from 2.0 to 2.5
      // jan02 2022 increased new_length penalty from 2.50 to 2.75, then to 3.00
      // jan02 2022 found solution to what I thought was caused by new length penalty, but was in fact caused by real flags
      //             not being considered in "fake flag" solutions when discovered a long the way but not the original target
      //             so connections were being made to new splitting flags instead of appropriate existing flags because the
      //             existing flag was not the target of the plot_road call (because it only considers halfway point first, then others)
      //             and so existing flag was ignored in favor of nearby fake-flag.  changing this back to 2.0 to re-evaluate!
      // jan03 2022 ANOTHER UPDATE - setting this very high for stock/warehouse, attempt to reduce road clutter around them
      // jan12 2023 since implementing passthru, increasing new_length_penalty a bit more for both castle & stocks
      double new_length_penalty;
      if (road_options.test(RoadOption::PenalizeNewLength)) {
        if (inventory_pos != castle_flag_pos){
          // non-castle (Stocks) parallel economies
          //new_length_penalty = 5.00;  // this might be too high
          //new_length_penalty = 3.50;  // this might be too high
          new_length_penalty = 4.00;
        }else{
          // castle economy
          //new_length_penalty = 2.00;
          new_length_penalty = 3.00;
        }
      }
      else {
        new_length_penalty = 1.00; // no penalty
      }
      if (road_options.test(RoadOption::ReducedNewLengthPenalty)) {
        new_length_penalty = 1.50;
      }
      if (road_options.test(RoadOption::IncreasedNewLengthPenalty)){
        new_length_penalty = new_length_penalty * 2;
      }
      AILogDebug["util_build_best_road"] << "" << calling_function << " using new_length_penalty of " << new_length_penalty << "x for this scoring, based on RoadOptions";
      unsigned int adjusted_score = static_cast<unsigned int>(static_cast<double>(tile_dist * 1) + static_cast<double>(new_length) * new_length_penalty + static_cast<double>(flag_dist * 1));
      if (road_options.test(RoadOption::PenalizeCastleFlag) && contains_castle_flag == true) {
        if (target_pos == castle_flag_pos){
          AILogDebug["util_build_best_road"] << "" << calling_function << " not applying contains_castle_flag penalty because the target *is* the castle flag pos!";
        }else{
          adjusted_score += contains_castle_flag_penalty;
          AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_proads, applying contains_castle_flag penalty of " << contains_castle_flag_penalty;
        }
      }

      //
      // handle passthru roads
      //  - sanity check (impossible solutions will be frequently found as the pathfinder doesn't check new-Flag validity)
      //  - store each partial Road segment in passthru_road_segments map, indexed by the MapPos of the end of the entire solution (the nearby_flag_pos)
      //  - add the passthru flag count to the flag_dist score for the overall solution
      //
      ai_mark_pos.clear();
      int passthru_flags = 0;
      MapPos pos = start_pos;
      MapPos segment_start_pos = start_pos;
      Direction start_dir = rbroad->get_road().get_first();
      MapPos last_new_flag_pos = bad_map_pos;
      Road this_solution_passthru_road_segment;
      Roads this_solution_passthru_road_segments = {};
      this_solution_passthru_road_segment.start(start_pos);

      /* this logic is now implemented!  delete this code once confirmed stable
      // now that following along EXISTING paths is allowed for passthru solutions, must somehow indicate this
      //  so that we do not attempt to build a new path "along this path" which is obviously not possible
      // for now, just note that the condition is identified
      if (map->has_path(start_pos, start_dir)){
        AILogDebug["util_build_best_road"] << "in passthru check, NOTE - this road segment follow along an existing path!  it will fail to build, new logic needed";
        AILogError["util_build_best_road"] << "in passthru check, NOTE - this road segment follow along an existing path!  marking start_pos in green, next pos in red, and pausing AI";
        ai_mark_pos.insert(ColorDot(start_pos, "green"));
        ai_mark_pos.insert(ColorDot(map->move(start_pos,start_dir), "red"));
        AILogWarn["util_build_best_road"] << "SLEEPING AI FOR LONG TIME";
        std::this_thread::sleep_for(std::chrono::milliseconds(1000000));
        AILogWarn["util_build_best_road"] << "DONE SLEEPING";
      }
      */

      bool reject_solution = false;
      //ai_mark_pos.insert(ColorDot(start_pos, "blue"));
      AILogDebug["util_build_best_road"] << "tracing this Road's paths to see if it is a passthru solution";
      for (Direction dir : rbroad->get_road().get_dirs()){
        AILogDebug["util_build_best_road"] << "in passthru check, found Dir " << dir;
        //rbroad->set_is_passthru_solution();  // do NOT set this here!  if it is invalidated it will confuse things later

        if (reject_solution){ break; }
        MapPos last_pos = pos;
        pos = map->move(pos, dir);
        AILogDebug["util_build_best_road"] << "in passthru check, pos is " << pos;

        this_solution_passthru_road_segment.extend(dir);
        // if this pos has no flag and is blocked by an existing path, but a flag could be built here AND this is not the end pos, this is a new-splitting passthru flag
        //  OR
        // if this pos has a flag but it isn't the start or end flag, this is an existing passthru flag
        if ( (!map->is_road_segment_valid(last_pos, dir) && game->can_build_flag(pos, player) && map->has_any_path(pos) && pos != nearby_flag_pos)
          || (map->has_flag(pos) && pos != start_pos && pos != nearby_flag_pos)
                ){
          AILogDebug["util_build_best_road"] << "" << calling_function << " this is a passthru solution";
          passthru_flags++;  // ... it is a passthru new splitting flag
          // Flags cannot be created right next to each other, so reject any solution that depends on this
          // ALSO, if the end of the road ALSO requires a new flag, reject if the end of the road is one pos away
          if (passthru_flags > 1){
            AILogDebug["util_build_best_road"] << "" << calling_function << " passthru check sanity-checking pos surrounding " << pos << " to see if any are the last_new_flag_pos " << last_new_flag_pos;
            for (Direction check_dir : cycle_directions_cw()){
              if (map->move(pos, check_dir) == last_new_flag_pos
               || map->move(pos, check_dir) == nearby_flag_pos){
                AILogDebug["util_build_best_road"] << "" << calling_function << " passthru check sanity-check rejecting this passthru solution because it depends on multiple new flags impossibly close to each other";
                reject_solution = true;
                //ai_mark_pos.insert(ColorDot(pos, "red"));
                //sleep_speed_adjusted(200);
                // hmm, it seems that the plot_road filtering cannot be relied on to reject "new flag too close to another flag" passthru solutions
                //  because there is a single open/closed list shared by entire search, but no specific splitting pos is always invalid, it is only in
                //  the scope of a complete road solution that this can be determined
                //throw ExceptionFreeserf("build_best_road passthru sanity check failed!  this previously passed check during plot_road call, how could this happen?");
                break;
              }
            }
            if (reject_solution){ continue; }
          }
          if (passthru_flags > max_passthru_flags_per_solution){
            AILogDebug["util_build_best_road"] << "" << calling_function << " breached max_passthru_flags_per_solution of " << max_passthru_flags_per_solution << ", rejecting this solution";
            reject_solution = true;
            break;
          }

          //if (map->has_flag(pos)){
          //  ai_mark_pos.insert(ColorDot(pos, "lt_green"));
          //}else{
            //ai_mark_pos.insert(ColorDot(pos, "yellow"));
          //}
          MapPos segment_end_pos = pos;
          last_new_flag_pos = segment_end_pos;
          this_solution_passthru_road_segments.push_back(this_solution_passthru_road_segment);
          AILogDebug["util_build_best_road"] << "" << calling_function << " passthru sanity-check ok, appending this partial Road with segment_start_pos " << segment_start_pos << " segment_end_pos " << segment_end_pos << " to segments vector for orig start_pos " << start_pos << ", and final end pos " << nearby_flag_pos << ", this solution now contains " << this_solution_passthru_road_segments.size() << " Roads";
          // reset the Road so it can be used for next segment
          this_solution_passthru_road_segment.invalidate(); // clear it
          this_solution_passthru_road_segment.start(segment_end_pos);  // start it with current pos
          //AILogDebug["util_build_best_road"] << "" << calling_function << " DEBUG - current passthru Road this_solution_passthru_road_segment now contains " << this_solution_passthru_road_segment.get_dirs().size() << " elements";
          AILogDebug["util_build_best_road"] << "" << calling_function << " passthru Roads list this_solution_passthru_road_segments now contains " << this_solution_passthru_road_segments.size() << " elements";

        }else if (pos == nearby_flag_pos) {
          //ai_mark_pos.insert(ColorDot(pos, "dk_blue"));  // end pos
          AILogDebug["util_build_best_road"] << "" << calling_function << " passthru check, this pos " << pos << " is the end pos flag";
        }else{
          //ai_mark_pos.insert(ColorDot(pos, "white"));  // normal pos
          AILogDebug["util_build_best_road"] << "" << calling_function << " passthru check, this pos " << pos << " contains nothing blocking";
        }
        //sleep_speed_adjusted(200);

        AILogDebug["util_build_best_road"] << "" << calling_function << " DEBUG2 - current passthru Road this_solution_passthru_road_segment now contains " << this_solution_passthru_road_segment.get_dirs().size() << " elements";

      }

      AILogDebug["util_build_best_road"] << "" << calling_function << " DEBUG3 - current passthru Road this_solution_passthru_road_segment now contains " << this_solution_passthru_road_segment.get_dirs().size() << " elements";

      if (!reject_solution){
        if (passthru_flags > 0){
          rbroad->set_is_passthru_solution();

          AILogDebug["util_build_best_road"] << "" << calling_function << " done passthru check, this is a passthru solution with " << passthru_flags << " passthru_flags, adding these to flag_dist";
          flag_dist += passthru_flags;

          // append the last segment of the solution now!
          if (segment_start_pos == this_solution_passthru_road_segment.get_end(map.get())){
            // if the last segment is zero length, it isn't actually a passthru segment
            //  but rather a splitting flag termination point (though it could be at the end 
            //  of a passthru solution
            AILogDebug["util_build_best_road"] << "" << calling_function << " final segment start and end pos are same, " << segment_start_pos << ", this must be the end of the road, not adding another segment";
          }else{
            this_solution_passthru_road_segments.push_back(this_solution_passthru_road_segment);
            AILogDebug["util_build_best_road"] << "" << calling_function << " appending final segment Road with segment_start_pos " << segment_start_pos << " segment_end_pos " << this_solution_passthru_road_segment.get_end(map.get()) << " to this_solution_passthru_road_segments vector for original start_pos " << start_pos << ", and final end pos " << nearby_flag_pos << ", this solution now contains " << this_solution_passthru_road_segments.size() << " Roads";
          }

          AILogDebug["util_build_best_road"] << "" << calling_function << " done passthru check, this is a passthru solution comprised of " << this_solution_passthru_road_segments.size() << " roads, storing in passthru_road_segments map ";
          // ????????
          // does this also need a unique index / primary key instead of using MapPos?
          //  ?????????
          passthru_road_segments.insert(std::make_pair(nearby_flag_pos, this_solution_passthru_road_segments));
        }else{
          AILogDebug["util_build_best_road"] << "" << calling_function << " done passthru check, this is not a passthru solution";
        }
        //sleep_speed_adjusted(200);
      }

      if (reject_solution){
        // this doesn't work because it sets bad_score for the nearby_flag_pos which might have other better routes available?
        AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_proads, this solution was rejected, setting bad_score " << bad_score << " for nearby_flag_pos " << nearby_flag_pos;
        //scored_proads.insert(std::make_pair(nearby_flag_pos, bad_score));
        scored_proads.push_back(std::make_pair(proad_index, bad_score));
      }else{
        AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_proads, AFTER penalties, potential road from start_pos " << start_pos << " to nearby_flag_pos " << nearby_flag_pos <<
          " has tile_dist " << tile_dist << ", flag_dist " << flag_dist << ", new_length " << new_length << ", adjusted_score " << adjusted_score;
        //scored_proads.insert(std::make_pair(nearby_flag_pos, adjusted_score));
        scored_proads.push_back(std::make_pair(proad_index, adjusted_score));
      }

    }  // foreach potential new road found rb.get_proads()

    AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_proads, done scoring proads";


    // sort the potential new roads by their score, best (lowest) score first
    AILogDebug["util_build_best_road"] << "" << calling_function << " scoring_proads, preparing to sort scored_proads into a MapPosVector";
    //MapPosVector sorted_scored_proads = sort_by_val_asc(scored_proads);
    std::vector<unsigned int> sorted_scored_proads = sort_scores_by_val_asc(scored_proads);

    // debug - print all scores for this solution
    //for (std::pair<MapPos, unsigned int> adjusted_score : scored_proads){
    for (std::pair<int, unsigned int> adjusted_score : scored_proads){
      AILogDebug["util_build_best_road"] << "" << calling_function << " sorted_scored_proads, debug printing ALL scores, found index " << adjusted_score.first << " has score " << adjusted_score.second;
    }

    //sofarduration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    //AILogDebug["util_build_best_road"] << "I. " << calling_function << " SO FAR call took " << sofarduration;

    //
    // try to build a road, starting with the best scoring potential_road end flag, until successful
    //
    AILogDebug["util_build_best_road"] << "" << calling_function << " sorted_scored_proads, preparing to iterate over sorted_scored_proads to try to build roads to best nearby_flag";
    //for (MapPos end_pos : sorted_scored_proads) {
    for (int proad_index : sorted_scored_proads) {

      // because sorted_scored_proads are now indexed by a primary key instead of
      //  an end_pos (which might not actually be unique!) we should be able to 
      //  determine the "end" pos (as opposed to the start_pos) simply because
      //  there is only a single fixed start_pos for each solution so the pos
      //  that is NOT the start pos must by the end pos
      RoadBuilderRoad *proad = rb.get_proad(proad_index);
      MapPos end1_pos = proad->get_end1();
      MapPos end2_pos = proad->get_end2();
      MapPos end_pos = bad_map_pos;
      if (end1_pos == start_pos){
        end_pos = end2_pos;
        AILogDebug["util_build_best_road"] << "" << calling_function << " sorted_scored_proads, proad end1_pos is start_pos " << start_pos << ", so the end_pos must be the other pos " << end_pos;
      }else if (end2_pos == start_pos){
        end_pos = end1_pos;
        AILogDebug["util_build_best_road"] << "" << calling_function << " sorted_scored_proads, proad end2_pos is start_pos " << start_pos << ", so the end_pos must be the other pos " << end_pos;
      }else{
        // fail
        AILogDebug["util_build_best_road"] << "" << calling_function << " sorted_scored_proads, neither of proad end pos end1_pos " << end1_pos << " nor end2_pos " << end2_pos << " match start_pos " << start_pos << "!  crashing";
        throw ExceptionFreeserf("sorted_scored_proads, neither of proad end pos end1_pos nor end2_pos match start_pos");
      }
      

      // debug - find the score for this solution
      unsigned int score = 0;
      bool found_score = false;
      for (std::pair<MapPos, unsigned int> adjusted_score : scored_proads){
        if (adjusted_score.first == proad_index){
          score = adjusted_score.second;
          found_score = true;
          break;
        }
      }
      if (!found_score){
        throw ExceptionFreeserf("score not found for end_pos in sorted_scored_proad");
      }

      AILogDebug["util_build_best_road"] << "" << calling_function << " sorted_scored_proads, considering building road from start_pos " << start_pos << " to end_pos " << end_pos << ", this proad has score " << score;
      if (complete_roads_built >= target_count) {
        AILogDebug["util_build_best_road"] << "" << calling_function << " sorted_scored_proads, complete_roads_built " << complete_roads_built << " >= target_count " << target_count << ", done building roads";
        return true;
      }

      //RoadBuilderRoad *proad = rb.get_proad(end_pos);
      // this is now retrieved earlier so start_pos and end_pos can be determined
      //RoadBuilderRoad *proad = rb.get_proad(proad_index);

      if (road_options.test(RoadOption::ReconnectNetwork)){
        AILogDebug["util_build_best_road"] << "" << calling_function << " RoadOption::ReconnectNetwork set, NOT checking vs existing (ineffective) solutions";
      }else{
        // check and see if this solution is actually better than the existing best complete
        //   solution from start_pos to target_pos (eroad score)
        // if not, skip this target and move on to next target
        if (game->get_flag_at_pos(start_pos) != nullptr && game->get_flag_at_pos(start_pos)->is_connected() && road_options.test(RoadOption::Improve)) {
          bool skip_target = false;
          //for (std::pair<MapPos, unsigned int> ppair : scored_proads) {
          for (std::pair<int, unsigned int> ppair : scored_proads) {
            if (ppair.first != proad_index) {
              continue;  // this is correct here, do NOT break
              // NO NO NO // now that passthru is allowed, must break instead of continue
              // NO NO NO //  as multiple road segments could comprise a solution, and this check must reject the ENTIRE solution
              //break; // NO DO NOT BREAK HERE AS THIS IS A CHECK FOR A MATCHING INDEX ONLY!
            }
            AILogDebug["util_build_best_road"] << "" << calling_function << " checking vs existing solution, found adjusted_score of " << ppair.second << " for this proad ending at end_pos " << end_pos;
            // it must be SIGNIFICANTLY better, say 50% + 2?
            // reducing from 50% to 20% + 2 on dec04 2021
            double modified_prod_score = static_cast<double>(ppair.second) * static_cast<double>(1.20) + 2;
            //if (best_eroad_score <= ppair.second) {
            if (best_eroad_score <= modified_prod_score) {
              AILogDebug["util_build_best_road"] << "" << calling_function << " checking vs existing solution, proad from start_pos " << start_pos << " to end_pos " << end_pos << " has +20%+2 score " << modified_prod_score << " which is not significantly better than best_eroad_score " << best_eroad_score << ", skipping this target";
              skip_target = true;
              break;
            }
            else {
              AILogDebug["util_build_best_road"] << "" << calling_function << " checking vs existing solution, proad from start_pos " << start_pos << " to end_pos " << end_pos << " has +20%+2 score " << modified_prod_score << " which is significantly better than best_eroad_score " << best_eroad_score << ", will build new road";
              break;
            }
          }
          if (skip_target == true) {
            AILogDebug["util_build_best_road"] << "" << calling_function << " checking vs existing solution, skip_target is true, breaking from foreach sorted_scored_proads - not building road to this target";
            break;
          }
        }
        else {
          AILogDebug["util_build_best_road"] << "" << calling_function << " sorted_scored_proads, start_pos " << start_pos << " has no paths, or RoadOption::Improve not set, not comparing this solution to any best_eroad_score";
        } // is potential new solution better than existing solution?
      } // if RoadOption::ReconnectNetworks

      Roads roads_to_build;
      if (proad->get_is_passthru_solution()){
        AILogDebug["util_build_best_road"] << "" << calling_function << " this is a passthru multi-segment Road, collecting road segments from passthru_road_segments map";
        AILogDebug["util_build_best_road"] << "" << calling_function << " looking for an entry in passthru_road_segments map for end_pos " << end_pos;
        if (passthru_road_segments.count(end_pos) > 0){
          AILogDebug["util_build_best_road"] << "" << calling_function << " found entry in passthru_road_segments for end_pos " << end_pos << ", it contains " << passthru_road_segments.at(end_pos).size() << " individual Road segments";
          for (Road segment : passthru_road_segments.at(end_pos)){
            AILogDebug["util_build_best_road"] << "" << calling_function << " adding Road with start pos " << segment.get_source() << " and end pos " << segment.get_end(map.get());
            roads_to_build.push_back(segment);
          }
        }else{
          AILogError["util_build_best_road"] << "" << calling_function << " could not find passthru_road_segments entry for this proad solution with end_pos " << end_pos << "! skipping this solution";
          //continue;
          // now that passthru is allowed, must break instead of continue
          //  as multiple road segments could comprise a solution, and this length check must reject the ENTIRE solution
          break;
        }
      }else{
        AILogDebug["util_build_best_road"] << "" << calling_function << " this is a normal Road, not a passthru";
        //Road road = proad->get_road();
        roads_to_build.push_back(proad->get_road());
      }

      //
      // build the road[s] to this target
      //
      int road_segment_num = 0;
      int road_segments_count = roads_to_build.size();
      //MapPosVector failed_solution_flags = {};  // ugh, need to track and demolish any created Road segments also, skipping this for now
      for (Road road : roads_to_build){
        road_segment_num++;  // start at 1 for human readability
        MapPos this_segment_start_pos = road.get_source();
        MapPos this_segment_end_pos = road.get_end(map.get());
        Direction this_segment_start_dir = road.get_first();
        AILogDebug["util_build_best_road"] << "" << calling_function << " segment " << road_segment_num << " of " << road_segments_count << ", preparing to build road from this_segment_start_pos " << this_segment_start_pos << " to " << this_segment_end_pos << ", with final end pos " << end_pos << " and original start_pos of " << start_pos;

        /* no longer allowing water roads for AI - jan13 2023
        //
        // check to see if this is a water road about to be built
        //  it should only be possible for BOTH ends to be in water, but checking both just to be sure
        //
        bool water_road = false;
        if (map->road_segment_in_water(this_segment_end_pos, reverse_direction(road.get_last()))){
          AILogDebug["util_build_best_road"] << "" << calling_function << " preparing to build road, this_segment_end_pos " << this_segment_end_pos << " is in water!";
          water_road = true;
        }
        if (map->road_segment_in_water(this_segment_start_pos, road.get_first())){
          AILogDebug["util_build_best_road"] << "" << calling_function << " preparing to build road, this_segment_start_pos " << this_segment_start_pos << " is in water!";
          water_road = true;
        }
        */

        /* no longer allowing AI to create water roads at all
        //  tlongstretch Jan 2023
        if (water_road) {
          if (!road_options.test(RoadOption::AllowWaterRoad)) {
            AILogDebug["util_build_best_road"] << "" << calling_function << " preparing to build road, this solution is a water road, but AllowWaterRoad RoadOption is not set, skipping";
            //continue;
            // now that passthru is allowed, must break instead of continue
            //  as multiple road segments could comprise a solution, and this length check must reject the ENTIRE solution
            break;
          }
          RoadOptions recursive_land_road_options = road_options;
          recursive_land_road_options.reset(RoadOption::AllowWaterRoad);
          // sanity check, disallow water roads from multi-road passthru solutions
          if (road_segments_count > 0){
            AILogError["util_build_best_road"] << "" << calling_function << " found a passthru solution containing a water road!  this is not possible";
            throw ExceptionFreeserf("found a passthru solution containing a water road!  this is not possible");
          }
          // does this return true if an existing (better?) land road already exists?? or only if it creates one?
          Road notused; // not used here
          bool land_road_was_built = AI::build_best_road(start_pos, recursive_land_road_options, &notused);  // it is okay to use regular start_pos here because multi-road solutions forbidden
          if (land_road_was_built) { 
            AILogDebug["util_build_best_road"] << "" << calling_function << " preparing to build road, successfully built land road, allowing a water road to " << start_pos;  // it is okay to use regular start_pos here because multi-road solutions forbidden
          }
          else {
            AILogDebug["util_build_best_road"] << "" << calling_function << " preparing to build road, failed to build land road, NOT allowing a water road to " << start_pos;  // it is okay to use regular start_pos here because multi-road solutions forbidden
            //continue;
            // now that passthru is allowed, must break instead of continue
            //  as multiple road segments could comprise a solution, and this length check must reject the ENTIRE solution
            break;
          }
        }
        */



        //    *************************************************************************
        //      I THINK THIS CHECK NEEDS TO BE MOVED OUTSIDE OF THE road_sements LOOP
        //        to the complete road solutions loop!!!!!!
        //    *************************************************************************
        // if there are two targets to connect to check if the second road is actually better 
        //  than the first road built (if one was built) even though the target is different
        // If it is not better, do not build.  Often the targets are in the same general
        //  direction and there is no benefit to a second road
        // NOTE - this should be comparing the adjusted proad score of the first road
        //  to the first target, TO the score of the first road to the SECOND target
        if (targets.size() > 1){
          if (complete_roads_built == 0){
            // this is the first road
            // store the end_pos of this road so it can be compared for the next one
            // note that this is the FINAL END POS in case this is a multi-path solution, not the segment end pos
            //  because the intent is to compare the score of the existing true end pos
            first_target_road_end = end_pos;   // end_pos IS NOT A TYPO!
            AILogDebug["util_build_best_road"] << "" << calling_function << " this is the first of " << targets.size() << " targets, saving first_target_road_end of " << first_target_road_end << " for next target road to compare to";
          }else{
            // this is the 2nd+ road
            // find the adjusted score of the first road to the first target
            // to the adjusted score of the *first road* to the *second target*
            // by using the scored_proads from the *second target* (does it share one scored_proad set?)
            unsigned int this_target_score = bad_score;
            unsigned int first_target_score = bad_score;
            for (std::pair<MapPos, unsigned int> pair : scored_proads){
              if (pair.first == end_pos){ this_target_score = pair.second; }  // end_pos IS NOT A TYPO!
              if (pair.first == first_target_road_end){ first_target_score = pair.second; }
            }
            if (this_target_score == bad_score && first_target_score == bad_score){
              AILogWarn["util_build_best_road"] << "" << calling_function << " this is the second of " << targets.size() << " targets, BOTH two scores is bad_score!  not sure what this means, quitting this build_best_road call, returning true because at least one road built";
              // return true because at least one road was built
              return true;
            }
            if (this_target_score == bad_score || first_target_score == bad_score){
              AILogWarn["util_build_best_road"] << "" << calling_function << " this is the second of " << targets.size() << " targets, one of the two scores is bad_score!  not comparing! continueing... is that okay?  this_target_score " << this_target_score << ", first_target_score " << first_target_score;
              //continue;
              // now that passthru is allowed, must break instead of continue
              //  as multiple road segments could comprise a solution, and this length check must reject the ENTIRE solution
              break;
            }
            if (this_target_score + 10 < first_target_score){  // it must be significantly better
              AILogDebug["util_build_best_road"] << "" << calling_function << " this is the second of " << targets.size() << " targets, and this adjusted proad score of " << this_target_score << " is significantly better than the other road's score of " << first_target_score << " will try to create the road";
            }else{
              // don't try additional positions because they all have worse scores than the first, because they are sorted by score
              AILogDebug["util_build_best_road"] << "" << calling_function << " this is the second of " << targets.size() << " targets, and this adjusted proad score of " << this_target_score << " is NOT significantly better than the other road's score of " << first_target_score << ", not building a second road for this building";
              // return true because the first road was successful
              return true;
            }
          }
        }
        // end of "is this a 2nd target complete road, and if so is the solution actually better than the last one built?"
        //*****************************************************************************************************************



        // avoid creation extremely long roads
        if (road.get_length() > 30){
          AILogDebug["util_build_best_road"] << "" << calling_function << " road is over 30 tiles long!  not building this road";
          //continue;
          // now that passthru is allowed, must break instead of continue
          //  as multiple road segments could comprise a solution, and this length check must reject the ENTIRE solution
          break;
        }

        // add support for RoadOption::PlotOnlyNoBuild
        //   INVESTIGATE HOW THIS IS USED, SEE IF IT CONFLICTS WITH NEW AllowPassthru LOGIC!
        if (road_options.test(RoadOption::PlotOnlyNoBuild)){
          AILogDebug["util_build_best_road"] << "" << calling_function << " RoadOption::PlotOnlyNoBuild is set, not actually building the road.  returning Road object containing it";
          *built_road = *(&road);
          return true;
        } 

        // build a new flag if this solution ends with no flag (i.e. a split road solution)
        bool created_new_flag = false;
        if (!map->has_flag(this_segment_end_pos)) {
          AILogDebug["util_build_best_road"] << "" << calling_function << " this_segment_end_pos " << this_segment_end_pos << ", on way to final end_pos " << end_pos << " has no flag, must be fake flag/split road, trying to create a real flag";
          mutex_lock("AI::build_best_road calling game->build_flag (split road)");
          bool was_built = game->build_flag(this_segment_end_pos, player);
          mutex_unlock();
          if (was_built) {
            AILogDebug["util_build_best_road"] << "" << calling_function << " successfully built new flag at this_segment_end_pos " << this_segment_end_pos << ", on way to final end_pos " << end_pos << ", splitting the road";
            created_new_flag = true;
            // disabling this as it doesn't even work 100% and slows things down
            //   or maybe, only do this for initial castle and not once multiple Inventories?
            //  ALSO - this is itself creating new split-road solutions which is wasteful!
            // RE-ENABLING, I think this is actually essential!!!
            //  find a better way to fix up the end of the road without having to re-do the entire pathfinding

          /* temp disabling again when testing passthru sanity checking early rejection and recheck
            // IMPROVEMENT/hack 
            //  because split-road solutions do not "heapify, the last segment of the road is not sorted by "score", and often dog-legs for no good reason when a straight
            //  segment is possible.  It isn't obvious to me how to re-heapify the splitting path when it is being made, so instead, re-plot the road as a direct road so it
            //  gets the actual best path.  This means the road actually built should be at least as good as the originally scored splitting road, but could be 1 tile better!
            //  the HoldBuildingPos feature must be passed also to be sure replotted road can't interfere
            AILogDebug["util_build_best_road"] << "" << calling_function << " because this is a splitting flag solution, REPLOT THE ROAD so the last segment can't dogleg";
            Roads notused;  // this already a splitting road, no additional split roads considered
            //Road road = plot_road(map, player_index, start_pos, end_pos, &notused, road_options.test(RoadOption::HoldBuildingPos));
            road = plot_road(map, player_index, this_segment_start_pos, this_segment_end_pos, &notused, road_options.test(RoadOption::HoldBuildingPos));
          */
          }else {
            AILogDebug["util_build_best_road"] << "" << calling_function << " failed to build flag at this_segment_end_pos " << this_segment_end_pos << ", on way to final end_pos " << end_pos << ", FIND OUT WHY!  not trying to build this road.  marking pos in cyan";
            ai_mark_pos.insert(ColorDot(this_segment_end_pos, "cyan"));
            //// I'm not sure what is causing this yet, but it seems to happen for many-segment complex roads near busy areas, and isn't all that of a big deal to use alternate solution
            //AILogWarn["util_build_best_road"] << "SLEEPING AI FOR LONG TIME";
            //std::this_thread::sleep_for(std::chrono::milliseconds(1000000));
            //AILogWarn["util_build_best_road"] << "DONE SLEEPING";
            //continue;
            // now that passthru is allowed, must break instead of continue
            //  as multiple road segments could comprise a solution, and this length check must reject the ENTIRE solution
            //break;
            // UPDATE - I am seeing rare issue where a bunch of cyan pos on raods appear for failed flags, and then I see
            //   a bunch of junk flags polluting the same area, I think from related failed passthru solutions where it 
            //   seems to have created new splitting flags but then abandoned the solution.  As a work-around trying 
            //   a return here instead of break
            // saw this Jan19 when building Stonecutter
            AILogDebug["util_build_best_road"] << "" << calling_function << " failed to build flag at this_segment_end_pos " << this_segment_end_pos << ", on way to final end_pos " << end_pos << ", FIND OUT WHY!  not trying to build this road.  RETURNING EARLY";
            return false;
          }
        }// if no flag at  this_segment_end_pos
          
        // build the road!
        //ai_mark_road = &road;

        // now that multi-segment passthru routes are allowed, we must check EACH SEGMENT to see if
        //  there already exists a road connection between the two segment flags, and if this proposed new
        //  path is significantly better than the existing one.  Because the passthru logic disallows following
        //  existing routes, it will be VERY COMMON for a passthru route to be plotted that suggests new road 
        //  segments when it would be much better to instead use the existing path for that segment, even if other
        //  segments are new.
        bool skip_segment = false;
        
        /*
        // check and see if this segment is significantly shorter than any existing segment between the same two flags
        if (map->has_flag(this_segment_start_pos) && map->has_flag(this_segment_end_pos)) {
          AILogDebug["util_build_best_road"] << "" << calling_function << " segment_start_pos " << this_segment_start_pos << ", segment_end_pos " << this_segment_end_pos << ", checking for existing solution for this segment, flags already exist at both segment start and segment end pos, checking for a path between them";
          for (Direction check_dir : cycle_directions_cw()){
            if (map->has_path_IMPROVED(road.get_source(), check_dir)){
              AILogDebug["util_build_best_road"] << "" << calling_function << " segment_start_pos " << this_segment_start_pos << ", segment_end_pos " << this_segment_end_pos << ", checking for existing solution for this segment, there is a path from segment start in Dir" << check_dir;
              Flag *segment_start_flag = game->get_flag_at_pos(this_segment_start_pos);
              //if (segment_start_flag == nullptr){
              if (segment_start_flag->get_other_end_flag(check_dir)->get_position() == this_segment_end_pos){
                AILogDebug["util_build_best_road"] << "" << calling_function << " segment_start_pos " << this_segment_start_pos << ", segment_end_pos " << this_segment_end_pos << ", checking for existing solution for this segment, Dir" << check_dir << " leads to segment_end_pos flag at " << this_segment_end_pos;
                Road existing_segment_road = trace_existing_road(map, this_segment_start_pos, check_dir);
                AILogDebug["util_build_best_road"] << "" << calling_function << " segment_start_pos " << this_segment_start_pos << ", segment_end_pos " << this_segment_end_pos << ", checking for existing solution for this segment, existing road between these two flags has length " << existing_segment_road.get_length();
                if (road.get_length() * 1.5 < existing_segment_road.get_length()){
                  AILogDebug["util_build_best_road"] << "" << calling_function << " segment_start_pos " << this_segment_start_pos << ", segment_end_pos " << this_segment_end_pos << ", checking for existing solution for this segment, this new road segment is significantly better than the current one, will try to build it";
                }else{
                  AILogDebug["util_build_best_road"] << "" << calling_function << " segment_start_pos " << this_segment_start_pos << ", segment_end_pos " << this_segment_end_pos << ", checking for existing solution for this segment, this road segment is not significantly better than the current one, skipping it to re-use existing";
                  skip_segment = true;
                  break;
                }
              }
            }
          }
        }*/

        // check that this new segment is significantly better than any existing complete multi-flag path that may exist between these two flags
        if (map->has_flag(this_segment_start_pos) && map->has_flag(this_segment_end_pos)) {
          AILogDebug["util_build_best_road"] << "" << calling_function << " segment_start_pos " << this_segment_start_pos << ", segment_end_pos " << this_segment_end_pos << ", checking for existing solution for this segment, flags already exist at both segment start and segment end pos, checking any existing connection";
          //if (find_nearest_inventory(map, player_index, flag_pos, DistType::FlagAndStraightLine, &ai_mark_pos) != inventory_pos){
          MapPosVector existing_solution_flags = {};  // this is the list of flags in the solution, will be used to count flag score
          unsigned int existing_solution_tile_dist = 0;
          if (find_flag_path_and_tile_dist_between_flags(map, this_segment_start_pos, this_segment_end_pos, &existing_solution_flags, &existing_solution_tile_dist, &ai_mark_pos)){
            AILogDebug["util_build_best_road"] << "" << calling_function << " segment_start_pos " << this_segment_start_pos << ", segment_end_pos " << this_segment_end_pos << ", found existing solution with flag count " << existing_solution_flags.size() << " and tile count " << existing_solution_tile_dist;
            float new_segment_versus_existing_penalty = 1.5f;
            if (road_options.test(RoadOption::IncreasedNewLengthPenalty)){
               new_segment_versus_existing_penalty = new_segment_versus_existing_penalty * 2;
            }
            if (road.get_length() * new_segment_versus_existing_penalty + 8 < existing_solution_tile_dist + existing_solution_flags.size()){  // path pos with flags are penalized one aditional point like usual
              AILogDebug["util_build_best_road"] << "" << calling_function << " segment_start_pos " << this_segment_start_pos << ", segment_end_pos " << this_segment_end_pos << ", found existing solution, this new road segment is significantly better than the current solution, will try to build it";
            }else{
              AILogDebug["util_build_best_road"] << "" << calling_function << " segment_start_pos " << this_segment_start_pos << ", segment_end_pos " << this_segment_end_pos << ", found existing solution, this new road segment is not significantly better than the current solution, skipping it to re-use existing";
              skip_segment = true;
            }
          }else{
            AILogDebug["util_build_best_road"] << "" << calling_function << " segment_start_pos " << this_segment_start_pos << ", segment_end_pos " << this_segment_end_pos << ", no existing connection found between these flags";
          }
        }

        /*

          // at the very least, this segment doesn't need to be built
          //for (std::pair<MapPos, unsigned int> ppair : scored_proads) {
          for (std::pair<int, unsigned int> ppair : scored_proads) {
            if (ppair.first != proad_index) {
              continue;  // this is correct here, do NOT break
              // NO NO NO // now that passthru is allowed, must break instead of continue
              // NO NO NO //  as multiple road segments could comprise a solution, and this check must reject the ENTIRE solution
              //break; // NO DO NOT BREAK HERE AS THIS IS A CHECK FOR A MATCHING INDEX ONLY!
            }
            AILogDebug["util_build_best_road"] << "" << calling_function << " checking vs existing solution, found adjusted_score of " << ppair.second << " for this proad ending at end_pos " << end_pos;
            // it must be SIGNIFICANTLY better, say 50% + 2?
            // reducing from 50% to 20% + 2 on dec04 2021
            double modified_prod_score = static_cast<double>(ppair.second) * static_cast<double>(1.20) + 2;
            //if (best_eroad_score <= ppair.second) {
            if (best_eroad_score <= modified_prod_score) {
              AILogDebug["util_build_best_road"] << "" << calling_function << " checking vs existing solution, proad from start_pos " << start_pos << " to end_pos " << end_pos << " has +20%+2 score " << modified_prod_score << " which is not significantly better than best_eroad_score " << best_eroad_score << ", skipping this target";
              skip_target = true;
              break;
            }
            else {
              AILogDebug["util_build_best_road"] << "" << calling_function << " checking vs existing solution, proad from start_pos " << start_pos << " to end_pos " << end_pos << " has +20%+2 score " << modified_prod_score << " which is significantly better than best_eroad_score " << best_eroad_score << ", will build new road";
              break;
            }
          }
          if (skip_target == true) {
            AILogDebug["util_build_best_road"] << "" << calling_function << " checking vs existing solution, skip_target is true, breaking from foreach sorted_scored_proads - not building road to this target";
            break;
          }
        }
        else {
          AILogDebug["util_build_best_road"] << "" << calling_function << " sorted_scored_proads, start_pos " << start_pos << " has no paths, or RoadOption::Improve not set, not comparing this solution to any best_eroad_score";
        } // is potential new solution better than existing solution?
        */

        
        // DEBUG
        AILogDebug["util_build_best_road"] << "" << calling_function << " DEBUG PRINTING DIRS about to build road with source=" << this_segment_start_pos << ", end=" << this_segment_end_pos;
        for (Direction debug_dir : road.get_dirs()){
          AILogDebug["util_build_best_road"] << "" << calling_function << " DEBUG PRINTING DIRS, Dir" << debug_dir;
        }
        AILogDebug["util_build_best_road"] << "" << calling_function << " DONE PRINTING DIRS about to build road with source=" << this_segment_start_pos << ", end=" << this_segment_end_pos;

        // now that re-use of existing paths as segments is allowed for multi-road passthru solutions,
        //  we cannot attempt (and fail) to build these paths as they already exist
        // To avoid this, see if there is already a path IN THE DIRECTION of the Road dir
        //  and if so simply don't build, but continue with the next solution part
        if (map->has_path(this_segment_start_pos, this_segment_start_dir)){
          AILogDebug["util_build_best_road"] << "" << calling_function << " this road segment appears to be re-using an existing path";
          if (road_segments_count > 1){
            AILogDebug["util_build_best_road"] << "" << calling_function << " this road segment appears to be re-using an existing path and is part of a multi-road passthru solution so it is allowed, not attempting to build this segment";
            skip_segment = true;  // could just 'continue' here
          }else{
            AILogError["util_build_best_road"] << "" << calling_function << " this road segment appears to be re-using an existing path but it is a single segment solution, nothing to build";
            // I think this is actually not a problem at all, preventing it would mean extra logic in plot_road that isn't really needed.  Simply skip this solution
            skip_segment = true;  // could just 'continue' here
            /* 
            //return false;
            // instead of returning false, for now throw exception for debugging
            //throw ExceptionFreeserf("util_build_best_road found re-use of existing path in single path solution");
            AILogError["util_build_best_road"] << "" << calling_function << " this road segment appears to be re-using an existing path, marking start in blue and end in yellow";
            ai_mark_pos.insert(ColorDot(this_segment_start_pos, "blue"));
            ai_mark_pos.insert(ColorDot(this_segment_end_pos, "yellow"));
            AILogWarn["util_build_best_road"] << "SLEEPING AI FOR LONG TIME";
            std::this_thread::sleep_for(std::chrono::milliseconds(1000000));
            AILogWarn["util_build_best_road"] << "DONE SLEEPING";
            */
          }
        }

        if (skip_segment){
          AILogDebug["util_build_best_road"] << "" << calling_function << " not building this road segment because skip_segment is true";
        }else{
          // build the Road!
          AILogDebug["util_build_best_road"] << "" << calling_function << " about to build road with source=" << this_segment_start_pos << ", end=" << this_segment_end_pos << " and size " << road.get_length();
          mutex_lock("AI::build_best_road calling game->build_road (non-direct road)");
          bool was_built = game->build_road(road, game->get_player(player_index));
          mutex_unlock();
          if (was_built) {
            //roads_built++;
            AILogDebug["util_build_best_road"] << "" << calling_function << " successfully built road segment from " << this_segment_start_pos << " to " << this_segment_end_pos << ", with final end_pos " << end_pos << " as specified in PotentialRoad, roads segments built this target solution: " << road_segment_num;

            // see if this is the last (or only) segment
            if (road_segment_num == road_segments_count){
              AILogDebug["util_build_best_road"] << "" << calling_function << " successfully built complete road solution from original start_pos " << start_pos << " to final end_pos " << end_pos << ", " << road_segment_num << " out of " << road_segments_count << ", complete road solutions (targets) built: " << complete_roads_built;
              complete_roads_built++;

              // ???????
              // how to handle this so it returns the complete road instead of a segment?
              //  AND - how is this used by calling function?  it likely needs updating
              //  ???????
              // set this so the calling function can tell the exact Road built if it needs it
              //   I guess this only works to return the last road built...
              *built_road = *(&road);
              
              // for debugging, keep track of the largest road built to get an idea of what is a reasonable limit to set for performance reasons
              //  also, consider making the limit an argument to this function, so that high limits can be allowed for certain roads but low limits
              //  for others in plot_road AI util function
              //if (road.get_length() > longest_road_so_far.get_length()){
              //  AILogDebug["build_best_road"] << "this road is the new longest road built by this AI so far, with length " << road.get_length() << ", highlighting it as ai_mark_road";
              //  longest_road_so_far = road; 
              //  ai_mark_road = &longest_road_so_far;
              //}

              //continue;
            }else{
              AILogDebug["util_build_best_road"] << "" << calling_function << " this road solution is not done building yet, still " << road_segments_count - road_segment_num << "segments remain in this target's solution";
            }
            continue;
          }else{
            AILogWarn["util_build_best_road"] << "" << calling_function << " ERROR - failed to build road from " << this_segment_start_pos << " to " << this_segment_end_pos << ", with final end_pos " << end_pos << ", FIND OUT WHY!  marking pos in dk_green, trying next best solution...";
            /* disabling this for now as it happens only for really convoluted solutions that aren't very helpful anyway
            ai_mark_pos.erase(end_pos);
            ai_mark_pos.insert(ColorDot(end_pos, "dk_green"));
            AILogWarn["util_build_best_road"] << "SLEEPING AI FOR LONG TIME";
            std::this_thread::sleep_for(std::chrono::milliseconds(1000000));
            AILogWarn["util_build_best_road"] << "DONE SLEEPING";
            AILogWarn["util_build_best_road"] << "" << calling_function << " ERROR - failed to build road, returning early for debugging";
            return false;
            */
            break;
          }

          // if a new flag was built but the road creation failed, destroy the newly built flag
          if (created_new_flag) {

            // this should create a list of flags (and Roads) build and remove them all, but not bothering with this yet, still testing the initial logic
            //AILogDebug["util_build_best_road"] << "" << calling_function << " marking the newly created flag at this_segment_end_pos " << this_segment_end_pos << " for deletion so it doesn't pollute the road network";
            //failed_solution_flags.push_back(this_segment_end_pos);

            AILogDebug["util_build_best_road"] << "" << calling_function << " removing the newly created flag at end_pos so it doesn't screw up the rest of the road solutions";
            mutex_lock("AI::build_best_road calling game->demolish_flag (couldn't connect new flag)");
            game->demolish_flag(end_pos, game->get_player(player_index));
            mutex_unlock();
          }

          if (!was_built){
            AILogDebug["util_build_best_road"] << "" << calling_function << " failed to create at least one segment in this road segment list, abandoning the entire solution";
            break;
          }

          AILogDebug["util_build_best_road"] << "" << calling_function << " successfully built road from " << this_segment_start_pos << " to " << this_segment_end_pos << ", with final end_pos " << end_pos << " and original start_pos of " << start_pos;

        } // if !skip_segment (because it is a re-used path part of a multi-part passthru solution

      }  // for each road_to_build (which is always one for non-passthru roads, but can be multiple for passthru roads)
         //  this is NOT the same as multiple TARGETS, up to two targets can receive individual roads, which may or may not be passthru 

    } // end foreach end_pos (of sorted_scored_road solutions)

    AILogDebug["util_build_best_road"] << "" << calling_function << " done building roads from original start_pos " << start_pos << " to final target_pos " << target_pos;

  } //foreach target

  // return success if at least one road built, even if two targets found

  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_build_best_road"] << "" << calling_function << " build_best_road call took " << duration;

  if (complete_roads_built < 1) {
    AILogDebug["util_build_best_road"] << "" << calling_function << " failed to connect specified flag to road system!";
    return false;
  }
  AILogDebug["util_build_best_road"] << "" << calling_function << " done with build_best_road, complete_roads_built: " << complete_roads_built << ", out of target_count: " << target_count;
  // sleep a bit to be more human like
  sleep_speed_adjusted(2000);
  return true;
}

MapPosVector
// return a vector of MapPos of affinity building[s], or a default if none defined in BuildingAffinity table (castle)
//***************************************
// THIS FUNCTION SHOULD EITHER RETURN TWO FIXED ITEMS OR TWO LISTS OF POTENTIAL TARGET BUILDINGS IN ORDER OF DIST/BUILD PROGRESS!
//((((((((((((((((((()))))))))))))))))))
//
AI::get_affinity(MapPos flag_pos, Building::Type optional_affinity_building_type){
  AILogDebug["util_get_affinity"] << "inside AI::get_affinity for flag_pos " << flag_pos;

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
  AILogDebug["util_get_affinity"] << "adding flag pos of nearest_inventory_flag pos " << nearest_inventory_flag << " to affinity list as a fallback";
  affinity.push_back(nearest_inventory_flag);
  // just do it twice for now
  affinity.push_back(nearest_inventory_flag);
*/

  // find out what kind of building we are getting affinity for
  Building::Type request_type = Building::TypeNone;
  if (optional_affinity_building_type != Building::TypeNone){
    request_type = optional_affinity_building_type;
    AILogDebug["util_get_affinity"] << "using specified optional_affinity_building_type " << NameBuilding[request_type];
  }else{
    if (!map->has_flag(flag_pos)){
      AILogDebug["util_get_affinity"] << "no flag at flag_pos " << flag_pos << " and no optional_affinity building specified, returning empty vector";
      return affinity;
    }
    if (!map->has_building(map->move_up_left(flag_pos))){
      AILogDebug["util_get_affinity"] << "no building attached to flag_pos " << flag_pos << " and no optional_affinity building specified, returning empty vector";
      return affinity;
    }
    if (game->get_flag_at_pos(flag_pos) != nullptr ){
      if (game->get_flag_at_pos(flag_pos)->get_building() != nullptr){
        request_type = game->get_flag_at_pos(flag_pos)->get_building()->get_type();
      }
    }
    AILogDebug["util_get_affinity"] << "flag at flag_pos " << flag_pos << " is attached to a building of type " << NameBuilding[request_type];
  }
  if (request_type == Building::TypeNone){
    AILogDebug["util_get_affinity"] << "request_type is TypeNone, maybe flag or building removed?";
  }

  // up to two affinity types for a given request type
  for (int i = 0; i < 2; i++){
    Building::Type affinity_type = BuildingAffinity[request_type][i];
    if (affinity_type == Building::TypeNone){
      AILogDebug["util_get_affinity"] << "requested building of type " << NameBuilding[request_type] << " has no affinity type #" << i << ", returning affinity list so far";
      return affinity;
    }
    AILogDebug["util_get_affinity"] << "looking for nearest connected building of affinity type#" << i << " " << NameBuilding[affinity_type];
    Building *building = AI::find_nearest_building(flag_pos, CompletionLevel::Connected, affinity_type);
    if (building == nullptr) {
      AILogDebug["util_get_affinity"] << "could not find any connected affinity building of type#" << i << " " << NameBuilding[affinity_type] << " for this building of request_type " << NameBuilding[request_type] << ", returning affinity list so far";
      return affinity;
    }
    MapPos found_pos = building->get_position();
    AILogDebug["util_get_affinity"] << "closest connected affinity building of affinity type#" << i << " " << NameBuilding[affinity_type] << " found at found_pos " << found_pos << ", adding its flag to affinity list";
    if (game->get_flag(building->get_flag_index()) == nullptr){
      AILogDebug["util_get_affinity"] << "got nullptr for building flag pos!";
    }else{
      MapPos building_flag_pos = game->get_flag(building->get_flag_index())->get_position();
      AILogDebug["util_get_affinity"] << "adding found building's flag_pos " << building_flag_pos << " to affinity list";
      affinity.push_back(building_flag_pos);
    }
  }

  AILogDebug["util_get_affinity"] << "found buildings for two affinity types, returning the flag_pos of both";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_get_affinity"] << "done util_get_affinity call took " << duration;
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
  AILogDebug["util_find_nearest_building"] << "inside find_nearest_building of type " << NameBuilding[building_type] << ", completion_level " << NameCompletionLevel[level] << ", max_dist " << (signed)max_dist << ", and pos " << pos;
  unsigned int shortest_dist = bad_score;
  Building *closest_building = nullptr;
  Game::ListBuildings buildings = game->get_player_buildings(player);
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
        (level == Connected && game->get_flag_at_pos(building_flag_pos) != nullptr) ||
        (level >= Completed && building->is_done())){
      AILogDebug["util_find_nearest_building"] << "SO FAR, the closest " << NameCompletionLevel[level] << " building of type " << NameBuilding[building_type] << " within max_dist " << max_dist << " to center pos " << pos << " found at " << building_flag_pos;

      // ensure the building_flag_pos is closest-by-flag-dist to the currently selected Inventory (castle/warehouse)
      //  or else it will try to connect to buildings that aren't part of the same "economy" and fail to 
      //  connect a road because of the same check done when placing roads
      if (building_type == Building::TypeStock){
        AILogDebug["util_find_nearest_building"] << "not performing flagsearch because this is a Stock";
      }else{
        AILogDebug["util_find_nearest_building"] << "performing flagsearch to find nearest inventory to this building of type " << NameBuilding[building_type] << " found at " << building_flag_pos;
        // this needs to be a FLAG ONLY search 
        //int nearest_inventory = find_nearest_inventory(map, player_index, building_flag_pos, DistType::FlagAndStraightLine, &ai_mark_pos);
        int nearest_inventory = find_nearest_inventory(map, player_index, building_flag_pos, DistType::FlagOnly, &ai_mark_pos);
        if (nearest_inventory < 0){
          AILogDebug["util_find_nearest_building"] << "inventory not found for flag at building_flag_pos " << building_flag_pos << ", maybe this flag isn't part of the road system??";
          continue;
        }
        if (nearest_inventory != inventory_pos){
          AILogDebug["util_find_nearest_building"] << "flag at building_flag_pos " << building_flag_pos << " is not closest to current inventory_pos " << inventory_pos << ", skipping";
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
    AILogDebug["util_find_nearest_building"] << "closest " << NameCompletionLevel[level] << " building of type " << NameBuilding[building_type] << " within max_dist " << (signed)max_dist << " to center pos " << pos << " found at " << closest_building->get_position();
  }else{
    AILogDebug["util_find_nearest_building"] << "did not find any building of type " << NameBuilding[building_type] << " with CompletionLevel " << NameCompletionLevel[level] << " within max_dist " << (signed)max_dist << " and nearest to inventory_pos " << inventory_pos << " in this player's realm!";
  }
  return closest_building;
}

/*
// return Building* for first building of the specified type exists within specified distance from pos
Building*
AI::find_nearest_building(MapPos center_pos, unsigned int distance, Building::Type building_type) {
  AILogDebug["util_find_nearest_building"] << "inside find_nearest_building " << NameBuilding[building_type] << ", distance " << distance << ", and target pos " << center_pos;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    if (!map->has_building(pos))
      continue;
    if (game->get_building_at_pos(pos)->is_burning())
      continue;
    if (game->get_building_at_pos(pos)->get_type() == building_type) {
      AILogDebug["util_find_nearest_building"] << "found a building of type " << NameBuilding[building_type] << " at pos " << pos;
      return game->get_building_at_pos(pos);
    }
  }

  AILogDebug["util_find_nearest_building"] << "no nearby building found of type " << NameBuilding[building_type] << ", returning nullptr";
  return nullptr;
}

// return Building* for first building of the specified type exists within specified distance from pos
//   THAT HAS AT LEAST ONE PATH.  This building does not have to be completed
Building*
AI::find_nearest_connected_building(MapPos center_pos, unsigned int distance, Building::Type building_type) {
  AILogDebug["find_nearest_connected_building"] << "inside find_nearest_connected_building " << NameBuilding[building_type] << ", distance " << distance << ", and target pos " << center_pos;
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
      AILogDebug["find_nearest_connected_building"] << "found a connected building of type " << NameBuilding[building_type] << " at pos " << pos;
      return game->get_building_at_pos(pos);
    }
  }

  AILogDebug["util_find_nearest_building"] << "no nearby connected building found of type " << NameBuilding[building_type] << ", returning nullptr";
  return nullptr;
}

// return Building* for first building of the specified type exists within specified distance from pos
//   THAT IS FULLY BUILT
Building*
AI::find_nearest_completed_building(MapPos center_pos, unsigned int distance, Building::Type building_type) {
  AILogDebug["util_find_nearest_completed_building"] << "inside find_nearest_completed_building " << NameBuilding[building_type] << ", distance " << distance << ", and target pos " << center_pos;
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
    AILogDebug["util_find_nearest_completed_building"] << "found a completed building of type " << NameBuilding[building_type] << " at pos " << pos;
    return game->get_building_at_pos(pos);
  }

  AILogDebug["util_find_nearest_completed_building"] << "no nearby completed building found of type " << NameBuilding[building_type] << ", returning nullptr";
  return nullptr;
}
*/

// return true if any building of the specified type exists within specified spiral 
//  distance from pos only considers this player's buildings
//  IS THIS DEPRECATED NOW???
bool
AI::building_exists_near_pos(MapPos center_pos, unsigned int distance, Building::Type building_type) {
  AILogDebug["util_building_exists_near_pos"] << "inside AI::building_exists_near_pos with type " << NameBuilding[building_type] << ", distance " << distance << ", and target pos " << center_pos;
  for (unsigned int i = 0; i < distance; i++) {
    //AILogDebug["util_building_exists_near_pos"] << "inside AI::building_exists_near_pos debug2, i=" << i;
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    //AILogDebug["util_building_exists_near_pos"] << "inside AI::building_exists_near_pos debug3";
    if (!map->has_building(pos) || map->get_owner(pos) != player_index)
      continue;
    if (game->get_building_at_pos(pos)->is_burning())
      continue;
    if (game->get_building_at_pos(pos)->get_type() == building_type) {
      AILogDebug["util_building_exists_near_pos"] << "found a building of type " << NameBuilding[building_type] << " at pos " << pos;
      return true;
    }
  }

  AILogDebug["util_building_exists_near_pos"] << "no building found of type " << NameBuilding[building_type] << " near center_pos " << center_pos << ", returning false";
  return false;
}

/* I still don't think this is useful
// find the "best" of two building types, in preference order: occupied->completed->any
//   and find the halfway point between those two and return it
// for trying to build between two buildings, such as building a SteelSmelter halfway between CoalMine and IronMine
//   currently this is ONLY used for placing a steelsmelter between coal and iron mines
MapPos
AI::find_halfway_pos_between_buildings(MapPos inventory_pos, Building::Type first, Building::Type second) {
  AILogDebug["util_find_halfway_pos_between_buildings"] << "inside get_halfway_pos_between_buildings, inventory_pos " << inventory_pos << ", type1 " << NameBuilding[first] << " type2 " << NameBuilding[second] << " for inventory_pos " << inventory_pos;
  //update_buildings();
  Building::Type type[2] = { first, second };
  MapPos found_pos[2] = { bad_map_pos, bad_map_pos };
  for (int x = 0; x < 2; x++) {
    AILogDebug["util_find_halfway_pos_between_buildings"] << "searching this stock area for a building of type" << x << " " << NameBuilding[type[x]];
    if (stock_building_counts.at(inventory_pos).occupied_count[type[x]] >= 1) {
      AILogDebug["util_find_halfway_pos_between_buildings"] << "searching for an OCCUPIED building of type" << x << " " << NameBuilding[type[x]];
      for (MapPos center_pos : stock_building_counts.at(inventory_pos).occupied_military_pos) {
        for (unsigned int i = 0; i < spiral_dist(23); i++) {
          MapPos pos = map->pos_add_extended_spirally(center_pos, i);
          if (!map->has_building(pos))
            continue;
          if (game->get_building_at_pos(pos)->is_burning())
            continue;
          AILogDebug["util_find_halfway_pos_between_buildings"] << "found a building at pos " << pos;
          Building *building = game->get_building_at_pos(pos);
          AILogDebug["util_find_halfway_pos_between_buildings"] << "building at pos " << pos << " is of type " << NameBuilding[building->get_type()];
          if (building->get_type() != type[x]) {
            AILogDebug["util_find_halfway_pos_between_buildings"] << "building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is not the right type, continuing";
            continue;
          }
          AILogDebug["util_find_halfway_pos_between_buildings"] << "building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is the right type, checking its status";
          if (!building->is_done())
            continue;
          if (!building->has_serf())
            continue;
          if (find_nearest_inventory(map, player_index, building->get_position(), DistType::FlagOnly, &ai_mark_pos) != inventory_pos)
            continue;
          found_pos[x] = building->get_position();
          AILogDebug["util_find_halfway_pos_between_buildings"] << "found an acceptable OCCUPIED building of type" << x << " " << NameBuilding[type[x]] << " at pos " << found_pos[x];
          break;
        }

      }
    }
    else if (realm_completed_building_count[type[x]] >= 1) {
      AILogDebug["util_find_halfway_pos_between_buildings"] << "searching for a COMPLETED building of type" << x << " " << NameBuilding[type[x]];
      for (MapPos center_pos : stock_building_counts.at(inventory_pos).occupied_military_pos) {
        for (unsigned int i = 0; i < spiral_dist(0); i++) {
          MapPos pos = map->pos_add_extended_spirally(center_pos, i);
          if (!map->has_building(pos))
            continue;
          if (game->get_building_at_pos(pos)->is_burning())
            continue;
          AILogDebug["util_find_halfway_pos_between_buildings"] << "found a building at pos " << pos;
          Building *building = game->get_building_at_pos(pos);
          AILogDebug["util_find_halfway_pos_between_buildings"] << "building at pos " << pos << " is of type " << NameBuilding[building->get_type()];
          if (building->get_type() != type[x]) {
            AILogDebug["util_find_halfway_pos_between_buildings"] << "building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is not the right type, continuing";
            continue;
          }
          AILogDebug["util_find_halfway_pos_between_buildings"] << "building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is the right type, checking its status";
          if (!building->is_done())
            continue;
          if (find_nearest_inventory(map, player_index, building->get_position(), DistType::FlagOnly, &ai_mark_pos) != inventory_pos)
            continue;
          found_pos[x] = building->get_position();
          AILogDebug["util_find_halfway_pos_between_buildings"] << "found an acceptable COMPLETED building of type" << x << " " << NameBuilding[type[x]] << " at pos " << found_pos[x];
          break;
        }
      }
    }
    else if (realm_building_count[type[x]] >= 1) {
      AILogDebug["util_find_halfway_pos_between_buildings"] << "searching for ANY building of type" << x << " " << NameBuilding[type[x]];
      for (MapPos center_pos : stock_building_counts.at(inventory_pos).occupied_military_pos) {
        AILogDebug["util_find_halfway_pos_between_buildings"] << "searching around center_pos " << center_pos;
        for (unsigned int i = 0; i < spiral_dist(0); i++) {
          MapPos pos = map->pos_add_extended_spirally(center_pos, i);
          if (!map->has_building(pos))
            continue;
          if (game->get_building_at_pos(pos)->is_burning())
            continue;
          AILogDebug["util_find_halfway_pos_between_buildings"] << "found a building at pos " << pos;
          Building *building = game->get_building_at_pos(pos);
          AILogDebug["util_find_halfway_pos_between_buildings"] << "building at pos " << pos << " is of type " << NameBuilding[building->get_type()];
          if (building->get_type() != type[x]) {
            AILogDebug["util_find_halfway_pos_between_buildings"] << "building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is not the right type, continuing";
            continue;
          }
          AILogDebug["util_find_halfway_pos_between_buildings"] << "building at pos " << pos << " of type " << NameBuilding[building->get_type()] << " is the right type, checking its status";
          //if (!building->has_serf())  // why was this check here before?
          //  continue;
          if (find_nearest_inventory(map, player_index, building->get_position(), DistType::FlagOnly, &ai_mark_pos) != inventory_pos)
            continue;
          found_pos[x] = building->get_position();
          AILogDebug["util_find_halfway_pos_between_buildings"] << "found an acceptable ANY building of type" << x << " " << NameBuilding[type[x]] << " at pos " << found_pos[x];
          break;
        }

      }
    }
    else {
      AILogDebug["util_find_halfway_pos_between_buildings"] << "no buildings of type" << x << " " << NameBuilding[type[x]] << " known in realm, returning bad_map_pos";
      return bad_map_pos;
    }
    // this shouldn't happen if update_buildings() is accurate unless building was destroyed while searching
    if (found_pos[x] == bad_map_pos) {
      AILogDebug["util_find_halfway_pos_between_buildings"] << "could not find expected building of type" << x << " " << NameBuilding[type[x]] << " in realm despite being known! returning bad_map_pos";
      return bad_map_pos;
    }
    AILogDebug["util_find_halfway_pos_between_buildings"] << "found best building of type" << x << " " << NameBuilding[type[x]] << " at pos " << found_pos[x];
  }
  AILogDebug["util_find_halfway_pos_between_buildings"] << "found both buildings, " << NameBuilding[type[0]] << " at " << found_pos[0] << ", and " << NameBuilding[type[1]] << " at " << found_pos[1];
  AILogDebug["util_find_halfway_pos_between_buildings"] << "finding halfway point";
  MapPos halfway_pos = get_halfway_pos(found_pos[0], found_pos[1]);
  AILogDebug["util_find_halfway_pos_between_buildings"] << "done get_halfway_pos_between_buildings, returning halfway_pos " << halfway_pos;
  return halfway_pos;
}
*/


//
// build a Road object by following the paths from start_pos in specified direction
//    until another flag is found.  The start pos doesn't have to be a real flag
Road
AI::trace_existing_road(PMap map, MapPos start_pos, Direction dir) {
  //AILogDebug["util_trace_existing_road"] << "inside trace_existing_road, start_pos " << start_pos << ", dir: " << NameDirection[dir];
  Road road;
  if (!map->has_path_IMPROVED(start_pos, dir)) {
    AILogWarn["util_trace_existing_road"] << "no path found at " << start_pos << " in direction " << NameDirection[dir] << "!  FIND OUT WHY";
    //ai_mark_pos.insert(ColorDot(start_pos, "white"));
    //sleep_speed_adjusted(100000);
    return road;
  }
  road.start(start_pos);
  //ai_mark_road.start(start_pos);
  MapPos pos = start_pos;
  while (true) {
    road.extend(dir);
    //ai_mark_road->extend(dir);
    pos = map->move(pos, dir);
    //sleep_speed_adjusted(0);
    if (map->has_flag(pos) && pos != start_pos) {
      //AILogDebug["util_trace_existing_road"] << "flag found at pos " << pos << ", returning road (which has length " << road.get_length() << ")";
      //ai_mark_road->invalidate();
      return road;
    }
    for (Direction new_dir : cycle_directions_cw()) {
      //AILogDebug["util_trace_existing_road"] << "looking for path from pos " << pos << " in dir " << NameDirection[new_dir];
      if (map->has_path_IMPROVED(pos, new_dir) && new_dir != reverse_direction(dir)) {
        //AILogDebug["util_trace_existing_road"] << "found path from pos " << pos << " in dir " << NameDirection[new_dir];
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
  //AILogDebug["util_get_corners"] << "inside AI::get_corners(pos)";
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
  //AILogDebug["util_get_corners"] << "inside AI::get_corners(pos, dist)";
  MapPosVector positions;
  if (distance > 24) {
    AILogError["util_get_corners2"] << "get_corners only supports up to distance 24!  this is straight line not spirally distance.  Capping it at 24";
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
  //AILogDebug["util_get_dir_from_corner"] << "inside AI::get_dir_from_corner(pos) with center_pos " << center_pos << " and corner_pos " << corner_pos;
  for (Direction dir : cycle_directions_cw()) {
    MapPos pos = center_pos;
    for (int x = 0; x < 5; x++) {
      pos = map->move(pos, dir);
    }
    if (pos == corner_pos){
      //AILogDebug["util_get_dir_from_corner"] << "found direction from center_pos " << center_pos << " to corner_pos " << corner_pos << ": " << NameDirection[dir] << " / " << dir;
      return dir;
    }
  }
  // if we reached this point it wasn't found, throw an exception as this should never happen
  //  WRONG - this happens all the time, see ISSUE above, it is okay
  //AILogError["util_get_dir_from_corner"] << "failed to find the Direction of the corner_pos " << corner_pos << " associated with center_pos " << center_pos;
  //ai_mark_pos.insert(ColorDot(center_pos, "green"));
  //ai_mark_pos.insert(ColorDot(corner_pos, "red"));
  //ai_status.assign("LOOK AT green RED POS ERROR");
  //sleep_speed_adjusted(300000);
  //throw ExceptionFreeserf("util_get_dir_from_corner failed to find the Direction from center_pos to corner_pos!  This should never happen");
  return DirectionNone;
}


// return count of terrain tiles of the specified type range, like hills (Tundra)
unsigned int
AI::count_terrain_near_pos(MapPos center_pos, unsigned int distance, Map::Terrain res_start_index, Map::Terrain res_end_index, std::string color) {
  //AILogDebug["util_count_terrain_near_pos"] << "inside count_terrain_near_pos";
  //AILogDebug["util_count_terrain_near_pos"] << "AI: center_pos " << center_pos << ", distance " << distance << ", res_start_index " << NameTerrain[res_start_index] << ", res_end_index " << NameTerrain[res_end_index];
  unsigned int count = 0;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    //AILogDebug["util_count_terrain_near_pos"] << "AI: terrain at pos " << pos << " has type " << terrain;
    if (AI::has_terrain_type(game, pos, res_start_index, res_end_index)) {
      //AILogDebug["util_count_terrain_near_pos"] << "AI: found matching terrain at pos " << pos;
      ++count;
    }
  }
  //AILogDebug["util_count_terrain_near_pos"] << "AI: found count " << count << " matching terrain of types " << NameTerrain[res_start_index] << " - " << NameTerrain[res_end_index];
  return count;
}

// return count of terrain tiles of the specified type range, like hills (Tundra)...
//   ... that have NO hard objects on them (flags, buildings, stones, etc.).  Paths are acceptable
//  initially only used for hill/sign ratios
// this function is almost identical to count_farmable_land except it doesn't include existing wheat fields
//  maybe merge them?  Or make this one geologist/hill-specific
unsigned int
AI::count_empty_terrain_near_pos(MapPos center_pos, unsigned int distance, Map::Terrain res_start_index, Map::Terrain res_end_index, std::string color) {
  //AILogDebug["util_count_empty_terrain_near_pos"] << "inside count_empty_terrain_near_pos";
  //AILogDebug["util_count_empty_terrain_near_pos"] << "AI: inside AI::count_empty_terrain_near_pos";
  //AILogDebug["util_count_empty_terrain_near_pos"] << "AI: center_pos " << center_pos << ", distance " << distance << ", res_start_index " << NameTerrain[res_start_index] << ", res_end_index " << NameTerrain[res_end_index];
  unsigned int count = 0;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    //AILogDebug["util_count_empty_terrain_near_pos"] << "AI: terrain at pos " << pos << " has type " << terrain;
    if (AI::has_terrain_type(game, pos, res_start_index, res_end_index)) {
      Map::Object obj_type = map->get_obj(pos);
      // exclude tiles with blocking objects (anything not on this list)
      if (obj_type == Map::ObjectNone
        || (obj_type >= Map::ObjectFieldExpired && obj_type <= Map::ObjectSignSmallStone)) {
        //AILogDebug["util_count_empty_terrain_near_pos"] << "AI: found matching empty terrain at pos " << pos;
        ++count;
      }
    }
  }
  //AILogDebug["util_count_empty_terrain_near_pos"] << "AI: found count " << count << " matching empty terrain of types " << NameTerrain[res_start_index] << " - " << NameTerrain[res_end_index];
  return count;
}

// count farmable land  - count the number of grass tiles, with no paths,
//     and no obstacles (other than existing wheat fields)
// UPDATE dec 2021, changing this from "has grass" to "doesn't have water/desert/tundra/snow"
//  because I keep seeing farms built in crappy spots around mountains despite a really high
//  min_openspace_farm setting
//    this works way better!

unsigned int
AI::count_farmable_land(MapPos center_pos, unsigned int distance, std::string color) {
  /*
  typedef enum {
    TerrainWater0 = 0,
    TerrainWater1,
    TerrainWater2,
    TerrainWater3,
    TerrainGrass0,  // 4
    TerrainGrass1,
    TerrainGrass2,
    TerrainGrass3,
    TerrainDesert0,  // 8
    TerrainDesert1,
    TerrainDesert2,
    TerrainTundra0,  // 11
    TerrainTundra1,
    TerrainTundra2,
    TerrainSnow0,  // 14
    TerrainSnow1
  } Terrain;
  */

  AILogDebug["util_count_farmable_land"] << "inside AI::count_farmable_land";
  //AILogDebug["util_count_farmable_land"] << "center_pos " << center_pos << ", distance " << distance << ", res_start_index " << NameTerrain[res_start_index] << ", res_end_index " << NameTerrain[res_end_index];
  unsigned int count = 0;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    //AILogDebug["util_count_farmable_land"] << "AI: terrain at pos " << pos << " has type " << terrain;
    if (AI::has_terrain_type(game, pos, Map::TerrainWater0, Map::TerrainWater3)
            || AI::has_terrain_type(game, pos, Map::TerrainDesert0, Map::TerrainSnow1)) {
      // this tile touches non-Grass, exclude it
      continue;
    }else{
      // this tile touches ONLY GRASS
      Map::Object obj_type = map->get_obj(pos);
      // allow tiles that have existing fields or felled trees
      if (obj_type == Map::ObjectNone
        || (obj_type >= Map::ObjectSeeds0 && obj_type <= Map::ObjectFieldExpired)
        || (obj_type >= Map::ObjectField0 && obj_type <= Map::ObjectField5)
        || (obj_type >= Map::ObjectFelledPine0 && obj_type <= Map::ObjectFelledTree4) ) {
      }
      else {
        // exclude tiles with blocking objects (anything not on this list)
        continue;
      }
      // exclude tiles with paths
      if (map->has_any_path(pos)) {
        continue;
      }

      ++count;
    }
  }
  //AILogDebug["util_count_farmable_land"] << "AI: found count " << count << " matching terrain of types " << NameTerrain[res_start_index] << " - " << NameTerrain[res_end_index];
  AILogDebug["util_count_farmable_land"] << "done count_farmable_land, returning count " << count;
  return count;
}

// count the number of grass tiles, with no paths, and no obstacles
// this is identical to count_farmable_lands but it doesn't include existing fields
//  intended for placing buildings that are not location specific such as blacksmiths
// NOTE - updating this to specifically avoid Farms because they will quickly
//  consume space even if they have not yet.  Seeing too many blacksmiths placed near farms
unsigned int
AI::count_open_space(PGame game, MapPos center_pos, unsigned int distance, std::string color) {
  AILogDebug["count_open_space"] << "inside AI::count_open_space";
  unsigned int count = 0;
  bool farm_penalty = false;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    //AILogDebug["count_open_space"] << "AI: terrain at pos " << pos << " has type " << terrain;
    if (map->has_building(pos) && game->get_building_at_pos(pos)->get_type() == Building::TypeFarm){
      farm_penalty = true;
    }
    if (AI::has_terrain_type(game, pos, Map::TerrainWater0, Map::TerrainWater3)
            || AI::has_terrain_type(game, pos, Map::TerrainDesert0, Map::TerrainSnow1)) {
      // this tile touches non-Grass, exclude it
      continue;
    }else{
      // this tile touches ONLY GRASS
      Map::Object obj_type = map->get_obj(pos);
      // allow tiles that have felled trees
      if (obj_type == Map::ObjectNone
        || (obj_type >= Map::ObjectFelledPine0 && obj_type <= Map::ObjectFelledTree4) ) {
      }
      else {
        // exclude tiles with blocking objects (anything not on this list)
        continue;
      }
      // exclude tiles with paths
      if (map->has_any_path(pos))
        continue;
      ++count;
    }
  }
  if (farm_penalty){
    AILogDebug["count_open_space"] << "inside count_open_space, applying farm penalty for this area";
    if (count < 25){ count = 0; }else{ count -= 25; }
  }
  AILogDebug["count_open_space"] << "done count_open_space, returning count " << count;
  return count;
}


// sort a MapPosSet by value, ascending, and return as sorted vector of the keys (throwing the values away)
MapPosVector
AI::sort_by_val_asc(MapPosSet set) {
  //AILogDebug["util_sort_by_val_asc"] << "inside AI::sort_by_val_asc";
  // this is C++14 only?  worked on visual studio 2017 on windows but not VC code w/gcc on linux
  auto cmp = [](const auto &p1, const auto &p2) { return p2.second > p1.second || !(p1.second > p2.second) && p1.first > p2.first; };
  std::set< std::pair<MapPos, unsigned int>, decltype(cmp)> sorted_set(set.begin(), set.end(), cmp);
  MapPosVector sorted_vector;
  for (auto pair : sorted_set) {
    //AILogDebug["util_sort_by_val_asc"] << "sorted MapPosSet pos: " << pair.first << ", val: " << pair.second;
    sorted_vector.push_back(pair.first);
  }
  return sorted_vector;
}

// sort a MapPosSet by value, descending, and return as sorted vector of the keys (throwing the values away)
MapPosVector
AI::sort_by_val_desc(MapPosSet set) {
  //AILogDebug["util_sort_by_val_desc"] << "inside AI::sort_by_val_desc";
  auto cmp = [](const auto &p1, const auto &p2) { return p2.second < p1.second || !(p1.second < p2.second) && p1.first < p2.first; };
  std::set< std::pair<MapPos, unsigned int>, decltype(cmp)> sorted_set(set.begin(), set.end(), cmp);
  MapPosVector sorted_vector;
  for (auto pair : sorted_set) {
    //AILogDebug["util_sort_by_val_desc"] << "sorted MapPosSet pos: " << pair.first << ", val: " << pair.second;
    sorted_vector.push_back(pair.first);
  }
  return sorted_vector;
}

// sort a MapPosVector<int,unsigned int> of proad_index:scores by value, ascending, and return as sorted vector of the keys (throwing the values away)
std::vector<unsigned int>
AI::sort_scores_by_val_asc(std::vector<std::pair<int, unsigned int>> scores) {
  //AILogDebug["util_sort_by_val_asc"] << "inside AI::sort_scores_by_val_asc";
  // this is C++14 only?  worked on visual studio 2017 on windows but not VC code w/gcc on linux
  auto cmp = [](const auto &p1, const auto &p2) { return p2.second > p1.second || !(p1.second > p2.second) && p1.first > p2.first; };
  //std::vector< std::pair<int, unsigned int>, decltype(cmp)> sorted_scores(scores.begin(), scores.end(), cmp);
  std::sort(scores.begin(), scores.end(), cmp);
  std::vector<unsigned int> sorted_keys;
  //for (auto pair : sorted_scores) {
  for (std::pair<int, unsigned int> pair : scores){
    AILogDebug["util_sort_by_val_asc"] << "sorted scores list contains index: " << pair.first << ", score: " << pair.second;
    sorted_keys.push_back(pair.first);
  }
  return sorted_keys;
}

/* desc might not even be used, leaving it out until needed
// sort a MapPosVector<int,unsigned int> of proad_index:scores by value, descending, and return as sorted vector of the keys (throwing the values away)
std::vector<unsigned int>
AI::sort_scores_by_val_desc(std::vector<std::pair<int, unsigned int>> scores) {
// copy from sort_scores_by_val_asc if needed
*/


// return count of individual objects of the specified type range, such as trees or geologist signs
unsigned int
AI::count_objects_near_pos(MapPos center_pos, unsigned int distance, Map::Object res_start_index, Map::Object res_end_index, std::string color) {
  //AILogDebug["util_count_objects_near_pos"] << "inside AI::count_objects_near_pos";
  //AILogDebug["util_count_objects_near_pos"] << "AI: center_pos " << center_pos << ", distance " << distance << ", res_start_index " << res_start_index << "(" << NameObject[res_start_index] << ")"
  //      << ", res_end_index " << res_end_index << "(" << NameObject[res_end_index] << ")";
  unsigned int count = 0;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    if (map->get_obj(pos) >= res_start_index && map->get_obj(pos) <= res_end_index) {
      ++count;
      //AILogDebug["util_count_objects_near_pos"] << "AI: found matching object at pos " << pos << ", type " << map->get_obj(pos);
    }
    //sleep_speed_adjusted(0);
  }
  //AILogDebug["util_count_objects_near_pos"] << "AI: found count " << count << " matching objects of types " << NameObject[res_start_index] << " - " << NameObject[res_end_index];
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

  AILogDebug["util_build_near_pos"] << "inside AI::build_near_pos with building type index " << NameBuilding[building_type] << ", distance " << distance << ", target pos " << center_pos << ", inventory_pos " << inventory_pos;

  /* moving this logic outside this function

  // this function is being changed from using the *nearest* Inventory to using the *current*
  //  inventory, does that introduce any problems?
  if (building_type == Building::TypeHut) {
    if (stock_building_counts.at(inventory_pos).unfinished_hut_count >= max_unfinished_huts) {
      AILogDebug["util_build_near_pos"] << "max unfinished huts limit " << max_unfinished_huts << " reached, not building";
      // should be returning not_built_pos or bad_map pos?? stopbuilding is less appropriate with separate counts set up for huts vs other buildings
      return stopbuilding_pos;
    }
  }

  // always be willing to build wood/stone buildings and knight huts or everything can grind to a halt
  //   also Mines, but don't connect these to the road network yet
  if (stock_building_counts.at(inventory_pos).unfinished_count >= max_unfinished_buildings && building_type != Building::TypeSawmill
  //if (unfinished_building_count >= max_unfinished_buildings && building_type != Building::TypeSawmill
    && building_type != Building::TypeLumberjack && building_type != Building::TypeStonecutter
    && building_type != Building::TypeHut && building_type != Building::TypeCoalMine
    && building_type != Building::TypeIronMine && building_type != Building::TypeGoldMine
    && building_type != Building::TypeStoneMine && building_type != Building::TypeForester
    ) {
    AILogDebug["util_build_near_pos"] << "max unfinished buildings limit " << max_unfinished_buildings << " reached, not building";
    return stopbuilding_pos;
  }
  */

  // note if enemy borders are near
  bool enemy_near = false;
  for (unsigned int i = 0; i < AI::spiral_dist(8); i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    if (map->get_owner(pos) != player_index && map->has_owner(pos)) {
      AILogDebug["util_build_near_pos"] << "enemy territory found near this center_pos";
      enemy_near = true;
      break;
    }
  }

  // don't build large civilian buildings, including stocks, near enemy borders
  if (enemy_near && 
      building_type >= Building::TypeStock && building_type != Building::TypeHut &&
      building_type != Building::TypeTower && building_type != Building::TypeForester){
    AILogDebug["util_build_near_pos"] << "enemy borders near this potential build_near_pos center and this building of type " << NameBuilding[building_type] << " is a Large Civilian building!  Not building in this area, returning";
    return bad_map_pos;
  }

  // if this is an economy building (except Fisher which isn't counted), make sure the actual building position 
  //  is closest to the currently "selected" stock so that this building becomes "attached" to the stock.
  //  Otherwise, the stock doesn't see this building as part of its range and will keep building more
  bool verify_stock = false;
  if (building_type != Building::TypeFisher && building_type != Building::TypeStock && 
      building_type != Building::TypeHut && building_type != Building::TypeTower && 
      building_type != Building::TypeFortress && building_type != Building::TypeCastle &&
      building_type != Building::TypeForester){
    AILogDebug["util_build_near_pos"] << "this is a tracked economy building of type " << NameBuilding[building_type] << ", setting verify_stock to true";
    verify_stock = true;
  }

  bool is_mine = false;
  // Mines are to be built, but not connected immediately so they can be placed before geologist signs fade,
  //   even if player's economy doesn't yet require the mine.  Used to establish good mine locations early
  if (building_type == Building::TypeCoalMine
    || building_type == Building::TypeIronMine
    || building_type == Building::TypeGoldMine
    || building_type == Building::TypeStoneMine) {
    AILogDebug["util_build_near_pos"] << "this is a Mine of type " << NameBuilding[building_type] << ", setting is_mine to true";
    is_mine = true;
  }

  // if this is a tracked-economy building, but not a mine (because we have less control
  //  over placement of mines), and the current Inventory is a stock (not castle)
  //  then also ensure the straight-line distance is fairly close to the stock
  //  Otherwise, buildings tend to be built far away from it even if they are still closest
  //  by verify_stock checks
  // NOTE - this goes by center_pos not actual pos so it is a bit more sensitive than it seems
  if (!is_mine && verify_stock && inventory_pos != castle_flag_pos){
    AILogDebug["util_build_near_pos"] << "this is a non-mine tracked economy building of type " << NameBuilding[building_type] << " and this is not the castle area, doing straightline-dist-to-Stock check";
    if (get_straightline_tile_dist(map, center_pos, inventory_pos) > 25){
      AILogDebug["util_build_near_pos"] << "this is a non-mine tracked economy building of type " << NameBuilding[building_type] << " and this is not the castle area, rejecting center_pos because too far from Stock";
      return bad_map_pos;
    }
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
        AILogWarn["build_near_pos"] << "DIRECTIONAL FILL ONLY WORKS FOR ~SPIRAL-4 DIST! " << DIRECTIONAL_FILL_POS_MAX << " POSITIONS, limit reached, quitting here";
        return bad_map_pos;
      }
      pos = map->pos_add_directional_fill(center_pos, i, optional_fill_dir);
    }else{
      pos = map->pos_add_extended_spirally(center_pos, i);
    }
    if (map->get_owner(pos) != player_index) {
      continue;
    }

    if (game->can_build_building(pos, building_type, player)) {
      AILogDebug["util_build_near_pos"] << "inside AI::build_near_pos, can build " << NameBuilding[building_type] << " at pos " << pos;
    }
    else {
      // simply can't build here
      //AILogDebug["util_build_near_pos"] << "cannot build " << NameBuilding[building_type] << " of sizetype " << building_sizes[building_type] << " at pos " << pos;
      continue;
    }

    // avoid known bad positions for this building type.  note this list is NOT persisted if AI thread goes away or game load/restart
    if (is_bad_building_pos(pos, building_type)) {
      AILogDebug["util_build_near_pos"] << "skipping this pos, it is marked in bad_building_pos list";
      continue;
    }

    // avoid building knight huts close together
    //  however, if enemy borders near and knights > 2x min - then a reduced spacing is allowed
    if (building_type == Building::TypeHut){
      int allowed_dist = 6;  // default spacing if no exception
      if (enemy_near){
        unsigned int idle_knights = serfs_idle[Serf::TypeKnight0] + serfs_idle[Serf::TypeKnight1] + serfs_idle[Serf::TypeKnight2] + serfs_idle[Serf::TypeKnight3] + serfs_idle[Serf::TypeKnight4];
        if(idle_knights >= knights_min * 2) {
          AILogDebug["util_build_near_pos"] << "enemy territory near and idle knights > 2x min, allowing reduced spacing for knight_hut near pos " << pos;
          allowed_dist = 4;  // reduced spacing
        }
      }
      if (AI::building_exists_near_pos(pos, AI::spiral_dist(allowed_dist), Building::TypeHut)) {
        AILogDebug["util_build_near_pos"] << "another knight hut already exists within allowed_dist " << allowed_dist << " of pos " << pos << ", skipping this pos";
        continue;
      }
    }
    
    // if this is a wheat Farm, don't build it too close to the castle
    if (building_type == Building::TypeFarm && get_straightline_tile_dist(map, pos, castle_pos) <= 8){  
      AILogDebug["util_build_near_pos"] << "this is a wheat farm, but center_pos " << pos << " is too close to the castle, skipping this area";
      continue;
    }

    // don't build two of various buildings too close to each other, they become redundant
    if (building_type == Building::TypeMill || building_type == Building::TypeBaker ||
        building_type == Building::TypeSawmill || building_type == Building::TypeButcher){
      if (AI::building_exists_near_pos(pos, AI::spiral_dist(10), building_type)) {
        AILogDebug["util_build_near_pos"] << "there is already another building of same type " << NameBuilding[building_type] << " within 10 tiles, not building here";
        continue;
      }
    }


    //
    // FIRST, try to place a FLAG that results in a good road connection
    //
    AILogDebug["util_build_near_pos"] << "trying to place a flag that can connect this building";
    MapPos flag_pos = map->move_down_right(pos);

    if (building_type == Building::TypeStock){
      // avoid building a warehouse/stock where there are any existing roads or flags adjacent to the potential new stock's flag pos
      //  which makes it harder to connect the new stock to other nearby flags, as two flags cannot be adjacent to each other'
      // oh wait, I forgot trees and stones and various other things block flags, instead... just check can_build_flag
      AILogDebug["util_build_near_pos"] << "this is a warehouse/stock, checking for adjacent flags or paths";
      bool skip = false;
      for (Direction dir : cycle_directions_cw()) {
        MapPos adjacent_pos = map->move(flag_pos, dir);
        if (!game->can_build_flag(adjacent_pos, player)){
          AILogDebug["util_build_near_pos"] << "this is a warehouse/stock, but can_build_flag test against adjacent_pos " << adjacent_pos << " to flag_pos " << flag_pos << " fails, skipping this pos";
          break;
          skip = true;
        }
      }
      if (skip){
        AILogDebug["util_build_near_pos"] << "this is a warehouse/stock, but there is at least one obstacle near this pos's potential flag pos, skipping this pos";
        continue;
      }
      // avoid building stock near unclaimed borders, partly to avoid putting at risk of nearby enemies
      //  encroaching, but also to help ensure that knight huts are already established around it so 
      //  that when the "rebuild nearby roads" happens
      // note if enemy borders are near
      for (unsigned int i = 0; i < AI::spiral_dist(10); i++) {
        MapPos pos = map->pos_add_extended_spirally(center_pos, i);
        if (!map->has_owner(pos)) {
          AILogDebug["util_build_near_pos"] << "unclaimed territory found near this center_pos, not building warehouse in this corner";
          return bad_map_pos;
        }
      }
    }

    bool road_built = false;

    if (is_mine){
      AILogDebug["util_build_near_pos"] << "this is a mine, skipping flag/road checks as this will not be connected to the road system yet";
    }else{
      bool built_new_flag = false;
      if (!map->has_flag(flag_pos)){
        AILogDebug["util_build_near_pos"] << "no flag yet exists at flag_pos " << flag_pos << " for potential new building of type " << NameBuilding[building_type] << " at pos " << pos << ", trying to build one";
        mutex_lock("AI::build_near_pos calling game->demolish_flag (build_near_pos verify_stock test)");
        built_new_flag = game->build_flag(flag_pos, player);
        mutex_unlock();
        if (!built_new_flag){
          AILogWarn["util_build_near_pos"] << "failed to build flag at flag_pos " << flag_pos << " for potential new building of type " << NameBuilding[building_type] << " at pos " << pos << "!  skipping this pos";
          continue;
        }
        AILogDebug["util_build_near_pos"] << "successfully built flag at flag_pos " << flag_pos << " for potential new building of type " << NameBuilding[building_type] << " at pos " << pos;
        built_new_flag = true;
      }

      if (game->get_flag_at_pos(flag_pos) != nullptr && !game->get_flag_at_pos(flag_pos)->is_connected()) {
        AILogDebug["util_build_near_pos"] << "the flag already at flag_pos " << flag_pos << " is not yet connected.  For potential new building of type " << NameBuilding[building_type] << " at pos " << pos << ", trying to connect it to road system";
        //
        // build road to connect this potential building
        //
        //  this may include a check for nearest inventory and disqualify the build if
        //  the closest Inventory building by flagsearch is not the current one
        //
        Road notused; // not used here
        road_built = AI::build_best_road(flag_pos, road_options, &notused, "build_near_pos(canPassthru):"+NameBuilding[building_type]+"@"+std::to_string(flag_pos), building_type, Building::TypeNone, bad_map_pos, verify_stock);
        // now that passthru roads are enabled, the older style "go around" basically never happens
        //  so if no road was built with passthru enabled, try again without it to see if going around works
        if (!road_built && road_options.test(RoadOption::AllowPassthru)){
          AILogDebug["util_build_near_pos"] << "failed to build road using Passthru, trying again without it";
          road_options.reset(RoadOption::AllowPassthru);
          road_built = AI::build_best_road(flag_pos, road_options, &notused, "build_near_pos(nonPassthru):"+NameBuilding[building_type]+"@"+std::to_string(flag_pos), building_type, Building::TypeNone, bad_map_pos, verify_stock);
          road_options.set(RoadOption::AllowPassthru);
        }
      }else{
        AILogDebug["util_build_near_pos"] << "the flag already at flag_pos " << flag_pos << " is already connected to road system.  For potential new building of type " << NameBuilding[building_type] << " at pos " << pos;
        // must do the verify_stock check here on this flag, because normally it is done when placing the road but this
        //  flag/road already exists
        if (verify_stock == true){
          // this needs to be FLAG ONLY
          // wait, no why would it be Flag only, FlagAndStraightLine is *more* restrictive, which is generall good
          //if (find_nearest_inventory(map, player_index, flag_pos, DistType::FlagOnly, &ai_mark_pos) != inventory_pos){
          if (find_nearest_inventory(map, player_index, flag_pos, DistType::FlagAndStraightLine, &ai_mark_pos) != inventory_pos){
            AILogDebug["util_build_best_road"] << "verify_stock for existing flag - flag at flag_pos " << flag_pos << " is not closest to current inventory_pos " << inventory_pos << ", skipping";
            // if a new flag was built, remove it
            if (built_new_flag){
              mutex_lock("AI::build_near_pos calling game->demolish_flag (build_near_pos verify_stock test)");
              game->demolish_flag(flag_pos, player);
              mutex_unlock();
            }
            // skip this flag/pos and try the next position
            continue;
          }
        }
      }

      if (!road_built && game->get_flag_at_pos(flag_pos) != nullptr && !game->get_flag_at_pos(flag_pos)->is_connected()) {
        AILogDebug["util_build_near_pos"] << "failed to connect flag at flag_pos " << flag_pos << " to road network! removing it.  For potential new building of type " << NameBuilding[building_type];
        // disabling this for now until I see more significant road building issues
        //  jan19 2021
        //AILogDebug["util_build_near_pos"] << "LOOK AT BUILDING AT POS " << pos << ", MARKED IN CYAN";
        //ai_mark_pos.erase(pos);
        //ai_mark_pos.insert(std::make_pair(pos, "cyan"));
        //sleep_speed_adjusted(5000);
        mutex_lock("AI::build_near_pos calling game->demolish_flag (build_near_pos failed to connect)");
        game->demolish_flag(flag_pos, player);
        mutex_unlock();
        // mark as bad pos, to avoid repeateadly rebuilding same building in same spot
        bad_building_pos.insert(std::make_pair(pos, building_type));
        // try the next position
        continue;
      }else{
        AILogDebug["util_build_near_pos"] << "successfully connected flag at flag_pos " << flag_pos << " to road network.  For potential new building of type " << NameBuilding[building_type];
        //if (building_type == Building::TypeHut) {
        //  stock_building_counts.at(inventory_pos).unfinished_hut_count++;
        //  AILogDebug["util_build_near_pos"] << "incrementing unfinished_hut_count, is now: " << unfinished_hut_count;
        //}
        //else {
        //  stock_building_counts.at(inventory_pos).unfinished_count++;
        //  AILogDebug["util_build_near_pos"] << "found unfinished " << NameBuilding[building_type] << ", incrementing unfinished_building_count, is now: " << unfinished_building_count;
        //}
        //duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
        //AILogDebug["util_build_near_pos"] << "successful util_build_near_pos for building of type " << NameBuilding[building_type] << " call took " << duration;
        //return pos;
      }
    } // if is_mine

    // try to build it
    mutex_lock("AI::build_near_pos calling game->build_building (build_near_pos)");
    bool was_built = game->build_building(pos, building_type, player);
    mutex_unlock();
    if (!was_built) {
      AILogDebug["util_build_near_pos"] << "failed to build building of type " << NameBuilding[building_type] << " despite can_build being true!  WAITING 10sec - look at the pos in coral!";
      //ai_mark_pos.insert(ColorDot(pos, "coral"));
      //sleep_speed_adjusted(10000);
      continue;
    }

    AILogDebug["util_build_near_pos"] << "successfully placed and connected (unless it was a mine) a building of type " << NameBuilding[building_type] << " at pos " << pos;

    duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    AILogDebug["util_build_near_pos"] << "successful util_build_near_pos, built building of type " << NameBuilding[building_type] << " at pos " << pos << ", call took " << duration;

    // sleep a bit to be more human-like
    sleep_speed_adjusted(2000);
    return pos;
  }

  AILogDebug["util_build_near_pos"] << "could not find a place to build, or failed to build, type " << NameBuilding[building_type] << " near pos " << center_pos << " after checking " << distance << " positions";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_build_near_pos"] << "failed util_build_near_pos for building of type " << NameBuilding[building_type] << " call took " << duration;
  return notplaced_pos;

}

// return count of total stones in the area.  A stone pile can have 1-8 stones
// ALSO, have to work around the original-game bug where a stonecutter cannot harvest stones if a building is directly down-right from a stone pile
//   To avoid this, do not count stones that have a building down-right from the pile
unsigned int
AI::count_stones_near_pos(MapPos center_pos, unsigned int distance) {
  AILogDebug["util_count_stones_near_pos"] << "inside count_stones_near_pos";
  Map::Object res_start_index = Map::ObjectStone0;
  Map::Object res_end_index = Map::ObjectStone7;
  std::string color = "gray";
  //AILogDebug["util_count_stones_near_pos"] << "AI: center_pos " << center_pos << ", distance " << distance << ", res_start_index " << NameObject[res_start_index] << ", res_end_index " << NameObject[res_end_index];
  unsigned int total = 0;
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    if (map->get_obj(pos) >= res_start_index && map->get_obj(pos) <= res_end_index) {
      //AILogDebug["util_count_stones_near_pos"] << "getting value of map object at pos: " << pos << " with map obj type " << NameObject[map->get_obj(pos)];
      // because the higher MapObject indexes represent smaller resource values, the difference is negative
      //   so invert the difference and add one, because it starts at zero
      int value = 1 + (-1 * (map->get_obj(pos) - res_end_index));
      //AILogDebug["util_count_stones_near_pos"] << "map object has resource value of " << value << " at pos " << pos;
      // bug work-around - ignore stones that cannot be harvested because a building is down-right from the pile
      if (map->has_building(map->move_down_right(pos))) {
        // this seems to be working as expected
        AILogDebug["util_count_stones_near_pos"] << "stones bug workaround - ignoring stone pile at pos " << pos << " because a building is down-right from it so it cannot be harvested";
        value = 0;
      }
      total += value;
    }
    //sleep_speed_adjusted(0);
  }
  //AILogDebug["util_count_stones_near_pos"] << "AI: found total value " << total << " of objects types " << NameObject[res_start_index] << " - " << NameObject[res_end_index];
  return total;
}

// count the number of knights that would be sent out if occupation level is changed, to avoid depleting the reserve of idle_knights
//  this function only counts the **TARGET** levels, ignoring if that many knights are actually available and/or currently in buildings
unsigned int
AI::count_knights_affected_by_occupation_level_change(unsigned int current_level, unsigned int new_level) {
  AILogDebug["util_count_knights_affected_by_occupation_level_change"] << "inside count_knights_affected_by_occupation_level_change, level " << current_level << " -> " << new_level;
  // calling update_buildings here is too slow because of mutex locking and causes do_manage_knight_occupation_levels to leave things at zero
  //   for long enough that the game update actually starts moving knights around.  Having a super accurate count is not critical here, it should
  //   be able to use whatever values were found during the most recent update, which I believe is happening at the start of each AI loop anyway
  //update_buildings();
  // note this includes incomplete buildings? I think.  I doubt it matters much, though
  unsigned int hut_count = realm_building_count[Building::TypeHut];
  unsigned int tower_count = realm_building_count[Building::TypeTower];
  unsigned int garrison_count = realm_building_count[Building::TypeFortress];
  AILogDebug["util_count_knights_affected_by_occupation_level_change"] << "military building counts: huts " << hut_count << ", towers " << tower_count << ", garrisons " << garrison_count;
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
    AILogDebug["util_count_knights_affected_by_occupation_level_change"] << "diff " << diff << " is negative, setting to zero instead";
    diff = 0;
  }
  AILogDebug["util_count_knights_affected_by_occupation_level_change"] << "knight counts if at fully-occupied-*target* staffing level: current " << total_current << ", new " << total_new << ", diff " << diff;
  return diff;
}

// this is only considering expanding the borders of the military buildings associated with this stock
//   should it be done this way per-stock, or once globally using realm_occupied_military_buildings ?
void
AI::expand_borders() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  AILogDebug["util_expand_borders"] << inventory_pos << " start of AI::expand_borders";
  ai_status.assign("EXPANDING BORDERS");

  // don't expand borders if running low of knights AND already have all 3 mine types
  unsigned int idle_knights = serfs_idle[Serf::TypeKnight0] + serfs_idle[Serf::TypeKnight1] + serfs_idle[Serf::TypeKnight2] + serfs_idle[Serf::TypeKnight3] + serfs_idle[Serf::TypeKnight4];
  if (idle_knights <= knights_med) {
    AILogDebug["util_expand_borders"] << inventory_pos << " running low on idle_knights, checking to see if already have all three mine types";
    if (realm_building_count[Building::TypeCoalMine] > 0 &&
        realm_building_count[Building::TypeIronMine] > 0 &&
        realm_building_count[Building::TypeGoldMine] > 0){
      AILogDebug["util_expand_borders"] << inventory_pos << " running low on idle_knights and have at least one of each mine type, not expanding borders";
      return;
    }
  }

  MapPos built_pos = bad_map_pos;
  MapPosSet count_by_corner;

  for (std::string goal : expand_towards) {
    AILogDebug["util_expand_borders"] << inventory_pos << " expand_towards goal list includes item: " << goal;
  }

  // search outward from each military building until border pos reached
  //  then score that area
  for (MapPos center_pos : stock_building_counts.at(inventory_pos).occupied_military_pos) {
    duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    AILogDebug["util_expand_borders"] << inventory_pos << " about to check around pos " << center_pos << ", SO FAR util_expand_borders call took " << duration;
    // find territory edge in each direction
    for (Direction dir : cycle_directions_rand_cw()) {
      MapPos pos = center_pos;
      AILogDebug["util_expand_borders"] << inventory_pos << " looking for territory edge from pos " << pos << " in direction " << dir << " / " << NameDirection[dir];
      // give up after 10 tiles because we should only be looking from huts that are right on the borders, not internal ones
      //  these huts should be at most 8 tiles or so from the border
      // Without this optimization every single dir from every hut is followed to borders and area scored, resulting
      //  in a lot of duplicate scoring
      // Also, it makes the "check for circumnavigating the globe" obsolete because that is way more than ten tiles
      for (int tiles_moved = 0; tiles_moved < 11; tiles_moved++){
        pos = map->move(pos, dir);
  //AILogDebug["util_expand_borders"] << "mappos " << pos << " is owned by player " << map->get_owner(pos) << " and has_owner is " << map->has_owner(pos);
        if (map->get_owner(pos) != player_index){
          unsigned int score = AI::score_area(pos, AI::spiral_dist(6));
          AILogDebug["util_expand_borders"] << inventory_pos << " border in direction " << dir << " has score " << score;
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
    // ensure Inventory unfinished_knight_huts below limit
    if (stock_building_counts.at(inventory_pos).unfinished_hut_count >= max_unfinished_huts) {
      AILogDebug["util_expand_borders"] << inventory_pos << " Inventory unfinished_huts_count limit " << max_unfinished_huts << " reached, cannot build knight huts for this Inventory";
      break;
    }
    AILogDebug["util_expand_borders"] << inventory_pos << " try to build knight hut near pos " << corner_pos;

    duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    AILogDebug["util_expand_borders"] << inventory_pos << " SO FAR util_expand_borders call took " << duration;

    road_options.set(RoadOption::IncreasedNewLengthPenalty);
    road_options.reset(RoadOption::AllowPassthru);
    built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(4), Building::TypeHut);
    road_options.reset(RoadOption::IncreasedNewLengthPenalty);
    road_options.set(RoadOption::AllowPassthru);
    if (built_pos != bad_map_pos && built_pos != notplaced_pos){
      AILogDebug["util_expand_borders"] << inventory_pos << " built a knight hut at pos " << built_pos;
      stock_building_counts.at(inventory_pos).unfinished_hut_count++;
    }
  }

  // if we can no longer expand by building Huts, alter the attack behavior to make desperate attacks to claim resources
  stock_building_counts.at(inventory_pos).inv_cannot_expand_borders = false;
  if (built_pos == bad_map_pos || built_pos == notplaced_pos){
    AILogDebug["util_expand_borders"] << inventory_pos << " no knight huts could be placed around this Inventory.  Setting inv_cannot_expand_borders bool to true";
    stock_building_counts.at(inventory_pos).inv_cannot_expand_borders = true;
  }

  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_expand_borders"] << inventory_pos << " done util_expand_borders call took " << duration;

  // this function used to accept and return a MapPos, but no longer does so just return
  return;
}


// score area for placement of new military buildings
//  to expand borders
unsigned int
AI::score_area(MapPos center_pos, unsigned int distance) {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  //AILogDebug["util_score_area"] << "inside AI::score_area, center_pos " << center_pos << ", distance " << distance;
  unsigned int total_value = 0;
  //ai_mark_pos.erase(center_pos);
  //ai_mark_pos.insert(ColorDot(center_pos, "white"));
  //sleep_speed_adjusted(100);
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    Map::Object obj = map->get_obj(pos);
    ////ai_mark_pos.erase(pos);
    //ai_mark_pos.insert(ColorDot(pos, "gray"));
    //sleep_speed_adjusted(100);
      
    //AILogDebug["util_score_area"] << "at pos " << pos << " with object type " << NameObject[obj];
    size_t pos_value = 0;  // easier to make this size_t than static_cast all the .size values

    //
    // terrain and resources
    //
    unsigned int gold_signs = 0;
    unsigned int iron_signs = 0;
    unsigned int coal_signs = 0;
    unsigned int stone_signs = 0;
    if (obj == Map::ObjectNone &&  // if has a field, or can build a field here
      (AI::has_terrain_type(game, pos, Map::TerrainGrass0, Map::TerrainGrass3) ||
        AI::has_terrain_type(game, pos, Map::TerrainWater0, Map::TerrainWater3)) ||
      obj >= Map::ObjectSeeds0 && obj <= Map::ObjectFieldExpired ||
      obj >= Map::ObjectField0 && obj <= Map::ObjectField5) {
      pos_value += expand_towards.count("foods") * foods_weight;
      //AILogDebug["util_score_area"] << "adding potential & existing fields count 1 with value " << expand_towards.count("foods") * foods_weight;
    }
    if (obj >= Map::ObjectTree0 && obj <= Map::ObjectPine7) {
      pos_value += expand_towards.count("trees") * trees_weight;
      //AILogDebug["util_score_area"] << "adding trees count 1 with value " << expand_towards.count("trees") * trees_weight;
    }
    if (obj >= Map::ObjectStone0 && obj <= Map::ObjectStone7) {
      int stonepile_value = 1 + (-1 * (obj - Map::ObjectStone7));
      pos_value += expand_towards.count("stones") * stones_weight * stonepile_value;
      //AILogDebug["util_score_area"] << "adding stones count " << stonepile_value << " with value " << expand_towards.count("stones") * stones_weight;
    }
    if (AI::has_terrain_type(game, pos, Map::TerrainTundra0, Map::TerrainSnow0)) {
      if (obj >= Map::ObjectSignEmpty && obj <= Map::ObjectSignSmallStone) {
        //AILogDebug["util_score_area"] << "found a sign (of type " << NameObject[obj] << "), not counting this hill";
      }
      else {
        pos_value += expand_towards.count("hills") * hills_weight;
        //AILogDebug["util_score_area"] << "adding hills count 1 with value " << expand_towards.count("hills") * hills_weight;
      }
    }
    if (obj == Map::ObjectSignLargeGold) { gold_signs += 2; }
    if (obj == Map::ObjectSignSmallGold) { gold_signs += 1; }
    if (obj == Map::ObjectSignSmallGold || obj == Map::ObjectSignLargeGold) {
      pos_value += expand_towards.count("gold_ore") * gold_ore_weight * gold_signs;
      //AILogDebug["util_score_area"] << "adding gold_ore count " << gold_signs << " with value " << expand_towards.count("gold_ore") * gold_ore_weight * gold_signs;
    }
    if (obj == Map::ObjectSignLargeIron) { iron_signs += 2; }
    if (obj == Map::ObjectSignSmallIron) { iron_signs += 1; }
    if (obj == Map::ObjectSignSmallIron || obj == Map::ObjectSignLargeIron) {
      pos_value += expand_towards.count("iron_ore") * iron_ore_weight * iron_signs;
      //AILogDebug["util_score_area"] << "adding iron_ore count " << iron_signs << " with value " << expand_towards.count("iron_ore") * iron_ore_weight * iron_signs;
    }
    if (obj == Map::ObjectSignLargeCoal) { coal_signs += 2; }
    if (obj == Map::ObjectSignSmallCoal) { coal_signs += 1; }
    if (obj == Map::ObjectSignSmallCoal || obj == Map::ObjectSignLargeCoal) {
      pos_value += expand_towards.count("coal")      * coal_weight * coal_signs;
      //AILogDebug["util_score_area"] << "adding coal count " << coal_signs << " with value " << expand_towards.count("coal_ore") * coal_weight * coal_signs;
    }
    if (obj == Map::ObjectSignLargeStone) { stone_signs += 2; }
    if (obj == Map::ObjectSignSmallStone) { stone_signs += 1; }
    if (obj == Map::ObjectSignSmallStone || obj == Map::ObjectSignLargeStone) {
      pos_value += expand_towards.count("stones") * stone_signs_weight * stone_signs;
      //AILogDebug["util_score_area"] << "adding stones count " << stone_signs << " with value " << expand_towards.count("stones") * stone_signs_weight * stone_signs;
    }

    // oppose_enemy - any place that enemy territory found
    //    this has the effect of tending to encircle the enemy, which is a good thing
    if (map->get_owner(pos) != player_index && map->has_owner(pos)) {
      if (expand_towards.count("oppose_enemy") == 0){
        expand_towards.insert("oppose_enemy");
        //AILogDebug["util_score_area"] << "found enemy territory near our borders, adding oppose_enemy expansion goal for informative purpose";
      }
      pos_value += expand_towards.count("oppose_enemy") * 2;
      //AILogDebug["util_score_area"] << "adding oppose_enemy value for enemy territory";
    }

    //
    // protect_economy - value areas where own civilian buildings are near our borders
    //    to ensure they are protected
    //
    //  TODO: are mines Large or Small?  Should they get extra protection? 
    //    or even moreso... check to see if they are the only mine of that type
    //    we have and if so score highly?
    if (map->get_owner(pos) == player_index) {
      // "auto-add" this expansion goal for AI Overlay visualization
      if (expand_towards.count("protect_economy") == 0){
        expand_towards.insert("protect_economy");
        //AILogDebug["util_score_area"] << "found one of our civilian buildings near our borders, adding protect_economy expansion goal for informative purpose";
      }
      if (obj == Map::ObjectLargeBuilding && !game->get_building_at_pos(pos)->is_military()) {
        pos_value += expand_towards.count("protect_economy") * 3;
        //AILogDebug["util_score_area"] << "adding protect_economy building value x3 for large civilian building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
      }
      if (obj == Map::ObjectSmallBuilding && !game->get_building_at_pos(pos)->is_military()) {
        pos_value += expand_towards.count("protect_economy") * 1;
        //AILogDebug["util_score_area"] << "adding protect_economy building value x1 for small civilian building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
      }
    }

    //
    // castle_buffer
    //
    //  tiles owned by nobody, outside our borders, where the center_pos
    //   is near the castle, are valued to encourage building huts the castle
    if (get_straightline_tile_dist(map, center_pos, castle_flag_pos) < 18){
      if (!map->has_owner(pos)) {
        pos_value += expand_towards.count("castle_buffer") * 1;
        //AILogDebug["util_score_area"] << "adding castle_buffer value for unclaimed territory near our castle with value of " << expand_towards.count("castle_buffer") * 2;
        //ai_mark_pos.erase(pos);
        //ai_mark_pos.insert(ColorDot(pos, "black"));
        //sleep_speed_adjusted(100);
      }
    }

    //
    // total it up
    //
    total_value += static_cast<unsigned int>(pos_value);
    //AILogDebug["util_score_area"] << "score total so far: " << total_value;

  }
  AILogDebug["util_score_area"] << "found total score_area value " << total_value << " of terrain & objects in area " << center_pos;
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_score_area"] << "done util_score_area call took " << duration;
  return total_value;
}


// score enemy area for attack
unsigned int
AI::score_enemy_area(MapPos center_pos, unsigned int distance) {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  //AILogDebug["util_score_enemy_area"] << "inside AI::score_enemy_area, center_pos " << center_pos << ", distance " << distance;
  unsigned int total_value = 0;
  //ai_mark_pos.erase(center_pos);
  //ai_mark_pos.insert(ColorDot(center_pos, "white"));
  //sleep_speed_adjusted(100);
  for (unsigned int i = 0; i < distance; i++) {
    MapPos pos = map->pos_add_extended_spirally(center_pos, i);
    Map::Object obj = map->get_obj(pos);
    //ai_mark_pos.erase(pos);
    //ai_mark_pos.insert(ColorDot(pos, "gray"));
    //sleep_speed_adjusted(100);
      
    //AILogDebug["util_score_enemy_area"] << "at pos " << pos << " with object type " << NameObject[obj];
    size_t pos_value = 0;  // easier to make this size_t than static_cast all the .size values

    //
    // prioritize any enemy building that is close to OUR castle
    //
    if (cannot_expand_borders){
      // do NOT count enemy buildings for scoring, only resources
      AILogDebug["util_score_enemy_area"] << "cannot_expand_borders is true, not counting enemy buildings towards total";
    }else{
      // normal behavior
      if (obj == Map::ObjectCastle && map->get_owner(pos) == player_index){
        AILogDebug["util_score_enemy_area"] << "found our own castle in attack area!  scoring this area highly to reduce risk of our castle flag paths becoming blocked";
        if (obj == Map::ObjectCastle){
          pos_value += 75;
          //AILogDebug["util_score_enemy_area"] << "adding attack value 75 for enemy building near our own castle at pos " << pos;
          continue;
        }
      }
    }

    //
    // stone piles above ground
    //
    if (cannot_expand_borders && !no_stone_within_borders){
      // disregard this resource, some other resource is desperately needed
      AILogDebug["util_score_enemy_area"] << "cannot_expand_borders is true but stone is already within our borders. not counting stone towards total";
    }else{
      // normal behavior
      if (map->get_obj(pos) >= Map::ObjectStone0 && map->get_obj(pos) <= Map::ObjectStone7) {
        //AILogDebug["util_count_stones_near_pos"] << "getting value of map object at pos: " << pos << " with map obj type " << NameObject[map->get_obj(pos)];
        // because the higher MapObject indexes represent smaller resource values, the difference is negative
        //   so invert the difference and add one, because it starts at zero
        int value = 1 + (-1 * (map->get_obj(pos) - Map::ObjectStone7));
        pos_value += expand_towards.count("stone") * value;
        //AILogDebug["util_score_enemy_area"] << "adding stones count " << value << " with value " << expand_towards.count("stones") * value;
      }
    }

    //
    // mined resources
    //
    // Gold
    if (cannot_expand_borders && !no_goldore_within_borders){
      // disregard this resource, some other resource is desperately needed
      AILogDebug["util_score_enemy_area"] << "cannot_expand_borders is true but gold is already within our borders. not counting gold towards total";
    }else{
      // normal behavior
      unsigned int gold_signs = 0;
      if (obj == Map::ObjectSignLargeGold) { gold_signs += 2; }
      if (obj == Map::ObjectSignSmallGold) { gold_signs += 1; }
      if (obj == Map::ObjectSignSmallGold || obj == Map::ObjectSignLargeGold) {
        pos_value += expand_towards.count("gold_ore") * gold_ore_weight * gold_signs;
        //AILogDebug["util_score_enemy_area"] << "adding gold_ore count " << gold_signs << " with value " << expand_towards.count("gold_ore") * gold_ore_weight * gold_signs;
      }
    }
    // Iron
    if (cannot_expand_borders && !no_ironore_within_borders){
      // disregard this resource, some other resource is desperately needed
      AILogDebug["util_score_enemy_area"] << "cannot_expand_borders is true but iron is already within our borders. not counting iron towards total";
    }else{
      // normal behavior
      unsigned int iron_signs = 0;
      if (obj == Map::ObjectSignLargeIron) { iron_signs += 2; }
      if (obj == Map::ObjectSignSmallIron) { iron_signs += 1; }
      if (obj == Map::ObjectSignSmallIron || obj == Map::ObjectSignLargeIron) {
        pos_value += expand_towards.count("iron_ore") * iron_ore_weight * iron_signs;
        //AILogDebug["util_score_enemy_area"] << "adding iron_ore count " << iron_signs << " with value " << expand_towards.count("iron_ore") * iron_ore_weight * iron_signs;
      }
    }
    // Coal
    if (cannot_expand_borders && !no_coal_within_borders){
      // disregard this resource, some other resource is desperately needed
      AILogDebug["util_score_enemy_area"] << "cannot_expand_borders is true but coal is already within our borders. not counting coal towards total";
    }else{
      // normal behavior
      unsigned int coal_signs = 0;
      if (obj == Map::ObjectSignLargeCoal) { coal_signs += 2; }
      if (obj == Map::ObjectSignSmallCoal) { coal_signs += 1; }
      if (obj == Map::ObjectSignSmallCoal || obj == Map::ObjectSignLargeCoal) {
        pos_value += expand_towards.count("coal")      * coal_weight * coal_signs;
        //AILogDebug["util_score_enemy_area"] << "adding coal count " << coal_signs << " with value " << expand_towards.count("coal_ore") * coal_weight * coal_signs;
      }
    }
    // Stone in mountains
    if (cannot_expand_borders && !no_stone_within_borders){
      // disregard this resource, some other resource is desperately needed
      AILogDebug["util_score_enemy_area"] << "cannot_expand_borders is true but stone is already within our borders. not counting mined-stone-in-mountains towards total";
    }else{
      // normal behavior
      unsigned int stone_signs = 0;
      if (obj == Map::ObjectSignLargeStone) { stone_signs += 2; }
      if (obj == Map::ObjectSignSmallStone) { stone_signs += 1; }
      if (obj == Map::ObjectSignSmallStone || obj == Map::ObjectSignLargeStone) {
        pos_value += expand_towards.count("stone") * stone_signs;
      }
    }

    //  NEED TO ADD LOGIC TO "BEELINE" TO ENEMY CASTLE
    //   perhaps by checking the straight-line dist from this enemy building
    //   to enemy castle, and scoring based on that only
    //

    //
    // enemy buildings
    //
    if (cannot_expand_borders){
      // do NOT count enemy buildings for scoring, only resources
      AILogDebug["util_score_enemy_area"] << "cannot_expand_borders is true, not counting enemy buildings towards total";
    }else{
      // normal behavior
      if (map->get_owner(pos) != player_index && map->has_owner(pos) && map->has_building(pos)){
        //AILogDebug["util_score_enemy_area"] << "potential attack area contains a building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()];
        if (obj == Map::ObjectCastle){
          pos_value += 25;  // this needs to be high enough to ensure that a nearly-isolated castle is still worth isolating even if no resources nearby
          //AILogDebug["util_score_enemy_area"] << "adding attack value 20 for enemy castle at pos " << pos;
        }
        if (obj == Map::ObjectLargeBuilding && game->get_building_at_pos(pos)->get_type() == Building::TypeStock){
          pos_value += 25;
          //AILogDebug["util_score_enemy_area"] << "adding additional attack value 12 for enemy stock/warehouse at pos " << pos;
        }
        if (obj == Map::ObjectLargeBuilding && !game->get_building_at_pos(pos)->is_military()) {
          pos_value += 9;
          //AILogDebug["util_score_enemy_area"] << "adding attack value 9 for large civilian building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
        }
        if (obj == Map::ObjectSmallBuilding && game->get_building_at_pos(pos)->get_type() == Building::TypeCoalMine){
          pos_value += 6;
          //AILogDebug["util_score_enemy_area"] << "adding additional attack value 6 for building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
        }
        if (obj == Map::ObjectSmallBuilding && game->get_building_at_pos(pos)->get_type() == Building::TypeIronMine) {
          pos_value += 12;
          //AILogDebug["util_score_enemy_area"] << "adding additional attack value 12 for building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
        }
        if (obj == Map::ObjectSmallBuilding && game->get_building_at_pos(pos)->get_type() == Building::TypeGoldMine) {
          pos_value += 25;
          //AILogDebug["util_score_enemy_area"] << "adding additional attack value 18 for building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
        }
        if (obj == Map::ObjectSmallBuilding && !game->get_building_at_pos(pos)->is_military()) {
          pos_value += 3;
          //AILogDebug["util_score_enemy_area"] << "adding attack value 3 for small civilian building of type " << NameBuilding[game->get_building_at_pos(pos)->get_type()] << " at pos " << pos;
        }
      }
    }

    //
    // total it up
    //
    total_value += static_cast<unsigned int>(pos_value);
    //AILogDebug["util_score_enemy_area"] << "score total so far: " << total_value;

  }
  AILogDebug["util_score_enemy_area"] << "found total score_enemy_area value " << total_value << " of terrain & objects in area " << center_pos;
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_score_enemy_area"] << "done util_score_enemy_area call took " << duration;
  return total_value;
}


bool
AI::is_bad_building_pos(MapPos pos, Building::Type building_type) {
  //AILogDebug["util_is_bad_building_pos"] << "inside AI::is_bad_building_pos";
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  if (bad_building_pos.find(std::make_pair(pos, building_type)) != bad_building_pos.end()) {
    return true;
  }
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_score_enemy_area"] << "is_bad_building_pos call took " << duration << ", bad_building_pos has " << bad_building_pos.size() << " elements";
  return false;
}


// get halfway point between two MapPos
MapPos
AI::get_halfway_pos(MapPos start_pos, MapPos end_pos) {
  AILogDebug["util_get_halfway_pos"] << "inside get_halfway_pos";
  MapPos halfway_pos;
  int start_col = map->pos_col(start_pos);
  int start_row = map->pos_row(start_pos);
  AILogDebug["util_get_halfway_pos"] << "start_pos is " << start_pos << ", start_col is " << start_col << ", start_row is " << start_row;
  int end_col = map->pos_col(end_pos);
  int end_row = map->pos_row(end_pos);
  AILogDebug["util_get_halfway_pos"] << "end_col is " << end_col << ", end_row is " << end_row;
  // it seems these are reversed start/end pos
  int col_dist = map->dist_x(start_pos, end_pos);
  AILogDebug["util_get_halfway_pos"] << "col_dist is " << col_dist;
  int row_dist = map->dist_y(start_pos, end_pos);
  AILogDebug["util_get_halfway_pos"] << "row_dist is " << row_dist;
  int halfway_col_dist = (map->dist_x(start_pos, end_pos) / 2);
  AILogDebug["util_get_halfway_pos"] << "halfway_col_dist is " << halfway_col_dist;
  int halfway_row_dist = (map->dist_y(start_pos, end_pos) / 2);
  AILogDebug["util_get_halfway_pos"] << "halfway_row_dist is " << halfway_row_dist;
  halfway_pos = map->pos(end_col + halfway_col_dist, end_row + halfway_row_dist);
  AILogDebug["util_get_halfway_pos"] << "halfway_pos is " << halfway_pos;
  return halfway_pos;
}

void
AI::score_enemy_targets(MapPosSet *scored_targets) {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  AILogDebug["util_score_enemy_targets"] << "inside score_enemy_targets";
  //
  // MAJOR BUG - this is running before expansion goals are set (later in the AI loop), so it is not looking at resources that could
  //      be taken, ONLY at enemy buildings.  Does it make sense to persist expansion goals between AI loops?  Or move attacks to the end of the loop?

  //  temp fix -  stupid hack to work-around it
  expand_towards = last_expand_towards;

  update_buildings();
  Game::ListBuildings buildings = game->get_player_buildings(player);
  std::set<MapPos> unique_enemy_targets = {};
  // foreach my hut in range of attacking enemy
  //    foreach enemy hut within range of attack
  //      score
  std::clock_t this_item_start = std::clock();
  for (Building *building : buildings) {
    double duration_so_far = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    double duration_last_item = (std::clock() - this_item_start) / static_cast<double>(CLOCKS_PER_SEC);
    AILogDebug["util_score_enemy_targets"] << "inside score_enemy_targets, total time spent so far " << duration_so_far << ", last item took " << duration_last_item;
    this_item_start = std::clock();
    
    if (building == nullptr)
      continue;
    if (!building->is_done() ||
      !building->is_military() ||
      !building->is_active() ||
      !(building->get_threat_level() == 3)) {
      continue;
    }
    //ai_mark_pos.clear();
    //AILogDebug["util_score_enemy_targets"] << "looking for enemy buildings to attack near to my building of type " << NameBuilding[building->get_type()] << " at pos " << building->get_position();
    MapPos attacker_pos = building->get_position();
    // compile a list for scoring of each enemy building in attackable radius (which is?? NEED TO FIND OUT)
    //  at 14, seeing some unattackable targets, lowering to 13
    //  trying 12, was seeing unreachable buildings at 13
    //  12 looks good NO I see REACHable buildings that aren't showing as within 12, going back to 13
    //  I am seeing an attackable building that isn't showing up in this check, may need to switch to
    //  the actual game logic instead of this guessing...
    for (unsigned int i = 0; i < AI::spiral_dist(13); i++) {
      MapPos pos = map->pos_add_extended_spirally(attacker_pos, i);
      if (!map->has_building(pos) ||
        map->get_owner(pos) == player_index) {
        continue;
      }
      Building *target_building = game->get_building_at_pos(pos);
      if (target_building == nullptr) {
        AILogDebug["util_score_enemy_targets"] << "target_building is nullptr!  at pos " << pos;
        continue;
      }
      if (!target_building->is_military() || !target_building->is_active() || !building->get_threat_level() == 3){
        continue;
      }
      MapPos target_pos = pos;
      Building::Type target_building_type = target_building->get_type();
      size_t target_player_index = map->get_owner(pos);
      AILogDebug["util_score_enemy_targets"] << "our building of type " << NameBuilding[building->get_type()] << " at pos " << attacker_pos << " can attack building of type " << NameBuilding[target_building_type] << " at pos " << target_pos << " belonging to player " << target_player_index;

      /* UPDATE - this call is quite slow, moving it to AFTER score_area and sorting
         is done to see if that is actually faster
      // copied from viewport open-attack-window logic, I am not sure exactly what the 
      //  purpose of this code is, as how could there be more knights available to 
      //  attack than there are knights in the building?  Except for castle,
      //  maybe the intent was simply to disallow attacking with more than 20 knights
      //  at once from the castle, and the others have no effect?  
      // ... can you even attack directly from the castle?  I forget
      int max_knights = 0;
      switch (building->get_type()) {  // limit based on our building, the attacking building
      case Building::TypeHut: max_knights = 3; break;
      case Building::TypeTower: max_knights = 6; break;
      case Building::TypeFortress: max_knights = 12; break;
      case Building::TypeCastle: max_knights = 20; break;
      default: NOT_REACHED(); break;
      }
      std::clock_t foo = std::clock();
      int attacking_knights = player->knights_available_for_attack(target_pos);
      AILogDebug["util_score_enemy_targets"] << "inside score_enemy_targets, this player->knights_available_for_attack(" << target_pos << ") took " << (std::clock() - foo) / static_cast<double>(CLOCKS_PER_SEC);

      attacking_knights = std::min(attacking_knights, max_knights);
      //AILogDebug["util_score_enemy_targets"] << "send up to " << attacking_knights << " knights to attack enemy building of type " << NameBuilding[target_building_type]
      //  << " at pos " << target_pos << " belonging to player " << target_player_index << " / " << target_player_face;
      AILogDebug["util_score_enemy_targets"] << "our building of type " << NameBuilding[building->get_type()] << " at pos " << attacker_pos << " can attack with up to " << attacking_knights << " knights to attack enemy building of type " << NameBuilding[target_building_type]
        << " at pos " << target_pos << " belonging to player " << target_player_index;
      if (attacking_knights <= 0) {
        AILogDebug["util_score_enemy_targets"] << "our building of type " << NameBuilding[building->get_type()] << " at pos " << attacker_pos << " cannot send any knights, not marking target for scoring";
        continue;
      }
      */
      if (target_building_type == Building::TypeCastle){
        //
        // DO SOMETHING HERE IF TARGET IS ENEMY CASTLE - EITHER NEVER ATTACK IT OR ALWAYS ATTACK IT
        //  IF IT IS REACHABLE, BASED ON OWN MORALE AND KNIGHT COUNT?
        //
        // for now, simply including it in scorable targets
        AILogDebug["util_score_enemy_targets"] << "enemy castle is attackable!  At target_pos " << target_pos << ".  Including it in unique_targets list for scoring";
        //ai_mark_pos.insert(ColorDot(target_pos, "lt_blue"));
        unique_enemy_targets.insert(target_pos);
      }else{
        AILogDebug["util_score_enemy_targets"] << "adding target_pos " << target_pos << " to unique_enemy_targets set to score";
        //ai_mark_pos.insert(ColorDot(target_pos, "black"));
        unique_enemy_targets.insert(target_pos);
      }
    }
  }

  AILogDebug["util_score_enemy_targets"] << "done finding targets, unique_enemy_targets contains " << unique_enemy_targets.size() << " items";

  // score the targets found
  for (MapPos target_pos : unique_enemy_targets){
    AILogDebug["util_score_enemy_targets"] << "scoring attackable building at pos " << target_pos;
    //unsigned int score = AI::score_enemy_area(target_pos, AI::spiral_dist(8));
    unsigned int score = AI::score_enemy_area(target_pos, AI::spiral_dist(10)); // try scoring a bit wider
    AILogDebug["util_score_enemy_targets"] << "attackable enemy target at pos " << target_pos << " has score " << score;
    scored_targets->insert(std::make_pair(target_pos, score));
    /*
    // debug - mark score of each target on AI overlay
    if (score >  12){ ai_mark_pos.erase(target_pos); ai_mark_pos.insert(ColorDot(target_pos, "dk_gray"));}
    if (score >  25){ ai_mark_pos.erase(target_pos); ai_mark_pos.insert(ColorDot(target_pos, "gray"));}
    if (score >  37){ ai_mark_pos.erase(target_pos); ai_mark_pos.insert(ColorDot(target_pos, "yellow"));}
    if (score >  50){ ai_mark_pos.erase(target_pos); ai_mark_pos.insert(ColorDot(target_pos, "orange"));}
    if (score >  75){ ai_mark_pos.erase(target_pos); ai_mark_pos.insert(ColorDot(target_pos, "red"));}
    if (score > 100){ ai_mark_pos.erase(target_pos); ai_mark_pos.insert(ColorDot(target_pos, "magenta"));}
    if (score > 125){ ai_mark_pos.erase(target_pos); ai_mark_pos.insert(ColorDot(target_pos, "cyan"));}
    */
  }

  // second part of stupid hack to work-around it
  expand_towards.clear();

  AILogDebug["util_score_enemy_targets"] << "done score_enemy_targets";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_score_enemy_targets"] << "done score_enemy_targets call took " << duration;
}



void
//AI::attack_best_target(MapPosSet *scored_targets) {
//AI::attack_best_target(MapPosSet *scored_targets, unsigned int min_score, double min_ratio) {
AI::attack_best_target(MapPosSet *scored_targets, int loss_tolerance) {
  AILogDebug["util_attack_best_target"] << "inside AI::attack_best_target with loss_tolerance " << loss_tolerance << ", cannot_expand_borders bool is " << cannot_expand_borders;
  
  MapPosVector sorted_scored_targets = sort_by_val_desc(*(scored_targets));

  for (MapPos target_pos : sorted_scored_targets) {
    // get the score for this target
    unsigned int target_score = 0;
    for (std::pair<MapPos, unsigned int> target : *(scored_targets)){
      if (target.first == target_pos){
        target_score = target.second;
        break;
      }
    }

    AILogDebug["util_attack_best_target"] << "target with target_pos " << target_pos << " has score " << target_score;

    Building *target_building = game->get_building_at_pos(target_pos);
    if (target_building == nullptr){
      AILogWarn["util_attack_best_target"] << "target building at pos " << target_pos << " is now nullptr!  skipping";
      continue;
    }

    int estimated_defenders = target_building->get_knight_count();  // this might be "exact number of defenders" now, oh well
    AILogDebug["util_attack_best_target"] << "target_building of type " << NameBuilding[target_building->get_type()] << " has estimated_defenders " << estimated_defenders;


    // copied from viewport open-attack-window logic, I am not sure exactly what the 
    //  purpose of this code is, as how could there be more knights available to 
    //  attack than there are knights in the building?  Except for castle,
    //  maybe the intent was simply to disallow attacking with more than 20 knights
    //  at once from the castle, and the others have no effect?  
    // ... can you even attack directly from the castle?  I forget
    int max_knights = 0;
    switch (target_building->get_type()) {  // limit based on our building, the attacking building
      case Building::TypeHut: max_knights = 3; break;
      case Building::TypeTower: max_knights = 6; break;
      case Building::TypeFortress: max_knights = 12; break;
      case Building::TypeCastle: max_knights = 20; break;
      default: NOT_REACHED(); break;
    }

    // this call is quite slow, time it.  Originally I was doing it PRIOR to scoring, but it was slow enough that
    //  I tried moving it here, after scoring
    std::clock_t foo = std::clock();
    int attacking_knights = player->knights_available_for_attack(target_pos);
    AILogDebug["util_attack_best_target"] << "this player->knights_available_for_attack(" << target_pos << ") took " << (std::clock() - foo) / static_cast<double>(CLOCKS_PER_SEC);

    attacking_knights = std::min(attacking_knights, max_knights);
    if (attacking_knights <= 0) {
      AILogDebug["util_attack_best_target"] << "to target_building pos " << target_building->get_position() << ", we cannot send any knights, skipping it";
      continue;
    }
    AILogDebug["util_attack_best_target"] << "can send up to " << attacking_knights << " knights to attack enemy building at pos " << target_pos;
    
    // if AI can no longer expand by building huts and has little to none of one or more resources, it MUST ATTACK to gain them
    if (cannot_expand_borders && (no_coal_within_borders || no_ironore_within_borders || no_goldore_within_borders)){
      AILogDebug["util_attack_best_target"] << "at least one mined resource is totally unavailable within our borders, and we cannot expand borders, will make desperate attacks for resouces!";
    }else{
      //
      // handle normal case where AI can still expand, use caution
      //
      //AILogDebug["util_attack_best_target"] << "ENEMY military_score = " << game->get_player(target_player_index)->get_military_score() << ",  ENEMY knight_morale = " << game->get_player(target_player_index)->get_knight_morale();
      // enemy knight morale does NOT MATTER when attacking because it is always 100% for defenders (even if their attack morale is higher!)
      double enemy_morale = 100.00;
      double morale = (100*player->get_knight_morale())/0x1000;  // this should be % morale, not the integer that defaults to 4096
      // NOTE that this completely ignores knight experience level, because
      //  for enemies it is unknown (though it is technically possible to track it if knights are observed entering/leaving)
      //  and for friendly buildings it is not obvious, though it could be determined by checking each building within range
      //  and accounting for the "send strong/weak to battle" setting
      // if our morale is > 100, our expected losses are less than one knight per defender
      // if our morale is < 100, our expected losses are more than one knight per defender
      unsigned int expected_losses = double(enemy_morale / morale) * estimated_defenders;
      if (expected_losses < 1){expected_losses = 1;} // avoid divide by zero error
      AILogDebug["util_attack_best_target"] << "enemy_morale " << enemy_morale << ", our morale " << morale << ", expected_losses to attack this building are " << expected_losses;

      if (loss_tolerance == 3){
        if (target_score > 100 && attacking_knights > 1 * expected_losses){
          AILogDebug["util_attack_best_target"] << "loss_tolerance is high, this target has an exceptionally high score of " << target_score << " and victory is possible, will attack";
        }else if (target_score / expected_losses > 15 && attacking_knights > 1.25 * expected_losses){
          AILogDebug["util_attack_best_target"] << "loss_tolerance is high, this target_score / expected_losses ratio " << target_score / expected_losses << ":1 is over 15, an acceptable risk, and victory is likely, will attack";
        }else{
          AILogDebug["util_attack_best_target"] << "loss_tolerance is high, this target_score / expected_losses ratio " << target_score / expected_losses << ":1 is too low, OR we are not likely to capture it, skipping target";
          continue;
        }
      }else if (loss_tolerance == 2){
        if (target_score > 100 && attacking_knights > 1.5 * expected_losses){
          AILogDebug["util_attack_best_target"] << "loss_tolerance is moderate, this target has an exceptionally high score of " << target_score << " and victory is possible, will attack";
        }else if (target_score / expected_losses > 25 && attacking_knights > 1.5 * expected_losses){
          AILogDebug["util_attack_best_target"] << "loss_tolerance is moderate, this target_score / expected_losses ratio " << target_score / expected_losses << ":1 is over 25, a worthy risk, and victory is likely, will attack";
        }else{
          AILogDebug["util_attack_best_target"] << "loss_tolerance is moderate, this target_score / expected_losses ratio " << target_score / expected_losses << ":1 is too low, OR we are not likely to capture it, skipping target";
          continue;
        }
      }else if (loss_tolerance == 1){
        if (target_score > 100 && attacking_knights > 2 * expected_losses){
          AILogDebug["util_attack_best_target"] << "loss_tolerance is low, this target has an exceptionally high score of " << target_score << " and victory is likely, will attack";
        }else if (target_score / expected_losses > 40 && attacking_knights > 2 * expected_losses){
          AILogDebug["util_attack_best_target"] << "loss_tolerance is low, this target_score / expected_losses ratio " << target_score / expected_losses << ":1 is over 40, a worthy risk, and victory is likely, will attack";
        }else{
          AILogDebug["util_attack_best_target"] << "loss_tolerance is low, this target_score / expected_losses ratio " << target_score / expected_losses << ":1 is too low, OR we are not likely to capture it, skipping target";
          continue;
        }
      }else{
        throw ExceptionFreeserf("AI::attack_best_target was called with loss_tolerance of <1 or >3, this should not happen");
      }
    }
    AILogDebug["util_attack_best_target"] << "PROCEEDING WITH THE ATTACK on target_pos " << target_pos;
    player->building_attacked = target_building->get_index();
    player->attacking_building_count = attacking_knights;
    AILogDebug["util_attack_best_target"] << "calling player->start_attack()";
    player->start_attack();
    AILogDebug["util_attack_best_target"] << "DONE calling player->start_attack()";

    AILogDebug["util_attack_best_target"] << "only doing one attack per AI loop";
    break;
  }
  AILogDebug["util_attack_best_target"] << "done AI::attack_best_target";
}


// count the number of signs of any type, and the number of hills that have no blocking objects (signs are okay, they are not blocking)
// NOTE - signs can be placed on full-snow tiles which cannot actually have mines, so sign_density is unlikely to reach 100%
double
AI::count_geologist_sign_density(MapPos pos, unsigned int distance) {
  AILogDebug["count_geologist_sign_density"] << "inside count_geologist_sign_density, around pos " << pos;
  double signs_count = AI::count_objects_near_pos(pos, distance, Map::ObjectSignLargeGold, Map::ObjectSignSmallStone, "dk_orange");
  double empty_hills_count = AI::count_empty_terrain_near_pos(pos, distance, Map::TerrainTundra0, Map::TerrainSnow1, "orange");
  double sign_density = signs_count / empty_hills_count;
  AILogDebug["count_geologist_sign_density"] << "done, area around pos " << pos << " has signs_count: " << signs_count << ", empty_hills_count: " << empty_hills_count << ", sign_density: " << sign_density << ", deprioritize at " << geologist_sign_density_deprio;
  return sign_density;
}

// set any serf that is part of current ai_mark_serf to LostState
//  for debugging
void
AI::set_serf_lost() {
  Log::Info["set_serf_lost"] << "inside set_serf_lost";
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


// check a given flag return true if:
// - the flag has no resources sitting
// - the flag has only one road
// - the road has no transporters that are carrying resources
// ALSO, populate the Direction of the road in the provided reference
//
// this is intended for use by remove_road_stubs function
//  with various extra checks
//
bool
AI::flag_and_road_suitable_for_removal(PGame game, PMap map, MapPos flag_pos, Direction *road_dir){
  AILogDebug["util_flag_and_road_suitable_for_removal"] << "inside AI::flag_and_road_suitable_for_removal for flag at pos " << flag_pos;

  // check for paths in each Direction
  unsigned int paths = 0;
  for (Direction dir : cycle_directions_cw()) {
    if (!map->has_path_IMPROVED(flag_pos, dir)) { continue; }
    paths++;
    // reject if multiple paths
    if (paths > 1) {
      //AILogDebug["util_flag_and_road_suitable_for_removal"] << "flag at pos " << flag_pos << " has more than one path, not eligible for removal";
      return false;
    }
    // this variable is provided to the calling function by reference
    *(road_dir) = dir;
  }

  // reject if no path found
  if (paths == 0 || *(road_dir) == DirectionNone){
    //AILogDebug["util_flag_and_road_suitable_for_removal"] << "flag at pos " << flag_pos << " has no paths!  this might be eligible for *flag* removal but returning false because path cannot be found to destroy a road";
    return false;
  }

  // reject if flag has resources sitting
  Flag *flag = game->get_flag_at_pos(flag_pos);
  if (flag != nullptr && flag->has_resources()){
    AILogDebug["util_flag_and_road_suitable_for_removal"] << "flag at pos " << flag_pos << " has resources waiting, not eligible for removal (yet)";
    return false;
  }

  // reject if the road has any transporters carrying resources
  Direction tmp_dir = *(road_dir);
  MapPos pos = flag_pos;
  while (true) {
    // trace the road until it ends, looking for active
    //  transporters at each pos, and see if they have a resource
    Serf *serf_on_path = game->get_serf_at_pos(pos);
    if (serf_on_path != nullptr) {
      if (serf_on_path->get_type() == Serf::TypeTransporter) {
        if (serf_on_path->get_delivery() > Resource::TypeNone){
          AILogDebug["util_flag_and_road_suitable_for_removal"] << "flag at pos " << flag_pos << " has a transporting serf at pos " << pos << " carrying a resource, not eligible for removal (yet)";
          return false;
        }
      }
    }
    // if end of the road reached, break
    if (map->has_flag(pos) && pos != flag->get_position())
      break;  
    // otherwise continue to next pos along the road
    pos = map->move(pos, tmp_dir);
    for (Direction new_dir : cycle_directions_cw()) {
      if (map->has_path_IMPROVED(pos, new_dir) && new_dir != reverse_direction(tmp_dir)) {
        tmp_dir = new_dir;
        break;
      }
    }
  }

  // flag must be eligible if this point reached
  AILogDebug["util_flag_and_road_suitable_for_removal"] << "flag at pos " << flag_pos << " is eligible for removal, road dir is " << *(road_dir) << " / " << NameDirection[*(road_dir)] << ", returning true";
  return true;
}

//
// store the "forbidden zone" to prevent creation of paths blocking the castle or a stock flag
//
void
AI::set_forbidden_pos_around_inventory(MapPos inventory_flag_pos){
  //            22
  //            --\2
  //     castle_   \2
  //  2/\2 /\cc/\   \2 
  // 2/_2\/cc\/cc\   \2
  // 2\--2\cc/\cc/\1  \2
  //  2\  2\/__\/__\1  \2  castle/stock flag area, double lines are forbidden paths
  //   2\  1\  /\  /1  /2            NOTE that ring1 pos can connect to ring2 pos, 
  //    2\  1\/__\/1  /2                   but ring1-to-ring1 and ring2-to-ring2 disallowed
  //     2\   1111   /2
  //      2\________/2
  //        22222222
  //
  // and path segment with both ends in forbidden list must be rejected
  // first ring
  MapPos forbidden_pos = map->move_left(inventory_flag_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring1.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "cyan"));
  forbidden_pos = map->move_down_right(forbidden_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring1.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "cyan"));
  forbidden_pos = map->move_right(forbidden_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring1.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "cyan"));
  forbidden_pos = map->move_up(forbidden_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring1.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "cyan"));
  forbidden_pos = map->move_up_left(forbidden_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring1.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "cyan"));
  // second ring
  forbidden_pos = map->move_up_left(map->move_left(inventory_flag_pos)); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring2.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "dk_cyan"));
  forbidden_pos = map->move_up_left(forbidden_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring2.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "dk_cyan"));
  forbidden_pos = map->move_down(forbidden_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring2.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "dk_cyan"));
  forbidden_pos = map->move_down_right(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "dk_cyan"));
  //forbidden_pos = map->move_down(forbidden_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring2.push_back(forbidden_pos); ai_mark_pos.insert(ColorDot(forbidden_pos, "dk_cyan"));
  forbidden_pos = map->move_down_right(forbidden_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring2.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "dk_cyan"));
  forbidden_pos = map->move_down_right(forbidden_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring2.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "dk_cyan"));
  forbidden_pos = map->move_right(forbidden_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring2.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "dk_cyan"));
  forbidden_pos = map->move_right(forbidden_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring2.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "dk_cyan"));
  forbidden_pos = map->move_up(forbidden_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring2.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "dk_cyan"));
  forbidden_pos = map->move_up(forbidden_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring2.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "dk_cyan"));
  forbidden_pos = map->move_up_left(forbidden_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring2.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "dk_cyan"));
  forbidden_pos = map->move_up_left(forbidden_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring2.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "dk_cyan"));
  forbidden_pos = map->move_left(forbidden_pos); stock_building_counts.at(inventory_flag_pos).forbidden_paths_ring2.push_back(forbidden_pos);// ai_mark_pos.insert(ColorDot(forbidden_pos, "dk_cyan"));
}


// lock and unlock mutex during non-threadsafe
//  iterations and changes between game and AI threads
// this calls mutex_lock, ut is only separate so that
//  it can write to ai log instead of main console log
void
AI::mutex_lock(const char* message){
  AILogVerbose["game.cc"] << "inside AI::mutex_lock, thread #" << std::this_thread::get_id() << " about to lock mutex, message: " << message;
  clock_t start = std::clock();
  game->get_mutex()->lock();
  double wait_for_mutex = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogVerbose["game.cc"] << "inside AI::mutex_lock, thread #" << std::this_thread::get_id() << " has locked mutex, message: " << mutex_message << ", waited " << wait_for_mutex << "sec for lock";
  // store the lock message and start a timer so message and time-in-mutex can be printed on unlock
  mutex_message = message;
  mutex_timer_start = std::clock();
}

void
AI::mutex_unlock(){
  // message is known from lock
  //Log::Error["game.cc"] << "inside Game::mutex_unlock, thread #" << std::this_thread::get_id() << " about to unlock mutex, message: " << mutex_message;
  double time_in_mutex = (std::clock() - mutex_timer_start) / static_cast<double>(CLOCKS_PER_SEC);
  game->get_mutex()->unlock();
  AILogVerbose["game.cc"] << "inside AI::mutex_unlock, thread #" << std::this_thread::get_id() << " has unlocked mutex, message: " << mutex_message << ", spent " << time_in_mutex << "sec holding lock";
}
