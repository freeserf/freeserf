/* panel.h */

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
