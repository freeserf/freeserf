/* gui.c */

#include <stdlib.h>
#include <math.h>

#include "globals.h"
#include "game.h"
#include "viewport.h"
#include "player.h"
#include "data.h"
#include "gfx.h"
#include "sdl-video.h"
#include "audio.h"
#include "debug.h"


/* Draw the frame around the popup box. */
static void
draw_popup_box_frame(frame_t *frame)
{
	gfx_draw_sprite(0, 0, DATA_FRAME_POPUP_BASE+0, frame);
	gfx_draw_sprite(0, 153, DATA_FRAME_POPUP_BASE+1, frame);
	gfx_draw_sprite(0, 9, DATA_FRAME_POPUP_BASE+2, frame);
	gfx_draw_sprite(136, 9, DATA_FRAME_POPUP_BASE+3, frame);
}

/* Draw the frame around action buttons. */
void
gui_draw_bottom_frame(frame_t *frame)
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

	/* TODO request full buffer swap */

	const int *layout = bottom_svga_layout;

	/* draw layout */
	for (int i = 0; layout[i] != -1; i += 3) {
		gfx_draw_sprite(globals.player[0]->bottom_panel_x + layout[i+1],
				globals.player[0]->bottom_panel_y + layout[i+2], layout[i], frame);
	}
}

/* Draw notification icon in action panel. */
static void
draw_message_notify(player_t *player)
{
	player->msg_flags |= BIT(2);
	gfx_draw_sprite(player->bottom_panel_x + player->msg_icon_x,
			player->bottom_panel_y + 4, DATA_FRAME_BOTTOM_NOTIFY, player->frame);
}

/* Remove drawn notification icon in action panel. */
static void
draw_message_no_notify(player_t *player)
{
	player->msg_flags &= ~BIT(2);
	gfx_draw_sprite(player->bottom_panel_x + player->msg_icon_x,
			player->bottom_panel_y + 4, DATA_FRAME_BOTTOM_NO_NOTIFY, player->frame);
}

/* Draw return arrow icon in action panel. */
static void
draw_return_arrow(player_t *player)
{
	gfx_draw_sprite(player->bottom_panel_x + player->msg_icon_x,
			player->bottom_panel_y + 28, DATA_FRAME_BOTTOM_ARROW, player->frame);
}

/* Remove drawn return arrow icon in action panel. */
static void
draw_no_return_arrow(player_t *player)
{
	gfx_draw_sprite(player->bottom_panel_x + player->msg_icon_x,
			player->bottom_panel_y + 28, DATA_FRAME_BOTTOM_NO_ARROW, player->frame);
}

/* Draw buttons in action panel. */
static void
draw_player_panel_btns(player_t *player)
{
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
							draw_message_notify(player);
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
					draw_message_no_notify(player);
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
				draw_message_notify(player);
			} else {
				draw_message_no_notify(player);
			}
		}

		/* Return arrow icon. */
		if (1/*TODO only redraw if update needed: BIT_TEST(player->msg_flags, 4)*/) {
			player->msg_flags &= ~BIT(4);
			if (BIT_TEST(player->msg_flags, 3)) {
				draw_return_arrow(player);
			} else {
				draw_no_return_arrow(player);
			}
		}
	}

	for (int i = 0; i < 5; i++) {
		int new = player->panel_btns[i];
		/*int set = player->panel_btns_set[i];*/
		/*if (new == set) continue;*/

		/* TODO request panel redraw */

		player->panel_btns_set[i] = new;

		int x = player->bottom_panel_x + player->panel_btns_x + i*player->panel_btns_dist;
		int y = player->bottom_panel_y + 4;
		int sprite = DATA_FRAME_BUTTON_BASE + new;

		gfx_draw_sprite(x, y, sprite, player->frame);
	}
}

void
gui_draw_panel_buttons()
{
	draw_player_panel_btns(globals.player[0]);
	draw_player_panel_btns(globals.player[1]);
}

/* Draw icon in a popup frame. */
static void
draw_popup_icon(int x, int y, int sprite, frame_t *frame)
{
	gfx_draw_sprite(8*x+8, y+9, DATA_ICON_BASE + sprite, frame);
}

/* Draw building in a popup frame. */
static void
draw_popup_building(int x, int y, int sprite, frame_t *frame)
{
	gfx_draw_transp_sprite(8*x+8, y+9, DATA_MAP_OBJECT_BASE + sprite, frame);
}

/* Fill the background of a popup frame. */
static void
draw_box_background(int sprite, frame_t *frame)
{
	for (int y = 0; y < 144; y += 16) {
		for (int x = 0; x < 16; x += 2) {
			draw_popup_icon(x, y, sprite, frame);
		}
	}
}

/* Fill one row of a popup frame. */
static void
draw_box_row(int sprite, int y, frame_t *frame)
{
	for (int x = 0; x < 16; x += 2) draw_popup_icon(x, y, sprite, frame);
}

/* Draw a green string in a popup frame. */
static void
draw_green_string(int x, int y, frame_t *frame, const char *str)
{
	gfx_draw_string(8*x+8, y+9, 31, 0, frame, str);
}

/* Draw a green number in a popup frame.
   n must be non-negative. If > 999 simply draw ">999" (three characters). */
static void
draw_green_number(int x, int y, frame_t *frame, int n)
{
	if (n >= 1000) {
		draw_popup_icon(x, y, 0xd5, frame); /* Draw >999 */
		draw_popup_icon(x+1, y, 0xd6, frame);
		draw_popup_icon(x+2, y, 0xd7, frame);
	} else {
		/* Not the same sprites as are used to draw numbers
		   in gfx_draw_number(). */
		int draw_zero = 0;
		if (n >= 100) {
			int n100 = floor(n / 100);
			n -= n100 * 100;
			draw_popup_icon(x, y, 0x4e + n100, frame);
			x += 1;
			draw_zero = 1;
		}

		if (n >= 10 || draw_zero) {
			int n10 = floor(n / 10);
			n -= n10 * 10;
			draw_popup_icon(x, y, 0x4e + n10, frame);
			x += 1;
			draw_zero = 1;
		}

		draw_popup_icon(x, y, 0x4e + n, frame);
	}
}

/* Draw a green number in a popup frame.
   No limits on n. */
static void
draw_green_large_number(int x, int y, frame_t *frame, int n)
{
	gfx_draw_number(8*x+8, 9+y, 31, 0, frame, n);
}

/* Draw small green number. */
static void
draw_additional_number(int x, int y, frame_t *frame, int n)
{
	if (n > 0) draw_popup_icon(x, y, 240 + min(n, 10), frame);
}

/* Get the sprite number for a face. */
static int
get_player_face_sprite(int face)
{
	if (face != 0) return 0x10b + face;
	return 0x119; /* sprite_face_none */
}

/* Draw player face in popup frame. */
static void
draw_player_face(int x, int y, int player, frame_t *frame)
{
	const int player_colors[] = {
		64, 72, 68, 76
	};

	gfx_fill_rect(8*x, y+5, 48, 72, player_colors[player], frame);
	draw_popup_icon(x, y, get_player_face_sprite(globals.pl_init[player].face), frame);
}

/* Draw a layout of buildings in a popup box. */
static void
draw_custom_bld_box(const int sprites[], frame_t *frame)
{
	while (sprites[0] > 0) {
		int x = sprites[1];
		int y = sprites[2];
		gfx_draw_transp_sprite(8*x+8, y+9, DATA_MAP_OBJECT_BASE + sprites[0], frame);
		sprites += 3;
	}
}

/* Draw a layout of icons in a popup box. */
static void
draw_custom_icon_box(const int sprites[], frame_t *frame)
{
	while (sprites[0] > 0) {
		draw_popup_icon(sprites[1], sprites[2], sprites[0], frame);
		sprites += 3;
	}
}

/* Translate resource amount to text. */
static const char *
prepare_res_amount_text(int amount)
{
	if (amount == 0) return "Not Present";
	else if (amount < 100) return "Minimum";
	else if (amount < 180) return "Very Few";
	else if (amount < 240) return "Few";
	else if (amount < 300) return "Below Average";
	else if (amount < 400) return "Average";
	else if (amount < 500) return "Above Average";
	else if (amount < 600) return "Much";
	else if (amount < 800) return "Very Much";
	return "Perfect";
}

/* Draw map popup box. */
static void
draw_map_box(player_t *player)
{
	player->clkmap = BOX_MAP_OVERLAY;
	/* TODO ... */
}

static void
draw_map_overlay_box(player_t *player)
{
	/* TODO ... */
}

/* Draw building mine popup box. */
static void
draw_mine_building_box(player_t *player)
{
	const int layout[] = {
		0xa3, 2, 8,
		0xa4, 8, 8,
		0xa5, 4, 77,
		0xa6, 10, 77,
		-1
	};

	draw_box_background(0x83, player->popup_frame);

	if (!BIT_TEST(player->sett->build, 1)) { /* Can build flag */
		draw_popup_building(2, 114, 0x80+4*player->sett->player_num, player->popup_frame);
	}

	draw_custom_bld_box(layout, player->popup_frame);
}

/* Draw .. popup box... */
static void
draw_basic_building_box(player_t *player, int flip)
{
	const int layout[] = {
		0xab, 10, 13, /* hut */
		0xa9, 2, 13,
		0xa8, 0, 58,
		0xaa, 6, 56,
		0xa7, 12, 55,
		0xbc, 2, 85,
		0xae, 10, 87,
		-1
	};

	draw_box_background(0x83, player->popup_frame);

	const int *l = layout;
	if (BIT_TEST(player->sett->build, 0)) { /* Can not build military building */
		l += 3; /* Skip hut */
	}

	draw_custom_bld_box(l, player->popup_frame);

	if (!BIT_TEST(player->sett->build, 1)) { /* Can build flag */
		draw_popup_building(8, 108, 0x80+4*player->sett->player_num, player->popup_frame);
	}

	if (flip) draw_popup_icon(0, 128, 0x3d, player->popup_frame);
}

static void
draw_adv_1_building_box(player_t *player)
{
	const int layout[] = {
		0x9c, 0, 15,
		0x9d, 8, 15,
		0xa1, 0, 50,
		0xa0, 8, 50,
		0xa2, 2, 100,
		0x9f, 10, 96,
		-1
	};

	draw_box_background(0x83, player->popup_frame);
	draw_custom_bld_box(layout, player->popup_frame);
	draw_popup_icon(0, 128, 0x3d, player->popup_frame);
}

static void
draw_adv_2_building_box(player_t *player)
{
	const int layout[] = {
		0x9e, 2, 99, /* tower */
		0x98, 8, 84, /* fortress */
		0x99, 0, 1,
		0xc0, 0, 46,
		0x9a, 8, 1,
		0x9b, 8, 45,
		-1
	};

	const int *l = layout;
	if (BIT_TEST(player->sett->build, 0)) { /* Can not build military building */
		l += 2*3; /* Skip tower and fortress */
	}

	draw_box_background(0x83, player->popup_frame);
	draw_custom_bld_box(l, player->popup_frame);
	draw_popup_icon(0, 128, 0x3d, player->popup_frame);
}

