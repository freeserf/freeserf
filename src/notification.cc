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

#include <sstream>
#include <string>

#include "src/interface.h"
#include "src/game.h"
#include "src/debug.h"
#include "src/data.h"

void
notification_box_t::draw_icon(int x, int y, int sprite) {
  frame->draw_sprite(8*x, y, DATA_ICON_BASE + sprite);
}

void
notification_box_t::draw_background(int width, int height, int sprite) {
  for (int y = 0; y < height; y += 16) {
    for (int x = 0; x < width; x += 16) {
      frame->draw_sprite(x, y, DATA_ICON_BASE + sprite);
    }
  }
}

void
notification_box_t::draw_string(int x, int y, const std::string &str) {
  std::stringstream sin;
  sin << str;
  std::string line;
  int cy = y;
  while (std::getline(sin, line)) {
    frame->draw_string(x*8, cy, 31, 0, line);
    cy += 10;
  }
}

void
notification_box_t::draw_map_object(int x, int y, int sprite) {
  frame->draw_transp_sprite(8*x, y, DATA_MAP_OBJECT_BASE + sprite, false);
}

unsigned int
notification_box_t::get_player_face_sprite(size_t face) {
  if (face != 0) {
    return static_cast<unsigned int>(0x10b + face);
  }
  return 0x119; /* sprite_face_none */
}

void
notification_box_t::draw_player_face(int x, int y, int player) {
  player_t *p = interface->get_game()->get_player(player);
  frame->fill_rect(8*x, y, 48, 72, p->get_color());
  draw_icon(x+1, y+4,
    get_player_face_sprite(p->get_face()));
}

#define NOTIFICATION_SHOW_OPPONENT 0
#define NOTIFICATION_SHOW_MINE 1
#define NOTIFICATION_SHOW_BUILDING 2
#define NOTIFICATION_SHOW_MAP_OBJECT 3
#define NOTIFICATION_SHOW_ICON 4
#define NOTIFICATION_SHOW_MENU 5

enum notification {
  NOTIFICATION_CATEGORY_ALL = -1,
  NOTIFICATION_CATEGORY_0 = 0,
  NOTIFICATION_CATEGORY_1 = 1,
  NOTIFICATION_CATEGORY_2 = 2,
  NOTIFICATION_CATEGORY_3 = 3,
  NOTIFICATION_CATEGORY_4 = 4,
  NOTIFICATION_CATEGORY_5 = 5,
};


