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
#include <assert.h>

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
	interface->popup.box = (box_t)box;
	gui_object_set_displayed(GUI_OBJECT(&interface->popup), 1);
}

/* Close the current popup. */
void
interface_close_popup(interface_t *interface)
{
	interface->popup.box = (box_t)0;
	gui_object_set_displayed(GUI_OBJECT(&interface->popup), 0);
	interface->panel_btns[2] = PANEL_BTN_MAP;
	interface->panel_btns[3] = PANEL_BTN_STATS;
	interface->panel_btns[4] = PANEL_BTN_SETT;

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
	if (interface->player->msg_queue_type[0] == 0) {
		sfx_play_clip(SFX_CLICK);
		return;
	} else if (!BIT_TEST(interface->msg_flags, 3)) {
		interface->msg_flags |= BIT(4);
		interface->msg_flags |= BIT(3);
		viewport_t *viewport = interface_get_top_viewport(interface);
		map_pos_t pos = viewport_get_current_map_pos(viewport);
		interface->return_pos = pos;
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
		viewport_move_to_map_pos(viewport, interface->return_pos);

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
	} else if (!MAP_HAS_BUILDING(pos) && !MAP_HAS_FLAG(pos)) {
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
	int length = interface->building_road_length;

	for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
		int sprite = 0;

		if (length > 0 && interface->building_road_dirs[length-1] == DIR_REVERSE(d)) {
			sprite = 45; /* undo */
			valid_dir |= BIT(d);
		} else if (game_road_segment_valid(pos, (dir_t)d)) {
			/* Check that road does not cross itself. */
			map_pos_t road_pos = interface->building_road_source;
			int crossing_self = 0;
			for (int i = 0; i < length; i++) {
				road_pos = MAP_MOVE(road_pos, interface->building_road_dirs[i]);
				if (road_pos == MAP_MOVE(pos, d)) {
					crossing_self = 1;
					break;
				}
			}

			if (!crossing_self) {
				int h_diff = MAP_HEIGHT(MAP_MOVE(pos, d)) - h;
				sprite = 39 + h_diff; /* height indicators */
				valid_dir |= BIT(d);
			} else {
				sprite = 44;
			}
		} else {
			sprite = 44; /* striped */
		}
		interface->map_cursor_sprites[d+1].sprite = sprite;
	}

	interface->building_road_valid_dir = valid_dir;
}

