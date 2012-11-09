/* game.h */

#ifndef _GAME_H
#define _GAME_H

#include "player.h"

#define DEFAULT_GAME_SPEED 0x20000

void game_pause(int enable);

void prepare_ground_analysis(player_t *player);

#endif /* !_GAME_H */
