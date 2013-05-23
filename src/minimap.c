/*
 * minimap.c - Minimap GUI component
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

#include "minimap.h"
#include "gui.h"
#include "viewport.h"
#include "interface.h"
#include "sdl-video.h"
#include "game.h"
#include "player.h"
#include "misc.h"


#define MINIMAP_MAX_SCALE  8

static void
draw_minimap_point(minimap_t *minimap, int col, int row, uint8_t color,
		   int density, frame_t *frame)
{
	int map_width = game.map.cols * minimap->scale;
	int map_height = game.map.rows * minimap->scale;

	int mm_y = row*minimap->scale - minimap->offset_y;
	col -= (game.map.rows/2) * (int)(mm_y / map_height);
	mm_y = mm_y % map_height;

	while (mm_y < minimap->obj.height) {
		if (mm_y >= -density) {
			int mm_x = col*minimap->scale -
				(row*minimap->scale)/2 - minimap->offset_x;
			mm_x = mm_x % map_width;
			while (mm_x < minimap->obj.width) {
				if (mm_x >= -density) {
					sdl_fill_rect(mm_x, mm_y, density,
						      density, color, frame);
				}
				mm_x += map_width;
			}
		}
		col -= game.map.rows/2;
		mm_y += map_height;
	}
}

static void
draw_minimap_map(minimap_t *minimap, frame_t *frame)
{
	uint8_t *color_data = game.minimap;
	for (int row = 0; row < game.map.rows; row++) {
		for (int col = 0; col < game.map.cols; col++) {
			uint8_t color = *(color_data++);
			draw_minimap_point(minimap, col, row, color,
					   minimap->scale, frame);
		}
	}
}

static void
draw_minimap_ownership(minimap_t *minimap, int density, frame_t *frame)
{
	const int player_colors[] = {
		64, 72, 68, 76
	};

	for (int row = 0; row < game.map.rows; row++) {
		for (int col = 0; col < game.map.cols; col++) {
			map_pos_t pos = MAP_POS(col, row);
			if (MAP_HAS_OWNER(pos)) {
				int color = player_colors[MAP_OWNER(pos)];
				draw_minimap_point(minimap, col, row, color,
						   density, frame);
			}
		}
	}
}

static void
draw_minimap_roads(minimap_t *minimap, frame_t *frame)
{
	for (int row = 0; row < game.map.rows; row++) {
		for (int col = 0; col < game.map.cols; col++) {
			int pos = MAP_POS(col, row);
			if (MAP_PATHS(pos)) {
				draw_minimap_point(minimap, col, row, 1,
						   minimap->scale, frame);
			}
		}
	}
}

static void
draw_minimap_buildings(minimap_t *minimap, frame_t *frame)
{
	const int player_colors[] = {
		64, 72, 68, 76
	};

	const int building_remap[] = {
		BUILDING_CASTLE,
		BUILDING_STOCK, BUILDING_TOWER, BUILDING_HUT,
		BUILDING_FORTRESS, BUILDING_TOOLMAKER, BUILDING_SAWMILL,
		BUILDING_WEAPONSMITH, BUILDING_STONECUTTER, BUILDING_BOATBUILDER,
		BUILDING_FORESTER, BUILDING_LUMBERJACK, BUILDING_PIGFARM,
		BUILDING_FARM, BUILDING_FISHER, BUILDING_MILL, BUILDING_BUTCHER,
		BUILDING_BAKER, BUILDING_STONEMINE, BUILDING_COALMINE,
		BUILDING_IRONMINE, BUILDING_GOLDMINE, BUILDING_STEELSMELTER,
		BUILDING_GOLDSMELTER
	};

	for (int row = 0; row < game.map.rows; row++) {
		for (int col = 0; col < game.map.cols; col++) {
			int pos = MAP_POS(col, row);
			int obj = MAP_OBJ(pos);
			if (obj > MAP_OBJ_FLAG && obj <= MAP_OBJ_CASTLE) {
				int color = player_colors[MAP_OWNER(pos)];
				if (minimap->interface->minimap_advanced > 0) {
					building_t *bld = game_get_building(MAP_OBJ_INDEX(pos));
					if (BUILDING_TYPE(bld) == building_remap[minimap->interface->minimap_advanced]) {
						draw_minimap_point(minimap, col, row, color,
								   minimap->scale, frame);
					}
				} else {
					draw_minimap_point(minimap, col, row, color,
							   minimap->scale, frame);
				}
			}
		}
	}
}

static void
draw_minimap_traffic(minimap_t *minimap, frame_t *frame)
{
	const int player_colors[] = {
		64, 72, 68, 76
	};

	for (int row = 0; row < game.map.rows; row++) {
		for (int col = 0; col < game.map.cols; col++) {
			int pos = MAP_POS(col, row);
			if (MAP_IDLE_SERF(pos)) {
				int color = player_colors[MAP_OWNER(pos)];
				draw_minimap_point(minimap, col, row, color,
						   minimap->scale, frame);
			}
		}
	}
}

static void
draw_minimap_grid(minimap_t *minimap, frame_t *frame)
{
	for (int y = 0; y < game.map.rows * minimap->scale; y += 2) {
		draw_minimap_point(minimap, 0, y, 47, 1, frame);
		draw_minimap_point(minimap, 0, y+1, 1, 1, frame);
	}

	for (int x = 0; x < game.map.cols * minimap->scale; x += 2) {
		draw_minimap_point(minimap, x, 0, 47, 1, frame);
		draw_minimap_point(minimap, x+1, 0, 1, 1, frame);
	}
}

static void
draw_minimap_rect(minimap_t *minimap, frame_t *frame)
{
	void *sprite = gfx_get_data_object(354, NULL);
	int y = minimap->obj.height/2;
	int x = minimap->obj.width/2;
	sdl_draw_transp_sprite(sprite, x, y, 1, 0, 0, frame);
}

static void
minimap_draw(minimap_t *minimap, frame_t *frame)
{
	interface_t *interface = minimap->interface;

	if (BIT_TEST(interface->minimap_flags, 1)) {
		sdl_fill_rect(0, 0, 128, 128, 1, frame);
		draw_minimap_ownership(minimap, 2, frame);
	} else {
		draw_minimap_map(minimap, frame);
		if (BIT_TEST(interface->minimap_flags, 0)) {
			draw_minimap_ownership(minimap, 1, frame);
		}
	}

	if (BIT_TEST(interface->minimap_flags, 2)) {
		draw_minimap_roads(minimap, frame);
	}

	if (BIT_TEST(interface->minimap_flags, 3)) {
		draw_minimap_buildings(minimap, frame);
	}

	if (BIT_TEST(interface->minimap_flags, 4)) {
		draw_minimap_grid(minimap, frame);
	}

	if (interface->minimap_advanced) {
		draw_minimap_traffic(minimap, frame);
	}

	draw_minimap_rect(minimap, frame);
}

static int
minimap_handle_event_click(minimap_t *minimap, int x, int y)
{
	map_pos_t pos = minimap_map_pos_from_screen_pix(minimap, x, y);
	viewport_move_to_map_pos(interface_get_top_viewport(minimap->interface), pos);

	interface_update_map_cursor_pos(minimap->interface, pos);
	interface_close_popup(minimap->interface);

	return 0;
}

static int
minimap_handle_scroll(minimap_t *minimap, int up)
{
	int scale = 0;
	if (up) scale = minimap->scale + 1;
	else scale = minimap->scale - 1;

	minimap_set_scale(minimap, clamp(1, scale, MINIMAP_MAX_SCALE));
	return 0;
}

static int
minimap_handle_drag(minimap_t *minimap, int x, int y,
		    gui_event_button_t button)
{
	if (button == GUI_EVENT_BUTTON_RIGHT) {
		int dx = x - minimap->pointer_x;
		int dy = y - minimap->pointer_y;
		if (dx != 0 || dy != 0) {
			minimap_move_by_pixels(minimap, dx, dy);
			SDL_WarpMouse(minimap->interface->pointer_x,
				      minimap->interface->pointer_y);
		}
	}

	return 0;
}

static int
minimap_handle_event(minimap_t *minimap, const gui_event_t *event)
{
	int x = event->x;
	int y = event->y;

	switch (event->type) {
	case GUI_EVENT_TYPE_CLICK:
		if (event->button == GUI_EVENT_BUTTON_LEFT) {
			return minimap_handle_event_click(minimap, x, y);
		}
		break;
	case GUI_EVENT_TYPE_BUTTON_UP:
		if (event->button == 4 || event->button == 5) {
			return minimap_handle_scroll(minimap, event->button == 4);
		}
		break;
	case GUI_EVENT_TYPE_DRAG_MOVE:
		return minimap_handle_drag(minimap, x, y,
					   event->button);
	case GUI_EVENT_TYPE_DRAG_START:
		minimap->interface->cursor_lock_target = (gui_object_t *)minimap;
		minimap->pointer_x = x;
		minimap->pointer_y = y;
		return 0;
	case GUI_EVENT_TYPE_DRAG_END:
		minimap->interface->cursor_lock_target = NULL;
		return 0;
	default:
		break;
	}

	return 0;
}

void
minimap_init(minimap_t *minimap, interface_t *interface)
{
	gui_object_init((gui_object_t *)minimap);
	minimap->obj.draw = (gui_draw_func *)minimap_draw;
	minimap->obj.handle_event = (gui_handle_event_func *)minimap_handle_event;

	minimap->interface = interface;
	minimap->offset_x = 0;
	minimap->offset_y = 0;
	minimap->scale = 1;
}

/* Set the scale of the map (zoom). Must be positive. */
void
minimap_set_scale(minimap_t *minimap, int scale)
{
	map_pos_t pos = minimap_get_current_map_pos(minimap);
	minimap->scale = scale;
	minimap_move_to_map_pos(minimap, pos);

	gui_object_set_redraw((gui_object_t *)minimap);
}

