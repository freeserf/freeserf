/*
 * interface.h - Top-level GUI interface
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

#ifndef SRC_INTERFACE_H_
#define SRC_INTERFACE_H_

#include "src/misc.h"
BEGIN_EXT_C
  #include "src/random.h"
  #include "src/map.h"
  #include "src/player.h"
  #include "src/building.h"
END_EXT_C
#include "src/gui.h"

#define MAX_ROAD_LENGTH  256

static const int map_building_sprite[] = {
  0, 0xa7, 0xa8, 0xae, 0xa9,
  0xa3, 0xa4, 0xa5, 0xa6,
  0xaa, 0xc0, 0xab, 0x9a, 0x9c, 0x9b, 0xbc,
  0xa2, 0xa0, 0xa1, 0x99, 0x9d, 0x9e, 0x98, 0x9f, 0xb2
};

typedef enum {
  MAP_CURSOR_TYPE_NONE = 0,
  MAP_CURSOR_TYPE_FLAG,
  MAP_CURSOR_TYPE_REMOVABLE_FLAG,
  MAP_CURSOR_TYPE_BUILDING,
  MAP_CURSOR_TYPE_PATH,
  MAP_CURSOR_TYPE_CLEAR_BY_FLAG,
  MAP_CURSOR_TYPE_CLEAR_BY_PATH,
  MAP_CURSOR_TYPE_CLEAR
} map_cursor_type_t;

typedef enum {
  CAN_BUILD_NONE = 0,
  CAN_BUILD_FLAG,
  CAN_BUILD_MINE,
  CAN_BUILD_SMALL,
  CAN_BUILD_LARGE,
  CAN_BUILD_CASTLE,
} build_possibility_t;

typedef struct {
  int sprite;
  int x, y;
} sprite_loc_t;

class viewport_t;
class panel_bar_t;
class popup_box_t;
class game_init_box_t;
class notification_box_t;

class interface_t : public gui_object_t {
 protected:
  uint32_t *serf_animation_table;

  random_state_t random;

  viewport_t *viewport;
  panel_bar_t *panel;
  popup_box_t *popup;
  game_init_box_t *init_box;
  notification_box_t *notification_box;

  map_pos_t map_cursor_pos;
  map_cursor_type_t map_cursor_type;
  build_possibility_t build_possibility;

  uint last_const_tick;

  bool building_road;
  map_pos_t building_road_source;
  dir_t building_road_dirs[MAX_ROAD_LENGTH];
  int building_road_length;
  int building_road_valid_dir;

  int sfx_queue[4];

  player_t *player;
  int config;
  int msg_flags;

  sprite_loc_t map_cursor_sprites[7];

  int current_stat_8_mode;
  int current_stat_7_item;

  int water_in_view;
  int trees_in_view;

  int return_timeout;
  int return_pos;

 public:
  interface_t();
  virtual ~interface_t();

  viewport_t *get_viewport();
  panel_bar_t *get_panel_bar();
  popup_box_t *get_popup_box();
  notification_box_t *get_notification_box() { return notification_box; }

  bool get_config(int i) const { return (BIT_TEST(config, i) != 0); }
  void set_config(int i) { config |= BIT(i); }
  void switch_config(int i) { BIT_INVERT(config, i); }

  map_pos_t get_map_cursor_pos() const { return map_cursor_pos; }
  map_cursor_type_t get_map_cursor_type() const { return map_cursor_type; }
  int get_map_cursor_sprite(int i) const {
    return map_cursor_sprites[i].sprite; }

  random_state_t *get_random() { return &random; }

  bool get_msg_flag(int i) const { return (BIT_TEST(msg_flags, i) != 0); }
  void set_msg_flag(int i) { msg_flags |= BIT(i); }

  int get_current_stat_8_mode() const { return current_stat_8_mode; }
  void set_current_stat_8_mode(int mode) { current_stat_8_mode = mode; }
  int get_current_stat_7_item() const { return current_stat_7_item; }
  void set_current_stat_7_item(int item) { current_stat_7_item = item; }

  uint32_t *get_animation_table() const { return serf_animation_table; }

  build_possibility_t get_build_possibility() const {
    return build_possibility; }

  void open_popup(int box);
  void close_popup();

  void open_game_init();
  void close_game_init();

  void open_message();
  void return_from_message();
  void close_message();

  player_t *get_player() const { return player; }
  void set_player(uint player);
  void update_map_cursor_pos(map_pos_t pos);

  bool is_building_road() const { return building_road; }
  map_pos_t get_building_road_source() const { return building_road_source; }
  int get_building_road_length() const { return building_road_length; }
  dir_t get_building_road_dir(int i) const { return building_road_dirs[i]; }
  void build_road_begin();
  void build_road_end();
  void build_road_reset() { building_road_length = 0; }
  int build_road_segment(dir_t dir);
  int remove_road_segment();
  int extend_road(dir_t *dirs, uint length);
  bool build_roid_is_valid_dir(dir_t dir) {
    return (BIT_TEST(building_road_valid_dir, dir) != 0); }

  void demolish_object();

  void build_flag();
  void build_building(building_type_t type);
  void build_castle();
  void build_road();

  void game_reset();
  void update();

  virtual bool handle_event(const event_t *event);

 protected:
  void get_map_cursor_type(const player_t *player, map_pos_t pos,
                           build_possibility_t *bld_possibility,
                           map_cursor_type_t *cursor_type);
  void determine_map_cursor_type();
  void determine_map_cursor_type_road();
  void update_interface();
  static void update_map_height(map_pos_t pos, void *data);
  void load_serf_animation_table();

  virtual void internal_draw();
  virtual void layout();
  virtual bool handle_key_pressed(char key, int modifier);
};

#endif  // SRC_INTERFACE_H_
