#include <fstream>

#include "src/game-options.h"
#include "src/configfile.h"
#include "src/log.h"

#ifdef _WIN32
#include <Windows.h>
#include <Knownfolders.h>
#include <Shlobj.h>
#include <direct.h>
#elif defined(__APPLE__)
#include <dirent.h>
#include <sys/stat.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

GameOptions &
GameOptions::get_instance() {
  Log::Debug["game-options.cc"] << "inside GameOptions::get_instance()";
  static GameOptions game_options_filestore;
  return game_options_filestore;
}

GameOptions::GameOptions(){
  Log::Debug["game-options.cc"] << "inside GameOptions::GameOptions constructor";
    folder_path = ".";
    // copied from savegame.cc
    #ifdef _WIN32
    PWSTR saved_games_path;
    HRESULT res = SHGetKnownFolderPath(FOLDERID_SavedGames,
                                        KF_FLAG_CREATE | KF_FLAG_INIT,
                                        nullptr, &saved_games_path);
    int len = static_cast<int>(wcslen(saved_games_path));
    folder_path.resize(len);
    ::WideCharToMultiByte(CP_ACP, 0, saved_games_path, len,
                            (LPSTR)folder_path.data(), len, nullptr, nullptr);
    ::CoTaskMemFree(saved_games_path);
    folder_path += '\\';
    #elif defined(__APPLE__)
    folder_path = std::string(std::getenv("HOME"));
    folder_path += "/Library/Application Support";
    folder_path += '/';
    #else
    folder_path = std::string(std::getenv("HOME"));
    //folder_path += "/.local/share";
    folder_path += "/.local/share/forkserf";
    folder_path += '/';
    #endif

/*
    //folder_path += "freeserf";
    folder_path += "forkserf";
    if (!is_folder_exists(folder_path)) {
        if (!create_folder(folder_path)) {
        throw ExceptionFreeserf("Failed to create folder");
        }
    }

    #ifndef _WIN32
    folder_path += "/options";
    if (!is_folder_exists(folder_path)) {
        if (!create_folder(folder_path)) {
        throw ExceptionFreeserf("Failed to create folder");
        }
    }
    #endif
    */

  path = folder_path;
  filename = "forkserf_options.ini";
}

