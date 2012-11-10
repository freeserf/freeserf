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
#include "viewport.h"
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

static int redraw_landscape;

static frame_t svga_full_frame;
static frame_t game_area_svga_frame;
static frame_t svga_normal_frame;

static frame_t popup_box_left_frame;

/* Viewport holds the state of the main map window
   (e.g. size and current map location). */
static viewport_t viewport;

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
	viewport.width = width;
	viewport.height = height;
	viewport.layers = VIEWPORT_LAYER_ALL;

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
	globals.field_286 = 33;

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
				globals.field_286 = 49;
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

	/* Landscape animation does not care about low bits in anim */
	if (anim_xor >= 1 << 3) redraw_landscape = 1;

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

		int dx = x - player->pointer_x;
		int dy = y - player->pointer_y;
		if (dx != 0 || dy != 0) {
			/* TODO drag accelerates in linux */
			viewport_move_by_pixels(&viewport, dx, dy);
			SDL_WarpMouse(player->pointer_x, player->pointer_y);
		}
	} else {
		player->click &= ~BIT(3); /* reset right click pending */

		if (x != player->pointer_x || y != player->pointer_y) {
			player->pointer_x = min(max(0, x), player->pointer_x_max);
			player->pointer_y = min(max(0, y), player->pointer_y_max);
		}
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

/* Determine what buildings can possibly be built at map_pos[0]. */
static void
determine_possible_building(const player_sett_t *sett, map_pos_t map_pos[], int hills,
			    panel_btn_t *panel_btn, int *build_flags, int *height_after_level)
{
	if (hills) {
		if (BIT_TEST(sett->flags, 0)) { /* Has castle */
			*panel_btn = PANEL_BTN_BUILD_MINE;
		}
	} else {
		if (BIT_TEST(sett->flags, 0)) { /* Has castle */
			*panel_btn = PANEL_BTN_BUILD_SMALL;
		}

		/* Check for adjacent military building */
		*build_flags &= ~BIT(0); /* Can build military building */
		for (int i = 0; i < 12; i++) {
			map_obj_t obj = MAP_OBJ(map_pos[7+i]);
			if (obj >= MAP_OBJ_SMALL_BUILDING &&
			    obj <= MAP_OBJ_CASTLE) {
				building_t *bld = game_get_building(MAP_OBJ_INDEX(map_pos[7+i]));
				building_type_t bld_type = BUILDING_TYPE(bld);
				if (bld_type == BUILDING_HUT ||
				    bld_type == BUILDING_TOWER ||
				    bld_type == BUILDING_FORTRESS ||
				    bld_type == BUILDING_CASTLE) {
					*build_flags |= BIT(0); /* Can not build military building */
					break;
				}
			}
		}

		/* Check that surroundings are passable by serfs. */
		for (int i = 0; i < 6; i++) {
			map_space_t s = map_space_from_obj[MAP_OBJ(map_pos[1+i])];
			if (s >= MAP_SPACE_IMPASSABLE && s != MAP_SPACE_FLAG) return;
		}

		/* Check that buildings in the second shell aren't large or castle. */
		for (int i = 0; i < 12; i++) {
			map_space_t s = map_space_from_obj[MAP_OBJ(map_pos[7+i])];
			if (s >= MAP_SPACE_LARGE_BUILDING) return;
		}

		/* Check if center hexagon is not type 5 (grass?) */
		if (MAP_TYPE_UP(map_pos[0]) != 5 ||
		    MAP_TYPE_DOWN(map_pos[0]) != 5 ||
		    MAP_TYPE_DOWN(map_pos[1+DIR_LEFT]) != 5 ||
		    MAP_TYPE_UP(map_pos[1+DIR_UP_LEFT]) != 5 ||
		    MAP_TYPE_DOWN(map_pos[1+DIR_UP_LEFT]) != 5 ||
		    MAP_TYPE_UP(map_pos[1+DIR_UP]) != 5) {
			return;
		}

		/* Find min and max height */
		int h_min = 31;
		int h_max = 0;
		for (int i = 0; i < 12; i++) {
			int h = MAP_HEIGHT(map_pos[7+i]);
			if (h_min > h) h_min = h;
			if (h_max < h) h_max = h;
		}

		/* Adjust for height of adjacent unleveled buildings */
		for (int i = 0; i < 18; i++) {
			if (MAP_OBJ(map_pos[19+i]) == MAP_OBJ_LARGE_BUILDING) {
				building_t *bld = game_get_building(MAP_OBJ_INDEX(map_pos[19+i]));
				if (BIT_TEST(bld->bld, 7) && /* Unfinished */
				    bld->progress == 0) { /* Leveling in progress */
					int h = bld->u.s.level;
					if (h_min > h) h_min = h;
					if (h_max < h) h_max = h;
				}
			}
		}

		/* Return if height difference is too big */
		if (h_max - h_min >= 9) return;

		/* Calculate "mean" height. Height of center is added twice. */
		int h_mean = MAP_HEIGHT(map_pos[0]);
		for (int i = 0; i < 7; i++) {
			h_mean += MAP_HEIGHT(map_pos[i]);
		}
		h_mean >>= 3;

		/* Calcualte height after leveling */
		int h_new_min = max((h_max > 4) ? (h_max - 4) : 1, 1);
		int h_new_max = h_min + 4;
		int h_new = clamp(h_new_min, h_mean, h_new_max);
		*height_after_level = h_new;

		if (BIT_TEST(sett->flags, 0)) { /* Has castle */
			*panel_btn = PANEL_BTN_BUILD_LARGE;
		} else {
			*panel_btn = PANEL_BTN_BUILD_CASTLE;
		}
	}
}

static int
determine_map_cursor_type_sub(int type)
{
	if (type >= 4 && type < 8) return 0;
	else if (type >= 11 && type < 15) return BIT(0);
	return BIT(1);
}

/* Initialize an array of map_pos_t following a spiral pattern based in init_pos. */
static void
populate_circular_map_pos_array(map_pos_t map_pos[], map_pos_t init_pos, int size)
{
	for (int i = 0; i < size; i++) {
		map_pos[i] = (init_pos + globals.spiral_pos_pattern[i]) & globals.map_index_mask;
	}
}

/* Return the cursor type and various related values of a map_pos_t. */
static void
get_map_cursor_type(const player_sett_t *sett, map_pos_t pos, panel_btn_t *panel_btn,
		    int *build_flags, int *cursor_type, int *height_after_level)
{
	map_pos_t map_pos[1+6+12+18];
	populate_circular_map_pos_array(map_pos, pos, 1+6+12+18);

	int player = sett->player_num;
	if (!BIT_TEST(sett->flags, 0)) player = -1; /* Has no castle */

	if (player >= 0 &&
	    (!MAP_HAS_OWNER(map_pos[0]) ||
	     MAP_OWNER(map_pos[0]) != player)) {
		return;
	}

	if (map_space_from_obj[MAP_OBJ(map_pos[0])] == MAP_SPACE_FLAG) {
		if (BIT_TEST(MAP_PATHS(map_pos[0]), DIR_UP_LEFT) &&
		    map_space_from_obj[MAP_OBJ(map_pos[1+DIR_UP_LEFT])] >= MAP_SPACE_SMALL_BUILDING) {
			*cursor_type = 1;
			return;
		}

		if (MAP_PATHS(map_pos[0]) == 0) {
			*cursor_type = 2;
			return;
		}

		flag_t *flag = game_get_flag(MAP_OBJ_INDEX(map_pos[0]));
		int connected = 0;
		void *other_end = NULL;

		for (int i = DIR_UP; i >= DIR_RIGHT; i--) {
			if (BIT_TEST(flag->path_con, i)) {
				if (!BIT_TEST(flag->endpoint, i)) {
					*cursor_type = 1;
					return;
				}

			        connected += 1;

				if (other_end != NULL) {
					if (flag->other_endpoint.v[i] == other_end) {
						*cursor_type = 1;
						return;
					}
				} else {
					other_end = flag->other_endpoint.v[i];
				}

				if (i >= DIR_UP_LEFT) {
					i = DIR_DOWN_RIGHT;
				} else {
					break;
				}
			}
		}

		if (connected == 2) *cursor_type = 2;
		else *cursor_type = 1;
	} else if (map_space_from_obj[MAP_OBJ(map_pos[0])] < MAP_SPACE_SMALL_BUILDING) {
		int paths = MAP_PATHS(map_pos[0]);
		if (paths == 0) {
			if (map_space_from_obj[MAP_OBJ(map_pos[1+DIR_DOWN_RIGHT])] == MAP_SPACE_FLAG) {
				*cursor_type = 5;
			} else if (MAP_PATHS(map_pos[1+DIR_DOWN_RIGHT]) == 0) {
				*cursor_type = 7;
			} else {
				*cursor_type = 6;
			}
		} else if (paths == BIT(DIR_DOWN_RIGHT) ||
			   paths == BIT(DIR_UP_LEFT)) {
			return;
		} else {
			*cursor_type = 4;
		}

		if (map_space_from_obj[MAP_OBJ(map_pos[0])] != MAP_SPACE_OPEN) return;

		/* Return if cursor is in water */
		if (MAP_TYPE_UP(map_pos[0]) < 4 &&
		    MAP_TYPE_DOWN(map_pos[0]) < 4 &&
		    MAP_TYPE_DOWN(map_pos[1+DIR_LEFT]) < 4 &&
		    MAP_TYPE_UP(map_pos[1+DIR_UP_LEFT]) < 4 &&
		    MAP_TYPE_DOWN(map_pos[1+DIR_UP_LEFT]) < 4 &&
		    MAP_TYPE_UP(map_pos[1+DIR_UP]) < 4) {
			return;
		}

		int found = 0;
		for (int i = 0; i < 6; i++) {
			if (map_space_from_obj[MAP_OBJ(map_pos[1+i])] == MAP_SPACE_FLAG) {
				if (*cursor_type == 4) return;
				found = 1;
				break;
			}
		}

		if (!found) {
			*build_flags &= ~BIT(1);
			if (BIT_TEST(sett->flags, 0)) { /* Has castle */
				*panel_btn = PANEL_BTN_BUILD_FLAG;
				if (*cursor_type == 4) return;
			}
		}

		for (int i = 0; i < 6; i++) {
			if (map_space_from_obj[MAP_OBJ(map_pos[1+i])] >= MAP_SPACE_SMALL_BUILDING) return;
		}

		if (*cursor_type != 5) {
			if (map_space_from_obj[MAP_OBJ(map_pos[1+DIR_DOWN_RIGHT])] != MAP_SPACE_OPEN) return;
		}

		/* bleh */
		if (map_space_from_obj[MAP_OBJ(map_pos[7])] == MAP_SPACE_FLAG ||
		    map_space_from_obj[MAP_OBJ(map_pos[8])] == MAP_SPACE_FLAG ||
		    map_space_from_obj[MAP_OBJ(map_pos[14])] == MAP_SPACE_FLAG ||
		    map_space_from_obj[MAP_OBJ(map_pos[1+DIR_RIGHT])] == MAP_SPACE_FLAG ||
		    map_space_from_obj[MAP_OBJ(map_pos[1+DIR_DOWN])] == MAP_SPACE_FLAG) {
			return;
		}

		/* bleh */
		if (MAP_TYPE_UP(map_pos[1+DIR_RIGHT]) < 4 ||
		    MAP_TYPE_DOWN(map_pos[1+DIR_DOWN]) < 4 ||
		    MAP_TYPE_UP(map_pos[1+DIR_DOWN_RIGHT]) < 4 ||
		    MAP_TYPE_DOWN(map_pos[1+DIR_DOWN_RIGHT]) < 4) {
			return;
		}

		/* 426FD: Check owner of surrounding land */
		for (int i = 0; i < 6; i++) {
			if (player >= 0 &&
			    (!MAP_HAS_OWNER(map_pos[1+i]) ||
			     MAP_OWNER(map_pos[1+i]) != player)) {
				return;
			}
		}

		int bits = 0;
		bits |= determine_map_cursor_type_sub(MAP_TYPE_UP(map_pos[0]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_DOWN(map_pos[0]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_DOWN(map_pos[1+DIR_LEFT]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_UP(map_pos[1+DIR_UP_LEFT]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_DOWN(map_pos[1+DIR_UP_LEFT]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_UP(map_pos[1+DIR_UP]));

		if (bits < 2) {
			determine_possible_building(sett, map_pos, bits,
						    panel_btn, build_flags,
						    height_after_level);
		}
	} else if (map_space_from_obj[MAP_OBJ(map_pos[0])] != MAP_SPACE_CASTLE) {
		building_t *bld = game_get_building(MAP_OBJ_INDEX(map_pos[0]));
		if (BIT_TEST(bld->serf, 5)) return; /* Building burning */

		*cursor_type = 3;

		/* 426FD: Check owner of surrounding land */
		for (int i = 0; i < 6; i++) {
			if (player >= 0 &&
			    (!MAP_HAS_OWNER(map_pos[1+i]) ||
			     MAP_OWNER(map_pos[1+i]) != player)) {
				return;
			}
		}

		int bits = 0;
		bits |= determine_map_cursor_type_sub(MAP_TYPE_UP(map_pos[0]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_DOWN(map_pos[0]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_DOWN(map_pos[1+DIR_LEFT]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_UP(map_pos[1+DIR_UP_LEFT]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_DOWN(map_pos[1+DIR_UP_LEFT]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_UP(map_pos[1+DIR_UP]));

		if (bits < 2) {
			determine_possible_building(sett, map_pos, bits,
						    panel_btn, build_flags,
						    height_after_level);
		}
	}
}

/* Update the player_t object with the information returned
   in get_map_cursor_type(). */
static void
determine_map_cursor_type(player_t *player)
{
	player->sett->build |= BIT(1); /* Can not build flag */

	player->sett->map_cursor_type = 0;
	player->sett->panel_btn_type = PANEL_BTN_BUILD_INACTIVE;

	map_pos_t cursor_pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
	get_map_cursor_type(player->sett, cursor_pos,
			    &player->sett->panel_btn_type,
			    &player->sett->build,
			    &player->sett->map_cursor_type,
			    &player->sett->building_height_after_level);
}

/* Update the player_t object with the information returned
   in get_map_cursor_type(). This is sets the appropriate values
   when the player interface is in road construction mode. */
static void
determine_map_cursor_type_road(player_t *player)
{
	map_pos_t cursor_pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
	map_pos_t map_pos[1+6];
	populate_circular_map_pos_array(map_pos, cursor_pos, 1+6);

	int h = MAP_HEIGHT(map_pos[0]);
	int bits = 0;
	int paths = 0;
	if (player->road_length > 0) paths = MAP_PATHS(map_pos[0]);

	for (int i = 0; i < 6; i++) {
		int sprite = 0;

		if (MAP_HAS_OWNER(map_pos[1+i]) &&
		    MAP_OWNER(map_pos[1+i]) == player->sett->player_num) {
			if (!BIT_TEST(paths, i)) {
				if (map_space_from_obj[MAP_OBJ(map_pos[1+i])] == MAP_SPACE_IMPASSABLE ||
				    map_space_from_obj[MAP_OBJ(map_pos[1+i])] == MAP_SPACE_SMALL_BUILDING) {
					sprite = 44; /* striped */
				} else if (map_space_from_obj[MAP_OBJ(map_pos[1+i])] == MAP_SPACE_FLAG ||
					   MAP_PATHS(map_pos[1+i]) == 0) {
					int h_diff = MAP_HEIGHT(map_pos[1+i]) - h;
					sprite = 39 + h_diff; /* height indicators */
					bits |= BIT(i);
				} else {
					panel_btn_t panel_btn;
					int build_flags, cursor_type, height_after_level;
					get_map_cursor_type(player->sett, map_pos[1+i],
							    &panel_btn, &build_flags,
							    &cursor_type, &height_after_level);
					if (BIT_TEST(build_flags, 1) ||
					    panel_btn == PANEL_BTN_BUILD_INACTIVE ||
					    /*check_can_build_flag_on_road(map_pos[1+i]) < 0*/1) {
						sprite = 44; /* striped */
					} else {
						int h_diff = MAP_HEIGHT(map_pos[1+i]) - h;
						sprite = 39 + h_diff; /* height indicators */
						bits |= BIT(i);
					}
				}
			} else {
				sprite = 45; /* undo */
				bits |= BIT(i);
			}
		} else {
			sprite = 44; /* striped */
		}
		player->map_cursor_sprites[i+1].sprite = sprite;
	}

	player->field_D0 = bits;
}

/* Set the appropriate sprites for the panel buttons and the map cursor. */
static void
update_panel_btns_and_map_cursor(player_t *player)
{
	if (/*not demo mode*/1) {
		if (BIT_TEST(player->click, 7)) { /* Building road */
			player->panel_btns[0] = PANEL_BTN_BUILD_ROAD_STARRED;
			player->panel_btns[1] = PANEL_BTN_BUILD_INACTIVE;
		} else {
			switch (player->sett->map_cursor_type) {
				case 0:
					player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
					if (BIT_TEST(player->sett->flags, 0)) { /* Has castle */
						player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					} else {
						player->panel_btns[1] = PANEL_BTN_GROUND_ANALYSIS;
					}
					player->map_cursor_sprites[0].sprite = 32;
					player->map_cursor_sprites[2].sprite = 33;
					break;
				case 1:
					player->panel_btns[0] = PANEL_BTN_BUILD_ROAD;
					player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					player->map_cursor_sprites[0].sprite = 51;
					player->map_cursor_sprites[2].sprite = 33;
					break;
				case 2:
					player->panel_btns[0] = PANEL_BTN_BUILD_ROAD;
					player->panel_btns[1] = PANEL_BTN_DESTROY;
					player->map_cursor_sprites[0].sprite = 51;
					player->map_cursor_sprites[2].sprite = 33;
					break;
				case 3:
					player->panel_btns[0] = player->sett->panel_btn_type;
					player->panel_btns[1] = PANEL_BTN_DESTROY;
					player->map_cursor_sprites[0].sprite = 46 + player->sett->panel_btn_type;
					player->map_cursor_sprites[2].sprite = 33;
					break;
				case 4:
					player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
					player->panel_btns[1] = PANEL_BTN_DESTROY_ROAD;
					player->map_cursor_sprites[0].sprite = 52;
					player->map_cursor_sprites[2].sprite = 33;
					if (player->sett->panel_btn_type != PANEL_BTN_BUILD_INACTIVE) {
						player->panel_btns[0] = PANEL_BTN_BUILD_FLAG;
						player->map_cursor_sprites[0].sprite = 47;
					}
					break;
				case 5:
					if (player->sett->panel_btn_type < PANEL_BTN_BUILD_MINE) {
						/* TODO */
						LOGD("gui", "cursor type: unhandled 5 case.");
					} else {
						player->panel_btns[0] = player->sett->panel_btn_type;
						player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
						player->map_cursor_sprites[0].sprite = 46 + player->sett->panel_btn_type;
						player->map_cursor_sprites[2].sprite = 33;
					}
					break;
				case 6:
					player->panel_btns[0] = player->sett->panel_btn_type;
					player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					if (player->sett->panel_btn_type != PANEL_BTN_BUILD_INACTIVE) {
						player->map_cursor_sprites[0].sprite = 46 + player->sett->panel_btn_type;
						if (player->sett->panel_btn_type == PANEL_BTN_BUILD_FLAG) {
							player->map_cursor_sprites[2].sprite = 33;
						} else {
							player->map_cursor_sprites[2].sprite = 47;
						}
					} else {
						player->map_cursor_sprites[0].sprite = 32;
						player->map_cursor_sprites[2].sprite = 33;
					}
					break;
				case 7:
					player->panel_btns[0] = player->sett->panel_btn_type;
					if (BIT_TEST(player->sett->flags, 0)) { /* Has castle */
						player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					} else {
						player->panel_btns[1] = PANEL_BTN_GROUND_ANALYSIS;
					}
					if (player->sett->panel_btn_type) {
						if (player->sett->panel_btn_type == PANEL_BTN_BUILD_CASTLE) {
							player->map_cursor_sprites[0].sprite = 50;
						} else {
							player->map_cursor_sprites[0].sprite = 46 + player->sett->panel_btn_type;
						}
						if (player->sett->panel_btn_type == PANEL_BTN_BUILD_FLAG) {
							player->map_cursor_sprites[2].sprite = 33;
						} else {
							player->map_cursor_sprites[2].sprite = 47;
						}
					} else {
						player->map_cursor_sprites[0].sprite = 32;
						player->map_cursor_sprites[2].sprite = 33;
					}
					break;
				default:
					NOT_REACHED();
					break;
			}
		}
	} else {
		/* TODO demo mode */
	}
}

/* Close the popup box for player. */
static void
close_box(player_t *player)
{
	player->click &= ~BIT(6);
	player->panel_btns[2] = PANEL_BTN_MAP;
	player->panel_btns[3] = PANEL_BTN_STATS;
	player->panel_btns[4] = PANEL_BTN_SETT;
	player->click |= BIT(1);
	player->clkmap = 0;
	player->click |= BIT(2);
}

/* Calculate the flag state of military buildings (distance to enemy). */
void
calculate_military_flag_state(building_t *building)
{
	const int border_check_offsets[] = {
		31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
		100, 101, 102, 103, 104, 105, 106, 107, 108,
		259, 260, 261, 262, 263, 264,
		241, 242, 243, 244, 245, 246,
		217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228,
		247, 248, 249, 250, 251, 252,
		-1,

		265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276,
		-1,

		277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288,
		289, 290, 291, 292, 293, 294,
		-1
	};

	int f, k;
	for (f = 3, k = 0; f > 0; f--) {
		int offset;
		while ((offset = border_check_offsets[k++]) >= 0) {
			map_pos_t check_pos =
				(building->pos + globals.spiral_pos_pattern[offset]) &
				globals.map_index_mask;
			if (MAP_HAS_OWNER(check_pos) &&
			    MAP_OWNER(check_pos) != BUILDING_PLAYER(building)) {
				goto break_loops;
			}
		}
	}

break_loops:
	building->serf = (building->serf & 0xfc) | f;
}

/* Update land ownership around col,row. */
void
update_land_ownership(int col, int row)
{
	/* Currently the below algorithm will only work when
	   both influence_radius and calculate_radius are 8. */
	const int influence_radius = 8;
	const int influence_diameter = 1 + 2*influence_radius;

	int calculate_radius = influence_radius;
	int calculate_diameter = 1 + 2*calculate_radius;

	int *temp_arr = calloc(4*calculate_diameter*calculate_diameter, sizeof(int));
	if (temp_arr == NULL) abort();

	const int military_influence[] = {
		0, 1, 2, 4, 7, 12, 18, 29, -1, -1,	/* hut */
		0, 3, 5, 8, 11, 15, 22, 30, -1, -1,	/* tower */
		0, 6, 10, 14, 19, 23, 27, 31, -1, -1	/* fortress */
	};

	const int map_closeness[] = {
		1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
		1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0,
		1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, 0, 0, 0, 0, 0,
		1, 2, 3, 4, 4, 4, 4, 4, 4, 3, 2, 1, 0, 0, 0, 0, 0,
		1, 2, 3, 4, 5, 5, 5, 5, 5, 4, 3, 2, 1, 0, 0, 0, 0,
		1, 2, 3, 4, 5, 6, 6, 6, 6, 5, 4, 3, 2, 1, 0, 0, 0,
		1, 2, 3, 4, 5, 6, 7, 7, 7, 6, 5, 4, 3, 2, 1, 0, 0,
		1, 2, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 2, 1, 0,
		1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 7, 6, 5, 4, 3, 2, 1,
		0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 2, 1,
		0, 0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 6, 5, 4, 3, 2, 1,
		0, 0, 0, 1, 2, 3, 4, 5, 6, 6, 6, 6, 5, 4, 3, 2, 1,
		0, 0, 0, 0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 4, 3, 2, 1,
		0, 0, 0, 0, 0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 3, 2, 1,
		0, 0, 0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1,
		0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1,
		0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1
	};

	/* Find influence from buildings in 33*33 square
	   around the center. */
	for (int i = -(influence_radius+calculate_radius);
	     i <= influence_radius+calculate_radius; i++) {
		for (int j = -(influence_radius+calculate_radius);
		     j <= influence_radius+calculate_radius; j++) {
			int c = (col + j) & globals.map_col_mask;
			int r = (row + i) & globals.map_row_mask;

			map_pos_t pos = MAP_POS(c, r);

			if (MAP_OBJ(pos) >= MAP_OBJ_SMALL_BUILDING &&
			    MAP_OBJ(pos) <= MAP_OBJ_CASTLE &&
			    BIT_TEST(MAP_PATHS(pos), DIR_DOWN_RIGHT)) { /* TODO Why wouldn't this be set? */
				building_t *building = game_get_building(MAP_OBJ_INDEX(pos));
				int mil_type = -1;

				if (BUILDING_TYPE(building) == BUILDING_CASTLE) {
					/* Castle has military influence even when not done. */
					mil_type = 2;
				} else if (BUILDING_IS_DONE(building) &&
					   BIT_TEST(building->serf, 4)) { /* Working */
					switch (BUILDING_TYPE(building)) {
					case BUILDING_HUT: mil_type = 0; break;
					case BUILDING_TOWER: mil_type = 1; break;
					case BUILDING_FORTRESS: mil_type = 2; break;
					default: break;
					}
				}

				if (mil_type >= 0 &&
				    !BUILDING_IS_BURNING(building)) {
					const int *influence = military_influence + 10*mil_type;
					const int *closeness = map_closeness +
						influence_diameter*max(-i, 0) + max(-j, 0);
					int *arr = temp_arr +
						(BUILDING_PLAYER(building) * calculate_diameter*calculate_diameter) +
						calculate_diameter*max(i, 0) + max(j, 0);

					for (int k = 0; k < influence_diameter - abs(i); k++) {
						for (int l = 0; l < influence_diameter - abs(j); l++) {
							int inf = influence[*closeness];
							if (inf < 0) *arr = 128;
							else if (*arr < 128) *arr = min(*arr + inf, 127);

							closeness += 1;
							arr += 1;
						}
						closeness += abs(j);
						arr += abs(j);
					}
				}
			}
		}
	}

	map_1_t *map = globals.map_mem2_ptr;

	/* Update owner of 17*17 square. */
	for (int i = -calculate_radius; i <= calculate_radius; i++) {
		for (int j = -calculate_radius; j <= calculate_radius; j++) {
			int max_val = 0;
			int player = -1;
			for (int p = 0; p < 4; p++) {
				int *arr = temp_arr +
					p*calculate_diameter*calculate_diameter +
					calculate_diameter*(i+calculate_radius) + (j+calculate_radius);
				if (*arr > max_val) {
					max_val = *arr;
					player = p;
				}
			}

			int c = (col + j) & globals.map_col_mask;
			int r = (row + i) & globals.map_row_mask;
			map_pos_t pos = MAP_POS(c, r);

			if (player >= 0) {
				if (MAP_HAS_OWNER(pos)) {
					int old_player = MAP_OWNER(pos);
					globals.player_sett[old_player]->total_land_area -= 1;
					/* TODO sub_5630A(); surrender land */
				}

				globals.player_sett[player]->total_land_area += 1;
				map[pos].height = (1 << 7) | (player << 5) | MAP_HEIGHT(pos);
			}
		}
	}

	free(temp_arr);

	/* update_military_flag_state() */
	/* Update military building flag state. */
	for (int i = -25; i <= 25; i++) {
		for (int j = -25; j <= 25; j++) {
			int c = (col + i) & globals.map_col_mask;
			int r = (row + j) & globals.map_row_mask;

			map_pos_t pos = MAP_POS(c, r);

			if (MAP_OBJ(pos) >= MAP_OBJ_SMALL_BUILDING &&
			    MAP_OBJ(pos) <= MAP_OBJ_CASTLE &&
			    BIT_TEST(MAP_PATHS(pos), DIR_DOWN_RIGHT)) {
				building_t *building = game_get_building(MAP_OBJ_INDEX(pos));
				if ((BUILDING_IS_DONE(building) &&
				     (BUILDING_TYPE(building) == BUILDING_HUT ||
				      BUILDING_TYPE(building) == BUILDING_TOWER ||
				      BUILDING_TYPE(building) == BUILDING_FORTRESS)) ||
				    BUILDING_TYPE(building) == BUILDING_CASTLE) {
					calculate_military_flag_state(building);
				}
			}
		}
	}
}

/* Start road construction mode for player interface. */
static void
build_road_begin(player_t *player)
{
	player->flags &= ~BIT(6);

	determine_map_cursor_type(player);

	if (player->sett->map_cursor_type != 1 &&
	    player->sett->map_cursor_type != 2) {
		update_panel_btns_and_map_cursor(player);
		return;
	}

	player->panel_btns[0] = PANEL_BTN_BUILD_ROAD_STARRED;
	player->panel_btns[1] = PANEL_BTN_BUILD_INACTIVE;
	player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
	player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
	player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
	player->click |= BIT(6);
	player->click |= BIT(7);
	player->click |= BIT(2);
	player->road_length = 0;
}

/* End road construction mode for player interface. */
static void
build_road_end(player_t *player)
{
	player->click &= ~BIT(6);
	player->panel_btns[2] = PANEL_BTN_MAP;
	player->panel_btns[3] = PANEL_BTN_STATS;
	player->panel_btns[4] = PANEL_BTN_SETT;
	player->click &= ~BIT(7);
	player->click |= BIT(2);

	player->map_cursor_sprites[1].sprite = 33;
	player->map_cursor_sprites[2].sprite = 33;
	player->map_cursor_sprites[3].sprite = 33;
	player->map_cursor_sprites[4].sprite = 33;
	player->map_cursor_sprites[5].sprite = 33;
	player->map_cursor_sprites[6].sprite = 33;

	map_1_t *map = globals.map_mem2_ptr;
	map_pos_t pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);

	for (int i = 0; i < player->road_length; i++) {
		dir_t backtrack_dir = -1;
		for (dir_t d = 0; d < 6; d++) {
			if (BIT_TEST(map[pos].flags, d)) {
				backtrack_dir = d;
				break;
			}
		}

		map_pos_t next_pos = MAP_MOVE(pos, backtrack_dir);

		map[pos].flags &= ~BIT(backtrack_dir);
		map[next_pos].flags &= ~BIT(DIR_REVERSE(backtrack_dir));
		pos = next_pos;
	}

	/* TODO set_map_redraw(); */
}

/* Get road length category value for real length.
   Determines number of serfs servicing the path segment.(?) */
static int
get_road_length_value(int length)
{
	if (length >= 24) return 7 << 4;
	else if (length >= 18) return 6 << 4;
	else if (length >= 13) return 5 << 4;
	else if (length >= 10) return 4 << 4;
	else if (length >= 7) return 3 << 4;
	else if (length >= 6) return 2 << 4;
	else if (length >= 4) return 1 << 4;
	return 0;
}

/* Connect a road under construction to an existing flag. */
static int
build_road_connect_flag(player_t *player, map_1_t *map, map_pos_t clk_pos, dir_t out_dir)
{
	if (!MAP_HAS_OWNER(clk_pos) || MAP_OWNER(clk_pos) != player->sett->player_num) {
		return -1;
	}

	map_pos_t next_pos = clk_pos;
	dir_t in_dir = -1;

	flag_t *src = game_get_flag(MAP_OBJ_INDEX(clk_pos));

	int paths = BIT(out_dir);
	int test = 0;

	/* Backtrack along path to other flag. Test along the way
	   whether the path is on ground or in water. */
	for (int i = 0; i < player->road_length + 1; i++) {
		if (BIT_TEST(paths, DIR_RIGHT)) {
			if (MAP_TYPE_UP(next_pos) < 4 &&
			    MAP_TYPE_UP(MAP_MOVE_UP(next_pos)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_LEFT;
			next_pos = MAP_MOVE_RIGHT(next_pos);
		} else if (BIT_TEST(paths, DIR_DOWN_RIGHT)) {
			if (MAP_TYPE_UP(next_pos) < 4 &&
			    MAP_TYPE_DOWN(next_pos) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_UP_LEFT;
			next_pos = MAP_MOVE_DOWN_RIGHT(next_pos);
		} else if (BIT_TEST(paths, DIR_DOWN)) {
			if (MAP_TYPE_UP(next_pos) < 4 &&
			    MAP_TYPE_DOWN(MAP_MOVE_LEFT(next_pos)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_UP;
			next_pos = MAP_MOVE_DOWN(next_pos);
		} else if (BIT_TEST(paths, DIR_LEFT)) {
			if (MAP_TYPE_DOWN(MAP_MOVE_LEFT(next_pos)) < 4 &&
			    MAP_TYPE_UP(MAP_MOVE_UP(next_pos)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_RIGHT;
			next_pos = MAP_MOVE_LEFT(next_pos);
		} else if (BIT_TEST(paths, DIR_UP_LEFT)) {
			if (MAP_TYPE_UP(MAP_MOVE_UP_LEFT(next_pos)) < 4 &&
			    MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(next_pos)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_DOWN_RIGHT;
			next_pos = MAP_MOVE_UP_LEFT(next_pos);
		} else if (BIT_TEST(paths, DIR_UP)) {
			if (MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(next_pos)) < 4 &&
			    MAP_TYPE_UP(MAP_MOVE_RIGHT(next_pos)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_DOWN;
			next_pos = MAP_MOVE_UP(next_pos);
		}

		if (!MAP_HAS_OWNER(next_pos) || MAP_OWNER(next_pos) != player->sett->player_num) {
			return -1;
		}

		paths = MAP_PATHS(next_pos) & ~BIT(in_dir);
	}

	/* Bit 0 indicates a ground path, bit 1 indicates
	   water path. Abort if path went through both
	   ground and water. */
	int water_path = 0;
	if (test != BIT(0)) {
		water_path = 1;
		if (test != BIT(1)) return -1;
	}

	/* Connect flags */
	flag_t *dest = game_get_flag(MAP_OBJ_INDEX(next_pos));

	src->path_con |= BIT(out_dir);
	src->endpoint |= BIT(out_dir);
	src->transporter &= ~BIT(out_dir);

	dest->path_con |= BIT(in_dir);
	dest->endpoint |= BIT(in_dir);
	dest->transporter &= ~BIT(in_dir);

	if (water_path) {
		src->endpoint &= ~BIT(out_dir);
		dest->endpoint &= ~BIT(in_dir);
	}

	src->other_end_dir[out_dir] = (src->other_end_dir[out_dir] & 0xc7) | (in_dir << 3);
	dest->other_end_dir[in_dir] = (dest->other_end_dir[in_dir] & 0xc7) | (out_dir << 3);

	int len = get_road_length_value(player->road_length + 1);

	src->length[out_dir] = len;
	dest->length[in_dir] = len;

	src->other_endpoint.f[out_dir] = dest;
	dest->other_endpoint.f[in_dir] = src;

	return 0;
}

/* Find a transporter at pos and change it to state. */
static int
change_transporter_state_at_pos(map_pos_t pos, serf_state_t state)
{
	for (int i = 1; i < globals.max_ever_serf_index; i++) {
		if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
			serf_t *serf = game_get_serf(i);
			if (serf->pos == pos &&
			    (serf->state == SERF_STATE_WAKE_AT_FLAG ||
			     serf->state == SERF_STATE_WAKE_ON_PATH ||
			     serf->state == SERF_STATE_WAIT_IDLE_ON_PATH ||
			     serf->state == SERF_STATE_IDLE_ON_PATH)) {
				serf_log_state_change(serf, state);
				serf->state = state;
				return SERF_INDEX(serf);
			}
		}
	}

	return -1;
}

static int
wake_transporter_at_flag(map_pos_t pos)
{
	return change_transporter_state_at_pos(pos, SERF_STATE_WAKE_AT_FLAG);
}

static int
wake_transporter_on_path(map_pos_t pos)
{
	return change_transporter_state_at_pos(pos, SERF_STATE_WAKE_ON_PATH);
}

typedef struct {
	int path_len;
	int serf_count;
	int flag_index;
	dir_t flag_dir;
	int serfs[16];
} serf_path_info_t;

static void
fill_path_serf_info(map_pos_t pos, dir_t dir, serf_path_info_t *data)
{
	if (MAP_IDLE_SERF(pos)) wake_transporter_at_flag(pos);

	int serf_count = 0;
	int path_len = 0;

	/* Handle first position. */
	if (MAP_SERF_INDEX(pos) != 0) {
		serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));
		if (serf->state == SERF_STATE_TRANSPORTING &&
		    serf->s.walking.wait_counter != -1) {
			dir_t d = serf->s.walking.dir;
			if (d < 0) d += 6;

			if (dir == d) {
				serf->s.walking.wait_counter = 0;
				data->serfs[serf_count++] = SERF_INDEX(serf);
			}
		}
	}

	/* Trace along the path to the flag at the other end. */
	int paths = 0;
	while (1) {
		path_len += 1;
		pos = MAP_MOVE(pos, dir);
		paths = MAP_PATHS(pos);
		paths &= ~BIT(DIR_REVERSE(dir));

		if (MAP_HAS_FLAG(pos)) break;

		/* Find out which direction the path follows. */
		for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
			if (BIT_TEST(paths, d)) {
				dir = d;
				break;
			}
		}

		/* Check if there is a transporter waiting here. */
		if (MAP_IDLE_SERF(pos)) {
			int index = wake_transporter_on_path(pos);
			if (index >= 0) data->serfs[serf_count++] = index;
		}

		/* Check if there is a serf occupying this space. */
		if (MAP_SERF_INDEX(pos) != 0) {
			serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));
			if (serf->state == SERF_STATE_TRANSPORTING &&
			    serf->s.walking.wait_counter != -1) {
				serf->s.walking.wait_counter = 0;
				data->serfs[serf_count++] = SERF_INDEX(serf);
			}
		}
	}

	/* Handle last position. */
	if (MAP_SERF_INDEX(pos) != 0) {
		serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));
		if ((serf->state == SERF_STATE_TRANSPORTING &&
		     serf->s.walking.wait_counter != -1) ||
		    serf->state == SERF_STATE_DELIVERING) {
			dir_t d = serf->s.walking.dir;
			if (d < 0) d += 6;

			if (d == DIR_REVERSE(dir)) {
				serf->s.walking.wait_counter = 0;
				data->serfs[serf_count++] = SERF_INDEX(serf);
			}
		}
	}

	/* Fill the rest of the struct. */
	data->path_len = path_len;
	data->serf_count = serf_count;
	data->flag_index = MAP_OBJ_INDEX(pos);
	data->flag_dir = DIR_REVERSE(dir);
}

static void
restore_path_serf_info(flag_t *flag, dir_t dir, serf_path_info_t *data)
{
	const int max_path_serfs[] = { 1, 2, 3, 4, 6, 8, 11, 15 };

	flag_t *other_flag = game_get_flag(data->flag_index);
	dir_t other_dir = data->flag_dir;

	flag->path_con |= BIT(dir);
	flag->endpoint &= ~BIT(dir);

	if (BIT_TEST(other_flag->endpoint, other_dir)) {
		flag->endpoint |= BIT(dir);
	}

	other_flag->transporter &= ~BIT(other_dir);
	flag->transporter &= ~BIT(dir);

	int len = get_road_length_value(data->path_len);

	flag->length[dir] = len;
	other_flag->length[other_dir] = (0x80 & other_flag->length[other_dir]) | len;

	if (BIT_TEST(other_flag->length[other_dir], 7)) {
		flag->length[dir] |= BIT(7);
	}

	flag->other_end_dir[dir] = (flag->other_end_dir[dir] & 0xc7) | (other_dir << 3);
	other_flag->other_end_dir[other_dir] = (other_flag->other_end_dir[other_dir] & 0xc7) | (dir << 3);

	flag->other_endpoint.f[dir] = other_flag;
	other_flag->other_endpoint.f[other_dir] = flag;

	int max_serfs = max_path_serfs[(len >> 4) & 7];
	if (BIT_TEST(flag->length[dir], 7)) max_serfs -= 1;

	if (data->serf_count > max_serfs) {
		for (int i = 0; i < data->serf_count - max_serfs; i++) {
			serf_t *serf = game_get_serf(data->serfs[i]);
			if (serf->state != SERF_STATE_WAKE_ON_PATH) {
				serf->s.walking.wait_counter = -1;
				if (serf->s.walking.res != 0) {
					resource_type_t res = serf->s.walking.res-1;
					serf->s.walking.res = 0;

					/* Remove gold from total count. */
					if (res == RESOURCE_GOLDBAR ||
					    res == RESOURCE_GOLDORE) {
						globals.map_gold_deposit -= 1;
					}

					flag_cancel_transported_stock(game_get_flag(serf->s.walking.dest), res+1);
				}
			} else {
				serf_log_state_change(serf, SERF_STATE_WAKE_AT_FLAG);
				serf->state = SERF_STATE_WAKE_AT_FLAG;
			}
		}
	}

	if (min(data->serf_count, max_serfs) > 0) {
		/* There is still transporters on the paths. */
		flag->transporter |= BIT(dir);
		other_flag->transporter |= BIT(other_dir);

		flag->length[dir] |= min(data->serf_count, max_serfs);
		other_flag->length[other_dir] |= min(data->serf_count, max_serfs);
	}
}

/* Build flag on existing path. Path must be split in two segments. */
static void
build_flag_split_path(map_pos_t pos)
{
	/* Find directions of path segments to be split. */
	dir_t path_1_dir = -1;
	for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
		if (BIT_TEST(MAP_PATHS(pos), d)) {
			path_1_dir = d;
			break;
		}
	}

	dir_t path_2_dir = -1;
	for (dir_t d = path_1_dir+1; d <= DIR_UP; d++) {
		if (BIT_TEST(MAP_PATHS(pos), d)) {
			path_2_dir = d;
			break;
		}
	}

	/* If last segment direction is UP LEFT it could
	   be to a building and the real path is at UP. */
	if (path_2_dir == DIR_UP_LEFT &&
	    BIT_TEST(MAP_PATHS(pos), DIR_UP)) {
		path_2_dir = DIR_UP;
	}

	serf_path_info_t path_1_data;
	serf_path_info_t path_2_data;

	fill_path_serf_info(pos, path_1_dir, &path_1_data);
	fill_path_serf_info(pos, path_2_dir, &path_2_data);

	flag_t *flag_2 = game_get_flag(path_2_data.flag_index);
	dir_t dir_2 = path_2_data.flag_dir;

	int select = -1;
	if (BIT_TEST(flag_2->length[dir_2], 7)) {
		for (int i = 1; i < globals.max_ever_serf_index; i++) {
			if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
				serf_t *serf = game_get_serf(i);

				/* TODO ??? */
				if (serf->state == SERF_STATE_WALKING) {
					if (serf->s.walking.dest == path_1_data.flag_index &&
					    serf->s.walking.res == path_1_data.flag_dir) {
						select = 0;
						break;
					} else if (serf->s.walking.dest == path_2_data.flag_index &&
						   serf->s.walking.res == path_2_data.flag_dir) {
						select = 1;
						break;
					}
				} else if (serf->state == SERF_STATE_READY_TO_LEAVE_INVENTORY) {
					if (serf->s.ready_to_leave_inventory.dest == path_1_data.flag_index &&
					    serf->s.ready_to_leave_inventory.mode == path_1_data.flag_dir) {
						select = 0;
						break;
					} else if (serf->s.ready_to_leave_inventory.dest == path_2_data.flag_index &&
						   serf->s.ready_to_leave_inventory.mode == path_2_data.flag_dir) {
						select = 1;
						break;
					}
				} else if ((serf->state == SERF_STATE_READY_TO_LEAVE ||
					    serf->state == SERF_STATE_LEAVING_BUILDING) &&
					   serf->s.leaving_building.next_state == SERF_STATE_WALKING) {
					if (serf->s.leaving_building.dest == path_1_data.flag_index &&
					    serf->s.leaving_building.field_B == path_1_data.flag_dir) {
						select = 0;
						break;
					} else if (serf->s.leaving_building.dest == path_2_data.flag_index &&
						   serf->s.leaving_building.field_B == path_2_data.flag_dir) {
						select = 1;
						break;
					}
				} else {
					NOT_REACHED();
				}
			}
		}

		serf_path_info_t *path_data = &path_1_data;
		if (select == 0) path_data = &path_2_data;

		flag_t *selected_flag = game_get_flag(path_data->flag_index);
		selected_flag->length[path_data->flag_dir] &= ~BIT(7);
	}

	flag_t *flag = game_get_flag(MAP_OBJ_INDEX(pos));

	restore_path_serf_info(flag, path_1_dir, &path_1_data);
	restore_path_serf_info(flag, path_2_dir, &path_2_data);
}

/* Build new flag. */
static void
build_flag(player_t *player)
{
	player->flags &= ~BIT(7);

	determine_map_cursor_type(player);

	if (player->sett->panel_btn_type < PANEL_BTN_BUILD_FLAG ||
	    (player->sett->map_cursor_type != 7 &&
	     player->sett->map_cursor_type != 6 &&
	     player->sett->map_cursor_type != 4)) {
		update_panel_btns_and_map_cursor(player);
		return;
	}

	player->click |= BIT(2);

	flag_t *flag;
	int flg_index;
	int r = game_alloc_flag(&flag, &flg_index);
	if (r < 0) return;

	flag->path_con = player->sett->player_num << 6;

	map_pos_t map_cursor_pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
	map_1_t *map = globals.map_mem2_ptr;
	map_2_t *map_data = MAP_2_DATA(map);

	flag->pos = map_cursor_pos;
	map_set_object(map_cursor_pos, MAP_OBJ_FLAG);
	map[map_cursor_pos].flags |= BIT(7);
	/* move_map_resources(..); */
	map_data[map_cursor_pos].u.index = flg_index;

	if (player->sett->map_cursor_type == 4) { /* built on existing road */
		build_flag_split_path(map_cursor_pos);
	}
}

/* Build a new building. The type is stored in globals.building_type. */
static void
build_building(player_t *player, map_obj_t obj_type)
{
	const int construction_cost[] = {
		0, 0, 2, 0, 2, 0, 3, 0, 2, 0,
		4, 1, 5, 0, 5, 0, 5, 0,
		2, 0, 4, 3, 1, 1, 4, 1, 2, 1, 4, 1, 3, 1, 2, 1,
		3, 2, 3, 2, 3, 3, 2, 1, 2, 3, 5, 5, 4, 1
	};

	building_type_t bld_type = globals.building_type;

	sfx_play_clip(SFX_ACCEPTED);
	player->click |= BIT(2);

	if (bld_type == BUILDING_STOCK) {
		/* TODO Check that more stocks are allowed to be built */
	}

	building_t *bld;
	int bld_index;
	int r = game_alloc_building(&bld, &bld_index);
	if (r < 0) return;

	flag_t *flag = NULL;
	int flg_index = 0;
	if (player->sett->map_cursor_type != 5) {
		r = game_alloc_flag(&flag, &flg_index);
		if (r < 0) {
			game_free_building(bld_index);
			return;
		}
	}

	if (/*!BIT_TEST(player->sett->field_163, 0)*/0) {
		switch (bld_type) {
		case BUILDING_LUMBERJACK:
			if (player->sett->lumberjack_index == 0) {
				player->sett->lumberjack_index = bld_index;
			}
			break;
		case BUILDING_SAWMILL:
			if (player->sett->sawmill_index == 0) {
				player->sett->sawmill_index = bld_index;
			}
			break;
		case BUILDING_STONECUTTER:
			if (player->sett->stonecutter_index == 0) {
				player->sett->stonecutter_index = bld_index;
			}
			break;
		default:
			break;
		}
	}

	/* request_redraw_if_pos_visible(player->sett->map_cursor_col, player->sett->map_cursor_row); */

	map_1_t *map = globals.map_mem2_ptr;
	map_2_t *map_data = MAP_2_DATA(map);

	map_pos_t pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
	bld->u.s.level = player->sett->building_height_after_level;
	bld->pos = pos;
	player->sett->incomplete_building_count[bld_type] += 1;
	bld->bld = BIT(7) | (bld_type << 2) | player->sett->player_num; /* bit 7: Unfinished building */
	bld->progress = 0;
	if (obj_type == 2) bld->progress = 1;

	if (player->sett->map_cursor_type != 5) {
		flag->path_con = player->sett->player_num << 6;
	} else {
		flg_index = MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(pos));
		flag = game_get_flag(flg_index);
	}

	flag->pos = MAP_MOVE_DOWN_RIGHT(pos);

	bld->flg_index = flg_index;
	flag->other_endpoint.b[DIR_UP_LEFT] = bld;
	flag->endpoint |= BIT(6);
	flag->bld_flags = BIT(1);
	flag->stock1_prio = 0;
	flag->bld2_flags = BIT(4);
	flag->stock2_prio = 0;
	bld->u.s.planks_needed = construction_cost[2*bld_type];
	bld->u.s.stone_needed = construction_cost[2*bld_type+1];

	/* move_map_resources(pos, map_data); */
	/* TODO Resources should be moved, just set them to zero for now */
	map_data[pos].u.s.resource = 0;
	map_data[pos].u.s.field_1 = 0;

	map_data[pos].u.index = bld_index;
	map_set_object(pos, obj_type);
	map[pos].flags |= BIT(1) | BIT(6);

	if (player->sett->map_cursor_type != 5) {
		map_set_object(MAP_MOVE_DOWN_RIGHT(pos), MAP_OBJ_FLAG);
		map[MAP_MOVE_DOWN_RIGHT(pos)].flags |= BIT(4) | BIT(7);
		/* move_map_resources(MAP_MOVE_DOWN_RIGHT(pos), map_data); */
		map_data[MAP_MOVE_DOWN_RIGHT(pos)].u.index = flg_index;
	}

	if (player->sett->map_cursor_type == 6) {
		build_flag_split_path(MAP_MOVE_DOWN_RIGHT(pos));
	}

	/* Move cursor to flag. */
	player->sett->map_cursor_col = (player->sett->map_cursor_col + 1) & globals.map_col_mask;
	player->sett->map_cursor_row = (player->sett->map_cursor_row + 1) & globals.map_row_mask;
}

/* Build a mine. */
static void
build_mine_building(player_t *player)
{
	determine_map_cursor_type(player);

	if (player->sett->map_cursor_type == 3) {
		if (player->sett->panel_btn_type == PANEL_BTN_BUILD_MINE &&
		    BIT_TEST(player->click, 3)) { /* Special click */
			close_box(player);
			sfx_play_clip(SFX_ACCEPTED);
			/* sub_58DA8(player->sett->map_cursor_col, player->sett->map_cursor_row); */
			/* sub_41D58(); */
		}
	} else {
		close_box(player);
		if (player->sett->panel_btn_type != PANEL_BTN_BUILD_MINE ||
		    (player->sett->map_cursor_type != 7 &&
		     player->sett->map_cursor_type != 6 &&
		     player->sett->map_cursor_type != 5)) {
			sfx_play_clip(SFX_NOT_ACCEPTED);
			update_panel_btns_and_map_cursor(player);
		} else {
			build_building(player, MAP_OBJ_SMALL_BUILDING);
		}
	}
}

/* Build a basic building. */
static void
build_basic_building(player_t *player)
{
	determine_map_cursor_type(player);

	if (player->sett->map_cursor_type == 3) {
		if (player->sett->panel_btn_type >= PANEL_BTN_BUILD_SMALL &&
		    BIT_TEST(player->click, 3)) { /* Special click */
			close_box(player);
			sfx_play_clip(SFX_ACCEPTED);
			/* sub_58DA8(player->sett->map_cursor_col, player->sett->map_cursor_row); */
			/* sub_41D58(); */
		}
	} else {
		close_box(player);
		if (player->sett->panel_btn_type < PANEL_BTN_BUILD_SMALL ||
		    (player->sett->map_cursor_type != 7 &&
		     player->sett->map_cursor_type != 6 &&
		     player->sett->map_cursor_type != 5)) {
			sfx_play_clip(SFX_NOT_ACCEPTED);
			update_panel_btns_and_map_cursor(player);
		} else {
			build_building(player, MAP_OBJ_SMALL_BUILDING);
		}
	}
}

/* Build advanced building. */
static void
build_advanced_building(player_t *player)
{
	determine_map_cursor_type(player);

	if (player->sett->map_cursor_type == 3) {
		if (player->sett->panel_btn_type >= PANEL_BTN_BUILD_LARGE &&
		    BIT_TEST(player->click, 3)) { /* Special click */
			close_box(player);
			sfx_play_clip(SFX_ACCEPTED);
			/* sub_58DA8(player->sett->map_cursor_col, player->sett->map_cursor_row); */
			/* sub_41D58(); */
		}
	} else {
		close_box(player);
		if (player->sett->panel_btn_type != PANEL_BTN_BUILD_LARGE ||
		    (player->sett->map_cursor_type != 7 &&
		     player->sett->map_cursor_type != 6 &&
		     player->sett->map_cursor_type != 5)) {
			sfx_play_clip(SFX_NOT_ACCEPTED);
			update_panel_btns_and_map_cursor(player);
		} else {
			build_building(player, MAP_OBJ_LARGE_BUILDING);
		}
	}
}

/* Create the initial serfs that occupies the castle. */
static void
create_initial_castle_serfs(player_sett_t *sett)
{
	sett->build |= BIT(2);

	/* Spawn serf 4 */
	serf_t *serf;
	inventory_t *inventory;
	int r = game_spawn_serf(sett, &serf, &inventory, 0);
	if (r < 0) return;

	inventory->spawn_priority -= 1;
	serf->type = (serf->type & 0x83) | (SERF_4 << 2);

	serf_log_state_change(serf, SERF_STATE_BUILDING_CASTLE);
	serf->state = SERF_STATE_BUILDING_CASTLE;
	serf->s.building_castle.inv_index = sett->castle_inventory;
	map_set_serf_index(serf->pos, SERF_INDEX(serf));

	building_t *building = game_get_building(sett->building);
	building->serf_index = SERF_INDEX(serf);

	sett->serf_count[SERF_GENERIC] -= 1;
	sett->serf_count[SERF_TRANSPORTER] += 1;

	/* Spawn generic serfs */
	for (int i = 0; i < 5; i++) {
		game_spawn_serf(sett, NULL, NULL, 0);
	}

	/* Spawn three knights */
	for (int i = 0; i < 3; i++) {
		r = game_spawn_serf(sett, &serf, &inventory, 0);
		if (r < 0) return;

		serf->type = (serf->type & 0x83) | (SERF_KNIGHT_0 << 2);

		sett->serf_count[SERF_GENERIC] -= 1;
		sett->serf_count[SERF_KNIGHT_0] += 1;
		sett->total_military_score += 1;

		inventory->resources[RESOURCE_SWORD] -= 1;
		inventory->resources[RESOURCE_SHIELD] -= 1;
		inventory->spawn_priority -= 1;
	}

	/* Spawn toolmaker */
	r = game_spawn_serf(sett, &serf, &inventory, 0);
	if (r < 0) return;

	serf->type = (serf->type & 0x83) | (SERF_TOOLMAKER << 2);

	sett->serf_count[SERF_GENERIC] -= 1;
	sett->serf_count[SERF_TOOLMAKER] += 1;

	inventory->resources[RESOURCE_HAMMER] -= 1;
	inventory->resources[RESOURCE_SAW] -= 1;
	inventory->spawn_priority -= 1;

	/* Spawn timberman */
	r = game_spawn_serf(sett, &serf, &inventory, 0);
	if (r < 0) return;

	serf->type = (serf->type & 0x83) | (SERF_LUMBERJACK << 2);

	sett->serf_count[SERF_GENERIC] -= 1;
	sett->serf_count[SERF_LUMBERJACK] += 1;

	inventory->resources[RESOURCE_AXE] -= 1;
	inventory->spawn_priority -= 1;

	/* Spawn sawmiller */
	r = game_spawn_serf(sett, &serf, &inventory, 0);
	if (r < 0) return;

	serf->type = (serf->type & 0x83) | (SERF_SAWMILLER << 2);

	sett->serf_count[SERF_GENERIC] -= 1;
	sett->serf_count[SERF_SAWMILLER] += 1;

	inventory->resources[RESOURCE_SAW] -= 1;
	inventory->spawn_priority -= 1;

	/* Spawn stonecutter */
	r = game_spawn_serf(sett, &serf, &inventory, 0);
	if (r < 0) return;

	serf->type = (serf->type & 0x83) | (SERF_STONECUTTER << 2);

	sett->serf_count[SERF_GENERIC] -= 1;
	sett->serf_count[SERF_STONECUTTER] += 1;

	inventory->resources[RESOURCE_PICK] -= 1;
	inventory->spawn_priority -= 1;

	/* Spawn digger */
	r = game_spawn_serf(sett, &serf, &inventory, 0);
	if (r < 0) return;

	serf->type = (serf->type & 0x83) | (SERF_DIGGER << 2);

	sett->serf_count[SERF_GENERIC] -= 1;
	sett->serf_count[SERF_DIGGER] += 1;

	inventory->resources[RESOURCE_SHOVEL] -= 1;
	inventory->spawn_priority -= 1;

	/* Spawn builder */
	r = game_spawn_serf(sett, &serf, &inventory, 0);
	if (r < 0) return;

	serf->type = (serf->type & 0x83) | (SERF_BUILDER << 2);

	sett->serf_count[SERF_GENERIC] -= 1;
	sett->serf_count[SERF_BUILDER] += 1;

	inventory->resources[RESOURCE_HAMMER] -= 1;
	inventory->spawn_priority -= 1;

	/* Spawn fisherman */
	r = game_spawn_serf(sett, &serf, &inventory, 0);
	if (r < 0) return;

	serf->type = (serf->type & 0x83) | (SERF_FISHER << 2);

	sett->serf_count[SERF_GENERIC] -= 1;
	sett->serf_count[SERF_FISHER] += 1;

	inventory->resources[RESOURCE_ROD] -= 1;
	inventory->spawn_priority -= 1;

	/* Spawn two geologists */
	for (int i = 0; i < 2; i++) {
		r = game_spawn_serf(sett, &serf, &inventory, 0);
		if (r < 0) return;

		serf->type = (serf->type & 0x83) | (SERF_GEOLOGIST << 2);

		sett->serf_count[SERF_GENERIC] -= 1;
		sett->serf_count[SERF_GEOLOGIST] += 1;

		inventory->resources[RESOURCE_HAMMER] -= 1;
		inventory->spawn_priority -= 1;
	}

	/* Spawn two miners */
	for (int i = 0; i < 2; i++) {
		r = game_spawn_serf(sett, &serf, &inventory, 0);
		if (r < 0) return;

		serf->type = (serf->type & 0x83) | (SERF_MINER << 2);

		sett->serf_count[SERF_GENERIC] -= 1;
		sett->serf_count[SERF_MINER] += 1;

		inventory->resources[RESOURCE_PICK] -= 1;
		inventory->spawn_priority -= 1;
	}
}

/* Build castle. */
static void
build_castle(player_t *player)
{
	determine_map_cursor_type(player);

	if (player->sett->panel_btn_type != PANEL_BTN_BUILD_CASTLE ||
	    player->sett->map_cursor_type != 7) {
		sfx_play_clip(SFX_NOT_ACCEPTED);
		update_panel_btns_and_map_cursor(player);
		return;
	}

	player->flags &= ~BIT(6);
	sfx_play_clip(SFX_ACCEPTED);
	if (BIT_TEST(globals.split, 6)) {
		/* Coop mode */
	} else {
		player->click |= BIT(2);
	}

	inventory_t *inventory;
	int inv_index;
	int r = game_alloc_inventory(&inventory, &inv_index);
	if (r < 0) return;

	building_t *castle;
	int bld_index;
	r = game_alloc_building(&castle, &bld_index);
	if (r < 0) {
		game_free_inventory(inv_index);
		return;
	}

	flag_t *flag;
	int flg_index;
	r = game_alloc_flag(&flag, &flg_index);
	if (r < 0) {
		game_free_building(bld_index);
		game_free_inventory(inv_index);
		return;
	}

	/* TODO set_map_redraw(); */

	player->sett->flags |= BIT(0); /* Has castle */
	player->sett->build |= BIT(3);
	player->sett->total_building_score += building_get_score_from_type(BUILDING_CASTLE);

	castle->serf |= BIT(4) | BIT(6);
	castle->u.inventory = inventory;
	player->sett->castle_inventory = inv_index;
	inventory->bld_index = bld_index;
	inventory->flg_index = flg_index;
	inventory->player_num = player->sett->player_num;

	/* Create initial resources */
	const int supplies_template_0[] = { 0, 0, 0, 0, 0, 0, 0, 7, 0, 2, 0, 0, 0, 0, 0, 1, 6, 1, 0, 0, 1, 2, 3, 0, 10, 10 };
	const int supplies_template_1[] = { 2, 1, 1, 3, 2, 1, 0, 25, 1, 8, 4, 3, 8, 2, 1, 3, 12, 2, 1, 1, 2, 3, 4, 1, 30, 30 };
	const int supplies_template_2[] = { 3, 2, 2, 10, 3, 1, 0, 40, 2, 20, 12, 8, 20, 4, 2, 5, 20, 3, 1, 2, 3, 4, 6, 2, 60, 60 };
	const int supplies_template_3[] = { 8, 4, 6, 20, 7, 5, 3, 80, 5, 40, 20, 40, 50, 8, 4, 10, 30, 5, 2, 4, 6, 6, 12, 4, 100, 100 };
	const int supplies_template_4[] = { 30, 10, 30, 50, 10, 30, 10, 200, 10, 100, 30, 150, 100, 10, 5, 20, 50, 10, 5, 10, 20, 20, 50, 10, 200, 200 };

	int supplies = player->sett->initial_supplies;
	const int *template_1 = NULL;
	const int *template_2 = NULL;
	if (supplies < 10) {
		template_1 = supplies_template_0;
		template_2 = supplies_template_1;
	} else if (supplies < 20) {
		template_1 = supplies_template_1;
		template_2 = supplies_template_2;
		supplies -= 10;
	} else if (supplies < 30) {
		template_1 = supplies_template_2;
		template_2 = supplies_template_3;
		supplies -= 20;
	} else if (supplies < 40) {
		template_1 = supplies_template_3;
		template_2 = supplies_template_4;
		supplies -= 30;
	} else {
		template_1 = supplies_template_4;
		template_2 = supplies_template_4;
		supplies -= 40;
	}

	for (int i = 0; i < 26; i++) {
		int t1 = template_1[i];
		int n = (template_2[i] - template_1[i]) * (supplies * 6554);
		if (n >= 0x8000) t1 += 1;
		inventory->resources[i] = t1 + (n >> 16);
	}

	if (0/*globals.game_type == GAME_TYPE_TUTORIAL*/) {
		/* TODO ... */
	}

	inventory->resources[RESOURCE_PLANK] -= 7;
	inventory->resources[RESOURCE_STONE] -= 2;
	player->sett->extra_planks = 7;
	player->sett->extra_stone = 2;
	/* player->sett->field_163 &= ~(BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4) | BIT(5)); */

	/* player->sett->lumberjack_index = 0; */
	/* player->sett->sawmill_index = 0; */
	/* player->sett->stonecutter_index = 0; */

	/* TODO ... */

	int map_cursor_pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
	castle->pos = map_cursor_pos;
	flag->pos = MAP_MOVE_DOWN_RIGHT(map_cursor_pos);
	castle->bld = BIT(7) | (BUILDING_CASTLE << 2) | player->sett->player_num;
	castle->progress = 0;
	castle->stock1 = 0xff;
	castle->stock2 = 0xff;
	player->sett->building = bld_index;
	flag->path_con = player->sett->player_num << 6;
	flag->bld_flags = BIT(7) | BIT(6);
	flag->bld2_flags = BIT(7);
	castle->flg_index = flg_index;
	flag->other_endpoint.b[DIR_UP_LEFT] = castle;
	flag->endpoint |= BIT(6);

	map_1_t *map = globals.map_mem2_ptr;
	map_2_t *map_data = MAP_2_DATA(map);
	map_data[map_cursor_pos].u.index = bld_index;
	map_set_object(map_cursor_pos, MAP_OBJ_CASTLE);
	map[map_cursor_pos].flags |= BIT(1) | BIT(6);

	map_data[MAP_MOVE_DOWN_RIGHT(map_cursor_pos)].u.index = flg_index;
	map_set_object(MAP_MOVE_DOWN_RIGHT(map_cursor_pos), MAP_OBJ_FLAG);
	map[MAP_MOVE_DOWN_RIGHT(map_cursor_pos)].flags |= BIT(7) | BIT(4);

	/* Level land in hexagon below castle */
	int h = player->sett->building_height_after_level;
	map_set_height(map_cursor_pos, h);
	for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
		map_set_height(MAP_MOVE(map_cursor_pos, d), h);
	}

	update_land_ownership(player->sett->map_cursor_col, player->sett->map_cursor_row);
	create_initial_castle_serfs(player->sett);

	player->sett->last_anim = globals.anim;

	calculate_military_flag_state(castle);
}

static void
building_remove_pl_sett_refs(building_t *building)
{
	for (int i = 0; i < 4; i++) {
		if (globals.player_sett[i]->index == BUILDING_INDEX(building)) {
			globals.player_sett[i]->index = 0;
		}
	}

	player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];

	if (sett->sawmill_index == BUILDING_INDEX(building)) {
		sett->sawmill_index = 0;
	}

	if (sett->stonecutter_index == BUILDING_INDEX(building)) {
		sett->stonecutter_index = 0;
	}

	if (sett->lumberjack_index == BUILDING_INDEX(building)) {
		sett->lumberjack_index = 0;
	}
}

static void
flag_reset_transport(flag_t *flag)
{
	/* Clear destination for any serf with resources for this flag. */
	for (int i = 1; i < globals.max_ever_serf_index; i++) {
		if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
			serf_t *serf = game_get_serf(i);

			if (serf->state == SERF_STATE_WALKING &&
			    serf->s.walking.dest == FLAG_INDEX(flag) &&
			    serf->s.walking.res < 0) {
				serf->s.walking.res = -2;
				serf->s.walking.dest = 0;
			} else if (serf->state == SERF_STATE_READY_TO_LEAVE_INVENTORY &&
				   serf->s.ready_to_leave_inventory.dest == FLAG_INDEX(flag) &&
				   serf->s.ready_to_leave_inventory.mode < 0) {
				serf->s.ready_to_leave_inventory.mode = -2;
				serf->s.ready_to_leave_inventory.dest = 0;
			} else if ((serf->state == SERF_STATE_LEAVING_BUILDING ||
				    serf->state == SERF_STATE_READY_TO_LEAVE) &&
				   serf->s.leaving_building.next_state == SERF_STATE_WALKING &&
				   serf->s.leaving_building.dest == FLAG_INDEX(flag) &&
				   serf->s.leaving_building.field_B < 0) {
				serf->s.leaving_building.field_B = -2;
				serf->s.leaving_building.dest = 0;
			} else if (serf->state == SERF_STATE_TRANSPORTING &&
				   serf->s.walking.dest == FLAG_INDEX(flag)) {
				serf->s.walking.dest = 0;
			} else if (serf->state == SERF_STATE_MOVE_RESOURCE_OUT &&
				   serf->s.move_resource_out.next_state == SERF_STATE_DROP_RESOURCE_OUT &&
				   serf->s.move_resource_out.res_dest == FLAG_INDEX(flag)) {
				serf->s.move_resource_out.res_dest = 0;
			} else if (serf->state == SERF_STATE_DROP_RESOURCE_OUT &&
				   serf->s.move_resource_out.res_dest == FLAG_INDEX(flag)) {
				serf->s.move_resource_out.res_dest = 0;
			} else if (serf->state == SERF_STATE_LEAVING_BUILDING &&
				   serf->s.leaving_building.next_state == SERF_STATE_DROP_RESOURCE_OUT &&
				   serf->s.leaving_building.dest == FLAG_INDEX(flag)) {
				serf->s.leaving_building.dest = 0;
			}
		}
	}

	/* Flag. */
	for (int i = 1; i < globals.max_ever_flag_index; i++) {
		if (BIT_TEST(globals.flg_bitmap[i>>3], 7-(i&7))) {
			flag_t *flag = game_get_flag(i);

			for (int i = 0; i < 8; i++) {
				if (flag->res_waiting[i] != 0 &&
				    flag->res_dest[i] == FLAG_INDEX(flag)) {
					flag->res_dest[i] = 0;
					flag->endpoint |= BIT(7);

					if (((flag->res_waiting[i] >> 5) & 3) != 0) {
						dir_t dir = ((flag->res_waiting[i] >> 5) & 3)-1;
						player_sett_t *sett = globals.player_sett[FLAG_PLAYER(flag)];
						flag_prioritize_pickup(flag, dir, sett->flag_prio);
					}
				}
			}
		}
	}

	/* Inventories. */
	for (int i = 0; i < globals.max_ever_inventory_index; i++) {
		if (BIT_TEST(globals.inventories_bitmap[i>>3], 7-(i&7))) {
			inventory_t *inventory = game_get_inventory(i);
			if (inventory->out_dest[1] == FLAG_INDEX(flag)) {
				inventory->out_queue[1] = 0;
			}
			if (inventory->out_dest[0] == FLAG_INDEX(flag)) {
				inventory->out_queue[0] = inventory->out_queue[1];
				inventory->out_dest[0] = inventory->out_dest[1];
				inventory->out_queue[1] = 0;
			}
		}
	}
}

/* Demolish building at pos. */
static void
building_demolish(map_pos_t pos)
{
	/* request redraw at pos */

	building_t *building = game_get_building(MAP_OBJ_INDEX(pos));
	building_remove_pl_sett_refs(building);

	player_sett_t *sett = globals.player_sett[BUILDING_PLAYER(building)];
	map_1_t *map = globals.map_mem2_ptr;

	if (BIT_TEST(building->serf, 5)) return; /* Already burning */

	building->serf |= BIT(5);

	/* Remove path to building. */
	map[pos].flags &= ~BIT(1);
	map[MAP_MOVE_DOWN_RIGHT(pos)].flags &= ~BIT(4);

	/* Remove lost gold stock from total count. */
	if (BUILDING_IS_DONE(building) &&
	    (BUILDING_TYPE(building) == BUILDING_HUT ||
	     BUILDING_TYPE(building) == BUILDING_TOWER ||
	     BUILDING_TYPE(building) == BUILDING_FORTRESS ||
	     BUILDING_TYPE(building) == BUILDING_GOLDSMELTER)) {
		int gold_stock = (building->stock2 >> 4) & 0xf;
		globals.map_gold_deposit -= gold_stock;
	}

	/* Update land owner ship if the building is military. */
	if (BUILDING_IS_DONE(building) &&
	    (BUILDING_TYPE(building) == BUILDING_HUT ||
	     BUILDING_TYPE(building) == BUILDING_TOWER ||
	     BUILDING_TYPE(building) == BUILDING_FORTRESS ||
	     BUILDING_TYPE(building) == BUILDING_CASTLE)) {
		update_land_ownership(MAP_COORD_ARGS(building->pos));
	}

	if (BUILDING_IS_DONE(building) &&
	    (BUILDING_TYPE(building) == BUILDING_CASTLE ||
	     BUILDING_TYPE(building) == BUILDING_STOCK)) {
		/* Cancel resources in the out queue and remove gold
		   from map total. */
		if (BIT_TEST(building->serf, 4)) {
			inventory_t *inventory = building->u.inventory;

			for (int i = 0; i < 2 && inventory->out_queue[i] != 0; i++) {
				int res = inventory->out_queue[i] - 1;
				int dest = inventory->out_dest[i];

				/* Remove gold from total count. */
				if (res == RESOURCE_GOLDBAR ||
				    res == RESOURCE_GOLDORE) {
					globals.map_gold_deposit -= 1;
				}

				flag_cancel_transported_stock(game_get_flag(dest), res+1);
			}

			globals.map_gold_deposit -= inventory->resources[RESOURCE_GOLDBAR];
			globals.map_gold_deposit -= inventory->resources[RESOURCE_GOLDORE];
		}

		/* Let some serfs escape while the building is burning. */
		int escaping_serfs = 0;
		for (int i = 1; i < globals.max_ever_serf_index; i++) {
			if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
				serf_t *serf = game_get_serf(i);

				if (serf->pos == building->pos &&
				    (serf->state == SERF_STATE_IDLE_IN_STOCK ||
				     serf->state == SERF_STATE_READY_TO_LEAVE_INVENTORY)) {
					if (escaping_serfs < 12) {
						/* Serf is escaping. */
						escaping_serfs += 1;
						serf->state = SERF_STATE_ESCAPE_BUILDING;
					} else {
						/* Kill this serf. */
						if (SERF_TYPE(serf) >= SERF_KNIGHT_0 &&
						    SERF_TYPE(serf) <= SERF_KNIGHT_4) {
							int score = 1 << (SERF_TYPE(serf)-SERF_KNIGHT_0);
							sett->total_military_score -= score;
						}
						sett->serf_count[SERF_TYPE(serf)] -= 1;
						game_free_serf(SERF_INDEX(serf));
					}
				}
			}
		}
	} else {
		building->serf &= ~BIT(4);
	}

	/* Remove stock from building. */
	building->stock1 = 0;
	building->stock2 = 0;

	building->serf &= ~BIT(3);

	int serf_index = building->serf_index;
	building->serf_index = 2047;
	building->u.anim = globals.anim;

	/* Update player sett fields. */
	if (BUILDING_IS_DONE(building)) {
		sett->total_building_score -= building_get_score_from_type(BUILDING_TYPE(building));

		if (BUILDING_TYPE(building) != BUILDING_CASTLE) {
			sett->completed_building_count[BUILDING_TYPE(building)] -= 1;
		}
	} else {
		sett->incomplete_building_count[BUILDING_TYPE(building)] -= 1;
	}

	if (BIT_TEST(building->serf, 6)) {
		building->serf &= ~BIT(6);

		if (BUILDING_IS_DONE(building) &&
		    BUILDING_TYPE(building) == BUILDING_CASTLE) {
			sett->build &= ~BIT(3);
			/* sett->field_15E -= 1; */

			building->serf_index = 8191;

			if (sett->serf_index != 0) {
				serf_t *serf = game_get_serf(sett->serf_index);
				serf->type = (0x83 & serf->type) | SERF_TRANSPORTER;
				serf->counter = 0;

				if (MAP_SERF_INDEX(serf->pos) == SERF_INDEX(serf)) {
					serf_log_state_change(serf, SERF_STATE_LOST);
					serf->state = SERF_STATE_LOST;
					serf->s.lost.field_B = 0;
				} else {
					serf_log_state_change(serf, SERF_STATE_ESCAPE_BUILDING);
					serf->state = SERF_STATE_ESCAPE_BUILDING;
				}
			}
		}

		if (BUILDING_IS_DONE(building) &&
		    (BUILDING_TYPE(building) == BUILDING_HUT ||
		     BUILDING_TYPE(building) == BUILDING_TOWER ||
		     BUILDING_TYPE(building) == BUILDING_FORTRESS ||
		     BUILDING_TYPE(building) == BUILDING_CASTLE)) {
			while (serf_index != 0) {
				serf_t *serf = game_get_serf(serf_index);
				serf_index = serf->s.defending.next_knight;

				if (MAP_SERF_INDEX(serf->pos) == SERF_INDEX(serf)) {
					serf_log_state_change(serf, SERF_STATE_LOST);
					serf->state = SERF_STATE_LOST;
					serf->s.lost.field_B = 0;
				} else {
					serf_log_state_change(serf, SERF_STATE_ESCAPE_BUILDING);
					serf->state = SERF_STATE_ESCAPE_BUILDING;
				}
			}
		} else {
			serf_t *serf = game_get_serf(serf_index);
			if (SERF_TYPE(serf) == SERF_4) {
				serf->type = (0x83 & serf->type) | SERF_TRANSPORTER;
			}

			serf->counter = 0;

			if (MAP_SERF_INDEX(serf->pos) == SERF_INDEX(serf)) {
				serf_log_state_change(serf, SERF_STATE_LOST);
				serf->state = SERF_STATE_LOST;
				serf->s.lost.field_B = 0;
			} else {
				serf_log_state_change(serf, SERF_STATE_ESCAPE_BUILDING);
				serf->state = SERF_STATE_ESCAPE_BUILDING;
			}
		}
	}

	/* Flag. */
	flag_t *flag = game_get_flag(building->flg_index);
	flag->other_endpoint.b[DIR_UP_LEFT] = NULL;
	flag->endpoint &= ~BIT(6);

	flag->bld_flags = 0;
	flag->bld2_flags = 0;

	flag_reset_transport(flag);

	if (MAP_PATHS(MAP_MOVE_DOWN_RIGHT(pos)) == 0 &&
	    MAP_OBJ(MAP_MOVE_DOWN_RIGHT(pos)) == MAP_OBJ_FLAG) {
		/* TODO */
	}
}

static int
remove_road_backref_until_flag(map_pos_t pos, dir_t dir)
{
	map_1_t *map = globals.map_mem2_ptr;

	while (1) {
		pos = MAP_MOVE(pos, dir);

		/* Clear backreference */
		map[pos].flags &= ~BIT(DIR_REVERSE(dir));

		if (MAP_OBJ(pos) == MAP_OBJ_FLAG) break;

		/* Find next direction of path. */
		dir = -1;
		for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
			if (BIT_TEST(MAP_PATHS(pos), d)) {
				dir = d;
				break;
			}
		}

		if (dir == -1) return -1;
	}

	return 0;
}

static int
remove_road_backrefs(map_pos_t pos)
{
	if (MAP_PATHS(pos) == 0) return -1;

	/* Find directions of path segments to be split. */
	dir_t path_1_dir = -1;
	for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
		if (BIT_TEST(MAP_PATHS(pos), d)) {
			path_1_dir = d;
			break;
		}
	}

	dir_t path_2_dir = -1;
	for (dir_t d = path_1_dir+1; d <= DIR_UP; d++) {
		if (BIT_TEST(MAP_PATHS(pos), d)) {
			path_2_dir = d;
			break;
		}
	}

	if (path_1_dir == -1 || path_2_dir == -1) return -1;

	int r = remove_road_backref_until_flag(pos, path_1_dir);
	if (r < 0) return -1;

	r = remove_road_backref_until_flag(pos, path_2_dir);
	if (r < 0) return -1;

	return 0;
}

static int
path_serf_idle_to_wait_state(map_pos_t pos)
{
	/* Look through serf array for the corresponding serf. */
	for (int i = 1; i < globals.max_ever_serf_index; i++) {
		if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
			serf_t *serf = game_get_serf(i);
			if (serf->pos == pos &&
			    (serf->state == SERF_STATE_IDLE_ON_PATH ||
			     serf->state == SERF_STATE_WAIT_IDLE_ON_PATH ||
			     serf->state == SERF_STATE_WAKE_AT_FLAG ||
			     serf->state == SERF_STATE_WAKE_ON_PATH)) {
				serf_log_state_change(serf, SERF_STATE_WAKE_AT_FLAG);
				serf->state = SERF_STATE_WAKE_AT_FLAG;
				return 0;
			}
		}
	}

	return -1;
}

static void
lose_transported_resource(resource_type_t res, uint dest)
{
	static const int stock_type[] = {
		0, 0, 0, 0, 0, 0,
		1, 0, -1, 1, 1, 1,
		0, 1, 1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1,
		-1, -1, -1
	};

	if (res == RESOURCE_GOLDORE ||
	    res == RESOURCE_GOLDBAR) {
		globals.map_gold_deposit -= 1;
	}

	if (stock_type[res] >= 0 && dest != 0) {
		flag_t *flag = game_get_flag(dest);
		building_t *building = flag->other_endpoint.b[DIR_UP_LEFT];
		if (!(BUILDING_IS_DONE(building) &&
		      (BUILDING_TYPE(building) == BUILDING_CASTLE ||
		       BUILDING_TYPE(building) == BUILDING_STOCK))) {
			if (stock_type[res] == 0) building->stock1 -= 1;
			else building->stock2 -= 1;
		}
	}
}


/* ADDITION: Removed precondition that serf is in state walking or transporting. */
static void
mark_serf_as_lost(serf_t *serf)
{
	if (serf->state == SERF_STATE_WALKING) {
		if (serf->s.walking.res >= 0) {
			if (serf->s.walking.res != 6) {
				dir_t dir = serf->s.walking.res;
				flag_t *flag = game_get_flag(serf->s.walking.dest);
				flag->length[dir] &= ~BIT(7);

				dir_t other_dir = (flag->other_end_dir[dir] >> 3) & 7;
				flag->other_endpoint.f[dir]->length[other_dir] &= ~BIT(7);
			}
		} else if (serf->s.walking.res == -1) {
			flag_t *flag = game_get_flag(serf->s.walking.dest);
			building_t *building = flag->other_endpoint.b[DIR_UP_LEFT];

			if (BIT_TEST(building->serf, 7)) {
				building->serf &= ~BIT(7);
			} else if (building->stock1 != 0xff) {
				building->stock1 -= 1;
				if (building->stock1 < 0) building->stock1 = 0xff; /* Should probably just be a signed int. */
			}
		}

		serf_log_state_change(serf, SERF_STATE_LOST);
		serf->state = SERF_STATE_LOST;
		serf->s.lost.field_B = 0;
	} else if (serf->state == SERF_STATE_TRANSPORTING ||
		   serf->state == SERF_STATE_DELIVERING) {
		if (serf->s.walking.res != 0) {
			int res = serf->s.walking.res-1;
			int dest = serf->s.walking.dest;

			lose_transported_resource(res, dest);
		}

		if (serf->type != SERF_SAILOR) {
			serf_log_state_change(serf, SERF_STATE_LOST);
			serf->state = SERF_STATE_LOST;
			serf->s.lost.field_B = 0;
		} else {
			serf_log_state_change(serf, SERF_STATE_26);
			serf->state = SERF_STATE_26;
		}
	}
}

static void
remove_road_forwards(map_pos_t pos, dir_t dir)
{
	map_1_t *map = globals.map_mem2_ptr;

	while (1) {
		if (MAP_IDLE_SERF(pos)) {
			path_serf_idle_to_wait_state(pos);
		}

		if (MAP_SERF_INDEX(pos) != 0) {
			serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));
			if (!MAP_HAS_FLAG(pos)) {
				mark_serf_as_lost(serf);
			} else {
				/* Handle serf close to flag, where
				   it should only be lost if walking
				   in the wrong direction. */
				int d = serf->s.walking.dir;
				if (d < 0) d += 6;
				if (d == DIR_REVERSE(dir)) {
					mark_serf_as_lost(serf);
				}
			}
		}

		if (MAP_HAS_FLAG(pos)) {
			flag_t *flag = game_get_flag(MAP_OBJ_INDEX(pos));
			dir_t rev_dir = DIR_REVERSE(dir);

			flag->path_con &= ~BIT(rev_dir);
			flag->transporter &= ~BIT(rev_dir);
			flag->endpoint &= ~BIT(rev_dir);

			if (BIT_TEST(flag->length[rev_dir], 7)) {
				flag->length[rev_dir] &= ~BIT(7);

				for (int i = 1; i < globals.max_ever_serf_index; i++) {
					if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
						int dest = MAP_OBJ_INDEX(pos);
						serf_t *serf = game_get_serf(i);

						switch (serf->state) {
						case SERF_STATE_WALKING:
							if (serf->s.walking.dest == dest &&
							    serf->s.walking.res == rev_dir) {
								serf->s.walking.res = 0xfe;
								serf->s.walking.dest = 0;
							}
							break;
						case SERF_STATE_READY_TO_LEAVE_INVENTORY:
							if (serf->s.ready_to_leave_inventory.dest == dest &&
							    serf->s.ready_to_leave_inventory.mode == rev_dir) {
								serf->s.ready_to_leave_inventory.dest = 0xfe;
								serf->s.ready_to_leave_inventory.dest = 0;
							}
							break;
						case SERF_STATE_LEAVING_BUILDING:
						case SERF_STATE_READY_TO_LEAVE:
							if (serf->s.leaving_building.dest == dest &&
							    serf->s.leaving_building.field_B == rev_dir &&
							    serf->s.leaving_building.next_state == SERF_STATE_WALKING) {
								serf->s.leaving_building.field_B = 0xfe;
								serf->s.leaving_building.dest = 0;
							}
							break;
						}
					}
				}
			}

			flag->other_end_dir[rev_dir] &= 0x78;

			/* Mark resource path for recalculation if they would
			   have followed the removed path. */
			for (int i = 0; i < 8; i++) {
				if (flag->res_waiting[i] != 0 &&
				    (flag->res_waiting[i] >> 5) == rev_dir+1) {
					flag->res_waiting[i] &= 0x1f;
					flag->endpoint |= BIT(7);
				}
			}
			break;
		}

		/* Clear forward reference. */
		map[pos].flags &= ~BIT(dir);
		pos = MAP_MOVE(pos, dir);

		/* Clear backreference. */
		map[pos].flags &= ~BIT(DIR_REVERSE(dir));

		/* Find next direction of path. */
		dir = -1;
		for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
			if (BIT_TEST(MAP_PATHS(pos), d)) {
				dir = d;
				break;
			}
		}
	}
}

