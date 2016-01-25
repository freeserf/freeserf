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

#include <ctime>
#include <string>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
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

#ifndef DEFAULT_LOG_LEVEL
# ifndef NDEBUG
#  define DEFAULT_LOG_LEVEL  LOG_LEVEL_DEBUG
# else
#  define DEFAULT_LOG_LEVEL  LOG_LEVEL_INFO
# endif
#endif

/* Autosave interval */
#define AUTOSAVE_INTERVAL  (10*60*TICKS_PER_SEC)

/* In target, replace any character from needle with replacement character. */
static void
strreplace(char *target, const char *needle, char replace) {
  for (int i = 0; target[i] != '\0'; i++) {
    for (int j = 0; needle[j] != '\0'; j++) {
      if (needle[j] == target[i]) {
        target[i] = replace;
        break;
      }
    }
  }
}

int
save_game(int autosave) {
  size_t r;

  /* Build filename including time stamp. */
  char name[128];
  std::time_t t = time(NULL);

  struct tm *tm = std::localtime(&t);
  if (tm == NULL) return -1;

  if (!autosave) {
    r = strftime(name, sizeof(name), "%c.save", tm);
    if (r == 0) return -1;
  } else {
    r = strftime(name, sizeof(name), "autosave-%c.save", tm);
    if (r == 0) return -1;
  }

  /* Substitute problematic characters. These are problematic
     particularly on windows platforms, but also in general on FAT
     filesystems through any platform. */
  /* TODO Possibly use PathCleanupSpec() when building for windows platform. */
  strreplace(name, "\\/:*?\"<>| ", '_');

  FILE *f = fopen(name, "wb");
  if (f == NULL) return -1;

  int r1 = save_text_state(f);
  if (r1 < 0) return -1;

  fclose(f);

  LOGI("main", "Game saved to `%s'.", name);

  return 0;
}

class game_event_handler_t : public event_handler_t {
 public:
  virtual bool handle_event(const event_t *event) {
    switch (event->type) {
      case EVENT_UPDATE:
        game_update();
        return true;
        break;
      default:
        break;
    }
    return false;
  }
};

#define USAGE                                               \
  "Usage: %s [-g DATA-FILE]\n"
#define HELP                                                \
  USAGE                                                     \
      " -d NUM\t\tSet debug output level\n"                 \
      " -f\t\tFullscreen mode (CTRL-q to exit)\n"           \
      " -g DATA-FILE\tUse specified data file\n"            \
      " -h\t\tShow this help text\n"                        \
      " -l FILE\tLoad saved game\n"                         \
      " -r RES\t\tSet display resolution (e.g. 800x600)\n"  \
      " -t GEN\t\tMap generator (0 or 1)\n"                 \
      "\n"                                                  \
      "Please report bugs to <" PACKAGE_BUGREPORT ">\n"

int
main(int argc, char *argv[]) {
  std::string data_file;
  std::string save_file;

  int screen_width = DEFAULT_SCREEN_WIDTH;
  int screen_height = DEFAULT_SCREEN_HEIGHT;
  bool fullscreen = false;
  int map_generator = 0;

  init_missions();

  log_level_t log_level = DEFAULT_LOG_LEVEL;

#ifdef HAVE_GETOPT_H
  while (true) {
    char opt = getopt(argc, argv, "d:fg:hl:r:t:");
    if (opt < 0) break;

    switch (opt) {
      case 'd': {
          int d = atoi(optarg);
          if (d >= 0 && d < LOG_LEVEL_MAX) {
            log_level = static_cast<log_level_t>(d);
          }
        }
        break;
      case 'f':
        fullscreen = true;
        break;
      case 'g':
        if (strlen(optarg) > 0) {
          data_file = optarg;
        }
        break;
      case 'h':
        fprintf(stdout, HELP, argv[0]);
        exit(EXIT_SUCCESS);
        break;
      case 'l':
        if (strlen(optarg) > 0) {
          save_file = optarg;
        }
        break;
      case 'r': {
          char *hstr = strchr(optarg, 'x');
          if (hstr == NULL) {
            fprintf(stderr, USAGE, argv[0]);
            exit(EXIT_FAILURE);
          }
          screen_width = atoi(optarg);
          screen_height = atoi(hstr+1);
        }
        break;
      case 't':
        map_generator = atoi(optarg);
        break;
      default:
        fprintf(stderr, USAGE, argv[0]);
        exit(EXIT_FAILURE);
        break;
    }
  }
#endif

  /* Set up logging */
  log_set_file(stdout);
  log_set_level(log_level);

  LOGI("main", "freeserf %s", FREESERF_VERSION);

  data_t *data = data_t::get_instance();
  if (!data->load(data_file)) {
    delete data;
    LOGE("main", "Could not load game data.");
    exit(EXIT_FAILURE);
  }

  LOGI("main", "Initialize graphics...");

  gfx_t *gfx = NULL;
  try {
    gfx = gfx_t::get_instance();
    gfx->set_resolution(screen_width, screen_height, fullscreen);
  } catch (Freeserf_Exception &e) {
    LOGE(e.get_system(), e.what());
    return -1;
  }

  /* TODO move to right place */
  audio_t *audio = audio_t::get_instance();
  audio_volume_controller_t *volume_controller = audio->get_volume_controller();
  if (volume_controller != NULL) {
    volume_controller->set_volume(.75f);
  }
  audio_player_t *player = audio->get_music_player();
  if (player) {
    player->play_track(MIDI_TRACK_0);
  }

  game.map_generator = map_generator;

  game_init();

  /* Initialize interface */
  interface_t *interface = new interface_t();
  interface->set_size(screen_width, screen_height);
  interface->set_displayed(true);

  /* Either load a save game if specified or
     start a new game. */
  if (!save_file.empty()) {
    int r = game_load_save_game(save_file.c_str());
    if (r < 0) exit(EXIT_FAILURE);

    interface->set_player(0);
  } else {
    int r = game_load_random_map(3, interface->get_random());
    if (r < 0) exit(EXIT_FAILURE);

    /* Add default player */
    r = game_add_player(12, 64, 40, 40, 40);
    if (r < 0) exit(EXIT_FAILURE);

    interface->set_player(r);
    interface->open_game_init();
  }

  interface->game_reset();

  /* Init game loop */
  game_event_handler_t *handler = new game_event_handler_t();
  event_loop_t *event_loop = event_loop_t::get_instance();
  event_loop->add_handler(handler);
  event_loop->add_handler(interface);

  /* Start game loop */
  event_loop->run();

  LOGI("main", "Cleaning up...");

  /* Clean up */
  delete interface;
  map_deinit();
  delete audio;
  delete gfx;
  delete data;
  delete event_loop;

  return EXIT_SUCCESS;
}
