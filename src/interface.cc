/*
 * interface.cc - Top-level GUI interface
 *
 * Copyright (C) 2013-2018  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/interface.h"

#include <iostream>
#include <fstream>
#include <utility>
#include <thread>   //NOLINT (build/c++11) this is a Google Chromium req, not relevant to general C++.  // for AI threads
#include <vector>   //to satisfy cpplinter

#include "src/misc.h"
#include "src/debug.h"
#include "src/data.h"
#include "src/audio.h"
#include "src/freeserf_endian.h"
#include "src/freeserf.h"
#include "src/popup.h"
#include "src/game-init.h"
#include "src/viewport.h"
#include "src/notification.h"
#include "src/panel.h"
#include "src/savegame.h"
#include "src/lookup.h"
#include "src/ai.h"

// Interval between automatic save games
#define AUTOSAVE_INTERVAL  (10*60*TICKS_PER_SEC)

Interface::Interface()
  : building_road_valid_dir(0)
  , sfx_queue{0}
  , water_in_view(false)
  , trees_in_view(false)
  , return_pos(0) {
  displayed = true;

  game = nullptr;

  map_cursor_pos = 0;
  map_cursor_type = (CursorType)0;
  build_possibility = BuildPossibilityNone;

  player = nullptr;

  /* Settings */
  config = 0x39;
  msg_flags = 0;
  return_timeout = 0;

  current_stat_8_mode = 0;
  current_stat_7_item = 7;

  map_cursor_sprites[0].sprite = 32;
  map_cursor_sprites[1].sprite = 33;
  map_cursor_sprites[2].sprite = 33;
  map_cursor_sprites[3].sprite = 33;
  map_cursor_sprites[4].sprite = 33;
  map_cursor_sprites[5].sprite = 33;
  map_cursor_sprites[6].sprite = 33;

  last_const_tick = 0;

  viewport = nullptr;
  panel = nullptr;
  popup = nullptr;
  init_box = nullptr;
  notification_box = nullptr;

  GameManager::get_instance().add_handler(this);
  set_game(GameManager::get_instance().get_current_game());
}

Interface::~Interface() {
  GameManager::get_instance().del_handler(this);
  set_game(nullptr);

  delete viewport;
  delete panel;
  delete popup;
  delete init_box;
  delete notification_box;
}

Viewport *
Interface::get_viewport() {
  return viewport;
}

PanelBar *
Interface::get_panel_bar() {
  return panel;
}

PopupBox *
Interface::get_popup_box() {
  return popup;
}

/* Open popup box */
void
Interface::open_popup(int box) {
  if (popup == nullptr) {
    popup = new PopupBox(this);
    add_float(popup, 0, 0);
  }
  layout();
  // tlongstretch
  if (box == PopupBox::TypeAIPlusOptions){
    // double wide, normal height
    popup->set_size(288, 160);
    // recenter the popup
    //   because the popup->get_position call requires providing pointers to ints,
    //   we must create those ints and pointers and then check them after the call
    int options_pos_x = 0;
    int options_pos_y = 0;
    int *ptoptions_pos_x = &options_pos_x;
    int *ptoptions_pos_y = &options_pos_y;
    popup->get_position(ptoptions_pos_x, ptoptions_pos_y);
    options_pos_x = *ptoptions_pos_x;
    // shift left half of one "normal width", keep same height
    options_pos_x -= 72;
    popup->move_to(options_pos_x, options_pos_y);
  }
  popup->show((PopupBox::Type)box);
  if (panel != nullptr) {
    panel->update();
  }
}

/* Close the current popup. */
void
Interface::close_popup() {
  if (popup == nullptr) {
    return;
  }
  popup->hide();
  del_float(popup);
  delete popup;
  popup = nullptr;
  update_map_cursor_pos(map_cursor_pos);
  if (panel != nullptr) {
    panel->update();
  }
}

/* Open box for starting a new game */
void
Interface::open_game_init() {
  if (init_box == nullptr) {
    init_box = new GameInitBox(this);
    add_float(init_box, 0, 0);
  }
  init_box->set_displayed(true);
  init_box->set_enabled(true);
  if (panel != nullptr) {
    panel->set_displayed(false);
  }
  viewport->set_enabled(false);
  layout();
}

