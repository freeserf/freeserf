/* building.c */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "building.h"


int
building_get_score_from_type(building_type_t type)
{
	const int building_score_from_type[] = {
		2, 2, 2, 2, 5, 5, 5, 5, 2, 10,
		3, 6, 4, 6, 5, 4, 7, 7, 9, 4,
		8, 15, 6, 20
	};

	return building_score_from_type[type-1];
}
