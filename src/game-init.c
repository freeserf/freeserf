/*
 * game-init.c - Game initialization GUI component
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

#include <time.h>

#include "game-init.h"
#include "interface.h"

#include "game.h"
#include "mission.h"
#include "data.h"
#include "gfx.h"


typedef enum {
	ACTION_START_GAME,
	ACTION_TOGGLE_GAME_TYPE,
	ACTION_SHOW_OPTIONS,
	ACTION_SHOW_LOAD_GAME,
	ACTION_INCREMENT,
	ACTION_DECREMENT,
	ACTION_CLOSE
} action_t;


static void
draw_box_icon(int x, int y, int sprite, frame_t *frame)
{
	gfx_draw_sprite(8*x+20, y+16, DATA_ICON_BASE + sprite, frame);
}

static void
draw_box_string(int x, int y, frame_t *frame, const char *str)
{
	gfx_draw_string(8*x+20, y+16, 31, 1, frame, str);
}

/* Get the sprite number for a face. */
static int
get_player_face_sprite(int face)
{
	if (face != 0) return 0x10b + face;
	return 0x119; /* sprite_face_none */
}

static void
game_init_box_draw(game_init_box_t *box, frame_t *frame)
{
	/* Background */
	gfx_fill_rect(0, 0, box->obj.width, box->obj.height, 1, frame);

	const int layout[] = {
		251, 0, 40, 252, 0, 112, 253, 0, 48,
		254, 5, 48, 255, 9, 48,
		251, 10, 40, 252, 10, 112, 253, 10, 48,
		254, 15, 48, 255, 19, 48,
		251, 20, 40, 252, 20, 112, 253, 20, 48,
		254, 25, 48, 255, 29, 48,
		251, 30, 40, 252, 30, 112, 253, 30, 48,
		254, 35, 48, 255, 39, 48,

		266, 0, 0, 267, 31, 0, 316, 36, 0,
		-1
	};

	const int *i = layout;
	while (i[0] >= 0) {
		draw_box_icon(i[1], i[2], i[0], frame);
		i += 3;
	}

	/* Game type settings */
	if (box->game_mission < 0) {
		draw_box_icon(5, 0, 263, frame);

		char map_size[4] = {0};
		sprintf(map_size, "%d", box->map_size);

		draw_box_string(10, 0, frame, "Start new game");
		draw_box_string(10, 14, frame, "Map size:");
		draw_box_string(20, 14, frame, map_size);
	} else {
		draw_box_icon(5, 0, 260, frame);

		char level[4] = {0};
		sprintf(level, "%d", box->game_mission+1);

		draw_box_string(10, 0, frame, "Start mission");
		draw_box_string(10, 14, frame, "Mission:");
		draw_box_string(20, 14, frame, level);
	}

	draw_box_icon(28, 0, 237, frame);
	draw_box_icon(28, 16, 240, frame);

	/* Game info */
	if (box->game_mission < 0) {
		for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
			int face = i == 0 ? 12 : 0;
			draw_box_icon(10*i+1, 48, get_player_face_sprite(face), frame);
			draw_box_icon(10*i+6, 48, 282, frame);

			int intelligence = i == 0 ? 40 : 0;
			gfx_fill_rect(80*i+78, 124-intelligence, 4, intelligence, 30, frame);

			int supplies = i == 0 ? 40 : 0;
			gfx_fill_rect(80*i+72, 124-supplies, 4, supplies, 67, frame);

			int reproduction = i == 0 ? 40 : 0;
			gfx_fill_rect(80*i+84, 124-reproduction, 4, reproduction, 75, frame);
		}
	} else {
		int m = box->game_mission;
		for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
			int face = i == 0 ? 12 : mission[m].player[i].face;
			draw_box_icon(10*i+1, 48, get_player_face_sprite(face), frame);
			draw_box_icon(10*i+6, 48, 282, frame);

			int intelligence = i == 0 ? 40 : mission[m].player[i].intelligence;
			gfx_fill_rect(80*i+78, 124-intelligence, 4, intelligence, 30, frame);

			int supplies = mission[m].player[i].supplies;
			gfx_fill_rect(80*i+72, 124-supplies, 4, supplies, 67, frame);

			int reproduction = mission[m].player[i].reproduction;
			gfx_fill_rect(80*i+84, 124-reproduction, 4, reproduction, 75, frame);
		}
	}

	draw_box_icon(38, 128, 60, frame); /* exit */
}

