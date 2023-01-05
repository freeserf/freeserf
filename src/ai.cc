/*
 * ai.cc - AI main class
 *   Copyright 2019-2021 tlongstretch
 */


// NOTE - could use the game's "serf planning xxx" random-spiral-pos code to randomly choose within spirals when building
//  instead of always doing center-first
//      int dist = ((game->random_int() >> 2) & 0x1f) + 7;
//      MapPos dest = map->pos_add_spirally(pos, dist);


#include <algorithm>  // to satisfy cpplint

#include "src/ai.h"
#include "src/game-options.h"

AI::AI(PGame current_game, unsigned int _player_index) {

  ai_status = "INITIALIZING";
  player_index = _player_index;
  name = "Player" + std::to_string(player_index);

  AILogInfo["init"] << "inside AI::AI constructor with player_index: " << player_index;
  AILogVerbose["init"] << "AI log level is at least verbose";
  AILogDebug["init"] << "AI log level is at least debug";
  AILogInfo["init"] << "AI log level is at least info";
  AILogWarn["init"] << "AI log level is at least warn";
  AILogError["init"] << "AI log level is at least error";

  game = current_game;
  map = game->get_map();
  player = game->get_player(player_index);
  // for "build something" functions that return a MapPos of where built, stopbuilding_pos is a flag that can be returned that says to quit trying to build that thing
  //stopbuilding_pos = std::numeric_limits<unsigned int>::max() - 2;
  //stop_building = false;  // replace the 'stopbuilding_pos' idea with this, and set this to true as needed, reset at start of each loop
  loop_count = 0;
  castle = nullptr;
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
  stock_building_counts = {};
  stock_res_sitting_at_flags = {};
  realm_res_sitting_at_flags = {};
  change_buffer = 0;
  previous_knight_occupation_level = -1;
  last_sent_geologist_tick = 0;  // used to throttle sending geologists
  have_inventory_building = false; // used to quit loop early if AI is essentially defeated (has no Castle or Stocks)
  std::string mutex_message = "";  // used for logging why mutex being locked/unlocked
  clock_t mutex_timer_start = 0;  // used for logging how much time spent with mutex lock
  //longest_road_so_far = {};
  plot_road_closed_cache = {};
  plot_road_open_cache = {};
  use_plot_road_cache = true;   // this is definitely effective, tried comparing w/ cache off vs on

  road_options.reset(RoadOption::Direct);
  road_options.set(RoadOption::SplitRoads);
  road_options.set(RoadOption::PenalizeNewLength);
  road_options.set(RoadOption::PenalizeCastleFlag);
  road_options.reset(RoadOption::AvoidCastleArea);  // this currently does nothing! needs coding
  road_options.reset(RoadOption::Improve);
  road_options.reset(RoadOption::ReducedNewLengthPenalty);
  road_options.reset(RoadOption::AllowWaterRoad);
  road_options.reset(RoadOption::HoldBuildingPos);
  road_options.reset(RoadOption::MostlyStraight);
  road_options.reset(RoadOption::PlotOnlyNoBuild);
  road_options.reset(RoadOption::ReconnectNetwork);

  need_tools = false;

  /* not setting this here, it causes issues on game load
  // this seems to result in bogus value - 32??
  AILogDebug["init"] << "setting initial knight garrison levels to minimum";
  // change this to only happen on NEW game!  not on save game load
  player->change_knight_occupation(0, 0, -5);
  player->change_knight_occupation(0, 1, -5);
  player->change_knight_occupation(1, 0, -5);
  player->change_knight_occupation(1, 1, -5);
  player->change_knight_occupation(2, 0, -5);
  player->change_knight_occupation(2, 1, -5);
  player->change_knight_occupation(3, 0, -5);
  player->change_knight_occupation(3, 1, -5);
  */
  // instead only set the less-risk levels to minimum as those are always minimum
  player->change_knight_occupation(0, 0, -5);
  player->change_knight_occupation(0, 1, -5);
  player->change_knight_occupation(1, 0, -5);
  player->change_knight_occupation(1, 1, -5);
  player->change_knight_occupation(2, 0, -5);
  player->change_knight_occupation(2, 1, -5);
}

void
AI::start() {
  AILogInfo["start"] << "AI is starting, thread_id: " << std::this_thread::get_id();

  while (true) {
    //AILogDebug["start"] << "start AI::start while(true)";
    bool logged_paused = false;
    if (game->should_ai_stop() == true) {
      AILogInfo["start"] << "received stop_ai_threads signal, exiting!";
      game->ai_thread_exiting();
      return;
    }
    else if (game->get_game_speed() == 0) {
      // avoid repeat log messages when paused
      if (!logged_paused){
        AILogDebug["start"] << "game is paused, not running AI loops until unpaused";
        logged_paused = true;
      }
      ai_status.assign("AI_PAUSED");
      sleep_speed_adjusted(6000);
    }
    else if (game->is_ai_locked()) {
      AILogDebug["start"] << "AI is still locked, sleeping until game->unlock_ai called (when game init_box is closed)";
      sleep_speed_adjusted(6000);
    }
    else {
      next_loop();
      //AILogDebug["start"] << "done next_loop()";
    }
    //AILogDebug["start"] << "end AI::start while(true)";
  }
}

void
AI::next_loop(){
  AILogDebug["next_loop"] << "inside AI::next_loop()";
  loop_count++;

  ai_status.assign("SLEEPING_AT_START");
  AILogDebug["next_loop"] << "sleeping 6sec at start of new loop";
  sleep_speed_adjusted(6000);

  AILogDebug["next_loop"] << "starting AI loop #" << loop_count;
  // time entire loop
  std::clock_t loop_clock_start;
  double loop_clock_duration;
  loop_clock_start = std::clock();

  // place castle if this is start of new game
  do_place_castle();  

  // end loop early if AI is essentially defeated
  do_consider_capitulation(); if (!have_inventory_building){ return; }

  do_update_clear_reset();
  update_stocks_pos();
  inventory_pos = castle_flag_pos;  // need to set this so various functions work on the very first AI loop, before the loop over Inventories starts
  update_buildings();
  do_get_inventory(castle_flag_pos);

  // this is broken since messing with flagsearch, see details here:  https://github.com/forkserf/forkserf/issues/70
  //DEBUG
  //if (realm_building_count[Building::TypeHut] > 1){
  //  AI::identify_arterial_roads(map);
  //  //return;
  //}

  do_get_serfs();  // is this actually needed?
  //do_debug_building_triggers();   // not using these right now

  //-----------------------------------------------------------
  // housekeeping tasks
  //  NOTE - all of these should probably be moved to run AFTER the main game loop.
  //    for example, we shouldn't send geologists out as the very first action when starting a game,
  //      rather it should be done after first few buildings/roads placed
  //-----------------------------------------------------------

  do_connect_disconnected_flags(); // except unfinished mines
  do_connect_disconnected_road_networks();
  do_build_better_roads_for_important_buildings();  // is this working?  I still see pretty inefficient roads for important buildings
  do_pollute_castle_area_roads_with_flags(); // CHANGE THIS TO USE ARTERIAL ROADS  (nah, it works well enough as it is, do that later)
  do_fix_stuck_serfs();  // this is definitely still an issue, try to fix root cause
  do_fix_missing_transporters();  // is this still a problem anymore??  YES
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

  do_check_resource_needs();

  for (MapPos this_inventory_pos : stocks_pos) {
    inventory_pos = this_inventory_pos;
    
    AILogDebug["next_loop"] << inventory_pos << " inside next_loop AA, needs_wood bool is " << stock_building_counts.at(inventory_pos).needs_wood;

    AILogDebug["next_loop"] << "Starting economy loop for Inventory at pos " << inventory_pos;
    update_buildings();

    AILogDebug["next_loop"] << inventory_pos << " inside next_loop A, needs_wood bool is " << stock_building_counts.at(inventory_pos).needs_wood;

    // debug, log buildings
    //AILogDebug["next_loop"] << "stock at pos " << inventory_pos << " has all/completed/occupied buildings: ";
    //for (int x = 0; x < 25; x++) {
    //  AILogDebug["next_loop"] << "type " << x << " / " << NameBuilding[x] << ": " << stock_building_counts.at(inventory_pos).count[x]
    //    << "/" << stock_building_counts.at(inventory_pos).completed_count[x] << "/" << stock_building_counts.at(inventory_pos).occupied_count[x];
    //}

    do_get_inventory(inventory_pos);
    do_promote_serfs_to_knights();  // this is actually done per-stock in AI, not per-realm like the button does
    do_count_resources_sitting_at_flags(inventory_pos);

    AILogDebug["next_loop"] << inventory_pos << " inside next_loop B-2, needs_wood bool is " << stock_building_counts.at(inventory_pos).needs_wood;

    do_check_resource_needs();

    AILogDebug["next_loop"] << inventory_pos << " inside next_loop B-1, needs_wood bool is " << stock_building_counts.at(inventory_pos).needs_wood;

    do_create_star_roads_for_new_warehouses();

    AILogDebug["next_loop"] << inventory_pos << " inside next_loop B0, needs_wood bool is " << stock_building_counts.at(inventory_pos).needs_wood;

    do_build_sawmill_lumberjacks(); sleep_speed_adjusted(1000);  // crash here?  after do_build_sawmill_lumberjacks returned, bad malloc error

    AILogDebug["next_loop"] << inventory_pos << " inside next_loop B, needs_wood bool is " << stock_building_counts.at(inventory_pos).needs_wood;

    if(do_can_build_knight_huts())
      expand_borders(); sleep_speed_adjusted(1000);

    // PLACE MINES EARLY - but do not connect them to roads so they do not actually get built until later
    //   this is to secure good placement when resources are found, before the signs fade
    do_place_coal_mines(); sleep_speed_adjusted(1000);
    do_place_iron_mines(); sleep_speed_adjusted(1000);
    do_place_gold_mines(); sleep_speed_adjusted(1000);

    // if this is the Castle, don't place any other buildings until these have their materials
    if (inventory_pos == castle_flag_pos && do_wait_until_sawmill_lumberjacks_built() == false)
        break;

    AILogDebug["next_loop"] << inventory_pos << " inside next_loop C, needs_wood bool is " << stock_building_counts.at(inventory_pos).needs_wood;

    do_build_stonecutter(); sleep_speed_adjusted(1000);
    do_build_rangers(); sleep_speed_adjusted(1000);

    if(do_can_build_other())
      {do_build_toolmaker_steelsmelter(); sleep_speed_adjusted(1000);}

    if(do_can_build_other())
      {do_build_food_buildings(); sleep_speed_adjusted(1000);}

    if(do_can_build_other())
      {do_build_3rd_lumberjack(); sleep_speed_adjusted(1000);}

    if(do_can_build_other())
      {do_connect_coal_mines(); sleep_speed_adjusted(1000);}

    if(do_can_build_other())
      {do_connect_iron_mines(); sleep_speed_adjusted(1000);}

    if(do_can_build_other())
      {do_build_steelsmelter(); sleep_speed_adjusted(1000);}

    if(do_can_build_other())
      {do_build_blacksmith(); sleep_speed_adjusted(1000);}

    if(do_can_build_other())
      {do_build_gold_smelter_and_connect_gold_mines(); sleep_speed_adjusted(1000);}
    

    AILogDebug["next_loop"] << inventory_pos << " inside next_loop D, needs_wood bool is " << stock_building_counts.at(inventory_pos).needs_wood;

    do_demolish_excess_lumberjacks();
    do_demolish_excess_foresters();
	  do_demolish_excess_food_buildings();
    do_send_geologists(); sleep_speed_adjusted(1000);
    do_spiderweb_roads(); sleep_speed_adjusted(1000);

    AILogDebug["next_loop"] << "Done with economy loop for Inventory at pos " << inventory_pos;
  }

  // create parallel infrastructure!
  do_build_warehouse(); sleep_speed_adjusted(1000);

  AILogDebug["next_loop"] << "Done AI Loop #" << loop_count;
  ai_status.assign("END OF LOOP");
  AILogDebug["next_loop"] << "loop complete, sleeping 2sec";
  loop_clock_duration = (std::clock() - loop_clock_start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["next_loop"] << "done next_loop, loop took " << loop_clock_duration;
  sleep_speed_adjusted(2000);
}







void
AI::do_place_castle() {
  AILogDebug["do_place_castle"] << "inside do_place_castle()";
  ai_status.assign("HAS_CASTLE_CHECK");
  if (!player->has_castle()) {
    AILogDebug["do_place_castle"] << "does not yet have a castle";
    // place castle
    //   improve this so that it is more intelligent about other resources than trees/stones/building_sites
    //    but have the minimum scores reduced a bit for each area scored so it eventually settles on something
    //
    // pick random spots on the map until an acceptable area found, and try building there
    ///===========================================================================================================
    // IMPORTANT - with multiple AI threads all starting at the same time, they are trying to build castle at the same time,
    //    this Random rnd will actually be IDENTICAL across all threads!  To mix them up, sleep a bit so that
    //      the time that is fed to seed the random function gets a different time-seed for each player
    // changed this to mutex.lock() instead, but keeping this because... I dunno it seems nicer when they don't all start at exactly the same time
    //   maybe start doing random wait instead?  meh
    AILogDebug["do_place_castle"] << "sleeping " << player_index << "sec so each AI player thread gets different seed for random map pos";
    std::this_thread::sleep_for(std::chrono::milliseconds(2000 * player_index));
    int maxtries = 1500;  // crash if failed to place castle after this many tries, regardless of desperation
    int lower_standards_tries = 120;  // reduce standards after this many tries (can happen repeatedly)
    int desperation = 0;  // current level of lowered standards
    int x = 0;  // current try
    while (true) {
      x++;
      if (x > maxtries) {
        AILogDebug["do_place_castle"] << "unable to place castle after " << x << " tries, maxtries reached!";
        throw ExceptionFreeserf("unable to place castle for an AI player after exhausting all tries!");
      }
      if (x > lower_standards_tries * (desperation + 1)){
        desperation++;
        AILogDebug["do_place_castle"] << "unable to place castle after " << x << " tries, lowering standards to desperation level " << desperation;
      }
      MapPos pos = map->get_rnd_coord(NULL, NULL, game->get_rand());
      AILogDebug["do_place_castle"] << " considering placing castle at random pos " << pos;
      // first see if it is even possible to build large building here
      if (!game->can_build_castle(pos, player)) {
        AILogDebug["do_place_castle"] << "cannot build a castle at pos " << pos;
        continue;
      }
      // check if area has acceptable resources + building pos, and if so build there
      //if (place_castle(game, pos, spiral_dist(8), desperation)) {
      if (place_castle(game, pos, desperation)) {
        AILogDebug["do_place_castle"] << "found acceptable place to build castle, at pos: " << pos;
        mutex_lock("AI::do_place_castle calling game->build_castle");
        bool was_built = game->build_castle(pos, player);
        mutex_unlock();
        if (was_built) {
          AILogDebug["do_place_castle"] << "built castle at pos: " << pos << " after " << x << " tries";
          castle_pos = pos;
          castle_flag_pos = map->move_down_right(castle_pos);
          AILogDebug["do_place_castle"] << "castle has position " << castle_pos << ", with castle_flag_pos " << castle_flag_pos;
          return;
        }
        AILogDebug["do_place_castle"] << "failed to build castle at pos: " << pos << ", will keep trying";
      }

      //sleep_speed_adjusted(15000);  // TEMP DEBUG TESTING

    }
  }
  // on game load, castle_pos is unknown even though castle exists, need to find it
  if (castle_pos == bad_map_pos) {
    Game::ListBuildings buildings = game->get_player_buildings(player);
    for (Building *building : buildings) {
      if (building == nullptr)
        continue;
      if (building->get_type() == Building::TypeCastle) {
        castle_pos = building->get_position();
        castle_flag_pos = map->move_down_right(castle_pos);
        AILogDebug["do_place_castle"] << "found existing castle, has position " << castle_pos << ", with castle_flag_pos " << castle_flag_pos;
      }
    }
  }
}

// check and see if conditions are met for AI to consider itself defeated
//  currently, this will happen if AI loses its Castle and all Stocks
// Could have the AI thread exit entirely but, for now just have it quit 
//  the loop early and do nothing
void
AI::do_consider_capitulation() {
  AILogDebug["do_consider_capitulation"] << "inside do_consider_capitulation";
  ai_status.assign("CONSIDERING_CAPITULATION");
  Game::ListBuildings buildings = game->get_player_buildings(player);
  have_inventory_building = false;
  for (Building *building : buildings) {
    if (building == nullptr)
      continue;
    if ((building->get_type() == Building::TypeCastle || building->get_type() == Building::TypeStock)
        && !building->is_burning()){
      have_inventory_building = true;
      AILogDebug["do_consider_capitulation"] << "found an inventory building of type " << NameBuilding[building->get_type()] << " at pos " << building->get_position() << ", not capitulating";
      break;
    }
  }
  if (!have_inventory_building){
    AILogInfo["do_consider_capitulation"] << "THIS AI PLAYER HAS LOST ITS CASTLE AND HAS NO STOCKS, DOING NOTHING";
  }
}


void
AI::do_update_clear_reset() {
  AILogDebug["do_update_clear_reset"] << "inside do_update_clear_reset";
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
  road_options.reset(RoadOption::AvoidCastleArea);    // this currently donearest_inving! needs coding
  //road_options.reset(RoadOption::Improve);  // this wasn't here for a lonearest_inv, did I forget it or was this intentional?
  //road_options.reset(RoadOption::ReducedNewLengthPenalty);  // this wasn't here for a long time, did I forget it or was this intentional?
  road_options.reset(RoadOption::AllowWaterRoad);  // this wasn't here for a long time, did I forget it or was this intentional?
  //road_options.reset(RoadOption::HoldBuildingPos);  // this wasn't here for a long time, did I forget it or was this intentional?
  road_options.reset(RoadOption::MostlyStraight);
  road_options.reset(RoadOption::PlotOnlyNoBuild);
  road_options.reset(RoadOption::ReconnectNetwork);
  //unfinished_hut_count = 0;
  //unfinished_building_count = 0;
  realm_inv = player->get_stats_resources();
  //new_stocks.clear();  DO NOT CLEAR THIS IT NEEDS TO PERSIST THROUGH LOOPS

  // does this belong here?  this whole update_clear thing needs re-work
  for (int i = 0; i < 26; i++) {
    realm_res_sitting_at_flags[Resource::Type(i)] = 0;
  }

  //have_inventory_building = false;  // this isn't really needed as it checks each time

  // flush the closed-node cache for plot_roads
  // IMPORTANT - this should probably be flushed on every call of build_best_road
  //  or on each do_XXX AI function?
  //  or every time something is demolished?
  //  or add some function to the game to indicate when any flag/path-on-road-MapPos is tainted?
  AILogDebug["do_update_clear_reset"] << "clearing plot_road caches";
  plot_road_open_cache.clear();
  plot_road_closed_cache.clear();
  use_plot_road_cache = true;
}


void
AI::do_get_serfs() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();

  AILogDebug["do_get_serfs"] << "inside do_get_serfs";
  AILogDebug["do_get_serfs"] << "getting serfs";
  mutex_lock("AI::do_get_sefs getting serfs at AI loop start");
  serfs_idle = player->get_stats_serfs_idle();
  serfs_potential = player->get_stats_serfs_potential();
  serfs_total = player->get_serfs();
  mutex_unlock();
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_get_serfs"] << "done do_get_serfs call took " << duration;
}


//
// function only used for debugging, never called by AI on its own
//  building certain buildings manually triggers some function
void
AI::do_debug_building_triggers() {
  AILogDebug["do_debug_building_triggers"] << "inside do_debug_building_triggers";
  /*
  // DEBUG
  //   trigger demolish/rebuild all roads by placing a Pig Farm anywhere in the realm
  if (realm_building_count[Building::TypePigFarm] > 0) {
    AILogDebug["do_debug_building_triggers"] << "PigFarm found, running rebuild_all_roads";
    rebuild_all_roads();
    // then destroy the pig farm so it doesn't keep rebuilding forever
    AILogDebug["do_debug_building_triggers"] << "thread #" << std::this_thread::get_id() << " AI is locking mutex before demolishing pig farm";
    game->get_mutex()->lock();
    AILogDebug["do_debug_building_triggers"] << "thread #" << std::this_thread::get_id() << " AI has locked mutex before demolishing pig farm";
    Game::ListBuildings buildings = game->get_player_buildings(player);
    for (Building *building : buildings) {
      if (building->get_type() == Building::TypePigFarm)
        game->demolish_building(building->get_position(), player);
    }
    AILogDebug["do_debug_building_triggers"] << "thread #" << std::this_thread::get_id() << " AI is unlocking mutex after demolishing pig farm";
    game->get_mutex()->unlock();
    AILogDebug["do_debug_building_triggers"] << "thread #" << std::this_thread::get_id() << " AI has unlocked mutex after demolishing pig farm";
    // don't resume AI until most serfs have made it back into the castle
    AILogDebug["do_debug_building_triggers"] << "done rebuild_all_roads, waiting for lost serfs to clear out";
    for (int x = 0; x < 50; x++) {
      sleep_speed_adjusted(15000);
      unsigned int lost_serfs = 0;
      for (Serf *serf : game->get_player_serfs(player)) {
        if (serf->get_state() == Serf::StateFreeWalking) {
          lost_serfs++;
        }
      }
      if (lost_serfs <= 5) {
        break;
      }
      AILogDebug["do_debug_building_triggers"] << "there are " << lost_serfs << " lost serfs in FreeWalking state, waiting longer";
    }
    return;
  }
  */


  // DEBUG
  //   tell AI to quit by placing a BoatBuilder anywhere in the realm
  if (realm_building_count[Building::TypeBoatbuilder] > 0) {
    AILogDebug["do_debug_building_triggers"] << "BoatBuilder found, locking all AIs";
    game->lock_ai();
    return;
  }
}

// to be 100% "fair" this should use the global "promote all possible serfs to knights"
//  function that human players can click on, but because there is no advantage gained
//  by doing it per-stock I will leave this as it is for now.
//
// NOTE - I think there is some kind of rate limiting happening here when making this call
//   that either doesn't exist or isn't obvious in the original game, if I watch the count
//   of knights in a Stock that has tons of available serfs, and the number of weapons, it
//   seems that only a few knights are created at a time not all at once.  Not sure why,
//   but it doesn't really matter as they are all converted soon enough
//
void
AI::do_promote_serfs_to_knights() {
  AILogDebug["do_promote_serfs_to_knights"] << "inside do_promote_serfs_to_knights";
  ai_status.assign("HOUSEKEEPING - promote serfs to knights");
  if(get_stock_inv() == nullptr)
    return;
  unsigned int idle_serfs = static_cast<unsigned int>(get_stock_inv()->free_serf_count());
  unsigned int swords_count = get_stock_inv()->get_count_of(Resource::TypeSword);
  unsigned int shields_count = get_stock_inv()->get_count_of(Resource::TypeShield);
  unsigned int idle_knights = serfs_idle[Serf::TypeKnight0] + serfs_idle[Serf::TypeKnight1] + serfs_idle[Serf::TypeKnight2] + serfs_idle[Serf::TypeKnight3] + serfs_idle[Serf::TypeKnight4];
  unsigned int excess_serfs = idle_serfs < serfs_min ? 0 : idle_serfs - serfs_min;
  unsigned int promotable = std::min(excess_serfs, std::min(swords_count, shields_count));
  AILogDebug["do_promote_serfs_to_knights"] << "idle_knights: " << idle_knights << ", idle_serfs: " << idle_serfs << ", serfs_min: " << serfs_min << ", excess serfs: " << excess_serfs << ", swords: " << swords_count << ", shields: " << shields_count << ", promotable: " << promotable;
  AILogDebug["do_promote_serfs_to_knights"] << "promoting serfs to knights: " << promotable;
  // this 'promotable' function doesn't seem to work right, it doesn't limit the number promoted to the specified integer like it suggests
  //int promoted = player->promote_serfs_to_knights(promotable);
  int promoted = 0;
  mutex_lock("AI::do_promote_serfs_to_knights calling game->get_player_serfs(player) (for do_manage_knight_occupation_levels is_waiting)");
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
  mutex_unlock();
}


void
AI::do_connect_disconnected_flags() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  AILogDebug["do_connect_disconnected_flags"] << "inside do_connect_disconnected_flags";
  ai_status.assign("do_connect_disconnected_flags");
  // got a segfault during flags_copy = game->get_flags... need to mutex wrap all AI game->get_flags calls?
  Flags flags_copy = *(game->get_flags());  // create a copy so we don't conflict with the game thread, and don't want to mutex lock for a long function
  for (Flag *flag : flags_copy) {
    if (flag == nullptr)
      continue;
    if (flag->get_owner() != player_index)
      continue;
    if (flag->get_position() == castle_flag_pos)
      continue;
    if (flag->is_connected())
      continue;
    if (flag->has_building()) {
      AILogDebug["do_connect_disconnected_flags"] << "flag at pos " << flag->get_position() << " has an attached building of type " << NameBuilding[flag->get_building()->get_type()];
      if ((flag->get_building()->get_type() == Building::TypeCoalMine
        || flag->get_building()->get_type() == Building::TypeIronMine
        || flag->get_building()->get_type() == Building::TypeGoldMine
        || flag->get_building()->get_type() == Building::TypeStoneMine)
        && !flag->get_building()->is_done()) {
        AILogDebug["do_connect_disconnected_flags"] << "disconnected flag at pos " << flag->get_position() << " is attached to an incomplete Mine, skipping - it will be dealt with later";
        continue;
      }
      if (flag->get_building()->get_type() == Building::TypeForester &&
        flag->get_building()->is_done() && flag->get_building()->has_serf()){
        AILogDebug["do_connect_disconnected_flags"] << "disconnected flag at pos " << flag->get_position() << " is attached to an occupied ranger, skipping";
        continue;
      }
      /*
      if ((flag->get_building()->get_type() == Building::TypeHut
       || flag->get_building()->get_type() == Building::TypeTower
       || flag->get_building()->get_type() == Building::TypeFortress)
       && flag->get_building()->is_done() && flag->get_building()->has_serf()){
        AILogDebug["do_connect_disconnected_flags"] << "disconnected flag at pos " << flag->get_position() << " is attached to an occupied military building, skipping";
        continue;
      }
      */
    }
    // try to connect it to road network
    AILogDebug["do_connect_disconnected_flags"] << "flag at pos " << flag->get_position() << " has no connected road, trying to connect it";
    Road notused; // not used here, can I just pass a zero instead of &notused to build_best_road and skip initialization of a wasted object?
    bool was_built = AI::build_best_road(flag->get_position(), road_options, &notused, "do_connect_disconnected_flags:Flag@"+std::to_string(flag->get_position()));
    if (!was_built) {
      // if it could not be connected, burn any attached building and remove the flag
      // WAIT - this causes a problem for newly-taken enemy military buildings, which frequently cannot be connected to road network
      //  often they are completely disconnected from own borders
      //  it seems counterproductive to burn this based solely on not being able to connect them, applying some extra logic to avoid this
      // this works great, but it points out another problem, when excess attackers that cannot fit in the taken building
      //  become lost, they wander aimlessly all over instead of returning to their territory.  SEE WHAT ORIGINAL GAME DOES!!
      //  I think in original game the knights will return to their original hut??  if so... are replacements immediately dispatched to backflil
      //    before the attackers even attempt to return???  what happens when they arrive?  what happens when their original hut is full upon return?

      // NOTE - from serflings testing...
      //  when knights are sent out to fight, replacements are immediately dispatched to backfill
      //  when knights win a fight, the excess become Lost and return to castle/Inventory
      //  so, it seems like the problem here in Freeserf/Forkserf is that the knights are not making it back home in reasonable time

      int must_demolish_building = false;
      if (flag->has_building()) {
        if (flag->get_building()->is_military()){
          //AILogDebug["do_connect_disconnected_flags"] << "this unconnectable building is a military, threat_level is " << flag->get_building()->get_threat_level();
          if (flag->get_building()->get_threat_level() == 3){
            AILogDebug["do_connect_disconnected_flags"] << "this unconnectable building is a military building near enemy territory, possibly a newly taken one, NOT burning it nor its flag";
            continue;
          }
        }else{
          must_demolish_building = true;
        }
      }
      // burn any attached building and destroy the flag
      mutex_lock("AI::do_connect_disconnected_flags calling demolish_flag (and maybe attached building)");
      if (must_demolish_building){
        AILogDebug["do_connect_disconnected_flags"] << "failed to connect disconnected flag to road network!  BURNING ATTACHED BUILDING!";
        // how did this ever work before??
        //game->demolish_building(flag->get_position(), player);
        game->demolish_building(map->move_up_left(flag->get_position()), player);
        // sleep to appear more human
        sleep_speed_adjusted(3000);
      }
      AILogDebug["do_connect_disconnected_flags"] << "failed to connect disconnected flag to road network!  removing it";
      // got exception here, adding nullptr check
      if (flag != nullptr){ game->demolish_flag(flag->get_position(), player); }
      mutex_unlock();
    }
  }

  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_connect_disconnected_flags"] << "done do_connect_disconnected_flags call took " << duration;
}


