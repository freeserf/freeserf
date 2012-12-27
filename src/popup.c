/*
 * popup.c - Popup GUI component
 *
 * Copyright (C) 2012  Jon Lund Steffensen <jonlst@gmail.com>
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

#include <math.h>

#include "popup.h"
#include "gfx.h"
#include "data.h"
#include "viewport.h"
#include "game.h"
#include "sdl-video.h"
#include "audio.h"
#include "globals.h"
#include "debug.h"
#include "interface.h"
#include "viewport.h"
#include "minimap.h"


/* Action types that can be fired from
   clicks in the popup window. */
typedef enum {
	ACTION_MINIMAP_CLICK = 0,
	ACTION_MINIMAP_MODE,
	ACTION_MINIMAP_ROADS,
	ACTION_MINIMAP_BUILDINGS,
	ACTION_MINIMAP_GRID,
	ACTION_BUILD_STONEMINE,
	ACTION_BUILD_COALMINE,
	ACTION_BUILD_IRONMINE,
	ACTION_BUILD_GOLDMINE,
	ACTION_BUILD_FLAG,
	ACTION_BUILD_STONECUTTER,
	ACTION_BUILD_HUT,
	ACTION_BUILD_LUMBERJACK,
	ACTION_BUILD_FORESTER,
	ACTION_BUILD_FISHER,
	ACTION_BUILD_MILL,
	ACTION_BUILD_BOATBUILDER,
	ACTION_BUILD_BUTCHER,
	ACTION_BUILD_WEAPONSMITH,
	ACTION_BUILD_STEELSMELTER,
	ACTION_BUILD_SAWMILL,
	ACTION_BUILD_BAKER,
	ACTION_BUILD_GOLDSMELTER,
	ACTION_BUILD_FORTRESS,
	ACTION_BUILD_TOWER,
	ACTION_BUILD_TOOLMAKER,
	ACTION_BUILD_FARM,
	ACTION_BUILD_PIGFARM,
	ACTION_BLD_FLIP_PAGE,
	ACTION_SHOW_STAT_1,
	ACTION_SHOW_STAT_2,
	ACTION_SHOW_STAT_8,
	ACTION_SHOW_STAT_BLD,
	ACTION_SHOW_STAT_6,
	ACTION_SHOW_STAT_7,
	ACTION_SHOW_STAT_4,
	ACTION_SHOW_STAT_3,
	ACTION_SHOW_STAT_SELECT,
	ACTION_STAT_BLD_FLIP,
	ACTION_CLOSE_BOX,
	ACTION_SETT_8_SET_ASPECT_ALL,
	ACTION_SETT_8_SET_ASPECT_LAND,
	ACTION_SETT_8_SET_ASPECT_BUILDINGS,
	ACTION_SETT_8_SET_ASPECT_MILITARY,
	ACTION_SETT_8_SET_SCALE_30_MIN,
	ACTION_SETT_8_SET_SCALE_60_MIN,
	ACTION_SETT_8_SET_SCALE_600_MIN,
	ACTION_SETT_8_SET_SCALE_3000_MIN,
	ACTION_STAT_7_SELECT_FISH,
	ACTION_STAT_7_SELECT_PIG,
	ACTION_STAT_7_SELECT_MEAT,
	ACTION_STAT_7_SELECT_WHEAT,
	ACTION_STAT_7_SELECT_FLOUR,
	ACTION_STAT_7_SELECT_BREAD,
	ACTION_STAT_7_SELECT_LUMBER,
	ACTION_STAT_7_SELECT_PLANK,
	ACTION_STAT_7_SELECT_BOAT,
	ACTION_STAT_7_SELECT_STONE,
	ACTION_STAT_7_SELECT_IRONORE,
	ACTION_STAT_7_SELECT_STEEL,
	ACTION_STAT_7_SELECT_COAL,
	ACTION_STAT_7_SELECT_GOLDORE,
	ACTION_STAT_7_SELECT_GOLDBAR,
	ACTION_STAT_7_SELECT_SHOVEL,
	ACTION_STAT_7_SELECT_HAMMER,
	ACTION_STAT_7_SELECT_ROD,
	ACTION_STAT_7_SELECT_CLEAVER,
	ACTION_STAT_7_SELECT_SCYTHE,
	ACTION_STAT_7_SELECT_AXE,
	ACTION_STAT_7_SELECT_SAW,
	ACTION_STAT_7_SELECT_PICK,
	ACTION_STAT_7_SELECT_PINCER,
	ACTION_STAT_7_SELECT_SWORD,
	ACTION_STAT_7_SELECT_SHIELD,
	ACTION_ATTACKING_KNIGHTS_DEC,
	ACTION_ATTACKING_KNIGHTS_INC,
	ACTION_START_ATTACK,
	ACTION_CLOSE_ATTACK_BOX,
	/* ... 78 - 91 ... */
	ACTION_CLOSE_SETT_BOX = 92,
	ACTION_SHOW_SETT_1,
	ACTION_SHOW_SETT_2,
	ACTION_SHOW_SETT_3,
	ACTION_SHOW_SETT_7,
	ACTION_SHOW_SETT_4,
	ACTION_SHOW_SETT_5,
	ACTION_SHOW_SETT_SELECT,
	ACTION_SETT_1_ADJUST_STONEMINE,
	ACTION_SETT_1_ADJUST_COALMINE,
	ACTION_SETT_1_ADJUST_IRONMINE,
	ACTION_SETT_1_ADJUST_GOLDMINE,
	ACTION_SETT_2_ADJUST_CONSTRUCTION,
	ACTION_SETT_2_ADJUST_BOATBUILDER,
	ACTION_SETT_2_ADJUST_TOOLMAKER_PLANKS,
	ACTION_SETT_2_ADJUST_TOOLMAKER_STEEL,
	ACTION_SETT_2_ADJUST_WEAPONSMITH,
	ACTION_SETT_3_ADJUST_STEELSMELTER,
	ACTION_SETT_3_ADJUST_GOLDSMELTER,
	ACTION_SETT_3_ADJUST_WEAPONSMITH,
	ACTION_SETT_3_ADJUST_PIGFARM,
	ACTION_SETT_3_ADJUST_MILL,
	ACTION_KNIGHT_LEVEL_CLOSEST_MIN_DEC,
	ACTION_KNIGHT_LEVEL_CLOSEST_MIN_INC,
	ACTION_KNIGHT_LEVEL_CLOSEST_MAX_DEC,
	ACTION_KNIGHT_LEVEL_CLOSEST_MAX_INC,
	ACTION_KNIGHT_LEVEL_CLOSE_MIN_DEC,
	ACTION_KNIGHT_LEVEL_CLOSE_MIN_INC,
	ACTION_KNIGHT_LEVEL_CLOSE_MAX_DEC,
	ACTION_KNIGHT_LEVEL_CLOSE_MAX_INC,
	ACTION_KNIGHT_LEVEL_FAR_MIN_DEC,
	ACTION_KNIGHT_LEVEL_FAR_MIN_INC,
	ACTION_KNIGHT_LEVEL_FAR_MAX_DEC,
	ACTION_KNIGHT_LEVEL_FAR_MAX_INC,
	ACTION_KNIGHT_LEVEL_FARTHEST_MIN_DEC,
	ACTION_KNIGHT_LEVEL_FARTHEST_MIN_INC,
	ACTION_KNIGHT_LEVEL_FARTHEST_MAX_DEC,
	ACTION_KNIGHT_LEVEL_FARTHEST_MAX_INC,
	ACTION_SETT_4_ADJUST_SHOVEL,
	ACTION_SETT_4_ADJUST_HAMMER,
	ACTION_SETT_4_ADJUST_AXE,
	ACTION_SETT_4_ADJUST_SAW,
	ACTION_SETT_4_ADJUST_SCYTHE,
	ACTION_SETT_4_ADJUST_PICK,
	ACTION_SETT_4_ADJUST_PINCER,
	ACTION_SETT_4_ADJUST_CLEAVER,
	ACTION_SETT_4_ADJUST_ROD,
	ACTION_SETT_5_6_ITEM_1,
	ACTION_SETT_5_6_ITEM_2,
	ACTION_SETT_5_6_ITEM_3,
	ACTION_SETT_5_6_ITEM_4,
	ACTION_SETT_5_6_ITEM_5,
	ACTION_SETT_5_6_ITEM_6,
	ACTION_SETT_5_6_ITEM_7,
	ACTION_SETT_5_6_ITEM_8,
	ACTION_SETT_5_6_ITEM_9,
	ACTION_SETT_5_6_ITEM_10,
	ACTION_SETT_5_6_ITEM_11,
	ACTION_SETT_5_6_ITEM_12,
	ACTION_SETT_5_6_ITEM_13,
	ACTION_SETT_5_6_ITEM_14,
	ACTION_SETT_5_6_ITEM_15,
	ACTION_SETT_5_6_ITEM_16,
	ACTION_SETT_5_6_ITEM_17,
	ACTION_SETT_5_6_ITEM_18,
	ACTION_SETT_5_6_ITEM_19,
	ACTION_SETT_5_6_ITEM_20,
	ACTION_SETT_5_6_ITEM_21,
	ACTION_SETT_5_6_ITEM_22,
	ACTION_SETT_5_6_ITEM_23,
	ACTION_SETT_5_6_ITEM_24,
	ACTION_SETT_5_6_ITEM_25,
	ACTION_SETT_5_6_ITEM_26,
	ACTION_SETT_5_6_TOP,
	ACTION_SETT_5_6_UP,
	ACTION_SETT_5_6_DOWN,
	ACTION_SETT_5_6_BOTTOM,
	ACTION_QUIT_CONFIRM,
	ACTION_QUIT_CANCEL,
	ACTION_NO_SAVE_QUIT_CONFIRM,
	ACTION_SHOW_QUIT,
	ACTION_SHOW_OPTIONS,
	ACTION_SHOW_SAVE,
	ACTION_SETT_8_CYCLE,
	ACTION_CLOSE_OPTIONS,
	ACTION_OPTIONS_PATHWAY_SCROLLING_1,
	ACTION_OPTIONS_PATHWAY_SCROLLING_2,
	ACTION_OPTIONS_FAST_MAP_CLICK_1,
	ACTION_OPTIONS_FAST_MAP_CLICK_2,
	ACTION_OPTIONS_FAST_BUILDING_1,
	ACTION_OPTIONS_FAST_BUILDING_2,
	ACTION_OPTIONS_MESSAGE_COUNT_1,
	ACTION_OPTIONS_MESSAGE_COUNT_2,
	ACTION_SHOW_SETT_SELECT_FILE,
	ACTION_SHOW_STAT_SELECT_FILE,
	ACTION_DEFAULT_SETT_1,
	ACTION_DEFAULT_SETT_2,
	ACTION_DEFAULT_SETT_5_6,
	ACTION_BUILD_STOCK,
	ACTION_SHOW_CASTLE_SERF,
	ACTION_SHOW_RESDIR,
	ACTION_SHOW_CASTLE_RES,
	ACTION_SEND_GEOLOGIST,
	ACTION_RES_MODE_IN,
	ACTION_RES_MODE_STOP,
	ACTION_RES_MODE_OUT,
	ACTION_SERF_MODE_IN,
	ACTION_SERF_MODE_STOP,
	ACTION_SERF_MODE_OUT,
	ACTION_SHOW_SETT_8,
	ACTION_SHOW_SETT_6,
	ACTION_SETT_8_ADJUST_RATE,
	ACTION_SETT_8_TRAIN_1,
	ACTION_SETT_8_TRAIN_5,
	ACTION_SETT_8_TRAIN_20,
	ACTION_SETT_8_TRAIN_100,
	ACTION_DEFAULT_SETT_3,
	ACTION_SETT_8_SET_COMBAT_MODE_WEAK,
	ACTION_SETT_8_SET_COMBAT_MODE_STRONG,
	ACTION_ATTACKING_SELECT_ALL_1,
	ACTION_ATTACKING_SELECT_ALL_2,
	ACTION_ATTACKING_SELECT_ALL_3,
	ACTION_ATTACKING_SELECT_ALL_4,
	ACTION_MINIMAP_BLD_1,
	ACTION_MINIMAP_BLD_2,
	ACTION_MINIMAP_BLD_3,
	ACTION_MINIMAP_BLD_4,
	ACTION_MINIMAP_BLD_5,
	ACTION_MINIMAP_BLD_6,
	ACTION_MINIMAP_BLD_7,
	ACTION_MINIMAP_BLD_8,
	ACTION_MINIMAP_BLD_9,
	ACTION_MINIMAP_BLD_10,
	ACTION_MINIMAP_BLD_11,
	ACTION_MINIMAP_BLD_12,
	ACTION_MINIMAP_BLD_13,
	ACTION_MINIMAP_BLD_14,
	ACTION_MINIMAP_BLD_15,
	ACTION_MINIMAP_BLD_16,
	ACTION_MINIMAP_BLD_17,
	ACTION_MINIMAP_BLD_18,
	ACTION_MINIMAP_BLD_19,
	ACTION_MINIMAP_BLD_20,
	ACTION_MINIMAP_BLD_21,
	ACTION_MINIMAP_BLD_22,
	ACTION_MINIMAP_BLD_23,
	ACTION_MINIMAP_BLD_FLAG,
	ACTION_MINIMAP_BLD_NEXT,
	ACTION_MINIMAP_BLD_EXIT,
	ACTION_CLOSE_MESSAGE,
	ACTION_DEFAULT_SETT_4,
	ACTION_SHOW_PLAYER_FACES,
	ACTION_MINIMAP_SCALE,
	ACTION_OPTIONS_RIGHT_SIDE,
	ACTION_CLOSE_GROUND_ANALYSIS,
	ACTION_UNKNOWN_TP_INFO_FLAG,
	ACTION_SETT_8_CASTLE_DEF_DEC,
	ACTION_SETT_8_CASTLE_DEF_INC,
	ACTION_OPTIONS_MUSIC,
	ACTION_OPTIONS_SVGA,
	ACTION_OPTIONS_VOLUME_MINUS,
	ACTION_OPTIONS_VOLUME_PLUS,
	ACTION_DEMOLISH
} action_t;


/* Draw the frame around the popup box. */
static void
draw_popup_box_frame(frame_t *frame)
{
	gfx_draw_sprite(0, 0, DATA_FRAME_POPUP_BASE+0, frame);
	gfx_draw_sprite(0, 153, DATA_FRAME_POPUP_BASE+1, frame);
	gfx_draw_sprite(0, 9, DATA_FRAME_POPUP_BASE+2, frame);
	gfx_draw_sprite(136, 9, DATA_FRAME_POPUP_BASE+3, frame);
}

/* Draw icon in a popup frame. */
static void
draw_popup_icon(int x, int y, int sprite, frame_t *frame)
{
	gfx_draw_sprite(8*x+8, y+9, DATA_ICON_BASE + sprite, frame);
}

/* Draw building in a popup frame. */
static void
draw_popup_building(int x, int y, int sprite, frame_t *frame)
{
	gfx_draw_transp_sprite(8*x+8, y+9, DATA_MAP_OBJECT_BASE + sprite, frame);
}

/* Fill the background of a popup frame. */
static void
draw_box_background(int sprite, frame_t *frame)
{
	for (int y = 0; y < 144; y += 16) {
		for (int x = 0; x < 16; x += 2) {
			draw_popup_icon(x, y, sprite, frame);
		}
	}
}

/* Fill one row of a popup frame. */
static void
draw_box_row(int sprite, int y, frame_t *frame)
{
	for (int x = 0; x < 16; x += 2) draw_popup_icon(x, y, sprite, frame);
}

/* Draw a green string in a popup frame. */
static void
draw_green_string(int x, int y, frame_t *frame, const char *str)
{
	gfx_draw_string(8*x+8, y+9, 31, 0, frame, str);
}

/* Draw a green number in a popup frame.
   n must be non-negative. If > 999 simply draw ">999" (three characters). */
static void
draw_green_number(int x, int y, frame_t *frame, int n)
{
	if (n >= 1000) {
		draw_popup_icon(x, y, 0xd5, frame); /* Draw >999 */
		draw_popup_icon(x+1, y, 0xd6, frame);
		draw_popup_icon(x+2, y, 0xd7, frame);
	} else {
		/* Not the same sprites as are used to draw numbers
		   in gfx_draw_number(). */
		int draw_zero = 0;
		if (n >= 100) {
			int n100 = floor(n / 100);
			n -= n100 * 100;
			draw_popup_icon(x, y, 0x4e + n100, frame);
			x += 1;
			draw_zero = 1;
		}

		if (n >= 10 || draw_zero) {
			int n10 = floor(n / 10);
			n -= n10 * 10;
			draw_popup_icon(x, y, 0x4e + n10, frame);
			x += 1;
			draw_zero = 1;
		}

		draw_popup_icon(x, y, 0x4e + n, frame);
	}
}

/* Draw a green number in a popup frame.
   No limits on n. */
static void
draw_green_large_number(int x, int y, frame_t *frame, int n)
{
	gfx_draw_number(8*x+8, 9+y, 31, 0, frame, n);
}

