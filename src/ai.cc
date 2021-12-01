/*
 * ai.cc - AI main class
 *   Copyright 2019-2021 tlongstretch
 */

#include <algorithm>  // to satisfy cpplint

#include "src/ai.h"
#include "src/game-options.h"

AI::AI(PGame current_game, unsigned int _player_index) {

  ai_status = "INITIALIZING";
  player_index = _player_index;
  name = "Player" + std::to_string(player_index);

  AILogInfo["init"] << name << " inside AI::AI constructor with player_index: " << player_index;
  AILogVerbose["init"] << name << " AI log level is at least verbose";
  AILogDebug["init"] << name << " AI log level is at least debug";
  AILogInfo["init"] << name << " AI log level is at least info";
  AILogWarn["init"] << name << " AI log level is at least warn";
  AILogError["init"] << name << " AI log level is at least error";

  game = current_game;
  map = game->get_map();
  player = game->get_player(player_index);
  // for "build something" functions that return a MapPos of where built, stopbuilding_pos is a flag that can be returned that says to quit trying to build that thing
  stopbuilding_pos = std::numeric_limits<unsigned int>::max() - 2;
  loop_count = 0;
  castle = nullptr;
  stock_inv = nullptr;
  castle_pos = bad_map_pos;
  castle_flag_pos = bad_map_pos;
  inventory_pos = bad_map_pos;
  serfs_idle = {};
  serfs_potential = {};
  serfs_total = {};
  unfinished_building_count = 0;
  unfinished_hut_count = 0;
  expand_towards = {};
  last_expand_towards = {};
  no_transporter_timers = {};
  serf_wait_timers = {};
  serf_wait_idle_on_road_timers = {};
  realm_occupied_military_pos = {};
  bad_building_pos = {};   // AI STATEFULNESS WARNING - this bad_building_pos list is lost on save/load or if AI thread terminated
  stocks_pos = {};
  realm_inv = {};
  stock_buildings = {};
  stock_res_sitting_at_flags = {};
  realm_res_sitting_at_flags = {};
  scoring_attack = false;
  scoring_warehouse = false;
  cannot_expand_borders_this_loop = false;
  change_buffer = 0;
  previous_knight_occupation_level = -1;

  road_options.reset(RoadOption::Direct);
  road_options.set(RoadOption::SplitRoads);
  road_options.set(RoadOption::PenalizeNewLength);
  road_options.set(RoadOption::PenalizeCastleFlag);
  road_options.reset(RoadOption::AvoidCastleArea);
  road_options.reset(RoadOption::Improve);
  road_options.reset(RoadOption::ReducedNewLengthPenalty);
  road_options.set(RoadOption::AllowWaterRoad);
  road_options.reset(RoadOption::HoldBuildingPos);
  road_options.reset(RoadOption::MostlyStraight);

  need_tools = false;

  // this seems to result in bogus value - 32??
  AILogDebug["init"] << name << " setting initial knight garrison levels to minimum";
  // change this to only happen on NEW game!  not on save game load
  player->change_knight_occupation(0, 0, -5);
  player->change_knight_occupation(0, 1, -5);
  player->change_knight_occupation(1, 0, -5);
  player->change_knight_occupation(1, 1, -5);
  player->change_knight_occupation(2, 0, -5);
  player->change_knight_occupation(2, 1, -5);
  player->change_knight_occupation(3, 0, -5);
  player->change_knight_occupation(3, 1, -5);
}

