/*
 * viewport.h - Viewport GUI component
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

#ifndef SRC_VIEWPORT_H_
#define SRC_VIEWPORT_H_

#include <map>
#include <memory>

#include "src/gui.h"
#include "src/map.h"
#include "src/building.h"

class Interface;
class DataSource;

class Viewport : public GuiObject, public Map::Handler {
 public:
  typedef enum Layer {
    LayerLandscape = 1<<0,
    LayerPaths = 1<<1,
    LayerObjects = 1<<2,
    LayerSerfs = 1<<3,
    LayerCursor = 1<<4,
    LayerGrid = 1<<5,
    LayerBuilds = 1<<6,
    LayerAll = (LayerLandscape |
                LayerPaths |
                LayerObjects |
                LayerSerfs |
                LayerCursor),
  } Layer;

 protected:
  /* Cache prerendered tiles of the landscape. */
  typedef std::map<unsigned int, std::unique_ptr<Frame>> TilesMap;
  TilesMap landscape_tiles;

  int offset_x, offset_y;
  unsigned int layers;
  Interface *interface;
  unsigned int last_tick;
  PDataSource data_source;

  PMap map;

 public:
  Viewport(Interface *interface, PMap map);
  virtual ~Viewport();

  void switch_layer(Layer layer) { layers ^= layer; }

  void move_to_map_pos(MapPos pos);
  void move_by_pixels(int x, int y);
  MapPos get_current_map_pos();

  void screen_pix_from_map_pix(int mx, int my, int *sx, int *sy);
  void map_pix_from_map_coord(MapPos pos, int h, int *mx, int *my);
  MapPos map_pos_from_screen_pix(int x, int y);

  void redraw_map_pos(MapPos pos);

  void update();

 protected:
  void draw_triangle_up(int x, int y, int m, int left, int right, MapPos pos,
                        Frame *frame);
  void draw_triangle_down(int x, int y, int m, int left, int right,
                          MapPos pos, Frame *frame);
  void draw_up_tile_col(MapPos pos, int x_base, int y_base, int max_y,
                        Frame *frame);
  void draw_down_tile_col(MapPos pos, int x_base, int y_base, int max_y,
                          Frame *frame);
  void draw_landscape();
  void draw_path_segment(int x, int y, MapPos pos, Direction dir);
  void draw_border_segment(int x, int y, MapPos pos, Direction dir);
  void draw_paths_and_borders();
  void draw_game_sprite(int x, int y, int index);
  void draw_serf(int x, int y, const Color &color, int head, int body);
  void draw_shadow_and_building_sprite(int x, int y, int index,
                                       const Color &color = Color::transparent);
  void draw_shadow_and_building_unfinished(int x, int y, int index,
                                           int progress);
  void draw_building_unfinished(Building *building, Building::Type bld_type,
                                int x, int y);
  void draw_ocupation_flag(Building *building, int x, int y, float mul);
  void draw_unharmed_building(Building *building, int x, int y);
  void draw_burning_building(Building *building, int x, int y);
  void draw_building(MapPos pos, int x, int y);
  void draw_water_waves(MapPos pos, int x, int y);
  void draw_water_waves_row(MapPos pos, int y_base, int cols, int x_base);
  void draw_flag_and_res(MapPos pos, int x, int y);
  void draw_map_objects_row(MapPos pos, int y_base, int cols, int x_base);
  void draw_row_serf(int x, int y, bool shadow, const Color &color, int body);
  int serf_get_body(Serf *serf);
  void draw_active_serf(Serf *serf, MapPos pos, int x_base, int y_base);
  void draw_serf_row(MapPos pos, int y_base, int cols, int x_base);
  void draw_serf_row_behind(MapPos pos, int y_base, int cols, int x_base);
  void draw_game_objects(int layers);
  void draw_map_cursor_sprite(MapPos pos, int sprite);
  void draw_map_cursor_possible_build();
  void draw_map_cursor();
  void draw_base_grid_overlay(const Color &color);
  void draw_height_grid_overlay(const Color &color);

  virtual void internal_draw();
  virtual void layout();
  virtual bool handle_click_left(int x, int y);
  virtual bool handle_dbl_click(int x, int y, Event::Button button);
  virtual bool handle_drag(int x, int y);

  Frame *get_tile_frame(unsigned int tid, int tc, int tr);

 public:
  virtual void on_height_changed(MapPos pos);
  virtual void on_object_changed(MapPos pos);
};

#endif  // SRC_VIEWPORT_H_
