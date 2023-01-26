/*
 * player.cc - Player related functions
 *
 * Copyright (C) 2013-2017  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/player.h"

#include <algorithm>
#include <thread>        //NOLINT (build/c++11) this is a Google Chromium req, not relevant to general C++.  // for AI threads

#include "src/game.h"
#include "src/log.h"
#include "src/inventory.h"
#include "src/savegame.h"
#include "src/building.h"

#include "src/version.h"  // for game_ticks_per_update to adjust for time warp game_speeds

Player::Player(Game* game, unsigned int index)
  : GameObject(game, index)
  , tool_prio{}
  , resource_count{}
  , flag_prio{}
  , serf_count{}
  , completed_building_count{}
  , incomplete_building_count{}
  , inventory_prio{}
  , attacking_buildings{}
  , food_stonemine(0)
  , food_coalmine(0)
  , food_ironmine(0)
  , food_goldmine(0)
  , planks_construction(0)
  , planks_boatbuilder(0)
  , planks_toolmaker(0)
  , steel_toolmaker(0)
  , steel_weaponsmith(0)
  , coal_steelsmelter(0)
  , coal_goldsmelter(0)
  , coal_weaponsmith(0)
  , wheat_pigfarm(0)
  , wheat_mill(0)
  , player_stat_history{}
  , resource_count_history{}
  , attacking_knights{} {
  build = 0;
  color = { 0 };
  face = -1;
  flags = 0;
  castle_inventory = 0;
  reproduction_counter = 0;
  reproduction_reset = 0;
  analysis_coal = 0;
  analysis_stone = 0;
  analysis_ironore = 0;
  analysis_goldore = 0;
  initial_supplies = 0;
  knight_cycle_counter = 0;
  knight_morale = 0;
  gold_deposited = 0;
  ai_value_0 = 0;
  ai_value_1 = 0;
  ai_value_2 = 0;
  ai_value_3 = 0;
  ai_value_4 = 0;
  ai_value_5 = 0;
  ai_intelligence = 0;
  temp_index = 0;

  building = 0;
  cont_search_after_non_optimal_find = 7;
  knights_to_spawn = 0;
  total_land_area = 0;
  total_military_score = 0;
  total_building_score = 0;

  last_tick = 0;

  serf_to_knight_rate = 20000;
  serf_to_knight_counter = 0x8000;

  knight_occupation[0] = 0x10;
  knight_occupation[1] = 0x21;
  knight_occupation[2] = 0x32;
  knight_occupation[3] = 0x43;

  total_attacking_knights = 0;
  for (int i = 0; i < 4; i++) {
    attacking_knights[i] = 0;
  }

  attacking_building_count = 0;
  for (int i = 0; i < 64; i++) {
    attacking_buildings[i] = 0;
  }

  building_attacked = 0;
  knights_attacking = 0;

  reset_food_priority();
  reset_planks_priority();
  reset_steel_priority();
  reset_coal_priority();
  reset_wheat_priority();
  reset_tool_priority();

  reset_flag_priority();
  reset_inventory_priority();

  military_max_gold = 0;

  castle_knights_wanted = 3;
  castle_knights = 0;
  send_knight_delay = 0;  // game_speed adjusted for time warp speeds
  send_generic_delay = 0;  // game_speed adjusted for time warp speeds
  serf_index = 0;

  /* player->field_1b0 = 0; AI */
  /* player->field_1b2 = 0; AI */

  castle_score = 0;

  for (int i = 0; i < 26; i++) {
    resource_count[i] = 0;
  }

  for (int i = 0; i < 24; i++) {
    completed_building_count[i] = 0;
    incomplete_building_count[i] = 0;
  }

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 112; j++) {
      player_stat_history[i][j] = 0;
    }
  }

  for (int i = 0; i < 26; i++) {
    for (int j = 0; j < 120; j++) {
      resource_count_history[i][j] = 0;
    }
  }

  for (int i = 0; i < 27; i++) {
    serf_count[i] = 0;
  }

  /* TODO AI: Set array field_402 of length 25 to -1. */
  /* TODO AI: Set array field_434 of length 280*2 to 0 */
  /* TODO AI: Set array field_1bc of length 8 to -1 */
}

// Initialize player values.
//
// Supplies and reproduction are usually limited to 0-40 in random map games.
//
// Args:
//     face: the face image that represents this player.
//           1-12 is AI, 13-14 is human player.
//     color: Color of player as palette color index.
//     supplies: Initial resource supplies at castle (0-50).
//     reproduction: How quickly new serfs spawn during the game (0-60).
//     intelligence: AI only (unused) (0-40).
void
Player::init(unsigned int _intelligence, unsigned int _supplies,
             unsigned int _reproduction) {
  flags = 0;

  initial_supplies = _supplies;
  reproduction_reset = (60 - (size_t)_reproduction) * 50;
  ai_intelligence = (1300 * (size_t)_intelligence) + 13535;
  reproduction_counter = static_cast<int>(reproduction_reset);

  // for option_FogOfWar
  //  create a player-specific copy of the Map
  // THIS IS MORE COMPLEX THAN I INITIALLY THOUGHT, NEEDS DEVELOPMENT
  //  BUT THIS IS THE PLACE TO CREATE THE INITIAL COPY OF THE MAP... maybe?
  //known_map = game->get_map();
  
}

void
Player::init_view(Color _color, unsigned int _face) {
  face = _face;

  if (face < 12) { /* AI player */
    flags |= BIT(7); /* Set AI bit */
    /* TODO ... */
    /*game.max_next_index = 49;*/
  }

  if (is_ai()) init_ai_values(face);

  color = _color;
}

/* Initialize AI parameters. */
void
Player::init_ai_values(size_t face_) {
  const int ai_values_0[] = { 13, 10, 16, 9, 10, 8, 6, 10, 12, 5, 8 };
  const int ai_values_1[] = { 10000, 13000, 16000, 16000, 18000, 20000,
                              19000, 18000, 30000, 23000, 26000 };
  const int ai_values_2[] = { 10000, 35000, 20000, 27000, 37000, 25000,
                              40000, 30000, 50000, 35000, 40000 };
  const int ai_values_3[] = { 0, 36, 0, 31, 8, 480, 3, 16, 0, 193, 39 };
  const int ai_values_4[] = { 0, 30000, 5000, 40000, 50000, 20000, 45000,
                              35000, 65000, 25000, 30000 };
  const int ai_values_5[] = { 60000, 61000, 60000, 65400, 63000, 62000,
                              65000, 63000, 64000, 64000, 64000 };

  ai_value_0 = ai_values_0[face_ - 1];
  ai_value_1 = ai_values_1[face_ - 1];
  ai_value_2 = ai_values_2[face_ - 1];
  ai_value_3 = ai_values_3[face_ - 1];
  ai_value_4 = ai_values_4[face_ - 1];
  ai_value_5 = ai_values_5[face_ - 1];
}