//
// CONSIDER COMPLETELY REPLACING SPIDERWEB ROADS WITH THE FOLLOWING LOGIC:
//     (make separate function?)
//  instead of just around Inventories, randomly choose flags and look
//  where a relatively straight road could be built to another random flag
//  that would greatly improve the flag-dist between them, this should help
//  accomplish similar web network farther away from Invs where it can still help
//
void
AI::do_spiderweb_roads() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();

  AILogDebug["do_spiderweb_roads"] << inventory_pos << " inside do_spiderweb_roads";
  ai_status.assign("do_spiderweb_roads");
  // "spider-web" roads - because a "star network" pattern naturally emerges with castle at center, try to
  //   convert it to a "spider-web" shape by attemping build roads between any flags within a band a bit outside
  //    the castle area
  //
  //  trace a hex-circle 18 tiles out from the Inventory building (castle or stock/warehouse),
  //     at each corner draw a size8 circle and connect any flags within it
  //  this could be improved by making a "caret" or "flattened hexagon" shape at corners instead of spiral_pos
  //    -  tried doing this, seemed like a waste of time.  Circle is fine
  //
  // only do this every X loops, and only add one new road per run
  unsigned int completed_huts = stock_building_counts.at(inventory_pos).completed_count[Building::TypeHut];
  if (inventory_pos == castle_flag_pos){
    if ( loop_count % 30 != 0 || completed_huts < 8 || completed_huts > 20) {
      AILogDebug["do_spiderweb_roads"] << inventory_pos << " skipping spider-web roads for castle, only running every 30 loops and completed knight huts " << completed_huts << " is >8 or <20";
      return;
    }
  }else{
    // Stocks get a longer spiderweb range because they tend to "steal" large numbers of huts as soon
    //  as they are built, resulting in zero spiderweb roads if using same rules as castle
    if ( loop_count % 30 != 0 || completed_huts < 16 || completed_huts > 36) {
      AILogDebug["do_spiderweb_roads"] << inventory_pos << " skipping spider-web roads for this Stock, only running every 30 loops and completed knight huts " << completed_huts << " is >16 or <36";
      return;
    }
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
  //  UPDATE jan02 2022 - reducing the arc by one sector to 8, saw a long snakey road
  //   built that covered almost 180degrees of castle area and blocked a lot of stuff
  //  UPDATE jan04 2022 - reducing again, same reason though not as extreme
  //
  //MapPosVector flag_lists[9] = { }; //array of vectors
  MapPosVector flag_lists[7] = { }; //array of vectors

  //for (unsigned int i = AI::spiral_dist(14); i < AI::spiral_dist(15); i++) {
  // because the ring uses spiral function which only goes counter-clockwise starting
  //  from down-right, randomize the start of the ring to be a random pos between
  //  spiral(13.5) and spiral(14.5)
  unsigned int ring_start = AI::spiral_dist(14);
  unsigned int ring_end = AI::spiral_dist(15);
  unsigned int ring_length = ring_end - ring_start;
  unsigned int half_ring_length = ring_length * 0.5;
  unsigned int upper = ring_start + half_ring_length;
  unsigned int lower = ring_start - half_ring_length;
  unsigned int adjusted_ring_start = (rand() % (upper - lower + 1)) + lower;
  bool was_built = false;
  for (unsigned int i = adjusted_ring_start; i < adjusted_ring_start + ring_length; i++){
    MapPos ring_pos = map->pos_add_extended_spirally(inventory_pos, i);
    //ai_mark_pos.erase(ring_pos);
    //ai_mark_pos.insert(ColorDot(ring_pos, "dk_coral"));
    //sleep_speed_adjusted(150);
    // only every X positions or on last spot in ring
    if (i % 4 != 0 || i == AI::spiral_dist(15) - 1 )
      continue;
    //AILogDebug["do_spiderweb_roads"] << inventory_pos << " spiderweb doing stuff";

    // every X pos in the hexagonal "ring"/perimeter, do an area search for flags
    MapPosVector flag_list = { };
    //for (unsigned int x = 0; x < AI::spiral_dist(4); x++) {
    // trying a bit bigger
    for (unsigned int x = 0; x < AI::spiral_dist(5); x++) {
      MapPos area_pos = map->pos_add_extended_spirally(ring_pos, x);
      //ai_mark_pos.erase(area_pos);
      //ai_mark_pos.insert(ColorDot(area_pos, "lavender"));
      //sleep_speed_adjusted(11));
      if (!map->has_flag(area_pos) || map->get_owner(area_pos) != player_index)
        continue;
      if (game->get_flag_at_pos(area_pos) != nullptr && !game->get_flag_at_pos(area_pos)->is_connected())
        continue;
      //ai_mark_pos.erase(area_pos);
      //ai_mark_pos.insert(ColorDot(area_pos, "red"));
      //sleep_speed_adjusted(11));
      flag_list.push_back(area_pos);
    }
    // shift all the flag_list vectors back, dropping the oldest one
    //  and push the newest one to the front of the array
    //for (int y = 1; y < 9; y++) {
    for (int y = 1; y < 7; y++) {  // jan04 2022 reducing to 7 sectors
      flag_lists[y - 1] = flag_lists[y];
    }
    //flag_lists[8] = flag_list;
    flag_lists[6] = flag_list;  // jan04 2022 reducing to 7 sectors

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
    //AILogDebug["do_spiderweb_roads"] << "shuffling the flag_set so it is in random order";
    //std::random_shuffle(flag_set.begin(), flag_set.end());
    // instead convert to vector, shuffling seems important   oct22 2020
    MapPosVector shuffled_flag_vector(flag_set.begin(), flag_set.end());
    std::random_shuffle(shuffled_flag_vector.begin(), shuffled_flag_vector.end());

    AILogDebug["do_spiderweb_roads"] << inventory_pos << " shuffled_flag_vector contains " << shuffled_flag_vector.size() << " elements";

    //
    // loop over random pairs of flags (that were found in the last 8 sectors)    // jan02 2022 reducing to 8 sectors so last 7 sectors here
    //  and consider building a road between them, and if they look good, do so
    //
    for (MapPos area_flag_pos : shuffled_flag_vector) {
      //if (spider_web_roads_built > 0) { break; }  // only create one road per run
      AILogDebug["do_spiderweb_roads"] << inventory_pos << " considering roads from area_flag_pos " << area_flag_pos;
      for (MapPos other_area_flag_pos : shuffled_flag_vector) {
        if (area_flag_pos == other_area_flag_pos) { continue; }
        AILogDebug["do_spiderweb_roads"] << inventory_pos << " considering roads from area_flag_pos " << area_flag_pos << " to other_area_flag_pos " << other_area_flag_pos;

        // bitwise operator... to simplify checking for already tried combinations in either order  VERIFIED THIS DETECTS DUPES
        unsigned int pair = area_flag_pos & other_area_flag_pos;
        if (tried_pairs.count(pair) > 0) { continue; }
        tried_pairs.insert(pair);

        // only accept the pair if the current best-path between these two flags contains the 
        //  the currently selected Inventory (castle/stock) flag.  If if it not, there is some other 
        //  path such as a natural or spider-web road that does not bottleneck at the castle/stock
        //  and so does not need obvious improvement. 
        // ALSO, accept the pair if the current tile_dist between roads is very long

        MapPosVector flags_found = {};
        unsigned int tile_dist = 0;
        bool acceptable = false;
        if(find_flag_path_and_tile_dist_between_flags(map, area_flag_pos, other_area_flag_pos, &flags_found, &tile_dist, &ai_mark_pos)){
          AILogDebug["do_spiderweb_roads"] << inventory_pos << " flags_found between " << area_flag_pos << " and " << other_area_flag_pos << " with solution flag count " << flags_found.size() << " and solution tile_dist " << tile_dist;
          bool has_inventory_flag = false;
          for (MapPos pos : flags_found){
            AILogDebug["do_spiderweb_roads"] << inventory_pos << " flags_found contains: " << pos;
            if (pos == inventory_pos){
              //AILogDebug["do_spiderweb_roads"] << inventory_pos << " existing flag-path from flags_found contains the inventory_pos " << inventory_pos << " = " << pos << ", acceptable pair";
              acceptable = true;
              break;  // could break here for efficiency, but for now I like to have it dump the entire flags_found list
            }
          }
          if (tile_dist > 30){
            AILogDebug["do_spiderweb_roads"] << inventory_pos << " tile_dist is >30, acceptable pair";
            acceptable = true;
          }
          if (!acceptable){
            continue;
          }
        }

        //AILogDebug["do_spiderweb_roads"] << inventory_pos << " still considering";
        //ai_mark_pos.clear();
        //ai_mark_pos.insert(ColorDot(area_flag_pos, "green"));
        //ai_mark_pos.insert(ColorDot(other_area_flag_pos, "red"));
        //sleep_speed_adjusted(2000);

        //
        // try to build the spiderweb road
        //
        

        // Improve works well now, but is slow sometimes, taking up to 20sec to run in rare cases.  
        //    ahh it was because SplitRoads toggle wasn't working and it was still calculating split road solutions 
        //    now that I have SplitRoads-off working it is faster and seems okay still
        //    ehh... its still slow occasionally and seems to produce worse results than allow SplitRoads did.. not sure yet
        // Direct seems much faster, but results are worse (snakey roads)
        //
        // NOTE - the slowness is noticeable when testing at 40x speed, but probably not noticeable at reasonable game speeds
        //  trying to optimize this might be a waste of time
        // yes... using SplitRoads and Improve gives MUCH better results than forcing direct, the perf penalty is acceptable
        // definitely do not use Direct, the high-effort method gives fantastic results!
        road_options.set(RoadOption::Improve);
        //road_options.reset(RoadOption::PenalizeNewLength);
        road_options.set(RoadOption::ReducedNewLengthPenalty);
        //road_options.set(RoadOption::Direct);
        //road_options.set(RoadOption::MostlyStraight);
        //road_options.reset(RoadOption::SplitRoads);
        AILogDebug["do_spiderweb_roads"] << inventory_pos << " about to call build_best_road";
        Road built_road;
        was_built = build_best_road(area_flag_pos, road_options, &built_road, "do_spiderweb_roads", Building::TypeNone, Building::TypeNone, other_area_flag_pos);
        road_options.reset(RoadOption::Improve);
        //road_options.set(RoadOption::PenalizeNewLength);
        road_options.reset(RoadOption::ReducedNewLengthPenalty);
        //road_options.reset(RoadOption::Direct);
        //road_options.reset(RoadOption::MostlyStraight);
        //road_options.set(RoadOption::SplitRoads);
        if (was_built) {
          AILogDebug["do_spiderweb_roads"] << inventory_pos << " successfully built spider-web road between area_flag_pos " << area_flag_pos << " to other_area_flag_pos " << other_area_flag_pos;
          spider_web_roads_built++;
          // mark spiderweb roads on AI overlay
          //ai_mark_spiderweb_roads->push_back(built_road);
          sleep_speed_adjusted(3000);
          // only create one road per run
          break;
        }
        else {
          AILogDebug["do_spiderweb_roads"] << inventory_pos << " failed to build spider-web road between area_flag_pos " << area_flag_pos << " to other_area_flag_pos " << other_area_flag_pos;
        }
      }
      if (was_built){ break; }
    }
    if (was_built){ break; }
  }

  AILogDebug["do_spiderweb_roads"] << "sanity check: there were " << tried_pairs.size() << " tried pairs";
  AILogDebug["do_spiderweb_roads"] << inventory_pos << " done with building spider-web roads";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_spiderweb_roads"] << inventory_pos << " done with building spider-web roads call took " << duration;
}

// after the game has progressed a bit, add a bunch of flags to the roads immediately surrounding the castle
//  to disfavor transport paths that would otherwise route through the castle, encouraging alternate routes
void
AI::do_pollute_castle_area_roads_with_flags() {
  AILogDebug["do_pollute_castle_area_roads_with_flags"] << "inside do_pollute_castle_area_roads_with_flags";
  ai_status.assign("HOUSEKEEPING - do_pollute_castle_area_roads_with_flags");
  // only do this every X loops, and only once a certain number of huts have been built
  //  and don't do it again after a few more huts built, because it only ever needs to be done once
  unsigned int completed_huts = realm_completed_building_count[Building::TypeHut];
  if (loop_count % 30 != 0 || completed_huts < 15 || completed_huts >35) {
    AILogDebug["do_pollute_castle_area_roads_with_flags"] << "skipping do_pollute_castle_area_roads_with_flags roads, only running this every X loops, and if >Y and <Z knight huts built";
    return;
  }

  unsigned int created_flags = 0;
  for (unsigned int x = 0; x < AI::spiral_dist(8); x++) {
    MapPos pos = map->pos_add_extended_spirally(castle_flag_pos, x);
    if (map->has_any_path(pos)){
      if (game->can_build_flag(pos, player)) {
        AILogDebug["do_pollute_castle_area_roads_with_flags"] << "building a pollution flag at pos " << pos;
        mutex_lock("AI::do_pollute_castle_area_roads_with_flags calling game->build_flag");
        if (game->build_flag(pos, player)) {
          created_flags++;
        }
        mutex_unlock();
      }
    }
  }
  AILogDebug["do_pollute_castle_area_roads_with_flags"] << "done do_pollute_castle_area_roads_with_flags, created " << created_flags << " new flags";
}



