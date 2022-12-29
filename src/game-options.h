#ifndef SRC_GAME_OPTIONS_H_
#define SRC_GAME_OPTIONS_H_

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
extern int season;
extern int last_season;
extern int subseason;
extern int last_subseason;
extern int season_sprite_offset[4];
// because SDL Event Loop can only notify game/interface but cannot query it
//  this is needed to determine mousewheel behavior, so it can either zoom the viewport
//  or scroll the load games window
extern bool is_list_in_focus;

#endif  // SRC_GAME_OPTIONS_H_