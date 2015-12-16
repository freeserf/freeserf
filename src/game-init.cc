/*
 * game-init.cc - Game initialization GUI component
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

#include "src/game-init.h"

#include <ctime>
#include <cstdio>
#include <algorithm>

#include "src/interface.h"
BEGIN_EXT_C
  #include "src/mission.h"
  #include "src/data.h"
END_EXT_C

#ifdef min
# undef min
#endif

#ifdef max
# undef max
#endif

typedef enum {
  ACTION_START_GAME,
  ACTION_TOGGLE_GAME_TYPE,
  ACTION_SHOW_OPTIONS,
  ACTION_SHOW_LOAD_GAME,
  ACTION_INCREMENT,
  ACTION_DECREMENT,
  ACTION_CLOSE
} action_t;


static void
draw_box_icon(int x, int y, int sprite, frame_t *frame) {
  gfx_draw_sprite(8*x+20, y+16, DATA_ICON_BASE + sprite, frame);
}

static void
draw_box_string(int x, int y, frame_t *frame, const char *str) {
  gfx_draw_string(8*x+20, y+16, 31, 1, frame, str);
}

/* Get the sprite number for a face. */
static int
get_player_face_sprite(int face) {
  if (face != 0) return 0x10b + face;
  return 0x119; /* sprite_face_none */
}

static void
game_init_box_draw(game_init_box_t *box, frame_t *frame) {
  /* Background */
  gfx_fill_rect(0, 0, box->obj.width, box->obj.height, 1, frame);

  const int layout[] = {
    251, 0, 40, 252, 0, 112, 253, 0, 48,
    254, 5, 48, 255, 9, 48,
    251, 10, 40, 252, 10, 112, 253, 10, 48,
    254, 15, 48, 255, 19, 48,
    251, 20, 40, 252, 20, 112, 253, 20, 48,
    254, 25, 48, 255, 29, 48,
    251, 30, 40, 252, 30, 112, 253, 30, 48,
    254, 35, 48, 255, 39, 48,

    266, 0, 0, 267, 31, 0, 316, 36, 0,
    -1
  };

  const int *i = layout;
  while (i[0] >= 0) {
    draw_box_icon(i[1], i[2], i[0], frame);
    i += 3;
  }

  /* Game type settings */
  if (box->game_mission < 0) {
    draw_box_icon(5, 0, 263, frame);

    char map_size[4] = {0};
    snprintf(map_size, sizeof(map_size), "%d", box->map_size);

    draw_box_string(10, 0, frame, "Start new game");
    draw_box_string(10, 14, frame, "Map size:");
    draw_box_string(20, 14, frame, map_size);
  } else {
    draw_box_icon(5, 0, 260, frame);

    char level[4] = {0};
    snprintf(level, sizeof(level), "%d", box->game_mission+1);

    draw_box_string(10, 0, frame, "Start mission");
    draw_box_string(10, 14, frame, "Mission:");
    draw_box_string(20, 14, frame, level);
  }

  draw_box_icon(28, 0, 237, frame);
  draw_box_icon(28, 16, 240, frame);

  /* Game info */
  if (box->game_mission < 0) {
    for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
      int face = box->face[i];
      draw_box_icon(10*i+1, 48, get_player_face_sprite(face), frame);
      draw_box_icon(10*i+6, 48, 282, frame);

      if (face == 0) continue;

      int intelligence = box->intelligence[i];
      gfx_fill_rect(80*i+78, 124-intelligence, 4, intelligence, 30, frame);

      int supplies = box->supplies[i];
      gfx_fill_rect(80*i+72, 124-supplies, 4, supplies, 67, frame);

      int reproduction = box->reproduction[i];
      gfx_fill_rect(80*i+84, 124-reproduction, 4, reproduction, 75, frame);
    }
  } else {
    int m = box->game_mission;
    for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
      int face = i == 0 ? 12 : mission[m].player[i].face;
      draw_box_icon(10*i+1, 48, get_player_face_sprite(face), frame);
      draw_box_icon(10*i+6, 48, 282, frame);

      int intelligence = i == 0 ? 40 : mission[m].player[i].intelligence;
      gfx_fill_rect(80*i+78, 124-intelligence, 4, intelligence, 30, frame);

      int supplies = mission[m].player[i].supplies;
      gfx_fill_rect(80*i+72, 124-supplies, 4, supplies, 67, frame);

      int reproduction = mission[m].player[i].reproduction;
      gfx_fill_rect(80*i+84, 124-reproduction, 4, reproduction, 75, frame);
    }
  }

  draw_box_icon(38, 128, 60, frame); /* exit */
}