void
AI::do_fix_stuck_serfs() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();

  AILogDebug["do_fix_stuck_serfs"] << "inside do_fix_stuck_serfs";
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
      AILogDebug["do_fix_stuck_serfs"] << "SerfWaitTimer serf is no longer stuck, skipping";
      AILogDebug["do_fix_stuck_serfs"] << "SerfWaitTimer erasing serf_wait_idle_on_road_timer for serf " << serf->get_index() << " at pos " << serf->get_pos();
      // found in C++ example... iterator is set by the result of the erase call on the current iterator pos (which is serf_index)
      serf_wait_idle_on_road_timer = serf_wait_idle_on_road_timers.erase(serf_wait_idle_on_road_timer);
    }
    else {
      if (game->get_tick() > trigger_tick) {
        AILogDebug["do_fix_stuck_serfs"] << "SerfWaitTimer triggering serf_wait_idle_on_road_timer for serf " << serf->get_index() << " and serf type " << NameSerf[serf->get_type()] << " at pos " << serf->get_pos();

        //AILogWarn["do_fix_stuck_serfs"] << "SerfWaitTimer detected WAIT_IDLE_ON_PATH STUCK SERF at pos " << serf->get_pos() << " of type " << NameSerf[serf->get_type()] << ", marking its current pos in lt_purple";
        //ai_mark_pos.insert(ColorDot(serf->get_pos(), "lt_purple"));
        // I THINK I FIXED THIS ISSUE FOR GOOD - see https://github.com/freeserf/freeserf/issues/492
        //  changing this to a crash exception in case I am wrong about the fix being 100% effective
        //  nope it sitll happens just less often, I might be wrong about the root cause
        // UPDATE - Dec04 2022, I have not seen this in a long time, removing sleep, throwing exception
        //   NOPE, it definitely still happens I just wasn't noticing
        //AILogError["do_fix_stuck_serfs"] << "SerfWaitTimer detected WAIT_IDLE_ON_PATH STUCK SERF at pos " << serf->get_pos() << " of type " << NameSerf[serf->get_type()] << ", throwing exception";
        //AILogError["do_fix_stuck_serfs"] << "SerfWaitTimer detected WAIT_IDLE_ON_PATH STUCK SERF at pos " << serf->get_pos() << " of type " << NameSerf[serf->get_type()] << ", marking it in purple and pausing game";
        AILogError["do_fix_stuck_serfs"] << "SerfWaitTimer detected WAIT_IDLE_ON_PATH STUCK SERF at pos " << serf->get_pos() << " of type " << NameSerf[serf->get_type()] << ", will boot this serf to avoid gridlock";
        //throw ExceptionFreeserf("SerfWaitTimer detected WAIT_IDLE_ON_PATH STUCK SERF at pos - I THOUGHT I FIXED THIS");

        // don't erase timer if problem isn't fixed yet, keep checking each AI loop
        //AILogWarn["do_fix_stuck_serfs"] << "WARNING - SerfWaitTimer attempting to set serf to lost state, updating marking to purple";
        //ai_mark_pos.erase(serf->get_pos());
        //ai_mark_pos.insert(ColorDot(serf->get_pos(), "purple"));
        //game->pause();
        
        Serf::Type serf_job = serf->get_type();
        // got a nullptr here for flag->get_position, maybe break it up and mutex lock?
        // got another nullptr. trying a slight modification...
        //  haven't seen a nullptr in forever here, think its fixed  (oct28 2020)
        MapPos serf_dest_flag_pos = bad_map_pos;
        Flag *serf_dest_flag = game->get_flag(serf->get_walking_dest());
        if (serf_dest_flag == nullptr) {
          AILogWarn["do_fix_stuck_serfs"] << "WARNING - the flag that is the walking dest of serf about to be booted is nullptr!  serf_dest_flag_pos will remain bad_map_pos";
        }
        else {
          serf_dest_flag_pos = serf_dest_flag->get_position();
        }
        // more info, saw case of a stuck Digger (leveller) that was on way to a construction site, but when stuck his walking_dest flag was a flag somewhere totally different
        //   in the realm and not a place where a Digger was even needed.  Maybe a bad pointer somewhere is causing the walking_dest to be set to someplace invalid and this is
        //     the cause of the stuck serf WAIT_IDLE_ON_PATH issue???
        //debug
        if (game->get_flag(serf->get_recent_dest()) != nullptr){
          AILogWarn["do_fix_stuck_serfs"] << "WARNING - serf of type " << NameSerf[serf_job] << " being set to Lost, dest when it exited Inventory was flag #" << serf->get_recent_dest() << " which has pos " << game->get_flag(serf->get_recent_dest())->get_position();
        }else{
          AILogWarn["do_fix_stuck_serfs"] << "WARNING - serf of type " << NameSerf[serf_job] << " being set to Lost, dest when it exited Inventory was flag #" << serf->get_recent_dest() << " which is a nullptr";
        }
        AILogWarn["do_fix_stuck_serfs"] << "WARNING - about to boot serf with job type: " << serf->get_type() << " " << NameSerf[serf->get_type()] << " with current walking_dest/flag_pos " << serf_dest_flag_pos << " and original dest-when-leaving-Inv flag index of " << serf->get_recent_dest();
        if (serf_job == Serf::TypeTransporter) {
          AILogWarn["do_fix_stuck_serfs"] << "WARNING - a transporter was booted, see if this causes flag at map_pos " << serf_dest_flag_pos << " to be without a transporter!";
        }
        if (serf_job != Serf::TypeTransporter && serf_job != Serf::TypeGeologist && serf_job != Serf::TypeDigger && serf_job != Serf::TypeBuilder
          && serf_job != Serf::TypeKnight0 && serf_job != Serf::TypeKnight1 && serf_job != Serf::TypeKnight2 && serf_job != Serf::TypeKnight3 && serf_job != Serf::TypeKnight4) {
          AILogWarn["do_fix_stuck_serfs"] << "WARNING - a building-occupying professional serf was booted, see if this causes building with flag pos " << serf_dest_flag_pos << " to stay forever unoccupied!";
        }
        if (serf_job == Serf::TypeKnight0 || serf_job == Serf::TypeKnight1 || serf_job == Serf::TypeKnight2 || serf_job == Serf::TypeKnight3 || serf_job == Serf::TypeKnight4) {
          AILogWarn["do_fix_stuck_serfs"] << "WARNING - a knight was booted, see if this causes military building with flag pos " << serf_dest_flag_pos << " to have a forever empty slot!";
        }
        mutex_lock("AI::do_fix_stuck_serfs calling serf->set_lost_state (for bug workaround stuck serfs)");
        serf->set_lost_state();
        mutex_unlock();
        // still seeing this happen for Transporter serfs... but rare... keep an eye on it
        // - the first time I watched this closely since tracking serf dests I saw a transporter on way to a flag
        //  to become a transporter, and whent it was made lost a replacement seems to have been sent, which is good
        // - second time, same, transporter booted, another eventually came to replace him and solved the problem
        // - third time, same, no issue
        // - fourth, same.  disabling this pausing
        //AILogError["do_fix_stuck_serfs"] << "pausing game for debugging stuck serf booting";
        //sleep_speed_adjusted(5000);
        //game->pause();
      }
      ++serf_wait_idle_on_road_timer;
    }
  }
  AILogDebug["do_fix_stuck_serfs"] << "SerfWaitTimer after checking timeouts, there are now " << serf_wait_idle_on_road_timers.size() << " TOTAL serf_wait_idle_on_road_timers set";

  // look for new waiting serfs and set serf_wait_timers
  mutex_lock("AI::do_fix_stuck_serfs calling game->get_player_serfs(player) (for serf_wait_timers StateWaitIdleOnPath)");
  // this returns a copy, so it should be thread-safe
  //  maybe not, beause game->get_player_serfs internally just does for (Serf *serf : serfs)
  for (Serf *serf : game->get_player_serfs(player)) {
    if (serf->get_state() == Serf::StateIdleInStock)
      continue;
    if (serf->get_state() == Serf::StateWaitIdleOnPath) {
      // see if a serf_wait_timer already set for this flag & dir
      if (serf_wait_idle_on_road_timers.count(serf->get_index()) == 0) {
        AILogDebug["do_fix_stuck_serfs"] << "SerfWaitTimer WAIT_IDLE_ON_PATH DETECTED setting countdown serf_wait_idle_on_road_timer for serf with index " << serf->get_index();
        serf_wait_idle_on_road_timers.insert(std::make_pair(serf->get_index(), game->get_tick() + 10000));
        AILogDebug["do_fix_stuck_serfs"] << "SerfWaitTimer WAIT_IDLE_ON_PATH DETECTED marking serf on AI overlay";
        ai_mark_serf.push_back(serf->get_index());
        //sleep_speed_adjusted(12000);
      }
      else {
        int trigger_ticks = static_cast<int>(serf_wait_idle_on_road_timers.at(serf->get_index()) - game->get_tick());
        AILogDebug["do_fix_stuck_serfs"] << "SerfWaitTimer WAIT_IDLE_ON_PATH a serf_wait_idle_on_road_timer is already set for this serf, it will trigger in " << trigger_ticks << " ticks";
      }
    }
  }
  mutex_unlock();
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_fix_stuck_serfs"] << "done do_fix_stuck_serfs call took " << duration;
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
  AILogDebug["do_fix_missing_transporters"] << "inside do_fix_missing_transporters";
  ai_status.assign("HOUSEKEEPING - send transporters");
  //
  // first, check any timers already set
  //
  AILogDebug["do_fix_missing_transporters"] << "there are " << no_transporter_timers.size() << " TOTAL no_transporter_timers set";
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
      AILogDebug["do_fix_missing_transporters"] << "triggering timer for flag with pos " << flag->get_position() << ", index " << flag_index << ", dir " << dir << " / " << NameDirection[dir];
      
      if (!flag->has_path_IMPROVED(dir)){
        AILogVerbose["do_fix_missing_transporters"] << "a path no longer exists for flag with pos " << flag->get_position() << ", index " << flag_index << ", dir " << dir << " / " << NameDirection[dir] << ", erasing timer";
        no_transporter_timer = no_transporter_timers.erase(no_transporter_timer);
      }
      else if (flag->has_transporter(dir)) {
        AILogVerbose["do_fix_missing_transporters"] << "looks like a transporter finally arrived, not calling another";
        AILogVerbose["do_fix_missing_transporters"] << "erasing timer for flag-dir " << flag_index << "-" << dir << " / " << NameDirection[dir];
        // found in C++ example... iterator is set by the result of the erase call on the current iterator pos (which is serf_index)
        no_transporter_timer = no_transporter_timers.erase(no_transporter_timer);
      }
      else {
        //AILogDebug["do_fix_missing_transporters"] << "timer detected BUG FOUND - NO TRANSPORTER on road at pos " << flag->get_position() << " in dir " << NameDirection[dir] << ", marking flag in cyan and dir in dk_cyan";
        // UPDATE Dec04 2022 - throw exception instead
        AILogError["do_fix_missing_transporters"] << "timer detected BUG FOUND - NO TRANSPORTER on road at pos " << flag->get_position() << " in dir " << NameDirection[dir] << ", throwing exceptio";
        //ai_mark_pos.insert(ColorDot(flag->get_position(), "cyan"));
        //ai_mark_pos.insert(ColorDot(map->move(flag->get_position(), dir), "dk_cyan"));
        //sleep_speed_adjusted(5000);
        //game->pause();
        AILogDebug["do_fix_missing_transporters"] << "timer trying to force call a transporter";
        mutex_lock("AI::do_fix_missing_transporters calling flag->call_transporter, to work around no-transporter issue");
        // got access violation w/ 2x AIs, even with mutex, look for a foreach loop being invalidated elsewhere
        //Access violation reading location 0xFFFFFFFFFFFFFFFF.
        // oh... I think I just wasn't checking that this player owns the flag!  adding that
        // now getting some other read access violation after Inventory->have serfs... on Load Game testing though, dunno
        //AILogDebug["do_fix_missing_transporters"] << "NOT calling out missing new transporterZZZ, trying to debug this now";
        //bool was_called = false;
        bool was_called = flag->call_transporter(dir, false);  // hardcoding is_water_path to false because I am seeing weird crash with this checking for Serf::TypeSailor
        mutex_unlock();
        if (!was_called) {
          AILogDebug["do_fix_missing_transporters"] << "WARNING - flag->call_transporter failed while trying to work around no-transporter issue!  I guess let it try again next time";
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
  //AILogDebug["do_fix_missing_transporters"] << "aftebuild warer checking timeouts, there are now " << no_transporter_timers.size() << " TOTAL no_transporter_timers set";

  //
  // look for missing transporters and if found set timers for them
  //
  unsigned int flag_index = 0;
  Flags flags_copy = *(game->get_flags());  // create a copy so we don't conflict with the game thread, and don't want to mutex lock for a long function
  for (Flag *flag : flags_copy) {
    if (flag == nullptr || flag->get_owner() != player_index || !flag->is_connected())
      continue;
    // it seems the castle shows as having a path Up-Left into it...
    //    I wonder if any other buildings do also?  warehouses?
    // YES!  I saw other mention such as in path_splited() that at least 
    //  some buildings show up as having a valid path UP-LEFT
    // YES, it seems *every* building has a path up-left into it from its flag
    //  and this path is very real in the sense that it is drawn on game map
    //  if you remove it using code, the building and flag are still drawn
    //   and serfs will still walk the path in/out of building, but the path itself
    //   will no longer be drawn
    if (flag->get_position() == castle_flag_pos)
      continue;
    flag_index = flag->get_index();
    for (Direction dir : cycle_directions_cw()) {
      if (!map->has_path_IMPROVED(flag->get_position(), dir))
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
        if (!map->has_path_IMPROVED(flag->get_position(), dir)) {
          AILogVerbose["do_fix_missing_transporters"] << "no path found for flag with pos " << flag->get_position() << ", index " << flag_index << ", dir " << dir << " / " << NameDirection[dir] << " during no_transporter check!  FIND OUT WHY";
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
            if (map->has_path_IMPROVED(pos, new_dir) && new_dir != reverse_direction(tmp_dir)) {
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
          // UPDATE Dec04 2022 - changing this to throw exception, not sure how often it has been happening
          //     It is definitely still happening, not often but saw it again same day
          //AILogDebug["do_fix_missing_transporters"] << "WARNING - found rare type of missing transporter bug!  Flag #" << flag->get_index() << " at pos " << flag->get_position() << " seems to be missing a transporter on road in dir " << NameDirection[dir] << " despite it thinking there is one there!  pausing game and marking pos in lt_blue";
          //AILogError["do_fix_missing_transporters"] << "WARNING - found rare type of missing transporter bug!  Flag #" << flag->get_index() << " at pos " << flag->get_position() << " seems to be missing a transporter on road in dir " << NameDirection[dir] << " despite it thinking there is one there!  throwing exception";
          AILogError["do_fix_missing_transporters"] << "WARNING - found rare type of missing transporter bug!  Flag #" << flag->get_index() << " at pos " << flag->get_position() << " seems to be missing a transporter on road in dir " << NameDirection[dir] << " despite it thinking there is one there!  attempting to call another to work around the bug";
          //ai_mark_pos.insert(std::make_pair(flag->get_position(), "lt_blue"));
          //sleep_speed_adjusted(5000);
          //game->pause();
          //throw ExceptionFreeserf("found rare type of missing transporter bug!  seems to be missing a transporter on road in dir despite it thinking there is one there!");
          //AILogDebug["do_fix_missing_transporters"] << "NOT calling out missing new transporter, trying to debug this now";
          //bool was_called = false;
          AILogWarn["do_fix_missing_transporters"] << "trying to immediately force call a transporter";
          mutex_lock("do_fix_missing_transporters calling flag->call_transporter, to work around RARER no-transporter issue");
          bool was_called = flag->call_transporter(dir, false);  // hardcoding is_water_path to false because I am seeing weird crash with this checking for Serf::TypeSailor
          mutex_unlock();

          /*
          if (!was_called) {
            AILogDebug["do_fix_missing_transporters"] << "WARNING - flag->call_transporter to " << flag->get_position() << ", dir " << NameDirection[dir] << " - failed while trying to work around RARER no-transporter issue!  I guess let it try again next time";
          }
          sleep_speed_adjusted(5000);
          */
          //game->pause();
        }
        else {
          AILogVerbose["do_fix_missing_transporters"] << "found expected transporter with Flag #" << flag->get_index() << " at pos " << flag->get_position() << " on road in dir " << NameDirection[dir];
        }

      }
      /* I think I finally fixed this, it was due to typo in the path_splited road splitting logic
      //
      // check for common missing transporter bug, where no transporter is assigned at all
      //  I added some debugging and I see that the leave_building call is made so a serf is dispatched, but not sure if it ever actually
      //  exits the Inventory (castle/stock) and starts travelling and is lost along the way, or if it never actually starts walking
      //
      if (!flag->has_transporter(dir)) {
        AILogDebug["do_fix_missing_transporters"] << "flag at pos " << flag->get_position() << " has no transporter on path in dir " << dir << " / " << NameDirection[dir];
        // check to see if one was requested
        if (flag->serf_requested(dir)){
          AILogDebug["do_fix_missing_transporters"] << "flag->serf_requested is true at pos " << flag->get_position() << " in dir " << dir << " / " << NameDirection[dir];
        }else{
          AILogDebug["do_fix_missing_transporters"] << "flag->serf_requested is FALSE at pos " << flag->get_position() << " in dir " << dir << " / " << NameDirection[dir] << ", marking flag in yellow and dir in dk_yellow";
          // this happens when flags/roads/buildings were just created, not an issue as far as I have ever seen
          //ai_mark_pos.insert(ColorDot(flag->get_position(), "yellow"));
          //ai_mark_pos.insert(ColorDot(map->move(flag->get_position(), dir), "dk_yellow"));
          //sleep_speed_adjusted(3000);
        }
        // maybe check to see if there is a Walking Transporter serf whose dest is this path?
        // see if a timer already set for this flag & dir
        AILogDebug["do_fix_missing_transporters"] << "there are " << no_transporter_timers.count(std::make_pair(flag_index, dir)) << " no_transporter_timers set for this flag with pos " << flag->get_position() << ", index " << flag_index << ", dir " << dir << " / " << NameDirection[dir];
        if (no_transporter_timers.count(std::make_pair(flag_index, dir)) == 0) {
          AILogDebug["do_fix_missing_transporters"] << "setting countdown timer for flag with pos " << flag->get_position() << ", index " << flag_index << ", dir " << dir << " / " << NameDirection[dir];
          // this is difficult to tune... if too long the entire economy can break down while waiting to clear it
          //    but too soon as it could trigger for a faraway road that the original transporter simply hasn't reached
          //  Maybe make more advanced and check to see if there is an outgoing transporter serf with this destination flag/dir??
          // jan05 2021 - setting this 25x now that I am looking at it closer.  It should be either extremely long timer or
          //    based on the distance from the inventory (warehouse/stock) that is dispatching the serf
          no_transporter_timers.insert(std::make_pair(std::make_pair(flag_index, dir), game->get_tick() + 50000);
          AILogDebug["do_fix_missing_transporters"] << "there are now " << no_transporter_timers.count(std::make_pair(flag_index, dir)) << " no_transporter_timers set for this flag with pos " << flag->get_position() << ", index " << flag_index << ", dir " << dir << " / " << NameDirection[dir] << ", value is: " << no_transporter_timers.at(std::make_pair(flag_index, dir));

          // try to find a serf that is on way to this Flag-dir
          AILogVerbose["do_fix_missing_transporters"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is locking mutex before calling game->get_player_serfs(player) (for do_fix_missing_transporters)";
          game->get_mutex()->lock();
          AILogVerbose["do_fix_missing_transporters"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has locked mutex before calling game->get_player_serfs(player) (for do_fix_missing_transporters)";
          for (Serf *serf : game->get_player_serfs(player)) {
            if (serf->get_walking_dest() == flag_index){
              AILogDebug["do_fix_missing_transporters"] << "serf at pos " << serf->get_pos() << " with type " << NameSerf[serf->get_type()] << " has dest of this Flag with index " << flag_index << " and pos " << flag->get_position();
              ai_mark_serf.push_back(serf->get_index());
            }
          }
          AILogVerbose["do_fix_missing_transporters"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI is unlocking mutex before calling game->get_player_serfs(player) (for do_fix_missing_transporters)";
          game->get_mutex()->unlock();
          AILogVerbose["do_fix_missing_transporters"] << inventory_pos << " thread #" << std::this_thread::get_id() << " AI has unlocked mutex before calling game->get_player_serfs(player) (for do_fix_missing_transporters)";
          
        } else {
          int trigger_ticks = static_cast<int>(no_transporter_timers.at(std::make_pair(flag_index, dir)) - game->get_tick());
          AILogDebug["do_fix_missing_transporters"] << "a timer is already set for flag with pos " << flag->get_position() << ", index " << flag_index << ", dir " << dir << " / " << NameDirection[dir] << ", it will trigger in " << trigger_ticks << " ticks";
        }
      }
      */
    }
  }
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_fix_missing_transporters"] << "done do_fix_missing_transporters call took " << duration;
}


// after fixing the idle_serfs issue by adding a call to get_serfs
//  I still see a few extra geologists being created, I think this may be
//  when excess hammers exist that are consumed by new builders, etc. at the
//  same time that geologists are consuming them as the AI may build 6 or 7
//  buildings in parallel if conditions are right during the early game
//
// another idea to limit excess geologists - don't build hammers once a certain
//  number of builders reached, UNLESS a new unoccupied blacksmith exists and
//  no blacksmith/hammers available to fill it
//
void
AI::do_send_geologists() {
  //
  // send geologists to hills
  //
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  AILogDebug["do_send_geologists"] << inventory_pos << " starting";
  ai_status.assign("do_send_geologists");

  // throttle sending geologists after game progresses a bit
  int occupied_huts = realm_occupied_building_count[Building::TypeHut];
  if (occupied_huts >= 6){
    if (last_sent_geologist_tick + 10000 > game->get_tick()){
      AILogDebug["do_send_geologists"] << "it has been less than 10000 ticks since a geologist was last sent, not sending any now.  last_sent_geologist_tick " << last_sent_geologist_tick << ", current tick " << game->get_tick();
      return;
    }
  }

  MapPosSet count_by_corner;
  MapPosVector geologist_positions;
  /* don't stop sending geologists
  // don't send geologists if already have enough mines for this Inventory pos
  int completed_coalmine_count = stock_building_counts.at(inventory_pos).completed_count[Building::TypeCoalMine];
  int completed_ironmine_count = stock_building_counts.at(inventory_pos).completed_count[Building::TypeIronMine];
  int completed_goldmine_count = stock_building_counts.at(inventory_pos).completed_count[Building::TypeGoldMine];
  int total_coalmine_count = stock_building_counts.at(inventory_pos).count[Building::TypeCoalMine];
  int total_ironmine_count = stock_building_counts.at(inventory_pos).count[Building::TypeIronMine];
  int total_goldmine_count = stock_building_counts.at(inventory_pos).count[Building::TypeGoldMine];
  // true if at least one COMPLETED mine of each type, plus type-specific max which includes placed-but-incomplete mines
  if ((completed_coalmine_count >= 1 && completed_ironmine_count >= 1 && completed_goldmine_count) &&
    (total_coalmine_count >= max_coalmines && total_ironmine_count >= max_ironmines && total_goldmine_count >= max_goldmines)) {
    // maybe make this simply send geologists far less often... one every 10 loops?
    AILogDebug["do_send_geologists"] << inventory_pos << " enough mines placed, not sending anymore geologists";
    return;
  }
  */

  // IMPORTANT - refresh serfs or idle counts will be wrong 
  //  especially once Stocks built
  do_get_serfs();

  // figure out if we should be creating more geologists by counting the number
  //   of excess hammers (not reserved for future builders or blacksmiths)
  unsigned int total_geologists = player->get_serf_count(Serf::TypeGeologist);
  unsigned int idle_geologists = serfs_idle[Serf::TypeGeologist];
  unsigned int potential_geologists = 0;
  unsigned int hammers_count = realm_inv[Resource::TypeHammer];
  int reserve_hammers = 2;
  int excess_hammers = static_cast<int>(hammers_count - reserve_hammers);
  unsigned int adjusted_geologists_max = 2 + (geologists_max * stock_building_counts.size());
  AILogDebug["do_send_geologists"] << inventory_pos << " total_geologists " << total_geologists << ", idle_geologists " << idle_geologists << ", geologists_max "
    << geologists_max << ", hammers_count " << hammers_count << ", reserve_hammers " << reserve_hammers << ", excess_hammers " << excess_hammers << ", adjusted_geologists_max " << adjusted_geologists_max;
  

  // another way to try to limit creation of excess geologists
  int working_geologists = total_geologists - idle_geologists;
  if (working_geologists >= adjusted_geologists_max){
    AILogDebug["do_send_geologists"] << inventory_pos << " working_geologists " << working_geologists << " has reached adjusted_geologists_max " << adjusted_geologists_max << ", not sending any geologists until some return";
    return;
  }

  // only create new geologists is sufficient hammers
  if (excess_hammers > 0) {
    // its possible to accidentally create > geologists_max, so if this happens avoid going negative
    //   initial max is 4, adds 2 for each Stock built
    AILogDebug["do_send_geologists"] << inventory_pos << " debug - adjusted_geologists_max is " << adjusted_geologists_max;
    if (total_geologists >= adjusted_geologists_max) {
      AILogDebug["do_send_geologists"] << inventory_pos << " adjusted_geologists_max " << adjusted_geologists_max << " hit, have total_geologists " << total_geologists;
      potential_geologists = 0;
      // it seems there is an issue where idle_geologists is appearing greater than it really is, resulting in more geologists being created
      //  even after geologists_max hit.  As a work-around to hopefully avoid this, never allow idle_geologists to be >1 once geologists_max hit
      if (idle_geologists > 1) {
        idle_geologists = 1;
      }
    }
    else {
      // cap the number of potential geologists at the number of excess hammers (those not reserved for builder/blacksmith)
      potential_geologists = unsigned(excess_hammers) > adjusted_geologists_max - total_geologists ? adjusted_geologists_max - total_geologists : unsigned(excess_hammers);
      AILogDebug["do_send_geologists"] << inventory_pos << " at least one excess hammer, potential_geologists set to " << potential_geologists;
    }
  }
  else {
    AILogDebug["do_send_geologists"] << inventory_pos << " hammers_count <= hammers_reserve, potential_geologists set to zero.  Existing geologists: " << total_geologists;
  }

  if (idle_geologists < 1 && potential_geologists < 1){
    AILogDebug["do_send_geologists"] << inventory_pos << " no idle or potential geologists available, returning";
    return;
  }

  // determine where any geologists are currently operating (to later avoid sending too many to one area)
  mutex_lock("AI::do_send_geologists calling game->get_player_serfs(player) (for serf_wait_timers is_waiting)");
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
          AILogDebug["do_send_geologists"] << inventory_pos << " a geologist has walking_dest of flag at pos " << flag->get_position();
          pos = flag->get_position();
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
        AILogDebug["do_send_geologists"] << inventory_pos << " marking a geologists_positions at pos " << pos;
        geologist_positions.push_back(pos);
        for (MapPos geo_pos : geologist_positions){
          AILogDebug["do_send_geologists"] << inventory_pos << " geologists_positions contains a geo at pos " << geo_pos;
        }
      }
    }
  }
  mutex_unlock();
  AILogDebug["do_send_geologists"] << inventory_pos << " active geologists found: " << geologist_positions.size();

  // yet another attempt at limiting excess geologists
  if (geologist_positions.size() >= adjusted_geologists_max){
    AILogDebug["do_send_geologists"] << inventory_pos << " there are more active_geologists than max_geologists!  not sending more";
    return;
  }
  
  // for mined resouce finding,  reverse the occupied_military_buildings order
  //   so the newest military building is searched first,
  //    instead of castle first then outward
  MapPosVector foo = stock_building_counts.at(inventory_pos).occupied_military_pos;

  // UPDATE - originally the occupied_militar_pos was iterated over in reverse order,
  //  resulting in geologists being sent to the NEWEST knight huts in the realm.  This is 
  //  good in some ways, but some spots are missed because new huts are built faster than
  //  geologists can be sent/created/work.  To balance this without totally losing the 
  //  preference for evaluating new positions, have a chance of shuffling the positions
  // 50% chance of shuffle
  if (rand() > RAND_MAX / 2){
    AILogDebug["do_send_geologists"] << inventory_pos << " using shuffled occupied_military_pos list for sending geologists";
    std::random_shuffle(foo.begin(), foo.end());
  }else{
    AILogDebug["do_send_geologists"] << inventory_pos << " using newest-first occupied_military_pos list for sending geologists";
  }

  int sent_this_run = 0;
  // for some reason after switching to parallel stocks/warehouse support, cannot use stock_building_counts.at(inventory_pos).occupied_military_pos iterator directly
  //  must copy it to another MapPosVector and iterate over that.  I'm sure there is a cleaner way
  //for (MapPosVector::reverse_iterator it = stock_building_counts.at(inventory_pos).occupied_military_pos.rbegin(); it != stock_building_counts.at(inventory_pos).occupied_military_pos.rend(); ++it) {
  for (MapPosVector::reverse_iterator it = foo.rbegin(); it != foo.rend(); ++it) {
    MapPos center_pos = *it;
    MapPosVector corners = AI::get_corners(center_pos);
    for (MapPos corner_pos : corners) {
      //AILogDebug["do_send_geologists"] << inventory_pos << " considering corner_pos " << corner_pos;
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
    //AILogDebug["do_send_geologists"] << inventory_pos << " trying to find a good place to send geologists";
    MapPosVector search_positions = AI::sort_by_val_desc(count_by_corner);
    MapPos built_pos = bad_map_pos;
    for (MapPos corner_pos : search_positions) {
      //AILogDebug["do_send_geologists"] << inventory_pos << " considering sending geologists to the vicinity of pos " << corner_pos;
      // if two or more geologists are already operating in this area, skip this corner
      int geologists_corner = 0;
      for (unsigned int i = 0; i < AI::spiral_dist(6); i++) {
        MapPos pos = map->pos_add_extended_spirally(corner_pos, i);
        unsigned int geologists_pos = static_cast<unsigned int>(std::count(geologist_positions.begin(), geologist_positions.end(), pos));
        if (geologists_pos > 0) {
          AILogDebug["do_send_geologists"] << inventory_pos << " there are " << geologists_pos << " geologists operating at pos " << pos;
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
        //AILogDebug["do_send_geologists"] << inventory_pos << " checking terrain type to see if can build flag at pos " << pos;
        if (!AI::has_terrain_type(game, pos, Map::TerrainTundra0, Map::TerrainSnow0))
          continue;
        // build flags for geologists
        bool built_new_flag_for_geologist = false;
        //AILogDebug["do_send_geologists"] << inventory_pos << " checking to see if can build flag at pos " << pos << ". this pos has owner " << map->get_owner(pos) << " and my player index is " << player->get_index() << ", has_owner is " << map->has_owner(pos);
        if (game->can_build_flag(pos, player)) {
          AILogDebug["do_send_geologists"] << inventory_pos << " can build flag at pos " << pos << ", checking to see if any other flags nearby...";
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
            mutex_lock("AI::do_send_geologists calling game->build_flag, for geologist");
            bool was_built = game->build_flag(pos, player);
            mutex_unlock();
            sleep_speed_adjusted(2000);
            if (!map->has_flag(pos)) {
              AILogDebug["do_send_geologists"] << inventory_pos << " failed to build flag at pos " << pos << "!!! why??";
            }
            else {
              AILogDebug["do_send_geologists"] << inventory_pos << " built flag at pos " << pos << ", trying to connect it...";
              built_new_flag_for_geologist = true;
            }
            
            Road notused; // not used here, can I just pass a zero instead of &notused to build_best_road and skip initialization of a wasted object?
            if (!AI::build_best_road(pos, road_options, &notused, "do_send_geologists")) {
              AILogDebug["do_send_geologists"] << inventory_pos << " failed to connect new gologist flag to road network!  removing the flag";
              mutex_lock("AI::do_send_geologists calling demolish_flag (built for geoligist, couldn't connect)");
              game->demolish_flag(pos, player);
              mutex_unlock();
              sleep_speed_adjusted(2000);
              AILogDebug["do_send_geologists"] << inventory_pos << " adding this flag pos " << pos << " to bad_building_pos list";
              // because there is no Building::Type for a plain flag, use Building::TypeNone for now.
              //  alternatively, could create a new type, or use some number that has no type
              bad_building_pos.insert(std::make_pair(pos, Building::TypeNone));
            }
          }
        }
        // send geologist
        if (map->has_flag(pos) && map->get_owner(pos) == player_index && game->get_flag_at_pos(pos) != nullptr && game->get_flag_at_pos(pos)->is_connected()) {
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
          bool created_new = false;
          if (idle_geologists >= 1) {
            AILogDebug["do_send_geologists"] << inventory_pos << " sending an idle geologist to pos " << pos;
          }
          else if (potential_geologists >= 1) {
            AILogDebug["do_send_geologists"] << inventory_pos << " creating a new geologist and sending to pos " << pos;
            created_new = true;
          }
          else {
            AILogDebug["do_send_geologists"] << inventory_pos << " no idle or potential geologists available, returning";
            return;
          }
          mutex_lock("AI::do_send_geologists calling game->send_geologist(flag)");
          bool was_sent = game->send_geologist(flag);
          mutex_unlock();
          if (was_sent) {
            AILogDebug["do_send_geologists"] << inventory_pos << " sent an geologist to pos " << pos << ", moving on to next corner";
            idle_geologists--;
            if (created_new){
              total_geologists++;
              potential_geologists--;
            }
            /* back to only allowing one per run since improving sleep speeds to be gamespeed adjusted
            // only allow up to two geos sent per run
            sent_this_run++;
            if (sent_this_run > 1){
              AILogDebug["do_send_geologists"] << inventory_pos << " sent " << sent_this_run << " geologists this run, stopping";
              return;
            }
            break;
            */
            // because of incredibly frustrating "too many geologists" / idle_geologists issue, only send one geologist per call of this function.
            //   and even THIS will probably not work right for warehouse/stocks
            //AILogDebug["do_send_geologists"] << inventory_pos << " sent a geologist to pos " << pos << ", not sending any more geologists until next call of this function";

            // ugh still to many no matter what I do
            last_sent_geologist_tick = game->get_tick();
            return;
            //AILogDebug["do_send_geologists"] << inventory_pos << " sent a geologist to pos " << pos;
          }
          else {
            AILogDebug["do_send_geologists"] << inventory_pos << " failed to send geologist to pos " << pos << ".  This happens sometimes but appears benign.  Sleeping 1sec";
            //AILogDebug["do_send_geologists"] << inventory_pos << " game CLAIMS a geologist failed to send, but I suspect something funny going on, assuming it was actually sent to avoid too many geos";
            // actually, if this happens quit the function
            return;
          }
          sleep_speed_adjusted(2000);
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
  AILogDebug["do_build_rangers"] << "inside do_build_rangers";
  //
  // build ranger near lumberjacks that have few trees and no ranger nearby
  //
  AILogDebug["do_build_rangers"] << "HouseKeeping: build rangers near lumberjacks that have few trees and no ranger nearby";
  ai_status.assign("HOUSEKEEPING - build rangers");
  if (stock_building_counts.at(inventory_pos).needs_wood) {
    AILogInfo["do_build_stonecutter"] << inventory_pos << " need wood, considering building rangers";
    Game::ListBuildings buildings = game->get_player_buildings(player);
    for (Building *building : buildings) {
      if (building == nullptr)
        continue;
      if (building->get_type() != Building::TypeLumberjack)
        continue;
      if (find_nearest_inventory(map, player_index, building->get_position(), DistType::FlagOnly, &ai_mark_pos) != inventory_pos)
          continue;
      MapPos pos = building->get_position();
      unsigned int count = AI::count_objects_near_pos(pos, AI::spiral_dist(4), Map::ObjectTree0, Map::ObjectPine7, "lt_green");
      AILogDebug["do_build_rangers"] << "lumberjack trees nearby count: " << count << ", min acceptable is " << near_trees_min;
      if (count >= near_trees_min)
        continue;
      if (AI::building_exists_near_pos(pos, AI::spiral_dist(8), Building::TypeForester))
        continue;
      AILogDebug["do_build_rangers"] << "lumberjack at " << pos << " has < min trees and no ranger nearby, trying to place ranger";
      MapPos built_pos = AI::build_near_pos(pos, AI::spiral_dist(6), Building::TypeForester);
      if (built_pos != bad_map_pos && built_pos != notplaced_pos)
        AILogDebug["do_build_rangers"] << "built ranger at pos " << built_pos;
    }
  }else{
    AILogInfo["do_build_stonecutter"] << inventory_pos << " have sufficient wood, not considering building rangers";
  }
  AILogDebug["do_build_rangers"] << "done do_build_rangers";
}

void
AI::do_demolish_unproductive_3rd_lumberjacks() {
  AILogDebug["do_demolish_unproductive_3rd_lumberjacks"] << "inside do_demolish_unproductive_3rd_lumberjacks";
  ai_status.assign("do_demolish_unproductive_3rd_lumberjacks");
  Game::ListBuildings buildings = game->get_player_buildings(player);
  // find all sawmills in the realm, and check the area around each one
  // if three completed lumberjacks and a ranger are nearby, but still not many trees,
  //   then burn one lumberjack to allow it to be replaced elsewhere by the do_food_buildings_and_3rd_lumberjack function
  for (Building *building : buildings) {
    if (building == nullptr)
      continue;
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
        AILogDebug["do_demolish_unproductive_3rd_lumberjacks"] << "3rd lumberjack at pos " << lumberjack_pos << " has a nearby ranger yet still only has " << mature_tree_count << ", less than near_trees_min " << near_trees_min << ".  Burning it so it can be replaced in a better spot";
        mutex_lock("AI::do_demolish_unproductive_3rd_lumberjacks calling game->demolish_building");
        game->demolish_building(lumberjack_pos, player);
        mutex_unlock();
        // sleep to appear more human
        sleep_speed_adjusted(3000);
        break;
      }
    }
  }
  AILogDebug["do_demolish_unproductive_3rd_lumberjacks"] << "done do_demolish_unproductive_3rd_lumberjacks";
}



// remove roads that lead to:
//  - an occupied ranger building, if no other paths from ranger flag
//  - a mountain/geologist road, if sign density > max
//  - a non-mountain road that dead-ends with no building (likely because building was demolished)
// also:  fully-constructed, occupied huts that have another connected 
//  flag nearby that can be successfully connected to instead (and do so)
// UPDATE - adding logic to avoid destroying roads/flags that have resources!
//  either sitting at flags, or being carried by transporters on the road
void
AI::do_remove_road_stubs() {
  AILogDebug["do_remove_road_stubs"] << "inside do_remove_road_stubs";
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  unsigned int roads_removed = 0;
  //
  // ...an occupied ranger building, if no other paths from ranger flag
  //
  Game::ListBuildings buildings = game->get_player_buildings(player);
  for (Building *building : buildings) {
    if (building == nullptr)
      continue;
    if (building->get_type() != Building::TypeForester)
      continue;
    if (!building->is_done() || !building->has_serf())
      continue;
    MapPos pos = building->get_position();
    MapPos flag_pos = map->move_down_right(pos);
    Direction road_dir = DirectionNone;
    if (flag_and_road_suitable_for_removal(game, map, flag_pos, &road_dir)) {
      AILogDebug["do_remove_road_stubs"] << "occupied ranger at pos " << pos << "'s flag has only one path and no resources, removing the stub road";
      mutex_lock("AI::do_remove_road_stubs calling game->demolish_road() (for do_remove_road_stubs) for ranger");
      game->demolish_road(map->move(flag_pos, road_dir), player);
      mutex_unlock();
      roads_removed++;
      // sleep a bit to be more human like
      sleep_speed_adjusted(3000);
    }
  }
  //
  // a mountain/geologist road, if sign density > max
  //
  Flags flags_copy = *(game->get_flags());  // create a copy so we don't conflict with the game thread, and don't want to mutex lock for a long function
  for (Flag *flag : flags_copy) {
    if (flag == nullptr || flag->get_owner() != player_index || !flag->is_connected() || flag->has_building())
      continue;
    MapPos flag_pos = flag->get_position();
    if (!AI::has_terrain_type(game, flag_pos, Map::TerrainTundra0, Map::TerrainSnow1))
      continue;
    // if *either* condition is true, stub road is eligible for removal:
    //  - if sign density is > max
    //  - if another flag on a mountain tile is very close
    bool eligible = false;
    if (AI::count_geologist_sign_density(flag_pos, AI::spiral_dist(4)) > geologist_sign_density_max){
      AILogDebug["do_remove_road_stubs"] << "flag at pos " << flag_pos << " is eligible because sign_density > max";
      eligible = true;
    }
    else {
      for (unsigned int i = 1; i < AI::spiral_dist(2); i++) {
        MapPos pos = map->pos_add_extended_spirally(flag_pos, i);
        if (map->has_flag(pos) && map->get_owner(pos) == player_index && game->get_flag_at_pos(pos) != nullptr && game->get_flag_at_pos(pos)->is_connected()
            && has_terrain_type(game, pos, Map::TerrainTundra0, Map::TerrainSnow1)) {
          eligible = true;
          //AILogDebug["do_remove_road_stubs"] << "flag at pos " << flag_pos << " is eligible because another connected flag on a mountain tile, at pos " << pos << ", is very close";
          break;
        }
      }
    }
    if (!eligible)
      continue;
    Direction road_dir = DirectionNone;
    if (flag_and_road_suitable_for_removal(game, map, flag_pos, &road_dir)) {
      AILogDebug["do_remove_road_stubs"] << "eligible geologist road ending with flag at pos " << flag_pos << " has only one path and no resources, removing the stub road and its end flag";
      mutex_lock("AI::do_remove_road_stubs calling game->demolish_road() and flag (for do_remove_road_stubs) for geologist flag");
      game->demolish_road(map->move(flag_pos, road_dir), player);
      game->demolish_flag(flag_pos, player);
      mutex_unlock();
      roads_removed++;
      // sleep a bit to be more human like
      sleep_speed_adjusted(3000);
    }
  }
  //
  // any non-mountain road that dead-ends and has no building attached
  //
  flags_copy = *(game->get_flags());  // refresh the a copy
  for (Flag *flag : flags_copy) {
    if (flag == nullptr || flag->get_owner() != player_index || !flag->is_connected() || flag->has_building())
      continue;
    MapPos flag_pos = flag->get_position();
    if (AI::has_terrain_type(game, flag_pos, Map::TerrainTundra0, Map::TerrainSnow1))
      continue;
    Direction road_dir = DirectionNone;
    if (flag_and_road_suitable_for_removal(game, map, flag_pos, &road_dir)) {
      AILogDebug["do_remove_road_stubs"] << "eligible non-mountain road ending with flag at pos " << flag_pos << " has only one path and no resources, removing the stub road and its end flag";
      mutex_lock("AI::do_remove_road_stubs calling game->demolish_road() and flag (for do_remove_road_stubs) for non-mountain flag");
      game->demolish_road(map->move(flag_pos, road_dir), player);
      game->demolish_flag(flag_pos, player);
      mutex_unlock();
      roads_removed++;
      // sleep a bit to be more human like
      sleep_speed_adjusted(3000);
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
  //  BUG - saw (only once) a "flapping" road being rebuilt every run between
  //  two equally near flags, add check to esnure the new road is actually better
  //    DID THIS (added length check) - does it work?     NO I SAW FLAPPING AGAIN WHEN ONE ROAD WAS CLEARLY LONGER
  //
  if (loop_count % 6 != 0) {
    AILogDebug["do_remove_road_stubs"] << "skipping eligible knight hut stub roads, only running this every X loops";
  }else{
    flags_copy = *(game->get_flags());  // refresh the copy again
    for (Flag *flag : flags_copy) {
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
      size_t current_road_length = 0;
      // NOTE - not using flag_and_road_suitable_for_removal function here because knight roads are
      //  unlikely to have transporters carrying resources AND have only one path, and because
      //  the path will be re-created another way there is no concern about losing resources at flag
      //  and we have to check paths here anyway to get the Road object and the length
      for (Direction dir : cycle_directions_cw()) {
        if (!map->has_path_IMPROVED(flag_pos, dir)) { continue; }
        paths++;
        if (paths > 1) {
          //AILogDebug["do_remove_road_stubs"] << "eligible knight hut stub road ending with flag at pos " << flag_pos << " has more than one path, not removing road";
          break;
        }
        // only consider removing if its road is over a certain length
        //  and also store the Road found for later checking to prevent self-connection
        existing_road = trace_existing_road(map, flag_pos, dir);
        current_road_length = existing_road.get_length();
        //if (current_road_length < 6){
        if (current_road_length < 4){  // jan02 2022 reducing this from 6 to 4
          //AILogDebug["do_remove_road_stubs"] << "eligible knight hut stub road ending with flag at pos " << flag_pos << " is too short to bother with, not removing road";
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
      for (unsigned int i = 0; i < AI::spiral_dist(5); i++) {
        MapPos pos = map->pos_add_extended_spirally(flag->get_building()->get_position(), i);
        if (map->has_flag(pos) && pos != flag_pos && game->get_flag_at_pos(pos)->get_owner() == player_index && game->get_flag_at_pos(pos) != nullptr && game->get_flag_at_pos(pos)->is_connected()
         || (game->can_build_flag(pos, player) && map->has_any_path(pos))) {

          //if (map->has_flag(pos) && pos != flag_pos && flag->get_owner() == player_index && flag->is_connected()){
          //  AILogDebug["do_remove_road_stubs"] << "debug - suitable connected flag found at pos " << pos;
          //}

          // if this is a new splitting flag, make sure it isn't part of the same stub road we want to remove
          bool needs_flag = false;
          if (!map->has_flag(pos)){
            //AILogDebug["do_remove_road_stubs"] << "debug - need to build new flag at pos " << pos;
            needs_flag = true;
          }
          if (existing_road.has_pos(map.get(), pos)){
            //AILogDebug["do_remove_road_stubs"] << "eligible knight hut stub road ending with flag at pos " << flag_pos << " skipping pos " << pos << " as it is part of the existing road";
            continue;
          }

          AILogDebug["do_remove_road_stubs"] << "eligible knight hut stub road ending with flag at pos " << flag_pos << " with one path and suitable length, found nearby flag/pos at " << pos;

          // replacing build_best_road with direct road plot, to allow comparing length
          //    might be able to use Improve roads function but I don't trust it enough
          //Road not_used;
          //was_built = AI::build_best_road(flag->get_position(), road_options, &not_used, "do_remove_road_stubs", Building::TypeNone, Building::TypeNone, pos, false);
          Roads notused;  // not used here
          Road proposed_direct_road = plot_road(map, player_index, flag->get_position(), pos, &notused);
          size_t new_road_length = proposed_direct_road.get_length();
          if (new_road_length == 0){
            AILogDebug["do_remove_road_stubs"] << "eligible knight hut stub road ending with flag at pos " << flag_pos << ", failed to plot replacement road to flag/pos " << pos << ", will keep trying";
            continue;
          }
          // still seeing flapping roads, trying + 1 
          if (current_road_length <= new_road_length + 1){
            AILogDebug["do_remove_road_stubs"] << "eligible knight hut stub road ending with flag at pos " << flag_pos << ", proposed new direct road length " << new_road_length << " is not shorter than current road length " << current_road_length << ", skipping, will keep trying";
            continue;
          }

          // build the road (and new flag if needed)
          mutex_lock("AI::do_remove_road_stubs calling game->build_flag/build_road() for knight hut stub");
          bool built_flag = false;
          if (needs_flag){
            AILogDebug["do_remove_road_stubs"] << "trying to build flag for replacement road for knight hut stub road ending with flag at pos " << flag_pos;
            built_flag = game->build_flag(pos, player);
            if (built_flag){
              AILogDebug["do_remove_road_stubs"] << "eligible knight hut stub road ending with flag at pos " << flag_pos << ", successfully built flag for replacement road, at pos " << pos;
            }else{
              AILogDebug["do_remove_road_stubs"] << "eligible knight hut stub road ending with flag at pos " << flag_pos << ", failed to build flag for replacement road, at pos " << pos << ", will keep trying";
              mutex_unlock();
              continue;
            }
          }
          AILogDebug["do_remove_road_stubs"] << "trying to build replacement road for knight hut stub road ending with flag at pos " << flag_pos;
          was_built = game->build_road(proposed_direct_road, player);
          roads_removed++;
          if (was_built){
            AILogDebug["do_remove_road_stubs"] << "eligible knight hut stub road ending with flag at pos " << flag_pos << ", successfully built replacement road, to flag/pos " << pos;
            mutex_unlock();
            break;
          }else{
            AILogDebug["do_remove_road_stubs"] << "eligible knight hut stub road ending with flag at pos " << flag_pos << ", failed to build replacement road to flag/pos " << pos << ", will keep trying";
            // demolish any newly built flag if road failed
            if (built_flag){
              AILogDebug["do_remove_road_stubs"] << "demolishing newly built flag for replacement road for knight hut stub road ending with flag at pos " << flag_pos;
              game->demolish_flag(pos, player);
            }
          }
          mutex_unlock();
        }
      }

      road_options.reset(RoadOption::Direct);

      if (was_built){
        AILogDebug["do_remove_road_stubs"] << "eligible knight hut stub road ending with flag at pos " << flag_pos << " had replacement road built, destroying old road in dir " << NameDirection[road_dir] << " / " << road_dir;
        mutex_lock("AI::do_remove_road_stubs calling game->demolish_road() for knight hut road");
        game->demolish_road(map->move(flag_pos, road_dir), player);
        mutex_unlock();
        roads_removed++;
        // sleep a bit to be more human like
        sleep_speed_adjusted(3000);
      }
    }
  } // if only do knight stub removal every x loops

  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_remove_road_stubs"] << "call took " << duration;
  AILogDebug["do_remove_road_stubs"] << "done do_remove_road_stubs, removed " << roads_removed << " roads";

}


// demolish any stonecutters with no stones nearby
void
AI::do_demolish_unproductive_stonecutters() {
  AILogDebug["do_demolish_unproductive_stonecutters"] << "inside do_demolish_unproductive_stonecutters";
  ai_status.assign("HOUSEKEEPING - demolish stonecutters");
  Game::ListBuildings buildings = game->get_player_buildings(player);
  for (Building *building : buildings) {
    if (building == nullptr)
      continue;
    if (building->get_type() != Building::TypeStonecutter)
      continue;
    MapPos pos = building->get_position();
    int stones_check = AI::count_stones_near_pos(pos, AI::spiral_dist(4));
    if (stones_check < 1) {
      AILogDebug["do_demolish_unproductive_stonecutters"] << "stonecutter at pos " << pos << " has no more stones nearby!  burning it";
      mutex_lock("AI::do_demolish_unproductive_stonecutters calling game->demolish_building()");
      game->demolish_building(pos, player);
      mutex_unlock();
      // mark as bad pos, because there should be no reason it could ever become valid again (stone piles cannot regrow)
      bad_building_pos.insert(std::make_pair(pos, Building::TypeStonecutter));
      // sleep to appear more human
      sleep_speed_adjusted(3000);
    }
  }
  AILogDebug["do_demolish_unproductive_stonecutters"] << "done do_demolish_unproductive_stonecutters";
}


// demolish any completed mines that have food stored but are not productive
void
AI::do_demolish_unproductive_mines() {
  AILogDebug["do_demolish_unproductive_mines"] << "inside do_demolish_unproductive_mines";
  ai_status.assign("do_demolish_unproductive_mines");
  Game::ListBuildings buildings = game->get_player_buildings(player);
  for (Building *building : buildings) {
    if (building == nullptr)
      continue;
    if (!building->is_done())
      continue;
    if (building->is_burning())
      continue;
    Building::Type building_type = building->get_type();
    if (building_type != Building::TypeStoneMine && building_type != Building::TypeCoalMine
      && building_type != Building::TypeIronMine && building_type != Building::TypeGoldMine)
      continue;
    MapPos building_pos = building->get_position();
    // if this building is active, store the current tick so that it can be used for future
    //  checks.  Otherwise, it is possible for the AI to not notice when it becomes inactive 
    //  and then once it gets back to zero efficiency AI cannot tell if this is a NEW mine
    //  or an old depleted one
    if (building->has_serf() && building->is_active()){
      if (!active_mines.count(building_pos)){
        //AILogDebug["do_demolish_unproductive_mines"] << "mine of type " << NameBuilding[building_type] << " at pos " << building_pos << " is noticed active for the first time by AI, storing tick " << game->get_tick() << " for future depletion checks";
        active_mines.insert(std::make_pair(building_pos, game->get_tick()));
        continue;
      }
      //AILogDebug["do_demolish_unproductive_mines"] << "mine of type " << NameBuilding[building_type] << " at pos " << building_pos << " is noticed active now, but has been active before";
    }
    if (!active_mines.count(building_pos)){
      //AILogDebug["do_demolish_unproductive_mines"] << "mine of type " << NameBuilding[building_type] << " at pos " << building_pos << " has never been noticed active, not checking productivity";
      continue;
    }
    // see how long since it was first noticed as active
    std::map<MapPos, unsigned int>::iterator it = active_mines.find(building_pos);
    unsigned int first_found_tick = it->second;
    unsigned int current_tick = game->get_tick();
    unsigned int delta = current_tick - first_found_tick;
    //AILogDebug["do_demolish_unproductive_mines"] << "mine of type " << NameBuilding[building_type] << " at pos " << building_pos << " was first noticed active at tick " << first_found_tick << ", current tick is " << current_tick << ", delta is " << delta;
    if (delta < 100000){
      //AILogDebug["do_demolish_unproductive_mines"] << "mine of type " << NameBuilding[building_type] << " at pos " << building_pos << " not enough ticks have passed since this was first caught active, not checking productivity";
      continue;
    }
    //AILogDebug["do_demolish_unproductive_mines"] << "mine of type " << NameBuilding[building_type] << " at pos " << building_pos << " has been active long enough, will check productivity";

    /* this is no longer needed since directly checking 'progress'
        this is /slightly/ cheating by AI but meaningfully not different than 
        checking the efficiency percentage and using that
    // I copied this from popup.cc, it checks each bit of the 'progress' integer
    // which represents a bit-array of the past 16 miner results (1=found, 0=notfound)
    // and uses it to build a percentage success
    const int output_weight[] = { 10, 10, 9, 9, 8, 8, 7, 7,  6, 6, 5, 5, 4, 3, 2, 1 };
    int output = 0;
    for (int i = 0; i < 15; i++) {
      output += !!BIT_TEST(building->get_progress(), i) * output_weight[i];
    }
    */
    //AILogDebug["do_demolish_unproductive_mines"] << "mine of type " << NameBuilding[building_type] << " at pos " << building_pos << " has 'progress' " << building->get_progress();
    // explanation of mining progress indicator:
    //  I believe that the 'progress' integer is used as an array of bits
    //  and the bits represent whether or not a resource was found during
    //  the last miner activity.  When the mine becomes active, the bit-array
    //  is shifted left, dropping the oldest result.  If a resource is found, 
    //  the least significant bit is set to 1 by the if (res){progress++} code
    //  if a resource is not found, the bit is left at 0.  
    //  These 1s or 0s represent the last XX results
    //  and are also used to calculate the mine percentage efficiency
    //  progress of 0x8000/32768 is represented in binary as 1000000000000000
    //  which means a long string of failed efforts, meaning the resource
    //  sought is likely depleted
    //  new mines start at progress 0, which is 0000000000000000
    //  they cannot be marked as depleted until they reach 1000000000000000
    //  which means they have been active at least 16 times
    //
    //  to detect depletion EARLIER, could test to see if the leftmost digits
    //  contain any 1s (to show the mine is not brand new) and also if the 
    //  rightmost digits contain a string of zeros
    //
    // 0000000100000000 is 256, if number is higher than 256 it has at least one successful find
    //  over 8 runs ago, meaning the mine is not brand new
    // 0000000000111111 is 63, use this w/ bit-AMD to wipe the leftmost bits of the progress indicator
    //  which will leave only the most recent 6 miner runs.  If any positive value remains then at
    //  least one of the past 6 runs was successful, if not - demolish the mine!
    // 
    int recently_found_res = building->get_progress() & 63; // wipe the leftmost bits, keep only the rightmost 6 bits
    if (recently_found_res){  // if any bits still positive in last 6 results
      AILogDebug["do_demolish_unproductive_mines"] << "mine of type " << NameBuilding[building_type] << " at pos " << building_pos << " has 'progress' " << building->get_progress() << " has recent success, not demolishing";
    }else{
      AILogDebug["do_demolish_unproductive_mines"] << "mine of type " << NameBuilding[building_type] << " at pos " << building_pos << " has 'progress' " << building->get_progress() << " which shows no recent success, will demolish";
      AILogDebug["do_demolish_unproductive_mines"] << "burning unproductive mine of type " << NameBuilding[building_type] << " at pos " << building_pos;
      mutex_lock("AI::do_demolish_unproductive_mines calling game->demolish_building (for demolish unproductive mines)");
      game->demolish_building(building_pos, player);
      mutex_unlock();
      // mark as bad pos, because rebuilding same mine type seems pointless if it is actually out of resources
      bad_building_pos.insert(std::make_pair(building_pos, building_type));
      // sleep to appear more human
      sleep_speed_adjusted(3000);
    }
  }
  AILogDebug["do_demolish_unproductive_mines"] << "done do_demolish_unproductive_mines";
}


// burn all but one lumberjack per stock if planks_max reached, to avoid clogging roads
void
AI::do_demolish_excess_lumberjacks() {
  AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " inside do_demolish_excess_lumberjacks";
  ai_status.assign("do_demolish_excess_lumberjacks");
  if(get_stock_inv() == nullptr)
    return;

  int lumberjack_count = stock_building_counts.at(inventory_pos).count[Building::TypeLumberjack];

  //unsigned int wood_count = get_stock_inv()->get_count_of(Resource::TypePlank) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypePlank];
  //wood_count += get_stock_inv()->get_count_of(Resource::TypeLumber) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeLumber];
  //if (wood_count >= (planks_max + anti_flapping_buffer) && lumberjack_count > 1) {
  
  AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " inside do_demolish_excess_lumberjacks, needs_wood bool is " << stock_building_counts.at(inventory_pos).needs_wood;
  AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " inside do_demolish_excess_lumberjacks, lumberjack count is " << lumberjack_count;

  if (stock_building_counts.at(inventory_pos).needs_wood == false && lumberjack_count > 1) {
    
    AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " planks_max reached and lumberjack_count is " << lumberjack_count << ".  Burning all but one lumberjack (nearest to this stock)";
    bool first_one_found = false;
    Game::ListBuildings buildings = game->get_player_buildings(player);
    for (Building *building : buildings) {
      if (building == nullptr)
      continue;
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
          mutex_lock("AI::do_demolish_excess_lumberjacks calling game->demolish_building()");
          game->demolish_building(pos, player);
          mutex_unlock();
          // do NOT mark as bad pos
          //bad_building_pos.AI::do_demolish_excess_lumberjacks() {
          // sleep to appear more human
          sleep_speed_adjusted(3000);
        }
      }
    }
  }
  else {
        AILogDebug["do_demolish_excess_lumberjacks"] << inventory_pos << " planks_max not yet reached or no excess lumberjacks, skipping";
  }
}

// burn forester/rangers if planks > max, to avoid creating blocking forests
void
AI::do_demolish_excess_foresters() {
  AILogDebug["do_demolish_excess_foresters"] << inventory_pos << " inside do_demolish_excess_foresters";
  ai_status.assign("do_demolish_excess_foresters");
  if(get_stock_inv() == nullptr)
    return;
  
  //unsigned int wood_count = get_stock_inv()->get_count_of(Resource::TypePlank) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypePlank];
  //wood_count += get_stock_inv()->get_count_of(Resource::TypeLumber) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeLumber];
  //if (wood_count >= (planks_max + anti_flapping_buffer)) {

  if (stock_building_counts.at(inventory_pos).needs_wood != true){
    AILogDebug["do_demolish_excess_foresters"] << inventory_pos << " planks_max reached.  Burning any forester that has few open positions nearby";
    Game::ListBuildings buildings = game->get_player_buildings(player);
    for (Building *building : buildings) {
      if (building == nullptr || building->get_type() != Building::TypeForester)
        continue;
      MapPos forester_pos = building->get_position();
      // CANNOT USE this search because Foresters are usually disconnected from Inventory!
      //if (find_nearest_inventory(map, player_index, building->get_position(), DistType::FlagOnly, &ai_mark_pos) != inventory_pos)
      // trying straightline instead
      if (find_nearest_inventory(map, player_index, building->get_position(), DistType::StraightLineOnly, &ai_mark_pos) != inventory_pos)
        continue;
      // check number of open positions around the forester
      int open_positions = 0;
      int possible_positions = 0;
      for (unsigned int x = 0; x < AI::spiral_dist(4); x++) {
        MapPos pos = map->pos_add_extended_spirally(forester_pos, x);
        // trees (and non-mine buildings) can only be planted on grass)
        if (!map->types_within(pos, Map::TerrainGrass0, Map::TerrainGrass3)){
          continue;
        }
        possible_positions++;
        if (map->get_obj(pos) == Map::ObjectNone
         || map->map_space_from_obj[pos] == Map::SpaceOpen){
           open_positions++;
        }
      }
      // percent of grass tiles that are still open
      if (possible_positions > 0
        && (open_positions / possible_positions * 100) > min_pct_open_positions_burn_excess_forester){
        AILogDebug["do_demolish_excess_foresters"] << inventory_pos << " forester hut at pos " << forester_pos << " has " << open_positions << " open_positions within its work radius out of " << possible_positions << " possible_positions, this is greater than min of " << min_pct_open_positions_burn_excess_forester << "%, not burning it yet";
        // do not burn this one yet
        continue;
      }
      AILogDebug["do_demolish_excess_foresters"] << inventory_pos << " forester hut at pos " << forester_pos << " has " << open_positions << " open_positions within its work radius out of " << possible_positions << " possible_positions, this is below min of " << min_pct_open_positions_burn_excess_forester << "%, burning it to avoid crowding";
      AILogDebug["do_demolish_excess_foresters"] << inventory_pos << " burning forester at pos " << forester_pos;
      mutex_lock("AI::do_demolish_excess_foresters calling game->demolish_building()");
      game->demolish_building(forester_pos, player);
      mutex_unlock();
      sleep_speed_adjusted(3000); // sleep to appear more human
    }
  }
  else {
        AILogDebug["do_demolish_excess_foresters"] << inventory_pos << " planks_max not yet reached, skipping";
  }
}


// burn ALL fisherman huts, wheat farms, pig farms(?) attached to this stock if stock food_max reached, to avoid clogging roads
//  leave grain mills, bakers, butchers
// NOTE that food sitting at flags near the inventory counts also, but its contribution is capped
void
AI::do_demolish_excess_food_buildings() {
  AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " inside do_demolish_excess_food_buildings for stock at pos " << inventory_pos;
  ai_status.assign("do_demolish_excess_food_buildings");
  unsigned int stored_food_count = 0;
  // most important are read-to-use food items stored in Inventory
  if(get_stock_inv() == nullptr)
    return;
  stored_food_count += get_stock_inv()->get_count_of(Resource::TypeBread) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeBread];
  stored_food_count += get_stock_inv()->get_count_of(Resource::TypeMeat) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeMeat];
  stored_food_count += get_stock_inv()->get_count_of(Resource::TypeFish) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeFish];
  // include pigs, wheat, and flour because they ultimately will become food, but cap the amount they can contribute to the total
  //  to avoid situation where the processing buildings are missing
  unsigned int potential_food_count = 0;
  potential_food_count += get_stock_inv()->get_count_of(Resource::TypePig) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypePig];
  potential_food_count += get_stock_inv()->get_count_of(Resource::TypeWheat) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeWheat];
  potential_food_count += get_stock_inv()->get_count_of(Resource::TypeFlour) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeFlour];
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
  AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " debug: stored_food_count " << stored_food_count << ", potential_food_count " << potential_food_count << ", food_at_flags " << food_at_flags << ", adjusted_food_count " << adjusted_food_count << ", food_max " << food_max;
  AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " adjusted food_count at inventory_pos " << inventory_pos << ": " << adjusted_food_count;
  if (adjusted_food_count > (food_max + anti_flapping_buffer)) {
    AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " food_max reached at inventory_pos " << inventory_pos << ", burning all food buildings attached to this stock";
    Game::ListBuildings buildings = game->get_player_buildings(player);
    for (Building *building : buildings) {
      if (building == nullptr)
        continue;
      if (building->get_type() == Building::TypeFisher ||
		  building->get_type() == Building::TypeFarm ||
		  building->get_type() == Building::TypePigFarm) {
		    if (find_nearest_inventory(map, player_index, building->get_position(), DistType::FlagOnly, &ai_mark_pos) != inventory_pos)
          continue;
        AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " burning food building of type " << NameBuilding[building->get_type()] << " at pos " << building->get_position();
        mutex_lock("AI::do_demolish_excess_food_buildings calling game->get_player_buildings(player)");
        game->demolish_building(building->get_position(), player);
        mutex_unlock();
        // sleep to appear more human
        sleep_speed_adjusted(3000);
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
  AILogDebug["do_manage_tool_priorities"] << "inside manage_tool_priorities";
  AILogDebug["do_manage_tool_priorities"] << "HouseKeeping: ensure sufficient tools";
  ai_status.assign("do_manage_tool_priorities");

  // allow default priorities to determine which tool to make if more than one type is needed
  // UPDATE - no longer using default priorities see later in this function
  AILogDebug["do_manage_tool_priorities"] << "resetting plank/steel priorities to default";
  player->reset_tool_priority();
  player->set_steel_toolmaker(0);
  player->set_steel_weaponsmith(65500);
  player->set_planks_toolmaker(0);
  player->set_coal_steelsmelter(32750);
  player->set_coal_goldsmelter(65500);
  player->set_coal_weaponsmith(52400);

  Game::ListBuildings buildings = game->get_player_buildings(player);

  // WORKAROUND - limit hammer creation to help to avoid accidentally creating excess geologists:
  //  stop creating hammers once minimum number of geologists and builders and blacksmiths reached
  //  IF there is not currently a completed blacksmith building waiting for a new blacksmith serf
  bool forbid_hammers = false;
  unsigned int adjusted_geologists_max = 1 + (geologists_max * stock_building_counts.size());
  if (serfs_total[Serf::TypeBuilder] >= builders_max && serfs_total[Serf::TypeGeologist] >= adjusted_geologists_max && serfs_total[Serf::TypeWeaponSmith] >= blacksmiths_max){
    forbid_hammers = true;
    AILogDebug["do_manage_tool_priorities"] << "have sufficient builders, geologists, and blacksmiths, checking to see if any built, unoccupied WeaponSmith buildings";
    for (Building *building : buildings) {
      if (building == nullptr)
        continue;
      if (building->get_type() != Building::TypeWeaponSmith)
        continue;
      if (!building->is_done())
        continue;
      if (building->has_serf())
        continue;
      AILogDebug["do_manage_tool_priorities"] << "a completed blacksmith building requiring a serf was found at pos " << building->get_position() << ", allowing new hammers to be created";
      forbid_hammers = false;
    }
  }
  if (forbid_hammers){
    AILogDebug["do_manage_tool_priorities"] << "have sufficient builders, geologists, and blacksmiths, and no new blacksmith buildings needing Serf.  Not allowing new hammers to be created";
  }



  need_tools = false;
  unsigned int planks_count = realm_inv[Resource::TypePlank];
  for (int i = 0; i < 20; ++i) {

    unsigned int idle = serfs_idle[(Serf::Type)i];
    unsigned int potential = serfs_potential[(Serf::Type)i];
    unsigned int available = idle + potential;
    //unsigned int total = serfs_total[(Serf::Type)i];
    unsigned int total = serfs_total[i];
    //unsigned int min = specialists_reserve[Serf::Type(i)];
    //unsigned int max = specialists_max[Serf::Type(i)];
    //AILogDebug["do_manage_tool_priorities"] << "serf job " << NameSerf[i] << " idle: " << idle << ", potential: " << potential << ", available: " << available
    //      << ", total: " << total << ", min: " << min << ", max: " << max;
    //if (total >= max && max > 0) {
    //      AILogDebug["do_manage_tool_priorities"] << "maximum serfs with job type " << NameSerf[i] << " reached";
    //}
    // this caps the total number of tools & serfs with each job type to a fixed number
    //  that was good with single economy centered aroud castle, but fails with multiple economies w/ warehouses
    //   disabling this, going back to simple "ensure > 0"
    //if (total < max && available < min) {
    //  AILogDebug["do_manage_tool_priorities"] << "need more available serfs of job type " << NameSerf[i];
    //  need_tools = true;
    //}
    
    if (i == Serf::TypeBuilder || i == Serf::TypeGeologist || i == Serf::TypeWeaponSmith){
      if (forbid_hammers){
        AILogDebug["do_manage_tool_priorities"] << "forbid_hammers is true, not considering needs_tools for hammer-requiring profession " << NameSerf[i];
        continue;
      }
    }

    //if (available < 1) {
    if (available < 2) {
      AILogDebug["do_manage_tool_priorities"] << "need more available serfs of job type " << NameSerf[i];
      // boats (Serf::TypeSailor) are *not* made by toolmaker
      if (i == Serf::TypeSailor)
        continue;
      need_tools = true;
    }
  }
  
  // this is one toolmaker in entire REALM, but that is okay
  if (realm_building_count[Building::TypeToolMaker] < 1) {
    AILogDebug["do_manage_tool_priorities"] << "no toolmaker exists yet!";
  }
  else if (need_tools) {
    for (int i = 0; i < 9; ++i) {
      // tools Resource::Types are and 15-24
      int tool_index = 15 + i;
      int tool_count = realm_inv[Resource::Type(tool_index)];
      AILogDebug["do_manage_tool_priorities"] << "realm has " << tool_count << " of tool " << NameTool[i];
      if (tool_count >= 2) {
        // don't need this tool, set to zero prior
        player->set_tool_prio(i, 0);
        continue;
      }

      // hammer is tool #1 (second item in list - see NameTool lookup table)
      if (i == 1 && forbid_hammers){
        AILogDebug["do_manage_tool_priorities"] << "forbid_hammers is true, not setting hammer priority";
        player->set_tool_prio(i, 0);
        continue;
      }

      if (tool_count == 1) {
        // have one, want one more, set medium priority
        player->set_tool_prio(i, 32750);
      }else{
        // have none, set max priority
        player->set_tool_prio(i, 65500);
      }
      AILogDebug["do_manage_tool_priorities"] << "need to make tool: " << NameTool[i];
    }

    // shortcut to ensure farmer scythe tool is created first on very low resource starts
    if (serfs_potential[Serf::TypeFarmer] + player->get_serf_count(Serf::TypeFarmer) < 1) {
      AILogDebug["do_manage_tool_priorities"] << "No farmer and no scythe!  Setting scythe to priority and others to zero";
      for (int i = 0; i < 9; ++i) {
        player->set_tool_prio(i, 0);
      }
      // scythe is tool #4 (fifth item in list - see NameTool lookup table)
      player->set_tool_prio(4, 65500);
    }


    // manage ToolMaker
    for (Building *building : buildings) {
      if (building == nullptr)
        continue;
      if (building->get_type() != Building::TypeToolMaker)
        continue;

      MapPos pos = building->get_position();
      unsigned int planks = building->get_res_count_in_stock(0);
      if (planks <= 1 && planks_count >= planks_min) {
        AILogDebug["do_manage_tool_priorities"] << "toolmaker at pos " << pos << " has only " << planks << " planks, setting priority to max and reducing construction priority";
        player->set_planks_toolmaker(65500);
        player->set_planks_construction(32750);
      }
      unsigned int steel = building->get_res_count_in_stock(1);
      if (steel <= 1) {
        AILogDebug["do_manage_tool_priorities"] << "toolmaker at pos " << pos << " has only " << steel << " steel, setting priority to max and zeroing blacksmith priority";
        player->set_steel_toolmaker(65500);
        player->set_steel_weaponsmith(0);
        AILogDebug["do_manage_tool_priorities"] << "to avoid running out of coal for steel, also zeroing coal priority to gold and weaponsmiths, maxing to steel smelter";
        player->set_coal_goldsmelter(0);
        player->set_coal_weaponsmith(0);
        player->set_coal_steelsmelter(65550);
      }
      
    }
  }
  else {
    AILogDebug["do_manage_tool_priorities"] << "have toolmaker but don't need any tools now";
  }
}


void
AI::do_manage_mine_food_priorities() {
  AILogDebug["do_manage_mine_food_priorities"] << "inside do_manage_mine_food_priorities";
  ai_status.assign("do_manage_mine_food_priorities");
  // if sufficient gold/ore is stored, divert food to other resource miners
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
    AILogDebug["do_manage_mine_food_priorities"] << "coal_count " << coal_count << " is greater than coal_max " << coal_min << ", setting coal mine food priority to zero";
    player->set_food_coalmine(0);
  }else if (coal_count > coal_min) {
    AILogDebug["do_manage_mine_food_priorities"] << "coal_count " << coal_count << " is greater than coal_min " << coal_min << ", greatly reducing food priority to coal mines";
    player->set_food_coalmine(6550);
  }
  // iron
  if (iron_ore_count > iron_ore_max) {
    AILogDebug["do_manage_mine_food_priorities"] << "iron_ore_count " << iron_ore_count << " is greater than iron_ore_max " << iron_ore_max << ", setting iron mine food priority to zero";
    player->set_food_ironmine(0);
  }else if (iron_ore_count > iron_ore_min) {
    AILogDebug["do_manage_mine_food_priorities"] << "iron_ore_count " << iron_ore_count << " is greater than iron_ore_min " << iron_ore_min << ", greatly reducing food priority to iron mines";
    player->set_food_ironmine(6550);
  }
  // gold
  if (gold_ore_count > gold_ore_max) {
    AILogDebug["do_manage_mine_food_priorities"] << "gold_ore_count " << gold_ore_count << " is greater than gold_ore_max " << gold_ore_max << ", setting gold mine food priority to zero";
    player->set_food_goldmine(0);
  } else if (gold_ore_count > gold_ore_min) {
    AILogDebug["do_manage_mine_food_priorities"] << "gold_ore_count " << gold_ore_count << " is greater than gold_ore_min " << gold_ore_min << ", greatly reducing food priority to gold mines";
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
  AILogDebug["do_balance_sword_shield_priorities"] << "inside do_balance_sword_shield_priorities";
  ai_status.assign("do_balance_sword_shield_priorities");
  player->reset_flag_priority();
  int *prio = nullptr;
  // to adjust priorites, first get the pointer to the flag_prio array...
  prio = player->get_flag_prio();
  // ...then directly set the priority of the item by id
  //flag_prio[Resource::TypeShield] = 16;
  //flag_prio[Resource::TypeSword] = 17;
  unsigned int swords_count = realm_inv[Resource::TypeSword];
  unsigned int shields_count = realm_inv[Resource::TypeShield];
  AILogDebug["do_balance_sword_shield_priorities"] << "there are " << swords_count << " swords and " << shields_count << " shields stored";
  // actually, rather than worry about the overall ordering, just swap the two values
  if (swords_count >= shields_count + 2) {
    AILogDebug["do_balance_sword_shield_priorities"] << "more swords than shields stored";
    if (prio[Resource::TypeSword] > prio[Resource::TypeShield]) {
      int temp = prio[Resource::TypeSword];
      prio[Resource::TypeShield] = prio[Resource::TypeSword];
      prio[Resource::TypeSword] = temp;
      AILogDebug["do_balance_sword_shield_priorities"] << "sword priority swapped with shield priority, sword priority is now " << prio[Resource::TypeSword] << ", shield priority is now " << prio[Resource::TypeShield];
    }
    else {
      AILogDebug["do_balance_sword_shield_priorities"] << "sword priority " << prio[Resource::TypeSword] << " is already higher than shield priority " << prio[Resource::TypeShield] << ", no change needed";
    }
  }
  else if (shields_count >= swords_count + 2) {
    AILogDebug["do_balance_sword_shield_priorities"] << "more shields than swords stored";
    if (prio[Resource::TypeShield] > prio[Resource::TypeSword]) {
      int temp = prio[Resource::TypeShield];
      prio[Resource::TypeSword] = prio[Resource::TypeShield];
      prio[Resource::TypeShield] = temp;
      AILogDebug["do_balance_sword_shield_priorities"] << "shield priority swapped with sword priority, sword priority is now " << prio[Resource::TypeSword] << ", shield priority is now " << prio[Resource::TypeShield];
    }
    else {
      AILogDebug["do_balance_sword_shield_priorities"] << "shield priority " << prio[Resource::TypeShield] << " is already higher than sword priority " << prio[Resource::TypeSword] << ", no change needed";
    }
  }
  else {
    AILogDebug["do_balance_sword_shield_priorities"] << "swords and shields are sufficiently balanced, leaving sword priority at " << prio[Resource::TypeSword] << " and shield priority at " << prio[Resource::TypeShield];
  }
}


void
AI::do_manage_knight_occupation_levels() {
  AILogDebug["do_manage_knight_occupation_levels"] << "inside do_manage_knight_occupation_levels";
  if (player->cycling_knight()) {
    AILogDebug["do_manage_knight_occupation_levels"] << "is currently cycling_knights!  waiting until this is complete";
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
  AILogDebug["do_manage_knight_occupation_levels"] << "current knight occupation level: " << current_level;
  // to avoid flapping...
  //  ...if this is not the very first occupation level change...
  if (previous_knight_occupation_level > 0) {
    // ...and we last LOWERED the level
    if (previous_knight_occupation_level > current_level) {
      change_buffer = static_cast<int>(knight_occupation_change_buffer) * -1;
      AILogDebug["do_manage_knight_occupation_levels"] << "knight occupation level last DECREASED from " << previous_knight_occupation_level << " to "
        << current_level << ", setting a change buffer of " << change_buffer;
    }
    // ...and we last RAISED the occupation level
    if (previous_knight_occupation_level < current_level) {
      change_buffer = static_cast<int>(knight_occupation_change_buffer);
      AILogDebug["do_manage_knight_occupation_levels"] << "knight occupation level last INCREASED from " << previous_knight_occupation_level << " to "
        << current_level << ", setting a change buffer of " << change_buffer;
    }
  }
  previous_knight_occupation_level = current_level;
  AILogDebug["do_manage_knight_occupation_levels"] << "knight occupation change_buffer is currently set to " << change_buffer;
  //
  // NEED TO CHANGE THIS WHOLE FUNCTION
  //   make it determine current desired level without changing it
  //    compare to current level and only change it as required, do NOT reset it to zero first
  //     I suspect this is causing knights to shuffle in out... because it is being set to min for long enough the game update tells the knights to exit?
  //      this should only happen if count_knights_affected_by_occupation_level_change is taking a long time (more than the game update cycle)
  //       ... which it might if update_buildings uses mutex so it could be locked in the middle...
  //
  // get current occupation setting for thread level Frontier (thick cross) - closest to enemy
  //player->get_knight_occupation(3);
  // this is counting all knights in *any* building, but we only care about idle in castle/stock
  //unsigned int idle_knights = serfs_idle[Serf::TypeKnight0] + serfs_idle[Serf::TypeKnight1] + serfs_idle[Serf::TypeKnight2] + serfs_idle[Serf::TypeKnight3] + serfs_idle[Serf::TypeKnight4];
  // instead use this function I wrote elsewhere
  unsigned int idle_knights = 0;
  mutex_lock("AI::do_manage_knight_occupation_levels calling game->get_player_serfs(player) (for do_manage_knight_occupation_levels is_waiting)");
  for (Serf *serf : game->get_player_serfs(player)) {
    if (serf->get_state() == Serf::StateIdleInStock && serf->get_type() >= Serf::TypeKnight0 && serf->get_type() <= Serf::TypeKnight4) {
      idle_knights++;
    }
  }
  mutex_unlock();
  AILogDebug["do_manage_knight_occupation_levels"] << "found in stocks idle_knights: " << idle_knights;
  player->change_knight_occupation(3, 0, -5);   // reset lower bound to 'min'
  player->change_knight_occupation(3, 1, -5);   // reset upper bound to 'min'
  // must evaluate idle_knights as signed integer to avoid calculating negative values returning opposite result
  if      ((signed)idle_knights - (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 4) + (signed)change_buffer >= (signed)knights_max) {
    //AILogDebug["do_manage_knight_occupation_levels"] << "debug4: " << (signed)idle_knights << " - " << (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 4) << " + " << change_buffer
    //  << " = " << (signed)idle_knights - (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 4) + change_buffer << " ?? " << knights_max;
    AILogDebug["do_manage_knight_occupation_levels"] << "setting knight level to med/full";
    player->change_knight_occupation(3, 1, +4);   // increase upper bound to 'full'
    player->change_knight_occupation(3, 0, +2);   // increase lower bound to 'med'
  }
  else if ((signed)idle_knights - (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 3) + (signed)change_buffer >= (signed)knights_med) {
    //AILogDebug["do_manage_knight_occupation_levels"] << "debug3: " << (signed)idle_knights << " - " << (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 3) << " + " << change_buffer
    //  << " = " << (signed)idle_knights - (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 3) + change_buffer << " ?? " << knights_med;
    AILogDebug["do_manage_knight_occupation_levels"] << "setting knight level to weak/good";
    player->change_knight_occupation(3, 1, +3);   // increase upper bound to 'good'
    player->change_knight_occupation(3, 0, +1);   // increase lower bound to 'weak'
  }
  else if ((signed)idle_knights - (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 2) + (signed)change_buffer >= (signed)knights_min) {
    //AILogDebug["do_manage_knight_occupation_levels"] << "debug2: " << (signed)idle_knights << " - " << (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 2) << " + " << change_buffer
    //  << " = " << (signed)idle_knights - (signed)AI::count_knights_affected_by_occupation_level_change(current_level, 2) + change_buffer << " ?? " << knights_min;
    AILogDebug["do_manage_knight_occupation_levels"] << "setting knight level to min/med";
    player->change_knight_occupation(3, 1, +2);   // increase upper bound to 'med'
    // leave lower bound at 'min'
  }
  else {
    AILogDebug["do_manage_knight_occupation_levels"] << "setting knight level to min/min";
    // leave both at 'min'
  }
}


void
AI::do_place_mines(std::string type, Building::Type building_type, Map::Object large_sign, Map::Object small_sign, double sign_density_min) {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  ai_status.assign("do_place_mines " + type);
  AILogDebug["do_place_mines"] << inventory_pos << " inside do_place_mines() with type " << type << ", building_type " << NameBuilding[building_type] <<
    ", large_sign " << NameObject[large_sign] << ", small_sign " << NameObject[small_sign] << ", sign_density_min " << sign_density_min;
  MapPosSet count_by_corner;
  // for mined resouce finding,  reverse the occupied_military_buildings order
  //   so the newest military building is searched first,  instead of castle first
  MapPosVector foo = stock_building_counts.at(inventory_pos).occupied_military_pos;
  // for some reason after switching to parallel stocks/warehouse support, cannot use stock_building_counts.at(inventory_pos).occupied_militar_pos iterator directly
  //  must copy it to another MapPosVector and iterate over that.  I'm sure there is a cleaner way
  //for (MapPosVector::reverse_iterator it = stock_building_counts.at(inventory_pos).occupied_military_pos.rbegin(); it != stock_building_counts.at(inventory_pos).occupied_military_pos.rend(); ++it) {
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
        AILogDebug["do_place_mines"] << inventory_pos << " found at least one other mine of same type " << NameBuilding[building_type] << " nearby, not placing another.  Skipping this corner area";
        continue;
      }
      // try to build a mine on a Large resource sign if found,
      //    or if a Small resource find and sign_ratio is high enough
      // note - because we are checking even if blank signs found, this is sub-optimal.
      //   could improve by calculating sign_density in a way that lets us ignore blank
      //    signs when determining if trying to place mine
      // YES - I had always misunderstood how mines operate.  It does not matter if the
      //  mine is placed on a Large or Small resouce sign, only that the mine is optimally
      //  placed in the center of a deposit, which typically does have Large sign
      //  Also, this misunderstanding had always led me to place multiple mines near same 
      //   mineral deposit, not building the second until the first became depleted.  Little
      //   did I know that they all consume the same minerals and so if the first is depleted
      //   the second will find nothing!
      //AILogVerbose["do_place_mines"] << inventory_pos << " " << type << " mine: looking for a place to build a " << type << " mine near corner " << corner_pos;
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
          AILogDebug["do_place_mines"] << inventory_pos << " trying to build " << type << " mine at pos " << pos;
          MapPos built_pos = bad_map_pos;
          // note that distance = 1 means ONLY THIS SPOT
          built_pos = AI::build_near_pos(pos, 1, building_type);
          if (built_pos != bad_map_pos && built_pos != notplaced_pos) {
            AILogDebug["do_place_mines"] << inventory_pos << " placed (but not connected) " << type << " mine at pos " << built_pos;
            stock_building_counts.at(inventory_pos).count[building_type]++;
            //stock_building_counts.at(inventory_pos).unfinished_count++;  // do NOT include placed-but-not-connected mines in unfinished_count!
            break;
          }
        }
      } // foreach hills pos spirally
    } // foreach corner
  } // foreach military building
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_place_mines"] << inventory_pos << " do_place_mines call took " << duration;
}