void
AI::start() {
  AILogInfo["start"] << name << " AI is starting, thread_id: " << std::this_thread::get_id();

  while (true) {
    //AILogDebug["start"] << name << " start AI::start while(true)";
    bool logged_paused = false;
    if (game->should_ai_stop() == true) {
      AILogInfo["start"] << name << " received stop_ai_threads signal, exiting!";
      game->ai_thread_exiting();
      return;
    }
    else if (game->get_game_speed() == 0) {
      // avoid repeat log messages when paused
      if (!logged_paused){
        AILogDebug["start"] << name << " game is paused, not running AI loops until unpaused";
        logged_paused = true;
      }
      ai_status.assign("AI_PAUSED");
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    else if (game->is_ai_locked()) {
      AILogDebug["start"] << name << " AI is still locked, sleeping until game->unlock_ai called (when game init_box is closed)";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    else {
      next_loop();
      //AILogDebug["start"] << name << " done next_loop()";
    }
    //AILogDebug["start"] << name << " end AI::start while(true)";
  }
}

void
AI::next_loop(){
  AILogDebug["next_loop"] << name << " inside AI::next_loop()";
  loop_count++;

  ai_status.assign("SLEEPING_AT_START");
  AILogDebug["next_loop"] << name << " sleeping 6sec at start of new loop";
  std::this_thread::sleep_for(std::chrono::milliseconds(6000));

  AILogInfo["next_loop"] << name << " starting AI loop #" << loop_count;
  // time entire loop
  std::clock_t loop_clock_start;
  double loop_clock_duration;
  loop_clock_start = std::clock();

  do_place_castle();
  do_update_clear_reset();
  update_stocks_pos();
  inventory_pos = castle_flag_pos;  // need to set this so various functions work on the very first AI loop, before the loop over Inventories starts
  update_building_counts();
  do_get_inventory(castle_flag_pos);

  // this is broken since messing with flagsearch, see details here:  https://github.com/forkserf/forkserf/issues/70
  //DEBUG
  //if (realm_building_count[Building::TypeHut] > 1){
  //  AI::identify_arterial_roads(map);
  //  //return;
  //}

/*
  //DEBUG
  MapPosVector corners = AI::get_corners(castle_pos);
  std::map<int,MapPosVector> spots;
  int dir = -1;
  for (MapPos corner_pos : corners) {
    dir++;
    spots[dir] = {};
    for (unsigned int i = 0; i < AI::spiral_dist(4); i++) {
      MapPos spot_pos = map->pos_add_extended_spirally(corner_pos, i);
      ai_mark_pos.insert(ColorDot(spot_pos, "black"));
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      spots.at(dir).push_back(spot_pos);
    }
  }
  MapPosVector bigspiral = {};
  std::map<int,MapPosVector> matches;
  for (unsigned int i = AI::spiral_dist(8) - 1; i > 0; i--) {
    //MapPos ring_pos = map->pos_add_extended_reverse_spirally(inventory_pos, i);
    MapPos ring_pos = map->pos_add_extended_spirally(castle_pos, i);
    ai_mark_pos.erase(ring_pos);
    ai_mark_pos.insert(ColorDot(ring_pos, "lt_orange"));
    bigspiral.push_back(ring_pos);
    for (Direction dir : cycle_directions_cw()){
      if (std::find(spots[dir].begin(),spots[dir].end(),ring_pos) != spots[dir].end()){
        matches[dir].push_back(ring_pos);
        ai_mark_pos.erase(ring_pos);
        ai_mark_pos.insert(ColorDot(ring_pos, get_dir_color_name(dir)));
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  ai_mark_pos.clear();
  for (Direction dir : cycle_directions_cw()){
    for (MapPos pos : matches[dir]){
      ai_mark_pos.erase(pos);
      ai_mark_pos.insert(ColorDot(pos, get_dir_color_name(dir)));
      std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    ai_mark_pos.clear();
  }

  //======= copied from map.h =====================
  //// Extract col and row from MapPos
  //int pos_col(MapPos pos) const { return geom_.pos_col(pos); }
  //int pos_row(MapPos pos) const { return geom_.pos_row(pos); }
  //
  //// Translate col, row coordinate to MapPos value.
  //MapPos pos(int x, int y) const { return geom_.pos(x, y); }
  ================================================= 

  // make this a nested map that includes not just each direction
  //  but also each corner distance from center.  
  // Right now it is hardcoded to 4 pos away (it uses get_corners)
  std::map<int,MapPosVector> directional_fill;
  ai_mark_pos.clear();
  dir = -1;
  for (MapPos corner_pos : corners) {
    dir++;
    AILogInfo["spiral"] << " corner_pos " << corner_pos << " is in dir " << NameDirection[dir] << " / " << dir;
    int corner_col = map->pos_col(corner_pos);
    int corner_row = map->pos_row(corner_pos);
    //for (unsigned int i = 0; i < AI::spiral_dist(4); i++) {
    for (MapPos pos : matches.at(dir)){
      //MapPos pos = map->pos_add_extended_spirally(corner_pos, i);
      int col_offset = corner_col - map->pos_col(pos);
      int row_offset = corner_row - map->pos_row(pos);
      AILogInfo["spiral"] << " pos " << pos << " has offset " << col_offset << ", " << row_offset << " from corner_pos " << corner_pos;
      directional_fill[dir].push_back(col_offset);
      directional_fill[dir].push_back(row_offset);
    }
  }
  */

/* this works
  MapPosVector corners = AI::get_corners(castle_pos);
  //int dir = 2;
  int dir = -1;
  for (MapPos corner_pos : corners) {
    dir++;
    //if (dir == 6){
    //  dir = 0;
    //}
    //int *foo = map->get_directional_fill_pattern();
    //for (int i = 0; i < 104; i++){
      //int dir_offset = 104 * dir;
      //int x = foo[dir_offset + i];
      //int y = foo[dir_offset + i + 1];
      //int x = foo[i];
      //int y = foo[i+1];
      //i++;
      //AILogInfo["spiral"] << "x,y = " << x << ", " << y;
      //MapPos pos = map->pos_add(corner_pos,x,y);
      // I don't understand why the -9 is required, I had it working okay when I wasn't
      //  using pos_add_directional_fill but was instead directly calculating the offsets
      // from the fill pattern.  I guess either leave it this way, figure out why, or
      //  just use the fill pattern directly instead of the pos_pattern
    //for (unsigned int i = 0; i < (AI::spiral_dist(4) - 9); i++) {
    for (unsigned int i = 0; i < DIRECTIONAL_FILL_POS_MAX; i++) {
      MapPos pos = map->pos_add_directional_fill(corner_pos, i, dir);
      ai_mark_pos.erase(pos);
      ai_mark_pos.insert(ColorDot(pos, get_dir_color_name(Direction(dir))));
      std::this_thread::sleep_for(std::chrono::milliseconds(75));
    }
  }
  return;
  */
  //DEBUG

  do_get_serfs();
  //do_debug_building_triggers();   // not using these right now

  //-----------------------------------------------------------
  // housekeeping tasks
  //  NOTE - all of these should probably be moved to run AFTER the main game loop.
  //    for example, we shouldn't send geologists out as the very first action when starting a game,
  //      rather it should be done after first few buildings/roads placed
  //-----------------------------------------------------------

  do_connect_disconnected_flags(); // except unfinished mines
  do_build_better_roads_for_important_buildings();  // is this working?  I still see pretty inefficient roads for important buildings
  //do_spiderweb_roads();  // moved to inside warehouse/stock so it does this for each one
  //do_pollute_castle_area_roads_with_flags(); // CHANGE THIS TO USE ARTERIAL ROADS 
  do_fix_stuck_serfs();  // this is definitely still an issue, try to fix root cause
  do_fix_missing_transporters();  // is this still a problem anymore??
  //do_send_geologists();  move this to inside warehouse / stock loop only because it uses occupied_building_count which uses inventory_pos
  do_build_rangers();
  do_remove_road_stubs();
  do_demolish_unproductive_3rd_lumberjacks();
  do_demolish_unproductive_stonecutters();
  do_demolish_unproductive_mines();
  do_manage_tool_priorities();
  do_manage_mine_food_priorities();
  do_balance_sword_shield_priorities();
  do_attack();
  do_manage_knight_occupation_levels();

  // rename this to Inventories instead of Stocks
  update_stocks_pos();
  for (MapPos this_inventory_pos : stocks_pos) {
    inventory_pos = this_inventory_pos;
    AILogDebug["next_loop"] << name << " Starting economy loop for stock at pos " << inventory_pos;

    // debug
    AILogDebug["next_loop"] << name << " stock at pos " << inventory_pos << " has all/completed/occupied buildings: ";
    for (int x = 0; x < 25; x++) {
      AILogDebug["next_loop"] << name << " type " << x << " / " << NameBuilding[x] << ": " << stock_buildings.at(inventory_pos).count[x]
        << "/" << stock_buildings.at(inventory_pos).completed_count[x] << "/" << stock_buildings.at(inventory_pos).occupied_count[x];
    }

    do_get_inventory(inventory_pos);
    do_count_resources_sitting_at_flags(inventory_pos);
    do_promote_serfs_to_knights();
    do_demolish_excess_lumberjacks();
	  do_demolish_excess_food_buildings();
    do_send_geologists();

    do_spiderweb_roads();


    // PLACE MINES EARLY - but do not connect them to roads so they do not actually get built until later
    //   this is to secure good placement when resources are found, before the signs fade
    do_place_coal_mines();
    do_place_iron_mines();
    do_place_gold_mines();


    do_build_sawmill_lumberjacks();
    // really need to change stocks_pos to be the pos of the stock itself and not the flag
    //   it should be simple to do, need to test a bunch after to make sure nothing breaks
    //if (inventory_pos == castle_pos && do_wait_until_sawmill_lumberjacks_built() == false)
    if (map->move_up_left(inventory_pos) == castle_pos && do_wait_until_sawmill_lumberjacks_built() == false)
      return;
    unsigned int planks_count = realm_inv[Resource::TypePlank];
    if (planks_count < planks_crit) {
      AILogDebug["next_loop"] << name << " planks below crit, ending loop early";
      return;
    }

    do_build_stonecutter();

    // is this the right place for this?  ... yes I think so..
    //  but it should be limited to buffer around CASTLE only to avoid enemy encroaching critical castle roads area
    //   and another defensive buffer scoring should be kept to defend the rest of the territory/buildings
    do_create_defensive_buffer();

    do_build_toolmaker_steelsmelter();

    do_build_food_buildings_and_3rd_lumberjack();

    do_connect_coal_mines();
    do_connect_iron_mines();
    do_build_steelsmelter();
    do_build_blacksmith();

    do_build_gold_smelter_and_connect_gold_mines();

    AILogDebug["next_loop"] << name << " Done with economy loop for stock at pos " << inventory_pos;
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  }

  // create parallel infrastructure!
  do_build_warehouse();  // disabling this for now to debug core economy issues

  //ai_mark_pos.clear();
  AILogInfo["next_loop"] << name << " Done AI Loop #" << loop_count;
  ai_status.assign("END OF LOOP");
  AILogDebug["next_loop"] << name << " loop complete, sleeping 2sec";
  loop_clock_duration = (std::clock() - loop_clock_start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["next_loop"] << name << " done next_loop, call took " << loop_clock_duration;
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}







void
AI::do_place_castle() {
  AILogDebug["do_place_castle"] << name << " inside do_place_castle()";
  ai_status.assign("HAS_CASTLE_CHECK");
  if (!player->has_castle()) {
    AILogDebug["do_place_castle"] << name << " does not yet have a castle";
    // place castle
    //   improve this so that it is more intelligent about other resources than trees/stones/building_sites
    //    but have the minimum scores reduced a bit for each area scored so it eventually settles on something
    //
    // pick random spots on the map until an acceptable area found, and try building there
    //
    // IMPORTANT - with multiple AI threads all starting at the same time, they are trying to build castle at the same time,
    //    this Random rnd will actually be IDENTICAL across all threads!  To mix them up, sleep a bit so that
    //      the time that is fed to seed the random function gets a different time-seed for each player
    // changed this to mutex.lock() instead, but keeping this because... I dunno it seems nicer when they don't all start at exactly the same time
    //   maybe start doing random wait instead?  meh
    AILogDebug["do_place_castle"] << name << " sleeping " << player_index << "sec so each AI player thread gets different seed for random map pos";
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 * player_index));
    Random rnd;
    int tries = 250; // I saw 200 tries reached once, not sure if it was a particular map or what
    int x = 0;
    while (true) {
      x++;
      if (x > tries) {
        AILogDebug["do_place_castle"] << name << " unable to place castle after " << x << " tries!";
        exit(1);
      }
      MapPos pos = map->get_rnd_coord(NULL, NULL, &rnd);
      AILogDebug["do_place_castle"] << name << ": considering placing castle at random pos " << pos;
      if (!game->can_build_castle(pos, player)) {
        AILogDebug["do_place_castle"] << name << " cannot build a castle at pos " << pos;
        continue;
      }
      if (place_castle(game, pos, spiral_dist(8))) {
        AILogDebug["do_place_castle"] << name << " found acceptable place to build castle, at pos: " << pos;
        AILogDebug["do_place_castle"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->build_castle";
        game->get_mutex()->lock();
        AILogDebug["do_place_castle"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->build_castle";
        bool was_built = game->build_castle(pos, player);
        AILogDebug["do_place_castle"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->build_castle";
        game->get_mutex()->unlock();
        AILogDebug["do_place_castle"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->build_castle";
        if (was_built) {
          AILogDebug["do_place_castle"] << name << ": built castle at pos: " << pos << " after " << x << " tries";
          castle_pos = pos;
          castle_flag_pos = map->move_down_right(castle_pos);
          AILogDebug["do_place_castle"] << name << " castle has position " << castle_pos << ", with castle_flag_pos " << castle_flag_pos;
          return;
        }
        AILogDebug["do_place_castle"] << name << " failed to build castle at pos: " << pos << ", will keep trying";
      }
    }
  }
  // on game load, castle_pos is unknown even though castle exists, need to find it
  if (castle_pos == bad_map_pos) {
    AILogDebug["do_place_castle"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for finding castle on game load)";
    game->get_mutex()->lock();
    AILogDebug["do_place_castle"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for finding castle on game load)";
    Game::ListBuildings buildings = game->get_player_buildings(player);
    AILogDebug["do_place_castle"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player) (for finding castle on game load)";
    game->get_mutex()->unlock();
    AILogDebug["do_place_castle"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player) (for finding castle on game load)";
    for (Building *building : buildings) {
      if (building->get_type() == Building::TypeCastle) {
        castle_pos = building->get_position();
        AILogDebug["do_place_castle"] << name << "'s castle was found at pos " << castle_pos;
        castle_flag_pos = map->move_down_right(castle_pos);
      }
    }
  }
}


void
AI::do_update_clear_reset() {
  AILogDebug["do_update_clear_reset"] << name << " inside do_update_clear_reset";
  ai_status.assign("CLEARING_AND_RESETTING");
  ai_mark_pos.clear();
  ai_mark_serf.clear();
  ai_mark_arterial_road_pairs->clear();
  ai_mark_arterial_road_flags->clear();
  //ai_mark_spiderweb_road_pairs->clear();
  //ai_mark_spiderweb_roads->clear();  //don't clear this, let it accumulate to show them all
  last_expand_towards = expand_towards;
  expand_towards.clear();
  road_options.reset(RoadOption::Direct);
  road_options.set(RoadOption::SplitRoads);
  road_options.set(RoadOption::PenalizeNewLength);
  road_options.set(RoadOption::PenalizeCastleFlag);
  road_options.reset(RoadOption::AvoidCastleArea);
  //road_options.reset(RoadOption::Improve);  // this wasn't here for a long time, did I forget it or was this intentional?
  //road_options.reset(RoadOption::ReducedNewLengthPenalty);  // this wasn't here for a long time, did I forget it or was this intentional?
  //road_options.set(RoadOption::AllowWaterRoad);  // this wasn't here for a long time, did I forget it or was this intentional?
  //road_options.reset(RoadOption::HoldBuildingPos);  // this wasn't here for a long time, did I forget it or was this intentional?
  road_options.reset(RoadOption::MostlyStraight);
  unfinished_hut_count = 0;
  unfinished_building_count = 0;
  realm_inv = player->get_stats_resources();
  scoring_attack = false;
  scoring_warehouse = false;  // this is deprecated right now
  cannot_expand_borders_this_loop = false;

  // does this belong here?  this whole update_clear thing needs re-work
  for (int i = 0; i < 26; i++) {
    realm_res_sitting_at_flags[Resource::Type(i)] = 0;
  }
}


void
AI::do_get_serfs() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();

  AILogDebug["do_get_serfs"] << name << " inside do_get_serfs";
  AILogDebug["do_get_serfs"] << name << " getting serfs";
  AILogDebug["do_get_serfs"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before getting serfs at AI loop start";
  game->get_mutex()->lock();
  AILogDebug["do_get_serfs"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before getting serfs at AI loop start";
  serfs_idle = player->get_stats_serfs_idle();
  serfs_potential = player->get_stats_serfs_potential();
  serfs_total = player->get_serfs();
  AILogDebug["do_get_serfs"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after getting serfs at AI loop start";
  game->get_mutex()->unlock();
  AILogDebug["do_get_serfs"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after getting serfs at AI loop start";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_get_serfs"] << name << " done do_get_serfs call took " << duration;
}


//
// function only used for debugging, never called by AI on its own
//  building certain buildings manually triggers some function
void
AI::do_debug_building_triggers() {
  AILogDebug["do_debug_building_triggers"] << name << " inside do_debug_building_triggers";
  update_building_counts();
  /*
  // DEBUG
  //   trigger demolish/rebuild all roads by placing a Pig Farm anywhere in the realm
  if (realm_building_count[Building::TypePigFarm] > 0) {
    AILogDebug["do_debug_building_triggers"] << name << " PigFarm found, running rebuild_all_roads";
    rebuild_all_roads();
    // then destroy the pig farm so it doesn't keep rebuilding forever
    AILogDebug["do_debug_building_triggers"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before demolishing pig farm";
    game->get_mutex()->lock();
    AILogDebug["do_debug_building_triggers"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before demolishing pig farm";
    Game::ListBuildings buildings = game->get_player_buildings(player);
    for (Building *building : buildings) {
      if (building->get_type() == Building::TypePigFarm)
        game->demolish_building(building->get_position(), player);
    }
    AILogDebug["do_debug_building_triggers"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after demolishing pig farm";
    game->get_mutex()->unlock();
    AILogDebug["do_debug_building_triggers"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after demolishing pig farm";
    // don't resume AI until most serfs have made it back into the castle
    AILogDebug["do_debug_building_triggers"] << name << " done rebuild_all_roads, waiting for lost serfs to clear out";
    for (int x = 0; x < 50; x++) {
      std::this_thread::sleep_for(std::chrono::milliseconds(15000));
      unsigned int lost_serfs = 0;
      for (Serf *serf : game->get_player_serfs(player)) {
        if (serf->get_state() == Serf::StateFreeWalking) {
          lost_serfs++;
        }
      }
      if (lost_serfs <= 5) {
        break;
      }
      AILogDebug["do_debug_building_triggers"] << name << " there are " << lost_serfs << " lost serfs in FreeWalking state, waiting longer";
    }
    return;
  }
  */


  // DEBUG
  //   tell AI to quit by placing a BoatBuilder anywhere in the realm
  if (realm_building_count[Building::TypeBoatbuilder] > 0) {
    AILogDebug["do_debug_building_triggers"] << name << " BoatBuilder found, locking all AIs";
    game->lock_ai();
    return;
  }
}

// to be 100% "fair" this should use the global "promote all possible serfs to knights"
//  function that human players can click on, but because there is no advantage gained
//  by doing it per-stock I will leave this as it is for now.
void
AI::do_promote_serfs_to_knights() {
  AILogDebug["do_promote_serfs_to_knights"] << name << " inside do_promote_serfs_to_knights";
  ai_status.assign("HOUSEKEEPING - promote serfs to knights");
  unsigned int idle_serfs = static_cast<unsigned int>(stock_inv->free_serf_count());
  unsigned int swords_count = stock_inv->get_count_of(Resource::TypeSword);
  unsigned int shields_count = stock_inv->get_count_of(Resource::TypeShield);
  unsigned int idle_knights = serfs_idle[Serf::TypeKnight0] + serfs_idle[Serf::TypeKnight1] + serfs_idle[Serf::TypeKnight2] + serfs_idle[Serf::TypeKnight3] + serfs_idle[Serf::TypeKnight4];
  unsigned int excess_serfs = idle_serfs < serfs_min ? 0 : idle_serfs - serfs_min;
  unsigned int promotable = std::min(excess_serfs, std::min(swords_count, shields_count));
  AILogDebug["do_promote_serfs_to_knights"] << name << " idle_knights: " << idle_knights << ", idle_serfs: " << idle_serfs << ", serfs_min: " << serfs_min << ", excess serfs: " << excess_serfs << ", swords: " << swords_count << ", shields: " << shields_count << ", promotable: " << promotable;
  AILogDebug["do_promote_serfs_to_knights"] << name << " promoting serfs to knights: " << promotable;
  // this 'promotable' function doesn't seem to work right, it doesn't limit the number promoted to the specified integer like it suggests
  //int promoted = player->promote_serfs_to_knights(promotable);
  int promoted = 0;
  AILogDebug["do_promote_serfs_to_knights"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_serfs(player) (for do_manage_knight_occupation_levels is_waiting)";
  game->get_mutex()->lock();
  AILogDebug["do_promote_serfs_to_knights"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_serfs(player) (for do_manage_knight_occupation_levels is_waiting)";
  // this returns a copy, so it should be thread-safe
  //  maybe not, beause game->get_player_serfs internally just does for (Serf *serf : serfs)
  for (Serf *serf : game->get_player_serfs(player)) {
    if (promotable < 1) { break; }
    if (serf->get_state() == Serf::StateIdleInStock &&
      serf->get_type() == Serf::TypeGeneric) {
      Inventory *inv = game->get_inventory(serf->get_idle_in_stock_inv_index());
      if (inv->promote_serf_to_knight(serf)) {
        promoted++;
        promotable--;
      }
    }
  }
  AILogDebug["do_promote_serfs_to_knights"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_serfs(player) (for do_manage_knight_occupation_levels is_waiting)";
  game->get_mutex()->unlock();
  AILogDebug["do_promote_serfs_to_knights"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_serfs(player) (for do_manage_knight_occupation_levels is_waiting)";

}

void
AI::do_connect_disconnected_flags() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  AILogDebug["do_connect_disconnected_flags"] << name << " inside do_connect_disconnected_flags";
  AILogDebug["do_connect_disconnected_flags"] << name << " HouseKeeping: connect any disconnected flags";
  ai_status.assign("HOUSEKEEPING - connect any disconnected flags");
  flags_static_copy = *(game->get_flags());
  flags = &flags_static_copy;
  for (Flag *flag : *flags) {
    if (flag->get_owner() != player_index)
      continue;
    if (flag->get_position() == castle_flag_pos)
      continue;
    if (flag->is_connected())
      continue;
    if (flag->has_building()) {
      AILogDebug["do_connect_disconnected_flags"] << name << " flag at pos " << flag->get_position() << " has an attached building of type " << NameBuilding[flag->get_building()->get_type()];
      if ((flag->get_building()->get_type() == Building::TypeCoalMine
        || flag->get_building()->get_type() == Building::TypeIronMine
        || flag->get_building()->get_type() == Building::TypeGoldMine
        || flag->get_building()->get_type() == Building::TypeStoneMine)
        && !flag->get_building()->is_done()) {
        AILogDebug["do_connect_disconnected_flags"] << name << " disconnected flag at pos " << flag->get_position() << " is attached to an incomplete Mine, skipping - it will be dealt with later";
        continue;
      }
      if (flag->get_building()->get_type() == Building::TypeForester &&
        flag->get_building()->is_done() && flag->get_building()->has_serf()){
        AILogDebug["do_connect_disconnected_flags"] << name << " disconnected flag at pos " << flag->get_position() << " is attached to an occupied ranger, skipping";
        continue;
      }
    }
    AILogDebug["do_connect_disconnected_flags"] << name << " flag at pos " << flag->get_position() << " has no connected road, trying to connect it";
    Road notused; // not used here, can I just pass a zero instead of &notused to build_best_road and skip initialization of a wasted object?
    bool was_built = AI::build_best_road(flag->get_position(), road_options, &notused);
    if (!was_built) {
      AILogDebug["do_connect_disconnected_flags"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling demolish_flag (and maybe attached building)";
      game->get_mutex()->lock();
      AILogDebug["do_connect_disconnected_flags"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling demolish_flag (and maybe attached building)";
      // should I look for an attached building an burn it?
      // yes!  let's try that
      if (flag->has_building()) {
        AILogDebug["do_connect_disconnected_flags"] << name << " failed to connect disconnected flag to road network!  BURNING ATTACHED BUILDING!";
		// how did this ever work before??
        //game->demolish_building(flag->get_position(), player);
		game->demolish_building(map->move_up_left(flag->get_position()), player);
      }
      AILogDebug["do_connect_disconnected_flags"] << name << " failed to connect disconnected flag to road network!  removing it";
      game->demolish_flag(flag->get_position(), player);
      AILogDebug["do_connect_disconnected_flags"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling demolish_flag (and maybe attached building)";
      game->get_mutex()->unlock();
      AILogDebug["do_connect_disconnected_flags"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling demolish_flag (and maybe attached building)";
    }
  }

  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_connect_disconnected_flags"] << name << " done do_connect_disconnected_flags call took " << duration;
}


void
AI::do_spiderweb_roads() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();

  AILogDebug["do_spiderweb_roads"] << inventory_pos << " inside do_spiderweb_roads";
  ai_status.assign("HOUSEKEEPING - do_spiderweb_roads");
  // "spider-web" roads - because a "star network" pattern naturally emerges with castle at center, try to
  //   convert it to a "spider-web" shape by attemping build roads between any flags within a band a bit outside
  //    the castle area
  //
  //  trace a hex-circle 18 tiles out from castle,
  //     at each corner draw a size8 circle and connect any flags within it
  //  this could be improved by making a "caret" or "flattened hexagon" shape at corners instead of spiral_pos
  //    -  tried doing this, seemed like a waste of time.  Circle is fine
  //
  AILogDebug["do_spiderweb_roads"] << inventory_pos << " HouseKeeping: creating spider-web roads";
  // only do this every X loops, and only add one new road per run
  update_building_counts();
  unsigned int completed_huts = realm_completed_building_count[Building::TypeHut];
  //if (loop_count % 10 != 0 || completed_huts < 9 || completed_huts > 16) {
    // temporary run ever other loop for debugging
  //if (loop_count % 2 != 0 || completed_huts < 9 || completed_huts > 16) {
  //AILogDebug["do_spiderweb_roads"] << inventory_pos << " DEBUG: loop_count " << loop_count << ", desired freq 10, adjusted freq " << ai_loop_freq_adj_for_gamespeed(10) << ", completed_huts " << completed_huts;
  //if (loop_count % ai_loop_freq_adj_for_gamespeed(10) != 0) {
  //  AILogDebug["do_spiderweb_roads"] << inventory_pos << " skipping spider-web roads, only running this every ten loops";
  //  return;
  //}
  //if ( completed_huts < 9 || completed_huts > 16) {
    //AILogDebug["do_spiderweb_roads"] << inventory_pos << " skipping spider-web roads, knight huts built " << completed_huts << " is <9 or >16";
  if ( completed_huts < 11 || completed_huts > 25) {
    AILogDebug["do_spiderweb_roads"] << inventory_pos << " skipping spider-web roads, knight huts built " << completed_huts << " is <11 or >25";
    return;
  }

  std::set<MapPos> tried_pairs;
  unsigned int spider_web_roads_built = 0;
  // follow the hex-circle path around the castle/stock center pos 18 tiles out
  //   and every X tiles record the nearby flags.
  // while doing this, build a list of all the flags found in the most recent X area searches
  //   and at each new sector check random flag pairs in the past
  //
  //   "the past 9 sectors"  was chosen as it represents an arc (fraction of circle)
  //   36 MapPos long (9*4 because only ever 4 tiles is a sector) 
  //   it is about 1/3 of the circle
  //   that contains flags that are "reasonably close together" and so
  //   can be considered for spider-web road connection.  If a larger arc were
  //   to be used, it would result in more distant connections, and if unlimited would 
  //   result in connections straight across the center pos.  If a smaller arc were used
  //   it would not succeed in making as many new spiderweb connections as desired 
  MapPosVector flag_lists[9] = { }; //array of vectors
  for (unsigned int i = AI::spiral_dist(14); i < AI::spiral_dist(15); i++) {
    MapPos ring_pos = map->pos_add_extended_spirally(inventory_pos, i);
    ai_mark_pos.erase(ring_pos);
    ai_mark_pos.insert(ColorDot(ring_pos, "dk_coral"));
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    // only every X positions or on last spot in ring
    if (i % 4 != 0 || i == AI::spiral_dist(15) - 1 )
      continue;
    //AILogDebug["do_spiderweb_roads"] << inventory_pos << " spiderweb doing stuff";

    // every X pos in the hexagonal "ring"/perimeter, do an area search for flags
    MapPosVector flag_list = { };
    for (unsigned int x = 0; x < AI::spiral_dist(4); x++) {
      MapPos area_pos = map->pos_add_extended_spirally(ring_pos, x);
      ai_mark_pos.erase(area_pos);
      ai_mark_pos.insert(ColorDot(area_pos, "lavender"));
      std::this_thread::sleep_for(std::chrono::milliseconds(11));
      if (!map->has_flag(area_pos) || map->get_owner(area_pos) != player_index)
        continue;
      if (!game->get_flag_at_pos(area_pos)->is_connected())
        continue;
      ai_mark_pos.erase(area_pos);
      ai_mark_pos.insert(ColorDot(area_pos, "red"));
      std::this_thread::sleep_for(std::chrono::milliseconds(11));
      flag_list.push_back(area_pos);
    }
    // shift all the flag_list vectors back, dropping the oldest one
    //  and push the newest one to the front of the array
    for (int y = 1; y < 9; y++) {
      flag_lists[y - 1] = flag_lists[y];
    }
    flag_lists[8] = flag_list;

    // insert all the recent vectors into a set, to remove duplicates
    std::set<MapPos> flag_set;
    for (MapPosVector vector : flag_lists){
      //AILogDebug["do_spiderweb_roads"] << inventory_pos << " flag_list has elements: " << vector.size();
      for (MapPos flag_pos : vector) {
        //AILogDebug["do_spiderweb_roads"] << inventory_pos << " flag_list contains: " << flag_pos;
        flag_set.insert(flag_pos);
      }
    }

    // shuffle the ordering of the flags

    //  can't shuffle a std::set
    //AILogDebug["do_spiderweb_roads"] << name << " shuffling the flag_set so it is in random order";
    //std::random_shuffle(flag_set.begin(), flag_set.end());
    // instead convert to vector, shuffling seems important   oct22 2020
    MapPosVector shuffled_flag_vector(flag_set.begin(), flag_set.end());
    std::random_shuffle(shuffled_flag_vector.begin(), shuffled_flag_vector.end());

    //
    // loop over random pairs of flags (that were found in the last 8 sectors)
    //  and consider building a road between them, and if they look good, do so
    //
    for (MapPos area_flag_pos : shuffled_flag_vector) {
      if (spider_web_roads_built > 0) { break; }  // only create one road per run
      AILogDebug["do_spiderweb_roads"] << inventory_pos << " considering roads from area_flag_pos " << area_flag_pos;
      for (MapPos other_area_flag_pos : shuffled_flag_vector) {
        if (area_flag_pos == other_area_flag_pos) { continue; }
        AILogDebug["do_spiderweb_roads"] << inventory_pos << " considering roads from area_flag_pos " << area_flag_pos << " to other_area_flag_pos " << other_area_flag_pos;

        // bitwise operator... to simplify checking for already tried combinations in either order
        unsigned int pair = area_flag_pos & other_area_flag_pos;
        if (tried_pairs.count(pair) > 0) { continue; }
        tried_pairs.insert(pair);

        // reject the pair unless the current best-path between these two flags contains the the currently 
        //  selected Inventory (castle/stock) flag.  If if it not, there is some other path such
        //  as a natural or spider-web road that does not bottleneck at the castle/stock and so
        //  does not need obvious improvement. 
        //
        //  IDEA - instead or in addition to above, compare the flag-dist of the current solution and only
        //   try if it is a certain length, or possibly update build_best_road function to set a minimum 
        //   improvement threshold or it will not actually build it.
        //

        MapPosVector flags_found = {};
        if(NEW_find_flag_and_tile_dist(map, player->get_index(), area_flag_pos, other_area_flag_pos, &flags_found, &ai_mark_pos)){
          AILogDebug["do_spiderweb_roads"] << inventory_pos << " flags_found between " << area_flag_pos << " and " << other_area_flag_pos;
          bool has_inventory_flag = false;
          for (MapPos pos : flags_found){
            AILogDebug["do_spiderweb_roads"] << inventory_pos << " flags_found contains: " << pos;
            if (pos == inventory_pos){
              AILogDebug["do_spiderweb_roads"] << inventory_pos << " existing flag-path from flags_found contains the inventory_pos " << inventory_pos << " = " << pos;
              has_inventory_flag = true;
            }
          }
          if (!has_inventory_flag){
            AILogDebug["do_spiderweb_roads"] << inventory_pos << " flags_found does not contain the inventory_pos " << inventory_pos << ", skipping this pair";
            continue;
          }
        }

        //AILogDebug["do_spiderweb_roads"] << inventory_pos << " still considering";
        ai_mark_pos.clear();
        ai_mark_pos.insert(ColorDot(area_flag_pos, "green"));
        ai_mark_pos.insert(ColorDot(other_area_flag_pos, "red"));
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

        //
        // try to build the spiderweb road
        //
        
        //
        // issue - if Improve is set and Direct off, often stubby useless roads are built that do not
        //  accomplish the spider-web goal.  I am not exactly sure why but it is probably a result of the "best road" logic
        //       - if Direct is set and Improve off, often long snakey roads are built parallel to each other
        // UPDATE - now if I turn of Direct and turn on Improve it doesn't build anything ever, not sure why
        //
        road_options.set(RoadOption::Improve);
        road_options.reset(RoadOption::PenalizeNewLength);
        //road_options.set(RoadOption::Direct);
        road_options.set(RoadOption::MostlyStraight);
        AILogDebug["do_spiderweb_roads"] << inventory_pos << " about to call build_best_road";
        Road built_road;
        //AILogDebug["do_spiderweb_roads"] << inventory_pos << " spiderweb debug1 built_road has memory addr " << &built_road << " and source " << built_road.get_source();
        bool was_built = build_best_road(area_flag_pos, road_options, &built_road, Building::TypeNone, Building::TypeNone, other_area_flag_pos);
        road_options.reset(RoadOption::Improve);
        road_options.set(RoadOption::PenalizeNewLength);
        //road_options.reset(RoadOption::Direct);
        road_options.reset(RoadOption::MostlyStraight);
        if (was_built) {
          AILogDebug["do_spiderweb_roads"] << inventory_pos << " successfully built spider-web road between area_flag_pos " << area_flag_pos << " to other_area_flag_pos " << other_area_flag_pos;
          spider_web_roads_built++;
          //AILogDebug["do_spiderweb_roads"] << inventory_pos << " spiderweb debug2 built_road has memory addr " << &built_road << " and source " << built_road.get_source();
          ai_mark_spiderweb_roads->push_back(built_road);
          std::this_thread::sleep_for(std::chrono::milliseconds(2000));
          // only create one road per run
          break;
        }
        else {
          AILogDebug["do_spiderweb_roads"] << inventory_pos << " failed to build spider-web road between area_flag_pos " << area_flag_pos << " to other_area_flag_pos " << other_area_flag_pos;
          std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        }
      }
    }
  }

  AILogDebug["do_spiderweb_roads"] << name << " sanity check: there were " << tried_pairs.size() << " tried pairs";
  AILogDebug["do_spiderweb_roads"] << inventory_pos << " done with building spider-web roads";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_spiderweb_roads"] << inventory_pos << " done with building spider-web roads call took " << duration;
}

// after the game has progressed a bit, add a bunch of flags to the roads immediately surrounding the castle
//  to disfavor transport paths that would otherwise route through the castle, encouraging alternate routes
void
AI::do_pollute_castle_area_roads_with_flags() {
  AILogDebug["do_pollute_castle_area_roads_with_flags"] << name << " inside do_pollute_castle_area_roads_with_flags";
  ai_status.assign("HOUSEKEEPING - do_pollute_castle_area_roads_with_flags");
  // only do this every X loops, and only once a certain number of huts have been built
  //  and don't do it again after a few more huts built, because it only ever needs to be done once
  update_building_counts();
  unsigned int completed_huts = realm_completed_building_count[Building::TypeHut];
  if (loop_count % 5 != 0 || completed_huts < 15 || completed_huts >19) {
    AILogDebug["do_pollute_castle_area_roads_with_flags"] << name << " skipping do_pollute_castle_area_roads_with_flags roads, only running this every X loops, and if >Y and <Z knight huts built";
    return;
  }

  unsigned int created_flags = 0;
  for (unsigned int x = 0; x < AI::spiral_dist(8); x++) {
    MapPos pos = map->pos_add_extended_spirally(castle_flag_pos, x);
    if (map->has_any_path(pos)){
      if (game->can_build_flag(pos, player)) {
        AILogDebug["do_pollute_castle_area_roads_with_flags"] << name << " building a pollution flag at pos " << pos;
        AILogDebug["do_pollute_castle_area_roads_with_flags"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->build_flag";
        game->get_mutex()->lock();
        AILogDebug["do_pollute_castle_area_roads_with_flags"] << name << " thread #" << std::this_thread::get_id() << " AI has locking mutex before calling game->build_flag";
        if (game->build_flag(pos, player)) {
          created_flags++;
        }
        AILogDebug["do_pollute_castle_area_roads_with_flags"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->build_flag";
        game->get_mutex()->unlock();
        AILogDebug["do_pollute_castle_area_roads_with_flags"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->build_flag";
      }
    }
  }
  AILogDebug["do_pollute_castle_area_roads_with_flags"] << name << " done do_pollute_castle_area_roads_with_flags, created " << created_flags << " new flags";
}



void
AI::do_fix_stuck_serfs() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();

  AILogDebug["do_fix_stuck_serfs"] << name << " inside do_fix_stuck_serfs";
  //
  // bug workaround - check for stuck serfs on roads
  //    for unknown reasons, sometimes serfs become forever stuck in WAIT_IDLE_ON_PATH state
  //      Detect this state, and "boot" the serfs by changing their state to 'lost' so they return to castle

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // oct28 2020
  //  bug confirmation - when the stuck serf detector boots a serf that is on its way to occupy a building, the serf returns to the castle but the building will never be
  //  occupied!  I saw this early in a game where a fisherman was on way to a new fisherman hut, got stuck, was booted, returned to castle, but never was sent back to hut!
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

  // handle serf_wait_idle_on_path_timer that have already been set
  SerfWaitTimer::iterator serf_wait_idle_on_road_timer;
  for (serf_wait_idle_on_road_timer = serf_wait_idle_on_road_timers.begin(); serf_wait_idle_on_road_timer != serf_wait_idle_on_road_timers.end(); ) {
    unsigned int serf_index = serf_wait_idle_on_road_timer->first;
    Serf *serf = game->get_serf(serf_index);
    if (serf == nullptr) {
      ++serf_wait_idle_on_road_timer;
      continue;
    }
    unsigned int trigger_tick = serf_wait_idle_on_road_timer->second;
    if (serf->get_state() != Serf::StateWaitIdleOnPath) {
      AILogDebug["do_fix_stuck_serfs"] << name << " SerfWaitTimer serf is no longer stuck, skipping";
      AILogDebug["do_fix_stuck_serfs"] << name << " SerfWaitTimer erasing serf_wait_idle_on_road_timer for serf " << serf->get_index() << " at pos " << serf->get_pos();
      // found in C++ example... iterator is set by the result of the erase call on the current iterator pos (which is serf_index)
      serf_wait_idle_on_road_timer = serf_wait_idle_on_road_timers.erase(serf_wait_idle_on_road_timer);
    }
    else {
      if (game->get_tick() > trigger_tick) {
        AILogDebug["do_fix_stuck_serfs"] << name << " SerfWaitTimer triggering serf_wait_idle_on_road_timer for serf " << serf->get_index() << " at pos " << serf->get_pos();

        AILogDebug["do_fix_stuck_serfs"] << name << " SerfWaitTimer detected WAIT_IDLE_ON_PATH STUCK SERF at pos " << serf->get_pos() << ", marking its current pos in lt_purple";
        ai_mark_pos.insert(ColorDot(serf->get_pos(), "lt_purple"));
        // I THINK I FIXED THIS ISSUE FOR GOOD - see https://github.com/freeserf/freeserf/issues/492
        //  changing this to a crash exception in case I am wrong about the fix being 100% effective
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
        AILogDebug["do_fix_stuck_serfs"] << name << " SerfWaitTimer detected WAIT_IDLE_ON_PATH STUCK SERF at pos " << serf->get_pos() << " of type " << NameSerf[serf->get_type()] << ", I THOUGHT I FIXED THIS! crashing.  check to see";
        // still seeing this happen for Transporter serfs... but rare... keep an eye on it
        //throw ExceptionFreeserf("SerfWaitTimer detected WAIT_IDLE_ON_PATH STUCK SERF at pos - I THOUGHT I FIXED THIS");
        // don't erase timer if problem isn't fixed yet, keep checking each AI loop
        AILogDebug["do_fix_stuck_serfs"] << name << " SerfWaitTimer attempting to set serf to lost state, updating marking to purple";
        ai_mark_pos.erase(serf->get_pos());
        ai_mark_pos.insert(ColorDot(serf->get_pos(), "purple"));
        Serf::Type serf_job = serf->get_type();
        // got a nullptr here for flag->get_position, maybe break it up and mutex lock?
        // got another nullptr. trying a slight modification...
        //  haven't seen a nullptr in forever here, think its fixed  (oct28 2020)
        MapPos serf_dest_flag_pos = bad_map_pos;
        Flag *serf_dest_flag = game->get_flag(serf->get_walking_dest());
        if (serf_dest_flag == nullptr) {
          AILogDebug["do_fix_stuck_serfs"] << name << " the flag that is the walking dest of serf about to be booted is nullptr!  serf_dest_flag_pos will remain bad_map_pos";
        }
        else {
          serf_dest_flag_pos = serf_dest_flag->get_position();
        }
        // more info, saw case of a stuck Digger (leveller) that was on way to a construction site, but when stuck his walking_dest flag was a flag somewhere totally different
        //   in the realm and not a place where a Digger was even needed.  Maybe a bad pointer somewhere is causing the walking_dest to be set to someplace invalid and this is
        //     the cause of the stuck serf WAIT_IDLE_ON_PATH issue???
        AILogDebug["do_fix_stuck_serfs"] << name << " about to boot serf with job type: " << serf->get_type() << " " << NameSerf[serf->get_type()] << " with walking_dest/flag_pos " << serf_dest_flag_pos;
        /*
        if (serf_job == Serf::TypeTransporter) {
          AILogDebug["do_fix_stuck_serfs"] << name << " WARNING - a transporter was booted, see if this causes flag at map_pos " << serf_dest_flag_pos << " to be without a transporter!";
        }
        if (serf_job != Serf::TypeTransporter && serf_job != Serf::TypeGeologist && serf_job != Serf::TypeDigger && serf_job != Serf::TypeBuilder
          && serf_job != Serf::TypeKnight0 && serf_job != Serf::TypeKnight1 && serf_job != Serf::TypeKnight2 && serf_job != Serf::TypeKnight3 && serf_job != Serf::TypeKnight4) {
          AILogDebug["do_fix_stuck_serfs"] << name << " WARNING - a building-occupying professional serf was booted, see if this causes building with flag pos " << serf_dest_flag_pos << " to stay forever unoccupied!";
          //std::this_thread::sleep_for(std::chrono::milliseconds(6000));
        }
        if (serf_job == Serf::TypeKnight0 || serf_job == Serf::TypeKnight1 || serf_job == Serf::TypeKnight2 || serf_job == Serf::TypeKnight3 || serf_job == Serf::TypeKnight4) {
          AILogDebug["do_fix_stuck_serfs"] << name << " WARNING - a knight was booted, see if this causes military building with flag pos " << serf_dest_flag_pos << " to have a forever empty slot!";
          //::this_thread::sleep_for(std::chrono::milliseconds(6000));
        }
        */
       /* DONT ACTUALLY SET THEM TO LOST - just observere for now, as I think I fixed the bug
        AILogDebug["do_fix_stuck_serfs"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling serf->set_lost_state (for bug workaround stuck serfs)";
        game->get_mutex()->lock();
        AILogDebug["do_fix_stuck_serfs"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling serf->set_lost_state (for bug workaround stuck serfs)";
        // feb09 2021, try setting them to Walking state instead...
        //   nope, that doesn't work.  find out why Walking crashes.  Back to Lost
        serf->set_lost_state();
        //serf->set_serf_state(Serf::StateWalking);  // this crashes
        AILogDebug["do_fix_stuck_serfs"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after after serf->set_lost_state (for bug workaround stuck serfs)";
        game->get_mutex()->unlock();
        AILogDebug["do_fix_stuck_serfs"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after after serf->set_lost_state (for bug workaround stuck serfs)";
        //game->pause();
        */
      }
      ++serf_wait_idle_on_road_timer;
    }
  }
  AILogDebug["do_fix_stuck_serfs"] << name << " SerfWaitTimer after checking timeouts, there are now " << serf_wait_idle_on_road_timers.size() << " TOTAL serf_wait_idle_on_road_timers set";

  // look for new waiting serfs and set serf_wait_timers
  AILogVerbose["do_fix_stuck_serfs"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_serfs(player) (for serf_wait_timers StateWaitIdleOnPath)";
  game->get_mutex()->lock();
  AILogVerbose["do_fix_stuck_serfs"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_serfs(player) (for serf_wait_timers StateWaitIdleOnPath)";
  // this returns a copy, so it should be thread-safe
  //  maybe not, beause game->get_player_serfs internally just does for (Serf *serf : serfs)
  for (Serf *serf : game->get_player_serfs(player)) {
    if (serf->get_state() == Serf::StateIdleInStock)
      continue;
    if (serf->get_state() == Serf::StateWaitIdleOnPath) {
      // see if a serf_wait_timer already set for this flag & dir
      if (serf_wait_idle_on_road_timers.count(serf->get_index()) == 0) {
        AILogDebug["do_fix_stuck_serfs"] << name << " SerfWaitTimer WAIT_IDLE_ON_PATH DETECTED setting countdown serf_wait_idle_on_road_timer for serf with index " << serf->get_index();
        serf_wait_idle_on_road_timers.insert(std::make_pair(serf->get_index(), game->get_tick() + 10000));
        AILogDebug["do_fix_stuck_serfs"] << name << " SerfWaitTimer WAIT_IDLE_ON_PATH DETECTED marking serf on AI overlay";
        ai_mark_serf.push_back(serf->get_index());
        //std::this_thread::sleep_for(std::chrono::milliseconds(12000));
      }
      else {
        int trigger_ticks = static_cast<int>(serf_wait_idle_on_road_timers.at(serf->get_index()) - game->get_tick());
        AILogDebug["do_fix_stuck_serfs"] << name << " SerfWaitTimer WAIT_IDLE_ON_PATH a serf_wait_idle_on_road_timer is already set for this serf, it will trigger in " << trigger_ticks << " ticks";
      }
    }
  }
  AILogVerbose["do_fix_stuck_serfs"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_serfs(player) (for serf_wait_timers StateWaitIdleOnPath)";
  game->get_mutex()->unlock();
  AILogVerbose["do_fix_stuck_serfs"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_serfs(player) (for serf_wait_timers StateWaitIdleOnPath)";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_fix_stuck_serfs"] << name << " done do_fix_stuck_serfs call took " << duration;
}

// bug workaround - send transporters to paths where they are missing
//   for unknown reasons, sometimes roads have no transporter and cannot move resources, halting building and economy
//     Detect this and force sending a transporter using internal game function
void
AI::do_fix_missing_transporters() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  AILogDebug["do_fix_missing_transporters"] << name << " inside do_fix_missing_transporters";
  ai_status.assign("HOUSEKEEPING - send transporters");
  //
  // first, check any timers already set
  //
  AILogDebug["do_fix_missing_transporters"] << name << " there are " << no_transporter_timers.size() << " TOTAL no_transporter_timers set";
  FlagDirTimer::iterator no_transporter_timer;
  for (no_transporter_timer = no_transporter_timers.begin(); no_transporter_timer != no_transporter_timers.end(); ) {
    std::pair<unsigned int, Direction> flag_dir = no_transporter_timer->first;
    unsigned int flag_index = flag_dir.first;
    Direction dir = flag_dir.second;
    unsigned int trigger_tick = no_transporter_timer->second;
    if (game->get_tick() > trigger_tick) {
      Flag *flag = game->get_flag(flag_index);
      if (flag == nullptr || flag->get_owner() != player_index) {
        ++no_transporter_timer;
        continue;
      }
      AILogDebug["do_fix_missing_transporters"] << name << " triggering timer for flag with pos " << flag->get_position() << ", index " << flag_index << ", dir " << dir << " / " << NameDirection[dir];
      
      if (!flag->has_path(dir)){
        AILogVerbose["do_fix_missing_transporters"] << name << " a path no longer exists for flag with pos " << flag->get_position() << ", index " << flag_index << ", dir " << dir << " / " << NameDirection[dir] << ", erasing timer";
        no_transporter_timer = no_transporter_timers.erase(no_transporter_timer);
      }
      else if (flag->has_transporter(dir)) {
        AILogVerbose["do_fix_missing_transporters"] << name << " looks like a transporter finally arrived, not calling another";
        AILogVerbose["do_fix_missing_transporters"] << name << " erasing timer for flag-dir " << flag_index << "-" << dir << " / " << NameDirection[dir];
        // found in C++ example... iterator is set by the result of the erase call on the current iterator pos (which is serf_index)
        no_transporter_timer = no_transporter_timers.erase(no_transporter_timer);
      }
      else {
        AILogDebug["do_fix_missing_transporters"] << name << " timer detected BUG FOUND - NO TRANSPORTER on road at pos " << game->get_flag(flag_index)->get_position() << " in dir " << NameDirection[dir] << ", marking flag in cyan and dir in dk_cyan";
        ai_mark_pos.insert(ColorDot(flag->get_position(), "cyan"));
        ai_mark_pos.insert(ColorDot(map->move(flag->get_position(), dir), "dk_cyan"));
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        //game->pause();
        AILogDebug["do_fix_missing_transporters"] << name << " timer trying to force call a transporter";
        AILogDebug["do_fix_missing_transporters"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling flag->call_transporter, to work around no-transporter issue";
        game->get_mutex()->lock();
        AILogDebug["do_fix_missing_transporters"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling flag->call_transporter, to work around no-transporter issue";
        // got access violation w/ 2x AIs, even with mutex, look for a foreach loop being invalidated elsewhere
        //Access violation reading location 0xFFFFFFFFFFFFFFFF.
        // oh... I think I just wasn't checking that this player owns the flag!  adding that
        // now getting some other read access violation after Inventory->have serfs... on Load Game testing though, dunno
        AILogDebug["do_fix_missing_transporters"] << name << " NOT calling out missing new transporterZZZ, trying to debug this now";
        bool was_called = false;
        //bool was_called = flag->call_transporter(dir, false);  // hardcoding is_water_path to false because I am seeing weird crash with this checking for Serf::TypeSailor
        AILogDebug["do_fix_missing_transporters"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling flag->call_transporter, to work around no-transporter issue";
        game->get_mutex()->unlock();
        AILogDebug["do_fix_missing_transporters"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling flag->call_transporter, to work around no-transporter issue";
        if (!was_called) {
          AILogDebug["do_fix_missing_transporters"] << name << " WARNING - flag->call_transporter failed while trying to work around no-transporter issue!  I guess let it try again next time";
        }
        // don't erase if it isn't fixed yet, keep checking each AI loop
        //++no_transporter_timer;
        //  **actually, DO erase, because in rare cases if a road is removed when a timer set, and then a different road built there(?)
        //   it causes a neverending trigger.  Best to assume it works and let it discover all over again if it has not
        no_transporter_timer = no_transporter_timers.erase(no_transporter_timer);
      }
    }
    else {
      ++no_transporter_timer;
    }
  }
  //AILogDebug["do_fix_missing_transporters"] << name << " aftebuild warer checking timeouts, there are now " << no_transporter_timers.size() << " TOTAL no_transporter_timers set";

  //
  // look for missing transporters and if found set timers for them
  //
  unsigned int flag_index = 0;
  flags_static_copy = *(game->get_flags());
  flags = &flags_static_copy;
  for (Flag *flag : *flags) {
    if (flag == nullptr || flag->get_owner() != player_index || !flag->is_connected())
      continue;
    // it seems the castle shows as having a path Up-Left into it...
    //    I wonder if any other buildings do also?  warehouses?
    // YES!  I saw other mention such as in path_splited() that at least 
    //  some buildings show up as having a valid path UP-LEFT
    if (flag->get_position() == castle_flag_pos)
      continue;
    flag_index = flag->get_index();
    for (Direction dir : cycle_directions_cw()) {
      if (!map->has_path(flag->get_position(), dir))
        continue;
      if (map->road_segment_in_water(flag->get_position(), dir))
        continue;
      //
      // check for rare missing transporter bug, where flag lists a transporter in dir, but no serf is actually on the road
      //  I am starting to think this is caused by previously working transporters somehow disappearing
      //
      // NOTE - an easy "non-cheating" fix for this is to simply create a new flag on the road where the transporter is missing
      //  it will split the flag and call two new transporters, resolving the problem
      //
      if (flag->has_transporter(dir)) {
        bool found_transporter = false;
        Road road;
        if (!map->has_path(flag->get_position(), dir)) {
          AILogVerbose["do_fix_missing_transporters"] << name << " no path found for flag with pos " << flag->get_position() << ", index " << flag_index << ", dir " << dir << " / " << NameDirection[dir] << " during no_transporter check!  FIND OUT WHY";
        }
        MapPos pos = flag->get_position();
        Direction tmp_dir = dir;
        // trace the road until it ends, looking for transporters at each pos
        while (true) {
          // check for idle transporter
          // when transporters are idle on roads, they "disappear" and are set to index 0 with invalid job, etc.  I think this may be
          //   so other serfs can pass through them?  Regardless of why, they can be detected by checking if map->get_idle_serf(pos) is true
          if (map->get_idle_serf(pos)) {
            found_transporter = true;
            break;
          }
          // check for an active transporter
          Serf *serf_on_path = game->get_serf_at_pos(pos);
          if (serf_on_path != nullptr) {
            if (serf_on_path->get_type() == Serf::TypeTransporter) {
              // need to check state to make sure it isn't just passing through on way to another road ???
              // ALSO, if there is a serf working on an adjacent road that happens to be at this flag end, this will think it is
              //    servicing this road being checked, but on some subsequent pass it should detect it
              found_transporter = true;
              break;
            }
          }
          if (map->has_flag(pos) && pos != flag->get_position()) {
            break;
          }
          pos = map->move(pos, tmp_dir);
          for (Direction new_dir : cycle_directions_cw()) {
            if (map->has_path(pos, new_dir) && new_dir != reverse_direction(tmp_dir)) {
              tmp_dir = new_dir;
              break;
            }
          }
        }
        if (found_transporter == false) {
          //
          // when trying to debug the missing transporter issue, I noticed that when I loaded an auto-save that is supposed to show the problem,
          //  the missing serf is now on the road and functional!  this kind of makes sense if the game thinks there is a serf there, so it saves
          //  that a serf is there, and on game load it fixes the issue preventing the serf from appearing and working?
          //  NO, it actually looks like the transporter was previously in place and working, then somehow disappeared!  with no apparant change in
          //  the road or buildings on it.  Maybe the serf was somehow deleted by index due to some infrequent bug with serf management logic??
          //
          // 
          /*
          I was able to catch this live on a screen recording. What happens is that a functioning transporter enters Lost state and abandons his post. 
            There is no loss of territory or anything that should cause the serf to become Lost. 
             In this particular case, what I see in the video (at 2:43) is:

          - transporter (that disappears soon) is idle on road, looking back and forth as idle transporters do. Leading up to this the transporter was normally busy and working fine moving resources back and forth on a busy road
          - another serf drops a Res at this serf's flag
          - meanwhile, a Geologist is heading to the same flag, returning to castle
          - the transporter wakes as soon as the Res is dropped for him, and faces its flag direction
          - the Geologist approaching the same flag prevents the transporter from getting the Res, so the transporter waits, still facing the Res-flag
          - once the Geologist reaches the Res-flag, it is travelling in the direction of the transporter
          - they perform a serf swap, the Geologist passes the transporter heading west, the transporter passes the Geologist heading east to the Res-flag and picks up the Res
          - the Geologist is blocked from heading further west by another serf in his way (a Lumberjack moving a felled tree in front of him, not actually on the road but right next to it)
          - the transporter is facing west to transport the Res west as usual and is carring the Res, but is blocked by the Geologist
          - immediately as the transporter becomes blocked by the geologist (after facing west for a split second), the transporter becomes Lost and the Res he is carrying disappears (though he is still facing west)
          - the Lost transporter walks off somewhere east, eventually makes is way back to the Castle, and obviously never returns, and no replacement is sent (because this isn't supposed to happen)
          - the game doesn't realize the transporter is lost, the only way I was detecting it is by walking the road to see that no transporter was actually there despite the Flag having a record of an active transporter there
          */
          AILogDebug["do_fix_missing_transporters"] << name << " WARNING - found rare type of missing transporter bug!  Flag #" << flag->get_index() << " at pos " << flag->get_position() << " seems to be missing a transporter on road in dir " << NameDirection[dir] << " despite it thinking there is one there!";
          ai_mark_pos.insert(std::make_pair(flag->get_position(), "lt_blue"));
          std::this_thread::sleep_for(std::chrono::milliseconds(5000));
          ai_mark_pos.clear();
          AILogDebug["do_fix_missing_transporters"] << name << " detected BUG FOUND - RARER NO TRANSPORTER on road at pos " << game->get_flag(flag_index)->get_position() << ", marking in white";
          ai_mark_pos.insert(ColorDot(flag->get_position(), "white"));
          AILogDebug["do_fix_missing_transporters"] << name << " trying to immediately force call a transporter";
          AILogDebug["do_fix_missing_transporters"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling flag->call_transporter, to work around RARER no-transporter issue";
          game->get_mutex()->lock();
          AILogDebug["do_fix_missing_transporters"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling flag->call_transporter, to work around RARER no-transporter issue";
          
          AILogDebug["do_fix_missing_transporters"] << name << " NOT calling out missing new transporter, trying to debug this now";
          bool was_called = false;
          //bool was_called = flag->call_transporter(dir, false);  // hardcoding is_water_path to false because I am seeing weird crash with this checking for Serf::TypeSailor

          AILogDebug["do_fix_missing_transporters"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling flag->call_transporter, to work around RARER no-transporter issue";
          game->get_mutex()->unlock();
          AILogDebug["do_fix_missing_transporters"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling flag->call_transporter, to work around RARER no-transporter issue";
          /*
          if (!was_called) {
            AILogDebug["do_fix_missing_transporters"] << name << " WARNING - flag->call_transporter to " << flag->get_position() << ", dir " << NameDirection[dir] << " - failed while trying to work around RARER no-transporter issue!  I guess let it try again next time";
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(5000));
          */
          //game->pause();
        }
        else {
          AILogVerbose["do_fix_missing_transporters"] << name << " found expected transporter with Flag #" << flag->get_index() << " at pos " << flag->get_position() << " on road in dir " << NameDirection[dir];
        }

      }
      /* I think I finally fixed this, it was due to typo in the path_splited road splitting logic
      //
      // check for common missing transporter bug, where no transporter is assigned at all
      //  I added some debugging and I see that the leave_building call is made so a serf is dispatched, but not sure if it ever actually
      //  exits the Inventory (castle/stock) and starts travelling and is lost along the way, or if it never actually starts walking
      //
      if (!flag->has_transporter(dir)) {
        AILogDebug["do_fix_missing_transporters"] << name << " flag at pos " << flag->get_position() << " has no transporter on path in dir " << dir << " / " << NameDirection[dir];
        // check to see if one was requested
        if (flag->serf_requested(dir)){
          AILogDebug["do_fix_missing_transporters"] << name << " flag->serf_requested is true at pos " << flag->get_position() << " in dir " << dir << " / " << NameDirection[dir];
        }else{
          AILogDebug["do_fix_missing_transporters"] << name << " flag->serf_requested is FALSE at pos " << flag->get_position() << " in dir " << dir << " / " << NameDirection[dir] << ", marking flag in yellow and dir in dk_yellow";
          // this happens when flags/roads/buildings were just created, not an issue as far as I have ever seen
          //ai_mark_pos.insert(ColorDot(flag->get_position(), "yellow"));
          //ai_mark_pos.insert(ColorDot(map->move(flag->get_position(), dir), "dk_yellow"));
          //std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        }
        // maybe check to see if there is a Walking Transporter serf whose dest is this path?
        // see if a timer already set for this flag & dir
        AILogDebug["do_fix_missing_transporters"] << name << " there are " << no_transporter_timers.count(std::make_pair(flag_index, dir)) << " no_transporter_timers set for this flag with pos " << flag->get_position() << ", index " << flag_index << ", dir " << dir << " / " << NameDirection[dir];
        if (no_transporter_timers.count(std::make_pair(flag_index, dir)) == 0) {
          AILogDebug["do_fix_missing_transporters"] << name << " setting countdown timer for flag with pos " << flag->get_position() << ", index " << flag_index << ", dir " << dir << " / " << NameDirection[dir];
          // this is difficult to tune... if too long the entire economy can break down while waiting to clear it
          //    but too soon as it could trigger for a faraway road that the original transporter simply hasn't reached
          //  Maybe make more advanced and check to see if there is an outgoing transporter serf with this destination flag/dir??
          // jan05 2021 - setting this 25x now that I am looking at it closer.  It should be either extremely long timer or
          //    based on the distance from the inventory (warehouse/stock) that is dispatching the serf
          no_transporter_timers.insert(std::make_pair(std::make_pair(flag_index, dir), game->get_tick() + 50000));
          AILogDebug["do_fix_missing_transporters"] << name << " there are now " << no_transporter_timers.count(std::make_pair(flag_index, dir)) << " no_transporter_timers set for this flag with pos " << flag->get_position() << ", index " << flag_index << ", dir " << dir << " / " << NameDirection[dir] << ", value is: " << no_transporter_timers.at(std::make_pair(flag_index, dir));

          // try to find a serf that is on way to this Flag-dir
          AILogVerbose["do_fix_missing_transporters"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_serfs(player) (for do_fix_missing_transporters)";
          game->get_mutex()->lock();
          AILogVerbose["do_fix_missing_transporters"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_serfs(player) (for do_fix_missing_transporters)";
          for (Serf *serf : game->get_player_serfs(player)) {
            if (serf->get_walking_dest() == flag_index){
              AILogDebug["do_fix_missing_transporters"] << name << " serf at pos " << serf->get_pos() << " with type " << NameSerf[serf->get_type()] << " has dest of this Flag with index " << flag_index << " and pos " << flag->get_position();
              ai_mark_serf.push_back(serf->get_index());
            }
          }
          AILogVerbose["do_fix_missing_transporters"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex before calling game->get_player_serfs(player) (for do_fix_missing_transporters)";
          game->get_mutex()->unlock();
          AILogVerbose["do_fix_missing_transporters"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex before calling game->get_player_serfs(player) (for do_fix_missing_transporters)";
          
        } else {
          int trigger_ticks = static_cast<int>(no_transporter_timers.at(std::make_pair(flag_index, dir)) - game->get_tick());
          AILogDebug["do_fix_missing_transporters"] << name << " a timer is already set for flag with pos " << flag->get_position() << ", index " << flag_index << ", dir " << dir << " / " << NameDirection[dir] << ", it will trigger in " << trigger_ticks << " ticks";
        }
      }
      */
    }
  }
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_fix_missing_transporters"] << name << " done do_fix_missing_transporters call took " << duration;
}


void
AI::do_send_geologists() {
  //
  // send geologists to hills
  //
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  AILogDebug["do_send_geologists"] << inventory_pos << " HouseKeeping: send geologists to hills";
  ai_status.assign("HOUSEKEEPING - send geologists");
  MapPosSet count_by_corner;
  MapPosVector geologist_positions;
  // don't send geologists if already have enough mines for this Inventory pos
  update_building_counts();
  int completed_coalmine_count = stock_buildings.at(inventory_pos).completed_count[Building::TypeCoalMine];
  int completed_ironmine_count = stock_buildings.at(inventory_pos).completed_count[Building::TypeIronMine];
  int completed_goldmine_count = stock_buildings.at(inventory_pos).completed_count[Building::TypeGoldMine];
  int total_coalmine_count = stock_buildings.at(inventory_pos).count[Building::TypeCoalMine];
  int total_ironmine_count = stock_buildings.at(inventory_pos).count[Building::TypeIronMine];
  int total_goldmine_count = stock_buildings.at(inventory_pos).count[Building::TypeGoldMine];
  // true if at least one COMPLETED mine of each type, plus type-specific max which includes placed-but-incomplete mines
  if ((completed_coalmine_count >= 1 && completed_ironmine_count >= 1 && completed_goldmine_count) &&
    (total_coalmine_count >= max_coalmines && total_ironmine_count >= max_ironmines && total_goldmine_count >= max_goldmines)) {
    // maybe make this simply send geologists far less often... one every 10 loops?
    AILogDebug["do_send_geologists"] << inventory_pos << " enough mines placed, not sending anymore geologists";
    return;
  }
  // figure out if we should be creating more geologists by counting the number
  //   of excess hammers (not reserved for future builders or blacksmiths)
  //   If max number of builders & blacksmiths reached, no longer need to reserve hammers
  unsigned int total_geologists = player->get_serf_count(Serf::TypeGeologist);
  unsigned int idle_geologists = serfs_idle[Serf::TypeGeologist];
  unsigned int potential_geologists = 0;
  unsigned int hammers_count = realm_inv[Resource::TypeHammer];
  unsigned int total_builders = player->get_serf_count(Serf::TypeBuilder);
  unsigned int total_blacksmiths = player->get_serf_count(Serf::TypeWeaponSmith);
  int reserve_builders = static_cast<int>(specialists_max[Serf::TypeBuilder] - total_builders);
  if (reserve_builders < 0) { reserve_builders = 0; }
  int reserve_blacksmiths = static_cast<int>(specialists_max[Serf::TypeWeaponSmith] - total_blacksmiths);
  if (reserve_blacksmiths < 0) { reserve_blacksmiths = 0; }
  int reserve_hammers = reserve_builders + reserve_blacksmiths;
  int excess_hammers = static_cast<int>(hammers_count - reserve_hammers);
  AILogDebug["do_send_geologists"] << inventory_pos << " total_geologists " << total_geologists << ", idle_geologists " << idle_geologists << ", geologists_max "
    << geologists_max << ", total_builders " << total_builders << ", total_blacksmiths " << total_blacksmiths
    << ", hammers_count " << hammers_count << ", reserve_hammers " << reserve_hammers << ", excess_hammers " << excess_hammers;
  if (excess_hammers > 0) {
    // its possible to accidently create > geologists_max, so if this happens avoid going negative
    if (total_geologists >= geologists_max) {
      AILogDebug["do_send_geologists"] << inventory_pos << " geologists_max " << geologists_max << " hit, have total_geologists " << total_geologists;
      potential_geologists = 0;
      // it seems there is an issue where idle_geologists is appearing greater than it really is, resulting in more geologists being created
      //  even after geologists_max hit.  As a work-around to hopefully avoid this, never allow idle_geologists to be >1 once geologists_max hit
      if (idle_geologists > 1) {
        idle_geologists = 1;
      }
    }
    else {
      // cap the number of potential geologists at the number of excess hammers (those not reserved for builder/blacksmith)
      potential_geologists = unsigned(excess_hammers) > geologists_max - total_geologists ? geologists_max - total_geologists : unsigned(excess_hammers);
      AILogDebug["do_send_geologists"] << inventory_pos << " at least one excess hammer, potential_geologists set to " << potential_geologists;
    }
  }
  else {
    AILogDebug["do_send_geologists"] << inventory_pos << " hammers_count <= hammers_reserve, potential_geologists set to zero.  Existing geologists: " << total_geologists;
  }
  // determine where any geologists are currently operating (to later avoid sending too many to one area)
  AILogDebug["do_send_geologists"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_serfs(player) (for serf_wait_timers is_waiting)";
  game->get_mutex()->lock();
  AILogDebug["do_send_geologists"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_serfs(player) (for serf_wait_timers is_waiting)";
  // this returns a copy, so it should be thread-safe
  //  maybe not, beause game->get_player_serfs internally just does for (Serf *serf : serfs)
  for (Serf *serf : game->get_player_serfs(player)) {
    if (serf->get_type() == Serf::TypeGeologist) {
      MapPos pos = bad_map_pos;
      //AILogDebug["do_send_geologists"] << inventory_pos << " geologist state: " << serf->print_state();
      // geologist states:
      //   idle geologists are StateIdleInStock with walking_dest of *an invalid too-high flag*, not sure what it represents
      //   when a geologist is called out from castle state is StateLeavingBuilding and dest is flag dest
      //   as geo walks to flag dest, state is StateWalking
      //   after reaching flag dest, state becomes StateFreeWalking (when you'd think it was LookingForGeoSpot, because that is what geo is doing)
      //   when stopping to sample, state becomes StateSamplingGeoSpot as expected
      //   after sampling a spot, repeats StateFreeWalking and StateSamplingGeoSpot until it decides it is done
      //   when done, starts walking back to flag dest while still in StateFreeWalking, no dest exists at this point
      //   once reached flag dest, state becomes StateWalking with dest castle flag (Flag index # 1)
      //   then returns to castle via roads, state becomes StateEnteringBuilding as it enters castle
      // can't use walking_dest test because idle serfs have a nonzero but invalid walking_dest
      //if (serf->get_walking_dest() != 0) {
      Serf::State state = serf->get_state();
      int dest;
      Flag *flag;
      switch (state) {
      case Serf::StateLeavingBuilding:
      case Serf::StateWalking:
      case Serf::StateIdleOnPath:
        dest = serf->get_walking_dest();
        //AILogDebug["do_send_geologists"] << inventory_pos << " geo serf walking_dest flag index # is " << dest;
        flag = game->get_flag(dest);
        //AILogDebug["do_send_geologists"] << inventory_pos << " geo serf walking_dest flag pointer fetched";
        if (flag == nullptr) {
          // it seems that serf dest can be a flag index much higher than the actual highest flag index, it likely has some bit-math meaning
          AILogDebug["do_send_geologists"] << inventory_pos << " geo flag is nullptr!  why?  skipping";
        }
        else {
          MapPos pos = flag->get_position();
          AILogDebug["do_send_geologists"] << inventory_pos << " a geologist has walking_dest of flag at pos " << pos;
        }
        break;
      case Serf::StateLookingForGeoSpot:
      case Serf::StateSamplingGeoSpot:
      case Serf::StateFreeWalking:
        // note - I don't think StateLookingForGeoSpot is ever used... it seems to be some kind of fallback if Dir = 6 (invalid?)
        //    or maybe time spent in this state is so small it cannot be seen easily
        pos = serf->get_pos();
        AILogDebug["do_send_geologists"] << inventory_pos << " a geologist is currently working in the vicinity of pos " << pos;
        break;
      }
      if (pos != bad_map_pos && pos != castle_flag_pos) {
        geologist_positions.push_back(pos);
      }
    }
  }
  AILogDebug["do_send_geologists"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_serfs(player) (for serf_wait_timers is_waiting)";
  game->get_mutex()->unlock();
  AILogDebug["do_send_geologists"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_serfs(player) (for serf_wait_timers is_waiting)";
  AILogDebug["do_send_geologists"] << inventory_pos << " active geologists found: " << geologist_positions.size();
  // count hills (Tundra0-2, Snow0, NOT Snow1 because can't build mines there)
  // for mined resouce finding,  reverse the occupied_military_buildings order
  //   so the newest military building is searched first,
  //    instead of castle first then outward
  MapPosVector foo = stock_buildings.at(inventory_pos).occupied_military_pos;
  // for some reason after switching to parallel stocks/warehouse support, cannot use stock_buildings.at(inventory_pos).occupied_military_pos iterator directly
  //  must copy it to another MapPosVector and iterate over that.  I'm sure there is a cleaner way
  //for (MapPosVector::reverse_iterator it = stock_buildings.at(inventory_pos).occupied_military_pos.rbegin(); it != stock_buildings.at(inventory_pos).occupied_military_pos.rend(); ++it) {
  for (MapPosVector::reverse_iterator it = foo.rbegin(); it != foo.rend(); ++it) {
    MapPos center_pos = *it;
    MapPosVector corners = AI::get_corners(center_pos);
    for (MapPos corner_pos : corners) {
      AILogDebug["do_send_geologists"] << inventory_pos << " considering corner_pos " << corner_pos;
      // i'm not sure why Snow0 is valid for mining, it seems to be the edge where hill meets snow
      unsigned int count = AI::count_empty_terrain_near_pos(corner_pos, AI::spiral_dist(4), Map::TerrainTundra0, Map::TerrainSnow0, "orange");
      //AILogDebug["do_send_geologists"] << inventory_pos << " corner has hills count: " << count << ", min acceptable is " << hills_min;
      if (count >= hills_min) {
        double sign_density = AI::count_geologist_sign_density(corner_pos, AI::spiral_dist(4));
        if (sign_density > geologist_sign_density_max) {
          AILogDebug["do_send_geologists"] << inventory_pos << " sign density " << sign_density << " is over geologist_sign_density_max " << geologist_sign_density_max << ", skipping this corner";
          continue;
        }else if (sign_density > geologist_sign_density_deprio) {
          AILogDebug["do_send_geologists"] << inventory_pos << " sign density " << sign_density << " is over geologist_sign_density_deprio " << geologist_sign_density_deprio << ".  De-prioritizing this area for geologists";
          count = count / 3;
        }
        AILogDebug["do_send_geologists"] << inventory_pos << " inserting hills corner pos " << corner_pos << " to count_by_corner with adjusted value " << count;
        count_by_corner.insert(std::make_pair(corner_pos, count));
      }
    }
    // try to place a flag on hills anywhere no flag is nearby
    AILogDebug["do_send_geologists"] << inventory_pos << " trying to find a good place to send geologists";
    MapPosVector search_positions = AI::sort_by_val_desc(count_by_corner);
    MapPos built_pos = bad_map_pos;
    for (MapPos corner_pos : search_positions) {
      AILogDebug["do_send_geologists"] << inventory_pos << " considering sending geologists to the vicinity of pos " << corner_pos;
      // if two or more geologists are already operating in this area, skip this corner
      int geologists_corner = 0;
      for (unsigned int i = 0; i < AI::spiral_dist(6); i++) {
        MapPos pos = map->pos_add_extended_spirally(corner_pos, i);
        unsigned int geologists_pos = static_cast<unsigned int>(std::count(geologist_positions.begin(), geologist_positions.end(), pos));
        if (geologists_pos > 0) {
          //AILogDebug["do_send_geologists"] << inventory_pos << " there are " << geologists_pos << " geologists operating at pos " << pos;
          geologists_corner += geologists_pos;
        }
      }
      AILogDebug["do_send_geologists"] << inventory_pos << " there are " << geologists_corner << " other geologists operating in the vicinity of corner_pos " << corner_pos;
      if (geologists_corner >= 2) {
        AILogDebug["do_send_geologists"] << inventory_pos << " found at least two geologists in the vicinity of corner_pos " << corner_pos << ", skipping this corner";
        break;
      }
      AILogDebug["do_send_geologists"] << inventory_pos << " trying to build flags & send geologists near pos " << corner_pos;
      for (unsigned int i = 0; i < AI::spiral_dist(4); i++) {
        MapPos pos = map->pos_add_extended_spirally(corner_pos, i);
        if (!AI::has_terrain_type(game, pos, Map::TerrainTundra0, Map::TerrainSnow0))
          continue;
        // build flags for geologists
        bool built_new_flag_for_geologist = false;
        if (game->can_build_flag(pos, player)) {
          //AILogDebug["do_send_geologists"] << inventory_pos << " can build flag at pos " << pos << ", checking to see if any other flags nearby...";
          unsigned int other_flags = 0;
          for (unsigned int i2 = 0; i2 < AI::spiral_dist(5); i2++) {
            MapPos pos2 = map->pos_add_extended_spirally(pos, i2);
            if (map->has_flag(pos2)) {
              //AILogDebug["do_send_geologists"] << inventory_pos << " found a flag nearby to " << pos << ", breaking";
              other_flags++;
              break;
            }
          }
          // avoid known bad positions for this building type (using TypeNone to mean Flag here)
          //   note this list is NOT persisted if AI thread goes away or game load/restart
          if (is_bad_building_pos(pos, Building::TypeNone)) {
            AILogDebug["do_send_geologists"] << inventory_pos << " skipping this pos, TypeNone (i.e. geo Flag) marked in bad_building_pos list";
            continue;
          }
          if (other_flags == 0) {
            AILogDebug["do_send_geologists"] << inventory_pos << " no other flags nearby " << pos << ", building flag here";
            AILogDebug["do_send_geologists"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->build_flag, for geologist";
            game->get_mutex()->lock();
            AILogDebug["do_send_geologists"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has locking mutex before calling game->build_flag, for geologist";
            built_pos = game->build_flag(pos, player);
            AILogDebug["do_send_geologists"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->build_flag, for geologist";
            game->get_mutex()->unlock();
            AILogDebug["do_send_geologists"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->build_flag, for geologist";
            if (!map->has_flag(pos)) {
              AILogDebug["do_send_geologists"] << inventory_pos << " failed to build flag at pos " << pos << "!!! why??";
            }
            else {
              AILogDebug["do_send_geologists"] << inventory_pos << " built flag at pos " << pos << ", trying to connect it...";
              built_new_flag_for_geologist = true;
            }
            Road notused; // not used here, can I just pass a zero instead of &notused to build_best_road and skip initialization of a wasted object?
            if (!AI::build_best_road(pos, road_options, &notused)) {
              AILogDebug["do_send_geologists"] << inventory_pos << " failed to connect new gologist flag to road network!  removing the flag";
              AILogDebug["do_send_geologists"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling demolish_flag (built for geoligist, couldn't connect)";
              game->get_mutex()->lock();
              AILogDebug["do_send_geologists"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling demolish_flag (built for geoligist, couldn't connect)";
              game->demolish_flag(pos, player);
              AILogDebug["do_send_geologists"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling demolish_flag (built for geoligist, couldn't connect)";
              game->get_mutex()->unlock();
              AILogDebug["do_send_geologists"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling demolish_flag (built for geoligist, couldn't connect)";
              AILogDebug["do_send_geologists"] << inventory_pos << " adding this flag pos " << pos << " to bad_building_pos list";
              // because there is no Building::Type for a plain flag, use Building::TypeNone for now.
              //  alternatively, could create a new type, or use some number that has no type
              bad_building_pos.insert(std::make_pair(pos, Building::TypeNone));
            }
          }
        }
        // send geologist
        if (map->has_flag(pos) && map->get_owner(pos) == player_index && game->get_flag_at_pos(pos)->is_connected()) {
          AILogDebug["do_send_geologists"] << inventory_pos << " found connected flag on hills for geologist at pos " << pos;
          Flag *flag = game->get_flag_at_pos(pos);
          if (flag == nullptr)
            continue;
          // check once again for sign density, but this time centered around the Flag rather than the corner that the flag is near
          //  this is to avoid issue where geologists keep being sent to a flag that is already near 100% sign density, but the corner
          //   itself is not.  Should also check for number of geologists operating in area around flag and disqualify?
          double sign_density = AI::count_geologist_sign_density(corner_pos, AI::spiral_dist(4));
          if (sign_density > geologist_sign_density_max) {
            AILogDebug["do_send_geologists"] << inventory_pos << " sign density " << sign_density << " is over geologist_sign_density_max " << geologist_sign_density_max << ", not sending geologist to this flag";
            continue;
          }
          if (idle_geologists >= 1) {
            AILogDebug["do_send_geologists"] << inventory_pos << " sending an idle geologist to pos " << pos;
            idle_geologists--;
          }
          else if (potential_geologists >= 1 && specialists_max[Serf::TypeGeologist] > total_geologists) {
            AILogDebug["do_send_geologists"] << inventory_pos << " creating a new geologist and sending to pos " << pos;
            potential_geologists--;
            total_geologists++;
          }
          else {
            AILogDebug["do_send_geologists"] << inventory_pos << " no idle or potential geologists available, returning";
            return;
          }
          AILogDebug["do_send_geologists"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->send_geologist(flag)";
          game->get_mutex()->lock();
          AILogDebug["do_send_geologists"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->send_geologist(flag)";
          /*
          // even with this extra check it still will likely break once warehouse/stocks come into play
          //   also, it doesn't make sense that theis check would even be required, clearly something else is wrong
          ///  this STILL results in sending more geologists over max.  WTF.
          if (total_geologists >= geologists_max) {
            if (sending_idle_geologist == true) {
              AILogDebug["do_send_geologists"] << inventory_pos << " about to send an idle_geologists, double-checking to see if one is really idle...";
              serfs_idle = player->get_stats_serfs_idle();
              idle_geologists = serfs_idle[Serf::TypeGeologist];
              AILogDebug["do_send_geologists"] << inventory_pos << " after re-checking, found idle_geologists: " << idle_geologists;
              if (idle_geologists < 1) {
                AILogDebug["do_send_geologists"] << inventory_pos << " NO IDLE GEOLOGIST after re-check, returning early and not sending any geologists out";
              }
            }
          }
          */
          bool was_sent = game->send_geologist(flag);
          AILogDebug["do_send_geologists"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->send_geologist(flag)";
          game->get_mutex()->unlock();
          AILogDebug["do_send_geologists"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->send_geologist(flag)";
          if (was_sent) {
            //AILogDebug["do_send_geologists"] << inventory_pos << " sent an geologist to pos " << pos << ", moving on to next corner";
            //break;
            AILogDebug["do_send_geologists"] << inventory_pos << " sent a geologist to pos " << pos << ", not sending any more geologists until next call of this function";
            // because of incredibly frusterating "too many geologists" / idle_geologists issue, only send one geologist per call of this function.
            //   and even THIS will probably not work right for warehouse/stocks
            return;
          }
          else {
            AILogDebug["do_send_geologists"] << inventory_pos << " failed to send geologist to pos " << pos << ".  This happens sometimes but appears benign.  Sleeping 1sec";
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            // I stil cannot tell what is causing this to happen so often, maybe sending geologists is rate-limited per flag?
            //   my debug logs are showing that it is failing because the flag search failed...but why??
            ///    "inside Game::send_serf_to_flag, send_serf_to_flag_search returned false"

            // playing sound causes occasional write access violation, probably needs to be made thread-safe.  Simply disabling for now.
            //Audio &audio = Audio::get_instance();
            //Audio::PPlayer player = audio.get_sound_player();
            //if (player) { player->play_track(Audio::TypeSfxNotAccepted); }
          }
        } // if flag is acceptable for geologist
      } // foreach hills pos spirally
    } // foreach corner
  } // foreach military building
  AILogDebug["do_send_geologists"] << inventory_pos << " done do_send_geologists";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_send_geologists"] << inventory_pos << " done do_send_geologists call took " << duration;
}


void
AI::do_build_rangers() {
  AILogDebug["do_build_rangers"] << name << " inside do_build_rangers";
  //
  // build ranger near lumberjacks that have few trees and no ranger nearby
  //
  AILogDebug["do_build_rangers"] << name << " HouseKeeping: build rangers near lumberjacks that have few trees and no ranger nearby";
  ai_status.assign("HOUSEKEEPING - build rangers");
  AILogDebug["do_build_rangers"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for do_build_rangers)";
  game->get_mutex()->lock();
  AILogDebug["do_build_rangers"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for do_build_rangers)";
  Game::ListBuildings buildings = game->get_player_buildings(player);
  AILogDebug["do_build_rangers"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player) (for do_build_rangers)";
  game->get_mutex()->unlock();
  AILogDebug["do_build_rangers"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player) (for do_build_rangers)";
  for (Building *building : buildings) {
    if (building->get_type() != Building::TypeLumberjack)
      continue;
    MapPos pos = building->get_position();
    unsigned int count = AI::count_objects_near_pos(pos, AI::spiral_dist(4), Map::ObjectTree0, Map::ObjectPine7, "lt_green");
    AILogDebug["do_build_rangers"] << name << " lumberjack trees nearby count: " << count << ", min acceptable is " << near_trees_min;
    if (count >= near_trees_min)
      continue;
    if (AI::building_exists_near_pos(pos, AI::spiral_dist(6), Building::TypeForester))
      continue;
    AILogDebug["do_build_rangers"] << name << " lumberjack at " << pos << " has < min trees and no ranger nearby, trying to place ranger";
    MapPos built_pos = AI::build_near_pos(pos, AI::spiral_dist(6), Building::TypeForester);
    if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos)
      AILogDebug["do_build_rangers"] << name << " built ranger at pos " << built_pos;
  }
  AILogDebug["do_build_rangers"] << name << " done do_build_rangers";
}

void
AI::do_demolish_unproductive_3rd_lumberjacks() {
  AILogDebug["do_demolish_unproductive_3rd_lumberjacks"] << name << " inside do_demolish_unproductive_3rd_lumberjacks";
  ai_status.assign("HOUSEKEEPING - burn unproductive 3rd lumberjacks");
  AILogDebug["do_demolish_unproductive_3rd_lumberjacks"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player)";
  game->get_mutex()->lock();
  AILogDebug["do_demolish_unproductive_3rd_lumberjacks"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player)";
  Game::ListBuildings buildings = game->get_player_buildings(player);
  AILogDebug["do_demolish_unproductive_3rd_lumberjacks"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player)";
  game->get_mutex()->unlock();
  AILogDebug["do_demolish_unproductive_3rd_lumberjacks"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player)";
  // find all sawmills in the realm, and check the area around each one
  // if three completed lumberjacks and a ranger are nearby, but still not many trees,
  //   then burn one lumberjack to allow it to be replaced elsewhere by the do_food_buildings_and_3rd_lumberjack function
  for (Building *building : buildings) {
    if (building->get_type() != Building::TypeSawmill)
      continue;
    MapPos sawmill_pos = building->get_position();
    unsigned int lumberjack_count = 0;
    MapPosVector lumberjack_positions;
    for (unsigned int i = 0; i < AI::spiral_dist(8); i++) {
      MapPos pos = map->pos_add_extended_spirally(sawmill_pos, i);
      if (map->get_obj(pos) == Map::ObjectSmallBuilding
        && game->get_building_at_pos(pos)->get_type() == Building::TypeLumberjack
        && map->get_owner(pos) == player_index) {
        ++lumberjack_count;
        lumberjack_positions.push_back(pos);
      }
    }
    if (lumberjack_count < 3)
      continue;
    unsigned int ranger_count = 0;
    unsigned int mature_tree_count = 0;
    for (MapPos lumberjack_pos : lumberjack_positions) {
      for (unsigned int i = 0; i < AI::spiral_dist(6); i++) {
        MapPos pos = map->pos_add_extended_spirally(lumberjack_pos, i);
        if (map->get_obj(pos) == Map::ObjectSmallBuilding
          && map->get_owner(pos) == player_index
          && game->get_building_at_pos(pos)->get_type() == Building::TypeForester
          && game->get_building_at_pos(pos)->is_done())
          ++ranger_count;
        if (map->get_obj(pos) >= Map::ObjectTree0 && map->get_obj(pos) <= Map::ObjectPine7)
          ++mature_tree_count;
      }
      if (ranger_count > 0 && mature_tree_count < near_trees_min) {
        AILogDebug["do_demolish_unproductive_3rd_lumberjacks"] << name << " 3rd lumberjack at pos " << lumberjack_pos << " has a nearby ranger yet still only has " << mature_tree_count << ", less than near_trees_min " << near_trees_min << ".  Burning it so it can be replaced in a better spot";
        AILogDebug["do_demolish_unproductive_3rd_lumberjacks"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->demolish_building";
        game->get_mutex()->lock();
        AILogDebug["do_demolish_unproductive_3rd_lumberjacks"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->demolish_building";
        game->demolish_building(lumberjack_pos, player);
        AILogDebug["do_demolish_unproductive_3rd_lumberjacks"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->demolish_building";
        game->get_mutex()->unlock();
        AILogDebug["do_demolish_unproductive_3rd_lumberjacks"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->demolish_building";
        break;
      }
    }
  }
  AILogDebug["do_demolish_unproductive_3rd_lumberjacks"] << name << " done do_demolish_unproductive_3rd_lumberjacks";
}



// remove roads that lead to:
//  - an occupied ranger building, if no other paths from ranger flag
//  - a mountain/geologist road, if sign density > max
// also:  fully-constructed, occupied huts that have another connected 
//  flag nearby that can be successfully connected to instead (and do so)
void
AI::do_remove_road_stubs() {
  AILogDebug["do_remove_road_stubs"] << name << " inside do_remove_road_stubs";
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  unsigned int roads_removed = 0;
  //
  // ...an occupied ranger building, if no other paths from ranger flag
  //
  AILogDebug["do_remove_road_stubs"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for do_remove_road_stubs)";
  game->get_mutex()->lock();
  AILogDebug["do_remove_road_stubs"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for do_remove_road_stubs)";
  Game::ListBuildings buildings = game->get_player_buildings(player);
  AILogDebug["do_remove_road_stubs"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player) (for do_remove_road_stubs)";
  game->get_mutex()->unlock();
  AILogDebug["do_remove_road_stubs"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player) (for do_remove_road_stubs)";
  for (Building *building : buildings) {
    if (building->get_type() != Building::TypeForester)
      continue;
    if (!building->is_done() || !building->has_serf())
      continue;
    MapPos pos = building->get_position();
    MapPos flag_pos = map->move_down_right(pos);
    unsigned int paths = 0;
    Direction road_dir;
    for (Direction dir : cycle_directions_cw()) {
      if (!map->has_path(flag_pos, dir)) { continue; }
      paths++;
      if (paths > 1) {
        AILogDebug["do_remove_road_stubs"] << name << " occupied ranger at pos " << pos << "'s flag has more than one path, not removing road";
        break;
      }
      road_dir = dir;
    }
    if (paths == 1) {
      AILogDebug["do_remove_road_stubs"] << name << " occupied ranger at pos " << pos << "'s flag has only one path, removing the stub road";
      game->demolish_road(map->move(flag_pos, road_dir), player);
      roads_removed++;
    }
  }
  //
  // a mountain/geologist road, if sign density > max
  //
  flags_static_copy = *(game->get_flags());
  flags = &flags_static_copy;
  for (Flag *flag : *flags) {
    if (flag == nullptr || flag->get_owner() != player_index || !flag->is_connected() || flag->has_building())
      continue;
    MapPos flag_pos = flag->get_position();
    if (!AI::has_terrain_type(game, flag_pos, Map::TerrainTundra0, Map::TerrainSnow1))
      continue;
    // if *either* condition is true, stub road is eligible for removal:
    //  - if sign density is > max
    //  - if another flag is very close
    bool eligible = false;
    if (AI::count_geologist_sign_density(flag_pos, AI::spiral_dist(4)) > geologist_sign_density_max){
      AILogDebug["do_remove_road_stubs"] << name << " flag at pos " << flag_pos << " is eligible because sign_density > max";
      eligible = true;
    }
    else {
      for (unsigned int i = 1; i < AI::spiral_dist(2); i++) {
        MapPos pos = map->pos_add_extended_spirally(flag_pos, i);
        if (map->has_flag(pos) && map->get_owner(pos) == player_index && game->get_flag_at_pos(pos)->is_connected()) {
          eligible = true;
          AILogDebug["do_remove_road_stubs"] << name << " flag at pos " << flag_pos << " is eligible because another connected flag is very close";
          break;
        }
      }
    }
    if (!eligible)
      continue;
    // see if it has only one path
    unsigned int paths = 0;
    Direction road_dir;
    for (Direction dir : cycle_directions_cw()) {
      if (!map->has_path(flag_pos, dir)) { continue; }
      paths++;
      if (paths > 1) {
        AILogDebug["do_remove_road_stubs"] << name << " eligible geologist road ending with flag at pos " << flag_pos << " has more than one path, not removing road";
        break;
      }
      road_dir = dir;
    }
    if (paths == 1) {
      AILogDebug["do_remove_road_stubs"] << name << " eligible geologist road ending with flag at pos " << flag_pos << " has only one path, removing the stub road and its end flag";
      game->demolish_road(map->move(flag_pos, road_dir), player);
      roads_removed++;
      game->demolish_flag(flag_pos, player);
    }
  }
  //
  // any non-mountain road that dead-ends and has no building attached
  //
  flags_static_copy = *(game->get_flags());
  flags = &flags_static_copy;
  for (Flag *flag : *flags) {
    if (flag == nullptr || flag->get_owner() != player_index || !flag->is_connected() || flag->has_building())
      continue;
    MapPos flag_pos = flag->get_position();
    if (AI::has_terrain_type(game, flag_pos, Map::TerrainTundra0, Map::TerrainSnow1))
      continue;
    // see if it has only one path
    unsigned int paths = 0;
    Direction road_dir;
    for (Direction dir : cycle_directions_cw()) {
      if (!map->has_path(flag_pos, dir)) { continue; }
      paths++;
      if (paths > 1) {
        //AILogDebug["do_remove_road_stubs"] << name << " eligible non-mountain road ending with flag at pos " << flag_pos << " has more than one path, not removing road";
        break;
      }
      road_dir = dir;
    }
    if (paths == 1) {
      AILogDebug["do_remove_road_stubs"] << name << " eligible non-mountain road ending with flag at pos " << flag_pos << " has only one path, removing the stub road and its end flag";
      game->demolish_road(map->move(flag_pos, road_dir), player);
      roads_removed++;
      game->demolish_flag(flag_pos, player);
    }
  }
  //
  // fully-constructed, occupied huts that have another flag nearby 
  //   that can be successfully connected
  //
  // look for huts that have an attached stub road that is >4 tiles or so long
  //  try connecting the other road first
  //  if successful, destroy the original road
  //
  if (loop_count % ai_loop_freq_adj_for_gamespeed(6) != 0) {
    AILogDebug["do_remove_road_stubs"] << " skipping eligible knight hut stub roads, only running this every X loops";
  }else{
    flags_static_copy = *(game->get_flags());
    flags = &flags_static_copy;
    for (Flag *flag : *flags) {
      if (flag == nullptr || flag->get_owner() != player_index || !flag->is_connected() || !flag->has_building())
        continue;
      if (flag->get_building()->get_type() != Building::TypeHut)
        continue;
      if (!flag->get_building()->is_done())
        continue;
      if (!flag->get_building()->has_knight())
        continue;
      MapPos flag_pos = flag->get_position();
      // store the Road found for later checking to prevent self-connection
      //  and also for length checking
      Road existing_road;
      // see if it has only one path
      unsigned int paths = 0;
      Direction road_dir = DirectionNone;
      for (Direction dir : cycle_directions_cw()) {
        if (!map->has_path(flag_pos, dir)) { continue; }

        paths++;
        if (paths > 1) {
          //AILogDebug["do_remove_road_stubs"] << name << " eligible knight hut stub road ending with flag at pos " << flag_pos << " has more than one path, not removing road";
          break;
        }
        // only consider removing if its road is over a certain length
        //  and also store the Road found for later checking to prevent self-connection
        existing_road = trace_existing_road(map, flag_pos, dir);
        if (existing_road.get_length() < 6){
          AILogDebug["do_remove_road_stubs"] << name << " eligible knight hut stub road ending with flag at pos " << flag_pos << " is too short to bother with, not removing road";
          break;
        }else{
          road_dir = dir;
        }
      }
      if (paths != 1) 
        continue;
      if (road_dir == DirectionNone)
        continue;
      // find a nearby connected flag
      //  or any place on an existing road that a flag could be built
      road_options.set(RoadOption::Direct);
      bool was_built = false;
      for (unsigned int i = 0; i < AI::spiral_dist(4); i++) {
        MapPos pos = map->pos_add_extended_spirally(flag->get_building()->get_position(), i);
        if (map->has_flag(pos) && pos != flag_pos && flag->get_owner() == player_index && flag->is_connected()
         || (game->can_build_flag(pos, player) && map->has_any_path(pos))) {
          AILogDebug["do_remove_road_stubs"] << name << " eligible knight hut stub road ending with flag at pos " << flag_pos << " with one path and suitable length, found nearby flag/pos at " << pos;
          if (existing_road.has_pos(map.get(), pos)){
            AILogDebug["do_remove_road_stubs"] << name << " eligible knight hut stub road ending with flag at pos " << flag_pos << " skipping pos " << pos << " as it is part of the existing road";
            continue;
          }
          Road notused; // not used here, can I just pass a zero instead of &notused to build_best_road and skip initialization of a wasted object?
          was_built = AI::build_best_road(flag->get_position(), road_options, &notused, Building::TypeNone, Building::TypeNone, pos, false);
          if (was_built){
            AILogDebug["do_remove_road_stubs"] << name << " eligible knight hut stub road ending with flag at pos " << flag_pos << ", successfully built replacement road, to flag/pos " << pos;
            break;
          }else{
            AILogDebug["do_remove_road_stubs"] << name << " eligible knight hut stub road ending with flag at pos " << flag_pos << ", failed to build replacement road to flag/pos " << pos << ", will keep trying";
          }
        }
      }
      road_options.reset(RoadOption::Direct);

      if (was_built){
        AILogDebug["do_remove_road_stubs"] << name << " eligible knight hut stub road ending with flag at pos " << flag_pos << " had replacement road built, destroying old road in dir " << NameDirection[road_dir] << " / " << road_dir;
        game->demolish_road(map->move(flag_pos, road_dir), player);
        roads_removed++;
        // sleep a bit to be more human like
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
      }
    }
  } // if only do knight stub removal every x loops

  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_remove_road_stubs"] << name << " call took " << duration;
  AILogDebug["do_remove_road_stubs"] << name << " done do_remove_road_stubs, removed " << roads_removed << " roads";

}


// demolish any stonecutters with no stones nearby
void
AI::do_demolish_unproductive_stonecutters() {
  AILogDebug["do_demolish_unproductive_stonecutters"] << name << " inside do_demolish_unproductive_stonecutters";
  ai_status.assign("HOUSEKEEPING - demolish stonecutters");
  AILogDebug["do_demolish_unproductive_stonecutters"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for demolish stonecutters)";
  game->get_mutex()->lock();
  AILogDebug["do_demolish_unproductive_stonecutters"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for demolish stonecutters)";
  Game::ListBuildings buildings = game->get_player_buildings(player);
  AILogDebug["do_demolish_unproductive_stonecutters"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player) (for demolish stonecutters)";
  game->get_mutex()->unlock();
  AILogDebug["do_demolish_unproductive_stonecutters"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player) (for demolish stonecutters)";
  for (Building *building : buildings) {
    if (building->get_type() != Building::TypeStonecutter)
      continue;
    MapPos pos = building->get_position();
    int stones_check = AI::count_stones_near_pos(pos, AI::spiral_dist(4));
    if (stones_check < 1) {
      AILogDebug["do_demolish_unproductive_stonecutters"] << name << " stonecutter at pos " << pos << " has no more stones nearby!  burning it";
      game->demolish_building(pos, player);
      // mark as bad pos, because there should be no reason it could ever become valid again (stone piles cannot regrow)
      bad_building_pos.insert(std::make_pair(pos, Building::TypeStonecutter));
    }
  }
  AILogDebug["do_demolish_unproductive_stonecutters"] << name << " done do_demolish_unproductive_stonecutters";
}


// demolish any completed mines that have food stored but are not productive
void
AI::do_demolish_unproductive_mines() {
  AILogDebug["do_demolish_unproductive_mines"] << name << " inside do_demolish_unproductive_mines";
  ai_status.assign("HOUSEKEEPING - demolish unproductive mines");
  AILogDebug["do_demolish_unproductive_mines"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for demolish unproductive mines)";
  game->get_mutex()->lock();
  AILogDebug["do_demolish_unproductive_mines"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for demolish unproductive mines)";
  Game::ListBuildings buildings = game->get_player_buildings(player);
  AILogDebug["do_demolish_unproductive_mines"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player) (for demolish unproductive mines)";
  game->get_mutex()->unlock();
  AILogDebug["do_demolish_unproductive_mines"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player) (for demolish unproductive mines)";
  for (Building *building : buildings) {
    if (!building->is_done())
      continue;
    Building::Type building_type = building->get_type();
    if (building_type != Building::TypeStoneMine && building_type != Building::TypeCoalMine
      && building_type != Building::TypeIronMine && building_type != Building::TypeGoldMine)
      continue;
    // I copied this from popup.cc, I don't understand how it works
    /* Calculate output percentage (simple WMA) */ // weighted moving average?
    const int output_weight[] = { 10, 10, 9, 9, 8, 8, 7, 7,  6, 6, 5, 5, 4, 3, 2, 1 };
    int output = 0;
    for (int i = 0; i < 15; i++) {
      output += !!BIT_TEST(building->get_progress(), i) * output_weight[i];
    }
    MapPos building_pos = building->get_position();
    AILogDebug["do_demolish_unproductive_mines"] << name << " mine of type " << NameBuilding[building_type] << " at pos " << building_pos << " has output % " << output;
    // need to avoid flagging newly built mines that haven't yet received food as they start with 0% productivity!
    //   here is how to game handles it, but I don't understand... do mines always expire after a certain progression (ticks?)
    //     or is this just a lazy catch that assumes any mine will become unproductive after a certain period, but could catch
    //     it earlier by watching productivity % ?
    /*  -game code from Notification MineEmpty-
    if (progress == 0x8000) {
    // Handle empty mine.
    Player *player = game->get_player(owner);
    if (player->is_ai()) {
        // TODO Burn building.
    }
    */
    // my way is to check the output and burn if unproductive but still having food (to avoid burning new mines)
    //  it seems that once a mine gets food it should become 10% active (unless it doesn't find anything then??)
    //if (building->is_active()) {
    //      AILogDebug["do_demolish_unproductive_mines"] << name << " mine at " << building_pos << " is active";
    //}
    AILogDebug["do_demolish_unproductive_mines"] << name << " mine at " << building_pos << " has " << building->get_res_count_in_stock(0) << " food stored";
    if (output < mine_output_min && building->get_res_count_in_stock(0) > 0) {
      AILogDebug["do_demolish_unproductive_mines"] << name << " burning unproductive mine of type " << NameBuilding[building_type] << name << " at pos " << building_pos;
      AILogDebug["do_demolish_unproductive_mines"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->demolish_building (for demolish unproductive mines)";
      game->get_mutex()->lock();
      AILogDebug["do_demolish_unproductive_mines"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->demolish_building (for demolish unproductive mines)";
      game->demolish_building(building_pos, player);
      AILogDebug["do_demolish_unproductive_mines"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->demolish_building (for demolish unproductive mines)";
      game->get_mutex()->unlock();
      AILogDebug["do_demolish_unproductive_mines"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->demolish_building (for demolish unproductive mines)";
      // mark as bad pos, because rebuilding same mine type seems pointless if it is actually out of resources
      bad_building_pos.insert(std::make_pair(building_pos, building_type));
    }
  }
  AILogDebug["do_demolish_unproductive_mines"] << name << " done do_demolish_unproductive_mines";
}


// burn all but one lumberjack per stock if planks_max reached, to avoid clogging roads
void
AI::do_demolish_excess_lumberjacks() {
  AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " inside do_demolish_excess_lumberjacks";
  ai_status.assign("HOUSEKEEPING - burn lumberjacks");
  update_building_counts();
  int lumberjack_count = stock_buildings.at(inventory_pos).count[Building::TypeLumberjack];
  unsigned int wood_count = stock_inv->get_count_of(Resource::TypePlank) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypePlank];
  wood_count += stock_inv->get_count_of(Resource::TypeLumber) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeLumber];
  if (wood_count >= (planks_max + anti_flapping_buffer) && lumberjack_count > 1) {
    AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " planks_max reached and lumberjack_count is " << lumberjack_count << ".  Burning all but one lumberjack (nearest to this stock)";
    bool first_one_found = false;
    AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player)";
    game->get_mutex()->lock();
    AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player)";
    Game::ListBuildings buildings = game->get_player_buildings(player);
    AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player)";
    game->get_mutex()->unlock();
    AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player)";
    for (Building *building : buildings) {
      if (building->get_type() == Building::TypeLumberjack) {
        MapPos pos = building->get_position();
        if (find_nearest_inventory(map, player_index, building->get_position(), DistType::FlagOnly, &ai_mark_pos) != inventory_pos)
          continue;
        if (building->is_done() && building->has_serf() && !first_one_found) {
          AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " the lumberjack at pos " << pos << " will be preserved and the rest destroyed";
          first_one_found = true;
        }
        else {
          AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " burning lumberjack at pos " << pos;
          AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player)";
          game->get_mutex()->lock();
          AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player)";
          game->demolish_building(pos, player);
          AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player)";
          game->get_mutex()->unlock();
          AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player)";
          // do NOT mark as bad pos
          //bad_building_pos.AI::do_demolish_excess_lumberjacks() {
        }
      }
    }
  }
  else {
        AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " planks_max not yet reached or no excess lumberjacks, skipping";
  }
}


// burn ALL fisherman huts, wheat farms, pig farms(?) attached to this stock if stock food_max reached, to avoid clogging roads
//  leave grain mills, bakers, butchees
// NOTE that food sitting at flags near the inventory counts also, but its contribution is capped
void
AI::do_demolish_excess_food_buildings() {
  AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " inside do_demolish_excess_food_buildings for stock at pos " << inventory_pos;
  ai_status.assign("HOUSEKEEPING - burn excess food buildings");
  unsigned int stored_food_count = 0;
  // most important are read-to-use food items stored in Inventory
  stored_food_count += stock_inv->get_count_of(Resource::TypeBread) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeBread];
  stored_food_count += stock_inv->get_count_of(Resource::TypeMeat) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeMeat];
  stored_food_count += stock_inv->get_count_of(Resource::TypeFish) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeFish];
  // include pigs, wheat, and flour because they ultimately will become food, but cap the amount they can contribute to the total
  //  to avoid situation where the processing buildings are missing
  unsigned int potential_food_count = 0;
  potential_food_count += stock_inv->get_count_of(Resource::TypePig) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypePig];
  potential_food_count += stock_inv->get_count_of(Resource::TypeWheat) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeWheat];
  potential_food_count += stock_inv->get_count_of(Resource::TypeFlour) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeFlour];
  if (potential_food_count > ((food_max + 1) / 2)){
    AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " capping contribution of potential_food_count to half of food_max";
    potential_food_count = (food_max + 1) / 2;
  }
  // include food and potential food sitting at flags nearby, but cap the amount they can contribute to the total
  //   in case they are stock in congestion or some other problem
  unsigned int food_at_flags = 0;
  food_at_flags += stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeBread];
  food_at_flags += stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeMeat];
  food_at_flags += stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeFish];
  food_at_flags += stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypePig];
  food_at_flags += stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeWheat];
  food_at_flags += stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeFlour];
  if (food_at_flags > ((food_max + 1) / 2)){
    AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " capping contribution of food_at_flags to half of food_max";
    food_at_flags = (food_max + 1) / 2;
  }

  unsigned int adjusted_food_count = stored_food_count;
  if (food_at_flags + potential_food_count > ((food_max + 1) / 3)*2 ){
    // the COMBINED contribution of potential food and food-at-flags can't be more than 75% of food_max
    AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " capping combined contribution of potential_food_count and food_at_flags to 66% of food_max";
    adjusted_food_count += ((food_max + 1) / 3)*2;
  }else{
    // not capped
    adjusted_food_count += food_at_flags + potential_food_count;
  }
  AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " DEBUG: stored_food_count " << stored_food_count << ", potential_food_count " << potential_food_count << ", food_at_flags " << food_at_flags << ", adjusted_food_count " << adjusted_food_count << ", food_max " << food_max;
  AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " adjusted food_count at inventory_pos " << inventory_pos << ": " << adjusted_food_count;
  if (adjusted_food_count > (food_max + anti_flapping_buffer)) {
    AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " food_max reached at inventory_pos " << inventory_pos << ", burning all food buildings attached to this stock";
    AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player)";
    game->get_mutex()->lock();
    AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player)";
    Game::ListBuildings buildings = game->get_player_buildings(player);
    AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player)";
    game->get_mutex()->unlock();
    AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player)";
    for (Building *building : buildings) {
      if (building->get_type() == Building::TypeFisher ||
		  building->get_type() == Building::TypeFarm ||
		  building->get_type() == Building::TypePigFarm) {
		    if (find_nearest_inventory(map, player_index, building->get_position(), DistType::FlagOnly, &ai_mark_pos) != inventory_pos)
          continue;
        AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " burning food building of type " << NameBuilding[building->get_type()] << " at pos " << building->get_position();
        AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player)";
        game->get_mutex()->lock();
        AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player)";
        game->demolish_building(building->get_position(), player);
        AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player)";
        game->get_mutex()->unlock();
        AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player)";
      }
    }
  }
  else {
    AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " food_max not yet reached at inventory_pos " << inventory_pos << ", skipping";
  }
  AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " done do_demolish_excess_food_buildings";
}


// adjust tool priorities to match needed tools, reset unneccessary tools to default
void
AI::do_manage_tool_priorities() {
  AILogDebug["do_manage_tool_priorities"] << name << " inside manage_tool_priorities";
  AILogDebug["do_manage_tool_priorities"] << name << " HouseKeeping: ensure sufficient tools";
  ai_status.assign("HOUSEKEEPING - manage tools");
  // allow default priorities to determine which tool to make if more than one type is needed
  AILogDebug["do_manage_tool_priorities"] << name << " resetting plank/steel priorities to default";
  player->reset_tool_priority();
  player->set_steel_toolmaker(0);
  player->set_steel_weaponsmith(65500);
  player->set_planks_toolmaker(0);
  need_tools = false;
  unsigned int planks_count = realm_inv[Resource::TypePlank];
  for (int i = 0; i < 20; ++i) {

    unsigned int idle = serfs_idle[(Serf::Type)i];
    unsigned int potential = serfs_potential[(Serf::Type)i];
    unsigned int available = idle + potential;
    //unsigned int total = serfs_total[(Serf::Type)i];
    unsigned int total = serfs_total[i];
    unsigned int min = specialists_reserve[Serf::Type(i)];
    unsigned int max = specialists_max[Serf::Type(i)];
    AILogDebug["do_manage_tool_priorities"] << name << " serf job " << NameSerf[i] << " idle: " << idle << ", potential: " << potential << ", available: " << available
          << ", total: " << total << ", min: " << min << ", max: " << max;
    //if (total >= max && max > 0) {
    //      AILogDebug["do_manage_tool_priorities"] << name << " maximum serfs with job type " << NameSerf[i] << name << " reached";
    //}
    // this caps the total number of tools & serfs with each job type to a fixed number
    //  that was good with single economy centered aroud castle, but fails with multiple economies w/ warehouses
    //   disabling this, going back to simple "ensure > 0"
    //if (total < max && available < min) {
    //  AILogDebug["do_manage_tool_priorities"] << name << " need more available serfs of job type " << NameSerf[i];
    //  need_tools = true;
    //}
    if (available < 1) {
      AILogDebug["do_manage_tool_priorities"] << name << " need more available serfs of job type " << NameSerf[i];
      // boats (Serf::TypeSailor) are *not* made by toolmaker
      if (i == Serf::TypeSailor)
        continue;
      need_tools = true;
    }
  }
  update_building_counts();
  // this is one toolmaker in entire REALM, but that is okay
  if (realm_building_count[Building::TypeToolMaker] < 1) {
    AILogDebug["do_manage_tool_priorities"] << name << " no toolmaker exists yet!";
  }
  else if (need_tools) {
    for (int i = 0; i < 9; ++i) {
      // tools Resource::Types are and 15-24
      int tool_index = 15 + i;
      int tool_count = realm_inv[Resource::Type(tool_index)];
      AILogDebug["do_manage_tool_priorities"] << name << " castle has " << tool_count << " of tool " << NameTool[i];
      if (tool_count >= 1) {
        // don't need this tool, set to zero prior
        player->set_tool_prio(i, 0);
        continue;
      }
      AILogDebug["do_manage_tool_priorities"] << name << " need to make tool: " << NameTool[i];
      // this shouldn't even be necessary as all tools have nonzero starting priority
      //player->set_tool_prio(i, 65500);
    }

    // shortcut to ensure farmer scythe tool is created first on very low resource starts
    if (serfs_potential[Serf::TypeFarmer] + player->get_serf_count(Serf::TypeFarmer) < 1) {
      AILogDebug["do_manage_tool_priorities"] << name << " No farmer and no scythe!  Setting scythe to priority and others to zero";
      for (int i = 0; i < 9; ++i) {
        player->set_tool_prio(i, 0);
      }
      // scythe is tool #4 (fifth item in list - see NameTool lookup table)
      player->set_tool_prio(4, 65500);
    }
    AILogDebug["do_manage_tool_priorities"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for manage toolmaker)";
    game->get_mutex()->lock();
    AILogDebug["do_manage_tool_priorities"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for manage toolmaker)";
    Game::ListBuildings buildings = game->get_player_buildings(player);
    AILogDebug["do_manage_tool_priorities"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player) (for manage toolmaker)";
    game->get_mutex()->unlock();
    AILogDebug["do_manage_tool_priorities"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player) (for manage toolmaker)";
    for (Building *building : buildings) {
      if (building->get_type() != Building::TypeToolMaker)
        continue;

      MapPos pos = building->get_position();
      unsigned int planks = building->get_res_count_in_stock(0);
      if (planks <= 1 && planks_count >= planks_crit) {
        AILogDebug["do_manage_tool_priorities"] << name << " toolmaker at pos " << pos << " has only " << planks << " planks, setting priority to max and reducing construction priority";
        player->set_planks_toolmaker(65500);
        player->set_planks_construction(32750);
      }
      unsigned int steel = building->get_res_count_in_stock(1);
      if (steel <= 1) {
        AILogDebug["do_manage_tool_priorities"] << name << " toolmaker at pos " << pos << " has only " << steel << " steel, setting priority to max and zeroing blacksmith priority";
        player->set_steel_toolmaker(65500);
        player->set_steel_weaponsmith(0);
      }
    }
  }
  else {
    AILogDebug["do_manage_tool_priorities"] << name << " have toolmaker but don't need any tools yet";
  }
}


void
AI::do_manage_mine_food_priorities() {
  AILogDebug["do_manage_mine_food_priorities"] << name << " inside do_manage_mine_food_priorities";
  ai_status.assign("HOUSEKEEPING - manage mine food");
  // if sufficient coal/ore is stored, divert food to other resource miners
  player->reset_food_priority();
  unsigned int coal_count = realm_inv[Resource::TypeCoal];
  unsigned int iron_ore_count = realm_inv[Resource::TypeIronOre];
  unsigned int gold_ore_count = realm_inv[Resource::TypeGoldOre];
  //6550 is a near-zero value I chose to use as "very low priority"
  // default food priorities:
  //food_stonemine = 13100;
  //food_coalmine = 45850;
  //food_ironmine = 45850;
  //food_goldmine = 65500;
  // coal
  if (coal_count > coal_max) {
    AILogDebug["do_manage_mine_food_priorities"] << name << " coal_count " << coal_count << " is greater than coal_max " << coal_min << ", setting coal mine food priority to zero";
    player->set_food_coalmine(0);
  }else if (coal_count > coal_min) {
    AILogDebug["do_manage_mine_food_priorities"] << name << " coal_count " << coal_count << " is greater than coal_min " << coal_min << ", greatly reducing food priority to coal mines";
    player->set_food_coalmine(6550);
  }
  // iron
  if (iron_ore_count > iron_ore_max) {
    AILogDebug["do_manage_mine_food_priorities"] << name << " iron_ore_count " << iron_ore_count << " is greater than iron_ore_max " << iron_ore_max << ", setting iron mine food priority to zero";
    player->set_food_ironmine(0);
  }else if (iron_ore_count > iron_ore_min) {
    AILogDebug["do_manage_mine_food_priorities"] << name << " iron_ore_count " << iron_ore_count << " is greater than iron_ore_min " << iron_ore_min << ", greatly reducing food priority to iron mines";
    player->set_food_ironmine(6550);
  }
  // gold
  if (gold_ore_count > gold_ore_max) {
    AILogDebug["do_manage_mine_food_priorities"] << name << " gold_ore_count " << gold_ore_count << " is greater than gold_ore_max " << gold_ore_max << ", setting gold mine food priority to zero";
    player->set_food_goldmine(0);
  } else if (gold_ore_count > gold_ore_min) {
    AILogDebug["do_manage_mine_food_priorities"] << name << " gold_ore_count " << gold_ore_count << " is greater than gold_ore_min " << gold_ore_min << ", greatly reducing food priority to gold mines";
    player->set_food_goldmine(6550);
  }
  // avoid issue where all food goes to gold mining while running out of knights
  unsigned int idle_knights = serfs_idle[Serf::TypeKnight0] + serfs_idle[Serf::TypeKnight1] + serfs_idle[Serf::TypeKnight2] + serfs_idle[Serf::TypeKnight3] + serfs_idle[Serf::TypeKnight4];
  if (idle_knights <= knights_min) {
    // set gold food prio to less than coal or iron would be if they are needed
    player->set_food_goldmine(25000);
  }
}

// ensure equal numbers of swords and shields stored
//    because swords default to a higher serf-transport priority than shields, over time
//    significantly more swords will be stored than shields while shields sit on roads/flags
//     If they are balanced, more knights can be created
// NOTE - this function seems to result in a graphical bug in the priority list if viewed in game,
//    where the sword/shield icon is mising.  However I am quite sure that the function still works properly
void
AI::do_balance_sword_shield_priorities() {
  AILogDebug["do_balance_sword_shield_priorities"] << name << " inside do_balance_sword_shield_priorities";
  ai_status.assign("HOUSEKEEPING - balance swords/shields");
  player->reset_flag_priority();
  int *prio = nullptr;
  // to adjust priorites, first get the pointer to the flag_prio array...
  prio = player->get_flag_prio();
  // ...then directly set the priority of the item by id
  //flag_prio[Resource::TypeShield] = 16;
  //flag_prio[Resource::TypeSword] = 17;
  unsigned int swords_count = realm_inv[Resource::TypeSword];
  unsigned int shields_count = realm_inv[Resource::TypeShield];
  AILogDebug["do_balance_sword_shield_priorities"] << name << " there are " << swords_count << " swords and " << shields_count << " shields stored";
  // actually, rather than worry about the overall ordering, just swap the two values
  if (swords_count >= shields_count + 2) {
    AILogDebug["do_balance_sword_shield_priorities"] << name << " more swords than shields stored";
    if (prio[Resource::TypeSword] > prio[Resource::TypeShield]) {
      int temp = prio[Resource::TypeSword];
      prio[Resource::TypeShield] = prio[Resource::TypeSword];
      prio[Resource::TypeSword] = temp;
      AILogDebug["do_balance_sword_shield_priorities"] << name << " sword priority swapped with shield priority, sword priority is now " << prio[Resource::TypeSword] << ", shield priority is now " << prio[Resource::TypeShield];
    }
    else {
      AILogDebug["do_balance_sword_shield_priorities"] << name << " sword priority " << prio[Resource::TypeSword] << " is already higher than shield priority " << prio[Resource::TypeShield] << ", no change needed";
    }
  }
  else if (shields_count >= swords_count + 2) {
    AILogDebug["do_balance_sword_shield_priorities"] << name << " more shields than swords stored";
    if (prio[Resource::TypeShield] > prio[Resource::TypeSword]) {
      int temp = prio[Resource::TypeShield];
      prio[Resource::TypeSword] = prio[Resource::TypeShield];
      prio[Resource::TypeShield] = temp;
      AILogDebug["do_balance_sword_shield_priorities"] << name << " shield priority swapped with sword priority, sword priority is now " << prio[Resource::TypeSword] << ", shield priority is now " << prio[Resource::TypeShield];
    }
    else {
      AILogDebug["do_balance_sword_shield_priorities"] << name << " shield priority " << prio[Resource::TypeShield] << " is already higher than sword priority " << prio[Resource::TypeSword] << ", no change needed";
    }
  }
  else {
    AILogDebug["do_balance_sword_shield_priorities"] << name << " swords and shields are sufficiently balanced, leaving sword priority at " << prio[Resource::TypeSword] << " and shield priority at " << prio[Resource::TypeShield];
  }
}


void
AI::do_manage_knight_occupation_levels() {
  AILogDebug["do_manage_knight_occupation_levels"] << name << " inside do_manage_knight_occupation_levels";
  if (player->cycling_knight()) {
    AILogDebug["do_manage_knight_occupation_levels"] << name << " is currently cycling_knights!  waiting until this is complete";
    return;
  }
  // adjust military building occupation levels
  //   to avoid bouncing knights back and forth, only adjust occupation levels
  //     if doing so will not push it back across the same threshold
  //
  //        Hut Tower     Garrison
  //      Full    3       6       12
  //      Good    2       4       9
  //      Medium  2       3       6
  //      Weak    1       2       3
  //      Minimum 1       1       1
  //
  // Ai initializes the lowest three threat levels at min, and they never have to be adjusted
  //
  //   because there is never a good reason to increase these (unless map is enormous, maybe?)
  //      Frontier - thick cross        = 3   *this is the one we adjust
  //      Second line - thin cross      = 2
  //      Interior - thin line          = 1
  //      Interior / safe - white flag  = 0
  //
  // get current occupation level setting (regardless of how filled the buildings actually are!)
  //   freeserf knight_occupation values are goofy hex stuff for bit-math!!!
  // initial values for four threat levels:
  //knight_occupation[0] = 0x10;
  //knight_occupation[1] = 0x21;
  //knight_occupation[2] = 0x32;
  //knight_occupation[3] = 0x43;
  // get_knight_occupation returns a value that must be bit-shifted
  //   here's how to get the correct 0-4 value we work with using player->change_knight_occupation
  int current_level = (player->get_knight_occupation(3) >> 4) & 0xf;
  AILogDebug["do_manage_knight_occupation_levels"] << name << " current knight occupation level: " << current_level;
  // to avoid flapping...
  //  ...if this is not the very first occupation level change...
  if (previous_knight_occupation_level > 0) {
    // ...and we last LOWERED the level
    if (previous_knight_occupation_level > current_level) {
      change_buffer = static_cast<int>(knight_occupation_change_buffer) * -1;
      AILogDebug["do_manage_knight_occupation_levels"] << name << " knight occupation level last DECREASED from " << previous_knight_occupation_level << " to "
        << current_level << ", setting a change buffer of " << change_buffer;
    }
    // ...and we last RAISED the occupation level
    if (previous_knight_occupation_level < current_level) {
      change_buffer = static_cast<int>(knight_occupation_change_buffer);
      AILogDebug["do_manage_knight_occupation_levels"] << name << " knight occupation level last INCREASED from " << previous_knight_occupation_level << " to "
        << current_level << ", setting a change buffer of " << change_buffer;
    }
  }
  previous_knight_occupation_level = current_level;
  AILogDebug["do_manage_knight_occupation_levels"] << name << " knight occupation change_buffer is currently set to " << change_buffer;
  //
  // NEED TO CHANGE THIS WHOLE FUNCTION
  //   make it determine current desired level without changing it
  //    compare to current level and only change it as required, do NOT reset it to zero first
  //     I suspect this is causing knights to shuffle in out... because it is being set to min for long enough the game update tells the knights to exit?
  //      this should only happen if count_knights_affected_by_occupation_level_change is taking a long time (more than the game update cycle)
  //       ... which it might if update_building_counts uses mutex so it could be locked in the middle...
  //
  // get current occupation setting for thread level Frontier (thick cross) - closest to enemy
  //player->get_knight_occupation(3);
  // this is counting all knights in *any* building, but we only care about idle in castle/stock
  //unsigned int idle_knights = serfs_idle[Serf::TypeKnight0] + serfs_idle[Serf::TypeKnight1] + serfs_idle[Serf::TypeKnight2] + serfs_idle[Serf::TypeKnight3] + serfs_idle[Serf::TypeKnight4];
  // instead use this function I wrote elsewhere
  unsigned int idle_knights = 0;
  AILogDebug["do_manage_knight_occupation_levels"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_serfs(player) (for do_manage_knight_occupation_levels is_waiting)";
  game->get_mutex()->lock();
  AILogDebug["do_manage_knight_occupation_levels"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_serfs(player) (for do_manage_knight_occupation_levels is_waiting)";
  for (Serf *serf : game->get_player_serfs(player)) {
    if (serf->get_state() == Serf::StateIdleInStock && serf->get_type() >= Serf::TypeKnight0 && serf->get_type() <= Serf::TypeKnight4) {
      idle_knights++;
    }
  }
  AILogDebug["do_manage_knight_occupation_levels"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_serfs(player) (for do_manage_knight_occupation_levels is_waiting)";
  game->get_mutex()->unlock();
  AILogDebug["do_manage_knight_occupation_levels"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_serfs(player) (for do_manage_knight_occupation_levels is_waiting)";
  AILogDebug["do_manage_knight_occupation_levels"] << name << " found in stocks idle_knights: " << idle_knights;
  player->change_knight_occupation(3, 0, -5);   // reset lower bound to 'min'
  player->change_knight_occupation(3, 1, -5);   // reset upper bound to 'min'
  // must evaluate idle_knights as signed integer to avoid calculating negative values returning opposite result
  if      ((signed)idle_knights - (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 4) + (signed)change_buffer >= (signed)knights_max) {
    //AILogDebug["do_manage_knight_occupation_levels"] << name << " debug4: " << (signed)idle_knights << " - " << (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 4) << " + " << change_buffer
    //  << " = " << (signed)idle_knights - (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 4) + change_buffer << " ?? " << knights_max;
    AILogDebug["do_manage_knight_occupation_levels"] << name << " setting knight level to med/full";
    player->change_knight_occupation(3, 1, +4);   // increase upper bound to 'full'
    player->change_knight_occupation(3, 0, +2);   // increase lower bound to 'med'
  }
  else if ((signed)idle_knights - (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 3) + (signed)change_buffer >= (signed)knights_med) {
    //AILogDebug["do_manage_knight_occupation_levels"] << name << " debug3: " << (signed)idle_knights << " - " << (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 3) << " + " << change_buffer
    //  << " = " << (signed)idle_knights - (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 3) + change_buffer << " ?? " << knights_med;
    AILogDebug["do_manage_knight_occupation_levels"] << name << " setting knight level to weak/good";
    player->change_knight_occupation(3, 1, +3);   // increase upper bound to 'good'
    player->change_knight_occupation(3, 0, +1);   // increase lower bound to 'weak'
  }
  else if ((signed)idle_knights - (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 2) + (signed)change_buffer >= (signed)knights_min) {
    //AILogDebug["do_manage_knight_occupation_levels"] << name << " debug2: " << (signed)idle_knights << " - " << (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 2) << " + " << change_buffer
    //  << " = " << (signed)idle_knights - (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 2) + change_buffer << " ?? " << knights_min;
    AILogDebug["do_manage_knight_occupation_levels"] << name << " setting knight level to min/med";
    player->change_knight_occupation(3, 1, +2);   // increase upper bound to 'med'
    // leave lower bound at 'min'
  }
  else {
    AILogDebug["do_manage_knight_occupation_levels"] << name << " setting knight level to min/min";
    // leave both at 'min'
  }
}


void
AI::do_place_mines(std::string type, Building::Type building_type, Map::Object large_sign, Map::Object small_sign, int max_mines, double sign_density_min) {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  ai_status.assign("MAIN LOOP - early mine placement - " + type);
  AILogDebug["do_place_mines"] << inventory_pos << " inside do_place_mines() with type " << type << ", building_type " << NameBuilding[building_type] <<
    ", large_sign " << NameObject[large_sign] << ", small_sign " << NameObject[small_sign] << ", max_mines " << max_mines << ", sign_density_min " << sign_density_min;
  update_building_counts();
  int mine_count = stock_buildings.at(inventory_pos).count[building_type];
  if (mine_count < max_mines) {
    MapPosSet count_by_corner;
    // for mined resouce finding,  reverse the occupied_military_buildings order
    //   so the newest military building is searched first,  instead of castle first
    MapPosVector foo = stock_buildings.at(inventory_pos).occupied_military_pos;
    // for some reason after switching to parallel stocks/warehouse support, cannot use stock_buildings.at(inventory_pos).occupied_militar_pos iterator directly
    //  must copy it to another MapPosVector and iterate over that.  I'm sure there is a cleaner way
    //for (MapPosVector::reverse_iterator it = stock_buildings.at(inventory_pos).occupied_military_pos.rbegin(); it != stock_buildings.at(inventory_pos).occupied_military_pos.rend(); ++it) {
    for (MapPosVector::reverse_iterator it = foo.rbegin(); it != foo.rend(); ++it) {
      MapPos center_pos = *it;
      MapPosVector corners = AI::get_corners(center_pos);
      for (MapPos corner_pos : corners) {
        // count the number of signs of any type
        double signs_count = AI::count_objects_near_pos(corner_pos, AI::spiral_dist(4), Map::ObjectSignLargeGold, Map::ObjectSignSmallStone, "dk_orange");
        // count the number of empty hills (no blocking objects)
        double empty_hills_count = AI::count_empty_terrain_near_pos(corner_pos, AI::spiral_dist(4), Map::TerrainTundra0, Map::TerrainSnow0, "orange");
        if (signs_count < 1 || empty_hills_count < 1)
          continue;
        double sign_density = signs_count / empty_hills_count;
        AILogVerbose["do_place_mines"] << inventory_pos << " " << type << " mine: corner with center pos " << corner_pos << " has signs_count: " << signs_count << ", empty_hills_count: " << empty_hills_count << ", sign_density: " << sign_density << ", min is " << sign_density_min;
        if (sign_density >= sign_density_min) {
          AILogVerbose["do_place_mines"] << inventory_pos << " sign density " << sign_density << " is over sign_density_min " << sign_density_min;
        }
        // don't place a mine if there are already another mine of the same type nearby, it is inefficient because of
        //  the way that underground mined resources are depleted, if the first runs out the second will find little or nothing
        int found = 0;
        for (unsigned int i = 0; i < AI::spiral_dist(12); i++) {
          MapPos pos = map->pos_add_extended_spirally(corner_pos, i);
          if (map->has_building(pos) && map->get_owner(pos) == player_index){
            Building *building = game->get_building_at_pos(pos);
            if (building != nullptr){
              if (building->get_type() == building_type){
                AILogVerbose["do_place_mines"] << inventory_pos << " found another mine of type " << NameBuilding[building_type] << " near where looking to place another, not building";
                found++;
                break;
              }
            }
          }
        }
        if (found){
          AILogDebug["do_place_mines"] << inventory_pos << " found at least two other mines of same type " << NameBuilding[building_type] << " nearby, not placing another.  Skipping this corner area";
          continue;
        }
        // try to build a mine on a Large resource sign if found,
        //    or if a Small resource find and sign_ratio is high enough
        // note - because we are checking even if blank signs found, this is sub-optimal.
        //   could improve by calculating sign_density in a way that lets us ignore blank
        //    signs when determining if trying to place mine
        AILogVerbose["do_place_mines"] << inventory_pos << " " << type << " mine: looking for a place to build a " << type << " mine near corner " << corner_pos;
        for (unsigned int i = 0; i < AI::spiral_dist(4); i++) {
          MapPos pos = map->pos_add_extended_spirally(corner_pos, i);
          if (!game->can_build_mine(pos)) {
            continue;
          }
          if (map->get_owner(pos) != player_index) {
            continue;
          }
          Map::Object obj = map->get_obj(pos);
          if (obj == large_sign || (obj == small_sign && sign_density >= sign_density_min)) {
            AILogInfo["do_place_mines"] << inventory_pos << " trying to build " << type << " mine at pos " << pos;
            MapPos built_pos = bad_map_pos;
            // note that distance = 1 means ONLY THIS SPOT
            built_pos = AI::build_near_pos(pos, 1, building_type);
            if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) {
              AILogInfo["do_place_mines"] << inventory_pos << " built " << type << " mine at pos " << built_pos;
              break;
            }
            if (built_pos == stopbuilding_pos) { return; }
          }
        } // foreach hills pos spirally
        update_building_counts();
        mine_count = stock_buildings.at(inventory_pos).count[building_type];
        if (mine_count >= max_mines) {
          AILogDebug["do_place_mines"] << inventory_pos << " Already placed " << type << " mine, not building more";
          return;
        }
      } // foreach corner
    } // foreach military building
  } // if < max_mines
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_place_mines"] << inventory_pos << " plot do_place_mines call took " << duration;
}


void
AI::do_place_coal_mines() {
  AILogDebug["do_place_coal_mines"] << name << " inside do_place_coal_mines()";
  do_place_mines("coal", Building::TypeCoalMine, Map::ObjectSignLargeCoal, Map::ObjectSignSmallCoal, max_coalmines, coal_sign_density_min);
}

void
AI::do_place_iron_mines() {
  AILogDebug["do_place_iron_mines"] << name << " inside do_place_iron_mines()";
  do_place_mines("iron", Building::TypeIronMine, Map::ObjectSignLargeIron, Map::ObjectSignSmallIron, max_ironmines, iron_sign_density_min);
}

void
AI::do_place_gold_mines(){
  AILogDebug["do_place_gold_mines"] << name << " inside do_place_gold_mines()";
  do_place_mines("gold", Building::TypeGoldMine, Map::ObjectSignLargeGold, Map::ObjectSignSmallGold, max_goldmines, gold_sign_density_min);
}



// build a sawmill and two lumberjacks in area with most trees
void
AI::do_build_sawmill_lumberjacks() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " inside do_build_sawmill_lumberjacks";
  ai_status.assign("MAIN LOOP - wood");
  unsigned int wood_count = stock_inv->get_count_of(Resource::TypePlank) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypePlank];
  // include raw logs as they will become planks
  wood_count += stock_inv->get_count_of(Resource::TypeLumber) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeLumber];
  int sawmill_count = 0;
  int lumberjack_count = 0;
  if (wood_count < (planks_max - anti_flapping_buffer)) {
    AILogInfo["do_build_sawmill_lumberjacks"] << inventory_pos << " AI: desire more planks";
    // count trees around corners of each military building, starting with castle
    MapPosSet count_by_corner;
    MapPos built_pos = bad_map_pos;
    for (MapPos center_pos : stock_buildings.at(inventory_pos).occupied_military_pos) {
      update_building_counts();
      sawmill_count = stock_buildings.at(inventory_pos).count[Building::TypeSawmill];
      lumberjack_count = stock_buildings.at(inventory_pos).count[Building::TypeLumberjack];
      if (sawmill_count >= 1 && lumberjack_count >= 2) {
        AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " Already placed sawmill and lumberjacks, not building more";
        return;
      }
      if (sawmill_count < 1) {
        // build a list of corners by tree count and sort
        MapPosVector corners = AI::get_corners(center_pos);
        for (MapPos corner_pos : corners) {
          unsigned int count = AI::count_objects_near_pos(corner_pos, AI::spiral_dist(4), Map::ObjectTree0, Map::ObjectPine7, "lt_green");
          //AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " corner has count: " << count << ", min acceptable is " << AI::near_trees_min;
          if (count >= near_trees_min) {
            count_by_corner.insert(std::make_pair(corner_pos, count));
          }
        }
        // build sawmill near corner with most trees
        MapPosVector search_positions = AI::sort_by_val_desc(count_by_corner);
        for (MapPos corner_pos : search_positions) {
          AILogInfo["do_build_sawmill_lumberjacks"] << inventory_pos << " try to build sawmill near pos " << corner_pos;
          //built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(6), Building::TypeSawmill);
          built_pos = AI::build_near_pos(corner_pos, DIRECTIONAL_FILL_POS_MAX, Building::TypeSawmill, get_dir_from_corner(center_pos, corner_pos));
          if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) {
            AILogInfo["do_build_sawmill_lumberjacks"] << inventory_pos << " built sawmill at pos " << built_pos;
            break;
          }
          if (built_pos == stopbuilding_pos) { return; }
        }
      }
      update_building_counts();
      sawmill_count = stock_buildings.at(inventory_pos).count[Building::TypeSawmill];
      if (sawmill_count > 0) {
        //
        // build two lumberjacks near corner where sawmill was built, or corner with most trees
        //
        MapPosVector search_positions = AI::sort_by_val_desc(count_by_corner);
        //  push the location of the sawmill (built_pos) to the front of the search path to help keep lumberjacks close
        search_positions.insert(search_positions.begin(), built_pos);
        for (MapPos search_pos : search_positions) {
          AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " DEBUG checking search_pos " << search_pos;
          // try to build two
          for (int x = 0; x < 2; x++) {
            update_building_counts();
            if (stock_buildings.at(inventory_pos).count[Building::TypeLumberjack] >= 2) {
              break;
            }
            AILogInfo["do_build_sawmill_lumberjacks"] << inventory_pos << " trying to build a lumberjack near pos " << search_pos;
            built_pos = bad_map_pos;
            built_pos = AI::build_near_pos(search_pos, AI::spiral_dist(4), Building::TypeLumberjack);
            if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) {
              AILogInfo["do_build_sawmill_lumberjacks"] << inventory_pos << " built lumberjack at pos " << built_pos;
            }
            if (built_pos == stopbuilding_pos) {
              return;
            }
          }
          update_building_counts();
          if (stock_buildings.at(inventory_pos).count[Building::TypeLumberjack] >= 2) {
            break;
          }
        }
      } //end if have sawmill
    } // foreach military building

    update_building_counts();
    lumberjack_count = stock_buildings.at(inventory_pos).count[Building::TypeLumberjack];
    if (sawmill_count < 1 || lumberjack_count < 2) {
      AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " couldn't place all of 1 sawmill and 2 lumberjacks!  expands towards some trees";
      expand_towards.insert("trees");
      AI::expand_borders(castle_pos);
    }
  }
  else {
    AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " have sufficient planks, skipping";
  }
  AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " done do_build_sawmill_lumberjacks";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " done do_build_sawmill_lumberjacks call took " << duration;
}


// stop AI loop here if at least one sawmill an at least one lumberjack are not fully built
//    OR at least until all wood/stone delivered for sawmill AND at least one lumberjack fully built
// return false if need to wait, true if ready to continue
bool
AI::do_wait_until_sawmill_lumberjacks_built() {
  AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " inside do_wait_until_sawmill_lumberjacks_built";
  unsigned int planks_count = stock_inv->get_count_of(Resource::TypePlank);
  if (planks_count >= planks_min) {
    AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " have enough planks - no need to wait, continuing";
    return true;
  }
  AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " planks < planks_min, checking sawmill and lumberjack build state";
  bool have_sawmill = false;
  bool have_lumberjack = false;
  bool sawmill_has_stones = false;
  bool sawmill_has_planks = false;
  AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for wait until sawmill & lumberjack built)";
  game->get_mutex()->lock();
  AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for wait until sawmill & lumberjack built)";
  Game::ListBuildings buildings = game->get_player_buildings(player);
  AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player) (for wait until sawmill & lumberjack built)";
  game->get_mutex()->unlock();
  AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player) (for wait until sawmill & lumberjack built)";
  for (Building *building : buildings) {
    if (building->get_type() == Building::TypeSawmill && !building->is_done()) {
      // "waiting_XXXX" seems to mean that the resource has arrived and is waiting for builder to consume, rather than waiting for delivery
      AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " Sawmill under construction has waiting_planks: " << building->waiting_planks() << " and waiting_stones " << building->waiting_stone();
      AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " Sawmill under construction building progress: " << building->get_progress();
      // building progress 0 means leveling not yet complete
      // building progress 1 means leveling complete
      // building progress >1 means builder arrived and has received at least first plank and begun building
      // no way I know of to check if a builder is in place but no materials delivered yet.   Might be possible, didn't look
      // 16385 = 1st plank consumed, half framing done
      // 32769 = 2nd plank consumed, framing complete
      // 43697 = 1st stone consumed, 1/3 exterior complete
      // 54625 = 2nd stone consumed, 2/3 exterior complete
      // 3rd plank used for remainder of exterior, then building complete
      if (building->get_progress() > 1 || building->waiting_planks() >= 1) {
        AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " Sawmill under construction received at least one plank";
      }
      if (building->get_progress() > 16385 || building->waiting_planks() >= 2
        || (building->get_progress() > 1 && building->waiting_planks() >= 1)) {
        AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " Sawmill under construction received at least two planks";
      }
      if (building->get_progress() > 32769 || building->waiting_stone() >= 1) {
        AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " Sawmill under construction received at least one stone";
      }
      if (building->get_progress() > 43697 || building->waiting_stone() >= 2
        || (building->get_progress() > 32769 && building->waiting_stone() >= 1)) {
        AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " Sawmill under construction received all two stones";
        sawmill_has_stones = true;
      }
      if (building->get_progress() > 54625 || building->waiting_planks() >= 3
        || (building->get_progress() > 16385 && building->waiting_planks() >= 1)
        || (building->get_progress() > 1 && building->waiting_planks() >= 2)) {
        AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " Sawmill under construction received all three planks";
        sawmill_has_planks = true;
      }
    }
    // if sawmill has all materials delivered that is good enough to consider it complete for this check
    if (building->get_type() == Building::TypeSawmill && (building->is_done() || (sawmill_has_planks && sawmill_has_stones)))
      have_sawmill = true;
    if (building->get_type() == Building::TypeLumberjack && building->is_done())
      have_lumberjack = true;
    if (have_sawmill && have_lumberjack)
      return true;
  }
  AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " no sawmill and lumberjacks completed, must wait.  Returning false";
  return false;
}


// build a stonecutter near area with most stones
//   note that stone *mines* are NEVER built - maybe in future AI improvement
void
AI::do_build_stonecutter() {
  AILogDebug["do_build_stonecutter"] << inventory_pos << " Main Loop - stones & stonecutters";
  ai_status.assign("MAIN LOOP - stone");
  //ai_mark_pos.clear();
  unsigned int stones_count = stock_inv->get_count_of(Resource::TypeStone);
  //AILogDebug["do_build_stonecutter"] << inventory_pos << " has " << stones_count << " stones in this stock";
  if (stones_count < stones_max) {
    AILogInfo["do_build_stonecutter"] << inventory_pos << " AI: desire more stones";
    // count stones near military buildings
    MapPosSet count_by_corner;
    for (MapPos center_pos : stock_buildings.at(inventory_pos).occupied_military_pos) {
      update_building_counts();
      int stonecutter_count = stock_buildings.at(inventory_pos).count[Building::TypeStonecutter];
      if (stonecutter_count >= 1) {
        AILogDebug["do_build_stonecutter"] << inventory_pos << " Already placed stonecutter, not building more";
        return;
      }
      MapPosVector corners = AI::get_corners(center_pos);
      for (MapPos corner_pos : corners) {
        unsigned int count = AI::count_stones_near_pos(corner_pos, AI::spiral_dist(4));
        //AILogDebug["do_build_stonecutter"] << inventory_pos << " corner has count: " << count << ", min acceptable is " << near_stones_min;
        if (count >= near_stones_min) {
          count_by_corner.insert(std::make_pair(corner_pos, count));
        }
      }
      // build stonecutter near corner with most stones
      MapPosVector search_positions = AI::sort_by_val_desc(count_by_corner);
      MapPos built_pos = bad_map_pos;
      for (MapPos corner_pos : search_positions) {
        //ai_mark_pos.clear();
        AILogInfo["do_build_stonecutter"] << inventory_pos << " try to build stonecutter near corner pos " << corner_pos;
        // to avoid infinite loop bug where stonecutter is built a bit to far from stones and immediately demolished, need to check each potential build pos for suitability
        for (unsigned int i = 0; i < AI::spiral_dist(4); i++) {
          MapPos pos = map->pos_add_extended_spirally(corner_pos, i);
          //unsigned int count = AI::count_stones_near_pos(pos, AI::spiral_dist(4), Map::ObjectStone0, Map::ObjectStone7, "gray");
          unsigned int count = AI::count_stones_near_pos(pos, AI::spiral_dist(4));
          if (count < near_stones_min) {
            continue;
          }
          // try each specific pos one at a time
          AILogInfo["do_build_stonecutter"] << inventory_pos << " trying to build stonecutter near pos " << pos;
          built_pos = AI::build_near_pos(pos, 1, Building::TypeStonecutter);
          if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) {
            AILogInfo["do_build_stonecutter"] << inventory_pos << " built stonecutter at pos " << built_pos;
            break;
          }
          if (built_pos == stopbuilding_pos) { return; }
        }
        if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) { break; }
      }
    } // foreach military building
    update_building_counts();
    int stonecutter_count = stock_buildings.at(inventory_pos).count[Building::TypeStonecutter];
    if (stonecutter_count < 1) {
      AILogDebug["do_build_stonecutter"] << inventory_pos << " couldn't place stonecutter,  expand towards some stones";
      expand_towards.insert("stones");
      AI::expand_borders(castle_pos);
    }
  }
  else {
    AILogDebug["do_build_stonecutter"] << inventory_pos << " have sufficient stones, skipping";
  }
  AILogDebug["do_build_stonecutter"] << inventory_pos << " done do_build_stonecutter";
}


// expand borders to create defensive buffer
void
AI::do_create_defensive_buffer() {
  AILogDebug["do_create_defensive_buffer"] << inventory_pos << " inside do_create_defensive_buffer";
  expand_towards.insert("create_buffer");
  unsigned int idle_knights = serfs_idle[Serf::TypeKnight0] + serfs_idle[Serf::TypeKnight1] + serfs_idle[Serf::TypeKnight2] + serfs_idle[Serf::TypeKnight3] + serfs_idle[Serf::TypeKnight4];
  if (idle_knights >= knights_min) {
    AI::expand_borders(castle_pos);
  }
  else {
    AILogDebug["do_create_defensive_buffer"] << inventory_pos << " not enough knights to expand borders for defensive buffer, knights = " << idle_knights << ", knights_min = " << knights_min;
  }
  AILogDebug["do_create_defensive_buffer"] << inventory_pos << " done do_create_defensive_buffer";
}


// build a toolmaker, and a steel smelter if enough coal and iron ore
void
AI::do_build_toolmaker_steelsmelter() {
  AILogDebug["do_build_toolmaker_steelsmelter"] << inventory_pos << " inside do_build_toolmaker_steelsmelter";
  ai_status.assign("MAIN LOOP - tools");
  update_building_counts();
  // one toolmaker in entire REALM
  int toolmaker_count = realm_building_count[Building::TypeToolMaker];
  if (toolmaker_count < 1) {
    if (need_tools) {
      AILogInfo["do_build_toolmaker_steelsmelter"] << inventory_pos << " need tools but have no toolmaker, trying to build one near castle";
      MapPos built_pos = AI::build_near_pos(castle_flag_pos, AI::spiral_dist(14), Building::TypeToolMaker);
      if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) {
        AILogInfo["do_build_toolmaker_steelsmelter"] << inventory_pos << " built toolmaker at pos " << built_pos;
      }
      if (built_pos == stopbuilding_pos) { return; }
    }
    else {
      AILogDebug["do_build_toolmaker_steelsmelter"] << inventory_pos << " don't need any tools right now, not building toolmaker";
    }
  }
  else {
    AILogDebug["do_build_toolmaker_steelsmelter"] << inventory_pos << " Already placed toolmaker, not building another";
  }

  // if no steel, but have iron & coal, build steelsmelter to produce steel for toolmaker
  //   do this even if no toolmaker exists and tools not needed yet, they might be needed soon
  unsigned int steel_count = stock_inv->get_count_of(Resource::TypeSteel);
  unsigned int iron_ore_count = stock_inv->get_count_of(Resource::TypeIronOre);
  unsigned int coal_count = stock_inv->get_count_of(Resource::TypeCoal);
  update_building_counts();
  if (stock_buildings.at(inventory_pos).count[Building::TypeSteelSmelter] < 1
    && (steel_count < 1 && iron_ore_count > 0 && coal_count > 0)) {
    AILogInfo["do_build_toolmaker_steelsmelter"] << inventory_pos << " no steel, but have iron & coal, trying to build steelsmelter";
    for (MapPos center_pos : stock_buildings.at(inventory_pos).occupied_military_pos) {
      update_building_counts();
      int steelsmelter_count = stock_buildings.at(inventory_pos).count[Building::TypeSteelSmelter];
      if (steelsmelter_count >= 1) {
        AILogDebug["do_build_toolmaker_steelsmelter"] << inventory_pos << " Already placed steelsmelter, not building more";
        break;
      }
      MapPosVector corners = AI::get_corners(center_pos);
      MapPos built_pos = bad_map_pos;
      for (MapPos corner_pos : corners) {
        built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(6), Building::TypeSteelSmelter);
        if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) {
          AILogInfo["do_build_toolmaker_steelsmelter"] << inventory_pos << " built steelsmelter at pos " << built_pos;
          break;
        }
        if (built_pos == stopbuilding_pos) { return; }
      }
      if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) { break; }
    } // for each military building
  }
  else {
    AILogDebug["do_build_toolmaker_steelsmelter"] << inventory_pos << " not building steelsmelter, either already placed one or have no iron / coal";
  }
  AILogDebug["do_build_toolmaker_steelsmelter"] << inventory_pos << " done do_build_toolmaker_steelsmelter";
}

// build a fisherman in every good spot
// build up to 3 farms as needed ** changing this to TWO farms max to reduce clutter
// build up to 2 mills as needed - near the farms  ** changing this to ONE mill max to reduce clutter
// build one baker - near the mills
// build a 3rd lumberjack after the first farm is built - because it makes good use of time while farmer is sowing his fields
//    I keep trying to break this up into smaller functions it really works best together
//
// NOTE - had to change this function somewhat.  Even though it is ideal to do farmer->3rd_lumberjack->wait a bit->miller->baker the delay between
//   building tends to result in poor road connection between the food buildings.  Because it is so critical that the food buildings all have very good
//    connections to each other, and I cannot figure out a way to "save a spot" for roads without actually connecting them, build them all at once
//
void
AI::do_build_food_buildings_and_3rd_lumberjack() {
  AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " Main Loop - food (and 3rd lumberjack)";
  ai_status.assign("MAIN LOOP - food");
  update_building_counts();
  // to avoid placing a Farm right near the castle at the beginning of a game, just don't 
  //  build any food buildings until at least two occupied huts have been placed and staffed
  //  IN ADDITION TO DOING THIS, adding a "don't build XX distance to castle" to build_near_pos
  signed int occupied_huts = realm_occupied_building_count[Building::TypeHut];
  if (occupied_huts < 2){
    AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " not building any food buildings until at least two occupied knight huts in realm, currently have: " << occupied_huts;
    return;
  }
  unsigned int food_count = 0;
  food_count += stock_inv->get_count_of(Resource::TypeBread) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeBread];
  food_count += stock_inv->get_count_of(Resource::TypeMeat) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeMeat];
  food_count += stock_inv->get_count_of(Resource::TypeFish) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeFish]; 
  MapPos built_pos = bad_map_pos;
  bool need_farm = false;
  bool need_mill = false;
  if (food_count < (food_max - anti_flapping_buffer)) {
    //
    // build fisherman if water found with no nearby fisherman
    //
    MapPosSet count_by_corner;
    for (MapPos center_pos : stock_buildings.at(inventory_pos).occupied_military_pos) {
      MapPosVector corners = AI::get_corners(center_pos);
      for (MapPos corner_pos : corners) {
        unsigned int count = AI::count_terrain_near_pos(corner_pos, AI::spiral_dist(4), Map::TerrainWater0, Map::TerrainWater3, "dk_blue");
        if (count >= waters_min) {
          if (!AI::building_exists_near_pos(corner_pos, AI::spiral_dist(8), Building::TypeFisher)) {
            AILogInfo["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " water found and no fisherman nearby, trying to build fisherman";
            built_pos = bad_map_pos;
            built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(4), Building::TypeFisher);
            if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) {
              AILogInfo["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " built fisherman at pos " << built_pos;
              break;
            }
            if (built_pos == stopbuilding_pos) { return; }
          }
        }
        if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) { break; }
      }
    } // fisherman

    //
    // build wheat farm if none (or if needing another)
    //
    update_building_counts();
    int farm_count = stock_buildings.at(inventory_pos).count[Building::TypeFarm];
    if (farm_count == 0) {
      AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " has zero farms, need to build one";
      need_farm = true;
    }
    else {
      // if economy is far enough along, place a second and third farm near existing farm buildings
      //   (if they indeed exist) by inserting their positions in the front of the queue
      int mine_count = 0;
      mine_count += stock_buildings.at(inventory_pos).completed_count[Building::TypeCoalMine];
      mine_count += stock_buildings.at(inventory_pos).completed_count[Building::TypeIronMine];
      mine_count += stock_buildings.at(inventory_pos).completed_count[Building::TypeGoldMine];
      //mine_count += stock_buildings.at(inventory_pos).completed_count[Building::TypeStoneMine];  // not implemented
      // because grain mills and bakers are connected immediately, use 'count' rather than 'completed'
      //  to avoid placing more before the first ones complete.  Could maybe use 'connected_count' instead?
      int mill_count = stock_buildings.at(inventory_pos).count[Building::TypeMill];
      int baker_count = stock_buildings.at(inventory_pos).count[Building::TypeBaker];
      if (mine_count >= 3) {
        /*
        if (farm_count == 2 && mill_count >= 2 && baker_count >= 1) {
          AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " three completed mines, two farms, two mills, and a baker exist.  Need a third wheat farm";
          need_farm = true;
        }
        */
        if (farm_count == 1 && mill_count == 1 && baker_count >= 1) {
          AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " three completed mines, a mill, and a baker exist.  Need a second wheat farm";
          need_farm = true;
        }
        /*
        if (farm_count >= 1 && mill_count == 1 && baker_count >= 1) {
          AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " three completed mines, two farms, and a baker exist.  Need a second mill";
          need_mill = true;
        }
        */
      }
      else {
        AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " not building any more farms until having 3+ completed mines, currently have " << mine_count;
      }
    }

    if (need_farm) {
      MapPosVector farm_positions;  // pos with existing farm
      // if this is not the first farm, try to build near existing food infrastructure
      //   by locating food buildings and inserting to front of build pos list
      // WAIT this causes a problem when farms are demolished because of excess food
      //   and then more food is needed later, the new farm should be built near the 
      //   existing food infrastructure.  So I am forcing this true for now, if there
      //   are actually no food buildings it can simply fail to insert any new positions
      if (true){
      //if (farm_count == 1 || farm_count == 2) {
        MapPos mill_pos = bad_map_pos;
        MapPos farm_pos = bad_map_pos;
        MapPos baker_pos = bad_map_pos;
        AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for food buildings)";
        game->get_mutex()->lock();
        AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for food buildings)";
        Game::ListBuildings buildings = game->get_player_buildings(player);
        AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player) (for food buildings)";
        game->get_mutex()->unlock();
        AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player) (for food buildings)";
        for (Building *building : buildings) {
          if (building->get_type() == Building::TypeMill ||
              building->get_type() == Building::TypeFarm ||
              building->get_type() == Building::TypeBaker){
            if (find_nearest_inventory(map, player_index, building->get_position(), DistType::FlagAndStraightLine, &ai_mark_pos) != inventory_pos)
              continue;
          }
          // do NOT simply insert them as they are found or they won't be in priority order
          if (building->get_type() == Building::TypeMill) {
            mill_pos = building->get_position();
            AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " found mill at pos " << mill_pos << " with same closest inventory";
          }
          if (building->get_type() == Building::TypeFarm) {
            farm_pos = building->get_position();
            AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " found farm at pos " << farm_pos << " with same closest inventory";
          }
          if (building->get_type() == Building::TypeBaker) {
            baker_pos = building->get_position();
            AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " found baker at pos " << baker_pos << " with same closest inventory";
          }
        }

        // insert order matters here, mill is the best thing to be near because it is the middle of the food chain
        if (mill_pos != bad_map_pos) {
          AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " inserting mill_pos " << mill_pos << " into 2nd/3rd farm build_positions list";
          farm_positions.push_back(mill_pos);
        }
        if (farm_pos != bad_map_pos) {
          farm_positions.push_back(farm_pos);
          AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " inserting farm_pos " << farm_pos << " into 2nd/3rd farm build_positions list";
        }
        if (baker_pos != bad_map_pos) {
          farm_positions.push_back(baker_pos);
          AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " inserting baker_pos " << baker_pos << " into 2nd/3rd farm build_positions list";
        }
      }

      // start build_positions with the existing farm_positions
      // If this is a second farm it will first try to build near other farm buildings if it enough farmable land
      //    if it is the first farm, or the unlikely event no other farm buildings found, it will try to build near fields
      MapPosVector farm_search = farm_positions;
      // for first farm (or as fallback if can't connect to existing food buildings)
      //    try to build near open grass tiles
      // because farms take up a lot of space, try to place them a bit away from the castle
      //  to do this, instead of having current inventory_pos as the first area tried, make it the last but otherwise
      //   check centers in usual order this should result in a farm being built near the first knight hut expansion,
      //    or one of the first few.  THIS WAS WRITTEN PRIOR TO MULTIPLE ECONOMIES - needs work?
      MapPosVector farm_centers = stock_buildings.at(inventory_pos).occupied_military_pos;
      // remove first element, which is always castle_pos  (NOT castle_flag_pos, which might make more sense)
      farm_centers.erase(farm_centers.begin(), farm_centers.begin() + 1);
      // add current inventory_pos back to the end
      farm_centers.push_back(inventory_pos);
      // append the farm_centers to the farm_search list, which began with any existing farms
      farm_search.insert(farm_search.end(), farm_centers.begin(), farm_centers.end());
      MapPosSet count_by_corner;
      for (MapPos center_pos : farm_search) {
        if (need_farm == false) {
          break;
        }
        MapPosVector corners = AI::get_corners(center_pos);
        for (MapPos corner_pos : corners) {
          int count = count_farmable_land(corner_pos, spiral_dist(4), "dk_yellow");
          AILogVerbose["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " corner_pos " << corner_pos << " has open_space/fields count: " << count << ", min_openspace_farm is " << min_openspace_farm;
          if (count >= min_openspace_farm) {
            AILogVerbose["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " corner_pos " << corner_pos << " has enough open grass tiles to build a farm, adding to list";
            count_by_corner.insert(std::make_pair(corner_pos, count));
          }
          else {
            AILogVerbose["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " corner_pos " << corner_pos << "does not have enough open grass tiles to build a farm here";
          }
        }
        // build wheat farm near corner with most open grass
        MapPosVector search_positions = AI::sort_by_val_desc(count_by_corner);

        built_pos = bad_map_pos;
        for (MapPos corner_pos : search_positions) {
          //ai_mark_pos.clear();
          AILogInfo["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " trying to build wheat farm near pos " << corner_pos;
          built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(4), Building::TypeFarm);
          if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) {
            AILogInfo["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " built wheat farm at pos " << built_pos;
            need_farm = false;
            break;
          }
          if (built_pos == stopbuilding_pos) { return; }
        }
        if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) { break; }
      } // foreach military building
    } // if needing farm

    update_building_counts();
    farm_count = stock_buildings.at(inventory_pos).count[Building::TypeFarm];
    if (farm_count < 1) {
      AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " couldn't place first wheat farm,  expand towards some fields or water";
      expand_towards.insert("foods");
      AI::expand_borders(castle_pos);
    }
  } // if food < max
  //
  // third lumberjack
  //    this has nothing to do with food, but now is the optimal time to build the third lumberjack
  //      but it would be inefficient to break this up because farms are tied to mills and bakers
  //
  // if a 3rd lumberjack already exists, but no longer has the min number of trees nearby, burn it and place a new one
  //   this should fix an issue where a cluster of wood buildings exists but is waiting on trees to grow, bottlenecking it
  //     better to move one of the lumberjacks to an area with mature trees
  // Could move this to the do_build_rangers function which is almost identical, but that is a Realm-wide check while this is per-stock
  // actually... moving this to its own realm-wide function do_burn_unproductive_3rd_lumberjacks
  update_building_counts();
  unsigned int sawmill_count = stock_buildings.at(inventory_pos).count[Building::TypeSawmill];
  unsigned int farm_count = stock_buildings.at(inventory_pos).count[Building::TypeFarm];
  unsigned int lumberjack_count = stock_buildings.at(inventory_pos).count[Building::TypeLumberjack];
  unsigned int planks_count = stock_inv->get_count_of(Resource::TypePlank);
  AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " DEBUG current lumberjack count: " << lumberjack_count << ", sawmill_count: " << sawmill_count << ", farm_count: " << farm_count << ", panks_count: " << planks_count << ", planks_max: " << planks_max;
  if (sawmill_count > 0 && farm_count > 0 && planks_count < (planks_max - anti_flapping_buffer) && lumberjack_count < 3) {
    AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " planks not maxed and < 3 lumberjacks, build a third";
    // count trees near military buildings,
    //   the third lumberjack doesn't need to be near sawmill, if there is a spot with many trees that is fine
    MapPosSet count_by_corner;
    for (MapPos center_pos : stock_buildings.at(inventory_pos).occupied_military_pos) {
      update_building_counts();
      lumberjack_count = stock_buildings.at(inventory_pos).count[Building::TypeLumberjack];
      if (lumberjack_count >= 3) {
        AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " Already placed third lumberjack, not building more";
        break;
      }
      MapPosVector corners = AI::get_corners(center_pos);
      for (MapPos corner_pos : corners) {
        unsigned int count = AI::count_objects_near_pos(corner_pos,
          AI::spiral_dist(4), Map::ObjectTree0, Map::ObjectPine7, "lt_green");
        if (count >= near_trees_min) {
          count_by_corner.insert(std::make_pair(corner_pos, count));
        }
      }
    } // foreach military building
    // build lumberjack near corner with most trees
    MapPosVector search_positions = AI::sort_by_val_desc(count_by_corner);
    built_pos = bad_map_pos;
    for (MapPos corner_pos : search_positions) {
      AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " try to build lumberjack near pos " << corner_pos;
      built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(4), Building::TypeLumberjack);
      if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) {
        AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " built 3rd lumberjack at pos " << built_pos;
        break;
      }
      if (built_pos == stopbuilding_pos) { return; }
    }
    update_building_counts();
    lumberjack_count = stock_buildings.at(inventory_pos).count[Building::TypeLumberjack];
    if (lumberjack_count < 3) {
      // don't need to expand borders for this
      AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " couldn't place 3rd lumberjack,  will try again next AI loop";
    }
  } // 3rd lumberjack
  //
  // back to food - grain mills and baker
  //
  if (food_count < (food_max - anti_flapping_buffer)){
    // build mill & baker near *already productive* wheat farms
    //  MODIFIED - will build near unproductive wheat farm solely because the delay
    //   in waiting for wheat fields results in unacceptable road congestion and poor
    //   road connections between the mill/baker and wheat farm sometimes
    update_building_counts();
    farm_count = stock_buildings.at(inventory_pos).count[Building::TypeFarm];
    if (farm_count >= 1) {
      AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for mill and baker)";
      game->get_mutex()->lock();
      AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for mill and baker)";
      Game::ListBuildings buildings = game->get_player_buildings(player);
      AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player) (for mill and baker)";
      game->get_mutex()->unlock();
      AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player) (for mill and baker)";
      for (Building *building : buildings) {
        if (building->get_type() != Building::TypeFarm)
          continue;
        if (!building->is_done())
          continue;
        if (find_nearest_inventory(map, player_index, building->get_position(), DistType::FlagAndStraightLine, &ai_mark_pos) != inventory_pos)
          continue;
        MapPos farm_pos = building->get_position();
        /*
        // I don't think this is needed now that there is a general check to avoid this condition inside 
        //  build_near_pos.  ALSO, find_nearest_inventory, which considers only straightline-dist, has been
        //  replaced with the original game flag->find_nearest_inventory_for_resource function which
        //  considers flag-dist, and only considers inventories that are accepting resources
        if (find_nearest_inventory(farm_pos) != inventory_pos) {
          AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " farm at pos " << farm_pos << " is not closest to current inventory_pos " << inventory_pos << ", skipping";
          continue;
        }
        */
        /*
        // only build a miller if a farm is already productive (having some fields nearby)
        /// disabling this because the delay in building miller and baker results it bad road connections
        ///   even though the delay would be optimal for building priorities, having a very good road connection is even more important!
        unsigned int count = 0;
        count += AI::count_objects_near_pos(farm_pos, AI::spiral_dist(4), Map::ObjectSeeds0, Map::ObjectFieldExpired, "yellow");
        count += AI::count_objects_near_pos(farm_pos, AI::spiral_dist(4), Map::ObjectField0, Map::ObjectField5, "dk_yellow");
        AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " farm at pos " << farm_pos << " fields nearby count: " << count << ", min acceptable is " << near_fields_min;
        if (count < near_fields_min)
          continue;
        */
        // build grain mill near farm
        if (stock_buildings.at(inventory_pos).count[Building::TypeMill] < 1 || need_mill) {
          built_pos = bad_map_pos;
          AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " try to build grain mill near farm at pos " << farm_pos;
          built_pos = AI::build_near_pos(farm_pos, AI::spiral_dist(12), Building::TypeMill);
          if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) {
            AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " built grain mill at pos " << built_pos;
            need_mill = false;
            // just increase it here rather than doing update_building_counts again
            // this is needed because the stock_buildings.at(inv).count[Mill] won't be updated automatically!
            stock_buildings.at(inventory_pos).count[Building::TypeMill]++;
          }
        }
        // need to find a simple way to disable this check so it tries to place the baker immedately, ignoring max incomplete buildings
        //  if I comment it out there, it will still be rejected by the build_near_pos call (I think)
        if (built_pos == stopbuilding_pos) { return; }
        // build bakery near farm
        if (stock_buildings.at(inventory_pos).count[Building::TypeBaker] < 1) {
          built_pos = bad_map_pos;
          AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " try to build bakery near farm at pos " << farm_pos;
          built_pos = AI::build_near_pos(farm_pos, AI::spiral_dist(12), Building::TypeBaker);
          if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) {
            AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " built bakery at pos " << built_pos;
            break;
          }
          if (built_pos == stopbuilding_pos) { return; }
        }
      } // foreach farm
    } // if any wheat farms
  } // if food < max
  else {
    AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " have sufficient food, skipping";
  }
  AILogDebug["do_build_food_buildings_and_3rd_lumberjack"] << inventory_pos << " done do_build_food_buildings_3rd_lumberjack";
}