/* Draw generic popup box of resources. */
static void
draw_resources_box(player_t *player, const int resources[])
{
	const int layout[] = {
		0x28, 1, 0, /* resources */
		0x29, 1, 16,
		0x2a, 1, 32,
		0x2b, 1, 48,
		0x2e, 1, 64,
		0x2c, 1, 80,
		0x2d, 1, 96,
		0x2f, 1, 112,
		0x30, 1, 128,
		0x31, 6, 0,
		0x32, 6, 16,
		0x36, 6, 32,
		0x37, 6, 48,
		0x35, 6, 64,
		0x38, 6, 80,
		0x39, 6, 96,
		0x34, 6, 112,
		0x33, 6, 128,
		0x3a, 11, 0,
		0x3b, 11, 16,
		0x22, 11, 32,
		0x23, 11, 48,
		0x24, 11, 64,
		0x25, 11, 80,
		0x26, 11, 96,
		0x27, 11, 112,
		-1
	};

	draw_custom_icon_box(layout, player->popup_frame);

	/* First column */
	draw_green_number(3, 4, player->popup_frame, resources[RESOURCE_LUMBER]);
	draw_green_number(3, 20, player->popup_frame, resources[RESOURCE_PLANK]);
	draw_green_number(3, 36, player->popup_frame, resources[RESOURCE_BOAT]);
	draw_green_number(3, 52, player->popup_frame, resources[RESOURCE_STONE]);
	draw_green_number(3, 68, player->popup_frame, resources[RESOURCE_COAL]);
	draw_green_number(3, 84, player->popup_frame, resources[RESOURCE_IRONORE]);
	draw_green_number(3, 100, player->popup_frame, resources[RESOURCE_STEEL]);
	draw_green_number(3, 116, player->popup_frame, resources[RESOURCE_GOLDORE]);
	draw_green_number(3, 132, player->popup_frame, resources[RESOURCE_GOLDBAR]);

	/* Second column */
	draw_green_number(8, 4, player->popup_frame, resources[RESOURCE_SHOVEL]);
	draw_green_number(8, 20, player->popup_frame, resources[RESOURCE_HAMMER]);
	draw_green_number(8, 36, player->popup_frame, resources[RESOURCE_AXE]);
	draw_green_number(8, 52, player->popup_frame, resources[RESOURCE_SAW]);
	draw_green_number(8, 68, player->popup_frame, resources[RESOURCE_SCYTHE]);
	draw_green_number(8, 84, player->popup_frame, resources[RESOURCE_PICK]);
	draw_green_number(8, 100, player->popup_frame, resources[RESOURCE_PINCER]);
	draw_green_number(8, 116, player->popup_frame, resources[RESOURCE_CLEAVER]);
	draw_green_number(8, 132, player->popup_frame, resources[RESOURCE_ROD]);

	/* Third column */
	draw_green_number(13, 4, player->popup_frame, resources[RESOURCE_SWORD]);
	draw_green_number(13, 20, player->popup_frame, resources[RESOURCE_SHIELD]);
	draw_green_number(13, 36, player->popup_frame, resources[RESOURCE_FISH]);
	draw_green_number(13, 52, player->popup_frame, resources[RESOURCE_PIG]);
	draw_green_number(13, 68, player->popup_frame, resources[RESOURCE_MEAT]);
	draw_green_number(13, 84, player->popup_frame, resources[RESOURCE_WHEAT]);
	draw_green_number(13, 100, player->popup_frame, resources[RESOURCE_FLOUR]);
	draw_green_number(13, 116, player->popup_frame, resources[RESOURCE_BREAD]);
}

/* Draw generic popup box of serfs. */
static void
draw_serfs_box(player_t *player, const int serfs[], int total)
{
	const int layout[] = {
		0x9, 1, 0, /* serfs */
		0xa, 1, 16,
		0xb, 1, 32,
		0xc, 1, 48,
		0x21, 1, 64,
		0x20, 1, 80,
		0x1f, 1, 96,
		0x1e, 1, 112,
		0x1d, 1, 128,
		0xd, 6, 0,
		0xe, 6, 16,
		0x12, 6, 32,
		0xf, 6, 48,
		0x10, 6, 64,
		0x11, 6, 80,
		0x19, 6, 96,
		0x1a, 6, 112,
		0x1b, 6, 128,
		0x13, 11, 0,
		0x14, 11, 16,
		0x15, 11, 32,
		0x16, 11, 48,
		0x17, 11, 64,
		0x18, 11, 80,
		0x1c, 11, 96,
		0x82, 11, 112,
		-1
	};

	draw_custom_icon_box(layout, player->popup_frame);

	/* First column */
	draw_green_number(3, 4, player->popup_frame, serfs[SERF_TRANSPORTER]);
	draw_green_number(3, 20, player->popup_frame, serfs[SERF_SAILOR]);
	draw_green_number(3, 36, player->popup_frame, serfs[SERF_DIGGER]);
	draw_green_number(3, 52, player->popup_frame, serfs[SERF_BUILDER]);
	draw_green_number(3, 68, player->popup_frame, serfs[SERF_KNIGHT_4]);
	draw_green_number(3, 84, player->popup_frame, serfs[SERF_KNIGHT_3]);
	draw_green_number(3, 100, player->popup_frame, serfs[SERF_KNIGHT_2]);
	draw_green_number(3, 116, player->popup_frame, serfs[SERF_KNIGHT_1]);
	draw_green_number(3, 132, player->popup_frame, serfs[SERF_KNIGHT_0]);

	/* Second column */
	draw_green_number(8, 4, player->popup_frame, serfs[SERF_LUMBERJACK]);
	draw_green_number(8, 20, player->popup_frame, serfs[SERF_SAWMILLER]);
	draw_green_number(8, 36, player->popup_frame, serfs[SERF_SMELTER]);
	draw_green_number(8, 52, player->popup_frame, serfs[SERF_STONECUTTER]);
	draw_green_number(8, 68, player->popup_frame, serfs[SERF_FORESTER]);
	draw_green_number(8, 84, player->popup_frame, serfs[SERF_MINER]);
	draw_green_number(8, 100, player->popup_frame, serfs[SERF_BOATBUILDER]);
	draw_green_number(8, 116, player->popup_frame, serfs[SERF_TOOLMAKER]);
	draw_green_number(8, 132, player->popup_frame, serfs[SERF_WEAPONSMITH]);

	/* Third column */
	draw_green_number(13, 4, player->popup_frame, serfs[SERF_FISHER]);
	draw_green_number(13, 20, player->popup_frame, serfs[SERF_PIGFARMER]);
	draw_green_number(13, 36, player->popup_frame, serfs[SERF_BUTCHER]);
	draw_green_number(13, 52, player->popup_frame, serfs[SERF_FARMER]);
	draw_green_number(13, 68, player->popup_frame, serfs[SERF_MILLER]);
	draw_green_number(13, 84, player->popup_frame, serfs[SERF_BAKER]);
	draw_green_number(13, 100, player->popup_frame, serfs[SERF_GEOLOGIST]);
	draw_green_number(13, 116, player->popup_frame, serfs[SERF_GENERIC]);

	if (total >= 0) {
		draw_green_large_number(11, 132, player->popup_frame, total);
	}
}

static void
draw_stat_select_box(player_t *player)
{
	const int layout[] = {
		72, 1, 12,
		73, 6, 12,
		77, 11, 12,
		74, 1, 56,
		76, 6, 56,
		75, 11, 56,
		71, 1, 100,
		70, 6, 100,
		61, 12, 104, /* Flip */
		60, 14, 128, /* Exit */
		-1
	};

	draw_box_background(129, player->popup_frame);
	draw_custom_icon_box(layout, player->popup_frame);
}

static void
draw_stat_4_box(player_t *player)
{
	draw_box_background(129, player->popup_frame);

	int resources[26];
	memset(resources, '\0', 26*sizeof(int));

	/* Sum up resources of all inventories. */
	for (int i = 0; i < globals.max_ever_inventory_index; i++) {
		if (BIT_TEST(globals.inventories_bitmap[i>>3], 7-(i&7))) {
			inventory_t *inventory = game_get_inventory(i);
			if (inventory->player_num == player->sett->player_num) {
				for (int j = 0; j < 26; j++) {
					resources[j] += inventory->resources[j];
				}
			}
		}
	}

	/* Add extra resources. */
	resources[RESOURCE_PLANK] += player->sett->extra_planks;
	resources[RESOURCE_STONE] += player->sett->extra_stone;

	draw_resources_box(player, resources);

	draw_popup_icon(14, 128, 60, player->popup_frame); /* exit */
}

static void
draw_stat_bld_1_box(player_t *player)
{
	const int bld_layout[] = {
		192, 0, 5,
		171, 2, 77,
		158, 8, 7,
		152, 6, 69,
		-1
	};

	draw_box_background(129, player->popup_frame);

	draw_custom_bld_box(bld_layout, player->popup_frame);

	draw_green_number(2, 105, player->popup_frame, player->sett->completed_building_count[BUILDING_HUT]);
	draw_additional_number(3, 105, player->popup_frame, player->sett->incomplete_building_count[BUILDING_HUT]);

	draw_green_number(10, 53, player->popup_frame, player->sett->completed_building_count[BUILDING_TOWER]);
	draw_additional_number(11, 53, player->popup_frame, player->sett->incomplete_building_count[BUILDING_TOWER]);

	draw_green_number(9, 130, player->popup_frame, player->sett->completed_building_count[BUILDING_FORTRESS]);
	draw_additional_number(10, 130, player->popup_frame, player->sett->incomplete_building_count[BUILDING_FORTRESS]);

	draw_green_number(4, 61, player->popup_frame, player->sett->completed_building_count[BUILDING_STOCK]);
	draw_additional_number(5, 61, player->popup_frame, player->sett->incomplete_building_count[BUILDING_STOCK]);

	draw_popup_icon(0, 128, 61, player->popup_frame); /* flip */
	draw_popup_icon(14, 128, 60, player->popup_frame); /* exit */
}

static void
draw_stat_bld_2_box(player_t *player)
{
	const int bld_layout[] = {
		153, 0, 4,
		160, 8, 6,
		157, 0, 68,
		169, 8, 65,
		174, 12, 57,
		170, 4, 105,
		168, 8, 107,
		-1
	};

	draw_box_background(129, player->popup_frame);

	draw_custom_bld_box(bld_layout, player->popup_frame);

	draw_green_number(3, 54, player->popup_frame, player->sett->completed_building_count[BUILDING_TOOLMAKER]);
	draw_additional_number(4, 54, player->popup_frame, player->sett->incomplete_building_count[BUILDING_TOOLMAKER]);

	draw_green_number(10, 48, player->popup_frame, player->sett->completed_building_count[BUILDING_SAWMILL]);
	draw_additional_number(11, 48, player->popup_frame, player->sett->incomplete_building_count[BUILDING_SAWMILL]);

	draw_green_number(3, 95, player->popup_frame, player->sett->completed_building_count[BUILDING_WEAPONSMITH]);
	draw_additional_number(4, 95, player->popup_frame, player->sett->incomplete_building_count[BUILDING_WEAPONSMITH]);

	draw_green_number(8, 95, player->popup_frame, player->sett->completed_building_count[BUILDING_STONECUTTER]);
	draw_additional_number(9, 95, player->popup_frame, player->sett->incomplete_building_count[BUILDING_STONECUTTER]);

	draw_green_number(12, 95, player->popup_frame, player->sett->completed_building_count[BUILDING_BOATBUILDER]);
	draw_additional_number(13, 95, player->popup_frame, player->sett->incomplete_building_count[BUILDING_BOATBUILDER]);

	draw_green_number(5, 132, player->popup_frame, player->sett->completed_building_count[BUILDING_FORESTER]);
	draw_additional_number(6, 132, player->popup_frame, player->sett->incomplete_building_count[BUILDING_FORESTER]);

	draw_green_number(9, 132, player->popup_frame, player->sett->completed_building_count[BUILDING_LUMBERJACK]);
	draw_additional_number(10, 132, player->popup_frame, player->sett->incomplete_building_count[BUILDING_LUMBERJACK]);

	draw_popup_icon(0, 128, 61, player->popup_frame); /* flip */
	draw_popup_icon(14, 128, 60, player->popup_frame); /* exit */
}