/* Draw small green number. */
static void
draw_additional_number(int x, int y, frame_t *frame, int n)
{
	if (n > 0) draw_popup_icon(x, y, 240 + min(n, 10), frame);
}

/* Get the sprite number for a face. */
static int
get_player_face_sprite(int face)
{
	if (face != 0) return 0x10b + face;
	return 0x119; /* sprite_face_none */
}

/* Draw player face in popup frame. */
static void
draw_player_face(int x, int y, int player, frame_t *frame)
{
	const int player_colors[] = {
		64, 72, 68, 76
	};

	gfx_fill_rect(8*x, y+5, 48, 72, player_colors[player], frame);
	draw_popup_icon(x, y, get_player_face_sprite(globals.pl_init[player].face), frame);
}

/* Draw a layout of buildings in a popup box. */
static void
draw_custom_bld_box(const int sprites[], frame_t *frame)
{
	while (sprites[0] > 0) {
		int x = sprites[1];
		int y = sprites[2];
		gfx_draw_transp_sprite(8*x+8, y+9, DATA_MAP_OBJECT_BASE + sprites[0], frame);
		sprites += 3;
	}
}

/* Draw a layout of icons in a popup box. */
static void
draw_custom_icon_box(const int sprites[], frame_t *frame)
{
	while (sprites[0] > 0) {
		draw_popup_icon(sprites[1], sprites[2], sprites[0], frame);
		sprites += 3;
	}
}

/* Translate resource amount to text. */
static const char *
prepare_res_amount_text(int amount)
{
	if (amount == 0) return "Not Present";
	else if (amount < 100) return "Minimum";
	else if (amount < 180) return "Very Few";
	else if (amount < 240) return "Few";
	else if (amount < 300) return "Below Average";
	else if (amount < 400) return "Average";
	else if (amount < 500) return "Above Average";
	else if (amount < 600) return "Much";
	else if (amount < 800) return "Very Much";
	return "Perfect";
}

/* Draw map popup box. */
static void
draw_minimap_icons(player_t *player, frame_t *frame)
{
	draw_popup_icon(0, 128, player->minimap_flags & 3, frame); /* Mode */
	draw_popup_icon(4, 128, BIT_TEST(player->minimap_flags, 2) ? 3 : 4, frame); /* Roads */
	if (player->minimap_advanced >= 0) {
		draw_popup_icon(8, 128, player->minimap_advanced == 0 ? 306 : 305, frame); /* Unknown mode */
	} else {
		draw_popup_icon(8, 128, BIT_TEST(player->minimap_flags, 3) ? 5 : 6, frame); /* Buildings */
	}
	draw_popup_icon(12, 128, BIT_TEST(player->minimap_flags, 4) ? 7 : 8, frame); /* Grid */
	draw_popup_icon(14, 128, BIT_TEST(player->minimap_flags, 5) ? 91 : 92, frame); /* Scale */
}

static void
draw_map_overlay_box(popup_box_t *popup, frame_t *frame)
{
	/* Draw minimap */
	frame_t minimap_frame;
	sdl_frame_init(&minimap_frame,
		       frame->clip.x + 8,
		       frame->clip.y + 9,
		       128, 128, frame);

	gui_object_redraw((gui_object_t *)&popup->minimap, &minimap_frame);
}

static void
draw_map_box(popup_box_t *popup, frame_t *frame)
{
	popup->player->clkmap = BOX_MAP_OVERLAY;
	draw_minimap_icons(popup->player, frame);
	draw_map_overlay_box(popup, frame);
}

/* Draw building mine popup box. */
static void
draw_mine_building_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		0xa3, 2, 8,
		0xa4, 8, 8,
		0xa5, 4, 77,
		0xa6, 10, 77,
		-1
	};

	draw_box_background(0x83, frame);

	if (!BIT_TEST(popup->player->sett->build, 1)) { /* Can build flag */
		draw_popup_building(2, 114, 0x80+4*popup->player->sett->player_num, frame);
	}

	draw_custom_bld_box(layout, frame);
}

/* Draw .. popup box... */
static void
draw_basic_building_box(popup_box_t *popup, frame_t *frame, int flip)
{
	const int layout[] = {
		0xab, 10, 13, /* hut */
		0xa9, 2, 13,
		0xa8, 0, 58,
		0xaa, 6, 56,
		0xa7, 12, 55,
		0xbc, 2, 85,
		0xae, 10, 87,
		-1
	};

	draw_box_background(0x83, frame);

	const int *l = layout;
	if (BIT_TEST(popup->player->sett->build, 0)) { /* Can not build military building */
		l += 3; /* Skip hut */
	}

	draw_custom_bld_box(l, frame);

	if (!BIT_TEST(popup->player->sett->build, 1)) { /* Can build flag */
		draw_popup_building(8, 108, 0x80+4*popup->player->sett->player_num, frame);
	}

	if (flip) draw_popup_icon(0, 128, 0x3d, frame);
}

static void
draw_adv_1_building_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		0x9c, 0, 15,
		0x9d, 8, 15,
		0xa1, 0, 50,
		0xa0, 8, 50,
		0xa2, 2, 100,
		0x9f, 10, 96,
		-1
	};

	draw_box_background(0x83, frame);
	draw_custom_bld_box(layout, frame);
	draw_popup_icon(0, 128, 0x3d, frame);
}

static void
draw_adv_2_building_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		0x9e, 2, 99, /* tower */
		0x98, 8, 84, /* fortress */
		0x99, 0, 1,
		0xc0, 0, 46,
		0x9a, 8, 1,
		0x9b, 8, 45,
		-1
	};

	const int *l = layout;
	if (BIT_TEST(popup->player->sett->build, 0)) { /* Can not build military building */
		l += 2*3; /* Skip tower and fortress */
	}

	draw_box_background(0x83, frame);
	draw_custom_bld_box(l, frame);
	draw_popup_icon(0, 128, 0x3d, frame);
}

/* Draw generic popup box of resources. */
static void
draw_resources_box(frame_t *frame, const int resources[])
{
	const int layout[] = {
		0x28, 1, 0, /* resources */
		0x29, 1, 16,
		0x2a, 1, 32,
		0x2b, 1, 48,
		0x2e, 1, 64,
		0x2c, 1, 80,
		0x2d, 1, 96,
		0x2f, 1, 112,
		0x30, 1, 128,
		0x31, 6, 0,
		0x32, 6, 16,
		0x36, 6, 32,
		0x37, 6, 48,
		0x35, 6, 64,
		0x38, 6, 80,
		0x39, 6, 96,
		0x34, 6, 112,
		0x33, 6, 128,
		0x3a, 11, 0,
		0x3b, 11, 16,
		0x22, 11, 32,
		0x23, 11, 48,
		0x24, 11, 64,
		0x25, 11, 80,
		0x26, 11, 96,
		0x27, 11, 112,
		-1
	};

	draw_custom_icon_box(layout, frame);

	/* First column */
	draw_green_number(3, 4, frame, resources[RESOURCE_LUMBER]);
	draw_green_number(3, 20, frame, resources[RESOURCE_PLANK]);
	draw_green_number(3, 36, frame, resources[RESOURCE_BOAT]);
	draw_green_number(3, 52, frame, resources[RESOURCE_STONE]);
	draw_green_number(3, 68, frame, resources[RESOURCE_COAL]);
	draw_green_number(3, 84, frame, resources[RESOURCE_IRONORE]);
	draw_green_number(3, 100, frame, resources[RESOURCE_STEEL]);
	draw_green_number(3, 116, frame, resources[RESOURCE_GOLDORE]);
	draw_green_number(3, 132, frame, resources[RESOURCE_GOLDBAR]);

	/* Second column */
	draw_green_number(8, 4, frame, resources[RESOURCE_SHOVEL]);
	draw_green_number(8, 20, frame, resources[RESOURCE_HAMMER]);
	draw_green_number(8, 36, frame, resources[RESOURCE_AXE]);
	draw_green_number(8, 52, frame, resources[RESOURCE_SAW]);
	draw_green_number(8, 68, frame, resources[RESOURCE_SCYTHE]);
	draw_green_number(8, 84, frame, resources[RESOURCE_PICK]);
	draw_green_number(8, 100, frame, resources[RESOURCE_PINCER]);
	draw_green_number(8, 116, frame, resources[RESOURCE_CLEAVER]);
	draw_green_number(8, 132, frame, resources[RESOURCE_ROD]);

	/* Third column */
	draw_green_number(13, 4, frame, resources[RESOURCE_SWORD]);
	draw_green_number(13, 20, frame, resources[RESOURCE_SHIELD]);
	draw_green_number(13, 36, frame, resources[RESOURCE_FISH]);
	draw_green_number(13, 52, frame, resources[RESOURCE_PIG]);
	draw_green_number(13, 68, frame, resources[RESOURCE_MEAT]);
	draw_green_number(13, 84, frame, resources[RESOURCE_WHEAT]);
	draw_green_number(13, 100, frame, resources[RESOURCE_FLOUR]);
	draw_green_number(13, 116, frame, resources[RESOURCE_BREAD]);
}

/* Draw generic popup box of serfs. */
static void
draw_serfs_box(frame_t *frame, const int serfs[], int total)
{
	const int layout[] = {
		0x9, 1, 0, /* serfs */
		0xa, 1, 16,
		0xb, 1, 32,
		0xc, 1, 48,
		0x21, 1, 64,
		0x20, 1, 80,
		0x1f, 1, 96,
		0x1e, 1, 112,
		0x1d, 1, 128,
		0xd, 6, 0,
		0xe, 6, 16,
		0x12, 6, 32,
		0xf, 6, 48,
		0x10, 6, 64,
		0x11, 6, 80,
		0x19, 6, 96,
		0x1a, 6, 112,
		0x1b, 6, 128,
		0x13, 11, 0,
		0x14, 11, 16,
		0x15, 11, 32,
		0x16, 11, 48,
		0x17, 11, 64,
		0x18, 11, 80,
		0x1c, 11, 96,
		0x82, 11, 112,
		-1
	};

	draw_custom_icon_box(layout, frame);

	/* First column */
	draw_green_number(3, 4, frame, serfs[SERF_TRANSPORTER]);
	draw_green_number(3, 20, frame, serfs[SERF_SAILOR]);
	draw_green_number(3, 36, frame, serfs[SERF_DIGGER]);
	draw_green_number(3, 52, frame, serfs[SERF_BUILDER]);
	draw_green_number(3, 68, frame, serfs[SERF_KNIGHT_4]);
	draw_green_number(3, 84, frame, serfs[SERF_KNIGHT_3]);
	draw_green_number(3, 100, frame, serfs[SERF_KNIGHT_2]);
	draw_green_number(3, 116, frame, serfs[SERF_KNIGHT_1]);
	draw_green_number(3, 132, frame, serfs[SERF_KNIGHT_0]);

	/* Second column */
	draw_green_number(8, 4, frame, serfs[SERF_LUMBERJACK]);
	draw_green_number(8, 20, frame, serfs[SERF_SAWMILLER]);
	draw_green_number(8, 36, frame, serfs[SERF_SMELTER]);
	draw_green_number(8, 52, frame, serfs[SERF_STONECUTTER]);
	draw_green_number(8, 68, frame, serfs[SERF_FORESTER]);
	draw_green_number(8, 84, frame, serfs[SERF_MINER]);
	draw_green_number(8, 100, frame, serfs[SERF_BOATBUILDER]);
	draw_green_number(8, 116, frame, serfs[SERF_TOOLMAKER]);
	draw_green_number(8, 132, frame, serfs[SERF_WEAPONSMITH]);

	/* Third column */
	draw_green_number(13, 4, frame, serfs[SERF_FISHER]);
	draw_green_number(13, 20, frame, serfs[SERF_PIGFARMER]);
	draw_green_number(13, 36, frame, serfs[SERF_BUTCHER]);
	draw_green_number(13, 52, frame, serfs[SERF_FARMER]);
	draw_green_number(13, 68, frame, serfs[SERF_MILLER]);
	draw_green_number(13, 84, frame, serfs[SERF_BAKER]);
	draw_green_number(13, 100, frame, serfs[SERF_GEOLOGIST]);
	draw_green_number(13, 116, frame, serfs[SERF_GENERIC]);

	if (total >= 0) {
		draw_green_large_number(11, 132, frame, total);
	}
}

static void
draw_stat_select_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		72, 1, 12,
		73, 6, 12,
		77, 11, 12,
		74, 1, 56,
		76, 6, 56,
		75, 11, 56,
		71, 1, 100,
		70, 6, 100,
		61, 12, 104, /* Flip */
		60, 14, 128, /* Exit */
		-1
	};

	draw_box_background(129, frame);
	draw_custom_icon_box(layout, frame);
}

static void
draw_stat_4_box(popup_box_t *popup, frame_t *frame)
{
	draw_box_background(129, frame);

	int resources[26];
	memset(resources, '\0', 26*sizeof(int));

	/* Sum up resources of all inventories. */
	for (int i = 0; i < globals.max_ever_inventory_index; i++) {
		if (BIT_TEST(globals.inventories_bitmap[i>>3], 7-(i&7))) {
			inventory_t *inventory = game_get_inventory(i);
			if (inventory->player_num == popup->player->sett->player_num) {
				for (int j = 0; j < 26; j++) {
					resources[j] += inventory->resources[j];
				}
			}
		}
	}

	/* Add extra resources. */
	resources[RESOURCE_PLANK] += popup->player->sett->extra_planks;
	resources[RESOURCE_STONE] += popup->player->sett->extra_stone;

	draw_resources_box(frame, resources);

	draw_popup_icon(14, 128, 60, frame); /* exit */
}

static void
draw_stat_bld_1_box(popup_box_t *popup, frame_t *frame)
{
	const int bld_layout[] = {
		192, 0, 5,
		171, 2, 77,
		158, 8, 7,
		152, 6, 69,
		-1
	};

	draw_box_background(129, frame);

	draw_custom_bld_box(bld_layout, frame);

	draw_green_number(2, 105, frame, popup->player->sett->completed_building_count[BUILDING_HUT]);
	draw_additional_number(3, 105, frame, popup->player->sett->incomplete_building_count[BUILDING_HUT]);

	draw_green_number(10, 53, frame, popup->player->sett->completed_building_count[BUILDING_TOWER]);
	draw_additional_number(11, 53, frame, popup->player->sett->incomplete_building_count[BUILDING_TOWER]);

	draw_green_number(9, 130, frame, popup->player->sett->completed_building_count[BUILDING_FORTRESS]);
	draw_additional_number(10, 130, frame, popup->player->sett->incomplete_building_count[BUILDING_FORTRESS]);

	draw_green_number(4, 61, frame, popup->player->sett->completed_building_count[BUILDING_STOCK]);
	draw_additional_number(5, 61, frame, popup->player->sett->incomplete_building_count[BUILDING_STOCK]);

	draw_popup_icon(0, 128, 61, frame); /* flip */
	draw_popup_icon(14, 128, 60, frame); /* exit */
}

static void
draw_stat_bld_2_box(popup_box_t *popup, frame_t *frame)
{
	const int bld_layout[] = {
		153, 0, 4,
		160, 8, 6,
		157, 0, 68,
		169, 8, 65,
		174, 12, 57,
		170, 4, 105,
		168, 8, 107,
		-1
	};

	draw_box_background(129, frame);

	draw_custom_bld_box(bld_layout, frame);

	draw_green_number(3, 54, frame, popup->player->sett->completed_building_count[BUILDING_TOOLMAKER]);
	draw_additional_number(4, 54, frame, popup->player->sett->incomplete_building_count[BUILDING_TOOLMAKER]);

	draw_green_number(10, 48, frame, popup->player->sett->completed_building_count[BUILDING_SAWMILL]);
	draw_additional_number(11, 48, frame, popup->player->sett->incomplete_building_count[BUILDING_SAWMILL]);

	draw_green_number(3, 95, frame, popup->player->sett->completed_building_count[BUILDING_WEAPONSMITH]);
	draw_additional_number(4, 95, frame, popup->player->sett->incomplete_building_count[BUILDING_WEAPONSMITH]);

	draw_green_number(8, 95, frame, popup->player->sett->completed_building_count[BUILDING_STONECUTTER]);
	draw_additional_number(9, 95, frame, popup->player->sett->incomplete_building_count[BUILDING_STONECUTTER]);

	draw_green_number(12, 95, frame, popup->player->sett->completed_building_count[BUILDING_BOATBUILDER]);
	draw_additional_number(13, 95, frame, popup->player->sett->incomplete_building_count[BUILDING_BOATBUILDER]);

	draw_green_number(5, 132, frame, popup->player->sett->completed_building_count[BUILDING_FORESTER]);
	draw_additional_number(6, 132, frame, popup->player->sett->incomplete_building_count[BUILDING_FORESTER]);

	draw_green_number(9, 132, frame, popup->player->sett->completed_building_count[BUILDING_LUMBERJACK]);
	draw_additional_number(10, 132, frame, popup->player->sett->incomplete_building_count[BUILDING_LUMBERJACK]);

	draw_popup_icon(0, 128, 61, frame); /* flip */
	draw_popup_icon(14, 128, 60, frame); /* exit */
}

