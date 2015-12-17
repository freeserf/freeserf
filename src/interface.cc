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

#include <ctime>
#include <cassert>

#include "src/misc.h"
BEGIN_EXT_C
  #include "src/audio.h"
  #include "src/data.h"
  #include "src/debug.h"
END_EXT_C
#include "src/gfx.h"
#include "src/freeserf_endian.h"
#include "src/freeserf.h"

typedef struct {
  list_elm_t elm;
  gui_object_t *obj;
  int x, y;
  int redraw;
} interface_float_t;


viewport_t *
interface_t::get_top_viewport() {
  return &viewport;
}

panel_bar_t *
interface_t::get_panel_bar() {
  return &panel;
}

popup_box_t *
interface_t::get_popup_box() {
  return &popup;
}


/* Open popup box */
void
interface_t::open_popup(int box) {
  popup.show((box_t)box);
}

/* Close the current popup. */
void
interface_t::close_popup() {
  popup.hide();
  panel_btns[2] = PANEL_BTN_MAP;
  panel_btns[3] = PANEL_BTN_STATS;
  panel_btns[4] = PANEL_BTN_SETT;

  update_map_cursor_pos(map_cursor_pos);
}

/* Open box for starting a new game */
void
interface_t::open_game_init() {
  init_box.set_displayed(1);
  panel.set_enabled(0);
  viewport.set_enabled(0);
}

void
interface_t::close_game_init() {
  init_box.set_displayed(0);
  panel.set_enabled(1);
  viewport.set_enabled(1);

  update_map_cursor_pos(map_cursor_pos);
}

/* Open box for next message in the message queue */
void
interface_t::open_message() {
  if (player->msg_queue_type[0] == 0) {
    sfx_play_clip(SFX_CLICK);
    return;
  } else if (!BIT_TEST(msg_flags, 3)) {
    msg_flags |= BIT(4);
    msg_flags |= BIT(3);
    viewport_t *viewport = get_top_viewport();
    map_pos_t pos = viewport->get_current_map_pos();
    return_pos = pos;
  }

  int type = player->msg_queue_type[0] & 0x1f;

  if (type == 16) {
    /* TODO */
  }

  int param = (player->msg_queue_type[0] >> 5) & 7;
  notification_box.show(type, param);

  if (BIT_TEST(0x8f3fe, type)) {
    /* Move screen to new position */
    map_pos_t new_pos = player->msg_queue_pos[0];

    viewport_t *viewport = get_top_viewport();
    viewport->move_to_map_pos(new_pos);
    update_map_cursor_pos(new_pos);
  }

  /* Move notifications forward in the queue. */
  int i;
  for (i = 1; i < 64 && player->msg_queue_type[i] != 0; i++) {
    player->msg_queue_type[i-1] = player->msg_queue_type[i];
    player->msg_queue_pos[i-1] = player->msg_queue_pos[i];
  }
  player->msg_queue_type[i-1] = 0;

  msg_flags |= BIT(1);
  return_timeout = 60*TICKS_PER_SEC;
  sfx_play_clip(SFX_CLICK);
}

void
interface_t::return_from_message() {
  if (BIT_TEST(msg_flags, 3)) { /* Return arrow present */
    msg_flags |= BIT(4);
    msg_flags &= ~BIT(3);

    return_timeout = 0;
    viewport_t *viewport = get_top_viewport();
    viewport->move_to_map_pos(return_pos);

    if (popup.get_box() == BOX_MESSAGE) close_popup();
    sfx_play_clip(SFX_CLICK);
  }
}

void
interface_t::close_message() {
  notification_box.set_displayed(0);
}


