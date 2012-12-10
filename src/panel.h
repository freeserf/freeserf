/*
 * panel.h - Panel GUI component
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

#ifndef _PANEL_H
#define _PANEL_H

#include "gui.h"
#include "player.h"
#include "gfx.h"

typedef struct {
	gui_object_t obj;
	player_t *player;
} panel_bar_t;

void panel_bar_init(panel_bar_t *panel, player_t *player);

void panel_bar_activate_button(panel_bar_t *panel, int button);

#endif /* !_PANEL_H */