static void
draw_stat_bld_3_box(player_t *player)
{
	const int bld_layout[] = {
		155, 0, 2,
		154, 8, 3,
		167, 0, 61,
		156, 8, 60,
		188, 4, 75,
		162, 8, 100,
		-1
	};

	draw_box_background(129, player->popup_frame);

	draw_custom_bld_box(bld_layout, player->popup_frame);

	draw_green_number(3, 48, player->popup_frame, player->sett->completed_building_count[BUILDING_PIGFARM]);
	draw_additional_number(4, 48, player->popup_frame, player->sett->incomplete_building_count[BUILDING_PIGFARM]);

	draw_green_number(11, 48, player->popup_frame, player->sett->completed_building_count[BUILDING_FARM]);
	draw_additional_number(12, 48, player->popup_frame, player->sett->incomplete_building_count[BUILDING_FARM]);

	draw_green_number(0, 92, player->popup_frame, player->sett->completed_building_count[BUILDING_FISHER]);
	draw_additional_number(1, 92, player->popup_frame, player->sett->incomplete_building_count[BUILDING_FISHER]);

	draw_green_number(11, 87, player->popup_frame, player->sett->completed_building_count[BUILDING_BUTCHER]);
	draw_additional_number(12, 87, player->popup_frame, player->sett->incomplete_building_count[BUILDING_BUTCHER]);

	draw_green_number(5, 134, player->popup_frame, player->sett->completed_building_count[BUILDING_MILL]);
	draw_additional_number(6, 134, player->popup_frame, player->sett->incomplete_building_count[BUILDING_MILL]);

	draw_green_number(10, 134, player->popup_frame, player->sett->completed_building_count[BUILDING_BAKER]);
	draw_additional_number(11, 134, player->popup_frame, player->sett->incomplete_building_count[BUILDING_BAKER]);

	draw_popup_icon(0, 128, 61, player->popup_frame); /* flip */
	draw_popup_icon(14, 128, 60, player->popup_frame); /* exit */
}

static void
draw_stat_bld_4_box(player_t *player)
{
	const int bld_layout[] = {
		163, 0, 4,
		164, 4, 4,
		165, 8, 4,
		166, 12, 4,
		161, 2, 90,
		159, 8, 90,
		-1
	};

	draw_box_background(129, player->popup_frame);

	draw_custom_bld_box(bld_layout, player->popup_frame);

	draw_green_number(0, 71, player->popup_frame, player->sett->completed_building_count[BUILDING_STONEMINE]);
	draw_additional_number(1, 71, player->popup_frame, player->sett->incomplete_building_count[BUILDING_STONEMINE]);

	draw_green_number(4, 71, player->popup_frame, player->sett->completed_building_count[BUILDING_COALMINE]);
	draw_additional_number(5, 71, player->popup_frame, player->sett->incomplete_building_count[BUILDING_COALMINE]);

	draw_green_number(8, 71, player->popup_frame, player->sett->completed_building_count[BUILDING_IRONMINE]);
	draw_additional_number(9, 71, player->popup_frame, player->sett->incomplete_building_count[BUILDING_IRONMINE]);

	draw_green_number(12, 71, player->popup_frame, player->sett->completed_building_count[BUILDING_GOLDMINE]);
	draw_additional_number(13, 71, player->popup_frame, player->sett->incomplete_building_count[BUILDING_GOLDMINE]);

	draw_green_number(4, 130, player->popup_frame, player->sett->completed_building_count[BUILDING_STEELSMELTER]);
	draw_additional_number(5, 130, player->popup_frame, player->sett->incomplete_building_count[BUILDING_STEELSMELTER]);

	draw_green_number(9, 130, player->popup_frame, player->sett->completed_building_count[BUILDING_GOLDSMELTER]);
	draw_additional_number(10, 130, player->popup_frame, player->sett->incomplete_building_count[BUILDING_GOLDSMELTER]);

	draw_popup_icon(0, 128, 61, player->popup_frame); /* flip */
	draw_popup_icon(14, 128, 60, player->popup_frame); /* exit */
}

static void
draw_player_stat_chart(const int *data, int index, int color, frame_t *frame)
{
	int prev_value = data[index];
	index = index > 0 ? index-1 : 111;

	for (int i = 0; i < 112; i++) {
		int value = data[index];
		index = index > 0 ? index-1 : 111;

		/* TODO There are glitches in drawing these charts. */

		if (value > prev_value) {
			int diff = value - prev_value;
			int h = diff/2;
			gfx_fill_rect(119 - i, 108 - h - prev_value, 1, h, color, frame);
			diff -= h;
			gfx_fill_rect(118 - i, 108 - prev_value, 1, diff, color, frame);
		} else {
			int diff = prev_value - value;
			int h = diff/2;
			gfx_fill_rect(119 - i, 108 - prev_value, 1, h, color, frame);
			diff -= h;
			gfx_fill_rect(118 - i, 108 - diff - prev_value, 1, diff, color, frame);
		}

		prev_value = value;
	}
}

static void
draw_stat_8_box(player_t *player)
{
	const int layout[] = {
		0x58, 14, 0,
		0x59, 0, 100,
		0x41, 8, 112,
		0x42, 10, 112,
		0x43, 8, 128,
		0x44, 10, 128,
		0x45, 2, 112,
		0x40, 4, 112,
		0x3e, 2, 128,
		0x3f, 4, 128,
		0x133, 14, 112,

		0x3c, 14, 128, /* exit */
		-1
	};

	int mode = player->current_stat_8_mode;
	int aspect = (mode >> 2) & 3;
	int scale = mode & 3;

	/* Draw background */
	draw_box_row(132+aspect, 0, player->popup_frame);
	draw_box_row(132+aspect, 16, player->popup_frame);
	draw_box_row(132+aspect, 32, player->popup_frame);
	draw_box_row(132+aspect, 48, player->popup_frame);
	draw_box_row(132+aspect, 64, player->popup_frame);
	draw_box_row(132+aspect, 80, player->popup_frame);
	draw_box_row(132+aspect, 96, player->popup_frame);

	draw_box_row(136, 108, player->popup_frame);
	draw_box_row(129, 116, player->popup_frame);
	draw_box_row(137, 132, player->popup_frame);

	draw_custom_icon_box(layout, player->popup_frame);

	/* Draw checkmarks to indicate current settings. */
	draw_popup_icon(!BIT_TEST(aspect, 0) ? 1 : 6,
			!BIT_TEST(aspect, 1) ? 116 : 132,
			106, player->popup_frame); /* checkmark */

	draw_popup_icon(!BIT_TEST(scale, 0) ? 7 : 12,
			!BIT_TEST(scale, 1) ? 116 : 132,
			106, player->popup_frame); /* checkmark */

	/* Correct numbers on time scale. */
	draw_popup_icon(2, 103, 94 + 3*scale + 0, player->popup_frame);
	draw_popup_icon(6, 103, 94 + 3*scale + 1, player->popup_frame);
	draw_popup_icon(10, 103, 94 + 3*scale + 2, player->popup_frame);

	/* Draw chart */
	int index = globals.player_history_index[scale];
	draw_player_stat_chart(globals.player_sett[3]->player_stat_history[mode], index, 76, player->popup_frame);
	draw_player_stat_chart(globals.player_sett[2]->player_stat_history[mode], index, 68, player->popup_frame);
	draw_player_stat_chart(globals.player_sett[1]->player_stat_history[mode], index, 72, player->popup_frame);
	draw_player_stat_chart(globals.player_sett[0]->player_stat_history[mode], index, 64, player->popup_frame);
}

static void
draw_stat_7_box(player_t *player)
{
	const int layout[] = {
		0x81, 6, 80,
		0x81, 8, 80,
		0x81, 6, 96,
		0x81, 8, 96,

		0x59, 0, 64,
		0x5a, 14, 0,

		0x28, 0, 75, /* lumber */
		0x29, 2, 75, /* plank */
		0x2b, 4, 75, /* stone */
		0x2e, 0, 91, /* coal */
		0x2c, 2, 91, /* ironore */
		0x2f, 4, 91, /* goldore */
		0x2a, 0, 107, /* boat */
		0x2d, 2, 107, /* iron */
		0x30, 4, 107, /* goldbar */
		0x3a, 7, 83, /* sword */
		0x3b, 7, 99, /* shield */
		0x31, 10, 75, /* shovel */
		0x32, 12, 75, /* hammer */
		0x36, 14, 75, /* axe */
		0x37, 10, 91, /* saw */
		0x38, 12, 91, /* pick */
		0x35, 14, 91, /* scythe */
		0x34, 10, 107, /* cleaver */
		0x39, 12, 107, /* pincer */
		0x33, 14, 107, /* rod */
		0x22, 1, 125, /* fish */
		0x23, 3, 125, /* pig */
		0x24, 5, 125, /* meat */
		0x25, 7, 125, /* wheat */
		0x26, 9, 125, /* flour */
		0x27, 11, 125, /* bread */

		0x3c, 14, 128, /* exitbox */
		-1
	};

	draw_box_row(129, 64, player->popup_frame);
	draw_box_row(129, 112, player->popup_frame);
	draw_box_row(129, 128, player->popup_frame);

	draw_custom_icon_box(layout, player->popup_frame);

	int item = player->current_stat_7_item-1;

	/* Draw background of chart */
	for (int y = 0; y < 64; y += 16) {
		for (int x = 0; x < 14; x += 2) {
			draw_popup_icon(x, y, 138 + item, player->popup_frame);
		}
	}

	const int sample_weights[] = { 4, 6, 8, 9, 10, 9, 8, 6, 4 };

	/* Create array of historical counts */
	int historical_data[112];
	int max_val = 0;
	int index = globals.resource_history_index;

	for (int i = 0; i < 112; i++) {
		historical_data[i] = 0;
		int j = index;
		for (int k = 0; k < 9; k++) {
			historical_data[i] += sample_weights[k]*player->sett->resource_count_history[item][j];
			j = j > 0 ? j-1 : 119;
		}

		if (historical_data[i] > max_val) {
			max_val = historical_data[i];
		}

		index = index > 0 ? index-1 : 119;
	}

	const int axis_icons_1[] = { 110, 109, 108, 107 };
	const int axis_icons_2[] = { 112, 111, 110, 108 };
	const int axis_icons_3[] = { 114, 113, 112, 110 };
	const int axis_icons_4[] = { 117, 116, 114, 112 };
	const int axis_icons_5[] = { 120, 119, 118, 115 };
	const int axis_icons_6[] = { 122, 121, 120, 118 };
	const int axis_icons_7[] = { 125, 124, 122, 120 };
	const int axis_icons_8[] = { 128, 127, 126, 123 };

	const int *axis_icons = NULL;
	int multiplier = 0;

	/* TODO chart background pattern */

	if (max_val <= 64) {
		axis_icons = axis_icons_1;
		multiplier = 0x8000;
	} else if (max_val <= 128) {
		axis_icons = axis_icons_2;
		multiplier = 0x4000;
	} else if (max_val <= 256) {
		axis_icons = axis_icons_3;
		multiplier = 0x2000;
	} else if (max_val <= 512) {
		axis_icons = axis_icons_4;
		multiplier = 0x1000;
	} else if (max_val <= 1280) {
		axis_icons = axis_icons_5;
		multiplier = 0x666;
	} else if (max_val <= 2560) {
		axis_icons = axis_icons_6;
		multiplier = 0x333;
	} else if (max_val <= 5120) {
		axis_icons = axis_icons_7;
		multiplier = 0x199;
	} else {
		axis_icons = axis_icons_8;
		multiplier = 0xa3;
	}

	/* Draw axis icons */
	for (int i = 0; i < 4; i++) {
		draw_popup_icon(14, i*16, axis_icons[i], player->popup_frame);
	}

	/* Draw chart */
	for (int i = 0; i < 112; i++) {
		int value = min((historical_data[i]*multiplier) >> 16, 64);
		if (value > 0) {
			gfx_fill_rect(119 - i, 73 - value, 1, value, 72, player->popup_frame);
		}
	}
}

