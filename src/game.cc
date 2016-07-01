/*
 * game.cc - Gameplay related functions
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

/* The following are functions that concern the gameplay as
   a whole. Functions that only act on a specific game object should
   go in the respective source file. */

#include "src/game.h"

#include <cassert>
#include <cstring>
#include <string>
#include <algorithm>
#include <map>
#include <sstream>

#include "src/mission.h"
#include "src/savegame.h"
#include "src/debug.h"
#include "src/log.h"
#include "src/misc.h"
#include "src/inventory.h"

#define GROUND_ANALYSIS_RADIUS  25

game_t::game_t(int map_generator)
  : players(this)
  , flags(this)
  , inventories(this)
  , buildings(this)
  , serfs(this) {
  map = NULL;
  this->map_generator = map_generator;
  allocate_objects();
}

game_t::~game_t() {
  deinit();
}

/* Clear the serf request bit of all flags and buildings.
   This allows the flag or building to try and request a
   serf again. */
void
game_t::clear_serf_request_failure() {
  for (buildings_t::iterator i = buildings.begin(); i != buildings.end(); ++i) {
    building_t *building = *i;
    building->clear_serf_request_failure();
  }

  for (flags_t::iterator i = flags.begin(); i != flags.end(); ++i) {
    flag_t *flag = *i;
    flag->serf_request_clear();
  }
}

void
game_t::update_knight_morale() {
  for (players_t::iterator it = players.begin(); it != players.end(); ++it) {
    player_t *player = *it;
    player->update_knight_morale();
  }
}

typedef struct {
  resource_type_t resource;
  int *max_prio;
  flag_t **flags;
} update_inventories_data_t;

bool
game_t::update_inventories_cb(flag_t *flag, void *d) {
  update_inventories_data_t *data =
                                reinterpret_cast<update_inventories_data_t*>(d);
  int inv = flag->get_search_dir();
  if (data->max_prio[inv] < 255 && flag->has_building()) {
    building_t *building = flag->get_building();

    int bld_prio = building->get_max_priority_for_resource(data->resource, 16);
    if (bld_prio > data->max_prio[inv]) {
      data->max_prio[inv] = bld_prio;
      data->flags[inv] = flag;
    }
  }

  return false;
}

/* Update inventories as part of the game progression. Moves the appropriate
   resources that are needed outside of the inventory into the out queue. */
void
game_t::update_inventories() {
  const resource_type_t arr_1[] = {
    RESOURCE_PLANK,
    RESOURCE_STONE,
    RESOURCE_STEEL,
    RESOURCE_COAL,
    RESOURCE_LUMBER,
    RESOURCE_IRONORE,
    RESOURCE_GROUP_FOOD,
    RESOURCE_PIG,
    RESOURCE_FLOUR,
    RESOURCE_WHEAT,
    RESOURCE_GOLDBAR,
    RESOURCE_GOLDORE,
    RESOURCE_NONE,
  };

  const resource_type_t arr_2[] = {
    RESOURCE_STONE,
    RESOURCE_IRONORE,
    RESOURCE_GOLDORE,
    RESOURCE_COAL,
    RESOURCE_STEEL,
    RESOURCE_GOLDBAR,
    RESOURCE_GROUP_FOOD,
    RESOURCE_PIG,
    RESOURCE_FLOUR,
    RESOURCE_WHEAT,
    RESOURCE_LUMBER,
    RESOURCE_PLANK,
    RESOURCE_NONE,
  };

  const resource_type_t arr_3[] = {
    RESOURCE_GROUP_FOOD,
    RESOURCE_WHEAT,
    RESOURCE_PIG,
    RESOURCE_FLOUR,
    RESOURCE_GOLDBAR,
    RESOURCE_STONE,
    RESOURCE_PLANK,
    RESOURCE_STEEL,
    RESOURCE_COAL,
    RESOURCE_LUMBER,
    RESOURCE_GOLDORE,
    RESOURCE_IRONORE,
    RESOURCE_NONE,
  };

  /* AI: TODO */

  const resource_type_t *arr = NULL;
  switch (random_int() & 7) {
    case 0: arr = arr_2; break;
    case 1: arr = arr_3; break;
    default: arr = arr_1; break;
  }

  while (arr[0] != RESOURCE_NONE) {
    for (players_t::iterator it = players.begin(); it != players.end(); ++it) {
      inventory_t *invs[256];
      int n = 0;
      for (inventories_t::iterator i = inventories.begin();
           i != inventories.end(); ++i) {
        inventory_t *inventory = *i;
        if (inventory->get_owner() == (*it)->get_index() &&
            !inventory->is_queue_full()) {
          inventory_mode_t res_dir = inventory->get_res_mode();
          if (res_dir == mode_in || res_dir == mode_stop) {  // In mode,
                                                             // stop mode
            if (arr[0] == RESOURCE_GROUP_FOOD) {
              if (inventory->has_food()) {
                invs[n++] = inventory;
                if (n == 256) break;
              }
            } else if (inventory->get_count_of(arr[0]) != 0) {
              invs[n++] = inventory;
              if (n == 256) break;
            }
          } else { /* Out mode */
            player_t *player = *it;

            int prio = 0;
            resource_type_t type = RESOURCE_NONE;
            for (int i = 0; i < 26; i++) {
              if (inventory->get_count_of((resource_type_t)i) != 0 &&
                  player->get_inventory_prio(i) >= prio) {
                prio = player->get_inventory_prio(i);
                type = (resource_type_t)i;
              }
            }

            if (type != RESOURCE_NONE) {
              inventory->add_to_queue(type, 0);
            }
          }
        }
      }

      if (n == 0) continue;

      flag_search_t search(this);

      int max_prio[256];
      flag_t *flags_[256];

      for (int i = 0; i < n; i++) {
        max_prio[i] = 0;
        flags_[i] = NULL;
        flag_t *flag = flags[invs[i]->get_flag_index()];
        flag->set_search_dir((dir_t)i);
        search.add_source(flag);
      }

      update_inventories_data_t data;
      data.resource = arr[0];
      data.max_prio = max_prio;
      data.flags = flags_;
      search.execute(update_inventories_cb, false, true, &data);

      for (int i = 0; i < n; i++) {
        if (max_prio[i] > 0) {
          LOGV("game", " dest for inventory %i found", i);
          resource_type_t res = (resource_type_t)arr[0];

          building_t *dest_bld = flags_[i]->get_building();
          bool r = dest_bld->add_requested_resource(res, false);
          assert(r);

          /* Put resource in out queue */
          inventory_t *src_inv = invs[i];
          src_inv->add_to_queue(res, dest_bld->get_flag_index());
        }
      }
    }
    arr += 1;
  }
}

/* Update flags as part of the game progression. */
void
game_t::update_flags() {
  for (flags_t::iterator i = flags.begin(); i != flags.end(); ++i) {
    flag_t *flag = *i;
    flag->update();
  }
}

typedef struct {
  inventory_t *inventory;
  building_t *building;
  int serf_type;
  int dest_index;
  resource_type_t res1;
  resource_type_t res2;
} send_serf_to_flag_data_t;

bool
game_t::send_serf_to_flag_search_cb(flag_t *flag, void *d) {
  if (!flag->has_inventory()) {
    return false;
  }

  send_serf_to_flag_data_t *data =
                       reinterpret_cast<send_serf_to_flag_data_t*>(d);

  /* Inventory reached */
  building_t *building = flag->get_building();
  inventory_t *inv = building->get_inventory();

  int type = data->serf_type;
  if (type < 0) {
    int knight_type = -1;
    for (int i = 4; i >= -type-1; i--) {
      if (inv->have_serf((serf_type_t)(SERF_KNIGHT_0+i))) {
        knight_type = i;
        break;
      }
    }

    if (knight_type >= 0) {
      /* Knight of appropriate type was found. */
      serf_t *serf =
                  inv->call_out_serf((serf_type_t)(SERF_KNIGHT_0+knight_type));

      data->building->knight_request_granted();

      serf->go_out_from_inventory(inv->get_index(),
                                  data->building->get_flag_index(), -1);

      return true;
    } else if (type == -1) {
      /* See if a knight can be created here. */
      if (inv->have_serf(SERF_GENERIC) &&
          inv->get_count_of(RESOURCE_SWORD) > 0 &&
          inv->get_count_of(RESOURCE_SHIELD) > 0) {
        data->inventory = inv;
        return true;
      }
    }
  } else {
    if (inv->have_serf((serf_type_t)type)) {
      if (type != SERF_GENERIC || inv->free_serf_count() > 4) {
        serf_t *serf = inv->call_out_serf((serf_type_t)type);

        int mode = 0;

        if (type == SERF_GENERIC) {
          mode = -2;
        } else if (type == SERF_GEOLOGIST) {
          mode = 6;
        } else {
          building_t *dest_bld =
                    flag->get_game()->flags[data->dest_index]->get_building();
          dest_bld->request_serf();
          mode = -1;
        }

        serf->go_out_from_inventory(inv->get_index(), data->dest_index, mode);

        return true;
      }
    } else {
      if (data->inventory == NULL &&
          inv->have_serf(SERF_GENERIC) &&
          (data->res1 == -1 || inv->get_count_of(data->res1) > 0) &&
          (data->res2 == -1 || inv->get_count_of(data->res2) > 0)) {
        data->inventory = inv;
        /* player_t *player = globals->player[SERF_PLAYER(serf)]; */
        /* game.field_340 = player->cont_search_after_non_optimal_find; */
        return true;
      }
    }
  }

  return false;
}

/* Dispatch serf from (nearest?) inventory to flag. */
bool
game_t::send_serf_to_flag(flag_t *dest, serf_type_t type, resource_type_t res1,
                          resource_type_t res2) {
  building_t *building = NULL;
  if (dest->has_building()) {
    building = dest->get_building();
  }

  /* If type is negative, building is non-NULL. */
  if ((type < 0) && (building != NULL)) {
    player_t *player = players[building->get_owner()];
    type = player->get_cycling_sert_type(type);
  }

  send_serf_to_flag_data_t data;
  data.inventory = NULL;
  data.building = building;
  data.serf_type = type;
  data.dest_index = dest->get_index();
  data.res1 = res1;
  data.res2 = res2;

  bool r = flag_search_t::single(dest, send_serf_to_flag_search_cb, true, false,
                                 &data);
  if (!r) {
    return true;
  } else if (data.inventory != NULL) {
    inventory_t *inventory = data.inventory;
    serf_t *serf = inventory->call_out_serf(SERF_GENERIC);

    if ((type < 0) && (building != NULL)) {
      /* Knight */
      building->knight_request_granted();

      serf->set_type(SERF_KNIGHT_0);
      serf->go_out_from_inventory(inventory->get_index(),
                                  building->get_flag_index(), -1);

      inventory->pop_resource(RESOURCE_SWORD);
      inventory->pop_resource(RESOURCE_SHIELD);
    } else {
      serf->set_type((serf_type_t)type);

      int mode = 0;

      if (type == SERF_GEOLOGIST) {
        mode = 6;
      } else {
        building->request_serf();
        mode = -1;
      }

      serf->go_out_from_inventory(inventory->get_index(), dest->get_index(),
                                  mode);

      if (res1 != RESOURCE_NONE) inventory->pop_resource(res1);
      if (res2 != RESOURCE_NONE) inventory->pop_resource(res2);
    }

    return true;
  }

  return false;
}

