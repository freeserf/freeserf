/*
 * interface.c - Top-level GUI interface
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

#include <time.h>

#include "interface.h"
#include "gui.h"
#include "audio.h"
#include "viewport.h"
#include "panel.h"
#include "game-init.h"
#include "game.h"
#include "sdl-video.h"
#include "data.h"
#include "debug.h"
#include "freeserf_endian.h"


typedef struct {
	list_elm_t elm;
	gui_object_t *obj;
	int x, y;
	int redraw;
} interface_float_t;


viewport_t *
interface_get_top_viewport(interface_t *interface)
{
	return &interface->viewport;
}

panel_bar_t *
interface_get_panel_bar(interface_t *interface)
{
	return &interface->panel;
}

popup_box_t *
interface_get_popup_box(interface_t *interface)
{
	return &interface->popup;
}


/* Open popup box */
void
interface_open_popup(interface_t *interface, int box)
{
	interface->popup.box = box;
	gui_object_set_displayed(GUI_OBJECT(&interface->popup), 1);
}

/* Close the current popup. */
void
interface_close_popup(interface_t *interface)
{
	interface->popup.box = 0;
	gui_object_set_displayed(GUI_OBJECT(&interface->popup), 0);
	interface->click &= ~BIT(6);
	interface->panel_btns[2] = PANEL_BTN_MAP;
	interface->panel_btns[3] = PANEL_BTN_STATS;
	interface->panel_btns[4] = PANEL_BTN_SETT;
	interface->click |= BIT(1);
	interface->click |= BIT(2);

	interface_update_map_cursor_pos(interface,
					interface->map_cursor_pos);
}

/* Open box for starting a new game */
void
interface_open_game_init(interface_t *interface)
{
	gui_object_set_displayed(GUI_OBJECT(&interface->init_box), 1);
	gui_object_set_enabled(GUI_OBJECT(&interface->panel), 0);
	gui_object_set_enabled(GUI_OBJECT(&interface->viewport), 0);
}

void
interface_close_game_init(interface_t *interface)
{
	gui_object_set_displayed(GUI_OBJECT(&interface->init_box), 0);
	gui_object_set_enabled(GUI_OBJECT(&interface->panel), 1);
	gui_object_set_enabled(GUI_OBJECT(&interface->viewport), 1);

	interface_update_map_cursor_pos(interface,
					interface->map_cursor_pos);
}

/* Open box for next message in the message queue */
void
interface_open_message(interface_t *interface)
{
	if (interface->player->msg_queue_type[0] == 0 || /* No message */
	    BIT_TEST(interface->click, 7)) { /* Building road */
		sfx_play_clip(SFX_CLICK);
		return;
	} else {
		interface->flags &= ~BIT(6);
		if (!BIT_TEST(interface->msg_flags, 3)) {
			interface->msg_flags |= BIT(4);
			interface->msg_flags |= BIT(3);
			viewport_t *viewport = interface_get_top_viewport(interface);
			map_pos_t pos = viewport_get_current_map_pos(viewport);
			interface->return_col_game_area = MAP_POS_COL(pos);
			interface->return_row_game_area = MAP_POS_ROW(pos);
		}
	}

	int type = interface->player->msg_queue_type[0] & 0x1f;

	if (type == 16) {
		/* TODO */
	}

	int param = (interface->player->msg_queue_type[0] >> 5) & 7;
	interface->notification_box.type = type;
	interface->notification_box.param = param;
	gui_object_set_displayed(GUI_OBJECT(&interface->notification_box), 1);

	if (BIT_TEST(0x8f3fe, type)) {
		/* Move screen to new position */
		map_pos_t new_pos = interface->player->msg_queue_pos[0];

		viewport_t *viewport = interface_get_top_viewport(interface);
		viewport_move_to_map_pos(viewport, new_pos);
		interface_update_map_cursor_pos(interface, new_pos);
	}

	interface->click &= ~BIT(6);
	interface->click &= ~BIT(1);

	/* Move notifications forward in the queue. */
	int i;
	for (i = 1; i < 64 && interface->player->msg_queue_type[i] != 0; i++) {
		interface->player->msg_queue_type[i-1] = interface->player->msg_queue_type[i];
		interface->player->msg_queue_pos[i-1] = interface->player->msg_queue_pos[i];
	}
	interface->player->msg_queue_type[i-1] = 0;

	interface->msg_flags |= BIT(1);
	interface->return_timeout = 60*TICKS_PER_SEC;
	sfx_play_clip(SFX_CLICK);
}

void
interface_return_from_message(interface_t *interface)
{
	if (BIT_TEST(interface->msg_flags, 3)) { /* Return arrow present */
		interface->msg_flags |= BIT(4);
		interface->msg_flags &= ~BIT(3);

		interface->return_timeout = 0;
		viewport_t *viewport = interface_get_top_viewport(interface);
		map_pos_t pos = MAP_POS(interface->return_col_game_area,
					interface->return_row_game_area);
		viewport_move_to_map_pos(viewport, pos);

		if (interface->popup.box == BOX_MESSAGE) interface_close_popup(interface);
		sfx_play_clip(SFX_CLICK);
	}
}