static void
draw_stat_bld_3_box(popup_box_t *popup, frame_t *frame)
{
	const int bld_layout[] = {
		155, 0, 2,
		154, 8, 3,
		167, 0, 61,
		156, 8, 60,
		188, 4, 75,
		162, 8, 100,
		-1
	};

	draw_box_background(129, frame);

	draw_custom_bld_box(bld_layout, frame);

	draw_green_number(3, 48, frame, popup->player->sett->completed_building_count[BUILDING_PIGFARM]);
	draw_additional_number(4, 48, frame, popup->player->sett->incomplete_building_count[BUILDING_PIGFARM]);

	draw_green_number(11, 48, frame, popup->player->sett->completed_building_count[BUILDING_FARM]);
	draw_additional_number(12, 48, frame, popup->player->sett->incomplete_building_count[BUILDING_FARM]);

	draw_green_number(0, 92, frame, popup->player->sett->completed_building_count[BUILDING_FISHER]);
	draw_additional_number(1, 92, frame, popup->player->sett->incomplete_building_count[BUILDING_FISHER]);

	draw_green_number(11, 87, frame, popup->player->sett->completed_building_count[BUILDING_BUTCHER]);
	draw_additional_number(12, 87, frame, popup->player->sett->incomplete_building_count[BUILDING_BUTCHER]);

	draw_green_number(5, 134, frame, popup->player->sett->completed_building_count[BUILDING_MILL]);
	draw_additional_number(6, 134, frame, popup->player->sett->incomplete_building_count[BUILDING_MILL]);

	draw_green_number(10, 134, frame, popup->player->sett->completed_building_count[BUILDING_BAKER]);
	draw_additional_number(11, 134, frame, popup->player->sett->incomplete_building_count[BUILDING_BAKER]);

	draw_popup_icon(0, 128, 61, frame); /* flip */
	draw_popup_icon(14, 128, 60, frame); /* exit */
}

static void
draw_stat_bld_4_box(popup_box_t *popup, frame_t *frame)
{
	const int bld_layout[] = {
		163, 0, 4,
		164, 4, 4,
		165, 8, 4,
		166, 12, 4,
		161, 2, 90,
		159, 8, 90,
		-1
	};

	draw_box_background(129, frame);

	draw_custom_bld_box(bld_layout, frame);

	draw_green_number(0, 71, frame, popup->player->sett->completed_building_count[BUILDING_STONEMINE]);
	draw_additional_number(1, 71, frame, popup->player->sett->incomplete_building_count[BUILDING_STONEMINE]);

	draw_green_number(4, 71, frame, popup->player->sett->completed_building_count[BUILDING_COALMINE]);
	draw_additional_number(5, 71, frame, popup->player->sett->incomplete_building_count[BUILDING_COALMINE]);

	draw_green_number(8, 71, frame, popup->player->sett->completed_building_count[BUILDING_IRONMINE]);
	draw_additional_number(9, 71, frame, popup->player->sett->incomplete_building_count[BUILDING_IRONMINE]);

	draw_green_number(12, 71, frame, popup->player->sett->completed_building_count[BUILDING_GOLDMINE]);
	draw_additional_number(13, 71, frame, popup->player->sett->incomplete_building_count[BUILDING_GOLDMINE]);

	draw_green_number(4, 130, frame, popup->player->sett->completed_building_count[BUILDING_STEELSMELTER]);
	draw_additional_number(5, 130, frame, popup->player->sett->incomplete_building_count[BUILDING_STEELSMELTER]);

	draw_green_number(9, 130, frame, popup->player->sett->completed_building_count[BUILDING_GOLDSMELTER]);
	draw_additional_number(10, 130, frame, popup->player->sett->incomplete_building_count[BUILDING_GOLDSMELTER]);

	draw_popup_icon(0, 128, 61, frame); /* flip */
	draw_popup_icon(14, 128, 60, frame); /* exit */
}

static void
draw_player_stat_chart(const int *data, int index, int color, frame_t *frame)
{
	int prev_value = data[index];
	index = index > 0 ? index-1 : 111;

	for (int i = 0; i < 112; i++) {
		int value = data[index];
		index = index > 0 ? index-1 : 111;

		/* TODO There are glitches in drawing these charts. */

		if (value > prev_value) {
			int diff = value - prev_value;
			int h = diff/2;
			gfx_fill_rect(119 - i, 108 - h - prev_value, 1, h, color, frame);
			diff -= h;
			gfx_fill_rect(118 - i, 108 - prev_value, 1, diff, color, frame);
		} else {
			int diff = prev_value - value;
			int h = diff/2;
			gfx_fill_rect(119 - i, 108 - prev_value, 1, h, color, frame);
			diff -= h;
			gfx_fill_rect(118 - i, 108 - diff - prev_value, 1, diff, color, frame);
		}

		prev_value = value;
	}
}

static void
draw_stat_8_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		0x58, 14, 0,
		0x59, 0, 100,
		0x41, 8, 112,
		0x42, 10, 112,
		0x43, 8, 128,
		0x44, 10, 128,
		0x45, 2, 112,
		0x40, 4, 112,
		0x3e, 2, 128,
		0x3f, 4, 128,
		0x133, 14, 112,

		0x3c, 14, 128, /* exit */
		-1
	};

	int mode = popup->player->current_stat_8_mode;
	int aspect = (mode >> 2) & 3;
	int scale = mode & 3;

	/* Draw background */
	draw_box_row(132+aspect, 0, frame);
	draw_box_row(132+aspect, 16, frame);
	draw_box_row(132+aspect, 32, frame);
	draw_box_row(132+aspect, 48, frame);
	draw_box_row(132+aspect, 64, frame);
	draw_box_row(132+aspect, 80, frame);
	draw_box_row(132+aspect, 96, frame);

	draw_box_row(136, 108, frame);
	draw_box_row(129, 116, frame);
	draw_box_row(137, 132, frame);

	draw_custom_icon_box(layout, frame);

	/* Draw checkmarks to indicate current settings. */
	draw_popup_icon(!BIT_TEST(aspect, 0) ? 1 : 6,
			!BIT_TEST(aspect, 1) ? 116 : 132,
			106, frame); /* checkmark */

	draw_popup_icon(!BIT_TEST(scale, 0) ? 7 : 12,
			!BIT_TEST(scale, 1) ? 116 : 132,
			106, frame); /* checkmark */

	/* Correct numbers on time scale. */
	draw_popup_icon(2, 103, 94 + 3*scale + 0, frame);
	draw_popup_icon(6, 103, 94 + 3*scale + 1, frame);
	draw_popup_icon(10, 103, 94 + 3*scale + 2, frame);

	/* Draw chart */
	int index = globals.player_history_index[scale];
	draw_player_stat_chart(globals.player_sett[3]->player_stat_history[mode], index, 76, frame);
	draw_player_stat_chart(globals.player_sett[2]->player_stat_history[mode], index, 68, frame);
	draw_player_stat_chart(globals.player_sett[1]->player_stat_history[mode], index, 72, frame);
	draw_player_stat_chart(globals.player_sett[0]->player_stat_history[mode], index, 64, frame);
}

static void
draw_stat_7_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		0x81, 6, 80,
		0x81, 8, 80,
		0x81, 6, 96,
		0x81, 8, 96,

		0x59, 0, 64,
		0x5a, 14, 0,

		0x28, 0, 75, /* lumber */
		0x29, 2, 75, /* plank */
		0x2b, 4, 75, /* stone */
		0x2e, 0, 91, /* coal */
		0x2c, 2, 91, /* ironore */
		0x2f, 4, 91, /* goldore */
		0x2a, 0, 107, /* boat */
		0x2d, 2, 107, /* iron */
		0x30, 4, 107, /* goldbar */
		0x3a, 7, 83, /* sword */
		0x3b, 7, 99, /* shield */
		0x31, 10, 75, /* shovel */
		0x32, 12, 75, /* hammer */
		0x36, 14, 75, /* axe */
		0x37, 10, 91, /* saw */
		0x38, 12, 91, /* pick */
		0x35, 14, 91, /* scythe */
		0x34, 10, 107, /* cleaver */
		0x39, 12, 107, /* pincer */
		0x33, 14, 107, /* rod */
		0x22, 1, 125, /* fish */
		0x23, 3, 125, /* pig */
		0x24, 5, 125, /* meat */
		0x25, 7, 125, /* wheat */
		0x26, 9, 125, /* flour */
		0x27, 11, 125, /* bread */

		0x3c, 14, 128, /* exitbox */
		-1
	};

	draw_box_row(129, 64, frame);
	draw_box_row(129, 112, frame);
	draw_box_row(129, 128, frame);

	draw_custom_icon_box(layout, frame);

	int item = popup->player->current_stat_7_item-1;

	/* Draw background of chart */
	for (int y = 0; y < 64; y += 16) {
		for (int x = 0; x < 14; x += 2) {
			draw_popup_icon(x, y, 138 + item, frame);
		}
	}

	const int sample_weights[] = { 4, 6, 8, 9, 10, 9, 8, 6, 4 };

	/* Create array of historical counts */
	int historical_data[112];
	int max_val = 0;
	int index = globals.resource_history_index;

	for (int i = 0; i < 112; i++) {
		historical_data[i] = 0;
		int j = index;
		for (int k = 0; k < 9; k++) {
			historical_data[i] += sample_weights[k]*popup->player->sett->resource_count_history[item][j];
			j = j > 0 ? j-1 : 119;
		}

		if (historical_data[i] > max_val) {
			max_val = historical_data[i];
		}

		index = index > 0 ? index-1 : 119;
	}

	const int axis_icons_1[] = { 110, 109, 108, 107 };
	const int axis_icons_2[] = { 112, 111, 110, 108 };
	const int axis_icons_3[] = { 114, 113, 112, 110 };
	const int axis_icons_4[] = { 117, 116, 114, 112 };
	const int axis_icons_5[] = { 120, 119, 118, 115 };
	const int axis_icons_6[] = { 122, 121, 120, 118 };
	const int axis_icons_7[] = { 125, 124, 122, 120 };
	const int axis_icons_8[] = { 128, 127, 126, 123 };

	const int *axis_icons = NULL;
	int multiplier = 0;

	/* TODO chart background pattern */

	if (max_val <= 64) {
		axis_icons = axis_icons_1;
		multiplier = 0x8000;
	} else if (max_val <= 128) {
		axis_icons = axis_icons_2;
		multiplier = 0x4000;
	} else if (max_val <= 256) {
		axis_icons = axis_icons_3;
		multiplier = 0x2000;
	} else if (max_val <= 512) {
		axis_icons = axis_icons_4;
		multiplier = 0x1000;
	} else if (max_val <= 1280) {
		axis_icons = axis_icons_5;
		multiplier = 0x666;
	} else if (max_val <= 2560) {
		axis_icons = axis_icons_6;
		multiplier = 0x333;
	} else if (max_val <= 5120) {
		axis_icons = axis_icons_7;
		multiplier = 0x199;
	} else {
		axis_icons = axis_icons_8;
		multiplier = 0xa3;
	}

	/* Draw axis icons */
	for (int i = 0; i < 4; i++) {
		draw_popup_icon(14, i*16, axis_icons[i], frame);
	}

	/* Draw chart */
	for (int i = 0; i < 112; i++) {
		int value = min((historical_data[i]*multiplier) >> 16, 64);
		if (value > 0) {
			gfx_fill_rect(119 - i, 73 - value, 1, value, 72, frame);
		}
	}
}

static void
draw_stat_1_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		0x18, 0, 0, /* baker */
		0xb4, 0, 16,
		0xb3, 0, 24,
		0xb2, 0, 32,
		0xb3, 0, 40,
		0xb2, 0, 48,
		0xb3, 0, 56,
		0xb2, 0, 64,
		0xb3, 0, 72,
		0xb2, 0, 80,
		0xb3, 0, 88,
		0xd4, 0, 96,
		0xb1, 0, 112,
		0x13, 0, 120, /* fisher */
		0x15, 2, 48, /* butcher */
		0xb4, 2, 64,
		0xb3, 2, 72,
		0xd4, 2, 80,
		0xa4, 2, 96,
		0xa4, 2, 112,
		0xae, 4, 4,
		0xae, 4, 36,
		0xa6, 4, 80,
		0xa6, 4, 96,
		0xa6, 4, 112,
		0x26, 6, 0, /* flour */
		0x23, 6, 32, /* pig */
		0xb5, 6, 64,
		0x24, 6, 76, /* meat */
		0x27, 6, 92, /* bread */
		0x22, 6, 108, /* fish */
		0xb6, 6, 124,
		0x17, 8, 0, /* miller */
		0x14, 8, 32, /* pigfarmer */
		0xa6, 8, 64,
		0xab, 8, 88,
		0xab, 8, 104,
		0xa6, 8, 128,
		0xba, 12, 8,
		0x11, 12, 56, /* miner */
		0x11, 12, 80, /* miner */
		0x11, 12, 104, /* miner */
		0x11, 12, 128, /* miner */
		0x16, 14, 0, /* farmer */
		0x25, 14, 16, /* wheat */
		0x2f, 14, 56, /* goldore */
		0x2e, 14, 80, /* coal */
		0x2c, 14, 104, /* ironore */
		0x2b, 14, 128, /* stone */
		-1
	};

	draw_box_background(129, frame);

	draw_custom_icon_box(layout, frame);

	/* TODO */
}

static void
draw_stat_2_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		0x11, 0, 0, /* miner */
		0x11, 0, 24, /* miner */
		0x11, 0, 56, /* miner */
		0xd, 0, 80, /* lumberjack */
		0x11, 0, 104, /* miner */
		0xf, 0, 128, /* stonecutter */
		0x2f, 2, 0, /* goldore */
		0x2e, 2, 24, /* coal */
		0xb0, 2, 40,
		0x2c, 2, 56, /* ironore */
		0x28, 2, 80, /* lumber */
		0x2b, 2, 104, /* stone */
		0x2b, 2, 128, /* stone */
		0xaa, 4, 4,
		0xab, 4, 24,
		0xad, 4, 32,
		0xa8, 4, 40,
		0xac, 4, 60,
		0xaa, 4, 84,
		0xbb, 4, 108,
		0xa4, 6, 32,
		0xe, 6, 96, /* sawmiller */
		0xa5, 6, 132,
		0x30, 8, 0, /* gold */
		0x12, 8, 16, /* smelter */
		0xa4, 8, 32,
		0x2d, 8, 40, /* steel */
		0x12, 8, 56, /* smelter */
		0xb8, 8, 80,
		0x29, 8, 96, /* planks */
		0xaf, 8, 112,
		0xa5, 8, 132,
		0xaa, 10, 4,
		0xb9, 10, 24,
		0xab, 10, 40,
		0xb7, 10, 48,
		0xa6, 10, 80,
		0xa9, 10, 96,
		0xa6, 10, 112,
		0xa7, 10, 132,
		0x21, 14, 0, /* knight 4 */
		0x1b, 14, 28, /* weaponsmith */
		0x1a, 14, 64, /* toolmaker */
		0x19, 14, 92, /* boatbuilder */
		0xc, 14, 120, /* builder */
		-1
	};

	draw_box_background(129, frame);

	draw_custom_icon_box(layout, frame);

	/* TODO */
}

static void
draw_stat_6_box(popup_box_t *popup, frame_t *frame)
{
	draw_box_background(129, frame);

	int total = 0;
	for (int i = 0; i < 27; i++) {
		if (i != SERF_4) total += popup->player->sett->serf_count[i];
	}

	draw_serfs_box(frame, popup->player->sett->serf_count, total);

	draw_popup_icon(14, 128, 60, frame); /* exit */
}

static void
draw_stat_3_box(popup_box_t *popup, frame_t *frame)
{
	draw_box_background(129, frame);

	int serfs[27];
	memset(serfs, '\0', 27*sizeof(int));

	/* Sum up all existing serfs. */
	for (int i = 1; i < globals.max_ever_serf_index; i++) {
		if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
			serf_t *serf = game_get_serf(i);
			if (SERF_PLAYER(serf) == popup->player->sett->player_num &&
			    serf->state == SERF_STATE_IDLE_IN_STOCK) {
				serfs[SERF_TYPE(serf)] += 1;
			}
		}
	}

	/* Sum up potential serfs of all inventories. */
	for (int i = 0; i < globals.max_ever_inventory_index; i++) {
		if (BIT_TEST(globals.inventories_bitmap[i>>3], 7-(i&7))) {
			inventory_t *inventory = game_get_inventory(i);
			if (inventory->player_num == popup->player->sett->player_num) {
				/* TODO */
			}
		}
	}

	/* TODO draw meters */
	draw_serfs_box(frame, serfs, -1);

	draw_popup_icon(14, 128, 60, frame); /* exit */
}

static void
draw_start_attack_redraw_box(popup_box_t *popup, frame_t *frame)
{
	/* TODO Should overwrite the previously drawn number.
	   Just use fill_rect(), perhaps? */
	draw_green_string(6, 116, frame, "    ");
	draw_green_number(7, 116, frame, popup->player->sett->knights_attacking);
}

