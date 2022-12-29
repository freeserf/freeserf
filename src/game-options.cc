#include <fstream>

#include "src/game-options.h"
#include "src/configfile.h"
#include "src/log.h"

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
  option_FishSpawnSlowly = meta_main->value("options", "fishspawnslowly", option_FishSpawnSlowly);
  option_InvertMouse = meta_main->value("options", "invertmouse", option_InvertMouse);
  option_InvertWheelZoom = meta_main->value("options", "invertwheelzoom", option_InvertWheelZoom);
  option_SpecialClickBoth = meta_main->value("options", "specialclickboth", option_SpecialClickBoth);
  option_SpecialClickMiddle = meta_main->value("options", "specialclickmiddle", option_SpecialClickMiddle);
  option_SpecialClickDouble = meta_main->value("options", "specialclickdouble", option_SpecialClickDouble);
  option_SailorsMoveFaster = meta_main->value("options", "sailorsmovefaster", option_SailorsMoveFaster);
  option_WaterDepthLuminosity = meta_main->value("options", "waterdepthluminosity", option_WaterDepthLuminosity);
  option_RandomizeInstruments = meta_main->value("options", "randomizeinstruments", option_RandomizeInstruments);
  //
  // need to add map generator options here also
  //
  // maybe save map size also?
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
  file << "FishSpawnSlowly=" << option_FishSpawnSlowly << "\n";
  file << "InvertMouse=" << option_InvertMouse << "\n";
  file << "InvertWheelZoom=" << option_InvertWheelZoom << "\n";
  file << "SpecialClickBoth=" << option_SpecialClickBoth << "\n";
  file << "SpecialClickMiddle=" << option_SpecialClickMiddle << "\n";
  file << "SpecialClickDouble=" << option_SpecialClickDouble << "\n";
  file << "SailorsMoveFaster=" << option_SailorsMoveFaster << "\n";
  file << "WaterDepthLuminosity=" << option_WaterDepthLuminosity << "\n";
  file << "RandomizeInstruments=" << option_RandomizeInstruments << "\n";
  Log::Debug["game-options.cc"] << "inside GameOptions::save_options_to_file, done saving options";
}