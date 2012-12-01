/* game.h */

#ifndef _GAME_H
#define _GAME_H

#include "player.h"
#include "flag.h"
#include "serf.h"
#include "building.h"
#include "freeserf.h"

#define DEFAULT_GAME_SPEED 0x20000

int game_alloc_flag(flag_t **flag, int *index);
flag_t *game_get_flag(int index);
void game_free_flag(int index);

int game_alloc_building(building_t **building, int *index);
building_t *game_get_building(int index);
void game_free_building(int index);

int game_alloc_inventory(inventory_t **inventory, int *index);
inventory_t *game_get_inventory(int index);
void game_free_inventory(int index);

int game_alloc_serf(serf_t **serf, int *index);
serf_t *game_get_serf(int index);
void game_free_serf(int index);

int game_spawn_serf(player_sett_t *sett, serf_t **serf, inventory_t **inventory, int want_knight);

void game_update();
void game_pause(int enable);

void game_prepare_ground_analysis(player_t *player);
int game_send_geologist(flag_t *dest, int dest_index);

int game_get_road_length_value(int length);

void game_demolish_road(map_pos_t pos);
void game_demolish_flag(map_pos_t pos);
void game_demolish_building(map_pos_t pos);

void game_calculate_military_flag_state(building_t *building);
void game_update_land_ownership(int col, int row);
void game_occupy_enemy_building(building_t *building, int player);

void game_set_inventory_resource_mode(inventory_t *inventory, int mode);
void game_set_inventory_serf_mode(inventory_t *inventory, int mode);

#endif /* !_GAME_H */
