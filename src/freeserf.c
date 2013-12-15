/*
 * freeserf.c - Main program source.
 *
 * Copyright (C) 2013  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "freeserf.h"
#include "player.h"
#include "map.h"
#include "game.h"
#include "gui.h"
#include "popup.h"
#include "panel.h"
#include "viewport.h"
#include "interface.h"
#include "gfx.h"
#include "data.h"
#include "sdl-video.h"
#include "misc.h"
#include "debug.h"
#include "log.h"
#include "audio.h"
#include "savegame.h"
#include "mission.h"
#include "version.h"

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


/* Facilitates quick lookup of offsets following a spiral pattern in the map data.
   The columns following the second are filled out by setup_spiral_pattern(). */
static int spiral_pattern[] = {
	0, 0,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	5, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	5, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	5, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	9, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	9, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	9, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	9, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	9, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	9, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	9, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	16, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	24, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	24, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Initialize the global spiral_pattern. */
static void
init_spiral_pattern()
{
	static const int spiral_matrix[] = {
		1,  0,  0,  1,
		1,  1, -1,  0,
		0,  1, -1, -1,
		-1,  0,  0, -1,
		-1, -1,  1,  0,
		0, -1,  1,  1
	};

	game.spiral_pattern = spiral_pattern;

	for (int i = 0; i < 49; i++) {
		int x = spiral_pattern[2 + 12*i];
		int y = spiral_pattern[2 + 12*i + 1];

		for (int j = 0; j < 6; j++) {
			spiral_pattern[2+12*i+2*j] = x*spiral_matrix[4*j+0] + y*spiral_matrix[4*j+2];
			spiral_pattern[2+12*i+2*j+1] = x*spiral_matrix[4*j+1] + y*spiral_matrix[4*j+3];
		}
	}
}


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
	int r;

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

	r = save_text_state(f);
	if (r < 0) return -1;

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
	int fps = 0;
	float fps_ema = 0;
	int fps_target = 25;
	/* TODO: compute alpha dynamically based on frametime */
	const float ema_alpha = 0.07;

	const int frametime_target = 1000 / fps_target; /* in milliseconds */
	int last_frame = SDL_GetTicks();

	int drag_button = 0;

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
					gui_object_handle_event((gui_object_t *)&interface, &ev);

					drag_button = 0;
				}

				ev.type = GUI_EVENT_TYPE_BUTTON_UP;
				ev.x = event.button.x;
				ev.y = event.button.y;
				ev.button = event.button.button;
				gui_object_handle_event((gui_object_t *)&interface, &ev);

				if (event.button.button <= 3 &&
				    current_ticks - last_down[event.button.button-1] < MOUSE_TIME_SENSITIVITY) {
					ev.type = GUI_EVENT_TYPE_CLICK;
					ev.x = event.button.x;
					ev.y = event.button.y;
					ev.button = event.button.button;
					gui_object_handle_event((gui_object_t *)&interface, &ev);

					if (current_ticks - last_click[event.button.button-1] < MOUSE_TIME_SENSITIVITY &&
					    event.button.x >= last_click_x - MOUSE_MOVE_SENSITIVITY &&
					    event.button.x <= last_click_x + MOUSE_MOVE_SENSITIVITY &&
					    event.button.y >= last_click_y - MOUSE_MOVE_SENSITIVITY &&
					    event.button.y <= last_click_y + MOUSE_MOVE_SENSITIVITY) {
						ev.type = GUI_EVENT_TYPE_DBL_CLICK;
						ev.x = event.button.x;
						ev.y = event.button.y;
						ev.button = event.button.button;
						gui_object_handle_event((gui_object_t *)&interface, &ev);
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
				gui_object_handle_event((gui_object_t *)&interface, &ev);

				if (event.button.button <= 3) last_down[event.button.button-1] = current_ticks;
				break;
			case SDL_MOUSEMOTION:
				if (drag_button == 0) {
					/* Move pointer normally. */
					interface_set_cursor(&interface, event.motion.x, event.motion.y);
				}

				for (int button = 1; button <= 3; button++) {
					if (event.motion.state & SDL_BUTTON(button)) {
						if (drag_button == 0) {
							drag_button = button;

							ev.type = GUI_EVENT_TYPE_DRAG_START;
							ev.x = event.motion.x;
							ev.y = event.motion.y;
							ev.button = drag_button;
							gui_object_handle_event((gui_object_t *)&interface, &ev);
						}

						ev.type = GUI_EVENT_TYPE_DRAG_MOVE;
						ev.x = event.motion.x;
						ev.y = event.motion.y;
						ev.button = drag_button;
						gui_object_handle_event((gui_object_t *)&interface, &ev);
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
					viewport_move_by_pixels(viewport, 0, -1);
				}
					break;
				case SDLK_DOWN: {
					viewport_t *viewport = interface_get_top_viewport(&interface);
					viewport_move_by_pixels(viewport, 0, 1);
				}
					break;
				case SDLK_LEFT: {
					viewport_t *viewport = interface_get_top_viewport(&interface);
					viewport_move_by_pixels(viewport, -1, 0);
				}
					break;
				case SDLK_RIGHT: {
					viewport_t *viewport = interface_get_top_viewport(&interface);
					viewport_move_by_pixels(viewport, 1, 0);
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
						sdl_set_fullscreen(!sdl_is_fullscreen());
					}
					break;

					/* Misc */
				case SDLK_ESCAPE:
					if (GUI_OBJECT(&interface.popup)->displayed) {
						interface_close_popup(&interface);
					} else if (interface.building_road) {
						interface_build_road_end(&interface);
					}
					break;

					/* Debug */
				case SDLK_g:
					interface.viewport.layers ^= VIEWPORT_LAYER_GRID;
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
			}
		}

		unsigned int new_ticks = SDL_GetTicks();
		int delta_ticks = new_ticks - current_ticks;
		current_ticks = new_ticks;

		/* Update FPS EMA per frame */
		fps = 1000*(1.0 / (float)delta_ticks);
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
				LOGV("main", "FPS: %i", (int)fps_ema);
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

#define MAX_DATA_PATH      1024

/* Load data file from path is non-NULL, otherwise search in
   various standard paths. */
static int
load_data_file(const char *path)
{
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
		int r = gfx_load_file(path);
		if (r < 0) return -1;
		return 0;
	}

	/* Use default data file if none was specified. */
	char cp[MAX_DATA_PATH];
	char *env;

	/* Look in home */
	if ((env = getenv("XDG_DATA_HOME")) != NULL &&
	    env[0] != '\0') {
		for (const char **df = default_data_file; *df != NULL; df++) {
			snprintf(cp, sizeof(cp), "%s/freeserf/%s", env, *df);
			LOGI("main", "Looking for game data in `%s'...", cp);
			int r = gfx_load_file(cp);
			if (r >= 0) return 0;
		}
	}

#ifdef _WIN32
	if ((env = getenv("userprofile")) != NULL && env[0] != '\0') {
		for (const char **df = default_data_file; *df != NULL; df++) {
			snprintf(cp, sizeof(cp),
				 "%s/.local/share/freeserf/%s", env, *df);
			LOGI("main", "Looking for game data in `%s'...", cp);
			int r = gfx_load_file(cp);
			if (r >= 0) return 0;
		}
	}
#endif

	if ((env = getenv("HOME")) != NULL && env[0] != '\0') {
		for (const char **df = default_data_file; *df != NULL; df++) {
			snprintf(cp, sizeof(cp),
				 "%s/.local/share/freeserf/%s", env, *df);
			LOGI("main", "Looking for game data in `%s'...", cp);
			int r = gfx_load_file(cp);
			if (r >= 0) return 0;
		}
	}

	/* TODO look in DATADIR, getenv("XDG_DATA_DIRS") or
	   if not set look in /usr/local/share:/usr/share". */

	/* Look in current directory */
	for (const char **df = default_data_file; *df != NULL; df++) {
		LOGI("main", "Looking for game data in `%s'...", *df);
		int r = gfx_load_file(*df);
		if (r >= 0) return 0;
	}

	return -1;
}


