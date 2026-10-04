/*
 * ai-want.h - Computer player: build wants per building type
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

#ifndef SRC_AI_WANT_H_
#define SRC_AI_WANT_H_

#include "src/ai-internal.h"

class AI::Want {
 public:
  /* 0x2b8c4..0x2cc5c: each writes player->ai.build_want[type - 1]. */
  static void want_type25(Player *player);
  static void want_fisher(Player *player);
  static void want_lumberjack(Player *player);
  static void want_boatbuilder(Player *player);
  static void want_stonecutter(Player *player);
  static void want_stonemine(Player *player);
  static void want_coalmine(Player *player);
  static void want_ironmine(Player *player);
  static void want_goldmine(Player *player);
  static void want_forester(Player *player);
  static void want_stock(Player *player);
  static void want_hut(Player *player);
  static void want_farm(Player *player);
  static void want_butcher(Player *player);
  static void want_pigfarm(Player *player);
  static void want_mill(Player *player);
  static void want_baker(Player *player);
  static void want_sawmill(Player *player);
  static void want_steelsmelter(Player *player);
  static void want_toolmaker(Player *player);
  static void want_weaponsmith(Player *player);
  static void want_tower(Player *player);
  static void want_fortress(Player *player);
  static void want_goldsmelter(Player *player);
  static void want_castle(Player *player);

 private:
  /* Word at Amiga pointer-relative offset off (0x33c..0x3ce) of the
     statistics area: poi[4] and u_394..u_3ce. */
  static int *stat(Player *player, int off);
  static unsigned int sadd(unsigned int a, unsigned int b);
  static unsigned int mulhi(unsigned int a, unsigned int b);
  static unsigned int min16(unsigned int a, unsigned int b);
  static unsigned int want_curve(unsigned int have, unsigned int need,
                                 int shift, int step, unsigned int base);
  static unsigned int want_share(unsigned int want, unsigned int s1,
                                 unsigned int s2, int *second);
  static unsigned int stone_needed(Player *player);
  static unsigned int miners_available(Player *player);
  static unsigned int incomplete_mines(Player *player);
  static unsigned int knights_available(Player *player);
  static unsigned int incomplete_military(Player *player);
  static unsigned int want_scale(AILocation *loc);       /* 0x2cc16 */
  static unsigned int want_scale_mine(AILocation *loc);  /* 0x2cc5c */
};

#endif  // SRC_AI_WANT_H_