static void
draw_stat_1_box(player_t *player)
{
	const int layout[] = {
		0x18, 0, 0, /* baker */
		0xb4, 0, 16,
		0xb3, 0, 24,
		0xb2, 0, 32,
		0xb3, 0, 40,
		0xb2, 0, 48,
		0xb3, 0, 56,
		0xb2, 0, 64,
		0xb3, 0, 72,
		0xb2, 0, 80,
		0xb3, 0, 88,
		0xd4, 0, 96,
		0xb1, 0, 112,
		0x13, 0, 120, /* fisher */
		0x15, 2, 48, /* butcher */
		0xb4, 2, 64,
		0xb3, 2, 72,
		0xd4, 2, 80,
		0xa4, 2, 96,
		0xa4, 2, 112,
		0xae, 4, 4,
		0xae, 4, 36,
		0xa6, 4, 80,
		0xa6, 4, 96,
		0xa6, 4, 112,
		0x26, 6, 0, /* flour */
		0x23, 6, 32, /* pig */
		0xb5, 6, 64,
		0x24, 6, 76, /* meat */
		0x27, 6, 92, /* bread */
		0x22, 6, 108, /* fish */
		0xb6, 6, 124,
		0x17, 8, 0, /* miller */
		0x14, 8, 32, /* pigfarmer */
		0xa6, 8, 64,
		0xab, 8, 88,
		0xab, 8, 104,
		0xa6, 8, 128,
		0xba, 12, 8,
		0x11, 12, 56, /* miner */
		0x11, 12, 80, /* miner */
		0x11, 12, 104, /* miner */
		0x11, 12, 128, /* miner */
		0x16, 14, 0, /* farmer */
		0x25, 14, 16, /* wheat */
		0x2f, 14, 56, /* goldore */
		0x2e, 14, 80, /* coal */
		0x2c, 14, 104, /* ironore */
		0x2b, 14, 128, /* stone */
		-1
	};

	draw_box_background(129, player->popup_frame);

	draw_custom_icon_box(layout, player->popup_frame);

	/* TODO */
}

static void
draw_stat_2_box(player_t *player)
{
	const int layout[] = {
		0x11, 0, 0, /* miner */
		0x11, 0, 24, /* miner */
		0x11, 0, 56, /* miner */
		0xd, 0, 80, /* lumberjack */
		0x11, 0, 104, /* miner */
		0xf, 0, 128, /* stonecutter */
		0x2f, 2, 0, /* goldore */
		0x2e, 2, 24, /* coal */
		0xb0, 2, 40,
		0x2c, 2, 56, /* ironore */
		0x28, 2, 80, /* lumber */
		0x2b, 2, 104, /* stone */
		0x2b, 2, 128, /* stone */
		0xaa, 4, 4,
		0xab, 4, 24,
		0xad, 4, 32,
		0xa8, 4, 40,
		0xac, 4, 60,
		0xaa, 4, 84,
		0xbb, 4, 108,
		0xa4, 6, 32,
		0xe, 6, 96, /* sawmiller */
		0xa5, 6, 132,
		0x30, 8, 0, /* gold */
		0x12, 8, 16, /* smelter */
		0xa4, 8, 32,
		0x2d, 8, 40, /* steel */
		0x12, 8, 56, /* smelter */
		0xb8, 8, 80,
		0x29, 8, 96, /* planks */
		0xaf, 8, 112,
		0xa5, 8, 132,
		0xaa, 10, 4,
		0xb9, 10, 24,
		0xab, 10, 40,
		0xb7, 10, 48,
		0xa6, 10, 80,
		0xa9, 10, 96,
		0xa6, 10, 112,
		0xa7, 10, 132,
		0x21, 14, 0, /* knight 4 */
		0x1b, 14, 28, /* weaponsmith */
		0x1a, 14, 64, /* toolmaker */
		0x19, 14, 92, /* boatbuilder */
		0xc, 14, 120, /* builder */
		-1
	};

	draw_box_background(129, player->popup_frame);

	draw_custom_icon_box(layout, player->popup_frame);

	/* TODO */
}

static void
draw_stat_6_box(player_t *player)
{
	draw_box_background(129, player->popup_frame);

	int total = 0;
	for (int i = 0; i < 27; i++) {
		if (i != SERF_4) total += player->sett->serf_count[i];
	}

	draw_serfs_box(player, player->sett->serf_count, total);

	draw_popup_icon(14, 128, 60, player->popup_frame); /* exit */
}

static void
draw_stat_3_box(player_t *player)
{
	draw_box_background(129, player->popup_frame);

	int serfs[27];
	memset(serfs, '\0', 27*sizeof(int));

	/* Sum up all existing serfs. */
	for (int i = 1; i < globals.max_ever_serf_index; i++) {
		if (BIT_TEST(globals.serfs_bitmap[i>>3], 7-(i&7))) {
			serf_t *serf = game_get_serf(i);
			if (SERF_PLAYER(serf) == player->sett->player_num &&
			    serf->state == SERF_STATE_IDLE_IN_STOCK) {
				serfs[SERF_TYPE(serf)] += 1;
			}
		}
	}

	/* Sum up potential serfs of all inventories. */
	for (int i = 0; i < globals.max_ever_inventory_index; i++) {
		if (BIT_TEST(globals.inventories_bitmap[i>>3], 7-(i&7))) {
			inventory_t *inventory = game_get_inventory(i);
			if (inventory->player_num == player->sett->player_num) {
				/* TODO */
			}
		}
	}

	/* TODO draw meters */
	draw_serfs_box(player, serfs, -1);

	draw_popup_icon(14, 128, 60, player->popup_frame); /* exit */
}

static void
draw_start_attack_redraw_box(player_t *player)
{
	/* TODO Should overwrite the previously drawn number.
	   Just use fill_rect(), perhaps? */
	draw_green_string(6, 116, player->popup_frame, "    ");
	draw_green_number(7, 116, player->popup_frame, player->sett->knights_attacking);
}

static void
draw_start_attack_box(player_t *player)
{
	const int building_layout[] = {
		0x0, 2, 33,
		0xa, 6, 30,
		0x7, 10, 33,
		0xc, 14, 30,
		0xe, 2, 36,
		0x2, 6, 39,
		0xb, 10, 36,
		0x4, 12, 39,
		0x8, 8, 42,
		0xf, 12, 42,
		-1
	};

	const int icon_layout[] = {
		216, 1, 80,
		217, 5, 80,
		218, 9, 80,
		219, 13, 80,
		220, 4, 112,
		221, 10, 112,
		222, 0, 128,
		60, 14, 128,
		-1
	};

	draw_box_background(131, player->popup_frame);

	for (int i = 0; building_layout[i] >= 0; i += 3) {
		draw_popup_building(building_layout[i+1], building_layout[i+2],
				    building_layout[i], player->popup_frame);
	}

	building_t *building = game_get_building(player->sett->building_attacked);
	int y = 0;

	switch (BUILDING_TYPE(building)) {
	case BUILDING_HUT: y = 50; break;
	case BUILDING_TOWER: y = 32; break;
	case BUILDING_FORTRESS: y = 17; break;
	case BUILDING_CASTLE: y = 0; break;
	default: NOT_REACHED(); break;
	}

	draw_popup_building(0, y, map_building_sprite[BUILDING_TYPE(building)],
			    player->popup_frame);
	draw_custom_icon_box(icon_layout, player->popup_frame);

	draw_start_attack_redraw_box(player);
}

static void
draw_ground_analysis_box(player_t *player)
{
	const int layout[] = {
		0x1c, 7, 10,
		0x2f, 1, 50,
		0x2c, 1, 70,
		0x2e, 1, 90,
		0x2b, 1, 110,
		0x3c, 14, 128,
		-1
	};

	draw_box_background(0x81, player->popup_frame);
	draw_custom_icon_box(layout, player->popup_frame);
	game_prepare_ground_analysis(player);
	draw_green_string(0, 30, player->popup_frame, "GROUND-ANALYSIS:");

	/* Gold */
	const char *s = prepare_res_amount_text(2*player->sett->analysis_goldore);
	draw_green_string(3, 54, player->popup_frame, s);

	/* Iron */
	s = prepare_res_amount_text(player->sett->analysis_ironore);
	draw_green_string(3, 74, player->popup_frame, s);

	/* Coal */
	s = prepare_res_amount_text(player->sett->analysis_coal);
	draw_green_string(3, 94, player->popup_frame, s);

	/* Stone */
	s = prepare_res_amount_text(2*player->sett->analysis_stone);
	draw_green_string(3, 114, player->popup_frame, s);
}

static void
draw_sett_select_box(player_t *player)
{
	const int layout[] = {
		230, 1, 8,
		231, 6, 8,
		232, 11, 8,
		234, 1, 48,
		235, 6, 48,
		299, 11, 48,
		233, 1, 88,
		298, 6, 88,
		61, 12, 104,
		60, 14, 128,

		285, 4, 128,
		286, 0, 128,
		224, 8, 128,
		-1
	};

	draw_box_background(311, player->popup_frame);
	draw_custom_icon_box(layout, player->popup_frame);
}

/* Draw slide bar in a popup box. */
static void
draw_slide_bar(int x, int y, int value, frame_t *frame)
{
	draw_popup_icon(x, y, 236, frame);

	int width = value/1310;
	if (width > 0) {
		gfx_fill_rect(8*x+15, y+11, width, 4, 30, frame);
	}
}

static void
draw_sett_1_box(player_t *player)
{
	const int bld_layout[] = {
		163, 12, 21,
		164, 8, 41,
		165, 4, 61,
		166, 0, 81,
		-1
	};

	const int layout[] = {
		34, 4, 1,
		36, 7, 1,
		39, 10, 1,
		60, 14, 128,
		295, 1, 8,
		-1
	};

	draw_box_background(311, player->popup_frame);
	draw_custom_bld_box(bld_layout, player->popup_frame);
	draw_custom_icon_box(layout, player->popup_frame);

	draw_slide_bar(4, 21, player->sett->food_stonemine, player->popup_frame);
	draw_slide_bar(0, 41, player->sett->food_coalmine, player->popup_frame);
	draw_slide_bar(8, 114, player->sett->food_ironmine, player->popup_frame);
	draw_slide_bar(4, 133, player->sett->food_goldmine, player->popup_frame);
}

static void
draw_sett_2_box(player_t *player)
{
	const int bld_layout[] = {
		186, 2, 0,
		174, 2, 41,
		153, 8, 54,
		157, 0, 102,
		-1
	};

	const int layout[] = {
		41, 9, 25,
		45, 9, 119,
		60, 14, 128,
		295, 13, 8,
		-1
	};

	draw_box_background(311, player->popup_frame);
	draw_custom_bld_box(bld_layout, player->popup_frame);
	draw_custom_icon_box(layout, player->popup_frame);

	draw_slide_bar(0, 26, player->sett->planks_construction, player->popup_frame);
	draw_slide_bar(0, 36, player->sett->planks_boatbuilder, player->popup_frame);
	draw_slide_bar(8, 44, player->sett->planks_toolmaker, player->popup_frame);
	draw_slide_bar(8, 103, player->sett->steel_toolmaker, player->popup_frame);
	draw_slide_bar(0, 130, player->sett->steel_weaponsmith, player->popup_frame);
}

static void
draw_sett_3_box(player_t *player)
{
	const int bld_layout[] = {
		161, 0, 1,
		159, 10, 0,
		157, 4, 56,
		188, 12, 61,
		155, 0, 101,
		-1
	};

	const int layout[] = {
		46, 7, 19,
		37, 8, 101,
		60, 14, 128,
		295, 1, 60,
		-1
	};

	draw_box_background(311, player->popup_frame);
	draw_custom_bld_box(bld_layout, player->popup_frame);
	draw_custom_icon_box(layout, player->popup_frame);

	draw_slide_bar(0, 39, player->sett->coal_steelsmelter, player->popup_frame);
	draw_slide_bar(8, 39, player->sett->coal_goldsmelter, player->popup_frame);
	draw_slide_bar(4, 47, player->sett->coal_weaponsmith, player->popup_frame);
	draw_slide_bar(0, 92, player->sett->wheat_pigfarm, player->popup_frame);
	draw_slide_bar(8, 118, player->sett->wheat_mill, player->popup_frame);
}

