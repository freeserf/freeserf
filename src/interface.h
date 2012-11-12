/* interface.h */

#ifndef _INTERFACE_H
#define _INTERFACE_H

#include "gui.h"
#include "viewport.h"
#include "panel.h"
#include "list.h"

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
void gui_show_popup_frame(int show);


void interface_init(interface_t *interface);
void interface_set_top(interface_t *interface, gui_object_t *obj);
void interface_add_float(interface_t *interface, gui_object_t *obj,
			 int x, int y, int width, int height);

#endif /* !_INTERFACE_H */
