/*
 * panel.c - Panel GUI component
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

#include "panel.h"
#include "gui.h"
#include "viewport.h"
#include "minimap.h"
#include "interface.h"
#include "player.h"
#include "game.h"
#include "data.h"
#include "gfx.h"
#include "audio.h"


/* Draw the frame around action buttons. */
static void
draw_panel_frame(panel_bar_t *panel, frame_t *frame)
{
	const int bottom_svga_layout[] = {
		DATA_FRAME_BOTTOM_SHIELD, 0, 0,
		DATA_FRAME_BOTTOM_BASE+0, 40, 0,
		DATA_FRAME_BOTTOM_BASE+20, 48, 0,

		DATA_FRAME_BOTTOM_BASE+7, 64, 0,
		DATA_FRAME_BOTTOM_BASE+8, 64, 36,
		DATA_FRAME_BOTTOM_BASE+21, 96, 0,

		DATA_FRAME_BOTTOM_BASE+9, 112, 0,
		DATA_FRAME_BOTTOM_BASE+10, 112, 36,
		DATA_FRAME_BOTTOM_BASE+22, 144, 0,

		DATA_FRAME_BOTTOM_BASE+11, 160, 0,
		DATA_FRAME_BOTTOM_BASE+12, 160, 36,
		DATA_FRAME_BOTTOM_BASE+23, 192, 0,

		DATA_FRAME_BOTTOM_BASE+13, 208, 0,
		DATA_FRAME_BOTTOM_BASE+14, 208, 36,
		DATA_FRAME_BOTTOM_BASE+24, 240, 0,

		DATA_FRAME_BOTTOM_BASE+15, 256, 0,
		DATA_FRAME_BOTTOM_BASE+16, 256, 36,
		DATA_FRAME_BOTTOM_BASE+25, 288, 0,

		DATA_FRAME_BOTTOM_TIMERS, 304, 0,
		DATA_FRAME_BOTTOM_SHIELD, 312, 0,
		-1
	};

	/* TODO request full buffer swap(?) */

	const int *layout = bottom_svga_layout;

	/* Draw layout */
	for (int i = 0; layout[i] != -1; i += 3) {
		gfx_draw_sprite(layout[i+1], layout[i+2], layout[i], frame);
	}
}

/* Draw notification icon in action panel. */
static void
draw_message_notify(panel_bar_t *panel, frame_t *frame)
{
	panel->interface->msg_flags |= BIT(2);
	gfx_draw_sprite(panel->interface->msg_icon_x, 4,
			DATA_FRAME_BOTTOM_NOTIFY, frame);
}

/* Remove drawn notification icon in action panel. */
static void
draw_message_no_notify(panel_bar_t *panel, frame_t *frame)
{
	panel->interface->msg_flags &= ~BIT(2);
	gfx_draw_sprite(panel->interface->msg_icon_x, 4,
			DATA_FRAME_BOTTOM_NO_NOTIFY, frame);
}

/* Draw return arrow icon in action panel. */
static void
draw_return_arrow(panel_bar_t *panel, frame_t *frame)
{
	gfx_draw_sprite(panel->interface->msg_icon_x, 28,
			DATA_FRAME_BOTTOM_ARROW, frame);
}

/* Remove drawn return arrow icon in action panel. */
static void
draw_no_return_arrow(panel_bar_t *panel, frame_t *frame)
{
	gfx_draw_sprite(panel->interface->msg_icon_x, 28,
			DATA_FRAME_BOTTOM_NO_ARROW, frame);
}