void
AI::do_place_coal_mines() {
  AILogDebug["do_place_coal_mines"] << "inside do_place_coal_mines()";
  do_place_mines("coal", Building::TypeCoalMine, Map::ObjectSignLargeCoal, Map::ObjectSignSmallCoal, coal_sign_density_min);
}

void
AI::do_place_iron_mines() {
  AILogDebug["do_place_iron_mines"] << "inside do_place_iron_mines()";
  do_place_mines("iron", Building::TypeIronMine, Map::ObjectSignLargeIron, Map::ObjectSignSmallIron, iron_sign_density_min);
}

void
AI::do_place_gold_mines(){
  AILogDebug["do_place_gold_mines"] << "inside do_place_gold_mines()";
  do_place_mines("gold", Building::TypeGoldMine, Map::ObjectSignLargeGold, Map::ObjectSignSmallGold, gold_sign_density_min);
}



// build a sawmill and two lumberjacks in area with most trees
void
AI::do_build_sawmill_lumberjacks() {
  // time this function for debugging
  std::clock_t start;
  double duration;
  start = std::clock();
  AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " inside do_build_sawmill_lumberjacks";
  ai_status.assign("do_build_sawmill_lumberjacks");
  if(get_stock_inv() == nullptr)
    return;
  // don't include planks at flags, it is critical to have planks available in the Inventory
  //unsigned int wood_count = get_stock_inv()->get_count_of(Resource::TypePlank) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypePlank];
  //unsigned int wood_count = get_stock_inv()->get_count_of(Resource::TypePlank);
  // include raw logs as they will become planks, but only if:
  // - at least planks_min planks stored in Inventory already
  // - one connected sawmill
  //if (wood_count >= planks_min && stock_building_counts.at(inventory_pos).connected_count[Building::TypeSawmill] > 0){
  //  wood_count += get_stock_inv()->get_count_of(Resource::TypeLumber) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeLumber];
  //}

  // DEBUG 
  //AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " aggregate wood_count " << wood_count;
  //AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " planks in inv " << get_stock_inv()->get_count_of(Resource::TypePlank);
  //AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " planks at flags " << stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypePlank];
  //AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " logs in inv " << get_stock_inv()->get_count_of(Resource::TypeLumber);
  //AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " logs at flags " << stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeLumber];

  int sawmill_count = 0;
  int lumberjack_count = 0;
  //if (wood_count < (planks_max - anti_flapping_buffer)) {
  if (stock_building_counts.at(inventory_pos).needs_wood == true){
    sawmill_count = stock_building_counts.at(inventory_pos).count[Building::TypeSawmill];
    lumberjack_count = stock_building_counts.at(inventory_pos).count[Building::TypeLumberjack];
    if (sawmill_count >= 1 && lumberjack_count >= 2) {
      AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " Already placed sawmill and lumberjacks, not building more";
      return;
    }
    AILogInfo["do_build_sawmill_lumberjacks"] << inventory_pos << " want build sawmill";
    // count trees around corners of each military building pos
    MapPosSet count_by_corner;
    MapPos built_pos = bad_map_pos;
    for (MapPos center_pos : stock_building_counts.at(inventory_pos).occupied_military_pos) {
      sawmill_count = stock_building_counts.at(inventory_pos).count[Building::TypeSawmill];
      lumberjack_count = stock_building_counts.at(inventory_pos).count[Building::TypeLumberjack];
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
          AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " try to build sawmill near pos " << corner_pos;
          // use directional fill pattern if this is the first sawmill near the castle, to keep it near edge
          if (inventory_pos == castle_flag_pos){
            built_pos = AI::build_near_pos(corner_pos, DIRECTIONAL_FILL_POS_MAX, Building::TypeSawmill, get_dir_from_corner(center_pos, corner_pos));
          }else{
            built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(6), Building::TypeSawmill);
          }
          if (built_pos != bad_map_pos && built_pos != notplaced_pos) {
            AILogInfo["do_build_sawmill_lumberjacks"] << inventory_pos << " built sawmill at pos " << built_pos;
            stock_building_counts.at(inventory_pos).count[Building::TypeSawmill]++;
            stock_building_counts.at(inventory_pos).unfinished_count++;
            break;
          }
        }
      }
      sawmill_count = stock_building_counts.at(inventory_pos).count[Building::TypeSawmill];
      if (sawmill_count > 0) {
        //
        // build two lumberjacks near corner where sawmill was built, or corner with most trees
        //
        AILogInfo["do_build_sawmill_lumberjacks"] << inventory_pos << " want to build lumberjack";
        MapPosVector search_positions = AI::sort_by_val_desc(count_by_corner);
        //  push the location of the sawmill (built_pos) to the front of the search path to help keep lumberjacks close
        if (built_pos != bad_map_pos){
          search_positions.insert(search_positions.begin(), built_pos);
        }
        for (MapPos search_pos : search_positions) {
          //AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " debug checking search_pos " << search_pos;
          // try to build two
          for (int x = 0; x < 2; x++) {
            if (stock_building_counts.at(inventory_pos).count[Building::TypeLumberjack] >= 2) {
              break;
            }
            AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " trying to build a lumberjack near pos " << search_pos;
            built_pos = bad_map_pos;
            built_pos = AI::build_near_pos(search_pos, AI::spiral_dist(4), Building::TypeLumberjack);
            if (built_pos != bad_map_pos && built_pos != notplaced_pos) {
              AILogInfo["do_build_sawmill_lumberjacks"] << inventory_pos << " built lumberjack at pos " << built_pos;
              stock_building_counts.at(inventory_pos).count[Building::TypeLumberjack]++;
              stock_building_counts.at(inventory_pos).unfinished_count++;
            }
          }
          if (stock_building_counts.at(inventory_pos).count[Building::TypeLumberjack] >= 2) {
            break;
          }
        }
      } //end if have sawmill
    } // foreach military building
  }
  else {
    AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " have sufficient planks and/or wood buildings, skipping";
  }
  AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " done do_build_sawmill_lumberjacks";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_build_sawmill_lumberjacks"] << inventory_pos << " done do_build_sawmill_lumberjacks call took " << duration;
}