/* Return the cursor type and various related values of a map_pos_t. */
void
interface_t::get_map_cursor_type(const player_t *player, map_pos_t pos,
                                 panel_btn_t *panel_btn,
                                 map_cursor_type_t *cursor_type) {
  if (game_can_build_castle(pos, player)) {
    *panel_btn = PANEL_BTN_BUILD_CASTLE;
  } else if (game_can_player_build(pos, player) &&
       map_space_from_obj[MAP_OBJ(pos)] == MAP_SPACE_OPEN &&
       (game_can_build_flag(MAP_MOVE_DOWN_RIGHT(pos), player) ||
        MAP_HAS_FLAG(MAP_MOVE_DOWN_RIGHT(pos)))) {
    if (game_can_build_mine(pos)) {
      *panel_btn = PANEL_BTN_BUILD_MINE;
    } else if (game_can_build_large(pos)) {
      *panel_btn = PANEL_BTN_BUILD_LARGE;
    } else if (game_can_build_small(pos)) {
      *panel_btn = PANEL_BTN_BUILD_SMALL;
    } else if (game_can_build_flag(pos, player)) {
      *panel_btn = PANEL_BTN_BUILD_FLAG;
    } else {
      *panel_btn = PANEL_BTN_BUILD_INACTIVE;
    }
  } else if (game_can_build_flag(pos, player)) {
    *panel_btn = PANEL_BTN_BUILD_FLAG;
  } else {
    *panel_btn = PANEL_BTN_BUILD_INACTIVE;
  }

  if (MAP_OBJ(pos) == MAP_OBJ_FLAG &&
      MAP_OWNER(pos) == player->player_num) {
    if (game_can_demolish_flag(pos, player)) {
      *cursor_type = MAP_CURSOR_TYPE_REMOVABLE_FLAG;
    } else {
      *cursor_type = MAP_CURSOR_TYPE_FLAG;
    }
  } else if (!MAP_HAS_BUILDING(pos) && !MAP_HAS_FLAG(pos)) {
    int paths = MAP_PATHS(pos);
    if (paths == 0) {
      if (MAP_OBJ(MAP_MOVE_DOWN_RIGHT(pos)) == MAP_OBJ_FLAG) {
        *cursor_type = MAP_CURSOR_TYPE_CLEAR_BY_FLAG;
      } else if (MAP_PATHS(MAP_MOVE_DOWN_RIGHT(pos)) == 0) {
        *cursor_type = MAP_CURSOR_TYPE_CLEAR;
      } else {
        *cursor_type = MAP_CURSOR_TYPE_CLEAR_BY_PATH;
      }
    } else if (MAP_OWNER(pos) == player->player_num) {
      *cursor_type = MAP_CURSOR_TYPE_PATH;
    } else {
      *cursor_type = MAP_CURSOR_TYPE_NONE;
    }
  } else if ((MAP_OBJ(pos) == MAP_OBJ_SMALL_BUILDING ||
              MAP_OBJ(pos) == MAP_OBJ_LARGE_BUILDING) &&
             MAP_OWNER(pos) == player->player_num) {
    building_t *bld = game_get_building(MAP_OBJ_INDEX(pos));
    if (!BUILDING_IS_BURNING(bld)) {
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
  map_pos_t cursor_pos = map_cursor_pos;
  get_map_cursor_type(player, cursor_pos, &panel_btn_type, &map_cursor_type);
}

/* Update the interface_t object with the information returned
   in get_map_cursor_type(). This is sets the appropriate values
   when the player interface is in road construction mode. */
void
interface_t::determine_map_cursor_type_road() {
  map_pos_t pos = map_cursor_pos;
  int h = MAP_HEIGHT(pos);
  int valid_dir = 0;
  int length = building_road_length;

  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    int sprite = 0;

    if ((length > 0) &&
        (building_road_dirs[length-1] == DIR_REVERSE(d))) {
      sprite = 45; /* undo */
      valid_dir |= BIT(d);
    } else if (game_road_segment_valid(pos, (dir_t)d)) {
      /* Check that road does not cross itself. */
      map_pos_t road_pos = building_road_source;
      int crossing_self = 0;
      for (int i = 0; i < length; i++) {
        road_pos = MAP_MOVE(road_pos, building_road_dirs[i]);
        if (road_pos == MAP_MOVE(pos, d)) {
          crossing_self = 1;
          break;
        }
      }

      if (!crossing_self) {
        int h_diff = MAP_HEIGHT(MAP_MOVE(pos, d)) - h;
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
  if (building_road) {
    panel_btns[0] = PANEL_BTN_BUILD_ROAD_STARRED;
    panel_btns[1] = PANEL_BTN_BUILD_INACTIVE;
  } else {
    switch (map_cursor_type) {
    case MAP_CURSOR_TYPE_NONE:
      panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
      if (PLAYER_HAS_CASTLE(player)) {
        panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
      } else {
        panel_btns[1] = PANEL_BTN_GROUND_ANALYSIS;
      }
      map_cursor_sprites[0].sprite = 32;
      map_cursor_sprites[2].sprite = 33;
      break;
    case MAP_CURSOR_TYPE_FLAG:
      panel_btns[0] = PANEL_BTN_BUILD_ROAD;
      panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
      map_cursor_sprites[0].sprite = 51;
      map_cursor_sprites[2].sprite = 33;
      break;
    case MAP_CURSOR_TYPE_REMOVABLE_FLAG:
      panel_btns[0] = PANEL_BTN_BUILD_ROAD;
      panel_btns[1] = PANEL_BTN_DESTROY;
      map_cursor_sprites[0].sprite = 51;
      map_cursor_sprites[2].sprite = 33;
      break;
    case MAP_CURSOR_TYPE_BUILDING:
      panel_btns[0] = panel_btn_type;
      panel_btns[1] = PANEL_BTN_DESTROY;
      map_cursor_sprites[0].sprite = 32;
      map_cursor_sprites[2].sprite = 33;
      break;
    case MAP_CURSOR_TYPE_PATH:
      panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
      panel_btns[1] = PANEL_BTN_DESTROY_ROAD;
      map_cursor_sprites[0].sprite = 52;
      map_cursor_sprites[2].sprite = 33;
      if (panel_btn_type != PANEL_BTN_BUILD_INACTIVE) {
        panel_btns[0] = PANEL_BTN_BUILD_FLAG;
        map_cursor_sprites[0].sprite = 47;
      }
      break;
    case MAP_CURSOR_TYPE_CLEAR_BY_FLAG:
      if (panel_btn_type < PANEL_BTN_BUILD_MINE) {
        panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
        if (PLAYER_HAS_CASTLE(player)) {
          panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
        } else {
          panel_btns[1] = PANEL_BTN_GROUND_ANALYSIS;
        }
        map_cursor_sprites[0].sprite = 32;
        map_cursor_sprites[2].sprite = 33;
      } else {
        panel_btns[0] = panel_btn_type;
        panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
        map_cursor_sprites[0].sprite = 46 + panel_btn_type;
        map_cursor_sprites[2].sprite = 33;
      }
      break;
    case MAP_CURSOR_TYPE_CLEAR_BY_PATH:
      panel_btns[0] = panel_btn_type;
      panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
      if (panel_btn_type != PANEL_BTN_BUILD_INACTIVE) {
        map_cursor_sprites[0].sprite = 46 + panel_btn_type;
        if (panel_btn_type == PANEL_BTN_BUILD_FLAG) {
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
      panel_btns[0] = panel_btn_type;
      if (PLAYER_HAS_CASTLE(player)) {
        panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
      } else {
        panel_btns[1] = PANEL_BTN_GROUND_ANALYSIS;
      }
      if (panel_btn_type) {
        if (panel_btn_type == PANEL_BTN_BUILD_CASTLE) {
          map_cursor_sprites[0].sprite = 50;
        } else {
          map_cursor_sprites[0].sprite = 46 + panel_btn_type;
        }
        if (panel_btn_type == PANEL_BTN_BUILD_FLAG) {
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
}

void
interface_t::set_player(uint player) {
  assert(PLAYER_IS_ACTIVE(game.player[player]));
  this->player = game.player[player];

  /* Move viewport to initial position */
  map_pos_t init_pos = MAP_POS(0, 0);
  if (this->player->castle_flag != 0) {
    flag_t *flag = game_get_flag(this->player->castle_flag);
    init_pos = MAP_MOVE_UP_LEFT(flag->pos);
  }

  update_map_cursor_pos(init_pos);
  viewport.move_to_map_pos(map_cursor_pos);
}

void
interface_t::update_map_cursor_pos(map_pos_t pos) {
  map_cursor_pos = pos;
  if (building_road) {
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

  panel_btns[0] = PANEL_BTN_BUILD_ROAD_STARRED;
  panel_btns[1] = PANEL_BTN_BUILD_INACTIVE;
  panel_btns[2] = PANEL_BTN_MAP_INACTIVE;
  panel_btns[3] = PANEL_BTN_STATS_INACTIVE;
  panel_btns[4] = PANEL_BTN_SETT_INACTIVE;

  building_road = 1;
  building_road_length = 0;
  building_road_source = map_cursor_pos;

  update_map_cursor_pos(map_cursor_pos);
}

/* End road construction mode for player interface. */
void
interface_t::build_road_end() {
  panel_btns[2] = PANEL_BTN_MAP;
  panel_btns[3] = PANEL_BTN_STATS;
  panel_btns[4] = PANEL_BTN_SETT;

  map_cursor_sprites[1].sprite = 33;
  map_cursor_sprites[2].sprite = 33;
  map_cursor_sprites[3].sprite = 33;
  map_cursor_sprites[4].sprite = 33;
  map_cursor_sprites[5].sprite = 33;
  map_cursor_sprites[6].sprite = 33;

  building_road = 0;
  update_map_cursor_pos(map_cursor_pos);
}

/* Build a single road segment. Return -1 on fail, 0 on successful
   construction, and 1 if this segment completed the path. */
int
interface_t::build_road_segment(dir_t dir) {
  if (building_road_length+1 >= MAX_ROAD_LENGTH) {
    /* Max length reached */
    return -1;
  }

  building_road_dirs[building_road_length] = dir;
  building_road_length += 1;

  map_pos_t dest;
  int r = game_can_build_road(building_road_source, building_road_dirs,
                              building_road_length, player, &dest, NULL);
  if (!r) {
    /* Invalid construction, undo. */
    return remove_road_segment();
  }

  if (MAP_OBJ(dest) == MAP_OBJ_FLAG) {
    /* Existing flag at destination, try to connect. */
    int r = game_build_road(building_road_source, building_road_dirs,
                            building_road_length, player);
    if (r < 0) {
      build_road_end();
      return -1;
    } else {
      build_road_end();
      update_map_cursor_pos(dest);
      return 1;
    }
  } else if (MAP_PATHS(dest) == 0) {
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
  building_road_length -= 1;

  map_pos_t dest;
  int r = game_can_build_road(building_road_source, building_road_dirs,
                              building_road_length, player, &dest, NULL);
  if (!r) {
    /* Road construction is no longer valid, abort. */
    build_road_end();
    return -1;
  }

  update_map_cursor_pos(dest);

  /* TODO Pathway scrolling */

  return 0;
}

/* Extend currently constructed road with an array of directions. */
int
interface_t::extend_road(dir_t *dirs, uint length) {
  for (uint i = 0; i < length; i++) {
    dir_t dir = dirs[i];
    int r = build_road_segment(dir);
    if (r < 0) {
      /* Backtrack */
      for (int j = i-1; j >= 0; j--) {
        remove_road_segment();
      }
      return -1;
    } else if (r == 1) {
      return 1;
    }
  }

  return 0;
}

void
interface_t::demolish_object() {
  determine_map_cursor_type();

  map_pos_t cursor_pos = map_cursor_pos;

  if (map_cursor_type == MAP_CURSOR_TYPE_REMOVABLE_FLAG) {
    sfx_play_clip(SFX_CLICK);
    game_demolish_flag(cursor_pos, player);
  } else if (map_cursor_type == MAP_CURSOR_TYPE_BUILDING) {
    building_t *building = game_get_building(MAP_OBJ_INDEX(cursor_pos));

    if (BUILDING_IS_DONE(building) &&
        (BUILDING_TYPE(building) == BUILDING_HUT ||
         BUILDING_TYPE(building) == BUILDING_TOWER ||
         BUILDING_TYPE(building) == BUILDING_FORTRESS)) {
      /* TODO */
    }

    sfx_play_clip(SFX_AHHH);
    game_demolish_building(cursor_pos, player);
  } else {
    sfx_play_clip(SFX_NOT_ACCEPTED);
    update_interface();
  }
}

/* Build new flag. */
void
interface_t::build_flag() {
  int r = game_build_flag(map_cursor_pos, player);
  if (r < 0) {
    sfx_play_clip(SFX_NOT_ACCEPTED);
    return;
  }

  update_map_cursor_pos(map_cursor_pos);
}

/* Build a new building. */
void
interface_t::build_building(building_type_t type) {
  int r = game_build_building(map_cursor_pos, type, player);
  if (r < 0) {
    sfx_play_clip(SFX_NOT_ACCEPTED);
    return;
  }

  sfx_play_clip(SFX_ACCEPTED);
  close_popup();

  /* Move cursor to flag. */
  map_pos_t flag_pos = MAP_MOVE_DOWN_RIGHT(map_cursor_pos);
  update_map_cursor_pos(flag_pos);
}

/* Build castle. */
void
interface_t::build_castle() {
  int r = game_build_castle(map_cursor_pos, player);
  if (r < 0) {
    sfx_play_clip(SFX_NOT_ACCEPTED);
    return;
  }

  sfx_play_clip(SFX_ACCEPTED);
  update_map_cursor_pos(map_cursor_pos);
}

void
interface_t::build_road() {
  int r = game_build_road(building_road_source,
                          building_road_dirs,
                          building_road_length,
                          player);
  if (r < 0) {
    sfx_play_clip(SFX_NOT_ACCEPTED);
    game_demolish_flag(map_cursor_pos, player);
  } else {
    sfx_play_clip(SFX_ACCEPTED);
    build_road_end();
  }
}


void
interface_t::update_map_height(map_pos_t pos, void *data) {
  interface_t *interface = reinterpret_cast<interface_t*>(data);
  interface->viewport.redraw_map_pos(pos);
}

void
interface_t::draw(frame_t *frame) {
  int redraw_above = need_redraw;

  if (top->get_displayed() &&
      (redraw_top || redraw_above)) {
    top->redraw(frame);
    redraw_top = 0;
    redraw_above = 1;
  }

  list_elm_t *elm;
  list_foreach(&floats, elm) {
    interface_float_t *fl = reinterpret_cast<interface_float_t*>(elm);
    if (fl->obj->get_displayed() && (fl->redraw || redraw_above)) {
      frame_t float_frame;
      gfx_frame_init(&float_frame,
                     frame->clip.x + fl->x,
                     frame->clip.y + fl->y,
                     fl->obj->get_width(), fl->obj->get_height(), frame);
      fl->obj->redraw(&float_frame);
      fl->redraw = 0;
      redraw_above = 1;
    }
  }
}

int
interface_t::handle_event(const gui_event_t *event) {
  /* Handle locked cursor */
  if (cursor_lock_target != NULL) {
    if (cursor_lock_target == top) {
      return top->handle_event(event);
    } else {
      if (event->type == GUI_EVENT_TYPE_DRAG_MOVE) {
        return cursor_lock_target->handle_event(event);
      }
      gui_event_t float_event;
      float_event.type = event->type;
      float_event.x = event->x;
      float_event.y = event->y;
      float_event.button = event->button;
      gui_object_t *obj = cursor_lock_target;
      while (obj->get_parent() != NULL) {
        int x, y;
        int r = obj->get_parent()->get_child_position(obj, &x, &y);
        if (r < 0) return -1;

        float_event.x -= x;
        float_event.y -= y;

        obj = obj->get_parent();
      }

      if (obj != this) return -1;
      return cursor_lock_target->handle_event(&float_event);
    }
  }

  /* Find the corresponding float element if any */
  list_elm_t *elm;
  list_foreach_reverse(&floats, elm) {
    interface_float_t *fl = reinterpret_cast<interface_float_t*>(elm);
    if (fl->obj->get_displayed() &&
        event->x >= fl->x && event->y >= fl->y &&
        event->x < fl->x + fl->obj->get_width() &&
        event->y < fl->y + fl->obj->get_height()) {
      gui_event_t float_event;
      float_event.type = event->type;
      float_event.x = event->x - fl->x;
      float_event.y = event->y - fl->y;
      float_event.button = event->button;
      return fl->obj->handle_event(&float_event);
    }
  }

  return top->handle_event(event);
}

void
interface_t::set_size(int width, int height) {
  this->width = width;
  this->height = height;

  int panel_width = 352;
  int panel_height = 40;
  int panel_x = (width - panel_width) / 2;
  int panel_y = height - panel_height;

  int popup_width = 144;
  int popup_height = 160;
  int popup_x = (width - popup_width) / 2;
  int popup_y = (height - popup_height) / 2;

  int init_box_width = 360;
  int init_box_height = 174;
  int init_box_x = (width - init_box_width) / 2;
  int init_box_y = (height - init_box_height) / 2;

  int notification_box_width = 200;
  int notification_box_height = 88;
  int notification_box_x = panel_x + 40;
  int notification_box_y = panel_y - notification_box_height;

  top->set_size(width, height);
  redraw_top = 1;

  /* Reassign position of floats. */
  list_elm_t *elm;
  list_foreach(&floats, elm) {
    interface_float_t *fl = reinterpret_cast<interface_float_t*>(elm);
    if (fl->obj == &popup) {
      fl->x = popup_x;
      fl->y = popup_y;
      fl->redraw = 1;
      fl->obj->set_size(popup_width, popup_height);
    } else if (fl->obj == &panel) {
      fl->x = panel_x;
      fl->y = panel_y;
      fl->redraw = 1;
      fl->obj->set_size(panel_width, panel_height);
    } else if (fl->obj == &init_box) {
      fl->x = init_box_x;
      fl->y = init_box_y;
      fl->redraw = 1;
      fl->obj->set_size(init_box_width, init_box_height);
    } else if (fl->obj == &notification_box) {
      fl->x = notification_box_x;
      fl->y = notification_box_y;
      fl->redraw = 1;
      fl->obj->set_size(notification_box_width, notification_box_height);
    }
  }

  set_redraw();
}

void
interface_t::set_redraw_child(gui_object_t *child) {
  if (parent != NULL) {
    parent->set_redraw_child(this);
  }

  if (top == child) {
    redraw_top = 1;
    return;
  }

  list_elm_t *elm;
  list_foreach(&floats, elm) {
    interface_float_t *fl = reinterpret_cast<interface_float_t*>(elm);
    if (fl->obj == child) {
      fl->redraw = 1;
      break;
    }
  }
}

int
interface_t::get_child_position(gui_object_t *child, int *x, int *y) {
  if (top == child) {
    *x = 0;
    *y = 0;
    return 0;
  }

  list_elm_t *elm;
  list_foreach(&floats, elm) {
    interface_float_t *fl = reinterpret_cast<interface_float_t*>(elm);
    if (fl->obj == child) {
      *x = fl->x;
      *y = fl->y;
      return 0;
    }
  }

  return -1;
}

void
interface_t::load_serf_animation_table() {
  /* The serf animation table is stored in big endian
     order in the data file.

     * The first uint32 is the byte length of the rest
     of the table (skipped below).
     * Next is 199 uint32s that are offsets from the start
     of this table to an animation table (one for each
     animation).
     * The animation tables are of varying lengths.
     Each entry in the animation table is three bytes
     long. First byte is used to determine the serf body
     sprite. Second byte is a signed horizontal sprite
     offset. Third byte is a signed vertical offset.
  */
  serf_animation_table =
    (reinterpret_cast<uint32_t*>(data_get_object(DATA_SERF_ANIMATION_TABLE,
                                                 NULL))) + 1;

  /* Endianess convert from big endian. */
  for (int i = 0; i < 199; i++) {
    serf_animation_table[i] = be32toh(serf_animation_table[i]);
  }
}

interface_t::interface_t()
  : panel(this)
  , popup(this)
  , viewport(this)
  , notification_box(this)
  , init_box(this) {
  top = NULL;
  redraw_top = 0;
  list_init(&floats);

  load_serf_animation_table();

  /* Viewport */
  viewport.set_displayed(1);

  /* Panel bar */
  panel.set_displayed(1);

  /* Add objects to interface container. */
  set_top(&viewport);

  add_float(&popup, 0, 0, 0, 0);
  add_float(&panel, 0, 0, 0, 0);

  /* Game init box */
  init_box.set_displayed(1);
  add_float(&init_box, 0, 0, 0, 0);

  /* Notification box */
  add_float(&notification_box, 0, 0, 0, 0);

  map_cursor_pos = MAP_POS(0, 0);
  map_cursor_type = (map_cursor_type_t)0;
  panel_btn_type = (panel_btn_t)0;

  building_road = 0;

  player = NULL;

  /* Settings */
  config = 0x39;
  msg_flags = 0;
  return_timeout = 0;

  panel_btns[0] = PANEL_BTN_BUILD_INACTIVE;
  panel_btns[1] = PANEL_BTN_DESTROY_INACTIVE;
  panel_btns[2] = PANEL_BTN_MAP;
  panel_btns[3] = PANEL_BTN_STATS;
  panel_btns[4] = PANEL_BTN_SETT;

  current_stat_8_mode = 0;
  current_stat_7_item = 7;

  map_cursor_sprites[0].sprite = 32;
  map_cursor_sprites[1].sprite = 33;
  map_cursor_sprites[2].sprite = 33;
  map_cursor_sprites[3].sprite = 33;
  map_cursor_sprites[4].sprite = 33;
  map_cursor_sprites[5].sprite = 33;
  map_cursor_sprites[6].sprite = 33;

  /* Randomness for interface */
  srand((uint)time(NULL));
  random.state[0] = rand();
  random.state[1] = rand();
  random.state[2] = rand();
  random_int(&random);

  last_const_tick = 0;

  /* Listen for updates to the map height */
  game.update_map_height_cb =
    reinterpret_cast<game_update_map_height_func*>(update_map_height);
  game.update_map_height_data = this;
}

void
interface_t::set_top(gui_object_t *obj) {
  top = obj;
  obj->set_parent(this);
  top->set_size(width, height);
  redraw_top = 1;
  set_redraw();
}

void
interface_t::add_float(gui_object_t *obj, int x, int y, int width, int height) {
  interface_float_t *fl =
    reinterpret_cast<interface_float_t*>(malloc(sizeof(interface_float_t)));
  if (fl == NULL) abort();

  /* Store currect location with object. */
  fl->obj = obj;
  fl->x = x;
  fl->y = y;
  fl->redraw = 1;

  obj->set_parent(this);
  list_append(&floats, reinterpret_cast<list_elm_t*>(fl));
  obj->set_size(width, height);
  set_redraw();
}

/* Called periodically when the game progresses. */
void
interface_t::update() {
  int tick_diff = game.const_tick - last_const_tick;
  last_const_tick = game.const_tick;

  /* Update timers */
  for (int i = 0; i < player->timers_count; i++) {
    player->timers[i].timeout -= tick_diff;
    if (player->timers[i].timeout < 0) {
      /* Timer has expired. */
      /* TODO box (+ pos) timer */
      player_add_notification(player, 5,
            player->timers[i].pos);

      /* Delete timer from list. */
      player->timers_count -= 1;
      for (int j = i; j < player->timers_count; j++) {
        player->timers[j].timeout = player->timers[j+1].timeout;
        player->timers[j].pos = player->timers[j+1].pos;
      }
    }
  }

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
  if (PLAYER_HAS_MESSAGE(player)) {
    player->flags &= ~BIT(3);
    while (player->msg_queue_type[0] != 0) {
      int type = player->msg_queue_type[0] & 0x1f;
      if (BIT_TEST(config, msg_category[type])) {
        sfx_play_clip(SFX_MESSAGE);
        msg_flags |= BIT(0);
        break;
      }

      /* Message is ignored. Remove. */
      int i;
      for (i = 1; i < 64 && player->msg_queue_type[i] != 0; i++) {
        player->msg_queue_type[i-1] = player->msg_queue_type[i];
        player->msg_queue_pos[i-1] = player->msg_queue_pos[i];
      }
      player->msg_queue_type[i-1] = 0;
    }
  }

  if (BIT_TEST(msg_flags, 1)) {
    msg_flags &= ~BIT(1);
    while (1) {
      if (player->msg_queue_type[0] == 0) {
        msg_flags &= ~BIT(0);
        break;
      }

      int type = player->msg_queue_type[0] & 0x1f;
      if (BIT_TEST(config, msg_category[type])) break;

      /* Message is ignored. Remove. */
      int i;
      for (i = 1; i < 64 && player->msg_queue_type[i] != 0; i++) {
        player->msg_queue_type[i-1] = player->msg_queue_type[i];
        player->msg_queue_pos[i-1] = player->msg_queue_pos[i];
      }
      player->msg_queue_type[i-1] = 0;
    }
  }

  viewport.update();
}
