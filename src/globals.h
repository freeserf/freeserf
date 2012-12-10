/*
 * globals.h - The global struct keeping the game state
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

#ifndef _GLOBALS_H
#define _GLOBALS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "freeserf.h"
#include "map.h"
#include "serf.h"
#include "flag.h"
#include "player.h"
#include "random.h"


/* Globals struct */
typedef struct {
	map_t map; /* ADDITION */
	/* 0 */
	/* MOVED to map_t
	uint32_t map_index_mask;
	uint32_t map_dirs[8];
	map_1_t *map_mem2_ptr;
	uint32_t map_col_size;
	uint32_t map_elms; */
	/* 30 */
	/*
	uint16_t map_row_shift;
	uint16_t map_col_mask;
	uint16_t map_row_mask;
	uint32_t map_data_offset;
	uint16_t map_shifted_col_mask;
	uint32_t map_shifted_row_mask; */
	/* 40 */
	/*
	uint16_t map_col_pairs;
	uint16_t map_row_pairs; */
	int16_t map_water_level;
	int16_t map_max_lake_area;
	uint16_t map_max_serfs_left;
	int map_field_4A;
	uint32_t map_gold_deposit;
	/* 50 */
	uint16_t map_size;
	int map_field_52;
	/* 58 */
	uint16_t map_62_5_times_regions;
	int map_gold_morale_factor;
	/* 5E */
	int winning_player;
	/* 60 */
	/* uint16_t map_move_left_2; MOVED to map_t */
	/* 64 */
	player_sett_t *player_sett[4];
	/* 78 */
	player_t *player[2];
	/* 80 */
	int *spiral_pattern;
	/*void *player_map_rows[2];*/ /* OBSOLETE */
	uint8_t *minimap;
	/* 90 */
	/*int **map_serf_rows_left;*/ /* OBSOLETE */
	/*int **map_serf_rows_right;*/ /* OBSOLETE */
	/* 98 */
	flag_t *flgs;
	building_t *buildings;
	serf_t *serfs;
	/* A4 */
	uint8_t *flg_bitmap;
	uint8_t *buildings_bitmap;
	uint8_t *serfs_bitmap;
	/* B0 */
	int *serf_animation_table;
	flag_t **flag_queue_black;
	flag_t **flag_queue_white;
	/* C4 */
	map_pos_t *spiral_pos_pattern;
	/* F0 */
	inventory_t *inventories;
	uint8_t *inventories_bitmap;
	/* 108 */
	frame_t *frame;
	/* 1C2 */
	/* MOVED to map_t 
	uint16_t map_cols;
	uint16_t map_rows; */
	/* 1C8 */
	uint8_t svga; /* flags */
	/* 1D6 */
	player_init_t pl_init[4];
	/* 1EE */
	random_state_t init_map_rnd;
	/* 1FA */
	uint32_t game_speed_save;
	uint32_t game_speed;
	uint32_t game_tick;  /* Overflow might be important */
	uint16_t anim;  /* Overflow might be important */
	uint16_t old_anim;
	/* 20E */
	uint16_t game_stats_counter;
	uint16_t history_counter;
	random_state_t rnd;
	uint8_t field_218[4];
	uint16_t map_regions;
	/* OBSOLETE by local vars */
	/*uint8_t init_map_old_type;
	uint8_t init_map_seed_type;
	uint8_t init_map_new_type;
	uint8_t init_map_saved_types[9];
	uint16_t field_22A;
	int16_t field_22C;
	uint16_t init_map_tries;*/
	/* 230 */
	/*uint16_t field_230;
	uint16_t field_232;
	uint8_t init_map_type_min;
	uint8_t init_map_type_max;
	uint8_t init_map_rnd_offset;
	uint8_t init_map_rnd_mask;*/
	/* 248 */
	/* OBSOLETE by local vars*/
	/*uint16_t build_road_source_flag;
	uint16_t build_road_out_dir;
	uint16_t build_road_in_dir;*/
	uint16_t field_24E;
	/* 250 */
	/* OBSOLETE by local vars */
	/*uint16_t short_row_length;
	uint16_t long_row_length;*/
	/* 258 */
	uint16_t max_flg_cnt;
	uint16_t max_building_cnt;
	uint16_t max_serf_cnt;
	uint16_t max_ever_flag_index;
	/* 260 */
	uint16_t max_ever_building_index;
	uint16_t max_ever_serf_index;
	uint16_t max_inventory_cnt;
	uint16_t max_ever_inventory_index;
	/* 26C */
	uint16_t next_index;
	uint16_t flag_search_counter;
	uint16_t flag_queue_select;
	/* 276 */
	int16_t flags_in_queue;
	/* 27A */
	building_type_t building_type;
	uint16_t update_map_last_anim;
	int16_t update_map_counter;
	/* 280 */
	map_pos_t update_map_initial_pos;
	int anim_diff;
	/* 286 */
	uint16_t max_next_index;
	/* 28C*/
	int16_t update_map_16_loop;
	/* 2F8 */
	/*map_1_t *map_mem2; MOVED to map_t */
	/*uint8_t *map_mem5;*/
	/* 320 */
	int player_history_index[4];
	int player_history_counter[3];
	/* 32E */
	int resource_history_index;
	/* 340 */
	uint16_t field_340;
	uint16_t field_342;
	inventory_t *field_344;
	/* 352 */
	int game_type;
	int tutorial_level;
	int mission_level;
	int map_generator; /* ADDITION */
	int map_preserve_bugs; /* ADDITION */
	/* 37C */
	uint8_t split;
	/* 380 */
	int player_score_leader;
	/* 3D8 */
	uint8_t cfg_left;
	uint8_t cfg_right;
	/* ... */
} globals_t;

globals_t globals;

#endif /* ! _GLOBALS_H */