/* Demolish road at position. */
static void
road_demolish(map_pos_t pos)
{
	globals.player[0]->flags |= BIT(4);
	globals.player[1]->flags |= BIT(4);

	int r = remove_road_backrefs(pos);
	if (r < 0) {
		/* TODO */
	}

	/* Find directions of path segments to be split. */
	dir_t path_1_dir = -1;
	for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
		if (BIT_TEST(MAP_PATHS(pos), d)) {
			path_1_dir = d;
			break;
		}
	}

	dir_t path_2_dir = -1;
	for (dir_t d = path_1_dir+1; d <= DIR_UP; d++) {
		if (BIT_TEST(MAP_PATHS(pos), d)) {
			path_2_dir = d;
			break;
		}
	}

	/* If last segment direction is UP LEFT it could
	   be to a building and the real path is at UP. */
	if (path_2_dir == DIR_UP_LEFT &&
	    BIT_TEST(MAP_PATHS(pos), DIR_UP)) {
		path_2_dir = DIR_UP;
	}

	remove_road_forwards(pos, path_1_dir);
	remove_road_forwards(pos, path_2_dir);
}

static void
do_demolish(player_t *player)
{
	determine_map_cursor_type(player);

	if (player->sett->map_cursor_type == 2) {
		/* TODO */
	} else if (player->sett->map_cursor_type == 3) {
		map_pos_t cursor_pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
		building_t *building = game_get_building(MAP_OBJ_INDEX(cursor_pos));

		if (BUILDING_IS_DONE(building) &&
		    (BUILDING_TYPE(building) == BUILDING_HUT ||
		     BUILDING_TYPE(building) == BUILDING_TOWER ||
		     BUILDING_TYPE(building) == BUILDING_FORTRESS)) {
			/* TODO */
		}

		sfx_play_clip(SFX_AHHH);
		player->click |= BIT(2);
		building_demolish(cursor_pos);
	} else {
		sfx_play_clip(SFX_NOT_ACCEPTED);
		update_panel_btns_and_map_cursor(player);
	}
}

