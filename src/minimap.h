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

#include <vector>

#include "src/gui.h"
#include "src/map.h"
#include "src/game.h"

class Interface;

class Minimap : public GuiObject {
 protected:
  PMap map;

  int offset_x, offset_y;
  int scale;

  bool draw_grid;

  int current_player_index;  // for option_FogOfWar

  std::vector<Color> minimap;

 public:
  explicit Minimap(PMap map);

  void set_map(PMap map);

  void set_player_index(int player_index) {
    Log::Debug["minimap.c"] << "inside Minimap::set_player_index, current_player_index was " << current_player_index << ", now changed to " << player_index;
     current_player_index = player_index;
  }  // for option_FogOfWar

  int get_scale() const { return scale; }
  void set_scale(int scale);
  bool get_draw_grid() const { return draw_grid; }
  void set_draw_grid(bool draw_grid);

  void move_to_map_pos(MapPos pos);
  void move_by_pixels(int x, int y);
  MapPos get_current_map_pos();

  void screen_pix_from_map_pos(MapPos pos, int *sx, int *sy);
  MapPos map_pos_from_screen_pix(int x, int y);

 protected:
  static const int max_scale;

  void init_minimap();

  void draw_minimap_point(int col, int row, const Color &color, int density);
  void draw_minimap_map();
  void draw_minimap_grid();
  void draw_minimap_rect();
  int handle_scroll(int up);
  void screen_pix_from_map_pix(int mx, int my, int *sx, int *sy);
  void map_pix_from_map_coord(MapPos pos, int *mx, int *my);

  virtual void internal_draw();
  virtual bool handle_drag(int dx, int dy);
  virtual bool handle_mouse_button_down(int x, int y, Event::Button button);
};

class MinimapGame : public Minimap {
 public:
  typedef enum OwnershipMode {
    OwnershipModeNone = 0,
    OwnershipModeMixed = 1,
    OwnershipModeSolid = 2,
    OwnershipModeLast = OwnershipModeSolid
  } OwnershipMode;

 protected:
  Interface *interface;

  PGame game;

  int advanced;
  bool draw_roads;
  bool draw_buildings;
  OwnershipMode ownership_mode;

 public:
  MinimapGame(Interface *interface, PGame game);

  int get_advanced() const { return advanced; }
  void set_advanced(int advanced) { this->advanced = advanced; }
  bool get_draw_roads() const { return draw_roads; }
  void set_draw_roads(bool draw_roads);
  bool get_draw_buildings() const { return draw_buildings; }
  void set_draw_buildings(bool draw_buildings);
  OwnershipMode get_ownership_mode() { return ownership_mode; }
  void set_ownership_mode(OwnershipMode _ownership_mode);

 protected:
  void draw_minimap_ownership(int density);
  void draw_minimap_roads();
  void draw_minimap_buildings();
  void draw_minimap_traffic();

  virtual void internal_draw();
  virtual bool handle_left_click(int x, int y, int modifier);
};

#endif  // SRC_MINIMAP_H_
