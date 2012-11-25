/* freeserf.c */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <assert.h>

#include "freeserf.h"
#include "freeserf_endian.h"
#include "globals.h"
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


static unsigned int tick;
static int update_from_cb;

static frame_t svga_full_frame;
static frame_t game_area_svga_frame;
static frame_t svga_normal_frame;

static frame_t popup_box_left_frame;

/* Viewport holds the state of the main map window
   (e.g. size and current map location). */
static viewport_t viewport;
static panel_bar_t panel;
static popup_box_t popup;

static interface_t interface;

static char *game_file = NULL;



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
setup_spiral_pattern()
{
	static const int spiral_matrix[] = {
		1,  0,  0,  1,
		1,  1, -1,  0,
		0,  1, -1, -1,
		-1,  0,  0, -1,
		-1, -1,  1,  0,
		0, -1,  1,  1
	};

	globals.spiral_pattern = spiral_pattern;

	for (int i = 0; i < 49; i++) {
		int x = spiral_pattern[2 + 12*i];
		int y = spiral_pattern[2 + 12*i + 1];

		for (int j = 0; j < 6; j++) {
			spiral_pattern[2+12*i+2*j] = x*spiral_matrix[4*j+0] + y*spiral_matrix[4*j+2];
			spiral_pattern[2+12*i+2*j+1] = x*spiral_matrix[4*j+1] + y*spiral_matrix[4*j+3];
		}
	}
}

viewport_t *
gui_get_top_viewport()
{
	return &viewport;
}

panel_bar_t *
gui_get_panel_bar()
{
	return &panel;
}

popup_box_t *
gui_get_popup_box()
{
	return &popup;
}

void
gui_show_popup_frame(int show)
{
	gui_object_set_displayed((gui_object_t *)&popup, show);
}

/* Initialize game speed. */
static void
start_game_tick()
{
	globals.game_speed = DEFAULT_GAME_SPEED;
	update_from_cb = 1;
}

/* Initialize player objects. */
static void
init_player_structs(player_t *p[])
{
	/* Player 1 */
	p[0]->flags = 0;
	p[0]->config = globals.cfg_left;
	p[0]->msg_flags = 0;
	p[0]->return_timeout = 0;
	/* ... */
	p[0]->click = 0;
	p[0]->minimap_advanced = -1;
	/* ... */
	p[0]->click |= BIT(1);
	p[0]->clkmap = 0;
	p[0]->flags |= BIT(4);

	/* OBSOLETE
	p[0]->col_game_area = 0;
	p[0]->row_game_area = 0;
	p[0]->map_rows
	*/

	/* TODO ... */

	p[0]->popup_frame = &popup_box_left_frame;

	p[0]->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
	p[0]->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
	p[0]->panel_btns[2] = PANEL_BTN_MAP; /* TODO init to inactive */
	p[0]->panel_btns[3] = PANEL_BTN_STATS; /* TODO init to inactive */
	p[0]->panel_btns[4] = PANEL_BTN_SETT; /* TODO init to inactive */

	p[0]->panel_btns_set[0] = -1;
	p[0]->panel_btns_set[1] = -1;
	p[0]->panel_btns_set[2] = -1;
	p[0]->panel_btns_set[3] = -1;
	p[0]->panel_btns_set[4] = -1;

	/* TODO ... */

	p[0]->sett = globals.player_sett[0];
	/*p[0]->map_serf_rows = globals.map_serf_rows_left; OBSOLETE */
	p[0]->minimap_flags = 8;
	p[0]->current_stat_8_mode = 0;
	p[0]->current_stat_7_item = 7;
	p[0]->box = 0;

	/* TODO ... */

	p[0]->map_cursor_sprites[0].sprite = 32;
	p[0]->map_cursor_sprites[1].sprite = 33;
	p[0]->map_cursor_sprites[2].sprite = 33;
	p[0]->map_cursor_sprites[3].sprite = 33;
	p[0]->map_cursor_sprites[4].sprite = 33;
	p[0]->map_cursor_sprites[5].sprite = 33;
	p[0]->map_cursor_sprites[6].sprite = 33;

	/* TODO ... */

	/* Player 2 */
	/* TODO */
}