static void
draw_start_attack_box(popup_box_t *popup, frame_t *frame)
{
	const int building_layout[] = {
		0x0, 2, 33,
		0xa, 6, 30,
		0x7, 10, 33,
		0xc, 14, 30,
		0xe, 2, 36,
		0x2, 6, 39,
		0xb, 10, 36,
		0x4, 12, 39,
		0x8, 8, 42,
		0xf, 12, 42,
		-1
	};

	const int icon_layout[] = {
		216, 1, 80,
		217, 5, 80,
		218, 9, 80,
		219, 13, 80,
		220, 4, 112,
		221, 10, 112,
		222, 0, 128,
		60, 14, 128,
		-1
	};

	draw_box_background(131, frame);

	for (int i = 0; building_layout[i] >= 0; i += 3) {
		draw_popup_building(building_layout[i+1], building_layout[i+2],
				    building_layout[i], frame);
	}

	building_t *building = game_get_building(popup->player->sett->building_attacked);
	int y = 0;

	switch (BUILDING_TYPE(building)) {
	case BUILDING_HUT: y = 50; break;
	case BUILDING_TOWER: y = 32; break;
	case BUILDING_FORTRESS: y = 17; break;
	case BUILDING_CASTLE: y = 0; break;
	default: NOT_REACHED(); break;
	}

	draw_popup_building(0, y, map_building_sprite[BUILDING_TYPE(building)],
			    frame);
	draw_custom_icon_box(icon_layout, frame);

	/* Draw number of knight at each distance. */
	for (int i = 0; i < 4; i++) {
		draw_green_number(1+4*i, 96, frame,
				  popup->player->sett->attacking_knights[i]);
	}

	draw_start_attack_redraw_box(popup, frame);
}

static void
draw_ground_analysis_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		0x1c, 7, 10,
		0x2f, 1, 50,
		0x2c, 1, 70,
		0x2e, 1, 90,
		0x2b, 1, 110,
		0x3c, 14, 128,
		-1
	};

	draw_box_background(0x81, frame);
	draw_custom_icon_box(layout, frame);
	game_prepare_ground_analysis(popup->player);
	draw_green_string(0, 30, frame, "GROUND-ANALYSIS:");

	/* Gold */
	const char *s = prepare_res_amount_text(2*popup->player->sett->analysis_goldore);
	draw_green_string(3, 54, frame, s);

	/* Iron */
	s = prepare_res_amount_text(popup->player->sett->analysis_ironore);
	draw_green_string(3, 74, frame, s);

	/* Coal */
	s = prepare_res_amount_text(popup->player->sett->analysis_coal);
	draw_green_string(3, 94, frame, s);

	/* Stone */
	s = prepare_res_amount_text(2*popup->player->sett->analysis_stone);
	draw_green_string(3, 114, frame, s);
}

static void
draw_sett_select_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		230, 1, 8,
		231, 6, 8,
		232, 11, 8,
		234, 1, 48,
		235, 6, 48,
		299, 11, 48,
		233, 1, 88,
		298, 6, 88,
		61, 12, 104,
		60, 14, 128,

		285, 4, 128,
		286, 0, 128,
		224, 8, 128,
		-1
	};

	draw_box_background(311, frame);
	draw_custom_icon_box(layout, frame);
}

/* Draw slide bar in a popup box. */
static void
draw_slide_bar(int x, int y, int value, frame_t *frame)
{
	draw_popup_icon(x, y, 236, frame);

	int width = value/1310;
	if (width > 0) {
		gfx_fill_rect(8*x+15, y+11, width, 4, 30, frame);
	}
}

static void
draw_sett_1_box(popup_box_t *popup, frame_t *frame)
{
	const int bld_layout[] = {
		163, 12, 21,
		164, 8, 41,
		165, 4, 61,
		166, 0, 81,
		-1
	};

	const int layout[] = {
		34, 4, 1,
		36, 7, 1,
		39, 10, 1,
		60, 14, 128,
		295, 1, 8,
		-1
	};

	draw_box_background(311, frame);
	draw_custom_bld_box(bld_layout, frame);
	draw_custom_icon_box(layout, frame);

	draw_slide_bar(4, 21, popup->player->sett->food_stonemine, frame);
	draw_slide_bar(0, 41, popup->player->sett->food_coalmine, frame);
	draw_slide_bar(8, 114, popup->player->sett->food_ironmine, frame);
	draw_slide_bar(4, 133, popup->player->sett->food_goldmine, frame);
}

static void
draw_sett_2_box(popup_box_t *popup, frame_t *frame)
{
	const int bld_layout[] = {
		186, 2, 0,
		174, 2, 41,
		153, 8, 54,
		157, 0, 102,
		-1
	};

	const int layout[] = {
		41, 9, 25,
		45, 9, 119,
		60, 14, 128,
		295, 13, 8,
		-1
	};

	draw_box_background(311, frame);
	draw_custom_bld_box(bld_layout, frame);
	draw_custom_icon_box(layout, frame);

	draw_slide_bar(0, 26, popup->player->sett->planks_construction, frame);
	draw_slide_bar(0, 36, popup->player->sett->planks_boatbuilder, frame);
	draw_slide_bar(8, 44, popup->player->sett->planks_toolmaker, frame);
	draw_slide_bar(8, 103, popup->player->sett->steel_toolmaker, frame);
	draw_slide_bar(0, 130, popup->player->sett->steel_weaponsmith, frame);
}

static void
draw_sett_3_box(popup_box_t *popup, frame_t *frame)
{
	const int bld_layout[] = {
		161, 0, 1,
		159, 10, 0,
		157, 4, 56,
		188, 12, 61,
		155, 0, 101,
		-1
	};

	const int layout[] = {
		46, 7, 19,
		37, 8, 101,
		60, 14, 128,
		295, 1, 60,
		-1
	};

	draw_box_background(311, frame);
	draw_custom_bld_box(bld_layout, frame);
	draw_custom_icon_box(layout, frame);

	draw_slide_bar(0, 39, popup->player->sett->coal_steelsmelter, frame);
	draw_slide_bar(8, 39, popup->player->sett->coal_goldsmelter, frame);
	draw_slide_bar(4, 47, popup->player->sett->coal_weaponsmith, frame);
	draw_slide_bar(0, 92, popup->player->sett->wheat_pigfarm, frame);
	draw_slide_bar(8, 118, popup->player->sett->wheat_mill, frame);
}

static void
draw_knight_level_box(popup_box_t *popup, frame_t *frame)
{
	const char *level_str[] = {
		"Minimum", "Weak", "Medium", "Good", "Full"
	};

	const int layout[] = {
		226, 0, 2,
		227, 0, 36,
		228, 0, 70,
		229, 0, 104,

		220, 4, 2,  /* minus */
		221, 6, 2,  /* plus */
		220, 4, 18, /* ... */
		221, 6, 18,
		220, 4, 36,
		221, 6, 36,
		220, 4, 52,
		221, 6, 52,
		220, 4, 70,
		221, 6, 70,
		220, 4, 86,
		221, 6, 86,
		220, 4, 104,
		221, 6, 104,
		220, 4, 120,
		221, 6, 120,

		60, 14, 128, /* exit */
		-1
	};

	draw_box_background(311, frame);

	player_sett_t *sett = popup->player->sett;
	draw_green_string(8, 8, frame,
			  level_str[sett->knight_occupation[3] & 0xf]);
	draw_green_string(8, 19, frame,
			  level_str[(sett->knight_occupation[3] >> 4) & 0xf]);
	draw_green_string(8, 42, frame,
			  level_str[sett->knight_occupation[2] & 0xf]);
	draw_green_string(8, 53, frame,
			  level_str[(sett->knight_occupation[2] >> 4) & 0xf]);
	draw_green_string(8, 76, frame,
			  level_str[sett->knight_occupation[1] & 0xf]);
	draw_green_string(8, 87, frame,
			  level_str[(sett->knight_occupation[1] >> 4) & 0xf]);
	draw_green_string(8, 110, frame,
			  level_str[sett->knight_occupation[0] & 0xf]);
	draw_green_string(8, 121, frame,
			  level_str[(sett->knight_occupation[0] >> 4) & 0xf]);

	draw_custom_icon_box(layout, frame);
}

static void
draw_sett_4_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		49, 1, 0, /* shovel */
		50, 1, 16, /* hammer */
		54, 1, 32, /* axe */
		55, 1, 48, /* saw */
		53, 1, 64, /* scythe */
		56, 1, 80, /* pick */
		57, 1, 96, /* pincer */
		52, 1, 112, /* cleaver */
		51, 1, 128, /* rod */

		60, 14, 128, /* exit*/
		295, 13, 8, /* default */
		-1
	};

	draw_box_background(311, frame);
	draw_custom_icon_box(layout, frame);

	draw_slide_bar(4, 4, popup->player->sett->tool_prio[0], frame); /* shovel */
	draw_slide_bar(4, 20, popup->player->sett->tool_prio[1], frame); /* hammer */
	draw_slide_bar(4, 36, popup->player->sett->tool_prio[5], frame); /* axe */
	draw_slide_bar(4, 52, popup->player->sett->tool_prio[6], frame); /* saw */
	draw_slide_bar(4, 68, popup->player->sett->tool_prio[4], frame); /* scythe */
	draw_slide_bar(4, 84, popup->player->sett->tool_prio[7], frame); /* pick */
	draw_slide_bar(4, 100, popup->player->sett->tool_prio[8], frame); /* pincer */
	draw_slide_bar(4, 116, popup->player->sett->tool_prio[3], frame); /* cleaver */
	draw_slide_bar(4, 132, popup->player->sett->tool_prio[2], frame); /* rod */
}

/* Draw generic popup box of resource stairs. */
static void
draw_popup_resource_stairs(int order[], frame_t *frame)
{
	const int spiral_layout[] = {
		5, 4,
		7, 6,
		9, 8,
		11, 10,
		13, 12,
		13, 28,
		11, 30,
		9, 32,
		7, 34,
		5, 36,
		3, 38,
		1, 40,
		1, 56,
		3, 58,
		5, 60,
		7, 62,
		9, 64,
		11, 66,
		13, 68,
		13, 84,
		11, 86,
		9, 88,
		7, 90,
		5, 92,
		3, 94,
		1, 96
	};

	for (int i = 0; i < 26; i++) {
		int pos = 26 - order[i];
		draw_popup_icon(spiral_layout[2*pos],
				spiral_layout[2*pos+1],
				34+i, frame);
	}
}

static void
draw_sett_5_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		237, 1, 120, /* up */
		238, 3, 120, /* smallup */
		239, 9, 120, /* smalldown */
		240, 11, 120, /* down */
		295, 1, 4, /* default*/
		60, 14, 128, /* exit */
		-1
	};

	draw_box_background(311, frame);
	draw_custom_icon_box(layout, frame);
	draw_popup_resource_stairs(popup->player->sett->flag_prio, frame);

	draw_popup_icon(6, 120, 33+popup->player->sett->current_sett_5_item, frame);
}

static void
draw_quit_confirm_box(popup_box_t *popup, frame_t *frame)
{
	game_pause(1);

	draw_box_background(310, frame);

	draw_green_string(0, 10, frame, "   Do you want");
	draw_green_string(0, 20, frame, "     to quit");
	draw_green_string(0, 30, frame, "   this game?");
	draw_green_string(0, 45, frame, "  Yes       No");

	/* wait_x_timer_ticks(8); */

	globals.svga &= ~BIT(5);
}

static void
draw_no_save_quit_confirm_box(popup_box_t *popup, frame_t *frame)
{
	draw_green_string(0, 70, frame, "The game has not");
	draw_green_string(0, 80, frame, "   been saved");
	draw_green_string(0, 90, frame, "   recently.");
	draw_green_string(0, 100, frame, "    Are you");
	draw_green_string(0, 110, frame, "     sure?");
	draw_green_string(0, 125, frame, "  Yes       No");
}

static void
draw_options_box(popup_box_t *popup, frame_t *frame)
{
	draw_box_background(310, frame);

	draw_popup_icon(0, 0, 311, frame);
	draw_popup_icon(2, 0, 311, frame);
	draw_popup_icon(4, 0, 311, frame);
	draw_popup_icon(10, 0, 311, frame);
	draw_popup_icon(12, 0, 311, frame);
	draw_popup_icon(14, 0, 311, frame);

	draw_popup_icon(0, 8, 311, frame);
	draw_popup_icon(2, 8, 311, frame);
	draw_popup_icon(4, 8, 311, frame);
	draw_popup_icon(10, 8, 311, frame);
	draw_popup_icon(12, 8, 311, frame);
	draw_popup_icon(14, 8, 311, frame);

	draw_green_string(0, 2, frame, "Left       Right");
	draw_green_string(0, 11, frame, "side        side");

	draw_green_string(2, 28, frame, "  Pathway-");
	draw_green_string(2, 37, frame, " scrolling");
	draw_green_string(2, 48, frame, "    Fast");
	draw_green_string(2, 57, frame, "  map click");
	draw_green_string(2, 68, frame, "    Fast");
	draw_green_string(2, 77, frame, "  building");

	char *messages = strdup("    Messages    ");
	messages[0] = '3';
	if (!BIT_TEST(globals.cfg_left,3)) {
		messages[0] = '2';
		if (!BIT_TEST(globals.cfg_left,4)) {
			messages[0] = '1';
			if (!BIT_TEST(globals.cfg_left,5)) {
				messages[0] = '0';
			}
		}
	}
	messages[15] = '3';
	if (!BIT_TEST(globals.cfg_right,3)) {
		messages[15] = '2';
		if (!BIT_TEST(globals.cfg_right,4)) {
			messages[15] = '1';
			if (!BIT_TEST(globals.cfg_right,5)) {
				messages[15] = '0';
			}
		}
	}
	draw_green_string(0, 88, frame, messages);
	free(messages);

	draw_popup_icon(7, 0, 60, frame); /* exit */

	draw_popup_icon(0, 28, BIT_TEST(globals.cfg_left, 0) ? 288 : 220, frame);
	draw_popup_icon(0, 48, BIT_TEST(globals.cfg_left, 1) ? 288 : 220, frame);
	draw_popup_icon(0, 68, BIT_TEST(globals.cfg_left, 2) ? 288 : 220, frame);

	draw_popup_icon(14, 28, BIT_TEST(globals.cfg_right, 0) ? 288 : 220, frame);
	draw_popup_icon(14, 48, BIT_TEST(globals.cfg_right, 1) ? 288 : 220, frame);
	draw_popup_icon(14, 68, BIT_TEST(globals.cfg_right, 2) ? 288 : 220, frame);

	draw_green_string(2, 110, frame, "Music");
	draw_green_string(7, 105, frame, "  SVGA"); /* TODO replace with fullscreen? */
	draw_green_string(7, 114, frame, "  mode"); /* TODO replace with fullscreen? */
	draw_green_string(0, 125, frame, ""); /* What is it? Look on Amiga?. */
	draw_green_string(0, 134, frame, " Volume:");

	draw_popup_icon(0, 106, midi_is_enabled() ? 288 : 220, frame); /* Music */
	draw_popup_icon(14, 106, 288, frame); /* SVGA */ /* TODO replace with fullscreen? */
	draw_popup_icon(12, 126, 220, frame); /* Volude minus */
	draw_popup_icon(14, 126, 221, frame); /* Volude plus */

	char volume[4] = {0};
	sprintf(volume, "%d", audio_volume());
	draw_green_string(9, 134, frame, volume);
}

static void
draw_castle_res_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		0x3d, 12, 128, /* flip */
		0x3c, 14, 128, /* exit */
		-1
	};

	draw_box_background(0x138, frame);
	draw_custom_icon_box(layout, frame);

	if (popup->player->sett->index == 0) return;/*player_close_popup();*/

	building_t *building = game_get_building(popup->player->sett->index);
	if (BIT_TEST(building->serf, 5)) return;/*player_close_popup();*/ /* Building is burning */

	inventory_t *inventory = building->u.inventory;
	if (BUILDING_TYPE(building) == BUILDING_STOCK) {
		/* TODO supply zeroed array to draw_resources_box */
	} else if (BUILDING_TYPE(building) == BUILDING_CASTLE) {
		/* TODO add extra planks and extra stone */
	} else {
		return;/*player_close_popup();*/
	}

	draw_resources_box(frame, inventory->resources);
}

