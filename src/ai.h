/*
 * ai.h - AI main class and AI related functions
 *   Copyright 2019-2021 tlongstretch
 */

#ifndef SRC_AI_H_
#define SRC_AI_H_

#include <map>
#include <set>
#include <string>        // to satisfy cpplinter
#include <utility>       // to satisfy cpplinter
#include <vector>        // to satisfy cpplinter
#include <limits>        // to satisfy cpplinter
#include <chrono>        //NOLINT (build/c++11) this is a Google Chromium req, not relevant to general C++.  // for thread sleeps
#include <thread>        //NOLINT (build/c++11) this is a Google Chromium req, not relevant to general C++.  // for AI threads
#include <bitset>        // for RoadOptions option-bit manipulation
#include <fstream>       // for writing individual console log for each AI player
#include <ctime>         // for timing function call runs
#include <cstring>       // for memset

#include "src/audio.h"   // for audio notifications
#include "src/flag.h"    // for flag->call_transporter for
#include "src/game.h"
#include "src/gfx.h"     // for AI overlay, needed to get Color class, maybe find a simpler way?
#include "src/log.h"     // for separate AI logger
#include "src/savegame.h"   // for auto-saving
#include "src/serf.h"    // for serf->is_waiting check for stuck serfs

#include "src/ai_roadbuilder.h"  // additional pathfinder functions for AI
#include "src/lookup.h"  // for console log, has text names for enums and such


class AI {
    //
 public:
  typedef std::map<std::pair<unsigned int, Direction>, unsigned int> FlagDirTimer;
  typedef std::map<unsigned int, unsigned int> SerfWaitTimer;
  const MapPos notplaced_pos = std::numeric_limits<unsigned int>::max() - 1;
  std::string name;          // 'Player1', 'Player2', etc.  used to tag log lines

 private:
  std::ofstream ai_ofstream; // each AI thread logs to its own file
  PGame game;
  PMap map;
  Player *player;
  AIPlusOptions aiplus_options;  // bitfield of bools for AIPlus game options, passed in from Interface::initialize_AI
  //Flags *flags;   // don't use a pointer to game.flags, not thread safe   ?  is this still used?  oct28 2020
  //Flags flags;    // instead use a copy that is created before each foreach Flag loop   ?  is this still used?  oct28 2020
  Flags *flags;        //or maybe just create a copy and move the pointer to point to that new copy instead?? is that easier than changing all the foreach Flag loops?   ?  is this still used?  oct28 2020
  Flags flags_static_copy;  // store the copy here each time it is fetched from game->get_flags    ?  is this still used?  oct28 2020
  unsigned int loop_count;
  unsigned int player_index;
  std::string ai_status;        // used to describe what AI is doing when AI overlay is on (top-left corner of screen)
  unsigned int unfinished_building_count;
  unsigned int unfinished_hut_count;
  ColorDotMap ai_mark_pos;      // used to mark spots on map with various colored dots.  For debugging, when AI overlay is on
  std::vector<int> ai_mark_serf;    // used to mark serfs on map with status text.  For debugging, when AI overlay is on
  Road *ai_mark_road = (new Road);  // used to trace roads on map as pathfinding runs.  For debugging, when AI overlay is on
  std::set<std::string> expand_towards;
  std::set<std::string> last_expand_towards;  // quick hack to save a copy for attack scoring
  MapPos stopbuilding_pos;
  MapPos castle_pos;
  MapPos castle_flag_pos;
  FlagDirTimer no_transporter_timers;
  SerfWaitTimer serf_wait_timers;
  SerfWaitTimer serf_wait_idle_on_road_timers;
  MapPosVector realm_occupied_military_pos;
  Building *castle;
  Inventory *stock_inv;
  RoadOptions road_options;
  Serf::SerfMap serfs_idle;
  Serf::SerfMap serfs_potential;
  int *serfs_total;
  bool need_tools;
  int building_count[25] = {0};
  int completed_building_count[25] = {0};
  //int incomplete_building_count[25] = {0};
  int occupied_building_count[25] = {0};
  int connected_building_count[25] = {0};
  int change_buffer;
  int previous_knight_occupation_level;
  bool has_autosave_mutex = false;
  // list of bad building positions (where buildings had to be demolished for certain reasons)
  // AI STATEFULNESS WARNING - this bad_building_pos list is lost on save/load or if AI thread terminated
  MapPosSet bad_building_pos;
  std::vector<MapPos> stocks_pos; // positions of stocks - CASTLE and warehouses
  Log::Logger AILogVerbose{ Log::LevelVerbose, "Verbose" };
  Log::Logger AILogDebug{ Log::LevelDebug, "Debug" };
  Log::Logger AILogInfo{ Log::LevelInfo, "Info" };
  Log::Logger AILogWarn{ Log::LevelWarn, "Warn" };
  Log::Logger AILogError{ Log::LevelError, "Error" };