/* Set the appropriate sprites for the panel buttons and the map cursor. */
static void
interface_update_interface(interface_t *interface)
{
	if (interface->building_road) {
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
interface_set_player(interface_t *interface, uint player)
{
	assert(PLAYER_IS_ACTIVE(game.player[player]));
	interface->player = game.player[player];

	/* Move viewport to initial position */
	map_pos_t init_pos = MAP_POS(0,0);
	if (interface->player->castle_flag != 0) {
		flag_t *flag = game_get_flag(interface->player->castle_flag);
		init_pos = MAP_MOVE_UP_LEFT(flag->pos);
	}

	interface_update_map_cursor_pos(interface, init_pos);
	viewport_move_to_map_pos(&interface->viewport, interface->map_cursor_pos);
}

void
interface_update_map_cursor_pos(interface_t *interface, map_pos_t pos)
{
	interface->map_cursor_pos = pos;
	if (interface->building_road) {
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

	interface->building_road = 1;
	interface->building_road_length = 0;
	interface->building_road_source = interface->map_cursor_pos;

	interface_update_map_cursor_pos(interface,
					interface->map_cursor_pos);
}

/* End road construction mode for player interface. */
void
interface_build_road_end(interface_t *interface)
{
	interface->panel_btns[2] = PANEL_BTN_MAP;
	interface->panel_btns[3] = PANEL_BTN_STATS;
	interface->panel_btns[4] = PANEL_BTN_SETT;

	interface->map_cursor_sprites[1].sprite = 33;
	interface->map_cursor_sprites[2].sprite = 33;
	interface->map_cursor_sprites[3].sprite = 33;
	interface->map_cursor_sprites[4].sprite = 33;
	interface->map_cursor_sprites[5].sprite = 33;
	interface->map_cursor_sprites[6].sprite = 33;

	interface->building_road = 0;
	interface_update_map_cursor_pos(interface,
					interface->map_cursor_pos);
}

/* Build a single road segment. Return -1 on fail, 0 on successful
   construction, and 1 if this segment completed the path. */
int
interface_build_road_segment(interface_t *interface, dir_t dir)
{
	if (interface->building_road_length+1 >= MAX_ROAD_LENGTH) {
		/* Max length reached */
		return -1;
	}

	interface->building_road_dirs[interface->building_road_length] = dir;
	interface->building_road_length += 1;

	map_pos_t dest;
	int r = game_can_build_road(interface->building_road_source,
				    interface->building_road_dirs,
				    interface->building_road_length,
				    interface->player, &dest, NULL);
	if (!r) {
		/* Invalid construction, undo. */
		return interface_remove_road_segment(interface);
	}

	if (MAP_OBJ(dest) == MAP_OBJ_FLAG) {
		/* Existing flag at destination, try to connect. */
		int r = game_build_road(interface->building_road_source,
					interface->building_road_dirs,
					interface->building_road_length,
					interface->player);
		if (r < 0) {
			interface_build_road_end(interface);
			return -1;
		} else {
			interface_build_road_end(interface);
			interface_update_map_cursor_pos(interface, dest);
			return 1;
		}
	} else if (MAP_PATHS(dest) == 0) {
		/* No existing paths at destination, build segment. */
		interface_update_map_cursor_pos(interface, dest);

		/* TODO Pathway scrolling */
	} else {
		/* TODO fast split path and connect on double click */
		return -1;
	}

	return 0;
}

int
interface_remove_road_segment(interface_t *interface)
{
	interface->building_road_length -= 1;

	map_pos_t dest;
	int r = game_can_build_road(interface->building_road_source,
				    interface->building_road_dirs,
				    interface->building_road_length,
				    interface->player, &dest, NULL);
	if (!r) {
		/* Road construction is no longer valid, abort. */
		interface_build_road_end(interface);
		return -1;
	}

	interface_update_map_cursor_pos(interface, dest);

	/* TODO Pathway scrolling */

	return 0;
}

/* Extend currently constructed road with an array of directions. */
int
interface_extend_road(interface_t *interface, dir_t *dirs, uint length)
{
	for (uint i = 0; i < length; i++) {
		dir_t dir = dirs[i];
		int r = interface_build_road_segment(interface, dir);
		if (r < 0) {
			/* Backtrack */
			for (int j = i-1; j >= 0; j--) {
				interface_remove_road_segment(interface);
			}
			return -1;
		} else if (r == 1) {
			return 1;
		}
	}

	return 0;
}

void
interface_demolish_object(interface_t *interface)
{
	interface_determine_map_cursor_type(interface);

	map_pos_t cursor_pos = interface->map_cursor_pos;

	if (interface->map_cursor_type == MAP_CURSOR_TYPE_REMOVABLE_FLAG) {
		sfx_play_clip(SFX_CLICK);
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
	int r = game_build_flag(interface->map_cursor_pos,
				interface->player);
	if (r < 0) {
		sfx_play_clip(SFX_NOT_ACCEPTED);
		return;
	}

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
			gui_event_t float_event;
			
			float_event.type = event->type;
			float_event.x = event->x;
			float_event.y = event->y;
			float_event.button = event->button;
			
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
			gui_event_t float_event;
			
			float_event.type = event->type;
			float_event.x = event->x - fl->x;
			float_event.y = event->y - fl->y;
			float_event.button = event->button;

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

	int panel_width = 352;
	int panel_height = 40;
	int panel_x = (width - panel_width) / 2;
	int panel_y = height - panel_height;

	int popup_width = 144;
	int popup_height = 160;
	int popup_x = (width - popup_width) / 2;
	int popup_y = (height - popup_height) / 2;

	int init_box_width = 360;
	int init_box_height = 174;
	int init_box_x = (width - init_box_width) / 2;
	int init_box_y = (height - init_box_height) / 2;

	int notification_box_width = 200;
	int notification_box_height = 88;
	int notification_box_x = panel_x + 40;
	int notification_box_y = panel_y - notification_box_height;

	gui_object_set_size(interface->top, width, height);
	interface->redraw_top = 1;

	/* Reassign position of floats. */
	list_elm_t *elm;
	list_foreach(&interface->floats, elm) {
		interface_float_t *fl = (interface_float_t *)elm;
		if (fl->obj == GUI_OBJECT(&interface->popup)) {
			fl->x = popup_x;
			fl->y = popup_y;
			fl->redraw = 1;
			gui_object_set_size(fl->obj, popup_width, popup_height);
		} else if (fl->obj == GUI_OBJECT(&interface->panel)) {
			fl->x = panel_x;
			fl->y = panel_y;
			fl->redraw = 1;
			gui_object_set_size(fl->obj, panel_width,
					    panel_height);
		} else if (fl->obj == GUI_OBJECT(&interface->init_box)) {
			fl->x = init_box_x;
			fl->y = init_box_y;
			fl->redraw = 1;
			gui_object_set_size(fl->obj, init_box_width,
					    init_box_height);
		} else if (fl->obj == GUI_OBJECT(&interface->notification_box)) {
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
	interface->map_cursor_type = (map_cursor_type_t)0;
	interface->panel_btn_type = (panel_btn_t)0;

	interface->building_road = 0;

	interface->player = NULL;

	/* Settings */
	interface->config = 0x39;
	interface->msg_flags = 0;
	interface->return_timeout = 0;

	interface->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
	interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
	interface->panel_btns[2] = PANEL_BTN_MAP;
	interface->panel_btns[3] = PANEL_BTN_STATS;
	interface->panel_btns[4] = PANEL_BTN_SETT;

	interface->current_stat_8_mode = 0;
	interface->current_stat_7_item = 7;

	interface->map_cursor_sprites[0].sprite = 32;
	interface->map_cursor_sprites[1].sprite = 33;
	interface->map_cursor_sprites[2].sprite = 33;
	interface->map_cursor_sprites[3].sprite = 33;
	interface->map_cursor_sprites[4].sprite = 33;
	interface->map_cursor_sprites[5].sprite = 33;
	interface->map_cursor_sprites[6].sprite = 33;

	/* Randomness for interface */
	srand((unsigned int)time(NULL));
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
	interface_float_t *fl = (interface_float_t *)malloc(sizeof(interface_float_t));
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
		interface->pointer_x = min(max(0, x), GUI_OBJECT(interface)->width);
		interface->pointer_y = min(max(0, y), GUI_OBJECT(interface)->height);
		gui_object_set_redraw(GUI_OBJECT(interface));

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

	player_t *player = interface->player;

	/* Update timers */
	for (int i = 0; i < player->timers_count; i++) {
		player->timers[i].timeout -= tick_diff;
		if (player->timers[i].timeout < 0) {
			/* Timer has expired. */
			/* TODO box (+ pos) timer */
			player_add_notification(player, 5,
						player->timers[i].pos);

			/* Delete timer from list. */
			player->timers_count -= 1;
			for (int j = i; j < player->timers_count; j++) {
				player->timers[j].timeout = player->timers[j+1].timeout;
				player->timers[j].pos = player->timers[j+1].pos;
			}
		}
	}

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
	if (PLAYER_HAS_MESSAGE(player)) {
		player->flags &= ~BIT(3);
		while (player->msg_queue_type[0] != 0) {
			int type = player->msg_queue_type[0] & 0x1f;
			if (BIT_TEST(interface->config, msg_category[type])) {
				sfx_play_clip(SFX_MESSAGE);
				interface->msg_flags |= BIT(0);
				break;
			}

			/* Message is ignored. Remove. */
			int i;
			for (i = 1; i < 64 && player->msg_queue_type[i] != 0; i++) {
				player->msg_queue_type[i-1] = player->msg_queue_type[i];
				player->msg_queue_pos[i-1] = player->msg_queue_pos[i];
			}
			player->msg_queue_type[i-1] = 0;
		}
	}

	if (BIT_TEST(interface->msg_flags, 1)) {
		interface->msg_flags &= ~BIT(1);
		while (1) {
			if (player->msg_queue_type[0] == 0) {
				interface->msg_flags &= ~BIT(0);
				break;
			}

			int type = player->msg_queue_type[0] & 0x1f;
			if (BIT_TEST(interface->config, msg_category[type])) break;

			/* Message is ignored. Remove. */
			int i;
			for (i = 1; i < 64 && player->msg_queue_type[i] != 0; i++) {
				player->msg_queue_type[i-1] = player->msg_queue_type[i];
				player->msg_queue_pos[i-1] = player->msg_queue_pos[i];
			}
			player->msg_queue_type[i-1] = 0;
		}
	}

	viewport_update(&interface->viewport);
}
