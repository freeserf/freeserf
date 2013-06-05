/*
 * game-init.h - Game initialization GUI component header
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

#ifndef _GAME_INIT_H
#define _GAME_INIT_H

#include "gui.h"

struct interface;

typedef struct {
	gui_object_t obj;
	struct interface *interface;
	int map_size;
	int game_mission;
} game_init_box_t;

void game_init_box_init(game_init_box_t *box, struct interface *interface);

#endif /* !_GAME_INIT_H */
