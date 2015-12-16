/*
 * notification.cc - Notification GUI component
 *
 * Copyright (C) 2013-2014  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/notification.h"

#include <cstdlib>

#include "src/interface.h"
#include "src/misc.h"
BEGIN_EXT_C
  #include "src/game.h"
  #include "src/data.h"
  #include "src/debug.h"
END_EXT_C

static void
draw_icon(int x, int y, int sprite, frame_t *frame) {
  gfx_draw_sprite(8*x, y, DATA_ICON_BASE + sprite, frame);
}

static void
draw_background(int width, int height, int sprite, frame_t *frame) {
  for (int y = 0; y < height; y += 16) {
    for (int x = 0; x < width; x += 16) {
      gfx_draw_sprite(x, y, DATA_ICON_BASE + sprite, frame);
    }
  }
}

static void
draw_string(int x, int y, frame_t *frame, const char *str) {
  gfx_draw_string(8*x, y, 31, 0, frame, str);
}

static void
draw_map_object(int x, int y, int sprite, frame_t *frame) {
  gfx_draw_transp_sprite(8*x, y, DATA_MAP_OBJECT_BASE + sprite,
             0, 0, 0, frame);
}

static int
get_player_face_sprite(int face) {
  if (face != 0) return 0x10b + face;
  return 0x119; /* sprite_face_none */
}

static void
draw_player_face(int x, int y, int player, frame_t *frame) {
  gfx_fill_rect(8*x, y, 48, 72, game.player[player]->color, frame);
  draw_icon(x+1, y+4, get_player_face_sprite(game.player[player]->face), frame);
}


/* Messages boxes */
static void
draw_under_attack_message_box(frame_t *frame, int opponent) {
  draw_string(1, 10, frame, "Your settlement");
  draw_string(1, 20, frame, "is under attack");
  draw_player_face(18, 8, opponent, frame);
}

static void
draw_lost_fight_message_box(frame_t *frame, int opponent) {
  draw_string(1, 10, frame, "Your knights");
  draw_string(1, 20, frame, "just lost the");
  draw_string(1, 30, frame, "fight");
  draw_player_face(18, 8, opponent, frame);
}

static void
draw_victory_fight_message_box(frame_t *frame, int opponent) {
  draw_string(1, 10, frame, "You gained");
  draw_string(1, 20, frame, "a victory here");
  draw_player_face(18, 8, opponent, frame);
}

static void
draw_mine_empty_message_box(frame_t *frame, int mine) {
  draw_string(1, 10, frame, "This mine hauls");
  draw_string(1, 20, frame, "no more raw");
  draw_string(1, 30, frame, "materials");
  draw_map_object(18, 8, map_building_sprite[BUILDING_STONEMINE] + mine,
      frame);
}

static void
draw_call_to_location_message_box(frame_t *frame, int param) {
  draw_string(1, 10, frame, "You wanted me");
  draw_string(1, 20, frame, "to call you to");
  draw_string(1, 30, frame, "this location");
  draw_map_object(20, 14, 0x90, frame);
}