/* Enqueue a new notification message for player. */
void
Player::add_notification(Message::Type type, MapPos pos, unsigned int data) {
  flags |= BIT(3); /* Message in queue. */
  Message new_message;
  new_message.type = type;
  new_message.pos = pos;
  new_message.data = data;
  messages.push(new_message);
}

bool
Player::has_notification() {
  return (messages.size() > 0);
}

Message
Player::pop_notification() {
  Message message = messages.front();
  messages.pop();
  return message;
}

Message
Player::peek_notification() {
  Message message = messages.front();
  return message;
}

void
Player::add_timer(int timeout, MapPos pos) {
  PosTimer new_timer;
  new_timer.timeout = timeout;
  new_timer.pos = pos;
  timers.push_back(new_timer);
}

/* Set defaults for food distribution priorities. */
void
Player::reset_food_priority() {
  //food_stonemine = 13100;  // default
  food_stonemine = 65500;  // make stone mine food very high because stone mines are not built unless out of stone, and then its an emergency!
  food_coalmine = 45850;
  food_ironmine = 45850;
  food_goldmine = 65500;
}

/* Set defaults for planks distribution priorities. */
void
Player::reset_planks_priority() {
  planks_construction = 65500;
  planks_boatbuilder = 3275;
  planks_toolmaker = 19650;
}

/* Set defaults for steel distribution priorities. */
void
Player::reset_steel_priority() {
  steel_toolmaker = 45850;
  steel_weaponsmith = 65500;
}

/* Set defaults for coal distribution priorities. */
void
Player::reset_coal_priority() {
  coal_steelsmelter = 32750;
  coal_goldsmelter = 65500;
  coal_weaponsmith = 52400;
}

/* Set defaults for coal distribution priorities. */
void
Player::reset_wheat_priority() {
  wheat_pigfarm = 65500;
  wheat_mill = 32750;
}

/* Set defaults for tool production priorities. */
void
Player::reset_tool_priority() {
  tool_prio[0] = 9825; /* SHOVEL */
  tool_prio[1] = 65500; /* HAMMER */
  tool_prio[2] = 13100; /* ROD */
  tool_prio[3] = 6550; /* CLEAVER */
  tool_prio[4] = 13100; /* SCYTHE */
  tool_prio[5] = 26200; /* AXE */
  tool_prio[6] = 32750; /* SAW */
  tool_prio[7] = 45850; /* PICK */
  tool_prio[8] = 6550; /* PINCER */
}

/* Set defaults for flag priorities. */
void
Player::reset_flag_priority() {
  flag_prio[Resource::TypeGoldOre] = 1;
  flag_prio[Resource::TypeGoldBar] = 2;
  flag_prio[Resource::TypeWheat] = 3;
  flag_prio[Resource::TypeFlour] = 4;
  flag_prio[Resource::TypePig] = 5;

  flag_prio[Resource::TypeBoat] = 6;
  flag_prio[Resource::TypePincer] = 7;
  flag_prio[Resource::TypeScythe] = 8;
  flag_prio[Resource::TypeRod] = 9;
  flag_prio[Resource::TypeCleaver] = 10;

  flag_prio[Resource::TypeSaw] = 11;
  flag_prio[Resource::TypeAxe] = 12;
  flag_prio[Resource::TypePick] = 13;
  flag_prio[Resource::TypeShovel] = 14;
  flag_prio[Resource::TypeHammer] = 15;

  flag_prio[Resource::TypeShield] = 16;
  flag_prio[Resource::TypeSword] = 17;
  flag_prio[Resource::TypeBread] = 18;
  flag_prio[Resource::TypeMeat] = 19;
  flag_prio[Resource::TypeFish] = 20;

  flag_prio[Resource::TypeIronOre] = 21;
  flag_prio[Resource::TypeLumber] = 22;
  flag_prio[Resource::TypeCoal] = 23;
  flag_prio[Resource::TypeSteel] = 24;
  flag_prio[Resource::TypeStone] = 25;
  flag_prio[Resource::TypePlank] = 26;
}

/* Set defaults for inventory priorities. */
void
Player::reset_inventory_priority() {
  inventory_prio[Resource::TypeWheat] = 1;
  inventory_prio[Resource::TypeFlour] = 2;
  inventory_prio[Resource::TypePig] = 3;
  inventory_prio[Resource::TypeBread] = 4;
  inventory_prio[Resource::TypeFish] = 5;

  inventory_prio[Resource::TypeMeat] = 6;
  inventory_prio[Resource::TypeLumber] = 7;
  inventory_prio[Resource::TypePlank] = 8;
  inventory_prio[Resource::TypeBoat] = 9;
  inventory_prio[Resource::TypeStone] = 10;

  inventory_prio[Resource::TypeCoal] = 11;
  inventory_prio[Resource::TypeIronOre] = 12;
  inventory_prio[Resource::TypeSteel] = 13;
  inventory_prio[Resource::TypeShovel] = 14;
  inventory_prio[Resource::TypeHammer] = 15;

  inventory_prio[Resource::TypeRod] = 16;
  inventory_prio[Resource::TypeCleaver] = 17;
  inventory_prio[Resource::TypeScythe] = 18;
  inventory_prio[Resource::TypeAxe] = 19;
  inventory_prio[Resource::TypeSaw] = 20;

  inventory_prio[Resource::TypePick] = 21;
  inventory_prio[Resource::TypePincer] = 22;
  inventory_prio[Resource::TypeShield] = 23;
  inventory_prio[Resource::TypeSword] = 24;
  inventory_prio[Resource::TypeGoldOre] = 25;
  inventory_prio[Resource::TypeGoldBar] = 26;
}

void
Player::change_knight_occupation(int index_, int adjust_max, int delta) {
  int max = (knight_occupation[index_] >> 4) & 0xf;
  int min = knight_occupation[index_] & 0xf;

  if (adjust_max) {
    max = clamp(min, max + delta, 4);
  } else {
    min = clamp(0, min + delta, max);
  }

  knight_occupation[index_] = (max << 4) | min;
}

/* Turn a number of serfs into knight for the given player. */
int
Player::promote_serfs_to_knights(int number) {
  int promoted = 0;

  for (Serf *serf : game->get_player_serfs(this)) {
    if (serf->get_state() == Serf::StateIdleInStock &&
        serf->get_type() == Serf::TypeGeneric) {
      Inventory *inv = game->get_inventory(serf->get_idle_in_stock_inv_index());
      if (inv->promote_serf_to_knight(serf)) {
        promoted += 1;
        number -= 1;
      }
    }
  }

  return promoted;
}

