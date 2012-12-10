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
	uint16_t *rnd = globals.rnd.state;
	uint16_t r = (rnd[0] + rnd[1]) ^ rnd[2];
	rnd[2] += rnd[1];
	rnd[1] ^= rnd[2];
	rnd[1] = (rnd[1] >> 1) | (rnd[1] << 15);
	rnd[2] = (rnd[2] >> 1) | (rnd[2] << 15);
	rnd[0] = r;

	return r;
}