#define USAGE					\
	"Usage: %s [-g DATA-FILE]\n"
#define HELP							\
	USAGE							\
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

	int log_level = DEFAULT_LOG_LEVEL;

	int opt;
	while (1) {
		opt = getopt(argc, argv, "d:fg:hl:r:t:");
		if (opt < 0) break;

		switch (opt) {
		case 'd':
		{
			int d = atoi(optarg);
			if (d >= 0 && d < LOG_LEVEL_MAX) {
				log_level = d;
			}
		}
			break;
		case 'f':
			fullscreen = 1;
			break;
		case 'g':
			data_file = malloc(strlen(optarg)+1);
			if (data_file == NULL) exit(EXIT_FAILURE);
			strcpy(data_file, optarg);
			break;
		case 'h':
			fprintf(stdout, HELP, argv[0]);
			exit(EXIT_SUCCESS);
			break;
		case 'l':
			save_file = malloc(strlen(optarg)+1);
			if (save_file == NULL) exit(EXIT_FAILURE);
			strcpy(save_file, optarg);
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

	/* Set up logging */
	log_set_file(stdout);
	log_set_level(log_level);

	LOGI("main", "freeserf %s", FREESERF_VERSION);

	r = load_data_file(data_file);
	if (r < 0) {
		LOGE("main", "Could not load game data.");
		exit(EXIT_FAILURE);
	}

	free(data_file);

	gfx_data_fixup();

	LOGI("main", "SDL init...");

	r = sdl_init();
	if (r < 0) exit(EXIT_FAILURE);

	/* TODO move to right place */
	audio_init();
	audio_set_volume(75);
	midi_play_track(MIDI_TRACK_0);

	/*gfx_set_palette(DATA_PALETTE_INTRO);*/
	gfx_set_palette(DATA_PALETTE_GAME);

	LOGI("main", "SDL resolution %ix%i...", screen_width, screen_height);

	r = sdl_set_resolution(screen_width, screen_height, fullscreen);
	if (r < 0) exit(EXIT_FAILURE);

	game.map_generator = map_generator;

	/* Initialize global lookup tables */
	init_spiral_pattern();

	game_init();

	/* Initialize interface */
	interface_init(&interface);
	gui_object_set_size((gui_object_t *)&interface,
			    screen_width, screen_height);
	gui_object_set_displayed((gui_object_t *)&interface, 1);

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
	sdl_deinit();
	gfx_unload();

	return EXIT_SUCCESS;
}