/* Draw buttons in action panel. */
static void
draw_panel_buttons(panel_bar_t *panel, frame_t *frame)
{
	interface_t *interface = panel->interface;

	const int msg_category[] = {
		-1, 5, 5, 5, 4, 0, 4, 3, 4, 5,
		5, 5, 4, 4, 4, 4, 0, 0, 0, 0
	};

	if (BIT_TEST(interface->flags, 0)) return; /* Player not active */

	if (BIT_TEST(game.svga, 3)) { /* Game has started */
		if (1/*!coop mode || ...*/) {
			if (PLAYER_HAS_MESSAGE(interface->player)) {
				interface->player->flags &= ~BIT(3);
				while (interface->player->msg_queue_type[0] != 0) {
					int type = interface->player->msg_queue_type[0] & 0x1f;
					if (BIT_TEST(interface->config, msg_category[type])) {
						sfx_play_clip(SFX_MESSAGE);
						if (!BIT_TEST(interface->msg_flags, 0)) {
							interface->msg_flags |= BIT(0);
							draw_message_notify(panel, frame);
						}
						break;
					}

					/* Message is ignored. Remove. */
					int i;
					for (i = 1; i < 64 && interface->player->msg_queue_type[i] != 0; i++) {
						interface->player->msg_queue_type[i-1] = interface->player->msg_queue_type[i];
						interface->player->msg_queue_pos[i-1] = interface->player->msg_queue_pos[i];
					}
					interface->player->msg_queue_type[i-1] = 0;
				}
			}
		}

		if (BIT_TEST(interface->msg_flags, 1)) {
			interface->msg_flags &= ~BIT(1);
			while (1) {
				if (interface->player->msg_queue_type[0] == 0) {
					interface->msg_flags &= ~BIT(0);
					draw_message_no_notify(panel, frame);
					break;
				}

				int type = interface->player->msg_queue_type[0] & 0x1f;
				if (BIT_TEST(interface->config, msg_category[type])) break;

				/* Message is ignored. Remove. */
				int i;
				for (i = 1; i < 64 && interface->player->msg_queue_type[i] != 0; i++) {
					interface->player->msg_queue_type[i-1] = interface->player->msg_queue_type[i];
					interface->player->msg_queue_pos[i-1] = interface->player->msg_queue_pos[i];
				}
				interface->player->msg_queue_type[i-1] = 0;
			}
		}

		/* Blinking message icon. */
		if (BIT_TEST(interface->msg_flags, 0)) {
			if (game.const_tick & 0x30) {
				draw_message_notify(panel, frame);
			} else {
				draw_message_no_notify(panel, frame);
			}
		}

		/* Return arrow icon. */
		if (1/*TODO only redraw if update needed: BIT_TEST(interface->msg_flags, 4)*/) {
			interface->msg_flags &= ~BIT(4);
			if (BIT_TEST(interface->msg_flags, 3)) {
				draw_return_arrow(panel, frame);
			} else {
				draw_no_return_arrow(panel, frame);
			}
		}
	}

	const int inactive_buttons[] = {
		PANEL_BTN_BUILD_INACTIVE,
		PANEL_BTN_DESTROY_INACTIVE,
		PANEL_BTN_MAP_INACTIVE,
		PANEL_BTN_STATS_INACTIVE,
		PANEL_BTN_SETT_INACTIVE
	};

	for (int i = 0; i < 5; i++) {
		int button = interface->panel_btns[i];
		if (!panel->obj.enabled) button = inactive_buttons[i];

		int x = interface->panel_btns_x + i*interface->panel_btns_dist;
		int y = 4;
		int sprite = DATA_FRAME_BUTTON_BASE + button;

		gfx_draw_sprite(x, y, sprite, frame);
	}
}

static void
panel_bar_draw(panel_bar_t *panel, frame_t *frame)
{
	draw_panel_frame(panel, frame);
	draw_panel_buttons(panel, frame);
}