void
Interface::close_game_init() {
  if (init_box != nullptr) {
    init_box->set_displayed(false);
    del_float(init_box);
    delete init_box;
    init_box = nullptr;
  }
  if (panel != nullptr) {
    panel->set_displayed(true);
    panel->set_enabled(true);
  }
  viewport->set_enabled(true);
  layout();
  update_map_cursor_pos(map_cursor_pos);
  //
  // tlongstretch
  //
  // print AIPlus game options
  //   there is no way to iterate over a bitfield or an enum so this must be hardcoded
  //   and it must be updated any time an option is added!
  Log::Info["interface"] << " AIOption::Foo is " << std::to_string(aiplus_options.test(AIPlusOption::Foo));
  Log::Info["interface"] << " AIOption::Bar is " << std::to_string(aiplus_options.test(AIPlusOption::Bar));
  Log::Info["interface"] << " AIOption::Baz is " << std::to_string(aiplus_options.test(AIPlusOption::Baz));
  // start any AI threads
  initialize_AI();
}

// tlongstretch
AIPlusOptions &
Interface::get_aiplus_options() {
  static AIPlusOptions aiplus_options;
  return aiplus_options;
}

// tlongstretch
// Initialize AI for non-human players
void
Interface::initialize_AI() {
  //Log::Debug["interface"] << " TEMPORARILY DISABLING AI TO DO PERF CHECK";
  //return;
//     face: the face image that represents this player.
//           1-12 is AI, 13-14 is human player.
// NOTE - values are actually 1-11, 12-13 because index starts at zero(invalid)!
  Player *player = game->get_player(0);
  unsigned int index = 0;
  do {
    Log::Debug["interface"] << "player #" << index << " has face " << player->get_face() << " / " << NamePlayerFace[player->get_face()];
    if (player->get_face() >= 1 && player->get_face() <= 11) {
      Log::Info["freeserf"] << "Initializing AI for player #" << index;
      Log::Debug["ai"] << "MAIN GAME thread_id: " << std::this_thread::get_id();
      // set AI log file... I don't understand why this works the way it does
      //  the AI thread "takes" the file handle but the main game filehandle still works even though they all
      //    use the same static function???  and the class is a singleton??
      Log::set_file(new std::ofstream("ai_Player" + std::to_string(index) + ".txt"));
      AI *ai = new AI(game, index, aiplus_options);
      game->ai_thread_starting();
      // store AI pointer in game so it can be fetched by other functions (viewport, at least, for AI overlay)
      set_ai_ptr(index, ai);
      std::thread ai_thread(&AI::start, ai);
      ai_thread.detach();
    }
    index++;
    player = game->get_player(index);
  } while (player != nullptr);
  // this locking mechanism probably isn't needed anymore now that AI startup is moved to this function instead of during 'mission' game initialization
  Log::Debug["interface"] << "unlocking AI from interface.cc";
  game->unlock_ai();
}

/* Open box for next message in the message queue */
void
Interface::open_message() {
  if (!player->has_notification()) {
    play_sound(Audio::TypeSfxClick);
    return;
  } else if (!BIT_TEST(msg_flags, 3)) {
    msg_flags |= BIT(4);
    msg_flags |= BIT(3);
    MapPos pos = viewport->get_current_map_pos();
    return_pos = pos;
  }

  Message message = player->pop_notification();

  if (message.type == Message::TypeCallToMenu) {
    /* TODO */
  }

  if (notification_box == nullptr) {
    notification_box = new NotificationBox(this);
    add_float(notification_box, 0, 0);
  }
  notification_box->show(message);
  layout();

  if (BIT_TEST(0x8f3fe, message.type)) {
    /* Move screen to new position */
    viewport->move_to_map_pos(message.pos);
    update_map_cursor_pos(message.pos);
  }

  msg_flags |= BIT(1);
  return_timeout = 60*TICKS_PER_SEC;
  play_sound(Audio::TypeSfxClick);
}

void
Interface::return_from_message() {
  if (BIT_TEST(msg_flags, 3)) { /* Return arrow present */
    msg_flags |= BIT(4);
    msg_flags &= ~BIT(3);

    return_timeout = 0;
    viewport->move_to_map_pos(return_pos);

    if ((popup != nullptr) && (popup->get_box() == PopupBox::TypeMessage)) {
      close_popup();
    }
    play_sound(Audio::TypeSfxClick);
  }
}

