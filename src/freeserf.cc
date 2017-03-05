/*
 * freeserf.cc - Main program source.
 *
 * Copyright (C) 2013-2014  Jon Lund Steffensen <jonlst@gmail.com>
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
#include <cstdint>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#include "src/log.h"
#include "src/savegame.h"
#include "src/mission.h"
#include "src/version.h"
#include "src/game.h"
#include "src/data.h"
#include "src/audio.h"
#include "src/gfx.h"
#include "src/video-sdl.h"
#include "src/event_loop.h"
#include "src/interface.h"

#define DEFAULT_SCREEN_WIDTH  800
#define DEFAULT_SCREEN_HEIGHT 600

#define USAGE                                               \
  "Usage: %s [-g DATA-FILE]\n"
#define HELP                                                \
  USAGE                                                     \
      " -d NUM\t\tSet debug output level\n"                 \
      " -f\t\tFullscreen mode (CTRL-q to exit)\n"           \
      " -g DATA-FILE\tUse specified data directory\n"       \
      " -h\t\tShow this help text\n"                        \
      " -l FILE\tLoad saved game\n"                         \
      " -r RES\t\tSet display resolution (e.g. 800x600)\n"  \
      "\n"                                                  \
      "Please report bugs to <" PACKAGE_BUGREPORT ">\n"

int
main(int argc, char *argv[]) {
  std::string data_dir;
  std::string save_file;

  int screen_width = DEFAULT_SCREEN_WIDTH;
  int screen_height = DEFAULT_SCREEN_HEIGHT;
  bool fullscreen = false;

#ifdef HAVE_GETOPT_H
  while (true) {
    int opt = getopt(argc, argv, "d:fg:hl:r:");
    if (opt < 0) break;

    switch (opt) {
      case 'd': {
          int d = atoi(optarg);
          if (d >= 0 && d < Log::LevelMax) {
            Log::set_level(static_cast<Log::Level>(d));
          }
        }
        break;
      case 'f':
        fullscreen = true;
        break;
      case 'g':
        if (strlen(optarg) > 0) {
          data_dir = optarg;
        }
        break;
      case 'h':
        fprintf(stdout, HELP, argv[0]);
        return EXIT_SUCCESS;
      case 'l':
        if (strlen(optarg) > 0) {
          save_file = optarg;
        }
        break;
      case 'r': {
          char *hstr = strchr(optarg, 'x');
          if (hstr == NULL) {
            fprintf(stderr, USAGE, argv[0]);
            return EXIT_FAILURE;
          }
          screen_width = atoi(optarg);
          screen_height = atoi(hstr+1);
        }
        break;
      default:
        fprintf(stderr, USAGE, argv[0]);
        return EXIT_FAILURE;
    }
  }
#endif

  Log::Info["main"] << "freeserf " << FREESERF_VERSION;

  Data *data = Data::get_instance();
  if (!data->load(&data_dir)) {
    Log::Error["main"] << "Could not load game data.";
    return EXIT_FAILURE;
  }

  Log::Info["main"] << "Initialize graphics...";

  Graphics *gfx = NULL;
  try {
    gfx = Graphics::get_instance();
    gfx->set_resolution(screen_width, screen_height, fullscreen);
  } catch (ExceptionFreeserf &e) {
    Log::Error[e.get_system().c_str()] << e.what();
    return EXIT_FAILURE;
  }

  /* TODO move to right place */
  Audio *audio = Audio::get_instance();
  Audio::VolumeController *volume_controller = audio->get_volume_controller();
  if (volume_controller != NULL) {
    volume_controller->set_volume(.75f);
  }
  Audio::Player *player = audio->get_music_player();
  if (player) {
    player->play_track(Audio::TypeMidiTrack0);
  }

  Game *game = new Game();

  /* Either load a save game if specified or
     start a new game. */
  if (!save_file.empty()) {
    if (!GameStore::get_instance()->load(save_file, game)) {
      return EXIT_FAILURE;
    }
  } else {
    PGameInfo game_info(new GameInfo(Random()));
    if (!game->load_mission_map(game_info)) {
      return EXIT_FAILURE;
    }
  }

  /* Initialize interface */
  Interface *interface = new Interface();
  interface->set_size(screen_width, screen_height);
  interface->set_displayed(true);
  interface->set_game(game);
  interface->set_player(0);

  if (save_file.empty()) {
    interface->open_game_init();
  }

  /* Init game loop */
  EventLoop *event_loop = EventLoop::get_instance();
  event_loop->add_handler(game);
  event_loop->add_handler(interface);

  /* Start game loop */
  event_loop->run();

  event_loop->del_handler(interface);
  event_loop->del_handler(game);

  Log::Info["main"] << "Cleaning up...";

  /* Clean up */
  game = interface->get_game();
  delete interface;
  if (game != NULL) {
    EventLoop::get_instance()->del_handler(game);
    delete game;
    game = NULL;
  }
  delete audio;
  delete gfx;
  delete event_loop;

  return EXIT_SUCCESS;
}
