/* gui.h */

#ifndef _GUI_H
#define _GUI_H

#include "gfx.h"
#include "list.h"


typedef enum {
	GUI_EVENT_TYPE_CLICK,
	GUI_EVENT_TYPE_DBL_CLICK,
	GUI_EVENT_TYPE_BUTTON_UP,
	GUI_EVENT_TYPE_BUTTON_DOWN,
	GUI_EVENT_TYPE_DRAG_START,
	GUI_EVENT_TYPE_DRAG_MOVE,
	GUI_EVENT_TYPE_DRAG_END
} gui_event_type_t;

typedef enum {
	GUI_EVENT_BUTTON_LEFT = 1,
	GUI_EVENT_BUTTON_MIDDLE,
	GUI_EVENT_BUTTON_RIGHT
} gui_event_button_t;

typedef struct {
	gui_event_type_t type;
	int x, y;
	int button;
} gui_event_t;

typedef struct gui_object gui_object_t;
typedef struct gui_container gui_container_t;

typedef void gui_draw_func(gui_object_t *obj, frame_t *frame);
typedef int gui_handle_event_func(gui_object_t *obj, const gui_event_t *event);
typedef void gui_set_size_func(gui_object_t *obj, int width, int height);

struct gui_object {
	int width, height;
	int displayed;
	int redraw;
	gui_container_t *parent;

	gui_draw_func *draw;
	gui_handle_event_func *handle_event;
	gui_set_size_func *set_size;
};

typedef void gui_set_redraw_child_func(gui_container_t *cont,
				       gui_object_t *child);

struct gui_container {
	gui_object_t obj;

	gui_set_redraw_child_func *set_redraw_child;
};

typedef struct {
	gui_container_t cont;
	gui_object_t *top;
	int redraw_top;
	list_t floats;
} interface_t;


int gui_get_slider_click_value(int x);


void gui_object_init(gui_object_t *obj);
void gui_object_redraw(gui_object_t *obj, frame_t *frame);
int gui_object_handle_event(gui_object_t *obj, const gui_event_t *event);
void gui_object_set_size(gui_object_t *obj, int width, int height);
void gui_object_set_displayed(gui_object_t *obj, int displayed);
void gui_object_set_redraw(gui_object_t *obj);

void gui_container_init(gui_container_t *cont);
void gui_container_set_redraw_child(gui_container_t *cont,
				    gui_object_t *child);

void interface_init(interface_t *interface);
void interface_set_top(interface_t *interface, gui_object_t *obj);
void interface_add_float(interface_t *interface, gui_object_t *obj,
			 int x, int y, int width, int height);

#endif /* !_GUI_H */
