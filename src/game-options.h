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
extern bool option_AdvancedDemolition;
extern bool option_TreesReproduce;
extern bool option_BabyTreesMatureSlowly;
extern bool option_ResourceRequestsTimeOut;
extern bool option_PrioritizeUsableResources;
extern bool option_LostTransportersClearFaster;
extern bool option_FourSeasons;
extern bool option_FishSpawnSlowly;
extern bool option_FogOfWar;
extern int season;
extern int last_season;
extern int subseason;
extern int last_subseason;
extern int season_sprite_offset[4];

#endif  // SRC_GAME_OPTIONS_H_