/* Dispatch geologist to flag. */
bool
game_t::send_geologist(flag_t *dest) {
  return send_serf_to_flag(dest, SERF_GEOLOGIST, RESOURCE_HAMMER,
                           RESOURCE_NONE);
}

/* Update buildings as part of the game progression. */
void
game_t::update_buildings() {
  buildings_t::iterator i = buildings.begin();
  while (i != buildings.end()) {
    building_t *building = *i;
    ++i;
    building->update(tick);
  }
}

/* Update serfs as part of the game progression. */
void
game_t::update_serfs() {
  for (serfs_t::iterator i = serfs.begin(); i != serfs.end(); ++i) {
    serf_t *serf = *i;
    serf->update();
  }
}

/* Update historical player statistics for one measure. */
void
game_t::record_player_history(int max_level, int aspect,
                              const int history_index[],
                              const values_t &values) {
  unsigned int total = 0;
  for (values_t::const_iterator it = values.begin(); it != values.end(); ++it) {
    total += it->second;
  }
  total = std::max(1u, total);

  for (int i = 0; i < max_level+1; i++) {
    int mode = (aspect << 2) | i;
    int index = history_index[i];
    for (values_t::const_iterator it = values.begin();
         it != values.end(); ++it) {
      uint64_t value = it->second;
      players[it->first]->set_player_stat_history(mode, index,
                                           static_cast<int>((100*value)/total));
    }
  }
}

/* Calculate whether one player has enough advantage to be
   considered a clear winner regarding one aspect.
   Return -1 if there is no clear winner. */
int
game_t::calculate_clear_winner(const values_t &values) {
  int total = 0;
  for (values_t::const_iterator it = values.begin(); it != values.end(); ++it) {
    total += it->second;
  }
  total = std::max(1, total);

  for (values_t::const_iterator it = values.begin(); it != values.end(); ++it) {
    uint64_t value = it->second;
    if ((100*value)/total >= 75) return it->first;
  }

  return -1;
}

// Check if the player satisfy the conditions of this mission
bool
game_t::player_check_victory_conditions(mission_t * mission, player_t * player) {

	for (int c = 0; c < GAME_MAX_VICTORY_CONDITION_COUNT; c++) {
		if (mission->victory[c].condition != mission_t::VICTORY_NO_CONDITION) {

			// is this a building condition?
			if ((mission->victory[c].condition & mission_t::VICTORY_IS_BUILDING_CONDITION) == mission_t::VICTORY_IS_BUILDING_CONDITION) {
				int building_type = mission->victory[c].condition - mission_t::VICTORY_IS_BUILDING_CONDITION;

				if (player->get_completed_building_count(building_type) < mission->victory[c].amount) return false;
			}
			// is this a building resource condition?
			else if ((mission->victory[c].condition & mission_t::VICTORY_IS_RESOURCE_CONDITION) == mission_t::VICTORY_IS_RESOURCE_CONDITION) {
				int resource_type = mission->victory[c].condition - mission_t::VICTORY_IS_RESOURCE_CONDITION;

				if (player->get_resource_count_total((resource_type_t)resource_type) < mission->victory[c].amount) return false;
			}
			// is this a building resource condition?
			else if ((mission->victory[c].condition & mission_t::VICTORY_IS_SPECIAL_CONDITION) == mission_t::VICTORY_IS_SPECIAL_CONDITION) {

				int sum = 0;
				switch (mission->victory[c].condition) {
					case mission_t::VICTORY_SPECIAL_WEAPONS:
						sum += player->get_resource_count_total(resource_type_t::RESOURCE_SWORD);
						sum += player->get_resource_count_total(resource_type_t::RESOURCE_SHIELD);
						break;
					case mission_t::VICTORY_SPECIAL_TOOLS:
						sum += player->get_resource_count_total(resource_type_t::RESOURCE_SHOVEL);
						sum += player->get_resource_count_total(resource_type_t::RESOURCE_HAMMER);
						sum += player->get_resource_count_total(resource_type_t::RESOURCE_ROD);
						sum += player->get_resource_count_total(resource_type_t::RESOURCE_CLEAVER);
						sum += player->get_resource_count_total(resource_type_t::RESOURCE_SCYTHE);
						sum += player->get_resource_count_total(resource_type_t::RESOURCE_AXE);
						sum += player->get_resource_count_total(resource_type_t::RESOURCE_SAW);
						sum += player->get_resource_count_total(resource_type_t::RESOURCE_PICK);
						sum += player->get_resource_count_total(resource_type_t::RESOURCE_PINCER);
						break;
				}

				if (sum < mission->victory[c].amount) return false;
			}
		}
	}
	return true;
	
}

/* Update statistics of the game. */
void
game_t::update_game_stats() {
  if (static_cast<int>(game_stats_counter) > tick_diff) {
    game_stats_counter -= tick_diff;
  } else {
    game_stats_counter += 1500 - tick_diff;

	player_score_leader = 0;
    int player_score_leader_land = -1;
	int player_score_leader_minitary = -1;
	int player_score_victory_condition = -1;

    int update_level = 0;

    /* Update first level index */
    player_history_index[0] = player_history_index[0]+1 < 112 ?
                              player_history_index[0]+1 : 0;

    player_history_counter[0] -= 1;
    if (player_history_counter[0] < 0) {
      update_level = 1;
      player_history_counter[0] = 3;

      /* Update second level index */
      player_history_index[1] = player_history_index[1]+1 < 112 ?
                                player_history_index[1]+1 : 0;

      player_history_counter[1] -= 1;
      if (player_history_counter[1] < 0) {
        update_level = 2;
        player_history_counter[1] = 4;

        /* Update third level index */
        player_history_index[2] = player_history_index[2]+1 < 112 ?
                                  player_history_index[2]+1 : 0;

        player_history_counter[2] -= 1;
        if (player_history_counter[2] < 0) {
          update_level = 3;

          player_history_counter[2] = 4;

          /* Update fourth level index */
          player_history_index[3] = player_history_index[3]+1 < 112 ?
                                    player_history_index[3]+1 : 0;
        }
      }
    }

    std::map<unsigned int, unsigned int> values;

    /* Store land area stats in history. */
    for (players_t::iterator it = players.begin(); it != players.end(); ++it) {
      values[(*it)->get_index()] = (*it)->get_land_area();
    }
    record_player_history(update_level, 1, player_history_index, values);
	player_score_leader_land = calculate_clear_winner(values);

    /* Store building stats in history. */
    for (players_t::iterator it = players.begin(); it != players.end(); ++it) {
      values[(*it)->get_index()] = (*it)->get_building_score();
    }
    record_player_history(update_level, 2, player_history_index, values);

    /* Store military stats in history. */
    for (players_t::iterator it = players.begin(); it != players.end(); ++it) {
      values[(*it)->get_index()] = (*it)->get_military_score();
    }
    record_player_history(update_level, 3, player_history_index, values);
	player_score_leader_minitary = calculate_clear_winner(values);

    /* Store condensed score of all aspects in history. */
    for (players_t::iterator it = players.begin(); it != players.end(); ++it) {
      values[(*it)->get_index()] = (*it)->get_score();
    }
    record_player_history(update_level, 0, player_history_index, values);
	

	// check victory conditions set by the mission
	if (mission_level > -1) {
      mission_t *mission;

      if (mission_is_tutorial) {
        mission = mission_t::get_tutorial(mission_level);
      }
      else {
        mission = mission_t::get_mission(mission_level);
      }

	  // are ther special victory conditions?
      if (mission->victory[0].condition != mission_t::VICTORY_NO_CONDITION) {
        // delete the other conditions
        player_score_leader_land = -1;
        player_score_leader_minitary = -1;

        for (players_t::iterator it = players.begin(); it != players.end(); ++it) {
          if (player_check_victory_conditions(mission, (*it))) player_score_victory_condition = (*it)->get_index();
        }
      }
	}


	// Determine winner based on "player_scores" values
	if ((player_score_leader_land >= 0) || (player_score_leader_minitary >= 0) || (player_score_victory_condition >= 0)) {

      int winner = player_score_leader_land;
      if (player_score_leader_minitary >= 0) winner = player_score_leader_minitary;
      if (player_score_victory_condition >= 0) winner = player_score_victory_condition;

	  // TODO show victory screen
      printf("Player %i win!", winner);
	}


  }

  if (static_cast<int>(history_counter) > tick_diff) {
    history_counter -= tick_diff;
  } else {
    history_counter += 6000 - tick_diff;

    int index = resource_history_index;

    for (int res = 0; res < 26; res++) {
      for (players_t::iterator it = players.begin();
           it != players.end(); ++it) {
        (*it)->update_stats(res);
      }
    }

    resource_history_index = index+1 < 120 ? index+1 : 0;
  }
}

/* Update game state after tick increment. */
void
game_t::update() {
  /* Increment tick counters */
  const_tick += 1;

  /* Update tick counters based on game speed */
  last_tick = tick;
  tick += game_speed;
  tick_diff = tick - last_tick;

  clear_serf_request_failure();
  map->update(tick);

  /* Update players */
  for (players_t::iterator it = players.begin(); it != players.end(); ++it) {
    (*it)->update();
  }

  /* Update knight morale */
  knight_morale_counter -= tick_diff;
  if (knight_morale_counter < 0) {
    update_knight_morale();
    knight_morale_counter += 256;
  }

  /* Schedule resources to go out of inventories */
  inventory_schedule_counter -= tick_diff;
  if (inventory_schedule_counter < 0) {
    update_inventories();
    inventory_schedule_counter += 64;
  }

#if 0
  /* AI related updates */
  game.next_index = (game.next_index + 1) % game.max_next_index;
  if (game.next_index > 32) {
    for (int i = 0; i < game.max_next_index) {
      int i = 33 - game.next_index;
      player_t *player = game.player[i & 3];
      if (PLAYER_IS_ACTIVE(player) && PLAYER_IS_AI(player)) {
        /* AI */
        /* TODO */
      }
      game.next_index += 1;
    }
  } else if (game.game_speed > 0 &&
       game.max_flag_index < 50) {
    player_t *player = game.player[game.next_index & 3];
    if (PLAYER_IS_ACTIVE(player) && PLAYER_IS_AI(player)) {
      /* AI */
      /* TODO */
    }
  }
#endif

  update_flags();
  update_buildings();
  update_serfs();
  update_game_stats();
}

/* Pause or unpause the game. */
void
game_t::pause() {
  if (game_speed != 0) {
    game_speed_save = game_speed;
    game_speed = 0;
  } else {
    game_speed = game_speed_save;
  }

  LOGI("game", "Game speed: %u", game_speed);
}

void
game_t::speed_increase() {
  if (game_speed < 40) {
    game_speed += 1;
    LOGI("game", "Game speed: %u", game_speed);
  }
}

void
game_t::speed_decrease() {
  if (game_speed >= 1) {
    game_speed -= 1;
    LOGI("game", "Game speed: %u", game_speed);
  }
}

void
game_t::speed_reset() {
  game_speed = DEFAULT_GAME_SPEED;
  LOGI("game", "Game speed: %u", game_speed);
}

