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
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

#include "freeserf.h"
#include "freeserf_endian.h"
#include "serf.h"
#include "flag.h"
#include "building.h"
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
#include "version.h"

/* TODO This file is one big of mess of all the things that should really
   be separated out into smaller files.  */

#define DEFAULT_SCREEN_WIDTH  800
#define DEFAULT_SCREEN_HEIGHT 600

#ifndef DEFAULT_LOG_LEVEL
# ifndef NDEBUG
#  define DEFAULT_LOG_LEVEL  LOG_LEVEL_DEBUG
# else
#  define DEFAULT_LOG_LEVEL  LOG_LEVEL_INFO
# endif
#endif


static int game_loop_run;

static frame_t screen_frame;
static frame_t cursor_buffer;

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


static void
player_interface_init()
{
	game.frame = &screen_frame;
	int width = sdl_frame_get_width(game.frame);
	int height = sdl_frame_get_height(game.frame);

	interface_init(&interface);
	gui_object_set_size((gui_object_t *)&interface, width, height);
	gui_object_set_displayed((gui_object_t *)&interface, 1);
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

/* Update global anim counters based on game.tick.
   Note: anim counters control the rate of updates in
   the rest of the game objects (_not_ just gfx animations). */
static void
anim_update_and_more()
{
	/* TODO ... */
	game.old_anim = game.anim;
	game.anim = game.tick >> 16;
	game.anim_diff = game.anim - game.old_anim;

	int anim_xor = game.anim ^ game.old_anim;

	/* Viewport animation does not care about low bits in anim */
	if (anim_xor >= 1 << 3) {
		viewport_t *viewport = interface_get_top_viewport(&interface);
		gui_object_set_redraw((gui_object_t *)viewport);
	}

	if ((game.anim & 0xffff) == 0 && game.game_speed > 0) {
		int r = save_game(1);
		if (r < 0) LOGW("main", "Autosave failed.");
	}

	if (BIT_TEST(game.svga, 3)) { /* Game has started */
		/* TODO */

		if (interface.return_timeout < game.anim_diff) {
			interface.msg_flags |= BIT(4);
			interface.msg_flags &= ~BIT(3);
			interface.return_timeout = 0;
		} else {
			interface.return_timeout -= game.anim_diff;
		}
	}
}


/* One iteration of game_loop(). */
static void
game_loop_iter()
{
	/* TODO music and sound effects */

	anim_update_and_more();

	/* TODO ... */

	game_update();

	/* TODO ... */

	interface.flags &= ~BIT(4);
	interface.flags &= ~BIT(7);

	/* TODO */

	/* Undraw cursor */
	sdl_draw_frame(interface.pointer_x-8, interface.pointer_y-8,
		       sdl_get_screen_frame(), 0, 0, &cursor_buffer, 16, 16);

	gui_object_redraw((gui_object_t *)&interface, game.frame);

	/* TODO very crude dirty marking algortihm: mark everything. */
	sdl_mark_dirty(0, 0, sdl_frame_get_width(game.frame),
		       sdl_frame_get_height(game.frame));

	/* ADDITIONS */

	/* Restore cursor buffer */
	sdl_draw_frame(0, 0, &cursor_buffer,
		       interface.pointer_x-8, interface.pointer_y-8,
		       sdl_get_screen_frame(), 16, 16);

	/* Mouse cursor */
	gfx_draw_transp_sprite(interface.pointer_x-8,
			       interface.pointer_y-8,
			       DATA_CURSOR, sdl_get_screen_frame());

	sdl_swap_buffers();
}


/* The length of a game tick in miliseconds. */
#define TICK_LENGTH  20

/* How fast consequtive mouse events need to be generated
   in order to be interpreted as click and double click. */
#define MOUSE_SENSITIVITY  600


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
	int fps_ema = 0;
	int fps_target = 100;
	const float ema_alpha = 0.003;

	int drag_button = 0;

	unsigned int last_down[3] = {0};
	unsigned int last_click[3] = {0};

	unsigned int current_ticks = SDL_GetTicks();
	unsigned int accum = 0;
	unsigned int accum_frames = 0;

	SDL_Event event;
	gui_event_t ev;

	game_loop_run = 1;
	while (game_loop_run) {
		if (SDL_PollEvent(&event)) {
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
				    current_ticks - last_down[event.button.button-1] < MOUSE_SENSITIVITY) {
					ev.type = GUI_EVENT_TYPE_CLICK;
					ev.x = event.button.x;
					ev.y = event.button.y;
					ev.button = event.button.button;
					gui_object_handle_event((gui_object_t *)&interface, &ev);

					if (current_ticks - last_click[event.button.button-1] < MOUSE_SENSITIVITY) {
						ev.type = GUI_EVENT_TYPE_DBL_CLICK;
						ev.x = event.button.x;
						ev.y = event.button.y;
						ev.button = event.button.button;
						gui_object_handle_event((gui_object_t *)&interface, &ev);
					}

					last_click[event.button.button-1] = current_ticks;
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
					if (event.motion.x != interface.pointer_x || event.motion.y != interface.pointer_y) {
						/* Undraw cursor */
						sdl_draw_frame(interface.pointer_x-8, interface.pointer_y-8,
							       sdl_get_screen_frame(), 0, 0, &cursor_buffer, 16, 16);

						interface.pointer_x = min(max(0, event.motion.x), interface.pointer_x_max);
						interface.pointer_y = min(max(0, event.motion.y), interface.pointer_y_max);

						/* Restore cursor buffer */
						sdl_draw_frame(0, 0, &cursor_buffer,
							       interface.pointer_x-8, interface.pointer_y-8,
							       sdl_get_screen_frame(), 16, 16);
					}
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

					/* Game speed */
				case SDLK_PLUS:
				case SDLK_KP_PLUS:
					if (game.game_speed < 0xffff0000) game.game_speed += 0x10000;
					LOGI("main", "Game speed: %u", game.game_speed >> 16);
					break;
				case SDLK_MINUS:
				case SDLK_KP_MINUS:
					if (game.game_speed >= 0x10000) game.game_speed -= 0x10000;
					LOGI("main", "Game speed: %u", game.game_speed >> 16);
					break;
				case SDLK_0:
					game.game_speed = 0x20000;
					LOGI("main", "Game speed: %u", game.game_speed >> 16);
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

					/* Misc */
				case SDLK_ESCAPE:
					if (BIT_TEST(interface.click, 7)) { /* Building road */
						interface_build_road_end(&interface);
					} else if (interface.clkmap != 0) {
						interface_close_popup(&interface);
					}
					break;

					/* Debug */
				case SDLK_g:
					interface.viewport.layers ^= VIEWPORT_LAYER_GRID;
					break;
				case SDLK_j: {
					int current = 0;
					for (int i = 0; i < 4; i++) {
						if (interface.player == game.player[i]) {
							current = i;
							break;
						}
					}

					for (int i = (current+1) % 4; i != current; i = (i+1) % 4) {
						if (PLAYER_IS_ACTIVE(game.player[i])) {
							interface.player = game.player[i];
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

		accum += delta_ticks;
		while (accum >= TICK_LENGTH) {
			game.const_tick += 1;
			game.tick += game.game_speed;

			/* FPS */
			fps = 1000*((float)accum_frames / accum);
			if (fps_ema > 0) fps_ema = ema_alpha*fps + (1-ema_alpha)*fps_ema;
			else if (fps > 0) fps_ema = fps;

			accum -= TICK_LENGTH;
			accum_frames = 0;
		}

		game_loop_iter();
#if 0
		draw_green_string(6, 10, sdl_get_screen_frame(), "FPS");
		draw_green_number(10, 10, sdl_get_screen_frame(), fps_ema);
#endif

		accum_frames += 1;

		/* Reduce framerate to target */
		if (fps_target > 0) {
			int delay = 0;
			if (fps_ema > 0) delay = (1000/fps_target) - (1000/fps_ema);
			if (delay > 0) SDL_Delay(delay);
		}
	}
}

/* Allocate global memory before game starts. */
static void
allocate_global_memory()
{
	/* Players */
	game.player[0] = malloc(sizeof(player_t));
	if (game.player[0] == NULL) abort();

	game.player[1] = malloc(sizeof(player_t));
	if (game.player[1] == NULL) abort();

	game.player[2] = malloc(sizeof(player_t));
	if (game.player[2] == NULL) abort();

	game.player[3] = malloc(sizeof(player_t));
	if (game.player[3] == NULL) abort();

	/* TODO this should be allocated on game start according to the
	   map size of the particular game instance. */
	int max_map_size = 10;
	game.serf_limit = (0x1f84 * (1 << max_map_size) - 4) / 0x81;
	game.flag_limit = (0x2314 * (1 << max_map_size) - 4) / 0x231;
	game.building_limit = (0x54c * (1 << max_map_size) - 4) / 0x91;
	game.inventory_limit = (0x54c * (1 << max_map_size) - 4) / 0x3c1;

	/* Serfs */
	game.serfs = malloc(game.serf_limit * sizeof(serf_t));
	if (game.serfs == NULL) abort();

	game.serf_bitmap = malloc(((game.serf_limit-1) / 8) + 1);
	if (game.serf_bitmap == NULL) abort();

	/* Flags */
	game.flags = malloc(game.flag_limit * sizeof(flag_t));
	if (game.flags == NULL) abort();

	game.flag_bitmap = malloc(((game.flag_limit-1) / 8) + 1);
	if (game.flag_bitmap == NULL) abort();

	/* Buildings */
	game.buildings = malloc(game.building_limit * sizeof(building_t));
	if (game.buildings == NULL) abort();

	game.building_bitmap = malloc(((game.building_limit-1) / 8) + 1);
	if (game.building_bitmap == NULL) abort();

	/* Inventories */
	game.inventories = malloc(game.inventory_limit * sizeof(inventory_t));
	if (game.inventories == NULL) abort();

	game.inventory_bitmap = malloc(((game.inventory_limit-1) / 8) + 1);
	if (game.inventory_bitmap == NULL) abort();

	/* Setup screen frame */
	frame_t *screen = sdl_get_screen_frame();
	sdl_frame_init(&screen_frame, 0, 0, sdl_frame_get_width(screen),
		       sdl_frame_get_height(screen), screen);

	/* Setup cursor occlusion buffer */
	sdl_frame_init(&cursor_buffer, 0, 0, 16, 16, NULL);
}

#define MAX_DATA_PATH      1024
#define DEFAULT_DATA_FILE  "SPAE.PA"

/* Load data file from path is non-NULL, otherwise search in
   various standard paths. */
static int
load_data_file(const char *path)
{
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
		snprintf(cp, sizeof(cp), "%s/freeserf/" DEFAULT_DATA_FILE, env);
		path = cp;
#ifdef _WIN32
	} else if ((env = getenv("userprofile")) != NULL && env[0] != '\0') {
		snprintf(cp, sizeof(cp),
			 "%s/.local/share/freeserf/" DEFAULT_DATA_FILE, env);
		path = cp;
#endif
	} else if ((env = getenv("HOME")) != NULL && env[0] != '\0') {
		snprintf(cp, sizeof(cp),
			 "%s/.local/share/freeserf/" DEFAULT_DATA_FILE, env);
		path = cp;
	}

	if (path != NULL) {
		LOGI("main", "Looking for game data in `%s'...", path);
		int r = gfx_load_file(path);
		if (r >= 0) return 0;
	}

	/* TODO look in DATADIR, getenv("XDG_DATA_DIRS") or
	   if not set look in /usr/local/share:/usr/share". */

	/* Look in current directory */
	LOGI("main", "Looking for game data in `%s'...", DEFAULT_DATA_FILE);
	int r = gfx_load_file(DEFAULT_DATA_FILE);
	if (r >= 0) return 0;

	return -1;
}


#define USAGE					\
	"Usage: %s [-g DATA-FILE]\n"
#define HELP							\
	USAGE							\
	" -d NUM\t\tSet debug output level\n"			\
	" -f\t\tFullscreen mode (CTRL-q to exit)\n"		\
	" -g DATA-FILE\tUse specified data file\n"		\
	" -h\t\tShow this help text\n"				\
	" -l FILE\tLoad saved game\n"				\
	" -m MAP\t\tSelect world map (1-3)\n"			\
	" -p\t\tPreserve map bugs of the original game\n"	\
	" -r RES\t\tSet display resolution (e.g. 800x600)\n"	\
	" -t GEN\t\tMap generator (0 or 1)\n"

int
main(int argc, char *argv[])
{
	int r;

	char *data_file = NULL;
	char *save_file = NULL;

	int screen_width = DEFAULT_SCREEN_WIDTH;
	int screen_height = DEFAULT_SCREEN_HEIGHT;
	int fullscreen = 0;
	int game_map = 1;
	int map_generator = 0;
	int preserve_map_bugs = 0;

	int log_level = DEFAULT_LOG_LEVEL;

	int opt;
	while (1) {
		opt = getopt(argc, argv, "d:fg:hl:m:pr:t:");
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
		case 'm':
			game_map = atoi(optarg);
			break;
		case 'p':
			preserve_map_bugs = 1;
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
	midi_play_track(MIDI_TRACK_0);
	audio_set_volume(75);

	/*gfx_set_palette(DATA_PALETTE_INTRO);*/
	gfx_set_palette(DATA_PALETTE_GAME);

	LOGI("main", "SDL resolution %ix%i...", screen_width, screen_height);

	r = sdl_set_resolution(screen_width, screen_height, fullscreen);
	if (r < 0) exit(EXIT_FAILURE);

	game.mission_level = game_map - 1; /* set game map */
	game.map_generator = map_generator;
	game.map_preserve_bugs = preserve_map_bugs;

	/* Init globals */
	allocate_global_memory();
	player_interface_init();

	/* Initialize global lookup tables */
	init_spiral_pattern();

	game_init();

	/* Either load a save game if specified or
	   start a new game. */
	if (save_file != NULL) {
		int r = game_load_save_game(save_file);
		if (r < 0) exit(EXIT_FAILURE);
		free(save_file);
	} else {
		game_load_random_map();
	}

	/* Move viewport to initial position */
	viewport_move_to_map_pos(&interface.viewport, interface.map_cursor_pos);

	/* Start game loop */
	game_loop();

	LOGI("main", "Cleaning up...");

	/* Clean up */
	audio_cleanup();
	sdl_deinit();
	gfx_unload();

	return EXIT_SUCCESS;
}