// stop AI loop here if planks below crit and at least one sawmill & lumberjack are not fully built
//    OR at least until all wood/stone delivered for sawmill AND at least one lumberjack fully built
// return false if need to wait, true if ready to continue
bool
AI::do_wait_until_sawmill_lumberjacks_built() {
  AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " inside do_wait_until_sawmill_lumberjacks_built";
  if(get_stock_inv() == nullptr)
    return false;
  unsigned int planks_count = get_stock_inv()->get_count_of(Resource::TypePlank);
  if (planks_count >= planks_min) {
    AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " have enough planks - no need to wait, continuing";
    return true;
  }
  AILogDebug["do_wait_until_sawmill_lumberjacks_built"] << inventory_pos << " planks < planks_min, checking sawmill and lumberjack build state";
  bool have_sawmill = false;
  bool have_lumberjack = false;
  bool sawmill_has_stones = false;
  bool sawmill_has_planks = false;
  Game::ListBuildings buildings = game->get_player_buildings(player);
  for (Building *building : buildings) {
    if (building == nullptr)
      continue;
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
  ai_status.assign("do_build_stonecutter");
  if (stock_building_counts.at(inventory_pos).needs_stone) {
    int stonecutter_count = stock_building_counts.at(inventory_pos).count[Building::TypeStonecutter];
    if (stonecutter_count >= 1) {
      AILogDebug["do_build_stonecutter"] << inventory_pos << " Already placed stonecutter, not building more";
      return;
    }
    AILogInfo["do_build_stonecutter"] << inventory_pos << " want to build stonecutter";
    // count stones near military buildings
    MapPosSet count_by_corner;
    for (MapPos center_pos : stock_building_counts.at(inventory_pos).occupied_military_pos) {
      stonecutter_count = stock_building_counts.at(inventory_pos).count[Building::TypeStonecutter];
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
        AILogDebug["do_build_stonecutter"] << inventory_pos << " try to build stonecutter near corner pos " << corner_pos;
        // to avoid infinite loop bug where stonecutter is built a bit to far from stones and immediately demolished, need to check each potential build pos for suitability
        for (unsigned int i = 0; i < AI::spiral_dist(4); i++) {
          MapPos pos = map->pos_add_extended_spirally(corner_pos, i);
          //unsigned int count = AI::count_stones_near_pos(pos, AI::spiral_dist(4), Map::ObjectStone0, Map::ObjectStone7, "gray");
          unsigned int count = AI::count_stones_near_pos(pos, AI::spiral_dist(4));
          if (count < near_stones_min) {
            continue;
          }
          // try each specific pos one at a time
          AILogDebug["do_build_stonecutter"] << inventory_pos << " trying to build stonecutter near pos " << pos;
          built_pos = AI::build_near_pos(pos, 1, Building::TypeStonecutter);
          if (built_pos != bad_map_pos && built_pos != notplaced_pos) {
            AILogInfo["do_build_stonecutter"] << inventory_pos << " built stonecutter at pos " << built_pos;
            stock_building_counts.at(inventory_pos).count[Building::TypeStonecutter]++;
            stock_building_counts.at(inventory_pos).unfinished_count++;
            break;
          }
          // if couldn't build, add to bad_building_pos list so it doesn't keep trying every loop
          bad_building_pos.insert(std::make_pair(pos, Building::TypeStonecutter));
        }
        if (built_pos != bad_map_pos && built_pos != notplaced_pos) { break; }
      }
    } // foreach military building
  }
  else {
    AILogDebug["do_build_stonecutter"] << inventory_pos << " have sufficient stone and/or stonecutters, skipping";
  }
  AILogDebug["do_build_stonecutter"] << inventory_pos << " done do_build_stonecutter";
}


/* this no longer needs to be a standalone function it is part of expand_borders logic
// expand borders to create defensive buffer
void
AI::do_create_defensive_buffer() {
  AILogDebug["do_create_defensive_buffer"] << inventory_pos << " inside do_create_defensive_buffer";
  expand_towards.insert("create_buffer");
  unsigned int idle_knights = serfs_idle[Serf::TypeKnight0] + serfs_idle[Serf::TypeKnight1] + serfs_idle[Serf::TypeKnight2] + serfs_idle[Serf::TypeKnight3] + serfs_idle[Serf::TypeKnight4];
  if (idle_knights >= knights_min) {
    AI::expand_borders();
  }
  else {
    AILogDebug["do_create_defensive_buffer"] << inventory_pos << " not enough knights to expand borders for defensive buffer, knights = " << idle_knights << ", knights_min = " << knights_min;
  }
  AILogDebug["do_create_defensive_buffer"] << inventory_pos << " done do_create_defensive_buffer";
}
*/