/* Generate an estimate of the amount of resources in the ground at map pos.*/
void
game_t::get_resource_estimate(map_pos_t pos, int weight, int estimates[5]) {
  if ((map->get_obj(pos) == MAP_OBJ_NONE ||
       map->get_obj(pos) >= MAP_OBJ_TREE_0) &&
       map->get_res_type(pos) != GROUND_DEPOSIT_NONE) {
    int value = weight * map->get_res_amount(pos);
    estimates[map->get_res_type(pos)] += value;
  }
}

/* Prepare a ground analysis at position. */
void
game_t::prepare_ground_analysis(map_pos_t pos, int estimates[5]) {
  for (int i = 0; i < 5; i++) estimates[i] = 0;

  /* Sample the cursor position with maximum weighting. */
  get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS, estimates);

  /* Move outward in a spiral around the initial pos.
     The weighting of the samples attenuates linearly
     with the distance to the center. */
  for (int i = 0; i < GROUND_ANALYSIS_RADIUS-1; i++) {
    pos = map->move_right(pos);

    for (int j = 0; j < i+1; j++) {
      get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, estimates);
      pos = map->move_down(pos);
    }

    for (int j = 0; j < i+1; j++) {
      get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, estimates);
      pos = map->move_left(pos);
    }

    for (int j = 0; j < i+1; j++) {
      get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, estimates);
      pos = map->move_up_left(pos);
    }

    for (int j = 0; j < i+1; j++) {
      get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, estimates);
      pos = map->move_up(pos);
    }

    for (int j = 0; j < i+1; j++) {
      get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, estimates);
      pos = map->move_right(pos);
    }

    for (int j = 0; j < i+1; j++) {
      get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, estimates);
      pos = map->move_down_right(pos);
    }
  }

  /* Process the samples. */
  for (int i = 0; i < 5; i++) {
    estimates[i] >>= 4;
    estimates[i] = std::min(estimates[i], 999);
  }
}

int
game_t::road_segment_in_water(map_pos_t pos, dir_t dir) {
  if (dir > DIR_DOWN) {
    pos = map->move(pos, dir);
    dir = DIR_REVERSE(dir);
  }

  int water = 0;

  switch (dir) {
  case DIR_RIGHT:
    if (map->type_down(pos) < 4 && map->type_up(map->move_up(pos)) < 4) {
      water = 1;
    }
    break;
  case DIR_DOWN_RIGHT:
    if (map->type_up(pos) < 4 && map->type_down(pos) < 4) {
      water = 1;
    }
    break;
  case DIR_DOWN:
    if (map->type_up(pos) < 4 && map->type_down(map->move_left(pos)) < 4) {
      water = 1;
    }
    break;
  default:
    NOT_REACHED();
    break;
  }

  return water;
}

/* Test whether a given road can be constructed by player. The final
   destination will be returned in dest, and water will be set if the
   resulting path is a water path.
   This will return success even if the destination does _not_ contain
   a flag, and therefore partial paths can be validated with this function. */
int
game_t::can_build_road(const road_t &road, const player_t *player,
                       map_pos_t *dest, bool *water) {
  /* Follow along path to other flag. Test along the way
     whether the path is on ground or in water. */
  map_pos_t pos = road.get_source();
  int test = 0;

  if (!map->has_owner(pos) || map->get_owner(pos) != player->get_index() ||
      !map->has_flag(pos)) {
    return 0;
  }

  road_t::dirs_t dirs = road.get_dirs();
  road_t::dirs_t::const_iterator it = dirs.begin();
  for (; it != dirs.end(); ++it) {
    if (!map->is_road_segment_valid(pos, *it)) {
      return -1;
    }

    if (map->road_segment_in_water(pos, *it)) {
      test |= BIT(1);
    } else {
      test |= BIT(0);
    }

    pos = map->move(pos, *it);

    /* Check that owner is correct, and that only the destination has a flag. */
    if (!map->has_owner(pos) || map->get_owner(pos) != player->get_index() ||
        (map->has_flag(pos) && it != --dirs.end())) {
      return 0;
    }
  }

  map_pos_t d = pos;
  if (dest != NULL) *dest = d;

  /* Bit 0 indicates a ground path, bit 1 indicates
     water path. Abort if path went through both
     ground and water. */
  bool w = false;
  if (BIT_TEST(test, 1)) {
    w = true;
    if (BIT_TEST(test, 0)) return 0;
  }
  if (water != NULL) *water = w;

  return 1;
}

/* Construct a road spefified by a source and a list of directions. */
bool
game_t::build_road(const road_t &road, const player_t *player) {
  if (road.get_length() == 0) return false;

  map_pos_t dest;
  bool water_path;
  if (!can_build_road(road, player, &dest, &water_path)) {
    return false;
  }
  if (!map->has_flag(dest)) return false;

  road_t::dirs_t dirs = road.get_dirs();
  dir_t out_dir = dirs.front();
  dir_t in_dir = DIR_REVERSE(dirs.back());

  /* Actually place road segments */
  if (!map->place_road_segments(road)) return false;

  /* Connect flags */
  flag_t *src_flag = get_flag_at_pos(road.get_source());
  flag_t *dest_flag = get_flag_at_pos(dest);

  src_flag->link_with_flag(dest_flag, water_path, road.get_length(),
                           in_dir, out_dir);

  return true;
}

void
game_t::flag_reset_transport(flag_t *flag) {
  /* Clear destination for any serf with resources for this flag. */
  for (serfs_t::iterator i = serfs.begin(); i != serfs.end(); ++i) {
    serf_t *serf = *i;
    serf->reset_transport(flag);
  }

  /* Flag. */
  for (flags_t::iterator i = flags.begin(); i != flags.end(); ++i) {
    flag->reset_transport(*i);
  }

  /* Inventories. */
  for (inventories_t::iterator i = inventories.begin();
       i != inventories.end(); ++i) {
    inventory_t *inventory = *i;
    inventory->reset_queue_for_dest(flag);
  }
}

void
game_t::building_remove_player_refs(building_t *building) {
  for (players_t::iterator it = players.begin(); it != players.end(); ++it) {
    if ((*it)->temp_index == building->get_index()) {
      (*it)->temp_index = 0;
    }
  }
}

bool
game_t::path_serf_idle_to_wait_state(map_pos_t pos) {
  /* Look through serf array for the corresponding serf. */
  for (serfs_t::iterator i = serfs.begin(); i != serfs.end(); ++i) {
    serf_t *serf = *i;
    if (serf->idle_to_wait_state(pos)) {
      return true;
    }
  }

  return false;
}

void
game_t::remove_road_forwards(map_pos_t pos, dir_t dir) {
  dir_t in_dir = DIR_NONE;

  while (true) {
    if (map->get_idle_serf(pos)) {
      path_serf_idle_to_wait_state(pos);
    }

    if (map->get_serf_index(pos) != 0) {
      serf_t *serf = serfs[map->get_serf_index(pos)];
      if (!map->has_flag(pos)) {
        serf->set_lost_state();
      } else {
        /* Handle serf close to flag, where
           it should only be lost if walking
           in the wrong direction. */
        int d = serf->get_walking_dir();
        if (d < 0) d += 6;
        if (d == DIR_REVERSE(dir)) {
          serf->set_lost_state();
        }
      }
    }

    if (map->has_flag(pos)) {
      flag_t *flag = flags[map->get_obj_index(pos)];
      flag->del_path(DIR_REVERSE(in_dir));
      break;
    }

    in_dir = dir;
    dir = map->remove_road_segment(&pos, dir);
  }
}

bool
game_t::demolish_road_(map_pos_t pos) {
  /* TODO necessary?
  game.player[0]->flags |= BIT(4);
  game.player[1]->flags |= BIT(4);
  */

  if (!map->remove_road_backrefs(pos)) {
    /* TODO */
    return false;
  }

  /* Find directions of path segments to be split. */
  dir_t path_1_dir = DIR_NONE;
  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    if (BIT_TEST(map->paths(pos), d)) {
      path_1_dir = (dir_t)d;
      break;
    }
  }

  dir_t path_2_dir = DIR_NONE;
  for (int d = path_1_dir+1; d <= DIR_UP; d++) {
    if (BIT_TEST(map->paths(pos), d)) {
      path_2_dir = (dir_t)d;
      break;
    }
  }

  /* If last segment direction is UP LEFT it could
     be to a building and the real path is at UP. */
  if (path_2_dir == DIR_UP_LEFT && BIT_TEST(map->paths(pos), DIR_UP)) {
    path_2_dir = DIR_UP;
  }

  remove_road_forwards(pos, path_1_dir);
  remove_road_forwards(pos, path_2_dir);

  return 0;
}

/* Demolish road at position. */
bool
game_t::demolish_road(map_pos_t pos, player_t *player) {
  if (!can_demolish_road(pos, player)) return false;

  return demolish_road_(pos);
}

/* Build flag on existing path. Path must be split in two segments. */
void
game_t::build_flag_split_path(map_pos_t pos) {
  /* Find directions of path segments to be split. */
  dir_t path_1_dir = DIR_NONE;
  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    if (BIT_TEST(map->paths(pos), d)) {
      path_1_dir = (dir_t)d;
      break;
    }
  }

  dir_t path_2_dir = DIR_NONE;
  for (int d = path_1_dir+1; d <= DIR_UP; d++) {
    if (BIT_TEST(map->paths(pos), d)) {
      path_2_dir = (dir_t)d;
      break;
    }
  }

  /* If last segment direction is UP LEFT it could
     be to a building and the real path is at UP. */
  if (path_2_dir == DIR_UP_LEFT && BIT_TEST(map->paths(pos), DIR_UP)) {
    path_2_dir = DIR_UP;
  }

  serf_path_info_t path_1_data;
  serf_path_info_t path_2_data;

  flag_t::fill_path_serf_info(this, pos, path_1_dir, &path_1_data);
  flag_t::fill_path_serf_info(this, pos, path_2_dir, &path_2_data);

  flag_t *flag_2 = flags[path_2_data.flag_index];
  dir_t dir_2 = path_2_data.flag_dir;

  int select = -1;
  if (flag_2->serf_requested(dir_2)) {
    for (serfs_t::iterator i = serfs.begin(); i != serfs.end(); ++i) {
      serf_t *serf = *i;
      if (serf->path_splited(path_1_data.flag_index, path_1_data.flag_dir,
                             path_2_data.flag_index, path_2_data.flag_dir,
                             &select)) {
        break;
      }
    }

    serf_path_info_t *path_data = &path_1_data;
    if (select == 0) path_data = &path_2_data;

    flag_t *selected_flag = flags[path_data->flag_index];
    selected_flag->cancel_serf_request(path_data->flag_dir);
  }

  flag_t *flag = flags[map->get_obj_index(pos)];

  flag->restore_path_serf_info(path_1_dir, &path_1_data);
  flag->restore_path_serf_info(path_2_dir, &path_2_data);
}

