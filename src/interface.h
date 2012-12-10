/*
 * interface.h - Top-level GUI interface
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

#ifndef _INTERFACE_H
#define _INTERFACE_H

#include "gui.h"
#include "viewport.h"
#include "panel.h"
#include "list.h"
#include "popup.h"

typedef struct {
	gui_container_t cont;
	gui_object_t *top;
	int redraw_top;
	list_t floats;
} interface_t;


/* TODO Since interface is a generic gui container, either these
   functions should be defined elseware, or interface should
   be aware of the viewport and panel. The functions are
   currently defined in freeserf.c (not in interface.c). */
viewport_t *gui_get_top_viewport();
panel_bar_t *gui_get_panel_bar();
popup_box_t *gui_get_popup_box();
void gui_show_popup_frame(int show);


void interface_init(interface_t *interface);
void interface_set_top(interface_t *interface, gui_object_t *obj);
void interface_add_float(interface_t *interface, gui_object_t *obj,
			 int x, int y, int width, int height);

#endif /* !_INTERFACE_H */
