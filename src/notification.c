/*
 * notification.c - Notification GUI component
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

#include "notification.h"
#include "gfx.h"
#include "gui.h"
#include "interface.h"
#include "game.h"
#include "building.h"
#include "data.h"
#include "debug.h"
#include "language.h"

static void
draw_icon(int x, int y, int sprite, frame_t *frame)
{
	gfx_draw_sprite(8*x, y, DATA_ICON_BASE + sprite, frame);
}

static void
draw_background(int width, int height, int sprite, frame_t *frame)
{
	for (int y = 0; y < height; y += 16) {
		for (int x = 0; x < width; x += 16) {
			gfx_draw_sprite(x, y, DATA_ICON_BASE + sprite, frame);
		}
	}
}

static void
draw_string(int x, int y, frame_t *frame, const char *str)
{
	gfx_draw_string(8*x, y, 31, 0, frame, str);
}

static int
	draw_string_block(int x, int y, frame_t *frame, const char *str)
{
	const char * startstr = str;
	int lastlen = 0;

	for (; *str != 0; str++) {
		if (*str == '|') {
			gfx_draw_n_string(8 * x, y, 31, 0, frame, startstr, lastlen);
			y += 10;
			lastlen = 0;
			startstr = str + 1;
		}
		else {
			lastlen++;
		}	
	}

	if (lastlen > 0) {
		gfx_draw_n_string(8 * x, y, 31, 0, frame, startstr, lastlen);
		y += 10;
	}

	return y;
}

static void
draw_map_object(int x, int y, int sprite, frame_t *frame)
{
	gfx_draw_transp_sprite(8*x, y, DATA_MAP_OBJECT_BASE + sprite, frame);
}

static int
get_player_face_sprite(int face)
{
	if (face != 0) return 0x10b + face;
	return 0x119; /* sprite_face_none */
}

static void
draw_player_face(int x, int y, int player, frame_t *frame)
{
	gfx_fill_rect(8*x, y, 48, 72, game.player[player]->color, frame);
	draw_icon(x+1, y+4, get_player_face_sprite(game.player[player]->face), frame);
}


/* Messages boxes */
static void
draw_under_attack_message_box(frame_t *frame, int opponent)
{
	draw_string_block(1, 10, frame, _S(STR_UNDERATK, "Your settlement|is under attack"));
	draw_player_face(18, 8, opponent, frame);
}

static void
draw_lost_fight_message_box(frame_t *frame, int opponent)
{
	draw_string_block(1, 10, frame, _S(STR_KNIGHTLOST, "Your knights|just lost the|fight"));
	draw_player_face(18, 8, opponent, frame);
}

static void
draw_victory_fight_message_box(frame_t *frame, int opponent)
{
	draw_string_block(1, 10, frame, _S(STR_KNIGHTVIC, "You gained|a victory here"));
	draw_player_face(18, 8, opponent, frame);
}

static void
draw_mine_empty_message_box(frame_t *frame, int mine)
{

	draw_string_block(1, 10, frame, _S(STR_MINEEMPTY, "This mine hauls|no more raw|materials"));
	draw_map_object(18, 8, map_building_sprite[BUILDING_STONEMINE] + mine,
			frame);
}

static void
draw_call_to_location_message_box(frame_t *frame, int param)
{
	draw_string_block(1, 10, frame, _S(STR_CALLYOU, "You wanted me|to call you to|this location"));
	draw_map_object(20, 14, 0x90, frame);
}

static void
draw_knight_occupied_message_box(frame_t *frame, int building)
{
	draw_string_block(1, 10, frame, _S(STR_KNIGHTOCCUP, "A knight has|occupied this|new building"));

	switch (building) {
	case 0:
		draw_map_object(18, 8,
				map_building_sprite[BUILDING_HUT],
				frame);
		break;
	case 1:
		draw_map_object(18, 8,
				map_building_sprite[BUILDING_TOWER],
				frame);
		break;
	case 2:
		draw_map_object(16, 8,
				map_building_sprite[BUILDING_FORTRESS],
				frame);
		break;
	default:
		NOT_REACHED();
		break;
	}
}

static void
draw_new_stock_message_box(frame_t *frame, int param)
{
	draw_string_block(1, 10, frame, _S(STR_STOCKBUILD, "A new stock|has been built"));
	draw_map_object(16, 8, map_building_sprite[BUILDING_STOCK], frame);
}

static void
draw_lost_land_message_box(frame_t *frame, int opponent)
{
	draw_string_block(1, 10, frame, _S(STR_LANDLOST, "Because of this|enemy building|you lost some|land"));
	draw_player_face(18, 8, opponent, frame);
}

static void
draw_lost_buildings_message_box(frame_t *frame, int opponent)
{
	draw_string_block(1, 10, frame, _S(STR_BUILDLOST, "Because of this|enemy building|you lost some|land and|some buildings"));
	draw_player_face(18, 8, opponent, frame);
}

static void
draw_emergency_active_message_box(frame_t *frame, int param)
{
	draw_string_block(1, 10, frame, _S(STR_EMERG_PROG_START, "Emergency|program|activated"));
	draw_map_object(18, 8, map_building_sprite[BUILDING_CASTLE] + 1, frame);
}