 protected:
  // all the tuning variables were moved outside the class to satisfy gcc/g++ which threw compile errors when they were here
  // VS2017 had no issue with it

 public:
  AI(PGame, unsigned int, AIPlusOptions);
  void start();
  void next_loop();
  ColorDotMap * get_ai_mark_pos() { return &ai_mark_pos; }
  std::vector<int> * get_ai_mark_serf() { return &ai_mark_serf; }
  Road * get_ai_mark_road() { return ai_mark_road; }
  Color get_mark_color(std::string color) { return colors.at(color); }
  std::string get_ai_status() { return ai_status; }
  // stupid way to pass game speed and AI loop count to viewport for AI overlay
  unsigned int get_game_speed() { return game->get_game_speed(); }
  unsigned int get_loop_count() { return loop_count; }
  std::set<std::string> get_ai_expansion_goals() { return expand_towards; }

 protected:
  //
 private:
  //
  // ai_util.cc
  //
  static bool has_terrain_type(PGame, MapPos, Map::Terrain, Map::Terrain);  // why does this need to be static?
  bool place_castle(PGame, MapPos, unsigned int);
  static unsigned int spiral_dist(int);   // why does this need to be static?
  void rebuild_all_roads();
  bool build_best_road(MapPos, RoadOptions, Building::Type optional_affinity = Building::TypeNone, MapPos optional_target = bad_map_pos);
  MapPosVector get_affinity(MapPos);
  Building* find_nearest_building(MapPos, unsigned int, Building::Type);
  Building* find_nearest_completed_building(MapPos, unsigned int, Building::Type);
  Road trace_existing_road(PMap, MapPos, Direction);
  MapPosVector get_corners(MapPos);
  MapPosVector get_corners(MapPos, unsigned int distance);
  unsigned int count_terrain_near_pos(MapPos, unsigned int, Map::Terrain, Map::Terrain, std::string);
  unsigned int count_empty_terrain_near_pos(MapPos, unsigned int, Map::Terrain, Map::Terrain, std::string);
  unsigned int count_farmable_land(MapPos, unsigned int, std::string);
  unsigned int count_objects_near_pos(MapPos, unsigned int, Map::Object, Map::Object, std::string);
  double count_geologist_sign_density(MapPos, unsigned int);
  MapPosVector sort_by_val_asc(MapPosSet);
  MapPosVector sort_by_val_desc(MapPosSet);
  MapPos build_near_pos(MapPos, unsigned int, Building::Type);
  bool building_exists_near_pos(MapPos, unsigned int, Building::Type);
  MapPos find_halfway_pos_between_buildings(Building::Type, Building::Type);
  unsigned int count_stones_near_pos(MapPos, unsigned int);
  unsigned int count_knights_affected_by_occupation_level_change(unsigned int, unsigned int);
  MapPos expand_borders(MapPos);
  unsigned int score_area(MapPos, unsigned int);
  bool is_bad_building_pos(MapPos, Building::Type);
  void update_building_counts();
  void update_stocks_pos();
  MapPos get_halfway_pos(MapPos, MapPos);
  ResourceMap realm_inv;
  MapPos stock_pos;
  MapPos find_nearest_stock(MapPos);
  bool scoring_attack;
  bool scoring_warehouse;
  bool cannot_expand_borders_this_loop;
  void score_enemy_targets(MapPosSet*);
  void attack_nearest_target(MapPosSet*);

  struct StockBuildings {
    int count[25] = { 0 };
    int connected_count[25] = { 0 };
    int completed_count[25] = { 0 };
    int occupied_count[25] = { 0 };
    int unfinished_count;
    int unfinished_hut_count;
    MapPosVector occupied_military_pos;
  };
  std::map<MapPos, StockBuildings> stock_buildings;

