/* panel.c */

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
#include "globals.h"


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
	panel->player->msg_flags |= BIT(2);
	gfx_draw_sprite(panel->player->msg_icon_x, 4,
			DATA_FRAME_BOTTOM_NOTIFY, frame);
}

/* Remove drawn notification icon in action panel. */
static void
draw_message_no_notify(panel_bar_t *panel, frame_t *frame)
{
	panel->player->msg_flags &= ~BIT(2);
	gfx_draw_sprite(panel->player->msg_icon_x, 4,
			DATA_FRAME_BOTTOM_NO_NOTIFY, frame);
}

/* Draw return arrow icon in action panel. */
static void
draw_return_arrow(panel_bar_t *panel, frame_t *frame)
{
	gfx_draw_sprite(panel->player->msg_icon_x, 28,
			DATA_FRAME_BOTTOM_ARROW, frame);
}

/* Remove drawn return arrow icon in action panel. */
static void
draw_no_return_arrow(panel_bar_t *panel, frame_t *frame)
{
	gfx_draw_sprite(panel->player->msg_icon_x, 28,
			DATA_FRAME_BOTTOM_NO_ARROW, frame);
}

/* Draw buttons in action panel. */
static void
draw_panel_buttons(panel_bar_t *panel, frame_t *frame)
{
	player_t *player = panel->player;

	const int msg_category[] = {
		-1, 5, 5, 5, 4, 0, 4, 3, 4, 5,
		5, 5, 4, 4, 4, 4, 0, 0, 0, 0
	};

	if (BIT_TEST(player->flags, 0)) return; /* Player not active */

	if (BIT_TEST(globals.svga, 3)) { /* Game has started */
		if (1/*!coop mode || ...*/) {
			for (int i = 0; i < player->sett->timers_count; i++) {
				player->sett->timers[i].timeout -= globals.anim_diff;
				if (player->sett->timers[i].timeout < 0) {
					/* Timer has expired. */
					/* TODO box (+ pos) timer */
					player_add_notification(player->sett, 5,
								player->sett->timers[i].pos);

					/* Delete timer from list. */
					player->sett->timers_count -= 1;
					for (int j = i; j < player->sett->timers_count; j++) {
						player->sett->timers[j].timeout = player->sett->timers[j+1].timeout;
						player->sett->timers[j].pos = player->sett->timers[j+1].pos;
					}
				}
			}

			if (BIT_TEST(player->sett->flags, 3)) { /* Message in queue */
				player->sett->flags &= ~BIT(3);
				while (player->sett->msg_queue_type[0] != 0) {
					int type = player->sett->msg_queue_type[0] & 0x1f;
					if (BIT_TEST(player->config, msg_category[type])) {
						sfx_play_clip(SFX_MESSAGE);
						if (!BIT_TEST(player->msg_flags, 0)) {
							player->msg_flags |= BIT(0);
							draw_message_notify(panel, frame);
						}
						break;
					}

					/* Message is ignored. Remove. */
					int i;
					for (i = 1; i < 64 && player->sett->msg_queue_type[i] != 0; i++) {
						player->sett->msg_queue_type[i-1] = player->sett->msg_queue_type[i];
						player->sett->msg_queue_pos[i-1] = player->sett->msg_queue_pos[i];
					}
					player->sett->msg_queue_type[i-1] = 0;
				}
			}
		}

		if (BIT_TEST(player->msg_flags, 1)) {
			player->msg_flags &= ~BIT(1);
			while (1) {
				if (player->sett->msg_queue_type[0] == 0) {
					player->msg_flags &= ~BIT(0);
					draw_message_no_notify(panel, frame);
					break;
				}

				int type = player->sett->msg_queue_type[0] & 0x1f;
				if (BIT_TEST(player->config, msg_category[type])) break;

				/* Message is ignored. Remove. */
				int i;
				for (i = 1; i < 64 && player->sett->msg_queue_type[i] != 0; i++) {
					player->sett->msg_queue_type[i-1] = player->sett->msg_queue_type[i];
					player->sett->msg_queue_pos[i-1] = player->sett->msg_queue_pos[i];
				}
				player->sett->msg_queue_type[i-1] = 0;
			}
		}

		/* Blinking message icon. */
		if (BIT_TEST(player->msg_flags, 0)) {
			if (globals.anim & 0x60) {
				draw_message_notify(panel, frame);
			} else {
				draw_message_no_notify(panel, frame);
			}
		}

		/* Return arrow icon. */
		if (1/*TODO only redraw if update needed: BIT_TEST(player->msg_flags, 4)*/) {
			player->msg_flags &= ~BIT(4);
			if (BIT_TEST(player->msg_flags, 3)) {
				draw_return_arrow(panel, frame);
			} else {
				draw_no_return_arrow(panel, frame);
			}
		}
	}

	for (int i = 0; i < 5; i++) {
		int new = player->panel_btns[i];
		/*int set = player->panel_btns_set[i];*/
		/*if (new == set) continue;*/

		/* TODO request panel redraw */

		player->panel_btns_set[i] = new;

		int x = player->panel_btns_x + i*player->panel_btns_dist;
		int y = 4;
		int sprite = DATA_FRAME_BUTTON_BASE + new;

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
handle_panel_button_click(player_t *player, int btn)
{
	switch (player->panel_btns[btn]) {
		case PANEL_BTN_MAP:
		case PANEL_BTN_MAP_STARRED:
			sfx_play_clip(SFX_CLICK);
			if (BIT_TEST(player->click, 6)) {
				player_close_popup(player);
			} else {
				player->flags &= ~BIT(6);
				if (BIT_TEST(player->click, 3)) {
					/* TODO */
				}
				else {
					player->flags &= ~BIT(6);
					player->click |= BIT(6);
					player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
					player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					player->panel_btns[2] = PANEL_BTN_MAP_STARRED;
					player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
					player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
					player->click &= ~BIT(1);
					player->minimap_advanced = -1;
					player->minimap_flags = 8;

					/* Synchronize minimap window with viewport. */
					map_pos_t pos = viewport_get_current_map_pos(gui_get_top_viewport());
					minimap_move_to_map_pos(&gui_get_popup_box()->minimap, pos);

					player_open_popup(player, BOX_MAP);
				}
			}
			break;
		case PANEL_BTN_SETT:
		case PANEL_BTN_SETT_STARRED:
			sfx_play_clip(SFX_CLICK);
			if (BIT_TEST(player->click, 6)) { /* Popup open */
				player_close_popup(player);
			} else {
				player->flags &= ~BIT(6);
				player->click |= BIT(6);
				player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
				player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
				player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
				player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
				player->panel_btns[4] = PANEL_BTN_SETT_STARRED;
				player->click &= ~BIT(1);
				if (BIT_TEST(player->click, 0)) {
					player_open_popup(player, BOX_SETT_SELECT);
				} else {
					player_open_popup(player, BOX_SETT_SELECT_FILE);
				}
			}
			break;
		case PANEL_BTN_STATS:
		case PANEL_BTN_STATS_STARRED:
			sfx_play_clip(SFX_CLICK);
			if (BIT_TEST(player->click, 6)) { /* Popup open */
				player_close_popup(player);
			} else {
				player->flags &= ~BIT(6);
				player->click |= BIT(6);
				player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
				player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
				player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
				player->panel_btns[3] = PANEL_BTN_STATS_STARRED;
				player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
				player->click &= ~BIT(1);
				player_open_popup(player, BOX_STAT_SELECT);
			}
			break;
		case PANEL_BTN_BUILD_ROAD:
		case PANEL_BTN_BUILD_ROAD_STARRED:
			if (BIT_TEST(player->click, 3)) { /* Special click */
				/* TODO */
			} else {
				sfx_play_clip(SFX_CLICK);
				if (BIT_TEST(player->click, 6)) { /* Popup open (Building road) */
					player_build_road_end(player);
				} else {
					player_build_road_begin(player);
				}
			}
			break;
		case PANEL_BTN_BUILD_FLAG:
			if (BIT_TEST(player->click, 3)) { /* Special click */
				/* TODO */
			} else {
				sfx_play_clip(SFX_CLICK);
				player_build_flag(player);
			}
			break;
		case PANEL_BTN_BUILD_SMALL:
		case PANEL_BTN_BUILD_SMALL_STARRED:
			if (BIT_TEST(player->click, 3)) { /* Special click */
				/* TODO */
			} else {
				sfx_play_clip(SFX_CLICK);
				if (BIT_TEST(player->click, 6)) { /* Popup open */
					player_close_popup(player);
				} else {
					player->click |= BIT(6);
					player->panel_btns[0] = PANEL_BTN_BUILD_SMALL_STARRED;
					player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
					player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
					player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
					player->click &= ~BIT(1);
					player_open_popup(player, BOX_BASIC_BLD);
				}
			}
			break;
		case PANEL_BTN_BUILD_LARGE:
		case PANEL_BTN_BUILD_LARGE_STARRED:
			if (BIT_TEST(player->click, 3)) { /* Special click */
				/* TODO */
			} else {
				sfx_play_clip(SFX_CLICK);
				if (BIT_TEST(player->click, 6)) { /* Popup open */
					player_close_popup(player);
				} else {
					player->click |= BIT(6);
					player->panel_btns[0] = PANEL_BTN_BUILD_LARGE_STARRED;
					player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
					player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
					player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
					player->click &= ~BIT(1);
					player_open_popup(player, BOX_BASIC_BLD_FLIP);
				}
			}
			break;
		case PANEL_BTN_BUILD_MINE:
		case PANEL_BTN_BUILD_MINE_STARRED:
			if (BIT_TEST(player->click, 3)) { /* Special click */
				/* TODO */
			} else {
				sfx_play_clip(SFX_CLICK);
				if (BIT_TEST(player->click, 6)) { /* Popup open */
					player_close_popup(player);
				} else {
					player->click |= BIT(6);
					player->panel_btns[0] = PANEL_BTN_BUILD_MINE_STARRED;
					player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
					player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
					player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
					player->click &= ~BIT(1);
					player_open_popup(player, BOX_MINE_BUILDING);
				}
			}
			break;
		case PANEL_BTN_DESTROY:
			if (BIT_TEST(player->click, 3)) { /* Special click */
				if (player->sett->map_cursor_type == 2) {
					player_demolish_object(player);
				} else {
					player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
					player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
					player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
					player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
					player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
					player->click &= ~BIT(1);
					player_open_popup(player, BOX_DEMOLISH);
				}
			}
			break;
		case PANEL_BTN_BUILD_CASTLE:
			if (BIT_TEST(player->click, 3)) { /* Special click */
				/* TODO */
			} else {
				player_build_castle(player);
			}
			break;
		case PANEL_BTN_DESTROY_ROAD:
			if (BIT_TEST(player->click, 3)) { /* Special click */
				/* TODO */
			} else {
				player->flags &= ~BIT(6);
				player_determine_map_cursor_type(player);
				if (player->sett->map_cursor_type == 4) {
					sfx_play_clip(SFX_ACCEPTED);
					player->click |= BIT(2);

					map_pos_t cursor_pos = MAP_POS(player->sett->map_cursor_col,
								       player->sett->map_cursor_row);
					game_demolish_road(cursor_pos);
				} else {
					sfx_play_clip(SFX_NOT_ACCEPTED);
					player_update_interface(player);
				}
			}
			break;
		case PANEL_BTN_GROUND_ANALYSIS:
		case PANEL_BTN_GROUND_ANALYSIS_STARRED:
			sfx_play_clip(SFX_CLICK);
			if (BIT_TEST(player->click, 6)) { /* Popup open */
				player_close_popup(player);
			} else {
				if (BIT_TEST(globals.split, 6)) { /* Coop mode */
					/* TODO */
				}
				player->flags &= ~BIT(6);
				player->click |= BIT(6);
				player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
				player->panel_btns[1] = PANEL_BTN_GROUND_ANALYSIS_STARRED;
				player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
				player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
				player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;
				player->click &= ~BIT(1);
				player_open_popup(player, BOX_GROUND_ANALYSIS);
			}
			break;
		case PANEL_BTN_BUILD_INACTIVE:
			/* TODO */
			break;
	}
}

static void
handle_message_icon_click(player_t *player)
{
	int type = player->sett->msg_queue_type[0] & 0x1f;

	if (type == 16) {
		/* TODO */
	}

	player->message_box = player->sett->msg_queue_type[0];

	if (BIT_TEST(0x8f3fe, type)) {
		/* Move screen to new position */
		map_pos_t new_pos = player->sett->msg_queue_pos[0];

		viewport_t *viewport = gui_get_top_viewport();
		viewport_move_to_map_pos(viewport, new_pos);
		player->sett->map_cursor_col = MAP_POS_COL(new_pos);
		player->sett->map_cursor_row = MAP_POS_ROW(new_pos);
	}

	player_open_popup(player, BOX_MESSAGE);
	player->click &= ~BIT(6);
	player->click &= ~BIT(1);

	player->panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
	player->panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
	player->panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
	player->panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
	player->panel_btns[4] = PANEL_BTN_SETT_INACTIVE;

	/* Move notifications forward in the queue. */
	int i;
	for (i = 1; i < 64 && player->sett->msg_queue_type[i] != 0; i++) {
		player->sett->msg_queue_type[i-1] = player->sett->msg_queue_type[i];
		player->sett->msg_queue_pos[i-1] = player->sett->msg_queue_pos[i];
	}
	player->sett->msg_queue_type[i-1] = 0;
}

static int
panel_bar_handle_event_click(panel_bar_t *panel, int x, int y)
{
	gui_object_set_redraw((gui_object_t *)panel);

	player_t *player = panel->player;

	if (x <= player->msg_icon_x - player->pointer_x_off ||
	    x > player->msg_icon_x - player->pointer_x_off + 12) {
		if (x <= player->timer_icon_x - player->pointer_x_off - 4 ||
		    x > player->timer_icon_x - player->pointer_x_off + 8) {
			if (y < 4 || y >= 36 || x < player->panel_btns_first_x) return 0;
			x -= player->panel_btns_first_x;

			/* Figure out what button was clicked */
			int btn = 0;
			while (1) {
				if (x < 32) {
					if (btn < 5) break;
					else return 0;
				}
				btn += 1;
				if (x < player->panel_btns_dist) return 0;
				x -= player->panel_btns_dist;
			}
			handle_panel_button_click(player, btn);
		} else {
			/* Timer bar click */
			if (BIT_TEST(globals.svga, 3)) { /* Game has started */
				if ((BIT_TEST(globals.split, 6) && /* Coop mode */
				     BIT_TEST(player->click, 0)) ||
				    player->sett->timers_count >= 64) {
					sfx_play_clip(SFX_NOT_ACCEPTED);
					return 0;
				}

				if (BIT_TEST(player->click, 1)) {
					/* Call to map position */
					int timer_id = player->sett->timers_count++;
					int timer_length = -1;

					if (y < 7) timer_length = 5*60*100;
					else if (y < 14) timer_length = 10*60*100;
					else if (y < 21) timer_length = 20*60*100;
					else if (y < 28) timer_length = 30*60*100;
					else timer_length = 60*60*100;

					player->sett->timers[timer_id].timeout = timer_length;
					player->sett->timers[timer_id].pos = MAP_POS(player->sett->map_cursor_col,
										     player->sett->map_cursor_row);
				} else {
					/* Call to box (+ map) position */
					/* TODO */
				}

				sfx_play_clip(SFX_ACCEPTED);
			}
		}
	} else {
		/* Message bar click */
		if (BIT_TEST(globals.svga, 3)) { /* Game has started */
			if (y < 16) {
				/* Message icon */
				if (!BIT_TEST(player->msg_flags, 0) || /* No message */
				    BIT_TEST(player->click, 7)) { /* Building road */
					sfx_play_clip(SFX_CLICK);
				} else if (player->clkmap == BOX_LOAD_ARCHIVE ||
					   player->clkmap == BOX_LOAD_SAVE ||
					   player->clkmap == BOX_DISK_MSG ||
					   player->clkmap == BOX_QUIT_CONFIRM ||
					   player->clkmap == BOX_NO_SAVE_QUIT_CONFIRM ||
					   player->clkmap == BOX_OPTIONS) {
					sfx_play_clip(SFX_NOT_ACCEPTED);
				} else {
					player->flags &= ~BIT(6);
					if (!BIT_TEST(player->msg_flags, 3)) {
						player->msg_flags |= BIT(4);
						player->msg_flags |= BIT(3);
						viewport_t *viewport = gui_get_top_viewport();
						map_pos_t pos = viewport_get_current_map_pos(viewport);
						player->return_col_game_area = MAP_POS_COL(pos);
						player->return_row_game_area = MAP_POS_ROW(pos);
					}

					handle_message_icon_click(player);
					player->msg_flags |= BIT(1);
					player->return_timeout = 2000;
					sfx_play_clip(SFX_CLICK);
				}
			} else if (y >= 28) {
				/* Return arrow */
				if (BIT_TEST(player->msg_flags, 3)) { /* Return arrow present */
					player->msg_flags |= BIT(4);
					player->msg_flags &= ~BIT(3);

					player->return_timeout = 0;
					viewport_t *viewport = gui_get_top_viewport();
					map_pos_t pos = MAP_POS(player->return_col_game_area,
								player->return_row_game_area);
					viewport_move_to_map_pos(viewport, pos);

					if (player->clkmap == BOX_MESSAGE) player_close_popup(player);
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
panel_bar_init(panel_bar_t *panel, player_t *player)
{
	gui_object_init((gui_object_t *)panel);
	panel->obj.draw = (gui_draw_func *)panel_bar_draw;
	panel->obj.handle_event = (gui_handle_event_func *)panel_bar_handle_event;

	panel->player = player;
}

void
panel_bar_activate_button(panel_bar_t *panel, int button)
{
	handle_panel_button_click(panel->player, button);
}