static void
draw_mine_output_box(popup_box_t *popup, frame_t *frame)
{
	draw_box_background(0x138, frame);

	if (popup->player->sett->index == 0) return;/*player_close_popup();*/

	building_t *building = game_get_building(popup->player->sett->index);
	/* if (BIT_TEST(building->serf, 5)) player_close_popup();*/ /* Building is burning */
	building_type_t type = BUILDING_TYPE(building);

	if (type != BUILDING_STONEMINE &&
	    type != BUILDING_COALMINE &&
	    type != BUILDING_IRONMINE &&
	    type != BUILDING_GOLDMINE) {
		return;/*player_close_popup();*/
	}

	/* Draw building */
	draw_popup_building(6, 60, map_building_sprite[type], frame);

	/* Draw serf icon */
	int sprite = 0xdc; /* minus box */
	if (BIT_TEST(building->serf, 6)) sprite = 0x11; /* miner */

	draw_popup_icon(10, 75, sprite, frame);

	/* Draw food present at mine */
	int stock = (building->stock1 >> 4) & 0xf;
	int stock_left_col = (stock + 1) >> 1;
	int stock_right_col = stock >> 1;

	/* Left column */
	for (int i = 0; i < stock_left_col; i++) {
		draw_popup_icon(1, 90 - 8*stock_left_col + i*16, 0x24, frame); /* meat (food) sprite */
	}

	/* Right column */
	for (int i = 0; i < stock_right_col; i++) {
		draw_popup_icon(13, 90 - 8*stock_right_col + i*16, 0x24, frame); /* meat (food) sprite */
	}

	/* Calculate output percentage (simple WMA) */
	const int output_weight[] = { 10, 10, 9, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 3, 2, 1 };
	int output = 0;
	for (int i = 0; i < 15; i++) {
		output += !!BIT_TEST(building->progress, i) * output_weight[i];
	}

	/* Print output precentage */
	int x = 7;
	if (output >= 100) x += 1;
	if (output >= 10) x += 1;
	draw_green_string(x, 38, frame, "%");
	draw_green_number(6, 38, frame, output);

	draw_green_string(1, 14, frame, "MINING");
	draw_green_string(1, 24, frame, "OUTPUT:");

	/* Exit box */
	draw_popup_icon(14, 128, 0x3c, frame);
}

static void
draw_ordered_building_box(popup_box_t *popup, frame_t *frame)
{
	draw_box_background(0x138, frame);

	if (popup->player->sett->index == 0) return;/*player_close_popup();*/

	building_t *building = game_get_building(popup->player->sett->index);
	/* if (BIT_TEST(building->serf, 5)) player_close_popup();*/ /* Building is burning */
	building_type_t type = BUILDING_TYPE(building);

	int sprite = map_building_sprite[type];
	int x = 6;
	if (sprite == 0xc0 /*stock*/ || sprite >= 0x9e /*tower*/) x = 4;
	draw_popup_building(x, 40, sprite, frame);

	draw_green_string(2, 4, frame, "Ordered");
	draw_green_string(2, 14, frame, "Building");

	if (BIT_TEST(building->serf, 6)) { /* Serf present */
		if (building->progress == 0) draw_popup_icon(2, 100, 0xb, frame); /* Digger */
		else draw_popup_icon(2, 100, 0xc, frame); /* Builder */
	} else {
		draw_popup_icon(2, 100, 0xdc, frame); /* Minus box */
	}

	draw_popup_icon(14, 128, 0x3c, frame); /* Exit box */
}

static void
draw_defenders_box(popup_box_t *popup, frame_t *frame)
{
	draw_box_background(0x138, frame);

	if (popup->player->sett->index == 0) return;/*player_close_popup();*/

	building_t *building = game_get_building(popup->player->sett->index);
	if (BUILDING_IS_BURNING(building)) return;/*player_close_popup();*/ /* Building is burning */

	if (!BIT_TEST(globals.split, 5) && /* Demo mode */
	    BUILDING_PLAYER(building) != popup->player->sett->player_num) {
		return;/*player_close_popup();*/
	}

	if (BUILDING_TYPE(building) != BUILDING_HUT &&
	    BUILDING_TYPE(building) != BUILDING_TOWER &&
	    BUILDING_TYPE(building) != BUILDING_FORTRESS) {
		return;/*player_close_popup();*/
	}

	/* Draw building sprite */
	int sprite = map_building_sprite[BUILDING_TYPE(building)];
	int x = 0, y = 0;
	switch (BUILDING_TYPE(building)) {
	case BUILDING_HUT: x = 6; y = 20; break;
	case BUILDING_TOWER: x = 4; y = 6; break;
	case BUILDING_FORTRESS: x = 4; y = 1; break;
	default: NOT_REACHED(); break;
	}

	draw_popup_building(x, y, sprite, frame);

	/* Draw gold stock */
	if (building->stock2 & 0xf0) {
		int amount = (building->stock2 >> 4) & 0xf;

		int left = (amount + 1) / 2;
		for (int i = 0; i < left; i++) {
			draw_popup_icon(1, 32 - 8*left + 16*i, 0x30, frame);
		}

		int right = amount / 2;
		for (int i = 0; i < right; i++) {
			draw_popup_icon(13, 32 - 8*right + 16*i, 0x30, frame);
		}
	}

	/* Draw heading string */
	draw_green_string(3, 62, frame, "DEFENDERS:");

	/* Draw knights */
	int next_knight = building->serf_index;
	for (int i = 0; next_knight != 0; i++) {
		serf_t *serf = game_get_serf(next_knight);
		draw_popup_icon(3 + 4*(i%4), 72 + 14*(i/4), 7 + SERF_TYPE(serf), frame);
		next_knight = serf->s.defending.next_knight;
	}

	draw_green_string(0, 128, frame, "STATE:");
	draw_green_number(7, 128, frame, building->serf & 3);

	draw_popup_icon(14, 128, 0x3c, frame); /* Exit box */
}

static void
draw_transport_info_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		9, 24,
		5, 24,
		3, 44,
		5, 64,
		9, 64,
		11, 44
	};

	draw_box_background(0x138, frame);
	if (!BIT_TEST(popup->player->sett->flags, 7)) { /* Not AI (???) */
		/* TODO int r = ... */
		/* if (r == 0) draw_popup_icon(7, 51, 0x135, frame); */
	}

	if (popup->player->sett->index == 0) return;/*player_close_popup();*/

	flag_t *flag = game_get_flag(popup->player->sett->index);

#if 1
	/* Draw viewport of flag */
	frame_t flag_frame;
	sdl_frame_init(&flag_frame,
		       frame->clip.x + 8,
		       frame->clip.y + 24,
		       128, 64, frame);

	viewport_t flag_view;
	viewport_init(&flag_view, popup->player);
	flag_view.layers = VIEWPORT_LAYER_PATHS | VIEWPORT_LAYER_OBJECTS;
	gui_object_set_displayed((gui_object_t *)&flag_view, 1);

	flag_view.obj.parent = (gui_container_t *)popup;
	gui_object_set_size((gui_object_t *)&flag_view, 128, 64);
	viewport_move_to_map_pos(&flag_view, flag->pos);
	flag_view.offset_y -= 10;

	gui_object_redraw((gui_object_t *)&flag_view, &flag_frame);
#else
	/* Static flag */
	draw_popup_building(8, 40, 0x80 + 4*popup->player->sett->player_num, frame);
#endif

	for (int i = 0; i < 6; i++) {
		int x = layout[2*i];
		int y = layout[2*i+1];
		if (BIT_TEST(flag->path_con, 5-i)) {
			int sprite = 0xdc; /* Minus box */
			if (BIT_TEST(flag->transporter, 5-i)) sprite = 0x120; /* Check box */
			draw_popup_icon(x, y, sprite, frame);
		}
	}

	draw_green_string(0, 4, frame, "Transport Info:");
	draw_popup_icon(2, 96, 0x1c, frame); /* Geologist */
	draw_popup_icon(14, 128, 0x3c, frame); /* Exit box */

	/* Draw list of resources */
	for (int i = 0; i < 8; i++) {
		if (flag->res_waiting[i] != 0) {
			draw_popup_icon(7 + 2*(i&3), 88 + 16*(i>>2), 0x22 + (flag->res_waiting[i] & 0x1f)-1, frame);
		}
	}

	draw_green_string(0, 128, frame, "Index:");
	draw_green_number(7, 128, frame, FLAG_INDEX(flag));
}

static void
draw_castle_serf_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		0x3d, 12, 128, /* flip */
		0x3c, 14, 128, /* exit */
		-1
	};

	int serfs[27];
	memset(serfs, 0, 27*sizeof(int));

	draw_box_background(0x138, frame);
	draw_custom_icon_box(layout, frame);

	if (popup->player->sett->index == 0) return;/*player_close_popup();*/

	building_t *building = game_get_building(popup->player->sett->index);
	if (BIT_TEST(building->serf, 5)) return;/*player_close_popup();*/ /* Building is burning */

	building_type_t type = BUILDING_TYPE(building);
	if (type != BUILDING_STOCK && type != BUILDING_CASTLE) return;/*player_close_popup();*/

	inventory_t *inventory = building->u.inventory;

	for (int i = 1; i < globals.max_ever_serf_index; i++) {
		if (BIT_TEST(globals.serfs_bitmap[i/8], 7-(i&7))) {
			serf_t *serf = game_get_serf(i);
			if (serf->state == SERF_STATE_IDLE_IN_STOCK &&
			    inventory == game_get_inventory(serf->s.idle_in_stock.inv_index)) {
				serfs[SERF_TYPE(serf)] += 1;
			}
		}
	}

	draw_serfs_box(frame, serfs, -1);
}

static void
draw_resdir_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		0x128, 4, 16,
		0x129, 4, 80,
		0xdc, 9, 16,
		0xdc, 9, 32,
		0xdc, 9, 48,
		0xdc, 9, 80,
		0xdc, 9, 96,
		0xdc, 9, 112,
		0x3d, 12, 128,
		0x3c, 14, 128,
		-1
	};

	const int knights_layout[] = {
		0x21, 12, 16,
		0x20, 12, 36,
		0x1f, 12, 56,
		0x1e, 12, 76,
		0x1d, 12, 96,
		-1
	};

	draw_box_background(0x138, frame);
	draw_custom_icon_box(layout, frame);

	if (popup->player->sett->index == 0) return;/*player_close_popup();*/

	building_t *building = game_get_building(popup->player->sett->index);
	if (BIT_TEST(building->serf, 5)) return;/*player_close_popup();*/ /* Building is burning */

	building_type_t type = BUILDING_TYPE(building);
	if (type == BUILDING_CASTLE) {
		int knights[] = { 0, 0, 0, 0, 0 };
		draw_custom_icon_box(knights_layout, frame);

		/* Follow linked list of knights on duty */
		for (int index = building->serf_index; index != 0; index = game_get_serf(index)->s.defending.next_knight) {
			serf_t *serf = game_get_serf(index);
			serf_type_t serf_type = SERF_TYPE(serf);
			if (serf_type >= SERF_KNIGHT_0 && serf_type <= SERF_KNIGHT_4) {
				knights[serf_type-SERF_KNIGHT_0] += 1;
			} else {
				break;
			}
		}

		draw_green_number(14, 20, frame, knights[4]);
		draw_green_number(14, 40, frame, knights[3]);
		draw_green_number(14, 60, frame, knights[2]);
		draw_green_number(14, 80, frame, knights[1]);
		draw_green_number(14, 100, frame, knights[0]);
	} else if (type != BUILDING_STOCK) {
		return;/*player_close_popup();*/
	}

	/* Draw resource mode checkbox */
	inventory_t *inventory = building->u.inventory;
	int res_mode = inventory->res_dir & 3;
	if (res_mode == 0) draw_popup_icon(9, 16, 0x120, frame); /* in */
	else if (res_mode == 1) draw_popup_icon(9, 32, 0x120, frame); /* stop */
	else draw_popup_icon(9, 48, 0x120, frame); /* out */

	/* Draw serf mode checkbox */
	int serf_mode = (inventory->res_dir >> 2) & 3;
	if (serf_mode == 0) draw_popup_icon(9, 80, 0x120, frame); /* in */
	else if (serf_mode == 1) draw_popup_icon(9, 96, 0x120, frame); /* stop */
	else draw_popup_icon(9, 112, 0x120, frame); /* out */
}

static void
draw_sett_8_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		9, 2, 8,
		29, 12, 8,
		300, 2, 28,
		59, 7, 44,
		130, 8, 28,
		58, 9, 44,
		304, 3, 64,
		303, 11, 64,
		302, 2, 84,
		220, 6, 84,
		220, 6, 100,
		301, 10, 84,
		220, 3, 120,
		221, 9, 120,
		60, 14, 128,
		-1
	};

	draw_box_background(311, frame);
	draw_custom_icon_box(layout, frame);

	player_sett_t *sett = popup->player->sett;

	draw_slide_bar(4, 12, sett->serf_to_knight_rate, frame);
	draw_green_number(6, 63, frame, (100*sett->knight_morale)/0x1000);

	draw_green_large_number(6, 73, frame, sett->gold_deposited);

	draw_green_number(6, 119, frame, sett->castle_knights_wanted);
	draw_green_number(6, 129, frame, sett->castle_knights);

	if (!BIT_TEST(sett->flags, 1)) { /* Send weakest */
		draw_popup_icon(6, 84, 288, frame); /* checkbox */
	} else {
		draw_popup_icon(6, 100, 288, frame); /* checkbox */
	}

	int convertible_to_knights = 0;
	for (int i = 0; i < globals.max_ever_inventory_index; i++) {
		if (BIT_TEST(globals.inventories_bitmap[i>>3], 7-(i&7))) {
			inventory_t *inv = game_get_inventory(i);
			if (inv->player_num == sett->player_num) {
				int c = min(inv->resources[RESOURCE_SWORD],
					    inv->resources[RESOURCE_SHIELD]);
				convertible_to_knights += min(c, inv->spawn_priority);
			}
		}
	}

	draw_green_number(12, 40, frame, convertible_to_knights);
}

static void
draw_sett_6_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		237, 1, 120,
		238, 3, 120,
		239, 9, 120,
		240, 11, 120,

		295, 1, 4, /* default */
		60, 14, 128, /* exit */
		-1
	};

	draw_box_background(311, frame);
	draw_custom_icon_box(layout, frame);
	draw_popup_resource_stairs(popup->player->sett->inventory_prio, frame);

	draw_popup_icon(6, 120, 33+popup->player->sett->current_sett_6_item, frame);
}

static void
draw_bld_1_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		0xc0, 0, 5, /* stock */
		0xab, 2, 77, /* hut */
		0x9e, 8, 7, /* tower */
		0x98, 6, 69, /* fortress */
		-1
	};

	draw_box_background(313, frame);

	draw_popup_building(4, 112, 0x80 + 4*popup->player->sett->player_num, frame);
	draw_custom_bld_box(layout, frame);

	draw_popup_icon(0, 128, 0x3d, frame); /* flipbox */
	draw_popup_icon(14, 128, 0x3c, frame); /* exit */
}

static void
draw_bld_2_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		153, 0, 4,
		160, 8, 6,
		157, 0, 68,
		169, 8, 65,
		174, 12, 57,
		170, 4, 105,
		168, 8, 107,
		-1
	};

	draw_box_background(313, frame);

	draw_custom_bld_box(layout, frame);

	draw_popup_icon(0, 128, 0x3d, frame); /* flipbox */
	draw_popup_icon(14, 128, 0x3c, frame); /* exit */
}

static void
draw_bld_3_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		155, 0, 2,
		154, 8, 3,
		167, 0, 61,
		156, 8, 60,
		188, 4, 75,
		162, 8, 100,
		-1
	};

	draw_box_background(313, frame);

	draw_custom_bld_box(layout, frame);

	draw_popup_icon(0, 128, 0x3d, frame); /* flipbox */
	draw_popup_icon(14, 128, 0x3c, frame); /* exit */
}

static void
draw_bld_4_box(popup_box_t *popup, frame_t *frame)
{
	const int layout[] = {
		163, 0, 4,
		164, 4, 4,
		165, 8, 4,
		166, 12, 4,
		161, 2, 90,
		159, 8, 90,
		-1
	};

	draw_box_background(313, frame);

	draw_custom_bld_box(layout, frame);

	draw_popup_icon(0, 128, 0x3d, frame); /* flipbox */
	draw_popup_icon(14, 128, 0x3c, frame); /* exit */
}

/* Draw .. message popup box. */
static void
draw_under_attack_message_box(frame_t *frame, int opponent)
{
	draw_green_string(0, 10, frame, "YOUR SETTLEMENT");
	draw_green_string(0, 20, frame, "IS UNDER ATTACK");
	draw_player_face(6, 40, opponent, frame);
}

static void
draw_lost_fight_message_box(frame_t *frame, int opponent)
{
	draw_green_string(0, 10, frame, "  YOUR KNIGHTS");
	draw_green_string(0, 20, frame, " JUST LOST THE");
	draw_green_string(0, 30, frame, "     FIGHT");
	draw_player_face(6, 50, opponent, frame);
}

static void
draw_victory_fight_message_box(frame_t *frame, int opponent)
{
	draw_green_string(0, 10, frame, "   YOU GAINED");
	draw_green_string(0, 20, frame, " A VICTORY HERE");
	draw_player_face(6, 40, opponent, frame);
}

static void
draw_mine_empty_message_box(frame_t *frame, int mine)
{
	draw_green_string(0, 10, frame, "THIS MINE HAULS");
	draw_green_string(0, 20, frame, "  NO MORE RAW");
	draw_green_string(0, 30, frame, "   MATERIALS");
	draw_popup_building(6, 50, map_building_sprite[BUILDING_STONEMINE] + mine,
			    frame);
}