/* Return the cursor type and various related values of a map_pos_t. */
static void
get_map_cursor_type(const player_t *player, map_pos_t pos, panel_btn_t *panel_btn,
		    map_cursor_type_t *cursor_type)
{
	if (game_can_build_castle(pos, player)) {
		*panel_btn = PANEL_BTN_BUILD_CASTLE;
	} else if (game_can_player_build(pos, player) &&
		   map_space_from_obj[MAP_OBJ(pos)] == MAP_SPACE_OPEN &&
		   (game_can_build_flag(MAP_MOVE_DOWN_RIGHT(pos), player) ||
		    MAP_HAS_FLAG(MAP_MOVE_DOWN_RIGHT(pos)))) {
		if (game_can_build_mine(pos)) {
			*panel_btn = PANEL_BTN_BUILD_MINE;
		} else if (game_can_build_large(pos)) {
			*panel_btn = PANEL_BTN_BUILD_LARGE;
		} else if (game_can_build_small(pos)) {
			*panel_btn = PANEL_BTN_BUILD_SMALL;
		} else if (game_can_build_flag(pos, player)) {
			*panel_btn = PANEL_BTN_BUILD_FLAG;
		} else {
			*panel_btn = PANEL_BTN_BUILD_INACTIVE;
		}
	} else if (game_can_build_flag(pos, player)) {
		*panel_btn = PANEL_BTN_BUILD_FLAG;
	} else {
		*panel_btn = PANEL_BTN_BUILD_INACTIVE;
	}

	if (MAP_OBJ(pos) == MAP_OBJ_FLAG &&
	    MAP_OWNER(pos) == player->player_num) {
		if (game_can_demolish_flag(pos, player)) {
			*cursor_type = MAP_CURSOR_TYPE_REMOVABLE_FLAG;
		} else {
			*cursor_type = MAP_CURSOR_TYPE_FLAG;
		}
	} else if (map_space_from_obj[MAP_OBJ(pos)] < MAP_SPACE_FLAG) {
		int paths = MAP_PATHS(pos);
		if (paths == 0) {
			if (MAP_OBJ(MAP_MOVE_DOWN_RIGHT(pos)) == MAP_OBJ_FLAG) {
				*cursor_type = MAP_CURSOR_TYPE_CLEAR_BY_FLAG;
			} else if (MAP_PATHS(MAP_MOVE_DOWN_RIGHT(pos)) == 0) {
				*cursor_type = MAP_CURSOR_TYPE_CLEAR;
			} else {
				*cursor_type = MAP_CURSOR_TYPE_CLEAR_BY_PATH;
			}
		} else if (MAP_OWNER(pos) == player->player_num) {
			*cursor_type = MAP_CURSOR_TYPE_PATH;
		} else {
			*cursor_type = MAP_CURSOR_TYPE_NONE;
		}
	} else if ((MAP_OBJ(pos) == MAP_OBJ_SMALL_BUILDING ||
		    MAP_OBJ(pos) == MAP_OBJ_LARGE_BUILDING) &&
		   MAP_OWNER(pos) == player->player_num) {
		building_t *bld = game_get_building(MAP_OBJ_INDEX(pos));
		if (!BUILDING_IS_BURNING(bld)) {
			*cursor_type = MAP_CURSOR_TYPE_BUILDING;
		} else {
			*cursor_type = MAP_CURSOR_TYPE_NONE;
		}
	} else {
		*cursor_type = MAP_CURSOR_TYPE_NONE;
	}
}


/* Update the interface_t object with the information returned
   in get_map_cursor_type(). */
static void
interface_determine_map_cursor_type(interface_t *interface)
{
	map_pos_t cursor_pos = interface->map_cursor_pos;
	get_map_cursor_type(interface->player, cursor_pos,
			    &interface->panel_btn_type,
			    &interface->map_cursor_type);
}

/* Update the interface_t object with the information returned
   in get_map_cursor_type(). This is sets the appropriate values
   when the player interface is in road construction mode. */
static void
interface_determine_map_cursor_type_road(interface_t *interface)
{
	map_pos_t pos = interface->map_cursor_pos;
	int h = MAP_HEIGHT(pos);
	int valid_dir = 0;
	int paths = 0;
	if (interface->road_length > 0) paths = MAP_PATHS(pos);

	for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
		int sprite = 0;

		if (!BIT_TEST(paths, d)) {
			if (game_road_segment_valid(pos, d)) {
				int h_diff = MAP_HEIGHT(MAP_MOVE(pos, d)) - h;
				sprite = 39 + h_diff; /* height indicators */
				valid_dir |= BIT(d);
			} else {
				sprite = 44; /* striped */
			}
		} else {
			sprite = 45; /* undo */
			valid_dir |= BIT(d);
		}
		interface->map_cursor_sprites[d+1].sprite = sprite;
	}

	interface->road_valid_dir = valid_dir;
}