// coal mines are PLACED early in the AI loop to secure good placement, but not CONNECTED/BUILT until here
void
AI::do_connect_coal_mines() {
  ai_status.assign("MAIN LOOP - coal");
  AILogDebug["do_connect_coal_mines"] << inventory_pos << " inside do_connect_coal_mines()";
  unsigned int coal_count = stock_inv->get_count_of(Resource::TypeCoal) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeCoal];
  if (coal_count < coal_max - anti_flapping_buffer) {
    AILogInfo["do_connect_coal_mines"] << inventory_pos << " AI: desire more coal";
    // note that expanding towards hills when territory already contains unmarked hills
    //    may be sub-optimal... but is probably good enough
    update_building_counts();
    int coalmine_count = stock_buildings.at(inventory_pos).count[Building::TypeCoalMine];
    if (coalmine_count < max_coalmines) {
      AILogDebug["do_connect_coal_mines"] << inventory_pos << " coalmine_count " << coalmine_count << " is less than max_coalmines " << max_coalmines << ", expand towards hills & coal flags";
      expand_towards.insert("hills");
      expand_towards.insert("coal");
      AI::expand_borders(castle_pos);
    }else{
      AILogDebug["do_connect_coal_mines"] << inventory_pos << " coalmine_count " << coalmine_count << " is >= max_coalmines " << max_coalmines << ", max_coalmines reached, not connecting more";
      return;
    }
    // ADDING - don't connect a second coalmine until have at least one iron mine
    // REMOVED - don't connect a second coalmine until have at least two non-coal mines
    // REMOVED - don't connect a third coalmine until have at least three non-coal mines
    if (coalmine_count > 0){
      if (stock_buildings.at(inventory_pos).connected_count[Building::TypeIronMine] < 1){
        AILogDebug["do_connect_coal_mines"] << inventory_pos << " already have a connected coal mine and do not yet have a connected iron mine, not connecting another coal mine yet";
        return;
      }
      /*
      int other_mine_count = stock_buildings.at(inventory_pos).connected_count[Building::TypeIronMine] + stock_buildings.at(inventory_pos).connected_count[Building::TypeGoldMine];
      if (other_mine_count < 2){
        AILogDebug["do_connect_coal_mines"] << inventory_pos << " already have a connected coal mine and do not yet have two other non-coal mines, not connecting another coal mine yet";
        return;
      }
      */
      /*
      if (other_mine_count < 3 && coalmine_count > 2){
        AILogDebug["do_connect_coal_mines"] << inventory_pos << " already have two connected coal mines and do not yet have three other non-coal mines, not connecting another coal mine yet";
        return;
      }
      */
    }
    //  if low on miners/pickaxes, don't build a second/third coal mine unless already having
    //   at least one occupied iron and gold mine to avoid depleting pickaxes/miners
    update_building_counts();
    if (serfs_idle[Serf::TypeMiner] + serfs_potential[Serf::TypeMiner] < 3
       && stock_buildings.at(inventory_pos).connected_count[Building::TypeCoalMine] >= 1
       && (stock_buildings.at(inventory_pos).occupied_count[Building::TypeIronMine] == 0 || stock_buildings.at(inventory_pos).occupied_count[Building::TypeGoldMine] == 0)) {
      AILogDebug["do_connect_coal_mines"] << inventory_pos << " has <3 miners+pickaxes remaining, but not yet both an occupied iron mine and an occupied gold mine.  Not connecting this coal mine";
      return;
    }
    // connect a disconnected coal mine that was placed if conditions are right
    flags_static_copy = *(game->get_flags());
    flags = &flags_static_copy;
    for (Flag *flag : *flags) {
      if (flag == nullptr || flag->get_owner() != player_index || flag->is_connected() || !flag->has_building())
        continue;
      if (flag->get_building()->get_type() != Building::TypeCoalMine)
        continue;
      AILogDebug["do_connect_coal_mines"] << inventory_pos << " disconnected coal mine found with flag pos " << flag->get_position();
      if (stock_buildings.at(inventory_pos).unfinished_count >= max_unfinished_buildings) {
        AILogDebug["do_connect_coal_mines"] << inventory_pos << " max unfinished buildings reached, not connecting this coal mine to road system";
        return;
      }
      AILogDebug["do_connect_coal_mines"] << inventory_pos << " trying to connect unfinished coal mine flag to road system";
      Road notused; // not used here, can I just pass a zero instead of &notused to build_best_road and skip initialization of a wasted object?
      bool was_built = AI::build_best_road(flag->get_position(), road_options, &notused);
      if (!was_built) {
        // should the mine be demolished if this happens?
        AILogDebug["do_connect_coal_mines"] << inventory_pos << " failed to connect coal mine to road network! ";
        // YES it should
        AILogDebug["do_connect_coal_mines"] << inventory_pos << " demolishing coal mine that could not be connected to road network";
        AILogDebug["do_connect_coal_mines"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling demolish flag&building (failed to connect coal mine)";
        game->get_mutex()->lock();
        AILogDebug["do_connect_coal_mines"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling demolish flag&building (failed to connect coal mine)";
        game->demolish_building(flag->get_building()->get_position(), player);
        AILogDebug["do_connect_coal_mines"] << inventory_pos << " demolishing flag for coal mine that could not be connected to road network";
        game->demolish_flag(flag->get_position(), player);
        AILogDebug["do_connect_coal_mines"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling demolish flag&building (failed to connect coal mine)";
        game->get_mutex()->unlock();
        AILogDebug["do_connect_coal_mines"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling demolish flag&building (failed to connect coal mine)";
        update_building_counts();
      }
      else {
        AILogDebug["do_connect_coal_mines"] << inventory_pos << " successfully connected unfinished coal mine to road network";
        stock_buildings.at(inventory_pos).unfinished_count++;
      }
    } // foreach flag
  }
  else {
    AILogDebug["do_connect_coal_mines"] << inventory_pos << " have sufficient coal, skipping";
  }
  AILogDebug["do_connect_coal_mines"] << inventory_pos << " done do_connect_coal_mines";
}


// iron mines are PLACED early in the AI loop to secure good placement, but not CONNECTED/BUILT until here
void
AI::do_connect_iron_mines() {
  ai_status.assign("MAIN LOOP - iron");
  AILogDebug["do_connect_iron_mines"] << inventory_pos << " inside do_connect_iron_mines()";
  unsigned int iron_count = stock_inv->get_count_of(Resource::TypeIronOre) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeIronOre];
  if (iron_count < (iron_ore_max - anti_flapping_buffer)) {
    AILogInfo["do_connect_iron_mines"] << inventory_pos << " AI: desire more iron";
    // note that expanding towards hills when territory already contains unmarked hills
    //    may be sub-optimal... but is probably good enough
    update_building_counts();
    int ironmine_count = stock_buildings.at(inventory_pos).count[Building::TypeIronMine];
    if (ironmine_count < max_ironmines) {
      AILogDebug["do_connect_iron_mines"] << inventory_pos << " ironmine_count " << ironmine_count << " is less than max_ironmines " << max_ironmines << ", expand towards hills & iron flags";
      expand_towards.insert("hills");
      expand_towards.insert("iron_ore");
      AI::expand_borders(castle_pos);
    }else{
      AILogDebug["do_connect_iron_mines"] << inventory_pos << " ironmine_count " << ironmine_count << " is >= max_ironmines " << max_ironmines << ", max_ironmines reached, not connecting more";
      return;
    }
    /* now limiting to one iron mine per Inventory
    // don't connect a second ironmine until have at least one coal mines
    if (ironmine_count > 0){
      int coalmine_count = stock_buildings.at(inventory_pos).connected_count[Building::TypeCoalMine];
      if (coalmine_count < 1){
        AILogDebug["do_connect_iron_mines"] << inventory_pos << " already have a connected iron mine and do not yet have a conneted coal mine, not connecting another iron mine yet";
        return;
      }
    }
    */
    //  if low on miners/pickaxes, don't build a second iron mine unless already having
    //   at least one occupied coal and gold mine to avoid depleting pickaxes/miners
    update_building_counts();
    if (serfs_idle[Serf::TypeMiner] + serfs_potential[Serf::TypeMiner] < 3
      && stock_buildings.at(inventory_pos).connected_count[Building::TypeIronMine] >= 1
      && (stock_buildings.at(inventory_pos).occupied_count[Building::TypeCoalMine] == 0 || stock_buildings.at(inventory_pos).occupied_count[Building::TypeGoldMine] == 0)) {
      AILogDebug["do_connect_iron_mines"] << inventory_pos << " has <3 miners+pickaxes remaining, but not yet connecting this iron mine";
      return;
    }
    // connect any disconnected iron mine that was placed if conditions are right
    flags_static_copy = *(game->get_flags());
    flags = &flags_static_copy;
    for (Flag *flag : *flags) {
      if (flag == nullptr || flag->get_owner() != player_index || flag->is_connected() || !flag->has_building())
        continue;
      if (flag->get_building()->get_type() != Building::TypeIronMine)
        continue;
      AILogDebug["do_connect_iron_mines"] << inventory_pos << " disconnected iron mine found with flag pos " << flag->get_position();
      if (stock_buildings.at(inventory_pos).unfinished_count >= max_unfinished_buildings) {
        AILogDebug["do_connect_iron_mines"] << inventory_pos << " max unfinished buildings reached, not connecting this iron mine to road system";
        return;
      }
      AILogDebug["do_connect_iron_mines"] << inventory_pos << " trying to connect unfinished iron mine flag to road system";
      Road notused; // not used here, can I just pass a zero instead of &notused to build_best_road and skip initialization of a wasted object?
      bool was_built = AI::build_best_road(flag->get_position(), road_options, &notused);
      if (!was_built) {
        // should the mine be demolished if this happens?
        AILogDebug["do_connect_iron_mines"] << inventory_pos << " failed to connect iron mine to road network! ";
        // YES it should
        AILogDebug["do_connect_iron_mines"] << inventory_pos << " demolishing iron mine that could not be connected to road network";
        AILogDebug["do_connect_iron_mines"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling demolish flag&building (failed to connect iron mine)";
        game->get_mutex()->lock();
        AILogDebug["do_connect_iron_mines"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling demolish flag&building (failed to connect iron mine)";
        game->demolish_building(flag->get_building()->get_position(), player);
        AILogDebug["do_connect_iron_mines"] << inventory_pos << " demolishing flag for iron mine that could not be connected to road network";
        game->demolish_flag(flag->get_position(), player);
        AILogDebug["do_connect_iron_mines"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling demolish flag&building (failed to connect iron mine)";
        game->get_mutex()->unlock();
        AILogDebug["do_connect_iron_mines"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling demolish flag&building (failed to connect iron mine)";
        update_building_counts();
      }
      else {
        AILogDebug["do_connect_iron_mines"] << inventory_pos << " successfully connected unfinished iron mine to road network";
        stock_buildings.at(inventory_pos).unfinished_count++;
      }
    }
  }
  else {
    AILogDebug["do_connect_iron_mines"] << inventory_pos << " have sufficient iron, skipping";
  }
  AILogDebug["do_connect_iron_mines"] << inventory_pos << " done do_connect_iron_mines";
}


// build a steel smelter if one wasn't already created earlier to support toolmaker
void
AI::do_build_steelsmelter() {
  ai_status.assign("MAIN LOOP - steel");
  // saw an infinite loop here dec11 2020, AI never ended doing update_buildings calls??
  // saw again dec13, not actually infinite loop but took very long to run... minutes
  AILogDebug["do_build_steelsmelter"] << inventory_pos << " inside do_build_steelsmelter()";
  unsigned int steel_count = stock_inv->get_count_of(Resource::TypeSteel);
  update_building_counts();
  int steelsmelter_count = stock_buildings.at(inventory_pos).count[Building::TypeSteelSmelter];
  MapPos built_pos = bad_map_pos;
  if (steelsmelter_count > 1 || steel_count > (steel_max - anti_flapping_buffer)) {
    AILogDebug["do_build_steelsmelter"] << inventory_pos << " have steel smelter or sufficient steel, skipping";
    return;
  }
  AILogInfo["do_build_steelsmelter"] << inventory_pos << " desire more steel, trying to build steel smelter";
  // if we got to this point we should already have iron & coal stored or mines connected
  //&& iron_ore_count > 0 && coal_count > 0)) {
  // try to place steel smelter halfway between a coal mine and iron mine if both exist, preferring active ones
  // NOTE - there isn't much point to this until second/third+ steel smelters built, because one will almost certainly
  //   have already been built to fuel toolmaker.   This does seem to work correctly though if tested in a contrived way
  //      actually Because there is only ever one toolmaker, and it is near castle, this function should be helpful for non-castle stocks
  // DISABLING THIS FOR NOW  - jan19 2021
  //MapPos halfway_pos = find_halfway_pos_between_buildings(Building::TypeCoalMine, Building::TypeIronMine);
  MapPos halfway_pos = bad_map_pos;  // intended to fail and run fallback condition code
  MapPosVector steelsmelter_pos;
  if (halfway_pos != bad_map_pos) {
    AILogDebug["do_build_steelsmelter"] << inventory_pos << " adding pos halfway between a coal mine and an iron mine to first build_near center";
    steelsmelter_pos.push_back(halfway_pos);
  }
  steelsmelter_pos.insert(steelsmelter_pos.end(), stock_buildings.at(inventory_pos).occupied_military_pos.begin(), stock_buildings.at(inventory_pos).occupied_military_pos.end());
  for (MapPos center_pos : steelsmelter_pos) {
    update_building_counts();
    int steelsmelter_count = stock_buildings.at(inventory_pos).count[Building::TypeSteelSmelter];
    if (steelsmelter_count >= 1) {
      AILogDebug["do_build_steelsmelter"] << inventory_pos << " Already placed steel smelter, not building more";
      break;
    }
    MapPosVector corners = AI::get_corners(center_pos);
    for (MapPos corner_pos : corners) {
      built_pos = bad_map_pos;
      built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(6), Building::TypeSteelSmelter);
      if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) {
        AILogDebug["do_build_steelsmelter"] << inventory_pos << " built steel smelter at pos " << built_pos;
        return;
      }
      if (built_pos == stopbuilding_pos) { return; }
    }
    if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) { break; }
  } //foreach military building
  AILogDebug["do_build_steelsmelter"] << inventory_pos << " done do_build_steelsmelter";
}


// build a blacksmith (weaponsmith)
void
AI::do_build_blacksmith() {
  ai_status.assign("MAIN LOOP - weapons");
  AILogDebug["do_build_blacksmith"] << inventory_pos << " inside do_build_blacksmith()";
  MapPos built_pos = bad_map_pos;
  update_building_counts();
  if (stock_buildings.at(inventory_pos).count[Building::TypeWeaponSmith] < 1) {
    // don't build unless sufficient coal, and iron or steel
    unsigned int coal_count = stock_inv->get_count_of(Resource::TypeCoal);
    unsigned int iron_ore_count = stock_inv->get_count_of(Resource::TypeIronOre);
    unsigned int steel_count = stock_inv->get_count_of(Resource::TypeSteel);
    if ((coal_count >= coal_min || stock_buildings.at(inventory_pos).completed_count[Building::TypeCoalMine] > 0)
      && (iron_ore_count >= iron_ore_min || (stock_buildings.at(inventory_pos).completed_count[Building::TypeIronMine] > 0 && stock_buildings.at(inventory_pos).completed_count[Building::TypeSteelSmelter] > 0))
      || steel_count >= steel_min) {
      AILogInfo["do_build_blacksmith"] << inventory_pos << " trying to build blacksmith";
      for (MapPos center_pos : stock_buildings.at(inventory_pos).occupied_military_pos) {
        update_building_counts();
        if (stock_buildings.at(inventory_pos).count[Building::TypeWeaponSmith] >= 1) {
          AILogDebug["do_build_blacksmith"] << inventory_pos << " Already placed blacksmith, not building more";
          break;
        }
        MapPosVector corners = AI::get_corners(center_pos);
        for (MapPos corner_pos : corners) {
          built_pos = bad_map_pos;
          built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(6), Building::TypeWeaponSmith);
          if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) {
            AILogInfo["do_build_blacksmith"] << inventory_pos << " built blacksmith at pos " << built_pos;
            break;
          }
          if (built_pos == stopbuilding_pos) { return; }
        } // for each corner
        if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) { break; }
      } // for each military building
    } // if have sufficient coal, iron, steel
    else {
      AILogDebug["do_build_blacksmith"] << inventory_pos << " not enough coal, or iron/steel to build blacksmith";
      AILogDebug["do_build_blacksmith"] << inventory_pos << " coal_count " << coal_count << ", coal_min " << coal_min << ", iron_ore_count " << iron_ore_count <<
        ", iron_ore_min " << iron_ore_min << ", steel_count " << steel_count << ", steel_min " << steel_min;
      AILogDebug["do_build_blacksmith"] << inventory_pos << " completed coal mines: " << stock_buildings.at(inventory_pos).completed_count[Building::TypeCoalMine]
        << ", completed iron mines: " << stock_buildings.at(inventory_pos).completed_count[Building::TypeIronMine]
        << ", completed steel foundries: " << stock_buildings.at(inventory_pos).completed_count[Building::TypeSteelSmelter];
    }
  }  // if no blacksmith
  else {
    AILogDebug["do_build_blacksmith"] << inventory_pos << " already have blacksmith, not building another";
  }
  AILogDebug["do_build_blacksmith"] << inventory_pos << " done do_build_blacksmith";
}


