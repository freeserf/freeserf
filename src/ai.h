/*
 * ai.h - Computer player of the Amiga original
 *
 * Copyright (C) 2026  Wicked_Digger <wicked_digger@mail.ru>
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

/* The computer player of the Amiga original (0x28ee2..0x2edd6). The
   original passes most values through fields of the player record and a
   few global game fields instead of arguments; the port keeps that
   structure. Field names give the original's offsets, relative to the
   Amiga player pointer (0x80 bytes into the record, as the hex comments
   in player.h). All values are 16-bit words in the original unless noted
   otherwise. */

#ifndef SRC_AI_H_
#define SRC_AI_H_

#include <cstdint>

#include "src/map-geometry.h"

class Game;
class Player;

/* Statistics of the area around the cursor (ai_scan_points_of_interest
   @0x2e88e, 88 bytes). */
typedef struct AIPoi {
  int field_0;        /* +0x00 */
  int field_2;        /* +0x02 */
  int field_4;        /* +0x04 */
  int field_6;        /* +0x06 */
  int field_8;        /* +0x08 */
  int bld_count[25];  /* +0x0a buildings by type */
  int trees;          /* +0x3c */
  int field_3e;       /* +0x3e */
  int stones;         /* +0x40 */
  int deposit[4];     /* +0x42 gold, iron, coal, stone */
  int field_4a;       /* +0x4a */
  int field_4c;       /* +0x4c */
  int field_4e;       /* +0x4e */
  int field_50;       /* +0x50 */
  int field_52;       /* +0x52 */
  int field_54;       /* +0x54 */
  int field_56;       /* +0x56 */
} AIPoi;

typedef struct AILocation {
  int value;
  int col;
  int row;
} AILocation;

#define AI_LOCATION_CATEGORIES  35
#define AI_LOCATIONS_PER_CATEGORY  8

/* AI fields of the player record. */
typedef struct PlayerAI {
  /* Cursor of the computer player (ptr+0xfc..0x101, the same fields the
     human player's panel uses). */
  int cursor_col;       /* 0xfc */
  int cursor_row;       /* 0xfe */
  int map_cursor_type;  /* 0x100 (byte), Interface::CursorType */
  int panel_btn_type;   /* 0x101 (byte), Interface::BuildPossibility */
  /* Unknown words 0x19a..0x1ac (road builder parameters etc.). */
  int u_19a, u_19c, u_19e, u_1a0, u_1a2, u_1a4, u_1a6, u_1a8,
      u_1aa, u_1ac;
  int military_ratio;   /* 0x186: own vs. others' military strength */
  int u_1b0;            /* 0x1b0 */
  int u_1b2;            /* 0x1b2 */
  int phase;            /* 0x1b4: 0 place castle, 1 wait, 2 running, 3 off */
  int counter;          /* 0x1b6 */
  int build_threshold;  /* 0x1b8 */
  /* Unknown words 0x1ba..0x1da. */
  int u_1ba, u_1bc, u_1be, u_1c0, u_1c2, u_1c4, u_1c6, u_1c8,
      u_1ca, u_1cc, u_1ce, u_1d0, u_1d2, u_1d4, u_1d6, u_1d8,
      u_1da;
  AIPoi poi[5];         /* 0x1dc, 0x234, 0x28c, 0x2e4, 0x33c */
  /* Unknown words 0x394..0x3ce. */
  int u_394, u_396, u_398, u_39a, u_39c, u_39e, u_3a0, u_3a2,
      u_3a4, u_3a6, u_3a8, u_3aa, u_3ac, u_3ae, u_3b0, u_3b2,
      u_3b4, u_3b6, u_3b8, u_3ba, u_3bc, u_3be, u_3c0, u_3c2,
      u_3c4, u_3c6, u_3c8, u_3ca, u_3cc, u_3ce;
  int build_want[25];   /* 0x3d0: index = building type - 1 */
  int build_damp[25];   /* 0x402: index = building type - 1 */
  AILocation locations[AI_LOCATION_CATEGORIES][AI_LOCATIONS_PER_CATEGORY];
                        /* 0x434 */
} PlayerAI;

/* AI work fields of the game record. */
typedef struct GameAI {
  int g_246;            /* game+0x246 */
  int g_248;            /* game+0x248 */
  int g_24a;            /* game+0x24a (col of the road end / flag built) */
  int g_24c;            /* game+0x24c (row) */
  int g_24e;            /* game+0x24e */
  AILocation *some_location;  /* game+0x254 */
  int build_building_type;    /* game+0x27a */
  int ticks_288;        /* game+0x288, ticks for update_build_damping */
  /* game+0x12c land_influence, used by the AI as a scratch byte array
     (indexed like the Amiga byte offsets). */
  uint8_t land_influence[8192];
} GameAI;

/* The modules are nested classes, so that the friend declaration of AI
   in the game classes gives them access to the fields the original
   reads. */
class AI {
 public:
  class Core;
  class Rate;
  class Road;
  class Want;
  class Manage;

  /* Game of the current AI call; the original reads the game record
     from A5. */
  static Game *game;

  /* Scheduler slots (Amiga update_scheduled @0xa864). */
  static void update(Game *game, Player *player);      /* 33..48 */
  static void scan_sites(Game *game, Player *player);  /* 0..31 */
  static void update_build_damping_all(Game *game);    /* 32 */
  static int calc_military_ratio(Game *game, Player *player);

  /* Game hooks. */
  /* A knight of the player occupied the enemy building with the flag at
     flag_pos: remember it to connect it by road later (Amiga @0xd68c). */
  static void building_conquered(Game *game, Player *player,
                                 MapPos flag_pos);
  /* The mission gives the castle position: skip the castle placement
     phase (Amiga @0x4b36). */
  static void castle_given(Game *game, Player *player, MapPos pos);
};

#endif  // SRC_AI_H_
