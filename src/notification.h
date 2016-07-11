/*
 * notification.h - Notification GUI component
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

#ifndef SRC_NOTIFICATION_H_
#define SRC_NOTIFICATION_H_

#include <string>
#include <queue>

#include "src/gui.h"
#include "src/map.h"

typedef enum {
  NOTIFICATION_NONE = 0,
  NOTIFICATION_UNDER_ATTACK = 1,
  NOTIFICATION_LOST_FIGHT = 2,
  NOTIFICATION_VICTORY_FIGHT = 3,
  NOTIFICATION_MINE_EMPTY = 4,
  NOTIFICATION_CALL_TO_LOCATION = 5,
  NOTIFICATION_KNIGHT_OCCUPIED = 6,
  NOTIFICATION_NEW_STOCK = 7,
  NOTIFICATION_LOST_LAND = 8,
  NOTIFICATION_LOST_BUILDINGS = 9,
  NOTIFICATION_EMERGENCY_ACTIVE = 10,
  NOTIFICATION_EMERGENCY_NEUTRAL = 11,
  NOTIFICATION_FOUND_GOLD = 12,
  NOTIFICATION_FOUND_IRON = 13,
  NOTIFICATION_FOUND_COAL = 14,
  NOTIFICATION_FOUND_STONE = 15,
  NOTIFICATION_CALL_TO_MENU = 16,
  NOTIFICATION_30M_SINCE_SAVE = 17,
  NOTIFICATION_1H_SINCE_SAVE = 18,
  NOTIFICATION_CALL_TO_STOCK = 19
} notification_type_t;

typedef struct {
  unsigned int decoration;
  unsigned int icon;
  const char *text;
  int category;
} notification_view_t;


class message_t {
public:
	notification_type_t type;
	map_pos_t pos;
	unsigned int data;
};
typedef std::queue<message_t> messages_t;

class pos_timer_t {
public:
	int timeout;
	map_pos_t pos;
};
typedef std::vector<pos_timer_t> timers_t;


class interface_t;

class notification_box_t : public gui_object_t {
 protected:
  message_t message;
  interface_t *interface;

 public:
  explicit notification_box_t(interface_t *interface);

  static const notification_view_t notification_views[];

  void show(const message_t &message);

 protected:
  void draw_notification(const notification_view_t *view);

  void draw_icon(int x, int y, int sprite);
  void draw_background(int width, int height, int sprite);
  void draw_string(int x, int y, const std::string &str);
  void draw_map_object(int x, int y, int sprite);
  unsigned int get_player_face_sprite(size_t face);
  void draw_player_face(int x, int y, int player);

  virtual void internal_draw();
  virtual bool handle_click_left(int x, int y);
};

#endif  // SRC_NOTIFICATION_H_