static void
draw_knight_level_box(player_t *player)
{
	const char *level_str[] = {
		"Minimum", "Weak", "Medium", "Good", "Full"
	};

	const int layout[] = {
		226, 0, 2,
		227, 0, 36,
		228, 0, 70,
		229, 0, 104,

		220, 4, 2,  /* minus */
		221, 6, 2,  /* plus */
		220, 4, 18, /* ... */
		221, 6, 18,
		220, 4, 36,
		221, 6, 36,
		220, 4, 52,
		221, 6, 52,
		220, 4, 70,
		221, 6, 70,
		220, 4, 86,
		221, 6, 86,
		220, 4, 104,
		221, 6, 104,
		220, 4, 120,
		221, 6, 120,

		60, 14, 128, /* exit */
		-1
	};

	draw_box_background(311, player->popup_frame);

	player_sett_t *sett = player->sett;
	draw_green_string(8, 8, player->popup_frame,
			  level_str[sett->knight_occupation[3] & 0xf]);
	draw_green_string(8, 19, player->popup_frame,
			  level_str[(sett->knight_occupation[3] >> 4) & 0xf]);
	draw_green_string(8, 42, player->popup_frame,
			  level_str[sett->knight_occupation[2] & 0xf]);
	draw_green_string(8, 53, player->popup_frame,
			  level_str[(sett->knight_occupation[2] >> 4) & 0xf]);
	draw_green_string(8, 76, player->popup_frame,
			  level_str[sett->knight_occupation[1] & 0xf]);
	draw_green_string(8, 87, player->popup_frame,
			  level_str[(sett->knight_occupation[1] >> 4) & 0xf]);
	draw_green_string(8, 110, player->popup_frame,
			  level_str[sett->knight_occupation[0] & 0xf]);
	draw_green_string(8, 121, player->popup_frame,
			  level_str[(sett->knight_occupation[0] >> 4) & 0xf]);

	draw_custom_icon_box(layout, player->popup_frame);
}

static void
draw_sett_4_box(player_t *player)
{
	const int layout[] = {
		49, 1, 0, /* shovel */
		50, 1, 16, /* hammer */
		54, 1, 32, /* axe */
		55, 1, 48, /* saw */
		53, 1, 64, /* scythe */
		56, 1, 80, /* pick */
		57, 1, 96, /* pincer */
		52, 1, 112, /* cleaver */
		51, 1, 128, /* rod */

		60, 14, 128, /* exit*/
		295, 13, 8, /* default */
		-1
	};

	draw_box_background(311, player->popup_frame);
	draw_custom_icon_box(layout, player->popup_frame);

	draw_slide_bar(4, 4, player->sett->tool_prio[0], player->popup_frame); /* shovel */
	draw_slide_bar(4, 20, player->sett->tool_prio[1], player->popup_frame); /* hammer */
	draw_slide_bar(4, 36, player->sett->tool_prio[5], player->popup_frame); /* axe */
	draw_slide_bar(4, 52, player->sett->tool_prio[6], player->popup_frame); /* saw */
	draw_slide_bar(4, 68, player->sett->tool_prio[4], player->popup_frame); /* scythe */
	draw_slide_bar(4, 84, player->sett->tool_prio[7], player->popup_frame); /* pick */
	draw_slide_bar(4, 100, player->sett->tool_prio[8], player->popup_frame); /* pincer */
	draw_slide_bar(4, 116, player->sett->tool_prio[3], player->popup_frame); /* cleaver */
	draw_slide_bar(4, 132, player->sett->tool_prio[2], player->popup_frame); /* rod */
}

/* Draw generic popup box of resource stairs. */
static void
draw_popup_resource_stairs(int order[], frame_t *frame)
{
	const int spiral_layout[] = {
		5, 4,
		7, 6,
		9, 8,
		11, 10,
		13, 12,
		13, 28,
		11, 30,
		9, 32,
		7, 34,
		5, 36,
		3, 38,
		1, 40,
		1, 56,
		3, 58,
		5, 60,
		7, 62,
		9, 64,
		11, 66,
		13, 68,
		13, 84,
		11, 86,
		9, 88,
		7, 90,
		5, 92,
		3, 94,
		1, 96
	};

	for (int i = 0; i < 26; i++) {
		int pos = 26 - order[i];
		draw_popup_icon(spiral_layout[2*pos],
				spiral_layout[2*pos+1],
				34+i, frame);
	}
}

static void
draw_sett_5_box(player_t *player)
{
	const int layout[] = {
		237, 1, 120, /* up */
		238, 3, 120, /* smallup */
		239, 9, 120, /* smalldown */
		240, 11, 120, /* down */
		295, 1, 4, /* default*/
		60, 14, 128, /* exit */
		-1
	};

	draw_box_background(311, player->popup_frame);
	draw_custom_icon_box(layout, player->popup_frame);
	draw_popup_resource_stairs(player->sett->flag_prio, player->popup_frame);

	draw_popup_icon(6, 120, 33+player->sett->current_sett_5_item, player->popup_frame);
}

static void
draw_quit_confirm_box(player_t *player)
{
	game_pause(1);

	draw_box_background(310, player->popup_frame);

	draw_green_string(0, 10, player->popup_frame, "   Do you want");
	draw_green_string(0, 20, player->popup_frame, "     to quit");
	draw_green_string(0, 30, player->popup_frame, "   this game?");
	draw_green_string(0, 45, player->popup_frame, "  Yes       No");

	/* wait_x_timer_ticks(8); */

	globals.svga &= ~BIT(5);
}

static void
draw_no_save_quit_confirm_box(player_t *player)
{
	draw_green_string(0, 70, player->popup_frame, "The game has not");
	draw_green_string(0, 80, player->popup_frame, "   been saved");
	draw_green_string(0, 90, player->popup_frame, "   recently.");
	draw_green_string(0, 100, player->popup_frame, "    Are you");
	draw_green_string(0, 110, player->popup_frame, "     sure?");
	draw_green_string(0, 125, player->popup_frame, "  Yes       No");
}

static void
draw_options_box(player_t *player)
{
	draw_box_background(310, player->popup_frame);

	draw_popup_icon(0, 0, 311, player->popup_frame);
	draw_popup_icon(2, 0, 311, player->popup_frame);
	draw_popup_icon(4, 0, 311, player->popup_frame);
	draw_popup_icon(10, 0, 311, player->popup_frame);
	draw_popup_icon(12, 0, 311, player->popup_frame);
	draw_popup_icon(14, 0, 311, player->popup_frame);

	draw_popup_icon(0, 8, 311, player->popup_frame);
	draw_popup_icon(2, 8, 311, player->popup_frame);
	draw_popup_icon(4, 8, 311, player->popup_frame);
	draw_popup_icon(10, 8, 311, player->popup_frame);
	draw_popup_icon(12, 8, 311, player->popup_frame);
	draw_popup_icon(14, 8, 311, player->popup_frame);

	draw_green_string(0, 2, player->popup_frame, "Left       Right");
	draw_green_string(0, 11, player->popup_frame, "side        side");

	draw_green_string(2, 28, player->popup_frame, "  Pathway-");
	draw_green_string(2, 37, player->popup_frame, " scrolling");
	draw_green_string(2, 48, player->popup_frame, "    Fast");
	draw_green_string(2, 57, player->popup_frame, "  map click");
	draw_green_string(2, 68, player->popup_frame, "    Fast");
	draw_green_string(2, 77, player->popup_frame, "  building");

	draw_green_string(0, 88, player->popup_frame, "    Messages");

	/* TODO messages options */

	draw_popup_icon(7, 0, 60, player->popup_frame); /* exit */

	draw_popup_icon(0, 28, BIT_TEST(globals.cfg_left, 0) ? 288 : 220, player->popup_frame);
	draw_popup_icon(0, 48, BIT_TEST(globals.cfg_left, 1) ? 288 : 220, player->popup_frame);
	draw_popup_icon(0, 68, BIT_TEST(globals.cfg_left, 2) ? 288 : 220, player->popup_frame);

	draw_popup_icon(14, 28, BIT_TEST(globals.cfg_right, 0) ? 288 : 220, player->popup_frame);
	draw_popup_icon(14, 48, BIT_TEST(globals.cfg_right, 1) ? 288 : 220, player->popup_frame);
	draw_popup_icon(14, 68, BIT_TEST(globals.cfg_right, 2) ? 288 : 220, player->popup_frame);

	draw_green_string(2, 110, player->popup_frame, "Music");
	draw_green_string(7, 105, player->popup_frame, "  SVGA"); /* TODO replace with fullscreen? */
	draw_green_string(7, 114, player->popup_frame, "  mode"); /* TODO replace with fullscreen? */
	draw_green_string(0, 125, player->popup_frame, ""); /* What is it? Look on Amiga?. */
	draw_green_string(0, 134, player->popup_frame, " Volume:");

	draw_popup_icon(0, 106, midi_is_enabled() ? 288 : 220, player->popup_frame); /* Music */
	draw_popup_icon(14, 106, 288, player->popup_frame); /* SVGA */ /* TODO replace with fullscreen? */
	draw_popup_icon(12, 126, 220, player->popup_frame); /* Volude minus */
	draw_popup_icon(14, 126, 221, player->popup_frame); /* Volude plus */

	char volume[4] = {0};
	sprintf(volume, "%d", audio_volume());
	draw_green_string(9, 134, player->popup_frame, volume);
}

static void
draw_castle_res_box(player_t *player)
{
	const int layout[] = {
		0x3d, 12, 128, /* flip */
		0x3c, 14, 128, /* exit */
		-1
	};

	draw_box_background(0x138, player->popup_frame);
	draw_custom_icon_box(layout, player->popup_frame);

	if (player->sett->index == 0) return;/*close_box();*/

	building_t *building = game_get_building(player->sett->index);
	if (BIT_TEST(building->serf, 5)) return;/*close_box();*/ /* Building is burning */

	inventory_t *inventory = building->u.inventory;
	if (BUILDING_TYPE(building) == BUILDING_STOCK) {
		/* TODO supply zeroed array to draw_resources_box */
	} else if (BUILDING_TYPE(building) == BUILDING_CASTLE) {
		/* TODO add extra planks and extra stone */
	} else {
		return;/*close_box();*/
	}

	draw_resources_box(player, inventory->resources);
}

static void
draw_mine_output_box(player_t *player)
{
	draw_box_background(0x138, player->popup_frame);

	if (player->sett->index == 0) return;/*close_box();*/

	building_t *building = game_get_building(player->sett->index);
	/* if (BIT_TEST(building->serf, 5)) close_box();*/ /* Building is burning */
	building_type_t type = BUILDING_TYPE(building);

	if (type != BUILDING_STONEMINE &&
	    type != BUILDING_COALMINE &&
	    type != BUILDING_IRONMINE &&
	    type != BUILDING_GOLDMINE) {
		return;/*close_box();*/
	}

	/* Draw building */
	draw_popup_building(6, 60, map_building_sprite[type], player->popup_frame);

	/* Draw serf icon */
	int sprite = 0xdc; /* minus box */
	if (BIT_TEST(building->serf, 6)) sprite = 0x11; /* miner */

	draw_popup_icon(10, 75, sprite, player->popup_frame);

	/* Draw food present at mine */
	int stock = (building->stock1 >> 4) & 0xf;
	int stock_left_col = (stock + 1) >> 1;
	int stock_right_col = stock >> 1;

	/* Left column */
	for (int i = 0; i < stock_left_col; i++) {
		draw_popup_icon(1, 90 - 8*stock_left_col + i*16, 0x24, player->popup_frame); /* meat (food) sprite */
	}

	/* Right column */
	for (int i = 0; i < stock_right_col; i++) {
		draw_popup_icon(13, 90 - 8*stock_right_col + i*16, 0x24, player->popup_frame); /* meat (food) sprite */
	}

	/* Calculate output percentage (simple WMA) */
	const int output_weight[] = { 10, 10, 9, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 3, 2, 1 };
	int output = 0;
	for (int i = 0; i < 15; i++) {
		output += !!BIT_TEST(building->progress, i) * output_weight[i];
	}

	/* Print output precentage */
	int x = 7;
	if (output >= 100) x += 1;
	if (output >= 10) x += 1;
	draw_green_string(x, 38, player->popup_frame, "%");
	draw_green_number(6, 38, player->popup_frame, output);

	draw_green_string(1, 14, player->popup_frame, "MINING");
	draw_green_string(1, 24, player->popup_frame, "OUTPUT:");

	/* Exit box */
	draw_popup_icon(14, 128, 0x3c, player->popup_frame);
}