/* Check whether player can build flag at pos. */
bool
game_t::can_build_flag(map_pos_t pos, const player_t *player) {
  /* Check owner of land */
  if (!map->has_owner(pos) || map->get_owner(pos) != player->get_index()) {
    return false;
  }

  /* Check that land is clear */
  if (map_t::map_space_from_obj[map->get_obj(pos)] != MAP_SPACE_OPEN) {
    return false;
  }

  /* Check whether cursor is in water */
  if (map->type_up(pos) < 4 &&
      map->type_down(pos) < 4 &&
      map->type_down(map->move_left(pos)) < 4 &&
      map->type_up(map->move_up_left(pos)) < 4 &&
      map->type_down(map->move_up_left(pos)) < 4 &&
      map->type_up(map->move_up(pos)) < 4) {
    return false;
  }

  /* Check that no flags are nearby */
  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    if (map->get_obj(map->move(pos, (dir_t)d)) == MAP_OBJ_FLAG) {
      return false;
    }
  }

  return true;
}

/* Build flag at pos. */
bool
game_t::build_flag(map_pos_t pos, player_t *player) {
  if (!can_build_flag(pos, player)) {
    return false;
  }

  flag_t *flag = flags.allocate();
  if (flag == NULL) return false;

  flag->set_owner(player->get_index());
  flag->set_position(pos);
  map->set_object(pos, MAP_OBJ_FLAG, flag->get_index());

  if (map->paths(pos) != 0) {
    build_flag_split_path(pos);
  }

  return true;
}

/* Check whether military buildings are allowed at pos. */
bool
game_t::can_build_military(map_pos_t pos) {
  /* Check that no military buildings are nearby */
  for (int i = 0; i < 1+6+12; i++) {
    map_pos_t p = map->pos_add_spirally(pos, i);
    if (map->get_obj(p) >= MAP_OBJ_SMALL_BUILDING &&
        map->get_obj(p) <= MAP_OBJ_CASTLE) {
      building_t *bld = buildings[map->get_obj_index(p)];
      if (bld->is_military()) {
        return false;
      }
    }
  }

  return true;
}

/* Return the height that is needed before a large building can be built.
   Returns negative if the needed height cannot be reached. */
int
game_t::get_leveling_height(map_pos_t pos) {
  /* Find min and max height */
  int h_min = 31;
  int h_max = 0;
  for (int i = 0; i < 12; i++) {
    map_pos_t p = map->pos_add_spirally(pos, 7+i);
    int h = map->get_height(p);
    if (h_min > h) h_min = h;
    if (h_max < h) h_max = h;
  }

  /* Adjust for height of adjacent unleveled buildings */
  for (int i = 0; i < 18; i++) {
    map_pos_t p = map->pos_add_spirally(pos, 19+i);
    if (map->get_obj(p) == MAP_OBJ_LARGE_BUILDING) {
      building_t *bld = buildings[map->get_obj_index(p)];
      if (bld->is_leveling()) { /* Leveling in progress */
        int h = bld->get_level();
        if (h_min > h) h_min = h;
        if (h_max < h) h_max = h;
      }
    }
  }

  /* Return if height difference is too big */
  if (h_max - h_min >= 9) return -1;

  /* Calculate "mean" height. Height of center is added twice. */
  int h_mean = map->get_height(pos);
  for (int i = 0; i < 7; i++) {
    map_pos_t p = map->pos_add_spirally(pos, i);
    h_mean += map->get_height(p);
  }
  h_mean >>= 3;

  /* Calcualte height after leveling */
  int h_new_min = std::max((h_max > 4) ? (h_max - 4) : 1, 1);
  int h_new_max = h_min + 4;
  int h_new = clamp(h_new_min, h_mean, h_new_max);

  return h_new;
}

bool
game_t::map_types_within(map_pos_t pos, unsigned int low, unsigned int high) {
  if ((map->type_up(pos) >= low &&
       map->type_up(pos) <= high) &&
      (map->type_down(pos) >= low &&
       map->type_down(pos) <= high) &&
      (map->type_down(map->move_left(pos)) >= low &&
       map->type_down(map->move_left(pos)) <= high) &&
      (map->type_up(map->move_up_left(pos)) >= low &&
       map->type_up(map->move_up_left(pos)) <= high) &&
      (map->type_down(map->move_up_left(pos)) >= low &&
       map->type_down(map->move_up_left(pos)) <= high) &&
      (map->type_up(map->move_up(pos)) >= low &&
       map->type_up(map->move_up(pos)) <= high)) {
    return true;
  }

  return false;
}

/* Checks whether a small building is possible at position.*/
bool
game_t::can_build_small(map_pos_t pos) {
  return map_types_within(pos, 4, 7);
}

/* Checks whether a mine is possible at position. */
bool
game_t::can_build_mine(map_pos_t pos) {
  return map_types_within(pos, 11, 14);
}

/* Checks whether a large building is possible at position. */
bool
game_t::can_build_large(map_pos_t pos) {
  /* Check that surroundings are passable by serfs. */
  for (int i = 0; i < 6; i++) {
    map_pos_t p = map->pos_add_spirally(pos, 1+i);
    map_space_t s = map_t::map_space_from_obj[map->get_obj(p)];
    if (s >= MAP_SPACE_SEMIPASSABLE) return false;
  }

  /* Check that buildings in the second shell aren't large or castle. */
  for (int i = 0; i < 12; i++) {
    map_pos_t p = map->pos_add_spirally(pos, 7+i);
    if (map->get_obj(p) >= MAP_OBJ_LARGE_BUILDING &&
        map->get_obj(p) <= MAP_OBJ_CASTLE) {
      return false;
    }
  }

  /* Check if center hexagon is not type grass. */
  if (map->type_up(pos) != 5 ||
      map->type_down(pos) != 5 ||
      map->type_down(map->move_left(pos)) != 5 ||
      map->type_up(map->move_up_left(pos)) != 5 ||
      map->type_down(map->move_up_left(pos)) != 5 ||
      map->type_up(map->move_up(pos)) != 5) {
    return false;
  }

  /* Check that leveling is possible */
  int r = get_leveling_height(pos);
  if (r < 0) return false;

  return true;
}

/* Checks whether a castle can be built by player at position. */
bool
game_t::can_build_castle(map_pos_t pos, const player_t *player) {
  if (player->has_castle()) return false;

  /* Check owner of land around position */
  for (int i = 0; i < 7; i++) {
    map_pos_t p = map->pos_add_spirally(pos, i);
    if (map->has_owner(p)) return false;
  }

  /* Check that land is clear at position */
  if (map_t::map_space_from_obj[map->get_obj(pos)] != MAP_SPACE_OPEN ||
      map->paths(pos) != 0) {
    return false;
  }

  map_pos_t flag_pos = map->move_down_right(pos);

  /* Check that land is clear at position */
  if (map_t::map_space_from_obj[map->get_obj(flag_pos)] != MAP_SPACE_OPEN ||
      map->paths(flag_pos) != 0) {
    return false;
  }

  if (!can_build_large(pos)) return false;

  return true;
}

/* Check whether player is allowed to build anything
   at position. To determine if the initial castle can
   be built use game_can_build_castle() instead.

   TODO Existing buildings at position should be
   disregarded so this can be used to determine what
   can be built after the existing building has been
   demolished. */
bool
game_t::can_player_build(map_pos_t pos, const player_t *player) {
  if (!player->has_castle()) return false;

  /* Check owner of land around position */
  for (int i = 0; i < 7; i++) {
    map_pos_t p = map->pos_add_spirally(pos, i);
    if (!map->has_owner(p) || map->get_owner(p) != player->get_index()) {
      return false;
    }
  }

  /* Check whether cursor is in water */
  if (map->type_up(pos) < 4 &&
      map->type_down(pos) < 4 &&
      map->type_down(map->move_left(pos)) < 4 &&
      map->type_up(map->move_up_left(pos)) < 4 &&
      map->type_down(map->move_up_left(pos)) < 4 &&
      map->type_up(map->move_up(pos)) < 4) {
    return false;
  }

  /* Check that no paths are blocking. */
  if (map->paths(pos) != 0) return false;

  return true;
}

/* Checks whether a building of the specified type is possible at
   position. */
bool
game_t::can_build_building(map_pos_t pos, building_type_t type,
                           const player_t *player) {
  if (!can_player_build(pos, player)) return false;

  /* Check that space is clear */
  if (map_t::map_space_from_obj[map->get_obj(pos)] != MAP_SPACE_OPEN) {
    return false;
  }

  /* Check that building flag is possible if it
     doesn't already exist. */
  map_pos_t flag_pos = map->move_down_right(pos);
  if (!map->has_flag(flag_pos) &&
      !can_build_flag(flag_pos, player)) {
    return false;
  }

  /* Check if building size is possible. */
  switch (type) {
  case BUILDING_FISHER:
  case BUILDING_LUMBERJACK:
  case BUILDING_BOATBUILDER:
  case BUILDING_STONECUTTER:
  case BUILDING_FORESTER:
  case BUILDING_HUT:
  case BUILDING_MILL:
    if (!can_build_small(pos)) return false;
    break;
  case BUILDING_STONEMINE:
  case BUILDING_COALMINE:
  case BUILDING_IRONMINE:
  case BUILDING_GOLDMINE:
    if (!can_build_mine(pos)) return false;
    break;
  case BUILDING_STOCK:
  case BUILDING_FARM:
  case BUILDING_BUTCHER:
  case BUILDING_PIGFARM:
  case BUILDING_BAKER:
  case BUILDING_SAWMILL:
  case BUILDING_STEELSMELTER:
  case BUILDING_TOOLMAKER:
  case BUILDING_WEAPONSMITH:
  case BUILDING_TOWER:
  case BUILDING_FORTRESS:
  case BUILDING_GOLDSMELTER:
    if (!can_build_large(pos)) return false;
    break;
  default:
    NOT_REACHED();
    break;
  }

  /* Check if military building is possible */
  if ((type == BUILDING_HUT ||
       type == BUILDING_TOWER ||
       type == BUILDING_FORTRESS) &&
      !can_build_military(pos)) {
    return false;
  }

  return true;
}

/* Build building at position. */
bool
game_t::build_building(map_pos_t pos, building_type_t type, player_t *player) {
  if (!can_build_building(pos, type, player)) {
    return false;
  }

  if (type == BUILDING_STOCK) {
    /* TODO Check that more stocks are allowed to be built */
  }

  building_t *bld = buildings.allocate();
  if (bld == NULL) {
    return false;
  }

  flag_t *flag = NULL;
  unsigned int flg_index = 0;
  if (map->get_obj(map->move_down_right(pos)) != MAP_OBJ_FLAG) {
    flag = flags.allocate();
    if (flag == NULL) {
      buildings.erase(bld->get_index());
      return false;
    }
    flg_index = flag->get_index();
  }

  bld->set_level(get_leveling_height(pos));
  bld->set_position(pos);
  map_obj_t map_obj = bld->start_building(type);
  player->building_founded(bld);

  bool split_path = false;
  if (map->get_obj(map->move_down_right(pos)) != MAP_OBJ_FLAG) {
    flag->set_owner(player->get_index());
    split_path = (map->paths(map->move_down_right(pos)) != 0);
  } else {
    flg_index = map->get_obj_index(map->move_down_right(pos));
    flag = flags[flg_index];
  }

  flag->set_position(map->move_down_right(pos));

  bld->link_flag(flg_index);
  flag->link_building(bld);

  flag->clear_flags();

  map->clear_idle_serf(pos);

  map->set_object(pos, map_obj, bld->get_index());
  map->add_path(pos, DIR_DOWN_RIGHT);

  if (map->get_obj(map->move_down_right(pos)) != MAP_OBJ_FLAG) {
    map->set_object(map->move_down_right(pos), MAP_OBJ_FLAG, flg_index);
    map->add_path(map->move_down_right(pos), DIR_UP_LEFT);
  }

  if (split_path) build_flag_split_path(map->move_down_right(pos));

  return true;
}