/* Initialize player viewport. */
static void
init_players_svga(player_t *p[])
{
	globals.frame = &game_area_svga_frame;
	int width = sdl_frame_get_width(globals.frame);
	int height = sdl_frame_get_height(globals.frame);

	/* ADDITION init viewport */
	viewport_init(&viewport, p[0]);
	gui_object_set_displayed((gui_object_t *)&viewport, 1);

	/* ADDITION init panel bar */
	panel_bar_init(&panel, p[0]);
	gui_object_set_displayed((gui_object_t *)&panel, 1);

	/* ADDITION init popup box */
	popup_box_init(&popup, p[0]);

	/* ADDITION interface */
	interface_init(&interface);
	gui_object_set_size((gui_object_t *)&interface, width, height);
	gui_object_set_displayed((gui_object_t *)&interface, 1);

	/* OBSOLETE */
	/*
	p[0]->pointer_x_drag = 0;
	p[0]->pointer_y_drag = 0;
	*/

	p[0]->pointer_x_max = width;
	p[0]->pointer_y_max = height;
	p[0]->pointer_x_off = 0;

	p[0]->game_area_cols = (width >> 4) + 4;
	p[0]->game_area_rows = (height >> 4) + 4;
	p[0]->bottom_panel_width = 352;
	p[0]->bottom_panel_height = 40;
	p[0]->bottom_panel_x = (width - p[0]->bottom_panel_width) / 2;
	p[0]->bottom_panel_y = height - p[0]->bottom_panel_height;

	/* map_serf_rows is OBSOLETE
	int r = p[0]->game_area_rows;
	p[0]->map_serf_rows = malloc((r+1)*sizeof(int *) + (r+1)*253*sizeof(int));
	if (p[0]->map_serf_rows == NULL) abort();
	*/

	p[0]->frame_width = width;
	p[0]->frame_height = height;

	p[0]->col_offset = 0; /* TODO center */
	p[0]->row_offset = 0; /* TODO center */
	p[0]->map_min_x = 0;
	p[0]->map_min_y = 0;
	p[0]->map_max_y = height + 2*MAP_TILE_HEIGHT;

	p[0]->panel_btns_x = 64;
	p[0]->panel_btns_first_x = 64;
	p[0]->panel_btns_dist = 48;
	p[0]->msg_icon_x = 40;
	p[0]->timer_icon_x = 304;

	p[0]->popup_x = (width - 144) / 2;
	p[0]->popup_y = 270;

	p[0]->map_x_off = 0/*288*/;
	p[0]->map_y_off = -4/*276*/;
	p[0]->map_cursor_col_max = 2*p[0]->game_area_cols + 8/*76*/;
	p[0]->map_cursor_col_off = 0/*36*/;

	p[0]->frame = &svga_normal_frame;

	/* Add objects to interface container. */
	interface_set_top(&interface, (gui_object_t *)&viewport);
	interface_add_float(&interface, (gui_object_t *)&popup,
			    p[0]->popup_x, p[0]->popup_y, 144, 160);
	interface_add_float(&interface, (gui_object_t *)&panel,
			    p[0]->bottom_panel_x, p[0]->bottom_panel_y,
			    p[0]->bottom_panel_width,
			    p[0]->bottom_panel_height);
}

static void
init_players()
{
	/* mouse related call */

	init_player_structs(globals.player);

	if (/* split mode */ 0) {
	} else {
		globals.player[1]->flags |= 1;
		if (BIT_TEST(globals.svga, 7)) {
			init_players_svga(globals.player);
		} else {
			/*init_players_lowres(globals.player);*/
		}
	}
}

/* Part of initialization procedure. */
static void
switch_to_pregame_video_mode()
{
	if (/*svga_mode*/ 0) {
		/* svga_mode = 0; */
		/* lowres_mode_and_redraw_frames(); */
	}

	/* set unknown flag */

	/* split_mode = 0; */

	init_players();

	/* check more flags */
}

static int
handle_map_drag(player_t *player)
{
	if (BIT_TEST(player->click, 3)) { /* TODO should check bit 1, not bit 3*/
		if (!/*pathway scrolling*/0) {
			/* handled in the viewport */
		} else {
			/* TODO pathway scrolling */
		}
	} else {
		return -1;
	}

	return 0;
}

/* Initialize spiral_pos_pattern from spiral_pattern. */
static void
init_spiral_pos_pattern()
{
	int *pattern = globals.spiral_pattern;

	for (int i = 0; i < 295; i++) {
		int x = pattern[2*i];
		int y = pattern[2*i+1];

		map_pos_t pos = ((y & globals.map_row_mask) << globals.map_row_shift) | (x & globals.map_col_mask);
		globals.spiral_pos_pattern[i] = pos;
	}
}

/* Reset various global map/game state. */
static void
reset_game_objs()
{
	globals.map_water_level = 20;
	globals.map_max_lake_area = 14;

	globals.update_map_last_anim = 0;
	globals.update_map_counter = 0;
	globals.update_map_16_loop = 0;
	globals.update_map_initial_pos = 0;
	/* globals.field_54 = 0; */
	/* globals.field_56 = 0; */
	globals.next_index = 0;

	/* loops */
	memset(globals.flg_bitmap, 0, ((globals.max_flg_cnt-1) / 8) + 1);
	memset(globals.buildings_bitmap, 0, ((globals.max_building_cnt-1) / 8) + 1);
	memset(globals.serfs_bitmap, 0, ((globals.max_serf_cnt-1) / 8) + 1);
	memset(globals.inventories_bitmap, 0, ((globals.max_inventory_cnt-1) / 8) + 1);

	globals.max_ever_flag_index = 0;
	globals.max_ever_building_index = 0;
	globals.max_ever_serf_index = 0;
	globals.max_ever_inventory_index = 0;

	/* Create NULL-serf */
	serf_t *serf;
	game_alloc_serf(&serf, NULL);
	serf->state = SERF_STATE_NULL;
	serf->type = 0;
	serf->animation = 0;
	serf->counter = 0;
	serf->pos = -1;

	/* Create NULL-flag (index 0 is undefined) */
	game_alloc_flag(NULL, NULL);

	/* Create NULL-building (index 0 is undefined) */
	building_t *building;
	game_alloc_building(&building, NULL);
	building->bld = 0;
}

