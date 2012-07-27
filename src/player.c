/* player.c */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "freeserf.h"
#include "player.h"


/* Set defaults for food distribution priorities. */
void
player_sett_reset_food_priority(player_sett_t *sett)
{
	sett->food_stonemine = 13100;
	sett->food_coalmine = 45850;
	sett->food_ironmine = 45850;
	sett->food_goldmine = 65500;
}

/* Set defaults for planks distribution priorities. */
void
player_sett_reset_planks_priority(player_sett_t *sett)
{
	sett->planks_construction = 65500;
	sett->planks_boatbuilder = 3275;
	sett->planks_toolmaker = 19650;
}

/* Set defaults for steel distribution priorities. */
void
player_sett_reset_steel_priority(player_sett_t *sett)
{
	sett->steel_toolmaker = 45850;
	sett->steel_weaponsmith = 65500;
}

/* Set defaults for coal distribution priorities. */
void
player_sett_reset_coal_priority(player_sett_t *sett)
{
	sett->coal_steelsmelter = 32750;
	sett->coal_goldsmelter = 65500;
	sett->coal_weaponsmith = 52400;
}

/* Set defaults for coal distribution priorities. */
void
player_sett_reset_wheat_priority(player_sett_t *sett)
{
	sett->wheat_pigfarm = 65500;
	sett->wheat_mill = 32750;
}

/* Set defaults for tool production priorities. */
void
player_sett_reset_tool_priority(player_sett_t *sett)
{
	sett->tool_prio[0] = 9825; /* SHOVEL */
	sett->tool_prio[1] = 65500; /* HAMMER */
	sett->tool_prio[2] = 13100; /* ROD */
	sett->tool_prio[3] = 6550; /* CLEAVER */
	sett->tool_prio[4] = 13100; /* SCYTHE */
	sett->tool_prio[5] = 26200; /* AXE */
	sett->tool_prio[6] = 32750; /* SAW */
	sett->tool_prio[7] = 45850; /* PICK */
	sett->tool_prio[8] = 6550; /* PINCER */
}

/* Set defaults for flag priorities. */
void
player_sett_reset_flag_priority(player_sett_t *sett)
{
	sett->flag_prio[RESOURCE_GOLDORE] = 1;
	sett->flag_prio[RESOURCE_GOLDBAR] = 2;
	sett->flag_prio[RESOURCE_WHEAT] = 3;
	sett->flag_prio[RESOURCE_FLOUR] = 4;
	sett->flag_prio[RESOURCE_PIG] = 5;

	sett->flag_prio[RESOURCE_BOAT] = 6;
	sett->flag_prio[RESOURCE_PINCER] = 7;
	sett->flag_prio[RESOURCE_SCYTHE] = 8;
	sett->flag_prio[RESOURCE_ROD] = 9;
	sett->flag_prio[RESOURCE_CLEAVER] = 10;

	sett->flag_prio[RESOURCE_SAW] = 11;
	sett->flag_prio[RESOURCE_AXE] = 12;
	sett->flag_prio[RESOURCE_PICK] = 13;
	sett->flag_prio[RESOURCE_SHOVEL] = 14;
	sett->flag_prio[RESOURCE_HAMMER] = 15;

	sett->flag_prio[RESOURCE_SHIELD] = 16;
	sett->flag_prio[RESOURCE_SWORD] = 17;
	sett->flag_prio[RESOURCE_BREAD] = 18;
	sett->flag_prio[RESOURCE_MEAT] = 19;
	sett->flag_prio[RESOURCE_FISH] = 20;

	sett->flag_prio[RESOURCE_IRONORE] = 21;
	sett->flag_prio[RESOURCE_LUMBER] = 22;
	sett->flag_prio[RESOURCE_COAL] = 23;
	sett->flag_prio[RESOURCE_STEEL] = 24;
	sett->flag_prio[RESOURCE_STONE] = 25;
	sett->flag_prio[RESOURCE_PLANK] = 26;
}

/* Set defaults for inventory priorities. */
void
player_sett_reset_inventory_priority(player_sett_t *sett)
{
	sett->inventory_prio[RESOURCE_WHEAT] = 1;
	sett->inventory_prio[RESOURCE_FLOUR] = 2;
	sett->inventory_prio[RESOURCE_PIG] = 3;
	sett->inventory_prio[RESOURCE_BREAD] = 4;
	sett->inventory_prio[RESOURCE_FISH] = 5;

	sett->inventory_prio[RESOURCE_MEAT] = 6;
	sett->inventory_prio[RESOURCE_LUMBER] = 7;
	sett->inventory_prio[RESOURCE_PLANK] = 8;
	sett->inventory_prio[RESOURCE_BOAT] = 9;
	sett->inventory_prio[RESOURCE_STONE] = 10;

	sett->inventory_prio[RESOURCE_COAL] = 11;
	sett->inventory_prio[RESOURCE_IRONORE] = 12;
	sett->inventory_prio[RESOURCE_STEEL] = 13;
	sett->inventory_prio[RESOURCE_SHOVEL] = 14;
	sett->inventory_prio[RESOURCE_HAMMER] = 15;

	sett->inventory_prio[RESOURCE_ROD] = 16;
	sett->inventory_prio[RESOURCE_CLEAVER] = 17;
	sett->inventory_prio[RESOURCE_SCYTHE] = 18;
	sett->inventory_prio[RESOURCE_AXE] = 19;
	sett->inventory_prio[RESOURCE_SAW] = 20;

	sett->inventory_prio[RESOURCE_PICK] = 21;
	sett->inventory_prio[RESOURCE_PINCER] = 22;
	sett->inventory_prio[RESOURCE_SHIELD] = 23;
	sett->inventory_prio[RESOURCE_SWORD] = 24;
	sett->inventory_prio[RESOURCE_GOLDORE] = 25;
	sett->inventory_prio[RESOURCE_GOLDBAR] = 26;
}
