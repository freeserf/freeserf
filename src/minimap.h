/* minimap.h */

#ifndef _MINIMAP_H
#define _MINIMAP_H

#include "gui.h"
#include "map.h"
#include "player.h"

typedef struct {
	gui_object_t obj;
	player_t *player;
	int pointer_x, pointer_y;
	int offset_x, offset_y;
	int scale;
} minimap_t;


void minimap_init(minimap_t *minimap, player_t *player);

void minimap_set_scale(minimap_t *minimap, int scale);

void minimap_move_to_map_pos(minimap_t *minimap, int col, int row);
void minimap_move_by_pixels(minimap_t *minimap, int x, int y);
void minimap_get_current_map_pos(minimap_t *minimap, int *col, int *row);

void minimap_screen_pix_from_map_pos(minimap_t *minimap, map_pos_t pos, int *sx, int *sy);
map_pos_t minimap_map_pos_from_screen_pix(minimap_t *minimap, int x, int y);


#endif /* !_MINIMAP_H */
