/* random.c */

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
