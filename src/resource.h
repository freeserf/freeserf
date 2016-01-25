/*
 * resource.h - Resources related definitions.
 *
 * Copyright (C) 2014  Wicked_Digger <wicked_digger@mail.ru>
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

#ifndef SRC_RESOURCE_H_
#define SRC_RESOURCE_H_

typedef enum {
  RESOURCE_NONE = -1,

  RESOURCE_FISH = 0,
  RESOURCE_PIG,
  RESOURCE_MEAT,
  RESOURCE_WHEAT,
  RESOURCE_FLOUR,
  RESOURCE_BREAD,
  RESOURCE_LUMBER,
  RESOURCE_PLANK,
  RESOURCE_BOAT,
  RESOURCE_STONE,
  RESOURCE_IRONORE,
  RESOURCE_STEEL,
  RESOURCE_COAL,
  RESOURCE_GOLDORE,
  RESOURCE_GOLDBAR,
  RESOURCE_SHOVEL,
  RESOURCE_HAMMER,
  RESOURCE_ROD,
  RESOURCE_CLEAVER,
  RESOURCE_SCYTHE,
  RESOURCE_AXE,
  RESOURCE_SAW,
  RESOURCE_PICK,
  RESOURCE_PINCER,
  RESOURCE_SWORD,
  RESOURCE_SHIELD,

  RESOURCE_GROUP_FOOD
} resource_type_t;


typedef struct {
  int player_num;
  int res_dir;
  /* Index of flag connected to this inventory */
  int flag;
  /* Index of building containing this inventory */
  int building;
  /* Count of resources */
  int resources[26];
  /* Resources waiting to be moved out */
  struct {
    resource_type_t type;
    unsigned int dest;
  } out_queue[2];
  /* Count of serfs waiting to move out */
  unsigned int serfs_out;
  /* Count of generic serfs */
  int generic_count;
  /* Indices to serfs of each type */
  int serfs[27];
} inventory_t;

#define INVENTORY_INDEX(ptr)  (static_cast<int>((ptr) - game.inventories))
#define INVENTORY_ALLOCATED(i)  \
                            BIT_TEST(game.inventory_bitmap[(i) >> 3], 7-((i)&7))

#endif  // SRC_RESOURCE_H_
