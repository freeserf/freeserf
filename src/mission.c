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

mission_t mission[12] = {0};

void
init_missions()
{
	/* Mission 1: START */
	mission[0].rnd.state[0] = 0x6d6f;
	mission[0].rnd.state[1] = 0xf7f0;
	mission[0].rnd.state[2] = 0xc8d4;

	mission[0].player[0].supplies = 35,
	mission[0].player[0].reproduction = 30,
	mission[0].player[0].castle.col = -1;
	mission[0].player[0].castle.row = -1;

	mission[0].player[1].face = 1,
	mission[0].player[1].intelligence = 10,
	mission[0].player[1].supplies = 5,
	mission[0].player[1].reproduction = 30,
	mission[0].player[1].castle.col = -1;
	mission[0].player[1].castle.row = -1;

	/* Mission 2: STATION */
	mission[1].rnd.state[0] = 0x60b9;
	mission[1].rnd.state[1] = 0xe728;
	mission[1].rnd.state[2] = 0xc484;

	mission[1].player[0].supplies = 30,
	mission[1].player[0].reproduction = 40,
	mission[1].player[0].castle.col = -1;
	mission[1].player[0].castle.row = -1;

	mission[1].player[1].face = 2,
	mission[1].player[1].intelligence = 12,
	mission[1].player[1].supplies = 15,
	mission[1].player[1].reproduction = 30,
	mission[1].player[1].castle.col = -1;
	mission[1].player[1].castle.row = -1;

	mission[1].player[2].face = 3,
	mission[1].player[2].intelligence = 14,
	mission[1].player[2].supplies = 15,
	mission[1].player[2].reproduction = 30,
	mission[1].player[2].castle.col = -1;
	mission[1].player[2].castle.row = -1;

		/* Mission 3: UNITY */
	mission[2].rnd.state[0] = 0x12ab;
	mission[2].rnd.state[1] = 0x7a4a;
	mission[2].rnd.state[2] = 0xe483;

	mission[2].player[0].supplies = 30,
	mission[2].player[0].reproduction = 30,
	mission[2].player[0].castle.col = -1;
	mission[2].player[0].castle.row = -1;

	mission[2].player[1].face = 2,
	mission[2].player[1].intelligence = 18,
	mission[2].player[1].supplies = 10,
	mission[2].player[1].reproduction = 25,
	mission[2].player[1].castle.col = -1;
	mission[2].player[1].castle.row = -1;

	mission[2].player[2].face = 4,
	mission[2].player[2].intelligence = 18,
	mission[2].player[2].supplies = 10,
	mission[2].player[2].reproduction = 25,
	mission[2].player[2].castle.col = -1;
	mission[2].player[2].castle.row = -1;

	/* Mission 4 */
	mission[3].rnd.state[0] = 0xacdf;
	mission[3].rnd.state[1] = 0xee65;
	mission[3].rnd.state[2] = 0x3701;

	mission[3].player[0].supplies = 25,
	mission[3].player[0].reproduction = 40,
	mission[3].player[0].castle.col = -1;
	mission[3].player[0].castle.row = -1;

	mission[3].player[1].face = 2,
	mission[3].player[1].intelligence = 15,
	mission[3].player[1].supplies = 20,
	mission[3].player[1].reproduction = 30,
	mission[3].player[1].castle.col = -1;
	mission[3].player[1].castle.row = -1;

	/* Mission 5 */
	mission[4].rnd.state[0] = 0x3b8b;
	mission[4].rnd.state[1] = 0xd867;
	mission[4].rnd.state[2] = 0xd847;

	mission[4].player[0].supplies = 30,
	mission[4].player[0].reproduction = 30,
	mission[4].player[0].castle.col = -1;
	mission[4].player[0].castle.row = -1;

	mission[4].player[1].face = 3,
	mission[4].player[1].intelligence = 16,
	mission[4].player[1].supplies = 25,
	mission[4].player[1].reproduction = 20,
	mission[4].player[1].castle.col = -1;
	mission[4].player[1].castle.row = -1;

	mission[4].player[2].face = 4,
	mission[4].player[2].intelligence = 16,
	mission[4].player[2].supplies = 25,
	mission[4].player[2].reproduction = 20,
	mission[4].player[2].castle.col = -1;
	mission[4].player[2].castle.row = -1;

	/* Mission 6 */
	mission[5].rnd.state[0] = 0x4491;
	mission[5].rnd.state[1] = 0x36fb;
	mission[5].rnd.state[2] = 0xf9e1;

	mission[5].player[0].supplies = 30,
	mission[5].player[0].reproduction = 30,
	mission[5].player[0].castle.col = -1;
	mission[5].player[0].castle.row = -1;

	mission[5].player[1].face = 3,
	mission[5].player[1].intelligence = 20,
	mission[5].player[1].supplies = 12,
	mission[5].player[1].reproduction = 14,
	mission[5].player[1].castle.col = -1;
	mission[5].player[1].castle.row = -1;

	mission[5].player[2].face = 5,
	mission[5].player[2].intelligence = 20,
	mission[5].player[2].supplies = 12,
	mission[5].player[2].reproduction = 14,
	mission[5].player[2].castle.col = -1;
	mission[5].player[2].castle.row = -1;

	/* Mission 7 */
	mission[6].rnd.state[0] = 0xca18;
	mission[6].rnd.state[1] = 0x4221;
	mission[6].rnd.state[2] = 0x7f96;

	mission[6].player[0].supplies = 30,
	mission[6].player[0].reproduction = 40,
	mission[6].player[0].castle.col = -1;
	mission[6].player[0].castle.row = -1;

	mission[6].player[1].face = 3,
	mission[6].player[1].intelligence = 22,
	mission[6].player[1].supplies = 30,
	mission[6].player[1].reproduction = 30,
	mission[6].player[1].castle.col = -1;
	mission[6].player[1].castle.row = -1;

	/* Mission 8 */
	mission[7].rnd.state[0] = 0x88fe;
	mission[7].rnd.state[1] = 0xe0db;
	mission[7].rnd.state[2] = 0xed5c;

	mission[7].player[0].supplies = 25,
	mission[7].player[0].reproduction = 30,
	mission[7].player[0].castle.col = -1;
	mission[7].player[0].castle.row = -1;

	mission[7].player[1].face = 4,
	mission[7].player[1].intelligence = 23,
	mission[7].player[1].supplies = 25,
	mission[7].player[1].reproduction = 30,
	mission[7].player[1].castle.col = -1;
	mission[7].player[1].castle.row = -1;

	mission[7].player[2].face = 6,
	mission[7].player[2].intelligence = 24,
	mission[7].player[2].supplies = 25,
	mission[7].player[2].reproduction = 30,
	mission[7].player[2].castle.col = -1;
	mission[7].player[2].castle.row = -1;

	/* Mission 9 */
	mission[8].rnd.state[0] = 0xe9c4;
	mission[8].rnd.state[1] = 0x16fe;
	mission[8].rnd.state[2] = 0x2ef0;

	mission[8].player[0].supplies = 25,
	mission[8].player[0].reproduction = 40,
	mission[8].player[0].castle.col = -1;
	mission[8].player[0].castle.row = -1;

	mission[8].player[1].face = 4,
	mission[8].player[1].intelligence = 26,
	mission[8].player[1].supplies = 13,
	mission[8].player[1].reproduction = 30,
	mission[8].player[1].castle.col = -1;
	mission[8].player[1].castle.row = -1;

	mission[8].player[2].face = 5,
	mission[8].player[2].intelligence = 28,
	mission[8].player[2].supplies = 13,
	mission[8].player[2].reproduction = 30,
	mission[8].player[2].castle.col = -1;
	mission[8].player[2].castle.row = -1;

	mission[8].player[3].face = 6,
	mission[8].player[3].intelligence = 30,
	mission[8].player[3].supplies = 13,
	mission[8].player[3].reproduction = 30,
	mission[8].player[3].castle.col = -1;
	mission[8].player[3].castle.row = -1;

	/* Mission 10 */
	mission[9].rnd.state[0] = 0x15c2;
	mission[9].rnd.state[1] = 0xf9d0;
	mission[9].rnd.state[2] = 0x5fb1;

	mission[9].player[0].supplies = 20,
	mission[9].player[0].reproduction = 16,
	mission[9].player[0].castle.col = 28;
	mission[9].player[0].castle.row = 14;

	mission[9].player[1].face = 4,
	mission[9].player[1].intelligence = 30,
	mission[9].player[1].supplies = 19,
	mission[9].player[1].reproduction = 20,
	mission[9].player[1].castle.col = 5;
	mission[9].player[1].castle.row = 47;

	/* Mission 11 */
	mission[10].rnd.state[0] = 0x9b93;
	mission[10].rnd.state[1] = 0x6be1;
	mission[10].rnd.state[2] = 0x79c0;

	mission[10].player[0].supplies = 16,
	mission[10].player[0].reproduction = 20,
	mission[10].player[0].castle.col = 16;
	mission[10].player[0].castle.row = 42;

	mission[10].player[1].face = 5,
	mission[10].player[1].intelligence = 33,
	mission[10].player[1].supplies = 10,
	mission[10].player[1].reproduction = 20,
	mission[10].player[1].castle.col = 0x34;
	mission[10].player[1].castle.row = 0x19;

	mission[10].player[2].face = 7,
	mission[10].player[2].intelligence = 34,
	mission[10].player[2].supplies = 13,
	mission[10].player[2].reproduction = 20,
	mission[10].player[2].castle.col = 0x17;
	mission[10].player[2].castle.row = 12;

	/* Mission 12 */
	mission[11].rnd.state[0] = 0x4195;
	mission[11].rnd.state[1] = 0x7dba;
	mission[11].rnd.state[2] = 0xd884;

	mission[11].player[0].supplies = 23,
	mission[11].player[0].reproduction = 27,
	mission[11].player[0].castle.col = 0x35;
	mission[11].player[0].castle.row = 13;

	mission[11].player[1].face = 5,
	mission[11].player[1].intelligence = 27,
	mission[11].player[1].supplies = 17,
	mission[11].player[1].reproduction = 24,
	mission[11].player[1].castle.col = 0x1b;
	mission[11].player[1].castle.row = 10;

	mission[11].player[2].face = 6,
	mission[11].player[2].intelligence = 27,
	mission[11].player[2].supplies = 13,
	mission[11].player[2].reproduction = 24,
	mission[11].player[2].castle.col = 0x1d;
	mission[11].player[2].castle.row = 0x26;

	mission[11].player[3].face = 7,
	mission[11].player[3].intelligence = 27,
	mission[11].player[3].supplies = 13,
	mission[11].player[3].reproduction = 24,
	mission[11].player[3].castle.col = 15;
	mission[11].player[3].castle.row = 32;
}

const int mission_count = sizeof(mission) / sizeof(mission[0]);
