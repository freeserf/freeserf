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
#include "src/random.h"
#include "src/map.h"
#include "src/player.h"
#include "src/building.h"
#include "src/gui.h"
#include "src/game-manager.h"

static const unsigned int map_building_sprite[] = {
  0, 0xa7, 0xa8, 0xae, 0xa9,
  0xa3, 0xa4, 0xa5, 0xa6,
  0xaa, 0xc0, 0xab, 0x9a, 0x9c, 0x9b, 0xbc,
  0xa2, 0xa0, 0xa1, 0x99, 0x9d, 0x9e, 0x98, 0x9f, 0xb2
};

class Viewport;
class PanelBar;
class PopupBox;
class GameInitBox;
class NotificationBox;

class Interface : public GuiObject, public GameManager::Handler {
 public:
  typedef enum CursorType {
    CursorTypeNone = 0,
    CursorTypeFlag,
    CursorTypeRemovableFlag,
    CursorTypeBuilding,
    CursorTypePath,
    CursorTypeClearByFlag,
    CursorTypeClearByPath,
    CursorTypeClear
  } CursorType;

  typedef enum BuildPossibility {
    BuildPossibilityNone = 0,
    BuildPossibilityFlag,
    BuildPossibilityMine,
    BuildPossibilitySmall,
    BuildPossibilityLarge,
    BuildPossibilityCastle,
  } BuildPossibility;

  typedef enum StatAspect {
    StatAspectAll = 0,
    StatAspectLand,
    StatAspectBuildings,
    StatAspectMilitary
  } StatAspect;

  typedef enum StatScale {
    StatScale30Min = 0,
    StatScale60Min,
    StatScale600Min,
    StatScale3000Min
  } StatScale;

 protected:
  typedef struct SpriteLoc {
    int sprite;
    int x, y;
  } SpriteLoc;

 protected:
  PGame game;

  Random random;

  Viewport *viewport;
  PanelBar *panel;
  PopupBox *popup;
  GameInitBox *init_box;
  NotificationBox *notification_box;

  MapPos map_cursor_pos;
  CursorType map_cursor_type;
  BuildPossibility build_possibility;

  unsigned int last_const_tick;

  Road building_road;
  int building_road_valid_dir;

  int sfx_queue[4];

  Player *player;
  int config;
  int msg_flags;

  SpriteLoc map_cursor_sprites[7];

  StatScale selected_stat_scale;
  StatAspect selected_stat_aspect;
  Resource::Type selected_stat_resource;

  int water_in_view;
  int trees_in_view;

  int return_timeout;
  int return_pos;

 public:
  Interface();
  virtual ~Interface();

  PGame get_game() { return game; }
  void set_game(PGame game);

  Color get_player_color(unsigned int player_index);

  Viewport *get_viewport();
  PanelBar *get_panel_bar();
  PopupBox *get_popup_box();
  NotificationBox *get_notification_box() { return notification_box; }

  bool get_config(int i) const { return (BIT_TEST(config, i) != 0); }
  void set_config(int i) { config |= BIT(i); }
  void switch_config(int i) { BIT_INVERT(config, i); }

  MapPos get_map_cursor_pos() const { return map_cursor_pos; }
  CursorType get_map_cursor_type() const { return map_cursor_type; }
  int get_map_cursor_sprite(int i) const {
    return map_cursor_sprites[i].sprite; }

  Random *get_random() { return &random; }

  bool get_msg_flag(int i) const { return (BIT_TEST(msg_flags, i) != 0); }
  void set_msg_flag(int i) { msg_flags |= BIT(i); }

  StatScale get_selected_stat_scale() const { return selected_stat_scale; }
  void set_selected_stat_scale(StatScale scale) { selected_stat_scale = scale; }
  StatAspect get_selected_stat_aspect() const {
    return selected_stat_aspect;
  }
  void set_selected_stat_aspect(StatAspect aspect) {
    selected_stat_aspect = aspect;
  }
  Resource::Type get_selected_stat_resource() const {
      return selected_stat_resource;
  }
  void set_selected_stat_resource(Resource::Type item) {
      selected_stat_resource = item;
  }

  BuildPossibility get_build_possibility() const { return build_possibility; }

  void open_popup(int box);
  void close_popup();

  void open_game_init();
  void close_game_init();

  void open_message();
  void return_from_message();
  void close_message();

  Player *get_player() const { return player; }
  void set_player(unsigned int player);
  void update_map_cursor_pos(MapPos pos);

  bool is_building_road() const { return building_road.is_valid(); }
  const Road &get_building_road() const { return building_road; }
  void build_road_begin();
  void build_road_end();
  void build_road_reset() { build_road_end(); build_road_begin(); }
  int build_road_segment(Direction dir);
  int remove_road_segment();
  int extend_road(const Road &road);
  bool build_road_is_valid_dir(Direction dir) {
    return (BIT_TEST(building_road_valid_dir, dir) != 0); }

  void demolish_object();

  void build_flag();
  void build_building(Building::Type type);
  void build_castle();
  void build_road();

  void update();

  virtual bool handle_event(const Event *event);

 protected:
  void get_map_cursor_type(const Player *player, MapPos pos,
                           BuildPossibility *bld_possibility,
                           CursorType *cursor_type);
  void determine_map_cursor_type();
  void determine_map_cursor_type_road();
  void update_interface();
  static void update_map_height(MapPos pos, void *data);

  virtual void internal_draw();
  virtual void layout();
  virtual bool handle_key_pressed(char key, int modifier);

  // GameManager::Handler implementation
 public:
  virtual void on_new_game(PGame game);
  virtual void on_end_game(PGame game);
};

#endif  // SRC_INTERFACE_H_