/* Initialize map constants for the given map size (map_cols, map_rows). */
static void
init_map_vars()
{
	/* globals.split |= BIT(3); */

	if (globals.map_cols < 64 || globals.map_rows < 64) {
		/* globals.split &= ~BIT(3); */
	}

	globals.map_col_pairs = globals.map_cols >> 1;
	globals.map_row_pairs = globals.map_rows >> 1;

	globals.map_elms = globals.map_cols * globals.map_rows;

	globals.map_col_mask = globals.map_cols - 1;
	globals.map_row_mask = globals.map_rows - 1;

	globals.map_row_shift = 0;
	int cols = globals.map_cols;
	while (cols > 0) {
		globals.map_row_shift += 1;
		cols >>= 1;
	}

	globals.map_shifted_row_mask = globals.map_row_mask << globals.map_row_shift;
	globals.map_shifted_col_mask = globals.map_col_mask;

	globals.map_index_mask = globals.map_shifted_row_mask | globals.map_shifted_col_mask;

	globals.map_col_size = 2*globals.map_cols;
	globals.map_data_offset = globals.map_cols;

	/* init map movement vars */
	globals.map_dirs[DIR_RIGHT] = 1 & globals.map_col_mask;
	globals.map_dirs[DIR_LEFT] = -1 & globals.map_col_mask;
	globals.map_move_left_2 = -1 & globals.map_col_mask;
	globals.map_dirs[DIR_DOWN] = globals.map_col_size & globals.map_shifted_row_mask;
	globals.map_dirs[DIR_UP] = -globals.map_col_size & globals.map_shifted_row_mask;

	globals.map_dirs[DIR_DOWN_RIGHT] = globals.map_dirs[DIR_DOWN] | globals.map_dirs[DIR_RIGHT];
	globals.map_dirs[DIR_UP_RIGHT] = globals.map_dirs[DIR_UP] | globals.map_dirs[DIR_RIGHT];
	globals.map_dirs[DIR_DOWN_LEFT] = globals.map_dirs[DIR_DOWN] | globals.map_dirs[DIR_LEFT];
	globals.map_dirs[DIR_UP_LEFT] = globals.map_dirs[DIR_UP] | globals.map_dirs[DIR_LEFT];

	globals.map_regions = (globals.map_cols >> 5) * (globals.map_rows >> 5);
	globals.map_max_serfs_left = globals.map_regions * 500;
	globals.map_62_5_times_regions = (globals.map_regions * 500) >> 3;

	/* TODO ... */
}

/* Initialize AI parameters. */
static void
init_ai_values(player_sett_t *sett, int face)
{
	const int ai_values_0[] = { 13, 10, 16, 9, 10, 8, 6, 10, 12, 5, 8 };
	const int ai_values_1[] = { 10000, 13000, 16000, 16000, 18000, 20000, 19000, 18000, 30000, 23000, 26000 };
	const int ai_values_2[] = { 10000, 35000, 20000, 27000, 37000, 25000, 40000, 30000, 50000, 35000, 40000 };
	const int ai_values_3[] = { 0, 36, 0, 31, 8, 480, 3, 16, 0, 193, 39 };
	const int ai_values_4[] = { 0, 30000, 5000, 40000, 50000, 20000, 45000, 35000, 65000, 25000, 30000 };
	const int ai_values_5[] = { 60000, 61000, 60000, 65400, 63000, 62000, 65000, 63000, 64000, 64000, 64000 };

	sett->ai_value_0 = ai_values_0[face-1];
	sett->ai_value_1 = ai_values_1[face-1];
	sett->ai_value_2 = ai_values_2[face-1];
	sett->ai_value_3 = ai_values_3[face-1];
	sett->ai_value_4 = ai_values_4[face-1];
	sett->ai_value_5 = ai_values_5[face-1];
}

/* Initialize player_sett_t objects. */
static void
reset_player_settings()
{
	globals.winning_player = -1;
	/* TODO ... */
	globals.max_next_index = 33;

	/* TODO */

	for (int i = 0; i < 4; i++) {
		player_sett_t *sett = globals.player_sett[i];
		memset(sett, 0, sizeof(player_sett_t));
		sett->flags = 0;

		player_init_t *init = &globals.pl_init[i];
		if (init->face != 0) {
			sett->flags |= BIT(6); /* Player active */
			if (init->face < 12) { /* AI player */
				sett->flags |= BIT(7); /* Set AI bit */
				/* TODO ... */
				globals.max_next_index = 49;
			}

			sett->player_num = i;
			/* sett->field_163 = 0; */
			sett->build = 0;
			/*sett->field_163 |= BIT(0);*/

			sett->map_cursor_col = 0;
			sett->map_cursor_row = 0;
			sett->map_cursor_type = 0;
			sett->panel_btn_type = 0;
			sett->building_height_after_level = 0;
			sett->building = 0;
			sett->castle_flag = 0;
			sett->cont_search_after_non_optimal_find = 7;
			sett->field_110 = 4;
			sett->knights_to_spawn = 0;
			/* OBSOLETE sett->spawn_serf_want_knight = 0; **/
			sett->total_land_area = 0;

			/* TODO ... */

			sett->last_anim = 0;

			sett->serf_to_knight_rate = 20000;
			sett->serf_to_knight_counter = 0x8000;

			sett->knight_occupation[0] = 0x10;
			sett->knight_occupation[1] = 0x21;
			sett->knight_occupation[2] = 0x32;
			sett->knight_occupation[3] = 0x43;

			player_sett_reset_food_priority(sett);
			player_sett_reset_planks_priority(sett);
			player_sett_reset_steel_priority(sett);
			player_sett_reset_coal_priority(sett);
			player_sett_reset_wheat_priority(sett);
			player_sett_reset_tool_priority(sett);

			player_sett_reset_flag_priority(sett);
			player_sett_reset_inventory_priority(sett);

			sett->current_sett_5_item = 8;
			sett->current_sett_6_item = 15;

			/* TODO ... */
			sett->timers_count = 0;

			sett->castle_knights_wanted = 3;
			sett->castle_knights = 0;
			/* TODO ... */
			sett->serf_index = 0;
			/* TODO ... */
			sett->initial_supplies = init->supplies;
			sett->reproduction_reset = (60 - init->reproduction) * 50;
			sett->ai_intelligence = 1300*init->intelligence + 13535;

			if (/*init->face != 0*/1) { /* Redundant check */
				if (init->face < 12) { /* AI player */
					init_ai_values(sett, init->face);
				}
			}

			sett->reproduction_counter = sett->reproduction_reset;
			/* TODO ... */
			for (int i = 0; i < 26; i++) sett->resource_count[i] = 0;
			for (int i = 0; i < 24; i++) {
				sett->completed_building_count[i] = 0;
				sett->incomplete_building_count[i] = 0;
			}

			/* TODO */

			for (int i = 0; i < 16; i++) {
				for (int j = 0; j < 112; j++) sett->player_stat_history[i][j] = 0;
			}

			for (int i = 0; i < 26; i++) {
				for (int j = 0; j < 120; j++) sett->resource_count_history[i][j] = 0;
			}

			for (int i = 0; i < 27; i++) sett->serf_count[i] = 0;
		}
	}

	if (BIT_TEST(globals.split, 6)) { /* Coop mode */
		/* TODO ... */
	}
}

