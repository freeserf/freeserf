#ifndef SRC_GAME_OPTIONS_H_
#define SRC_GAME_OPTIONS_H_

// these variables are deCLAREd here, and this header is to be included in any
//  code file that needs to use them
// these variables are deFINEd (and initialized?) in game.cc (for now) which is an arbitrary 
//  choice because it could be done in any file that includes this header, I think.
extern bool option_EnableAutoSave;
extern bool option_ImprovedPigFarms;
extern bool option_CanTransportSerfsInBoats;
extern bool option_QuickDemoEmptyBuildSites;
extern bool option_TreesReproduce;
extern bool option_BabyTreesMatureSlowly;
extern bool option_ResourceRequestsTimeOut;  // these are forced true to indicate that the code to make them optional isn't added yet
extern bool option_LostTransportersClearFaster;  // these are forced true to indicate that the code to make them optional isn't added yet

#endif  // SRC_GAME_OPTIONS_H_