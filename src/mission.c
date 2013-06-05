/*
 * mission.c - Predefined game mission maps
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

#include "mission.h"
#include "random.h"

const mission_t mission[] = {
	{
		/* Mission 1: START */
		.rnd = {{ 0x6d6f, 0xf7f0, 0xc8d4 }},

		.player[0].supplies = 35,
		.player[0].reproduction = 30,

		.player[1].face = 1,
		.player[1].intelligence = 10,
		.player[1].supplies = 5,
		.player[1].reproduction = 30,
	}, {
		/* Mission 2: STATION */
		.rnd = {{ 0x60b9, 0xe728, 0xc484 }},

		.player[0].supplies = 30,
		.player[0].reproduction = 40,

		.player[1].face = 2,
		.player[1].intelligence = 12,
		.player[1].supplies = 15,
		.player[1].reproduction = 30,

		.player[2].face = 3,
		.player[2].intelligence = 14,
		.player[2].supplies = 15,
		.player[2].reproduction = 30
	}, {
		/* Mission 3: UNITY */
		.rnd = {{ 0x12ab, 0x7a4a, 0xe483 }},

		.player[0].supplies = 30,
		.player[0].reproduction = 30,

		.player[1].face = 2,
		.player[1].intelligence = 18,
		.player[1].supplies = 10,
		.player[1].reproduction = 25,

		.player[2].face = 4,
		.player[2].intelligence = 18,
		.player[2].supplies = 10,
		.player[2].reproduction = 25
	}, {
		/* Mission 4 */
		.rnd = {{ 0xacdf, 0xee65, 0x3701 }},

		.player[0].supplies = 25,
		.player[0].reproduction = 40,

		.player[1].face = 2,
		.player[1].intelligence = 15,
		.player[1].supplies = 20,
		.player[1].reproduction = 30,
	}, {
		/* Mission 5 */
		.rnd = {{ 0x3b8b, 0xd867, 0xd847 }},

		.player[0].supplies = 30,
		.player[0].reproduction = 30,

		.player[1].face = 3,
		.player[1].intelligence = 16,
		.player[1].supplies = 25,
		.player[1].reproduction = 20,

		.player[2].face = 4,
		.player[2].intelligence = 16,
		.player[2].supplies = 25,
		.player[2].reproduction = 20,
	}, {
		/* Mission 6 */
		.rnd = {{ 0x4491, 0x36fb, 0xf9e1 }},

		.player[0].supplies = 30,
		.player[0].reproduction = 30,

		.player[1].face = 3,
		.player[1].intelligence = 20,
		.player[1].supplies = 12,
		.player[1].reproduction = 14,

		.player[2].face = 5,
		.player[2].intelligence = 20,
		.player[2].supplies = 12,
		.player[2].reproduction = 14,
	}, {
		/* Mission 7 */
		.rnd = {{ 0xca18, 0x4221, 0x7f96 }},

		.player[0].supplies = 30,
		.player[0].reproduction = 40,

		.player[1].face = 3,
		.player[1].intelligence = 22,
		.player[1].supplies = 30,
		.player[1].reproduction = 30,
	}, {
		/* Mission 8 */
		.rnd = {{ 0x88fe, 0xe0db, 0xed5c }},

		.player[0].supplies = 25,
		.player[0].reproduction = 30,

		.player[1].face = 4,
		.player[1].intelligence = 23,
		.player[1].supplies = 25,
		.player[1].reproduction = 30,

		.player[2].face = 6,
		.player[2].intelligence = 24,
		.player[2].supplies = 25,
		.player[2].reproduction = 30,
	}, {
		/* Mission 9 */
		.rnd = {{ 0xe9c4, 0x16fe, 0x2ef0 }},

		.player[0].supplies = 25,
		.player[0].reproduction = 40,

		.player[1].face = 4,
		.player[1].intelligence = 26,
		.player[1].supplies = 13,
		.player[1].reproduction = 30,

		.player[2].face = 5,
		.player[2].intelligence = 28,
		.player[2].supplies = 13,
		.player[2].reproduction = 30,

		.player[3].face = 6,
		.player[3].intelligence = 30,
		.player[3].supplies = 13,
		.player[3].reproduction = 30,
	}, {
		/* Mission 10 */
		.rnd = {{ 0x15c2, 0xf9d0, 0x5fb1 }},

		.player[0].supplies = 20,
		.player[0].reproduction = 16,

		.player[1].face = 4,
		.player[1].intelligence = 30,
		.player[1].supplies = 19,
		.player[1].reproduction = 20,

		/* extra[0] = 0x0e1c */
		/* extra[1] = 0x2f05 */
	}, {
		/* Mission 11 */
		.rnd = {{ 0x9b93, 0x6be1, 0x79c0 }},

		.player[0].supplies = 16,
		.player[0].reproduction = 20,

		.player[1].face = 5,
		.player[1].intelligence = 33,
		.player[1].supplies = 10,
		.player[1].reproduction = 20,

		.player[2].face = 7,
		.player[2].intelligence = 34,
		.player[2].supplies = 13,
		.player[2].reproduction = 20,

		/* extra[0] = 0x2a10 */
		/* extra[1] = 0x1934 */
		/* extra[2] = 0x0c17 */
	}, {
		/* Mission 12 */
		.rnd = {{ 0x4195, 0x7dba, 0xd884 }},

		.player[0].supplies = 23,
		.player[0].reproduction = 27,

		.player[1].face = 5,
		.player[1].intelligence = 27,
		.player[1].supplies = 17,
		.player[1].reproduction = 24,

		.player[2].face = 6,
		.player[2].intelligence = 27,
		.player[2].supplies = 13,
		.player[2].reproduction = 24,

		.player[3].face = 7,
		.player[3].intelligence = 27,
		.player[3].supplies = 13,
		.player[3].reproduction = 24,

		/* extra[0] = 0x0d35 */
		/* extra[1] = 0x0a1b */
		/* extra[2] = 0x261d */
		/* extra[3] = 0x200f */
	}
};

const int mission_count = sizeof(mission) / sizeof(mission[0]);