int
Player::available_knights_at_pos(MapPos pos, int index_, int dist) {
  //Log::Debug["player.cc"] << "inside Player::available_knights_at_pos, potential building pos " << pos << ", attacked_building_flag_pos " << attacked_building_flag_pos;
  const int min_level_hut[] = { 1, 1, 2, 2, 3 };
  const int min_level_tower[] = { 1, 2, 3, 4, 6 };
  const int min_level_fortress[] = { 1, 3, 6, 9, 12 };

  PMap map = game->get_map();
  if (map->get_owner(pos) != index ||
      map->type_up(pos) <= Map::TerrainWater3 ||
      map->type_down(pos) <= Map::TerrainWater3 ||
      map->get_obj(pos) < Map::ObjectSmallBuilding ||
      map->get_obj(pos) > Map::ObjectCastle) {
    return index_;  // I guess "return index_" means "no knights available"
  }

  //Log::Debug["player.cc"] << "inside Player::available_knights_at_pos, attacking building pos " << pos << ", attacked_building_flag_pos " << attacked_building_flag_pos;

  int bld_index = map->get_obj_index(pos);
  // check if this building already considered?
  for (int i = 0; i < index_; i++) {
    if (attacking_buildings[i] == bld_index) {
      return index_;
    }
  }

  Building *building_ = game->get_building(bld_index);
  // reject if building is not active
  if (!building_->is_done() || building_->is_burning()) {
    return index_;
  }

  const int *min_level = NULL;
  switch (building_->get_type()) {
    case Building::TypeHut: min_level = min_level_hut; break;
    case Building::TypeTower: min_level = min_level_tower; break;
    case Building::TypeFortress: min_level = min_level_fortress; break;
    default: return index_; break;
  }

  // verify index isn't out of range (attacking_buildings[] max is 63)
  if (index_ >= 64) return index_;

  // first verify there are enough knights to send
  size_t state = building_->get_threat_level();
  int knights_present = building_->get_knight_count();
  int to_send = knights_present - min_level[knight_occupation[state] & 0xf];
  if (to_send < 1){
    return index_;  // I guess "return index_" means "no knights available"
  }

  // tlongstretch - can the knights actually REACH the target building from this building??
  // NOTE that because the knight is currently inside a building, and buildings are not technically
  //  eligible for traverse, and because pathfinder does a reverse search from end to start, the
  //  start pos must be the FLAG of the knight building and not the building itself!
  // ADD NEW OPTION TO DESIGNATE THIS
  //  option_CheckPathBeforeAttack
  MapPos start_pos = map->move_down_right(pos);
  // note that pathfinder_freewalking_serf will reject road solutions that are too long!  
  //std::clock_t pathfinder_free_start = std::clock();
  Road freewalking_route = pathfinder_freewalking_serf(game->get_map().get(), start_pos, attacked_building_flag_pos, 100);
  //Log::Debug["player.cc"] << "inside Player::available_knights_at_pos, this pathfinder_freewalking_serf call took " << (std::clock() - pathfinder_free_start) / static_cast<double>(CLOCKS_PER_SEC);;

  if (freewalking_route.get_length() > 0){
    //game->set_debug_mark_road(freewalking_route);
    //Log::Debug["player.cc"] << "inside Player::available_knights_at_pos, attacking building pos " << pos << ", start_pos " << start_pos << ", attacked_building_flag_pos " << attacked_building_flag_pos << ", found freewalking solution to attacked_building_flag_pos, length " << freewalking_route.get_length();

    //
    // check convolution ratio, if the road is too convoluted it is decreasingly likely that
    //  freewalking knights will be able to navigate it consistently or at all
    //
    //int ideal_length = AI::get_straightline_tile_dist(map, start_pos, target_pos);
    // function copied from AI::get_straightline_tile_dist, MAKE THIS A GAME FUNCTION INSTEAD OF AI-SPECIFIC!
    int dist_col = map->dist_x(start_pos, attacked_building_flag_pos);
    int dist_row = map->dist_y(start_pos, attacked_building_flag_pos);
    int ideal_length = 0;
    if ((dist_col > 0 && dist_row > 0) || (dist_col < 0 && dist_row < 0)) {
      ideal_length = std::max(abs(dist_col), abs(dist_row));
    }else {
      ideal_length = abs(dist_col) + abs(dist_row);
    }
    // NOTE - to log this properly, AI and main game thread logs must be separated
    //  as AI calls this also to perform attacks!
    //Log::Debug["player.cc"] << "inside Player::available_knights_at_pos, attacking building pos " << pos << ", has straight-line tile distance " << ideal_length << " from attacked_building_flag_pos " << attacked_building_flag_pos;
    //Log::Debug["player.cc"] << "inside Player::available_knights_at_pos, attacking building pos " << pos << ", route from start_pos " << start_pos << " to attacked_building_flag_pos_pos " << attacked_building_flag_pos << " has length " << freewalking_route.get_length();
    double convolution = static_cast<double>(freewalking_route.get_length()) / static_cast<double>(ideal_length);
    //Log::Debug["player.cc"] << "inside Player::available_knights_at_pos, attacking building pos " << pos << ", route length: " << freewalking_route.get_length() << ", ideal length: " << ideal_length << ", convolution ratio: " << convolution;
    if (convolution >= 3.00) {
      //Log::Info["player.cc"] << "inside Player::available_knights_at_pos, this route solution is too convoluted, rejecting";
      return index_;
    }
  }else{
    //Log::Debug["player.cc"] << "inside Player::available_knights_at_pos, attacking building pos " << pos << ", attacked_building_flag_pos " << attacked_building_flag_pos << ", cannot reach attacked_building_flag_pos within specified limits";
    return index_;
  }

  attacking_buildings[index_] = bld_index;
  if (to_send > 0) attacking_knights[dist] += to_send;
  return index_ + 1;
}