/* Handle a click on the panel buttons. */
static void
handle_panel_btn_click(player_t *player, int btn)
{
	switch (player->panel_btns[btn]) {
		case PANEL_BTN_MAP:
		case PANEL_BTN_MAP_STARRED:
			sfx_play_clip(SFX_CLICK);
			/* TODO */
			break;
		case PANEL_BTN_SETT:
		case PANEL_BTN_SETT_STARRED:
			sfx_play_clip(SFX_CLICK);
			if (BIT_TEST(player->click, 6)) { /* Popup open */
				close_box(player);
			} else {
				player->flags &= ~BIT(6);
				player->click |= BIT(6);
				player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
				player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
				player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
				player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
				player->panel_btns[4] = PANEL_BTN_SETT_STARRED;
				player->click &= ~BIT(1);
				if (BIT_TEST(player->click, 0)) {
					player->box = BOX_SETT_SELECT;
				} else {
					player->box = BOX_SETT_SELECT_FILE;
				}
			}
			break;
		case PANEL_BTN_STATS:
		case PANEL_BTN_STATS_STARRED:
			sfx_play_clip(SFX_CLICK);
			if (BIT_TEST(player->click, 6)) { /* Popup open */
				close_box(player);
			} else {
				player->flags &= ~BIT(6);
				player->click |= BIT(6);
				player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
				player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
				player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
				player->panel_btns[3] = PANEL_BTN_STATS_STARRED;
				player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
				player->click &= ~BIT(1);
				player->box = BOX_STAT_SELECT;
			}
			break;
		case PANEL_BTN_BUILD_ROAD:
		case PANEL_BTN_BUILD_ROAD_STARRED:
			if (BIT_TEST(player->click, 3)) { /* Special click */
				/* TODO */
			} else {
				sfx_play_clip(SFX_CLICK);
				if (BIT_TEST(player->click, 6)) { /* Popup open (Building road) */
					build_road_end(player);
				} else {
					build_road_begin(player);
				}
			}
			break;
		case PANEL_BTN_BUILD_FLAG:
			if (BIT_TEST(player->click, 3)) { /* Special click */
				/* TODO */
			} else {
				sfx_play_clip(SFX_CLICK);
				build_flag(player);
			}
			break;
		case PANEL_BTN_BUILD_SMALL:
		case PANEL_BTN_BUILD_SMALL_STARRED:
			if (BIT_TEST(player->click, 3)) { /* Special click */
				/* TODO */
			} else {
				sfx_play_clip(SFX_CLICK);
				if (BIT_TEST(player->click, 6)) { /* Popup open */
					close_box(player);
				} else {
					player->click |= BIT(6);
					player->panel_btns[0] = PANEL_BTN_BUILD_SMALL_STARRED;
					player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
					player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
					player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
					player->click &= ~BIT(1);
					player->box = BOX_BASIC_BLD;
				}
			}
			break;
		case PANEL_BTN_BUILD_LARGE:
		case PANEL_BTN_BUILD_LARGE_STARRED:
			if (BIT_TEST(player->click, 3)) { /* Special click */
				/* TODO */
			} else {
				sfx_play_clip(SFX_CLICK);
				if (BIT_TEST(player->click, 6)) { /* Popup open */
					close_box(player);
				} else {
					player->click |= BIT(6);
					player->panel_btns[0] = PANEL_BTN_BUILD_LARGE_STARRED;
					player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
					player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
					player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
					player->click &= ~BIT(1);
					player->box = BOX_BASIC_BLD_FLIP;
				}
			}
			break;
		case PANEL_BTN_BUILD_MINE:
		case PANEL_BTN_BUILD_MINE_STARRED:
			if (BIT_TEST(player->click, 3)) { /* Special click */
				/* TODO */
			} else {
				sfx_play_clip(SFX_CLICK);
				if (BIT_TEST(player->click, 6)) { /* Popup open */
					close_box(player);
				} else {
					player->click |= BIT(6);
					player->panel_btns[0] = PANEL_BTN_BUILD_MINE_STARRED;
					player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
					player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
					player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
					player->click &= ~BIT(1);
					player->box = BOX_MINE_BUILDING;
				}
			}
			break;
		case PANEL_BTN_DESTROY:
			if (BIT_TEST(player->click, 3)) { /* Special click */
				if (player->sett->map_cursor_type == 2) {
					do_demolish(player);
				} else {
					player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
					player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
					player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
					player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
					player->click &= ~BIT(1);
					player->box = BOX_DEMOLISH;
				}
			}
			break;
		case PANEL_BTN_BUILD_CASTLE:
			if (BIT_TEST(player->click, 3)) { /* Special click */
				/* TODO */
			} else {
				build_castle(player);
			}
			break;
		case PANEL_BTN_DESTROY_ROAD:
			if (BIT_TEST(player->click, 3)) { /* Special click */
				/* TODO */
			} else {
				player->flags &= ~BIT(6);
				determine_map_cursor_type(player);
				if (player->sett->map_cursor_type == 4) {
					sfx_play_clip(SFX_ACCEPTED);
					player->click |= BIT(2);

					map_pos_t cursor_pos = MAP_POS(player->sett->map_cursor_col,
								       player->sett->map_cursor_row);
					road_demolish(cursor_pos);
				} else {
					sfx_play_clip(SFX_NOT_ACCEPTED);
					update_panel_btns_and_map_cursor(player);
				}
			}
			break;
		case PANEL_BTN_GROUND_ANALYSIS:
		case PANEL_BTN_GROUND_ANALYSIS_STARRED:
			sfx_play_clip(SFX_CLICK);
			if (BIT_TEST(player->click, 6)) { /* Popup open */
				close_box(player);
			} else {
				if (BIT_TEST(globals.split, 6)) { /* Coop mode */
					/* TODO */
				}
				player->flags &= ~BIT(6);
				player->click |= BIT(6);
				player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
				player->panel_btns[1] = PANEL_BTN_GROUND_ANALYSIS_STARRED;
				player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
				player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
				player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
				player->click &= ~BIT(1);
				player->box = BOX_GROUND_ANALYSIS;
			}
			break;
		case PANEL_BTN_BUILD_INACTIVE:
			/* TODO */
			break;
	}
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
handle_send_geologist(player_t *player)
{
	map_pos_t pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
	map_2_t *map_data = MAP_2_DATA(globals.map_mem2_ptr);
	int flag_index = map_data[pos].u.index;
	flag_t *flag = game_get_flag(flag_index);
	int r = game_send_geologist(flag, flag_index);
	if (r < 0) {
		sfx_play_clip(SFX_NOT_ACCEPTED);
	} else {
		sfx_play_clip(SFX_ACCEPTED);
		close_box(player);
	}
}

/* Get the resulting value from a click on a slider bar. */
static int
get_slider_click_value(int x)
{
	return 1310 * clamp(0, x - 7, 50);
}

static void
knight_occupation_adjust(player_t *player, int index, int adjust_max, int delta)
{
	int max = (player->sett->knight_occupation[index] >> 4) & 0xf;
	int min = player->sett->knight_occupation[index] & 0xf;

	if (adjust_max) {
		max = clamp(min, max + delta, 4);
	} else {
		min = clamp(0, min + delta, max);
	}

	player->sett->knight_occupation[index] = (max << 4) | min;
}

static void
activate_sett_5_6_item(player_t *player, int index)
{
	if (player->clkmap == BOX_SETT_5) {
		int i;
		for (i = 0; i < 26; i++) {
			if (player->sett->flag_prio[i] == index) break;
		}
		player->sett->current_sett_5_item = i+1;
	} else {
		int i;
		for (i = 0; i < 26; i++) {
			if (player->sett->inventory_prio[i] == index) break;
		}
		player->sett->current_sett_6_item = i+1;
	}
	player->box = player->clkmap;
}

static void
move_sett_5_6_item(player_t *player, int up, int to_end)
{
	int *prio = NULL;
	int cur = -1;

	if (player->clkmap == BOX_SETT_5) {
		prio = player->sett->flag_prio;
		cur = player->sett->current_sett_5_item-1;
	} else {
		prio = player->sett->inventory_prio;
		cur = player->sett->current_sett_6_item-1;
	}

	int cur_value = prio[cur];
	int next_value = -1;
	if (up) {
		if (to_end) next_value = 26;
		else next_value = cur_value + 1;
	} else {
		if (to_end) next_value = 1;
		else next_value = cur_value - 1;
	}

	if (next_value >= 1 && next_value < 27) {
		int delta = next_value > cur_value ? -1 : 1;
		int min = next_value > cur_value ? cur_value+1 : next_value;
		int max = next_value > cur_value ? next_value : cur_value-1;
		for (int i = 0; i < 26; i++) {
			if (prio[i] >= min && prio[i] <= max) prio[i] += delta;
		}
		prio[cur] = next_value;
	}

	player->box = player->clkmap;
}

static int
promote_serfs_to_knights(player_sett_t *sett, int number)
{
	int promoted = 0;

	for (int i = 1; i < globals.max_ever_serf_index && number > 0; i++) {
		if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
			serf_t *serf = game_get_serf(i);
			if (serf->state == SERF_STATE_IDLE_IN_STOCK &&
			    SERF_PLAYER(serf) == sett->player_num &&
			    SERF_TYPE(serf) == SERF_GENERIC) {
				inventory_t *inv = game_get_inventory(serf->s.idle_in_stock.inv_index);
				if (inv->resources[RESOURCE_SWORD] > 0 &&
				    inv->resources[RESOURCE_SHIELD] > 0) {
					inv->resources[RESOURCE_SWORD] -= 1;
					inv->resources[RESOURCE_SHIELD] -= 1;
					inv->spawn_priority -= 1;
					inv->serfs[SERF_GENERIC] = 0;

					serf->type = (SERF_KNIGHT_0 << 2) | (serf->type & 3);

					sett->serf_count[SERF_GENERIC] -= 1;
					sett->serf_count[SERF_KNIGHT_0] += 1;
					sett->total_military_score += 1;

					promoted += 1;
					number -= 1;
				}
			}
		}
	}

	return promoted;
}

