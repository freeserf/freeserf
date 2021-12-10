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
#include "src/serf.h"    // for serf->is_waiting check for stuck serfs

#include "src/ai_roadbuilder.h"  // additional pathfinder functions for AI
#include "src/lookup.h"  // for console log, has text names for enums and such

// used for build_near_pos loops, the number of MapPos in the fill pattern
#define DIRECTIONAL_FILL_POS_MAX  52

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
  //FlagDirToDirPathMap *ai_mark_arterial_road_paths = (new FlagDirToDirPathMap);    // used to highlight discovered arterial roads for AI overlay for debugging, ordered list of Dirs of entire path
  FlagDirToFlagVectorMap *ai_mark_arterial_road_flags = (new FlagDirToFlagVectorMap);    // used to highlight discovered arterial roads for AI overlay for debugging, unordered(?) list of flags
  FlagDirToFlagDirVectorMap *ai_mark_arterial_road_pairs = (new FlagDirToFlagDirVectorMap);    // used to highlight discovered arterial roads for AI overlay for debugging, unordered list of flag_pos-Dirs pairs
  //MapPosDirVector *ai_mark_spiderweb_road_pairs = (new MapPosDirVector);   // used to highlight spiderweb-built roads for AI overlay for debugging.  
  Roads *ai_mark_spiderweb_roads = (new Roads);   // used to highlight spiderweb-built roads for AI overlay for debugging.  
  Roads *ai_mark_build_better_roads = (new Roads);   // used to highlight build_better-built roads for AI overlay for debugging.  
  std::set<std::string> expand_towards;
  std::set<std::string> last_expand_towards;  // quick hack to save a copy for attack scoring
  //MapPos stopbuilding_pos;
  //bool stop_building;
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
  std::set<MapPos> new_stocks = {};  // AI STATEFULNESS WARNING - this is used to detect when a stock transitions from completed-but-not-occupied to occupied, and will not trigger if game saved/loaded between
  // Now that multiple economies implemented I think the entire XXX_building_counts are worthless
  //   remove the entire concept and instead just search for nearby buildings
  int realm_building_count[25] = {0};
  int realm_completed_building_count[25] = {0};
  //int realm_incomplete_building_count[25] = {0};
  int realm_occupied_building_count[25] = {0};
  //int realm_connected_building_count[25] = {0};

  int change_buffer;
  int previous_knight_occupation_level;
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
  AI(PGame, unsigned int);
  void start();
  void next_loop();
  ColorDotMap * get_ai_mark_pos() { return &ai_mark_pos; }
  std::vector<int> * get_ai_mark_serf() { return &ai_mark_serf; }
  Road * get_ai_mark_road() { return ai_mark_road; }
  // a "pseudo-Road" ordered list of Dirs from each Inventory flag-dir that follows
  //  along the determined arterial flags in the dir, tracing each TILE-path
  FlagDirToFlagDirVectorMap * get_ai_mark_arterial_road_pairs() { return ai_mark_arterial_road_pairs; }
  // the unordered list of arterial flags for each Inventory flag-dir
  FlagDirToFlagVectorMap * get_ai_mark_arterial_road_flags() { return ai_mark_arterial_road_flags; }
  //MapPosDirVector * get_ai_mark_spiderweb_road_pairs() { return ai_mark_spiderweb_road_pairs; }
  Roads* get_ai_mark_spiderweb_roads() { return ai_mark_spiderweb_roads;}
  Roads* get_ai_mark_build_better_roads() { return ai_mark_build_better_roads;}
  Color get_mark_color(std::string color) { return colors.at(color); }
  Color get_random_mark_color() {
    auto it = colors.begin();
    std::advance(it, rand() % colors.size());
    std::string random_key = it->first;
    return colors.at(random_key);
  }
  Color get_dir_color(Direction dir) {
    switch (dir) {
      case DirectionNone:           // undef, should we throw error?
        return colors.at("black");

      case DirectionRight:          // 0 / East
        return colors.at("red");

      case DirectionDownRight:      // 1 / SouthEast
        return colors.at("yellow");

      case DirectionDown:           // 2 / SouthWest
        return colors.at("blue");

      case DirectionLeft:           // 3 / West
        return colors.at("orange");

      case DirectionUpLeft:         // 4 / NorthWest
        return colors.at("purple");

      case DirectionUp:             // 5 / NorthEast
        return colors.at("green");
        
      default:                      // some bad dir, should we throw error?
        return colors.at("white");
    }
  }

  std::string get_dir_color_name(Direction dir) {
    switch (dir) {
      case DirectionNone:           // undef, should we throw error?
        return "black";

      case DirectionRight:          // 0 / East
        return "red";

      case DirectionDownRight:      // 1 / SouthEast
        return "yellow";

      case DirectionDown:           // 2 / SouthWest
        return "blue";

      case DirectionLeft:           // 3 / West
        return "orange";

      case DirectionUpLeft:         // 4 / NorthWest
        return "purple";

      case DirectionUp:             // 5 / NorthEast
        return "green";
        
      default:                      // some bad dir, should we throw error?
        return "white";
    }
  }
  std::string get_ai_status() { return ai_status; }
  // stupid way to pass game speed and AI loop count to viewport for AI overlay
  unsigned int get_game_speed() { return game->get_game_speed(); }
  unsigned int get_loop_count() { return loop_count; }
  unsigned int ai_loop_freq_adj_for_gamespeed(unsigned int freq) { 
    // because certain AI functions only run every X loops,
    //  they do occur at the right frequency when the game
    //  speed is increased.  To handle this, take the "every X loops"
    //  and reduce it as game speed is increased, return the adjusted number
    switch (game->get_game_speed()) {
      case 0 ... 5:
        return freq;
      case 6 ... 15:
        return freq*.5;
      case 16 ... 40:
        return freq*.2;
      default:
        return freq*.2;
    }
  }
  void sleep_speed_adjusted(int msec){
    // sleep for specified millisec if speed is normal '2'
    // adjust sleep speed to be less as game speed increases
    int speed = game->get_game_speed();
    double msec_ = msec;
    if (speed > 2){
      msec_ = msec_ * 1/(speed - 1);
    }
    //AILogDebug["sleep_speed_adjusted"] << "msec: " << msec << ", game speed: " << speed << ", adjusted msec: " << int(msec_);
    msec = msec_;
    std::this_thread::sleep_for(std::chrono::milliseconds(msec));
  }
  std::set<std::string> get_ai_expansion_goals() { return expand_towards; }
  MapPos get_ai_inventory_pos() { return inventory_pos; }
  void set_serf_lost();
  
 protected:
  //
 private:
  //
  // ai_util.cc
  //
  static bool has_terrain_type(PGame, MapPos, Map::Terrain, Map::Terrain);  // why does this need to be static?
  bool place_castle(PGame, MapPos, unsigned int);
  static unsigned int spiral_dist(int);   // why does this need to be static?
  //void rebuild_all_roads();  // no longer need this
  // changing these to support *planning* a road without actually building it, prior to placing a new building
  //bool build_best_road(MapPos, RoadOptions, Building::Type optional_affinity = Building::TypeNone, MapPos optional_target = bad_map_pos);
  //bool build_best_road(MapPos, RoadOptions, Building::Type optional_affinity_building_type = Building::TypeNone, Building::Type optional_affinity = Building::TypeNone, MapPos optional_target = bad_map_pos);
  // adding verify_stock checking
  //bool build_best_road(MapPos, RoadOptions, Building::Type optional_affinity_building_type = Building::TypeNone, Building::Type optional_affinity = Building::TypeNone, MapPos optional_target = bad_map_pos, bool verify_stock = false);
  // adding store the built road in a Road pointer
  bool build_best_road(MapPos, RoadOptions, Road *road_built, 
        std::string calling_function = "",
        Building::Type optional_affinity_building_type = Building::TypeNone,
        Building::Type optional_affinity = Building::TypeNone, 
        MapPos optional_target = bad_map_pos, 
        bool verify_stock = false);
  //MapPosVector get_affinity(MapPos);
  MapPosVector get_affinity(MapPos, Building::Type optional_affinity_building_type = Building::TypeNone);
  /*
  // make these wrappers aroud a single function?  or single function with args?
  Building* find_nearest_building(MapPos, unsigned int, Building::Type);
  Building* find_nearest_connected_building(MapPos, unsigned int, Building::Type);
  Building* find_nearest_completed_building(MapPos, unsigned int, Building::Type);
  */
  Building* find_nearest_building(MapPos, CompletionLevel, Building::Type, unsigned int max_dist = -1);
  Road trace_existing_road(PMap, MapPos, Direction);
  MapPosVector get_corners(MapPos);
  MapPosVector get_corners(MapPos, unsigned int distance);
  Direction get_dir_from_corner(MapPos center_pos, MapPos corner_pos);
  unsigned int count_terrain_near_pos(MapPos, unsigned int, Map::Terrain, Map::Terrain, std::string);
  unsigned int count_empty_terrain_near_pos(MapPos, unsigned int, Map::Terrain, Map::Terrain, std::string);
  unsigned int count_farmable_land(MapPos, unsigned int, std::string);
  unsigned int count_objects_near_pos(MapPos, unsigned int, Map::Object, Map::Object, std::string);
  double count_geologist_sign_density(MapPos, unsigned int);
  MapPosVector sort_by_val_asc(MapPosSet);
  MapPosVector sort_by_val_desc(MapPosSet);
  //MapPos build_near_pos(MapPos, unsigned int, Building::Type);
  MapPos build_near_pos(MapPos, unsigned int, Building::Type, Direction optional_fill_dir = DirectionNone);
  bool building_exists_near_pos(MapPos, unsigned int, Building::Type);
  //MapPos find_halfway_pos_between_buildings(Building::Type, Building::Type);
  unsigned int count_stones_near_pos(MapPos, unsigned int);
  unsigned int count_knights_affected_by_occupation_level_change(unsigned int, unsigned int);
  void expand_borders();
  unsigned int score_area(MapPos, unsigned int);  
  bool is_bad_building_pos(MapPos, Building::Type);
  void update_building_counts();
  void update_stocks_pos();
  MapPos get_halfway_pos(MapPos, MapPos);
  ResourceMap realm_inv;
  MapPos inventory_pos;
  // MapPos find_nearest_inventory(MapPos);   // deprecating this and replacing with find_nearest_inventory_for_res_producer()
  //MapPos find_nearest_inventory(MapPos);  // nevermind, rewriting it but keeping same function name and input/output type
  // moving this to ai_pathfinder.cc and totally changing it to use the ai_pathfinder pyrdacor style FlagSearch that does not 
  //  require the use of flag->search_xxx vars that are not threadsafe
  //MapPos find_nearest_inventory(PMap map, unsigned int player_index, MapPos pos, ColorDotMap *ai_mark_pos);
  bool scoring_attack;
  bool scoring_warehouse;
  //bool cannot_expand_borders_this_loop;
  void score_enemy_targets(MapPosSet*);
  void attack_nearest_target(MapPosSet*);

  //
  // I do not think this is needed anymore now that multiple economies
  //  implemented??
  //
  struct StockBuilding {
    int count[25] = { 0 };
    int connected_count[25] = { 0 };
    int completed_count[25] = { 0 };
    int occupied_count[25] = { 0 };
    int unfinished_count;
    int unfinished_hut_count;
    bool needs_wood = false;
    bool needs_stone = false;
    bool needs_foods = false;
    bool needs_steel = false;
    bool needs_coal = false;
    bool needs_iron_ore = false;
    bool needs_gold_ore = false;
    MapPosVector occupied_military_pos;
  };
  std::map<MapPos, StockBuilding> stock_buildings;
  std::map<MapPos, ResourceMap> stock_res_sitting_at_flags;
  ResourceMap realm_res_sitting_at_flags;

  //
  // ai.cc
  //
  void do_place_castle();
  void do_get_inventory(MapPos);
  void do_update_clear_reset();
  void do_get_serfs();
  void do_debug_building_triggers();
  void do_promote_serfs_to_knights();
  void do_connect_disconnected_flags();
  void do_spiderweb_roads();
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
  void do_demolish_excess_food_buildings();
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
  void do_count_resources_sitting_at_flags(MapPos);
  bool do_can_build_knight_huts();
  bool do_can_build_other();
  void do_check_resource_needs();
  void do_create_star_roads_for_new_warehouses();
  void do_connect_disconnected_road_networks();

  //
  // ai_pathfinder.cc
  //
  // adding support for HoldBuildingPos
  //Road plot_road(PMap map, unsigned int player_index, MapPos start, MapPos end, Roads * const &potential_roads);
  Road plot_road(PMap map, unsigned int player_index, MapPos start, MapPos end, Roads * const &potential_roads, bool hold_building_pos = false);
  int get_straightline_tile_dist(PMap map, MapPos start_pos, MapPos end_pos);
  bool score_flag(PMap map, unsigned int player_index, RoadBuilder *rb, RoadOptions road_options, MapPos flag_pos, MapPos castle_flag_pos, ColorDotMap *ai_mark_pos);
  bool find_flag_path_and_tile_dist_between_flags(PMap map, MapPos start_pos, MapPos target_pos, MapPosVector *solution_flags, unsigned int *tile_dist, ColorDotMap *ai_mark_pos);
  MapPos find_nearest_inventory(PMap map, unsigned int player_index, MapPos flag_pos, DistType dist_type, ColorDotMap *ai_mark_pos);
  MapPos find_nearest_inventory_by_straightline(PMap map, unsigned int player_index, MapPos flag_pos, ColorDotMap *ai_mark_pos);
  MapPos find_nearest_inventory_by_flag(PMap map, unsigned int player_index, MapPos flag_pos, ColorDotMap *ai_mark_pos);
  MapPosVector find_nearest_inventories_to_military_building(MapPos pos);
  RoadEnds get_roadends(PMap map, Road road);
  Road reverse_road(PMap map, Road road);
  void identify_arterial_roads(PMap);
  void arterial_road_depth_first_recursive_flagsearch(MapPos, std::pair<MapPos,Direction>, MapPosVector *, int *);
};