// check the area in a 32 tile radius around the specified pos
//  (which should be an attackable enemy military building) and
//  return the number of knights that are available to attack it
//  Broken into four groups based on distance, each maps to the
//  knight distance icon in the attack popup
//
// this is a relatively heavy call!  a 32-radius check is very large
//  so avoid running it excessively
int
Player::knights_available_for_attack(MapPos pos) {
  /* Reset counters. */
  for (int i = 0; i < 4; i++) {
    attacking_knights[i] = 0;
  }

  int count = 0;
  PMap map = game->get_map();

  // tlongstretch - adding this to sanity check if knights can actually reach the target building's flag pos
  //  it is used inside available_knights_at_pos
  attacked_building_flag_pos = map->move_down_right(pos);

  /* Iterate each shell around the position.*/
  for (int i = 0; i < 32; i++) {
    pos = map->move_right(pos);
    for (int j = 0; j < i+1; j++) {
      count = available_knights_at_pos(pos, count, i >> 3);
      pos = map->move_down(pos);
    }
    for (int j = 0; j < i+1; j++) {
      count = available_knights_at_pos(pos, count, i >> 3);
      pos = map->move_left(pos);
    }
    for (int j = 0; j < i+1; j++) {
      count = available_knights_at_pos(pos, count, i >> 3);
      pos = map->move_up_left(pos);
    }
    for (int j = 0; j < i+1; j++) {
      count = available_knights_at_pos(pos, count, i >> 3);
      pos = map->move_up(pos);
    }
    for (int j = 0; j < i+1; j++) {
      count = available_knights_at_pos(pos, count, i >> 3);
      pos = map->move_right(pos);
    }
    for (int j = 0; j < i+1; j++) {
      count = available_knights_at_pos(pos, count, i >> 3);
      pos = map->move_down_right(pos);
    }
  }

  attacking_building_count = count;

  total_attacking_knights = 0;
  for (int i = 0; i < 4; i++) {
    total_attacking_knights += attacking_knights[i];
  }

  return total_attacking_knights;
}

void
Player::start_attack() {
  const int min_level_hut[] = { 1, 1, 2, 2, 3 };
  const int min_level_tower[] = { 1, 2, 3, 4, 6 };
  const int min_level_fortress[] = { 1, 3, 6, 9, 12 };

  Building *target = game->get_building(building_attacked);
  if (!target->is_done() || !target->is_military() ||
      !target->is_active() || target->get_threat_level() != 3) {
    return;
  }

  PMap map = game->get_map();
  for (int i = 0; i < attacking_building_count; i++) {
    /* TODO building index may not be valid any more(?). */
    Building *b = game->get_building(attacking_buildings[i]);
    if (b->is_burning() || map->get_owner(b->get_position()) != index) {
      continue;
    }

    MapPos flag_pos = map->move_down_right(b->get_position());
    if (map->has_serf(flag_pos)) {
      /* Check if building is under siege. */
      Serf *s = game->get_serf_at_pos(flag_pos);
      if (s->get_owner() != index) continue;
    }

    const int *min_level = NULL;
    switch (b->get_type()) {
    case Building::TypeHut: min_level = min_level_hut; break;
    case Building::TypeTower: min_level = min_level_tower; break;
    case Building::TypeFortress: min_level = min_level_fortress; break;
    default: continue; break;
    }

    size_t state = b->get_threat_level();
    int knights_present = b->get_knight_count();
    int to_send = knights_present - min_level[knight_occupation[state] & 0xf];

    for (int j = 0; j < to_send; j++) {
      /* Find most approriate knight to send according to player settings. */
      int best_type = send_strongest() ? Serf::TypeKnight0:
                                         Serf::TypeKnight4;
      int best_index = -1;

      int knight_index = b->get_holder_or_first_knight();
      while (knight_index != 0) {
        Serf *knight = game->get_serf(knight_index);
        if (send_strongest()) {
          if (knight->get_type() >= best_type) {
            best_index = knight_index;
            best_type = knight->get_type();
          }
        } else {
          if (knight->get_type() <= best_type) {
            best_index = knight_index;
            best_type = knight->get_type();
          }
        }

        knight_index = knight->get_next();
      }

      Serf *def_serf = b->call_attacker_out(best_index);

      target->set_under_attack();

      /* Calculate distance to target. */
      int dist_col = map->dist_x(target->get_position(), def_serf->get_pos());
      int dist_row = map->dist_y(target->get_position(), def_serf->get_pos());

      /* Send this serf off to fight. */
      def_serf->send_off_to_fight(dist_col, dist_row);

      knights_attacking -= 1;
      if (knights_attacking == 0) return;
    }
  }
}

/* Begin cycling knights by sending knights from military buildings
   to inventories. The knights can then be replaced by more experienced
   knights. */
void
Player::cycle_knights() {
  flags |= BIT(2) | BIT(4);
  knight_cycle_counter = 2400;
}

void
Player::increase_castle_knights_wanted() {
  castle_knights_wanted = std::min(castle_knights_wanted+1, 99);
}

void
Player::decrease_castle_knights_wanted() {
  castle_knights_wanted = std::max(1, castle_knights_wanted-1);
}

void
Player::building_founded(Building *building_) {
  building_->set_owner(index);

  if (building_->get_type() == Building::TypeCastle) {
    flags |= BIT(0); /* Has castle */
    build |= BIT(3);
    total_building_score += building_get_score_from_type(Building::TypeCastle);
    castle_inventory = building_->get_inventory()->get_index();
    building = building_->get_index();
    create_initial_castle_serfs(building_);
    last_tick = game->get_tick();
  } else {
    incomplete_building_count[building_->get_type()] += 1;
  }
}

void
Player::building_built(Building *building_) {
  Building::Type type = building_->get_type();
  total_building_score += building_get_score_from_type(type);
  completed_building_count[type] += 1;
  incomplete_building_count[type] -= 1;
}

void
Player::building_captured(Building *building_) {
  Player *def_player = game->get_player(building_->get_owner());

  def_player->add_notification(Message::TypeLoseFight,
                               building_->get_position(), index);
  add_notification(Message::TypeWinFight, building_->get_position(),
                   index);

  if (building_->get_type() == Building::TypeCastle) {
    castle_score += 1;
  } else {
    /* Update player scores. */
    def_player->total_building_score -=
      building_get_score_from_type(building_->get_type());
    def_player->total_land_area -= 7;
    def_player->completed_building_count[building_->get_type()] -= 1;

    total_building_score += building_get_score_from_type(building_->get_type());
    total_land_area += 7;
    completed_building_count[building_->get_type()] += 1;

    /* Change owner of building */
    building_->set_owner(index);

    if (is_ai()) {
      /* TODO AI */
    }
  }
}

void
Player::building_demolished(Building *building_) {
  /* Update player fields. */
  if (building_->is_done()) {
    total_building_score -= building_get_score_from_type(building_->get_type());

    if (building_->get_type() != Building::TypeCastle) {
      completed_building_count[building_->get_type()] -= 1;
    } else {
      Log::Warn["player.cc"] << "Player" << index << "'s castle has just been destroyed!";
      //game->pause();
      build &= ~BIT(3);
      castle_score -= 1;
    }
  } else {
    incomplete_building_count[building_->get_type()] -= 1;
  }
}