void
Interface::close_message() {
  if (notification_box == nullptr) {
    return;
  }

  notification_box->set_displayed(false);
  del_float(notification_box);
  delete notification_box;
  notification_box = nullptr;
  layout();
}

/* Return the cursor type and various related values of a MapPos. */
void
Interface::get_map_cursor_type(const Player *player_, MapPos pos,
                               BuildPossibility *bld_possibility,
                               CursorType *cursor_type) {
  PMap map = game->get_map();
  if (player_ == nullptr) {
    *bld_possibility = BuildPossibilityNone;
    *cursor_type = CursorTypeClear;
    return;
  }

  if (game->can_build_castle(pos, player_)) {
    *bld_possibility = BuildPossibilityCastle;
  } else if (game->can_player_build(pos, player_) &&
       Map::map_space_from_obj[map->get_obj(pos)] ==
         Map::SpaceOpen &&
       (game->can_build_flag(map->move_down_right(pos), player_) ||
         map->has_flag(map->move_down_right(pos)))) {
    if (game->can_build_mine(pos)) {
      *bld_possibility = BuildPossibilityMine;
    } else if (game->can_build_large(pos)) {
      *bld_possibility = BuildPossibilityLarge;
    } else if (game->can_build_small(pos)) {
      *bld_possibility = BuildPossibilitySmall;
    } else if (game->can_build_flag(pos, player_)) {
      *bld_possibility = BuildPossibilityFlag;
    } else {
      *bld_possibility = BuildPossibilityNone;
    }
  } else if (game->can_build_flag(pos, player_)) {
    *bld_possibility = BuildPossibilityFlag;
  } else {
    *bld_possibility = BuildPossibilityNone;
  }

  if (map->get_obj(pos) == Map::ObjectFlag &&
    map->get_owner(pos) == player_->get_index()) {
    if (game->can_demolish_flag(pos, player_)) {
      *cursor_type = CursorTypeRemovableFlag;
    } else {
      *cursor_type = CursorTypeFlag;
    }
  } else if (!map->has_building(pos) &&
             !map->has_flag(pos)) {
    int paths = map->paths(pos);
    if (paths == 0) {
      if (map->get_obj(map->move_down_right(pos)) ==
          Map::ObjectFlag) {
        *cursor_type = CursorTypeClearByFlag;
      } else if (map->paths(map->move_down_right(pos)) == 0) {
        *cursor_type = CursorTypeClear;
      } else {
        *cursor_type = CursorTypeClearByPath;
      }
    } else if (map->get_owner(pos) == player_->get_index()) {
      *cursor_type = CursorTypePath;
    } else {
      *cursor_type = CursorTypeNone;
    }
  } else if ((map->get_obj(pos) == Map::ObjectSmallBuilding ||
              map->get_obj(pos) == Map::ObjectLargeBuilding) &&
             map->get_owner(pos) == player_->get_index()) {
    Building *bld = game->get_building_at_pos(pos);
    if (!bld->is_burning()) {
      *cursor_type = CursorTypeBuilding;
    } else {
      *cursor_type = CursorTypeNone;
    }
  } else {
    *cursor_type = CursorTypeNone;
  }
}

/* Update the interface_t object with the information returned
   in get_map_cursor_type(). */
void
Interface::determine_map_cursor_type() {
  get_map_cursor_type(player, map_cursor_pos, &build_possibility,
                      &map_cursor_type);
}

/* Update the interface_t object with the information returned
   in get_map_cursor_type(). This is sets the appropriate values
   when the player interface is in road construction mode. */
void
Interface::determine_map_cursor_type_road() {
  PMap map = game->get_map();
  MapPos pos = map_cursor_pos;
  int h = map->get_height(pos);
  int valid_dir = 0;

  for (Direction d : cycle_directions_cw()) {
    int sprite = 0;

    if (building_road.is_undo(d)) {
      sprite = 45; /* undo */
      valid_dir |= BIT(d);
    } else if (map->is_road_segment_valid(pos, d)) {
      if (building_road.is_valid_extension(map.get(), d)) {
        int h_diff = map->get_height(map->move(pos, d)) - h;
        sprite = 39 + h_diff; /* height indicators */
        valid_dir |= BIT(d);
      } else {
        sprite = 44;
      }
    } else {
      sprite = 44; /* striped */
    }
    map_cursor_sprites[d+1].sprite = sprite;
  }

  building_road_valid_dir = valid_dir;
}