//
// these were originally in AI class, protected
//
// moving these to after the class declaration works to fix g++ linker errors, but why?
//   info here:  https://stackoverflow.com/questions/272900/undefined-reference-to-static-class-member
//  these compiled fine on visual studio community 2017 on windows
//  maybe this is because of g++ switch to C++14 and not the compiler?
static const int serfs_min = 5;  // don't convert serfs to knights below this value
 // the ${resource}_max values below are modified by this buffer to avoid constantly flipping between not enough and too many
 // when determining if AI should gather more resources, the buffer is subtracted from resource_max so it stops building early
 // when determining if AI should demolish resource producing buildings, the buffer is added to resource_max so it delays demo
 // so, far example with a buffer of 5 and resource_max of 30:
 //  resource production buildings will be placed if res < 25
 //  no changes made if res count 25 - 35
 //  resource production buildings will be destroyed if res > 35
 //  if stored res count drops below 25 again it will build res buildings

// the anti_flapping_buffer is added to the xxxx_max value when "destroy excess producer buildings" calls are made
//  so that there is a period where new res producer buildings are not created, but existing ones are not destroyed
static const unsigned int anti_flapping_buffer = 8;
static const unsigned int knights_min = 3;
static const unsigned int knights_med = 18;
static const unsigned int knights_max = 50;
static const unsigned int knight_occupation_change_buffer = 4; // to avoid repeatedly cycling knights, increase/lower bar to change levels again by this amount
static const unsigned int near_building_sites_min = 250;   // don't place castle unless this many sites available.  small += 1, large += 3
static const unsigned int gold_bars_max = 50;  // does this do anything?  maybe use to deprioritize food to gold mines over this amount?
static const unsigned int steel_min = 8;   // don't build blacksmith if under this value, unless sufficient iron or an iron mine
static const unsigned int steel_max = 60;  // don't build iron foundry if over this value
static const unsigned int planks_min = 8; // if planks below this value, don't build any other buildings until sawmill and lumberjacks done.  Also, don't build warehouse/stocks if below this
static const unsigned int planks_max = 40; // build a sawmill & lumberjack if planks count goes below this value, destroy all but one lumberjack (per stock) if over this number
static const unsigned int near_trees_min = 5; // only place sawmills near at least this many trees; place rangers near sawmills with fewer than this many trees.  Also for castle placement (WEIGHTED 4x)
static const unsigned int stones_min = 10;
static const unsigned int stones_max = 25;
static const unsigned int near_stones_min = 5;  // don't place castle unless sufficient stones, considers pile size
static const unsigned int food_max = 35;  // demolish all food buildings if stored food over this amount (includes food at flags and unprocessed food up to a certain cap)
static const unsigned int min_openspace_farm = 45; // min open tiles in area to build farm (existing fields count favorably, though). FYI: there are 60 tiles in spiral_dist(4)
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
static const unsigned int geologists_max = 2; // UPDATE this is now not a fixed value at all, it is now: 2+(geologists_max*number of Inventories (castle/stock))

// deprioritize sending geologists to area where signs density is over this amount (prefer send geologists to unevaluated areas)
static constexpr double geologist_sign_density_deprio = 0.40;
// never send geologists to a flag that has a sign_density over this amount
static constexpr double geologist_sign_density_max = 0.65;

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

/* temporarily increasing these to effectively eliminate cap
  hoping that mines will be placed in all good spots, but not connected
  until actually needed
// don't send geologists (or maybe reduce rate) if this many mines already placed
//   also don't expand borders for mines if >= max?
// Feb 2021 - reducing these to reduce clutter, they never all get used to their potential anyway 
static const unsigned int max_coalmines = 2;   // was 3
static const unsigned int max_ironmines = 1;   // was 2
static const unsigned int max_goldmines = 1;
*/
static const unsigned int max_coalmines = 99;
static const unsigned int max_ironmines = 99;
static const unsigned int max_goldmines = 99;

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
static const unsigned int contains_castle_flag_penalty = 20;  //increased this from 10 to 20 on dec04 2021


#endif // SRC_AI_H_