void
minimap_screen_pix_from_map_pix(minimap_t *minimap, int mx, int my, int *sx, int *sy)
{
	int width = game.map.cols * minimap->scale;
	int height = game.map.rows * minimap->scale;

	*sx = mx - minimap->offset_x;
	*sy = my - minimap->offset_y;

	while (*sy < 0) {
		*sx -= height/2;
		*sy += height;
	}

	while (*sy >= height) {
		*sx += height/2;
		*sy -= height;
	}

	while (*sx < 0) *sx += width;
	while (*sx >= width) *sx -= width;
}

void
minimap_map_pix_from_map_coord(minimap_t *minimap, map_pos_t pos, int *mx, int *my)
{
	int width = game.map.cols * minimap->scale;
	int height = game.map.rows * minimap->scale;

	*mx = minimap->scale*MAP_POS_COL(pos) - (minimap->scale*MAP_POS_ROW(pos))/2;
	*my = minimap->scale*MAP_POS_ROW(pos);

	if (*my < 0) {
		*mx -= height/2;
		*my += height;
	}

	if (*mx < 0) *mx += width;
	else if (*mx >= width) *mx -= width;
}

map_pos_t
minimap_map_pos_from_screen_pix(minimap_t *minimap, int x, int y)
{
	int mx = x + minimap->offset_x;
	int my = y + minimap->offset_y;

	int col = ((my/2 + mx)/minimap->scale) & game.map.col_mask;
	int row = (my/minimap->scale) & game.map.row_mask;

	return MAP_POS(col, row);
}