static void
handle_action(game_init_box_t *box, int action) {
  const uint default_player_colors[] = {
    64, 72, 68, 76
  };

  switch (action) {
  case ACTION_START_GAME:
    game_init();
    if (box->game_mission < 0) {
      random_state_t rnd = {{ 0x5a5a,
                              (uint16_t)(time(NULL) >> 16),
                              (uint16_t)time(NULL) }};
      int r = game_load_random_map(box->map_size, &rnd);
      if (r < 0) return;

      for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
        if (box->face[i] == 0) continue;
        int p = game_add_player(box->face[i],
              default_player_colors[i],
              box->supplies[i],
              box->reproduction[i],
              box->intelligence[i]);
        if (p < 0) return;
      }
    } else {
      int r = game_load_mission_map(box->game_mission);
      if (r < 0) return;
    }

    viewport_map_reinit();
    interface_set_player(box->interface, 0);
    interface_close_game_init(box->interface);
    break;
  case ACTION_TOGGLE_GAME_TYPE:
    if (box->game_mission < 0) {
      box->game_mission = 0;
    } else {
      box->game_mission = -1;
      box->map_size = 3;
    }
    break;
  case ACTION_SHOW_OPTIONS:
    break;
  case ACTION_SHOW_LOAD_GAME:
    break;
  case ACTION_INCREMENT:
    if (box->game_mission < 0) {
      box->map_size = std::min(box->map_size+1, 10);
    } else {
      box->game_mission = std::min(box->game_mission+1, mission_count-1);
    }
    break;
  case ACTION_DECREMENT:
    if (box->game_mission < 0) {
      box->map_size = std::max(3, box->map_size-1);
    } else {
      box->game_mission = std::max(0, box->game_mission-1);
    }
    break;
  case ACTION_CLOSE:
    interface_close_game_init(box->interface);
    break;
  default:
    break;
  }
}

static int
game_init_box_handle_event_click(game_init_box_t *box, int x, int y) {
  const int clickmap[] = {
    ACTION_START_GAME, 20, 16, 32, 32,
    ACTION_TOGGLE_GAME_TYPE, 60, 16, 32, 32,
    ACTION_SHOW_OPTIONS, 268, 16, 32, 32,
    ACTION_SHOW_LOAD_GAME, 308, 16, 32, 32,
    ACTION_INCREMENT, 244, 16, 16, 16,
    ACTION_DECREMENT, 244, 32, 16, 16,
    ACTION_CLOSE, 324, 144, 16, 16,
    -1
  };

  const int *i = clickmap;
  while (i[0] >= 0) {
    if (x >= i[1] && x < i[1]+i[3] &&
        y >= i[2] && y < i[2]+i[4]) {
      handle_action(box, i[0]);
      break;
    }
    i += 5;
  }

  /* Check player area */
  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    if (x >= 80*i+20 && x <= 80*(i+1)+20 && y >= 48 && y <= 132) {
      x -= 80*i + 20;

      if (x >= 8 && x < 8+32 && y >= 48 && y < 132) {
        /* Face */
        int in_use = 0;
        do {
          box->face[i] = (box->face[i] + 1) % 14;

          /* Check that face is not already in use
             by another player */
          in_use = 0;
          for (int j = 0; j < GAME_MAX_PLAYER_COUNT; j++) {
            if (i != j &&
                box->face[i] != 0 &&
                box->face[j] == box->face[i]) {
              in_use = 1;
              break;
            }
          }
        } while (in_use);
      } else if (x >= 48 && x < 72 && y >= 64 && y < 80) {
        /* Controller */
        /* TODO */
      } else if (x >= 52 && x < 52+4 && y >= 84 && y < 124) {
        /* Supplies */
        box->supplies[i] = clamp(0, 124 - y, 40);
      } else if (x >= 58 && x < 58+4 && y >= 84 && y < 124) {
        /* Intelligence */
        box->intelligence[i] = clamp(0, 124 - y, 40);
      } else if (x >= 64 && x < 64+4 && y >= 84 && y < 124) {
        /* Reproduction */
        box->reproduction[i] = clamp(0, 124 - y, 40);
      }
      break;
    }
  }

  return 0;
}

static int
game_init_box_handle_event(game_init_box_t *box, const gui_event_t *event) {
  switch (event->type) {
  case GUI_EVENT_TYPE_CLICK:
    if (event->button == GUI_EVENT_BUTTON_LEFT) {
      return game_init_box_handle_event_click(box, event->x, event->y);
    }
  default:
    break;
  }

  return 0;
}

void
game_init_box_init(game_init_box_t *box, interface_t *interface) {
  gui_object_init(reinterpret_cast<gui_object_t*>(box));
  box->obj.draw = reinterpret_cast<gui_draw_func*>(game_init_box_draw);
  box->obj.handle_event =
    reinterpret_cast<gui_handle_event_func*>(game_init_box_handle_event);

  box->interface = interface;
  box->map_size = 3;
  box->game_mission = -1;

  /* Clear player settings */
  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    box->face[i] = 0;
    box->intelligence[i] = 0;
    box->supplies[i] = 0;
    box->reproduction[i] = 0;
  }

  /* Create default game setup */
  box->face[0] = 12;
  box->intelligence[0] = 40;
  box->supplies[0] = 40;
  box->reproduction[0] = 40;

  box->face[1] = 1;
  box->intelligence[1] = 20;
  box->supplies[1] = 30;
  box->reproduction[1] = 40;
}