static void
handle_action(game_init_box_t *box, int action)
{
	switch (action) {
	case ACTION_START_GAME:
		game_init();
		if (box->game_mission < 0) {
			random_state_t rnd = {{ 0x5a5a, time(NULL) >> 16, time(NULL) }};
			int r = game_load_random_map(box->map_size, &rnd);
			if (r < 0) return;
		} else {
			int r = game_load_mission_map(box->game_mission);
			if (r < 0) return;
		}

		/* Move viewport to initial position */
		map_pos_t init_pos = MAP_POS(0,0);
		if (box->interface->player->castle_flag != 0) {
			flag_t *flag = game_get_flag(box->interface->player->castle_flag);
			init_pos = MAP_MOVE_UP_LEFT(flag->pos);
		}

		viewport_map_reinit();
		interface_update_map_cursor_pos(box->interface, init_pos);
		viewport_move_to_map_pos(&box->interface->viewport, init_pos);
		interface_close_game_init(box->interface);
		break;
	case ACTION_TOGGLE_GAME_TYPE:
		if (box->game_mission < 0) {
			box->game_mission = 0;
		} else {
			box->game_mission = -1;
			box->map_size = 3;
		}
		break;
	case ACTION_SHOW_OPTIONS:
		break;
	case ACTION_SHOW_LOAD_GAME:
		break;
	case ACTION_INCREMENT:
		if (box->game_mission < 0) {
			box->map_size = min(box->map_size+1, 10);
		} else {
			box->game_mission = min(box->game_mission+1, mission_count-1);
		}
		break;
	case ACTION_DECREMENT:
		if (box->game_mission < 0) {
			box->map_size = max(3, box->map_size-1);
		} else {
			box->game_mission = max(0, box->game_mission-1);
		}
		break;
	case ACTION_CLOSE:
		interface_close_game_init(box->interface);
		break;
	default:
		break;
	}
}

static int
game_init_box_handle_event_click(game_init_box_t *box, int x, int y)
{
	const int clickmap[] = {
		ACTION_START_GAME, 20, 16, 32, 32,
		ACTION_TOGGLE_GAME_TYPE, 60, 16, 32, 32,
		ACTION_SHOW_OPTIONS, 268, 16, 32, 32,
		ACTION_SHOW_LOAD_GAME, 308, 16, 32, 32,
		ACTION_INCREMENT, 244, 16, 16, 16,
		ACTION_DECREMENT, 244, 32, 16, 16,
		ACTION_CLOSE, 324, 144, 16, 16,
		-1
	};

	const int *i = clickmap;
	while (i[0] >= 0) {
		if (x >= i[1] && x < i[1]+i[3] &&
		    y >= i[2] && y < i[2]+i[4]) {
			handle_action(box, i[0]);
			break;
		}
		i += 5;
	}

	return 0;
}

static int
game_init_box_handle_event(game_init_box_t *box, const gui_event_t *event)
{
	switch (event->type) {
	case GUI_EVENT_TYPE_CLICK:
		if (event->button == GUI_EVENT_BUTTON_LEFT) {
			return game_init_box_handle_event_click(box, event->x, event->y);
		}
	default:
		break;
	}

	return 0;
}

void
game_init_box_init(game_init_box_t *box, interface_t *interface)
{
	gui_object_init((gui_object_t *)box);
	box->obj.draw = (gui_draw_func *)game_init_box_draw;
	box->obj.handle_event = (gui_handle_event_func *)game_init_box_handle_event;

	box->interface = interface;
	box->map_size = 3;
	box->game_mission = -1;
}
