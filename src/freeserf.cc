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

#include "misc.h"

BEGIN_EXT_C
  #include "freeserf.h"
  #include "interface.h"
  #include "gfx.h"
  #include "data.h"
  #include "sdl-video.h"
  #include "log.h"
  #include "audio.h"
  #include "savegame.h"
  #include "mission.h"
  #include "version.h"

  #ifdef HAVE_CONFIG_H
  # include <config.h>
  #endif
END_EXT_C

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

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

/* How fast consequtive mouse events need to be generated
   in order to be interpreted as click and double click. */
#define MOUSE_TIME_SENSITIVITY  600
/* How much the mouse can move between events to be still
   considered as a double click. */
#define MOUSE_MOVE_SENSITIVITY  8


static int game_loop_run;

static interface_t interface;

/* In target, replace any character from needle with replacement character. */
static void
strreplace(char *target, const char *needle, char replace)
{
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
save_game(int autosave)
{
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


void
game_loop_quit()
{
  game_loop_run = 0;
}

/* game_loop() has been turned into a SDL based loop.
   The code for one iteration of the original game_loop is
   in game_loop_iter. */
static void
game_loop()
{
  /* FPS */
  float fps = 0;
  float fps_ema = 0;
  int fps_target = 25;
  /* TODO: compute alpha dynamically based on frametime */
  const float ema_alpha = 0.07f;

  const int frametime_target = 1000 / fps_target; /* in milliseconds */
  int last_frame = SDL_GetTicks();

  int drag_button = 0;
  int drag_x = 0;
  int drag_y = 0;

  uint last_down[3] = {0};
  uint last_click[3] = {0};
  uint last_click_x = 0;
  uint last_click_y = 0;

  uint current_ticks = SDL_GetTicks();
  uint accum = 0;

  SDL_Event event;
  gui_event_t ev;

  game_loop_run = 1;
  while (game_loop_run) {
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_MOUSEBUTTONUP:
          if (drag_button == event.button.button) {
            ev.type = GUI_EVENT_TYPE_DRAG_END;
            ev.x = event.button.x;
            ev.y = event.button.y;
            ev.button = drag_button;
            gui_object_handle_event(reinterpret_cast<gui_object_t*>(&interface), &ev);

            drag_button = 0;
          }

          ev.type = GUI_EVENT_TYPE_BUTTON_UP;
          ev.x = event.button.x;
          ev.y = event.button.y;
          ev.button = event.button.button;
          gui_object_handle_event(reinterpret_cast<gui_object_t*>(&interface), &ev);

          if (event.button.button <= 3 &&
              current_ticks - last_down[event.button.button-1] < MOUSE_TIME_SENSITIVITY) {
            ev.type = GUI_EVENT_TYPE_CLICK;
            ev.x = event.button.x;
            ev.y = event.button.y;
            ev.button = event.button.button;
            gui_object_handle_event(reinterpret_cast<gui_object_t*>(&interface), &ev);

            if (current_ticks - last_click[event.button.button-1] < MOUSE_TIME_SENSITIVITY &&
                event.button.x >= static_cast<int>(last_click_x - MOUSE_MOVE_SENSITIVITY) &&
                event.button.x <= static_cast<int>(last_click_x + MOUSE_MOVE_SENSITIVITY) &&
                event.button.y >= static_cast<int>(last_click_y - MOUSE_MOVE_SENSITIVITY) &&
                event.button.y <= static_cast<int>(last_click_y + MOUSE_MOVE_SENSITIVITY)) {
              ev.type = GUI_EVENT_TYPE_DBL_CLICK;
              ev.x = event.button.x;
              ev.y = event.button.y;
              ev.button = event.button.button;
              gui_object_handle_event(reinterpret_cast<gui_object_t *>(&interface), &ev);
            }

            last_click[event.button.button-1] = current_ticks;
            last_click_x = event.button.x;
            last_click_y = event.button.y;
          }
          break;
        case SDL_MOUSEBUTTONDOWN:
          ev.type = GUI_EVENT_TYPE_BUTTON_DOWN;
          ev.x = event.button.x;
          ev.y = event.button.y;
          ev.button = event.button.button;
          gui_object_handle_event(reinterpret_cast<gui_object_t*>(&interface), &ev);

          if (event.button.button <= 3) last_down[event.button.button-1] = current_ticks;
          break;
        case SDL_MOUSEMOTION:
          for (int button = 1; button <= 3; button++) {
            if (event.motion.state & SDL_BUTTON(button)) {
              if (drag_button == 0) {
                drag_button = button;
                drag_x = event.motion.x;
                drag_y = event.motion.y;

                ev.type = GUI_EVENT_TYPE_DRAG_START;
                ev.x = event.motion.x;
                ev.y = event.motion.y;
                ev.button = drag_button;
                gui_object_handle_event(reinterpret_cast<gui_object_t*>(&interface), &ev);
              }

              ev.type = GUI_EVENT_TYPE_DRAG_MOVE;
              ev.x = event.motion.x - drag_x;
              ev.y = event.motion.y - drag_y;
              ev.button = drag_button;
              gui_object_handle_event(reinterpret_cast<gui_object_t*>(&interface), &ev);

              sdl_warp_mouse(drag_x, drag_y);

              break;
            }
          }
          break;
        case SDL_KEYDOWN:
          if (event.key.keysym.sym == SDLK_q &&
              (event.key.keysym.mod & KMOD_CTRL)) {
            game_loop_quit();
            break;
          }

          switch (event.key.keysym.sym) {
            /* Map scroll */
            case SDLK_UP: {
              viewport_t *viewport = interface_get_top_viewport(&interface);
              viewport_move_by_pixels(viewport, 0, -32);
            }
              break;
            case SDLK_DOWN: {
              viewport_t *viewport = interface_get_top_viewport(&interface);
              viewport_move_by_pixels(viewport, 0, 32);
            }
              break;
            case SDLK_LEFT: {
              viewport_t *viewport = interface_get_top_viewport(&interface);
              viewport_move_by_pixels(viewport, -32, 0);
            }
              break;
            case SDLK_RIGHT: {
              viewport_t *viewport = interface_get_top_viewport(&interface);
              viewport_move_by_pixels(viewport, 32, 0);
            }
              break;

              /* Panel click shortcuts */
            case SDLK_1: {
              panel_bar_t *panel = interface_get_panel_bar(&interface);
              panel_bar_activate_button(panel, 0);
            }
              break;
            case SDLK_2: {
              panel_bar_t *panel = interface_get_panel_bar(&interface);
              panel_bar_activate_button(panel, 1);
            }
              break;
            case SDLK_3: {
              panel_bar_t *panel = interface_get_panel_bar(&interface);
              panel_bar_activate_button(panel, 2);
            }
              break;
            case SDLK_4: {
              panel_bar_t *panel = interface_get_panel_bar(&interface);
              panel_bar_activate_button(panel, 3);
            }
              break;
            case SDLK_5: {
              panel_bar_t *panel = interface_get_panel_bar(&interface);
              panel_bar_activate_button(panel, 4);
            }
              break;

            case SDLK_TAB:
              if (event.key.keysym.mod & KMOD_SHIFT) {
                interface_return_from_message(&interface);
              } else {
                interface_open_message(&interface);
              }
              break;

              /* Game speed */
            case SDLK_PLUS:
            case SDLK_KP_PLUS:
            case SDLK_EQUALS:
              if (game.game_speed < 40) game.game_speed += 1;
              LOGI("main", "Game speed: %u", game.game_speed);
              break;
            case SDLK_MINUS:
            case SDLK_KP_MINUS:
              if (game.game_speed >= 1) game.game_speed -= 1;
              LOGI("main", "Game speed: %u", game.game_speed);
              break;
            case SDLK_0:
              game.game_speed = DEFAULT_GAME_SPEED;
              LOGI("main", "Game speed: %u", game.game_speed);
              break;
            case SDLK_p:
              if (game.game_speed == 0) game_pause(0);
              else game_pause(1);
              break;

              /* Audio */
            case SDLK_s:
              sfx_enable(!sfx_is_enabled());
              break;
            case SDLK_m:
              midi_enable(!midi_is_enabled());
              break;

              /* Video */
            case SDLK_f:
              if (event.key.keysym.mod & KMOD_CTRL) {
                gfx_set_fullscreen(!gfx_is_fullscreen());
              }
              break;

              /* Misc */
            case SDLK_ESCAPE:
              if (GUI_OBJECT(&interface.notification_box)->displayed) {
                interface_close_message(&interface);
              } else if (GUI_OBJECT(&interface.popup)->displayed) {
                interface_close_popup(&interface);
              } else if (interface.building_road) {
                interface_build_road_end(&interface);
              }
              break;

              /* Debug */
            case SDLK_g:
              interface.viewport.layers = static_cast<viewport_layer_t>(interface.viewport.layers ^ VIEWPORT_LAYER_GRID);
              break;
            case SDLK_b:
              interface.viewport.show_possible_build = !interface.viewport.show_possible_build;
              break;
            case SDLK_j: {
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
            }
              break;
            case SDLK_z:
              if (event.key.keysym.mod & KMOD_CTRL) {
                save_game(0);
              }
              break;
            case SDLK_F10:
              interface_open_game_init(&interface);
              break;

            default:
              break;
          }
          break;
        case SDL_QUIT:
          game_loop_quit();
          break;
        case SDL_WINDOWEVENT:
          if (SDL_WINDOWEVENT_SIZE_CHANGED == event.window.event){
            int width = 0;
            int height = 0;
            sdl_get_resolution(&width, &height);
            sdl_set_resolution(width, height, gfx_is_fullscreen());
            gui_object_set_size(reinterpret_cast<gui_object_t*>(&interface), width, height);
          }
          break;
      }
    }

    uint new_ticks = SDL_GetTicks();
    int delta_ticks = new_ticks - current_ticks;
    current_ticks = new_ticks;

    /* Update FPS EMA per frame */
    fps = 1000.f*(1.f / static_cast<float>(delta_ticks));
    if (fps_ema > 0) fps_ema = ema_alpha*fps + (1-ema_alpha)*fps_ema;
    else if (fps > 0) fps_ema = fps;

    accum += delta_ticks;
    while (accum >= TICK_LENGTH) {
      game_update();

      /* Autosave periodically */
      if ((game.const_tick % AUTOSAVE_INTERVAL) == 0 &&
          game.game_speed > 0) {
        int r = save_game(1);
        if (r < 0) LOGW("main", "Autosave failed.");
      }

      /* Print FPS */
      if ((game.const_tick % (10*TICKS_PER_SEC)) == 0) {
        LOGV("main", "FPS: %i", static_cast<int>(fps_ema));
      }

      accum -= TICK_LENGTH;
    }

    /* Update and draw interface */
    interface_update(&interface);

    frame_t *screen = sdl_get_screen_frame();
    gui_object_redraw(GUI_OBJECT(&interface), screen);

    /* Swap video buffers */
    sdl_swap_buffers();

    /* Reduce framerate to target if we finished too fast */
    int now = SDL_GetTicks();
    int frametime_spent = now - last_frame;

    if (frametime_spent < frametime_target) {
      SDL_Delay(frametime_target - frametime_spent);
    }
    last_frame = SDL_GetTicks();
  }
}


