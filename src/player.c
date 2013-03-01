/*
 * player.c - Player related functions
 *
 * Copyright (C) 2013  Jon Lund Steffensen <jonlst@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "player.h"
#include "freeserf.h"
#include "game.h"
#include "audio.h"
#include "globals.h"
#include "debug.h"


void gui_show_popup_frame(int show);

void
player_open_popup(player_t *player, int box)
{
	player->box = box;
	gui_show_popup_frame(1);
}

/* Close the popup box for player. */
void
player_close_popup(player_t *player)
{
	gui_show_popup_frame(0);
	player->click &= ~BIT(6);
	player->panel_btns[2] = PANEL_BTN_MAP;
	player->panel_btns[3] = PANEL_BTN_STATS;
	player->panel_btns[4] = PANEL_BTN_SETT;
	player->click |= BIT(1);
	player->clkmap = 0;
	player->click |= BIT(2);
}

/* Determine what buildings can possibly be built at map_pos[0]. */
static void
determine_possible_building(const player_sett_t *sett, map_pos_t map_pos[], int hills,
			    panel_btn_t *panel_btn, int *build_flags, int *height_after_level)
{
	if (hills) {
		if (PLAYER_HAS_CASTLE(sett)) {
			*panel_btn = PANEL_BTN_BUILD_MINE;
		}
	} else {
		if (PLAYER_HAS_CASTLE(sett)) {
			*panel_btn = PANEL_BTN_BUILD_SMALL;
		}

		/* Check for adjacent military building */
		*build_flags &= ~BIT(0); /* Can build military building */
		for (int i = 0; i < 12; i++) {
			map_obj_t obj = MAP_OBJ(map_pos[7+i]);
			if (obj >= MAP_OBJ_SMALL_BUILDING &&
			    obj <= MAP_OBJ_CASTLE) {
				building_t *bld = game_get_building(MAP_OBJ_INDEX(map_pos[7+i]));
				building_type_t bld_type = BUILDING_TYPE(bld);
				if (bld_type == BUILDING_HUT ||
				    bld_type == BUILDING_TOWER ||
				    bld_type == BUILDING_FORTRESS ||
				    bld_type == BUILDING_CASTLE) {
					*build_flags |= BIT(0); /* Can not build military building */
					break;
				}
			}
		}

		/* Check that surroundings are passable by serfs. */
		for (int i = 0; i < 6; i++) {
			map_space_t s = map_space_from_obj[MAP_OBJ(map_pos[1+i])];
			if (s >= MAP_SPACE_IMPASSABLE && s != MAP_SPACE_FLAG) return;
		}

		/* Check that buildings in the second shell aren't large or castle. */
		for (int i = 0; i < 12; i++) {
			map_space_t s = map_space_from_obj[MAP_OBJ(map_pos[7+i])];
			if (s >= MAP_SPACE_LARGE_BUILDING) return;
		}

		/* Check if center hexagon is not type 5 (grass?) */
		if (MAP_TYPE_UP(map_pos[0]) != 5 ||
		    MAP_TYPE_DOWN(map_pos[0]) != 5 ||
		    MAP_TYPE_DOWN(map_pos[1+DIR_LEFT]) != 5 ||
		    MAP_TYPE_UP(map_pos[1+DIR_UP_LEFT]) != 5 ||
		    MAP_TYPE_DOWN(map_pos[1+DIR_UP_LEFT]) != 5 ||
		    MAP_TYPE_UP(map_pos[1+DIR_UP]) != 5) {
			return;
		}

		/* Find min and max height */
		int h_min = 31;
		int h_max = 0;
		for (int i = 0; i < 12; i++) {
			int h = MAP_HEIGHT(map_pos[7+i]);
			if (h_min > h) h_min = h;
			if (h_max < h) h_max = h;
		}

		/* Adjust for height of adjacent unleveled buildings */
		for (int i = 0; i < 18; i++) {
			if (MAP_OBJ(map_pos[19+i]) == MAP_OBJ_LARGE_BUILDING) {
				building_t *bld = game_get_building(MAP_OBJ_INDEX(map_pos[19+i]));
				if (BIT_TEST(bld->bld, 7) && /* Unfinished */
				    bld->progress == 0) { /* Leveling in progress */
					int h = bld->u.s.level;
					if (h_min > h) h_min = h;
					if (h_max < h) h_max = h;
				}
			}
		}

		/* Return if height difference is too big */
		if (h_max - h_min >= 9) return;

		/* Calculate "mean" height. Height of center is added twice. */
		int h_mean = MAP_HEIGHT(map_pos[0]);
		for (int i = 0; i < 7; i++) {
			h_mean += MAP_HEIGHT(map_pos[i]);
		}
		h_mean >>= 3;

		/* Calcualte height after leveling */
		int h_new_min = max((h_max > 4) ? (h_max - 4) : 1, 1);
		int h_new_max = h_min + 4;
		int h_new = clamp(h_new_min, h_mean, h_new_max);
		*height_after_level = h_new;

		if (PLAYER_HAS_CASTLE(sett)) {
			*panel_btn = PANEL_BTN_BUILD_LARGE;
		} else {
			*panel_btn = PANEL_BTN_BUILD_CASTLE;
		}
	}
}

static int
determine_map_cursor_type_sub(int type)
{
	if (type >= 4 && type < 8) return 0;
	else if (type >= 11 && type < 15) return BIT(0);
	return BIT(1);
}

/* Initialize an array of map_pos_t following a spiral pattern based in init_pos. */
static void
populate_circular_map_pos_array(map_pos_t map_pos[], map_pos_t init_pos, int size)
{
	for (int i = 0; i < size; i++) {
		map_pos[i] = MAP_POS_ADD(init_pos, globals.spiral_pos_pattern[i]);
	}
}