/* Handle a click on the panel buttons. */
static void
handle_panel_button_click(interface_t *interface, int btn)
{
	switch (interface->panel_btns[btn]) {
		case PANEL_BTN_MAP:
		case PANEL_BTN_MAP_STARRED:
			sfx_play_clip(SFX_CLICK);
			if (BIT_TEST(interface->click, 6)) {
				interface_close_popup(interface);
			} else {
				interface->flags &= ~BIT(6);
				if (BIT_TEST(interface->click, 3)) {
					/* TODO */
				}
				else {
					interface->flags &= ~BIT(6);
					interface->click |= BIT(6);
					interface->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
					interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					interface->panel_btns[2] = PANEL_BTN_MAP_STARRED;
					interface->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
					interface->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
					interface->click &= ~BIT(1);
					interface->minimap_advanced = -1;
					interface->minimap_flags = 8;

					/* Synchronize minimap window with viewport. */
					viewport_t *viewport = interface_get_top_viewport(interface);
					popup_box_t *popup = interface_get_popup_box(interface);
					map_pos_t pos = viewport_get_current_map_pos(viewport);
					minimap_move_to_map_pos(&popup->minimap, pos);

					interface_open_popup(interface, BOX_MAP);
				}
			}
			break;
		case PANEL_BTN_SETT:
		case PANEL_BTN_SETT_STARRED:
			sfx_play_clip(SFX_CLICK);
			if (BIT_TEST(interface->click, 6)) { /* Popup open */
				interface_close_popup(interface);
			} else {
				interface->flags &= ~BIT(6);
				interface->click |= BIT(6);
				interface->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
				interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
				interface->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
				interface->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
				interface->panel_btns[4] = PANEL_BTN_SETT_STARRED;
				interface->click &= ~BIT(1);
				if (BIT_TEST(interface->click, 0)) {
					interface_open_popup(interface, BOX_SETT_SELECT);
				} else {
					interface_open_popup(interface, BOX_SETT_SELECT_FILE);
				}
			}
			break;
		case PANEL_BTN_STATS:
		case PANEL_BTN_STATS_STARRED:
			sfx_play_clip(SFX_CLICK);
			if (BIT_TEST(interface->click, 6)) { /* Popup open */
				interface_close_popup(interface);
			} else {
				interface->flags &= ~BIT(6);
				interface->click |= BIT(6);
				interface->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
				interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
				interface->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
				interface->panel_btns[3] = PANEL_BTN_STATS_STARRED;
				interface->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
				interface->click &= ~BIT(1);
				interface_open_popup(interface, BOX_STAT_SELECT);
			}
			break;
		case PANEL_BTN_BUILD_ROAD:
		case PANEL_BTN_BUILD_ROAD_STARRED:
			if (BIT_TEST(interface->click, 3)) { /* Special click */
				/* TODO */
			} else {
				sfx_play_clip(SFX_CLICK);
				if (BIT_TEST(interface->click, 6)) { /* Popup open (Building road) */
					interface_build_road_end(interface);
				} else {
					interface_build_road_begin(interface);
				}
			}
			break;
		case PANEL_BTN_BUILD_FLAG:
			if (BIT_TEST(interface->click, 3)) { /* Special click */
				/* TODO */
			} else {
				sfx_play_clip(SFX_CLICK);
				interface_build_flag(interface);
			}
			break;
		case PANEL_BTN_BUILD_SMALL:
		case PANEL_BTN_BUILD_SMALL_STARRED:
			if (BIT_TEST(interface->click, 3)) { /* Special click */
				/* TODO */
			} else {
				sfx_play_clip(SFX_CLICK);
				if (BIT_TEST(interface->click, 6)) { /* Popup open */
					interface_close_popup(interface);
				} else {
					interface->click |= BIT(6);
					interface->panel_btns[0] = PANEL_BTN_BUILD_SMALL_STARRED;
					interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					interface->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
					interface->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
					interface->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
					interface->click &= ~BIT(1);
					interface_open_popup(interface, BOX_BASIC_BLD);
				}
			}
			break;
		case PANEL_BTN_BUILD_LARGE:
		case PANEL_BTN_BUILD_LARGE_STARRED:
			if (BIT_TEST(interface->click, 3)) { /* Special click */
				/* TODO */
			} else {
				sfx_play_clip(SFX_CLICK);
				if (BIT_TEST(interface->click, 6)) { /* Popup open */
					interface_close_popup(interface);
				} else {
					interface->click |= BIT(6);
					interface->panel_btns[0] = PANEL_BTN_BUILD_LARGE_STARRED;
					interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					interface->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
					interface->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
					interface->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
					interface->click &= ~BIT(1);
					interface_open_popup(interface, BOX_BASIC_BLD_FLIP);
				}
			}
			break;
		case PANEL_BTN_BUILD_MINE:
		case PANEL_BTN_BUILD_MINE_STARRED:
			if (BIT_TEST(interface->click, 3)) { /* Special click */
				/* TODO */
			} else {
				sfx_play_clip(SFX_CLICK);
				if (BIT_TEST(interface->click, 6)) { /* Popup open */
					interface_close_popup(interface);
				} else {
					interface->click |= BIT(6);
					interface->panel_btns[0] = PANEL_BTN_BUILD_MINE_STARRED;
					interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					interface->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
					interface->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
					interface->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
					interface->click &= ~BIT(1);
					interface_open_popup(interface, BOX_MINE_BUILDING);
				}
			}
			break;
		case PANEL_BTN_DESTROY:
			if (interface->map_cursor_type == MAP_CURSOR_TYPE_REMOVABLE_FLAG) {
				interface_demolish_object(interface);
			} else {
				interface->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
				interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
				interface->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
				interface->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
				interface->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
				interface->click &= ~BIT(1);
				interface_open_popup(interface, BOX_DEMOLISH);
			}
			break;
		case PANEL_BTN_BUILD_CASTLE:
			if (BIT_TEST(interface->click, 3)) { /* Special click */
				/* TODO */
			} else {
				interface_build_castle(interface);
			}
			break;
		case PANEL_BTN_DESTROY_ROAD:
			if (BIT_TEST(interface->click, 3)) { /* Special click */
				/* TODO */
			} else {
				interface->flags &= ~BIT(6);
				int r = game_demolish_road(interface->map_cursor_pos,
							   interface->player);
				if (r < 0) {
					sfx_play_clip(SFX_NOT_ACCEPTED);
					interface_update_map_cursor_pos(interface, interface->map_cursor_pos);
				} else {
					sfx_play_clip(SFX_ACCEPTED);
					interface->click |= BIT(2);
				}
			}
			break;
		case PANEL_BTN_GROUND_ANALYSIS:
		case PANEL_BTN_GROUND_ANALYSIS_STARRED:
			sfx_play_clip(SFX_CLICK);
			if (BIT_TEST(interface->click, 6)) { /* Popup open */
				interface_close_popup(interface);
			} else {
				if (BIT_TEST(game.split, 6)) { /* Coop mode */
					/* TODO */
				}
				interface->flags &= ~BIT(6);
				interface->click |= BIT(6);
				interface->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
				interface->panel_btns[1] = PANEL_BTN_GROUND_ANALYSIS_STARRED;
				interface->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
				interface->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
				interface->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
				interface->click &= ~BIT(1);
				interface_open_popup(interface, BOX_GROUND_ANALYSIS);
			}
			break;
		case PANEL_BTN_BUILD_INACTIVE:
			/* TODO */
			break;
	}
}

