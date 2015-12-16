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

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <string>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include "src/event_loop-sdl.h"
#include "src/interface.h"
#include "src/misc.h"

BEGIN_EXT_C
  #include "src/gfx.h"
  #include "src/data.h"
  #include "src/sdl-video.h"
  #include "src/log.h"
  #include "src/audio.h"
  #include "src/savegame.h"
  #include "src/mission.h"
  #include "src/version.h"
END_EXT_C

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

static interface_t interface;

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

static int
save_game(int autosave) {
  size_t r;

  /* Build filename including time stamp. */
  char name[128];
  time_t t = time(NULL);

  struct tm *tm = localtime(&t);
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

static event_loop_t *event_loop = NULL;

void
game_loop_quit() {
  if (event_loop != NULL) {
    event_loop->quit();
  }
}

class game_event_handler_t : public event_handler_t {
 public:
  virtual bool handle_event(const event_t *event) {
    gui_event_t ev;

    switch (event->type) {
      case EVENT_TYPE_CLICK:
        ev.type = GUI_EVENT_TYPE_CLICK;
        ev.x = event->x;
        ev.y = event->y;
        ev.button = event->button;
        gui_object_handle_event(
          reinterpret_cast<gui_object_t*>(&interface), &ev);
        break;
      case EVENT_TYPE_DBL_CLICK:
        ev.type = GUI_EVENT_TYPE_DBL_CLICK;
        ev.x = event->x;
        ev.y = event->y;
        ev.button = event->button;
        gui_object_handle_event(
          reinterpret_cast<gui_object_t *>(&interface), &ev);
        break;
      case EVENT_TYPE_DRAG:
        ev.type = GUI_EVENT_TYPE_DRAG_MOVE;
        ev.x = event->dx;
        ev.y = event->dy;
        ev.button = event->button;
        gui_object_handle_event(
          reinterpret_cast<gui_object_t*>(&interface), &ev);
        break;
      case EVENT_KEY_PRESSED:
        switch (event->dx) {
          /* Panel click shortcuts */
          case '1': {
            panel_bar_t *panel = interface_get_panel_bar(&interface);
            panel_bar_activate_button(panel, 0);
            break;
          }
          case '2': {
            panel_bar_t *panel = interface_get_panel_bar(&interface);
            panel_bar_activate_button(panel, 1);
            break;
          }
          case '3': {
            panel_bar_t *panel = interface_get_panel_bar(&interface);
            panel_bar_activate_button(panel, 2);
            break;
          }
          case '4': {
            panel_bar_t *panel = interface_get_panel_bar(&interface);
            panel_bar_activate_button(panel, 3);
            break;
          }
          case '5': {
            panel_bar_t *panel = interface_get_panel_bar(&interface);
            panel_bar_activate_button(panel, 4);
            break;
          }
          case '\t':
            if (event->dy & 2) {
              interface_return_from_message(&interface);
            } else {
              interface_open_message(&interface);
            }
            break;

          /* Game speed */
          case '+':
            if (game.game_speed < 40) game.game_speed += 1;
            LOGI("main", "Game speed: %u", game.game_speed);
            break;
          case '-':
            if (game.game_speed >= 1) game.game_speed -= 1;
            LOGI("main", "Game speed: %u", game.game_speed);
            break;
          case '0':
            game.game_speed = DEFAULT_GAME_SPEED;
            LOGI("main", "Game speed: %u", game.game_speed);
            break;
          case 'p':
            game_pause(game.game_speed == 0 ? 0 : 1);
            break;

          /* Audio */
          case 's':
            sfx_enable(!sfx_is_enabled());
            break;
          case 'm':
            midi_enable(!midi_is_enabled());
            break;

          /* Misc */
          case 27:
            if (GUI_OBJECT(&interface.notification_box)->displayed) {
              interface_close_message(&interface);
            } else if (GUI_OBJECT(&interface.popup)->displayed) {
              interface_close_popup(&interface);
            } else if (interface.building_road) {
              interface_build_road_end(&interface);
            }
            break;

          /* Debug */
          case 'g':
            interface.viewport.layers = static_cast<viewport_layer_t>(
                                          interface.viewport.layers ^
                                          VIEWPORT_LAYER_GRID);
            break;
          case 'b':
            interface.viewport.show_possible_build =
              !interface.viewport.show_possible_build;
            break;
          case 'j': {
            int current = 0;
            for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
              if (interface.player == game.player[i]) {
                current = i;
                break;
              }
            }

            for (int i = (current+1) % GAME_MAX_PLAYER_COUNT;
                 i != current; i = (i+1) % GAME_MAX_PLAYER_COUNT) {
              if (PLAYER_IS_ACTIVE(game.player[i])) {
                interface_set_player(&interface, i);
                LOGD("main", "Switched to player %i.", i);
                break;
              }
            }
            break;
          }
          case 'z':
            if (event->dy == 1) {
              save_game(0);
            }
            break;
          case 'n':
            interface_open_game_init(&interface);
            break;
          case 'c':
            if (event->dy == 1) {
              interface_open_popup(&interface, BOX_QUIT_CONFIRM);
            }
          default:
            break;
        }
        break;
      case EVENT_RESIZE:
        gui_object_set_size(reinterpret_cast<gui_object_t*>(&interface),
                            event->dx, event->dy);
        break;
      case EVENT_UPDATE:
        game_update();

        /* Autosave periodically */
        if ((game.const_tick % AUTOSAVE_INTERVAL) == 0 &&
            game.game_speed > 0) {
          int r = save_game(1);
          if (r < 0) LOGW("main", "Autosave failed.");
        }
        break;
      case EVENT_DRAW: {
        interface_update(&interface);
        frame_t *screen = static_cast<frame_t*>(event->object);
        gui_object_redraw(GUI_OBJECT(&interface), screen);
        break;
      }
      default:
        break;
    }

    return true;
  }
};

