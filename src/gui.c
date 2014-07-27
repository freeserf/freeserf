/*
 * gui.c - Base functions for the GUI hierarchy
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

#include "gui.h"
#include "misc.h"


/* Get the resulting value from a click on a slider bar. */
int
gui_get_slider_click_value(int x)
{
	return 1310 * clamp(0, x - 7, 50);
}


static void
gui_object_set_size_default(gui_object_t *obj, int width, int height)
{
	obj->width = width;
	obj->height = height;
}

void
gui_object_init(gui_object_t *obj)
{
	obj->width = 0;
	obj->height = 0;
	obj->displayed = 0;
	obj->enabled = 1;
	obj->redraw = 0;
	obj->parent = NULL;

	obj->set_size = gui_object_set_size_default;
}

void
gui_object_redraw(gui_object_t *obj, frame_t *frame)
{
	obj->draw(obj, frame);
	obj->redraw = 0;
}

int
gui_object_handle_event(gui_object_t *obj, const gui_event_t *event)
{
	if (!obj->enabled) return 0;
	return obj->handle_event(obj, event);
}

void
gui_object_set_size(gui_object_t *obj, int width, int height)
{
	obj->set_size(obj, width, height);
}

void
gui_object_set_displayed(gui_object_t *obj, int displayed)
{
	obj->displayed = displayed;
	if (displayed) {
		gui_object_set_redraw(obj);
	} else if (obj->parent != NULL) {
		gui_object_set_redraw(GUI_OBJECT(obj->parent));
	}
}

void
gui_object_set_enabled(gui_object_t *obj, int enabled)
{
	obj->enabled = enabled;
}

void
gui_object_set_redraw(gui_object_t *obj)
{
	obj->redraw = 1;
	if (obj->parent != NULL) {
		gui_container_set_redraw_child(obj->parent, obj);
	}
}

static void
gui_container_set_redraw_child_default(gui_container_t *cont, gui_object_t *child)
{
	gui_object_set_redraw(GUI_OBJECT(cont));
}

void
gui_container_init(gui_container_t *cont)
{
	gui_object_init(GUI_OBJECT(cont));
	cont->set_redraw_child = gui_container_set_redraw_child_default;
}

void
gui_container_set_redraw_child(gui_container_t *cont, gui_object_t *child)
{
	cont->set_redraw_child(cont, child);
}

int
gui_container_get_child_position(gui_container_t *cont, gui_object_t *child,
				 int *x, int *y)
{
	return cont->get_child_position(cont, child, x, y);
}
