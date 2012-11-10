/* popup.h */

#ifndef _POPUP_H
#define _POPUP_H

#include "gui.h"
#include "player.h"


typedef struct {
	gui_container_t cont;
	player_t *player;
} popup_box_t;

void popup_box_init(popup_box_t *popup, player_t *player);


#endif /* !_POPUP_H */