void
GameOptions::load_options_from_file(){
  //
  // NOTE - all letters in config files are evaluated as lowercase
  //  so it is okay if they are upper or lower case in the config file
  //  but all EVALUATIONS here must use lower case version to match
  //  or they will not find it
  //
  Log::Debug["game-options.cc"] << "inside GameOptions::load_options_from_file";
  meta_main = std::make_shared<ConfigFile>();
  if (!meta_main->load(path + "/" + filename)) {
    Log::Warn["game-options.cc"] << "inside GameOptions::load_options_from_file, failed to load!" << path << "/" << filename;
  }else{
    Log::Debug["game-options.cc"] << "inside GameOptions::load_options_from_file, successfully loaded file " << path << "/" << filename;
  }
    /*
    bool option_EnableAutoSave = false;
    //bool option_ImprovedPigFarms = false;  // removing this as it turns out the default behavior for pig farms is to require almost no grain
    bool option_CanTransportSerfsInBoats = false;
    bool option_QuickDemoEmptyBuildSites = false;
    //bool option_AdvancedDemolition = true;  // this needs more playtesting  
    bool option_TreesReproduce = false;
    bool option_BabyTreesMatureSlowly = false;
    bool option_ResourceRequestsTimeOut = true;  // this is forced true to indicate that the code to make them optional isn't added yet
    bool option_PrioritizeUsableResources = true;    // this is forced true to indicate that the code to make them optional isn't added yet
    bool option_LostTransportersClearFaster = false;
    bool option_FourSeasons = false;
    bool option_AdvancedFarming = false;
    bool option_FishSpawnSlowly = false;
    bool option_FogOfWar = false;
    //bool option_EastSlopesShadeObjects = true;   // make this an option, maybe
    bool option_InvertMouse = false;
    bool option_InvertWheelZoom = false;
    bool option_SpecialClickBoth = true;
    bool option_SpecialClickMiddle = true;
    bool option_SpecialClickDouble = true;
    bool option_SailorsMoveFaster = false;
    bool option_WaterDepthLuminosity = false;
    bool option_RandomizeInstruments = false;  // only affects DOS music
    */
  option_FogOfWar = meta_main->value("options", "fogofwar", option_FogOfWar);
  option_EnableAutoSave = meta_main->value("options", "enableautosave", option_EnableAutoSave);
  option_CanTransportSerfsInBoats = meta_main->value("options", "cantransportserfsinboats", option_CanTransportSerfsInBoats);
  option_QuickDemoEmptyBuildSites = meta_main->value("options", "quickdemoemptybuildsites", option_QuickDemoEmptyBuildSites);
  option_TreesReproduce = meta_main->value("options", "treesreproduce", option_TreesReproduce);
  option_BabyTreesMatureSlowly = meta_main->value("options", "babytreesmatureslowly", option_BabyTreesMatureSlowly);
  option_LostTransportersClearFaster = meta_main->value("options", "losttransportersclearfaster", option_LostTransportersClearFaster);
  option_FourSeasons = meta_main->value("options", "fourseasons", option_FourSeasons);
  option_AdvancedFarming = meta_main->value("options", "advancedfarming", option_AdvancedFarming);
  option_FishSpawnSlowly = meta_main->value("options", "fishspawnslowly", option_FishSpawnSlowly);
  option_InvertMouse = meta_main->value("options", "invertmouse", option_InvertMouse);
  option_InvertWheelZoom = meta_main->value("options", "invertwheelzoom", option_InvertWheelZoom);
  option_SpecialClickBoth = meta_main->value("options", "specialclickboth", option_SpecialClickBoth);
  option_SpecialClickMiddle = meta_main->value("options", "specialclickmiddle", option_SpecialClickMiddle);
  option_SpecialClickDouble = meta_main->value("options", "specialclickdouble", option_SpecialClickDouble);
  option_SailorsMoveFaster = meta_main->value("options", "sailorsmovefaster", option_SailorsMoveFaster);
  option_WaterDepthLuminosity = meta_main->value("options", "waterdepthluminosity", option_WaterDepthLuminosity);
  option_RandomizeInstruments = meta_main->value("options", "randomizeinstruments", option_RandomizeInstruments);
  option_ForesterMonoculture = meta_main->value("options", "forestermonoculture", option_ForesterMonoculture);

  mapgen_size = meta_main->value("mapgen", "size", mapgen_size);
  mapgen_trees = meta_main->value("mapgen", "trees", mapgen_trees);
  mapgen_stonepile_dense = meta_main->value("mapgen", "stonepiledense", mapgen_stonepile_dense);
  mapgen_stonepile_sparse = meta_main->value("mapgen", "stonepilesparse", mapgen_stonepile_sparse);
  mapgen_fish = meta_main->value("mapgen", "fish", mapgen_fish);
  mapgen_mountain_gold = meta_main->value("mapgen", "mountaingold", mapgen_mountain_gold);
  mapgen_mountain_iron = meta_main->value("mapgen", "mountainiron", mapgen_mountain_iron);
  mapgen_mountain_coal = meta_main->value("mapgen", "mountaincoal", mapgen_mountain_coal);
  mapgen_mountain_stone = meta_main->value("mapgen", "mountainstone", mapgen_mountain_stone);
  mapgen_desert_frequency = meta_main->value("mapgen", "desertfrequency", mapgen_desert_frequency);
  mapgen_water_level = meta_main->value("mapgen", "waterlevel", mapgen_water_level);
  mapgen_junk_grass_sandstone = meta_main->value("mapgen", "junkgrasssandstone", mapgen_junk_grass_sandstone);
  mapgen_junk_grass_small_boulders = meta_main->value("mapgen", "junkgrasssmallboulders", mapgen_junk_grass_small_boulders);
  mapgen_junk_grass_stub_trees = meta_main->value("mapgen", "junkgrassstubtrees", mapgen_junk_grass_stub_trees);
  mapgen_junk_grass_dead_trees = meta_main->value("mapgen", "junkgrassdeadtrees", mapgen_junk_grass_dead_trees);
  mapgen_junk_water_boulders = meta_main->value("mapgen", "junkwaterboulders", mapgen_junk_water_boulders);
  mapgen_junk_water_trees = meta_main->value("mapgen", "junkwatertrees", mapgen_junk_water_trees);
  mapgen_junk_desert_cadavers = meta_main->value("mapgen", "junkdesertcadavers", mapgen_junk_desert_cadavers);
  mapgen_junk_desert_cacti = meta_main->value("mapgen", "junkdesertcacti", mapgen_junk_desert_cacti);
  mapgen_junk_desert_palm_trees = meta_main->value("mapgen", "junkdesertpalmtrees", mapgen_junk_desert_palm_trees);

}

