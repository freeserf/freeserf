/*
 * test_integrity.cc - Consistency check of the game state
 *
 * Copyright (C) 2026  Wicked_Digger <wicked_digger@mail.ru>
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

#include <memory>
#include <sstream>

#include "src/game.h"
#include "src/building.h"
#include "src/integrity.h"
#include "src/random.h"
#include "src/savegame.h"

static std::unique_ptr<Game>
create_game(Player **player) {
  std::unique_ptr<Game> game(new Game());
  game->init(3, Random("8667715887436237"));
  game->add_player(35, 30, 40);
  *player = game->get_player(0);
  return game;
}

TEST(Integrity, CleanGame) {
  Player *player = nullptr;
  std::unique_ptr<Game> game = create_game(&player);
  ASSERT_TRUE(game->build_castle(game->get_map()->pos(6, 6), player));

  for (int i = 0; i < 2000; i++) game->update();
  EXPECT_EQ(0u, IntegrityCheck::run(game.get(), false));

  // Loading runs the check too; the loaded state must be clean as well.
  std::stringstream str;
  ASSERT_TRUE(GameStore::get_instance().write(&str, game.get()));
  str.seekg(0, std::ios::beg);
  std::unique_ptr<Game> loaded(new Game());
  ASSERT_TRUE(GameStore::get_instance().read(&str, loaded.get()));
  EXPECT_EQ(0u, IntegrityCheck::run(loaded.get(), false));
}

TEST(Integrity, RepairsLostRequest) {
  Player *player = nullptr;
  std::unique_ptr<Game> game = create_game(&player);
  PMap map = game->get_map();
  MapPos castle = map->pos(6, 6);
  ASSERT_TRUE(game->build_castle(castle, player));

  // A construction site next to the castle.
  Building *site = nullptr;
  for (int i = 1; i < 120 && site == nullptr; i++) {
    MapPos pos = map->pos_add_spirally(castle, i);
    if (game->can_build_building(pos, Building::TypeLumberjack, player) &&
        game->build_building(pos, Building::TypeLumberjack, player)) {
      site = game->get_building_at_pos(pos);
    }
  }
  ASSERT_TRUE(site != nullptr);
  EXPECT_EQ(0u, IntegrityCheck::run(game.get(), false));

  // A request without a plank on its way, as left by a lost resource.
  ASSERT_TRUE(site->add_requested_resource(Resource::TypePlank, false));
  EXPECT_EQ(1u, IntegrityCheck::run(game.get(), true));
  EXPECT_EQ(0, site->get_requested_in_stock(0));
  EXPECT_EQ(0u, IntegrityCheck::run(game.get(), false));
}