// build a toolmaker, and a steel smelter if enough coal and iron ore
//  only one toolmaker will be built in the entire realm, but it seems okay to
//  allow it to check and consider it for every Inventory in case of some very rare case
//  where one can't be placed near castle, or more likely if it (or entire castle) destroyed
//
// in medieval times, a steel smelter was probably called a Bloomery
void
AI::do_build_toolmaker_steelsmelter() {
  AILogDebug["do_build_toolmaker_steelsmelter"] << inventory_pos << " inside do_build_toolmaker_steelsmelter";
  ai_status.assign("do_build_toolmaker_steelsmelter");
  // one toolmaker in entire REALM
  int toolmaker_count = realm_building_count[Building::TypeToolMaker];
  if (toolmaker_count < 1) {
    if (need_tools) {
      AILogInfo["do_build_toolmaker_steelsmelter"] << inventory_pos << " need tools but have no toolmaker, trying to build one near castle or current inventory";
      MapPos built_pos = AI::build_near_pos(inventory_pos, AI::spiral_dist(24), Building::TypeToolMaker);
      if (built_pos != bad_map_pos && built_pos != notplaced_pos) {
        AILogInfo["do_build_toolmaker_steelsmelter"] << inventory_pos << " built toolmaker at pos " << built_pos;
        stock_building_counts.at(inventory_pos).count[Building::TypeToolMaker]++;
        stock_building_counts.at(inventory_pos).unfinished_count++;
      }
    }
    else {
      AILogDebug["do_build_toolmaker_steelsmelter"] << inventory_pos << " don't need any tools right now, not building toolmaker";
    }
  }
  else {
    AILogDebug["do_build_toolmaker_steelsmelter"] << inventory_pos << " Already placed toolmaker, not building another";
  }

  // if no steel, but have iron & coal, build steelsmelter to produce steel for toolmaker
  //   do this even if nineo toolmaker exists and tools not needed yet, they might be needed soon
  if(get_stock_inv() == nullptr)
    return;
  unsigned int steel_count = get_stock_inv()->get_count_of(Resource::TypeSteel);
  unsigned int iron_ore_count = get_stock_inv()->get_count_of(Resource::TypeIronOre);
  unsigned int coal_count = get_stock_inv()->get_count_of(Resource::TypeCoal);
  if (stock_building_counts.at(inventory_pos).count[Building::TypeSteelSmelter] < 1
       && (steel_count < 1 && iron_ore_count > 0 && coal_count > 0)) {
    AILogInfo["do_build_toolmaker_steelsmelter"] << inventory_pos << " no steel, but have iron & coal, want to build steelsmelter";
    do_build_steelsmelter();
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
AI::do_build_food_buildings() {
  AILogDebug["do_build_food_buildings"] << inventory_pos << " Main Loop - food";
  ai_status.assign("do_build_food_buildings");

  if (!stock_building_counts.at(inventory_pos).needs_foods){
    AILogDebug["do_build_food_buildings"] << inventory_pos << " have sufficient food and/or food buildings, not building more";
    return;
  }

  // to avoid placing a Farm right near the castle at the beginning of a game, don't 
  //  build any food buildings until at least two occupied huts have been placed and staffed
  //  IN ADDITION TO DOING THIS, adding a "don't build XX distance to castle" to build_near_pos
  int occupied_huts = realm_occupied_building_count[Building::TypeHut];
  if (occupied_huts < 2){
    AILogDebug["do_build_food_buildings"] << inventory_pos << " not building any food buildings until at least two occupied knight huts in realm, currently have: " << occupied_huts;
    return;
  }

  //
  // build fisherman if water found with no nearby fisherman
  //
  MapPosSet count_by_corner;
  for (MapPos center_pos : stock_building_counts.at(inventory_pos).occupied_military_pos) {
    MapPos built_pos = bad_map_pos;
    MapPosVector corners = AI::get_corners(center_pos);
    for (MapPos corner_pos : corners) {
      unsigned int count = AI::count_terrain_near_pos(corner_pos, AI::spiral_dist(4), Map::TerrainWater0, Map::TerrainWater3, "dk_blue");
      if (count >= waters_min) {
        if (!AI::building_exists_near_pos(corner_pos, AI::spiral_dist(8), Building::TypeFisher)) {
          AILogInfo["do_build_food_buildings"] << inventory_pos << " water found and no fisherman nearby, want to build fisherman";
          built_pos = bad_map_pos;
          built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(4), Building::TypeFisher);
          if (built_pos != bad_map_pos && built_pos != notplaced_pos) {
            AILogInfo["do_build_food_buildings"] << inventory_pos << " built fisherman at pos " << built_pos;
            // BUG NOTICE:
            // fishermen do not count because they are not "attached" to Stocks for most calculations
            // because they are effectively unlimited, so it should be okay to ignore them for the 
            // purpose of unfinished building checks, at least at this level (update_buildings will fix it)
            // so there is no guarantee that the Inventory they are being associated with here is actually the one
            // they will be associated with when the Flagsearch is done.  However, it probably doesn't matter.  It
            // seems like a good idea to include them in unfinished_count for this inventory_pos now to limit runaway
            // building for this Inv, even if on next update_buildings call they end up being associated with another Inv.
            stock_building_counts.at(inventory_pos).count[Building::TypeFisher]++;
            stock_building_counts.at(inventory_pos).unfinished_count++;
            break;
          }
        }
      }
      if (built_pos != bad_map_pos && built_pos != notplaced_pos) { break; }
    }
  } // fisherman

  
  //
  // build wheat farm if none (or if needing another)
  //

    // problem - if this Inventory "steals" a farm building from another Inventory, the food
    // infrastructure will be poor, we want it straight-line dist closest to this Inventory 
    // not just Flag-dist.  However, messing with stock_building_counts will cause other issues
    // Instead, verify that the farm buildings are actually closest to this inv

  bool need_farm = false;
  int mine_count = 0;
  int farm_count = 0;
  int mill_count = 0;
  int baker_count = 0;
  for (Building *building : stock_attached_buildings.at(inventory_pos)){
    if (building == nullptr)
      continue;
    Building::Type type = building->get_type();
    //AILogDebug["do_build_food_buildings"] << inventory_pos << " this stock has an attached building of type " << NameBuilding[type] << " at pos " << building->get_position();

    if (type == Building::TypeCoalMine || type == Building::TypeIronMine || type == Building::TypeGoldMine)
      mine_count++;
    if (type == Building::TypeFarm || type == Building::TypeMill || type == Building::TypeBaker){
      // make sure food buidlings are closest by straight-line dist (we already know it is closest by flag because it is on attached list)
      //  to keep food buildings reasonably close to the Inv, and minimize impact of "stealing" food buildings from other Invs
      if (find_nearest_inventory(map, player_index, building->get_position(), DistType::StraightLineOnly, &ai_mark_pos) != inventory_pos){
        AILogDebug["do_build_food_buildings"] << inventory_pos << " food building of type " << NameBuilding[type] << " is not straight-line closest to this Inv, pretending it does not exist";
        continue;
      }
      if (type == Building::TypeFarm){
        AILogDebug["do_build_food_buildings"] << inventory_pos << " found farm closest by straight-line and flag to this inventory, at pos " << building->get_position();
        farm_count++;
      }
      if (type == Building::TypeMill){
        AILogDebug["do_build_food_buildings"] << inventory_pos << " found mill closest by straight-line and flag to this inventory, at pos " << building->get_position();
        mill_count++;
      }
      if (type == Building::TypeBaker){
        AILogDebug["do_build_food_buildings"] << inventory_pos << " found baker closest by straight-line and flag to this inventory, at pos " << building->get_position();
        baker_count++;
      }
    }
  }

  AILogDebug["do_build_food_buildings"] << inventory_pos << " debug - has " << farm_count << " farms near this inventory";
  if (farm_count == 0) {
    AILogDebug["do_build_food_buildings"] << inventory_pos << " has zero farms, need to build one";
    need_farm = true;
  }
  else {
    // if economy is far enough along, place a second and third farm near existing farm buildings
    //   (if they indeed exist) by inserting their positions in the front of the queue
    if (mine_count >= 3) {
      if (farm_count == 1 && mill_count >= 1 && baker_count >= 1) {
        AILogDebug["do_build_food_buildings"] << inventory_pos << " three completed mines, a mill, and a baker exist.  Need a second wheat farm";
        need_farm = true;
      }else{
        AILogDebug["do_build_food_buildings"] << inventory_pos << " three completed mines, but do not have both mill and baker, not building another wheat farm yet";
      }
    }
    else {
      AILogDebug["do_build_food_buildings"] << inventory_pos << " not building any more farms until having 3+ completed mines, currently have " << mine_count;
    }
  }


  //
  // create list of existing farm building locations so additional ones can
  //   be built near them
  //
  MapPosVector farm_buildings ={};  // pos with existing wheat-farm, mill, or baker
  MapPos mill_pos = bad_map_pos;
  MapPos farm_pos = bad_map_pos;
  MapPos baker_pos = bad_map_pos;
  Game::ListBuildings buildings = game->get_player_buildings(player);
  for (Building *building : buildings) {
    if (building == nullptr)
      continue;
    if (building->get_type() == Building::TypeMill ||
        building->get_type() == Building::TypeFarm ||
        building->get_type() == Building::TypeBaker){
      // keeping this as FlagAndStraightLine because it should keep the building placement near the Inv
      if (find_nearest_inventory(map, player_index, building->get_position(), DistType::FlagAndStraightLine, &ai_mark_pos) != inventory_pos)
        continue;
    }
    // do NOT simply insert them as they are found or they won't be in priority order
    if (building->get_type() == Building::TypeMill) {
      mill_pos = building->get_position();
      AILogDebug["do_build_food_buildings"] << inventory_pos << " found mill at pos " << mill_pos << " with same closest inventory";
    }
    if (building->get_type() == Building::TypeFarm) {
      farm_pos = building->get_position();
      AILogDebug["do_build_food_buildings"] << inventory_pos << " found farm at pos " << farm_pos << " with same closest inventory";
    }
    if (building->get_type() == Building::TypeBaker) {
      baker_pos = building->get_position();
      AILogDebug["do_build_food_buildings"] << inventory_pos << " found baker at pos " << baker_pos << " with same closest inventory";
    }
  }
  // the insert order matters here, mill is the best thing to be near because it is the middle of the food chain
  if (mill_pos != bad_map_pos) {
    AILogDebug["do_build_food_buildings"] << inventory_pos << " inserting mill_pos " << mill_pos << " into 2nd/3rd farm build_positions list";
    farm_buildings.push_back(mill_pos);
  }
  if (farm_pos != bad_map_pos) {
    AILogDebug["do_build_food_buildings"] << inventory_pos << " inserting farm_pos " << farm_pos << " into 2nd/3rd farm build_positions list";
    farm_buildings.push_back(farm_pos);
  }
  if (baker_pos != bad_map_pos) {
    AILogDebug["do_build_food_buildings"] << inventory_pos << " inserting baker_pos " << baker_pos << " into 2nd/3rd farm build_positions list";
    farm_buildings.push_back(baker_pos);
  }

  AILogDebug["do_build_food_buildings"] << inventory_pos << " debug, here need_farm is " << need_farm;

  if (need_farm){
    // the minimum number of open tiles (where fields could be built) is a range
    //  with an absolute minimum where a farm will never be built unless more.
    //  This floor is increased by 2x the number of knights available, the idea being that if knights are available more huts can
    //   be built and more land claimed, allowing better farm placement.  But, if few knights available lower the standards
    //   so that at least SOME farm can be built, otherwise it risks never building any farm because of suboptimal areas and
    //   running out of knights.  Saw this in a game I played against AI Nov 2022. 
    //  and a "maximum minimum" which caps the influence of knights on the land requirements 
    mutex_lock("AI::do_build_food_buildings calling player->get_stats_serfs_idle()");
    serfs_idle = player->get_stats_serfs_idle();
    mutex_unlock();
    unsigned int idle_knights = serfs_idle[Serf::TypeKnight0] + serfs_idle[Serf::TypeKnight1] + serfs_idle[Serf::TypeKnight2] + serfs_idle[Serf::TypeKnight3] + serfs_idle[Serf::TypeKnight4];
    AILogDebug["do_build_food_buildings"] << inventory_pos << " lower_min_openspace_farm is " << lower_min_openspace_farm << ", idle_knights is " << idle_knights << ", upper_min_openspace_farm is " << upper_min_openspace_farm;
    unsigned int min_openspace_farm = lower_min_openspace_farm + idle_knights * 2;  // 2x the number of idle_knights
    if (min_openspace_farm > upper_min_openspace_farm){
      AILogDebug["do_build_food_buildings"] << inventory_pos << " min_openspace_farm of " << min_openspace_farm << " will be capped at the value of upper_min_openspace_farm" << upper_min_openspace_farm;
      min_openspace_farm = upper_min_openspace_farm;
    }
    AILogDebug["do_build_food_buildings"] << inventory_pos << " min_openspace_farm is " << min_openspace_farm;
    MapPos built_pos = bad_map_pos;
    // if this is not the first farm, try to build near existing food infrastructure
    //
    // if it is the first farm, or the unlikely event no other farm buildings found, it will try to build near fields
    if (farm_buildings.size() > 0){
      MapPosSet count_near_existing_farm_building;
      for (MapPos existing_farm_building_pos : farm_buildings){
        AILogDebug["do_build_food_buildings"] << inventory_pos << " debug, considering existing_farm_building_pos" << existing_farm_building_pos << " for placing wheat farm";
        int count = count_farmable_land(existing_farm_building_pos, spiral_dist(4), "dk_yellow");
        AILogDebug["do_build_food_buildings"] << inventory_pos << " existing_farm_building_pos " << existing_farm_building_pos << " has open_space/fields count: " << count << ", min_openspace_farm is " << min_openspace_farm;
        //// tolerate less available farm land than normal if it means building near existing farm buildings
        //if (count >= min_openspace_farm / 2) {
        //  AILogDebug["do_build_food_buildings"] << inventory_pos << " existing_farm_building_pos " << existing_farm_building_pos << " has enough (reduced req for existing food buildings near) open grass tiles to build a farm, adding to list";
          count_near_existing_farm_building.insert(std::make_pair(existing_farm_building_pos, count));
        //}
        //else {
        //  AILogDebug["do_build_food_buildings"] << inventory_pos << " existing_farm_building_pos " << existing_farm_building_pos << " does not have enough (reduced req for existing food buildings near) open grass tiles to build a farm here";
        //}
      }
      AILogInfo["do_build_food_buildings"] << inventory_pos << " want to build wheat farm";
      // build wheat farm near existing farm_building with most open grass
      MapPosVector search_positions = AI::sort_by_val_desc(count_near_existing_farm_building);
      for (MapPos potential_build_pos : search_positions) {
        AILogDebug["do_build_food_buildings"] << inventory_pos << " trying to build wheat farm near pos " << potential_build_pos;
        built_pos = AI::build_near_pos(potential_build_pos, AI::spiral_dist(4), Building::TypeFarm);
        if (built_pos != bad_map_pos && built_pos != notplaced_pos) {
          AILogInfo["do_build_food_buildings"] << inventory_pos << " built wheat farm at pos " << built_pos << ", near existing farm building at pos " << potential_build_pos;
          stock_building_counts.at(inventory_pos).count[Building::TypeFarm]++;
          stock_building_counts.at(inventory_pos).unfinished_count++;
          farm_buildings.push_back(farm_pos); // add this so the call to place mills and baker finds it
          need_farm = false;
          break;
        }
      }
    } // if found any existing farm_building to build near

    if (built_pos == bad_map_pos || built_pos == notplaced_pos) {
      AILogDebug["do_build_food_buildings"] << inventory_pos << " no existing farm_buildings to build near, or was unable to place near them, looking for open space elsewhere near this Inventory";

      // for first farm (or as fallback if can't connect to existing food buildings)
      //    try to build near open grass tiles
      // because farms take up a lot of space, try to place them a bit away from the castle
      //  to do this, instead of having current inventory_pos as the first area tried, make it the last but otherwise
      //   check centers in usual order this should result in a farm being built near the first knight hut expansion,
      //    or one of the first few.  THIS WAS WRITTEN PRIOR TO MULTIPLE ECONOMIES - needs work?
      MapPosVector farm_centers = stock_building_counts.at(inventory_pos).occupied_military_pos;
      // remove first element, which is always inv-flag/castle_pos  (NOT castle_flag_pos, which might make more sense)
      farm_centers.erase(farm_centers.begin(), farm_centers.begin() + 1);
      // add current inventory_pos back to the end
      farm_centers.push_back(inventory_pos);

      MapPosSet count_by_corner;
      for (MapPos center_pos : farm_centers) {
        AILogDebug["do_build_food_buildings"] << inventory_pos << " debug, considering center_pos " << center_pos << " for placing wheat farm near open fields near Inventory";
        // count farmable land in this area
        MapPosVector corners = AI::get_corners(center_pos);
        for (MapPos corner_pos : corners) {
          int count = count_farmable_land(corner_pos, spiral_dist(4), "dk_yellow");
          AILogDebug["do_build_food_buildings"] << inventory_pos << " corner_pos " << corner_pos << " has open_space/fields count: " << count << ", min_openspace_farm is " << min_openspace_farm;
          if (count >= min_openspace_farm) {
            AILogDebug["do_build_food_buildings"] << inventory_pos << " corner_pos " << corner_pos << " has enough open grass tiles to build a farm, adding to list";
            count_by_corner.insert(std::make_pair(corner_pos, count));
          }
          else {
            AILogDebug["do_build_food_buildings"] << inventory_pos << " corner_pos " << corner_pos << " does not have enough open grass tiles to build a farm here";
          }
        }
      } // foreach military building

      AILogInfo["do_build_food_buildings"] << inventory_pos << " want to build wheat farm";
      // build wheat farm near area with most open grass
      MapPosVector search_positions = AI::sort_by_val_desc(count_by_corner);
      MapPos built_pos = bad_map_pos;
      for (MapPos corner_pos : search_positions) {
        //ai_mark_pos.clear();
        AILogDebug["do_build_food_buildings"] << inventory_pos << " trying to build wheat farm near pos " << corner_pos;
        built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(4), Building::TypeFarm);
        if (built_pos != bad_map_pos && built_pos != notplaced_pos) {
          AILogInfo["do_build_food_buildings"] << inventory_pos << " built wheat farm at pos " << built_pos;
          stock_building_counts.at(inventory_pos).count[Building::TypeFarm]++;
          stock_building_counts.at(inventory_pos).unfinished_count++;
          need_farm = false;
          break;
        }
      }
      if (built_pos == bad_map_pos || built_pos == notplaced_pos) {
        AILogDebug["do_build_food_buildings"] << inventory_pos << " could not place wheat farm";
      }
    } // if already placed farm near existing farm_building      
  } // if needing farm

  //
  // grain mills and baker
  //
  // ORIGINALLY - build mill & baker near *already productive* wheat farms
  // UPDATE - build any placed wheat farm solely because the delay
  //   in waiting for wheat fields results in unacceptable road congestion and poor
  //   road connections between the mill/baker and wheat farm sometimes
  if (farm_count >= 1) {
    for (MapPos farm_building_pos : farm_buildings){
      // build grain mill
      if (stock_building_counts.at(inventory_pos).count[Building::TypeMill] < 1) {
        MapPos built_pos = bad_map_pos;
        AILogDebug["do_build_food_buildings"] << inventory_pos << " try to build grain mill near farm_building at pos " << farm_pos;
        built_pos = AI::build_near_pos(farm_pos, AI::spiral_dist(12), Building::TypeMill);
        if (built_pos != bad_map_pos && built_pos != notplaced_pos) {
          AILogDebug["do_build_food_buildings"] << inventory_pos << " built grain mill at pos " << built_pos;
          stock_building_counts.at(inventory_pos).count[Building::TypeMill]++;
          stock_building_counts.at(inventory_pos).unfinished_count++;
        }
      }
      // build bakery
      if (stock_building_counts.at(inventory_pos).count[Building::TypeBaker] < 1) {
        MapPos built_pos = bad_map_pos;
        AILogDebug["do_build_food_buildings"] << inventory_pos << " try to build bakery near farm at pos " << farm_pos;
        built_pos = AI::build_near_pos(farm_pos, AI::spiral_dist(12), Building::TypeBaker);
        if (built_pos != bad_map_pos && built_pos != notplaced_pos) {
          AILogDebug["do_build_food_buildings"] << inventory_pos << " built bakery at pos " << built_pos;
          stock_building_counts.at(inventory_pos).count[Building::TypeBaker]++;
          stock_building_counts.at(inventory_pos).unfinished_count++;
        }
      }
    } // foreach farm_building
  } // if any wheat farms, build mill and baker
  
} // end do_build_food_buildings


// coal mines are PLACED early in the AI loop to secure good placement, but not CONNECTED/BUILT until here
void
AI::do_connect_coal_mines() {
  ai_status.assign("do_connect_coal_mines");
  AILogDebug["do_connect_coal_mines"] << inventory_pos << " inside do_connect_coal_mines()";
  if (stock_building_counts.at(inventory_pos).needs_coal) {
    //  to avoid depleting pickaxes/miners, if low on miners/pickaxes:
    //   if REALM has at least one occupied coal mine, don't build another unless already have occupied iron mine
    if (serfs_idle[Serf::TypeMiner] + serfs_potential[Serf::TypeMiner] < 3){
      if (realm_occupied_building_count[Building::TypeCoalMine] >= 1 && realm_occupied_building_count[Building::TypeIronMine] == 0) {
        AILogDebug["do_connect_coal_mines"] << inventory_pos << " has <3 miners+pickaxes remaining, but this Inventory does not yet both an occupied iron mine.  Not connecting this coal mine to avoid running out of steel/tools deadlock";
        return;
      }
    }
    // connect a disconnected coal mine that was placed if conditions are right
    Flags flags_copy = *(game->get_flags());  // create a copy so we don't conflict with the game thread, and don't want to mutex lock for a long function
    for (Flag *flag : flags_copy) {
      if (flag == nullptr || flag->get_owner() != player_index || flag->is_connected() || !flag->has_building())
        continue;
      if (flag->get_building()->get_type() != Building::TypeCoalMine)
        continue;
      AILogDebug["do_connect_coal_mines"] << inventory_pos << " disconnected coal mine found with flag pos " << flag->get_position();
      AILogInfo["do_connect_coal_mines"] << inventory_pos << " trying to connect unfinished coal mine flag to road system";
      Road notused; // not used here, can I just pass a zero instead of &notused to build_best_road and skip initialization of a wasted object?
      //bool was_built = AI::build_best_road(flag->get_position(), road_options, &notused, "do_connect_coal_mines");
      // updated with verify_stock = true
      bool was_built = AI::build_best_road(flag->get_position(), road_options, &notused, "do_connect_coal_mines:Building::TypeCoalMine@"+std::to_string(flag->get_position()), Building::TypeNone, Building::TypeNone, bad_map_pos, true);
      if (!was_built) {
        AILogInfo["do_connect_coal_mines"] << inventory_pos << " failed to connect coal mine to road network!  demolishing it and its flag ";
        mutex_lock("AI::do_connect_coal_mines calling demolish flag&building (failed to connect coal mine)");
        game->demolish_building(flag->get_building()->get_position(), player);
        AILogDebug["do_connect_coal_mines"] << inventory_pos << " demolishing flag for coal mine that could not be connected to road network";
        // getting crashes here, try checking for flag is nullptr
        if (flag == nullptr){
          AILogWarn["do_connect_coal_mines"] << inventory_pos << " debug flag is now nullptr, not running demolish_flag";
        }else{
          AILogDebug["do_connect_coal_mines"] << inventory_pos << " debug about to call demolish_flag";
          game->demolish_flag(flag->get_position(), player);
        }
        mutex_unlock();
        // sleep to appear more human
        sleep_speed_adjusted(3000);
      }
      else {
        AILogInfo["do_connect_coal_mines"] << inventory_pos << " successfully connected unfinished coal mine to road network";
        stock_building_counts.at(inventory_pos).count[Building::TypeCoalMine]++;
        stock_building_counts.at(inventory_pos).unfinished_count++;
        break;
      }
    } // foreach flag
  }
  else {
    AILogDebug["do_connect_coal_mines"] << inventory_pos << " have sufficient coal and/or coal buildings, skipping";
  }
  AILogDebug["do_connect_coal_mines"] << inventory_pos << " done do_connect_coal_mines";
}


// iron mines are PLACED early in the AI loop to secure good placement, but not CONNECTED/BUILT until here
void
AI::do_connect_iron_mines() {
  ai_status.assign("do_connect_iron_mines");
  AILogDebug["do_connect_iron_mines"] << inventory_pos << " inside do_connect_iron_mines()";
  if (stock_building_counts.at(inventory_pos).needs_iron_ore) {
    //  to avoid depleting pickaxes/miners, if low on miners/pickaxes:
    //   if REALM has at least one occupied coal mine, don't build another unless already have occupied iron mine
    if (serfs_idle[Serf::TypeMiner] + serfs_potential[Serf::TypeMiner] < 3){
      if (realm_occupied_building_count[Building::TypeIronMine] >= 1 && realm_occupied_building_count[Building::TypeCoalMine] == 0) {
        AILogDebug["do_connect_iron_mines"] << inventory_pos << " has <3 miners+pickaxes remaining, but this Inventory does not yet both an occupied coal mine.  Not connecting this iron mine to avoid running out of steel/tools deadlock";
        return;
      }
    }
    // connect any disconnected iron mine that was placed if conditions are right
    Flags flags_copy = *(game->get_flags());  // create a copy so we don't conflict with the game thread, and don't want to mutex lock for a long function
    for (Flag *flag : flags_copy) {
      if (flag == nullptr || flag->get_owner() != player_index || flag->is_connected() || !flag->has_building())
        continue;
      if (flag->get_building()->get_type() != Building::TypeIronMine)
        continue;
      AILogDebug["do_connect_iron_mines"] << inventory_pos << " disconnected iron mine found with flag pos " << flag->get_position();
      AILogInfo["do_connect_iron_mines"] << inventory_pos << " trying to connect unfinished iron mine flag to road system";
      Road notused; // not used here, can I just pass a zero instead of &notused to build_best_road and skip initialization of a wasted object?
      //bool was_built = AI::build_best_road(flag->get_position(), road_options, &notused, "do_connect_iron_mines");
      // updated with verify_stock = true
      bool was_built = AI::build_best_road(flag->get_position(), road_options, &notused, "do_connect_iron_mines:Building::TypeIronMine@"+std::to_string(flag->get_position()), Building::TypeNone, Building::TypeNone, bad_map_pos, true);
      if (!was_built) {
        AILogInfo["do_connect_iron_mines"] << inventory_pos << " failed to connect iron mine to road network!  demolishing it and its flag ";
        mutex_lock("AI::do_connect_iron_mines calling demolish flag&building (failed to connect iron mine)");
        game->demolish_building(flag->get_building()->get_position(), player);
        AILogDebug["do_connect_iron_mines"] << inventory_pos << " demolishing flag for iron mine that could not be connected to road network";
        // getting crashes here, try checking for flag is nullptr
        if (flag == nullptr){
          AILogWarn["do_connect_iron_mines"] << inventory_pos << " debug flag is now nullptr, not running demolish_flag";
        }else{
          AILogDebug["do_connect_iron_mines"] << inventory_pos << " debug about to call demolish_flag";
          game->demolish_flag(flag->get_position(), player);
        }
        mutex_unlock();
        // sleep to appear more human
        sleep_speed_adjusted(3000);
      }
      else {
        AILogInfo["do_connect_iron_mines"] << inventory_pos << " successfully connected unfinished iron mine to road network";
        stock_building_counts.at(inventory_pos).count[Building::TypeIronMine]++;
        stock_building_counts.at(inventory_pos).unfinished_count++;
        break;
      }
    }
  }
  else {
    AILogDebug["do_connect_iron_mines"] << inventory_pos << " have sufficient iron and/or iron buildings, skipping";
  }
  AILogDebug["do_connect_iron_mines"] << inventory_pos << " done do_connect_iron_mines";
}

// build a steel smelter if one wasn't already created earlier to support toolmaker
void
AI::do_build_steelsmelter() {
  ai_status.assign("do_build_steelsmelter");
  AILogDebug["do_build_steelsmelter"] << inventory_pos << " inside do_build_steelsmelter()";
  if(get_stock_inv() == nullptr)
    return;
  unsigned int steel_count = get_stock_inv()->get_count_of(Resource::TypeSteel);
  int steelsmelter_count = stock_building_counts.at(inventory_pos).count[Building::TypeSteelSmelter];
  MapPos built_pos = bad_map_pos;
  if (steelsmelter_count > 1 || steel_count > (steel_max - anti_flapping_buffer)) {
    AILogDebug["do_build_steelsmelter"] << inventory_pos << " have steel smelter or sufficient steel, skipping";
    return;
  }

  // for first steel smelter (realm), wait until borders have expanded a bit
  if (realm_occupied_building_count[Building::TypeHut] < 4) {
    AILogDebug["do_build_steelsmelter"] << inventory_pos << " waiting until more knight huts occupied in realm to reduce chance of placement near castle";
    return;
  }

  AILogInfo["do_build_steelsmelter"] << inventory_pos << " want to build steelsmelter";
  // try to place steel smelter halfway between a coal mine and iron mine if both exist, preferring active ones
  // re-enabling halfway_pos checking again
  //  ehh I still think this is never going to be used effectively
  //  and even if it were able to, the build_better_road and normal
  //  two-target affinity should cover it anyway, disabling again
  MapPos halfway_pos = bad_map_pos;
  MapPosVector steelsmelter_pos = {};
  //if (halfway_pos != bad_map_pos) {
  //  AILogDebug["do_build_steelsmelter"] << inventory_pos << " adding pos halfway between a coal mine and an iron mine to first build_near center";
  //  steelsmelter_pos.push_back(halfway_pos);
  //}
  // score areas by most open space, simply to avoid crowding and allow good road connections
  MapPosSet open_space_counts = {};
  AILogDebug["do_build_steelsmelter"] << inventory_pos << " trying to build steelsmelter near any area with most open space";
  for (MapPos center_pos : stock_building_counts.at(inventory_pos).occupied_military_pos) {
    MapPosVector corners = AI::get_corners(center_pos);
    for (MapPos corner_pos : corners) {
      int count = count_open_space(game, corner_pos, spiral_dist(4), "gray");
      // strongly disfavor area right near castle
      if (inventory_pos == castle_flag_pos){
        if (get_straightline_tile_dist(map, castle_pos, corner_pos) < 9){
          AILogDebug["do_build_steelsmelter"] << inventory_pos << " this corner_pos is very close to castle, adding large penalty to placement score";
          if (count <= 20){ count = 0; } else { count -= 20; }
        }
      }
      open_space_counts.insert(std::make_pair(corner_pos, count));
    }
  }
  // sort areas by most open space
  MapPosVector search_positions = AI::sort_by_val_desc(open_space_counts);
  // add the sorted-by-space positions AFTER the halfway pos (if found)
  steelsmelter_pos.insert(steelsmelter_pos.end(), search_positions.begin(), search_positions.end());
  // try to build steelsmelter
  for (MapPos center_pos : steelsmelter_pos) {
    int steelsmelter_count = stock_building_counts.at(inventory_pos).count[Building::TypeSteelSmelter];
    if (steelsmelter_count >= 1) {
      AILogDebug["do_build_steelsmelter"] << inventory_pos << " Already placed steel smelter, not building more";
      break;
    }
    MapPosVector corners = AI::get_corners(center_pos);
    for (MapPos corner_pos : corners) {
      built_pos = bad_map_pos;
      built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(6), Building::TypeSteelSmelter);
      if (built_pos != bad_map_pos && built_pos != notplaced_pos) {
        AILogDebug["do_build_steelsmelter"] << inventory_pos << " built steel smelter at pos " << built_pos;
        stock_building_counts.at(inventory_pos).count[Building::TypeSteelSmelter];
        stock_building_counts.at(inventory_pos).unfinished_count++;        
        break;
      }
    }
    if (built_pos != bad_map_pos && built_pos != notplaced_pos) { break; }
  }
  AILogDebug["do_build_steelsmelter"] << inventory_pos << " done do_build_steelsmelter";
}