Serf*
Player::spawn_serf_generic() {
  Serf *serf = game->create_serf();
  //if (serf == NULL) return NULL;
  if (serf == NULL){
    Log::Warn["player"] << "inside spawn_serf_generic, game->create_serf was null!";
    return NULL;
  }

  serf->set_owner(index);

  // I have been trying to track down issue where Generic serfs show up
  //  twice in realm/player stats, checking to see if this is the cause...
  // YES this fixes it, numbers look correct now.  I believe these new Generic
  //  serfs area alread being accounted for in a increase_serf_count call and this
  //  is doubling it
  //serf_count[Serf::TypeGeneric] += 1;

  return serf;
}

// Spawn new serf. Returns true on success.
// The serf_t object and inventory are returned if non-NULL.
bool
Player::spawn_serf(Serf **serf, Inventory **inventory, bool want_knight) {
  if (!can_spawn()) {
    Log::Warn["player"] << "inside spawn_serf for player " << index << ", can_spawn is false!";
    return false;
  }
  //Log::Debug["player"] << "inside spawn_serf for player " << index << ", can_spawn is true";

  Game::ListInventories inventories = game->get_player_inventories(this);
  if (inventories.size() < 1) {
    // this happens when all Inventories destroyed
    //Log::Warn["game.cc"] << "inside Player::spawn_serf, inventories.size is <1, it is currently " << inventories.size() << ", does this mean max inventories reached?  Game just returns false";
    return false;
  }

  Inventory *inv = nullptr;
  for (Inventory *loop_inv : inventories) {
    if (loop_inv->get_serf_mode() == Inventory::ModeIn) {
      if (want_knight && (loop_inv->get_count_of(Resource::TypeSword) == 0 ||
                          loop_inv->get_count_of(Resource::TypeShield) == 0)) {
        continue;
      } else if (loop_inv->free_serf_count() == 0) {
        inv = loop_inv;
        break;
      } else if (inv == nullptr ||
                 loop_inv->free_serf_count() < inv->free_serf_count()) {
        inv = loop_inv;
      }
    }
  }

  if (inv == nullptr) {
    if (want_knight) {
      return spawn_serf(serf, inventory, false);
    } else {
      return false;
    }
  }

  Serf *s = inv->spawn_serf_generic();
  if (s == nullptr) {
    Log::Warn["player"] << "inside spawn_serf for player " << index << ", inv->create_serf_generic s was null!";
    return false;
  }

  if (serf != nullptr) {
    *serf = s;
  }
  if (inventory != nullptr) {
    *inventory = inv;
  }

  return true;
}

// NOTE - in original Freeserf code these did not scale with game_speed
//  meaning on speed 1 the delay was not increased and on speed 3+ the delay was not reduced
// this should now scale properly with game speed at speeds 1 and 13+ (slow and "time warp" speeds)
bool
Player::tick_send_generic_delay() {
  //send_generic_delay -= 1;  // this is the original value, which is HALF the default tick speed
  send_generic_delay -= game_ticks_per_update;  // adjust for time warp game_speeds
  if (send_generic_delay < 0) {
    //send_generic_delay = 5;  // remember that normal game_ticks_per_update is 2 but the original value here was 1-per-update...
    send_generic_delay = 10;  // ... so just double the timer length
    return true;
  }
  return false;
}

bool
Player::tick_send_knight_delay() {
  //send_knight_delay -= 1;  // this is the original value, which is HALF the default tick speed
  send_knight_delay -= game_ticks_per_update;  // adjust for time warp game_speeds
  if (send_knight_delay < 0) {
    //send_knight_delay = 5;  // remember that normal game_ticks_per_update is 2 but the original value here was 1-per-update...
    send_knight_delay = 10;  // ... so just double the timer length
    return true;
  }
  return false;
}

Serf::Type
Player::get_cycling_serf_type(Serf::Type type) const {
  if (cycling_second()) {
    type = (Serf::Type)-((knight_cycle_counter >> 8) + 1);
  }
  return type;
}

void
Player::decrease_serf_count(Serf::Type type) {
  if (serf_count[type] == 0) {
    throw ExceptionFreeserf("Failed to decrease serf count");
  }
  serf_count[type]--;
}

/* Create the initial serfs that occupies the castle. */
void
Player::create_initial_castle_serfs(Building *castle) {
  build |= BIT(2);

  /* Spawn serf 4 */
  Inventory *inventory = castle->get_inventory();
  Serf *serf = inventory->spawn_serf_generic();
  if (serf == nullptr) {
    Log::Warn["player"] << "inside create_initial_castle_serfs, inventory->spawn_serf_generic serf was null!";
    return;
  }
  inventory->specialize_serf(serf, Serf::TypeTransporterInventory);
  serf->init_inventory_transporter(inventory);

  game->get_map()->set_serf_index(serf->get_pos(), serf->get_index());

  Building *building_ = game->get_building(building);
  building_->set_holder_or_first_knight(serf->get_index());

  /* Spawn generic serfs */
  for (int i = 0; i < 5; i++) {
    spawn_serf(nullptr, nullptr, false);
  }

  /* Spawn three knights */
  for (int i = 0; i < 3; i++) {
    serf = inventory->spawn_serf_generic();
    if (serf == nullptr) return;
    inventory->promote_serf_to_knight(serf);
  }

  /* Spawn toolmaker */
  serf = inventory->spawn_serf_generic();
  if (serf == nullptr) return;
  inventory->specialize_serf(serf, Serf::TypeToolmaker);

  /* Spawn timberman */
  serf = inventory->spawn_serf_generic();
  if (serf == nullptr) return;
  inventory->specialize_serf(serf, Serf::TypeLumberjack);

  /* Spawn sawmiller */
  serf = inventory->spawn_serf_generic();
  if (serf == nullptr) return;
  inventory->specialize_serf(serf, Serf::TypeSawmiller);

  /* Spawn stonecutter */
  serf = inventory->spawn_serf_generic();
  if (serf == nullptr) return;
  inventory->specialize_serf(serf, Serf::TypeStonecutter);

  /* Spawn digger */
  serf = inventory->spawn_serf_generic();
  if (serf == nullptr) return;
  inventory->specialize_serf(serf, Serf::TypeDigger);

  /* Spawn builder */
  serf = inventory->spawn_serf_generic();
  if (serf == nullptr) return;
  inventory->specialize_serf(serf, Serf::TypeBuilder);

  /* Spawn fisherman */
  serf = inventory->spawn_serf_generic();
  if (serf == nullptr) return;
  inventory->specialize_serf(serf, Serf::TypeFisher);

  /* Spawn two geologists */
  for (int i = 0; i < 2; i++) {
    serf = inventory->spawn_serf_generic();
    if (serf == nullptr) return;
    inventory->specialize_serf(serf, Serf::TypeGeologist);
  }

  /* Spawn two miners */
  for (int i = 0; i < 2; i++) {
    serf = inventory->spawn_serf_generic();
    if (serf == nullptr) return;
    inventory->specialize_serf(serf, Serf::TypeMiner);
  }
}