// build a gold smelter if enough gold ore (should it consider coal?)
// connect any disconnected gold mines if conditions are met
void
AI::do_build_gold_smelter_and_connect_gold_mines() {
  ai_status.assign("MAIN LOOP - gold");
  AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " inside do_build_gold_smelter_and_connect_gold_mines()";
  unsigned int gold_bars_count = stock_inv->get_count_of(Resource::TypeGoldBar);
  MapPos built_pos = bad_map_pos;
  if (gold_bars_count < ( gold_bars_max - anti_flapping_buffer)) {
    AILogInfo["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " AI: desire more gold";
    unsigned int gold_ore_count = stock_inv->get_count_of(Resource::TypeGoldOre) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeGoldOre];
    update_building_counts();
    if (gold_ore_count >= gold_ore_min || stock_buildings.at(inventory_pos).completed_count[Building::TypeGoldMine] > 0) {
      // build a gold smelter
      if (stock_buildings.at(inventory_pos).count[Building::TypeGoldSmelter] < 1) {
        AILogInfo["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " trying to build gold smelter";
        for (MapPos center_pos : stock_buildings.at(inventory_pos).occupied_military_pos) {
          if (stock_buildings.at(inventory_pos).count[Building::TypeGoldSmelter] >= 1) {
            AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " Already placed gold smelter, not building more";
            break;
          }
          MapPosVector corners = AI::get_corners(center_pos);
          for (MapPos corner_pos : corners) {
            built_pos = bad_map_pos;
            built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(6), Building::TypeGoldSmelter);
            if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) {
              AILogInfo["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " built gold smelter at pos " << built_pos;
              break;
            }
            if (built_pos == stopbuilding_pos) { return; }
          }
          if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) { break; }
        } // foreach military pos
      } // if have gold smelter
    } // if need gold smelter
    // note that expanding towards hills when territory already contains unmarked hills
    //    may be sub-optimal... but is probably good enough
    update_building_counts();
    int goldmine_count = stock_buildings.at(inventory_pos).count[Building::TypeGoldMine];
    if (goldmine_count < 1) {
      AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " has no gold mine, expand towards hills & gold flags";
      expand_towards.insert("hills");
      expand_towards.insert("gold_ore");
      AI::expand_borders(castle_pos);
    }
    //  if low on miners/pickaxes, don't build a gold mine unless already having
    //   at least one occupied coal and iron mine to avoid depleting pickaxes/miners
    update_building_counts();
    if (serfs_idle[Serf::TypeMiner] + serfs_potential[Serf::TypeMiner] < 3
      && (stock_buildings.at(inventory_pos).occupied_count[Building::TypeCoalMine] == 0 || stock_buildings.at(inventory_pos).occupied_count[Building::TypeIronMine] == 0)) {
      AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " has <2 miners+pickaxes remaining, but not yet both an occupied coal mine and an occupied iron mine.  Not connecting this gold mine";
      return;
    }
    // don't create more than one gold mine per Inventory-economy
    if (stock_buildings.at(inventory_pos).count[Building::TypeGoldMine] >= 1) {
      AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " already have a connected gold mine for this Inventory, not connecting more";
      return;
    }
    flags_static_copy = *(game->get_flags());
    flags = &flags_static_copy;
    for (Flag *flag : *flags) {
      if (flag == nullptr || flag->get_owner() != player_index || flag->is_connected() || !flag->has_building())
        continue;
      if (flag->get_building()->get_type() != Building::TypeGoldMine)
        continue;
      AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " disconnected gold mine found with flag pos " << flag->get_position();
      if (stock_buildings.at(inventory_pos).unfinished_count >= max_unfinished_buildings) {
        AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " max unfinished buildings reached, not connecting this gold mine to road system";
        continue;
      }
      AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " trying to connect unfinished gold mine flag to road system";
      Road notused; // not used here, can I just pass a zero instead of &notused to build_best_road and skip initialization of a wasted object?
      bool was_built = AI::build_best_road(flag->get_position(), road_options, &notused);
      if (!was_built) {
        // should the mine be demolished if this happens?
        AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " failed to connect gold mine to road network! ";
        // YES it should
        AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " demolishing gold mine that could not be connected to road network";
        AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling demolish flag&building (failed to connect gold mine)";
        game->get_mutex()->lock();
        AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling demolish flag&building (failed to connect gold mine)";
        game->demolish_building(flag->get_building()->get_position(), player);
        AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " demolishing flag for gold mine that could not be connected to road network";
        game->demolish_flag(flag->get_position(), player);
        AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling demolish flag&building (failed to connect gold mine)";
        game->get_mutex()->unlock();
        AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling demolish flag&building (failed to connect gold mine)";
      }
      else {
        AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " successfully connected unfinished gold mine to road network";
        stock_buildings.at(inventory_pos).unfinished_count++;
      }
    }
  }
  else {
    // is it ever possible to have enough gold?  Is morale derived from this player's gold compared to each opponents gold??  Or total of everyone else's gold??
    //  it seems to be the player's ratio of refined gold compared to the opponents and the total amount of all gold ore originally in mountains on the map
    AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " have sufficient gold, skipping";
  }
  AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " done do_build_gold_smelter_and_connect_gold_mines";
}


