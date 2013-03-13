/*
 * interface.h - Top-level GUI interface
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

#ifndef _INTERFACE_H
#define _INTERFACE_H

#include "gui.h"
#include "viewport.h"
#include "panel.h"
#include "list.h"
#include "popup.h"
#include "player.h"


typedef enum {
	MAP_CURSOR_TYPE_NONE = 0,
	MAP_CURSOR_TYPE_FLAG,
	MAP_CURSOR_TYPE_REMOVABLE_FLAG,
	MAP_CURSOR_TYPE_BUILDING,
	MAP_CURSOR_TYPE_PATH,
	MAP_CURSOR_TYPE_CLEAR_BY_FLAG,
	MAP_CURSOR_TYPE_CLEAR_BY_PATH,
	MAP_CURSOR_TYPE_CLEAR
} map_cursor_type_t;


typedef struct interface interface_t;

struct interface {
	gui_container_t cont;
	gui_object_t *top;
	int redraw_top;
	list_t floats;

	uint32_t *serf_animation_table;

	viewport_t viewport;
	panel_bar_t panel;
	popup_box_t popup;

	map_pos_t map_cursor_pos;
	map_cursor_type_t map_cursor_type;
	panel_btn_t panel_btn_type;

	/* 0 */
	int flags;
	int click;
	int pointer_x_max;
	int pointer_y_max;
	int pointer_x;
	int pointer_y;
	int pointer_x_off;
	int pointer_x_clk;
	int pointer_y_clk;
	/* 10 */
	/* OBSOLETE
	int pointer_x_drag;
	int pointer_y_drag;
	*/
	/* 16 */
	int sfx_queue[4];
	frame_t *frame;
	/* 20 */
	int game_area_cols;
	/* 2E */
	int minimap_advanced;
	/* 30 */
	int bottom_panel_x; /* ADDITION */
	int bottom_panel_y;
	int bottom_panel_width; /* ADDITION */
	int bottom_panel_height; /* ADDITION */
	/* 3E */
	int frame_width;
	int frame_height;
	/* 46 */
	/*int col_game_area;*/ /* OBSOLETE */
	/*int row_game_area;*/ /* OBSOLETE */
	int col_offset;
	int row_offset;
	int map_min_x;
	int map_min_y; /* ADDITION */
	int game_area_rows;
	int map_max_y;
	/* 54 */
	map_tile_t **map_rows;
	/* 5C */
	frame_t *popup_frame;
	/* 60 */
	int panel_btns[5];
	int panel_btns_set[5];
	int panel_btns_x;
	int msg_icon_x;
	/* 70 */
	box_t box;
	box_t clkmap;
	/* OBSOLETE moved to minimap object
	int minimap_row;
	int minimap_col;
	*/
	/* 78 */
	int popup_x;
	int popup_y;
	/* 82 */
	player_t *player;
	int config;
	int msg_flags;
	int map_cursor_col_max;
	/* 8E */
	int map_cursor_col_off;
	int map_y_off;
	/*int **map_serf_rows;*/ /* OBSOLETE */
	int message_box;
	/* 9A */
	int map_x_off;
	/* 9E */
	int right_dbl_time;
	/* A0 */
	int panel_btns_dist;
	/* A4 */
	sprite_loc_t map_cursor_sprites[7];
	int road_length;
	int road_valid_dir;
	uint8_t minimap_flags;
	/* D2 */
	uint16_t last_anim;
	int current_stat_8_mode;
	int current_stat_7_item;
	int pathway_scrolling_threshold;
	/* 1B4 */
	/* Determines what sfx should be played. */
	int water_in_view;
	int trees_in_view;
	/* 1C0 */
	int return_timeout;
	int return_col_game_area;
	int return_row_game_area;
	/* 1E0 */
	int panel_btns_first_x;
	int timer_icon_x;
	/* ... */
};


viewport_t *interface_get_top_viewport(interface_t *interface);
panel_bar_t *interface_get_panel_bar(interface_t *interface);
popup_box_t *interface_get_popup_box(interface_t *interface);


void interface_open_popup(interface_t *interface, int box);
void interface_close_popup(interface_t *interface);

void interface_update_map_cursor_pos(interface_t *interface, map_pos_t pos);

void interface_build_road_begin(interface_t *interface);
void interface_build_road_end(interface_t *interface);

int interface_build_road_segment(interface_t *interface, map_pos_t pos, dir_t dir);
int interface_remove_road_segment(interface_t *interface, map_pos_t pos, dir_t dir);

int interface_build_road(interface_t *interface, map_pos_t pos, dir_t *dirs, uint length);

void interface_demolish_object(interface_t *interface);

void interface_build_flag(interface_t *interface);
void interface_build_building(interface_t *interface, building_type_t type);
void interface_build_castle(interface_t *interface);



void interface_init(interface_t *interface);
void interface_set_top(interface_t *interface, gui_object_t *obj);
void interface_add_float(interface_t *interface, gui_object_t *obj,
			 int x, int y, int width, int height);

#endif /* !_INTERFACE_H */
