/*
 * test_map.cc - TAP test for loading/saving game
 *
 * Copyright (C) 2016  Jon Lund Steffensen <jonlst@gmail.com>
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

#include <iostream>
#include <sstream>

#include "src/game.h"
#include "src/random.h"
#include "src/savegame.h"

int
main(int argc, char *argv[]) {
  // Print number of tests for TAP
  std::cout << "1..3" << "\n";

  // Create random map game
  Game *game = new Game(0);
  game->init();
  game->load_random_map(3, Random("8667715887436237"));

  // Run game for a number of ticks
  for (int i = 0; i < 100; i++) game->update();

  std::cout << "ok 1 - Random map game started and running\n";

  // Save the game state
  std::stringstream str;
  bool saved = save_text_state(&str, game);
  str.flush();

  if (saved && str.good()) {
    std::cout << "ok 2 - Saved game state\n";
  } else {
    std::cout << "not ok 2 - Failed to save game state; returned " <<
      saved << "\n";
    return 0;
  }

  // Load the game state into a new game
  str.seekg(0, std::ios::beg);
  Game *loaded_game = new Game(0);
  bool loaded = load_text_state(&str, loaded_game);

  if (loaded) {
    std::cout << "ok 3 - Loaded game state\n";
  } else {
    std::cout << "not ok 3 - Failed to load save game state; returned " <<
      loaded << "\n";
    return 0;
  }

  delete game;
}