#define USAGE					\
  "Usage: %s [-g DATA-FILE]\n"
#define HELP                                                            \
  USAGE                                                                 \
      " -d NUM\t\tSet debug output level\n"				\
      " -f\t\tFullscreen mode (CTRL-q to exit)\n"			\
      " -g DATA-FILE\tUse specified data file\n"			\
      " -h\t\tShow this help text\n"					\
      " -l FILE\tLoad saved game\n"					\
      " -r RES\t\tSet display resolution (e.g. 800x600)\n"		\
      " -t GEN\t\tMap generator (0 or 1)\n"				\
      "\n"								\
      "Please report bugs to <" PACKAGE_BUGREPORT ">\n"

int
main(int argc, char *argv[])
{
  int r;

  char *data_file = NULL;
  char *save_file = NULL;

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
        if (data_file != NULL) {
          free(data_file);
          data_file = NULL;
        }
        if (strlen(optarg) > 0) {
          data_file = static_cast<char*>(malloc(strlen(optarg)+1));
          if (data_file == NULL) exit(EXIT_FAILURE);
          strcpy(data_file, optarg);
        }
        break;
      case 'h':
        fprintf(stdout, HELP, argv[0]);
        exit(EXIT_SUCCESS);
        break;
      case 'l':
        if (save_file != NULL) {
          free(save_file);
          save_file = NULL;
        }
        if (strlen(optarg) > 0) {
          save_file = static_cast<char*>(malloc(strlen(optarg)+1));
          if (save_file == NULL) exit(EXIT_FAILURE);
          strcpy(save_file, optarg);
		}
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

  r = data_init(data_file);
  if (data_file != NULL) {
    free(data_file);
  }
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
  if (save_file != NULL) {
    int r = game_load_save_game(save_file);
    if (r < 0) exit(EXIT_FAILURE);
    free(save_file);

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

  if (save_file != NULL) {
    interface_close_game_init(&interface);
  }

  /* Start game loop */
  game_loop();

  LOGI("main", "Cleaning up...");

  /* Clean up */
  map_deinit();
  viewport_map_deinit();
  audio_deinit();
  gfx_deinit();
  data_deinit();

  return EXIT_SUCCESS;
}
