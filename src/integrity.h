/*
 * integrity.h - Consistency check of the game state
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

#ifndef SRC_INTEGRITY_H_
#define SRC_INTEGRITY_H_

class Game;

/* Checks that the links between the game objects agree with each other:
   map tiles and objects, building stocks and the resources on their way,
   flag slots and their schedule, serf references. Each problem is logged
   as a warning; with fix, the repairable ones are repaired. Returns the
   number of problems found. Run after loading a game. */
class IntegrityCheck {
 public:
  static unsigned int run(Game *game, bool fix);

 protected:
  static unsigned int check_map(Game *game);
  static unsigned int check_flags(Game *game, bool fix);
  static unsigned int check_requests(Game *game, bool fix);
  static unsigned int check_serfs(Game *game);
};

#endif  // SRC_INTEGRITY_H_
