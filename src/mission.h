/*
 * mission.h - Predefined game mission maps
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

#ifndef SRC_MISSION_H_
#define SRC_MISSION_H_

#include "src/random.h"

class mission_t {
 public:
  random_state_t rnd;
  struct {
    int face;
    int supplies;
    int intelligence;
    int reproduction;
    struct { int col; int row; } castle;
  } player[4];

  static mission_t mission[12];
  static void init_missions();
  static mission_t *get_mission(int mission);
  static int get_mission_count();
};

#endif  // SRC_MISSION_H_