static void
handle_message_icon_click(interface_t *interface)
{
	int type = interface->player->msg_queue_type[0] & 0x1f;

	if (type == 16) {
		/* TODO */
	}

	interface->message_box = interface->player->msg_queue_type[0];

	if (BIT_TEST(0x8f3fe, type)) {
		/* Move screen to new position */
		map_pos_t new_pos = interface->player->msg_queue_pos[0];

		viewport_t *viewport = interface_get_top_viewport(interface);
		viewport_move_to_map_pos(viewport, new_pos);
		interface_update_map_cursor_pos(interface, new_pos);
	}

	interface_open_popup(interface, BOX_MESSAGE);
	interface->click &= ~BIT(6);
	interface->click &= ~BIT(1);

	interface->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
	interface->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
	interface->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
	interface->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
	interface->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;

	/* Move notifications forward in the queue. */
	int i;
	for (i = 1; i < 64 && interface->player->msg_queue_type[i] != 0; i++) {
		interface->player->msg_queue_type[i-1] = interface->player->msg_queue_type[i];
		interface->player->msg_queue_pos[i-1] = interface->player->msg_queue_pos[i];
	}
	interface->player->msg_queue_type[i-1] = 0;
}

static int
panel_bar_handle_event_click(panel_bar_t *panel, int x, int y)
{
	gui_object_set_redraw((gui_object_t *)panel);

	interface_t *interface = panel->interface;

	if (x <= interface->msg_icon_x - interface->pointer_x_off ||
	    x > interface->msg_icon_x - interface->pointer_x_off + 12) {
		if (x <= interface->timer_icon_x - interface->pointer_x_off - 4 ||
		    x > interface->timer_icon_x - interface->pointer_x_off + 8) {
			if (y < 4 || y >= 36 || x < interface->panel_btns_first_x) return 0;
			x -= interface->panel_btns_first_x;

			/* Figure out what button was clicked */
			int btn = 0;
			while (1) {
				if (x < 32) {
					if (btn < 5) break;
					else return 0;
				}
				btn += 1;
				if (x < interface->panel_btns_dist) return 0;
				x -= interface->panel_btns_dist;
			}
			handle_panel_button_click(interface, btn);
		} else {
			/* Timer bar click */
			if (BIT_TEST(game.svga, 3)) { /* Game has started */
				if ((BIT_TEST(game.split, 6) && /* Coop mode */
				     BIT_TEST(interface->click, 0)) ||
				    interface->player->timers_count >= 64) {
					sfx_play_clip(SFX_NOT_ACCEPTED);
					return 0;
				}

				if (BIT_TEST(interface->click, 1)) {
					/* Call to map position */
					int timer_id = interface->player->timers_count++;
					int timer_length = -1;

					if (y < 7) timer_length = 5*60;
					else if (y < 14) timer_length = 10*60;
					else if (y < 21) timer_length = 20*60;
					else if (y < 28) timer_length = 30*60;
					else timer_length = 60*60;

					interface->player->timers[timer_id].timeout = timer_length*TICKS_PER_SEC;
					interface->player->timers[timer_id].pos = interface->map_cursor_pos;
				} else {
					/* Call to box (+ map) position */
					/* TODO */
				}

				sfx_play_clip(SFX_ACCEPTED);
			}
		}
	} else {
		/* Message bar click */
		if (BIT_TEST(game.svga, 3)) { /* Game has started */
			if (y < 16) {
				/* Message icon */
				if (!BIT_TEST(interface->msg_flags, 0) || /* No message */
				    BIT_TEST(interface->click, 7)) { /* Building road */
					sfx_play_clip(SFX_CLICK);
				} else if (interface->clkmap == BOX_LOAD_ARCHIVE ||
					   interface->clkmap == BOX_LOAD_SAVE ||
					   interface->clkmap == BOX_DISK_MSG ||
					   interface->clkmap == BOX_QUIT_CONFIRM ||
					   interface->clkmap == BOX_NO_SAVE_QUIT_CONFIRM ||
					   interface->clkmap == BOX_OPTIONS) {
					sfx_play_clip(SFX_NOT_ACCEPTED);
				} else {
					interface->flags &= ~BIT(6);
					if (!BIT_TEST(interface->msg_flags, 3)) {
						interface->msg_flags |= BIT(4);
						interface->msg_flags |= BIT(3);
						viewport_t *viewport = interface_get_top_viewport(interface);
						map_pos_t pos = viewport_get_current_map_pos(viewport);
						interface->return_col_game_area = MAP_POS_COL(pos);
						interface->return_row_game_area = MAP_POS_ROW(pos);
					}

					handle_message_icon_click(interface);
					interface->msg_flags |= BIT(1);
					interface->return_timeout = 60*TICKS_PER_SEC;
					sfx_play_clip(SFX_CLICK);
				}
			} else if (y >= 28) {
				/* Return arrow */
				if (BIT_TEST(interface->msg_flags, 3)) { /* Return arrow present */
					interface->msg_flags |= BIT(4);
					interface->msg_flags &= ~BIT(3);

					interface->return_timeout = 0;
					viewport_t *viewport = interface_get_top_viewport(interface);
					map_pos_t pos = MAP_POS(interface->return_col_game_area,
								interface->return_row_game_area);
					viewport_move_to_map_pos(viewport, pos);

					if (interface->clkmap == BOX_MESSAGE) interface_close_popup(interface);
					sfx_play_clip(SFX_CLICK);
				}
			}
		}
	}

	return 0;
}

static int
panel_bar_handle_event(panel_bar_t *panel, const gui_event_t *event)
{
	int x = event->x;
	int y = event->y;

	switch (event->type) {
	case GUI_EVENT_TYPE_CLICK:
		if (event->button == GUI_EVENT_BUTTON_LEFT) {
			return panel_bar_handle_event_click(panel, x, y);
		}
	default:
		break;
	}

	return 0;
}

void
panel_bar_init(panel_bar_t *panel, interface_t *interface)
{
	gui_object_init((gui_object_t *)panel);
	panel->obj.draw = (gui_draw_func *)panel_bar_draw;
	panel->obj.handle_event = (gui_handle_event_func *)panel_bar_handle_event;

	panel->interface = interface;
}

void
panel_bar_activate_button(panel_bar_t *panel, int button)
{
	handle_panel_button_click(panel->interface, button);
}
