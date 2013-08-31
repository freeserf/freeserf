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
#include "ezxml.h"

mission_t * mission = NULL;
int mission_count;

mission_t * training = NULL;
int training_count;

int ezxml_attr_int(ezxml_t xml, const char * attr, int defaultRet)
{
	const char * value = ezxml_attr(xml, attr);
	
	if (value == 0) return defaultRet;

	if (value[0] == '#')
	{
		return strtol(&value[1], NULL, 16);
	}
	else
	{
		return atoi(value);
	}
}

void init_mission(const char * mission_filename)
{
	ezxml_t rootTag = ezxml_parse_file(mission_filename);

	mission_count = init_mission_type(& mission, rootTag, "mission");
	training_count = init_mission_type(& training, rootTag, "training");

	ezxml_free(rootTag);
}


int init_mission_type(mission_t ** mission_dest, ezxml_t rootTag, const char * mission_tag_name)
{
	int count = 0;
	int i = 0;

	
	ezxml_t playerTag, missionTag;

	if (*mission_dest != NULL)
	{
		free(*mission_dest);
		*mission_dest = NULL;
	}

	//- count missions and create buffer
	for (missionTag = ezxml_child(rootTag, mission_tag_name); missionTag; missionTag = missionTag->next) count++;

	if (count == 0)
	{
		ezxml_free(rootTag);
		return 0;
	}

	*mission_dest = (mission_t *) calloc(count, sizeof(mission_t));
	mission_t * dest = *mission_dest;

	for (missionTag = ezxml_child(rootTag, mission_tag_name); missionTag; missionTag = missionTag->next)
	{

		dest[i].rnd.state[0] = ezxml_attr_int(missionTag, "rnd0", 0);
		dest[i].rnd.state[1] = ezxml_attr_int(missionTag, "rnd1", 0);
		dest[i].rnd.state[2] = ezxml_attr_int(missionTag, "rnd2", 0);

		for (int p = 0; p < 4; p++)
		{
			char PlayerBuffer[50];
			sprintf(PlayerBuffer, "player%d", p);

			playerTag = ezxml_child(missionTag, PlayerBuffer);
			if (playerTag)
			{
				dest[i].player[p].castle.col = ezxml_attr_int(playerTag, "castleCol", 0);
				dest[i].player[p].castle.row = ezxml_attr_int(playerTag, "castleRow", 0);

				dest[i].player[p].face = ezxml_attr_int(playerTag, "face", 0);
				dest[i].player[p].intelligence = ezxml_attr_int(playerTag, "intelligence", 0);
				dest[i].player[p].reproduction = ezxml_attr_int(playerTag, "reproduction", 0);
				dest[i].player[p].supplies = ezxml_attr_int(playerTag, "supplies", 0);
			}
			else
			{
				dest[i].player[p].castle.col = 0;
				dest[i].player[p].castle.row = 0;

				dest[i].player[p].face = 0;
				dest[i].player[p].intelligence = 0;
				dest[i].player[p].reproduction = 0;
				dest[i].player[p].supplies = 0;
			}
		}
		i++;
	}

	
	return count;
}


