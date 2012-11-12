/* interface.c */

#include "interface.h"
#include "gui.h"
#include "viewport.h"
#include "panel.h"
#include "sdl-video.h"


typedef struct {
	list_elm_t elm;
	gui_object_t *obj;
	int x, y;
	int redraw;
} interface_float_t;


static void
interface_draw(interface_t *interface, frame_t *frame)
{
	int redraw_above = interface->cont.obj.redraw;

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
}

static int
interface_handle_event(interface_t *interface, const gui_event_t *event)
{
	LOGV("interface", "Event: %i, %i, %i, %i.", event->type, event->button, event->x, event->y);

	list_elm_t *elm;
	list_foreach_reverse(&interface->floats, elm) {
		interface_float_t *fl = (interface_float_t *)elm;
		if (fl->obj->displayed &&
		    event->x >= fl->x && event->y >= fl->y &&
		    event->x < fl->x + fl->obj->width &&
		    event->y < fl->y + fl->obj->height) {
			gui_event_t float_event = {
				.type = event->type,
				.x = event->x - fl->x,
				.y = event->y - fl->y,
				.button = event->button
			};
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
}

static void
interface_set_redraw_child(interface_t *interface, gui_object_t *child)
{
	if (interface->cont.obj.parent != NULL) {
		gui_container_set_redraw_child(interface->cont.obj.parent,
					       (gui_object_t *)interface);
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

void
interface_init(interface_t *interface)
{
	gui_container_init((gui_container_t *)interface);
	interface->cont.obj.draw = (gui_draw_func *)interface_draw;
	interface->cont.obj.handle_event = (gui_handle_event_func *)interface_handle_event;
	interface->cont.obj.set_size = (gui_set_size_func *)interface_set_size;
	interface->cont.set_redraw_child = (gui_set_redraw_child_func *)interface_set_redraw_child;

	interface->top = NULL;
	interface->redraw_top = 0;
	list_init(&interface->floats);
}

void
interface_set_top(interface_t *interface, gui_object_t *obj)
{
	interface->top = obj;
	obj->parent = (gui_container_t *)interface;
	gui_object_set_size(interface->top, interface->cont.obj.width,
			    interface->cont.obj.height);
	interface->redraw_top = 1;
	gui_object_set_redraw((gui_object_t *)interface);
}

void
interface_add_float(interface_t *interface, gui_object_t *obj,
		    int x, int y, int width, int height)
{
	interface_float_t *fl = malloc(sizeof(interface_float_t));
	if (fl == NULL) abort();

	/* Store currect location with object. */
	fl->obj = obj;
	fl->x = x;
	fl->y = y;
	fl->redraw = 1;

	obj->parent = (gui_container_t *)interface;
	list_append(&interface->floats, (list_elm_t *)fl);
	gui_object_set_size(obj, width, height);
	gui_object_set_redraw((gui_object_t *)interface);
}
