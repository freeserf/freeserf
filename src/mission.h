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

#ifndef _MISSION_H
#define _MISSION_H

#include "random.h"

typedef struct {
	random_state_t rnd;
	struct {
		int face;
		int supplies;
		int intelligence;
		int reproduction;
		struct { int col; int row; } castle;
	} player[4];
} mission_t;

extern mission_t mission[12];
extern const int mission_count;
void init_missions();

#endif /* !_MISSION_H */