/* Set the appropriate sprites for the panel buttons and the map cursor. */
static void
interface_update_interface(interface_t *interface)
{
	if (BIT_TEST(interface->click, 7)) { /* Building road */
		interface->panel_btns[0] = PANEL_BTN_BUILD_ROAD_STARRED;
		interface->panel_btns[1] = PANEL_BTN_BUILD_INACTIVE;
	} else {
		switch (interface->map_cursor_type) {
		case MAP_CURSOR_TYPE_NONE:
			interface->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
			if (PLAYER_HAS_CASTLE(interface->player)) {
				interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
			} else {
				interface->panel_btns[1] = PANEL_BTN_GROUND_ANALYSIS;
			}
			interface->map_cursor_sprites[0].sprite = 32;
			interface->map_cursor_sprites[2].sprite = 33;
			break;
		case MAP_CURSOR_TYPE_FLAG:
			interface->panel_btns[0] = PANEL_BTN_BUILD_ROAD;
			interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
			interface->map_cursor_sprites[0].sprite = 51;
			interface->map_cursor_sprites[2].sprite = 33;
			break;
		case MAP_CURSOR_TYPE_REMOVABLE_FLAG:
			interface->panel_btns[0] = PANEL_BTN_BUILD_ROAD;
			interface->panel_btns[1] = PANEL_BTN_DESTROY;
			interface->map_cursor_sprites[0].sprite = 51;
			interface->map_cursor_sprites[2].sprite = 33;
			break;
		case MAP_CURSOR_TYPE_BUILDING:
			interface->panel_btns[0] = interface->panel_btn_type;
			interface->panel_btns[1] = PANEL_BTN_DESTROY;
			interface->map_cursor_sprites[0].sprite = 32;
			interface->map_cursor_sprites[2].sprite = 33;
			break;
		case MAP_CURSOR_TYPE_PATH:
			interface->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
			interface->panel_btns[1] = PANEL_BTN_DESTROY_ROAD;
			interface->map_cursor_sprites[0].sprite = 52;
			interface->map_cursor_sprites[2].sprite = 33;
			if (interface->panel_btn_type != PANEL_BTN_BUILD_INACTIVE) {
				interface->panel_btns[0] = PANEL_BTN_BUILD_FLAG;
				interface->map_cursor_sprites[0].sprite = 47;
			}
			break;
		case MAP_CURSOR_TYPE_CLEAR_BY_FLAG:
			if (interface->panel_btn_type < PANEL_BTN_BUILD_MINE) {
				interface->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
				if (PLAYER_HAS_CASTLE(interface->player)) {
					interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
				} else {
					interface->panel_btns[1] = PANEL_BTN_GROUND_ANALYSIS;
				}
				interface->map_cursor_sprites[0].sprite = 32;
				interface->map_cursor_sprites[2].sprite = 33;
			} else {
				interface->panel_btns[0] = interface->panel_btn_type;
				interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
				interface->map_cursor_sprites[0].sprite = 46 + interface->panel_btn_type;
				interface->map_cursor_sprites[2].sprite = 33;
			}
			break;
		case MAP_CURSOR_TYPE_CLEAR_BY_PATH:
			interface->panel_btns[0] = interface->panel_btn_type;
			interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
			if (interface->panel_btn_type != PANEL_BTN_BUILD_INACTIVE) {
				interface->map_cursor_sprites[0].sprite = 46 + interface->panel_btn_type;
				if (interface->panel_btn_type == PANEL_BTN_BUILD_FLAG) {
					interface->map_cursor_sprites[2].sprite = 33;
				} else {
					interface->map_cursor_sprites[2].sprite = 47;
				}
			} else {
				interface->map_cursor_sprites[0].sprite = 32;
				interface->map_cursor_sprites[2].sprite = 33;
			}
			break;
		case MAP_CURSOR_TYPE_CLEAR:
			interface->panel_btns[0] = interface->panel_btn_type;
			if (PLAYER_HAS_CASTLE(interface->player)) {
				interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
			} else {
				interface->panel_btns[1] = PANEL_BTN_GROUND_ANALYSIS;
			}
			if (interface->panel_btn_type) {
				if (interface->panel_btn_type == PANEL_BTN_BUILD_CASTLE) {
					interface->map_cursor_sprites[0].sprite = 50;
				} else {
					interface->map_cursor_sprites[0].sprite = 46 + interface->panel_btn_type;
				}
				if (interface->panel_btn_type == PANEL_BTN_BUILD_FLAG) {
					interface->map_cursor_sprites[2].sprite = 33;
				} else {
					interface->map_cursor_sprites[2].sprite = 47;
				}
			} else {
				interface->map_cursor_sprites[0].sprite = 32;
				interface->map_cursor_sprites[2].sprite = 33;
			}
			break;
		default:
			NOT_REACHED();
			break;
		}
	}
}

void
interface_update_map_cursor_pos(interface_t *interface, map_pos_t pos)
{
	interface->map_cursor_pos = pos;
	if (BIT_TEST(interface->click, 7)) { /* Building road */
		interface_determine_map_cursor_type_road(interface);
	} else {
		interface_determine_map_cursor_type(interface);
	}
	interface_update_interface(interface);
}


/* Start road construction mode for player interface. */
void
interface_build_road_begin(interface_t *interface)
{
	interface->flags &= ~BIT(6);

	interface_determine_map_cursor_type(interface);

	if (interface->map_cursor_type != MAP_CURSOR_TYPE_FLAG &&
	    interface->map_cursor_type != MAP_CURSOR_TYPE_REMOVABLE_FLAG) {
		interface_update_interface(interface);
		return;
	}

	interface->panel_btns[0] = PANEL_BTN_BUILD_ROAD_STARRED;
	interface->panel_btns[1] = PANEL_BTN_BUILD_INACTIVE;
	interface->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
	interface->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
	interface->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
	interface->click |= BIT(6);
	interface->click |= BIT(7);
	interface->click |= BIT(2);
	interface->road_length = 0;

	interface_update_map_cursor_pos(interface,
					interface->map_cursor_pos);
}

