/*
 * mission.h - Predefined game mission maps
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

#ifndef SRC_MISSION_H_
#define SRC_MISSION_H_

#include "src/random.h"
#include "src/resource.h"
#include "src/building.h"
#include <vector>

class mission_t {
 public:
  enum victory_condition : unsigned char {
    VICTORY_NO_CONDITION = 0,

    VICTORY_IS_BUILDING_CONDITION = 0x20,

    VICTORY_BUILDING_FISHER = VICTORY_IS_BUILDING_CONDITION + BUILDING_FISHER,
    VICTORY_BUILDING_LUMBERJACK,
    VICTORY_BUILDING_BOATBUILDER,
    VICTORY_BUILDING_STONECUTTER,
    VICTORY_BUILDING_STONEMINE,
    VICTORY_BUILDING_COALMINE,
    VICTORY_BUILDING_IRONMINE,
    VICTORY_BUILDING_GOLDMINE,
    VICTORY_BUILDING_FORESTER,
    VICTORY_BUILDING_STOCK,
    VICTORY_BUILDING_HUT,
    VICTORY_BUILDING_FARM,
    VICTORY_BUILDING_BUTCHER,
    VICTORY_BUILDING_PIGFARM,
    VICTORY_BUILDING_MILL,
    VICTORY_BUILDING_BAKER,
    VICTORY_BUILDING_SAWMILL,
    VICTORY_BUILDING_STEELSMELTER,
    VICTORY_BUILDING_TOOLMAKER,
    VICTORY_BUILDING_WEAPONSMITH,
    VICTORY_BUILDING_TOWER,
    VICTORY_BUILDING_FORTRESS,
    VICTORY_BUILDING_GOLDSMELTER,
    VICTORY_BUILDING_CASTLE,

    VICTORY_IS_RESOURCE_CONDITION = 0x40,

    VICTORY_RESOURCE_FISH = VICTORY_IS_RESOURCE_CONDITION + RESOURCE_FISH,
    VICTORY_RESOURCE_PIG,
    VICTORY_RESOURCE_MEAT,
    VICTORY_RESOURCE_WHEAT,
    VICTORY_RESOURCE_FLOUR,
    VICTORY_RESOURCE_BREAD,
    VICTORY_RESOURCE_LUMBER,
    VICTORY_RESOURCE_PLANK,
    VICTORY_RESOURCE_BOAT,
    VICTORY_RESOURCE_STONE,
    VICTORY_RESOURCE_IRONORE,
    VICTORY_RESOURCE_STEEL,
    VICTORY_RESOURCE_COAL,
    VICTORY_RESOURCE_GOLDORE,
    VICTORY_RESOURCE_GOLDBAR,
    VICTORY_RESOURCE_SHOVEL,
    VICTORY_RESOURCE_HAMMER,
    VICTORY_RESOURCE_ROD,
    VICTORY_RESOURCE_CLEAVER,
    VICTORY_RESOURCE_SCYTHE,
    VICTORY_RESOURCE_AXE,
    VICTORY_RESOURCE_SAW,
    VICTORY_RESOURCE_PICK,
    VICTORY_RESOURCE_PINCER,
    VICTORY_RESOURCE_SWORD,
    VICTORY_RESOURCE_SHIELD,

    VICTORY_IS_SPECIAL_CONDITION = 0x80,
      VICTORY_SPECIAL_TOOLS = VICTORY_IS_SPECIAL_CONDITION + 1,
      VICTORY_SPECIAL_WEAPONS
  };


  typedef struct {
    building_type_t building;
    int col;
    int row;
  } building_preset_t;

  typedef struct {
    int col;
    int row;
  } pos_preset_t;

  typedef struct {
    size_t face;
    size_t intelligence;
    size_t supplies;
    size_t reproduction;
    pos_preset_t castle;
  } player_preset_t;

  typedef struct {
    victory_condition condition;
    uint8_t amount;
  } victory_condition_t;

  const char *name;
  random_state_t rnd;
  player_preset_t player[4];
  victory_condition_t victory[4];
  const char * start_text;
  const char * win_text;
  const char * lose_text;

  static mission_t *get_mission(int mission);
  static mission_t *get_tutorial(int mission);

  static int get_mission_count();
  static int get_tutorials_count();
};

#endif  // SRC_MISSION_H_
