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
#include "src/ai.h"

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
  std::vector<PopupBox *> pinned_popups;  // tlongstretch adding support for multiple/pinned/moveable popups
  GameInitBox *init_box;
  NotificationBox *notification_box;

  MapPos map_cursor_pos;
  CursorType map_cursor_type;
  BuildPossibility build_possibility;

  unsigned int last_const_tick;
  unsigned int last_autosave_tick;
  unsigned int last_subseason_tick;  // messing with weather/seasons/palette

  Road building_road;
  int building_road_valid_dir;

  int sfx_queue[4];

  Player *player;
  int config;
  int msg_flags;

  SpriteLoc map_cursor_sprites[7];

  int current_stat_8_mode;
  Resource::Type current_stat_7_item;

  int return_timeout;
  int return_pos;

  AI *ai_ptrs[5] = { NULL, NULL, NULL, NULL, NULL };
  CustomMapGeneratorOptions custom_map_generator_options;
  //bool regen_map = false;

 public:
  Interface();
  virtual ~Interface();

  PGame get_game() { return game; }
  void set_game(PGame game);

  Color get_player_color(unsigned int player_index);

  Viewport *get_viewport();
  PanelBar *get_panel_bar();
  PopupBox *get_popup_box();
  std::vector<PopupBox *> get_pinned_popup_boxes();
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

  int get_current_stat_8_mode() const { return current_stat_8_mode; }
  void set_current_stat_8_mode(int mode) { current_stat_8_mode = mode; }
  Resource::Type get_current_stat_7_item() const { return current_stat_7_item; }
  void set_current_stat_7_item(Resource::Type item) { current_stat_7_item = item; }

  BuildPossibility get_build_possibility() const { return build_possibility; }

  void open_popup(int box);
  //void close_popup();
  //void close_popup(int x, int y);  // added coordinates to determine which pinned popup (if any) is being closed
  void close_popup(PopupBox *popup_to_close);  // close specific popup box
  void close_pinned_popups();  // close ALL pinned popups
  void pin_popup();  // for moveable popups
  void draw_transient_popup(); // for PleaseWait notification popups that require no user action and disappear once complete, for slow functions

  void open_game_init();
  void close_game_init();
  bool is_game_init_open() { return init_box != nullptr; }  // added for FogOfWar and other new options, so that minimap regen can be triggered by closing a gameoption popup";
  GameInitBox *get_game_init() { return init_box; }  // added for FogOfWar and other new options, so that minimap regen can be triggered by closing a gameoption popup";

  void reload_any_minimaps();

  void initialize_AI();

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

  // used for viewport and maybe other funtions to get the ai object so they can fetch the ai_mark stuff for AI overlay for debugging
  AI * get_ai_ptr(unsigned int index) { return ai_ptrs[index]; }

  CustomMapGeneratorOptions get_custom_map_generator_options();

  // this allows the EditCustomMapGenerator popup to tell the
  //  GameInitBox to refresh the map on popup
  void tell_gameinit_regen_map();

