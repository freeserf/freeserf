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
#include "ezxml.h"

#if _MSC_VER
  #pragma warning(disable:4996)

  #define snprintf _snprintf
#endif


#define PLAYER_BUFFER_LENGTH 64

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

extern mission_t * mission;
extern int mission_count;

extern mission_t * training;
extern int training_count;

void init_mission(const char * mission_filename);
int init_mission_type(mission_t ** mission_dest, ezxml_t rootTag, const char * mission_tag_name);

void mission_cleanup();



#endif /* !_MISSION_H */