/* Initialize global stuff. */
static void
init_player_settings()
{
	globals.anim = 0;
	/* TODO ... */
}

/* Initialize global counters for game updates. */
static void
init_game_globals()
{
	memset(globals.player_history_index, '\0', sizeof(globals.player_history_index));
	memset(globals.player_history_counter, '\0', sizeof(globals.player_history_counter));

	globals.resource_history_index = 0;
	globals.game_tick = 0;
	globals.anim = 0;
	/* TODO ... */
	globals.game_stats_counter = 0;
	globals.history_counter = 0;
	globals.anim_diff = 0;
	/* TODO */
}

/* Update global anim counters based on game_tick.
   Note: anim counters control the rate of updates in
   the rest of the game objects (_not_ just gfx animations). */
static void
anim_update_and_more()
{
	/* TODO ... */
	globals.old_anim = globals.anim;
	globals.anim = globals.game_tick >> 16;
	globals.anim_diff = globals.anim - globals.old_anim;

	int anim_xor = globals.anim ^ globals.old_anim;

	/* Viewport animation does not care about low bits in anim */
	if (anim_xor >= 1 << 3) {
		gui_object_set_redraw((gui_object_t *)&viewport);
	}

	if (BIT_TEST(globals.svga, 3)) { /* Game has started */
		/* TODO */

		player_t *player = globals.player[0];
		if (player->return_timeout < globals.anim_diff) {
			player->msg_flags |= BIT(4);
			player->msg_flags &= ~BIT(3);
			player->return_timeout = 0;
		} else {
			player->return_timeout -= globals.anim_diff;
		}

		/* TODO Same for player 2 return timeout. */
	}
}

/* update_player_input is split into a click handler and a drag handler.
   x and y are absolute instead of relative coords.
   lmb and rmb are boolean (i.e. zero is false, non-zero is true),
   instead of 0 is true, -1 is false. */
static void
update_player_input_drag(player_t *player, int x, int y, int lmb, int rmb)
{
	if (rmb) {
		if (/*fast mapclick*/0) {
			/* TODO fast mapclick */
		} else {
			player->click |= BIT(3); /* set right click pending */
		}
	} else {
		player->click &= ~BIT(3); /* reset right click pending */
	}

	/* TODO more lmb(?) */
}

static void
update_player_input_click(player_t *player, int x, int y, int lmb, int rmb, int lmb_dbl_time)
{
	/*if (player->field_9E) {
		player->field_9E -= 1;
		if (player->field_9E == 0) player->click &= ~BIT(4);
	}*/

	player->pointer_x = min(max(0, x), player->pointer_x_max);
	player->pointer_y = min(max(0, y), player->pointer_y_max);

	if (lmb) {
		/* TODO ... */
		player->flags &= ~BIT(3);
		player->flags |= BIT(2);
		/* player->field_8A = 0; */
		player->pointer_x_clk = player->pointer_x;
		player->pointer_y_clk = player->pointer_y;
	} else {
		player->flags |= BIT(3);
	}

	/* Handle double click as special click */
	if (lmb && lmb_dbl_time < 600) player->click |= BIT(3);
	else player->click &= ~BIT(3);
}

/* Not used any more (?). */
int
init_flag_search()
{
	globals.flag_search_counter += 1;

	if (globals.flag_search_counter == 0) {
		globals.flag_search_counter += 1;
		for (int i = 1; i < globals.max_ever_flag_index; i++) {
			if (BIT_TEST(globals.flg_bitmap[i>>3], 7-(i&7))) {
				game_get_flag(i)->search_num = 0;
			}
		}
	}

	globals.flag_queue_select = 0;
	return globals.flag_search_counter;
}

static void
handle_player_click(player_t *player)
{
	player->flags &= ~BIT(2);
}

/* Handle player click and update buttons/cursors/sprites. */
static void
handle_player_click_and_update(player_t *player)
{
	if (BIT_TEST(player->flags, 0)) return; /* Player not active */

	/* TODO ... */

	if (BIT_TEST(player->flags, 2)) { /* Left click pending */
		handle_player_click(player);
	}

	if (BIT_TEST(player->click, 5)) { /* Unknown */
		/* TODO ... */
	}

	/* TODO ... */

	if (BIT_TEST(player->click, 2)) {
		player->click &= ~BIT(2);

		/* Extracted from a small function 42059 */
		if (!BIT_TEST(player->click, 7)) { /* Not building road */
			player_determine_map_cursor_type(player);
		} else { /* Building road */
			player_determine_map_cursor_type_road(player);
		}

		player_update_interface(player);
		/* redraw map cursor */
	}

	/* TODO ... */
}

static void
handle_player_inputs()
{
	handle_player_click_and_update(globals.player[0]);
	if (/*not coop mode*/1) {
		handle_player_click_and_update(globals.player[1]);
	} else {
		/* TODO coop mode */
	}
}

