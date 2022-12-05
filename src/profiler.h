/*
 * profiler.h - Profiling tool.
 *
 * Copyright (C) 2018   Wicked_Digger <wicked_digger@mail.ru>
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

#ifndef SRC_PROFILER_H_
#define SRC_PROFILER_H_

// The length between game updates in miliseconds
//   this is controlled by SDL_Timer ever TICK_LENGTH which triggers and SDL Event
//   and is handled by the SDL_WaitEvent inside event_loop-sdl.cc EventLoopSDL::run()
//   which also handles other SDL Events such as user input immediately

// THIS IS NO LONGER SET HERE!!! profiler needs to be updated to match main game timer tick control
#define TICK_LENGTH  20
#define TICKS_PER_SEC  (1000/TICK_LENGTH)


#endif  // SRC_PROFILER_H_
