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

#include "src/gui.h"
#include "src/player.h"

class Interface;

class NotificationBox : public GuiObject {
 public:
  typedef struct NotificationView {
    Message::Type type;
    unsigned int decoration;
    unsigned int icon;
    const char *text;
  } NotificationView;

 protected:
  Message message;
  Interface *interface;

 public:
  explicit NotificationBox(Interface *interface);

  void show(const Message &message);

 protected:
  void draw_notification(NotificationView *view);

  void draw_icon(int x, int y, int sprite);
  void draw_background(int width, int height, int sprite);
  void draw_string(int x, int y, const std::string &str);
  void draw_map_object(int x, int y, int sprite);
  void draw_player_face(int x, int y, int player);

  virtual void internal_draw();
  virtual bool handle_click_left(int x, int y);
};

#endif  // SRC_NOTIFICATION_H_
