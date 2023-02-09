#ifndef SRC_GAME_OPTIONS_H_
#define SRC_GAME_OPTIONS_H_

#include "src/configfile.h"
//#include "src/savegame.h"  // DO NOT INCLUDE THIS IT BREAKS THINGS //for SaveGameWriter to write options to file

class GameOptions {
 public:
  void load_options_from_file();
  void save_options_to_file();
  GameOptions();
  static GameOptions &get_instance();
 protected:
  PConfigFile meta_main;
  std::string folder_path;
  std::string path;
  std::string filename;
};

// these variables are deCLAREd here, and this header is to be included in any
//  code file that needs to use them
// these variables are deFINEd (and initialized?) in game.cc (for now) which is an arbitrary 
//  choice because it could be done in any file that includes this header, I think.
extern bool option_EnableAutoSave;
//extern bool option_ImprovedPigFarms;  // removing this as it turns out the default behavior for pig farms is to require almost no grain
extern bool option_CanTransportSerfsInBoats;
extern bool option_QuickDemoEmptyBuildSites;
//extern bool option_AdvancedDemolition;    /* removing AdvancedDemolition for now, see https://github.com/forkserf/forkserf/issues/180*/
extern bool option_TreesReproduce;
extern bool option_BabyTreesMatureSlowly;
extern bool option_ResourceRequestsTimeOut;
extern bool option_PrioritizeUsableResources;
extern bool option_LostTransportersClearFaster;
extern bool option_FourSeasons;
extern bool option_AdvancedFarming;
extern bool option_FishSpawnSlowly;
extern bool option_FogOfWar;
extern bool option_InvertMouse;
extern bool option_InvertWheelZoom;
extern bool option_SpecialClickBoth;
extern bool option_SpecialClickMiddle;
extern bool option_SpecialClickDouble;
extern bool option_SailorsMoveFaster;
extern bool option_WaterDepthLuminosity;
extern bool option_RandomizeInstruments;  // only affects DOS music
extern bool option_ForesterMonoculture;
extern bool option_CheckPathBeforeAttack;  // this is forced on
extern bool option_SpinningAmigaStar;
extern bool option_HighMinerFoodConsumption;

extern unsigned int mapgen_size;
extern uint16_t mapgen_trees;
extern uint16_t mapgen_stonepile_dense;
extern uint16_t mapgen_stonepile_sparse;
extern uint16_t mapgen_fish;
extern uint16_t mapgen_mountain_gold;
extern uint16_t mapgen_mountain_iron;
extern uint16_t mapgen_mountain_coal;
extern uint16_t mapgen_mountain_stone;
extern uint16_t mapgen_desert_frequency;
extern uint16_t mapgen_water_level;
extern uint16_t mapgen_junk_grass_sandstone;
extern uint16_t mapgen_junk_grass_small_boulders;
extern uint16_t mapgen_junk_grass_stub_trees;
extern uint16_t mapgen_junk_grass_dead_trees;
extern uint16_t mapgen_junk_water_boulders;
extern uint16_t mapgen_junk_water_trees;
extern uint16_t mapgen_junk_desert_cadavers;
extern uint16_t mapgen_junk_desert_cacti;
extern uint16_t mapgen_junk_desert_palm_trees;
extern uint16_t mapgen_junk_water_reeds_cattails;

extern int season;
extern int last_season;
extern int subseason;
extern int last_subseason;
extern int season_sprite_offset[4];
// because SDL Event Loop can only notify game/interface but cannot query it
//  this is needed to determine mousewheel behavior, so it can either zoom the viewport
//  or scroll the load games window
extern bool is_list_in_focus;
extern bool is_dragging_popup;
//extern bool is_dragging_viewport_or_minimap;
extern int mouse_x_after_drag;
extern int mouse_y_after_drag;

#endif  // SRC_GAME_OPTIONS_H_