static void
draw_player_popup_to_frame(player_t *player)
{
	if (BIT_TEST(player->flags, 0)) return; /* Player inactive */

	if (BIT_TEST(player->click, 1)) return;

	if (BIT_TEST(globals.split, 5)) { /* Demo mode */
		/* TODO ... */
	} else {
		sdl_draw_frame(player->popup_x-8, player->popup_y-9, player->frame,
			       0, 0, player->popup_frame, 144, 160);
	}
}

static void
draw_popups_to_frame()
{
	draw_player_popup_to_frame(globals.player[0]);
	draw_player_popup_to_frame(globals.player[1]);
}

/* Update game_tick. */
static void
update_game_tick()
{
	/*globals.field_208 += 1;*/
	globals.game_tick += globals.game_speed;

	/* Update player input: This is done from the SDL main loop instead. */

	/* TODO pcm sounds ... */
}

/* Initialize map parameters from mission number. */
static int
load_map_spec()
{
	/* Only the three first are available for now. */
	const map_spec_t mission[] = {
		{
			/* Mission 1: START */
			.rnd_1 = 0x6d6f,
			.rnd_2 = 0xf7f0,
			.rnd_3 = 0xc8d4,

			.pl_0_supplies = 35,
			.pl_0_reproduction = 30,

			.pl_1_face = 1,
			.pl_1_intelligence = 10,
			.pl_1_supplies = 5,
			.pl_1_reproduction = 30,
		}, {
			/* Mission 2: STATION */
			.rnd_1 = 0x60b9,
			.rnd_2 = 0xe728,
			.rnd_3 = 0xc484,

			.pl_0_supplies = 30,
			.pl_0_reproduction = 40,

			.pl_1_face = 2,
			.pl_1_intelligence = 12,
			.pl_1_supplies = 15,
			.pl_1_reproduction = 30,

			.pl_2_face = 3,
			.pl_2_intelligence = 14,
			.pl_2_supplies = 15,
			.pl_2_reproduction = 30
		}, {
			/* Mission 3: UNITY */
			.rnd_1 = 0x12ab,
			.rnd_2 = 0x7a4a,
			.rnd_3 = 0xe483,

			.pl_0_supplies = 30,
			.pl_0_reproduction = 30,

			.pl_1_face = 2,
			.pl_1_intelligence = 18,
			.pl_1_supplies = 10,
			.pl_1_reproduction = 25,

			.pl_2_face = 4,
			.pl_2_intelligence = 18,
			.pl_2_supplies = 10,
			.pl_2_reproduction = 25
		}
	};

	int m = globals.mission_level;

	globals.pl_init[0].face = 12;
	globals.pl_init[0].supplies = mission[m].pl_0_supplies;
	globals.pl_init[0].intelligence = 40;
	globals.pl_init[0].reproduction = mission[m].pl_0_reproduction;

	globals.pl_init[1].face = mission[m].pl_1_face;
	globals.pl_init[1].supplies = mission[m].pl_1_supplies;
	globals.pl_init[1].intelligence = mission[m].pl_1_intelligence;
	globals.pl_init[1].reproduction = mission[m].pl_1_reproduction;

	globals.pl_init[2].face = mission[m].pl_2_face;
	globals.pl_init[2].supplies = mission[m].pl_2_supplies;
	globals.pl_init[2].intelligence = mission[m].pl_2_intelligence;
	globals.pl_init[2].reproduction = mission[m].pl_2_reproduction;

	globals.pl_init[3].face = mission[m].pl_3_face;
	globals.pl_init[3].supplies = mission[m].pl_3_supplies;
	globals.pl_init[3].intelligence = mission[m].pl_3_intelligence;
	globals.pl_init[3].reproduction = mission[m].pl_3_reproduction;

	/* TODO ... */

	globals.init_map_rnd_1 = mission[m].rnd_1;
	globals.init_map_rnd_2 = mission[m].rnd_2;
	globals.init_map_rnd_3 = mission[m].rnd_3;

	int map_size = 3;

	globals.init_map_rnd_1 ^= 0x5a5a;
	globals.init_map_rnd_2 ^= 0xa5a5;
	globals.init_map_rnd_3 ^= 0xc3c3;

	return map_size;
}

/* Initialize various things before game starts. */
static void
start_game()
{
	globals.map_size = load_map_spec();
	globals.map_cols = 32 << (globals.map_size >> 1);
	globals.map_rows = 32 << ((globals.map_size - 1) >> 1);

	globals.split &= ~BIT(2); /* Not split screen */
	if (0/*globals.game_type == GAME_TYPE_2_PLAYERS*/) globals.split |= BIT(2);

	globals.split &= ~BIT(6); /* Not coop mode */

	/* TODO ... */

	globals.split &= ~BIT(5); /* Not demo mode */
	if (0/*globals.game_type == GAME_TYPE_DEMO*/) globals.split |= BIT(5);

	/*globals.game_loop_transition |= BIT(1); */
	globals.svga |= BIT(3); /* Game has started. */
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

	handle_player_inputs();

	handle_map_drag(globals.player[0]);
	globals.player[0]->flags &= ~BIT(4);
	globals.player[0]->flags &= ~BIT(7);

	/* TODO */

	gui_object_redraw((gui_object_t *)&interface, globals.frame);

	/* TODO very crude dirty marking algortihm: mark everything. */
	sdl_mark_dirty(0, 0, sdl_frame_get_width(globals.frame),
		       sdl_frame_get_height(globals.frame));

	/* ADDITIONS */
	/* Mouse cursor */
	gfx_draw_transp_sprite(globals.player[0]->pointer_x-8,
			       globals.player[0]->pointer_y-8,
			       DATA_CURSOR, sdl_get_screen_frame());

#if 0
	draw_green_string(2, 316, sdl_get_screen_frame(), "Col:");
	draw_green_number(10, 316, sdl_get_screen_frame(), globals.player_sett[0]->map_cursor_col);

	draw_green_string(2, 324, sdl_get_screen_frame(), "Row:");
	draw_green_number(10, 324, sdl_get_screen_frame(), globals.player_sett[0]->map_cursor_row);

	map_pos_t cursor_pos = MAP_POS(globals.player_sett[0]->map_cursor_col, globals.player_sett[0]->map_cursor_row);
	gfx_draw_string(16, 332, 47, 1, sdl_get_screen_frame(), "Height:");
	gfx_draw_number(80, 332, 47, 1, sdl_get_screen_frame(), MAP_HEIGHT(cursor_pos));

	gfx_draw_string(16, 340, 47, 1, sdl_get_screen_frame(), "Object:");
	gfx_draw_number(80, 340, 47, 1, sdl_get_screen_frame(), MAP_OBJ(cursor_pos));
#endif

	sdl_swap_buffers();
}