// build a blacksmith (weaponsmith)
void
AI::do_build_blacksmith() {
  ai_status.assign("do_build_blacksmith");
  AILogDebug["do_build_blacksmith"] << inventory_pos << " inside do_build_blacksmith()";
  MapPos built_pos = bad_map_pos;
  if (stock_building_counts.at(inventory_pos).count[Building::TypeWeaponSmith] < 1) {

    // don't build unless sufficient coal, and iron or steel
    if(get_stock_inv() == nullptr)
      return;
    unsigned int coal_count = get_stock_inv()->get_count_of(Resource::TypeCoal);
    unsigned int iron_ore_count = get_stock_inv()->get_count_of(Resource::TypeIronOre);
    unsigned int steel_count = get_stock_inv()->get_count_of(Resource::TypeSteel);

    if ((coal_count >= coal_min || stock_building_counts.at(inventory_pos).completed_count[Building::TypeCoalMine] > 0)
      && (iron_ore_count >= iron_ore_min || (stock_building_counts.at(inventory_pos).completed_count[Building::TypeIronMine] > 0) && stock_building_counts.at(inventory_pos).completed_count[Building::TypeSteelSmelter] > 0)
      || steel_count >= steel_min) {

      AILogInfo["do_build_blacksmith"] << inventory_pos << " want to build blacksmith";
      // score areas by most open space, simply to avoid crowding and allow good road connections
      MapPosSet open_space_counts = {};
      AILogDebug["do_build_blacksmith"] << inventory_pos << " trying to build blacksmith near any area with most open space";
      for (MapPos center_pos : stock_building_counts.at(inventory_pos).occupied_military_pos) {
        MapPosVector corners = AI::get_corners(center_pos);
        for (MapPos corner_pos : corners) {
          int count = count_open_space(game, corner_pos, spiral_dist(4), "black");
          open_space_counts.insert(std::make_pair(corner_pos, count));
        }
      }
      // build blacksmith in area with most open space
      MapPosVector search_positions = AI::sort_by_val_desc(open_space_counts);
      for (MapPos potential_build_pos : search_positions) {
        built_pos = bad_map_pos;
        built_pos = AI::build_near_pos(potential_build_pos, AI::spiral_dist(6), Building::TypeWeaponSmith);
        if (built_pos != bad_map_pos && built_pos != notplaced_pos) {
          AILogInfo["do_build_blacksmith"] << inventory_pos << " built blacksmith at pos " << built_pos;
          stock_building_counts.at(inventory_pos).count[Building::TypeWeaponSmith]++;
          stock_building_counts.at(inventory_pos).unfinished_count++;
          break;
        }
      }

    } // if have sufficient coal, iron, steel
    else {
      AILogDebug["do_build_blacksmith"] << inventory_pos << " not enough coal, or iron/steel to build blacksmith";
      AILogDebug["do_build_blacksmith"] << inventory_pos << " coal_count " << coal_count << ", coal_min " << coal_min << ", iron_ore_count " << iron_ore_count <<
        ", iron_ore_min " << iron_ore_min << ", steel_count " << steel_count << ", steel_min " << steel_min;
      AILogDebug["do_build_blacksmith"] << inventory_pos << " completed coal mines: " << stock_building_counts.at(inventory_pos).completed_count[Building::TypeCoalMine]
        << ", completed iron mines: " << stock_building_counts.at(inventory_pos).completed_count[Building::TypeIronMine]
        << ", completed steel foundries: " << stock_building_counts.at(inventory_pos).completed_count[Building::TypeSteelSmelter];
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
  ai_status.assign("do_build_gold_smelter_and_connect_gold_mines");
  AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " inside do_build_gold_smelter_and_connect_gold_mines()";
  // build a gold smelter
  //   if enough unprocessed gold ore is stored
  //   OR if already have a gold mine
  if(get_stock_inv() == nullptr)
    return;
  unsigned int gold_ore_count = get_stock_inv()->get_count_of(Resource::TypeGoldOre) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeGoldOre];
  if ((gold_ore_count >= gold_ore_min || stock_building_counts.at(inventory_pos).completed_count[Building::TypeGoldMine] > 0)
         && stock_building_counts.at(inventory_pos).count[Building::TypeGoldSmelter] < 1) {
    AILogInfo["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " want to build gold smelter";
    // try to place gold smelter halfway between a coal mine and gold mine if both exist, preferring active ones
    //  ehh I still think this is never going to be used effectively
    //  and even if it were able to, the build_better_road and normal
    //  two-target affinity should cover it anyway, disabling again
    MapPos halfway_pos = bad_map_pos;
    //halfway_pos = find_halfway_pos_between_buildings(inventory_pos, Building::TypeCoalMine, Building::TypeIronMine);
    MapPosVector goldsmelter_pos;
    //if (halfway_pos != bad_map_pos) {
    //  AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " adding pos halfway between a coal mine and an gold mine to first build_near center";
    //  goldsmelter_pos.push_back(halfway_pos);
    //}
    // score areas by most open space, simply to avoid crowding and allow good road connections
    MapPosSet open_space_counts = {};
    AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " trying to build goldsmelter near any area with most open space";
    for (MapPos center_pos : stock_building_counts.at(inventory_pos).occupied_military_pos) {
      MapPosVector corners = AI::get_corners(center_pos);
      for (MapPos corner_pos : corners) {
        int count = count_open_space(game, corner_pos, spiral_dist(4), "dk_yellow");
        open_space_counts.insert(std::make_pair(corner_pos, count));
      }
    }
    // sort areas by most open space
    MapPosVector search_positions = AI::sort_by_val_desc(open_space_counts);
    // add the sorted-by-space positions AFTER the halfway pos (if found)
    goldsmelter_pos.insert(goldsmelter_pos.end(), search_positions.begin(), search_positions.end());
    // try to build goldsmelter
    MapPos built_pos = bad_map_pos;
    for (MapPos center_pos : goldsmelter_pos) {
      int goldsmelter_count = stock_building_counts.at(inventory_pos).count[Building::TypeGoldSmelter];
      if (goldsmelter_count >= 1) {
        AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " Already placed gold smelter, not building more";
        break;
      }
      MapPosVector corners = AI::get_corners(center_pos);
      for (MapPos corner_pos : corners) {
        built_pos = bad_map_pos;
        built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(6), Building::TypeGoldSmelter);
        if (built_pos != bad_map_pos && built_pos != notplaced_pos) {
          AILogInfo["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " built gold smelter at pos " << built_pos;
          stock_building_counts.at(inventory_pos).count[Building::TypeGoldSmelter];
          stock_building_counts.at(inventory_pos).unfinished_count++;        
          break;
        }
      }
      if (built_pos != bad_map_pos && built_pos != notplaced_pos) { break; }
    }
  }

  //  if low on miners/pickaxes, don't connect a gold mine unless already having
  //   at least one occupied coal and iron mine to avoid depleting pickaxes/miners
  if (serfs_idle[Serf::TypeMiner] + serfs_potential[Serf::TypeMiner] < 3
    && (stock_building_counts.at(inventory_pos).occupied_count[Building::TypeCoalMine] == 0 || stock_building_counts.at(inventory_pos).occupied_count[Building::TypeIronMine] == 0)) {
    AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " has <2 miners+pickaxes remaining, but not yet both an occupied coal mine and an occupied iron mine.  Not connecting this gold mine";
    return;
  }

  //
  // connect a gold mine
  //
  if (stock_building_counts.at(inventory_pos).needs_gold_ore){
    AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " looking for a disconnected gold mine to connect to road system";
    Flags flags_copy = *(game->get_flags());  // create a copy so we don't conflict with the game thread, and don't want to mutex lock for a long function
    for (Flag *flag : flags_copy) {
      if (flag == nullptr || flag->get_owner() != player_index || flag->is_connected() || !flag->has_building())
        continue;
      if (flag->get_building()->get_type() != Building::TypeGoldMine)
        continue;
      AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " disconnected gold mine found with flag pos " << flag->get_position();
      AILogInfo["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " trying to connect unfinished gold mine flag to road system";
      Road notused; // not used here, can I just pass a zero instead of &notused to build_best_road and skip initialization of a wasted object?
      //bool was_built = AI::build_best_road(flag->get_position(), road_options, &notused, "do_connect_gold_mines");
      // updated with verify_stock = true
      bool was_built = AI::build_best_road(flag->get_position(), road_options, &notused, "do_connect_gold_mines:Building::TypeGoldMine@"+std::to_string(flag->get_position()), Building::TypeNone, Building::TypeNone, bad_map_pos, true);
      if (!was_built) {
        AILogInfo["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " failed to connect gold mine to road network!  demolishing it and its flag ";
        mutex_lock("AI::do_build_gold_smelter_and_connect_gold_mines calling demolish flag&building (failed to connect gold mine)");
        game->demolish_building(flag->get_building()->get_position(), player);
        AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " demolishing flag for gold mine that could not be connected to road network";
        // getting crashes here, try checking for flag is nullptr
        if (flag == nullptr){
          AILogWarn["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " debug flag is now nullptr, not running demolish_flag";
        }else{
          AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " debug about to call demolish_flag";
          game->demolish_flag(flag->get_position(), player);
        }
        mutex_unlock();
        // sleep to appear more human
        sleep_speed_adjusted(3000);
      }
      else {
        AILogInfo["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " successfully connected unfinished gold mine to road network";
        stock_building_counts.at(inventory_pos).count[Building::TypeGoldMine]++;
        stock_building_counts.at(inventory_pos).unfinished_count++;
        break;
      }
    }
  } else {
    // is it ever possible to have enough gold?  Is morale derived from this player's gold compared to each opponents gold??  Or total of everyone else's gold??
    //  it seems to be the player's ratio of refined gold compared to the opponents and the total amount of all gold ore originally in mountains on the map
    AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " have sufficient gold and/or gold buildings, skipping";
  }
  AILogDebug["do_build_gold_smelter_and_connect_gold_mines"] << inventory_pos << " done do_build_gold_smelter_and_connect_gold_mines";
}


// score enemy targets and attack based on own knight and morale strength
//   this function needs a lot more development
//
// NOTE ABOUT MORALE - in original game, for any knight fighting in their own territory (i.e. "defender"), morale is always = 100
//  regardless if they are in a building or free walking
//  This means that as the game progresses and attackers' morale increases over 100 from gold, the ATTACKERS
//  gain an advantage over defenders (i.e. fighters inside their own borders), regardless of defenders' gold/morale,
//  because defenders can never have more than 100 morale.  
//  The AI does not currently care about morale >75, so this does not affect AI decisionmaking
//
void
AI::do_attack() {
  MapPosSet scored_targets = {};
  AILogDebug["do_attack"] << "calling score_enemy_targets...";
  score_enemy_targets(&scored_targets);
  AILogDebug["do_attack"] << "score_enemy_targets call found " << scored_targets.size() << " targets";

  // this is the %morale drawn in the sett8 popup
  //draw_green_number(6, 63, (100*player->get_knight_morale())/0x1000);
  int morale = (100*player->get_knight_morale())/0x1000;  // this should be % morale, not the integer that defaults to 4096
  mutex_lock("AI::do_attack calling player->get_stats_serfs_idle()");
  serfs_idle = player->get_stats_serfs_idle();
  mutex_unlock();
  unsigned int idle_knights = serfs_idle[Serf::TypeKnight0] + serfs_idle[Serf::TypeKnight1] + serfs_idle[Serf::TypeKnight2] + serfs_idle[Serf::TypeKnight3] + serfs_idle[Serf::TypeKnight4];
  AILogDebug["do_attack"] << "idle_knights: " << idle_knights << ", morale%: " << morale;

  if (idle_knights >= knights_max && morale >= morale_max) {
    AILogDebug["do_attack"] << "idle_knights and morale both maxed! beeline for enemy castle (NOT IMPLEMENTED YET, COMING SOON)";
    AI::attack_nearest_target(&scored_targets, 0,  0);  // attack the best scoring target found, regardless of morale, defender ratio
    // NEEDS LOGIC TO BEELINE TO CASTLE!!!
  }
  else if (idle_knights >= 2*knights_max) {
    AILogDebug["do_attack"] << "idle_knights is DOUBLE max value, though morale could be low, attack anything";
    AI::attack_nearest_target(&scored_targets, 0,  0);  // attack the best scoring target found, regardless of morale, defender ratio
  }
  else if ((idle_knights >= knights_max && morale >= morale_min) || (morale >= morale_max && idle_knights >= knights_med) ) {
    AILogDebug["do_attack"] << "(idle_knights > max && morale > min ) OR (morale > max && idle_knights > med), attack liberally";
    AI::attack_nearest_target(&scored_targets, 25, 1.00);  // attack medium-high scoring targets, if at least even attacker-defender ratio
  }
  else if (idle_knights >= knights_max || (morale >= morale_min && idle_knights >= knights_med)) {
    AILogDebug["do_attack"] << "idle_knights " << idle_knights << " is above max or (morale " << morale << " is above min && idle_knights above med), attack only high value targets";
    AI::attack_nearest_target(&scored_targets, 50, min_knight_ratio_attack); // attack high scoring targets, and only if attackers > defender ratio
  }
  else if (idle_knights >= knights_med && morale >= morale_min) {
    AILogDebug["do_attack"] << "idle_knights " << idle_knights << " is above med and morale " << morale << " is above min, attack only very high value targets";
    AI::attack_nearest_target(&scored_targets, 75, min_knight_ratio_attack); // attack very high scoring targets, and only if attackers > defender ratio
  }
  // it feels like an Aggressive AI might consider attacking in these below situations, while a cautious one would not
  // Consider this for future Character/face implementatin
  else if (idle_knights >= knights_min && morale >= morale_min) {
    AILogDebug["do_attack"] << "idle_knights " << idle_knights << " is above min and morale is above min, attack only in extremely desperate situations (NONE YET DO NOT ATTACK)";
    // insert attack logic here... maybe only attack if unable to expand and missing a resource?
  }
  else if (idle_knights >= knights_min) {
    AILogDebug["do_attack"] << "idle_knights " << idle_knights << " is above min, but morale is below min, do not attack";
    // insert attack logic here... maybe only attack if unable to expand and missing a resource?
  }
  else if (idle_knights < knights_min) {
    AILogDebug["do_attack"] << "idle_knights " << idle_knights << " is below minimum, DO NOT ATTACK";
    // do not attack.  Also, cannot attack at this level of occupation (min/min)
  }
  else{
    AILogWarn["do_attack"] << "no do_attack conditions were true!  this is actually unexpected";
  }
  AILogDebug["do_attack"] << "done do_attack";
  // TEMP TEST
  //AILogDebug["do_attack"] << "TEMP SHORTCUT FORCE ATTACK do_attack";
  //AI::attack_nearest_target(&scored_targets);
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
  AILogDebug["do_build_better_roads_for_important_buildings"] << "inside do_build_better_roads_for_important_buildings";
  // only do this every X loops and once X knight huts built
  unsigned int completed_huts = realm_completed_building_count[Building::TypeHut];
  if (loop_count % (10 + 5*stocks_pos.size()) != 0 || completed_huts < 14) {  // reduce the frequency of this running with each warehouse built
    AILogDebug["do_build_better_roads_for_important_buildings"] << "running until a significant number of huts completed, and only every X loops";
    return;
  }
  ai_status.assign("do_build_better_roads_for_important_buildings");
  Game::ListBuildings buildings = game->get_player_buildings(player);
  for (Building *building : buildings) {
    if (building == nullptr)
      continue;
    Building::Type type = building->get_type();
    if (!building->is_done() || building->is_burning())
      continue;
    // only consider these building types for road improvement
    if (type != Building::TypeWeaponSmith && type != Building::TypeSteelSmelter
      && type != Building::TypeGoldSmelter && type != Building::TypeCoalMine
      && type != Building::TypeIronMine && type != Building::TypeGoldMine
      && type != Building::TypeBaker && type != Building::TypeButcher) {
      continue;
    }
    AILogDebug["do_build_better_roads_for_important_buildings"] << "do_build_better_roads_for_important_buildings found high-priority building of type " << NameBuilding[type] << " at pos " << building->get_position();
    //ai_mark_pos.erase(building->get_position());
    //ai_mark_pos.insert(ColorDot(building->get_position(), "dk_coral"));
    //sleep_speed_adjusted(3000);
    road_options.set(RoadOption::Improve);
    MapPos building_flag_pos = map->move_down_right(building->get_position());
    Road road_built;
    if(build_best_road(building_flag_pos, road_options, &road_built, "do_build_better_roads:"+NameBuilding[type]+"@"+std::to_string(building_flag_pos), type)){
      AILogDebug["do_build_better_roads_for_important_buildings"] << "successfully built an improved road connection for building of type " << NameBuilding[type] << " at pos " << building->get_position() << " to its affinity building (whatever that may be - check build_best_road result)";
      ai_mark_build_better_roads->push_back(road_built);
    }
    road_options.reset(RoadOption::Improve);
  }
  AILogDebug["do_build_better_roads_for_important_buildings"] << "done do_build_better_roads_for_important_buildings";
  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["do_build_better_roads_for_important_buildings"] << "done do_build_better_roads_for_important_buildings call took " << duration;
}


// once all necessary buildings built for castle, build a warehouse and parallel infrastructure
//
// BIG IDEA - once warehouse is built (and occupied?)  look for all nearby un-tracked buildings
//   such as knight huts, fisherman, demolish their roads and re-create roads to this new Inventory
//   ALSO, for any tracked buildings... possibly take these also if certain conditions are met,
//    such as calculating if the "stolen-from" Inventory could place a new one, 
//
//
void
AI::do_build_warehouse() {
  AILogDebug["do_build_warehouse"] << "inside do_build_warehouse";
  // don't build warehouses until territory reaches a certain size
  unsigned int completed_huts = realm_completed_building_count[Building::TypeHut];
  if (loop_count % 10 != 0 || completed_huts < 16) {
    AILogDebug["do_build_warehouse"] << "not considering building warehouses (stocks) until a significant number of huts completed, and only every X loops";
    return;
  }
  int warehouse_count = realm_building_count[Building::TypeStock];
  int completed_warehouse_count = realm_completed_building_count[Building::TypeStock];
  if (warehouse_count > completed_warehouse_count) {
    AILogDebug["do_build_warehouse"] << "already placed a new warehouse (stock), not building another until previous one is built";
    return;
  }
  unsigned int planks_count = realm_inv[Resource::TypePlank];
  unsigned int stones_count = realm_inv[Resource::TypeStone];
  // increase the requirements a bit for each warehouse because it is realm-wide check
  if (planks_count < planks_min + ( 4 * warehouse_count) || stones_count < stones_min + ( 2 * warehouse_count)) {
    AILogDebug["do_build_warehouse"] << "not building warehouse, not enough planks or stones in realm";
    return;
  }

  for (MapPos center_pos : realm_occupied_military_pos) {
    AILogDebug["do_build_warehouse"] << "considering building warehouse near corners around realm_occupied_military_pos " << center_pos;
    MapPosVector corners = AI::get_corners(center_pos);
    for (MapPos corner_pos : corners) {
      if (get_straightline_tile_dist(map, corner_pos, castle_pos) <= 40){  
        AILogDebug["do_build_warehouse"] << "corner_pos " << corner_pos << " is too close to the castle, skipping this area";
        continue;
      }
      if (find_nearest_building(corner_pos, CompletionLevel::Unfinished, Building::TypeStock, 40) != nullptr) {
        AILogDebug["do_build_warehouse"] << "there is already a stock near corner_pos " << corner_pos << ", skipping this area";
        continue;
      }
      AILogDebug["do_build_warehouse"] << "try to build warehouse near pos " << corner_pos;
      MapPos built_pos = bad_map_pos;
      built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(5), Building::TypeStock);
      if (built_pos != bad_map_pos && built_pos != notplaced_pos) {
        AILogDebug["do_build_warehouse"] << "built warehouse (stock) at pos " << built_pos;
        return;
      }
    }
  }
  AILogDebug["do_build_warehouse"] << "done do_build_warehouse";
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
  AILogDebug["do_get_inventory"] << "inside do_get_inventory for pos " << inventory_pos;
  if (inventory_pos == castle_flag_pos) {
    AILogDebug["do_get_inventory"] << "this stock is the castle ";
  }
  AILogDebug["do_get_inventory"] << "getting player's stock inventory for stock at pos " << inventory_pos;
  Building *this_stock = game->get_building_at_pos(map->move_up_left(inventory_pos));
  if (this_stock == nullptr) {
    AILogDebug["do_get_inventory"] << "got nullptr for stock at pos " << inventory_pos << "!";
    return;
  }
  Inventory *this_stock_inv = this_stock->get_inventory();
  if (this_stock_inv == nullptr) {
    AILogDebug["do_get_inventory"] << "got nullptr for stock_inv at pos " << inventory_pos << "!";
    return;
  }
  this_stock_inv_index = this_stock_inv->get_index();
  AILogDebug["do_get_inventory"] << "this_stock_inv_index is game->inventories[" << this_stock_inv << "]";

  // dump stock inventory every loop for debugging
  for (int i = 0; i < 26; i++) {
    AILogDebug["do_get_inventory"] << "stock at pos " << inventory_pos << " has " << NameResource[i] << ": " << get_stock_inv()->get_count_of(Resource::Type(i));
  }

  for (int i = 0; i < 26; i++) {
    stock_res_sitting_at_flags[inventory_pos][Resource::Type(i)] = 0;
  }

  AILogDebug["do_get_inventory"] << "done get_inventory";
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
  Flags flags_copy = *(game->get_flags());  // create a copy so we don't conflict with the game thread, and don't want to mutex lock for a long function
  for (Flag *flag : flags_copy) {
    if (flag == nullptr)
      continue;
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
        //AILogDebug["do_count_resources_sitting_at_flags"] << "flag at pos " << flag->get_position() << " has a resource of type " << NameResource[type] << " at slot " << i;
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



// refuse to build new knight hut in this Inventory area if:
// - REALM wood below crit   (don't check stone, it will come)
// - REALM idle knights in Inventories below min
// - Inventory unfinished_knight_huts above limit
// NOTE this function relies on potentially stale Inventory, Serf, and building count data
//   but that should be okay??  
bool
AI::do_can_build_knight_huts() {
  //
  // NOTE!!!!! because verify_stock is not the norm, it is likely that buildings are being marked as "Attached" to 
  //  an Inv that they are not closest to once the first non-castle Inv is built.  This will mess up these checks
  //  to prove this theory, for now I am turning back on running of update_buildings every run here
  //
  //update_buildings();  // this did't fix it, removing again

  //AILogDebug["do_can_build_knight_huts"] << inventory_pos << " inside do_can_build_knight_huts";
  // ensure REALM planks above crit
  unsigned int realm_planks_count = realm_inv[Resource::TypePlank];
  if (realm_planks_count < planks_min) {
    AILogDebug["do_can_build_knight_huts"] << inventory_pos << " realm_planks_count " << realm_planks_count << " is below planks_min " << planks_min << ", cannot build knight huts anywhere";
    return false;
  }
  // ensure REALM idle knights in Inventories above min
  // NOTE - to avoid issue where at start of new game there are few Knights and Serfs in Castle, but plenty of swords/shields
  //    don't perform this check until at least X knight huts built
  if (realm_completed_building_count[Building::TypeHut] >= 3) {
    unsigned int realm_idle_knights = serfs_idle[Serf::TypeKnight0] + serfs_idle[Serf::TypeKnight1] + serfs_idle[Serf::TypeKnight2] + serfs_idle[Serf::TypeKnight3] + serfs_idle[Serf::TypeKnight4];
    if (realm_idle_knights < knights_min) {
      AILogDebug["do_can_build_knight_huts"] << inventory_pos << " realm_idle_knights " << realm_idle_knights << " is below knights_min " << knights_min << ", cannot build knight huts anywhere";
      return false;
    }
  }
  // ensure Inventory unfinished_knight_huts below limit
  if (stock_building_counts.at(inventory_pos).unfinished_hut_count >= max_unfinished_huts) {
    AILogDebug["do_can_build_knight_huts"] << inventory_pos << " Inventory unfinished_huts_count limit " << max_unfinished_huts << " reached, cannot build knight huts for this Inventory";
    return false;
  }
  //AILogDebug["do_can_build_knight_huts"] << inventory_pos << " this Inventory area can build more knight_huts";
  return true;
}


// refuse to build new buildings in this Inventory area if:
// - REALM wood below crit   (don't check stone, it will come)
// - Inventory unfinished_buildings above limit
// NOTE this function relies on potentially stale Inventory, Serf, and building count data
//   but the best solution I think is to improve the quality of the data by updating it as it changes
//   which is already mostly happening, rather than re-checking everything constantly
//
// MAJOR IMPROVEMENT - instead of wait until completed, only wait until all construction materials + builder arrive
//   and if so do not mark as unfinished.  To do this need to see simple it is to check completion status for each
//   building and see how many materials required for each... possible to check the flag requests?  rather than
//   completion status?  see how the Sawmill check one I wrote is doing it
//
//

bool
AI::do_can_build_other() {
  //
  // NOTE!!!!! because verify_stock is not the norm, it is likely that buildings are being marked as "Attached" to 
  //  an Inv that they are not closest to once the first non-castle Inv is built.  This will mess up these checks
  //  to prove this theory, for now I am turning back on running of update_buildings every run here
  //
  //update_buildings();  // this did't fix it, removing again

  //AILogDebug["do_can_build_other"] << inventory_pos << " inside do_can_build_other";
  // ensure REALM planks above crit
  unsigned int realm_planks_count = realm_inv[Resource::TypePlank];
  if (realm_planks_count < planks_min) {
    AILogDebug["do_can_build_other"] << inventory_pos << " realm_planks_count " << realm_planks_count << " is below planks_min " << planks_min << ", cannot build generic buildings anywhere";
    return false;
  }
  // ensure Inventory unfinished_count below limit
  if (stock_building_counts.at(inventory_pos).unfinished_count >= max_unfinished_buildings) {
    AILogDebug["do_can_build_other"] << inventory_pos << " Inventory unfinished_count limit " << max_unfinished_buildings << " reached, cannot build generic buildings for this Inventory";
    return false;
  }
  //AILogDebug["do_can_build_other"] << inventory_pos << " this Inventory area can build more generic buildings";
  return true;
}


// for this Inventory, check to see which
//  resources are needed and note them for both expand_borders calls
//  and do_<build_XXX_buildings> calls
// the primary purpose of this function is to skip resources that this
//  Inventory has already maxed out
void
AI::do_check_resource_needs(){
  AILogDebug["do_check_resource_needs"] << inventory_pos << " inside do_check_resource_needs";

  // IN GENERAL:
  //  if res > max, do not need res, do not bother to check for res buildings
  //  if res < max, if res buildings built, do not need res
  //  if res < max, if res buildings not built, need res

  //
  // wood planks
  //
  if(get_stock_inv() == nullptr)
    return;
  /*
  unsigned int wood_count = get_stock_inv()->get_count_of(Resource::TypePlank) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypePlank];
  //   include raw logs that will be processed into planks at SawMill
  wood_count += get_stock_inv()->get_count_of(Resource::TypeLumber) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeLumber];
  */

  // don't include planks at flags, it is critical to have planks available *in* the Inventory
  //unsigned int wood_count = get_stock_inv()->get_count_of(Resource::TypePlank) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypePlank];
  unsigned int wood_count = get_stock_inv()->get_count_of(Resource::TypePlank);

  // include raw logs as they will become planks, but only if:
  // - at least planks_min planks stored in Inventory already
  // - one connected sawmill
  if (wood_count >= planks_min && stock_building_counts.at(inventory_pos).connected_count[Building::TypeSawmill] > 0){
    wood_count += get_stock_inv()->get_count_of(Resource::TypeLumber) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeLumber];
  }

  //if (wood_count < planks_max) {
  stock_building_counts.at(inventory_pos).needs_wood = false;
  if (wood_count < (planks_max + anti_flapping_buffer)) {
    AILogDebug["do_check_resource_needs"] << inventory_pos << " desire more wood";
    stock_building_counts.at(inventory_pos).needs_wood = true;
    //if (stock_building_counts.at(inventory_pos).count[Building::TypeSawmill] < 1 || stock_building_counts.at(inventory_pos).count[Building::TypeLumberjack] < 2) {
    //  AILogDebug["do_check_resource_needs"] << inventory_pos << " desire more wood buildings";
    //  stock_building_counts.at(inventory_pos).needs_wood = true;
      expand_towards.insert("trees");
    //}
  }

  //
  // stones
  //
  // don't include stones at flags, it is critical to have stones available in Inventory
  //unsigned int stones_count = get_stock_inv()->get_count_of(Resource::TypeStone) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeStone];
  unsigned int stones_count = get_stock_inv()->get_count_of(Resource::TypeStone);
  stock_building_counts.at(inventory_pos).needs_stone = false;
  if (stones_count < stones_max) {
    AILogDebug["do_check_resource_needs"] << inventory_pos << " desire more stones";
    stock_building_counts.at(inventory_pos).needs_stone = true;
    //if (stock_building_counts.at(inventory_pos).count[Building::TypeStonecutter] < 1) {
    //  AILogDebug["do_check_resource_needs"] << inventory_pos << " desire more stone buildings";
    //  stock_building_counts.at(inventory_pos).needs_stone = true;
      expand_towards.insert("stones");
    //}
  }

  //
  // defensive buffer
  //
  // if there is any unowned land near the castle, encourage claiming it to
  //  create a buffer around the castle
  if (inventory_pos == castle_flag_pos){
    for (unsigned int x = 0; x < AI::spiral_dist(12); x++) {
      MapPos pos = map->pos_add_extended_spirally(castle_flag_pos, x);
      if (!map->has_owner(pos)){
        if(expand_towards.count("castle_buffer") == 0){
          AILogDebug["do_check_resource_needs"] << inventory_pos << " there is unowned land near the castle, adding castle_buffer to expansion goals";
          expand_towards.insert("castle_buffer");
        }
      }
    }
  }

  //
  // foods
  //
  unsigned int stored_food_count = 0;
  // most important are ready-to-use food items stored in this Inventory
  if(get_stock_inv() == nullptr)
    return;
  stored_food_count += get_stock_inv()->get_count_of(Resource::TypeBread) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeBread];
  stored_food_count += get_stock_inv()->get_count_of(Resource::TypeMeat) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeMeat];
  stored_food_count += get_stock_inv()->get_count_of(Resource::TypeFish) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeFish];
  // also include pigs, wheat, and flour because they ultimately will become food, but cap the amount they can contribute to the total
  //  to avoid situation where the processing buildings are missing and they never become food
  unsigned int potential_food_count = 0;
  potential_food_count += get_stock_inv()->get_count_of(Resource::TypePig) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypePig];
  potential_food_count += get_stock_inv()->get_count_of(Resource::TypeWheat) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeWheat];
  potential_food_count += get_stock_inv()->get_count_of(Resource::TypeFlour) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeFlour];
  if (potential_food_count > ((food_max + 1) / 2)){
    //AILogDebug["do_demolish_excess_food_buildings"] << inventory_pos << " capping contribution of potential_food_count to half of food_max";
    potential_food_count = (food_max + 1) / 2;
  }
  // also include food and potential food sitting at flags nearby, but cap the amount they can contribute to the total
  //   in case they are stuck in road congestion or some other problem
  unsigned int food_at_flags = 0;
  food_at_flags += stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeBread];
  food_at_flags += stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeMeat];
  food_at_flags += stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeFish];
  food_at_flags += stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypePig];
  food_at_flags += stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeWheat];
  food_at_flags += stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeFlour];
  if (food_at_flags > ((food_max + 1) / 2)){
    //AILogDebug["do_check_resource_needs"] << inventory_pos << " capping contribution of food_at_flags to half of food_max";
    food_at_flags = (food_max + 1) / 2;
  }
  unsigned int adjusted_food_count = stored_food_count;
  if (food_at_flags + potential_food_count > ((food_max + 1) / 3)*2 ){
    // the COMBINED contribution of potential food and food-at-flags can't be more than 75% of food_max
    //AILogDebug["do_check_resource_needs"] << inventory_pos << " capping combined contribution of potential_food_count and food_at_flags to 66% of food_max";
    adjusted_food_count += ((food_max + 1) / 3)*2;
  }else{
    // not capped
    adjusted_food_count += food_at_flags + potential_food_count;
  }
  //AILogDebug["do_check_resource_needs"] << inventory_pos << " debug: stored_food_count " << stored_food_count << ", potential_food_count " << potential_food_count << ", food_at_flags " << food_at_flags << ", adjusted_food_count " << adjusted_food_count << ", food_max " << food_max;
  //AILogDebug["do_check_resource_needs"] << inventory_pos << " adjusted food_count at inventory_pos " << inventory_pos << ": " << adjusted_food_count;
  if (adjusted_food_count < food_max) {
    AILogDebug["do_check_resource_needs"] << inventory_pos << " desire more food";
    // don't check for food buildings because the do_food_buildings function does, and the checks are too complex to move into here
    //if (stock_building_counts.at(inventory_pos).count[Building::TypeFarm] < 1) {
      //AILogDebug["do_check_resource_needs"] << inventory_pos << " desire more food buildings";
      stock_building_counts.at(inventory_pos).needs_foods = true;
      expand_towards.insert("foods");
    //}
  }


  //
  // coal
  //
  if(get_stock_inv() == nullptr)
    return;
  unsigned int coal_count = get_stock_inv()->get_count_of(Resource::TypeCoal) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeCoal];
  if (coal_count < coal_max) {
    AILogDebug["do_check_resource_needs"] << inventory_pos << " desire more coal";
    if (stock_building_counts.at(inventory_pos).count[Building::TypeCoalMine] < max_coalmines) {
      AILogDebug["do_check_resource_needs"] << inventory_pos << " desire more coal mines";
      stock_building_counts.at(inventory_pos).needs_coal = true;
      expand_towards.insert("hills");
      expand_towards.insert("coal");
    }
  }


  //
  // iron & steel
  //  if steel > max, ignore steel buildings, iron ore, iron buildings
  //
  if(get_stock_inv() == nullptr)
    return;
  unsigned int steel_count = get_stock_inv()->get_count_of(Resource::TypeSteel) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeSteel];
  if (steel_count < steel_max){
    AILogDebug["do_check_resource_needs"] << inventory_pos << " desire more steel";
    //if (stock_building_counts.at(inventory_pos).count[Building::TypeSteelSmelter] < 1) {
    //  AILogDebug["do_check_resource_needs"] << inventory_pos << " desire a steel smelter";
    //  stock_building_counts.at(inventory_pos).needs_steel = true;
    //}
    unsigned int iron_ore_count = get_stock_inv()->get_count_of(Resource::TypeIronOre) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeIronOre];
    if (iron_ore_count < iron_ore_max) {
      AILogDebug["do_check_resource_needs"] << inventory_pos << " desire more iron ore";
      if (stock_building_counts.at(inventory_pos).count[Building::TypeIronMine] < max_ironmines) {
        AILogDebug["do_check_resource_needs"] << inventory_pos << " desire more iron mines";
        stock_building_counts.at(inventory_pos).needs_iron_ore = true;
        expand_towards.insert("hills");
        expand_towards.insert("iron");
      }
    }
  }


  //
  // gold ore  (no limits on gold bars)
  //
  //if (stock_building_counts.at(inventory_pos).count[Building::TypeGoldSmelter] < 1){
  //  AILogDebug["do_check_resource_needs"] << inventory_pos << " desire a gold smelter";
  //  e
  //}
  if(get_stock_inv() == nullptr)
    return;
  unsigned int gold_ore_count = get_stock_inv()->get_count_of(Resource::TypeGoldOre) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeGoldOre];
  if (gold_ore_count < gold_ore_max) {
    AILogDebug["do_check_resource_needs"] << inventory_pos << " desire more gold ore";
    if (stock_building_counts.at(inventory_pos).count[Building::TypeGoldMine] < max_goldmines) {
      AILogDebug["do_check_resource_needs"] << inventory_pos << " desire more gold mines";
      stock_building_counts.at(inventory_pos).needs_gold_ore = true;
      expand_towards.insert("hills");
      expand_towards.insert("gold");
    }
  }

}