static void
draw_ordered_building_box(player_t *player)
{
	draw_box_background(0x138, player->popup_frame);

	if (player->sett->index == 0) return;/*close_box();*/

	building_t *building = game_get_building(player->sett->index);
	/* if (BIT_TEST(building->serf, 5)) close_box();*/ /* Building is burning */
	building_type_t type = BUILDING_TYPE(building);

	int sprite = map_building_sprite[type];
	int x = 6;
	if (sprite == 0xc0 /*stock*/ || sprite >= 0x9e /*tower*/) x = 4;
	draw_popup_building(x, 40, sprite, player->popup_frame);

	draw_green_string(2, 4, player->popup_frame, "Ordered");
	draw_green_string(2, 14, player->popup_frame, "Building");

	if (BIT_TEST(building->serf, 6)) { /* Serf present */
		if (building->progress == 0) draw_popup_icon(2, 100, 0xb, player->popup_frame); /* Digger */
		else draw_popup_icon(2, 100, 0xc, player->popup_frame); /* Builder */
	} else {
		draw_popup_icon(2, 100, 0xdc, player->popup_frame); /* Minus box */
	}

	draw_popup_icon(14, 128, 0x3c, player->popup_frame); /* Exit box */
}

static void
draw_defenders_box(player_t *player)
{
	draw_box_background(0x138, player->popup_frame);

	if (player->sett->index == 0) return;/*close_box();*/

	building_t *building = game_get_building(player->sett->index);
	if (BUILDING_IS_BURNING(building)) return;/*close_box();*/ /* Building is burning */

	if (!BIT_TEST(globals.split, 5) && /* Demo mode */
	    BUILDING_PLAYER(building) != player->sett->player_num) {
		return;/*close_box();*/
	}

	if (BUILDING_TYPE(building) != BUILDING_HUT &&
	    BUILDING_TYPE(building) != BUILDING_TOWER &&
	    BUILDING_TYPE(building) != BUILDING_FORTRESS) {
		return;/*close_box();*/
	}

	/* Draw building sprite */
	int sprite = map_building_sprite[BUILDING_TYPE(building)];
	int x = 0, y = 0;
	switch (BUILDING_TYPE(building)) {
	case BUILDING_HUT: x = 6; y = 20; break;
	case BUILDING_TOWER: x = 4; y = 6; break;
	case BUILDING_FORTRESS: x = 4; y = 1; break;
	default: NOT_REACHED(); break;
	}

	draw_popup_building(x, y, sprite, player->popup_frame);

	/* Draw gold stock */
	if (building->stock2 & 0xf0) {
		int amount = (building->stock2 >> 4) & 0xf;

		int left = (amount + 1) / 2;
		for (int i = 0; i < left; i++) {
			draw_popup_icon(1, 32 - 8*left + 16*i, 0x30, player->popup_frame);
		}

		int right = amount / 2;
		for (int i = 0; i < right; i++) {
			draw_popup_icon(13, 32 - 8*right + 16*i, 0x30, player->popup_frame);
		}
	}

	/* Draw heading string */
	draw_green_string(3, 62, player->popup_frame, "DEFENDERS:");

	/* Draw knights */
	int next_knight = building->serf_index;
	for (int i = 0; next_knight != 0; i++) {
		serf_t *serf = game_get_serf(next_knight);
		draw_popup_icon(3 + 4*(i%4), 72 + 14*(i/4), 7 + SERF_TYPE(serf), player->popup_frame);
		next_knight = serf->s.defending.next_knight;
	}

	draw_green_string(0, 128, player->popup_frame, "STATE:");
	draw_green_number(7, 128, player->popup_frame, building->serf & 3);

	draw_popup_icon(14, 128, 0x3c, player->popup_frame); /* Exit box */
}

static void
draw_transport_info_box(player_t *player)
{
	const int layout[] = {
		9, 24,
		5, 24,
		3, 44,
		5, 64,
		9, 64,
		11, 44
	};

	draw_box_background(0x138, player->popup_frame);
	if (!BIT_TEST(player->sett->flags, 7)) { /* Not AI (???) */
		/* TODO int r = ... */
		/* if (r == 0) draw_popup_icon(7, 51, 0x135, player->popup_frame); */
	}

	if (player->sett->index == 0) return;/*close_box();*/

	flag_t *flag = game_get_flag(player->sett->index);

#if 1
	/* Draw viewport of flag */
	frame_t flag_frame;
	sdl_frame_init(&flag_frame, 8, 24, 128, 64, player->popup_frame);

	viewport_t flag_view = {
		.x = 8, .y = 24,
		.width = 128, .height = 64,
		.layers = VIEWPORT_LAYER_PATHS | VIEWPORT_LAYER_OBJECTS
	};

	viewport_move_to_map_pos(&flag_view, MAP_COORD_ARGS(flag->pos));
	flag_view.offset_y -= 10;

	viewport_draw(&flag_view, &flag_frame);
#else
	/* Static flag */
	draw_popup_building(8, 40, 0x80 + 4*player->sett->player_num, player->popup_frame);
#endif

	for (int i = 0; i < 6; i++) {
		int x = layout[2*i];
		int y = layout[2*i+1];
		if (BIT_TEST(flag->path_con, 5-i)) {
			int sprite = 0xdc; /* Minus box */
			if (BIT_TEST(flag->transporter, 5-i)) sprite = 0x120; /* Check box */
			draw_popup_icon(x, y, sprite, player->popup_frame);
		}
	}

	draw_green_string(0, 4, player->popup_frame, "Transport Info:");
	draw_popup_icon(2, 96, 0x1c, player->popup_frame); /* Geologist */
	draw_popup_icon(14, 128, 0x3c, player->popup_frame); /* Exit box */

	/* Draw list of resources */
	for (int i = 0; i < 8; i++) {
		if (flag->res_waiting[i] != 0) {
			draw_popup_icon(7 + 2*(i&3), 88 + 16*(i>>2), 0x22 + (flag->res_waiting[i] & 0x1f)-1, player->popup_frame);
		}
	}

	draw_green_string(0, 128, player->popup_frame, "Index:");
	draw_green_number(7, 128, player->popup_frame, FLAG_INDEX(flag));
}

static void
draw_castle_serf_box(player_t *player)
{
	const int layout[] = {
		0x3d, 12, 128, /* flip */
		0x3c, 14, 128, /* exit */
		-1
	};

	int serfs[27];
	memset(serfs, 0, 27*sizeof(int));

	draw_box_background(0x138, player->popup_frame);
	draw_custom_icon_box(layout, player->popup_frame);

	if (player->sett->index == 0) return;/*close_box();*/

	building_t *building = game_get_building(player->sett->index);
	if (BIT_TEST(building->serf, 5)) return;/*close_box();*/ /* Building is burning */

	building_type_t type = BUILDING_TYPE(building);
	if (type != BUILDING_STOCK && type != BUILDING_CASTLE) return;/*close_box();*/

	inventory_t *inventory = building->u.inventory;

	for (int i = 1; i < globals.max_ever_serf_index; i++) {
		if (BIT_TEST(globals.serfs_bitmap[i/8], 7-(i&7))) {
			serf_t *serf = game_get_serf(i);
			if (serf->state == SERF_STATE_IDLE_IN_STOCK &&
			    inventory == game_get_inventory(serf->s.idle_in_stock.inv_index)) {
				serfs[SERF_TYPE(serf)] += 1;
			}
		}
	}

	draw_serfs_box(player, serfs, -1);
}

static void
draw_resdir_box(player_t *player)
{
	const int layout[] = {
		0x128, 4, 16,
		0x129, 4, 80,
		0xdc, 9, 16,
		0xdc, 9, 32,
		0xdc, 9, 48,
		0xdc, 9, 80,
		0xdc, 9, 96,
		0xdc, 9, 112,
		0x3d, 12, 128,
		0x3c, 14, 128,
		-1
	};

	const int knights_layout[] = {
		0x21, 12, 16,
		0x20, 12, 36,
		0x1f, 12, 56,
		0x1e, 12, 76,
		0x1d, 12, 96,
		-1
	};

	draw_box_background(0x138, player->popup_frame);
	draw_custom_icon_box(layout, player->popup_frame);

	if (player->sett->index == 0) return;/*close_box();*/

	building_t *building = game_get_building(player->sett->index);
	if (BIT_TEST(building->serf, 5)) return;/*close_box();*/ /* Building is burning */

	building_type_t type = BUILDING_TYPE(building);
	if (type == BUILDING_CASTLE) {
		int knights[] = { 0, 0, 0, 0, 0 };
		draw_custom_icon_box(knights_layout, player->popup_frame);

		/* Follow linked list of knights on duty */
		for (int index = building->serf_index; index != 0; index = game_get_serf(index)->s.defending.next_knight) {
			serf_t *serf = game_get_serf(index);
			serf_type_t serf_type = SERF_TYPE(serf);
			if (serf_type >= SERF_KNIGHT_0 && serf_type <= SERF_KNIGHT_4) {
				knights[serf_type-SERF_KNIGHT_0] += 1;
			} else {
				break;
			}
		}

		draw_green_number(14, 20, player->popup_frame, knights[4]);
		draw_green_number(14, 40, player->popup_frame, knights[3]);
		draw_green_number(14, 60, player->popup_frame, knights[2]);
		draw_green_number(14, 80, player->popup_frame, knights[1]);
		draw_green_number(14, 100, player->popup_frame, knights[0]);
	} else if (type != BUILDING_STOCK) {
		return;/*close_box();*/
	}

	/* Draw resource mode checkbox */
	inventory_t *inventory = building->u.inventory;
	int res_mode = inventory->res_dir & 3;
	if (res_mode == 0) draw_popup_icon(9, 16, 0x120, player->popup_frame); /* in */
	else if (res_mode == 1) draw_popup_icon(9, 32, 0x120, player->popup_frame); /* stop */
	else draw_popup_icon(9, 48, 0x120, player->popup_frame); /* out */

	/* Draw serf mode checkbox */
	int serf_mode = (inventory->res_dir >> 2) & 3;
	if (serf_mode == 0) draw_popup_icon(9, 80, 0x120, player->popup_frame); /* in */
	else if (serf_mode == 1) draw_popup_icon(9, 96, 0x120, player->popup_frame); /* stop */
	else draw_popup_icon(9, 112, 0x120, player->popup_frame); /* out */
}