/* Return the cursor type and various related values of a map_pos_t. */
static void
get_map_cursor_type(const player_sett_t *sett, map_pos_t pos, panel_btn_t *panel_btn,
		    int *build_flags, map_cursor_type_t *cursor_type, int *height_after_level)
{
	map_pos_t map_pos[1+6+12+18];
	populate_circular_map_pos_array(map_pos, pos, 1+6+12+18);

	int player = sett->player_num;
	if (!PLAYER_HAS_CASTLE(sett)) player = -1;

	if (player >= 0 &&
	    (!MAP_HAS_OWNER(map_pos[0]) ||
	     MAP_OWNER(map_pos[0]) != player)) {
		return;
	}

	if (map_space_from_obj[MAP_OBJ(map_pos[0])] == MAP_SPACE_FLAG) {
		if (BIT_TEST(MAP_PATHS(map_pos[0]), DIR_UP_LEFT) &&
		    map_space_from_obj[MAP_OBJ(map_pos[1+DIR_UP_LEFT])] >= MAP_SPACE_SMALL_BUILDING) {
			*cursor_type = MAP_CURSOR_TYPE_FLAG;
			return;
		}

		if (MAP_PATHS(map_pos[0]) == 0) {
			*cursor_type = MAP_CURSOR_TYPE_REMOVABLE_FLAG;
			return;
		}

		flag_t *flag = game_get_flag(MAP_OBJ_INDEX(map_pos[0]));
		int connected = 0;
		void *other_end = NULL;

		for (int i = DIR_UP; i >= DIR_RIGHT; i--) {
			if (FLAG_HAS_PATH(flag, i)) {
				if (FLAG_IS_WATER_PATH(flag, i)) {
					*cursor_type = MAP_CURSOR_TYPE_FLAG;
					return;
				}

			        connected += 1;

				if (other_end != NULL) {
					if (flag->other_endpoint.v[i] == other_end) {
						*cursor_type = MAP_CURSOR_TYPE_FLAG;
						return;
					}
				} else {
					other_end = flag->other_endpoint.v[i];
				}
			}
		}

		if (connected == 2) *cursor_type = MAP_CURSOR_TYPE_REMOVABLE_FLAG;
		else *cursor_type = MAP_CURSOR_TYPE_FLAG;
	} else if (map_space_from_obj[MAP_OBJ(map_pos[0])] < MAP_SPACE_FLAG) {
		int paths = MAP_PATHS(map_pos[0]);
		if (paths == 0) {
			if (map_space_from_obj[MAP_OBJ(map_pos[1+DIR_DOWN_RIGHT])] == MAP_SPACE_FLAG) {
				*cursor_type = MAP_CURSOR_TYPE_CLEAR_BY_FLAG;
			} else if (MAP_PATHS(map_pos[1+DIR_DOWN_RIGHT]) == 0) {
				*cursor_type = MAP_CURSOR_TYPE_CLEAR;
			} else {
				*cursor_type = MAP_CURSOR_TYPE_CLEAR_BY_PATH;
			}
		} else if (paths == BIT(DIR_DOWN_RIGHT) ||
			   paths == BIT(DIR_UP_LEFT)) {
			NOT_REACHED();
		} else {
			*cursor_type = MAP_CURSOR_TYPE_PATH;
		}

		if (map_space_from_obj[MAP_OBJ(map_pos[0])] != MAP_SPACE_OPEN) return;

		/* Return if cursor is in water */
		if (MAP_TYPE_UP(map_pos[0]) < 4 &&
		    MAP_TYPE_DOWN(map_pos[0]) < 4 &&
		    MAP_TYPE_DOWN(map_pos[1+DIR_LEFT]) < 4 &&
		    MAP_TYPE_UP(map_pos[1+DIR_UP_LEFT]) < 4 &&
		    MAP_TYPE_DOWN(map_pos[1+DIR_UP_LEFT]) < 4 &&
		    MAP_TYPE_UP(map_pos[1+DIR_UP]) < 4) {
			return;
		}

		int found = 0;
		for (int i = 0; i < 6; i++) {
			if (map_space_from_obj[MAP_OBJ(map_pos[1+i])] == MAP_SPACE_FLAG) {
				if (*cursor_type == MAP_CURSOR_TYPE_PATH) return;
				found = 1;
				break;
			}
		}

		if (!found) {
			*build_flags &= ~BIT(1);
			if (PLAYER_HAS_CASTLE(sett)) {
				*panel_btn = PANEL_BTN_BUILD_FLAG;
				if (*cursor_type == MAP_CURSOR_TYPE_PATH) return;
			}
		}

		for (int i = 0; i < 6; i++) {
			if (map_space_from_obj[MAP_OBJ(map_pos[1+i])] >= MAP_SPACE_SMALL_BUILDING) return;
		}

		if (*cursor_type != MAP_CURSOR_TYPE_CLEAR_BY_FLAG) {
			if (map_space_from_obj[MAP_OBJ(map_pos[1+DIR_DOWN_RIGHT])] != MAP_SPACE_OPEN) return;
		}

		/* Check whether a flag exists anywhere around the position down right. */
		if (map_space_from_obj[MAP_OBJ(map_pos[1+6+0])] == MAP_SPACE_FLAG ||
		    map_space_from_obj[MAP_OBJ(map_pos[1+6+1])] == MAP_SPACE_FLAG ||
		    map_space_from_obj[MAP_OBJ(map_pos[1+6+7])] == MAP_SPACE_FLAG ||
		    map_space_from_obj[MAP_OBJ(map_pos[1+DIR_RIGHT])] == MAP_SPACE_FLAG ||
		    map_space_from_obj[MAP_OBJ(map_pos[1+DIR_DOWN])] == MAP_SPACE_FLAG) {
			return;
		}

		/* Check whether there is water around the position down right. */
		if (MAP_TYPE_UP(map_pos[1+DIR_RIGHT]) < 4 ||
		    MAP_TYPE_DOWN(map_pos[1+DIR_DOWN]) < 4 ||
		    MAP_TYPE_UP(map_pos[1+DIR_DOWN_RIGHT]) < 4 ||
		    MAP_TYPE_DOWN(map_pos[1+DIR_DOWN_RIGHT]) < 4) {
			return;
		}

		/* 426FD: Check owner of surrounding land */
		for (int i = 0; i < 6; i++) {
			if (player >= 0 &&
			    (!MAP_HAS_OWNER(map_pos[1+i]) ||
			     MAP_OWNER(map_pos[1+i]) != player)) {
				return;
			}
		}

		int bits = 0;
		bits |= determine_map_cursor_type_sub(MAP_TYPE_UP(map_pos[0]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_DOWN(map_pos[0]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_DOWN(map_pos[1+DIR_LEFT]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_UP(map_pos[1+DIR_UP_LEFT]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_DOWN(map_pos[1+DIR_UP_LEFT]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_UP(map_pos[1+DIR_UP]));

		if (bits < 2) {
			determine_possible_building(sett, map_pos, bits,
						    panel_btn, build_flags,
						    height_after_level);
		}
	} else if (map_space_from_obj[MAP_OBJ(map_pos[0])] == MAP_SPACE_SMALL_BUILDING ||
		   map_space_from_obj[MAP_OBJ(map_pos[0])] == MAP_SPACE_LARGE_BUILDING) {
		building_t *bld = game_get_building(MAP_OBJ_INDEX(map_pos[0]));
		if (BUILDING_IS_BURNING(bld)) return;

		*cursor_type = MAP_CURSOR_TYPE_BUILDING;

		/* 426FD: Check owner of surrounding land */
		for (int i = 0; i < 6; i++) {
			if (player >= 0 &&
			    (!MAP_HAS_OWNER(map_pos[1+i]) ||
			     MAP_OWNER(map_pos[1+i]) != player)) {
				return;
			}
		}

		int bits = 0;
		bits |= determine_map_cursor_type_sub(MAP_TYPE_UP(map_pos[0]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_DOWN(map_pos[0]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_DOWN(map_pos[1+DIR_LEFT]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_UP(map_pos[1+DIR_UP_LEFT]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_DOWN(map_pos[1+DIR_UP_LEFT]));
		bits |= determine_map_cursor_type_sub(MAP_TYPE_UP(map_pos[1+DIR_UP]));

		if (bits < 2) {
			determine_possible_building(sett, map_pos, bits,
						    panel_btn, build_flags,
						    height_after_level);
		}
	}
}

/* Update the player_t object with the information returned
   in get_map_cursor_type(). */
void
player_determine_map_cursor_type(player_t *player)
{
	player->sett->build |= BIT(1); /* Can not build flag */

	player->sett->map_cursor_type = MAP_CURSOR_TYPE_NONE;
	player->sett->panel_btn_type = PANEL_BTN_BUILD_INACTIVE;

	map_pos_t cursor_pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
	get_map_cursor_type(player->sett, cursor_pos,
			    &player->sett->panel_btn_type,
			    &player->sett->build,
			    &player->sett->map_cursor_type,
			    &player->sett->building_height_after_level);
}

/* Update the player_t object with the information returned
   in get_map_cursor_type(). This is sets the appropriate values
   when the player interface is in road construction mode. */
void
player_determine_map_cursor_type_road(player_t *player)
{
	map_pos_t cursor_pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
	map_pos_t map_pos[1+6];
	populate_circular_map_pos_array(map_pos, cursor_pos, 1+6);

	int h = MAP_HEIGHT(map_pos[0]);
	int bits = 0;
	int paths = 0;
	if (player->road_length > 0) paths = MAP_PATHS(map_pos[0]);

	for (int i = 0; i < 6; i++) {
		int sprite = 0;

		if (MAP_HAS_OWNER(map_pos[1+i]) &&
		    MAP_OWNER(map_pos[1+i]) == player->sett->player_num) {
			if (!BIT_TEST(paths, i)) {
				if (map_space_from_obj[MAP_OBJ(map_pos[1+i])] == MAP_SPACE_IMPASSABLE ||
				    map_space_from_obj[MAP_OBJ(map_pos[1+i])] == MAP_SPACE_SMALL_BUILDING) {
					sprite = 44; /* striped */
				} else if (map_space_from_obj[MAP_OBJ(map_pos[1+i])] == MAP_SPACE_FLAG ||
					   MAP_PATHS(map_pos[1+i]) == 0) {
					int h_diff = MAP_HEIGHT(map_pos[1+i]) - h;
					sprite = 39 + h_diff; /* height indicators */
					bits |= BIT(i);
				} else {
					panel_btn_t panel_btn;
					map_cursor_type_t cursor_type;
					int build_flags, height_after_level;
					get_map_cursor_type(player->sett, map_pos[1+i],
							    &panel_btn, &build_flags,
							    &cursor_type, &height_after_level);
					if (BIT_TEST(build_flags, 1) ||
					    panel_btn == PANEL_BTN_BUILD_INACTIVE ||
					    /*check_can_build_flag_on_road(map_pos[1+i]) < 0*/1) {
						sprite = 44; /* striped */
					} else {
						int h_diff = MAP_HEIGHT(map_pos[1+i]) - h;
						sprite = 39 + h_diff; /* height indicators */
						bits |= BIT(i);
					}
				}
			} else {
				sprite = 45; /* undo */
				bits |= BIT(i);
			}
		} else {
			sprite = 44; /* striped */
		}
		player->map_cursor_sprites[i+1].sprite = sprite;
	}

	player->field_D0 = bits;
}

/* Set the appropriate sprites for the panel buttons and the map cursor. */
void
player_update_interface(player_t *player)
{
	if (/*not demo mode*/1) {
		if (BIT_TEST(player->click, 7)) { /* Building road */
			player->panel_btns[0] = PANEL_BTN_BUILD_ROAD_STARRED;
			player->panel_btns[1] = PANEL_BTN_BUILD_INACTIVE;
		} else {
			switch (player->sett->map_cursor_type) {
				case MAP_CURSOR_TYPE_NONE:
					player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
					if (PLAYER_HAS_CASTLE(player->sett)) {
						player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					} else {
						player->panel_btns[1] = PANEL_BTN_GROUND_ANALYSIS;
					}
					player->map_cursor_sprites[0].sprite = 32;
					player->map_cursor_sprites[2].sprite = 33;
					break;
				case MAP_CURSOR_TYPE_FLAG:
					player->panel_btns[0] = PANEL_BTN_BUILD_ROAD;
					player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					player->map_cursor_sprites[0].sprite = 51;
					player->map_cursor_sprites[2].sprite = 33;
					break;
				case MAP_CURSOR_TYPE_REMOVABLE_FLAG:
					player->panel_btns[0] = PANEL_BTN_BUILD_ROAD;
					player->panel_btns[1] = PANEL_BTN_DESTROY;
					player->map_cursor_sprites[0].sprite = 51;
					player->map_cursor_sprites[2].sprite = 33;
					break;
				case MAP_CURSOR_TYPE_BUILDING:
					player->panel_btns[0] = player->sett->panel_btn_type;
					player->panel_btns[1] = PANEL_BTN_DESTROY;
					player->map_cursor_sprites[0].sprite = 46 + player->sett->panel_btn_type;
					player->map_cursor_sprites[2].sprite = 33;
					break;
				case MAP_CURSOR_TYPE_PATH:
					player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
					player->panel_btns[1] = PANEL_BTN_DESTROY_ROAD;
					player->map_cursor_sprites[0].sprite = 52;
					player->map_cursor_sprites[2].sprite = 33;
					if (player->sett->panel_btn_type != PANEL_BTN_BUILD_INACTIVE) {
						player->panel_btns[0] = PANEL_BTN_BUILD_FLAG;
						player->map_cursor_sprites[0].sprite = 47;
					}
					break;
				case MAP_CURSOR_TYPE_CLEAR_BY_FLAG:
					if (player->sett->panel_btn_type < PANEL_BTN_BUILD_MINE) {
						player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
						if (PLAYER_HAS_CASTLE(player->sett)) {
							player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
						} else {
							player->panel_btns[1] = PANEL_BTN_GROUND_ANALYSIS;
						}
						player->map_cursor_sprites[0].sprite = 32;
						player->map_cursor_sprites[2].sprite = 33;
					} else {
						player->panel_btns[0] = player->sett->panel_btn_type;
						player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
						player->map_cursor_sprites[0].sprite = 46 + player->sett->panel_btn_type;
						player->map_cursor_sprites[2].sprite = 33;
					}
					break;
				case MAP_CURSOR_TYPE_CLEAR_BY_PATH:
					player->panel_btns[0] = player->sett->panel_btn_type;
					player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					if (player->sett->panel_btn_type != PANEL_BTN_BUILD_INACTIVE) {
						player->map_cursor_sprites[0].sprite = 46 + player->sett->panel_btn_type;
						if (player->sett->panel_btn_type == PANEL_BTN_BUILD_FLAG) {
							player->map_cursor_sprites[2].sprite = 33;
						} else {
							player->map_cursor_sprites[2].sprite = 47;
						}
					} else {
						player->map_cursor_sprites[0].sprite = 32;
						player->map_cursor_sprites[2].sprite = 33;
					}
					break;
				case MAP_CURSOR_TYPE_CLEAR:
					player->panel_btns[0] = player->sett->panel_btn_type;
					if (PLAYER_HAS_CASTLE(player->sett)) {
						player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					} else {
						player->panel_btns[1] = PANEL_BTN_GROUND_ANALYSIS;
					}
					if (player->sett->panel_btn_type) {
						if (player->sett->panel_btn_type == PANEL_BTN_BUILD_CASTLE) {
							player->map_cursor_sprites[0].sprite = 50;
						} else {
							player->map_cursor_sprites[0].sprite = 46 + player->sett->panel_btn_type;
						}
						if (player->sett->panel_btn_type == PANEL_BTN_BUILD_FLAG) {
							player->map_cursor_sprites[2].sprite = 33;
						} else {
							player->map_cursor_sprites[2].sprite = 47;
						}
					} else {
						player->map_cursor_sprites[0].sprite = 32;
						player->map_cursor_sprites[2].sprite = 33;
					}
					break;
				default:
					NOT_REACHED();
					break;
			}
		}
	} else {
		/* TODO demo mode */
	}
}


/* Start road construction mode for player interface. */
void
player_build_road_begin(player_t *player)
{
	player->flags &= ~BIT(6);

	player_determine_map_cursor_type(player);

	if (player->sett->map_cursor_type != MAP_CURSOR_TYPE_FLAG &&
	    player->sett->map_cursor_type != MAP_CURSOR_TYPE_REMOVABLE_FLAG) {
		player_update_interface(player);
		return;
	}

	player->panel_btns[0] = PANEL_BTN_BUILD_ROAD_STARRED;
	player->panel_btns[1] = PANEL_BTN_BUILD_INACTIVE;
	player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
	player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
	player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
	player->click |= BIT(6);
	player->click |= BIT(7);
	player->click |= BIT(2);
	player->road_length = 0;
}

/* End road construction mode for player interface. */
void
player_build_road_end(player_t *player)
{
	player->click &= ~BIT(6);
	player->panel_btns[2] = PANEL_BTN_MAP;
	player->panel_btns[3] = PANEL_BTN_STATS;
	player->panel_btns[4] = PANEL_BTN_SETT;
	player->click &= ~BIT(7);
	player->click |= BIT(2);

	player->map_cursor_sprites[1].sprite = 33;
	player->map_cursor_sprites[2].sprite = 33;
	player->map_cursor_sprites[3].sprite = 33;
	player->map_cursor_sprites[4].sprite = 33;
	player->map_cursor_sprites[5].sprite = 33;
	player->map_cursor_sprites[6].sprite = 33;

	map_tile_t *tiles = globals.map.tiles;
	map_pos_t pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);

	for (int i = 0; i < player->road_length; i++) {
		dir_t backtrack_dir = -1;
		for (dir_t d = 0; d < 6; d++) {
			if (BIT_TEST(tiles[pos].flags, d)) {
				backtrack_dir = d;
				break;
			}
		}

		map_pos_t next_pos = MAP_MOVE(pos, backtrack_dir);

		tiles[pos].flags &= ~BIT(backtrack_dir);
		tiles[next_pos].flags &= ~BIT(DIR_REVERSE(backtrack_dir));
		pos = next_pos;
	}

	/* TODO set_map_redraw(); */
}

/* Connect a road under construction to an existing flag at dest. out_dir is the
   direction from the flag down the new road. */
static int
player_build_road_connect_flag(player_t *player, map_pos_t dest, dir_t out_dir)
{
	if (!MAP_HAS_OWNER(dest) || MAP_OWNER(dest) != player->sett->player_num) {
		return -1;
	}

	dir_t in_dir = -1;

	flag_t *dest_flag = game_get_flag(MAP_OBJ_INDEX(dest));

	int paths = BIT(out_dir);
	int test = 0;

	/* Backtrack along path to other flag. Test along the way
	   whether the path is on ground or in water. */
	map_pos_t src = dest;
	for (int i = 0; i < player->road_length + 1; i++) {
		if (BIT_TEST(paths, DIR_RIGHT)) {
			if (MAP_TYPE_UP(src) < 4 &&
			    MAP_TYPE_UP(MAP_MOVE_UP(src)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_LEFT;
			src = MAP_MOVE_RIGHT(src);
		} else if (BIT_TEST(paths, DIR_DOWN_RIGHT)) {
			if (MAP_TYPE_UP(src) < 4 &&
			    MAP_TYPE_DOWN(src) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_UP_LEFT;
			src = MAP_MOVE_DOWN_RIGHT(src);
		} else if (BIT_TEST(paths, DIR_DOWN)) {
			if (MAP_TYPE_UP(src) < 4 &&
			    MAP_TYPE_DOWN(MAP_MOVE_LEFT(src)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_UP;
			src = MAP_MOVE_DOWN(src);
		} else if (BIT_TEST(paths, DIR_LEFT)) {
			if (MAP_TYPE_DOWN(MAP_MOVE_LEFT(src)) < 4 &&
			    MAP_TYPE_UP(MAP_MOVE_UP(src)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_RIGHT;
			src = MAP_MOVE_LEFT(src);
		} else if (BIT_TEST(paths, DIR_UP_LEFT)) {
			if (MAP_TYPE_UP(MAP_MOVE_UP_LEFT(src)) < 4 &&
			    MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(src)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_DOWN_RIGHT;
			src = MAP_MOVE_UP_LEFT(src);
		} else if (BIT_TEST(paths, DIR_UP)) {
			if (MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(src)) < 4 &&
			    MAP_TYPE_UP(MAP_MOVE_RIGHT(src)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_DOWN;
			src = MAP_MOVE_UP(src);
		}

		if (!MAP_HAS_OWNER(src) || MAP_OWNER(src) != player->sett->player_num) {
			return -1;
		}

		paths = MAP_PATHS(src) & ~BIT(in_dir);
	}

	/* Bit 0 indicates a ground path, bit 1 indicates
	   water path. Abort if path went through both
	   ground and water. */
	int water_path = 0;
	if (test != BIT(0)) {
		water_path = 1;
		if (test != BIT(1)) return -1;
	}

	/* Connect flags */
	flag_t *src_flag = game_get_flag(MAP_OBJ_INDEX(src));

	dest_flag->path_con |= BIT(out_dir);
	dest_flag->endpoint |= BIT(out_dir);
	dest_flag->transporter &= ~BIT(out_dir);

	src_flag->path_con |= BIT(in_dir);
	src_flag->endpoint |= BIT(in_dir);
	src_flag->transporter &= ~BIT(in_dir);

	if (water_path) {
		dest_flag->endpoint &= ~BIT(out_dir);
		src_flag->endpoint &= ~BIT(in_dir);
	}

	dest_flag->other_end_dir[out_dir] = (dest_flag->other_end_dir[out_dir] & 0xc7) | (in_dir << 3);
	src_flag->other_end_dir[in_dir] = (src_flag->other_end_dir[in_dir] & 0xc7) | (out_dir << 3);

	int len = game_get_road_length_value(player->road_length + 1);

	dest_flag->length[out_dir] = len;
	src_flag->length[in_dir] = len;

	dest_flag->other_endpoint.f[out_dir] = src_flag;
	src_flag->other_endpoint.f[in_dir] = dest_flag;

	return 0;
}

/* Build a single road segment. Return -1 on fail, 0 on successful
   construction, and 1 if this segment completed the path. */
int
player_build_road_segment(player_t *player, map_pos_t pos, dir_t dir)
{
	if (!game_road_segment_valid(pos, dir)) return -1;

	map_pos_t dest = MAP_MOVE(pos, dir);
	dir_t dir_rev = DIR_REVERSE(dir);
	map_tile_t *tiles = globals.map.tiles;

	if (MAP_OBJ(dest) == MAP_OBJ_FLAG) {
		/* Existing flag at destination, try to connect. */
		int r = player_build_road_connect_flag(player, dest, dir_rev);
		if (r < 0) {
			player_build_road_end(player);
			return -1;
		} else {
			player->sett->map_cursor_col = MAP_POS_COL(dest);
			player->sett->map_cursor_row = MAP_POS_ROW(dest);
			tiles[pos].flags |= BIT(dir);
			tiles[dest].flags |= BIT(dir_rev);
			player->road_length = 0;
			player_build_road_end(player);
			return 1;
		}
	} else if (MAP_PATHS(dest) == 0) {
		/* No existing paths at destination, build segment. */
		player->road_length += 1;
		tiles[pos].flags |= BIT(dir);
		tiles[dest].flags |= BIT(dir_rev);

		player->sett->map_cursor_col = MAP_POS_COL(dest);
		player->sett->map_cursor_row = MAP_POS_ROW(dest);

		/* TODO Pathway scrolling */

		player->click |= BIT(2);
	} else {
		/* TODO fast split path and connect on double click */
		player->click |= BIT(2);
		return -1;
	}

	return 0;
}

int
player_remove_road_segment(player_t *player, map_pos_t pos, dir_t dir)
{
	map_pos_t dest = MAP_MOVE(pos, dir);
	dir_t dir_rev = DIR_REVERSE(dir);
	map_tile_t *tiles = globals.map.tiles;

	player->road_length -= 1;
	tiles[pos].flags &= ~BIT(dir);
	tiles[dest].flags &= ~BIT(dir_rev);

	player->sett->map_cursor_col = MAP_POS_COL(dest);
	player->sett->map_cursor_row = MAP_POS_ROW(dest);

	/* TODO Pathway scrolling */

	player->click |= BIT(2);

	return 0;
}

void
player_demolish_object(player_t *player)
{
	player_determine_map_cursor_type(player);

	map_pos_t cursor_pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);

	if (player->sett->map_cursor_type == MAP_CURSOR_TYPE_REMOVABLE_FLAG) {
		sfx_play_clip(SFX_CLICK);
		player->click |= BIT(2);
		game_demolish_flag(cursor_pos);
	} else if (player->sett->map_cursor_type == MAP_CURSOR_TYPE_BUILDING) {
		building_t *building = game_get_building(MAP_OBJ_INDEX(cursor_pos));

		if (BUILDING_IS_DONE(building) &&
		    (BUILDING_TYPE(building) == BUILDING_HUT ||
		     BUILDING_TYPE(building) == BUILDING_TOWER ||
		     BUILDING_TYPE(building) == BUILDING_FORTRESS)) {
			/* TODO */
		}

		sfx_play_clip(SFX_AHHH);
		player->click |= BIT(2);
		game_demolish_building(cursor_pos);
	} else {
		sfx_play_clip(SFX_NOT_ACCEPTED);
		player_update_interface(player);
	}
}

/* Build new flag. */
void
player_build_flag(player_t *player)
{
	player->flags &= ~BIT(7);

	player_determine_map_cursor_type(player);

	if (player->sett->panel_btn_type < PANEL_BTN_BUILD_FLAG ||
	    (player->sett->map_cursor_type != MAP_CURSOR_TYPE_CLEAR &&
	     player->sett->map_cursor_type != MAP_CURSOR_TYPE_CLEAR_BY_PATH &&
	     player->sett->map_cursor_type != MAP_CURSOR_TYPE_PATH)) {
		player_update_interface(player);
		return;
	}

	player->click |= BIT(2);

	map_pos_t map_cursor_pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);

	game_build_flag(map_cursor_pos, player->sett);
}

/* Build a new building. The type is stored in globals.building_type. */
static void
build_building(player_t *player)
{
	sfx_play_clip(SFX_ACCEPTED);
	player->click |= BIT(2);

	map_pos_t pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
	game_build_building(pos, globals.building_type, player->sett);

	/* Move cursor to flag. */
	player->sett->map_cursor_col = (player->sett->map_cursor_col + 1) & globals.map.col_mask;
	player->sett->map_cursor_row = (player->sett->map_cursor_row + 1) & globals.map.row_mask;
}

/* Build a mine. */
void
player_build_mine_building(player_t *player)
{
	player_determine_map_cursor_type(player);

	if (player->sett->map_cursor_type != MAP_CURSOR_TYPE_BUILDING) {
		player_close_popup(player);
		if (player->sett->panel_btn_type != PANEL_BTN_BUILD_MINE ||
		    (player->sett->map_cursor_type != MAP_CURSOR_TYPE_CLEAR &&
		     player->sett->map_cursor_type != MAP_CURSOR_TYPE_CLEAR_BY_PATH &&
		     player->sett->map_cursor_type != MAP_CURSOR_TYPE_CLEAR_BY_FLAG)) {
			sfx_play_clip(SFX_NOT_ACCEPTED);
			player_update_interface(player);
		} else {
			build_building(player);
		}
	}
}

/* Build a basic building. */
void
player_build_basic_building(player_t *player)
{
	player_determine_map_cursor_type(player);

	if (player->sett->map_cursor_type != MAP_CURSOR_TYPE_BUILDING) {
		player_close_popup(player);
		if (player->sett->panel_btn_type < PANEL_BTN_BUILD_SMALL ||
		    (player->sett->map_cursor_type != MAP_CURSOR_TYPE_CLEAR &&
		     player->sett->map_cursor_type != MAP_CURSOR_TYPE_CLEAR_BY_PATH &&
		     player->sett->map_cursor_type != MAP_CURSOR_TYPE_CLEAR_BY_FLAG)) {
			sfx_play_clip(SFX_NOT_ACCEPTED);
			player_update_interface(player);
		} else {
			build_building(player);
		}
	}
}

/* Build advanced building. */
void
player_build_advanced_building(player_t *player)
{
	player_determine_map_cursor_type(player);

	if (player->sett->map_cursor_type != MAP_CURSOR_TYPE_BUILDING) {
		player_close_popup(player);
		if (player->sett->panel_btn_type != PANEL_BTN_BUILD_LARGE ||
		    (player->sett->map_cursor_type != MAP_CURSOR_TYPE_CLEAR &&
		     player->sett->map_cursor_type != MAP_CURSOR_TYPE_CLEAR_BY_PATH &&
		     player->sett->map_cursor_type != MAP_CURSOR_TYPE_CLEAR_BY_FLAG)) {
			sfx_play_clip(SFX_NOT_ACCEPTED);
			player_update_interface(player);
		} else {
			build_building(player);
		}
	}
}

/* Build castle. */
void
player_build_castle(player_t *player)
{
	player_determine_map_cursor_type(player);

	if (player->sett->panel_btn_type != PANEL_BTN_BUILD_CASTLE ||
	    player->sett->map_cursor_type != MAP_CURSOR_TYPE_CLEAR) {
		sfx_play_clip(SFX_NOT_ACCEPTED);
		player_update_interface(player);
		return;
	}

	player->flags &= ~BIT(6);
	sfx_play_clip(SFX_ACCEPTED);
	if (BIT_TEST(globals.split, 6)) {
		/* Coop mode */
	} else {
		player->click |= BIT(2);
	}

	int map_cursor_pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
	game_build_castle(map_cursor_pos, player->sett);
}


/* Enqueue a new notification message for player. */
void
player_add_notification(player_sett_t *sett, int type, map_pos_t pos)
{
	sett->flags |= BIT(3); /* Message in queue. */
	for (int i = 0; i < 64; i++) {
		if (sett->msg_queue_type[i] == 0) {
			sett->msg_queue_type[i] = type;
			sett->msg_queue_pos[i] = pos;
			break;
		}
	}
}

/* Set defaults for food distribution priorities. */
void
player_sett_reset_food_priority(player_sett_t *sett)
{
	sett->food_stonemine = 13100;
	sett->food_coalmine = 45850;
	sett->food_ironmine = 45850;
	sett->food_goldmine = 65500;
}

/* Set defaults for planks distribution priorities. */
void
player_sett_reset_planks_priority(player_sett_t *sett)
{
	sett->planks_construction = 65500;
	sett->planks_boatbuilder = 3275;
	sett->planks_toolmaker = 19650;
}

/* Set defaults for steel distribution priorities. */
void
player_sett_reset_steel_priority(player_sett_t *sett)
{
	sett->steel_toolmaker = 45850;
	sett->steel_weaponsmith = 65500;
}

/* Set defaults for coal distribution priorities. */
void
player_sett_reset_coal_priority(player_sett_t *sett)
{
	sett->coal_steelsmelter = 32750;
	sett->coal_goldsmelter = 65500;
	sett->coal_weaponsmith = 52400;
}

/* Set defaults for coal distribution priorities. */
void
player_sett_reset_wheat_priority(player_sett_t *sett)
{
	sett->wheat_pigfarm = 65500;
	sett->wheat_mill = 32750;
}

/* Set defaults for tool production priorities. */
void
player_sett_reset_tool_priority(player_sett_t *sett)
{
	sett->tool_prio[0] = 9825; /* SHOVEL */
	sett->tool_prio[1] = 65500; /* HAMMER */
	sett->tool_prio[2] = 13100; /* ROD */
	sett->tool_prio[3] = 6550; /* CLEAVER */
	sett->tool_prio[4] = 13100; /* SCYTHE */
	sett->tool_prio[5] = 26200; /* AXE */
	sett->tool_prio[6] = 32750; /* SAW */
	sett->tool_prio[7] = 45850; /* PICK */
	sett->tool_prio[8] = 6550; /* PINCER */
}

/* Set defaults for flag priorities. */
void
player_sett_reset_flag_priority(player_sett_t *sett)
{
	sett->flag_prio[RESOURCE_GOLDORE] = 1;
	sett->flag_prio[RESOURCE_GOLDBAR] = 2;
	sett->flag_prio[RESOURCE_WHEAT] = 3;
	sett->flag_prio[RESOURCE_FLOUR] = 4;
	sett->flag_prio[RESOURCE_PIG] = 5;

	sett->flag_prio[RESOURCE_BOAT] = 6;
	sett->flag_prio[RESOURCE_PINCER] = 7;
	sett->flag_prio[RESOURCE_SCYTHE] = 8;
	sett->flag_prio[RESOURCE_ROD] = 9;
	sett->flag_prio[RESOURCE_CLEAVER] = 10;

	sett->flag_prio[RESOURCE_SAW] = 11;
	sett->flag_prio[RESOURCE_AXE] = 12;
	sett->flag_prio[RESOURCE_PICK] = 13;
	sett->flag_prio[RESOURCE_SHOVEL] = 14;
	sett->flag_prio[RESOURCE_HAMMER] = 15;

	sett->flag_prio[RESOURCE_SHIELD] = 16;
	sett->flag_prio[RESOURCE_SWORD] = 17;
	sett->flag_prio[RESOURCE_BREAD] = 18;
	sett->flag_prio[RESOURCE_MEAT] = 19;
	sett->flag_prio[RESOURCE_FISH] = 20;

	sett->flag_prio[RESOURCE_IRONORE] = 21;
	sett->flag_prio[RESOURCE_LUMBER] = 22;
	sett->flag_prio[RESOURCE_COAL] = 23;
	sett->flag_prio[RESOURCE_STEEL] = 24;
	sett->flag_prio[RESOURCE_STONE] = 25;
	sett->flag_prio[RESOURCE_PLANK] = 26;
}

/* Set defaults for inventory priorities. */
void
player_sett_reset_inventory_priority(player_sett_t *sett)
{
	sett->inventory_prio[RESOURCE_WHEAT] = 1;
	sett->inventory_prio[RESOURCE_FLOUR] = 2;
	sett->inventory_prio[RESOURCE_PIG] = 3;
	sett->inventory_prio[RESOURCE_BREAD] = 4;
	sett->inventory_prio[RESOURCE_FISH] = 5;

	sett->inventory_prio[RESOURCE_MEAT] = 6;
	sett->inventory_prio[RESOURCE_LUMBER] = 7;
	sett->inventory_prio[RESOURCE_PLANK] = 8;
	sett->inventory_prio[RESOURCE_BOAT] = 9;
	sett->inventory_prio[RESOURCE_STONE] = 10;

	sett->inventory_prio[RESOURCE_COAL] = 11;
	sett->inventory_prio[RESOURCE_IRONORE] = 12;
	sett->inventory_prio[RESOURCE_STEEL] = 13;
	sett->inventory_prio[RESOURCE_SHOVEL] = 14;
	sett->inventory_prio[RESOURCE_HAMMER] = 15;

	sett->inventory_prio[RESOURCE_ROD] = 16;
	sett->inventory_prio[RESOURCE_CLEAVER] = 17;
	sett->inventory_prio[RESOURCE_SCYTHE] = 18;
	sett->inventory_prio[RESOURCE_AXE] = 19;
	sett->inventory_prio[RESOURCE_SAW] = 20;

	sett->inventory_prio[RESOURCE_PICK] = 21;
	sett->inventory_prio[RESOURCE_PINCER] = 22;
	sett->inventory_prio[RESOURCE_SHIELD] = 23;
	sett->inventory_prio[RESOURCE_SWORD] = 24;
	sett->inventory_prio[RESOURCE_GOLDORE] = 25;
	sett->inventory_prio[RESOURCE_GOLDBAR] = 26;
}

void
player_change_knight_occupation(player_sett_t *sett, int index, int adjust_max, int delta)
{
	int max = (sett->knight_occupation[index] >> 4) & 0xf;
	int min = sett->knight_occupation[index] & 0xf;

	if (adjust_max) {
		max = clamp(min, max + delta, 4);
	} else {
		min = clamp(0, min + delta, max);
	}

	sett->knight_occupation[index] = (max << 4) | min;
}

/* Turn a number of serfs into knight for the given player. */
int
player_promote_serfs_to_knights(player_sett_t *sett, int number)
{
	int promoted = 0;

	for (int i = 1; i < globals.max_ever_serf_index && number > 0; i++) {
		if (SERF_ALLOCATED(i)) {
			serf_t *serf = game_get_serf(i);
			if (serf->state == SERF_STATE_IDLE_IN_STOCK &&
			    SERF_PLAYER(serf) == sett->player_num &&
			    SERF_TYPE(serf) == SERF_GENERIC) {
				inventory_t *inv = game_get_inventory(serf->s.idle_in_stock.inv_index);
				if (inv->resources[RESOURCE_SWORD] > 0 &&
				    inv->resources[RESOURCE_SHIELD] > 0) {
					inv->resources[RESOURCE_SWORD] -= 1;
					inv->resources[RESOURCE_SHIELD] -= 1;
					inv->spawn_priority -= 1;
					inv->serfs[SERF_GENERIC] = 0;

					serf->type = (SERF_KNIGHT_0 << 2) | (serf->type & 3);

					sett->serf_count[SERF_GENERIC] -= 1;
					sett->serf_count[SERF_KNIGHT_0] += 1;
					sett->total_military_score += 1;

					promoted += 1;
					number -= 1;
				}
			}
		}
	}

	return promoted;
}

static int
available_knights_at_pos(player_sett_t *sett, map_pos_t pos, int index, int dist)
{
	const int min_level_hut[] = { 1, 1, 2, 2, 3 };
	const int min_level_tower[] = { 1, 2, 3, 4, 6 };
	const int min_level_fortress[] = { 1, 3, 6, 9, 12 };

	if (MAP_OWNER(pos) != sett->player_num || MAP_WATER(pos) ||
	    MAP_OBJ(pos) < MAP_OBJ_SMALL_BUILDING ||
	    MAP_OBJ(pos) > MAP_OBJ_CASTLE) {
		return index;
	}

	int bld_index = MAP_OBJ_INDEX(pos);
	for (int i = 0; i < index; i++) {
		if (sett->attacking_buildings[i] == bld_index) {
			return index;
		}
	}

	building_t *building = game_get_building(bld_index);
	if (!BUILDING_IS_DONE(building) ||
	    BUILDING_IS_BURNING(building)) {
		return index;
	}

	const int *min_level = NULL;
	switch (BUILDING_TYPE(building)) {
	case BUILDING_HUT: min_level = min_level_hut; break;
	case BUILDING_TOWER: min_level = min_level_tower; break;
	case BUILDING_FORTRESS: min_level = min_level_fortress; break;
	default: return index; break;
	}

	if (index >= 64) return index;

	sett->attacking_buildings[index] = bld_index;

	int state = BUILDING_STATE(building);
	int knights_present = building->stock[0].available;
	int to_send = knights_present - min_level[sett->knight_occupation[state] & 0xf];

	if (to_send > 0) sett->attacking_knights[dist] += to_send;

	return index + 1;
}

int
player_knights_available_for_attack(player_sett_t *sett, map_pos_t pos)
{
	/* Reset counters. */
	for (int i = 0; i < 4; i++) sett->attacking_knights[i] = 0;

	int index = 0;

	/* Iterate each shell around the position.*/
	for (int i = 0; i < 32; i++) {
		pos = MAP_MOVE_RIGHT(pos);
		for (int j = 0; j < i+1; j++) {
			index = available_knights_at_pos(sett, pos, index, i >> 3);
			pos = MAP_MOVE_DOWN(pos);
		}
		for (int j = 0; j < i+1; j++) {
			index = available_knights_at_pos(sett, pos, index, i >> 3);
			pos = MAP_MOVE_LEFT(pos);
		}
		for (int j = 0; j < i+1; j++) {
			index = available_knights_at_pos(sett, pos, index, i >> 3);
			pos = MAP_MOVE_UP_LEFT(pos);
		}
		for (int j = 0; j < i+1; j++) {
			index = available_knights_at_pos(sett, pos, index, i >> 3);
			pos = MAP_MOVE_UP(pos);
		}
		for (int j = 0; j < i+1; j++) {
			index = available_knights_at_pos(sett, pos, index, i >> 3);
			pos = MAP_MOVE_RIGHT(pos);
		}
		for (int j = 0; j < i+1; j++) {
			index = available_knights_at_pos(sett, pos, index, i >> 3);
			pos = MAP_MOVE_DOWN_RIGHT(pos);
		}
	}

	sett->attacking_building_count = index;

	sett->total_attacking_knights = 0;
	for (int i = 0; i < 4; i++) {
		sett->total_attacking_knights +=
			sett->attacking_knights[i];
	}

	return sett->total_attacking_knights;
}

void
player_start_attack(player_sett_t *sett)
{
	const int min_level_hut[] = { 1, 1, 2, 2, 3 };
	const int min_level_tower[] = { 1, 2, 3, 4, 6 };
	const int min_level_fortress[] = { 1, 3, 6, 9, 12 };

	building_t *target = game_get_building(sett->building_attacked);
	if (!BUILDING_IS_DONE(target) ||
	    (BUILDING_TYPE(target) != BUILDING_HUT &&
	     BUILDING_TYPE(target) != BUILDING_TOWER &&
	     BUILDING_TYPE(target) != BUILDING_FORTRESS &&
	     BUILDING_TYPE(target) != BUILDING_CASTLE) ||
	    !BUILDING_IS_ACTIVE(target) ||
	    BUILDING_STATE(target) != 3) {
		return;
	}

	for (int i = 0; i < sett->attacking_building_count; i++) {
		/* TODO building index may not be valid any more(?). */
		building_t *b = game_get_building(sett->attacking_buildings[i]);
		if (BUILDING_IS_BURNING(b) ||
		    MAP_OWNER(b->pos) != sett->player_num) {
			continue;
		}

		map_pos_t flag_pos = MAP_MOVE_DOWN_RIGHT(b->pos);
		if (MAP_SERF_INDEX(flag_pos) != 0) {
			/* Check if building is under siege. */
			serf_t *s = game_get_serf(MAP_SERF_INDEX(flag_pos));
			if (SERF_PLAYER(s) != sett->player_num) continue;
		}

		const int *min_level = NULL;
		switch (BUILDING_TYPE(b)) {
		case BUILDING_HUT: min_level = min_level_hut; break;
		case BUILDING_TOWER: min_level = min_level_tower; break;
		case BUILDING_FORTRESS: min_level = min_level_fortress; break;
		default: continue; break;
		}

		int state = BUILDING_STATE(b);
		int knights_present = b->stock[0].available;
		int to_send = knights_present - min_level[sett->knight_occupation[state] & 0xf];

		for (int j = 0; j < to_send; j++) {
			/* Find most approriate knight to send according to player settings. */
			int best_type = PLAYER_SEND_STRONGEST(sett) ? SERF_KNIGHT_0 : SERF_KNIGHT_4;
			int best_index = -1;

			int knight_index = b->serf_index;
			while (knight_index != 0) {
				serf_t *knight = game_get_serf(knight_index);
				if (PLAYER_SEND_STRONGEST(sett)) {
					if (SERF_TYPE(knight) >= best_type) {
						best_index = knight_index;
						best_type = SERF_TYPE(knight);
					}
				} else {
					if (SERF_TYPE(knight) <= best_type) {
						best_index = knight_index;
						best_type = SERF_TYPE(knight);
					}
				}

				knight_index = knight->s.defending.next_knight;
			}

			/* Unlink knight from list. */
			int *def_index = &b->serf_index;
			serf_t *def_serf = game_get_serf(*def_index);
			while (*def_index != best_index) {
				def_index = &def_serf->s.defending.next_knight;
				def_serf = game_get_serf(*def_index);
			}
			*def_index = def_serf->s.defending.next_knight;
			b->stock[0].available -= 1;

			target->progress |= BIT(0);

			/* Calculate distance to target. */
			int dist_col = (MAP_POS_COL(target->pos) -
					MAP_POS_COL(def_serf->pos)) & globals.map.col_mask;
			if (dist_col >= globals.map.cols/2) dist_col -= globals.map.cols;

			int dist_row = (MAP_POS_ROW(target->pos) -
					MAP_POS_ROW(def_serf->pos)) & globals.map.row_mask;
			if (dist_row >= globals.map.rows/2) dist_row -= globals.map.rows;

			/* Send this serf off to fight. */
			serf_log_state_change(def_serf, SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT);
			def_serf->state = SERF_STATE_KNIGHT_LEAVE_FOR_WALK_TO_FIGHT;
			def_serf->s.leave_for_walk_to_fight.dist_col = dist_col;
			def_serf->s.leave_for_walk_to_fight.dist_row = dist_row;
			def_serf->s.leave_for_walk_to_fight.field_D = 0;
			def_serf->s.leave_for_walk_to_fight.field_E = 0;
			def_serf->s.leave_for_walk_to_fight.next_state = SERF_STATE_KNIGHT_FREE_WALKING;

			sett->knights_attacking -= 1;
			if (sett->knights_attacking == 0) return;
		}
	}
}