/* Set the appropriate sprites for the panel buttons and the map cursor. */
void
Interface::update_interface() {
  if (!building_road.is_valid()) {
    switch (map_cursor_type) {
    case CursorTypeNone:
      map_cursor_sprites[0].sprite = 32;
      map_cursor_sprites[2].sprite = 33;
      break;
    case CursorTypeFlag:
      map_cursor_sprites[0].sprite = 51;
      map_cursor_sprites[2].sprite = 33;
      break;
    case CursorTypeRemovableFlag:
      map_cursor_sprites[0].sprite = 51;
      map_cursor_sprites[2].sprite = 33;
      break;
    case CursorTypeBuilding:
      map_cursor_sprites[0].sprite = 32;
      map_cursor_sprites[2].sprite = 33;
      break;
    case CursorTypePath:
      map_cursor_sprites[0].sprite = 52;
      map_cursor_sprites[2].sprite = 33;
      if (build_possibility != BuildPossibilityNone) {
        map_cursor_sprites[0].sprite = 47;
      }
      break;
    case CursorTypeClearByFlag:
      if (build_possibility < BuildPossibilityMine) {
        map_cursor_sprites[0].sprite = 32;
        map_cursor_sprites[2].sprite = 33;
      } else {
        map_cursor_sprites[0].sprite = 46 + build_possibility;
        map_cursor_sprites[2].sprite = 33;
      }
      break;
    case CursorTypeClearByPath:
      if (build_possibility != BuildPossibilityNone) {
        map_cursor_sprites[0].sprite = 46 + build_possibility;
        if (build_possibility == BuildPossibilityFlag) {
          map_cursor_sprites[2].sprite = 33;
        } else {
          map_cursor_sprites[2].sprite = 47;
        }
      } else {
        map_cursor_sprites[0].sprite = 32;
        map_cursor_sprites[2].sprite = 33;
      }
      break;
    case CursorTypeClear:
      if (build_possibility) {
        if (build_possibility == BuildPossibilityCastle) {
          map_cursor_sprites[0].sprite = 50;
        } else {
          map_cursor_sprites[0].sprite = 46 + build_possibility;
        }
        if (build_possibility == BuildPossibilityFlag) {
          map_cursor_sprites[2].sprite = 33;
        } else {
          map_cursor_sprites[2].sprite = 47;
        }
      } else {
        map_cursor_sprites[0].sprite = 32;
        map_cursor_sprites[2].sprite = 33;
      }
      break;
    default:
      NOT_REACHED();
      break;
    }
  }

  if (panel != nullptr) {
    panel->update();
  }
}

void
Interface::set_game(PGame new_game) {
  if (viewport != nullptr) {
    del_float(viewport);
    delete viewport;
    viewport = nullptr;
  }

  game = std::move(new_game);

  if (game) {
    viewport = new Viewport(this, game->get_map());
    viewport->set_displayed(true);
    add_float(viewport, 0, 0);
  }

  layout();

  set_player(0);
}

void
Interface::set_player(unsigned int player_index) {
  if (panel != nullptr) {
    del_float(panel);
    delete panel;
    panel = nullptr;
  }

  if (game == nullptr) {
    return;
  }

  if ((player != nullptr) && (player_index == player->get_index())) {
    return;
  }

  player = game->get_player(player_index);

  /* Move viewport to initial position */
  MapPos init_pos = game->get_map()->pos(0, 0);

  if (player != NULL) {
    panel = new PanelBar(this);
    panel->set_displayed(true);
    add_float(panel, 0, 0);
    layout();

    for (Building *building : game->get_player_buildings(player)) {
      if (building->get_type() == Building::TypeCastle) {
        init_pos = building->get_position();
      }
    }
  }

  update_map_cursor_pos(init_pos);
  viewport->move_to_map_pos(map_cursor_pos);
}

Color
Interface::get_player_color(unsigned int player_index) {
  //Log::Debug["interface"] << " inside Interface::get_player_color with player_index " << player_index;
  Player::Color player_color = game->get_player(player_index)->get_color();
  Color color(player_color.red, player_color.green, player_color.blue);
  return color;
}