/* Update player game state as part of the game progression. */
void
Player::update() {
  uint16_t delta = game->get_tick() - last_tick;
  last_tick = game->get_tick();

  if (total_land_area > 0xffff0000) total_land_area = 0;
  if (total_military_score > 0xffff0000) total_military_score = 0;
  if (total_building_score > 0xffff0000) total_building_score = 0;

  if (is_ai()) {
    /*if (player->field_1B2 != 0) player->field_1B2 -= 1;*/
    /*if (player->field_1B0 != 0) player->field_1B0 -= 1;*/
  }

  if (cycling_knight()) {
    knight_cycle_counter -= delta;
    if (knight_cycle_counter < 1) {
      flags &= ~BIT(5);
      flags &= ~BIT(2);
    } else if (knight_cycle_counter < 2048 && reduced_knight_level()) {
      flags |= BIT(5);
      flags &= ~BIT(4);
    }
  }

  if (has_castle()) {
    reproduction_counter -= delta;
    while (reproduction_counter < 0) {
      serf_to_knight_counter += serf_to_knight_rate;
      if (serf_to_knight_counter < serf_to_knight_rate) {
        knights_to_spawn += 1;
        if (knights_to_spawn > 2) knights_to_spawn = 2;
      }
      game->mutex_lock("Player::update before spawn_serf");
      if (knights_to_spawn == 0) {
        // Create unassigned serf
        spawn_serf(nullptr, nullptr, false);
      } else {
        // Create knight serf
        Serf *serf = nullptr;
        Inventory *inventory = nullptr;
        if (spawn_serf(&serf, &inventory, true)) {
          if (inventory->get_count_of(Resource::TypeSword) != 0 &&
              inventory->get_count_of(Resource::TypeShield) != 0) {
            knights_to_spawn -= 1;
            if (inventory->specialize_serf(serf, Serf::TypeKnight0)){
              //Log::Debug["player"] << "inside player->update for player " << index << ", creating knight, successful return from specialize_serf";
            }else{
              Log::Warn["player"] << "inside player->update for player " << index << ", creating knight, FAILURE from specialize_serf";
            }
          }
        }
      }
      game->mutex_unlock();
      reproduction_counter += static_cast<int>(reproduction_reset);
    }
  }

  /* Update timers */
  PosTimers::iterator it = timers.begin();
  while (it != timers.end()) {
    it->timeout -= delta;
    if (it->timeout < 0) {
      /* Timer has expired. */
      /* TODO box (+ pos) timer */
      add_notification(Message::TypeCallToLocation, it->pos, 0);
      it = timers.erase(it);
    } else {
      ++it;
    }
  }
}

void
Player::update_stats(int res) {
  //std::string histogram = NameResource[res] + ": ";
  //for (int i = 0; i < 120; i++){
  //  histogram = histogram + std::to_string(resource_count_history[res][i]);
  //}
  //Log::Debug["game.cc"] << "inside Player::update_stats, was " << histogram;

  /*
  // shift array right
  for (int z=119; z>0; z--){
    resource_count_history[res][z] = resource_count_history[res][z-1]; 
  }
  // new data goes into first element
  resource_count_history[res][0] = resource_count[res];
  */
  // instead using Pyrdacor's logic from https://github.com/freeserf/freeserf/issues/469#issuecomment-1374200624
  resource_count_history[res][game->get_resource_history_index()] = resource_count[res];
  // reset the "now" res value to zero
  resource_count[res] = 0;

  //std::string histogram2 = NameResource[res] + ": ";
  //for (int i = 0; i < 120; i++){
  //  histogram2 = histogram2 + std::to_string(resource_count_history[res][i]);
  //}
  //Log::Debug["game.cc"] << "inside Player::update_stats, is now " << histogram2;
}

void
Player::update_knight_morale() {
  unsigned int inventory_gold = 0;
  unsigned int military_gold = 0;

  /* Sum gold collected in inventories */
  for (Inventory *inventory : game->get_player_inventories(this)) {
    inventory_gold += inventory->get_count_of(Resource::TypeGoldBar);
  }

  /* Sum gold deposited in military buildings */
  for (Building *building : game->get_player_buildings(this)) {
    military_gold += building->military_gold_count();
  }

  unsigned int depot = inventory_gold + military_gold;
  gold_deposited = inventory_gold + military_gold;

  /* Calculate according to gold collected. */
  unsigned int total_gold = game->get_gold_total();
  if (total_gold != 0) {
    while (total_gold > 0xffff) {
      total_gold >>= 1;
      depot >>= 1;
    }
    depot = std::min(depot, total_gold-1);
    knight_morale = 1024 + (game->get_gold_morale_factor() * depot)/total_gold;
  } else {
    knight_morale = 4096;
  }

  /* Adjust based on castle score. */
  if (castle_score < 0) {
    knight_morale = std::max(1, knight_morale - 1023);
  } else if (castle_score > 0) {
    knight_morale = std::min(knight_morale + 1024 * castle_score, 0xffff);
  }

  unsigned int military_score = total_military_score;
  unsigned int morale = knight_morale >> 5;
  while (military_score > 0xffff) {
    military_score >>= 1;
    morale <<= 1;
  }

  /* Calculate fractional score used by AI */
  unsigned int player_score = (military_score * morale) >> 7;
  unsigned int enemy_score = game->get_enemy_score(this);

  while (player_score > 0xffff && enemy_score > 0xffff) {
    player_score >>= 1;
    enemy_score >>= 1;
  }
/*
  player_score >>= 1;
  unsigned int frac_score = 0;
  if (player_score != 0 && enemy_score != 0) {
    if (player_score > enemy_score) {
      frac_score = 0xffffffff;
    } else {
      frac_score = (player_score * 0x10000) / enemy_score;
    }
  }
*/
  military_max_gold = 0;
}

/* Calculate condensed score from military score and knight morale. */
int
Player::get_military_score() const {
  return (2048 + (knight_morale >> 1)) * (total_military_score << 6);
}

int
Player::get_score() const {
  int mil_score = get_military_score();
  return total_building_score + ((total_land_area + mil_score) >> 4);
}