/* Build castle at position. */
bool
game_t::build_castle(map_pos_t pos, player_t *player) {
  if (!can_build_castle(pos, player)) {
    return false;
  }

  inventory_t *inventory = inventories.allocate();
  if (inventory == NULL) {
    return false;
  }

  building_t *castle = buildings.allocate();
  if (castle == NULL) {
    inventories.erase(inventory->get_index());
    return false;
  }

  flag_t *flag = flags.allocate();
  if (flag == NULL) {
    buildings.erase(castle->get_index());
    inventories.erase(inventory->get_index());
    return false;
  }

  castle->start_activity();
  castle->serf_arrive();
  castle->set_inventory(inventory);

  inventory->set_building_index(castle->get_index());
  inventory->set_flag_index(flag->get_index());
  inventory->set_owner(player->get_index());
  inventory->apply_supplies_preset(player->get_initial_supplies());

  map->add_gold_deposit(inventory->get_count_of(RESOURCE_GOLDBAR));
  map->add_gold_deposit(inventory->get_count_of(RESOURCE_GOLDORE));

  castle->set_position(pos);
  flag->set_position(map->move_down_right(pos));
  castle->set_owner(player->get_index());
  castle->start_building(BUILDING_CASTLE);

  flag->set_owner(player->get_index());
  flag->set_accepts_serfs(true);
  flag->set_has_inventory();
  flag->set_accepts_resources(true);
  castle->link_flag(flag->get_index());
  flag->link_building(castle);

  map->set_object(pos, MAP_OBJ_CASTLE, castle->get_index());
  map->add_path(pos, DIR_DOWN_RIGHT);

  map->set_object(map->move_down_right(pos), MAP_OBJ_FLAG, flag->get_index());
  map->add_path(map->move_down_right(pos), DIR_UP_LEFT);

  /* Level land in hexagon below castle */
  int h = get_leveling_height(pos);
  map->set_height(pos, h);
  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    map->set_height(map->move(pos, (dir_t)d), h);
  }

  update_land_ownership(pos);

  player->building_founded(castle);

  calculate_military_flag_state(castle);

  return true;
}

void
game_t::flag_remove_player_refs(flag_t *flag) {
  for (players_t::iterator it = players.begin(); it != players.end(); ++it) {
    if ((*it)->temp_index == flag->get_index()) {
      (*it)->temp_index = 0;
    }
  }
}

/* Check whether road can be demolished. */
bool
game_t::can_demolish_road(map_pos_t pos, const player_t *player) {
  if (!map->has_owner(pos) || map->get_owner(pos) != player->get_index()) {
    return false;
  }

  if (map->paths(pos) == 0 ||
      map->has_flag(pos) ||
      map->has_building(pos)) {
    return false;
  }

  return true;
}

/* Check whether flag can be demolished. */
bool
game_t::can_demolish_flag(map_pos_t pos, const player_t *player) {
  if (map->get_obj(pos) != MAP_OBJ_FLAG) return false;

  if (BIT_TEST(map->paths(pos), DIR_UP_LEFT) &&
      map->get_obj(map->move_up_left(pos)) >= MAP_OBJ_SMALL_BUILDING &&
      map->get_obj(map->move_up_left(pos)) <= MAP_OBJ_CASTLE) {
    return false;
  }

  if (map->paths(pos) == 0) return true;

  flag_t *flag = flags[map->get_obj_index(pos)];

  if (flag->get_owner() != player->get_index()) return false;

  return flag->can_demolish();
}

bool
game_t::demolish_flag_(map_pos_t pos) {
  /* Handle any serf at pos. */
  if (map->get_serf_index(pos) != 0) {
    serf_t *serf = serfs[map->get_serf_index(pos)];
    serf->flag_deleted(pos);
  }

  flag_t *flag = flags[map->get_obj_index(pos)];
  assert(!flag->has_building());

  flag_remove_player_refs(flag);

  /* Handle connected flag. */
  flag->merge_paths(pos);

  /* Update serfs with reference to this flag. */
  for (serfs_t::iterator i = serfs.begin(); i != serfs.end(); ++i) {
    serf_t *serf = *i;
    serf->path_merged(flag);
  }

  map->set_object(pos, MAP_OBJ_NONE, 0);

  /* Remove resources from flag. */
  flag->remove_all_resources();

  flags.erase(flag->get_index());

  return true;
}

/* Demolish flag at pos. */
bool
game_t::demolish_flag(map_pos_t pos, player_t *player) {
  if (!can_demolish_flag(pos, player)) return false;

  return demolish_flag_(pos);
}

bool
game_t::demolish_building_(map_pos_t pos) {
  building_t *building = buildings[map->get_obj_index(pos)];

  if (building->is_burning()) return false;

  building_remove_player_refs(building);

  player_t *player = players[building->get_owner()];

  building->burnup();

  /* Remove path to building. */
  map->del_path(pos, DIR_DOWN_RIGHT);
  map->del_path(map->move_down_right(pos), DIR_UP_LEFT);

  /* Disconnect flag. */
  flag_t *flag = flags[building->get_flag_index()];
  flag->unlink_building();
  flag_reset_transport(flag);

  /* Remove lost gold stock from total count. */
  if (building->is_done() &&
      (building->get_type() == BUILDING_HUT ||
       building->get_type() == BUILDING_TOWER ||
       building->get_type() == BUILDING_FORTRESS ||
       building->get_type() == BUILDING_GOLDSMELTER)) {
    int gold_stock = building->get_res_count_in_stock(1);
    map->add_gold_deposit(-gold_stock);
  }

  /* Update land owner ship if the building is military. */
  if (building->is_done() &&
      building->is_active() &&
      building->is_military()) {
    update_land_ownership(building->get_position());
  }

  if (building->is_done() &&
      (building->get_type() == BUILDING_CASTLE ||
       building->get_type() == BUILDING_STOCK)) {
    /* Cancel resources in the out queue and remove gold
       from map total. */
    if (building->is_active()) {
      inventory_t *inventory = building->get_inventory();

      inventory->lose_queue();

      map->add_gold_deposit(-static_cast< int >(inventory->get_count_of(RESOURCE_GOLDBAR)));
      map->add_gold_deposit(-static_cast< int >(inventory->get_count_of(RESOURCE_GOLDORE)));

      inventories.erase(inventory->get_index());
    }

    /* Let some serfs escape while the building is burning. */
    int escaping_serfs = 0;
    for (serfs_t::iterator i = serfs.begin(); i != serfs.end(); ++i) {
      serf_t *serf = *i;
      if (serf->building_deleted(building->get_position(),
                                 escaping_serfs < 12)) {
        escaping_serfs++;
      }
    }
  } else {
    building->stop_activity();
  }

  /* Remove stock from building. */
  building->remove_stock();

  building->stop_playing_sfx();

  int serf_index = building->get_burning_counter();
  building->set_burning_counter(2047);
  building->set_tick(tick);

  player->building_demolished(building);

  if (building->has_serf()) {
    building->serf_gone();

    if (building->is_done() &&
        building->get_type() == BUILDING_CASTLE) {
      building->set_burning_counter(8191);

      for (serfs_t::iterator i = serfs.begin(); i != serfs.end(); ++i) {
        serf_t *serf = *i;
        serf->castle_deleted(building->get_position(), true);
      }
    }

    if (building->is_done() &&
        building->is_military()) {
      while (serf_index != 0) {
        serf_t *serf = serfs[serf_index];
        serf_index = serf->get_next();

        serf->castle_deleted(building->get_position(), false);
      }
    } else {
      serf_t *serf = serfs[serf_index];
      if (serf->get_type() == SERF_TRANSPORTER_INVENTORY) {
        serf->set_type(SERF_TRANSPORTER);
      }

      serf->castle_deleted(building->get_position(), false);
    }
  }

  map_pos_t flag_pos = map->move_down_right(pos);
  if (map->paths(flag_pos) == 0 &&
      map->get_obj(flag_pos) == MAP_OBJ_FLAG) {
    demolish_flag(flag_pos, player);
  }

  return true;
}

/* Demolish building at pos. */
bool
game_t::demolish_building(map_pos_t pos, player_t *player) {
  building_t *building = buildings[map->get_obj_index(pos)];

  if (building->get_owner() != player->get_index()) return false;
  if (building->is_burning()) return false;

  return demolish_building_(pos);
}

/* Calculate the flag state of military buildings (distance to enemy). */
void
game_t::calculate_military_flag_state(building_t *building) {
  const int border_check_offsets[] = {
    31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
    100, 101, 102, 103, 104, 105, 106, 107, 108,
    259, 260, 261, 262, 263, 264,
    241, 242, 243, 244, 245, 246,
    217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228,
    247, 248, 249, 250, 251, 252,
    -1,

    265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276,
    -1,

    277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288,
    289, 290, 291, 292, 293, 294,
    -1
  };

  int f, k;
  for (f = 3, k = 0; f > 0; f--) {
    int offset;
    while ((offset = border_check_offsets[k++]) >= 0) {
      map_pos_t check_pos = map->pos_add_spirally(building->get_position(),
                                                  offset);
      if (map->has_owner(check_pos) &&
          map->get_owner(check_pos) != building->get_owner()) {
        goto break_loops;
      }
    }
  }

break_loops:
  building->set_state(f);
}

/* Map pos is lost to the owner, demolish everything. */
void
game_t::surrender_land(map_pos_t pos) {
  /* Remove building. */
  if (map->get_obj(pos) >= MAP_OBJ_SMALL_BUILDING &&
      map->get_obj(pos) <= MAP_OBJ_CASTLE) {
    demolish_building_(pos);
  }

  if (!map->has_flag(pos) && map->paths(pos) != 0) {
    demolish_road_(pos);
  }

  int remove_roads = map->has_flag(pos);

  /* Remove roads and building around pos. */
  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    map_pos_t p = map->move(pos, (dir_t)d);

    if (map->get_obj(p) >= MAP_OBJ_SMALL_BUILDING &&
        map->get_obj(p) <= MAP_OBJ_CASTLE) {
      demolish_building_(p);
    }

    if (remove_roads &&
        (map->paths(p) & BIT(DIR_REVERSE(d)))) {
      demolish_road_(p);
    }
  }

  /* Remove flag. */
  if (map->get_obj(pos) == MAP_OBJ_FLAG) {
    demolish_flag_(pos);
  }
}

/* Initialize land ownership for whole map. */
void
game_t::init_land_ownership() {
  for (buildings_t::iterator i = buildings.begin(); i != buildings.end(); ++i) {
    building_t *building = *i;
    if (building->is_military()) {
      update_land_ownership(building->get_position());
    }
  }
}