  //
  // ai.cc
  //
  void do_place_castle();
  void do_get_inventory(MapPos);
  void do_save_game();
  void do_update_clear_reset();
  void do_get_serfs();
  void do_debug_building_triggers();
  void do_promote_serfs_to_knights();
  void do_connect_disconnected_flags();
  void do_spiderweb_roads1();
  void do_spiderweb_roads2();
  void do_pollute_castle_area_roads_with_flags();
  void do_build_better_roads_for_important_buildings();
  void do_fix_stuck_serfs();
  void do_fix_missing_transporters();
  void do_send_geologists();
  void do_build_rangers();
  void do_remove_road_stubs();
  void do_demolish_unproductive_3rd_lumberjacks();
  void do_demolish_unproductive_stonecutters();
  void do_demolish_unproductive_mines();
  void do_demolish_excess_lumberjacks();
  void do_demolish_excess_fishermen();
  void do_manage_tool_priorities();
  void do_manage_mine_food_priorities();
  void do_balance_sword_shield_priorities();
  void do_attack();
  void do_manage_knight_occupation_levels();
  void do_place_mines(std::string, Building::Type, Map::Object, Map::Object, int, double);
  void do_place_coal_mines(); //wrapper around do_place_mines
  void do_place_iron_mines(); //wrapper around do_place_mines
  void do_place_gold_mines(); //wrapper around do_place_mines
  void do_build_sawmill_lumberjacks();
  bool do_wait_until_sawmill_lumberjacks_built();
  void do_build_stonecutter();
  void do_create_defensive_buffer();
  void do_build_toolmaker_steelsmelter();
  void do_build_food_buildings_and_3rd_lumberjack();
  void do_connect_coal_mines();
  void do_connect_iron_mines();
  void do_build_steelsmelter();
  void do_build_blacksmith();
  void do_build_gold_smelter_and_connect_gold_mines();
  void do_build_warehouse();

  //
  // ai_pathfinder.cc
  //
  Road plot_road(PMap map, unsigned int player_index, MapPos start, MapPos end, Roads * const &potential_roads);
  int get_straightline_tile_dist(PMap map, MapPos start_pos, MapPos end_pos);
  bool score_flag(PMap map, unsigned int player_index, RoadBuilder *rb, RoadOptions road_options, MapPos flag_pos, MapPos castle_flag_pos, ColorDotMap *ai_mark_pos);
  bool find_flag_and_tile_dist(PMap map, unsigned int player_index, RoadBuilder *rb, MapPos flag_pos, MapPos castle_flag_pos, ColorDotMap *ai_mark_pos);
  RoadEnds get_roadends(PMap map, Road road);
  Road reverse_road(PMap map, Road road);
};

//
// these were originally in AI class, protected
//
// moving these to after the class declaration works to fix g++ linker errors, but why?
//   info here:  https://stackoverflow.com/questions/272900/undefined-reference-to-static-class-member
//  these compiled fine on visual studio community 2017 on windows
//  maybe this is because of g++ switch to C++14 and not the compiler?
static const int serfs_min = 5;  // don't convert serfs to knights below this value
static const unsigned int knights_min = 3;
static const unsigned int knights_med = 18;
static const unsigned int knights_max = 50;
static const unsigned int knight_occupation_change_buffer = 4; // to avoid repeatedly cycling knights, increase/lower bar to change levels again by this amount
static const unsigned int near_building_sites_min = 250;   // don't place castle unless this many sites available.  small += 1, large += 3
static const unsigned int gold_bars_max = 50;  // does this do anything?  maybe use to deprioritize food to gold mines over this amount?
static const unsigned int steel_min = 8;   // don't build blacksmith if under this value, unless sufficient iron or an iron mine
static const unsigned int steel_max = 60;  // don't build iron foundry if over this value
static const unsigned int planks_crit = 5; // don't build anything other than wood buildings if under this value
static const unsigned int planks_min = 24; // if planks below this value, don't build any other buildings until sawmill and lumberjacks done.  Also, don't build warehouse/stocks if below this
static const unsigned int planks_max = 40; // build a sawmill & lumberjack if planks count goes below this value, destroy all but one lumberjack (per stock) if over this number
static const unsigned int near_trees_min = 4; // only place sawmills near at least this many trees; place rangers near sawmills with fewer than this many trees.  Also for castle placement (WEIGHTED 4x)
static const unsigned int stones_min = 10;
static const unsigned int stones_max = 25;
static const unsigned int near_stones_min = 5;  // don't place castle unless sufficient stones, considers pile size
static const unsigned int food_max = 35;  // demolish all fisherman if stored food over this amount
static const unsigned int min_openspace_farm = 25; // min open tiles in area to build farm (existing fields count favorably, though). FYI: there are 60 tiles in spiral_dist(4)
static const unsigned int near_fields_min = 3; // don't build mill and baker until a farm has this man fields already sown
static const unsigned int coal_min = 12;   // don't build blacksmith if under this value and no coal mine.  Also, de-prioritize coal miner's food supply if over this value
static const unsigned int coal_max = 60;   // don't build coal mine if over this value, don't give food to mine over this value
static const unsigned int iron_ore_min = 8; // don't build blacksmith if under this value and no iron mine, OR sufficient steel.  Also, de-prioritize iron miner's food supply if over this value
static const unsigned int iron_ore_max = 30; // don't build iron mine if over this value, don't give food to mine over this value
static const unsigned int gold_ore_min = 8;    // don't build gold smelter if under this value and no gold min.  Also, de-prioritize gold miner's food supply if over this value
static const unsigned int gold_ore_max = 40;  // don't build gold mine over this value (is this implemented?),   don't give food to mine over this value
static const unsigned int hills_min = 9;   // don't send geologists unless substantial hills
static const unsigned int waters_min = 24;  // don't build fisherman unless substantial waters
static const unsigned int hammers_min = 6; // don't create geologists unless this many hammers in reserve
static const unsigned int geologists_max = 4; // try not to create more geologists if have this many, hard to tell if they are out working