/* The length of a game tick in miliseconds. */
#define TICK_LENGTH  20

/* How fast consequtive mouse events need to be generated
   in order to be interpreted as click and double click. */
#define MOUSE_SENSITIVITY  600


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

	int lmb_state = 0;
	int rmb_state = 0;
	int drag_button = 0;

	unsigned int last_down[3] = {0};
	unsigned int last_click[3] = {0};

	unsigned int current_ticks = SDL_GetTicks();
	unsigned int accum = 0;
	unsigned int accum_frames = 0;

	int do_loop = 1;
	SDL_Event event;
	gui_event_t ev;

	while (do_loop) {
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

				if (event.button.button == SDL_BUTTON_LEFT) lmb_state = 0;
				else if (event.button.button == SDL_BUTTON_RIGHT) rmb_state = 0;

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
				if (event.button.button == SDL_BUTTON_LEFT) lmb_state = 1;
				else if (event.button.button == SDL_BUTTON_RIGHT) rmb_state = 1;

				ev.type = GUI_EVENT_TYPE_BUTTON_DOWN;
				ev.x = event.button.x;
				ev.y = event.button.y;
				ev.button = event.button.button;
				gui_object_handle_event((gui_object_t *)&interface, &ev);

				update_player_input_click(globals.player[0], event.button.x, event.button.y, lmb_state, rmb_state, current_ticks - last_down[SDL_BUTTON_LEFT-1]);

				if (event.button.button <= 3) last_down[event.button.button-1] = current_ticks;
				break;
			case SDL_MOUSEMOTION:
				if (drag_button == 0) {
					/* Move pointer normally. */
					if (event.motion.x != globals.player[0]->pointer_x || event.motion.y != globals.player[0]->pointer_y) {
						globals.player[0]->pointer_x = min(max(0, event.motion.x), globals.player[0]->pointer_x_max);
						globals.player[0]->pointer_y = min(max(0, event.motion.y), globals.player[0]->pointer_y_max);
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

				update_player_input_drag(globals.player[0], event.motion.x, event.motion.y,
							 event.motion.state & SDL_BUTTON(1), event.motion.state & SDL_BUTTON(3));
				break;
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_q &&
				    (event.key.keysym.mod & KMOD_CTRL)) {
					do_loop = 0;
					break;
				}

				switch (event.key.keysym.sym) {
					/* Map scroll */
				case SDLK_UP:
					viewport_move_by_pixels(&viewport, 0, -1);
					break;
				case SDLK_DOWN:
					viewport_move_by_pixels(&viewport, 0, 1);
					break;
				case SDLK_LEFT:
					viewport_move_by_pixels(&viewport, -1, 0);
					break;
				case SDLK_RIGHT:
					viewport_move_by_pixels(&viewport, 1, 0);
					break;

					/* Panel click shortcuts */
				case SDLK_1:
					panel_bar_activate_button(&panel, 0);
					break;
				case SDLK_2:
					panel_bar_activate_button(&panel, 1);
					break;
				case SDLK_3:
					panel_bar_activate_button(&panel, 2);
					break;
				case SDLK_4:
					panel_bar_activate_button(&panel, 3);
					break;
				case SDLK_5:
					panel_bar_activate_button(&panel, 4);
					break;

					/* Game speed */
				case SDLK_PLUS:
				case SDLK_KP_PLUS:
					if (globals.game_speed < 0xffff0000) globals.game_speed += 0x10000;
					LOGI("main", "Game speed: %u", globals.game_speed >> 16);
					break;
				case SDLK_MINUS:
				case SDLK_KP_MINUS:
					if (globals.game_speed >= 0x10000) globals.game_speed -= 0x10000;
					LOGI("main", "Game speed: %u", globals.game_speed >> 16);
					break;
				case SDLK_0:
					globals.game_speed = 0x20000;
					LOGI("main", "Game speed: %u", globals.game_speed >> 16);
					break;
				case SDLK_p:
					if (globals.game_speed == 0) game_pause(0);
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
					if (BIT_TEST(globals.player[0]->click, 7)) { /* Building road */
						player_build_road_end(globals.player[0]);
					} else if (globals.player[0]->clkmap != 0) {
						player_close_popup(globals.player[0]);
					}
					break;

					/* Debug */
				case SDLK_g:
					viewport.layers ^= VIEWPORT_LAYER_GRID;
					break;

				default:
					break;
				}
				break;
			case SDL_QUIT:
				do_loop = 0;
				break;
			}
		}

		unsigned int new_ticks = SDL_GetTicks();
		int delta_ticks = new_ticks - current_ticks;
		current_ticks = new_ticks;

		accum += delta_ticks;
		while (accum >= TICK_LENGTH) {
			/* This is main_timer_cb */
			tick += 1;
			/* In original, deep_tree is called which will call update_game.
			   Here, update_game is just called directly. */
			update_game_tick();

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

/* Initialization before game starts. */
static void
pregame_continue()
{
	/* TODO ... */

	/* Start the game. This should be called when the start button in the setup screen is clicked. */
	start_game();

	if (BIT_TEST(globals.svga, 3)) { /* Game has been started */
		init_map_vars();
		reset_game_objs();
		map_init();

		init_spiral_pos_pattern();
		/* lowres_mode_and_init_players(); */

		reset_player_settings();
		
		/*draw_game_and_popup_frame();*/
		/*panel.obj.draw((gui_object_t *)&panel);*/

		init_player_settings();
		init_game_globals();
	} else {
		/* TODO ... */
	}

	/* TEST Load save game data */
	if (game_file != NULL) {
		FILE *f = fopen(game_file, "rb");
		if (f == NULL) {
			LOGE("main", "Unable to open save game file: `%s'.", game_file);
			exit(EXIT_FAILURE);
		}

		int r = load_v0_state(f);
		if (r < 0) {
			LOGE("main", "Unable to load save game.");
			exit(EXIT_FAILURE);
		}
	}

#if 0
	for (int i = 0; i < globals.map_rows; i++) {
		map_1_t *map = globals.map_mem2_ptr;
		map_2_t *map_data = MAP_2_DATA(map);
		for (int j = 0; j < globals.map_cols; j++) {
			map_pos_t pos = MAP_POS(j, i);
			LOGD(NULL, "% 3i, % 3i:  %02x %02x %02x %02x    %04x %04x  H(%i)",
			    j, i,
			    map[pos].flags, map[pos].height, map[pos].type, map[pos].obj,
			    map_data[pos].u.index, map_data[pos].serf_index,
			    map[pos].height & 0x1f);

			const char *name_from_dir[] = { "right", "down right", "down" };

			for (dir_t d = DIR_RIGHT; d <= DIR_DOWN; d++) {
				map_pos_t other_pos = MAP_MOVE(pos, d);
				int h_diff = MAP_HEIGHT(pos) - MAP_HEIGHT(other_pos);
				if (h_diff < -4 || h_diff > 4) {
					LOGD(NULL, "h_diff fail: %s, (%i, %i, %i) -> (%i, %i, %i)",
					       name_from_dir[d],
					       MAP_COORD_ARGS(pos), MAP_HEIGHT(pos),
					       MAP_COORD_ARGS(other_pos), MAP_HEIGHT(other_pos));
				}
			}
		}
	}
#endif

	/* ADDITION move viewport to (0,0). */
	viewport_move_to_map_pos(&viewport, 0, 0);

	game_loop();
}

static void
load_serf_animation_table()
{
	/* The serf animation table is stored in big endian
	   order in the data file.

	   * The first uint32 is the byte length of the rest
	   of the table (skipped below).
	   * Next is 199 uint32s that are offsets from the start
	   of this table to an animation table (one for each
	   animation).
	   * The animation tables are of varying lengths.
	   Each entry in the animation table is three bytes
	   long. First byte is used to determine the serf body
	   sprite. Second byte is a signed horizontal sprite
	   offset. Third byte is a signed vertical offset.
	*/
	globals.serf_animation_table = ((int *)gfx_get_data_object(DATA_SERF_ANIMATION_TABLE, NULL)) + 1;

	/* Endianess convert from big endian. */
	for (int i = 0; i < 199; i++) {
		globals.serf_animation_table[i] = be32toh(globals.serf_animation_table[i]);
	}
}

static void
pregame_init()
{
	/* TODO unknown function ... */

	/* mouse setup */

	setup_spiral_pattern();

	/* sound and joystick setup */

	/* reset two globals */

	switch_to_pregame_video_mode();

	/*draw_game_and_popup_frame();*/

	/*
	gui_draw_bottom_frame(&svga_full_frame);
	gui_draw_panel_buttons();
	*/

	start_game_tick();

	/* show_intro_screens(); */
	load_serf_animation_table(); /* called in show_intro_screens(); */

	pregame_continue();
}

/* Memory allocation before game starts. */
static void
hand_out_memory_2()
{
	/* TODO ... */

	/* hand out memory */

	/* Player structs */
	globals.player[0] = malloc(sizeof(player_t));
	if (globals.player[0] == NULL) abort();

	globals.player[1] = malloc(sizeof(player_t));
	if (globals.player[1] == NULL) abort();

	/* Flag queues (for road graph search) */
	globals.flag_queue_black = malloc(1000*sizeof(flag_t *));
	if (globals.flag_queue_black == NULL) abort();

	globals.flag_queue_white = malloc(1000*sizeof(flag_t *));
	if (globals.flag_queue_white == NULL) abort();

	/* TODO ... */

	globals.spiral_pos_pattern = malloc(295*sizeof(map_pos_t));
	if (globals.spiral_pos_pattern == NULL) abort();

	/* TODO ... */

	/* Map rows */
	/* OBSOLETE */
	/*globals.player_map_rows[0] = malloc(0x984);
	if (globals.player_map_rows[0] == NULL) abort();*/

	/*globals.player_map_rows[1] = malloc(0x984);
	if (globals.player_map_rows[1] == NULL) abort();*/

	/* Player settings */
	globals.player_sett[0] = malloc(sizeof(player_sett_t));
	if (globals.player_sett[0] == NULL) abort();

	globals.player_sett[1] = malloc(sizeof(player_sett_t));
	if (globals.player_sett[1] == NULL) abort();

	globals.player_sett[2] = malloc(sizeof(player_sett_t));
	if (globals.player_sett[2] == NULL) abort();

	globals.player_sett[3] = malloc(sizeof(player_sett_t));
	if (globals.player_sett[3] == NULL) abort();

	/* Map serf rows OBSOLETE */
	/*globals.map_serf_rows_left = malloc(29*sizeof(int *) + 29*253*sizeof(int));
	if (globals.map_serf_rows_left == NULL) abort();

	globals.map_serf_rows_right = malloc(29*sizeof(int *) + 29*253*sizeof(int));
	if (globals.map_serf_rows_right == NULL) abort();*/

	/* vid mem popups: allocated as part of SDL surface later */

	/* TODO ... */

	/* void *credits_bg = gfx_get_data_object(DATA_CREDITS_BG, NULL); */

	/* globals.map_ascii_sprite = map_ascii_sprite; */ /* not neccessary */

	/* more memory handout */
	/* TODO ... */

	globals.map_mem2_ptr = globals.map_mem2;

	int max_map_size = /*(globals.map & 0xf) + 2*/10;
	globals.max_serf_cnt = (0x1f84 * (1 << max_map_size) - 4) / 0x81;
	globals.max_flg_cnt = (0x2314 * (1 << max_map_size) - 4) / 0x231;
	globals.max_building_cnt = (0x54c * (1 << max_map_size) - 4) / 0x91;
	globals.max_inventory_cnt = (0x54c * (1 << max_map_size) - 4) / 0x3c1;

	/* map mem3 */
	globals.serfs = malloc(globals.max_serf_cnt * sizeof(serf_t));
	if (globals.serfs == NULL) abort();

	globals.serfs_bitmap = malloc(((globals.max_serf_cnt-1) / 8) + 1);
	if (globals.serfs_bitmap == NULL) abort();

	/* map mem1 */
	globals.flgs = malloc(globals.max_flg_cnt * sizeof(flag_t));
	if (globals.flgs == NULL) abort();

	globals.flg_bitmap = malloc(((globals.max_flg_cnt-1) / 8) + 1);
	if (globals.flg_bitmap == NULL) abort();

	/* map mem4 */
	globals.buildings = malloc(globals.max_building_cnt * sizeof(building_t));
	if (globals.buildings == NULL) abort();

	globals.buildings_bitmap = malloc(((globals.max_building_cnt-1) / 8) + 1);
	if (globals.buildings_bitmap == NULL) abort();

	globals.inventories = malloc(globals.max_inventory_cnt * sizeof(inventory_t));
	if (globals.inventories == NULL) abort();

	globals.inventories_bitmap = malloc(((globals.max_inventory_cnt-1) / 8) + 1);
	if (globals.inventories_bitmap == NULL) abort();

	/* Setup frames */
	/* TODO ... */

	frame_t *screen = sdl_get_screen_frame();

	/*
	sdl_frame_init(&lowres_full_frame, 0, 0, 352, 240, screen);
	sdl_frame_init(&lowres_normal_frame, 16, 8, 320, 192, screen);
	*/

	/* TODO ...*/

	sdl_frame_init(&svga_full_frame, 0, 0, sdl_frame_get_width(screen),
		       sdl_frame_get_height(screen), screen);
	sdl_frame_init(&svga_normal_frame, 0, 0, sdl_frame_get_width(screen),
		       sdl_frame_get_height(screen), screen);

	/* TODO ... */

	/*sdl_frame_init(&game_area_lowres_frame, 0, 0, 352, 192, screen);*/
	sdl_frame_init(&game_area_svga_frame, 0, 0, sdl_frame_get_width(screen),
		       sdl_frame_get_height(screen), screen);

	sdl_frame_init(&popup_box_left_frame, 0, 0, 144, 160, NULL);
	/*sdl_frame_init_new(&popup_box_right_frame, 0, 0, 144, 160);*/

	pregame_init();
}

/* No need to supply an arg to this function as main_timer_cb doesn't call
   this anyway, it calls update_game directly, so there's only one type of
   call to this which is from main. */
static void
deep_tree()
{
	/* TODO init mouse and joystick devices */
	globals.cfg_left = 0x39;
	globals.cfg_right = 0x39;

	/* allocate memory for maps */
	/* TODO ... */

	globals.map_mem2 = malloc(0x2314 * (1 << /*max_map_size+2(?)*/10));
	if (globals.map_mem2 == NULL) abort();

	/* TODO ... */

	hand_out_memory_2();
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
			game_file = malloc(strlen(optarg)+1);
			if (game_file == NULL) exit(EXIT_FAILURE);
			strcpy(game_file, optarg);
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

	/*gfx_set_palette(DATA_PALETTE_INTRO);*/
	gfx_set_palette(DATA_PALETTE_GAME);

	LOGI("main", "SDL resolution %ix%i...", screen_width, screen_height);

	r = sdl_set_resolution(screen_width, screen_height, fullscreen);
	if (r < 0) exit(EXIT_FAILURE);

	globals.svga |= BIT(7); /* set svga mode */

	globals.mission_level = game_map - 1; /* set game map */
	globals.map_generator = map_generator;
	globals.map_preserve_bugs = preserve_map_bugs;

	deep_tree();

	LOGI("main", "Cleaning up...");

	/* Clean up */
	audio_cleanup();
	sdl_deinit();
	gfx_unload();

	return EXIT_SUCCESS;
}
