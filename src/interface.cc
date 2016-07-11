/*
 * interface.cc - Top-level GUI interface
 *
 * Copyright (C) 2013-2014  Jon Lund Steffensen <jonlst@gmail.com>
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

#include <cstdlib>

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

viewport_t *
interface_t::get_viewport() {
  return viewport;
}

panel_bar_t *
interface_t::get_panel_bar() {
  return panel;
}

popup_box_t *
interface_t::get_popup_box() {
  return popup;
}

/* Open popup box */
void
interface_t::open_popup(int box) {
  if (popup == NULL) {
    popup = new popup_box_t(this);
    add_float(popup, 0, 0);
  }
  layout();
  popup->show((box_t)box);
  if (panel != NULL) {
    panel->update();
  }
}

/* Close the current popup. */
void
interface_t::close_popup() {
  if (popup == NULL) {
    return;
  }
  popup->hide();
  del_float(popup);
  delete popup;
  popup = NULL;
  update_map_cursor_pos(map_cursor_pos);
  panel->update();
}

/* Open box for starting a new game */
void
interface_t::open_game_init() {
  if (init_box == NULL) {
    init_box = new game_init_box_t(this);
    add_float(init_box, 0, 0);
  }
  init_box->set_displayed(true);
  init_box->set_enabled(true);
  if (panel != NULL) {
    panel->set_displayed(false);
  }
  viewport->set_enabled(false);
  layout();
}

void
interface_t::close_game_init() {
  if (init_box != NULL) {
    init_box->set_displayed(false);
    del_float(init_box);
    delete init_box;
    init_box = NULL;
  }
  if (panel != NULL) {
    panel->set_displayed(true);
    panel->set_enabled(true);
  }
  viewport->set_enabled(true);
  layout();

  update_map_cursor_pos(map_cursor_pos);
}