/* Update land ownership around map position. */
void
game_t::update_land_ownership(map_pos_t init_pos) {
  /* Currently the below algorithm will only work when
     both influence_radius and calculate_radius are 8. */
  const int influence_radius = 8;
  const int influence_diameter = 1 + 2*influence_radius;

  int calculate_radius = influence_radius;
  int calculate_diameter = 1 + 2*calculate_radius;

  int *temp_arr = reinterpret_cast<int*>(calloc(GAME_MAX_PLAYER_COUNT*
                                                calculate_diameter*
                                                calculate_diameter,
                                                sizeof(int)));
  if (temp_arr == NULL) abort();

  const int military_influence[] = {
    0, 1, 2, 4, 7, 12, 18, 29, -1, -1,  /* hut */
    0, 3, 5, 8, 11, 15, 22, 30, -1, -1,  /* tower */
    0, 6, 10, 14, 19, 23, 27, 31, -1, -1  /* fortress */
  };

  const int map_closeness[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0,
    1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, 0, 0, 0, 0, 0,
    1, 2, 3, 4, 4, 4, 4, 4, 4, 3, 2, 1, 0, 0, 0, 0, 0,
    1, 2, 3, 4, 5, 5, 5, 5, 5, 4, 3, 2, 1, 0, 0, 0, 0,
    1, 2, 3, 4, 5, 6, 6, 6, 6, 5, 4, 3, 2, 1, 0, 0, 0,
    1, 2, 3, 4, 5, 6, 7, 7, 7, 6, 5, 4, 3, 2, 1, 0, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 7, 6, 5, 4, 3, 2, 1,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 2, 1,
    0, 0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 6, 5, 4, 3, 2, 1,
    0, 0, 0, 1, 2, 3, 4, 5, 6, 6, 6, 6, 5, 4, 3, 2, 1,
    0, 0, 0, 0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 4, 3, 2, 1,
    0, 0, 0, 0, 0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 3, 2, 1,
    0, 0, 0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1,
    0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1
  };

  /* Find influence from buildings in 33*33 square
     around the center. */
  for (int i = -(influence_radius+calculate_radius);
       i <= influence_radius+calculate_radius; i++) {
    for (int j = -(influence_radius+calculate_radius);
         j <= influence_radius+calculate_radius; j++) {
      map_pos_t pos = map->pos_add(init_pos, map->pos(j & map->get_col_mask(),
                                                      i & map->get_row_mask()));

      if (map->get_obj(pos) >= MAP_OBJ_SMALL_BUILDING &&
          map->get_obj(pos) <= MAP_OBJ_CASTLE &&
          BIT_TEST(map->paths(pos), DIR_DOWN_RIGHT)) {  // TODO(_): Why wouldn't
                                                        //          this be set?
        building_t *building = buildings[map->get_obj_index(pos)];
        int mil_type = -1;

        if (building->get_type() == BUILDING_CASTLE) {
          /* Castle has military influence even when not done. */
          mil_type = 2;
        } else if (building->is_done() &&
                   building->is_active()) {
          switch (building->get_type()) {
          case BUILDING_HUT: mil_type = 0; break;
          case BUILDING_TOWER: mil_type = 1; break;
          case BUILDING_FORTRESS: mil_type = 2; break;
          default: break;
          }
        }

        if (mil_type >= 0 &&
            !building->is_burning()) {
          const int *influence = military_influence + 10*mil_type;
          const int *closeness = map_closeness +
            influence_diameter*std::max(-i, 0) + std::max(-j, 0);
          int *arr = temp_arr +
            (building->get_owner() * calculate_diameter*calculate_diameter) +
            calculate_diameter * std::max(i, 0) + std::max(j, 0);

          for (int k = 0; k < influence_diameter - abs(i); k++) {
            for (int l = 0; l < influence_diameter - abs(j); l++) {
              int inf = influence[*closeness];
              if (inf < 0) *arr = 128;
              else if (*arr < 128) *arr = std::min(*arr + inf, 127);

              closeness += 1;
              arr += 1;
            }
            closeness += abs(j);
            arr += abs(j);
          }
        }
      }
    }
  }

  /* Update owner of 17*17 square. */
  for (int i = -calculate_radius; i <= calculate_radius; i++) {
    for (int j = -calculate_radius; j <= calculate_radius; j++) {
      int max_val = 0;
      int player = -1;
      for (players_t::iterator it = players.begin();
           it != players.end(); ++it) {
        int *arr = temp_arr +
          (*it)->get_index()*calculate_diameter*calculate_diameter +
          calculate_diameter*(i+calculate_radius) + (j+calculate_radius);
        if (*arr > max_val) {
          max_val = *arr;
          player = (*it)->get_index();
        }
      }

      map_pos_t pos = map->pos_add(init_pos, map->pos(j & map->get_col_mask(),
                                                      i & map->get_row_mask()));
      int old_player = -1;
      if (map->has_owner(pos)) old_player = map->get_owner(pos);

      if (old_player >= 0 && player != old_player) {
        players[old_player]->decrease_land_area();
        surrender_land(pos);
      }

      if (player >= 0) {
        if (player != old_player) {
          players[player]->increase_land_area();
          map->set_owner(pos, player);
        }
      } else {
        map->del_owner(pos);
      }
    }
  }

  free(temp_arr);

  /* Update military building flag state. */
  for (int i = -25; i <= 25; i++) {
    for (int j = -25; j <= 25; j++) {
      map_pos_t pos = map->pos_add(init_pos, map->pos(i & map->get_col_mask(),
                                                      j & map->get_row_mask()));

      if (map->get_obj(pos) >= MAP_OBJ_SMALL_BUILDING &&
          map->get_obj(pos) <= MAP_OBJ_CASTLE &&
          BIT_TEST(map->paths(pos), DIR_DOWN_RIGHT)) {
        building_t *building = buildings[map->get_obj_index(pos)];
        if (building->is_done() && building->is_military()) {
          calculate_military_flag_state(building);
        }
      }
    }
  }
}

void
game_t::demolish_flag_and_roads(map_pos_t pos) {
  if (map->has_flag(pos)) {
    /* Remove roads around pos. */
    for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
      map_pos_t p = map->move(pos, (dir_t)d);

      if (map->paths(p) & BIT(DIR_REVERSE(d))) {
        demolish_road_(p);
      }
    }

    if (map->get_obj(pos) == MAP_OBJ_FLAG) {
      demolish_flag_(pos);
    }
  } else if (map->paths(pos) != 0) {
    demolish_road_(pos);
  }
}

/* The given building has been defeated and is being
   occupied by player. */
void
game_t::occupy_enemy_building(building_t *building, int player_num) {
  /* Take the building. */
  player_t *player = players[player_num];

  player->building_captured(building);

  if (building->get_type() == BUILDING_CASTLE) {
    demolish_building_(building->get_position());
  } else {
    flag_t *flag = flags[building->get_flag_index()];
    flag_reset_transport(flag);

    /* Demolish nearby buildings. */
    for (int i = 0; i < 12; i++) {
      map_pos_t pos = map->pos_add_spirally(building->get_position(), 7+i);
      if (map->get_obj(pos) >= MAP_OBJ_SMALL_BUILDING &&
          map->get_obj(pos) <= MAP_OBJ_CASTLE) {
        demolish_building_(pos);
      }
    }

    /* Change owner of land and remove roads and flags
       except the flag associated with the building. */
    map->set_owner(building->get_position(), player_num);

    for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
      map_pos_t pos = map->move(building->get_position(), (dir_t)d);
      map->set_owner(pos, player_num);
      if (pos != flag->get_position()) {
        demolish_flag_and_roads(pos);
      }
    }

    /* Change owner of flag. */
    flag->set_owner(player_num);

    /* Reset destination of stolen resources. */
    flag->reset_destination_of_stolen_resources();

    /* Remove paths from flag. */
    for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
      if (flag->has_path((dir_t)d)) {
        demolish_road_(map->move(flag->get_position(), (dir_t)d));
      }
    }

    update_land_ownership(building->get_position());
  }
}

/* mode: 0: IN, 1: STOP, 2: OUT */
void
game_t::set_inventory_resource_mode(inventory_t *inventory, int mode) {
  flag_t *flag = flags[inventory->get_flag_index()];

  if (mode == 0) {
    inventory->set_res_mode(mode_in);
  } else if (mode == 1) {
    inventory->set_res_mode(mode_stop);
  } else {
    inventory->set_res_mode(mode_out);
  }

  if (mode > 0) {
    flag->set_accepts_resources(false);

    /* Clear destination of serfs with resources destined
       for this inventory. */
    int dest = flag->get_index();
    for (serfs_t::iterator i = serfs.begin(); i != serfs.end(); ++i) {
      serf_t *serf = *i;
      serf->clear_destination2(dest);
    }
  } else {
    flag->set_accepts_resources(true);
  }
}

/* mode: 0: IN, 1: STOP, 2: OUT */
void
game_t::set_inventory_serf_mode(inventory_t *inventory, int mode) {
  flag_t *flag = flags[inventory->get_flag_index()];

  if (mode == 0) {
    inventory->set_serf_mode(mode_in);
  } else if (mode == 1) {
    inventory->set_serf_mode(mode_stop);
  } else {
    inventory->set_serf_mode(mode_out);
  }

  if (mode > 0) {
    flag->set_accepts_serfs(false);

    /* Clear destination of serfs destined for this inventory. */
    int dest = flag->get_index();
    for (serfs_t::iterator i = serfs.begin(); i != serfs.end(); ++i) {
      serf_t *serf = *i;
      serf->clear_destination(dest);
    }
  } else {
    flag->set_accepts_serfs(true);
  }
}

/* Add new player to the game. Returns the player number
   or negative on error. */
unsigned int
game_t::add_player(size_t face, unsigned int color, size_t supplies,
                   size_t reproduction, size_t intelligence) {
  /* Allocate object */
  player_t *player = players.allocate();
  if (player == NULL) abort();

  player->init(player->get_index(), face, color, supplies, reproduction,
               intelligence);

  /* Update map values dependent on player count */
  map_gold_morale_factor = 10 * 1024 * static_cast<int>(players.size());

  return player->get_index();
}

void
game_t::init() {
  /* Initialize global lookup tables */
  game_speed = DEFAULT_GAME_SPEED;

  update_map_last_tick = 0;
  update_map_counter = 0;
  update_map_16_loop = 0;
  update_map_initial_pos = 0;
  //next_index = 0;

  memset(player_history_index, '\0', sizeof(player_history_index));
  memset(player_history_counter, '\0', sizeof(player_history_counter));
  resource_history_index = 0;

  tick = 0;
  const_tick = 0;
  tick_diff = 0;

  game_stats_counter = 0;
  history_counter = 0;

  knight_morale_counter = 0;
  inventory_schedule_counter = 0;

  mission_level = -1;
  mission_is_tutorial = false;
}

/* Initialize spiral_pos_pattern from spiral_pattern. */
void
game_t::init_map(int size, const random_state_t &rnd, bool preserve_bugs) {
  if (map != NULL) {
    delete map;
    map = NULL;
  }

  map = new map_t();
  map->init(size);
  map->generate(map_generator, rnd, preserve_bugs);
}

