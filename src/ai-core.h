/*
 * ai-core.h - Computer player: phases, castle placement, site scan
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

#ifndef SRC_AI_CORE_H_
#define SRC_AI_CORE_H_

#include "src/ai-internal.h"

class AI::Core {
 public:
  /* ai_update @0x28ee2: one AI step of the player. */
  static void update(Player *player);
  /* ai_update_build_damping_all @0xb094 (scheduler slot 32). */
  static void update_build_damping_all();
  /* determine_map_cursor_type @0x19368 for the AI cursor: sets
     ai.map_cursor_type, ai.panel_btn_type and the allow bits of
     player->build. */
  static void determine_map_cursor_type(Player *player);
  /* player_ai @0x2d162: random site scan. */
  static void scan_sites(Player *player);
  /* ai_build_building @0x2a5c4: build ai_game.build_building_type. */
  static void build_building(Player *player);
  static int check_construction_limit(Player *player);

 private:
  /* Result of the map cursor evaluation (determine_map_cursor_type). */
  typedef struct Cursor {
    int cursor_type;
    int possibility;
    int no_flag;
    int no_military;
    int military_known;
  } Cursor;

  static unsigned int sadd(unsigned int a, unsigned int b);
  static unsigned int ssub(unsigned int a, unsigned int b);
  static int *stat(Player *player, int off);
  static int *pending(Player *player, int i, int row);
  static unsigned int owner_bits(MapPos pos);
  static unsigned int type_byte(MapPos pos);
  static unsigned int own_bits(const Player *player);

  /* Map cursor of the original (0x1932e..0x19af8). */
  static unsigned int cursor_own(const Player *player);
  static int cursor_triangle_class(unsigned int type);
  static int cursor_military_near(MapPos pos);
  static void cursor_possible_building(const Player *player, MapPos pos,
                                       unsigned int own, Cursor *c);
  static void cursor_build_possibility(const Player *player, MapPos pos,
                                       unsigned int own, Cursor *c);
  static void cursor_clear(const Player *player, MapPos pos,
                           unsigned int own, Cursor *c);
  static void cursor_reset(Cursor *c);
  static void get_map_cursor(const Player *player, MapPos pos, Cursor *c);
  static void set_cursor_result(Player *player, const Cursor *c);
  static void get_map_cursor_type_at(Player *player, MapPos pos,
                                     unsigned int own);

  static void wait(Player *player);
  static AILocation *best_location(Player *player, int category);
  static void place_castle(Player *player);
  static void update_build_damping(Player *player, unsigned int elapsed);
  static void adjust_flags(Player *player);
  static unsigned int tool_value(unsigned int base, int shift,
                                 unsigned int stock);
  static void adjust_priorities(Player *player);
  static void update_settings(Player *player);
  static unsigned int stock_fill(const Building *b, int i);
  static void update_building_stats(Player *player);
  static void road_params(Player *player, int u_1ba, int u_19c, int u_1a8,
                          int u_1a4, int u_19e);
  static void move_cursor(Player *player, int dc, int dr);
  static void want_and_build(Player *player);
  static void find_flag_connection_at(Player *player, MapPos pos,
                                      int *budget);
  static int asr1(int v);
  static void connect_roads(Player *player);
};

#endif  // SRC_AI_CORE_H_