/* Open box for next message in the message queue */
void
interface_t::open_message() {
  if (!player->has_notification()) {
    play_sound(SFX_CLICK);
    return;
  } else if (!BIT_TEST(msg_flags, 3)) {
    msg_flags |= BIT(4);
    msg_flags |= BIT(3);
    map_pos_t pos = viewport->get_current_map_pos();
    return_pos = pos;
  }

  message_t message = player->pop_notification();

  if (message.type == NOTIFICATION_CALL_TO_MENU) {
    /* TODO */
  }

  if (notification_box == NULL) {
    notification_box = new notification_box_t(this);
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
  play_sound(SFX_CLICK);
}

void
interface_t::return_from_message() {
  if (BIT_TEST(msg_flags, 3)) { /* Return arrow present */
    msg_flags |= BIT(4);
    msg_flags &= ~BIT(3);

    return_timeout = 0;
    viewport->move_to_map_pos(return_pos);

    if ((popup != NULL) && (popup->get_box() == BOX_MESSAGE)) {
      close_popup();
    }
    play_sound(SFX_CLICK);
  }
}

void
interface_t::close_message() {
  if (notification_box == NULL) {
    return;
  }

  notification_box->set_displayed(false);
  del_float(notification_box);
  delete notification_box;
  notification_box = NULL;
  layout();
}

/* Return the cursor type and various related values of a map_pos_t. */
void
interface_t::get_map_cursor_type(const player_t *player, map_pos_t pos,
                                 build_possibility_t *bld_possibility,
                                 map_cursor_type_t *cursor_type) {
  if (player == NULL) {
    *bld_possibility = CAN_BUILD_NONE;
    *cursor_type = MAP_CURSOR_TYPE_CLEAR;
    return;
  }

  if (game->can_build_castle(pos, player)) {
    *bld_possibility = CAN_BUILD_CASTLE;
  } else if (game->can_player_build(pos, player) &&
       map_t::map_space_from_obj[game->get_map()->get_obj(pos)] ==
             MAP_SPACE_OPEN &&
       (game->can_build_flag(game->get_map()->move_down_right(pos), player) ||
        game->get_map()->has_flag(game->get_map()->move_down_right(pos)))) {
    if (game->can_build_mine(pos)) {
      *bld_possibility = CAN_BUILD_MINE;
    } else if (game->can_build_large(pos)) {
      *bld_possibility = CAN_BUILD_LARGE;
    } else if (game->can_build_small(pos)) {
      *bld_possibility = CAN_BUILD_SMALL;
    } else if (game->can_build_flag(pos, player)) {
      *bld_possibility = CAN_BUILD_FLAG;
    } else {
      *bld_possibility = CAN_BUILD_NONE;
    }
  } else if (game->can_build_flag(pos, player)) {
    *bld_possibility = CAN_BUILD_FLAG;
  } else {
    *bld_possibility = CAN_BUILD_NONE;
  }

  if (game->get_map()->get_obj(pos) == MAP_OBJ_FLAG &&
      game->get_map()->get_owner(pos) == player->get_index()) {
    if (game->can_demolish_flag(pos, player)) {
      *cursor_type = MAP_CURSOR_TYPE_REMOVABLE_FLAG;
    } else {
      *cursor_type = MAP_CURSOR_TYPE_FLAG;
    }
  } else if (!game->get_map()->has_building(pos) &&
             !game->get_map()->has_flag(pos)) {
    int paths = game->get_map()->paths(pos);
    if (paths == 0) {
      if (game->get_map()->get_obj(game->get_map()->move_down_right(pos)) ==
          MAP_OBJ_FLAG) {
        *cursor_type = MAP_CURSOR_TYPE_CLEAR_BY_FLAG;
      } else if (game->get_map()->paths(
                                  game->get_map()->move_down_right(pos)) == 0) {
        *cursor_type = MAP_CURSOR_TYPE_CLEAR;
      } else {
        *cursor_type = MAP_CURSOR_TYPE_CLEAR_BY_PATH;
      }
    } else if (game->get_map()->get_owner(pos) == player->get_index()) {
      *cursor_type = MAP_CURSOR_TYPE_PATH;
    } else {
      *cursor_type = MAP_CURSOR_TYPE_NONE;
    }
  } else if ((game->get_map()->get_obj(pos) == MAP_OBJ_SMALL_BUILDING ||
              game->get_map()->get_obj(pos) == MAP_OBJ_LARGE_BUILDING) &&
             game->get_map()->get_owner(pos) == player->get_index()) {
    building_t *bld = game->get_building_at_pos(pos);
    if (!bld->is_burning()) {
      *cursor_type = MAP_CURSOR_TYPE_BUILDING;
    } else {
      *cursor_type = MAP_CURSOR_TYPE_NONE;
    }
  } else {
    *cursor_type = MAP_CURSOR_TYPE_NONE;
  }
}

/* Update the interface_t object with the information returned
   in get_map_cursor_type(). */
void
interface_t::determine_map_cursor_type() {
  get_map_cursor_type(player, map_cursor_pos, &build_possibility,
                      &map_cursor_type);
}

/* Update the interface_t object with the information returned
   in get_map_cursor_type(). This is sets the appropriate values
   when the player interface is in road construction mode. */
void
interface_t::determine_map_cursor_type_road() {
  map_pos_t pos = map_cursor_pos;
  int h = game->get_map()->get_height(pos);
  int valid_dir = 0;

  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    int sprite = 0;

    if (building_road.is_undo((dir_t)d)) {
      sprite = 45; /* undo */
      valid_dir |= BIT(d);
    } else if (game->get_map()->is_road_segment_valid(pos, (dir_t)d)) {
      if (building_road.is_valid_extension(game->get_map(), (dir_t)d)) {
        int h_diff =
          game->get_map()->get_height(game->get_map()->move(pos, (dir_t)d)) - h;
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
interface_t::update_interface() {
  if (!building_road.is_valid()) {
    switch (map_cursor_type) {
    case MAP_CURSOR_TYPE_NONE:
      map_cursor_sprites[0].sprite = 32;
      map_cursor_sprites[2].sprite = 33;
      break;
    case MAP_CURSOR_TYPE_FLAG:
      map_cursor_sprites[0].sprite = 51;
      map_cursor_sprites[2].sprite = 33;
      break;
    case MAP_CURSOR_TYPE_REMOVABLE_FLAG:
      map_cursor_sprites[0].sprite = 51;
      map_cursor_sprites[2].sprite = 33;
      break;
    case MAP_CURSOR_TYPE_BUILDING:
      map_cursor_sprites[0].sprite = 32;
      map_cursor_sprites[2].sprite = 33;
      break;
    case MAP_CURSOR_TYPE_PATH:
      map_cursor_sprites[0].sprite = 52;
      map_cursor_sprites[2].sprite = 33;
      if (build_possibility != CAN_BUILD_NONE) {
        map_cursor_sprites[0].sprite = 47;
      }
      break;
    case MAP_CURSOR_TYPE_CLEAR_BY_FLAG:
      if (build_possibility < CAN_BUILD_MINE) {
        map_cursor_sprites[0].sprite = 32;
        map_cursor_sprites[2].sprite = 33;
      } else {
        map_cursor_sprites[0].sprite = 46 + build_possibility;
        map_cursor_sprites[2].sprite = 33;
      }
      break;
    case MAP_CURSOR_TYPE_CLEAR_BY_PATH:
      if (build_possibility != CAN_BUILD_NONE) {
        map_cursor_sprites[0].sprite = 46 + build_possibility;
        if (build_possibility == CAN_BUILD_FLAG) {
          map_cursor_sprites[2].sprite = 33;
        } else {
          map_cursor_sprites[2].sprite = 47;
        }
      } else {
        map_cursor_sprites[0].sprite = 32;
        map_cursor_sprites[2].sprite = 33;
      }
      break;
    case MAP_CURSOR_TYPE_CLEAR:
      if (build_possibility) {
        if (build_possibility == CAN_BUILD_CASTLE) {
          map_cursor_sprites[0].sprite = 50;
        } else {
          map_cursor_sprites[0].sprite = 46 + build_possibility;
        }
        if (build_possibility == CAN_BUILD_FLAG) {
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

  if (panel != NULL) {
    panel->update();
  }
}

void
interface_t::set_game(game_t *game) {
  if (viewport != NULL) {
    del_float(viewport);
    delete viewport;
    viewport = NULL;
  }

  this->game = game;
  player = NULL;

  if (game != NULL) {
    viewport = new viewport_t(this, game->get_map());
    viewport->set_displayed(true);
    add_float(viewport, 0, 0);
  }

  layout();
}

void
interface_t::set_player(unsigned int player) {
  if (game == NULL) {
    return;
  }

  if ((this->player != NULL) && (player == this->player->get_index())) {
    return;
  }

  if (panel != NULL) {
    del_float(panel);
    delete panel;
    panel = NULL;
  }

  this->player = game->get_player(player);

  /* Move viewport to initial position */
  map_pos_t init_pos = game->get_map()->pos(0, 0);

  if (this->player != NULL) {
    panel = new panel_bar_t(this);
    panel->set_displayed(true);
    add_float(panel, 0, 0);
    layout();

    list_buildings_t buildings = game->get_player_buildings(this->player);
    for (list_buildings_t::iterator it = buildings.begin();
         it != buildings.end(); ++it) {
      building_t *building = *it;
      if (building->get_type() == BUILDING_CASTLE) {
        init_pos = building->get_position();
      }
    }
  }

  update_map_cursor_pos(init_pos);
  viewport->move_to_map_pos(map_cursor_pos);
}

void
interface_t::update_map_cursor_pos(map_pos_t pos) {
  map_cursor_pos = pos;
  if (building_road.is_valid()) {
    determine_map_cursor_type_road();
  } else {
    determine_map_cursor_type();
  }
  update_interface();
}

/* Start road construction mode for player interface. */
void
interface_t::build_road_begin() {
  determine_map_cursor_type();

  if (map_cursor_type != MAP_CURSOR_TYPE_FLAG &&
      map_cursor_type != MAP_CURSOR_TYPE_REMOVABLE_FLAG) {
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
interface_t::build_road_end() {
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
interface_t::build_road_segment(dir_t dir) {
  if (!building_road.is_extandable()) {
    /* Max length reached */
    return -1;
  }

  building_road.extand(dir);

  map_pos_t dest;
  int r = game->can_build_road(building_road, player, &dest, NULL);
  if (!r) {
    /* Invalid construction, undo. */
    return remove_road_segment();
  }

  if (game->get_map()->get_obj(dest) == MAP_OBJ_FLAG) {
    /* Existing flag at destination, try to connect. */
    int r = game->build_road(building_road, player);
    if (r < 0) {
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
interface_t::remove_road_segment() {
  map_pos_t dest = building_road.get_source();
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
interface_t::extend_road(const road_t &road) {
  road_t old_road = building_road;
  road_t::dirs_t dirs = road.get_dirs();
  road_t::dirs_t::const_iterator it = dirs.begin();
  for (; it != dirs.end(); ++it) {
    dir_t dir = *it;
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
interface_t::demolish_object() {
  determine_map_cursor_type();

  if (map_cursor_type == MAP_CURSOR_TYPE_REMOVABLE_FLAG) {
    play_sound(SFX_CLICK);
    game->demolish_flag(map_cursor_pos, player);
  } else if (map_cursor_type == MAP_CURSOR_TYPE_BUILDING) {
    building_t *building = game->get_building_at_pos(map_cursor_pos);

    if (building->is_done() &&
        (building->get_type() == BUILDING_HUT ||
         building->get_type() == BUILDING_TOWER ||
         building->get_type() == BUILDING_FORTRESS)) {
      /* TODO */
    }

    play_sound(SFX_AHHH);
    game->demolish_building(map_cursor_pos, player);
  } else {
    play_sound(SFX_NOT_ACCEPTED);
    update_interface();
  }
}

/* Build new flag. */
void
interface_t::build_flag() {
  if (!game->build_flag(map_cursor_pos, player)) {
    play_sound(SFX_NOT_ACCEPTED);
    return;
  }

  update_map_cursor_pos(map_cursor_pos);
}

/* Build a new building. */
void
interface_t::build_building(building_type_t type) {
  if (!game->build_building(map_cursor_pos, type, player)) {
    play_sound(SFX_NOT_ACCEPTED);
    return;
  }

  play_sound(SFX_ACCEPTED);
  close_popup();

  /* Move cursor to flag. */
  map_pos_t flag_pos = game->get_map()->move_down_right(map_cursor_pos);
  update_map_cursor_pos(flag_pos);
}

/* Build castle. */
void
interface_t::build_castle() {
  if (!game->build_castle(map_cursor_pos, player)) {
    play_sound(SFX_NOT_ACCEPTED);
    return;
  }

  play_sound(SFX_ACCEPTED);
  update_map_cursor_pos(map_cursor_pos);
}

void
interface_t::build_road() {
  bool r = game->build_road(building_road, player);
  if (!r) {
    play_sound(SFX_NOT_ACCEPTED);
    game->demolish_flag(map_cursor_pos, player);
  } else {
    play_sound(SFX_ACCEPTED);
    build_road_end();
  }
}

void
interface_t::update_map_height(map_pos_t pos, void *data) {
  interface_t *interface = reinterpret_cast<interface_t*>(data);
  interface->viewport->redraw_map_pos(pos);
}

void
interface_t::internal_draw() {
}

void
interface_t::layout() {
  int panel_x = 0;
  int panel_y = height;

  if (panel != NULL) {
    int panel_width = 352;
    int panel_height = 40;
    panel_x = (width - panel_width) / 2;
    panel_y = height - panel_height;
    panel->move_to(panel_x, panel_y);
    panel->set_size(panel_width, panel_height);
  }

  if (popup != NULL) {
    int popup_width = 144;
    int popup_height = 160;
    int popup_x = (width - popup_width) / 2;
    int popup_y = (height - popup_height) / 2;
    popup->move_to(popup_x, popup_y);
    popup->set_size(popup_width, popup_height);
  }

  if (init_box != NULL) {
    int init_box_width = 360;
    int init_box_height = 256;
    int init_box_x = (width - init_box_width) / 2;
    int init_box_y = (height - init_box_height) / 2;
    init_box->move_to(init_box_x, init_box_y);
    init_box->set_size(init_box_width, init_box_height);
  }

  if (notification_box != NULL) {
    int notification_box_width = 200;
    int notification_box_height = 88;
    int notification_box_x = panel_x + 40;
    int notification_box_y = panel_y - notification_box_height;
    notification_box->move_to(notification_box_x, notification_box_y);
    notification_box->set_size(notification_box_width, notification_box_height);
  }

  if (viewport != NULL) {
    viewport->set_size(width, height);
  }

  set_redraw();
}

interface_t::interface_t() {
  displayed = true;

  game = NULL;

  map_cursor_pos = 0;
  map_cursor_type = (map_cursor_type_t)0;
  build_possibility = CAN_BUILD_NONE;

  player = NULL;

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

  viewport = NULL;
  panel = NULL;
  popup = NULL;
  init_box = NULL;
  notification_box = NULL;
}

interface_t::~interface_t() {
  if (viewport != NULL) {
    delete viewport;
  }
  if (panel != NULL) {
    delete panel;
  }
  if (popup != NULL) {
    delete popup;
  }
  if (init_box != NULL) {
    delete init_box;
  }
  if (notification_box != NULL) {
    delete notification_box;
  }
}

/* Called periodically when the game progresses. */
void
interface_t::update() {
  if (game == NULL) {
    return;
  }

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


  /* Handle newly enqueued messages */
  if ((player != NULL) && player->has_message()) {
    player->drop_message();
    while (player->has_notification()) {
      message_t message = player->peek_notification();
	  
      if (BIT_TEST(config, notification_box_t::notification_views[message.type].category)) {
        play_sound(SFX_MESSAGE);
        msg_flags |= BIT(0);
        break;
      }
      player->pop_notification();
    }
  }

  if ((player != NULL) && BIT_TEST(msg_flags, 1)) {
    msg_flags &= ~BIT(1);
    while (1) {
      if (!player->has_notification()) {
        msg_flags &= ~BIT(0);
        break;
      }
      message_t message = player->peek_notification();
      if (BIT_TEST(config, notification_box_t::notification_views[message.type].category)) break;
      player->pop_notification();
    }
  }

  viewport->update();

  // Handle newly box popup events
  if ((player != NULL) && player->has_popup()) {
	  open_popup(player->get_popup());
  }


  set_redraw();
}

bool
interface_t::handle_key_pressed(char key, int modifier) {
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
    case '\033': {
      if ((notification_box != NULL) && notification_box->is_displayed()) {
        close_message();
      } else if ((popup != NULL) && popup->is_displayed()) {
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
      audio_t *audio = audio_t::get_instance();
      audio_player_t *player = audio->get_sound_player();
      if (player != NULL) {
        player->enable(!player->is_enabled());
      }
      break;
    }
    case 'm': {
      audio_t *audio = audio_t::get_instance();
      audio_player_t *player = audio->get_music_player();
      if (player != NULL) {
        player->enable(!player->is_enabled());
      }
      break;
    }

    /* Debug */
    case 'g': {
      viewport->switch_layer(VIEWPORT_LAYER_GRID);
      break;
    }

    /* Game control */
    case 'b': {
      viewport->switch_layer(VIEWPORT_LAYER_BUILDS);
      break;
    }
    case 'j': {
      unsigned int index = game->get_next_player(player)->get_index();
      set_player(index);
      LOGD("main", "Switched to player %u.", index);
      break;
    }
    case 'z':
      if (modifier & 1) {
        save_game(0, game);
      }
      break;
    case 'n':
      if (modifier & 1) {
        open_game_init();
      }
      break;
    case 'c':
      if (modifier & 1) {
        open_popup(BOX_QUIT_CONFIRM);
      }
      break;

    default:
      return false;
  }

  return true;
}

bool
interface_t::handle_event(const event_t *event) {
  switch (event->type) {
    case EVENT_RESIZE:
      set_size(event->dx, event->dy);
      break;
    case EVENT_UPDATE:
      update();
      break;
    case EVENT_DRAW:
      draw(reinterpret_cast<frame_t*>(event->object));
      break;

    default:
      return gui_object_t::handle_event(event);
      break;
  }

  return true;
}
