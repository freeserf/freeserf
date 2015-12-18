/*
 * minimap.h - Minimap GUI component
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

#ifndef SRC_MINIMAP_H_
#define SRC_MINIMAP_H_

#include "src/gui.h"
#include "src/misc.h"
BEGIN_EXT_C
  #include "src/map.h"
END_EXT_C

class interface_t;

class minimap_t : public gui_object_t {
 protected:
  interface_t *interface;

  int offset_x, offset_y;
  int scale;

  int advanced;
  int flags;

 public:
  explicit minimap_t(interface_t *interface);

  int get_flags() const { return flags; }
  void set_flags(int flags) { this->flags = flags; }
  int get_advanced() const { return advanced; }
  void set_advanced(int advanced) { this->advanced = advanced; }
  int get_scale() const { return scale; }
  void set_scale(int scale);

  void move_to_map_pos(map_pos_t pos);
  void move_by_pixels(int x, int y);
  map_pos_t get_current_map_pos();

  void screen_pix_from_map_pos(map_pos_t pos, int *sx, int *sy);
  map_pos_t map_pos_from_screen_pix(int x, int y);

 protected:
  void draw_minimap_point(int col, int row, uint8_t color, int density);
  void draw_minimap_map();
  void draw_minimap_ownership(int density);
  void draw_minimap_roads();
  void draw_minimap_buildings();
  void draw_minimap_traffic();
  void draw_minimap_grid();
  void draw_minimap_rect();
  int handle_scroll(int up);
  void screen_pix_from_map_pix(int mx, int my, int *sx, int *sy);
  void map_pix_from_map_coord(map_pos_t pos, int *mx, int *my);

  virtual void internal_draw();
  virtual bool handle_click_left(int x, int y);
  virtual bool handle_drag(int dx, int dy);
};

#endif  // SRC_MINIMAP_H_
