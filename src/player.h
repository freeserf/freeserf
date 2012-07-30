/* player.h */

#ifndef _PLAYER_H
#define _PLAYER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "freeserf.h"
#include "gfx.h"


/* player_sett_t object.
   Actually holds the game state of a player.
   This is the same for both human and AI players. */
typedef struct {
	/* pl_sett_pre */
	int tool_prio[9];
	int resource_count[26];
	int flag_prio[26];
	int serf_count[27];
	int knight_occupation[4];

	/* pl_sett */
	int player_num;
	int flags;
	int build;
	int completed_building_count[24];
	int incomplete_building_count[24];
	/* 60 */
	int inventory_prio[26];
	int attacking_buildings[64];
	int current_sett_5_item;
	int map_cursor_col;
	int map_cursor_row;
	/* 100 */
	int map_cursor_type;
	panel_btn_t panel_btn_type;
	int building_height_after_level;
	int building;
	int castle_flag;
	int castle_inventory;
	int cont_search_after_non_optimal_find;
	int knights_to_spawn;
	/*int spawn_serf_want_knight; OBSOLETE by local var */
	/* 110 */
	int field_110;
	uint total_land_area;
	uint total_building_score;
	uint total_military_score;
	uint16_t last_anim;
	/* 120 */
	int reproduction_counter;
	int reproduction_reset;
	int serf_to_knight_rate;
	uint16_t serf_to_knight_counter; /* Overflow is important */
	int field_128;
	int attacking_knights[4];
	int total_attacking_knights;
	int building_attacked;
	int knights_attacking;
	int analysis_goldore;
	int analysis_ironore;
	int analysis_coal;
	int analysis_stone;
	/* 140 */
	int food_stonemine; /* Food delivery priority of food for mines. */
	int food_coalmine;
	int food_ironmine;
	int food_goldmine;
	int planks_construction; /* Planks delivery priority. */
	int planks_boatbuilder;
	int planks_toolmaker;
	int steel_toolmaker;
	/* 150 */
	int steel_weaponsmith;
	int coal_steelsmelter;
	int coal_goldsmelter;
	int coal_weaponsmith;
	int wheat_pigfarm;
	int wheat_mill;
	int current_sett_6_item;
	/* 162 */
	int initial_supplies;
	/* 164 */
	int extra_planks;
	int extra_stone;
	int lumberjack_index;
	int sawmill_index;
	int stonecutter_index;
	/* 16E */
	int serf_index;
	/* 170 */
	int field_170;
	int timers_count;
	/* 176 */
	int index;
	/* 184 */
	int knight_morale;
	/* 188 */
	int gold_deposited;
	int castle_knights_wanted;
	int castle_knights;
	int ai_value_0;
	int ai_value_1;
	int ai_value_2;
	int ai_value_3;
	int ai_value_4;
	int ai_value_5;
	/* 1AE */
	int ai_intelligence;
	/* 11C4 */
	int resource_count_history[26][120];
	/* 1E34 ? */
	int msg_queue_type[64];
	map_pos_t msg_queue_pos[64];

	struct {
		int timeout;
		map_pos_t pos;
	} timers[64];

	/* ... */
} player_sett_t;

/* player_t object.
   Actually represents the interface to the human player.
   Note: there is no player_t object for AI players for this reason. */
typedef struct {
	int flags;
	int click;
	int pointer_x_max;
	int pointer_y_max;
	int pointer_x;
	int pointer_y;
	int pointer_x_off;
	int pointer_x_clk;
	int pointer_y_clk;
	/* 10 */
	/* OBSOLETE
	int pointer_x_drag;
	int pointer_y_drag;
	*/
	/* 1A */
	frame_t *frame;
	/* 20 */
	int game_area_cols;
	/* 30 */
	int bottom_panel_x; /* ADDITION */
	int bottom_panel_y;
	int bottom_panel_width; /* ADDITION */
	int bottom_panel_height; /* ADDITION */
	/* 3E */
	int frame_width;
	int frame_height;
	/* 46 */
	/*int col_game_area;*/ /* OBSOLETE */
	/*int row_game_area;*/ /* OBSOLETE */
	int col_offset;
	int row_offset;
	int map_min_x;
	int map_min_y; /* ADDITION */
	int game_area_rows;
	int map_max_y;
	/* 54 */
	map_1_t **map_rows;
	/* 5C */
	frame_t *popup_frame;
	/* 60 */
	int panel_btns[5];
	int panel_btns_set[5];
	int panel_btns_x;
	int msg_icon_x;
	/* 70 */
	box_t box;
	box_t clkmap;
	/* 78 */
	int popup_x;
	int popup_y;
	/* 82 */
	player_sett_t *sett;
	int config;
	int msg_flags;
	int map_cursor_col_max;
	/* 8E */
	int map_cursor_col_off;
	int map_y_off;
	/*int **map_serf_rows;*/ /* OBSOLETE */
	int message_box;
	/* 9A */
	int map_x_off;
	/* A0 */
	int panel_btns_dist;
	/* A4 */
	sprite_loc_t map_cursor_sprites[7];
	int road_length;
	int field_D0;
	/* D2 */
	uint16_t last_anim;
	/* D6 */
	int current_stat_7_item;
	/* 1B4 */
	/* Determines what sfx should be played. */
	int water_in_view;
	int trees_in_view;
	/* 1C0 */
	int return_timeout;
	int return_col_game_area;
	int return_row_game_area;
	/* 1E0 */
	int panel_btns_first_x;
	int timer_icon_x;
	/* ... */
} player_t;


void player_sett_reset_food_priority(player_sett_t *sett);
void player_sett_reset_planks_priority(player_sett_t *sett);
void player_sett_reset_steel_priority(player_sett_t *sett);
void player_sett_reset_coal_priority(player_sett_t *sett);
void player_sett_reset_wheat_priority(player_sett_t *sett);
void player_sett_reset_tool_priority(player_sett_t *sett);

void player_sett_reset_flag_priority(player_sett_t *sett);
void player_sett_reset_inventory_priority(player_sett_t *sett);


#endif /* ! _PLAYER_H */