void
game_t::deinit() {
  while (serfs.size()) {
    serfs_t::iterator it = serfs.begin();
    serfs.erase((*it)->get_index());
  }

  while (buildings.size()) {
    buildings_t::iterator it = buildings.begin();
    buildings.erase((*it)->get_index());
  }

  while (inventories.size()) {
    inventories_t::iterator it = inventories.begin();
    inventories.erase((*it)->get_index());
  }

  while (flags.size()) {
    flags_t::iterator it = flags.begin();
    flags.erase((*it)->get_index());
  }

  while (players.size()) {
    players_t::iterator it = players.begin();
    players.erase((*it)->get_index());
  }

  if (map != NULL) {
    delete map;
    map = NULL;
  }
}

void
game_t::allocate_objects() {
  /* Create NULL-serf */
  serfs.allocate();

  /* Create NULL-building (index 0 is undefined) */
  buildings.allocate();

  /* Create NULL-flag (index 0 is undefined) */
  flags.allocate();
}

bool
game_t::load_mission_map(int level, bool load_tutorial) {
  const unsigned int default_player_colors[] = {
    64, 72, 68, 76
  };

  deinit();

  mission_t *mission;

  if (load_tutorial) {
    mission = mission_t::get_tutorial(level);
  }
  else {
    mission = mission_t::get_mission(level);
  }
  

  init_map_rnd = mission->rnd;

  mission_level = level;
  mission_is_tutorial = load_tutorial;

  init_map(3, mission->rnd, true);
  allocate_objects();

  /* Initialize player and build initial castle */
  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    size_t face = (i == 0) ? 12 : mission->player[i].face;
    if (face == 0) continue;

    int n = add_player(face, default_player_colors[i],
                       mission->player[i].supplies,
                       mission->player[i].reproduction,
                       mission->player[i].intelligence);
    if (n < 0) return false;

    if (mission->player[i].castle.col > -1 &&
        mission->player[i].castle.row > -1) {
      map_pos_t pos = map->pos(mission->player[i].castle.col,
                               mission->player[i].castle.row);
      build_castle(pos, players[n]);
    }
  }

  return true;
}

bool
game_t::load_random_map(int size, const random_state_t &rnd) {
  if (size < 3 || size > 10) return false;

  init_map(size, rnd, false);
  allocate_objects();

  return true;
}

bool
game_t::load_save_game(const std::string &path) {
  if (!load_state(path, this)) {
    return false;
  }

  init_land_ownership();

  return true;
}

/* Cancel a resource being transported to destination. This
   ensures that the destination can request a new resource. */
void
game_t::cancel_transported_resource(resource_type_t res, unsigned int dest) {
  if (dest == 0) return;

  flag_t *flag = flags[dest];
  assert(flag->has_building());
  building_t *building = flag->get_building();
  building->cancel_transported_resource(res);
}

/* Called when a resource is lost forever from the game. This will
   update any global state keeping track of that resource. */
void
game_t::lose_resource(resource_type_t res) {
  if (res == RESOURCE_GOLDORE ||
      res == RESOURCE_GOLDBAR) {
    map->add_gold_deposit(-1);
  }
}

uint16_t
game_t::random_int() {
  return init_map_rnd.random();
}

bool
game_t::handle_event(const event_t *event) {
  switch (event->type) {
    case EVENT_UPDATE:
      update();
      return true;
      break;
    default:
      break;
  }
  return false;
}

int
game_t::next_search_id() {
  flag_search_counter += 1;

  /* If we're back at zero the counter has overflown,
   everything needs a reset to be safe. */
  if (flag_search_counter == 0) {
    flag_search_counter += 1;
    clear_search_id();
  }

  return flag_search_counter;
}

serf_t *
game_t::create_serf(int index) {
  if (index == -1) {
    return serfs.allocate();
  } else {
    return serfs.get_or_insert(index);
  }
}

void
game_t::delete_serf(serf_t *serf) {
  serfs.erase(serf->get_index());
}

flag_t *
game_t::create_flag(int index) {
  if (index == -1) {
    return flags.allocate();
  } else {
    return flags.get_or_insert(index);
  }
}

inventory_t *
game_t::create_inventory(int index) {
  if (index == -1) {
    return inventories.allocate();
  } else {
    return inventories.get_or_insert(index);
  }
}

building_t *
game_t::create_building(int index) {
  if (index == -1) {
    return buildings.allocate();
  } else {
    return buildings.get_or_insert(index);
  }
}

void
game_t::delete_building(building_t *building) {
  map->set_object(building->get_position(), MAP_OBJ_NONE, 0);
  buildings.erase(building->get_index());
}

list_serfs_t
game_t::get_player_serfs(player_t *player) {
  list_serfs_t player_serfs;

  for (serfs_t::iterator i = serfs.begin(); i != serfs.end(); ++i) {
    serf_t *serf = *i;
    if (serf->get_player() == player->get_index()) {
      player_serfs.push_back(serf);
    }
  }

  return player_serfs;
}

list_buildings_t
game_t::get_player_buildings(player_t *player) {
  list_buildings_t player_buildings;

  for (buildings_t::iterator i = buildings.begin(); i != buildings.end(); ++i) {
    building_t *building = *i;
    if (building->get_owner() == player->get_index()) {
      player_buildings.push_back(building);
    }
  }

  return player_buildings;
}

list_inventories_t
game_t::get_player_inventories(player_t *player) {
  list_inventories_t player_inventories;

  for (inventories_t::iterator i = inventories.begin();
       i != inventories.end(); ++i) {
    inventory_t *inventory = *i;
    if (inventory->get_owner() == player->get_index()) {
      player_inventories.push_back(inventory);
    }
  }

  return player_inventories;
}

list_serfs_t
game_t::get_serfs_at_pos(map_pos_t pos) {
  list_serfs_t result;

  for (serfs_t::iterator i = serfs.begin(); i != serfs.end(); ++i) {
    serf_t *serf = *i;
    if (serf->get_pos() == pos) {
      result.push_back(serf);
    }
  }

  return result;
}

list_serfs_t
game_t::get_serfs_in_inventory(inventory_t *inventory) {
  list_serfs_t result;

  for (serfs_t::iterator i = serfs.begin(); i != serfs.end(); ++i) {
    serf_t *serf = *i;
    if (serf->get_state() == SERF_STATE_IDLE_IN_STOCK &&
        inventory->get_index() == serf->get_idle_in_stock_inv_index()) {
      result.push_back(serf);
    }
  }

  return result;
}

list_serfs_t
game_t::get_serfs_related_to(unsigned int dest, dir_t dir) {
  list_serfs_t result;

  for (serfs_t::iterator it = serfs.begin(); it != serfs.end(); ++it) {
    serf_t *serf = *it;
    if (serf->is_related_to(dest, dir)) {
      result.push_back(serf);
    }
  }

  return result;
}

flag_t *
game_t::gat_flag_at_pos(map_pos_t pos) {
  return flags[map->get_obj_index(pos)];
}

player_t *
game_t::get_next_player(player_t *player) {
  players_t::iterator p = players.begin();
  while (*p != player) {
    ++p;
  }
  ++p;
  if (p == players.end()) {
    p = players.begin();
  }

  return (*p);
}

unsigned int
game_t::get_enemy_score(player_t *player) {
  unsigned int enemy_score = 0;

  for (players_t::iterator it = players.begin(); it != players.end(); ++it) {
    if (player->get_index() != (*it)->get_index()) {
      enemy_score += (*it)->get_total_military_score();
    }
  }

  return enemy_score;
}

void
game_t::building_captured(building_t *building) {
  /* Save amount of land and buildings for each player */
  std::map<int, int> land_before;
  std::map<int, int> buildings_before;
  for (players_t::iterator it = players.begin(); it != players.end(); ++it) {
    player_t *player = *it;
    land_before[player->get_index()] = player->get_land_area();
    buildings_before[player->get_index()] = player->get_building_score();
  }

  /* Update land ownership */
  update_land_ownership(building->get_position());

  /* Create notfications for lost land and buildings */
  for (players_t::iterator it = players.begin(); it != players.end(); ++it) {
    player_t *player = *it;
    if (buildings_before[player->get_index()] > player->get_building_score()) {
      player->add_notification(9, building->get_position(),
                               building->get_owner());
    } else if (land_before[player->get_index()] > player->get_land_area()) {
      player->add_notification(8, building->get_position(),
                               building->get_owner());
    }
  }
}

void
game_t::clear_search_id() {
  for (flags_t::iterator i = flags.begin(); i != flags.end(); ++i) {
    flag_t *flag = *i;
    flag->clear_search_id();
  }
}

save_reader_binary_t&
operator >> (save_reader_binary_t &reader, game_t &game) {
  /* Load these first so map dimensions can be reconstructed.
   This is necessary to load map positions. */

  reader.skip(74);
  uint16_t v16;
  reader >> v16;  // 74
  game.game_type = v16;
  reader >> v16;  // 76
  reader >> v16;  // 78
  game.tick = v16;
  game.game_stats_counter = 0;
  game.history_counter = 0;

  reader.skip(4);

  uint16_t r1, r2, r3;
  reader >> r1;  // 84
  reader >> r2;  // 86
  reader >> r3;  // 88
  game.rnd = random_state_t(r1, r2, r3);

  reader >> v16;  // 90
  int max_flag_index = v16;
  reader >> v16;  // 92
  int max_building_index = v16;
  reader >> v16;  // 94
  int max_serf_index = v16;

  reader >> v16;  // 96
  //game.next_index = v16;
  reader >> v16;  // 98
  game.flag_search_counter = v16;

  reader.skip(4);

  for (int i = 0; i < 4; i++) {
    reader >> v16;  // 104 + i*2
    game.player_history_index[i] = v16;
  }

  for (int i = 0; i < 3; i++) {
    reader >> v16;  // 112 + i*2
    game.player_history_counter[i] = v16;
  }

  reader >> v16;  // 118
  game.resource_history_index = v16;

//  if (0/*game.game_type == GAME_TYPE_TUTORIAL*/) {
//    game.tutorial_level = *reinterpret_cast<uint16_t*>(&data[122]);
//  } else if (0/*game.game_type == GAME_TYPE_MISSION*/) {
//    game.mission_level = *reinterpret_cast<uint16_t*>(&data[124]);
//  }

  reader.skip(54);

  reader >> v16;  // 174
  int max_inventory_index = v16;

  reader.skip(4);
  reader >> v16;  // 180
  game.max_next_index = v16;

  reader.skip(8);
  reader >> v16;  // 190
  game.map = new map_t();
  game.map->init(v16);

  reader.skip(8);
  reader >> v16;  // 200
  game.map_gold_morale_factor = v16;
  reader.skip(2);
  uint8_t v8;
  reader >> v8;  // 204
  game.player_score_leader = v8;

  reader.skip(45);

  /* Load players state from save game. */
  for (int i = 0; i < 4; i++) {
    save_reader_binary_t player_reader = reader.extract(8628);
    player_reader.skip(130);
    player_reader >> v8;
    if (BIT_TEST(v8, 6)) {
      player_reader.reset();
      player_t *player = game.players.get_or_insert(i);
      player_reader >> *player;
    }
  }

  /* Load map state from save game. */
  unsigned int tile_count = game.map->get_cols() * game.map->get_rows();
  save_reader_binary_t map_reader = reader.extract(8 * tile_count);
  map_reader >> *(game.map);

  game.load_serfs(&reader, max_serf_index);
  game.load_flags(&reader, max_flag_index);
  game.load_buildings(&reader, max_building_index);
  game.load_inventories(&reader, max_inventory_index);

  game.game_speed = 0;
  game.game_speed_save = DEFAULT_GAME_SPEED;

  return reader;
}