ResourceMap
Player::get_stats_resources() {
  ResourceMap resources;

  /* Sum up resources of all inventories. */
  for (Inventory *inventory : game->get_player_inventories(this)) {
    for (int j = 0; j < 26; j++) {
      resources[(Resource::Type)j] +=
                                     inventory->get_count_of((Resource::Type)j);
    }
  }

  return resources;
}

Serf::SerfMap
Player::get_stats_serfs_idle() {
  Serf::SerfMap res;

  /* Sum up all existing serfs. */
  for (Serf *serf : game->get_player_serfs(this)) {
    if (serf->get_state() == Serf::StateIdleInStock) {
      res[serf->get_type()] += 1;
    }
  }

  return res;
}

Serf::SerfMap
Player::get_stats_serfs_potential() {
  Serf::SerfMap res;

  /* Sum up potential serfs of all inventories. */
  for (Inventory *inventory : game->get_player_inventories(this)) {
    if (inventory->free_serf_count() > 0) {
      for (int i = 0; i < 27; i++) {
        res[(Serf::Type)i] += inventory->serf_potential_count((Serf::Type)i);
      }
    }
  }

  return res;
}

unsigned int
Player::get_food_for_building(unsigned int bld_type) const {
  unsigned int res = 0;
  switch (bld_type) {
    case Building::TypeStoneMine:
      res = get_food_stonemine();
      break;
    case Building::TypeCoalMine:
      res = get_food_coalmine();
      break;
    case Building::TypeIronMine:
      res = get_food_ironmine();
      break;
    case Building::TypeGoldMine:
      res = get_food_goldmine();
      break;
    default:
      break;
  }
  return res;
}

SaveReaderBinary&
operator >> (SaveReaderBinary &reader, Player &player)  {
  const Player::Color default_player_colors[] = {
    {0x00, 0xe3, 0xe3},
    {0xcf, 0x63, 0x63},
    {0xdf, 0x7f, 0xef},
    {0xef, 0xef, 0x8f}
  };

  uint16_t v16;
  for (int j = 0; j < 9; j++) {
    reader >> v16;  // 0
    player.tool_prio[j] = v16;
  }

  uint8_t v8;
  for (int j = 0; j < 26; j++) {
    reader >> v8;  // 18
    player.resource_count[j] = v8;
  }
  for (int j = 0; j < 26; j++) {
    reader >> v8;  // 44
    player.flag_prio[j] = v8;
  }

  for (int j = 0; j < 27; j++) {
    reader >> v16;  // 70
    player.serf_count[j] = v16;
  }

  for (int j = 0; j < 4; j++) {
    reader >> v8;  // 124
    player.knight_occupation[j] = v8;
  }

  reader >> v16;  // 128
  player.index = v16;
  player.color = default_player_colors[player.index];
  reader >> v8;  // 130
  player.flags = v8;
  reader >> v8;  // 131
  player.build = v8;

  for (int j = 0; j < 23; j++) {
    reader >> v16;  // 132
    player.completed_building_count[j] = v16;
  }
  for (int j = 0; j < 23; j++) {
    reader >> v16;  // 178
    player.incomplete_building_count[j] = v16;
  }

  for (int j = 0; j < 26; j++) {
    reader >> v8;  // 224
    player.inventory_prio[j] = v8;
  }

  for (int j = 0; j < 64; j++) {
    reader >> v16;  // 250
    player.attacking_buildings[j] = v16;
  }

  reader >> v16;  // 378
//  player.current_sett_5_item = v16;
  reader >> v16;  // 380 ???
  reader >> v16;  // 382 ???
  reader >> v16;  // 384 ???
  reader >> v16;  // 386 ???
  reader >> v16;  // 388
  player.building = v16;

  reader >> v16;  // 390 // castle_flag
  reader >> v16;  // 392
  player.castle_inventory = v16;

  reader >> v16;  // 394
  player.cont_search_after_non_optimal_find = v16;
  reader >> v16;  // 396
  player.knights_to_spawn = v16;
  reader >> v16;  // 398
  reader >> v16;  // 400
  /*player->field_110 = v16;*/
  reader >> v16;  // 402 ???
  reader >> v16;  // 404 ???

  uint32_t v32;
  reader >> v32;  // 406
  player.total_building_score = v32;
  reader >> v32;  // 410
  player.total_military_score = v32;

  reader >> v16;  // 414
  player.last_tick = v16;

  reader >> v16;  // 416
  player.reproduction_counter = v16;
  reader >> v16;  // 418
  player.reproduction_reset = v16;
  reader >> v16;  // 420
  player.serf_to_knight_rate = v16;
  reader >> v16;  // 422
  player.serf_to_knight_counter = v16;

  reader >> v16;  // 424
  player.attacking_building_count = v16;
  for (int j = 0; j < 4; j++) {
    reader >> v16;  // 426
    player.attacking_knights[j] = v16;
  }
  reader >> v16;  // 434
  player.total_attacking_knights = v16;
  reader >> v16;  // 436
  player.building_attacked = v16;
  reader >> v16;  // 438
  player.knights_attacking = v16;

  reader >> v16;  // 440
  player.analysis_goldore = v16;
  reader >> v16;  // 442
  player.analysis_ironore = v16;
  reader >> v16;  // 444
  player.analysis_coal = v16;
  reader >> v16;  // 446
  player.analysis_stone = v16;

  reader >> v16;  // 448
  player.food_stonemine = v16;
  reader >> v16;  // 450
  player.food_coalmine = v16;
  reader >> v16;  // 452
  player.food_ironmine = v16;
  reader >> v16;  // 454
  player.food_goldmine = v16;

  reader >> v16;  // 456
  player.planks_construction = v16;
  reader >> v16;  // 458
  player.planks_boatbuilder = v16;
  reader >> v16;  // 460
  player.planks_toolmaker = v16;

  reader >> v16;  // 462
  player.steel_toolmaker = v16;
  reader >> v16;  // 464
  player.steel_weaponsmith = v16;

  reader >> v16;  // 466
  player.coal_steelsmelter = v16;
  reader >> v16;  // 468
  player.coal_goldsmelter = v16;
  reader >> v16;  // 470
  player.coal_weaponsmith = v16;

  reader >> v16;  // 472
  player.wheat_pigfarm = v16;
  reader >> v16;  // 474
  player.wheat_mill = v16;

  reader >> v16;  // 476
//  player.current_sett_6_item = v16;

  reader >> v16;  // 478
  player.castle_score = v16;

  /* TODO */

  return reader;
}

