/* player.c */

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
		if (BIT_TEST(sett->flags, 0)) { /* Has castle */
			*panel_btn = PANEL_BTN_BUILD_MINE;
		}
	} else {
		if (BIT_TEST(sett->flags, 0)) { /* Has castle */
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

		if (BIT_TEST(sett->flags, 0)) { /* Has castle */
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
		map_pos[i] = (init_pos + globals.spiral_pos_pattern[i]) & globals.map_index_mask;
	}
}

/* Return the cursor type and various related values of a map_pos_t. */
static void
get_map_cursor_type(const player_sett_t *sett, map_pos_t pos, panel_btn_t *panel_btn,
		    int *build_flags, int *cursor_type, int *height_after_level)
{
	map_pos_t map_pos[1+6+12+18];
	populate_circular_map_pos_array(map_pos, pos, 1+6+12+18);

	int player = sett->player_num;
	if (!BIT_TEST(sett->flags, 0)) player = -1; /* Has no castle */

	if (player >= 0 &&
	    (!MAP_HAS_OWNER(map_pos[0]) ||
	     MAP_OWNER(map_pos[0]) != player)) {
		return;
	}

	if (map_space_from_obj[MAP_OBJ(map_pos[0])] == MAP_SPACE_FLAG) {
		if (BIT_TEST(MAP_PATHS(map_pos[0]), DIR_UP_LEFT) &&
		    map_space_from_obj[MAP_OBJ(map_pos[1+DIR_UP_LEFT])] >= MAP_SPACE_SMALL_BUILDING) {
			*cursor_type = 1;
			return;
		}

		if (MAP_PATHS(map_pos[0]) == 0) {
			*cursor_type = 2;
			return;
		}

		flag_t *flag = game_get_flag(MAP_OBJ_INDEX(map_pos[0]));
		int connected = 0;
		void *other_end = NULL;

		for (int i = DIR_UP; i >= DIR_RIGHT; i--) {
			if (BIT_TEST(flag->path_con, i)) {
				if (!BIT_TEST(flag->endpoint, i)) {
					*cursor_type = 1;
					return;
				}

			        connected += 1;

				if (other_end != NULL) {
					if (flag->other_endpoint.v[i] == other_end) {
						*cursor_type = 1;
						return;
					}
				} else {
					other_end = flag->other_endpoint.v[i];
				}

				if (i >= DIR_UP_LEFT) {
					i = DIR_DOWN_RIGHT;
				} else {
					break;
				}
			}
		}

		if (connected == 2) *cursor_type = 2;
		else *cursor_type = 1;
	} else if (map_space_from_obj[MAP_OBJ(map_pos[0])] < MAP_SPACE_SMALL_BUILDING) {
		int paths = MAP_PATHS(map_pos[0]);
		if (paths == 0) {
			if (map_space_from_obj[MAP_OBJ(map_pos[1+DIR_DOWN_RIGHT])] == MAP_SPACE_FLAG) {
				*cursor_type = 5;
			} else if (MAP_PATHS(map_pos[1+DIR_DOWN_RIGHT]) == 0) {
				*cursor_type = 7;
			} else {
				*cursor_type = 6;
			}
		} else if (paths == BIT(DIR_DOWN_RIGHT) ||
			   paths == BIT(DIR_UP_LEFT)) {
			return;
		} else {
			*cursor_type = 4;
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
				if (*cursor_type == 4) return;
				found = 1;
				break;
			}
		}

		if (!found) {
			*build_flags &= ~BIT(1);
			if (BIT_TEST(sett->flags, 0)) { /* Has castle */
				*panel_btn = PANEL_BTN_BUILD_FLAG;
				if (*cursor_type == 4) return;
			}
		}

		for (int i = 0; i < 6; i++) {
			if (map_space_from_obj[MAP_OBJ(map_pos[1+i])] >= MAP_SPACE_SMALL_BUILDING) return;
		}

		if (*cursor_type != 5) {
			if (map_space_from_obj[MAP_OBJ(map_pos[1+DIR_DOWN_RIGHT])] != MAP_SPACE_OPEN) return;
		}

		/* bleh */
		if (map_space_from_obj[MAP_OBJ(map_pos[7])] == MAP_SPACE_FLAG ||
		    map_space_from_obj[MAP_OBJ(map_pos[8])] == MAP_SPACE_FLAG ||
		    map_space_from_obj[MAP_OBJ(map_pos[14])] == MAP_SPACE_FLAG ||
		    map_space_from_obj[MAP_OBJ(map_pos[1+DIR_RIGHT])] == MAP_SPACE_FLAG ||
		    map_space_from_obj[MAP_OBJ(map_pos[1+DIR_DOWN])] == MAP_SPACE_FLAG) {
			return;
		}

		/* bleh */
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
	} else if (map_space_from_obj[MAP_OBJ(map_pos[0])] != MAP_SPACE_CASTLE) {
		building_t *bld = game_get_building(MAP_OBJ_INDEX(map_pos[0]));
		if (BIT_TEST(bld->serf, 5)) return; /* Building burning */

		*cursor_type = 3;

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

	player->sett->map_cursor_type = 0;
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
					int build_flags, cursor_type, height_after_level;
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
				case 0:
					player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
					if (BIT_TEST(player->sett->flags, 0)) { /* Has castle */
						player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					} else {
						player->panel_btns[1] = PANEL_BTN_GROUND_ANALYSIS;
					}
					player->map_cursor_sprites[0].sprite = 32;
					player->map_cursor_sprites[2].sprite = 33;
					break;
				case 1:
					player->panel_btns[0] = PANEL_BTN_BUILD_ROAD;
					player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					player->map_cursor_sprites[0].sprite = 51;
					player->map_cursor_sprites[2].sprite = 33;
					break;
				case 2:
					player->panel_btns[0] = PANEL_BTN_BUILD_ROAD;
					player->panel_btns[1] = PANEL_BTN_DESTROY;
					player->map_cursor_sprites[0].sprite = 51;
					player->map_cursor_sprites[2].sprite = 33;
					break;
				case 3:
					player->panel_btns[0] = player->sett->panel_btn_type;
					player->panel_btns[1] = PANEL_BTN_DESTROY;
					player->map_cursor_sprites[0].sprite = 46 + player->sett->panel_btn_type;
					player->map_cursor_sprites[2].sprite = 33;
					break;
				case 4:
					player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
					player->panel_btns[1] = PANEL_BTN_DESTROY_ROAD;
					player->map_cursor_sprites[0].sprite = 52;
					player->map_cursor_sprites[2].sprite = 33;
					if (player->sett->panel_btn_type != PANEL_BTN_BUILD_INACTIVE) {
						player->panel_btns[0] = PANEL_BTN_BUILD_FLAG;
						player->map_cursor_sprites[0].sprite = 47;
					}
					break;
				case 5:
					if (player->sett->panel_btn_type < PANEL_BTN_BUILD_MINE) {
						/* TODO */
						LOGD("player", "cursor type: unhandled 5 case.");
					} else {
						player->panel_btns[0] = player->sett->panel_btn_type;
						player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
						player->map_cursor_sprites[0].sprite = 46 + player->sett->panel_btn_type;
						player->map_cursor_sprites[2].sprite = 33;
					}
					break;
				case 6:
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
				case 7:
					player->panel_btns[0] = player->sett->panel_btn_type;
					if (BIT_TEST(player->sett->flags, 0)) { /* Has castle */
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

	if (player->sett->map_cursor_type != 1 &&
	    player->sett->map_cursor_type != 2) {
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

	map_1_t *map = globals.map_mem2_ptr;
	map_pos_t pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);

	for (int i = 0; i < player->road_length; i++) {
		dir_t backtrack_dir = -1;
		for (dir_t d = 0; d < 6; d++) {
			if (BIT_TEST(map[pos].flags, d)) {
				backtrack_dir = d;
				break;
			}
		}

		map_pos_t next_pos = MAP_MOVE(pos, backtrack_dir);

		map[pos].flags &= ~BIT(backtrack_dir);
		map[next_pos].flags &= ~BIT(DIR_REVERSE(backtrack_dir));
		pos = next_pos;
	}

	/* TODO set_map_redraw(); */
}

/* Connect a road under construction to an existing flag. */
int
player_build_road_connect_flag(player_t *player, map_1_t *map, map_pos_t clk_pos, dir_t out_dir)
{
	if (!MAP_HAS_OWNER(clk_pos) || MAP_OWNER(clk_pos) != player->sett->player_num) {
		return -1;
	}

	map_pos_t next_pos = clk_pos;
	dir_t in_dir = -1;

	flag_t *src = game_get_flag(MAP_OBJ_INDEX(clk_pos));

	int paths = BIT(out_dir);
	int test = 0;

	/* Backtrack along path to other flag. Test along the way
	   whether the path is on ground or in water. */
	for (int i = 0; i < player->road_length + 1; i++) {
		if (BIT_TEST(paths, DIR_RIGHT)) {
			if (MAP_TYPE_UP(next_pos) < 4 &&
			    MAP_TYPE_UP(MAP_MOVE_UP(next_pos)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_LEFT;
			next_pos = MAP_MOVE_RIGHT(next_pos);
		} else if (BIT_TEST(paths, DIR_DOWN_RIGHT)) {
			if (MAP_TYPE_UP(next_pos) < 4 &&
			    MAP_TYPE_DOWN(next_pos) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_UP_LEFT;
			next_pos = MAP_MOVE_DOWN_RIGHT(next_pos);
		} else if (BIT_TEST(paths, DIR_DOWN)) {
			if (MAP_TYPE_UP(next_pos) < 4 &&
			    MAP_TYPE_DOWN(MAP_MOVE_LEFT(next_pos)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_UP;
			next_pos = MAP_MOVE_DOWN(next_pos);
		} else if (BIT_TEST(paths, DIR_LEFT)) {
			if (MAP_TYPE_DOWN(MAP_MOVE_LEFT(next_pos)) < 4 &&
			    MAP_TYPE_UP(MAP_MOVE_UP(next_pos)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_RIGHT;
			next_pos = MAP_MOVE_LEFT(next_pos);
		} else if (BIT_TEST(paths, DIR_UP_LEFT)) {
			if (MAP_TYPE_UP(MAP_MOVE_UP_LEFT(next_pos)) < 4 &&
			    MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(next_pos)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_DOWN_RIGHT;
			next_pos = MAP_MOVE_UP_LEFT(next_pos);
		} else if (BIT_TEST(paths, DIR_UP)) {
			if (MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(next_pos)) < 4 &&
			    MAP_TYPE_UP(MAP_MOVE_RIGHT(next_pos)) < 4) {
				test |= BIT(1);
			} else {
				test |= BIT(0);
			}
			in_dir = DIR_DOWN;
			next_pos = MAP_MOVE_UP(next_pos);
		}

		if (!MAP_HAS_OWNER(next_pos) || MAP_OWNER(next_pos) != player->sett->player_num) {
			return -1;
		}

		paths = MAP_PATHS(next_pos) & ~BIT(in_dir);
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
	flag_t *dest = game_get_flag(MAP_OBJ_INDEX(next_pos));

	src->path_con |= BIT(out_dir);
	src->endpoint |= BIT(out_dir);
	src->transporter &= ~BIT(out_dir);

	dest->path_con |= BIT(in_dir);
	dest->endpoint |= BIT(in_dir);
	dest->transporter &= ~BIT(in_dir);

	if (water_path) {
		src->endpoint &= ~BIT(out_dir);
		dest->endpoint &= ~BIT(in_dir);
	}

	src->other_end_dir[out_dir] = (src->other_end_dir[out_dir] & 0xc7) | (in_dir << 3);
	dest->other_end_dir[in_dir] = (dest->other_end_dir[in_dir] & 0xc7) | (out_dir << 3);

	int len = game_get_road_length_value(player->road_length + 1);

	src->length[out_dir] = len;
	dest->length[in_dir] = len;

	src->other_endpoint.f[out_dir] = dest;
	dest->other_endpoint.f[in_dir] = src;

	return 0;
}

void
player_demolish_object(player_t *player)
{
	player_determine_map_cursor_type(player);

	if (player->sett->map_cursor_type == 2) {
		/* TODO */
	} else if (player->sett->map_cursor_type == 3) {
		map_pos_t cursor_pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
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

/* Find a transporter at pos and change it to state. */
static int
change_transporter_state_at_pos(map_pos_t pos, serf_state_t state)
{
	for (int i = 1; i < globals.max_ever_serf_index; i++) {
		if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
			serf_t *serf = game_get_serf(i);
			if (serf->pos == pos &&
			    (serf->state == SERF_STATE_WAKE_AT_FLAG ||
			     serf->state == SERF_STATE_WAKE_ON_PATH ||
			     serf->state == SERF_STATE_WAIT_IDLE_ON_PATH ||
			     serf->state == SERF_STATE_IDLE_ON_PATH)) {
				serf_log_state_change(serf, state);
				serf->state = state;
				return SERF_INDEX(serf);
			}
		}
	}

	return -1;
}

static int
wake_transporter_at_flag(map_pos_t pos)
{
	return change_transporter_state_at_pos(pos, SERF_STATE_WAKE_AT_FLAG);
}

static int
wake_transporter_on_path(map_pos_t pos)
{
	return change_transporter_state_at_pos(pos, SERF_STATE_WAKE_ON_PATH);
}

typedef struct {
	int path_len;
	int serf_count;
	int flag_index;
	dir_t flag_dir;
	int serfs[16];
} serf_path_info_t;

static void
fill_path_serf_info(map_pos_t pos, dir_t dir, serf_path_info_t *data)
{
	if (MAP_IDLE_SERF(pos)) wake_transporter_at_flag(pos);

	int serf_count = 0;
	int path_len = 0;

	/* Handle first position. */
	if (MAP_SERF_INDEX(pos) != 0) {
		serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));
		if (serf->state == SERF_STATE_TRANSPORTING &&
		    serf->s.walking.wait_counter != -1) {
			int d = serf->s.walking.dir;
			if (d < 0) d += 6;

			if (dir == d) {
				serf->s.walking.wait_counter = 0;
				data->serfs[serf_count++] = SERF_INDEX(serf);
			}
		}
	}

	/* Trace along the path to the flag at the other end. */
	int paths = 0;
	while (1) {
		path_len += 1;
		pos = MAP_MOVE(pos, dir);
		paths = MAP_PATHS(pos);
		paths &= ~BIT(DIR_REVERSE(dir));

		if (MAP_HAS_FLAG(pos)) break;

		/* Find out which direction the path follows. */
		for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
			if (BIT_TEST(paths, d)) {
				dir = d;
				break;
			}
		}

		/* Check if there is a transporter waiting here. */
		if (MAP_IDLE_SERF(pos)) {
			int index = wake_transporter_on_path(pos);
			if (index >= 0) data->serfs[serf_count++] = index;
		}

		/* Check if there is a serf occupying this space. */
		if (MAP_SERF_INDEX(pos) != 0) {
			serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));
			if (serf->state == SERF_STATE_TRANSPORTING &&
			    serf->s.walking.wait_counter != -1) {
				serf->s.walking.wait_counter = 0;
				data->serfs[serf_count++] = SERF_INDEX(serf);
			}
		}
	}

	/* Handle last position. */
	if (MAP_SERF_INDEX(pos) != 0) {
		serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));
		if ((serf->state == SERF_STATE_TRANSPORTING &&
		     serf->s.walking.wait_counter != -1) ||
		    serf->state == SERF_STATE_DELIVERING) {
			int d = serf->s.walking.dir;
			if (d < 0) d += 6;

			if (d == DIR_REVERSE(dir)) {
				serf->s.walking.wait_counter = 0;
				data->serfs[serf_count++] = SERF_INDEX(serf);
			}
		}
	}

	/* Fill the rest of the struct. */
	data->path_len = path_len;
	data->serf_count = serf_count;
	data->flag_index = MAP_OBJ_INDEX(pos);
	data->flag_dir = DIR_REVERSE(dir);
}

static void
restore_path_serf_info(flag_t *flag, dir_t dir, serf_path_info_t *data)
{
	const int max_path_serfs[] = { 1, 2, 3, 4, 6, 8, 11, 15 };

	flag_t *other_flag = game_get_flag(data->flag_index);
	dir_t other_dir = data->flag_dir;

	flag->path_con |= BIT(dir);
	flag->endpoint &= ~BIT(dir);

	if (BIT_TEST(other_flag->endpoint, other_dir)) {
		flag->endpoint |= BIT(dir);
	}

	other_flag->transporter &= ~BIT(other_dir);
	flag->transporter &= ~BIT(dir);

	int len = game_get_road_length_value(data->path_len);

	flag->length[dir] = len;
	other_flag->length[other_dir] = (0x80 & other_flag->length[other_dir]) | len;

	if (BIT_TEST(other_flag->length[other_dir], 7)) {
		flag->length[dir] |= BIT(7);
	}

	flag->other_end_dir[dir] = (flag->other_end_dir[dir] & 0xc7) | (other_dir << 3);
	other_flag->other_end_dir[other_dir] = (other_flag->other_end_dir[other_dir] & 0xc7) | (dir << 3);

	flag->other_endpoint.f[dir] = other_flag;
	other_flag->other_endpoint.f[other_dir] = flag;

	int max_serfs = max_path_serfs[(len >> 4) & 7];
	if (BIT_TEST(flag->length[dir], 7)) max_serfs -= 1;

	if (data->serf_count > max_serfs) {
		for (int i = 0; i < data->serf_count - max_serfs; i++) {
			serf_t *serf = game_get_serf(data->serfs[i]);
			if (serf->state != SERF_STATE_WAKE_ON_PATH) {
				serf->s.walking.wait_counter = -1;
				if (serf->s.walking.res != 0) {
					resource_type_t res = serf->s.walking.res-1;
					serf->s.walking.res = 0;

					/* Remove gold from total count. */
					if (res == RESOURCE_GOLDBAR ||
					    res == RESOURCE_GOLDORE) {
						globals.map_gold_deposit -= 1;
					}

					flag_cancel_transported_stock(game_get_flag(serf->s.walking.dest), res+1);
				}
			} else {
				serf_log_state_change(serf, SERF_STATE_WAKE_AT_FLAG);
				serf->state = SERF_STATE_WAKE_AT_FLAG;
			}
		}
	}

	if (min(data->serf_count, max_serfs) > 0) {
		/* There is still transporters on the paths. */
		flag->transporter |= BIT(dir);
		other_flag->transporter |= BIT(other_dir);

		flag->length[dir] |= min(data->serf_count, max_serfs);
		other_flag->length[other_dir] |= min(data->serf_count, max_serfs);
	}
}

/* Build flag on existing path. Path must be split in two segments. */
static void
build_flag_split_path(map_pos_t pos)
{
	/* Find directions of path segments to be split. */
	dir_t path_1_dir = -1;
	for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
		if (BIT_TEST(MAP_PATHS(pos), d)) {
			path_1_dir = d;
			break;
		}
	}

	dir_t path_2_dir = -1;
	for (dir_t d = path_1_dir+1; d <= DIR_UP; d++) {
		if (BIT_TEST(MAP_PATHS(pos), d)) {
			path_2_dir = d;
			break;
		}
	}

	/* If last segment direction is UP LEFT it could
	   be to a building and the real path is at UP. */
	if (path_2_dir == DIR_UP_LEFT &&
	    BIT_TEST(MAP_PATHS(pos), DIR_UP)) {
		path_2_dir = DIR_UP;
	}

	serf_path_info_t path_1_data;
	serf_path_info_t path_2_data;

	fill_path_serf_info(pos, path_1_dir, &path_1_data);
	fill_path_serf_info(pos, path_2_dir, &path_2_data);

	flag_t *flag_2 = game_get_flag(path_2_data.flag_index);
	dir_t dir_2 = path_2_data.flag_dir;

	int select = -1;
	if (BIT_TEST(flag_2->length[dir_2], 7)) {
		for (int i = 1; i < globals.max_ever_serf_index; i++) {
			if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
				serf_t *serf = game_get_serf(i);

				/* TODO ??? */
				if (serf->state == SERF_STATE_WALKING) {
					if (serf->s.walking.dest == path_1_data.flag_index &&
					    serf->s.walking.res == path_1_data.flag_dir) {
						select = 0;
						break;
					} else if (serf->s.walking.dest == path_2_data.flag_index &&
						   serf->s.walking.res == path_2_data.flag_dir) {
						select = 1;
						break;
					}
				} else if (serf->state == SERF_STATE_READY_TO_LEAVE_INVENTORY) {
					if (serf->s.ready_to_leave_inventory.dest == path_1_data.flag_index &&
					    serf->s.ready_to_leave_inventory.mode == path_1_data.flag_dir) {
						select = 0;
						break;
					} else if (serf->s.ready_to_leave_inventory.dest == path_2_data.flag_index &&
						   serf->s.ready_to_leave_inventory.mode == path_2_data.flag_dir) {
						select = 1;
						break;
					}
				} else if ((serf->state == SERF_STATE_READY_TO_LEAVE ||
					    serf->state == SERF_STATE_LEAVING_BUILDING) &&
					   serf->s.leaving_building.next_state == SERF_STATE_WALKING) {
					if (serf->s.leaving_building.dest == path_1_data.flag_index &&
					    serf->s.leaving_building.field_B == path_1_data.flag_dir) {
						select = 0;
						break;
					} else if (serf->s.leaving_building.dest == path_2_data.flag_index &&
						   serf->s.leaving_building.field_B == path_2_data.flag_dir) {
						select = 1;
						break;
					}
				} else {
					NOT_REACHED();
				}
			}
		}

		serf_path_info_t *path_data = &path_1_data;
		if (select == 0) path_data = &path_2_data;

		flag_t *selected_flag = game_get_flag(path_data->flag_index);
		selected_flag->length[path_data->flag_dir] &= ~BIT(7);
	}

	flag_t *flag = game_get_flag(MAP_OBJ_INDEX(pos));

	restore_path_serf_info(flag, path_1_dir, &path_1_data);
	restore_path_serf_info(flag, path_2_dir, &path_2_data);
}

/* Build new flag. */
void
player_build_flag(player_t *player)
{
	player->flags &= ~BIT(7);

	player_determine_map_cursor_type(player);

	if (player->sett->panel_btn_type < PANEL_BTN_BUILD_FLAG ||
	    (player->sett->map_cursor_type != 7 &&
	     player->sett->map_cursor_type != 6 &&
	     player->sett->map_cursor_type != 4)) {
		player_update_interface(player);
		return;
	}

	player->click |= BIT(2);

	flag_t *flag;
	int flg_index;
	int r = game_alloc_flag(&flag, &flg_index);
	if (r < 0) return;

	flag->path_con = player->sett->player_num << 6;

	map_pos_t map_cursor_pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
	map_1_t *map = globals.map_mem2_ptr;
	map_2_t *map_data = MAP_2_DATA(map);

	flag->pos = map_cursor_pos;
	map_set_object(map_cursor_pos, MAP_OBJ_FLAG);
	map[map_cursor_pos].flags |= BIT(7);
	/* move_map_resources(..); */
	map_data[map_cursor_pos].u.index = flg_index;

	if (player->sett->map_cursor_type == 4) { /* built on existing road */
		build_flag_split_path(map_cursor_pos);
	}
}

/* Build a new building. The type is stored in globals.building_type. */
static void
build_building(player_t *player, map_obj_t obj_type)
{
	const int construction_cost[] = {
		0, 0, 2, 0, 2, 0, 3, 0, 2, 0,
		4, 1, 5, 0, 5, 0, 5, 0,
		2, 0, 4, 3, 1, 1, 4, 1, 2, 1, 4, 1, 3, 1, 2, 1,
		3, 2, 3, 2, 3, 3, 2, 1, 2, 3, 5, 5, 4, 1
	};

	building_type_t bld_type = globals.building_type;

	sfx_play_clip(SFX_ACCEPTED);
	player->click |= BIT(2);

	if (bld_type == BUILDING_STOCK) {
		/* TODO Check that more stocks are allowed to be built */
	}

	building_t *bld;
	int bld_index;
	int r = game_alloc_building(&bld, &bld_index);
	if (r < 0) return;

	flag_t *flag = NULL;
	int flg_index = 0;
	if (player->sett->map_cursor_type != 5) {
		r = game_alloc_flag(&flag, &flg_index);
		if (r < 0) {
			game_free_building(bld_index);
			return;
		}
	}

	if (/*!BIT_TEST(player->sett->field_163, 0)*/0) {
		switch (bld_type) {
		case BUILDING_LUMBERJACK:
			if (player->sett->lumberjack_index == 0) {
				player->sett->lumberjack_index = bld_index;
			}
			break;
		case BUILDING_SAWMILL:
			if (player->sett->sawmill_index == 0) {
				player->sett->sawmill_index = bld_index;
			}
			break;
		case BUILDING_STONECUTTER:
			if (player->sett->stonecutter_index == 0) {
				player->sett->stonecutter_index = bld_index;
			}
			break;
		default:
			break;
		}
	}

	/* request_redraw_if_pos_visible(player->sett->map_cursor_col, player->sett->map_cursor_row); */

	map_1_t *map = globals.map_mem2_ptr;
	map_2_t *map_data = MAP_2_DATA(map);

	map_pos_t pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
	bld->u.s.level = player->sett->building_height_after_level;
	bld->pos = pos;
	player->sett->incomplete_building_count[bld_type] += 1;
	bld->bld = BIT(7) | (bld_type << 2) | player->sett->player_num; /* bit 7: Unfinished building */
	bld->progress = 0;
	if (obj_type == 2) bld->progress = 1;

	if (player->sett->map_cursor_type != 5) {
		flag->path_con = player->sett->player_num << 6;
	} else {
		flg_index = MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(pos));
		flag = game_get_flag(flg_index);
	}

	flag->pos = MAP_MOVE_DOWN_RIGHT(pos);

	bld->flg_index = flg_index;
	flag->other_endpoint.b[DIR_UP_LEFT] = bld;
	flag->endpoint |= BIT(6);
	flag->bld_flags = BIT(1);
	flag->stock1_prio = 0;
	flag->bld2_flags = BIT(4);
	flag->stock2_prio = 0;
	bld->u.s.planks_needed = construction_cost[2*bld_type];
	bld->u.s.stone_needed = construction_cost[2*bld_type+1];

	/* move_map_resources(pos, map_data); */
	/* TODO Resources should be moved, just set them to zero for now */
	map_data[pos].u.s.resource = 0;
	map_data[pos].u.s.field_1 = 0;

	map_data[pos].u.index = bld_index;
	map_set_object(pos, obj_type);
	map[pos].flags |= BIT(1) | BIT(6);

	if (player->sett->map_cursor_type != 5) {
		map_set_object(MAP_MOVE_DOWN_RIGHT(pos), MAP_OBJ_FLAG);
		map[MAP_MOVE_DOWN_RIGHT(pos)].flags |= BIT(4) | BIT(7);
		/* move_map_resources(MAP_MOVE_DOWN_RIGHT(pos), map_data); */
		map_data[MAP_MOVE_DOWN_RIGHT(pos)].u.index = flg_index;
	}

	if (player->sett->map_cursor_type == 6) {
		build_flag_split_path(MAP_MOVE_DOWN_RIGHT(pos));
	}

	/* Move cursor to flag. */
	player->sett->map_cursor_col = (player->sett->map_cursor_col + 1) & globals.map_col_mask;
	player->sett->map_cursor_row = (player->sett->map_cursor_row + 1) & globals.map_row_mask;
}

/* Build a mine. */
void
player_build_mine_building(player_t *player)
{
	player_determine_map_cursor_type(player);

	if (player->sett->map_cursor_type == 3) {
		if (player->sett->panel_btn_type == PANEL_BTN_BUILD_MINE &&
		    BIT_TEST(player->click, 3)) { /* Special click */
			player_close_popup(player);
			sfx_play_clip(SFX_ACCEPTED);
			/* sub_58DA8(player->sett->map_cursor_col, player->sett->map_cursor_row); */
			/* sub_41D58(); */
		}
	} else {
		player_close_popup(player);
		if (player->sett->panel_btn_type != PANEL_BTN_BUILD_MINE ||
		    (player->sett->map_cursor_type != 7 &&
		     player->sett->map_cursor_type != 6 &&
		     player->sett->map_cursor_type != 5)) {
			sfx_play_clip(SFX_NOT_ACCEPTED);
			player_update_interface(player);
		} else {
			build_building(player, MAP_OBJ_SMALL_BUILDING);
		}
	}
}

/* Build a basic building. */
void
player_build_basic_building(player_t *player)
{
	player_determine_map_cursor_type(player);

	if (player->sett->map_cursor_type == 3) {
		if (player->sett->panel_btn_type >= PANEL_BTN_BUILD_SMALL &&
		    BIT_TEST(player->click, 3)) { /* Special click */
			player_close_popup(player);
			sfx_play_clip(SFX_ACCEPTED);
			/* sub_58DA8(player->sett->map_cursor_col, player->sett->map_cursor_row); */
			/* sub_41D58(); */
		}
	} else {
		player_close_popup(player);
		if (player->sett->panel_btn_type < PANEL_BTN_BUILD_SMALL ||
		    (player->sett->map_cursor_type != 7 &&
		     player->sett->map_cursor_type != 6 &&
		     player->sett->map_cursor_type != 5)) {
			sfx_play_clip(SFX_NOT_ACCEPTED);
			player_update_interface(player);
		} else {
			build_building(player, MAP_OBJ_SMALL_BUILDING);
		}
	}
}

/* Build advanced building. */
void
player_build_advanced_building(player_t *player)
{
	player_determine_map_cursor_type(player);

	if (player->sett->map_cursor_type == 3) {
		if (player->sett->panel_btn_type >= PANEL_BTN_BUILD_LARGE &&
		    BIT_TEST(player->click, 3)) { /* Special click */
			player_close_popup(player);
			sfx_play_clip(SFX_ACCEPTED);
			/* sub_58DA8(player->sett->map_cursor_col, player->sett->map_cursor_row); */
			/* sub_41D58(); */
		}
	} else {
		player_close_popup(player);
		if (player->sett->panel_btn_type != PANEL_BTN_BUILD_LARGE ||
		    (player->sett->map_cursor_type != 7 &&
		     player->sett->map_cursor_type != 6 &&
		     player->sett->map_cursor_type != 5)) {
			sfx_play_clip(SFX_NOT_ACCEPTED);
			player_update_interface(player);
		} else {
			build_building(player, MAP_OBJ_LARGE_BUILDING);
		}
	}
}

/* Create the initial serfs that occupies the castle. */
static void
create_initial_castle_serfs(player_sett_t *sett)
{
	sett->build |= BIT(2);

	/* Spawn serf 4 */
	serf_t *serf;
	inventory_t *inventory;
	int r = game_spawn_serf(sett, &serf, &inventory, 0);
	if (r < 0) return;

	inventory->spawn_priority -= 1;
	serf->type = (serf->type & 0x83) | (SERF_4 << 2);

	serf_log_state_change(serf, SERF_STATE_BUILDING_CASTLE);
	serf->state = SERF_STATE_BUILDING_CASTLE;
	serf->s.building_castle.inv_index = sett->castle_inventory;
	map_set_serf_index(serf->pos, SERF_INDEX(serf));

	building_t *building = game_get_building(sett->building);
	building->serf_index = SERF_INDEX(serf);

	sett->serf_count[SERF_GENERIC] -= 1;
	sett->serf_count[SERF_TRANSPORTER] += 1;

	/* Spawn generic serfs */
	for (int i = 0; i < 5; i++) {
		game_spawn_serf(sett, NULL, NULL, 0);
	}

	/* Spawn three knights */
	for (int i = 0; i < 3; i++) {
		r = game_spawn_serf(sett, &serf, &inventory, 0);
		if (r < 0) return;

		serf->type = (serf->type & 0x83) | (SERF_KNIGHT_0 << 2);

		sett->serf_count[SERF_GENERIC] -= 1;
		sett->serf_count[SERF_KNIGHT_0] += 1;
		sett->total_military_score += 1;

		inventory->resources[RESOURCE_SWORD] -= 1;
		inventory->resources[RESOURCE_SHIELD] -= 1;
		inventory->spawn_priority -= 1;
	}

	/* Spawn toolmaker */
	r = game_spawn_serf(sett, &serf, &inventory, 0);
	if (r < 0) return;

	serf->type = (serf->type & 0x83) | (SERF_TOOLMAKER << 2);

	sett->serf_count[SERF_GENERIC] -= 1;
	sett->serf_count[SERF_TOOLMAKER] += 1;

	inventory->resources[RESOURCE_HAMMER] -= 1;
	inventory->resources[RESOURCE_SAW] -= 1;
	inventory->spawn_priority -= 1;

	/* Spawn timberman */
	r = game_spawn_serf(sett, &serf, &inventory, 0);
	if (r < 0) return;

	serf->type = (serf->type & 0x83) | (SERF_LUMBERJACK << 2);

	sett->serf_count[SERF_GENERIC] -= 1;
	sett->serf_count[SERF_LUMBERJACK] += 1;

	inventory->resources[RESOURCE_AXE] -= 1;
	inventory->spawn_priority -= 1;

	/* Spawn sawmiller */
	r = game_spawn_serf(sett, &serf, &inventory, 0);
	if (r < 0) return;

	serf->type = (serf->type & 0x83) | (SERF_SAWMILLER << 2);

	sett->serf_count[SERF_GENERIC] -= 1;
	sett->serf_count[SERF_SAWMILLER] += 1;

	inventory->resources[RESOURCE_SAW] -= 1;
	inventory->spawn_priority -= 1;

	/* Spawn stonecutter */
	r = game_spawn_serf(sett, &serf, &inventory, 0);
	if (r < 0) return;

	serf->type = (serf->type & 0x83) | (SERF_STONECUTTER << 2);

	sett->serf_count[SERF_GENERIC] -= 1;
	sett->serf_count[SERF_STONECUTTER] += 1;

	inventory->resources[RESOURCE_PICK] -= 1;
	inventory->spawn_priority -= 1;

	/* Spawn digger */
	r = game_spawn_serf(sett, &serf, &inventory, 0);
	if (r < 0) return;

	serf->type = (serf->type & 0x83) | (SERF_DIGGER << 2);

	sett->serf_count[SERF_GENERIC] -= 1;
	sett->serf_count[SERF_DIGGER] += 1;

	inventory->resources[RESOURCE_SHOVEL] -= 1;
	inventory->spawn_priority -= 1;

	/* Spawn builder */
	r = game_spawn_serf(sett, &serf, &inventory, 0);
	if (r < 0) return;

	serf->type = (serf->type & 0x83) | (SERF_BUILDER << 2);

	sett->serf_count[SERF_GENERIC] -= 1;
	sett->serf_count[SERF_BUILDER] += 1;

	inventory->resources[RESOURCE_HAMMER] -= 1;
	inventory->spawn_priority -= 1;

	/* Spawn fisherman */
	r = game_spawn_serf(sett, &serf, &inventory, 0);
	if (r < 0) return;

	serf->type = (serf->type & 0x83) | (SERF_FISHER << 2);

	sett->serf_count[SERF_GENERIC] -= 1;
	sett->serf_count[SERF_FISHER] += 1;

	inventory->resources[RESOURCE_ROD] -= 1;
	inventory->spawn_priority -= 1;

	/* Spawn two geologists */
	for (int i = 0; i < 2; i++) {
		r = game_spawn_serf(sett, &serf, &inventory, 0);
		if (r < 0) return;

		serf->type = (serf->type & 0x83) | (SERF_GEOLOGIST << 2);

		sett->serf_count[SERF_GENERIC] -= 1;
		sett->serf_count[SERF_GEOLOGIST] += 1;

		inventory->resources[RESOURCE_HAMMER] -= 1;
		inventory->spawn_priority -= 1;
	}

	/* Spawn two miners */
	for (int i = 0; i < 2; i++) {
		r = game_spawn_serf(sett, &serf, &inventory, 0);
		if (r < 0) return;

		serf->type = (serf->type & 0x83) | (SERF_MINER << 2);

		sett->serf_count[SERF_GENERIC] -= 1;
		sett->serf_count[SERF_MINER] += 1;

		inventory->resources[RESOURCE_PICK] -= 1;
		inventory->spawn_priority -= 1;
	}
}

/* Build castle. */
void
player_build_castle(player_t *player)
{
	player_determine_map_cursor_type(player);

	if (player->sett->panel_btn_type != PANEL_BTN_BUILD_CASTLE ||
	    player->sett->map_cursor_type != 7) {
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

	inventory_t *inventory;
	int inv_index;
	int r = game_alloc_inventory(&inventory, &inv_index);
	if (r < 0) return;

	building_t *castle;
	int bld_index;
	r = game_alloc_building(&castle, &bld_index);
	if (r < 0) {
		game_free_inventory(inv_index);
		return;
	}

	flag_t *flag;
	int flg_index;
	r = game_alloc_flag(&flag, &flg_index);
	if (r < 0) {
		game_free_building(bld_index);
		game_free_inventory(inv_index);
		return;
	}

	/* TODO set_map_redraw(); */

	player->sett->flags |= BIT(0); /* Has castle */
	player->sett->build |= BIT(3);
	player->sett->total_building_score += building_get_score_from_type(BUILDING_CASTLE);

	castle->serf |= BIT(4) | BIT(6);
	castle->u.inventory = inventory;
	player->sett->castle_inventory = inv_index;
	inventory->bld_index = bld_index;
	inventory->flg_index = flg_index;
	inventory->player_num = player->sett->player_num;

	/* Create initial resources */
	const int supplies_template_0[] = { 0, 0, 0, 0, 0, 0, 0, 7, 0, 2, 0, 0, 0, 0, 0, 1, 6, 1, 0, 0, 1, 2, 3, 0, 10, 10 };
	const int supplies_template_1[] = { 2, 1, 1, 3, 2, 1, 0, 25, 1, 8, 4, 3, 8, 2, 1, 3, 12, 2, 1, 1, 2, 3, 4, 1, 30, 30 };
	const int supplies_template_2[] = { 3, 2, 2, 10, 3, 1, 0, 40, 2, 20, 12, 8, 20, 4, 2, 5, 20, 3, 1, 2, 3, 4, 6, 2, 60, 60 };
	const int supplies_template_3[] = { 8, 4, 6, 20, 7, 5, 3, 80, 5, 40, 20, 40, 50, 8, 4, 10, 30, 5, 2, 4, 6, 6, 12, 4, 100, 100 };
	const int supplies_template_4[] = { 30, 10, 30, 50, 10, 30, 10, 200, 10, 100, 30, 150, 100, 10, 5, 20, 50, 10, 5, 10, 20, 20, 50, 10, 200, 200 };

	int supplies = player->sett->initial_supplies;
	const int *template_1 = NULL;
	const int *template_2 = NULL;
	if (supplies < 10) {
		template_1 = supplies_template_0;
		template_2 = supplies_template_1;
	} else if (supplies < 20) {
		template_1 = supplies_template_1;
		template_2 = supplies_template_2;
		supplies -= 10;
	} else if (supplies < 30) {
		template_1 = supplies_template_2;
		template_2 = supplies_template_3;
		supplies -= 20;
	} else if (supplies < 40) {
		template_1 = supplies_template_3;
		template_2 = supplies_template_4;
		supplies -= 30;
	} else {
		template_1 = supplies_template_4;
		template_2 = supplies_template_4;
		supplies -= 40;
	}

	for (int i = 0; i < 26; i++) {
		int t1 = template_1[i];
		int n = (template_2[i] - template_1[i]) * (supplies * 6554);
		if (n >= 0x8000) t1 += 1;
		inventory->resources[i] = t1 + (n >> 16);
	}

	if (0/*globals.game_type == GAME_TYPE_TUTORIAL*/) {
		/* TODO ... */
	}

	inventory->resources[RESOURCE_PLANK] -= 7;
	inventory->resources[RESOURCE_STONE] -= 2;
	player->sett->extra_planks = 7;
	player->sett->extra_stone = 2;
	/* player->sett->field_163 &= ~(BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4) | BIT(5)); */

	/* player->sett->lumberjack_index = 0; */
	/* player->sett->sawmill_index = 0; */
	/* player->sett->stonecutter_index = 0; */

	/* TODO ... */

	int map_cursor_pos = MAP_POS(player->sett->map_cursor_col, player->sett->map_cursor_row);
	castle->pos = map_cursor_pos;
	flag->pos = MAP_MOVE_DOWN_RIGHT(map_cursor_pos);
	castle->bld = BIT(7) | (BUILDING_CASTLE << 2) | player->sett->player_num;
	castle->progress = 0;
	castle->stock1 = 0xff;
	castle->stock2 = 0xff;
	player->sett->building = bld_index;
	flag->path_con = player->sett->player_num << 6;
	flag->bld_flags = BIT(7) | BIT(6);
	flag->bld2_flags = BIT(7);
	castle->flg_index = flg_index;
	flag->other_endpoint.b[DIR_UP_LEFT] = castle;
	flag->endpoint |= BIT(6);

	map_1_t *map = globals.map_mem2_ptr;
	map_2_t *map_data = MAP_2_DATA(map);
	map_data[map_cursor_pos].u.index = bld_index;
	map_set_object(map_cursor_pos, MAP_OBJ_CASTLE);
	map[map_cursor_pos].flags |= BIT(1) | BIT(6);

	map_data[MAP_MOVE_DOWN_RIGHT(map_cursor_pos)].u.index = flg_index;
	map_set_object(MAP_MOVE_DOWN_RIGHT(map_cursor_pos), MAP_OBJ_FLAG);
	map[MAP_MOVE_DOWN_RIGHT(map_cursor_pos)].flags |= BIT(7) | BIT(4);

	/* Level land in hexagon below castle */
	int h = player->sett->building_height_after_level;
	map_set_height(map_cursor_pos, h);
	for (dir_t d = DIR_RIGHT; d <= DIR_UP; d++) {
		map_set_height(MAP_MOVE(map_cursor_pos, d), h);
	}

	update_land_ownership(player->sett->map_cursor_col, player->sett->map_cursor_row);
	create_initial_castle_serfs(player->sett);

	player->sett->last_anim = globals.anim;

	calculate_military_flag_state(castle);
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
		if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
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

	if (MAP_OWNER(pos) != sett->player_num || MAP_WATER_2(pos) ||
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

	int state = building->serf & 3;
	int knights_present = (building->stock1 >> 4) & 0xf;
	int to_send = knights_present - min_level[sett->knight_occupation[state] & 0xf];

	if (to_send > 0) sett->attacking_knights[dist] += to_send;

	return index;
}

int
player_knights_available_for_attack(player_sett_t *sett, map_pos_t pos)
{
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

	sett->total_attacking_knights = 0;
	for (int i = 0; i < 4; i++) {
		sett->total_attacking_knights +=
			sett->attacking_knights[i];
	}

	sett->attacking_building_count = index;
	return index;
}
