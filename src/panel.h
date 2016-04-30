/*
 * panel.h - Panel GUI component
 *
 * Copyright (C) 2012  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef SRC_PANEL_H_
#define SRC_PANEL_H_

#include "src/gui.h"

class interface_t;

class panel_bar_t : public gui_object_t {
 protected:
  typedef enum {
    PANEL_BTN_BUILD_INACTIVE = 0,
    PANEL_BTN_BUILD_FLAG,
    PANEL_BTN_BUILD_MINE,
    PANEL_BTN_BUILD_SMALL,
    PANEL_BTN_BUILD_LARGE,
    PANEL_BTN_BUILD_CASTLE,
    PANEL_BTN_DESTROY,
    PANEL_BTN_DESTROY_INACTIVE,
    PANEL_BTN_BUILD_ROAD,
    PANEL_BTN_MAP_INACTIVE,
    PANEL_BTN_MAP,
    PANEL_BTN_STATS_INACTIVE,
    PANEL_BTN_STATS,
    PANEL_BTN_SETT_INACTIVE,
    PANEL_BTN_SETT,
    PANEL_BTN_DESTROY_ROAD,
    PANEL_BTN_GROUND_ANALYSIS,
    PANEL_BTN_BUILD_SMALL_STARRED,
    PANEL_BTN_BUILD_LARGE_STARRED,
    PANEL_BTN_MAP_STARRED,
    PANEL_BTN_STATS_STARRED,
    PANEL_BTN_SETT_STARRED,
    PANEL_BTN_GROUND_ANALYSIS_STARRED,
    PANEL_BTN_BUILD_MINE_STARRED,
    PANEL_BTN_BUILD_ROAD_STARRED
  } panel_btn_t;

  interface_t *interface;
  int panel_btns[5];

 public:
  explicit panel_bar_t(interface_t *interface);

  void update();

 protected:
  void draw_panel_frame();
  void draw_message_notify();
  void draw_return_arrow();
  void draw_panel_buttons();
  void button_click(int button);
  panel_btn_t button_type_with_build_possibility(int build_possibility);

  virtual void internal_draw();
  virtual bool handle_click_left(int x, int y);
  virtual bool handle_key_pressed(char key, int modifier);
};

#endif  // SRC_PANEL_H_
