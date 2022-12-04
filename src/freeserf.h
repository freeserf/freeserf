/*
 * freeserf.h - Various definitions.
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

#ifndef SRC_FREESERF_H_
#define SRC_FREESERF_H_

// The length between game updates in miliseconds
//   this is controlled by SDL_Timer ever TICK_LENGTH which triggers and SDL Event
//   and is handled by the SDL_WaitEvent inside event_loop-sdl.cc EventLoopSDL::run()
//   which also handles other SDL Events such as user input immediately
#define TICK_LENGTH  20
#define TICKS_PER_SEC  (1000/TICK_LENGTH)
/* this file doesn't seem to be included by building.cc
    so I am moving it there, and also copying the TICKS_PER_SEC there
// adding support for requested resource timeouts
//  this number represents the number of seconds to allow for a requested
//   resource to travel one game tile at default game speed (2)
//   It needs to account for steepness and reasonable traffic
//  A quick test shows that it takes about nine seconds for a serf
//   to travel one tile up a road of the steepest category
#define TIMEOUT_SECS_PER_TILE    15
*/


#endif  // SRC_FREESERF_H_