/*
  //uint16_t slider_double_to_uint16(double val){ return uint16_t(val * 32750); }
  double get_custom_map_generator_trees(){ return uint16_t(16375 * custom_map_generator_options.opt[CustomMapGeneratorOption::Trees]); }
  double get_custom_map_generator_stonepile_dense(){ return slider_double_to_uint16(custom_map_generator_options.opt[CustomMapGeneratorOption::StonepileDense]); }
  double get_custom_map_generator_stonepile_sparse(){ return slider_double_to_uint16(custom_map_generator_options.opt[CustomMapGeneratorOption::StonepileSparse]); }
  double get_custom_map_generator_fish(){ return uint16_t(16375 * custom_map_generator_options.opt[CustomMapGeneratorOption::Fish]); }
  double get_custom_map_generator_mountain_gold(){ return slider_double_to_uint16(custom_map_generator_options.opt[CustomMapGeneratorOption::MountainGold]); }
  double get_custom_map_generator_mountain_iron(){ return slider_double_to_uint16(custom_map_generator_options.opt[CustomMapGeneratorOption::MountainIron]); }
  double get_custom_map_generator_mountain_coal(){ return slider_double_to_uint16(custom_map_generator_options.opt[CustomMapGeneratorOption::MountainCoal]); }
  double get_custom_map_generator_mountain_stone(){ return slider_double_to_uint16(custom_map_generator_options.opt[CustomMapGeneratorOption::MountainStone]); }
  double get_custom_map_generator_desert_frequency(){ return slider_double_to_uint16(custom_map_generator_options.opt[CustomMapGeneratorOption::DesertFrequency]); }
  double get_custom_map_generator_lakes_water_level(){ return uint16_t(8188 * custom_map_generator_options.opt[CustomMapGeneratorOption::LakesWaterLevel]); }
  double get_custom_map_generator_junk_grass_dead_trees(){ return slider_double_to_uint16(custom_map_generator_options.opt[CustomMapGeneratorOption::JunkGrassDeadTrees]); }
  double get_custom_map_generator_junk_grass_sandstone(){ return slider_double_to_uint16(custom_map_generator_options.opt[CustomMapGeneratorOption::JunkGrassSandStone]); }
  double get_custom_map_generator_junk_grass_stub_trees(){ return slider_double_to_uint16(custom_map_generator_options.opt[CustomMapGeneratorOption::JunkGrassStubTrees]); }
  double get_custom_map_generator_junk_grass_small_boulders(){ return slider_double_to_uint16(custom_map_generator_options.opt[CustomMapGeneratorOption::JunkGrassSmallBoulders]); }
  double get_custom_map_generator_junk_water_trees(){ return slider_double_to_uint16(custom_map_generator_options.opt[CustomMapGeneratorOption::JunkWaterSubmergedTrees]); }
  double get_custom_map_generator_junk_water_boulders(){ return slider_double_to_uint16(custom_map_generator_options.opt[CustomMapGeneratorOption::JunkWaterSubmergedBoulders]); }
  double get_custom_map_generator_junk_desert_cadavers(){ return slider_double_to_uint16(custom_map_generator_options.opt[CustomMapGeneratorOption::JunkDesertAnimalCadavers]); }
  double get_custom_map_generator_junk_desert_cacti(){ return slider_double_to_uint16(custom_map_generator_options.opt[CustomMapGeneratorOption::JunkDesertCacti]); }
  double get_custom_map_generator_junk_desert_palm_trees(){ return slider_double_to_uint16(custom_map_generator_options.opt[CustomMapGeneratorOption::JunkDesertPalmTrees]); }
  double get_custom_map_generator_junk_water_reeds_cattails(){ return uint16_t(4096 * custom_map_generator_options.opt[CustomMapGeneratorOption::JunkWaterReedsCattails]); }
  */

  // this is a dumb hack using globals to allow the game-options class to indirectly interact with the Interface
  //  to support save/load of map generator options to disk
  void set_custom_map_generator_options_from_global();
  void reset_custom_map_generator_options();

  void set_custom_map_generator_trees(uint16_t val){
    // reasonable values for trees are 0.00-4.00, so divide max slider 65500 by 4 to get 16375 and let 1.00 == 16375
    custom_map_generator_options.opt[CustomMapGeneratorOption::Trees] = double(val) / double(16375);
  }
  void set_custom_map_generator_stonepile_dense(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::StonepileDense] = slider_uint16_to_double(val); }
  void set_custom_map_generator_stonepile_sparse(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::StonepileSparse] = slider_uint16_to_double(val); }
  //void set_custom_map_generator_fish(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::Fish] = slider_uint16_to_double(val); }
  void set_custom_map_generator_fish(uint16_t val){
    // reasonable values for fish are 0.00-4.00, so divide max slider 65500 by 4 to get 16375 and let 1.00 == 16375
    custom_map_generator_options.opt[CustomMapGeneratorOption::Fish] = double(val) / double(16375);
  }
  void set_custom_map_generator_mountain_gold(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::MountainGold] = slider_mineral_uint16_to_int_to_double(val); }
  void set_custom_map_generator_mountain_iron(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::MountainIron] = slider_mineral_uint16_to_int_to_double(val); }
  void set_custom_map_generator_mountain_coal(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::MountainCoal] = slider_mineral_uint16_to_int_to_double(val); }
  void set_custom_map_generator_mountain_stone(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::MountainStone] = slider_mineral_uint16_to_int_to_double(val); }
  void set_custom_map_generator_desert_frequency(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::DesertFrequency] = slider_uint16_to_double(val); }
  void set_custom_map_generator_lakes_water_level(uint16_t val){
    // default is 20 (really, 20??), values as high as 150 seem okay, resulting in islands surrounded by water
    // these island maps are not very playable, and AI won't be able to place a castle, but leaving
    // this available for experimentation.  A set of game rules adjustments need to be made to
    // create island maps with beaches and some way of being able to place knight huts far away
    // so other islands can be colonized
    custom_map_generator_options.opt[CustomMapGeneratorOption::LakesWaterLevel] = double(val) / double(8188);
  }
  void set_custom_map_generator_junk_grass_sandstone(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::JunkGrassSandStone] = slider_uint16_to_double(val); }
  void set_custom_map_generator_junk_grass_stub_trees(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::JunkGrassStubTrees] = slider_uint16_to_double(val); }
  void set_custom_map_generator_junk_grass_small_boulders(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::JunkGrassSmallBoulders] = slider_uint16_to_double(val); }
  void set_custom_map_generator_junk_grass_dead_trees(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::JunkGrassDeadTrees] = slider_uint16_to_double(val); }
  void set_custom_map_generator_junk_water_boulders(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::JunkWaterSubmergedBoulders] = double(val) / double(4096); }
  void set_custom_map_generator_junk_water_trees(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::JunkWaterSubmergedTrees] = double(val) / double(4096); }
  void set_custom_map_generator_junk_desert_cadavers(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::JunkDesertAnimalCadavers] = slider_uint16_to_double(val); }
  void set_custom_map_generator_junk_desert_cacti(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::JunkDesertCacti] = slider_uint16_to_double(val); }
  void set_custom_map_generator_junk_desert_palm_trees(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::JunkDesertPalmTrees] = slider_uint16_to_double(val); }
  
  void set_custom_map_generator_junk_water_reeds_cattails(uint16_t val){ custom_map_generator_options.opt[CustomMapGeneratorOption::JunkWaterReedsCattails] = double(val) / double(4096); }

  // used to trigger ambient bird/wind/wave sounds
  int trees_in_view;
  bool is_playing_birdsfx;
  int desert_in_view;
  bool is_playing_desertsfx;
  int water_in_view;
  bool is_playing_watersfx;

  void clear_custom_graphics_cache();  // for messing with weather/seasons/palette  // moved from protected
  

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

  void set_ai_ptr(unsigned int index, AI *ai) { ai_ptrs[index] = ai; }

  // THESE FUNCTIONS BELOW ARE ALSO DEFINED IN map-generator.h and popup.h !!!!
  // 65500 (not 65535) / 2 = 32750
  double slider_uint16_to_double(uint16_t val){ return double(double(val) / double(32750)); }
  uint16_t slider_double_to_uint16(double val){ return uint16_t(val * 32750); }
  /* the default mineral amounts are already really high, leave the defaults as the max reasonable values
  // 65500 (not 65535) / 17 = 3852.94  (trying 3852)
  double slider_mineral_uint16_to_int_to_double(uint16_t val){ return double(int(val / 3852)); }  // convert to int midway so there are no fractional values
  uint16_t slider_mineral_double_to_uint16(double val){ return uint16_t(val * 3853); }
  */
  // 65500 (not 65535) / 9 = 7277.77, trying 7277 and 7278
  double slider_mineral_uint16_to_int_to_double(uint16_t val){ return double(int(val / 7277)); }  // convert to int midway so there are no fractional values
  uint16_t slider_mineral_double_to_uint16(double val){ return uint16_t(val * 7278); }

  //void clear_custom_graphics_cache();  // for messing with weather/seasons/palette  // moved to public

  // GameManager::Handler implementation
 public:
  virtual void on_new_game(PGame game);
  virtual void on_end_game(PGame game);
};

#endif  // SRC_INTERFACE_H_