const notification_view_t notification_box_t::notification_views[] = {
  // NOTIFICATION_NONE
  { 0, 0, NULL, 0 },
  // NOTIFICATION_UNDER_ATTACK
  { NOTIFICATION_SHOW_OPPONENT,
      0,
      "Your settlement\nis under attack",
    -1},
  // NOTIFICATION_LOST_FIGHT
  { NOTIFICATION_SHOW_OPPONENT,
      0,
      "Your knights\njust lost the\nfight",
    5},
  // NOTIFICATION_VICTORY_FIGHT
  { NOTIFICATION_SHOW_OPPONENT,
      0,
      "You gained\na victory here",
    5},
  // NOTIFICATION_MINE_EMPTY    
  { NOTIFICATION_SHOW_MINE,
      0,
      "This mine hauls\nno more raw\nmaterials",
    5},
  // NOTIFICATION_CALL_TO_LOCATION
  { NOTIFICATION_SHOW_MAP_OBJECT,
      0x90,
      "You wanted me\nto call you to\nthis location",
    4},
  // NOTIFICATION_KNIGHT_OCCUPIED
  { NOTIFICATION_SHOW_BUILDING,
      0,
      "A knight has\noccupied this\nnew building",
    0},
  // NOTIFICATION_NEW_STOCK
  { NOTIFICATION_SHOW_MAP_OBJECT,
      map_building_sprite[BUILDING_STOCK],
      "A new stock\nhas been built",
    4},
  // NOTIFICATION_LOST_LAND
  { NOTIFICATION_SHOW_OPPONENT,
      0,
      "Because of this\nenemy building\nyou lost some\nland",
    3},
  // NOTIFICATION_LOST_BUILDINGS
  { NOTIFICATION_SHOW_OPPONENT,
      0,
      "Because of this\nenemy building\nyou lost some\n"
      "land and\nsome buildings",
    4},
  // NOTIFICATION_EMERGENCY_ACTIVE
  { NOTIFICATION_SHOW_MAP_OBJECT,
      map_building_sprite[BUILDING_CASTLE] + 1,
      "Emergency\nprogram\nactivated",
    5},
  // NOTIFICATION_EMERGENCY_NEUTRAL
  { NOTIFICATION_SHOW_MAP_OBJECT,
      map_building_sprite[BUILDING_CASTLE],
      "Emergency\nprogram\nneutralized",
    5},
  // NOTIFICATION_FOUND_GOLD
  { NOTIFICATION_SHOW_ICON,
      0x2f,
      "A geologist\nhas found gold",
    5},
  // NOTIFICATION_FOUND_IRON
  { NOTIFICATION_SHOW_ICON,
      0x2c,
      "A geologist\nhas found iron",
    4},
  // NOTIFICATION_FOUND_COAL
  { NOTIFICATION_SHOW_ICON,
      0x2e,
      "A geologist\nhas found coal",
    4},
  // NOTIFICATION_FOUND_STONE
  { NOTIFICATION_SHOW_ICON,
      0x2b,
      "A geologist\nhas found stone",
    4},
  // NOTIFICATION_CALL_TO_MENU
  { NOTIFICATION_SHOW_MENU,
      0,
      "You wanted me\nto call you\nto this menu",
    4},
  // NOTIFICATION_30M_SINCE_SAVE
  { NOTIFICATION_SHOW_ICON,
      0x5d,
      "30 min. passed\nsince the last\nsaving",
    0},
  // NOTIFICATION_1H_SINCE_SAVE
  { NOTIFICATION_SHOW_ICON,
      0x5d,
      "One hour passed\nsince the last\nsaving",
    0},
  // NOTIFICATION_CALL_TO_STOCK
  { NOTIFICATION_SHOW_MAP_OBJECT,
      map_building_sprite[BUILDING_STOCK],
      "You wanted me\nto call you\nto this stock",
    0},
};

/* Messages boxes */
void
notification_box_t::draw_notification(const notification_view_t *view) {
  const int map_menu_sprite[] = {
    0xe6, 0xe7, 0xe8, 0xe9,
    0xea, 0xeb, 0x12a, 0x12b
  };

  draw_string(1, 10, view->text);
  switch (view->decoration) {
    case NOTIFICATION_SHOW_OPPONENT:
      draw_player_face(18, 8, message.data);
      break;
    case NOTIFICATION_SHOW_MINE:
      draw_map_object(18, 8, map_building_sprite[BUILDING_STONEMINE] +
                             message.data);
      break;
    case NOTIFICATION_SHOW_BUILDING:
      switch (message.data) {
        case 0:
          draw_map_object(18, 8, map_building_sprite[BUILDING_HUT]);
          break;
        case 1:
          draw_map_object(18, 8, map_building_sprite[BUILDING_TOWER]);
          break;
        case 2:
          draw_map_object(16, 8, map_building_sprite[BUILDING_FORTRESS]);
          break;
        default:
          NOT_REACHED();
          break;
      }
      break;
    case NOTIFICATION_SHOW_MAP_OBJECT:
      draw_map_object(16, 8, view->icon);
      break;
    case NOTIFICATION_SHOW_ICON:
      draw_icon(20, 14, view->icon);
      break;
    case NOTIFICATION_SHOW_MENU:
      draw_icon(18, 8, map_menu_sprite[message.data]);
      break;
    default:
      break;
  }
}

void
notification_box_t::internal_draw() {
  draw_background(width, height, 0x13a);
  draw_icon(14, 128, 0x120); /* Checkbox */

  if (message.type != NOTIFICATION_NONE) {
      draw_notification(&notification_box_t::notification_views[message.type]);
  }
}

bool
notification_box_t::handle_click_left(int x, int y) {
  set_displayed(0);
  return true;
}

notification_box_t::notification_box_t(interface_t *interface) {
  this->interface = interface;
}

void notification_box_t::show(const message_t &message) {
  this->message = message;
  set_displayed(1);
}
