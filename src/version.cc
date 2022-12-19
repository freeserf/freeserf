/*
 * version.cc - Version definition
 *
 * Copyright (C) 2012  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/version.h"
#include "src/version-vcs.h"

//const char FREESERF_VERSION[] = VERSION_VCS;
const char FORKSERF_VERSION[] = VERSION_VCS;

// I couldn't figure where else to put these externs without duplicate/redefinitions because of the rat's nest of header includes
#define DEFAULT_TICK_LENGTH  20
int tick_length = DEFAULT_TICK_LENGTH;
int game_ticks_per_update = DEFAULT_TICK_LENGTH;  