map_pos_t
minimap_get_current_map_pos(minimap_t *minimap)
{
	return minimap_map_pos_from_screen_pix(minimap,
					       minimap->obj.width/2,
					       minimap->obj.height/2);
}

void
minimap_move_to_map_pos(minimap_t *minimap, map_pos_t pos)
{
	int mx, my;
	minimap_map_pix_from_map_coord(minimap, pos, &mx, &my);

	int map_width = game.map.cols*minimap->scale;
	int map_height = game.map.rows*minimap->scale;

	/* Center view */
	mx -= minimap->obj.width/2;
	my -= minimap->obj.height/2;

	if (my < 0) {
		mx -= map_height/2;
		my += map_height;
	}

	if (mx < 0) mx += map_width;
	else if (mx >= map_width) mx -= map_width;

	minimap->offset_x = mx;
	minimap->offset_y = my;

	gui_object_set_redraw((gui_object_t *)minimap);
}

void
minimap_move_by_pixels(minimap_t *minimap, int dx, int dy)
{
	int width = game.map.cols * minimap->scale;
	int height = game.map.rows * minimap->scale;

	minimap->offset_x += dx;
	minimap->offset_y += dy;

	if (minimap->offset_y < 0) {
		minimap->offset_y += height;
		minimap->offset_x -= height/2;
	} else if (minimap->offset_y >= height) {
		minimap->offset_y -= height;
		minimap->offset_x += height/2;
	}

	if (minimap->offset_x >= width) minimap->offset_x -= width;
	else if (minimap->offset_x < 0) minimap->offset_x += width;

	gui_object_set_redraw((gui_object_t *)minimap);
}