void
Interface::update_map_cursor_pos(MapPos pos) {
  map_cursor_pos = pos;
  if (building_road.is_valid()) {
    determine_map_cursor_type_road();
  }
  else {
    determine_map_cursor_type();
  }

  // with AI overlay on, mark any serf that is clicked on
  // this doesn't seem to work reliably at all
  //  also I think it is crashing if player doesn't have castle yet and AI grid is turned on?
  if (get_ai_ptr(player->get_index()) != nullptr) {
    Serf *serf = game->get_serf_at_pos(pos);
    if (serf == nullptr){
      Log::Debug["interface"] << "no serf found at clicked pos";
    }
    else {
      Log::Debug["interface"] << "evaluating clicked-on serf with index " << serf->get_index();
      std::vector<int> *iface_ai_mark_serf = get_ai_ptr(get_player()->get_index())->get_ai_mark_serf();
      if (serf->get_owner() == get_player()->get_index() && (serf->get_state() != Serf::StateIdleInStock)) {
        Log::Info["interface"] << "adding serf with index " << serf->get_index() << " to ai_mark_serf";
        iface_ai_mark_serf->push_back(serf->get_index());
      }
      else {
        Log::Debug["interface"] << "not marking serf with index " << serf->get_index();
        Log::Debug["interface"] << "not marking serf with index " << serf->get_index() << ", job " << serf->get_type() << ", state " << serf->get_state();
      }
    }
  }
  else {
    Log::Debug["interface"] << "this player is not an AI, not marking clicked-on serf";
  }
  update_interface();
}

/* Start road construction mode for player interface. */
void
Interface::build_road_begin() {
  determine_map_cursor_type();

  if (map_cursor_type != CursorTypeFlag &&
      map_cursor_type != CursorTypeRemovableFlag) {
    update_interface();
    return;
  }

  building_road.invalidate();
  building_road.start(map_cursor_pos);
  update_map_cursor_pos(map_cursor_pos);

  panel->update();
}

/* End road construction mode for player interface. */
void
Interface::build_road_end() {
  map_cursor_sprites[1].sprite = 33;
  map_cursor_sprites[2].sprite = 33;
  map_cursor_sprites[3].sprite = 33;
  map_cursor_sprites[4].sprite = 33;
  map_cursor_sprites[5].sprite = 33;
  map_cursor_sprites[6].sprite = 33;

  building_road.invalidate();
  update_map_cursor_pos(map_cursor_pos);

  panel->update();
}

/* Build a single road segment. Return -1 on fail, 0 on successful
   construction, and 1 if this segment completed the path. */
int
Interface::build_road_segment(Direction dir) {
  if (!building_road.is_extendable()) {
    /* Max length reached */
    return -1;
  }

  building_road.extend(dir);

  MapPos dest;
  int r = game->can_build_road(building_road, player, &dest, NULL);
  if (r <= 0) {
    /* Invalid construction, undo. */
    return remove_road_segment();
  }

  if (game->get_map()->get_obj(dest) == Map::ObjectFlag) {
    /* Existing flag at destination, try to connect. */
    if (!game->build_road(building_road, player)) {
      build_road_end();
      return -1;
    } else {
      build_road_end();
      update_map_cursor_pos(dest);
      return 1;
    }
  } else if (game->get_map()->paths(dest) == 0) {
    /* No existing paths at destination, build segment. */
    update_map_cursor_pos(dest);

    /* TODO Pathway scrolling */
  } else {
    /* TODO fast split path and connect on double click */
    return -1;
  }

  return 0;
}

int
Interface::remove_road_segment() {
  MapPos dest = building_road.get_source();
  int res = 0;
  building_road.undo();
  if (building_road.get_length() == 0 ||
      game->can_build_road(building_road, player, &dest, NULL) == 0) {
    /* Road construction is no longer valid, abort. */
    build_road_end();
    res = -1;
  }

  update_map_cursor_pos(dest);

  /* TODO Pathway scrolling */

  return res;
}

/* Extend currently constructed road with an array of directions. */
int
Interface::extend_road(const Road &road) {
  Road old_road = building_road;
  for (const Direction dir : road.get_dirs()) {
    int r = build_road_segment(dir);
    if (r < 0) {
      building_road = old_road;
      return -1;
    } else if (r == 1) {
      building_road.invalidate();
      return 1;
    }
  }

  return 0;
}