void
GameOptions::save_options_to_file(){
  //Log::Debug["game-options.cc"] << "inside GameOptions::save_options_to_file";
  std::ofstream file(path+"/"+filename);
  if (!file.is_open()) {
    Log::Warn["game-options.cc"] << "inside GameOptions::save_options_to_file, failed to open file for writing!" << path << "/" << filename;
    return;
  }else{
    Log::Debug["game-options.cc"] << "inside GameOptions::save_options_to_file, successfully opened file for writing " << path << "/" << filename;
  }
  file << "[options]" << "\n";
  file << "FogOfWar=" << option_FogOfWar << "\n";
  file << "EnableAutoSave=" << option_EnableAutoSave << "\n";
  file << "CanTransportSerfsInBoats=" << option_CanTransportSerfsInBoats << "\n";
  file << "QuickDemoEmptyBuildSites=" << option_QuickDemoEmptyBuildSites << "\n";
  file << "TreesReproduce=" << option_TreesReproduce << "\n";
  file << "BabyTreesMatureSlowly=" << option_BabyTreesMatureSlowly << "\n";
  file << "LostTransportersClearFaster=" << option_LostTransportersClearFaster << "\n";
  file << "FourSeasons=" << option_FourSeasons << "\n";
  file << "AdvancedFarming=" << option_AdvancedFarming << "\n";
  file << "FishSpawnSlowly=" << option_FishSpawnSlowly << "\n";
  file << "InvertMouse=" << option_InvertMouse << "\n";
  file << "InvertWheelZoom=" << option_InvertWheelZoom << "\n";
  file << "SpecialClickBoth=" << option_SpecialClickBoth << "\n";
  file << "SpecialClickMiddle=" << option_SpecialClickMiddle << "\n";
  file << "SpecialClickDouble=" << option_SpecialClickDouble << "\n";
  file << "SailorsMoveFaster=" << option_SailorsMoveFaster << "\n";
  file << "WaterDepthLuminosity=" << option_WaterDepthLuminosity << "\n";
  file << "RandomizeInstruments=" << option_RandomizeInstruments << "\n";
  file << "option_ForesterMonoculture=" << option_ForesterMonoculture << "\n";

 /*
  case ACTION_RESET_MAPGEN_DEFAULTS:
    // reasonable values for trees are 0.00-4.00, so divide max slider 65500 by 4 to get 16375 and let 1.00 == 16375
    interface->set_custom_map_generator_trees(uint16_t(16375 * 1.00));
    interface->set_custom_map_generator_stonepile_dense(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_stonepile_sparse(slider_double_to_uint16(1.00)); 
    //interface->set_custom_map_generator_fish(slider_double_to_uint16(1.00)); 
    // reasonable values for fish are 0.00-4.00, so divide max slider 65500 by 4 to get 16375 and let 1.00 == 16375
    interface->set_custom_map_generator_fish(uint16_t(16375 * 1.00));
    interface->set_custom_map_generator_mountain_gold(slider_mineral_double_to_uint16(2.00));   // 2
    interface->set_custom_map_generator_mountain_iron(slider_mineral_double_to_uint16(4.00));   // 4
    interface->set_custom_map_generator_mountain_coal(slider_mineral_double_to_uint16(9.00));   // 9
    interface->set_custom_map_generator_mountain_stone(slider_mineral_double_to_uint16(2.00));  // 2
    interface->set_custom_map_generator_desert_frequency(slider_double_to_uint16(1.00)); 
    //interface->set_custom_map_generator_lakes_size(slider_double_to_uint16(1.00)); 
    //interface->set_custom_map_generator_lakes_water_level(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_lakes_water_level(uint16_t(8188 * 1.00)); 
    interface->set_custom_map_generator_junk_grass_sandstone(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_junk_grass_small_boulders(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_junk_grass_stub_trees(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_junk_grass_dead_trees(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_junk_water_boulders(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_junk_water_trees(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_junk_desert_cadavers(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_junk_desert_cacti(slider_double_to_uint16(1.00)); 
    interface->set_custom_map_generator_junk_desert_palm_trees(slider_double_to_uint16(1.00));
    */

  file << "" << "\n";

  // save custom map generator settings AND map size
  file << "[mapgen]" << "\n";
  file << "Size=" << mapgen_size << "\n";
  file << "Trees=" << mapgen_trees << "\n";
  file << "StonepileDense=" << mapgen_stonepile_dense << "\n";
  file << "StonepileSparse=" << mapgen_stonepile_sparse << "\n";
  file << "Fish=" << mapgen_fish << "\n";
  file << "MountainGold=" << mapgen_mountain_gold << "\n";
  file << "MountainIron=" << mapgen_mountain_iron << "\n";
  file << "MountainCoal=" << mapgen_mountain_coal << "\n";
  file << "MountainStone=" << mapgen_mountain_stone << "\n";
  file << "DesertFrequency=" << mapgen_desert_frequency << "\n";
  file << "WaterLevel=" << mapgen_water_level << "\n";
  file << "JunkGrassSandstone=" << mapgen_junk_grass_sandstone << "\n";
  file << "JunkGrassSmallBoulders=" << mapgen_junk_grass_small_boulders << "\n";
  file << "JunkGrassStubTrees=" << mapgen_junk_grass_stub_trees << "\n";
  file << "JunkGrassDeadTrees=" << mapgen_junk_grass_dead_trees << "\n";
  file << "JunkWaterBoulders=" << mapgen_junk_water_boulders << "\n";
  file << "JunkWaterTrees=" << mapgen_junk_water_trees << "\n";
  file << "JunkDesertCadavers=" << mapgen_junk_desert_cadavers << "\n";
  file << "JunkDesertCacti=" << mapgen_junk_desert_cacti << "\n";
  file << "JunkDesertPalmTrees=" << mapgen_junk_desert_palm_trees << "\n";

  Log::Debug["game-options.cc"] << "inside GameOptions::save_options_to_file, done saving options";
}