#define MAX_DATA_PATH      1024

/* Load data file from path is non-NULL, otherwise search in
   various standard paths. */
static int
load_data_file(const char *path) {
  const char *default_data_file[] = {
    "SPAE.PA", /* English */
    "SPAF.PA", /* French */
    "SPAD.PA", /* German */
    "SPAU.PA", /* Engish (US) */
    NULL
  };

  /* Use specified path. If something was specified
     but not found, this function should fail without
     looking anywhere else. */
  if (path != NULL) {
    LOGI("main", "Looking for game data in `%s'...", path);
    int r = data_load(path);
    if (r < 0) return -1;
    return 0;
  }

  /* If a path is not specified (path is NULL) then
     the configuration file is searched for in the directories
     specified by the XDG Base Directory Specification
     <http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html>.

     On windows platforms the %localappdata% is used in place of XDG_CONFIG_HOME.
  */

  char cp[MAX_DATA_PATH];
  char *env;

  /* Look in home */
  if ((env = getenv("XDG_DATA_HOME")) != NULL &&
      env[0] != '\0') {
    for (const char **df = default_data_file; *df != NULL; df++) {
      snprintf(cp, sizeof(cp), "%s/freeserf/%s", env, *df);
      LOGI("main", "Looking for game data in `%s'...", cp);
      int r = data_load(cp);
      if (r >= 0) return 0;
    }
  } else if ((env = getenv("HOME")) != NULL && env[0] != '\0') {
    for (const char **df = default_data_file; *df != NULL; df++) {
      snprintf(cp, sizeof(cp),
               "%s/.local/share/freeserf/%s", env, *df);
      LOGI("main", "Looking for game data in `%s'...", cp);
      int r = data_load(cp);
      if (r >= 0) return 0;
    }
  }

#ifdef _WIN32
  if ((env = getenv("userprofile")) != NULL && env[0] != '\0') {
    for (const char **df = default_data_file; *df != NULL; df++) {
      snprintf(cp, sizeof(cp),
               "%s/.local/share/freeserf/%s", env, *df);
      LOGI("main", "Looking for game data in `%s'...", cp);
      int r = data_load(cp);
      if (r >= 0) return 0;
    }
  }
#endif

  if ((env = getenv("XDG_DATA_DIRS")) != NULL && env[0] != '\0') {
    char *begin = env;
    while (1) {
      char *end = strchr(begin, ':');
      if (end == NULL) end = strchr(begin, '\0');

      int len = static_cast<int>(end - begin);
      if (len > 0) {
        for (const char **df = default_data_file; *df != NULL; df++) {
          snprintf(cp, sizeof(cp),
                   "%.*s/freeserf/%s", len, begin, *df);
          LOGI("main", "Looking for game data in `%s'...", cp);
          int r = data_load(cp);
          if (r >= 0) return 0;
        }
      }

      if (end[0] == '\0') break;
      begin = end + 1;
    }
  } else {
    /* Look in /usr/local/share and /usr/share per XDG spec. */
    for (const char **df = default_data_file; *df != NULL; df++) {
      snprintf(cp, sizeof(cp),
               "/usr/local/share/freeserf/%s", *df);
      LOGI("main", "Looking for game data in `%s'...", cp);
      int r = data_load(cp);
      if (r >= 0) return 0;
    }

    for (const char **df = default_data_file; *df != NULL; df++) {
      snprintf(cp, sizeof(cp),
               "/usr/share/freeserf/%s", *df);
      LOGI("main", "Looking for game data in `%s'...", cp);
      int r = data_load(cp);
      if (r >= 0) return 0;
    }
  }

  /* Look in current directory */
  for (const char **df = default_data_file; *df != NULL; df++) {
    LOGI("main", "Looking for game data in `%s'...", *df);
    int r = data_load(*df);
    if (r >= 0) return 0;
  }

  return -1;
}


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
  int r;

  std::string data_file;
  std::string save_file;

  int screen_width = DEFAULT_SCREEN_WIDTH;
  int screen_height = DEFAULT_SCREEN_HEIGHT;
  int fullscreen = 0;
  int map_generator = 0;

  init_missions();

  log_level_t log_level = DEFAULT_LOG_LEVEL;

