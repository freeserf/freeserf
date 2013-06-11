/*
 * minimap.h - Minimap GUI component
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

#ifndef _MINIMAP_H
#define _MINIMAP_H

#include "gui.h"
#include "map.h"

typedef struct {
	gui_object_t obj;
	struct interface *interface;

	int pointer_x, pointer_y;
	int offset_x, offset_y;
	int scale;

	int advanced;
	int flags;
} minimap_t;


void minimap_init(minimap_t *minimap, struct interface *interface);

void minimap_set_scale(minimap_t *minimap, int scale);

void minimap_move_to_map_pos(minimap_t *minimap, map_pos_t pos);
void minimap_move_by_pixels(minimap_t *minimap, int x, int y);
map_pos_t minimap_get_current_map_pos(minimap_t *minimap);

void minimap_screen_pix_from_map_pos(minimap_t *minimap, map_pos_t pos, int *sx, int *sy);
map_pos_t minimap_map_pos_from_screen_pix(minimap_t *minimap, int x, int y);


#endif /* !_MINIMAP_H */
