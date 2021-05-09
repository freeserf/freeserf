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
NotificationBox::draw_icon(int ix, int iy, int sprite) {
  frame->draw_sprite(8 * ix, iy, Data::AssetIcon, sprite);
}

void
NotificationBox::draw_background(int bwidth, int bheight, int sprite) {
  for (int sy = 0; sy < bheight; sy += 16) {
    for (int sx = 0; sx < bwidth; sx += 16) {
      frame->draw_sprite(sx, sy, Data::AssetIcon, sprite);
    }
  }
}

void
NotificationBox::draw_string(int sx, int sy, const std::string &str) {
  std::stringstream sin;
  sin << str;
  std::string line;
  int cy = sy;
  while (std::getline(sin, line)) {
    frame->draw_string(sx*8, cy, line, Color::green);
    cy += 10;
  }
}

void
NotificationBox::draw_map_object(int ox, int oy, int sprite) {
  frame->draw_sprite(8 * ox, oy, Data::AssetMapObject, sprite);
}

void
NotificationBox::draw_player_face(int fx, int fy, int player) {
  Color color = interface->get_player_color(player);
  size_t face = interface->get_player_face(player);
  frame->fill_rect(8 * fx, fy, 48, 72, color);
  draw_icon(fx + 1, fy + 4, face);
}

#define NOTIFICATION_SHOW_OPPONENT 0
#define NOTIFICATION_SHOW_MINE 1
#define NOTIFICATION_SHOW_BUILDING 2
#define NOTIFICATION_SHOW_MAP_OBJECT 3
#define NOTIFICATION_SHOW_ICON 4
#define NOTIFICATION_SHOW_MENU 5

NotificationBox::NotificationView notification_views[] = {
  { Message::TypeUnderAttack,
      NOTIFICATION_SHOW_OPPONENT,
      0,
      "Your settlement\nis under attack" },
  { Message::TypeLoseFight,
      NOTIFICATION_SHOW_OPPONENT,
      0,
      "Your knights\njust lost the\nfight" },
  { Message::TypeWinFight,
      NOTIFICATION_SHOW_OPPONENT,
      0,
      "You gained\na victory here" },
  { Message::TypeMineEmpty,
      NOTIFICATION_SHOW_MINE,
      0,
      "This mine hauls\nno more raw\nmaterials" },
  { Message::TypeCallToLocation,
      NOTIFICATION_SHOW_MAP_OBJECT,
      0x90,
      "You wanted me\nto call you to\nthis location" },
  { Message::TypeKnightOccupied,
      NOTIFICATION_SHOW_BUILDING,
      0,
      "A knight has\noccupied this\nnew building" },
  { Message::TypeNewStock,
      NOTIFICATION_SHOW_MAP_OBJECT,
      map_building_sprite[Building::TypeStock],
      "A new stock\nhas been built" },
  { Message::TypeLostLand,
      NOTIFICATION_SHOW_OPPONENT,
      0,
      "Because of this\nenemy building\nyou lost some\nland" },
  { Message::TypeLostBuildings,
      NOTIFICATION_SHOW_OPPONENT,
      0,
      "Because of this\nenemy building\nyou lost some\n"
      "land and\nsome buildings" },
  { Message::TypeEmergencyActive,
      NOTIFICATION_SHOW_MAP_OBJECT,
      map_building_sprite[Building::TypeCastle] + 1,
      "Emergency\nprogram\nactivated" },
  { Message::TypeEmergencyNeutral,
      NOTIFICATION_SHOW_MAP_OBJECT,
      map_building_sprite[Building::TypeCastle],
      "Emergency\nprogram\nneutralized" },
  { Message::TypeFoundGold,
      NOTIFICATION_SHOW_ICON,
      0x2f,
      "A geologist\nhas found gold" },
  { Message::TypeFoundIron,
      NOTIFICATION_SHOW_ICON,
      0x2c,
      "A geologist\nhas found iron" },
  { Message::TypeFoundCoal,
      NOTIFICATION_SHOW_ICON,
      0x2e,
      "A geologist\nhas found coal" },
  { Message::TypeFoundStone,
      NOTIFICATION_SHOW_ICON,
      0x2b,
      "A geologist\nhas found stone" },
  { Message::TypeCallToMenu,
      NOTIFICATION_SHOW_MENU,
      0,
      "You wanted me\nto call you\nto this menu" },
  { Message::Type30MSinceSave,
      NOTIFICATION_SHOW_ICON,
      0x5d,
      "30 min. passed\nsince the last\nsaving" },
  { Message::Type1HSinceSave,
      NOTIFICATION_SHOW_ICON,
      0x5d,
      "One hour passed\nsince the last\nsaving" },
  { Message::TypeCallToStock,
      NOTIFICATION_SHOW_MAP_OBJECT,
      map_building_sprite[Building::TypeStock],
      "You wanted me\nto call you\nto this stock" },
  { Message::TypeNone, 0, 0, NULL }
};

/* Messages boxes */
void
NotificationBox::draw_notification(NotificationView *view) {
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
      draw_map_object(18, 8, map_building_sprite[Building::TypeStoneMine] +
                             message.data);
      break;
    case NOTIFICATION_SHOW_BUILDING:
      switch (message.data) {
        case 0:
          draw_map_object(18, 8, map_building_sprite[Building::TypeHut]);
          break;
        case 1:
          draw_map_object(18, 8, map_building_sprite[Building::TypeTower]);
          break;
        case 2:
          draw_map_object(16, 8, map_building_sprite[Building::TypeFortress]);
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
NotificationBox::internal_draw() {
  draw_background(width, height, 0x13a);
  draw_icon(14, 128, 0x120); /* Checkbox */

  for (int i = 0; notification_views[i].type != Message::TypeNone; i++) {
    if (notification_views[i].type == message.type) {
      draw_notification(&notification_views[i]);
    }
  }
}

bool
NotificationBox::handle_click_left(int x, int y) {
  set_displayed(0);
  return true;
}

NotificationBox::NotificationBox(Interface *_interface)
  : interface(_interface) {
}

void
NotificationBox::show(const Message &_message) {
  message = _message;
  set_displayed(1);
}