SaveReaderText&
operator >> (SaveReaderText &reader, Player &player) {
  reader.value("flags") >> player.flags;
  reader.value("build") >> player.build;
  unsigned int val;
  reader.value("color")[0] >> val; player.color.red = val;
  reader.value("color")[1] >> val; player.color.green = val;
  reader.value("color")[2] >> val; player.color.blue = val;
  reader.value("face") >> player.face;
  for (int i = 0; i < 9; i++) {
    reader.value("tool_prio")[i] >> player.tool_prio[i];
  }
  for (int i = 0; i < 26; i++) {
    reader.value("resource_count")[i] >> player.resource_count[i];
    reader.value("flag_prio")[i] >> player.flag_prio[i];
    reader.value("serf_count")[i] >> player.serf_count[i];
    reader.value("inventory_prio")[i] >> player.inventory_prio[i];
  }
  reader.value("serf_count")[26] >> player.serf_count[26];
  for (int i = 0; i < 4; i++) {
    reader.value("knight_occupation")[i] >> player.knight_occupation[i];
    reader.value("attacking_knights")[i] >> player.attacking_knights[i];
  }
  for (int i = 0; i < 23; i++) {
    reader.value("completed_building_count")[i] >>
      player.completed_building_count[i];
    reader.value("incomplete_building_count")[i] >>
      player.incomplete_building_count[i];
  }
  for (int i = 0; i < 64; i++) {
    reader.value("attacking_buildings")[i] >> player.attacking_buildings[i];
  }
  reader.value("initial_supplies") >> player.initial_supplies;
  reader.value("knights_to_spawn") >> player.knights_to_spawn;
  reader.value("total_building_score") >> player.total_building_score;
  reader.value("total_military_score") >> player.total_military_score;
  reader.value("last_tick") >> player.last_tick;
  reader.value("reproduction_counter") >> player.reproduction_counter;
  reader.value("reproduction_reset") >> player.reproduction_reset;
  reader.value("serf_to_knight_rate") >> player.serf_to_knight_rate;
  reader.value("serf_to_knight_counter") >> player.serf_to_knight_counter;
  reader.value("attacking_building_count") >> player.attacking_building_count;
  reader.value("total_attacking_knights") >> player.total_attacking_knights;
  reader.value("building_attacked") >> player.building_attacked;
  reader.value("knights_attacking") >> player.knights_attacking;
  reader.value("food_stonemine") >> player.food_stonemine;
  reader.value("food_coalmine") >> player.food_coalmine;
  reader.value("food_ironmine") >> player.food_ironmine;
  reader.value("food_goldmine") >> player.food_goldmine;
  reader.value("planks_construction") >> player.planks_construction;
  reader.value("planks_boatbuilder") >> player.planks_boatbuilder;
  reader.value("planks_toolmaker") >> player.planks_toolmaker;
  reader.value("steel_toolmaker") >> player.steel_toolmaker;
  reader.value("steel_weaponsmith") >> player.steel_weaponsmith;
  reader.value("coal_steelsmelter") >> player.coal_steelsmelter;
  reader.value("coal_goldsmelter") >> player.coal_goldsmelter;
  reader.value("coal_weaponsmith") >> player.coal_weaponsmith;
  reader.value("wheat_pigfarm") >> player.wheat_pigfarm;
  reader.value("wheat_mill") >> player.wheat_mill;
  reader.value("castle_score") >> player.castle_score;
  reader.value("castle_knights") >> player.castle_knights;
  reader.value("castle_knights_wanted") >> player.castle_knights_wanted;

  return reader;
}

SaveWriterText&
operator << (SaveWriterText &writer, Player &player) {
  writer.value("flags") << player.flags;
  writer.value("build") << player.build;
  writer.value("color") << player.color.red;
  writer.value("color") << player.color.green;
  writer.value("color") << player.color.blue;
  writer.value("face") << player.face;

  for (int i = 0; i < 9; i++) {
    writer.value("tool_prio") << player.tool_prio[i];
  }

  for (int i = 0; i < 26; i++) {
    writer.value("resource_count") << player.resource_count[i];
    writer.value("flag_prio") << player.flag_prio[i];
    writer.value("serf_count") << player.serf_count[i];
    writer.value("inventory_prio") << player.inventory_prio[i];
  }
  writer.value("serf_count") << player.serf_count[26];

  for (int i = 0; i < 4; i++) {
    writer.value("knight_occupation") << player.knight_occupation[i];
    writer.value("attacking_knights") << player.attacking_knights[i];
  }

  for (int i = 0; i < 23; i++) {
    writer.value("completed_building_count") <<
      player.completed_building_count[i];
    writer.value("incomplete_building_count") <<
      player.incomplete_building_count[i];
  }

  for (int i = 0; i < 64; i++) {
    writer.value("attacking_buildings") << player.attacking_buildings[i];
  }

  writer.value("initial_supplies") << player.initial_supplies;
  writer.value("knights_to_spawn") << player.knights_to_spawn;

  writer.value("total_building_score") << player.total_building_score;
  writer.value("total_military_score") << player.total_military_score;

  writer.value("last_tick") << player.last_tick;

  writer.value("reproduction_counter") << player.reproduction_counter;
  writer.value("reproduction_reset") << player.reproduction_reset;
  writer.value("serf_to_knight_rate") << player.serf_to_knight_rate;
  writer.value("serf_to_knight_counter") << player.serf_to_knight_counter;

  writer.value("attacking_building_count") << player.attacking_building_count;
  writer.value("total_attacking_knights") << player.total_attacking_knights;
  writer.value("building_attacked") << player.building_attacked;
  writer.value("knights_attacking") << player.knights_attacking;

  writer.value("food_stonemine") << player.food_stonemine;
  writer.value("food_coalmine") << player.food_coalmine;
  writer.value("food_ironmine") << player.food_ironmine;
  writer.value("food_goldmine") << player.food_goldmine;

  writer.value("planks_construction") << player.planks_construction;
  writer.value("planks_boatbuilder") << player.planks_boatbuilder;
  writer.value("planks_toolmaker") << player.planks_toolmaker;

  writer.value("steel_toolmaker") << player.steel_toolmaker;
  writer.value("steel_weaponsmith") << player.steel_weaponsmith;

  writer.value("coal_steelsmelter") << player.coal_steelsmelter;
  writer.value("coal_goldsmelter") << player.coal_goldsmelter;
  writer.value("coal_weaponsmith") << player.coal_weaponsmith;

  writer.value("wheat_pigfarm") << player.wheat_pigfarm;
  writer.value("wheat_mill") << player.wheat_mill;

  writer.value("castle_score") << player.castle_score;

  writer.value("castle_knights") << player.castle_knights;
  writer.value("castle_knights_wanted") << player.castle_knights_wanted;

  return writer;
}
