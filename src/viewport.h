/*
 * viewport.h - Viewport GUI component
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

#ifndef _VIEWPORT_H
#define _VIEWPORT_H

#include "freeserf.h"
#include "gfx.h"
#include "gui.h"
#include "map.h"


typedef enum {
	VIEWPORT_LAYER_LANDSCAPE = 1<<0,
	VIEWPORT_LAYER_PATHS = 1<<1,
	VIEWPORT_LAYER_OBJECTS = 1<<2,
	VIEWPORT_LAYER_SERFS = 1<<3,
	VIEWPORT_LAYER_CURSOR = 1<<4,
	VIEWPORT_LAYER_ALL = (1<<5)-1,

	VIEWPORT_LAYER_GRID = 1<<5
} viewport_layer_t;

typedef struct {
	gui_object_t obj;
	int offset_x, offset_y;
	viewport_layer_t layers;
	struct interface *interface;
	uint last_tick;
} viewport_t;


void viewport_init(viewport_t *viewport, struct interface *interface);

void viewport_move_to_map_pos(viewport_t *viewport, map_pos_t pos);
void viewport_move_by_pixels(viewport_t *viewport, int x, int y);
map_pos_t viewport_get_current_map_pos(viewport_t *viewport);

void viewport_screen_pix_from_map_pix(viewport_t *viewport, int mx, int my, int *sx, int *sy);
void viewport_map_pix_from_map_coord(viewport_t *viewport, map_pos_t pos, int h, int *mx, int *my);
map_pos_t viewport_map_pos_from_screen_pix(viewport_t *viewport, int x, int y);

void viewport_redraw_map_pos(map_pos_t pos);

void viewport_update(viewport_t *viewport);


#endif /* ! _VIEWPORT_H */