static void
draw_knight_occupied_message_box(frame_t *frame, int building) {
  draw_string(1, 10, frame, "A knight has");
  draw_string(1, 20, frame, "occupied this");
  draw_string(1, 30, frame, "new building");

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
draw_new_stock_message_box(frame_t *frame, int param) {
  draw_string(1, 10, frame, "A new stock");
  draw_string(1, 20, frame, "has been built");
  draw_map_object(16, 8, map_building_sprite[BUILDING_STOCK], frame);
}

static void
draw_lost_land_message_box(frame_t *frame, int opponent) {
  draw_string(1, 10, frame, "Because of this");
  draw_string(1, 20, frame, "enemy building");
  draw_string(1, 30, frame, "you lost some");
  draw_string(1, 40, frame, "land");
  draw_player_face(18, 8, opponent, frame);
}

static void
draw_lost_buildings_message_box(frame_t *frame, int opponent) {
  draw_string(1, 10, frame, "Because of this");
  draw_string(1, 20, frame, "enemy building");
  draw_string(1, 30, frame, "you lost some");
  draw_string(1, 40, frame, "land and");
  draw_string(1, 50, frame, "some buildings");
  draw_player_face(18, 8, opponent, frame);
}

static void
draw_emergency_active_message_box(frame_t *frame, int param) {
  draw_string(1, 10, frame, "Emergency");
  draw_string(1, 20, frame, "program");
  draw_string(1, 30, frame, "activated");
  draw_map_object(18, 8, map_building_sprite[BUILDING_CASTLE] + 1, frame);
}

static void
draw_emergency_neutral_message_box(frame_t *frame, int param) {
  draw_string(1, 10, frame, "Emergency");
  draw_string(1, 20, frame, "program");
  draw_string(1, 30, frame, "neutralized");
  draw_map_object(16, 8, map_building_sprite[BUILDING_CASTLE], frame);
}

static void
draw_found_gold_message_box(frame_t *frame, int param) {
  draw_string(1, 10, frame, "A geologist");
  draw_string(1, 20, frame, "has found gold");
  draw_icon(20, 14, 0x2f, frame);
}

static void
draw_found_iron_message_box(frame_t *frame, int param) {
  draw_string(1, 10, frame, "A geologist");
  draw_string(1, 20, frame, "has found iron");
  draw_icon(20, 14, 0x2c, frame);
}

static void
draw_found_coal_message_box(frame_t *frame, int param) {
  draw_string(1, 10, frame, "A geologist");
  draw_string(1, 20, frame, "has found coal");
  draw_icon(20, 14, 0x2e, frame);
}

static void
draw_found_stone_message_box(frame_t *frame, int param) {
  draw_string(1, 10, frame, "A geologist");
  draw_string(1, 20, frame, "has found stone");
  draw_icon(20, 14, 0x2b, frame);
}

static void
draw_call_to_menu_message_box(frame_t *frame, int menu) {
  const int map_menu_sprite[] = {
    0xe6, 0xe7, 0xe8, 0xe9,
    0xea, 0xeb, 0x12a, 0x12b
  };

  draw_string(1, 10, frame, "You wanted me");
  draw_string(1, 20, frame, "to call you");
  draw_string(1, 30, frame, "to this menu");
  draw_icon(18, 8, map_menu_sprite[menu], frame);
}

static void
draw_30m_since_save_message_box(frame_t *frame, int param) {
  draw_string(1, 10, frame, "30 min. passed");
  draw_string(1, 20, frame, "since the last");
  draw_string(1, 30, frame, "saving");
  draw_icon(20, 14, 0x5d, frame);
}

static void
draw_1h_since_save_message_box(frame_t *frame, int param) {
  draw_string(1, 10, frame, "One hour passed");
  draw_string(1, 20, frame, "since the last");
  draw_string(1, 30, frame, "saving");
  draw_icon(20, 14, 0x5d, frame);
}

static void
draw_call_to_stock_message_box(frame_t *frame, int param) {
  draw_string(1, 10, frame, "You wanted me");
  draw_string(1, 20, frame, "to call you");
  draw_string(1, 30, frame, "to this stock");
  draw_map_object(16, 8, map_building_sprite[BUILDING_STOCK], frame);
}

static void
notification_box_draw(notification_box_t *box, frame_t *frame) {
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
notification_box_handle_event_click(notification_box_t *box, int x, int y) {
  gui_object_set_displayed(reinterpret_cast<gui_object_t*>(box), 0);
  return 0;
}

static int
notification_box_handle_event(notification_box_t *box,
                              const gui_event_t *event) {
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
notification_box_init(notification_box_t *box, interface_t *interface) {
  gui_object_init(reinterpret_cast<gui_object_t*>(box));
  box->obj.draw = reinterpret_cast<gui_draw_func*>(notification_box_draw);
  box->obj.handle_event =
    reinterpret_cast<gui_handle_event_func*>(notification_box_handle_event);

  box->interface = interface;
  box->type = 0;
  box->param = 0;
}
