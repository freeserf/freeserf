/* gui.c */

#include <stdlib.h>

#include "gui.h"
#include "gfx.h"
#include "sdl-video.h"
#include "list.h"
#include "misc.h"
#include "globals.h"


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
		gui_object_set_redraw((gui_object_t *)obj->parent);
	}
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
	gui_object_set_redraw((gui_object_t *)cont);
}

void
gui_container_init(gui_container_t *cont)
{
	gui_object_init((gui_object_t *)cont);
	cont->set_redraw_child = gui_container_set_redraw_child_default;
}

void
gui_container_set_redraw_child(gui_container_t *cont, gui_object_t *child)
{
	cont->set_redraw_child(cont, child);
}