void
Interface::demolish_object() {
  determine_map_cursor_type();

  if (map_cursor_type == CursorTypeRemovableFlag) {
    play_sound(Audio::TypeSfxClick);
    game->demolish_flag(map_cursor_pos, player);
  } else if (map_cursor_type == CursorTypeBuilding) {
    Building *building = game->get_building_at_pos(map_cursor_pos);

    if (building->is_done() &&
        (building->get_type() == Building::TypeHut ||
         building->get_type() == Building::TypeTower ||
         building->get_type() == Building::TypeFortress)) {
      /* TODO */
    }

    play_sound(Audio::TypeSfxAhhh);
    game->demolish_building(map_cursor_pos, player);
  } else {
    play_sound(Audio::TypeSfxNotAccepted);
    update_interface();
  }
}

/* Build new flag. */
void
Interface::build_flag() {
  if (!game->build_flag(map_cursor_pos, player)) {
    play_sound(Audio::TypeSfxNotAccepted);
    return;
  }

  update_map_cursor_pos(map_cursor_pos);
}

/* Build a new building. */
void
Interface::build_building(Building::Type type) {
  if (!game->build_building(map_cursor_pos, type, player)) {
    play_sound(Audio::TypeSfxNotAccepted);
    return;
  }

  play_sound(Audio::TypeSfxAccepted);
  close_popup();

  /* Move cursor to flag. */
  MapPos flag_pos = game->get_map()->move_down_right(map_cursor_pos);
  update_map_cursor_pos(flag_pos);
}

/* Build castle. */
void
Interface::build_castle() {
  if (!game->build_castle(map_cursor_pos, player)) {
    play_sound(Audio::TypeSfxNotAccepted);
    return;
  }

  play_sound(Audio::TypeSfxAccepted);
  update_map_cursor_pos(map_cursor_pos);
}

void
Interface::build_road() {
  bool r = game->build_road(building_road, player);
  if (!r) {
    play_sound(Audio::TypeSfxNotAccepted);
    game->demolish_flag(map_cursor_pos, player);
  } else {
    play_sound(Audio::TypeSfxAccepted);
    build_road_end();
  }
}

void
Interface::update_map_height(MapPos pos, void *data) {
  Interface *interface = reinterpret_cast<Interface*>(data);
  interface->viewport->redraw_map_pos(pos);
}

void
Interface::internal_draw() {
}

void
Interface::layout() {
  int panel_x = 0;
  int panel_y = height;

  if (panel != nullptr) {
    int panel_width = 352;
    int panel_height = 40;
    panel_x = (width - panel_width) / 2;
    panel_y = height - panel_height;
    panel->move_to(panel_x, panel_y);
    panel->set_size(panel_width, panel_height);
  }

  if (popup != nullptr) {
    int popup_width = 144;
    int popup_height = 160;
    int popup_x = (width - popup_width) / 2;
    int popup_y = (height - popup_height) / 2;
    popup->move_to(popup_x, popup_y);
    popup->set_size(popup_width, popup_height);
  }

  if (init_box != nullptr) {
    int init_box_width = 360;
    int init_box_height = 256;
    int init_box_x = (width - init_box_width) / 2;
    int init_box_y = (height - init_box_height) / 2;
    init_box->move_to(init_box_x, init_box_y);
    init_box->set_size(init_box_width, init_box_height);
  }

  if (notification_box != nullptr) {
    int notification_box_width = 200;
    int notification_box_height = 88;
    int notification_box_x = panel_x + 40;
    int notification_box_y = panel_y - notification_box_height;
    notification_box->move_to(notification_box_x, notification_box_y);
    notification_box->set_size(notification_box_width, notification_box_height);
  }

  if (viewport != nullptr) {
    viewport->set_size(width, height);
  }

  set_redraw();
}

/* Called periodically when the game progresses. */
void
Interface::update() {
  if (!game) {
    return;
  }

  game->update();

  int tick_diff = game->get_const_tick() - last_const_tick;
  last_const_tick = game->get_const_tick();

  /* Clear return arrow after a timeout */
  if (return_timeout < tick_diff) {
    msg_flags |= BIT(4);
    msg_flags &= ~BIT(3);
    return_timeout = 0;
  } else {
    return_timeout -= tick_diff;
  }

  const int msg_category[] = {
    -1, 5, 5, 5, 4, 0, 4, 3, 4, 5,
    5, 5, 4, 4, 4, 4, 0, 0, 0, 0
  };

  /* Handle newly enqueued messages */
  if ((player != nullptr) && player->has_message()) {
    player->drop_message();
    while (player->has_notification()) {
      Message message = player->peek_notification();
      if (BIT_TEST(config, msg_category[message.type])) {
        play_sound(Audio::TypeSfxMessage);
        msg_flags |= BIT(0);
        break;
      }
      player->pop_notification();
    }
  }

  if ((player != nullptr) && BIT_TEST(msg_flags, 1)) {
    msg_flags &= ~BIT(1);
    while (1) {
      if (!player->has_notification()) {
        msg_flags &= ~BIT(0);
        break;
      }

      Message message = player->peek_notification();
      if (BIT_TEST(config, msg_category[message.type])) break;
      player->pop_notification();
    }
  }

  viewport->update();
  set_redraw();
}

