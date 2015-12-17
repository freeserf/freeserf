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

#include "src/gui.h"

class interface_t;

class notification_box_t : public gui_object_t {
 protected:
  interface_t *interface;

  int type;
  int param;

 public:
  explicit notification_box_t(interface_t *interface);

  void show(int type, int param);

  virtual void draw(frame_t *frame);
  virtual int handle_event(const gui_event_t *event);

 protected:
  void draw_icon(int x, int y, int sprite, frame_t *frame);
  void draw_background(int width, int height, int sprite, frame_t *frame);
  void draw_string(int x, int y, frame_t *frame, const char *str);
  void draw_map_object(int x, int y, int sprite, frame_t *frame);
  int get_player_face_sprite(int face);
  void draw_player_face(int x, int y, int player, frame_t *frame);
  void draw_under_attack_message_box(frame_t *frame, int opponent);
  void draw_lost_fight_message_box(frame_t *frame, int opponent);
  void draw_victory_fight_message_box(frame_t *frame, int opponent);
  void draw_mine_empty_message_box(frame_t *frame, int mine);
  void draw_call_to_location_message_box(frame_t *frame, int param);
  void draw_knight_occupied_message_box(frame_t *frame, int building);
  void draw_new_stock_message_box(frame_t *frame, int param);
  void draw_lost_land_message_box(frame_t *frame, int opponent);
  void draw_lost_buildings_message_box(frame_t *frame, int opponent);
  void draw_emergency_active_message_box(frame_t *frame, int param);
  void draw_emergency_neutral_message_box(frame_t *frame, int param);
  void draw_found_gold_message_box(frame_t *frame, int param);
  void draw_found_iron_message_box(frame_t *frame, int param);
  void draw_found_coal_message_box(frame_t *frame, int param);
  void draw_found_stone_message_box(frame_t *frame, int param);
  void draw_call_to_menu_message_box(frame_t *frame, int menu);
  void draw_30m_since_save_message_box(frame_t *frame, int param);
  void draw_1h_since_save_message_box(frame_t *frame, int param);
  void draw_call_to_stock_message_box(frame_t *frame, int param);
  int handle_event_click(int x, int y);
};

#endif  // SRC_NOTIFICATION_H_