static void
draw_call_to_location_message_box(frame_t *frame, int param)
{
	draw_green_string(0, 10, frame, " YOU WANTED ME");
	draw_green_string(0, 20, frame, " TO CALL YOU TO");
	draw_green_string(0, 30, frame, " THIS LOCATION");
	draw_popup_building(6, 50, 0x90, frame);
}

static void
draw_knight_occupied_message_box(frame_t *frame, int building)
{
	draw_green_string(0, 10, frame, "  A KNIGHT HAS");
	draw_green_string(0, 20, frame, "  OCCUPIED THIS");
	draw_green_string(0, 30, frame, "  NEW BUILDING");

	switch (building) {
	case 0:
		draw_popup_building(6, 50,
				    map_building_sprite[BUILDING_HUT],
				    frame);
		break;
	case 1:
		draw_popup_building(6, 50,
				    map_building_sprite[BUILDING_TOWER],
				    frame);
		break;
	case 2:
		draw_popup_building(4, 50,
				    map_building_sprite[BUILDING_FORTRESS],
				    frame);
		break;
	default:
		NOT_REACHED();
		break;
	}
}

static void
draw_new_stock_message_box(frame_t *frame, int param)
{
	draw_green_string(0, 10, frame, "A NEW STOCK HAS");
	draw_green_string(0, 20, frame, "  BEEN BUILT");
	draw_popup_building(4, 40, map_building_sprite[BUILDING_STOCK], frame);
}

static void
draw_lost_land_message_box(frame_t *frame, int opponent)
{
	draw_green_string(0, 10, frame, "BECAUSE OF THIS");
	draw_green_string(0, 20, frame, " ENEMY BUILDING");
	draw_green_string(0, 30, frame, "    YOU LOST");
	draw_green_string(0, 40, frame, "   SOME LAND");
	draw_player_face(6, 65, opponent, frame);
}

static void
draw_lost_buildings_message_box(frame_t *frame, int opponent)
{
	draw_green_string(0, 10, frame, "BECAUSE OF THIS");
	draw_green_string(0, 20, frame, " ENEMY BUILDING");
	draw_green_string(0, 30, frame, "    YOU LOST");
	draw_green_string(0, 40, frame, "   SOME LAND");
	draw_green_string(0, 50, frame, " AND BUILDINGS");
	draw_player_face(6, 75, opponent, frame);
}

static void
draw_emergency_active_message_box(frame_t *frame, int param)
{
	draw_green_string(0, 10, frame, "   EMERGENCY");
	draw_green_string(0, 20, frame, "    PROGRAM");
	draw_green_string(0, 30, frame, "   ACTIVATED");
	draw_popup_building(4, 60, map_building_sprite[BUILDING_CASTLE] + 1, frame);
}

static void
draw_emergency_neutral_message_box(frame_t *frame, int param)
{
	draw_green_string(0, 10, frame, "   EMERGENCY");
	draw_green_string(0, 20, frame, "    PROGRAM");
	draw_green_string(0, 30, frame, "  NEUTRALIZED");
	draw_popup_building(4, 60, map_building_sprite[BUILDING_CASTLE], frame);
}

static void
draw_found_gold_message_box(frame_t *frame, int param)
{
	draw_green_string(0, 10, frame, "   A GEOLOGIST");
	draw_green_string(0, 20, frame, " HAS FOUND GOLD");
	draw_popup_icon(7, 40, 0x2f, frame);
}

static void
draw_found_iron_message_box(frame_t *frame, int param)
{
	draw_green_string(0, 10, frame, "   A GEOLOGIST");
	draw_green_string(0, 20, frame, " HAS FOUND IRON");
	draw_popup_icon(7, 40, 0x2c, frame);
}

static void
draw_found_coal_message_box(frame_t *frame, int param)
{
	draw_green_string(0, 10, frame, "   A GEOLOGIST");
	draw_green_string(0, 20, frame, " HAS FOUND COAL");
	draw_popup_icon(7, 40, 0x2e, frame);
}

static void
draw_found_stone_message_box(frame_t *frame, int param)
{
	draw_green_string(0, 10, frame, "   A GEOLOGIST");
	draw_green_string(0, 20, frame, " HAS FOUND STONE");
	draw_popup_icon(7, 40, 0x2b, frame);
}

static void
draw_call_to_menu_message_box(frame_t *frame, int menu)
{
	const int map_menu_sprite[] = {
		0xe6, 0xe7, 0xe8, 0xe9,
		0xea, 0xeb, 0x12a, 0x12b
	};

	draw_green_string(0, 10, frame, " YOU WANTED ME");
	draw_green_string(0, 20, frame, " TO CALL YOU TO");
	draw_green_string(0, 30, frame, "   THIS MENU");
	draw_popup_icon(6, 50, map_menu_sprite[menu], frame);
}

static void
draw_30m_since_save_message_box(frame_t *frame, int param)
{
	draw_green_string(0, 20, frame, " 30 MIN. PASSED");
	draw_green_string(0, 30, frame, " SINCE THE LAST");
	draw_green_string(0, 40, frame, "     SAVING");
	draw_popup_icon(7, 60, 0x5d, frame);
}

static void
draw_1h_since_save_message_box(frame_t *frame, int param)
{
	draw_green_string(0, 20, frame, "ONE HOUR PASSED");
	draw_green_string(0, 30, frame, " SINCE THE LAST");
	draw_green_string(0, 40, frame, "     SAVING");
	draw_popup_icon(7, 60, 0x5d, frame);
}

static void
draw_call_to_stock_message_box(frame_t *frame, int param)
{
	draw_green_string(0, 10, frame, " YOU WANTED ME");
	draw_green_string(0, 20, frame, " TO CALL YOU TO");
	draw_green_string(0, 30, frame, "   THIS STOCK");
	draw_popup_building(4, 50, map_building_sprite[BUILDING_STOCK], frame);
}

/* Dispatch to one of the message box functions above.
   Note: message box is a specific type of popup box. */
static void
draw_message_box(popup_box_t *popup, frame_t *frame)
{
	draw_box_background(0x13a, frame);
	draw_popup_icon(14, 128, 0x120, frame); /* Checkbox */

	int type = popup->player->message_box & 0x1f;
	int param = (popup->player->message_box >> 5) & 7;
	switch (type) {
	case 1:
		draw_under_attack_message_box(frame, param);
		break;
	case 2:
		draw_lost_fight_message_box(frame, param);
		break;
	case 3:
		draw_victory_fight_message_box(frame, param);
		break;
	case 4:
		draw_mine_empty_message_box(frame, param);
		break;
	case 5:
		draw_call_to_location_message_box(frame, param);
		break;
	case 6:
		draw_knight_occupied_message_box(frame, param);
		break;
	case 7:
		draw_new_stock_message_box(frame, param);
		break;
	case 8:
		draw_lost_land_message_box(frame, param);
		break;
	case 9:
		draw_lost_buildings_message_box(frame, param);
		break;
	case 10:
		draw_emergency_active_message_box(frame, param);
		break;
	case 11:
		draw_emergency_neutral_message_box(frame, param);
		break;
	case 12:
		draw_found_gold_message_box(frame, param);
		break;
	case 13:
		draw_found_iron_message_box(frame, param);
		break;
	case 14:
		draw_found_coal_message_box(frame, param);
		break;
	case 15:
		draw_found_stone_message_box(frame, param);
		break;
	case 16:
		draw_call_to_menu_message_box(frame, param);
		break;
	case 17:
		draw_30m_since_save_message_box(frame, param);
		break;
	case 18:
		draw_1h_since_save_message_box(frame, param);
		break;
	case 19:
		draw_call_to_stock_message_box(frame, param);
		break;
	default:
		NOT_REACHED();
		break;
	}
}

static void
draw_building_stock_box(popup_box_t *popup, frame_t *frame)
{
	draw_box_background(0x138, frame);

	if (popup->player->sett->index == 0) return;/*player_close_popup();*/

	building_t *building = game_get_building(popup->player->sett->index);
	if (BIT_TEST(building->serf, 5)) return;/*player_close_popup();*/ /* Building is burning */

	int sprite1 = -1;
	int sprite2 = -1;
	building_type_t type = BUILDING_TYPE(building);

	switch (type) {
	case BUILDING_BOATBUILDER:
		sprite1 = 34 + RESOURCE_PLANK;
		break;
	case BUILDING_BUTCHER:
		sprite1 = 34 + RESOURCE_PIG;
		break;
	case BUILDING_PIGFARM:
	case BUILDING_MILL:
		sprite1 = 34 + RESOURCE_WHEAT;
		break;
	case BUILDING_BAKER:
		sprite1 = 34 + RESOURCE_FLOUR;
		break;
	case BUILDING_SAWMILL:
		sprite2 = 34 + RESOURCE_LUMBER;
		break;
	case BUILDING_STEELSMELTER:
		sprite1 = 34 + RESOURCE_COAL;
		sprite2 = 34 + RESOURCE_IRONORE;
		break;
	case BUILDING_TOOLMAKER:
		sprite1 = 34 + RESOURCE_PLANK;
		sprite2 = 34 + RESOURCE_STEEL;
		break;
	case BUILDING_WEAPONSMITH:
		sprite1 = 34 + RESOURCE_COAL;
		sprite2 = 34 + RESOURCE_STEEL;
		break;
	case BUILDING_GOLDSMELTER:
		sprite1 = 34 + RESOURCE_COAL;
		sprite2 = 34 + RESOURCE_GOLDORE;
		break;
	case BUILDING_FISHER:
	case BUILDING_LUMBERJACK:
	case BUILDING_STONECUTTER:
	case BUILDING_FORESTER:
	case BUILDING_FARM:
		break;
	default:
		NOT_REACHED();
	}

	/* Draw list of primary resource */
	if (sprite1 >= 0) {
		int stock1 = (building->stock1 >> 4) & 0xf;
		if (stock1 > 0) {
			for (int i = 0; i < stock1; i++) {
				draw_popup_icon(8-stock1+2*i, 110, sprite1, frame);
			}
		} else {
			draw_popup_icon(7, 110, 0xdc, frame); /* minus box */
		}
	}

	/* Draw list of secondary resource */
	if (sprite2 >= 0) {
		int stock2 = (building->stock2 >> 4) & 0xf;
		if (stock2 > 0) {
			for (int i = 0; i < stock2; i++) {
				draw_popup_icon(8-stock2+2*i, 90, sprite2, frame);
			}
		} else {
			draw_popup_icon(7, 90, 0xdc, frame); /* minus box */
		}
	}

	const int map_building_serf_sprite[] = {
		-1, 0x13, 0xd, 0x19,
		0xf, -1, -1, -1,
		-1, 0x10, -1, -1,
		0x16, 0x15, 0x14, 0x17,
		0x18, 0xe, 0x12, 0x1a,
		0x1b, -1, -1, 0x12,
		-1
	};

	/* Draw picture of serf present */
	int serf_sprite = 0xdc; /* minus box */
	if (BIT_TEST(building->serf, 6)) serf_sprite = map_building_serf_sprite[type];

	draw_popup_icon(1, 36, serf_sprite, frame);

	/* Draw building */
	int bld_sprite = map_building_sprite[type];
	int x = 6;
	if (bld_sprite == 0xc0 /*stock*/ || bld_sprite >= 0x9e /*tower*/) x = 4;
	draw_popup_building(x, 30, bld_sprite, frame);

	draw_green_string(1, 4, frame, "STOCK OF");
	draw_green_string(1, 14, frame, "THIS BUILDING:");

	draw_popup_icon(14, 128, 0x3c, frame); /* exit box */
}

static void
draw_player_faces_box(popup_box_t *popup, frame_t *frame)
{
	draw_box_background(129, frame);

	draw_player_face(2, 4, 0, frame);
	draw_player_face(10, 4, 1, frame);
	draw_player_face(2, 76, 2, frame);
	draw_player_face(10, 76, 3, frame);
}

static void
draw_demolish_box(popup_box_t *popup, frame_t *frame)
{
	draw_box_background(314, frame);

	draw_popup_icon(14, 128, 60, frame); /* Exit */
	draw_popup_icon(7, 45, 288, frame); /* Checkbox */

	draw_green_string(0, 10, frame, "    Demolish:");
	draw_green_string(0, 30, frame, "   Click here");
	draw_green_string(0, 68, frame, "   if you are");
	draw_green_string(0, 86, frame, "      sure");
}

static void
popup_box_draw(popup_box_t *popup, frame_t *frame)
{
	draw_popup_box_frame(frame);

	if (BIT_TEST(popup->player->flags, 0)) return; /* Player inactive */

	box_t box = popup->player->box;
#if 0
	if (box == 0) {
		if (popup->player->clkmap == 0) return;
		box = popup->player->clkmap;

		/* Certain boxes need to be redrawn periodically */
		if (box < 32 && !BIT_TEST(0x20ffe04, box)) return;
		else if (!BIT_TEST(0x103ec0, box-32)) return;

		if (box == BOX_25) {
			/*if (!BIT_TEST(globals.string_bg, 1)) return;*/
			/*globals.string_bg &= ~BIT(1);*/
		} else if (globals.anim - popup->player->last_anim < 100/*1000*/) return;
	}

	popup->player_open_popup(player, 0);
#endif
	popup->player->last_anim = globals.anim;
	popup->player->clkmap = box;

	/* Dispatch to one of the popup box functions above. */
	switch (box) {
	case BOX_MAP:
		draw_map_box(popup, frame);
		break;
	case BOX_MAP_OVERLAY:
		draw_map_overlay_box(popup, frame);
		break;
	case BOX_MINE_BUILDING:
		draw_mine_building_box(popup, frame);
		break;
	case BOX_BASIC_BLD:
		draw_basic_building_box(popup, frame, 0);
		break;
	case BOX_BASIC_BLD_FLIP:
		draw_basic_building_box(popup, frame, 1);
		break;
	case BOX_ADV_1_BLD:
		draw_adv_1_building_box(popup, frame);
		break;
	case BOX_ADV_2_BLD:
		draw_adv_2_building_box(popup, frame);
		break;
	case BOX_STAT_SELECT:
		draw_stat_select_box(popup, frame);
		break;
	case BOX_STAT_4:
		draw_stat_4_box(popup, frame);
		break;
	case BOX_STAT_BLD_1:
		draw_stat_bld_1_box(popup, frame);
		break;
	case BOX_STAT_BLD_2:
		draw_stat_bld_2_box(popup, frame);
		break;
	case BOX_STAT_BLD_3:
		draw_stat_bld_3_box(popup, frame);
		break;
	case BOX_STAT_BLD_4:
		draw_stat_bld_4_box(popup, frame);
		break;
	case BOX_STAT_8:
		draw_stat_8_box(popup, frame);
		break;
	case BOX_STAT_7:
		draw_stat_7_box(popup, frame);
		break;
	case BOX_STAT_1:
		draw_stat_1_box(popup, frame);
		break;
	case BOX_STAT_2:
		draw_stat_2_box(popup, frame);
		break;
	case BOX_STAT_6:
		draw_stat_6_box(popup, frame);
		break;
	case BOX_STAT_3:
		draw_stat_3_box(popup, frame);
		break;
	case BOX_START_ATTACK:
		draw_start_attack_box(popup, frame);
		break;
	case BOX_START_ATTACK_REDRAW:
		draw_start_attack_redraw_box(popup, frame);
		break;
	case BOX_GROUND_ANALYSIS:
		draw_ground_analysis_box(popup, frame);
		break;
		/* TODO */
	case BOX_SETT_SELECT:
	case BOX_SETT_SELECT_FILE:
		draw_sett_select_box(popup, frame);
		break;
	case BOX_SETT_1:
		draw_sett_1_box(popup, frame);
		break;
	case BOX_SETT_2:
		draw_sett_2_box(popup, frame);
		break;
	case BOX_SETT_3:
		draw_sett_3_box(popup, frame);
		break;
	case BOX_KNIGHT_LEVEL:
		draw_knight_level_box(popup, frame);
		break;
	case BOX_SETT_4:
		draw_sett_4_box(popup, frame);
		break;
	case BOX_SETT_5:
		draw_sett_5_box(popup, frame);
		break;
	case BOX_QUIT_CONFIRM:
		draw_quit_confirm_box(popup, frame);
		break;
	case BOX_NO_SAVE_QUIT_CONFIRM:
		draw_no_save_quit_confirm_box(popup, frame);
		break;
	case BOX_OPTIONS:
		draw_options_box(popup, frame);
		break;
	case BOX_CASTLE_RES:
		draw_castle_res_box(popup, frame);
		break;
	case BOX_MINE_OUTPUT:
		draw_mine_output_box(popup, frame);
		break;
	case BOX_ORDERED_BLD:
		draw_ordered_building_box(popup, frame);
		break;
	case BOX_DEFENDERS:
		draw_defenders_box(popup, frame);
		break;
	case BOX_TRANSPORT_INFO:
		draw_transport_info_box(popup, frame);
		break;
	case BOX_CASTLE_SERF:
		draw_castle_serf_box(popup, frame);
		break;
	case BOX_RESDIR:
		draw_resdir_box(popup, frame);
		break;
	case BOX_SETT_8:
		draw_sett_8_box(popup, frame);
		break;
	case BOX_SETT_6:
		draw_sett_6_box(popup, frame);
		break;
	case BOX_BLD_1:
		draw_bld_1_box(popup, frame);
		break;
	case BOX_BLD_2:
		draw_bld_2_box(popup, frame);
		break;
	case BOX_BLD_3:
		draw_bld_3_box(popup, frame);
		break;
	case BOX_BLD_4:
		draw_bld_4_box(popup, frame);
		break;
	case BOX_MESSAGE:
		draw_message_box(popup, frame);
		break;
	case BOX_BLD_STOCK:
		draw_building_stock_box(popup, frame);
		break;
	case BOX_PLAYER_FACES:
		draw_player_faces_box(popup, frame);
		break;
	case BOX_DEMOLISH:
		draw_demolish_box(popup, frame);
		break;
	default:
		break;
	}
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
	player_open_popup(player, player->clkmap);
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

	player_open_popup(player, player->clkmap);
}