static void
draw_emergency_neutral_message_box(frame_t *frame, int param)
{
	draw_string_block(1, 10, frame, _S(STR_EMERG_PROG_STOP, "Emergency|program|neutralized"));
	draw_map_object(16, 8, map_building_sprite[BUILDING_CASTLE], frame);
}

static void
draw_found_gold_message_box(frame_t *frame, int param)
{
	draw_string_block(1, 10, frame, _S(STR_GEO_GOLD, "A geologist|has found gold"));
	draw_icon(20, 14, 0x2f, frame);
}

static void
draw_found_iron_message_box(frame_t *frame, int param)
{
	draw_string_block(1, 10, frame, _S(STR_GEO_IRON, "A geologist|has found iron"));
	draw_icon(20, 14, 0x2c, frame);
}

static void
draw_found_coal_message_box(frame_t *frame, int param)
{
	draw_string_block(1, 10, frame, _S(STR_GEO_COAL, "A geologist|has found coal"));
	draw_icon(20, 14, 0x2e, frame);
}

static void
draw_found_stone_message_box(frame_t *frame, int param)
{
	draw_string_block(1, 10, frame, _S(STR_GEO_STONE, "A geologist|has found stone"));
	draw_icon(20, 14, 0x2b, frame);
}

static void
draw_call_to_menu_message_box(frame_t *frame, int menu)
{
	const int map_menu_sprite[] = {
		0xe6, 0xe7, 0xe8, 0xe9,
		0xea, 0xeb, 0x12a, 0x12b
	};

	draw_string_block(1, 10, frame, _S(STR_CALLYOU_MENU, "You wanted me|to call you|to this menu"));
	draw_icon(18, 8, map_menu_sprite[menu], frame);
}

static void
draw_30m_since_save_message_box(frame_t *frame, int param)
{
	draw_string_block(1, 10, frame, _S(STR_30M_SAVE, "30 min. passed|since the last|saving"));
	draw_icon(20, 14, 0x5d, frame);
}

static void
draw_1h_since_save_message_box(frame_t *frame, int param)
{
	draw_string_block(1, 10, frame, _S(STR_1H_SAVE, "One hour passed|since the last|saving"));
	draw_icon(20, 14, 0x5d, frame);
}

static void
draw_call_to_stock_message_box(frame_t *frame, int param)
{
	draw_string_block(1, 10, frame, _S(STR_CALLYOU_STOCK, "You wanted me|to call you|to this stock"));
	draw_map_object(16, 8, map_building_sprite[BUILDING_STOCK], frame);
}

static void
notification_box_draw(notification_box_t *box, frame_t *frame)
{
	draw_background(box->obj.width, box->obj.height,
			0x13a, frame);
	draw_icon(14, 128, 0x120, frame); /* Checkbox */

	int param = box->param;
	switch (box->type) {
	case 1:
		draw_under_attack_message_box(frame, param);
		break;
	case 2:
		draw_lost_fight_message_box(frame, param);
		break;
	case 3:
		draw_victory_fight_message_box(frame, param);
		break;
	case 4:
		draw_mine_empty_message_box(frame, param);
		break;
	case 5:
		draw_call_to_location_message_box(frame, param);
		break;
	case 6:
		draw_knight_occupied_message_box(frame, param);
		break;
	case 7:
		draw_new_stock_message_box(frame, param);
		break;
	case 8:
		draw_lost_land_message_box(frame, param);
		break;
	case 9:
		draw_lost_buildings_message_box(frame, param);
		break;
	case 10:
		draw_emergency_active_message_box(frame, param);
		break;
	case 11:
		draw_emergency_neutral_message_box(frame, param);
		break;
	case 12:
		draw_found_gold_message_box(frame, param);
		break;
	case 13:
		draw_found_iron_message_box(frame, param);
		break;
	case 14:
		draw_found_coal_message_box(frame, param);
		break;
	case 15:
		draw_found_stone_message_box(frame, param);
		break;
	case 16:
		draw_call_to_menu_message_box(frame, param);
		break;
	case 17:
		draw_30m_since_save_message_box(frame, param);
		break;
	case 18:
		draw_1h_since_save_message_box(frame, param);
		break;
	case 19:
		draw_call_to_stock_message_box(frame, param);
		break;
	default:
		NOT_REACHED();
		break;
	}
}

static int
notification_box_handle_event_click(notification_box_t *box, int x, int y)
{
	gui_object_set_displayed((gui_object_t *)box, 0);
	return 0;
}

static int
notification_box_handle_event(notification_box_t *box, const gui_event_t *event)
{
	switch (event->type) {
	case GUI_EVENT_TYPE_CLICK:
		if (event->button == GUI_EVENT_BUTTON_LEFT) {
			return notification_box_handle_event_click(box, event->x, event->y);
		}
	default:
		break;
	}

	return 0;
}

void
notification_box_init(notification_box_t *box, interface_t *interface)
{
	gui_object_init((gui_object_t *)box);
	box->obj.draw = (gui_draw_func *)notification_box_draw;
	box->obj.handle_event = (gui_handle_event_func *)notification_box_handle_event;

	box->interface = interface;
	box->type = 0;
	box->param = 0;
}
