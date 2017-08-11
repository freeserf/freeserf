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
  bool saved = GameStore::get_instance()->write(&str, game.get());
  str.flush();

  ASSERT_TRUE(saved && str.good()) <<
    "Failed to save game state; returned " << saved;

  // Load the game state into a new game
  str.seekg(0, std::ios::beg);
  std::unique_ptr<Game> loaded_game(new Game());
  bool loaded = GameStore::get_instance()->read(&str, loaded_game.get());

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
