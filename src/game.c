/* game.c */

/* Functions that concern the game play as a whole,
   instead of any specific game object or across multiple
   different game objects. */

#include <stdlib.h>

#include "map.h"
#include "player.h"
#include "globals.h"
#include "log.h"
#include "debug.h"


/* Pause or unpause the game. */
void
game_pause(int enable)
{
	if (enable) {
		globals.game_speed_save = globals.game_speed;
		globals.game_speed = 0;
	} else {
		globals.game_speed = globals.game_speed_save;
	}

	LOGI("Game speed: %u", globals.game_speed >> 16);
}

/* Generate an estimate of the amount of resources in the ground at map pos.*/
static void
get_resource_estimate(map_pos_t pos, int weight, player_t *player)
{
	if ((MAP_OBJ(pos) == MAP_OBJ_NONE ||
	     MAP_OBJ(pos) >= MAP_OBJ_TREE_0) &&
	     MAP_RES_TYPE(pos) != GROUND_DEPOSIT_NONE) {
		int value = weight*MAP_RES_AMOUNT(pos);

		switch (MAP_RES_TYPE(pos)) {
		case GROUND_DEPOSIT_GOLD:
			player->sett->analysis_goldore += value;
			break;
		case GROUND_DEPOSIT_IRON:
			player->sett->analysis_ironore += value;
			break;
		case GROUND_DEPOSIT_COAL:
			player->sett->analysis_coal += value;
			break;
		case GROUND_DEPOSIT_STONE:
			player->sett->analysis_stone += value;
			break;
		default:
			NOT_REACHED();
			break;
		}
	}
}

#define GROUND_ANALYSIS_RADIUS  25

/* Prepare a ground analysis for player.
   The cursor position is the center of the analysis. */
void
prepare_ground_analysis(player_t *player)
{
	player->sett->analysis_goldore = 0;
	player->sett->analysis_ironore = 0;
	player->sett->analysis_coal = 0;
	player->sett->analysis_stone = 0;

	/* Use cursor position, not viewport position as
	   was used in the original game. */
	map_pos_t pos = MAP_POS(player->sett->map_cursor_col,
				player->sett->map_cursor_row);

	/* Sample the cursor position with maximum weighting. */
	get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS, player);

	/* Move outward in a spiral around the initial pos.
	   The weighting of the samples attenuates linearly
	   with the distance to the center. */
	for (int i = 0; i < GROUND_ANALYSIS_RADIUS-1; i++) {
		pos = MAP_MOVE_RIGHT(pos);

		for (int j = 0; j < i+1; j++) {
			get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, player);
			pos = MAP_MOVE_DOWN(pos);
		}

		for (int j = 0; j < i+1; j++) {
			get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, player);
			pos = MAP_MOVE_LEFT(pos);
		}

		for (int j = 0; j < i+1; j++) {
			get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, player);
			pos = MAP_MOVE_UP_LEFT(pos);
		}

		for (int j = 0; j < i+1; j++) {
			get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, player);
			pos = MAP_MOVE_UP(pos);
		}

		for (int j = 0; j < i+1; j++) {
			get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, player);
			pos = MAP_MOVE_RIGHT(pos);
		}

		for (int j = 0; j < i+1; j++) {
			get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, player);
			pos = MAP_MOVE_DOWN_RIGHT(pos);
		}
	}

	/* Process the samples. */
	player->sett->analysis_goldore >>= 4;
	player->sett->analysis_goldore = min(player->sett->analysis_goldore, 999);

	player->sett->analysis_ironore >>= 4;
	player->sett->analysis_ironore = min(player->sett->analysis_ironore, 999);

	player->sett->analysis_coal >>= 4;
	player->sett->analysis_coal = min(player->sett->analysis_coal, 999);

	player->sett->analysis_stone >>= 4;
	player->sett->analysis_stone = min(player->sett->analysis_stone, 999);
}