/* Load serf state from save game. */
bool
game_t::load_serfs(save_reader_binary_t *reader, int max_serf_index) {
  /* Load serf bitmap. */
  int bitmap_size = 4*((max_serf_index + 31)/32);
  uint8_t *bitmap = reader->read(bitmap_size);
  if (bitmap == NULL) return false;

  /* Load serf data. */
  for (int i = 0; i < max_serf_index; i++) {
    save_reader_binary_t serf_reader = reader->extract(16);
    if (BIT_TEST(bitmap[(i)>>3], 7-((i)&7))) {
      serf_t *serf = serfs.get_or_insert(i);
      serf_reader >> *serf;
    }
  }

  return true;
}

/* Load flags state from save game. */
bool
game_t::load_flags(save_reader_binary_t *reader, int max_flag_index) {
  /* Load flag bitmap. */
  int bitmap_size = 4*((max_flag_index + 31)/32);
  uint8_t *bitmap = reader->read(bitmap_size);
  if (bitmap == NULL) return false;

  /* Load flag data. */
  for (int i = 0; i < max_flag_index; i++) {
    save_reader_binary_t flag_reader = reader->extract(70);
    if (BIT_TEST(bitmap[(i)>>3], 7-((i)&7))) {
      flag_t *flag = flags.get_or_insert(i);
      flag_reader >> *flag;
    }
  }

  /* Set flag positions. */
  for (unsigned int y = 0; y < map->get_rows(); y++) {
    for (unsigned int x = 0; x < map->get_cols(); x++) {
      map_pos_t pos = map->pos(x, y);
      if (map->get_obj(pos) == MAP_OBJ_FLAG) {
        flag_t *flag = flags[map->get_obj_index(pos)];
        flag->set_position(pos);
      }
    }
  }

  return true;
}

/* Load buildings state from save game. */
bool
game_t::load_buildings(save_reader_binary_t *reader, int max_building_index) {
  /* Load building bitmap. */
  int bitmap_size = 4*((max_building_index + 31)/32);
  uint8_t *bitmap = reader->read(bitmap_size);
  if (bitmap == NULL) return false;

  /* Load building data. */
  for (int i = 0; i < max_building_index; i++) {
    save_reader_binary_t building_reader = reader->extract(18);
    if (BIT_TEST(bitmap[(i)>>3], 7-((i)&7))) {
      building_t *building = buildings.get_or_insert(i);
      building_reader >> *building;
    }
  }

  return true;
}

/* Load inventories state from save game. */
bool
game_t::load_inventories(save_reader_binary_t *reader,
                         int max_inventory_index) {
  /* Load inventory bitmap. */
  int bitmap_size = 4*((max_inventory_index + 31)/32);
  uint8_t *bitmap = reader->read(bitmap_size);
  if (bitmap == NULL) return false;

  /* Load inventory data. */
  for (int i = 0; i < max_inventory_index; i++) {
    save_reader_binary_t inventory_reader = reader->extract(120);
    if (BIT_TEST(bitmap[(i)>>3], 7-((i)&7))) {
      inventory_t *inventory = inventories.get_or_insert(i);
      inventory_reader >> *inventory;
    }
  }

  return true;
}

building_t *
game_t::get_building_at_pos(map_pos_t pos) {
  return buildings[map->get_obj_index(pos)];
}

flag_t *
game_t::get_flag_at_pos(map_pos_t pos) {
  return flags[map->get_obj_index(pos)];
}

serf_t *
game_t::get_serf_at_pos(map_pos_t pos) {
  return serfs[map->get_serf_index(pos)];
}

save_reader_text_t&
operator >> (save_reader_text_t &reader, game_t &game) {
  /* Load essential values for calculating map positions
   so that map positions can be loaded properly. */
  readers_t sections = reader.get_sections("game");
  save_reader_text_t *game_reader = sections.front();

  unsigned int size = 0;
  try {
    game_reader->value("map.size") >> size;
  } catch (...) {
    unsigned int col_size = 0;
    unsigned int row_size = 0;
    game_reader->value("map.col_size") >> col_size;
    game_reader->value("map.row_size") >> row_size;
    size = (col_size + row_size) - 9;
  }

  /* Initialize remaining map dimensions. */
  game.map = new map_t();
  game.map->init(size);
  game.map->init_dimensions();
  sections = reader.get_sections("map");
  for (readers_t::iterator it = sections.begin();
       it != sections.end(); ++it) {
    **it >> *game.map;
  }

//  std::string version;
//  reader.value("version") >> version;
//  LOGV("savegame", "Loading save game from version %s.", version.c_str());

  game_reader->value("game_type") >> game.game_type;
  game_reader->value("tick") >> game.tick;
  game_reader->value("game_stats_counter") >> game.game_stats_counter;
  game_reader->value("history_counter") >> game.history_counter;
  std::string rnd_str;
  try {
    game_reader->value("random") >> rnd_str;
    game.rnd = random_state_t(rnd_str);
  } catch (...) {
    game_reader->value("rnd") >> rnd_str;
    std::stringstream ss;
    ss << rnd_str;
    uint16_t r1, r2, r3;
    char c;
    ss >> r1 >> c >> r2 >> c >> r3;
    game.rnd = random_state_t(r1, r2, r3);
  }
  //game_reader->value("next_index") >> game.next_index;
  game_reader->value("flag_search_counter") >> game.flag_search_counter;
  for (int i = 0; i < 4; i++) {
    game_reader->value("player_history_index")[i] >>
                                                   game.player_history_index[i];
  }
  for (int i = 0; i < 3; i++) {
    game_reader->value("player_history_counter")[i] >>
                                                 game.player_history_counter[i];
  }
  game_reader->value("resource_history_index") >> game.resource_history_index;
  game_reader->value("max_next_index") >> game.max_next_index;
  game_reader->value("map.gold_morale_factor") >> game.map_gold_morale_factor;
  game_reader->value("player_score_leader") >> game.player_score_leader;

  /* Allocate game objects */
  game.allocate_objects();

  sections = reader.get_sections("player");
  for (readers_t::iterator it = sections.begin();
       it != sections.end(); ++it) {
    player_t *p = game.players.get_or_insert((*it)->get_number());
    **it >> *p;
  }

  sections = reader.get_sections("flag");
  for (readers_t::iterator it = sections.begin();
       it != sections.end(); ++it) {
    flag_t *p = game.flags.get_or_insert((*it)->get_number());
    **it >> *p;
  }

  sections = reader.get_sections("building");
  for (readers_t::iterator it = sections.begin();
       it != sections.end(); ++it) {
    building_t *p = game.buildings.get_or_insert((*it)->get_number());
    **it >> *p;
  }

  sections = reader.get_sections("inventory");
  for (readers_t::iterator it = sections.begin();
       it != sections.end(); ++it) {
    inventory_t *p = game.inventories.get_or_insert((*it)->get_number());
    **it >> *p;
  }

  sections = reader.get_sections("serf");
  for (readers_t::iterator it = sections.begin();
       it != sections.end(); ++it) {
    serf_t *p = game.serfs.get_or_insert((*it)->get_number());
    **it >> *p;
  }

  /* Restore idle serf flag */
  for (serfs_t::iterator i = game.serfs.begin(); i != game.serfs.end(); ++i) {
    serf_t *serf = *i;
    if (serf->get_index() == 0) continue;

    if (serf->get_state() == SERF_STATE_IDLE_ON_PATH ||
        serf->get_state() == SERF_STATE_WAIT_IDLE_ON_PATH) {
      game.map->set_idle_serf(serf->get_pos());
    }
  }

  /* Restore building index */
  for (buildings_t::iterator i = game.buildings.begin();
       i != game.buildings.end(); ++i) {
    building_t *building = *i;
    if (building->get_index() == 0) continue;

    if (game.map->get_obj(building->get_position()) < MAP_OBJ_SMALL_BUILDING ||
        game.map->get_obj(building->get_position()) > MAP_OBJ_CASTLE) {
      throw Freeserf_Exception("Data integrity is broken");
    }

    game.map->set_obj_index(building->get_position(), building->get_index());
  }

  /* Restore flag index */
  for (flags_t::iterator i = game.flags.begin(); i != game.flags.end(); ++i) {
    flag_t *flag = *i;
    if (flag->get_index() == 0) continue;

    if (game.map->get_obj(flag->get_position()) != MAP_OBJ_FLAG) {
      throw Freeserf_Exception("Data integrity is broken");
    }

    game.map->set_obj_index(flag->get_position(), flag->get_index());
  }

  game.game_speed = 0;
  game.game_speed_save = DEFAULT_GAME_SPEED;

  return reader;
}

save_writer_text_t&
operator << (save_writer_text_t &writer, game_t &game) {
  writer.value("map.size") << game.map->get_size();
  writer.value("game_type") << game.game_type;
  writer.value("tick") << game.tick;
  writer.value("game_stats_counter") << game.game_stats_counter;
  writer.value("history_counter") << game.history_counter;
  writer.value("random") << (std::string)game.rnd;

  //writer.value("next_index") << game.next_index;
  writer.value("flag_search_counter") << game.flag_search_counter;

  for (int i = 0; i < 4; i++) {
    writer.value("player_history_index") << game.player_history_index[i];
  }
  for (int i = 0; i < 3; i++) {
    writer.value("player_history_counter") << game.player_history_counter[i];
  }
  writer.value("resource_history_index") << game.resource_history_index;

  writer.value("max_next_index") << game.max_next_index;
  writer.value("map.gold_morale_factor") << game.map_gold_morale_factor;
  writer.value("player_score_leader") << game.player_score_leader;

  for (players_t::iterator p = game.players.begin();
       p != game.players.end(); ++p) {
    save_writer_text_t &player_writer = writer.add_section("player",
                                                           (*p)->get_index());
    player_writer << **p;
  }

  for (flags_t::iterator f = game.flags.begin(); f != game.flags.end(); ++f) {
    if ((*f)->get_index() == 0) continue;
    save_writer_text_t &flag_writer = writer.add_section("flag",
                                                         (*f)->get_index());
    flag_writer << **f;
  }

  for (buildings_t::iterator b = game.buildings.begin();
       b != game.buildings.end(); ++b) {
    if ((*b)->get_index() == 0) continue;
    save_writer_text_t &building_writer = writer.add_section("building",
                                                             (*b)->get_index());
    building_writer << **b;
  }

  for (inventories_t::iterator i = game.inventories.begin();
       i != game.inventories.end(); ++i) {
    save_writer_text_t &inventory_writer = writer.add_section("inventory",
                                                             (*i)->get_index());
    inventory_writer << **i;
  }

  for (serfs_t::iterator s = game.serfs.begin(); s != game.serfs.end(); ++s) {
    if ((*s)->get_index() == 0) continue;
    save_writer_text_t &serf_writer = writer.add_section("serf",
                                                         (*s)->get_index());
    serf_writer << **s;
  }

  writer << *game.map;

  return writer;
}