static void
draw_sett_8_box(player_t *player)
{
	const int layout[] = {
		9, 2, 8,
		29, 12, 8,
		300, 2, 28,
		59, 7, 44,
		130, 8, 28,
		58, 9, 44,
		304, 3, 64,
		303, 11, 64,
		302, 2, 84,
		220, 6, 84,
		220, 6, 100,
		301, 10, 84,
		220, 3, 120,
		221, 9, 120,
		60, 14, 128,
		-1
	};

	draw_box_background(311, player->popup_frame);
	draw_custom_icon_box(layout, player->popup_frame);

	player_sett_t *sett = player->sett;

	draw_slide_bar(4, 12, sett->serf_to_knight_rate, player->popup_frame);
	draw_green_number(6, 63, player->popup_frame, (100*sett->knight_morale)/0x1000);

	draw_green_large_number(6, 73, player->popup_frame, sett->gold_deposited);

	draw_green_number(6, 119, player->popup_frame, sett->castle_knights_wanted);
	draw_green_number(6, 129, player->popup_frame, sett->castle_knights);

	if (!BIT_TEST(sett->flags, 1)) { /* Send weakest */
		draw_popup_icon(6, 84, 288, player->popup_frame); /* checkbox */
	} else {
		draw_popup_icon(6, 100, 288, player->popup_frame); /* checkbox */
	}

	int convertible_to_knights = 0;
	for (int i = 0; i < globals.max_ever_inventory_index; i++) {
		if (BIT_TEST(globals.inventories_bitmap[i>>3], 7-(i&7))) {
			inventory_t *inv = game_get_inventory(i);
			if (inv->player_num == sett->player_num) {
				int c = min(inv->resources[RESOURCE_SWORD],
					    inv->resources[RESOURCE_SHIELD]);
				convertible_to_knights += min(c, inv->spawn_priority);
			}
		}
	}

	draw_green_number(12, 40, player->popup_frame, convertible_to_knights);
}

static void
draw_sett_6_box(player_t *player)
{
	const int layout[] = {
		237, 1, 120,
		238, 3, 120,
		239, 9, 120,
		240, 11, 120,

		295, 1, 4, /* default */
		60, 14, 128, /* exit */
		-1
	};

	draw_box_background(311, player->popup_frame);
	draw_custom_icon_box(layout, player->popup_frame);
	draw_popup_resource_stairs(player->sett->inventory_prio, player->popup_frame);

	draw_popup_icon(6, 120, 33+player->sett->current_sett_6_item, player->popup_frame);
}

static void
draw_bld_1_box(player_t *player)
{
	const int layout[] = {
		0xc0, 0, 5, /* stock */
		0xab, 2, 77, /* hut */
		0x9e, 8, 7, /* tower */
		0x98, 6, 69, /* fortress */
		-1
	};

	draw_box_background(313, player->popup_frame);

	draw_popup_building(4, 112, 0x80 + 4*player->sett->player_num, player->popup_frame);
	draw_custom_bld_box(layout, player->popup_frame);

	draw_popup_icon(0, 128, 0x3d, player->popup_frame); /* flipbox */
	draw_popup_icon(14, 128, 0x3c, player->popup_frame); /* exit */
}

static void
draw_bld_2_box(player_t *player)
{
	draw_box_background(313, player->popup_frame);

	/* TODO */

	draw_popup_icon(0, 128, 0x3d, player->popup_frame); /* flipbox */
	draw_popup_icon(14, 128, 0x3c, player->popup_frame); /* exit */
}

static void
draw_bld_3_box(player_t *player)
{
	draw_box_background(313, player->popup_frame);

	/* TODO */

	draw_popup_icon(0, 128, 0x3d, player->popup_frame); /* flipbox */
	draw_popup_icon(14, 128, 0x3c, player->popup_frame); /* exit */
}

static void
draw_bld_4_box(player_t *player)
{
	draw_box_background(313, player->popup_frame);

	/* TODO */

	draw_popup_icon(0, 128, 0x3d, player->popup_frame); /* flipbox */
	draw_popup_icon(14, 128, 0x3c, player->popup_frame); /* exit */
}

/* Draw .. message popup box. */
static void
draw_under_attack_message_box(player_t *player, int opponent)
{
	draw_green_string(0, 10, player->popup_frame, "YOUR SETTLEMENT");
	draw_green_string(0, 20, player->popup_frame, "IS UNDER ATTACK");
	draw_player_face(6, 40, opponent, player->popup_frame);
}

static void
draw_lost_fight_message_box(player_t *player, int opponent)
{
	draw_green_string(0, 10, player->popup_frame, "  YOUR KNIGHTS");
	draw_green_string(0, 20, player->popup_frame, " JUST LOST THE");
	draw_green_string(0, 30, player->popup_frame, "     FIGHT");
	draw_player_face(6, 50, opponent, player->popup_frame);
}

static void
draw_victory_fight_message_box(player_t *player, int opponent)
{
	draw_green_string(0, 10, player->popup_frame, "   YOU GAINED");
	draw_green_string(0, 20, player->popup_frame, " A VICTORY HERE");
	draw_player_face(6, 40, opponent, player->popup_frame);
}

static void
draw_mine_empty_message_box(player_t *player, int mine)
{
	draw_green_string(0, 10, player->popup_frame, "THIS MINE HAULS");
	draw_green_string(0, 20, player->popup_frame, "  NO MORE RAW");
	draw_green_string(0, 30, player->popup_frame, "   MATERIALS");
	draw_popup_building(6, 50, map_building_sprite[BUILDING_STONEMINE] + mine,
			    player->popup_frame);
}

static void
draw_call_to_location_message_box(player_t *player, int param)
{
	draw_green_string(0, 10, player->popup_frame, " YOU WANTED ME");
	draw_green_string(0, 20, player->popup_frame, " TO CALL YOU TO");
	draw_green_string(0, 30, player->popup_frame, " THIS LOCATION");
	draw_popup_building(6, 50, 0x90, player->popup_frame);
}

static void
draw_knight_occupied_message_box(player_t *player, int building)
{
	draw_green_string(0, 10, player->popup_frame, "  A KNIGHT HAS");
	draw_green_string(0, 20, player->popup_frame, "  OCCUPIED THIS");
	draw_green_string(0, 30, player->popup_frame, "  NEW BUILDING");

	switch (building) {
	case 0:
		draw_popup_building(6, 50,
				    map_building_sprite[BUILDING_HUT],
				    player->popup_frame);
		break;
	case 1:
		draw_popup_building(6, 50,
				    map_building_sprite[BUILDING_TOWER],
				    player->popup_frame);
		break;
	case 2:
		draw_popup_building(4, 50,
				    map_building_sprite[BUILDING_FORTRESS],
				    player->popup_frame);
		break;
	default:
		NOT_REACHED();
		break;
	}
}

static void
draw_new_stock_message_box(player_t *player, int param)
{
	draw_green_string(0, 10, player->popup_frame, "A NEW STOCK HAS");
	draw_green_string(0, 20, player->popup_frame, "  BEEN BUILT");
	draw_popup_building(4, 40, map_building_sprite[BUILDING_STOCK], player->popup_frame);
}

static void
draw_lost_land_message_box(player_t *player, int opponent)
{
	draw_green_string(0, 10, player->popup_frame, "BECAUSE OF THIS");
	draw_green_string(0, 20, player->popup_frame, " ENEMY BUILDING");
	draw_green_string(0, 30, player->popup_frame, "    YOU LOST");
	draw_green_string(0, 40, player->popup_frame, "   SOME LAND");
	draw_player_face(6, 65, opponent, player->popup_frame);
}

static void
draw_lost_buildings_message_box(player_t *player, int opponent)
{
	draw_green_string(0, 10, player->popup_frame, "BECAUSE OF THIS");
	draw_green_string(0, 20, player->popup_frame, " ENEMY BUILDING");
	draw_green_string(0, 30, player->popup_frame, "    YOU LOST");
	draw_green_string(0, 40, player->popup_frame, "   SOME LAND");
	draw_green_string(0, 50, player->popup_frame, " AND BUILDINGS");
	draw_player_face(6, 75, opponent, player->popup_frame);
}

static void
draw_emergency_active_message_box(player_t *player, int param)
{
	draw_green_string(0, 10, player->popup_frame, "   EMERGENCY");
	draw_green_string(0, 20, player->popup_frame, "    PROGRAM");
	draw_green_string(0, 30, player->popup_frame, "   ACTIVATED");
	draw_popup_building(4, 60, map_building_sprite[BUILDING_CASTLE] + 1, player->popup_frame);
}

static void
draw_emergency_neutral_message_box(player_t *player, int param)
{
	draw_green_string(0, 10, player->popup_frame, "   EMERGENCY");
	draw_green_string(0, 20, player->popup_frame, "    PROGRAM");
	draw_green_string(0, 30, player->popup_frame, "  NEUTRALIZED");
	draw_popup_building(4, 60, map_building_sprite[BUILDING_CASTLE], player->popup_frame);
}

static void
draw_found_gold_message_box(player_t *player, int param)
{
	draw_green_string(0, 10, player->popup_frame, "   A GEOLOGIST");
	draw_green_string(0, 20, player->popup_frame, " HAS FOUND GOLD");
	draw_popup_icon(7, 40, 0x2f, player->popup_frame);
}

static void
draw_found_iron_message_box(player_t *player, int param)
{
	draw_green_string(0, 10, player->popup_frame, "   A GEOLOGIST");
	draw_green_string(0, 20, player->popup_frame, " HAS FOUND IRON");
	draw_popup_icon(7, 40, 0x2c, player->popup_frame);
}

static void
draw_found_coal_message_box(player_t *player, int param)
{
	draw_green_string(0, 10, player->popup_frame, "   A GEOLOGIST");
	draw_green_string(0, 20, player->popup_frame, " HAS FOUND COAL");
	draw_popup_icon(7, 40, 0x2e, player->popup_frame);
}

static void
draw_found_stone_message_box(player_t *player, int param)
{
	draw_green_string(0, 10, player->popup_frame, "   A GEOLOGIST");
	draw_green_string(0, 20, player->popup_frame, " HAS FOUND STONE");
	draw_popup_icon(7, 40, 0x2b, player->popup_frame);
}

static void
draw_call_to_menu_message_box(player_t *player, int menu)
{
	const int map_menu_sprite[] = {
		0xe6, 0xe7, 0xe8, 0xe9,
		0xea, 0xeb, 0x12a, 0x12b
	};

	draw_green_string(0, 10, player->popup_frame, " YOU WANTED ME");
	draw_green_string(0, 20, player->popup_frame, " TO CALL YOU TO");
	draw_green_string(0, 30, player->popup_frame, "   THIS MENU");
	draw_popup_icon(6, 50, map_menu_sprite[menu], player->popup_frame);
}

static void
draw_30m_since_save_message_box(player_t *player, int param)
{
	draw_green_string(0, 20, player->popup_frame, " 30 MIN. PASSED");
	draw_green_string(0, 30, player->popup_frame, " SINCE THE LAST");
	draw_green_string(0, 40, player->popup_frame, "     SAVING");
	draw_popup_icon(7, 60, 0x5d, player->popup_frame);
}

static void
draw_1h_since_save_message_box(player_t *player, int param)
{
	draw_green_string(0, 20, player->popup_frame, "ONE HOUR PASSED");
	draw_green_string(0, 30, player->popup_frame, " SINCE THE LAST");
	draw_green_string(0, 40, player->popup_frame, "     SAVING");
	draw_popup_icon(7, 60, 0x5d, player->popup_frame);
}

static void
draw_call_to_stock_message_box(player_t *player, int param)
{
	draw_green_string(0, 10, player->popup_frame, " YOU WANTED ME");
	draw_green_string(0, 20, player->popup_frame, " TO CALL YOU TO");
	draw_green_string(0, 30, player->popup_frame, "   THIS STOCK");
	draw_popup_building(4, 50, map_building_sprite[BUILDING_STOCK], player->popup_frame);
}

/* Dispatch to one of the message box functions above.
   Note: message box is a specific type of popup box. */