// when a new warehouse is first built and occupied,
//  reconnect all nearby non-tracked buildings to it
// and possible tracked buildings if conditions met?
void
AI::do_create_star_roads_for_new_warehouses(){
  if (new_stocks.size() == 0){
    AILogDebug["do_create_star_roads_for_new_warehouse"] << inventory_pos << " new_stocks has no entries, nothing to do";
    return;
  }

  for (MapPos new_stock_flag_pos : new_stocks){
    // this is really the only check that matters, skip the others
    if (new_stock_flag_pos != inventory_pos){
      AILogDebug["do_create_star_roads_for_new_warehouse"] << inventory_pos << " new_stocks contains a stock with flag pos " << new_stock_flag_pos << ", but it is not the currently selected inventory_pos, skipping";
      continue;
    }
    /*
    AILogInfo["do_create_star_roads_for_new_warehouse"] << inventory_pos << " new_stocks contains a stock with flag pos " << new_stock_flag_pos << ", checking to see if it has become occupied";
    Flag *new_stock_flag = game->get_flag_at_pos(new_stock_flag_pos);
    if (new_stock_flag == nullptr){
      AILogWarn["do_create_star_roads_for_new_warehouse"] << inventory_pos << " new_stocks contains a stock with flag pos " << new_stock_flag_pos << ", but get_flag_at_pos returns nullptr!  removing this from new_stocks list";
      new_stocks.erase(new_stock_flag_pos);
      continue;
    }
    Building *new_stock_building = new_stock_flag->get_building();
    if (new_stock_building == nullptr){
      AILogWarn["do_create_star_roads_for_new_warehouse"] << inventory_pos << " new_stocks contains a stock with flag pos " << new_stock_flag_pos << ", but flag->get_building returns nullptr!  removing this from new_stocks list";
      new_stocks.erase(new_stock_flag_pos);
      continue; 
    }
    if (!new_stock_building->is_done()){
      AILogInfo["do_create_star_roads_for_new_warehouse"] << inventory_pos << " new_stocks contains a stock with flag pos " << new_stock_flag_pos << ", this stock is not completed yet, try again later";
      continue;
    }
    if (!new_stock_building->has_serf()){
      AILogInfo["do_create_star_roads_for_new_warehouse"] << inventory_pos << " new_stocks contains a stock with flag pos " << new_stock_flag_pos << ", this stock is not occupied yet, try again later";
      continue;
    }
    if (!new_stock_flag->accepts_resources()){
      // I think this is essentially the same check as building->has_serf for a Stock, but double-check to be sure
      AILogWarn["do_create_star_roads_for_new_warehouse"] << inventory_pos << " new_stocks contains a stock with flag pos " << new_stock_flag_pos << ", building->serf_serf is true but building flag->accepts_resources is false!  this is unexpected!  try again later";
      continue;
    }
    */

    // NEWLY-OCCUPIED LIVE STOCK DETECTED, DO THE FUNCTION
    AILogDebug["do_create_star_roads_for_new_warehouse"] << inventory_pos << " new_stocks contains a stock with flag pos " << new_stock_flag_pos << ", this stock WAS JUST COMPLETED, doing things";
    // remove it from the new_stocks list to avoid infinite loop
    new_stocks.erase(new_stock_flag_pos);

    // check for around the Inventory for standalone flags,
    //  or flags connected to non-tracked buildings (knight huts and fisherman, basically)
    MapPosVector eligible_flags = {};
    for (unsigned int x = 0; x < AI::spiral_dist(14); x++) {
      MapPos pos = map->pos_add_extended_spirally(inventory_pos, x);
      if (!map->has_flag(pos))
        continue;  // skip if no flag
      Flag *flag = game->get_flag_at_pos(pos);
      if (flag == nullptr)
        continue;  // skip if flag not found
      //if (!flag->has_building())
      //  continue;  // skip if no building
      if (flag->has_building()){
        Building *building = flag->get_building();
        if (building == nullptr)
          continue;  // skip if building not found
        if (!building->is_done())
          continue;  // skip buildings under construction
        Building::Type type = building->get_type();
        if (type == Building::TypeFisher || type == Building::TypeHut || type == Building::TypeTower || type == Building::TypeFortress){
          AILogDebug["do_create_star_roads_for_new_warehouse"] << inventory_pos << " found a non-tracked building of type " << NameBuilding[type] << " at pos " << pos;
        }else{
          // skip if building is a tracked type (only non-tracked are eligible for now)
          AILogDebug["do_create_star_roads_for_new_warehouse"] << inventory_pos << " skipping tracked building of type " << NameBuilding[type] << " at pos " << pos;
          continue; 
        }
        MapPos building_inv_pos = find_nearest_inventory(map, player_index, pos, DistType::FlagOnly, &ai_mark_pos);
        if (building_inv_pos == bad_map_pos)
          continue;  // skip if current inv not found
        //if (building_inv_pos == inventory_pos)
        //  continue;  // skip if THIS is the current inv ?  maybe still consider for rebuild in case it results in better connect?
        //AILogDebug["do_create_star_roads_for_new_warehouse"] << inventory_pos << " building of type " << NameBuilding[type] << " at pos " << pos << " has closest-by-flag Inventory of " << building_inv_pos;
        //int distance_to_current_inv = get_straightline_tile_dist(map, pos, building_inv_pos);
        //int distance_to_this_inv = get_straightline_tile_dist(map, pos, inventory_pos);
        //if (distance_to_this_inv < distance_to_current_inv){
        //  AILogDebug["do_create_star_roads_for_new_warehouse"] << inventory_pos << " building of type " << NameBuilding[type] << " at pos " << pos << " straightline dist " << distance_to_this_inv << " to this new Inv is less than dist " << distance_to_current_inv << " to its current Inv pos " << building_inv_pos;
        //  eligible_buildings.push_back(pos);
        //}
        AILogDebug["do_create_star_roads_for_new_warehouse"] << inventory_pos << " adding eligible_flag attached to building of type " << NameBuilding[type] << " at pos " << pos;
      }else{
        AILogDebug["do_create_star_roads_for_new_warehouse"] << inventory_pos << " found a flag with no attached building at pos " << pos;
      }
      // add flag to eligible list
      eligible_flags.push_back(pos);
    }
    AILogDebug["do_create_star_roads_for_new_warehouse"] << inventory_pos << " found " << eligible_flags.size() << " eligible_flags to consider";


    // do a big spiral search around the new Inventory, note any buildings that are straight-line distance closer
    //  to this Inventory than their current one
    bool repeat = false;
    int loops = 0;
    int roads_removed = 0;
    Direction only_dir = DirectionNone;
    while (true){
      repeat = false;
      loops++;
      for (MapPos flag_pos : eligible_flags){

        if (!map->has_flag(flag_pos))
          continue;  // skip if no flag
        Flag *flag = game->get_flag_at_pos(flag_pos);
        if (flag == nullptr)
          continue;  // skip if flag not found
        if (!flag->is_connected())
          continue;  // skip if no path (possibly it was deleted in a prior loop)
        int paths = 0;
        for (Direction dir : cycle_directions_cw()){
          if (map->has_path_IMPROVED(flag_pos, dir)){
            paths++;
            only_dir = dir;
            if (paths > 1){break;}
          }
        }
        if (paths > 1){
          AILogDebug["do_create_star_roads_for_new_warehouse"] << inventory_pos << " rejecting flag at flag_pos " << flag_pos << ", it has more than one path"; 
          continue;
        }
        // SHOULD WE CHECK FOR VALUABLE RESOURCES CURRENTLY BEING TRANSPORTED ALONG THE ROAD??
        //  ESPECIALLY SERFS AND RESUORCES DESTINED FOR BUILDINGS, might never arrive?
        //  depends on how well the resource lost stuff works
        //not now... first see how well it works

        AILogDebug["do_create_star_roads_for_new_warehouse"] << inventory_pos << " destroying road for flag_pos " << flag_pos << " in dir " << only_dir << " / " << NameDirection[only_dir];
        mutex_lock("AI::do_create_star_roads_for_new_warehouse calling game->demolish_road()");
        bool was_destroyed = game->demolish_road(map->move(flag_pos, only_dir), player);
        mutex_unlock();
        if (was_destroyed){
          roads_removed++;
          // any time a road is removed, force retrying the entire set in case new possibilities open up
          repeat = true;
          // sleep a bit to be more human like
          sleep_speed_adjusted(1000);
        }
      }

      // retry the whole list if any roads removed, it opens new possibilities
      if (repeat){
        AILogDebug["do_create_star_roads_for_new_warehouse"] << inventory_pos << " at least one road was removed, re-evaluating the entire list again to see if more possibilities opened.  loops: " << loops;
        if (loops > 50){
          AILogWarn["do_create_star_roads_for_new_warehouse"] << inventory_pos << " maximum loops hit!  this is probably a bug, returning now";
          return;
        }
      }else{
        AILogDebug["do_create_star_roads_for_new_warehouse"] << inventory_pos << " done, loops performed: " << loops << ", roads removed: " << roads_removed;
        return;
      }

    } // while true / keep re-checking Flag list until no more paths can be removed

  } // foreach new_stocks, check if completed
} // end do_create_star_roads_for_new_warehouse


//
// for every flag owned by this player, record every other flag this flag connects to
//  and identify groups of flags that are not connected to each ohter
//
void
AI::do_connect_disconnected_road_networks(){
  AILogDebug["do_connect_disconnected_road_networks"] << "starting";
  ai_status.assign("do_connect_disconnected_road_networks");

  std::vector<std::set<unsigned int>> network = {};

  Flags flags_copy = *(game->get_flags());  // create a copy so we don't conflict with the game thread, and don't want to mutex lock for a long function
  for (Flag *flag : flags_copy) {

    if (flag == nullptr)
      continue;
    if (flag->get_owner() != player_index)
      continue;

    //AILogDebug["do_connect_disconnected_road_networks"] << "global flag " << flag->get_index();

    // create a list containing this flag and its neighbors
    std::set<unsigned int> these_flags = { flag->get_index() };
    for (Direction dir : cycle_directions_cw()){
      Flag *other_end_flag = flag->get_other_end_flag(dir);
      bool rejected = false;
      // I am seeing other_end_flag giving a valid flag, but one that isn't actaully connected to this flag!! why???
      //  adding a backup check, and throwing a warning if they do not both agree
      if (!map->has_path_IMPROVED(flag->get_position(), dir)){
        //AILogDebug["do_connect_disconnected_road_networks"] << "map has no path in dir " << dir << " / " << NameDirection[dir] << "! rejecting";
        rejected = true;
      }else if (other_end_flag == nullptr){
        AILogWarn["do_connect_disconnected_road_networks"] << "RARE sanity check failed! other_end_flag in dir " << dir << " / " << NameDirection[dir] << " is nullptr, but map has path in dir! is this Dir4/UpLeft?  is there a building here?";
        //AILogError["do_connect_disconnected_road_networks"] << "pausing game for debugging RARE sanity checked failed other_end_flag in dir";
        //game->pause();
      }

      if (rejected)
        continue;
      unsigned int other_end_flag_index = other_end_flag->get_index();
      //AILogDebug["do_connect_disconnected_road_networks"] << "global flag has partner in dir " << dir << " / " << NameDirection[dir] << " with index " << other_end_flag_index;
      these_flags.insert(other_end_flag_index);
    }

    //for (unsigned int flag_index : these_flags){
    //  AILogDebug["do_connect_disconnected_road_networks"] << "these_flags contains " << flag_index;
    //}

    int found = -1; // the first matching group, if any match found
    for (unsigned int flag_index : these_flags){
      found = -1;
      for (int i = 0; i < network.size(); i++){
        if (network[i].count(flag_index)){
          //AILogDebug["do_connect_disconnected_road_networks"] << "found " << flag_index << " in group #" << i;
          if (found == -1){
            // this is first group found with a matching flag, insert all these_flags into group
            found = i;
            //AILogDebug["do_connect_disconnected_road_networks"] << "first group found, inserting these_flags into network[" << i << "]";
            network[i].insert(these_flags.begin(), these_flags.end());
          }else{
            //AILogDebug["do_connect_disconnected_road_networks"] << "another found, merging this group #" << i << " into first found group #" << found;
            // this is another matching group, merge it into the first group found, they must be one network
            network[found].insert(network[i].begin(), network[i].end());
            // erase the group that was merged into earlier group
            //  by swapping it to the end of the vector and then popping it off
            //AILogDebug["do_connect_disconnected_road_networks"] << "removing group #" << i;
            std::swap(network[i], network.back());
            network.pop_back();
          }
        }
      }
    }
    if (found == -1){
      //AILogDebug["do_connect_disconnected_road_networks"] << "no match, creating new group containing these_flags";
      network.push_back(these_flags);
    }

  } // foreach Flag in entire game

  //AILogDebug["do_connect_disconnected_road_networks"] << "done search, found " << network.size() << " separate flag_groups";
  for (int i = 0; i < network.size(); i++){
    //AILogDebug["do_connect_disconnected_road_networks"] << "network[" << i << "] contains " << network[i].size() << " elements";
    if (i > 0 && network[i].size() > 1){
      AILogDebug["do_connect_disconnected_road_networks"] << "DISCONNECTED ROAD SYSTEM FOUND: network[" << i << "] contains " << network[i].size() << " elements";
      Roads possible_roads = {};
      for (unsigned int flag_index : network[i]){
        Flag *flag = game->get_flag(flag_index);
        if (flag == nullptr)
          continue;
        MapPos flag_pos = flag->get_position();
        AILogDebug["do_connect_disconnected_road_networks"] << "network[" << i << "] contains flag_index " << flag_index << " with pos " << flag_pos;

        // mark it for AI overlay
        // re-use get_dir_color function with indexes and hope <6 disconnected sections! (actually it has a failsafe of 'white')
        ai_mark_pos.insert(ColorDot(flag_pos, get_dir_color_name(Direction(i))));

        //
        // attempt to reconnect it
        //
        // first, check all flags in the disconnected road network, plot best road but do not actually build it
        // then compare the solutions and try building the shortest one
        //
        Road road_solution = {};
        road_options.set(RoadOption::Improve);  // this is required I think, because these flags already have connections to each other
        // Improve is causing issues, possibly create a new option that forces a new road even if it isn't "better" because the current solution is disconnected??
        // try Direct for now
        //road_options.set(RoadOption::Direct);  // CANNOT USE DIRECT!  it tries connect directly to the INVENTORY FLAG ONLY and usually fail because that is the default affinity if no target set
        road_options.set(RoadOption::PlotOnlyNoBuild);
        road_options.set(RoadOption::ReconnectNetwork);
        // BUG - seeing a road attempt to be "reconnected" to a new-splitting-flag part of SAME NETWORK
        bool was_plotted = AI::build_best_road(flag_pos, road_options, &road_solution, "do_connect_disconnected_road_networks:Flag@"+std::to_string(flag->get_position()));
        if (was_plotted && road_solution.get_length() > 0){
          MapPos end_pos = road_solution.get_end(map.get());
          AILogDebug["do_connect_disconnected_road_networks"] << "DISCONNECTED ROAD SYSTEM: network[" << i << "], was able to plot a road from flag at pos " << flag_pos << " to end_pos " << end_pos << " with new length " << road_solution.get_length();
          if (possible_roads.size() == 0 || road_solution.get_length() < possible_roads.front().get_length()) {
            AILogDebug["do_connect_disconnected_road_networks"] << "DISCONNECTED ROAD SYSTEM: network[" << i << "], this road from flag_pos " << flag_pos << " to end_pos " << end_pos << " is new best solution";
            // insert into front of list
            possible_roads.insert(possible_roads.begin(), road_solution);
          }else{
            // push to back of list
            possible_roads.push_back(road_solution);
          }
        }else{
          AILogDebug["do_connect_disconnected_road_networks"] << "DISCONNECTED ROAD SYSTEM: network[" << i << "], was NOT able to plot a road from flag at pos " << flag_pos << " to existing road network";
        }
        road_options.reset(RoadOption::Improve);
        //road_options.reset(RoadOption::Direct);
        road_options.reset(RoadOption::PlotOnlyNoBuild);
        road_options.reset(RoadOption::ReconnectNetwork);

      }
      AILogDebug["do_connect_disconnected_road_networks"] << "DISCONNECTED ROAD SYSTEM: network[" << i << "], found " << possible_roads.size() << " possible_solutions to connect network";
      bool was_built = false;
      for (Road road : possible_roads){
        AILogDebug["do_connect_disconnected_road_networks"] << "DISCONNECTED ROAD SYSTEM: network[" << i << "], attempting to build road to connect this road_network (using build_best_road which shuold connect to nearest inventory?)";
        bool built_flag = false;
        mutex_lock("AI::do_connect_disconnected_road_networks calling game->build_road");
        if (!map->has_flag(road.get_end(map.get()))){
          built_flag = game->build_flag(road.get_end(map.get()), player);
        }
        was_built = game->build_road(road, player);
        if (was_built) {
          AILogInfo["do_connect_disconnected_road_networks"] << "DISCONNECTED ROAD SYSTEM: network[" << i << "], successfully built road to connect this road_network, from " << road.get_source() << " to " << road.get_end(map.get());
        }else{
          AILogDebug["do_connect_disconnected_road_networks"] << "DISCONNECTED ROAD SYSTEM: network[" << i << "], failed to build road! from " << road.get_source() << " to " << road.get_end(map.get()) << ", will try next one in list";
          if (built_flag){
            AILogDebug["do_connect_disconnected_road_networks"] << "DISCONNECTED ROAD SYSTEM: network[" << i << "], failed to build road! from " << road.get_source() << " to " << road.get_end(map.get()) << ", demolishing flag built for this splitting road";
            game->demolish_flag(road.get_end(map.get()), player);
          }
        }
        mutex_unlock();
        if (was_built){
          sleep_speed_adjusted(3000); 
          break;
        }
      }
      if (!was_built) {
        AILogDebug["do_connect_disconnected_road_networks"] << "DISCONNECTED ROAD SYSTEM: network[" << i << "], exhausted all solutions, failed to connect network[" << i << "]!";
      }
    }
  }
  AILogDebug["do_connect_disconnected_road_networks"] << "done";
}


void
AI::do_build_3rd_lumberjack() { 
  unsigned int sawmill_count = stock_building_counts.at(inventory_pos).count[Building::TypeSawmill];
  unsigned int lumberjack_count = stock_building_counts.at(inventory_pos).count[Building::TypeLumberjack];
  if(get_stock_inv() == nullptr)
    return;

  //unsigned int planks_count = get_stock_inv()->get_count_of(Resource::TypePlank);
  //unsigned int wood_count = get_stock_inv()->get_count_of(Resource::TypePlank) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypePlank];
  ////   include raw logs that will be processed into planks at SawMill
  //wood_count += get_stock_inv()->get_count_of(Resource::TypeLumber) + stock_res_sitting_at_flags.at(inventory_pos)[Resource::TypeLumber];
  //AILogDebug["do_build_3rd_lumberjack"] << inventory_pos << " debug current lumberjack count: " << lumberjack_count << ", sawmill_count: " << sawmill_count << ", planks_count: " << planks_count << ", planks_max: " << planks_max << " adjusted wood_count: " << wood_count;
  //if (wood_count < planks_max && sawmill_count > 0 && planks_count <= planks_max && lumberjack_count < 3) {
  if (stock_building_counts.at(inventory_pos).needs_wood == true && lumberjack_count < 3) {
    AILogDebug["do_build_3rd_lumberjack"] << inventory_pos << " wood not maxed, have sawmill, and have few than three lumberjacks, build a third";
    // count trees near military buildings,
    //   the third lumberjack doesn't need to be near sawmill, if there is a spot with many trees that is fine
    MapPosSet count_by_corner;
    for (MapPos center_pos : stock_building_counts.at(inventory_pos).occupied_military_pos) {
      MapPosVector corners = AI::get_corners(center_pos);
      for (MapPos corner_pos : corners) {
        unsigned int count = AI::count_objects_near_pos(corner_pos, AI::spiral_dist(4), Map::ObjectTree0, Map::ObjectPine7, "lt_green");
        if (count >= near_trees_min) {
          count_by_corner.insert(std::make_pair(corner_pos, count));
        }
      }
    } // foreach military building
    // build lumberjack near corner with most trees
    MapPosVector search_positions = AI::sort_by_val_desc(count_by_corner);
    MapPos built_pos = bad_map_pos;
    for (MapPos corner_pos : search_positions) {
      AILogDebug["do_build_3rd_lumberjack"] << inventory_pos << " try to build lumberjack near pos " << corner_pos;
      built_pos = AI::build_near_pos(corner_pos, AI::spiral_dist(4), Building::TypeLumberjack);
      if (built_pos != bad_map_pos && built_pos != notplaced_pos) {
        AILogDebug["do_build_3rd_lumberjack"] << inventory_pos << " built 3rd lumberjack at pos " << built_pos;
        stock_building_counts.at(inventory_pos).count[Building::TypeLumberjack]++;
        stock_building_counts.at(inventory_pos).unfinished_count++;
        return;
      }
    }
    if (built_pos == bad_map_pos || built_pos == notplaced_pos) {
      AILogDebug["do_build_3rd_lumberjack"] << inventory_pos << " couldn't place 3rd lumberjack";
    }
  } else{
    AILogDebug["do_build_3rd_lumberjack"] << inventory_pos << " have sufficient wood or wood buildings";
  }
} // 3rd lumberjack