#ifdef HAVE_UNISTD_H
  char opt;
  while (1) {
    opt = getopt(argc, argv, "d:fg:hl:r:t:");
    if (opt < 0) break;

    switch (opt) {
      case 'd':
        {
          int d = atoi(optarg);
          if (d >= 0 && d < LOG_LEVEL_MAX) {
            log_level = static_cast<log_level_t>(d);
          }
        }
        break;
      case 'f':
        fullscreen = 1;
        break;
      case 'g':
        data_file = optarg;
        break;
      case 'h':
        fprintf(stdout, HELP, argv[0]);
        exit(EXIT_SUCCESS);
        break;
      case 'l':
        save_file = optarg;
        break;
      case 'r':
        {
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

  r = load_data_file(data_file.empty() ? NULL : data_file.c_str());
  if (r < 0) {
    LOGE("main", "Could not load game data.");
    exit(EXIT_FAILURE);
  }

  LOGI("main", "Initialize graphics...");

  r = gfx_init(screen_width, screen_height, fullscreen);
  if (r < 0) exit(EXIT_FAILURE);

  /* TODO move to right place */
  audio_init();
  audio_set_volume(75);
  midi_play_track(MIDI_TRACK_0);

  game.map_generator = map_generator;

  game_init();

  /* Initialize interface */
  interface_init(&interface);
  gui_object_set_size(reinterpret_cast<gui_object_t*>(&interface),
      screen_width, screen_height);
  gui_object_set_displayed(reinterpret_cast<gui_object_t*>(&interface), 1);

  /* Either load a save game if specified or
     start a new game. */
  if (!save_file.empty()) {
    int r = game_load_save_game(save_file.c_str());
    if (r < 0) exit(EXIT_FAILURE);

    interface_set_player(&interface, 0);
  } else {
    int r = game_load_random_map(3, &interface.random);
    if (r < 0) exit(EXIT_FAILURE);

    /* Add default player */
    r = game_add_player(12, 64, 40, 40, 40);
    if (r < 0) exit(EXIT_FAILURE);

    interface_set_player(&interface, r);
  }

  viewport_map_reinit();

  if (!save_file.empty()) {
    interface_close_game_init(&interface);
  }

  /* Init game loop */
  game_event_handler_t *handler = new game_event_handler_t();
  event_loop = new event_loop_sdl_t();
  event_loop->add_handler(handler);

  /* Start game loop */
  event_loop->run();

  LOGI("main", "Cleaning up...");

  /* Clean up */
  map_deinit();
  viewport_map_deinit();
  audio_deinit();
  gfx_deinit();
  data_unload();

  return EXIT_SUCCESS;
}