static void
sett_8_train(player_t *player, int number)
{
	int r = promote_serfs_to_knights(player->sett, number);

	if (r == 0) sfx_play_clip(SFX_NOT_ACCEPTED);
	else sfx_play_clip(SFX_ACCEPTED);

	player->box = player->clkmap;
}

/* Generic handler for clicks in popup boxes. */
static int
handle_clickmap(player_t *player, int x, int y, const int clkmap[])
{
	while (clkmap[0] > 0) {
		if (clkmap[1] <= x && x <= clkmap[2] &&
		    clkmap[3] <= y && y <= clkmap[4]) {
			sfx_play_clip(SFX_CLICK);

			action_t action = clkmap[0];
			switch (action) {
			case ACTION_BUILD_STONEMINE:
				globals.building_type = BUILDING_STONEMINE;
				build_mine_building(player);
				break;
			case ACTION_BUILD_COALMINE:
				globals.building_type = BUILDING_COALMINE;
				build_mine_building(player);
				break;
			case ACTION_BUILD_IRONMINE:
				globals.building_type = BUILDING_IRONMINE;
				build_mine_building(player);
				break;
			case ACTION_BUILD_GOLDMINE:
				globals.building_type = BUILDING_GOLDMINE;
				build_mine_building(player);
				break;
			case ACTION_BUILD_FLAG:
				if (BIT_TEST(player->sett->build, 1)) break; /* Can not build flag */
				build_flag(player);
				close_box(player);
				break;
			case ACTION_BUILD_STONECUTTER:
				globals.building_type = BUILDING_STONECUTTER;
				build_basic_building(player);
				break;
			case ACTION_BUILD_HUT:
				if (!BIT_TEST(player->sett->build, 0)) { /* Can build military building */
					globals.building_type = BUILDING_HUT;
					build_basic_building(player);
				}
				break;
			case ACTION_BUILD_LUMBERJACK:
				globals.building_type = BUILDING_LUMBERJACK;
				build_basic_building(player);
				break;
			case ACTION_BUILD_FORESTER:
				globals.building_type = BUILDING_FORESTER;
				build_basic_building(player);
				break;
			case ACTION_BUILD_FISHER:
				globals.building_type = BUILDING_FISHER;
				build_basic_building(player);
				break;
			case ACTION_BUILD_MILL:
				globals.building_type = BUILDING_MILL;
				build_basic_building(player);
				break;
			case ACTION_BUILD_BOATBUILDER:
				globals.building_type = BUILDING_BOATBUILDER;
				build_basic_building(player);
				break;
			case ACTION_BUILD_BUTCHER:
				globals.building_type = BUILDING_BUTCHER;
				build_advanced_building(player);
				break;
			case ACTION_BUILD_WEAPONSMITH:
				globals.building_type = BUILDING_WEAPONSMITH;
				build_advanced_building(player);
				break;
			case ACTION_BUILD_STEELSMELTER:
				globals.building_type = BUILDING_STEELSMELTER;
				build_advanced_building(player);
				break;
			case ACTION_BUILD_SAWMILL:
				globals.building_type = BUILDING_SAWMILL;
				build_advanced_building(player);
				break;
			case ACTION_BUILD_BAKER:
				globals.building_type = BUILDING_BAKER;
				build_advanced_building(player);
				break;
			case ACTION_BUILD_GOLDSMELTER:
				globals.building_type = BUILDING_GOLDSMELTER;
				build_advanced_building(player);
				break;
			case ACTION_BUILD_FORTRESS:
				if (!BIT_TEST(player->sett->build, 0)) { /* Can build military building */
					globals.building_type = BUILDING_FORTRESS;
					build_advanced_building(player);
				}
				break;
			case ACTION_BUILD_TOWER:
				if (!BIT_TEST(player->sett->build, 0)) { /* Can build military building */
					globals.building_type = BUILDING_TOWER;
					build_advanced_building(player);
				}
				break;
			case ACTION_BUILD_TOOLMAKER:
				globals.building_type = BUILDING_TOOLMAKER;
				build_advanced_building(player);
				break;
			case ACTION_BUILD_FARM:
				globals.building_type = BUILDING_FARM;
				build_advanced_building(player);
				break;
			case ACTION_BUILD_PIGFARM:
				globals.building_type = BUILDING_PIGFARM;
				build_advanced_building(player);
				break;
			case ACTION_BLD_FLIP_PAGE:
				player->box = (player->clkmap + 1 <= BOX_ADV_2_BLD) ? (player->clkmap + 1) : BOX_BASIC_BLD_FLIP;
				break;
			case ACTION_SHOW_STAT_1:
				player->box = BOX_STAT_1;
				break;
			case ACTION_SHOW_STAT_2:
				player->box = BOX_STAT_2;
				break;
			case ACTION_SHOW_STAT_8:
				player->box = BOX_STAT_8;
				break;
			case ACTION_SHOW_STAT_BLD:
				player->box = BOX_STAT_BLD_1;
				break;
			case ACTION_SHOW_STAT_6:
				player->box = BOX_STAT_6;
				break;
			case ACTION_SHOW_STAT_7:
				player->box = BOX_STAT_7;
				break;
			case ACTION_SHOW_STAT_4:
				player->box = BOX_STAT_4;
				break;
			case ACTION_SHOW_STAT_3:
				player->box = BOX_STAT_3;
				break;
			case ACTION_SHOW_STAT_SELECT:
			case ACTION_SHOW_STAT_SELECT_FILE:
				player->box = BOX_STAT_SELECT;
				break;
			case ACTION_STAT_BLD_FLIP:
				player->box = (player->clkmap + 1 <= BOX_STAT_BLD_4) ? (player->clkmap + 1) : BOX_STAT_BLD_1;
				break;
			case ACTION_CLOSE_BOX:
			case ACTION_CLOSE_SETT_BOX:
			case ACTION_CLOSE_GROUND_ANALYSIS:
				close_box(player);
				break;
			case ACTION_SETT_8_SET_ASPECT_ALL:
				player->current_stat_8_mode = (0 << 2) | (player->current_stat_8_mode & 3);
				player->box = player->clkmap;
				break;
			case ACTION_SETT_8_SET_ASPECT_LAND:
				player->current_stat_8_mode = (1 << 2) | (player->current_stat_8_mode & 3);
				player->box = player->clkmap;
				break;
			case ACTION_SETT_8_SET_ASPECT_BUILDINGS:
				player->current_stat_8_mode = (2 << 2) | (player->current_stat_8_mode & 3);
				player->box = player->clkmap;
				break;
			case ACTION_SETT_8_SET_ASPECT_MILITARY:
				player->current_stat_8_mode = (3 << 2) | (player->current_stat_8_mode & 3);
				player->box = player->clkmap;
				break;
			case ACTION_SETT_8_SET_SCALE_30_MIN:
				player->current_stat_8_mode = (player->current_stat_8_mode & 0xc) | 0;
				player->box = player->clkmap;
				break;
			case ACTION_SETT_8_SET_SCALE_60_MIN:
				player->current_stat_8_mode = (player->current_stat_8_mode & 0xc) | 1;
				player->box = player->clkmap;
				break;
			case ACTION_SETT_8_SET_SCALE_600_MIN:
				player->current_stat_8_mode = (player->current_stat_8_mode & 0xc) | 2;
				player->box = player->clkmap;
				break;
			case ACTION_SETT_8_SET_SCALE_3000_MIN:
				player->current_stat_8_mode = (player->current_stat_8_mode & 0xc) | 3;
				player->box = player->clkmap;
				break;
			case ACTION_STAT_7_SELECT_FISH:
			case ACTION_STAT_7_SELECT_PIG:
			case ACTION_STAT_7_SELECT_MEAT:
			case ACTION_STAT_7_SELECT_WHEAT:
			case ACTION_STAT_7_SELECT_FLOUR:
			case ACTION_STAT_7_SELECT_BREAD:
			case ACTION_STAT_7_SELECT_LUMBER:
			case ACTION_STAT_7_SELECT_PLANK:
			case ACTION_STAT_7_SELECT_BOAT:
			case ACTION_STAT_7_SELECT_STONE:
			case ACTION_STAT_7_SELECT_IRONORE:
			case ACTION_STAT_7_SELECT_STEEL:
			case ACTION_STAT_7_SELECT_COAL:
			case ACTION_STAT_7_SELECT_GOLDORE:
			case ACTION_STAT_7_SELECT_GOLDBAR:
			case ACTION_STAT_7_SELECT_SHOVEL:
			case ACTION_STAT_7_SELECT_HAMMER:
			case ACTION_STAT_7_SELECT_ROD:
			case ACTION_STAT_7_SELECT_CLEAVER:
			case ACTION_STAT_7_SELECT_SCYTHE:
			case ACTION_STAT_7_SELECT_AXE:
			case ACTION_STAT_7_SELECT_SAW:
			case ACTION_STAT_7_SELECT_PICK:
			case ACTION_STAT_7_SELECT_PINCER:
			case ACTION_STAT_7_SELECT_SWORD:
			case ACTION_STAT_7_SELECT_SHIELD:
				player->current_stat_7_item = action - ACTION_STAT_7_SELECT_FISH + 1;
				player->box = player->clkmap;
				break;
				/* TODO */
			case ACTION_SHOW_SETT_1:
				player->box = BOX_SETT_1;
				break;
			case ACTION_SHOW_SETT_2:
				player->box = BOX_SETT_2;
				break;
			case ACTION_SHOW_SETT_3:
				player->box = BOX_SETT_3;
				break;
			case ACTION_SHOW_SETT_7:
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_SHOW_SETT_4:
				player->box = BOX_SETT_4;
				break;
			case ACTION_SHOW_SETT_5:
				player->box = BOX_SETT_5;
				break;
			case ACTION_SHOW_SETT_SELECT:
			case ACTION_SHOW_SETT_SELECT_FILE:
				player->box = BOX_SETT_SELECT;
				break;
			case ACTION_SETT_1_ADJUST_STONEMINE:
				player->box = BOX_SETT_1;
				player->sett->food_stonemine = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_1_ADJUST_COALMINE:
				player->box = BOX_SETT_1;
				player->sett->food_coalmine = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_1_ADJUST_IRONMINE:
				player->box = BOX_SETT_1;
				player->sett->food_ironmine = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_1_ADJUST_GOLDMINE:
				player->box = BOX_SETT_1;
				player->sett->food_goldmine = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_2_ADJUST_CONSTRUCTION:
				player->box = BOX_SETT_2;
				player->sett->planks_construction = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_2_ADJUST_BOATBUILDER:
				player->box = BOX_SETT_2;
				player->sett->planks_boatbuilder = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_2_ADJUST_TOOLMAKER_PLANKS:
				player->box = BOX_SETT_2;
				player->sett->planks_toolmaker = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_2_ADJUST_TOOLMAKER_STEEL:
				player->box = BOX_SETT_2;
				player->sett->steel_toolmaker = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_2_ADJUST_WEAPONSMITH:
				player->box = BOX_SETT_2;
				player->sett->steel_weaponsmith = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_3_ADJUST_STEELSMELTER:
				player->box = BOX_SETT_3;
				player->sett->coal_steelsmelter = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_3_ADJUST_GOLDSMELTER:
				player->box = BOX_SETT_3;
				player->sett->coal_goldsmelter = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_3_ADJUST_WEAPONSMITH:
				player->box = BOX_SETT_3;
				player->sett->coal_weaponsmith = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_3_ADJUST_PIGFARM:
				player->box = BOX_SETT_3;
				player->sett->wheat_pigfarm = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_3_ADJUST_MILL:
				player->box = BOX_SETT_3;
				player->sett->wheat_mill = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_KNIGHT_LEVEL_CLOSEST_MIN_DEC:
				knight_occupation_adjust(player, 3, 0, -1);
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_KNIGHT_LEVEL_CLOSEST_MIN_INC:
				knight_occupation_adjust(player, 3, 0, 1);
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_KNIGHT_LEVEL_CLOSEST_MAX_DEC:
				knight_occupation_adjust(player, 3, 1, -1);
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_KNIGHT_LEVEL_CLOSEST_MAX_INC:
				knight_occupation_adjust(player, 3, 1, 1);
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_KNIGHT_LEVEL_CLOSE_MIN_DEC:
				knight_occupation_adjust(player, 2, 0, -1);
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_KNIGHT_LEVEL_CLOSE_MIN_INC:
				knight_occupation_adjust(player, 2, 0, 1);
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_KNIGHT_LEVEL_CLOSE_MAX_DEC:
				knight_occupation_adjust(player, 2, 1, -1);
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_KNIGHT_LEVEL_CLOSE_MAX_INC:
				knight_occupation_adjust(player, 2, 1, 1);
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_KNIGHT_LEVEL_FAR_MIN_DEC:
				knight_occupation_adjust(player, 1, 0, -1);
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_KNIGHT_LEVEL_FAR_MIN_INC:
				knight_occupation_adjust(player, 1, 0, 1);
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_KNIGHT_LEVEL_FAR_MAX_DEC:
				knight_occupation_adjust(player, 1, 1, -1);
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_KNIGHT_LEVEL_FAR_MAX_INC:
				knight_occupation_adjust(player, 1, 1, 1);
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_KNIGHT_LEVEL_FARTHEST_MIN_DEC:
				knight_occupation_adjust(player, 0, 0, -1);
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_KNIGHT_LEVEL_FARTHEST_MIN_INC:
				knight_occupation_adjust(player, 0, 0, 1);
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_KNIGHT_LEVEL_FARTHEST_MAX_DEC:
				knight_occupation_adjust(player, 0, 1, -1);
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_KNIGHT_LEVEL_FARTHEST_MAX_INC:
				knight_occupation_adjust(player, 0, 1, 1);
				player->box = BOX_KNIGHT_LEVEL;
				break;
			case ACTION_SETT_4_ADJUST_SHOVEL:
				player->box = BOX_SETT_4;
				player->sett->tool_prio[0] = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_4_ADJUST_HAMMER:
				player->box = BOX_SETT_4;
				player->sett->tool_prio[1] = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_4_ADJUST_AXE:
				player->box = BOX_SETT_4;
				player->sett->tool_prio[5] = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_4_ADJUST_SAW:
				player->box = BOX_SETT_4;
				player->sett->tool_prio[6] = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_4_ADJUST_SCYTHE:
				player->box = BOX_SETT_4;
				player->sett->tool_prio[4] = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_4_ADJUST_PICK:
				player->box = BOX_SETT_4;
				player->sett->tool_prio[7] = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_4_ADJUST_PINCER:
				player->box = BOX_SETT_4;
				player->sett->tool_prio[8] = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_4_ADJUST_CLEAVER:
				player->box = BOX_SETT_4;
				player->sett->tool_prio[3] = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_4_ADJUST_ROD:
				player->box = BOX_SETT_4;
				player->sett->tool_prio[2] = get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_5_6_ITEM_1:
			case ACTION_SETT_5_6_ITEM_2:
			case ACTION_SETT_5_6_ITEM_3:
			case ACTION_SETT_5_6_ITEM_4:
			case ACTION_SETT_5_6_ITEM_5:
			case ACTION_SETT_5_6_ITEM_6:
			case ACTION_SETT_5_6_ITEM_7:
			case ACTION_SETT_5_6_ITEM_8:
			case ACTION_SETT_5_6_ITEM_9:
			case ACTION_SETT_5_6_ITEM_10:
			case ACTION_SETT_5_6_ITEM_11:
			case ACTION_SETT_5_6_ITEM_12:
			case ACTION_SETT_5_6_ITEM_13:
			case ACTION_SETT_5_6_ITEM_14:
			case ACTION_SETT_5_6_ITEM_15:
			case ACTION_SETT_5_6_ITEM_16:
			case ACTION_SETT_5_6_ITEM_17:
			case ACTION_SETT_5_6_ITEM_18:
			case ACTION_SETT_5_6_ITEM_19:
			case ACTION_SETT_5_6_ITEM_20:
			case ACTION_SETT_5_6_ITEM_21:
			case ACTION_SETT_5_6_ITEM_22:
			case ACTION_SETT_5_6_ITEM_23:
			case ACTION_SETT_5_6_ITEM_24:
			case ACTION_SETT_5_6_ITEM_25:
			case ACTION_SETT_5_6_ITEM_26:
				activate_sett_5_6_item(player, 26-(action-ACTION_SETT_5_6_ITEM_1));
				break;
			case ACTION_SETT_5_6_TOP:
				move_sett_5_6_item(player, 1, 1);
				break;
			case ACTION_SETT_5_6_UP:
				move_sett_5_6_item(player, 1, 0);
				break;
			case ACTION_SETT_5_6_DOWN:
				move_sett_5_6_item(player, 0, 0);
				break;
			case ACTION_SETT_5_6_BOTTOM:
				move_sett_5_6_item(player, 0, 1);
				break;
				/* TODO */
			case ACTION_SHOW_OPTIONS:
				player->click &= ~BIT(6);
				player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
				player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
				player->box = BOX_OPTIONS;
				break;
				/* TODO */
			case ACTION_SETT_8_CYCLE:
				player->sett->flags |= BIT(2) | BIT(4);
				player->sett->field_170 = 1200;
				sfx_play_clip(SFX_ACCEPTED);
				break;
				/* TODO */
			case ACTION_DEFAULT_SETT_1:
				player->box = BOX_SETT_1;
				player_sett_reset_food_priority(player->sett);
				break;
			case ACTION_DEFAULT_SETT_2:
				player->box = BOX_SETT_2;
				player_sett_reset_planks_priority(player->sett);
				player_sett_reset_steel_priority(player->sett);
				break;
			case ACTION_DEFAULT_SETT_5_6:
				player->box = player->clkmap;
				switch (player->clkmap) {
				case BOX_SETT_5:
					player_sett_reset_flag_priority(player->sett);
					break;
				case BOX_SETT_6:
					player_sett_reset_inventory_priority(player->sett);
					break;
				default:
					NOT_REACHED();
					break;
				}
				break;
			case ACTION_BUILD_STOCK:
				globals.building_type = BUILDING_STOCK;
				build_advanced_building(player);
				break;
			case ACTION_SHOW_CASTLE_SERF:
				player->box = BOX_CASTLE_SERF;
				break;
			case ACTION_SHOW_RESDIR:
				player->box = BOX_RESDIR;
				break;
			case ACTION_SHOW_CASTLE_RES:
				player->box = BOX_CASTLE_RES;
				break;
			case ACTION_SEND_GEOLOGIST:
				handle_send_geologist(player);
				break;
				/* TODO */
			case ACTION_SHOW_SETT_8:
				player->box = BOX_SETT_8;
				break;
			case ACTION_SHOW_SETT_6:
				player->box = BOX_SETT_6;
				break;
			case ACTION_SETT_8_ADJUST_RATE:
				player->sett->serf_to_knight_rate = get_slider_click_value(x - clkmap[1]);
				player->box = player->clkmap;
				break;
			case ACTION_SETT_8_TRAIN_1:
				sett_8_train(player, 1);
				break;
			case ACTION_SETT_8_TRAIN_5:
				sett_8_train(player, 5);
				break;
			case ACTION_SETT_8_TRAIN_20:
				sett_8_train(player, 20);
				break;
			case ACTION_SETT_8_TRAIN_100:
				sett_8_train(player, 100);
				break;
			case ACTION_DEFAULT_SETT_3:
				player->box = BOX_SETT_3;
				player_sett_reset_coal_priority(player->sett);
				player_sett_reset_wheat_priority(player->sett);
				break;
			case ACTION_SETT_8_SET_COMBAT_MODE_WEAK:
				player->sett->flags &= ~BIT(1);
				player->box = player->clkmap;
				sfx_play_clip(SFX_ACCEPTED);
				break;
			case ACTION_SETT_8_SET_COMBAT_MODE_STRONG:
				player->sett->flags |= BIT(1);
				player->box = player->clkmap;
				sfx_play_clip(SFX_ACCEPTED);
				break;
				/* TODO ... */
			case ACTION_CLOSE_MESSAGE:
				if ((player->message_box & 0x1f) == 16) {
					/* TODO */
				} else {
					close_box(player);
				}
				break;
			case ACTION_DEFAULT_SETT_4:
				player->box = BOX_SETT_4;
				player_sett_reset_tool_priority(player->sett);
				break;
			case ACTION_SHOW_PLAYER_FACES:
				player->box = BOX_PLAYER_FACES;
				break;
				/* TODO */
			case ACTION_CLOSE_OPTIONS:
				close_box(player);
				break;
			case ACTION_OPTIONS_PATHWAY_SCROLLING_1:
				BIT_INVERT(globals.cfg_left,0);
				sfx_play_clip(SFX_CLICK);
				break;
			case ACTION_OPTIONS_PATHWAY_SCROLLING_2:
				BIT_INVERT(globals.cfg_right,0);
				sfx_play_clip(SFX_CLICK);
				break;
			case ACTION_OPTIONS_FAST_MAP_CLICK_1:
				BIT_INVERT(globals.cfg_left,1);
				sfx_play_clip(SFX_CLICK);
				break;
			case ACTION_OPTIONS_FAST_MAP_CLICK_2:
				BIT_INVERT(globals.cfg_right,1);
				sfx_play_clip(SFX_CLICK);
				break;
			case ACTION_OPTIONS_FAST_BUILDING_1:
				BIT_INVERT(globals.cfg_left,2);
				sfx_play_clip(SFX_CLICK);
				break;
			case ACTION_OPTIONS_FAST_BUILDING_2:
				BIT_INVERT(globals.cfg_right,2);
				sfx_play_clip(SFX_CLICK);
				break;
			case ACTION_OPTIONS_MESSAGE_COUNT_1: {
				if (BIT_TEST(globals.cfg_left,3)) {
					BIT_INVERT(globals.cfg_left, 3);
					globals.cfg_left |= BIT(4);
				} else if (BIT_TEST(globals.cfg_left,4)) {
					BIT_INVERT(globals.cfg_left, 4);
					globals.cfg_left |= BIT(5);
				} else if (BIT_TEST(globals.cfg_left,5)) {
					BIT_INVERT(globals.cfg_left, 5);
				} else {
					globals.cfg_left |= BIT(3) | BIT(4) | BIT(5);
				}
				sfx_play_clip(SFX_CLICK);
				break;
			}
			case ACTION_OPTIONS_MESSAGE_COUNT_2:
				if (BIT_TEST(globals.cfg_right,3)) {
					BIT_INVERT(globals.cfg_right, 3);
					globals.cfg_left |= BIT(4);
				} else if (BIT_TEST(globals.cfg_right,4)) {
					BIT_INVERT(globals.cfg_right, 4);
					globals.cfg_left |= BIT(5);
				} else if (BIT_TEST(globals.cfg_right,5)) {
					BIT_INVERT(globals.cfg_right, 5);
				} else {
					globals.cfg_right |= BIT(3) | BIT(4) | BIT(5);
				}
				sfx_play_clip(SFX_CLICK);
				break;
				/* TODO */
			case ACTION_SETT_8_CASTLE_DEF_DEC:
				player->sett->castle_knights_wanted = max(1, player->sett->castle_knights_wanted-1);
				player->box = player->clkmap;
				break;
			case ACTION_SETT_8_CASTLE_DEF_INC:
				player->sett->castle_knights_wanted = min(player->sett->castle_knights_wanted+1, 99);
				player->box = player->clkmap;
				break;
			case ACTION_OPTIONS_MUSIC:
				midi_enable(!midi_is_enabled());
				sfx_play_clip(SFX_CLICK);
				break;
			case ACTION_OPTIONS_SVGA:
				/* TODO */
				sfx_play_clip(SFX_CLICK);
				break;
			case ACTION_OPTIONS_VOLUME_MINUS:
				audio_volume_down();
				sfx_play_clip(SFX_CLICK);
				break;
			case ACTION_OPTIONS_VOLUME_PLUS:
				audio_volume_up();
				sfx_play_clip(SFX_CLICK);
				break;
			case ACTION_DEMOLISH:
				do_demolish(player);
				close_box(player);
				break;
			default:
				LOGW("gui", "unhandled action %i", action);
				break;
			}
			return 0;
		}
		clkmap += 5;
	}

	return -1;
}