static void
handle_send_geologist(player_t *player)
{
	map_pos_t pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
	flag_t *flag = game_get_flag(MAP_OBJ_INDEX(pos));

	int r = game_send_geologist(flag, MAP_OBJ_INDEX(pos));
	if (r < 0) {
		sfx_play_clip(SFX_NOT_ACCEPTED);
	} else {
		sfx_play_clip(SFX_ACCEPTED);
		player_close_popup(player);
	}
}

static void
sett_8_train(player_t *player, int number)
{
	int r = player_promote_serfs_to_knights(player->sett, number);

	if (r == 0) sfx_play_clip(SFX_NOT_ACCEPTED);
	else sfx_play_clip(SFX_ACCEPTED);

	player_open_popup(player, player->clkmap);
}

static void
set_inventory_resource_mode(player_t *player, int mode)
{
	building_t *building = game_get_building(player->sett->index);
	inventory_t *inventory = building->u.inventory;
	game_set_inventory_resource_mode(inventory, mode);
}

static void
set_inventory_serf_mode(player_t *player, int mode)
{
	building_t *building = game_get_building(player->sett->index);
	inventory_t *inventory = building->u.inventory;
	game_set_inventory_serf_mode(inventory, mode);
}

/* Generic handler for clicks in popup boxes. */
static int
handle_clickmap(player_t *player, int x, int y, const int clkmap[])
{
	while (clkmap[0] >= 0) {
		if (clkmap[1] <= x && x <= clkmap[2] &&
		    clkmap[3] <= y && y <= clkmap[4]) {
			sfx_play_clip(SFX_CLICK);

			action_t action = clkmap[0];
			switch (action) {
			case ACTION_MINIMAP_CLICK:
				/* Not handled here, event is passed to minimap. */
				break;
			case ACTION_MINIMAP_MODE: {
				int mode = (player->minimap_flags & 3) + 1;
				player->minimap_flags &= ~3;
				if (mode != 3) {
					player->minimap_flags |= mode;
				}
				player->box = BOX_MAP;
				break;
			}
			case ACTION_MINIMAP_ROADS:
				BIT_INVERT(player->minimap_flags, 2);
				player->box = BOX_MAP;
				break;
			case ACTION_MINIMAP_BUILDINGS:
				if (BIT_TEST(player->click,3)) {
					if (player->minimap_advanced >= 0) {
						player->minimap_advanced = -1;
					} else {
						player->box = BOX_BLD_1;
					}
				} else {
					if (player->minimap_advanced >= 0) {
						player->minimap_advanced = -1;
						player->minimap_flags |= BIT(3);
					} else {
						BIT_INVERT(player->minimap_flags, 3);
					}
					player->box = BOX_MAP;
				}
				break;
			case ACTION_MINIMAP_GRID:
				BIT_INVERT(player->minimap_flags, 4);
				player->box = BOX_MAP;
				break;
			case ACTION_BUILD_STONEMINE:
				globals.building_type = BUILDING_STONEMINE;
				player_build_mine_building(player);
				break;
			case ACTION_BUILD_COALMINE:
				globals.building_type = BUILDING_COALMINE;
				player_build_mine_building(player);
				break;
			case ACTION_BUILD_IRONMINE:
				globals.building_type = BUILDING_IRONMINE;
				player_build_mine_building(player);
				break;
			case ACTION_BUILD_GOLDMINE:
				globals.building_type = BUILDING_GOLDMINE;
				player_build_mine_building(player);
				break;
			case ACTION_BUILD_FLAG:
				if (BIT_TEST(player->sett->build, 1)) break; /* Can not build flag */
				player_build_flag(player);
				player_close_popup(player);
				break;
			case ACTION_BUILD_STONECUTTER:
				globals.building_type = BUILDING_STONECUTTER;
				player_build_basic_building(player);
				break;
			case ACTION_BUILD_HUT:
				if (!BIT_TEST(player->sett->build, 0)) { /* Can build military building */
					globals.building_type = BUILDING_HUT;
					player_build_basic_building(player);
				}
				break;
			case ACTION_BUILD_LUMBERJACK:
				globals.building_type = BUILDING_LUMBERJACK;
				player_build_basic_building(player);
				break;
			case ACTION_BUILD_FORESTER:
				globals.building_type = BUILDING_FORESTER;
				player_build_basic_building(player);
				break;
			case ACTION_BUILD_FISHER:
				globals.building_type = BUILDING_FISHER;
				player_build_basic_building(player);
				break;
			case ACTION_BUILD_MILL:
				globals.building_type = BUILDING_MILL;
				player_build_basic_building(player);
				break;
			case ACTION_BUILD_BOATBUILDER:
				globals.building_type = BUILDING_BOATBUILDER;
				player_build_basic_building(player);
				break;
			case ACTION_BUILD_BUTCHER:
				globals.building_type = BUILDING_BUTCHER;
				player_build_advanced_building(player);
				break;
			case ACTION_BUILD_WEAPONSMITH:
				globals.building_type = BUILDING_WEAPONSMITH;
				player_build_advanced_building(player);
				break;
			case ACTION_BUILD_STEELSMELTER:
				globals.building_type = BUILDING_STEELSMELTER;
				player_build_advanced_building(player);
				break;
			case ACTION_BUILD_SAWMILL:
				globals.building_type = BUILDING_SAWMILL;
				player_build_advanced_building(player);
				break;
			case ACTION_BUILD_BAKER:
				globals.building_type = BUILDING_BAKER;
				player_build_advanced_building(player);
				break;
			case ACTION_BUILD_GOLDSMELTER:
				globals.building_type = BUILDING_GOLDSMELTER;
				player_build_advanced_building(player);
				break;
			case ACTION_BUILD_FORTRESS:
				if (!BIT_TEST(player->sett->build, 0)) { /* Can build military building */
					globals.building_type = BUILDING_FORTRESS;
					player_build_advanced_building(player);
				}
				break;
			case ACTION_BUILD_TOWER:
				if (!BIT_TEST(player->sett->build, 0)) { /* Can build military building */
					globals.building_type = BUILDING_TOWER;
					player_build_advanced_building(player);
				}
				break;
			case ACTION_BUILD_TOOLMAKER:
				globals.building_type = BUILDING_TOOLMAKER;
				player_build_advanced_building(player);
				break;
			case ACTION_BUILD_FARM:
				globals.building_type = BUILDING_FARM;
				player_build_advanced_building(player);
				break;
			case ACTION_BUILD_PIGFARM:
				globals.building_type = BUILDING_PIGFARM;
				player_build_advanced_building(player);
				break;
			case ACTION_BLD_FLIP_PAGE:
				player_open_popup(player, (player->clkmap + 1 <= BOX_ADV_2_BLD) ? (player->clkmap + 1) : BOX_BASIC_BLD_FLIP);
				break;
			case ACTION_SHOW_STAT_1:
				player_open_popup(player, BOX_STAT_1);
				break;
			case ACTION_SHOW_STAT_2:
				player_open_popup(player, BOX_STAT_2);
				break;
			case ACTION_SHOW_STAT_8:
				player_open_popup(player, BOX_STAT_8);
				break;
			case ACTION_SHOW_STAT_BLD:
				player_open_popup(player, BOX_STAT_BLD_1);
				break;
			case ACTION_SHOW_STAT_6:
				player_open_popup(player, BOX_STAT_6);
				break;
			case ACTION_SHOW_STAT_7:
				player_open_popup(player, BOX_STAT_7);
				break;
			case ACTION_SHOW_STAT_4:
				player_open_popup(player, BOX_STAT_4);
				break;
			case ACTION_SHOW_STAT_3:
				player_open_popup(player, BOX_STAT_3);
				break;
			case ACTION_SHOW_STAT_SELECT:
			case ACTION_SHOW_STAT_SELECT_FILE:
				player_open_popup(player, BOX_STAT_SELECT);
				break;
			case ACTION_STAT_BLD_FLIP:
				player_open_popup(player, (player->clkmap + 1 <= BOX_STAT_BLD_4) ? (player->clkmap + 1) : BOX_STAT_BLD_1);
				break;
			case ACTION_CLOSE_BOX:
			case ACTION_CLOSE_SETT_BOX:
			case ACTION_CLOSE_GROUND_ANALYSIS:
				player_close_popup(player);
				break;
			case ACTION_SETT_8_SET_ASPECT_ALL:
				player->current_stat_8_mode = (0 << 2) | (player->current_stat_8_mode & 3);
				player_open_popup(player, player->clkmap);
				break;
			case ACTION_SETT_8_SET_ASPECT_LAND:
				player->current_stat_8_mode = (1 << 2) | (player->current_stat_8_mode & 3);
				player_open_popup(player, player->clkmap);
				break;
			case ACTION_SETT_8_SET_ASPECT_BUILDINGS:
				player->current_stat_8_mode = (2 << 2) | (player->current_stat_8_mode & 3);
				player_open_popup(player, player->clkmap);
				break;
			case ACTION_SETT_8_SET_ASPECT_MILITARY:
				player->current_stat_8_mode = (3 << 2) | (player->current_stat_8_mode & 3);
				player_open_popup(player, player->clkmap);
				break;
			case ACTION_SETT_8_SET_SCALE_30_MIN:
				player->current_stat_8_mode = (player->current_stat_8_mode & 0xc) | 0;
				player_open_popup(player, player->clkmap);
				break;
			case ACTION_SETT_8_SET_SCALE_60_MIN:
				player->current_stat_8_mode = (player->current_stat_8_mode & 0xc) | 1;
				player_open_popup(player, player->clkmap);
				break;
			case ACTION_SETT_8_SET_SCALE_600_MIN:
				player->current_stat_8_mode = (player->current_stat_8_mode & 0xc) | 2;
				player_open_popup(player, player->clkmap);
				break;
			case ACTION_SETT_8_SET_SCALE_3000_MIN:
				player->current_stat_8_mode = (player->current_stat_8_mode & 0xc) | 3;
				player_open_popup(player, player->clkmap);
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
				player_open_popup(player, player->clkmap);
				break;
			case ACTION_ATTACKING_KNIGHTS_DEC:
				player->sett->knights_attacking = max(player->sett->knights_attacking-1, 0);
				break;
			case ACTION_ATTACKING_KNIGHTS_INC:
				player->sett->knights_attacking = min(player->sett->knights_attacking+1,
								      min(player->sett->total_attacking_knights, 100));
				break;
			case ACTION_START_ATTACK:
				if (player->sett->knights_attacking > 0) {
					if (player->sett->attacking_building_count > 0) {
						sfx_play_clip(SFX_ACCEPTED);
						player_start_attack(player->sett);
					}
					player_close_popup(player);
				} else {
					sfx_play_clip(SFX_NOT_ACCEPTED);
				}
				break;
			case ACTION_CLOSE_ATTACK_BOX:
				player_close_popup(player);
				break;
				/* TODO */
			case ACTION_SHOW_SETT_1:
				player_open_popup(player, BOX_SETT_1);
				break;
			case ACTION_SHOW_SETT_2:
				player_open_popup(player, BOX_SETT_2);
				break;
			case ACTION_SHOW_SETT_3:
				player_open_popup(player, BOX_SETT_3);
				break;
			case ACTION_SHOW_SETT_7:
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_SHOW_SETT_4:
				player_open_popup(player, BOX_SETT_4);
				break;
			case ACTION_SHOW_SETT_5:
				player_open_popup(player, BOX_SETT_5);
				break;
			case ACTION_SHOW_SETT_SELECT:
			case ACTION_SHOW_SETT_SELECT_FILE:
				player_open_popup(player, BOX_SETT_SELECT);
				break;
			case ACTION_SETT_1_ADJUST_STONEMINE:
				player_open_popup(player, BOX_SETT_1);
				player->sett->food_stonemine = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_1_ADJUST_COALMINE:
				player_open_popup(player, BOX_SETT_1);
				player->sett->food_coalmine = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_1_ADJUST_IRONMINE:
				player_open_popup(player, BOX_SETT_1);
				player->sett->food_ironmine = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_1_ADJUST_GOLDMINE:
				player_open_popup(player, BOX_SETT_1);
				player->sett->food_goldmine = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_2_ADJUST_CONSTRUCTION:
				player_open_popup(player, BOX_SETT_2);
				player->sett->planks_construction = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_2_ADJUST_BOATBUILDER:
				player_open_popup(player, BOX_SETT_2);
				player->sett->planks_boatbuilder = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_2_ADJUST_TOOLMAKER_PLANKS:
				player_open_popup(player, BOX_SETT_2);
				player->sett->planks_toolmaker = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_2_ADJUST_TOOLMAKER_STEEL:
				player_open_popup(player, BOX_SETT_2);
				player->sett->steel_toolmaker = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_2_ADJUST_WEAPONSMITH:
				player_open_popup(player, BOX_SETT_2);
				player->sett->steel_weaponsmith = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_3_ADJUST_STEELSMELTER:
				player_open_popup(player, BOX_SETT_3);
				player->sett->coal_steelsmelter = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_3_ADJUST_GOLDSMELTER:
				player_open_popup(player, BOX_SETT_3);
				player->sett->coal_goldsmelter = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_3_ADJUST_WEAPONSMITH:
				player_open_popup(player, BOX_SETT_3);
				player->sett->coal_weaponsmith = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_3_ADJUST_PIGFARM:
				player_open_popup(player, BOX_SETT_3);
				player->sett->wheat_pigfarm = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_3_ADJUST_MILL:
				player_open_popup(player, BOX_SETT_3);
				player->sett->wheat_mill = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_KNIGHT_LEVEL_CLOSEST_MIN_DEC:
				player_change_knight_occupation(player->sett, 3, 0, -1);
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_KNIGHT_LEVEL_CLOSEST_MIN_INC:
				player_change_knight_occupation(player->sett, 3, 0, 1);
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_KNIGHT_LEVEL_CLOSEST_MAX_DEC:
				player_change_knight_occupation(player->sett, 3, 1, -1);
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_KNIGHT_LEVEL_CLOSEST_MAX_INC:
				player_change_knight_occupation(player->sett, 3, 1, 1);
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_KNIGHT_LEVEL_CLOSE_MIN_DEC:
				player_change_knight_occupation(player->sett, 2, 0, -1);
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_KNIGHT_LEVEL_CLOSE_MIN_INC:
				player_change_knight_occupation(player->sett, 2, 0, 1);
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_KNIGHT_LEVEL_CLOSE_MAX_DEC:
				player_change_knight_occupation(player->sett, 2, 1, -1);
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_KNIGHT_LEVEL_CLOSE_MAX_INC:
				player_change_knight_occupation(player->sett, 2, 1, 1);
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_KNIGHT_LEVEL_FAR_MIN_DEC:
				player_change_knight_occupation(player->sett, 1, 0, -1);
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_KNIGHT_LEVEL_FAR_MIN_INC:
				player_change_knight_occupation(player->sett, 1, 0, 1);
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_KNIGHT_LEVEL_FAR_MAX_DEC:
				player_change_knight_occupation(player->sett, 1, 1, -1);
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_KNIGHT_LEVEL_FAR_MAX_INC:
				player_change_knight_occupation(player->sett, 1, 1, 1);
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_KNIGHT_LEVEL_FARTHEST_MIN_DEC:
				player_change_knight_occupation(player->sett, 0, 0, -1);
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_KNIGHT_LEVEL_FARTHEST_MIN_INC:
				player_change_knight_occupation(player->sett, 0, 0, 1);
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_KNIGHT_LEVEL_FARTHEST_MAX_DEC:
				player_change_knight_occupation(player->sett, 0, 1, -1);
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_KNIGHT_LEVEL_FARTHEST_MAX_INC:
				player_change_knight_occupation(player->sett, 0, 1, 1);
				player_open_popup(player, BOX_KNIGHT_LEVEL);
				break;
			case ACTION_SETT_4_ADJUST_SHOVEL:
				player_open_popup(player, BOX_SETT_4);
				player->sett->tool_prio[0] = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_4_ADJUST_HAMMER:
				player_open_popup(player, BOX_SETT_4);
				player->sett->tool_prio[1] = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_4_ADJUST_AXE:
				player_open_popup(player, BOX_SETT_4);
				player->sett->tool_prio[5] = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_4_ADJUST_SAW:
				player_open_popup(player, BOX_SETT_4);
				player->sett->tool_prio[6] = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_4_ADJUST_SCYTHE:
				player_open_popup(player, BOX_SETT_4);
				player->sett->tool_prio[4] = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_4_ADJUST_PICK:
				player_open_popup(player, BOX_SETT_4);
				player->sett->tool_prio[7] = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_4_ADJUST_PINCER:
				player_open_popup(player, BOX_SETT_4);
				player->sett->tool_prio[8] = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_4_ADJUST_CLEAVER:
				player_open_popup(player, BOX_SETT_4);
				player->sett->tool_prio[3] = gui_get_slider_click_value(x - clkmap[1]);
				break;
			case ACTION_SETT_4_ADJUST_ROD:
				player_open_popup(player, BOX_SETT_4);
				player->sett->tool_prio[2] = gui_get_slider_click_value(x - clkmap[1]);
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
				player_open_popup(player, BOX_OPTIONS);
				break;
				/* TODO */
			case ACTION_SETT_8_CYCLE:
				player->sett->flags |= BIT(2) | BIT(4);
				player->sett->field_170 = 1200;
				sfx_play_clip(SFX_ACCEPTED);
				break;
			case ACTION_CLOSE_OPTIONS:
				player_close_popup(player);
				globals.player[0]->config = globals.cfg_left;
				globals.player[1]->config = globals.cfg_right;
				break;
			case ACTION_OPTIONS_PATHWAY_SCROLLING_1:
				BIT_INVERT(globals.cfg_left, 0);
				break;
			case ACTION_OPTIONS_PATHWAY_SCROLLING_2:
				BIT_INVERT(globals.cfg_right, 0);
				break;
			case ACTION_OPTIONS_FAST_MAP_CLICK_1:
				BIT_INVERT(globals.cfg_left, 1);
				break;
			case ACTION_OPTIONS_FAST_MAP_CLICK_2:
				BIT_INVERT(globals.cfg_right, 1);
				break;
			case ACTION_OPTIONS_FAST_BUILDING_1:
				BIT_INVERT(globals.cfg_left, 2);
				break;
			case ACTION_OPTIONS_FAST_BUILDING_2:
				BIT_INVERT(globals.cfg_right, 2);
				break;
			case ACTION_OPTIONS_MESSAGE_COUNT_1:
				if (BIT_TEST(globals.cfg_left, 3)) {
					BIT_INVERT(globals.cfg_left, 3);
					globals.cfg_left |= BIT(4);
				} else if (BIT_TEST(globals.cfg_left, 4)) {
					BIT_INVERT(globals.cfg_left, 4);
					globals.cfg_left |= BIT(5);
				} else if (BIT_TEST(globals.cfg_left, 5)) {
					BIT_INVERT(globals.cfg_left, 5);
				} else {
					globals.cfg_left |= BIT(3) | BIT(4) | BIT(5);
				}
				break;
			case ACTION_OPTIONS_MESSAGE_COUNT_2:
				if (BIT_TEST(globals.cfg_right, 3)) {
					BIT_INVERT(globals.cfg_right, 3);
					globals.cfg_left |= BIT(4);
				} else if (BIT_TEST(globals.cfg_right, 4)) {
					BIT_INVERT(globals.cfg_right, 4);
					globals.cfg_left |= BIT(5);
				} else if (BIT_TEST(globals.cfg_right, 5)) {
					BIT_INVERT(globals.cfg_right, 5);
				} else {
					globals.cfg_right |= BIT(3) | BIT(4) | BIT(5);
				}
				break;
			case ACTION_DEFAULT_SETT_1:
				player_open_popup(player, BOX_SETT_1);
				player_sett_reset_food_priority(player->sett);
				break;
			case ACTION_DEFAULT_SETT_2:
				player_open_popup(player, BOX_SETT_2);
				player_sett_reset_planks_priority(player->sett);
				player_sett_reset_steel_priority(player->sett);
				break;
			case ACTION_DEFAULT_SETT_5_6:
				player_open_popup(player, player->clkmap);
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
				player_build_advanced_building(player);
				break;
			case ACTION_SHOW_CASTLE_SERF:
				player_open_popup(player, BOX_CASTLE_SERF);
				break;
			case ACTION_SHOW_RESDIR:
				player_open_popup(player, BOX_RESDIR);
				break;
			case ACTION_SHOW_CASTLE_RES:
				player_open_popup(player, BOX_CASTLE_RES);
				break;
			case ACTION_SEND_GEOLOGIST:
				handle_send_geologist(player);
				break;
			case ACTION_RES_MODE_IN:
			case ACTION_RES_MODE_STOP:
			case ACTION_RES_MODE_OUT:
				set_inventory_resource_mode(player, action - ACTION_RES_MODE_IN);
				break;
			case ACTION_SERF_MODE_IN:
			case ACTION_SERF_MODE_STOP:
			case ACTION_SERF_MODE_OUT:
				set_inventory_serf_mode(player, action - ACTION_SERF_MODE_IN);
				break;
			case ACTION_SHOW_SETT_8:
				player_open_popup(player, BOX_SETT_8);
				break;
			case ACTION_SHOW_SETT_6:
				player_open_popup(player, BOX_SETT_6);
				break;
			case ACTION_SETT_8_ADJUST_RATE:
				player->sett->serf_to_knight_rate = gui_get_slider_click_value(x - clkmap[1]);
				player_open_popup(player, player->clkmap);
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
				player_open_popup(player, BOX_SETT_3);
				player_sett_reset_coal_priority(player->sett);
				player_sett_reset_wheat_priority(player->sett);
				break;
			case ACTION_SETT_8_SET_COMBAT_MODE_WEAK:
				player->sett->flags &= ~BIT(1);
				player_open_popup(player, player->clkmap);
				sfx_play_clip(SFX_ACCEPTED);
				break;
			case ACTION_SETT_8_SET_COMBAT_MODE_STRONG:
				player->sett->flags |= BIT(1);
				player_open_popup(player, player->clkmap);
				sfx_play_clip(SFX_ACCEPTED);
				break;
			case ACTION_ATTACKING_SELECT_ALL_1:
				player->sett->knights_attacking =
					player->sett->attacking_knights[0];
				break;
			case ACTION_ATTACKING_SELECT_ALL_2:
				player->sett->knights_attacking =
					player->sett->attacking_knights[0] +
					player->sett->attacking_knights[1];
				break;
			case ACTION_ATTACKING_SELECT_ALL_3:
				player->sett->knights_attacking =
					player->sett->attacking_knights[0] +
					player->sett->attacking_knights[1] +
					player->sett->attacking_knights[2];
				break;
			case ACTION_ATTACKING_SELECT_ALL_4:
				player->sett->knights_attacking =
					player->sett->attacking_knights[0] +
					player->sett->attacking_knights[1] +
					player->sett->attacking_knights[2] +
					player->sett->attacking_knights[3];
				break;
			case ACTION_MINIMAP_BLD_1:
			case ACTION_MINIMAP_BLD_2:
			case ACTION_MINIMAP_BLD_3:
			case ACTION_MINIMAP_BLD_4:
			case ACTION_MINIMAP_BLD_5:
			case ACTION_MINIMAP_BLD_6:
			case ACTION_MINIMAP_BLD_7:
			case ACTION_MINIMAP_BLD_8:
			case ACTION_MINIMAP_BLD_9:
			case ACTION_MINIMAP_BLD_10:
			case ACTION_MINIMAP_BLD_11:
			case ACTION_MINIMAP_BLD_12:
			case ACTION_MINIMAP_BLD_13:
			case ACTION_MINIMAP_BLD_14:
			case ACTION_MINIMAP_BLD_15:
			case ACTION_MINIMAP_BLD_16:
			case ACTION_MINIMAP_BLD_17:
			case ACTION_MINIMAP_BLD_18:
			case ACTION_MINIMAP_BLD_19:
			case ACTION_MINIMAP_BLD_20:
			case ACTION_MINIMAP_BLD_21:
			case ACTION_MINIMAP_BLD_22:
			case ACTION_MINIMAP_BLD_23:
				player->minimap_advanced = action - ACTION_MINIMAP_BLD_1 + 1;
				player->minimap_flags |= BIT(3);
				player->box = BOX_MAP;
				break;
			case ACTION_MINIMAP_BLD_FLAG:
				player->minimap_advanced = 0;
				player->box = BOX_MAP;
				break;
			case ACTION_MINIMAP_BLD_NEXT:
				player->box = player->clkmap + 1;
				if (player->box > BOX_BLD_4) {
					player->box = BOX_BLD_1;
				}
				break;
			case ACTION_MINIMAP_BLD_EXIT:
				player->box = BOX_MAP;
				break;
			case ACTION_CLOSE_MESSAGE:
				if ((player->message_box & 0x1f) == 16) {
					/* TODO */
				} else {
					player_close_popup(player);
				}
				break;
			case ACTION_DEFAULT_SETT_4:
				player_open_popup(player, BOX_SETT_4);
				player_sett_reset_tool_priority(player->sett);
				break;
			case ACTION_SHOW_PLAYER_FACES:
				player_open_popup(player, BOX_PLAYER_FACES);
				break;
			case ACTION_MINIMAP_SCALE: {
				popup_box_t *popup = gui_get_popup_box();
				BIT_INVERT(player->minimap_flags, 5);
				minimap_set_scale(&popup->minimap,
						  popup->minimap.scale == 1 ? 2 : 1);
				player->box = BOX_MAP;
			}
				break;
				/* TODO */
			case ACTION_SETT_8_CASTLE_DEF_DEC:
				player->sett->castle_knights_wanted = max(1, player->sett->castle_knights_wanted-1);
				player_open_popup(player, player->clkmap);
				break;
			case ACTION_SETT_8_CASTLE_DEF_INC:
				player->sett->castle_knights_wanted = min(player->sett->castle_knights_wanted+1, 99);
				player_open_popup(player, player->clkmap);
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
				player_demolish_object(player);
				player_close_popup(player);
				break;
			default:
				LOGW("popup", "unhandled action %i", action);
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
		ACTION_CLOSE_BOX, 112, 127, 128, 143,
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
handle_start_attack_click(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_ATTACKING_KNIGHTS_DEC, 32, 47, 112, 127,
		ACTION_ATTACKING_KNIGHTS_INC, 80, 95, 112, 127,
		ACTION_START_ATTACK, 0, 31, 128, 143,
		ACTION_CLOSE_ATTACK_BOX, 112, 127, 128, 143,
		ACTION_ATTACKING_SELECT_ALL_1, 8, 23, 80, 103,
		ACTION_ATTACKING_SELECT_ALL_2, 40, 55, 80, 103,
		ACTION_ATTACKING_SELECT_ALL_3, 72, 87, 80, 103,
		ACTION_ATTACKING_SELECT_ALL_4, 104, 119, 80, 103,
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
handle_minimap_clk(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_MINIMAP_CLICK, 0, 127, 0, 127,
		ACTION_MINIMAP_MODE, 0, 31, 128, 143,
		ACTION_MINIMAP_ROADS, 32, 63, 128, 143,
		ACTION_MINIMAP_BUILDINGS, 64, 95, 128, 143,
		ACTION_MINIMAP_GRID, 96, 111, 128, 143,
		ACTION_MINIMAP_SCALE, 112, 127, 128, 143,
		-1
	};

	handle_clickmap(player, x, y, clkmap);
}

static void
handle_box_bld_1(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_MINIMAP_BLD_1, 0, 63, 0, 50,
		ACTION_MINIMAP_BLD_2, 64, 111, 0, 50,
		ACTION_MINIMAP_BLD_3, 16, 47, 64, 95,
		ACTION_MINIMAP_BLD_4, 48, 111, 60, 130,
		ACTION_MINIMAP_BLD_FLAG, 25, 40, 110, 143,
		ACTION_MINIMAP_BLD_NEXT, 0, 15, 128, 143,
		ACTION_MINIMAP_BLD_EXIT, 112, 127, 128, 143,
		-1
	};

	handle_clickmap(player, x, y, clkmap);
}

static void
handle_box_bld_2(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_MINIMAP_BLD_5, 0, 63, 0, 55,
		ACTION_MINIMAP_BLD_6, 64, 95, 0, 50,
		ACTION_MINIMAP_BLD_7, 0, 63, 64, 95,
		ACTION_MINIMAP_BLD_8, 64, 95, 64, 95,
		ACTION_MINIMAP_BLD_9, 96, 127, 60, 95,
		ACTION_MINIMAP_BLD_10, 32, 63, 104, 139,
		ACTION_MINIMAP_BLD_11, 64, 95, 104, 139,
		ACTION_MINIMAP_BLD_NEXT, 0, 15, 128, 143,
		ACTION_MINIMAP_BLD_EXIT, 112, 127, 128, 143,
		-1
	};

	handle_clickmap(player, x, y, clkmap);
}

static void
handle_box_bld_3(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_MINIMAP_BLD_12, 0, 63, 0, 47,
		ACTION_MINIMAP_BLD_13, 64, 127, 0, 47,
		ACTION_MINIMAP_BLD_14, 0, 31, 56, 89,
		ACTION_MINIMAP_BLD_15, 32, 63, 86, 139,
		ACTION_MINIMAP_BLD_16, 64, 127, 56, 89,
		ACTION_MINIMAP_BLD_17, 64, 111, 100, 139,
		ACTION_MINIMAP_BLD_NEXT, 0, 15, 128, 143,
		ACTION_MINIMAP_BLD_EXIT, 112, 127, 128, 143,
		-1
	};

	handle_clickmap(player, x, y, clkmap);
}

static void
handle_box_bld_4(player_t *player, int x, int y)
{
	const int clkmap[] = {
		ACTION_MINIMAP_BLD_18, 0, 31, 0, 63,
		ACTION_MINIMAP_BLD_19, 32, 63, 0, 63,
		ACTION_MINIMAP_BLD_20, 61, 95, 0, 63,
		ACTION_MINIMAP_BLD_21, 96, 127, 0, 63,
		ACTION_MINIMAP_BLD_22, 16, 63, 95, 135,
		ACTION_MINIMAP_BLD_23, 64, 111, 95, 135,
		ACTION_MINIMAP_BLD_NEXT, 0, 15, 128, 143,
		ACTION_MINIMAP_BLD_EXIT, 112, 127, 128, 143,
		-1
	};

	handle_clickmap(player, x, y, clkmap);
}

static int
popup_box_handle_event_click(popup_box_t *popup, int x, int y)
{
	player_t *player = popup->player;
	x -= 8;
	y -= 8;

	switch (player->clkmap) {
	case BOX_MAP:
	case BOX_MAP_OVERLAY:
		handle_minimap_clk(player, x, y);
		break;
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
	case BOX_START_ATTACK:
	case BOX_START_ATTACK_REDRAW:
		handle_start_attack_click(player, x, y);
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
	case BOX_BLD_1:
		handle_box_bld_1(player, x, y);
		break;
	case BOX_BLD_2:
		handle_box_bld_2(player, x, y);
		break;
	case BOX_BLD_3:
		handle_box_bld_3(player, x, y);
		break;
	case BOX_BLD_4:
		handle_box_bld_4(player, x, y);
		break;
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
		LOGD("popup", "unhandled box: %i", player->clkmap);
		break;
	}

	return 0;
}

static int
popup_box_handle_event(popup_box_t *popup, const gui_event_t *event)
{
	int x = event->x;
	int y = event->y;

	/* Pass event on to minimap */
	if ((popup->player->clkmap == BOX_MAP ||
	     popup->player->clkmap == BOX_MAP_OVERLAY) &&
	    x >= 8 && x < 128+8 && y >= 9 && y < 128+9) {
		gui_event_t minimap_event = {
			.type = event->type,
			.x = event->x - 8,
			.y = event->y - 9,
			.button = event->button
		};
		return gui_object_handle_event((gui_object_t *)&popup->minimap,
					       &minimap_event);
	}

	switch (event->type) {
	case GUI_EVENT_TYPE_CLICK:
		if (event->button == GUI_EVENT_BUTTON_LEFT) {
			return popup_box_handle_event_click(popup, x, y);
		}
	default:
		break;
	}

	return 0;
}

void
popup_box_init(popup_box_t *popup, player_t *player)
{
	gui_container_init((gui_container_t *)popup);
	popup->cont.obj.draw = (gui_draw_func *)popup_box_draw;
	popup->cont.obj.handle_event = (gui_handle_event_func *)popup_box_handle_event;

	popup->player = player;

	/* Initialize minimap */
	minimap_init(&popup->minimap, player);
	gui_object_set_displayed((gui_object_t *)&popup->minimap, 1);
	popup->minimap.obj.parent = (gui_container_t *)popup;
	gui_object_set_size((gui_object_t *)&popup->minimap, 128, 128);
}