/* End road construction mode for player interface. */
void
interface_build_road_end(interface_t *interface)
{
	interface->click &= ~BIT(6);
	interface->panel_btns[2] = PANEL_BTN_MAP;
	interface->panel_btns[3] = PANEL_BTN_STATS;
	interface->panel_btns[4] = PANEL_BTN_SETT;
	interface->click &= ~BIT(7);
	interface->click |= BIT(2);

	interface->map_cursor_sprites[1].sprite = 33;
	interface->map_cursor_sprites[2].sprite = 33;
	interface->map_cursor_sprites[3].sprite = 33;
	interface->map_cursor_sprites[4].sprite = 33;
	interface->map_cursor_sprites[5].sprite = 33;
	interface->map_cursor_sprites[6].sprite = 33;

	map_tile_t *tiles = game.map.tiles;
	map_pos_t pos = interface->map_cursor_pos;

	for (int i = 0; i < interface->road_length; i++) {
		dir_t backtrack_dir = -1;
		for (dir_t d = 0; d < 6; d++) {
			if (BIT_TEST(tiles[pos].paths, d)) {
				backtrack_dir = d;
				break;
			}
		}

		map_pos_t next_pos = MAP_MOVE(pos, backtrack_dir);

		tiles[pos].paths &= ~BIT(backtrack_dir);
		tiles[next_pos].paths &= ~BIT(DIR_REVERSE(backtrack_dir));
		pos = next_pos;
	}

	/* TODO set_map_redraw(); */
}

/* Connect a road under construction to an existing flag at dest. out_dir is the
   direction from the flag down the new road. */