static void
handle_box_close_clk(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_CLOSE_BOX, 112, 127, 128, 143
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_box_options_clk(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_CLOSE_OPTIONS, 56, 71, 0, 15,
		ACTION_OPTIONS_PATHWAY_SCROLLING_1, 0, 15, 28, 43,
		ACTION_OPTIONS_FAST_MAP_CLICK_1, 0, 15, 48, 63,
		ACTION_OPTIONS_FAST_BUILDING_1, 0, 15, 68, 83,
		ACTION_OPTIONS_PATHWAY_SCROLLING_2, 112, 127, 28, 43,
		ACTION_OPTIONS_FAST_MAP_CLICK_2, 112, 127, 48, 63,
		ACTION_OPTIONS_FAST_BUILDING_2, 112, 127, 68, 83,
		ACTION_OPTIONS_MESSAGE_COUNT_1, 0, 8, 88, 95,
		ACTION_OPTIONS_MESSAGE_COUNT_2, 120, 127, 88, 95,
		ACTION_OPTIONS_MUSIC, 0, 15, 106, 121,
		ACTION_OPTIONS_SVGA, 112, 127, 106, 121,
		ACTION_OPTIONS_VOLUME_MINUS, 96, 111, 126, 141,
		ACTION_OPTIONS_VOLUME_PLUS, 112, 127, 126, 141,
		ACTION_OPTIONS_RIGHT_SIDE, 88, 127, 0, 15,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_mine_building_clk(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_BUILD_STONEMINE, 16, 48, 8, 72,
		ACTION_BUILD_COALMINE, 64, 96, 8, 72,
		ACTION_BUILD_IRONMINE, 32, 64, 77, 141,
		ACTION_BUILD_GOLDMINE, 80, 112, 77, 141,
		ACTION_BUILD_FLAG, 10, 26, 114, 134,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_basic_building_clk(player_t *player, int x, int y, int flip)
{
	const int clkmap[] = {
		ACTION_BLD_FLIP_PAGE, 0, 15, 129, 143,
		ACTION_BUILD_STONECUTTER, 16, 48, 13, 41,
		ACTION_BUILD_HUT, 80, 112, 13, 39,
		ACTION_BUILD_LUMBERJACK, 0, 32, 58, 81,
		ACTION_BUILD_FORESTER, 48, 80, 56, 81,
		ACTION_BUILD_FISHER, 96, 128, 55, 84,
		ACTION_BUILD_MILL, 16, 48, 92, 137,
		ACTION_BUILD_FLAG, 58, 74, 108, 128,
		ACTION_BUILD_BOATBUILDER, 80, 112, 87, 139,
		-1
	};

	const int *c = clkmap;
	if (!flip) c += 5; /* Skip flip button */

	handle_clickmap(player, x, y, c);
}

static void
handle_adv_1_building_clk(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_BLD_FLIP_PAGE, 0, 15, 129, 143,
		ACTION_BUILD_BUTCHER, 0, 64, 15, 40,
		ACTION_BUILD_WEAPONSMITH, 64, 128, 15, 40,
		ACTION_BUILD_STEELSMELTER, 0, 48, 50, 88,
		ACTION_BUILD_SAWMILL, 64, 112, 50, 90,
		ACTION_BUILD_BAKER, 16, 64, 100, 132,
		ACTION_BUILD_GOLDSMELTER, 80, 128, 96, 135,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_adv_2_building_clk(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_BLD_FLIP_PAGE, 0, 15, 129, 143,
		ACTION_BUILD_FORTRESS, 64, 127, 87, 142,
		ACTION_BUILD_TOWER, 16, 63, 99, 141,
		ACTION_BUILD_TOOLMAKER, 0, 63, 1, 48,
		ACTION_BUILD_FARM, 64, 127, 1, 42,
		ACTION_BUILD_PIGFARM, 64, 127, 45, 85,
		ACTION_BUILD_STOCK, 0, 47, 50, 97,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_stat_select_click(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_SHOW_STAT_1, 8, 39, 12, 43,
		ACTION_SHOW_STAT_2, 48, 79, 12, 43,
		ACTION_SHOW_STAT_3, 88, 119, 12, 43,
		ACTION_SHOW_STAT_4, 8, 39, 56, 87,
		ACTION_SHOW_STAT_BLD, 48, 79, 56, 87,
		ACTION_SHOW_STAT_6, 88, 119, 56, 87,
		ACTION_SHOW_STAT_7, 8, 39, 100, 131,
		ACTION_SHOW_STAT_8, 48, 79, 100, 131,
		ACTION_CLOSE_BOX, 112, 127, 128, 143,
		ACTION_SHOW_SETT_SELECT_FILE, 96, 111, 104, 119,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_stat_3_4_6_click(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_SHOW_STAT_SELECT, 0, 127, 0, 143,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_stat_bld_click(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_SHOW_STAT_SELECT, 112, 127, 128, 143,
		ACTION_STAT_BLD_FLIP, 0, 15, 128, 143,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_stat_8_click(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_SETT_8_SET_ASPECT_ALL, 16, 31, 112, 127,
		ACTION_SETT_8_SET_ASPECT_LAND, 32, 47, 112, 127,
		ACTION_SETT_8_SET_ASPECT_BUILDINGS, 16, 31, 128, 143,
		ACTION_SETT_8_SET_ASPECT_MILITARY, 32, 47, 128, 143,

		ACTION_SETT_8_SET_SCALE_30_MIN, 64, 79, 112, 127,
		ACTION_SETT_8_SET_SCALE_60_MIN, 80, 95, 112, 127,
		ACTION_SETT_8_SET_SCALE_600_MIN, 64, 79, 128, 143,
		ACTION_SETT_8_SET_SCALE_3000_MIN, 80, 95, 128, 143,

		ACTION_SHOW_PLAYER_FACES, 112, 127, 112, 125,
		ACTION_SHOW_STAT_SELECT, 112, 127, 128, 143,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_stat_7_click(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_STAT_7_SELECT_LUMBER, 0, 15, 75, 90,
		ACTION_STAT_7_SELECT_PLANK, 16, 31, 75, 90,
		ACTION_STAT_7_SELECT_STONE, 32, 47, 75, 90,
		ACTION_STAT_7_SELECT_COAL, 0, 15, 91, 106,
		ACTION_STAT_7_SELECT_IRONORE, 16, 31, 91, 106,
		ACTION_STAT_7_SELECT_GOLDORE, 32, 47, 91, 106,
		ACTION_STAT_7_SELECT_BOAT, 0, 15, 107, 122,
		ACTION_STAT_7_SELECT_STEEL, 16, 31, 107, 122,
		ACTION_STAT_7_SELECT_GOLDBAR, 32, 47, 107, 122,

		ACTION_STAT_7_SELECT_SWORD, 56, 71, 83, 98,
		ACTION_STAT_7_SELECT_SHIELD, 56, 71, 99, 114,

		ACTION_STAT_7_SELECT_SHOVEL, 80, 95, 75, 90,
		ACTION_STAT_7_SELECT_HAMMER, 96, 111, 75, 90,
		ACTION_STAT_7_SELECT_AXE, 112, 127, 75, 90,
		ACTION_STAT_7_SELECT_SAW, 80, 95, 91, 106,
		ACTION_STAT_7_SELECT_PICK, 96, 111, 91, 106,
		ACTION_STAT_7_SELECT_SCYTHE, 112, 127, 91, 106,
		ACTION_STAT_7_SELECT_CLEAVER, 80, 95, 107, 122,
		ACTION_STAT_7_SELECT_PINCER, 96, 111, 107, 122,
		ACTION_STAT_7_SELECT_ROD, 112, 127, 107, 122,

		ACTION_STAT_7_SELECT_FISH, 8, 23, 125, 140,
		ACTION_STAT_7_SELECT_PIG, 24, 39, 125, 140,
		ACTION_STAT_7_SELECT_MEAT, 40, 55, 125, 140,
		ACTION_STAT_7_SELECT_WHEAT, 56, 71, 125, 140,
		ACTION_STAT_7_SELECT_FLOUR, 72, 87, 125, 140,
		ACTION_STAT_7_SELECT_BREAD, 88, 103, 125, 140,

		ACTION_SHOW_STAT_SELECT, 112, 127, 128, 143,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_stat_1_2_click(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_SHOW_STAT_SELECT, 0, 127, 0, 143,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_ground_analysis_clk(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_CLOSE_GROUND_ANALYSIS, 112, 127, 128, 143,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_sett_select_clk(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_SHOW_QUIT, 0, 31, 128, 143,
		ACTION_SHOW_OPTIONS, 32, 63, 128, 143,
		ACTION_SHOW_SAVE, 64, 95, 128, 143,

		ACTION_SHOW_SETT_1, 8, 39, 8, 39,
		ACTION_SHOW_SETT_2, 48, 79, 8, 39,
		ACTION_SHOW_SETT_3, 88, 119, 8, 39,
		ACTION_SHOW_SETT_4, 8, 39, 48, 79,
		ACTION_SHOW_SETT_5, 48, 79, 48, 79,
		ACTION_SHOW_SETT_6, 88, 119, 48, 79,
		ACTION_SHOW_SETT_7, 8, 39, 88, 119,
		ACTION_SHOW_SETT_8, 48, 79, 88, 119,

		ACTION_CLOSE_SETT_BOX, 112, 127, 128, 143,
		ACTION_SHOW_STAT_SELECT_FILE, 96, 111, 104, 119,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_sett_1_click(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_SETT_1_ADJUST_STONEMINE, 32, 95, 22, 27,
		ACTION_SETT_1_ADJUST_COALMINE, 0, 63, 42, 47,
		ACTION_SETT_1_ADJUST_IRONMINE, 64, 127, 115, 120,
		ACTION_SETT_1_ADJUST_GOLDMINE, 32, 95, 134, 139,

		ACTION_SHOW_SETT_SELECT, 112, 127, 128, 143,
		ACTION_DEFAULT_SETT_1, 8, 23, 8, 23,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_sett_2_click(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_SETT_2_ADJUST_CONSTRUCTION, 0, 63, 27, 32,
		ACTION_SETT_2_ADJUST_BOATBUILDER, 0, 63, 37, 42,
		ACTION_SETT_2_ADJUST_TOOLMAKER_PLANKS, 64, 127, 45, 50,
		ACTION_SETT_2_ADJUST_TOOLMAKER_STEEL, 64, 127, 104, 109,
		ACTION_SETT_2_ADJUST_WEAPONSMITH, 0, 63, 131, 136,

		ACTION_SHOW_SETT_SELECT, 112, 127, 128, 143,
		ACTION_DEFAULT_SETT_2, 104, 119, 8, 23,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_sett_3_click(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_SETT_3_ADJUST_STEELSMELTER, 0, 63, 40, 45,
		ACTION_SETT_3_ADJUST_GOLDSMELTER, 64, 127, 40, 45,
		ACTION_SETT_3_ADJUST_WEAPONSMITH, 32, 95, 48, 53,
		ACTION_SETT_3_ADJUST_PIGFARM, 0, 63, 93, 98,
		ACTION_SETT_3_ADJUST_MILL, 64, 127, 119, 124,

		ACTION_SHOW_SETT_SELECT, 112, 127, 128, 143,
		ACTION_DEFAULT_SETT_3, 8, 23, 60, 75,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_knight_level_click(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_KNIGHT_LEVEL_CLOSEST_MIN_DEC, 32, 47, 2, 17,
		ACTION_KNIGHT_LEVEL_CLOSEST_MIN_INC, 48, 63, 2, 17,
		ACTION_KNIGHT_LEVEL_CLOSEST_MAX_DEC, 32, 47, 18, 33,
		ACTION_KNIGHT_LEVEL_CLOSEST_MAX_INC, 48, 63, 18, 33,
		ACTION_KNIGHT_LEVEL_CLOSE_MIN_DEC, 32, 47, 36, 51,
		ACTION_KNIGHT_LEVEL_CLOSE_MIN_INC, 48, 63, 36, 51,
		ACTION_KNIGHT_LEVEL_CLOSE_MAX_DEC, 32, 47, 52, 67,
		ACTION_KNIGHT_LEVEL_CLOSE_MAX_INC, 48, 63, 52, 67,
		ACTION_KNIGHT_LEVEL_FAR_MIN_DEC, 32, 47, 70, 85,
		ACTION_KNIGHT_LEVEL_FAR_MIN_INC, 48, 63, 70, 85,
		ACTION_KNIGHT_LEVEL_FAR_MAX_DEC, 32, 47, 86, 101,
		ACTION_KNIGHT_LEVEL_FAR_MAX_INC, 48, 63, 86, 101,
		ACTION_KNIGHT_LEVEL_FARTHEST_MIN_DEC, 32, 47, 104, 119,
		ACTION_KNIGHT_LEVEL_FARTHEST_MIN_INC, 48, 63, 104, 119,
		ACTION_KNIGHT_LEVEL_FARTHEST_MAX_DEC, 32, 47, 120, 135,
		ACTION_KNIGHT_LEVEL_FARTHEST_MAX_INC, 48, 63, 120, 135,

		ACTION_SHOW_SETT_SELECT, 112, 127, 128, 143,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_sett_4_click(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_SETT_4_ADJUST_SHOVEL, 32, 95, 4, 11,
		ACTION_SETT_4_ADJUST_HAMMER, 32, 95, 20, 27,
		ACTION_SETT_4_ADJUST_AXE, 32, 95, 36, 43,
		ACTION_SETT_4_ADJUST_SAW, 32, 95, 52, 59,
		ACTION_SETT_4_ADJUST_SCYTHE, 32, 95, 68, 75,
		ACTION_SETT_4_ADJUST_PICK, 32, 95, 84, 91,
		ACTION_SETT_4_ADJUST_PINCER, 32, 95, 100, 107,
		ACTION_SETT_4_ADJUST_CLEAVER, 32, 95, 116, 123,
		ACTION_SETT_4_ADJUST_ROD, 32, 95, 132, 139,

		ACTION_SHOW_SETT_SELECT, 112, 127, 128, 143,
		ACTION_DEFAULT_SETT_4, 104, 119, 8, 23,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_sett_5_6_click(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_SETT_5_6_ITEM_1, 40, 55, 4, 19,
		ACTION_SETT_5_6_ITEM_2, 56, 71, 6, 21,
		ACTION_SETT_5_6_ITEM_3, 72, 87, 8, 23,
		ACTION_SETT_5_6_ITEM_4, 88, 103, 10, 25,
		ACTION_SETT_5_6_ITEM_5, 104, 119, 12, 27,
		ACTION_SETT_5_6_ITEM_6, 104, 119, 28, 43,
		ACTION_SETT_5_6_ITEM_7, 88, 103, 30, 45,
		ACTION_SETT_5_6_ITEM_8, 72, 87, 32, 47,
		ACTION_SETT_5_6_ITEM_9, 56, 71, 34, 49,
		ACTION_SETT_5_6_ITEM_10, 40, 55, 36, 51,
		ACTION_SETT_5_6_ITEM_11, 24, 39, 38, 53,
		ACTION_SETT_5_6_ITEM_12, 8, 23, 40, 55,
		ACTION_SETT_5_6_ITEM_13, 8, 23, 56, 71,
		ACTION_SETT_5_6_ITEM_14, 24, 39, 58, 73,
		ACTION_SETT_5_6_ITEM_15, 40, 55, 60, 75,
		ACTION_SETT_5_6_ITEM_16, 56, 71, 62, 77,
		ACTION_SETT_5_6_ITEM_17, 72, 87, 64, 79,
		ACTION_SETT_5_6_ITEM_18, 88, 103, 66, 81,
		ACTION_SETT_5_6_ITEM_19, 104, 119, 68, 83,
		ACTION_SETT_5_6_ITEM_20, 104, 119, 84, 99,
		ACTION_SETT_5_6_ITEM_21, 88, 103, 86, 101,
		ACTION_SETT_5_6_ITEM_22, 72, 87, 88, 103,
		ACTION_SETT_5_6_ITEM_23, 56, 71, 90, 105,
		ACTION_SETT_5_6_ITEM_24, 40, 55, 92, 107,
		ACTION_SETT_5_6_ITEM_25, 24, 39, 94, 109,
		ACTION_SETT_5_6_ITEM_26, 8, 23, 96, 111,

		ACTION_SETT_5_6_TOP, 8, 23, 120, 135,
		ACTION_SETT_5_6_UP, 24, 39, 120, 135,
		ACTION_SETT_5_6_DOWN, 72, 87, 120, 135,
		ACTION_SETT_5_6_BOTTOM, 88, 103, 120, 135,

		ACTION_SHOW_SETT_SELECT, 112, 127, 128, 143,
		ACTION_DEFAULT_SETT_5_6, 8, 23, 4, 19,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_castle_res_clk(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_CLOSE_BOX, 112, 127, 128, 143,
		ACTION_SHOW_CASTLE_SERF, 96, 111, 128, 143,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_transport_info_clk(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_UNKNOWN_TP_INFO_FLAG, 56, 71, 51, 65,
		ACTION_SEND_GEOLOGIST, 16, 31, 96, 111,
		ACTION_CLOSE_BOX, 112, 127, 128, 143,
		-1
	};
	if (!BIT_TEST(globals.split, 5)) { /* Not demo mode */
		handle_clickmap(player, x, y, clkmap);
	} else {
		handle_box_close_clk(player, x, y);
	}
}

static void
handle_castle_serf_clk(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_CLOSE_BOX, 112, 127, 128, 143,
		ACTION_SHOW_RESDIR, 96, 111, 128, 143,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_resdir_clk(player_t *player, int x, int y)
{
	const int mode_clkmap[] = {
		ACTION_RES_MODE_IN, 72, 87, 16, 31,
		ACTION_RES_MODE_STOP, 72, 87, 32, 47,
		ACTION_RES_MODE_OUT, 72, 87, 48, 63,
		ACTION_SERF_MODE_IN, 72, 87, 80, 95,
		ACTION_SERF_MODE_STOP, 72, 87, 96, 111,
		ACTION_SERF_MODE_OUT, 72, 87, 112, 127,
		-1
	};

	const int clkmap[] = {
		ACTION_CLOSE_BOX, 112, 127, 128, 143,
		ACTION_SHOW_CASTLE_RES, 96, 111, 128, 143,
		-1
	};

	int r = -1;
	if (!BIT_TEST(globals.split, 5)) { /* Not demo mode */
		r = handle_clickmap(player, x, y, mode_clkmap);
	}
	if (r < 0) handle_clickmap(player, x, y, clkmap);
}

static void
handle_sett_8_click(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_SETT_8_ADJUST_RATE, 32, 95, 12, 19,

		ACTION_SETT_8_TRAIN_1, 16, 31, 28, 43,
		ACTION_SETT_8_TRAIN_5, 32, 47, 28, 43,
		ACTION_SETT_8_TRAIN_20, 16, 31, 44, 59,
		ACTION_SETT_8_TRAIN_100, 32, 47, 44, 59,

		ACTION_SETT_8_SET_COMBAT_MODE_WEAK, 48, 63, 84, 99,
		ACTION_SETT_8_SET_COMBAT_MODE_STRONG, 48, 63, 100, 115,

		ACTION_SETT_8_CYCLE, 80, 111, 84, 115,

		ACTION_SETT_8_CASTLE_DEF_DEC, 24, 39, 120, 135,
		ACTION_SETT_8_CASTLE_DEF_INC, 72, 87, 120, 135,

		ACTION_SHOW_SETT_SELECT, 112, 127, 128, 143,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_message_clk(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_CLOSE_MESSAGE, 112, 127, 128, 143,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_player_faces_click(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_SHOW_STAT_8, 0, 127, 0, 143,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_box_demolish_clk(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_CLOSE_BOX, 112, 127, 128, 143,
		ACTION_DEMOLISH, 56, 71, 45, 60,
		-1
	};
	handle_clickmap(player, x, y, clkmap);
}

static void
handle_popup_click(player_t *player, int x, int y)
{
	switch (player->clkmap) {
	case BOX_MINE_BUILDING:
		handle_mine_building_clk(player, x, y);
		break;
	case BOX_BASIC_BLD:
		handle_basic_building_clk(player, x, y, 0);
		break;
	case BOX_BASIC_BLD_FLIP:
		handle_basic_building_clk(player, x, y, 1);
		break;
	case BOX_ADV_1_BLD:
		handle_adv_1_building_clk(player, x, y);
		break;
	case BOX_ADV_2_BLD:
		handle_adv_2_building_clk(player, x, y);
		break;
	case BOX_STAT_SELECT:
		handle_stat_select_click(player, x, y);
		break;
	case BOX_STAT_4:
	case BOX_STAT_6:
	case BOX_STAT_3:
		handle_stat_3_4_6_click(player, x, y);
		break;
	case BOX_STAT_BLD_1:
	case BOX_STAT_BLD_2:
	case BOX_STAT_BLD_3:
	case BOX_STAT_BLD_4:
		handle_stat_bld_click(player, x, y);
		break;
	case BOX_STAT_8:
		handle_stat_8_click(player, x, y);
		break;
	case BOX_STAT_7:
		handle_stat_7_click(player, x, y);
		break;
	case BOX_STAT_1:
	case BOX_STAT_2:
		handle_stat_1_2_click(player, x, y);
		break;
		/* TODO */
	case BOX_GROUND_ANALYSIS:
		handle_ground_analysis_clk(player, x, y);
		break;
		/* TODO ... */
	case BOX_SETT_SELECT:
	case BOX_SETT_SELECT_FILE:
		handle_sett_select_clk(player, x, y);
		break;
	case BOX_SETT_1:
		handle_sett_1_click(player, x, y);
		break;
	case BOX_SETT_2:
		handle_sett_2_click(player, x, y);
		break;
	case BOX_SETT_3:
		handle_sett_3_click(player, x, y);
		break;
	case BOX_KNIGHT_LEVEL:
		handle_knight_level_click(player, x, y);
		break;
	case BOX_SETT_4:
		handle_sett_4_click(player, x, y);
		break;
	case BOX_SETT_5:
		handle_sett_5_6_click(player, x, y);
		break;
		/* TODO */
	case BOX_OPTIONS:
		handle_box_options_clk(player, x, y);
		break;
		/* TODO */
	case BOX_CASTLE_RES:
		handle_castle_res_clk(player, x, y);
		break;
	case BOX_MINE_OUTPUT:
		handle_box_close_clk(player, x, y);
		break;
	case BOX_ORDERED_BLD:
		handle_box_close_clk(player, x, y);
		break;
	case BOX_DEFENDERS:
		handle_box_close_clk(player, x, y);
		break;
	case BOX_TRANSPORT_INFO:
		handle_transport_info_clk(player, x, y);
		break;
	case BOX_CASTLE_SERF:
		handle_castle_serf_clk(player, x, y);
		break;
	case BOX_RESDIR:
		handle_resdir_clk(player, x, y);
		break;
	case BOX_SETT_8:
		handle_sett_8_click(player, x, y);
		break;
	case BOX_SETT_6:
		handle_sett_5_6_click(player, x, y);
		break;
		/* TODO ... */
	case BOX_MESSAGE:
		handle_message_clk(player, x, y);
		break;
	case BOX_BLD_STOCK:
		handle_box_close_clk(player, x, y);
		break;
	case BOX_PLAYER_FACES:
		handle_player_faces_click(player, x, y);
		break;
	case BOX_DEMOLISH:
		handle_box_demolish_clk(player, x, y);
		break;
	default:
		LOGD("gui", "unhandled box: %i", player->clkmap);
		break;
	}
}

static void
handle_message_icon_click(player_t *player)
{
	int type = player->sett->msg_queue_type[0] & 0x1f;

	if (type == 16) {
		/* TODO */
	}

	player->message_box = player->sett->msg_queue_type[0];

	if (BIT_TEST(0x8f3fe, type)) {
		/* Move screen to new position */
		map_pos_t new_pos = player->sett->msg_queue_pos[0];
		int col = new_pos & globals.map_col_mask;
		int row = (new_pos >> globals.map_row_shift) & globals.map_row_mask;

		viewport_move_to_map_pos(&viewport, col, row);
		player->sett->map_cursor_col = col;
		player->sett->map_cursor_row = row;
	}

	player->box = BOX_MESSAGE;
	player->click &= ~BIT(6);
	player->click &= ~BIT(1);

	player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
	player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
	player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
	player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
	player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;

	/* Move notifications forward in the queue. */
	int i;
	for (i = 1; i < 64 && player->sett->msg_queue_type[i] != 0; i++) {
		player->sett->msg_queue_type[i-1] = player->sett->msg_queue_type[i];
		player->sett->msg_queue_pos[i-1] = player->sett->msg_queue_pos[i];
	}
	player->sett->msg_queue_type[i-1] = 0;
}

/* Handle player click in the bottom panel. */
static void
handle_bottom_panel_click(player_t *player, int x, int y)
{
	if (x <= player->msg_icon_x - player->pointer_x_off ||
	    x > player->msg_icon_x - player->pointer_x_off + 12) {
		if (x <= player->timer_icon_x - player->pointer_x_off - 4 ||
		    x > player->timer_icon_x - player->pointer_x_off + 8) {
			if (y < 4 || y >= 36 || x < player->panel_btns_first_x) return;
			x -= player->panel_btns_first_x;

			/* Figure out what button was clicked */
			int btn = 0;
			while (1) {
				if (x < 32) {
					if (btn < 5) break;
					else return;
				}
				btn += 1;
				if (x < player->panel_btns_dist) return;
				x -= player->panel_btns_dist;
			}
			handle_panel_btn_click(player, btn);
		} else {
			/* Timer bar click */
			if (BIT_TEST(globals.svga, 3)) { /* Game has started */
				if ((BIT_TEST(globals.split, 6) && /* Coop mode */
				     BIT_TEST(player->click, 0)) ||
				    player->sett->timers_count >= 64) {
					sfx_play_clip(SFX_NOT_ACCEPTED);
					return;
				}

				if (BIT_TEST(player->click, 1)) {
					/* Call to map position */
					int timer_id = player->sett->timers_count++;
					int timer_length = -1;

					if (y < 7) timer_length = 5*60*100;
					else if (y < 14) timer_length = 10*60*100;
					else if (y < 21) timer_length = 20*60*100;
					else if (y < 28) timer_length = 30*60*100;
					else timer_length = 60*60*100;

					player->sett->timers[timer_id].timeout = timer_length;
					player->sett->timers[timer_id].pos = MAP_POS(player->sett->map_cursor_col,
										     player->sett->map_cursor_row);
				} else {
					/* Call to box (+ map) position */
					/* TODO */
				}

				sfx_play_clip(SFX_ACCEPTED);
			}
		}
	} else {
		/* Message bar click */
		if (BIT_TEST(globals.svga, 3)) { /* Game has started */
			if (y < 16) {
				/* Message icon */
				if (!BIT_TEST(player->msg_flags, 0) || /* No message */
				    BIT_TEST(player->click, 7)) { /* Building road */
					sfx_play_clip(SFX_CLICK);
				} else if (player->clkmap == BOX_LOAD_ARCHIVE ||
					   player->clkmap == BOX_LOAD_SAVE ||
					   player->clkmap == BOX_DISK_MSG ||
					   player->clkmap == BOX_QUIT_CONFIRM ||
					   player->clkmap == BOX_NO_SAVE_QUIT_CONFIRM ||
					   player->clkmap == BOX_OPTIONS) {
					sfx_play_clip(SFX_NOT_ACCEPTED);
				} else {
					player->flags &= ~BIT(6);
					if (!BIT_TEST(player->msg_flags, 3)) {
						player->msg_flags |= BIT(4);
						player->msg_flags |= BIT(3);
						viewport_get_current_map_pos(&viewport,
									     &player->return_col_game_area,
									     &player->return_row_game_area);
					}

					handle_message_icon_click(player);
					player->msg_flags |= BIT(1);
					player->return_timeout = 2000;
					sfx_play_clip(SFX_CLICK);
				}
			} else if (y >= 28) {
				/* Return arrow */
				if (BIT_TEST(player->msg_flags, 3)) { /* Return arrow present */
					player->msg_flags |= BIT(4);
					player->msg_flags &= ~BIT(3);

					player->return_timeout = 0;
					viewport_move_to_map_pos(&viewport, player->return_col_game_area,
								 player->return_row_game_area);

					if (player->clkmap == BOX_MESSAGE) close_box(player);
					sfx_play_clip(SFX_CLICK);
				}
			}
		}
	}
}

/* Handle click in the main viewport. */
static void
handle_map_click(player_t *player, int x, int y)
{
	if (x >= 0 && y >= 0 &&
	    x < player->frame_width && y < player->frame_height) {
		map_pos_t clk_pos = viewport_map_pos_from_screen_pix(&viewport, x, y);
		int clk_col = clk_pos & globals.map_col_mask;
		int clk_row = (clk_pos >> globals.map_row_shift) & globals.map_col_mask;

		if (BIT_TEST(player->click, 7)) { /* Building road */
			int y = (clk_col - player->sett->map_cursor_col + 1) & globals.map_col_mask;
			int x = (clk_row - player->sett->map_cursor_row + 1) & globals.map_row_mask;
			dir_t dir = -1;

			if (y == 0) {
				if (x == 1) dir = DIR_LEFT;
				else if (x == 0) dir = DIR_UP_LEFT;
				else return;
			} else if (y == 1) {
				if (x == 2) dir = DIR_DOWN;
				else if (x == 0) dir = DIR_UP;
				else return;
			} else if (y == 2) {
				if (x == 1) dir = DIR_RIGHT;
				else if (x == 2) dir = DIR_DOWN_RIGHT;
				else return;
			} else {
				return;
			}

			if (BIT_TEST(player->field_D0, dir)) {
				map_pos_t pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
				map_1_t *map = globals.map_mem2_ptr;
				dir_t dir_rev = DIR_REVERSE(dir);

				if (!BIT_TEST(MAP_PATHS(pos), dir)) { /* No existing path: Create path */
					if (MAP_PATHS(clk_pos) == 0) { /* No paths at destination */
						if (BIT_TEST(player->click, 3)) { /* Special click */
							/* TODO ... */
						} else {
							/* loc_3ABF0 */
							if (MAP_OBJ(clk_pos) == MAP_OBJ_FLAG) { /* Existing flag */
								/* 3AC0A */
								int r = build_road_connect_flag(player, map, clk_pos, dir_rev);
								if (r < 0) {
									sfx_play_clip(SFX_NOT_ACCEPTED);
								} else {
									player->sett->map_cursor_col = clk_col;
									player->sett->map_cursor_row = clk_row;
									map[pos].flags |= BIT(dir);
									map[clk_pos].flags |= BIT(dir_rev);
									player->road_length = 0;
									/* redraw map cursor */
									sfx_play_clip(SFX_ACCEPTED);
								}
								build_road_end(player);
							} else {
								player->road_length += 1;
								map[pos].flags |= BIT(dir);
								map[clk_pos].flags |= BIT(dir_rev);
								sfx_play_clip(SFX_CLICK);

								/* loc_3AD32 */
								player->sett->map_cursor_col = clk_col;
								player->sett->map_cursor_row = clk_row;

								if (BIT_TEST(player->config, 0)) { /* Pathway scrolling */
									/* TODO */
								}

								/*sub_4737E(player->sett->map_cursor_col, player->sett->map_cursor_row);*/
								player->click |= BIT(2);
							}
						}
					} else { /* Dest has existing paths */
						if ((map[clk_pos].obj & 0x7f) == 1) { /* Flag at dest */
							/* 3AC0A */
							int r = build_road_connect_flag(player, map, clk_pos, dir_rev);
							if (r < 0) {
								sfx_play_clip(SFX_NOT_ACCEPTED);
							} else {
								player->sett->map_cursor_col = clk_col;
								player->sett->map_cursor_row = clk_row;
								map[pos].flags |= BIT(dir);
								map[clk_pos].flags |= BIT(dir_rev);
								player->road_length = 0;
								/* redraw map cursor */
								sfx_play_clip(SFX_ACCEPTED);
							}
							build_road_end(player);
						} else { /* No flag at dest */
							if (BIT_TEST(player->click, 3)) { /* Special click */
								/* TODO ... */
							} else {
								player->click |= BIT(2);
								sfx_play_clip(SFX_NOT_ACCEPTED);
							}
						}
					}
				} else { /* Existing path: Delete path */
					player->road_length -= 1;
					map[pos].flags &= ~BIT(dir);
					map[clk_pos].flags &= ~BIT(dir_rev);
					sfx_play_clip(SFX_CLICK);

					/* loc_3AD32 */
					player->sett->map_cursor_col = clk_col;
					player->sett->map_cursor_row = clk_row;

					if (BIT_TEST(player->config, 0)) { /* Pathway scrolling */
						/* TODO */
					}

					/*sub_4737E(player->sett->map_cursor_col, player->sett->map_cursor_row);*/
					player->click |= BIT(2);
				}
			} else {
				player->click |= BIT(2);
				sfx_play_clip(SFX_NOT_ACCEPTED);
			}
		} else if (BIT_TEST(player->config, 2)) { /* Fast building */
			/* TODO ... */
		} else {
			/* 39F5C */
			player->sett->map_cursor_col = clk_col;
			player->sett->map_cursor_row = clk_row;

			player->click |= BIT(2);

			if (BIT_TEST(player->click, 3)) { /* Special click */
				/* 39FCB */
				if (MAP_OBJ(clk_pos) == MAP_OBJ_NONE ||
				    MAP_OBJ(clk_pos) > MAP_OBJ_CASTLE) {
					return;
				}

				if (MAP_OBJ(clk_pos) == MAP_OBJ_FLAG) {
					if (BIT_TEST(globals.split, 5) || /* Demo mode */
					    MAP_OWNER(clk_pos) == player->sett->player_num) {
						player->box = BOX_TRANSPORT_INFO;
					}
				} else { /* Building */
					if (BIT_TEST(globals.split, 5) || /* Demo mode */
					    MAP_OWNER(clk_pos) == player->sett->player_num) {
						building_t *building = game_get_building(MAP_OBJ_INDEX(clk_pos));
						if (BIT_TEST(building->bld, 7)) {
							player->box = BOX_ORDERED_BLD;
						} else if (BUILDING_TYPE(building) == BUILDING_CASTLE) {
							player->box = BOX_CASTLE_RES;
						} else if (BUILDING_TYPE(building) == BUILDING_STOCK) {
							if (!BIT_TEST(building->serf, 4)) return;
							player->box = BOX_CASTLE_RES;
						} else if (BUILDING_TYPE(building) == BUILDING_HUT ||
							   BUILDING_TYPE(building) == BUILDING_TOWER ||
							   BUILDING_TYPE(building) == BUILDING_FORTRESS) {
							player->box = BOX_DEFENDERS;
						} else if (BUILDING_TYPE(building) == BUILDING_STONEMINE ||
							   BUILDING_TYPE(building) == BUILDING_COALMINE ||
							   BUILDING_TYPE(building) == BUILDING_IRONMINE ||
							   BUILDING_TYPE(building) == BUILDING_GOLDMINE) {
							player->box = BOX_MINE_OUTPUT;
						} else {
							player->box = BOX_BLD_STOCK;
						}
					} else {
						/* TODO */
					}
				}

				/* 3A1C7 */
				if (BIT_TEST(globals.split, 5)) { /* Demo mode */
					/* TODO .. */
				} else {
					player->sett->index = MAP_OBJ_INDEX(clk_pos);
				}

				player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
				player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
				player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
				player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
				player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
				player->click &= ~BIT(2);
				player->click &= ~BIT(1);
			} else {
				/* TODO ... */
			}
		}
	}
}

static void
handle_player_click(player_t *player)
{
	player->flags &= ~BIT(2);

	int x = player->pointer_x_clk;
	int y = player->pointer_y_clk;

	if (x >= player->bottom_panel_x && x < player->bottom_panel_x + player->bottom_panel_width &&
	    y >= player->bottom_panel_y && y < player->bottom_panel_y + player->bottom_panel_height) {
		/* Handle panel clicks */
		handle_bottom_panel_click(player, x - player->bottom_panel_x, y - player->bottom_panel_y);
	} else if (!BIT_TEST(player->click, 1) && /* Popup open for clicks */
		   x >= player->popup_x && x < player->popup_x + 144 &&
		   y >= player->popup_y && y < player->popup_y + 160) {
		/* Handle as popup click */
		handle_popup_click(player, x - player->popup_x, y - player->popup_y);
	} else {
		/* Handle map clicks */
		handle_map_click(player, x - player->map_min_x, y - player->map_min_y);
	}
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
			determine_map_cursor_type(player);
		} else { /* Building road */
			determine_map_cursor_type_road(player);
		}

		update_panel_btns_and_map_cursor(player);
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
	/*draw_panel_buttons();*/
	gui_draw_player_popups();

	handle_map_drag(globals.player[0]);
	globals.player[0]->flags &= ~BIT(4);
	globals.player[0]->flags &= ~BIT(7);

	/* TODO */

	if (redraw_landscape) {
		player_t *player = globals.player[0];

#if 0
		/* Clear screen helps debugging */
		gfx_fill_rect(0, 0, sdl_frame_get_width(player->frame),
			      sdl_frame_get_height(player->frame),
			      1, player->frame);
#endif

		/* From draw_player_landscape() */
		/* player->field_1BC = 38; */
		/* player->field_1BE = player->frame_height + 38; */

		viewport_draw(&viewport, player->frame);
		sdl_mark_dirty(viewport.x, viewport.y, viewport.width, viewport.height);
		/* draw_player_extra_game_objs2(); */
		redraw_landscape = 0;
	}

	draw_popups_to_frame();

	/* Redraw bottom panel */
	gui_draw_bottom_frame(&svga_full_frame);
	gui_draw_panel_buttons();

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
	unsigned int last_lmb = 0;

	unsigned int current_ticks = SDL_GetTicks();
	unsigned int accum = 0;
	unsigned int accum_frames = 0;

	int do_loop = 1;
	SDL_Event event;
	while (do_loop) {
		if (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_MOUSEBUTTONUP:
				if (event.button.button == SDL_BUTTON_LEFT) lmb_state = 0;
				else if (event.button.button == SDL_BUTTON_RIGHT) rmb_state = 0;
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (event.button.button == SDL_BUTTON_LEFT) lmb_state = 1;
				else if (event.button.button == SDL_BUTTON_RIGHT) rmb_state = 1;
				update_player_input_click(globals.player[0], event.button.x, event.button.y, lmb_state, rmb_state, current_ticks - last_lmb);
				if (event.button.button == SDL_BUTTON_LEFT) last_lmb = current_ticks;
				break;
			case SDL_MOUSEMOTION:
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
					handle_panel_btn_click(globals.player[0], 0);
					break;
				case SDLK_2:
					handle_panel_btn_click(globals.player[0], 1);
					break;
				case SDLK_3:
					handle_panel_btn_click(globals.player[0], 2);
					break;
				case SDLK_4:
					handle_panel_btn_click(globals.player[0], 3);
					break;
				case SDLK_5:
					handle_panel_btn_click(globals.player[0], 4);
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
						build_road_end(globals.player[0]);
					} else if (globals.player[0]->clkmap != 0) {
						close_box(globals.player[0]);
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
		gui_draw_bottom_frame(&svga_full_frame);
		gui_draw_panel_buttons();

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

	gui_draw_bottom_frame(&svga_full_frame);
	gui_draw_panel_buttons();

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
	globals.map_mem5_ptr = globals.map_mem5;

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

	globals.map_mem5 = malloc(0xa98 * (1 << /*max_map_size+2(?)*/10));
	if (globals.map_mem5 == NULL) abort();

	hand_out_memory_2();
}


#define USAGE					\
	"Usage: %s [-g DATA-FILE]\n"
#define HELP							\
	USAGE							\
	" -d NUM\t\tSet debug output level\n"				\
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
			if (hstr == NULL) fprintf(stdout, argv[0]);
			screen_width = atoi(optarg);
			screen_height = atoi(hstr+1);
		}
			break;
		case 't':
			map_generator = atoi(optarg);
			break;
		default:
			fprintf(stderr, argv[0]);
			exit(EXIT_FAILURE);
			break;
		}
	}

	/* Set up logging */
	log_set_file(stdout);
	log_set_level(log_level);

	LOGI("main", "freeserf %s", FREESERF_VERSION);

	/* Use default data file if none was specified. */
	if (data_file == NULL) {
		data_file = malloc(strlen("SPAE.PA")+1);
		if (data_file == NULL) exit(EXIT_FAILURE);
		strcpy(data_file, "SPAE.PA");
	}

	LOGI("main", "Loading game data from `%s'...", data_file);

	r = gfx_load_file(data_file);
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