bool
Interface::handle_key_pressed(char key, int modifier) {
  switch (key) {
    /* Interface control */
    case '\t': {
      if (modifier & 2) {
        return_from_message();
      } else {
        open_message();
      }
      break;
    }
    case 27: {
      if ((notification_box != nullptr) && notification_box->is_displayed()) {
        close_message();
      } else if ((popup != nullptr) && popup->is_displayed()) {
        close_popup();
      } else if (building_road.is_valid()) {
        build_road_end();
      }
      break;
    }

    /* Game speed */
    case '+': {
      game->speed_increase();
      break;
    }
    case '-': {
      game->speed_decrease();
      break;
    }
    case '0': {
      game->speed_reset();
      break;
    }
    case 'p': {
      game->pause();
      break;
    }

    /* Audio */
    case 's': {
      Audio &audio = Audio::get_instance();
      Audio::PPlayer splayer = audio.get_sound_player();
      if (splayer) {
        splayer->enable(!splayer->is_enabled());
      }
      break;
    }
    case 'm': {
      Audio &audio = Audio::get_instance();
      Audio::PPlayer splayer = audio.get_music_player();
      if (splayer) {
        splayer->enable(!splayer->is_enabled());
      }
      break;
    }

    /* Debug */
    case 'g': {
      viewport->switch_layer(Viewport::LayerGrid);
      break;
    }

    /* Game control */
    case 'b': {
      viewport->switch_layer(Viewport::LayerBuilds);
      break;
    }
    /* AI overlay grid - colored dots showing AI searching positions, roads being build, AI status, serf status, etc. */
  case 'y': {
    Log::Info["interface"] << "'y' key pressed, switching to LayerAI";
    viewport->switch_layer(Viewport::LayerAI);
    break;
  }
  /* Weather feature test.  */
  /* tlongstretch experimental 
  case 'w': {
    // you must also resize the screen to get it to change, probably need to add some refresh command
    Log::Info["interface"] << "'w' key pressed, enabling weather!";
    if (viewport->weather_enabled() == true) {
      Log::Info["interface"] << "'w' key pressed, disabling weather!";
      viewport->disable_weather();
    }
    else {
      Log::Info["interface"] << "'w' key pressed, enabling weather!";
      viewport->enable_weather(); 
    }
    //viewport->switch_layer(Viewport::LayerAI);
    break;
  }
  */

    case 'j': {
    unsigned int current_index = player->get_index();
    unsigned int next_index = game->get_next_player(player)->get_index();
    if (current_index == next_index) {
      Log::Debug["interface"] << "next player index is current player index - i.e. there is only one player, not switching";
      play_sound(Audio::TypeSfxNotAccepted);
    }
    else {
      set_player(next_index);
      Log::Debug["interface"] << "Switched to player #" << next_index;
    }
      break;
    }
    case 'z':
      if (modifier & 1) {
        GameStore::get_instance().quick_save("quicksave", game.get());
      }
      break;
    case 'n':
      if (modifier & 1) {
        open_game_init();
      }
      break;
    case 'c':
      if (modifier & 1) {
        open_popup(PopupBox::TypeQuitConfirm);
      }
      break;

    default:
      return false;
  }

  return true;
}

bool
Interface::handle_event(const Event *event) {
  switch (event->type) {
    case Event::TypeResize:
      set_size(event->dx, event->dy);
      break;
    case Event::TypeUpdate:
      update();
      break;
    case Event::TypeDraw:
      draw(reinterpret_cast<Frame*>(event->object));
      break;

    default:
      return GuiObject::handle_event(event);
      break;
  }

  return true;
}

void
Interface::on_new_game(PGame new_game) {
  set_game(new_game);
}

void
Interface::on_end_game(PGame /*game*/) {
  set_game(nullptr);
}