// deprioritize sending geologists to area where signs density is over this amount (prefer send geologists to unevaluated areas)
static constexpr double geologist_sign_density_deprio = 0.40;
// never send geologists to a flag that has a sign_density over this amount
static constexpr double geologist_sign_density_max = 0.70;

// don't build mines on Small resource signs until this ratio of potential resource signs are placed.
//    until this % of signs placed, only build if you find a Large resource sign
static constexpr double coal_sign_density_min = 0.50;
static constexpr double iron_sign_density_min = 0.50;
static constexpr double gold_sign_density_min = 0.30;  // lower min for gold mines because even a small flag is usually enough gold, and gold is rare

//  because new mines start at zero productivity, but any mine that is active and/or has food stored is not *brand new* and can be tested
static const unsigned int mine_output_min = 8; // burn if below this % production

// may need to do something to account for the fact that minerals come in clusters
//    because once a few signs are found to indicate a cluster, the true value doesn't change
//    but the AI function valuation will increase as more signs are found for the known mineral cluster
//       maybe only add valuation if no adjacent signs for same mineral?
static const unsigned int foods_weight = 2;
static const unsigned int trees_weight = 2;
static const unsigned int stones_weight = 2;
static const unsigned int stone_signs_weight = 1;
static const unsigned int hills_weight = 2;
static const unsigned int iron_ore_weight = 3;
static const unsigned int coal_weight = 2;
static const unsigned int gold_ore_weight = 5;

// don't build anything new if at max.
//     TODO: need to be 100% sure unfinished buildings don't get stuck and halt AI progress
//      separated huts vs other buildings because we don't ever want to stop one type entirely
// lumberjack & sawmill ignore this limit (and maybe stonecutter too?)
static const unsigned int max_unfinished_buildings = 2;
static const unsigned int max_unfinished_huts = 2;

// don't send geologists (or maybe reduce rate) if this many mines already placed
//   also don't expand borders for mines if >= max?
static const unsigned int max_coalmines = 3;
static const unsigned int max_ironmines = 2;
static const unsigned int max_goldmines = 1;

// max ratio of actual road length compared to ideal straight-line length to determine if road is acceptably short
//   example, 3.00 means a road of up to 3x the length of a perfectly straight road is acceptable
//   this length INCLUDES ANY PENALTIES (for flags encountered, AvoidCastle, PenalizeCastleFlag, etc.),
//     whereas the ideal_length has no penalties
static constexpr double max_convolution = 3.00;

// attack if own knight_morale is at least this high (starting morale is 1024?)
static const unsigned int min_knight_morale_attack = 1400;
// attack if knights sent is at least this many times the defending knights
//  ex. if enemy hut has 2 defenders, and this is set to 4.00, only attack if 8 knights can be sent
static constexpr double min_knight_ratio_attack = 2.00;
// TODO add some kind of factor that reduces the required attack ratio as own morale increases

// fixed penalty for a non-direct road that contains the castle flag (but doesn't start/end there)
static const unsigned int contains_castle_flag_penalty = 10;


#endif // SRC_AI_H_