static void
draw_message_box(player_t *player)
{
	draw_box_background(0x13a, player->popup_frame);
	draw_popup_icon(14, 128, 0x120, player->popup_frame); /* Checkbox */

	int type = player->message_box & 0x1f;
	int param = (player->message_box >> 5) & 7;
	switch (type) {
	case 1:
		draw_under_attack_message_box(player, param);
		break;
	case 2:
		draw_lost_fight_message_box(player, param);
		break;
	case 3:
		draw_victory_fight_message_box(player, param);
		break;
	case 4:
		draw_mine_empty_message_box(player, param);
		break;
	case 5:
		draw_call_to_location_message_box(player, param);
		break;
	case 6:
		draw_knight_occupied_message_box(player, param);
		break;
	case 7:
		draw_new_stock_message_box(player, param);
		break;
	case 8:
		draw_lost_land_message_box(player, param);
		break;
	case 9:
		draw_lost_buildings_message_box(player, param);
		break;
	case 10:
		draw_emergency_active_message_box(player, param);
		break;
	case 11:
		draw_emergency_neutral_message_box(player, param);
		break;
	case 12:
		draw_found_gold_message_box(player, param);
		break;
	case 13:
		draw_found_iron_message_box(player, param);
		break;
	case 14:
		draw_found_coal_message_box(player, param);
		break;
	case 15:
		draw_found_stone_message_box(player, param);
		break;
	case 16:
		draw_call_to_menu_message_box(player, param);
		break;
	case 17:
		draw_30m_since_save_message_box(player, param);
		break;
	case 18:
		draw_1h_since_save_message_box(player, param);
		break;
	case 19:
		draw_call_to_stock_message_box(player, param);
		break;
	default:
		NOT_REACHED();
		break;
	}
}

static void
draw_building_stock_box(player_t *player)
{
	draw_box_background(0x138, player->popup_frame);

	if (player->sett->index == 0) return;/*close_box();*/

	building_t *building = game_get_building(player->sett->index);
	if (BIT_TEST(building->serf, 5)) return;/*close_box();*/ /* Building is burning */

	int sprite1 = -1;
	int sprite2 = -1;
	building_type_t type = BUILDING_TYPE(building);

	switch (type) {
	case BUILDING_BOATBUILDER:
		sprite1 = 34 + RESOURCE_PLANK;
		break;
	case BUILDING_BUTCHER:
		sprite1 = 34 + RESOURCE_PIG;
		break;
	case BUILDING_PIGFARM:
	case BUILDING_MILL:
		sprite1 = 34 + RESOURCE_WHEAT;
		break;
	case BUILDING_BAKER:
		sprite1 = 34 + RESOURCE_FLOUR;
		break;
	case BUILDING_SAWMILL:
		sprite2 = 34 + RESOURCE_LUMBER;
		break;
	case BUILDING_STEELSMELTER:
		sprite1 = 34 + RESOURCE_COAL;
		sprite2 = 34 + RESOURCE_IRONORE;
		break;
	case BUILDING_TOOLMAKER:
		sprite1 = 34 + RESOURCE_PLANK;
		sprite2 = 34 + RESOURCE_STEEL;
		break;
	case BUILDING_WEAPONSMITH:
		sprite1 = 34 + RESOURCE_COAL;
		sprite2 = 34 + RESOURCE_STEEL;
		break;
	case BUILDING_GOLDSMELTER:
		sprite1 = 34 + RESOURCE_COAL;
		sprite2 = 34 + RESOURCE_GOLDORE;
		break;
	case BUILDING_FISHER:
	case BUILDING_LUMBERJACK:
	case BUILDING_STONECUTTER:
	case BUILDING_FORESTER:
	case BUILDING_FARM:
		break;
	default:
		NOT_REACHED();
	}

	/* Draw list of primary resource */
	if (sprite1 >= 0) {
		int stock1 = (building->stock1 >> 4) & 0xf;
		if (stock1 > 0) {
			for (int i = 0; i < stock1; i++) {
				draw_popup_icon(8-stock1+2*i, 110, sprite1, player->popup_frame);
			}
		} else {
			draw_popup_icon(7, 110, 0xdc, player->popup_frame); /* minus box */
		}
	}

	/* Draw list of secondary resource */
	if (sprite2 >= 0) {
		int stock2 = (building->stock2 >> 4) & 0xf;
		if (stock2 > 0) {
			for (int i = 0; i < stock2; i++) {
				draw_popup_icon(8-stock2+2*i, 90, sprite2, player->popup_frame);
			}
		} else {
			draw_popup_icon(7, 90, 0xdc, player->popup_frame); /* minus box */
		}
	}

	const int map_building_serf_sprite[] = {
		-1, 0x13, 0xd, 0x19,
		0xf, -1, -1, -1,
		-1, 0x10, -1, -1,
		0x16, 0x15, 0x14, 0x17,
		0x18, 0xe, 0x12, 0x1a,
		0x1b, -1, -1, 0x12,
		-1
	};

	/* Draw picture of serf present */
	int serf_sprite = 0xdc; /* minus box */
	if (BIT_TEST(building->serf, 6)) serf_sprite = map_building_serf_sprite[type];

	draw_popup_icon(1, 36, serf_sprite, player->popup_frame);

	/* Draw building */
	int bld_sprite = map_building_sprite[type];
	int x = 6;
	if (bld_sprite == 0xc0 /*stock*/ || bld_sprite >= 0x9e /*tower*/) x = 4;
	draw_popup_building(x, 30, bld_sprite, player->popup_frame);

	draw_green_string(1, 4, player->popup_frame, "STOCK OF");
	draw_green_string(1, 14, player->popup_frame, "THIS BUILDING:");

	draw_popup_icon(14, 128, 0x3c, player->popup_frame); /* exit box */
}

static void
draw_player_faces_box(player_t *player)
{
	draw_box_background(129, player->popup_frame);

	draw_player_face(2, 4, 0, player->popup_frame);
	draw_player_face(10, 4, 1, player->popup_frame);
	draw_player_face(2, 76, 2, player->popup_frame);
	draw_player_face(10, 76, 3, player->popup_frame);
}

static void
draw_demolish_box(player_t *player)
{
	draw_box_background(314, player->popup_frame);

	draw_popup_icon(14, 128, 60, player->popup_frame); /* Exit */
	draw_popup_icon(7, 45, 288, player->popup_frame); /* Checkbox */

	draw_green_string(0, 10, player->popup_frame, "    Demolish:");
	draw_green_string(0, 30, player->popup_frame, "   Click here");
	draw_green_string(0, 68, player->popup_frame, "   if you are");
	draw_green_string(0, 86, player->popup_frame, "      sure");
}

/* Dispatch to one of the popup box functions above. */
static void
draw_popup_box(player_t *player)
{
	if (BIT_TEST(player->flags, 0)) return; /* Player inactive */

	box_t box = player->box;
	if (box == 0) {
		if (player->clkmap == 0) return;
		box = player->clkmap;

#if 0
		/* Certain boxes need to be redrawn periodically */
		if (box < 32 && !BIT_TEST(0x20ffe04, box)) return;
		else if (!BIT_TEST(0x103ec0, box-32)) return;
#endif

		if (box == BOX_25) {
			/*if (!BIT_TEST(globals.string_bg, 1)) return;*/
			/*globals.string_bg &= ~BIT(1);*/
		} else if (globals.anim - player->last_anim < 100/*1000*/) return;
	}

	player->last_anim = globals.anim;
	player->clkmap = box;
	player->box = 0;

	switch (box) {
	case BOX_MAP:
		draw_map_box(player);
		break;
	case BOX_MAP_OVERLAY:
		draw_map_overlay_box(player);
		break;
	case BOX_MINE_BUILDING:
		draw_mine_building_box(player);
		break;
	case BOX_BASIC_BLD:
		draw_basic_building_box(player, 0);
		break;
	case BOX_BASIC_BLD_FLIP:
		draw_basic_building_box(player, 1);
		break;
	case BOX_ADV_1_BLD:
		draw_adv_1_building_box(player);
		break;
	case BOX_ADV_2_BLD:
		draw_adv_2_building_box(player);
		break;
	case BOX_STAT_SELECT:
		draw_stat_select_box(player);
		break;
	case BOX_STAT_4:
		draw_stat_4_box(player);
		break;
	case BOX_STAT_BLD_1:
		draw_stat_bld_1_box(player);
		break;
	case BOX_STAT_BLD_2:
		draw_stat_bld_2_box(player);
		break;
	case BOX_STAT_BLD_3:
		draw_stat_bld_3_box(player);
		break;
	case BOX_STAT_BLD_4:
		draw_stat_bld_4_box(player);
		break;
	case BOX_STAT_8:
		draw_stat_8_box(player);
		break;
	case BOX_STAT_7:
		draw_stat_7_box(player);
		break;
	case BOX_STAT_1:
		draw_stat_1_box(player);
		break;
	case BOX_STAT_2:
		draw_stat_2_box(player);
		break;
	case BOX_STAT_6:
		draw_stat_6_box(player);
		break;
	case BOX_STAT_3:
		draw_stat_3_box(player);
		break;
	case BOX_START_ATTACK:
		draw_start_attack_box(player);
		break;
	case BOX_START_ATTACK_REDRAW:
		draw_start_attack_redraw_box(player);
		break;
	case BOX_GROUND_ANALYSIS:
		draw_ground_analysis_box(player);
		break;
		/* TODO */
	case BOX_SETT_SELECT:
	case BOX_SETT_SELECT_FILE:
		draw_sett_select_box(player);
		break;
	case BOX_SETT_1:
		draw_sett_1_box(player);
		break;
	case BOX_SETT_2:
		draw_sett_2_box(player);
		break;
	case BOX_SETT_3:
		draw_sett_3_box(player);
		break;
	case BOX_KNIGHT_LEVEL:
		draw_knight_level_box(player);
		break;
	case BOX_SETT_4:
		draw_sett_4_box(player);
		break;
	case BOX_SETT_5:
		draw_sett_5_box(player);
		break;
	case BOX_QUIT_CONFIRM:
		draw_quit_confirm_box(player);
		break;
	case BOX_NO_SAVE_QUIT_CONFIRM:
		draw_no_save_quit_confirm_box(player);
		break;
	case BOX_OPTIONS:
		draw_options_box(player);
		break;
	case BOX_CASTLE_RES:
		draw_castle_res_box(player);
		break;
	case BOX_MINE_OUTPUT:
		draw_mine_output_box(player);
		break;
	case BOX_ORDERED_BLD:
		draw_ordered_building_box(player);
		break;
	case BOX_DEFENDERS:
		draw_defenders_box(player);
		break;
	case BOX_TRANSPORT_INFO:
		draw_transport_info_box(player);
		break;
	case BOX_CASTLE_SERF:
		draw_castle_serf_box(player);
		break;
	case BOX_RESDIR:
		draw_resdir_box(player);
		break;
	case BOX_SETT_8:
		draw_sett_8_box(player);
		break;
	case BOX_SETT_6:
		draw_sett_6_box(player);
		break;
	case BOX_BLD_1:
		draw_bld_1_box(player);
		break;
	case BOX_BLD_2:
		draw_bld_2_box(player);
		break;
	case BOX_BLD_3:
		draw_bld_3_box(player);
		break;
	case BOX_BLD_4:
		draw_bld_4_box(player);
		break;
	case BOX_MESSAGE:
		draw_message_box(player);
		break;
	case BOX_BLD_STOCK:
		draw_building_stock_box(player);
		break;
	case BOX_PLAYER_FACES:
		draw_player_faces_box(player);
		break;
	case BOX_DEMOLISH:
		draw_demolish_box(player);
		break;
	default:
		break;
	}
}

void
gui_draw_player_popups()
{
	draw_popup_box_frame(globals.player[0]->popup_frame);
	draw_popup_box(globals.player[0]);

	if (BIT_TEST(globals.split, 6)) { /* coop mode */
		/* TODO ... */
	} else {
		draw_popup_box(globals.player[1]);
	}
}
