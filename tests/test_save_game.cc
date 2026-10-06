/*
 * test_save_game.cc - test for loading/saving game
 *
 * Copyright (C) 2016-2017  Jon Lund Steffensen <jonlst@gmail.com>
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

#include <gtest/gtest.h>

#include <iostream>
#include <sstream>
#include <memory>
#include <string>

#include "src/game.h"
#include "src/random.h"
#include "src/savegame.h"
#include "src/mission.h"


TEST(SaveGame, RandomMapSaveGame) {
  // Create random map game
  std::unique_ptr<Game> game(new Game());
  game->init(3, Random("8667715887436237"));
  // Add player to game
  game->add_player(35, 30, 40);

  Player *player_0 = game->get_player(0);
  ASSERT_TRUE(player_0 != NULL);

  // Build castle
  bool r = game->build_castle(game->get_map()->pos(6, 6), player_0);
  ASSERT_TRUE(r) << "Player was not able to build castle";

  // Run game for a number of ticks
  for (int i = 0; i < 500; i++) game->update();

  // Save the game state
  std::stringstream str;
  bool saved = GameStore::get_instance().write(&str, game.get());
  str.flush();

  ASSERT_TRUE(saved && str.good()) <<
    "Failed to save game state; returned " << saved;

  // Load the game state into a new game
  str.seekg(0, std::ios::beg);
  std::unique_ptr<Game> loaded_game(new Game());
  bool loaded = GameStore::get_instance().read(&str, loaded_game.get());

  ASSERT_TRUE(loaded) <<
    "Failed to load save game state; returned " << loaded;

  // Check map
  EXPECT_EQ(*game->get_map(), *loaded_game->get_map());

  // Check gold deposit
  EXPECT_EQ(game->get_gold_total(), loaded_game->get_gold_total());

  // Check player
  Player *loaded_player_0 = loaded_game->get_player(0);
  ASSERT_TRUE(loaded_player_0 != NULL);

  // Check player land area
  EXPECT_EQ(player_0->get_land_area(), loaded_player_0->get_land_area());
}

static std::string
save_state(Game *game) {
  std::stringstream str;
  GameStore::get_instance().write(&str, game);
  return str.str();
}

/* A loaded game goes on as the saved one: everything that changes the
   course of the game is in the save. A mission with its computer players,
   the same in every run (the generator of the game set in its save), is
   saved at a step, loaded and both go on. */
static void
check_loaded_game_goes_on(size_t mission, int save_at, int steps) {
  std::string start =
    save_state(GameInfo::get_mission(mission)->instantiate().get());
  size_t random = start.find("  random = ");
  ASSERT_NE(random, std::string::npos);
  start.replace(random, start.find('\n', random) - random,
                "  random = 1234567812345678");
  std::stringstream start_str(start);
  std::unique_ptr<Game> game(new Game());
  ASSERT_TRUE(GameStore::get_instance().read(&start_str, game.get()));
  if (game->is_paused()) game->pause();

  for (int i = 0; i < save_at; i++) game->update();

  std::stringstream saved(save_state(game.get()));
  std::unique_ptr<Game> loaded(new Game());
  ASSERT_TRUE(GameStore::get_instance().read(&saved, loaded.get()));
  /* Loading pauses the game, the player goes on. */
  if (loaded->is_paused()) loaded->pause();

  for (int i = 1; i <= steps; i++) {
    game->update();
    loaded->update();
    if (i % 100 == 0) {
      ASSERT_TRUE(save_state(game.get()) == save_state(loaded.get())) <<
        "The loaded game went another way by step " << i;
    }
  }
}

/* The counts of all building types (the gold smelters were lost). */
TEST(SaveGame, LoadedGameGoesOnLate) {
  check_loaded_game_goes_on(7, 60000, 1000);
}

/* The threat levels of the military buildings, not computed again. */
TEST(SaveGame, LoadedGameGoesOnEarly) {
  check_loaded_game_goes_on(9, 20000, 1000);
}

/* The values that earlier states of a serf left in its state memory. */
TEST(SaveGame, LoadedGameGoesOnSerfStates) {
  check_loaded_game_goes_on(14, 40000, 1000);
}

/* A save of an older version, without the values added since, loads. */
TEST(SaveGame, OlderSave) {
  std::unique_ptr<Game> game(new Game());
  game->init(3, Random("8667715887436237"));
  game->add_player(35, 30, 40);
  ASSERT_TRUE(game->build_castle(game->get_map()->pos(6, 6),
                                 game->get_player(0)));
  for (int i = 0; i < 500; i++) game->update();

  static const char *const added[] = {
    "map_random", "knight_morale_counter", "inventory_schedule_counter",
    "game_end_pending", "castle", "castle_inventory",
    "cont_search_after_non_optimal_find", "send_generic_delay",
    "send_knight_delay", "knight_morale", "gold_deposited",
    "military_max_gold", "timers", "messages", "player_stat_history",
    "resource_count_history", "burning_counter", "serfs_out", "state.raw"
  };
  std::istringstream lines(save_state(game.get()));
  std::string older;
  std::string line;
  while (std::getline(lines, line)) {
    std::string key = line.substr(0, line.find(" = "));
    key.erase(0, key.find_first_not_of(' '));
    bool skip = false;
    for (const char *name : added) {
      if (key == name) skip = true;
    }
    /* 23 building counts, without the gold smelters. */
    if (key == "completed_building_count" ||
        key == "incomplete_building_count") {
      line = line.substr(0, line.rfind(','));
    }
    if (!skip) older += line + "\n";
  }

  std::stringstream str(older);
  std::unique_ptr<Game> loaded(new Game());
  ASSERT_TRUE(GameStore::get_instance().read(&str, loaded.get()));
  EXPECT_EQ(game->get_player(0)->get_land_area(),
            loaded->get_player(0)->get_land_area());
  EXPECT_TRUE(loaded->get_player(0)->has_castle());
}
