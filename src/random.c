/*
 * random.c - Random number generator
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "globals.h"


uint16_t
random_int()
{
	uint16_t r = (globals.rnd_1 + globals.rnd_2) ^ globals.rnd_3;
	globals.rnd_3 += globals.rnd_2;
	globals.rnd_2 ^= globals.rnd_3;
	globals.rnd_2 = (globals.rnd_2 >> 1) | (globals.rnd_2 << 15);
	globals.rnd_3 = (globals.rnd_3 >> 1) | (globals.rnd_3 << 15);
	globals.rnd_1 = r;

	return r;
}