static int
interface_build_road_connect_flag(interface_t *interface, map_pos_t dest, dir_t out_dir)
{
	if (!MAP_HAS_OWNER(dest) ||
	    MAP_OWNER(dest) != interface->player->player_num ||
	    !MAP_HAS_FLAG(dest)) {
		return -1;
	}

	dir_t in_dir = -1;

	flag_t *dest_flag = game_get_flag(MAP_OBJ_INDEX(dest));

	int paths = BIT(out_dir);
	int test = 0;

	/* Backtrack along path to other flag. Test along the way
	   whether the path is on ground or in water. */
	map_pos_t src = dest;
	for (int i = 0; i < interface->road_length + 1; i++) {
		if (BIT_TEST(paths, DIR_RIGHT)) {
			if (MAP_TYPE_DOWN(src) < 4 &&
			    MAP_TYPE_UP(MAP_MOVE_UP(src)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_LEFT;
			src = MAP_MOVE_RIGHT(src);
		} else if (BIT_TEST(paths, DIR_DOWN_RIGHT)) {
			if (MAP_TYPE_UP(src) < 4 &&
			    MAP_TYPE_DOWN(src) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_UP_LEFT;
			src = MAP_MOVE_DOWN_RIGHT(src);
		} else if (BIT_TEST(paths, DIR_DOWN)) {
			if (MAP_TYPE_UP(src) < 4 &&
			    MAP_TYPE_DOWN(MAP_MOVE_LEFT(src)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_UP;
			src = MAP_MOVE_DOWN(src);
		} else if (BIT_TEST(paths, DIR_LEFT)) {
			if (MAP_TYPE_DOWN(MAP_MOVE_LEFT(src)) < 4 &&
			    MAP_TYPE_UP(MAP_MOVE_UP_LEFT(src)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_RIGHT;
			src = MAP_MOVE_LEFT(src);
		} else if (BIT_TEST(paths, DIR_UP_LEFT)) {
			if (MAP_TYPE_UP(MAP_MOVE_UP_LEFT(src)) < 4 &&
			    MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(src)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_DOWN_RIGHT;
			src = MAP_MOVE_UP_LEFT(src);
		} else if (BIT_TEST(paths, DIR_UP)) {
			if (MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(src)) < 4 &&
			    MAP_TYPE_UP(MAP_MOVE_UP(src)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_DOWN;
			src = MAP_MOVE_UP(src);
		}

		if (!MAP_HAS_OWNER(src) || MAP_OWNER(src) != interface->player->player_num) {
			return -1;
		}

		paths = MAP_PATHS(src) & ~BIT(in_dir);
	}

	/* Bit 0 indicates a ground path, bit 1 indicates
	   water path. Abort if path went through both
	   ground and water. */
	int water_path = 0;
	if (test != BIT(0)) {
		water_path = 1;
		if (test != BIT(1)) return -1;
	}

	/* Check that source also has an existing flag */
	if (!MAP_HAS_FLAG(src)) return -1;

	/* Connect flags */
	flag_t *src_flag = game_get_flag(MAP_OBJ_INDEX(src));

	dest_flag->path_con |= BIT(out_dir);
	dest_flag->endpoint |= BIT(out_dir);
	dest_flag->transporter &= ~BIT(out_dir);

	src_flag->path_con |= BIT(in_dir);
	src_flag->endpoint |= BIT(in_dir);
	src_flag->transporter &= ~BIT(in_dir);

	if (water_path) {
		dest_flag->endpoint &= ~BIT(out_dir);
		src_flag->endpoint &= ~BIT(in_dir);
	}

	dest_flag->other_end_dir[out_dir] = (dest_flag->other_end_dir[out_dir] & 0xc7) | (in_dir << 3);
	src_flag->other_end_dir[in_dir] = (src_flag->other_end_dir[in_dir] & 0xc7) | (out_dir << 3);

	int len = game_get_road_length_value(interface->road_length + 1);

	dest_flag->length[out_dir] = len;
	src_flag->length[in_dir] = len;

	dest_flag->other_endpoint.f[out_dir] = src_flag;
	src_flag->other_endpoint.f[in_dir] = dest_flag;

	return 0;
}

/* Build a single road segment. Return -1 on fail, 0 on successful
   construction, and 1 if this segment completed the path. */
int
interface_build_road_segment(interface_t *interface, map_pos_t pos, dir_t dir)
{
	if (!game_road_segment_valid(pos, dir)) return -1;

	map_pos_t dest = MAP_MOVE(pos, dir);
	dir_t dir_rev = DIR_REVERSE(dir);
	map_tile_t *tiles = game.map.tiles;

	if (MAP_OBJ(dest) == MAP_OBJ_FLAG) {
		/* Existing flag at destination, try to connect. */
		int r = interface_build_road_connect_flag(interface, dest, dir_rev);
		if (r < 0) {
			interface_build_road_end(interface);
			return -1;
		} else {
			interface->map_cursor_pos = dest;
			tiles[pos].paths |= BIT(dir);
			tiles[dest].paths |= BIT(dir_rev);
			interface->road_length = 0;
			interface_build_road_end(interface);
			interface_update_map_cursor_pos(interface, dest);
			return 1;
		}
	} else if (MAP_PATHS(dest) == 0) {
		/* No existing paths at destination, build segment. */
		interface->road_length += 1;
		tiles[pos].paths |= BIT(dir);
		tiles[dest].paths |= BIT(dir_rev);

		interface_update_map_cursor_pos(interface, dest);

		/* TODO Pathway scrolling */

		interface->click |= BIT(2);
	} else {
		/* TODO fast split path and connect on double click */
		interface->click |= BIT(2);
		return -1;
	}

	return 0;
}

int
interface_remove_road_segment(interface_t *interface, map_pos_t pos, dir_t dir)
{
	map_pos_t dest = MAP_MOVE(pos, dir);
	dir_t dir_rev = DIR_REVERSE(dir);
	map_tile_t *tiles = game.map.tiles;

	interface->road_length -= 1;
	tiles[pos].paths &= ~BIT(dir);
	tiles[dest].paths &= ~BIT(dir_rev);

	interface_update_map_cursor_pos(interface, dest);

	/* TODO Pathway scrolling */

	interface->click |= BIT(2);

	return 0;
}

/* Build a complete road from pos with dirs specifying the directions. */
int
interface_build_road(interface_t *interface, map_pos_t pos, dir_t *dirs, uint length)
{
	for (int i = 0; i < length; i++) {
		dir_t dir = dirs[i];
		int r = interface_build_road_segment(interface, pos, dir);
		if (r < 0) {
			/* Backtrack */
			for (int j = i-1; j >= 0; j--) {
				dir_t rev_dir = DIR_REVERSE(dirs[j]);
				interface_remove_road_segment(interface, pos,
							      rev_dir);
				pos = MAP_MOVE(pos, rev_dir);
			}
			return -1;
		} else if (r == 1) {
			return 0;
		}
		pos = MAP_MOVE(pos, dir);
	}

	return -1;
}

void
interface_demolish_object(interface_t *interface)
{
	interface_determine_map_cursor_type(interface);

	map_pos_t cursor_pos = interface->map_cursor_pos;

	if (interface->map_cursor_type == MAP_CURSOR_TYPE_REMOVABLE_FLAG) {
		sfx_play_clip(SFX_CLICK);
		interface->click |= BIT(2);
		game_demolish_flag(cursor_pos, interface->player);
	} else if (interface->map_cursor_type == MAP_CURSOR_TYPE_BUILDING) {
		building_t *building = game_get_building(MAP_OBJ_INDEX(cursor_pos));

		if (BUILDING_IS_DONE(building) &&
		    (BUILDING_TYPE(building) == BUILDING_HUT ||
		     BUILDING_TYPE(building) == BUILDING_TOWER ||
		     BUILDING_TYPE(building) == BUILDING_FORTRESS)) {
			/* TODO */
		}

		sfx_play_clip(SFX_AHHH);
		interface->click |= BIT(2);
		game_demolish_building(cursor_pos, interface->player);
	} else {
		sfx_play_clip(SFX_NOT_ACCEPTED);
		interface_update_interface(interface);
	}
}

/* Build new flag. */
void
interface_build_flag(interface_t *interface)
{
	interface->flags &= ~BIT(7);

	int r = game_build_flag(interface->map_cursor_pos,
				interface->player);
	if (r < 0) {
		sfx_play_clip(SFX_NOT_ACCEPTED);
		return;
	}

	interface->click |= BIT(2);
	interface_update_map_cursor_pos(interface,
					interface->map_cursor_pos);
}

/* Build a new building. */
void
interface_build_building(interface_t *interface, building_type_t type)
{
	int r = game_build_building(interface->map_cursor_pos, type,
				    interface->player);
	if (r < 0) {
		sfx_play_clip(SFX_NOT_ACCEPTED);
		return;
	}

	sfx_play_clip(SFX_ACCEPTED);
	interface->click |= BIT(2);
	interface_close_popup(interface);

	/* Move cursor to flag. */
	map_pos_t flag_pos = MAP_MOVE_DOWN_RIGHT(interface->map_cursor_pos);
	interface_update_map_cursor_pos(interface, flag_pos);
}

/* Build castle. */
void
interface_build_castle(interface_t *interface)
{
	int r = game_build_castle(interface->map_cursor_pos,
				  interface->player);
	if (r < 0) {
		sfx_play_clip(SFX_NOT_ACCEPTED);
		return;
	}

	sfx_play_clip(SFX_ACCEPTED);
	interface->flags &= ~BIT(6);
	interface->click |= BIT(2);
	interface_update_map_cursor_pos(interface,
					interface->map_cursor_pos);
}


static void
update_map_height(map_pos_t pos, interface_t *interface)
{
	viewport_redraw_map_pos(&interface->viewport, pos);
}

static void
interface_draw(interface_t *interface, frame_t *frame)
{
	int redraw_above = interface->cont.obj.redraw;

	/* Undraw cursor */
	sdl_draw_frame(interface->pointer_x-8, interface->pointer_y-8,
		       sdl_get_screen_frame(), 0, 0,
		       &interface->cursor_buffer, 16, 16);

	if (interface->top->displayed &&
	    (interface->redraw_top || redraw_above)) {
		gui_object_redraw(interface->top, frame);
		interface->redraw_top = 0;
		redraw_above = 1;
	}

	list_elm_t *elm;
	list_foreach(&interface->floats, elm) {
		interface_float_t *fl = (interface_float_t *)elm;
		if (fl->obj->displayed &&
		    (fl->redraw || redraw_above)) {
			frame_t float_frame;
			sdl_frame_init(&float_frame,
				       frame->clip.x + fl->x,
				       frame->clip.y + fl->y,
				       fl->obj->width, fl->obj->height, frame);
			gui_object_redraw(fl->obj, &float_frame);
			fl->redraw = 0;
			redraw_above = 1;
		}
	}

	/* Restore cursor buffer */
	sdl_draw_frame(0, 0, &interface->cursor_buffer,
		       interface->pointer_x-8, interface->pointer_y-8,
		       sdl_get_screen_frame(), 16, 16);

	/* Mouse cursor */
	gfx_draw_transp_sprite(interface->pointer_x-8,
			       interface->pointer_y-8,
			       DATA_CURSOR, sdl_get_screen_frame());

}

static int
interface_handle_event(interface_t *interface, const gui_event_t *event)
{
	/* Handle locked cursor */
	if (interface->cursor_lock_target != NULL) {
		if (interface->cursor_lock_target == interface->top) {
			return gui_object_handle_event(interface->top, event);
		} else {
			gui_event_t float_event = {
				.type = event->type,
				.x = event->x,
				.y = event->y,
				.button = event->button
			};
			gui_object_t *obj = interface->cursor_lock_target;
			while (obj->parent != NULL) {
				int x, y;
				int r = gui_container_get_child_position(obj->parent, obj,
									 &x, &y);
				if (r < 0) return -1;

				float_event.x -= x;
				float_event.y -= y;

				obj = GUI_OBJECT(obj->parent);
			}

			if (obj != GUI_OBJECT(interface)) return -1;
			return gui_object_handle_event(interface->cursor_lock_target,
						       &float_event);
		}
	}

	/* Find the corresponding float element if any */
	list_elm_t *elm;
	list_foreach_reverse(&interface->floats, elm) {
		interface_float_t *fl = (interface_float_t *)elm;
		if (fl->obj->displayed &&
		    event->x >= fl->x && event->y >= fl->y &&
		    event->x < fl->x + fl->obj->width &&
		    event->y < fl->y + fl->obj->height) {
			gui_event_t float_event = {
				.type = event->type,
				.x = event->x - fl->x,
				.y = event->y - fl->y,
				.button = event->button
			};
			return gui_object_handle_event(fl->obj, &float_event);
		}
	}

	return gui_object_handle_event(interface->top, event);
}

static void
interface_set_size(interface_t *interface, int width, int height)
{
	interface->cont.obj.width = width;
	interface->cont.obj.height = height;

	interface->pointer_x_max = width;
	interface->pointer_y_max = height;

	interface->game_area_cols = (width >> 4) + 4;
	interface->game_area_rows = (height >> 4) + 4;
	interface->bottom_panel_width = 352;
	interface->bottom_panel_height = 40;
	interface->bottom_panel_x = (width - interface->bottom_panel_width) / 2;
	interface->bottom_panel_y = height - interface->bottom_panel_height;

	interface->frame_width = width;
	interface->frame_height = height;

	interface->col_offset = 0; /* TODO center */
	interface->row_offset = 0; /* TODO center */
	interface->map_min_x = 0;
	interface->map_min_y = 0;
	interface->map_max_y = height + 2*MAP_TILE_HEIGHT;

	interface->panel_btns_x = 64;
	interface->panel_btns_first_x = 64;
	interface->panel_btns_dist = 48;
	interface->msg_icon_x = 40;
	interface->timer_icon_x = 304;

	interface->popup_x = (width - 144) / 2;
	interface->popup_y = (height - 160) / 2;

	interface->map_x_off = 0/*288*/;
	interface->map_y_off = -4/*276*/;
	interface->map_cursor_col_max = 2*interface->game_area_cols + 8/*76*/;
	interface->map_cursor_col_off = 0/*36*/;

	int init_box_width = 360;
	int init_box_height = 174;
	int init_box_x = (width - init_box_width) / 2;
	int init_box_y = (height - init_box_height) / 2;

	int notification_box_width = 200;
	int notification_box_height = 88;
	int notification_box_x = interface->bottom_panel_x + 40;
	int notification_box_y = interface->bottom_panel_y - notification_box_height;

	gui_object_set_size(interface->top, width, height);
	interface->redraw_top = 1;

	/* Reassign position of floats. */
	list_elm_t *elm;
	list_foreach(&interface->floats, elm) {
		interface_float_t *fl = (interface_float_t *)elm;
		if (fl->obj == (gui_object_t *)&interface->popup) {
			fl->x = interface->popup_x;
			fl->y = interface->popup_y;
			fl->redraw = 1;
			gui_object_set_size(fl->obj, 144, 160);
		} else if (fl->obj == (gui_object_t *)&interface->panel) {
			fl->x = interface->bottom_panel_x;
			fl->y = interface->bottom_panel_y;
			fl->redraw = 1;
			gui_object_set_size(fl->obj, interface->bottom_panel_width,
					    interface->bottom_panel_height);
		} else if (fl->obj == (gui_object_t *)&interface->init_box) {
			fl->x = init_box_x;
			fl->y = init_box_y;
			fl->redraw = 1;
			gui_object_set_size(fl->obj, init_box_width,
					    init_box_height);
		} else if (fl->obj == (gui_object_t *)&interface->notification_box) {
			fl->x = notification_box_x;
			fl->y = notification_box_y;
			fl->redraw = 1;
			gui_object_set_size(fl->obj, notification_box_width,
					    notification_box_height);
		}
	}

	gui_object_set_redraw(GUI_OBJECT(interface));
}

static void
interface_set_redraw_child(interface_t *interface, gui_object_t *child)
{
	if (interface->cont.obj.parent != NULL) {
		gui_container_set_redraw_child(interface->cont.obj.parent,
					       GUI_OBJECT(interface));
	}

	if (interface->top == child) {
		interface->redraw_top = 1;
		return;
	}

	list_elm_t *elm;
	list_foreach(&interface->floats, elm) {
		interface_float_t *fl = (interface_float_t *)elm;
		if (fl->obj == child) {
			fl->redraw = 1;
			break;
		}
	}
}

int
interface_get_child_position(interface_t *interface, gui_object_t *child,
			     int *x, int *y)
{
	if (interface->top == child) {
		*x = 0;
		*y = 0;
		return 0;
	}

	list_elm_t *elm;
	list_foreach(&interface->floats, elm) {
		interface_float_t *fl = (interface_float_t *)elm;
		if (fl->obj == child) {
			*x = fl->x;
			*y = fl->y;
			return 0;
		}
	}

	return -1;
}

static void
load_serf_animation_table(interface_t *interface)
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
	interface->serf_animation_table = ((uint32_t *)gfx_get_data_object(DATA_SERF_ANIMATION_TABLE, NULL)) + 1;

	/* Endianess convert from big endian. */
	for (int i = 0; i < 199; i++) {
		interface->serf_animation_table[i] = be32toh(interface->serf_animation_table[i]);
	}
}

void
interface_init(interface_t *interface)
{
	gui_container_init(GUI_CONTAINER(interface));
	interface->cont.obj.draw = (gui_draw_func *)interface_draw;
	interface->cont.obj.handle_event = (gui_handle_event_func *)interface_handle_event;
	interface->cont.obj.set_size = (gui_set_size_func *)interface_set_size;
	interface->cont.set_redraw_child = (gui_set_redraw_child_func *)interface_set_redraw_child;
	interface->cont.get_child_position = (gui_get_child_position_func *)interface_get_child_position;

	interface->top = NULL;
	interface->redraw_top = 0;
	list_init(&interface->floats);

	/* Cursor occlusion buffer */
	sdl_frame_init(&interface->cursor_buffer, 0, 0, 16, 16, NULL);

	load_serf_animation_table(interface);

	/* Viewport */
	viewport_init(&interface->viewport, interface);
	gui_object_set_displayed(GUI_OBJECT(&interface->viewport), 1);

	/* Panel bar */
	panel_bar_init(&interface->panel, interface);
	gui_object_set_displayed(GUI_OBJECT(&interface->panel), 1);

	/* Popup box */
	popup_box_init(&interface->popup, interface);

	/* Add objects to interface container. */
	interface_set_top(interface, GUI_OBJECT(&interface->viewport));

	interface_add_float(interface, GUI_OBJECT(&interface->popup),
			    0, 0, 0, 0);
	interface_add_float(interface, GUI_OBJECT(&interface->panel),
			    0, 0, 0, 0);

	/* Game init box */
	game_init_box_init(&interface->init_box, interface);
	gui_object_set_displayed(GUI_OBJECT(&interface->init_box), 1);
	interface_add_float(interface, GUI_OBJECT(&interface->init_box),
			    0, 0, 0, 0);

	/* Notification box */
	notification_box_init(&interface->notification_box, interface);
	interface_add_float(interface, GUI_OBJECT(&interface->notification_box),
			    0, 0, 0, 0);

	interface->map_cursor_pos = MAP_POS(0, 0);
	interface->map_cursor_type = 0;
	interface->panel_btn_type = 0;

	/* Settings */
	interface->flags = 0;
	interface->config = 0x39;
	interface->msg_flags = 0;
	interface->return_timeout = 0;
	interface->click = 0;
	interface->click |= BIT(1);
	interface->flags |= BIT(4);

	/* OBSOLETE
	interface->col_game_area = 0;
	interface->row_game_area = 0;
	interface->map_rows
	*/

	/*interface->popup_frame = &popup_box_left_frame;*/

	interface->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
	interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
	interface->panel_btns[2] = PANEL_BTN_MAP; /* TODO init to inactive */
	interface->panel_btns[3] = PANEL_BTN_STATS; /* TODO init to inactive */
	interface->panel_btns[4] = PANEL_BTN_SETT; /* TODO init to inactive */

	interface->panel_btns_set[0] = -1;
	interface->panel_btns_set[1] = -1;
	interface->panel_btns_set[2] = -1;
	interface->panel_btns_set[3] = -1;
	interface->panel_btns_set[4] = -1;

	interface->player = game.player[0];
	/*interface->map_serf_rows = game.map_serf_rows_left; OBSOLETE */
	interface->current_stat_8_mode = 0;
	interface->current_stat_7_item = 7;
	interface->pathway_scrolling_threshold = 0;

	interface->map_cursor_sprites[0].sprite = 32;
	interface->map_cursor_sprites[1].sprite = 33;
	interface->map_cursor_sprites[2].sprite = 33;
	interface->map_cursor_sprites[3].sprite = 33;
	interface->map_cursor_sprites[4].sprite = 33;
	interface->map_cursor_sprites[5].sprite = 33;
	interface->map_cursor_sprites[6].sprite = 33;

	/* SVGA settings */

	/* OBSOLETE */
	/*
	interface->pointer_x_drag = 0;
	interface->pointer_y_drag = 0;
	*/

	/* map_serf_rows is OBSOLETE
	int r = interface->game_area_rows;
	interface->map_serf_rows = malloc((r+1)*sizeof(int *) + (r+1)*253*sizeof(int));
	if (interface->map_serf_rows == NULL) abort();
	*/

	/*interface->frame = &svga_normal_frame;*/

	/* Randomness for interface */
	srand(time(NULL));
	interface->random.state[0] = rand();
	interface->random.state[1] = rand();
	interface->random.state[2] = rand();
	random_int(&interface->random);

	interface->last_const_tick = 0;

	/* Listen for updates to the map height */
	game.update_map_height_cb =
		(game_update_map_height_func *)update_map_height;
	game.update_map_height_data = interface;
}

void
interface_set_top(interface_t *interface, gui_object_t *obj)
{
	interface->top = obj;
	obj->parent = GUI_CONTAINER(interface);
	gui_object_set_size(interface->top, interface->cont.obj.width,
			    interface->cont.obj.height);
	interface->redraw_top = 1;
	gui_object_set_redraw(GUI_OBJECT(interface));
}

void
interface_add_float(interface_t *interface, gui_object_t *obj,
		    int x, int y, int width, int height)
{
	interface_float_t *fl = malloc(sizeof(interface_float_t));
	if (fl == NULL) abort();

	/* Store currect location with object. */
	fl->obj = obj;
	fl->x = x;
	fl->y = y;
	fl->redraw = 1;

	obj->parent = GUI_CONTAINER(interface);
	list_append(&interface->floats, (list_elm_t *)fl);
	gui_object_set_size(obj, width, height);
	gui_object_set_redraw(GUI_OBJECT(interface));
}

void
interface_set_cursor(interface_t *interface, int x, int y)
{
	if (x != interface->pointer_x || y != interface->pointer_y) {
		/* Undraw cursor */
		sdl_draw_frame(interface->pointer_x-8, interface->pointer_y-8,
			       sdl_get_screen_frame(), 0, 0, &interface->cursor_buffer, 16, 16);

		/* Update position */
		interface->pointer_x = min(max(0, x), interface->pointer_x_max);
		interface->pointer_y = min(max(0, y), interface->pointer_y_max);

		/* Restore cursor buffer */
		sdl_draw_frame(0, 0, &interface->cursor_buffer,
			       interface->pointer_x-8, interface->pointer_y-8,
			       sdl_get_screen_frame(), 16, 16);
	}
}


/* Called periodically when the game progresses. */
void
interface_update(interface_t *interface)
{
	uint tick_diff = game.const_tick - interface->last_const_tick;
	interface->last_const_tick = game.const_tick;

	/* Clear return arrow after a timeout */
	if (interface->return_timeout < tick_diff) {
		interface->msg_flags |= BIT(4);
		interface->msg_flags &= ~BIT(3);
		interface->return_timeout = 0;
	} else {
		interface->return_timeout -= tick_diff;
	}

	const int msg_category[] = {
		-1, 5, 5, 5, 4, 0, 4, 3, 4, 5,
		5, 5, 4, 4, 4, 4, 0, 0, 0, 0
	};

	/* Handle newly enqueued messages */
	if (PLAYER_HAS_MESSAGE(interface->player)) {
		interface->player->flags &= ~BIT(3);
		while (interface->player->msg_queue_type[0] != 0) {
			int type = interface->player->msg_queue_type[0] & 0x1f;
			if (BIT_TEST(interface->config, msg_category[type])) {
				sfx_play_clip(SFX_MESSAGE);
				interface->msg_flags |= BIT(0);
				break;
			}

			/* Message is ignored. Remove. */
			int i;
			for (i = 1; i < 64 && interface->player->msg_queue_type[i] != 0; i++) {
				interface->player->msg_queue_type[i-1] = interface->player->msg_queue_type[i];
				interface->player->msg_queue_pos[i-1] = interface->player->msg_queue_pos[i];
			}
			interface->player->msg_queue_type[i-1] = 0;
		}
	}

	if (BIT_TEST(interface->msg_flags, 1)) {
		interface->msg_flags &= ~BIT(1);
		while (1) {
			if (interface->player->msg_queue_type[0] == 0) {
				interface->msg_flags &= ~BIT(0);
				break;
			}

			int type = interface->player->msg_queue_type[0] & 0x1f;
			if (BIT_TEST(interface->config, msg_category[type])) break;

			/* Message is ignored. Remove. */
			int i;
			for (i = 1; i < 64 && interface->player->msg_queue_type[i] != 0; i++) {
				interface->player->msg_queue_type[i-1] = interface->player->msg_queue_type[i];
				interface->player->msg_queue_pos[i-1] = interface->player->msg_queue_pos[i];
			}
			interface->player->msg_queue_type[i-1] = 0;
		}
	}

	viewport_update(&interface->viewport);
}