// score enemy targets and attack based on own knight and morale strength
//   this function needs a lot more development
void
AI::do_attack() {
  MapPosSet scored_targets = {};
  AILogDebug["do_attack"] << name << " calling score_enemy_targets...";
  score_enemy_targets(&scored_targets);
  AILogDebug["do_attack"] << name << " score_enemy_targets call found " << scored_targets.size() << " targets";

  // TEMPORARY
  int morale = 0;
  int morale_max = 99999;

  //AILogDebug["do_attack"] << name << " getting serfs again";
  AILogDebug["do_attack"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling player->get_stats_serfs_idle()";
  game->get_mutex()->lock();
  AILogDebug["do_attack"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling player->get_stats_serfs_idle()";
  serfs_idle = player->get_stats_serfs_idle();
  AILogDebug["do_attack"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling player->get_stats_serfs_idle()";
  game->get_mutex()->unlock();
  AILogDebug["do_attack"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling player->get_stats_serfs_idle()";
  //Serf::SerfMap serfs_potential = player->get_stats_serfs_potential();
  unsigned int idle_knights = serfs_idle[Serf::TypeKnight0] + serfs_idle[Serf::TypeKnight1] + serfs_idle[Serf::TypeKnight2] + serfs_idle[Serf::TypeKnight3] + serfs_idle[Serf::TypeKnight4];
  AILogDebug["do_attack"] << name << " idle_knights: " << idle_knights << ", morale_count: " << morale;

  if (idle_knights >= knights_max && morale >= morale_max) {
    AILogDebug["do_attack"] << name << " idle_knights & morale counts are above max, beeline for enemy castle";
    // insert attack logic here
    AI::attack_nearest_target(&scored_targets);
  }
  else if (idle_knights >= knights_max) {
    AILogDebug["do_attack"] << name << " idle_knights " << idle_knights << " is above max, focus on gold/morale, attack only high value targets";
    // insert attack logic here
    // temp to get started
    AI::attack_nearest_target(&scored_targets);
  }
  else if (idle_knights >= knights_med) {
    AILogDebug["do_attack"] << name << " idle_knights " << idle_knights << " is above med, build economy, attack only high value targets";
    // insert attack logic here
  }
  else if (idle_knights >= knights_min) {
    AILogDebug["do_attack"] << name << " idle_knights " << idle_knights << " is above min, build economy, attack only very high value targets";
    // insert attack logic here
  }
  else if (idle_knights < knights_min) {
    AILogDebug["do_attack"] << name << " idle_knights " << idle_knights << " is below minimum, conserve knights";
    // do not attack.  Also, cannot attack at this level of occupation (min/min)
  }
  AILogDebug["do_attack"] << name << " done do_attack";
  // TEMP TEST
  AILogDebug["do_attack"] << name << " TEMP SHORTCUT FORCE ATTACK do_attack";
  AI::attack_nearest_target(&scored_targets);
}


// build better roads for high priority buildings
//   for each one, see if a better-scoring multi-road solution can be plotted to its affinity_building[s]
//    and if so, build that road in.  Do not remove any existing roads
// I think a major limitation of this function is that it only considers the 
//  building flag (right?) but should also consider nearby flags as source flags
void
AI::do_build_better_roads_for_important_buildings() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  AILogDebug["do_build_better_roads_for_important_buildings"] << name << " inside do_build_better_roads_for_important_buildings";
  // only do this every X loops
  if (loop_count % 10 != 0) {
    AILogDebug["do_build_better_roads_for_important_buildings"] << name << " skipping build_better_roads roads, only running this every X loops";
    return;
  }
  ai_status.assign("HOUSEKEEPING - build better roads");
  AILogDebug["do_build_better_roads_for_important_buildings"] << name << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_buildings(player) (for finding positions of military buildings)";
  game->get_mutex()->lock();
  AILogDebug["do_build_better_roads_for_important_buildings"] << name << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_buildings(player) (for finding positions of military buildings)";
  Game::ListBuildings buildings = game->get_player_buildings(player);
  AILogDebug["do_build_better_roads_for_important_buildings"] << name << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex after calling game->get_player_buildings(player) (for finding positions of military buildings)";
  game->get_mutex()->unlock();
  AILogDebug["do_build_better_roads_for_important_buildings"] << name << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex after calling game->get_player_buildings(player) (for finding positions of military buildings)";
  for (Building *building : buildings) {
    if (building == nullptr)
      continue;
    Building::Type type = building->get_type();
    if (!building->is_done() || building->is_burning())
      continue;
    // only consider these building types for road improvement
    if (type != Building::TypeWeaponSmith && type != Building::TypeSteelSmelter
      && type != Building::TypeGoldSmelter && type != Building::TypeCoalMine
      && type != Building::TypeIronMine && type != Building::TypeGoldMine) {
      continue;
    }
    AILogDebug["do_build_better_roads_for_important_buildings"] << name << " do_build_better_roads_for_important_buildings found high-priority building of type " << NameBuilding[type] << name << " at pos " << building->get_position();
    road_options.set(RoadOption::Improve);
    MapPos building_flag_pos = map->move_down_right(building->get_position());
    Road notused; // not used here, can I just pass a zero instead of &notused to build_best_road and skip initialization of a wasted object?
    build_best_road(building_flag_pos, road_options, &notused, type);
    road_options.reset(RoadOption::Improve);
  }
  AILogDebug["do_build_better_roads_for_important_buildings"] << name << " done do_build_better_roads_for_important_buildings";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_build_better_roads_for_important_buildings"] << name << " done do_build_better_roads_for_important_buildings call took " << duration;
}


// once all necessary buildings built for castle, build a warehouse and parallel infrastructure
void
AI::do_build_warehouse() {
  AILogDebug["do_build_warehouse"] << name << " inside do_build_warehouse";
  update_building_counts();
  // don't build warehouses until territory reaches a certain size
  unsigned int completed_huts = realm_completed_building_count[Building::TypeHut];
  if (loop_count % 10 != 0 || completed_huts < 12) {
    AILogDebug["do_build_warehouse"] << name << " not considering building warehouses (stocks) until a significant number of huts completed, and only every X loops";
    return;
  }
  int warehouse_count = realm_building_count[Building::TypeStock];
  int completed_warehouse_count = realm_completed_building_count[Building::TypeStock];
  if (warehouse_count > completed_warehouse_count) {
    AILogDebug["do_build_warehouse"] << name << " already placed a new warehouse (stock), not building another until previous one is built";
    return;
  }
  unsigned int planks_count = realm_inv[Resource::TypePlank];
  unsigned int stones_count = realm_inv[Resource::TypeStone];
  // increase the requirements a bit for each warehouse because it is realm-wide check
  if (planks_count < planks_min + ( 4 * warehouse_count) || stones_count < stones_min + ( 2 * warehouse_count)) {
    AILogDebug["do_build_warehouse"] << name << " not building warehouse, not enough planks or stones in realm";
    return;
  }
  // DEBUG - I think I see teh realm_occupied_military_pos list repeating a lot
  // dump it to verify
  for (MapPos center_pos : realm_occupied_military_pos) {
    AILogError["do_build_warehouse"] << name << " DEBUG - dumping realm_occupied_military_pos, found: " << center_pos;
  }

  for (MapPos center_pos : realm_occupied_military_pos) {
    AILogDebug["do_build_warehouse"] << name << " considering building warehouse near corners around realm_occupied_military_pos " << center_pos;
    MapPosVector corners = AI::get_corners(center_pos);
    for (MapPos corner_pos : corners) {
      if (get_straightline_tile_dist(map, corner_pos, castle_pos) <= 32){  
        AILogDebug["do_build_warehouse"] << name << " corner_pos " << corner_pos << " is too close to the castle, skipping this area";
        continue;
      }
      if (find_nearest_building(corner_pos, CompletionLevel::Unfinished, Building::TypeStock, 32) != nullptr) {
        AILogDebug["do_build_warehouse"] << name << " there is already a stock near corner_pos " << corner_pos << ", skipping this area";
        continue;
      }
      AILogDebug["do_build_warehouse"] << name << " try to build warehouse near pos " << corner_pos;
      MapPos built_pos = bad_map_pos;
      built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(5), Building::TypeStock);
      if (built_pos != bad_map_pos && built_pos != notplaced_pos && built_pos != stopbuilding_pos) {
        AILogDebug["do_build_warehouse"] << name << " built warehouse (stock) at pos " << built_pos;
        return;
      }
      if (built_pos == stopbuilding_pos) {
        return;
      }
    }
  }
  AILogDebug["do_build_warehouse"] << name << " done do_build_warehouse";
}


void
AI::do_get_inventory(MapPos inventory_pos) {
  // a stock's Inventory object contains a ResourceMap object for that inventory
  //unsigned int get_count_of(Resource::Type resource) {
  //    return resources[resource];
  //  }
  //ResourceMap get_all_resources() { return resources; }
  // this gets a ResourceMap for AGGREGATE INVENTORY of all player stocks including castle
  //ResourceMap resources = interface->get_player()->get_stats_resources();
  AILogDebug["do_get_inventory"] << name << " inside do_get_inventory for pos " << inventory_pos;
  AILogDebug["do_get_inventory"] << name << " DEBUG inside do_get_inventory, inventory_pos=" << inventory_pos << ", castle_flag_pos=" << castle_flag_pos;
  if (inventory_pos == castle_flag_pos) {
    AILogDebug["do_get_inventory"] << name << " this stock is the castle ";
  }
  AILogDebug["do_get_inventory"] << name << " getting player's stock inventory for stock at pos " << inventory_pos;
  Building *this_stock = game->get_building_at_pos(map->move_up_left(inventory_pos));
  if (this_stock == nullptr) {
    AILogDebug["do_get_inventory"] << name << " got nullptr for stock at pos " << inventory_pos << "!";
    return;
  }
  Inventory *this_stock_inv = this_stock->get_inventory();
  if (this_stock_inv == nullptr) {
    AILogDebug["do_get_inventory"] << name << " got nullptr for stock_inv at pos " << inventory_pos << "!";
    return;
  }
  stock_inv = this_stock_inv;
  // dump stock inventory every loop for debugging
  for (int i = 0; i < 26; i++) {
    AILogDebug["do_get_inventory"] << name << "'s stock at pos " << inventory_pos << " has " << NameResource[i] << ": " << stock_inv->get_count_of(Resource::Type(i));
  }

  //AILogDebug["do_get_inventory"] << name << "foo0";
  //stock_res_sitting_at_flags[inventory_pos] = {};
  //AILogDebug["do_get_inventory"] << name << "foo1";
  //ResourceMap flag_res_map = stock_res_sitting_at_flags.at(inventory_pos);
  //do_count_resources_sitting_at_flags();
  for (int i = 0; i < 26; i++) {
    //AILogDebug["do_get_inventory"] << name << "fooi" << i;
    //flag_res_map.at(Resource::Type(i)) = 0;
    //stock_res_sitting_at_flags.at(inventory_pos)[i] = 0;
    stock_res_sitting_at_flags[inventory_pos][Resource::Type(i)] = 0;
  }
  //AILogDebug["do_get_inventory"] << name << "foo2";

  AILogDebug["do_get_inventory"] << name << " done get_inventory";
}


// count all Resources sitting at Flags-that-are-closest-to-this-inventory_pos
//  so they can be included in max_XXX checks
// NOTE: I have seen often enough that many resources are sitting at flags but
//  none or little are in storage, but the res at flags aren't moving fast enough
//  due to various congestion.  Destroying produces in this case seems unhelpful,
//  so I am modifying the functions that check the number of res at flags to
//  cap the amount considered as part of the "max stored" 
//
void
AI::do_count_resources_sitting_at_flags(MapPos inv_pos) {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  AILogDebug["do_count_resources_sitting_at_flags"] << inv_pos << " inside do_count_resources_sitting_at_flags";
  AILogDebug["do_count_resources_sitting_at_flags"] << inv_pos << " HouseKeeping: count resources sitting at flag";
  ai_status.assign("HOUSEKEEPING - count resources sitting at flags");
  ResourceMap res_at_flags;
  flags_static_copy = *(game->get_flags());
  flags = &flags_static_copy;
  for (Flag *flag : *flags) {
    if (flag->get_owner() != player_index)
      continue;
    if (!flag->is_connected())
      continue;
    // don't spend time doing a flagsearch unless it actually has a resource sitting
    bool has_res = false;
    for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
      Resource::Type type = flag->get_resource_at_slot(i);
      if (type != Resource::TypeNone){
        has_res = true;
      }
    }
    if (!has_res)
      continue;
    if (find_nearest_inventory(map, player_index, flag->get_position(), DistType::FlagOnly, &ai_mark_pos) != inv_pos)
      continue;
    for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
      Resource::Type type = flag->get_resource_at_slot(i);
      if (type != Resource::TypeNone){
        //AILogDebug["do_count_resources_sitting_at_flags"] << name << " flag at pos " << flag->get_position() << " has a resource of type " << NameResource[type] << " at slot " << i;
        stock_res_sitting_at_flags[inv_pos][type]++;
        realm_res_sitting_at_flags[type]++;
      }
    }
  }
  // dump stock_res_sitting_at_flags every loop for debugging
  for (int i = 0; i < 26; i++) {
    //AILogDebug["do_count_resources_sitting_at_flags"] << inv_pos << " flags nearest to stock at pos " << inv_pos << " have " << NameResource[i] << ": " << stock_res_sitting_at_flags.at(inv_pos).count(Resource::Type(i));
    AILogDebug["do_count_resources_sitting_at_flags"] << inv_pos << " flags nearest to stock at pos " << inv_pos << " have " << NameResource[i] << ": " << stock_res_sitting_at_flags[inv_pos][Resource::Type(i)];
  }


  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_count_resources_sitting_at_flags"] << inv_pos << " done do_count_resources_sitting_at_flags call took " << duration;
}
