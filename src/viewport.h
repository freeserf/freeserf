/*
 * viewport.h - Viewport GUI component
 *
 * Copyright (C) 2013-2019  Jon Lund Steffensen <jonlst@gmail.com>
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
  typedef enum Layer {  // 'layers' is an unsigned int which is 4-bytes (32bits) so can have up to 32 layers
    LayerLandscape = 1<<0,
    LayerPaths = 1<<1,
    LayerObjects = 1<<2,
    LayerSerfs = 1<<3,
    LayerCursor = 1<<4,
    LayerGrid = 1<<5,
    LayerBuilds = 1<<6,
    LayerAI = 1<<7,
    LayerHiddenResources = 1<<8,
    LayerDebug = 1<<9,
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
  Data::PSource data_source;
  float last_zoom_factor;  // tlongstretch, attempt to recenter screen when zooming
  unsigned int last_res_width; // tlongstretch, attempt to recenter screen when zooming
  unsigned int last_res_height; // tlongstretch, attempt to recenter screen when zooming
  unsigned int last_window_width; // tlongstretch, attempt to recenter screen when zooming
  unsigned int last_window_height; // tlongstretch, attempt to recenter screen when zooming

  const int fall_1st_colors[8] = {  0, 10, 20, 60, 10,  0, 50, 40};  // for option_FourSeasons fall tree coloration
  const int fall_2nd_colors[8] = { 20, 30, 50, 20, 70, 30, 70, 50};  // for option_FourSeasons fall tree coloration


  const int res_pos[16] = {  6, -4,      // moved this from Viewport::draw_flag_and_res
                            10, -2,
                            -4, -4,
                            10,  2,
                            -8, -2,
                             6,  4,
                            -8,  2,
                            -4,  4 };

  const int pigfarm_anim[256] = {    // moved this from Viewport::draw_unharmed_building
    0xa2, 0, 0xa2, 0, 0xa2, 0, 0xa2, 0, 0xa2, 0, 0xa3, 0,
    0xa2, 1, 0xa3, 1, 0xa2, 2, 0xa3, 2, 0xa2, 3, 0xa3, 3,
    0xa2, 4, 0xa3, 4, 0xa6, 4, 0xa6, 4, 0xa6, 4, 0xa6, 4,
    0xa4, 4, 0xa5, 4, 0xa4, 3, 0xa5, 3, 0xa4, 2, 0xa5, 2,
    0xa4, 1, 0xa5, 1, 0xa4, 0, 0xa5, 0, 0xa2, 0, 0xa2, 0,
    0xa6, 0, 0xa6, 0, 0xa6, 0, 0xa2, 0, 0xa7, 0, 0xa8, 0,
    0xa7, 0, 0xa8, 0, 0xa7, 0, 0xa8, 0, 0xa7, 0, 0xa8, 0,
    0xa7, 0, 0xa8, 0, 0xa7, 0, 0xa8, 0, 0xa7, 0, 0xa8, 0,
    0xa7, 0, 0xa8, 0, 0xa7, 0, 0xa2, 0, 0xa2, 0, 0xa2, 0,
    0xa2, 0, 0xa6, 0, 0xa6, 0, 0xa6, 0, 0xa6, 0, 0xa6, 0,
    0xa6, 0, 0xa2, 0, 0xa2, 0, 0xa7, 0, 0xa8, 0, 0xa9, 0,
    0xaa, 0, 0xab, 0, 0xac, 0, 0xad, 0, 0xac, 0, 0xad, 0,
    0xac, 0, 0xad, 0, 0xac, 0, 0xad, 0, 0xac, 0, 0xad, 0,
    0xac, 0, 0xad, 0, 0xac, 0, 0xad, 0, 0xac, 0, 0xab, 0,
    0xaa, 0, 0xa9, 0, 0xa8, 0, 0xa7, 0, 0xa2, 0, 0xa2, 0,
    0xa2, 0, 0xa2, 0, 0xa3, 0, 0xa2, 1, 0xa3, 1, 0xa2, 1,
    0xa3, 2, 0xa2, 2, 0xa3, 2, 0xa7, 2, 0xa8, 2, 0xa7, 2,
    0xa8, 2, 0xa7, 2, 0xa8, 2, 0xa7, 2, 0xa8, 2, 0xa7, 2,
    0xa8, 2, 0xa7, 2, 0xa8, 2, 0xa7, 2, 0xa8, 2, 0xa7, 2,
    0xa2, 2, 0xa2, 2, 0xa6, 2, 0xa6, 2, 0xa6, 2, 0xa6, 2,
    0xa4, 2, 0xa5, 2, 0xa4, 1, 0xa5, 1, 0xa4, 0, 0xa5, 0,
    0xa2, 0, 0xa2, 0
  };

  const int pigs_layout[36] = {    // moved this from Viewport::draw_unharmed_building
    0,   0,   0,  0,
    1,  40,   2, 11,
    2, 460,   0, 17,
    3, 420, -11,  8,
    4,  90, -11, 19,
    5, 280,   8,  8,
    6, 140,  -2,  6,
    7, 180,  -8, 13,
    8, 320,  13, 14,
  };

  const int building_anim_offset_from_type[25] = {    // moved this from Viewport::draw_burning_building
    0, 10, 26, 39, 49, 62, 78, 97, 97, 116,
    129, 157, 167, 198, 211, 236, 255, 277, 305, 324,
    349, 362, 381, 418, 446
  };

  const int building_burn_animation[483] = {  // moved this from Viewport::draw_burning_building
    /* Unfinished */
    0, -8, 2,
    8, 0, 4,
    0, 8, 2, -1,

    /* Fisher */
    0, -8, -10,
    0, 4, -12,
    8, -8, 4,
    8, 7, 4,
    0, -2, 8, -1,

    /* Lumberjack */
    0, 3, -13,
    0, -8, -10,
    8, 9, 3,
    8, -6, 3, -1,

    /* Boat builder */
    0, -1, -12,
    8, -8, 11,
    8, 7, 5, -1,

    /* Stone cutter */
    0, 6, -14,
    0, -9, -11,
    8, -8, 5,
    8, 6, 5, -1,

    /* Stone mine */
    8, -4, -40,
    8, -8, -15,
    8, 3, -14,
    8, -9, 4,
    8, 6, 5, -1,

    /* Coal mine */
    8, -4, -40,
    8, -1, -18,
    8, -8, -15,
    8, 6, 2,
    8, 0, 8,
    8, -8, 9, -1,

    /* Iron mine, gold mine */
    8, -4, -40,
    8, -2, -19,
    8, -9, -14,
    8, -8, 2,
    8, 6, 2,
    0, -4, 8, -1,

    /* Forester */
    0, 8, -11,
    0, -6, -8,
    8, -8, 4,
    8, 6, 4, -1,

    /* Stock */
    0, -2, -25,
    0, 6, -17,
    0, -9, -16,
    8, -21, 1,
    8, 21, 2,
    0, 15, 18,
    0, -16, 10,
    8, -8, 15,
    8, 5, 15, -1,

    /* Hut */
    0, 0, -11,
    8, -8, 5,
    8, 8, 5, -1,

    /* Farm */
    8, 22, -2,
    8, 7, -5,
    8, -3, -1,
    8, -23, 0,
    8, -12, 4,
    0, 25, 5,
    0, 21, 13,
    0, -17, 8,
    0, -10, 15,
    0, -2, 15, -1,

    /* Butcher */
    8, -15, 3,
    8, 20, 3,
    8, 7, 3,
    8, -4, 3, -1,

    /* Pig farm */
    8, 0, -2,
    8, 22, 1,
    8, 15, 5,
    8, -20, -1,
    8, -11, 3,
    0, 20, 12,
    0, -16, 7,
    0, -12, 14, -1,

    /* Mill */
    0, 7, -33,
    0, 5, -20,
    8, -2, -24,
    8, -6, 1,
    8, 4, 2,
    0, -3, 6, -1,

    /* Baker */
    0, -15, -16,
    0, -4, -19,
    0, 3, -16,
    8, -13, 2,
    8, -9, 7,
    8, 6, 7,
    0, 17, 1, -1,

    /* Saw mill */
    0, 7, -19,
    0, -1, -14,
    0, 16, -13,
    0, 5, -8,
    8, 14, 4,
    0, 10, 9,
    0, -17, 8,
    8, -11, 10,
    8, -1, 12, -1,

    /* Steel smelter */
    0, 5, -19,
    0, 16, -16,
    8, -14, 2,
    8, -10, 5,
    8, 15, 5,
    8, 2, 5, -1,

    /* Tool maker */
    8, 7, -19,
    0, -11, -17,
    0, -4, -11,
    0, 12, -10,
    8, -20, 0,
    8, -15, 7,
    8, 1, 7,
    8, 15, 7, -1,

    /* Weapon smith */
    8, -15, 1,
    8, -10, 3,
    8, 20, 3,
    8, 5, 3, -1,

    /* Tower */
    0, -6, -30,
    0, 7, -14,
    8, -11, -3,
    0, -8, 4,
    8, 9, 5,
    8, -4, 5, -1,

    /* Fortress */
    0, -3, -30,
    0, -15, -26,
    0, 21, -29,
    0, -13, -17,
    8, 4, -11,
    8, -2, -6,
    8, -22, 0,
    8, -17, 8,
    8, 20, 1,
    8, 10, 8,
    8, 4, 13,
    8, -11, 15, -1,

    /* Gold smelter */
    0, -15, -20,
    0, 10, -22,
    0, -3, -25,
    0, -8, -10,
    0, 7, -10,
    0, -13, 2,
    8, -8, 5,
    8, 6, 5,
    0, 16, 6, -1,

    /* Castle */
    0, 11, -46,
    0, -19, -42,
    8, 1, -27,
    8, 10, -13,
    0, -7, -24,
    8, -16, -6,
    0, -23, 4,
    8, -2, 0,
    8, 12, 12,
    8, -14, 17,
    8, -4, 19,
    0, 13, 19, -1
  };

  
  // these are all used for determing which sounds should play (when viewport is large)
  bool in_ambient_focus; // this is the final check set true if both col and row true
  bool row_in_ambient_focus;   // this must be shared between parent and child functions
  //bool col_in_ambient_focus;  // this is local to the drawing function
  //const int focus_cols = 18;
  //const int focus_cols = 24;
  const int focus_cols = 30;
  //int center_col;  // this is local to the drawing function
  //int leftmost_focus_col;  // this is local to the drawing function
  //int rightmost_focus_col;  // this is local to the drawing function
  //const int focus_y_pixels = 480;
  const int focus_y_pixels = 640;
  //const int focus_y_pixels = 800;
  int center_y;
  int topmost_focus_y;
  int lowest_focus_y;

  PMap map;

 public:
  Viewport(Interface *interface, PMap map);
  virtual ~Viewport();

  void switch_layer(Layer layer) { layers ^= layer; }
  bool layer_active(Layer layer) { return layers & layer; }

  void move_to_map_pos(MapPos pos);
  void move_by_pixels(int x, int y);
  MapPos get_current_map_pos();

  void screen_pix_from_map_pix(int mx, int my, int *sx, int *sy);
  void map_pix_from_map_coord(MapPos pos, int h, int *mx, int *my);
  void screen_pix_from_map_coord(MapPos pos, int *sx, int *sy);
  MapPos map_pos_from_screen_pix(int x, int y);
  //MapPos map_pos_from_tile_frame_coord(unsigned int tid, int tc, int tr);
  MapPos map_pos_from_tile_frame_coord(int tc, int tr);
  unsigned int tile_id_from_map_pos(MapPos pos);

  void redraw_map_pos(MapPos pos);

  void update();

  //virtual void store_prev_res();
  virtual void store_prev_window_size();

 protected:
  Map::Terrain special_terrain_type(MapPos pos, Map::Terrain type);  // convenience function to allow changes to both draw_triangle_up & _down without code duplication
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
  // this says shadow and building but it seems to include ANY map object sprite such as trees, stones
  //void draw_shadow_and_building_sprite(int x, int y, int index, const Color &color = Color::transparent);
  void draw_shadow_and_building_sprite(int x, int y, int index, const Color &color = Color::transparent, int mutate = 0);
  //void draw_shadow_and_custom_building_sprite(int x, int y, int index, const Color &color = Color::transparent);
  // new function to try messing with weather/seasons/palette
  //void draw_map_sprite_special(int x, int y, int index, unsigned int pos, unsigned int obj, const Color &color = Color::transparent);
  void draw_map_sprite_special(int x, int y, int index, const Color &color = Color::transparent, int mutate = 0); // this is basically just used for custom shadow logic, it is no longer really needed! 
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
  // add passing ly to this function to allow determination of y/row location
  //  inside the viewport so that a focus area can be selected for ambient sounds
  //void draw_map_objects_row(MapPos pos, int y_base, int cols, int x_base, int debug_ly);
  void draw_row_serf(int x, int y, bool shadow, const Color &color, int body);
  int serf_get_body(Serf *serf);
  void draw_active_serf(Serf *serf, MapPos pos, int x_base, int y_base);
  void draw_serf_row(MapPos pos, int y_base, int cols, int x_base);
  //void draw_serf_row(MapPos pos, int y_base, int cols, int x_base, int debug_ly);
  void draw_serf_row_behind(MapPos pos, int y_base, int cols, int x_base);
  //void draw_serf_row_behind(MapPos pos, int y_base, int cols, int x_base, int debug_ly);
  void draw_game_objects(int layers);
  void draw_map_cursor_sprite(MapPos pos, int sprite);
  void draw_map_cursor_possible_build();
  void draw_map_cursor();
  void draw_base_grid_overlay(const Color &color);
  void draw_height_grid_overlay(const Color &color);
  void draw_ai_overlay();
  void draw_debug_overlay();
  void draw_hidden_res_overlay();
  MapPos get_offset(int *x_off, int *y_off,
                    int *col = nullptr, int *row = nullptr);

  virtual void internal_draw();
  virtual void layout();
  //virtual void store_prev_res();
  //void store_prev_res();
  virtual bool handle_numpad_key_pressed(char numpad_key);
  virtual bool handle_left_click(int x, int y, int modifier);
  //virtual bool handle_right_click(int x, int y);
  virtual bool handle_dbl_click(int x, int y, Event::Button button);
  virtual bool handle_special_click(int x, int y);//, Event::Button button);
  virtual bool handle_mouse_button_down(int x, int y, Event::Button button); // testing moveable popups
  virtual bool handle_drag(int x, int y);

  Frame *get_tile_frame(unsigned int tid, int tc, int tr);

 public:
  virtual void on_height_changed(MapPos pos);
  virtual void on_object_changed(MapPos pos);
};

#endif  // SRC_VIEWPORT_H_
