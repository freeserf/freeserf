/*
 * profiler.cc - Profiling tool.
 *
 * Copyright (C) 2018-2019   Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/profiler.h"

#include <string>
#include <istream>

#include "src/command_line.h"
#include "src/log.h"
#include "src/version.h"
#include "src/game-manager.h"
#include "src/game.h"

int
main(int argc, char *argv[]) {
  std::string save_file;

  CommandLine command_line;
  command_line.add_option('h', "Show this help text", [&command_line](){
                  command_line.show_help();
                  exit(EXIT_SUCCESS);
                });
  command_line.add_option('l', "Load saved game")
                .add_parameter("FILE", [&save_file](std::istream& s) {
                  std::getline(s, save_file);
                  return true;
                });
  command_line.set_comment("Please report bugs to <" PACKAGE_BUGREPORT ">");
  if (!command_line.process(argc, argv) || save_file.empty()) {
    return EXIT_FAILURE;
  }

  Log::Info["profiler"] << "starts " << FREESERF_VERSION;

  GameManager &game_manager = GameManager::get_instance();
  PGameSource game_source = game_manager.get_game_source("local");

  if (!game_source) {
    return EXIT_FAILURE;
  }
  PGameInfo game_info = game_source->create_game(save_file);
  if (!game_info) {
    return EXIT_FAILURE;
  }
  game_manager.start_game(game_info);
  Log::Info["profiler"] << "loaded game '" << save_file << "'";

  PGame game = game_manager.get_current_game();
  while (true) {
    game->update();
  }

  return EXIT_SUCCESS;
}
