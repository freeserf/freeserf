/*
 * freeserf.cc - Main program source.
 *
 * Copyright (C) 2013-2018  Jon Lund Steffensen <jonlst@gmail.com>
 *
 * This file is part of freeserf.
 *
 * freeserf is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * freeserf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with freeserf.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "src/freeserf.h"

#include <string>
#include <iostream>
// #include <fstream>

#include "src/log.h"
#include "src/version.h"
#include "src/data.h"
#include "src/audio.h"
#include "src/gfx.h"
#include "src/interface.h"
#include "src/game-manager.h"
#include "src/command_line.h"

#ifdef WIN32
# include <SDL.h>
#endif  // WIN32



int
main(int argc, char *argv[]) {

  // why does simply calling new(Log) make console output work on windows???
  new(Log);
  std::ofstream* filestr = new std::ofstream("console_out.txt");
  Log::set_file(filestr);


  std::string data_dir;
  std::string save_file;

  unsigned int screen_width = 0;
  unsigned int screen_height = 0;
  bool fullscreen = false;

  CommandLine command_line;
  command_line.add_option('d', "Set Debug output level")
                .add_parameter("NUM", [](std::istream& s) {
                  int d;
                  s >> d;
                  if (d >= 0 && d < Log::LevelMax) {
                    Log::set_level(static_cast<Log::Level>(d));
                  }
                  return true;
                });
  command_line.add_option('f', "Run in Fullscreen mode",
                          [&fullscreen](){ fullscreen = true; });
  command_line.add_option('g', "Use specified data directory")
                .add_parameter("DATA-PATH", [&data_dir](std::istream& s) {
                  s >> data_dir;
                  return true;
                });
  command_line.add_option('h', "Show this help text", [&command_line](){
                  command_line.show_help();
                  exit(EXIT_SUCCESS);
                });
  command_line.add_option('l', "Load saved game")
                .add_parameter("FILE", [&save_file](std::istream& s) {
                  std::getline(s, save_file);
                  return true;
                });
  command_line.add_option('r', "Set display resolution (e.g. 800x600)")
                .add_parameter("RES",
                              [&screen_width, &screen_height](std::istream& s) {
                  s >> screen_width;
                  char c; s >> c;
                  s >> screen_height;
                  return true;
                });
  //command_line.set_comment("Please report bugs to <" PACKAGE_BUGREPORT ">");
  if (!command_line.process(argc, argv)) {
    return EXIT_FAILURE;
  }

  //Log::Info["main"] << "freeserf " << FREESERF_VERSION;
  Log::Info["freeserf.cc"] << "forkserf " << FORKSERF_VERSION;

  Data &data = Data::get_instance();
  if (!data.load(data_dir)) {
    Log::Error["main"] << "Could not load game data.";
    //p1plp1_throw_exception_win32_if_missing_SPA_data
    #ifdef WIN32
     int msgboxID = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "FreeSerf", "missing SPA*.PA", NULL);
    #endif
    return EXIT_FAILURE;
  }

  Log::Info["main"] << "Initialize graphics...";

  Graphics &gfx = Graphics::get_instance();

  /* TODO move to right place */
  Audio &audio = Audio::get_instance();  // 
  Audio::PPlayer player = audio.get_music_player();  // TEMP DISABLING AUDIO WHILE RUNNING DEBUGGER BECAUSE OF ANNOYING RANDOM CRASH!
  // NOTE - it seems Amiga has a single long track, 
  //  while DOS has three or four similar tracks that play one after another
  if (player) {
    Log::Debug["freeserf.cc"] << "inside freeserf main(), starting audio midi music, about to call player->play_track";
    //Audio::PTrack t = player->play_track(Audio::TypeMidiTrack0);
    Data &data = Data::get_instance();
    if (data.get_data_source_Amiga() != nullptr && data.get_data_source_DOS() != nullptr){
      // if BOTH Amiga and DOS music available, pick one at random
      if (rand() % 2){
        Log::Info["freeserf.cc"] << "inside freeserf main(), both Amiga and DOS music available, randomly chose Amiga";
        Audio::PTrack t = player->play_track(Audio::TypeMidiTrack0, DataSourceType::Amiga);  // 0=Amiga, 1=DOS, 2=Custom
      }else{
        Log::Info["freeserf.cc"] << "inside freeserf main(), both Amiga and DOS music available, randomly chose DOS";
        Audio::PTrack t = player->play_track(Audio::TypeMidiTrack0, DataSourceType::DOS);  // 0=Amiga, 1=DOS, 2=Custom
      }
    }else if(data.get_data_source_Amiga() != nullptr){
      Log::Info["freeserf.cc"] << "inside freeserf main(), starting audio midi music, found only Amiga music, using it";
      Audio::PTrack t = player->play_track(Audio::TypeMidiTrack0, DataSourceType::Amiga);  // 0=Amiga, 1=DOS, 2=Custom
    }else if(data.get_data_source_DOS() != nullptr){
      Log::Info["freeserf.cc"] << "inside freeserf main(), starting audio midi music, found only DOS music, using it";
      Audio::PTrack t = player->play_track(Audio::TypeMidiTrack0, DataSourceType::DOS);  // 0=Amiga, 1=DOS, 2=Custom
    }else{
      // music not available
      Log::Error["freeserf.cc"] << "inside freeserf main(), neither Amiga nor DOS music found, this should not happen as game should not even start without either"; 
    }

    // testing the audio tracks. I understand DOS has four short tracks, Amiga has one very long one
    // it doesn't seem to matter what track# is played for Amiga, it always plays the only track
    //Audio::PTrack t = player->play_track(Audio::TypeMidiTrack3, DataSourceType::Amiga);  // 0=Amiga, 1=DOS, 2=Custom
    // DOS MUSIC DOES NOT WORK IN LINUX AT ALL - at least not for me, but Amiga works fine in linux
    //Audio::PTrack t = player->play_track(Audio::TypeMidiTrack0, DataSourceType::DOS);  // 0=Amiga, 1=DOS, 2=Custom
  }

  GameManager &game_manager = GameManager::get_instance();

  /* Either load a save game if specified or
     start a new game. */
  if (!save_file.empty()) {
    if (!game_manager.load_game(save_file)) {
      return EXIT_FAILURE;
    }
  } else {
    if (!game_manager.start_random_game()) {
      return EXIT_FAILURE;
    }
  }

  /* Initialize interface */
  Interface interface;
  if ((screen_width == 0) || (screen_height == 0)) {
    gfx.get_resolution(&screen_width, &screen_height);
  }
  interface.set_size(screen_width, screen_height);
  interface.set_displayed(true);

  /* Initialize AI */
  bool loaded_game_start_ai = false;
  if (save_file.empty()) {
    interface.open_game_init();
  }
  else {
    loaded_game_start_ai = true;
  }

  /* Init game loop */
  EventLoop &event_loop = EventLoop::get_instance();
  event_loop.add_handler(&interface);

  /* Initialize AI on load_game */
  if (loaded_game_start_ai) {
    interface.initialize_AI();
  }

  /* Start game loop */
  event_loop.run();

  event_loop.del_handler(&interface);

  Log::Info["main"] << "Cleaning up...";

  return EXIT_SUCCESS;
}
