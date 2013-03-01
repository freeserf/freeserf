/*
 * game.h - Gameplay related functions
 *
 * Copyright (C) 2012  Jon Lund Steffensen <jonlst@gmail.com>
 *
 * This file is part of freeserf.
 *
 * freeserf is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * freeserf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with freeserf.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GAME_H
#define _GAME_H

#include "player.h"
#include "flag.h"
#include "serf.h"
#include "building.h"
#include "map.h"
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

int game_road_segment_valid(map_pos_t pos, dir_t dir);
int game_get_road_length_value(int length);

void game_build_flag(map_pos_t pos, player_sett_t *sett);
void game_build_building(map_pos_t pos, building_type_t type, player_sett_t *sett);
void game_build_castle(map_pos_t pos, player_sett_t *sett);

void game_demolish_road(map_pos_t pos);
void game_demolish_flag(map_pos_t pos);
void game_demolish_building(map_pos_t pos);

void game_calculate_military_flag_state(building_t *building);
void game_update_land_ownership(map_pos_t pos);
void game_occupy_enemy_building(building_t *building, int player);

void game_set_inventory_resource_mode(inventory_t *inventory, int mode);
void game_set_inventory_serf_mode(inventory_t *inventory, int mode);

#endif /* !_GAME_H */
